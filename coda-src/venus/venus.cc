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
#include "venusmux.h"

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
const char *realmtab;
const char *CachePrefix;
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

/* Global red and yellow zone limits on CML length; default is infinite */
CodaConfFileParser config_file_parser(GetVenusConf());
CodaConfCmdLineParser cmlline_parser(GetVenusConf());

/* *****  Private constants  ***** */
struct timeval DaemonExpiry = { TIMERINTERVAL, 0 };

/* *****  Private routines  ***** */
static void CdToCacheDir();
static void CheckInitFile();
static void UnsetInitFile();
static void SetRlimits();
static void Usage(char *argv0);

struct in_addr venus_relay_addr = { INADDR_LOOPBACK };

/* *****  venus.c  ***** */


/* test if we can open the kernel device and purge the cache,
   BSD systems like to purge that cache */
void testKernDevice()
{
#ifdef __CYGWIN32__
    return;
#else
    const char *kernDevice = GetVenusConf().get_value("kerneldevice");
    int fd                 = -1;
    char *str, *p, *q = NULL;
    CODA_ASSERT((str = p = strdup(kernDevice)) != NULL);

    for (p = strtok(p, ","); p && fd == -1; p = strtok(NULL, ",")) {
        fd = ::open(p, O_RDWR, 0);
        if (fd >= 0)
            q = p;
    }

    /* If the open of the kernel device succeeds we know that there is
	   no other living venus. */
    if (fd < 0) {
        eprint("Probably another Venus is running! open failed for %s, exiting",
               kernDevice);
        free(str);
        exit(EXIT_FAILURE);
    }

    CODA_ASSERT(q);
    GetVenusConf().set("kerneldevice", q);
    free(str);

    /* Construct a purge message */
    union outputArgs msg;
    memset(&msg, 0, sizeof(msg));

    msg.oh.opcode = CODA_FLUSH;
    msg.oh.unique = 0;

    /* Send the message. */
    if (write(fd, (const void *)&msg, sizeof(struct coda_out_hdr)) !=
        sizeof(struct coda_out_hdr)) {
        eprint("Write for flush failed (%d), exiting", errno);
        exit(EXIT_FAILURE);
    }

    /* Close the kernel device. */
    if (close(fd) < 0) {
        eprint("close of %s failed (%d), exiting", kernDevice, errno);
        exit(EXIT_FAILURE);
    }
#endif
}

