/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2018 Carnegie Mellon University
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
 * Specification of the Venus Communications subsystem.
 *
 */

#ifndef _VENUS_COMM_H_
#define _VENUS_COMM_H_ 1

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <sys/time.h>

#include <rpc2/rpc2.h>
#include <rpc2/se.h>

#ifdef __cplusplus
}
#endif

/* interfaces */
#include <callback.h>
#include <vice.h>

/* from util */
#include <olist.h>
#include <rec_olist.h>
#include <ohash.h>
#include <rec_ohash.h>

/* from vv */
#include <inconsist.h>

#include "refcounted.h"
#include "fso.h"
#include "venusrecov.h"
#include "vproc.h"

/* Forward declarations. */
class connent;
class conn_iterator;
class srvent;
class srv_iterator;
class RepOpCommCtxt;
class mgrpent;
class probeslave;

/* forward declarations for venusvol.h */
class volent;
class repvol;

/* Bogus forward declarations to placate C++! */

extern void ConnPrint();
extern void ConnPrint(FILE *);
extern void ConnPrint(int);
extern void ServerPrint();
extern void ServerPrint(FILE *);

/*  *****  Types  *****  */

/*
 *  ***  Non-Replication Communications Objects  ***
 *
 *  Connections:
 *
 *  Servers:
 *
*/
class connent {
    friend void CommInit();
    friend void Conn_Wait();
    friend void Conn_Signal();
    friend void PutConn(connent **);
    friend void ConnPrint(int);
    friend class conn_iterator;
    friend void DoProbes(int, struct in_addr *);
    friend class srvent;
    friend class mgrpent;
    friend class fsobj;
    friend int GetTime(long *, long *);
    friend class vdb;
    friend class volent;
    friend class repvol;
    friend class ClientModifyLog;
    friend class cmlent;

    /* The connection list. */
    static olist *conntab;
    static char conntab_sync;

    /* Transient members. */
    olink tblhandle;

    /* Static state; immutable after construction. */
    //struct in_addr Host;	/* Who to contact. */
    srvent *srv;
    uid_t uid; /* UID to validate with respect to. */
    unsigned authenticated : 1;

    /* Dynamic state; varies with each call. */
    unsigned dying : 1;
    unsigned int inuse;

    /* Constructors, destructors, and private utility routines. */
    connent(srvent *, uid_t, RPC2_Handle, int);
    connent(connent &) { abort(); } /* not supported! */
    int operator=(connent &)
    {
        abort();
        return (0);
    } /* not supported! */
    ~connent();

public:
#ifdef VENUSDEBUG
    static int allocs;
    static int deallocs;
#endif /* VENUSDEBUG */
    RPC2_Handle connid; /* RPC connid. */

    void Suicide(void);
    int CheckResult(int, VolumeId, int TranslateEINCOMP = 1);
    void GetRef(void) { inuse++; }
    void PutRef(void)
    {
        CODA_ASSERT(inuse);
        inuse--;
    }
    int RefCount(void) { return inuse; }

    void print() { print(stdout); }
    void print(FILE *fp)
    {
        fflush(fp);
        print(fileno(fp));
    }
    void print(int);
};

struct ConnKey {
    struct in_addr host;
    uid_t uid;
};

class conn_iterator : public olist_iterator {
    struct ConnKey *key;

public:
    conn_iterator(struct ConnKey * = (struct ConnKey *)0);
    ~conn_iterator();
    connent *operator()();
};

