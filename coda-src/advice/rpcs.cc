#ifndef _BLURB_
#define _BLURB_
/*

            Coda: an Experimental Distributed File System
                             Release 4.0

          Copyright (c) 1987-1996 Carnegie Mellon University
                         All Rights Reserved

Permission  to  use, copy, modify and distribute this software and its
documentation is hereby granted,  provided  that  both  the  copyright
notice  and  this  permission  notice  appear  in  all  copies  of the
software, derivative works or  modified  versions,  and  any  portions
thereof, and that both notices appear in supporting documentation, and
that credit is given to Carnegie Mellon University  in  all  documents
and publicity pertaining to direct or indirect use of this code or its
derivatives.

CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
ANY DERIVATIVE WORK.

Carnegie  Mellon  encourages  users  of  this  software  to return any
improvements or extensions that  they  make,  and  to  grant  Carnegie
Mellon the rights to redistribute these changes without encumbrance.
*/

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/advice/Attic/rpcs.cc,v 4.3 1997/12/30 18:10:55 braam Exp $";
#endif /*_BLURB_*/


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#ifdef __linux__
#include <netinet/in.h>
#endif __linux__

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <assert.h>
#ifdef __MACH__
#include <sys/dir.h>
#else	/* __linux__ || __BSD44__ */
#include <dirent.h>
#endif 
#include <errno.h>
#include <stdio.h>
#include <strings.h>

#include <lwp.h>
#include <rpc2.h>

#ifdef __MACH__
int setuid(uid_t);
int execl(char *, char *, ...);

#endif /* __MACH__ */

#ifdef __cplusplus
}
#endif __cplusplus

/* from util */
#include <util.h>
#include <proc.h>
#include <bstree.h>

/* from vicedep */
#include "admon.h"
#include "adsrv.h"

/* from venus */
#include "advice.h"

/* local */
#include "advice_srv.h"
#include "console_handler.h"
#include "counters.h"
#include "globals.h"
#include "helpers.h"
#include "miss.h"
//#include "rpcs.h"

#define DFT_USERTIMEOUT 30

/* RPC Variables */
extern RPC2_PortalIdent rpc2_LocalPortal;

/* Log Levels */
int RPCdebugging = 100;

/* Counts */
int NumASRStarted = 0;
int NumRecoSurveys = 0;
int NumDiscoMissSurveys = 0;

void CheckStack(char *);
void AwaitSynchronization(char *, char *);
int IsDuplicate(char *, char *, char *, char *);
char *GetCacheAdviceString(CacheMissAdvice);
int PresentRQ(char *);

void InitDiscoFile(char *, int, int, int, int, int, int, int, ViceFid, char *, char *);
void InitReconFile(char *, int, int, int, int, int, ReconnectionQuestionnaire *);

/*************************************************************
 ******************  Incoming RPC Handlers  ******************
 *************************************************************/



long EstablishedConnection()
{
    char msg[MAXPATHLEN];

    CheckStack("Pre:EstablishedConnection");
    LogMsg(EnterLeaveMsgs,LogLevel,LogFile,"EstablishedConnection with \"venus\"");
    IncrementCounter(&EstablishedConnectionCount,ARRIVED);

    /* Inform the CodaConsole */
    sprintf(msg, "ValidateIndicators\n");
    SendToConsole(msg);
    CheckStack("Post:EstablishedConnection");
}

long S_LostConnection(RPC2_Handle _cid)
{
    char msg[MAXPATHLEN];

    CheckStack("Pre:LostConnection");
    LogMsg(EnterLeaveMsgs,LogLevel,LogFile,"LostConnection to \"venus\"");
    IncrementCounter(&LostConnectionCount, ARRIVED);

    WeLostTheConnection = TRUE;
    (void) RPC2_Unbind(rpc2_LocalPortal.Value.InetPortNumber);

    /* Inform the CodaConsole */
    sprintf(msg, "InvalidateIndicators\n");
    SendToConsole(msg);

    CheckStack("Post:LostConnection");
    return(RPC2_SUCCESS);
}


long S_TokensAcquired(RPC2_Handle _cid, RPC2_Integer EndTimestamp) 
{
    char msg[MAXPATHLEN];

    CheckStack("Pre:TokensAcquired");
    LogMsg(EnterLeaveMsgs,LogLevel,LogFile,"E TokensAcquired:  newExpiry=%d", EndTimestamp);
    IncrementCounter(&TokensAcquiredCount, ARRIVED);

    /* Inform the CodaConsole */
    sprintf(msg, "TokensAcquiredEvent\n");
    SendToConsole(msg);

    // Log and print counters
    LogMsg(EnterLeaveMsgs,LogLevel,LogFile, "L TokensAcquired()");
    PrintCounters();

    CheckStack("Post:TokensAcquired");
    return(RPC2_SUCCESS);
}

long S_TokensExpired(RPC2_Handle _cid) 
{
    char msg[MAXPATHLEN];

    CheckStack("Pre:TokensExpired");
    LogMsg(EnterLeaveMsgs,LogLevel,LogFile,"E TokensExpired()");
    IncrementCounter(&TokensExpiredCount, ARRIVED);

    /* Inform the CodaConsole */
    sprintf(msg, "TokenExpiryEvent\n");
    SendToConsole(msg);

    // Log and print counters
    LogMsg(EnterLeaveMsgs,LogLevel,LogFile, "L TokensExpired()");
    PrintCounters();

    CheckStack("Post:TokensExpired");
    return(RPC2_SUCCESS);
}

long S_ActivityPendingTokens(RPC2_Handle _cid, ActivityID activity, RPC2_String object)
{
    char msg[MAXPATHLEN];

    CheckStack("Pre:ActivityPendingTokens");
    LogMsg(EnterLeaveMsgs,LogLevel,LogFile,"E ActivityPendingTokens(%d,%s)", activity, object);
    IncrementCounter(&TokensPendingCount, ARRIVED);

    /* Inform the CodaConsole */
    switch (activity) {
        case FileAccess:
            sprintf(msg, "ActivityPendingTokensEvent %s %s\n", "fetch", object);
	    break;
	case ObjectNeedsRepair:
            sprintf(msg, "ActivityPendingTokensEvent %s %s\n", "repair", object);
	    break;
	case ObjectNeedsReintegration:
            sprintf(msg, "ActivityPendingTokensEvent %s %s\n", "reintegration", object);
	    break;
        default:
	    LogMsg(0,LogLevel,LogFile, "ActivityPendingTokens:  Invalid activity = %d\n", activity);
	    return(RPC2_SUCCESS);
    }
    SendToConsole(msg);

    // Log and print counters
    LogMsg(EnterLeaveMsgs,LogLevel,LogFile, "L ActivityPendingTokens()");
    PrintCounters();

    CheckStack("Post:ActivityPendingTokens");
    return(RPC2_SUCCESS);
}


