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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/venus/advice.cc,v 4.2 97/12/16 16:08:20 braam Exp $";
#endif /*_BLURB_*/





#include "venus.private.h"

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <unistd.h>
#include <netinet/in.h>

#ifdef __cplusplus
}
#endif __cplusplus

/* interfaces */
#include <admon.h>
#include <adsrv.h>

/* from util */
#include <proc.h>

/* from venus */
#include "user.h"
#include "advice.h"
#include "adviceconn.h"

#define FALSE 0
#define TRUE 1

int ASRinProgress = 0;
int ASRresult = -1;

char *InterestToString(InterestID);
#define MAXEVENTLEN 64

adviceconn::adviceconn() {
  LOG(100, ("adviceconn::adviceconn()\n"));
  Lock_Init(&userLock);
  Reset();
  ResetCounters();
}

/*
 * we don't support assignments to objects of this type.
 * bomb in an obvious way if it inadvertently happens.
 */
adviceconn::adviceconn(adviceconn &a) {
  abort();
}

adviceconn::operator=(adviceconn& a) {
  abort();
  return(0);
}

adviceconn::~adviceconn() {
  LOG(100, ("adviceconn::~adviceconn()\n"));
}

/*******************************************************************************************
 *  RPC Requests to the user via Advice Monitor:
 *      RequestReadDisconnectedCacheMissAdvice
 *      RequestHoardWalkAdvice
 *      RequestDisconnectedQuestionnaire
 *      RequestReconnectionQuestionnaire
 *	RequestReintegratePending
 *      RequestASRInvokation
 *      RequestWeaklyConnectedCacheMissAdvice
 *******************************************************************************************/


void adviceconn::TokensAcquired(int expirationTime) {
  long rc;
  InterestID callType = TokensAcquiredID;

LOG(100, ("E(initial) adviceconn::TokensAcquired(%d)\n", expirationTime));

  IncrRequested(callType);

  if (!IsAdviceValid(callType, 0)) 
    return;
  if (!IsInterested(callType))
    return;

  LOG(100, ("E(official) adviceconn::TokensAcquired(%d)\n", expirationTime));
  ObtainWriteLock(&userLock);
  IncrRPCInitiated(callType); 
  rc = C_TokensAcquired(handle, (RPC2_Integer)expirationTime);
  ReleaseWriteLock(&userLock);

  CheckError(rc, callType);

  LOG(100, ("L adviceconn::TokensAcquired()\n"));
  return;
}

void adviceconn::TokensExpired() {
  long rc;
  InterestID callType = TokensExpiredID;
  if (!IsAdviceValid(callType, 0))
    return;
  if (!IsInterested(callType))
    return;

  LOG(100, ("E adviceconn::TokensExpired()\n"));
  ObtainWriteLock(&userLock);
  IncrRPCInitiated(callType); 
  rc = C_TokensExpired(handle);
  ReleaseWriteLock(&userLock);

  CheckError(rc, callType);

  LOG(100, ("L adviceconn::TokensExpired()\n"));
  return;
}

void adviceconn::ServerAccessible(char *name) {
  long rc;
  InterestID callType = ServerAccessibleID;
  if (!IsAdviceValid(callType, 0))
    return;
  if (!IsInterested(callType))
    return;

  LOG(10, ("E adviceconn::ServerAccessible()\n"));
  ObtainWriteLock(&userLock);
  IncrRPCInitiated(callType); 
  rc = C_ServerAccessible(handle, (RPC2_String) name);
  ReleaseWriteLock(&userLock);

  CheckError(rc, callType);

  LOG(10, ("L adviceconn::ServerAccessible()\n"));
  return;
}

void adviceconn::ServerInaccessible(char *name) {
  long rc;
  InterestID callType = ServerInaccessibleID;
  if (!IsAdviceValid(callType, 0))
    return;
  if (!IsInterested(callType))
    return;

  LOG(10, ("E adviceconn::ServerInaccessible()\n"));
  ObtainWriteLock(&userLock);
  IncrRPCInitiated(callType); 
  rc = C_ServerInaccessible(handle, (RPC2_String)name);
  ReleaseWriteLock(&userLock);

  CheckError(rc, callType);

  LOG(10, ("L adviceconn::ServerInaccessible()\n"));
  return;
}

void adviceconn::ServerConnectionWeak(char *name) {
  long rc;
  InterestID callType = ServerConnectionWeakID;
  if (!IsAdviceValid(callType, 0))
    return;
  if (!IsInterested(callType))
    return;

  LOG(10, ("E adviceconn::ServerConnectionWeak()\n"));
  ObtainWriteLock(&userLock);
  IncrRPCInitiated(callType); 
  rc = C_ServerConnectionWeak(handle, (RPC2_String)name);
  ReleaseWriteLock(&userLock);

  CheckError(rc, callType);

  LOG(10, ("L adviceconn::ServerConnectionWeak()\n"));
  return;
}

void adviceconn::ServerConnectionStrong(char *name) {
  long rc;
  InterestID callType = ServerConnectionStrongID;
  if (!IsAdviceValid(callType, 0))
    return;
  if (!IsInterested(callType))
    return;

  LOG(10, ("E adviceconn::ServerConnectionStrong()\n"));
  ObtainWriteLock(&userLock);
  IncrRPCInitiated(callType); 
  rc = C_ServerConnectionStrong(handle, (RPC2_String)name);
  ReleaseWriteLock(&userLock);

  CheckError(rc, callType);

  LOG(10, ("L adviceconn::ServerConnectionStrong()\n"));
  return;
}

void adviceconn::ServerBandwidthEstimate(char *name, long bandwidth) {
  long rc;
  QualityEstimate serverList[1];

  InterestID callType = NetworkQualityEstimateID;
  if (!IsAdviceValid(callType, 0))
    return;
  if (!IsInterested(callType))
    return;

  serverList[0].ServerName = (RPC2_String)name;
  serverList[0].BandwidthEstimate = (RPC2_Integer)bandwidth;
  serverList[0].Intermittent = False;

  LOG(10, ("E adviceconn::ServerBandwidthEstimate()\n"));
  ObtainWriteLock(&userLock);
  IncrRPCInitiated(callType); 
  rc = C_NetworkQualityEstimate(handle, 1, serverList);
  ReleaseWriteLock(&userLock);

  CheckError(rc, callType);

  LOG(10, ("L adviceconn::ServerBandwidthEstimate()\n"));
  return;
}

