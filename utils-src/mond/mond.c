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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/utils-src/mond/mond.c,v 3.4 1998/09/07 15:57:24 braam Exp $";
#endif /*_BLURB_*/






/*
 *    Mond Daemon.
 */


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <sys/file.h>
#include <sys/time.h>
#include <libc.h>
#include <netdb.h>
#include <stdio.h>
#include <assert.h>
#include "lwp.h"
#include "rpc2.h"
#include "lock.h"
extern void SetMallocCheckLevel(int);

#ifdef __cplusplus
}
#endif __cplusplus

#include <new.h>
#include <stdarg.h>
#include <newplumb.h>

#include "mondgen.h"
#include "mond.h"
#include "report.h"
#include "vargs.h"
#include "data.h"
#include "bbuf.h"
#include "util.h"
#include "ohash.h"
#include "version.h"
#include "mondutil.h"

#define UNWINDOBJ "/usr/mond/bin/unwind"

/* Command line arguments */
char WorkingDir[256] =	"/usr/mond/log";        /* -wd */
char DataBaseName[256] = "codastats2";     	/* -db */
int VmonPort =	1356;				/* -mondp */
int LogLevel = 0;				/* -d */
int BufSize = 50;             		        /* -b */
int Listeners = 3;                     		/* -l */
int LowWater = 0;                       	/* -w */
int SleepTime = 60*60*2;                        /* -ui */
int NoSpool = 0;                                /* -nospool */
char *RemoveOnDone = "-r";                      /* -r(!remove)/-R(remove)*/

int started = 0;

void Log_Done();

static void ListenerLWP(char *);
static void TalkerLWP(char *);
static void UtilityLWP(char *);
static void StartThreads();
static void ParseArgs(int, char **);

extern int h_errno;
extern void PutMagicNumber(void);

FILE *LogFile = 0;
FILE *DataFile = 0;

bbuf *buffer;
static CONDITION UtilityTimer = (char *) 0x1100;
static const int STACKSIZE = 16 * 1024;
static const char *VENUSNM = "venus";
static const char *VICENM = "vice";

#define	STREQ(a, b) (strcmp((a), (b)) == 0)

static void my_new_handler(void);

static void my_new_handler(void) {
	fprintf(stderr,"Ack!  new returned zero\n");
	fflush(stderr);
	assert(0);
}

int main(int argc, char *argv[]) {
    set_new_handler(&my_new_handler);
    ParseArgs(argc, argv);
    SetDate();
    Log_Init();
    Data_Init();
    buffer = Buff_Init();
    ClientTable = new connection_table(CLIENT_TABSIZE);
    InitRPC(VmonPort);
    InitSignals();
    StartThreads();
    newPlumber(LogFile);
    fflush(LogFile);
    /* BrainSurgeon never returns */
    BrainSurgeon();
}

static void StartThreads()
{
    int listenerCount = 0;
    PROCESS pid;

    /* create the utility thread */
    assert (LWP_CreateProcess((PFIC)UtilityLWP, STACKSIZE,
			      LWP_NORMAL_PRIORITY-1, NULL,
			      NULL, &pid)
	    == LWP_SUCCESS);
    /* create the talker */
    assert (LWP_CreateProcess((PFIC)TalkerLWP, STACKSIZE,
			      LWP_NORMAL_PRIORITY, (char *) 0,
			      NULL, &pid)
	    == LWP_SUCCESS);
    if (Listeners < 1)
	Die("StartThreads: Bad listener argument\n");
    int i;
    /* create the listeners */
    for (i=0; i<Listeners; i++)
    {
	assert (LWP_CreateProcess((PFIC)ListenerLWP,STACKSIZE,
				  LWP_NORMAL_PRIORITY+1, (char *) i,
				  NULL,&pid)
		== LWP_SUCCESS);
    }
    LogMsg(100,LogLevel,LogFile,"My Listener count is %d", Listeners);
}

