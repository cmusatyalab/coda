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

static char *rcsid = "$Header: /home/braam/src/coda-src/rvmres/RCS/resfile.cc,v 1.4 1996/12/09 17:33:14 braam Exp $";
#endif /*_BLURB_*/






/********************************************************
 * resfile.c						*
 * Implements File resolution.				*
 ********************************************************/

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <stdio.h>

#ifdef	__MACH__
#include <libc.h>
#endif	/* __MACH__ */

#if defined(__linux__) || defined(__NetBSD__)
#include <unistd.h>
#include <stdlib.h>
#endif /* __NetBSD__ || LINUX */

#include <struct.h>

#ifdef CAMELOT
#include <cam/camelot_prefixed.h>
#include <camlib/camlib_prefixed.h>
#include <cam/_setjmp.h>
#endif CAMELOT
#if 0
#include <cthreads.h>
#else
#include <dummy_cthreads.h>
#endif

#include <rpc2.h>
#include <se.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>

#ifndef CAMELOT
#include <rvmlib.h>
#endif CAMELOT 

#include <olist.h>
#include <errors.h>
#include <vcrcommon.h>
#include <srv.h>
#include <coda_dir.h>
#include <vrdb.h>
#include <inconsist.h>

#include <rescomm.h>
#include <resutil.h>
#include <res.h>
#include <pdlist.h>
#include <reslog.h>
#include <remotelog.h>
#include <resforce.h>
#include <timing.h>

#include "resstats.h"
/* declarations of routines */
PRIVATE int IncVVGroup(ViceVersionVector **, int *);
PRIVATE void SetResStatus(Vnode *, ResStatus *);
PRIVATE void UpdateStats(ViceFid *, fileresstats *);    

/* FILE RESOLUTION 
 *	Look at Version Vectors from all hosts;
 *	If the set is weakly equal, set new vv for all sites;
 *	If the set is inconsistent, mark all replicas with special flag 
 *	If there is a dominant version, distribute it to all sites (COP1)
 *		and perform COP2;
 */
