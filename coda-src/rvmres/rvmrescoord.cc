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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/rvmres/Attic/rvmrescoord.cc,v 4.3.8.1 1998/10/05 16:54:47 rvb Exp $";
#endif /*_BLURB_*/






#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <assert.h>
#include <stdio.h>
#include <struct.h>
#include <lwp.h>
#include <rpc2.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>
#include <olist.h>
#include <errors.h>
#include <res.h>
#include <srv.h>
#include <inconsist.h>
#include <rvmdir.h>
#include <vlist.h>
#include <vrdb.h>
#include <resutil.h>
#include <rescomm.h>
#include <resforce.h>
#include <timing.h>
#include "rvmrestiming.h"
#include "resstats.h"

// ********** Private Routines *************
PRIVATE int ComparePhase3Status(res_mgrpent *, int *, ViceStatus *, ViceStatus *);
PRIVATE int RegDirResRequired(res_mgrpent *, ViceFid *, ViceVersionVector **, ResStatus **, int *);
PRIVATE char *CoordPhase2(res_mgrpent *, ViceFid *, int *, int *, int *, unsigned long *,dirresstats *);
PRIVATE int CoordPhase3(res_mgrpent*, ViceFid*, char*, int, int, ViceVersionVector**, dlist*, ResStatus**, unsigned long*, int*);
PRIVATE int CoordPhase4(res_mgrpent *, ViceFid *, unsigned long *, int *);
PRIVATE int CoordPhase34(res_mgrpent *, ViceFid *, dlist *, int *, int *);
PRIVATE int AlreadyIncGroup(ViceVersionVector **, int);
PRIVATE void AllocateBufs(res_mgrpent *, char **, int *);
PRIVATE void DeAllocateBufs(char **);
PRIVATE char *ConcatLogs(res_mgrpent *, char **, RPC2_Integer *, RPC2_Integer *, int *, int *);
PRIVATE int ResolveInc(res_mgrpent *, ViceFid *, ViceVersionVector **);
PRIVATE int CompareDirContents(SE_Descriptor *, ViceFid *);
PRIVATE int CompareDirStatus(ViceStatus *, res_mgrpent *, ViceVersionVector **);
PRIVATE DumpDirContents(SE_Descriptor *, ViceFid *);
PRIVATE PrintPaths(int *sizes, ResPathElem **paths, res_mgrpent *);
PRIVATE int AllIncGroup(ViceVersionVector **, int );
PRIVATE void UpdateStats(ViceFid *, dirresstats *);
PRIVATE void UpdateStats(ViceFid *, int , int );

// * Dir Resolution with logs in RVM
// * This consists of 4 phases
// *	Phase 1: Locking
// *		Volume gets locked at each subordinate
// *		Each server returns status of path from root
// *		Coordinator makes sure that the ancestors are all equal 
// *
// *	Phase 2: Log Collection and Merging
// *		Each subordinate returns the log of related objects 
// *			as a byte stream
// *		Coordinator merges these logs together into a big 
// *			linear buffer
// *
// *	Phase 3: Log Distribution and Compensation
// *		Coordinator distributes the combined logs 
// *		Subordinates parse logs, compute compensating operations
// *		and perform the operations
// * 		Subs return list of inconsistencies, if any, that arose.
// *
// *	Phase 3.5: (Phase34)
// *		Coordinator distributes list of inconsistencies to all sites
// *		Sub's make sure that inconsistency is present  
// *	Phase 4: 
// *		Coordinator ships out a new storeid for each subordinate's replica
// *		Subs stamp their replicas with storeid and unlock volume (not done currently)
// *			(for now subs also return directory contents)
// *		Coordinator compares contents and makes sure directories are equal
// *		(This isn't necessary in a production server)
// *     	Phase 5:
// *		In case of problems it marks the replicas inconsistent
//

