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



#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <sys/time.h>
#include <setjmp.h>

#ifdef __cplusplus
}
#endif __cplusplus


/* from venus */
#include "comm.h"
#include "venus.private.h"
#include "venusrecov.h"
#include "vproc.h"

/*
 *
 *    Implementation of the Venus Communications daemons.
 *
 *    There are two separate daemons:
 *       1. A "probe" daemon, which periodically and on-demand sends probe messages 
 *	    to servers and checks the health of the network.
 *       2. A "vsg" daemon, which is responsible for maintenance of the VSGDB.
 *
 */


/*  *****  Probe Daemon  *****  */

static const int ProbeDaemonStackSize = 40960;
static const int T1Interval = 12 * 60;	     /* "up" servers */
static const int T2Interval = 4 * 60;	     /* "down" servers */
static const int CommCheckInterval = 5;
static const int ProbeInterval	= CommCheckInterval; /* min(T1, T2, Comm) */

static char probe_sync;

void PROD_Init() {
    (void)new vproc("ProbeDaemon", (PROCBODY)&ProbeDaemon,
		     VPT_ProbeDaemon, ProbeDaemonStackSize);    
}

void ProbeDaemon() {
    /* Hack!  Vproc must yield before data members become valid! */
    VprocYield();

    vproc *vp = VprocSelf();
    RegisterDaemon(ProbeInterval, &probe_sync);

    long curr_time = 0;
    long LastT1Check = 0;
    long LastT2Check = 0;
    long LastCommCheck = 0;

    for (;;) {
	VprocWait(&probe_sync);

	START_TIMING();
	curr_time = Vtime();

	/* probe the servers.  ServerProbe updates the last probed times. */
	if ((curr_time - LastT1Check >= T1Interval) ||
	    (curr_time - LastT2Check >= T2Interval)) {

	    ServerProbe(&LastT1Check, &LastT2Check);
	}

	/* refresh network measurements for each server */
	if (curr_time - LastCommCheck >= CommCheckInterval) {
	    CheckServerBW(curr_time);

	    LastCommCheck = curr_time;
	}

	END_TIMING();
	LOG(10, ("ProbeDaemon: elapsed = %3.1f (%3.1f, %3.1f)\n",
		 elapsed, elapsed_ru_utime, elapsed_ru_stime));

	/* Bump sequence number. */
	vp->seq++;
    }
}


/*
 * Determine which servers should be probed, and probe them.
 * lasttupp and lastdownp, if set, contain the last probe times of up 
 * and down servers, respectively (down servers are probed more often 
 * than up servers).  The probe of a given server may be suppressed if 
 * the last probe time is recent enough.  
 *
 * This routine updates the last probe times.
 */
void ServerProbe(long *lastupp, long *lastdownp) {
    LOG(1, ("ServerProbe: lastup = %d, lastdown = %d\n", 
	    lastupp?*lastupp:0, lastdownp?*lastdownp:0));

    CODA_ASSERT((lastupp && lastdownp) || (!lastupp && !lastdownp));

    int upprobe = 0, downprobe = 0;

    /* Mark the servers to be probed */
    {
	long curr_time = Vtime();	
	long minlastup = curr_time;	/* assume everyone gets probed now. */
	long minlastdown = curr_time;

	srv_iterator next;
	srvent *s;
	while ((s = next())) {
	    /* 
	     * We will probe the server if the check is being forced (no 
	     * times sent in), or if the server has not been heard from 
	     * within the appropriate interval. Otherwise, pretend a probe
	     * occurred at the "last live" time.
	     */
	    struct timeval lastword;

	    (void) s->GetLiveness(&lastword);		/* if error, lastword = 0 */
	    if ((lastupp == 0 && lastdownp == 0) ||	/* forced check */
		(s->ServerIsUp() && 
		 (lastword.tv_sec + T1Interval <= curr_time) && /* heard recently */
		 (*lastupp + T1Interval <= curr_time)) ||	/* up interval expired */
		(s->ServerIsDown() && 
		 (lastword.tv_sec + T2Interval <= curr_time) && /* heard recently */
		 (*lastdownp + T2Interval <= curr_time))) {	/* down interval expired */

		s->probeme = 1;
		if (s->ServerIsUp()) upprobe = 1;
		else downprobe = 1;
	    } else {
		LOG(1, ("Suppressing probe to %s, %s %d sec ago\n",
			s->name, timerisset(&lastword)?"heard from":"probed",
			timerisset(&lastword)?(curr_time-lastword.tv_sec):
			(s->ServerIsUp()?(curr_time-*lastupp):
			 (curr_time-*lastdownp))));
		/* Keep track of the least recent probe time. */
		if (s->ServerIsUp()) {
		    if (lastword.tv_sec < minlastup) minlastup = lastword.tv_sec;
		} else {
		    if (lastword.tv_sec < minlastdown) minlastdown = lastword.tv_sec;
		}
	    }
	}

	/* Update the last probe times, if supplied */
	if (lastdownp && (minlastdown > *lastdownp))
	    *lastdownp = minlastdown;
	if (lastupp && (minlastup > *lastupp))
	    *lastupp = minlastup;
    }

    /* Probe t1 (i.e., "up") servers. */
    char upsync = 0;
    if (upprobe)
	(void)new probeslave(ProbeUpServers, 0, 0, &upsync);

    /* Probe t2 (i.e., "down") servers. */
    char downsync = 0;
    if (downprobe)
	(void)new probeslave(ProbeDownServers, 0, 0, &downsync);

    /* Reap t1 slave. */
    if (upprobe)
	while (upsync == 0)
	    VprocWait(&upsync);

    /* Reap t2 slave. */
    if (downprobe)
	while (downsync == 0)
	    VprocWait(&downsync);
}


