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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/utils-src/mond/datalog.c,v 3.2 1995/10/09 19:26:45 satya Exp $";
#endif /*_BLURB_*/



#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/errno.h>
#include <libc.h>
#include <stdio.h>
#include <strings.h>
#include <netinet/in.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include "mondgen.h"
#include "mond.h"
#include <stdarg.h>
#include "util.h"
#include "vargs.h"

extern int LogLevel;
extern FILE *LogFile;
extern FILE *DataFile;

PRIVATE int GetVenusId(VmonVenusId *);
PRIVATE int GetViceId(SmonViceId *);
PRIVATE int GetLong(unsigned long *);
PRIVATE int GetLong(long *);
PRIVATE int GetString(unsigned char **, unsigned long *);
PRIVATE int GetAVSG(VmonAVSG *);
PRIVATE int GetEventArray(VmonSessionEventArray *);
PRIVATE int GetSessionStats(SessionStatistics *);
PRIVATE int GetCacheStats(CacheStatistics *);
PRIVATE int GetRvmStats(RvmStatistics *);
PRIVATE int GetVCBStats(VCBStatistics *);
PRIVATE CallCountEntry *MakeCountArray(unsigned long);
void RemoveCountArray(unsigned long, CallCountEntry*);
PRIVATE MultiCallEntry *MakeMultiArray(unsigned long);
void RemoveMultiArray(unsigned long, MultiCallEntry*);
PRIVATE int GetCallCountArray(unsigned long,CallCountEntry*);
PRIVATE int GetMultiCallArray(unsigned long,MultiCallEntry*);
PRIVATE int GetAdviceStats(AdviceStatistics *);
PRIVATE int GetAdviceCalls(unsigned long, AdviceCalls *);
PRIVATE int GetAdviceResults(unsigned long, AdviceResults *);
PRIVATE int GetMiniCacheStat(unsigned long,VmonMiniCacheStat*);
PRIVATE int GetSmonStatistics(SmonStatistics *);
PRIVATE int GetResOpArray(unsigned long,ResOpEntry *);
PRIVATE int GetFileRes(FileResStats *FileRes);
PRIVATE int GetDirRes(DirResStats *DirRes);
PRIVATE int GetHistogram(long *size, HistoElem **hist);
PRIVATE int GetConflicts(ResConflictStats *Conflicts);
PRIVATE int GetLogStats(ResLogStats *ResLog);
PRIVATE int CheckGuard(const char *s);

long ScanPastMagicNumber(long *rt) {
    // returns the number of non-magic words skipped, 
    // and places the value of the first word past the last magic word
    // in a row in *rt.
    // *rt == -1 <--> end of file, or can't read file
    *rt = 0;
    long result = -1;
    bool afterMN = mfalse;
    do {
	if (fread((char*)rt,(int)sizeof(long),1,DataFile) != 1) {
	    *rt = -1;
	    return result;
	}
	*rt = (long)ntohl((unsigned long)*rt);
	if (*rt == MAGIC_NUMBER) 
	    afterMN = mtrue;
	else result++;
    } while (*rt == MAGIC_NUMBER || afterMN == mfalse);
    return result;
}

int ReadSessionRecord(VmonVenusId *Venus, VmonSessionId *Session, 
		      VolumeId *Volume, UserId *User, VmonAVSG *AVSG,
		      RPC2_Unsigned *StartTime, RPC2_Unsigned *EndTime,
		      RPC2_Unsigned *CETime, VmonSessionEventArray *Events, 
		      SessionStatistics *Stats, CacheStatistics *CacheStats)
{
    // tag has already been read.
    int sum =0;
    sum += GetVenusId(Venus);
    sum += GetLong(Session);
    sum += GetLong(Volume);
    sum += GetLong(User);
    sum += GetAVSG(AVSG);
    sum += GetLong(StartTime);
    sum += GetLong(EndTime);
    sum += GetLong(CETime);
    sum += GetEventArray(Events);
    sum += GetSessionStats(Stats);
    sum += GetCacheStats(CacheStats);
    sum += CheckGuard("in ReadSessionRecord");
    return sum;
}

int ReadCommRecord(VmonVenusId *Venus, RPC2_Unsigned *ServerIPAddress,
			   RPC2_Integer *SerialNumber, RPC2_Unsigned *Time,
			   VmonCommEventType *Type)
{
    int sum = 0;
    sum += GetVenusId(Venus);
    sum += GetLong(ServerIPAddress);
    sum += GetLong(SerialNumber);
    sum += GetLong(Time);
    sum += GetLong((unsigned long *)Type);
    sum += CheckGuard("in ReadCommRecord");
    return sum;
}

