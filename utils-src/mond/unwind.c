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
#endif /*_BLURB_*/

/*
 *    Vmon Daemon -- Data Spool Unwinder.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "mondgen.h"
#include "mond.h"

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/time.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include "coda_string.h"
#include <mach.h>
#include "advice_parser.h"
#include "scandir.h"
#include "db.h"

#ifdef __cplusplus
}
#endif __cplusplus

#include <stdarg.h>
#include "util.h"
#include "vargs.h"
#include "datalog.h"

#define STREQ(a, b) (strcmp((a), (b)) == 0)
#define LOGNAME "UnwindLog"
#define LOCKNAME "/usr/mond/bin/UnwindLock"

/* command line arguments */

/* unwindp is randomly assigned until RPC2_Init is idempotent */
char *DataBaseName = "codastats2"; /* -db */
char *WorkingDir   = "/usr/mond/log"; /* -dir */
char *DataPrefix   = "mond.data."; /* -pre */
int LogLevel       = 0; /* -d */
bool removeOnDone  = mtrue; /* -R/r */
bool doLog         = mtrue; /* -L/l */

static FILE *lockFile;
static bool done      = mfalse;
static bool everError = mfalse;

void Log_Done();

static void ParseArgs(int, char *[]);
static void SendData(char *);
static void GetSession(bool *);
static void GetComm(bool *);
static void GetClientCall(bool *);
static void GetClientMCall(bool *);
static void GetClientRVM(bool *);
static void GetVCB(bool *);
static void GetAdvice(bool *);
static void GetMiniCache(bool *);
static void GetOverflow(bool *);
static void GetSrvCall(bool *);
static void GetResEvent(bool *);
static void GetRvmResEvent(bool *);
static void GetSrvOverflow(bool *);
static void GetIotInfo(bool *);
static void GetIotStats(bool *);
static void GetSubtree(bool *);
static void GetRepair(bool *);
static void GetRwsStats(bool *);
static void InitLog();
static int ScreenForData(struct direct *);
static void GetFilesAndSpool();
static int TestAndLock();
static void RemoveLock();
static void InitSignals();
static void TermSignal();
static void LogErrorPoint(int[]);
static void zombie(int, int, struct sigcontext *);

FILE *LogFile  = 0;
FILE *DataFile = 0;
static struct sigcontext OldContext;

main(int argc, char *argv[])
{
    if (TestAndLock()) {
        fprintf(stderr, "Another unwind running or abandoned, please check\n");
        exit(EXIT_FAILURE);
    }
    ParseArgs(argc, argv);
    InitSignals();
    InitLog();
    if (chdir(WorkingDir)) {
        RemoveLock();
        Die("Could not cd into %s", WorkingDir);
    }
    if (InitDB(DataBaseName)) {
        RemoveLock();
        fprintf(stderr, "Could not connect to database %s", DataBaseName);
        exit(EXIT_FAILURE);
    }
    GetFilesAndSpool();
    RemoveLock();
    Log_Done();
}

static void ParseArgs(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++) {
        if (STREQ(argv[i], "-db")) { /* database */
            DataBaseName = argv[++i];
            continue;
        }
        if (STREQ(argv[i], "-wd")) { /* working directory */
            WorkingDir = argv[++i];
            continue;
        }
        if (STREQ(argv[i], "-pre")) { /* working directory */
            DataPrefix = argv[++i];
            continue;
        } else if (STREQ(argv[i], "-d")) { /* log level */
            LogLevel = atoi(argv[++i]);
            continue;
        } else if (STREQ(argv[i], "-R")) { /* remove */
            removeOnDone = mtrue;
            continue;
        } else if (STREQ(argv[i], "-r")) { /* don't remove */
            removeOnDone = mfalse;
            continue;
        } else if (STREQ(argv[i], "-L")) { /* log */
            doLog = mtrue;
            continue;
        } else if (STREQ(argv[i], "-l")) { /* don't log */
            doLog = mfalse;
            continue;
        }
        printf("usage: unwind [-db database] [-wd workingDir]\n");
        printf("              [-pre dataPrefix] [-d logLevel]\n");
        printf("              [-R | -r] [-L | -l]\n");
        RemoveLock();
        exit(EXIT_FAILURE);
    }
}

