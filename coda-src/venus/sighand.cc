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

static char *rcsid = "$Header: /usr/rvb/XX/src/coda-src/venus/RCS/sighand.cc,v 4.1 1997/01/08 21:51:33 rvb Exp $";
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
#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#include <mach.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif

#ifdef __cplusplus
}
#endif __cplusplus


#include "sighand.h"
#include "venus.private.h"
#include "venusrecov.h"
#include "worker.h"


PRIVATE void HUP(int, int, struct sigcontext *);
PRIVATE void ILL(int, int, struct sigcontext *);
PRIVATE void TRAP(int, int, struct sigcontext *);
PRIVATE void IOT(int, int, struct sigcontext *);
PRIVATE void EMT(int, int, struct sigcontext *);
PRIVATE void FPE(int, int, struct sigcontext *);
PRIVATE void BUS(int, int, struct sigcontext *);
PRIVATE void SEGV(int, int, struct sigcontext *);
PRIVATE void SYS(int, int, struct sigcontext *);
PRIVATE void TERM(int, int, struct sigcontext *);
PRIVATE void TSTP(int, int, struct sigcontext *);
PRIVATE void XCPU(int, int, struct sigcontext *);
PRIVATE void XFSZ(int, int, struct sigcontext *);
PRIVATE void VTALRM(int, int, struct sigcontext *);
PRIVATE void USR1(int, int, struct sigcontext *);
PRIVATE void FatalSignal(int, int, struct sigcontext *);


void SigInit() {
    /* Establish/Join our own process group to avoid extraneous signals. */
#ifdef	__linux__
        if (setpgrp() < 0)
#else
                if (setpgrp(0, getpid()) < 0)
#endif
	Choke("SigInit: setpgrp failed (%d)", errno);

    /* Install the signal handlers. */
    signal(SIGHUP, (void (*)(int))HUP);		/* turn on debugging */
    signal(SIGILL, (void (*)(int))ILL);		/* Choke */
    signal(SIGTRAP, (void (*)(int))TRAP);	/* Choke */
    signal(SIGIOT, (void (*)(int))IOT);		/* turn on profiling */
#ifndef	__linux__
    signal(SIGEMT, (void (*)(int))EMT);		/* turn off profiling */
#endif
    /* SIGFPE is ignored for the short term */
    /* signal(SIGFPE, (void (*)(int))FPE);*/		/* Choke */
    signal(SIGFPE, SIG_IGN);                    /* Ignore */
    signal(SIGBUS, (void (*)(int))BUS);		/* Choke */
    signal(SIGSEGV, (void (*)(int))SEGV);	/* Choke */
#ifndef	__linux__
    signal(SIGSYS, (void (*)(int))SYS);		/* set {COPmode, Mcast, DebugLevel} */
#endif
    signal(SIGPIPE, SIG_IGN);	                /* ignore write on pipe with no one to read */
    signal(SIGTERM, (void (*)(int))TERM);	/* exit */
    signal(SIGTSTP, (void (*)(int))TSTP);	/* turn off debugging */
    signal(SIGXCPU, (void (*)(int))XCPU);	/* dump state */
    signal(SIGXFSZ, (void (*)(int))XFSZ);	/* initialize statistics */
    signal(SIGVTALRM, (void (*)(int))VTALRM);	/* swap log */
    signal(SIGUSR1, (void (*)(int))USR1);	/* Toggle malloc trace */

    if (!Simulating) {
	/* Write our pid to a file so scripts can find us easily. */
	FILE *fp = fopen("pid","w");
	if (fp == NULL)
	    Choke("SigInit: can't open file for pid!");
	fprintf(fp, "%d", getpid());
	fclose(fp);
    }
}


PRIVATE void HUP(int sig, int code, struct sigcontext *contextPtr) {
    DebugOn();

    signal(SIGHUP, (void (*)(int))HUP);
}


PRIVATE void ILL(int sig, int code, struct sigcontext *contextPtr) {
    FatalSignal(sig, code, contextPtr);
}


PRIVATE void TRAP(int sig, int code, struct sigcontext *contextPtr) {
    FatalSignal(sig, code, contextPtr);
}


PRIVATE void IOT(int sig, int code, struct sigcontext *contextPtr) {
    if (!Profiling)
	ToggleProfiling();

    signal(SIGIOT, (void (*)(int))IOT);
}

#ifndef	__linux__
PRIVATE void EMT(int sig, int code, struct sigcontext *contextPtr) {
    if (Profiling)
	ToggleProfiling();

    signal(SIGEMT, (void (*)(int))EMT);
}
#endif

PRIVATE void FPE(int sig, int code, struct sigcontext *contextPtr) {
    FatalSignal(sig, code, contextPtr);
}


PRIVATE void BUS(int sig, int code, struct sigcontext *contextPtr) {
    FatalSignal(sig, code, contextPtr);
}


PRIVATE void SEGV(int sig, int code, struct sigcontext *contextPtr) {
    FatalSignal(sig, code, contextPtr);
}