class srvent : private RefCountedObject {
    friend void CommInit();
    friend void Srvr_Wait();
    friend void Srvr_Signal();
    friend srvent *FindServer(struct in_addr *host);
    friend srvent *FindServerByCBCid(RPC2_Handle);
    friend srvent *GetServer(struct in_addr *host, RealmId realmid);
    friend void PutServer(srvent **);
    friend void ProbeServers(int);
    friend void ServerProbe(long *, long *);
    friend long HandleProbe(int, RPC2_Handle Handles[], long, long, ...);
    friend void CheckServerBW(long);
    friend void DownServers(char *, unsigned int *);
    friend void DownServers(int, struct in_addr *, char *, unsigned int *);
    friend void ServerPrint(FILE *);
    friend class srv_iterator;
    friend class connent;
    friend class mgrpent;
    friend long VENUS_CallBack(RPC2_Handle, ViceFid *);
    friend long VENUS_CallBackFetch(RPC2_Handle, ViceFid *, SE_Descriptor *);
    friend long VENUS_CallBackConnect(RPC2_Handle, RPC2_Integer, RPC2_Integer,
                                      RPC2_Integer, RPC2_Integer,
                                      RPC2_CountedBS *);
    friend long VENUS_RevokeWBPermit(RPC2_Handle RPCid, VolumeId Vid);
    friend int FailDisconnect(int, struct in_addr *);
    friend int FailReconnect(int, struct in_addr *);
    friend class userent;
    friend class vproc;
    friend class fsobj;
    friend class repvol;

    friend class Realm;
    friend connent *conn_iterator::operator()();
    friend class probeslave;
    friend void MultiBind(int, struct in_addr *, connent **);

    /* The server list. */
    static olist *srvtab;
    static char srvtab_sync;

    /* Transient members. */
    olink tblhandle;
    char *name;
    struct in_addr host;
    RealmId realmid;
    RPC2_Handle connid; /* The callback connid. */
    unsigned Xbinding : 1; /* 1 --> BINDING, 0 --> NOT_BINDING */
    unsigned probeme : 1; /* should ProbeD probe this server? */
    unsigned unused : 1;
    unsigned fetchpartial_support : 1;
    unsigned long bw; /* bandwidth estimate, Bytes/sec */
    struct timeval lastobs; /* time of most recent estimate */

    /* Constructors, destructors, and private utility routines. */
    srvent(struct in_addr *host, RealmId realmid);
    srvent(srvent &) { abort(); } /* not supported! */
    int operator=(srvent &)
    {
        abort();
        return (0);
    } /* not supported! */
    ~srvent();
    int Connect(RPC2_Handle *, int *, uid_t, int);

public:
#ifdef VENUSDEBUG
    static int allocs;
    static int deallocs;
#endif

    int GetConn(connent **c, uid_t uid, int force = 0);

    int GetStatistics(ViceStatistics *);

    long GetLiveness(struct timeval *);
    long GetBandwidth(unsigned long *);
    void Reset();

    void ServerError(int *);
    void ServerUp(RPC2_Handle);
    int ServerIsDown() { return (connid == 0); }
    int ServerIsUp() { return (connid != 0); }
    /* quasi-up != up */

    const char *Name(void) { return name; }

    void print() { print(stdout); }
    void print(FILE *fp);
};

class srv_iterator : public olist_iterator {
public:
    srv_iterator();
    srvent *operator()();
};

/* server probes */
enum ProbeSlaveTask
{
    ProbeUpServers,
    ProbeDownServers,
    BindToServer
};

class probeslave : public vproc {
    ProbeSlaveTask task;
    void *arg; /* optional */
    void *result; /* optional */
    char *sync; /* write TRUE here and signal when finished */

protected:
    virtual void main(void); /* entry point */

public:
    probeslave(ProbeSlaveTask, void *, void *, char *);
};

/*  *****  Variables  *****  */

extern int COPModes;
extern char myHostName[];
extern int rpc2_retries;
extern int rpc2_timeout;
extern int sftp_windowsize;
extern int sftp_sendahead;
extern int sftp_ackpoint;
extern int sftp_packetsize;
extern int rpc2_timeflag;
extern int mrpc2_timeflag;

/*  *****  Functions  *****  */

/* (ASYNCCOP1 || PIGGYCOP2) --> ASYNCCOP2 */
#define ASYNCCOP1 (COPModes & 1)
#define ASYNCCOP2 (COPModes & 2)
#define PIGGYCOP2 (COPModes & 4)

