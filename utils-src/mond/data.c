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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/utils-src/mond/data.c,v 3.5 1998/11/30 11:39:54 jaharkes Exp $";
#endif /*_BLURB_*/




//
// data.c
//
// implementation of the classes encapsulating the data types collected
// by mond along with the buffer pool class.

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include "coda_assert.h"
#include <stdlib.h>
#include "lock.h"

#ifdef __cplusplus
}
#endif __cplusplus

#include "mondgen.h"
#include "mond.h"
#include "report.h"
#include "data.h"
#include "util.h"


extern int LogLevel;
extern FILE *LogFile;


/*
** global buffer pools -- one per data type
*/

bufpool session_pool(SESSION);
bufpool comm_pool(COMM);
bufpool clientCall_pool(CLNTCALL);
bufpool clientMCall_pool(CLNTMCALL);
bufpool clientRVM_pool(CLNTRVM);
bufpool vcb_pool(VCB);
bufpool advice_pool(ADVICE);
bufpool miniCache_pool(MINICACHE);
bufpool overflow_pool(OVERFLOW);
bufpool srvrCall_pool(SRVCALL);
bufpool resEvent_pool(SRVRES);
bufpool rvmResEvent_pool(SRVRVMRES);
bufpool srvOverflow_pool(SRVOVRFLW);
bufpool iotInfo_pool(IOTINFO);
bufpool iotStat_pool(IOTSTAT);
bufpool subtree_pool(SUBTREE);
bufpool repair_pool(REPAIR);
bufpool rwsStat_pool(RWSSTAT);

void callCountArray::set(long newSize, CallCountEntry *newArray)
{
    if (size != newSize) {
	if (array != NULL) {
	    for (int i=0; i<size; i++)
		delete [] array[i].name;
	    delete [] array;
	}
	size = newSize;
	array = new CallCountEntry[newSize];
	for (int i=0; i<size; i++)
	    array[i].name = (RPC2_String) new char[SIGCHAR];
    }
    for (int i=0; i<size; i++) {
	strncpy((char *) array[i].name,(char *) newArray[i].name,SIGCHAR);
	// guarantee null termination
	array[i].name[SIGCHAR-1] = '\0';
	array[i].countent = newArray[i].countent;
	array[i].countexit = newArray[i].countexit;
	array[i].tsec = newArray[i].tsec;
	array[i].tusec = newArray[i].tusec;
	array[i].counttime = newArray[i].counttime;
    }
}

callCountArray::Print(void) {
  LogMsg(0, LogLevel, LogFile, "callCountArray::Print:  this=%x, size=%d", this, size);
  for (int i = 0; i < size; i++) 
    LogMsg(0, LogLevel, LogFile, "callCountArray::Print: array[%d] = <%s, %d, %d, %d, %d, %d>", i, array[i].name, array[i].countent, array[i].countexit, array[i].tsec, array[i].tusec, array[i].counttime);
}

callCountArray::callCountArray(void) {
    size = 0;
    array = NULL;
}

callCountArray::~callCountArray(void) {
    if (array != NULL) {
	for (int i=0; i<size; i++)
	    delete [] array[i].name;
	delete [] array;
    }
}


multiCallArray::multiCallArray(void) {
    size = 0;
    array = NULL;
}

multiCallArray::~multiCallArray(void) {
    if (array != NULL) {
	for (int i=0; i<size; i++)
	    delete [] array[i].name;
	delete [] array;
    }
}

void multiCallArray::set(long newSize, MultiCallEntry *newArray) {
    if (size != newSize) {
	if (array != NULL) {
	    for (int i=0; i<size; i++)
		delete [] array[i].name;
	    delete [] array;
	}
	size = newSize;
	array = new MultiCallEntry[newSize];
	for (int i=0; i<size; i++)
	    array[i].name = (RPC2_String) new char[SIGCHAR];
    }
    for (int i=0; i<size; i++) {
	strncpy((char *) array[i].name,(char *) newArray[i].name,SIGCHAR);
	// guarantee null termination
	array[i].name[SIGCHAR-1] = '\0';
	array[i].countent = newArray[i].countent;
	array[i].countexit = newArray[i].countexit;
	array[i].tsec = newArray[i].tsec;
	array[i].tusec = newArray[i].tusec;
	array[i].counttime = newArray[i].counttime;
	array[i].counthost = newArray[i].counthost;
    }
}

