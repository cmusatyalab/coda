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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/vice/smon.cc,v 4.1 1997/01/08 21:51:58 rvb Exp $";
#endif /*_BLURB_*/





/*
 *
 * Implementation of the Server Monitor.
 *
 */


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <sys/time.h>
#ifdef __MACH__
#include <sys/dk.h>
#endif
#include <netinet/in.h>
#include <errno.h>
#include <nlist.h>
/* nlist.h defines this function but it isnt getting included because it is
   guarded by an ifdef of CMU which isnt getting defined.  XXXXX pkumar 6/13/95 */ 
extern int nlist(const char*, struct nlist[]);

#include <libc.h>
#include <lwp.h>
#include <rpc2.h>
#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>
#include <olist.h>
#include "mond.h"
#include "vice.h"
#include <callback.h>
#include <res.h>
#include <logalloc.h>
#include <resstats.h>
#include <voldump.h>

/* *****  Private constants  ***** */

#define	DFLT_MONDHOST	"barber.coda.cs.cmu.edu"
#define	DFLT_MONDPORTAL	1356

PRIVATE const int SmonMaxDataSize = 1024 * 1024;
PRIVATE const int SmonBindInterval = 300;
PRIVATE const int TIMERINTERVAL = 3600;
PRIVATE const int callReportInterval = 4*60*60;
PRIVATE const int rvmresReportInterval = 4*60*60;

/* Need this for the definition of rvmrese */

PRIVATE RPC2_Handle SmonHandle = 0;

/* ***** Private types ***** */

PRIVATE SmonViceId MyViceId;

struct rvmrese : public olink {
    SmonViceId Vice;
    RPC2_Unsigned Time;
    RPC2_Unsigned VolID;
    FileResStats FileRes;
    DirResStats DirRes;
    HistoElem LogSizeHisto[SHIPHISTOSIZE];
    HistoElem LogMaxHisto[NENTRIESHISTOSIZE];
    ResConflictStats Conflicts;
    HistoElem SuccHierHist[DEPTHSIZE];
    HistoElem FailHierHist[DEPTHSIZE];
    ResLogStats ResLog;
    HistoElem VarLogHisto[MAXSIZES];
    HistoElem LogSize[SIZEBUCKETS];
    
    void Init(RPC2_Unsigned time, resstats *sp)
    {
	Vice = MyViceId;
	Time = time;
	VolID = sp->vid;

	/* fileresstats */
	FileRes.Resolves = sp->file.file_nresolves;
	FileRes.NumSucc = sp->file.file_nsucc;
	FileRes.NumConf = sp->file.file_conf;
	FileRes.RuntForce = sp->file.file_runtforce;
	FileRes.WeakEq = sp->file.file_we;
	FileRes.NumReg = sp->file.file_reg;
	FileRes.UsrResolver = sp->file.file_userresolver;
	FileRes.SuccUsrResolver = sp->file.file_succuserresolve;
	FileRes.PartialVSG = sp->file.file_incvsg;

	/* dirresstats */
	DirRes.Resolves = sp->dir.dir_nresolves;
	DirRes.NumSucc = sp->dir.dir_succ;
	DirRes.NumConf = sp->dir.dir_conf;
	DirRes.NumNoWork = sp->dir.dir_nowork;
	DirRes.Problems = sp->dir.dir_problems;
	DirRes.PartialVSG = sp->dir.dir_incvsg;
	for (int i=0; i<SHIPHISTOSIZE; i++)
	    LogSizeHisto[i].bucket = sp->dir.logshipstats.totalsize[i];
	for (i=0; i<NENTRIESHISTOSIZE; i++)
	    LogMaxHisto[i].bucket = sp->dir.logshipstats.maxentries[i];

	/* conflictstats */
	Conflicts.NameName = sp->conf.nn;
	Conflicts.RemoveUpdate = sp->conf.ru;
	Conflicts.UpdateUpdate = sp->conf.uu;
	Conflicts.Rename = sp->conf.mv;
	Conflicts.LogWrap = sp->conf.wrap;
	Conflicts.Other = sp->conf.other;

	/* hierarchystats */
	for (i=0; i<DEPTHSIZE; i++) {
	    SuccHierHist[i].bucket = sp->hstats.succres[i];
	    FailHierHist[i].bucket = sp->hstats.unsuccres[i];
	}

	/* logstats */
	ResLog.NumWraps = sp->lstats.nwraps;
	ResLog.NumAdmGrows = sp->lstats.nadmgrows;
	ResLog.NumVAllocs = sp->lstats.vdist.vallocs;
	ResLog.NumVFrees = sp->lstats.vdist.vfrees;
	ResLog.Highest = sp->lstats.lsize.highest;
	for (i=0; i<MAXSIZES; i++)
	    VarLogHisto[i].bucket = sp->lstats.vdist.bucket[i];
	for (i=0; i<SIZEBUCKETS; i++)
	    LogSize[i].bucket = sp->lstats.lsize.bucket[i];

	/* finis! */
    }
    long Report() 
    { return SmonReportRVMResStats (SmonHandle, &Vice, Time, VolID, 
				    &FileRes, &DirRes, SHIPHISTOSIZE,
				    LogSizeHisto, NENTRIESHISTOSIZE,
				    LogMaxHisto, &Conflicts,
				    DEPTHSIZE, SuccHierHist,
				    DEPTHSIZE, FailHierHist,
				    &ResLog, MAXSIZES, VarLogHisto,
				    SIZEBUCKETS, LogSize); }
};

