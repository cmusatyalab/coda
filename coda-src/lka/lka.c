/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.


                        Additional copyrights
              Copyright (c) 2002-2003 Intel Corporation

#*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/param.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>

#include "coda_string.h"
#include <copyfile.h>
#include "lka_private.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif


/* doubly-linked list of lookaside databases; each item in list is an lkdb */
static INIT_LIST_HEAD(lkdbchain);

static struct lkdb *GetlkdbByName(char *); /* forward ref */
static void RemoveAll(); /* forward ref */
static void ListAll(char *, int); /* forward ref */

int LookAsideAndFillContainer (unsigned char sha[SHA_DIGEST_LENGTH], int cfd, 
       int expectedlength, char *codaprefix, char *emsgbuf, int emsgbuflen)
{
/* LookAsideAndFillContainer() takes the SHA of an object and sees if
   it is able to find its contents via lookaside.  If successful, it 
   copies contents of lookaside target into the specified container file.
   As a sanity check, it then computes and verifies the SHA of the container file.

   expectedlength is a "belt-and-suspenders" sanity check;  it is ignored
   if set to a negative value; if zero or positive, it is assumed to be 
   be the expected length of the file;  in principle, a SHA match on a 
   wrong length file should be so highly improbable that the length is
   not needed; but it is there to help people sleep better.

   If codaprefix is non-null, it points to the mount point of Coda (typically "/coda");
   pathnames beginning with this prefix are not used for lookaside, to avoid
   deadlock due to recursive Venus invocation.  This will only happen if
   pathnames with this prefix appear in a lookaside database.

   Returns 1 if lookaside,copy and verification were all successful; 
      0 in all other situations. If emsgbuf is non-empty on return, it
      contains an error message.

   Note: contents of containerfile unspecified upon exit with 0 return code;
   in particular, it is NOT guaranteed to be restored to original value
*/
  char hitpathname[MAXPATHLEN];
  struct lkdb *dbp;
  struct dllist_head *p;
  unsigned char cshabuf[SHA_DIGEST_LENGTH];
  int err, hfd;

  emsgbuf[0] = '\0'; /* null message, anticipating success */

  /* duh, 0-length files are easy! */
  if (expectedlength == 0)
      return 1;

  list_for_each(p, lkdbchain) {
      dbp = list_entry(p, struct lkdb, chain);

      /* check next lkdb for sha value */
      dbp->attempts++;
      if (lkdb_GetFilenameFromSHA(dbp, sha, hitpathname, sizeof(hitpathname),
				 emsgbuf, emsgbuflen))
      {
	  dbp->hits++;
	  goto FoundSHA; /* success! */
      }
      if (*emsgbuf) return(0); /* fatal error in GetFilenameFromSHA() */
  }
  return(0); /* sha not in any of the lkdb's */

 FoundSHA: ;/* found the sha in the lkdb pointed to by dbp */
  /* Save pathname of hit as info on success return; overwritten by error msgs below */
  snprintf(emsgbuf, emsgbuflen, "hitpathname: %s\n", hitpathname);

  /* Do Coda prefix check */
  if (codaprefix && !strncmp(hitpathname, codaprefix, strlen(codaprefix))) {
    snprintf(emsgbuf, emsgbuflen, "Avoiding recursive lookaside on hitpathname: %s\n", 
	     hitpathname);
    return(0);
  }

  hfd = open(hitpathname, O_RDONLY | O_BINARY);
  /* Copy to container file and validate SHA (just to be safe) */
  lseek(cfd, 0, SEEK_SET);
  err = CopyAndComputeViceSHA(hfd, cfd, cshabuf);
  close(hfd);

  if (err < 0) {
    snprintf(emsgbuf, emsgbuflen, "CopyAndComputeSHA(%s): %s", 
	     hitpathname, strerror(errno));
    return(0); /* weird failure during copy */
  }

  if (memcmp(cshabuf, sha, SHA_DIGEST_LENGTH)) {
      snprintf(emsgbuf, emsgbuflen, "%s: mismatch on SHA verification",
	       hitpathname);
      dbp->shafails++;
      return(0);    
  }

  if (expectedlength >= 0) { /* verify length if specified */
    struct stat cfstat;
    if (fstat(cfd, &cfstat) < 0) {
      snprintf(emsgbuf, emsgbuflen, "lookaside: stat %s", strerror(errno));
      return(0);
    }
    if (cfstat.st_size != expectedlength) {
      snprintf(emsgbuf, emsgbuflen, "lookaside: length mismatch (%ld instead of %d)", 
	       cfstat.st_size, expectedlength);
      return(0);
    }
  }

  return(1); /* Success! */
}

