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

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

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
#include <fcntl.h>
#include <netdb.h>
#ifdef __cplusplus
}
#endif __cplusplus

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
#include "venus.version.h"
#include "venuscb.h"
#include "venuswb.h"
#include "venusrecov.h"
#include "venusvol.h"
#include "vproc.h"
#include "worker.h"
#include "coda_assert.h"
#include "codaconf.h"

#include <coda_config.h>

/* FreeBSD 2.2.5 defines this in rpc/types.h, all others in netinet/in.h */
#ifndef INADDR_LOOPBACK
#define INADDR_LOOPBACK 0x7f000001
#endif

/* *****  Exported variables  ***** */
/* globals in the .bss are implicitly initialized to 0 according to ANSI-C standards */
vproc *Main;
ViceFid	rootfid;
long rootnodeid;
int CleanShutDown;
int SearchForNOreFind;  // Look for better detection method for iterrupted hoard walks. mre 1/18/93

/* Command-line/venus.conf parameters. */
char *consoleFile;
char *venusRoot;
char *kernDevice;
char *fsname;
char *CacheDir;
int   CacheBlocks;
char *RootVolName;
vuid_t PrimaryUser = (vuid_t)UNSET_PRIMARYUSER;
char *SpoolDir;
char *VenusPidFile;
char *VenusControlFile;
char *VenusLogFile;
char *MarinerSocketPath;
int masquerade;

#ifdef HAVE_SYS_UN_H
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

static char *venusdotconf = SYSCONFDIR "/venus.conf";

/* *****  venus.c  ***** */

