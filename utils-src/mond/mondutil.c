#ifndef _BLURB_
#define _BLURB_
/*

            Coda: an Experimental Distributed File System
                             Release 3.1

          Copyright (c) 1987-1995 Carnegie Mellon University
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

static char *rcsid = "$Header$";
#endif /*_BLURB_*/



#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <libc.h>
#include <signal.h>
#include <stdio.h>
#include <mach.h>
#include "lwp.h"
#include "rpc2.h"
#include "lock.h"

#ifdef __cplusplus
}
#endif __cplusplus

#include "mond.h"
#include "mondgen.h"
#include "report.h"
#include "data.h"
#include "bbuf.h"
#include "mondutil.h"
#include "vargs.h"
#include "util.h"
#include "ohash.h"
#include "version.h"

PRIVATE int Curr_Mon = -1;
PRIVATE int Curr_Mday = -1;
PRIVATE MUTEX DateLock;
PRIVATE CONDITION DoLobotomy;

PRIVATE void QuitSignal();
PRIVATE void TermSignal();
PRIVATE void ChildSignal();
PRIVATE void zombie(int, int, struct sigcontext *);
PRIVATE void RestoreSignals();

extern RPC2_PortalIdent rpc2_LocalPortal;
extern int LogLevel;
extern FILE *LogFile;
extern int started;
extern int BufSize;
extern int LowWater;
extern char WorkingDir[];
extern FILE *DataFile;

extern PutMagicNumber(void);

bool lobotomy = mfalse;
struct sigcontext OldContext;

void SetDate() {
    long curr_time = time(0);
    struct tm *lt = localtime(&curr_time);

    Curr_Mon = lt->tm_mon;
    Curr_Mday = lt->tm_mday;

}


int DateChanged()
{
    /* only one thread can switch logs per day */
    ObtainWriteLock(&DateLock);
    long curr_time = time(0);
    struct tm *lt = localtime(&curr_time);
    
    if (Curr_Mon == lt->tm_mon && Curr_Mday == lt->tm_mday) {
	ReleaseWriteLock(&DateLock);
	return(0);
    }

    Curr_Mon = lt->tm_mon;
    Curr_Mday = lt->tm_mday;
    ReleaseWriteLock(&DateLock);
    return(1);
}


void InitRPC(int VmonPort) {
    long rc = 0;
    int RPC2_TimeOut = 30;
    int RPC2_Retries = 5;

    /* Initialize LWP. */
    PROCESS lwpid;

    if ((rc = LWP_Init(LWP_VERSION, LWP_NORMAL_PRIORITY, &lwpid)) != LWP_SUCCESS)
	Die("InitRPC: LWP_Init failed (%d)\n", rc);

    if ((rc = IOMGR_Initialize()) != LWP_SUCCESS)
	Die("InitRPC: IOMGR Init failed (%d)\n", rc);

    RPC2_PortalIdent portal1;
    portal1.Tag = RPC2_PORTALBYINETNUMBER;
    portal1.Value.InetPortNumber = htons(VmonPort);
    RPC2_PortalIdent *portallist[1];
    portallist[0] = &portal1;
    struct timeval tv;
    tv.tv_sec = RPC2_TimeOut;
    tv.tv_usec = 0;
    rc = RPC2_Init(RPC2_VERSION, 0, portallist, 1, RPC2_Retries,&tv);
    if (rc <= RPC2_ELIMIT)
	Die("InitRPC: RPC2_Init failed (%d)", rc);
    if (rc != RPC2_SUCCESS) {
	LogMsg(0,LogLevel,LogFile, "InitRPC: RPC2_Init warning (%d)", rc);
	rc = 0;
    }

    /* set debug level in lwp */
    if (LogLevel >= 1010)
	lwp_debug = 1;

    /* Export the mond service. */
    RPC2_SubsysIdent server;
    server.Tag = RPC2_SUBSYSBYID;
    server.Value.SubsysId = MondSubsysId;
    if ((rc = RPC2_Export(&server)) != RPC2_SUCCESS)
	Die("InitRPC: RPC2_Export failed (%d)", rc);
    /* get our portal number */
    if (rpc2_LocalPortal.Tag != RPC2_PORTALBYINETNUMBER)
	Die("No portal number.  Tag value (%d)\n",rpc2_LocalPortal.Tag);
    LogMsg(0,LogLevel,LogFile,
	   "My portnum is %d",ntohs(rpc2_LocalPortal.Value.InetPortNumber));
}