long S_SpaceInformation(RPC2_Handle _cid, RPC2_Integer PercentFilesFilledByHoardedData, 
		      RPC2_Integer PercentBlocksFilledByHoardedData, RPC2_Integer PercentRVMFull, 
		      Boolean RVMFragmented)
{
    char msg[MAXPATHLEN];

    CheckStack("Pre:SpaceInformation");
    LogMsg(EnterLeaveMsgs,LogLevel,LogFile,
           "E SpaceInformation(%%filesOccupied:%d, %%blocksOccupied=%d,\
	   %%RVMOccupied=%d, RVMFragmented=%d)", 
           PercentFilesFilledByHoardedData, PercentBlocksFilledByHoardedData, 
	   PercentRVMFull, RVMFragmented);
    IncrementCounter(&SpaceInformationCount, ARRIVED);

    /* Inform the CodaConsole */
    sprintf(msg, "UpdateSpaceStatistics %d %d %d\n",
                 PercentFilesFilledByHoardedData, PercentBlocksFilledByHoardedData, 
		 PercentRVMFull);
    SendToConsole(msg);

    // Log and print counters
    LogMsg(EnterLeaveMsgs,LogLevel,LogFile, "L SpaceInformation()");
    PrintCounters();

    CheckStack("Post:SpaceInformation");
    return(RPC2_SUCCESS);
}

long S_ServerAccessible(RPC2_Handle _cid, RPC2_String ServerName) 
{
    char msg[MAXPATHLEN];

    CheckStack("Pre:ServerAccessible");
    LogMsg(EnterLeaveMsgs,LogLevel,LogFile, "E ServerAccessible(%s)", ServerName);
    IncrementCounter(&NetworkServerAccessibleCount, ARRIVED);

    /* Inform the CodaConsole */
    sprintf(msg, "ServerAccessibleEvent %s\n", ServerName);
    SendToConsole(msg);

    // Log and print counters
    LogMsg(EnterLeaveMsgs,LogLevel,LogFile, "L ServerAccessible()");
    PrintCounters();

    CheckStack("Post:ServerAccessible");
    return(RPC2_SUCCESS);
}

long S_ServerConnectionStrong(RPC2_Handle _cid, RPC2_String ServerName) 
{
    char msg[MAXPATHLEN];

    CheckStack("Pre:ServerConnectionStrong");
    LogMsg(EnterLeaveMsgs,LogLevel,LogFile, "E ServerConnectionStrong(%s)", ServerName);
    IncrementCounter(&NetworkServerConnectionStrongCount, ARRIVED);

    /* Inform the CodaConsole */
    sprintf(msg, "ServerConnectionStrongEvent %s\n", ServerName);
    SendToConsole(msg);

    // Log and print counters
    LogMsg(EnterLeaveMsgs,LogLevel,LogFile, "L ServerConnectionStrong()");
    PrintCounters();

    CheckStack("Post:ServerConnectionStrong");
    return(RPC2_SUCCESS);
}

long S_ServerConnectionWeak(RPC2_Handle _cid, RPC2_String ServerName) 
{
    char msg[MAXPATHLEN];

    CheckStack("Pre:ServerConnectionWeak");
    LogMsg(EnterLeaveMsgs,LogLevel,LogFile, "E ServerConnectionWeak(%s)", ServerName);
    IncrementCounter(&NetworkServerConnectionWeakCount, ARRIVED);

    /* Inform the CodaConsole */
    sprintf(msg, "ServerConnectionWeakEvent %s\n", ServerName);
    SendToConsole(msg);

    // Log and print counters
    LogMsg(EnterLeaveMsgs,LogLevel,LogFile, "L ServerConnectionWeak()");
    PrintCounters();

    CheckStack("Post:ServerConnectionWeak");
    return(RPC2_SUCCESS);
}


long S_NetworkQualityEstimate(RPC2_Handle _cid, long numEstimates, QualityEstimate estimates[])
{
    char tmpString[64];
    char msg[MAXPATHLEN];
    int msgSize = 0;

    CheckStack("Pre:NetworkQualityEstimate");
    LogMsg(EnterLeaveMsgs,LogLevel,LogFile, "E NetworkQualityEstimate()");
    IncrementCounter(&NetworkQualityEstimateCount, ARRIVED);

    /* Inform the CodaConsole */
    msg[0] = '\0';
    strcat(msg, "ServerBandwidthEstimateEvent ");
    for (int i = 0; i < (int)numEstimates; i++) {
        sprintf(tmpString, "%s %d ", 
		(char *)estimates[i].ServerName, (int)estimates[i].BandwidthEstimate);
        strcat(msg, tmpString);
    }
    strcat(msg, "\n");
    assert(strlen(msg) <= MAXPATHLEN);
    SendToConsole(msg);

    // Log and print counters
    LogMsg(EnterLeaveMsgs,LogLevel,LogFile, "L NetworkQualityEstimate()");
    PrintCounters();

    CheckStack("Post:NetworkQualityEstimate");
    return(RPC2_SUCCESS);
}

long S_ServerInaccessible(RPC2_Handle _cid, RPC2_String ServerName) 
{
    char msg[MAXPATHLEN];

    CheckStack("Pre:ServerInaccessible");
    LogMsg(EnterLeaveMsgs,LogLevel,LogFile, "E ServerInaccessible(%s)", ServerName);
    IncrementCounter(&NetworkServerInaccessibleCount, ARRIVED);

    /* Inform the CodaConsole */
    sprintf(msg, "ServerInaccessibleEvent %s\n", ServerName);
    SendToConsole(msg);

    // Log and print counters
    LogMsg(EnterLeaveMsgs,LogLevel,LogFile, "L ServerInaccessible()");
    PrintCounters();

    CheckStack("Post:ServerInaccessible");
    return(RPC2_SUCCESS);
}

long S_VolumeTransitionEvent(RPC2_Handle _cid, RPC2_String VolumeName, RPC2_Integer vid, 
                      VolumeStateID NewState, VolumeStateID OldState)
{
    CheckStack("Pre:VolumeTransitionEvent");
    LogMsg(EnterLeaveMsgs,LogLevel,LogFile, "E VolumeTransitionEvent(%s %d %d %d)", 
           VolumeName, vid, NewState, OldState);
    IncrementCounter(&VolumeTransitionCount,ARRIVED);

    LogMsg(EnterLeaveMsgs,LogLevel,LogFile, "L VolumeTransitionEvent()");
    PrintCounters();
    CheckStack("Post:VolumeTransitionEvent");
    return(RPC2_SUCCESS);
}