vmon_data::~vmon_data(void) 
{ 
  // This destructor *must* exist!  
  // See explanation in "Effective C++" book by Meyers.
}

void vmon_data::Print(void) 
{ 
  // This is the default Print routine for vmon_data (and its
  // subclasses.  If a Print() routine hasn't been specifically
  // defined for a particular subclass, then we aren't interested
  // in printing anything and so we do nothing.
}

void session_data::init(VmonVenusId *venus, VmonSessionId session, 
			VolumeId volume, UserId user, VmonAVSG *avsg, 
			unsigned long starttime, unsigned long endtime, 
			unsigned long cetime,
			long VSEA_size, VmonSessionEvent events[],
			SessionStatistics *stats, CacheStatistics *cachestats)
{
    int i;
    VmonSessionEvent *se1, se2;
    
    Venus.IPAddress = venus->IPAddress;
    Venus.BirthTime = venus->BirthTime;
    Session = session;
    Volume = volume;
    User = user;
    for (i = 0; i < 8 ; i++)
	(&(AVSG.Member0))[i] = (&avsg->Member0)[i];
    StartTime = starttime;
    EndTime = endtime;
    CETime = cetime;
    LogMsg(1000,LogLevel,LogFile,"Session Event init");
    bzero((char *) &Events,(int)sizeof(VmonSessionEventArray));
    for (i=0; i<VSEA_size; i++)
    {
	LogMsg(1000,LogLevel,LogFile,"\t%d\t%d\t%d\t%d\t%d",
	       events[i].Opcode,events[i].SuccessCount,
	       events[i].SigmaT,events[i].SigmaTSquared,
	       events[i].FailureCount);
	/* gross hack to convert record to array */
	se2 = events[i];
	se1 = &((&(Events.Event0))[se2.Opcode]);
	se1->Opcode = se2.Opcode;
	se1->SuccessCount = se2.SuccessCount;
	se1->SigmaT = se2.SigmaT;
	se1->SigmaTSquared = se2.SigmaTSquared;
	se1->FailureCount = se2.FailureCount;
    }
    Stats = *stats;
    CacheStats = *cachestats;
}    

void session_data::Release(void)
{
    session_pool.putSlot((vmon_data *) this);
}

void comm_data::init(VmonVenusId *venus, unsigned long serveripaddress,
		     long serial, unsigned long time, VmonCommEventType type)
{
    Venus.IPAddress = venus->IPAddress;
    Venus.BirthTime = venus->BirthTime;
    ServerIPAddress = serveripaddress;
    SerialNumber = serial;
    Time = time;
    EvType = type;
}

void comm_data::Release(void)
{
    comm_pool.putSlot((vmon_data *) this);
}

void clientCall_data::init(VmonVenusId *venus, long time, long scsize,
			   CallCountEntry *srvcount)
{
    Venus.IPAddress = venus->IPAddress;
    Venus.BirthTime = venus->BirthTime;
    Time = time;
    SrvCount.set(scsize,srvcount);
}    

void clientCall_data::Release(void)
{
    clientCall_pool.putSlot((vmon_data *) this);
}

void clientCall_data::Print(void)
{
  LogMsg(0, LogLevel, LogFile, "clientCall_data::Print: this = %x; Venus = %x; Time = %d\n", 
	 this, Venus.IPAddress, Time);
  SrvCount.Print();
}

void clientMCall_data::init(VmonVenusId *venus, long time, long mscsize,
			    MultiCallEntry *msrvcount)
{
    Venus.IPAddress = venus->IPAddress;
    Venus.BirthTime = venus->BirthTime;
    Time = time;
    MSrvCount.set(mscsize,msrvcount);
}    

void clientMCall_data::Release(void)
{
    clientMCall_pool.putSlot((vmon_data *) this);
}

void clientRVM_data::init(VmonVenusId *venus, long time, RvmStatistics *stats)
{
    Venus.IPAddress = venus->IPAddress;
    Venus.BirthTime = venus->BirthTime;
    Time = time;
    Stats.Malloc = stats->Malloc;
    Stats.Free = stats->Free;
    Stats.FreeBytes = stats->FreeBytes;
    Stats.MallocBytes = stats->MallocBytes;
}    

void clientRVM_data::Release(void)
{
    clientRVM_pool.putSlot((vmon_data *) this);
}