/* OverflowEvent Entry */
struct smoe {
    SmonViceId Vice;
    RPC2_Unsigned Time;
    RPC2_Unsigned StartTime;
    RPC2_Unsigned EndTime;
    RPC2_Integer Count;

    void Init(RPC2_Unsigned time, RPC2_Unsigned starttime, 
	      RPC2_Unsigned endtime, RPC2_Integer count) {
	Vice = MyViceId;
	Time = time;
	StartTime = starttime;
	EndTime = endtime;
	Count = count;
    }
};


/* Raw Statistic Entry. */
PRIVATE struct nlist RawStats[] = 
{
#define CPTIME	0
    {
	"_cp_time"
    },
#define BOOT	1
    {

	"_boottime"
    },
#define DISK	2
    {

	"_dk_xfer"
    },
#define HZ	3
    {

	"_hz"
    },
#define PHZ	4
    {

	"_phz"
    },
    {
	0
    },
};

/* ***** Private variables  ***** */

PRIVATE int SmonEnabled = 1;
PRIVATE int SmonInited = 0;
PRIVATE smoe SOE;
PRIVATE olist *RVMResList = 0;
PRIVATE unsigned long LastSmonBindAttempt = 0;
PRIVATE SmonStatistics stats;

/* ***** Private routines  ***** */

PRIVATE void SmonNoteOverflow(RPC2_Unsigned);
PRIVATE	void CheckCallStat();		    /* Daemon Report Entries */
PRIVATE void CheckRVMResStat();
PRIVATE int ValidateSmonHandle();
PRIVATE long CheckSmonResult(long);
PRIVATE int GetRawStatistics(SmonStatistics *);


/*  *****  External variables  *****  */

char *SmonHost = DFLT_MONDHOST;	/* may be overridden from command line */

int SmonPortal = DFLT_MONDPORTAL;	/* may be overridden from command line */	    
/*  *****  Smon  *****  */

void SmonInit()
{
    MyViceId.IPAddress = gethostid();
    MyViceId.BirthTime = ::time(0);
    SOE.Init(0, 0, 0, 0);
    RVMResList = new olist;

    SmonInited = 1;
}



PRIVATE void SmonNoteOverflow(RPC2_Unsigned time) {
    if (SOE.Count++ == 0)
	SOE.StartTime = time;
}

/*
** we really only need to send data if the mond is running,
** enqueuing is a waste of resources, since the new result will
** be wiped out by the old result anyway.
*/

PRIVATE void CheckCallStat() {
    static unsigned long last_time = 0;
    
    if (!SmonEnabled || !SmonInited 
	|| !ValidateSmonHandle()) return;
    unsigned long curr_time = ::time(0);
    if (curr_time - last_time > callReportInterval) {
	last_time = curr_time;
	GetRawStatistics(&stats);
	long code = SmonReportCallEvent(SmonHandle, &MyViceId, curr_time,
					cbOPARRAYSIZE, cb_CallCount, 
					resolutionOPARRAYSIZE, resolution_CallCount,
					mondOPARRAYSIZE, mond_CallCount,
					volDumpOPARRAYSIZE, volDump_CallCount,
					resolutionOPARRAYSIZE, resolution_MultiCall,
					&stats);
	code = CheckSmonResult(code);
    }

}


