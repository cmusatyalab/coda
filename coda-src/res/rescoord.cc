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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/res/Attic/rescoord.cc,v 4.4 1998/01/14 15:16:43 braam Exp $";
#endif /*_BLURB_*/






/* 
 * rescoord.c
 *	Implements the coordinator side for 
 *	directory resolution
 */

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
#include <srv.h>
#include <inconsist.h>
#include <rvmdir.h>
#include <vlist.h>
#include <operations.h>
#include <res.h>
#include <treeremove.h>

#include "rescomm.h"
#include "resutil.h"
#include "pdlist.h"
#include "reslog.h" 
#include "remotelog.h"
#include "resforce.h"
#include "timing.h"

timing_path *tpinfo = 0;
timing_path *FileresTPinfo = 0; 
extern void PollAndYield();
extern void ResCheckServerLWP();

/* private routines */
PRIVATE int AlreadyIncGroup(ViceVersionVector **VV, int nvvs);
PRIVATE char *CollectLogs(res_mgrpent *, ViceFid *, int *, int *);
PRIVATE int Phase1(res_mgrpent *, ViceFid *, rlent *, int, ViceVersionVector **,
		   dlist *, ViceStatus *, unsigned long *, int *);
PRIVATE int Phase2(res_mgrpent *, ViceFid *, dlist *, int *);
PRIVATE int WEResPhase1(ViceVersionVector **, res_mgrpent *, ViceFid *, 
			unsigned long *, ViceStoreId *);
PRIVATE int WEResPhase2(res_mgrpent *, ViceFid *, unsigned long *, ViceStoreId *);
PRIVATE int CompareDirContents(SE_Descriptor *);

/* Directory Resolution
 * Coordinator side of resolution algorithm
 * 	If any replica is already inconsistent then mark all replicas so;
 *	If directories are weakly equal, 
 *		compute new vv and send it to all sites
 *	Otherwise Do actual resolution:
 *	  PHASE 1
 *		Collect logs from all sites.
 *		Compute New VV.
 *		Concat logs and ship them with VV to all sites;
 *			< EACH SITE DOES RESOLUION - Phase 1>
 *		Collect replies.
 *	  PHASE 2
 *		If no inconsistencies found in Phase 1 go to Phase 3
 *		Collect list of inconsistencies
 *		Ship list of inconsistencies to each site.
 *	  PHASE 3
 *		If no error in Phase1 or Phase2 do equivalent of venus COP2.
 *		  i.e. send list of successful sites to every successful site
 *		Side effect of Phase 3 is the comparing of directory contents.
 *		In case of error, mark all replicas inconsistent
 */
