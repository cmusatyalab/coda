#ifndef _BLURB_
#define _BLURB_
/*

            Coda: an Experimental Distributed File System
                             Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/utils-src/mond/wind.c,v 3.7 2003/05/23 18:27:58 jaharkes Exp $";
#endif /*_BLURB_*/



#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <netinet/in.h>
#include <sys/file.h>
#include <stdlib.h>
#include <sysent.h>
#include <stdio.h>
#include "coda_assert.h"
#include "lock.h"
#ifdef __cplusplus
}
#endif __cplusplus

#include <stdarg.h>
#include "mond.h"
#include "mondgen.h"
#include "report.h"
#include "vargs.h"
#include "data.h"
#include "bbuf.h"
#include "mondutil.h"

extern FILE *DataFile;

static void PutViceId(SmonViceId*);
static void PutSmonStatistics(SmonStatistics*);
static void PutVenusId(VmonVenusId *);
void inline PutLong(unsigned long);
void inline PutLong(long);
void inline PutString(unsigned char *, unsigned long);
static void PutAVSG(VmonAVSG *);
static void PutMCStat(long,VmonMiniCacheStat*);
static void PutEventArray(VmonSessionEventArray*);
static void PutSessionStats(SessionStatistics*);
static void PutCacheStats(CacheStatistics*);
static void PutResOpArray(long,ResOpEntry[]);
static void PutCallCountArray(long,CallCountEntry[]);
static void PutMultiCallArray(long,MultiCallEntry[]);
static void PutFileResStats(FileResStats FileRes);
static void PutDirResStats(DirResStats DirRes);
static void PutHistogram(Histogram *hist);
static void PutResConflictStats(ResConflictStats Conflicts);
static void PutResLogStats(ResLogStats ResLog);
static void PutAdviceStats(AdviceStatistics*);
static void PutCallStatArray(long,AdviceCalls[]);
static void PutResultStatArray(long,AdviceResults[]);
static void PutRvmStats(RvmStatistics*);
static void PutVCBStats(VCBStatistics*);
static void PutIotInfo(IOT_INFO*);
static void PutIotStats(IOT_STAT*);
static void PutSubtreeStats(LocalSubtreeStats*);
static void PutRepairStats(RepairSessionStats*);
static void PutRwsStats(ReadWriteSharingStats*);

long ReportSession(VmonVenusId *Venus, VmonSessionId Session, VolumeId Volume,
		   UserId User, VmonAVSG *AVSG, RPC2_Unsigned StartTime,
		   RPC2_Unsigned EndTime, RPC2_Unsigned CETime,
		   VmonSessionEventArray *Events,
		   SessionStatistics *Stats, CacheStatistics *CacheStats) 
{
    int code = 0;

    PutLong((long) SESSION_TAG);
    PutVenusId(Venus);
    PutLong(Session);
    PutLong(Volume);
    PutLong(User);
    PutAVSG(AVSG);
    PutLong(StartTime);
    PutLong(EndTime);
    PutLong(CETime);
    PutEventArray(Events);
    PutSessionStats(Stats);
    PutCacheStats(CacheStats);
    PutLong((long) END_GUARD);
    return(code);
}

long ReportCommEvent(VmonVenusId *Venus, RPC2_Unsigned ServerIPAddress,
		     long SerialNumber, RPC2_Unsigned Time, VmonCommEventType Type) 
{
    int code = 0;

    PutLong((long) COMM_TAG);
    PutVenusId(Venus);
    PutLong(ServerIPAddress);
    PutLong(SerialNumber);
    PutLong(Time);
    PutLong((unsigned long) Type);
    PutLong((long) END_GUARD);

    return(code);
}

long ReportClntCall(VmonVenusId *Venus, long Time, 
		    class callCountArray *SrvCount) 
{
    int code = 0;

    PutLong((long) CLNTCALL_TAG);
    PutVenusId(Venus);
    PutLong(Time);
    PutCallCountArray(SrvCount->getSize(),SrvCount->getArray());
    PutLong((long) END_GUARD);

