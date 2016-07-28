/* BLURB lgpl

			Coda File System
			    Release 5

	Copyright (c) 2005-2016 Carnegie Mellon University
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

#ifdef SAVE_SIGMASK
#define JMP_BUF sigjmp_buf
#define SETJMP(x,y) sigsetjmp(x, y)
#define LONGJMP(x) siglongjmp(x, 1)
#else
#define JMP_BUF jmp_buf
#define SETJMP(x,y) setjmp(x)
#define LONGJMP(x) longjmp(x, 1)
#endif

struct lwp_ucontext {
    struct lwp_ucontext *uc_link;
    stack_t		 uc_stack;
    JMP_BUF		 uc_mcontext;
};

/* SETJMP _has_ to be in a macro, because we can not call LONGJMP when we
 * unwrap the stack, i.e. return from a 'lwp_getcontext' function. */
#define lwp_getcontext(ucp) ( memset((ucp), 0, sizeof(struct lwp_ucontext)), \
			      SETJMP((ucp)->uc_mcontext, 1) )
int lwp_setcontext(const struct lwp_ucontext *ucp);
void lwp_makecontext(struct lwp_ucontext *ucp, void (*func)(void *), void *arg);
int lwp_swapcontext(struct lwp_ucontext *oucp, const struct lwp_ucontext *ucp);

#endif /* LWP_UCONTEXT_H */