long RecovDirResolve(res_mgrpent *mgrp, ViceFid *Fid, ViceVersionVector **VV, 
		     ResStatus **rstatusp, int *sizes, int *pathsizes, 
		     ResPathElem **paths, int checkpaths) {
    int reserror = EINCONS;
    char *AllLogs = NULL;
    int totalentries = 0;
    int totalsize = 0;
    int dirlengths[VSG_MEMBERS];
    unsigned long succFlags[VSG_MEMBERS];
    dlist *inclist = NULL;
    dirresstats drstats;
    long retval = 0;
    int dirdepth = -1;
    int noinc = -1;		// used only for updating res stats (dept statistics)
    
    // res stats stuff 
    {
	drstats.dir_nresolves++;
	if (mgrp->IncompleteVSG()) drstats.dir_incvsg++;
    }
    
    LogMsg(0, SrvDebugLevel, stdout,
	   "Entering RecovDirResolve (0x%x.%x.%x)\n",
	   Fid->Volume, Fid->Vnode, Fid->Unique);

    // Check if object can be resolved 
    if (checkpaths) {
	if (SrvDebugLevel >= 10)
	    PrintPaths(pathsizes, paths, mgrp);
	ViceFid UnEqFid;
	ViceVersionVector *UnEqVV[VSG_MEMBERS];
	ResStatus *UnEqResStatus[VSG_MEMBERS];
	if (ComparePath(pathsizes, paths, mgrp, &dirdepth, 
			&UnEqFid, &UnEqVV[0], &UnEqResStatus[0])) {
	    LogMsg(0, SrvDebugLevel, stdout,
		   "%x.%x has ancestors that need to be resolved - therefore not resolving\n",
		   Fid->Vnode, Fid->Unique);
	    if (UnEqFid.Vnode != 0 && UnEqFid.Unique != 0) {
		UnEqFid.Volume = Fid->Volume;
		LogMsg(0, SrvDebugLevel, stdout, 
		       "resolving %x.%x.%x  instead\n", 
		       UnEqFid.Volume, UnEqFid.Vnode, UnEqFid.Unique);
		RecovDirResolve(mgrp, &UnEqFid, &UnEqVV[0], &UnEqResStatus[0],
				sizes, NULL, NULL, 0);
	    }
	    drstats.dir_problems++;
	    reserror = 0;
	    retval = EINVAL;
	    goto Exit;
	}
    }
    // Check if Regular Directory Resolution is required 
    {	
	int errorCode = 0;
	if (!RegDirResRequired(mgrp, Fid, VV, rstatusp, &errorCode)) {
	    LogMsg(0, SrvDebugLevel, stdout, 
		   "RecovDirResolve: No need for Resolution\n");
	    if (errorCode == EINCONS) {
		// undo statistics collection stuff;
		drstats.dir_nresolves--;
		drstats.dir_incvsg = 0;
		retval = ResolveInc(mgrp, Fid, VV);
		reserror = 0;
		goto Exit;
	    }
	    
	    if (errorCode) {
		retval = EINCONS;
		goto Exit;	// mark object inconsistent
	    }
	    // for statistics collection
	    drstats.dir_nowork++;
	    drstats.dir_succ++;
	    noinc = 1;

	    retval = 0;
	    reserror = 0;
	    goto Exit;
	}
    }

    // Phase 1 has already been done by ViceResolve
    // XXXXX needs to be modified
    {
	// lock volume
    }
    
    // Phase 2 
    {
	PROBE(tpinfo, RecovCoorP2Begin);
	AllLogs = CoordPhase2(mgrp, Fid, &totalentries, sizes, 
			      &totalsize, succFlags, &drstats);
	PROBE(tpinfo, RecovCoorP2End);
	if (!AllLogs) {
	    LogMsg(0, SrvDebugLevel, stdout,
		   "RecovDirResolve: Error during phase2\n");
	    goto Exit;
	}
	PollAndYield();
    }
    // Phase3
    {	
	PROBE(tpinfo, RecovCoorP3Begin);
	inclist = new dlist((CFN)CompareIlinkEntry);
	if (CoordPhase3(mgrp, Fid, AllLogs, totalsize, totalentries, VV, 
			inclist, rstatusp, succFlags, dirlengths)) {
	    LogMsg(0, SrvDebugLevel, stdout,
		   "RecovDirResolve: Error during phase 3\n");
	    goto Exit;
	}
	PROBE(tpinfo, RecovCoorP3End);

    }

    // Phase34
    {
	PROBE(tpinfo, RecovCoorP34Begin);
	if (CoordPhase34(mgrp, Fid, inclist, dirlengths, &noinc)) {
	    LogMsg(0, SrvDebugLevel, stdout,
		   "RecovDirResolve: Error during phase 34\n");
	    goto Exit;
	}	    
	PROBE(tpinfo, RecovCoorP34End);
    }
    
    // Phase 4
    {
	PROBE(tpinfo, RecovCoorP4Begin);
	if (!CoordPhase4(mgrp, Fid, succFlags, dirlengths)) {
	    reserror = 0;
	    noinc = 1;
	    drstats.dir_succ++;
	}
	PROBE(tpinfo, RecovCoorP4End);
    }
  Exit:
    // mark object inconsistent in case of error 
    // Phase5
    if (reserror) {
	MRPC_MakeMulti(MarkInc_OP, MarkInc_PTR, VSG_MEMBERS,
		       mgrp->rrcc.handles, mgrp->rrcc.retcodes, 
		       mgrp->rrcc.MIp, 0, 0, Fid);
	noinc = 0;
	drstats.dir_conf++;
	
    }
    // clean up
    {
	if (AllLogs) free(AllLogs);
	if (inclist) CleanIncList(inclist);
    }
    LogMsg(1, SrvDebugLevel, stdout,
	   "RecovDirResolve returns %d\n", retval);
    UpdateStats(Fid, &drstats);
    if ((dirdepth != -1) && (noinc != -1))
	UpdateStats(Fid, noinc, dirdepth);
    return(retval);
}

