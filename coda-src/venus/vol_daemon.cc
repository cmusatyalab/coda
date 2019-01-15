/* BLURB gpl

                           Coda File System
                              Release 7

          Copyright (c) 1987-2019 Carnegie Mellon University
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
 *
 *     Note that COP2 messages are piggybacked on normal CFS calls whenever possible.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#ifdef __cplusplus
}
#endif

/* interfaces */
#include <vice.h>

/* from venus */
#include "local.h"
#include "venus.private.h"
#include "venusrecov.h"
#include "venusvol.h"
#include "vproc.h"

static const int VolDaemonStackSize    = 0xffff; /* 64k stack, because of all
						   the MAXPATHLEN stuff in
						   vdb::CheckPoint. JH */
static const int VolDaemonInterval     = 5;
static const int VolumeCheckInterval   = 120 * 60;
static const int VolGetDownInterval    = 5 * 60;
static const int COP2CheckInterval     = 5;
static const int COP2Window            = 10;
static const int VolCheckPointInterval = 10 * 60;
static const int LocalSubtreeCheckInterval     = 10 * 60;
static const int VolTrickleReintegrateInterval = 10;

char voldaemon_sync;

void VOLD_Init(void)
{
    (void)new vproc("VolDaemon", &VolDaemon, VPT_VolDaemon, VolDaemonStackSize);
}

void VolDaemon(void)
{
    /* Hack!  Vproc must yield before data members become valid! */
    VprocYield();

    vproc *vp = VprocSelf();
    RegisterDaemon(VolDaemonInterval, &voldaemon_sync);

    time_t curr_time = Vtime();

    /* Avoid checks on first firing! */
    time_t LastGetDown            = 0; /* except for GC'ing empty volumes */
    time_t LastCOP2Check          = curr_time;
    time_t LastCheckPoint         = curr_time;
    time_t LastTrickleReintegrate = curr_time;

    for (;;) {
        VprocWait(&voldaemon_sync);

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

            /* Checkpoint modify logs if necessary. */
            if (curr_time - LastCheckPoint >= VolCheckPointInterval) {
                VDB->CheckPoint(curr_time); /* loss of symmetry. sigh. */
                LastCheckPoint = curr_time;
            }

            /* Propagate updates, if any. */
            if (curr_time - LastTrickleReintegrate >=
                VolTrickleReintegrateInterval) {
                LastTrickleReintegrate = curr_time;

                TrickleReintegrate();
            }
        }

        END_TIMING();
        LOG(10, ("VolDaemon: elapsed = %3.1f\n", elapsed));

        /* Bump sequence number. */
        vp->seq++;
    }
}

/* local-repair modification */
void vdb::GetDown()
{
    LOG(10, ("vdb::GetDown: \n"));

    Recov_BeginTrans();
    { /* find reclaimable replicated volumes */
        repvol_iterator next;
        repvol *v;
        while ((v = next())) {
            if (v->refcnt == 1 && v->CML.count() == 0)
                LOG(10, ("vdb::GetDown destroying %x\n", v->GetVolumeId()));
        }
    }
    { /* find reclaimable volume replicas */
        volrep_iterator next;
        volrep *v;
        while ((v = next()))
            if (v->refcnt == 1)
                LOG(10, ("vdb::GetDown destroying %x\n", v->GetVolumeId()));
    }
    Recov_EndTrans(0);
}

/* local-repair modification */
void vdb::FlushCOP2()
{
    LOG(100, ("vdb::FlushCOP2: \n"));

    /* For each volume. */
    repvol_iterator vnext;
    repvol *v;
    while ((v = vnext())) {
        for (;;) {
            int code = 0;

            if (!v->IsReplicated())
                continue;

            if (v->Enter((VM_OBSERVING | VM_NDELAY), V_UID) == 0) {
                code = v->FlushCOP2(COP2Window);
                v->Exit(VM_OBSERVING, V_UID);
            }

            if (code != ERETRY)
                break;
        }
    }
}

/* local-repair modification */
/* XXX Use this routine to "touch" all volumes periodically so that volume state changes get taken! */
void vdb::TakeTransition()
{
    LOG(100, ("vdb::TakeTransition: \n"));

    /* For each volume. */
    repvol_iterator rvnext;
    volrep_iterator vrnext;
    volent *v;
    while ((v = rvnext()) || (v = vrnext())) {
        if (v->IsLocalRealm())
            continue;
        LOG(1000, ("vdb::TakeTransition: checking %s\n", v->name));
        if (v->Enter((VM_OBSERVING | VM_NDELAY), V_UID) == 0)
            v->Exit(VM_OBSERVING, V_UID);
    }
}

/* local-repair modification */
/* 
 * periodically checkpoint any volumes with non-empty CMLs 
 * if the CML has changed since the last checkpoint interval.
 */
void vdb::CheckPoint(unsigned long curr_time)
{
    LOG(100, ("vdb::CheckPoint: \n"));

    /* For each volume. */
    repvol_iterator vnext;
    reintvol *v;

    while ((v = vnext())) {
        unsigned long lmTime = 0;

        /* check if the volume contains cmlent to be repaired */
        if (v->ContainUnrepairedCML()) {
            eprint(
                "volume %s has unrepaired local subtree(s), skip checkpointing CML!\n",
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

/* Note: no longer in class vdb, since VolDaemon isn't (Satya, 5/20/95) */
void TrickleReintegrate()
{
    LOG(100, ("TrickleReintegrate(): \n"));

    /* For each volume. */
    repvol_iterator r_next;
    nonrepvol_iterator nr_next;
    reintvol *v;
    while ((v = (reintvol *)r_next())) {
        LOG(1000, ("TrickleReintegrate: checking %s\n", v->GetName()));
        if (v->Enter((VM_OBSERVING | VM_NDELAY), V_UID) == 0) {
            /* force a connectivity check? */
            /* try to propagate updates from this volume.  */
            if (v->ReadyToReintegrate())
                ::Reintegrate(v);
            v->Exit(VM_OBSERVING, V_UID);
        }
    }
    while ((v = nr_next())) {
        LOG(1000, ("TrickleReintegrate: checking %s\n", v->GetName()));
        if (v->Enter((VM_OBSERVING | VM_NDELAY), V_UID) == 0) {
            /* force a connectivity check? */
            /* try to propagate updates from this volume.  */
            if (v->ReadyToReintegrate())
                ::Reintegrate(v);
            v->Exit(VM_OBSERVING, V_UID);
        }
    }
}