/*  *****  VSG Daemon  *****  */

static const int VSGDaemonStackSize = 8192;
static const int VSGDaemonInterval = 300;
static const int VSGGetDownInterval = 300;

static char vsg_sync;

void VSGD_Init() {
    (void)new vproc("VSGDaemon", (PROCBODY) &VSGDaemon,
		     VPT_VSGDaemon, VSGDaemonStackSize);
}

void VSGDaemon() {
    /* Hack!  Vproc must yield before data members become valid! */
    VprocYield();

    vproc *vp = VprocSelf();
    RegisterDaemon(VSGDaemonInterval, &vsg_sync);

    long curr_time = Vtime();
    long LastGetDown = curr_time;

    for (;;) {
	VprocWait(&vsg_sync);

	START_TIMING();
	curr_time = Vtime();

	/* Periodically free up cache resources. */
	if (curr_time - LastGetDown >= VSGGetDownInterval) {
	    VSGDB->GetDown();

	    LastGetDown = curr_time;
	}

	END_TIMING();
	LOG(10, ("VSGDaemon: elapsed = %3.1f (%3.1f, %3.1f)\n",
		 elapsed, elapsed_ru_utime, elapsed_ru_stime));

	/* Bump sequence number. */
	vp->seq++;
    }
}


void vsgdb::GetDown() {
    LOG(100, ("vsgdb::GetDown: \n"));

    /* We need to GC unreferenced VSG entries when some reasonable threshold is passed. */
    /* The threshold is assumed to be high enough that it will almost never be hit. */
    /* Therefore, we don't do anything special to prioritize the set of candidate entries. */
#define	VSGThreshold	(CacheFiles >> 4)
    if (VSGDB->htab.count() >= VSGThreshold) {
	vsg_iterator next;
	vsgent *v;
	int readahead = 0;
	while ((VSGDB->htab.count() >= VSGThreshold) && (readahead || (v = next()))) {
	    readahead = 0;

	    if (v->refcnt > 0) continue;

	    vsgent *tv = 0;
	    readahead = ((tv = next()) != 0);

	    LOG(10, ("vsgdb::GetDown: GC'ing %x\n", v->Addr));
	    Recov_BeginTrans();
	    delete v;
	    Recov_EndTrans(0);
	    if (readahead) v = tv;
	}
    }

    /* The number of referenced VSGs is bounded by the number of referenced volumes, which in turn is */
    /* bounded by the number of allocated fsobjs.  However, it is extremely unlikely that this bound will */
    /* ever be hit in the course of normal operation.  It is far more likely that if the bound is reached then */
    /* we have a programming error.  Thus, we panic in such event. */
    if (VSGDB->htab.count() >= CacheFiles)
	CHOKE("vsgdb::GetDown: vsg entries >= CacheFiles");
}
