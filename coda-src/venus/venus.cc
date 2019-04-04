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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <rpc2/codatunnel.h>

#include "archive.h"

#ifdef __cplusplus
}
#endif

/* interfaces */
#include <vice.h>
#include <stringkeyvaluestore.h>

/* from venus */
#include "comm.h"
#include "fso.h"
#include "hdb.h"
#include "local.h"
#include "mariner.h"
#include "sighand.h"
#include "user.h"
#include "venus.private.h"
#include "venuscb.h"
#include "venusrecov.h"
#include "venusvol.h"
#include "vproc.h"
#include "worker.h"
#include "coda_assert.h"
#include "codaconf.h"
#include "realmdb.h"
#include "daemonizer.h"

#include "nt_util.h"
#ifdef __CYGWIN32__
//  Not right now ... should go #define main venus_main
uid_t V_UID;
#endif

/* FreeBSD 2.2.5 defines this in rpc/types.h, all others in netinet/in.h */
#ifndef INADDR_LOOPBACK
#define INADDR_LOOPBACK 0x7f000001
#endif

/* *****  Exported variables  ***** */
/* globals in the .bss are implicitly initialized to 0 according to ANSI-C standards */
vproc *Main;
VenusFid rootfid;
long rootnodeid;
int CleanShutDown;
int SearchForNOreFind; // Look for better detection method for iterrupted hoard walks. mre 1/18/93
int ASRallowed = 1;

/* Command-line/venus.conf parameters. */
const char *consoleFile;
const char *venusRoot;
const char *kernDevice;
const char *realmtab;
const char *CacheDir;
const char *CachePrefix;
uint64_t CacheBlocks;
uid_t PrimaryUser = UNSET_PRIMARYUSER;
const char *SpoolDir;
const char *CheckpointFormat;
const char *VenusPidFile;
const char *VenusControlFile;
const char *VenusLogFile;
const char *ASRLauncherFile;
const char *ASRPolicyFile;
const char *MarinerSocketPath;
int masquerade_port;
int PiggyValidations;
pid_t ASRpid;
VenusFid ASRfid;
uid_t ASRuid;
int detect_reintegration_retry;
int option_isr;
/* exit codes (http://refspecs.linuxbase.org/LSB_3.1.1/LSB-Core-generic/LSB-Core-generic/iniscrptact.html) */
// EXIT_SUCCESS             0   /* stdlib.h - success */
// EXIT_FAILURE             1   /* stdlib.h - generic or unspecified error */
#define EXIT_INVALID_ARG 2 /* invalid or excess argument(s) */
#define EXIT_UNIMPLEMENTED 3 /* unimplemented feature */
#define EXIT_PRIVILEGE_ERR 4 /* user had insufficient privilege */
#define EXIT_UNINSTALLED 5 /* program is not installed */
#define EXIT_UNCONFIGURED 6 /* program is not configured */
#define EXIT_NOT_RUNNING 7 /* program is not running */

#if defined(HAVE_SYS_UN_H) && !defined(__CYGWIN32__)
int mariner_tcp_enable = 0;
#else
int mariner_tcp_enable = 1;
#endif
static int codafs_enabled;
int plan9server_enabled;
int nofork;

/* Global red and yellow zone limits on CML length; default is infinite */
int redzone_limit = -1, yellowzone_limit = -1;

static int codatunnel_enabled;
static int codatunnel_onlytcp;

StringKeyValueStore venus_global_config;

/* *****  Private constants  ***** */

struct timeval DaemonExpiry = { TIMERINTERVAL, 0 };

/* *****  Private routines  ***** */

static void LoadDefaultValuesIntoConfig();
static void ParseCmdline(int, char **);
static void DefaultCmdlineParms();
static void CdToCacheDir();
static void CheckInitFile();
static void UnsetInitFile();
static void SetRlimits();
/* **** defined in worker.c **** */
extern int testKernDevice();

struct in_addr venus_relay_addr = { INADDR_LOOPBACK };

/* *****  venus.c  ***** */

/* socket connecting us back to our parent */
int parent_fd = -1;

/* Bytes units convertion */
static const char *KBYTES_UNIT[] = { "KB", "kb", "Kb", "kB", "K", "k" };
static const unsigned int KBYTE_UNIT_SCALE = 1;
static const char *MBYTES_UNIT[] = { "MB", "mb", "Mb", "mB", "M", "m" };
static const unsigned int MBYTE_UNIT_SCALE = 1024 * KBYTE_UNIT_SCALE;
static const char *GBYTES_UNIT[] = { "GB", "gb", "Gb", "gB", "G", "g" };
static const unsigned int GBYTE_UNIT_SCALE = 1024 * MBYTE_UNIT_SCALE;
static const char *TBYTES_UNIT[] = { "TB", "tb", "Tb", "tB", "T", "t" };
static const unsigned int TBYTE_UNIT_SCALE = 1024 * GBYTE_UNIT_SCALE;

/* Some helpers to add fd/callbacks to the inner select loop */
struct mux_cb_entry {
    struct mux_cb_entry *next;
    int fd;
    void (*cb)(int fd, void *udata);
    void *udata;
};
static struct mux_cb_entry *_MUX_CBEs;

/* Add file descriptors that have a callback to the fd_set */
static int _MUX_FD_SET(fd_set *fds)
{
    struct mux_cb_entry *cbe = _MUX_CBEs;
    int maxfd                = -1;

    for (; cbe; cbe = cbe->next) {
        FD_SET(cbe->fd, fds);
        if (cbe->fd > maxfd)
            maxfd = cbe->fd;
    }
    return maxfd;
}