    return code;
}

long ReportClntMCall(VmonVenusId *Venus, long Time, 
		     class multiCallArray *MSrvCount) 
{
    int code = 0;

    PutLong((long) CLNTMCALL_TAG);
    PutVenusId(Venus);
    PutLong(Time);
    PutMultiCallArray(MSrvCount->getSize(),MSrvCount->getArray());
    PutLong((long) END_GUARD);

    return code;
}

long ReportClntRVM(VmonVenusId *Venus, long Time, 
		   RvmStatistics *Stats)
{
    int code = 0;

    PutLong((long) CLNTRVM_TAG);
    PutVenusId(Venus);
    PutLong(Time);
    PutRvmStats(Stats);
    PutLong((long) END_GUARD);

    return code;
}

long ReportVCB(VmonVenusId *Venus, long VenusInit, long Time,
	       VolumeId Volume, VCBStatistics *Stats)
{
    int code = 0;

    PutLong((long) VCB_TAG);
    PutVenusId(Venus);
    PutLong(VenusInit);
    PutLong(Time);
    PutLong(Volume);
    PutVCBStats(Stats);
    PutLong((long) END_GUARD);

    return code;
}

long ReportAdviceCall(VmonVenusId *Venus, long Time,
		      UserId User, AdviceStatistics *Stats,
		      unsigned long Call_Size, AdviceCalls *Call_Stats, 
		      unsigned long Result_Size, AdviceResults *Result_Stats)
{
    int code = 0;

    
    PutLong((long) ADVICE_TAG);
    PutVenusId(Venus);
    PutLong(Time);
    PutLong(User);
    PutAdviceStats(Stats);
    PutCallStatArray(Call_Size, Call_Stats);
    PutResultStatArray(Result_Size, Result_Stats);
    PutLong((long) END_GUARD);

    return code;
}

long ReportMiniCacheCall(VmonVenusId *Venus, long Time,
			 unsigned long vn_size, 
			 VmonMiniCacheStat *vn_stat,
			 unsigned long vfs_size,
			 VmonMiniCacheStat *vfs_stat)
{
    int code = 0;

    PutLong((long) MINICACHE_TAG);
    PutVenusId(Venus);
    PutLong(Time);
    PutMCStat(vn_size,vn_stat);
    PutMCStat(vfs_size,vfs_stat);
    PutLong((long) END_GUARD);

    return(code);
}

long ReportOverflow(VmonVenusId *Venus, RPC2_Unsigned VMStartTime,
		    RPC2_Unsigned VMEndTime, RPC2_Integer VMCount,
		    RPC2_Unsigned RVMStartTime,
		    RPC2_Unsigned RVMEndTime, RPC2_Integer RVMCount) 
{
    int code = 0;

    PutLong((long) OVERFLOW_TAG);
    PutVenusId(Venus);
    PutLong(VMStartTime);
    PutLong(VMEndTime);
    PutLong(VMCount);
    PutLong(RVMStartTime);
    PutLong(RVMEndTime);
    PutLong(RVMCount);
    PutLong((long) END_GUARD);

    return(code);
}

long ReportSrvrCall(SmonViceId *Vice, RPC2_Unsigned Time, 
		    class callCountArray *CBCount,
		    class callCountArray *ResCount,
		    class callCountArray *SmonCount,
		    class callCountArray *VolDCount,
		    class multiCallArray *MultiCount,
		    SmonStatistics *Stats)
{
    int code = 0;
    PutLong((long) SRVCALL_TAG);
    PutViceId(Vice);
    PutLong(Time);
    PutCallCountArray(CBCount->getSize(),CBCount->getArray());
    PutCallCountArray(ResCount->getSize(),ResCount->getArray());
    PutCallCountArray(SmonCount->getSize(),SmonCount->getArray());
    PutCallCountArray(VolDCount->getSize(),VolDCount->getArray());
    PutMultiCallArray(MultiCount->getSize(),MultiCount->getArray());
    PutSmonStatistics(Stats);
    PutLong((long) END_GUARD);
    
    return code;
}