PRIVATE int RegDirResRequired(res_mgrpent *mgrp, ViceFid *Fid, 
			      ViceVersionVector **VV, ResStatus **rstatusp, 
			      int *errorCode) {
    LogMsg(1, SrvDebugLevel, stdout,
	   "Entering RegDirResRequired for (0x%x.%x.%x)\n",
	   Fid->Volume, Fid->Vnode, Fid->Unique);

    int resrequired = 1;
    for (int i = 0; i < VSG_MEMBERS; i++) 
	if (!mgrp->rrcc.hosts[i])
	    VV[i] = NULL;
    
    UpdateRunts(mgrp, VV, Fid);

    // check if any object already inc 
    {
	if (AlreadyIncGroup(VV, VSG_MEMBERS)) {
	    LogMsg(0, SrvDebugLevel, stdout,  
		   "RegDirResRequired: Group already inconsistent");
	    *errorCode = EINCONS;
	    resrequired = 0;
	}
    }
    // checking if vv's already equal 
    if (resrequired) {
	LogMsg(9, SrvDebugLevel, stdout,
	       "RegDirResRequired: Checking if Objects equal \n");
	ViceVersionVector *vv[VSG_MEMBERS];
	for (int i = 0; i < VSG_MEMBERS; i++) 
	    vv[i] = VV[i];
	int HowMany = 0;
	if (VV_Check(&HowMany, vv, 1) == 1) {
	    LogMsg(0, SrvDebugLevel, stdout,  
		   "RegDirResRequired: VECTORS ARE ALREADY EQUAL\n");
	    for (int i = 0; i < VSG_MEMBERS; i++) 
		if (vv[i])
		    PrintVV(stdout, vv[i]);
	    resrequired = 0;
	    extern void ResCheckServerLWP();
	    LWP_NoYieldSignal((char *)ResCheckServerLWP);
	}
    }
    //PROBE(tpinfo, WEAKEQBEGIN);

    //check for weak equality
    {
	if (resrequired) {
	    LogMsg(9, SrvDebugLevel, stdout,  
		   "RegDirResRequired: Checking for weak Equality\n");
	    if (IsWeaklyEqual(VV, VSG_MEMBERS)) {
		unsigned long hosts[VSG_MEMBERS];
		LogMsg(39, SrvDebugLevel, stdout,  
		       "RegDirResRequired: WEAKLY EQUAL DIRECTORIES");
		*errorCode = WERes(Fid, VV, rstatusp, mgrp, hosts);
		if (*errorCode) 
		    LogMsg(0, SrvDebugLevel, stdout,  
			   "RegDirResRequired: error %d in WERes()",
			   errorCode);
		else 
		    resrequired = 0;
	    }
	}
    }
    LogMsg(1, SrvDebugLevel, stdout,
	   "RegDirRes %s Required: errorCode = %d\n", 
	   resrequired ? "" : "not",
	   *errorCode);
    return(resrequired);
}

// collect logs for a directory 
// return pointer to buffer.
PRIVATE char *CoordPhase2(res_mgrpent *mgrp, ViceFid *fid, 
			  int *totalentries, int *sizes, 
			  int *totalsize, unsigned long *successFlags, 
			  dirresstats *drstats) {
    
    char *bufs[VSG_MEMBERS];
    char *logbuffer = NULL;
    *totalsize = 0;
    *totalentries = 0;
    int errorCode = 0;
    
    LogMsg(9, SrvDebugLevel, stdout,  
	   "CoordPhase2: Fetching logs for %x.%x.%x",
	   fid->Volume, fid->Vnode, fid->Unique);
    
    AllocateBufs(mgrp, bufs, sizes);
    
    // set up the parameters 
    SE_Descriptor sid;
    bzero((void *)&sid, sizeof(SE_Descriptor));
    sid.Tag = SMARTFTP;
    sid.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
    sid.Value.SmartFTPD.Tag = FILEINVM;
    sid.Value.SmartFTPD.ByteQuota = -1;
    ARG_MARSHALL(IN_OUT_MODE, SE_Descriptor, sidvar, sid, VSG_MEMBERS);
    for (int i = 0; i < VSG_MEMBERS; i++) {
	sidvar_bufs[i].Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqLen = 
	    sizes[i];
	sidvar_bufs[i].Value.SmartFTPD.FileInfo.ByAddr.vmfile.MaxSeqLen = 
	    sizes[i];
	sidvar_bufs[i].Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqBody = 
	    (RPC2_ByteSeq)bufs[i];
    }
    int logsize = 0;
    ARG_MARSHALL(OUT_MODE, RPC2_Integer, logsizevar, logsize, VSG_MEMBERS);
    int nentries = 0;
    ARG_MARSHALL(OUT_MODE, RPC2_Integer, nentriesvar, nentries, VSG_MEMBERS);
    
    // fetch the logs 
    {
	LogMsg(9, SrvDebugLevel, stdout,  
	       "CoordPhase2: Going to do Multirpc fetch");
	MRPC_MakeMulti(ResPhase2_OP, ResPhase2_PTR, VSG_MEMBERS, 
		       mgrp->rrcc.handles, mgrp->rrcc.retcodes, 
		       mgrp->rrcc.MIp, 0, 0, fid, logsizevar_ptrs,
		       nentriesvar_ptrs, sidvar_bufs);
	LogMsg(39, SrvDebugLevel, stdout,  "CollectLogs: ret codes from FetchLog [%d %d %d %d %d %d %d %d]",
	       mgrp->rrcc.retcodes[0], mgrp->rrcc.retcodes[1], 
	       mgrp->rrcc.retcodes[2], mgrp->rrcc.retcodes[3], 
	       mgrp->rrcc.retcodes[4], mgrp->rrcc.retcodes[5], 
	       mgrp->rrcc.retcodes[6], mgrp->rrcc.retcodes[7]);
	mgrp->CheckResult();
	if (errorCode = CheckRetCodes((unsigned long *)mgrp->rrcc.retcodes, 
				      mgrp->rrcc.hosts, successFlags)) {
	    LogMsg(0, SrvDebugLevel, stdout,  
		   "Error Code from at least one server sending log");
	   goto Exit;
       }
       LogMsg(9, SrvDebugLevel, stdout,  
	      "CoordPhase2: Returned from Multirpc fetch \n");
   }
   
   
   // concat into a big buf 
   {
       logbuffer = ConcatLogs(mgrp, bufs, logsizevar_bufs, 
			      nentriesvar_bufs, totalsize, totalentries);
   }

 Exit:
   {
       DeAllocateBufs(bufs);
   }
   if (logbuffer) {
       drstats->logshipstats.add(*totalsize, (int *)nentriesvar_bufs, VSG_MEMBERS);
   }
   return(logbuffer);
       
}