/* Dispatch callbacks for file descriptors in the fd_set */
static void _MUX_Dispatch(fd_set *fds)
{
    struct mux_cb_entry *cbe = _MUX_CBEs, *next;
    while (cbe) {
        /* allow callback to remove itself without messing with the iterator */
        next = cbe->next;

        if (FD_ISSET(cbe->fd, fds))
            cbe->cb(cbe->fd, cbe->udata);

        cbe = next;
    }
}

/* Helper to add a file descriptor with callback to main select loop.
 *
 * Call with cb == NULL to remove existing callback.
 * cb is called with fd == -1 when an existing callback is removed or updated.
 */
void MUX_add_callback(int fd, void (*cb)(int fd, void *udata), void *udata)
{
    struct mux_cb_entry *cbe = _MUX_CBEs, *prev = NULL;

    for (; cbe; cbe = cbe->next) {
        if (cbe->fd == fd) {
            /* remove old callback entry */
            if (prev)
                prev->next = cbe->next;
            else
                _MUX_CBEs = cbe->next;

            /* allow cb to free udata resources */
            cbe->cb(-1, cbe->udata);

            free(cbe);
            break;
        }
        prev = cbe;
    }
    /* if we are not adding a new callback, we're done */
    if (cb == NULL)
        return;

    cbe = (struct mux_cb_entry *)malloc(sizeof(*cbe));
    assert(cbe != NULL);

    cbe->fd    = fd;
    cbe->cb    = cb;
    cbe->udata = udata;

    cbe->next = _MUX_CBEs;
    _MUX_CBEs = cbe;
}

/*
 * Parse size value and converts into amount of 1K-Blocks
 */
static uint64_t ParseSizeWithUnits(const char *SizeWUnits)
{
    const char *units = NULL;
    int scale_factor  = 1;
    char SizeWOUnits[256];
    size_t size_len   = 0;
    uint64_t size_int = 0;

    /* Locate the units and determine the scale factor */
    for (int i = 0; i < 6; i++) {
        if ((units = strstr(SizeWUnits, KBYTES_UNIT[i]))) {
            scale_factor = KBYTE_UNIT_SCALE;
            break;
        }

        if ((units = strstr(SizeWUnits, MBYTES_UNIT[i]))) {
            scale_factor = MBYTE_UNIT_SCALE;
            break;
        }

        if ((units = strstr(SizeWUnits, GBYTES_UNIT[i]))) {
            scale_factor = GBYTE_UNIT_SCALE;
            break;
        }

        if ((units = strstr(SizeWUnits, TBYTES_UNIT[i]))) {
            scale_factor = TBYTE_UNIT_SCALE;
            break;
        }
    }

    /* Strip the units from string */
    if (units) {
        size_len = (size_t)((units - SizeWUnits) / sizeof(char));
        strncpy(SizeWOUnits, SizeWUnits, size_len);
        SizeWOUnits[size_len] = 0; // Make it null-terminated
    } else {
        snprintf(SizeWOUnits, sizeof(SizeWOUnits), "%s", SizeWUnits);
    }

    /* Scale the value */
    size_int = scale_factor * atof(SizeWOUnits);

    return size_int;
}

static int power_of_2(uint64_t num)
{
    int power = 0;

    if (!num)
        return -1;

    /*Find the first 1 */
    while (!(num & 0x1)) {
        num = num >> 1;
        power++;
    }

    /* Shift the first 1 */
    num = num >> 1;

    /* Any other 1 means not power of 2 */
    if (num)
        return -1;

    return power;
}

void ParseCacheChunkBlockSize(const char *ccblocksize)
{
    uint64_t TmpCacheChunkBlockSize = ParseSizeWithUnits(ccblocksize) * 1024;
    int TmpCacheChunkBlockSizeBit   = power_of_2(TmpCacheChunkBlockSize);

    if (TmpCacheChunkBlockSizeBit < 0) {
        /* Not a power of 2 FAIL!!! */
        eprint(
            "Cannot start: provided cache chunk block size is not a power of 2");
        exit(EXIT_UNCONFIGURED);
    }

    if (TmpCacheChunkBlockSizeBit < 12) {
        /* Smaller than minimum FAIL*/
        eprint("Cannot start: minimum cache chunk block size is 4KB");
        exit(EXIT_UNCONFIGURED);
    }

    CacheChunkBlockSizeBits   = TmpCacheChunkBlockSizeBit;
    CacheChunkBlockSize       = 1 << CacheChunkBlockSizeBits;
    CacheChunkBlockSizeMax    = CacheChunkBlockSize - 1;
    CacheChunkBlockBitmapSize = (UINT_MAX >> CacheChunkBlockSizeBits) + 1;
}

