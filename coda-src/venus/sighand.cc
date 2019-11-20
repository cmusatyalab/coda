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
#include <sys/wait.h>
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
#include "venusvol.h"
#include "worker.h"
#include "fso.h"
#include "codaconf.h"
#include "daemonizer.h"

static void SigControl(int);
static void SigChoke(int);
static void SigExit(int);
static void SigMounted(int);
static void SigASR(int);

static const char *VenusControlFile;

int TerminateVenus;
int mount_done;

extern long int RPC2_Trace;
extern pid_t ASRpid;
extern VenusFid ASRfid;

void SigInit(void)
{
    VenusControlFile = GetVenusConf().get_value("run_control_file");
    /* Establish/Join our own process group to avoid extraneous signals. */
    if (setpgid(0, 0) < 0)
        eprint("SigInit: setpgid failed (%d)", errno);

    /* set up the signal handlers */
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; /* SA_RESTART? */

    /* ignore... */
    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, NULL);
#ifdef SIGIO
    sigaction(SIGIO, &sa, NULL);
#endif
#ifdef SIGTSTP
    sigaction(SIGTSTP, &sa, NULL);
#endif
#ifdef SIGTTOU
    sigaction(SIGTTOU, &sa, NULL);
#endif
#ifdef SIGTTIN
    sigaction(SIGTTIN, &sa, NULL);
#endif
#ifdef SIGXCPU
    sigaction(SIGXCPU, &sa, NULL);
#endif
#ifdef SIGXFSZ
    sigaction(SIGXFSZ, &sa, NULL);
#endif
#ifdef SIGVTALARM
    sigaction(SIGVTALRM, &sa, NULL);
#endif

    /* shutdown... */
    sa.sa_handler = SigExit;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
#ifdef SIGPWR
    sigaction(SIGPWR, &sa, NULL);
#endif

    /* venus control... */
    sa.sa_handler = SigControl;
    sigaction(SIGHUP, &sa, NULL);

    /* coerce coredumps and unexpected signals into zombie state... */
    sa.sa_handler = SigChoke;
    sigaction(SIGQUIT, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
    sigaction(SIGSEGV, &sa, NULL);

    /* various other signals that cause random coredumps and sudden exits. */
    /* as these are not POSIX, they may be missing on some platforms. */
    sigaction(SIGTRAP, &sa, NULL);
#ifdef SIGBUS
    sigaction(SIGBUS, &sa, NULL);
#endif
#ifdef SIGEMT
    sigaction(SIGEMT, &sa, NULL);
#endif
#ifdef SIGSYS
    sigaction(SIGSYS, &sa, NULL);
#endif
#ifdef SIGSTKFLT
    sigaction(SIGSTKFLT, &sa, NULL);
#endif

    sa.sa_handler = SigMounted;
    sigaction(SIGUSR1, &sa, NULL);

    /* There are also some aliases on linux, maybe they are different on other
     * platforms, and if they are not ignored we'd have to create more complex
     * ifdef's to handle them.
     * SIGIOT  == SIGABRT
     * SIGPOLL == SIGIO
     */

    sa.sa_handler = SigASR;
#ifdef SIGCHLD
    sigaction(SIGCHLD, &sa, NULL);
    ASRpid = NO_ASR;
#endif
}

