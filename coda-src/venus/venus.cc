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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/venus/venus.cc,v 4.8 1997/12/19 23:23:41 braam Exp $";
#endif /*_BLURB_*/







#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/param.h>
#include <stdio.h>
#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif

extern int rpause(int, int, int);  /* why isn't this in sys/resource.h? */
#ifdef __cplusplus
}
#endif __cplusplus

/* interfaces */
#include <vice.h>

/* from venus */
#include "advice.h"
#include "advice_daemon.h"
#include "comm.h"
#include "fso.h"
#include "hdb.h"
#include "local.h"
#include "mariner.h"
#include "sighand.h"
#include "simulate.h"
#include "user.h"
#include "venus.private.h"
#include "venus.version.h"
#include "venuscb.h"
#include "venusrecov.h"
#include "venusvm.h"
#include "venusvol.h"
#include "vproc.h"
#include "vstab.h"
#include "worker.h"


/* *****  Exported variables  ***** */

vproc *Main;
ViceFid	rootfid = {0, 0, 0};
long rootnodeid = 0;
int CleanShutDown = 0;
int SearchForNOreFind = 0;  // Look for better detection method for iterrupted hoard walks. mre 1/18/93

/* Command-line/vstab parameters. */
char *consoleFile = UNSET_CONSOLE;
char *venusRoot = UNSET_VR;
char *kernDevice = UNSET_KD;
char *fsname = UNSET_FS;
char *CacheDir = UNSET_CD;
int CacheBlocks = UNSET_CB;
char *RootVolName = UNSET_RV;
int PrimaryUser = UNSET_PRIMARYUSER;
char *SpoolDir = UNSET_SPOOLDIR;

/* *****  Private constants  ***** */

struct timeval DaemonExpiry = {TIMERINTERVAL, 0};

/* *****  Private routines  ***** */

PRIVATE void ParseCmdline(int, char **);
PRIVATE void DefaultCmdlineParms();
PRIVATE void CdToCacheDir();
PRIVATE void CheckInitFile();
PRIVATE void UnsetInitFile();
PRIVATE void SetRlimits();
/* **** defined in worker.c **** */
extern testKernDevice();



/* *****  venus.c  ***** */

/* local-repair modification */
int main(int argc, char **argv) {
    /* Print to the console -- important during reboot. */
    freopen("/dev/console", "w", stderr);
    fprintf(stderr, "Coda Venus, version %d.%d (%d)\n\r",
	    VenusMajorVersion, VenusMinorVersion, RecovVersionNumber);
    fflush(stderr);

    ParseCmdline(argc, argv);
    DefaultCmdlineParms();   /* read vstab */
    CdToCacheDir(); 
    CheckInitFile();
    SetRlimits();

    /* Initialize.  N.B. order of execution is very important here! */
    /* RecovInit < VSGInit < VolInit < FSOInit < HDB_Init */

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
    SpoolInit();    /* make sure the spooling directory exists */
    DaemonInit();   /* before any Daemons initialize and after LogInit */
    ProfInit();
    StatsInit();
    SimInit();      /* if simulating, open trace file and set filter */
    SigInit();      /* set up signal handlers */
    RecovInit();    /* set up RVM and recov daemon */
    CommInit();     /* set up RPC2, {connection,server,mgroup} lists, probe daemon */
    UserInit();     /* fire up user daemon */
    VSGInit();      /* first alloc of recoverable vm, init VSGDB and daemon */
    VolInit();      /* init VDB, daemon */
    FSOInit();      /* allocate FSDB if necessary, recover FSOs, start FSO daemon */
    HDB_Init();     /* allocate HDB if necessary, scan entries, start the HDB daemon */
    VmonInit();     /* set up Vmon and start Vmon daemon */
    MarinerInit();  /* set up mariner socket */
    WorkerInit();   /* open kernel device */
    CallBackInit(); /* set up callback subsystem and create callback server threads */
    AdviceInit();   /* set up AdSrv and start the advice daemon */
    LRInit();	    /* set up local-repair database */
    VFSMount();

    if (!Simulating) {
	/* Get the Root Volume. */
	while (!GetRootVolume()) {
	    ServerProbe();

	    struct timeval tv;
	    tv.tv_sec = 15;
	    tv.tv_usec = 0;
	    VprocSleep(&tv);
	}
    }

    UnsetInitFile();
    eprint("Venus starting...");

    /* Act as message-multiplexor/daemon-dispatcher. */
    for (;;) {
	if (Simulating) {
	    VprocYield();
	    Simulate();
	}
	else {
	    /* Wait for a message or daemon expiry. */
	    int rdfds = (KernelMask | MarinerMask);
	    if (VprocSelect(NFDS, &rdfds, 0, 0, &DaemonExpiry) > 0) {
		/* Handle mariner request(s). */
		if (rdfds & MarinerMask)
		    MarinerMux(rdfds);

		/* Handle worker request. */
		if (rdfds & KernelMask)
		    WorkerMux(rdfds);
	    }
	}

	/* Fire daemons that are ready to run. */
	DispatchDaemons();
    }

    /* NOTREACHED */
    return(0); /* to pacify g++ */
}


