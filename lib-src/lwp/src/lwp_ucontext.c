/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 2005 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights

#*/

#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>
#include "lwp_ucontext.h"

/* if we already have ucontext.h we don't need any of this */
#ifndef HAVE_UCONTEXT_H

/* Some architectures have a stack that grows up instead of down */
//#define STACK_GROWS_UP 1

/* next most portable alternative is to use sigaltstack */
#ifdef HAVE_SIGALTSTACK
#define returnto(ctx) return
#else
/* otherwise, use the old LWP savecontext/returnto assembly routines */
struct lwp_context {	/* saved context for dispatcher */
    char *topstack;	/* ptr to top of process stack */
    char *returnadd;	/* return address ? */
    char *ccr;		/* Condition code register */
};

int savecontext (void (*f)(void), struct lwp_context *ctx, char *stack);
int returnto (struct lwp_context *ctx);

static struct lwp_context parent;
#endif

/* make stacks 16-byte aligned and leave space for silly LinuxPPC lunkage, or
 * we segfault entering functions --troy */
#define STACK_PAD 64

/* global variables used when initializing new threads */
static ucontext_t *child;
static void (*child_func)();
static int child_argc;
static void *child_arg;

/* helper function to start a new thread */
static void _thread(void)
{
    ucontext_t *this = child;
    void (*func)() = child_func;
    int argc = child_argc;
    void *arg = child_arg;
    child = NULL;

    if (sigsetjmp(this->uc_mcontext, 0) == 0)
	returnto(&parent);
    
    /* restore signal mask */
    sigprocmask(SIG_SETMASK, &this->uc_sigmask, NULL);

    if (argc)
	func(arg);
    else
	func();

    if (this->uc_link)
	setcontext(this->uc_link);

    /* according to the makecontext documentation, this 'thread' now exits.
     * But we don't know which other 'context' we should jump to... */
    exit(0);
}

/* Implementations for getcontext, setcontext, makecontext, and swapcontext */
void _lwp_initctx(ucontext_t *ucp)
{
    sigset_t sigempty;
    sigemptyset(&sigempty);
    memset(ucp, 0, sizeof(ucontext_t));

    /* save the signal mask of the current thread */
    sigprocmask(SIG_BLOCK, &sigempty, &ucp->uc_sigmask);
}

int setcontext(const ucontext_t *ucp)
{
    siglongjmp(*(sigjmp_buf *)&ucp->uc_mcontext, 1);
    assert(0); /* we should never get here */
}

int swapcontext(ucontext_t *oucp, ucontext_t *ucp)
{
    if (sigsetjmp(oucp->uc_mcontext, 1) == 0)
	siglongjmp(ucp->uc_mcontext, 1);
    return 0;
}

void makecontext(ucontext_t *ucp, void (*func)(), int argc, ...)
{
    va_list ap;
    char *stack = ucp->uc_stack.ss_sp;

#ifdef HAVE_SIGALTSTACK
    struct sigaction action, oldaction;
    sigset_t sigs, oldsigs;
    stack_t oldstack;
#endif

    assert(stack != NULL);

    child = ucp;
    child_func = func;
    child_argc = argc;

    if (argc) {
	assert(argc <= 1);
	va_start(ap, argc);
	child_arg = va_arg(ap, void *);
	va_end(ap);
    }

#ifndef HAVE_SIGALTSTACK
#ifndef STACK_GROWS_UP
    stack += (ucp->uc_stack.ss_size-1) & ~(STACK_PAD-1);
#endif

    /* The old lwp technique looks a lot simpler now doesn't it. However
     * it requires processor specific assembly which is a PITA */
    savecontext(_thread, &parent, stack);

#else /* HAVE_SIGALTSTACK */

    action.sa_handler = (void(*)(int))_thread;
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
    while (child)
	sigsuspend(&sigs);

    /* The new context should be set up, now revert to the old state... */
    sigaltstack(&oldstack, NULL);
    sigaction(SIGUSR1, &oldaction, NULL);
    sigprocmask(SIG_SETMASK, &oldsigs, NULL);
#endif /* HAVE_SIGALTSTACK */
}
#endif
