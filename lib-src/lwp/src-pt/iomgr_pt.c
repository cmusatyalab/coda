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
#include <assert.h>

#include <lwp/lwp.h>
#include "lwp.private_pt.h"

/***** IOMGR routines *****/
static void iomgr_sigio_handler(int n)
{
    struct sigaction action;
   
    action.sa_handler  = iomgr_sigio_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags    = 0;
#if !defined(sun) 
    action.sa_restorer = NULL;
#endif

    sigaction(SIGIO, &action, NULL);
}

int IOMGR_Initialize(void)
{
    iomgr_sigio_handler(0);
    return 0;
}

int IOMGR_Select(int fds, int *readfds, int *writefds, int *exceptfds,
                 struct timeval *timeout)
{
    PROCESS pid;
    int retval, i, m;
    struct timeval to = {0,0}, *tp = NULL;
    fd_set rd, wr, ex;

    if (LWP_CurrentProcess(&pid))
        return LWP_EBADPID;

    /* copy the arguments into a proper fdset */
    FD_ZERO(&rd); FD_ZERO(&wr); FD_ZERO(&ex);
    for (i = 0; i < fds; i++) { m = 1 << i;
        if (readfds   && (*readfds   & m)) FD_SET(i, &rd);
        if (writefds  && (*writefds  & m)) FD_SET(i, &wr);
        if (exceptfds && (*exceptfds & m)) FD_SET(i, &ex);
    }

    /* avoid clobbering of timeout, existing programs using LWP don't
     * like that behaviour */
    if (timeout) {
        to = *timeout;
        tp = &to;
    }

    if (pid->havelock) lwp_LEAVE(pid);
    retval = select(fds, &rd, &wr, &ex, tp);
    if (!pid->concurrent) lwp_JOIN(pid);

    /* copy the results back into the arguments */
    if (readfds)   *readfds   = 0;
    if (writefds)  *writefds  = 0;
    if (exceptfds) *exceptfds = 0;
    for (i = 0; i < fds; i++) { m = 1 << i;
        if (readfds   && FD_ISSET(i, &rd)) *readfds   |= m;
        if (writefds  && FD_ISSET(i, &wr)) *writefds  |= m;
        if (exceptfds && FD_ISSET(i, &ex)) *exceptfds |= m;
    }

    return retval;
}

/* ofcourse not to be confused with poll(2) :( */
int IOMGR_Poll(void)
{
    int i;

    if (!dllist_empty(&lwp_join_queue))
        return 1;

    for (i = 0; i < LWP_MAX_PRIORITY; i++)
        if (!dllist_empty(&lwp_runq[i]))
            return 1;
    return 0;
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