static void SendData(char *file)
{
    DataFile   = fopen(file, "r");
    bool error = mfalse;

    done      = mfalse;
    everError = mfalse;

    if (DataFile == NULL) {
        LogMsg(0, LogLevel, LogFile, "Could not Open %s for reading", file);
        done  = mtrue;
        error = mtrue;
    }

    int recordCounts[dataClass_last_tag];
    for (int i = 0; i < dataClass_last_tag; i++)
        recordCounts[i] = 0;

    long rt;
    long count;
    while (done == mfalse) {
        count = ScanPastMagicNumber(&rt);
        if (count > 0) {
            LogMsg(
                0, LogLevel, LogFile,
                "Out of sync with data file: %d words skipped to next sync point",
                count);
            everError = mtrue;
            LogErrorPoint(recordCounts);
        } else if (count < 0) {
            LogMsg(0, LogLevel, LogFile, "End of data file");
            LogErrorPoint(recordCounts);
        }
        switch (rt) {
        case -1:
            done = mtrue;
            break;
        case SESSION_TAG:
            GetSession(&error);
            recordCounts[SESSION]++;
            break;
        case COMM_TAG:
            GetComm(&error);
            recordCounts[COMM]++;
            break;
        case CLNTCALL_TAG:
            GetClientCall(&error);
            recordCounts[CLNTCALL]++;
            break;
        case CLNTMCALL_TAG:
            GetClientMCall(&error);
            recordCounts[CLNTMCALL]++;
            break;
        case CLNTRVM_TAG:
            GetClientRVM(&error);
            recordCounts[CLNTRVM]++;
            break;
        case VCB_TAG:
            GetVCB(&error);
            recordCounts[VCB]++;
            break;
        case ADVICE_TAG:
            GetAdvice(&error);
            recordCounts[ADVICE]++;
            break;
        case MINICACHE_TAG:
            GetMiniCache(&error);
            recordCounts[MINICACHE]++;
            break;
        case OVERFLOW_TAG:
            GetOverflow(&error);
            recordCounts[OVERFLOW]++;
            break;
        case SRVCALL_TAG:
            GetSrvCall(&error);
            recordCounts[SRVCALL]++;
            break;
        case SRVRES_TAG:
            GetResEvent(&error);
            recordCounts[SRVRES]++;
            break;
        case SRVRVMRES_TAG:
            GetRvmResEvent(&error);
            recordCounts[SRVRVMRES]++;
            break;
        case SRVOVRFLW_TAG:
            GetSrvOverflow(&error);
            recordCounts[SRVOVRFLW]++;
            break;
        case IOTINFO_TAG:
            GetIotInfo(&error);
            recordCounts[IOTINFO]++;
            break;
        case IOTSTAT_TAG:
            GetIotStats(&error);
            recordCounts[IOTSTAT]++;
            break;
        case SUBTREE_TAG:
            GetSubtree(&error);
            recordCounts[SUBTREE]++;
            break;
        case REPAIR_TAG:
            GetRepair(&error);
            recordCounts[REPAIR]++;
            break;
        case RWSSTAT_TAG:
            GetRwsStats(&error);
            recordCounts[RWSSTAT]++;
            break;
        default:
            LogMsg(1, LogLevel, LogFile, "main: bogus rt (%d)", rt);
            error = mtrue;
        }
        if (error == mtrue) {
            everError = mtrue;
            LogErrorPoint(recordCounts);
            error = mfalse;
        }
    }
    fclose(DataFile);
    if (everError == mfalse) {
        if (removeOnDone == mtrue) {
            if (unlink(file))
                LogMsg(0, LogLevel, LogFile,
                       "Could not unlink %s, but spooled it with no errors",
                       file);
        }
    } else
        LogMsg(0, LogLevel, LogFile, "Error spooling file %s", file);
}