long ReportResEvent(SmonViceId *Vice, RPC2_Unsigned Time, 
		    VolumeId Volid, RPC2_Integer HighWaterMark,
		    RPC2_Integer AllocNumber, RPC2_Integer DeallocNumber,
		    RPC2_Integer ResOpSize, ResOpEntry ResOp[])
{
    int code = 0;

    PutLong((long) SRVRES_TAG);
    PutViceId(Vice);
    PutLong(Time);
    PutLong(Volid);
    PutLong(HighWaterMark);
    PutLong(AllocNumber);
    PutLong(DeallocNumber);
    PutLong(ResOpSize);
    PutResOpArray(ResOpSize,ResOp);
    PutLong((long) END_GUARD);
    
    return code;
}

long ReportRvmResEvent(SmonViceId Vice, unsigned long Time, unsigned long VolID, 
		       FileResStats FileRes, DirResStats DirRes, 
		       Histogram *LogSizeHisto, Histogram *LogMaxHisto, 
		       ResConflictStats Conflicts, Histogram *SuccHierHist, 
		       Histogram *FailHierHist, ResLogStats ResLog, 
		       Histogram *VarLogHisto, Histogram *LogSize) {
    long code =0;
    
    PutLong((long) SRVRVMRES_TAG);
    PutViceId(&Vice);
    PutLong(Time);
    PutLong(VolID);
    PutFileResStats(FileRes);
    PutDirResStats(DirRes);
    PutHistogram(LogSizeHisto);
    PutHistogram(LogMaxHisto);
    PutResConflictStats(Conflicts);
    PutHistogram(SuccHierHist);
    PutHistogram(FailHierHist);
    PutResLogStats(ResLog);
    PutHistogram(VarLogHisto);
    PutHistogram(LogSize);
    PutLong((long) END_GUARD);

    return code;
}


long ReportSrvOverflow(SmonViceId *Vice, unsigned long Time,
		       unsigned long StartTime, unsigned long EndTime,
		       long Count)
{
    long code =0;

    PutLong((long) SRVOVRFLW_TAG);
    PutViceId(Vice);
    PutLong(Time);
    PutLong(StartTime);
    PutLong(EndTime);
    PutLong(Count);
    PutLong((long) END_GUARD);

    return code;
}

static void PutIotInfo(IOT_INFO *info)
{
    info->Tid 		= htonl(info->Tid);
    info->ResOpt 	= htonl(info->ResOpt);
    info->ElapsedTime 	= htonl(info->ElapsedTime);
    info->ReadSetSize 	= htonl(info->ReadSetSize);
    info->WriteSetSize 	= htonl(info->WriteSetSize);
    info->ReadVolNum 	= htonl(info->ReadVolNum);
    info->WriteVolNum 	= htonl(info->WriteVolNum);
    info->Validation 	= htonl(info->Validation);
    info->InvalidSize 	= htonl(info->InvalidSize);
    info->BackupObjNum 	= htonl(info->BackupObjNum);
    info->LifeCycle 	= htonl(info->LifeCycle);
    info->PredNum 	= htonl(info->PredNum);
    info->SuccNum 	= htonl(info->SuccNum);
    
    if (fwrite((char *)info, (int)sizeof(IOT_INFO), 1, DataFile) != 1)
      Die("PutIotInfo: fwrite");
}

long ReportIotInfoCall(VmonVenusId *Venus, IOT_INFO *Info, RPC2_Integer AppNameLen,
		       RPC2_String AppName)
{
    long code = 0;
    PutLong((long)IOTINFO_TAG);
    PutVenusId(Venus);

    PutIotInfo(Info);
    PutString(AppName, AppNameLen);
    PutLong((long) END_GUARD);

    return code;
}

