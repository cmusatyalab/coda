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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/utils-src/mond/parselog.c,v 3.6 2003/05/23 18:27:57 jaharkes Exp $";
#endif /*_BLURB_*/





/*
 *    Vmon Daemon -- Log Parser.
 */


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <sys/file.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include "lock.h"

#ifdef __cplusplus
}
#endif __cplusplus

#include "mondgen.h"
#include "mond.h"
#include "util.h"
#include "datalog.h"

static bool PrintVmonSession();
static bool PrintVmonCommEvent();
static bool PrintClientCalls();
static bool PrintClientMCalls();
static bool PrintClientRVM();
static bool PrintVCB();
static bool PrintAdviceCalls();
static bool PrintMiniCache();
static bool PrintVmonOverflow();
static bool PrintSrvCall();
static bool PrintResEvent();
static bool PrintRvmResEvent();
static bool PrintSrvOvrflw();
static bool PrintIotInfoCall();
static bool PrintIotStatsCall();
static bool PrintSubtreeStatsCall();
static bool PrintRepairStatsCall();
static bool PrintRwsStatsCall();

static void PrintVenusId(VmonVenusId *);

static void PrintHex(unsigned long);
static void PrintDecimal(unsigned long);
static void PrintDecimal(long);

static void PrintAVSG(VmonAVSG *);
static void PrintEventArray(VmonSessionEventArray *);

static void PrintVCET(VmonCommEventType);

static void PrintSessionStats(SessionStatistics *);
static void PrintCacheStats(CacheStatistics *);
static void PrintCacheEntries(CacheEventEntry);

static void PrintViceId(SmonViceId *);

static void PrintCallArray(unsigned long, CallCountEntry*);
static void PrintMultiArray(unsigned long, MultiCallEntry*);
static void PrintSmonStats(SmonStatistics *);

static void PrintResOpArray(unsigned long,ResOpEntry *);

static void PrintHistogram(long, HistoElem *);

static void PrintRvmStats(RvmStatistics*);

static void PrintVCBStats(VCBStatistics*);

static void PrintAdviceStats(AdviceStatistics*);
static void PrintAdviceCallsArray(unsigned long, AdviceCalls*);
static void PrintAdviceResultsArray(unsigned long, AdviceResults*);

static void PrintIotInfo(IOT_INFO *);
static void PrintIotStats(IOT_STAT *);
static void PrintSubtreeStats(LocalSubtreeStats *);
static void PrintRepairStats(RepairSessionStats *);
static void PrintString(RPC2_String, RPC2_Integer);
static void PrintRwsStats(ReadWriteSharingStats *);

static void LogErrorPoint(int[]);

int LogLevel = 0;
FILE *DataFile = stdin;
FILE *LogFile = stdin;

int main(int argc, char *argv[]) {

    int recordCounts[SRVOVRFLW+1];
    for (int i=0;i<=SRVOVRFLW;i++)
	recordCounts[i] = 0;

    bool done = mfalse;
    bool error = mfalse;
    long rt;
    long count;
    long counter = 0;
    while (done == mfalse) {
	counter++;
	count = ScanPastMagicNumber(&rt);
	if (count > 0) {
	    LogMsg(0,LogLevel,LogFile,
		   "Out of sync with data file: %d words skipped to next sync point",
		   count);
	    LogErrorPoint(recordCounts);
	} else if (count < 0) {
	    LogMsg(0,LogLevel,LogFile,"End of data file");
	    LogErrorPoint(recordCounts);
	}
	printf("Record #%d:  Type=%d\n", counter, rt); 
	switch(rt) {
	case -1:
	    done = mtrue;
	    break;
	case SESSION_TAG:
	    error = PrintVmonSession();
	    recordCounts[SESSION]++;
	    break;
	case COMM_TAG:
	    error = PrintVmonCommEvent();
	    recordCounts[COMM]++;
	    break;
	case CLNTCALL_TAG:
	    error = PrintClientCalls();
	    recordCounts[CLNTCALL]++;
	    break;
	case CLNTMCALL_TAG:
	    error = PrintClientMCalls();
	    recordCounts[CLNTMCALL]++;
	    break;
	case CLNTRVM_TAG:
	    error = PrintClientRVM();
	    recordCounts[CLNTRVM]++;
	    break;
	case VCB_TAG:
 	    error = PrintVCB();
 	    recordCounts[VCB]++;
 	    break;
	case ADVICE_TAG:
	    error = PrintAdviceCalls();
	    recordCounts[ADVICE]++;
	    break;
	case MINICACHE_TAG:
	    error = PrintMiniCache();
	    recordCounts[MINICACHE]++;
	    break;
	case OVERFLOW_TAG:
	    error = PrintVmonOverflow();
	    recordCounts[OVERFLOW]++;
	    break;
	case SRVCALL_TAG:
	    error = PrintSrvCall();
	    recordCounts[SRVCALL]++;
	    break;
	case SRVRES_TAG:
	    error = PrintResEvent();
	    recordCounts[SRVRES]++;
	    break;
	case SRVRVMRES_TAG:
	    error = PrintRvmResEvent();
	    recordCounts[SRVRVMRES]++;
	    break;
	case SRVOVRFLW_TAG:
	    error = PrintSrvOvrflw();
	    recordCounts[SRVOVRFLW]++;
	    break;
	case IOTINFO_TAG:
	    error = PrintIotInfoCall();
	    recordCounts[IOTINFO]++;
	    break;
	case IOTSTAT_TAG:
	    error = PrintIotStatsCall();
	    recordCounts[IOTSTAT]++;
	    break;
	case SUBTREE_TAG:
	    error = PrintSubtreeStatsCall();
	    recordCounts[SUBTREE]++;
	    break;
	case REPAIR_TAG:
	    error = PrintRepairStatsCall();
	    recordCounts[REPAIR]++;
	    break;
	case RWSSTAT_TAG:
	    error = PrintRwsStatsCall();
	    recordCounts[RWSSTAT]++;
	    break;
	case MAGIC_NUMBER:
	    printf("Magic Number found\n");
	    break;
	default:
	    LogMsg(0,LogLevel,LogFile,"parselog: bogus rt (%d)",rt);
	    error = mtrue;
	    break;
	}
	if (error == mtrue) {
	    LogErrorPoint(recordCounts);
	    error = mfalse;
	}
    }
}


