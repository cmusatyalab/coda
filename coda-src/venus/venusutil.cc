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

static char *rcsid = "$Header: /home/braam/src/coda-src/venus/RCS/venusutil.cc,v 1.2 1996/11/24 21:11:20 braam Exp $";
#endif /*_BLURB_*/







/*
 *
 *     Utility routines used by Venus.
 *
 *    ToDo:
 *
 */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/param.h>
#include <errno.h>
#include <stdarg.h>
#include <math.h>
#include <string.h>
#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#endif /* __MACH__ */
#ifdef __NetBSD__
#include <unistd.h>
#include <stdlib.h>
#endif __NetBSD__

#include <rpc2.h>

#ifdef	__MACH__
#include <mach.h>
#endif	__MACH__

#ifdef __cplusplus
}
#endif __cplusplus

/* interfaces */
#include <vice.h>

/* from plumber (include-special) */
#include <newplumb.h>

/* from util */
#include <util.h>

/* from vv */
#include <inconsist.h>

/* from venus */
#include "comm.h"
#include "fso.h"
#include "hdb.h"
#include "local.h"
#include "mariner.h"
#include "simulate.h"
#include "user.h"
#include "venus.private.h"
#include "venus.version.h"
#include "venuscb.h"
#include "venusioctl.h"
#include "venusrecov.h"
#include "venusstats.h"
#include "venusvm.h"
#include "venusvol.h"
#include "vproc.h"
#include "worker.h"


/* *****  Exported variables  ***** */

FILE *logFile = 0;
int LogLevel = 0;
int ProfBoot = 0;
int MallocTrace = 0;
int Profiling = 0;
ViceFid NullFid = {0, 0, 0};
vv_t NullVV = {{0, 0, 0, 0, 0, 0, 0, 0}, {0, 0}, 0};
VFSStatistics VFSStats;
RPCOpStatistics RPCOpStats;


/* *****  Private variables  ***** */

PRIVATE int LogInited = 0;
PRIVATE char *VFSOpsNameTemplate[NVFSOPS] = {
    "No-Op",
    "No-Op",
    "Root",
    "Sync",
    "Open",
    "Close",
    "Ioctl",
    "Getattr",
    "Setattr",
    "Access",
    "Lookup",
    "Create",
    "Remove",
    "Link",
    "Rename",
    "Mkdir",
    "Rmdir",
    "Readdir",
    "Symlink",
    "Readlink",
    "Fsync",
    "Inactive",
    "Vget",
    "Signal",
    "Invalidate",
    "Flush",
    "PurgeUser",
    "ZapFile",
    "ZapDir",
    "ZapVnode",
    "PurgeFid",
    "RdWr",
    "Resolve",
    "Reintegrate",
    "No-Op",
    "No-Op",
    "No-Op",
    "No-Op",
    "No-Op",
    "No-Op"
};



/* *****  util.c  ***** */

/* Send a message out on a file descriptor. */
void fdprint(long afd, char *fmt ...) {
    va_list ap;
    char buf[240];

    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    va_end(ap);
    write((int) afd, buf, (int) strlen(buf));
}


/* Send an error message to stderr. */
void eprint(char *fmt ...) {
    va_list ap;
    char msg[240];
    char *cp = msg;

    /* Construct message in buffer and add newline */
    va_start(ap, fmt);
    vsprintf(cp, fmt, ap);
    va_end(ap);
    cp += strlen(cp);
    strcat(cp, "\n");

    /* Write to stderr */
    PrintTimeStamp(stderr);  /* first put out a timestamp */
    fprintf(stderr, msg); /* then the message */
    fflush(stderr);

    /* Put a copy in the log for debugging. */
    LOG(0, ("%.128s", msg));
}


/* Print a debugging message to the log file. */
void dprint(char *fmt ...) {
    va_list ap;

    if (!LogInited) return;

    char msg[240];
    (VprocSelf())->GetStamp(msg);

    /* Output a newline if we are starting a new block. */
    static int last_vpid = -1;
    static int last_seq = -1;
    int this_vpid;
    int this_seq;
    if (sscanf(msg, "[ %*c(%d) : %d : %*02d:%*02d:%*02d ] ", &this_vpid, &this_seq) != 2) {
	fprintf(stderr, "Choking in dprint\n");
	exit(-1);
    }
    if ((this_vpid != last_vpid || this_seq != last_seq) && (this_vpid != -1)) {
	fprintf(logFile, "\n");
	last_vpid = this_vpid;
	last_seq = this_seq;
    }

    va_start(ap, fmt);
    vsprintf(msg + strlen(msg), fmt, ap);
    va_end(ap);

    fwrite(msg, (int)sizeof(char), (int) strlen(msg), logFile);
    fflush(logFile);
}


/* Print an error message and then exit. */
void Choke(char *fmt ...) {
    static int dying = 0;

    if (!dying) {
	/* Avoid recursive death. */
	dying = 1;

	/* eprint the message, with an indication that it is fatal. */
	va_list ap;
	char msg[240];
	strcpy(msg, "fatal error -- ");
	va_start(ap, fmt);
	vsprintf(msg + strlen(msg), fmt, ap);
	va_end(ap);
	eprint(msg);

	/* Dump system state to the log. */
	if (Simulating && recPtr != 0)
	    Trace_PrintRecord(recPtr);
	DumpState();

	/* Flush session record to RVM. */
	VDB->FlushVolume();

	/* Force meta-data changes to disk. */
	RecovFlush(1);
	RecovTerminate();

	/* Unmount if possible. */
	VFSUnmount();

	/* Make a final check for arena corruption. */
	(void)CheckAllocs("Choke");
    }

    if (LogInited)
	fflush(logFile);
    fflush(stderr);
    fflush(stdout);

    assert(0);

    /* NOTREACHED */
}