void vcb_data::init(VmonVenusId *venus, long venusinit, long time, VolumeId volume, VCBStatistics *stats)
{
    Venus.IPAddress = venus->IPAddress;
    Venus.BirthTime = venus->BirthTime;
    VenusInit = venusinit;
    Time = time;
    Volume = volume;
    Stats = *stats;
}

void vcb_data::Release(void)
{
    vcb_pool.putSlot((vmon_data *) this);
}

advice_data::advice_data()
{
    Call_Size = 0L;
    Result_Size = 0L;
    Call_Stats = NULL;
    Result_Stats = NULL;
}

advice_data::~advice_data()
{
    delete [] Call_Stats;
    delete [] Result_Stats;
}

void advice_data::init(VmonVenusId *venus, 
		       long time, 
		       UserId user, 
		       AdviceStatistics *stats, 
		       unsigned long call_size, 
		       AdviceCalls call_stats[], 
		       unsigned long result_size, 
		       AdviceResults result_stats[])
{
    Venus.IPAddress = venus->IPAddress;
    Venus.BirthTime = venus->BirthTime;
    Time = time;
    User = user;

    Stats.NotEnabled = stats->NotEnabled;
    Stats.NotValid = stats->NotValid;
    Stats.Outstanding = stats->Outstanding;
    Stats.ASRnotAllowed = stats->ASRnotAllowed;
    Stats.ASRinterval = stats->ASRinterval;
    Stats.VolumeNull = stats->VolumeNull;
    Stats.TotalNumberAttempts = stats->TotalNumberAttempts;

    if (call_size != Call_Size) {
	delete Call_Stats;
	Call_Size = call_size;
	Call_Stats = new AdviceCalls[Call_Size];
    }

    if (result_size != Result_Size) {
	delete Result_Stats;
	Result_Size = result_size;
	Result_Stats = new AdviceResults[Result_Size];
    }

    for (int i=0; i<Call_Size; i++)
	    Call_Stats[i] = call_stats[i];

    for (i=0; i<Result_Size; i++)
	    Result_Stats[i] = result_stats[i];
}

void advice_data::Release(void)
{
    advice_pool.putSlot((vmon_data *) this);
}

miniCache_data::miniCache_data()
{
    VN_Size = VFS_Size = 0L;
    VN_Stats = VFS_Stats = NULL;
}

miniCache_data::~miniCache_data()
{
    delete [] VN_Stats;
    delete [] VFS_Stats;
}

void miniCache_data::init(VmonVenusId *venus,
			 long time,
			 unsigned long vn_size,
			 VmonMiniCacheStat vn_stats[],
			 unsigned long vfs_size,
			 VmonMiniCacheStat vfs_stats[])
{
    Venus.IPAddress = venus->IPAddress;
    Venus.BirthTime = venus->BirthTime;
    Time = time;

    if (vn_size != VN_Size) {
	delete VN_Stats;
	VN_Size = vn_size;
	VN_Stats = new VmonMiniCacheStat[VN_Size];
    }

    if (vfs_size != VFS_Size) {
	delete VFS_Stats;
	VFS_Size = vfs_size;
	VFS_Stats = new VmonMiniCacheStat[VFS_Size];
    }

    for (int i=0; i<vn_size; i++)
	VN_Stats[i] = vn_stats[i];

    for (i=0; i<vfs_size; i++)
	VFS_Stats[i] = vfs_stats[i];

}

void miniCache_data::Release(void)
{
    miniCache_pool.putSlot((vmon_data *) this);
}

void overflow_data::init(VmonVenusId *venus, unsigned long vmstarttime,
			 unsigned long vmendtime, long vmcount,
			 unsigned long rvmstarttime, unsigned long rvmendtime,
			 long rvmcount)
{
    Venus.IPAddress = venus->IPAddress;
    Venus.BirthTime = venus->BirthTime;
    VMStartTime = vmstarttime;
    VMEndTime = vmendtime;
    VMCount = vmcount;
    RVMStartTime = rvmstarttime;
    RVMEndTime = rvmendtime;
    RVMCount = rvmcount;
}

void overflow_data::Release(void)
{
    overflow_pool.putSlot((vmon_data *) this);
}

