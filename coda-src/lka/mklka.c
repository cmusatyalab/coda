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
int SetDescriptor(db_type *, int); 
int WalkTree(char *, db_type *); 

int main(int argc, char **argv)
{
  int i;
  db_type *dbh; /* database handle */

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
  dbh = db_open(NewLKDB, O_RDWR|O_TRUNC|O_CREAT, 0755, DB_HASH, 0);
  if (!dbh) {
    printf("%s: %s\n", NewLKDB, strerror(errno));
    return(-1);
  }

  /* Walk the tree at TreeRoot, inserting records for each file */
  if (WalkTree(TreeRoot, dbh) < 0) { /* error */
    db_close(dbh);
    return(-1);
  }

  /* Insert the descriptor record */
  if (SetDescriptor(dbh, NumEntries)) {/* error */
    db_close(dbh);
    return(-1);
  }

 /* DBFillComplete */
  db_close(dbh);
  return(0);

 ParseError:
  printf("Usage: mklka [-v] [-r] <newlkdb.lka> <treeroot>\n");
  return(-1);
}

int SetDescriptor(db_type *dbhandle, int entrycount)
{
  /* Inserts (or replaces) a descriptor record (i.e. key value of 0) into
     open lookaside database specified by dbhandle;
     Called after database creation with an entrycount of 0, and
     at the very end with entrycount equal to number of entries in dbhandle;
     Returns 0 on success;  -1 on error of any kind.
  */
  int dbrc;
  char zerosha[SHA_DIGEST_LENGTH];
  db_data dbkey, dbdata;

  /* Construct zero key */
  memset(zerosha, 0, SHA_DIGEST_LENGTH);
  dbkey.db_dataptr  = zerosha;
  dbkey.db_datasize = SHA_DIGEST_LENGTH;

  /* Construct descriptor */
  dbdata.db_datasize = strlen(LKA_VERSION_STRING) + 1;
  dbdata.db_dataptr  = malloc(dbdata.db_datasize);
  if (!dbdata.db_dataptr) {
    printf("Arrgh ... malloc(%d) failed\n", dbdata.db_datasize);
    return(-1);
  }
  sprintf((char *)dbdata.db_dataptr, "%s", LKA_VERSION_STRING);

  /* Now do the insert */
  dbrc = db_put(dbhandle, &dbkey, &dbdata, 0);
  free(dbdata.db_dataptr); /* no reason to hang on to it */
  if (dbrc) {
    printf("SetDescriptor() failed: %s\n", strerror(errno));
    return(-1);
  }

  /* success! */
  printf("Created lkdb %s with %d %s entries\n", 
	 NewLKDB, entrycount, (RelativePathFlag ? "relative" : "absolute"));
  return(0);
}


int WalkTree(char *troot, db_type *dbhandle)
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
    int myfd = open(nextf->fts_name, O_RDONLY, 0);
    if (myfd < 0) {
      printf("Skipping %s: %s\n", nextf->fts_path, strerror(errno));
      continue;
    }

    ComputeViceSHA(myfd, shabuf);
    close(myfd);

    /* Construct record to be inserted */
    db_data dk, dd;
    dk.db_datasize = SHA_DIGEST_LENGTH;
    dk.db_dataptr  = shabuf;
    dd.db_datasize = (nextf->fts_pathlen + 1); /* assuming absolute path */
    dd.db_dataptr  = nextf->fts_path; /* assuming absolute path */

    if (RelativePathFlag){/* troot should be prefix of fts_path */
      if (strncmp(nextf->fts_path, troot, troot_strlen)) {
	printf("Weirdness: tree root (%s) not prefix of %s\n",
	       troot, nextf->fts_path);
	close(myfd); rc = -1; goto WalkDone;
      }
      else {
	/* skip over troot and the slash that follows it */
	dd.db_datasize -= (troot_strlen + 1);
	dd.db_dataptr = ((char *)dd.db_dataptr) + (troot_strlen + 1);
      }
    }

    /* Insert record into db */
    if (db_put(dbhandle, &dk, &dd, 0) < 0) {
      printf("%s: insert into database failed\n", nextf->fts_path);
      rc = -1; goto WalkDone;
    }
    
    /* Successfully added one more entry! */
    NumEntries++;
    if (VerboseFlag) {
      char temp[3*SHA_DIGEST_LENGTH];
      ViceSHAtoHex(shabuf, temp, sizeof(temp));
      printf("Entry %05d:  %s  %s\n", NumEntries, temp, (char *)dd.db_dataptr);
    }
    else {
      if ((NumEntries > 99) && (!(NumEntries % 100))) 
	printf("#"); /* indicate progress */
      fflush(stdout);
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
