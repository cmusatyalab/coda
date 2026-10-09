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
 * Regression tests for the TM_Final() empty-list assert (timer.c).
 *
 * TM_Final() is the destructor for a timer list; the (re-enabled) assert
 * requires the list to be drained (sentinel self-referential) before the
 * sentinel is freed. In production the only caller is IOMGR_Finalize(), which
 * drains via Purge() first, and rpc2_TimerQueue is never finalized. This file
 * drives the API directly to pin the contract:
 *
 *   - final_ok_when_drained:  insert, remove, then finalize -> returns 0, no
 *     abort. A positive control that holds on both patched and unpatched
 *     trees.
 *   - final_aborts_when_nonempty: finalize while an element is still linked ->
 *     the assert must fire (SIGABRT). On an unpatched tree (assert commented
 *     out) the process does not die, so the death test reports "failed to die"
 *     and the suite fails; once the assert is re-enabled the child aborts and
 *     the test passes.
 *
 * These are pure TM_* tests (no LWP), so they run under plain gtest TEST()
 * rather than the LWP_TEST_F fork/leak wrapper.
 */

#include "gtest/gtest.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/time.h>
#include <lwp/timer.h>

#ifdef __cplusplus
}
#endif

namespace
{

class TimerTest : public ::testing::Test {};

/* A minimal object whose TM_Elem can be linked into a timer list, standing in
 * for the IoRequest / SL_Entry that embeds one in production. */
struct TimerItem {
    struct TM_Elem elem;
    char payload;
};

/* Insert a finite-timeout element, drain it, then finalize: the list is empty
 * at finalization so TM_Final succeeds without tripping the assert. */
TEST(TimerTest, final_ok_when_drained)
{
    struct TM_Elem *list = NULL;
    TimerItem item;

    ASSERT_EQ(TM_Init(&list), 0);
    item.elem.BackPointer       = (char *)&item;
    item.elem.TotalTime.tv_sec  = 1;
    item.elem.TotalTime.tv_usec = 0;
    TM_Insert(list, &item.elem);
    TM_Remove(list, &item.elem);
    EXPECT_EQ(TM_Final(&list), 0);
}

/* Finalizing a list that still holds a live element must abort in TM_Final.
 * The whole setup lives inside the death statement so the child re-runs it
 * self-contained. */
TEST(TimerTest, final_aborts_when_nonempty)
{
    EXPECT_DEATH(
        {
            struct TM_Elem *list = NULL;
            TimerItem item;

            TM_Init(&list);
            item.elem.BackPointer       = (char *)&item;
            item.elem.TotalTime.tv_sec  = 1;
            item.elem.TotalTime.tv_usec = 0;
            TM_Insert(list, &item.elem);
            TM_Final(&list); /* not drained -> the assert must fire */
        },
        ".*");
}

} // namespace
