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
 * Specification of the Venus Communications subsystem.
 *
 */


#ifndef	_VENUS_COMM_H_
#define	_VENUS_COMM_H_	1


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <sys/time.h>

#include <rpc2.h>
#include <se.h>

#ifdef __cplusplus
}
#endif __cplusplus

/* interfaces */
#include <callback.h>
#include <vice.h>
#include <adsrv.h>

/* from util */
#include <olist.h>
#include <rec_olist.h>
#include <ohash.h>
#include <rec_ohash.h>

/* from vv */
#include <inconsist.h>

#include "fso.h"
#include "venusrecov.h"
#include "venus.private.h"
#include "venusvol.h"
#include "vproc.h"


/* Forward declarations. */
class connent;
class conn_iterator;
class srvent;
class srv_iterator;
class RepOpCommCtxt;
class mgrpent;
class mgrp_iterator;
class vsgdb;
class vsgent;
class vsg_iterator;

/* Bogus forward declarations to placate C++! */

extern void ConnPrint();
extern void ConnPrint(FILE *);
extern void ConnPrint(int);
extern void ServerPrint();
extern void ServerPrint(FILE *);
extern void ServerPrint(int);
extern void MgrpPrint();
extern void MgrpPrint(FILE *);
extern void MgrpPrint(int);

extern unsigned long WCThresh;



/*  *****  Constants  *****  */

#define	VSGDB	(rvg->recov_VSGDB)
const int VSGDB_MagicNumber = 9392989;
const int VSGDB_NBUCKETS = 32;
const int VSGENT_MagicNumber = 2777477;
const int VSGMaxFreeEntries = 8;

const int DFLT_RT = 4;			    /* rpc2 retries */
const int UNSET_RT = -1;
const int DFLT_TO = 15;			    /* rpc2 timeout */
const int UNSET_TO = -1;
const int DFLT_WS = 32;			    /* sftp window size */
const int UNSET_WS = -1;
const int DFLT_SA = 8;			    /* sftp send ahead */
const int UNSET_SA = -1;
const int DFLT_AP = 8;			    /* sftp ack point */
const int UNSET_AP = -1;
const int DFLT_PS = (1024 /*body*/ + 60 /*header*/); /* sftp packet size */
const int UNSET_PS = -1;
const int UNSET_ST = -1;                    /* do we time rpcs? */
const int UNSET_MT = -1;                    /* do we time mrpcs? */
#ifdef TIMING
const int DFLT_ST = 1;
const int DFLT_MT = 1;
#else
const int DFLT_ST = 0;
const int DFLT_MT = 0;
#endif
const unsigned int INIT_BW   = 10000000;
const unsigned int UNSET_WCT = 0;
const unsigned int DFLT_WCT  = 50000;
const int UNSET_WCS = -1;
const int DFLT_WCS  = 1800;		    /* 30 minutes */


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
  friend int GetConn(connent **, unsigned long, vuid_t, int);
  friend void PutConn(connent **);
  friend void ConnPrint(int);
  friend class conn_iterator;
  friend void DoProbes(int, unsigned long *);
  friend class srvent;
  friend class mgrpent;
  friend class fsobj;
  friend int GetTime(long *, long *);
  friend int GetRootVolume();
  friend class vdb;
  friend class volent;
  friend class ClientModifyLog;
  friend class cmlent;

    /* The connection list. */
    static olist *conntab;
    static char conntab_sync;

    /* Transient members. */
    olink tblhandle;

    /* Static state; immutable after construction. */
    unsigned long Host;		/* Who to contact. */
    vuid_t uid;			/* UID to validate with respect to. */
    RPC2_Handle connid;		/* RPC connid. */
    unsigned authenticated : 1;

    /* Dynamic state; varies with each call. */
    unsigned inuse : 1;
    unsigned dying : 1;

    /* Constructors, destructors, and private utility routines. */
    connent(unsigned long, vuid_t, RPC2_Handle, int);
    connent(connent&) { abort(); }	/* not supported! */
    operator=(connent&) { abort(); return(0); }	/* not supported! */
    ~connent();

  public:
