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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/venus/Attic/venusvm.cc,v 4.11 1998/08/26 21:24:39 braam Exp $";
#endif /*_BLURB_*/


/*
 *
 * Implementation of the Venus Vmon module.
 *
 */


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>

#ifdef __BSD44__
#include <machine/endian.h>
#include <nlist.h>
/* nlist.h defines this function but it isnt getting included because it is
   guarded by an ifdef of CMU which isnt getting defined.  XXXXX pkumar 6/13/95 */ 
extern int nlist(const char*, struct nlist[]);
#endif

#include <rpc2.h>
#include <rds.h>
#include <rvm.h>
#include <cfs/coda_opstats.h>

/* interfaces */
#include <vice.h>
#include <mond.h>
#ifdef __cplusplus
}
#endif __cplusplus


/* from util */
#include <olist.h>

/* from venus */
#include "adviceconn.h"
#include "local.h"
#include "mariner.h"
#include "simulate.h"
#include "user.h"
#include "venus.private.h"
#include "venusrecov.h"
#include "venusvm.h"
#include "vproc.h"


/* *****  Private constants  ***** */

#ifdef __NetBSD__
#define VMUNIX "/netbsd"
#endif
#ifdef __FreeBSD__
#undef VMUNIX
#define VMUNIX "/kernel"
#endif

#define	DFLT_VMONHOST	"barber.coda.cs.cmu.edu"
#define	DFLT_VMONPORTAL	1356

static const int WarnInterval = 2*60*60;      /* every two hours */
static const int VmonMaxDataSize = 1024 * 1024;       /* one meg */
static const int VmonMaxRvmDataSize = 64 * 1024;          /* 64K */
static const int VmonMaxFreeSEs = 10;
static const int VmonMaxFreeCEs = 10;
static const int VmonBindInterval = 300;   /* every five minutes */
static const int VmonCallEventInterval = 60 * 60;  /* every hour */
static const int VmonMiniCacheInterval = 60 * 60;  /* every hour */
static const int VmonAdviceInterval = 60 * 60;     /* every hour */
static const int VmonRwsInterval = 120 * 60;       /* every two hours */
static const int VmonSubtreeInterval = 120 * 60;   /* every two hours */
static const int VmonRepairInterval = 120 * 60;    /* every two hours */
static const int VmonVCBInterval = 120 * 60;  /* every two hours */

/* ***** Private types ***** */

/* Forward declaration. */
VmonVenusId MyVenusId;
static RPC2_Integer VmonCommSerial = 0;


/* Session Entry. */
struct vmse : public olink {
#ifdef	VENUSDEBUG
    static int allocs;
    static int deallocs;
#endif	VENUSDEBUG

    VmonVenusId Venus;
    VmonSessionId Session;
    VolumeId Volume;
    UserId User;
    VmonAVSG AVSG;
    RPC2_Unsigned StartTime;
    RPC2_Unsigned EndTime;
    RPC2_Unsigned CETime;
    RPC2_Integer Size;
    VmonSessionEvent *Events;
    SessionStatistics Stats;
    CacheStatistics CacheStats;

#if	defined(romp) || defined(ibm032) || defined (ibmrt)
#else
    vmse() {
#ifdef	VENUSDEBUG
    allocs++;
#endif	VENUSDEBUG
    }
    ~vmse() {
#ifdef	VENUSDEBUG
    deallocs++;
#endif	VENUSDEBUG
    }
#endif

    void Init(VmonSessionId session, VolumeId volume, UserId user, VmonAVSG *avsg,
	       RPC2_Unsigned starttime, RPC2_Unsigned endtime,
	       RPC2_Unsigned cetime,
	       RPC2_Integer size, VmonSessionEvent *events, 
	       SessionStatistics *stats, CacheStatistics *cachestats) {
	Venus = MyVenusId;
	Session = session;
	Volume = volume;
	User = user;
	AVSG = *avsg;
	StartTime = starttime;
	EndTime = endtime;
	CETime = cetime;
	Size = size;
	Events = events;
	Stats = *stats;
	CacheStats = *cachestats;
    }
};

#ifdef	VENUSDEBUG
int vmse::allocs = 0;
int vmse::deallocs = 0;
#endif	VENUSDEBUG



/* CommEvent Entry. */
struct vmce : public olink {
#ifdef	VENUSDEBUG
    static int allocs;
    static int deallocs;
#endif	VENUSDEBUG

    VmonVenusId Venus;
    RPC2_Unsigned ServerIPAddress;
    RPC2_Integer SerialNumber;
    RPC2_Unsigned Time;
    VmonCommEventType Type;

#if	defined(romp) || defined(ibm032) || defined (ibmrt)
#else
    vmce() {
#ifdef	VENUSDEBUG
    allocs++;
#endif	VENUSDEBUG
    }
    ~vmce() {
#ifdef	VENUSDEBUG
    deallocs++;
#endif	VENUSDEBUG
    }
#endif