int ReadClientCall(VmonVenusId *Venus, long *Time, unsigned long *sc_size,
			   CallCountEntry **SrvCount)
{
    int sum =0;
    sum += GetVenusId(Venus);
    sum += GetLong(Time);
    sum += GetLong(sc_size);
    *SrvCount = MakeCountArray(*sc_size);
    sum += GetCallCountArray(*sc_size,*SrvCount);
    sum += CheckGuard("in ReadClientCall");
    return sum;
}

int ReadClientMCall(VmonVenusId *Venus, long *Time, unsigned long *msc_size,
		   MultiCallEntry **MSrvCount)
{
    int sum =0;
    sum += GetVenusId(Venus);
    sum += GetLong(Time);
    sum += GetLong(msc_size);
    *MSrvCount = MakeMultiArray(*msc_size);
    sum += GetMultiCallArray(*msc_size,*MSrvCount);
    sum += CheckGuard("in ReadClientMCall");
    return sum;
}

int ReadClientRVM(VmonVenusId *Venus, long *Time, RvmStatistics *Stats)
{
    int sum =0;
    sum += GetVenusId(Venus);
    sum += GetLong(Time);
    sum += GetRvmStats(Stats);
    sum += CheckGuard("in ReadClientRVM");
    return sum;
}

int ReadVCB(VmonVenusId *Venus, long *VenusInit, long *Time,
	    VolumeId *Volume, VCBStatistics *Stats) 
{
    int sum =0;
    sum += GetVenusId(Venus);
    sum += GetLong(VenusInit);
    sum += GetLong(Time);
    sum += GetLong(Volume);
    sum += GetVCBStats(Stats);
    sum += CheckGuard("in ReadVCB");
    return sum;
}

int ReadAdviceCall(VmonVenusId *Venus, long *Time, 
		   UserId *User, AdviceStatistics *Stats, 
		   unsigned long *Call_Size, AdviceCalls **Call_Stats,
		   unsigned long *Result_Size, AdviceResults **Result_Stats) {
    int sum = 0;
    sum += GetVenusId(Venus);
    sum += GetLong(Time);
    sum += GetLong(User);
    sum += GetAdviceStats(Stats);
    sum += GetLong(Call_Size);
    *Call_Stats = new AdviceCalls[*Call_Size];
    sum += GetAdviceCalls(*Call_Size, *Call_Stats);
    sum += GetLong(Result_Size);
    *Result_Stats = new AdviceResults[*Result_Size];
    sum += GetAdviceResults(*Result_Size, *Result_Stats);

    sum += CheckGuard("in ReadAdviceCall");
    return sum;
}


int ReadMiniCacheCall(VmonVenusId *Venus, long *Time, 
		      unsigned long *vn_size,
		      VmonMiniCacheStat **vn_stat,
		      unsigned long *vfs_size,
		      VmonMiniCacheStat **vfs_stat) {

    int sum =0;
    sum += GetVenusId(Venus);
    sum += GetLong(Time);
    sum += GetLong(vn_size);
    *vn_stat = new VmonMiniCacheStat[*vn_size];
    sum += GetMiniCacheStat(*vn_size,*vn_stat);
    sum += GetLong(vfs_size);
    *vfs_stat = new VmonMiniCacheStat[*vfs_size];
    sum += GetMiniCacheStat(*vfs_size,*vfs_stat);
    sum += CheckGuard("in ReadClientCall");
    return sum;
}

int ReadOverflow(VmonVenusId *Venus, RPC2_Unsigned *VMStartTime,
		RPC2_Unsigned *VMEndTime, RPC2_Unsigned *VMCount,
		RPC2_Unsigned *RVMStartTime, RPC2_Unsigned *RVMEndTime,
		RPC2_Unsigned *RVMCount)
{
    int sum = 0;
    sum += GetVenusId(Venus);
    sum += GetLong(VMStartTime);
    sum += GetLong(VMEndTime);
    sum += GetLong(VMCount);
    sum += GetLong(RVMStartTime);
    sum += GetLong(RVMEndTime);
    sum += GetLong(RVMCount);
    sum += CheckGuard("in ReadOverflow");
    return sum;
}