static void GetSession(bool *error)
{
    LogMsg(100, LogLevel, LogFile, "Spooling a session event");
    int sum = 0;
    VmonVenusId Venus;
    VmonSessionId Session;
    VolumeId Volume;
    UserId User;
    VmonAVSG AVSG;
    RPC2_Unsigned StartTime;
    RPC2_Unsigned EndTime;
    RPC2_Unsigned CETime;
    VmonSessionEventArray Events;
    SessionStatistics Stats;
    CacheStatistics CacheStats;

    sum = ReadSessionRecord(&Venus, &Session, &Volume, &User, &AVSG, &StartTime,
                            &EndTime, &CETime, &Events, &Stats, &CacheStats);
    if (sum == 0)
        LogMsg(100, LogLevel, LogFile, "Spooling a session: [%lu 0x%lx %d %lu]",
               Session, Volume, User, CETime);
    sum = ReportSession(&Venus, Session, Volume, User, &AVSG, StartTime,
                        EndTime, CETime, &Events, &Stats, &CacheStats);
    if (sum != 0) {
        *error = mtrue;
    }
}

static void GetComm(bool *error)
{
    LogMsg(100, LogLevel, LogFile, "Spooling a comm event");
    VmonVenusId Venus;
    RPC2_Unsigned ServerIPAddress;
    RPC2_Integer SerialNumber;
    RPC2_Unsigned Time;
    VmonCommEventType Type;
    int sum =
        ReadCommRecord(&Venus, &ServerIPAddress, &SerialNumber, &Time, &Type);
    if (sum == 0)
        sum =
            ReportCommEvent(&Venus, ServerIPAddress, SerialNumber, Time, Type);
    if (sum != 0) {
        *error = mtrue;
    }
}

static void GetClientCall(bool *error)
{
    int sum = 0;
    LogMsg(100, LogLevel, LogFile, "Spooling a client call record");

    VmonVenusId Venus;
    long Time;
    unsigned long sc_size;
    CallCountEntry *SrvCount;
    sum = ReadClientCall(&Venus, &Time, &sc_size, &SrvCount);
    if (sum == 0)
        sum = ReportClientCall(&Venus, Time, sc_size, SrvCount);
    if (sum != 0) {
        *error = mtrue;
    }
    RemoveCountArray(sc_size, SrvCount);
}

static void GetClientMCall(bool *error)
{
    int sum = 0;
    LogMsg(100, LogLevel, LogFile, "Spooling a client mcall record");

    VmonVenusId Venus;
    long Time;
    unsigned long msc_size;
    MultiCallEntry *MSrvCount;
    sum = ReadClientMCall(&Venus, &Time, &msc_size, &MSrvCount);
    if (sum == 0)
        sum = ReportClientMCall(&Venus, Time, msc_size, MSrvCount);
    if (sum != 0) {
        *error = mtrue;
    }
    RemoveMultiArray(msc_size, MSrvCount);
}

static void GetClientRVM(bool *error)
{
    int sum = 0;
    LogMsg(100, LogLevel, LogFile, "Spooling a client RVM record");

    VmonVenusId Venus;
    long Time;
    RvmStatistics Stats;
    sum = ReadClientRVM(&Venus, &Time, &Stats);
    if (sum == 0)
        sum = ReportClientRVM(&Venus, Time, &Stats);
    if (sum != 0) {
        *error = mtrue;
    }
}

static void GetVCB(bool *error)
{
    int sum = 0;
    LogMsg(100, LogLevel, LogFile, "Spooling a VCB record");

    VmonVenusId Venus;
    long VenusInit;
    long Time;
    VolumeId Volume;
    VCBStatistics Stats;
    sum = ReadVCB(&Venus, &VenusInit, &Time, &Volume, &Stats);
    if (sum == 0)
        sum = ReportVCB(&Venus, VenusInit, Time, Volume, &Stats);
    if (sum != 0) {
        *error = mtrue;
    }
}