void adviceconn::HoardWalkBegin() {
  long rc;
  InterestID callType = HoardWalkBeginID;
  if (!IsAdviceValid(callType, 0))
    return;
  if (!IsInterested(callType))
    return;

  LOG(100, ("E adviceconn::HoardWalkBegin()\n"));
  ObtainWriteLock(&userLock);
  IncrRPCInitiated(callType); 
  rc = C_HoardWalkBegin(handle);
  ReleaseWriteLock(&userLock);

  CheckError(rc, callType);

  LOG(100, ("L adviceconn::HoardWalkBegin()\n"));
  return;
}

void adviceconn::HoardWalkStatus(int percentDone) {
  long rc;
  InterestID callType = HoardWalkStatusID;
  if (!IsAdviceValid(callType, 0))
    return;
  if (!IsInterested(callType))
    return;

  LOG(100, ("E adviceconn::HoardWalkStatus(%d)\n", percentDone));
  ObtainWriteLock(&userLock);
  IncrRPCInitiated(callType); 
  rc = C_HoardWalkStatus(handle, (RPC2_Integer)percentDone);
  ReleaseWriteLock(&userLock);

  CheckError(rc, callType);

  LOG(100, ("L adviceconn::HoardWalkStatus()\n"));
  return;
}

void adviceconn::HoardWalkEnd() {
  long rc;
  InterestID callType = HoardWalkEndID;
  if (!IsAdviceValid(callType, 0))
    return;
  if (!IsInterested(callType))
    return;

  LOG(100, ("E adviceconn::HoardWalkEnd()\n"));
  ObtainWriteLock(&userLock);
  IncrRPCInitiated(callType); 
  rc = C_HoardWalkEnd(handle);
  ReleaseWriteLock(&userLock);

  CheckError(rc, callType);

  LOG(100, ("L adviceconn::HoardWalkEnd()\n"));
  return;
}

void adviceconn::HoardWalkPeriodicOn() {
  long rc;
  InterestID callType = HoardWalkPeriodicOnID;
  if (!IsAdviceValid(callType, 0))
    return;
  if (!IsInterested(callType))
    return;

  LOG(100, ("E adviceconn::HoardWalkPeriodicOn()\n"));
  ObtainWriteLock(&userLock);
  IncrRPCInitiated(callType); 
  rc = C_HoardWalkPeriodicOn(handle);
  ReleaseWriteLock(&userLock);

  CheckError(rc, callType);

  LOG(100, ("L adviceconn::HoardWalkPeriodicOn()\n"));
  return;
}

void adviceconn::HoardWalkPeriodicOff() {
  long rc;
  InterestID callType = HoardWalkPeriodicOffID;
  if (!IsAdviceValid(callType, 0))
    return;
  if (!IsInterested(callType))
    return;

  LOG(100, ("E adviceconn::HoardWalkPeriodicOff()\n"));
  ObtainWriteLock(&userLock);
  IncrRPCInitiated(callType); 
  rc = C_HoardWalkPeriodicOff(handle);
  ReleaseWriteLock(&userLock);

  CheckError(rc, callType);

  LOG(100, ("L adviceconn::HoardWalkPeriodicOff()\n"));
  return;
}




CacheMissAdvice adviceconn::RequestReadDisconnectedCacheMissAdvice(ViceFid *fid, char *pathname, int pid) {
  ObjectInformation objInfo;
  ProcessInformation processInfo;
  RPC2_Unsigned TimeOfMiss;
  CacheMissAdvice advice;
  RPC2_Integer RC;
  long rc;

  InterestID callType = ReadDisconnectedCacheMissEventID;
  if (!IsAdviceValid(callType, 0))
    return(FetchFromServers);
  if (!IsInterested(callType))
    return(FetchFromServers);

  /* Initialize the arguments */
  objInfo.Pathname = (RPC2_String)pathname;
  objInfo.Fid = *fid;
  processInfo.pid = pid;
  TimeOfMiss = (RPC2_Unsigned) Vtime();

  LOG(100, ("E adviceconn::RequestReadDisconnectedCacheMissAdvice()\n"));
  ObtainWriteLock(&userLock);
  LOG(100, ("Requesting read disconnected cache miss advice on %s from handle = %d\n", 
	    pathname, handle));
  IncrRPCInitiated(callType); 
  rc = C_ReadDisconnectedCacheMissEvent(handle, &objInfo, &processInfo, TimeOfMiss, &advice, &RC) ;
  ReleaseWriteLock(&userLock);

  CheckError(rc, callType);
  if (rc != RPC2_SUCCESS) 
    advice = FetchFromServers;

  LOG(100, ("Advice was to '%s' on %s\n", CacheMissAdviceToString(advice), pathname));

  if (advice < 0) {
    LOG(0, ("Read Disconnected Cache Miss Advice failed with %d.  Fetching anyway.\n", (int)advice));
    advice = FetchFromServers;
  }
  assert(advice <= MaxCacheMissAdvice);
  LOG(100, ("L adviceconn::RequestReadDisconnectedCacheMissAdvice()\n"));
  return(advice);
}

void adviceconn::RequestHoardWalkAdvice(char *input, char *output) {
  RPC2_Integer ReturnCode;
  long rc;

  InterestID callType = HoardWalkAdviceRequestID;
  if (!IsInterested(callType))
    return;

  LOG(100, ("E adviceconn::RequestHoardWalkAdvice()\n"));
  ObtainWriteLock(&userLock);
  LOG(100, ("Requesting hoard walk advice on %s\n", input));
  IncrRPCInitiated(callType); 
  rc = C_HoardWalkAdviceRequest(handle, (RPC2_String)input, (RPC2_String)output, &ReturnCode);
  ReleaseWriteLock(&userLock);

  CheckError(rc, callType);

  LOG(100, ("L adviceconn::RequestHoardWalkAdvice(ReturnCode=%d)\n", (int)ReturnCode));
  return;
}


void adviceconn::RequestDisconnectedQuestionnaire(ViceFid *fid, char *pathname, int pid, long DiscoTime) {
  ObjectInformation objInfo;
  ProcessInformation processInfo;
  RPC2_Unsigned TimeOfMiss;
  RPC2_Unsigned TimeOfDisconnection;
  RPC2_Integer RC;
  long rc;

  InterestID callType = DisconnectedCacheMissEventID;
  if (!IsAdviceValid(callType, 0))
    return;
  if (!IsInterested(callType))
    return;

  objInfo.Pathname = (RPC2_String)pathname;
  objInfo.Fid = *fid;
  processInfo.pid = pid;
  TimeOfDisconnection = (RPC2_Unsigned) DiscoTime;
  TimeOfMiss = (RPC2_Unsigned)Vtime();


  LOG(100, ("E adviceconn::RequestDisconnectedQuestionnaire()\n"));
  ObtainWriteLock(&userLock);
  IncrRPCInitiated(callType); 
  rc = C_DisconnectedCacheMissEvent(handle, &objInfo, &processInfo, TimeOfMiss, TimeOfDisconnection, &RC);
  ReleaseWriteLock(&userLock);

  CheckError(rc, callType);

  LOG(100, ("L adviceconn::RequestDisconnectedQuestionnaire()\n"));
  return;
}