static bool PrintVmonSession() {
    int sum;
    VmonVenusId Venus;
    VmonSessionId Session;
    VolumeId Volume;
    UserId User;
    VmonAVSG AVSG;
    unsigned long StartTime;
    unsigned long EndTime;
    unsigned long CETime;
    VmonSessionEventArray Events;
    SessionStatistics Stats;
    CacheStatistics CacheStats;

    sum = ReadSessionRecord(&Venus, &Session, &Volume, &User, &AVSG,
			    &StartTime, &EndTime, &CETime, &Events, &Stats, &CacheStats);

    if (sum != 0) LogMsg(0,LogLevel,LogFile,"**** BAD SESSION RECORD ****");
    printf("%-12s:  ", "SESSION");
    PrintVenusId(&Venus);
    PrintHex(Session);
    PrintHex(Volume);
    PrintDecimal(User);
    PrintDecimal(StartTime);
    PrintDecimal(EndTime);
    PrintDecimal(CETime);
    printf("\n");

    PrintAVSG(&AVSG);
    printf("\n");

    PrintEventArray(&Events);
    printf("\n");

    PrintSessionStats(&Stats);
    printf("\n-----------\n");
    PrintCacheStats(&CacheStats);
    printf("=================================================\n");

    if (sum) return mtrue;
    else return mfalse;
}


static bool PrintVmonCommEvent() {
    VmonVenusId Venus;
    unsigned long ServerIPAddress;
    long SerialNumber;
    unsigned long Time;
    VmonCommEventType Type;

    int sum = ReadCommRecord(&Venus,&ServerIPAddress,
			     &SerialNumber, &Time, &Type);

    if (sum != 0) LogMsg(0,LogLevel,LogFile,"**** BAD COMM RECORD ****");
    printf("%-12s:  ", "COMMEVENT");
    PrintVenusId(&Venus);
    PrintHex(ServerIPAddress);
    PrintDecimal(Time);
    PrintDecimal(SerialNumber);
    PrintVCET(Type);
    printf("\n");
    printf("=================================================\n");

    if (sum) return mtrue;
    else return mfalse;
}

static bool PrintClientCalls() {
    VmonVenusId Venus;
    long Time;
    unsigned long sc_size; 
    CallCountEntry *SrvCount;

    int sum = ReadClientCall(&Venus, &Time, &sc_size, &SrvCount);
    
    if (sum != 0) LogMsg(0,LogLevel,LogFile,"**** BAD CLIENTCALL RECORD ****");
    printf("%-12s: ","CLNTCALL");
    PrintVenusId(&Venus);
    PrintDecimal(Time);
    printf("\n");
    PrintCallArray(sc_size,SrvCount);
    printf("\n");
    printf("=================================================\n");

    RemoveCountArray(sc_size,SrvCount);

    if (sum) return mtrue;
    else return mfalse;
}

static bool PrintClientMCalls() {
    VmonVenusId Venus;
    long Time;
    unsigned long msc_size;
    MultiCallEntry *MSrvCount;

    int sum = ReadClientMCall(&Venus, &Time, &msc_size, &MSrvCount);
    
    if (sum != 0) 
	LogMsg(0,LogLevel,LogFile,"**** BAD CLIENTMCALL RECORD ****");

    printf("%-12s: ","CLNTMCALL");
    PrintVenusId(&Venus);
    PrintDecimal(Time);
    printf("\n");
    PrintMultiArray(msc_size,MSrvCount);
    printf("\n");
    printf("=================================================\n");

    RemoveMultiArray(msc_size,MSrvCount);

    if (sum) return mtrue;
    else return mfalse;
}

static bool PrintClientRVM() {
    VmonVenusId Venus;
    long Time;
    unsigned long sc_size; 
    CallCountEntry *SrvCount;
    unsigned long msc_size;
    MultiCallEntry *MSrvCount;
    RvmStatistics Stats;

    int sum = ReadClientRVM(&Venus, &Time, &Stats);
    
    if (sum != 0) LogMsg(0,LogLevel,LogFile,"**** BAD CLIENTRVM RECORD ****");
    printf("%-12s: ","CLNTRVM");
    PrintVenusId(&Venus);
    PrintDecimal(Time);
    printf("\n");
    PrintRvmStats(&Stats);
    printf("\n");
    printf("=================================================\n");

    if (sum) return mtrue;
    else return mfalse;
}

