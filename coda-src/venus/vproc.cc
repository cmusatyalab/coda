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
 * Implementation of the Venus process abstraction
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
#include <sys/time.h>
#include "coda_string.h"

#include <unistd.h>
#include <stdlib.h>

#include <math.h>
#include <lwp/lwp.h>
#include <lwp/lock.h>

#ifdef __cplusplus
}
#endif

/* interfaces */
#include <vice.h>

/* from venus */
#include "local.h"
#include "user.h"
#include "venus.private.h"
#include "venusrecov.h"
#include "venusvol.h"
#include "vproc.h"
#include "worker.h"

/* Compatibility with rpc2 < 2.6 */
#ifndef EVOLUME
#define EVOLUME CEVOLUME
#endif

olist vproc::tbl;
int vproc::counter;
char vproc::rtry_sync;

extern const int RVM_THREAD_DATA_ROCK_TAG;

static const int VPROC_ROCK_TAG = 1776;

static void DoNothing(void)
{
    return;
}

void VprocInit()
{
    vproc::counter = 0;

    /*
     * Create main process.
     * This call initializes LWP and IOMGR support.
     * That's why it doesn't pass in a function.
     */
    Main = new vproc("Main", &DoNothing, VPT_Main);

    VprocSetRetry();
}

void Rtry_Wait()
{
    LOG(0, ("WAITING(RTRYQ):\n"));
    START_TIMING();
    VprocWait(&vproc::rtry_sync);
    END_TIMING();
    LOG(0, ("WAIT OVER, elapsed = %3.1f\n", elapsed));
}

void Rtry_Signal()
{
    VprocSignal(&vproc::rtry_sync);
}

vproc *FindVproc(int vpid)
{
    vproc_iterator next;
    vproc *vp;
    while ((vp = next()))
        if (vp->vpid == vpid)
            return (vp);

    return (0);
}

/* This code gets control when the new vproc starts running. */
/* It's job is to initialize state which needs to be set in the context of the
 * new vproc. Certain things can't be done in the vproc::ctor because that may
 * be executed in a different context! */
void VprocPreamble(void *arg)
{
    struct Lock *init_lock = (struct Lock *)arg;

    /* This lock is released by start_thread, as soon as the initialization has
     * finalized (aka. this->lwpid has been set */
    if (init_lock)
        ObtainWriteLock(init_lock); /* we never have to give it up. */

    /* VPROC rock permits mapping back to vproc object. */
    PROCESS x;
    int lwprc = LWP_CurrentProcess(&x);
    if (lwprc != LWP_SUCCESS)
        CHOKE("VprocPreamble: LWP_CurrentProcess failed (%d)", lwprc);
    vproc_iterator next;
    vproc *vp;
    while ((vp = next()))
        if (vp->lwpid == x)
            break;
    if (vp == NULL)
        CHOKE("VprocPreamble: lwp not found");
    lwprc = LWP_NewRock(VPROC_ROCK_TAG, (char *)vp);
    if (lwprc != LWP_SUCCESS)
        CHOKE("VprocPreamble: LWP_NewRock(VPROC) failed (%d)", lwprc);

    /* RVM_THREAD_DATA rock allows rvmlib to derive tids, etc. */
    lwprc = LWP_NewRock(RVM_THREAD_DATA_ROCK_TAG, (char *)&vp->rvm_data);
    if (lwprc != LWP_SUCCESS)
        CHOKE("VprocPreamble: LWP_NewRock(RVM_THREAD) failed (%d)", lwprc);

    /* Call the entry point for the new thread. main() is a virtual function of
     * the vproc class, and should be overloaded by any classes that inherit
     * from vproc. Others should pass a function pointer to indicate the entry
     * point when initializing a new vproc */
    vp->main();
    if (init_lock)
        delete vp;
}

vproc *VprocSelf()
{
    char *rock;
    if (LWP_GetRock(VPROC_ROCK_TAG, &rock) == LWP_SUCCESS)
        return ((vproc *)rock);

    /* Presumably this is the first call.  Set the rock so that future lookups will be fast. */
    {
    }

    /* Presumably, we are in the midst of handling a signal. */
    return (Main);
}