void UtilityLWP(char *p)
{
    /* the argument is ignored (it's just NULL) */
    int rc;
    struct itimerval utilTimer;
    char *args[11];
    char llstring[128];
    int pid;

    args[0] = "unwind";
    args[1] = "-wd";
    args[2] = WorkingDir;
    args[3] = "-pre";
    args[4] = DATAFILE_PREFIX;
    args[5] = "-d";
    sprintf(llstring,"%d",LogLevel);
    args[6] = llstring;
    args[7] = RemoveOnDone;
    args[8] = "-db";
    args[9] = DataBaseName;
    args[10] = NULL;
    utilTimer.it_interval.tv_usec = 0;
    utilTimer.it_interval.tv_sec = SleepTime;
    utilTimer.it_value.tv_usec = 0;
    utilTimer.it_value.tv_sec = SleepTime;
    if ((rc = IOMGR_Signal(SIGALRM, UtilityTimer))
	!= LWP_SUCCESS)
	Die("Could not setup SIGALARM catcher (%d)\n",rc);
    for (;;)
    {
	setitimer(ITIMER_REAL,&utilTimer,NULL);
	LWP_WaitProcess(UtilityTimer);
	newPlumber(LogFile);
	fflush(LogFile);
	LogMsg(100,LogLevel,LogFile, "SIGALARM recieved by utilTimer");
	/* for now, the only utility task is to startup the unwinder */
	if (NoSpool) {
	    LogMsg(1,LogLevel,LogFile,
		   "Spooling disabled...unwind data by hand");
	}
	else {
	    if (started == 0) {
		started = 1;
		pid = fork();
		if (pid == -1)
		    LogMsg(0,LogLevel,LogFile,
			   "UtilityLWP: fork failed...unwind by hand");
		if (pid == 0) {
		    if (execv(UNWINDOBJ,args)) {
			fprintf(stderr,"Couldn't start up unwind...GACK!");
			exit(-1);
		    }
		}
	    } else {
		LogMsg(0,LogLevel,LogFile,
		       "Another unwind (pid %d) is currently running",
		       pid);
	    }
	}
    }
}

void ListenerLWP(char *p)
{

/* the argument, p, is actually an integer which denotes the 
   number of this thread. */

    int listenerNo = (int) p;
    RPC2_RequestFilter VmonFilter;
    RPC2_Handle VmonHandle = 0;
    RPC2_PacketBuffer *VmonPacket = 0;
    char *Hostname;

    LogMsg(1000,LogLevel,LogFile,"Starting listener number %d",listenerNo);

    for (;;) {
	/* check for lobotomy - if so go away */
	extern int lobotomy;
	if (lobotomy == mtrue) {
	    return;
	}
	VmonFilter.FromWhom = ONESUBSYS;
	VmonFilter.OldOrNew = OLDORNEW;
	VmonFilter.ConnOrSubsys.SubsysId = MondSubsysId;
	long code = RPC2_GetRequest(&VmonFilter, &VmonHandle, &VmonPacket, 
				   0, 0, 0, 0);
	Hostname = HostNameOfConn(VmonHandle);
	LogMsg(1000,LogLevel,LogFile,"Listener %d received request w/ header opcode %d, client=%s",
	       listenerNo,VmonPacket->Header.Opcode,Hostname);

	if (VmonPacket->Header.Opcode == RPC2_NEWCONNECTION)
	    PrintPinged(VmonHandle);
	if (code <= RPC2_WLIMIT)
	    LogMsg(1,LogLevel,LogFile,
		   "Listener Number %d: GetRequest -> %s", listenerNo, 
		   RPC2_ErrorMsg((int)code));
	if (code <= RPC2_ELIMIT) continue;
	
	/* Swap files on first record of new day. */
	if (DateChanged()) {
	    Data_Done();
	    Log_Done();
	    Log_Init();
	    Data_Init();
	}
	
	code = mond_ExecuteRequest(VmonHandle, VmonPacket, 0);
	if (code <= RPC2_WLIMIT)
	    LogMsg(1,LogLevel,LogFile, 
		   "ExecuteRequest -> %s", RPC2_ErrorMsg((int)code));
    }

  delete [] Hostname; 
}

