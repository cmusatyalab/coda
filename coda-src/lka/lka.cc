/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-2003 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.


                        Additional copyrights
              Copyright (c) 2002-2003 Intel Corporation

#*/

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include "coda_string.h"
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>

#ifdef __cplusplus
}
#endif

#include <rpc2/rpc2.h>
#include <codaconf.h>
#include <copyfile.h>
#include "lka.h"


static dlist lkdbchain; /* doubly-linked list of lookaside databases;
			  each item in list is of class lkdb */


static lkdb *GetlkdbByName(char *); /* forward ref */
static void RemoveAll(); /* forward ref */
static void ListAll(char *, int); /* forward ref */


int LookAsideAndFillContainer (RPC2_BoundedBS *fsha, char *containerfile, 
       int expectedlength, char *codaprefix, char *emsgbuf, int emsgbuflen) {

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

  if (fsha->SeqLen != SHA_DIGEST_LENGTH) {
    snprintf(emsgbuf, emsgbuflen, "Bogus sha length: %d", fsha->SeqLen);
    return (0);
  }
  else *emsgbuf = 0; /* null message, anticipating success */

  if (lkdbchain.count() == 0) return(0); /* no lookaside dbs */

  /* We have at least one lookaside db, perhaps more; search them
     until first hit */

  dlist_iterator nextdb(lkdbchain);
  lkdb *dbp;
  char hitpathname[MAXPATHLEN+1];

  while ((dbp = (lkdb *)nextdb())) {
    /* check next lkdb for sha value */
    dbp->attempts++;
    if (dbp->GetFilenameFromSHA(fsha, hitpathname, sizeof(hitpathname),
				emsgbuf, emsgbuflen))
      goto FoundSHA; /* success! */
    if (*emsgbuf) return(0); /* error in GetFilenameFromSHA() */
  }
  return(0); /* fsha not in any of the lkdb's */

 FoundSHA: ;/* found the sha in the lkdb pointed to by dbp */
  dbp->hits++;

  /* Add absolute prefix to hitpathname, if necessary */
  if (dbp->pathnames_are_relative) {
    char *temp = (char *)malloc(MAXPATHLEN+1);
    strncpy(temp, hitpathname, MAXPATHLEN); /* save relative part */
    strncpy(hitpathname, dbp->dblocation, MAXPATHLEN); /* copy prefix */
    int lh = strlen(hitpathname);
    if (lh < MAXPATHLEN) {strcat(hitpathname, "/"); lh++;}
    strncat(hitpathname, temp, MAXPATHLEN - lh);
    free(temp);
  }

  /* Save pathname of hit as info on success return; overwritten by error msgs below */
  snprintf(emsgbuf, emsgbuflen, "hitpathname: %s\n", hitpathname);

  /* Do Coda prefix check */

  if (codaprefix && !strncmp(hitpathname, codaprefix, strlen(codaprefix))) {
    snprintf(emsgbuf, emsgbuflen, "Avoiding recursive lookaside on hitpathname: %s\n", 
	     hitpathname);
    return(0);
  }

  /* Copy to container file and validate SHA (just to be safe) */
  if (copyfile_byname(hitpathname, containerfile) < 0) {
    snprintf(emsgbuf, emsgbuflen, "filecopy_byname(%s, %s): %s", 
	     hitpathname, containerfile, strerror(errno));
    return(0); /* weird failure during copy */
  }

  int cfd = open(containerfile, O_RDONLY, 0);
  if (cfd < 0) {
    snprintf(emsgbuf, emsgbuflen, "%s: %s", containerfile, strerror(errno));
    return(0);
  }

  if (expectedlength >= 0) { /* verify length if specified */
    struct stat cfstat;
    if (fstat(cfd, &cfstat) < 0) {
      snprintf(emsgbuf, emsgbuflen, "%s: %s", containerfile, strerror(errno));
      close (cfd); return(0);
    }
    if (cfstat.st_size != expectedlength) {
      snprintf(emsgbuf, emsgbuflen, "%s: length mismatch (%d instead of %d)", 
	       containerfile, cfstat.st_size, expectedlength);
      close (cfd); return(0);
    }
  }

  RPC2_BoundedBS csha;
  unsigned char cshabuf[SHA_DIGEST_LENGTH];
  csha.MaxSeqLen = SHA_DIGEST_LENGTH;
  csha.SeqLen = 0;
  csha.SeqBody = cshabuf;

  if (!ComputeViceSHA(cfd, &csha)) {
    snprintf(emsgbuf, emsgbuflen, "%s: can't compute SHA", containerfile);
    close(cfd); return(0);    
  }
  else close(cfd);


  if (memcmp(csha.SeqBody, fsha->SeqBody, SHA_DIGEST_LENGTH)) {
    snprintf(emsgbuf, emsgbuflen, "%s: mismatch on SHA verification", hitpathname);
    dbp->shafails++;
    return(0);    
  }

  return(1); /* Success! */
}

