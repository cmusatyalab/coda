/* BLURB lgpl

                           Coda File System
                              Release 5

          Copyright (c) 2005 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights

#*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include "lwp_stacktrace.h"

#ifndef HAVE_REGISTER_T
typedef int register_t;
#endif

void lwp_stacktrace(FILE *fp, void *top, stack_t *stack)
{
    /* Add others here as needed */
#if defined(__i386__)
    register_t ip;
    register_t ebp;
    register_t *esp;
    int count;

    /* Set current stack pointer to top */
    /* top is the address of the first parameter on the stack and it is an
     * integer. Next to this _should_ be the return address and the previous
     * stack pointer */
    esp = (register_t *)top;
    esp -= 2;

    fprintf(fp, "Call Trace:\n [<unknown>]");
    while (esp) {
        ebp = *(esp++); /* LEAVE */
        ip  = *(esp++); /* RET */

        /* make sure we stay within the allocated stack frame */
        if ((register_t *)ebp <= esp ||
            (stack && stack->ss_sp &&
             (char *)ebp >= (char *)stack->ss_sp + stack->ss_size))
            break;

        //for (count = 0; count < 6 && esp < (register_t *)ebp; count++)
        for (count = 0; esp < (register_t *)ebp; count++)
            fprintf(fp, " <%08x>", *(esp++));

        fprintf(fp, "\n [<%08x>] ", ip);

        esp = (register_t *)ebp;
    }
    fprintf(fp, "\n");
#endif
}
