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
 * Implementation of the Venus Volume Daemon.
 *
 *  This daemon has the following responsibilities:
 *      1. Validating cached volname --> volid bindings
 *      2. Ensuring that the volume cache does not exceed its resource limits
 *      3. Sending batched COP2 messages to the AVSG
 *      4. Triggering write-back of modified volumes upon reconnection (aka reintegration).
 *      5. Flushing Volume Session Records (VSRs).
 *
 *     Note that COP2 messages are piggybacked on normal CFS calls whenever possible.
 */


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>

#ifdef __cplusplus
}
#endif __cplusplus

/* interfaces */
#include <vice.h>

/* from venus */
#include "local.h"
#include "venus.private.h"
#include "venusrecov.h"
#include "venusvol.h"
#include "vproc.h"


static const int VolDaemonStackSize =  0xffff; /* 64k stack, because of all
						   the MAXPATHLEN stuff in
						   vdb::CheckPoint. JH */
static const int VolDaemonInterval = 5;
static const int VolumeCheckInterval = 120 * 60;
static const int VolGetDownInterval = 5 * 60;
static const int COP2CheckInterval = 5;
static const int COP2Window = 10;
static const int VolFlushVSRsInterval = 240 * 60;
static const int VolCheckPointInterval = 10 * 60;
static	const int UserRPMInterval = 15 * 60;  
static const int LocalSubtreeCheckInterval = 10 * 60;
static const int VolTrickleReintegrateInterval = 10;
static const int AutoWBPermitRequestInterval = 30;

char vol_sync;

void VOLD_Init() {
    (void)new vproc("VolDaemon", (PROCBODY) &VolDaemon,
		     VPT_VolDaemon, VolDaemonStackSize);
}

void VolDaemon() {

    /* Hack!  Vproc must yield before data members become valid! */
    VprocYield();

    vproc *vp = VprocSelf();
    RegisterDaemon(VolDaemonInterval, &vol_sync);

    unsigned long curr_time = Vtime();

    /* Avoid checks on first firing! */
    unsigned long LastGetDown = curr_time;
    unsigned long LastCOP2Check = curr_time;
    unsigned long LastCheckPoint = curr_time;
    unsigned long LastRPM = curr_time;
    unsigned long LastLocalSubtree = curr_time;
    unsigned long LastTrickleReintegrate = curr_time;
    unsigned long LastWBPermitRequest = curr_time;

    for (;;) {
	VprocWait(&vol_sync);

	START_TIMING();
	curr_time = Vtime();

	{
	    /* always check if there are transitions to be taken */
	    VDB->TakeTransition();

	    /* Periodically and on-demand free up cache resources. */
	    if (curr_time - LastGetDown >= VolGetDownInterval) {
		LastGetDown = curr_time;

		VDB->GetDown();
	    }

	    /* Send off expired COP2 entries. */
	    if (curr_time - LastCOP2Check >= COP2CheckInterval) {
		LastCOP2Check = curr_time;

		VDB->FlushCOP2();
	    }

#if 0
	    /* Flush Volume Session Records (VSRs). */
	    if (curr_time - LastFlushVSRs >= VolFlushVSRsInterval) {
		LastFlushVSRs = curr_time;

		VDB->FlushVSR();
	    }
#endif

	    /* Checkpoint modify logs if necessary. */
	    if (curr_time - LastCheckPoint >= VolCheckPointInterval) {
		LastCheckPoint = curr_time;

		VDB->CheckPoint(curr_time);  /* loss of symmetry. sigh. */
	    }

	    /* Propagate updates, if any. */
	    if (curr_time - LastTrickleReintegrate >= VolTrickleReintegrateInterval) {
		LastTrickleReintegrate = curr_time;
		    
		TrickleReintegrate();
	    }

	    /* Print "reintegrate pending" messages, if necessary. */
	    if (curr_time - LastRPM >= UserRPMInterval) {
		LastRPM = curr_time;
		    
		VDB->CheckReintegratePending();
	    }

	    /* Check LocalSubtree */
	    if (curr_time - LastLocalSubtree >= LocalSubtreeCheckInterval) {
		LastLocalSubtree = curr_time;
		
		VDB->CheckLocalSubtree();
	    }
	    
	    /* Ask for a writeback permit if I don't have one */
	    if (curr_time - LastWBPermitRequest >= AutoWBPermitRequestInterval) {
		LastWBPermitRequest = curr_time;
		VDB->AutoRequestWBPermit;
	    }
	}

	END_TIMING();
	LOG(10, ("VolDaemon: elapsed = %3.1f (%3.1f, %3.1f)\n",
		 elapsed, elapsed_ru_utime, elapsed_ru_stime));

	/* Bump sequence number. */
	vp->seq++;
    }
}