int lkdb_BindDB(struct lkdb *dbp, char *dbpathname, char *emsgbuf, int emsgbuflen)
{
  /* Associates the db at dbpathname with this lkdb structure; typically
     called only once per lkdb, immediately after creation.
     Returns 1 on success, 0 on failure.
     In case of failure, an error msg of at most emsgbuflen is placed in
     buffer emsgbuf; it is empty on success.
  */
  char *c, *d, zero = '\0', *desc;
  int rc, dlen, dpos;

  if (emsgbuflen > 0) *emsgbuf = 0; /* null msg for success case */

  if (dbpathname[0] != '/') {
    snprintf(emsgbuf, emsgbuflen, "dbpathname not an absolute path");
    goto err;
  }

  dbp->dbname = strdup(dbpathname);
  if (!dbp->dbname) {
    snprintf(emsgbuf, emsgbuflen, "strdup(dbname) failed");
    goto err;
  }
  
  c = strrchr(dbpathname, '/');
  assert(c != NULL); /* should never fail because dbpathname[0] == '/' */
  dlen = c - dbpathname;

  dbp->dblocation = malloc(dlen + 2);
  if (!dbp->dblocation) {
    snprintf(emsgbuf, emsgbuflen, "malloc(dblocation) failed");
    goto err;
  }

  strncpy(dbp->dblocation, dbpathname, dlen+1);
  dbp->dblocation[dlen+1] = '\0';

  /* now obtain database handle */

  rc = rwcdb_init(&dbp->dbh, dbp->dbname, O_RDONLY);
  if (rc) {
    snprintf(emsgbuf, emsgbuflen, "dbopen(%s) failed", dbp->dbname);
    goto err;
  }

  /* now obtain the descriptor record */
  rc = rwcdb_find(&dbp->dbh, &zero, 0);
  if (rc != 1) {
    snprintf(emsgbuf, emsgbuflen,
	     "%s: malformed lookaside database; can't find descriptor record",
	     dbp->dbname);
    goto err;
  }

  dlen = rwcdb_datalen(&dbp->dbh);
  dpos = rwcdb_datapos(&dbp->dbh);

  desc = malloc(dlen);
  if (!desc) {
    snprintf(emsgbuf, emsgbuflen, "%s can't allocate descriptor", dbp->dbname);
    goto err;
  }

  rc = rwcdb_read(&dbp->dbh, desc, dlen, dpos);
  if (rc == -1) {
    snprintf(emsgbuf, emsgbuflen, "%s can't read descriptor", dbp->dbname);
    free(desc);
    goto err;
  }

  /* Parse descriptor record */

  c = desc; /* start at the very beginning */
  d = c + dlen; /* end of record pointer */

  if (!(c < d)) goto BadDescriptorRecord;
  if (strncmp(c, LKA_VERSION_STRING, strlen(LKA_VERSION_STRING)))
    goto BadDescriptorRecord;

  c += strlen(LKA_VERSION_STRING); /* step over version string */
  if (!(c < d)) goto BadDescriptorRecord;

  /* nothing more to parse in Version 1.1 */
  free(desc);

  /* get number of entries */
  rc = rwcdb_next(&dbp->dbh, 1);
  while (rc == 1) {
      rc = rwcdb_next(&dbp->dbh, 0);
      dbp->entrycount++;
  }

  /* subtract one because we also counted the descriptor record */
  dbp->entrycount--;

  return(1); /* all is well */


BadDescriptorRecord:
  snprintf(emsgbuf, emsgbuflen, "%s: bad descriptor record '%s'",
	   dbp->dbname, desc);
  free(desc);
err:
  return(0);
}

