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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/vice/srv.cc,v 4.11 1998/01/22 18:46:35 braam Exp $";
#endif /*_BLURB_*/


/*

                         IBM COPYRIGHT NOTICE

                          Copyright (C) 1986
             International Business Machines Corporation
                         All Rights Reserved

This  file  contains  some  code identical to or derived from the 1986
version of the Andrew File System ("AFS"), which is owned by  the  IBM
Corporation.    This  code is provded "AS IS" and IBM does not warrant
that it is free of infringement of  any  intellectual  rights  of  any
third  party.    IBM  disclaims  liability of any kind for any damages
whatsoever resulting directly or indirectly from use of this  software
or  of  any  derivative work.  Carnegie Mellon University has obtained
permission to distribute this code, which is based on Version 2 of AFS
and  does  not  contain the features and enhancements that are part of
Version 3 of AFS.  Version 3 of  AFS  is  commercially  available  and
supported by Transarc Corporation, Pittsburgh, PA.

*/


/************************************************************************/
/*									*/
/*  file.c	- File Server main loop					*/
/*									*/
/************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/file.h>
#ifndef __CYGWIN32__
#include <sys/dir.h>
#endif
#include <strings.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netdb.h>
#ifdef __MACH__
#include <libc.h>
#include <mach.h> 
#endif
#include <stdarg.h>
#include <stdlib.h>

#include <lwp.h>
#include <timer.h>
#include <rpc2.h>
#include <rpc2.private.h>
#include <sftp.h>
#include <fail.h>
#include <fcon.h>
#include <partition.h>
#include <util.h>

extern int nice(int);
extern int Fcon_Init(); 

#ifdef _TIMECALLS_
#include <histo.h>
#endif _TIMECALLS_

#ifdef __cplusplus
}
#endif __cplusplus

extern void setmyname(char *);

#include <rvmlib.h>

#ifdef __cplusplus
extern "C" {
#endif __cplusplus
#include <rvm_statistics.h>
#ifdef __cplusplus
}
#endif __cplusplus




#include <prs.h>
#include <prs_fs.h>
#include <al.h>
#include <auth2.h>
#include <srv.h>
#include <coda_dir.h>
#include <recov.h>
#include <camprivate.h>
#include <coda_globals.h>
#include <rvmdir.h>
#include <vrdb.h>
#include <res.h>
#include <rescomm.h>
#include <lockqueue.h>
#include <vsg.h>
#include <newplumb.h>
#include "coppend.h"
#include <volutil.h>


/* Auth2 imports. */
extern long GetKeysFromToken(RPC2_CountedBS *, RPC2_EncryptionKey, RPC2_EncryptionKey);
extern void SetServerKeys(RPC2_EncryptionKey, RPC2_EncryptionKey);


/* *****  Exported variables  ***** */

int SystemId = 0;
unsigned StartTime = 0;
int CurrentConnections = 0;
int Authenticate = 1;
int Counters[MAXCNTRS];
ViceFid NullFid = {0, 0, 0};
int AllowResolution = 1;	/* controls directory resolution */
int comparedirreps = 1;
int pathtiming = 0;
int pollandyield = 1; 
int probingon = 0;
int optimizationson = 0;
int OptimizeStore = 0;
int MaxVols = MAXVOLS;          /* so we can use it in vicecb.c. yuck. */

extern rvm_length_t rvm_test;
extern int canonicalize;	/* controls if vrdb - Getvolumeinfo should return 
				   hosts in canonical order - this is only temporary */

#ifdef _TIMECALLS_
int clockFD = 0;		/* for timing with the NSC clock board */
struct hgram Create_Total_hg, Remove_Total_hg, Link_Total_hg;
struct hgram Rename_Total_hg, MakeDir_Total_hg, RemoveDir_Total_hg, SymLink_Total_hg;
struct hgram SpoolVMLogRecord_hg, PutObjects_Transaction_hg, PutObjects_TransactionEnd_hg,
	PutObjects_Inodes_hg, PutObjects_RVM_hg; 
#endif _TIMECALLS_

int large = 500;		/* control size of lru cache for large vnode */
int small = 500;		/* control size of lru cache for small vnodes */

extern char *SmonHost;
extern int SmonPortal; 

#ifdef PERFORMANCE
/* added array of thread id's for thread_info, Puneet */
#define	NLWPS	5
thread_t lwpth[NLWPS];
thread_array_t thread_list;
int thread_count;
#endif PERFORMANCE


/* *****  Private variables  ***** */

/* File server parameters. */
PRIVATE int trace = 0;
PRIVATE int SrvWindowSize = 32;
PRIVATE int SrvSendAhead = 8;
PRIVATE	int timeout = 60;	/* formerly 30 */
PRIVATE	int retrycnt = 6;	/* formerly 20 */
PRIVATE int Statistics;
PRIVATE int debuglevel = 0;
PRIVATE int lwps = 6;
PRIVATE	int buffs = 100;	/* formerly 200 */
int stack = 96;
PRIVATE int cbwait = 300;
PRIVATE int chk = 30;
/* PRIVATE int ProcSize = 0; */
PRIVATE int ForceSalvage = 1;
PRIVATE int SalvageOnShutdown = 1;
PRIVATE int ViceShutDown = 0;

/* Camelot/RVM stuff. */
DEFINE_RECOVERABLE_OBJECTS

extern int etext, edata;	/* Info to be used in creating rvm segment */
/*PRIVATE */char *_Rvm_Log_Device;
/*PRIVATE */char *_Rvm_Data_Device;
/*PRIVATE */rvm_offset_t _Rvm_DataLength;
/*PRIVATE */int _Rvm_Truncate = 0;
/*PRIVATE */char *cam_log_file;
/*PRIVATE */int camlog_fd;
/*PRIVATE */char camlog_record[SIZEOF_LARGEDISKVNODE + 8 + sizeof(VolumeDiskData)];
/* PRIVATE */ char *_DEBUG_p;
/*PRIVATE */int nodumpvm = FALSE;
int prottrunc = FALSE;
/* PRIVATE */int MallocTrace = FALSE;
/* PRIVATE */void rds_printer(char *fmt ...);

/* vicetab */
#define VCT "/vice/db/vicetab"

/* PDB stuff. */
PRIVATE int pdbtime = 0;
#define PDB "/vice/db/vice.pdb"
#define PCF "/vice/db/vice.pcf"

/* Token stuff. */
PRIVATE int keytime = 0;
#define KEY1 "/vice/db/auth2.tk"
#define KEY2 "/vice/db/auth2.tk.BAK"

/* (Worker) LWP statistics.  Currently unused. */
#define MAXLWP 16
PRIVATE RPC2_Integer LastOp[MAXLWP];
PRIVATE int StackUsed[MAXLWP];
PRIVATE int StackAllocated[MAXLWP];
PRIVATE ClientEntry *CurrentClient[MAXLWP];

/* *****  Private routines  ***** */

PRIVATE void ServerLWP(int *);
PRIVATE void ResLWP(int *);
PRIVATE void CallBackCheckLWP();
PRIVATE void CheckLWP();

PRIVATE void ClearCounters();
PRIVATE void FileMsg();
PRIVATE void SetDebug();
PRIVATE void ResetDebug();
PRIVATE void ShutDown();
PRIVATE int pushlog();

PRIVATE int ParseArgs(int, char **);
PRIVATE void NewParms(int);
PRIVATE void InitServerKeys(char *, char *);
PRIVATE void DaemonizeSrv(void);
static void InitializeServerRVM(void *initProc,char *name);

#ifdef RVMTESTING
#include <rvmtesting.h>
#endif RVMTESTING

/* Signal handlers in Linux will not be passed the arguments code and scp */
#ifdef __BSD44__
struct sigcontext OldContext; /* zombie() saves original context here */
#endif
extern void dumpvm();

  /* We need to have this zombie because of Camelot and the IBM-RT stack bogosity.
       The zombie can be attached to via gdb, and the context set to OldContext.
       Backtraces will then make sense.
       Otherwise the gap in the RT stack causes the backtrace to end prematurely.
    */