#ifdef	VENUSDEBUG
    static int allocs;
    static int deallocs;
#endif	VENUSDEBUG

    int	Suicide(int);		/* 1 --> dead, 0 --> dying */
    int CheckResult(int, VolumeId);

    void print() { print(stdout); }
    void print(FILE *fp) { fflush(fp); print(fileno(fp)); }
    void print(int);
};


const unsigned long ALL_HOSTS = 0xFFFFFFFF;

struct ConnKey {
    unsigned long host;
    vuid_t vuid;
};

class conn_iterator : public olist_iterator {
    struct ConnKey *key;

  public:
    conn_iterator(struct ConnKey * =(struct ConnKey *)0);
    connent *operator()();
};





class srvent {
  friend void CommInit();
  friend void Srvr_Wait();
  friend void Srvr_Signal();
  friend srvent *FindServer(unsigned long);
  friend srvent *FindServerByCBCid(RPC2_Handle);
  friend void GetServer(srvent **, unsigned long);
  friend void PutServer(srvent **);
  friend void ProbeServers(int);
  friend void ServerProbe(long *, long *);
  friend long HandleProbe(int, RPC2_Handle *, long, long);
  friend void CheckServerBW(long);
  friend void DownServers(char *, unsigned int *);
  friend void DownServers(int, unsigned long *, char *, unsigned int *);
  friend void ServerPrint(int);
  friend long S_GetServerInformation(RPC2_Handle, RPC2_Integer, RPC2_Integer *, ServerEnt *);
  friend class srv_iterator;
  friend class connent;
  friend class mgrpent;
  friend class vsgdb;
  friend int GetConn(connent **, unsigned long, vuid_t, int);
  friend int GetAdmConn(connent **);
  friend long CallBack(RPC2_Handle, ViceFid *);
  friend long CallBackFetch(RPC2_Handle, ViceFid *, SE_Descriptor *);
  friend long CallBackConnect(RPC2_Handle, RPC2_Integer, RPC2_Integer, RPC2_Integer, RPC2_Integer, RPC2_CountedBS *);
  friend int FailDisconnect(int, unsigned long *);
  friend int FailReconnect(int, unsigned long *);
  friend int FailSlow(unsigned *);
  friend class userent;
  friend class vproc;
  friend class fsobj;

    /* The server list. */
    static olist *srvtab;
    static char srvtab_sync;

    /* Transient members. */
    olink tblhandle;
    char *name;
    unsigned long host;
    RPC2_Handle	connid;		/* The callback connid. */
    int	EventCounter;		/* incremented on every Up/Down event */
    unsigned Xbinding : 1;	/* 1 --> BINDING, 0 --> NOT_BINDING */
    unsigned probeme : 1;	/* should ProbeD probe this server? */
    unsigned userbw : 1;	/* is current BW set by the user? */
    unsigned long bw;		/* bandwidth estimate, Bytes/sec */
    unsigned long bwvar;	/* variance of the bandwidth estimate */
    struct timeval lastobs;	/* time of most recent estimate */
  
    /* Constructors, destructors, and private utility routines. */
    srvent(unsigned long);
    srvent(srvent&) { abort(); }	/* not supported! */
    operator=(srvent&) { abort(); return(0); }	/* not supported! */
    ~srvent();

  public:
#ifdef	VENUSDEBUG
    static int allocs;
    static int deallocs;
#endif	VENUSDEBUG

    int Connect(RPC2_Handle *, int *, vuid_t, int);
    int GetStatistics(ViceStatistics *);

    int GetEventCounter() { return(EventCounter); }
    long GetLiveness(struct timeval *);
    long GetBandwidth(unsigned long *);
    long InitBandwidth(unsigned long);
    void Reset();

    void ServerError(int *);
    void ServerUp(RPC2_Handle);
    int	ServerIsDown() { return(connid == 0); }
    int ServerIsUp() { return(connid != 0); }
    int ServerIsWeak() { return(connid > 0 && bw <= WCThresh); }
                         /* quasi-up != up */