PRIVATE void AllocateBufs(res_mgrpent *mgrp, char **bufs, int *sizes) {
    for (int i = 0; i < VSG_MEMBERS; i++) {
	if (sizes[i] > 0 && mgrp->rrcc.handles[i]) 
	    bufs[i] = (char *)malloc(sizes[i]);
	else {
	    sizes[i] = 0;
	    bufs[i] = 0;
	}
    }
    LogMsg(39, SrvDebugLevel, stdout,  
	   "AllocateBufs: Log sizes are [%d %d %d %d %d %d %d %d]",
	   sizes[0], sizes[1], sizes[2], sizes[3], sizes[4], sizes[5], 
	   sizes[6], sizes[7]);
}

PRIVATE void DeAllocateBufs(char **bufs) {
    for (int i = 0; i < VSG_MEMBERS; i++) 
	if (bufs[i]) {
	    free(bufs[i]);
	    bufs[i] = 0;
	}
}

PRIVATE char *ConcatLogs(res_mgrpent *mgrp, char **bufs, 
			 RPC2_Integer *sizes, RPC2_Integer *entries, 
			 int *totalsize, int *totalentries) {
    char *logbuffer = NULL;
    // calculate the size 
    for (int i = 0; i < VSG_MEMBERS; i++) {
	if (mgrp->rrcc.hosts[i] && mgrp->rrcc.retcodes[i] == 0) {
	    *totalsize += sizes[i];
	    *totalentries += entries[i];
	}
	else {
	    sizes[i] = 0;
	    entries[i] = 0;
	}
    }
    if ((*totalsize > 0) && (*totalentries > 0)) 
	logbuffer = (char *)malloc(*totalsize);

    /* copy into buf */
    char *tmp = logbuffer;
    if (logbuffer) {
	for (int i = 0; i < VSG_MEMBERS; i++) 
	    if (mgrp->rrcc.hosts[i] &&
		(mgrp->rrcc.retcodes[i] == 0) &&
		bufs[i]) {
		bcopy(bufs[i], tmp, sizes[i]);
		tmp += sizes[i];
	    }
    }
    return(logbuffer);
}


