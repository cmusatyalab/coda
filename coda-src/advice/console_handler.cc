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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/param.h>
#include <sys/types.h>
#include "coda_assert.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include "coda_string.h"

#include <stdlib.h>
#include <unistd.h>


#include <lock.h>
#include <lwp.h>
#include <rpc2.h>
#include <adsrv.h>
#include <admon.h>
#include <util.h>

#ifdef __cplusplus
}
#endif __cplusplus


#include "advice_srv.h"
#include "console_handler.h"
#include "globals.h"
#include "helpers.h"
#include "conversions.h"
#include "mybstree.h"
#include "miss.h"
#include "replacementlog.h"
#include "rpc_setup.h"
#include "srv.h"

#define BLOCKING 1
#define NONBLOCKING 0

extern int CheckClient(char *, char *);

extern char **environ;
extern int errno;

char *ReadFromConsole(int block);

void Hoard();
void HoardFile(char *, int, HoardCmd *);
void HoardDirectory(char *, int, char, HoardCmd *);
void HoardOn();
void HoardOff();

#define CommandFileName "/tmp/hoard_profile"
FILE *CommandFILE;
int HoardOpenCommandFile();
int HoardWriteCommandFile(int, HoardCmd *);
int HoardCloseCommandFile();
int HoardExecCommandFile();

const int MSGSIZE = 1024;

FILE *toCONSOLE;
FILE *fromCONSOLE;

void InitializeCodaConsole() {
  int toAdviceSrv[2];
  int toConsole[2];
  char *args[2];
  int rc;

  rc = pipe(toAdviceSrv);  
  if (rc != 0) {
    printf("Error creating pipe toAdviceSrv\n"); fflush(stdout); exit(-1);
  }
  rc = pipe(toConsole);
  if (rc != 0) {
    printf("Error creating pipe toConsole\n"); fflush(stdout); exit(-1);
  }
  rc = fork();
  if (rc == -1) { /* error */
    printf("ERROR forking process\n"); fflush(stdout); exit(-1);
  } else if (rc) { /* parent -- advice_srv*/
    char msg[MSGSIZE];

    interfacePID = rc;

    toCONSOLE = fdopen(toConsole[1], "w");
    fromCONSOLE = fdopen(toAdviceSrv[0], "r");

  } else {  /* child -- CodaConsole */
    FILE *fromADSRV;
    FILE *toADSRV;
    int newIn, newOut;

    // Close the CodaConsole's stdin
    // Redirect it to come from the toConsole[0] file descriptor
    fclose(stdin);
    newIn = dup(toConsole[0]); 
    CODA_ASSERT(newIn == 0);

    // Close the CodaConsole's stdout
    // Redirect it to the toAdviceSrv[1] file descriptor
    fclose(stdout);
    newOut = dup(toAdviceSrv[1]);
    CODA_ASSERT(newOut == 1);

    // Setup the arguments to the call
    args[0] = CODACONSOLEOUT;
    args[1] = NULL;

    if (execlp(CODACONSOLEOUT, CODACONSOLEOUT, NULL)) {
      fprintf(stderr, "ERROR exec'ing %s (errno=%d)\n", CODACONSOLEOUT, errno); 
      fflush(stderr); 
      chdir(TMPDIR);
      abort();
    }
  }
}

void SendToConsole(char *msg) {
    int rc;

    rc = fprintf(toCONSOLE, "%s", msg);
    CODA_ASSERT(rc == strlen(msg));

    rc = fflush(toCONSOLE);
    CODA_ASSERT(rc == 0);
}

#define READFREQUENCY 1
#define POLL 0

char *ReadFromConsole(int block) {
    static char inputline[BUFSIZ];
    char *rc;
    long lrc;
    int returncode;
    int rdfds;
    int consolefd;
    int ConsoleMask = 0;

    consolefd = fileno(fromCONSOLE);
    ConsoleMask |= (1 << consolefd);
    
    do {
        rc = NULL;
	rdfds = ConsoleMask;

	lrc = IOMGR_Select(32, &rdfds, 0, 0, NULL);

	if (lrc > 0) {
	  if (rdfds & ConsoleMask) {
	    rc = fgets(inputline, BUFSIZ, fromCONSOLE);
	  }
	} else if (lrc < 0) {
	  fprintf(stderr, "\t\t\tRFC: IOMGR_Select received error (lrc = %d)\n", lrc);
	  fflush(stderr);
	}
    } while ((block == 1) && (rc == NULL));                   /* end do */

    if (rc == NULL) {
      CODA_ASSERT(block == 0);
      return(NULL);
    } else {
      fprintf(stderr, "ReadFromConsole: inputline=%s\n", inputline); fflush(stderr);
      return(inputline);
    }
}