    void Init(RPC2_Unsigned serveripaddress, RPC2_Unsigned time, VmonCommEventType type) {
	Venus = MyVenusId;
	ServerIPAddress = serveripaddress;
	SerialNumber = VmonCommSerial++;
	Time = time;
	Type = type;
    }
};

#ifdef	VENUSDEBUG
int vmce::allocs = 0;
int vmce::deallocs = 0;
#endif	VENUSDEBUG



/* Call Count records are constructed at Report time, and are not
   stored in VM. */

/* OverflowEvent Entry. */
struct vmoe {
    VmonVenusId Venus;
    RPC2_Unsigned VMStartTime;
    RPC2_Unsigned VMEndTime;
    RPC2_Integer VMCount;
    RPC2_Unsigned RVMStartTime;
    RPC2_Unsigned RVMEndTime;
    RPC2_Integer RVMCount;

    void Init(RPC2_Unsigned starttime, RPC2_Unsigned endtime, RPC2_Integer count) {
	Venus = MyVenusId;
	VMStartTime = starttime;
	VMEndTime = endtime;
	VMCount = count;
	RVMStartTime = starttime;
	RVMEndTime = endtime;
	RVMCount = count;
    }
};


/* ***** Private variables  ***** */

static int VmonEnabled = 1;
static int VmonInited = 0;
static olist *CEActiveList = 0;
static olist *CEFreeList = 0;
static vmoe OE;
static RPC2_Handle VmonHandle = 0;
static unsigned long LastVmonBindAttempt = 0;
static RvmStatistics stats;
static int VmonSessionEventArraySize = 0;

static int kmem;
#ifdef __BSD44__
static struct nlist RawStats[3];
#else
long RawStats[3];
#endif
static struct cfs_op_stats vfsop_init_stats[CFS_VFSOPS_SIZE];
static struct cfs_op_stats vnode_init_stats[CFS_VNODEOPS_SIZE];

/* ***** Private routines  ***** */

static void VmonNoteOverflow(enum OverFlow);
static void CheckSE();			    /* Session Entries */
static void CheckCE();			    /* CommEvent Entries */
static void CheckCL();                     /* CallCount Entries */
static void CheckST();                     /* Statistics Entries */
static void CheckMC();                     /* MiniCache Entries */
static void CheckAdvice();                 /* Advice Entries */
static void CheckOE();			    /* Overflow Entry */
static void CheckRW();	    		    /* Report Read/Write Sharing Stats */
static void CheckSubtree();		    /* Report Local Subtree Stats */
static void CheckRepair(); 		    /* Repair Local-Global Repair Stats */
void CheckVCB();			    /* VCB Entries */
static int ValidateVmonHandle();
static void tprint(char *,long);
static int CheckVmonResult(long);
static int VmonSpaceUsed();
static int VmonRvmSpaceUsed();
static void GetStatistics(RvmStatistics *);


/*  *****  External variables  *****  */

char *VmonHost = DFLT_VMONHOST;		    /* may be overridden from command line */
unsigned long VmonAddr = 0;
int VmonPortal = DFLT_VMONPORTAL;	    /* may be overridden from command line */

/*  *****  Vmon  *****  */

void VmonInit() {

#ifndef __BSD44__
    VmonEnabled = 0;
    return;
#endif

    if (Simulating) {
	VmonEnabled = 0;
	return;
    }

    struct hostent *h = gethostbyname(VmonHost);
    if (h) VmonAddr = ntohl(*((unsigned long *)h->h_addr));

    MyVenusId.IPAddress = myHostId;
    MyVenusId.BirthTime = Vtime();

    CEActiveList = new olist;
    CEFreeList = new olist;
    OE.Init(0, 0, 0);

    kmem = open("/dev/kmem",0,0);
    if (kmem <= 0) {
	Choke("Could not open /dev/kmem for reading");
    }
#ifdef __BSD44__

    RawStats[0].n_name = "_cfs_vfsopstats";
    RawStats[1].n_name = "_cfs_vnodeopstats";
    RawStats[2].n_name = 0;
    if (nlist(VMUNIX,RawStats) != 0) {
	fprintf(stderr, "ERROR: running a pre-vfs-statistics kernel\n");
	fflush(stderr);
	LOG(0, ("ERROR: running a pre-vfs-statistics kernel\n"));
	/* make the penalty a bit harsh */
	ASSERT(0);
    }

    kmem = open("/dev/kmem",0,0);
    if (kmem <= 0) {
	fprintf(stderr, "ERROR: could not open /dev/kmem for reading\n");
	fflush(stderr);
	LOG(0, ("ERROR: could not open /dev/kmem for reading\n"));
	ASSERT(0);
    }

    lseek(kmem, (long)RawStats[0].n_value, 0);
    read(kmem, (char *)vfsop_init_stats, 
	 (int) (sizeof(struct cfs_op_stats)*CFS_VFSOPS_SIZE));
    
    lseek(kmem, (long)RawStats[1].n_value, 0);
    read(kmem, (char*)vnode_init_stats,
	 (int)(sizeof(struct cfs_op_stats)*CFS_VNODEOPS_SIZE));

    VmonSessionEventArraySize = (int) sizeof(VmonSessionEventArray);

    VMOND_Init();
#endif
    VmonInited = 1;
}