/* Dir package expects to find a Die() routine */
void Die(char *msg)
    {
    Choke(msg);
    }


void VenusPrint(int argc, char **argv) {
    VenusPrint(stdout, argc, argv);
}


void VenusPrint(FILE *fp, int argc, char **argv) {
    fflush(fp);
    VenusPrint(fileno(fp), argc, argv);
}


/* local-repair modification */
void VenusPrint(int fd, int argc, char **argv) {
    int allp = 0;
    int rusagep = 0;
    int recovp = 0;
    int vprocp = 0;
    int userp = 0;
    int serverp = 0;
    int connp = 0;
    int vsgp = 0;
    int mgrpp = 0;
    int volumep = 0;
    int fsop = 0;
    int	fsosump	= 0;	    /* summary only */
    int vfsp = 0;
    int rpcp = 0;
    int hdbp = 0;
    int vmonp = 0;
    int mallocp = 0;
    int lrdbp = 0;
    int vcbdbp = 0;

    /* Parse the argv to see what modules should be printed. */
    for (int i = 0; i < argc; i++) {
	if (STREQ(argv[i], "all")) { allp++; break; }
	else if (STREQ(argv[i], "rusage")) rusagep++;
	else if (STREQ(argv[i], "recov")) recovp++;
	else if (STREQ(argv[i], "vproc")) vprocp++;
	else if (STREQ(argv[i], "user")) userp++;
	else if (STREQ(argv[i], "server")) serverp++;
	else if (STREQ(argv[i], "conn")) connp++;
	else if (STREQ(argv[i], "vsg")) vsgp++;
	else if (STREQ(argv[i], "mgrp")) mgrpp++;
	else if (STREQ(argv[i], "volume")) volumep++;
	else if (STREQ(argv[i], "fso")) fsop++;
	else if (STREQ(argv[i], "fsosum")) fsosump++;
	else if (STREQ(argv[i], "vfs")) vfsp++;
	else if (STREQ(argv[i], "rpc")) rpcp++;
	else if (STREQ(argv[i], "hdb")) hdbp++;
	else if (STREQ(argv[i], "vmon")) vmonp++;
	else if (STREQ(argv[i], "malloc")) mallocp++;
	else if (STREQ(argv[i], "lrdb")) lrdbp++;
	else if (STREQ(argv[i], "vcbdb")) vcbdbp++;
    }

    fdprint(fd, "*****  VenusPrint  *****\n\n");
    if (rusagep || allp) RusagePrint(fd);
    if (recovp || allp) if (RecovInited) RecovPrint(fd);
    if (vprocp || allp) PrintVprocs(fd);
    if (userp || allp) UserPrint(fd);
    if (serverp || allp) ServerPrint(fd);
    if (connp || allp) ConnPrint(fd);
    if (vsgp || allp) if (RecovInited) VSGDB->print(fd);
    if (mgrpp || allp) MgrpPrint(fd);
    if (volumep || allp) if (RecovInited) VDB->print(fd);
    if (fsop || allp) if (RecovInited) FSDB->print(fd);
    if (fsosump && !allp) if (RecovInited) FSDB->print(fd, 1);
    if (vfsp || allp) VFSPrint(fd);
    if (rpcp || allp) RPCPrint(fd);
    if (hdbp || allp) if (RecovInited) HDB->print(fd);
    if (vmonp || allp) VmonPrint(fd);
    if (mallocp || allp) MallocPrint(fd);
    if (lrdbp || allp) if (RecovInited) LRDB->print(fd);
    if (vcbdbp || allp) if (RecovInited) VCBDB->print(fd);
    fdprint(fd, "************************\n\n");
}


char *VenusOpStr(int opcode) {
    static char	buf[12];    /* This is shaky. */

    if (opcode >= 0 && opcode < NVFSOPS)
	return(VFSStats.VFSOps[opcode].name);

    sprintf(buf, "%d", opcode);
    return(buf);
}


