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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/venus/vol_daemon.cc,v 4.2 1998/06/16 15:43:14 braam Exp $";
#endif /*_BLURB_*/







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
#include "simulate.h"
#include "venus.private.h"
#include "venusrecov.h"
#include "venusvol.h"
#include "vproc.h"


PRIVATE const int VolDaemonStackSize =  0xc000;
PRIVATE const int VolDaemonInterval = 5;
PRIVATE const int VolumeCheckInterval = 120 * 60;
PRIVATE const int VolGetDownInterval = 5 * 60;
PRIVATE const int COP2CheckInterval = 5;
PRIVATE const int COP2Window = 10;
PRIVATE const int VolFlushVSRsInterval = 240 * 60;
PRIVATE const int VolCheckPointInterval = 10 * 60;
PRIVATE	const int UserRPMInterval = 15 * 60;  
PRIVATE const int LocalSubtreeCheckInterval = 10 * 60;
PRIVATE const int VolTrickleReintegrateInterval = 10;

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
    unsigned long LastVolumeCheck = curr_time;
    unsigned long LastGetDown = curr_time;
    unsigned long LastCOP2Check = curr_time;
    unsigned long LastFlushVSRs = curr_time;
    unsigned long LastCheckPoint = curr_time;
    unsigned long LastRPM = curr_time;
    unsigned long LastLocalSubtree = curr_time;
    unsigned long LastTrickleReintegrate = curr_time;

    for (;;) {
	VprocWait(&vol_sync);

	START_TIMING();
	curr_time = Vtime();

	{
	    /* always check if there are transitions to be taken */
	    VDB->TakeTransition();

	    /* 
	     * Periodically validate volume data to recognize
	     * new releases of read-only volumes.
	     */
	    if (curr_time - LastVolumeCheck >= VolumeCheckInterval) {
		LastVolumeCheck = curr_time;

		VDB->Validate();
	    }

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

	    /* Flush Volume Session Records (VSRs). */
	    if (curr_time - LastFlushVSRs >= VolFlushVSRsInterval) {
		LastFlushVSRs = curr_time;

		VDB->FlushVSR();
	    }

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
	}

	END_TIMING();
	LOG(10, ("VolDaemon: elapsed = %3.1f (%3.1f, %3.1f)\n",
		 elapsed, elapsed_ru_utime, elapsed_ru_stime));

	/* Bump sequence number. */
	vp->seq++;
    }
}


/* This should be called vdb::CheckVolumes()! */
void vdb::Validate() {
    LOG(100, ("vdb::Validate: \n"));

    FSDB->InvalidateMtPts();
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
	    if (v->vid == LocalFakeVid) continue;
	    if (v->refcnt > 0) continue;
	    
	    volent *tv = 0;
	    readahead = ((tv = next()) != 0);

	    LOG(10, ("vdb::GetDown: GC'ing (%x, %s)\n", v->vid, v->name));
	    TRANSACTION(
		delete v;
	    )

	    if (readahead) v = tv;
	}
    }

    /* The number of referenced volumes is bounded by the number of allocated fsobjs.  However, it is */
    /* extremely unlikely that this bound will ever be hit in the course of normal operation.  It is far more */
    /* likely that if the bound is reached then we have a programming error.  Thus, we panic in such event. */
    if (VDB->htab.count() >= CacheFiles)
	Choke("vdb::GetDown: volume entries >= CacheFiles");
}


/* local-repair modification */
void vdb::FlushCOP2() {
    if (Simulating) return;

    LOG(100, ("vdb::FlushCOP2: \n"));

    /* For each volume. */
    vol_iterator vnext;
    volent *v;
    while (v = vnext()) {
	if (v->vid == LocalFakeVid) continue;
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
    if (Simulating) return;

    LOG(100, ("vdb::TakeTransition: \n"));

    /* For each volume. */
    vol_iterator vnext;
    volent *v;
    while (v = vnext()) {
	if (v->vid == LocalFakeVid) continue;

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
    while (v = vnext()) {
	if (v->vid == LocalFakeVid) continue;
	v->FlushVSRs(VSR_FLUSH_NOT_HARD);
    }
}


/* local-repair modification */
/* 
 * periodically checkpoint any volumes with non-empty CMLs 
 * if the CML has changed since the last checkpoint interval.
 */
void vdb::CheckPoint(unsigned long curr_time) {
    if (Simulating) return;

    LOG(100, ("vdb::CheckPoint: \n"));

    /* For each volume. */
    vol_iterator vnext;
    volent *v;

    while (v = vnext()) {
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
		 v->CheckPointMLEs(V_UID, NULL);
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
    while (v = vnext())
	v->CheckReintegratePending();
}


void vdb::CheckLocalSubtree()
{
    vol_iterator next;
    volent *v;
    while (v = next())
      v->CheckLocalSubtree();
}


/* Note: no longer in class vdb, since VolDaemon isn't (Satya, 5/20/95) */
void TrickleReintegrate() {
    if (Simulating) return;

    LOG(100, ("TrickleReintegrate(): \n"));

    /* For each volume. */
    vol_iterator vnext;
    volent *v;
    while (v = vnext()) {
	if (v->vid == LocalFakeVid) continue;

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