void VmonEnqueueSession(VmonSessionId Session, VolumeId Volume, UserId User,
			 VmonAVSG *AVSG, RPC2_Unsigned StartTime,
			 RPC2_Unsigned EndTime, RPC2_Unsigned CETime,
			 VmonSessionEventArray *Events,
			 SessionStatistics *Stats, CacheStatistics *CacheStats) {
    int i;

    if (!VmonInited || !VmonEnabled) return;

    if (EndTime == 0) return;    /* if 0, it means it is not pushed by PushVSR */

    if (LogLevel >= 100) 
	MarinerLog("mond::EnqueueSession (%x, %d)\n", Volume, User);
    LOG(100, ("VmonEnqueueSession: [ %d  %x  %d  %d  %d ]\n",
	       Session, Volume, User, StartTime, EndTime));

    int packedsize, entries, checks;
    packedsize = entries = checks = 0;
    VmonSessionEvent *elem;

    for (i = 0; i < (VmonSessionEventArraySize/sizeof(VmonSessionEvent)); i++) {
        elem = &((&(Events->Event0))[i]);
	if (elem->SuccessCount != 0 || elem->FailureCount != 0)
	    ++entries;
    }
    packedsize = sizeof(VmonSessionEvent) * entries;

    /* make sure that new data can be stored in RVM */
    if (VmonRvmSpaceUsed() + sizeof(struct vmse) + packedsize > VmonMaxRvmDataSize) {
	VmonNoteOverflow(RVMOVERFLOW);
	return;
    }

    struct vmse *se;
    VmonSessionEvent *packed;         /* packed VmonSessionEventArray */
    struct olink *shadowp;
    /* N.B. Won't use rvmlib macros here, since this routine may be called
       in the context of a signal handler, which doesn't satisfy the state
       assumed by the macros. */
    {
        rvm_tid_t tid;
	int err;
	rvm_init_tid(&tid);
	rvm_return_t rvmret;

	rvmret = rvm_begin_transaction(&tid, no_restore);
	ASSERT(rvmret == RVM_SUCCESS);

	se = (vmse *)rds_malloc(sizeof(vmse), &tid, &err);
	if (err != 0) {
	    if (LogLevel >= 100) 
		MarinerLog("mond::EnqueueSession failed (%d)\n", err);
	    rvmret = rvm_abort_transaction(&tid);
	    return;
	}
	if (packedsize != 0) {
	    packed = (VmonSessionEvent *)rds_malloc(packedsize, &tid, &err);
	    if (err != 0) {
	        if (LogLevel >= 100) 
		    MarinerLog("mond::EnqueueSession failed (%d)\n", err);
		rvmret = rvm_abort_transaction(&tid);
		return;
	    }
	} else packed = NULL;
	rvmret = rvm_set_range(&tid, (char *)se, sizeof(*se));
	ASSERT(rvmret == RVM_SUCCESS);
	if (packedsize != 0) {
	    rvmret = rvm_set_range(&tid, (char *)packed, packedsize);
	    ASSERT(rvmret == RVM_SUCCESS);
	}
	rvmret = rvm_set_range(&tid, (char *)&VMSE, sizeof(VMSE));
	ASSERT(rvmret == RVM_SUCCESS);

	if ((shadowp = SEActiveList.last()) != 0) {
	    rvmret = rvm_set_range(&tid, (char *)shadowp, sizeof(*shadowp));
	    ASSERT(rvmret == RVM_SUCCESS);
	}

	for (i = 0; i < (VmonSessionEventArraySize/sizeof(VmonSessionEvent)); i++) {
	    elem = &((&(Events->Event0))[i]);
	    if (elem->SuccessCount != 0 || elem->FailureCount != 0)
	        packed[checks++] = *elem;
	}
	ASSERT(entries == checks);

	se->Init(Session, Volume, User, AVSG, StartTime, EndTime, 
		 CETime, entries, packed, Stats, CacheStats);
	SEActiveList.append(se);
	VMSE.count += packedsize;
	
	rvmret = rvm_end_transaction(&tid, flush);
	ASSERT(rvmret == RVM_SUCCESS);
    }
    
}


void VmonEnqueueCommEvent(RPC2_Unsigned ServerIPAddress,
			   RPC2_Unsigned Time, VmonCommEventType Type) {
    if (!VmonInited || !VmonEnabled) return;

    LOG(100, ("VmonEnqueueCommEvent: [ %x  %d  %d ]\n",
	       ServerIPAddress, Time, Type));

    if (VmonSpaceUsed() + sizeof(struct vmce) > VmonMaxDataSize) {
	VmonNoteOverflow(VMOVERFLOW);
	return;
    }

    struct vmce *ce = (struct vmce *)CEFreeList->get();
    if (ce == 0) ce = new vmce;
    ce->Init(ServerIPAddress, Time, Type);
    CEActiveList->append(ce);
}


