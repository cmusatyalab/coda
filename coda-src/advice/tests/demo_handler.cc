extern "C" {

#include <sys/types.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __MACH__
#include <sysent.h>
#include <libc.h>

/* Mach definition differs from Linux and NetBSD...  Translate */
#define O_NONBLOCK FNDELAY

#else   /* __linux__ || __BSD44__ */
#include <stdlib.h>
#include <unistd.h>
#endif

#include <lwp.h>
#include <rpc2.h>

}

#include "admon.h"
#include "adsrv.h"

#include "demo_handler.h"
#include "globals.h"

#define TRUE 1
#define FALSE 0

extern char **environ;

const int MSGSIZE = 1024;

FILE *toDEMO;
FILE *fromDEMO;

extern RPC2_Handle handle;

extern int InterestedInReadDisco;

void InitializeCodaDemo() {
  int toAdviceSrv[2];
  int toDemo[2];
  char *args[2];
  int rc;

  rc = pipe(toAdviceSrv);  
  if (rc != 0) {
    printf("Error creating pipe toAdviceSrv\n"); fflush(stdout); exit(-1);
  }
  rc = pipe(toDemo);
  if (rc != 0) {
    printf("Error creating pipe toDemo\n"); fflush(stdout); exit(-1);
  }
  rc = fork();
  if (rc == -1) { /* error */
    printf("ERROR forking process\n"); fflush(stdout); exit(-1);
  } else if (rc) { /* parent -- advice_srv*/
    char msg[MSGSIZE];

    toDEMO = fdopen(toDemo[1], "w");
    fromDEMO = fdopen(toAdviceSrv[0], "r");

    rc = fcntl(fileno(fromDEMO), F_SETFL, O_NONBLOCK);
    if (rc == -1) {
      printf("ERROR setting input fromDEMO as non-blocking\n"); fflush(stdout); exit(-1);
    }

sleep(3);
    sprintf(msg, "source %s\n", CODADEMO);
fprintf(stderr,"Sending %s to demo\n",CODADEMO); fflush(stderr);
    SendToDemo(msg);

  } else {  /* child -- CodaDemo */
    FILE *fromADSRV;
    FILE *toADSRV;
    int newIn, newOut;

    setenv("TCL_LIBRARY", TCL, 1);
    setenv("TK_LIBRARY", TK, 1);

    /* Close the CodaDemo's stdin and redirect it to come from the toDemo[0] file descriptor */
    fclose(stdin);
    newIn = dup(toDemo[0]); 
    assert(newIn == 0);

    fclose(stdout);
    newOut = dup(toAdviceSrv[1]);
    assert(newOut == 1);

    args[0] = CODACONSOLEOUT;
    args[1] = NULL;

    if (execve(CODACONSOLEOUT, args, environ)) {
      fprintf(stderr,"ERROR exec'ing child\n"); fflush(stderr); exit(-1);
    }
  }
}

void SendToDemo(char *msg) {
    int rc;

    rc = fprintf(toDEMO, "%s", msg);
 #ifdef __MACH__
    assert(rc == 0);
#else
    assert(rc == strlen(msg));
#endif /* __MACH__*/
    rc = fflush(toDEMO);
    assert(rc == 0);
}


char *ReadFromDemo() {
    static char inputline[BUFSIZ];
    char *rc;

    rc = fgets(inputline, BUFSIZ, fromDEMO);
    if (rc == NULL) 
      return(NULL);
    else
      return(inputline);
}

int CompareInputCommand(char *rfd, char *compare) {
    if (strncmp(rfd, compare, strlen(compare)) == 0)
        return TRUE;
    else
        return FALSE;
}