    void print() { print(stdout); }
    void print(FILE *fp) { fflush(fp); print(fileno(fp)); }
    void print(int);
};


class srv_iterator : public olist_iterator {

  public:
    srv_iterator();
    srvent *operator()();
};


/* server probes */
enum ProbeSlaveTask { ProbeUpServers, ProbeDownServers, BindToServer };

class probeslave : public vproc {
    ProbeSlaveTask task;
    void *arg;			/* optional */
    void *result;		/* optional */
    char *sync;			/* write TRUE here and signal when finished */

  public:
    probeslave::probeslave(ProbeSlaveTask, void *, void *, char *);
    void main(void *);
};


/*
 *  ***  Replication Communication Objects  ***
 *
 *  MultiGroups:
 *	RepOpCommCtxts:
 * 
 *  VSGs:
 *
*/

#define	BENIGNERROR(code)   ((code) == ENOSPC ||\
			     (code) == EDQUOT ||\
			     (code) == EIO ||\
			     (code) == EACCES ||\
			     (code) == EWOULDBLOCK)

class RepOpCommCtxt {
  friend class mgrpent;
  friend int GetMgrp(mgrpent **, unsigned long, vuid_t);
  friend void PutMgrp(mgrpent **);
  friend class fsobj;
  friend class volent;
  friend class ClientModifyLog;
  friend class cmlent;

    RPC2_Integer HowMany;
    RPC2_Handle handles[VSG_MEMBERS];
    unsigned long hosts[VSG_MEMBERS];
    RPC2_Integer retcodes[VSG_MEMBERS];
    unsigned long primaryhost;
    RPC2_Multicast *MIp;
    unsigned dying[VSG_MEMBERS];

  public:
    RepOpCommCtxt();
    RepOpCommCtxt(RepOpCommCtxt&) { abort(); }  /* not supported! */
    operator=(RepOpCommCtxt&) { abort(); return(0); }	    /* not supported! */
    ~RepOpCommCtxt() {}

    void print() { print(stdout); }
    void print(FILE *fp) { fflush(fp); print(fileno(fp)); }
    void print(int fd) { fdprint(fd, "%#08x : HowMany = %d\n", (long)this, HowMany); }
};


class mgrpent {
  friend void CommInit();
  friend void Mgrp_Wait();
  friend void Mgrp_Signal();
  friend int GetMgrp(mgrpent **, unsigned long, vuid_t);
  friend void PutMgrp(mgrpent **);
  friend void MgrpPrint(int);
  friend class mgrp_iterator;
  friend class fsobj;
  friend class volent;
  friend class ClientModifyLog;
  friend class cmlent;

    /* The mgrp list. */
    static olist *mgrptab;
    static char mgrptab_sync;

    /* Transient members. */
    olink tblhandle;

    /* Static state; immutable after construction. */
    unsigned long VSGAddr;		/* should be a reference to vsgent! */
    vuid_t uid;				/* UID to validate with respect to. */
    RPC2_Multicast McastInfo;
    unsigned long Hosts[VSG_MEMBERS];	/* All VSG hosts in canonical order. */
    unsigned nhosts;                    /* how many there are, really.       */
    unsigned authenticated : 1;

    /* Dynamic state; varies with each call. */
    unsigned inuse : 1;
    unsigned dying : 1;
    RepOpCommCtxt rocc;

    /* Constructors, destructors, and private utility routines. */
    mgrpent(unsigned long, vuid_t, RPC2_Handle, int);
    mgrpent(mgrpent&) { abort(); }	/* not supported! */
    operator=(mgrpent&) { abort(); return(0); }	/* not supported! */
    ~mgrpent();

  public:
#ifdef	VENUSDEBUG
    static int allocs;
    static int deallocs;
#endif	VENUSDEBUG

