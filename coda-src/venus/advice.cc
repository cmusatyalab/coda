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

static char *rcsid = "$Header: /home/braam/src/coda-src/venus/RCS/advice.cc,v 1.1 1996/11/22 19:10:45 braam Exp braam $";
#endif /*_BLURB_*/





#include "venus.private.h"

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <netinet/in.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include "user.h"
#include "advice.h"
#include "adviceconn.h"
#include "admon.h"
#include "adsrv.h"

#define FALSE 0
#define TRUE 1

int ASRinProgress = 0;
int ASRresult = -1;

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

ReadDiscAdvice adviceconn::RequestReadDisconnectedCacheMissAdvice(char *pathname, int pid) {
  RPC2_Integer advice;
  long rc;

  if (!IsAdviceValid(0))
    return(ReadDiscUnknown);
  if (ReadDisconnectedCacheMisses == 0)
    return(ReadDiscUnknown);

  LOG(100, ("E adviceconn::RequestReadDisconnectedCacheMissAdvice()\n"));
  ObtainWriteLock(&userLock);
  LOG(100, ("Requesting read disconnected cache miss advice on %s from handle = %d\n", pathname, handle));
  rc = ReadDisconnectedMiss(handle, (RPC2_String) pathname, (RPC2_Integer) pid, &advice) ;
  ReleaseWriteLock(&userLock);

  CheckError(rc, PCM);
  if (rc != RPC2_SUCCESS) 
    advice = ReadDiscUnknown;

  LOG(100, ("Advice was to %s on %s\n", ReadDiscAdviceString((ReadDiscAdvice)advice), pathname));

  assert(advice >= -1);
  assert(advice <= MaxReadDiscAdvice);
  LOG(100, ("L adviceconn::RequestReadDisconnectedCacheMissAdvice()\n"));
  return((ReadDiscAdvice)advice);
}

void adviceconn::RequestHoardWalkAdvice(char *input, char *output) {
  RPC2_Integer ReturnCode;
  long rc;

  if (HoardWalks == 0)
    return;

  LOG(100, ("E adviceconn::RequestHoardWalkAdvice()\n"));
  ObtainWriteLock(&userLock);
  LOG(100, ("Requesting hoard walk advice on %s\n", input));
  rc = HoardWalkAdvice(handle, (RPC2_String)input, (RPC2_String)output, &ReturnCode);
  ReleaseWriteLock(&userLock);

  CheckError(rc, HWA);

  LOG(100, ("L adviceconn::RequestHoardWalkAdvice(ReturnCode=%d)\n", (int)ReturnCode));
  return;
}


void adviceconn::RequestDisconnectedQuestionnaire(char *pathname, int pid, ViceFid *fid, long DiscoTime) {
  DisconnectedMissQuestionnaire questionnaire;
  RPC2_Integer Qrc;
  long rc;

  if (!IsAdviceValid(0))
    return;
  if (DisconnectedCacheMisses == 0)
    return;

  questionnaire.DMQVersionNumber = DMQ_VERSION;
  questionnaire.pid = pid;
  questionnaire.TimeOfDisconnection = (RPC2_Unsigned) DiscoTime;
  questionnaire.TimeOfCacheMiss = (RPC2_Unsigned) Vtime();
  questionnaire.Fid = *fid;
  questionnaire.Pathname = (RPC2_String) pathname;

  LOG(100, ("E adviceconn::RequestDisconnectedQuestionnaire()\n"));
  ObtainWriteLock(&userLock);
  rc = DisconnectedMiss(handle, &questionnaire, &Qrc);
  ReleaseWriteLock(&userLock);

  CheckError(rc, DM);

  LOG(100, ("L adviceconn::RequestDisconnectedQuestionnaire()\n"));
  return;
}