int ReadSrvCall(SmonViceId *Vice, unsigned long *Time, unsigned long *CBSize,
		CallCountEntry **CBCount, unsigned long *ResSize,
		CallCountEntry **ResCount, unsigned long *SmonSize,
		CallCountEntry **SmonCount, unsigned long *VolDSize,
		CallCountEntry **VolDCount, unsigned long *MultiSize,
		MultiCallEntry **MultiCount, SmonStatistics *Stats) {

    int sum = 0;
    sum += GetViceId(Vice);
    sum += GetLong(Time);
    sum += GetLong(CBSize);
    *CBCount = MakeCountArray(*CBSize);
    sum += GetCallCountArray(*CBSize,*CBCount);
    sum += GetLong(ResSize);
    *ResCount = MakeCountArray(*ResSize);
    sum += GetCallCountArray(*ResSize,*ResCount);
    sum += GetLong(SmonSize);
    *SmonCount = MakeCountArray(*SmonSize);
    sum += GetCallCountArray(*SmonSize,*SmonCount);
    sum += GetLong(VolDSize);
    *VolDCount = MakeCountArray(*VolDSize);
    sum += GetCallCountArray(*VolDSize,*VolDCount);
    sum += GetLong(MultiSize);
    *MultiCount = MakeMultiArray(*MultiSize);
    sum += GetMultiCallArray(*MultiSize,*MultiCount);
    sum += GetSmonStatistics(Stats);
    sum += CheckGuard("in ReadSrvCall");
    return sum;
}

int ReadResEvent(SmonViceId *Vice, unsigned long *Time, unsigned long *Volid,
		long *HighWaterMark, long *AllocNumber, long *DeallocNumber,
		unsigned long *ResOpSize, ResOpEntry **ResOp)
{
    int sum = 0;
    sum += GetViceId(Vice);
    sum += GetLong(Time);
    sum += GetLong(Volid);
    sum += GetLong(HighWaterMark);
    sum += GetLong(AllocNumber);
    sum += GetLong(DeallocNumber);
    sum += GetLong(ResOpSize);
    *ResOp = new ResOpEntry[*ResOpSize];
    sum += GetResOpArray(*ResOpSize,*ResOp);
    sum += CheckGuard("in ReadResEvent");
    return sum;
}

int ReadRvmResEvent(SmonViceId *Vice, unsigned long *Time,
		    unsigned long *VolID, FileResStats *FileRes,
		    DirResStats *DirRes, long *lshsize, HistoElem **LogSizeHisto,
		    long *lmhsize, HistoElem **LogMaxHisto, ResConflictStats *Conflicts,
		    long *shhsize, HistoElem **SuccHierHist, 
		    long *fhhsize, HistoElem **FailHierHist,
		    ResLogStats *ResLog, long *vlhsize, HistoElem **VarLogHisto,
		    long *lssize, HistoElem **LogSize)
{

    int sum =0;
    sum += GetViceId(Vice);
    sum += GetLong(Time);
    sum += GetLong(VolID);
    sum += GetFileRes(FileRes);
    sum += GetDirRes(DirRes);
    sum += GetHistogram(lshsize,LogSizeHisto);
    sum += GetHistogram(lmhsize,LogMaxHisto);
    sum += GetConflicts(Conflicts);
    sum += GetHistogram(shhsize,SuccHierHist);
    sum += GetHistogram(fhhsize,FailHierHist);
    sum += GetLogStats(ResLog);
    sum += GetHistogram(vlhsize,VarLogHisto);
    sum += GetHistogram(lssize,LogSize);
    sum += CheckGuard("in ReadRvmResEvent");
    return sum;
}

int ReadSrvOverflow(SmonViceId *Vice, unsigned long *Time,
		   unsigned long *StartTime, unsigned long *EndTime,
		   long *Count)
{
    int sum=0;
    sum += GetViceId(Vice);
    sum += GetLong(Time);
    sum += GetLong(StartTime);
    sum += GetLong(EndTime);
    sum += GetLong(Count);
    sum += CheckGuard("in ReadSrvOverflow");
    return sum;
}

PRIVATE int GetIotInfo(IOT_INFO *info)
{
    if (fread((char *)info, (int)sizeof(IOT_INFO), 1, DataFile) != 1)
      return -1;
    
    info->Tid 		= ntohl(info->Tid);
    info->ResOpt 	= ntohl(info->ResOpt);
    info->ElapsedTime 	= ntohl(info->ElapsedTime);
    info->ReadSetSize 	= ntohl(info->ReadSetSize);
    info->WriteSetSize 	= ntohl(info->WriteSetSize);
    info->ReadVolNum 	= ntohl(info->ReadVolNum);
    info->WriteVolNum 	= ntohl(info->WriteVolNum);
    info->Validation 	= ntohl(info->Validation);
    info->InvalidSize 	= ntohl(info->InvalidSize);
    info->BackupObjNum 	= ntohl(info->BackupObjNum);
    info->LifeCycle 	= ntohl(info->LifeCycle);
    info->PredNum 	= ntohl(info->PredNum);
    info->SuccNum 	= ntohl(info->SuccNum);
    
    return 0;
}