void VmonPrint() {
    VmonPrint(stdout);
}


void VmonPrint(FILE *fp) {
    fflush(fp);
    VmonPrint(fileno(fp));
}


void VmonPrint(int fd) {
    if (!VmonInited || !VmonInited) return;

    fdprint(fd, "Vmon: SE = (%d 0), CE = (%d %d), OE = (%d %d)\n",
	     SEActiveList.count(),
	     CEActiveList->count(), CEFreeList->count(),
	     OE.VMCount, OE.RVMCount);

    /* Don't bother printing detail.  -JJK */

#ifdef	VENUSDEBUG
    fdprint(fd, "vmse: %d, %d, %d\n", vmse::allocs, vmse::deallocs,
	     (vmse::allocs - vmse::deallocs) * sizeof(vmse));
    fdprint(fd, "vmce: %d, %d, %d\n", vmce::allocs, vmce::deallocs,
	     (vmce::allocs - vmce::deallocs) * sizeof(vmce));
#endif	VENUSDEBUG
}


static void VmonNoteOverflow(enum OverFlow vm) {

    switch (vm) {
        case VMOVERFLOW:
                LOG(0, ("VmonNoteOverflow(VM): count = %d, space used = %d, max = %d\n",
			OE.VMCount, VmonSpaceUsed(), VmonMaxDataSize));
                if (OE.VMCount++ == 0)
		    OE.VMStartTime = Vtime();
		break;
        case RVMOVERFLOW:
	        LOG(0, ("VmonNoteOverflow(RVM): count = %d, space used = %d, max = %d\n",
			OE.RVMCount, VmonRvmSpaceUsed(), VmonMaxRvmDataSize));
		if (OE.RVMCount++ == 0)
		    OE.RVMStartTime = Vtime();
		break;
	default:
		ASSERT(0);    /* never reached */
		break;
    }
}


static void CheckSE() {
    if (!VmonInited || !VmonEnabled) return;

    if (!ValidateVmonHandle()) return;

    struct vmse *se = 0;
    int hits = 0;
    int misses = 0;
    Recov_BeginTrans();
    while (se = (struct vmse *)SEActiveList.first()) {
	    if (LogLevel >= 100) 
		    MarinerLog("mond::ReportSession (%x, %d)\n", se->Volume, se->User);
	    hits = (int)(se->CacheStats.HoardDataHit.Count + 
			 se->CacheStats.NonHoardDataHit.Count + 
			 se->CacheStats.UnknownHoardDataHit.Count + 
			 se->CacheStats.HoardAttrHit.Count + 
			 se->CacheStats.NonHoardAttrHit.Count + 
			 se->CacheStats.UnknownHoardAttrHit.Count);
	    misses = (int)(se->CacheStats.HoardDataMiss.Count +
			   se->CacheStats.NonHoardDataMiss.Count +
			   se->CacheStats.UnknownHoardDataMiss.Count +
			   se->CacheStats.HoardAttrMiss.Count +
			   se->CacheStats.NonHoardAttrMiss.Count +
			   se->CacheStats.UnknownHoardAttrMiss.Count);
	    VDB->SaveCacheInfo(se->Volume, se->User, hits, misses);
	    long code = VmonReportSession(VmonHandle, &se->Venus, se->Session,
					  se->Volume, se->User, &se->AVSG,
					  se->StartTime, se->EndTime,
					  se->CETime,
					  se->Size, se->Events,
					  &se->Stats, &se->CacheStats);
	    LOG(100,("Session report: [%x:%d] (%x:%d) (%d) [%d : %d] (%ul)\n",
		     se->Venus.IPAddress, se->Venus.BirthTime,
		     se->Volume, se->User, se->Session, se->StartTime,
		     se->EndTime, se->CETime));
		    LOG(100,("[%x %x %x]\n",se->AVSG.Member0,se->AVSG.Member1,se->AVSG.Member2));
		    if (LogLevel >= 100) 
			    MarinerLog("mond::ReportSession done\n");
		    code = CheckVmonResult(code);
		    if (code != 0) break;  
		    
		    //		    RVMLIB_REC_OBJECT(*se);    /* not needed */
		    RVMLIB_REC_OBJECT(VMSE);
		    if (SEActiveList.remove(se) != se)
			    Choke("CheckSE: remove(se)");
		    struct olink *shadowp = SEActiveList.first();
		    if (shadowp != NULL) RVMLIB_REC_OBJECT(*shadowp);
		    VMSE.count -= (sizeof(VmonSessionEvent) * (se->Size));
		    if (se->Size != 0)
			    rvmlib_rec_free(se->Events);
		    rvmlib_rec_free(se);
    }
    Recov_EndTrans(0);
}


