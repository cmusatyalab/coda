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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/venus/Attic/simulate.cc,v 4.2 1997/02/26 16:03:24 rvb Exp $";
#endif /*_BLURB_*/



/* IMPORTANT: README (Satya, 8/22/96)

   This code has not been tested on BSD44.  
   Main change has been to eliminate use of VFMT, which does
   not exist on BSD44.
*/



/*
 *
 *    Implementation of the Venus Simulation package.
 *
 * All network, disk, and kernel reading activity is stubbed out.
 * All updates are logged, to varying degrees of completeness.
 * For example, the target of a symbolic link is not included
 * in the log record, even though it would be under normal operation.
 * 
 * The simulator assumes that all status and data is valid (no cache
 * misses).  If an object unknown to Venus is referenced, the simulator  
 * creates it.
 * 
 * The simulation output file contains statistics on CML usage and
 * log record cancellations.  Note that not all cancellations go
 * through cmlent::cancel.  Removes and rmdirs that cancel creates
 * also "cancel" themselves (never create records).  These records
 * are included the cancellation statistics.
 *
 * The simulator also generates a skeleton and replay file.  The
 * replay file contains the final set of updates (optimized).  The
 * skeleton file contains commands that create the scaffolding
 * necessary for the replay file to be executed.
 */


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <sys/file.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#ifdef __MACH__
#include <sysent.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif

#ifdef __BSD44__
#include <dirent.h> /* to get defn of MAXNAMLEN */
#endif /* __BSD44 */

#include <libcs.h>

#ifdef __cplusplus
}
#endif __cplusplus

/* interfaces */
#include <vice.h>

/* from dir */
#include <coda_dir.h>



/* from venus */
#include "fso.h"
#include "simulate.h"
#include "venus.private.h"
#include "vproc.h"
#include "worker.h"


/* This should be in some header! */
#define	FID_ASSERT(fid, ex)\
{\
    if (!(ex)) {\
	dprint("Fid = (%x.%x.%x)\n", (fid).Volume, (fid).Vnode, (fid).Unique);\
	Choke("Assertion failed: file \"%s\", line %d\n", __FILE__, __LINE__);\
    }\
}


/* Define wrapper macro to cope with absence of VFMT on BSD44 */
#ifdef __MACH__
#define SIM_VTTOFT(v) VTTOFT((v) & VFMT)
#endif /* __MACH__ */

#if defined(__linux__) || defined(__BSD44__)
#define SIM_VTTOFT(v)  VTTOFT((v))
#endif /* __linux__ ||__BSD44__ */


const int SimulatorStackSize = 131072;
const vuid_t SIMUID = 0;
const unsigned short SIMMODE = 0644;
const int SimReportInterval = 900;

int Simulating = 0;
unsigned long SimTime = 0;
long LastReport = 0;
char *SimInfilename = 0;
char *SimOutfilename = 0;
char *SimFilterfilename = 0;
char *SimAtSys = 0;
char *SimTmpFid = 0;
unsigned long SimStartTime = (unsigned long)-1;
unsigned long SimEndTime = (unsigned long)-1;
dfs_header_t *recPtr = 0;
char recType;		/* file system type (ITYPE_{UFS,NFS,AFS,CFS}) of record recPtr */
ViceFid tmpFid;
char excludeTmp = 0;

PRIVATE FILE *SimInfile = 0;
PRIVATE FILE *SimOutfile = 0;
PRIVATE simulator *Simulator = 0;

PRIVATE void InferNameRemoval(long, char *, long, long);
PRIVATE void XlateFid(generic_fid_t *, ViceFid *, ViceDataType);
PRIVATE void GetComponent(char *, char *);
PRIVATE void UpdateFsoName(char **, char *);
PRIVATE void ParseTmpFid();
PRIVATE ViceDataType VTTOFT(unsigned short);


void SimInit() {
    if (!Simulating) return;

    /* Open IN, OUT and FILTER files. */
    if ((SimInfile = Trace_Open(SimInfilename)) == NULL)
	Choke("SimInit: cannot open infile (%s)", SimInfilename);
    if ((SimOutfile = fopen(SimOutfilename, "w+")) == NULL)
	Choke("SimInit: cannot open outfile (%s)", SimOutfilename);
    if (Trace_SetFilter(SimInfile, SimFilterfilename))
	Choke("SimInit: cannot set filterfile (%s)", SimFilterfilename);

    /* Get first record and initialize "simulated time". */
    recPtr = Trace_GetRecord(SimInfile);
    if (recPtr == NULL) SimExit();
    SimTime = recPtr->time.tv_sec;
    {
	FILE *tfp = fopen(SimFilterfilename, "r");
	if (tfp == NULL)
	    Choke("SimInit: cannot open filterfile (%s)", SimFilterfilename);
	char buf[1024];
	while (fgets(buf, 1024, tfp) != NULL) {
	    if (sscanf(buf, "start ") == 1)
		SimStartTime = atot(buf + 6);
	    else if (sscanf(buf, "end ") == 1)
		SimEndTime = atot(buf + 4);
	}
	fclose(tfp);
    }
    if (SimStartTime == -1)
	SimStartTime = SimTime;
    if (SimEndTime == -1)
	SimEndTime = 0xFFFFFFFF;
    if (SimStartTime >= SimEndTime)
	Choke("SimInit: bogus times (%d, %d)", SimStartTime, SimEndTime);
    fprintf(stderr, "SimTime = %d, <start, end> = <%d, %d>\n",
	     SimTime, SimStartTime, SimEndTime);
    fflush(stderr);

    LastReport = SimStartTime;

    /* Initialize tmp fid */
    ParseTmpFid();

    /* Create simulator thread. */
    Simulator = new simulator;
}


void SimExit() {
    /* Dump statistics to stderr. */
    int fd = fileno(stderr);
    FSDB->print(fd, 1);
    VFSPrint(fd);

    /* output final simulation statistics */
    SimReport();

    /* Close trace files. */
    Trace_Close(SimInfile);
    (void)fclose(SimOutfile);

    /* Generate replay log files. */
    FILE *sfp = fopen("skeleton.out", "w");
    if (sfp == NULL)
	Choke("Simulate: cannot open skeleton file");
    FILE *rfp = fopen("replay.out", "w");
    if (rfp == NULL)
	Choke("Simulate: cannot open replay file");
    vol_iterator next;
    volent *v;
    while (v = next())
	Simulator->OutputSimulationFiles(v, sfp, rfp);
    (void)fclose(sfp);
    (void)fclose(rfp);

    if (LogLevel > 0) DumpState();
    exit(0);
}

/* report log high water marks */
void SimReport() {
    ASSERT(Simulating);

    LastReport += SimReportInterval;    

    /* Compute fsobj counts and bytes in use by vnode type. */
    int FileCount = 0;
    float FileBytes = 0.0;
    int DirCount = 0;
    float DirBytes = 0.0;
    int SymlinkCount = 0;
    float SymlinkBytes = 0.0;
    {
	fso_iterator next(NL);
	fsobj *f;
	while (f = next())
	    switch(f->stat.VnodeType) {
		case File:
		    FileCount++;
		    if (HAVEDATA(f) && !f->flags.owrite)
			FileBytes += (float)((f->stat.Length + 1023) & 0xFFFFFC00);
		    break;

		case Directory:
		    DirCount++;
		    if (HAVEDATA(f))
			DirBytes += (float)(f->stat.Length);
		    break;

		case SymbolicLink:
		    SymlinkCount++;
		    if (HAVEDATA(f))
			SymlinkBytes += (float)(f->stat.Length);
		    break;

		case Invalid:
		default:
		    f->print(logFile);
		    Choke("Simulate: bogus vnode type (%d)", f->stat.VnodeType);
	    }
	if (FileBytes != (float)FSDB->blocks * 1024.0)
	    Choke("Simulate: file bytes mismatch (%f != %f)",
		  FileBytes, (float)FSDB->blocks * 1024.0);
    }

    /* Compute volume statistics. */
    cmlstats current;
    cmlstats cancelled;
    VDB->GetCmlStats(current, cancelled);

    /* Send stats to out file. */
    fdprint(fileno(SimOutfile), "%2.2f  %4d  %4d  %4d  %10.1f  %10.1f  %10.1f  ",
	    (float)(LastReport - SimStartTime) / 60.0, FileCount, DirCount, 
	    SymlinkCount, FileBytes / 1024.0, DirBytes / 1024.0, 
	    SymlinkBytes / 1024.0);
    fdprint(fileno(SimOutfile), "%4d  %10.1f  %10.1f  %4d  %10.1f  ",
	    current.store_count, current.store_size / 1024.0,
	    current.store_contents_size / 1024.0,
	    current.other_count, current.other_size / 1024.0);
    fdprint(fileno(SimOutfile), "%4d  %10.1f  %10.1f  %4d  %10.1f\n",
	    cancelled.store_count, cancelled.store_size / 1024.0,
	    cancelled.store_contents_size / 1024.0,
	    cancelled.other_count, cancelled.other_size / 1024.0);
}


void Simulate() {
    ASSERT(Simulating);

    for (;;) {
	if (recPtr == NULL)
	    SimExit();
	if (recPtr->time.tv_sec < SimTime)
	    Choke("Simulate: currTime (%d) < SimTime (%d)",
		  recPtr->time.tv_sec, SimTime);
	if (recPtr->time.tv_sec > SimTime) {
	    SimTime = recPtr->time.tv_sec;

	    /* Reports high-water marks periodically. */
	    while (SimTime - LastReport >= SimReportInterval) 
		SimReport();

	    return;
	}

	/* Skip records outside period of interest. */
	if (SimTime < SimStartTime) {
	    Trace_FreeRecord(SimInfile, recPtr);
	    recPtr = Trace_GetRecord(SimInfile);
	    continue;
	}
	if (SimTime >= SimEndTime) {
	    Trace_FreeRecord(SimInfile, recPtr);
	    SimExit();
	}

	/* Dispatch simulator thread and wait for it to complete. */
	ASSERT(Simulator->idle);
	Simulator->idle = 0;
	VprocSignal((char *)Simulator);
	while (!Simulator->idle)
	    VprocWait((char *)Simulator);

	/* Sanity check block count. */
extern char verbose;
	 if (/*verbose*/LogLevel > 0) {
	     int real_blocks = 0;
	     fso_iterator next(NL);
	     fsobj *f;
	     while (f = next())
		 if (f->IsFile() && HAVEDATA(f) && !f->flags.owrite)
		     real_blocks += (int)BLOCKS(f);
	     if (real_blocks != FSDB->blocks)
		 Choke("Simulate: real_blocks (%d) != FSDB->blocks (%d)",
		       real_blocks, FSDB->blocks);
	 }

	 /* Get next record. */
	 Trace_FreeRecord(SimInfile, recPtr);
	 recPtr = Trace_GetRecord(SimInfile);
    }
}


simulator::simulator() : vproc("Simulator", (PROCBODY) &simulator::main, VPT_Simulator, SimulatorStackSize) {
    LOG(100, ("simulator::simulator(%#x): %-16s : lwpid = %d\n", this, name, lwpid));

    /* Poke main procedure. */
    VprocSignal((char *)this, 1);
}


simulator::simulator(simulator& s) : vproc(*((vproc *)&s)) {
    abort();
}


simulator::operator=(simulator& s) {
    abort();
    return(0);
}


simulator::~simulator() {
    LOG(100, ("simulator::~simulator: %-16s : lwpid = %d\n", name, lwpid));
}