char *IoctlOpStr(int opcode) {
    static char	buf[12];    /* This is shaky. */

    switch(opcode) {
/*
	case VIOCCLOSEWAIT:	    return("CloseWait");
	case VIOCABORT:		    return("Abort");
*/
	case VIOCSETAL:		    return("Set ACL");
	case VIOCGETAL:		    return("Get ACL");
	case VIOCSETTOK:	    return("Set Tokens");
	case VIOCGETVOLSTAT:	    return("Get VolStat");
	case VIOCSETVOLSTAT:	    return("Set VolStat");
	case VIOCFLUSH:		    return("Flush");
	case VIOCSTAT:		    return("Stat");
	case VIOCGETTOK:	    return("Get Tokens");
	case VIOCUNLOG:		    return("Unlog");
	case VIOCCKSERV:	    return("Check Servers");
	case VIOCCKBACK:	    return("Check Backups");
	case VIOCCKCONN:	    return("Check Conn");
	case VIOCGETTIME:	    return("Get Time");
	case VIOCWHEREIS:	    return("Whereis");
	case VIOCPREFETCH:	    return("Prefetch");
	case VIOCNOP:		    return("NOP");
	case VIOCENGROUP:	    return("Enable Group");
	case VIOCDISGROUP:	    return("Disable Group");
	case VIOCLISTGROUPS:	    return("List Groups");
	case VIOCACCESS:	    return("Access");
	case VIOCUNPAG:		    return("Unpag");
	case VIOCGETWD:		    return("Getwd");
	case VIOCWAITFOREVER:	    return("Wait Forever");
	case VIOCSETCACHESIZE:	    return("Set Cache Size");
	case VIOCFLUSHCB:	    return("Flush CB");
	case VIOCNEWCELL:	    return("New Cell");
	case VIOCGETCELL:	    return("Get Cell");
	case VIOC_AFS_DELETE_MT_PT: return("[AFS] Delete Mount Point");
	case VIOC_AFS_STAT_MT_PT:   return("[AFS] Stat Mount Point");
	case VIOC_FILE_CELL_NAME:   return("File Cell Name");
	case VIOC_GET_WS_CELL:	    return("WS Cell Name");
	case VIOC_AFS_MARINER_HOST: return("[AFS] Mariner Host");
	case VIOC_GET_PRIMARY_CELL: return("Get Primary Cell");
	case VIOC_VENUSLOG:	    return("Venus Log");
	case VIOC_GETCELLSTATUS:    return("Get Cell Status");
	case VIOC_SETCELLSTATUS:    return("Set Cell Status");
	case VIOC_FLUSHVOLUME:	    return("Flush Volume");
	case VIOC_ENABLEREPAIR:	    return("Enable Repair");
	case VIOC_DISABLEREPAIR:    return("Disable Repair");
	case VIOC_REPAIR:	    return("Repair");
	case VIOC_GETSERVERSTATS:   return("Get Server Stats");
	case VIOC_GETVENUSSTATS:    return("Get Venus Stats");
	case VIOC_GETFID:	    return("Get Fid");
	case VIOC_FLUSHCACHE:	    return("Flush Cache");
	case VIOC_SETVV:	    return("Set	VV");
	case VIOC_HDB_ADD:	    return("HDB Add");
	case VIOC_HDB_DELETE:	    return("HDB Delete");
	case VIOC_HDB_MODIFY:	    return("HDB Modify");
	case VIOC_HDB_CLEAR:	    return("HDB Clear");
	case VIOC_HDB_LIST:	    return("HDB List");
	case VIOC_WAITFOREVER:	    return("Waitforever");
	case VIOC_HDB_WALK:	    return("HDB Walk");
	case VIOC_CLEARPRIORITIES:  return("Clear Priorities");
	case VIOC_GETPATH:          return("Get Path");
	case VIOC_COMPRESS:         return("Compress");
	case VIOC_UNCOMPRESS:       return("Uncompress");
	case VIOC_CHECKPOINTML:     return("Checkpoint Modify Log");
	case VIOC_PURGEML:          return("Purge Modify Log");
	case VIOC_BEGINRECORDING:   return("Begin Recording References");
	case VIOC_ENDRECORDING:     return("End Recording References");
	case VIOC_TRUNCATELOG:      return("Truncate Log");
	case VIOC_DISCONNECT:       return("Disconnect");
	case VIOC_RECONNECT:        return("Reconnect");
	case VIOC_SLOW:             return("Slow");
	case VIOC_GETPFID:          return("Get Parent Fid");
	case VIOC_BEGINML:          return("Begin Modify Logging");
	case VIOC_ENDML:            return("End Modify Logging");
        case VIOC_ENABLEASR:        return("Enable ASR");
	case VIOC_DISABLEASR:       return("Disable ASR");
        case VIOC_FLUSHASR:         return("Flush ASR");
	default:		    sprintf(buf, "%d", opcode); return(buf);
    }
}


char *VenusRetStr(int retcode) {
    static char	buf[12];    /* This is shaky. */

    if (retcode == 0) return("SUCCESS");
    if (retcode < 0) return(RPC2_ErrorMsg(retcode));
    if (retcode < sys_nerr) return((char *)sys_errlist[retcode]);
    if (retcode == ERETRY) return("Retry");
    if (retcode == EINCONS) return("Inconsistent");
    sprintf(buf, "%d", retcode); return(buf);
}


void VVPrint(FILE *fp, vv_t **vvp) {
    for (int i = 0; i < VSG_MEMBERS; i++)
	if (vvp[i]) {
	    fprintf(fp, "\t\t%d: ", i);
	    PrintVV(fp, vvp[i]);
	}
}


int binaryfloor(int n) {
    int m = 1;
    while (m < n) m *= 2;
    if (m > n) m /= 2;
    return(m);
}