long DirResolve(res_mgrpent *mgrp, ViceFid *Fid, ViceVersionVector **VV, 
		int *sizes) {

    dlist *inclist = 0;
    int reserror = 1;
    char *AllLogs = 0;
    char *dirbufs[VSG_MEMBERS];

    LogMsg(9, SrvDebugLevel, stdout,  "Entering DirResolve(%x.%x)", Fid->Vnode, Fid->Unique);
    
    PROBE(tpinfo, RUNTUPDATEBEGIN);

    /* regenerate VVs for host set */
    for (int i = 0; i < VSG_MEMBERS; i++) {
	if (!mgrp->rrcc.hosts[i])  VV[i] = NULL;
	dirbufs[i] = 0;
    }

    UpdateRunts(mgrp, VV, Fid); 
    PROBE(tpinfo, RUNTUPDATEEND);

    /* check if any object already inc */
    {
	if (AlreadyIncGroup(VV, VSG_MEMBERS)) {
	    LogMsg(0, SrvDebugLevel, stdout,  "DirResolve: Group already inconsistent");
	    goto Exit;
	}
    }

    /* checking if vv's already equal */
    {
	LogMsg(9, SrvDebugLevel, stdout,  "DirResolve: Checking if Objects equal ");
	ViceVersionVector *vv[VSG_MEMBERS];
	for (int i = 0; i < VSG_MEMBERS; i++) 
	    vv[i] = VV[i];
	int HowMany = 0;
	if (VV_Check(&HowMany, vv, 1) == 1) {
	    LogMsg(0, SrvDebugLevel, stdout,  "DirResolve: VECTORS ARE ALREADY EQUAL");
	    LWP_NoYieldSignal((char *)ResCheckServerLWP);
	    for (int i = 0; i < VSG_MEMBERS; i++) 
		if (vv[i])
		    PrintVV(stdout, vv[i]);
	    return(0);
	}
    }
    PROBE(tpinfo, WEAKEQBEGIN);
    {
	LogMsg(9, SrvDebugLevel, stdout,  "DirResolve: Checking for weak Equality");
	if (IsWeaklyEqual(VV, VSG_MEMBERS)) {
	    unsigned long hosts[VSG_MEMBERS];
	    LogMsg(39, SrvDebugLevel, stdout,  "DirResolve: WEAKLY EQUAL DIRECTORIES");

	    reserror = WERes(Fid, VV, NULL, mgrp, hosts);
	    if (reserror) {
		LogMsg(0, SrvDebugLevel, stdout,  "DirResolve: error %d in WERes()",
			reserror);
		goto Exit;
	    }
	    else 
		return(0);
	}
    }

    /* Regular directory resolution */
    LogMsg(9, SrvDebugLevel, stdout,  "DirResolve: Regular Directory Resolution");
    PROBE(tpinfo, COLLECTLOGBEGIN);
    {
	unsigned long succFlags[VSG_MEMBERS];	
	ViceStatus status;
	int dirlengths[VSG_MEMBERS];
	inclist = new dlist((CFN)CompareIlinkEntry);

	/* Collect Logs and do Phase 1 */
	{
	    int AllLogSize = 0;
	    AllLogs = CollectLogs(mgrp, Fid, sizes, &AllLogSize);
	    if (!AllLogs) {
		LogMsg(0, SrvDebugLevel, stdout,  "DirResolve: CollectLogs returned empty logs");
		LogMsg(0, SrvDebugLevel, stdout,  "Marking Object inconsistent");
		goto Exit;
	    }
	    PROBE(tpinfo, COLLECTLOGEND);
	    PollAndYield();
	    PROBE(tpinfo, COORP1BEGIN);
	    int Phase1Err = Phase1(mgrp, Fid, (rlent *)AllLogs, AllLogSize, 
				   VV, inclist, &status, succFlags, dirlengths);
	    PROBE(tpinfo, COORP1END);
	    free(AllLogs);
	    AllLogs = 0;
	    
	    if (Phase1Err) {
		LogMsg(0, SrvDebugLevel, stdout,  "Error during Phase1 for fid %x.%x.%x",
			Fid->Volume, Fid->Vnode, Fid->Unique);
		goto Exit;
	    }
	}
	PROBE(tpinfo, P1PANDYBEGIN);
	PollAndYield();
	PROBE(tpinfo, COORP2BEGIN);
	/* Phase2 if necessary */
	LogMsg(9, SrvDebugLevel, stdout,  "DirResolve: Doing Phase 2");
	if (Phase2(mgrp, Fid, inclist, dirlengths)) {
	    LogMsg(0, SrvDebugLevel, stdout,  "Error during Phase2 for fid %x.%x.%x",
		    Fid->Volume, Fid->Vnode, Fid->Unique);
	    goto Exit;
	}
	PROBE(tpinfo, COORP3BEGIN);
	
	/* Phase 3 */
	{
	    ViceVersionVector UpdateSet;
	    int Phase3Err = 0;
	    for (int i = 0; i < VSG_MEMBERS; i++) {
		if (succFlags[i]) 
		    (&(UpdateSet.Versions.Site0))[i] = 1;
		else 
		    (&(UpdateSet.Versions.Site0))[i] = 0;
	    }
	    AllocStoreId(&UpdateSet.StoreId);
	    
	    /* initialize sed for transferring dir contents */
	    SE_Descriptor sid;
	    bzero((void *)&sid, sizeof(SE_Descriptor));
	    sid.Tag = SMARTFTP;
	    sid.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
	    sid.Value.SmartFTPD.Tag = FILEINVM;
	    sid.Value.SmartFTPD.ByteQuota = -1;
	    ARG_MARSHALL(IN_OUT_MODE, SE_Descriptor, sidvar, sid, VSG_MEMBERS);
    { /* drop scope for int i below; to avoid identifier clash */
	    for (int i = 0; i < VSG_MEMBERS; i++) {
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
		else {
		    sidvar_bufs[i].Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqLen = 0;
		    sidvar_bufs[i].Value.SmartFTPD.FileInfo.ByAddr.vmfile.MaxSeqLen=0;
		    sidvar_bufs[i].Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqBody = 0;
		}
	    }
    } /* drop scope for int i above; to avoid identifier clash */
	    LogMsg(9, SrvDebugLevel, stdout,  "DirResolve: Doing Phase 3");
	    MRPC_MakeMulti(DirResPhase3_OP, DirResPhase3_PTR, VSG_MEMBERS,
			   mgrp->rrcc.handles, mgrp->rrcc.retcodes,
			   mgrp->rrcc.MIp, 0, 0, Fid, &UpdateSet, sidvar_bufs);
	    mgrp->CheckResult();
	    PROBE(tpinfo, COORP3END);

	    if (Phase3Err = CheckRetCodes((unsigned long *)mgrp->rrcc.retcodes, mgrp->rrcc.hosts, 
					  succFlags)) 
		LogMsg(0, SrvDebugLevel, stdout,  "DirResolve: Phase3 Error %d", Phase3Err);
	    else {
		PollAndYield();
		if (CompareDirContents(sidvar_bufs) == 0){
		    LogMsg(9, SrvDebugLevel, stdout,  "DirResolve: Dir Contents equal after phase3");
		    reserror = 0;
		}
		else {
		    LogMsg(0, SrvDebugLevel, stdout,  "DirResolve: Dir Contents inequal after phase3");
		    reserror = 1;
		}
	    }
	}
    }
    
  Exit:
    if (AllLogs) free(AllLogs);
    if (inclist) {
	ilink *il;
	while (il = (ilink *)inclist->get()) 
	    delete il;
	delete inclist;
    }
  { /* drop scope for int i below; to avoid identifier clash */
    for (int i = 0; i < VSG_MEMBERS; i++) 
	if (dirbufs[i]) 
	    free(dirbufs[i]);
  } /* drop scope for int i above; to avoid identifier clash */
    PROBE(tpinfo, COORMARKINCBEGIN);
    if (reserror) {
	MRPC_MakeMulti(MarkInc_OP, MarkInc_PTR, VSG_MEMBERS,
		       mgrp->rrcc.handles, mgrp->rrcc.retcodes, 
		       mgrp->rrcc.MIp, 0, 0, Fid);
	LogMsg(9, SrvDebugLevel, stdout,  "DirResolve returns EINCONS");
	return(EINCONS);
    }
    PROBE(tpinfo, COORMARKINCEND);
    LogMsg(9, SrvDebugLevel, stdout,  "DirResolve returns 0");
    return(0);
}

