#ifndef _BLURB_
#define _BLURB_
/*

            Coda: an Experimental Distributed File System
                             Release 4.0

          Copyright (c) 1998 Carnegie Mellon University
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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/lib-src/base/coda_assert.c,v 1.1 1998/11/02 16:47:26 rvb Exp $";
#endif /*_BLURB_*/


#include <stdio.h>
#include <unistd.h>
#include "coda_assert.h"

int (*coda_assert_cleanup)() = (int (*)()) 0;
int   coda_assert_action = CODA_ASSERT_SLEEP;

void
coda_assert(char *pred, char *file, int line)
{
    fprintf(stderr,"Assertion failed: %s, file \"%s\", line %d\n", pred, file, line);
    fflush(stderr);

    if (coda_assert_cleanup) (coda_assert_cleanup)();

    switch (coda_assert_action) {
    default:
	fprintf(stderr,"coda_assert: bad coda_assert_action value %d, assuming CODA_ASSERT_SLEEP\n",
		coda_assert_action);
	fflush(stderr);

    case CODA_ASSERT_SLEEP:
	fprintf(stderr, "Sleeping forever.  You may use gdb to attach to process %d.",
		getpid());
	fflush(stderr);
        for (;;)
	     sleep(1);
	break;

    case CODA_ASSERT_EXIT:
	fprintf(stderr, "VENUS IS EXITTING! Bye!\n");
	fflush(stderr);
    	exit(77);
	break;

    case CODA_ASSERT_ABORT:
	fprintf(stderr, "VENUS WILL TRY TO DUMP CORE\n");
	fflush(stderr);
	abort();
	break;
    }
}

void
coda_note(char *pred, char *file, int line)
{
    fprintf(stderr,"Note failed: %s, file \"%s\", line %d\n", pred, file, line);
    fflush(stderr);
}