PRIVATE int CoordPhase3(res_mgrpent *mgrp, ViceFid *Fid, char *AllLogs, int logsize,
			int totalentries, ViceVersionVector **VV, 
			dlist *inclist, ResStatus **rstatusp, 
			unsigned long *successFlags, int *dirlengths) {

    RPC2_BoundedBS PBinc;
    char buf[RESCOMM_MAXBSLEN];
    SE_Descriptor	sid;
    ViceStatus status;
    // init parms PB, sid, status block
    {
	PBinc.SeqBody = (RPC2_ByteSeq)buf;
	PBinc.SeqLen = RESCOMM_MAXBSLEN;
	PBinc.MaxSeqLen = RESCOMM_MAXBSLEN;
	
	bzero((void *)&sid, sizeof(SE_Descriptor));
	sid.Tag = SMARTFTP;
	sid.Value.SmartFTPD.TransmissionDirection = CLIENTTOSERVER;
	sid.Value.SmartFTPD.ByteQuota = -1;
	sid.Value.SmartFTPD.Tag = FILEINVM;
	sid.Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqLen = logsize;
	sid.Value.SmartFTPD.FileInfo.ByAddr.vmfile.MaxSeqLen = logsize;
	sid.Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqBody = 
	    (RPC2_ByteSeq)AllLogs;

	// Get final version of Vicestatus from all the status blocks 
	{
	    GetResStatus(successFlags, rstatusp, &status);
	    GetMaxVV(&status.VV, VV, -1);
	    AllocStoreId(&status.VV.StoreId);
	}
    }
    ARG_MARSHALL(IN_OUT_MODE, SE_Descriptor, sidvar, sid, VSG_MEMBERS);
    ARG_MARSHALL_BS(IN_OUT_MODE, RPC2_BoundedBS, PBincvar, PBinc, VSG_MEMBERS, RESCOMM_MAXBSLEN);
    ARG_MARSHALL(IN_OUT_MODE, ViceStatus, statusvar, status, VSG_MEMBERS);

    // Ship log to Subordinates & Parse results
    {
	MRPC_MakeMulti(ResPhase3_OP, ResPhase3_PTR, VSG_MEMBERS,
		       mgrp->rrcc.handles, mgrp->rrcc.retcodes, 
		       mgrp->rrcc.MIp, 0, 0, Fid, logsize, 
		       totalentries, statusvar_ptrs, PBincvar_ptrs, 
		       sidvar_bufs);
	mgrp->CheckResult();
	int errorCode = 0;
	if (errorCode = CheckRetCodes((unsigned long *)mgrp->rrcc.retcodes, 
				      mgrp->rrcc.hosts, successFlags)) {
	    LogMsg(0, SrvDebugLevel, stdout,  
		   "CoordPhase3: Error %d in ResPhase3", errorCode);
	    return(errorCode);
	}

	if (ComparePhase3Status(mgrp, dirlengths, statusvar_bufs, &status)) {
	    LogMsg(0, SrvDebugLevel, stdout,
		   "CoordPhase3: Status blocks do not match\n");
	    return(EINCONS);
	}
    }

    /* Parse inconsistencies */
    {
	for (int i = 0; i < VSG_MEMBERS; i++) 
	    if (mgrp->rrcc.hosts[i] && !mgrp->rrcc.retcodes[i]) 
		BSToDlist(PBincvar_ptrs[i], inclist);
	LogMsg(9, SrvDebugLevel, stdout,  "Phase1 returns 0");
	return(0);
    }
}

PRIVATE int ComparePhase3Status(res_mgrpent *mgrp, int *dirlengths, 
				ViceStatus *status_bufs, ViceStatus *status) {
    int statusgotalready = 0;
    for (int i = 0; i < VSG_MEMBERS; i++) {
	dirlengths[i] = 0;
	if (mgrp->rrcc.hosts[i] && !mgrp->rrcc.retcodes[i]) {
	    dirlengths[i] = status_bufs[i].Length;
	    if (!statusgotalready) 
		*status = status_bufs[i];
	    else {
		ViceStatus *vs = &status_bufs[i];
		int unequal = ((vs->Author != status->Author) ||
			       (vs->Owner != status->Owner) ||
			       (vs->Mode != status->Mode) ||
			       (vs->vparent != status->vparent) ||
			       (vs->uparent != status->uparent));
		if (unequal) {
		    LogMsg(0, SrvDebugLevel, stdout,  
			   "Phase3: replica status not equal at end of phase 3");
		    return(EINCONS);
		}
	    }
	}
    }
    return(0);
}


PRIVATE int CoordPhase4(res_mgrpent *mgrp, ViceFid *Fid, 
			unsigned long *succflags, int *dirlengths) {
    ViceVersionVector UpdateSet;
    char *dirbufs[VSG_MEMBERS];
    int Phase4Err = 0;
    SE_Descriptor sid;

    // initialize parameters for call to subordinate VV, sid, 
    {
	
	for (int i = 0; i < VSG_MEMBERS; i++) 
		(&(UpdateSet.Versions.Site0))[i] = 0;
	    
    { /* drop scope for int i below; to avoid identifier clash */
	for (int i = 0; i < VSG_MEMBERS; i++) 
	    if (succflags[i]) {
		// find the index in the update set 
		vrent *vre = VRDB.find(Fid->Volume);
		assert(vre);
		(&(UpdateSet.Versions.Site0))[vre->index(succflags[i])] = 1;
	    }
    } /* drop scope for int i above; to avoid identifier clash */
	AllocStoreId(&UpdateSet.StoreId);
	
	bzero((void *)&sid, sizeof(SE_Descriptor));
	sid.Tag = SMARTFTP;
	sid.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
	sid.Value.SmartFTPD.Tag = FILEINVM;
	sid.Value.SmartFTPD.ByteQuota = -1;
    }
    ARG_MARSHALL(IN_OUT_MODE, SE_Descriptor, sidvar, sid, VSG_MEMBERS);
    for (int i = 0; i < VSG_MEMBERS; i++)  {
	if (dirlengths[i]) {
	    dirbufs[i] = (char *)malloc(dirlengths[i]);
	    assert(dirbufs[i]);
	    sidvar_bufs[i].Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqLen = 
		dirlengths[i];
	    sidvar_bufs[i].Value.SmartFTPD.FileInfo.ByAddr.vmfile.MaxSeqLen = 
		dirlengths[i];
	    sidvar_bufs[i].Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqBody = 
		(RPC2_ByteSeq)dirbufs[i];
	}
	else dirbufs[i] = NULL;
    }
    // call the subordinate
    {
	LogMsg(9, SrvDebugLevel, stdout,  
	       "CoordPhase4: Doing Phase 4");
	// for now use old res interface 
	MRPC_MakeMulti(ResPhase4_OP, ResPhase4_PTR, VSG_MEMBERS,
		       mgrp->rrcc.handles, mgrp->rrcc.retcodes,
		       mgrp->rrcc.MIp, 0, 0, Fid, &UpdateSet, sidvar_bufs);
	mgrp->CheckResult();
	
	if (Phase4Err = CheckRetCodes((unsigned long *)mgrp->rrcc.retcodes, 
				      mgrp->rrcc.hosts, succflags)) 
	    LogMsg(0, SrvDebugLevel, stdout,  
		   "CoordPhase4: Phase4 Error %d", Phase4Err);
	PollAndYield();

    }
    // compare contents of directory replicas 
    {
	if (!Phase4Err  && ((Phase4Err = CompareDirContents(sidvar_bufs, Fid)) == 0))
	    LogMsg(9, SrvDebugLevel, stdout,  
		   "CoordPhase4: Dir Contents equal after phase4");
    }

    // clean up 
    {
	for (int i = 0; i < VSG_MEMBERS; i++) 
	    if (dirbufs[i]) free(dirbufs[i]);
    }
    LogMsg(1, SrvDebugLevel, stdout,  
	   "CoordPhase4: returns %d\n", Phase4Err);
    return(Phase4Err);
}