void LogInit() {
    rename(LOGFILE, LOGFILE_OLD);

    logFile = fopen(LOGFILE, "w+");
    if (logFile == NULL)
	{ eprint("LogInit failed"); exit(-1); }
    LogInited = 1;
    LOG(0, ("Coda Venus, version %d.%d (%d)\n", 
	    VenusMajorVersion, VenusMinorVersion, RecovVersionNumber));

    struct timeval now;
    gettimeofday(&now, 0);
    LOG(0, ("LOGFILE initialized with LogLevel = %d at %s\n",
	    LogLevel, ctime((const long int *)&now.tv_sec)));
}


void DebugOn() {
    LogLevel = ((LogLevel == 0) ? 1 : LogLevel * 10);
    LOG(0, ("LogLevel is now %d.\n", LogLevel));
}


void DebugOff() {
    LogLevel = 0;
    LOG(0, ("LogLevel is now %d.\n", LogLevel));
}


void Terminate() {
    Choke("terminate signal received");
}


void DumpState() {
    if (!LogInited) return;

    char *argv[1];
    argv[0] = "all";
    VenusPrint(logFile, 1, argv);
    fflush(logFile);
}


void RusagePrint(int afd) {
    /* Unix rusage statistics. */
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    fdprint(afd, "Unix Rusage:\n");
    fdprint(afd, "\ttimes = (%u, %u), rss = (%u, %u, %u, %u)\n",
	     ru.ru_utime.tv_sec, ru.ru_stime.tv_sec,
	     ru.ru_maxrss, ru.ru_ixrss, ru.ru_idrss, ru.ru_isrss);
    fdprint(afd, "\tpage = (%u, %u), swap = (%u), block = (%u, %u)\n",
	     ru.ru_minflt, ru.ru_majflt, ru.ru_nswap, ru.ru_inblock, ru.ru_oublock);
    fdprint(afd, "\tmsg = (%u, %u), sig = (%u), csw = (%u, %u)\n",
	     ru.ru_msgsnd, ru.ru_msgrcv, ru.ru_nsignals, ru.ru_nvcsw, ru.ru_nivcsw);
extern unsigned etext;
extern unsigned edata;
extern unsigned end;
    fdprint(afd, "\tsegment sizes = (%#08x, %#08x, %#08x, %#08x)\n",
	     etext, edata - etext, end - edata, sbrk(0) - end);

#ifdef	__MACH__
    /* Mach statistics. */
    fdprint(afd, "Mach Rusage:\n");
    vm_task_t target_task = task_self();
/*
      task_info_data_t taskinfo;
      unsigned int taskinfoCnt = TASK_INFO_MAX;
      (void)task_info(target_task, TASK_BASIC_INFO, &taskinfo, &taskinfoCnt);
      thread_info_data_t threadinfo;
      unsigned int threadinfoCnt = THREAD_INFO_MAX;
      (void)thread_info(thread_self, THREAD_BASIC_INFO, &threadinfo, &threadinfoCnt);
      LOG(0, ("*** Task/Thread Info: (%u %u) (%u %u %u %u) ***\n",
	       taskinfo.virtual_size, taskinfo.resident_size,
	       threadinfo.user_time, threadinfo.system_time,
	       threadinfo.cpu_usage, threadinfo.sleep_time));
*/
    int region = 0;
    vm_address_t address = 0;
    vm_size_t totalsize = 0;
    for (;;) {
	vm_size_t size;
	vm_prot_t protection;
	vm_prot_t max_protection;
	vm_inherit_t inheritance;
	boolean_t shared;
	port_t object_name;
	vm_offset_t offset;

	kern_return_t ret = vm_region(target_task, &address, &size, &protection,
				      &max_protection, &inheritance, &shared,
				      &object_name, &offset);
	if (ret != KERN_SUCCESS) break;

	fdprint(afd, "\tregion %d = [%#08x, %#08x], [%d, %d, %d, %d, %d, %d]\n",
		region, address, size, protection, max_protection,
		inheritance, shared, object_name, offset);

	region++;
	address += size;
	totalsize += size;
    }
    fdprint(afd, "\ttotal VM allocated = %#08x\n", totalsize);
#endif	/* __MACH__ */

    fdprint(afd, "\n");
}


int VMUsage() {
#ifdef	__MACH__
    vm_task_t target_task = task_self();
    int region = 0;
    vm_address_t address = 0;
    vm_size_t totalsize = 0;
    for (;;) {
	vm_size_t size;
	vm_prot_t protection;
	vm_prot_t max_protection;
	vm_inherit_t inheritance;
	boolean_t shared;
	port_t object_name;
	vm_offset_t offset;

	kern_return_t ret = vm_region(target_task, &address, &size, &protection,
				      &max_protection, &inheritance, &shared,
				      &object_name, &offset);
	if (ret != KERN_SUCCESS) break;

	region++;
	address += size;
	totalsize += size;
    }
    return(totalsize);
#else	MACH
    return(0);
#endif	/* __MACH__ */
}


void VFSPrint(int afd) {
    fdprint(afd, "VFS Operations\n");
    fdprint(afd, " Operation                 Counts                    Times\n");
    for (int i = 0; i < NVFSOPS; i++)
	if (!STREQ(VFSStats.VFSOps[i].name, "No-Op")) {
	    VFSStat *t = &VFSStats.VFSOps[i];

	    double mean = (t->success > 0 ? (t->time / (double)t->success) : 0.0);
	    double stddev = (t->success > 1 ? sqrt(((double)t->success * t->time2 - t->time * t->time) / ((double)t->success * (double)(t->success - 1))) : 0.0);

	    fdprint(afd, "%-12s  :  %5d  [%5d %5d %5d]  :  %5.1f (%5.1f)\n",
		    t->name, t->success, t->retry, t->timeout, t->failure, mean, stddev);
	}
    fdprint(afd, "\n");
}