int ReadIotInfoCall(VmonVenusId *Venus, IOT_INFO *Info,
		    RPC2_Integer *AppNameLen, RPC2_String *AppName)
{
    int sum = 0;
    sum += GetVenusId(Venus);
    sum += GetIotInfo(Info);
    sum += GetString(AppName, (unsigned long *)AppNameLen);
    sum += CheckGuard("in ReadIotInfoCall");
    return sum;
}

PRIVATE int GetIotStats(IOT_STAT *stats)
{
    if (fread((char *)stats, (int)sizeof(IOT_STAT), 1, DataFile) != 1)
      return -1;
    
    stats->MaxElapsedTime = ntohl(stats->MaxElapsedTime);
    stats->AvgElapsedTime = ntohl(stats->AvgElapsedTime);
    stats->MaxReadSetSize = ntohl(stats->MaxReadSetSize);
    stats->AvgReadSetSize = ntohl(stats->AvgReadSetSize);
    stats->MaxWriteSetSize = ntohl(stats->MaxWriteSetSize);
    stats->AvgWriteSetSize = ntohl(stats->AvgWriteSetSize);
    stats->MaxReadVolNum = ntohl(stats->MaxReadVolNum);
    stats->AvgReadVolNum = ntohl(stats->AvgReadVolNum);
    stats->MaxWriteVolNum = ntohl(stats->MaxWriteVolNum);
    stats->AvgWriteVolNum = ntohl(stats->AvgWriteVolNum);
    stats->Committed = ntohl(stats->Committed);
    stats->Pending = ntohl(stats->Pending);
    stats->Resolved = ntohl(stats->Resolved);
    stats->Repaired = ntohl(stats->Repaired);
    stats->OCCRerun = ntohl(stats->OCCRerun);

    return 0;
}

int ReadIotStatsCall(VmonVenusId *Venus, RPC2_Integer *Time, IOT_STAT *Stats)
{
    int sum = 0;
    sum += GetVenusId(Venus);
    sum += GetLong(Time);
    sum += GetIotStats(Stats);
    sum += CheckGuard("in ReadIotStatsCall");
    return sum;
}

PRIVATE int GetSubtreeStats(LocalSubtreeStats *stats)
{
    if (fread((char *)stats, (int)sizeof(LocalSubtreeStats), 1, DataFile) != 1)
      return -1;

    stats->SubtreeNum = ntohl(stats->SubtreeNum);
    stats->MaxSubtreeSize = ntohl(stats->MaxSubtreeSize);
    stats->AvgSubtreeSize = ntohl(stats->AvgSubtreeSize);
    stats->MaxSubtreeHgt = ntohl(stats->MaxSubtreeHgt);
    stats->AvgSubtreeHgt = ntohl(stats->AvgSubtreeHgt);
    stats->MaxMutationNum = ntohl(stats->MaxMutationNum);
    stats->AvgMutationNum = ntohl(stats->AvgMutationNum);

    return 0;
}

int ReadSubtreeCall(VmonVenusId *Venus, RPC2_Integer *Time, LocalSubtreeStats *Stats)
{
    int sum = 0;
    sum += GetVenusId(Venus);
    sum += GetLong(Time);
    sum += GetSubtreeStats(Stats);
    sum += CheckGuard("in ReadSubtreeCall");
    return sum;
}

PRIVATE int GetRepairStats(RepairSessionStats *stats)
{
    if (fread((char *)stats, (int)sizeof(RepairSessionStats), 1, DataFile) != 1)
      return -1;

    stats->SessionNum = ntohl(stats->SessionNum);
    stats->CommitNum = ntohl(stats->CommitNum);
    stats->AbortNum = ntohl(stats->AbortNum);
    stats->CheckNum = ntohl(stats->CheckNum);
    stats->PreserveNum = ntohl(stats->PreserveNum);
    stats->DiscardNum = ntohl(stats->DiscardNum);
    stats->RemoveNum = ntohl(stats->RemoveNum);
    stats->GlobalViewNum = ntohl(stats->GlobalViewNum);
    stats->LocalViewNum = ntohl(stats->LocalViewNum);
    stats->KeepLocalNum = ntohl(stats->KeepLocalNum);
    stats->ListLocalNum = ntohl(stats->ListLocalNum);
    stats->NewCommand1Num = ntohl(stats->NewCommand1Num);
    stats->NewCommand2Num = ntohl(stats->NewCommand2Num);
    stats->NewCommand3Num = ntohl(stats->NewCommand3Num);
    stats->NewCommand4Num = ntohl(stats->NewCommand4Num);
    stats->NewCommand5Num = ntohl(stats->NewCommand5Num);
    stats->NewCommand6Num = ntohl(stats->NewCommand6Num);
    stats->NewCommand7Num = ntohl(stats->NewCommand7Num);
    stats->NewCommand8Num = ntohl(stats->NewCommand8Num);
    stats->RepMutationNum = ntohl(stats->RepMutationNum);
    stats->MissTargetNum = ntohl(stats->MissTargetNum);
    stats->MissParentNum = ntohl(stats->MissParentNum);
    stats->AclDenyNum = ntohl(stats->AclDenyNum);
    stats->UpdateUpdateNum = ntohl(stats->UpdateUpdateNum);
    stats->NameNameNum = ntohl(stats->NameNameNum);
    stats->RemoveUpdateNum = ntohl(stats->RemoveUpdateNum);

    return 0;
}