int lkdb::BindDB(char *dbpathname, char *emsgbuf, int emsgbuflen) {

  /* Associates the db at dbpathname with this lkdb structure; typically
     called only once per lkdb, immediately after creation.
     Returns 1 on success, 0 on failure.
     In case of failure, an error msg of at most emsgbuflen is placed in
     buffer emsgbuf; it is empty on success.
  */

  if (emsgbuflen > 0) *emsgbuf = 0; /* null msg for success case */
  dbname = (char *) malloc(1+strlen(dbpathname));
  dblocation = (char *)malloc(MAXPATHLEN); 
  if (!dbname || !dblocation) {
    snprintf(emsgbuf, emsgbuflen, "malloc() failed");
    return(0);
  }
  strcpy(dbname, dbpathname);

  /* Get cwd of database; will use as relative pathname prefix */
  if (!realpath(dbname, dblocation)){
    snprintf(emsgbuf, emsgbuflen, "realpath(%s) --> %s", dbname, strerror(errno));
    return(0);
  }
  char *c = rindex(dblocation, '/');
  if (!c) {
    snprintf(emsgbuf, emsgbuflen, "Can't find / in absolute path %s", dblocation);
    return(0);
  }
  else *c = 0; /* location of db is string before last slash in abs path */

  /* now obtain database handle */

  dbh = dbopen(dbname, O_RDONLY, 0, DB_HASH, 0);
  if (!dbh) {
    snprintf(emsgbuf, emsgbuflen, "dbopen(%s) --> %s", dbname, strerror(errno));
    return(0);
  }

  /* now obtain the descriptor record */

  int dbrc;
  char zerosha[SHA_DIGEST_LENGTH];
  memset(zerosha, 0, SHA_DIGEST_LENGTH);
  DBT dbkey, dbdata;
  dbkey.data = zerosha;
  dbkey.size = SHA_DIGEST_LENGTH;

  dbrc = (dbh->get)(dbh, &dbkey, &dbdata, 0);

  switch (dbrc){

  case 0: /* key found! */;
    break;

  case 1: /* no key found */
    snprintf(emsgbuf, emsgbuflen,
	     "%s: malformed lookaside database; can't find descriptor record",
	     dbname);
    return(0);

  case -1: /* some other error */
    snprintf(emsgbuf, emsgbuflen, "%s descriptor record: %s", dbname,
	     strerror(errno));
    return(0);


  default: /* should never get here! */
    snprintf(emsgbuf, emsgbuflen, "%s: bogus return code %d from get()", 
	     dbname, dbrc);
    return(0);
  }

  /* Parse descriptor record */

  c = (char *)dbdata.data; /* start at the very beginning */
  char *d = c + dbdata.size; /* end of record pointer */

  if (!(c < d)) goto BadDescriptorRecord;
  if (strncmp(c, LKA_VERSION_STRING, strlen(LKA_VERSION_STRING)))
    goto BadDescriptorRecord;

  c += strlen(LKA_VERSION_STRING); /* step over version string */
  if (!(c < d)) goto BadDescriptorRecord;

  if (!strncmp(c, LKA_ABSPATH_STRING, strlen(LKA_ABSPATH_STRING))) {
    pathnames_are_relative = 0;
    c += strlen(LKA_ABSPATH_STRING);
  }
  else {
    if (!strncmp(c, LKA_RELPATH_STRING, strlen(LKA_RELPATH_STRING))) {
      pathnames_are_relative = 1;
    c += strlen(LKA_RELPATH_STRING);
    }
    else goto BadDescriptorRecord;
  }

  /* get number of entries */
  if (!(c < d)) goto BadDescriptorRecord;
  if (strncmp(c, LKA_NUMENTRIES_STRING, strlen(LKA_NUMENTRIES_STRING)))
    goto BadDescriptorRecord;
  c += strlen(LKA_NUMENTRIES_STRING);
  if (!(c < d)) goto BadDescriptorRecord;
  if (sscanf(c, "%d", &entrycount) != 1) goto BadDescriptorRecord;
  while ((c < d) && isdigit(*c)) c++; /* skip over entry count */

  /* verify presence of trailer */
  if (!(c < d)) goto BadDescriptorRecord;
  if (strncmp(c, LKA_END_STRING, strlen(LKA_END_STRING)))
    goto BadDescriptorRecord;

  /* nothing more to parse in Version 1.0 */
  c += strlen(LKA_END_STRING);
  if ((c != (d-1)) || *c)
    goto BadDescriptorRecord; /* should be at null at the end */
  return(1); /* all is well */


BadDescriptorRecord:
  snprintf(emsgbuf, emsgbuflen, "%s: bad descriptor record '%s'",
	     dbname, dbdata.data);
    return(0);
}