PRIVATE void CheckRVMResStat()
{
    static unsigned long last_time = 0; 

    LogMsg(0,SrvDebugLevel,stdout,"Entered CheckRVMResStat");
    /* if we can't send anything, no sense queueing things */
    if (!SmonEnabled || !SmonInited 
	|| !ValidateSmonHandle()) return;
    
    /* if there are any old ones to send, try to send them first */
    if (RVMResList->count() >0) {
	LogMsg(0,SrvDebugLevel,stdout,"Sending some RVMResEvents");
	struct rvmrese *re =0;
	while(re = (struct rvmrese *)RVMResList->first()) {
	    LogMsg(10, SrvDebugLevel, stdout,
		   "Sending an RVMRes event, volume = 0x%lx time = %d",
		   re->Time, re->VolID);
	    if (CheckSmonResult(re->Report()))
		break;
	    if (RVMResList->remove(re) != re)
		assert(0);
	    delete re;
	}
    /* we don't enqueue anything if there was already something on the list */
    } else {
	unsigned long curr_time = ::time(0);
	if (curr_time - last_time > rvmresReportInterval) {
	    last_time = curr_time;
	    LogMsg(0,SrvDebugLevel,stdout,"Enqueueing some RVMResEvents");
	    olist_iterator next(ResStatsList);
	    resstats *s;
	    /* We don't need to check the size -- we know that only
	       one set of stats will exist in the queue at any one time */
	    while (s=(struct resstats*)next()) {
		struct rvmrese *e = new(struct rvmrese);
		s->precollect();
		e->Init(curr_time,s);
		s->postcollect();
		RVMResList->append(e);
	    }
	}
    }
}


PRIVATE void CheckSOE() 
{
    if (SOE.Count > 0) {
	if (!SmonEnabled || !SmonInited) return;

	unsigned long curr_time = ::time(0);

	SOE.EndTime = curr_time;
	long code = SmonReportOverflow(SmonHandle, &SOE.Vice, curr_time,
				      SOE.StartTime, SOE.EndTime, SOE.Count);
	code = CheckSmonResult(code);
	if (code != 0) return;

	SOE.Init(0, 0, 0, 0);
    }
}


PRIVATE int ValidateSmonHandle()
{
    RPC2_HostIdent hid;
    RPC2_PortalIdent pid;
    RPC2_SubsysIdent sid;
    RPC2_BindParms bp;

    if (SmonHandle != 0) return(1);

    long curr_time = ::time(0);

    /* Try to bind unless the most recent attempt was within SmonBindInterval seconds. */
    if (curr_time - LastSmonBindAttempt < SmonBindInterval) return(0);

    /* Attempt the bind. */
    LastSmonBindAttempt = curr_time;

    hid.Tag = RPC2_HOSTBYNAME;
    strcpy(hid.Value.Name, SmonHost);

    pid.Tag = RPC2_PORTALBYINETNUMBER;
    pid.Value.InetPortNumber = htons(SmonPortal);

    sid.Tag = RPC2_SUBSYSBYID;
    sid.Value.SubsysId = MondSubsysId;

    bp.SecurityLevel = RPC2_OPENKIMONO;
    bp.EncryptionType = NULL;
    bp.SideEffectType = NULL;
    bp.ClientIdent = NULL;
    bp.SharedSecret = NULL;

    long code = RPC2_NewBinding(&hid, &pid, &sid, &bp, &SmonHandle);

    LogMsg(1, SrvDebugLevel, stdout, "ValidateSmonHandle: bind to [ %s, %d, %d ] returned (%d, %d)",
	     SmonHost, SmonPortal, MondSubsysId, code, SmonHandle);
    if (code != 0) {
	SmonHandle = 0;
	return(0);
    }

    /* Successful bind. */

    LogMsg(0, SrvDebugLevel, stdout, "Trying to establish a connection");
    code = (int) MondEstablishConn(SmonHandle,MOND_CURRENT_VERSION,
				   MOND_VICE_CLIENT,0,(SpareEntry*)NULL);
    LogMsg(0, SrvDebugLevel, stdout, "Connection establishment returned %d",code);
    if (code != MOND_OK && code != MOND_OLDVERSION
	&& code != MOND_CONNECTED) {
	LogMsg(0, SrvDebugLevel, stdout,
	       "ValidateSmonHandle: MondEstablishConn failed with return (%d)",
		code);
	RPC2_Unbind(SmonHandle);
	SmonHandle = 0;
	return(0);
    }
    if (code == MOND_OLDVERSION)
	LogMsg(0, SrvDebugLevel, stdout, "ValidateSmonHandle: MondEstablishConn recieved MOND_OLDVERSION");

    return(1);
}