void PutIotStats(IOT_STAT *stats)
{
    stats->MaxElapsedTime = htonl(stats->MaxElapsedTime);
    stats->AvgElapsedTime = htonl(stats->AvgElapsedTime);
    stats->MaxReadSetSize = htonl(stats->MaxReadSetSize);
    stats->AvgReadSetSize = htonl(stats->AvgReadSetSize);
    stats->MaxWriteSetSize = htonl(stats->MaxWriteSetSize);
    stats->AvgWriteSetSize = htonl(stats->AvgWriteSetSize);
    stats->MaxReadVolNum = htonl(stats->MaxReadVolNum);
    stats->AvgReadVolNum = htonl(stats->AvgReadVolNum);
    stats->MaxWriteVolNum = htonl(stats->MaxWriteVolNum);
    stats->AvgWriteVolNum = htonl(stats->AvgWriteVolNum);
    stats->Committed = htonl(stats->Committed);
    stats->Pending = htonl(stats->Pending);
    stats->Resolved = htonl(stats->Resolved);
    stats->Repaired = htonl(stats->Repaired);
    stats->OCCRerun = htonl(stats->OCCRerun);

    if (fwrite((char *)stats, (int)sizeof(IOT_STAT), 1, DataFile) != 1)
      Die("PutIotStats: fwrite");
}

long ReportIotStatsCall(VmonVenusId *Venus, RPC2_Integer Time, IOT_STAT *Stats)
{
    long code = 0;
    PutLong((long)IOTSTAT_TAG);
    PutVenusId(Venus);
    PutLong(Time);

    PutIotStats(Stats);
    PutLong((long) END_GUARD);

    return code;
}

void PutSubtreeStats(LocalSubtreeStats *stats)
{
    stats->SubtreeNum = htonl(stats->SubtreeNum);
    stats->MaxSubtreeSize = htonl(stats->MaxSubtreeSize);
    stats->AvgSubtreeSize = htonl(stats->AvgSubtreeSize);
    stats->MaxSubtreeHgt = htonl(stats->MaxSubtreeHgt);
    stats->AvgSubtreeHgt = htonl(stats->AvgSubtreeHgt);
    stats->MaxMutationNum = htonl(stats->MaxMutationNum);
    stats->AvgMutationNum = htonl(stats->AvgMutationNum);

    if (fwrite((char *)stats, (int)sizeof(LocalSubtreeStats), 1, DataFile) != 1)
      Die("PutSubtreeStats: fwrite");
}

long ReportSubtreeCall(VmonVenusId *Venus, RPC2_Integer Time, LocalSubtreeStats *Stats)
{
    long code = 0;
    PutLong((long)SUBTREE_TAG);
    PutVenusId(Venus);
    PutLong(Time);
    PutSubtreeStats(Stats);
    PutLong((long) END_GUARD);

    return code;
}

void PutRepairStats(RepairSessionStats *stats)
{
    stats->SessionNum = htonl(stats->SessionNum);
    stats->CommitNum = htonl(stats->CommitNum);
    stats->AbortNum = htonl(stats->AbortNum);
    stats->CheckNum = htonl(stats->CheckNum);
    stats->PreserveNum = htonl(stats->PreserveNum);
    stats->DiscardNum = htonl(stats->DiscardNum);
    stats->RemoveNum = htonl(stats->RemoveNum);
    stats->GlobalViewNum = htonl(stats->GlobalViewNum);
    stats->LocalViewNum = htonl(stats->LocalViewNum);
    stats->KeepLocalNum = htonl(stats->KeepLocalNum);
    stats->ListLocalNum = htonl(stats->ListLocalNum);
    stats->NewCommand1Num = htonl(stats->NewCommand1Num);
    stats->NewCommand2Num = htonl(stats->NewCommand2Num);
    stats->NewCommand3Num = htonl(stats->NewCommand3Num);
    stats->NewCommand4Num = htonl(stats->NewCommand4Num);
    stats->NewCommand5Num = htonl(stats->NewCommand5Num);
    stats->NewCommand6Num = htonl(stats->NewCommand6Num);
    stats->NewCommand7Num = htonl(stats->NewCommand7Num);
    stats->NewCommand8Num = htonl(stats->NewCommand8Num);
    stats->RepMutationNum = htonl(stats->RepMutationNum);
    stats->MissTargetNum = htonl(stats->MissTargetNum);
    stats->MissParentNum = htonl(stats->MissParentNum);
    stats->AclDenyNum = htonl(stats->AclDenyNum);
    stats->UpdateUpdateNum = htonl(stats->UpdateUpdateNum);
    stats->NameNameNum = htonl(stats->NameNameNum);
    stats->RemoveUpdateNum = htonl(stats->RemoveUpdateNum);

    if (fwrite((char *)stats, (int)sizeof(RepairSessionStats), 1, DataFile) != 1)
      Die("PutRepairStats: fwrite");
}