PRIVATE int CoordPhase34(res_mgrpent *mgrp, ViceFid *Fid, 
			 dlist *inclist, int *dirlengths, int *noinc) {
    RPC2_BoundedBS PB;
    char buf[RESCOMM_MAXBSLEN];
    int errorCode = 0;
    ViceStatus status;

    if (inclist->count() ==  0) {
	*noinc = 1;
	return(0);
    }
    
    *noinc = 0;
    /* pack list of inconsistencies into a BoundedBS */
    {
	PB.MaxSeqLen = RESCOMM_MAXBSLEN;
	PB.SeqBody = (RPC2_ByteSeq)buf;
	PB.SeqLen = 0;
	DlistToBS(inclist, &PB);
    }

    ViceStoreId logid;
    AllocStoreId(&logid);
    ARG_MARSHALL(OUT_MODE, ViceStatus, statusvar, status, VSG_MEMBERS);
    MRPC_MakeMulti(ResPhase34_OP, ResPhase34_PTR, VSG_MEMBERS,
		   mgrp->rrcc.handles, mgrp->rrcc.retcodes,
		   mgrp->rrcc.MIp, 0, 0, Fid, &logid, statusvar_ptrs, &PB);
    for (int i = 0; i < VSG_MEMBERS; i++) {
	if (mgrp->rrcc.hosts[i] && !mgrp->rrcc.retcodes[i])
	    dirlengths[i] = statusvar_bufs[i].Length;
	else 
	    dirlengths[i] = 0;
    }
    mgrp->CheckResult();
    unsigned long hosts[VSG_MEMBERS];
    if (errorCode = CheckRetCodes((unsigned long *)mgrp->rrcc.retcodes, mgrp->rrcc.hosts, hosts)){
	LogMsg(0, SrvDebugLevel, stdout,  
	       "CoordPhase34: Error %d in DirResPhase2", 
	       errorCode);
	return(errorCode);
    }
    return(0);
}

// XXXXX adapted from dir.private.h
#define MAXPAGES 128
#define PAGESIZE 2048

