#ifndef _BLURB_
#define _BLURB_
/*

            Coda: an Experimental Distributed File System
                             Release 3.1

          Copyright (c) 1987-1995 Carnegie Mellon University
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

static char *rcsid = "$Header$";
#endif /*_BLURB_*/



#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <signal.h>
#include <stdio.h>
#include <sys/time.h>
#include <libc.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <stdarg.h>
#include "util.h"

extern FILE *LogFile;
extern int LogLevel;
extern void Log_Done(void);

/* Print an error message and then exit. */
void Die(char *fmt ...) {
    static int dying = 0;

    if (!dying) {
	/* Avoid recursive death. */
	dying = 1;

	/* Log the message, with an indication that it is fatal. */
	LogMsg(-1,LogLevel,LogFile," ***** Fatal Error");
	va_list ap;
	va_start(ap, fmt);
	LogMsg(-1,LogLevel,LogFile,fmt,ap);
	va_end(ap);

    }

    /* Leave a core file. */
    kill(getpid(), SIGFPE);

    /* NOTREACHED */
}

