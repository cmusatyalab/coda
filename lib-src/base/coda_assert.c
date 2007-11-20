/* BLURB lgpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "coda_assert.h"

void (*coda_assert_cleanup)() = (void (*)()) 0;
int   coda_assert_action = CODA_ASSERT_SLEEP;

void
coda_assert(const char *pred, const char *file, int line)
{
    fprintf(stderr,"Assertion failed: %s, file \"%s\", line %d\n", pred, file, line);
    fflush(stderr);

    if (coda_assert_cleanup) (coda_assert_cleanup)();

    switch (coda_assert_action) {
    default:
	fprintf(stderr,"coda_assert: bad coda_assert_action value %d, assuming CODA_ASSERT_SLEEP\n", coda_assert_action);
	fflush(stderr);

    case CODA_ASSERT_SLEEP:
	fprintf(stderr, "Sleeping forever.  You may use gdb to attach to process %d.",
		(int)getpid());
	fflush(stderr);
        for (;;)
	     sleep(1);
	break;

    case CODA_ASSERT_EXIT:
	fprintf(stderr, "EXITING! Bye!\n");
	fflush(stderr);
    	exit(77);
	break;

    case CODA_ASSERT_ABORT:
	fprintf(stderr, "TRYING TO DUMP CORE\n");
	fflush(stderr);
	abort();
	break;
    }
}

void
coda_note(const char *pred, const char *file, int line)
{
    fprintf(stderr,"Note failed: %s, file \"%s\", line %d\n", pred, file, line);
    fflush(stderr);
}

