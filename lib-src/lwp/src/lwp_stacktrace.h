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

#ifndef LWP_STACKTRACE_H
#define LWP_STACKTRACE_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <signal.h>

#ifndef HAVE_STACK_T
typedef struct sigaltstack {
    void *ss_sp;
    int ss_flags;
    size_t ss_size;
} stack_t;
#endif

void lwp_stacktrace(FILE *fp, void *top, stack_t *stack);

#endif /* LWP_STACKTRACE_H */