int ReadRepairCall(VmonVenusId *Venus, RPC2_Integer *Time, RepairSessionStats *Stats)
{
    int sum = 0;
    sum += GetVenusId(Venus);
    sum += GetLong(Time);
    sum += GetRepairStats(Stats);
    sum += CheckGuard("in ReadRepairCall");
    return sum;
}

PRIVATE int GetRwsStats(ReadWriteSharingStats *stats)
{
    if (fread((char *)stats, (int)sizeof(ReadWriteSharingStats), 1, DataFile) != 1)
      return -1;
    
    stats->Vid = ntohl(stats->Vid);
    stats->RwSharingCount = ntohl(stats->RwSharingCount);
    stats->DiscReadCount = ntohl(stats->DiscReadCount);
    stats->DiscDuration = ntohl(stats->DiscDuration);
    return 0;
}

int ReadRwsStatsCall(VmonVenusId *Venus, RPC2_Integer *Time, ReadWriteSharingStats *Stats)
{
    int sum = 0;
    sum += GetVenusId(Venus);
    sum += GetLong(Time);
    sum += GetRwsStats(Stats);
    sum += CheckGuard("in ReadRwsStatsCall");
    return sum;
}


PRIVATE int GetVenusId(VmonVenusId *Venus) {
    if (fread((char *)Venus, (int)sizeof(VmonVenusId), 1, DataFile) != 1)
	return -1;
    Venus->IPAddress = ntohl(Venus->IPAddress);
    Venus->BirthTime = ntohl(Venus->BirthTime);
    return 0;
}

PRIVATE int GetViceId(SmonViceId *Vice) {
    if (fread((char *)Vice, (int)sizeof(SmonViceId), 1, DataFile) != 1)
	return -1;
    Vice->IPAddress = ntohl(Vice->IPAddress);
    Vice->BirthTime = ntohl(Vice->BirthTime);
    return 0;
}


PRIVATE int GetLong(unsigned long *Value) {
    if (fread((char *)Value, (int)sizeof(unsigned long), 1, DataFile) != 1)
	return -1;
    *Value = ntohl(*Value);
    return 0;
}


PRIVATE int GetLong(long *Value) {
    if (fread((char *)Value, (int)sizeof(long), 1, DataFile) != 1)
	return -1;
    *Value = ntohl(*Value);
    return 0;
}

PRIVATE int GetString(unsigned char **Str, unsigned long *Len) {
    if (fread((char *)Len, (int)sizeof(unsigned long), 1, DataFile) != 1) 
      return -1;
    *Str = new unsigned char[*Len];
    if (fread((char *)*Str, (int)sizeof(unsigned char), *Len, DataFile) != *Len)
      return -1;
    return 0;
}

PRIVATE int GetAVSG(VmonAVSG *AVSG) {
    if (fread((char *)AVSG, (int)sizeof(VmonAVSG), 1, DataFile) != 1)
	return -1;
    for (int i = 0; i < 8; i++)
	(&AVSG->Member0)[i] = ntohl((&AVSG->Member0)[i]);
    return 0;
}


PRIVATE int GetEventArray(VmonSessionEventArray *Events) {
    if (fread((char *)Events, (int)sizeof(VmonSessionEventArray), 1, DataFile) != 1)
	return -1;
    for (int i = 0; i < nVSEs; i++) {
	VmonSessionEvent *se = &((&Events->Event0)[i]);
	se->Opcode = ntohl(se->Opcode);
	se->SuccessCount = ntohl(se->SuccessCount);
	se->SigmaT = ntohl(se->SigmaT);
	se->SigmaTSquared = ntohl(se->SigmaTSquared);
	se->FailureCount = ntohl(se->FailureCount);
    }
    return 0;
}