void TalkerLWP(char *p)
{
    int talkerNo = (int) p;
    vmon_data *slot;
    
    LogMsg(1000,LogLevel,LogFile,"Starting talker number %d",talkerNo);
    
    for (;;)
    {
	assert(buffer->remove(&slot) == BBUFOK);
	// Put in magic number as a guard...
	PutMagicNumber();
	slot->Report();
	LogMsg(0,LogLevel,LogFile, "Talker %d spooled a slot of type %s",
	       talkerNo,slot->TypeName());
	slot->Release();	
    }
}


long VmonReportSession(RPC2_Handle cid, VmonVenusId *Venus, 
		       VmonSessionId Session, VolumeId Volume, UserId User, 
		       VmonAVSG *AVSG, RPC2_Unsigned StartTime,
		       RPC2_Unsigned EndTime, 
		       RPC2_Unsigned CETime, long VSEA_size, 
		       VmonSessionEvent Events[],
		       SessionStatistics *Stats,
		       CacheStatistics *CacheStats) {
    int code = 0;

#ifdef VERSION_CONTROL
    code = ClientTable->ConnectionValid(cid,MOND_VENUS_CLIENT);
    if (code == MOND_OK) {
#endif VERSION_CONTROL
	session_data *slot;
	slot = (session_data *) session_pool.getSlot();
	slot->init(Venus,Session,Volume,User,AVSG,StartTime,EndTime,
		   CETime,VSEA_size,
		   Events,Stats,CacheStats);
	LogEventArray(slot->theEvents());
	assert(buffer->insert((vmon_data *)slot) == BBUFOK);
	LogEventArray(slot->theEvents());
#ifdef VERSION_CONTROL
    } else
	CheckCVResult(cid,code,"session event",VICENM);
#endif VERSION_CONTROL
    return(code);
}


long VmonReportCommEvent(RPC2_Handle cid, VmonVenusId *Venus, 
			 RPC2_Unsigned ServerIPAddress, RPC2_Integer SN,
			 RPC2_Unsigned Time, VmonCommEventType Type) {
    int code = 0;

#ifdef VERSION_CONTROL
    code = ClientTable->ConnectionValid(cid,MOND_VENUS_CLIENT);
    if (code == MOND_OK) {
#endif VERSION_CONTROL
	comm_data *slot;
	slot = (comm_data *) comm_pool.getSlot();
	slot->init(Venus, ServerIPAddress, SN, Time, Type);
	assert(buffer->insert((vmon_data *)slot) == BBUFOK);
#ifdef VERSION_CONTROL
    } else
	CheckCVResult(cid,code,"comm event",VICENM);
#endif VERSION_CONTROL
    return(code);
}

long VmonReportCallEvent(RPC2_Handle cid, VmonVenusId *Venus,
			 long Time, long sc_size, CallCountEntry sc[]) {
    int code =0;

#ifdef VERSION_CONTROL
    code = ClientTable->ConnectionValid(cid,MOND_VENUS_CLIENT);
    if (code == MOND_OK) {
#endif VERSION_CONTROL
	clientCall_data *slot;
	slot = (clientCall_data *) clientCall_pool.getSlot();
	slot->init(Venus,Time,sc_size,sc);
	assert(buffer->insert((vmon_data *)slot) == BBUFOK);
#ifdef VERSION_CONTROL
    } else
	CheckCVResult(cid,code,"venus call record",VICENM);
#endif VERSION_CONTROL
    return(code);
}

long VmonReportMCallEvent(RPC2_Handle cid, VmonVenusId *Venus,
			 long Time, long msc_size, MultiCallEntry msc[]) {
    int code =0;

#ifdef VERSION_CONTROL
    code = ClientTable->ConnectionValid(cid,MOND_VENUS_CLIENT);
    if (code == MOND_OK) {
#endif VERSION_CONTROL
	clientMCall_data *slot;
	slot = (clientMCall_data *) clientMCall_pool.getSlot();
	slot->init(Venus,Time,msc_size,msc);
	assert(buffer->insert((vmon_data *)slot) == BBUFOK);
#ifdef VERSION_CONTROL
    } else
	CheckCVResult(cid,code,"venus mcall record",VICENM);
#endif VERSION_CONTROL
    return(code);
}

