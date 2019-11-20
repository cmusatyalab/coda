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
 *    Implementation of the Venus File-System Object (fso) Daemon.
 *
 *    ToDo:
 *       1. GetDown of symbolic links needs implemented
 *       2. Solve "owrite problem" by having kernel MiniCache reserve blocks
 *       3. Loop in GetDown to account for hierarchical effects?
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <sys/types.h>
#include <struct.h>

#ifdef __cplusplus
}
#endif

/* from util */
#include <dlist.h>

/* from venus */
#include "fso.h"
#include "venus.private.h"
#include "venusrecov.h"
#include "vproc.h"
#include "worker.h"

/* *****  Private constants  ***** */

static const int FSODaemonInterval   = 5;
static const int GetDownInterval     = 30;
static const int FlushRefVecInterval = 90;
static const int FSODaemonStackSize  = 32768;
static time_t LastGetDown            = 0;

/* ***** Private variables  ***** */

static char fsdaemon_sync;

/* ***** Public functions ***** */

/* wake the fso daemon so that it can free up some FSOs */
void FSOD_ReclaimFSOs(void)
{
    LastGetDown = 0;
    VprocSignal(&fsdaemon_sync);
}

/* ***** Private routines  ***** */

void FSODaemon(void)
{
    /* Hack!  Vproc must yield before data members become valid! */
    VprocYield();

    vproc *vp = VprocSelf();
    RegisterDaemon(FSODaemonInterval, &fsdaemon_sync);

    long LastFlushRefVec = 0;

    for (;;) {
        VprocWait(&fsdaemon_sync);

        START_TIMING();
        time_t curr_time = Vtime();

        /* Periodic events. */
        {
            /* Check cache limits. */
            if (curr_time - LastGetDown >= GetDownInterval) {
                Recov_BeginTrans();
                FSDB->GetDown();
                Recov_EndTrans(MAXFP);
                LastGetDown = curr_time;
            }

            /* Flush reference vector. */
            if (curr_time - LastFlushRefVec >= FlushRefVecInterval) {
                FSDB->FlushRefVec();
                LastFlushRefVec = curr_time;
            }
        }

        END_TIMING();
        LOG(10, ("FSODaemon: elapsed = %3.1f\n", elapsed));

        /* Bump sequence number. */
        vp->seq++;
    }
}

/* This is needed to "age" object priorities. */
void fsdb::RecomputePriorities(int Force)
{
    static long LastRefCounter = 0;

    int Count = (int)(RefCounter - LastRefCounter);
    if (Count == 0)
        return; /* don't recompute if nothing has been referenced */
    if (!Force && Count < /*MAXRC*/ (int)(MaxFiles >> 16))
        return;

    LastRefCounter = RefCounter;

    START_TIMING();
    int recomputes = 0;
    fso_iterator next(NL);
    fsobj *f;
    while ((f = next())) {
        recomputes++;
        f->ComputePriority(Force);
    }
    END_TIMING();
    LOG(100, ("fsdb::RecomputePriorities: recomputes = %d, elapsed = %3.1f\n",
              recomputes, elapsed));
}

/* MUST be called from within transaction! */
void fsdb::GarbageCollect()
{
    if (delq->count() > 0) {
        START_TIMING();

        int busy = 0;
        int gced = 0;

        dlist_iterator next(*delq);
        dlink *d, *dnext;
        dnext = next();
        while ((d = dnext) != NULL) {
            dnext    = next();
            fsobj *f = strbase(fsobj, d, del_handle);

            if (!DYING(f)) {
                f->print(GetLogFile());
                CHOKE("fsdb::GarbageCollect: !dying");
            }

            /* Skip busy and local entries. */
            if (!GCABLE(f)) {
                busy++;
                continue;
            }

            /* Reclaim the object. */
            gced++;
            f->GC();
        }

        END_TIMING();
        LOG(100,
            ("fsdb::GarbageCollect: busy = %d, gced = %d, elapsed = %3.1f\n",
             busy, gced, elapsed));
    }
}

/* MUST be called from within transaction! */
void fsdb::GetDown()
{
    /* GC anything out there first. */
    GarbageCollect();

    /* Start with fso priorities at their correct values. */
    RecomputePriorities();

    /* Reclaim fsos and/or blocks as needed. */
    START_TIMING();
    int FsosNeeded = FreeFileMargin - FreeFsoCount();
    if (FsosNeeded > 0)
        ReclaimFsos(MarginPri(), FsosNeeded);
    int BlocksNeeded = FreeBlockMargin - FreeBlockCount();
    if (BlocksNeeded > 0)
        ReclaimBlocks(MarginPri(), BlocksNeeded);
    END_TIMING();
    LOG(100, ("fsdb::GetDown: elapsed = %3.1f\n", elapsed));

    if (FreeFsoCount() < 0 || FreeBlockCount() < 0)
        eprint("Cache Overflow: (%d, %d)", FreeFsoCount(), FreeBlockCount());
}

/* MUST NOT be called from within transaction! */
/* flush LastRef to RVM, as we don't do it on references. */
void fsdb::FlushRefVec()
{
    static long LastRefCounter = 0;

    if (LastRefCounter == RefCounter)
        return; /* don't flush if nothing has been referenced */

    LastRefCounter = RefCounter;
    Recov_BeginTrans();
    rvmlib_set_range(LastRef, MaxFiles * sizeof(long));
    Recov_EndTrans(MAXFP);
}

void FSOD_Init(void)
{
    (void)new vproc("FSODaemon", &FSODaemon, VPT_FSODaemon, FSODaemonStackSize);
}