long ReportRepairCall(VmonVenusId *Venus, RPC2_Integer Time, RepairSessionStats *Stats)
{
    long code = 0;
    PutLong((long)REPAIR_TAG);
    PutVenusId(Venus);
    PutLong(Time);
    PutRepairStats(Stats);
    PutLong((long) END_GUARD);

    return code;
}

void PutRwsStats(ReadWriteSharingStats *stats)
{
    stats->Vid = htonl(stats->Vid);
    stats->RwSharingCount = htonl(stats->RwSharingCount);
    stats->DiscReadCount = htonl(stats->DiscReadCount);
    stats->DiscDuration = htonl(stats->DiscDuration);

    if (fwrite((char *)stats, (int)sizeof(ReadWriteSharingStats), 1, DataFile) != 1)
      Die("PutRwsStats: fwrite");
}

long ReportRwsStatsCall(VmonVenusId *Venus, RPC2_Integer Time, ReadWriteSharingStats *Stats)
{
    long code = 0;
    PutLong((long)RWSSTAT_TAG);
    PutVenusId(Venus);
    PutLong(Time);

    PutRwsStats(Stats);
    PutLong((long) END_GUARD);

    return code;
}


static void PutViceId(SmonViceId *Vice) {
    Vice->IPAddress = htonl(Vice->IPAddress);
    Vice->BirthTime = htonl(Vice->BirthTime);
    if (fwrite((char *)Vice, (int)sizeof(SmonViceId),1,DataFile) != 1)
	Die("PutViceId: fwrite");
}

static void PutSmonStatistics(SmonStatistics *stats) {
    stats->SystemCPU = htonl(stats->SystemCPU);
    stats->UserCPU = htonl(stats->UserCPU);
    stats->IdleCPU = htonl(stats->IdleCPU);
    stats->BootTime = htonl(stats->BootTime);
    stats->TotalIO = htonl(stats->TotalIO);
    if (fwrite((char *)stats, (int)sizeof(SmonStatistics),1,DataFile) != 1)
	Die("PutViceId: fwrite");
}

static void PutMCStat(long size, VmonMiniCacheStat *stats)
{
    PutLong(size);
    for (int i=0; i<size; i++) {
	stats[i].Opcode = htonl(stats[i].Opcode);
	stats[i].Entries = htonl(stats[i].Entries);
	stats[i].SatIntrn = htonl(stats[i].SatIntrn);
	stats[i].UnsatIntrn = htonl(stats[i].UnsatIntrn);
	stats[i].GenIntrn = htonl(stats[i].GenIntrn);
	if (fwrite((char*)&(stats[i]), (int)sizeof(VmonMiniCacheStat),1,DataFile) != 1)
	    Die("PutMCStat: fwrite");
    }
}

static void PutCallCountArray(long size, CallCountEntry array[])
{
    PutLong(size);
    int i;
    for (i=0; i<size; i++)
    {
	/* for now, we'll just leave strings alone... */
	if (fwrite((char *)array[i].name,(int)sizeof(char[SIGCHAR]),
		   1,DataFile) != 1)
	    Die("Putting names of CallCount");
	PutLong(array[i].countent);
	PutLong(array[i].countexit);
	PutLong(array[i].tsec);
	PutLong(array[i].tusec);
	PutLong(array[i].counttime);
    }
}