void simulator::main(void *parm) {
    /* Wait for ctor to poke us. */
    VprocWait((char *)this);

    for (;;) {
	/* Signal readiness. */
	idle = 1;
	VprocSignal((char *)this);

	/* Wait for new request. */
	while (idle)
	    VprocWait((char *)this);

	u.Init();
	u.u_priority = FSDB->StdPri();
	u.u_flags = (FOLLOW_SYMLINKS | TRAVERSE_MTPTS | REFERENCE);
	switch (recPtr->opcode) {
	    case DFS_OPEN: {
		struct dfs_open *r = (struct dfs_open *)recPtr;
		recType = r->fid.tag;

		/* Get/initialize/create object. */
		ViceDataType type = SIM_VTTOFT(r->fileType);
		ViceFid fid; XlateFid(&r->fid, &fid, type);
		Begin_VFS(fid.Volume, (int)VFSOP_OPEN,
			  (r->flags & (FWRITE|O_CREAT|O_TRUNC)) ? VM_MUTATING : VM_OBSERVING);
		fsobj *f = 0;
		if (r->oldSize == -1) {
		    /* Create a file. */
		    ASSERT(type == File);
		    ViceFid pfid; XlateFid(&r->dirFid, &pfid, Directory);
		    char name[MAXNAMLEN];
		    GetComponent(r->path, name);
		    f = CreateFso(&fid, type, &pfid, name);
		    f->DemoteLock();
		}
		else {
		    /* Get an existing object. */
		    unsigned long length = (type == File ? r->oldSize :
					    type == Directory ? DIR_SIZE : SYMLINK_SIZE);
		    f = GetFso(&fid, RC_DATA, type, length, 0, 0);
		}

		/* Perform operation. */
		int writep = r->flags & FWRITE;
		int execp = 0;
		int truncp = (r->flags & O_TRUNC) && (r->oldSize	!= -1);	/* truncate explicitly below! */
		f->PromoteLock();
		if (f->Open(writep, execp, /*truncp*/0, 0, 0, SIMUID) != 0)
		    Choke("Simulate: fsobj::Open failed");

		/* Truncate if necessary. */
		if (truncp && !WRITING(f)) {
		    FSO_ASSERT(f, r->size == 0);
		    if (!excludeTmp || !IsTmpFile(f))
		        if (f->vol->LogSetAttr(SimTime, SIMUID, &fid,
					   0, (Date_t)-1, (UserId)-1, (RPC2_Unsigned)-1) != 0)
			    Choke("Simulate: LogSetAttr failed");
		    f->LocalSetAttr(SimTime, 0, (Date_t)-1, (vuid_t)-1, (unsigned short)-1);
		}

		PutFso(&f);
		End_VFS(0);
		}	break;

	    case DFS_CLOSE: {
		struct dfs_close *r = (struct dfs_close *)recPtr;
		recType = r->fid.tag;

		/* Get/initialize object. */
		ViceDataType type = SIM_VTTOFT(r->fileType);
		ViceFid fid; XlateFid(&r->fid, &fid, type);
		unsigned long length = (type == File ? (unsigned long)-1 :
					type == Directory ? DIR_SIZE : SYMLINK_SIZE);
		Begin_VFS(fid.Volume, (int)VFSOP_CLOSE,
			  (r->flags & FWRITE) ? VM_MUTATING : VM_OBSERVING);
		fsobj *f = GetFso(&fid, RC_DATA, type, length, 0, 0);

		/* Perform operation. */
		int writep = r->flags & FWRITE;
		int execp = 0;
		f->PromoteLock();
		if (f->Close(writep, execp, SIMUID) != 0)
		    Choke("Simulate: fsobj::Close failed");
		if (writep) {
		    if (!WRITING(f) && !DYING(f)) {
			/* This duplicates code in fsobj::Close()! */
			int FinalSize = (int)r->size;
			int old_blocks = (int)BLOCKS(f);
			int new_blocks = NBLOCKS(FinalSize);
			UpdateCacheStats(&FSDB->FileDataStats, WRITE, MIN(old_blocks, new_blocks));
			if (FinalSize < f->stat.Length)
			    UpdateCacheStats(&FSDB->FileDataStats, REMOVE, (old_blocks - new_blocks));
			else if (FinalSize > f->stat.Length)
			    UpdateCacheStats(&FSDB->FileDataStats, CREATE, (new_blocks - old_blocks));
			FSDB->ChangeDiskUsage(NBLOCKS(FinalSize));

			if (!excludeTmp || !IsTmpFile(f))
			    if (f->vol->LogStore(SimTime, SIMUID, &fid, r->size) != 0)
			        Choke("Simulate: LogStore failed");
			f->LocalStore(SimTime, r->size);
		    }
		}

		PutFso(&f);
		End_VFS(0);
		}	break;

	    case DFS_STAT:
	    case DFS_LSTAT: {
		struct dfs_stat *r = (struct dfs_stat *)recPtr;
		recType = r->fid.tag;

		/* Get/initialize object. */
		ViceDataType type = SIM_VTTOFT(r->fileType);
		ViceFid fid; XlateFid(&r->fid, &fid, type);
		unsigned long length = (type == File ? (unsigned long)-1 :
					type == Directory ? DIR_SIZE : SYMLINK_SIZE);
		Begin_VFS(fid.Volume, (int)VFSOP_GETATTR);
		fsobj *f = GetFso(&fid, RC_STATUS, type, length, 0, 0);

		PutFso(&f);
		End_VFS(0);
		}	break;

	    case DFS_CHDIR:
	    case DFS_CHROOT: {
		struct dfs_chdir *r = (struct dfs_chdir *)recPtr;
		recType = r->fid.tag;

		/* Get/initialize object. */
		ViceFid fid; XlateFid(&r->fid, &fid, Directory);
		Begin_VFS(fid.Volume, (int)VFSOP_GETATTR);
		fsobj *f = GetFso(&fid, RC_STATUS, Directory, DIR_SIZE, 0, 0);

		PutFso(&f);
		End_VFS(0);
		}	break;

	    case DFS_CREAT: {
		/* This is very like a DFS_OPEN! */
		struct dfs_creat *r = (struct dfs_creat *)recPtr;
		recType = r->fid.tag;

		/* Get/initialize parent. */
		ViceFid pfid; XlateFid(&r->dirFid, &pfid, Directory);
		Begin_VFS(pfid.Volume, (int)VFSOP_CREATE);
		fsobj *pf = GetFso(&pfid, RC_DATA, Directory, DIR_SIZE, 0, 0);

		/* Get/initialize/create target. */
		ViceFid cfid; XlateFid(&r->fid, &cfid, File);
		char name[MAXNAMLEN];
		GetComponent(r->path, name);
		fsobj *cf = 0;
		if (r->oldSize == -1) {
		    /* Create a file. */
		    pf->PromoteLock();
		    cf = CreateFso(&cfid, File, pf, name);
		    cf->DemoteLock();
		    pf->DemoteLock();
		}
		else {
		    /* Get an existing file. */
		    FSO_HOLD(pf); pf->UnLock(RD);
		    cf = GetFso(&cfid, RC_DATA, File, r->oldSize, &pfid, name);
		    pf->Lock(RD); FSO_RELE(pf);
		}

		/* Perform open. */
		int writep = 1;
		int execp = 0;
		int truncp = (r->oldSize != -1);	    /* truncate explicitly below! */
		cf->PromoteLock();
		if (cf->Open(writep, execp, /*truncp*/0, 0, 0, SIMUID) != 0)
		    Choke("Simulate: fsobj::Open failed");

		/* Truncate if necessary. */
		if (truncp && !WRITING(cf)) {
		    if (!excludeTmp || !IsTmpFile(cf))
		        if (cf->vol->LogSetAttr(SimTime, SIMUID, &cfid,
					    0, (Date_t)-1, (UserId)-1, (RPC2_Unsigned)-1) != 0)
			    Choke("Simulate: LogSetAttr failed");
		    cf->LocalSetAttr(SimTime, 0, (Date_t)-1, (vuid_t)-1, (unsigned short)-1);
		}

		PutFso(&pf);
		PutFso(&cf);
		End_VFS(0);
		}	break;

	    case DFS_MKDIR: {
		struct dfs_mkdir *r = (struct dfs_mkdir *)recPtr;
		recType = r->fid.tag;

		/* Get/initialize parent. */
		ViceFid pfid; XlateFid(&r->dirFid, &pfid, Directory);
		Begin_VFS(pfid.Volume, (int)VFSOP_MKDIR);
		fsobj *pf = GetFso(&pfid, RC_DATA, Directory, DIR_SIZE, 0, 0);

		/* Create target. */
		ViceFid cfid; XlateFid(&r->fid, &cfid, Directory);
		char name[MAXNAMLEN];
		GetComponent(r->path, name);
		pf->PromoteLock();
		fsobj *cf = CreateFso(&cfid, Directory, pf, name);

		PutFso(&pf);
		PutFso(&cf);
		End_VFS(0);
		}	break;

	    case DFS_ACCESS:
	    case DFS_CHMOD: {
		struct dfs_access *r = (struct dfs_access *)recPtr;
		recType = r->fid.tag;

		/* Get/initialize object. */
		ViceDataType type = SIM_VTTOFT(r->fileType);
		ViceFid fid; XlateFid(&r->fid, &fid, type);
		unsigned long length = (type == File ? (unsigned long)-1 :
					type == Directory ? DIR_SIZE : SYMLINK_SIZE);
		Begin_VFS(fid.Volume,
			  (int)(recPtr->opcode == DFS_ACCESS ? VFSOP_ACCESS : VFSOP_SETATTR));
		fsobj *f = GetFso(&fid, RC_STATUS, type, length, 0, 0);

		/* Perform operation. */
		if (recPtr->opcode == DFS_CHMOD) {
		    f->PromoteLock();
		    if (!excludeTmp || !IsTmpFile(f))
		        if (f->vol->LogSetAttr(SimTime, SIMUID, &fid,
					   (RPC2_Unsigned)-1, (Date_t)-1,
					   (UserId)-1, (RPC2_Unsigned)r->mode) != 0)
			    Choke("Simulate: LogSetAttr failed");
		    f->LocalSetAttr(SimTime, (RPC2_Unsigned)-1, (Date_t)-1,
				    (vuid_t)-1, (unsigned short)r->mode);
		}

		PutFso(&f);
		End_VFS(0);
		} break;

	    case DFS_READLINK: {
		struct dfs_chdir *r = (struct dfs_chdir *)recPtr;
		recType = r->fid.tag;

		/* Get/initialize object. */
		ViceFid fid; XlateFid(&r->fid, &fid, SymbolicLink);
		Begin_VFS(fid.Volume, (int)VFSOP_READLINK);
		fsobj *f = GetFso(&fid, RC_DATA, SymbolicLink, SYMLINK_SIZE, 0, 0);

		PutFso(&f);
		End_VFS(0);
		}	break;

	    case DFS_GETSYMLINK: {
		struct dfs_getsymlink *r = (struct dfs_getsymlink *)recPtr;
		recType = r->fid.tag;

		/* Get/initialize object. */
		ViceFid fid; XlateFid(&r->fid, &fid, SymbolicLink);
		Begin_VFS(fid.Volume, (int)VFSOP_READLINK);
		fsobj *f = GetFso(&fid, RC_DATA, SymbolicLink, SYMLINK_SIZE, 0, 0);

		/* fill in link contents if unset or not current */
		/* 
		 * don't do this unless the target path is included
		 * in the call to LogSymlink.  Currently it isn't.
		 */
/*		if (!STREQ(f->data.symlink, r->path)) 
		    UpdateFsoName(&f->data.symlink, r->path);
*/
		PutFso(&f);
		End_VFS(0);
		}	break;

	    case DFS_CHOWN: {
		struct dfs_chown *r = (struct dfs_chown *)recPtr;
		recType = r->fid.tag;

		/* Get/initialize object. */
		ViceDataType type = SIM_VTTOFT(r->fileType);
		ViceFid fid; XlateFid(&r->fid, &fid, type);
		unsigned long length = (type == File ? (unsigned long)-1 :
					type == Directory ? DIR_SIZE : SYMLINK_SIZE);
		Begin_VFS(fid.Volume, (int)VFSOP_SETATTR);
		fsobj *f = GetFso(&fid, RC_STATUS, type, length, 0, 0);

		/* Perform operation. */
		f->PromoteLock();
		if (!excludeTmp || !IsTmpFile(f))
		    if (f->vol->LogSetAttr(SimTime, SIMUID, &fid,
				       (RPC2_Unsigned)-1, (Date_t)-1,
				       (UserId)r->owner, (RPC2_Unsigned)-1) != 0)
		        Choke("Simulate: LogSetAttr failed");
		f->LocalSetAttr(SimTime, (RPC2_Unsigned)-1, (Date_t)-1,
				(vuid_t)r->owner, (unsigned short)-1);

		PutFso(&f);
		End_VFS(0);
		}	break;

	    case DFS_UTIMES: {
		struct dfs_utimes *r = (struct dfs_utimes *)recPtr;
		recType = r->fid.tag;

		/* Get/initialize object. */
		ViceDataType type = SIM_VTTOFT(r->fileType);
		ViceFid fid; XlateFid(&r->fid, &fid, type);
		unsigned long length = (type == File ? (unsigned long)-1 :
					type == Directory ? DIR_SIZE : SYMLINK_SIZE);
		Begin_VFS(fid.Volume, (int)VFSOP_SETATTR);
		fsobj *f = GetFso(&fid, RC_STATUS, type, length, 0, 0);

		/* Perform operation. */
		f->PromoteLock();
		if (!excludeTmp || !IsTmpFile(f))
		    if (f->vol->LogSetAttr(SimTime, SIMUID, &fid,
				       (RPC2_Unsigned)-1, (Date_t)r->mtime.tv_sec,
				       (UserId)-1, (RPC2_Unsigned)-1) != 0)
		        Choke("Simulate: LogSetAttr failed");
		f->LocalSetAttr(SimTime, (RPC2_Unsigned)-1, (Date_t)r->mtime.tv_sec,
				(vuid_t)-1, (unsigned short)-1);

		PutFso(&f);
		End_VFS(0);
		}	break;

	    case DFS_TRUNCATE: {
		struct dfs_truncate *r = (struct dfs_truncate *)recPtr;
		recType = r->fid.tag;

		/* Get/initialize object. */
		ViceFid fid; XlateFid(&r->fid, &fid, File);
		Begin_VFS(fid.Volume, (int)VFSOP_SETATTR);
		fsobj *f = GetFso(&fid, RC_DATA, File, r->oldSize, 0, 0);

		/* Perform operation. */
		if (!WRITING(f)) {
		    f->PromoteLock();
		    if (!excludeTmp || !IsTmpFile(f))
		        if (f->vol->LogSetAttr(SimTime, SIMUID, &fid,
					   0, (Date_t)-1, (UserId)-1, (RPC2_Unsigned)-1) != 0)
			    Choke("Simulate: LogSetAttr failed");
		    f->LocalSetAttr(SimTime, 0, (Date_t)-1, (vuid_t)-1, (unsigned short)-1);
		}

		PutFso(&f);
		End_VFS(0);
		}	break;

	    case DFS_RENAME: {
		struct dfs_rename *r = (struct dfs_rename *)recPtr;
		recType = r->fromFid.tag;

		/* Get/initialize source parent. */
		ViceFid spfid; XlateFid(&r->fromDirFid, &spfid, Directory);
		Begin_VFS(spfid.Volume, (int)VFSOP_RENAME);
		fsobj *spf = GetFso(&spfid, RC_DATA, Directory, DIR_SIZE, 0, 0);
		FSO_HOLD(spf); spf->UnLock(RD);

		/* Get/initialize target parent. */
		ViceFid tpfid; XlateFid(&r->toDirFid, &tpfid, Directory);
		fsobj *tpf;
		if (FID_EQ(spfid, tpfid))
		    tpf = spf;
		else {
		  tpf = GetFso(&tpfid, RC_DATA, Directory, DIR_SIZE, 0, 0);
		  FSO_HOLD(tpf); tpf->UnLock(RD);
		}

		/* Get/initialize source. */
		ViceDataType stype = SIM_VTTOFT(r->fileType);
		ViceFid sfid; XlateFid(&r->fromFid, &sfid, stype);
		unsigned long slength = (stype == File ? (unsigned long)-1 :
					 stype == Directory ? DIR_SIZE : SYMLINK_SIZE);
		char fromname[MAXNAMLEN];
		GetComponent(r->fromPath, fromname);
		fsobj *sf = GetFso(&sfid, RC_STATUS, stype, slength, &spfid, fromname);
		FSO_HOLD(sf); sf->UnLock(RD);

		/* Deal with cross-directory links. */
		if (stype == File && spf != tpf && sf->stat.LinkCount > 1) {
		    spf->Lock(RD); FSO_RELE(spf);
		    sf->Lock(RD); FSO_RELE(sf);
		    InferOtherNameRemoval(spf, sf, fromname);
		    FSO_HOLD(spf); spf->UnLock(RD);
		    FSO_HOLD(sf); sf->UnLock(RD);
		}

		/* Get/initialize target (if appropriate). */
		ViceDataType ttype = stype;
		ViceFid tfid;
		char toname[MAXNAMLEN];
		GetComponent(r->toPath, toname);
		fsobj *tf = 0;
		int LinkDiscrepancy = 0;
		if (r->toFid.tag != (char)-1) {	/* target exists */
		    XlateFid(&r->toFid, &tfid, ttype);
		    unsigned long tlength = (ttype == File ? (unsigned long)r->size :
					     ttype == Directory ? DIR_SIZE : SYMLINK_SIZE);
		    int trights = (ttype == Directory ? RC_DATA : RC_STATUS);
		    tf = GetFso(&tfid, trights, ttype, tlength, &tpfid, toname);
		    FSO_ASSERT(tf, (ttype != File || tf->stat.LinkCount >= 1) && (ttype != SymbolicLink || tf->stat.LinkCount == 1));

		    /* 
		     * if the target link count does not match the record link
		     * count, a cross directory link was probably ignored. 
		     * ignore this record too. 
		     */
		    if (r->numLinks > 1 && tf->stat.LinkCount != r->numLinks) {
			eprint("ignoring rename after cross directory link: %s %s",
			       r->fromPath, r->toPath);
			LinkDiscrepancy = 1;
			tf->UnLock(RD);
		    } else if (ttype == Directory && !tf->dir_IsEmpty()) {
			/* Remove target's children if it's a directory! */
                            ::EnumerateDir((long *)tf->data.dir, (int (*)(void * ...))::InferNameRemoval, (long)tf);
			FSO_ASSERT(tf, tf->dir_IsEmpty());
		    }
		}
		else {
		    /* Check whether inferred name removal is needed! */
		    if (tpf->dir_Lookup(toname, &tfid) == 0) {
			tpf->Lock(RD); FSO_RELE(tpf);
			InferNameRemoval(tpf, toname);
			FSO_HOLD(tpf); tpf->UnLock(RD);
		    }

		    tfid = NullFid;
		    tf = 0;
		}

		/* Perform the operation. */
		if (!LinkDiscrepancy) {
		    tpf->Lock(WR); FSO_RELE(tpf);
		    if (spf != tpf) { spf->Lock(WR); FSO_RELE(spf); }
		    sf->Lock(WR); FSO_RELE(sf);
		    if (tf != 0) tf->PromoteLock();
		    if (!excludeTmp) {
			if (tpf->vol->LogRename(SimTime, SIMUID, &spfid, fromname,
						&tpfid, toname, &sfid, &tfid, 
						(tf ? tf->stat.LinkCount : 0)) != 0)
			    Choke("Simulate: LogRename failed");
		    } else 
			LogTmpRename(SimTime, spf, fromname, sf, tpf, toname, tf);

		    tpf->LocalRename(SimTime, spf, fromname, sf, toname, tf);

		    /* Must infer removal of other links when r->numLinks == 1. */
		    if (tf && r->numLinks == 1) {
			tpf->DemoteLock();
			if (spf != tpf) { FSO_HOLD(spf); spf->UnLock(WR); }
			FSO_HOLD(sf); sf->UnLock(WR);
			FSO_HOLD(tf); tf->UnLock(WR);
			while (tf->stat.LinkCount > 0) {
			    char comp[MAXNAMLEN];
			    FSO_ASSERT(tpf, tpf->dir_LookupByFid(comp, &tf->fid) == 0);
			    InferNameRemoval(tpf, comp);
			}
			if (spf != tpf) { spf->Lock(WR); FSO_RELE(spf); }
			sf->Lock(WR); FSO_RELE(sf);
			tf->Lock(WR); FSO_RELE(tf);
		    }

		    if (spf != tpf)
			PutFso(&spf);
		    PutFso(&tpf);
		    PutFso(&sf);
		    PutFso(&tf);
		}

		End_VFS(0);
		}	break;

	    case DFS_LINK: {
		struct dfs_link *r = (struct dfs_link *)recPtr;
		recType = r->fromFid.tag;

		/* Get/initialize source/target parent. */
		ViceFid spfid; XlateFid(&r->fromDirFid, &spfid, Directory);
		ViceFid tpfid; XlateFid(&r->toDirFid, &tpfid, Directory);
		Begin_VFS(tpfid.Volume, (int)VFSOP_LINK);
		fsobj *tpf = GetFso(&tpfid, RC_DATA, Directory, DIR_SIZE, 0, 0);
		FSO_HOLD(tpf); tpf->UnLock(RD);

		/* Can't have cross-directory links!  -JJK */
/*		ASSERT(FID_EQ(spfid, tpfid));*/
		if (!FID_EQ(spfid, tpfid)) {
		    eprint("ignoring cross-directory link:  %s %s", r->fromPath, r->toPath);
		    tpf->Lock(RD); FSO_RELE(tpf);
		    PutFso(&tpf);
		    End_VFS(0);
		    break;
		}

		/* Get/initialize source. */
		ViceDataType stype = SIM_VTTOFT(r->fileType);
		ASSERT(stype == File);
		ViceFid sfid; XlateFid(&r->fromFid, &sfid, stype);
		char fromname[MAXNAMLEN];
		GetComponent(r->fromPath, fromname);
		fsobj *sf = GetFso(&sfid, RC_STATUS, stype, (unsigned long)-1, &spfid, fromname);

		/* Perform operation. */
		tpf->Lock(WR); FSO_RELE(tpf);
		sf->PromoteLock();
		char toname[MAXNAMLEN];
		GetComponent(r->toPath, toname);
		{
		    /* Unlink toname in target-dir if necessary. */
		    FSO_ASSERT(tpf, toname != 0 && toname[0] != '\0' && !STREQ(toname, "..") && !STREQ(toname, "."));
		    ViceFid tmpfid;
		    if (tpf->dir_Lookup(toname, &tmpfid) == 0) {
			tpf->DemoteLock();
			InferNameRemoval(tpf, toname);
			tpf->PromoteLock();
		    }
		}
		/* 
		 * since we ignore cross-directory links, if one of the source
		 * and target is in /tmp so is the other.
		 */
		if (!excludeTmp || !IsTmpFile(sf))
		    if (tpf->vol->LogLink(SimTime, SIMUID, &tpfid, toname, &sfid) != 0)
			Choke("Simulate: LogLink failed");
		tpf->LocalLink(SimTime, toname, sf);

		PutFso(&tpf);
		PutFso(&sf);
		End_VFS(0);
		}	break;

	    case DFS_SYMLINK: {
		struct dfs_symlink *r = (struct dfs_symlink *)recPtr;
		recType = r->fid.tag;

		/* Get/initialize parent. */
		ViceFid pfid; XlateFid(&r->dirFid, &pfid, Directory);
		Begin_VFS(pfid.Volume, (int)VFSOP_SYMLINK);
		fsobj *pf = GetFso(&pfid, RC_DATA, Directory, DIR_SIZE, 0, 0);

		/* Create target. */
		ViceFid cfid; XlateFid(&r->fid, &cfid, SymbolicLink);
		if (recType == ITYPE_AFS || recType == ITYPE_CFS) {
		    /* Special case mount point creation! */
		    fsobj *cf = FSDB->Find(&cfid);
		    if (cf) {
			PutFso(&pf);
			End_VFS(0);
			break;
		    }
		}
		char name[MAXNAMLEN];
		GetComponent(r->linkPath, name);
		pf->PromoteLock();
		fsobj *cf = CreateFso(&cfid, SymbolicLink, pf, name);

		PutFso(&pf);
		PutFso(&cf);
		End_VFS(0);
		}	break;

	    case DFS_UNLINK: {
		struct dfs_rmdir *r = (struct dfs_rmdir *)recPtr;
		recType = r->fid.tag;

		/* Get/initialize parent. */
		ViceFid pfid; XlateFid(&r->dirFid, &pfid, Directory);
		Begin_VFS(pfid.Volume, (int)VFSOP_REMOVE);
		fsobj *pf = GetFso(&pfid, RC_DATA, Directory, DIR_SIZE, 0, 0);
		FSO_HOLD(pf); pf->UnLock(RD);

		/* Get/initialize target */
		ViceDataType ctype = SIM_VTTOFT(r->fileType);
		ViceFid cfid; XlateFid(&r->fid, &cfid, ctype);
		unsigned long clength = (ctype == File ? (unsigned long)r->size :
					 ctype == Directory ? DIR_SIZE : SYMLINK_SIZE);
		char name[MAXNAMLEN];
		GetComponent(r->path, name);
		fsobj *cf = GetFso(&cfid, RC_STATUS, ctype, clength, &pfid, name);
		FSO_ASSERT(cf, (ctype == File && cf->stat.LinkCount >= 1) || (ctype == SymbolicLink && cf->stat.LinkCount == 1));

		/* 
		 * if the object link count does not match the record link count
		 * a cross directory link probably occurred and was ignored.
		 * ignore this record too.
		 */
		if (r->numLinks > 1 && cf->stat.LinkCount != r->numLinks) {
		    eprint("ignoring unlink after cross directory link: %s",
			   r->path);
		    FSO_RELE(pf);
		} else {
		    /* Perform operation. */
		    pf->Lock(WR); FSO_RELE(pf);
		    cf->PromoteLock();
		    if (!excludeTmp || !IsTmpFile(cf))
			if (pf->vol->LogRemove(SimTime, SIMUID, &pfid, name, &cfid, cf->stat.LinkCount) != 0)
			    Choke("Simulate: LogRemove failed");
		    pf->LocalRemove(SimTime, name, cf);

		    /* Must infer removal of other links when r->numLinks == 1. */
		    if (r->numLinks == 1) {
			pf->DemoteLock();
			FSO_HOLD(cf); cf->UnLock(WR);
			while (cf->stat.LinkCount > 0) {
			    char comp[MAXNAMLEN];
			    FSO_ASSERT(pf, pf->dir_LookupByFid(comp, &cf->fid) == 0);
			    InferNameRemoval(pf, comp);
			}
			cf->Lock(WR); FSO_RELE(cf);
		    }

		    PutFso(&pf);
		}
		PutFso(&cf);
		End_VFS(0);
		}	break;

	    case DFS_RMDIR: {
		struct dfs_rmdir *r = (struct dfs_rmdir *)recPtr;
		recType = r->fid.tag;

		/* Can't link directories in Coda */
		if (r->numLinks == 1) {
		    /* Get/initialize parent. */
		    ViceFid pfid; XlateFid(&r->dirFid, &pfid, Directory);
		    Begin_VFS(pfid.Volume, (int)VFSOP_RMDIR);
		    fsobj *pf = GetFso(&pfid, RC_DATA, Directory, DIR_SIZE, 0, 0);
		    FSO_HOLD(pf); pf->UnLock(RD);

		    /* Get/initialize target. */
		    ViceFid cfid; XlateFid(&r->fid, &cfid, Directory);
		    char name[MAXNAMLEN];
		    GetComponent(r->path, name);
		    fsobj *cf = GetFso(&cfid, RC_DATA, Directory, DIR_SIZE, &pfid, name);

		    /* Remove any children. */
		    if (!cf->dir_IsEmpty()) {
                            ::EnumerateDir((long *)cf->data.dir, (int (*)(void * ...))::InferNameRemoval, (long)cf);
			FSO_ASSERT(cf, cf->dir_IsEmpty());
		    }

		    /* Perform operation. */
		    pf->Lock(WR); FSO_RELE(pf);
		    cf->PromoteLock();
		    if (!excludeTmp || !IsTmpFile(cf))
			if (pf->vol->LogRmdir(SimTime, SIMUID, &pfid, name, &cfid) != 0)
			    Choke("Simulate: LogRmdir failed");
		    pf->LocalRmdir(SimTime, name, cf);

		    PutFso(&pf);
		    PutFso(&cf);
		    End_VFS(0);
		}
		}	break;

	    case DFS_LOOKUP: {
		struct dfs_lookup *r = (struct dfs_lookup *)recPtr;
		ASSERT(r->compFid.tag == r->parentFid.tag);
		recType = r->compFid.tag;

		char name[MAXNAMLEN];
		GetComponent(r->path, name);
		if (!STREQ(name, "..") && !STREQ(name, ".")) {
		    /* Get/initialize parent. */
		    ViceFid pfid; XlateFid(&r->parentFid, &pfid, Directory);
		    Begin_VFS(pfid.Volume, (int)VFSOP_LOOKUP);
		    fsobj *pf = GetFso(&pfid, RC_DATA, Directory, DIR_SIZE, 0, 0);
		    FSO_HOLD(pf); pf->UnLock(RD);

		    /* Get/initialize target. */
		    ViceDataType ctype = SIM_VTTOFT(r->fileType);
		    ViceFid cfid; XlateFid(&r->compFid, &cfid, ctype);
		    unsigned long clength = (ctype == File ? (unsigned long)-1 :
					     ctype == Directory ? DIR_SIZE : SYMLINK_SIZE);
		    /* Watch out for fake mount points! */
		    if (ISFAKE(cfid)) {
			FID_ASSERT(cfid, recType == ITYPE_CFS);
			pf->Lock(RD); FSO_RELE(pf);
			PutFso(&pf);
			End_VFS(0);
			break;
		    }
		    /* Watch out for volume roots! */
		    fsobj *cf;
		    if (pfid.Volume != cfid.Volume) {
			ASSERT(recType == ITYPE_AFS || recType == ITYPE_CFS);
			ASSERT((cfid.Vnode == ROOT_VNODE && cfid.Unique == ROOT_UNIQUE) || recType == ITYPE_CFS);
			cf = GetFso(&cfid, RC_STATUS, ctype, clength, 0, 0);

			/* fill in component name */
			if (cf && !STREQ(cf->comp, r->path)) 
			    UpdateFsoName(&cf->comp, r->path);
		    }
		    else {
			cf = GetFso(&cfid, RC_STATUS, ctype, clength, &pfid, name);
		    }

		    pf->Lock(RD); FSO_RELE(pf);
		    PutFso(&pf);
		    PutFso(&cf);
		    End_VFS(0);
		}
		}	break;

	    case DFS_ROOT: {
		struct dfs_root *r = (struct dfs_root *)recPtr;

		/* Get/initialize coverer. */
		ViceFid pfid; XlateFid(&r->compFid, &pfid, Directory);
		Begin_VFS(pfid.Volume, (int)VFSOP_ROOT);
		fsobj *pf = GetFso(&pfid, RC_DATA, Directory, DIR_SIZE, 0, 0);

		/* Get/initialize coveree. */
		ViceFid cfid; XlateFid(&r->targetFid, &cfid, Directory);
		fsobj *cf = GetFso(&cfid, RC_DATA, Directory, DIR_SIZE, 0, 0);
		
		/* if unset, link root & mount point fsos */
		if (!pf->mvstat) pf->mvstat = MOUNTPOINT;
		if (!pf->u.root) pf->u.root = cf;

		/* 
		 * root fso's mvstat may not be set because ufs fids 
		 * don't have vnodes and uniquifiers == ROOT_etc.
		 * Set that, and the component name from the root record.
		 */
		if (!cf->mvstat) cf->mvstat = ROOT;
		if (!cf->u.mtpoint) cf->u.mtpoint = pf;
		if (!STREQ(cf->comp, r->path)) 
		    UpdateFsoName(&cf->comp, r->path);

		PutFso(&pf);
		PutFso(&cf);
		End_VFS(0);
		}	break;

	    case DFS_EXECVE:	    /* Open with (execp = T)? */
	    case DFS_EXIT:	    /* Close with (execp = T)? */
	    default:
		Choke("Simulate: bogus type (%d)", recPtr->opcode);
	}
    }
}