static bool PrintVCB() {
    VmonVenusId Venus;
    long VenusInit;
    long Time;
    VolumeId Volume;
    VCBStatistics Stats;
    
    int sum = ReadVCB(&Venus, &VenusInit, &Time, &Volume, &Stats);

    if (sum != 0) LogMsg(0,LogLevel,LogFile,"**** BAD VCB RECORD ****");
    
    printf("%-12s: ","VCB");
    PrintVenusId(&Venus);
    PrintDecimal(VenusInit);
    PrintDecimal(Time);
    printf("\n");
    PrintHex(Volume);
    printf("\n");
    PrintVCBStats(&Stats);
    printf("\n");
    printf("=================================================\n");

    if (sum) return mtrue;
    else return mfalse;
}

static bool PrintAdviceCalls() {
    VmonVenusId Venus;
    long Time;
    UserId User;
    AdviceStatistics Stats;
    unsigned long Call_Size;
    AdviceCalls *Call_Stats;
    unsigned long Result_Size;
    AdviceResults *Result_Stats;


    int sum = ReadAdviceCall(&Venus, &Time, &User, &Stats, 
		&Call_Size, &Call_Stats, &Result_Size, &Result_Stats);
    
    if (sum != 0) LogMsg(0,LogLevel,LogFile,"**** BAD ADVICE RECORD ****");

    printf("%-12s: ","ADVICE");
    PrintVenusId(&Venus);
    PrintDecimal(Time);
    PrintDecimal(User);
    printf("\n");

    PrintAdviceStats(&Stats);
    printf("CallSize = %d\t Result_Size = %d\n", Call_Size, Result_Size);
    PrintAdviceCallsArray(Call_Size, Call_Stats);
    PrintAdviceResultsArray(Result_Size, Result_Stats);

    printf("\n");
    printf("=================================================\n");

    if (sum) return mtrue;
    else return mfalse;
}

static bool PrintMiniCache() 
{
    VmonVenusId venus;
    long time;
    unsigned long vn_size;
    VmonMiniCacheStat *vn_stat;
    unsigned long vfs_size;
    VmonMiniCacheStat *vfs_stat;
    
    int sum = ReadMiniCacheCall(&venus,&time,&vn_size,&vn_stat,
				&vfs_size,&vfs_stat);

    if (sum != 0) LogMsg(0,LogLevel,LogFile,"**** BAD MINICACHE RECORD ****");


    printf("%-12s: ","MINICACHE");
    PrintVenusId(&venus);
    PrintDecimal(time);
    printf("\n-----------\n");
    printf("VFS Operations:\n");
    for (int i=0; i<vfs_size; i++) {
	PrintDecimal(vfs_stat[i].Opcode);
	PrintDecimal(vfs_stat[i].Entries);
	PrintDecimal(vfs_stat[i].SatIntrn);
	PrintDecimal(vfs_stat[i].UnsatIntrn);
	PrintDecimal(vfs_stat[i].GenIntrn);
	printf("\n\n");
    }	
    printf("Vnode Operations:\n");
    for (i=0; i<vn_size; i++) {
	PrintDecimal(vn_stat[i].Opcode);
	PrintDecimal(vn_stat[i].Entries);
	PrintDecimal(vn_stat[i].SatIntrn);
	PrintDecimal(vn_stat[i].UnsatIntrn);
	PrintDecimal(vn_stat[i].GenIntrn);
	printf("\n\n");
    }	
    printf("=================================================\n");

    delete [] vn_stat;
    delete [] vfs_stat;

    if (sum) 
	return mtrue;
    else return mfalse;
}

static bool PrintVmonOverflow() {
    VmonVenusId Venus;
    unsigned long VMStartTime;
    unsigned long VMEndTime;
    unsigned long VMCount;
    unsigned long RVMStartTime;
    unsigned long RVMEndTime;
    unsigned long RVMCount;

    int sum = ReadOverflow(&Venus, &VMStartTime, &VMEndTime, &VMCount,
		       &RVMStartTime, &RVMEndTime, &RVMCount);

    if (sum != 0) LogMsg(0,LogLevel,LogFile,"**** BAD OVERFLOW RECORD ****");

    printf("%-12s:  ", "OVERFLOW");
    PrintVenusId(&Venus);
    PrintDecimal(VMStartTime);
    PrintDecimal(VMEndTime);
    PrintDecimal(VMCount);
    PrintDecimal(RVMStartTime);
    PrintDecimal(RVMEndTime);
    PrintDecimal(RVMCount);
    printf("\n");
    printf("=================================================\n");

    if (sum) return mtrue;
    else return mfalse;
}