static void PutMultiCallArray(long size, MultiCallEntry array[])
{
    PutLong(size);
    int i;
    for (i=0; i<size; i++)
    {
	/* for now, we'll just leave strings alone... */
	if (fwrite((char *)array[i].name,(int)sizeof(char[SIGCHAR]),
		   1,DataFile) != 1)
	    Die("Putting names of MultiCall");
	PutLong(array[i].countent);
	PutLong(array[i].countexit);
	PutLong(array[i].tsec);
	PutLong(array[i].tusec);
	PutLong(array[i].counttime);
	PutLong(array[i].counthost);
    }
}

static void PutVenusId(VmonVenusId *Venus) {
    Venus->IPAddress = htonl(Venus->IPAddress);
    Venus->BirthTime = htonl(Venus->BirthTime);
    if (fwrite((char *)Venus, (int)sizeof(VmonVenusId), 1, DataFile) != 1)
	Die("PutVenusId: fwrite");
}


static void PutAVSG(VmonAVSG *AVSG) {
    for (int i = 0; i < 8; i++)
	(&AVSG->Member0)[i] = htonl((&AVSG->Member0)[i]);
    if (fwrite((char *)AVSG, (int)sizeof(VmonAVSG), 1, DataFile) != 1)
	Die("PutAVSG: fwrite");
}


static void PutEventArray(VmonSessionEventArray *Events) {
    for (int i = 0; i < nVSEs; i++) {
	VmonSessionEvent *se = &((&Events->Event0)[i]);
	se->Opcode = htonl(se->Opcode);
	se->SuccessCount = htonl(se->SuccessCount);
	se->SigmaT = htonl(se->SigmaT);
	se->SigmaTSquared = htonl(se->SigmaTSquared);
	se->FailureCount = htonl(se->FailureCount);
    }
    if (fwrite((char *)Events, (int)sizeof(VmonSessionEventArray), 1, DataFile) != 1)
	Die("PutEventArray: fwrite");
}

static void PutSessionStats(SessionStatistics *stats)
{
    stats->BytesStart = htonl(stats->BytesStart);
    stats->BytesEnd = htonl(stats->BytesEnd);
    stats->BytesHighWater = htonl(stats->BytesHighWater);
    stats->EntriesStart = htonl(stats->EntriesStart);
    stats->EntriesEnd = htonl(stats->EntriesEnd);
    stats->EntriesHighWater = htonl(stats->EntriesHighWater);
    stats->RecordsCancelled = htonl(stats->RecordsCancelled);
    stats->RecordsCommitted = htonl(stats->RecordsCommitted);
    stats->RecordsAborted = htonl(stats->RecordsAborted);
    stats->FidsRealloced = htonl(stats->FidsRealloced);
    stats->BytesBackFetched = htonl(stats->BytesBackFetched);
    stats->SystemCPU = htonl(stats->SystemCPU);
    stats->UserCPU = htonl(stats->UserCPU);
    stats->IdleCPU = htonl(stats->IdleCPU);
    stats->CacheHighWater = htonl(stats->CacheHighWater);

    if (fwrite((char *)stats, (int)sizeof(SessionStatistics),1,DataFile) != 1)
	Die("PutSessionStatistics: fwrite");
}

