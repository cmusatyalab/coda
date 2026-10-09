/* BLURB gpl

                            Coda File System
                               Release 8

            Copyright (c) 2026 Carnegie Mellon University
                   Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently
#*/

/*
 * IOMGR bookkeeping-desync reproduction.
 *
 * Bug: an IOMGR_Select() user that is woken by something other than an IOMGR
 * wake path (a raw LWP_QSignal, or a leftover qpending credit) returns from
 * IOMGR_Select() with its TM_Elem still linked in the Requests list, its
 * IoRequest pushed onto the LIFO free list, and iomgrRequest left stale. The
 * owner's next IOMGR_Select() reuses the same IoRequest (LIFO NewRequest) and
 * TM_Inserts an already-linked element, corrupting the list. The corrupted
 * self-referential element is then hit by the next timer-list operation,
 * tripping the TM_* consistency asserts (timer.c:178/204/227/248).
 *
 * This is the class of crash seen in the field as:
 *   codasrv: ../subprojects/lwp/src/timer.c:204: TM_Remove:
 *   Assertion `elem->Next != elem && elem->Prev != elem' failed.
 *
 * Deterministic choreography (credit-on-entry variant):
 *   - The worker (priority 1) simply runs a loop of IOMGR_Select() calls.
 *     It is created, but the dispatcher re-picks main (priority
 *     LWP_MAX_PRIORITY) rather than the worker, so the worker has NOT yet run
 *     when main acts.
 *   - main LWP_QSignal()s the worker. The worker is on the runnable queue
 *     (not blocked in QWait), so the signal becomes a persistent +1 qpending
 *     credit (LWP_ENOWAIT) rather than a wake.
 *   - main then blocks (LWP_WaitProcess), letting the worker run. The
 *     worker's first IOMGR_Select() consumes the credit in LWP_QWait() and
 *     returns immediately WITHOUT blocking, freeing its request while the
 *     element is still linked and leaving iomgrRequest stale. (On a patched
 *     build the post-QWait self-removal heals this.)
 *   - The worker's second IOMGR_Select() reuses the same request (LIFO) and
 *     TM_Inserts the still-linked element => double insert => the single
 *     element becomes fully self-referential. The worker then blocks in that
 *     second IOMGR_Select().
 *   - With main and the worker both blocked, the IOMGR process (priority 0)
 *     runs IOMGR_CheckTimeouts() -> TM_Rescan(), which walks the corrupted
 *     list and trips the assert (timer.c:227).
 *
 * On an UNPATCHED build this test ABORTS (SIGABRT) in the forked LWP_TEST_F
 * instance and is reported as a failure. On a PATCHED build (IOMGR_Select
 * self-removes its element after QWait when iomgrRequest == request) the
 * desync is healed and the test completes normally.
 *
 * Note on the assert line: in the field the corruption was hit by
 * IOMGR_Cancel()'s raw TM_Remove (timer.c:204) during the RPC2 SL re-arm
 * storm of reintegration. Here the IOMGR process's own TM_Rescan hits it
 * first (timer.c:227). Both are the same desync manifesting through a
 * different timer-list operation; both are prevented by the same fix.
 */

#include "gtest/gtest.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <lwp/lwp.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>

#ifdef __cplusplus
}
#endif

#include <testing/memory.h>