/*
 * Gnarly routine.
 * if excluding references to /tmp, log the appropriate 
 * operations based on the location of the source and target.
 * if both source and target are in /tmp, log nothing.  if 
 * neither is in /tmp log a rename.  If the source is in /tmp
 * but the target isn't, log a create for the file and a store
 * or a mkdir if the source is a directory.  If the source is 
 * not in /tmp but the target is, log a remove or rmdir for
 * the source.
 */
void simulator::LogTmpRename(time_t SimTime, fsobj *spf, char *fromname, fsobj *sf, 
			     fsobj *tpf, char *toname, fsobj *tf) {
    int code = 0;

    if (IsTmpFile(sf)) {
	if ((tf && !IsTmpFile(tf)) || (!tf && !IsTmpFile(tpf))) {
	    /* 
	     * target is not in tmp, create it.  First, if it already exists,
	     * remove it.
	     */
	    if (tf) {
		if (tf->IsDir())
		    code = tpf->vol->LogRmdir(SimTime, SIMUID, &tpf->fid, toname, &tf->fid);
		else 
		    code = tpf->vol->LogRemove(SimTime, SIMUID, &tpf->fid, toname,
					       &tf->fid, tf->stat.LinkCount);
		if (code) 
		    Choke("Simulate: LogTmpRename: Couldn't remove target (code = %d)", code);
	    }
	    switch (sf->stat.VnodeType) {
	    case File:
		code = tpf->vol->LogCreate(SimTime, SIMUID, &tpf->fid, toname, &sf->fid, SIMMODE);
		if (!code) 
		    code = tpf->vol->LogStore(SimTime, SIMUID, &sf->fid, sf->stat.Length);
		break;
	    case Directory:
	        code = tpf->vol->LogMkdir(SimTime, SIMUID, &tpf->fid, toname, &sf->fid, SIMMODE);
		break;
	    case SymbolicLink:
		ASSERT(!tf || tf->IsSymLink());
		code = tpf->vol->LogSymlink(SimTime, SIMUID, &tpf->fid, toname, "", 
					   &sf->fid, SIMMODE);
		break;
	    case Invalid:
	        Choke("simulate::LogTmpRename: bogus VnodeType (%d)", sf->stat.VnodeType);
	    }
	    if (code) 
		Choke("simulate:LogTmpRename: couldn't create target (code = %d)", code);
	}				
    } else {
	/* source is not in /tmp */
	if ((tf && IsTmpFile(tf)) || (!tf && IsTmpFile(tpf))) {
	    /* target is in /tmp, remove the source */
	    if (sf->IsDir())
		code = spf->vol->LogRmdir(SimTime, SIMUID, &spf->fid, fromname, &sf->fid);
	    else 
		code = spf->vol->LogRemove(SimTime, SIMUID, &spf->fid, fromname,
					   &sf->fid, sf->stat.LinkCount);
	    if (code) 
		Choke("Simulate: LogTmpRename: Couldn't remove source (code = %d)", code);
	} else {
	    /* whew! neither is in /tmp, log a rename. */
	    ViceFid tfid;

	    if (tf) tfid = tf->fid;
	    else tfid = NullFid;

	    code = tpf->vol->LogRename(SimTime, SIMUID, &spf->fid, fromname,
				       &tpf->fid, toname, &sf->fid, &tfid, 
				       (tf ? tf->stat.LinkCount : 0));
	    if (code) 
		Choke("Simulate: LogTmpRename: LogRename failed (code = %d)", code);
        }
    }
}