void VprocWait(const void *addr)
{
#ifdef VENUSDEBUG
    {
        /* Sanity-check: vproc must not context-switch in mid-transaction! */
        rvm_perthread_t *_rvm_data = rvmlib_thread_data();
        if (_rvm_data && _rvm_data->tid != 0)
            CHOKE("VprocWait: in transaction (tid = %x)", _rvm_data->tid);
    }
#endif

    int lwprc = LWP_WaitProcess(addr);
    if (lwprc != LWP_SUCCESS)
        CHOKE("VprocWait(%x): LWP_WaitProcess failed (%d)", addr, lwprc);
}

void VprocMwait(int wcount, const void **addrs)
{
#ifdef VENUSDEBUG
    {
        /* Sanity-check: vproc must not context-switch in mid-transaction! */
        rvm_perthread_t *_rvm_data = rvmlib_thread_data();
        if (_rvm_data && _rvm_data->tid != 0)
            CHOKE("VprocMwait: in transaction (tid = %x)", _rvm_data->tid);
    }
#endif

    int lwprc = LWP_MwaitProcess(wcount, addrs);
    if (lwprc != LWP_SUCCESS)
        CHOKE("VprocMwait(%d, %x): LWP_MwaitProcess failed (%d)", wcount, addrs,
              lwprc);
}

void VprocSignal(const void *addr, int yield)
{
#ifdef VENUSDEBUG
    if (yield) {
        /* Sanity-check: vproc must not context-switch in mid-transaction! */
        rvm_perthread_t *_rvm_data = rvmlib_thread_data();
        if (_rvm_data && _rvm_data->tid != 0)
            CHOKE("VprocSignal: in transaction (tid = %x)", _rvm_data->tid);
    }
#endif

    int lwprc = (yield ? LWP_SignalProcess(addr) : LWP_NoYieldSignal(addr));
    if (lwprc != LWP_SUCCESS && lwprc != LWP_ENOWAIT)
        CHOKE("VprocSignal(%x): %s failed (%d)", addr,
              (yield ? "LWP_SignalProcess" : "LWP_NoYieldSignal"), lwprc);
    /*
    if (lwprc == LWP_ENOWAIT)
	LOG(100, ("VprocSignal: ENOWAIT returned for addr %x\n",
		addr, (yield ? "LWP_SignalProcess" : "LWP_NoYieldSignal")));
*/
}

void VprocSleep(struct timeval *delay)
{
#ifdef VENUSDEBUG
    {
        /* Sanity-check: vproc must not context-switch in mid-transaction! */
        rvm_perthread_t *_rvm_data = rvmlib_thread_data();
        if (_rvm_data && _rvm_data->tid != 0)
            CHOKE("VprocSleep: in transaction (tid = %x)", _rvm_data->tid);
    }
#endif

    IOMGR_Select(0, NULL, NULL, NULL, delay);
}

void VprocYield()
{
#ifdef VENUSDEBUG
    {
        /* Sanity-check: vproc must not context-switch in mid-transaction! */
        rvm_perthread_t *_rvm_data = rvmlib_thread_data();
        if (_rvm_data && _rvm_data->tid != 0)
            CHOKE("VprocYield: in transaction (tid = %x)", _rvm_data->tid);
    }
#endif

    /* Do a polling select first to make vprocs with pending I/O runnable. */
    (void)IOMGR_Poll();

    LOG(1000, ("VprocYield: pre-yield\n"));
    int lwprc = LWP_DispatchProcess();
    if (lwprc != LWP_SUCCESS)
        CHOKE("VprocYield: LWP_DispatchProcess failed (%d)", lwprc);
    LOG(1000, ("VprocYield: post-yield\n"));
}