#ifdef 0
void adviceconn::NotifyHoarding(char *volname, VolumeId vid) {
  long rc;

  InterestID callType = VolumeTransitionEventID;
  if (!IsAdviceValid(callType, 0))
    return;

  LOG(100, ("E adviceconn::NotifyHoarding(volname=%s, vid=%x)\n", volname, vid));

  ObtainWriteLock(&userLock);
  rc = VSHoarding(handle, (RPC2_String)volname, vid);
  ReleaseWriteLock(&userLock);

  CheckError(rc, callType);

  LOG(100, ("L adviceconn::NotifyHoarding()\n"));
  return;
}

void adviceconn::NotifyEmulating(char *volname, VolumeId vid) {
  long rc;

  InterestID callType = VolumeTransitionEventID;
  if (!IsAdviceValid(callType, 0))
    return;

  LOG(100, ("E adviceconn::NotifyEmulating(volname=%s, vid=%x)\n", volname, vid));

  ObtainWriteLock(&userLock);
  rc = VSEmulating(handle, (RPC2_String)volname, vid);
  ReleaseWriteLock(&userLock);

  CheckError(rc, callType);

  LOG(100, ("L adviceconn::NotifyEmulating()\n"));
  return;
}

void adviceconn::NotifyLogging(char *volname, VolumeId vid) {
  long rc;

  InterestID callType = VolumeTransitionEventID;
  if (!IsAdviceValid(callType, 0))
    return;

  LOG(100, ("E adviceconn::NotifyLogging(volname=%s, vid=%x)\n", volname, vid));

  ObtainWriteLock(&userLock);
  rc = VSLogging(handle, (RPC2_String)volname, vid);
  ReleaseWriteLock(&userLock);

  CheckError(rc, callType);

  LOG(100, ("L adviceconn::NotifyLogging()\n"));
  return;
}

void adviceconn::NotifyResolving(char *volname, VolumeId vid) {
  long rc;

  InterestID callType = VolumeTransitionEventID;
  if (!IsAdviceValid(callType, 0))
    return;

  LOG(100, ("E adviceconn::NotifyResolving(volname=%s, vid=%x)\n", volname, vid));

  ObtainWriteLock(&userLock);
  rc = VSResolving(handle, (RPC2_String)volname, vid);
  ReleaseWriteLock(&userLock);

  CheckError(rc, callType);

  LOG(100, ("L adviceconn::NotifyResolving()\n"));
  return;
}
#endif

void adviceconn::RequestReconnectionQuestionnaire(char *volname, VolumeId vid, int CMLcount, long DiscoTime, long WalkTime, int NumberReboots, int cacheHit, int cacheMiss, int unique_hits, int unique_nonrefs) {
  ReconnectionQuestionnaire questionnaire;
  RPC2_Integer Qrc;
  long rc;

  InterestID callType = ReconnectionID;
  if (!IsAdviceValid(callType, 0))
    return;
  if (!IsInterested(callType))
    return;

  LOG(100, ("E adviceconn::RequestReconnectionQuestionnaire(volname=%s)\n", volname));

  questionnaire.RQVersionNumber = RQ_VERSION;
  questionnaire.VolumeName = (RPC2_String) volname;
  questionnaire.VID = vid;
  questionnaire.CMLcount = (RPC2_Unsigned) CMLcount;
  questionnaire.TimeOfDisconnection = (RPC2_Unsigned) DiscoTime;
  questionnaire.TimeOfReconnection = (RPC2_Unsigned) Vtime();
  questionnaire.TimeOfLastDemandHoardWalk = (RPC2_Unsigned) WalkTime;
  questionnaire.NumberOfReboots = (RPC2_Unsigned) NumberReboots;
  questionnaire.NumberOfCacheHits = (RPC2_Unsigned) cacheHit;
  questionnaire.NumberOfCacheMisses = (RPC2_Unsigned) cacheMiss;
  questionnaire.NumberOfUniqueCacheHits = (RPC2_Unsigned) unique_hits;
  questionnaire.NumberOfObjectsNotReferenced = (RPC2_Unsigned) unique_nonrefs;

  ObtainWriteLock(&userLock);
  IncrRPCInitiated(callType); 
  rc = C_Reconnection(handle, &questionnaire, &Qrc);
  ReleaseWriteLock(&userLock);

  CheckError(rc, callType);

  LOG(100, ("L adviceconn::RequestReconnectionQuestionnaire()\n"));
  return;
}


void adviceconn::NotifyReintegrationPending(char *volname) {
  long rc;
  Boolean boo;
  InterestID callType = ReintegrationPendingTokensID;

  if (!IsAdviceValid(callType, 0))
    return;
  if (!IsInterested(callType))
    return;

  LOG(100, ("E adviceconn::NotifyReintegrationPendingTokens()\n"));

  ObtainWriteLock(&userLock);
  IncrRPCInitiated(callType); 
  rc = C_ReintegrationPendingTokens(handle, (RPC2_String)volname);
  ReleaseWriteLock(&userLock);

  CheckError(rc, callType);

  LOG(100, ("L adviceconn::NotifyReintegrationPendingTokens()\n"));
  return;
}


void adviceconn::NotifyReintegrationEnabled(char *volname) {
  long rc;
  Boolean boo;
  InterestID callType = ReintegrationEnabledID;

  if (!IsAdviceValid(callType, 0))
    return;
  if (!IsInterested(callType))
    return;

  LOG(100, ("E adviceconn::NotifyReintegrationEnabled()\n"));

  ObtainWriteLock(&userLock);
  IncrRPCInitiated(callType); 
  rc = C_ReintegrationEnabled(handle, (RPC2_String)volname);
  ReleaseWriteLock(&userLock);

  CheckError(rc, callType);

  LOG(100, ("L adviceconn::NotifyReintegrationEnabled()\n"));
  return;
}