static bool PrintSrvCall() {
    SmonViceId Vice;
    unsigned long Time;
    unsigned long CBSize;
    CallCountEntry *CBCount;
    unsigned long ResSize;
    CallCountEntry *ResCount;
    unsigned long SmonSize;
    CallCountEntry *SmonCount;
    unsigned long VolDSize;
    CallCountEntry *VolDCount;
    unsigned long MultiSize;
    MultiCallEntry *MultiCount;
    SmonStatistics Stats;

    int sum = ReadSrvCall(&Vice, &Time, &CBSize, &CBCount, &ResSize,
		      &ResCount, &SmonSize, &SmonCount, &VolDSize,
		      &VolDCount, &MultiSize, &MultiCount, &Stats);

    
    if (sum != 0) LogMsg(0,LogLevel,LogFile,"**** BAD SRVCALL RECORD ****");

    printf("%-12s: ","SRVCALL");
    PrintViceId(&Vice);
    PrintDecimal(Time);
    printf("\n");
    PrintCallArray(CBSize,CBCount);
    PrintCallArray(ResSize,ResCount);
    PrintCallArray(SmonSize,SmonCount);
    PrintCallArray(VolDSize,VolDCount);
    printf("mulitcalls\n");
    PrintMultiArray(MultiSize,MultiCount);
    PrintSmonStats(&Stats);
    printf("=================================================\n");

    RemoveCountArray(CBSize,CBCount);
    RemoveCountArray(ResSize,ResCount);
    RemoveCountArray(SmonSize,SmonCount);
    RemoveCountArray(VolDSize,VolDCount);
    RemoveMultiArray(MultiSize,MultiCount);

    if (sum) return mtrue;
    else return mfalse;
}

static bool PrintResEvent(void)
{
    SmonViceId Vice; 
    unsigned long Time;
    unsigned long Volid;
    long HighWaterMark;
    long AllocNumber;
    long DeallocNumber;
    unsigned long ResOpSize;
    ResOpEntry *ResOp;

    int sum = ReadResEvent(&Vice,&Time,&Volid,&HighWaterMark,&AllocNumber,
		       &DeallocNumber,&ResOpSize,&ResOp);

    if (sum != 0) LogMsg(0,LogLevel,LogFile,"**** BAD RESOLUTION RECORD ****");

    printf("%-12s: ","SRVRES");
    PrintViceId(&Vice);
    PrintDecimal(Time);
    PrintHex(Volid);
    PrintDecimal(HighWaterMark);
    PrintDecimal(AllocNumber);
    PrintDecimal(DeallocNumber);
    printf("\n");
    PrintResOpArray(ResOpSize,ResOp);
    printf("\n");
    printf("=================================================\n");

    delete [] ResOp;

    if (sum) return mtrue;
    else return mfalse;
}

static bool PrintRvmResEvent()
{
    int sum=0;
    SmonViceId Vice;
    unsigned long Time;
    unsigned long VolID;
    FileResStats FileRes;
    DirResStats DirRes;
    long lshsize;
    HistoElem *LogSizeHisto;
    long lmhsize;
    HistoElem *LogMaxHisto;
    ResConflictStats Conflicts;
    long shhsize;
    HistoElem *SuccHierHist;
    long fhhsize;
    HistoElem *FailHierHist;
    ResLogStats ResLog;
    long vlhsize;
    HistoElem *VarLogHisto;
    long lssize;
    HistoElem *LogSize;
    sum = ReadRvmResEvent(&Vice, &Time, &VolID, &FileRes, &DirRes,
			  &lshsize, &LogSizeHisto, 
			  &lmhsize, &LogMaxHisto, &Conflicts,
			  &shhsize, &SuccHierHist, 
			  &fhhsize, &FailHierHist, &ResLog,
			  &vlhsize, &VarLogHisto, &lssize, &LogSize);

    if (sum != 0) LogMsg(0,LogLevel,LogFile,"**** BAD RESOLUTION RECORD ****");

    printf("%-12s: ","SRVRVMRES");
    PrintViceId(&Vice);
    PrintDecimal(Time);
    PrintHex(VolID);

    printf("\nGeneral:\n");
    PrintDecimal(ResLog.NumWraps);
    PrintDecimal(ResLog.NumAdmGrows);
    PrintDecimal(ResLog.NumVAllocs);
    PrintDecimal(ResLog.NumVFrees);
    PrintDecimal(ResLog.Highest);

    printf("\nFile:\n");
    PrintDecimal(FileRes.Resolves);
    PrintDecimal(FileRes.NumSucc);
    PrintDecimal(FileRes.NumConf);
    PrintDecimal(FileRes.RuntForce);
    PrintDecimal(FileRes.WeakEq);
    printf("\n");
    PrintDecimal(FileRes.NumReg);
    PrintDecimal(FileRes.UsrResolver);
    PrintDecimal(FileRes.SuccUsrResolver);
    PrintDecimal(FileRes.PartialVSG);

    printf("\nDir:\n");
    PrintDecimal(DirRes.Resolves);
    PrintDecimal(DirRes.NumSucc);
    PrintDecimal(DirRes.NumConf);
    PrintDecimal(DirRes.NumNoWork);
    PrintDecimal(DirRes.Problems);
    PrintDecimal(DirRes.PartialVSG);
    printf("\n");

    PrintHistogram(lshsize,LogSizeHisto);
    PrintHistogram(lmhsize,LogMaxHisto);

    printf("Conflicts:\n");
    PrintDecimal(Conflicts.NameName);
    PrintDecimal(Conflicts.RemoveUpdate);
    PrintDecimal(Conflicts.UpdateUpdate);
    PrintDecimal(Conflicts.Rename);
    PrintDecimal(Conflicts.LogWrap);
    PrintDecimal(Conflicts.Other);
    printf("\n");

    PrintHistogram(shhsize,SuccHierHist);
    PrintHistogram(fhhsize,FailHierHist);

    PrintHistogram(vlhsize,VarLogHisto);
    PrintHistogram(lssize,LogSize);

    if (sum) return mtrue;
    else return mfalse;

}

