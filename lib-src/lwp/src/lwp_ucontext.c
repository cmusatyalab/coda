/* BLURB lgpl

			Coda File System
			    Release 5

	Copyright (c) 2005-2008 Carnegie Mellon University
		Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

		    Additional copyrights
#*/

#include <sys/types.h>
#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include "lwp_ucontext.h"

/* Some architectures have a stack that grows up instead of down */
//#define STACK_GROWS_UP 1

/* most portable alternative is to use sigaltstack */
#ifdef HAVE_SIGALTSTACK
static JMP_BUF parent;
#define returnto(ctx) LONGJMP(*ctx)
#else
/* otherwise, use the old LWP savecontext/returnto assembly routines

The following documents the Assembler interfaces used by old LWP:

savecontext(void (*ep)(), struct lwp_context *savearea, char *sp)

    Stub for Assembler routine that will
    save the current SP value in the passed
    context savearea and call the function
    whose entry point is in ep.  If the sp
    parameter is NULL, the current stack is
    used, otherwise sp becomes the new stack
    pointer.

returnto(struct lwp_context *savearea);

    Stub for Assembler routine that will
    restore context from a passed savearea
    and return to the restored C frame.
*/

static struct lwp_context {	/* saved context for dispatcher */
    void *topstack;	/* ptr to top of process stack */
    void *returnadd;	/* return address ? */
    void *ccr;		/* Condition code register */
} parent;

int savecontext (void (*f)(int), struct lwp_context *ctx, char *stack);
int returnto (struct lwp_context *ctx);
#endif

/* make stacks 16-byte aligned and leave space for silly LinuxPPC lunkage, or
 * we segfault entering functions --troy */
#define STACK_PAD 64

/* global variables used when we are initializing new threads */
static struct lwp_ucontext *child;
static void (*child_func)(void *);
static void *child_arg;

/* helper function to start a new thread */
static void _thread(int sig)
{
    struct lwp_ucontext *this = child;
    void (*func)(void *) = child_func;
    void *arg = child_arg;

    /* indicate that we copied all arguments from global state */
    child = NULL;

    if (SETJMP(this->uc_mcontext, 0) == 0)
	returnto(&parent);

    func(arg);

    if (this->uc_link)
	lwp_setcontext(this->uc_link);

    /* this 'thread' now exits. But we don't know which other context we
     * should jump to... */
    exit(0);
}

int lwp_setcontext(const struct lwp_ucontext *ucp)
{
    LONGJMP(*(JMP_BUF *)&ucp->uc_mcontext);
    return -1; /* we shouldn't get here */
}

int lwp_swapcontext(struct lwp_ucontext *oucp, const struct lwp_ucontext *ucp)
{
    if (SETJMP(oucp->uc_mcontext, 1) != 0)
	return 0;
    return lwp_setcontext(ucp);
}

void lwp_makecontext(struct lwp_ucontext *ucp, void (*func)(void *), void *arg)
{
#ifdef HAVE_SIGALTSTACK
    struct sigaction action, oldaction;
    sigset_t sigs, oldsigs;
    stack_t oldstack;
#else
    char *stack = ucp->uc_stack.ss_sp;
    assert(stack != NULL);
#endif

    /* set up state to pass to the new thread */
    child = ucp;
    child_func = func;
    child_arg = arg;

#ifdef HAVE_SIGALTSTACK
    /* we use a signal to jump onto the new stack */
    sigfillset(&sigs);
    sigprocmask(SIG_BLOCK, &sigs, &oldsigs);

    sigaltstack(&ucp->uc_stack, &oldstack);

    action.sa_handler = _thread;
    action.sa_flags = SA_ONSTACK;
    sigemptyset(&action.sa_mask);
    sigaction(SIGUSR1, &action, &oldaction);

    /* send the signal */
    kill(getpid(), SIGUSR1);

    /* and allow the signal handler to run */
    sigdelset(&sigs, SIGUSR1);
    if (!SETJMP(parent, 0))
	while (child)
	    sigsuspend(&sigs);

    /* The new context should be set up, remove our signal handler */
    sigaltstack(&oldstack, NULL);
    sigaction(SIGUSR1, &oldaction, NULL);
    sigprocmask(SIG_SETMASK, &oldsigs, NULL);

#else /* !HAVE_SIGALTSTACK */
    /* The old lwp technique looks a lot simpler now. However it requires
     * processor specific assembly which is a PITA */
#ifndef STACK_GROWS_UP
    stack += (ucp->uc_stack.ss_size-1) & ~(STACK_PAD-1);
#endif

    savecontext(_thread, &parent, stack);
#endif /* ~HAVE_SIGALTSTACK */
}