/* collect logs for a directory 
 * return pointer to buffer.
 */
PRIVATE char *CollectLogs(res_mgrpent *mgrp, ViceFid *fid, int *sizes, 
			  int *totalsize) {
   char *bufs[VSG_MEMBERS];
   char *logbuffer = 0;
   *totalsize = 0;
   int errorCode = 0;

   LogMsg(9, SrvDebugLevel, stdout,  "CollectLogs: Fetching logs for %x.%x.%x",
	   fid->Volume, fid->Vnode, fid->Unique);

   /* set up the buffers to receive the logs */
   {
       for (int i = 0; i < VSG_MEMBERS; i++) {
	   if (sizes[i] > 0 && mgrp->rrcc.handles[i]) 
	       bufs[i] = (char *)malloc(sizes[i]);
	   else {
	       sizes[i] = 0;
	       bufs[i] = 0;
	   }
       }
       LogMsg(39, SrvDebugLevel, stdout,  "CollectLogs: Log sizes are [%d %d %d %d %d %d %d %d]",
	       sizes[0], sizes[1], sizes[2], sizes[3], sizes[4], sizes[5], 
	       sizes[6], sizes[7]);
   }

   /* set up the parameters */
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

   /* fetch the logs */
   {
       LogMsg(9, SrvDebugLevel, stdout,  "CollectLogs: Going to do Multirpc fetch");
       MRPC_MakeMulti(FetchLog_OP, FetchLog_PTR, VSG_MEMBERS, 
		      mgrp->rrcc.handles, mgrp->rrcc.retcodes, 
		      mgrp->rrcc.MIp, 0, 0, fid, logsizevar_ptrs,
		      sidvar_bufs);
       LogMsg(39, SrvDebugLevel, stdout,  "CollectLogs: ret codes from FetchLog [%d %d %d %d %d %d %d %d]",
	       mgrp->rrcc.retcodes[0], mgrp->rrcc.retcodes[1], 
	       mgrp->rrcc.retcodes[2], mgrp->rrcc.retcodes[3], 
	       mgrp->rrcc.retcodes[4], mgrp->rrcc.retcodes[5], 
	       mgrp->rrcc.retcodes[6], mgrp->rrcc.retcodes[7]);
       mgrp->CheckResult();
       unsigned long successFlags[VSG_MEMBERS];
       if (errorCode = CheckRetCodes((unsigned long *)mgrp->rrcc.retcodes, mgrp->rrcc.hosts, successFlags)) {
	   LogMsg(0, SrvDebugLevel, stdout,  "Error Code from at least one server sending log");
	   goto Exit;
       }
       LogMsg(9, SrvDebugLevel, stdout,  "CollectLogs: Returned from Multirpc fetch ");
   }


   /* concat into a big buf */
   {
       /* calculate the size */
       for (int i = 0; i < VSG_MEMBERS; i++) 
	   if (mgrp->rrcc.hosts[i] && mgrp->rrcc.retcodes[i] == 0) 
	       *totalsize += logsizevar_bufs[i];
       if (*totalsize > 0) {
	   logbuffer = (char *)malloc(*totalsize);
	   assert(logbuffer != 0);
       }
       else 
	   goto Exit;
       
       /* copy into buf */
       char *tmp = logbuffer;
    { /* drop scope for int i below; to avoid identifier clash */
       for (int i = 0; i < VSG_MEMBERS; i++) 
	   if (mgrp->rrcc.hosts[i] &&
	       (mgrp->rrcc.retcodes[i] == 0) &&
	       bufs[i]) {
	       bcopy(bufs[i], tmp, logsizevar_bufs[i]);
	       tmp += logsizevar_bufs[i];
	   }
    } /* drop scope for int i above; to avoid identifier clash */

   }
 Exit:
   {
       for (int i = 0; i < VSG_MEMBERS; i++) 
	   if (bufs[i]) {
	       free(bufs[i]);
	       bufs[i] = 0;
	   }
       
   }
   return(logbuffer);
       
}