/*
** Signal Facilities
**
** SIGTERM lobotomizes the listeners.  Mond will die
** when the talker is done talking.  If the talker
** is hung, this does nothing of interest, except shut
** down the listeners, which are probably blocked on a
** full buffer anyway.
**
** SIGQUIT will sacrifice the data in the bounded buffer,
** but shut down the log in an orderly fashion.
**
** Of course, if you're in a hurry, you can always use
** SIGKILL...
*/

void InitSignals() {
    DoLobotomy = new char;
    signal(SIGQUIT, (void (*)(int))QuitSignal);
    signal(SIGTERM, (void (*)(int))TermSignal);
    signal(SIGCHLD, (void (*)(int))ChildSignal);
    signal(SIGTRAP, (void (*)(int))zombie);
    signal(SIGILL,  (void (*)(int))zombie);
    signal(SIGBUS,  (void (*)(int))zombie);
    signal(SIGSEGV, (void (*)(int))zombie);
    signal(SIGFPE,  (void (*)(int))zombie);  // software exception
}

PRIVATE void RestoreSignals() {
    signal(SIGTRAP, (void (*)(int))SIG_DFL);
    signal(SIGILL,  (void (*)(int))SIG_DFL);
    signal(SIGBUS,  (void (*)(int))SIG_DFL);
    signal(SIGSEGV, (void (*)(int))SIG_DFL);
    signal(SIGFPE,  (void (*)(int))SIG_DFL);
}

PRIVATE void QuitSignal() {
    LogMsg(0,LogLevel,LogFile, "Quit signal caught");
    LogMsg(0,LogLevel,LogFile, "***** Terminating");
    Data_Done();
    Log_Done();
    exit(0);
}

PRIVATE void TermSignal() {
    LogMsg(0,LogLevel,LogFile, "Term signal caught");
    lobotomy = mtrue;
    /* wake up the BrainSurgeon */
    lwp_debug = 1;
    LWP_NoYieldSignal(DoLobotomy);
    return;
}

PRIVATE void ChildSignal() {
    /* just wait on it and bail */
    extern int errno;
    union wait status;
    int pid;
    pid = wait3(&status,WNOHANG,0);
    if (pid == -1)
	LogMsg(0,LogLevel,LogFile,
	       "Wait3 on SigChld failed: errno (%d)",
	       errno);
    started = 0;
}

bbuf *Buff_Init()
{
    bbuf *buffer;
    if (BufSize < 1)
	Die("Buff_Init: Buffer Size too small (%d)", BufSize);
    if (LowWater < 0)
	Die("Buff_Init: Low Water Mark negative (%d)", LowWater);
    buffer = new bbuf(BufSize,LowWater);

    session_pool.putSlot(session_pool.getSlot());
    comm_pool.putSlot(comm_pool.getSlot());
    clientCall_pool.putSlot(clientCall_pool.getSlot());
    clientMCall_pool.putSlot(clientMCall_pool.getSlot());
    clientRVM_pool.putSlot(clientRVM_pool.getSlot());
    vcb_pool.putSlot(vcb_pool.getSlot());
    advice_pool.putSlot(advice_pool.getSlot());
    miniCache_pool.putSlot(miniCache_pool.getSlot());
    overflow_pool.putSlot(overflow_pool.getSlot());
    srvrCall_pool.putSlot(srvrCall_pool.getSlot());
    resEvent_pool.putSlot(resEvent_pool.getSlot());
    rvmResEvent_pool.putSlot(rvmResEvent_pool.getSlot());
    srvOverflow_pool.putSlot(srvOverflow_pool.getSlot());
    iotInfo_pool.putSlot(iotInfo_pool.getSlot());
    subtree_pool.putSlot(subtree_pool.getSlot());
    repair_pool.putSlot(repair_pool.getSlot());

    return buffer;
}

void Log_Init() {
    char LogFileName[256];	/* "WORKINGDIR/LOGFILE_PREFIX.MMDD" */
    {
	strcpy(LogFileName, WorkingDir);
	strcat(LogFileName, "/");
	strcat(LogFileName, LOGFILE_PREFIX);
	strcat(LogFileName, ".");
	char mon_mday[5];
	sprintf(mon_mday, "%02d%02d", Curr_Mon + 1, Curr_Mday);
	strcat(LogFileName, mon_mday);
    }

    LogFile = fopen(LogFileName, "a");
    if (LogFile == NULL) {
	fprintf(stderr, "LOGFILE (%s) initialization failed\n", LogFileName);
	exit(-1);
    }

    struct timeval now;
    gettimeofday(&now, 0);
    char *s = ctime(&now.tv_sec);
    LogMsg(0,LogLevel,LogFile,"LOGFILE initialized with LogLevel = %d at %s",
	   LogLevel,ctime(&now.tv_sec));
    LogMsg(0,LogLevel,LogFile,"My pid is %d",getpid());
}