static void PutCacheStats(CacheStatistics *stats)
{
    stats->HoardAttrHit.Count = htonl(stats->HoardAttrHit.Count);
    stats->HoardAttrMiss.Blocks = htonl(stats->HoardAttrMiss.Blocks);
    stats->HoardAttrNoSpace.Count = htonl(stats->HoardAttrNoSpace.Count);
    stats->HoardDataHit.Blocks = htonl(stats->HoardDataHit.Blocks);
    stats->HoardDataMiss.Count = htonl(stats->HoardDataMiss.Count);
    stats->HoardDataNoSpace.Blocks = htonl(stats->HoardDataNoSpace.Blocks);
    stats->NonHoardAttrHit.Count = htonl(stats->NonHoardAttrHit.Count);
    stats->NonHoardAttrMiss.Blocks = htonl(stats->NonHoardAttrMiss.Blocks);
    stats->NonHoardAttrNoSpace.Count = htonl(stats->NonHoardAttrNoSpace.Count);
    stats->NonHoardDataHit.Blocks = htonl(stats->NonHoardDataHit.Blocks);
    stats->NonHoardDataMiss.Count = htonl(stats->NonHoardDataMiss.Count);
    stats->NonHoardDataNoSpace.Blocks = htonl(stats->NonHoardDataNoSpace.Blocks);
    stats->UnknownHoardAttrHit.Count = htonl(stats->UnknownHoardAttrHit.Count);
    stats->UnknownHoardAttrMiss.Blocks = htonl(stats->UnknownHoardAttrMiss.Blocks);
    stats->UnknownHoardAttrNoSpace.Count = htonl(stats->UnknownHoardAttrNoSpace.Count);
    stats->UnknownHoardDataHit.Blocks = htonl(stats->UnknownHoardDataHit.Blocks);
    stats->UnknownHoardDataMiss.Count = htonl(stats->UnknownHoardDataMiss.Count);
    stats->UnknownHoardDataNoSpace.Blocks = htonl(stats->UnknownHoardDataNoSpace.Blocks);

    if (fwrite((char *)stats, (int)sizeof(CacheStatistics), 1, DataFile) != 1)
	Die("PutCacheStatistics: fwrite");
}

inline void PutLong(unsigned long Value) {
    Value = htonl(Value);
    if (fwrite((char *)&Value, (int)sizeof(unsigned long), 1, DataFile) != 1)
	Die("PutLong: fwrite");
}

inline void PutString(unsigned char *Str, unsigned long Len) {
    if (fwrite((char *)&Len, (int)sizeof(unsigned long), 1, DataFile) != 1)
      Die("PutString: fwrite");
    if (fwrite((char *)Str, (int)sizeof(unsigned char), Len, DataFile) != Len)
      Die("PutString: fwrite");
}

inline void PutLong(long Value) {
    Value = htonl(Value);
    if (fwrite((char *)&Value, (int)sizeof(long), 1, DataFile) != 1)
	Die("PutLong: fwrite");
}

static void PutResOpArray(long size, ResOpEntry array[])
{
    for (int i=0;i<size;i++) {
	PutLong(array[i].alloccount);
	PutLong(array[i].dealloccount);
    }
}

static void PutFileResStats(FileResStats FileRes) {
    FileRes.Resolves = htonl(FileRes.Resolves);
    FileRes.NumSucc = htonl(FileRes.NumSucc);
    FileRes.NumConf = htonl(FileRes.NumConf);
    FileRes.RuntForce = htonl(FileRes.RuntForce);
    FileRes.WeakEq = htonl(FileRes.WeakEq);
    FileRes.NumReg = htonl(FileRes.NumReg);
    FileRes.UsrResolver = htonl(FileRes.UsrResolver);
    FileRes.SuccUsrResolver = htonl(FileRes.SuccUsrResolver);
    FileRes.PartialVSG = htonl(FileRes.PartialVSG);

    if (fwrite((char*)&FileRes, (int)sizeof(FileResStats), 1, DataFile) != 1)
	Die("PutFileResStats: fwrite");
}

static void PutDirResStats(DirResStats DirRes) {

    DirRes.Resolves = htonl(DirRes.Resolves);
    DirRes.NumSucc = htonl(DirRes.NumSucc);
    DirRes.NumConf = htonl(DirRes.NumConf);
    DirRes.NumNoWork = htonl(DirRes.NumNoWork);
    DirRes.Problems = htonl(DirRes.Problems);
    DirRes.PartialVSG = htonl(DirRes.PartialVSG);

    if (fwrite((char*)&DirRes, (int)sizeof(DirResStats), 1, DataFile) != 1)
	Die("PutDirResStats: fwrite");
}