void adviceconn::NotifyReintegrationActive(char *volname) {
  long rc;
  Boolean boo;
  InterestID callType = ReintegrationActiveID;

  if (!IsAdviceValid(callType, 0))
    return;
  if (!IsInterested(callType))
    return;

  LOG(100, ("E adviceconn::NotifyReintegrationActive()\n"));

  ObtainWriteLock(&userLock);
  IncrRPCInitiated(callType); 
  rc = C_ReintegrationActive(handle, (RPC2_String)volname);
  ReleaseWriteLock(&userLock);

  CheckError(rc, callType);

  LOG(100, ("L adviceconn::NotifyReintegrationActive()\n"));
  return;
}


void adviceconn::NotifyReintegrationCompleted(char *volname) {
  long rc;
  Boolean boo;
  InterestID callType = ReintegrationCompletedID;

  if (!IsAdviceValid(callType, 0))
    return;
  if (!IsInterested(callType))
    return;

  LOG(100, ("E adviceconn::NotifyReintegrationCompleted()\n"));

  ObtainWriteLock(&userLock);
  IncrRPCInitiated(callType); 
  rc = C_ReintegrationCompleted(handle, (RPC2_String)volname);
  ReleaseWriteLock(&userLock);

  CheckError(rc, callType);

  LOG(100, ("L adviceconn::NotifyReintegrationCompleted()\n"));
  return;
}


void adviceconn::NotifyObjectInConflict(char *pathname, ViceFid *fid) {
  long rc;
  Boolean boo;
  InterestID callType = ObjectInConflictID;

  if (!IsAdviceValid(callType, 0))
    return;
  if (!IsInterested(callType))
    return;

  LOG(100, ("E adviceconn::NotifyObjectInConflict(%s, %x.%x.%x)\n",
	    pathname,fid->Volume,fid->Vnode,fid->Unique));

  ObtainWriteLock(&userLock);
  IncrRPCInitiated(callType); 
  rc = C_ObjectInConflict(handle, (RPC2_String)pathname, fid);
  ReleaseWriteLock(&userLock);

  CheckError(rc, callType);

  LOG(100, ("L adviceconn::NotifyObjectInConflict()\n"));
  return;
}

void adviceconn::NotifyObjectConsistent(char *pathname, ViceFid *fid) {
  long rc;
  Boolean boo;
  InterestID callType = ObjectConsistentID;

  if (!IsAdviceValid(callType, 0))
    return;
  if (!IsInterested(callType))
    return;

  LOG(100, ("E adviceconn::NotifyObjectConsistent(%s, %x.%x.%x)\n",
	    pathname,fid->Volume,fid->Vnode,fid->Unique));

  ObtainWriteLock(&userLock);
  IncrRPCInitiated(callType); 
  rc = C_ObjectConsistent(handle, (RPC2_String)pathname, fid);
  ReleaseWriteLock(&userLock);

  CheckError(rc, callType);

  LOG(100, ("L adviceconn::NotifyObjectConsistent()\n"));
  return;
}

void adviceconn::NotifyTaskAvailability(int i, TallyInfo *tallyInfo) {
  long rc;
  InterestID callType = TaskAvailabilityID;

  if (!IsAdviceValid(callType, 0))
    return;
  if (!IsInterested(callType))
    return;

  LOG(100, ("E adviceconn::NotifyTaskAvailability()\n"));

  ObtainWriteLock(&userLock);
  IncrRPCInitiated(callType); 
  rc = C_TaskAvailability(handle, (RPC2_Integer)i, tallyInfo);
  ReleaseWriteLock(&userLock);

  CheckError(rc, callType);

  LOG(100, ("L adviceconn::NotifyTaskAvailability()\n"));
  return;
}

void adviceconn::NotifyTaskUnavailable(int priority, int size) {
  long rc;
  InterestID callType = TaskUnavailableID;

  if (!IsAdviceValid(callType, 0))
    return;
  if (!IsInterested(callType))
    return;

  LOG(0, ("E adviceconn::NotifyTaskUnavailable(%d, %d)\n", priority, size));

  ObtainWriteLock(&userLock);
  IncrRPCInitiated(callType); 
  LOG(0, ("Calling C_TaskUnavailable()\n"));
  fflush(logFile);
  rc = C_TaskUnavailable(handle, (RPC2_Integer)priority, (RPC2_Integer)size);
  LOG(0, ("Returned from C_TaskUnavailable()\n"));
  fflush(logFile);

  ReleaseWriteLock(&userLock);

  CheckError(rc, callType);

  LOG(0, ("L adviceconn::NotifyTaskUnavailable()\n"));
  return;
}

void adviceconn::NotifyProgramAccessLogAvailable(char *pathname) {
  long rc;
  InterestID callType = ProgramAccessLogsID;

  if (!IsAdviceValid(callType, 0))
    return;

  LOG(0, ("E adviceconn::NotifyProgramAccessLogAvailable(%s)\n", pathname));

  ObtainWriteLock(&userLock);
  IncrRPCInitiated(callType); 
  LOG(0, ("Calling C_ProgramAccessLogAvailable()\n"));
  fflush(logFile);
  rc = C_ProgramAccessLogAvailable(handle, (RPC2_String)pathname);
  LOG(0, ("Returned from C_ProgramAccessLogAvailable()\n"));
  fflush(logFile);

  ReleaseWriteLock(&userLock);

  CheckError(rc, callType);

  LOG(0, ("L adviceconn::NotifyProgramAccessLogAvailable()\n"));
  return;
}

void adviceconn::NotifyReplacementLogAvailable(char *pathname) {
  long rc;
  InterestID callType = ReplacementLogsID;

  if (!IsAdviceValid(callType, 0))
    return;

  LOG(0, ("E adviceconn::NotifyReplacementLogAvailable(%s)\n", pathname));

  ObtainWriteLock(&userLock);
  IncrRPCInitiated(callType); 
  LOG(0, ("Calling C_ReplacementLogAvailable()\n"));
  fflush(logFile);
  rc = C_ReplacementLogAvailable(handle, (RPC2_String)pathname);
  LOG(0, ("Returned from C_ReplacementLogAvailable()\n"));
  fflush(logFile);

  ReleaseWriteLock(&userLock);

  CheckError(rc, callType);

  LOG(0, ("L adviceconn::NotifyReplacementLogAvailable()\n"));
  return;
}