int VprocSelect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
                struct timeval *timeout)
{
#ifdef VENUSDEBUG
    {
        /* Sanity-check: vproc must not context-switch in mid-transaction! */
        rvm_perthread_t *_rvm_data = rvmlib_thread_data();
        if (_rvm_data && _rvm_data->tid != 0)
            CHOKE("VprocSelect: in transaction (tid = %x)", _rvm_data->tid);
    }
#endif

    return (IOMGR_Select(nfds, readfds, writefds, exceptfds, timeout));
}

const int DFLT_VprocRetryCount             = 5;
const struct timeval DFLT_VprocRetryBeta0  = { 15, 0 };
/*PRIVATE*/ int VprocRetryN                = -1; /* total number of retries */
/*PRIVATE*/ struct timeval *VprocRetryBeta = 0; /* array of timeout intervals */

/*
 *  This implementation has:
 *    (1)  Beta[i+1] = 2*Beta[i]
 *    (2)  Beta[0] = Beta[1] + Beta[2] ... + Beta[RetryN+1]
 *
 *  Time constants less than LOWERLIMIT are set to LOWERLIMIT.
 */
void VprocSetRetry(int HowManyRetries, struct timeval *Beta0)
{
    if (HowManyRetries >= 30)
        HowManyRetries =
            DFLT_VprocRetryCount; /* else overflow with 32-bit integers */
    if (HowManyRetries < 0)
        HowManyRetries = DFLT_VprocRetryCount;
    if (Beta0 == 0)
        Beta0 = (struct timeval *)&DFLT_VprocRetryBeta0;

    CODA_ASSERT(VprocRetryN == -1 && VprocRetryBeta == 0);
    VprocRetryN = HowManyRetries;
    VprocRetryBeta =
        (struct timeval *)malloc(sizeof(struct timeval) * (2 + HowManyRetries));
    memset((void *)VprocRetryBeta, 0,
           (int)sizeof(struct timeval) * (2 + HowManyRetries));
    VprocRetryBeta[0] = *Beta0;

    /* compute VprocRetryBeta[1] .. VprocRetryBeta[N] */
#define LOWERLIMIT 300000 /* .3 seconds */
    int betax, timeused, beta0; /* entirely in microseconds */
    betax =
        (int)(1000000 * VprocRetryBeta[0].tv_sec + VprocRetryBeta[0].tv_usec) /
        ((1 << (VprocRetryN + 1)) - 1);
    beta0 =
        (int)(1000000 * VprocRetryBeta[0].tv_sec + VprocRetryBeta[0].tv_usec);
    timeused = 0;
    for (int i = 1; i < VprocRetryN + 2 && beta0 > timeused; i++) {
        if (betax < LOWERLIMIT) {
            /* NOTE: we don't bother with (beta0 - timeused < LOWERLIMIT) */
            VprocRetryBeta[i].tv_sec  = 0;
            VprocRetryBeta[i].tv_usec = LOWERLIMIT;
            timeused += LOWERLIMIT;
        } else {
            if (beta0 - timeused > betax) {
                VprocRetryBeta[i].tv_sec  = betax / 1000000;
                VprocRetryBeta[i].tv_usec = betax % 1000000;
                timeused += betax;
            } else {
                VprocRetryBeta[i].tv_sec  = (beta0 - timeused) / 1000000;
                VprocRetryBeta[i].tv_usec = (beta0 - timeused) % 1000000;
                timeused                  = beta0;
            }
        }
        betax = betax << 1;
    }
}

int VprocIdle()
{
    return ((VprocSelf())->idle);
}

int VprocInterrupted()
{
    return ((VprocSelf())->interrupted);
}

void PrintVprocs()
{
    PrintVprocs(stdout);
}

void PrintVprocs(FILE *fp)
{
    fflush(fp);
    PrintVprocs(fileno(fp));
    fflush(fp);
}

void PrintVprocs(int fd)
{
    fdprint(fd, "Vprocs: tbl = %#08x, counter = %d, nprocs = %d\n",
            (long)&vproc::tbl, vproc::counter, vproc::tbl.count());

    vproc_iterator next;
    vproc *vp;
    while ((vp = next()))
        vp->print(fd);

    fdprint(fd, "\n");
}

