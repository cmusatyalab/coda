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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/venus/adviceconn.h,v 4.2 97/12/16 16:08:22 braam Exp $";
#endif /*_BLURB_*/



/*
 *
 * Specification of the Venus Advice Monitor server.
 *
 */

#ifndef _ADVICECONN_H_
#define _ADVICECONN_H_

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <rpc2.h>

#ifdef __cplusplus
}
#endif __cplusplus


#include "venus.private.h"
#include "vproc.h" 
#include "fso.h"
#include "lock.h"
#include "advice.h"
#include "adsrv.h"
#include "admon.h"


typedef struct {
  int requested;
  int advicenotvalid;
  int rpc_initiated;
  int rpc_success;
  int rpc_connbusy;
  int rpc_fail;
  int rpc_noconnection;
  int rpc_timeout;
  int rpc_dead;
  int rpc_othererrors;
} AdviceCallCounter;


#define NumRPCResultTypes 7

class adviceconn {
    friend class userent;
    friend class fsobj;
    friend int fsdb::Get(fsobj **f_addr, ViceFid *key, vuid_t vuid, int rights, char *comp, int *rcode);

    struct Lock userLock;  /* Lock indicates outstanding request to user */

    AdviceState state;
    char hostname[MAXHOSTNAMELEN];
    unsigned short port; 
    RPC2_Handle handle;  
    int pgid;                    /* Process group of the advice monitor */

    /* Information Requested */
    int InterestArray[MAXEVENTS];

    /* Data Logs */
    FILE *programFILE;
    int numLines;
    char programLogName[MAXPATHLEN];

    FILE *replacementFILE;
    int numRLines;
    char replacementLogName[MAXPATHLEN];


    int stoplight_data;

    /* Statistics Counting */
    int AdviceNotEnabledCount;
    int AdviceNotValidCount;
    int AdviceOutstandingCount;
    int ASRnotAllowedCount;
    int ASRintervalNotReachedCount;
    int VolumeNullCount;
    int TotalAttempts;
    AdviceCallCounter CurrentValues[MAXEVENTS];

    adviceconn();
    adviceconn(adviceconn&);     /* not supported! */
    operator=(adviceconn&);      /* not supported! */
    ~adviceconn();

  public:
    
    void TokensAcquired(int);
    void TokensExpired();
    void ServerAccessible(char *);
    void ServerInaccessible(char *);
    void ServerConnectionWeak(char *);
    void ServerConnectionStrong(char *);
    void ServerBandwidthEstimate(char *, long);
    void HoardWalkBegin();
    void HoardWalkStatus(int);
    void HoardWalkEnd();
    void HoardWalkPeriodicOn();
    void HoardWalkPeriodicOff();
    
    CacheMissAdvice RequestReadDisconnectedCacheMissAdvice(ViceFid *fid, char *pathname, int pid);
    void RequestHoardWalkAdvice(char *input, char *output);
    void RequestDisconnectedQuestionnaire(ViceFid *fid, char *pathname, int pid, long DiscoTime);
    void RequestReconnectionQuestionnaire(char *volname, VolumeId vid, int CMLcount, 
                                          long DiscoTime, long WalkTime, int NumberReboots, 
                                          int cacheHit, int cacheMiss, int unique_hits, 
                                          int unique_nonrefs);
    void NotifyHoarding(char *volname, VolumeId vid);
    void NotifyEmulating(char *volname, VolumeId vid);
    void NotifyLogging(char *volname, VolumeId vid);
    void NotifyResolving(char *volname, VolumeId vid);

    void NotifyReintegrationPending(char *volname);
    void NotifyReintegrationEnabled(char *volname);
    void NotifyReintegrationActive(char *volname);
    void NotifyReintegrationCompleted(char *volname);

    void NotifyObjectInConflict(char *pathname, ViceFid *fid);
    void NotifyObjectConsistent(char *pathname, ViceFid *fid);
    void NotifyTaskAvailability(int count, TallyInfo *tallyInfo);
    void NotifyTaskUnavailable(int priority, int size);
    void NotifyProgramAccessLogAvailable(char *pathname);
    void NotifyReplacementLogAvailable(char *pathname);
    