int HoardOpenCommandFile() {
    CommandFILE = fopen(CommandFileName, "w");
    if (CommandFILE == NULL)
      return(-1);
    else
      return(0);
}

int HoardCloseCommandFile() {
    return(fclose(CommandFILE));
}

int HoardWriteCommandFile(int number, HoardCmd *commands) {
    for (int i = 0; i < number; i++) {
        switch (commands[i].command) {
            case AddCMD:
	        switch (commands[i].meta) {
  		    case ChildrenPlusMETA:
	                fprintf(CommandFILE, "add %s %d:c+\n", 
				(char *)commands[i].pathname, (int)commands[i].priority);
		        break;
		    case DescendantsPlusMETA:
	                fprintf(CommandFILE, "add %s %d:d+\n", 
				(char *)commands[i].pathname, (int)commands[i].priority);
		        break;
		    default:
		        LogMsg(0, LogLevel, LogFile, 
			       "HoardWriteCommandFile: invalid meta-information for %s, %d\n",
			       (char *)commands[i].pathname, (int)commands[i].priority);
			/* fall-through */
		    case NoneMETA:
	                fprintf(CommandFILE, "add %s %d\n", 
				(char *)commands[i].pathname, (int)commands[i].priority);
		        break;
		}
	        break;

	    case ClearCMD:
	        fprintf(CommandFILE, "clear\n");
	        break;

            case DeleteCMD:
	        fprintf(CommandFILE, "delete %s\n", (char *)commands[i].pathname);
	        break;

            case ListCMD:
	        fprintf(CommandFILE, "list /tmp/hoard_listing\n");
	        break;

            case OffCMD:
	        fprintf(CommandFILE, "off\n");
	        break;

            case OnCMD:
	        fprintf(CommandFILE, "on\n");
	        break;

            case WalkCMD:
	        fprintf(CommandFILE, "walk\n");
	        break;

            case VerifyCMD:
	        fprintf(CommandFILE, "verify /tmp/verify_listing\n");
	        break;

   	    default:
	        break;
	}
    }
    fflush(CommandFILE);
    return -1;
}

int HoardExecCommandFile() {
    char command[MAXPATHLEN];

    snprintf(command, MAXPATHLEN, "hoard -f %s", CommandFileName);
    return(system(command));
}


#define MAXHOARDCMDS 5
void Hoard() {
    HoardCmd commands[MAXHOARDCMDS];
    char *inputline;
    char pathname[MAXPATHLEN];
    int priority;
    char meta;
    int i = 0;
    int numFields;

    CODA_ASSERT(HoardOpenCommandFile() == 0);
    
    inputline = ReadFromConsole(BLOCKING);
    CODA_ASSERT(inputline != NULL);
    while (strncmp(inputline, "END", strlen("END")) != 0) {
      if (strncmp("Output ERROR", inputline, strlen("Output ERROR")) == 0) {
	fprintf(stderr, "Output ERROR:\n");
        inputline = ReadFromConsole(BLOCKING);
	fprintf(stderr, "Error:\n");
    }

      // scanf for the maximum numbers of fields
      numFields = sscanf(inputline, "%d %s %c", &priority, pathname, &meta);

      // assume that we either got two or three fields: a file or a directory
      CODA_ASSERT((numFields == 2) || (numFields == 3));

      // setup the command based upon the number of fields found
      if (numFields == 2)
	HoardFile(pathname, priority, &(commands[i]));
      if (numFields == 3)
	HoardDirectory(pathname, priority, meta, &(commands[i]));
      i++;

      // We have reached the maximum
      if (i == 5) {
        // Ship of the commands
//        CODA_ASSERT(C_HoardCommands(VenusCID, (RPC2_Integer)uid, i, commands) == RPC2_SUCCESS);
	  HoardWriteCommandFile(i, commands);

        // Free up the pathname memory
	for (int j = 0; j < i; j++) {  free(commands[i].pathname);  }

	// and start again
	i = 0;
      }

      // Get next input line
      inputline = ReadFromConsole(BLOCKING);
      CODA_ASSERT(inputline != NULL);
    }

    CODA_ASSERT(strncmp(inputline, "END", strlen("END")) == 0);
//    printf("Make Hoard RPC call\n"); fflush(stdout);
//    CODA_ASSERT(C_HoardCommands(VenusCID, (RPC2_Integer)uid, i, commands) == RPC2_SUCCESS);
    HoardWriteCommandFile(i, commands);

    // Free up the pathname memory
    for (int j = 0; j < i; j++) {  free(commands[i].pathname);	}

    CODA_ASSERT(HoardCloseCommandFile() == 0);
    CODA_ASSERT(HoardExecCommandFile() == 0);
}