vproc::vproc(const char *n, PROCBODY f, vproctype t, int stksize, int priority)
{
    /* Initialize the data members. LWPid is filled in as a side effect of
     * LWP operations in start_thread. */
    lwpid = NULL;
    name  = strdup(n);
    func  = f;
    vpid  = counter++;
    memset((void *)&rvm_data, 0, (int)sizeof(rvm_perthread_t));
    /*    rvm_data.die = &CHOKE; */
    type        = t;
    stacksize   = stksize;
    lwpri       = priority;
    seq         = 0;
    idle        = 0;
    interrupted = 0;
    u.Init();
    ve = NULL;
    /* Create a lock to get startup synchronization correct */
    Lock_Init(&init_lock);

    /* Insert the new vproc into the table. */
    tbl.insert(this);

    /* This start_thread call is way too early for derived classes, the
     * constructors haven't finished yet. Overloaded virtual functions in
     * derived classes will definitely not be available yet. So derived classes
     * should run start_thread around the end of their constructor */
    if (f)
        start_thread();
}

/* Fork off the new thread */
void vproc::start_thread(void)
{
    static int initialized = 0;

    if (initialized) {
        /* grab the lock */
        ObtainWriteLock(&init_lock);
        /* Create an LWP for this vproc. */
        /* pass the lock, instead of the function; we can get back to the
	 * function once we know which vproc the new lwp is */
        int lwprc = LWP_CreateProcess(VprocPreamble, stacksize, lwpri,
                                      &init_lock, name, &lwpid);
        if (lwprc != LWP_SUCCESS)
            CHOKE("vproc::start_thread: LWP_CreateProcess(%d, %s) failed (%d)",
                  stacksize, name, lwprc);

        /* Bogus handshaking so that new LWP continues after its yield! */
        ReleaseWriteLock(&init_lock);
        VprocYield();
    } else {
        /* Initialize the LWP subsystem. */
        int lwprc = LWP_Init(LWP_VERSION, LWP_NORMAL_PRIORITY, &lwpid);
        if (lwprc != LWP_SUCCESS)
            CHOKE("VprocInit: LWP_Init failed (%d)", lwprc);

        int iomgrrc = IOMGR_Initialize();
        if (iomgrrc != LWP_SUCCESS)
            CHOKE("VprocInit: IOMGR_Initialize failed (%d)", iomgrrc);

        initialized = 1;
        VprocPreamble(NULL);
    }
}

void vproc::main(void)
{
    CODA_ASSERT(func);
    func();
}

/*
 * we don't support assignments to objects of this type.
 * bomb in an obvious way if it inadvertently happens.
 */
vproc::vproc(vproc &)
{
    abort();
}

int vproc::operator=(vproc &vp)
{
    abort();
    return (0);
}

vproc::~vproc()
{
    if (LogLevel >= 1)
        print(logFile);

    if (!idle)
        CHOKE("vproc::~vproc: not idle!");

    /* Remove the entry from the table. */
    tbl.remove(this);

    if (LWP_DestroyProcess((PROCESS)lwpid) != LWP_SUCCESS)
        CHOKE("vproc::~vproc: LWP_DestroyProcess failed");

    free(name);
}

