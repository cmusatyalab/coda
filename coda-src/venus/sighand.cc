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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/venus/sighand.cc,v 4.12 98/09/23 20:26:32 jaharkes Exp $";
#endif /*_BLURB_*/








/*
 *
 *    Implementation of the Venus Signal Handler facility.
 *
 */


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>

#ifdef __cplusplus
}
#endif __cplusplus


#include "sighand.h"
#include "venus.private.h"
#include "venusrecov.h"
#include "worker.h"
#include "advice_daemon.h"
#if defined(__linux__) && defined(sparc)
#include <asm/sigcontext.h>
#define sigcontext sigcontext_struct
#endif

static void HUP(int, int, struct sigcontext *);
static void ILL(int, int, struct sigcontext *);
static void TRAP(int, int, struct sigcontext *);
static void IOT(int, int, struct sigcontext *);
static void EMT(int, int, struct sigcontext *);
static void FPE(int, int, struct sigcontext *);
static void BUS(int, int, struct sigcontext *);
static void SEGV(int, int, struct sigcontext *);
static void USR1(int, int, struct sigcontext *);
static void TERM(int, int, struct sigcontext *);
static void XCPU(int, int, struct sigcontext *);
static void XFSZ(int, int, struct sigcontext *);
static void VTALRM(int, int, struct sigcontext *);
static void FatalSignal(int, int, struct sigcontext *);


void SigInit() {
    /* Establish/Join our own process group to avoid extraneous signals. */
#ifndef DJGPP
#ifdef	__linux__
        if (setpgrp() < 0)
#else
                if (setpgrp(0, getpid()) < 0)
#endif
	Choke("SigInit: setpgrp failed (%d)", errno);
#endif /* DJGPP */
    /* Install the signal handlers. */
#ifdef __BSD44__
    signal(SIGHUP, (void (*)(int))HUP);		/* turn on debugging */
    signal(SIGILL, (void (*)(int))ILL);		/* Choke */
    signal(SIGTRAP, (void (*)(int))TRAP);	/* Choke */
    signal(SIGIOT, (void (*)(int))IOT);		/* turn on profiling */
    signal(SIGEMT, (void (*)(int))EMT);		/* turn off profiling */
    /* SIGFPE is ignored for the short term */
    /* signal(SIGFPE, (void (*)(int))FPE);*/		/* Choke */
    signal(SIGFPE, SIG_IGN);                    /* Ignore */
    signal(SIGBUS, (void (*)(int))BUS);		/* Choke */
    signal(SIGSEGV, (void (*)(int))SEGV);	/* Choke */
    signal(SIGPIPE, SIG_IGN);	                /* ignore write on pipe with no one to read */
    signal(SIGTERM, (void (*)(int))TERM);	/* exit */
    signal(SIGINT, (void (*)(int))TERM);	/* exit */
    /*signal(SIGTSTP, (void (*)(int))TSTP);*/	/* turn off debugging */
    signal(SIGXCPU, (void (*)(int))XCPU);	/* dump state */
    signal(SIGXFSZ, (void (*)(int))XFSZ);	/* initialize statistics */
    signal(SIGVTALRM, (void (*)(int))VTALRM);	/* swap log */
    signal(SIGUSR1, (void (*)(int))USR1);	/* set {COPmode, Mcast, DebugLevel} */
#endif

#if  defined(__linux__) 
    signal(SIGHUP, (void (*)(int))HUP);		/* turn on debugging */
    signal(SIGILL, (void (*)(int))ILL);		/* Choke */
    signal(SIGTRAP, (void (*)(int))TRAP);	/* Choke */
    signal(SIGIOT, (void (*)(int))IOT);		/* turn on profiling */
    /* SIGFPE was ignored for the `short term' */
    /*signal(SIGFPE, SIG_IGN);*/                    /* Ignore */
    signal(SIGFPE, (void (*)(int))FPE);		/* Choke */
    signal(SIGBUS, (void (*)(int))BUS);		/* Choke */
    signal(SIGSEGV, (void (*)(int))SEGV);	/* Choke */
    signal(SIGPIPE, SIG_IGN);	                /* ignore write on pipe with no one to read */
    signal(SIGTERM, (void (*)(int))TERM);	/* exit */
    signal(SIGINT, (void (*)(int))TERM);	/* exit */
    /*signal(SIGTSTP, (void (*)(int))TSTP);*/	/* turn off debugging */
    signal(SIGXCPU, (void (*)(int))XCPU);	/* dump state */
    signal(SIGXFSZ, (void (*)(int))XFSZ);	/* initialize statistics */
    signal(SIGVTALRM, (void (*)(int))VTALRM);	/* swap log */
    signal(SIGUSR1, (void (*)(int))USR1);	/* set {COPmode, Mcast, DebugLevel} */
#endif

#ifdef __CYWIN32__
    signal(SIGHUP, (void (*)(int))HUP);		/* turn on debugging */
    signal(SIGILL, (void (*)(int))ILL);		/* Choke */
    signal(SIGTRAP, (void (*)(int))TRAP);	/* Choke */
    signal(SIGEMT, (void (*)(int))EMT);		/* turn off profiling */
    /* SIGFPE is ignored for the short term */
    /* signal(SIGFPE, (void (*)(int))FPE);*/		/* Choke */
    signal(SIGFPE, SIG_IGN);                    /* Ignore */
    signal(SIGBUS, (void (*)(int))BUS);		/* Choke */
    signal(SIGSEGV, (void (*)(int))SEGV);	/* Choke */
    signal(SIGPIPE, SIG_IGN);	                /* ignore write on pipe with no one to read */
    signal(SIGTERM, (void (*)(int))TERM);	/* exit */
    signal(SIGINT, (void (*)(int))TERM);	/* exit */
    /*signal(SIGTSTP, (void (*)(int))TSTP);*/	/* turn off debugging */
    signal(SIGUSR1, (void (*)(int))USR1);	/* set {COPmode, Mcast, DebugLevel} */
#endif

    /* Write our pid to a file so scripts can find us easily. */
    FILE *fp = fopen("pid","w");
    if (fp == NULL)
	Choke("SigInit: can't open file for pid!");
    fprintf(fp, "%d", getpid());
    fclose(fp);
}