/* local-repair modification */
int main(int argc, char **argv) {
    /* Print to the console -- important during reboot. */
#if ! defined(__CYGWIN32__) && ! defined(DJGPP)
    freopen("/dev/console", "w", stderr);
#endif
    fprintf(stderr, "Coda Venus, version %d.%d.%d\n\r",
	    VenusMajorVersion, VenusMinorVersion, VenusReleaseVersion);
    fflush(stderr);

    coda_assert_action = CODA_ASSERT_SLEEP;
    coda_assert_cleanup = VFSUnmount;

    ParseCmdline(argc, argv);
    DefaultCmdlineParms();   /* read /etc/coda/venus.conf */

    /* open the console file and print vital info */
    freopen(consoleFile, "w", stderr);
    fprintf(stderr, "Coda Venus, version %d.%d.%d\n",
             VenusMajorVersion, VenusMinorVersion, VenusReleaseVersion);
    fflush(stderr);
    
    CdToCacheDir(); 
    CheckInitFile();
    SetRlimits();
    /* Initialize.  N.B. order of execution is very important here! */
    /* RecovInit < VSGInit < VolInit < FSOInit < HDB_Init */

#ifdef DJGPP
    /* disable debug messages */
    __djgpp_set_quiet_socket(1);
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
    VolInit();      /* init VDB, daemon */
    FSOInit();      /* allocate FSDB if necessary, recover FSOs, start FSO daemon */
    HDB_Init();     /* allocate HDB if necessary, scan entries, start the HDB daemon */
    MarinerInit();  /* set up mariner socket */
    WorkerInit();   /* open kernel device */
    CallBackInit(); /* set up callback subsystem and create callback server threads */
    WritebackInit(); /* set up writeback subsystem */
    AdviceInit();   /* set up AdSrv and start the advice daemon */
    LRInit();	    /* set up local-repair database */
    //    VFSMount();

    /* Get the Root Volume. */
    eprint("Getting Root Volume information...");
    while (!GetRootVolume()) {
	ServerProbe();

	struct timeval tv;
	tv.tv_sec = 15;
	tv.tv_usec = 0;
	VprocSleep(&tv);
    }
    
    VFSMount();
#ifdef DJGPP
    k_Purge();
#endif

    UnsetInitFile();
    eprint("Venus starting...");

    /* Act as message-multiplexor/daemon-dispatcher. */
    for (;;) {
	/* Wait for a message or daemon expiry. */
	int rdfds = (KernelMask | MarinerMask);

	if (VprocSelect(sizeof(int) * 8, &rdfds, 0, 0, &DaemonExpiry) > 0) {
	    /* Handle mariner request(s). */
	    if (rdfds & MarinerMask)
		MarinerMux(rdfds);

	    /* Handle worker request. */
	    if (rdfds & KernelMask)
		WorkerMux(rdfds);
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

    LWP_TerminateProcessSupport();
    exit(0);
}

static void ParseCmdline(int argc, char **argv) {
      for(int i = 1; i < argc; i++)
  	if (argv[i][0] == '-') {
 	    if (STREQ(argv[i], "-conffile")) {/* default /etc/coda/venus.conf */
 		i++, venusdotconf = argv[i];
	    } else if (STREQ(argv[i], "-relay")) {   /* default is 127.0.0.1 */
 		i++;
		inet_aton(argv[i], &venus_relay_addr);
 	    } else if (STREQ(argv[i], "-k"))         /* default is /dev/cfs0 */
  		i++, kernDevice = argv[i];
  	    else if (STREQ(argv[i], "-h"))    /* names of file servers */
  		i++, fsname = argv[i];        /* should be italians! */
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
	    else if (STREQ(argv[i], "-r"))     /* name of root volume */
		i++, RootVolName = argv[i];
	    else if (STREQ(argv[i], "-f"))     /* location of cache files */
		i++, CacheDir = argv[i];
	    else if (STREQ(argv[i], "-m"))
		i++, COPModes = atoi(argv[i]);
	    else if (STREQ(argv[i], "-mc"))
		i++, UseMulticast = atoi(argv[i]);
	    else if (STREQ(argv[i], "-maxworkers"))  /* number of worker threads */
		i++, MaxWorkers = atoi(argv[i]);
	    else if (STREQ(argv[i], "-maxworktime")) {  /* max acceptible pause times */
		extern struct timeval cont_sw_threshold;
		cont_sw_threshold.tv_sec = (atoi(argv[++i]));
		cont_sw_threshold.tv_usec = 0;
	    }
            else if (STREQ(argv[i], "-maxrunwait")) {  /* max acceptible lwp run-wait time */
                extern struct timeval run_wait_threshold;
                run_wait_threshold.tv_sec = (atoi(argv[++i]));
                run_wait_threshold.tv_usec = 0;
            }
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
  	    else if (STREQ(argv[i], "-masquerade"))
  		masquerade = 1;
  	    else if (STREQ(argv[i], "-nomasquerade"))
  		masquerade = 0;
	    /* Private mapping ... */
	    else if (STREQ(argv[i], "-mapprivate"))
		MapPrivate = true;
	    else {
		eprint("bad command line option %-4s", argv[i]);
		exit(-1);
	    }
	}
	else
	    venusRoot = argv[i];   /* default is /coda */
}


/* Initialize "general" unset command-line parameters to user specified values
 * or hard-wired defaults. */
/* Note that individual modules initialize their own unset command-line
 * parameters as appropriate. */
static void DefaultCmdlineParms()
{
    int DontUseRVM = 0;

    /* Load the venusdotconf file */
    conf_init(venusdotconf);

    CONF_INT(CacheBlocks,       "cacheblocks",   40000);
    CONF_STR(CacheDir,          "cachedir",      DFLT_CD);
    CONF_STR(SpoolDir,          "checkpointdir", "/usr/coda/spool");
    CONF_STR(VenusLogFile,	"logfile",	 "/usr/coda/etc/venus.log");
    CONF_STR(consoleFile,       "errorlog",      "/usr/coda/etc/console");
    CONF_STR(kernDevice,        "kerneldevice",  "/dev/cfs0");
    CONF_INT(MapPrivate,	"mapprivate",	 0);
    CONF_STR(MarinerSocketPath, "marinersocket", "/usr/coda/spool/mariner");
    CONF_INT(masquerade,	"masquerade",	 0);
    CONF_STR(venusRoot,         "mountpoint",    DFLT_VR);
    CONF_INT(PrimaryUser,       "primaryuser",   UNSET_PRIMARYUSER);
    CONF_STR(fsname,            "rootservers",   "");
    CONF_STR(RootVolName,       "rootvolume",    NULL);
    CONF_STR(VenusLogDevice,    "rvm_log",       "/usr/coda/LOG");
    CONF_STR(VenusDataDevice,   "rvm_data",      "/usr/coda/DATA");

    CONF_INT(DontUseRVM,	"dontuservm",	 0);
    {
	if (DontUseRVM)
	    RvmType = VM;
    }

    CONF_INT(CacheFiles,	"cachefiles",	 0);
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

    CONF_INT(MLEs,		"cml_entries",	 0);
    {
	if (!MLEs) MLEs = CacheBlocks / BLOCKS_PER_MLE;

	if (MLEs < MIN_MLE) {
	    eprint("Cannot start: minimum number of cml entries is %d",MIN_MLE);
	    exit(-1); 
	}
    }

    CONF_INT(HDBEs,		"hoard_entries", 0);
    {
	if (!HDBEs) HDBEs = CacheBlocks / BLOCKS_PER_HDBE;

	if (HDBEs < MIN_HDBE) {
	    eprint("Cannot start: minimum number of hoard entries is %d",
		   MIN_HDBE);
	    exit(-1); 
	}
    }

    CONF_STR(VenusPidFile,	"pid_file",	 NULL);
    {
#define PIDFILE "/pid"
	if (!VenusPidFile) {
	    VenusPidFile = (char *)malloc(strlen(CacheDir)+strlen(PIDFILE)+1);
	    strcpy(VenusPidFile, CacheDir);
	    strcat(VenusPidFile, PIDFILE);
	}
    }

    CONF_STR(VenusControlFile,	"run_control_file",  NULL);
    {
#define CTRLFILE "/VENUS_CTRL"
	if (!VenusControlFile) {
	    VenusControlFile=(char*)malloc(strlen(CacheDir)+strlen(CTRLFILE)+1);
	    strcpy(VenusControlFile, CacheDir);
	    strcat(VenusControlFile, CTRLFILE);
	}
    }

#ifdef moremoremore
    char *x = NULL;
    CONF_STR(x, "relay", NULL, "127.0.0.1");
    inet_aton(x, &venus_relay_addr);
#endif
}


static void CdToCacheDir() {
    if (chdir(CacheDir) < 0) {
	if (errno != ENOENT)
	    { perror("CacheDir chdir"); exit(-1); }
	if (mkdir(CacheDir, 0644) < 0)
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
