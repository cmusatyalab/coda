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

#*/

/*
                         IBM COPYRIGHT NOTICE

                          Copyright (C) 1986
             International Business Machines Corporation
                         All Rights Reserved

This  file  contains  some  code identical to or derived from the 1986
version of the Andrew File System ("AFS"), which is owned by  the  IBM
Corporation.   This  code is provided "AS IS" and IBM does not warrant
that it is free of infringement of  any  intellectual  rights  of  any
third  party.    IBM  disclaims  liability of any kind for any damages
whatsoever resulting directly or indirectly from use of this  software
or  of  any  derivative work.  Carnegie Mellon University has obtained
permission to  modify,  distribute and sublicense this code,  which is
based on Version 2  of  AFS  and  does  not  contain  the features and
enhancements that are part of  Version 3 of  AFS.  Version 3 of AFS is
commercially   available   and  supported  by   Transarc  Corporation,
Pittsburgh, PA.

*/


/************************************************************************/
/*									*/
/*  file.c	- File Server main loop					*/
/*									*/
/************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/file.h>
#include "coda_string.h"
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include "coda_flock.h"
#include "codaconf.h"

#include <ports.h>
#include <lwp/lwp.h>
#include <lwp/timer.h>
#include <rpc2/rpc2.h>
#include <rpc2/sftp.h>
#include <rpc2/fail.h>
#include <rpc2/fcon.h>
#include <partition.h>
#include <util.h>
#include <rvmlib.h>
#include <resolution.h>

extern int nice(int);
extern int Fcon_Init(); 

#ifdef _TIMECALLS_
#include <histo.h>
#endif

#include <rvm/rvm_statistics.h>

#include <prs.h>
#include <al.h>

#include <auth2.h>
#include <res.h>
#include <vice.h>
#include <volutil.h>
#include <codadir.h>
#include <avice.h>

#ifdef __cplusplus
}
#endif

#include <volume.h>
#include <srv.h>
#include <vice.private.h>
#include <recov.h>
#include <camprivate.h>
#include <coda_globals.h>
#include <vrdb.h>
#include <rescomm.h>
#include <lockqueue.h>
#include "coppend.h"


/* *****  Exported variables  ***** */

int SystemId, AnyUserId;
unsigned StartTime;
int CurrentConnections;
int Counters[MAXCNTRS];
const ViceFid NullFid = { 0, 0, 0 };
const int MaxVols = MAXVOLS;          /* so we can use it in vicecb.c. yuck. */

/* defaults set by ReadConfigFile() */
int probingon;			// default 0
int optimizationson;		// default 0
int Authenticate;		// default 1
int AllowResolution;		// default 1, controls directory resolution 
int AllowSHA;			// default 0, whether we calculate SHA checksums
int comparedirreps;		// default 1 
int pathtiming;			// default 0 
int pollandyield;		// default 1 
char *CodaSrvIp;		// default NULL ('ipaddress' in server.conf)

/* local */
static int MapPrivate;		// default 0

/* imported */
extern rvm_length_t rvm_test;

#ifdef _TIMECALLS_
int clockFD = 0;		/* for timing with the NSC clock board */
struct hgram Create_Total_hg, Remove_Total_hg, Link_Total_hg;
struct hgram Rename_Total_hg, MakeDir_Total_hg, RemoveDir_Total_hg, SymLink_Total_hg;
struct hgram SpoolVMLogRecord_hg, PutObjects_Transaction_hg, PutObjects_TransactionEnd_hg,
	PutObjects_Inodes_hg, PutObjects_RVM_hg; 
#endif /* _TIMECALLS_ */

int large = 0;	  // default 500, control size of lru cache for large vnodes 
int small = 0;	  // default 500, control size of lru cache for small vnodes

extern char *SmonHost;
extern int SmonPort; 

#ifdef PERFORMANCE
/* added array of thread id's for thread_info, Puneet */
#define	NLWPS	5
thread_t lwpth[NLWPS];
thread_array_t thread_list;
int thread_count;
#endif /* PERFORMANCE */


/* *****  Private variables  ***** */

static int ServerNumber = 0;	/* 0 => single server,
				   1,2,3 => number in multi server */

/* File server parameters.   Defaults set by ReadConfigFile. */

static char *vicedir;		// default "/vice"
static char *srvhost;		// default NULL

static int trace = 0;		// default 0 
static int SrvWindowSize = 0;	// default 32
static int SrvSendAhead = 0;	// default 8
static int timeout = 0;		// default 15, formerly 30, then 60
static int retrycnt = 0;	// default 4, formerly 20, then 6
static int debuglevel = 0;	// Command line set only.
static int lwps = 0;		// default 6
static int buffs = 0;		// default 100, formerly 200
       int stack = 0;		// default 96
static int cbwait = 0;		// default 240
static int chk = 0;		// default 30
static int ForceSalvage = 0;	// default 1
static int SalvageOnShutdown = 0; // default 0 */

static int ViceShutDown = 0;
static int Statistics;

/* static int ProcSize = 0; */

/* Camelot/RVM stuff. */
struct camlib_recoverable_segment *camlibRecoverableSegment;

/*static */char *_Rvm_Log_Device;
/*static */char *_Rvm_Data_Device;
/*static */rvm_offset_t _Rvm_DataLength;
/*static */int _Rvm_Truncate = 0;	// default 0 
/*static */char *cam_log_file;
/*static */int camlog_fd;
/*static */char camlog_record[SIZEOF_LARGEDISKVNODE + 8 + sizeof(VolumeDiskData)];
/* static */ char *_DEBUG_p;
/*static */int DumpVM = 0;
int prottrunc = FALSE;
/* static */int MallocTrace = FALSE;
/* static */void rds_printer(char *fmt ...);

/* vicetab */
char *vicetab = NULL;	       /* default db/vicetab */

/* PDB stuff. */
static int pdbtime = 0;
#define CODADB vice_sharedfile("db/prot_users.db")

/* Token stuff. */
static int keytime = 0;
#define KEY1 vice_sharedfile("db/auth2.tk")
#define KEY2 vice_sharedfile("db/auth2.tk.BAK")

/* (Worker) LWP statistics.  Currently unused. */
#define MAXLWP 16
static RPC2_Integer LastOp[MAXLWP];
static int StackUsed[MAXLWP];
static int StackAllocated[MAXLWP];
static ClientEntry *CurrentClient[MAXLWP];

/* *****  Private routines  ***** */

static void ServerLWP(int *);
static void ResLWP(int *);
static void CallBackCheckLWP();
static void CheckLWP();

static void ClearCounters();
static void FileMsg();
static void SetDebug();
static void ResetDebug();
static void ShutDown();

static int ReadConfigFile(void);
static int ParseArgs(int, char **);
static void InitServerKeys(char *, char *);
static void DaemonizeSrv(void);
static void InitializeServerRVM(char *name);

#ifdef RVMTESTING
#include <rvmtesting.h>
#endif

/* Signal handlers in Linux will not be passed the arguments code and scp */
#ifdef __BSD44__
struct sigcontext OldContext; /* zombie() saves original context here */
#endif
extern void dumpvm();