static void CheckCE() {
    if (!VmonInited || !VmonEnabled) return;

    if (!ValidateVmonHandle()) return;

    struct vmce *ce = 0;
    while (ce = (struct vmce *)CEActiveList->first()) {
	if (LogLevel >= 100) 
	    MarinerLog("mond::ReportCommEvent (%x, %d)\n", ce->ServerIPAddress, ce->Type);
	long code = VmonReportCommEvent(VmonHandle, &ce->Venus,
				       ce->ServerIPAddress, ce->SerialNumber,
				       ce->Time, ce->Type);
	if (LogLevel >= 100) 
	    MarinerLog("mond::reportcommevent done\n");
	code = CheckVmonResult(code);
	if (code != 0) return;

	if (CEActiveList->remove(ce) != ce)
	    Choke("CheckCE: remove(ce)");
	if (CEFreeList->count() < VmonMaxFreeCEs)
	    CEFreeList->insert(ce);
	else
	    delete ce;
    }
}


static void CheckCL() {

    if (!VmonInited || !VmonEnabled) return;

    static long LastTime =0;
    long Time = Vtime();
    if (Time - LastTime < VmonCallEventInterval) return;
    LastTime = Time;

    if (ValidateVmonHandle()) {
	GetStatistics(&stats);
	if (LogLevel >= 100) 
	    MarinerLog("mond::ReportCallEvent (%d)\n", Time);
	long code = VmonReportCallEvent(VmonHandle, &MyVenusId, Time,
				       srvOPARRAYSIZE, srv_CallCount);
	if (!code) {
	    code = VmonReportMCallEvent(VmonHandle, &MyVenusId, Time,
					srvOPARRAYSIZE, srv_MultiCall);
	    if (!code) {
		code = VmonReportRVMStats(VmonHandle, &MyVenusId, Time,
					  &stats);
	    }
	}
      exit:
	code = CheckVmonResult(code);
    }
}

static void CheckMC() {       // Check minicache stats
#if defined(__BSD44__)
    int i;

    if (!VmonInited || !VmonEnabled) return;

    static long LastTime =0;
    long Time = Vtime();
    if (Time - LastTime < VmonMiniCacheInterval) return;
    LastTime = Time;

    struct cfs_op_stats vfsop_stats[CFS_VFSOPS_SIZE];
    struct cfs_op_stats vnode_stats[CFS_VNODEOPS_SIZE];

    lseek(kmem, (long)RawStats[0].n_value, 0);
    read(kmem, (char *)vfsop_stats, 
	 (int)(sizeof(struct cfs_op_stats)*CFS_VFSOPS_SIZE));
    
    for (i=0; i<CFS_VFSOPS_SIZE; i++) {
	vfsop_stats[i].opcode = i;
	vfsop_stats[i].entries -= vfsop_init_stats[i].entries;
	vfsop_stats[i].sat_intrn -= vfsop_init_stats[i].sat_intrn;
	vfsop_stats[i].unsat_intrn -= vfsop_init_stats[i].unsat_intrn;
	vfsop_stats[i].gen_intrn -= vfsop_init_stats[i].gen_intrn;
    }

    lseek(kmem, (long)RawStats[1].n_value, 0);
    read(kmem, (char*)vnode_stats,
	 (int)(sizeof(struct cfs_op_stats)*CFS_VNODEOPS_SIZE));

    for (i=0; i<CFS_VNODEOPS_SIZE; i++) {
	vnode_stats[i].opcode = i;
	vnode_stats[i].entries -= vnode_init_stats[i].entries;
	vnode_stats[i].sat_intrn -= vnode_init_stats[i].sat_intrn;
	vnode_stats[i].unsat_intrn -= vnode_init_stats[i].unsat_intrn;
	vnode_stats[i].gen_intrn -= vnode_init_stats[i].gen_intrn;
    }

    if (LogLevel >= 100) 
	MarinerLog("mond::Reporting MiniCache stats\n");
    long code = VmonReportMiniCache (VmonHandle,
				    &MyVenusId,
				    Vtime(),
				    CFS_VNODEOPS_SIZE,
				    (VmonMiniCacheStat *)vnode_stats,
				    CFS_VFSOPS_SIZE,
				    (VmonMiniCacheStat *)vfsop_stats);
    CheckVmonResult(code);
    if (LogLevel >= 100) 
	MarinerLog("mond::Reported MiniCache stats\n");
#endif				    
}