static void HUP(int sig, int code, struct sigcontext *contextPtr) {
    DebugOn();

    signal(SIGHUP, (void (*)(int))HUP);
}


static void ILL(int sig, int code, struct sigcontext *contextPtr) {
    FatalSignal(sig, code, contextPtr);
}


static void TRAP(int sig, int code, struct sigcontext *contextPtr) {
    FatalSignal(sig, code, contextPtr);
}


#ifdef SIGIOT
static void IOT(int sig, int code, struct sigcontext *contextPtr) {

  LOG(0, ("Call into IOT\n"));
  fflush(logFile);

  /* linux gets this signal when it shouldn't */
#ifdef __BSD44__
    if (!Profiling)
	ToggleProfiling();
    signal(SIGIOT, (void (*)(int))IOT);
#endif
}
#endif

#ifdef SIGEMT
static void EMT(int sig, int code, struct sigcontext *contextPtr) {
    if (Profiling)
	ToggleProfiling();

    signal(SIGEMT, (void (*)(int))EMT);
}
#endif

static void FPE(int sig, int code, struct sigcontext *contextPtr) {
    FatalSignal(sig, code, contextPtr);
}


static void BUS(int sig, int code, struct sigcontext *contextPtr) {
    FatalSignal(sig, code, contextPtr);
}


static void SEGV(int sig, int code, struct sigcontext *contextPtr) {
    FatalSignal(sig, code, contextPtr);
}

