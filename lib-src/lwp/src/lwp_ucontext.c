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
static sigjmp_buf parent;
#define returnto(ctx) siglongjmp(*ctx, 1)
#else

/* otherwise, use the old LWP savecontext/returnto assembly routines */
struct lwp_context {	/* saved context for dispatcher */
    char *topstack;	/* ptr to top of process stack */
    char *returnadd;	/* return address ? */
    char *ccr;		/* Condition code register */
};
static struct lwp_context parent;

int savecontext (void (*f)(void), struct lwp_context *ctx, char *stack);
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
    child = NULL;

    if (sigsetjmp(this->uc_mcontext, 0) == 0)
	returnto(&parent);

    /* restore signal mask */
    sigprocmask(SIG_SETMASK, &this->uc_sigmask, NULL);

    func(arg);

    if (this->uc_link)
	lwp_setcontext(this->uc_link);

    /* this 'thread' now exits. But we don't know which other context we
     * should jump to... */
    exit(0);
}

void lwp_initctx(struct lwp_ucontext *ucp)
{
    sigset_t sigempty;
    sigemptyset(&sigempty);
    memset(ucp, 0, sizeof(*ucp));

    /* save the signal mask of the current thread */
    sigprocmask(SIG_BLOCK, &sigempty, &ucp->uc_sigmask);
}

int lwp_setcontext(const struct lwp_ucontext *ucp)
{
    siglongjmp(*(sigjmp_buf *)&ucp->uc_mcontext, 1);
    assert(0); /* we should never get here */
}

int lwp_swapcontext(struct lwp_ucontext *oucp, const struct lwp_ucontext *ucp)
{
    if (sigsetjmp(oucp->uc_mcontext, 1) == 0)
	lwp_setcontext(ucp);
    return 0;
}

void lwp_makecontext(struct lwp_ucontext *ucp, void (*func)(void *), void *arg)
{
    char *stack = ucp->uc_stack.ss_sp;

#ifdef HAVE_SIGALTSTACK
    struct sigaction action, oldaction;
    sigset_t sigs, oldsigs;
    stack_t oldstack;
#endif

    assert(stack != NULL);

    child = ucp;
    child_func = func;
    child_arg = arg;

#ifdef HAVE_SIGALTSTACK
    action.sa_handler = _thread;
    action.sa_flags = SA_ONSTACK;
    sigemptyset(&action.sa_mask);

    sigemptyset(&sigs);
    sigaddset(&sigs, SIGUSR1);

    /* we use a signal to jump into the new stack */
    sigprocmask(SIG_BLOCK, &sigs, &oldsigs);
    sigaltstack(&ucp->uc_stack, &oldstack);
    sigaction(SIGUSR1, &action, &oldaction);

    kill(getpid(), SIGUSR1);

    /* handle the SIGUSR1 signal */
    sigfillset(&sigs);
    sigdelset(&sigs, SIGUSR1);
    if (!sigsetjmp(parent, 1))
	while (child)
	    sigsuspend(&sigs);

    /* The new context should be set up, now revert to the old state... */
    sigaltstack(&oldstack, NULL);
    sigaction(SIGUSR1, &oldaction, NULL);
    sigprocmask(SIG_SETMASK, &oldsigs, NULL);

#else /* !HAVE_SIGALTSTACK */

#ifndef STACK_GROWS_UP
    stack += (ucp->uc_stack.ss_size-1) & ~(STACK_PAD-1);
#endif

    /* The old lwp technique looks a lot simpler now doesn't it. However
     * it requires processor specific assembly which is a PITA */
    savecontext(_thread, &parent, stack);

#endif /* ~HAVE_SIGALTSTACK */
}

/*  The following documents the Assembler interfaces used by old LWP:

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