/* static -- was private but couldn't access u->admon then... */ 
void ReportAdviceStatistics(vuid_t vuid) 
{
    userent *u;
    AdviceStatistics stats;
    AdviceCalls calls[MAXEVENTS];
    AdviceResults results[NumRPCResultTypes];
    int numCalls = MAXEVENTS;
    int numResults = NumRPCResultTypes;

    LOG(100, ("ReportAdviceStatistics(%d)\n", vuid));

    /* Get all the data we need */
    GetUser(&u, vuid);
    assert(u != NULL);
    u->GetStatistics(calls, results, &stats);

    if (LogLevel >= 100) 
	MarinerLog("mond::Reporting Advice stats for %d\n", vuid);
    long code = VmonReportAdviceStats (VmonHandle,
                                       &MyVenusId,
                                       Vtime(),
                                       vuid,
                                       &stats,
                                       numCalls,
                                       (AdviceCalls *)calls,
                                       numResults,
                                       (AdviceResults *)results);
    if (LogLevel >= 100) 
	MarinerLog("mond::Reported Advice stats for %d\n", vuid);
    CheckVmonResult(code);
    PutUser(&u);
}

void CheckAdvice()
{
    if (!VmonInited || !VmonEnabled) return;

    user_iterator next;
    userent *u;
    static long LastTime =0;
    long Time = Vtime();

    /* Check if its time to send advice statistics again */
    if (Time - LastTime < VmonAdviceInterval) return;
    LastTime = Time;

    while (u = next()) {
	if ((u->GetUid() != V_UID) &&
	    (u->GetUid() != ALL_UIDS) &&
	    (u->GetUid() != HOARD_UID))
	  ReportAdviceStatistics(u->GetUid());
    }
}

static void CheckOE() {
    if (!VmonInited || !VmonEnabled) return;

    if (OE.VMCount > 0 || OE.RVMCount > 0) {
	if (!ValidateVmonHandle()) return;

	if (OE.VMCount > 0)
	    OE.VMEndTime = Vtime();
	if (OE.RVMCount > 0)
	    OE.RVMEndTime = Vtime();
	if (LogLevel >= 100) 
	    MarinerLog("mond::ReportOverflowEvent(VM) (%d - %d, %d)\n", OE.VMStartTime, OE.VMEndTime, OE.VMCount);
	if (LogLevel >= 100) 
	    MarinerLog("mond::ReportOverflowEvent(RVM) (%d - %d, %d)\n",
		   OE.RVMStartTime, OE.RVMEndTime, OE.RVMCount);
	long code = VmonReportOverflow(VmonHandle, &OE.Venus,
				      OE.VMStartTime, OE.VMEndTime, OE.VMCount,
				      OE.RVMStartTime, OE.RVMEndTime, OE.RVMCount);
	if (LogLevel >= 100) 
	    MarinerLog("mond::reportoverflow done\n");
	code = CheckVmonResult(code);
	if (code != 0) return;

	OE.Init(0, 0, 0);
    }
}

/* must not be called from within a transaction */
/* Report Read/Write Sharing Stat */
void CheckRW()
{
    if (!VmonInited || !VmonEnabled) return;

    static long LastTime =0;
    long Time = Vtime();    
    if (Time - LastTime < VmonRwsInterval) return;
    LastTime = Time;

    if (LogLevel >= 100) 
	MarinerLog("mond::Reporting RW stats\n");
    /* iterating through every volume */
    vol_iterator next;
    volent *v;
    while (v = next()) {
	rec_dlist *RWSQ = v->GetRwQueue();
	if (RWSQ->count() == 0) continue;

	/* iterate throuhg all entries in v->rwsq */
	/* there is a remote possibility of concurrency control problem with rws-queue changes */
	rec_dlist_iterator next(*RWSQ);
	rec_dlink *d, *to_be_deleted = NULL;
	while (d = next()) {
	    if (to_be_deleted) {
		Recov_BeginTrans();
		       ASSERT(RWSQ->remove(to_be_deleted) == to_be_deleted);
		       delete to_be_deleted;
		       to_be_deleted = NULL;
		Recov_EndTrans(MAXFP);
	    }
	    rwsent *rws = (rwsent *)d;
	    ReadWriteSharingStats Stats;
	    bzero((void *)&Stats, (int)sizeof(Stats));
	    Stats.Vid = v->GetVid();
	    Stats.RwSharingCount = rws->sharing_count;
	    Stats.DiscReadCount = rws->disc_read_count;
	    Stats.DiscDuration = rws->disc_duration;
	    if (LogLevel >= 100) 
		MarinerLog("mond::Report RW stats\n");
	    long code = VmonReportRwsStats(VmonHandle,
					   &MyVenusId,
					   Vtime(),
					   &Stats);
	    if (LogLevel >= 100) 
		MarinerLog("mond::Reported RW stats code = %d\n", code);    
	    if (code = CheckVmonResult(code)) return;
	    to_be_deleted = d;
	}
	if (to_be_deleted) {
	    Recov_BeginTrans();
		   ASSERT(RWSQ->remove(to_be_deleted) == to_be_deleted);
		   delete to_be_deleted;
	    Recov_EndTrans(MAXFP);
	}
    }
}