PRIVATE void ParseCmdline(int argc, char **argv) {
    for(int i = 1; i < argc; i++)
	if (argv[i][0] == '-') {
	    if (STREQ(argv[i], "-k"))         /* default is /dev/cfs0 */
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
	    else if (STREQ(argv[i], "-p"))     /* profiling */
		ProfBoot = 1;
	     else if (STREQ(argv[i], "-rdstrace"))     /* RDS heap tracing */
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
		extern long WCThresh;
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
	    else if (STREQ(argv[i], "-mondhost"))
		i++, VmonHost = argv[i];
	    else if (STREQ(argv[i], "-mondportal"))
		i++, VmonPortal = atoi(argv[i]);
	    else if (STREQ(argv[i], "-sim")) {        /* simulator mode */
		Simulating = 1;
		i++, SimInfilename = argv[i];         /* input is a trace */
		i++, SimOutfilename = argv[i];
		i++, SimFilterfilename = argv[i];     /* filter for trace */
		i++, SimAtSys = argv[i];              /* value of @sys for trace */
		i++, SimTmpFid = argv[i];	      /* fid of /tmp, if excluding */
	    }
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
	    else if (STREQ(argv[i], "-noadvice"))
	        AdviceEnabled = 0;
	    else if (STREQ(argv[i], "-noasr"))
	        ASRallowed = 0;
	    else if (STREQ(argv[i], "-novcb"))
	        VCBEnabled = 0;
	    else if (STREQ(argv[i], "-nowalk")) {
	        extern char PeriodicWalksAllowed;
	        PeriodicWalksAllowed = 0;
	    }
	    else if (STREQ(argv[i], "-noRoundRobin")) {
	        extern int RoundRobin;
	        RoundRobin = 0;
	    }
	    else if (STREQ(argv[i], "-RoundRobin")) {
	        extern int RoundRobin;
	        RoundRobin = 1;
	    }
	    else if (STREQ(argv[i], "-noAllowIP")) {
	        extern int AllowIPAddrs;
	        AllowIPAddrs = 0;
	    }
	    else if (STREQ(argv[i], "-spooldir")) {
	        i++, SpoolDir = argv[i];
	    }
	    else {
		eprint("bad command line option %-4s", argv[i]);
		exit(-1);
	    }
	}
	else
	    venusRoot = argv[i];   /* default is /coda */

    /* open the console file and print vital info */
    if (consoleFile == UNSET_CONSOLE) consoleFile = DFLT_CONSOLE;
    if (SpoolDir == UNSET_SPOOLDIR) SpoolDir = DFLT_SPOOLDIR;
    freopen(consoleFile, "w", stderr);
    fprintf(stderr, "Coda Venus, version %d.%d (%d)\n",
             VenusMajorVersion, VenusMinorVersion, RecovVersionNumber);
    fflush(stderr);

    if (Simulating) {
	RvmType = VM;
	COPModes = 0;
    }
}


/* Initialize "general" unset command-line parameters to their vstab values or hard-wired defaults. */
/* Note that individual modules initialize their own unset command-line parameters as appropriate. */
PRIVATE void DefaultCmdlineParms() {
    /* Try vstab first. */
    if (!Simulating) {
	struct vstab *v = getvsent();
	if (v) {
	    if (venusRoot == UNSET_VR) venusRoot = v->v_dir;
	    if (kernDevice == UNSET_KD) kernDevice = v->v_dev;
	    if (fsname == UNSET_FS) fsname = v->v_host;
	    if (CacheDir == UNSET_CD) CacheDir = v->v_cache;
	    if (CacheBlocks == UNSET_CB) CacheBlocks = v->v_cachesize;
	}
    }

    /* Use hard-wired defaults otherwise. */
    {
	if (venusRoot == UNSET_VR) venusRoot = DFLT_VR;
	if (kernDevice == UNSET_KD) kernDevice = DFLT_KD;
	if (fsname == UNSET_FS) fsname = DFLT_FS;
	if (CacheDir == UNSET_CD) CacheDir = DFLT_CD;
	if (CacheBlocks == UNSET_CB) CacheBlocks = DFLT_CB;
    }
}