int MkHoardFile(char *path) {
  FILE *P;

  P = fopen(path, "w");
  assert(P != NULL);
  
  fprintf(P, "Cache Space Allocated: 12500 files (100000 blocks)\n");
  fprintf(P, "Cache Space Occupied: 708 files (6884 blocks)\n");
  fprintf(P, "Speed of Network Connection = 0 Bytes/sec\n");
  fprintf(P, "7f00033c.5c.31 & 1 & /coda/misc/tex/i386_mach/omega/bin/virtex & 700 & 934 & 34\n");
  fprintf(P, "7f00033c.5c.31 & 1 & /coda/misc/tex/i386_mach/omega/bin/bibtex & 700 & 439 & 34\n");
  fprintf(P, "7f00025f.d238.1dd89 & 1 & /coda/usr/satya/papers/s16/FIGS/impulse-down.eps & 800 & 634 & 79\n");
  fprintf(P, "7f00025f.1f2.1fdf5 & 1 & /coda/usr/satya/papers/s16/talk.saved/demand.eps & 800 & 918 & 22\n");
  fprintf(P, "7f00025f.cc50.1dce8 & 1 & /coda/usr/satya/papers/s16/SUBMISSION/FIGS/xanim.eps & 800 & 894 & 75\n");
  fprintf(P, "7f00025f.1ec.1fdf4 & 1 & /coda/usr/satya/papers/s16/talk.saved/stepdown.eps & 800 & 924 & 79\n");
  fprintf(P, "7f00025f.d052.1dd38 & 1 & /coda/usr/satya/papers/s16/SUBMISSION/s16-submission.tex & 800 & 452 & 82\n");
  fprintf(P, "7f00025f.d1c6.1dd76 & 1 & /coda/usr/satya/papers/s16/FIGS/clntarch.cdr.BOGUS & 800 & 634 & 19\n");
  fprintf(P, "7f00025f.d0e2.1dd50 & 1 & /coda/usr/satya/papers/s16/TABS/tsop.tex.aux & 800 & 123 & 1\n");
  fprintf(P, "7f00033c.56.30 & 1 & /coda/usr/mre/bovik/letters/recommend/bnoble/job.tex & 1000 & 34 & 1\n");
  fprintf(P, "7f00033c.68.33 & 1 & /coda/usr/mre/bovik/letters/recommend/mre/job/ & 1000 & 65 & 1 \n");
  fprintf(P, "7f00033c.68.33 & 1 & /coda/usr/mre/bovik/letters/recommend/mre/job.tex & 1000 & 13 & 1\n");
  fprintf(P, "7f00033c.80.37 & 1 & /coda/usr/mre/bovik/letters/recommend/lily/job.tex & 1000 & 65 & 1\n");
  fprintf(P, "7f00033c.74.35 & 1 & /coda/usr/mre/bovik/letters/recommend/garth/review.tex & 1000 & 25 & 1\n");
  fprintf(P, "7f00033c.62.32 & 1 & /coda/usr/mre/bovik/letters/recommend/mre/xerox-internship.tex & 1000 & 87 & 1\n");
  fprintf(P, "7f00033c.7a.36 & 1 & /coda/usr/mre/bovik/letters/recommend/garth/tenure.tex & 1000 & 345 & 1\n");
  fprintf(P, "7f00033c.6e.34 & 1 & /coda/usr/mre/bovik/letters/recommend/dbj/review.tex & 1000 & 54 & 1\n");
  fprintf(P, "7f00033c.50.2f & 1 & /coda/usr/mre/bovik/letters/recommend/bnoble/perf.tex & 1000 & 12 & 1\n");
  fprintf(P, "7f00033c.5c.31 & 1 & /coda/usr/mre/bovik/letters/recommend/mre/xerox-fellowship.tex & 1000 & 39 & 1\n");

  fflush(P);
  fclose(P);
}