/* Report Local Subtree Stats */
void CheckSubtree()
{
    if (!VmonInited || !VmonEnabled) return;

    static long LastTime =0;
    long Time = Vtime();
    if (Time - LastTime < VmonSubtreeInterval) return;
    LastTime = Time;

    if (LogLevel >= 100) 
	MarinerLog("mond::Reporting Subtree stats\n");
    RPC2_Integer Total = LRDB->subtree_stats.SubtreeNum;
    if (Total == 0) {
	LOG(100, ("No subtree stats to report\n"));
	return;
    }
    LocalSubtreeStats Stats;
    Stats.SubtreeNum = Total;
    Stats.MaxSubtreeSize = LRDB->subtree_stats.MaxSubtreeSize;
    Stats.AvgSubtreeSize = LRDB->subtree_stats.TotalSubtreeSize / Total;
    Stats.MaxSubtreeHgt = LRDB->subtree_stats.MaxSubtreeHgt;
    Stats.AvgSubtreeHgt =LRDB->subtree_stats.TotalSubtreeHgt / Total;
    Stats.MaxMutationNum =LRDB->subtree_stats.MaxMutationNum;
    Stats.AvgMutationNum =LRDB->subtree_stats.TotalMutationNum / Total;
    
    long code = VmonReportSubtreeStats (VmonHandle,
					&MyVenusId,
					Vtime(),
					&Stats);
    if (LogLevel >= 100) 
	MarinerLog("mond::Reported Subtree stats code = %d\n", code);
    CheckVmonResult(code);
}

/* Report Local-Global Repair Stats */
void CheckRepair()
{
    if (!VmonInited || !VmonEnabled) return;

    static long LastTime =0;
    long Time = Vtime();
    if (Time - LastTime < VmonSubtreeInterval) return;
    LastTime = Time;

    if (LRDB->repair_stats.SessionNum == 0) {
	LOG(100, ("There is no subtree stats to report\n"));
	return;
    }
    RepairSessionStats Stats;
    bzero((void *)&Stats, (int)sizeof(RepairSessionStats));
    Stats.SessionNum = LRDB->repair_stats.SessionNum;
    Stats.CommitNum = LRDB->repair_stats.CommitNum;
    Stats.AbortNum = LRDB->repair_stats.AbortNum;
    Stats.CheckNum = LRDB->repair_stats.CheckNum;
    Stats.PreserveNum = LRDB->repair_stats.PreserveNum;
    Stats.DiscardNum = LRDB->repair_stats.DiscardNum;
    Stats.RemoveNum = LRDB->repair_stats.RemoveNum;
    Stats.GlobalViewNum = LRDB->repair_stats.GlobalViewNum;
    Stats.LocalViewNum = LRDB->repair_stats.LocalViewNum;
    Stats.KeepLocalNum = LRDB->repair_stats.KeepLocalNum;
    Stats.ListLocalNum = LRDB->repair_stats.ListLocalNum;
    Stats.NewCommand1Num = 0; 
    Stats.NewCommand2Num = 0; 
    Stats.NewCommand3Num = 0; 
    Stats.NewCommand4Num = 0; 
    Stats.NewCommand5Num = 0; 
    Stats.NewCommand6Num = 0; 
    Stats.NewCommand7Num = 0; 
    Stats.NewCommand8Num = 0;
    Stats.RepMutationNum = LRDB->repair_stats.RepMutationNum;
    Stats.MissTargetNum = LRDB->repair_stats.MissTargetNum;
    Stats.MissParentNum = LRDB->repair_stats.MissParentNum;
    Stats.AclDenyNum = LRDB->repair_stats.AclDenyNum;
    Stats.UpdateUpdateNum = LRDB->repair_stats.UpdateUpdateNum;
    Stats.NameNameNum = LRDB->repair_stats.NameNameNum;
    Stats.RemoveUpdateNum = LRDB->repair_stats.RemoveUpdateNum;

    if (LogLevel >= 100) 
	MarinerLog("mond::Reporting Repair stats\n");
    long code = VmonReportRepairStats(VmonHandle,
				      &MyVenusId,
				      Vtime(),
				      &Stats);
    if (LogLevel >= 100) 
	MarinerLog("mond::Reported Repair stats code = %d\n", code);
    CheckVmonResult(code);
}

void CheckVCB() {
    if (!VmonInited || !VmonEnabled) return;

    static long LastTime =0;
    long Time = Vtime();

    /* Check if its time to send advice statistics again */
    if (Time - LastTime < VmonVCBInterval) return;
    LastTime = Time;

    if (LogLevel >= 100) 
	MarinerLog("mond::Reporting VCB stats\n");

    vcbd_iterator next;
    vcbdent *v;
    while (v = next()) {
	long code = VmonReportVCBStats(VmonHandle, 
				       &MyVenusId,
				       rvg->recov_LastInit,
				       Time,
				       v->vid,
				       &v->data);
	code = CheckVmonResult(code);
	if (code != 0) return;
    }
}


