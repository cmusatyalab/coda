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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/venus/daemon.cc,v 4.7 1998/09/29 21:04:40 jaharkes Exp $";
#endif /*_BLURB_*/







/*
 *
 *    Daemon subsystem for Venus
 *
 */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <sys/param.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>


#ifdef __cplusplus
}
#endif __cplusplus

#include "vice.h"

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <cfs/coda.h>
#include <timer.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include "vproc.h"
#include "venus.private.h"


static struct TM_Elem *DaemonList;

void InitOneADay();

struct DaemonInfo { 
	unsigned long interval;  /* in seconds */
	char *sync;              /* who to signal, if anyone */
};

void DaemonInit() {
    if (TM_Init(&DaemonList))
	CHOKE("Couldn't create DaemonList!");

    /* set timer for once-a-day log messages */
    InitOneADay(); 
}

void RegisterDaemon(unsigned long interval, char *sync) {
    LOG(100, ("RegisterDaemon:\n"));

    struct TM_Elem *tp = (struct TM_Elem *) malloc(sizeof(struct TM_Elem));
    tp->TotalTime.tv_sec = interval;
    tp->TotalTime.tv_usec = 0;

    struct DaemonInfo *dp = (struct DaemonInfo *) malloc(sizeof(struct DaemonInfo));
    dp->interval = interval;
    dp->sync = sync;

    tp->BackPointer = (char *)dp;
    TM_Insert(DaemonList, tp);
}

#define SECSPERDAY 86400

void InitOneADay() {    
    /* want once-a-day tasks to run around midnight */
    unsigned long curr_time = Vtime();

    /* figure out when midnight is */
    struct tm *lt = localtime((time_t *) &curr_time);
    lt->tm_sec = lt->tm_min = lt->tm_hour = 0;       /* midnight today */

    unsigned long midnight = mktime(lt) + SECSPERDAY; /* midnight tomorrow */
    struct TM_Elem *tp = (struct TM_Elem *) malloc(sizeof(struct TM_Elem));
    tp->TotalTime.tv_sec = midnight - curr_time;       /* time until then */
    tp->TotalTime.tv_usec = 0;

    struct DaemonInfo *dp = (struct DaemonInfo *) malloc(sizeof(struct DaemonInfo));
    dp->interval = SECSPERDAY;
    dp->sync = NULL;

    tp->BackPointer = (char *)dp;
    TM_Insert(DaemonList, tp);	
}

void DispatchDaemons() {
    unsigned long curr_time = Vtime();

    int num_expired = TM_Rescan(DaemonList);
    for (int i = 0; i < num_expired; i++) {
	    struct TM_Elem *tp = TM_GetExpired(DaemonList);
	    TM_Remove(DaemonList, tp);

	    tp->TotalTime.tv_sec = ((struct DaemonInfo *)tp->BackPointer)->interval;
	    tp->TotalTime.tv_usec = 0;
	    TM_Insert(DaemonList, tp);

	    if (((struct DaemonInfo *)tp->BackPointer)->sync) 
		    VprocSignal(((struct DaemonInfo *)tp->BackPointer)->sync);
	    else {  /* once a day task */
		    LOG(0, ("At the tone the time will be %s", ctime((time_t *)&curr_time)));
		    RusagePrint(fileno(logFile));
		    MallocPrint(fileno(logFile));
	    }
    }
}