void ReconnectionLogParams(ReconnectionQuestionnaire *questionnaire) {
    // Log params to this reconnection event
    LogMsg(RPCdebugging,LogLevel,LogFile,"\tVenusVersionNumber = %d.%d", 
	   VenusMajorVersionNumber, VenusMinorVersionNumber);
    LogMsg(RPCdebugging,LogLevel,LogFile,"\tRQVersionNumber = %d", 
	   (int)questionnaire->RQVersionNumber);
    LogMsg(RPCdebugging,LogLevel,LogFile,"\tVolume Name = %s <%x>", 
	   questionnaire->VolumeName, questionnaire->VID);
    LogMsg(RPCdebugging,LogLevel,LogFile,"\tCMLcount = %d", 
	   questionnaire->CMLcount);
    LogMsg(RPCdebugging,LogLevel,LogFile,
	   "\tTimes:  Disconnection=%s, Reconnection=%s, LastDemandHoardWalk=%s",
	   TimeString((long)questionnaire->TimeOfDisconnection),
	   TimeString((long)questionnaire->TimeOfReconnection),
	   "Unknown");
    LogMsg(RPCdebugging,LogLevel,LogFile,
	   "\tCounts: Reboots=%d, HITS=%d, MISSES=%d, UniqueHITS=%d, ObjectsNotReferenced=%d",
	   questionnaire->NumberOfReboots,
	   questionnaire->NumberOfCacheHits,
	   questionnaire->NumberOfCacheMisses,
	   questionnaire->NumberOfUniqueCacheHits,
	   questionnaire->NumberOfObjectsNotReferenced);
}

long S_Reconnection(RPC2_Handle _cid, ReconnectionQuestionnaire *questionnaire, RPC2_Integer *ReturnCode) 
{
    char tmpReconnectionFileName[MAXPATHLEN];
    char ReconnectionFileName[MAXPATHLEN];
    char msg[MAXPATHLEN];

    assert(questionnaire != NULL);

    CheckStack("Pre:Reconnection");
    LogMsg(EnterLeaveMsgs,LogLevel,LogFile, "E Reconnection(volume=%s)", questionnaire->VolumeName);
    ReconnectionLogParams(questionnaire);
    IncrementCounter(&ReconnectionCount,ARRIVED);

    LogMsg(EnterLeaveMsgs,LogLevel,LogFile, "L Reconnection()");
    PrintCounters();
    CheckStack("Post:Reconnection");

    /* After reconnection questionnaire is implemented in CodaConsole */
    *ReturnCode = ADMON_SUCCESS;

    // Decide whether or not to present the reconnection questionnaire
    if (!PresentRQ((char *)questionnaire->VolumeName)) {
	IncrementCounter(&ReconnectionCount, ARRIVED);
    	LogMsg(EnterLeaveMsgs,LogLevel,LogFile, "L Reconnection()");
	PrintCounters();
        CheckStack("Post:Reconnection");
	return(RPC2_SUCCESS);
    }


    // Determine the filename
    snprintf(tmpReconnectionFileName, MAXPATHLEN, 
	     "%s/#Reconnection.%d", tmpDir, ReconnectionCount.arrivedFromVenus);
    snprintf(ReconnectionFileName, MAXPATHLEN, 
	     "%s/Reconnection.%d", tmpDir, ReconnectionCount.arrivedFromVenus);

    InitReconFile(tmpReconnectionFileName, VenusMajorVersionNumber, VenusMinorVersionNumber, 
		  ADVICE_MONITOR_VERSION, ADSRV_VERSION, ADMON_VERSION, questionnaire);

    sprintf(msg, "ReconnectionEvent \"%s\" \"%s\" \"%s\" \"%s\"\n",
	    tmpReconnectionFileName,
	    GetDateFromLong((long)questionnaire->TimeOfDisconnection),
	    GetTimeFromLong((long)questionnaire->TimeOfDisconnection),
	    GetStringFromTimeDiff((long)questionnaire->TimeOfReconnection-
				  (long)questionnaire->TimeOfDisconnection));
    SendToConsole(msg);
    IncrementCounter(&ReconnectionCount, SENT);

    LogMsg(EnterLeaveMsgs,LogLevel,LogFile, "L Reconnection()");
    PrintCounters();
    CheckStack("Post:Reconnection");
    *ReturnCode = ADMON_SUCCESS;
    return(RPC2_SUCCESS);
}


long S_ReadDisconnectedCacheMissEvent(RPC2_Handle _cid, 
			       ObjectInformation *objInfo, ProcessInformation *processInfo, 
			       RPC2_Unsigned TimeOfMiss, CacheMissAdvice *Advice, 
			       RPC2_Integer *ReturnCode)
{
    static char lastPathname[MAXPATHLEN]; char thisPathname[MAXPATHLEN];
    static char lastProgName[MAXPATHLEN]; char thisProgName[MAXPATHLEN];
    static CacheMissAdvice lastAdvice;
    miss *m;
    char msg[MAXPATHLEN];
    int pid;

    CheckStack("Pre:ReadDisconnectedCacheMissEvent");

    assert(objInfo != NULL);
    assert(processInfo != NULL);
    pid = (int)processInfo->pid;

    LogMsg(EnterLeaveMsgs,LogLevel,LogFile,
	   "E ReadDisconnectedCacheMissEvent: %s %d", 
	   (char *)objInfo->Pathname, pid);
    IncrementCounter(&AdviceReadCacheMissCount, ARRIVED);

    *Advice = FetchFromServers;

    /* Filter out duplicates -- if it's the same as the last one... */
    strncpy(thisProgName, GetCommandName(pid), MAXPATHLEN);
    if (IsDuplicate((char*)objInfo->Pathname, thisProgName, lastPathname, lastProgName)) {
        IncrementCounter(&AdviceReadCacheMissCount, DUPLICATE);
	*Advice = lastAdvice;
	LogMsg(EnterLeaveMsgs,LogLevel,LogFile,"L ReadDisconnectedCacheMissEvent(): duplicate");
        CheckStack("Post:ReadDisconnectedCacheMissEvent");
	return(RPC2_SUCCESS);
    }
    strncpy(lastPathname, (char *)objInfo->Pathname, MAXPATHLEN);
    strncpy(lastProgName, thisProgName, MAXPATHLEN);

    LogMsg(EnterLeaveMsgs,LogLevel,LogFile,
	   "E ReadDisconnectedCacheMissEvent (non-duplicate): %s %s", 
	   lastPathname, lastProgName);

    m = new miss(lastPathname, lastProgName);

    sprintf(msg, "ReadDisconnectedCacheMissEvent %s %s\n", lastPathname, lastProgName);
    SendToConsole(msg);

    AwaitSynchronization("readmissSync", &readmissSync);

    *Advice = (CacheMissAdvice) readmissAnswer;
    LogMsg(RPCdebugging,LogLevel,LogFile, 
	   "ReadDisconnectedCacheMissEvent Advice= %s\n", 
	   GetCacheAdviceString(*Advice));
    lastAdvice = *Advice;

    LogMsg(EnterLeaveMsgs,LogLevel,LogFile, "L ReadDisconnectedCacheMissEvent(advice=%d)", *Advice);
    PrintCounters();
    CheckStack("Post:ReadDisconnectedCacheMissEvent");
    return(RPC2_SUCCESS);
}

