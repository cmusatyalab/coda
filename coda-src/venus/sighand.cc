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
 *    Implementation of the Venus Signal Handler facility.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>

#ifdef __cplusplus
}
#endif


#include "sighand.h"
#include "venus.private.h"
#include "venusrecov.h"
#include "worker.h"
#include "adv_monitor.h"
#include "adv_daemon.h"
#include "codaconf.h"

static void SigControl(int);
static void SigChoke(int);
static void SigExit(int);

int TerminateVenus;

void SigInit(void)
{
    /* Establish/Join our own process group to avoid extraneous signals. */
#ifndef DJGPP
  if (setpgid(0, 0) < 0)
    CHOKE("SigInit: setpgid failed (%d)", errno);
#endif /* !DJGPP */

    /* set up the signal handlers */
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; /* SA_RESTART? */

    /* ignore... */
    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, NULL);
#ifdef SIGIO
    sigaction(SIGIO,  &sa, NULL);
#endif

    /* shutdown... */
    sa.sa_handler = SigExit;
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
#ifdef SIGPWR
    sigaction(SIGPWR,  &sa, NULL);
#endif

    /* venus control... */
    sa.sa_handler = SigControl;
    sigaction(SIGHUP,  &sa, NULL);

    /* coerce coredumps and unexpected signals into zombie state... */
    sa.sa_handler = SigChoke;
    sigaction(SIGQUIT, &sa, NULL);
    sigaction(SIGILL,  &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
    sigaction(SIGFPE,  &sa, NULL);
    sigaction(SIGSEGV, &sa, NULL);

    /* various other signals that cause random coredumps and sudden exits. */
    /* as these are not POSIX, they may be missing on some platforms. */
    sigaction(SIGTRAP, &sa, NULL);
#ifdef SIGBUS
    sigaction(SIGBUS,  &sa, NULL);
#endif
#ifdef SIGEMT
    sigaction(SIGEMT,  &sa, NULL);
#endif
#ifdef SIGSYS
    sigaction(SIGSYS,  &sa, NULL);
#endif
#ifdef SIGSTKFLT
    sigaction(SIGSTKFLT,  &sa, NULL);
#endif

    /* There are also some aliases on linux, maybe they are different on other
     * platforms, and if they are not ignored we'd have to create more complex
     * ifdef's to handle them.
     * SIGIOT  == SIGABRT
     * SIGPOLL == SIGIO
     */

    /* These were used by vutil, ignoring them should avoid problems when we
     * accidently to use an old vutil script. */
    sa.sa_handler = SIG_IGN;
    sigaction(SIGUSR1, &sa, NULL);
#ifdef SIGXCPU
    sigaction(SIGXCPU, &sa, NULL);
#endif
#ifdef SIGXFSZ
    sigaction(SIGXFSZ, &sa, NULL);
#endif
#ifdef SIGVTALARM
    sigaction(SIGVTALRM, &sa, NULL);
#endif

    /* Write our pid to a file so scripts can find us easily. */
    FILE *fp = fopen(VenusPidFile,"w");
    if (fp == NULL)
	CHOKE("SigInit: can't open file for pid!");
    fprintf(fp, "%d", getpid());
    fclose(fp);
}

static void SigControl(int sig)
{
    struct stat tstat;
    FILE *fp;
    char command[80];

    if (stat(VenusControlFile, &tstat) != 0) {
	SwapLog();
	adv_mon.SwapProgramLog();
	adv_mon.SwapReplacementLog();
        return;
    }

    fp = fopen(VenusControlFile, "r+");
    if (fp == NULL) {
        LOG(0, ("SigControl: open(%s) failed", VenusControlFile));
        return;
    }

    (void)fscanf(fp, "%79s", &command);

    if (strcmp(command, "COPMODES") == 0) {
#if 0
	int NewModes = 0;
	(void)fscanf(fp, "%d", &NewModes);

	/* This is a hack! -JJK */
	int OldModes = COPModes;
	COPModes = NewModes;
	if ((ASYNCCOP1 || PIGGYCOP2) && !ASYNCCOP2) {
	    eprint("Bogus modes (%x)\n", COPModes);
	    COPModes = OldModes;
	}
#endif
	LOG(100, ("COPModes = %x\n", COPModes));
    }

    if (strcmp(command, "MCAST") == 0) {
#if 0
	(void)fscanf(fp, "%d", &UseMulticast);
#endif
	LOG(100, ("UseMulticast is now %d.\n", UseMulticast));
    }

    if (strcmp(command, "DEBUG") == 0) {
	int found, loglevel, rpc2level, lwplevel;

	found = fscanf(fp, "%d %d %d", &loglevel, &rpc2level, &lwplevel);

	if (found > 0 && loglevel >= 0)
		LogLevel = loglevel;

	if (found > 1 && rpc2level >= 0) {
		RPC2_DebugLevel = rpc2level;
		RPC2_Trace = (rpc2level > 0) ? 1 : 0;
	}

	if (found > 2 && lwplevel >= 0)
		lwp_debug = lwplevel;

	LOG(0, ("LogLevel is now %d.\n", LogLevel));
	LOG(0, ("RPC2_DebugLevel is now %d.\n", RPC2_DebugLevel));
	LOG(0, ("lwp_debug is now %d.\n", lwp_debug));
    }

    if (strcmp(command, "SWAPLOGS") == 0) {
	SwapLog();
	adv_mon.SwapProgramLog();
	adv_mon.SwapReplacementLog();
    }

    if (strcmp(command, "STATSINIT") == 0)
	StatsInit();

    if (strcmp(command, "STATS") == 0)
	DumpState();

Exit:
    if (fclose(fp) == EOF)
	LOG(0, ("SigControl: fclose(%s) failed", VenusControlFile));
    if (unlink(VenusControlFile) < 0)
	LOG(0, ("SigControl: unlink(%s) failed", VenusControlFile));
}

static void SigChoke(int sig)
{
    LOG(0, ("*****  FATAL SIGNAL (%d) *****\n", sig));

#ifndef DJGPP
    eprint("Fatal Signal (%d); pid %d becoming a zombie...", sig, getpid());
    eprint("You may use gdb to attach to %d", getpid());

    {
	int       living_dead = 1;
	sigset_t  mask;
	sigemptyset(&mask);
	while (living_dead) {
	    sigsuspend(&mask);
	}
    }
#endif

    SigExit(sig);
}

static void SigExit(int sig)
{
    LOG(0, ("TERM: About to terminate venus\n"));
    TerminateVenus = 1;

    RecovFlush(1);
    RecovTerminate();
    VFSUnmount();
    fflush(logFile);
    fflush(stderr);
    exit(0);
}