PRIVATE int ResolveInc(res_mgrpent *mgrp, ViceFid *Fid, ViceVersionVector **VVGroup) {
    SE_Descriptor sid;
    char *dirbufs[VSG_MEMBERS];
    int *dirlengths[VSG_MEMBERS];
    int dirlength = MAXPAGES * PAGESIZE + VAclSize(foo);
    ViceStatus status;
    int DirsEqual = 0;
    ViceVersionVector *VV;
    int size;
    unsigned long succflags[VSG_MEMBERS];
    int errorcode = EINCONS;

    // make all replicas inconsistent
    if (!AllIncGroup(VVGroup, VSG_MEMBERS)) {
	LogMsg(0, SrvDebugLevel, stdout,
	       "ResolveInc: Not all replicas of (0x%x.%x.%x) are inconsistent yet\n",
	       Fid->Volume, Fid->Vnode, Fid->Unique);
	MRPC_MakeMulti(MarkInc_OP, MarkInc_PTR, VSG_MEMBERS,
		       mgrp->rrcc.handles, mgrp->rrcc.retcodes,
		       mgrp->rrcc.MIp, 0, 0, Fid);
	return(EINCONS);
    }
    
    // set up buffers to get dir contents & status blocks
    {
	bzero((void *)&sid, sizeof(SE_Descriptor));
	sid.Tag = SMARTFTP;
	sid.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
	sid.Value.SmartFTPD.Tag = FILEINVM;
	sid.Value.SmartFTPD.ByteQuota = -1;
    }
    ARG_MARSHALL(IN_OUT_MODE, SE_Descriptor, sidvar, sid, VSG_MEMBERS);
    
    for (int i = 0; i < VSG_MEMBERS; i++)  {
	if (mgrp->rrcc.handles[i]) {
	    dirbufs[i] = (char *)malloc(dirlength);
	    assert(dirbufs[i]);
	    sidvar_bufs[i].Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqLen = 
		dirlength;
	    sidvar_bufs[i].Value.SmartFTPD.FileInfo.ByAddr.vmfile.MaxSeqLen = 
		dirlength;
	    sidvar_bufs[i].Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqBody = 
		(RPC2_ByteSeq)dirbufs[i];
	}
	else dirbufs[i] = NULL;
    }
    ARG_MARSHALL(OUT_MODE, ViceStatus, statusvar, status, VSG_MEMBERS);
    ARG_MARSHALL(OUT_MODE, RPC2_Integer, sizevar, size, VSG_MEMBERS);
    // get the dir replica's contents
    {
	MRPC_MakeMulti(FetchDirContents_OP, FetchDirContents_PTR, VSG_MEMBERS,
		       mgrp->rrcc.handles, mgrp->rrcc.retcodes,
		       mgrp->rrcc.MIp, 0, 0, Fid, sizevar_ptrs, statusvar_ptrs, sidvar_bufs);
	mgrp->CheckResult();
	if (CheckRetCodes((unsigned long *)mgrp->rrcc.retcodes, mgrp->rrcc.hosts, 
			  succflags)) {
	    LogMsg(0, SrvDebugLevel, stdout,
		   "ResolveInc: Error during FetchDirContents\n");
	    goto Exit;
	}
    }
    
    // compare the contents 
    {
	if (CompareDirStatus(statusvar_bufs, mgrp, &VV) == 0) {
	    if (CompareDirContents(sidvar_bufs, Fid) == 0) {
		LogMsg(0, SrvDebugLevel, stdout, 
		       "ResolveInc: Dir contents are equal\n");
		DirsEqual = 1;
	    }
	    else 
		LogMsg(0, SrvDebugLevel, stdout,
		       "ResolveInc: Dir contents are unequal\n");
	}
	else 
	    LogMsg(0, SrvDebugLevel, stdout,
		   "ResolveInc: Dir Status blocks are different\n");
    }
    // clear inconsistency if equal
    {
	if (DirsEqual)
	    MRPC_MakeMulti(ClearIncon_OP, ClearIncon_PTR, VSG_MEMBERS,
			   mgrp->rrcc.handles, mgrp->rrcc.retcodes,
			   mgrp->rrcc.MIp, 0, 0, Fid, VV);
	mgrp->CheckResult();
	errorcode = CheckRetCodes((unsigned long *)mgrp->rrcc.retcodes, mgrp->rrcc.hosts, succflags);
    }
  Exit:
    // free all the allocate dir bufs
  { /* drop scope for int i below; to avoid identifier clash */
    for (int i = 0 ; i < VSG_MEMBERS; i++) 
	if (dirbufs[i]) 
	    free(dirbufs[i]);
  } /* drop scope for int i above; to avoid identifier clash */

    LogMsg(0, SrvDebugLevel, stdout,
	   "ResolveInc: returns(%d)\n",
	   errorcode);
    return(errorcode);
}

//taken from rescoord.c
PRIVATE int AlreadyIncGroup(ViceVersionVector **VV, int nvvs) {
    for (int i = 0; i < nvvs; i++) {
	if (VV[i] == NULL) continue;
	if (IsIncon((*(VV[i])))) return(1);
    }
    return(0);
}

// return 1 if all the vectors in a group show inconsistency
PRIVATE int AllIncGroup(ViceVersionVector **VV, int nvvs) {
    for (int i = 0; i < nvvs; i++) {
	if (VV[i] == NULL) continue;
	if (!IsIncon((*(VV[i])))) return(0);
    }
    return(1);

}
extern int comparedirreps;
PRIVATE int CompareDirContents(SE_Descriptor *sid_bufs, ViceFid *fid) {
    LogMsg(9, SrvDebugLevel, stdout,  "Entering CompareDirContents()");

    if (!comparedirreps) return(0);

    if (SrvDebugLevel > 9) 
	// dump contents to files 
	DumpDirContents(sid_bufs, fid);
    int replicafound = 0;
    char *firstreplica = 0;
    int firstreplicasize = 0;
    for (int i = 0; i < VSG_MEMBERS; i++) {
	int len = sid_bufs[i].Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqLen;
	char *buf = (char *)sid_bufs[i].Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqBody;
	
	if (len) {
	    if (!replicafound) {
		replicafound = 1;
		firstreplica  = buf;
		firstreplicasize = len;
	    }
	    else {
		if (bcmp(firstreplica, buf, len)) {
		    LogMsg(0, SrvDebugLevel, stdout,  
			   "DirContents/Vol Quotas ARE DIFFERENT");
		    DumpDirContents(sid_bufs, fid);
		    return(-1);
		}
	    }
	}
    }
    return(0);
}