int adviceconn::RequestASRInvokation(char *pathname, vuid_t vuid) {
  long rc;
  RPC2_Integer ASRid;
  RPC2_Integer ASRrc;

  InterestID callType = InvokeASRID;
  if (!IsInterested(callType))
    return(0);

  LOG(100, ("E adviceconn::RequestASRInvokation(%s, %d)\n", pathname, vuid));
  if (ASRinProgress) {
    LOG(0, ("adviceconn::RASRI() - One ASR already in progress \n", 
	    pathname, vuid));
    return(CAEASRINPROGRESS);
  }
  ASRinProgress = 1;	// XXX gross way of locking ASR Invocation - Puneet
  ObtainWriteLock(&userLock);
  LOG(100, ("\tRequesting ASR Invokation\n"));
  IncrRPCInitiated(callType); 
  rc = C_InvokeASR(handle, (RPC2_String)pathname, vuid, &ASRid, &ASRrc);
  ASRinProgress = (int)ASRid;
  ReleaseWriteLock(&userLock);
  
  CheckError(rc, callType);
  if (rc != RPC2_SUCCESS) 
    ASRinProgress = 0;	// XXX gross method for unlocking ASR invocation
  if (ASRrc != ADMON_SUCCESS) 
    ASRinProgress = 0;  // XXX gross method for unlocking ASR invocation

  LOG(100, ("Result of ASR was %d", ASRrc));
  return(rc);
}

CacheMissAdvice adviceconn::RequestWeaklyConnectedCacheMissAdvice(ViceFid *fid, char *pathname, int pid, int length, int estimatedBandwidth, char *Vfilename) {
  ObjectInformation objInfo;
  ProcessInformation processInfo;
  RPC2_Unsigned TimeOfMiss;
  CacheMissAdvice advice;
  RPC2_Integer RC;
  long rc;
  InterestID callType = WeaklyConnectedCacheMissEventID;

  if (!IsAdviceValid(callType, 0)) {
    LOG(100, ("RequestWeaklyConnectedCacheMissAdvice: Advice not valid for this user...\n"));
    return(FetchFromServers);
  }
  if (!IsInterested(callType)) {
    LOG(100, ("RequestWeaklyConnectedCacheMissAdvice: This type of advice not requested for this user...\n"));
    return(FetchFromServers);
  }

  objInfo.Fid = *fid;
  objInfo.Pathname = (RPC2_String)pathname;
  processInfo.pid = pid;
  TimeOfMiss = (RPC2_Unsigned)Vtime();

  LOG(100, ("E adviceconn::RequestWeaklyConnectedCacheMiss(%s)\n", pathname));
  ObtainWriteLock(&userLock);
  IncrRPCInitiated(callType); 
  rc = C_WeaklyConnectedCacheMissEvent(handle, &objInfo, &processInfo, TimeOfMiss, (RPC2_Integer)length, (RPC2_Integer)estimatedBandwidth, (RPC2_String)Vfilename, &advice, &RC);
  ReleaseWriteLock(&userLock);

  CheckError(rc, callType);
  if (rc != RPC2_SUCCESS)
    advice = FetchFromServers;

  assert(advice >= -1);
  assert(advice <= MaxCacheMissAdvice);

  LOG(100, ("L adviceconn::RequestWeaklyConnectedCacheMissAdvice() with %s\n", 
	    CacheMissAdviceToString(advice)));
  return(advice);
}

void adviceconn::InformLostConnection() {
  long rc;

  LOG(100, ("Informing advice server it has lost connection\n"));

  ObtainWriteLock(&userLock);

  ReleaseWriteLock(&userLock);
  rc = C_LostConnection(handle);
  if (rc != RPC2_SUCCESS)
    LOG(0, ("%s: ConnectionLost message failed.\n", RPC2_ErrorMsg((int)rc)));
  else
    LOG(100, ("Advice server knows it has lost connection\n"));

  Reset();
}

int adviceconn::NewConnection(char *hostName, int portNumber, int pgrp) {
  char myHostName[MAXHOSTNAMELEN+1];
  assert(strlen(hostName) <= MAXHOSTNAMELEN);

  LOG(100, ("E adviceconn::NewConnection(%s, %d,  %d)\n", 
	    hostName, portNumber, pgrp));
  if (IsAdviceOutstanding(0) == TRUE) {
    LOG(0, ("Cannot start a new advice monitor while a request for advice is outstanding!\n"));
    return(CAEADVICEPENDING);
  }

  if (IsAdviceValid((InterestID)-1, 0) == TRUE) {
    LOG(0, ("adviceconn::NewConnection:  Inform old advice server that it has lost its connection\n"));
    InformLostConnection();
  }

  strcpy(hostname, hostName);
  int hostlen = gethostname(myHostName, MAXHOSTNAMELEN+1);
  assert(hostlen != -1);
  assert(myHostName != NULL);
  assert(hostname != NULL);
  assert((strncmp(hostname, "localhost", strlen("localhost")) == 0) ||
	 (strncmp(hostname, myHostName, MAXHOSTNAMELEN)) == 0);
  
  port = (unsigned short) portNumber;
  pgid = pgrp;
  state = AdviceWaiting;

  LOG(100, ("L adviceconn::NewConnection()\n"));
  return(0);
}

int adviceconn::RegisterInterest(vuid_t uid, long numEvents, InterestValuePair events[])
{
    char formatString[64];

    LOG(0, ("adviceconn::RegisterInterest: %d is interested in the following %d items:\n", 
	    uid, numEvents));
    sprintf(formatString, "    %%%ds:  <argument=%%d, value=%%d>\n", MAXEVENTLEN);

    if (!AuthorizedUser(uid)) {
      LOG(0, ("adviceconn::RegisterInterest:  Unauthorized user (%d) attempted to register interest\n", uid));
      return(-1);
    }

    for (int i=0; i < numEvents; i++) {
        LOG(0, (formatString, InterestToString(events[i].interest), 
		events[i].argument, events[i].value));
	InterestArray[events[i].interest] = events[i].value;
	if (events[i].interest == HoardWalkAdviceRequestID) {
	    if (events[i].value == 0) 
	      HDB->SetSolicitAdvice(-1);
	    else if (events[i].value == 1)
	      HDB->SetSolicitAdvice(uid);
	    else
	      LOG(0, ("adviceconn::RegisterInterest:  Unknown value (%d) for HoardWalkAdviceRequest -- ignored\n", events[i].value));
	}

    }

    return(0);
}

void adviceconn::InitializeProgramLog(vuid_t uid) {
    char UserSpoolDir[MAXPATHLEN];
    char programFileName[MAXPATHLEN];
    int rc;

    if (programFILE != NULL)
      return;

    MakeUserSpoolDir(UserSpoolDir, uid);
    snprintf(programLogName, MAXPATHLEN, "%s/%s", 
	     UserSpoolDir, PROGRAMLOG);
    (void) unlink(programLogName);
    LOG(0, ("Opening %s\n", programLogName));

    programFILE = fopen(programLogName, "a");
    if (programFILE == NULL) 
      LOG(0, ("InitializeProgramLog(%d) failed\n", uid));

    numLines = 0;
    return;
}