long FileResolve(res_mgrpent *mgrp, ViceFid *Fid, 
		 ViceVersionVector **VV) {
    int	dix = 0;    /* dominant file's index in the canonical order */
    SE_Descriptor sid;
    int errorcode = 0;
    fileresstats frstats;
    char filename[50];
    int tmp_fd;     /* For mkstemp */

    PROBE(FileresTPinfo, COORDSTARTFILERES); 
    // statistics stuff 
    frstats.file_nresolves++;
    if (mgrp->IncompleteVSG()) frstats.file_incvsg++;

    /* regenerate VVs for host set */
    for (int i = 0; i < VSG_MEMBERS; i++) 
	if (!mgrp->rrcc.hosts[i])
	    VV[i] = NULL;
    
    if (IsWeaklyEqual(VV, VSG_MEMBERS)) {
	unsigned long hosts[VSG_MEMBERS];
	errorcode = WERes(Fid, VV, NULL, mgrp, hosts);
	frstats.file_we++;
    }
    else if (IncVVGroup(VV, &dix)){
	assert(dix != -1);
	errorcode = EINCONS;
    }
    else {				/* regular file resolution */
	ResStatus Status;

	// statistics collection stuff 
	{
	    int isrunt[VSG_MEMBERS];
	    int nonruntfile = 0;
	    if (RuntExists(VV, VSG_MEMBERS, isrunt, &nonruntfile)) 
		frstats.file_runtforce++;
	    else 
		frstats.file_reg++;
	}
	PROBE(FileresTPinfo, COORDSTARTFILEFETCH);
	/* fetch dominant file */
	{
	    bzero(&sid, sizeof(SE_Descriptor));
	    sid.Tag = SMARTFTP;
	    sid.Value.SmartFTPD.Tag = FILEBYNAME;
	    sid.Value.SmartFTPD.FileInfo.ByName.ProtectionBits = 0644;
	    sid.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
	    sid.Value.SmartFTPD.ByteQuota = -1;

	    /**********************************************
	     * WARNING: RACE CONDITION	  (raiff 8/26/96)
	     * DO NOT CHANGE
	     *
	     * Using mktemp can cause a race condition between 2
	     * resolve threads.  The first thread gets a temp name and
	     * yields before creating the file, so the second thread
	     * gets the same name.  The last thread to complete will
	     * then fail at the 'stat' below.
	     * Creating the file immediately with mkstemp will
	     * alieviate this. 
	     */
	     
	    sprintf(filename, "/tmp/fforceXXXXXX");
	    tmp_fd = mkstemp(filename);
	    close(tmp_fd);
	    strcpy(sid.Value.SmartFTPD.FileInfo.ByName.LocalFileName,
		   filename);
	    /* right now just use the connection from the mgrp -
	       implement the uni-connection groups  --- PUNEET */
	    if (errorcode = Res_FetchFile(mgrp->rrcc.handles[dix], Fid, 
					  mgrp->rrcc.hosts[dix], &Status, &sid)){
		LogMsg(0, SrvDebugLevel, stdout,  "FileResolve: Error %d in fetchfile", 
			errorcode);
		assert(dix != -1);
		goto EndFileResolve;
	    }
	}
	PROBE(FileresTPinfo, COORDENDFILEFETCH);
	/* force new file and VV to all sites - cop1 , cop2 */
	{
	    ViceVersionVector newvv;
	    GetMaxVV(&newvv, VV, dix);
	    struct stat buf;
	    assert(stat(filename, &buf) == 0);
	    int length = buf.st_size;
	    sid.Value.SmartFTPD.TransmissionDirection = CLIENTTOSERVER;
	    LogMsg(9, SrvDebugLevel, stdout,  "FileResolve: Going to force file");
	    ARG_MARSHALL(IN_OUT_MODE, SE_Descriptor, sidvar, sid, VSG_MEMBERS);
	    {	/* 
		 * find the dominant set, and omit the file transfer to hosts
		 * that already have a dominant copy.  VV_Check should find
		 * no inconsistencies.  However, it is possible to
		 * have a weakly equal set of vectors after excluding
		 * the submissive ones.  Allow this case raiff 8/16/96
		 */
		int HowMany = 0;
		assert(VV_Check(&HowMany, VV, 0) || IsWeaklyEqual(VV, VSG_MEMBERS));
		LogMsg(0, SrvDebugLevel, stdout, "FileResolve: %d dominant copies",
		       HowMany);
		for (int i = 0; i < VSG_MEMBERS; i++) 
		    if (VV[i]) sidvar_bufs[i].Tag = OMITSE;
	    }

	    MRPC_MakeMulti(ForceFile_OP, ForceFile_PTR, VSG_MEMBERS,
			   mgrp->rrcc.handles, mgrp->rrcc.retcodes, 
			   mgrp->rrcc.MIp, 0, 0, Fid, ResStoreData, 
			   length, &newvv, &Status, sidvar_bufs);
	    
	    LogMsg(9, SrvDebugLevel, stdout,  "FileResolve: Returned from ForceFile");
	    /* coerce rpc errors as timeouts */
	    mgrp->CheckResult();
	    /* collect replies and do cop2 */
	    unsigned long hosts[VSG_MEMBERS];
	    errorcode = CheckRetCodes((unsigned long *)mgrp->rrcc.retcodes,
				      mgrp->rrcc.hosts, hosts);
	    mgrp->GetHostSet(hosts);
	    PROBE(FileresTPinfo, COORDENDFORCEFILE);
	    /* do cop2 */
	    if (!errorcode){
		ViceVersionVector UpdateSet;
		bzero(&UpdateSet, sizeof(ViceVersionVector));
		for (int i = 0; i < VSG_MEMBERS; i++)
		    if (hosts[i])
			(&(UpdateSet.Versions.Site0))[i] = 1;
		ViceStoreId stid = newvv.StoreId;
		MRPC_MakeMulti(COP2_OP, COP2_PTR, VSG_MEMBERS, 
			       mgrp->rrcc.handles, mgrp->rrcc.retcodes,
			       mgrp->rrcc.MIp, 0, 0, &stid,&UpdateSet);
	    }
	}
    }

EndFileResolve:
    unlink(filename);
    if (errorcode) {
	LogMsg(0, SrvDebugLevel, stdout,  "FileResolve: Marking object 0x%x.%x.%x inconsistent",
		Fid->Volume, Fid->Vnode, Fid->Unique);
	/* inconsistency - mark it and return */
	MRPC_MakeMulti(MarkInc_OP, MarkInc_PTR, VSG_MEMBERS,
		       mgrp->rrcc.handles, mgrp->rrcc.retcodes, 
		       mgrp->rrcc.MIp, 0, 0, Fid);
	frstats.file_conf++;
	errorcode = EINCONS;
    }
    else frstats.file_nsucc++;
    UpdateStats(Fid, &frstats);
    PROBE(FileresTPinfo, COORDENDFILERES);
    return(errorcode);
}