void HoardFile(char *pathname, int priority, HoardCmd *cmd) {
    char *path;

    CODA_ASSERT(pathname != NULL);
    path = (char *)malloc(strlen(pathname)+1);
    strncpy(path, pathname, strlen(pathname)+1);

    cmd->command = AddCMD;
    cmd->pathname = (RPC2_String)path;
    cmd->priority = (RPC2_Integer)priority;
    cmd->meta = NoneMETA;
}

void HoardDirectory(char *pathname, int priority, char meta, HoardCmd *cmd) {
    char *path;

    //    printf("Hoard: File=%s Priority=%d Meta=%c\n", pathname, priority, meta);
    //    fflush(stdout);

    CODA_ASSERT(pathname != NULL);
    path = (char *)malloc(strlen(pathname)+1);
    strncpy(path, pathname, strlen(pathname)+1);

    cmd->command = AddCMD;
    cmd->pathname = (RPC2_String)path;
    cmd->priority = (RPC2_Integer)priority;
    cmd->meta = GetMetaInfoID(&meta);
}

void HoardClear() {
    HoardCmd commands[2];
    char pathname[8];

    LogMsg(100, LogLevel, LogFile,
	   "Received request to clear the hoard database\n");

    commands[0].command = ClearCMD;
    commands[0].pathname = (RPC2_String)pathname;
    commands[0].priority = (RPC2_Integer)-1;
    commands[0].meta = NoneMETA;
    
//    CODA_ASSERT(C_HoardCommands(VenusCID, (RPC2_Integer)uid, 1, commands) == RPC2_SUCCESS);

    CODA_ASSERT(HoardOpenCommandFile() == 0);
    HoardWriteCommandFile(1, commands);
    CODA_ASSERT(HoardCloseCommandFile() == 0);
    CODA_ASSERT(HoardExecCommandFile() == 0);

    LogMsg(100, LogLevel, LogFile,
	   "Completed clear request\n");

}

void HoardList() {
    HoardCmd commands[2];
    char pathname[8];

    LogMsg(100, LogLevel, LogFile, 
	   "Received request to list the hoard database\n");

    commands[0].command = ListCMD;
    commands[0].pathname = (RPC2_String)pathname;
    commands[0].priority = (RPC2_Integer)-1;
    commands[0].meta = NoneMETA;
    
//    CODA_ASSERT(C_HoardCommands(VenusCID, (RPC2_Integer)uid, 1, commands) == RPC2_SUCCESS);

    CODA_ASSERT(HoardOpenCommandFile() == 0);
    HoardWriteCommandFile(1, commands);
    CODA_ASSERT(HoardCloseCommandFile() == 0);
    CODA_ASSERT(HoardExecCommandFile() == 0);

}

void HoardOff() {
    HoardCmd commands[2];
    char pathname[8];

    LogMsg(100, LogLevel, LogFile,
	   "Received request to turn periodic hoard walks off\n");

    commands[0].command = OffCMD;
    commands[0].pathname = (RPC2_String)pathname;
    commands[0].priority = (RPC2_Integer)-1;
    commands[0].meta = NoneMETA;
    
//    CODA_ASSERT(C_HoardCommands(VenusCID, (RPC2_Integer)uid, 1, commands) == RPC2_SUCCESS);

    CODA_ASSERT(HoardOpenCommandFile() == 0);
    HoardWriteCommandFile(1, commands);
    CODA_ASSERT(HoardCloseCommandFile() == 0);
    CODA_ASSERT(HoardExecCommandFile() == 0);

}