void adviceconn::SwapProgramLog() {
    char oldName[MAXPATHLEN];

    if (programFILE == NULL) return;

    fflush(programFILE);
    snprintf(oldName, MAXPATHLEN, "%s.old", programLogName);
    LOG(0, ("Moving %s to %s\n", programLogName, oldName));

    if (rename(programLogName, oldName) < 0) {
        LOG(0, ("rename(%s, %s) failed (%d)\n",
		programLogName, oldName, errno));
        return;
    }

    freopen(programLogName, "a", programFILE);
    resetpid();
    numLines = 0;

    NotifyProgramAccessLogAvailable(oldName);
}

#define MAXLINES 1000
void adviceconn::LogProgramAccess(int pid, int pgid, ViceFid *fid) {
    if (programFILE != NULL) {
        outputcommandname(programFILE, pgid);
        fprintf(programFILE, "%d %d <%x.%x.%x>\n", 
		pid, pgid, fid->Volume, fid->Vnode, fid->Unique);

	if (++numLines > MAXLINES) 
	    SwapProgramLog();
    }
    return;
}

void adviceconn::InitializeReplacementLog(vuid_t uid) {
    char UserSpoolDir[MAXPATHLEN];
    char replacementFileName[MAXPATHLEN];
    int rc;

    if (replacementFILE != NULL)
      return;

    MakeUserSpoolDir(UserSpoolDir, uid);
    snprintf(replacementLogName, MAXPATHLEN, "%s/%s", 
	     UserSpoolDir, REPLACEMENTLOG);
    (void) unlink(replacementLogName);
    LOG(0, ("Opening %s\n", replacementLogName));

    replacementFILE = fopen(replacementLogName, "a");
    if (replacementFILE == NULL) 
      LOG(0, ("InitializeReplacementLog(%d) failed\n", uid));

    numLines = 0;
    return;
}

void adviceconn::SwapReplacementLog() {
    char oldName[MAXPATHLEN];

    if (replacementFILE == NULL) return;

    fflush(replacementFILE);
    snprintf(oldName, MAXPATHLEN, "%s.old", replacementLogName);
    LOG(0, ("Moving %s to %s\n", replacementLogName, oldName));

    if (rename(replacementLogName, oldName) < 0) {
        LOG(0, ("rename(%s, %s) failed (%d)\n",
	        replacementLogName, oldName, errno));
        return;
    }

    freopen(replacementLogName, "a", replacementFILE);
    resetpid();
    numLines = 0;

    NotifyReplacementLogAvailable(oldName);
}

#define MAXRLINES 100
void adviceconn::LogReplacement(char *path, int status, int data) {
    if (replacementFILE != NULL) {
        fprintf(replacementFILE, "%s %d %d\n", path, status, data);

	if (++numLines > MAXRLINES) 
	    SwapReplacementLog();
    }
    return;
}

int adviceconn::OutputUsageStatistics(vuid_t uid, char *pathname) {

    if (!AuthorizedUser(uid)) {
      LOG(0, ("adviceconn::OutputUsageStatistics:  Unauthorized user (%d) requested usage statistics\n", uid));
      return(-1);
    }

    LOG(0, ("E OutputUsageStatistics(%s)\n", pathname));

    assert(FSDB);
    FSDB->OutputDisconnectedUseStatistics(pathname);
    LOG(0, ("L OutputUsageStatistics(%s)\n", pathname));
    return(0);
}


//int adviceconn::BeginStoplightMonitor() {
//    LOG(100, ("E adviceconn::BeginStoplightMonitor()\n"));
//    SetStoplightData();
//    LOG(100, ("L adviceconn::BeginStoplightMonitor()\n"));
//    return(0);
//}

//int adviceconn::EndStoplightMonitor() {
//    LOG(100, ("E adviceconn::EndStoplightMonitor()\n"));
//    UnsetStoplightData();
//    LOG(100, ("L adviceconn::EndStoplightMonitor()\n"));
//    return(0);
//}


void adviceconn::CheckConnection() {
  LOG(100, ("E adviceconn::CheckConnection(): state = %s\n", StateString()));
  if (state == AdviceDying) 
    TearDownConnection();
  if (state == AdviceWaiting) 
    ReturnConnection();
  LOG(100, ("L adviceconn::CheckConnection()\n"));
}

void adviceconn::ReturnConnection() {
  RPC2_HostIdent hid;
  RPC2_PortalIdent pid;
  RPC2_SubsysIdent sid;
  RPC2_Handle cid;
  long rc;
  RPC2_BindParms bp;

  LOG(100, ("E adviceconn:ReturnConnection:  return connection to %s on port %d\n", hostname, port));

  assert(state == AdviceWaiting);
  assert(strlen(hostname) < 64);

  ObtainWriteLock(&userLock);
  state = AdviceInvalid;

  hid.Tag = RPC2_HOSTBYNAME;
  strcpy(hid.Value.Name, hostname);
  pid.Tag = RPC2_PORTALBYINETNUMBER;
  pid.Value.InetPortNumber = port;
  sid.Tag = RPC2_SUBSYSBYID;
  sid.Value.SubsysId = ADMONSUBSYSID;

  bp.SecurityLevel = RPC2_OPENKIMONO;
  bp.EncryptionType = NULL;
  bp.SideEffectType = NULL;
  bp.ClientIdent = NULL;
  bp.SharedSecret = NULL;
  rc = RPC2_NewBinding(&hid, &pid, &sid, &bp, &cid);
  if (rc != RPC2_SUCCESS) {
    LOG(0, ("%s: Cannot connect to machine %s on port %d\n",
	    RPC2_ErrorMsg((int)rc), hostname, port));
    Reset();
  }
  else {
    state = AdviceValid;
    handle = cid;
  }

  ReleaseWriteLock(&userLock);
  LOG(100, ("L adviceconn::ReturnConnection()\n"));
}

void adviceconn::TearDownConnection() {
  assert(state == AdviceDying);

  LOG(100, ("adviceconn::TearDownConnection()\n"));

  Reset();
}