/* fetch status and data for given fid */
long RS_FetchFile(RPC2_Handle RPCid, ViceFid *Fid, 
		  RPC2_Unsigned PrimaryHost, ResStatus *Status, 
		  SE_Descriptor *BD) {
    
    SE_Descriptor sid;
    Volume *volptr = 0;
    Vnode *vptr = 0;
    int errorcode = 0;
    Inode tmpinode = 0;

    LogMsg(9, SrvDebugLevel, stdout,  "RS_FetchFile(%x.%x.%x %x)", Fid->Volume,
	     Fid->Vnode, Fid->Unique, PrimaryHost);

    LogMsg(9, SrvDebugLevel, stdout,  "RS_FetchFile ThisHostAddr = %x", ThisHostAddr);
    if (PrimaryHost != ThisHostAddr) return 0;
    assert(!ISDIR(*Fid));
    if (!XlateVid(&Fid->Volume)) {
	LogMsg(0, SrvDebugLevel, stdout,  "RS_FetchFile: Couldnt xlate vid %x", Fid->Volume);
	return(EINVAL);
    }
    
    /* no need for access checks for resolution subsystem */
    if (errorcode = GetFsObj(Fid, &volptr, &vptr, READ_LOCK, NO_LOCK, 0, 0)){
	errorcode = EINVAL;
	LogMsg(0, SrvDebugLevel, stdout,  "RS_FetchFile Error in GetFsObj %d", errorcode);
	goto FreeLocks;
    }

    /* Do the file Transfer */
    bzero(&sid, sizeof(SE_Descriptor));
    sid.Tag = SMARTFTP;
    sid.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
    sid.Value.SmartFTPD.Tag = FILEBYINODE;
    sid.Value.SmartFTPD.FileInfo.ByInode.Device = V_device(volptr);

    if (vptr->disk.inodeNumber)
	sid.Value.SmartFTPD.FileInfo.ByInode.Inode = vptr->disk.inodeNumber;
    else {
	/* no inode for this file - send over an empty file */
	tmpinode = icreate(V_device(volptr), 0, V_id(volptr), 
			   vptr->vnodeNumber, vptr->disk.uniquifier, 0);
	sid.Value.SmartFTPD.FileInfo.ByInode.Inode = tmpinode;
    }
    
    if ((errorcode = RPC2_InitSideEffect(RPCid, &sid)) <= RPC2_ELIMIT) {
	LogMsg(0, SrvDebugLevel, stdout,  "RS_FetchFile: InitSideEffect failed %d", 
		errorcode);
	errorcode = EINVAL;
	goto FreeLocks;
    }

    if ((errorcode = RPC2_CheckSideEffect(RPCid, &sid, SE_AWAITLOCALSTATUS)) <= RPC2_ELIMIT) {
	if (errorcode == RPC2_SEFAIL1) errorcode = EIO;
	LogMsg(0, SrvDebugLevel, stdout,  "RS_FetchFile: CheckSideEffect failed %d",
		errorcode);
	goto FreeLocks;
    }
    /* set OUT parameters */
    SetResStatus(vptr, Status);
    
FreeLocks:
    if (vptr){
	if (tmpinode){
	    LogMsg(9, SrvDebugLevel, stdout,  "RS_FetchFile: Getting rid of tmp inode");
	    assert(!(idec(V_device(volptr), tmpinode,
			  V_parentId(volptr))));
	}
	Error filecode = 0;
	VPutVnode(&filecode, vptr);
	assert(filecode == 0);
    }
    PutVolObj(&volptr, NO_LOCK);
    LogMsg(9, SrvDebugLevel, stdout,  "RS_FetchFile Returns %d", errorcode);
    return(errorcode);
}