PRIVATE DumpDirContents(SE_Descriptor *sid_bufs, ViceFid *fid) {
    for (int j = 0; j < VSG_MEMBERS; j++) {
	int length = sid_bufs[j].Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqLen;
	if (length) {
	    char fname[256];
	    sprintf(fname, "/tmp/dir.0x%x.0x%x.%d", fid->Vnode, fid->Unique, j);
	    int fd = open(fname, O_CREAT | O_TRUNC | O_RDWR, 0777);
	    assert(fd > 0);
	    write(fd, sid_bufs[j].Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqBody, 
		  length);
	    close(fd);
	}
    }
}

PRIVATE void PrintStatus(ViceStatus *status) {
    LogMsg(0, SrvDebugLevel, stdout,
	   "LinkCount(%d), Length(%d), Author(%u), Owner(%u), Mode(%u), Parent(0x%x.%x)\n",
	   status->LinkCount, status->Length, status->Author, status->Owner, status->Mode,
	   status->vparent, status->uparent);
    PrintVV(stdout, &(status->VV));
}

PRIVATE int CompareDirStatus(ViceStatus *status, res_mgrpent *mgrp, ViceVersionVector **VV) {
    int dirfound = -1;
    for (int i = 0; i < VSG_MEMBERS; i++) {
	if (mgrp->rrcc.hosts[i] && (mgrp->rrcc.retcodes[i] == 0)) {
	    if (dirfound == -1) {
		dirfound = i;
		*VV = &status[i].VV;
	    }
	    else {
		// compare the status blocks
		if ((status[i].LinkCount != status[dirfound].LinkCount) ||
		    (status[i].Length != status[dirfound].Length) ||
		    (status[i].Author != status[dirfound].Author) ||
		    (status[i].Owner != status[dirfound].Owner) ||
		    (status[i].Mode != status[dirfound].Mode) ||
		    (status[i].vparent != status[dirfound].vparent) ||
		    (status[i].uparent != status[dirfound].uparent) ||
		    (VV_Cmp_IgnoreInc(&status[i].VV, &status[dirfound].VV) != VV_EQ)) {
		    LogMsg(0, SrvDebugLevel, stdout,
			   "CompareDirStatus: Status blocks are different\n");
		    PrintStatus(&status[i]);
		    PrintStatus(&status[dirfound]);
		    return(-1);
		}
	    }
	}
    }
    return(0);
}

PRIVATE PrintPaths(int *sizes, ResPathElem **paths, res_mgrpent *mgrp) {
    for (int i = 0; i < VSG_MEMBERS; i++) 
	if (sizes[i] && !mgrp->rrcc.retcodes[i] && mgrp->rrcc.handles) {
	    LogMsg(0, SrvDebugLevel, stdout,
		   "There are %d Components for path %d\n",
		   sizes[i], i);
	    ResPathElem *components = paths[i];
	    for (int j = 0; j < sizes[i]; j++) 
		LogMsg(0, SrvDebugLevel, stdout, 
		       "Fid: 0x%x.%x StoreId 0x%x.%x\n",
		       components[j].vn, components[j].un,
		       components[j].vv.StoreId.Host, 
		       components[j].vv.StoreId.Uniquifier);
	}
}

PRIVATE void UpdateStats(ViceFid *Fid, dirresstats *drstats) {
    VolumeId vid = Fid->Volume;
    Volume *volptr = 0;
    if (XlateVid(&vid)) {
	if (!GetVolObj(vid, &volptr, VOL_NO_LOCK, 0, 0)) {
	    if (AllowResolution && V_RVMResOn(volptr)) 
		V_VolLog(volptr)->vmrstats->update(drstats);
	}
	else { 
	    LogMsg(0, SrvDebugLevel, stdout,
	       "UpdateStats: couldn't get vol obj 0x%x\n", vid);
	    volptr = 0;
	}
    }
    else 
	LogMsg(0, SrvDebugLevel, stdout,
	       "UpdateStats: couldn't Xlate Fid 0x%x\n", vid);
    if (volptr) 
	PutVolObj(&volptr, VOL_NO_LOCK, 0);

}

PRIVATE void UpdateStats(ViceFid *Fid, int success, int dirdepth) {
    VolumeId vid = Fid->Volume;
    Volume *volptr = 0;
    if (XlateVid(&vid)) {
	if (!GetVolObj(vid, &volptr, VOL_NO_LOCK, 0, 0)) {
	    if (AllowResolution && V_RVMResOn(volptr)) {
		int bucketnumber = (dirdepth >= DEPTHSIZE) ? (DEPTHSIZE - 1): dirdepth;
		if (success) 
		    V_VolLog(volptr)->vmrstats->hstats.succres[bucketnumber]++;
		else
		    V_VolLog(volptr)->vmrstats->hstats.unsuccres[bucketnumber]++;
	    }
	}
	else { 
	    LogMsg(0, SrvDebugLevel, stdout,
		   "UpdateStats: couldn't get vol obj 0x%x\n", vid);
	    volptr = 0;
	}
    }
    else 
	LogMsg(0, SrvDebugLevel, stdout,
	       "UpdateStats: couldn't Xlate Fid 0x%x\n", vid);
    if (volptr) 
	PutVolObj(&volptr, VOL_NO_LOCK, 0);

}