#ifndef	__linux__
PRIVATE void SYS(int sig, int code, struct sigcontext *contextPtr) {
    int RealSigSys = 1;
    struct stat tstat;
    if (stat("COPMODES", &tstat) == 0) {
	RealSigSys = 0;

	int NewModes = 0;
	FILE *fp = fopen("COPMODES", "r+");
	if (fp == NULL) Choke("SYS: fopen(COPMODES)");
	(void)fscanf(fp, "%d", &NewModes);
	if (fclose(fp) == EOF) Choke("SYS: fclose(COPMODES)");
	if (unlink("COPMODES") < 0) Choke("SYS: unlink(COPMODES)");

	/* This is a hack! -JJK */
	int OldModes = COPModes;
	COPModes = NewModes;
	if ((ASYNCCOP1 || PIGGYCOP2) && !ASYNCCOP2) {
	    eprint("Bogus modes (%x)\n", COPModes);
	    COPModes = OldModes;
	}
	LOG(100, ("COPModes = %x\n", COPModes));
    }
    if (stat("MCAST", &tstat) == 0) {
	RealSigSys = 0;

	FILE *fp = fopen("MCAST", "r+");
	if (fp == NULL) Choke("SYS: fopen(MCAST)");
	(void)fscanf(fp, "%d", &UseMulticast);
	if (fclose(fp) == EOF) Choke("SYS: fclose(MCAST)");
	if (unlink("MCAST") < 0) Choke("SYS: unlink(MCAST)");

	LOG(100, ("UseMulticast is now %d.\n", UseMulticast));
    }
    if (stat("DEBUG", &tstat) == 0) {
	RealSigSys = 0;

	FILE *fp = fopen("DEBUG", "r+");
	if (fp == NULL) Choke("SYS: fopen(DEBUG)");
	(void)fscanf(fp, "%d", &LogLevel);
	if (fclose(fp) == EOF) Choke("SYS: fclose(DEBUG)");
	if (unlink("DEBUG") < 0) Choke("SYS: unlink(DEBUG)");

	LOG(0, ("LogLevel is now %d.\n", LogLevel));
    }
    if (stat("DUMP", &tstat) == 0) {
	RealSigSys = 0;

	/* No longer used! -JJK */

	if (unlink("DUMP") < 0) Choke("SYS: unlink(DUMP)");
    }

    if (RealSigSys)
	FatalSignal(sig, code, contextPtr);

    signal(SIGSYS, (void (*)(int))SYS);
}

#endif

PRIVATE void TERM(int sig, int code, struct sigcontext *contextPtr) {
    LOG(0, ("TERM: Venus exiting\n"));

    VDB->FlushVolume();
    RecovFlush(1);
    RecovTerminate();
    VFSUnmount();
    (void)CheckAllocs("TERM");
    fflush(logFile);
    fflush(stderr);
    exit(0);
}


PRIVATE void TSTP(int sig, int code, struct sigcontext *contextPtr) {
    DebugOff();

    signal(SIGTSTP, (void (*)(int))TSTP);
}


PRIVATE void XCPU(int sig, int code, struct sigcontext *contextPtr) {
    DumpState();

    signal(SIGXCPU, (void (*)(int))XCPU);
}


PRIVATE void XFSZ(int sig, int code, struct sigcontext *contextPtr) {
    StatsInit();

    signal(SIGXFSZ, (void (*)(int))XFSZ);
}


PRIVATE void VTALRM(int sig, int code, struct sigcontext *contextPtr) {
    SwapLog();

    signal(SIGVTALRM, (void (*)(int))VTALRM);
}


PRIVATE void USR1(int sig, int code, struct sigcontext *contextPtr) {
    ToggleMallocTrace();

    signal(SIGUSR1, (void (*)(int))USR1);
}


PRIVATE void FatalSignal(int sig, int code, struct sigcontext *contextPtr) {
    LOG(0, ("*****  FATAL SIGNAL (%d) *****\n", sig));

#ifdef	__MACH__
    eprint("Fatal Signal (%d); pid %d becoming a zombie...", sig, getpid());
    task_suspend(task_self());
#endif	/* __MACH__ */

    /* Dump the process context. */
    {
	fprintf(logFile, "sig=%d\n", sig);
	fprintf(logFile, "code=%d\n", code);
#ifdef	i386
#else	i386
	fprintf(logFile, "sc_pc=0x%x\n", contextPtr->sc_pc);
#endif	i386
#ifdef	__linux__
	for (int i = 0; i < sizeof(struct sigaction) / sizeof(int); i++)
                fprintf(logFile, "context[%d] = 0x%x\n", i, *((u_int *)contextPtr + i));
#else
	for (int i = 0; i < sizeof(struct sigcontext) / sizeof(int); i++)
                fprintf(logFile, "context[%d] = 0x%x\n", i, *((u_int *)contextPtr + i));
#endif

#ifdef	__MACH__
/*
	 task_t task = task_self();
	 vm_address_t address = 0;
	 vm_size_t size;
	 vm_prot_t protection;
	 vm_prot_t max_protection;
	 vm_inherit_t inheritance;
	 boolean_t shared;
	 port_t object_name;
	 vm_offset_t offset;
	 while (vm_region(task, &address, &size, &protection, &max_protection,
			  &inheritance, &shared, &object_name, &offset) == KERN_SUCCESS) {
	     fprintf(logFile, "Start 0x%x, size 0x%x, p %d, mp %d ih %d shared %d on 0x%x off 0x%x\n",
		     address, size, protection, max_protection,
		     inheritance, shared, object_name, offset);

	     if (size == 0) break;
	     address += size;
	 }
*/
#endif	/* __MACH__ */

	fflush(logFile);
    }

    TERM(sig, code, contextPtr);
}
