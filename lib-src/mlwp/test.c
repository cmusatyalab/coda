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

/*
                         IBM COPYRIGHT NOTICE

                          Copyright (C) 1986
             International Business Machines Corporation
                         All Rights Reserved

This  file  contains  some  code identical to or derived from the 1986
version of the Andrew File System ("AFS"), which is owned by  the  IBM
Corporation.   This  code is provided "AS IS" and IBM does not warrant
that it is free of infringement of  any  intellectual  rights  of  any
third  party.    IBM  disclaims  liability of any kind for any damages
whatsoever resulting directly or indirectly from use of this  software
or  of  any  derivative work.  Carnegie Mellon University has obtained
permission to  modify,  distribute and sublicense this code,  which is
based on Version 2  of  AFS  and  does  not  contain  the features and
enhancements that are part of  Version 3 of  AFS.  Version 3 of AFS is
commercially   available   and  supported  by   Transarc  Corporation,
Pittsburgh, PA.

*/

#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include "coda_assert.h"
#include "lwp.h"

char semaphore;

void OtherProcess(void)
    {
    for(;;)
	{
	LWP_SignalProcess(&semaphore);
	}
    }

int main(int argc, char **argv)
{
    struct timeval t1, t2;
    struct timeval sleeptime;
    PROCESS pid, otherpid;
    register long i,  count, x;
    int j;
    char *waitarray[2];
    static char c[] = "OtherProcess";
    
    count = atoi(argv[1]);


    cont_sw_threshold.tv_sec = 0;
    cont_sw_threshold.tv_usec = 10000;
    last_context_switch.tv_sec = 0;
    last_context_switch.tv_usec = 0;

    CODA_ASSERT(LWP_Init(LWP_VERSION, 0, (PROCESS *)&pid) == LWP_SUCCESS);
    CODA_ASSERT(LWP_CreateProcess((PFI)OtherProcess,4096,0, 0, c, (PROCESS *)&otherpid) == LWP_SUCCESS);
    CODA_ASSERT(IOMGR_Initialize() == LWP_SUCCESS);
    waitarray[0] = &semaphore;
    waitarray[1] = 0;
    gettimeofday(&t1, NULL);
    for (i = 0; i < count; i++)
	{
	for (j = 0; j < 100000; j++);
	sleeptime.tv_sec = i;
	sleeptime.tv_usec = 0;
	IOMGR_Select(0, NULL, NULL, NULL, &sleeptime);
	LWP_MwaitProcess(1, waitarray);
	}
    gettimeofday(&t2, NULL);

    if (count)
    {
	x = (t2.tv_sec -t1.tv_sec)*1000000 + (t2.tv_usec - t1.tv_usec);
	printf("%ld milliseconds for %ld MWaits (%f usec per Mwait and Signal)\n", x/1000, count, (float)(x/count));
    }

    return 0;

}
