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
                           none currently

#*/

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

    DaemonInfo *dp = new DaemonInfo;
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

    DaemonInfo *dp = new DaemonInfo;
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
	    CODA_ASSERT((DaemonInfo *)tp->BackPointer != NULL);

            tp->TotalTime.tv_sec = ((DaemonInfo *)tp->BackPointer)->interval;
            tp->TotalTime.tv_usec = 0;
            TM_Insert(DaemonList, tp);

            if (((DaemonInfo *)tp->BackPointer)->sync) {
	        LWP_SignalProcess(((DaemonInfo *)tp->BackPointer)->sync);
	    } else   /* once a day task */
	        LogMsg(0,LogLevel,LogFile, "At the tone the time will be %s", ctime((long *)&curr_time));
    }
    LogMsg(200,LogLevel,LogFile, "L DispatchDaemons()");
}