static void GetAdvice(bool *error)
{
    int sum = 0;
    LogMsg(100, LogLevel, LogFile, "Spooling an advice record");

    VmonVenusId Venus;
    long Time;
    UserId User;
    AdviceStatistics Stats;
    unsigned long Call_Size;
    AdviceCalls *Call_Stats;
    unsigned long Result_Size;
    AdviceResults *Result_Stats;

    sum = ReadAdviceCall(&Venus, &Time, &User, &Stats, &Call_Size, &Call_Stats,
                         &Result_Size, &Result_Stats);
    if (sum == 0)
        sum = ReportAdviceCall(&Venus, Time, User, &Stats, Call_Size,
                               Call_Stats, Result_Size, Result_Stats);
    if (sum != 0)
        *error = mtrue;

    delete[] Call_Stats;
    delete[] Result_Stats;
}

static void GetMiniCache(bool *error)
{
    int sum = 0;
    LogMsg(100, LogLevel, LogFile, "Spooling a minicache record");

    VmonVenusId Venus;
    long Time;
    unsigned long vn_size;
    VmonMiniCacheStat *vn_stat;
    unsigned long vfs_size;
    VmonMiniCacheStat *vfs_stat;

    sum = ReadMiniCacheCall(&Venus, &Time, &vn_size, &vn_stat, &vfs_size,
                            &vfs_stat);

    if (sum == 0)
        sum =
            ReportMiniCache(&Venus, Time, vn_size, vn_stat, vfs_size, vfs_stat);
    if (sum != 0) {
        *error = mtrue;
    }

    delete[] vn_stat;
    delete[] vfs_stat;
}

static void GetOverflow(bool *error)
{
    LogMsg(100, LogLevel, LogFile, "Spooling an overflow event");
    int sum = 0;
    VmonVenusId Venus;
    RPC2_Unsigned VMStartTime;
    RPC2_Unsigned VMEndTime;
    RPC2_Unsigned VMCount;
    RPC2_Unsigned RVMStartTime;
    RPC2_Unsigned RVMEndTime;
    RPC2_Unsigned RVMCount;
    sum = ReadOverflow(&Venus, &VMStartTime, &VMEndTime, &VMCount,
                       &RVMStartTime, &RVMEndTime, &RVMCount);
    if (sum == 0)
        sum = ReportOverflow(&Venus, VMStartTime, VMEndTime, VMCount,
                             RVMStartTime, RVMEndTime, RVMCount);
    if (sum != 0) {
        *error = mtrue;
    }
}

static void GetSrvCall(bool *error)
{
    LogMsg(100, LogLevel, LogFile, "Spooling a server call event");
    int sum = 0;

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
    sum = ReadSrvCall(&Vice, &Time, &CBSize, &CBCount, &ResSize, &ResCount,
                      &SmonSize, &SmonCount, &VolDSize, &VolDCount, &MultiSize,
                      &MultiCount, &Stats);
    if (sum == 0)
        sum = ReportSrvCall(&Vice, Time, CBSize, CBCount, ResSize, ResCount,
                            SmonSize, SmonCount, VolDSize, VolDCount, MultiSize,
                            MultiCount, &Stats);
    if (sum != 0) {
        *error = mtrue;
    }
    RemoveCountArray(CBSize, CBCount);
    RemoveCountArray(ResSize, ResCount);
    RemoveCountArray(SmonSize, SmonCount);
    RemoveCountArray(VolDSize, VolDCount);
    RemoveMultiArray(MultiSize, MultiCount);
}