/* local-repair modification */
void vdb::GetDown() {
    LOG(100, ("vdb::GetDown: \n"));

    /* We need to GC unreferenced volume entries when some reasonable threshold is passed. */
    /* The threshold is assumed to be high enough that it will almost never be hit. */
    /* Therefore, we don't do anything special to prioritize the set of candidate entries. */
#define	VOLThreshold	(CacheFiles >> 2)
    if (VDB->htab.count() >= VOLThreshold) {
	vol_iterator next;
	volent *v;
	int readahead = 0;
	while ((VDB->htab.count() >= VOLThreshold) && (readahead || (v = next()))) {
	    readahead = 0;
	    if (FID_VolIsFake(v->vid)) continue;
	    if (v->refcnt > 0) continue;
	    
	    volent *tv = 0;
	    readahead = ((tv = next()) != 0);

	    LOG(10, ("vdb::GetDown: GC'ing (%x, %s)\n", v->vid, v->name));
	    Recov_BeginTrans();
	    delete v;
	    Recov_EndTrans(0);

	    if (readahead) v = tv;
	}
    }

    /* The number of referenced volumes is bounded by the number of allocated fsobjs.  However, it is */
    /* extremely unlikely that this bound will ever be hit in the course of normal operation.  It is far more */
    /* likely that if the bound is reached then we have a programming error.  Thus, we panic in such event. */
    if (VDB->htab.count() >= CacheFiles)
	CHOKE("vdb::GetDown: volume entries >= CacheFiles");
}


/* local-repair modification */
void vdb::FlushCOP2() {
    LOG(100, ("vdb::FlushCOP2: \n"));

    /* For each volume. */
    vol_iterator vnext;
    volent *v;
    while ((v = vnext())) {
	if (FID_VolIsFake(v->vid)) continue;
	if (v->IsReplicated()) {
	    for (;;) {
		int code = 0;

		if (v->Enter((VM_OBSERVING | VM_NDELAY), V_UID) == 0) {
		    code = v->FlushCOP2(COP2Window);
		    v->Exit(VM_OBSERVING, V_UID);
		}

		if (code != ERETRY) break;
	    }
	}
    }
}


/* local-repair modification */
/* XXX Use this routine to "touch" all volumes periodically so that volume state changes get taken! */
void vdb::TakeTransition() {
    LOG(100, ("vdb::TakeTransition: \n"));

    /* For each volume. */
    vol_iterator vnext;
    volent *v;
    while ((v = vnext())) {
	if (!FID_VolIsFake(v->vid)) 
		continue;

	LOG(1000, ("vdb::TakeTransition: checking %s\n", v->name));
	if (v->Enter((VM_OBSERVING | VM_NDELAY), V_UID) == 0) {
	    v->Exit(VM_OBSERVING, V_UID);
	}
    }
}

/* local-repair modification */
void vdb::FlushVSR() {
    LOG(100, ("vdb::FlushVSR: \n"));

    /* For each volume. */
    vol_iterator vnext;
    volent *v;
    while ((v = vnext())) {
	if (FID_VolIsFake(v->vid)) 
		continue;
	v->FlushVSRs(VSR_FLUSH_NOT_HARD);
    }
}


/* local-repair modification */
/* 
 * periodically checkpoint any volumes with non-empty CMLs 
 * if the CML has changed since the last checkpoint interval.
 */
void vdb::CheckPoint(unsigned long curr_time) {
    LOG(100, ("vdb::CheckPoint: \n"));

    /* For each volume. */
    vol_iterator vnext;
    volent *v;

    while ((v = vnext())) {
	unsigned long lmTime= 0;

        /* check if the volume contains cmlent to be repaired */
        if (v->ContainUnrepairedCML()) {
	    eprint("volume %s has unrepaired local subtree(s), skip checkpointing CML!\n",
		   v->name);
	    /* skip checkpointing if there is unrepaired mutations */
	} else if (v->LastMLETime(&lmTime) == 0 && 
		   (lmTime > curr_time - VolCheckPointInterval)) {

	    LOG(1000, ("vdb::CheckPoint: checking %s\n", v->name));
	    if (CheckLock(&v->CML_lock)) {    
		eprint("volume %s CML is busy, skip checkpoint!\n", v->name);
	    } else if (v->Enter((VM_OBSERVING | VM_NDELAY), V_UID) == 0) {
		 v->CheckPointMLEs(V_UID, (char *)NULL);
		 v->Exit(VM_OBSERVING, V_UID);
	    }
	}
    }
}


void vdb::CheckReintegratePending() {
    LOG(100, ("vdb::CheckReintegratePending: \n"));

    /* For each volume. */
    vol_iterator vnext;
    volent *v;
    while ((v = vnext()))
	v->CheckReintegratePending();
}


void vdb::CheckLocalSubtree()
{
    vol_iterator next;
    volent *v;
    while ((v = next()))
      v->CheckLocalSubtree();
}

void vdb::AutoRequestWBPermit()
{
    vol_iterator next;
    volent *v;
    while ((v = next()))
	if (v->flags.autowriteback && !v->flags.writebacking)
	    v->EnterWriteback();
}


/* Note: no longer in class vdb, since VolDaemon isn't (Satya, 5/20/95) */
void TrickleReintegrate() {
    LOG(100, ("TrickleReintegrate(): \n"));
    
    /* For each volume. */
    vol_iterator vnext;
    volent *v;

    while ((v = vnext())) {
	if (FID_VolIsFake(v->vid)) 
		continue;

	LOG(1000, ("TrickleReintegrate: checking %s\n", v->name));
	if (v->Enter((VM_OBSERVING | VM_NDELAY), V_UID) == 0) {
	    /* force a connectivity check? */
	    /* try to propagate updates from this volume.  */
	    if (v->ReadyToReintegrate())
		::Reintegrate(v);
	    v->Exit(VM_OBSERVING, V_UID);
	}
    }
}