/* local-repair modification */
int main(int argc, char **argv)
{
    coda_assert_action  = CODA_ASSERT_SLEEP;
    coda_assert_cleanup = VFSUnmount;

    LoadDefaultValuesIntoConfig();

    ParseCmdline(argc, argv);
    DefaultCmdlineParms(); /* read /etc/coda/venus.conf */

    // Cygwin runs as a service and doesn't need to daemonize.
#ifndef __CYGWIN__
    if (!nofork && LogLevel == 0)
        parent_fd = daemonize();
#endif

    update_pidfile(VenusPidFile);

    /* open the console file and print vital info */
    if (!nofork) /* only redirect stderr when daemonizing */
        freopen(consoleFile, "a+", stderr);
    eprint("Coda Venus, version " PACKAGE_VERSION);

    CdToCacheDir();
    CheckInitFile();
    SetRlimits();
    /* Initialize.  N.B. order of execution is very important here! */
    /* RecovInit < VSGInit < VolInit < FSOInit < HDB_Init */

#ifdef __CYGWIN32__
    /* MapPrivate does not work on Cygwin */
    if (MapPrivate) {
        eprint("Private mapping turned off, does not work on CYGWIN.");
        MapPrivate = 0;
    }
    V_UID = getuid();
#endif

    /* test mismatch with kernel before doing real work */
    testKernDevice();

    if (codatunnel_enabled) {
        int rc;
        /* masquerade_port is the UDP portnum specified via venus.conf */
        char service[6];
        sprintf(service, "%hu", masquerade_port);
        rc = codatunnel_fork(argc, argv, NULL, "0.0.0.0", service,
                             codatunnel_onlytcp);
        if (rc < 0) {
            perror("codatunnel_fork: ");
            exit(-1);
        }
        printf("codatunneld started\n");
    }

    /*
     * VprocInit MUST precede LogInit. Log messages are stamped
     * with the id of the vproc that writes them, so log messages
     * can't be properly stamped until the vproc class is initialized.
     * The logging routines return without doing anything if LogInit
     * hasn't yet been called.
     */
    VprocInit(); /* init LWP/IOMGR support */
    LogInit(); /* move old Venus log and create a new one */

    LWP_SetLog(logFile, lwp_debug);
    RPC2_SetLog(logFile, RPC2_DebugLevel);
    DaemonInit(); /* before any Daemons initialize and after LogInit */
    StatsInit();
    SigInit(); /* set up signal handlers */

    DIR_Init(RvmType == VM ? DIR_DATA_IN_VM : DIR_DATA_IN_RVM);
    RecovInit(); /* set up RVM and recov daemon */
    CommInit(); /* set up RPC2, {connection,server,mgroup} lists, probe daemon */
    UserInit(); /* fire up user daemon */
    VSGDBInit(); /* init VSGDB */
    RealmDBInit();
    VolInit(); /* init VDB, daemon */
    FSOInit(); /* allocate FSDB if necessary, recover FSOs, start FSO daemon */
    VolInitPost(); /* drop extra volume refcounts */
    HDB_Init(); /* allocate HDB if necessary, scan entries, start the HDB daemon */
    MarinerInit(); /* set up mariner socket */
    WorkerInit(); /* open kernel device */
    CallBackInit(); /* set up callback subsystem and create callback server threads */

    /* Get the Root Volume. */
    if (codafs_enabled) {
        eprint("Mounting root volume...");
        VFSMount();
    }

    UnsetInitFile();
    eprint("Venus starting...");

    freopen("/dev/null", "w", stdout);

    /* allow the daemonization to complete */
    if (!codafs_enabled)
        kill(getpid(), SIGUSR1);

    /* Act as message-multiplexor/daemon-dispatcher. */
    for (;;) {
        /* Wait for a message or daemon expiry. */
        fd_set rfds;
        int maxfd;

        FD_ZERO(&rfds);
        maxfd = _MUX_FD_SET(&rfds);

        if (VprocSelect(maxfd + 1, &rfds, 0, 0, &DaemonExpiry) > 0)
            _MUX_Dispatch(&rfds);

        /* set in sighand.cc whenever we want to perform a clean shutdown */
        if (TerminateVenus)
            break;

        /* Fire daemons that are ready to run. */
        DispatchDaemons();
    }

    LOG(0, ("Venus exiting\n"));

    RecovFlush(1);
    RecovTerminate();
    VFSUnmount();
    fflush(logFile);
    fflush(stderr);

    MarinerLog("shutdown in progress\n");

    LWP_TerminateProcessSupport();

#if defined(__CYGWIN32__)
    nt_stop_ipc();
    return 0;
#endif

    exit(EXIT_SUCCESS);
}