static void GetResEvent(bool *error)
{
    LogMsg(100, LogLevel, LogFile, "Spooling a resolve event");
    int sum = 0;
    SmonViceId Vice;
    unsigned long Time;
    unsigned long Volid;
    long HighWaterMark;
    long AllocNumber;
    long DeallocNumber;
    unsigned long ResOpSize;
    ResOpEntry *ResOp;

    sum = ReadResEvent(&Vice, &Time, &Volid, &HighWaterMark, &AllocNumber,
                       &DeallocNumber, &ResOpSize, &ResOp);
    if (sum == 0)
        sum = ReportResEvent(&Vice, Time, Volid, HighWaterMark, AllocNumber,
                             DeallocNumber, ResOpSize, ResOp);
    if (sum != 0)
        *error = mtrue;
    delete[] ResOp;
}

static void GetRvmResEvent(bool *error)
{
    LogMsg(100, LogLevel, LogFile, "Spooling a rvmres summary");
    int sum = 0;
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
    sum = ReadRvmResEvent(&Vice, &Time, &VolID, &FileRes, &DirRes, &lshsize,
                          &LogSizeHisto, &lmhsize, &LogMaxHisto, &Conflicts,
                          &shhsize, &SuccHierHist, &fhhsize, &FailHierHist,
                          &ResLog, &vlhsize, &VarLogHisto, &lssize, &LogSize);
    if (sum == 0)
        sum = ReportRvmResEvent(&Vice, Time, VolID, &FileRes, &DirRes, lshsize,
                                LogSizeHisto, lmhsize, LogMaxHisto, &Conflicts,
                                shhsize, SuccHierHist, fhhsize, FailHierHist,
                                &ResLog, vlhsize, VarLogHisto, lssize, LogSize);
    if (sum != 0)
        *error = mtrue;
}

static void GetSrvOverflow(bool *error)
{
    int sum = 0;

    SmonViceId Vice;
    unsigned long Time;
    unsigned long StartTime;
    unsigned long EndTime;
    long Count;
    sum = ReadSrvOverflow(&Vice, &Time, &StartTime, &EndTime, &Count);
    if (sum == 0)
        sum = ReportSrvOvrflw(&Vice, Time, StartTime, EndTime, Count);
    if (sum != 0)
        *error = mtrue;
}

static void GetIotInfo(bool *error)
{
    int sum = 0;
    LogMsg(100, LogLevel, LogFile, "Spooling an iotInfo record");

    VmonVenusId Venus;
    IOT_INFO Info;
    RPC2_Integer AppNameLen;
    RPC2_String AppName;

    sum = ReadIotInfoCall(&Venus, &Info, &AppNameLen, &AppName);
    if (sum == 0)
        sum = ReportIotInfoCall(&Venus, &Info, AppNameLen, AppName);
    else
        LogMsg(100, LogLevel, LogFile, "GetIotInfo: ReadIotInfoCall error");
    if (sum != 0)
        *error = mtrue;

    delete[] AppName;
}

static void GetIotStats(bool *error)
{
    int sum = 0;
    LogMsg(100, LogLevel, LogFile, "Spooling an iotStat record");

    VmonVenusId Venus;
    RPC2_Integer Time;
    IOT_STAT Stats;

    sum = ReadIotStatsCall(&Venus, &Time, &Stats);
    if (sum == 0)
        sum = ReportIotStatsCall(&Venus, Time, &Stats);
    if (sum != 0)
        *error = mtrue;
}

static void GetSubtree(bool *error)
{
    int sum = 0;
    LogMsg(100, LogLevel, LogFile, "Spooling an subtree record");

    VmonVenusId Venus;
    RPC2_Integer Time;
    LocalSubtreeStats Stats;

    sum = ReadSubtreeCall(&Venus, &Time, &Stats);
    if (sum == 0)
        sum = ReportSubtreeCall(&Venus, Time, &Stats);
    if (sum != 0)
        *error = mtrue;
}

static void GetRepair(bool *error)
{
    int sum = 0;
    LogMsg(100, LogLevel, LogFile, "Spooling an repair record");

    VmonVenusId Venus;
    RPC2_Integer Time;
    RepairSessionStats Stats;

    sum = ReadRepairCall(&Venus, &Time, &Stats);
    if (sum == 0)
        sum = ReportRepairCall(&Venus, Time, &Stats);
    if (sum != 0)
        *error = mtrue;
}

