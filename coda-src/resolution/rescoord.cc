/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently
#*/

/* 
 * rescoord.c
 *	Implements the coordinator side for 
 *	directory resolution
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include "coda_assert.h"
#include <stdio.h>
#include <struct.h>
#include <lwp/lwp.h>
#include <rpc2/rpc2.h>
#include <util.h>
#include <codadir.h>
#include "coda_string.h"

#ifdef __cplusplus
}
#endif

#include <olist.h>
#include <rpc2/errors.h>
#include <srv.h>
#include <inconsist.h>
#include <vlist.h>
#include <operations.h>
#include <res.h>
#include <treeremove.h>

#include "rescomm.h"
#include "rescoord.h"
#include "resutil.h"
#include "resforce.h"
#include "timing.h"

timing_path *tpinfo = 0;
timing_path *FileresTPinfo = 0; 

/* return 1 if a vector in a group show inconsistency */
static int AlreadyIncGroup(ViceVersionVector **VV, int nvvs)
{
    for (int i = 0; i < nvvs; i++) {
	if (VV[i] == NULL) 
	    continue;
	if (IsIncon((*(VV[i])))) 
	    return(1);
    }
    return(0);
}

/* return 1 if all the vectors in a group show inconsistency */
static int AllIncGroup(ViceVersionVector **VV, int nvvs)
{
    for (int i = 0; i < nvvs; i++) {
	if (VV[i] == NULL)
	    continue;
	if (!IsIncon((*(VV[i]))))
	    return(0);
    }
    return(1);
}

static void PrintDirStatus(ViceStatus *status)
{
    SLog(0, "LinkCount(%d), Length(%d), Author(%u), Owner(%u), Mode(%u), Parent(0x%x.%x)",
	   status->LinkCount, status->Length, status->Author, status->Owner,
	   status->Mode, status->vparent, status->uparent);
    PrintVV(stdout, &(status->VV));
}

static int CompareDirStatus(ViceStatus *status, res_mgrpent *mgrp, ViceVersionVector **VV)
{
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
//		    (status[i].Length != status[dirfound].Length) ||
		    (status[i].Author != status[dirfound].Author) ||
		    (status[i].Owner != status[dirfound].Owner) ||
		    (status[i].Mode != status[dirfound].Mode) ||
		    (status[i].vparent != status[dirfound].vparent) ||
		    (status[i].uparent != status[dirfound].uparent) ||
		    (VV_Cmp_IgnoreInc(&status[i].VV, &status[dirfound].VV) != VV_EQ)) {
		    SLog(0, "CompareDirStatus: Status blocks are different");
		    PrintDirStatus(&status[i]);
		    PrintDirStatus(&status[dirfound]);
		    return(-1);
		}
	    }
	}
    }
    return(0);
}

static void DumpDirContents(SE_Descriptor *sid_bufs, ViceFid *fid)
{
    for (int j = 0; j < VSG_MEMBERS; j++) {
	int length = sid_bufs[j].Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqLen;
	if (length) {
	    char fname[256];
	    sprintf(fname, "/tmp/dir.0x%lx.0x%lx.%d", fid->Vnode, fid->Unique, j);
	    int fd = open(fname, O_CREAT | O_TRUNC | O_RDWR, 0777);
	    CODA_ASSERT(fd > 0);
	    write(fd, sid_bufs[j].Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqBody, 
		  length);
	    close(fd);
	}
    }
}