    int	Suicide(int);		/* 1 --> dead, 0 --> dying */
    void CheckResult();
    int CheckNonMutating(int);
    int CheckCOP1(int, vv_t *, int =1);
    int CheckReintegrate(int, vv_t *);
    int RVVCheck(vv_t **, int);
    int DHCheck(vv_t **, int, int *,  int =0);
    int PickDH(vv_t **RVVs);

    int GetHostSet();
    int CreateMember(unsigned long);
    void PutHostSet();
    void KillMember(unsigned long, int);
    unsigned long GetPrimaryHost(int * =0);

    void print() { print(stdout); }
    void print(FILE *fp) { fflush(fp); print(fileno(fp)); }
    void print(int fd) {     
	    fdprint(fd, "%#08x : vsgaddr = %#08x, nhosts = %d, uid = %d, mid = %d, auth = %d, inuse = %d, dying = %d\n",
		    (long)this, VSGAddr, nhosts, uid, McastInfo.Mgroup, authenticated, inuse, dying);
    }
};


const unsigned long ALL_VSGS = 0xFFFFFFFF;

struct MgrpKey {
    unsigned long vsgaddr;
    vuid_t vuid;
};

class mgrp_iterator : public olist_iterator {
    struct MgrpKey *key;

  public:
    mgrp_iterator(struct MgrpKey * =(struct MgrpKey *)0);
    mgrpent *operator()();
};


class vsgdb {
  friend void VSGInit();
  friend void VSGD_Init();
  friend void VSGDaemon();
  friend class vsgent;
  friend class vsg_iterator;

    int MagicNumber;

    /* The hash table. */
    rec_ohashtab htab;

    /* The free list. */
    rec_olist freelist;

    /* Constructors, destructors. */
    void *operator new(size_t);
    void operator delete(void *, size_t);

    vsgdb();
    void ResetTransient();
    ~vsgdb() { abort(); }	/* they never go away ... */

    /* Allocation/Deallocation routines. */
    vsgent *Create(unsigned long, unsigned long *);

    /* Daemon functions. */
    void GetDown();

  public:
    vsgent *Find(unsigned long);
    int Get(vsgent **, unsigned long, unsigned long * =0);
    void Put(vsgent **);

    void DownEvent(unsigned long);
    void UpEvent(unsigned long);
    void WeakEvent(unsigned long);
    void StrongEvent(unsigned long);

    void print() { print(stdout); } 
    void print(FILE *fp) { fflush(fp); print(fileno(fp)); }
    void print(int, int =0);
};


class vsgent {
  friend void VSGInit();
  friend class vsgdb;
  friend class vsg_iterator;
  friend class volent;

    int MagicNumber;

    /* Key */
    unsigned long Addr;

    /* Assoc(Key) */
    unsigned long Hosts[VSG_MEMBERS];

    rec_olink	handle;			/* link for {htab, freelist} */
    /*T*/int	refcnt;			/* entry may NOT be deleted while this is non-zero. */
    /*T*/int	EventCounter;		/* count of AVSG membership transitions */

    /* Constructors, destructors, and private utility routines. */
    void *operator new(size_t);
    void operator delete(void *, size_t);
    vsgent(int, unsigned long, unsigned long *);
    void ResetTransient();
    vsgent(vsgent&) { abort(); }	/* not supported! */
    operator=(vsgent&) { abort(); return(0); }	/* not supported! */
    ~vsgent();
    void hold();
    void release();

  public:
    void GetHosts(unsigned long *);
    int IsMember(unsigned long);
    void DownMember(long);
    void UpMember(long);
    void WeakMember();
    void StrongMember();
    int GetEventCounter() { return(EventCounter); }
    int Connect(RPC2_Handle *, int *, vuid_t);
    void GetBandwidth(unsigned long *);

    void print() { print(stdout); }
    void print(FILE *fp) { fflush(fp); print(fileno(fp)); }
    void print(int);
};


class vsg_iterator : public rec_ohashtab_iterator {

  public:
    vsg_iterator(void * =(void *)-1);
    vsgent *operator()();
};


/*  *****  Variables  *****  */