void HoardOn() {
    HoardCmd commands[2];
    char pathname[8];

    LogMsg(100, LogLevel, LogFile,
	   "Received request to turn periodic hoard walks on\n");

    commands[0].command = OnCMD;
    commands[0].pathname = (RPC2_String)pathname;
    commands[0].priority = (RPC2_Integer)-1;
    commands[0].meta = NoneMETA;
    
//    CODA_ASSERT(C_HoardCommands(VenusCID, (RPC2_Integer)uid, 1, commands) == RPC2_SUCCESS);

    CODA_ASSERT(HoardOpenCommandFile() == 0);
    HoardWriteCommandFile(1, commands);
    CODA_ASSERT(HoardCloseCommandFile() == 0);
    CODA_ASSERT(HoardExecCommandFile() == 0);

}

void HoardWalk() {
    HoardCmd commands[2];
    char pathname[8];

    LogMsg(100, LogLevel, LogFile,
	   "Received request to walk the hoard database\n"); 

    commands[0].command = WalkCMD;
    commands[0].pathname = (RPC2_String)pathname;
    commands[0].priority = (RPC2_Integer)-1;
    commands[0].meta = NoneMETA;
    
//    CODA_ASSERT(C_HoardCommands(VenusCID, (RPC2_Integer)uid, 1, commands) == RPC2_SUCCESS);

    CODA_ASSERT(HoardOpenCommandFile() == 0);
    HoardWriteCommandFile(1, commands);
    CODA_ASSERT(HoardCloseCommandFile() == 0);
    CODA_ASSERT(HoardExecCommandFile() == 0);

}

void HoardVerify() {
    HoardCmd commands[2];
    char pathname[8];

    LogMsg(100, LogLevel, LogFile,
	   "Received request to verify the hoard database\n");

    commands[0].command = VerifyCMD;
    commands[0].pathname = (RPC2_String)pathname;
    commands[0].priority = (RPC2_Integer)-1;
    commands[0].meta = NoneMETA;
    
//    CODA_ASSERT(C_HoardCommands(VenusCID, (RPC2_Integer)uid, 1, commands) == RPC2_SUCCESS);

    CODA_ASSERT(HoardOpenCommandFile() == 0);
    HoardWriteCommandFile(1, commands);
    CODA_ASSERT(HoardCloseCommandFile() == 0);
    CODA_ASSERT(HoardExecCommandFile() == 0);

}

void RegisterInterests() {
    char *inputline;
    char eventOfInterest[MAXEVENTLEN+1];
    InterestValuePair interests[MAXEVENTS+1];
    int i = 0;
    long rc;

    inputline = ReadFromConsole(BLOCKING);
    CODA_ASSERT(inputline != NULL);
    while ((strncmp(inputline, "END", strlen("END")) != 0) && (i < MAXEVENTS)) {
      Yield();
        sscanf(inputline, "%s\n", eventOfInterest);
	
	if (strncmp(eventOfInterest, "OperatingStronglyConnected", strlen(eventOfInterest)) == 0) {
            interests[i].interest = ServerAccessibleID;
            interests[i].value = 1;
            interests[i].argument = 0;
            i++;
            interests[i].interest = ServerConnectionStrongID;
            interests[i].value = 1;
            interests[i].argument = 0;
            i++;
	} else if (strncmp(eventOfInterest, "OperatingWeaklyConnected", strlen(eventOfInterest)) == 0) {
            interests[i].interest = NetworkQualityEstimateID;
            interests[i].value = 1;
            interests[i].argument = 0;
            i++;
            interests[i].interest = ServerConnectionWeakID;
            interests[i].value = 1;
            interests[i].argument = 0;
            i++;

	} else if (strncmp(eventOfInterest, "OperatingDisconnected", strlen(eventOfInterest)) == 0) {
            interests[i].interest = ServerInaccessibleID;
            interests[i].value = 1;
            interests[i].argument = 0;
            i++;

	} else {
            interests[i].interest = GetInterestID(eventOfInterest);
            interests[i].value = 1;
            interests[i].argument = 0;
            i++;
	}
        inputline = ReadFromConsole(BLOCKING);
        CODA_ASSERT(inputline != NULL);
    }
    rc = (int)(strncmp(inputline, "END", strlen("END")));
    CODA_ASSERT(rc == 0);
    ObtainWriteLock(&VenusLock);
    rc = C_RegisterInterest(VenusCID, (RPC2_Integer)uid, i, interests);
    ReleaseWriteLock(&VenusLock);
    CODA_ASSERT(rc == RPC2_SUCCESS);
}

