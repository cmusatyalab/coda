/* BLURB lgpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

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

#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>

#include <lwp/lwp.h>

static enum InitState { notTried, tried, done } initState = notTried;
/* last time returned by RT_FastTime.  Used to implement FT_ApproxTime */
struct timeval FT_LastTime;

/* 
   This routine was used on SUN's to memory map the time.
*/
int FT_Init(int printErrors, int notReally)
{
    return -1;
    /* This is in case explicit initialization occurs after
           automatic initialization */

    if (initState != notTried && !notReally)
        return (initState == done ? 0 : -1);
    initState = tried;

    /* fake success, but leave initState wrong. */
    if (notReally)
        return 0;
    if (printErrors)
        fprintf(stderr, "FT_Init: mmap  not implemented on this kernel\n");
    return (-1);
}

/* Call this to get the time of day.  It will automatically initialize
   the first time you call it.  If you want error messages when you
   initialize, call FT_Init yourself.  If the initialization failed,
   this will just call gettimeofday.  If you ask for the timezone
   info, this routine will punt to gettimeofday. */

int FT_GetTimeOfDay(struct timeval *tv, struct timezone *tz)
{
    register int ret;
    ret = gettimeofday(tv, tz);
    if (!ret) {
        FT_LastTime.tv_sec  = tv->tv_sec;
        FT_LastTime.tv_usec = tv->tv_usec;
    }
    return ret;
}

/* For compatibility.  Should go away. */
int TM_GetTimeOfDay(struct timeval *tv, struct timezone *tz)
{
    return FT_GetTimeOfDay(tv, tz);
}

int FT_AGetTimeOfDay(struct timeval *tv, struct timezone *tz)
{
    if (FT_LastTime.tv_sec) {
        tv->tv_sec  = FT_LastTime.tv_sec;
        tv->tv_usec = FT_LastTime.tv_usec;
        return 0;
    }
    return FT_GetTimeOfDay(tv, tz);
}

unsigned int FT_ApproxTime()
{
    if (!FT_LastTime.tv_sec) {
        FT_GetTimeOfDay(&FT_LastTime, 0);
    }
    return FT_LastTime.tv_sec;
}