fsobj *simulator::GetFso(ViceFid *key, int rights, ViceDataType type,
			 unsigned long length, ViceFid *pfid, char *comp) {
    LOG(1, ("simulator::GetFso: (%x.%x.%x), %d, %d, %d, (%x.%x.%x), %s\n",
	    key->Volume, key->Vnode, key->Unique, rights, type, length,
	    pfid ? pfid->Volume : 0, pfid ? pfid->Vnode : 0, pfid ? pfid->Unique : 0,
	    comp ? comp : ""));
    ASSERT((type != Directory || length == DIR_SIZE) && (type != SymbolicLink || length == SYMLINK_SIZE));
    ASSERT((pfid == 0 && comp == 0) || (pfid != 0 && comp != 0));
    if (comp != 0 && (comp[0] == '\0' || STREQ(comp, "..") || STREQ(comp, "."))) {
	pfid = 0;
	comp = 0;
    }

    int code = 0;
    fsobj *f = 0;

    /* Can't handle fake mount points! */
    if (ISFAKE(*key)) {
	FID_ASSERT(*key, recType == ITYPE_CFS);
	goto Exit;
    }

    /* 1. Unlink component name in parent if not equal to key. */
    if (pfid != 0) {
	fsobj *pf = GetFso(pfid, RC_DATA, Directory, DIR_SIZE, 0, 0);
	ViceFid cfid;
	if (pf->dir_Lookup(comp, &cfid) == 0 && !FID_EQ(*key, cfid))
	    InferNameRemoval(pf, comp);
	PutFso(&pf);
    }

    /* 2. Get the object. */
    code = FSDB->Get(&f, key, SIMUID, rights, comp);
    if (code != 0) Choke("Simulate: FSDB->Get failed (%d)", code);

    /* 3. Sanity-check type of existing object. */
    if (HAVESTATUS(f) && f->stat.VnodeType != type) {
	FSO_ASSERT(f, recType == ITYPE_UFS || recType == ITYPE_NFS || recType == ITYPE_CFS);
	if (recType == ITYPE_UFS || recType == ITYPE_NFS) {
	    /* Delete object in case this is a new use of a UFS/NFS inode. */
	    InferDelete(f);
	    f = GetFso(key, rights, type, length, pfid, comp);
	}
	else {
	    /* Coda inconsistent object transition. */
	    eprint("Inconsistent object: (%x.%x.%x), %s --> %s",
		   f->fid.Volume, f->fid.Vnode, f->fid.Unique,
		   PrintVnodeType(f->stat.VnodeType), PrintVnodeType(type));

	    switch(f->stat.VnodeType) {
		case File:
		    FSO_ASSERT(f, type == Directory || type == SymbolicLink);
		    if (type == Directory) {
			/* Transition to fake directory -- flush if !dirty, infer deletion otherwise. */
			if (DIRTY(f)) {
			    InferDelete(f);
			}
			else {
			    FlushObject(f);
			}
			f = GetFso(key, rights, type, length, pfid, comp);
		    }
		    else {
			/* Transition to @fid symlink -- ignore. */
			PutFso(&f);
			f = 0;
		    }
		    break;

		case Directory:
		    FSO_ASSERT(f, type == File || type == SymbolicLink);
		    if (type == File) {
			/* Fake directory to real file transition -- flush the fake dir. */
			FSO_ASSERT(f, !DIRTY(f));
			FlushObject(f);
			f = GetFso(key, rights, type, length, pfid, comp);
		    }
		    else {
			/* Transition to @fid symlink -- ignore. */
			 PutFso(&f);
			 f = 0;
		    }
		    break;

		case SymbolicLink:
		    FSO_ASSERT(f, type == File || type == Directory);
		    /* Flush @fid symlink. */
		    FSO_ASSERT(f, !DIRTY(f));
		    FlushObject(f);
		    f = GetFso(key, rights, type, length, pfid, comp);
		    break;

		case Invalid:
		default:
		    FSO_ASSERT(f, FALSE);
	    }
	}

	goto Exit;
    }

    /* 4. Sanity-check length of existing object. */
    if (HAVESTATUS(f)) {
	switch(type) {
	    case File:
		if (!WRITING(f) && length != (unsigned long)-1 && length != f->stat.Length) {
		    if (f->stat.Length == (unsigned long)-1) {
			FSO_ASSERT(f, !HAVEDATA(f));
			f->stat.Length = length;
		    }
		    else
			InferStore(f, length);
		}
		break;

	    case Directory:
		FSO_ASSERT(f, f->stat.Length == DIR_SIZE);
		break;

	    case SymbolicLink:
		FSO_ASSERT(f, f->stat.Length == SYMLINK_SIZE);
		break;

	    case Invalid:
	    default:
		FSO_ASSERT(f, FALSE);
	}
    }

	/* 5. Sanity-check parent of existing object. */
    if (HAVESTATUS(f) && pfid != 0) {
	if (FID_EQ(NullFid, f->pfid)) {
	    /* <pfid, comp> must not be dirty, because that would require an InferredCreate */
	    /* (at the time the object was first ``gotten''). */
	    FSO_ASSERT(f, !DirtyDirEntry(f->vol, pfid, comp) && !CreatedObject(f->vol, pfid));
	    NameInsertion(pfid, comp, f);
	}
	else {
	    if (!FID_EQ(*pfid, f->pfid)) {
		/* If object is a multi-linked file, then remove existing names and insert new one in new parent.*/
		/* Otherwise, infer a real rename. */
		if (type == File) {
		    if (CreatedObject(f->vol, pfid) || DirtyDirEntry(f->vol, pfid, comp) || DIRTY(f)) {
			if (f->stat.LinkCount > 1) {
			    FSO_HOLD(f); f->UnLock(RD);
			    fsobj *pf = GetFso(&f->pfid, RC_DATA, Directory, DIR_SIZE, 0, 0);
			    f->Lock(RD); FSO_RELE(f);
			    char tcomp[MAXNAMLEN];
			    FSO_ASSERT(pf, pf->dir_LookupByFid(tcomp, &f->fid) == 0);
			    InferOtherNameRemoval(pf, f, tcomp);
			    PutFso(&pf);
			}
			InferRename(f, pfid, comp);
		    }
		    else {
			NameRemoval(f);
			NameInsertion(pfid, comp, f);
		    }
		}
		else {
		    InferRename(f, pfid, comp);
		}
	    }
	    else {
		FSO_HOLD(f); f->UnLock(RD);
		fsobj *pf = GetFso(pfid, RC_DATA, Directory, DIR_SIZE, 0, 0);
		ViceFid cfid;
		int code = pf->dir_Lookup(comp, &cfid);
		PutFso(&pf);
		f->Lock(RD); FSO_RELE(f);
		if (code == 0) {
		    FSO_ASSERT(f, FID_EQ(f->fid, cfid));
		}
		else {
		    if (type == File) {
			if (CreatedObject(f->vol, pfid) || CreatedObject(f->vol, &f->fid) || DirtyDirEntry(f->vol, pfid, comp)) {
			    InferLink(f, pfid, comp);
			}
			else {
			    NameInsertion(pfid, comp, f);
			}
		    }
		    else {
			InferRename(f, pfid, comp);
		    }
		}
	    }
	}
    }

    /* 6. Initialize new object. */
    if (!HAVESTATUS(f)) {
	/* Need to create object if parent was created or name has been mutated by earlier transaction. */
	if (pfid != 0 && (CreatedObject(f->vol, pfid) || DirtyDirEntry(f->vol, pfid, comp))) {
	    f->PromoteLock();
	    f->Kill();
	    PutFso(&f);
	    f = InferCreate(key, type, pfid, comp);
	    f->DemoteLock();
	    goto Exit;
	}

	/* Set attributes. */
	f->stat.VnodeType = type;
	f->stat.Length = length;
	f->stat.LinkCount = 0;		/* relevant only for files! */
	if (pfid != 0)
	    NameInsertion(pfid, comp, f);
	f->Matriculate();
	f->Reference();
	f->ComputePriority();
    }

    /* 7. Record data if necessary. */
    if ((rights & RC_DATA) && !HAVEDATA(f)) {
	switch(f->stat.VnodeType) {
	    case File:
		FSO_ASSERT(f, f->stat.Length != (unsigned long)-1);
		FSO_ASSERT(f, FSDB->AllocBlocks(u.u_priority, (int)BLOCKS(f)) == 0);
		f->data.file = &f->cf;
		break;

	    case Directory:
		f->dir_MakeDir();
		break;

	    case SymbolicLink:
		f->data.symlink = (char *)RVMLIB_REC_MALLOC((unsigned)f->stat.Length + 1);
		break;

	    case Invalid:
		FSO_ASSERT(f, 0);
	}
    }

Exit:
    return(f);
}