PRIVATE int GetSessionStats(SessionStatistics *stats)
{
    if (fread((char *)stats, (int)sizeof(SessionStatistics),1,DataFile) != 1)
	return -1;

    stats->BytesStart = ntohl(stats->BytesStart);
    stats->BytesEnd = ntohl(stats->BytesEnd);
    stats->BytesHighWater = ntohl(stats->BytesHighWater);
    stats->EntriesStart = ntohl(stats->EntriesStart);
    stats->EntriesEnd = ntohl(stats->EntriesEnd);
    stats->EntriesHighWater = ntohl(stats->EntriesHighWater);
    stats->RecordsCancelled = ntohl(stats->RecordsCancelled);
    stats->RecordsCommitted = ntohl(stats->RecordsCommitted);
    stats->RecordsAborted = ntohl(stats->RecordsAborted);
    stats->FidsRealloced = ntohl(stats->FidsRealloced);
    stats->BytesBackFetched = ntohl(stats->BytesBackFetched);
    stats->SystemCPU = ntohl(stats->SystemCPU);
    stats->UserCPU = ntohl(stats->UserCPU);
    stats->IdleCPU = ntohl(stats->IdleCPU);
    stats->CacheHighWater = ntohl(stats->CacheHighWater);

    return 0;
}

PRIVATE int GetCacheStats(CacheStatistics *stats) 
{
    if (fread((char *)stats, (int)sizeof(CacheStatistics),1,DataFile) != 1)
	return -1;

    stats->HoardAttrHit.Count = ntohl(stats->HoardAttrHit.Count);
    stats->HoardAttrMiss.Blocks = ntohl(stats->HoardAttrMiss.Blocks);
    stats->HoardAttrNoSpace.Count = ntohl(stats->HoardAttrNoSpace.Count);
    stats->HoardDataHit.Blocks = ntohl(stats->HoardDataHit.Blocks);
    stats->HoardDataMiss.Count = ntohl(stats->HoardDataMiss.Count);
    stats->HoardDataNoSpace.Blocks = ntohl(stats->HoardDataNoSpace.Blocks);
    stats->NonHoardAttrHit.Count = ntohl(stats->NonHoardAttrHit.Count);
    stats->NonHoardAttrMiss.Blocks = ntohl(stats->NonHoardAttrMiss.Blocks);
    stats->NonHoardAttrNoSpace.Count = ntohl(stats->NonHoardAttrNoSpace.Count);
    stats->NonHoardDataHit.Blocks = ntohl(stats->NonHoardDataHit.Blocks);
    stats->NonHoardDataMiss.Count = ntohl(stats->NonHoardDataMiss.Count);
    stats->NonHoardDataNoSpace.Blocks = ntohl(stats->NonHoardDataNoSpace.Blocks);
    stats->UnknownHoardAttrHit.Count = ntohl(stats->UnknownHoardAttrHit.Count);
    stats->UnknownHoardAttrMiss.Blocks = ntohl(stats->UnknownHoardAttrMiss.Blocks);
    stats->UnknownHoardAttrNoSpace.Count = ntohl(stats->UnknownHoardAttrNoSpace.Count);
    stats->UnknownHoardDataHit.Blocks = ntohl(stats->UnknownHoardDataHit.Blocks);
    stats->UnknownHoardDataMiss.Count = ntohl(stats->UnknownHoardDataMiss.Count);
    stats->UnknownHoardDataNoSpace.Blocks = ntohl(stats->UnknownHoardDataNoSpace.Blocks);

    return 0;
}

PRIVATE int GetRvmStats(RvmStatistics *stats) {
    int sum =0;
    sum += GetLong(&(stats->Malloc));
    sum += GetLong(&(stats->Free));
    sum += GetLong(&(stats->MallocBytes));
    sum += GetLong(&(stats->FreeBytes));
    return sum;
}

PRIVATE int GetVCBStats(VCBStatistics *stats) {
    if (fread((char *)stats, sizeof(VCBStatistics),1,DataFile) != 1)
	return -1;

    stats->Acquires = ntohl(stats->Acquires);
    stats->AcquireObjs = ntohl(stats->AcquireObjs);
    stats->AcquireChecked = ntohl(stats->AcquireChecked);
    stats->AcquireFailed = ntohl(stats->AcquireFailed);
    stats->AcquireNoObjFails = ntohl(stats->AcquireNoObjFails);
    stats->Validates = ntohl(stats->Validates);
    stats->ValidateObjs = ntohl(stats->ValidateObjs);
    stats->FailedValidates = ntohl(stats->FailedValidates);
    stats->FailedValidateObjs = ntohl(stats->FailedValidateObjs);
    stats->Breaks = ntohl(stats->Breaks);
    stats->BreakObjs = ntohl(stats->BreakObjs);
    stats->BreakVolOnly = ntohl(stats->BreakVolOnly);
    stats->BreakRefs = ntohl(stats->BreakRefs);
    stats->Clears = ntohl(stats->Clears);
    stats->ClearObjs = ntohl(stats->ClearObjs);
    stats->ClearRefs = ntohl(stats->ClearRefs);
    stats->NoStamp = ntohl(stats->NoStamp);
    stats->NoStampObjs = ntohl(stats->NoStampObjs);

    return 0;
}

