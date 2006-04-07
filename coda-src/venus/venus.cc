/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
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
#include <netdb.h>
#ifdef __cplusplus
}
#endif

/* interfaces */
#include <vice.h>

/* from venus */
#include "advice.h"
#include "adv_daemon.h"
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
int SearchForNOreFind;  // Look for better detection method for iterrupted hoard walks. mre 1/18/93

/* Command-line/venus.conf parameters. */
char *consoleFile;
char *venusRoot;
char *kernDevice;
char *realmtab;
char *CacheDir;
char *CachePrefix;
int   CacheBlocks;
uid_t PrimaryUser = UNSET_PRIMARYUSER;
char *SpoolDir;
char *VenusPidFile;
char *VenusControlFile;
char *VenusLogFile;
char *MarinerSocketPath;
int masquerade_port;
int PiggyValidations;


#if defined(HAVE_SYS_UN_H) && !defined(__CYGWIN32__)
int mariner_tcp_enable = 0;
#else
int mariner_tcp_enable = 1;
#endif

/* *****  Private constants  ***** */

struct timeval DaemonExpiry = {TIMERINTERVAL, 0};

/* *****  Private routines  ***** */

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

/* local-repair modification */
int main(int argc, char **argv)
{
    coda_assert_action = CODA_ASSERT_SLEEP;
    coda_assert_cleanup = VFSUnmount;

    ParseCmdline(argc, argv);
    DefaultCmdlineParms();   /* read /etc/coda/venus.conf */

    // Cygwin runs as a service and doesn't need to daemonize.
#ifndef __CYGWIN__
    if (LogLevel == 0)
	parent_fd = daemonize();
#endif

    update_pidfile(VenusPidFile);

    /* open the console file and print vital info */
    freopen(consoleFile, "a+", stderr);
    eprint("Coda Venus, version " PACKAGE_VERSION);

    CdToCacheDir(); 
    CheckInitFile();
    SetRlimits();
    /* Initialize.  N.B. order of execution is very important here! */
    /* RecovInit < VSGInit < VolInit < FSOInit < HDB_Init */

#ifdef DJGPP
    /* disable debug messages */
    __djgpp_set_quiet_socket(1);
#endif

#ifdef __CYGWIN32__
    /* MapPrivate does not work on Cygwin */
    if (MapPrivate) {
      eprint ("Private mapping turned off, does not work on CYGWIN.");
      MapPrivate = 0;
    }
    V_UID = getuid();
#endif    

    /* test mismatch with kernel before doing real work */
    testKernDevice();

    /* 
     * VprocInit MUST precede LogInit. Log messages are stamped
     * with the id of the vproc that writes them, so log messages
     * can't be properly stamped until the vproc class is initialized.
     * The logging routines return without doing anything if LogInit 
     * hasn't yet been called. 
     */
    VprocInit();    /* init LWP/IOMGR support */
    LogInit();      /* move old Venus log and create a new one */
   
    LWP_SetLog(logFile, lwp_debug);
    RPC2_SetLog(logFile, RPC2_DebugLevel);
    DaemonInit();   /* before any Daemons initialize and after LogInit */
    StatsInit();
    SigInit();      /* set up signal handlers */

    DIR_Init(RvmType == VM ? DIR_DATA_IN_VM : DIR_DATA_IN_RVM);
    RecovInit();    /* set up RVM and recov daemon */
    CommInit();     /* set up RPC2, {connection,server,mgroup} lists, probe daemon */
    UserInit();     /* fire up user daemon */
    VSGDBInit();    /* init VSGDB */
    RealmDBInit();
    VolInit();	    /* init VDB, daemon */
    FSOInit();      /* allocate FSDB if necessary, recover FSOs, start FSO daemon */
    VolInitPost();  /* drop extra volume refcounts */
    HDB_Init();     /* allocate HDB if necessary, scan entries, start the HDB daemon */
    MarinerInit();  /* set up mariner socket */
    WorkerInit();   /* open kernel device */
    CallBackInit(); /* set up callback subsystem and create callback server threads */
    AdviceInit();   /* set up AdSrv and start the advice daemon */
    LRInit();	    /* set up local-repair database */

    /* Get the Root Volume. */
    eprint("Mounting root volume...");

    VFSMount();
#ifdef DJGPP
    k_Purge();
#endif

    UnsetInitFile();
    eprint("Venus starting...");

    freopen("/dev/null", "w", stdout);

    /* Act as message-multiplexor/daemon-dispatcher. */
    for (;;) {
	/* Wait for a message or daemon expiry. */
	fd_set rfds;
	int maxfd = KernelFD;

	FD_ZERO(&rfds);
	FD_SET(KernelFD, &rfds);

	for (int fd = 0; fd <= MarinerMaxFD; fd++)
	    if (FD_ISSET(fd, &MarinerMask))
		FD_SET(fd, &rfds);

	if (MarinerMaxFD > maxfd)
	    maxfd = MarinerMaxFD;

	if (VprocSelect(maxfd+1, &rfds, 0, 0, &DaemonExpiry) > 0) {
	    /* Handle mariner request(s). */
	    MarinerMux(&rfds);

	    /* Handle worker request. */
	    if (FD_ISSET(KernelFD, &rfds))
		WorkerMux(&rfds);
	}
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

    exit(0);
}

static void Usage(char *argv0)
{
    printf("Usage: %s [OPTION]...\n\n"
"Options:\n"
" -init\t\t\t\twipe and reinitialize persistent state\n"
" -newinstance\t\t\tfake a 'reinit'\n"
" -primaryuser <n>\t\tprimary user\n"
" -mapprivate\t\t\tuse private mmap for RVM\n"
" -d <debug level>\t\tdebug level\n"
" -rpcdebug <n>\t\t\trpc2 debug level\n"
" -lwpdebug <n>\t\t\tlwp debug level\n"
" -cf <n>\t\t\t# of cache files\n"
" -c <n>\t\t\t\tcache size in KB\n"
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
" -weakthresh <n>\t\tstrong/weak threshold (bytes/sec)\n"
" -weakstale <n>\t\t\twhen estimates become too old (seconds)\n"
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
" -relay <addr>\t\t\trelay socket address (windows only)\n\n"
"For more information see http://www.coda.cs.cmu.edu/\n"
"Report bugs to <bugs@coda.cs.cmu.edu>.\n", argv0);
}

static void ParseCmdline(int argc, char **argv)
{
    int i, done = 0;

    for(i = 1; i < argc; i++) {
  	if (argv[i][0] == '-') {
	    if (STREQ(argv[i], "-h") || STREQ(argv[i], "--help") ||
		STREQ(argv[i], "-?"))
		done = 1, Usage(argv[0]);
	    else if (STREQ(argv[i], "--version"))
		done = 1, printf("Venus " PACKAGE_VERSION "\n");

	    else if (STREQ(argv[i], "-relay")) {   /* default is 127.0.0.1 */
 		i++;
		inet_aton(argv[i], &venus_relay_addr);
 	    } else if (STREQ(argv[i], "-k"))         /* default is /dev/cfs0 */
  		i++, kernDevice = argv[i];
	    else if (STREQ(argv[i], "-mles")) /* total number of CML entries */
		i++, MLEs = atoi(argv[i]);
	    else if (STREQ(argv[i], "-cf"))   /* number of cache files */
		i++, CacheFiles = atoi(argv[i]);
	    else if (STREQ(argv[i], "-c"))    /* cache block size */
		i++, CacheBlocks = atoi(argv[i]);  
	    else if (STREQ(argv[i], "-hdbes")) /* hoard DB entries */
		i++, HDBEs = atoi(argv[i]);
	    else if (STREQ(argv[i], "-d"))     /* debugging */
		i++, LogLevel = atoi(argv[i]);
	    else if (STREQ(argv[i], "-rpcdebug")) {    /* debugging */
		i++;
		RPC2_DebugLevel = atoi(argv[i]);
		RPC2_Trace = 1;
	    } else if (STREQ(argv[i], "-lwpdebug")) {    /* debugging */
		i++;
		lwp_debug =atoi(argv[i]);
	    } else if (STREQ(argv[i], "-rdstrace"))     /* RDS heap tracing */
		MallocTrace = 1;
	    else if (STREQ(argv[i], "-f"))     /* location of cache files */
		i++, CacheDir = argv[i];
	    else if (STREQ(argv[i], "-m"))
		i++, COPModes = atoi(argv[i]);
	    else if (STREQ(argv[i], "-maxworkers"))  /* number of worker threads */
		i++, MaxWorkers = atoi(argv[i]);
	    else if (STREQ(argv[i], "-weakthresh")) {   /* Threshold at which to go to weak mode */
		WCThresh = atoi(argv[++i]);		/* in Bytes/sec */
	    }
	    else if (STREQ(argv[i], "-weakstale")) {   /* When estimates become too old */
		extern int WCStale;
		WCStale = atoi(argv[++i]);		/* in seconds */
	    }
	    else if (STREQ(argv[i], "-maxcbservers")) 
		i++, MaxCBServers = atoi(argv[i]);
	    else if (STREQ(argv[i], "-maxprefetchers")) /* max number of threads */
		i++, MaxPrefetchers = atoi(argv[i]);    /* doing prefetch ioctl */
	    else if (STREQ(argv[i], "-console"))      /* location of console file */
		i++, consoleFile = argv[i];
	    else if (STREQ(argv[i], "-retries"))      /* number of rpc2 retries */
		i++, rpc2_retries = atoi(argv[i]);
	    else if (STREQ(argv[i], "-timeout"))      /* rpc timeout */
		i++, rpc2_timeout = atoi(argv[i]);
	    else if (STREQ(argv[i], "-ws"))           /* sftp window size */
		i++, sftp_windowsize = atoi(argv[i]);
	    else if (STREQ(argv[i], "-sa"))           /* sftp send ahead */
		i++, sftp_sendahead = atoi(argv[i]);
	    else if (STREQ(argv[i], "-ap"))           /* sftp ack point */
		i++, sftp_ackpoint = atoi(argv[i]);
	    else if (STREQ(argv[i], "-ps"))           /* sftp packet size */
		i++, sftp_packetsize = atoi(argv[i]);
	    else if (STREQ(argv[i], "-init"))        /* brain wipe rvm */
		InitMetaData = 1;
	    else if (STREQ(argv[i], "-newinstance")) /* fake a 'reinit' */
		InitNewInstance = 1;
	    else if (STREQ(argv[i], "-rvmt"))
		i++, RvmType = (rvm_type_t)(atoi(argv[i]));
	    else if (STREQ(argv[i], "-vld"))          /* location of log device */
		i++, VenusLogDevice = argv[i];        /* normally /usr/coda/LOG */
	    else if (STREQ(argv[i], "-vlds"))         /* log device size */
		i++, VenusLogDeviceSize = atoi(argv[i]);  
	    else if (STREQ(argv[i], "-vdd"))          /* location of data device */
                i++, VenusDataDevice = argv[i];       /* normally /usr/coda/DATA */
	    else if (STREQ(argv[i], "-vdds"))         /* meta-data device size */
		i++, VenusDataDeviceSize = atoi(argv[i]);
	    else if (STREQ(argv[i], "-rdscs"))        
		i++, RdsChunkSize = atoi(argv[i]);    
	    else if (STREQ(argv[i], "-rdsnl"))
		i++, RdsNlists = atoi(argv[i]);
	    else if (STREQ(argv[i], "-logopts"))
		i++, LogOpts = atoi(argv[i]);
	    else if (STREQ(argv[i], "-swt"))         /* short term pri weight */
		i++, FSO_SWT = atoi(argv[i]);
	    else if (STREQ(argv[i], "-mwt"))         /* med term priority weight */
		i++, FSO_MWT = atoi(argv[i]);
	    else if (STREQ(argv[i], "-ssf"))         /* short term scale factor */
		i++, FSO_SSF = atoi(argv[i]);
	    else if (STREQ(argv[i], "-alpha"))	     /* patience ALPHA value */
		i++, PATIENCE_ALPHA = atoi(argv[i]);
	    else if (STREQ(argv[i], "-beta"))	     /* patience BETA value */
		i++, PATIENCE_BETA = atoi(argv[i]);
	    else if (STREQ(argv[i], "-gamma"))	     /* patience GAMMA value */
		i++, PATIENCE_GAMMA = atoi(argv[i]);
	    else if (STREQ(argv[i], "-primaryuser")) /* primary user of this machine */
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
	    else if (STREQ(argv[i], "-noskk"))
	        SkkEnabled = 0;
	    else if (STREQ(argv[i], "-noasr"))
	        ASRallowed = 0;
	    else if (STREQ(argv[i], "-novcb"))
	        VCBEnabled = 0;
	    else if (STREQ(argv[i], "-nowalk")) {
	        extern char PeriodicWalksAllowed;
	        PeriodicWalksAllowed = 0;
	    }
	    else if (STREQ(argv[i], "-spooldir")) {
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
  	    else if (STREQ(argv[i], "-masquerade")) /* always on */;
  	    else if (STREQ(argv[i], "-nomasquerade")) /* always on */;
	    /* Private mapping ... */
	    else if (STREQ(argv[i], "-mapprivate"))
		MapPrivate = true;
	    else {
		eprint("bad command line option %-4s", argv[i]);
		done = -1;
	    }
	}
	else
	    venusRoot = argv[i];   /* default is /coda */
    }
    if (done) exit(done < 0 ? 1 : 0);
}


/* Initialize "general" unset command-line parameters to user specified values
 * or hard-wired defaults. */
/* Note that individual modules initialize their own unset command-line
 * parameters as appropriate. */
static void DefaultCmdlineParms()
{
    int DontUseRVM = 0;

    /* Load the "venus.conf" configuration file */
    codaconf_init("venus.conf");

    CODACONF_INT(CacheBlocks,	    "cacheblocks",   40000);
    CODACONF_STR(CacheDir,	    "cachedir",      DFLT_CD);
    CODACONF_STR(SpoolDir,	    "checkpointdir", "/usr/coda/spool");
    CODACONF_STR(VenusLogFile,	    "logfile",	     "/usr/coda/etc/venus.log");
    CODACONF_STR(consoleFile,	    "errorlog",      "/usr/coda/etc/console");
    CODACONF_STR(kernDevice,	    "kerneldevice",  "/dev/cfs0,/dev/coda/0");
    CODACONF_INT(MapPrivate,	    "mapprivate",     0);
    CODACONF_STR(MarinerSocketPath, "marinersocket", "/usr/coda/spool/mariner");
    CODACONF_INT(masquerade_port,   "masquerade_port", 0);
    CODACONF_STR(venusRoot,	    "mountpoint",     DFLT_VR);
    CODACONF_INT(PrimaryUser,	    "primaryuser",    UNSET_PRIMARYUSER);
    CODACONF_STR(realmtab,	    "realmtab",	      "/etc/coda/realms");
    CODACONF_STR(VenusLogDevice,    "rvm_log",        "/usr/coda/LOG");
    CODACONF_STR(VenusDataDevice,   "rvm_data",       "/usr/coda/DATA");

    CODACONF_INT(rpc2_timeout,	    "RPC2_timeout",   DFLT_TO);
    CODACONF_INT(rpc2_retries,	    "RPC2_retries",   DFLT_RT);

    CODACONF_INT(T1Interval,	    "serverprobe",    150);
    // used to be 12 minutes

#if defined(__CYGWIN32__)
    CODACONF_STR(CachePrefix, "cache_prefix", "/?" "?/C:/cygwin");
#else
    CachePrefix = "";
#endif

    CODACONF_INT(DontUseRVM, "dontuservm", 0);
    {
	if (DontUseRVM)
	    RvmType = VM;
    }

    CODACONF_INT(CacheFiles, "cachefiles", 0);
    {
	if (!CacheFiles) CacheFiles = CacheBlocks / BLOCKS_PER_FILE;

#ifdef DJGPP
	if (CacheFiles > 1500)
	    CacheFiles = 1500;
#endif
	if (CacheFiles < MIN_CF) {
	    eprint("Cannot start: minimum number of cache files is %d", MIN_CF);
	    exit(-1); 
	}
    }

    CODACONF_INT(MLEs, "cml_entries", 0);
    {
	if (!MLEs) MLEs = CacheFiles * MLES_PER_FILE;

	if (MLEs < MIN_MLE) {
	    eprint("Cannot start: minimum number of cml entries is %d",MIN_MLE);
	    exit(-1); 
	}
    }

    CODACONF_INT(HDBEs, "hoard_entries", 0);
    {
	if (!HDBEs) HDBEs = CacheFiles / FILES_PER_HDBE;

	if (HDBEs < MIN_HDBE) {
	    eprint("Cannot start: minimum number of hoard entries is %d",
		   MIN_HDBE);
	    exit(-1); 
	}
    }

    CODACONF_STR(VenusPidFile, "pid_file", NULL);
    {
#define PIDFILE "/pid"
	if (!VenusPidFile) {
	    VenusPidFile = (char *)malloc(strlen(CacheDir)+strlen(PIDFILE)+1);
	    strcpy(VenusPidFile, CacheDir);
	    strcat(VenusPidFile, PIDFILE);
	}
    }

    CODACONF_STR(VenusControlFile, "run_control_file", NULL);
    {
#define CTRLFILE "/VENUS_CTRL"
	if (!VenusControlFile) {
	    VenusControlFile=(char*)malloc(strlen(CacheDir)+strlen(CTRLFILE)+1);
	    strcpy(VenusControlFile, CacheDir);
	    strcat(VenusControlFile, CTRLFILE);
	}
    }

    CODACONF_INT(PiggyValidations, "validateattrs", 21);
    {
	if (PiggyValidations > MAX_PIGGY_VALIDATIONS)
	    PiggyValidations = MAX_PIGGY_VALIDATIONS;
    }

#ifdef moremoremore
    char *x = NULL;
    CODACONF_STR(x, "relay", NULL, "127.0.0.1");
    inet_aton(x, &venus_relay_addr);
#endif
}


static void CdToCacheDir() {
    if (chdir(CacheDir) < 0) {
	if (errno != ENOENT)
	    { perror("CacheDir chdir"); exit(-1); }
	if (mkdir(CacheDir, 0700) < 0)
	    { perror("CacheDir mkdir"); exit(-1); }
	if (chdir(CacheDir) < 0)
	    { perror("CacheDir chdir"); exit(-1); }
    }
}

static void CheckInitFile() {
    char initPath[MAXPATHLEN];
    struct stat tstat;

    /* Construct name for INIT file */
#ifndef DJGPP
        snprintf(initPath, MAXPATHLEN, "%s/INIT", CacheDir);
#else
        sprintf(initPath, "%s/INIT", CacheDir);
#endif

    /* See if it's there */ 
    if (stat(initPath, &tstat) == 0) {
        /* If so, set InitMetaData */
	InitMetaData = 1;
    } else if ((errno == ENOENT) && (InitMetaData == 1)) {
        int initFD;

	/* If not and it should be, create it. */
        initFD = open(initPath, O_CREAT, S_IREAD);
        if (initFD) {
	    write(initFD, initPath, strlen(initPath));
            close(initFD);
        }
    }
}

static void UnsetInitFile() {
    char initPath[MAXPATHLEN];

    /* Create the file, if it doesn't already exist */
#ifndef DJGPP
         snprintf(initPath, MAXPATHLEN, "%s/INIT", CacheDir);
#else
        sprintf(initPath, "%s/INIT", CacheDir);
#endif
    unlink(initPath);
}

static void SetRlimits() {
#if !defined(__CYGWIN32__) && !defined(DJGPP)
    /* Set DATA segment limit to maximum allowable. */
    struct rlimit rl;
    if (getrlimit(RLIMIT_DATA, &rl) < 0)
	{ perror("getrlimit"); exit(-1); }

    rl.rlim_cur = rl.rlim_max;
    if (setrlimit(RLIMIT_DATA, &rl) < 0)
	{ perror("setrlimit"); exit(-1); }
#endif
}