PRIVATE int Phase1(res_mgrpent *mgrp, ViceFid *Fid, rlent *log, int logsize,
		   ViceVersionVector **VV, dlist *inclist, ViceStatus *status,
		   unsigned long *successFlags, int *dirlengths) {

    /* form the bounded bs for inconsistencies list */
    RPC2_BoundedBS PBinc;
    char buf[RESCOMM_MAXBSLEN];
    PBinc.SeqBody = (RPC2_ByteSeq)buf;
    PBinc.SeqLen = RESCOMM_MAXBSLEN;
    PBinc.MaxSeqLen = RESCOMM_MAXBSLEN;
    ARG_MARSHALL_BS(IN_OUT_MODE, RPC2_BoundedBS, PBincvar, PBinc, VSG_MEMBERS, RESCOMM_MAXBSLEN);

    SE_Descriptor	sid;
    /* form the descriptor */
    bzero((void *)(void *)&sid, sizeof(SE_Descriptor));
    sid.Tag = SMARTFTP;
    sid.Value.SmartFTPD.TransmissionDirection = CLIENTTOSERVER;
    sid.Value.SmartFTPD.ByteQuota = -1;
    sid.Value.SmartFTPD.Tag = FILEINVM;
    sid.Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqLen = logsize;
    sid.Value.SmartFTPD.FileInfo.ByAddr.vmfile.MaxSeqLen = logsize;
    sid.Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqBody = 
	(RPC2_ByteSeq)log;
    ARG_MARSHALL(IN_OUT_MODE, SE_Descriptor, sidvar, sid, VSG_MEMBERS);

    /* Ship log - PHASE 1 of Dir Resolution */
    {
	GetMaxVV(&status->VV, VV, -1);
	AllocStoreId(&status->VV.StoreId);

	ARG_MARSHALL(IN_OUT_MODE, ViceStatus, statusvar, *status, VSG_MEMBERS);

	MRPC_MakeMulti(DirResPhase1_OP, DirResPhase1_PTR, VSG_MEMBERS,
		       mgrp->rrcc.handles, mgrp->rrcc.retcodes, 
		       mgrp->rrcc.MIp, 0, 0, Fid, logsize, 
		       statusvar_ptrs, PBincvar_ptrs, sidvar_bufs);
	mgrp->CheckResult();
	int errorCode = CheckRetCodes((unsigned long *)mgrp->rrcc.retcodes, mgrp->rrcc.hosts, 
				      successFlags);
	if (errorCode) {
	    /* non - rpc error code */
	    LogMsg(0, SrvDebugLevel, stdout,  "Phase1: Error %d in DirResPhase1_OP", errorCode);
	    return(errorCode);
	}

	/* match some fields of status blocks from different sites */
	int statusgotalready = 0;
	for (int i = 0; i < VSG_MEMBERS; i++) {
	    dirlengths[i] = 0;
	    if (mgrp->rrcc.hosts[i] && !mgrp->rrcc.retcodes[i]) {
		dirlengths[i] = statusvar_bufs[i].Length;
		if (!statusgotalready) 
		    *status = statusvar_bufs[i];
		else {
		    ViceStatus *vs = statusvar_ptrs[i];
		    int unequal = ((vs->Author != status->Author) ||
				   (vs->Owner != status->Owner) ||
				   (vs->Mode != status->Mode) ||
				   (vs->vparent != status->vparent) ||
				   (vs->uparent != status->uparent));
		    if (unequal) {
			LogMsg(0, SrvDebugLevel, stdout,  "Phase1 : replica status not equal at end of phase 1");
			return(EINCONS);
		    }
		}
	    }
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

PRIVATE int Phase2(res_mgrpent *mgrp, ViceFid *Fid, dlist *inclist, int *dirlengths) {
    RPC2_BoundedBS PB;
    char buf[RESCOMM_MAXBSLEN];
    int errorCode = 0;
    ViceStatus status;

    if (inclist->count() ==  0) 
	return(0);
    
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
    MRPC_MakeMulti(DirResPhase2_OP, DirResPhase2_PTR, VSG_MEMBERS,
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
	LogMsg(0, SrvDebugLevel, stdout,  "Phase2: Error %d in DirResPhase2", errorCode);
	return(errorCode);
    }
    return(0);
}
/* non-NULL VV pointers correspond to real VVs */
int IsWeaklyEqual(ViceVersionVector **VV, int nvvs) {
    int i, j;

    LogMsg(69, SrvDebugLevel, stdout,  "Entering IsWeaklyEqual()");
    for (i = 0; i < nvvs - 1 ; ){
	if (VV[i] == NULL) {
	    i++;
	    continue;
	}
	LogMsg(49, SrvDebugLevel, stdout,  "IsWeaklyEqual: Doing for i = %d", i);
	for (j = i + 1; j < nvvs; j++) {
	    LogMsg(49, SrvDebugLevel, stdout,  "IsWeaklyEqual: Doing for j = %d", 
		    j);
	    if (VV[j] == NULL) continue;
	    if (bcmp((const void *)&(VV[i]->StoreId), (const void *) &(VV[j]->StoreId), 
		     sizeof(ViceStoreId))) {
		LogMsg(49, SrvDebugLevel, stdout,  "IsWeaklyEqual - NO - returning");
		return(0);
	    }

	    LogMsg(49, SrvDebugLevel, stdout,  "IsWeaklyEqual: VV[%d] and VV[%d] have same storeid",
		    i, j);
	    i = j;
	    break;
	}
	i = j;	/* when j = nvvs */
    }
    /* weakly equal vvs */
    LogMsg(69, SrvDebugLevel, stdout,  "IsWeaklyEqual - YES - returning");
    return(1);
}

PRIVATE int AlreadyIncGroup(ViceVersionVector **VV, int nvvs) {
    for (int i = 0; i < nvvs; i++) {
	if (VV[i] == NULL) continue;
	if (IsIncon((*(VV[i])))) return(1);
    }
    return(0);
}

PRIVATE int WEResPhase1(ViceVersionVector **VV, 
			res_mgrpent *mgrp, ViceFid *Fid, 
			unsigned long *hosts, ViceStoreId *stid) {

    int errorCode = 0;

    LogMsg(9, SrvDebugLevel, stdout,  "Entering WEResPhase1(%x.%x)",
	    Fid->Vnode, Fid->Unique);

    /* force a new vv */
    {
	ViceVersionVector newvv;
	GetMaxVV(&newvv, VV, -1);
	*stid = newvv.StoreId;
	MRPC_MakeMulti(ForceDirVV_OP, ForceDirVV_PTR, VSG_MEMBERS, 
		       mgrp->rrcc.handles, mgrp->rrcc.retcodes,
		       mgrp->rrcc.MIp, 0, 0, Fid, &newvv);
	LogMsg(9, SrvDebugLevel, stdout,  "WEResPhase1 returned from ForceDir");
    }

    /* coerce rpc errors as timeouts - check ret codes */
    {
	mgrp->CheckResult();
	errorCode = CheckRetCodes((unsigned long *)mgrp->rrcc.retcodes, 
				  mgrp->rrcc.hosts, hosts);
    }
    return(errorCode);
}

PRIVATE int WEResPhase2(res_mgrpent *mgrp, ViceFid *Fid, 
		      unsigned long *successHosts, ViceStoreId *stid) {

    LogMsg(9, SrvDebugLevel, stdout,  "Entering ResPhase2(%x.%x)", Fid->Vnode, Fid->Unique);
    ViceVersionVector UpdateSet;
    /* form the update set */
    {
	bzero((void *)&UpdateSet, sizeof(ViceVersionVector));
	
	for (int i = 0; i < VSG_MEMBERS; i++)
	    if (successHosts[i])
		(&(UpdateSet.Versions.Site0))[i] = 1;
    }
    
    MRPC_MakeMulti(COP2_OP, COP2_PTR, VSG_MEMBERS, 
		   mgrp->rrcc.handles, mgrp->rrcc.retcodes,
		   mgrp->rrcc.MIp, 0, 0, stid, &UpdateSet);     

    /* check return codes */
    {
	mgrp->CheckResult();
	unsigned long hosts[VSG_MEMBERS];
	int error = CheckRetCodes((unsigned long *)mgrp->rrcc.retcodes, mgrp->rrcc.hosts, 
				  hosts);
	return(error);
    }
}

extern int comparedirreps;
PRIVATE int CompareDirContents(SE_Descriptor *sidvar_bufs) {
    LogMsg(9, SrvDebugLevel, stdout,  "Entering CompareDirContents()");

    if (!comparedirreps) return(0);

    if (SrvDebugLevel > 9) {
	/* dump contents to files */
	for (int j = 0; j < VSG_MEMBERS; j++) {
	    int length = sidvar_bufs[j].Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqLen;
	    if (length) {
		char fname[256];
		sprintf(fname, "/tmp/dir%d", j);
		int fd = open(fname, O_CREAT | O_TRUNC | O_RDWR, 0777);
		assert(fd > 0);
		write(fd, sidvar_bufs[j].Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqBody, 
		      length);
		close(fd);
	    }
	}
    }
    int replicafound = 0;
    char *firstreplica = 0;
    int firstreplicasize = 0;
    for (int i = 0; i < VSG_MEMBERS; i++) {
	int len = sidvar_bufs[i].Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqLen;
	char *buf = (char *)sidvar_bufs[i].Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqBody;
	
	if (len) {
	    if (!replicafound) {
		replicafound = 1;
		firstreplica  = buf;
		firstreplicasize = len;
	    } else {
		int cmplen = (len < firstreplicasize) ? len : firstreplicasize;
		if (bcmp(firstreplica, buf, cmplen)) {
		    LogMsg(0, SrvDebugLevel, stdout,  
			   "DirContents ARE DIFFERENT");
		    return(-1);
		}
	    }
	}
    }
    return(0);
}

long OldDirResolve(res_mgrpent *mgrp, ViceFid *Fid, ViceVersionVector **VV) {

    int reserror = 1;

    /* regenerate VVs for host set */
    for (int i = 0; i < VSG_MEMBERS; i++) 
	if (!mgrp->rrcc.hosts[i])
	    VV[i] = NULL;

    UpdateRunts(mgrp, VV, Fid);

    /* check if any object already inc */
    {
	if (AlreadyIncGroup(VV, VSG_MEMBERS)) {
	    LogMsg(0, SrvDebugLevel, stdout,  "OldDirResolve: Group already inconsistent");
	    goto Exit;
	}
    }
    /* checking if vv's already equal */
    {
	LogMsg(9, SrvDebugLevel, stdout,  "DirResolve: Checking if Objects equal ");
	ViceVersionVector *vv[VSG_MEMBERS];
	for (int i = 0; i < VSG_MEMBERS; i++) 
	    vv[i] = VV[i];
	int HowMany = 0;
	if (VV_Check(&HowMany, vv, 1) == 1) {
	    LogMsg(0, SrvDebugLevel, stdout,  "OldDirResolve: VECTORS ARE ALREADY EQUAL");
	    return(0);
	}
    }

    LogMsg(9, SrvDebugLevel, stdout,  "OldDirResolve: Checking for weak Equality");
    if (IsWeaklyEqual(VV, VSG_MEMBERS)) {
	unsigned long hosts[VSG_MEMBERS];
	ViceStoreId stid;
	LogMsg(39, SrvDebugLevel, stdout,  "DirResolve: WEAKLY EQUAL DIRECTORIES");
	reserror = WEResPhase1(VV, mgrp, Fid, hosts, &stid);

	if (!reserror && !mgrp->GetHostSet(hosts)){
	    reserror = WEResPhase2(mgrp, Fid, hosts, &stid);
	    if (reserror) {
		LogMsg(0, SrvDebugLevel, stdout,  "OldDirResolve: error %d in (WE)ResPhase2",
			reserror);
		goto Exit;
	    }
	    else 
		return(0);
	}
    }
  Exit:
    if (reserror) {
	MRPC_MakeMulti(MarkInc_OP, MarkInc_PTR, VSG_MEMBERS,
		       mgrp->rrcc.handles, mgrp->rrcc.retcodes, 
		       mgrp->rrcc.MIp, 0, 0, Fid);
	reserror = EINCONS;
    }
    LogMsg(9, SrvDebugLevel, stdout,  "OldDirResolve returns %d", reserror);
    return(reserror);
}
    
	    
