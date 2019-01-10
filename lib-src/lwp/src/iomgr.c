/* BLURB lgpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights

#*/

/*
                         IBM COPYRIGHT NOTICE

                          Copyright (C) 1986
             International Business Machines Corporation
                         All Rights Reserved

This  file  contains  some  code identical to or derived from the 1986
version of the Andrew File System ("AFS"), which is owned by  the  IBM
Corporation.   This  code is provided "AS IS" and IBM does not warrant
that it is free of infringement of  any  intellectual  rights  of  any
third  party.    IBM  disclaims  liability of any kind for any damages
whatsoever resulting directly or indirectly from use of this  software
or  of  any  derivative work.  Carnegie Mellon University has obtained
permission to  modify,  distribute and sublicense this code,  which is
based on Version 2  of  AFS  and  does  not  contain  the features and
enhancements that are part of  Version 3 of  AFS.  Version 3 of AFS is
commercially   available   and  supported  by   Transarc  Corporation,
Pittsburgh, PA.

*/

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <sys/file.h>
#include <sys/time.h>
#include <fcntl.h>
#include <assert.h>

#include <lwp/lwp.h>
#include <lwp/timer.h>
#include "lwp.private.h"

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#ifndef NSIG
#define NSIG 8 * sizeof(sigset_t)
#endif

/* Stack size for IOMGR process and processes instantiated to handle signals */
#define STACK_SIZE 0x8000

/********************************\
* 				 *
*  Stuff for managing IoRequests *
* 				 *
\********************************/

struct IoRequest {
    /* Pid of process making request (for use in IOMGR_Cancel) */
    PROCESS pid;

    /* Descriptor masks for requests */
    int nfds;
    fd_set readfds;
    fd_set writefds;
    fd_set exceptfds;

    /* returned descriptors */
    fd_set rreadfds;
    fd_set rwritefds;
    fd_set rexceptfds;

    struct TM_Elem timeout;

    /* Result of select call */
    int result;

    /* Free'd IoRequests are chained together (head == iorFreeList) */
    struct IoRequest *free;
};

/********************************\
* 				 *
*  Stuff for managing signals    *
* 				 *
\********************************/

#define badsig(signo) (((signo) <= 0) || ((signo) >= NSIG))
#define mysigmask(signo) (1 << ((signo)-1))

static long sigsHandled; /* sigmask(signo) is on if we handle signo */
static int anySigsDelivered; /* true if any have been delivered. */
static struct sigaction oldVecs[NSIG]; /* the old signal vectors */
static char *sigEvents[NSIG]; /* the event to do an LWP signal on */
static int sigDelivered[NSIG]; /* True for signals delivered
					   so far.  This is an int
					   array to make sure there
					   are no conflicts when
					   trying to write it */
/* software 'signals' */
#define NSOFTSIG 4
static void (*sigProc[NSOFTSIG])(void *);
static char *sigRock[NSOFTSIG];

static struct IoRequest *iorFreeList = 0;

static struct TM_Elem *Requests; /* List of requests */
static struct timeval iomgr_timeout; /* global so signal handler can zap it */

#define FreeRequest(x) ((x)->free = iorFreeList, iorFreeList = (x))

/* function & procedure declarations */
static struct IoRequest *NewRequest();
static int IOMGR_CheckSignals();
static int IOMGR_CheckTimeouts();
static int IOMGR_CheckDescriptors(int PollingCheck);
static void IOMGR(void *unused);
static int SignalIO(fd_set *readfds, fd_set *writefds, fd_set *exceptfds);
static int SignalTimeout(struct timeval *timeout);
static int SignalSignals();

static struct IoRequest *NewRequest()
{
    struct IoRequest *request;

    if ((request = iorFreeList))
        iorFreeList = request->free;
    else
        request = (struct IoRequest *)malloc(sizeof(struct IoRequest));

    return request;
}

#define Purge(list) FOR_ALL_ELTS(req, list, { free(req->BackPointer); })

/*
 *    The IOMGR module manages three types of IO for the LWPs in the process: 
 *       1. Signals
 *       2. Sleeps
 *       3. Descriptors
 *    There is a separate "check" routine for each which looks for its type of IO and
 *    makes corresponding LWPs runnable.
 *
 *    N.B.  "Sleeps" result from an LWP calling IOMGR_Select with blank fd arrays and a non-null timeout.
 */

