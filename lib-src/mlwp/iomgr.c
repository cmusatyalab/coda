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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/lib-src/mlwp/iomgr.c,v 4.5 1997/12/18 23:44:53 braam Exp $";
#endif /*_BLURB_*/


/*

                         IBM COPYRIGHT NOTICE

                          Copyright (C) 1986
             International Business Machines Corporation
                         All Rights Reserved

This  file  contains  some  code identical to or derived from the 1986
version of the Andrew File System ("AFS"), which is owned by  the  IBM
Corporation.    This  code is provded "AS IS" and IBM does not warrant
that it is free of infringement of  any  intellectual  rights  of  any
third  party.    IBM  disclaims  liability of any kind for any damages
whatsoever resulting directly or indirectly from use of this  software
or  of  any  derivative work.  Carnegie Mellon University has obtained
permission to distribute this code, which is based on Version 2 of AFS
and  does  not  contain the features and enhancements that are part of
Version 3 of AFS.  Version 3 of  AFS  is  commercially  available  and
supported by Transarc Corporation, Pittsburgh, PA.

*/


#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <sys/file.h>
#include <sys/time.h>
#include "lwp.h"
#include "lwp.private.h"
#include "timer.h"

#define NIL	0
#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif  TRUE

#ifndef NSIG
#define NSIG 8*sizeof(sigset_t)
#endif


/* Stack size for IOMGR process and processes instantiated to handle signals */
#define STACK_SIZE	32768  /* 32K */

/********************************\
* 				 *
*  Stuff for managing IoRequests *
* 				 *
\********************************/

struct IoRequest {

    /* Pid of process making request (for use in IOMGR_Cancel */
    PROCESS		pid;

    /* Descriptor masks for requests */
    int			readfds;
    int			writefds;
    int			exceptfds;

    struct TM_Elem	timeout;

    /* Result of select call */
    int			result;

};

/********************************\
* 				 *
*  Stuff for managing signals    *
* 				 *
\********************************/

#define badsig(signo)		(((signo) <= 0) || ((signo) >= NSIG))
#define mysigmask(signo)		(1 << ((signo)-1))

extern int errno;

PRIVATE long openMask;			/* mask of open files on an IOMGR abort */
PRIVATE long sigsHandled;		/* sigmask(signo) is on if we handle signo */
PRIVATE int anySigsDelivered;		/* true if any have been delivered. */
PRIVATE struct sigaction oldVecs[NSIG];	/* the old signal vectors */
PRIVATE char *sigEvents[NSIG];		/* the event to do an LWP signal on */
PRIVATE int sigDelivered[NSIG];		/* True for signals delivered so far.
					   This is an int array to make sure there
					   are no conflicts when trying to write it */
/* software 'signals' */
#define NSOFTSIG		4
PRIVATE PFIC sigProc[NSOFTSIG];
PRIVATE char *sigRock[NSOFTSIG];


PRIVATE struct IoRequest *iorFreeList = 0;

PRIVATE struct TM_Elem *Requests;	/* List of requests */
PRIVATE struct timeval iomgr_timeout;	/* global so signal handler can zap it */

#define FreeRequest(x) ((x)->result = (int) iorFreeList, iorFreeList = (x))

/* function & procedure declarations */
PRIVATE struct IoRequest *NewRequest();
PRIVATE int IOMGR_CheckSignals C_ARGS(());
PRIVATE int IOMGR_CheckTimeouts C_ARGS(());
PRIVATE int IOMGR_CheckDescriptors C_ARGS((int PollingCheck));
PRIVATE void IOMGR C_ARGS((char *dummy));
PRIVATE int SignalIO C_ARGS((int fds, register int readfds, register int writefds, register int exceptfds));
PRIVATE int SignalTimeout C_ARGS((int fds, register struct timeval *timeout));
#ifdef	__STDC__
#define SigHandlerType void
#else
#define SigHandlerType int
#endif
PRIVATE SigHandlerType SigHandler C_ARGS((int signo));
PRIVATE int SignalSignals ();


PRIVATE struct IoRequest *NewRequest()
{

    register struct IoRequest *request;

    if (request=iorFreeList) iorFreeList = (struct IoRequest *) (request->result);
    else request = (struct IoRequest *) malloc(sizeof(struct IoRequest));
    return request;
}