static void Usage(char *argv0)
{
    printf(
        "Usage: %s [OPTION]...\n\n"
        "Options:\n"
        " -init\t\t\t\twipe and reinitialize persistent state\n"
        " -newinstance\t\t\tfake a 'reinit'\n"
        " -primaryuser <n>\t\tprimary user\n"
        " -mapprivate\t\t\tuse private mmap for RVM\n"
        " -d <debug level>\t\tdebug level\n"
        " -rpcdebug <n>\t\t\trpc2 debug level\n"
        " -lwpdebug <n>\t\t\tlwp debug level\n"
        " -cf <n>\t\t\t# of cache files\n"
        " -pcfr <n>\t\t\t%% of files that could be partially cached out \n"
        "\t\t\t\tof the total cache files\n"
        " -c <n>[KB|MB|GB|TB]\t\tcache size in the given units (e.g. 10MB)\n"
        " -ccbs <n>[KB|MB|GB|TB]\t\tcache chunk block size (shall be power of 2)\n"
        " -mles <n>\t\t\t# of CML entries\n"
        " -hdbes <n>\t\t\t# of hoard database entries\n"
        " -rdstrace\t\t\tenable RDS heap tracing\n"
        " -k <kernel device>\t\tTake kernel device to be the device for\n"
        "\t\t\t\tkernel/venus communication (/dev/cfs0)\n"
        " -f <cache dir>\t\t\tlocation of cache files\n"
        " -console <error log>\t\tlocation of error log file\n"
        " -m <n>\t\t\t\tCOP modes\n"
        " -maxworkers <n>\t\t# of worker threads\n"
        " -maxcbservers <n>\t\t# of callback server threads\n"
        " -maxprefetchers <n>\t\t# of threads servicing prefetch ioctl\n"
        " -retries <n>\t\t\t# of rpc2 retries\n"
        " -timeout <n>\t\t\trpc2 timeout\n"
        " -ws <n>\t\t\tsftp window size\n"
        " -sa <n>\t\t\tsftp send ahead\n"
        " -ap <n>\t\t\tsftp ack point\n"
        " -ps <n>\t\t\tsftp packet size\n"
        " -rvmt <n>\t\t\tRVM type\n"
        " -vld <RVM log>\t\t\tlocation of RVM log\n"
        " -vlds <n>\t\t\tsize of RVM log\n"
        " -vdd <RVM data>\t\tlocation of RVM data\n"
        " -vdds <n>\t\t\tsize of RVM data\n"
        " -rdscs <n>\t\t\tRDS chunk size\n"
        " -rdsnl <n>\t\t\tRDS # lists\n"
        " -logopts <n>\t\t\tRVM log optimizations\n"
        " -swt <n>\t\t\tshort term weight\n"
        " -mwt <n>\t\t\tmedium term weight\n"
        " -ssf <n>\t\t\tshort term scale factor\n"
        " -alpha <n>\t\t\tpatience ALPHA value\n"
        " -beta <n>\t\t\tpatience BETA value\n"
        " -gamma <n>\t\t\tpatience GAMMA value\n"
        " -von\t\t\t\tenable rpc2 timing\n"
        " -voff\t\t\t\tdisable rpc2 timing\n"
        " -vmon\t\t\t\tenable multirpc2 timing\n"
        " -vmoff\t\t\t\tdisable multirpc2 timing\n"
        " -SearchForNOreFind\t\tsomething, forgot what\n"
        " -noskk\t\t\t\tdisable venus sidekick\n"
        " -noasr\t\t\t\tdisable application specific resolvers\n"
        " -novcb\t\t\t\tdisable volume callbacks\n"
        " -nowalk\t\t\tdisable hoard walks\n"
        " -spooldir <spool directory>\tspooldir to hold CML snapshots\n"
        " -MarinerTcp\t\t\tenable mariner tcp port\n"
        " -noMarinerTcp\t\t\tdisable mariner tcp port\n"
        " -allow-reattach\t\tallow reattach to already mounted tree\n"
        " -relay <addr>\t\t\trelay socket address (windows only)\n"
        " -codatunnel\t\t\tenable codatunneld helper\n"
        " -no-codatunnel\t\t\tdisable codatunneld helper\n"
        " -onlytcp\t\t\tonly use TCP tunnel connections to servers\n"
        " -9pfs\t\t\t\tenable embedded 9pfs server (experimental, INSECURE!)\n"
        " -no-codafs\t\t\tdo not automatically mount /coda\n"
        " -nofork\t\t\tdo not daemonize the process\n"
        " -wfmax\t\t\t\tsize of files above which it's partially cached\n"
        " -wfmin\t\t\t\tsize of files below which it's NEVER partially cached\n"
        " -wfstall\t\t\tmaximum time to wait for a whole file caching. If \n"
        "\t\t\t\texceeded it's partially cached\n\n"
        "For more information see http://www.coda.cs.cmu.edu/\n"
        "Report bugs to <bugs@coda.cs.cmu.edu>.\n",
        argv0);
}