/* Signal handlers in Linux will not be passed the arguments code and scp */
#ifndef	__BSD44__
void zombie(int sig) {
#else
void zombie(int sig, int code, struct sigcontext *scp) {
    bcopy(scp, &OldContext, sizeof(struct sigcontext));
#endif

#ifndef  __BSD44__
    LogMsg(0, 0, stdout,  "****** FILE SERVER INTERRUPTED BY SIGNAL %d ******", sig);
#else
    LogMsg(0, 0, stdout,  "****** FILE SERVER INTERRUPTED BY SIGNAL %d CODE %d ******", sig, code);
#endif    
    LogMsg(0, 0, stdout,  "****** Aborting outstanding transactions, stand by...");
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
	    LogMsg(0, 0, stdout,  "rvm_query returned %s", rvm_return(ret));	
	else {
	    LogMsg(0, 0, stdout,  "Uncommitted transactions: %d", curopts.n_uncommit);
	
	    for (i = 0; i < curopts.n_uncommit; i++) {
		rvm_abort_transaction(&(curopts.tid_array[i]));
		if (ret != RVM_SUCCESS) 
		    LogMsg(0, 0, stdout,  "ERROR: abort failed, code: %s", rvm_return(ret));
	    }
	    
	    ret = rvm_query(&curopts, NULL);
	    if (ret != RVM_SUCCESS)
		LogMsg(0, 0, stdout,  "rvm_query returned %s", rvm_return(ret));	
	    else 
		LogMsg(0, 0, stdout,  "Uncommitted transactions: %d", curopts.n_uncommit);
	}
	rvm_free_options(&curopts);

	if (!nodumpvm)
	    dumpvm(); /* sanity check rvm recovery. */
    }
    
    LogMsg(0, 0, stdout, "Becoming a zombie now ........");
    LogMsg(0, 0, stdout, "You may use gdb to attach to %d", getpid());
    {
	int      living_dead = 1;
	sigset_t mask;

	sigemptyset(&mask);
	while (living_dead) {
	    sigsuspend(&mask); /* pending gdb attach */
	}
    }
}





/* The real stuff! */

main(int argc, char *argv[])
{
    char    sname[20];
    int     i;
    int     len;
    FILE   *file;
    struct stat buff;
    PROCESS parentPid, serverPid, resPid, smonPid, resworkerPid;
    RPC2_PortalIdent portal1, *portallist[1];
    RPC2_SubsysIdent server;
    SFTP_Initializer sei;
    ProgramType *pt;


    if(ParseArgs(argc,argv)) {
	LogMsg(0, 0, stdout, "usage: srv [-d (debug level)] [-p (number of processes)] ");
	LogMsg(0, 0, stdout, "[-b (buffers)] [-l (large vnodes)] [-s (small vnodes)]");
	LogMsg(0, 0, stdout, "[-k (stack size)] [-w (call back wait interval)]");
	LogMsg(0, 0, stdout, "[-r (RPC retry count)] [-o (RPC timeout value)]");
	LogMsg(0, 0, stdout, "[-c (check interval)] [-t (number of RPC trace buffers)]");
	LogMsg(0, 0, stdout, "[-noauth] [-forcesalvage] [-quicksalvage]");
	LogMsg(0, 0, stdout, "[-cp (connections in process)] [-cm (connections max)");
	LogMsg(0, 0, stdout, "[-cam] [-nc] [-rvm logdevice datadevice length] [-nores] [-trunc percent]");
	LogMsg(0, 0, stdout, " [-nocmp] [-nopy] [-nodumpvm] [-nosalvageonshutdown] [-mondhost hostname] [-mondportal portalnumber]");
	LogMsg(0, 0, stdout, "[-debarrenize] [-optstore]");
	LogMsg(0, 0, stdout, " [-rvmopt] [-newchecklevel checklevel] [-canonicalize] [-usenscclock");

	exit(-1);
    }

    setmyname(argv[0]);

    

    len = (int) strlen(argv[0]);
    for(i = 0;i < len;i++) {
	*(argv[0]+i) = ' ';
    }
    strcpy(argv[0],"srv");

    if(chdir("/vice/srv")) {
	LogMsg(0, 0, stdout, "could not cd to /vice/srv - exiting");
	exit(-1);
    }

    NewParms(1);
    unlink("/vice/srv/NEWSRV");

    freopen("SrvLog","a+",stdout);
    freopen("SrvErr","a+",stderr);
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
#endif _TIMECALLS_

    /* open the file where records are written at end of transaction */
    if (cam_log_file){
	camlog_fd = open(cam_log_file, O_RDWR | O_TRUNC | O_CREAT, 0777);
	if (camlog_fd < 0) perror("Error opening cam_log_file\n");
    }
    VInitServerList();	/* initialize server info for volume pkg */

    /* Notify log of sizes of text and data regions. */
#ifndef __CYGWIN32__
    LogMsg(0, 0, stdout, "Server etext 0x%x, edata 0x%x", &etext, &edata);
#endif
    switch (RvmType) {
        case UFS	   :
 	case RAWIO     	   : LogMsg(0, 0, stdout, "RvmType is Rvm"); break;
	case VM		   : LogMsg(0, 0, stdout, "RvmType is NoPersistence"); break;
	case UNSET	   : LogMsg(0, 0, stdout, "No RvmType selected!"); exit(-1);
    }

    LogMsg(0, SrvDebugLevel, stdout, "Main process doing a LWP_Init()");
    assert(LWP_Init(LWP_VERSION, LWP_NORMAL_PRIORITY,&parentPid)==LWP_SUCCESS);

    /* using rvm - so set the per thread data structure for executing transactions */
    rvm_perthread_t rvmptt;
    if (RvmType == RAWIO || RvmType == UFS) {
	rvmptt.tid = NULL;
	rvmptt.list.table = NULL;
	rvmptt.list.count = 0;
	rvmptt.list.size = 0;
	rvmptt.die = NULL;
	RVM_SET_THREAD_DATA(&rvmptt);
	assert(RVM_THREAD_DATA != 0);
	LogMsg(0, SrvDebugLevel, stdout, 
	       "Main thread just did a RVM_SET_THREAD_DATA\n");
    }

#ifdef PERFORMANCE 
    /* initialize the array of thread_t to 0 - Puneet */
    for (i = 0; i < NLWPS; i ++)
	lwpth[i] = (thread_t)0;
#endif PERFORMANCE

    stat(KEY1, &buff);
    keytime = (int)buff.st_mtime;
    InitServerKeys(KEY1, KEY2);

    portal1.Tag = RPC2_PORTALBYNAME;
    strcpy(portal1.Value.Name, "coda_filesrv");
    portallist[0] = &portal1;

    SFTP_SetDefaults(&sei);
    /* set optimal window size and send ahead parameters */
    sei.WindowSize = SrvWindowSize;
    sei.AckPoint = sei.SendAhead = SrvSendAhead;
    sei.EnforceQuota = 1;
    sei.Portal.Tag = RPC2_PORTALBYINETNUMBER;
    sei.Portal.Value.InetPortNumber = htons(1362);	/* XXX -JJK */
    SFTP_Activate(&sei);
    struct timeval to;
    to.tv_sec = timeout;
    to.tv_usec = 0;
    assert(RPC2_Init(RPC2_VERSION, 0, portallist, 1, retrycnt, &to) == RPC2_SUCCESS);
    RPC2_InitTraceBuffer(trace);
    RPC2_Trace = trace;

    InitPartitions(VCT);
    InitializeServerRVM(NULL, "codaserver"); 

    /* Trace mallocs and frees in the persistent heap if requested. */
    if (MallocTrace) {	
      rds_trace_on(rds_printer);
      rds_trace_dump_heap();
    }
    
    coda_init();

    FileMsg();


    assert(file = fopen("pid","w"));
    fprintf(file,"%d",getpid());
    fclose(file);
    /* Init per LWP process functions */
    for(i=0;i<MAXLWP;i++)
	LastOp[i] = StackAllocated[i] = StackUsed[i] = 0;
    AL_DebugLevel = SrvDebugLevel/10;
    VolDebugLevel = SrvDebugLevel;
    RPC2_Perror = 1;
    nice(-5);
    assert(DInit(buffs) == 0);
    DirHtbInit();  /* initialize rvm dir package */

    stat(PDB, &buff);
    pdbtime = (int)buff.st_mtime;
    assert(AL_Initialize(AL_VERSION, PDB, PCF) == 0);

    assert(AL_NameToId("Administrators", &SystemId) == 0);

 /* Initialize failure package */
    Fail_Initialize("file", 0);
    Fcon_Init();

    /* tag main fileserver lwp for volume package */
    pt = (ProgramType *) malloc(sizeof(ProgramType));
    *pt = fileServer;
    assert(LWP_NewRock(FSTAG, (char *)pt) == LWP_SUCCESS);

    InitCallBack();
    VInitVolumePackage(large,small, ForceSalvage);
    CheckVRDB();


    InitCopPendingTable();

    /* Initialize the lock queue and the resolution comm package */
    InitLockQueue();
    ResCommInit();
    server.Tag = RPC2_SUBSYSBYID;
    server.Value.SubsysId = RESOLUTIONSUBSYSID;
    assert(RPC2_Export(&server) == RPC2_SUCCESS);

    server.Tag = RPC2_SUBSYSBYNAME;
    strcpy(server.Value.Name, "Vice2-FileServer");
    assert(RPC2_Export(&server) == RPC2_SUCCESS);
    ClearCounters();

    assert(LWP_CreateProcess((PFIC)CallBackCheckLWP, stack*1024, LWP_NORMAL_PRIORITY,
	    (char *)&cbwait, "CheckCallBack", &serverPid) == LWP_SUCCESS);
	    
    assert(LWP_CreateProcess((PFIC)CheckLWP, stack*1024, LWP_NORMAL_PRIORITY,
	    (char *)&chk, "Check", &serverPid) == LWP_SUCCESS);
	    
    for (i=0; i < lwps; i++) {
	sprintf(sname, "ServerLWP-%d",i);
	assert(LWP_CreateProcess((PFIC)ServerLWP, stack*1024, LWP_NORMAL_PRIORITY,
		(char *)&i, sname, &serverPid) == LWP_SUCCESS);
    }

    /* set up resolution threads */
    for (i = 0; i < 2; i++){
	sprintf(sname, "ResLWP-%d", i);
	assert(LWP_CreateProcess((PFIC)ResLWP, stack*1024, 
				 LWP_NORMAL_PRIORITY, (char *)&i, 
				 sname, &resPid) == LWP_SUCCESS);
    }
    extern void ResCheckServerLWP();
    sprintf(sname, "ResCheckSrvrLWP");
    assert(LWP_CreateProcess((PFIC)ResCheckServerLWP, stack*1024,
			      LWP_NORMAL_PRIORITY, (char *)&i, 
			      sname, &resPid) == LWP_SUCCESS);

    extern void ResCheckServerLWP_worker();
    sprintf(sname, "ResCheckSrvrLWP_worker");
    assert(LWP_CreateProcess((PFIC)ResCheckServerLWP_worker, stack*1024,
			      LWP_NORMAL_PRIORITY, (char *)&i, 
			      sname, &resworkerPid) == LWP_SUCCESS);
    /* Set up volume utility subsystem (spawns 2 lwps) */
    LogMsg(29, SrvDebugLevel, stdout, "fileserver: calling InitvolUtil");
    extern void InitVolUtil(int stacksize);

    InitVolUtil(stack*1024);
    LogMsg(29, SrvDebugLevel, stdout, "fileserver: returning from InitvolUtil");

    extern void SmonDaemon();
    sprintf(sname, "SmonDaemon");
    assert(LWP_CreateProcess((PFIC)SmonDaemon, stack*1024,
			     LWP_NORMAL_PRIORITY, (char *)&smonPid,
			     sname, &smonPid) == LWP_SUCCESS);
#ifdef PERFORMANCE
#ifndef OLDLWP
    /* initialize global array of thread_t for timing - Puneet */
    if (task_threads(task_self(), &thread_list, &thread_count) != KERN_SUCCESS)
	LogMsg(0, 0, stdout, "*****Couldn't get threads for task ");
    else
	LogMsg(0, 0, stdout, "Thread ids for %d threads initialized", thread_count);
#endif
#endif PERFORMANCE
    struct timeval tp;
    struct timezone tsp;
    TM_GetTimeOfDay(&tp, &tsp);
#ifdef	__linux__
    LogMsg(0, 0, stdout,"File Server started %s", ctime((const long int *)&tp.tv_sec));
#else
    LogMsg(0, 0, stdout,"File Server started %s", ctime(&tp.tv_sec));
#endif
    StartTime = (unsigned int)tp.tv_sec;
    assert(LWP_WaitProcess((char *)&parentPid) == LWP_SUCCESS);

}


