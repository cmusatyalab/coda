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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/advice/daemon.cc,v 4.6 98/11/02 16:44:24 rvb Exp $";
#endif /*_BLURB_*/



/*
 *
 *    Daemon subsystem for Advice Monitor (stolen from Venus)
 *
 */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include "coda_assert.h" 

#include <lwp.h>
#include <timer.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>
#include "advice_srv.h"


struct TM_Elem *DaemonList;

void InitOneADay();
void DaemonInit();
void RegisterDaemon(unsigned long, char *);
void DispatchDaemons();

typedef struct DaemonInfo_t { 
        unsigned long interval;  /* in seconds */
        char *sync;              /* who to signal, if anyone */
} DaemonInfo;


void DaemonInit() {
     CODA_ASSERT(TM_Init(&DaemonList) == 0);

    /* set timer for once-a-day log messages */
    InitOneADay(); 
}

void RegisterDaemon(unsigned long interval, char *sync) {
    LogMsg(100,LogLevel,LogFile, "RegisterDaemon: %d, %x\n", interval, sync);

    CODA_ASSERT(sync != NULL);

    struct TM_Elem *tp = new TM_Elem;
    CODA_ASSERT(tp != NULL);
    tp->TotalTime.tv_sec = interval;
    tp->TotalTime.tv_usec = 0;

    struct DaemonInfo *dp = new DaemonInfo;
    CODA_ASSERT(dp != NULL);
    dp->interval = interval;
    dp->sync = sync;

    tp->BackPointer = (char *)dp;
    TM_Insert(DaemonList, tp);
}

#define SECSPERDAY 86400

void InitOneADay() {    
    /* want once-a-day tasks to run around midnight */
    unsigned long curr_time = time(0);

    /* figure out when midnight is */
    struct tm *lt = localtime((long *) &curr_time);
    CODA_ASSERT(lt != NULL);
    lt->tm_sec = lt->tm_min = lt->tm_hour = 0;       /* midnight today */
    unsigned long midnight = mktime(lt) + SECSPERDAY; /* midnight tomorrow */
    struct TM_Elem *tp = new TM_Elem;
    CODA_ASSERT(tp != NULL);
    tp->TotalTime.tv_sec = midnight - curr_time;       /* time until then */
    tp->TotalTime.tv_usec = 0;

    struct DaemonInfo *dp = new DaemonInfo;
    CODA_ASSERT(dp != NULL);
    dp->interval = SECSPERDAY;
    dp->sync = NULL;

    tp->BackPointer = (char *)dp;

    TM_Insert(DaemonList, tp);
}

void DispatchDaemons() {
    unsigned long curr_time = time(0);

    LogMsg(200,LogLevel,LogFile, "E DispatchDaemons()");
    CODA_ASSERT(DaemonList != NULL);

    int num_expired = TM_Rescan(DaemonList);
    for (int i = 0; i < num_expired; i++) {
            struct TM_Elem *tp = TM_GetExpired(DaemonList);

	    CODA_ASSERT(tp != NULL);
            TM_Remove(DaemonList, tp);

	    CODA_ASSERT(tp != NULL);
	    CODA_ASSERT((struct DaemonInfo *)tp->BackPointer != NULL);

            tp->TotalTime.tv_sec = ((struct DaemonInfo *)tp->BackPointer)->interval;
            tp->TotalTime.tv_usec = 0;
            TM_Insert(DaemonList, tp);

            if (((struct DaemonInfo *)tp->BackPointer)->sync) {
	        LWP_SignalProcess(((struct DaemonInfo *)tp->BackPointer)->sync);
	    } else   /* once a day task */
	        LogMsg(0,LogLevel,LogFile, "At the tone the time will be %s", ctime((long *)&curr_time));
    }
    LogMsg(200,LogLevel,LogFile, "L DispatchDaemons()");
}
