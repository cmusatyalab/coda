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
#include <sys/types.h>
#include <sys/stat.h>
#include <fts.h>

#ifdef __cplusplus
}
#endif

#include "lka.h"



int VerboseFlag = 0; 
int RelativePathFlag = 0; /* iff true use relative paths; else absolute */
int NumEntries = 0; /* count of entries inserted into database */

char *NewLKDB = 0; /* pathname of lookaside db to be created */
char *TreeRoot = 0; /* pathname of root of tree to be walked and hashed */

/* Forward refs */
int SetDescriptor(DB *, int); 
int WalkTree(char *, DB *); 

main(int argc, char **argv) {
  int i;
  DB *dbh; /* database handle */

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
  dbh = dbopen(NewLKDB, O_RDWR|O_TRUNC|O_CREAT, 0755, DB_HASH, 0);
  if (!dbh) {
    printf("%s: %s\n", NewLKDB, strerror(errno));
    return(-1);
  }

  /* Walk the tree at TreeRoot, inserting records for each file */
  if (WalkTree(TreeRoot, dbh) < 0) { /* error */
    dbh->close(dbh);
    return(-1);
  }

  /* Insert the descriptor record */
  if (SetDescriptor(dbh, NumEntries)) {/* error */
    dbh->close(dbh);
    return(-1);
  }

 DBFillComplete:
  dbh->close(dbh);
  return(0);

 ParseError:
  printf("Usage: mklka [-v] [-r] <newlkdb.lka> <treeroot>\n");
  return(-1);
}

int SetDescriptor(DB *dbhandle, int entrycount) {
  /* Inserts (or replaces) a descriptor record (i.e. key value of 0) into
     open lookaside database specified by dbhandle;
     Called after database creation with an entrycount of 0, and
     at the very end with entrycount equal to number of entries in dbhandle;
     Returns 0 on success;  -1 on error of any kind.
  */
  int dbrc;
  char zerosha[SHA_DIGEST_LENGTH];
  DBT dbkey, dbdata;

  /* Construct zero key */
  memset(zerosha, 0, SHA_DIGEST_LENGTH);
  dbkey.data = zerosha;
  dbkey.size = SHA_DIGEST_LENGTH;

  /* Construct descriptor */
  dbdata.size = strlen(LKA_VERSION_STRING) ;
  if (RelativePathFlag) dbdata.size += strlen(LKA_RELPATH_STRING);
  else dbdata.size += strlen(LKA_ABSPATH_STRING);
  dbdata.size += strlen(LKA_NUMENTRIES_STRING);
  dbdata.size += LKA_NUMENTRIES_FIELD_WIDTH;
  dbdata.size += strlen(LKA_END_STRING);
  dbdata.size++; /* for null terminating string */
  dbdata.data = malloc(dbdata.size);
  if (!dbdata.data) {
    printf("Arrgh ... malloc(%d) failed\n");
    return(-1);
  }
  sprintf((char *)dbdata.data, "%s%s%s%0*d%s", LKA_VERSION_STRING,
	  (RelativePathFlag ? LKA_RELPATH_STRING : LKA_ABSPATH_STRING),
	  LKA_NUMENTRIES_STRING, LKA_NUMENTRIES_FIELD_WIDTH, entrycount,
	  LKA_END_STRING);

  /* Now do the insert */
  dbrc = (dbhandle->put)(dbhandle, &dbkey, &dbdata, 0);
  free(dbdata.data); /* no reason to hang on to it */
  if (dbrc) {
    printf("SetDescriptor() failed: %s\n", strerror(errno));
    return(-1);
  }

  /* success! */
  printf("Created lkdb %s with %d %s entries\n", 
	 NewLKDB, entrycount, (RelativePathFlag ? "relative" : "absolute"));
  return(0);

}


int WalkTree(char *troot, DB *dbhandle) {
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

  while (nextf = fts_read(fth)) {
    if (nextf->fts_info != FTS_F) continue; /* skip all but plain files */

    /* compute the SHA of this file */
    int myfd = open(nextf->fts_name, O_RDONLY, 0);
    if (myfd < 0) {
      printf("Skipping %s: %s\n", nextf->fts_path, strerror(errno));
      continue;
    }

    if (!ComputeViceSHA(myfd, shabuf)) {
      printf("%s: can't compute SHA\n", nextf->fts_name);
      close(myfd); rc = -1; goto WalkDone;
    }
    else close(myfd);

    /* Construct record to be inserted */
    DBT dk, dd;
    dk.size = SHA_DIGEST_LENGTH; dk.data = shabuf;
    dd.size = (nextf->fts_pathlen + 1); /* assuming absolute path */
    dd.data = nextf->fts_path; /* assuming absolute path */

    if (RelativePathFlag){/* troot should be prefix of fts_path */
      if (strncmp(nextf->fts_path, troot, troot_strlen)) {
	printf("Weirdness: tree root (%s) not prefix of %s\n",
	       troot, nextf->fts_path);
	close(myfd); rc = -1; goto WalkDone;
      }
      else {
	/* skip over troot and the slash that follows it */
	dd.size -= (troot_strlen + 1);
	dd.data = ((char *)dd.data) + (troot_strlen + 1);
      }
    }

    /* Insert record into db */
    if ((dbhandle->put)(dbhandle, &dk, &dd, 0) < 0) {
      printf("%s: insert into database failed\n", nextf->fts_path);
      rc = -1; goto WalkDone;
    }
    
    /* Successfully added one more entry! */
    NumEntries++;
    if (VerboseFlag) {
      char temp[3*SHA_DIGEST_LENGTH];
      ViceSHAtoHex(shabuf, temp, sizeof(temp));
      printf("Entry %05d:  %s  %s\n", NumEntries, temp, dd.data);
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
