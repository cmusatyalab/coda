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
 * Implementation of the Venus Volume Session Record (VSR) facility.
 *
 */


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "coda_string.h"

#if    defined(__BSD44__)
#include <sys/dkstat.h>
#include <sys/param.h>
#if defined(__NetBSD_Version__) && (__NetBSD_Version__ >= 104250000)
#include <sys/sched.h>
#endif /* __NetBSD__ */
#endif /* __BSD44__ */

#include <unistd.h>
#include <stdlib.h>

#include <fcntl.h>

#ifdef __BSD44__
#include <nlist.h>
/* nlist.h defines this function but it isnt getting included because it is
   guarded by an ifdef of CMU which isnt getting defined.  XXXXX pkumar 6/13/95 */ 
extern int nlist(const char*, struct nlist[]);
#endif

    
#include <rpc2/rpc2.h>

#ifdef __cplusplus
}
#endif __cplusplus

/* interfaces */
#include <vice.h>
#include <mond.h>

/* from util */
#include <olist.h>

/* from venus */
#include "local.h"
#ifdef VENUSDEBUG
#include "mariner.h"
#endif VENUSDEBUG
#include "venusvol.h"
#include "venus.private.h"
#include "venusvm.h"


/* **** static Variable **** */
#ifdef __BSD44__
static int VmonKmem = 0;
static int hertz = 0;
#endif

#ifdef __NetBSD__
#define VMUNIX "/netbsd"
#endif
#ifdef __FreeBSD__
#undef VMUNIX
#define VMUNIX "/kernel"
#endif
#ifdef	__linux__
#define VMUNIX "/vmlinuz"
#endif
#ifdef __CYGWIN32__
#define CPUSTATES 199
#define VMUNIX "ntos"
#define CP_SYS 0
#define CP_USER 1
#define CP_NICE 2
#define CP_IDLE 3
#endif


/* Raw Statistic Entry. */
#ifdef __BSD44__
static struct nlist RawStats[] = 
{
#define CPTIME	0
    {
	"_cp_time"
    },
#define HZ 1
    {
	"_hz"
    },
#define PHZ 2
    {
	"_phz"
    },
    {
	0
    },
};
#endif

#ifdef VENUSDEBUG
int vsr::allocs = 0;
int vsr::deallocs = 0;
#endif VENUSDEBUG

vsr *volent::GetVSR(vuid_t uid)
{
    CODA_ASSERT(!IsFake());
    LOG(100, ("volent::GetVSR: vol = %x, session = %d, uid = %d\n",
	       vid, VsrUnique, uid));

    /* Flush VSRs when AVSG has changed. */
    if (GetAVSG(NULL) != VsrUnique) {
	FlushVSRs(VSR_FLUSH_HARD);
	VsrUnique = GetAVSG((struct in_addr *)&AVSG);
    }

    /* Check for an existing VSR for this user. */
    olist_iterator next(*vsr_list);
    vsr *v;
    while ((v = (vsr *)next()))
	if (uid == v->uid) return(v);

    /* Construct a new one. */
    v = new vsr(uid);
    InitStatsVSR(v);
    vsr_list->append(v);
    return(v);
}


void volent::PutVSR(vsr *v)
{
    CODA_ASSERT(!IsFake());
    LOG(100, ("volent::PutVSR: vol = %x, session = %d, uid = %d\n",
	       vid, VsrUnique, v->uid));

    CODA_ASSERT(v->cetime == 0);
    v->endtime = Vtime();
    UpdateStatsVSR(v);
}


