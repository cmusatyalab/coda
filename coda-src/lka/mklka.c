/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-2003 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.


              Copyright (c) 2002-2003 Intel Corporation

#*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include "coda_string.h"
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fts.h>
#include <fcntl.h>

#include "lka_private.h"



int VerboseFlag = 0; 
int RelativePathFlag = 0; /* iff true use relative paths; else absolute */
int NumEntries = 0; /* count of entries inserted into database */

char *NewLKDB = 0; /* pathname of lookaside db to be created */
char *TreeRoot = 0; /* pathname of root of tree to be walked and hashed */

/* Forward refs */
int SetDescriptor(struct rwcdb *dbh); 
int WalkTree(char *, struct rwcdb *dbh); 

int main(int argc, char **argv)
{
  struct rwcdb dbh; /* database handle */
  int rc, i, open = 0;

  /* Parse args */
  for (i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "-v")) {VerboseFlag = 1; continue;}
    if (!strcmp(argv[i], "-r")) {RelativePathFlag = 1; continue;}
    if (*(argv[i]) == '-') goto ParseError; /* no other flags meaningful */
    if (!NewLKDB) {NewLKDB = argv[i]; continue;}
    if (!TreeRoot) {TreeRoot = argv[i]; continue;}
    goto ParseError;
  }
  if (!NewLKDB || !TreeRoot) goto ParseError;

  /* Create lookaside database */
  rc = rwcdb_init(&dbh, NewLKDB, O_RDWR);
  if (rc) {
    printf("%s: %s\n", NewLKDB, strerror(errno));
    goto err;
  }

  open = 1;

  /* Insert the descriptor record */
  if (SetDescriptor(&dbh)) /* error */
      goto err;

  /* Walk the tree at TreeRoot, inserting records for each file */
  if (WalkTree(TreeRoot, &dbh) < 0) /* error */
      goto err;

 /* DBFillComplete */
  rwcdb_free(&dbh);
  return(0);

ParseError:
  printf("Usage: mklka [-v] [-r] <newlkdb.lka> <treeroot>\n");

err:
  if (open)
      rwcdb_free(&dbh);
  return(-1);
}

int SetDescriptor(struct rwcdb *dbh)
{
  /* Inserts (or replaces) a descriptor record (i.e. key value of 0) into
     open lookaside database specified by dbhandle;
     Returns 0 on success;  -1 on error of any kind.
  */
  char zero = '\0', *desc;
  int rc, dlen;

  /* Construct descriptor */
  dlen = strlen(LKA_VERSION_STRING) + 1;
  desc = malloc(dlen);
  if (!desc) {
    printf("Arrgh ... malloc(%d) failed\n", dlen);
    return(-1);
  }
  sprintf((char *)desc, "%s", LKA_VERSION_STRING);

  /* Now do the insert */
  rc = rwcdb_insert(dbh, &zero, 0, desc, dlen);
  free(desc); /* no reason to hang on to it */

  if (rc != 1) {
    printf("SetDescriptor() failed\n");
    return(-1);
  }

  /* success! */
  printf("Creating lkdb %s\n", NewLKDB);
  return(0);
}


int WalkTree(char *troot, struct rwcdb *dbh)
{
  /* Traverse tree at troot and insert a record into database
     dbhandle for each (plain) file.  The key of the record is the
     SHA value of the file; the data part of the record is
     the pathname of the file (absolute or relative to troot,
     depending on RelativePathFlag.
     Returns 0 on success, -1 on error of any kind.
     On exit, NumEntries has number of records inserted.
  */

  int rc;
  FTSENT *nextf; /* next element in tree */
  FTS *fth; /* open handle for fts() routines */
  char *path_argv[2];
  unsigned char shabuf[SHA_DIGEST_LENGTH];
  int troot_strlen; /* save length of troot in this */
  int myfd, dlen;
  char *path;


  troot_strlen = strlen(troot);
  path_argv[0] = troot;
  path_argv[1] = 0;
  fth = fts_open(path_argv, FTS_PHYSICAL, 0);
  if (!fth) {
    printf("%s: %s\n", troot, strerror(errno));
    return(-1);
  }

  while ((nextf = fts_read(fth)) != NULL) {
    if (nextf->fts_info != FTS_F) continue; /* skip all but plain files */

    /* compute the SHA of this file */
    myfd = open(nextf->fts_name, O_RDONLY, 0);
    if (myfd < 0) {
      printf("Skipping %s: %s\n", nextf->fts_path, strerror(errno));
      continue;
    }

    ComputeViceSHA(myfd, shabuf);
    close(myfd);

    /* Construct record to be inserted */
    dlen = nextf->fts_pathlen + 1; /* assuming absolute path */
    path = nextf->fts_path; /* assuming absolute path */

    if (RelativePathFlag){/* troot should be prefix of fts_path */
      if (strncmp(nextf->fts_path, troot, troot_strlen)) {
	printf("Weirdness: tree root (%s) not prefix of %s\n",
	       troot, nextf->fts_path);
	rc = -1; goto WalkDone;
      }

      /* skip over troot and the slash that follows it */
      dlen -= troot_strlen + 1;
      path += troot_strlen + 1;
    }

    /* Insert record into db */
    rc = rwcdb_insert(dbh, shabuf, SHA_DIGEST_LENGTH, path, dlen);
    if (rc != 1) {
      printf("%s: insert into database failed\n", nextf->fts_path);
      rc = -1; goto WalkDone;
    }
    
    /* Successfully added one more entry! */
    NumEntries++;
    if (VerboseFlag) {
      char temp[3*SHA_DIGEST_LENGTH];
      ViceSHAtoHex(shabuf, temp, sizeof(temp));
      printf("Entry %05d:  %s  %s\n", NumEntries, temp, path);
    }
    else {
      if ((NumEntries > 99) && (!(NumEntries % 100))) {
	printf("#"); /* indicate progress */
	fflush(stdout);
      }
    }
  }

  /* loop exited on fts_read() error or when done */
  if (errno) {
    printf("fts_read(): %s\n", strerror(errno));
    rc = -1; goto WalkDone;
  }
  else {
    rc = 0; 
    if (!VerboseFlag && (NumEntries > 99)) printf("\n"); /* terminate line of hashes */
  }

 WalkDone: /* rc has return code on entry */
  fts_close(fth);
  return(rc);
}