void ProcessInputFromDemo() {
    char *rfd;
    static int count = 0;

    count++;
    rfd = ReadFromDemo();
    while (rfd != NULL) {
        if (CompareInputCommand(rfd, "LostConnection") == TRUE) {
	    C_LostConnection(handle);
	    fflush(stdout);
	    exit(0);
        } else if (CompareInputCommand(rfd, "TokensAcquiredEvent") == TRUE) {
	    RPC2_Integer EndTimestamp;

	    EndTimestamp = 0;
	    C_TokensAcquired(handle, EndTimestamp);
	} else if (CompareInputCommand(rfd, "TokenExpiryEvent") == TRUE) {
	    C_TokensExpired(handle);
	} else if (CompareInputCommand(rfd, "ActivityPendingTokensEvent") == TRUE) {
	    char activityString[64];
	    ActivityID activityType;
	    RPC2_String argument;

	    sscanf(rfd+strlen("ActivityPendingTokensEvent"), "%s %s", activityString, argument);
	    if (strncmp(activityString, "reintegrate", strlen("reintegrate")) == 0) 
	      activityType = ObjectNeedsReintegration;
	    else if (strncmp(activityString, "repair", strlen("repair")) == 0) 
	      activityType = ObjectNeedsReintegration;
	     else 
	       activityType = FileAccess;

	    C_ActivityPendingTokens(handle, activityType, argument);

	} else if (CompareInputCommand(rfd, "UpdateSpaceStatistics") == TRUE) {
	    char spaceString1[16];
	    char spaceString2[16];
	    char spaceString3[16];
	    RPC2_Integer space1;
	    RPC2_Integer space2;
	    RPC2_Integer space3;

	    sscanf(rfd, "UpdateSpaceStatistics %s %s %s", spaceString1, spaceString2, spaceString3);
	    space1 = (RPC2_Integer)atoi(spaceString1);
	    space2 = (RPC2_Integer)atoi(spaceString2);
	    space3 = (RPC2_Integer)atoi(spaceString3);
	    C_SpaceInformation(handle, space1,space2,space3,False);
	} else if (CompareInputCommand(rfd, "ServerAccessible") == TRUE) {
	    char server[16];
	    sscanf(rfd, "ServerAccessible %s", server);
	    C_ServerAccessible(handle, (RPC2_String)server);
	} else if (CompareInputCommand(rfd, "ServerInaccessible") == TRUE) {
	    char server[16];
	    sscanf(rfd, "ServerInaccessible %s", (RPC2_String)server);
	    C_ServerInaccessible(handle, (RPC2_String)server);
	} else if (CompareInputCommand(rfd, "ServerConnectionStrongEvent") == TRUE) {
	    char server[16];
	    sscanf(rfd, "ServerConnectionStrongEvent %s", (RPC2_String)server);
	    C_ServerConnectionStrong(handle, (RPC2_String)server);
	} else if (CompareInputCommand(rfd, "ServerConnectionWeakEvent") == TRUE) {
	    char server[16];
	    sscanf(rfd, "ServerConnectionWeakEvent %s", (RPC2_String)server);
	    C_ServerConnectionWeak(handle, (RPC2_String)server);
	} else if (CompareInputCommand(rfd, "ServerConnectionGoneEvent") == TRUE) {
	    char server[16];
	    sscanf(rfd, "ServerConnectionGoneEvent %s", (RPC2_String)server);
	    C_ServerInaccessible(handle, (RPC2_String)server);
	} else if (CompareInputCommand(rfd, "ServerBandwidthEstimateEvent") == TRUE) {
	    char *inString;
	    char *endString;
	    char estimateString[9];
	    char serverStrings[MAXSERVERS][16];
	    QualityEstimate serverList[MAXSERVERS];
	    int serverCount = 0;
	    int rc;
	    
	    inString = rfd+strlen("ServerBandwidthEstimateEvent");
	    endString = rfd+strlen(rfd);
	    while ((inString < endString) && (serverCount < MAXSERVERS)) {
	        rc = sscanf(inString, " %s %s", serverStrings[serverCount], estimateString);
		if (rc != 2) break;
		serverList[serverCount].ServerName = (RPC2_String)serverStrings[serverCount];
		serverList[serverCount].BandwidthEstimate = (RPC2_Integer)atoi(estimateString);
		serverList[serverCount].Intermittent = False;
		inString += (strlen(serverStrings[serverCount])+1);
		inString+=(strlen(estimateString)+1);
	        serverCount++;
	    }
	    C_NetworkQualityEstimate(handle, serverCount, serverList);

	} else if (CompareInputCommand(rfd, "ReconnectionSurvey") == TRUE) {
	    ReconnectionQuestionnaire questionnaire;
	    RPC2_Integer ReturnCode;
	    char volname[64];
	    
	    questionnaire.RQVersionNumber = 0;
	    strcpy(volname, "u.mre");
            questionnaire.VolumeName = (RPC2_String)volname;
	    questionnaire.VID = (VolumeId) 2;
	    questionnaire.CMLcount = 3;
	    questionnaire.TimeOfDisconnection = 4;
	    questionnaire.TimeOfReconnection = 5;
	    questionnaire.TimeOfLastDemandHoardWalk = 6;
	    questionnaire.NumberOfReboots = 7;
	    questionnaire.NumberOfCacheHits = 8;
	    questionnaire.NumberOfCacheMisses = 9;
	    questionnaire.NumberOfUniqueCacheHits = 10;
	    questionnaire.NumberOfObjectsNotReferenced;

	    C_Reconnection(handle, &questionnaire, &ReturnCode);
	} else if (CompareInputCommand(rfd, "ReadDisconnectedCacheMissEvent") == TRUE) {
	    char pathname[128];
 	    ObjectInformation objInfo;
	    ProcessInformation processInfo;
	    RPC2_Unsigned TimeOfMiss;
	    CacheMissAdvice Advice;
	    RPC2_Integer RC;

	    if (InterestedInReadDisco == 1) {
 	        strcpy(pathname, "/coda/usr/mre/thesis/dissertation/user_interface.tex");
                objInfo.Pathname = (RPC2_String)pathname;
                processInfo.pid = 1;
	        TimeOfMiss = (RPC2_Integer)0;

	        C_ReadDisconnectedCacheMissEvent(handle, &objInfo, &processInfo, TimeOfMiss, &Advice, &RC);
	    } else {
	        Advice = FetchFromServers;
	    }
 	    printf("Read Miss Advice = %d\n", (int)Advice); fflush(stdout);
	} else if (CompareInputCommand(rfd, "WeakMissEvent") == TRUE) {
	    char pathname[128];
 	    ObjectInformation objInfo;
	    ProcessInformation processInfo;
	    RPC2_Unsigned TimeOfMiss;
	    RPC2_Integer Length;
	    RPC2_Integer EstimatedBandwidth;
	    CacheMissAdvice Advice;
	    RPC2_Integer RC;

	    strcpy(pathname, "/coda/usr/mre/thesis/dissertation/user_interface.tex");
            objInfo.Pathname = (RPC2_String)pathname;
            processInfo.pid = 1;
	    TimeOfMiss = (RPC2_Integer)0;
	    Length = (RPC2_Integer)1470000;
	    EstimatedBandwidth = (RPC2_Integer)42000;

	    C_WeaklyConnectedCacheMissEvent(handle, &objInfo, &processInfo, TimeOfMiss, 
					    Length, EstimatedBandwidth, 
					    (RPC2_String)"/usr/coda/venus.cache/V42", 
					    &Advice, &RC);
	    printf("Weak Miss Advice = %d\n", (int)Advice); fflush(stdout);
	} else if (CompareInputCommand(rfd, "DisconnectedCacheMissEvent") == TRUE) {
	    char pathname[128];
 	    ObjectInformation objInfo;
	    ProcessInformation processInfo;
	    RPC2_Unsigned TimeOfMiss;
	    RPC2_Unsigned TimeOfDisconnection;
	    RPC2_Integer RC;

	    strcpy(pathname, "/coda/usr/mre/thesis/dissertation/user_interface.tex");
            objInfo.Pathname = (RPC2_String)pathname;
            processInfo.pid = 1;
	    TimeOfMiss = (RPC2_Integer)0;
	    TimeOfDisconnection = (RPC2_Integer)3600;

	    C_DisconnectedCacheMissEvent(handle, &objInfo, &processInfo, 
					 TimeOfMiss, TimeOfDisconnection, &RC);

	} else if (CompareInputCommand(rfd, "HoardWalkBeginEvent") == TRUE) {
	    C_HoardWalkBegin(handle);
	} else if (CompareInputCommand(rfd, "HoardWalkPendingAdviceEvent") == TRUE) {
	    char input[128], output[128];
	    RPC2_Integer RC;
	    long rc;

	    strcpy(input, "/tmp/hoardlist.demo");
	    strcpy(output, "/tmp/hoardadvice.demo");
	    MkHoardFile(input);
	    rc = C_HoardWalkAdviceRequest(handle, (RPC2_String)input, (RPC2_String)output, &RC);
	    fprintf(stderr, "Returned from HoardWalkAdviceRequest: rc=%d and RC=%d\n",rc, (int)RC);
	    fflush(stderr);
	} else if (CompareInputCommand(rfd, "HoardWalkEndEvent") == TRUE) {
	    C_HoardWalkEnd(handle);
	} else if (CompareInputCommand(rfd, "HoardWalkPeriodicOn") == TRUE) {
	    C_HoardWalkPeriodicOn(handle);
	} else if (CompareInputCommand(rfd, "HoardWalkPeriodicOff") == TRUE) {
	    C_HoardWalkPeriodicOff(handle);
	} else if (CompareInputCommand(rfd, "ReintegrationEnabledEvent") == TRUE) {
	    C_ReintegrationEnabled(handle, (RPC2_String)"/coda/usr/mre");
	} else if (CompareInputCommand(rfd, "ReintegrationPendingEvent") == TRUE) {
	    C_ReintegrationPendingTokens(handle, (RPC2_String)"/coda/usr/mre");
	} else if (CompareInputCommand(rfd, "ReintegrationActiveEvent") == TRUE) {
	    C_ReintegrationActive(handle, (RPC2_String)"/coda/usr/mre");
	} else if (CompareInputCommand(rfd, "ReintegrationCompleteEvent") == TRUE) {
	    C_ReintegrationCompleted(handle, (RPC2_String)"/coda/usr/mre");
	} else if (CompareInputCommand(rfd, "RepairPendingEvent") == TRUE) {
	    ViceFid fid;
	    fid.Volume = 42; fid.Vnode = 84; fid.Unique = 126;

	    C_ObjectInConflict(handle, (RPC2_String)"/coda/usr/mre/src/coda-src/advice", &fid);
	} else if (CompareInputCommand(rfd, "RepairCompleteEvent") == TRUE) {
	    ViceFid fid;
	    fid.Volume = 42; fid.Vnode = 84; fid.Unique = 126;

	    C_ObjectConsistent(handle, (RPC2_String)"/coda/usr/mre/src/coda-src/advice", &fid);
	} else if (CompareInputCommand(rfd, "MakeAllHoardedTasksAvailable") == TRUE) {
	    TallyInfo tallyInfo[3];
	    tallyInfo[0].TaskPriority = 1000;
	    tallyInfo[0].AvailableBlocks = 4235;
	    tallyInfo[0].UnavailableBlocks = 0;
	    tallyInfo[0].IncompleteInformation = 0;
	    tallyInfo[1].TaskPriority = 800;
	    tallyInfo[1].AvailableBlocks = 2346;
	    tallyInfo[1].UnavailableBlocks = 30457;
	    tallyInfo[1].IncompleteInformation = 0;
	    tallyInfo[2].TaskPriority = 700;
	    tallyInfo[2].AvailableBlocks = 234;
	    tallyInfo[2].UnavailableBlocks = 0;
	    tallyInfo[2].IncompleteInformation = 1;

	    C_TaskAvailability(handle, 3, tallyInfo);
	} else if (CompareInputCommand(rfd, "MakeAllHoardedTasksUnavailable") == TRUE) {
	    C_TaskUnavailable(handle, (RPC2_Integer)1000, (RPC2_Integer)1050);
 /*
	} else if (CompareInputCommand(rfd, "") == TRUE) {
	    printf("Received  request\n"); 
	    fflush(stdout);
	    C_
	} else if (CompareInputCommand(rfd, "") == TRUE) {
	    printf("Received  request\n"); 
	    fflush(stdout);
	    C_
  */
	} else {
	    printf("Received unrecognized command: %s\n", rfd);
	    fflush(stdout);
	}

        rfd = ReadFromDemo();
    }
}