void RPCPrint(int afd) {
    /* Operation statistics. */
    fdprint(afd, "RPC Operations:\n");
    fdprint(afd, " Operation    \tGood  Bad   Time MGood  MBad MTime   RPCR MRPCR\n");
    for (int i = 1; i < srvOPARRAYSIZE; i++) {
	RPCOpStat *t = &RPCOpStats.RPCOps[i];
	fdprint(afd, "%-16s %5d %5d %5.1f %5d %5d %5.1f %5d %5d\n",
		t->name, t->good, t->bad, (t->time > 0 ? t->time / t->good : 0),
		t->Mgood, t->Mbad, (t->Mtime > 0 ? t->Mtime / t->Mgood : 0),
		t->rpc_retries, t->Mrpc_retries);
    }
    fdprint(afd, "\n");

    /* Communication statistics. */
    fdprint(afd, "RPC Packets:\n");
    RPCPktStatistics RPCPktStats;
    bzero(&RPCPktStats, (int)sizeof(RPCPktStatistics));
    GetCSS(&RPCPktStats);
    struct SStats *rsu = &RPCPktStats.RPC2_SStats_Uni;
    struct SStats *rsm = &RPCPktStats.RPC2_SStats_Multi;
    struct RStats *rru = &RPCPktStats.RPC2_RStats_Uni;
    struct RStats *rrm = &RPCPktStats.RPC2_RStats_Multi;
    fdprint(afd, "RPC2:\n");
    fdprint(afd, "   Sent:           Total        Retrys  Busies   Naks\n");
    fdprint(afd, "      Uni:    %5d : %-8d  %5d   %5d   %5d\n",
	     rsu->Total, rsu->Bytes, rsu->Retries, rsu->Busies, rsu->Naks);
    fdprint(afd, "      Multi:  %5d : %-8d  %5d   %5d   %5d\n",
	     rsm->Total, rsm->Bytes, rsm->Retries, 0, 0);
    fdprint(afd, "   Received:       Total          Replys       Reqs       Busies    Bogus    Naks\n");
    fdprint(afd, "      Uni:    %5d : %-8d  %5d : %-2d  %5d : %-2d  %5d : %-2d  %5d   %5d\n",
	     rru->Total, rru->Bytes, rru->GoodReplies, (rru->Replies - rru->GoodReplies),
	     rru->GoodRequests, (rru->Requests - rru->GoodRequests), rru->GoodBusies,
	     (rru->Busies - rru->GoodBusies), rru->Bogus, rru->Naks);
    fdprint(afd, "      Multi:  %5d : %-8d  %5d : %-2d  %5d : %-2d  %5d : %-2d  %5d   %5d\n",
	     rrm->Total, rrm->Bytes, 0, 0,
	     rrm->GoodRequests, (rrm->Requests - rrm->GoodRequests), 0, 0, 0, 0);
    struct sftpStats *msu = &RPCPktStats.SFTP_SStats_Uni;
    struct sftpStats *msm = &RPCPktStats.SFTP_SStats_Multi;
    struct sftpStats *mru = &RPCPktStats.SFTP_RStats_Uni;
    struct sftpStats *mrm = &RPCPktStats.SFTP_RStats_Multi;
    fdprint(afd, "SFTP:\n");
    fdprint(afd, "   Sent:           Total        Starts     Datas       Acks    Naks   Busies\n");
    fdprint(afd, "      Uni:    %5d : %-8d  %5d   %5d : %-4d  %5d   %5d   %5d\n",
	     msu->Total, msu->Bytes, msu->Starts, msu->Datas,
	     msu->DataRetries, msu->Acks, msu->Naks, msu->Busies);
    fdprint(afd, "      Multi:  %5d : %-8d  %5d   %5d : %-4d  %5d   %5d   %5d\n",
	     msm->Total, msm->Bytes, msm->Starts, msm->Datas,
	     msm->DataRetries, msm->Acks, msm->Naks, msm->Busies);
    fdprint(afd, "   Received:       Total        Starts     Datas       Acks    Naks   Busies\n");
    fdprint(afd, "      Uni:    %5d : %-8d  %5d   %5d : %-4d  %5d   %5d   %5d\n",
	     mru->Total, mru->Bytes, mru->Starts, mru->Datas,
	     mru->DataRetries, mru->Acks, mru->Naks, mru->Busies);
    fdprint(afd, "      Multi:  %5d : %-8d  %5d   %5d : %-4d  %5d   %5d   %5d\n",
	     mrm->Total, mrm->Bytes, mrm->Starts, mrm->Datas,
	     mrm->DataRetries, mrm->Acks, mrm->Naks, mrm->Busies);
    fdprint(afd, "\n");
}