/* local-repair modification */
int main(int argc, char **argv)
{
    coda_assert_action  = CODA_ASSERT_SLEEP;
    coda_assert_cleanup = VFSUnmount;
    int ret_code        = 0;

    GetVenusConf().load_default_config();
    GetVenusConf().configure_cmdline_options();

    config_file_parser.set_conffile("venus.conf");
    config_file_parser.parse();

    cmlline_parser.set_args(argc, argv);
    ret_code = cmlline_parser.parse();
    if (!ret_code) {
        Usage(argv[0]);
        exit(EXIT_INVALID_ARG);
    }

    GetVenusConf().apply_consistency_rules();

    ret_code = GetVenusConf().check();
    if (ret_code)
        exit(EXIT_UNCONFIGURED);

    MapToLegacyVariables();

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

    int conf_fd = open("run.conf", O_WRONLY | O_CREAT, 644);
    GetVenusConf().print(conf_fd);

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

/* TODO: This functions should be removed once all the gets are
 * moved to the corresponding subsystems */
static void MapToLegacyVariables()
{
    int DontUseRVM = 0;

    ParseCacheChunkBlockSize(GetVenusConf().get_value("cachechunkblocksize"));

    PartialCacheFilesRatio =
        GetVenusConf().get_int_value("partialcachefilesratio");

    SpoolDir            = GetVenusConf().get_value("checkpointdir");
    VenusLogFile        = GetVenusConf().get_value("logfile");
    consoleFile         = GetVenusConf().get_value("errorlog");
    MapPrivate          = GetVenusConf().get_int_value("mapprivate");
    MarinerSocketPath   = GetVenusConf().get_value("marinersocket");
    masquerade_port     = GetVenusConf().get_int_value("masquerade_port");
    allow_backfetch     = GetVenusConf().get_int_value("allow_backfetch");
    venusRoot           = GetVenusConf().get_value("mountpoint");
    PrimaryUser         = GetVenusConf().get_int_value("primaryuser");
    realmtab            = GetVenusConf().get_value("realmtab");
    VenusLogDevice      = GetVenusConf().get_value("rvm_log");
    VenusLogDeviceSize  = GetVenusConf().get_int_value("rvm_log_size");
    VenusDataDevice     = GetVenusConf().get_value("rvm_data");
    VenusDataDeviceSize = GetVenusConf().get_int_value("rvm_data_size");

    rpc2_timeout = GetVenusConf().get_int_value("RPC2_timeout");
    rpc2_retries = GetVenusConf().get_int_value("RPC2_retries");

    T1Interval = GetVenusConf().get_int_value("serverprobe");

    default_reintegration_age =
        GetVenusConf().get_int_value("reintegration_age");
    default_reintegration_time =
        GetVenusConf().get_int_value("reintegration_time");
    default_reintegration_time *= 1000; /* reintegration time is in msec */

    CachePrefix = "";

    if (GetVenusConf().get_bool_value("dontuservm"))
        RvmType = VM;

    MLEs = GetVenusConf().get_int_value("cml_entries");

    HDBEs = GetVenusConf().get_int_value("hoard_entries");

    VenusPidFile     = GetVenusConf().get_value("pid_file");
    VenusControlFile = GetVenusConf().get_value("run_control_file");

    ASRLauncherFile = GetVenusConf().get_value("asrlauncher_path");

    ASRPolicyFile = GetVenusConf().get_value("asrpolicy_path");

    PiggyValidations = GetVenusConf().get_int_value("validateattrs");

    option_isr = GetVenusConf().get_int_value("isr");
    detect_reintegration_retry =
        GetVenusConf().get_int_value("detect_reintegration_retry");

    /* Kernel filesystem support */
    codafs_enabled      = GetVenusConf().get_int_value("codafs");
    plan9server_enabled = GetVenusConf().get_int_value("9pfs");

    /* Enable client-server communication helper process */
    codatunnel_enabled = GetVenusConf().get_int_value("codatunnel");
    codatunnel_onlytcp = GetVenusConf().get_int_value("onlytcp");

    CheckpointFormat = GetVenusConf().get_value("checkpointformat");
    if (strcmp(CheckpointFormat, "tar") == 0)
        archive_type = TAR_TAR;
    if (strcmp(CheckpointFormat, "ustar") == 0)
        archive_type = TAR_USTAR;
    if (strcmp(CheckpointFormat, "odc") == 0)
        archive_type = CPIO_ODC;
    if (strcmp(CheckpointFormat, "newc") == 0)
        archive_type = CPIO_NEWC;

    // Command line only
    LogLevel          = GetVenusConf().get_int_value("loglevel");
    RPC2_Trace        = GetVenusConf().get_bool_value("loglevel") ? 1 : 0;
    RPC2_DebugLevel   = GetVenusConf().get_int_value("rpc2loglevel");
    lwp_debug         = GetVenusConf().get_int_value("lwploglevel");
    MallocTrace       = GetVenusConf().get_int_value("rdstrace");
    COPModes          = GetVenusConf().get_int_value("copmodes");
    MaxWorkers        = GetVenusConf().get_int_value("maxworkers");
    MaxCBServers      = GetVenusConf().get_int_value("maxcbservers");
    MaxPrefetchers    = GetVenusConf().get_int_value("maxprefetchers");
    sftp_windowsize   = GetVenusConf().get_int_value("sftp_windowsize");
    sftp_sendahead    = GetVenusConf().get_int_value("sftp_sendahead");
    sftp_ackpoint     = GetVenusConf().get_int_value("sftp_ackpoint");
    sftp_packetsize   = GetVenusConf().get_int_value("sftp_packetsize");
    RvmType           = (rvm_type_t)GetVenusConf().get_int_value("rvmtype");
    RdsChunkSize      = GetVenusConf().get_int_value("rds_chunk_size");
    RdsNlists         = GetVenusConf().get_int_value("rds_list_size");
    LogOpts           = GetVenusConf().get_int_value("log_optimization");
    FSO_SWT           = GetVenusConf().get_int_value("swt");
    FSO_MWT           = GetVenusConf().get_int_value("mwt");
    FSO_SSF           = GetVenusConf().get_int_value("ssf");
    rpc2_timeflag     = GetVenusConf().get_int_value("von");
    mrpc2_timeflag    = GetVenusConf().get_int_value("vmon");
    SearchForNOreFind = GetVenusConf().get_int_value("SearchForNOreFind");
    ASRallowed        = GetVenusConf().get_int_value("noasr") ? 0 : 1;
    VCBEnabled        = GetVenusConf().get_int_value("novcb") ? 0 : 1;
    extern char PeriodicWalksAllowed;
    PeriodicWalksAllowed = GetVenusConf().get_int_value("nowalk") ? 0 : 1;
    mariner_tcp_enable   = GetVenusConf().get_int_value("MarinerTcp");
    allow_reattach       = GetVenusConf().get_int_value("allow-reattach");
    nofork               = GetVenusConf().get_int_value("nofork");

    InitMetaData = GetVenusConf().get_int_value("-init");
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
    const char *CacheDir = GetVenusConf().get_value("cachedir");

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
    const char *CacheDir = GetVenusConf().get_value("cachedir");

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
    const char *CacheDir = GetVenusConf().get_value("cachedir");

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
