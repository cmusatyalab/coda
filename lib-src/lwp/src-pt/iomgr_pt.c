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
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <assert.h>

#include <lwp/lwp.h>
#include "lwp.private_pt.h"

/***** IOMGR routines *****/
static void iomgr_sigio_handler(int n)
{
    struct sigaction action;
   
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = iomgr_sigio_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    sigaction(SIGIO, &action, NULL);
}

int IOMGR_Initialize(void)
{
    iomgr_sigio_handler(0);
    return 0;
}

int IOMGR_Select(int fds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
                 struct timeval *timeout)
{
    PROCESS pid;
    int retval;
    struct timeval to = {0,0}, *tp = NULL;

    if (LWP_CurrentProcess(&pid))
        return LWP_EBADPID;

    /* avoid clobbering of timeout, existing programs using LWP don't
     * like that behaviour */
    if (timeout) {
        to = *timeout;
        tp = &to;
    }

    lwp_LEAVE(pid);
    retval = select(fds, readfds, writefds, exceptfds, tp);
    lwp_JOIN(pid);

    return retval;
}

/* ofcourse not to be confused with poll(2) :( */
int IOMGR_Poll(void)
{
    return !list_empty(&lwp_runq);
}

int IOMGR_Cancel (PROCESS pid)
{
    /* this should wake him up, I only hope it won't kill the thread */
    pthread_kill(pid->thread, SIGIO);
    return LWP_SUCCESS;
}

/* These don't do anything for us */
int IOMGR_Finalize() { return 0; }

/* signal delivery is not implemented yet */
int IOMGR_SoftSig (PFIC aproc, char *arock)
{
    assert(0);
    return LWP_SUCCESS;
}

int IOMGR_Signal (int signo, char *event)
{
    assert(0);
    return LWP_SUCCESS;
}

int IOMGR_CancelSignal (int signo)
{
    assert(0);
    return LWP_SUCCESS;
}