/* local-repair modification */
void vproc::GetStamp(char *buf)
{
    char t;
    switch (type) {
    case VPT_Main:
        t = 'X';
        break;
    case VPT_Worker:
        t = 'W';
        break;
    case VPT_Mariner:
        t = 'M';
        break;
    case VPT_CallBack:
        t = 'C';
        break;
    case VPT_HDBDaemon:
        t = 'H';
        break;
    case VPT_Reintegrator:
        t = 'I';
        break;
    case VPT_Resolver:
        t = 'R';
        break;
    case VPT_FSODaemon:
        t = 'F';
        break;
    case VPT_ProbeDaemon:
        t = 'D';
        break;
    case VPT_VSGDaemon:
        t = 'G';
        break;
    case VPT_VolDaemon:
        t = 'V';
        break;
    case VPT_UserDaemon:
        t = 'U';
        break;
    case VPT_RecovDaemon:
        t = 'T';
        break;
    case VPT_VmonDaemon:
        t = 'N';
        break;
    case VPT_AdviceDaemon:
        t = 'A';
        break;
    case VPT_LRDaemon:
        t = 'L';
        break;
    case VPT_Daemon:
        t = 'd';
        break;
    default:
        t = '?';
        eprint("???vproc::GetStamp: bogus type (%d)!", type);
    }
    time_t curr_time = Vtime();
    struct tm *lt    = localtime(&curr_time);
    sprintf(buf, "[ %c(%02d) : %04d : %02d:%02d:%02d ] ", t, vpid, seq,
            lt->tm_hour, lt->tm_min, lt->tm_sec);
}

static int VolModeMap[NVFSOPS] = {
    VM_MUTATING, /* UNUSED */
    VM_MUTATING, /* UNUSED */
    VM_OBSERVING, /* CODA_ROOT */
    /*VM_UNSET*/ -1, /* CODA_OPEN_BY_FD */
    /*VM_UNSET*/ -1, /* CODA_OPEN */
    /*VM_UNSET*/ -1, /* CODA_CLOSE */
    /*VM_UNSET*/ -1, /* CODA_IOCTL */
    VM_OBSERVING, /* CODA_GETATTR */
    VM_MUTATING, /* CODA_SETATTR */
    VM_OBSERVING, /* CODA_ACCESS */
    VM_OBSERVING, /* CODA_LOOKUP */
    VM_MUTATING, /* CODA_CREATE */
    VM_MUTATING, /* CODA_REMOVE */
    VM_MUTATING, /* CODA_LINK */
    VM_MUTATING, /* CODA_RENAME */
    VM_MUTATING, /* CODA_MKDIR */
    VM_MUTATING, /* CODA_RMDIR */
    VM_OBSERVING, /* UNUSED (used to be CODA_READDIR) */
    VM_MUTATING, /* CODA_SYMLINK */
    VM_OBSERVING, /* CODA_READLINK */
    VM_OBSERVING, /* CODA_FSYNC */
    VM_MUTATING, /* UNUSED */
    VM_OBSERVING, /* CODA_VGET */
    VM_OBSERVING, /* CODA_SIGNAL */
    VM_MUTATING, /* DOWNCALL: CODA_REPLACE */
    VM_MUTATING, /* DOWNCALL: CODA_FLUSH */
    VM_MUTATING, /* DOWNCALL: CODA_PURGEUSER */
    VM_MUTATING, /* DOWNCALL: CODA_ZAPFILE */
    VM_MUTATING, /* DOWNCALL: CODA_ZAPDIR */
    VM_MUTATING, /* DOWNCALL: UNUSED */
    VM_MUTATING, /* DOWNCALL: CODA_PURGEFID */
    /*VM_UNSET*/ -1, /* CODA_OPEN_BY_PATH */
    VM_RESOLVING, /* CODA_RESOLVE */
    VM_OBSERVING, /* CODA_REINTEGRATE */
    VM_OBSERVING, /* CODA_STATFS */
    /*VM_UNSET*/ -1, /* CODA_STORE */
    /*VM_UNSET*/ -1, /* CODA_RELEASE */
    VM_MUTATING, /* UNUSED */
    VM_MUTATING, /* UNUSED */
    VM_MUTATING, /* UNUSED */
};

#define VFSOP_TO_VOLMODE(vfsop) \
    (((vfsop) >= 0 && (vfsop) < NVFSOPS) ? VolModeMap[vfsop] : VM_MUTATING)