PRIVATE CallCountEntry *MakeCountArray(unsigned long Size) {

    CallCountEntry *result;
    result = new CallCountEntry[Size];
    int i;

    for (i=0;i<Size;i++) {
	result[i].name = (RPC2_String) new char[SIGCHAR];
    }
    return result;
}

void RemoveCountArray(unsigned long Size, CallCountEntry *array) {
   for (int i=0;i<Size;i++) {
       delete [] array[i].name;
   }
   delete [] array;
   return;
}

PRIVATE MultiCallEntry *MakeMultiArray(unsigned long Size) {
    MultiCallEntry *result;
    result = new MultiCallEntry[Size];

    int i;
    for (i=0;i<Size;i++) {
	result[i].name = (RPC2_String) new char[SIGCHAR];
    }
    return result;
}

void RemoveMultiArray(unsigned long Size, MultiCallEntry *array) {
    for (int i=0;i<Size;i++) {
	delete [] array[i].name;
    }
    delete [] array;
    return;
}

PRIVATE int GetCallCountArray(unsigned long Size, CallCountEntry *array) {
    int i;
    for (i=0;i<Size;i++) {
	int result=0;
	if (fread((char*)array[i].name,(int)sizeof(char[SIGCHAR]),1,
		  DataFile) != 1)
	    return -1;
	result += GetLong(&(array[i].countent));
	result += GetLong(&(array[i].countexit));
	result += GetLong(&(array[i].tsec));
	result += GetLong(&(array[i].tusec));
	result += GetLong(&(array[i].counttime));
	if (result != 0)
	    return result;
    }
    return 0;
}

PRIVATE int GetMultiCallArray(unsigned long Size, MultiCallEntry *array) {
    int i;
    for (i=0;i<Size;i++) {
	int result=0;
	if (fread((char*)array[i].name,(int)sizeof(char[SIGCHAR]),1,
		  DataFile) != 1)
	    return -1;
	result += GetLong(&(array[i].countent));
	result += GetLong(&(array[i].countexit));
	result += GetLong(&(array[i].tsec));
	result += GetLong(&(array[i].tusec));
	result += GetLong(&(array[i].counttime));
	result += GetLong(&(array[i].counthost));
	if (result != 0)
	    return result;
    }
    return 0;
}

PRIVATE int GetAdviceStats(AdviceStatistics *stats)
{
    if (fread((char *)stats, (int)sizeof(AdviceStatistics),1,DataFile) != 1)
	return -1;

    stats->NotEnabled = ntohl(stats->NotEnabled);
    stats->NotValid = ntohl(stats->NotValid);
    stats->Outstanding = ntohl(stats->Outstanding);
    stats->ASRnotAllowed = ntohl(stats->ASRnotAllowed);
    stats->ASRinterval = ntohl(stats->ASRinterval);
    stats->VolumeNull = ntohl(stats->VolumeNull);
    stats->TotalNumberAttempts = ntohl(stats->TotalNumberAttempts);

    return 0;
}

PRIVATE int GetAdviceCalls(unsigned long size, AdviceCalls *array)
{
    for (int i=0; i<size; i++) {
	if (fread((char*)(&(array[i])),(int)sizeof(AdviceCalls),1,DataFile) != 1)
	    return -1;
	array[i].success = ntohl(array[i].success);
	array[i].failures = ntohl(array[i].failures);
    }
    return 0;
}

PRIVATE int GetAdviceResults(unsigned long size, AdviceResults *array)
{
    for (int i=0; i<size; i++) {
	if (fread((char*)(&(array[i])),(int)sizeof(AdviceResults),1,DataFile) != 1)
	    return -1;
	array[i].count = ntohl(array[i].count);
    }
    return 0;
}

PRIVATE int GetMiniCacheStat(unsigned long size, VmonMiniCacheStat *stats)
{
    for(int i=0;i<size;i++) {
	if (fread((char*)(&(stats[i])),(int)sizeof(VmonMiniCacheStat),1,DataFile) != 1)
	    return -1;
	stats[i].Opcode = ntohl(stats[i].Opcode);
	stats[i].Entries = ntohl(stats[i].Entries);
	stats[i].SatIntrn = ntohl(stats[i].SatIntrn);
	stats[i].UnsatIntrn = ntohl(stats[i].UnsatIntrn);
	stats[i].GenIntrn = ntohl(stats[i].GenIntrn);
    }
    return 0;
}