void volent::FlushVSRs(int hard)
{
    if (IsFake()) return;

    LOG(100, ("volent::FlushVSRs: vol = %x, session = %d, hard = %d\n", 
	      vid, VsrUnique, hard));

    if (hard) {
       /* 
        * A Hard VSR Flush means that the session is KNOWN TO HAVE ENDED.  
        * We get the record off the list, ship it off to the data collector,
        * and then remove it from the list.
        */
	vsr *v;
	while ((v = (vsr *)vsr_list->get())) {
	    /* Convert SigmaTSquared from floating-point milliseconds to fixed-point seconds. */
	    for (int i = 0; i < (sizeof(VmonSessionEventArray) / sizeof(VmonSessionEvent)); i++) {
		VmonSessionEvent *se = &((&(v->events.Event0))[i]);
		se->SigmaTSquared = (RPC2_Unsigned)((*((float *)&se->SigmaTSquared) + 500) / 1000);
	    }

	    VmonEnqueueSession(VsrUnique, vid, v->uid, &AVSG,
			       v->starttime, v->endtime, v->cetime,
                               &v->events, &v->stats, &v->cachestats);
	    delete v;
	}
    }
    else {
       /*
        * A soft VSR Flush means that the session has not yet ended.  We squirrel
        * away a *copy* of the data just in case the session ends unexpectedly.
        */
	vsr *v;
	olist_iterator vnext(*vsr_list);
	while ((v = (vsr *)vnext())) {
	    VmonSessionEventArray *na = new(VmonSessionEventArray);
	    memmove((void *)na, (const void *)&(v->events),(int)sizeof(VmonSessionEventArray));
	    /* Convert SigmaTSquared from floating-point milliseconds to fixed-point seconds. */
	    for (int i = 0; i < (sizeof(VmonSessionEventArray) / sizeof(VmonSessionEvent)); i++) {
		VmonSessionEvent *se = &((&(na->Event0))[i]);
		se->SigmaTSquared = (RPC2_Unsigned)((*((float *)&se->SigmaTSquared) + 500) / 1000);
	    }

	    SessionStatistics *ss = new(SessionStatistics);
	    memmove((void *) ss, (const void *)&v->stats, (int)sizeof(SessionStatistics));

            CacheStatistics *cs = new (CacheStatistics);
            memmove((void *) cs, (const void *)&v->cachestats, (int)sizeof(CacheStatistics));

	    VmonEnqueueSession(VsrUnique, vid, v->uid, &AVSG,
			       v->starttime, v->endtime, v->cetime,
			       na, ss, cs);
	    
	    delete na;
	    delete ss;
	    delete cs;
	}
    }
}


void volent::InitStatsVSR(vsr *v) {
#ifdef __BSD44__
    long busy[CPUSTATES];
    SessionStatistics *Stats = &v->stats;
    SessionStatistics *InitStats = &v->initstats;
    CacheStatistics *CacheStats = &v->cachestats;

    memset((void *)Stats, 0, (int) sizeof(SessionStatistics));
    memset((void *)InitStats, 0, (int) sizeof(SessionStatistics));
    memset((void *)CacheStats, 0, (int) sizeof(CacheStatistics));

    /* Gather ClientModifyLog information */
    Stats->EntriesStart = CML.count();
    Stats->BytesStart = CML.logBytes();

    /* Get volent records */
    InitStats->RecordsCancelled = RecordsCancelled;
    InitStats->RecordsCommitted = RecordsCommitted;
    InitStats->RecordsAborted = RecordsAborted;
    InitStats->FidsRealloced = FidsRealloced;
    InitStats->BytesBackFetched = BytesBackFetched;

    /* Get CPU data */
    if(VmonKmem == 0) {
	nlist(VMUNIX, RawStats);
	if(RawStats[0].n_type == 0) {
	    VmonKmem = -1;
	    return;
	}
	VmonKmem = open("/dev/kmem", 0, 0);
	if (VmonKmem <= 0) {
	    VmonKmem = -1;
	    return;
	}
	/* if phz is non-zero, that is the clock rate
	   for statistics gathering used by the kernel.
	   Otherwise, use hz
	 */
	lseek(VmonKmem, (long)RawStats[PHZ].n_value,0);
	read(VmonKmem, (char *)&hertz, (int)sizeof(hertz));
	if (hertz == 0) {
	    lseek(VmonKmem, (long)RawStats[HZ].n_value,0);
	    read(VmonKmem, (char *)&hertz, (int) sizeof(hertz));
	}
	/* XXX - if hertz is still 0, use the BSD default of 100 */
	if (hertz == 0)
	    hertz = 100;
    }

    if (VmonKmem != -1) {
        lseek(VmonKmem, (long)RawStats[CPTIME].n_value,0);
	read(VmonKmem, (char *)busy, (int)sizeof(busy));
	InitStats->SystemCPU = busy[CP_SYS];
	InitStats->UserCPU = (busy[CP_USER] + busy[CP_NICE]);
	InitStats->IdleCPU = busy[CP_IDLE];
    }
    /* Cache high water marks */
    /* At the present, this information won't be collected, because in the
       disconnected state, this information seems meaningless. */
#endif
}