static void ParseCmdline(int argc, char **argv)
{
    int i, done = 0;

    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (STREQ(argv[i], "-h") || STREQ(argv[i], "--help") ||
                STREQ(argv[i], "-?"))
                done = 1, Usage(argv[0]);
            else if (STREQ(argv[i], "--version"))
                done = 1, printf("Venus " PACKAGE_VERSION "\n");

            else if (STREQ(argv[i], "-relay")) { /* default is 127.0.0.1 */
                i++;
                inet_aton(argv[i], &venus_relay_addr);
            } else if (STREQ(argv[i], "-k")) /* default is /dev/cfs0 */
                i++, kernDevice = argv[i];
            else if (STREQ(argv[i], "-mles")) /* total number of CML entries */
                i++, MLEs = atoi(argv[i]);
            else if (STREQ(argv[i], "-cf")) /* number of cache files */
                i++, CacheFiles = atoi(argv[i]);
            else if (STREQ(argv[i], "-pcfr")) /* partial cache files ratio */
                i++, PartialCacheFilesRatio = atoi(argv[i]);
            else if (STREQ(argv[i], "-c")) /* cache block size */
                i++, CacheBlocks = ParseSizeWithUnits(argv[i]);
            else if (STREQ(argv[i], "-hdbes")) /* hoard DB entries */
                i++, HDBEs = atoi(argv[i]);
            else if (STREQ(argv[i], "-d")) /* debugging */
                i++, LogLevel = atoi(argv[i]);
            else if (STREQ(argv[i], "-rpcdebug")) { /* debugging */
                i++;
                RPC2_DebugLevel = atoi(argv[i]);
                RPC2_Trace      = 1;
            } else if (STREQ(argv[i], "-lwpdebug")) { /* debugging */
                i++;
                lwp_debug = atoi(argv[i]);
            } else if (STREQ(argv[i], "-rdstrace")) /* RDS heap tracing */
                MallocTrace = 1;
            else if (STREQ(argv[i], "-f")) /* location of cache files */
                i++, CacheDir = argv[i];
            else if (STREQ(argv[i], "-m"))
                i++, COPModes = atoi(argv[i]);
            else if (STREQ(argv[i],
                           "-maxworkers")) /* number of worker threads */
                i++, MaxWorkers = atoi(argv[i]);
            else if (STREQ(argv[i], "-maxcbservers"))
                i++, MaxCBServers = atoi(argv[i]);
            else if (STREQ(argv[i],
                           "-maxprefetchers")) /* max number of threads */
                i++, MaxPrefetchers = atoi(argv[i]); /* doing prefetch ioctl */
            else if (STREQ(argv[i], "-console")) /* location of console file */
                i++, consoleFile = argv[i];
            else if (STREQ(argv[i], "-retries")) /* number of rpc2 retries */
                i++, rpc2_retries = atoi(argv[i]);
            else if (STREQ(argv[i], "-timeout")) /* rpc timeout */
                i++, rpc2_timeout = atoi(argv[i]);
            else if (STREQ(argv[i], "-ws")) /* sftp window size */
                i++, sftp_windowsize = atoi(argv[i]);
            else if (STREQ(argv[i], "-sa")) /* sftp send ahead */
                i++, sftp_sendahead = atoi(argv[i]);
            else if (STREQ(argv[i], "-ap")) /* sftp ack point */
                i++, sftp_ackpoint = atoi(argv[i]);
            else if (STREQ(argv[i], "-ps")) /* sftp packet size */
                i++, sftp_packetsize = atoi(argv[i]);
            else if (STREQ(argv[i], "-init")) /* brain wipe rvm */
                InitMetaData = 1;
            else if (STREQ(argv[i], "-newinstance")) /* fake a 'reinit' */
                InitNewInstance = 1;
            else if (STREQ(argv[i], "-rvmt"))
                i++, RvmType = (rvm_type_t)(atoi(argv[i]));
            else if (STREQ(argv[i], "-vld")) /* location of log device */
                i++, VenusLogDevice = argv[i]; /* normally /usr/coda/LOG */
            else if (STREQ(argv[i], "-vlds")) /* log device size */
                i++, VenusLogDeviceSize = atoi(argv[i]);
            else if (STREQ(argv[i], "-vdd")) /* location of data device */
                i++, VenusDataDevice = argv[i]; /* normally /usr/coda/DATA */
            else if (STREQ(argv[i], "-vdds")) /* meta-data device size */
                i++, VenusDataDeviceSize = atoi(argv[i]);
            else if (STREQ(argv[i], "-rdscs"))
                i++, RdsChunkSize = atoi(argv[i]);
            else if (STREQ(argv[i], "-rdsnl"))
                i++, RdsNlists = atoi(argv[i]);
            else if (STREQ(argv[i], "-logopts"))
                i++, LogOpts = atoi(argv[i]);
            else if (STREQ(argv[i], "-swt")) /* short term pri weight */
                i++, FSO_SWT = atoi(argv[i]);
            else if (STREQ(argv[i], "-mwt")) /* med term priority weight */
                i++, FSO_MWT = atoi(argv[i]);
            else if (STREQ(argv[i], "-ssf")) /* short term scale factor */
                i++, FSO_SSF = atoi(argv[i]);
            else if (STREQ(argv[i],
                           "-primaryuser")) /* primary user of this machine */
                i++, PrimaryUser = atoi(argv[i]);
            else if (STREQ(argv[i], "-von"))
                rpc2_timeflag = 1;
            else if (STREQ(argv[i], "-voff"))
                rpc2_timeflag = 0;
            else if (STREQ(argv[i], "-vmon"))
                mrpc2_timeflag = 1;
            else if (STREQ(argv[i], "-vmoff"))
                mrpc2_timeflag = 0;
            else if (STREQ(argv[i], "-SearchForNOreFind"))
                SearchForNOreFind = 1;
            else if (STREQ(argv[i], "-noasr"))
                ASRallowed = 0;
            else if (STREQ(argv[i], "-novcb"))
                VCBEnabled = 0;
            else if (STREQ(argv[i], "-nowalk")) {
                extern char PeriodicWalksAllowed;
                PeriodicWalksAllowed = 0;
            } else if (STREQ(argv[i], "-spooldir")) {
                i++, SpoolDir = argv[i];
            }
            /* let venus listen to tcp port `venus', as mariner port, normally
             * it only listens to a unix domain socket */
            else if (STREQ(argv[i], "-MarinerTcp"))
                mariner_tcp_enable = 1;
            else if (STREQ(argv[i], "-noMarinerTcp"))
                mariner_tcp_enable = 0;
            else if (STREQ(argv[i], "-allow-reattach"))
                allow_reattach = 1;
            else if (STREQ(argv[i], "-masquerade")) /* always on */
                ;
            else if (STREQ(argv[i], "-nomasquerade")) /* always on */
                ;
            /* Private mapping ... */
            else if (STREQ(argv[i], "-mapprivate"))
                MapPrivate = true;
            else if (STREQ(argv[i], "-codatunnel")) {
                codatunnel_enabled = 1;
                eprint("codatunnel enabled");
            } else if (STREQ(argv[i], "-no-codatunnel")) {
                codatunnel_enabled = -1;
                eprint("codatunnel disabled");
            } else if (STREQ(argv[i], "-onlytcp")) {
                codatunnel_onlytcp = 1;
                eprint("codatunnel_onlytcp set");
            } else if (STREQ(argv[i], "-codafs")) {
                codafs_enabled = true;
            } else if (STREQ(argv[i], "-no-codafs")) {
                codafs_enabled = -1;
            } else if (STREQ(argv[i], "-9pfs")) {
                plan9server_enabled = true;
                eprint("9pfs enabled");
            } else if (STREQ(argv[i], "-no-9pfs")) {
                plan9server_enabled = -1;
                eprint("9pfs disabled");
            } else if (STREQ(argv[i], "-nofork")) {
                nofork = true;
            } else if (STREQ(argv[i], "-wfmax")) {
                i++, WholeFileMaxSize = ParseSizeWithUnits(argv[i]);
            } else if (STREQ(argv[i], "-wfmin")) {
                i++, WholeFileMinSize = ParseSizeWithUnits(argv[i]);
            } else if (STREQ(argv[i], "-wfstall")) {
                i++, WholeFileMaxStall = atoi(argv[i]);
            } else if (STREQ(argv[i], "-ccbs")) {
                i++, ParseCacheChunkBlockSize(argv[i]);
            } else {
                eprint("bad command line option %-4s", argv[i]);
                done = -1;
            }
        }
    }
    if (done)
        exit(done < 0 ? EXIT_INVALID_ARG : EXIT_SUCCESS);
}