PRIVATE int GetSmonStatistics(SmonStatistics *Stats) {
    if (fread((char *)Stats, (int)sizeof(SmonStatistics),1,DataFile)
	!= 1)
	return -1;
    Stats->SystemCPU = ntohl(Stats->SystemCPU);
    Stats->UserCPU = ntohl(Stats->UserCPU);
    Stats->IdleCPU = ntohl(Stats->IdleCPU);
    Stats->BootTime = ntohl(Stats->BootTime);
    Stats->TotalIO = ntohl(Stats->TotalIO);
    return 0;
}

PRIVATE int GetResOpArray(unsigned long size, ResOpEntry array[]) {
    int sum =0;
    for (int i=0; i<size; i++) {
	sum += GetLong(&array[i].alloccount);
	sum += GetLong(&array[i].dealloccount);
    }
    return sum;
}

PRIVATE int GetFileRes(FileResStats *FileRes) {

    if (fread((char*)FileRes, (int)sizeof(FileResStats), 1, DataFile) != 1)
	return -1;

    FileRes->Resolves = ntohl(FileRes->Resolves);
    FileRes->NumSucc = ntohl(FileRes->NumSucc);
    FileRes->NumConf = ntohl(FileRes->NumConf);
    FileRes->RuntForce = ntohl(FileRes->RuntForce);
    FileRes->WeakEq = ntohl(FileRes->WeakEq);
    FileRes->NumReg = ntohl(FileRes->NumReg);
    FileRes->UsrResolver = ntohl(FileRes->UsrResolver);
    FileRes->SuccUsrResolver = ntohl(FileRes->SuccUsrResolver);
    FileRes->PartialVSG = ntohl(FileRes->PartialVSG);
    return 0;
}

PRIVATE int GetDirRes(DirResStats *DirRes) {

    if (fread((char*)DirRes, (int)sizeof(DirResStats), 1, DataFile) != 1)
	return -1;

    DirRes->Resolves = ntohl(DirRes->Resolves);
    DirRes->NumSucc = ntohl(DirRes->NumSucc);
    DirRes->NumConf = ntohl(DirRes->NumConf);
    DirRes->NumNoWork = ntohl(DirRes->NumNoWork);
    DirRes->Problems = ntohl(DirRes->Problems);
    DirRes->PartialVSG = ntohl(DirRes->PartialVSG);
    return 0;
}

PRIVATE int GetHistogram(long *size, HistoElem **hist) {

    register int sum =0;
    register HistoElem *newhist;
    sum += GetLong(size);
    newhist = new HistoElem[*size];
    for (int i=0; i<*size; i++)
	/* ugh */
	sum += GetLong(&(newhist[i].bucket));
    *hist = newhist;
    return sum;
}

PRIVATE int GetConflicts(ResConflictStats *Conflicts) {

    if (fread((char*)Conflicts, (int)sizeof(ResConflictStats), 1, DataFile) != 1)
	return -1;

    Conflicts->NameName = ntohl(Conflicts->NameName);
    Conflicts->RemoveUpdate = ntohl(Conflicts->RemoveUpdate);
    Conflicts->UpdateUpdate = ntohl(Conflicts->UpdateUpdate);
    Conflicts->Rename = ntohl(Conflicts->Rename);
    Conflicts->LogWrap = ntohl(Conflicts->LogWrap);
    Conflicts->Other = ntohl(Conflicts->Other);
    return 0;
}

PRIVATE int GetLogStats(ResLogStats *ResLog) {

    if (fread((char*)ResLog, (int)sizeof(ResLogStats), 1, DataFile) != 1)
	return -1;

    ResLog->NumWraps = ntohl(ResLog->NumWraps);
    ResLog->NumAdmGrows = ntohl(ResLog->NumAdmGrows);
    ResLog->NumVAllocs = ntohl(ResLog->NumVAllocs);
    ResLog->NumVFrees = ntohl(ResLog->NumVFrees);
    ResLog->Highest = ntohl(ResLog->Highest);
    return 0;

}
    
PRIVATE int CheckGuard(const char *s) {
    const char *msg;
    if (s)
	msg = s;
    else msg = "";
    long checknum;
    GetLong(&checknum);
    if (checknum != END_GUARD) {
	LogMsg(0,LogLevel,LogFile,"End Guard missing: %s",msg);
	return -1;
    }
    return 0;
}