extern int COPModes;
extern int UseMulticast;
extern char myHostName[];
extern unsigned long myHostId;
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
#define	ASYNCCOP1	(COPModes & 1)
#define	ASYNCCOP2	(COPModes & 2)
#define	PIGGYCOP2	(COPModes & 4)

/* comm.c */
extern void CommInit();
extern void Conn_Wait();
extern void Conn_Signal();
extern int GetAdmConn(connent **);
extern int GetConn(connent **, unsigned long, vuid_t, int);
extern void PutConn(connent **);
//extern void ConnPrint();
//extern void ConnPrint(FILE *);
//extern void ConnPrint(int);
extern void Srvr_Wait();
extern void Srvr_Signal();
extern srvent *FindServer(unsigned long);
extern srvent *FindServerByCBCid(RPC2_Handle);
extern void GetServer(srvent **, unsigned long);
extern void PutServer(srvent **);
extern void ProbeServers(int);
extern void DoProbes(int, unsigned long *);
extern void MultiBind(int, unsigned long *, connent **);
extern void MultiProbe(int, RPC2_Handle *);
extern long HandleProbe(int, RPC2_Handle *, long, long);
extern void ServerProbe(long * =0, long * =0);
extern void DownServers(char *, unsigned int *);
extern void DownServers(int, unsigned long *, char *, unsigned int *);
extern void CheckServerBW(long);
//extern void ServerPrint();
//extern void ServerPrint(FILE *);
//extern void ServerPrint(int);
extern void Mgrp_Wait();
extern void Mgrp_Signal();
extern int GetMgrp(mgrpent **, unsigned long, vuid_t);
extern void PutMgrp(mgrpent **);
//extern void MgrpPrint();
//extern void MgrpPrint(FILE *);
//extern void MgrpPrint(int);
extern void VSGInit();
extern int FailDisconnect(int, unsigned long *);
extern int FailReconnect(int, unsigned long *);
extern int FailSlow(unsigned *);

/* comm_daemon.c */
extern void PROD_Init();
extern void ProbeDaemon();
extern void VSGDaemon(); /* used to be member of class vsgdb */
extern void VSGD_Init();

/* comm synchronization */
struct CommQueueStruct {
    int count[LWP_MAX_PRIORITY+1];
    char sync;
};

extern struct CommQueueStruct CommQueue;

/*
 * The CommQueue summarizes outstanding RPC traffic for all threads.
 * Threads servicing requests for weakly connected volumes defer to 
 * higher priority threads before using the network.  Note that Venus
 * cannot determine the location of a network bottleneck.  Therefore,
 * it conservatively assumes that all high priority requests are 
 * sources of interference.   Synchronization could be finer, currently 
 * all waiters are awakened instead of the highest priority ones. 
 */
#define START_COMMSYNC()\
{   vproc *vp = VprocSelf();\
    if (vp->u.u_vol && vp->u.u_vol->IsWeaklyConnected()) {\
	int pri = LWP_MAX_PRIORITY;\
	while (pri > vp->lwpri) {\
	    if (CommQueue.count[pri]) { /* anyone bigger than me? */\
		LOG(0, ("WAITING(CommQueue) pri = %d, for %d at pri %d\n",\
			vp->lwpri, CommQueue.count[pri], pri));\
		START_TIMING();\
		VprocWait(&CommQueue.sync);\
		END_TIMING();\
                LOG(0, ("WAIT OVER, elapsed = %3.1f\n", elapsed));\
		pri = LWP_MAX_PRIORITY;\
	    } else {\
		pri--;\
	    }\
	}\
    }\
    CommQueue.count[vp->lwpri]++;\
    LOG(10, ("CommQueue: insert pri %d count = %d\n", \
	    vp->lwpri, CommQueue.count[vp->lwpri]));\
}

#define END_COMMSYNC()\
{\
    vproc *vp = VprocSelf();\
    CommQueue.count[vp->lwpri]--;\
    LOG(10, ("CommQueue: remove pri %d count = %d\n", \
	    vp->lwpri, CommQueue.count[vp->lwpri]));\
    VprocSignal(&CommQueue.sync);\
}


