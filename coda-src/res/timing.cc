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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/res/timing.cc,v 4.2 1997/02/26 16:02:53 rvb Exp $";
#endif /*_BLURB_*/





/* timing.c 
 * class for recording timevalues stamped with an id
 * 	post processing prints out the delta between the 
 *	different times 
 * This package does not do any locking - external 
 *	synchronization is required for correct functionality
 */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/time.h>
#include <sys/file.h>

#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif

#include <sys/ioctl.h>
#if !defined(__GLIBC__)
#include <libc.h>
#endif
#include <assert.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include "timing.h"
/* c = a - b where a, b, c are timevals */
void tvaminustvb(struct timeval *a,
                 struct timeval *b,
                 struct timeval *c) {
    int carryover = 0;

    if (a->tv_usec < b->tv_usec) {
        carryover = 1;
        c->tv_usec = a->tv_usec + 1000000 - b->tv_usec;
    }
    else 
	c->tv_usec = a->tv_usec - b->tv_usec;
    if (carryover)
        c->tv_sec = a->tv_sec - b->tv_sec - 1;
    else
        c->tv_sec = a->tv_sec - b->tv_sec;
}

timing_path::timing_path(int n) {
    nentries = 0;
    maxentries = n;
    arr = (tpe *)malloc(sizeof(tpe) * n);
}

timing_path::~timing_path() {
    free(arr);
    
    nentries = 0;
}
void timing_path::grow_storage() {
    assert(nentries == maxentries);
    tpe *tmparr = 0;
    if (maxentries){
	tmparr = (tpe *)malloc(sizeof(tpe) * 2 * maxentries);
	bcopy(arr, tmparr, sizeof(tpe) * maxentries);
	maxentries += maxentries;
    }
    else {
	tmparr = (tpe *)malloc(sizeof(tpe) * TIMEGROWSIZE);
	maxentries += TIMEGROWSIZE;
    }
    assert(tmparr);
    free(arr);
    arr = tmparr;
	    
}
void timing_path::insert(int id) {
    if (nentries >= maxentries) 
	grow_storage();
    arr[nentries].id = id;

#ifdef _NSC_TIMING_
    extern clockFD;
#define NSC_GET_COUNTER         _IOR('c', 1, long)
    if (clockFD > 0) {
	arr[nentries].tv.tv_sec = 0;
	ioctl(clockFD, NSC_GET_COUNTER, &arr[nentries].tv.tv_usec);
    }
#else _NSC_TIMING_    
    gettimeofday(&arr[nentries].tv, NULL);
#endif _NSC_TIMING_
    nentries++;
}

void timing_path::postprocess() {
    postprocess(stdout);
}
void timing_path::postprocess(FILE *fp) {
    fflush(fp);
    postprocess(fileno(fp));
}
void timing_path::postprocess(int fd) {
    char buf[256];
    if (!nentries) {
	sprintf(buf,"PostProcess: No entries\n");
	write(fd, buf, strlen(buf));
    }
    else {
	timeval difft;
	sprintf(buf, "There are %d entries\n", nentries);
	write(fd, buf, strlen(buf));
	sprintf(buf, "Entry[0] is id: %d time (%u.%u)\n",
		arr[0].id, arr[0].tv.tv_sec, arr[0].tv.tv_usec);
	write(fd, buf, strlen(buf));
#ifdef _NSC_TIMING_
	for (int i = 1; i < nentries; i++) {
	    difft.tv_sec = 0;
	    if (arr[i].tv.tv_usec > arr[i-1].tv.tv_usec) 
		difft.tv_usec = (arr[i].tv.tv_usec - arr[i-1].tv.tv_usec)/25;
	    else {
		difft.tv_usec = (arr[i-1].tv.tv_usec - arr[i].tv.tv_usec)/25;
		difft.tv_usec = 171798692 - difft.tv_usec;
	    }
	    sprintf(buf, 
		    "Entry[%d] id: %d time (%u.%u), delta (%u secs %u usecs)\n",
		    i, arr[i].id, arr[i].tv.tv_sec, arr[i].tv.tv_usec,
		    difft.tv_sec, difft.tv_usec);
	    write(fd, buf, strlen(buf));
	}
	if (nentries > 1) {
	    difft.tv_sec = 0;
	    if (arr[nentries-1].tv.tv_usec > arr[0].tv.tv_usec) 
		difft.tv_usec = (arr[nentries-1].tv.tv_usec - arr[0].tv.tv_usec)/25;
	    else {
		difft.tv_usec = (arr[0].tv.tv_usec - arr[nentries-1].tv.tv_usec)/25;
		difft.tv_usec = 171798692 - difft.tv_usec;
	    }
	    sprintf(buf, "Final delta between entry id %d and %d is %u secs %u usecs\n",
		    arr[nentries-1].id, arr[0].id, difft.tv_sec, difft.tv_usec);
	    write(fd, buf, strlen(buf));
	}
#else _NSC_TIMING_	
	for (int i = 1; i < nentries; i++) {
	    tvaminustvb(&arr[i].tv, &arr[i-1].tv, &difft);
	    sprintf(buf, 
		    "Entry[%d] id: %d time (%u.%u), delta (%u secs %u usecs)\n",
		    i, arr[i].id, arr[i].tv.tv_sec, arr[i].tv.tv_usec,
		    difft.tv_sec, difft.tv_usec);
	    write(fd, buf, strlen(buf));
	}
	if (nentries > 1) {
	    tvaminustvb(&arr[nentries-1].tv, &arr[0].tv, &difft);
	    sprintf(buf, "Final delta between entry id %d and %d is %u secs %u usecs\n",
		    arr[nentries-1].id, arr[0].id, difft.tv_sec, difft.tv_usec);
	    write(fd, buf, strlen(buf));
	}
#endif _NSC_TIMING_	
    }
}