long VmonReportRVMStats(RPC2_Handle cid, VmonVenusId *Venus,
			 long Time, RvmStatistics *stats) {
    int code =0;

#ifdef VERSION_CONTROL
    code = ClientTable->ConnectionValid(cid,MOND_VENUS_CLIENT);
    if (code == MOND_OK) {
#endif VERSION_CONTROL
	clientRVM_data *slot;
	slot = (clientRVM_data *) clientRVM_pool.getSlot();
	slot->init(Venus,Time,stats);
	assert(buffer->insert((vmon_data *)slot) == BBUFOK);
#ifdef VERSION_CONTROL
    } else
	CheckCVResult(cid,code,"venus rvm record",VICENM);
#endif VERSION_CONTROL
    return(code);
}

long VmonReportVCBStats(RPC2_Handle cid, VmonVenusId *Venus,
			long VenusInit, long Time, VolumeId Volume,
			VCBStatistics *stats) {
    int code =0;

#ifdef VERSION_CONTROL
    code = ClientTable->ConnectionValid(cid,MOND_VENUS_CLIENT);
    if (code == MOND_OK) {
#endif VERSION_CONTROL
	vcb_data *slot;
	slot = (vcb_data *) vcb_pool.getSlot();
	LogMsg(100, LogLevel, LogFile, 
	       "VmonReportVCBStats: Venus 0x%x.%ld, VenusInit = %ld, Time = %ld, Volume = 0x%x",
	       Venus->IPAddress, Venus->BirthTime, VenusInit, Time, Volume);
	slot->init(Venus,VenusInit,Time,Volume,stats);
	assert(buffer->insert((vmon_data *)slot) == BBUFOK);
#ifdef VERSION_CONTROL
    } else
	CheckCVResult(cid,code,"venus VCB record",VICENM);
#endif VERSION_CONTROL
    return(code);
}

long VmonReportAdviceStats(RPC2_Handle cid, VmonVenusId *Venus,
			   long Time, UserId User, AdviceStatistics *Stats,
			   long Call_Size, AdviceCalls Call_Stats[],
			   long Result_Size, AdviceResults Result_Stats[]) {

    int code = 0;

#ifdef VERSION_CONTROL
    code = ClientTable->ConnectionValid(cid,MOND_VENUS_CLIENT);
    if (code == MOND_OK) {
#endif VERSION_CONTROL
	advice_data *slot;
	slot = (advice_data *) advice_pool.getSlot();
	LogMsg(100, LogLevel, LogFile, "VmonReportAdviceStats: Time = %ld, User = %d\n", Time, User);
	LogMsg(100, LogLevel, LogFile, "VmonReportAdviceStats: NotEnabled = %d, NotValid = %d, Outstanding = %d, ASRnotAllowed = %d, ASRinterval = %d, VolumeNull = %d, Attempts = %d\n", Stats->NotEnabled, Stats->NotValid, Stats->Outstanding, Stats->ASRnotAllowed, Stats->ASRinterval, Stats->VolumeNull, Stats->TotalNumberAttempts);
	LogMsg(100, LogLevel, LogFile, "VmonReportAdviceStats: Call_Size = %d; Result_Size = %d\n", Call_Size, Result_Size);

	slot->init(Venus,Time,User,Stats,Call_Size,Call_Stats,Result_Size,Result_Stats);
	assert(buffer->insert((vmon_data *)slot) == BBUFOK);
#ifdef VERSION_CONTROL
    } else
	CheckCVResult(cid,code,"advice record",VICENM);
#endif VERSION_CONTROL

    return(code);
}

long VmonReportMiniCache(RPC2_Handle cid, VmonVenusId *Venus,
			 long Time, long vn_size, 
			 VmonMiniCacheStat vn_stats[],
			 long vfs_size, 
			 VmonMiniCacheStat vfs_stats[]) {
    int code =0;

#ifdef VERSION_CONTROL
    code = ClientTable->ConnectionValid(cid,MOND_VENUS_CLIENT);
    if (code == MOND_OK) {
#endif VERSION_CONTROL
	miniCache_data *slot;
	slot = (miniCache_data *) miniCache_pool.getSlot();
	slot->init(Venus,Time,vn_size,vn_stats,vfs_size,vfs_stats);
	assert(buffer->insert((vmon_data *)slot) == BBUFOK);
#ifdef VERSION_CONTROL
    } else
	CheckCVResult(cid,code,"venus call record",VICENM);
#endif VERSION_CONTROL

    return(code);
}
    