#define Purge(list) FOR_ALL_ELTS(req, list, { free(req->BackPointer); })

#define MAX_FDS 32

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
PRIVATE int IOMGR_CheckSignals() {
    if (!anySigsDelivered) return(FALSE);

    return(SignalSignals());
}


/* Return value indicates whether anyone timed-out (and was woken-up). */
PRIVATE int IOMGR_CheckTimeouts() {
    int woke_someone = FALSE;

    TM_Rescan(Requests);
    for (;;) {
	register struct IoRequest *req;
	struct TM_Elem *expired = TM_GetExpired(Requests);
	if (expired == NIL) break;

	woke_someone = TRUE;
	req = (struct IoRequest *) expired -> BackPointer;
	req->readfds = req->writefds = req->exceptfds = 0;	/* no data ready */
	req->result = 0;					/* no fds ready */
	TM_Remove(Requests, &req->timeout);
	LWP_QSignal(req->pid);
	req->pid->iomgrRequest = 0;
    }

    return(woke_someone);
}


/* Return value indicates whether anyone was woken up. */
/* N.B.  Special return value of -1 indicates a signal was delivered
   prior to the select. */
PRIVATE int IOMGR_CheckDescriptors(PollingCheck)
    int PollingCheck;
{
    int fds, readfds, writefds, exceptfds;
    struct TM_Elem *earliest;
    struct timeval timeout, tmp_timeout, junk;

    earliest = TM_GetEarliest(Requests);
    if (earliest == NIL) return(0);

    /* Merge active descriptors. */
    readfds = 0;
    writefds = 0;
    exceptfds = 0;
    FOR_ALL_ELTS(r, Requests, {
		  register struct IoRequest *req;
		  req = (struct IoRequest *) r -> BackPointer;
		  readfds |= req -> readfds;
		  writefds |= req -> writefds;
		  exceptfds |= req -> exceptfds;
		  });

    /* Set timeout for select syscall. */
    if (PollingCheck) {
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
    }
    else
	timeout = earliest -> TimeLeft;
    iomgr_timeout = timeout;
    if (timeout.tv_sec == -1 && timeout.tv_usec == -1) {
	/* infinite, sort of */
	iomgr_timeout.tv_sec = 100000000;
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
    if (anySigsDelivered) return(-1);

    /* Do the select.  It runs much faster if 0's are passed instead of &0s! */
    lwpdebug(0, ("[select(%d, 0x%x, 0x%x, 0x%x, <%d, %d>)]\n",
		  MAX_FDS, readfds, writefds, exceptfds, timeout.tv_sec, timeout.tv_usec));

    if (iomgr_timeout.tv_sec != 0 || iomgr_timeout.tv_usec != 0)  {
	/* this is a non polling select 
	   ignore cont_sw_threshold flag in dispatcher */
	last_context_switch.tv_sec = 0;
	last_context_switch.tv_usec = 0;
    }
    /* Linux adheres to Posix standard for select and sets
       iomgr_timeout to 0; this needs to be reset before we proceed,
       otherwise SignalTimeout never gets called */
    tmp_timeout = iomgr_timeout;
    fds = select(MAX_FDS, (fd_set *)(readfds ? &readfds : 0), 
		  (fd_set *)(writefds ? &writefds : 0), 
		  (fd_set *)(exceptfds ? &exceptfds : 0), 
		  &iomgr_timeout);
    iomgr_timeout = tmp_timeout;

    if (fds < 0 && errno != EINTR) {
	for(fds=0;fds<MAX_FDS;fds++) {
	    if (fcntl(fds, F_GETFD, 0) < 0 && errno == EBADF) 
	      openMask |= (1<<fds);
	}
	abort();
    }

    /* Force a new gettimeofday call so FT_AGetTimeOfDay calls work. */
    FT_GetTimeOfDay(&junk, 0);

    /* See what happened. */
    if (fds > 0)
	/* Action -- wake up everyone involved. */
	return(SignalIO(fds, readfds, writefds, exceptfds));
    else if (fds == 0 && 
	     (iomgr_timeout.tv_sec != 0 || 
	      iomgr_timeout.tv_usec != 0))
	/* Real timeout only if signal handler hasn't set
           iomgr_timeout to zero. */
	return(SignalTimeout(fds, &timeout));
    else
	return(0);
}


/* The IOMGR process */

/* Important invariant: process->iomgrRequest is null iff request not
 * in timer queue also, request->pid is valid while request is in
 * queue, also, don't signal selector while request in queue, since
 * selector free's request.  */

PRIVATE void IOMGR(dummy)
    char *dummy;
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

PRIVATE int SignalIO(fds, readfds, writefds, exceptfds)
    int fds;
    register int readfds;
    register int writefds;
    register int exceptfds;
{
    int woke_someone = FALSE;

    /* Look at everyone who's bit mask was affected */
    FOR_ALL_ELTS(r, Requests, {
	register struct IoRequest *req;
	register PROCESS pid;
	req = (struct IoRequest *) r -> BackPointer;
	if ((req->readfds & readfds) ||
	    (req->writefds & writefds) ||
	    (req->exceptfds & exceptfds)) {

	    woke_someone = TRUE;
	    req -> readfds &= readfds;
	    req -> writefds &= writefds;
	    req -> exceptfds &= exceptfds;
	    req -> result = fds;
	    TM_Remove(Requests, &req->timeout);
	    LWP_QSignal(pid=req->pid);
	    pid->iomgrRequest = 0;
	}
    })

    return(woke_someone);
}

PRIVATE int SignalTimeout(fds, timeout)
    int fds;
    register struct timeval *timeout;
{
    int woke_someone = FALSE;

    /* Find everyone who has specified timeout */
    FOR_ALL_ELTS(r, Requests, {
	register struct IoRequest *req;
	register PROCESS pid;
	req = (struct IoRequest *) r -> BackPointer;
	if (TM_eql(&r->TimeLeft, timeout)) {
	    woke_someone = TRUE;
	    req -> result = fds;
	    TM_Remove(Requests, &req->timeout);
	    LWP_QSignal(pid=req->pid);
	    pid->iomgrRequest = 0;
	} else
	    break;
    })

    return(woke_someone);
}

/*****************************************************\
*						      *
*  Signal handling routine (not to be confused with   *
*  signalling routines, above).			      *
*						      *
\*****************************************************/
PRIVATE SigHandlerType SigHandler (signo)
    int signo;
{

    if (badsig(signo) || (sigsHandled & mysigmask(signo)) == 0)
	return;		/* can't happen. */
    sigDelivered[signo] = TRUE;
    anySigsDelivered = TRUE;
    /* Make sure that the IOMGR process doesn't pause on the select. */
    iomgr_timeout.tv_sec = 0;
    iomgr_timeout.tv_usec = 0;
}

/* Alright, this is the signal signalling routine.  It delivers LWP signals
   to LWPs waiting on Unix signals. NOW ALSO CAN YIELD!! */
PRIVATE int SignalSignals ()
{

    int gotone = FALSE;
    register int i;
    PFIC    p;

    anySigsDelivered = FALSE;

    /* handle software signals */
    for (i=0; i < NSOFTSIG; i++) {
	PROCESS *pid;
	if (p=sigProc[i]) /* This yields!!! */
	    LWP_CreateProcess(p, STACK_SIZE, LWP_NORMAL_PRIORITY, sigRock[i],
		"SignalHandler", pid);
	sigProc[i] = 0;
    }

    for (i = 1; i <= NSIG; ++i)  /* forall !badsig(i) */
	if ((sigsHandled & mysigmask(i)) && sigDelivered[i] == TRUE) {
	    sigDelivered[i] = FALSE;
	    LWP_NoYieldSignal (sigEvents[i]);
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
PRIVATE PROCESS IOMGR_Id = NIL;

int IOMGR_SoftSig(aproc, arock)
    PFIC aproc;
    char *arock;
{
    register int i;
    for (i=0;i<NSOFTSIG;i++) {
	if (sigProc[i] == 0) {
	    /* a free entry */
	    sigProc[i] = aproc;
	    sigRock[i] = arock;
	    anySigsDelivered = TRUE;
	    iomgr_timeout.tv_sec = 0;
	    iomgr_timeout.tv_usec = 0;
	    return 0;
	}
    }
    return -1;
}


int IOMGR_Initialize()
{
    extern int TM_Init C_ARGS((register struct TM_Elem **list));
    PROCESS pid;

    /* If lready initialized, just return */
    if (IOMGR_Id != NIL) return LWP_SUCCESS;

    /* Initialize request lists */
    if (TM_Init(&Requests) < 0) return -1;

    /* Initialize signal handling stuff. */
    sigsHandled = 0;
    anySigsDelivered = TRUE; /* A soft signal may have happened before
	IOMGR_Initialize:  so force a check for signals regardless */

    return LWP_CreateProcess((PFIC)IOMGR, STACK_SIZE, 0, 0, "IO MANAGER", &IOMGR_Id);
}

int IOMGR_Finalize()
{

    int status;

    Purge(Requests)
    TM_Final(&Requests);
    status = LWP_DestroyProcess(IOMGR_Id);
    IOMGR_Id = NIL;
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
	    if (CD_woke_someone < 0) continue;
	    if (CD_woke_someone > 0) woke_someone = TRUE;
	}

	break;
    }

    return(woke_someone);
}

int IOMGR_Select(fds, readfds, writefds, exceptfds, timeout)
    register int fds;
    register int *readfds;
    register int *writefds;
    register int *exceptfds;
    struct timeval *timeout; 
{

    register struct IoRequest *request;
    int result;

    /* See if polling request. If so, handle right here */
    if (timeout != NIL && timeout->tv_sec == 0 && timeout->tv_usec == 0) {
	int nfds;
	lwpdebug(0, ("[Polling SELECT]"))
	nfds = select(MAX_FDS, (fd_set *)readfds, (fd_set *)writefds, (fd_set *)exceptfds, timeout);
	return (nfds > 1 ? 1 : nfds);
    }

    /* Construct request block & insert */
    request = NewRequest();
    request -> readfds = (readfds != NIL ? *readfds : 0);
    request -> writefds = (writefds != NIL ? *writefds : 0);
    request -> exceptfds = (exceptfds != NIL ? *exceptfds : 0);
    if (timeout == NIL) {
	request -> timeout.TotalTime.tv_sec = -1;
	request -> timeout.TotalTime.tv_usec = -1;
    } else
	request -> timeout.TotalTime = *timeout;

    request -> timeout.BackPointer = (char *) request;

    /* Insert my PID in case of IOMGR_Cancel */
    request -> pid = lwp_cpptr;
    lwp_cpptr -> iomgrRequest = request;
    TM_Insert(Requests, &request->timeout);

    /* Wait for action */
    LWP_QWait();

    /* Update parameters & return */
    if (readfds != NIL) *readfds = request -> readfds;
    if (writefds != NIL) *writefds = request -> writefds;
    if (exceptfds != NIL) *exceptfds = request -> exceptfds;
    result = request -> result;
    FreeRequest(request);
    return (result > 1 ? 1 : result);
}

int IOMGR_Cancel(pid)
    register PROCESS pid;
{

    register struct IoRequest *request;

    if ((request = pid->iomgrRequest) == 0) return -1;	/* Pid not found */

    request -> readfds = 0;
    request -> writefds = 0;
    request -> exceptfds = 0;
    request -> result = -2;
    TM_Remove(Requests, &request->timeout);
    LWP_QSignal(request->pid);
    pid->iomgrRequest = 0;

    return 0;
}

/* Cause delivery of signal signo to result in a LWP_SignalProcess of
   event. */
int IOMGR_Signal (signo, event)
    int signo;
    char *event;
{

    struct sigaction sv;

    if (badsig(signo))
	return LWP_EBADSIG;
    if (event == NIL)
	return LWP_EBADEVENT;
    sv.sa_handler = SigHandler;
    sigfillset(&sv.sa_mask);
    sv.sa_flags = 0;
    sigsHandled |= mysigmask(signo);
    sigEvents[signo] = event;
    sigDelivered[signo] = FALSE;
    if (sigaction (signo, &sv, &oldVecs[signo]) == -1)
	return LWP_ESYSTEM;
    return LWP_SUCCESS;
}

/* Stop handling occurances of signo. */
int IOMGR_CancelSignal (signo)
    int signo;
{
    if (badsig(signo) || (sigsHandled & mysigmask(signo)) == 0)
	return LWP_EBADSIG;
    sigaction (signo, &oldVecs[signo], (struct sigaction *)0);
    sigsHandled &= ~mysigmask(signo);
    return LWP_SUCCESS;
}