/* This function is shared by both rescoord.cc and rvmrescoord.cc */
extern int comparedirreps;
int CompareDirContents(SE_Descriptor *sid_bufs, ViceFid *fid)
{
    SLog(9, "Entering CompareDirContents()");

    if (!comparedirreps) return(0);

    if (SrvDebugLevel > 9) 
	// dump contents to files 
	DumpDirContents(sid_bufs, fid);
    int replicafound = 0;
    DirHeader *firstreplica = NULL;
    int firstreplicasize = 0;
    for (int i = 0; i < VSG_MEMBERS; i++) {
	int len = sid_bufs[i].Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqLen;
	DirHeader *buf = (DirHeader *)sid_bufs[i].Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqBody;
	
	if (len) {
	    if (!replicafound) {
		replicafound = 1;
		firstreplica  = buf;
		firstreplicasize = len;
	    }
	    else {
		if (DIR_Compare(firstreplica, buf)) {
		    SLog(0, "CompareDirContents: DirContents ARE DIFFERENT");
		    if (SrvDebugLevel > 9) {
			DIR_Print(firstreplica, stdout);
			DIR_Print(buf, stdout);
		    }
		    return(-1);
		}
                if (memcmp((char *)firstreplica + DIR_Length(firstreplica),
                           (char *)buf + DIR_Length(buf), VAclSize(NULL)) != 0)
                {
		    SLog(0, "CompareDirContents: ACL's are DIFFERENT");
		    /* XXX ACL equality test is broken. same ACLs could be
		     * represented differently. We need to enumerate through
		     * all the entries of one replica and check this against
		     * the other replica. --JH */
		    //return(-1);
                }
	    }
	}
    }
    return(0);
}

// XXXXX adapted from dir.private.h
#define MAXPAGES 128
#ifdef PAGESIZE
#undef PAGESIZE
#endif
#define PAGESIZE 2048

static int ResolveInc(res_mgrpent *mgrp, ViceFid *Fid, ViceVersionVector **VV)
{
    SE_Descriptor sid;
    char *dirbufs[VSG_MEMBERS];
    int dirlength = MAXPAGES * PAGESIZE + VAclSize(foo);
    ViceStatus status;
    int DirsEqual = 0;
    ViceVersionVector *newVV;
    int size;
    unsigned long succflags[VSG_MEMBERS];
    int errorcode = EINCONS;

    // make all replicas inconsistent
    if (!AllIncGroup(VV, VSG_MEMBERS)) {
	SLog(5, "ResolveInc: Not all replicas of (%s) are inconsistent yet",
	     FID_(Fid));
	MRPC_MakeMulti(MarkInc_OP, MarkInc_PTR, VSG_MEMBERS,
		       mgrp->rrcc.handles, mgrp->rrcc.retcodes,
		       mgrp->rrcc.MIp, 0, 0, Fid);
    }
    
    // set up buffers to get dir contents & status blocks
    {
	memset(&sid, 0, sizeof(SE_Descriptor));
	sid.Tag = SMARTFTP;
	sid.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
	sid.Value.SmartFTPD.Tag = FILEINVM;
	sid.Value.SmartFTPD.ByteQuota = -1;
    }
    ARG_MARSHALL(IN_OUT_MODE, SE_Descriptor, sidvar, sid, VSG_MEMBERS);
    
    for (int i = 0; i < VSG_MEMBERS; i++)  {
	if (mgrp->rrcc.handles[i]) {
	    dirbufs[i] = (char *)malloc(dirlength);
	    CODA_ASSERT(dirbufs[i]);
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
		       mgrp->rrcc.MIp, 0, 0, Fid, sizevar_ptrs, statusvar_ptrs,
		       sidvar_bufs);
	mgrp->CheckResult();
	if (CheckRetCodes((unsigned long *)mgrp->rrcc.retcodes, mgrp->rrcc.hosts, 
			  succflags)) {
	    SLog(0, "ResolveInc: Error during FetchDirContents");
	    goto Exit;
	}
    }
    
    // compare the contents 
    {
	if (CompareDirStatus(statusvar_bufs, mgrp, &newVV) == 0) {
	    if (CompareDirContents(sidvar_bufs, Fid) == 0) {
		SLog(0, "ResolveInc: Dir contents are equal");
		DirsEqual = 1;
	    }
	    else 
		SLog(0, "ResolveInc: Dir contents are unequal");
	}
	else 
	    SLog(0, "ResolveInc: Dir Status blocks are different");
    }
    // clear inconsistency if equal
    {
	if (DirsEqual) {
	    MRPC_MakeMulti(ClearIncon_OP, ClearIncon_PTR, VSG_MEMBERS,
			   mgrp->rrcc.handles, mgrp->rrcc.retcodes,
			   mgrp->rrcc.MIp, 0, 0, Fid, newVV);
	    mgrp->CheckResult();
	    errorcode = CheckRetCodes((unsigned long *)mgrp->rrcc.retcodes, mgrp->rrcc.hosts, succflags);
	}
    }
  Exit:
    // free all the allocate dir bufs
  { /* drop scope for int i below; to avoid identifier clash */
    for (int i = 0 ; i < VSG_MEMBERS; i++) 
	if (dirbufs[i]) 
	    free(dirbufs[i]);
  } /* drop scope for int i above; to avoid identifier clash */

    SLog(0, "ResolveInc: returns(%d)\n", errorcode);
    return(errorcode);
}