long S_WeaklyConnectedCacheMissEvent(RPC2_Handle _cid, ObjectInformation *objInfo, 
			 ProcessInformation *processInfo, RPC2_Unsigned TimeOfMiss, 
			 RPC2_Integer Length, RPC2_Integer EstimatedBandwidth, 
			 RPC2_String Vfile, CacheMissAdvice *Advice, RPC2_Integer *ReturnCode)
{
    static char lastPathname[MAXPATHLEN]; char thisPathname[MAXPATHLEN];
    static char lastProgName[MAXPATHLEN]; char thisProgName[MAXPATHLEN];
    static CacheMissAdvice lastAdvice;
    char msg[MAXPATHLEN];
    miss *m;
    int pid;
    int expectedFetchTime;

    CheckStack("Pre:WeaklyConnectedCacheMiss");

    assert(objInfo != NULL);
    assert(processInfo != NULL);
    pid = (int)processInfo->pid;

    LogMsg(EnterLeaveMsgs,LogLevel,LogFile,
	   "E WeaklyConnectedCacheMissEvent: %s %d", 
	   (char*)objInfo->Pathname, pid);
    IncrementCounter(&AdviceWeakCacheMissCount, ARRIVED);

    /* Filter out duplicates -- if it's the same as the last one... */
    strncpy(thisProgName, GetCommandName(pid), MAXPATHLEN);
    if (IsDuplicate((char*)objInfo->Pathname, thisProgName, lastPathname, lastProgName)) {
        IncrementCounter(&AdviceWeakCacheMissCount, DUPLICATE);
	*Advice = lastAdvice;
	LogMsg(EnterLeaveMsgs,LogLevel,LogFile,"L WeaklyConnectedCacheMissEvent(): duplicate");
        CheckStack("Post:WeaklyConnectedCacheMissEvent");
	return(RPC2_SUCCESS);
    }
    strncpy(lastPathname, (char *)objInfo->Pathname, MAXPATHLEN);
    strncpy(lastProgName, thisProgName, MAXPATHLEN);

    expectedFetchTime = (int)(Length / EstimatedBandwidth);

    LogMsg(EnterLeaveMsgs,LogLevel,LogFile,
	   "E WeaklyConnectedCacheMissEvent (non-duplicate): %s %s %d", 
	   lastPathname, lastProgName, expectedFetchTime);

    m = new miss(lastPathname,lastProgName);

    sprintf(msg, "WeakMissEvent %s %s %d %d\n", 
	    lastPathname, lastProgName, expectedFetchTime, DFT_USERTIMEOUT);
    SendToConsole(msg);

    AwaitSynchronization("weakmissSync", &weakmissSync);

    *Advice = (CacheMissAdvice)weakmissAnswer;
    LogMsg(RPCdebugging,LogLevel,LogFile, 
	   "WeaklyConnectedCacheMiss Advice= %s\n", 
	   GetCacheAdviceString(*Advice));
    lastAdvice = *Advice;

    LogMsg(EnterLeaveMsgs,LogLevel,LogFile, "L WeaklyConnectedCacheMissEvent(advice = %d)", *Advice);
    PrintCounters();
    CheckStack("Post:WeaklyConnectedCacheMissEvent");
    return(RPC2_SUCCESS);
}


long S_DisconnectedCacheMissEvent(RPC2_Handle _cid, ObjectInformation *objInfo, ProcessInformation *processInfo,
		      RPC2_Unsigned TimeOfMiss, RPC2_Unsigned TimeOfDisconnection,
		      RPC2_Integer *ReturnCode)
{
    static char lastPathname[MAXPATHLEN]; char thisPathname[MAXPATHLEN];
    static char lastProgName[MAXPATHLEN]; char thisProgName[MAXPATHLEN];
    char tmpFileName[MAXPATHLEN];
    char FileName[MAXPATHLEN];
    miss *m;
    int pid;
    char msg[MAXPATHLEN];

    CheckStack("Pre:DisconnectedCacheMissEvent");

    assert(objInfo != NULL);
    assert(processInfo != NULL);
    pid = (int)(processInfo->pid);

    LogMsg(EnterLeaveMsgs,LogLevel,LogFile,
	   "E DisconnectedCacheMissEvent: %s %d", 
	   (char *)objInfo->Pathname, pid);
    IncrementCounter(&AdviceDiscoCacheMissCount, ARRIVED);

    /* Filter out duplicates -- if it's the same as the last one... */
    strncpy(thisProgName, GetCommandName(pid), MAXPATHLEN);
    if (IsDuplicate((char*)(objInfo->Pathname), thisProgName, lastPathname, lastProgName)) {
        IncrementCounter(&AdviceDiscoCacheMissCount, DUPLICATE);
        CheckStack("Post:DisconnectedCacheMissEvent");
	return(RPC2_SUCCESS);
    }
    strncpy(lastPathname, (char *)objInfo->Pathname, MAXPATHLEN);
    strncpy(lastProgName, thisProgName, MAXPATHLEN);

    LogMsg(EnterLeaveMsgs,LogLevel,LogFile,
	   "E DisconnectedCacheMissEvent (non-duplicate): %s %s", 
	   lastPathname, lastProgName);

    m = new miss(lastPathname, lastProgName);


    // Determine the filename
    snprintf(tmpFileName, MAXPATHLEN, "%s/#DisconnectedMiss.%d", tmpDir, 
	     AdviceDiscoCacheMissCount.arrivedFromVenus);
    snprintf(FileName, MAXPATHLEN, "%s/DisconnectedMiss.%d", tmpDir, 
	     AdviceDiscoCacheMissCount.arrivedFromVenus);

    InitDiscoFile(tmpFileName, VenusMajorVersionNumber, VenusMinorVersionNumber, 
		  ADVICE_MONITOR_VERSION, ADSRV_VERSION, ADMON_VERSION, 
		  (int)TimeOfDisconnection, (int)TimeOfMiss, objInfo->Fid, 
		  thisPathname, thisProgName);

    sprintf(msg, "DisconnectedCacheMissEvent %s %s %s\n", tmpFileName, lastPathname, lastProgName);
    SendToConsole(msg);
    IncrementCounter(&AdviceDiscoCacheMissCount, SENT);

    AwaitSynchronization("discomissSync", &discomissSync);
    IncrementCounter(&AdviceDiscoCacheMissCount, COMPLETED);

    // Move tmpFileName over to FileName (still in /tmp)
    rename(tmpFileName, FileName);    

    // Log and print counters
    LogMsg(EnterLeaveMsgs,LogLevel,LogFile, "L DisconnectedMiss()");
    PrintCounters();

    CheckStack("Post:DisconnectedCacheMissEvent");
    return(RPC2_SUCCESS);
}