long VmonReportOverflow(RPC2_Handle cid, VmonVenusId *Venus,
			RPC2_Unsigned VMStartTime, RPC2_Unsigned VMEndTime, 
			RPC2_Integer VMCount,
			RPC2_Unsigned RVMStartTime, RPC2_Unsigned RVMEndTime, 
			RPC2_Integer RVMCount) {
    int code = 0;

#ifdef VERSION_CONTROL
    code = ClientTable->ConnectionValid(cid,MOND_VENUS_CLIENT);
    if (code == MOND_OK) {
#endif VERSION_CONTROL
	overflow_data *slot;
	slot = (overflow_data *) overflow_pool.getSlot();
	slot->init(Venus,VMStartTime,VMEndTime,VMCount,
		   RVMStartTime,RVMEndTime,RVMCount);
	assert(buffer->insert((vmon_data *)slot) == BBUFOK);
#ifdef VERSION_CONTROL
    } else
	CheckCVResult(cid,code,"venus overflow",VICENM);
#endif VERSION_CONTROL
    return(code);
}

long SmonReportCallEvent(RPC2_Handle cid, SmonViceId *Vice, RPC2_Unsigned Time, 
			 RPC2_Integer CBCount_size_, CallCountEntry CBCount[], 
			 RPC2_Integer ResCount_size_, CallCountEntry ResCount[], 
			 RPC2_Integer SmonCount_size_, CallCountEntry SmonCount[], 
			 RPC2_Integer VolDCount_size_, CallCountEntry VolDCount[], 
			 RPC2_Integer MultiCount_size_, MultiCallEntry MultiCount[],
			 SmonStatistics *Stats)
{
    int code = 0;

#ifdef VERSION_CONTROL
    code = ClientTable->ConnectionValid(cid,MOND_VICE_CLIENT);
    if (code == MOND_OK) {
#endif VERSION_CONTROL
	srvrCall_data *slot;
	slot = (srvrCall_data *) srvrCall_pool.getSlot();
	slot->init(Vice,Time,CBCount_size_,CBCount,ResCount_size_,ResCount,
		   SmonCount_size_,SmonCount,VolDCount_size_,VolDCount,
		   MultiCount_size_,MultiCount,Stats);
	assert(buffer->insert((vmon_data *)slot) == BBUFOK);
#ifdef VERSION_CONTROL
    } else
	CheckCVResult(cid,code,"server stats record",VENUSNM);
#endif VERSION_CONTROL
    return(code);
}

long SmonReportResEvent(RPC2_Handle cid, SmonViceId *Vice, RPC2_Unsigned Time, 
			VolumeId Volid, RPC2_Integer HighWaterMark, 
			RPC2_Integer AllocNumber, RPC2_Integer DeallocNumber, 
			RPC2_Integer ResOp_size_, ResOpEntry ResOp[])
{
    int code =0;

#ifdef VERSION_CONTROL
    code = ClientTable->ConnectionValid(cid,MOND_VICE_CLIENT);
    if (code == MOND_OK) {
#endif VERSION_CONTROL
	resEvent_data *slot;
	slot = (resEvent_data *) resEvent_pool.getSlot();
	slot->init(Vice,Time,Volid,HighWaterMark,AllocNumber,
		   DeallocNumber,ResOp_size_,ResOp);
	assert(buffer->insert((vmon_data *)slot) == BBUFOK);
#ifdef VERSION_CONTROL
    } else
	CheckCVResult(cid,code,"resolve event record",VENUSNM);
#endif VERSION_CONTROL
    return code;
}