static bool PrintSrvOvrflw()
{
    SmonViceId Vice;
    unsigned long Time;
    unsigned long StartTime;
    unsigned long EndTime;
    long Count;

    int sum = ReadSrvOverflow(&Vice,&Time,&StartTime,&EndTime,&Count);

    if (sum != 0) LogMsg(0,LogLevel,LogFile,"**** BAD SRVOVERFLOW RECORD ****");

    printf("%-12s: ","SRVOVRFLW");
    PrintViceId(&Vice);
    PrintDecimal(Time);
    PrintDecimal(StartTime);
    PrintDecimal(EndTime);
    PrintDecimal(Count);
    printf("\n");

    if (sum) return mtrue;
    else return mfalse;
}

static void PrintVenusId(VmonVenusId *Venus) {
    printf("%08x:%08d  ", Venus->IPAddress, Venus->BirthTime);
}


static void PrintViceId(SmonViceId *Vice) {
    printf("%08x:%08d  ", Vice->IPAddress, Vice->BirthTime);
}


static inline void PrintHex(unsigned long Value) {
    printf("%08x  ", Value);
}


static inline void PrintDecimal(unsigned long Value) {
    printf("%8d  ", Value);
}


static inline void PrintDecimal(long Value) {
    printf("%8d  ", Value);
}


static void PrintAVSG(VmonAVSG *AVSG) {
    printf("\t[ ");
    for (int i = 0; i < /*VSG_MEMBERS*/8; i++) {
	RPC2_Unsigned *m = &(AVSG->Member0);

	printf("%08x ", m[i]);
    }
    printf("]");
}


static void PrintEventArray(VmonSessionEventArray *Events) {
    for (int i = 0; i < nVSEs; i++) {
	VmonSessionEvent *e = &((&(Events->Event0))[i]);

	if ((e->SuccessCount != 0) || (e->FailureCount != 0)) {
	    printf("\t%3d  %5d  %10d  %10d  %5d\n",
		   e->Opcode, e->SuccessCount, e->SigmaT, 
		   e->SigmaTSquared, e->FailureCount);
	}
    }
}

static void PrintSessionStats(SessionStatistics *stats) {
    printf("[%08d-%08d:%08d]  [%08d-%08d:%08d]\n",
	   stats->EntriesStart, stats->EntriesEnd,
	   stats->EntriesHighWater, stats->BytesStart,
	   stats->BytesEnd, stats->BytesHighWater);

    PrintDecimal(stats->RecordsCancelled);
    PrintDecimal(stats->RecordsAborted);
    PrintDecimal(stats->RecordsCommitted);
    printf("\n");

    printf("%08d\t%08d\t%08d\n",stats->FidsRealloced,stats->BytesBackFetched,
	   stats->CacheHighWater);
    printf("%08d\t%08d\t%08d\n",stats->SystemCPU,stats->UserCPU,
	   stats->IdleCPU);
}

static void PrintCacheStats(CacheStatistics *stats) {
    PrintCacheEntries(stats->HoardAttrHit);
    PrintCacheEntries(stats->HoardAttrMiss);
    PrintCacheEntries(stats->HoardAttrNoSpace);
    printf("\n");

    PrintCacheEntries(stats->HoardDataHit);
    PrintCacheEntries(stats->HoardDataMiss);
    PrintCacheEntries(stats->HoardDataNoSpace);
    printf("\n");

    PrintCacheEntries(stats->NonHoardAttrHit);
    PrintCacheEntries(stats->NonHoardAttrMiss);
    PrintCacheEntries(stats->NonHoardAttrNoSpace);
    printf("\n");

    PrintCacheEntries(stats->NonHoardDataHit);
    PrintCacheEntries(stats->NonHoardDataMiss);
    PrintCacheEntries(stats->NonHoardDataNoSpace);
    printf("\n");

    PrintCacheEntries(stats->UnknownHoardAttrHit);
    PrintCacheEntries(stats->UnknownHoardAttrMiss);
    PrintCacheEntries(stats->UnknownHoardAttrNoSpace);
    printf("\n");

    PrintCacheEntries(stats->UnknownHoardDataHit);
    PrintCacheEntries(stats->UnknownHoardDataMiss);
    PrintCacheEntries(stats->UnknownHoardDataNoSpace);
    printf("\n");
}

static void PrintCacheEntries(CacheEventEntry entry) {
    printf("%08d [%08d]\t",entry.Count,entry.Blocks);
}

static void PrintVCET(VmonCommEventType Type) {
    printf("%4s  ", Type == ServerDown ? "DOWN" : Type == ServerUp ? "UP" : "???");
}

