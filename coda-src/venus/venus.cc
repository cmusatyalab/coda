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
#include <venusconf.h>
#include <codaconffileparser.h>
#include <codaconfcmdlineparser.h>

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

VenusConf global_config;
CodaConfFileParser config_file_parser(global_config);
CodaConfCmdLineParser cmlline_parser(global_config);

/* *****  Private constants  ***** */

struct timeval DaemonExpiry = { TIMERINTERVAL, 0 };

/* *****  Private routines  ***** */

static void LoadDefaultValuesIntoConfig();
static void AddCmdLineOptionsToConfigurationParametersMapping();
static void ApplyConsistencyRules();
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
    int ret_code        = 0;

    LoadDefaultValuesIntoConfig();
    AddCmdLineOptionsToConfigurationParametersMapping();

    // ParseCmdline(argc, argv);
    // DefaultCmdlineParms(); /* read /etc/coda/venus.conf */

    config_file_parser.set_conffile("venus.conf");
    config_file_parser.parse();

    cmlline_parser.set_args(argc, argv);
    ret_code = cmlline_parser.parse();
    if (!ret_code)
        exit(EXIT_INVALID_ARG);
    global_config.print();

    ApplyConsistencyRules();

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

    global_config.add("cachesize", MIN_CS);
    global_config.add("cacheblocks", itoa(0, tmp));
    global_config.add("cachefiles", itoa(0, tmp));
    global_config.add("cachechunkblocksize", "32KB");
    global_config.add("wholefilemaxsize", "50MB");
    global_config.add("wholefileminsize", "4MB");
    global_config.add("wholefilemaxstall", "10");
    global_config.add("partialcachefilesratio", "1");
    global_config.add("cachedir", DFLT_CD);
    global_config.add("checkpointdir", "/usr/coda/spool");
    global_config.add("logfile", DFLT_LOGFILE);
    global_config.add("errorlog", DFLT_ERRLOG);
    global_config.add("kerneldevice", "/dev/cfs0,/dev/coda/0");
    global_config.add("mapprivate", itoa(0, tmp));
    global_config.add("marinersocket", "/usr/coda/spool/mariner");
    global_config.add("masquerade_port", itoa(0, tmp));
    global_config.add("allow_backfetch", itoa(0, tmp));
    global_config.add("mountpoint", DFLT_VR);
    global_config.add("primaryuser", itoa(UNSET_PRIMARYUSER, tmp));
    global_config.add("realmtab", "/etc/coda/realms");
    global_config.add("rvm_log", "/usr/coda/LOG");
    global_config.add("rvm_data", "/usr/coda/DATA");
    global_config.add("RPC2_timeout", itoa(DFLT_TO, tmp));
    global_config.add("RPC2_retries", itoa(DFLT_RT, tmp));
    global_config.add("serverprobe", itoa(150, tmp));
    global_config.add("reintegration_age", itoa(0, tmp));
    global_config.add("reintegration_time", itoa(15, tmp));
    global_config.add("dontuservm", itoa(0, tmp));
    global_config.add("cml_entries", itoa(0, tmp));
    global_config.add("hoard_entries", itoa(0, tmp));
    global_config.add("pid_file", DFLT_PIDFILE);
    global_config.add("run_control_file", DFLT_CTRLFILE);
    global_config.add("asrlauncher_path", "");
    global_config.add("asrpolicy_path", "");
    global_config.add("validateattrs", itoa(15, tmp));
    global_config.add("isr", itoa(0, tmp));
    global_config.add("codafs", itoa(1, tmp));
    global_config.add("no-codafs", itoa(0, tmp));
    global_config.add("9pfs", itoa(0, tmp));
    global_config.add("no-9pfs", itoa(1, tmp));
    global_config.add("codatunnel", itoa(1, tmp));
    global_config.add("no-codatunnel", itoa(0, tmp));
    global_config.add("onlytcp", itoa(0, tmp));
    global_config.add("detect_reintegration_retry", itoa(1, tmp));
    global_config.add("checkpointformat", "newc");

    //Newly added
    global_config.add("initmetadata", "0");
    global_config.add("loglevel", "0");
    global_config.add("rpc2loglevel", "0");
    global_config.add("lwploglevel", "0");
    global_config.add("rdstrace", "0");
    global_config.add("copmodes", "6");
    global_config.add("maxworkers", itoa(UNSET_MAXWORKERS, tmp));
    global_config.add("maxcbservers", itoa(UNSET_MAXCBSERVERS, tmp));
    global_config.add("maxprefetchers", itoa(UNSET_MAXWORKERS, tmp));
    global_config.add("sftp_windowsize", itoa(UNSET_WS, tmp));
    global_config.add("sftp_sendahead", itoa(UNSET_SA, tmp));
    global_config.add("sftp_ackpoint", itoa(UNSET_AP, tmp));
    global_config.add("sftp_packetsize", itoa(UNSET_PS, tmp));
    global_config.add("rvmtype", itoa(UNSET, tmp));
    global_config.add("rvm_log_size", itoa(UNSET_VLDS, tmp));
    global_config.add("rvm_data_size", itoa(UNSET_VDDS, tmp));
    global_config.add("rds_chunk_size", itoa(UNSET_RDSCS, tmp));
    global_config.add("rds_list_size", itoa(UNSET_RDSNL, tmp));
    global_config.add("log_optimization", "1");

    global_config.add("swt", itoa(UNSET_SWT, tmp));
    global_config.add("mwt", itoa(UNSET_MWT, tmp));
    global_config.add("ssf", itoa(UNSET_SSF, tmp));
    global_config.add("von", itoa(UNSET_RT, tmp));
    global_config.add("voff", itoa(UNSET_RT, tmp));
    global_config.add("vmon", itoa(UNSET_MT, tmp));
    global_config.add("vmoff", itoa(UNSET_MT, tmp));
    global_config.add("SearchForNOreFind", "0");
    global_config.add("noasr", "0");
    global_config.add("novcb", "0");
    global_config.add("nowalk", "0");