void adviceconn::NotifyHoarding(char *volname, VolumeId vid) {
  long rc;

  if (!IsAdviceValid(0))
    return;

  LOG(100, ("E adviceconn::NotifyHoarding(volname=%s, vid=%x)\n", volname, vid));

  ObtainWriteLock(&userLock);
  rc = VSHoarding(handle, (RPC2_String)volname, vid);
  ReleaseWriteLock(&userLock);

  CheckError(rc, NHoarding);

  LOG(100, ("L adviceconn::NotifyHoarding()\n"));
  return;
}

void adviceconn::NotifyEmulating(char *volname, VolumeId vid) {
  long rc;

  if (!IsAdviceValid(0))
    return;

  LOG(100, ("E adviceconn::NotifyEmulating(volname=%s, vid=%x)\n", volname, vid));

  ObtainWriteLock(&userLock);
  rc = VSEmulating(handle, (RPC2_String)volname, vid);
  ReleaseWriteLock(&userLock);

  CheckError(rc, NEmulating);

  LOG(100, ("L adviceconn::NotifyEmulating()\n"));
  return;
}

void adviceconn::NotifyLogging(char *volname, VolumeId vid) {
  long rc;

  if (!IsAdviceValid(0))
    return;

  LOG(100, ("E adviceconn::NotifyLogging(volname=%s, vid=%x)\n", volname, vid));

  ObtainWriteLock(&userLock);
  rc = VSLogging(handle, (RPC2_String)volname, vid);
  ReleaseWriteLock(&userLock);

  CheckError(rc, NLogging);

  LOG(100, ("L adviceconn::NotifyLogging()\n"));
  return;
}

void adviceconn::NotifyResolving(char *volname, VolumeId vid) {
  long rc;

  if (!IsAdviceValid(0))
    return;

  LOG(100, ("E adviceconn::NotifyResolving(volname=%s, vid=%x)\n", volname, vid));

  ObtainWriteLock(&userLock);
  rc = VSResolving(handle, (RPC2_String)volname, vid);
  ReleaseWriteLock(&userLock);

  CheckError(rc, NResolving);

  LOG(100, ("L adviceconn::NotifyResolving()\n"));
  return;
}

void adviceconn::RequestReconnectionQuestionnaire(char *volname, VolumeId vid, int CMLcount, long DiscoTime, long WalkTime, int NumberReboots, int cacheHit, int cacheMiss, int unique_hits, int unique_nonrefs) {
  ReconnectionQuestionnaire questionnaire;
  RPC2_Integer Qrc;
  long rc;

  if (!IsAdviceValid(0))
    return;
  if (ReconnectionQuestionnaires == 0)
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
  rc = Reconnection(handle, &questionnaire, &Qrc);
  ReleaseWriteLock(&userLock);

  CheckError(rc, R);

  LOG(100, ("L adviceconn::RequestReconnectionQuestionnaire()\n"));
  return;
}


void adviceconn::RequestReintegratePending(char *volname, int flag) {
  long rc;
  Boolean boo;

  if (!IsAdviceValid(0))
    return;
  if (ReintegrationPendings == 0)
    return;

  LOG(100, ("E adviceconn::RequestReintegratePendingTokens()\n"));

  assert((flag == 0) || (flag == 1));
  boo = (Boolean)flag;

  ObtainWriteLock(&userLock);
  rc = ReintegratePending(handle, (RPC2_String)volname, boo);
  ReleaseWriteLock(&userLock);

  CheckError(rc, RP);

  LOG(100, ("L adviceconn::RequestReintegratePendingTokens()\n"));
  return;
}