/* comm.c */
void CommInit();
void Conn_Wait();
void Conn_Signal();
void PutConn(connent **);
void Srvr_Wait();
void Srvr_Signal();
srvent *FindServer(struct in_addr *host);
srvent *FindServerByCBCid(RPC2_Handle);
srvent *GetServer(struct in_addr *host, RealmId realmid);
void PutServer(srvent **);
void ProbeServers(int);
void DoProbes(int, struct in_addr *);
void MultiBind(int, struct in_addr *, connent **);
void MultiProbe(int, RPC2_Handle *);
long HandleProbe(int, RPC2_Handle Handles[], long, long, ...);
void ServerProbe(long * = 0, long * = 0);
void DownServers(char *, unsigned int *);
void DownServers(int, struct in_addr *, char *, unsigned int *);
void CheckServerBW(long);
int FailDisconnect(int, struct in_addr *);
int FailReconnect(int, struct in_addr *);

/* comm_daemon.c */
extern void PROD_Init(void);
extern void ProbeDaemon(void);

/* comm synchronization */
struct CommQueueStruct {
    int count[LWP_MAX_PRIORITY + 1];
    char sync;
};

extern struct CommQueueStruct CommQueue;

/*
 * The CommQueue summarizes outstanding RPC traffic for all threads.
 * Threads servicing requests defer to higher priority threads before
 * using the network. Note that Venus cannot determine the location of a
 * network bottleneck. Therefore, it conservatively assumes that all
 * high priority requests are sources of interference. Synchronization
 * could be finer, currently all waiters are awakened instead of the
 * highest priority ones.
 */
#define COMM_YIELD 1
#define START_COMMSYNC()                                                       \
    {                                                                          \
        vproc *vp = VprocSelf();                                               \
        if (COMM_YIELD) {                                                      \
            int pri = LWP_MAX_PRIORITY;                                        \
            while (pri > vp->lwpri) {                                          \
                if (CommQueue.count[pri]) { /* anyone bigger than me? */       \
                    LOG(0, ("WAITING(CommQueue) pri = %d, for %d at pri %d\n", \
                            vp->lwpri, CommQueue.count[pri], pri));            \
                    START_TIMING();                                            \
                    VprocWait(&CommQueue.sync);                                \
                    END_TIMING();                                              \
                    LOG(0, ("WAIT OVER, elapsed = %3.1f\n", elapsed));         \
                    pri = LWP_MAX_PRIORITY;                                    \
                } else {                                                       \
                    pri--;                                                     \
                }                                                              \
            }                                                                  \
        }                                                                      \
        CommQueue.count[vp->lwpri]++;                                          \
        LOG(10, ("CommQueue: insert pri %d count = %d\n", vp->lwpri,           \
                 CommQueue.count[vp->lwpri]));                                 \
    }

#define END_COMMSYNC()                                               \
    {                                                                \
        vproc *vp = VprocSelf();                                     \
        CommQueue.count[vp->lwpri]--;                                \
        LOG(10, ("CommQueue: remove pri %d count = %d\n", vp->lwpri, \
                 CommQueue.count[vp->lwpri]));                       \
        VprocSignal(&CommQueue.sync);                                \
    }

/* comm statistics (move to venus.private.h?) */
#ifdef TIMING
#define START_COMMSTATS()            \
    RPCPktStatistics startCS, endCS; \
    GetCSS(&startCS);
#define END_COMMSTATS()            \
    if (LogLevel >= 1000) {        \
        GetCSS(&endCS);            \
        SubCSSs(&endCS, &startCS); \
    }
#define MULTI_START_MESSAGE(viceop)                                  \
    START_COMMSYNC();                                                \
    LOG(10, ("(Multi)%s: start\n", RPCOpStats.RPCOps[viceop].name)); \
    START_TIMING();                                                  \
    START_COMMSTATS();
#define UNI_START_MESSAGE(viceop)                             \
    START_COMMSYNC();                                         \
    LOG(10, ("%s: start\n", RPCOpStats.RPCOps[viceop].name)); \
    START_TIMING();                                           \
    START_COMMSTATS();