/* local-repair modification */
void vproc::Begin_VFS(Volid *volid, int vfsop, int volmode)
{
    LOG(1, ("vproc::Begin_VFS(%s): vid = %x.%x, u.u_vol = %x, mode = %d\n",
            VenusOpStr(vfsop), volid->Realm, volid->Volume, u.u_vol, volmode));

    /* Set up this thread's volume-related context. */
    if (u.u_vol == 0) {
        if (VDB->Get(&u.u_vol, volid)) {
            u.u_error = EVOLUME; /* ??? -JJK */
            return;
        }
    }
    u.u_volmode = (volmode ==
                           /*VM_UNSET*/ -1 ?
                       VFSOP_TO_VOLMODE(vfsop) :
                       volmode);
    u.u_vfsop   = vfsop;

    if (u.u_volmode == VM_MUTATING && u.u_vol->IsReplicated() &&
        !u.u_vol->IsUnreachable()) {
        struct timeval delay = { 1, 0 };
        int free_fsos, free_mles;
        uint64_t free_blocks;
        long cml_length;
        int inyellowzone, inredzone;

    wait_for_reintegration:
        free_fsos   = FSDB->FreeFsoCount();
        free_mles   = VDB->FreeMLECount();
        free_blocks = FSDB->GetMaxBlocks() - FSDB->DirtyBlockCount();
        cml_length  = ((repvol *)u.u_vol)->LengthOfCML();

        /* the redzone and yellow zone thresholds are pretty arbitrary at the
	 * moment. I am guessing that the number of worker threads might be a
	 * useful metric for redzoning on CML entries.
         * Explicit CML length checks added by Satya (2016-12-28).
         */
        inredzone = !free_fsos || free_mles <= MaxWorkers ||
                    free_blocks <=
                        (FSDB->GetMaxBlocks() >> 4) || /* ~94% cache dirty */
                    (redzone_limit > 0 && cml_length >= redzone_limit);
        inyellowzone = free_fsos <= MaxWorkers ||
                       free_mles <= (MLEs >> 3) || /* ~88% CMLs used */
                       free_blocks <=
                           (FSDB->GetMaxBlocks() >> 2) || /* ~75% cache dirty */
                       (yellowzone_limit > 0 && cml_length >= yellowzone_limit);

        if (inredzone)
            MarinerLog("progress::Red zone, stalling writer\n");
        else if (inyellowzone)
            MarinerLog("progress::Yellow zone, slowing down writer\n");

        if (inyellowzone || inredzone) {
            FSOD_ReclaimFSOs(); /* wake up fso daemon and let it reclaim fsos */
            VprocSleep(&delay);
        }

        if (inredzone)
            goto wait_for_reintegration;
    }

#ifdef TIMING
    gettimeofday(&u.u_tv1, 0);
    u.u_tv2.tv_sec = 0;
#endif

    /* Kick out non-ASR processes if an ASR is running */
    if ((u.u_vol->IsReadWrite()) && (vfsop != CODA_RESOLVE) &&
        (((reintvol *)u.u_vol)->asr_running() &&
         (u.u_pgid != ((reintvol *)u.u_vol)->asr_pgid())))
        u.u_error = EAGAIN;
    else /* Attempt to enter the volume. */
        u.u_error = u.u_vol->Enter(u.u_volmode, u.u_uid);

    if (u.u_error)
        VDB->Put(&u.u_vol);
}