void GetCSS(RPCPktStatistics *cs) {
    cs->RPC2_SStats_Uni = rpc2_Sent;
    cs->RPC2_SStats_Multi = rpc2_MSent;
    cs->RPC2_RStats_Uni = rpc2_Recvd;
    cs->RPC2_RStats_Multi = rpc2_MRecvd;
    cs->SFTP_SStats_Uni = sftp_Sent;
    cs->SFTP_SStats_Multi = sftp_MSent;
    cs->SFTP_RStats_Uni = sftp_Recvd;
    cs->SFTP_RStats_Multi = sftp_MRecvd;
 }


void SubCSSs(RPCPktStatistics *cs1, RPCPktStatistics *cs2) {
    cs1->RPC2_SStats_Uni.Total -= cs2->RPC2_SStats_Uni.Total;
    cs1->RPC2_SStats_Uni.Retries -= cs2->RPC2_SStats_Uni.Retries;
    cs1->RPC2_SStats_Uni.Multicasts -= cs2->RPC2_SStats_Uni.Multicasts;
    cs1->RPC2_SStats_Uni.Busies -= cs2->RPC2_SStats_Uni.Busies;
    cs1->RPC2_SStats_Uni.Naks -= cs2->RPC2_SStats_Uni.Naks;
    cs1->RPC2_SStats_Uni.Bytes -= cs2->RPC2_SStats_Uni.Bytes;

    cs1->RPC2_SStats_Multi.Total -= cs2->RPC2_SStats_Multi.Total;
    cs1->RPC2_SStats_Multi.Retries -= cs2->RPC2_SStats_Multi.Retries;
    cs1->RPC2_SStats_Multi.Multicasts -= cs2->RPC2_SStats_Multi.Multicasts;
    cs1->RPC2_SStats_Multi.Busies -= cs2->RPC2_SStats_Multi.Busies;
    cs1->RPC2_SStats_Multi.Naks -= cs2->RPC2_SStats_Multi.Naks;
    cs1->RPC2_SStats_Multi.Bytes -= cs2->RPC2_SStats_Multi.Bytes;

    cs1->RPC2_RStats_Uni.Total -= cs2->RPC2_RStats_Uni.Total;
    cs1->RPC2_RStats_Uni.Giant -= cs2->RPC2_RStats_Uni.Giant;
    cs1->RPC2_RStats_Uni.Replies -= cs2->RPC2_RStats_Uni.Replies;
    cs1->RPC2_RStats_Uni.Requests -= cs2->RPC2_RStats_Uni.Requests;
    cs1->RPC2_RStats_Uni.GoodReplies -= cs2->RPC2_RStats_Uni.GoodReplies;
    cs1->RPC2_RStats_Uni.GoodRequests -= cs2->RPC2_RStats_Uni.GoodRequests;
    cs1->RPC2_RStats_Uni.Multicasts -= cs2->RPC2_RStats_Uni.Multicasts;
    cs1->RPC2_RStats_Uni.GoodMulticasts -= cs2->RPC2_RStats_Uni.GoodMulticasts;
    cs1->RPC2_RStats_Uni.Busies -= cs2->RPC2_RStats_Uni.Busies;
    cs1->RPC2_RStats_Uni.GoodBusies -= cs2->RPC2_RStats_Uni.GoodBusies;
    cs1->RPC2_RStats_Uni.Bogus -= cs2->RPC2_RStats_Uni.Bogus;
    cs1->RPC2_RStats_Uni.Naks -= cs2->RPC2_RStats_Uni.Naks;
    cs1->RPC2_RStats_Uni.Bytes -= cs2->RPC2_RStats_Uni.Bytes;

    cs1->RPC2_RStats_Multi.Total -= cs2->RPC2_RStats_Multi.Total;
    cs1->RPC2_RStats_Multi.Giant -= cs2->RPC2_RStats_Multi.Giant;
    cs1->RPC2_RStats_Multi.Replies -= cs2->RPC2_RStats_Multi.Replies;
    cs1->RPC2_RStats_Multi.Requests -= cs2->RPC2_RStats_Multi.Requests;
    cs1->RPC2_RStats_Multi.GoodReplies -= cs2->RPC2_RStats_Multi.GoodReplies;
    cs1->RPC2_RStats_Multi.GoodRequests -= cs2->RPC2_RStats_Multi.GoodRequests;
    cs1->RPC2_RStats_Multi.Multicasts -= cs2->RPC2_RStats_Multi.Multicasts;
    cs1->RPC2_RStats_Multi.GoodMulticasts -= cs2->RPC2_RStats_Multi.GoodMulticasts;
    cs1->RPC2_RStats_Multi.Busies -= cs2->RPC2_RStats_Multi.Busies;
    cs1->RPC2_RStats_Multi.GoodBusies -= cs2->RPC2_RStats_Multi.GoodBusies;
    cs1->RPC2_RStats_Multi.Bogus -= cs2->RPC2_RStats_Multi.Bogus;
    cs1->RPC2_RStats_Multi.Naks -= cs2->RPC2_RStats_Multi.Naks;
    cs1->RPC2_RStats_Multi.Bytes -= cs2->RPC2_RStats_Multi.Bytes;

    cs1->SFTP_SStats_Uni.Total -= cs2->SFTP_SStats_Uni.Total;
    cs1->SFTP_SStats_Uni.Starts -= cs2->SFTP_SStats_Uni.Starts;
    cs1->SFTP_SStats_Uni.Datas -= cs2->SFTP_SStats_Uni.Datas;
    cs1->SFTP_SStats_Uni.DataRetries -= cs2->SFTP_SStats_Uni.DataRetries;
    cs1->SFTP_SStats_Uni.Acks -= cs2->SFTP_SStats_Uni.Acks;
    cs1->SFTP_SStats_Uni.Naks -= cs2->SFTP_SStats_Uni.Naks;
    cs1->SFTP_SStats_Uni.Busies -= cs2->SFTP_SStats_Uni.Busies;
    cs1->SFTP_SStats_Uni.Bytes -= cs2->SFTP_SStats_Uni.Bytes;

    cs1->SFTP_SStats_Multi.Total -= cs2->SFTP_SStats_Multi.Total;
    cs1->SFTP_SStats_Multi.Starts -= cs2->SFTP_SStats_Multi.Starts;
    cs1->SFTP_SStats_Multi.Datas -= cs2->SFTP_SStats_Multi.Datas;
    cs1->SFTP_SStats_Multi.DataRetries -= cs2->SFTP_SStats_Multi.DataRetries;
    cs1->SFTP_SStats_Multi.Acks -= cs2->SFTP_SStats_Multi.Acks;
    cs1->SFTP_SStats_Multi.Naks -= cs2->SFTP_SStats_Multi.Naks;
    cs1->SFTP_SStats_Multi.Busies -= cs2->SFTP_SStats_Multi.Busies;
    cs1->SFTP_SStats_Multi.Bytes -= cs2->SFTP_SStats_Multi.Bytes;

    cs1->SFTP_RStats_Uni.Total -= cs2->SFTP_RStats_Uni.Total;
    cs1->SFTP_RStats_Uni.Starts -= cs2->SFTP_RStats_Uni.Starts;
    cs1->SFTP_RStats_Uni.Datas -= cs2->SFTP_RStats_Uni.Datas;
    cs1->SFTP_RStats_Uni.DataRetries -= cs2->SFTP_RStats_Uni.DataRetries;
    cs1->SFTP_RStats_Uni.Acks -= cs2->SFTP_RStats_Uni.Acks;
    cs1->SFTP_RStats_Uni.Naks -= cs2->SFTP_RStats_Uni.Naks;
    cs1->SFTP_RStats_Uni.Busies -= cs2->SFTP_RStats_Uni.Busies;
    cs1->SFTP_RStats_Uni.Bytes -= cs2->SFTP_RStats_Uni.Bytes;

    cs1->SFTP_RStats_Multi.Total -= cs2->SFTP_RStats_Multi.Total;
    cs1->SFTP_RStats_Multi.Starts -= cs2->SFTP_RStats_Multi.Starts;
    cs1->SFTP_RStats_Multi.Datas -= cs2->SFTP_RStats_Multi.Datas;
    cs1->SFTP_RStats_Multi.DataRetries -= cs2->SFTP_RStats_Multi.DataRetries;
    cs1->SFTP_RStats_Multi.Acks -= cs2->SFTP_RStats_Multi.Acks;
    cs1->SFTP_RStats_Multi.Naks -= cs2->SFTP_RStats_Multi.Naks;
    cs1->SFTP_RStats_Multi.Busies -= cs2->SFTP_RStats_Multi.Busies;
    cs1->SFTP_RStats_Multi.Bytes -= cs2->SFTP_RStats_Multi.Bytes;
}