/*
 * Use an adjusted logarithmic function experimentally linearlized around
 * the following points;
 * 2MB -> 85 cache files
 * 100MB -> 4166 cache files
 * 200MB -> 8333 cache files
 * With the logarithmic function the following values are obtained
 * 2MB -> 98 cache files
 * 100MB -> 4412 cache files
 * 200MB -> 8142 cache files
 */
static unsigned int CalculateCacheFiles(unsigned int CacheBlocks)
{
    static const int y_scale         = 24200;
    static const double x_scale_down = 500000;

    return (unsigned int)(y_scale * log(CacheBlocks / x_scale_down + 1));
}

#ifndef itoa
static char *itoa(int value, char *str, int base = 10)
{
    sprintf(str, "%d", value);
    return str;
}
#endif

static void LoadDefaultValuesIntoConfig()
{
    char tmp[256];

    venus_global_config.add("cachesize", MIN_CS);
    venus_global_config.add("cacheblocks",
                            itoa(ParseSizeWithUnits(MIN_CS), tmp));
    venus_global_config.add("cachefiles", itoa(MIN_CF, tmp));
    venus_global_config.add("cachechunkblocksize", "32KB");
    venus_global_config.add("wholefilemaxsize", "50MB");
    venus_global_config.add("wholefileminsize", "4MB");
    venus_global_config.add("wholefilemaxstall", "10");
    venus_global_config.add("partialcachefilesratio", "1");
    venus_global_config.add("cachedir", DFLT_CD);
    venus_global_config.add("checkpointdir", "/usr/coda/spool");
    venus_global_config.add("logfile", DFLT_LOGFILE);
    venus_global_config.add("errorlog", DFLT_ERRLOG);
    venus_global_config.add("kerneldevice", "/dev/cfs0,/dev/coda/0");
    venus_global_config.add("mapprivate", itoa(0, tmp));
    venus_global_config.add("marinersocket", "/usr/coda/spool/mariner");
    venus_global_config.add("masquerade_port", itoa(0, tmp));
    venus_global_config.add("allow_backfetch", itoa(0, tmp));
    venus_global_config.add("mountpoint", DFLT_VR);
    venus_global_config.add("primaryuser", itoa(UNSET_PRIMARYUSER, tmp));
    venus_global_config.add("realmtab", "/etc/coda/realms");
    venus_global_config.add("rvm_log", "/usr/coda/LOG");
    venus_global_config.add("rvm_data", "/usr/coda/DATA");
    venus_global_config.add("RPC2_timeout", itoa(DFLT_TO, tmp));
    venus_global_config.add("RPC2_retries", itoa(DFLT_RT, tmp));
    venus_global_config.add("serverprobe",
                            itoa(150, tmp)); // used to be 12 minutes
    venus_global_config.add("reintegration_age", itoa(0, tmp));
    venus_global_config.add("reintegration_time", itoa(15, tmp));
    venus_global_config.add("dontuservm", itoa(0, tmp));
    venus_global_config.add("cml_entries", itoa(0, tmp));
    venus_global_config.add("hoard_entries", itoa(0, tmp));
    venus_global_config.add("pid_file", DFLT_PIDFILE);
    venus_global_config.add("run_control_file", DFLT_CTRLFILE);
    venus_global_config.add("asrlauncher_path", "");
    venus_global_config.add("asrpolicy_path", "");
    venus_global_config.add("validateattrs", itoa(15, tmp));
    venus_global_config.add("isr", itoa(0, tmp));
    venus_global_config.add("codafs", itoa(1, tmp));
    venus_global_config.add("9pfs", itoa(0, tmp));
    venus_global_config.add("codatunnel", itoa(1, tmp));
    venus_global_config.add("onlytcp", itoa(0, tmp));
    venus_global_config.add("detect_reintegration_retry", itoa(1, tmp));
    venus_global_config.add("checkpointformat", "newc");
}

/* Initialize "general" unset command-line parameters to user specified values
 * or hard-wired defaults. */
/* Note that individual modules initialize their own unset command-line
 * parameters as appropriate. */
