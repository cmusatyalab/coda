#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include "coda_assert.h"
#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gdbm.h>

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


MakeDatum(char *string, datum *element) {
    element->dptr = string;
    element->dsize = strlen(string) + 1;
}


StoreDatum(GDBM_FILE db, datum *key, datum *content) {
    int rc; 

    rc = gdbm_store(db, *key, *content, GDBM_REPLACE);
    if (rc != 0) {
        LogMsg(100, LogLevel, LogFile, 
	       "StoreDatum: Element not inserted into GhDB (Key=%s, Data=%s)\n", 
	       key->dptr, content->dptr);
        fprintf(stderr, 
		"StoreDatum: Element not inserted into GhDB (Key=%s, Data=%s)\n", 
		key->dptr, content->dptr);
        fflush(stderr);
        return(-1);
    }

    return(0);
}

GetDatum(GDBM_FILE db, datum *key, int *status, int *data) {
  datum content;

  *status = 0;
  *data = 0;

  content = gdbm_fetch(db, *key);
  if (content.dptr != NULL) {
      int rc = sscanf(content.dptr, "%d %d", status, data);
      CODA_ASSERT(rc == 2);
      free(content.dptr);
  }
}

void UpdateReplacementRecord(GDBM_FILE db, char *path, int status, int data) {
    int oldStatus, oldData;
    int newStatus, newData;
    datum key, content;
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
    GDBM_FILE db;
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

    db = gdbm_open(GhostDB, DBBLOCKSIZE, GDBM_WRCREAT, 0644, NULL);
    if (db == NULL) {
        LogMsg(100, LogLevel, LogFile,
	       "ParseReplacementLog:  Could not open database (%s)\n",
	       GhostDB);
	LogMsg(100, LogLevel, LogFile, 
	       "gdbm error msg: %s\n", gdbm_strerror(gdbm_errno));
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

    gdbm_close(db);
    fclose(replacementLog);

    ReleaseWriteLock(&GhostLock);

    return;
}

void PrintGhostDB() {
  GDBM_FILE db;
  datum key;
  int status, data;

  ObtainReadLock(&GhostLock);

  db = gdbm_open(GhostDB, DBBLOCKSIZE, GDBM_WRCREAT, 0644, NULL);

  printf("\n\nPrinting the database:\n");
  key = gdbm_firstkey(db);
  while (key.dptr) {
    GetDatum(db, &key, &status, &data);
    printf("  key=%s content = <%d,%d>\n",key.dptr, status, data);
    key = gdbm_nextkey(db, key);
  }

  printf("\n\n");

  gdbm_close(db);

  ReleaseReadLock(&GhostLock);
}

void OutputReplacementStatistics() {
  FILE *outfile;
  GDBM_FILE db;
  datum key;
  int status, data;

  ObtainReadLock(&GhostLock);

  outfile = fopen(TMPREPLACELIST, "w+");
  if (outfile == NULL) {
      LogMsg(0, LogLevel, LogFile, "OutputReplacementStatistics: cannot open %s\n", TMPREPLACELIST);
      return;
  }

  db = gdbm_open(GhostDB, DBBLOCKSIZE, GDBM_WRCREAT, 0644, NULL);

  key = gdbm_firstkey(db);
  while (key.dptr) {
    GetDatum(db, &key, &status, &data);
    if ((status >= MAXSTATUSREPLACEMENTS) || (data >= MAXDATAREPLACEMENTS))
	fprintf(outfile, "%s\n",key.dptr);
    key = gdbm_nextkey(db, key);
  }

  gdbm_close(db);
  fflush(outfile);
  fclose(outfile);
  ReleaseReadLock(&GhostLock);
}

Find(char *path) {
  GDBM_FILE db;
  datum key;
  int oldStatus, oldData;

  ObtainReadLock(&GhostLock);
  db = gdbm_open(GhostDB, DBBLOCKSIZE, GDBM_WRCREAT, 0644, NULL);

  MakeDatum(path, &key);
  GetDatum(db, &key, &oldStatus, &oldData);
  printf("%s (%d) is found with %d and %d\n", 
	 path, strlen(path), oldStatus, oldData);

  gdbm_close(db);
  ReleaseReadLock(&GhostLock);
}

