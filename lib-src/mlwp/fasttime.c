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

extern char *valloc ();
int ft_debug;


#ifdef notdef
#ifdef sun
#include <nlist.h>

static  struct nlist nl[] = {
    { "_time" },
#define X_TIME		0
    { "" },
};
#endif sun
#endif notdef

#define TRUE	1
#define FALSE	0

static enum InitState { notTried, tried, done } initState = notTried;

#ifdef notdef
#ifdef sun
static int memFd;	/* our fd on /dev/mem */
static char *mem;	/* pointer to our valloced memory */
static struct timeval *timeP;	/* pointer to time in our address space */
#endif
#endif notdef

struct timeval FT_LastTime;	/* last time returned by RT_FastTime.  Used to implement
			    	   FT_ApproxTime */


/* Call this to get the memory mapped.  It will return -1 if anything went
   wrong.  In that case, calls to FT_GetTimeOfDay will call gettimeofday
   instead.  If printErrors is true, errors in initialization will cause
   error messages to be printed on stderr.  If notReally is true, then
   things are set up so that all calls to FT_GetTimeOfDay call gettimeofday.
   You might want this if your program won't run too long and the nlist
   call is too expensive.  Yeah, it's pretty horrible.
*/
int FT_Init(printErrors, notReally)
    int printErrors;
    int notReally;{

#ifdef notdef
#ifdef sun			/* Alas, Vaxen and SBs do not have mmap()
				   implemented in their 4.2 yet */
#ifndef sunV3
    struct timeval  tv1, tv2;
    int     pageSize, pageMask;
    int     timeOff, timeEnd;
    int     start;
    int     nPages;
#endif sunV3
#endif
#endif notdef

    if (initState != notTried && !notReally)
	return (initState == done? 0: -1);	/* This is in case explicit initialization
						   occurs after automatic initialization */
    initState = tried;
    if (notReally)
	return 0;		/* fake success, but leave initState
				   wrong. */
#ifdef notdef
#ifdef sun
#ifndef sunV3
    nlist ("/vmunix", nl);
    if (nl[0].n_type == 0) {	/* no namelist */
	if (printErrors)
	    fprintf (stderr, "FT_Init: can't find name list.\n");
	return (-1);
    }
    if ((memFd = open ("/dev/mem", O_RDONLY)) < 0) {
	if (printErrors)
	    fprintf (stderr, "FT_Init: can't open /dev/mem.\n");
	return (-1);
    }
    lseek (memFd, nl[X_TIME].n_value, 0);
    read (memFd, &tv1, sizeof (tv1));
    gettimeofday (&tv2, 0);
    if (tv2.tv_sec - tv1.tv_sec < 0 || tv2.tv_sec - tv1.tv_sec > 5) {
	/* Either the system is REALLY loaded, or we've got the wrong
	   kernel. */
	close (memFd);
	if (printErrors)
	    fprintf (stderr, "FT_Init: looks like bogus kernel.\n");
	return (-1);
    }
    /* Ok, now figure out where the variables are so we can map them. */
    pageSize = getpagesize();
    pageMask = pageSize - 1;
    timeOff = nl[X_TIME].n_value;
    timeEnd = timeOff + sizeof (struct timeval);
    start = timeOff & ~pageMask;
    nPages = (timeEnd - start + pageSize - 1) / pageSize;
    /* Get some memory to put them in... */
    if ((mem = valloc (nPages * pageSize)) == NULL) {
	close (memFd);
	if (printErrors)
	    fprintf (stderr, "FT_Init: can't valloc memory.\n");
	return (-1);
    }
    /* map it */
    if (mmap (mem, nPages * pageSize, PROT_READ, MAP_SHARED, memFd, start) < 0) {
	close (memFd);
	if (printErrors)
	    fprintf (stderr, "FT_Init: can't mmap kernel.\n");
	return (-1);
    }
    /* And compute the new addresses. */
    timeP = (struct timeval *) (mem + (timeOff - start));
    initState = done;
    return 0;
#endif sunV3
#endif sun
#endif notdef
    if (printErrors)
	fprintf (stderr, "FT_Init: mmap  not implemented on this kernel\n");
    return (-1);
}


/* Call this to get the time of day.  It will automatically initialize the
   first time you call it.  If you want error messages when you initialize,
   call FT_Init yourself.  If the initialization failed, this will just
   call gettimeofday.  If you ask for the timezone info, this routine will
   punt to gettimeofday. */


int FT_GetTimeOfDay(tv, tz)
    register struct timeval *tv;
    register struct timezone *tz;{
    register int ret;
#ifdef notdef
#ifdef sun			/* Alas, Vaxen and SBs do not have mmap()
				   implemented in their 4.2 yet */
				 
#ifndef sunV3	/* Valloc screwes-up because of new malloc... (only on sun2v3.0)*/
    if (initState != done || tz != NULL) {
	if (initState == notTried)
	    FT_Init(FALSE, FALSE);
	if (initState != done || tz != NULL) {
	    if (ft_debug && tz != NULL)
		printf ("FT_GetTimeOfDay --> gettimeofday()\n");
	    ret = gettimeofday (tv, tz);
	    if (!ret) {
	        FT_LastTime.tv_sec = tv->tv_sec;
		FT_LastTime.tv_usec = tv->tv_usec;
	    }
	    return ret;
	}
    }
    if (tv != NULL) {
	/* A structure assignment would not be indivisible, so assign the
	   parts separately and loop until we see the same value for
	   seconds twice.  This guarantees that we see monotonically
	   increasing time values. */
	do {
	    tv->tv_sec = timeP->tv_sec;
	    tv->tv_usec = timeP->tv_usec;
	} while (tv->tv_sec != timeP->tv_sec);
	FT_LastTime = *tv;
    }
    return 0;
#endif sunV3
#endif sun
#endif notdef
    ret = gettimeofday (tv, tz);
    if (!ret) {
	FT_LastTime.tv_sec = tv->tv_sec;
	FT_LastTime.tv_usec = tv->tv_usec;
    }
    return ret;
}


/* For compatibility.  Should go away. */
int TM_GetTimeOfDay(tv, tz)
    struct timeval *tv;
    struct timezone *tz;
{
    return FT_GetTimeOfDay(tv, tz);
}

int FT_AGetTimeOfDay(tv, tz)
    struct timeval *tv;
    struct timezone *tz;
{
    if (FT_LastTime.tv_sec) {
	tv->tv_sec = FT_LastTime.tv_sec;
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