/* local-repair modification */
/* Retryp MUST be non-null in order to do any form of waiting. */
void vproc::End_VFS(int *retryp)
{
    LOG(1,
        ("vproc::End_VFS(%s): code = %d\n", VenusOpStr(u.u_vfsop), u.u_error));

    if (retryp)
        *retryp = 0;

    if (u.u_vol == 0)
        goto Exit;

    /* sync reintegrate whenever we create or destroy an object. Ignore open
     * because those don't result in action until close, and setattr because
     * those are mostly cosmetic and don't affect access to the object */
    if (u.u_volmode == VM_MUTATING && u.u_vol->IsReplicated() &&
        u.u_vfsop != CODA_OPEN && u.u_vfsop != CODA_SETATTR && u.u_error == 0) {
        if (((repvol *)u.u_vol)->IsSync())
            u.u_vol->flags.transition_pending = 1;
    }

    /* Exit the volume. */
    u.u_vol->Exit(u.u_volmode, u.u_uid);

    /* Handle synchronous resolves. */
    if (u.u_error == ESYNRESOLVE) {
        u.u_rescnt++;
        if (u.u_rescnt > 5) { /* XXX */
            eprint("ResolveMax exceeded...returning EWOULDBLOCK");
            u.u_error = EWOULDBLOCK;
            goto Exit;
        }
        CODA_ASSERT(u.u_vol->IsReplicated());
        int code = ((repvol *)u.u_vol)->ResAwait(u.u_resblk);

        /* Reset counter if result was equivocal. */
        if (code == ERETRY || code == EWOULDBLOCK)
            u.u_rescnt = 0;

        /* Retry on success, failure, or timeout. */
        if (code == 0 || code == EINCONS || code == ETIMEDOUT)
            u.u_error = ERETRY;
        else
            u.u_error = code;
    }

    /* This is the set of errors we may retry. */
    if (u.u_error != ETIMEDOUT && u.u_error != EWOULDBLOCK &&
        u.u_error != ERETRY)
        goto Exit;

    /* Now safe to return if interrupted. */
    if (VprocInterrupted()) {
        u.u_error = EINTR;
        goto Exit;
    }

    /* Caller should be prepared to retry! */
    if (!retryp) {
        if (u.u_error == ERETRY)
            u.u_error = EWOULDBLOCK;
        goto Exit;
    }

    switch (u.u_error) {
    case ERETRY: {
        u.u_retrycnt++;
        if (u.u_retrycnt > VprocRetryN) {
            eprint("MaxRetries exceeded...returning EWOULDBLOCK");
            u.u_error = EWOULDBLOCK;
            goto Exit;
        }
        VprocSleep(&VprocRetryBeta[u.u_retrycnt]);
    } break;

    case EWOULDBLOCK: {
        u.u_wdblkcnt++;
        if (u.u_wdblkcnt > 20) { /* XXX */
            eprint("MaxWouldBlock exceeded...returning EWOULDBLOCK");
            goto Exit;
        }
        eprint("Volume %s busy, waiting", u.u_vol->GetName());
        struct timeval delay;
        delay.tv_sec  = 15; /* XXX */
        delay.tv_usec = 0;
        VprocSleep(&delay);
    } break;

    case ETIMEDOUT:
        /* Check whether user wants to wait on blocking events. */
        {
            userent *ue     = u.u_vol->realm->GetUser(u.u_uid);
            int waitforever = ue->GetWaitForever();
            PutUser(&ue);
            if (!waitforever)
                goto Exit;

            /* Clear other counts. */
            u.u_rescnt   = 0;
            u.u_retrycnt = 0;
            u.u_wdblkcnt = 0;

            /* Wait a few seconds before retrying. */
            /* Perhaps exponential back-off would be good here? */
            /* Could also/instead wait for relevant event to happen! */
            struct timeval delay;
            delay.tv_sec  = 15; /* XXX */
            delay.tv_usec = 0;
            VprocSleep(&delay);

            if (VprocInterrupted()) {
                u.u_error = EINTR;
                goto Exit;
            }
        }
        break;

    default:
        CODA_ASSERT(0);
    }

    /* Give this call another try. */
    u.u_error = 0;
    *retryp   = 1;

Exit:
#ifdef TIMING
    gettimeofday(&u.u_tv2, 0);
#endif

    /* Update VFS statistics. */
    if (u.u_vfsop >= 0 && u.u_vfsop < NVFSOPS) {
        VFSStat *t    = &VFSStats.VFSOps[u.u_vfsop];
        float elapsed = 0.0;

        if (u.u_error == 0) {
            if (retryp && *retryp != 0)
                t->retry++;
            else {
                t->success++;
#ifdef TIMING
                elapsed = SubTimes(&(u.u_tv2), &(u.u_tv1));
                t->time += (double)elapsed;
                t->time2 += (double)(elapsed * elapsed);
#endif
            }
        } else if (u.u_error == ETIMEDOUT)
            t->timeout++;
        else
            t->failure++;
    }

    VDB->Put(&u.u_vol);
}