/* Return value indicates whether anyone was signalled. */
static int IOMGR_CheckSignals()
{
    if (!anySigsDelivered)
        return (FALSE);

    return (SignalSignals());
}

/* Return value indicates whether anyone timed-out (and was woken-up). */
static int IOMGR_CheckTimeouts()
{
    int woke_someone = FALSE;

    TM_Rescan(Requests);
    for (;;) {
        struct IoRequest *req;
        struct TM_Elem *expired = TM_GetExpired(Requests);
        if (expired == NULL)
            break;

        woke_someone = TRUE;
        req          = (struct IoRequest *)expired->BackPointer;
        req->nfds    = 0;
        req->result  = 0; /* no fds ready */
        TM_Remove(Requests, &req->timeout);
        LWP_QSignal(req->pid);
        req->pid->iomgrRequest = 0;
    }

    return (woke_someone);
}

/* Return value indicates whether anyone was woken up. */
/* N.B.  Special return value of -1 indicates a signal was delivered
   prior to the select. */
static int IOMGR_CheckDescriptors(int PollingCheck)
{
    int result, nfds, rf, wf, ef;
    fd_set readfds, writefds, exceptfds;
    struct TM_Elem *earliest;
    struct timeval timeout, tmp_timeout;

    earliest = TM_GetEarliest(Requests);
    if (earliest == NULL)
        return (0);

    /* Merge active descriptors. */
    rf = wf = ef = 0; /* set whenever a fd in a fd_set is set */
    nfds         = 0;
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_ZERO(&exceptfds);
    FOR_ALL_ELTS(r, Requests, {
        int i;
        struct IoRequest *req;
        req = (struct IoRequest *)r->BackPointer;

        for (i = 0; i < req->nfds; i++) {
            if (FD_ISSET(i, &req->readfds)) {
                FD_SET(i, &readfds);
                rf = 1;
            }
            if (FD_ISSET(i, &req->writefds)) {
                FD_SET(i, &writefds);
                wf = 1;
            }
            if (FD_ISSET(i, &req->exceptfds)) {
                FD_SET(i, &exceptfds);
                ef = 1;
            }
        }
        if (req->nfds > nfds)
            nfds = req->nfds;
    });

    /* Set timeout for select syscall. */
    if (PollingCheck) {
        timeout.tv_sec  = 0;
        timeout.tv_usec = 0;
    } else
        timeout = earliest->TimeLeft;

    iomgr_timeout = timeout;
    if (timeout.tv_sec == -1 && timeout.tv_usec == -1) {
        /* infinite, sort of */
        iomgr_timeout.tv_sec  = 100000000;
        iomgr_timeout.tv_usec = 0;
    }

    /* Check one last time for a signal delivery.  If one comes after
       this, the signal */
    /* handler will set iomgr_timeout to zero, causing the select to
       return immediately. */
    /* The timer package won't return a zero timeval because all of
       those guys were handled above. */
    /* I'm assuming that the kernel masks signals while it's picking
       up the parameters to select. */
    /* This may a bad assumption! -DN */
    if (anySigsDelivered)
        return (-1);

#if 0
    lwpdebug(0, "[select(%d, 0x%x, 0x%x, 0x%x, <%d, %d>)]\n",
		 nfds, readfds, writefds, exceptfds, (int)timeout.tv_sec, 
		 (int)timeout.tv_usec);
#endif

    if (iomgr_timeout.tv_sec != 0 || iomgr_timeout.tv_usec != 0) {
        /* this is a non polling select
	   ignore cont_sw_threshold flag in dispatcher */
        last_context_switch.tv_sec  = 0;
        last_context_switch.tv_usec = 0;
    }

    /* Linux adheres to Posix standard for select and sets
       iomgr_timeout to 0; this needs to be reset before we proceed,
       otherwise SignalTimeout never gets called.  Since Linux select
       changes the timeout, we must not pass iomgr_timeout.  if we
       did, the changes the signal handler may make to this variable
       will always be lost, since select resets the variable upon
       return. */

    tmp_timeout = iomgr_timeout;
    result      = select(nfds, rf ? &readfds : NULL, wf ? &writefds : NULL,
                    ef ? &exceptfds : NULL, &tmp_timeout);

    if (result < 0) {
        lwpdebug(-1, "Select returns error: %d\n", errno);

        /* some debugging code to catch bad filedescriptors.
	 * i.e. in most cases someone closed an FD we were waiting on. */
        if (errno != EINTR) {
            int i;
            for (i = 0; i < nfds; i++) {
                if ((FD_ISSET(i, &readfds) || FD_ISSET(i, &writefds) ||
                     FD_ISSET(i, &exceptfds)) &&
                    fcntl(i, F_GETFD, 0) < 0 && errno == EBADF) {
                    lwpdebug(0, "BADF fd %d", i);
                }
            }
            assert(0);
        }
        return (0);
    }

    /* See what happened. */
    if (result > 0)
        /* Action -- wake up everyone involved. */
        return (SignalIO(&readfds, &writefds, &exceptfds));

    if (iomgr_timeout.tv_sec != 0 || iomgr_timeout.tv_usec != 0)
        /* Real timeout only if signal handler hasn't set
           iomgr_timeout to zero. */
        return (SignalTimeout(&timeout));

    return (0);
}

