#ifndef _BLURB_
#define _BLURB_
/*

            Coda: an Experimental Distributed File System
                             Release 4.0

          Copyright (c) 1987-1996 Carnegie Mellon University
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

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/lib-src/mlwp/fasttime.c,v 1.2 1997/01/07 18:44:30 rvb Exp";
#endif /*_BLURB_*/


/*

                         IBM COPYRIGHT NOTICE

                          Copyright (C) 1986
             International Business Machines Corporation
                         All Rights Reserved

This  file  contains  some  code identical to or derived from the 1986
version of the Andrew File System ("AFS"), which is owned by  the  IBM
Corporation.    This  code is provded "AS IS" and IBM does not warrant
that it is free of infringement of  any  intellectual  rights  of  any
third  party.    IBM  disclaims  liability of any kind for any damages
whatsoever resulting directly or indirectly from use of this  software
or  of  any  derivative work.  Carnegie Mellon University has obtained
permission to distribute this code, which is based on Version 2 of AFS
and  does  not  contain the features and enhancements that are part of
Version 3 of AFS.  Version 3 of  AFS  is  commercially  available  and
supported by Transarc Corporation, Pittsburgh, PA.

*/


#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#ifdef	__linux__
#include <unistd.h>
#else
#endif
#include <sys/file.h>
#include <sys/mman.h>

extern char *valloc ();
int ft_debug;


#ifdef notdef
#ifdef sun
#include <nlist.h>

PRIVATE  struct nlist nl[] = {
    { "_time" },
#define X_TIME		0
    { "" },
};
#endif sun
#endif notdef

#define TRUE	1
#define FALSE	0

PRIVATE enum InitState { notTried, tried, done } initState = notTried;

#ifdef notdef
#ifdef sun
PRIVATE int memFd;	/* our fd on /dev/mem */
PRIVATE char *mem;	/* pointer to our valloced memory */
PRIVATE struct timeval *timeP;	/* pointer to time in our address space */
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
    pageSize = getpagesize ();
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