static void DefaultCmdlineParms()
{
    int DontUseRVM                     = 0;
    const char *CacheSize              = NULL;
    const char *TmpCacheChunkBlockSize = NULL;
    const char *TmpWFMax               = NULL;
    const char *TmpWFMin               = NULL;

    /* Load the "venus.conf" configuration file */
    codaconf_init("venus.conf");

    /* we will prefer the deprecated "cacheblocks" over "cachesize" */
    if (!CacheBlocks) {
        CODACONF_INT(CacheBlocks, "cacheblocks", 0);
        if (CacheBlocks)
            eprint(
                "Using deprecated config 'cacheblocks', try the more flexible 'cachesize'");
    }

    if (!CacheBlocks) {
        CODACONF_STR(CacheSize, "cachesize", MIN_CS);
        CacheBlocks = ParseSizeWithUnits(CacheSize);
    }

    /* In case of user misconfiguration */
    if (CacheBlocks < MIN_CB) {
        eprint("Cannot start: minimum cache size is %s", "2MB");
        exit(EXIT_UNCONFIGURED);
    }

    CODACONF_INT(CacheFiles, "cachefiles",
                 (int)CalculateCacheFiles(CacheBlocks));
    if (CacheFiles < MIN_CF) {
        eprint("Cannot start: minimum number of cache files is %d",
               CalculateCacheFiles(CacheBlocks));
        eprint("Cannot start: minimum number of cache files is %d", MIN_CF);
        exit(EXIT_UNCONFIGURED);
    }

    if (!CacheChunkBlockSize) {
        CODACONF_STR(TmpCacheChunkBlockSize, "cachechunkblocksize", "32KB");
        ParseCacheChunkBlockSize(TmpCacheChunkBlockSize);
    }

    if (!WholeFileMaxSize) {
        CODACONF_STR(TmpWFMax, "wholefilemaxsize", "50MB");
        WholeFileMaxSize = ParseSizeWithUnits(TmpWFMax);
    }

    if (!WholeFileMinSize) {
        CODACONF_STR(TmpWFMin, "wholefileminsize", "4MB");
        WholeFileMinSize = ParseSizeWithUnits(TmpWFMin);
    }

    CODACONF_INT(WholeFileMaxStall, "wholefilemaxstall", 10);

    CODACONF_INT(PartialCacheFilesRatio, "partialcachefilesratio", 1);

    CODACONF_STR(CacheDir, "cachedir", DFLT_CD);
    CODACONF_STR(SpoolDir, "checkpointdir", "/usr/coda/spool");
    CODACONF_STR(VenusLogFile, "logfile", DFLT_LOGFILE);
    CODACONF_STR(consoleFile, "errorlog", DFLT_ERRLOG);
    CODACONF_STR(kernDevice, "kerneldevice", "/dev/cfs0,/dev/coda/0");
    CODACONF_INT(MapPrivate, "mapprivate", 0);
    CODACONF_STR(MarinerSocketPath, "marinersocket", "/usr/coda/spool/mariner");
    CODACONF_INT(masquerade_port, "masquerade_port", 0);
    CODACONF_INT(allow_backfetch, "allow_backfetch", 0);
    CODACONF_STR(venusRoot, "mountpoint", DFLT_VR);
    CODACONF_INT(PrimaryUser, "primaryuser", UNSET_PRIMARYUSER);
    CODACONF_STR(realmtab, "realmtab", "/etc/coda/realms");
    CODACONF_STR(VenusLogDevice, "rvm_log", "/usr/coda/LOG");
    CODACONF_STR(VenusDataDevice, "rvm_data", "/usr/coda/DATA");

    CODACONF_INT(rpc2_timeout, "RPC2_timeout", DFLT_TO);
    CODACONF_INT(rpc2_retries, "RPC2_retries", DFLT_RT);

    CODACONF_INT(T1Interval, "serverprobe", 150); // used to be 12 minutes

    CODACONF_INT(default_reintegration_age, "reintegration_age", 0);
    CODACONF_INT(default_reintegration_time, "reintegration_time", 15);
    default_reintegration_time *= 1000; /* reintegration time is in msec */

#if defined(__CYGWIN32__)
    CODACONF_STR(CachePrefix, "cache_prefix",
                 "/?"
                 "?/C:/cygwin");
#else
    CachePrefix = "";
#endif

    CODACONF_INT(DontUseRVM, "dontuservm", 0);
    {
        if (DontUseRVM)
            RvmType = VM;
    }

    CODACONF_INT(MLEs, "cml_entries", 0);
    {
        if (!MLEs)
            MLEs = CacheFiles * MLES_PER_FILE;

        if (MLEs < MIN_MLE) {
            eprint("Cannot start: minimum number of cml entries is %d",
                   MIN_MLE);
            exit(EXIT_UNCONFIGURED);
        }
    }

    CODACONF_INT(HDBEs, "hoard_entries", 0);
    {
        if (!HDBEs)
            HDBEs = CacheFiles / FILES_PER_HDBE;

        if (HDBEs < MIN_HDBE) {
            eprint("Cannot start: minimum number of hoard entries is %d",
                   MIN_HDBE);
            exit(EXIT_UNCONFIGURED);
        }
    }

    CODACONF_STR(VenusPidFile, "pid_file", DFLT_PIDFILE);
    if (*VenusPidFile != '/') {
        char *tmp = (char *)malloc(strlen(CacheDir) + strlen(VenusPidFile) + 2);
        CODA_ASSERT(tmp);
        sprintf(tmp, "%s/%s", CacheDir, VenusPidFile);
        VenusPidFile = tmp;
    }

    CODACONF_STR(VenusControlFile, "run_control_file", DFLT_CTRLFILE);
    if (*VenusControlFile != '/') {
        char *tmp =
            (char *)malloc(strlen(CacheDir) + strlen(VenusControlFile) + 2);
        CODA_ASSERT(tmp);
        sprintf(tmp, "%s/%s", CacheDir, VenusControlFile);
        VenusControlFile = tmp;
    }

    CODACONF_STR(ASRLauncherFile, "asrlauncher_path", NULL);

    CODACONF_STR(ASRPolicyFile, "asrpolicy_path", NULL);

    CODACONF_INT(PiggyValidations, "validateattrs", 15);
    {
        if (PiggyValidations > MAX_PIGGY_VALIDATIONS)
            PiggyValidations = MAX_PIGGY_VALIDATIONS;
    }

    /* Enable special tweaks for running in a VM
     * - Write zeros to container file contents before truncation.
     * - Disable reintegration replay detection. */
    CODACONF_INT(option_isr, "isr", 0);

    /* Kernel filesystem support */
    CODACONF_INT(codafs_enabled, "codafs", 1);
    CODACONF_INT(plan9server_enabled, "9pfs", 0);

    /* Allow overriding of the default setting from command line */
    if (codafs_enabled == -1)
        codafs_enabled = false;
    if (plan9server_enabled == -1)
        plan9server_enabled = false;

    /* Enable client-server communication helper process */
    CODACONF_INT(codatunnel_enabled, "codatunnel", 1);
    CODACONF_INT(codatunnel_onlytcp, "onlytcp", 0);

    if (codatunnel_onlytcp && codatunnel_enabled != -1)
        codatunnel_enabled = 1;
    if (codatunnel_enabled == -1) {
        codatunnel_onlytcp = 0;
        codatunnel_enabled = 0;
    }

    CODACONF_INT(detect_reintegration_retry, "detect_reintegration_retry", 1);
    if (option_isr) {
        detect_reintegration_retry = 0;
    }

    CODACONF_STR(CheckpointFormat, "checkpointformat", "newc");
    if (strcmp(CheckpointFormat, "tar") == 0)
        archive_type = TAR_TAR;
    if (strcmp(CheckpointFormat, "ustar") == 0)
        archive_type = TAR_USTAR;
    if (strcmp(CheckpointFormat, "odc") == 0)
        archive_type = CPIO_ODC;
    if (strcmp(CheckpointFormat, "newc") == 0)
        archive_type = CPIO_NEWC;

#ifdef moremoremore
    char *x = NULL;
    CODACONF_STR(x, "relay", NULL, "127.0.0.1");
    inet_aton(x, &venus_relay_addr);
#endif
}