long SmonReportOverflow(RPC2_Handle cid, SmonViceId *Vice, RPC2_Unsigned Time, 
			RPC2_Unsigned StartTime, RPC2_Unsigned EndTime, 
			RPC2_Integer Count)
{
    int code = 0;
    
#ifdef VERSION_CONTROL
    code = ClientTable->ConnectionValid(cid,MOND_VICE_CLIENT);
    if (code == MOND_OK) {
#endif VERSION_CONTROL
	srvOverflow_data *slot;
	slot = (srvOverflow_data *) srvOverflow_pool.getSlot();
	slot->init(Vice,Time,StartTime,EndTime,Count);
	assert(buffer->insert((vmon_data *)slot) == BBUFOK);
#ifdef VERSION_CONTROL
    } else
	CheckCVResult(cid,code,"server overflow record",VENUSNM);
#endif VERSION_CONTROL
    return code;
}


long VmonReportIotInfo(RPC2_Handle cid, VmonVenusId *Venus,
		       IOT_INFO *Info, RPC2_Integer AppNameLen, 
		       RPC2_String AppName) {
    int code = 0;
    
#ifdef VERSION_CONTROL
    code = ClientTable->ConnectionValid(cid,MOND_VENUS_CLIENT);
    if (code == MOND_OK) {
#endif VERSION_CONTROL
	iotInfo_data *slot;
	slot = (iotInfo_data *) iotInfo_pool.getSlot();
	LogMsg(100, LogLevel, LogFile, "VmonReportIotInfo: Tid = %d, ResOpt = %d, ElapsedTime = %d, ReadSetSize = %d, WriteSetSize = %d ReadVolNum = %d WriteVolNum = %d Validation = %d InvalidSize = %d BackupObjNum = %d LifeCycle = %d PredNum = %d SuccNum = %d\n", Info->Tid, Info->ResOpt, Info->ElapsedTime, Info->ReadSetSize, Info->WriteSetSize, Info->Validation, Info->InvalidSize, Info->BackupObjNum, Info->LifeCycle, Info->PredNum, Info->SuccNum);
	LogMsg(100, LogLevel, LogFile, "VmonReportIotInof: AppNameLen = %d AppName = %s\n", AppNameLen, AppName); 	       
	slot->init(Venus, Info, AppNameLen, AppName);
	assert(buffer->insert((vmon_data *)slot) == BBUFOK);
#ifdef VERSION_CONTROL
    } else
      CheckCVResult(cid,code,"iotInfo record",VICENM);
#endif VERSION_CONTROL

    return(code);
}


long VmonReportIotStats(RPC2_Handle cid, VmonVenusId *Venus, RPC2_Integer Time,
		       IOT_STAT *Stats) {
    int code = 0;
    
#ifdef VERSION_CONTROL
    code = ClientTable->ConnectionValid(cid,MOND_VENUS_CLIENT);
    if (code == MOND_OK) {
#endif VERSION_CONTROL
	iotStat_data *slot;
	slot = (iotStat_data *) iotStat_pool.getSlot();
	LogMsg(100, LogLevel, LogFile, "VmonReportIotStats: MaxElapsedTime = %d AvgElapsedTime = %d MaxReadSetSize = %d AvgReadSetSize =%d MaxWriteSetSize = %d AvgWriteSetSize = %d MaxReadVolNum = %d AvgReadVolNum = %d MaxWriteVolNum = %d AvgWriteVolNum = %d Committed = %d Pending = %d Resolved = %d Repaired = %d OCCRerun = %d\n", Stats->MaxElapsedTime, Stats->AvgElapsedTime, Stats->MaxReadSetSize, Stats->AvgReadSetSize, Stats->MaxWriteSetSize, Stats->AvgWriteSetSize, Stats->MaxReadVolNum, Stats->AvgReadVolNum, Stats->MaxWriteVolNum, Stats->AvgWriteVolNum, Stats->Committed, Stats->Pending, Stats->Resolved, Stats->Repaired, Stats->OCCRerun);
	slot->init(Venus, Time, Stats);
	assert(buffer->insert((vmon_data *)slot) == BBUFOK);
#ifdef VERSION_CONTROL
    } else
      CheckCVResult(cid,code,"iotStat record",VICENM);
#endif VERSION_CONTROL

    return(code);
}