namespace
{

class IomgrTest : public ::testing::Test {};

/* Events shared between main and the worker. Each LWP_TEST_F runs in its own
 * forked instance, so these per-fork copies start at 0 and never interfere
 * across tests. */
static char ev_done;

/* 100 ms is plenty: it is short enough that the patched build's clean
 * timeouts complete quickly, and the unpatched abort (TM_Rescan on the
 * corrupted list) happens immediately, long before any 100 ms deadline. */
static const int SELECT_TIMEOUT_USEC = 100 * 1000;

/* The worker simply hammers IOMGR_Select. With a stray credit present (see
 * the test comment), the first call desyncs the bookkeeping and the second
 * corrupts the list. Without the credit (the `sane` control) every call is a
 * clean, self-timed-out select. */
static void iomgr_worker(void *p)
{
    struct timeval to = { 0, SELECT_TIMEOUT_USEC };
    int i;

    for (i = 0; i < 3; i++)
        IOMGR_Select(0, NULL, NULL, NULL, &to);

    LWP_SignalProcess(&ev_done);
}

/* Control: the exact same worker, but WITHOUT the stray LWP_QSignal. This
 * exercises the full IOMGR_Select insert -> IOMGR select() -> timeout ->
 * wake -> reuse cycle on a *consistent* list and must pass on both unpatched
 * and patched builds (confirms the machinery works and the TM_* asserts do
 * not fire when the bookkeeping is in sync). */
LWP_TEST_F(IomgrTest, sane)
{
    PROCESS main;
    PROCESS worker;
    int ret_val;

    ret_val = LWP_Init(LWP_VERSION, LWP_MAX_PRIORITY, &main);
    ASSERT_EQ(ret_val, LWP_SUCCESS);

    ret_val = IOMGR_Initialize();
    ASSERT_EQ(ret_val, LWP_SUCCESS);

    /* The dispatcher re-picks main (highest priority) here, so the worker is
     * parked on the runnable queue but has not yet run. */
    ret_val =
        LWP_CreateProcess(iomgr_worker, 0x8000, 1, NULL, "iomgr_sane", &worker);
    ASSERT_EQ(ret_val, LWP_SUCCESS);

    /* Block so the worker (priority 1) can run its clean selects. */
    ret_val = LWP_WaitProcess(&ev_done);
    ASSERT_EQ(ret_val, LWP_SUCCESS);

    ret_val = IOMGR_Finalize();
    ASSERT_EQ(ret_val, LWP_SUCCESS);

    ret_val = LWP_TerminateProcessSupport();
    ASSERT_EQ(ret_val, LWP_SUCCESS);
}

/* Reproduction: the same worker, but main LWP_QSignal()s it while it is on
 * the runnable queue. That stray signal becomes a qpending credit consumed by
 * the worker's first IOMGR_Select, desyncing the bookkeeping; the second
 * IOMGR_Select double-inserts and corrupts the list, and the IOMGR process's
 * TM_Rescan trips the timer assert. Unpatched: this aborts. Patched: passes. */
LWP_TEST_F(IomgrTest, stray_qsignal_credit)
{
    PROCESS main;
    PROCESS worker;
    int ret_val;

    ret_val = LWP_Init(LWP_VERSION, LWP_MAX_PRIORITY, &main);
    ASSERT_EQ(ret_val, LWP_SUCCESS);

    ret_val = IOMGR_Initialize();
    ASSERT_EQ(ret_val, LWP_SUCCESS);

    /* Worker is parked on the runnable queue, not yet run. */
    ret_val = LWP_CreateProcess(iomgr_worker, 0x8000, 1, NULL, "iomgr_worker",
                                &worker);
    ASSERT_EQ(ret_val, LWP_SUCCESS);

    /* The stray wake: the worker is not blocked in QWait, so this leaves a
     * persistent +1 qpending credit (ENOWAIT) instead of waking it. */
    ret_val = LWP_QSignal(worker);
    ASSERT_EQ(ret_val, LWP_ENOWAIT);

    /* Block so the worker (priority 1) runs: its first IOMGR_Select consumes
     * the credit and desyncs (unpatched), its second double-inserts and
     * corrupts the list, then it blocks. The IOMGR process (priority 0) then
     * trips the timer assert on the corrupted list (unpatched). On a patched
     * build the desync is healed and the worker simply completes its selects
     * and signals ev_done. */
    ret_val = LWP_WaitProcess(&ev_done);
    ASSERT_EQ(ret_val, LWP_SUCCESS);

    ret_val = IOMGR_Finalize();
    ASSERT_EQ(ret_val, LWP_SUCCESS);

    ret_val = LWP_TerminateProcessSupport();
    ASSERT_EQ(ret_val, LWP_SUCCESS);
}

} // namespace