/* Signal handlers in Linux will not be passed the arguments code and scp */
#ifndef	__BSD44__
void zombie(int sig) {
#else
void zombie(int sig, int code, struct sigcontext *scp) {
    memmove((void *)&OldContext, (const void *)scp, sizeof(struct sigcontext));
#endif

#ifndef  __BSD44__
    SLog(0,  "****** FILE SERVER INTERRUPTED BY SIGNAL %d ******", sig);
#else
    SLog(0,  "****** FILE SERVER INTERRUPTED BY SIGNAL %d CODE %d ******", sig, code);
#endif    
    SLog(0,  "****** Aborting outstanding transactions, stand by...");
    /* leave a sign to prevent automatic restart, ignore failures */
    creat("CRASH", 00600);
    /* Abort all transactions before suspending... */
    if (RvmType == RAWIO || RvmType == UFS) {
	rvm_options_t curopts;
	int i;
	rvm_return_t ret;
	      
	rvm_init_options(&curopts);
	ret = rvm_query(&curopts, NULL);
	if (ret != RVM_SUCCESS)
	    SLog(0,  "rvm_query returned %s", rvm_return(ret));	
	else {
	    SLog(0,  "Uncommitted transactions: %d", curopts.n_uncommit);
	
	    for (i = 0; i < curopts.n_uncommit; i++) {
		rvm_abort_transaction(&(curopts.tid_array[i]));
		if (ret != RVM_SUCCESS) 
		    SLog(0,  "ERROR: abort failed, code: %s", rvm_return(ret));
	    }
	    
	    ret = rvm_query(&curopts, NULL);
	    if (ret != RVM_SUCCESS)
		SLog(0,  "rvm_query returned %s", rvm_return(ret));	
	    else 
		SLog(0,  "Uncommitted transactions: %d", curopts.n_uncommit);
	}
	rvm_free_options(&curopts);

	if (DumpVM)
	    dumpvm(); /* sanity check rvm recovery. */
    }
    
    SLog(0, "Becoming a zombie now ........");
    SLog(0, "You may use gdb to attach to %d", getpid());
    {
	int      living_dead = 1;
	sigset_t mask;

	sigemptyset(&mask);
	CODA_ASSERT(0);
	while (living_dead) {
	    sigsuspend(&mask); /* pending gdb attach */
	}
    }
}





/* The real stuff! */

int main(int argc, char *argv[])
{
    char    sname[20];
    int     i;
    FILE   *file;
    struct stat buff;
    PROCESS parentPid, serverPid, resPid, smonPid, resworkerPid;
    RPC2_PortIdent port1, *portlist[1];
    RPC2_SubsysIdent server;
    SFTP_Initializer sei;
    ProgramType *pt;

    coda_assert_action = CODA_ASSERT_EXIT;

    if(ParseArgs(argc,argv)) {
	SLog(0, "usage: srv [-d (debug level)] [-p (number of processes)] ");
	SLog(0, "[-b (buffers)] [-l (large vnodes)] [-s (small vnodes)]");
	SLog(0, "[-k (stack size)] [-w (call back wait interval)]");
	SLog(0, "[-r (RPC retry count)] [-o (RPC timeout value)]");
	SLog(0, "[-c (check interval)] [-t (number of RPC trace buffers)]");
	SLog(0, "[-noauth] [-forcesalvage] [-quicksalvage]");
	SLog(0, "[-cp (connections in process)] [-cm (connections max)");
	SLog(0, "[-cam] [-nc] [-rvm logdevice datadevice length] [-nores] [-trunc percent]");
	SLog(0, " [-nocmp] [-nopy] [-dumpvm] [-nosalvageonshutdown] [-mondhost hostname] [-mondport portnumber]");
	SLog(0, "[-nodebarrenize] [-dir workdir] [-srvhost host]");
	SLog(0, " [-rvmopt] [-usenscclock]");
	SLog(0, " [-nowriteback] [-mapprivate] [-zombify]");

	exit(-1);
    }

    (void)ReadConfigFile();

    if(chdir(vice_file("srv"))) {
	SLog(0, "could not cd to %s - exiting", vice_file("srv"));
	exit(-1);
    }

    unlink("NEWSRV");

    SwapLog();
    DaemonizeSrv();

    /* CamHistoInit(); */	
    /* Initialize CamHisto package */
#ifdef _TIMECALLS_
    InitHisto(&Create_Total_hg, (double)0, (double)100000, 1000, LINEAR);
    InitHisto(&Remove_Total_hg, (double)0, (double)100000, 1000, LINEAR);
    InitHisto(&Link_Total_hg, (double)0, (double)100000, 1000, LINEAR);
    InitHisto(&Rename_Total_hg, (double)0, (double)100000, 1000, LINEAR);
    InitHisto(&MakeDir_Total_hg, (double)0, (double)100000, 1000, LINEAR);
    InitHisto(&RemoveDir_Total_hg, (double)0,(double)100000, 1000, LINEAR);
    InitHisto(&SymLink_Total_hg, (double)0, (double)100000, 1000, LINEAR);
    InitHisto(&SpoolVMLogRecord_hg, (double)0, (double)50000, 500, LINEAR);
    InitHisto(&PutObjects_Inodes_hg, (double)0, (double)50000, 500, LINEAR);
    InitHisto(&PutObjects_RVM_hg, (double)0, (double)50000, 500, LINEAR);
    InitHisto(&PutObjects_Transaction_hg, (double)0, (double)50000, 500, LINEAR);
    InitHisto(&PutObjects_TransactionEnd_hg, (double)0, (double)50000, 500, LINEAR);
#endif /* _TIMECALLS_ */

    /* open the file where records are written at end of transaction */
    if (cam_log_file){
	camlog_fd = open(cam_log_file, O_RDWR | O_TRUNC | O_CREAT, 0777);
	if (camlog_fd < 0) perror("Error opening cam_log_file\n");
    }

    VInitServerList(srvhost);	/* initialize server info for volume pkg */

    switch (RvmType) {
        case UFS	   :
 	case RAWIO     	   : SLog(0, "RvmType is Rvm"); break;
	case VM		   : SLog(0, "RvmType is NoPersistence"); break;
	case UNSET	   : SLog(0, "No RvmType selected!"); exit(-1);
    }

    /* Initialize the hosttable structure */
    CLIENT_InitHostTable();

    SLog(0, "Main process doing a LWP_Init()");
    CODA_ASSERT(LWP_Init(LWP_VERSION, LWP_NORMAL_PRIORITY,&parentPid)==LWP_SUCCESS);

    /* using rvm - so set the per thread data structure for executing
       transactions */
    rvm_perthread_t rvmptt;
    if (RvmType == RAWIO || RvmType == UFS) {
	rvmptt.tid = NULL;
	rvmptt.list.table = NULL;
	rvmptt.list.count = 0;
	rvmptt.list.size = 0;
	rvmlib_init_threaddata(&rvmptt);
	CODA_ASSERT(rvmlib_thread_data() != 0);
	SLog(0, 
	       "Main thread just did a RVM_SET_THREAD_DATA\n");
    }
    InitializeServerRVM("codaserver"); 

    /* Trace mallocs and frees in the persistent heap if requested. */
    if (MallocTrace) {	
      rds_trace_on(stdout);
      rds_trace_dump_heap();
    }
    
    coda_init();

#ifdef PERFORMANCE 
    /* initialize the array of thread_t to 0 - Puneet */
    for (i = 0; i < NLWPS; i ++)
	lwpth[i] = (thread_t)0;
#endif /* PERFORMANCE */

    stat(KEY1, &buff);
    keytime = (int)buff.st_mtime;
    InitServerKeys(KEY1, KEY2);

#ifdef __CYGWIN32__
    /* XXX -JJK */
    port1.Tag = RPC2_PORTBYINETNUMBER;
    port1.Value.InetPortNumber = htons(PORT_codasrv);
#else
    port1.Tag = RPC2_PORTBYNAME;
    strcpy(port1.Value.Name, "codasrv");
#endif
    portlist[0] = &port1;

    if (srvhost) {
	struct in_addr ip;
	if (CodaSrvIp) {
	    CODA_ASSERT(inet_aton(CodaSrvIp, &ip) != 0);
	} else {
	    struct hostent *he;
	    he = gethostbyname(srvhost);
	    CODA_ASSERT(he && he->h_length == sizeof(struct in_addr));
	    memcpy(&ip, he->h_addr_list[0], sizeof(struct in_addr));
	}
	RPC2_setip(&ip);
    }
    SFTP_SetDefaults(&sei);
    /* set optimal window size and send ahead parameters */
    sei.WindowSize = SrvWindowSize;
    sei.AckPoint = sei.SendAhead = SrvSendAhead;
    sei.EnforceQuota = 1;
    sei.Port.Tag = RPC2_PORTBYINETNUMBER;
    sei.Port.Value.InetPortNumber = htons(PORT_codasrvse);
    SFTP_Activate(&sei);
    struct timeval to;
    to.tv_sec = timeout;
    to.tv_usec = 0;
    CODA_ASSERT(RPC2_Init(RPC2_VERSION, 0, &port1, retrycnt, &to) == RPC2_SUCCESS);
    RPC2_InitTraceBuffer(trace);
    RPC2_Trace = trace;

    DP_Init(vicetab, srvhost);
    DIR_Init(DIR_DATA_IN_VM);
    DC_HashInit();

    FileMsg();


    CODA_ASSERT(file = fopen("pid","w"));
    fprintf(file,"%d",(int)getpid());
    fclose(file);
    /* Init per LWP process functions */
    for(i=0;i<MAXLWP;i++)
	LastOp[i] = StackAllocated[i] = StackUsed[i] = 0;
    AL_DebugLevel = SrvDebugLevel/10;
    VolDebugLevel = SrvDebugLevel;
    RPC2_Perror = 1;
    nice(-5);
    DIR_Init(DIR_DATA_IN_VM);

    stat(CODADB, &buff);
    pdbtime = (int)buff.st_mtime;
    CODA_ASSERT(AL_Initialize(AL_VERSION) == 0);
    if (AL_NameToId(PRS_ADMINGROUP, &SystemId) ||
	AL_NameToId(PRS_ANYUSERGROUP, &AnyUserId)) {
	SLog(0, "Failed to find '" PRS_ADMINGROUP "' or '" PRS_ANYUSERGROUP
	        "' in the pdb database.");
	CODA_ASSERT(0 && "check pdb database");
    }

 /* Initialize failure package */
#ifdef USE_FAIL_FILTERS
    Fail_Initialize("file", 0);
    Fcon_Init();
#endif

    /* tag main fileserver lwp for volume package */
    pt = (ProgramType *) malloc(sizeof(ProgramType));
    *pt = fileServer;
    CODA_ASSERT(LWP_NewRock(FSTAG, (char *)pt) == LWP_SUCCESS);

    InitCallBack();
    CheckVRDB();
    VInitVolumePackage(large,small, ForceSalvage);


    InitCopPendingTable();

    /* Initialize the lock queue and the resolution comm package */
    InitLockQueue();
    ResCommInit();
    server.Tag = RPC2_SUBSYSBYID;
    server.Value.SubsysId = RESOLUTIONSUBSYSID;
    CODA_ASSERT(RPC2_Export(&server) == RPC2_SUCCESS);

    server.Tag = RPC2_SUBSYSBYID;
    server.Value.SubsysId = SUBSYS_SRV;
    CODA_ASSERT(RPC2_Export(&server) == RPC2_SUCCESS);
    ClearCounters();

    CODA_ASSERT(LWP_CreateProcess((PFIC)CallBackCheckLWP, stack*1024, LWP_NORMAL_PRIORITY,
	    (char *)&cbwait, "CheckCallBack", &serverPid) == LWP_SUCCESS);
	    
    CODA_ASSERT(LWP_CreateProcess((PFIC)CheckLWP, stack*1024, LWP_NORMAL_PRIORITY,
	    (char *)&chk, "Check", &serverPid) == LWP_SUCCESS);
	    
    for (i=0; i < lwps; i++) {
	sprintf(sname, "ServerLWP-%d",i);
	CODA_ASSERT(LWP_CreateProcess((PFIC)ServerLWP, stack*1024, LWP_NORMAL_PRIORITY,
		(char *)&i, sname, &serverPid) == LWP_SUCCESS);
    }

    /* set up resolution threads */
    for (i = 0; i < 2; i++){
	sprintf(sname, "ResLWP-%d", i);
	CODA_ASSERT(LWP_CreateProcess((PFIC)ResLWP, stack*1024, 
				 LWP_NORMAL_PRIORITY, (char *)&i, 
				 sname, &resPid) == LWP_SUCCESS);
    }
    sprintf(sname, "ResCheckSrvrLWP");
    CODA_ASSERT(LWP_CreateProcess((PFIC)ResCheckServerLWP, stack*1024,
			      LWP_NORMAL_PRIORITY, (char *)&i, 
			      sname, &resPid) == LWP_SUCCESS);

    sprintf(sname, "ResCheckSrvrLWP_worker");
    CODA_ASSERT(LWP_CreateProcess((PFIC)ResCheckServerLWP_worker, stack*1024,
			      LWP_NORMAL_PRIORITY, (char *)&i, 
			      sname, &resworkerPid) == LWP_SUCCESS);
    /* Set up volume utility subsystem (spawns 2 lwps) */
    SLog(29, "fileserver: calling InitvolUtil");
    extern void InitVolUtil(int stacksize);

    InitVolUtil(stack*1024);
    SLog(29, "fileserver: returning from InitvolUtil");

    extern void SmonDaemon();
    sprintf(sname, "SmonDaemon");
    CODA_ASSERT(LWP_CreateProcess((PFIC)SmonDaemon, stack*1024,
			     LWP_NORMAL_PRIORITY, (char *)&smonPid,
			     sname, &smonPid) == LWP_SUCCESS);
#ifdef PERFORMANCE
#ifndef OLDLWP
    /* initialize global array of thread_t for timing - Puneet */
    if (task_threads(task_self(), &thread_list, &thread_count) != KERN_SUCCESS)
	SLog(0, "*****Couldn't get threads for task ");
    else
	SLog(0, "Thread ids for %d threads initialized", thread_count);
#endif
#endif /* PERFORMANCE */
    struct timeval tp;
    struct timezone tsp;
    TM_GetTimeOfDay(&tp, &tsp);
    SLog(0,"File Server started %s", ctime((const time_t *)&tp.tv_sec));
    StartTime = (unsigned int)tp.tv_sec;
    CODA_ASSERT(LWP_WaitProcess((char *)&parentPid) == LWP_SUCCESS);
    exit(0);
}


#define BADCLIENT 1000
static void ServerLWP(int *Ident)
{
    RPC2_RequestFilter myfilter;
    RPC2_PacketBuffer * myrequest;
    RPC2_Handle mycid;
    RPC2_Integer opcode;
    long    rc;
    int     lwpid;
    ClientEntry *client = 0;
    ProgramType *pt;

    /* using rvm - so set the per thread data structure */
    rvm_perthread_t rvmptt;
    rvmlib_init_threaddata(&rvmptt);
    SLog(0, "ServerLWP %d just did a rvmlib_set_thread_data()\n", *Ident);

    myfilter.FromWhom = ONESUBSYS;
    myfilter.OldOrNew = OLDORNEW;
    myfilter.ConnOrSubsys.SubsysId = SUBSYS_SRV;
    lwpid = *Ident;
    SLog(1, "Starting Worker %d", lwpid);

    /* tag fileserver lwps with rock */
    pt = (ProgramType *) malloc(sizeof(ProgramType));
    *pt = fileServer;
    CODA_ASSERT(LWP_NewRock(FSTAG, (char *)pt) == LWP_SUCCESS);

    while (1) {
	LastOp[lwpid] = 0;
	CurrentClient[lwpid] = (ClientEntry *)0;
	if ((rc = RPC2_GetRequest(&myfilter, &mycid, &myrequest, 0, GetKeysFromToken, RPC2_XOR, NULL))
		== RPC2_SUCCESS) {
	    if (RPC2_GetPrivatePointer(mycid, (char **)&client) != RPC2_SUCCESS) 
		client = 0;
	    
	    if (client) {
		/* check rpc2 connection handle */
		if (client->RPCid != mycid) {
		    SLog(0, "Invalid client pointer from GetPrivatePointer");
		    myrequest->Header.Opcode = BADCLIENT; /* ignore request & Unbind */
		    client = 0;
		}
		/* check token expiry, compare the token expiry time to the
		 * receive timestamp in the PacketBuffer. */
		if (client->SecurityLevel != RPC2_OPENKIMONO &&
		    myrequest->Prefix.RecvStamp.tv_sec > client->Token.EndTimestamp) {
		    SLog(0, "Client Token expired");
		    /* force a disconnection for this rpc2 connection */
		    myrequest->Header.Opcode = TokenExpired_OP;
		}
	    }
	    LastOp[lwpid] = myrequest->Header.Opcode;
	    CurrentClient[lwpid] = client;
	    if (client) {
		client->LastCall = client->VenusId->LastCall = (unsigned int)time(0);
		/* the next time is used to eliminate GetTime calls from active stat */
		if (myrequest->Header.Opcode != GETTIME)
		    client->VenusId->ActiveCall = client->LastCall;
		client->LastOp = (int)myrequest->Header.Opcode;
     	    }

	    SLog(5, "Worker %d received request %d on cid %d for %s at %s",
		    lwpid, myrequest->Header.Opcode, mycid,
		    client ? client->UserName : "NA",
		    client ? inet_ntoa(client->VenusId->host) : "NA");
	    if (myrequest->Header.Opcode > 0 && myrequest->Header.Opcode < FETCHDATA) {
		Counters[TOTAL]++;
		Counters[myrequest->Header.Opcode]++;
		if (!(Counters[TOTAL] & 0xFFF)) {
		    PrintCounters(stdout);
		}
	    }

            /* save fields we need, ExecuteRequest will thrash the buffer */
            opcode = myrequest->Header.Opcode;

	    rc = srv_ExecuteRequest(mycid, myrequest, 0);

	    if (rc) {
		SLog(0, "srv.c request %d for %s at %s failed: %s",
			opcode, client ? client->UserName : "NA",
			client ? inet_ntoa(client->VenusId->host) : "NA",
			ViceErrorMsg((int)rc));
		if(rc <= RPC2_ELIMIT) {
		    if(client && client->RPCid == mycid && !client->DoUnbind) {
			ObtainWriteLock(&(client->VenusId->lock));
			CLIENT_Delete(client);
			ReleaseWriteLock(&(client->VenusId->lock));
		    }
		}
	    }
	    if(client) {
		if(client->DoUnbind) {
		    SLog(0, "Worker%d: Unbinding RPC connection %d",
							    lwpid, mycid);
		    RPC2_Unbind(mycid);
		    AL_FreeCPS(&(client->CPS));
		    free((char *)client);
		    client = 0;
		}
		else {
		    client->LastOp = 0;
		}
	    }
            /* if bad client pointer Unbind the rpc connection */
	    if (opcode == BADCLIENT) {
		SLog(0, "Worker%d: Unbinding RPC connection %d (BADCLIENT)",
					    lwpid, mycid);
		RPC2_Unbind(mycid);
	    }
	}
	else {
	    SLog(0,"RPC2_GetRequest failed with %s",ViceErrorMsg((int)rc));
	}
    }
}

static void ResLWP(int *Ident){
    RPC2_RequestFilter myfilter;
    RPC2_Handle	mycid;
    RPC2_PacketBuffer *myrequest;
    ProgramType pt;
    register long rc;
    rvm_perthread_t rvmptt;

    /* using rvm - so set the per thread data structure for executing
       transactions */
    rvmlib_init_threaddata(&rvmptt);
    SLog(0, "ResLWP-%d just did a rvmlib_set_thread_data()\n", *Ident);

    pt = fileServer;
    CODA_ASSERT(LWP_NewRock(FSTAG, (char *)&pt) == LWP_SUCCESS);

    myfilter.FromWhom = ONESUBSYS;
    myfilter.OldOrNew = OLDORNEW;
    myfilter.ConnOrSubsys.SubsysId = RESOLUTIONSUBSYSID;
    SLog(1, "Starting ResLWP worker %d", *Ident);

    while(1) {
	mycid = 0;
	rc = RPC2_GetRequest(&myfilter, &mycid, &myrequest, 
			     NULL, NULL, 0, NULL);
	if (rc == RPC2_SUCCESS) {
	    SLog(9, "ResLWP %d Received request %d", 
		    *Ident, myrequest->Header.Opcode);
	    rc = resolution_ExecuteRequest(mycid, myrequest, NULL);
	    if (rc) 
		SLog(0, "ResLWP %d: request %d failed with %s",
			*Ident, ViceErrorMsg((int)rc));
	}
	else 
	    SLog(0, "RPC2_GetRequest failed with %s", 
		    ViceErrorMsg((int)rc));
    }

}

static void CallBackCheckLWP()
{
    struct timeval  time;
    ProgramType *pt;
    rvm_perthread_t rvmptt;

    /* tag lwps as fsUtilities */
    pt = (ProgramType *) malloc(sizeof(ProgramType));
    *pt = fsUtility;
    CODA_ASSERT(LWP_NewRock(FSTAG, (char *)pt) == LWP_SUCCESS);

    /* using rvm - so set the per thread data structure for executing
       transactions */
    rvmlib_init_threaddata(&rvmptt);
    SLog(0, "CallBackCheckLWP just did a rvmlib_set_thread_data()\n");

    SLog(1, "Starting CallBackCheck process");
    time.tv_sec = cbwait;
    time.tv_usec = 0;

    while (1) {
	if (IOMGR_Select(0, 0, 0, 0, &time) == 0) {
	    SLog(2, "Checking for dead venii");
	    CLIENT_CallBackCheck();
	    SLog(2, "Set disk usage statistics");
	    VSetDiskUsage();
	    if(time.tv_sec != cbwait) time.tv_sec = cbwait;
	}
    }
}


static void CheckLWP()
{
    struct timeval  time;
    struct timeval  tpl;
    struct timezone tspl;
    ProgramType *pt;
    rvm_perthread_t rvmptt;

    /* using rvm - so set the per thread data structure for executing
       transactions */
    rvmlib_init_threaddata(&rvmptt);
    SLog(0, "CheckLWP just did a rvmlib_set_thread_data()\n");

    
    /* tag lwps as fsUtilities */
    pt = (ProgramType *) malloc(sizeof(ProgramType));
    *pt = fsUtility;
    CODA_ASSERT(LWP_NewRock(FSTAG, (char *)pt) == LWP_SUCCESS);


    SLog(1, "Starting Check process");
    time.tv_sec = chk;
    time.tv_usec = 0;

    while (1) {
	if (IOMGR_Select(0, 0, 0, 0, &time) == 0) {
	    if(ViceShutDown) {
		ProgramType *pt, tmp_pt;

		TM_GetTimeOfDay(&tpl, &tspl);
		SLog(0, "Shutting down the File Server %s",
		     ctime((const time_t *)&tpl.tv_sec));
		CODA_ASSERT(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);
		/* masquerade as fileServer lwp */
		tmp_pt = *pt;
		*pt = fileServer;
	    	ShutDown();
		*pt = tmp_pt;
	    }

	    if(time.tv_sec != chk) time.tv_sec = chk;
	  }
    }
}

static void ShutDown()
{
    int     fd;

    PrintCounters(stdout);

    if (SalvageOnShutdown) {
	SLog(9, "Unlocking volutil lock...");
	int fvlock = open(vice_file("volutil.lock"), O_CREAT|O_RDWR, 0666);
	CODA_ASSERT(fvlock >= 0);
	while (myflock(fvlock, MYFLOCK_UN, MYFLOCK_BL) != 0);
	SLog(9, "Done");
	close(fvlock);
	
	ProgramType *pt, tmppt;
	CODA_ASSERT(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);
	tmppt = *pt;
	*pt = salvager;	/* MUST set *pt to salvager before vol_salvage */
	CODA_ASSERT(S_VolSalvage(0, NULL, 0, 1, 1, 0) == 0);
	*pt = tmppt;
    }

    VShutdown();

    fd = open("SHUTDOWN",O_CREAT+O_RDWR, 0666);
    close(fd);
    exit(0);
}




/*
  BEGIN_HTML
  <a name="ViceUpdateDB"><strong>Ensure the incore copy of the databases is up to date.
  </strong></a> 
  END_HTML
*/
void ViceUpdateDB()
{
    struct stat vbuff;

    stat(CODADB, &vbuff);

    if(pdbtime != vbuff.st_mtime) {
	pdbtime = (int)vbuff.st_mtime;
	CODA_ASSERT(AL_Initialize(AL_VERSION) == 0);
    }
    stat(KEY1, &vbuff);
    if(keytime != vbuff.st_mtime) {
	keytime = (int)vbuff.st_mtime;
	InitServerKeys(KEY1,KEY2);
    }
    VCheckVLDB();
    CheckVRDB();
    SLog(0, "New Data Base received");

}


static void ClearCounters()
{
    register	int	i;

    for(i=0; i<MAXCNTRS; i++)
        Counters[i] = 0;
}


void PrintCounters(FILE *fp)
{
    struct timeval  tpl;
    struct timezone tspl;
    int seconds;
    int workstations;
    int activeworkstations;

    TM_GetTimeOfDay(&tpl, &tspl);
    Statistics = 1;
    SLog(0, "Total operations for File Server = %d : time = %s",
	    Counters[TOTAL], ctime((const time_t *)&tpl.tv_sec));
    SLog(0, "Vice was last started at %s", ctime((const time_t *)&StartTime));

    SLog(0, "NewConnectFS %d", Counters[NEWCONNECTFS]);
    SLog(0, "DisconnectFS %d", Counters[DISCONNECT]);

    SLog(0, "GetAttr %d", Counters[GETATTR]);
    SLog(0, "GetAcl %d", Counters[GETACL]);
    SLog(0, "Fetch %d", Counters[FETCH]);
    SLog(0, "SetAttr %d", Counters[SETATTR]);
    SLog(0, "SetAcl %d", Counters[SETACL]);
    SLog(0, "Store %d", Counters[STORE]);
    SLog(0, "ValidateAttrs %d", Counters[VALIDATEATTRS]);

    SLog(0, "Remove %d", Counters[REMOVE]);
    SLog(0, "Create %d", Counters[CREATE]);
    SLog(0, "Rename %d", Counters[RENAME]);
    SLog(0, "SymLink %d", Counters[SYMLINK]);
    SLog(0, "Link %d", Counters[LINK]);
    SLog(0, "MakeDir %d", Counters[MAKEDIR]);
    SLog(0, "RemoveDir %d", Counters[REMOVEDIR]);
    SLog(0, "GetRootVolume %d", Counters[GETROOTVOLUME]);
    SLog(0, "SetRootVolume %d", Counters[SETROOTVOLUME]);
    SLog(0, "GetVolumeStatus %d", Counters[GETVOLUMESTAT]);
    SLog(0, "SetVolumeStatus %d", Counters[SETVOLUMESTAT]);
    
    SLog(0, "GetTime %d", Counters[GETTIME]); 
    SLog(0, "GetStatistics %d", Counters[GETSTATISTICS]); 
    SLog(0, "GetVolumeInfo %d", Counters[ViceGetVolumeInfo_OP]); 
    SLog(0, "AllocFids %d", Counters[ALLOCFIDS]); 
    SLog(0, "COP2 %d", Counters[ViceCOP2_OP]); 
    SLog(0, "Resolve %d", Counters[RESOLVE]);
    SLog(0, "Repair %d", Counters[REPAIR]);
    SLog(0, "SetVV %d", Counters[SETVV]);
    SLog(0, "Reintegrate %d", Counters[REINTEGRATE]);

    SLog(0, "OpenReintHandle %d", Counters[ViceOpenReintHandle_OP]); 
    SLog(0, "QueryReintHandle %d", Counters[ViceQueryReintHandle_OP]); 
    SLog(0, "SendReintFragment %d", Counters[ViceSendReintFragment_OP]); 
    SLog(0, "CloseReintHandle %d", Counters[ViceCloseReintHandle_OP]); 

    SLog(0, "GetVolVS %d", Counters[GETVOLVS]);
    SLog(0, "ValidateVols %d", Counters[VALIDATEVOLS]);

    SLog(0, "GetWBPermit %d", Counters[ViceGetWBPermit_OP]); 
    SLog(0, "TossWBPermit %d", Counters[ViceTossWBPermit_OP]); 
    SLog(0, "RejectWBPermit %d", Counters[ViceRejectWBPermit_OP]); 

    SLog(0, "GetAttrPlusSHA %d", Counters[GETATTRPLUSSHA]); 
    SLog(0, "ValidateAttrsPlusSHA %d", Counters[VALIDATEATTRSPLUSSHA]); 

    seconds = Counters[FETCHTIME]/1000;
    if(seconds <= 0) 
	seconds = 1;
    SLog(0,
	   "Total FetchDatas = %d, bytes transfered = %d, transfer rate = %d bps",
	   Counters[FETCHDATAOP], Counters[FETCHDATA],
	   Counters[FETCHDATA]/seconds);
    SLog(0,
	   "Fetched files <%dk = %d; <%dk = %d; <%dk = %d; <%dk = %d; >%dk = %d.",
	   SIZE1 / 1024, Counters[FETCHD1], SIZE2 / 1024,
	   Counters[FETCHD2], SIZE3 / 1024, Counters[FETCHD3], SIZE4 / 1024,
	   Counters[FETCHD4], SIZE4 / 1024, Counters[FETCHD5]);
    seconds = Counters[STORETIME]/1000;
    if(seconds <= 0) 
	seconds = 1;
    SLog(0,
	   "Total StoreDatas = %d, bytes transfered = %d, transfer rate = %d bps",
	   Counters[STOREDATAOP], Counters[STOREDATA],
	   Counters[STOREDATA]/seconds);
    SLog(0,
	   "Stored files <%dk = %d; <%dk = %d; <%dk = %d; <%dk = %d; >%dk = %d.",
	   SIZE1 / 1024, Counters[STORED1], SIZE2 / 1024,
	   Counters[STORED2], SIZE3 / 1024, Counters[STORED3], SIZE4 / 1024,
	   Counters[STORED4], SIZE4 / 1024, Counters[STORED5]);
    VPrintCacheStats();
    DP_PrintStats(fp);
    DH_PrintStats(fp);
    SLog(0,
	   "RPC Total bytes:     sent = %u, received = %u",
	   rpc2_Sent.Bytes + rpc2_MSent.Bytes + sftp_Sent.Bytes + sftp_MSent.Bytes,
	   rpc2_Recvd.Bytes + rpc2_MRecvd.Bytes + sftp_Recvd.Bytes + sftp_MRecvd.Bytes);
    SLog(0, 
	   "\tbytes sent: rpc = %d, multirpc = %d, sftp = %d, sftp multicasted = %d",
	   rpc2_Sent.Bytes, rpc2_MSent.Bytes, sftp_Sent.Bytes, sftp_MSent.Bytes);
    SLog(0, 
	   "\tbytes received: rpc = %d, multirpc = %d, sftp = %d, sftp multicasted = %d",
	   rpc2_Recvd.Bytes, rpc2_MRecvd.Bytes, sftp_Recvd.Bytes, sftp_MRecvd.Bytes);
    SLog(0,
	   "RPC Total packets:   sent = %d, received = %d",
	   rpc2_Sent.Total + rpc2_MSent.Total + sftp_Sent.Total + sftp_MSent.Total,
	   rpc2_Recvd.Total + rpc2_MRecvd.Total + sftp_Recvd.Total + sftp_MRecvd.Total);
    SLog(0, 
	   "\tpackets sent: rpc = %d, multirpc = %d, sftp = %d, sftp multicasted = %d",
	   rpc2_Sent.Total, rpc2_MSent.Total, sftp_Sent.Total, sftp_MSent.Total);
    SLog(0, 
	   "\tpackets received: rpc = %d, multirpc = %d, sftp = %d, sftp multicasted = %d",
	   rpc2_Recvd.Total, rpc2_MRecvd.Total, sftp_Recvd.Total, sftp_MRecvd.Total);
    SLog(0,
	   "RPC Packets retried = %d, Invalid packets received = %d, Busies sent = %d",
	   rpc2_Sent.Retries - rpc2_Recvd.GoodBusies,
	   rpc2_Recvd.Total + rpc2_MRecvd.Total - 
	        rpc2_Recvd.GoodRequests - rpc2_Recvd.GoodReplies - 
		rpc2_Recvd.GoodBusies - rpc2_MRecvd.GoodRequests,
	   rpc2_Sent.Busies);
    SLog(0,
	   "RPC Requests %d, Good Requests %d, Replies %d, Busies %d",
	   rpc2_Recvd.Requests, rpc2_Recvd.GoodRequests, 
	   rpc2_Recvd.GoodReplies, rpc2_Recvd.GoodBusies);
    SLog(0,
	   "RPC Counters: CCount %d; Unbinds %d; FConns %d; AConns %d; GCConns %d",
	   rpc2_ConnCount, rpc2_Unbinds, rpc2_FreeConns, 
	   rpc2_AllocConns, rpc2_GCConns);
    SLog(0,
	   "RPC Creation counts: Conn %d; SL %d; PB Small %d, Med %d, Large %d; SS %d",
	   rpc2_ConnCreationCount, rpc2_SLCreationCount, rpc2_PBSmallCreationCount,
	   rpc2_PBMediumCreationCount, rpc2_PBLargeCreationCount, rpc2_SSCreationCount);
    SLog(0,
	   "RPC2 In Use: Conn %d; SS %d",
	   rpc2_ConnCount, rpc2_SSCount);
    SLog(0,
	   "RPC2 PB: InUse %d, Hold %d, Freeze %d, SFree %d, MFree %d, LFree %d", 
	   rpc2_PBCount, rpc2_PBHoldCount, rpc2_PBFreezeCount,
	   rpc2_PBSmallFreeCount, rpc2_PBMediumFreeCount, rpc2_PBLargeFreeCount);
    SLog(0,
	   "RPC2 HW:  Freeze %d, Hold %d", rpc2_FreezeHWMark, rpc2_HoldHWMark);
    SLog(0,
	   "SFTP:	datas %d, datar %d, acks %d, ackr %d, retries %d, duplicates %d",
	   sftp_datas, sftp_datar, sftp_acks, sftp_ackr, sftp_retries, sftp_duplicates);
    SLog(0,
	   "SFTP:  timeouts %d, windowfulls %d, bogus %d, didpiggy %d",
	   sftp_timeouts, sftp_windowfulls, sftp_bogus, sftp_didpiggy);
    SLog(0,
	   "Total CB entries= %d, blocks = %d; and total file entries = %d, blocks = %d",
	   CBEs, CBEBlocks, FEs, FEBlocks);
    /*    ProcSize = sbrk(0) >> 10;*/
    SLog(0,
	   "There are currently %d connections in use",
	   CurrentConnections);
    CLIENT_GetWorkStats(&workstations, &activeworkstations, (unsigned)tpl.tv_sec-15*60);
    SLog(0,
	   "There are %d workstations and %d are active (req in < 15 mins)",
	   workstations, activeworkstations);
    if(!GetEtherStats()) {
	SLog(0,
	       "Ether Total bytes: sent = %u, received = %u",
	       etherBytesWritten, etherBytesRead);
	SLog(0,
	       "Ether Packets:     sent = %d, received = %d, errors = %d",
	       etherWrites, etherInterupts, etherRetries);
    }
    
    if (RvmType == RAWIO || RvmType == UFS) {
	SLog(0,
	       "Printing RVM statistics\n");
	rvm_statistics_t rvmstats;
        rvm_init_statistics(&rvmstats);
	RVM_STATISTICS(&rvmstats);
	rvm_print_statistics(&rvmstats, fp);
	rvm_free_statistics(&rvmstats);
    }
    SLog(0, "Printing RDS statistics\n");
    rds_print_stats();

    Statistics = 0;
    SLog(0, "done\n");
#ifdef _TIMECALLS_
    /* print the histograms timing the operations */
    SLog(0, SrvDebugLevel, fp, "Operation histograms\n");
    SLog(0, SrvDebugLevel, fp, "Create histogram:\n");
    PrintHisto(fp, &Create_Total_hg);
    ClearHisto(&Create_Total_hg);
    SLog(0, SrvDebugLevel, fp, "Remove histogram:\n");
    PrintHisto(fp, &Remove_Total_hg);
    ClearHisto(&Remove_Total_hg);
    SLog(0, SrvDebugLevel, fp, "Link histogram:\n");
    PrintHisto(fp, &Link_Total_hg);
    ClearHisto(&Link_Total_hg);
    SLog(0, SrvDebugLevel, fp, "Rename histogram:\n");
    PrintHisto(fp, &Rename_Total_hg);
    ClearHisto(&Rename_Total_hg);
    SLog(0, SrvDebugLevel, fp, "MakeDir histogram:\n");
    PrintHisto(fp, &MakeDir_Total_hg);
    ClearHisto(&MakeDir_Total_hg);
    SLog(0, SrvDebugLevel, fp, "RemoveDir histogram:\n");
    PrintHisto(fp, &RemoveDir_Total_hg);
    ClearHisto(&RemoveDir_Total_hg);
    SLog(0, SrvDebugLevel, fp, "SymLink histogram:\n");
    PrintHisto(fp, &SymLink_Total_hg);
    ClearHisto(&SymLink_Total_hg);
    SLog(0, SrvDebugLevel, fp, "SpoolVMLogRecord histogram:\n");
    PrintHisto(fp, &SpoolVMLogRecord_hg);
    ClearHisto(&SpoolVMLogRecord_hg);

    SLog(0, SrvDebugLevel, fp, "PutObjects_Transaction_hg histogram:\n");
    PrintHisto(fp, &PutObjects_Transaction_hg);
    ClearHisto(&PutObjects_Transaction_hg);

    SLog(0, SrvDebugLevel, fp, "PutObjects_TransactionEnd_hg histogram:\n");
    PrintHisto(fp, &PutObjects_TransactionEnd_hg);
    ClearHisto(&PutObjects_TransactionEnd_hg);

    SLog(0, SrvDebugLevel, fp, "PutObjects_RVM_hg histogram:\n");
    PrintHisto(fp, &PutObjects_RVM_hg);
    ClearHisto(&PutObjects_RVM_hg);


    SLog(0, SrvDebugLevel, fp, "PutObjects_Inodes_hg histogram:\n");
    PrintHisto(fp, &PutObjects_Inodes_hg);
    ClearHisto(&PutObjects_Inodes_hg);
#endif /* _TIMECALLS_ */
}


static void SetDebug()
{

    if (SrvDebugLevel > 0) {
	SrvDebugLevel *= 5;
    }
    else {
	SrvDebugLevel = 1;
    }
    AL_DebugLevel = SrvDebugLevel/10;
    RPC2_DebugLevel = (long)SrvDebugLevel/10;
    VolDebugLevel = DirDebugLevel = SrvDebugLevel;
    SLog(0, "Set Debug On level = %d, RPC level = %d",
	    SrvDebugLevel, RPC2_DebugLevel);
}


static void ResetDebug()
{
    AL_DebugLevel = 0;
    RPC2_DebugLevel = 0;
    DirDebugLevel = VolDebugLevel = 0;
    SLog(0, "Reset Debug levels to 0");
}

/*
  BEGIN_HTML
  <a name="SwapMalloc"><strong>Toggle tracing of recoverable(rds) mallocs  </strong></a> 
  END_HTML
*/
void SwapMalloc()
{
  if (MallocTrace) {
    rds_trace_dump_heap();
    rds_trace_off();
    MallocTrace = FALSE;
  } else {
    rds_trace_on(stdout);
    rds_trace_dump_heap();
    MallocTrace = TRUE;
  }
}

/* Note  TimeStamp and rds_printer are stolen directly from the SLog
   implementation.  I couldn't get SLog to work directly, because
   of problem relating to varargs.  SMN
   */
static void RdsTimeStamp(FILE *f)
    {
    struct tm *t;
    time_t clock;
    static int oldyear = -1, oldyday = -1; 
    
    time(&clock);
    t = localtime(&clock);
    if ((t->tm_year > oldyear) || (t->tm_yday > oldyday)) {
	char datestr[80];

	strftime(datestr, sizeof(datestr), "\nDate: %a %m/%d/%Y\n\n", t);
	fputs(datestr, f);
    }
    fprintf(f, "%02d:%02d:%02d ", t->tm_hour, t->tm_min, t->tm_sec);    
    oldyear = t->tm_year; /* remember when we were last called */
    oldyday = t->tm_yday;
    }

void rds_printer(char *fmt ...)
{
  va_list ap;
  
  RdsTimeStamp(stdout);
  
  va_start(ap, fmt);
  vfprintf(stdout, fmt, ap);
  va_end(ap);
}

/*
 * SwapLog: Reopen the /"vicedir"/srv/SrvLog and SrvErr files. If some other
 * process such as logrotate has moved the old logfile aside we'll start
 * writing to a new logfile.
 */
void SwapLog()
{
    struct timeval tp;

    /* Need to chdir() again, since salvage may have put me elsewhere */
    if(chdir(vice_file("srv"))) {
	SLog(0, "Could not cd to %s; not swapping logs", vice_file("srv"));
	return;
    }

    SLog(0, "Starting new SrvLog file");
    freopen("SrvLog", "a+", stdout);
    freopen("SrvErr", "a+", stderr);
    
    /* Print out time/date, since date info has "scrolled off" */
    TM_GetTimeOfDay(&tp, 0);
    SLog(0, "New SrvLog started at %s", ctime((const time_t *)&tp.tv_sec));
}

static void FileMsg()
{
    int srvpid;
    
    srvpid = getpid();
    SLog(0, "The server (pid %d) can be controlled using volutil commands", srvpid);
    SLog(0, "\"volutil -help\" will give you a list of these commands");
    SLog(0, "If desperate,\n\t\t\"kill -SIGWINCH %d\" will increase debugging level", srvpid);
    SLog(0, "\t\"kill -SIGUSR2 %d\" will set debugging level to zero", srvpid);
    SLog(0, "\t\"kill -9 %d\" will kill a runaway server", srvpid);
    }

/*
  BEGIN_HTML
  <a name="ViceTerminate"><strong>Request a shutdown by setting a global flag. 
  </strong></a> 
  END_HTML
*/
void ViceTerminate()
{
    ViceShutDown = 1;
    SLog(0, "Shutdown received");
}


static int ReadConfigFile(void)
{
    char confname[80];
    int  datalen = 0;
    int  multconf;
    int  zombify = 0;
    int  numservers = 0;

    /* Load configuration file. */
    codaconf_init("server.conf");

    /* Load server specific configuration file */
    sprintf (confname, "server_%d.conf", ServerNumber);
    multconf = codaconf_init(confname);

    /* srv.cc defined values ... */
    CONF_INT(Authenticate,	"authenticate",	   1); 
    CONF_INT(AllowResolution,	"resolution",	   1); 
    CONF_INT(AllowSHA,		"allow_sha",	   0); 
    CONF_INT(comparedirreps,	"comparedirreps",  1); 
    CONF_INT(pollandyield,	"pollandyield",    1); 
    CONF_INT(pathtiming,	"pathtiming",	   1);
    CONF_INT(MapPrivate,	"mapprivate",	   0);
    CONF_STR(CodaSrvIp,		"ipaddress",	   NULL);

    CONF_INT(large,		"large",	   500);
    CONF_INT(small,		"small",	   500);

    CONF_STR(vicedir,		"vicedir",	   "/vice");
    vice_dir_init(vicedir, ServerNumber);

    CONF_INT(trace,		"trace",	   0);
    CONF_INT(SrvWindowSize,	"windowsize",	   32);
    CONF_INT(SrvSendAhead,	"sendahead",	   8);
    CONF_INT(timeout,		"timeout",	   15);
    CONF_INT(retrycnt,		"retrycnt",	   4);
    CONF_INT(lwps,		"lwps",		   6);
    if (lwps > MAXLWP) lwps = MAXLWP;

    CONF_INT(buffs,		"buffs",	   100);
    CONF_INT(stack,		"stack",	   96);
    CONF_INT(cbwait,		"cbwait",	   240);
    CONF_INT(chk,		"chk",		   30);
    CONF_INT(ForceSalvage,	"forcesalvage",	   1);
    CONF_INT(SalvageOnShutdown,	"salvageonshutdown", 0);
    CONF_INT(DumpVM,		"dumpvm",	   0);

    /* Server numbers and  Rvm parameters */
    CONF_INT(numservers,      "numservers",	   1);
    if (numservers > 1 && ServerNumber == 0) {
        SLog(0, "Configuration error, server number 0 with multiple servers");
	exit(-1);
    }
    CONF_INT(_Rvm_Truncate, 	"rvmtruncate",	   0);

    if (RvmType == UNSET) {
	if (ServerNumber == 0 || multconf) {
	    CONF_STR(_Rvm_Log_Device,	"rvm_log",         "");
	    CONF_STR(_Rvm_Data_Device,	"rvm_data",        "");
	    CONF_INT(datalen,		"rvm_data_length", 0);
	    CONF_STR(srvhost,		"hostname",	   NULL);
	} else {
	    sprintf(confname, "rvm_log_%d", ServerNumber);
	    CONF_STR(_Rvm_Log_Device,  confname, "");

	    sprintf(confname, "rvm_data_%d", ServerNumber);
	    CONF_STR(_Rvm_Data_Device, confname, "");

	    sprintf(confname, "rvm_data_length_%d", ServerNumber);
	    CONF_INT(datalen, confname, 0);

	    sprintf(confname, "hostname_%d", ServerNumber);
	    CONF_STR(srvhost, confname,	NULL);
	}
    }
    if (ServerNumber && (srvhost == NULL || *srvhost == 0)) {
        SLog(0, "Multiple servers specified, must specify hostname.\n");
        exit(-1);
    }

    if (datalen != 0) {
        _Rvm_DataLength = RVM_MK_OFFSET(0, datalen);
	RvmType = RAWIO;
	/* Checks ... */
	if (_Rvm_Log_Device == NULL || *_Rvm_Log_Device == 0) {
	    SLog(0, "Must specify a RVM log file/device\n");
	    exit(-1);
	}
	if (_Rvm_Data_Device == NULL || *_Rvm_Data_Device == 0) {
	    SLog(0, "Must specify a RVM data file/device\n");
	    exit(-1);
	}
    }

    /* Other command line parameters ... */
    extern int nodebarrenize;

    CONF_INT(nodebarrenize,   "nodebarrenize",	   0);
    CONF_INT(NoWritebackConn, "nowriteback",	   0);
    CONF_INT(zombify,	      "zombify",	   0);
    if (zombify)
        coda_assert_action = CODA_ASSERT_SLEEP;
    CONF_STR(vicetab,	      "vicetab",	   NULL);
    if (!vicetab) {
        vicetab = strdup(vice_sharedfile("db/vicetab"));
    }
    

    return 0;
}


static int ParseArgs(int argc, char *argv[])
{
    int   i;

    NoWritebackConn = 0;

    for (i = 1; i < argc; i++) {
	if (!strcmp(argv[i], "-d")) {
	    debuglevel = atoi(argv[++i]);
	    printf("Setting debuglevel to %d\n", debuglevel);
	    SrvDebugLevel = debuglevel;
	    AL_DebugLevel = SrvDebugLevel/10;
	    DirDebugLevel = SrvDebugLevel;
	}
	else 
	    if (!strcmp(argv[i], "-zombify"))
		coda_assert_action = CODA_ASSERT_SLEEP;
	else 
	    if (!strcmp(argv[i], "-rpcdebug"))
		RPC2_DebugLevel = atoi(argv[++i]);
	else
	    if (!strcmp(argv[i], "-dir"))
		vicedir = argv[++i];
	else
	    if (!strcmp(argv[i], "-srvhost"))
		srvhost = argv[++i];
	else
	    if (!strcmp(argv[i], "-vicetab"))
		vicetab = argv[++i];
	else
	    if (!strcmp(argv[i], "-p")) {
		lwps = atoi(argv[++i]);
		if(lwps > MAXLWP)
		    lwps = MAXLWP;
	    }
	else
	    if (!strcmp(argv[i], "-c"))
	    	chk = atoi(argv[++i]);
	else
	    if (!strcmp(argv[i], "-noauth"))
	    	Authenticate = 0;
	else
	    if (!strcmp(argv[i], "-rdstrace"))
	    	MallocTrace = 1;
	else
	    if (!strcmp(argv[i], "-forcesalvage"))
		ForceSalvage = 1;
	else
	    if (!strcmp(argv[i], "-quicksalvage"))
		ForceSalvage = 0;
	else
	    if (!strcmp(argv[i], "-b"))
		buffs = atoi(argv[++i]);
	else
	    if (!strcmp(argv[i], "-l"))
		large = atoi(argv[++i]);
	else
	    if (!strcmp(argv[i], "-r"))
		retrycnt = atoi(argv[++i]);
	else
	    if (!strcmp(argv[i], "-o"))
		timeout = atoi(argv[++i]);
	else
	    if (!strcmp(argv[i], "-ws"))
		SrvWindowSize = atoi(argv[++i]);
	else
	    if (!strcmp(argv[i], "-sa"))
		SrvSendAhead = atoi(argv[++i]);
	else
	    if (!strcmp(argv[i], "-s"))
		small = atoi(argv[++i]);
	else	
	    if (!strcmp(argv[i], "-k"))
		stack = atoi(argv[++i]);
	else	
	    if (!strcmp(argv[i], "-t")) {
		trace = atoi(argv[++i]);
	    }
	else
	    if (!strcmp(argv[i], "-w"))
		cbwait = atoi(argv[++i]);
	else 
	    if (!strcmp(argv[i], "-nores")) 
		AllowResolution = 0;
	else 
	    if (!strcmp(argv[i], "-nocmp"))
		comparedirreps = 0;
 	else 
	    if (!strcmp(argv[i], "-time")) 
		pathtiming = 1;
	else 
	    if (!strcmp(argv[i], "-nopy")) 
		pollandyield = 0;
	else 
	    if (!strcmp(argv[i], "-salvageonshutdown")) 
		SalvageOnShutdown = 1;
	else 
	    if (!strcmp(argv[i], "-rvmopt")) 
		optimizationson = RVM_COALESCE_RANGES;
	else 
	    if (!strcmp(argv[i], "-prottrunc")) {
		prottrunc = TRUE;
		rvm_no_yield = TRUE;
	    }
	else 
	    if (!strcmp(argv[i], "-maxworktime")){
		extern struct timeval cont_sw_threshold;
		cont_sw_threshold.tv_sec = (atoi(argv[++i]));
	    }
#ifdef _TIMECALLS_	
    	else
	    if (!strcmp(argv[i], "-usenscclock")) {
		clockFD = open("/dev/cntr0", O_RDONLY, 0666);
		CODA_ASSERT(clockFD > 0);
	    }
#endif /* _TIMECALLS_ */
	else
	    if (!strcmp(argv[i], "-nc")){
		if (RvmType != UNSET) {
		    SLog(0, "Multiple Persistence methods selected.\n");
		    exit(-1);
		}
		RvmType = VM;
		if (i < argc - 1) cam_log_file = argv[++i];
	    }
	else
	    if (!strcmp(argv[i], "-cam")) {
		if (RvmType != UNSET) {
		    SLog(0, "Multiple Persistence methods selected.\n");
		    exit(-1);
		}
		SLog(0, "Camelot not supported any more\n");
		exit(-1);
	    }
	else
	    if (!strcmp(argv[i], "-rvm")) {
		struct stat buf;
		if (RvmType != UNSET) {
		    SLog(0, "Multiple Persistence methods selected.");
		    exit(-1);
		}

		if (i + 4 > argc) {	/* Need three more arguments here */
		    eprint("-rvm needs 3 args: LOGDEV DATADEV DATA-LENGTH.\n");
		    SLog(0, "-rvm needs 3 args: LOGDEV DATADEV DATA-LENGTH.");
		    exit(-1);
		}
		
		RvmType = RAWIO;
		_Rvm_Log_Device = (char *)malloc((unsigned)strlen(argv[++i]) + 1);
		strcpy(_Rvm_Log_Device, argv[i]);
		_Rvm_Data_Device = (char *)malloc((unsigned)strlen(argv[++i]) + 1);
		strcpy(_Rvm_Data_Device, argv[i]);
		if (stat(_Rvm_Log_Device, &buf) != 0) {
		    perror("Can't open Log Device");
		    exit(-1);
		}

		if (stat(_Rvm_Data_Device, &buf) != 0) {
		    perror("Can't open Data Device");
		    exit(-1);
		}
		_Rvm_DataLength = RVM_MK_OFFSET(0, atoi(argv[++i]));
	    }
	else 
	    if (!strcmp(argv[i], "-dumpvm")) {
		DumpVM = 1;
	    }
	else
	    if (!strcmp(argv[i], "-trunc")) {
		_Rvm_Truncate = atoi(argv[++i]);
	    }
	else
	    if (!strcmp(argv[i], "-mondhost")) {
		SmonHost = argv[++i];
	    }
	else
	    if (!strcmp(argv[i], "-mondport")) {
		SmonPort = atoi(argv[++i]);
	    }
	else 
	    if (!strcmp(argv[i], "-nodebarrenize")) {
		extern int nodebarrenize;
		nodebarrenize = 1;
	    }
	else 
	    if (!strcmp(argv[i], "-nowriteback")) {
		NoWritebackConn = 1;
	    }
	else 
	    if (!strcmp(argv[i], "-mapprivate")) {
		MapPrivate = 1;
	    }
	else
	    if (!strcmp(argv[i], "-n"))
		ServerNumber = atoi(argv[++i]);
	else {
	    return(-1);
	}
    }
    return(0);
}

/* char	* fkey1;	 name of file that contains key1 (normally KEY1) */
/* char	* fkey2;	 name of file that contains key2 (normally KEY2) */
static void InitServerKeys(char *fkey1, char *fkey2)
{
    FILE                * tf;
    RPC2_EncryptionKey    ptrkey1;
    RPC2_EncryptionKey    ptrkey2;
    int                   NoKey1 = 1;
    int                   NoKey2 = 1;

    tf = fopen(fkey1, "r");
    if( tf == NULL ) {
	perror("could not open key 1 file");
    } else {
	NoKey1 = 0;
	memset(ptrkey1, 0, RPC2_KEYSIZE);
	fread(ptrkey1, 1, RPC2_KEYSIZE, tf);
	fclose(tf);
    }

    tf = fopen(fkey2, "r");
    if( tf == NULL ) {
	perror("could not open key 2 file");
    } else {
        NoKey2 = 0;
	memset(ptrkey2, 0, RPC2_KEYSIZE);
	fread(ptrkey2, 1, RPC2_KEYSIZE, tf);
	fclose(tf);
    }

    /* no keys */
    if ( NoKey1 && NoKey2 ) {
	SLog(0, 0, stderr, "No Keys found. Zombifying..");
	CODA_ASSERT(0);
    }

    /* two keys: don't do a double key if they are equal */
    if( NoKey1 == 0  && NoKey2 == 0 ) {
	if ( memcmp(ptrkey1, ptrkey2, sizeof(ptrkey1)) == 0 ) 
	    SetServerKeys(ptrkey1, NULL);
	else
	    SetServerKeys(ptrkey1, ptrkey2);
	return;
    }

    /* one key */
    if ( NoKey1 == 0 && NoKey2 != 0 )
	SetServerKeys(ptrkey1, NULL);
    else 
	SetServerKeys(NULL, ptrkey2);
}


void Die(char *msg)
{
    SLog(0,"%s",msg);
    CODA_ASSERT(0);
}


static void DaemonizeSrv() { 
    int  rc; 
   /* Set DATA segment limit to maximum allowable. */
#ifndef __CYGWIN32__
    struct rlimit rl;
    if (getrlimit(RLIMIT_DATA, &rl) < 0) {
        perror("getrlimit"); exit(-1);
    }
    rl.rlim_cur = rl.rlim_max;
    SLog(0, 
	   "Resource limit on data size are set to %d\n", rl.rlim_cur);
    if (setrlimit(RLIMIT_DATA, &rl) < 0) {
	perror("setrlimit"); exit(-1); 
    }
#endif
    /* the forking code doesn't work well with our "startserver" script. 
       reactivate this when that silly thing is gone */
#if 0 
    child = fork();
    
    if ( child < 0 ) { 
	fprintf(stderr, "Cannot fork: exiting.\n");
	exit(1);
    }

    if ( child != 0 ) /* parent */
	exit(0); 
#endif 

    rc = setsid();
#if 0
    if ( rc < 0 ) {
	fprintf(stderr, "Error detaching from terminal.\n");
	exit(1);
    }
#endif

    signal(SIGUSR2, (void (*)(int))ResetDebug);
#ifndef __CYGWIN32__
    signal(SIGWINCH, (void (*)(int))SetDebug);
#endif
    signal(SIGHUP,  (void (*)(int))SwapLog);

    /* Signals that are zombied allow debugging via gdb */
    signal(SIGTRAP, (void (*)(int))zombie);
    signal(SIGILL,  (void (*)(int))zombie);
    signal(SIGFPE,  (void (*)(int))zombie);
    signal(SIGSEGV, (void (*)(int))zombie);

#ifdef	RVMTESTING
    signal(SIGBUS, (void (*)(int))my_sigBus); /* Defined in util/rvmtesting.c */
#else
    signal(SIGBUS,  (void (*)(int))zombie);
#endif
}

static void InitializeServerRVM(char *name)
{		    
    switch (RvmType) {							    
    case VM :					       	    
	camlibRecoverableSegment = (camlib_recoverable_segment *)
	    malloc(sizeof(struct camlib_recoverable_segment));
       break;                                                               
	                                                                    
    case RAWIO :                                                            
    case UFS : {                                                            
	rvm_return_t err;						    
	rvm_options_t *options = rvm_malloc_options();			    
#ifndef __CYGWIN32__
	struct rlimit stackLimit;					    
	stackLimit.rlim_cur = CODA_STACK_LENGTH;			    
/*	setrlimit(RLIMIT_STACK, &stackLimit);*/	/* Set stack growth limit */ 
#endif
	options->log_dev = _Rvm_Log_Device;				    
	options->flags = optimizationson;
	if (MapPrivate)
	  options->flags |= RVM_MAP_PRIVATE;
	if (prottrunc)
	    options->truncate = 0;
	else if (_Rvm_Truncate > 0 && _Rvm_Truncate < 100) {
	    SLog(0,
		 "Setting Rvm Truncate threshhold to %d.\n", _Rvm_Truncate); 
	    options->truncate = _Rvm_Truncate;
	}

#if	defined(__FreeBSD__)
	sbrk((int)(0x50000000 - (int)sbrk(0))); /* for garbage reasons. */
#elif	defined(__NetBSD__) && (defined(NetBSD1_3) || defined(__NetBSD_Version__))
	/*sbrk((void *)(0x50000000 - (int)sbrk(0))); / * for garbage reasons. */
	/* Commented out by Phil Nelson because it caused a SIGBUS on 1.4.2
	   and it appears to work just fine without this for 1.4.1 ... */
#elif	defined(__NetBSD__) && NetBSD1_2
	sbrk((void *)(0x20000000 - (int)sbrk(0))); /* for garbage reasons. */
#endif
        err = RVM_INIT(options);                   /* Start rvm */           
        if ( err == RVM_ELOG_VERSION_SKEW ) {                                
            SLog(0, 
		   "rvm_init failed because of skew RVM-log version."); 
            SLog(0, "Coda server not started.");                  
            exit(-1);                                                          
	} else if (err != RVM_SUCCESS) {                                     
	    SLog(0, "rvm_init failed %s",rvm_return(err));	    
            CODA_ASSERT(0);                                                       
	}                                                                    
	CODA_ASSERT(_Rvm_Data_Device != NULL);	   /* Load in recoverable mem */ 
        rds_load_heap(_Rvm_Data_Device, 
		      _Rvm_DataLength,(char **)&camlibRecoverableSegment, 
		      (int *)&err);  
	if (err != RVM_SUCCESS)						    
	    SLog(0, 
		   "rds_load_heap error %s",rvm_return(err));	    
	CODA_ASSERT(err == RVM_SUCCESS);                                         
        /* Possibly do recovery on data structures, coalesce, etc */	    
	rvm_free_options(options);					    
        break;                                                              
    }                                                                       
	                                                                    
    case UNSET:							    
    default:	                                                            
	printf("No persistence method selected!n");			    
	exit(-1); /* No persistence method selected, so die */		    
    }
}