long VmonReportSubtreeStats(RPC2_Handle cid, VmonVenusId *Venus, RPC2_Integer Time,
			    LocalSubtreeStats *Stats) {
    int code = 0;
    
#ifdef VERSION_CONTROL
    code = ClientTable->ConnectionValid(cid,MOND_VENUS_CLIENT);
    if (code == MOND_OK) {
#endif VERSION_CONTROL
	subtree_data *slot;
	slot = (subtree_data *) subtree_pool.getSlot();
	LogMsg(100, LogLevel, LogFile, "VmonReportSubtreeStats: SubtreeNum = %d, MaxSubtreeSize = %d, AvgSubtreeSize = %d, MaxSubtreeHgt = %d, AvgSubtreeHgt = %d, MaxMutationNum = %d, AvgMutationNum = %d\n", Stats->SubtreeNum, Stats->MaxSubtreeSize, Stats->AvgSubtreeSize, Stats->MaxSubtreeHgt, Stats->AvgSubtreeHgt, Stats->MaxMutationNum, Stats->AvgMutationNum);
	slot->init(Venus, Time, Stats);
	assert(buffer->insert((vmon_data *)slot) == BBUFOK);
#ifdef VERSION_CONTROL
    } else
      CheckCVResult(cid,code,"subtree record",VICENM);
#endif VERSION_CONTROL

    return(code);
}

long VmonReportRepairStats(RPC2_Handle cid, VmonVenusId *Venus, RPC2_Integer Time,
			   RepairSessionStats *Stats) {
    int code = 0;
    
#ifdef VERSION_CONTROL
    code = ClientTable->ConnectionValid(cid,MOND_VENUS_CLIENT);
    if (code == MOND_OK) {
#endif VERSION_CONTROL
	repair_data *slot;
	slot = (repair_data *) repair_pool.getSlot();
	LogMsg(100, LogLevel, LogFile, "VmonReportRepairStats: SessionNum = %d, CommitNum = %d, AbortNum = %d, CheckNum = %d, PreserveNum = %d, DiscardNum = %d, RemoveNum = %d GlobalViewNum = %d, LocalViewNum = %d KeepLocalNum = %d, ListLocalNum = %d, RepMutationNum = %d\n", Stats->SessionNum, Stats->CommitNum, Stats->AbortNum, Stats->CheckNum, Stats->PreserveNum, Stats->DiscardNum, Stats->RemoveNum, Stats->GlobalViewNum, Stats->LocalViewNum, Stats->KeepLocalNum, Stats->ListLocalNum, Stats->RepMutationNum);
	LogMsg(100, LogLevel, LogFile, "NewCommand1Num = %d NewCommand2Num = %d NewCommand3Num = %d NewCommand4Num = %d NewCommand5Num = %d NewCommand6Num = %d NewCommand7Num = %d NewCommand8Num = %d MissTarget = %d MissParentNum = %d AclDenyNm = %d UpdateUpdateNum = %d NameNameNum = %d RemoveUpdateNum = %d\n", Stats->NewCommand1Num, Stats->NewCommand2Num, Stats->NewCommand3Num, Stats->NewCommand4Num, Stats->NewCommand5Num, Stats->NewCommand6Num, Stats->NewCommand7Num, Stats->NewCommand8Num, Stats->MissTargetNum, Stats->MissParentNum, Stats->AclDenyNum, Stats->UpdateUpdateNum, Stats->NameNameNum, Stats->RemoveUpdateNum);
	slot->init(Venus, Time, Stats);
	assert(buffer->insert((vmon_data *)slot) == BBUFOK);
#ifdef VERSION_CONTROL
    } else
      CheckCVResult(cid,code,"subtree record",VICENM);
#endif VERSION_CONTROL

    return(code);
}

long VmonReportRwsStats(RPC2_Handle cid, VmonVenusId *Venus, RPC2_Integer Time,
			ReadWriteSharingStats *Stats) {
    int code = 0;
    
#ifdef VERSION_CONTROL
    code = ClientTable->ConnectionValid(cid,MOND_VENUS_CLIENT);
    if (code == MOND_OK) {
#endif VERSION_CONTROL
	rwsStat_data *slot;
	slot = (rwsStat_data *) rwsStat_pool.getSlot();
	LogMsg(100, LogLevel, LogFile, "VmonReportRwsStats: Vid = %x RwSharingCount = %d DiscReadCount = %d DiscDuration =%d\n", Stats->Vid, Stats->RwSharingCount, Stats->DiscReadCount);
	slot->init(Venus, Time, Stats);
	assert(buffer->insert((vmon_data *)slot) == BBUFOK);
#ifdef VERSION_CONTROL
    } else
      CheckCVResult(cid,code,"rwsStat record",VICENM);
#endif VERSION_CONTROL

    return(code);
}