static void PrintCallArray(unsigned long size, CallCountEntry *array) {
    if (size <= 1)
	return;
    for(int i=1; i<size; i++) {
	if (array[i].countent != 0) { 
	    printf("%-35s ",array[i].name);
	    PrintDecimal(array[i].countent);
	    PrintDecimal(array[i].countexit);
	    PrintDecimal(array[i].tsec);
	    PrintDecimal(array[i].tusec);
	    PrintDecimal(array[i].counttime);
	    printf("\n");
	}
    }
}

static void PrintMultiArray(unsigned long size, MultiCallEntry *array) {
    if (size <= 1)
	return;
    for(int i=1; i<size; i++) {
	if (array[i].countent != 0) { 
	    printf("%-35s ",array[i].name);
	    PrintDecimal(array[i].countent);
	    PrintDecimal(array[i].countexit);
	    PrintDecimal(array[i].tsec);
	    PrintDecimal(array[i].tusec);
	    PrintDecimal(array[i].counttime);
	    PrintDecimal(array[i].counthost);
	    printf("\n");
	}
    }
}

static void PrintSmonStats(SmonStatistics *stats) {
    PrintDecimal((unsigned long)stats->SystemCPU);
    PrintDecimal((unsigned long)stats->UserCPU);
    PrintDecimal((unsigned long)stats->IdleCPU);
    PrintDecimal((unsigned long)stats->BootTime);
    PrintDecimal((unsigned long)stats->TotalIO);
    printf("\n");
}

static void PrintResOpArray(unsigned long size, ResOpEntry array[]) {
    for (int i=0; i<size; i++) {
	PrintDecimal((unsigned long) array[i].alloccount);
	PrintDecimal((unsigned long) array[i].dealloccount);
	printf("\n");
    }
}

static void PrintHistogram(long size, HistoElem *array)
{
    printf("[");
    for (int i=0; i<size; i++) {
	if (i>0 && i%8 == 0)
	    printf("\n ");
	PrintDecimal(array[i].bucket);
    }
    printf("]\n");
}

static void PrintRvmStats(RvmStatistics *stats) {
    PrintDecimal(stats->Malloc);
    PrintDecimal(stats->Free);
    PrintDecimal(stats->MallocBytes);
    PrintDecimal(stats->FreeBytes);
}

static void PrintVCBStats(VCBStatistics *stats) {
    printf("\t%-15s", "Acquire");
    PrintDecimal(stats->Acquires);
    PrintDecimal(stats->AcquireObjs);
    PrintDecimal(stats->AcquireChecked);
    PrintDecimal(stats->AcquireFailed);
    PrintDecimal(stats->AcquireNoObjFails);	printf("\n");
    printf("\t%-15s", "Validate");
    PrintDecimal(stats->Validates);
    PrintDecimal(stats->ValidateObjs);	printf("\n");
    printf("\t%-15s", "FailedValidate");
    PrintDecimal(stats->FailedValidates);
    PrintDecimal(stats->FailedValidateObjs);	printf("\n");
    printf("\t%-15s", "Break");
    PrintDecimal(stats->Breaks);
    PrintDecimal(stats->BreakObjs);
    PrintDecimal(stats->BreakVolOnly);
    PrintDecimal(stats->BreakRefs);	printf("\n");
    printf("\t%-15s", "Clear");
    PrintDecimal(stats->Clears);
    PrintDecimal(stats->ClearObjs);
    PrintDecimal(stats->ClearRefs);	printf("\n");
    printf("\t%-15s", "NoStamp");
    PrintDecimal(stats->NoStamp);
    PrintDecimal(stats->NoStampObjs);	printf("\n");
}

static void PrintAdviceStats(AdviceStatistics *stats) {
    PrintDecimal(stats->NotEnabled);   		printf("\n");
    PrintDecimal(stats->NotValid);      	printf("\n");
    PrintDecimal(stats->Outstanding);   	printf("\n");
    PrintDecimal(stats->ASRnotAllowed); 	printf("\n");
    PrintDecimal(stats->ASRinterval);   	printf("\n");
    PrintDecimal(stats->VolumeNull);    	printf("\n");
    PrintDecimal(stats->TotalNumberAttempts);	printf("\n");
}

static void PrintAdviceCallsArray(unsigned long size, AdviceCalls array[]) {
    printf("-----------\n");
    printf("Advice Calls (by call type):\n");
    for (int i=0; i<size; i++) {
	PrintDecimal((unsigned long) i);
	PrintDecimal((unsigned long) array[i].success);
	PrintDecimal((unsigned long) array[i].failures);
	printf("\n");
    }
}

static void PrintAdviceResultsArray(unsigned long size, AdviceResults array[]) {
    printf("-----------\n");
    printf("Advice Results (by error type):\n");
    for (int i=0; i<size; i++) {
	PrintDecimal((unsigned long) i);
	PrintDecimal((unsigned long) array[i].count);
	printf("\n");
    }
}