PRIVATE void CdToCacheDir() {
    if (Simulating) return;

    if (chdir(CacheDir) < 0) {
	if (errno != ENOENT)
	    { perror("CacheDir chdir"); exit(-1); }
	if (mkdir(CacheDir, 0644) < 0)
	    { perror("CacheDir mkdir"); exit(-1); }
	if (chdir(CacheDir) < 0)
	    { perror("CacheDir chdir"); exit(-1); }
    }
}

PRIVATE void CheckInitFile() {
    char initPath[MAXPATHLEN];
    struct stat tstat;

    if (Simulating) return;

    /* Construct name for INIT file */
    snprintf(initPath, MAXPATHLEN, "%s/INIT", CacheDir);

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

PRIVATE void UnsetInitFile() {
    char initPath[MAXPATHLEN];

    if (Simulating) return;

    /* Create the file, if it doesn't already exist */
    snprintf(initPath, MAXPATHLEN, "%s/INIT", CacheDir);
    unlink(initPath);
}


PRIVATE void SetRlimits() {
    /* Set DATA segment limit to maximum allowable. */
    struct rlimit rl;
    if (getrlimit(RLIMIT_DATA, &rl) < 0)
	{ perror("getrlimit"); exit(-1); }
    rl.rlim_cur = rl.rlim_max;
    if (setrlimit(RLIMIT_DATA, &rl) < 0)
	{ perror("setrlimit"); exit(-1); }

#ifdef	CMU
    /* 
     * Turn off resource-pausing on ENOSPC. Otherwise, Venus
     * will hang if its cache partition becomes full while 
     * dumping a checkpoint file.  Standard BSD kernels don't
     * support resource pausing.
     */
    if (rpause(ENOSPC, RPAUSE_ALL, RPAUSE_DISABLE) < 0)
	{ perror("rpause"); exit(-1); }
#endif	CMU
}


/*  *****  Should be in a library!  *****  */

/* This uses statics!  It must NOT be called more than ONCE! */
#define MAXVSTABLINE 2000
struct vstab *getvsent() {
    static struct vstab v;
    static char buf[MAXVSTABLINE + 1];

    /* Open the vstab and read in a single line. */
    FILE *fp = fopen(VSTAB, "r");
    if (fp == NULL) return(0);
    if (fgets(buf, MAXVSTABLINE, fp) == NULL) {
	eprint("getvsent: fgets(%s) failed\n", VSTAB);
	fclose(fp); 
	return(0);
    }
    fclose(fp);

    /* Temporaries for parsing the vsent. */
    int c = 0;
    char *s = buf;
    char *t = 0;

    /* VenusRoot */
    for (t = s; *s && *s != ':'; s++) ; *s++ = '\0';
    v.v_dir = t;

    /* KernelDevice */
    for (t = s; *s && *s != ':'; s++) ; *s++ = '\0';
    v.v_dev = t;

    /* FileServers */
    for (t = s; *s && *s != ':'; s++) ; *s++ = '\0';
    v.v_host = t;

    /* CacheDirectory */
    for (t = s; *s && *s != ':'; s++) ; *s++ = '\0';
    v.v_cache = t;

    /* CacheBlocks */
    for (t = s; *s && *s != ':'; s++) ; *s++ = '\0';
    v.v_cachesize = atoi(t);

    /* Flags */
    for (t = s; *s && *s != ':'; s++) ; *s++ = '\0';
    v.v_checkint = atoi(t);

    return(&v);
}    


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

/*
 * SFTP needs this declared but Venus doesn't actually use it.
 * N.B. If Venus ever uses the FILEBYINODE transfer option this
 * will have to be changed!
 */
iopen(int dev, int inode_number, int flag)
{
    assert(0);
}

#ifdef __cplusplus
}
#endif __cplusplus