int lkdb_GetFilenameFromSHA(struct lkdb *dbp,
			   unsigned char sha[SHA_DIGEST_LENGTH],
			   char *hitpath, int hitpathlen,
			   char *emsgbuf, int emsgbuflen)
{
/* GetFilenameFromSHA() probes this db for key sha;  if successful,
   copies the value corresponding  to this key into the buffer hitpath.
   This value is the the pathname of a file whose SHA is sha.

   Returns 1 on successful lookup; 0 if no key found.  If emsgbuf is
   non-null on return, it has a useful error message.
*/

  int rc, dlen, dpos, prefix_len = 0;
  char prefix;

  /* check whether the lookaside database was updated */
  rc = rwcdb_sync(&dbp->dbh);
  if (rc == -1) {
      snprintf(emsgbuf, emsgbuflen, "%s unexpected failure", dbp->dbname);
      return 0;
  }

  rc = rwcdb_find(&dbp->dbh, sha, SHA_DIGEST_LENGTH);
  if (rc != 1) return 0; // not found

  dlen = rwcdb_datalen(&dbp->dbh);
  dpos = rwcdb_datapos(&dbp->dbh);
  if (dlen < 2) return 0; /* we need at least one character and a '\0' */

  /* Found it!  Copy out the result */

  rc = rwcdb_read(&dbp->dbh, &prefix, 1, dpos);
  if (rc == -1) {
      snprintf(emsgbuf, emsgbuflen, "%s can't read prefix", dbp->dbname);
      return(0);
  }

  /* Add absolute prefix to hitpathname, if necessary */
  if (prefix != '/') {
      prefix_len = strlen(dbp->dblocation);
      strcpy(hitpath, dbp->dblocation); /* copy prefix */
  }

  if (prefix_len + dlen >= hitpathlen) {
      snprintf(emsgbuf, emsgbuflen, "%s get(): result too big (%d, %d)",
	       dbp->dbname, dlen, hitpathlen);
      return(0);
  }

  rc = rwcdb_read(&dbp->dbh, &hitpath[prefix_len], dlen, dpos);
  if (rc == -1) {
    snprintf(emsgbuf, emsgbuflen, "%s can't read path", dbp->dbname);
    return(0);
  }

  return(1);
}


struct lkdb *new_lkdb(void)
{
    struct lkdb *dbp;
    dbp = malloc(sizeof(*dbp)); 
    if (!dbp) return NULL;

    memset(dbp, 0, sizeof(*dbp));
    list_head_init(&dbp->chain);

    return dbp;
}

void delete_lkdb(struct lkdb *dbp)
{
    if (!dbp) return;

    list_del(&dbp->chain);

    if (dbp->dbname) {
	free(dbp->dbname);
	dbp->dbname = NULL;
    }

    if (dbp->dblocation) {
	free(dbp->dblocation);
	dbp->dblocation = NULL;
    }

    rwcdb_free(&dbp->dbh);

    free(dbp);
}