PRIVATE long CheckSmonResult(long code)
{
    if (code == 0) return(0);

    LogMsg(1, SrvDebugLevel, stdout, "CheckSmonResult: failure (%d)", code);

    RPC2_Unbind(SmonHandle);
    SmonHandle = 0;

    return(ETIMEDOUT);
}


PRIVATE int GetRawStatistics(SmonStatistics *stats)
{
#ifdef __MACH__
    static	int	kmem = 0;
    static      int     hertz = 0;
    int		i;
    long	busy[CPUSTATES];
    long	xfer[DK_NDRIVE];
    struct	timeval bootTime;

    if(kmem == -1) {
	return -1;
    }

    if(kmem == 0) {
	nlist("/vmunix", RawStats);
	if(RawStats[0].n_type == 0) {
	    kmem = -1;
	    return -1;
	}
	kmem = open("/dev/kmem", 0, 0);
	if (kmem <= 0) {
	    kmem = -1;
	    return -1;
	}
	/* if phz is non-zero, that is the clock rate
	   for statistics gathering used by the kernel.
	   Otherwise, use hz
	 */
	lseek(kmem, (long)RawStats[PHZ].n_value,0);
	read(kmem, (char *)&hertz, sizeof(hertz));
	if (hertz == 0) {
	    lseek(kmem, (long)RawStats[HZ].n_value,0);
	    read(kmem, (char *)&hertz, sizeof(hertz));
	}
	/* XXX - if hertz is still 0, use the BSD default of 100 */
	if (hertz == 0)
	    hertz = 100;
    }

    stats->UserCPU = 0;
    stats->SystemCPU = 0;
    stats->IdleCPU = 0;
    stats->BootTime = 0;
    stats->TotalIO = 0;

    lseek(kmem, (long)RawStats[CPTIME].n_value,0);
    read(kmem, (char *)busy, sizeof(busy));
    stats->SystemCPU = busy[CP_SYS]/hertz;
    stats->UserCPU = (busy[CP_USER] + busy[CP_NICE])/hertz;
    stats->IdleCPU = busy[CP_IDLE]/hertz;
    lseek(kmem, (long)RawStats[BOOT].n_value,0);
    read(kmem, (char *)&bootTime, sizeof(bootTime));
    stats->BootTime = bootTime.tv_sec;
    lseek(kmem, (long)RawStats[DISK].n_value,0);
    read(kmem, (char *)xfer, sizeof(xfer));
    for (i = 0; i < DK_NDRIVE; i++) {
	stats->TotalIO += xfer[i];
    }
#else
    return 0;
#endif
}


// *********************************************************************
// 			Smon Daemon Definition
// *********************************************************************


PRIVATE const int SmonDaemonInterval = TIMERINTERVAL;

void SmonDaemon()
{
    struct timeval time;
    SmonInit();

    time.tv_sec = SmonDaemonInterval;
    time.tv_usec = 0;

    while(1){
	LogMsg(0,SrvDebugLevel,stdout,"Starting SmonDaemon timer");
	if (IOMGR_Select(0, 0, 0, 0, &time) == 0){

	    LogMsg(0,SrvDebugLevel,stdout,"SmonDaemon timer expired");
	    CheckCallStat();
	    CheckRVMResStat();
	    CheckSOE();

            if (time.tv_sec != SmonDaemonInterval)
		time.tv_sec = SmonDaemonInterval;

	}
    }
}