void MallocPrint(int fd) {
    int dfd = ::dup(fd);
    ASSERT(dfd >= 0);
    LOG(/*100*/0, ("MallocPrint: fd = %d, dfd = %d\n", fd, dfd));
    FILE *fp = fdopen(dfd, "a");
    ASSERT(fp != NULL);
	
    MallocStats("", fp);
    plumber(fp);
    /* the new plumber is broken */
/*    newPlumber(fp); */

/*    ASSERT(fclose(fp) != EOF);*/
    if (fclose(fp) == EOF)
	LOG(0, ("MallocPrint: fclose failed!\n"));

#ifdef	VENUSDEBUG
    fdprint(fd, "connent: %d, %d, %d\n", connent::allocs, connent::deallocs,
	     (connent::allocs - connent::deallocs) * sizeof(connent));
    fdprint(fd, "srvent: %d, %d, %d\n", srvent::allocs, srvent::deallocs,
	     (srvent::allocs - srvent::deallocs) * sizeof(srvent));
    fdprint(fd, "mgrpent: %d, %d, %d\n", mgrpent::allocs, mgrpent::deallocs,
	     (mgrpent::allocs - mgrpent::deallocs) * sizeof(mgrpent));
    fdprint(fd, "binding: %d, %d, %d\n", binding::allocs, binding::deallocs,
	     (binding::allocs - binding::deallocs) * sizeof(binding));
    fdprint(fd, "namectxt: %d, %d, %d\n", NameCtxt_allocs, NameCtxt_deallocs,
	     (NameCtxt_allocs - NameCtxt_deallocs) * sizeof(namectxt));
    fdprint(fd, "resent: %d, %d, %d\n", resent::allocs, resent::deallocs,
	     (resent::allocs - resent::deallocs) * sizeof(resent));
    fdprint(fd, "cop2ent: %d, %d, %d\n", cop2ent::allocs, cop2ent::deallocs,
	     (cop2ent::allocs - cop2ent::deallocs) * sizeof(cop2ent));
    fdprint(fd, "vsr: %d, %d, %d\n", vsr::allocs, vsr::deallocs,
	     (vsr::allocs - vsr::deallocs) * sizeof(vsr));
    fdprint(fd, "msgent: %d, %d, %d\n", msgent::allocs, msgent::deallocs,
	     (msgent::allocs - msgent::deallocs) * sizeof(msgent));
    fdprint(fd, "vnode: %d, %d, %d\n", vnode_allocs, vnode_deallocs,
	     (vnode_allocs - vnode_deallocs) * sizeof(struct cnode));
    VmonPrint(fd);
#endif	VENUSDEBUG
}