int LKParseAndExecute(char *command, char *emsgbuf, int emsgbuflen)
{
  /* Parses and executes specified cfs command;
     Returns 1 on success, 0 upon any kind of failure;
     On failure or success, emsgbuf is filled with a useful message.
     (emsgbuf must point to a buffer of length emsgbuflen)

   */
  struct cmd {
    char op; /* '+' or '-' */
    char dbname[MAXPATHLEN]; 
  };
  int cmdcount = 0; /* number of commands */
  struct cmd *cmdarray = 0; /* malloc'ed array of commands */
  int rc = 0, clearflag = 0;
  int maxoffset, nextoffset, xo; /* indexes into command */
  int i, eleft;
  char *emptr;
  #define USAGESTR "Usage: cfs lka [--clear] [+/-<db1>] [+/-<db2>] [+/-<db3>] ...\n        cfs lka --list\n"

  /* Ignore trailing whitespace in command; set maxoffset to just past rightmost
     non-whitespace character */
  maxoffset = strlen(command) - 1; /* max value of nextoffset */
  while (maxoffset >= 0) {
    if (command[maxoffset] == ' ' || command[maxoffset] == '\t') {
      maxoffset--; continue;
    }
    else break;
  }
  maxoffset++; /* move one past char that stopped loop */

  /* Ignore leading whitespace */
  nextoffset = 0; 
  while (nextoffset < maxoffset) {
    if (command[nextoffset] == ' ' || command[nextoffset] == '\t') {
      nextoffset++; continue;
    }
    else break;
  }

  /* Now nextoffset is the first non-whitespace character */

  if (!(nextoffset < maxoffset)) goto BogusCommand;

  if (!strncmp(&command[nextoffset], "--list", strlen("--list"))){
    ListAll(emsgbuf, emsgbuflen);
    rc = 1;
    goto EndParse; /* we are done! */    
  }

  if (!strncmp(&command[nextoffset], "--clear", strlen("--clear"))){
    clearflag = 1;
    nextoffset += strlen("--clear");
  }

  /* Now parse all the "+<db>"and "-<db>" items */
  cmdcount = 0;
  cmdarray = (struct cmd *) malloc(sizeof(struct cmd)); 
  if (!cmdarray) {
    snprintf(emsgbuf, emsgbuflen, "LKParseAndExecute: malloc failed");
    rc = 0; goto EndParse;
  }
  while (nextoffset < maxoffset) {
    /* get next command */
    if (sscanf(&command[nextoffset], " +%s%n", cmdarray[cmdcount].dbname, &xo)) {
      cmdarray[cmdcount].op = '+';
    }
    else {
      if (sscanf(&command[nextoffset], " -%s%n", cmdarray[cmdcount].dbname, &xo)) {
	cmdarray[cmdcount].op = '-';
      }
      else goto BogusCommand;
    }
    nextoffset += xo; /* jump past just-recognized item */
    cmdcount++;

    /* alloc entry for next iteration */
    cmdarray = (struct cmd *) realloc(cmdarray, (1+cmdcount)*sizeof(struct cmd)); 
    if (!cmdarray) {
      snprintf(emsgbuf, emsgbuflen, "LKParseAndExecute: realloc failed");
      rc = 0; goto EndParse;
    }
  }


  /* Parsing done; now execute commands in cmdarray */

 /* ExecuteCommands: */

  /* each command might add to emsgbuf; define local pointer and len */
  emptr = emsgbuf; /* pointer for next msg into emsgbuf */
  eleft = emsgbuflen; /* space left */
#define RESIZEBUF() \
     emptr = emsgbuf + strlen(emsgbuf);\
     eleft = emsgbuflen - (emptr - emsgbuf);

  if (clearflag) {
    RemoveAll();
    snprintf(emptr, eleft, "All lkdbs removed\n");
    RESIZEBUF();
  }

  /* Execute commands; no checking for duplicate inserts;
     pathname used for '-' must exactly match earlier '+';
     Diagnostic messages accumulate in emsgbuf until loop is done;
  */
  for (i = 0; i < cmdcount; i++) {
    struct lkdb *mylk;

    switch (cmdarray[i].op) {

    case '+':
      mylk = new_lkdb(); 
      if (mylk && lkdb_BindDB(mylk, cmdarray[i].dbname, emptr, eleft)) {
	  list_add(&mylk->chain, &lkdbchain);
	  snprintf(emptr, eleft, "%s added (%d entries)\n", cmdarray[i].dbname,
		   mylk->entrycount);
	  RESIZEBUF();
      }
      else {
	  delete_lkdb(mylk);
	  RESIZEBUF(); /* BindDB() might have added text to emsgbuf */
      }
      break; 

    case '-':
      if (clearflag) continue; /* already removed */
      mylk = GetlkdbByName(cmdarray[i].dbname);
      if (!mylk) {
	snprintf(emptr, eleft, "%s not in lookaside list\n", cmdarray[i].dbname);
      }
      else {
	delete_lkdb(mylk);
	snprintf(emptr, eleft, "%s removed\n", cmdarray[i].dbname);
      }
      RESIZEBUF();
      break;

    default: /* couldn't possibly be anything else */
      assert(0);
    }
  }

  /* done with everything!
     Success return is for whole sequence; check emsgbuf for
     individual success/failure */
  rc = 1; goto EndParse;

 BogusCommand: 
  snprintf(emsgbuf, emsgbuflen, "%s\n", USAGESTR); /*fall through to EndParse */

 EndParse:
  if (cmdarray) free(cmdarray);
  return(rc);
}


static struct lkdb *GetlkdbByName(char *name)
{
  /* Walks lkdbchain, looking for entry with matching dbname;
     Returns pointer to it, if found; NULL otherwise */

    struct lkdb *dbp;
    struct dllist_head *p;

    list_for_each(p, lkdbchain) {
	dbp = list_entry(p, struct lkdb, chain);

	if (strcmp(dbp->dbname, name) == 0)
	    return(dbp);
    }
    return(NULL);
}

static void RemoveAll()
{
    struct dllist_head *p, *next;

    for (p = lkdbchain.next; p != &lkdbchain; p = next) {
	struct lkdb *dbp = list_entry(p, struct lkdb, chain);
	next = p->next;

	delete_lkdb(dbp);
    }
}

static void ListAll(char *mptr, int mlen)
{
    char *where;
    int bytesleft, thislinelen;
    struct dllist_head *p;

    where = mptr;
    bytesleft = mlen;

    list_for_each(p, lkdbchain) {
	struct lkdb *dbp = list_entry(p, struct lkdb, chain);
	if (bytesleft <= 0) break;
	snprintf(where, bytesleft, "%s (%d entries) [attempts = %d  hits = %d  shafails = %d]\n", 
		 dbp->dbname, dbp->entrycount,
		 dbp->attempts, dbp->hits, dbp->shafails);

	thislinelen = strlen(where);
	bytesleft -= thislinelen;
	where += thislinelen;
    }
}

