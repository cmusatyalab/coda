/* BLURB gpl

                           Coda File System
                              Release 7

          Copyright (c) 1987-2019 Carnegie Mellon University
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
 *    Daemon subsystem for Venus
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <sys/param.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <lwp/timer.h>

#ifdef __cplusplus
}
#endif

#include "vice.h"
#include "vproc.h"
#include "venus.private.h"

static struct TM_Elem *DaemonList;

void InitOneADay();

struct DaemonInfo {
    unsigned long interval; /* in seconds */
    char *sync; /* who to signal, if anyone */
};

void DaemonInit()
{
    if (TM_Init(&DaemonList))
        CHOKE("Couldn't create DaemonList!");

    /* set timer for once-a-day log messages */
    InitOneADay();
}

void RegisterDaemon(unsigned long interval, char *sync)
{
    LOG(100, ("RegisterDaemon:\n"));

    struct TM_Elem *tp    = (struct TM_Elem *)malloc(sizeof(struct TM_Elem));
    tp->TotalTime.tv_sec  = interval;
    tp->TotalTime.tv_usec = 0;

    struct DaemonInfo *dp =
        (struct DaemonInfo *)malloc(sizeof(struct DaemonInfo));
    dp->interval = interval;
    dp->sync     = sync;

    tp->BackPointer = (char *)dp;
    TM_Insert(DaemonList, tp);
}

#define SECSPERDAY 86400

void InitOneADay()
{
    /* want once-a-day tasks to run around midnight */
    time_t curr_time = Vtime();

    /* figure out when midnight is */
    struct tm *lt = localtime((time_t *)&curr_time);
    lt->tm_sec = lt->tm_min = lt->tm_hour = 0; /* midnight today */

    unsigned long midnight = mktime(lt) + SECSPERDAY; /* midnight tomorrow */
    struct TM_Elem *tp     = (struct TM_Elem *)malloc(sizeof(struct TM_Elem));
    tp->TotalTime.tv_sec   = midnight - curr_time; /* time until then */
    tp->TotalTime.tv_usec  = 0;

    struct DaemonInfo *dp =
        (struct DaemonInfo *)malloc(sizeof(struct DaemonInfo));
    dp->interval = SECSPERDAY;
    dp->sync     = NULL;

    tp->BackPointer = (char *)dp;
    TM_Insert(DaemonList, tp);
}

void DispatchDaemons()
{
    time_t curr_time = Vtime();

    int num_expired = TM_Rescan(DaemonList);
    for (int i = 0; i < num_expired; i++) {
        struct TM_Elem *tp = TM_GetExpired(DaemonList);
        TM_Remove(DaemonList, tp);

        tp->TotalTime.tv_sec = ((struct DaemonInfo *)tp->BackPointer)->interval;
        tp->TotalTime.tv_usec = 0;
        TM_Insert(DaemonList, tp);

        if (((struct DaemonInfo *)tp->BackPointer)->sync)
            VprocSignal(((struct DaemonInfo *)tp->BackPointer)->sync);
        else { /* once a day task */
            LOG(0, ("At the tone the time will be %s",
                    ctime((time_t *)&curr_time)));
            RusagePrint(fileno(GetLogFile()));
            MallocPrint(fileno(GetLogFile()));
        }
    }
}

/* helper functions to create a simple daemon thread */
class Daemon : protected vproc {
public:
    char sync;

    Daemon(const char *name, PROCBODY function, int interval, int stacksize);
    ~Daemon();

private:
    PROCBODY function;
    int interval;

    void main(void);
};

Daemon::Daemon(const char *name, PROCBODY f, int i, int stacksize)
    : vproc(name, NULL, VPT_Daemon, stacksize)
{
    function = f;
    interval = i;

    start_thread();
}

Daemon::~Daemon() {}

void Daemon::main(void)
{
    VprocYield(); /* make sure our parent is waiting for us */
    VprocSignal(&sync);

    RegisterDaemon(interval, &sync);

    while (1) {
        VprocWait(&sync);

        LOG(10, ("%s: running\n", name));

        START_TIMING();
        function();
        END_TIMING();

        LOG(10, ("%s: elapsed = %3.1f\n", name, elapsed));

        seq++;
    }
}

/* helper to run trivial daemon threads */
void FireAndForget(const char *name, PROCBODY function, int interval, int stack)
{
    Daemon *d = new Daemon(name, function, interval, stack);
    VprocWait(&d->sync);
}