#if defined(HAVE_SYS_UN_H) && !defined(__CYGWIN32__)
    global_config.add("MarinerTcp", "0");
    global_config.add("noMarinerTcp", "1");
#else
    global_config.add("MarinerTcp", "1");
    global_config.add("noMarinerTcp", "0");
#endif
    global_config.add("allow-reattach", "0");
    global_config.add("nofork", "0");
}

static void AddCmdLineOptionsToConfigurationParametersMapping()
{
    char tmp[256];

    global_config.add_key_alias("cachesize", "-c");
    global_config.add_key_alias("cachefiles", "-cf");
    global_config.add_key_alias("cachechunkblocksize", "-ccbs");
    global_config.add_key_alias("wholefilemaxsize", "-wfmax");
    global_config.add_key_alias("wholefileminsize", "-wfmin");
    global_config.add_key_alias("wholefilemaxstall", "-wfstall");
    global_config.add_key_alias("partialcachefilesratio", "-pcfr");
    global_config.add_key_alias("initmetadata", "-init");
    global_config.add_key_alias("cachedir", "-f");
    global_config.add_key_alias("checkpointdir", "-spooldir");
    //global_config.add_key_alias("logfile", "");
    global_config.add_key_alias("errorlog", "-console");
    global_config.add_key_alias("kerneldevice", "-k");
    global_config.add_key_alias("mapprivate", "-mapprivate");
    //global_config.add_key_alias("marinersocket", "");
    //global_config.add_key_alias("masquerade_port", "");
    //global_config.add_key_alias("allow_backfetch", "");
    //global_config.add_key_alias("mountpoint", "");
    global_config.add_key_alias("primaryuser", "-primaryuser");
    //global_config.add_key_alias("realmtab", "");
    global_config.add_key_alias("rvm_log", "-vld");
    global_config.add_key_alias("rvm_log_size", "-vlds");
    global_config.add_key_alias("rvm_data", "-vdd");
    global_config.add_key_alias("rvm_data_size", "-vdds");
    global_config.add_key_alias("RPC2_timeout", "-timeout");
    global_config.add_key_alias("RPC2_retries", "-retries");
    //global_config.add_key_alias("serverprobe", "");
    //global_config.add_key_alias("reintegration_age", "");
    //global_config.add_key_alias("reintegration_time", "");
    //global_config.add_key_alias("dontuservm", "");
    global_config.add_key_alias("cml_entries", "-mles");
    global_config.add_key_alias("hoard_entries", "-hdbes");
    //global_config.add_key_alias("pid_file", "");
    //global_config.add_key_alias("run_control_file", "");
    //global_config.add_key_alias("asrlauncher_path", "");
    //global_config.add_key_alias("asrpolicy_path", "");
    //global_config.add_key_alias("validateattrs", "");
    //global_config.add_key_alias("isr", "");
    global_config.add_key_alias("codafs", "-codafs");
    global_config.add_key_alias("no-codafs", "-no-codafs");
    global_config.add_key_alias("9pfs", "-9pfs");
    global_config.add_key_alias("no-9pfs", "-no-9pfs");
    global_config.add_key_alias("codatunnel", "-codatunnel");
    global_config.add_key_alias("no-codatunnel", "-no-codatunnel");
    global_config.add_key_alias("onlytcp", "-onlytcp");
    //global_config.add_key_alias("detect_reintegration_retry", "");
    //global_config.add_key_alias("checkpointformat", "");

    global_config.add_key_alias("loglevel", "-d");
    global_config.add_key_alias("rpc2loglevel", "-rpcdebug");
    global_config.add_key_alias("lwploglevel", "-lwpdebug");
    global_config.add_key_alias("rdstrace", "-rdstrace");
    global_config.add_key_alias("copmodes", "-m");
    global_config.add_key_alias("maxworkers", "-maxworkers");
    global_config.add_key_alias("maxcbservers", "-maxcbservers");
    global_config.add_key_alias("maxprefetchers", "-maxprefetchers");
    global_config.add_key_alias("sftp_windowsize", "-ws");
    global_config.add_key_alias("sftp_sendahead", "-sa");
    global_config.add_key_alias("sftp_ackpoint", "-ap");
    global_config.add_key_alias("sftp_packetsize", "-ps");
    global_config.add_key_alias("rvmtype", "-rvmt");
    global_config.add_key_alias("rds_chunk_size", "-rdscs");
    global_config.add_key_alias("rds_list_size", "-rdsnl");
    global_config.add_key_alias("log_optimization", "-logopts");

    global_config.add_key_alias("swt", "-swt");
    global_config.add_key_alias("mwt", "-mwt");
    global_config.add_key_alias("ssf", "-ssf");
    global_config.add_key_alias("von", "-von");
    global_config.add_key_alias("voff", "-voff");
    global_config.add_key_alias("vmon", "-vmon");
    global_config.add_key_alias("vmoff", "-vmoff");
    global_config.add_key_alias("SearchForNOreFind", "-SearchForNOreFind");
    global_config.add_key_alias("noasr", "-noasr");
    global_config.add_key_alias("novcb", "-novcb");
    global_config.add_key_alias("nowalk", "-nowalk");
    global_config.add_key_alias("MarinerTcp", "-MarinerTcp");
    global_config.add_key_alias("noMarinerTcp", "-noMarinerTcp");
    global_config.add_key_alias("allow-reattach", "-allow-reattach");
    global_config.add_key_alias("masquerade", "-masquerade");
    global_config.add_key_alias("nomasquerade", "-nomasquerade");
    global_config.add_key_alias("nofork", "-nofork");
}