static int ValidateVmonHandle() {
    if (Simulating) return(0);
    if (VmonHandle != 0) return(1);

    long curr_time = Vtime();

    /* Try to bind unless the most recent attempt was within VmonBindInterval seconds. */
    if (curr_time - LastVmonBindAttempt < VmonBindInterval) return(0);

    /* Attempt the bind. */
    LastVmonBindAttempt = curr_time;
    RPC2_HostIdent hid;
    if (VmonAddr) { 	/* use the stashed address to avoid name lookups */
	hid.Tag = RPC2_HOSTBYINETADDR;
	hid.Value.InetAddress = htonl(VmonAddr);
    } else {
	hid.Tag = RPC2_HOSTBYNAME;
	strcpy(hid.Value.Name, VmonHost);
    }
    RPC2_PortalIdent pid;
    pid.Tag = RPC2_PORTALBYINETNUMBER;
    pid.Value.InetPortNumber = htons(VmonPortal);
    RPC2_SubsysIdent ssid;
    ssid.Tag = RPC2_SUBSYSBYID;
    ssid.Value.SubsysId = MondSubsysId;
    RPC2_BindParms bp;
    bp.SecurityLevel = RPC2_OPENKIMONO;
    RPC2_CountedBS ClientIdent;
    ClientIdent.SeqLen = strlen(myHostName) + 1;
    ClientIdent.SeqBody = (RPC2_ByteSeq)myHostName;
    bp.ClientIdent = &ClientIdent;

    long code = RPC2_NewBinding(&hid, &pid, &ssid, &bp, &VmonHandle);

    LOG(1, ("ValidateVmonHandle: bind to [ %s, %d, %d ] returned (%d, %d)\n",
	     VmonHost, VmonPortal, MondSubsysId, code, VmonHandle));
    if (code != 0) {
	VmonHandle = 0;
	return(0);
    }

    /* Successful bind. */

    code = (int) MondEstablishConn(VmonHandle,MOND_CURRENT_VERSION,
				   MOND_VENUS_CLIENT,0,(SpareEntry*)NULL);
    if (code != MOND_OK && code != MOND_OLDVERSION
	&& code != MOND_CONNECTED) {
	tprint("ValidateVmonHandle: You are running an ancient venus",
	       curr_time);

	RPC2_Unbind(VmonHandle);
	VmonHandle = 0;
	return(0);
    }
    if (code == MOND_OLDVERSION)
	tprint("ValidateVmonHandle: You are running an old venus",
	       curr_time);
    
    return(1);

}

static void tprint(char *string, long curr_time) {
    static long last_time = 0;

    if (curr_time - last_time > WarnInterval) {
	last_time = curr_time;
	eprint(string);
    }
}

static int CheckVmonResult(long code) {
    if (code == 0) return(0);

    LOG(0, ("CheckVmonResult: failure (%d)\n", code));

    RPC2_Unbind(VmonHandle);
    VmonHandle = 0;

    return(ETIMEDOUT);
}


static int VmonSpaceUsed() {
    return(CEActiveList->count() * sizeof(struct vmce));
}


static int VmonRvmSpaceUsed() {
    return(SEActiveList.count() * sizeof(struct vmse) + VMSE.count);
}


static void GetStatistics(RvmStatistics *stats)
{
    rds_stats_t rdsstat;

    if (rds_get_stats(&rdsstat) != EBAD_ARGS) {
        stats->Malloc = rdsstat.malloc;
        stats->Free = rdsstat.free;
        stats->FreeBytes = rdsstat.freebytes;
        stats->MallocBytes = rdsstat.mallocbytes;
    }

}


/*  *****  Vmon Daemon  *****  */

static const int VmonDaemonInterval = 60 * TIMERINTERVAL;
static const int VmonDaemonStackSize = 32768;

static char vmondaemon_sync;

void VMOND_Init() {
    (void)new vproc("VmonDaemon", (PROCBODY)&VmonDaemon,
		     VPT_VmonDaemon, VmonDaemonStackSize);
}


void VmonDaemon() {
    /* Hack!  Vproc must yield before data members become valid! */
    VprocYield();

    vproc *vp = VprocSelf();
    RegisterDaemon(VmonDaemonInterval, &vmondaemon_sync);

    for (;;) {
        VprocWait(&vmondaemon_sync);

	LOG(100, ("VmonDaemon: SE = (%d 0), CE = (%d %d), OE = (%d %d)\n",
		  SEActiveList.count(),
		  CEActiveList->count(), CEFreeList->count(),
		  OE.VMCount, OE.RVMCount));
        MarinerLog("mond: Reporting Data\n");
	CheckSE();
	CheckCE();
	CheckCL();
	CheckMC();
        CheckAdvice();
	CheckOE();
	CheckRW();
	CheckSubtree();
	CheckRepair();
	CheckVCB();
        MarinerLog("mond: Reporting Data --> Done\n");

	/* Bump sequence number. */
	vp->seq++;
    }
}