void adviceconn::CheckError(long rpc_code, InterestID callType) {
    int Invalidate = 1;

    TotalAttempts++;
    switch (rpc_code) {
      case RPC2_SUCCESS:
	CurrentValues[(int)callType].rpc_success++;
        Invalidate = 0;
        break;
      case RPC2_CONNBUSY:
	CurrentValues[(int)callType].rpc_connbusy++;
        LOG(0, ("ADMON STATS: Connection BUSY!\n"));
        Invalidate = 0;
        break;
      case RPC2_FAIL:
	CurrentValues[(int)callType].rpc_fail++;
        break;
      case RPC2_NOCONNECTION:
	CurrentValues[(int)callType].rpc_noconnection++;
        break;
      case RPC2_TIMEOUT:
	CurrentValues[(int)callType].rpc_timeout++;
        break;
      case RPC2_DEAD:
	CurrentValues[(int)callType].rpc_dead++;
        break;
      default:
	CurrentValues[(int)callType].rpc_othererrors++;
        LOG(0, ("ADMON STATS: Get advice failed with unanticipated error code!  (%s)\n", 
              RPC2_ErrorMsg((int)rpc_code)));
        LOG(0, ("Please report this error code to Maria!\n"));
        break;
    }

    if (Invalidate) {
        LOG(0, ("ADMON STATS: Get advice failed(%s)!  Invalidating connection!\n", RPC2_ErrorMsg((int)rpc_code)));
        Print(logFile);
        InvalidateConnection();
    }
}

void adviceconn::InvalidateConnection() {
  Reset();
}

void adviceconn::ResetCounters() {
  /* Initialize the counters. */ 
  AdviceNotEnabledCount = 0;            
  AdviceNotValidCount = 0;              
  AdviceOutstandingCount = 0;
  ASRnotAllowedCount = 0;
  ASRintervalNotReachedCount = 0;
  VolumeNullCount = 0;
  TotalAttempts = 0;                    
  for (int count = 0; count < MAXEVENTS; count++) {
    CurrentValues[count].requested = 0;
    CurrentValues[count].advicenotvalid = 0;
    CurrentValues[count].rpc_initiated = 0;
    CurrentValues[count].rpc_success = 0;
    CurrentValues[count].rpc_connbusy = 0;
    CurrentValues[count].rpc_fail = 0;
    CurrentValues[count].rpc_noconnection = 0;
    CurrentValues[count].rpc_timeout = 0;
    CurrentValues[count].rpc_dead = 0;
    CurrentValues[count].rpc_othererrors = 0;
  }
}
  
void adviceconn::Reset() {
  LOG(100, ("E adviceconn::Reset()\n"));
  state = AdviceInvalid;
  if (handle != -1)
    (void) RPC2_Unbind(handle);
  handle = -1;
  port = 0;
  bzero(hostname, MAXHOSTNAMELEN);

  for (int i = 0; i < MAXEVENTS; i++)
    InterestArray[i] = 0;

  programFILE = NULL;
  replacementFILE = NULL;

  LOG(100, ("L adviceconn::Reset()\n"));
}

void adviceconn::SetState(AdviceState newState) {
  state = newState;
}

void adviceconn::ObtainUserLock() {
  ObtainWriteLock(&userLock);
}

void adviceconn::ReleaseUserLock() {
  ReleaseWriteLock(&userLock);
}

int adviceconn::IsAdviceValid(InterestID callType, int bump) { /* bump == 1 --> stats will be incremented */
  if (state == AdviceValid)
     return TRUE;
  else {
     if ((bump) && (callType >= 0))
       IncrNotValid(callType);
     return FALSE;
  }
}

int adviceconn::IsAdviceOutstanding(int bump) { /* bump == 1 --> stats will be incremented */
  if (CheckLock(&userLock) == -1) {
     if (bump)
        AdviceOutstandingCount++;
     return TRUE;
  }
  else
     return FALSE;
}

int adviceconn::IsAdviceHandle(RPC2_Handle someHandle) {
  if (handle == someHandle)
    return TRUE;
  else
    return FALSE;
}

int adviceconn::Getpgid() {
  return pgid;
}

char *adviceconn::StateString()
{
  static char msgbuf[100];

  switch (state) {
    case AdviceInvalid:
      (void) sprintf(msgbuf, "AdviceInvalid");
      break;
    case AdviceDying:
      (void) sprintf(msgbuf, "AdviceDying");
      break;
    case AdviceWaiting:
      (void) sprintf(msgbuf, "AdviceWaiting");
      break;
    case AdviceValid:
      (void) sprintf(msgbuf, "AdviceValid");
      break;
    default:
      (void) sprintf(msgbuf, "UNKNOWN");
      break;
    }

  return(msgbuf);
}

#define MSGSIZE 64
char *adviceconn::CacheMissAdviceToString(CacheMissAdvice advice)
{
  static char msgbuf[MSGSIZE];

  switch (advice) {
    case FetchFromServers:
      (void) snprintf(msgbuf, MSGSIZE, "Fetch from servers");
      break;
    case CoerceToMiss:
      (void) snprintf(msgbuf, MSGSIZE, "Coerce to miss");
      break;
    default:
      (void) snprintf(msgbuf, MSGSIZE, "Unknown");
      break;
  }
  return(msgbuf);
}

int adviceconn::GetSuccesses(InterestID interest) {
    return(CurrentValues[interest].rpc_success);
}

int adviceconn::GetFailures(InterestID interest) {
    return(CurrentValues[interest].rpc_connbusy +
	    CurrentValues[interest].rpc_fail +
	    CurrentValues[interest].rpc_noconnection +
	    CurrentValues[interest].rpc_timeout +
	    CurrentValues[interest].rpc_dead +
	    CurrentValues[interest].rpc_othererrors);
}

void adviceconn::GetStatistics(AdviceCalls *calls, AdviceResults *results, AdviceStatistics *stats) {
    int counter;

    for (int i = 0; i < MAXEVENTS; i++) {
      calls[i].success = (RPC2_Integer)GetSuccesses((InterestID)i);
      calls[i].failures = (RPC2_Integer)GetFailures((InterestID)i);
    }

    for (counter = 0; counter < NumRPCResultTypes; counter++) 
        results[counter].count = 0;
    for (counter = 0; counter < MAXEVENTS; counter++) {
        results[0].count += GetSUCCESS((InterestID)counter);
        results[1].count += GetCONNBUSY((InterestID)counter);
        results[2].count += GetFAIL((InterestID)counter);
        results[3].count += GetNOCONNECTION((InterestID)counter);
        results[4].count += GetTIMEOUT((InterestID)counter);
        results[5].count += GetDEAD((InterestID)counter);
        results[6].count += GetOTHER((InterestID)counter);
    }

    stats->NotEnabled = AdviceNotEnabledCount;
    stats->NotValid = AdviceNotValidCount;
    stats->Outstanding = AdviceOutstandingCount;
    stats->ASRnotAllowed = ASRnotAllowedCount;
    stats->ASRinterval = ASRintervalNotReachedCount;
    stats->VolumeNull = VolumeNullCount;
    stats->TotalNumberAttempts = TotalAttempts;
}

