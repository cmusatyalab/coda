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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/venus/user.h,v 4.1 1997/01/08 21:51:36 rvb Exp $";
#endif /*_BLURB_*/




/*
 *
 * Specification of the Venus User abstraction.
 *
 */


#ifndef _VENUS_USER_H_
#define _VENUS_USER_H_ 1

/* Forward declarations. */
class userent;
class user_iterator;

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>

#include <rpc2.h>

#ifdef __cplusplus
}
#endif __cplusplus

/* interfaces */
#include <auth2.h>

/* from util */
#include <olist.h>

/* from venus */
#include "advice.h"
#include "adviceconn.h"
#include "comm.h"
#include "venus.private.h"


/*  *****  Types  *****  */

class userent {
  friend void UserInit();
  friend userent *FindUser(vuid_t);
  friend userent *FindUserByAdviceHandle(RPC2_Handle handle);
  friend void GetUser(userent **, vuid_t);
  friend void PutUser(userent **);
  friend void UserPrint(int);
  friend class user_iterator;
  friend class adviceserver;
  friend class fsdb;

    /* The user list. */
    static olist *usertab;

    /* Transient members. */
    olink tblhandle;
    vuid_t uid;
    int tokensvalid;
    SecretToken secret;
    ClearToken clear;
    unsigned int waitforever : 1;

    /* Advice stuff */
    adviceconn admon;
    long DemandHoardWalkTime;

    /* Constructors, destructors, and private utility routines. */
    userent(vuid_t);
    userent(userent&);	    /* not supported! */
    operator=(userent&);    /* not supported! */
    ~userent();

  public:
    long SetTokens(SecretToken *, ClearToken *);
    long GetTokens(SecretToken *, ClearToken *);
    int TokensValid();
    void CheckTokenExpiry();
    void Invalidate();
    void Reset();
    int Connect(RPC2_Handle *, int *, unsigned long);
    int GetWaitForever();
    void SetWaitForever(int);

    int GetUid() 
        { return(uid); }
    ReadDiscAdvice RequestReadDisconnectedCacheMissAdvice(char *pathname, int pid)
        { return(admon.RequestReadDisconnectedCacheMissAdvice(pathname, pid)); }
    void RequestHoardWalkAdvice(char *input, char *output)
        { admon.RequestHoardWalkAdvice(input, output); }
    void RequestDisconnectedQuestionnaire(char *pathname, int pid, ViceFid *fid, long DiscoTime)
        { admon.RequestDisconnectedQuestionnaire(pathname, pid, fid, DiscoTime); }
    void RequestReconnectionQuestionnaire(char *volname, VolumeId vid, int CMLcount, 
                                          long DiscoTime, long WalkTime, int NumberReboots, 
                                          int cacheHit, int cacheMiss, int unique_hits, 
                                          int unique_nonrefs)
        { admon.RequestReconnectionQuestionnaire(volname, vid, CMLcount, DiscoTime, WalkTime, NumberReboots, cacheHit, cacheMiss, unique_hits, unique_nonrefs); }
    void NotifyHoarding(char *volname, VolumeId vid)
        { admon.NotifyHoarding(volname,vid); }
    void NotifyEmulating(char *volname, VolumeId vid)
        { admon.NotifyEmulating(volname,vid); }
    void NotifyLogging(char *volname, VolumeId vid)
        { admon.NotifyLogging(volname,vid); }
    void NotifyResolving(char *volname, VolumeId vid)
        { admon.NotifyResolving(volname,vid); }
    void RequestReintegratePending(char *volname, int flag)
        { admon.RequestReintegratePending(volname, flag); }
    int RequestASRInvokation(char *pathname, vuid_t vuid)
        { return(admon.RequestASRInvokation(pathname, vuid)); }
    WeaklyAdvice RequestWeaklyConnectedCacheMissAdvice(char *pathname, int pid, int expectedCost)
        { return(admon.RequestWeaklyConnectedCacheMissAdvice(pathname, pid, expectedCost)); }
    int NewConnection(char *hostname, int port, int pgrp)
        { return(admon.NewConnection(hostname, port, pgrp)); }
    int RegisterInterest(vuid_t vuid, long numEvents, InterestValuePair events[])
        { return(admon.RegisterInterest(vuid, numEvents, events)); }
    void GetStatistics(AdviceCalls *calls, AdviceResults *results, AdviceStatistics *stats)
        { admon.GetStatistics(calls, results, stats); }

    int IsAdviceValid(int bump)
        { return(admon.IsAdviceValid(bump)); }
    void AdviceNotEnabled() 
        { admon.AdviceNotEnabled(); }
    void ASRnotAllowed()
        { admon.ASRnotAllowed(); }
    void ASRintervalNotReached() 
        { admon.ASRintervalNotReached(); }
    void VolumeNull() 
        { admon.VolumeNull(); }
    int IsAdvicePGID(int vp_pgid) 
        { return(admon.IsAdvicePGID(vp_pgid)); }
    void SetAdviceState(AdviceState newState) 
        { admon.SetState(newState); }



    void print();
    void print(FILE *);
    void print(int);
};


class user_iterator : public olist_iterator {

  public:
    user_iterator();
    userent *operator()();
};


/*  *****  Functions/Routines  *****  */

/* user.c */
extern void UserInit();
extern userent *FindUser(vuid_t);
extern void GetUser(userent **, vuid_t);
extern void PutUser(userent **);
extern void UserPrint();
extern void UserPrint(FILE *);
extern void UserPrint(int);
extern int AuthorizedUser(vuid_t);
extern vuid_t ConsoleUser();


/* user_daemon.c */
extern void USERD_Init();
extern void UserDaemon();

#endif	not _VENUS_USER_H_