static void LogErrorPoint(int recordCounts[]) {
    int total=0;
    for (int i=0;i<=SRVOVRFLW;i++)
	total+=recordCounts[i];
    LogMsg(0,0,LogFile,"Error encountered after processing %d records",
	   total);
    LogMsg(10,LogLevel,LogFile,
	   "\tSessions:         %d",recordCounts[SESSION]);
    LogMsg(10,LogLevel,LogFile,
	   "\tCommEvents:       %d",recordCounts[COMM]);
    LogMsg(10,LogLevel,LogFile,
	   "\tClient Calls:     %d",recordCounts[CLNTCALL]);
    LogMsg(10,LogLevel,LogFile,
	   "\tClient MCalls:	%d",recordCounts[CLNTMCALL]);
    LogMsg(10,LogLevel,LogFile,
	   "\tClient RVM Calls: %d",recordCounts[CLNTRVM]);
    LogMsg(10,LogLevel,LogFile,
	   "\tVCB records:	%d",recordCounts[VCB]);
    LogMsg(10,LogLevel,LogFile,
	   "\tAdvice records:	%d",recordCounts[ADVICE]);
    LogMsg(10,LogLevel,LogFile,
	   "\tMinicache records:%d",recordCounts[MINICACHE]);
    LogMsg(10,LogLevel,LogFile,
	   "\tOverflows:        %d",recordCounts[OVERFLOW]);
    LogMsg(10,LogLevel,LogFile,
	   "\tSrvCall Events:   %d",recordCounts[SRVCALL]);
    LogMsg(10,LogLevel,LogFile,
	   "\tResolve records:  %d",recordCounts[SRVRES]);
    LogMsg(10,LogLevel,LogFile,
	   "\tRVM Res records:	%d",recordCounts[SRVRVMRES]);
    LogMsg(10,LogLevel,LogFile,
	   "\tServer overflows: %d",recordCounts[SRVOVRFLW]);
    LogMsg(10,LogLevel,LogFile,
	   "\tIotInfo:		%d",recordCounts[IOTINFO]);
    LogMsg(10,LogLevel,LogFile,
	   "\tIotStat:		%d",recordCounts[IOTSTAT]);
    LogMsg(10,LogLevel,LogFile,
	   "\tSubtree:		%d",recordCounts[SUBTREE]);
    LogMsg(10,LogLevel,LogFile,
	   "\tRepair:		%d",recordCounts[REPAIR]);
    LogMsg(10,LogLevel,LogFile,
	   "\tRwsStat:		%d",recordCounts[RWSSTAT]);
}

static bool PrintIotInfoCall() {
    VmonVenusId Venus;
    IOT_INFO Info;
    RPC2_Integer AppNameLen;
    RPC2_String	AppName;

    int sum = ReadIotInfoCall(&Venus, &Info, &AppNameLen, &AppName);
    
    if (sum != 0) LogMsg(0,LogLevel,LogFile,"**** BAD IOTINFO RECORD ****");

    printf("%-12s: ","IOTINFO");
    PrintVenusId(&Venus);
    printf("\n");

    PrintIotInfo(&Info);
    PrintString(AppName, AppNameLen);
    printf("\n");
    
    delete [] AppName;

    if (sum) return mtrue;
    else return mfalse;
}

static bool PrintIotStatsCall()
{
    VmonVenusId Venus;
    RPC2_Integer Time;
    IOT_STAT Stats;

    int sum = ReadIotStatsCall(&Venus, &Time, &Stats);

    if (sum != 0) LogMsg(0,LogLevel,LogFile,"**** BAD IOTSTATS RECORD ****");

    printf("%-12s: ","IOTSTATS");
    PrintVenusId(&Venus);
    PrintDecimal(Time);
    PrintIotStats(&Stats);
    printf("\n");

    if (sum) return mtrue;
    else return mfalse;
}

static bool PrintSubtreeStatsCall()
{
    VmonVenusId Venus;
    RPC2_Integer Time;
    LocalSubtreeStats Stats;

    int sum = ReadSubtreeCall(&Venus, &Time, &Stats);

    if (sum != 0) LogMsg(0,LogLevel,LogFile,"**** BAD SUBTREE RECORD ****");

    printf("%-12s: ","SUBTREE");
    PrintVenusId(&Venus);
    PrintDecimal(Time);
    PrintSubtreeStats(&Stats);
    printf("\n");

    if (sum) return mtrue;
    else return mfalse;
}

static bool PrintRepairStatsCall()
{
    VmonVenusId Venus;
    RPC2_Integer Time;
    RepairSessionStats Stats;

    int sum = ReadRepairCall(&Venus, &Time, &Stats);

    if (sum != 0) LogMsg(0,LogLevel,LogFile,"**** BAD REPARI RECORD ****");

    printf("%-12s: ","REPAIR");
    PrintVenusId(&Venus);
    PrintDecimal(Time);
    PrintRepairStats(&Stats);
    printf("\n");

    if (sum) return mtrue;
    else return mfalse;
}

static bool PrintRwsStatsCall()
{
    VmonVenusId Venus;
    RPC2_Integer Time;
    ReadWriteSharingStats Stats;

    int sum = ReadRwsStatsCall(&Venus, &Time, &Stats);

    if (sum != 0) LogMsg(0,LogLevel,LogFile,"**** BAD RWSSTATS RECORD ****");
    
    printf("%-12s: ","RWSSTATS");
    PrintVenusId(&Venus);
    PrintDecimal(Time);
    PrintRwsStats(&Stats);
    printf("\n");

    if (sum) return mtrue;
    else return mfalse;
}