long SmonNoop(RPC2_Handle cid)
{
    return 0;
}

long SmonReportRVMResStats(RPC2_Handle cid, SmonViceId *Vice,
			   RPC2_Unsigned Time, RPC2_Integer VolID,
			   FileResStats *FileRes, DirResStats *DirRes,
			   RPC2_Integer LSH_size,
			   HistoElem LogSizeHisto[],
			   RPC2_Integer LMH_size, HistoElem LogMaxHisto[], 
			   ResConflictStats *Conflicts, 
			   RPC2_Integer SHH_size, HistoElem SuccHierHist[],
			   RPC2_Integer FHH_size, HistoElem FailHierHist[],
			   ResLogStats *ResLog, RPC2_Integer VLH_size, 
			   HistoElem VarLogHisto[], RPC2_Integer LS_size, 
			   HistoElem LogSize[])
{
    int code =0;

#ifdef VERSION_CONTROL
    code = ClientTable->ConnectionValid(cid, MOND_VICE_CLIENT);
    if (code == MOND_OK) {
#endif VERSION_CONTROL
	rvmResEvent_data *slot;
	slot = (rvmResEvent_data *) rvmResEvent_pool.getSlot();
	slot->init(*Vice, Time, VolID, *FileRes, *DirRes, LSH_size,
		   LogSizeHisto, LMH_size, LogMaxHisto, *Conflicts,
		   SHH_size, SuccHierHist, FHH_size, FailHierHist,
		   *ResLog, VLH_size, VarLogHisto, LS_size, LogSize);
	assert(buffer->insert((vmon_data *)slot) == BBUFOK);
#ifdef VERSION_CONTROL
    } else {
	CheckCVResult(cid,code,"rvm resolution record",VENUSNM);
    }
#endif VERSION_CONTROL

    return code;
}

static void ParseArgs(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
	if ((STREQ(argv[i], "-plumb"))) {
	    /* newSetCheckLevel(3); */
	    /* SetMallocCheckLevel(4); */
	    continue;
	}
	if ((STREQ(argv[i], "-r")) || (STREQ(argv[i],"-R"))) {
	    RemoveOnDone = argv[i];
	    continue;
	}
	if (STREQ(argv[i], "-nospool")) {
	    NoSpool = 1;
	    continue;
	}
	if (STREQ(argv[i], "-wd")) {		/* working directory */
	    strcpy(WorkingDir, argv[++i]);
	    continue;
	}
	if (STREQ(argv[i], "-db")) {		/* working directory */
	    strcpy(DataBaseName, argv[++i]);
	    continue;
	}
	else if	(STREQ(argv[i], "-mondp")) {	/* vmon/smon port */
	    VmonPort = atoi(argv[++i]);
	    continue;
	}
	else if	(STREQ(argv[i], "-d")) {	/* debug */
	    LogLevel = atoi(argv[++i]);
	    continue;
	}
	else if	(STREQ(argv[i], "-b")) {	/* buffer size */
	    BufSize = atoi(argv[++i]);
	    continue;
	}
	else if	(STREQ(argv[i], "-l")) {	/* listeners */
	    Listeners = atoi(argv[++i]);
	    continue;
	}
	else if	(STREQ(argv[i], "-w")) {	/* low water mark */
	    LowWater = atoi(argv[++i]);
	    continue;
	}
	else if	(STREQ(argv[i], "-ui")) {	/* low water mark */
	    SleepTime = atoi(argv[++i]);
	    continue;
	}
	else {
	    printf("usage: mond [[-wd workingdir] [-mondp port number] [-d debuglevel]\n");
	    printf("             [-b buffersize] [-l listeners] [-w lowWaterMark]\n");
	    printf("             [-ui utility interval] [-nospool] [-r|-R]\n");
	    exit(1000);
	}
    }
}