void srvrCall_data::init(SmonViceId *vice ,unsigned long time, long cbcsize, 
			 CallCountEntry cbcount[], long rcsize,
			 CallCountEntry rescount[], long scsize, 
			 CallCountEntry smoncount[], long vdcsize,
			 CallCountEntry voldcount[], long mcsize, 
			 MultiCallEntry multicount[], SmonStatistics *stats)
{
    Vice.IPAddress = vice->IPAddress;
    Vice.BirthTime = vice->BirthTime;
    Time = time;
    Stats.SystemCPU = stats->SystemCPU;
    Stats.UserCPU = stats->UserCPU;
    Stats.IdleCPU = stats->IdleCPU;
    Stats.BootTime = stats->BootTime;
    Stats.TotalIO = stats->TotalIO;
    CBCount.set(cbcsize,cbcount);
    ResCount.set(rcsize,rescount);
    SmonCount.set(scsize,smoncount);
    VolDCount.set(vdcsize,voldcount);
    MultiCount.set(mcsize,multicount);
}

void srvrCall_data::Release(void)
{
    srvrCall_pool.putSlot((vmon_data *) this);
}

resEvent_data::resEvent_data(void)
{
    // Uses the same array allocation/reallocation trick
    // seen in srvrCall_data::srvrCall_data()
    // only need to initialize resop_size and ResOp, the others
    // are covered by resevent::init

    ResOp_size = 0;
    ResOp = (ResOpEntry *)NULL;
}

void resEvent_data::init(SmonViceId *vice, RPC2_Unsigned time, 
			 VolumeId volid, RPC2_Integer highwatermark,
			 RPC2_Integer allocnumber, RPC2_Integer deallocnumber, 
			 RPC2_Integer resop_size, ResOpEntry resop[])
{
    // this is somewhat simpler than the code in srvrCall_data::init,
    // because none of the fields of ResOpEntry require allocation
    if (resop_size != ResOp_size) {
	if (ResOp != NULL)
	    delete [] ResOp;
	ResOp_size = resop_size;
	ResOp = new ResOpEntry[resop_size];
    }
    Vice.IPAddress = vice->IPAddress;
    Vice.BirthTime = vice->BirthTime;
    Time = time;
    Volid = volid;
    HighWaterMark = highwatermark;
    AllocNumber = allocnumber;
    DeallocNumber = deallocnumber;
    for (int i=0;i<resop_size;i++) {
	ResOp[i].alloccount = resop[i].alloccount;
	ResOp[i].dealloccount = resop[i].dealloccount;
    }
}

void resEvent_data::Release(void)
{
    resEvent_pool.putSlot((vmon_data *) this);
}

Histogram::Histogram() {
    size = 0;
    buckets = NULL;
}

Histogram::~Histogram()  {
    delete [] buckets;
}

void Histogram::set(long _size, HistoElem _buckets[]) {
    if (_size != size) {
	delete [] buckets;
	size = _size;
	buckets = new HistoElem[size];
    }
    for (int i =0; i<size; i++)
	buckets[i] = _buckets[i];
}

void rvmResEvent_data::init(SmonViceId vice, unsigned long time, unsigned long volid,
			    FileResStats fileres, DirResStats dirres,
			    long lsh_size, HistoElem logsizehisto[], long lmh_size, 
			    HistoElem logmaxhisto[], ResConflictStats conflicts, 
			    long shh_size, HistoElem succhierhist[],
			    long fhh_size, HistoElem failhierhist[], ResLogStats reslog,
			    long vlh_size, HistoElem varloghisto[], 
			    long ls_size, HistoElem logsize[]) {
    Vice = vice;
    Time = time;
    VolID = volid;
    FileRes = fileres;
    DirRes = dirres;
    LogSizeHisto.set(lsh_size,logsizehisto);
    LogMaxHisto.set(lmh_size,logmaxhisto);
    Conflicts = conflicts;
    SuccHierHist.set(shh_size,succhierhist);
    FailHierHist.set(fhh_size,failhierhist);
    ResLog = reslog;
    VarLogHisto.set(vlh_size,varloghisto);
    LogSize.set(ls_size,logsizehisto);
}

void rvmResEvent_data::Release() {
    rvmResEvent_pool.putSlot((vmon_data *) this);
}

void srvOverflow_data::init(SmonViceId *vice, RPC2_Unsigned time,
			    RPC2_Unsigned starttime, RPC2_Unsigned endtime,
			    RPC2_Integer count)
{
    Vice.IPAddress = vice->IPAddress;
    Vice.BirthTime = vice->BirthTime;
    Time = time;
    StartTime = starttime;
    EndTime = endtime;
    Count = count;
}

void srvOverflow_data::Release(void)
{
    srvOverflow_pool.putSlot((vmon_data *) this);
}