static void SigControl(int sig)
{
    struct stat tstat;
    FILE *fp;
    char command[80];

    if (stat(VenusControlFile, &tstat) != 0) {
        SwapLog();
        return;
    }

    fp = fopen(VenusControlFile, "r+");
    if (fp == NULL) {
        LOG(0, ("SigControl: open(%s) failed", VenusControlFile));
        return;
    }

    (void)fscanf(fp, "%79s", command);

    if (STREQ(command, "COPMODES")) {
        LOG(100, ("COPModes = %x\n", GetCOPModes()));
    }

    if (STREQ(command, "DEBUG")) {
        int found, loglevel, rpc2level, lwplevel;

        found = fscanf(fp, "%d %d %d", &loglevel, &rpc2level, &lwplevel);

        if (found > 0 && loglevel >= 0)
            SetLogLevel(loglevel);

        if (found > 1 && rpc2level >= 0) {
            RPC2_DebugLevel = rpc2level;
            RPC2_Trace      = (rpc2level > 0) ? 1 : 0;
        }

        if (found > 2 && lwplevel >= 0)
            lwp_debug = lwplevel;

        LOG(0, ("LogLevel is now %d.\n", GetLogLevel()));
        LOG(0, ("RPC2_DebugLevel is now %d.\n", RPC2_DebugLevel));
        LOG(0, ("lwp_debug is now %d.\n", lwp_debug));
    }

    if (STREQ(command, "SWAPLOGS"))
        SwapLog();

    if (STREQ(command, "STATSINIT"))
        StatsInit();

    if (STREQ(command, "STATS"))
        DumpState();

    if (fclose(fp) == EOF)
        LOG(0, ("SigControl: fclose(%s) failed", VenusControlFile));
    if (unlink(VenusControlFile) < 0)
        LOG(0, ("SigControl: unlink(%s) failed", VenusControlFile));
}

static void SigChoke(int sig)
{
    sigset_t mask;
    int pid = getpid();

    LOG(0, ("*****  FATAL SIGNAL (%d) *****\n", sig));

    eprint("Fatal Signal (%d); pid %d becoming a zombie...", sig, pid);
    eprint("You may use gdb to attach to %d", pid);
    MarinerLog("zombie state::pid %d", pid);

    /* just in case we still have a parent process waiting for us we don't want
     * to lock up the boot sequence... */
    WorkerCloseMuxfd();

    /* block all signals, except for INT and TERM (and the non-blockable ones,
     * KILL and STOP) */
    sigfillset(&mask);
    sigdelset(&mask, SIGINT);
    sigdelset(&mask, SIGTERM);
    sigsuspend(&mask);

    SigExit(sig);
}

static void SigExit(int sig)
{
    LOG(0, ("TERM: About to terminate venus\n"));
    MarinerLog("shutdown in progress\n");

    TerminateVenus = 1;

    RecovFlush(1);
    RecovTerminate();
    VFSUnmount();
    fflush(GetLogFile());
    fflush(stderr);
    exit(EXIT_SUCCESS);
}

static void SigMounted(int sig)
{
    gogogo(parent_fd);
}

static void SigASR(int sig)
{
    int child_pid, status, options;
    repvol *v;

    /* Beyond Venus initialization, the only forking occurring within Venus
   * is a result of ASRLauncher invocation. Thus, every SIGCHLD received is
   * an ASRLauncher completing execution, and the status is the return code
   * of success or failure of the repair. */

    if (ASRpid == NO_ASR)
        return;

    LOG(0,
        ("Signal Handler(ASR): ASRpid:%d, ASRfid:%s\n", ASRpid, FID_(&ASRfid)));

    status = options = 0;

    child_pid = waitpid(ASRpid, &status, WNOHANG);
    if (child_pid < 0) {
        perror("waitpid");
        exit(EXIT_FAILURE);
    } else if (child_pid == ASRpid)
        LOG(0, ("Signal Handler(ASR): Caught ASRLauncher (%d) with status %d\n",
                child_pid, status));
    else {
        LOG(0, ("Signal Handler(ASR): Caught an unknown child!\n"));
        return; /* If there are no documented ASR's running, this
			   * could be the VFSMount double-fork middle child. */
    }

    v = (repvol *)VDB->Find(MakeVolid(&ASRfid));
    if (v == NULL) {
        LOG(0, ("Signal Handler(ASR): Couldn't find volume!\n"));
        return;
    }

    /* Clear out table entry */
    ASRpid = NO_ASR;

    /* Unassign Tokens */
    /* TODO: not easy to do at the moment. */

    /* Unlock volume */
    v->asr_pgid(NO_ASR);
    v->unlock_asr();
}