    int RequestASRInvokation(char *pathname, vuid_t vuid);

    void InformLostConnection();
    CacheMissAdvice RequestWeaklyConnectedCacheMissAdvice(ViceFid *fid, char *pathname, int pid, int length, int estimatedBandwidth, char *Vfilename);

    int NewConnection(char *hostname, int port, int pgrp);
    int RegisterInterest(vuid_t vuid, long numEvents, InterestValuePair events[]);
    int OutputUsageStatistics(vuid_t vuid, char *pathname, int discosSinceLastUse, int percentDiscosUsed, int totalDiscosUsed);

    void InitializeProgramLog(vuid_t vuid);
    void SwapProgramLog();
    void LogProgramAccess(int pid, int pgid, ViceFid *fid);

    void InitializeReplacementLog(vuid_t vuid);
    void SwapReplacementLog();
    void LogReplacement(char *path, int status, int data);

    void CheckConnection();
    void ReturnConnection();
    void TearDownConnection();

    void IncrNotValid(InterestID callType) 
        { CurrentValues[(int)callType].advicenotvalid++; AdviceNotValidCount++; }
    void IncrRequested(InterestID callType)
        { CurrentValues[(int)callType].requested++; }
    void IncrRPCInitiated(InterestID callType)
        { CurrentValues[(int)callType].rpc_initiated++; }

    void CheckError(long rpc_code, InterestID callType);
    void InvalidateConnection();
    void Reset();
    void ResetCounters();
    void SetState(AdviceState newState);

    void ObtainUserLock();
    void ReleaseUserLock();

    int IsAdviceValid(InterestID, int bump);         /* T if adviceconn is in VALID state */
    int IsAdviceOutstanding(int bump);   /* T if outstanding request to user; F otherwise */
    int IsAdviceHandle(RPC2_Handle someHandle);
    int IsAdvicePGID(int calling_pgid)
        { return(calling_pgid == pgid); }
    int IsInterested(InterestID interest)
        { return(InterestArray[(int)interest]); }

    int SendStoplightData()
        { return(stoplight_data); }
    void SetStoplightData()
        { stoplight_data = 1; }
    void UnsetStoplightData()
        { stoplight_data = 0; }

    int Getpgid();

    char *StateString();
    char *CacheMissAdviceToString(CacheMissAdvice advice);

    int GetSuccesses(InterestID interest);
    int GetFailures(InterestID interest);
    int GetSUCCESS(InterestID interest) 
      { return(CurrentValues[(int)interest].rpc_success); }
    int GetCONNBUSY(InterestID interest) 
      { return(CurrentValues[(int)interest].rpc_connbusy); }
    int GetFAIL(InterestID interest) 
      { return(CurrentValues[(int)interest].rpc_fail); }
    int GetNOCONNECTION(InterestID interest) 
      { return(CurrentValues[(int)interest].rpc_noconnection); }
    int GetTIMEOUT(InterestID interest) 
      { return(CurrentValues[(int)interest].rpc_timeout); }
    int GetDEAD(InterestID interest) 
      { return(CurrentValues[(int)interest].rpc_dead); }
    int GetOTHER(InterestID interest) 
      { return(CurrentValues[(int)interest].rpc_othererrors); }
    void GetStatistics(AdviceCalls *calls, AdviceResults *results, AdviceStatistics *stats);

    /* Error events */
    void AdviceNotEnabled()
	{ AdviceNotEnabledCount++; }
    void AdviceNotValid()
	{ AdviceNotValidCount++; }
    void AdviceOutstanding() 
	{ AdviceOutstandingCount++; }
    void ASRnotAllowed()
	{ ASRnotAllowedCount++; }
    void ASRintervalNotReached()
	{ ASRintervalNotReachedCount++; }
    void VolumeNull()
	{ VolumeNullCount++; }

    void Print();
    void Print(FILE *);
    void Print(int);

    void PrintState();
    void PrintState(FILE *);
    void PrintState(int);
};

#endif _ADVICECONN_H_