iotInfo_data::iotInfo_data(void)
{
    AppNameLen = 0;
    AppName = NULL;
}

iotInfo_data::~iotInfo_data(void)
{
    delete [] AppName;
}

void iotInfo_data::init(VmonVenusId *venus, IOT_INFO *info,
			RPC2_Integer appnamelen, RPC2_String appname)
{
    Venus.IPAddress = venus->IPAddress;
    Venus.BirthTime = venus->BirthTime;

    Info.Tid  		= info->Tid;
    Info.ResOpt		= info->ResOpt;
    Info.ElapsedTime	= info->ElapsedTime;
    Info.ReadSetSize 	= info->ReadSetSize;
    Info.WriteSetSize	= info->WriteSetSize;
    Info.ReadVolNum	= info->ReadVolNum;
    Info.WriteVolNum	= info->WriteVolNum;
    Info.Validation	= info->Validation;
    Info.InvalidSize 	= info->InvalidSize;
    Info.BackupObjNum	= info->BackupObjNum;
    Info.LifeCycle	= info->LifeCycle;
    Info.PredNum	= info->PredNum;
    Info.SuccNum	= info->SuccNum;
    AppNameLen		= appnamelen;
    AppName 		= new unsigned char[appnamelen];
    strcpy((char *)AppName, (char *)appname);
}

void iotInfo_data::Release(void)
{
    iotInfo_pool.putSlot((vmon_data *) this);
}

void iotStat_data::init(VmonVenusId *venus, RPC2_Integer time, IOT_STAT *stats)
{
    Venus.IPAddress = venus->IPAddress;
    Venus.BirthTime = venus->BirthTime;
    Time = time;

    Stats.MaxElapsedTime = stats->MaxElapsedTime;
    Stats.AvgElapsedTime = stats->AvgElapsedTime;
    Stats.MaxReadSetSize = stats->MaxReadSetSize;
    Stats.AvgReadSetSize = stats->AvgReadSetSize;
    Stats.MaxWriteSetSize = stats->MaxWriteSetSize;
    Stats.AvgWriteSetSize = stats->AvgWriteSetSize;
    Stats.MaxReadVolNum = stats->MaxReadVolNum;
    Stats.AvgReadVolNum = stats->AvgReadVolNum;
    Stats.MaxWriteVolNum = stats->MaxWriteVolNum;
    Stats.AvgWriteVolNum = stats->AvgWriteVolNum;
    Stats.Committed = stats->Committed;
    Stats.Pending = stats->Pending;
    Stats.Resolved = stats->Resolved;
    Stats.Repaired = stats->Repaired;
    Stats.OCCRerun = stats->OCCRerun;
}

void iotStat_data::Release(void)
{
    iotStat_pool.putSlot((vmon_data *) this);
}

void subtree_data::init(VmonVenusId *venus, RPC2_Integer time,
			LocalSubtreeStats *stats)
{
    Venus.IPAddress = venus->IPAddress;
    Venus.BirthTime = venus->BirthTime;
    Time = time;

    Stats.SubtreeNum 		= stats->SubtreeNum;
    Stats.MaxSubtreeSize	= stats->MaxSubtreeSize;
    Stats.AvgSubtreeSize 	= stats->AvgSubtreeSize;
    Stats.MaxSubtreeHgt 	= stats->MaxSubtreeHgt;
    Stats.AvgSubtreeHgt 	= stats->AvgSubtreeHgt;
    Stats.MaxMutationNum 	= stats->MaxMutationNum;
    Stats.AvgMutationNum 	= stats->AvgMutationNum;
}

void subtree_data::Release(void)
{
    subtree_pool.putSlot((vmon_data *) this);
}