void StatsInit() {
    int i;

    bzero(&VFSStats, (int)sizeof(VFSStatistics));
    for (i = 0; i < NVFSOPS; i++)
	strcpy(VFSStats.VFSOps[i].name, VFSOpsNameTemplate[i]);

    bzero(&RPCOpStats, (int)sizeof(RPCOpStatistics));
    for (i = 0; i < srvOPARRAYSIZE; i++)
	strncpy(RPCOpStats.RPCOps[i].name, (char *) srv_CallCount[i].name+4, 
		RPCOPSTATNAMELEN);
}


/* The logic below seems warped, do I understand that we need 
   a three processor machine with a mips, sparc and a sun??
   pjb
   */
void ProfInit() {
#ifndef	__linux__
#if !defined(mips) && !defined(sparc) && !defined(sun4)
    moncontrol(0);   /* not available on mips, sun/sparc */
#endif
#endif
    if (ProfBoot) ToggleProfiling();
}


void ToggleProfiling() {
    Profiling = 1 - Profiling;
#ifndef	__linux__
#if !defined(mips) && !defined(sparc) && !defined(sun4)
    moncontrol(Profiling);
#endif
#endif

    LOG(0, ("Profiling is now %s\n", Profiling ? "on" : "off"));
}

void ToggleMallocTrace() {
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

void rds_printer(char *fmt ...) {
  LOG(0, (fmt));
}

void SwapLog() {
    struct timeval now;
    gettimeofday(&now, 0);
    LOG(0, ("Moving %s to %s at %s\n",
	     LOGFILE, LOGFILE_OLD, ctime((const long int *)&now.tv_sec)));
    fflush(logFile);

    if (rename(LOGFILE, LOGFILE_OLD) < 0) {
	eprint("rename(%s, %s) failed (%d)\n",
	       LOGFILE, LOGFILE_OLD, errno);
	return;
    }

    freopen(LOGFILE, "a+", logFile);
    LOG(0, ("New LOGFILE started at %s", ctime((const long int *)&now.tv_sec)));
}


char *lvlstr(LockLevel level) {
    switch(level) {
	case NL:
	    return("NL");

	case RD:
	    return("RD");

	case SH:
	    return("SH");

	case WR:
	    return("WR");

	default:
	    Choke("Illegal lock level!");
	    return (0); /* dummy to pacify g++ */
    }
}


int GetTime(long *secs, long *usecs) {
    LOG(100, ("GetTime: \n"));

    int code = 0;

    connent *c = 0;
    code = GetAdmConn(&c);
    if (code != 0) goto Exit;

    /* Make the RPC call. */
    MarinerLog("fetch::GetTime (%#x)\n", c->Host);
    UNI_START_MESSAGE(ViceGetTime_OP);
    code = (int) ViceGetTime(c->connid, (RPC2_Unsigned *)secs, (RPC2_Integer *)usecs);
    UNI_END_MESSAGE(ViceGetTime_OP);
    MarinerLog("fetch::gettime done\n");
    code = c->CheckResult(code, 0);
    UNI_RECORD_STATS(ViceGetTime_OP);

Exit:
    PutConn(&c);
    return(0);
}


long Vtime() {
    if (Simulating)
	return(SimTime);

    return(::time(0));
}


/* 
 * compares fids.  Assumes that the fids are in the same volume.
 */
int Fid_Compare(ViceFid *fid1, ViceFid *fid2) {
    if (((fid1->Vnode) < (fid2->Vnode)) ||
	((fid1->Vnode == fid2->Vnode) && ((fid1->Unique) < (fid2->Unique))))
	    return(-1);

    if (((fid1->Vnode) > (fid2->Vnode)) ||
	((fid1->Vnode == fid2->Vnode) && ((fid1->Unique) > (fid2->Unique))))
	    return(1);

    return(0);
}


/* 
 * compares fids embedded in a ViceFidAndVersionVector. 
 * assumes that the fids are in the same volume.
 */
int FAV_Compare(ViceFidAndVV *fav1, ViceFidAndVV *fav2) {
    if (((fav1->Fid.Vnode) < (fav2->Fid.Vnode)) ||
	((fav1->Fid.Vnode == fav2->Fid.Vnode) && ((fav1->Fid.Unique) < (fav2->Fid.Unique))))
	    return(-1);

    if (((fav1->Fid.Vnode) > (fav2->Fid.Vnode)) ||
	((fav1->Fid.Vnode == fav2->Fid.Vnode) && ((fav1->Fid.Unique) > (fav2->Fid.Unique))))
	    return(1);

    return(0);  /* this shouldn't happen */
}