static void PutHistogram(Histogram *hist) {

    PutLong(hist->size);
    for (int i=0; i<hist->size; i++)
	PutLong(hist->buckets[i].bucket);

}

static void PutResConflictStats(ResConflictStats Conflicts) {
    Conflicts.NameName = htonl(Conflicts.NameName);
    Conflicts.RemoveUpdate = htonl(Conflicts.RemoveUpdate);
    Conflicts.UpdateUpdate = htonl(Conflicts.UpdateUpdate);
    Conflicts.Rename = htonl(Conflicts.Rename);
    Conflicts.LogWrap = htonl(Conflicts.LogWrap);
    Conflicts.Other = htonl(Conflicts.Other);

    if (fwrite((char*)&Conflicts, (int)sizeof(ResConflictStats), 1, DataFile) != 1)
	Die("PutResConflictStats: fwrite");
}

static void PutResLogStats(ResLogStats ResLog) {
    ResLog.NumWraps = htonl(ResLog.NumWraps);
    ResLog.NumAdmGrows = htonl(ResLog.NumAdmGrows);
    ResLog.NumVAllocs = htonl(ResLog.NumVAllocs);
    ResLog.NumVFrees = htonl(ResLog.NumVFrees);
    ResLog.Highest = htonl(ResLog.Highest);

    if (fwrite((char*)&ResLog, (int)sizeof(ResLogStats), 1, DataFile) != 1)
	Die("PutResLogStats: fwrite");
}
    
static void PutRvmStats(RvmStatistics *stats)
{
    PutLong(stats->Malloc);
    PutLong(stats->Free);
    PutLong(stats->MallocBytes);
    PutLong(stats->FreeBytes);
}

static void PutVCBStats(VCBStatistics *stats)
{
    stats->Acquires = htonl(stats->Acquires);
    stats->AcquireObjs = htonl(stats->AcquireObjs);
    stats->AcquireChecked = htonl(stats->AcquireChecked);
    stats->AcquireFailed = htonl(stats->AcquireFailed);
    stats->AcquireNoObjFails = htonl(stats->AcquireNoObjFails);
    stats->Validates = htonl(stats->Validates);
    stats->ValidateObjs = htonl(stats->ValidateObjs);
    stats->FailedValidates = htonl(stats->FailedValidates);
    stats->FailedValidateObjs = htonl(stats->FailedValidateObjs);
    stats->Breaks = htonl(stats->Breaks);
    stats->BreakObjs = htonl(stats->BreakObjs);
    stats->BreakVolOnly = htonl(stats->BreakVolOnly);
    stats->BreakRefs = htonl(stats->BreakRefs);
    stats->Clears = htonl(stats->Clears);
    stats->ClearObjs = htonl(stats->ClearObjs);
    stats->ClearRefs = htonl(stats->ClearRefs);
    stats->NoStamp = htonl(stats->NoStamp);
    stats->NoStampObjs = htonl(stats->NoStampObjs);

    if (fwrite((char *)stats, (int)sizeof(VCBStatistics),1,DataFile) != 1)
	Die("PutVCBStats: fwrite");
}

static void PutAdviceStats(AdviceStatistics *stats)
{
    PutLong(stats->NotEnabled);
    PutLong(stats->NotValid);
    PutLong(stats->Outstanding);
    PutLong(stats->ASRnotAllowed);
    PutLong(stats->ASRinterval);
    PutLong(stats->VolumeNull);
    PutLong(stats->TotalNumberAttempts);
}

static void PutCallStatArray(long size, AdviceCalls array[])
{
    PutLong(size);
    for (int i=0; i<size; i++)
    {
	PutLong(array[i].success);
	PutLong(array[i].failures);
    }
}

static void PutResultStatArray(long size, AdviceResults array[])
{
    PutLong(size);
    for (int i=0; i<size; i++) 
	PutLong(array[i].count);
}

void PutMagicNumber(void) {
    PutLong((long)MAGIC_NUMBER);
}