/* two VV's are weakly equal if they have the same store-id: 
   this means that the files are identical, but the COP2 never made 
   it to the server
*/

/* Function used by rescoord.cc and resfile.cc */
int IsWeaklyEqual(ViceVersionVector **VV, int nvvs)
{
    int i, j;

    SLog(10,  "Entering IsWeaklyEqual()");

    /* find first one */
    for (i = 0; i < nvvs; i++)
	if (VV[i])
            break; 

    /* compare to all others */
    for (j = i + 1; j < nvvs; j++) {
	    if (VV[j] && memcmp(&(VV[i]->StoreId), &(VV[j]->StoreId), 
                                sizeof(ViceStoreId)) != 0)
            {
		    SLog(10,  "IsWeaklyEqual returning 0");
		    return 0;
	    }
    }

    /* all store-id's are identical, we have weakly equal VVs */
    SLog(10,  "IsWeaklyEqual returning 1.");
    return(1);
}

int WEResPhase1(ViceFid *Fid, ViceVersionVector **VV, 
		res_mgrpent *mgrp, unsigned long *hosts,
		ViceStoreId *stid, ResStatus **rstatusp) 
{
	int errorCode = 0;
	ViceVersionVector newvv;
	ViceStatus vstatus;
	memset(&vstatus, 0, sizeof(ViceStatus));

	SLog(9,  "Entering WEResPhase1 for %s", FID_(Fid));

	if (rstatusp) {
	    unsigned long succflags[VSG_MEMBERS];
	    CheckRetCodes((unsigned long *)mgrp->rrcc.retcodes, mgrp->rrcc.hosts, 
			  succflags);
	    GetResStatus(succflags, rstatusp, &vstatus);
	}

	/* force a new vv */
	GetMaxVV(&newvv, VV, -1);
        if (stid)
	    *stid = newvv.StoreId;

	MRPC_MakeMulti(ForceVV_OP, ForceVV_PTR, VSG_MEMBERS, 
		       mgrp->rrcc.handles, mgrp->rrcc.retcodes,
		       mgrp->rrcc.MIp, 0, 0, Fid, &newvv, &vstatus);
	SLog(9,  "WEResPhase1 returned from ForceVV");
	
	/* coerce rpc errors as timeouts - check ret codes */
	mgrp->CheckResult();
	errorCode = CheckRetCodes((unsigned long *)mgrp->rrcc.retcodes, 
				  mgrp->rrcc.hosts, hosts);
	SLog(9,  "WEResPhase1 returning %d", errorCode);
	return(errorCode);
}

static int WEResPhase2(res_mgrpent *mgrp, ViceFid *Fid, 
		       unsigned long *successHosts, ViceStoreId *stid) 
{
	int i;
	ViceVersionVector UpdateSet;
	unsigned long hosts[VSG_MEMBERS];
	int error;

	SLog(9,  "Entering ResPhase2 %s", FID_(Fid));
	/* form the update set */
	memset((void *)&UpdateSet, 0, sizeof(ViceVersionVector));
	
	for (i = 0; i < VSG_MEMBERS; i++)
		if (successHosts[i])
			(&(UpdateSet.Versions.Site0))[i] = 1;
    
	MRPC_MakeMulti(COP2_OP, COP2_PTR, VSG_MEMBERS, 
		       mgrp->rrcc.handles, mgrp->rrcc.retcodes,
		       mgrp->rrcc.MIp, 0, 0, stid, &UpdateSet);     

	mgrp->CheckResult();
	error = CheckRetCodes((unsigned long *)mgrp->rrcc.retcodes, 
			      mgrp->rrcc.hosts, 
			      hosts);
	return(error);
}