long S_DataFetchEvent(RPC2_Handle _cide, RPC2_String Pathname, RPC2_Integer Size, RPC2_String Vfile)
{
    CheckStack("Pre:DataFetchEvent");
    LogMsg(EnterLeaveMsgs,LogLevel,LogFile, 
	   "E DataFetchEvent:  Path=%s; Size=%d; Vfile=%s\n", 
	   (char *)Pathname, (int)Size, (char *)Vfile);
    IncrementCounter(&DataFetchEventCount,ARRIVED);


    LogMsg(EnterLeaveMsgs,LogLevel,LogFile, 
	   "L DataFetchEvent()\n");
    PrintCounters();
    CheckStack("Post:DataFetchEvent");
    return(RPC2_SUCCESS);
}

long S_HoardWalkAdviceRequest(RPC2_Handle _cid, RPC2_String InputPathname, 
			    RPC2_String OutputPathname, RPC2_Integer *ReturnCode)
{
    char msg[MAXPATHLEN];

    CheckStack("Pre:HoardWalkAdvice");

    LogMsg(EnterLeaveMsgs,LogLevel,LogFile,"E HoardWalkAdvice(%s,%s)", 
	   (char *)InputPathname, (char *)OutputPathname);
    IncrementCounter(&AdviceHoardWalkCount, ARRIVED);

    LogMsg(RPCdebugging,LogLevel,LogFile, "HoardWalkAdvice");
    sprintf(msg, "HoardWalkPendingAdviceEvent %s %s\n", 
	    (char *)InputPathname, (char *)OutputPathname);
    SendToConsole(msg);
    AwaitSynchronization("hoardwalkSync", &hoardwalkSync);

    *ReturnCode = ADMON_SUCCESS;

    LogMsg(EnterLeaveMsgs,LogLevel,LogFile, "L HoardWalkAdvice()");
    PrintCounters();
    CheckStack("Post:HoardWalkAdviceRequest");
    return(RPC2_SUCCESS);
}

long S_HoardWalkBegin(RPC2_Handle _cid)
{
    char msg[MAXPATHLEN];

    CheckStack("Pre:HoardWalkBegin");

    LogMsg(EnterLeaveMsgs,LogLevel,LogFile,"E HoardWalkBegin()");
    IncrementCounter(&HoardWalkBeginCount, ARRIVED);

    sprintf(msg, "HoardWalkBeginEvent\n");
    SendToConsole(msg);

    LogMsg(EnterLeaveMsgs,LogLevel,LogFile, "L HoardWalkBegin()");
    PrintCounters();
    CheckStack("Post:HoardWalkBegin");
    return(RPC2_SUCCESS);
}

long S_HoardWalkStatus(RPC2_Handle _cid, RPC2_Integer percentDone)
{
    char msg[MAXPATHLEN];

    CheckStack("Pre:HoardWalkStatus");

    LogMsg(EnterLeaveMsgs,LogLevel,LogFile,"E HoardWalkStatus()");
    IncrementCounter(&HoardWalkStatusCount, ARRIVED);

    sprintf(msg, "HoardWalkProgressUpdate %d\n",(int)percentDone);
    SendToConsole(msg);

    LogMsg(EnterLeaveMsgs,LogLevel,LogFile, "L HoardWalkStatus()");
    PrintCounters();
    CheckStack("Post:HoardWalkStatus");
    return(RPC2_SUCCESS);
}


long S_HoardWalkEnd(RPC2_Handle _cid)
{
    char msg[MAXPATHLEN];

    CheckStack("Pre:HoardWalkEnd");

    LogMsg(EnterLeaveMsgs,LogLevel,LogFile,"E HoardWalkEnd()");
    IncrementCounter(&HoardWalkEndCount, ARRIVED);

    sprintf(msg, "HoardWalkEndEvent\n");
    SendToConsole(msg);

    LogMsg(EnterLeaveMsgs,LogLevel,LogFile, "L HoardWalkEnd()");
    PrintCounters();
    CheckStack("Post:HoardWalkEnd");
    return(RPC2_SUCCESS);
}

long S_HoardWalkPeriodicOn(RPC2_Handle _cid)
{
    char msg[MAXPATHLEN];

    CheckStack("Pre:HoardWalkPeriodicOn");

    LogMsg(EnterLeaveMsgs,LogLevel,LogFile,"E HoardWalkPeriodicOn()");
    IncrementCounter(&HoardWalkOnCount, ARRIVED);

    sprintf(msg, "HoardWalkOnEvent\n");
    SendToConsole(msg);

    LogMsg(EnterLeaveMsgs,LogLevel,LogFile, "L HoardWalkPeriodicOn()");
    PrintCounters();
    CheckStack("Post:HoardWalkPeriodicOn");
    return(RPC2_SUCCESS);
}

long S_HoardWalkPeriodicOff(RPC2_Handle _cid)
{
    char msg[MAXPATHLEN];

    CheckStack("Pre:HoardWalkPeriodicOff");

    LogMsg(EnterLeaveMsgs,LogLevel,LogFile,"E HoardWalkPeriodicOff()");
    IncrementCounter(&HoardWalkOffCount, ARRIVED);

    sprintf(msg, "HoardWalkOffEvent\n");
    SendToConsole(msg);

    LogMsg(EnterLeaveMsgs,LogLevel,LogFile, "L HoardWalkPeriodicOff()");
    PrintCounters();
    CheckStack("Post:HoardWalkPeriodicOff");
    return(RPC2_SUCCESS);
}