/* comm statistics (move to venus.private.h?) */
#ifdef	TIMING
#define START_COMMSTATS()\
    RPCPktStatistics startCS, endCS;\
    GetCSS(&startCS);
#define END_COMMSTATS()\
    if (LogLevel >= 1000) {\
	GetCSS(&endCS);\
	SubCSSs(&endCS, &startCS);\
    }
#define	MULTI_START_MESSAGE(viceop)\
    START_COMMSYNC();\
    LOG(10, ("(Multi)%s: start\n", RPCOpStats.RPCOps[viceop].name));\
    START_TIMING();\
    START_COMMSTATS();
#define	UNI_START_MESSAGE(viceop)\
    START_COMMSYNC();\
    LOG(10, ("%s: start\n", RPCOpStats.RPCOps[viceop].name));\
    START_TIMING();\
    START_COMMSTATS();

/* The LOG message at the end of this macro causes the sun4 to die.  This
 * is the quick hack.
 */
#if defined(sun4) || defined(sparc)
#define MULTI_END_MESSAGE(viceop)\
    END_TIMING();\
    END_COMMSYNC();\
    END_COMMSTATS();\
    LOG(10, ("(Multi)%s: code = %d, elapsed = %3.1f\n", RPCOpStats.RPCOps[viceop].name, code, elapsed));\
    LOG(1000, ("RPC2_SStats: Total = %d\n", endCS.RPC2_SStats_Multi.Multicasts));\
    LOG(1000, ("SFTP_SStats: Starts = %d, Datas = %d, DataRetries = %d, Acks = %d\n", endCS.SFTP_SStats_Multi.Starts, endCS.SFTP_SStats_Multi.Datas, endCS.SFTP_SStats_Multi.DataRetries, endCS.SFTP_SStats_Multi.Acks));\
    LOG(1000, ("RPC2_RStats: Replies = %d, Busies = %d, Naks = %d, Bogus = %d\n", endCS.RPC2_RStats_Multi.Replies, endCS.RPC2_RStats_Multi.Busies, endCS.RPC2_RStats_Multi.Naks, endCS.RPC2_RStats_Multi.Bogus));\
    LOG(1000, ("SFTP_RStats: Datas = %d, Acks = %d, Busies = %d\n", endCS.SFTP_RStats_Multi.Datas, endCS.SFTP_RStats_Multi.Acks, endCS.SFTP_RStats_Multi.Busies));
#else
#define MULTI_END_MESSAGE(viceop)\
    END_TIMING();\
    END_COMMSYNC();\
    END_COMMSTATS();\
    LOG(10, ("(Multi)%s: code = %d, elapsed = %3.1f\n", RPCOpStats.RPCOps[viceop].name, code, elapsed));\
    LOG(1000, ("RPC2_SStats: Total = %d\n", endCS.RPC2_SStats_Multi.Multicasts));\
    LOG(1000, ("SFTP_SStats: Starts = %d, Datas = %d, DataRetries = %d, Acks = %d\n", endCS.SFTP_SStats_Multi.Starts, endCS.SFTP_SStats_Multi.Datas, endCS.SFTP_SStats_Multi.DataRetries, endCS.SFTP_SStats_Multi.Acks));\
    LOG(1000, ("RPC2_RStats: Replies = %d, Busies = %d, Naks = %d, Bogus = %d\n", endCS.RPC2_RStats_Multi.Replies, endCS.RPC2_RStats_Multi.Busies, endCS.RPC2_RStats_Multi.Naks, endCS.RPC2_RStats_Multi.Bogus));\
    LOG(1000, ("SFTP_RStats: Datas = %d, Acks = %d, Busies = %d\n", endCS.SFTP_RStats_Multi.Datas, endCS.SFTP_RStats_Multi.Acks, endCS.SFTP_RStats_Multi.Busies));\
    if (elapsed > 1000.0)\
	LOG(0, ("*** Long Running (Multi)%s: code = %d, elapsed = %3.1f ***\n",\
		RPCOpStats.RPCOps[viceop].name, code, elapsed));