/* This function is shared by both rescoord.cc and rvmrescoord.cc */
/* Resolves all kinds of weak equality, runts, and VV already equal cases */
int RegDirResolution(res_mgrpent *mgrp, ViceFid *Fid, ViceVersionVector **VV,
		     ResStatus **rstatusp, int *logresreq)
{
    SLog(1, "Entering RegDirResolution for (%s)", FID_(Fid));
    ViceVersionVector *vv[VSG_MEMBERS];
    int HowMany = 0;
    int done = 0;
    int ret = 0;

    for (int i = 0; i < VSG_MEMBERS; i++) 
	if (!mgrp->rrcc.hosts[i])
	    VV[i] = NULL;
    
    UpdateRunts(mgrp, VV, Fid);

    // check if any object already inc 
    if (AlreadyIncGroup(VV, VSG_MEMBERS)) {
	SLog(0, "RegDirResolution: Group already inconsistent");
	ret = ResolveInc(mgrp, Fid, VV);
	goto Exit_Resolved;
    }

    // checking if vv's already equal 
    SLog(9, "RegDirResolution: Checking if Objects are equal");
    for (int i = 0; i < VSG_MEMBERS; i++) 
	vv[i] = VV[i];

    if (VV_Check(&HowMany, vv, 1) == 1) {
	SLog(0, "RegDirResolution: VECTORS ARE ALREADY EQUAL");
	for (int i = 0; i < VSG_MEMBERS; i++) 
	    if (vv[i])
		PrintVV(stdout, vv[i]);

	/* We might have been looking at only a subset of the mgrp, by
	 * kicking off a server probe we should succeed the next time a
	 * ViceResolve is triggered. (at least that is what I assume the
	 * code is doing here) -JH */
	LWP_NoYieldSignal((char *)ResCheckServerLWP);
	goto Exit_Resolved;
    }

    //check for weak equality
    SLog(9, "RegDirResolution: Checking for weak Equality");
    if (!IsWeaklyEqual(VV, VSG_MEMBERS))
	goto Exit;

    SLog(39, "RegDirResolution: WEAKLY EQUAL DIRECTORIES");

    unsigned long hosts[VSG_MEMBERS];
    ViceStoreId stid;
    ret = WEResPhase1(Fid, VV, mgrp, hosts, &stid, rstatusp);
    if (ret || mgrp->GetHostSet(hosts)) {
	SLog(0, "RegDirResolution: error %d in (WE)ResPhase1()", ret);
	goto Exit;
    }

    if (!rstatusp) {
	/* OldDirResolve case, i.e. there are no RVM resolution logs so we
	 * have to do a COP2 here */
	ret = WEResPhase2(mgrp, Fid, hosts, &stid);
	if (ret) {
	    SLog(0,  "RegDirResolve: error %d in (WE)ResPhase2", ret);
	    goto Exit;
	}
    }
Exit_Resolved:
    done = 1;
Exit:
    SLog(1, "RegDirResolution: Further resolution %srequired: errorCode = %d",
	 done ? "not " : "", ret);

    if (logresreq)
	*logresreq = !done;
    return ret;
}

long OldDirResolve(res_mgrpent *mgrp, ViceFid *Fid, ViceVersionVector **VV)
{
    long ret;
    int logresreq;

    /* No resolution logs, we can only try to resolve the trivial cases */
    ret = RegDirResolution(mgrp, Fid, VV, NULL, &logresreq);
    if (logresreq || (ret && ret != EINCONS)) {
	SLog(9,  "OldDirResolution marking %s as conflict", FID_(Fid));
	MRPC_MakeMulti(MarkInc_OP, MarkInc_PTR, VSG_MEMBERS,
		       mgrp->rrcc.handles, mgrp->rrcc.retcodes, 
		       mgrp->rrcc.MIp, 0, 0, Fid);
	ret = EINCONS;
    }
    return ret;
}