/* The IOMGR process */

/* Important invariant: process->iomgrRequest is null iff request not
 * in timer queue also, request->pid is valid while request is in
 * queue, also, don't signal selector while request in queue, since
 * selector free's request.  */

static void IOMGR(void *unused)
{
    for (;;) {
        /* Wake up anyone who has expired or who has received a Unix
           signal between executions. */
        /* Keep going until we run out. */
        {
            int woke_someone;

            do {
                woke_someone = FALSE;

                /* Wake up anyone waiting on signals. */
                /* Note: IOMGR_CheckSignals() may yield! */
                if (IOMGR_CheckSignals())
                    woke_someone = TRUE;

                /* Wake up anyone who has timed out. */
                if (IOMGR_CheckTimeouts())
                    woke_someone = TRUE;

                if (woke_someone)
                    LWP_DispatchProcess();
            } while (woke_someone);
        }

        /* Check for IO on active descriptors. */
        /* Negative result indicates a late-breaking signal. */
        if (IOMGR_CheckDescriptors(FALSE) < 0)
            continue;

        LWP_DispatchProcess();
    }
}

/************************\
* 			 *
*  Signalling routines 	 *
* 			 *
\************************/

static int SignalIO(fd_set *readfds, fd_set *writefds, fd_set *exceptfds)
{
    int woke_someone = FALSE;

    /* Look at everyone who's bit mask was affected */
    FOR_ALL_ELTS(r, Requests, {
        int i;
        int wakethisone = 0;
        struct IoRequest *req;
        PROCESS pid;
        req = (struct IoRequest *)r->BackPointer;

        for (i = 0; i < req->nfds; i++) {
            if (FD_ISSET(i, readfds) && FD_ISSET(i, &req->readfds)) {
                FD_SET(i, &req->rreadfds);
                req->result += 1;
                wakethisone = 1;
            }
            if (FD_ISSET(i, writefds) && FD_ISSET(i, &req->writefds)) {
                FD_SET(i, &req->rwritefds);
                req->result += 1;
                wakethisone = 1;
            }
            if (FD_ISSET(i, exceptfds) && FD_ISSET(i, &req->exceptfds)) {
                FD_SET(i, &req->rexceptfds);
                req->result += 1;
                wakethisone = 1;
            }
        }
        if (wakethisone) {
            TM_Remove(Requests, &req->timeout);
            LWP_QSignal(pid = req->pid);
            pid->iomgrRequest = 0;
            woke_someone      = TRUE;
        }
    })

    return (woke_someone);
}

