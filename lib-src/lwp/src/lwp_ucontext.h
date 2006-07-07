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

#ifndef LWP_UCONTEXT_H
#define LWP_UCONTEXT_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef CODA_USE_UCONTEXT
#include <ucontext.h>

#else /* !CODA_USE_UCONTEXT */
#include <setjmp.h>
#include <signal.h>
#include "lwp_stacktrace.h"

typedef sigjmp_buf mcontext_t;
typedef struct ucontext {
    struct ucontext *uc_link;
    sigset_t uc_sigmask;
    stack_t uc_stack;
    mcontext_t uc_mcontext;
} ucontext_t;

void _lwp_initctx(ucontext_t *ucp);
#define getcontext(ucp) \
    (_lwp_initctx(ucp), sigsetjmp((ucp)->uc_mcontext, 1), 0)
int setcontext(const ucontext_t *ucp);
void makecontext(ucontext_t *ucp, void (*func)(), int argc, ...);
int swapcontext(ucontext_t *oucp, ucontext_t *ucp);
#endif /* !CODA_USE_UCONTEXT */

#endif /* LWP_UCONTEXT_H */