void vproc::print()
{
    print(stdout);
}

void vproc::print(FILE *fp)
{
    fflush(fp);
    print(fileno(fp));
}

void vproc::print(int fd)
{
    int max = 0, used = 0;

    /* LWP_StackUsed broken for initial LWP? -JJK */
    if (type != VPT_Main && lwpid != 0)
        (void)LWP_StackUsed((PROCESS)lwpid, &max, &used);
    fdprint(
        fd,
        "%#08x : %-16s : id = (%x : %d), stack = (%d : %d), seq = %d, flags = (%x%x)\n",
        (long)this, name, lwpid, vpid, max, used, seq, idle, interrupted);
}

vproc_iterator::vproc_iterator(vproctype t)
    : olist_iterator(vproc::tbl)
{
    type = t;
}

vproc *vproc_iterator::operator()()
{
    if (type == (vproctype)-1)
        return ((vproc *)olist_iterator::operator()());

    vproc *vp;
    while ((vp = (vproc *)olist_iterator::operator()()))
        if (vp->type == type)
            return (vp);
    return (0);
}

void va_init(struct coda_vattr *vap)
{
    vap->va_mode          = VA_IGNORE_MODE;
    vap->va_uid           = VA_IGNORE_UID;
    vap->va_gid           = VA_IGNORE_GID;
    vap->va_fileid        = VA_IGNORE_ID;
    vap->va_mtime.tv_sec  = VA_IGNORE_TIME1;
    vap->va_mtime.tv_nsec = 0;
    vap->va_bytes         = VA_IGNORE_STORAGE;
    vap->va_nlink         = VA_IGNORE_NLINK;
    vap->va_size          = VA_IGNORE_SIZE;
    vap->va_blocksize     = VA_IGNORE_BLOCKSIZE;
    vap->va_atime = vap->va_ctime = vap->va_mtime;
    vap->va_rdev                  = (long long unsigned int)VA_IGNORE_RDEV;
    vap->va_flags                 = 0; /* must be 0, not IGNORE_FLAGS for BSD */
}

void VPROC_printvattr(struct coda_vattr *vap)
{
    if (LogLevel >= 1000) {
        dprint("\tmode = %#o, uid = %d, gid = %d, rdev = %d\n", vap->va_mode,
               vap->va_uid, vap->va_gid, vap->va_rdev);
        dprint(
            "\tid = %d, nlink = %d, size = %d, blocksize = %d, storage = %d\n",
            vap->va_fileid, vap->va_nlink, vap->va_size, vap->va_blocksize,
            vap->va_bytes);
        dprint("\tatime = <%d, %d>, mtime = <%d, %d>, ctime = <%d, %d>\n",
               vap->va_atime.tv_sec, vap->va_atime.tv_nsec,
               vap->va_mtime.tv_sec, vap->va_mtime.tv_nsec,
               vap->va_ctime.tv_sec, vap->va_ctime.tv_nsec);
    }
}

long FidToNodeid(VenusFid *fid)
{
    if (FID_EQ(fid, &NullFid))
        CHOKE("FidToNodeid: null fid");

#if defined(__FreeBSD__) || defined(__NetBSD__)
    /* Venus Root.  Use the mount point's nodeid. */
    if (FID_EQ(fid, &rootfid))
        return (rootnodeid);
#endif

    /* Other volume root.  We need the relevant mount point's fid,
	   but we don't know what that is! */
    if (FID_IsVolRoot(fid)) {
        LOG(0, ("FidToNodeid: called for volume root (%x.%x)!!!\n", fid->Realm,
                fid->Volume));
    }

    /* Non volume root. */
    return coda_f2i(VenusToKernelFid(fid));
}