void CheckNetworkConnectivityForFilters() {
  char *inputline;
  char msg[MSGSIZE];
  char client[256], clientOriginal[256];
  char server[256], serverOriginal[256];
  int rc;

  /* Save the name of the client */
  inputline = ReadFromConsole(BLOCKING);
  CODA_ASSERT(inputline != NULL);
  sscanf(inputline, "client = %s\n", clientOriginal);
  strcpy(client, clientOriginal);
  toUpper(client);

  inputline = ReadFromConsole(BLOCKING);
  CODA_ASSERT(inputline != NULL);
  /* For each server... */
  while (strncmp(inputline, "END", strlen("END")) != 0) {

      /* Save the name of the server */
      sscanf(inputline, "server = %s\n", serverOriginal);
      strcpy(server, serverOriginal);
      toUpper(server);
      strcat(server, ".CODA.CS.CMU.EDU");

      /* Check for filters between the client and this server */
      rc = CheckClient(client,server);
      if (rc == 0)  {
	  /* No filters */
	  sprintf(msg, "NaturalNetwork %s\n", serverOriginal);
	  SendToConsole(msg);
      }
      else if (rc == 1) {
	  /* Filter exists */
	  sprintf(msg, "ArtificialNetwork %s\n", serverOriginal);
	  SendToConsole(msg);
      } else {
	  /* Give benefit of doubt */
	  sprintf(msg, "NaturalNetwork %s\n", serverOriginal);
	  SendToConsole(msg);
      }
      
      /* Get the next line of input */
      inputline = ReadFromConsole(BLOCKING);
      CODA_ASSERT(inputline != NULL);
  }
}