int adviceconn::RequestASRInvokation(char *pathname, vuid_t vuid) {
  long rc;
  RPC2_Integer ASRid;
  RPC2_Integer ASRrc;

  if (ASRs == 0)
    return(-1);

  LOG(100, ("E adviceconn::RequestASRInvokation(%s, %d)\n", pathname, vuid));
  if (ASRinProgress) {
    LOG(0, ("adviceconn::RASRI() - One ASR already in progress \n", 
	    pathname, vuid));
    return(-1);
  }
  ASRinProgress = 1;	// XXX gross way of locking ASR Invocation - Puneet
  ObtainWriteLock(&userLock);
  LOG(100, ("\tRequesting ASR Invokation\n"));
  rc = InvokeASR(handle, (RPC2_String)pathname, vuid, &ASRid, &ASRrc);
  ASRinProgress = (int)ASRid;
  ReleaseWriteLock(&userLock);
  
  CheckError(rc, IASR);
  if (rc != RPC2_SUCCESS) 
    ASRinProgress = 0;	// XXX gross method for unlocking ASR invocation
  if (ASRrc != ADMON_SUCCESS) 
    ASRinProgress = 0;  // XXX gross method for unlocking ASR invocation

  LOG(100, ("Result of ASR was %d", ASRrc));
  return(rc);
}

WeaklyAdvice adviceconn::RequestWeaklyConnectedCacheMissAdvice(char *pathname, int pid, int expectedCost) {
  WeaklyConnectedInformation information;
  RPC2_Integer advice;
  long rc;

  if (!IsAdviceValid(0)) {
    LOG(100, ("RequestWeaklyConnectedCacheMissAdvice: Advice not valid for this user...\n"));
    return(WeaklyUnknown);
  }
  if (WeaklyConnectedCacheMisses == 0) {
    LOG(100, ("RequestWeaklyConnectedCacheMissAdvice: This type of advice not requested for this user...\n"));
    return(WeaklyFetch);
  }

  information.Pathname = (RPC2_String) pathname;
  information.pid = pid;
  information.ExpectedFetchTime = expectedCost;

  LOG(100, ("E adviceconn::RequestWeaklyConnectedCacheMiss(%s)\n", pathname));
  ObtainWriteLock(&userLock);
  rc = WeaklyConnectedMiss(handle, &information, &advice);
  ReleaseWriteLock(&userLock);

  CheckError(rc, WCM);
  if (rc != RPC2_SUCCESS)
    advice = (RPC2_Integer)WeaklyUnknown;

  assert(advice >= (RPC2_Integer)-1);
  assert(advice <= (RPC2_Integer)MaxWeaklyAdvice);

  LOG(100, ("L adviceconn::RequestWeaklyConnectedCacheMissAdvice() with %s\n", WeaklyAdviceString((WeaklyAdvice)advice)));
  return((WeaklyAdvice)advice);
}

void adviceconn::InformLostConnection() {
  long rc;

  LOG(100, ("Informing advice server it has lost connection\n"));

  ObtainWriteLock(&userLock);
  rc = LostConnection(handle);
  ReleaseWriteLock(&userLock);
  CheckError(rc, LC);
  if (rc != RPC2_SUCCESS)
    LOG(0, ("%s: ConnectionLost message failed.\n", RPC2_ErrorMsg((int)rc)));
  else
    LOG(100, ("Advice server knows it has lost connection\n"));

  Reset();
}

int adviceconn::NewConnection(char *hostName, int portNumber, int pgrp) {
  assert(strlen(hostName) <= MAXHOSTNAMELEN);

  LOG(100, ("E adviceconn::NewConnection(%s, %d,  %d)\n", hostName, portNumber, pgrp));
  if (IsAdviceOutstanding(0) == TRUE) {
    LOG(0, ("Cannot start a new advice monitor while a request for advice is outstanding!\n"));
    return(-1);
  }

  if (IsAdviceValid(0) == TRUE) {
    LOG(0, ("adviceconn::NewConnection:  Inform old advice server that it has lost its connection\n"));
    InformLostConnection();
  }

  strcpy(hostname, hostName);
  LOG(0, ("MARIA: You should check that the hostname is our host\n"));
  port = (unsigned short) portNumber;
  pgid = pgrp;
  state = AdviceWaiting;
  LOG(100, ("L adviceconn::NewConnection()\n"));
  return(0);
}