static void GetRwsStats(bool *error)
{
    int sum = 0;
    LogMsg(100, LogLevel, LogFile, "Spooling an rwsStat record");

    VmonVenusId Venus;
    RPC2_Integer Time;
    ReadWriteSharingStats Stats;

    sum = ReadRwsStatsCall(&Venus, &Time, &Stats);
    if (sum == 0)
        sum = ReportRwsStatsCall(&Venus, Time, &Stats);
    if (sum != 0)
        *error = mtrue;
}

static void InitLog()
{
    char LogFilePath[256]; /* "WORKINGDIR/LOGFILE_PREFIX.MMDD" */
    {
        strcpy(LogFilePath, WorkingDir);
        strcat(LogFilePath, "/");
        strcat(LogFilePath, LOGNAME);
    }

    LogFile = fopen(LogFilePath, "a");
    /*    LogFile = stdout; */
    if (LogFile == NULL) {
        fprintf(stderr, "LOGFILE (%s) initialization failed\n", LOGNAME);
        exit(EXIT_FAILURE);
    }

    struct timeval now;
    gettimeofday(&now, 0);
    char *s = ctime(&now.tv_sec);
    LogMsg(0, LogLevel, LogFile, "LOGFILE initialized with LogLevel = %d at %s",
           LogLevel, ctime(&now.tv_sec));
    LogMsg(0, LogLevel, LogFile, "My pid is %d", getpid());
}

void Log_Done()
{
    struct timeval now;
    gettimeofday(&now, 0);
    LogMsg(0, LogLevel, LogFile, "LOGFILE terminated at %s",
           ctime(&now.tv_sec));

    fclose(LogFile);
    LogFile = 0;
}

static int ScreenForData(struct direct *de)
{
    return (!strncmp(DataPrefix, de->d_name, strlen(DataPrefix)));
}

static void GetFilesAndSpool()
// GFTS assumes we are cd'd into the WorkingDir (main does this)
{
    struct direct **nameList;
    int numfiles = scandir(".", &(nameList), (PFI)ScreenForData, NULL);
    if (numfiles <= 1) {
        LogMsg(0, LogLevel, LogFile, "No data to spool in directory %s",
               WorkingDir);
        return;
    }
    LogMsg(100, LogLevel, LogFile, "GFAS: Found %d data files", numfiles);
    time_t longest = 0;
    int longestIndex;
    struct stat buf;
    int i;
    for (i = 0; i < numfiles; i++) {
        stat(nameList[i]->d_name, &buf);
        if (buf.st_mtime == longest) {
            LogMsg(0, LogLevel, LogFile,
                   "Two (or more) data files with the same mtime - CYa!");
            return;
        }
        if (buf.st_mtime > longest) {
            longest      = buf.st_mtime;
            longestIndex = i;
        }
    }
    LogMsg(10, LogLevel, LogFile, "GFAS: Least recent file: %s",
           nameList[longestIndex]->d_name);
    for (i = 0; i < numfiles; i++) {
        if (i != longestIndex) {
            LogMsg(10, LogLevel, LogFile, "GFAS: Sending %s",
                   nameList[i]->d_name);
            SendData(nameList[i]->d_name);
        }
    }
    UpdateDB();
}

static int TestAndLock()
{
    struct stat buf;
    if (stat(LOCKNAME, &buf) == 0)
        return 1;
    if (errno != ENOENT) {
        fprintf(stderr, "Problem checking lock %s (%d)\n", LOCKNAME, errno);
        return 1;
    }
    lockFile = fopen(LOCKNAME, "w");
    if (lockFile == NULL) {
        fprintf(stderr, "Could not open lock file %s (%d)\n", LOCKNAME, errno);
        return 1;
    }
    fprintf(lockFile, "%d\n", getpid());
    fclose(lockFile);
    return 0;
}