static void PrintIotInfo(IOT_INFO *info) {
    PrintDecimal(info->Tid);	   		printf("\n");
    PrintDecimal(info->ResOpt);	   		printf("\n");
    PrintDecimal(info->ElapsedTime);   		printf("\n");
    PrintDecimal(info->ReadSetSize);   		printf("\n");
    PrintDecimal(info->WriteSetSize);  		printf("\n");
    PrintDecimal(info->ReadVolNum);   		printf("\n");
    PrintDecimal(info->WriteVolNum);   		printf("\n");
    PrintDecimal(info->Validation);   		printf("\n");
    PrintDecimal(info->InvalidSize);  		printf("\n");
    PrintDecimal(info->BackupObjNum);  		printf("\n");
    PrintDecimal(info->LifeCycle);   		printf("\n");
    PrintDecimal(info->PredNum);  		printf("\n");
    PrintDecimal(info->SuccNum);   		printf("\n");
}

static void PrintString(RPC2_String Str, RPC2_Integer Len)
{
    for (int i = 0; i < Len; i++) 
      printf("%c", Str[i]);
}

static void PrintIotStats(IOT_STAT *stats) {
    PrintDecimal(stats->MaxElapsedTime);	printf("\n");
    PrintDecimal(stats->AvgElapsedTime);	printf("\n");
    PrintDecimal(stats->MaxReadSetSize);	printf("\n");
    PrintDecimal(stats->AvgReadSetSize);	printf("\n");
    PrintDecimal(stats->MaxWriteSetSize);	printf("\n");
    PrintDecimal(stats->AvgWriteSetSize);	printf("\n");
    PrintDecimal(stats->MaxReadVolNum);		printf("\n");
    PrintDecimal(stats->AvgReadVolNum);		printf("\n");
    PrintDecimal(stats->MaxWriteVolNum);	printf("\n");
    PrintDecimal(stats->AvgWriteVolNum);	printf("\n");
    PrintDecimal(stats->Committed);		printf("\n");
    PrintDecimal(stats->Pending); 		printf("\n");
    PrintDecimal(stats->Resolved);		printf("\n");
    PrintDecimal(stats->Repaired);		printf("\n");
    PrintDecimal(stats->OCCRerun);		printf("\n");
}

static void PrintSubtreeStats(LocalSubtreeStats *stats) {
    PrintDecimal(stats->SubtreeNum);		printf("\n");
    PrintDecimal(stats->MaxSubtreeSize);	printf("\n");
    PrintDecimal(stats->AvgSubtreeSize);	printf("\n");
    PrintDecimal(stats->MaxSubtreeHgt);		printf("\n");
    PrintDecimal(stats->AvgSubtreeHgt);		printf("\n");
    PrintDecimal(stats->MaxMutationNum);	printf("\n");
    PrintDecimal(stats->AvgMutationNum);	printf("\n");
}

static void PrintRepairStats(RepairSessionStats *stats) {
    PrintDecimal(stats->SessionNum);		printf("\n");
    PrintDecimal(stats->CommitNum);		printf("\n");
    PrintDecimal(stats->AbortNum);		printf("\n");
    PrintDecimal(stats->CheckNum);		printf("\n");
    PrintDecimal(stats->PreserveNum); 		printf("\n");
    PrintDecimal(stats->DiscardNum);		printf("\n");
    PrintDecimal(stats->RemoveNum);		printf("\n");
    PrintDecimal(stats->GlobalViewNum);        	printf("\n");
    PrintDecimal(stats->LocalViewNum);		printf("\n");
    PrintDecimal(stats->KeepLocalNum);		printf("\n");
    PrintDecimal(stats->ListLocalNum);		printf("\n");
    PrintDecimal(stats->NewCommand1Num);	printf("\n");
    PrintDecimal(stats->NewCommand2Num);	printf("\n");
    PrintDecimal(stats->NewCommand3Num);	printf("\n");
    PrintDecimal(stats->NewCommand4Num);	printf("\n");
    PrintDecimal(stats->NewCommand5Num);	printf("\n");
    PrintDecimal(stats->NewCommand6Num);	printf("\n");
    PrintDecimal(stats->NewCommand7Num);	printf("\n");
    PrintDecimal(stats->NewCommand8Num);	printf("\n");
    PrintDecimal(stats->RepMutationNum);	printf("\n");
    PrintDecimal(stats->MissTargetNum);		printf("\n");
    PrintDecimal(stats->MissParentNum);		printf("\n");
    PrintDecimal(stats->AclDenyNum);		printf("\n");
    PrintDecimal(stats->UpdateUpdateNum);	printf("\n");
    PrintDecimal(stats->NameNameNum);		printf("\n");
    PrintDecimal(stats->RemoveUpdateNum);	printf("\n");
}

static void PrintRwsStats(ReadWriteSharingStats *stats) {
    PrintDecimal(stats->Vid);			printf("\n");
    PrintDecimal(stats->RwSharingCount);	printf("\n");
    PrintDecimal(stats->DiscReadCount);		printf("\n");
    PrintDecimal(stats->DiscDuration);		printf("\n");
}