void Log_Done() {
    struct timeval now;
    gettimeofday(&now, 0);
    ClientTable->LogConnections(0,LogFile);
    ClientTable->PurgeConnections();
    LogMsg(0, LogLevel, LogFile,"LOGFILE terminated at %s", ctime(&now.tv_sec));

    fclose(LogFile);
    LogFile = 0;
}


void Data_Init()
{
    char DataFileName[256];
    strcpy(DataFileName, WorkingDir);
    strcat(DataFileName, "/");
    strcat(DataFileName, DATAFILE_PREFIX);
    strcat(DataFileName, ".");
    char mon_mday[5];
    sprintf(mon_mday, "%02d%02d", Curr_Mon + 1, Curr_Mday);
    strcat(DataFileName, mon_mday);
    
    DataFile = fopen(DataFileName, "a");
    PutMagicNumber();
    if (DataFile == NULL) {
	Die("Data_Init(): Open on %s failed\n",DataFileName);
    }
}

void Data_Done()
{
    fclose(DataFile);
    DataFile = 0;
}

void BrainSurgeon()
{
    bool rc;
    
    LogMsg(1000,LogLevel,LogFile,"Starting Brain Surgeon thread");
    if (lobotomy == mfalse)
	LWP_WaitProcess(DoLobotomy);
    /* we only get here if a lobotomy has been arranged */
    LogMsg(0,LogLevel,LogFile, "***** Lobotomizing");
    extern bbuf *buffer;
    buffer->flush_the_tank();
    while((rc = buffer->empty()) != mtrue) {
	LWP_DispatchProcess();
    }
/*
** if we've gotten here, we know that no new requests have
** entered the buffer, and the buffer is empty, so die
** gracefully.
*/
    Data_Done();
    Log_Done();
    RestoreSignals();
    exit(0);
}

void PrintPinged(RPC2_Handle cid)
{
    char *Hostname;
    Hostname = HostNameOfConn(cid);
    LogMsg(0,LogLevel,LogFile,"Pinged by %s",Hostname);
    delete [] Hostname;
}

int CheckCVResult(RPC2_Handle cid, int code, const char *operation, 
		  const char *badClientType)
{
    char *hostname = HostNameOfConn(cid);
    if (code == MOND_NOTCONNECTED) {
	LogMsg(0,LogLevel,LogFile,
	       "Unknown host %s tried to report a %s",
	       hostname,operation);
    } else if (code == MOND_BADCONNECTION) {
	LogMsg(0,LogLevel,LogFile,
	       "Host %s claims to be a %s, but tried to report a %s",
	       hostname,badClientType,operation);
	LogMsg(0,LogLevel,LogFile,
	       "Dropping connection with %s",hostname);
	ClientTable->RemoveConnection(cid);
    } else {
	LogMsg(0,LogLevel,LogFile,
	       "Unkown return code (%d) looking up connection to %s",
	       code,hostname);
	LogMsg(0,LogLevel,LogFile,
	       "Dropping connection with %s",hostname);
	ClientTable->RemoveConnection(cid);
    }
    delete [] hostname;
    return code;
}

			       
void zombie(int sig, int code, struct sigcontext *scp) {
    static death=0;
    if (!death) {
	death = 1;
	bcopy(scp, &OldContext, sizeof(struct sigcontext));
	LogMsg(0, 0, LogFile,  "****** INTERRUPTED BY SIGNAL %d CODE %d ******", sig, code);
	LogMsg(0, 0, LogFile,  "****** Aborting outstanding transactions, stand by...");
	
	LogMsg(0, 0, LogFile, "To debug via gdb: attach %d, setcontext OldContext", getpid());
	LogMsg(0, 0, LogFile, "Becoming a zombie now ........");
	task_suspend(task_self());
    }
    death =0;
}

void LogEventArray(VmonSessionEventArray *events)
{
    LogMsg(1000,LogLevel,LogFile,"Current event array contents");
    for (int i=0;i<nVSEs;i++) {
	VmonSessionEvent *se = &((&events->Event0)[i]);
	LogMsg(1000,LogLevel,LogFile,"\t%d\t%d\t%d\t%d\t%d",
	       se->Opcode,se->SuccessCount,
	       se->SigmaT,se->SigmaTSquared,
	       se->FailureCount);
    }
}