int adviceconn::RegisterInterest(vuid_t uid, long numEvents, InterestValuePair events[])
{
    for (int i=0; i < numEvents; i++) {
	LOG(0, ("adviceconn::RegisterInterest:  events[%d].interest = %d\n", i, events[i].interest));
        switch (events[i].interest) {
	    case InconsistentObject:
		LOG(0, ("adviceconn::RegisterInterest:  Register %d for inconsistent objects\n", events[i].value));
		InconsistentObjects = events[i].value;
		break;
	    case ReadDisconnectedCacheMiss:
		LOG(0, ("adviceconn::RegisterInterest:  Register %d for read disconnected cache misses\n", events[i].value));
		ReadDisconnectedCacheMisses = events[i].value;
		break;
	    case WeaklyConnectedCacheMiss:
		LOG(0, ("adviceconn::RegisterInterest:  Register %d for weakly connected cache misses\n", events[i].value));
		WeaklyConnectedCacheMisses = events[i].value;
		break;
	    case DisconnectedCacheMiss:
		LOG(0, ("adviceconn::RegisterInterest:  Register %d for disconnected cache misses\n", events[i].value));
		DisconnectedCacheMisses = events[i].value;
		break;
	    case VolumeTransition:
		LOG(0, ("adviceconn::RegisterInterest:  Register %d for volume transitions\n", events[i].value));
		VolumeTransitions = events[i].value;
		break;
	    case ReconnectionEvent:
		LOG(0, ("adviceconn::RegisterInterest:  Register %d for reconnection questionnaires\n", events[i].value));
		ReconnectionQuestionnaires = events[i].value;
		break;
	    case ReintegrationPending:
		LOG(0, ("adviceconn::RegisterInterest:  Register %d for pending reintegrations\n", events[i].value));
		ReintegrationPendings = events[i].value;
		break;
	    case HoardWalk:
		LOG(0, ("adviceconn::RegisterInterest:  Register %d for hoard walks\n", events[i].value));
		if (!AuthorizedUser(uid)) {
		  LOG(0, ("adviceconn::RegisterInterest:  Unauthorized user (%d) attempted to capture hoard walk advice requests\n", uid));
		  return(-1);
		}

		HoardWalks = events[i].value;

		if (events[i].value == 0) 
		    HDB->SetSolicitAdvice(-1);
		else if (events[i].value == 1)
		    HDB->SetSolicitAdvice(uid);
		else
		    LOG(0, ("adviceconn::RegisterInterest:  Unknown value for HoardWalk -- ignored\n"));
		break;
	    case ASR:
		LOG(0, ("adviceconn::RegisterInterest:  Register %d for application specific resolvers\n", events[i].value));
		ASRs = events[i].value;
		break;
	    default:
		LOG(0, ("adviceconn::RegisterInterest:  Unknown interest %d -- ignored\n", events[i].interest));
        }
    }
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

void adviceconn::CheckError(long rpc_code, CallTypes callType) {
    int Invalidate = 1;

    TotalAttempts++;
    switch (rpc_code) {
      case RPC2_SUCCESS:
        NumSUCCESS[(int)callType]++;
        Invalidate = 0;
        break;
      case RPC2_CONNBUSY:
        NumCONNBUSY[(int)callType]++;
        LOG(0, ("ADMON STATS: Connection BUSY!\n"));
        Invalidate = 0;
        break;
      case RPC2_FAIL:
        NumFAIL[(int)callType]++;
        break;
      case RPC2_NOCONNECTION:
        NumNOCONNECTION[(int)callType]++;
        break;
      case RPC2_TIMEOUT:
        NumTIMEOUT[(int)callType]++;
        break;
      case RPC2_DEAD:
        NumDEAD[(int)callType]++;
        break;
      default:
        NumRPC2otherErrors[(int)callType]++;
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
  for (int count = 0; count < NumCallTypes; count++) {
    NumSUCCESS[count] = 0;              
    NumCONNBUSY[count] = 0;             
    NumFAIL[count] = 0;                 
    NumNOCONNECTION[count] = 0;         
    NumTIMEOUT[count] = 0;              
    NumDEAD[count] = 0;                 
    NumRPC2otherErrors[count] = 0;      
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

  InconsistentObjects = 0;
  ReadDisconnectedCacheMisses = 0;
  WeaklyConnectedCacheMisses = 0;
  DisconnectedCacheMisses = 0;
  VolumeTransitions = 0;
  ReconnectionQuestionnaires = 0;
  ReintegrationPendings = 0;
  HoardWalks = 0; 
  ASRs = 0;

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

int adviceconn::IsAdviceValid(int bump) { /* bump == 1 --> stats will be incremented */
  if (state == AdviceValid)
     return TRUE;
  else {
     if (bump)
        AdviceNotValidCount++;
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

char *adviceconn::ReadDiscAdviceString(ReadDiscAdvice advice)
{
  static char msgbuf[100];

  switch (advice) {
    case ReadDiscFetch:
      (void) sprintf(msgbuf, "FETCH");
      break;
    case ReadDiscHOARDimmedFETCH:
      (void) sprintf(msgbuf, "HOARD with immediate FETCH");
      break;
    case ReadDiscHOARDdelayFETCH:
      (void) sprintf(msgbuf, "HOARD with delayed FETCH");
      break;
    case ReadDiscTimeout:
      (void) sprintf(msgbuf, "MISS");
      break;
    case ReadDiscUnknown:
    default:
      (void) sprintf(msgbuf, "UNKNOWN");
      break;
    }

  return(msgbuf);

}

char *adviceconn::WeaklyAdviceString(WeaklyAdvice advice)
{
  static char msgbuf[100];

  switch (advice) {
    case WeaklyFetch:
      (void) sprintf(msgbuf, "FETCH");
      break;
    case WeaklyMiss:
      (void) sprintf(msgbuf, "COERCED MISS");
      break;
    case WeaklyUnknown:
    default:
      (void) sprintf(msgbuf, "UNKNOWN");
      break;
  }

  return(msgbuf);
}


void adviceconn::GetStatistics(AdviceCalls *calls, AdviceResults *results, AdviceStatistics *stats) {
    int counter;

        calls[0].success = (RPC2_Integer)NumSUCCESS[PCM];
        calls[0].failures = (RPC2_Integer)(NumCONNBUSY[PCM] + NumFAIL[PCM] + NumNOCONNECTION[PCM] + NumTIMEOUT[PCM] + NumDEAD[PCM] + NumRPC2otherErrors[PCM]);

        calls[1].success = (RPC2_Integer)NumSUCCESS[HWA];
        calls[1].failures = (RPC2_Integer)(NumCONNBUSY[HWA] + NumFAIL[HWA] + NumNOCONNECTION[HWA] + NumTIMEOUT[HWA] + NumDEAD[HWA] + NumRPC2otherErrors[HWA]);

        calls[2].success = (RPC2_Integer)NumSUCCESS[DM];
        calls[2].failures = (RPC2_Integer)(NumCONNBUSY[DM] + NumFAIL[DM] + NumNOCONNECTION[DM] + NumTIMEOUT[DM] + NumDEAD[DM] + NumRPC2otherErrors[DM]);

        calls[3].success = (RPC2_Integer)NumSUCCESS[R];
        calls[3].failures = (RPC2_Integer)(NumCONNBUSY[R] + NumFAIL[R] + NumNOCONNECTION[R] + NumTIMEOUT[R] + NumDEAD[R] + NumRPC2otherErrors[R]);

        calls[4].success = (RPC2_Integer)NumSUCCESS[RP];
        calls[4].failures = (RPC2_Integer)(NumCONNBUSY[RP] + NumFAIL[RP] + NumNOCONNECTION[RP] + NumTIMEOUT[RP] + NumDEAD[RP] + NumRPC2otherErrors[RP]);

        calls[5].success = (RPC2_Integer)NumSUCCESS[IASR];
        calls[5].failures = (RPC2_Integer)(NumCONNBUSY[IASR] + NumFAIL[IASR] + NumNOCONNECTION[IASR] + NumTIMEOUT[IASR] + NumDEAD[IASR] + NumRPC2otherErrors[IASR]);

        calls[6].success = (RPC2_Integer)NumSUCCESS[LC];
        calls[6].failures = (RPC2_Integer)(NumCONNBUSY[LC] + NumFAIL[LC] + NumNOCONNECTION[LC] + NumTIMEOUT[LC] + NumDEAD[LC] + NumRPC2otherErrors[LC]);

        calls[7].success = (RPC2_Integer)NumSUCCESS[WCM];
        calls[7].failures = (RPC2_Integer)(NumCONNBUSY[WCM] + NumFAIL[WCM] + NumNOCONNECTION[WCM] + NumTIMEOUT[WCM] + NumDEAD[WCM] + NumRPC2otherErrors[WCM]);

        for (counter = 0; counter < NumRPCResultTypes; counter++) 
            results[counter].count = 0;
        for (counter = 0; counter < NumCallTypes; counter++) {
            results[0].count += NumSUCCESS[counter];        
            results[1].count += NumCONNBUSY[counter];      
            results[2].count += NumFAIL[counter];              
            results[3].count += NumNOCONNECTION[counter];      
            results[4].count += NumTIMEOUT[counter];        
            results[5].count += NumDEAD[counter];
            results[6].count += NumRPC2otherErrors[counter];
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
  fdprint(afd, "SUCCESS = (%d, %d, %d, %d, %d, %d, %d)\n", NumSUCCESS[0], NumSUCCESS[1], NumSUCCESS[2], NumSUCCESS[3], NumSUCCESS[4], NumSUCCESS[5], NumSUCCESS[6]);
  fdprint(afd, "CONNBUSY = (%d, %d, %d, %d, %d, %d, %d)\n", NumCONNBUSY[0], NumCONNBUSY[1], NumCONNBUSY[2], NumCONNBUSY[3], NumCONNBUSY[4], NumCONNBUSY[5], NumCONNBUSY[6]);
  fdprint(afd, "FAIL = (%d, %d, %d, %d, %d, %d, %d)\n", NumFAIL[0], NumFAIL[1], NumFAIL[2], NumFAIL[3], NumFAIL[4], NumFAIL[5], NumFAIL[6]);
  fdprint(afd, "NOCONNECTION = (%d, %d, %d, %d, %d, %d, %d)\n", NumNOCONNECTION[0], NumNOCONNECTION[1], NumNOCONNECTION[2], NumNOCONNECTION[3], NumNOCONNECTION[4], NumNOCONNECTION[5], NumNOCONNECTION[6]);
  fdprint(afd, "TIMEOUT = (%d, %d, %d, %d, %d, %d, %d)\n", NumTIMEOUT[0], NumTIMEOUT[1], NumTIMEOUT[2], NumTIMEOUT[3], NumTIMEOUT[4], NumTIMEOUT[5], NumTIMEOUT[6]);
  fdprint(afd, "DEAD = (%d, %d, %d, %d, %d, %d, %d)\n", NumDEAD[0], NumDEAD[1], NumDEAD[2], NumDEAD[3], NumDEAD[4], NumDEAD[5], NumDEAD[6]);
  fdprint(afd, "RPC2otherErrors = (%d, %d, %d, %d, %d, %d, %d)\n", NumRPC2otherErrors[0], NumRPC2otherErrors[1], NumRPC2otherErrors[2], NumRPC2otherErrors[3], NumRPC2otherErrors[4], NumRPC2otherErrors[5], NumRPC2otherErrors[6]);
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