long S_InvokeASR(RPC2_Handle _cid, RPC2_String pathname, RPC2_Integer uid, 
               RPC2_Integer *ASRid, RPC2_Integer *ASRrc)
{
    struct stat statbuf;
    
    CheckStack("Pre:InvokeASR");

    LogMsg(EnterLeaveMsgs,LogLevel,LogFile,"E InvokeASR(%s,%d)", (char*)pathname, (int)uid);

    *ASRrc = ADMON_SUCCESS;

    NumASRStarted++;
    *ASRid = NumASRStarted;

    if (!::stat(JUMPSTARTASR, &statbuf)) {
        int rc = fork();
        if (rc == -1) {               /* fork failed, no child exists, we're the original process */
            LogMsg(0,LogLevel,LogFile,"InvokeASR: ERROR ==> during fork");
            LogMsg(0,LogLevel,EventFile, "tcl_fork: asr");
            *ASRrc = ADMON_FAIL;
            LogMsg(EnterLeaveMsgs,LogLevel,LogFile, "L InvokeASR()");
            PrintCounters();
            CheckStack("Post:InvokeASR");
            return(RPC2_SUCCESS);
        } else if (rc == 0) {                                          /* we're the child process */
            setuid((unsigned short)uid);
            char dnamebuf[MAXPATHLEN];
            char fnamebuf[MAXNAMLEN];
            path((char *)pathname, dnamebuf, fnamebuf);
            if (chdir(dnamebuf)) {
	        LogMsg(0,LogLevel,LogFile,
                       "InvokeASR: Child ERROR ==> ASR jump-starter couldn't change to directory %s", 
                       dnamebuf);
                LogMsg(0,LogLevel,EventFile, "jump-starter: cd");
	        *ASRrc = ADMON_FAIL;
	        return(ENOENT);
            }
            execl(JUMPSTARTASR, JUMPSTARTASR, pathname, 0);
        } else {	                                              /* we're the parent process */
            ASRinProgress = 1;
	    childPID = rc;
        }
    } else {
        LogMsg(0,LogLevel,LogFile, "InvokeASR: ERROR ==> JUMPSTARTASR (%s) not found", JUMPSTARTASR);
        LogMsg(0,LogLevel,EventFile, "jump-starter: not found");
        *ASRrc = ADMON_FAIL;
        LogMsg(EnterLeaveMsgs,LogLevel,LogFile, "L InvokeASR()");
        PrintCounters();
        CheckStack("Post:InvokeASR");
        return(RPC2_SUCCESS);
    }
    
    LogMsg(EnterLeaveMsgs, LogLevel, LogFile, "L InvokeASR()\n");
    PrintCounters();
    CheckStack("Post:InvokeASR");
    return(RPC2_SUCCESS);
}

long S_ObjectInConflict(RPC2_Handle _cid, RPC2_String Pathname, ViceFid *fid)
{
    char msg[MAXPATHLEN];

    CheckStack("Pre:ObjectInConflict");

    LogMsg(EnterLeaveMsgs,LogLevel,LogFile,"E ObjectInConflict(%s %x.%x.%x)",
	   Pathname, fid->Volume, fid->Vnode, fid->Unique);
    IncrementCounter(&RepairPendingCount, ARRIVED);

    sprintf(msg, "RepairPendingEvent %s %x.%x.%x\n", 
	    Pathname, fid->Volume, fid->Vnode, fid->Unique);
    SendToConsole(msg);

    LogMsg(EnterLeaveMsgs,LogLevel,LogFile, "L ObjectInConflict()");
    PrintCounters();
    CheckStack("Post:ObjectInConflict");
    return(RPC2_SUCCESS);
}

long S_ObjectConsistent(RPC2_Handle _cid, RPC2_String Pathname, ViceFid *fid)
{
    char msg[MAXPATHLEN];

    CheckStack("Pre:ObjectConsistent");

    LogMsg(EnterLeaveMsgs,LogLevel,LogFile,"E ObjectConsistent(%s %x.%x.%x)",
	   Pathname, fid->Volume, fid->Vnode, fid->Unique);
    IncrementCounter(&RepairCompletedCount, ARRIVED);

    sprintf(msg, "RepairCompleteEvent %s %x.%x.%x\n", 
	    Pathname, fid->Volume, fid->Vnode, fid->Unique);
    SendToConsole(msg);

    LogMsg(EnterLeaveMsgs,LogLevel,LogFile, "L ObjectConsistent()");
    PrintCounters();
    CheckStack("Post:ObjectConsistent");
    return(RPC2_SUCCESS);
}


long S_ReintegrationPendingTokens(RPC2_Handle _cid, RPC2_String volumeID)
{
    char msg[MAXPATHLEN];

    CheckStack("Pre:ReintegrationPendingTokens");

    LogMsg(EnterLeaveMsgs,LogLevel,LogFile,"E ReintegrationPendingTokens()");
    IncrementCounter(&ReintegrationPendingTokensCount, ARRIVED);

    sprintf(msg, "ReintegrationPendingEvent %s\n", volumeID);
    SendToConsole(msg);

    LogMsg(EnterLeaveMsgs,LogLevel,LogFile, "L ReintegrationPendingTokens()");
    PrintCounters();
    CheckStack("Post:ReintegrationPendingTokens");
    return(RPC2_SUCCESS);

}

long S_ReintegrationEnabled(RPC2_Handle _cid, RPC2_String volumeID)
{
    char msg[MAXPATHLEN];

    CheckStack("Pre:ReintegrationEnabled");
    LogMsg(EnterLeaveMsgs,LogLevel,LogFile, "E ReintegrationEnabled()");
    IncrementCounter(&ReintegrationEnabledCount, ARRIVED);

    sprintf(msg, "ReintegrationEnabledEvent %s\n", volumeID);
    SendToConsole(msg);

    LogMsg(EnterLeaveMsgs,LogLevel,LogFile, "L ReintegrationEnabled()");
    PrintCounters();
    CheckStack("Post:ReintegrationEnabled");
    return(RPC2_SUCCESS);
}

long S_ReintegrationActive(RPC2_Handle _cid, RPC2_String volumeID)
{
    char msg[MAXPATHLEN];

    CheckStack("Pre:ReintegrationActive");
    LogMsg(EnterLeaveMsgs,LogLevel,LogFile, "E ReintegrationActive()");
    IncrementCounter(&ReintegrationActiveCount, ARRIVED);

    sprintf(msg, "ReintegrationActiveEvent %s\n", volumeID);
    SendToConsole(msg);

    LogMsg(EnterLeaveMsgs,LogLevel,LogFile, "L ReintegrationActive()");
    PrintCounters();
    CheckStack("Post:ReintegrationActive");
    return(RPC2_SUCCESS);
}


long S_ReintegrationCompleted(RPC2_Handle _cid, RPC2_String volumeID)
{
    char msg[MAXPATHLEN];

    CheckStack("Pre:ReintegrationCompleted");

    LogMsg(EnterLeaveMsgs,LogLevel,LogFile,"E ReintegrationCompleted()");
    IncrementCounter(&ReintegrationCompletedCount, ARRIVED);

    sprintf(msg, "ReintegrationCompletedEvent %s\n", volumeID);
    SendToConsole(msg);

    LogMsg(EnterLeaveMsgs,LogLevel,LogFile, "L ReintegrationCompleted()");
    PrintCounters();
    CheckStack("Post:ReintegrationCompleted");
    return(RPC2_SUCCESS);
}