void simulator::PutFso(fsobj **f_addr) {
    if (!(*f_addr)) return;
    ViceFid OldFid = (*f_addr)->fid;

    FSDB->Put(f_addr);

    /* Translate fid of deleted, non-busy object! */  
    fsobj *f = FSDB->Find(&OldFid);
    if (f != 0 && DYING(f) && !BUSY(f))
	TranslateFid(&OldFid);
}


fsobj *simulator::CreateFso(ViceFid *key, ViceDataType type,
			    ViceFid *pfid, char *comp) {
    LOG(1, ("simulator::CreateFso: (%x.%x.%x), %d, (%x.%x.%x), %s\n",
	    key->Volume, key->Vnode, key->Unique, type,
	    pfid->Volume, pfid->Vnode, pfid->Unique, comp));
    int code = 0;
    fsobj *pf = 0;
    fsobj *f = 0;

    pf = GetFso(pfid, RC_DATA, Directory, DIR_SIZE, 0, 0);
    pf->PromoteLock();
    f = CreateFso(key, type, pf, comp);

    PutFso(&pf);
    return(f);
}


fsobj *simulator::CreateFso(ViceFid *key, ViceDataType type, fsobj *pf, char *comp) {
    LOG(1, ("simulator::CreateFso: (%x.%x.%x), %d, (%x.%x.%x), %s\n",
	    key->Volume, key->Vnode, key->Unique, type,
	    pf->fid.Volume, pf->fid.Vnode, pf->fid.Unique, comp));
    FSO_ASSERT(pf, comp != 0 && comp[0] != '\0' && !STREQ(comp, "..") && !STREQ(comp, "."));

    int code = 0;
    fsobj *f = 0;

    /* 1. Unlink component name in parent. */
    ViceFid cfid;
    if (pf->dir_Lookup(comp, &cfid) == 0) {
	pf->DemoteLock();
	InferNameRemoval(pf, comp);
	pf->PromoteLock();
    }

    /* 2. Kill any existing object with given key. */
    f = FSDB->Find(key);
    if (f != 0) {
	/* Delete object in case this is a new use of a UFS inode. */
/*	FSO_ASSERT(f, recType == ITYPE_UFS || recType == ITYPE_NFS);*/
	if (recType != ITYPE_UFS && recType != ITYPE_NFS) {
	    eprint("Inferring delete in CreateFso for non-UFS/NFS object: (%x.%x.%x), %s",
		key->Volume, key->Vnode, key->Unique, comp);
	    pf->print(logFile);
	    f->print(logFile);
	    Trace_PrintRecord(recPtr);
	}
	FSO_HOLD(pf); pf->UnLock(WR);
	f->Lock(RD);
	InferDelete(f);
	pf->Lock(WR); FSO_RELE(pf);
    }

    /* 3. Create and initialize the object. */
    f = FSDB->Create(key, WR, FSDB->StdPri(), comp);
    if (f == 0) Choke("Simulate: FSDB::Create failed");
    UpdateCacheStats((type == Directory ? &FSDB->DirAttrStats : &FSDB->FileAttrStats),
		     CREATE, NBLOCKS(sizeof(fsobj)));
    switch(type) {
	/* 
	 * if excluding /tmp files, check if the parent is in /tmp
	 * because the parent/child linkage isn't set up for the child
	 * until local creation.
	 */
	case File:
	    if (!excludeTmp || !IsTmpFile(pf)) {
		code = pf->vol->LogCreate(SimTime, SIMUID, &pf->fid, comp, key, SIMMODE);
		if (code != 0) Choke("Simulate: LogCreate failed");
	    }
	    pf->LocalCreate(SimTime, f, comp, SIMUID, SIMMODE);
	    break;

	case Directory:
	    if (!excludeTmp || !IsTmpFile(pf)) {
		code = pf->vol->LogMkdir(SimTime, SIMUID, &pf->fid, comp, key, SIMMODE);
		if (code != 0) Choke("Simulate: LogMkdir failed");
	    }
	    pf->LocalMkdir(SimTime, f, comp, SIMUID, SIMMODE);
	    break;

	case SymbolicLink:
	    if (!excludeTmp || !IsTmpFile(pf)) {
		code = pf->vol->LogSymlink(SimTime, SIMUID, &pf->fid, comp, "", key, SIMMODE);
		if (code != 0) Choke("Simulate: LogSymlink failed");
	    }
	    pf->LocalSymlink(SimTime, f, comp, "", SIMUID, SIMMODE);
	    f->stat.Length = SYMLINK_SIZE;
	    break;

	case Invalid:
	default:
	    FSO_ASSERT(f, FALSE);
    }

    return(f);
}


void simulator::FlushObject(fsobj *f) {
    LOG(1, ("simulator::FlushObject: (%x.%x.%x)\n",
	    f->fid.Volume, f->fid.Vnode, f->fid.Unique));

    FSO_ASSERT(f, !DIRTY(f));

    if (!FID_EQ(f->pfid, NullFid)) {
	NameRemoval(f);
    }

    f->PromoteLock();
    f->Kill();
    PutFso(&f);
}


void simulator::TranslateFid(ViceFid *OldFid) {
    static Unique_t UniqueCounter = 0x0FFFFFFF;

    ViceFid NewFid = *OldFid;
    NewFid.Unique = --UniqueCounter;

    LOG(1, ("simulator::TranslateFid: (%x.%x.%x) --> (%x.%x.%x)\n",
	    OldFid->Volume, OldFid->Vnode, OldFid->Unique,
	    NewFid.Volume, NewFid.Vnode, NewFid.Unique));

    /* Translate fid in both volume and fsobj databases. */
    volent *v = 0;
    ASSERT(VDB->Get(&v, OldFid->Volume) == 0);
    v->CML.TranslateFid(OldFid, &NewFid);
    VDB->Put(&v);
    ASSERT(FSDB->TranslateFid(OldFid, &NewFid) == 0);
}