#define BADCLIENT 1000
PRIVATE void ServerLWP(int *Ident)
{
    RPC2_RequestFilter myfilter;
    RPC2_PacketBuffer * myrequest;
    RPC2_Handle mycid;
    long    rc;
    int     lwpid;
    char    area[256];
    char   *userName;
    char   *workName;
    ClientEntry *client = 0;
    ProgramType *pt;

    /* using rvm - so set the per thread data structure */
    rvm_perthread_t rvmptt;
    if (RvmType == RAWIO || RvmType == UFS) {
	rvmptt.tid = NULL;
	rvmptt.list.table = NULL;
	rvmptt.list.count = 0;
	rvmptt.list.size = 0;
	rvmptt.die = NULL;
	RVM_SET_THREAD_DATA(&rvmptt);
	assert(RVM_THREAD_DATA != 0);
	LogMsg(0, SrvDebugLevel, stdout, 
	       "ServerLWP %d just did a RVM_SET_THREAD_DATA\n", 
	       *Ident);
    }


    myfilter.FromWhom = ONESUBSYS;
    myfilter.OldOrNew = OLDORNEW;
    myfilter.ConnOrSubsys.SubsysId = getsubsysbyname("Vice2-FileServer");
    lwpid = *Ident;
    LogMsg(1, SrvDebugLevel, stdout, "Starting Worker %d", lwpid);

    /* tag fileserver lwps with rock */
    pt = (ProgramType *) malloc(sizeof(ProgramType));
    *pt = fileServer;
    assert(LWP_NewRock(FSTAG, (char *)pt) == LWP_SUCCESS);

    while (1) {
	LastOp[lwpid] = 0;
	CurrentClient[lwpid] = (ClientEntry *)0;
	if ((rc = RPC2_GetRequest(&myfilter, &mycid, &myrequest, 0, (long (*)())GetKeysFromToken, RPC2_XOR, NULL))
		== RPC2_SUCCESS) {
	    if (RPC2_GetPrivatePointer(mycid, (char **)&client) != RPC2_SUCCESS) 
		client = 0;
	    
	    if (client && client->RPCid != mycid) {
	        LogMsg(0, 0, stdout, "Invalid client pointer from GetPrivatePointer");
		myrequest->Header.Opcode = BADCLIENT; /* ignore request & Unbind */
		client = 0;
	    }
	    LastOp[lwpid] = myrequest->Header.Opcode;
	    CurrentClient[lwpid] = client;
	    if (client == 0) {
	        userName = workName = area;
		strcpy(area, "NA");
	    } else {
		userName = area;
		strcpy(userName, client->UserName);
		workName = userName + strlen(userName) + 1;
		strcpy(workName, client->VenusId->HostName);
		client->LastCall = client->VenusId->LastCall = (unsigned int)time(0);
		/* the next time is used to eliminate GetTime calls from active stat */
		if (myrequest->Header.Opcode != GETTIME)
		    client->VenusId->ActiveCall = client->LastCall;
		client->LastOp = (int)myrequest->Header.Opcode;
     	    }

	    LogMsg(5, SrvDebugLevel, stdout, "Worker %d received request %d on cid %d for %s at %s",
		    lwpid, myrequest->Header.Opcode, mycid, userName, workName);
	    if (myrequest->Header.Opcode > 0 && myrequest->Header.Opcode < FETCHDATA) {
		Counters[TOTAL]++;
		Counters[myrequest->Header.Opcode]++;
		if (!(Counters[TOTAL] & 0xFFF)) {
		    PrintCounters(stdout);
		}
	    }
	    rc = srv_ExecuteRequest(mycid, myrequest, 0);
#ifdef PERFORMANCE 
#ifndef OLDLWP
	    if (SrvDebugLevel > 0){
		/* check how much time it took for call */
		if(thread_info(th_id, THREAD_BASIC_INFO, (thread_info_t)thrinfo, &info_cnt) != KERN_SUCCESS)
		    LogMsg(1, SrvDebugLevel, stdout, "Thread Info failed for %d", *Ident);
		else{
		    time_value_sub(thrinfo->user_time, save_utime, ptime);
		    LogMsg(1, SrvDebugLevel, stdout, "RES_USAGE: utime = %d secs, %d usecs", ptime.seconds, ptime.microseconds);
		    /* save new time in save area */
		    save_utime.seconds = thrinfo->user_time.seconds;
		    save_utime.microseconds = thrinfo->user_time.microseconds;
		    time_value_sub(thrinfo->system_time, save_stime, ptime);
		    LogMsg(1, SrvDebugLevel, stdout, " stime = %d secs, %d usecs", ptime.seconds, ptime.microseconds);
		    save_stime.seconds = thrinfo->system_time.seconds;
		    save_stime.microseconds = thrinfo->system_time.microseconds;
		}
	    }
#endif OLDLWP
#endif PERFORMANCE
	    if (rc) {
		LogMsg(0, 0, stdout, "srv.c request %d for %s at %s failed: %s",
			myrequest->Header.Opcode, userName, workName, ViceErrorMsg((int)rc));
		if(rc <= RPC2_ELIMIT) {
		    if(client && client->RPCid == mycid && !client->DoUnbind) {
			ObtainWriteLock(&(client->VenusId->lock));
			DeleteClient(client);
			ReleaseWriteLock(&(client->VenusId->lock));
		    }
		}
	    }
	    if(client) {
		if(client->DoUnbind) {
		    LogMsg(0, 0, stdout, "Worker%d: Unbinding RPC connection %d",
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
	    if (myrequest->Header.Opcode == BADCLIENT) /* if bad client ptr Unbind */ {
		LogMsg(0, 0, stdout, "Worker%d: Unbinding RPC connection %d (BADCLIENT)",
					    lwpid, mycid);
		RPC2_Unbind(mycid);
	    }
	}
	else {
	    LogMsg(0, 0, stdout,"RPC2_GetRequest failed with %s",ViceErrorMsg((int)rc));
	}
    }
}

PRIVATE void ResLWP(int *Ident){
    RPC2_RequestFilter myfilter;
    RPC2_Handle	mycid;
    RPC2_PacketBuffer *myrequest;
    ProgramType pt;
    register long rc;

    /* using rvm - so set the per thread data structure for executing transactions */
    rvm_perthread_t rvmptt;
    if (RvmType == RAWIO || RvmType == UFS) {
	rvmptt.tid = NULL;
	rvmptt.list.table = NULL;
	rvmptt.list.count = 0;
	rvmptt.list.size = 0;
	rvmptt.die = NULL;
	RVM_SET_THREAD_DATA(&rvmptt);
	assert(RVM_THREAD_DATA != 0);
	LogMsg(0, SrvDebugLevel, stdout, 
	       "ResLWP-%d just did a RVM_SET_THREAD_DATA\n",
	       *Ident);
    }

    pt = fileServer;
    assert(LWP_NewRock(FSTAG, (char *)&pt) == LWP_SUCCESS);

    myfilter.FromWhom = ONESUBSYS;
    myfilter.OldOrNew = OLDORNEW;
    myfilter.ConnOrSubsys.SubsysId = RESOLUTIONSUBSYSID;
    LogMsg(1, SrvDebugLevel, stdout, "Starting ResLWP worker %d", *Ident);

    while(1) {
	mycid = 0;
	rc = RPC2_GetRequest(&myfilter, &mycid, &myrequest, 
			     NULL, NULL, NULL, NULL);
	if (rc == RPC2_SUCCESS) {
	    LogMsg(9, SrvDebugLevel, stdout, "ResLWP %d Received request %d", 
		    *Ident, myrequest->Header.Opcode);
	    rc = resolution_ExecuteRequest(mycid, myrequest, NULL);
	    if (rc) 
		LogMsg(0, 0, stdout, "ResLWP %d: request %d failed with %s",
			*Ident, ViceErrorMsg((int)rc));
	}
	else 
	    LogMsg(0, 0, stdout, "RPC2_GetRequest failed with %s", 
		    ViceErrorMsg((int)rc));
    }

}

PRIVATE void CallBackCheckLWP()
{
    struct timeval  time;
    ProgramType *pt;

    /* tag lwps as fsUtilities */
    pt = (ProgramType *) malloc(sizeof(ProgramType));
    *pt = fsUtility;
    assert(LWP_NewRock(FSTAG, (char *)pt) == LWP_SUCCESS);

    /* using rvm - so set the per thread data structure for executing transactions */
    rvm_perthread_t rvmptt;
    if (RvmType == RAWIO || RvmType == UFS) {
	rvmptt.tid = NULL;
	rvmptt.list.table = NULL;
	rvmptt.list.count = 0;
	rvmptt.list.size = 0;
	rvmptt.die = NULL;
	RVM_SET_THREAD_DATA(&rvmptt);
	assert(RVM_THREAD_DATA != 0);
	LogMsg(0, SrvDebugLevel, stdout, 
	       "CallBackCheckLWP just did a RVM_SET_THREAD_DATA\n");
    }

    LogMsg(1, SrvDebugLevel, stdout, "Starting CallBackCheck process");
    time.tv_sec = cbwait;
    time.tv_usec = 0;

    while (1) {
	if (IOMGR_Select(0, 0, 0, 0, &time) == 0) {
	    LogMsg(2, SrvDebugLevel, stdout, "Checking for dead venii");
	    CallBackCheck();
	    LogMsg(2, SrvDebugLevel, stdout, "Set disk usage statistics");
	    VSetDiskUsage();
	    if(time.tv_sec != cbwait) time.tv_sec = cbwait;
	}
    }
}


PRIVATE void CheckLWP()
{
    struct timeval  time;
    struct timeval  tpl;
    struct timezone tspl;
    ProgramType *pt;

    /* using rvm - so set the per thread data structure for executing transactions */
    rvm_perthread_t rvmptt;
    if (RvmType == RAWIO || RvmType == UFS) {
	rvmptt.tid = NULL;
	rvmptt.list.table = NULL;
	rvmptt.list.count = 0;
	rvmptt.list.size = 0;
	rvmptt.die = NULL;
	RVM_SET_THREAD_DATA(&rvmptt);
	assert(RVM_THREAD_DATA != 0);
	LogMsg(0, SrvDebugLevel, stdout, 
	       "CheckLWP just did a RVM_SET_THREAD_DATA\n");

    }
    
    /* tag lwps as fsUtilities */
    pt = (ProgramType *) malloc(sizeof(ProgramType));
    *pt = fsUtility;
    assert(LWP_NewRock(FSTAG, (char *)pt) == LWP_SUCCESS);


    LogMsg(1, SrvDebugLevel, stdout, "Starting Check process");
    time.tv_sec = chk;
    time.tv_usec = 0;

    while (1) {
	if (IOMGR_Select(0, 0, 0, 0, &time) == 0) {
	    if(ViceShutDown) {
		ProgramType *pt, tmp_pt;

		TM_GetTimeOfDay(&tpl, &tspl);
#ifdef	__linux__
		LogMsg(0, 0, stdout, "Shutting down the File Server %s", ctime((const long int *)&tpl.tv_sec));
#else
		LogMsg(0, 0, stdout, "Shutting down the File Server %s", ctime(&tpl.tv_sec));
#endif
		assert(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);
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

PRIVATE void ShutDown()
{
    int     fd;
    int camstatus = 0;

    PrintCounters(stdout);

    if (SalvageOnShutdown) {
	LogMsg(9, SrvDebugLevel, stdout, "Unlocking volutil lock...");
	int fvlock = open("/vice/vol/volutil.lock", O_CREAT|O_RDWR, 0666);
	assert(fvlock >= 0);
	while (flock(fvlock, LOCK_UN) != 0);
	LogMsg(9, SrvDebugLevel, stdout, "Done");
	close(fvlock);
	
	ProgramType *pt, tmppt;
	assert(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);
	tmppt = *pt;
	*pt = salvager;	/* MUST set *pt to salvager before vol_salvage */
	assert(S_VolSalvage(0, NULL, 0, 1, 1, 0) == 0);
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

    stat(PDB, &vbuff);

    if(pdbtime != vbuff.st_mtime) {
	pdbtime = (int)vbuff.st_mtime;
	assert(AL_Initialize(AL_VERSION, PDB, PCF) == 0);
    }
    stat(KEY1, &vbuff);
    if(keytime != vbuff.st_mtime) {
	keytime = (int)vbuff.st_mtime;
	InitServerKeys(KEY1,KEY2);
    }
    VCheckVLDB();
    CheckVRDB();
    CheckVSGDB();
    LogMsg(0, 0, stdout, "New Data Base received");

}


PRIVATE void ClearCounters()
{
    register	int	i;

    for(i=0; i<MAXCNTRS; i++)
        Counters[i] = 0;
}


void PrintCounters(FILE *fp)
{
    int	dirbuff, dircall, dirio;
    struct timeval  tpl;
    struct timezone tspl;
    int seconds;
    int workstations;
    int activeworkstations;

    TM_GetTimeOfDay(&tpl, &tspl);
    Statistics = 1;
    LogMsg(0, 0, fp,
	   "Total operations for File Server = %d : time = %s",
	    Counters[TOTAL], ctime((long *)&tpl.tv_sec));
    LogMsg(0, 0, fp,
	   "Vice was last started at %s", ctime((long *)&StartTime));

    LogMsg(0, 0, fp, "ConnectFS %d", Counters[CONNECT]);
    LogMsg(0, 0, fp, "NewConnectFS %d", Counters[NEWCONNECTFS]);
    LogMsg(0, 0, fp, "DisconnectFS %d", Counters[DISCONNECT]);

    LogMsg(0, 0, fp, "Fetch %d", Counters[ViceFetch_OP]);
    LogMsg(0, 0, fp, "Store %d", Counters[ViceNewStore_OP] + Counters[ViceNewVStore_OP]);
    LogMsg(0, 0, fp, "ValidateAttrs %d", Counters[VALIDATEATTRS]);

    LogMsg(0, 0, fp, "Remove %d", Counters[REMOVE]);
    LogMsg(0, 0, fp, "Create %d", Counters[CREATE]);
    LogMsg(0, 0, fp, "Rename %d", Counters[RENAME]);
    LogMsg(0, 0, fp, "SymLink %d", Counters[SYMLINK]);
    LogMsg(0, 0, fp, "Link %d", Counters[LINK]);
    LogMsg(0, 0, fp, "MakeDir %d", Counters[MAKEDIR]);
    LogMsg(0, 0, fp, "RemoveDir %d", Counters[REMOVEDIR]);
    LogMsg(0, 0, fp, "RemoveCallBack %d", Counters[REMOVECALLBACK]);
    LogMsg(0, 0, fp, "GetRootVolume %d", Counters[GETROOTVOLUME]);
    LogMsg(0, 0, fp, "SetRootVolume %d", Counters[SETROOTVOLUME]);
    LogMsg(0, 0, fp, "GetVolumeStatus %d", Counters[GETVOLUMESTAT]);
    LogMsg(0, 0, fp, "SetVolumeStatus %d", Counters[SETVOLUMESTAT]);
    
    LogMsg(0, 0, fp, "GetTime %d", Counters[GETTIME]); 
    LogMsg(0, 0, fp, "GetStatistics %d", Counters[GETSTATISTICS]); 
    LogMsg(0, 0, fp, "GetVolumeInfo %d", Counters[ViceGetVolumeInfo_OP]); 
    LogMsg(0, 0, fp, "EnableGroup %d", Counters[ViceEnableGroup_OP]); 
    LogMsg(0, 0, fp, "DisableGroup %d", Counters[ViceDisableGroup_OP]); 
    LogMsg(0, 0, fp, "Probe %d", Counters[ViceProbe_OP]); 
    LogMsg(0, 0, fp, "AllocFids %d", Counters[ALLOCFIDS]); 
    LogMsg(0, 0, fp, "COP2 %d", Counters[ViceCOP2_OP]); 
    LogMsg(0, 0, fp, "Resolve %d", Counters[RESOLVE]);
    LogMsg(0, 0, fp, "Repair %d", Counters[REPAIR]);
    LogMsg(0, 0, fp, "SetVV %d", Counters[SETVV]);
    LogMsg(0, 0, fp, "Reintegrate %d", Counters[REINTEGRATE]);

    LogMsg(0, 0, fp, "GetVolVS %d", Counters[GETVOLVS]);
    LogMsg(0, 0, fp, "ValidateVols %d", Counters[VALIDATEVOLS]);

    seconds = Counters[FETCHTIME]/1000;
    if(seconds <= 0) 
	seconds = 1;
    LogMsg(0, 0, fp,
	   "Total FetchDatas = %d, bytes transfered = %d, transfer rate = %d bps",
	   Counters[FETCHDATAOP], Counters[FETCHDATA],
	   Counters[FETCHDATA]/seconds);
    LogMsg(0, 0, fp,
	   "Fetched files <%dk = %d; <%dk = %d; <%dk = %d; <%dk = %d; >%dk = %d.",
	   SIZE1 / 1024, Counters[FETCHD1], SIZE2 / 1024,
	   Counters[FETCHD2], SIZE3 / 1024, Counters[FETCHD3], SIZE4 / 1024,
	   Counters[FETCHD4], SIZE4 / 1024, Counters[FETCHD5]);
    seconds = Counters[STORETIME]/1000;
    if(seconds <= 0) 
	seconds = 1;
    LogMsg(0, 0, fp,
	   "Total StoreDatas = %d, bytes transfered = %d, transfer rate = %d bps",
	   Counters[STOREDATAOP], Counters[STOREDATA],
	   Counters[STOREDATA]/seconds);
    LogMsg(0, 0, fp,
	   "Stored files <%dk = %d; <%dk = %d; <%dk = %d; <%dk = %d; >%dk = %d.",
	   SIZE1 / 1024, Counters[STORED1], SIZE2 / 1024,
	   Counters[STORED2], SIZE3 / 1024, Counters[STORED3], SIZE4 / 1024,
	   Counters[STORED4], SIZE4 / 1024, Counters[STORED5]);
    VPrintCacheStats();
    VPrintDiskStats();
    DStat(&dirbuff, &dircall, &dirio);
    LogMsg(0, 0, fp,
	   "With %d directory buffers; %d reads resulted in %d read I/Os",
	   dirbuff, dircall, dirio);
    LogMsg(0, 0, fp,
	   "RPC Total bytes:     sent = %u, received = %u",
	   rpc2_Sent.Bytes + rpc2_MSent.Bytes + sftp_Sent.Bytes + sftp_MSent.Bytes,
	   rpc2_Recvd.Bytes + rpc2_MRecvd.Bytes + sftp_Recvd.Bytes + sftp_MRecvd.Bytes);
    LogMsg(0, 0, fp, 
	   "\tbytes sent: rpc = %d, multirpc = %d, sftp = %d, sftp multicasted = %d",
	   rpc2_Sent.Bytes, rpc2_MSent.Bytes, sftp_Sent.Bytes, sftp_MSent.Bytes);
    LogMsg(0, 0, fp, 
	   "\tbytes received: rpc = %d, multirpc = %d, sftp = %d, sftp multicasted = %d",
	   rpc2_Recvd.Bytes, rpc2_MRecvd.Bytes, sftp_Recvd.Bytes, sftp_MRecvd.Bytes);
    LogMsg(0, 0, fp,
	   "RPC Total packets:   sent = %d, received = %d",
	   rpc2_Sent.Total + rpc2_MSent.Total + sftp_Sent.Total + sftp_MSent.Total,
	   rpc2_Recvd.Total + rpc2_MRecvd.Total + sftp_Recvd.Total + sftp_MRecvd.Total);
    LogMsg(0, 0, fp, 
	   "\tpackets sent: rpc = %d, multirpc = %d, sftp = %d, sftp multicasted = %d",
	   rpc2_Sent.Total, rpc2_MSent.Total, sftp_Sent.Total, sftp_MSent.Total);
    LogMsg(0, 0, fp, 
	   "\tpackets received: rpc = %d, multirpc = %d, sftp = %d, sftp multicasted = %d",
	   rpc2_Recvd.Total, rpc2_MRecvd.Total, sftp_Recvd.Total, sftp_MRecvd.Total);
    LogMsg(0, 0, fp,
	   "RPC Packets retried = %d, Invalid packets received = %d, Busies sent = %d",
	   rpc2_Sent.Retries - rpc2_Recvd.GoodBusies,
	   rpc2_Recvd.Total + rpc2_MRecvd.Total - 
	        rpc2_Recvd.GoodRequests - rpc2_Recvd.GoodReplies - 
		rpc2_Recvd.GoodBusies - rpc2_MRecvd.GoodRequests,
	   rpc2_Sent.Busies);
    LogMsg(0, 0, fp,
	   "RPC Requests %d, Good Requests %d, Replies %d, Busies %d",
	   rpc2_Recvd.Requests, rpc2_Recvd.GoodRequests, 
	   rpc2_Recvd.GoodReplies, rpc2_Recvd.GoodBusies);
    LogMsg(0, 0, fp,
	   "RPC Counters: CCount %d; Unbinds %d; FConns %d; AConns %d; GCConns %d",
	   rpc2_ConnCount, rpc2_Unbinds, rpc2_FreeConns, 
	   rpc2_AllocConns, rpc2_GCConns);
    LogMsg(0, 0, fp,
	   "RPC Creation counts: Conn %d; SL %d; PB Small %d, Med %d, Large %d; SS %d",
	   rpc2_ConnCreationCount, rpc2_SLCreationCount, rpc2_PBSmallCreationCount,
	   rpc2_PBMediumCreationCount, rpc2_PBLargeCreationCount, rpc2_SSCreationCount);
    LogMsg(0, 0, fp,
	   "RPC2 In Use: Conn %d; SS %d",
	   rpc2_ConnCount, rpc2_SSCount);
    LogMsg(0, 0, fp,
	   "RPC2 PB: InUse %d, Hold %d, Freeze %d, SFree %d, MFree %d, LFree %d", 
	   rpc2_PBCount, rpc2_PBHoldCount, rpc2_PBFreezeCount,
	   rpc2_PBSmallFreeCount, rpc2_PBMediumFreeCount, rpc2_PBLargeFreeCount);
    LogMsg(0, 0, fp,
	   "RPC2 HW:  Freeze %d, Hold %d", rpc2_FreezeHWMark, rpc2_HoldHWMark);
    LogMsg(0, 0, fp,
	   "SFTP:	datas %d, datar %d, acks %d, ackr %d, retries %d, duplicates %d",
	   sftp_datas, sftp_datar, sftp_acks, sftp_ackr, sftp_retries, sftp_duplicates);
    LogMsg(0, 0, fp,
	   "SFTP:  timeouts %d, windowfulls %d, bogus %d, didpiggy %d",
	   sftp_timeouts, sftp_windowfulls, sftp_bogus, sftp_didpiggy);
    LogMsg(0, 0, fp,
	   "Total CB entries= %d, blocks = %d; and total file entries = %d, blocks = %d",
	   CBEs, CBEBlocks, FEs, FEBlocks);
    /*    ProcSize = sbrk(0) >> 10;*/
    LogMsg(0, 0, fp,
	   "There are currently %d connections in use",
	   CurrentConnections);
    GetWorkStats(&workstations, &activeworkstations, (unsigned)tpl.tv_sec-15*60);
    LogMsg(0, 0, fp,
	   "There are %d workstations and %d are active (req in < 15 mins)",
	   workstations, activeworkstations);
    if(supported && !GetEtherStats()) {
	LogMsg(0, 0, fp,
	       "Ether Total bytes: sent = %u, received = %u",
	       etherBytesWritten, etherBytesRead);
	LogMsg(0, 0, fp,
	       "Ether Packets:     sent = %d, received = %d, errors = %d",
	       etherWrites, etherInterupts, etherRetries);
    }
    
    if (RvmType == RAWIO || RvmType == UFS) {
	LogMsg(0, SrvDebugLevel, stdout,
	       "Printing RVM statistics\n");
	rvm_statistics_t rvmstats;
        rvm_init_statistics(&rvmstats);
	RVM_STATISTICS(&rvmstats);
	rvm_print_statistics(&rvmstats, fp);
	rvm_free_statistics(&rvmstats);
    }
    Statistics = 0;
    LogMsg(0,0,fp, "done\n");
#ifdef _TIMECALLS_
    /* print the histograms timing the operations */
    LogMsg(0, SrvDebugLevel, fp, "Operation histograms\n");
    LogMsg(0, SrvDebugLevel, fp, "Create histogram:\n");
    PrintHisto(fp, &Create_Total_hg);
    ClearHisto(&Create_Total_hg);
    LogMsg(0, SrvDebugLevel, fp, "Remove histogram:\n");
    PrintHisto(fp, &Remove_Total_hg);
    ClearHisto(&Remove_Total_hg);
    LogMsg(0, SrvDebugLevel, fp, "Link histogram:\n");
    PrintHisto(fp, &Link_Total_hg);
    ClearHisto(&Link_Total_hg);
    LogMsg(0, SrvDebugLevel, fp, "Rename histogram:\n");
    PrintHisto(fp, &Rename_Total_hg);
    ClearHisto(&Rename_Total_hg);
    LogMsg(0, SrvDebugLevel, fp, "MakeDir histogram:\n");
    PrintHisto(fp, &MakeDir_Total_hg);
    ClearHisto(&MakeDir_Total_hg);
    LogMsg(0, SrvDebugLevel, fp, "RemoveDir histogram:\n");
    PrintHisto(fp, &RemoveDir_Total_hg);
    ClearHisto(&RemoveDir_Total_hg);
    LogMsg(0, SrvDebugLevel, fp, "SymLink histogram:\n");
    PrintHisto(fp, &SymLink_Total_hg);
    ClearHisto(&SymLink_Total_hg);
    LogMsg(0, SrvDebugLevel, fp, "SpoolVMLogRecord histogram:\n");
    PrintHisto(fp, &SpoolVMLogRecord_hg);
    ClearHisto(&SpoolVMLogRecord_hg);

    LogMsg(0, SrvDebugLevel, fp, "PutObjects_Transaction_hg histogram:\n");
    PrintHisto(fp, &PutObjects_Transaction_hg);
    ClearHisto(&PutObjects_Transaction_hg);

    LogMsg(0, SrvDebugLevel, fp, "PutObjects_TransactionEnd_hg histogram:\n");
    PrintHisto(fp, &PutObjects_TransactionEnd_hg);
    ClearHisto(&PutObjects_TransactionEnd_hg);

    LogMsg(0, SrvDebugLevel, fp, "PutObjects_RVM_hg histogram:\n");
    PrintHisto(fp, &PutObjects_RVM_hg);
    ClearHisto(&PutObjects_RVM_hg);


    LogMsg(0, SrvDebugLevel, fp, "PutObjects_Inodes_hg histogram:\n");
    PrintHisto(fp, &PutObjects_Inodes_hg);
    ClearHisto(&PutObjects_Inodes_hg);
#endif _TIMECALLS_

}


PRIVATE void SetDebug()
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
    LogMsg(0, 0, stdout, "Set Debug On level = %d, RPC level = %d",
	    SrvDebugLevel, RPC2_DebugLevel);
}


PRIVATE void ResetDebug()
{
    AL_DebugLevel = 0;
    RPC2_DebugLevel = 0;
    DirDebugLevel = VolDebugLevel = 0;
    LogMsg(0, 0, stdout, "Reset Debug levels to 0");
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
    rds_trace_on(rds_printer);
    rds_trace_dump_heap();
    MallocTrace = TRUE;
  }
}

/* Note  TimeStamp and rds_printer are stolen directly from the LogMsg
   implementation.  I couldn't get LogMsg to work directly, because
   of problem relating to varargs.  SMN
   */
PRIVATE void RdsTimeStamp(FILE *f)
    {
    struct tm *t;
    time_t clock;
    static int oldyear = -1, oldyday = -1; 
    static char *day[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    
    time(&clock);
    t = localtime(&clock);
    if ((t->tm_year > oldyear) || (t->tm_yday > oldyday))
	fprintf(f, "\nDate: %3s %02d/%02d/%02d\n\n", day[t->tm_wday], t->tm_mon+1, t->tm_mday, t->tm_year);
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
SwapLog: Move the current /vice/srv/SrvLog file out of the way. 
*/
void SwapLog()
{
    struct timeval tp;

    /* Need to chdir() again, since salvage may have put me elsewhere */
    if(chdir("/vice/srv")) {
	LogMsg(0, 0, stdout, "Could not cd to /vice/srv; not swapping logs");
	return;
    }
#ifndef __CYGWIN32__
    if (pushlog() != 0){
	LogMsg(0, 0, stderr, 
	       "Log file names out of order or malformed; not swapping logs");
	return;
    }
#endif

    LogMsg(0, 0, stdout, "Starting new SrvLog file");
    freopen("/vice/srv/SrvLog","a+",stdout);
    
    /* Print out time/date, since date info has "scrolled off" */
    TM_GetTimeOfDay(&tp, 0);
#ifdef	__linux__
    LogMsg(0, 0, stdout, "New SrvLog started at %s", ctime((const long int *)&tp.tv_sec));
#else
    LogMsg(0, 0, stdout, "New SrvLog started at %s", ctime(&tp.tv_sec));
#endif
}

/* Filter for scandir(); eliminates all but names of form "SrvLog-" */
PRIVATE int xselect(struct dirent *d) {
    if (strncmp(d->d_name, "SrvLog-", sizeof("SrvLog-")-1)) 
	return(0); 
    else 
	return(1);    
}

/*	Descending order comparator func for qsort() invoked by scandir().
	All inputs assumed to be of the form "SrvLog-...." 
	Returns -ve if d1 > d2 
	        +ve if d1 < d2
		0  if d1 == d2
*/
PRIVATE int compar(struct dirent **dp1, struct dirent **dp2) { 
    struct dirent *d1, *d2;

    d1 = *dp1;
    d2 = *dp2;    

    
    /* Length comparison ensures SrvLog-10 > SrvLog-1 (for example) */
    if (strlen(d1->d_name) > strlen(d2->d_name)) return(-1);
    if (strlen(d1->d_name) < strlen(d2->d_name)) return(1);

    /* Order lexically if equal lengths */
    return(-strcmp(d1->d_name, d2->d_name));
}

/* Finds the highest index of SrvLog, SrvLog-1, SrvLog-2, ...SrvLog-N.
   Then "pushes" them, resulting in SrvLog-1, SrvLog-2,....SrvLog-(N+1).
   All work is done in the current directory.
*/
PRIVATE pushlog() { 
    int i, count;
    char buf[100], buf2[100]; /* can't believe there will be more logs! */
    struct dirent **namelist;
#ifndef __CYGWIN32__
   count = scandir(".", (struct direct ***)&namelist, 
		   (int (*)(const dirent *)) xselect, 
		   (int (*)(const dirent *const *, const dirent *const *))compar);
    /* It is safe now to blindly rename */
    for (i = 0; i < count; i++) {
	sprintf(buf, "SrvLog-%d", count-i);
	if (strcmp(namelist[i]->d_name, buf) != 0) 
	    continue;
	sprintf(buf2, "SrvLog-%d", count-i+1);	
	if (rename(buf, buf2)) {
	    perror(buf); 
	    return(-1);
	}
    }
#endif	
    /* Clean up storage malloc'ed by scandir() */
    for (i = 0; i < count; i++) free(namelist[i]);
    free(namelist);
    
    /* Rename SrvLog itself */
    if (rename("SrvLog", "SrvLog-1")) {
	perror("SrvLog"); 
	return(-1);
    }
    return(0);
}


PRIVATE void FileMsg()
    {
    int srvpid;
    
    srvpid = getpid();
    LogMsg(0, 0, stdout, "The server (pid %d) can be controlled using volutil commands", srvpid);
    LogMsg(0, 0, stdout, "\"volutil -help\" will give you a list of these commands");
    LogMsg(0, 0, stdout, "If desperate,\n\t\t\"kill -SIGWINCH %d\" will increase debugging level", srvpid);
    LogMsg(0, 0, stdout, "\t\"kill -SIGUSR2 %d\" will set debugging level to zero", srvpid);
    LogMsg(0, 0, stdout, "\t\"kill -9 %d\" will kill a runaway server", srvpid);
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
    LogMsg(0, 0, stdout, "Shutdown received");
    }



PRIVATE int ParseArgs(int argc, char *argv[])
{
    int   i;

    for (i = 1; i < argc; i++) {
	if (!strcmp(argv[i], "-d")) {
	    debuglevel = atoi(argv[++i]);
	    SrvDebugLevel = debuglevel;
	    AL_DebugLevel = SrvDebugLevel/10;
	    DirDebugLevel = SrvDebugLevel;
	}
	else 
	    if (!strcmp(argv[i], "-rpcdebug"))
		RPC2_DebugLevel = atoi(argv[++i]);
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
	    if (!strcmp(argv[i], "-nosalvageonshutdown")) 
		SalvageOnShutdown = 0;
	else 
	    if (!strcmp(argv[i], "-rvmopt")) 
		optimizationson = RVM_COALESCE_RANGES;
	else 
	    if (!strcmp(argv[i], "-newchecklevel")) {
		int newchklevel = atoi(argv[++i]);
		newSetCheckLevel(newchklevel);
	    }
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
	else 
	    if (!strcmp(argv[i], "-canonicalize")){
		canonicalize = 1;
	    }
#ifdef _TIMECALLS_	
    	else
	    if (!strcmp(argv[i], "-usenscclock")) {
		clockFD = open("/dev/cntr0", O_RDONLY, 0666);
		assert(clockFD > 0);
	    }
#endif _TIMECALLS_
	else
	    if (!strcmp(argv[i], "-nc")){
		if (RvmType != UNSET) {
		    LogMsg(0, 0, stdout, "Multiple Persistence methods selected.");
		    exit(-1);
		}
		RvmType = VM;
		if (i < argc - 1) cam_log_file = argv[++i];
	    }
	else
	    if (!strcmp(argv[i], "-cam")) {
		if (RvmType != UNSET) {
		    LogMsg(0, 0, stdout, "Multiple Persistence methods selected.");
		    exit(-1);
		}
		LogMsg(0, 0, stdout, "Camelot not supported any more\n");
		exit(-1);
	    }
	else
	    if (!strcmp(argv[i], "-rvm")) {
		struct stat buf;
		if (RvmType != UNSET) {
		    LogMsg(0, 0, stdout, "Multiple Persistence methods selected.");
		    exit(-1);
		}

		if (i + 3 > argc) {	/* Need three arguments here */
		    LogMsg(0, 0, stdout, "rvm needs 3 args: LOGDEV DATADEV DATA-LENGTH.");
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
	    if (!strcmp(argv[i], "-nodumpvm")) {
		nodumpvm = TRUE;
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
	    if (!strcmp(argv[i], "-mondportal")) {
		SmonPortal = atoi(argv[++i]);
	    }
	else 
	    if (!strcmp(argv[i], "-debarrenize")) {
		extern int debarrenize;
		debarrenize = 1;
	    }
	else 
	    if (!strcmp(argv[i], "-optstore")) {
		extern int OptimizeStore;
		OptimizeStore = 1;
	    }
	else {
	    return(-1);
	}
    }
    return(0);
}


#define MAXPARMS 15
PRIVATE void NewParms(int initializing)
{
    static struct stat sbuf;
    register int      i, fd;
    char   * parms;
    char   * argv[MAXPARMS];
    register int      argc;

    if(!(stat("parms",&sbuf))) {
	parms = (char *)malloc((unsigned)sbuf.st_size);
/*	if(parms <= 0) return; */
	if(parms == 0) return;
	fd = open("parms", O_RDONLY, 0666);
	if(fd <= 0) {
	    LogMsg(0, 0, stdout, "Open for parms failed with %s", (char *) ViceErrorMsg(errno));
	    return;
	}

	i = read(fd, parms, (int)sbuf.st_size);
	close(fd);
	if(i != sbuf.st_size) {
	    if (i < 0 )
		LogMsg(0, 0, stdout, "Read on parms failed with %s", (char *) ViceErrorMsg(errno));
	    else
		LogMsg(0, 0, stdout, "Read on parms failed should have got %d bytes but read %d",
			(char *) sbuf.st_size, (char *) i);
	    free(parms);
	    return;
	}

	for(i = 0;i < MAXPARMS; argv[i++] = 0 );
	
	for(argc = i = 0; i < sbuf.st_size; i++) {
	    if((*(parms + i) != ' ') && (*(parms + i) != '\n')){
		if(argv[argc] == 0) argv[argc] = (parms+i);
	    }
	    else {
		*(parms + i) = '\0';
		if(argv[argc] != 0) {
		    if(++argc == MAXPARMS) break;
		}
		while((*(parms + i + 1) == ' ') || (*(parms + i + 1) == '\n')) i++;
	    }
	}
	if(ParseArgs(argc, argv) == 0)
	    LogMsg(0, 0, stdout, "Change parameters to:");
	else
	    LogMsg(0, 0, stdout, "Invalid parameter in:");
	for(i = 0; i < argc; i++) {
	    LogMsg(0, 0, stdout, " %s", argv[i]);
	}
	LogMsg(0, 0, stdout,"");
	free(parms);
    }
    else
	if(!initializing)
	    LogMsg(0, 0, stdout, "Received request to change parms but no parms file exists");
}

/*char	* fkey1;		 name of file that contains key1 (normally KEY1) */
/*char	* fkey2;		 name of file that contains key2 (normally KEY2  */
PRIVATE void InitServerKeys(char *fkey1, char *fkey2)
{
    FILE                * tf;
    char                  inkey[RPC2_KEYSIZE+1];
    RPC2_EncryptionKey    ptrkey1;
    RPC2_EncryptionKey    ptrkey2;
    int                   NoKey1 = 1;
    int                   NoKey2 = 1;

    /* paranoia: no trash in the keys */
    bzero((char *)ptrkey1, RPC2_KEYSIZE);
    bzero((char *)ptrkey2, RPC2_KEYSIZE);

    tf = fopen(fkey1, "r");
    if( tf == NULL ) {
	perror("could not open key 1 file");
    } else {
	NoKey1 = 0;
	bzero(inkey, sizeof(inkey));
	fgets(inkey, RPC2_KEYSIZE+1, tf);
	bcopy(inkey, (char *)ptrkey1, RPC2_KEYSIZE);
	fclose(tf);
    }

    tf = fopen(fkey2, "r");
    if( tf == NULL ) {
	perror("could not open key 2 file");
    } else {
        NoKey2 = 0;
	bzero(inkey, sizeof(inkey));
	fgets(inkey, RPC2_KEYSIZE+1, tf);
	bcopy(inkey, (char *)ptrkey2, RPC2_KEYSIZE);
	fclose(tf);
    }

    /* no keys */
    if ( NoKey1 && NoKey2 ) {
	LogMsg(0, 0, stderr, "No Keys found. Zombifying..");
	assert(0);
    }

    /* two keys: don't do a double key if they are equal */
    if( NoKey1 == 0  && NoKey2 == 0 ) {
	if ( bcmp(ptrkey1, ptrkey2, sizeof(ptrkey1)) == 0 ) 
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
    LogMsg(0, 0, stdout,"%s",msg);
    assert(0);
}


PRIVATE void DaemonizeSrv() { 
    int child, rc; 
   /* Set DATA segment limit to maximum allowable. */
#ifndef __CYGWIN32__
    struct rlimit rl;
    if (getrlimit(RLIMIT_DATA, &rl) < 0) {
        perror("getrlimit"); exit(-1);
    }
    rl.rlim_cur = rl.rlim_max;
    LogMsg(0, 0, stdout, 
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
    signal(SIGHUP,  SIG_IGN);
    /* Signals that are zombied allow debugging via gdb */
    signal(SIGTRAP, (void (*)(int))zombie);
    signal(SIGILL,  (void (*)(int))zombie);
    signal(SIGFPE,  (void (*)(int))zombie);
#ifdef	RVMTESTING
    signal(SIGBUS, (void (*)(int))my_sigBus); /* Defined in util/rvmtesting.c */
#else	RVMTESTING
    signal(SIGBUS,  (void (*)(int))zombie);
#endif	RVMTESTING
    signal(SIGSEGV, (void (*)(int))zombie);
}

static void InitializeServerRVM(void *initProc,char *name)
{		    
    void (*dummyprocptr)() = initProc; /* to pacify g++ if initProc is NULL */ 
    switch (RvmType) {							    
    case VM :					       	    
	if (dummyprocptr != NULL)						    
	    (*dummyprocptr)();						    
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
	if (prottrunc)							    
	   options->truncate = 0;					    
	else if (_Rvm_Truncate > 0 && _Rvm_Truncate < 100) {		    
	    LogMsg(0, 0, stdout, 
		   "Setting Rvm Truncate threshhold to %d.\n", _Rvm_Truncate); 
	    options->truncate = _Rvm_Truncate;				    
	} 								    
	sbrk((void *)(0x20000000 - (int)sbrk(0))); /* for garbage reasons. */		    
        err = RVM_INIT(options);                   /* Start rvm */           
        if ( err == RVM_ELOG_VERSION_SKEW ) {                                
            LogMsg(0, 0, stdout, 
		   "rvm_init failed because of skew RVM-log version."); 
            LogMsg(0, 0, stdout, "Coda server not started.");                  
            exit(-1);                                                          
	} else if (err != RVM_SUCCESS) {                                     
	    LogMsg(0, 0, stdout, "rvm_init failed %s",rvm_return(err));	    
            assert(0);                                                       
	}                                                                    
	assert(_Rvm_Data_Device != NULL);	   /* Load in recoverable mem */ 
        rds_load_heap(_Rvm_Data_Device, 
		      _Rvm_DataLength,(char **)&camlibRecoverableSegment, 
		      (int *)&err);  
	if (err != RVM_SUCCESS)						    
	    LogMsg(0, SrvDebugLevel, stdout, 
		   "rds_load_heap error %s",rvm_return(err));	    
	assert(err == RVM_SUCCESS);                                         
        /* Possibly do recovery on data structures, coalesce, etc */	    
	rvm_free_options(options);					    
	if (dummyprocptr != NULL) /* Call user specified init procedure */   
	    (*dummyprocptr)();						    
        break;                                                              
    }                                                                       
	                                                                    
    case UNSET:							    
    default:	                                                            
	printf("No persistence method selected!n");			    
	exit(-1); /* No persistence method selected, so die */		    
    }
}