long S_TaskAvailability(RPC2_Handle _cid, RPC2_Integer count, TallyInfo *tallyInfo)
{
    char msg[MAXPATHLEN];

    CheckStack("Pre:TaskAvailability");

    LogMsg(EnterLeaveMsgs,LogLevel,LogFile,"E TaskAvailability()");
    IncrementCounter(&TaskAvailabilityCount, ARRIVED);

    for (int i = 0; i < (int)count; i++) {
        sprintf(msg, "TaskAvailabilityProc %d %d %d %d\n", 
		(int)tallyInfo[i].TaskPriority, (int)tallyInfo[i].AvailableBlocks,
		(int)tallyInfo[i].UnavailableBlocks, (int)tallyInfo[i].IncompleteInformation);
	LogMsg(0, LogLevel, LogFile, msg);
        SendToConsole(msg);
    }

    LogMsg(EnterLeaveMsgs,LogLevel,LogFile, "L TaskAvailability()");
    PrintCounters();
    CheckStack("Post:TaskAvailability");
fflush(LogFile);
    return(RPC2_SUCCESS);
}


long S_TaskUnavailable(RPC2_Handle _cid, RPC2_Integer TaskPriority, RPC2_Integer ElementSize)
{
    char msg[MAXPATHLEN];

    CheckStack("Pre:TaskUnavailable");

    LogMsg(EnterLeaveMsgs,LogLevel,LogFile,"E TaskUnavailable()");
    IncrementCounter(&TaskUnavailableCount, ARRIVED);

    sprintf(msg, "TaskUnavailable %d %d\n", (int)TaskPriority, (int)ElementSize);
    SendToConsole(msg);

    LogMsg(EnterLeaveMsgs,LogLevel,LogFile, "L TaskUnavailable()");
    PrintCounters();
    CheckStack("Post:TaskUnavailable");
    return(RPC2_SUCCESS);
}


long S_ProgramAccessLogAvailable(RPC2_Handle _cid, RPC2_String pathname) 
{
    char msg[MAXPATHLEN];

    CheckStack("Pre:ProgramAccessLogAvailable");

    LogMsg(EnterLeaveMsgs,LogLevel,LogFile,"E ProgramAccessLogAvailable()");
    IncrementCounter(&ProgramAccessLogAvailableCount, ARRIVED);

    strncpy(ProgramAccessLog, (char *)pathname, MAXPATHLEN);
    LWP_NoYieldSignal(&programlogSync);

    LogMsg(EnterLeaveMsgs,LogLevel,LogFile, "L ProgramAccessLogAvailable()");
    PrintCounters();
    CheckStack("Post:ProgramAccessLogAvailable()");
    return(RPC2_SUCCESS);
}

long S_ReplacementLogAvailable(RPC2_Handle _cid, RPC2_String pathname)
{
    char msg[MAXPATHLEN];

    CheckStack("Pre:ReplacementLogAvailable");

    LogMsg(EnterLeaveMsgs,LogLevel,LogFile,"E ReplacementLogAvailable()");
    IncrementCounter(&ReplacementLogAvailableCount, ARRIVED);

    strncpy(ReplacementLog, (char *)pathname, MAXPATHLEN);
    LWP_NoYieldSignal(&replacementlogSync);

    LogMsg(EnterLeaveMsgs,LogLevel,LogFile, "L ReplacementLogAvailable()");
    PrintCounters();
    CheckStack("Post:ReplacementLogAvailable()");
    return(RPC2_SUCCESS);
}

long S_TestConnection(RPC2_Handle _cid)
{
    CheckStack("Pre:TestConnection");
  LogMsg(EnterLeaveMsgs,LogLevel,LogFile, "TestConnection from venus");
    CheckStack("Post:TestConnection");
  return(RPC2_SUCCESS);
}



/*************************************************************************
 ***************************  Helper Routines  ***************************
 *************************************************************************/

void CheckStack(char *msg) 
{
   PROCESS processID;
   int max, used;
   char *rock;

   if (StackChecking) {
        assert(LWP_GetRock(42, &rock) == LWP_SUCCESS);
	assert(LWP_CurrentProcess(&processID) == LWP_SUCCESS);
	assert(LWP_StackUsed(processID, &max, &used) == LWP_SUCCESS);
	LogMsg(0,LogLevel,LogFile, 
	       "Thread=%c calls LWP_StackUsed in %s and finds that max=%d and used=%d\n", 
	       *rock, msg, max, used);
	if (*rock == 'W') {
   	    assert(used < (DFTSTACKSIZE*2*1024));
	} else {
	    assert(used < (DFTSTACKSIZE*1024));
	}
   }
}

void AwaitSynchronization(char *syncName, char *sync)
{
    char *rock;
    int pid = getpid();

    assert(LWP_GetRock(42, &rock) == LWP_SUCCESS);
    LogMsg(RPCdebugging,LogLevel,LogFile,
	   "Thread(rock=%c, pid=%d) waiting on %s\n",
	   *rock, pid, syncName); 

    assert(LWP_WaitProcess(sync) == LWP_SUCCESS);
    LogMsg(RPCdebugging,LogLevel,LogFile,
	   "Thread(rock=%c, pid=%d) woken from %s\n",
	   *rock, pid, syncName); 
}

int IsDuplicate(char *thisPathName, char *thisProgName, char *lastPathName, char *lastProgName)
{
    if ((strcmp(thisPathName,lastPathName) == 0) && 
	(strcmp(thisProgName,lastProgName) == 0)) {
      return(TRUE);
    } else {
      return(FALSE);
    }
}

char *GetCacheAdviceString(CacheMissAdvice advice)
{
    static char AdviceString[MAXPATHLEN];

    switch(advice) {
        case FetchFromServers:
	    strncpy(AdviceString, "fetch from servers", strlen("fetch from servers"));
            break;

        case CoerceToMiss:
	    strncpy(AdviceString, "coerce to miss", strlen("coerce to miss"));
            break;

        default:
            assert(1 == 0);
    }

    return(AdviceString);
}


int PresentRQ(char *volumeName) {
    char userVolume[smallStringLength];

    // Guess at the name of the user volume --> WEAK!
    snprintf(userVolume, smallStringLength, "u.%s", UserName);

    if (strcmp(volumeName, userVolume) == 0)
	return(1);
    else
	return(0);
}



/*************************************************************************
 ***************************  Data Collection  ***************************
 *************************************************************************/