static void USR1(int sig, int code, struct sigcontext *contextPtr) {
    struct stat tstat;
    if (stat("COPMODES", &tstat) == 0) {
#if 0
	int NewModes = 0;
	FILE *fp = fopen("COPMODES", "r+");
	if (fp == NULL) Choke("USR1: fopen(COPMODES)");
	(void)fscanf(fp, "%d", &NewModes);
	if (fclose(fp) == EOF) Choke("USR1: fclose(COPMODES)");
#endif
	if (unlink("COPMODES") < 0) Choke("USR1: unlink(COPMODES)");

#if 0
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
    if (stat("MCAST", &tstat) == 0) {
#if 0
	FILE *fp = fopen("MCAST", "r+");
	if (fp == NULL) Choke("USR1: fopen(MCAST)");
	(void)fscanf(fp, "%d", &UseMulticast);
	if (fclose(fp) == EOF) Choke("USR1: fclose(MCAST)");
#endif
	if (unlink("MCAST") < 0) Choke("USR1: unlink(MCAST)");
	LOG(100, ("UseMulticast is now %d.\n", UseMulticast));
    }

    if (stat("DEBUG", &tstat) == 0) {
	FILE *fp = fopen("DEBUG", "r+");
	int found, loglevel, rpc2level, lwplevel;

	if (fp == NULL) Choke("USR1: fopen(DEBUG)");

	found = fscanf(fp, "%d %d %d", &loglevel, &rpc2level, &lwplevel);

	if (found > 0 && loglevel >= 0)
	{
		LogLevel = loglevel;
	}

	if (found > 1 && rpc2level >= 0)
	{
		RPC2_DebugLevel = rpc2level;
		RPC2_Trace = (rpc2level > 0) ? 1 : 0;
	}

	if (found > 2 && lwplevel >= 0)
	{
		lwp_debug = lwplevel;
	}

	if (fclose(fp) == EOF) Choke("USR1: fclose(DEBUG)");
	if (unlink("DEBUG") < 0) Choke("USR1: unlink(DEBUG)");

	LOG(0, ("LogLevel is now %d.\n", LogLevel));
	LOG(0, ("RPC2_DebugLevel is now %d.\n", RPC2_DebugLevel));
	LOG(0, ("lwp_debug is now %d.\n", lwp_debug));
    }

    if (stat("DUMP", &tstat) == 0) {
	/* No longer used! -JJK */
	if (unlink("DUMP") < 0) Choke("USR1: unlink(DUMP)");
    }

    signal(SIGUSR1, (void (*)(int))USR1);
}


static void TERM(int sig, int code, struct sigcontext *contextPtr) {
    LOG(0, ("TERM: Venus exiting\n"));

    VDB->FlushVolume();
    RecovFlush(1);
    RecovTerminate();
#ifdef	__NetBSD__
    WorkerCloseMuxfd();
#else
    VFSUnmount();
#endif
    (void)CheckAllocs("TERM");
    fflush(logFile);
    fflush(stderr);
    exit(0);
}


#ifdef NOTUSED
/* This handler is now deactivated, as we would sometimes really like to put a
 * venus in the background */
static void TSTP(int sig, int code, struct sigcontext *contextPtr) {
    DebugOff();

    signal(SIGTSTP, (void (*)(int))TSTP);
}
#endif

#ifdef SIGXCPU
static void XCPU(int sig, int code, struct sigcontext *contextPtr) {
    DumpState();

    signal(SIGXCPU, (void (*)(int))XCPU);
}


#ifdef SIGXFSZ
static void XFSZ(int sig, int code, struct sigcontext *contextPtr) {
    StatsInit();

    signal(SIGXFSZ, (void (*)(int))XFSZ);
}
#endif

#ifdef SIGVTALRM
static void VTALRM(int sig, int code, struct sigcontext *contextPtr) {
    SwapLog();
    SwapProgramLogs();
    SwapReplacementLogs();

    signal(SIGVTALRM, (void (*)(int))VTALRM);
}
#endif

#endif



#ifdef DJGPP
static void FatalSignal(int sig, int code, struct sigcontext *contextPtr) 
{
    LOG(0, ("*****  FATAL SIGNAL (%d) *****\n", sig));
    TERM(sig, code, 0);
}
#else
static void FatalSignal(int sig, int code, struct sigcontext *contextPtr) 
{
    LOG(0, ("*****  FATAL SIGNAL (%d) *****\n", sig));

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

    /* Dump the process context. */
    {
	fprintf(logFile, "sig=%d\n", sig);
	fprintf(logFile, "code=%d\n", code);
#ifndef	i386
#if defined(sparc) && defined(__linux__)
	fprintf(logFile, "sc_pc=0x%x\n", contextPtr->sigc_pc);
#else
	fprintf(logFile, "sc_pc=0x%x\n", contextPtr->sc_pc);
#endif
#endif	/* !defined(i386) */

#ifndef __BSD44__
	for (int i = 0; i < sizeof(struct sigaction) / sizeof(int); i++)
                fprintf(logFile, "context[%d] = 0x%x\n", i, *((u_int *)contextPtr + i));
#else
	for (int i = 0; i < sizeof(struct sigcontext) / sizeof(int); i++)
                fprintf(logFile, "context[%d] = 0x%x\n", i, *((u_int *)contextPtr + i));
#endif

	fflush(logFile);
    }

    TERM(sig, code, contextPtr);
}
#endif