void simulator::NameInsertion(ViceFid *pfid, char *comp, fsobj *f) {
    LOG(1, ("simulator::NameInsertion: (%x.%x.%x), %s, (%x.%x.%x)\n",
	    pfid->Volume, pfid->Vnode, pfid->Unique, comp,
	    f->fid.Volume, f->fid.Vnode, f->fid.Unique));

    FSO_ASSERT(f, pfid->Volume == f->fid.Volume);
    FSO_ASSERT(f, FID_EQ(f->pfid, NullFid) || FID_EQ(f->pfid, *pfid));
    FSO_ASSERT(f, !CreatedObject(f->vol, pfid));
    FSO_ASSERT(f, !CreatedObject(f->vol, &f->fid));
    FSO_ASSERT(f, !DirtyDirEntry(f->vol, pfid, comp));

    int code = 0;
    fsobj *pf = 0;

    FSO_HOLD(f); f->UnLock(RD);
    pf = GetFso(pfid, RC_DATA, Directory, DIR_SIZE, 0, 0);
    pf->dir_Create(comp, &f->fid);
    PutFso(&pf);
    f->Lock(RD); FSO_RELE(f);

    f->stat.LinkCount++;
    f->SetParent(pfid->Vnode, pfid->Unique);
    if (HAVEDATA(f) && f->IsDir()) {
	f->dir_Delete("..");
	f->dir_Create("..", pfid);
    }
}


void simulator::NameRemoval(fsobj *f) {
    FSO_ASSERT(f, !DIRTY(f));
    FSO_ASSERT(f, !FID_EQ(f->pfid, NullFid));

    FSO_HOLD(f); f->UnLock(RD);
    fsobj *pf = GetFso(&f->pfid, RC_DATA, Directory, DIR_SIZE, 0, 0);
    f->Lock(RD); FSO_RELE(f);

    FSO_ASSERT(f, (f->stat.VnodeType != File || f->stat.LinkCount >= 1));
    FSO_ASSERT(f, (f->stat.VnodeType != SymbolicLink || f->stat.LinkCount == 1));
    int LinkCount = (f->stat.VnodeType == Directory ? 1 : f->stat.LinkCount);
    for (; LinkCount >= 1; LinkCount--) {
	char comp[MAXNAMLEN];
	FSO_ASSERT(pf, pf->dir_LookupByFid(comp, &f->fid) == 0);
	NameRemoval(pf, comp, f);
    }

    PutFso(&pf);
}


void simulator::NameRemoval(fsobj *pf, char *comp, fsobj *f) {
    LOG(1, ("simulator::NameRemoval: (%x.%x.%x), %s, (%x.%x.%x)\n",
	    pf->fid.Volume, pf->fid.Vnode, pf->fid.Unique, comp,
	    f->fid.Volume, f->fid.Vnode, f->fid.Unique));

    pf->dir_Delete(comp);

    f->stat.LinkCount--;
    if (f->IsDir() || f->stat.LinkCount == 0) {
	/* Detach from old parent if necessary. */
	 if (f->pfso != 0) {
	     f->pfso->DetachChild(f);
	     f->pfso = 0;
	 }

	 f->pfid = NullFid;
	 if (HAVEDATA(f) && f->IsDir()) {
	     f->dir_Delete("..");
	     f->dir_Create("..", &NullFid);
	 }
    }
}


void simulator::InferStore(fsobj *f, unsigned long length) {
    FSO_ASSERT(f, !WRITING(f) && !DYING(f));
    eprint("Inferring store: (%x.%x.%x), %d --> %d",
	   f->fid.Volume, f->fid.Vnode, f->fid.Unique, f->stat.Length, length);

    Begin_VFS(f->fid.Volume, (int)VFSOP_CLOSE, VM_MUTATING);
    f->PromoteLock();
    int old_blocks = (int)BLOCKS(f);
    int new_blocks = (int)NBLOCKS(length);
    UpdateCacheStats(&FSDB->FileDataStats, WRITE, MIN(old_blocks, new_blocks));
    if (length < f->stat.Length)
	UpdateCacheStats(&FSDB->FileDataStats, REMOVE, (old_blocks - new_blocks));
    else if (length > f->stat.Length)
	UpdateCacheStats(&FSDB->FileDataStats, CREATE, (new_blocks - old_blocks));
    FSDB->ChangeDiskUsage(new_blocks - old_blocks);
    if (!excludeTmp || !IsTmpFile(f)) 
        if (f->vol->LogStore(SimTime, SIMUID, &f->fid, length) != 0)
	    Choke("Simulate: LogStore failed");
    f->LocalStore(SimTime, length);
    f->DemoteLock();
    End_VFS(0);
}


fsobj *simulator::InferCreate(ViceFid *key, ViceDataType type, ViceFid *pfid, char *comp) {
    eprint("Inferring %s: (%x.%x.%x), %s, (%x.%x.%x)",
	   (type == File ? "create" : type == Directory ? "mkdir" : "symlink"),
	   pfid->Volume, pfid->Vnode, pfid->Unique,
	   comp, key->Volume, key->Vnode, key->Unique);

    Begin_VFS(pfid->Volume, (int)(type == File ? VFSOP_CREATE :
				  type == Directory ? VFSOP_MKDIR : VFSOP_SYMLINK));
    fsobj *pf = GetFso(pfid, RC_DATA, Directory, DIR_SIZE, 0, 0);
    pf->PromoteLock();
    fsobj *cf = CreateFso(key, type, pf, comp);
    PutFso(&pf);
    End_VFS(0);
    return(cf);
}


void simulator::InferDelete(fsobj *f) {
    if (FID_EQ(f->pfid, NullFid)) {
	eprint("Translating fid in lieu of %s: (%x.%x.%x)",
	       f->stat.VnodeType == Directory ? "rmdir" : "remove",
	       f->fid.Volume, f->fid.Vnode, f->fid.Unique);

	ViceFid OldFid = f->fid;

	/* This gross hack is due to the fact that the trace library */
	/* has opened a file with no corresponding close (I think). */
	if (f->refcnt > 1)
	    InferCloses(f);

	PutFso(&f);
	f = FSDB->Find(&OldFid);
	if (f != 0)
	    TranslateFid(&OldFid);
	return;
    }

    FSO_HOLD(f); f->UnLock(RD);
    fsobj *pf = GetFso(&f->pfid, RC_DATA, Directory, DIR_SIZE, 0, 0);
    f->Lock(RD); FSO_RELE(f);
    FSO_ASSERT(f, (f->stat.VnodeType != File || f->stat.LinkCount >= 1));
    FSO_ASSERT(f, (f->stat.VnodeType != SymbolicLink || f->stat.LinkCount == 1));
    int LinkCount = (f->stat.VnodeType == Directory ? 1 : f->stat.LinkCount);
    for (; LinkCount >= 1; LinkCount--) {
	char comp[MAXNAMLEN];
	FSO_ASSERT(pf, pf->dir_LookupByFid(comp, &f->fid) == 0);
	InferNameRemoval(pf, comp, f);
    }
    PutFso(&pf);
}


void simulator::InferNameRemoval(fsobj *pf, char *comp) {
    ViceFid cfid;
    FSO_ASSERT(pf, pf->dir_Lookup(comp, &cfid) == 0);
    fsobj *f = FSDB->Find(&cfid);
    FSO_ASSERT(pf, f != 0 && HAVESTATUS(f) && FID_EQ(pf->fid, f->pfid));

    ViceDataType ctype = (ViceDataType)f->stat.VnodeType;
    unsigned long clength = (ctype == File ? (unsigned long)-1 :
			     ctype == Directory ? DIR_SIZE : SYMLINK_SIZE);
    FSO_HOLD(pf); pf->UnLock(RD);
    f = GetFso(&cfid, (ctype == Directory ? RC_DATA : RC_STATUS),
	       ctype, clength, &pf->fid, comp);
    pf->Lock(RD); FSO_RELE(pf);
    InferNameRemoval(pf, comp, f);
}


PRIVATE void InferNameRemoval(long hook, char *name, long vnode, long vunique) {
    fsobj *pf = (fsobj *)hook;

    /* Skip over ".." and ".". */
    if (STREQ(name, "..") || STREQ(name, ".")) return;

    Simulator->InferNameRemoval(pf, name);
}

void simulator::InferNameRemoval(fsobj *pf, char *comp, fsobj *f) {
    /* Recursively remove children. */
    if (f->stat.VnodeType == Directory) {
	if (!HAVEDATA(f)) f->dir_MakeDir();
	if (!f->dir_IsEmpty()) {
	    FSO_HOLD(pf); pf->UnLock(RD);
	    ::EnumerateDir((long *)f->data.dir, (int (*)(void * ...))::InferNameRemoval, (long)f);
	    pf->Lock(RD); FSO_RELE(pf);
	}
    }

    if (f->stat.VnodeType == Directory)
	eprint("Inferring rmdir: (%x.%x.%x), %s, (%x.%x.%x)",
	       pf->fid.Volume, pf->fid.Vnode, pf->fid.Unique,
	       comp, f->fid.Volume, f->fid.Vnode, f->fid.Unique);
    else
	eprint("Inferring remove: (%x.%x.%x), %s, (%x.%x.%x), %d",
	       pf->fid.Volume, pf->fid.Vnode, pf->fid.Unique,
	       comp, f->fid.Volume, f->fid.Vnode, f->fid.Unique, f->stat.LinkCount);

    Begin_VFS(pf->fid.Volume,
	      (int)(f->stat.VnodeType == Directory ? VFSOP_RMDIR : VFSOP_REMOVE));
    pf->PromoteLock();
    f->PromoteLock();
    if (f->stat.VnodeType == Directory) {
	FSO_ASSERT(f, f->dir_IsEmpty());
	if (!excludeTmp || !IsTmpFile(f)) 
	    if (pf->vol->LogRmdir(SimTime, SIMUID, &pf->fid, comp, &f->fid) != 0)
	        Choke("Simulate: LogRmdir failed");
	pf->LocalRmdir(SimTime, comp, f);
    }
    else {
	if (!excludeTmp || !IsTmpFile(f)) 
	    if (pf->vol->LogRemove(SimTime, SIMUID, &pf->fid, comp, &f->fid, f->stat.LinkCount) != 0)
	        Choke("Simulate: LogRemove failed");
	pf->LocalRemove(SimTime, comp, f);
    }

    if (f->stat.VnodeType != File || f->stat.LinkCount == 0) {
	char tcomp[MAXNAMLEN];
	FSO_ASSERT(pf, pf->dir_LookupByFid(tcomp, &f->fid) == ENOENT);
    }

    pf->DemoteLock();
    PutFso(&f);
    End_VFS(0);
}


struct ionr_hook {
    fsobj *pf;
    ViceFid *cfid;
    char *comp;
};

PRIVATE void InferOtherNameRemoval(long hook, char *name, long vnode, long vunique) {
    struct ionr_hook *ionr_hook = (struct ionr_hook *)hook;

    if (vnode == ionr_hook->cfid->Vnode &&
	vunique == ionr_hook->cfid->Unique &&
	!STREQ(name, ionr_hook->comp))
	Simulator->InferNameRemoval(ionr_hook->pf, name);
}

/* Remove all names for cf in pf OTHER than comp. */
void simulator::InferOtherNameRemoval(fsobj *pf, fsobj *cf, char *comp) {
    FSO_ASSERT(cf, FID_EQ(pf->fid, cf->pfid) && cf->stat.VnodeType == File && cf->stat.LinkCount > 1);

    struct ionr_hook hook;
    hook.pf = pf;
    hook.cfid = &cf->fid;
    hook.comp = comp;
    FSO_HOLD(cf); cf->UnLock(RD);

    ::EnumerateDir((long *)pf->data.dir, (int (*)(void * ...))::InferOtherNameRemoval, (long)&hook);
    cf->Lock(RD); FSO_RELE(cf);
    FSO_ASSERT(cf, cf->stat.LinkCount == 1);
}