void InitDiscoFile(char *FileName, int venusmajor, int venusminor,
		   int advice, int adsrv, int admon, 
		   int DisconnectionTime, int MissTime, 
		   ViceFid fid, char *path, char *prog)
{
    FILE *DiscoFile;
    struct stat buf;

    // Ensure it does not exist
    stat(FileName, &buf);
    assert(errno == ENOENT);

    // Create and initialize it
    DiscoFile = fopen(FileName, "w+");
    assert(DiscoFile != NULL);

    fprintf(DiscoFile, "Disconnected Cache Miss Questionnaire\n");
    fprintf(DiscoFile, "hostid: 0x%o\n", gethostid());
    fprintf(DiscoFile, "user: %d\n", uid);
    fprintf(DiscoFile, "VenusVersion: %d.%d\n", venusmajor, venusminor);
    fprintf(DiscoFile, "AdviceMonitorVersion: %d\n", advice);
    fprintf(DiscoFile, "ADSRVversion: %d\n", adsrv);
    fprintf(DiscoFile, "ADMONversion: %d\n", admon);
    fprintf(DiscoFile, "Qversion: %d\n", DMQ_VERSION);
    fprintf(DiscoFile, "TimeOfDisconnection: %d\n", DisconnectionTime);
    fprintf(DiscoFile, "TimeOfCacheMiss: %d\n", MissTime);
    fprintf(DiscoFile, "Fid: <%x.%x.%x>\n", fid.Volume, fid.Vnode, fid.Unique);
    fprintf(DiscoFile, "Path: %s\n", path);
    fprintf(DiscoFile, "RequestingProgram: %s\n", prog);
    fflush(DiscoFile);
    fclose(DiscoFile);
}

void InitReconFile(char *FileName, int venusmajor, int venusminor, 
		   int advice, int adsrv, int admon, 
		   ReconnectionQuestionnaire *questionnaire)
{
    FILE *ReconFile;
    struct stat buf;

    // Ensure it does not exist
    stat(FileName, &buf);
    assert(errno == ENOENT);

    // Create and initialize it
    ReconFile = fopen(FileName, "w+");
    assert(ReconFile != NULL);

    assert(questionnaire != NULL);
    fprintf(ReconFile, "Reconnection Questionnaire\n");
    fprintf(ReconFile, "hostid: %d\n", gethostid());
    fprintf(ReconFile, "user: %d\n", uid);
    fprintf(ReconFile, "VenusVersion: %d.%d\n", venusmajor, venusminor);
    fprintf(ReconFile, "AdviceMonitorVersion: %d\n", advice);
    fprintf(ReconFile, "ADSRVversion: %d\n", adsrv);
    fprintf(ReconFile, "ADMONversion: %d\n", admon);
    fprintf(ReconFile, "Qversion: %d\n", questionnaire->RQVersionNumber);
    fprintf(ReconFile, "VolumeName: %s\n", questionnaire->VolumeName);
    fprintf(ReconFile, "VID: 0x%x\n", questionnaire->VID);
    fprintf(ReconFile, "CMLCount: %d\n", questionnaire->CMLcount);
    fprintf(ReconFile, "TimeOfDisconnection: %d\n", questionnaire->TimeOfDisconnection);
    fprintf(ReconFile, "TimeOfReconnection: %d\n", questionnaire->TimeOfReconnection);
    fprintf(ReconFile, "TimeOfLastDemandHoardWalk: %d\n", questionnaire->TimeOfLastDemandHoardWalk);
    fprintf(ReconFile, "NumberOfReboots: %d\n", questionnaire->NumberOfReboots);
    fprintf(ReconFile, "NumberOfCacheHits: %d\n", questionnaire->NumberOfCacheHits);
    fprintf(ReconFile, "NumberOfCacheMisses: %d\n", questionnaire->NumberOfCacheMisses);
    fprintf(ReconFile, "NumberOfUniqueCacheHits: %d\n", questionnaire->NumberOfUniqueCacheHits);
//    fprintf(ReconFile, "NumberOfUniqueCacheMisses: %d\n", questionnaire->NumberOfUniqueCacheMisses);
    fprintf(ReconFile, "NumberOfObjectsNotReferenced: %d\n", questionnaire->NumberOfObjectsNotReferenced);

    fflush(ReconFile);
    fclose(ReconFile);
}



/***************************************************************************
 ***************************  StopLight Support  ***************************
 ***************************************************************************/

#ifdef 0

long VSEmulating(RPC2_Handle _cid, RPC2_String VolumeName, VolumeId vid)
{
    StoplightStates newState;
    volent *v;

    CheckStack("Pre:VSEmulating");
    LogMsg(EnterLeaveMsgs,LogLevel,LogFile, "E VSEmulating(%s,%x)", (char *)VolumeName, vid);

    v = new volent((char *)VolumeName, vid, VSemulating);
    newState = StoplightState();
    if ((newState != CurrentStoplightState) && (CurrentStoplightState != SLquit))
        CurrentStoplightState = StoplightStateChange(newState);
    LogMsg(EnterLeaveMsgs,LogLevel,LogFile, "L VSEmulating()");
    CheckStack("Post:VSEmulating");
    return(RPC2_SUCCESS);
}

long VSHoarding(RPC2_Handle _cid, RPC2_String VolumeName, VolumeId vid)
{
    StoplightStates newState;
    volent *v;

    CheckStack("Pre:VSHoarding");
    LogMsg(EnterLeaveMsgs,LogLevel,LogFile, "E VSHoarding(%s,%x)", (char *)VolumeName, vid);
    v = new volent((char *)VolumeName, vid, VShoarding);
    newState = StoplightState();
    if ((newState != CurrentStoplightState) && (CurrentStoplightState != SLquit))
        CurrentStoplightState = StoplightStateChange(newState);
    LogMsg(EnterLeaveMsgs,LogLevel,LogFile, "L VSHoarding()");
    CheckStack("Post:VSHoarding");
    return(RPC2_SUCCESS);
}

long VSLogging(RPC2_Handle _cid, RPC2_String VolumeName, VolumeId vid)
{
    StoplightStates newState;
    volent *v;

    CheckStack("Pre:VSLogging");
    LogMsg(EnterLeaveMsgs,LogLevel,LogFile, "E VSLogging(%s,%x)", (char *)VolumeName, vid);
    v = new volent((char *)VolumeName, vid, VSlogging);
    newState = StoplightState();
    if ((newState != CurrentStoplightState) && (CurrentStoplightState != SLquit))
        CurrentStoplightState = StoplightStateChange(newState);
    LogMsg(EnterLeaveMsgs,LogLevel,LogFile, "L VSLogging()");
    CheckStack("Post:VSLogging");
    return(RPC2_SUCCESS);
}

long VSResolving(RPC2_Handle _cid, RPC2_String VolumeName, VolumeId vid)
{
    StoplightStates newState;
    volent *v;

    CheckStack("Pre:VSResolving");
    LogMsg(EnterLeaveMsgs,LogLevel,LogFile, "E VSResolving(%s,%x)", (char *)VolumeName, vid);
    v = new volent((char *)VolumeName, vid, VSresolving);
    newState = StoplightState();
    if ((newState != CurrentStoplightState) && (CurrentStoplightState != SLquit))
        CurrentStoplightState = StoplightStateChange(newState);
    LogMsg(EnterLeaveMsgs,LogLevel,LogFile, "L VSResolving()");
    CheckStack("Post:VSResolving");
    return(RPC2_SUCCESS);
}

#endif