void repair_data::init(VmonVenusId *venus, RPC2_Integer time,
		       RepairSessionStats *stats)
{
    Venus.IPAddress = venus->IPAddress;
    Venus.BirthTime = venus->BirthTime;
    Time = time;

    Stats.SessionNum 		= stats->SessionNum;
    Stats.CommitNum 		= stats->CommitNum;
    Stats.AbortNum 		= stats->AbortNum;
    Stats.CheckNum	 	= stats->CheckNum;
    Stats.PreserveNum 		= stats->PreserveNum;
    Stats.DiscardNum 		= stats->DiscardNum;
    Stats.RemoveNum 		= stats->RemoveNum;
    Stats.GlobalViewNum 	= stats->GlobalViewNum;
    Stats.LocalViewNum 		= stats->LocalViewNum;
    Stats.KeepLocalNum 		= stats->KeepLocalNum;
    Stats.ListLocalNum 		= stats->ListLocalNum;
    Stats.NewCommand1Num	= stats->NewCommand1Num;
    Stats.NewCommand2Num	= stats->NewCommand2Num;
    Stats.NewCommand3Num	= stats->NewCommand3Num;
    Stats.NewCommand4Num	= stats->NewCommand4Num;
    Stats.NewCommand5Num	= stats->NewCommand5Num;
    Stats.NewCommand6Num	= stats->NewCommand6Num;
    Stats.NewCommand7Num	= stats->NewCommand7Num;
    Stats.NewCommand8Num	= stats->NewCommand8Num;
    Stats.RepMutationNum 	= stats->RepMutationNum;
    Stats.MissTargetNum		= stats->MissTargetNum;
    Stats.MissParentNum		= stats->MissParentNum;
    Stats.AclDenyNum		= stats->AclDenyNum;
    Stats.UpdateUpdateNum	= stats->UpdateUpdateNum;
    Stats.NameNameNum		= stats->NameNameNum;
    Stats.RemoveUpdateNum	= stats->RemoveUpdateNum;
}

void repair_data::Release(void)
{
    repair_pool.putSlot((vmon_data *) this);
}

void rwsStat_data::init(VmonVenusId *venus, RPC2_Integer time, ReadWriteSharingStats *stats)
{
    Venus.IPAddress = venus->IPAddress;
    Venus.BirthTime = venus->BirthTime;
    Time = time;

    Stats.Vid = stats->Vid;
    Stats.RwSharingCount = stats->RwSharingCount;
    Stats.DiscReadCount = stats->DiscReadCount;
    Stats.DiscDuration = stats->DiscDuration;
}

void rwsStat_data::Release(void)
{
    rwsStat_pool.putSlot((vmon_data *) this);
}

bufpool::bufpool(dataClass Type)
{
  type = Type;
  Pool = NULL;
  Lock_Init(&lock);
}

vmon_data *bufpool::getSlot(void)
{
    vmon_data *temp;
    
    ObtainWriteLock(&lock);
    
    if (Pool == NULL) {
	switch (type) {
	case SESSION:
	    temp = (vmon_data*) new (session_data);
	    break;
	case COMM:
	    temp = (vmon_data*) new (comm_data);
	    break;
	case CLNTCALL:
	    temp = (vmon_data*) new (clientCall_data);
	    break;
	case CLNTMCALL:
	    temp = (vmon_data*) new (clientMCall_data);
	    break;
	case CLNTRVM:
	    temp = (vmon_data*) new (clientRVM_data);
	    break;
	case VCB:
	    temp = (vmon_data*) new (vcb_data);
	    break;
	case ADVICE:
	    temp = (vmon_data*) new (advice_data);
	    break;
	case MINICACHE:
	    temp = (vmon_data*) new (miniCache_data);
	    break;
	case OVERFLOW:
	    temp = (vmon_data*) new (overflow_data);
	    break;
	case SRVCALL:
	    temp = (vmon_data*) new (srvrCall_data);
	    break;
	case SRVRES:
	    temp = (vmon_data*) new (resEvent_data);
	    break;
	case SRVRVMRES:
	    temp = (vmon_data*) new (rvmResEvent_data);
	    break;
	case SRVOVRFLW:
	    temp = (vmon_data*) new (srvOverflow_data);
	    break;
	case IOTINFO:
	    temp = (vmon_data*) new (iotInfo_data);
	    break;
	case IOTSTAT:
	    temp = (vmon_data*) new (iotStat_data);
	    break;
	case SUBTREE:
	    temp = (vmon_data*) new (subtree_data);
	    break;
	case REPAIR:
	    temp = (vmon_data*) new (repair_data);
	    break;
	case RWSSTAT:
	    temp = (vmon_data*) new (rwsStat_data);
	    break;
	case dataClass_last_tag:
	default:
	    CODA_ASSERT(0);
	}
    } else {
	temp = (vmon_data*) Pool;
	Pool = Pool->Next();
	temp->NotOnList();
    }
    ReleaseWriteLock(&lock);
    return temp;
}


void bufpool::putSlot(vmon_data *slot)
{
    ObtainWriteLock(&lock);
    slot->SetNext(Pool);
    Pool = slot;
    ReleaseWriteLock(&lock);
}

