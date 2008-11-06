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

#ifndef LWP_UCONTEXT_H
#define LWP_UCONTEXT_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <setjmp.h>
#include <signal.h>
#include "lwp_stacktrace.h"

struct lwp_ucontext {
    struct lwp_ucontext *uc_link;
    sigset_t uc_sigmask;
    stack_t uc_stack;
    sigjmp_buf uc_mcontext;
};

void lwp_initctx(struct lwp_ucontext *ucp);
#define lwp_getcontext(ucp) \
    (lwp_initctx(ucp), sigsetjmp((ucp)->uc_mcontext, 1), 0)
int lwp_setcontext(const struct lwp_ucontext *ucp);
void lwp_makecontext(struct lwp_ucontext *ucp, void (*func)(void *), void *arg);
int lwp_swapcontext(struct lwp_ucontext *oucp, const struct lwp_ucontext *ucp);

#endif /* LWP_UCONTEXT_H */