/* The LOG message at the end of this macro causes the sun4 to die.  This
 * is the quick hack.
 */
#if defined(sun4) || defined(sparc)
#define MULTI_END_MESSAGE(viceop)                                               \
    END_TIMING();                                                               \
    END_COMMSYNC();                                                             \
    END_COMMSTATS();                                                            \
    LOG(10, ("(Multi)%s: code = %d, elapsed = %3.1f\n",                         \
             RPCOpStats.RPCOps[viceop].name, code, elapsed));                   \
    LOG(1000,                                                                   \
        ("RPC2_SStats: Total = %d\n", endCS.RPC2_SStats_Multi.Multicasts));     \
    LOG(1000,                                                                   \
        ("SFTP_SStats: Starts = %d, Datas = %d, DataRetries = %d, Acks = %d\n", \
         endCS.SFTP_SStats_Multi.Starts, endCS.SFTP_SStats_Multi.Datas,         \
         endCS.SFTP_SStats_Multi.DataRetries, endCS.SFTP_SStats_Multi.Acks));   \
    LOG(1000,                                                                   \
        ("RPC2_RStats: Replies = %d, Busies = %d, Naks = %d, Bogus = %d\n",     \
         endCS.RPC2_RStats_Multi.Replies, endCS.RPC2_RStats_Multi.Busies,       \
         endCS.RPC2_RStats_Multi.Naks, endCS.RPC2_RStats_Multi.Bogus));         \
    LOG(1000, ("SFTP_RStats: Datas = %d, Acks = %d, Busies = %d\n",             \
               endCS.SFTP_RStats_Multi.Datas, endCS.SFTP_RStats_Multi.Acks,     \
               endCS.SFTP_RStats_Multi.Busies));
#else
#define MULTI_END_MESSAGE(viceop)                                               \
    END_TIMING();                                                               \
    END_COMMSYNC();                                                             \
    END_COMMSTATS();                                                            \
    LOG(10, ("(Multi)%s: code = %d, elapsed = %3.1f\n",                         \
             RPCOpStats.RPCOps[viceop].name, code, elapsed));                   \
    LOG(1000,                                                                   \
        ("RPC2_SStats: Total = %d\n", endCS.RPC2_SStats_Multi.Multicasts));     \
    LOG(1000,                                                                   \
        ("SFTP_SStats: Starts = %d, Datas = %d, DataRetries = %d, Acks = %d\n", \
         endCS.SFTP_SStats_Multi.Starts, endCS.SFTP_SStats_Multi.Datas,         \
         endCS.SFTP_SStats_Multi.DataRetries, endCS.SFTP_SStats_Multi.Acks));   \
    LOG(1000,                                                                   \
        ("RPC2_RStats: Replies = %d, Busies = %d, Naks = %d, Bogus = %d\n",     \
         endCS.RPC2_RStats_Multi.Replies, endCS.RPC2_RStats_Multi.Busies,       \
         endCS.RPC2_RStats_Multi.Naks, endCS.RPC2_RStats_Multi.Bogus));         \
    LOG(1000, ("SFTP_RStats: Datas = %d, Acks = %d, Busies = %d\n",             \
               endCS.SFTP_RStats_Multi.Datas, endCS.SFTP_RStats_Multi.Acks,     \
               endCS.SFTP_RStats_Multi.Busies));                                \
    if (elapsed > 1000.0)                                                       \
        LOG(0,                                                                  \
            ("*** Long Running (Multi)%s: code = %d, elapsed = %3.1f ***\n",    \
             RPCOpStats.RPCOps[viceop].name, code, elapsed));
#endif

