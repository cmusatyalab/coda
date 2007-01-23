/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#ifndef _ADV_MONITOR_H_
#define _ADV_MONITOR_H_

#include "adv_skk.h"
#include "advice.h"
#include "fso.h"
#include <lwp/lock.h>
#include "venus.private.h"
#include "vproc.h" 

#ifdef __cplusplus
extern "C" {
#endif
#include <netdb.h>
#include <netinet/in.h>
#include <rpc2/rpc2.h>
#include <stdio.h>
#include <unistd.h>
#ifdef __cplusplus
}
#endif

#define MAXEVENTLEN 64
#define FALSE 0
#define TRUE 1
#define MAX_REPLACEMENTLOG_LINES 100

class adv_monitor {
    friend class userent;
    friend class fsobj;
    friend int fsdb::Get(fsobj **f_addr, VenusFid *key, uid_t uid, int rights, char *comp, VenusFid *parent, int *rcode, int GetInconstent);
    friend long S_ImminentDeath(RPC2_Handle _cid, RPC2_String hostname, RPC2_Integer port);
 private:

    struct Lock userLock;  /* Lock indicates outstanding request to user */
    ConnectionState cstate;
    char hostname[MAXHOSTNAMELEN];
    unsigned short port; 
    RPC2_Handle handle;  
    int pgid; /* Process group of the advice monitor */
    int InterestArray[MAXEVENTS]; /* Information Requested */

    /* Data Logs */
    FILE *programFILE;
    int numLines;
    char programLogName[MAXPATHLEN];
    FILE *replacementFILE;
    int numRLines;
    char replacementLogName[MAXPATHLEN];

  public:
    
    adv_monitor();
    ~adv_monitor();

    /* RPC calls */
    void TokensAcquired(int);
    void TokensExpired();
    void ServerAccessible(char *);
    void ServerInaccessible(char *);
    void ServerConnectionWeak(char *);
    void ServerConnectionStrong(char *);
    void ServerBandwidthEstimate(char *, long);
    int RequestASRInvokation(repvol *vol, char *pathname, uid_t uid);

    /* Log stuff */
    void InitializeReplacementLog(uid_t uid);
    void SwapReplacementLog();
    void LogReplacement(char *path, int status, int data);

    /* Connection stuff */
    int NewConnection(char *hostname, int port, int pgrp);
    int Spike(int); /* ping/kill sidekick, returns whether sidekick is alive */
    void CheckConnection();
    void ReturnConnection();
    void DestroyConnection();

    int RegisterInterest(uid_t uid, long numEvents, InterestValuePair events[]);

    void CheckError(long rpc_code, InterestID callType);

    void Reset(int);
    void Reset() { Reset(0); }

    void Print();
    void Print(FILE *);
    void Print(int);

    int ConnValid() { return (cstate == Valid); }
    int AdviceOutstanding() { return (CheckLock(&userLock) == -1); }
    int skkPgid(int x) { return(x == pgid); }
};

extern int ASRresult;

extern adv_monitor adv_mon;

#endif /* _ADV_MONITOR_H_ */
