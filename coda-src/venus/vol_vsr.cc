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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/venus/vol_vsr.cc,v 4.2 1997/02/26 16:03:38 rvb Exp $";
#endif /*_BLURB_*/







/*
 *
 * Implementation of the Venus Volume Session Record (VSR) facility.
 *
 */


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <string.h>

#ifdef __MACH__
#include <sys/dk.h>
#endif /* __MACH__ */
#if    defined(__BSD44__)
#include <sys/dkstat.h>
#endif /* __BSD44__ */
#ifdef	__linux__
#include "dkstat.h"
#endif 
#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif

#include <fcntl.h>

#if defined(__GLIBC__) && __GLIBC__ >= 2
#include <libelf/nlist.h>
#else
#include <nlist.h>
/* nlist.h defines this function but it isnt getting included because it is
   guarded by an ifdef of CMU which isnt getting defined.  XXXXX pkumar 6/13/95 */ 
extern int nlist(const char*, struct nlist[]);
#endif
    
#include <rpc2.h>

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


/* **** PRIVATE Variable **** */
PRIVATE int VmonKmem = 0;
PRIVATE int hertz = 0;

#ifdef	__MACH__
#define VMUNIX "/mach"
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

/* Raw Statistic Entry. */
PRIVATE struct nlist RawStats[] = 
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

#ifdef VENUSDEBUG
int vsr::allocs = 0;
int vsr::deallocs = 0;
#endif VENUSDEBUG

vsr *volent::GetVSR(vuid_t uid) {
    ASSERT(vid != LocalFakeVid);
    LOG(100, ("volent::GetVSR: vol = %x, session = %d, uid = %d\n",
	       vid, VsrUnique, uid));

    /* Flush VSRs when AVSG has changed. */
    if (GetAVSG() != VsrUnique) {
	FlushVSRs(VSR_FLUSH_HARD);
	VsrUnique = GetAVSG((unsigned long *)&AVSG);
    }

    /* Check for an existing VSR for this user. */
    olist_iterator next(*vsr_list);
    vsr *v;
    while (v = (vsr *)next())
	if (uid == v->uid) return(v);

    /* Construct a new one. */
    v = new vsr(uid);
    InitStatsVSR(v);
    vsr_list->append(v);
    return(v);
}


void volent::PutVSR(vsr *v) {
    ASSERT(vid != LocalFakeVid);
    LOG(100, ("volent::PutVSR: vol = %x, session = %d, uid = %d\n",
	       vid, VsrUnique, v->uid));

    ASSERT(v->cetime == 0);
    v->endtime = Vtime();
    UpdateStatsVSR(v);
}


void volent::FlushVSRs(int hard) {
    ASSERT(vid != LocalFakeVid);
    LOG(100, ("volent::FlushVSRs: vol = %x, session = %d, hard = %d\n", 
	      vid, VsrUnique, hard));

    if (hard) {
       /* 
        * A Hard VSR Flush means that the session is KNOWN TO HAVE ENDED.  
        * We get the record off the list, ship it off to the data collector,
        * and then remove it from the list.
        */
	vsr *v;
	while (v = (vsr *)vsr_list->get()) {
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
	while (v = (vsr *)vnext()) {
	    VmonSessionEventArray *na = new(VmonSessionEventArray);
	    bcopy(&(v->events),na,(int)sizeof(VmonSessionEventArray));
	    /* Convert SigmaTSquared from floating-point milliseconds to fixed-point seconds. */
	    for (int i = 0; i < (sizeof(VmonSessionEventArray) / sizeof(VmonSessionEvent)); i++) {
		VmonSessionEvent *se = &((&(na->Event0))[i]);
		se->SigmaTSquared = (RPC2_Unsigned)((*((float *)&se->SigmaTSquared) + 500) / 1000);
	    }

	    SessionStatistics *ss = new(SessionStatistics);
	    bcopy(&v->stats, ss, (int)sizeof(SessionStatistics));

            CacheStatistics *cs = new (CacheStatistics);
            bcopy(&v->cachestats, cs, (int)sizeof(CacheStatistics));

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
    long busy[CPUSTATES];
    SessionStatistics *Stats = &v->stats;
    SessionStatistics *InitStats = &v->initstats;
    CacheStatistics *CacheStats = &v->cachestats;

    bzero(Stats, (int) sizeof(SessionStatistics));
    bzero(InitStats, (int) sizeof(SessionStatistics));
    bzero(CacheStats, (int) sizeof(CacheStatistics));

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
#ifndef	__linux__
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
#ifndef	__linux__
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
    bzero(&events, (int)sizeof(VmonSessionEventArray));

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


vsr::operator=(vsr& v) {
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