long RS_ForceFile(RPC2_Handle RPCid, ViceFid *Fid, 
		  ResStoreType Request, RPC2_Integer Length, 
		  ViceVersionVector *VV, ResStatus *Status, 
		  SE_Descriptor *BD) {
    
    Vnode *vptr = 0;
    Volume *volptr = 0;
    VolumeId VSGVolnum = Fid->Volume;
    int errorcode = 0;
    SE_Descriptor sid;
    int status = 0;
    ViceVersionVector *DiffVV = 0;
    Inode newinode = 0;
    Inode oldinode = 0;
    Device device;
    VolumeId parentId;		
    int res = 0;

    assert(Request == ResStoreData);
    conninfo *cip = GetConnectionInfo(RPCid);
    if (cip == NULL){
	LogMsg(0, SrvDebugLevel, stdout,  "RS_ForceFile: Couldnt get conninfo ");
	return(EINVAL);
    }
    if (!XlateVid(&Fid->Volume)) {
	LogMsg(0, SrvDebugLevel, stdout,  "RS_ForceFile: Couldnt Xlate VSG %x", 
		Fid->Volume);
	return(EINVAL);
    }
    
    /* get object */
    if (errorcode = GetFsObj(Fid, &volptr, &vptr, WRITE_LOCK, NO_LOCK, 0, 0)){
	LogMsg(0, SrvDebugLevel, stdout,  "RS_ForceFile: GetFsObj returns error %d", errorcode);
	errorcode = EINVAL;
	goto FreeLocks;
    }

    /* make sure Volume is locked by coordinator */
    if (V_VolLock(volptr).IPAddress != cip->GetRemoteHost()) {
	LogMsg(0, SrvDebugLevel, stdout,  "RS_ForceFile: Volume not locked by coordinator");
	errorcode = EINVAL;
	goto FreeLocks;
    }
    /* no protection checks - meta data including VV is an IN parameter */
    assert(vptr->disk.type == vFile || vptr->disk.type == vSymlink);

    /* make sure new vv is dominant/equal to current vv */
    res = VV_Cmp(&Vnode_vv(vptr), VV);
    if (res != VV_EQ) {
	if (res == VV_SUB){
	    DiffVV = new ViceVersionVector;
	    *DiffVV = *VV;
	    SubVVs(DiffVV, &Vnode_vv(vptr));
	}
	else {
	    errorcode = EINCOMPATIBLE;
	    LogMsg(0, SrvDebugLevel, stdout,  "RS_ForceFile: Version Vectors are inconsistent");
	    goto FreeLocks;
	}
    }

    CodaBreakCallBack(0, Fid, VSGVolnum);

    if (res != VV_EQ) {
	assert(res == VV_SUB);

	/* make space for new file */
	newinode = icreate(V_device(volptr), 0, V_id(volptr), 
			    vptr->vnodeNumber, vptr->disk.uniquifier,
			    vptr->disk.dataVersion + 1);
	assert(newinode > 0);

	/* adjust the disk block count by the difference in the files */
	if(errorcode = AdjustDiskUsage(volptr, (nBlocks(Length) - nBlocks(vptr->disk.length)))) {
	    LogMsg(0, SrvDebugLevel, stdout,  "RS_ForceFile: Error %d in AdjustDiskUsage", errorcode);
	    goto FreeLocks;
	}

	/* fetch the file to be forced */
	{	
	    /* set up the SFTP structure */
	    bzero(&sid, sizeof(SE_Descriptor));
	    sid.Tag = SMARTFTP;
	    sid.Value.SmartFTPD.TransmissionDirection = CLIENTTOSERVER;
	    sid.Value.SmartFTPD.SeekOffset = 0;
	    sid.Value.SmartFTPD.Tag = FILEBYINODE;
	    sid.Value.SmartFTPD.ByteQuota = Length;
	    sid.Value.SmartFTPD.FileInfo.ByInode.Device = V_device(volptr);
	    sid.Value.SmartFTPD.FileInfo.ByInode.Inode = newinode;
	    if ((errorcode = RPC2_InitSideEffect(RPCid, &sid)) <= RPC2_ELIMIT) {
		ChangeDiskUsage(volptr, (nBlocks(vptr->disk.length)-nBlocks(Length)));
		LogMsg(0, SrvDebugLevel, stdout,  "RS_ForceFile Error in InitSideEffect %d", errorcode);
		goto FreeLocks;
	    }
	    if ((errorcode = RPC2_CheckSideEffect(RPCid, &sid, SE_AWAITLOCALSTATUS)) <= RPC2_ELIMIT) {
		if (errorcode == RPC2_SEFAIL1) errorcode = EIO;
		ChangeDiskUsage(volptr, (nBlocks(vptr->disk.length) - nBlocks(Length)));
		LogMsg(0, SrvDebugLevel, stdout,  "RS_ForceFile: Error %d in CheckSideEffect", 
			errorcode);
		goto FreeLocks;
	    }

	    LogMsg(9, SrvDebugLevel, stdout,  "RS_ForceFile: Transferred %d bytes", 
		    sid.Value.SmartFTPD.BytesTransferred);
	}
	/* do disk inode stuff */
	{
	    if (nBlocks(Length) != nBlocks(sid.Value.SmartFTPD.BytesTransferred)) {
		LogMsg(0, SrvDebugLevel, stdout,  "Len from res coordinator(%d) != len transfered(%d) for fid %x.%x.%x", 
			Length, sid.Value.SmartFTPD.BytesTransferred,
			Fid->Volume, Fid->Vnode, Fid->Unique);
		errorcode = EIO;
		ChangeDiskUsage(volptr, (nBlocks(vptr->disk.length) - nBlocks(Length)));
		goto FreeLocks;
	    }

	    if (vptr->disk.inodeNumber != 0) {
		LogMsg(9, SrvDebugLevel, stdout,  "RS_ForceFile: Blowing away old inode %x", 
			vptr->disk.inodeNumber);
		oldinode = vptr->disk.inodeNumber;
	    }

	    vptr->disk.length = sid.Value.SmartFTPD.BytesTransferred;
	    vptr->disk.inodeNumber = newinode;
	    vptr->disk.cloned = 0;		// ForceFile effectively is a COW
	    newinode = 0;
	}

	/* update vnode */

	vptr->disk.owner = Status->Owner;
	vptr->disk.author = Status->Author;
	vptr->disk.dataVersion++;
	vptr->disk.unixModifyTime = Status->Date;
	vptr->disk.modeBits = Status->Mode;

	/* update VV */
	if (DiffVV != 0) {
	    AddVVs(&Vnode_vv(vptr), DiffVV);
	    AddVVs(&V_versionvector(volptr), DiffVV);
	}
    }
    NewCOP1Update(volptr, vptr, &(VV->StoreId));
    
    /* await COP2 from coordinator */
#define	MAXFIDS	4
    ViceFid fids[MAXFIDS];
    bzero(fids, MAXFIDS * sizeof(ViceFid));
    fids[0] = *Fid;
    AddToCopPendingTable(&(VV->StoreId), fids);
#undef MAXFIDS 

FreeLocks:
    if (newinode) {
	assert(!idec(V_device(volptr), newinode, V_parentId(volptr)));
    }
    if (DiffVV) delete DiffVV;

    device = (volptr ? V_device(volptr) : 0);
    parentId = (volptr ? V_parentId(volptr) : 0);
    CAMLIB_BEGIN_TOP_LEVEL_TRANSACTION_2(CAM_TRAN_NV_SERVER_BASED) 
    Error filecode = 0;
    if (vptr) { 
	if (!errorcode) 
	    VPutVnode(&filecode, vptr);
	else
	    VFlushVnode(&filecode, vptr);
	assert(filecode == 0);
    }
    PutVolObj(&volptr, NO_LOCK);
    CAMLIB_END_TOP_LEVEL_TRANSACTION_2(CAM_PROT_TWO_PHASED, status); 
    assert(status == 0);
    if (oldinode && device && parentId)
	assert(!(idec(device, oldinode, parentId)));
    LogMsg(9, SrvDebugLevel, stdout,  "RS_ForceFile returns %d", errorcode);
    return(errorcode);
}