#define UNI_END_MESSAGE(viceop)                                                 \
    END_TIMING();                                                               \
    END_COMMSYNC();                                                             \
    END_COMMSTATS();                                                            \
    LOG(10, ("%s: code = %d, elapsed = %3.1f\n",                                \
             RPCOpStats.RPCOps[viceop].name, code, elapsed));                   \
    LOG(1000, ("RPC2_SStats: Total = %d\n", endCS.RPC2_SStats_Uni.Total));      \
    LOG(1000,                                                                   \
        ("SFTP_SStats: Starts = %d, Datas = %d, DataRetries = %d, Acks = %d\n", \
         endCS.SFTP_SStats_Uni.Starts, endCS.SFTP_SStats_Uni.Datas,             \
         endCS.SFTP_SStats_Uni.DataRetries, endCS.SFTP_SStats_Uni.Acks));       \
    LOG(1000,                                                                   \
        ("RPC2_RStats: Replies = %d, Busies = %d, Naks = %d, Bogus = %d\n",     \
         endCS.RPC2_RStats_Uni.Replies, endCS.RPC2_RStats_Uni.Busies,           \
         endCS.RPC2_RStats_Uni.Naks, endCS.RPC2_RStats_Uni.Bogus));             \
    LOG(1000, ("SFTP_RStats: Datas = %d, Acks = %d, Busies = %d\n",             \
               endCS.SFTP_RStats_Uni.Datas, endCS.SFTP_RStats_Uni.Acks,         \
               endCS.SFTP_RStats_Uni.Busies));                                  \
    if (elapsed > 1000.0)                                                       \
        LOG(0, ("*** Long Running %s: code = %d, elapsed = %3.1f ***\n",        \
                RPCOpStats.RPCOps[viceop].name, code, elapsed));
#define MULTI_RECORD_STATS(viceop)                  \
    if (code < 0)                                   \
        RPCOpStats.RPCOps[viceop].Mrpc_retries++;   \
    else if (code > 0)                              \
        RPCOpStats.RPCOps[viceop].Mbad++;           \
    else {                                          \
        RPCOpStats.RPCOps[viceop].Mgood++;          \
        RPCOpStats.RPCOps[viceop].Mtime += elapsed; \
    }
#define UNI_RECORD_STATS(viceop)                   \
    if (code < 0)                                  \
        RPCOpStats.RPCOps[viceop].rpc_retries++;   \
    else if (code > 0)                             \
        RPCOpStats.RPCOps[viceop].bad++;           \
    else {                                         \
        RPCOpStats.RPCOps[viceop].good++;          \
        RPCOpStats.RPCOps[viceop].time += elapsed; \
    }
#else
#define MULTI_START_MESSAGE(viceop) \
    START_COMMSYNC();               \
    LOG(10, ("(Multi)%s: start\n", RPCOpStats.RPCOps[viceop].name));
#define UNI_START_MESSAGE(viceop) \
    START_COMMSYNC();             \
    LOG(10, ("%s: start\n", RPCOpStats.RPCOps[viceop].name));
#define MULTI_END_MESSAGE(viceop) \
    END_COMMSYNC();               \
    LOG(10, ("(Multi)%s: code = %d\n", RPCOpStats.RPCOps[viceop].name, code));
#define UNI_END_MESSAGE(viceop) \
    END_COMMSYNC();             \
    LOG(10, ("%s: code = %d\n", RPCOpStats.RPCOps[viceop].name, code));
#define MULTI_RECORD_STATS(viceop)                \
    if (code < 0)                                 \
        RPCOpStats.RPCOps[viceop].Mrpc_retries++; \
    else if (code > 0)                            \
        RPCOpStats.RPCOps[viceop].Mbad++;         \
    else                                          \
        RPCOpStats.RPCOps[viceop].Mgood++;
#define UNI_RECORD_STATS(viceop)                 \
    if (code < 0)                                \
        RPCOpStats.RPCOps[viceop].rpc_retries++; \
    else if (code > 0)                           \
        RPCOpStats.RPCOps[viceop].bad++;         \
    else                                         \
        RPCOpStats.RPCOps[viceop].good++;
#endif /* !TIMING */

#define VENUS_MAXBSLEN 1024 /* For use in ARG_MARSHALL_BS */

#endif /* _VENUS_COMM_H_ */