#endif

#define UNI_END_MESSAGE(viceop)\
    END_TIMING();\
    END_COMMSYNC();\
    END_COMMSTATS();\
    LOG(10, ("%s: code = %d, elapsed = %3.1f\n", RPCOpStats.RPCOps[viceop].name, code, elapsed));\
    LOG(1000, ("RPC2_SStats: Total = %d\n", endCS.RPC2_SStats_Uni.Total));\
    LOG(1000, ("SFTP_SStats: Starts = %d, Datas = %d, DataRetries = %d, Acks = %d\n", endCS.SFTP_SStats_Uni.Starts, endCS.SFTP_SStats_Uni.Datas, endCS.SFTP_SStats_Uni.DataRetries, endCS.SFTP_SStats_Uni.Acks));\
    LOG(1000, ("RPC2_RStats: Replies = %d, Busies = %d, Naks = %d, Bogus = %d\n", endCS.RPC2_RStats_Uni.Replies, endCS.RPC2_RStats_Uni.Busies, endCS.RPC2_RStats_Uni.Naks, endCS.RPC2_RStats_Uni.Bogus));\
    LOG(1000, ("SFTP_RStats: Datas = %d, Acks = %d, Busies = %d\n", endCS.SFTP_RStats_Uni.Datas, endCS.SFTP_RStats_Uni.Acks, endCS.SFTP_RStats_Uni.Busies));\
    if (elapsed > 1000.0)\
	LOG(0, ("*** Long Running %s: code = %d, elapsed = %3.1f ***\n",\
		RPCOpStats.RPCOps[viceop].name, code, elapsed));
#define MULTI_RECORD_STATS(viceop)\
    if (code < 0) RPCOpStats.RPCOps[viceop].Mrpc_retries++;\
    else if (code > 0) RPCOpStats.RPCOps[viceop].Mbad++;\
    else {\
	RPCOpStats.RPCOps[viceop].Mgood++;\
	RPCOpStats.RPCOps[viceop].Mtime += elapsed;\
    }
#define UNI_RECORD_STATS(viceop)\
    if (code < 0) RPCOpStats.RPCOps[viceop].rpc_retries++;\
    else if (code > 0) RPCOpStats.RPCOps[viceop].bad++;\
    else {\
	RPCOpStats.RPCOps[viceop].good++;\
	RPCOpStats.RPCOps[viceop].time += elapsed;\
    }
#else	TIMING
#define	MULTI_START_MESSAGE(viceop)\
    START_COMMSYNC();\
    LOG(10, ("(Multi)%s: start\n", RPCOpStats.RPCOps[viceop].name));
#define	UNI_START_MESSAGE(viceop)\
    START_COMMSYNC();\
    LOG(10, ("%s: start\n", RPCOpStats.RPCOps[viceop].name));
#define MULTI_END_MESSAGE(viceop)\
    END_COMMSYNC();\
    LOG(10, ("(Multi)%s: code = %d\n", RPCOpStats.RPCOps[viceop].name, code));
#define UNI_END_MESSAGE(viceop)\
    END_COMMSYNC();\
    LOG(10, ("%s: code = %d\n", RPCOpStats.RPCOps[viceop].name, code));
#define MULTI_RECORD_STATS(viceop)\
    if (code < 0) RPCOpStats.RPCOps[viceop].Mrpc_retries++;\
    else if (code > 0) RPCOpStats.RPCOps[viceop].Mbad++;\
    else RPCOpStats.RPCOps[viceop].Mgood++;
#define UNI_RECORD_STATS(viceop)\
    if (code < 0) RPCOpStats.RPCOps[viceop].rpc_retries++;\
    else if (code > 0) RPCOpStats.RPCOps[viceop].bad++;\
    else RPCOpStats.RPCOps[viceop].good++;
#endif	TIMING

#define VENUS_MAXBSLEN 1024   /* For use in ARG_MARSHALL_BS */

#endif	not _VENUS_COMM_H_