void adviceconn::Print() {
  Print(stdout); 
}

void adviceconn::Print(FILE *fp) {
  fflush(fp);
  Print(fileno(fp));
  fflush(fp);
}

void adviceconn::Print(int afd) {

  if (LogLevel < 100) return;

  LOG(100, ("adviceconn::Print()\n")); fflush(logFile);

  fdprint(afd, "%#08x : State = %s, Host = %s, Port = %d, Handle = %d\n", 
          (long)this, StateString(), hostname, port, handle);
  fdprint(afd, "NotEnabled=%d, NotValid=%d, VolumeNull=%d\n", AdviceNotEnabledCount, AdviceNotValidCount, VolumeNullCount);
  fdprint(afd, "TotalAttempted calls = %d\n", TotalAttempts);

  for (int count = 0; count < MAXEVENTS; count++) {
    fdprint(afd, "%s: <req=%d, anv=%d, ri=%d, rs=%d, rcb=%d, rf=%d, rnc=%d, rto=%d, rd=%d, ro=%d>\n",
	    InterestToString((InterestID)count),
	    CurrentValues[count].requested,
	    CurrentValues[count].advicenotvalid,
	    CurrentValues[count].rpc_initiated,
	    CurrentValues[count].rpc_success,
	    CurrentValues[count].rpc_connbusy,
	    CurrentValues[count].rpc_fail,
	    CurrentValues[count].rpc_noconnection,
	    CurrentValues[count].rpc_timeout,
	    CurrentValues[count].rpc_dead ,
	    CurrentValues[count].rpc_othererrors);
  }
}


void adviceconn::PrintState() {
  PrintState(stdout);
}

void adviceconn::PrintState(FILE *fp) {
  fflush(fp);
  PrintState(fileno(fp));
  fflush(fp);
}

void adviceconn::PrintState(int afd) {
  fdprint(afd, "State = %s\n", StateString());
}

/***************************************************************************************
 *********************************** HELPER ROUTINES ***********************************
 ***************************************************************************************/

char *InterestToString(InterestID interest) {
  static char returnString[MAXEVENTLEN];

  switch (interest) {
      case TokensAcquiredID:
        strncpy(returnString, "TokensAcquired", MAXEVENTLEN);
        break;
      case TokensExpiredID:
        strncpy(returnString, "TokensExpired", MAXEVENTLEN);
        break;
      case ActivityPendingTokensID:
        strncpy(returnString, "ActivityPendingTokens", MAXEVENTLEN);
        break;
      case SpaceInformationID:
        strncpy(returnString, "SpaceInformation", MAXEVENTLEN);
        break;
      case ServerAccessibleID:
        strncpy(returnString, "ServerAccessible", MAXEVENTLEN);
        break;
      case ServerInaccessibleID:
        strncpy(returnString, "ServerInaccessible", MAXEVENTLEN);
        break;
      case ServerConnectionStrongID:
        strncpy(returnString, "ServerConnectionStrong", MAXEVENTLEN);
	break;
      case ServerConnectionWeakID:
        strncpy(returnString, "ServerConnectionWeak", MAXEVENTLEN);
	break;
      case NetworkQualityEstimateID:
        strncpy(returnString, "NetworkQualityEstimate", MAXEVENTLEN);
        break;
      case VolumeTransitionEventID:
        strncpy(returnString, "VolumeTransitionEvent", MAXEVENTLEN);
        break;
      case ReconnectionID:
        strncpy(returnString, "Reconnection", MAXEVENTLEN);
        break;
      case DataFetchEventID:
        strncpy(returnString, "DataFetchEvent", MAXEVENTLEN);
        break;
      case ReadDisconnectedCacheMissEventID:
        strncpy(returnString, "ReadDisconnectedCacheMissEvent", MAXEVENTLEN);
        break;
      case WeaklyConnectedCacheMissEventID:
        strncpy(returnString, "WeaklyConnectedCacheMissEvent", MAXEVENTLEN);
        break;
      case DisconnectedCacheMissEventID:
        strncpy(returnString, "DisconnectedCacheMissEvent", MAXEVENTLEN);
        break;
      case HoardWalkAdviceRequestID:
        strncpy(returnString, "HoardWalkAdviceRequest", MAXEVENTLEN);
        break;
      case HoardWalkBeginID:
        strncpy(returnString, "HoardWalkBegin", MAXEVENTLEN);
        break;
      case HoardWalkStatusID:
        strncpy(returnString, "HoardWalkStatus", MAXEVENTLEN);
        break;
      case HoardWalkEndID:
        strncpy(returnString, "HoardWalkEnd", MAXEVENTLEN);
        break;
      case HoardWalkPeriodicOnID:
        strncpy(returnString, "HoardWalkPeriodicOn", MAXEVENTLEN);
        break;
      case HoardWalkPeriodicOffID:
        strncpy(returnString, "HoardWalkPeriodicOff", MAXEVENTLEN);
        break;
      case ObjectInConflictID:
        strncpy(returnString, "ObjectInConflict", MAXEVENTLEN);
        break;
      case ObjectConsistentID:
        strncpy(returnString, "ObjectConsistent", MAXEVENTLEN);
        break;
      case ReintegrationPendingTokensID:
        strncpy(returnString, "ReintegrationPendingTokens", MAXEVENTLEN);
        break;
      case ReintegrationEnabledID:
        strncpy(returnString, "ReintegrationEnabled", MAXEVENTLEN);
        break;
      case ReintegrationActiveID:
        strncpy(returnString, "ReintegrationActive", MAXEVENTLEN);
        break;
      case ReintegrationCompletedID:
        strncpy(returnString, "ReintegrationCompleted", MAXEVENTLEN);
        break;
      case TaskAvailabilityID:
        strncpy(returnString, "TaskAvailable", MAXEVENTLEN);
        break;
      case TaskUnavailableID:
        strncpy(returnString, "TaskUnavailable", MAXEVENTLEN);
        break;
      case InvokeASRID:
        strncpy(returnString, "InvokeASR", MAXEVENTLEN);
        break;
      default:
        assert(1 == 0);
  }

  return(returnString);
}