static const char CACHEDIR_TAG[] =
    "Signature: 8a477f597d28d172789f06886806bc55\n"
    "# This file is a cache directory tag created by the Coda client (venus).\n"
    "# For information about cache directory tags, see:\n"
    "#   http://www.brynosaurus.com/cachedir/";

static void CdToCacheDir()
{
    struct stat statbuf;
    int fd;

    if (stat(CacheDir, &statbuf) != 0) {
        if (errno != ENOENT) {
            perror("CacheDir stat");
            exit(EXIT_FAILURE);
        }

        if (mkdir(CacheDir, 0700)) {
            perror("CacheDir mkdir");
            exit(EXIT_FAILURE);
        }
    }
    if (chdir(CacheDir)) {
        perror("CacheDir chdir");
        exit(EXIT_FAILURE);
    }

    /* create CACHEDIR.TAG as hint for backup programs */
    if (stat("CACHEDIR.TAG", &statbuf) == 0)
        return;

    fd = open("CACHEDIR.TAG", O_CREAT | O_WRONLY | O_EXCL | O_NOFOLLOW, 0444);
    if (fd == -1)
        return;
    write(fd, CACHEDIR_TAG, sizeof(CACHEDIR_TAG) - 1);
    close(fd);
}

static void CheckInitFile()
{
    char initPath[MAXPATHLEN];
    struct stat tstat;

    /* Construct name for INIT file */
    snprintf(initPath, MAXPATHLEN, "%s/INIT", CacheDir);

    /* See if it's there */
    if (stat(initPath, &tstat) == 0) {
        /* If so, set InitMetaData */
        InitMetaData = 1;
    } else if ((errno == ENOENT) && (InitMetaData == 1)) {
        int initFD;

        /* If not and it should be, create it. */
        initFD = open(initPath, O_CREAT, S_IRUSR);
        if (initFD) {
            write(initFD, initPath, strlen(initPath));
            close(initFD);
        }
    }
}

static void UnsetInitFile()
{
    char initPath[MAXPATHLEN];

    /* Create the file, if it doesn't already exist */
    snprintf(initPath, MAXPATHLEN, "%s/INIT", CacheDir);
    unlink(initPath);
}

static void SetRlimits()
{
#ifndef __CYGWIN32__
    /* Set DATA segment limit to maximum allowable. */
    struct rlimit rl;
    if (getrlimit(RLIMIT_DATA, &rl) < 0) {
        perror("getrlimit");
        exit(EXIT_FAILURE);
    }

    rl.rlim_cur = rl.rlim_max;
    if (setrlimit(RLIMIT_DATA, &rl) < 0) {
        perror("setrlimit");
        exit(EXIT_FAILURE);
    }
#endif
}