static int SignalTimeout(struct timeval *timeout)
{
    int woke_someone = FALSE;

    /* Find everyone who has specified timeout */
    FOR_ALL_ELTS(r, Requests, {
        struct IoRequest *req;
        PROCESS pid;
        req = (struct IoRequest *)r->BackPointer;
        if (TM_eql(&r->TimeLeft, timeout)) {
            woke_someone = TRUE;
            req->nfds    = 0;
            req->result  = 0;
            TM_Remove(Requests, &req->timeout);
            LWP_QSignal(pid = req->pid);
            pid->iomgrRequest = 0;
        } else
            break;
    })

    return (woke_someone);
}

/*****************************************************\
*						      *
*  Signal handling routine (not to be confused with   *
*  signalling routines, above).			      *
*						      *
\*****************************************************/
static void SigHandler(int signo)
{
    if (badsig(signo) || (sigsHandled & mysigmask(signo)) == 0)
        return; /* can't happen. */
    sigDelivered[signo] = TRUE;
    anySigsDelivered    = TRUE;
    /* Make sure that the IOMGR process doesn't pause on the select. */
    iomgr_timeout.tv_sec  = 0;
    iomgr_timeout.tv_usec = 0;
}

/* Alright, this is the signal signalling routine.  It delivers LWP signals
   to LWPs waiting on Unix signals. NOW ALSO CAN YIELD!! */
static int SignalSignals()
{
    int gotone = FALSE;
    void (*p)(void *);
    int i;

    anySigsDelivered = FALSE;

    /* handle software signals */
    for (i = 0; i < NSOFTSIG; i++) {
        PROCESS pid = NULL;
        if ((p = sigProc[i])) /* This yields!!! */
            LWP_CreateProcess(p, STACK_SIZE, LWP_NORMAL_PRIORITY, sigRock[i],
                              "SignalHandler", &pid);
        sigProc[i] = 0;
    }

    for (i = 1; i <= NSIG; ++i) /* forall !badsig(i) */
        if ((sigsHandled & mysigmask(i)) && sigDelivered[i] == TRUE) {
            sigDelivered[i] = FALSE;
            LWP_NoYieldSignal(sigEvents[i]);
            gotone = TRUE;
        }
    return gotone;
}

/***************************\
* 			    *
*  User-callable routines   *
* 			    *
\***************************/

/* Keep IOMGR process id */
static PROCESS IOMGR_Id = NULL;

int IOMGR_SoftSig(void (*aproc)(void *), char *arock)
{
    int i;
    for (i = 0; i < NSOFTSIG; i++) {
        if (sigProc[i] == 0) {
            /* a free entry */
            sigProc[i]            = aproc;
            sigRock[i]            = arock;
            anySigsDelivered      = TRUE;
            iomgr_timeout.tv_sec  = 0;
            iomgr_timeout.tv_usec = 0;
            return 0;
        }
    }
    return -1;
}

int IOMGR_Initialize()
{
    /* If already initialized, just return */
    if (IOMGR_Id != NULL)
        return LWP_SUCCESS;

    /* Initialize request lists */
    if (TM_Init(&Requests) < 0)
        return -1;

    /* Initialize signal handling stuff. */
    sigsHandled      = 0;
    anySigsDelivered = TRUE; /* A soft signal may have happened before
	IOMGR_Initialize:  so force a check for signals regardless */

    return LWP_CreateProcess(IOMGR, STACK_SIZE, 0, 0, "IO MANAGER", &IOMGR_Id);
}

int IOMGR_Finalize()
{
    int status;

    Purge(Requests) TM_Final(&Requests);
    status   = LWP_DestroyProcess(IOMGR_Id);
    IOMGR_Id = NULL;
    return status;
}

/* Check for pending IO, and set corresponding LWPs runnable. */
/* This is quite similar to the body of IOMGR, but everything MUST be
   done in polling fashion! */
/* Return value indicates whether anyone was set runnable by the poll. */
int IOMGR_Poll()
{
    int woke_someone = FALSE;

    for (;;) {
        /* Check for pending signals. */
        if (IOMGR_CheckSignals())
            woke_someone = TRUE;

        /* Check for timed-out waiters. */
        if (IOMGR_CheckTimeouts())
            woke_someone = TRUE;

        /* Check for descriptor activity.  Do a POLLing check! */
        {
            int CD_woke_someone = IOMGR_CheckDescriptors(TRUE);
            if (CD_woke_someone < 0)
                continue;
            if (CD_woke_someone > 0)
                woke_someone = TRUE;
        }

        break;
    }

    return (woke_someone);
}

