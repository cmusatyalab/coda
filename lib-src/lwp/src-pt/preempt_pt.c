/* BLURB lgpl

                           Coda File System
                              Release 5

            Copyright (c) 1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights
#*/

#include <pthread.h>
#include <assert.h>

#include <lwp/lwp.h>
#include "lwp.private_pt.h"

/***** Preemption/concurrency control routines *****/
void PRE_Concurrent(int on)
{
    PROCESS pid;
    assert(LWP_CurrentProcess(&pid) == 0);

    if (on) {
        /* take this thread out of the runqueues */
        lwp_LEAVE(pid);
        pid->concurrent = 1;
    } else {
        pid->concurrent = 0;
        lwp_JOIN(pid);
    }
}

/* Do we want to block only threads in the same critical region, or
 * can we assume that non-concurrent threads don't correctly specify
 * when they enter a critical region? I guess assuming the latter is
 * the safest choice. */
void PRE_BeginCritical(void)
{
    PROCESS pid;
    assert(LWP_CurrentProcess(&pid) == 0);
    /* how critical could this section be? */
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    PRE_Concurrent(0);
}

void PRE_EndCritical(void)
{
    PROCESS pid;
    assert(LWP_CurrentProcess(&pid) == 0);
    PRE_Concurrent(1);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_testcancel();
}