/* TODO: This functions should be removed once all the gets are
 * moved to the corresponding subsystems */
static void ApplyConsistencyRules()
{
    int DontUseRVM       = 0;
    const char *TmpChar  = NULL;
    const char *TmpWFMax = NULL;
    const char *TmpWFMin = NULL;

    /* we will prefer the deprecated "cacheblocks" over "cachesize" */
    CacheBlocks = global_config.get_int_value("cacheblocks");
    if (CacheBlocks)
        eprint(
            "Using deprecated config 'cacheblocks', try the more flexible 'cachesize'");
    else {
        CacheBlocks = ParseSizeWithUnits(global_config.get_value("cachesize"));
    }
    if (CacheBlocks < MIN_CB) {
        eprint("Cannot start: minimum cache size is %s", "2MB");
        exit(EXIT_UNCONFIGURED);
    }

    CacheFiles = global_config.get_int_value("cachefiles");
    if (CacheFiles == 0) {
        CacheFiles = (int)CalculateCacheFiles(CacheBlocks);
    }

    if (CacheFiles < MIN_CF) {
        eprint("Cannot start: minimum number of cache files is %d",
               CalculateCacheFiles(CacheBlocks));
        eprint("Cannot start: minimum number of cache files is %d", MIN_CF);
        exit(EXIT_UNCONFIGURED);
    }

    ParseCacheChunkBlockSize(global_config.get_value("cachechunkblocksize"));

    WholeFileMaxSize =
        ParseSizeWithUnits(global_config.get_value("wholefilemaxsize"));
    WholeFileMinSize =
        ParseSizeWithUnits(global_config.get_value("wholefileminsize"));
    WholeFileMaxStall =
        ParseSizeWithUnits(global_config.get_value("wholefilemaxstall"));

    PartialCacheFilesRatio =
        global_config.get_int_value("partialcachefilesratio");

    CacheDir            = global_config.get_value("cachedir");
    SpoolDir            = global_config.get_value("checkpointdir");
    VenusLogFile        = global_config.get_value("logfile");
    consoleFile         = global_config.get_value("errorlog");
    kernDevice          = global_config.get_value("kerneldevice");
    MapPrivate          = global_config.get_int_value("mapprivate");
    MarinerSocketPath   = global_config.get_value("marinersocket");
    masquerade_port     = global_config.get_int_value("masquerade_port");
    allow_backfetch     = global_config.get_int_value("allow_backfetch");
    venusRoot           = global_config.get_value("mountpoint");
    PrimaryUser         = global_config.get_int_value("primaryuser");
    realmtab            = global_config.get_value("realmtab");
    VenusLogDevice      = global_config.get_value("rvm_log");
    VenusLogDeviceSize  = global_config.get_int_value("rvm_log_size");
    VenusDataDevice     = global_config.get_value("rvm_data");
    VenusDataDeviceSize = global_config.get_int_value("rvm_data_size");

    rpc2_timeout = global_config.get_int_value("RPC2_timeout");
    rpc2_retries = global_config.get_int_value("RPC2_retries");

    T1Interval = global_config.get_int_value("serverprobe");

    default_reintegration_age =
        global_config.get_int_value("reintegration_age");
    default_reintegration_time =
        global_config.get_int_value("reintegration_time");
    default_reintegration_time *= 1000; /* reintegration time is in msec */

    CachePrefix = "";

    DontUseRVM = global_config.get_int_value("dontuservm");
    {
        if (DontUseRVM)
            RvmType = VM;
    }

    MLEs = global_config.get_int_value("cml_entries");
    {
        if (!MLEs)
            MLEs = CacheFiles * MLES_PER_FILE;

        if (MLEs < MIN_MLE) {
            eprint("Cannot start: minimum number of cml entries is %d",
                   MIN_MLE);
            exit(EXIT_UNCONFIGURED);
        }
    }

    HDBEs = global_config.get_int_value("hoard_entries");
    {
        if (!HDBEs)
            HDBEs = CacheFiles / FILES_PER_HDBE;

        if (HDBEs < MIN_HDBE) {
            eprint("Cannot start: minimum number of hoard entries is %d",
                   MIN_HDBE);
            exit(EXIT_UNCONFIGURED);
        }
    }

    TmpChar = global_config.get_value("pid_file");
    if (*TmpChar != '/') {
        char *tmp = (char *)malloc(strlen(CacheDir) + strlen(TmpChar) + 2);
        CODA_ASSERT(tmp);
        sprintf(tmp, "%s/%s", CacheDir, TmpChar);
        printf("%s\n", tmp);
        VenusPidFile = tmp;
    } else {
        VenusPidFile = TmpChar;
    }

    TmpChar = global_config.get_value("run_control_file");
    if (*TmpChar != '/') {
        char *tmp = (char *)malloc(strlen(CacheDir) + strlen(TmpChar) + 2);
        CODA_ASSERT(tmp);
        sprintf(tmp, "%s/%s", CacheDir, TmpChar);
        VenusControlFile = tmp;
    } else {
        VenusControlFile = TmpChar;
    }

    ASRLauncherFile = global_config.get_value("asrlauncher_path");

    ASRPolicyFile = global_config.get_value("asrpolicy_path");

    PiggyValidations = global_config.get_int_value("validateattrs");
    {
        if (PiggyValidations > MAX_PIGGY_VALIDATIONS)
            PiggyValidations = MAX_PIGGY_VALIDATIONS;
    }

    /* Enable special tweaks for running in a VM
     * - Write zeros to container file contents before truncation.
     * - Disable reintegration replay detection. */
    option_isr = global_config.get_int_value("isr");

    /* Kernel filesystem support */
    codafs_enabled = global_config.get_int_value("codafs");
    codafs_enabled = global_config.get_int_value("nocodafs") ? 0 :
                                                               codafs_enabled;
    plan9server_enabled = global_config.get_int_value("9pfs");
    plan9server_enabled =
        global_config.get_int_value("no9pfs") ? 0 : plan9server_enabled;

    /* Allow overriding of the default setting from command line */
    if (codafs_enabled == -1)
        codafs_enabled = false;
    if (plan9server_enabled == -1)
        plan9server_enabled = false;

    /* Enable client-server communication helper process */
    codatunnel_enabled = global_config.get_int_value("codatunnel");
    codatunnel_enabled =
        global_config.get_int_value("no-codatunnel") ? 0 : codatunnel_enabled;
    codatunnel_onlytcp = global_config.get_int_value("onlytcp");

    if (codatunnel_onlytcp && codatunnel_enabled != -1)
        codatunnel_enabled = 1;
    if (codatunnel_enabled == -1) {
        codatunnel_onlytcp = 0;
        codatunnel_enabled = 0;
    }

    detect_reintegration_retry =
        global_config.get_int_value("detect_reintegration_retry");
    if (option_isr) {
        detect_reintegration_retry = 0;
    }

    CheckpointFormat = global_config.get_value("checkpointformat");
    if (strcmp(CheckpointFormat, "tar") == 0)
        archive_type = TAR_TAR;
    if (strcmp(CheckpointFormat, "ustar") == 0)
        archive_type = TAR_USTAR;
    if (strcmp(CheckpointFormat, "odc") == 0)
        archive_type = CPIO_ODC;
    if (strcmp(CheckpointFormat, "newc") == 0)
        archive_type = CPIO_NEWC;

    // Command line only
    LogLevel          = global_config.get_int_value("loglevel");
    RPC2_Trace        = global_config.get_bool_value("loglevel") ? 1 : 0;
    RPC2_DebugLevel   = global_config.get_int_value("rpc2loglevel");
    lwp_debug         = global_config.get_int_value("lwploglevel");
    MallocTrace       = global_config.get_int_value("rdstrace");
    COPModes          = global_config.get_int_value("copmodes");
    MaxWorkers        = global_config.get_int_value("maxworkers");
    MaxCBServers      = global_config.get_int_value("maxcbservers");
    MaxPrefetchers    = global_config.get_int_value("maxprefetchers");
    sftp_windowsize   = global_config.get_int_value("sftp_windowsize");
    sftp_sendahead    = global_config.get_int_value("sftp_sendahead");
    sftp_ackpoint     = global_config.get_int_value("sftp_ackpoint");
    sftp_packetsize   = global_config.get_int_value("sftp_packetsize");
    RvmType           = (rvm_type_t)global_config.get_int_value("rvmtype");
    RdsChunkSize      = global_config.get_int_value("rds_chunk_size");
    RdsNlists         = global_config.get_int_value("rds_list_size");
    LogOpts           = global_config.get_int_value("log_optimization");
    FSO_SWT           = global_config.get_int_value("swt");
    FSO_MWT           = global_config.get_int_value("mwt");
    FSO_SSF           = global_config.get_int_value("ssf");
    rpc2_timeflag     = global_config.get_int_value("von") ? 1 : 0;
    rpc2_timeflag     = global_config.get_int_value("voff") ? 0 : 0;
    mrpc2_timeflag    = global_config.get_int_value("vmon") ? 1 : 0;
    mrpc2_timeflag    = global_config.get_int_value("vmoff") ? 0 : 0;
    SearchForNOreFind = global_config.get_int_value("SearchForNOreFind");
    ASRallowed        = global_config.get_int_value("noasr") ? 0 : 1;
    VCBEnabled        = global_config.get_int_value("novcb") ? 0 : 1;
    extern char PeriodicWalksAllowed;
    PeriodicWalksAllowed = global_config.get_int_value("nowalk") ? 0 : 1;
    mariner_tcp_enable   = global_config.get_int_value("MarinerTcp") ? 1 : 1;
    mariner_tcp_enable   = global_config.get_int_value("noMarinerTcp") ? 0 : 1;
    allow_reattach       = global_config.get_int_value("allow-reattach");
    nofork               = global_config.get_int_value("nofork");

    InitMetaData = global_config.get_int_value("-init");
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
