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

static char *rcsid = "$Header: /home/braam/src/coda-src/venus/RCS/adviceconn.h,v 1.1 1996/11/22 19:11:40 braam Exp braam $";
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

typedef enum { PCM=0, HWA=1, DM=2, R=3, RP=4, IASR=5, LC=6, WCM=7, VSC=8, DFE=9, NHoarding=10, NEmulating=11, NLogging=12, NResolving=13 } CallTypes;
#define NumCallTypes 10

#define NumRPCResultTypes 7

class adviceconn {
    friend class userent;
    friend void DisconnectedCacheMissEvent(vproc *vp, volent *v, fsobj *f, ViceFid *key, vuid_t vuid);
    friend void WeaklyConnectedCacheMissEvent(vproc *vp, fsobj *f, ViceFid *key, vuid_t vuid);
    friend int fsdb::Get(fsobj **f_addr, ViceFid *key, vuid_t vuid, int rights, char *comp, int *rcode);

    struct Lock userLock;  /* Lock indicates outstanding request to user */

    AdviceState state;
    char hostname[MAXHOSTNAMELEN];
    unsigned short port; 
    RPC2_Handle handle;  
    int pgid;                    /* Process group of the advice monitor */

    /* Information Requested */
    int InconsistentObjects;
    int ReadDisconnectedCacheMisses;
    int WeaklyConnectedCacheMisses;
    int DisconnectedCacheMisses;
    int VolumeTransitions;
    int ReconnectionQuestionnaires;
    int ReintegrationPendings;
    int HoardWalks;
    int ASRs;

    int stoplight_data;

    /* Statistics Counting */
    int AdviceNotEnabledCount;                  int initAdviceNotEnabledCount;
    int AdviceNotValidCount;                    int initAdviceNotValidCount;
    int AdviceOutstandingCount;                 int initAdviceOutstandingCount;
    int ASRnotAllowedCount;                     int initASRnotAllowedCount;
    int ASRintervalNotReachedCount;             int initASRintervalNotReachedCount;
    int VolumeNullCount;                        int initVolumeNullCount;
    int TotalAttempts;                          int initTotalAttempts;
    int NumSUCCESS[NumCallTypes];               int initNumSUCCESS[NumCallTypes];
    int NumCONNBUSY[NumCallTypes];              int initNumCONNBUSY[NumCallTypes];
    int NumFAIL[NumCallTypes];                  int initNumFAIL[NumCallTypes];
    int NumNOCONNECTION[NumCallTypes];          int initNumNOCONNECTION[NumCallTypes];
    int NumTIMEOUT[NumCallTypes];               int initNumTIMEOUT[NumCallTypes];
    int NumDEAD[NumCallTypes];                  int initNumDEAD[NumCallTypes];
    int NumRPC2otherErrors[NumCallTypes];       int initNumRPC2otherErrors[NumCallTypes];

    adviceconn();
    adviceconn(adviceconn&);     /* not supported! */
    operator=(adviceconn&);      /* not supported! */
    ~adviceconn();

  public:
    
    ReadDiscAdvice RequestReadDisconnectedCacheMissAdvice(char *pathname, int pid);
    void RequestHoardWalkAdvice(char *input, char *output);
    void RequestDisconnectedQuestionnaire(char *pathname, int pid, ViceFid *fid, long DiscoTime);
    void RequestReconnectionQuestionnaire(char *volname, VolumeId vid, int CMLcount, 
                                          long DiscoTime, long WalkTime, int NumberReboots, 
                                          int cacheHit, int cacheMiss, int unique_hits, 
                                          int unique_nonrefs);
    void NotifyHoarding(char *volname, VolumeId vid);
    void NotifyEmulating(char *volname, VolumeId vid);
    void NotifyLogging(char *volname, VolumeId vid);
    void NotifyResolving(char *volname, VolumeId vid);
    void RequestReintegratePending(char *volname, int flag);
    int RequestASRInvokation(char *pathname, vuid_t vuid);
    void InformLostConnection();
    WeaklyAdvice RequestWeaklyConnectedCacheMissAdvice(char *pathname, int pid, int expectedCost);

    int NewConnection(char *hostname, int port, int pgrp);
    int RegisterInterest(vuid_t vuid, long numEvents, InterestValuePair events[]);

    void CheckConnection();
    void ReturnConnection();
    void TearDownConnection();

    void CheckError(long rpc_code, CallTypes callType);
    void InvalidateConnection();
    void Reset();
    void ResetCounters();
    void SetState(AdviceState newState);

    void ObtainUserLock();
    void ReleaseUserLock();

    int IsAdviceValid(int bump);         /* T if adviceconn is in VALID state */
    int IsAdviceOutstanding(int bump);   /* T if outstanding request to user; F otherwise */
    int IsAdviceHandle(RPC2_Handle someHandle);
    int IsAdvicePGID(int calling_pgid)
        { return(calling_pgid == pgid); }

    int SendStoplightData()
        { return(stoplight_data); }
    int SetStoplightData()
        { stoplight_data = 1; }
    int UnsetStoplightData()
        { stoplight_data = 0; }

    int Getpgid();

    char *StateString();
    char *ReadDiscAdviceString(ReadDiscAdvice advice);
    char *WeaklyAdviceString(WeaklyAdvice advice);

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