void simulator::InferRename(fsobj *sf, ViceFid *tpfid, char *toname) {
    FSO_ASSERT(sf, sf->stat.VnodeType != File || (!FID_EQ(sf->pfid, *tpfid) && sf->stat.LinkCount == 1));
    FSO_HOLD(sf); sf->UnLock(RD);

    Begin_VFS(tpfid->Volume, (int)VFSOP_RENAME);
    fsobj *spf = GetFso(&sf->pfid, RC_DATA, Directory, DIR_SIZE, 0, 0);
    char fromname[MAXNAMLEN];
    FSO_ASSERT(spf, spf->dir_LookupByFid(fromname, &sf->fid) == 0);
    FSO_HOLD(spf); spf->UnLock(RD);
    fsobj *tpf;
    if (FID_EQ(sf->pfid, *tpfid))
	tpf = spf;
    else {
	tpf = GetFso(tpfid, RC_DATA, Directory, DIR_SIZE, 0, 0);
    }
    ViceFid tfid;
    FSO_ASSERT(tpf, tpf->dir_Lookup(toname, &tfid) == ENOENT);
    tfid = NullFid;

    eprint("Inferring rename: (%x.%x.%x), %s, (%x.%x.%x), %s, (%x.%x.%x)",
	   spf->fid.Volume, spf->fid.Vnode, spf->fid.Unique, fromname,
	   tpf->fid.Volume, tpf->fid.Vnode, tpf->fid.Unique, toname,
	   sf->fid.Volume, sf->fid.Vnode, sf->fid.Unique);

    if (spf != tpf) tpf->PromoteLock();
    spf->Lock(WR); FSO_RELE(spf);
    sf->Lock(WR); FSO_RELE(sf);
    if (!excludeTmp) {
	if (tpf->vol->LogRename(SimTime, SIMUID, &sf->pfid, fromname,
				tpfid, toname, &sf->fid, &tfid, 1) != 0)
	    Choke("Simulate: LogRename failed");
    } else 
	LogTmpRename(SimTime, spf, fromname, sf, tpf, toname, 0);

    tpf->LocalRename(SimTime, spf, fromname, sf, toname, 0);
    sf->DemoteLock();

    if (tpf != spf) PutFso(&tpf);
    PutFso(&spf);
    End_VFS(0);
}


void simulator::InferLink(fsobj *sf, ViceFid *tpfid, char *toname) {
    FSO_ASSERT(sf, FID_EQ(sf->pfid, *tpfid));
    FSO_HOLD(sf); sf->UnLock(RD);

    eprint("Inferring link: (%x.%x.%x), %s, (%x.%x.%x)",
	   tpfid->Volume, tpfid->Vnode, tpfid->Unique, toname,
	   sf->fid.Volume, sf->fid.Vnode, sf->fid.Unique);

    Begin_VFS(tpfid->Volume, (int)VFSOP_LINK);
    fsobj *tpf = GetFso(tpfid, RC_DATA, Directory, DIR_SIZE, 0, 0);
    tpf->PromoteLock();
    sf->Lock(WR); FSO_RELE(sf);

    if (!excludeTmp || !IsTmpFile(sf))
	if (tpf->vol->LogLink(SimTime, SIMUID, tpfid, toname, &sf->fid) != 0)
	    Choke("Simulate: LogLink failed");
    tpf->LocalLink(SimTime, toname, sf);
    sf->DemoteLock();

    PutFso(&tpf);
    End_VFS(0);
}


/* This shouldn't be needed! -JJK */
void simulator::InferCloses(fsobj *f) {
    FSO_ASSERT(f, (f->refcnt == 1 + f->openers) && (f->Execers == 0));

    eprint("Inferring %d closes for (%x.%x.%x)",
	f->openers, f->fid.Volume, f->fid.Vnode, f->fid.Unique);
    f->print(stderr);

    f->PromoteLock();

    /* Close readers first. */
    while (f->openers > f->Writers) {
	FSO_ASSERT(f, f->Close(0, 0, SIMUID) == 0);
    }

    /* Then close writers. */
    while (f->Writers > 0) {
	FSO_ASSERT(f, f->Close(1, 0, SIMUID) == 0);

	/* This is copied from DFS_CLOSE handler above! */	
	if (!WRITING(f) && !DYING(f)) {
	    /* This duplicates code in fsobj::Close()! */
	    FSO_ASSERT(f, f->stat.Length != (unsigned long)-1);
	    int FinalSize = (int)f->stat.Length;
	    int old_blocks = (int)BLOCKS(f);
	    int new_blocks = NBLOCKS(FinalSize);
	    UpdateCacheStats(&FSDB->FileDataStats, WRITE, MIN(old_blocks, new_blocks));
	    if (FinalSize < f->stat.Length)
		UpdateCacheStats(&FSDB->FileDataStats, REMOVE, (old_blocks - new_blocks));
	    else if (FinalSize > f->stat.Length)
		UpdateCacheStats(&FSDB->FileDataStats, CREATE, (new_blocks - old_blocks));
	    FSDB->ChangeDiskUsage(NBLOCKS(FinalSize));

	    if (!excludeTmp || !IsTmpFile(f)) 
		if (f->vol->LogStore(SimTime, SIMUID, &f->fid, FinalSize) != 0)
		    Choke("Simulate: LogStore failed");
	    f->LocalStore(SimTime, FinalSize);
	}
    }

    FSO_ASSERT(f, f->refcnt == 1 && f->openers == 0);
    f->DemoteLock();
}


/* Lily's macro doesn't do quite what I want. */
PRIVATE void XlateFid(generic_fid_t *_gfid_, ViceFid *_cfid_, ViceDataType type) {
    if (type != File && type != Directory && type != SymbolicLink)
	Choke("XlateFid: bogus vnode type (%d)", type);

#ifndef	ITYPE_CFS
#define ITYPE_CFS	5
#endif
    if (_gfid_->tag != ITYPE_AFS && _gfid_->tag != ITYPE_CFS && _gfid_->tag != ITYPE_UFS && _gfid_->tag != ITYPE_NFS)
	Choke("XlateFid: bogus fid type (%d)", _gfid_->tag);
    MAKE_CFS_FID((*_gfid_), (*_cfid_));

    /* Need guarantee that:  type = Directory <---> Vnode is Odd. */
    if (_gfid_->tag == ITYPE_UFS || _gfid_->tag == ITYPE_NFS) {
	_cfid_->Vnode *= 2;
	if (type == Directory) _cfid_->Vnode += 1;
    }
}


PRIVATE void GetComponent(char *path, char *comp) {
    char *cp = rindex(path, '/');
    if (cp == 0) cp = path;
    else cp++;

    /* Do @sys conversion if necessary. */
    if (STREQ(cp, "@sys"))
	strcpy(comp, SimAtSys);
    else
	strcpy(comp, cp);
}


/* update the name or symlink fields in an fso with info from a record */
/* these fso fields are in RVM */
PRIVATE void UpdateFsoName(char **f, char *r) {
    int len = (r ? (int) strlen(r) : 0) + 1;
    if (*f) RVMLIB_REC_FREE(*f);
    *f = (char *)RVMLIB_REC_MALLOC(len);
    RVMLIB_SET_RANGE(*f, len);
    if (r) strcpy(*f, r);
    else *f[0] = '\0';
}


/* Temporary hack to cope with the fact that closes of devices are sometimes passed through! */
PRIVATE ViceDataType VTTOFT(unsigned short vt) {
    switch (vt) {
	case VREG: return(File);
	case VDIR: return(Directory);
	case VLNK: return(SymbolicLink);

	default:
	    {
	    eprint("VTTOFT: Coercing vnode type %o to File", vt);
	    return(File);
	    }
    }
}


/*  *****  Fsobj/Volume Routines Supporting Simulation  *****  */

int simulator::CreatedObject(volent *v, ViceFid *Fid) {
    int created = 0;

    cml_iterator next(v->CML, CommitOrder, Fid);
    cmlent *m;
    while (m = next()) {
	switch(m->opcode) {
	    case ViceNewStore_OP:
	    case ViceUtimes_OP:
	    case ViceChown_OP:
	    case ViceChmod_OP:
	    case ViceRemove_OP:
	    case ViceLink_OP:
	    case ViceRename_OP:
	    case ViceRemoveDir_OP:
		break;

	    case ViceCreate_OP:
		if (FID_EQ(*Fid, m->u.u_create.CFid))
		    created = 1;
		break;

	    case ViceMakeDir_OP:
		if (FID_EQ(*Fid, m->u.u_mkdir.CFid))
		    created = 1;
		break;

	    case ViceSymLink_OP:
		if (FID_EQ(*Fid, m->u.u_symlink.CFid))
		    created = 1;
		break;

	    default:
		m->print(logFile);
		Choke("simulator::CreatedObject: bogus opcode (%d)", m->opcode);
	}
	if (created) break;
    }

    /* Sanity check.  This routine should go away soon! */
    fsobj *f = FSDB->Find(Fid);
    FID_ASSERT(*Fid, f != 0);
    FSO_ASSERT(f, ((excludeTmp && IsTmpFile(f)) || created == f->flags.created));

    return(created);
}


int simulator::DirtyDirEntry(volent *v, ViceFid *PFid, char *Name) {
    cml_iterator next(v->CML, CommitOrder, PFid);
    cmlent *m;
    while (m = next()) {
	switch(m->opcode) {
	    case ViceNewStore_OP:
	    case ViceUtimes_OP:
	    case ViceChown_OP:
	    case ViceChmod_OP:
		break;

	    case ViceCreate_OP:
		if (FID_EQ(*PFid, m->u.u_create.PFid) &&
		    STREQ(Name, (char *)m->u.u_create.Name))
		    return(1);
		break;

	    case ViceRemove_OP:
		if (FID_EQ(*PFid, m->u.u_remove.PFid) &&
		    STREQ(Name, (char *)m->u.u_remove.Name))
		    return(1);
		break;

	    case ViceLink_OP:
		if (FID_EQ(*PFid, m->u.u_link.PFid) &&
		    STREQ(Name, (char *)m->u.u_link.Name))
		    return(1);
		break;

	    case ViceRename_OP:
		if (FID_EQ(*PFid, m->u.u_rename.SPFid) &&
		    STREQ(Name, (char *)m->u.u_rename.OldName))
		    return(1);
		if (FID_EQ(*PFid, m->u.u_rename.TPFid) &&
		    STREQ(Name, (char *)m->u.u_rename.NewName))
		    return(1);
		break;

	    case ViceMakeDir_OP:
		if (FID_EQ(*PFid, m->u.u_mkdir.PFid) &&
		    STREQ(Name, (char *)m->u.u_mkdir.Name))
		    return(1);
		break;

	    case ViceRemoveDir_OP:
		if (FID_EQ(*PFid, m->u.u_rmdir.PFid) &&
		    STREQ(Name, (char *)m->u.u_rmdir.Name))
		    return(1);
		break;

	    case ViceSymLink_OP:
		if (FID_EQ(*PFid, m->u.u_symlink.PFid) &&
		    STREQ(Name, (char *)m->u.u_symlink.OldName))
		    return(1);
		break;

	    default:
		m->print(logFile);
		Choke("simulator::DirtyDirEntry: bogus opcode (%d)", m->opcode);
	}
    }

    return(0);
}


void simulator::OutputSimulationFiles(volent *v, FILE *sfp, FILE *rfp) {
    if (v->CML.count() == 0) {
/*	ValidateSimulationState();*/
	return;
    }

    fprintf(sfp, "mkdir %u\n", v->vid);
/*    ValidateSimulationState();*/
    UnperformNamingOperations(v);
/*    ValidateSimulationState();*/
    OutputSkeletonFile(v, sfp);
    fflush(sfp);
/*    ValidateSimulationState();*/
    OutputReplayFile(v, rfp);
    fflush(rfp);
}


void simulator::UnperformNamingOperations(volent *v) {
    cml_iterator next(v->CML, AbortOrder);
    cmlent *m;
    while (m = next()) {
	switch(m->opcode) {
	    case ViceNewStore_OP:
	    case ViceUtimes_OP:
	    case ViceChown_OP:
	    case ViceChmod_OP:
		break;

	    case ViceCreate_OP:
		UnperformInsertion(&m->u.u_create.PFid,
				   (char *)m->u.u_create.Name, &m->u.u_create.CFid);
		break;

	    case ViceRemove_OP:
		UnperformRemoval(&m->u.u_remove.PFid,
				 (char *)m->u.u_remove.Name, &m->u.u_remove.CFid);
		break;

	    case ViceLink_OP:
		UnperformInsertion(&m->u.u_link.PFid,
				   (char *)m->u.u_link.Name, &m->u.u_link.CFid);
		break;

	    case ViceRename_OP:
		UnperformInsertion(&m->u.u_rename.TPFid,
				   (char *)m->u.u_rename.NewName, &m->u.u_rename.SFid);
		UnperformRemoval(&m->u.u_rename.SPFid,
				 (char *)m->u.u_rename.OldName, &m->u.u_rename.SFid);
		break;

	    case ViceMakeDir_OP:
		UnperformInsertion(&m->u.u_mkdir.PFid,
				   (char *)m->u.u_mkdir.Name, &m->u.u_mkdir.CFid);
		break;

	    case ViceRemoveDir_OP:
		UnperformRemoval(&m->u.u_rmdir.PFid,
				 (char *)m->u.u_rmdir.Name, &m->u.u_rmdir.CFid);
		break;

	    case ViceSymLink_OP:
		UnperformInsertion(&m->u.u_symlink.PFid,
				   (char *)m->u.u_symlink.NewName, &m->u.u_symlink.CFid);
		break;

	    default:
		Choke("simulator::UnperformNamingOperations: bogus opcode (%d)", m->opcode);
	}
    }

    /* Sanity check.  It should now be the case that every child of a created directory was also created! */
    {

    }
}