void ProcessInputFromConsole() {
    char msg[MSGSIZE];
    char *rfc;
    static int count = 0;

    count++;
    rfc = ReadFromConsole(NONBLOCKING);
    while (rfc != NULL) {
        if (strncmp(rfc, "ClearHoardDatabase",
		    strlen("ClearHoardDatabase")) == 0) {
	  HoardClear();
	  
	} else if (strncmp(rfc, "DisconnectedCacheMissQuestionnaire",
			   strlen("DisconnectedCacheMissQuestionnaire")) == 0) {
	  CODA_ASSERT(LWP_SignalProcess(&discomissSync) == LWP_SUCCESS);

	} else if (strncmp(rfc, "HoardPeriodicOn",
			   strlen("HoardPeriodicOn")) == 0) {
	  HoardOn();
	} else if (strncmp(rfc, "HoardPeriodicOff",
			   strlen("HoardPeriodicOff")) == 0) {
	  HoardOff();
	} else if (strncmp(rfc, "Hoard Advice Available",
			   strlen("Hoard Advice Available")) == 0) {
	  CODA_ASSERT(LWP_SignalProcess(&hoardwalkSync) == LWP_SUCCESS);
	} else if (strncmp(rfc, "HoardWalkAdvice",
			   strlen("HoardWalkAdvice")) == 0) {
	  // Ignore this line of input
	} else if (strncmp(rfc, "Hoard",
			   strlen("Hoard")) == 0) {
	  LogMsg(100, LogLevel, LogFile,
		 "Received request to hoard\n");
	  Hoard();
	} else if (strncmp(rfc, "WeakMissQuestionnaire  returns",
			  strlen("WeakMissQuestionnaire")) == 0) {
	  CODA_ASSERT(sscanf(rfc, "WeakMissQuestionnaire returns %d\n", &weakmissAnswer) == 1);
	  CODA_ASSERT(LWP_SignalProcess(&weakmissSync) == LWP_SUCCESS);

	} else if (strncmp(rfc, "ReadMissQuestionnaire",
			  strlen("ReadMissQuestionnaire")) == 0) {
	  CODA_ASSERT(sscanf(rfc, "ReadMissQuestionnaire returns %d\n", &readmissAnswer) == 1);
	  CODA_ASSERT(LWP_SignalProcess(&readmissSync) == LWP_SUCCESS);

	} else if (strncmp(rfc, "RegisterEvents",
			   strlen("RegisterEvents")) == 0) {
	  
	  RegisterInterests();
	} else if (strncmp(rfc, "GetCacheStatistics",
			   strlen("GetCacheStatistics")) == 0) {
	  long rc;
	  int FilesAllocated, FilesOccupied;
	  int BlocksAllocated, BlocksOccupied;
	  int RVMAllocated, RVMOccupied;

	  ObtainWriteLock(&VenusLock);
fprintf(stderr, "getting cache statistics from venus\n"); fflush(stderr);
	  rc = C_GetCacheStatistics(VenusCID, 
				    (RPC2_Integer *)&FilesAllocated,
				    (RPC2_Integer *)&FilesOccupied,
				    (RPC2_Integer *)&BlocksAllocated,
				    (RPC2_Integer *)&BlocksOccupied,
				    (RPC2_Integer *)&RVMAllocated,
				    (RPC2_Integer *)&RVMOccupied);
fprintf(stderr, "got those stats\n"); fflush(stderr);
	  ReleaseWriteLock(&VenusLock);
	  if (rc != RPC2_SUCCESS) {
	    fprintf(stderr, "rc = %d\n", rc); 
	    fflush(stderr);
	  }
	  CODA_ASSERT(rc == RPC2_SUCCESS);

	  // Send the information to CodaConsole
	  sprintf(msg, "%d %d %d %d %d %d\n",
		   FilesAllocated, FilesOccupied,
		   BlocksAllocated, BlocksOccupied,
		   RVMAllocated, RVMOccupied);
	  SendToConsole(msg);

	} else if (strncmp(rfc, "GetUsageStats",
			   strlen("GetUsageStats")) == 0) {
	   long rc;
	   char filename[MAXPATHLEN];
           int sinceLastUse, percentUsed, totalUsed;

           CODA_ASSERT(sscanf(rfc, "GetUsageStats %d %d %d\n", &sinceLastUse, &percentUsed, &totalUsed) == 3);

	   snprintf(filename, MAXPATHLEN, "/tmp/usage_statistics");
	   unlink(filename);

	   ObtainWriteLock(&VenusLock);
           rc = C_OutputUsageStatistics(VenusCID, 
					(RPC2_Integer)uid, 
					(RPC2_String)filename, 
					(RPC2_Integer) sinceLastUse, 
					(RPC2_Integer) percentUsed, 
					(RPC2_Integer) totalUsed);
	   ReleaseWriteLock(&VenusLock);
	   CODA_ASSERT(rc == RPC2_SUCCESS);

	   OutputMissStatistics();
	   OutputReplacementStatistics();
	   sprintf(msg, "UsageStatisticsAvailable %s %s %s\n", 
		   filename, TMPMISSLIST, TMPREPLACELIST);
	   SendToConsole(msg);

	} else if (strncmp(rfc, "GetNetworkConnectivityInformation",
			   strlen("GetNetworkConnectivityInformation")) == 0) {

	  LogMsg(100, LogLevel, LogFile,
		 "Received request for network connectivity information\n");
	  CheckNetworkConnectivityForFilters();

	} else if (strncmp(rfc, "GetListOfServerNames",
			   strlen("GetListOfServerNames")) == 0) {
		long rc,i;
		RPC2_Integer numservers = MAXSERVLISTLEN;
		ServerEnt servers[MAXSERVLISTLEN];
		char srv_names[MAXSERVLISTLEN][MAXHOSTLENGTH];

		for(i=0;i<numservers;i++)
		    servers[i].name = (RPC2_String)srv_names[i];
		
		ObtainWriteLock(&VenusLock);
		fprintf(stderr, "getting servers from venus\n"); fflush(stderr);
		rc = C_GetServerInformation(VenusCID,MAXSERVERS,&numservers, servers);
		fprintf(stderr, "got server list\n"); fflush(stderr);
		ReleaseWriteLock(&VenusLock);
		if (rc != RPC2_SUCCESS) {
			fprintf(stderr, "rc = %d\n", rc); 
			fflush(stderr);
		}
		CODA_ASSERT(rc == RPC2_SUCCESS);
		
		// Send the information to CodaConsole 
		//sprintf(msg,"%d\n",numservers);
		for(i=0;i<numservers;i++) {
			sprintf(msg, "%s ", servers[i].name);
		} 
		strcat(msg,"\n");
		SendToConsole(msg);
	} else if (strncmp(rfc, "", strlen("")) == 0) {
	  /* Input line was empty -- ignore it */
	}
	else {
	  fprintf(stderr, "Input Unrecognized: \n%s\n\n", rfc); 
	  fflush(stderr);
	}

	/* Get next line of input */
        rfc = ReadFromConsole(NONBLOCKING);
    }


}

