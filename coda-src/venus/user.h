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
#include <admon.h>
#include <adsrv.h>

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
    int told_you_so;
    SecretToken secret;
    ClearToken clear;
    int waitforever : 1;

    /* Advice stuff */
    adviceconn admon;
    long DemandHoardWalkTime;

    /* Constructors, destructors, and private utility routines. */
    userent(vuid_t);
    userent(userent&);	    /* not supported! */
    int operator=(userent&);    /* not supported! */
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

    vuid_t GetUid() 
        { return(uid); }
    CacheMissAdvice RequestReadDisconnectedCacheMissAdvice(ViceFid *fid, char *pathname, int pid)
        { return(admon.RequestReadDisconnectedCacheMissAdvice(fid, pathname, pid)); }

    void RequestHoardWalkAdvice(char *input, char *output)
        { admon.RequestHoardWalkAdvice(input, output); }

    void RequestDisconnectedQuestionnaire(ViceFid *fid, char *pathname, int pid, long DiscoTime)
        { admon.RequestDisconnectedQuestionnaire(fid, pathname, pid, DiscoTime); }

    void RequestReconnectionQuestionnaire(char *volname, VolumeId vid, int CMLcount, 
                                          long DiscoTime, long WalkTime, int NumberReboots, 
                                          int cacheHit, int cacheMiss, int unique_hits, 
                                          int unique_nonrefs)
        { admon.RequestReconnectionQuestionnaire(volname, vid, CMLcount, DiscoTime, WalkTime, NumberReboots, cacheHit, cacheMiss, unique_hits, unique_nonrefs); }

  /*
    void NotifyHoarding(char *volname, VolumeId vid)
        { admon.NotifyHoarding(volname,vid); }

    void NotifyEmulating(char *volname, VolumeId vid)
        { admon.NotifyEmulating(volname,vid); }

    void NotifyLogging(char *volname, VolumeId vid)
        { admon.NotifyLogging(volname,vid); }

    void NotifyResolving(char *volname, VolumeId vid)
        { admon.NotifyResolving(volname,vid); }
	*/
    void ServerAccessible(char *name) 
        { admon.ServerAccessible(name); }
    void ServerInaccessible(char *name) 
        { admon.ServerInaccessible(name); }
    void ServerConnectionWeak(char *name) 
        { admon.ServerConnectionWeak(name); }
    void ServerConnectionStrong(char *name) 
        { admon.ServerConnectionStrong(name); }
    void ServerBandwidthEstimate(char *name, long bandwidth) 
        { admon.ServerBandwidthEstimate(name, bandwidth); }
    void HoardWalkBegin()
        { admon.HoardWalkBegin(); }
    void HoardWalkStatus(int percentDone)
        { admon.HoardWalkStatus(percentDone); }
    void HoardWalkEnd()
        { admon.HoardWalkEnd(); }
    void HoardWalkPeriodicOn()
        { admon.HoardWalkPeriodicOn(); }
    void HoardWalkPeriodicOff()
        { admon.HoardWalkPeriodicOff(); }

    void NotifyReintegrationPending(char *volname)
        { admon.NotifyReintegrationPending(volname); }
    void NotifyReintegrationEnabled(char *volname)
        { admon.NotifyReintegrationEnabled(volname); }
    void NotifyReintegrationActive(char *volname)
        { admon.NotifyReintegrationActive(volname); }
    void NotifyReintegrationCompleted(char *volname)
        { admon.NotifyReintegrationCompleted(volname); }

    void NotifyObjectInConflict(char *pathname, ViceFid *fid)
        { admon.NotifyObjectInConflict(pathname, fid); }
    void NotifyObjectConsistent(char *pathname, ViceFid *fid)
        { admon.NotifyObjectConsistent(pathname, fid); }
    void NotifyTaskAvailability(int i, TallyInfo *tallyInfo) 
        { admon.NotifyTaskAvailability(i, tallyInfo); }
    void NotifyTaskUnavailable(int priority, int size)
        { admon.NotifyTaskUnavailable(priority, size); }
    int RequestASRInvokation(char *pathname, vuid_t vuid)
        { return(admon.RequestASRInvokation(pathname, vuid)); }

    CacheMissAdvice RequestWeaklyConnectedCacheMissAdvice(ViceFid *fid, char *pathname, int pid, int length, int estimatedBandwidth, char *Vfilename)
        { return(admon.RequestWeaklyConnectedCacheMissAdvice(fid, pathname, pid, length, estimatedBandwidth, Vfilename)); }

    int NewConnection(char *hostname, int port, int pgrp)
        { return(admon.NewConnection(hostname, port, pgrp)); }

    int RegisterInterest(vuid_t vuid, long numEvents, InterestValuePair events[])
        { return(admon.RegisterInterest(vuid, numEvents, events)); }

    void InitializeProgramLog(vuid_t vuid) 
        { admon.InitializeProgramLog(vuid); }
    void SwapProgramLog()
        { admon.SwapProgramLog(); }
    void LogProgramAccess(int pid, int pgid, ViceFid *fid) 
        { admon.LogProgramAccess(pid, pgid, fid); }

    void InitializeReplacementLog(vuid_t vuid) 
        { admon.InitializeReplacementLog(vuid); }
    void SwapReplacementLog()
        { admon.SwapReplacementLog(); }
    void LogReplacement(char *path, int status, int data)
        { admon.LogReplacement(path, status, data); }

    int OutputUsageStatistics(vuid_t vuid, char *pathname, int discosSinceLastUse, int percentDiscosUsed, int totalDiscosUsed)
        { return(admon.OutputUsageStatistics(vuid, pathname, discosSinceLastUse, percentDiscosUsed, totalDiscosUsed)); }

    void GetStatistics(AdviceCalls *calls, AdviceResults *results, AdviceStatistics *stats)
        { admon.GetStatistics(calls, results, stats); }

    int IsAdviceValid(InterestID interest, int bump)
        { return(admon.IsAdviceValid(interest, bump)); }
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
extern int ConsoleUser(vuid_t user);


/* user_daemon.c */
extern void USERD_Init(void);
extern void UserDaemon(void);

#endif	not _VENUS_USER_H_