void volent::UpdateStatsVSR(vsr *v) {
#ifdef __BSD44__
    long busy[CPUSTATES];
    SessionStatistics *Stats = &v->stats;
    SessionStatistics *InitStats = &v->initstats;

    /* Gather ClientModifyLog information */
    /* (The Start parameters were already taken) */
    Stats->BytesEnd = CML.logBytes();
    Stats->BytesHighWater = CML.logBytesHighWater();
    Stats->EntriesEnd = CML.count();
    Stats->EntriesHighWater = CML.countHighWater();

    /* Get volent records */
    Stats->RecordsCancelled = RecordsCancelled - InitStats->RecordsCancelled;
    Stats->RecordsCommitted = RecordsCommitted - InitStats->RecordsCommitted;
    Stats->RecordsAborted = RecordsAborted - InitStats->RecordsAborted;
    Stats->FidsRealloced = FidsRealloced - InitStats->FidsRealloced;
    Stats->BytesBackFetched = BytesBackFetched - InitStats->BytesBackFetched;

    /* Gather CPU usage */
    if (VmonKmem == 0) {
	nlist(VMUNIX, RawStats);
	if(RawStats[0].n_type == 0) {
	    VmonKmem = -1;
	}
	VmonKmem = open("/dev/kmem", 0, 0);
	if (VmonKmem <= 0) {
	    VmonKmem = -1;
	}
	/* if phz is non-zero, that is the clock rate
	   for statistics gathering used by the kernel.
	   Otherwise, use hz
	 */
	lseek(VmonKmem, (long)RawStats[PHZ].n_value,0);
	read(VmonKmem, (char *)&hertz, (int)sizeof(hertz));
	if (hertz == 0) {
	    lseek(VmonKmem, (long)RawStats[HZ].n_value,0);
	    read(VmonKmem, (char *)&hertz, (int)sizeof(hertz));
	}
	/* XXX - if hertz is still 0, use the BSD default of 100 */
	if (hertz == 0)
	    hertz = 100;
    }

    if (VmonKmem != -1 && InitStats->IdleCPU != 0) {
        lseek(VmonKmem, (long)RawStats[CPTIME].n_value,0);
        read(VmonKmem, (char *)busy, (int)sizeof(busy));
	Stats->SystemCPU = (busy[CP_SYS] - InitStats->SystemCPU)/hertz;
	Stats->UserCPU = (busy[CP_USER] + busy[CP_NICE] - InitStats->UserCPU)/hertz;
	Stats->IdleCPU = (busy[CP_IDLE] - InitStats->IdleCPU)/hertz;
    }

    /* Cache high water marks */
    /* At the present, this information won't be collected, because in the
       disconnected state, this information seems meaningless. */
#endif
}



vsr::vsr(vuid_t Uid) {
    LOG(10, ("vsr::vsr: uid = %d\n", Uid));

    uid = Uid;
    starttime = Vtime();
    endtime = 0;
    cetime = 0;

    /* think about doing some flagging trick here -- bnoble */
    memset((void *)&events, 0, (int)sizeof(VmonSessionEventArray));

#ifdef	VENUSDEBUG
    allocs++;
#endif	VENUSDEBUG
}


/* 
 * we don't support assignments to objects of this type.
 * bomb in an obvious way if it inadvertently happens.
 */
vsr::vsr(vsr& v) {
    abort();
}


int vsr::operator=(vsr& v) {
    abort();
    return(0);
}


vsr::~vsr() {
#ifdef	VENUSDEBUG
    deallocs++;
#endif	VENUSDEBUG

    LOG(10, ("vsr::~vsr: uid = %d\n", uid));
}


void vsr::RecordEvent(int opcode, int result, RPC2_Unsigned elapsed) {
    VmonSessionEvent *se = &((&events.Event0)[opcode]);
    se->Opcode = opcode;  /* only set the ones we enter */
    if (result == 0) {
	se->SuccessCount++;
	se->SigmaT += elapsed;
	*((float *)&se->SigmaTSquared) += (float)elapsed * (float)elapsed;
    }
    else {
	se->FailureCount++;
    }
}


void vsr::print() {
    print(stdout);
}


void vsr::print(FILE *fp) {
    fflush(fp);
    print(fileno(fp));
}


void vsr::print(int afd) {
    fdprint(afd, "\t\t%#08x : uid = %d\n", (long)this, uid);
}
