/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights

#*/

#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <assert.h>

#include <lwp/lwp.h>

void OtherProcess(PROCESS parent)
    {
    for(;;)
	{
        /* we just yield */
#if 0
	LWP_DispatchProcess();
#else
	LWP_QSignal(parent);
        LWP_QWait();
#endif
	}
    }

int main(int argc, char **argv)
{
    struct timeval t1, t2;
    PROCESS pid, otherpid;
    register long i,  count, x;
    static char c[] = "OtherProcess";
    
    count = atoi(argv[1]);

    cont_sw_threshold.tv_sec = 0;
    cont_sw_threshold.tv_usec = 10000;
    last_context_switch.tv_sec = 0;
    last_context_switch.tv_usec = 0;

    assert(LWP_Init(LWP_VERSION, 0, &pid) == LWP_SUCCESS);
    assert(LWP_CreateProcess((PFI)OtherProcess, 16384, 0, (char *)pid, c, &otherpid) == LWP_SUCCESS);
    assert(IOMGR_Initialize() == LWP_SUCCESS);
    gettimeofday(&t1, NULL);

    for (i = 0; i < count; i++) {
#if 0
        LWP_DispatchProcess();
#else
        LWP_QSignal(otherpid);
        LWP_QWait();
#endif
    }

    gettimeofday(&t2, NULL);

    if (count)
    {
	x = (t2.tv_sec -t1.tv_sec)*1000000 + (t2.tv_usec - t1.tv_usec);
	printf("%ld milliseconds for %ld Yields (%f usec per Yield)\n", x/1000, count, (float)(x/count));
    }

    LWP_TerminateProcessSupport();
    exit(0);
}