static void RemoveLock()
{
    if (unlink(LOCKNAME) != 0)
        LogMsg(0, LogLevel, LogFile, "Could not remove lock %s (%d)\n",
               LOCKNAME, errno);
}

static void InitSignals()
{
    (void)signal(SIGTERM, (void (*)(int))TermSignal);
    signal(SIGTRAP, (void (*)(int))zombie);
    signal(SIGILL, (void (*)(int))zombie);
    signal(SIGBUS, (void (*)(int))zombie);
    signal(SIGSEGV, (void (*)(int))zombie);
    signal(SIGFPE, (void (*)(int))zombie); // software exception
}

void zombie(int sig, int code, struct sigcontext *scp)
{
    memcpy(&OldContext, scp, (int)sizeof(struct sigcontext));
    LogMsg(0, 0, LogFile, "****** INTERRUPTED BY SIGNAL %d CODE %d ******", sig,
           code);
    LogMsg(0, 0, LogFile,
           "****** Aborting outstanding transactions, stand by...");

    LogMsg(0, 0, LogFile, "To debug via gdb: attach %d, setcontext OldContext",
           getpid());
    LogMsg(0, 0, LogFile, "Becoming a zombie now ........");
    task_suspend(task_self());
}

static void TermSignal()
{
    LogMsg(0, LogLevel, LogFile,
           "Term signal caught, finishing current record");
    // set up things for the unwinder to end after this record
    // to avoid death in the midst of a transaction.
    done      = mtrue;
    everError = mtrue;
    return;
}

static void LogErrorPoint(int recordCounts[])
{
    int total = 0;
    for (int i = 0; i < dataClass_last_tag; i++)
        total += recordCounts[i];
    LogMsg(0, 0, LogFile, "Error encountered after processing %d records",
           total);
    LogMsg(10, LogLevel, LogFile, "\tSessions:         %d",
           recordCounts[SESSION]);
    LogMsg(10, LogLevel, LogFile, "\tCommEvents:       %d", recordCounts[COMM]);
    LogMsg(10, LogLevel, LogFile, "\tClient Calls:     %d",
           recordCounts[CLNTCALL]);
    LogMsg(10, LogLevel, LogFile, "\tClient MCalls:    %d",
           recordCounts[CLNTMCALL]);
    LogMsg(10, LogLevel, LogFile, "\tClient RVM:       %d",
           recordCounts[CLNTRVM]);
    LogMsg(10, LogLevel, LogFile, "\tVCB:              %d", recordCounts[VCB]);
    LogMsg(10, LogLevel, LogFile, "\tAdvice:		%d", recordCounts[ADVICE]);
    LogMsg(10, LogLevel, LogFile, "\tMiniCache Events: %d",
           recordCounts[MINICACHE]);
    LogMsg(10, LogLevel, LogFile, "\tOverflows:        %d",
           recordCounts[OVERFLOW]);
    LogMsg(10, LogLevel, LogFile, "\tSrvCall Events:   %d",
           recordCounts[SRVCALL]);
    LogMsg(10, LogLevel, LogFile, "\tResolve records:  %d",
           recordCounts[SRVRES]);
    LogMsg(10, LogLevel, LogFile, "\tRVM res events:   %d",
           recordCounts[SRVRVMRES]);
    LogMsg(10, LogLevel, LogFile, "\tServer overflows: %d",
           recordCounts[SRVOVRFLW]);
    LogMsg(10, LogLevel, LogFile, "\tIotInfo:		%d", recordCounts[IOTINFO]);
    LogMsg(10, LogLevel, LogFile, "\tIotStat:		%d", recordCounts[IOTSTAT]);
    LogMsg(10, LogLevel, LogFile, "\tSubtree:		%d", recordCounts[SUBTREE]);
    LogMsg(10, LogLevel, LogFile, "\tRepair:		%d", recordCounts[REPAIR]);
    LogMsg(10, LogLevel, LogFile, "\tRwsStat:		%d", recordCounts[RWSSTAT]);
}