int lkdb::GetFilenameFromSHA(RPC2_BoundedBS *xsha, char *hitpath,
			     int hitpathlen, char *emsgbuf, int emsgbuflen){

/* GetFilenameFromSHA() probes this db for key xsha;  if successful,
   copies the value corresponding  to this key into the buffer hitpath.
   This value is the the pathname of a file whose SHA is xsha.

   Returns 1 on successful lookup; 0 if no key found.  If emsgbuf is
   non-null on return, it has a useful error message.
*/

  DBT key, value;

  key.data = xsha->SeqBody;
  key.size = xsha->SeqLen; 

  int dbrc = (dbh->get)(dbh, &key, &value, 0);

  switch (dbrc){

  case 0: /* key found! */;
    break;

  case 1: /* no key found */
    return(0);

  case -1: /* some other error */
    snprintf(emsgbuf, emsgbuflen, "%s get(): %s", dbname, strerror(errno));
    return(0);

  default: /* should never get here! */
    snprintf(emsgbuf, emsgbuflen, "%s: bogus return code %d from get()", 
	     dbname, dbrc);
    return(0);
  }

  /* Found it!  Copy out the result */
  if (value.size <= hitpathlen) {
    memcpy(hitpath, value.data, value.size);
    return(1);
  }
  else {
    snprintf(emsgbuf, emsgbuflen, "%s get(): result too big (%d, %d)",
	     dbname, value.size, hitpathlen);
    return(0);
  }
}



lkdb::lkdb() {
  dbname = 0; /* set to NULL */
  dblocation = 0;
  dbh = 0; /* no handle until BindDB is called */
  pathnames_are_relative = 0; /* default is absolute pathnames */
  attempts = hits = 0;
}

lkdb::~lkdb() {
  if (dbname) free(dbname);  /* release malloc'ed name */
  if (dblocation) free(dblocation);
  if (dbh) {(dbh->close)(dbh); /* unbind database */}
}


int LKParseAndExecute(char *command, char *emsgbuf, int emsgbuflen) {

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

 ExecuteCommands:

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
    lkdb *mylk;

    switch (cmdarray[i].op) {

    case '+':
      mylk = new lkdb; 
      if (mylk->BindDB(cmdarray[i].dbname, emptr, eleft)){
      lkdbchain.append(mylk);
      snprintf(emptr, eleft, "%s added (%d entries)\n", cmdarray[i].dbname,
	       mylk->entrycount);
      RESIZEBUF();
      }
      else {
	RESIZEBUF(); /* BindDB() might have aded text to emsgbuf */
      }
      break; 

    case '-':
      if (clearflag) continue; /* already removed */
      mylk = GetlkdbByName(cmdarray[i].dbname);
      if (!mylk) {
	snprintf(emptr, eleft, "%s not in lookaside list\n", cmdarray[i].dbname);
      }
      else {
	lkdbchain.remove(mylk);
	delete mylk;
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


static lkdb *GetlkdbByName(char *name) {
  /* Walks lkdbchain, looking for entry with matching dbname;
     Returns pointer to it, if found; NULL otherwise */

  dlist_iterator nextdb(lkdbchain);
  lkdb *dbp;

  while ((dbp = (lkdb *)nextdb())) {
    if (!strcmp(dbp->dbname, name)) return(dbp);
  }
  return(0);
}

static void RemoveAll() {
  lkdb *dbp;
  int i, dbcount;

  dbcount = lkdbchain.count();
  
  for (i = 0; i < dbcount; i++) {
    assert(dbp = (lkdb *)lkdbchain.first()); /* better not be null! */
    lkdbchain.remove(dbp);
    delete dbp;
  }
}

static void ListAll(char *mptr, int mlen) {
  dlist_iterator nextdb(lkdbchain);
  lkdb *dbp;
  char *where;
  int bytesleft, thislinelen;

  where = mptr;
  bytesleft = mlen;
  while ((dbp = (lkdb *)nextdb())) {
    if (bytesleft <= 0) break;
    snprintf(where, bytesleft, "%s (%d %s entries) [attempts = %d  hits = %d  shafails = %d]\n", 
	     dbp->dbname, dbp->entrycount,
	     (dbp->pathnames_are_relative? "relative" : "absolute"),
	     dbp->attempts, dbp->hits, dbp->shafails);

    thislinelen = strlen(where);
    bytesleft -= thislinelen;
    where += thislinelen;
  }
}