long RS_COP2(RPC2_Handle RPCid, ViceStoreId *StoreId, 
	      ViceVersionVector *UpdateSet) {
    return(InternalCOP2(RPCid, StoreId, UpdateSet));
}

/* there are max VSG_MEMBERS VV pointers */
/* non-NULL VV pointers correspond to real VVs */
PRIVATE int IncVVGroup(ViceVersionVector **VV, int *domindex) {
    *domindex = -1;
    
    for (int i = 0; i < VSG_MEMBERS; i++)
	if (VV[i]) { *domindex = i; break;}
    if (*domindex == -1) return EINVAL;

  { /* drop scope for int i below; to avoid identifier clash */
    for(int i = *domindex; i < VSG_MEMBERS; i++){
	if (VV[i] == NULL) continue;
	int res = VV_Cmp(VV[i], VV[*domindex]);
	if (res == VV_EQ) continue;
	/* check for weak equality */
	if (res == VV_INC){
	    if (!bcmp(&(VV[i]->StoreId), &(VV[*domindex]->StoreId),
		      sizeof(ViceStoreId)))
		continue;
	    else 
		return(VV_INC);
	}
	if (res == VV_SUB) 
	    continue;
	if (res == VV_DOM)
	    *domindex = i;
    }
  } /* drop scope for int i above; to avoid identifier clash */
    return(0);
}

PRIVATE void SetResStatus(Vnode *vptr, ResStatus *Status) {
    Status->status = 0;
    Status->Author = vptr->disk.author;
    Status->Owner = vptr->disk.owner;
    Status->Date = vptr->disk.unixModifyTime;
    Status->Mode = vptr->disk.modeBits;
}

PRIVATE void UpdateStats(ViceFid *Fid, fileresstats *frstats) {
    
    VolumeId vid = Fid->Volume;
    Volume *volptr = 0;
    if (XlateVid(&vid)) {
	if (!GetVolObj(vid, &volptr, VOL_NO_LOCK, 0, 0)) {
	    if (AllowResolution && V_RVMResOn(volptr)) 
		V_VolLog(volptr)->vmrstats->update(frstats);
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