void simulator::UnperformInsertion(ViceFid *pfid, char *comp, ViceFid *cfid) {
    fsobj *pf = FSDB->Find(pfid);
    FID_ASSERT(*pfid, pf != 0);
    fsobj *cf = FSDB->Find(cfid);
    FID_ASSERT(*cfid, cf != 0);

    FSO_ASSERT(cf, FID_EQ(cf->pfid, *pfid));
    ViceFid tfid;
    FSO_ASSERT(pf, pf->dir_Lookup(comp, &tfid) == 0);
    FSO_ASSERT(pf, FID_EQ(tfid, *cfid));

    pf->dir_Delete(comp);
    char tcomp[MAXNAMLEN];
    if (pf->dir_LookupByFid(tcomp, cfid) == 0) {
	/* There are more links to this file. */
	FSO_ASSERT(cf, cf->IsFile());
    }
    else {
	cf->pfid = NullFid;
    }
}


void simulator::UnperformRemoval(ViceFid *pfid, char *comp, ViceFid *cfid) {
    fsobj *pf = FSDB->Find(pfid);
    FID_ASSERT(*pfid, pf != 0);
    fsobj *cf = FSDB->Find(cfid);
    FID_ASSERT(*cfid, cf != 0);

    FSO_ASSERT(cf, FID_EQ(cf->pfid, NullFid) || FID_EQ(cf->pfid, *pfid));
    ViceFid tfid;
    FSO_ASSERT(pf, pf->dir_Lookup(comp, &tfid) == ENOENT);

    pf->dir_Create(comp, cfid);
    cf->pfid = *pfid;
}


void simulator::OutputSkeletonFile(volent *v, FILE *sfp) {
    /* We need to output a {create, link, mkdir, symlink} for every name corresponding to a */
    /* dirty object.  In the the case of directories, we need to output a mkdir if any descendent */
    /* is dirty, not just the named object.  The names must be output in top-down fashion. */
    /* The algorithm has two passes through the fsobjs.  In the first pass, ``dirtiness'' is */
    /* propagated upwards through the hierarchy by setting a ``marked'' flag in ancestor fsobjs.  */
    /* In the second pass, ``root'' nodes are depth-first searched, with marked nodes being output. */

    /* Pass 1:  Marking */
    {
	fso_vol_iterator next(NL, v);
	fsobj *f;
	while (f = next())
	    if (DIRTY(f))
		MarkAncestors(f);
    }

    /* Pass 2:  Outputting */
    {
	fso_vol_iterator next(NL, v);
	fsobj *f;
	while (f = next())
	    if (FID_EQ(f->pfid, NullFid))
		Skeletize(f, 0, sfp);
    }
}


void simulator::MarkAncestors(fsobj *f) {
    /* Mark this fid and all its ancestors. */
    if (f->flags.marked) return;
    f->flags.marked = 1;
    if (FID_EQ(f->pfid, NullFid)) return;
    fsobj *pf = FSDB->Find(&f->pfid);
    FID_ASSERT(f->pfid, pf != 0);
    MarkAncestors(pf);
}


struct SkeletizeHook {
    VolumeId vid;
    FILE *sfp;
};

PRIVATE void Skeletize(long hook, char *name, long vnode, long vunique) {
    SkeletizeHook *s_hook = (SkeletizeHook *)hook;

    /* Skip over ".." and ".". */
    if (STREQ(name, "..") || STREQ(name, ".")) return;

    ViceFid fid;
    fid.Volume = s_hook->vid;
    fid.Vnode = vnode;
    fid.Unique = vunique;
    fsobj *f = FSDB->Find(&fid);
    FID_ASSERT(fid, f != 0);
    Simulator->Skeletize(f, name, s_hook->sfp);
}

void simulator::Skeletize(fsobj *f, char *comp, FILE *sfp) {
    if (!f->flags.marked || f->flags.created) return;

    char path1[MAXPATHLEN];
    if (comp != 0)
	GetPath(&f->pfid, path1, comp);
    else
	GetPath(&f->fid, path1);
    switch(f->stat.VnodeType) {
	case File:
	    {
	    /* Determine whether we need to ``create'' or ``link.'' */
	    if (comp != 0) {
		fsobj *pf = FSDB->Find(&f->pfid);
		FID_ASSERT(f->pfid, pf != 0);
		char tcomp[MAXNAMLEN];
		FSO_ASSERT(pf, pf->dir_LookupByFid(tcomp, &f->fid) == 0);
		if (!STREQ(comp, tcomp)) {
		    char path2[MAXPATHLEN];
		    GetPath(&f->pfid, path2, tcomp);
		    fprintf(sfp, "link %s %s\n", path2, path1);
		    break;
		}
	    }

	    fprintf(sfp, "create %s\n", path1);
	    }
	    break;

	case Directory:
	    fprintf(sfp, "mkdir %s\n", path1);
	    break;

	case SymbolicLink:
	    fprintf(sfp, "symlink %s\n", path1);
	    break;

	case Invalid:
	default:
	      FSO_ASSERT(f, FALSE);
    }
    fflush(sfp);

    if (f->stat.VnodeType == Directory && HAVEDATA(f)) {
	SkeletizeHook hook;
	hook.vid = f->fid.Volume;
	hook.sfp = sfp;

    }
}


void simulator::OutputReplayFile(volent *v, FILE *rfp) {
    cml_iterator next(v->CML, CommitOrder);
    cmlent *m;
    while (m = next()) {
	char path1[MAXPATHLEN];
	char path2[MAXPATHLEN];
	switch(m->opcode) {
	    case ViceNewStore_OP:
		GetPath(&m->u.u_store.Fid, path1);
		fprintf(rfp, "store %s %d\n", path1, m->u.u_store.Length);
		break;

	    case ViceUtimes_OP:
		GetPath(&m->u.u_utimes.Fid, path1);
		fprintf(rfp, "utimes %s\n", path1);
		break;

	    case ViceChown_OP:
		GetPath(&m->u.u_chown.Fid, path1);
		fprintf(rfp, "chown %s\n", path1);
		break;

	    case ViceChmod_OP:
		GetPath(&m->u.u_chmod.Fid, path1);
		fprintf(rfp, "chmod %s\n", path1);
		break;

	    case ViceCreate_OP:
		GetPath(&m->u.u_create.PFid, path1, (char *)m->u.u_create.Name);
		fprintf(rfp, "create %s\n", path1);
		ReperformInsertion(&m->u.u_create.PFid,
				   (char *)m->u.u_create.Name, &m->u.u_create.CFid);
		break;

	    case ViceRemove_OP:
		GetPath(&m->u.u_remove.PFid, path1, (char *)m->u.u_remove.Name);
		fprintf(rfp, "unlink %s\n", path1);
		ReperformRemoval(&m->u.u_remove.PFid,
				 (char *)m->u.u_remove.Name, &m->u.u_remove.CFid);
		break;

	    case ViceLink_OP:
		GetPath(&m->u.u_link.CFid, path1);
		GetPath(&m->u.u_link.PFid, path2, (char *)m->u.u_link.Name);
		fprintf(rfp, "link %s %s\n", path1, path2);
		ReperformInsertion(&m->u.u_link.PFid,
				   (char *)m->u.u_link.Name, &m->u.u_link.CFid);
		break;

	    case ViceRename_OP:
		GetPath(&m->u.u_rename.SPFid, path1, (char *)m->u.u_rename.OldName);
		GetPath(&m->u.u_rename.TPFid, path2, (char *)m->u.u_rename.NewName);
		fprintf(rfp, "rename %s %s\n", path1, path2);
		ReperformRemoval(&m->u.u_rename.SPFid,
				 (char *)m->u.u_rename.OldName, &m->u.u_rename.SFid);
		ReperformInsertion(&m->u.u_rename.TPFid,
				   (char *)m->u.u_rename.NewName, &m->u.u_rename.SFid);
		break;

	    case ViceMakeDir_OP:
		GetPath(&m->u.u_mkdir.PFid, path1, (char *)m->u.u_mkdir.Name);
		fprintf(rfp, "mkdir %s\n", path1);
		ReperformInsertion(&m->u.u_mkdir.PFid,
				   (char *)m->u.u_mkdir.Name, &m->u.u_mkdir.CFid);
		break;

	    case ViceRemoveDir_OP:
		GetPath(&m->u.u_rmdir.PFid, path1, (char *)m->u.u_rmdir.Name);
		fprintf(rfp, "rmdir %s\n", path1);
		ReperformRemoval(&m->u.u_rmdir.PFid,
					    (char *)m->u.u_rmdir.Name, &m->u.u_rmdir.CFid);
		break;

	    case ViceSymLink_OP:
		GetPath(&m->u.u_symlink.PFid, path1, (char *)m->u.u_symlink.NewName);
		fprintf(rfp, "symlink %s\n", path1);
		ReperformInsertion(&m->u.u_symlink.PFid,
					      (char *)m->u.u_symlink.NewName, &m->u.u_symlink.CFid);
		break;

	    default:
		Choke("simulator::OutputReplayFile: bogus opcode (%d)", m->opcode);
	}
	fflush(rfp);
    }
}


void simulator::ReperformInsertion(ViceFid *pfid, char *comp, ViceFid *cfid) {
    UnperformRemoval(pfid, comp, cfid);
}


void simulator::ReperformRemoval(ViceFid *pfid, char *comp, ViceFid *cfid) {
    UnperformInsertion(pfid, comp, cfid);
}


void simulator::GetPath(ViceFid *fid, char *buf, char *trailer) {
    fsobj *f = FSDB->Find(fid);
    FID_ASSERT(*fid, f != 0);

    if (FID_EQ(f->pfid, NullFid)) {
	sprintf(buf, "%u/%u.%u", f->fid.Volume, f->fid.Vnode, f->fid.Unique);
    }
    else {
    /* Get parent's path (without trailer). */
	GetPath(&f->pfid, buf);

    /* Add "/". */
	buf += strlen(buf);
	strcpy(buf, "/");

    /* Add component. */
	buf++;
	fsobj *pf = FSDB->Find(&f->pfid);
	FSO_ASSERT(pf, pf->dir_LookupByFid(buf, fid) == 0);
    }

    /* Add trailer. */
    if (trailer != 0) {
	strcat(buf, "/");
	strcat(buf, trailer);
    }
}


/* 
 * the tmp fid is given to us in device.inode format.
 * convert it from a string, and translate it.  The
 * tmp fid should be the actual tmp directory, not a 
 * symlink.
 */
PRIVATE void ParseTmpFid() {
    tmpFid = NullFid;

    if (sscanf(SimTmpFid, "%lx.%lx", &tmpFid.Volume, &tmpFid.Vnode) == 2) {
	    eprint("excluding /tmp files, fid is %s", SimTmpFid);

	    excludeTmp = 1;
	    tmpFid.Vnode *= 2; tmpFid.Vnode += 1;
	    tmpFid.Unique = 0xffffffff;          
    }
}

/* determine if an fsobj is /tmp or a descendant of /tmp */
int simulator::IsTmpFile(fsobj *suspect) {
    fsobj *tmpfso = 0;

    tmpfso = FSDB->Find(&tmpFid);
    if (!tmpfso) return(0);

    /* 
     * traverse backwards from suspect fso, crossing mount
     * points if necessary.
     */
    fsobj *curfso = suspect;
    while (curfso) {
	if (curfso == tmpfso)
	    return(1);

	if (curfso->IsRoot()) 
	    curfso = curfso->u.mtpoint;
        else
	    curfso = curfso->pfso;
    }
    return(0);
}
