/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include "coda_assert.h"
#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#ifdef HAVE_DB_185_H
#include <db_185.h>
#else 
#include <db.h>
#endif

#include <lock.h>
#include <lwp.h>
#include <util.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include "advice_srv.h"
#include "replacementlog.h"

#define DATASIZE 128
#define DBBLOCKSIZE 512

char GhostDB[MAXPATHLEN];
struct Lock GhostLock;


MakeDatum(char *string, DBT *element) {
    element->data = string;
    element->size = strlen(string) + 1;
}


StoreDatum(DB *db, DBT *key, DBT *content) {
    int rc; 

    rc = db->put(db, key, content, 0);
    if (rc != RET_SUCCESS) {
        LogMsg(100, LogLevel, LogFile, 
	       "StoreDatum: Element not inserted into GhDB (Key=%s, Data=%s)\n", 
	       key->data, content->data);
        fprintf(stderr, 
		"StoreDatum: Element not inserted into GhDB (Key=%s, Data=%s)\n", 
		key->data, content->data);
        fflush(stderr);
        return(-1);
    }

    return(0);
}

GetDatum(DB *db, DBT *key, int *status, int *data) {
  DBT content;
  int rc;

  *status = 0;
  *data = 0;

  rc = db->get(db, key, &content, 0);
  if (rc == RET_SUCCESS) {
      int rc = sscanf(content.data, "%d %d", status, data);
      CODA_ASSERT(rc == 2);
      // free(content.data);
  }
}

void UpdateReplacementRecord(DB *db, char *path, int status, int data) {
    int oldStatus, oldData;
    int newStatus, newData;
    DBT key, content;
    char dataString[DATASIZE];

    MakeDatum(path, &key);
    GetDatum(db, &key, &oldStatus, &oldData);
    newStatus = oldStatus + status;
    newData = oldData + data;
    snprintf(dataString, DATASIZE, "%d %d", newStatus, newData);
    MakeDatum(dataString, &content);

    StoreDatum(db, &key, &content);
}

int StringContainsQuestions(char *path) {
    char *rc;
    int questionIndex;

    rc = index(path, '?');
    while (rc != NULL) {
        if (strncmp(rc, "???", 3) == 0)
	    return(1);
	rc++;
	rc = index(rc, '?');
    }
    return(0);
}

void ParseReplacementLog(char *filename) {
    FILE *replacementLog;
    DB *db;
    char line[MAXPATHLEN];
    char *returnValue;
    int rc;
    int status, data;
    char path[MAXPATHLEN];
    int lineNumber = 0;

    ObtainWriteLock(&GhostLock);

    replacementLog = fopen(filename, "r");
    if (replacementLog == NULL) {
        LogMsg(100, LogLevel, LogFile,
	       "ParseReplacementLog:  Could not open replacement long (%s)\n",
	       filename);
        return;
    }

    db = dbopen(GhostDB, O_RDWR | O_CREAT, 0600, DB_BTREE, NULL);
    if (db == NULL) {
        LogMsg(100, LogLevel, LogFile,
	       "ParseReplacementLog:  Could not open database (%s)\n",
	       GhostDB);
	LogMsg(100, LogLevel, LogFile, 
	       "db error msg: %s\n", strerror(errno));
	fclose(replacementLog);
        return;
    }

    returnValue = fgets(line, MAXPATHLEN, replacementLog);
    while (returnValue != NULL) {
        lineNumber++;
	if ((lineNumber % 10) == 0)
	  Yield();

	rc = sscanf(line, "%s %d %d\n", path, &status, &data);
	if (rc != 3) {
	  LogMsg(100, LogLevel, LogFile,
		 "ReplacementLog: Ignoring record with invalid format: %s\n", 
		 line);
	  returnValue = fgets(line, MAXPATHLEN, replacementLog);
	  continue;
	}

	if (StringContainsQuestions(path)) {
	     LogMsg(100, LogLevel, LogFile, 
		    "ReplacementLog: Ignoring record with questionable path: %s\n",
		    path);
	     returnValue = fgets(line, MAXPATHLEN, replacementLog);
	     continue;
	}

	UpdateReplacementRecord(db, path, status, data);

	returnValue = fgets(line, MAXPATHLEN, replacementLog);
    }

    db->close(db);
    fclose(replacementLog);

    ReleaseWriteLock(&GhostLock);

    return;
}

void PrintGhostDB() {
  DB *db;
  DBT key, content;
  int status, data;
  int rc;

  ObtainReadLock(&GhostLock);

  db = dbopen(GhostDB, O_RDWR | O_CREAT, 0600, DB_BTREE, NULL);

  printf("\n\nPrinting the database:\n");
  rc = db->seq(db, &key, &content, R_FIRST);
  while (rc == RET_SUCCESS) {
    rc = sscanf(content.data, "%d %d", &status, &data);
    CODA_ASSERT(rc == 2);

    printf("  key=%s content = <%d, %d>\n",key.data, status, data);
    rc = db->seq(db, &key, &content, R_NEXT);
  }

  printf("\n\n");

  db->close(db);

  ReleaseReadLock(&GhostLock);
}

void OutputReplacementStatistics() {
  FILE *outfile;
  DB *db;
  DBT key, content;
  int status, data;
  int rc;

  ObtainReadLock(&GhostLock);

  outfile = fopen(TMPREPLACELIST, "w+");
  if (outfile == NULL) {
      LogMsg(0, LogLevel, LogFile, "OutputReplacementStatistics: cannot open %s\n", TMPREPLACELIST);
      return;
  }

  db = dbopen(GhostDB, O_RDWR | O_CREAT, 0600, DB_BTREE, NULL);

  rc = db->seq(db, &key, &content, R_FIRST);
  while (rc == RET_SUCCESS) {
    rc = sscanf(content.data, "%d %d", &status, &data);
    CODA_ASSERT(rc == 2);

    if ((status >= MAXSTATUSREPLACEMENTS) || (data >= MAXDATAREPLACEMENTS))
	fprintf(outfile, "%s\n",key.data);

    rc = db->seq(db, &key, &content, R_NEXT);
  }

  db->close(db);
  fflush(outfile);
  fclose(outfile);
  ReleaseReadLock(&GhostLock);
}

Find(char *path) {
  DB *db;
  DBT key;
  int oldStatus, oldData;

  ObtainReadLock(&GhostLock);
  db = dbopen(GhostDB, O_RDWR | O_CREAT, 0600, DB_BTREE, NULL);

  MakeDatum(path, &key);
  GetDatum(db, &key, &oldStatus, &oldData);
  printf("%s (%d) is found with %d and %d\n", 
	 path, strlen(path), oldStatus, oldData);

  db->close(db);
  ReleaseReadLock(&GhostLock);
}