int IOMGR_Select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
                 struct timeval *timeout)
{
    struct IoRequest *request;
    int result, i;

    /* See if polling request. If so, handle right here */
    if (timeout != NULL && timeout->tv_sec == 0 && timeout->tv_usec == 0) {
        lwpdebug(0, "[Polling SELECT]");
        result = select(nfds, readfds, writefds, exceptfds, timeout);
        return (result);
    }

    /* Construct request block & insert */
    request = NewRequest();

    FD_ZERO(&request->readfds);
    FD_ZERO(&request->writefds);
    FD_ZERO(&request->exceptfds);
    request->nfds = 0;

    for (i = 0; i < nfds; i++) {
        if (readfds && FD_ISSET(i, readfds)) {
            FD_SET(i, &request->readfds);
            request->nfds = i;
        }
        if (writefds && FD_ISSET(i, writefds)) {
            FD_SET(i, &request->writefds);
            request->nfds = i;
        }
        if (exceptfds && FD_ISSET(i, exceptfds)) {
            FD_SET(i, &request->exceptfds);
            request->nfds = i;
        }
    }
    request->nfds++;

    FD_ZERO(&request->rreadfds);
    FD_ZERO(&request->rwritefds);
    FD_ZERO(&request->rexceptfds);

    if (timeout == NULL) {
        request->timeout.TotalTime.tv_sec  = -1;
        request->timeout.TotalTime.tv_usec = -1;
    } else
        request->timeout.TotalTime = *timeout;

    request->timeout.BackPointer = (char *)request;

    /* Insert my PID in case of IOMGR_Cancel */
    request->pid            = lwp_cpptr;
    request->result         = 0;
    lwp_cpptr->iomgrRequest = request;
    TM_Insert(Requests, &request->timeout);

    /* Wait for action */
    LWP_QWait();

    /* Update parameters & return */
    if (readfds)
        FD_ZERO(readfds);
    if (writefds)
        FD_ZERO(writefds);
    if (exceptfds)
        FD_ZERO(exceptfds);

    for (i = 0; i < request->nfds; i++) {
        if (readfds && FD_ISSET(i, &request->rreadfds))
            FD_SET(i, readfds);
        if (writefds && FD_ISSET(i, &request->rwritefds))
            FD_SET(i, writefds);
        if (exceptfds && FD_ISSET(i, &request->rexceptfds))
            FD_SET(i, exceptfds);
    }

    result = request->result;
    FreeRequest(request);

    return (result);
}

int IOMGR_Cancel(PROCESS pid)
{
    struct IoRequest *request;

    if ((request = pid->iomgrRequest) == 0)
        return -1;

    request->nfds = 0;
    FD_ZERO(&request->readfds);
    FD_ZERO(&request->writefds);
    FD_ZERO(&request->exceptfds);
    request->result = -2;
    TM_Remove(Requests, &request->timeout);
    LWP_QSignal(request->pid);
    pid->iomgrRequest = 0;

    return 0;
}

/* Cause delivery of signal signo to result in a LWP_SignalProcess of
   event. */
int IOMGR_Signal(int signo, char *event)
{
    struct sigaction sv;

    if (badsig(signo))
        return LWP_EBADSIG;
    if (event == NULL)
        return LWP_EBADEVENT;
    sv.sa_handler = SigHandler;
    sigfillset(&sv.sa_mask);
    sv.sa_flags = 0;
    sigsHandled |= mysigmask(signo);
    sigEvents[signo]    = event;
    sigDelivered[signo] = FALSE;
    if (sigaction(signo, &sv, &oldVecs[signo]) == -1)
        return LWP_ESYSTEM;
    return LWP_SUCCESS;
}

/* Stop handling occurances of signo. */
int IOMGR_CancelSignal(int signo)
{
    if (badsig(signo) || (sigsHandled & mysigmask(signo)) == 0)
        return LWP_EBADSIG;
    sigaction(signo, &oldVecs[signo], (struct sigaction *)0);
    sigsHandled &= ~mysigmask(signo);
    return LWP_SUCCESS;
}
