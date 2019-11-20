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

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <sys/time.h>
#include <setjmp.h>

#ifdef __cplusplus
}
#endif

/* from venus */
#include "comm.h"
#include "venus.private.h"
#include "venusrecov.h"
#include "vproc.h"

/*
 *    Implementation of the Venus "probe" daemon, which periodically and
 *    on-demand sends probe messages to servers and checks the health of the
 *    network.
 */

/*  *****  Probe Daemon  *****  */

static const int ProbeDaemonStackSize = 40960;
static int T1Interval                 = 0; /* "up" server probe interval
				    initialized in venus.cc as 12 * 60 */
static int T2Interval                 = 4 * 60; /* "down" servers */
static const int CommCheckInterval    = 5;
static const int ProbeInterval = CommCheckInterval; /* min(T1, T2, Comm) */

static char probe_sync;

void PROD_Init(void)
{
    T1Interval = GetVenusConf().get_int_value("serverprobe");
    /* force the down server probe to be more frequent as well when the
     * 'serverprobe' option in venus.conf is low enough. */
    if (T1Interval < T2Interval)
        T2Interval = T1Interval;

    (void)new vproc("ProbeDaemon", &ProbeDaemon, VPT_ProbeDaemon,
                    ProbeDaemonStackSize);
}

void ProbeDaemon(void)
{
    /* Hack!  Vproc must yield before data members become valid! */
    VprocYield();

    vproc *vp = VprocSelf();
    RegisterDaemon(ProbeInterval, &probe_sync);

    long curr_time     = 0;
    long LastT1Check   = 0;
    long LastT2Check   = 0;
    long LastCommCheck = 0;

    for (;;) {
        VprocWait(&probe_sync);

        START_TIMING();
        curr_time = Vtime();

        /* probe the servers.  ServerProbe updates the last probed times. */
        if ((curr_time - LastT1Check >= T1Interval) ||
            (curr_time - LastT2Check >= T2Interval)) {
            ServerProbe(&LastT1Check, &LastT2Check);
        }

        /* refresh network measurements for each server */
        if (curr_time - LastCommCheck >= CommCheckInterval) {
            CheckServerBW(curr_time);

            LastCommCheck = curr_time;
        }

        END_TIMING();
        LOG(10, ("ProbeDaemon: elapsed = %3.1f\n", elapsed));

        /* Bump sequence number. */
        vp->seq++;
    }
}

/*
 * Determine which servers should be probed, and probe them.
 * lasttupp and lastdownp, if set, contain the last probe times of up
 * and down servers, respectively (down servers are probed more often
 * than up servers).  The probe of a given server may be suppressed if
 * the last probe time is recent enough.
 *
 * This routine updates the last probe times.
 */
void ServerProbe(long *lastupp, long *lastdownp)
{
    LOG(1, ("ServerProbe: lastup = %d, lastdown = %d\n", lastupp ? *lastupp : 0,
            lastdownp ? *lastdownp : 0));

    CODA_ASSERT((lastupp && lastdownp) || (!lastupp && !lastdownp));

    int upprobe = 0, downprobe = 0;

    /* Mark the servers to be probed */
    {
        time_t curr_time   = Vtime();
        time_t minlastup   = curr_time; /* assume everyone gets probed now. */
        time_t minlastdown = curr_time;

        srv_iterator next;
        srvent *s;
        while ((s = next())) {
            /*
	     * We will probe the server if the check is being forced (no
	     * times sent in), or if the server has not been heard from
	     * within the appropriate interval. Otherwise, pretend a probe
	     * occurred at the "last live" time.
	     */
            struct timeval lastword;

            (void)s->GetLiveness(&lastword); /* if error, lastword = 0 */
            if ((lastupp == 0 && lastdownp == 0) || /* forced check */
                (s->ServerIsUp() &&
                 (lastword.tv_sec + T1Interval <=
                  curr_time) && /* heard recently */
                 (*lastupp + T1Interval <=
                  curr_time)) || /* up interval expired */
                (s->ServerIsDown() &&
                 (lastword.tv_sec + T2Interval <=
                  curr_time) && /* heard recently */
                 (*lastdownp + T2Interval <=
                  curr_time))) { /* down interval expired */

                s->probeme = 1;
                if (s->ServerIsUp())
                    upprobe = 1;
                else
                    downprobe = 1;
            } else {
                LOG(1, ("Suppressing probe to %s, %s %d sec ago\n", s->name,
                        (lastword.tv_sec || lastword.tv_usec) ? "heard from" :
                                                                "probed",
                        (lastword.tv_sec || lastword.tv_usec) ?
                            (curr_time - lastword.tv_sec) :
                            (s->ServerIsUp() ? (curr_time - *lastupp) :
                                               (curr_time - *lastdownp))));
                /* Keep track of the least recent probe time. */
                if (s->ServerIsUp()) {
                    if (lastword.tv_sec < minlastup)
                        minlastup = lastword.tv_sec;
                } else {
                    if (lastword.tv_sec < minlastdown)
                        minlastdown = lastword.tv_sec;
                }
            }
        }

        /* Update the last probe times, if supplied */
        if (lastdownp && (minlastdown > *lastdownp))
            *lastdownp = minlastdown;
        if (lastupp && (minlastup > *lastupp))
            *lastupp = minlastup;
    }

    /* Probe t1 (i.e., "up") servers. */
    char upsync = 0;
    if (upprobe)
        (void)new probeslave(ProbeUpServers, 0, 0, &upsync);

    /* Probe t2 (i.e., "down") servers. */
    char downsync = 0;
    if (downprobe)
        (void)new probeslave(ProbeDownServers, 0, 0, &downsync);

    /* Reap t1 slave. */
    if (upprobe)
        while (upsync == 0)
            VprocWait(&upsync);

    /* Reap t2 slave. */
    if (downprobe)
        while (downsync == 0)
            VprocWait(&downsync);
}
