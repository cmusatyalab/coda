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





/* resclient.c
 * 	Code implementing the client part of resolution.
 */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include "coda_assert.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <struct.h>
#ifndef __CYGWIN32__
#include <dirent.h>
#endif

#include <lwp/lwp.h>
#include <rpc2/rpc2.h>
#include <inodeops.h>
#include <util.h>
#include <codadir.h>
#include <res.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <recov_vollog.h>
#include <lockqueue.h>
#include <cvnode.h>
#include <olist.h>
#include <rpc2/errors.h>
#include <srv.h>
#include <vlist.h>
#include <operations.h>
#include <treeremove.h>
#include <vrdb.h>

#include "ops.h"
#include "rescomm.h"
#include "resutil.h"
#include "timing.h"

class RUParm {	
  public:
    dlist *vlist;		/* list of vnodes of all objects */
    olist *hvlog;		/* remote log by host and vnode */
    unsigned long srvrid;  	/* serverid where rm happened */
    unsigned long vid;		/* volume id */
    int	rcode;			/* return code: 0 -> no conflicts */
    
    RUParm(dlist *vl, olist *rmtlog, unsigned long id, unsigned long v) {
	vlist = vl;
	hvlog = rmtlog;
	srvrid = id;
	vid = v;
	rcode = 0;
    }
};

/* Yield parameters */
/* N.B.  Yield "periods" MUST all be power of two so that AND'ing can be used! */
const int Yield_GetResObjPeriod = 8;
const int Yield_GetResObjMask = (Yield_GetResObjPeriod - 1);
const int Yield_CollectFidPeriod = 256;
const int Yield_CollectFidMask =(Yield_CollectFidPeriod - 1);
const int Yield_CheckSemPerformResPeriod = 8;
const int Yield_CheckSemPerformRes_Mask =(Yield_CheckSemPerformResPeriod - 1);
const int Yield_GetP2ObjFids_Period = 256;
const int Yield_GetP2ObjFids_Mask = (Yield_GetP2ObjFids_Period - 1);
const int Yield_GetP2Obj_Period = 8;
const int Yield_GetP2Obj_Mask = (Yield_GetP2Obj_Period - 1);
const int Yield_CreateP2Obj_Period = 8;
const int Yield_CreateP2Obj_Mask = (Yield_CreateP2Obj_Period - 1);
extern void PollAndYield();

/* definitions of OPS returned by CheckResSemantics */
#define PERFORMOP	0
#define NULLOP		1
#define	MARKPARENTINC	2
#define	MARKOBJINC	3
#define CREATEINCOBJ	4

/* extern routine declarations */
extern void AllocIncBSEntry(RPC2_BoundedBS *, char *, ViceFid *, ViceFid *, long);
extern int GetSubTree(ViceFid *, Volume *, dlist *);
extern int GetParentFid(Volume *, ViceFid *, ViceFid *);

int DirRUConf(RUParm *, char *, long, long);
int EDirRUConf(PDirEntry, void *);

/* private routine declarations */
extern void UpdateVVs(ViceVersionVector *, ViceVersionVector *, ViceVersionVector *);

long RS_InstallVV(RPC2_Handle RPCid, ViceFid *Fid, ViceVersionVector *VV,
		     SE_Descriptor *sed)
{
    PROBE(tpinfo, CPHASE3BEGIN);
    Volume *volptr = NULL;
    dlist *vlist = new dlist((CFN) VLECmp);
    int ix = -1;
    long errorCode = 0;
    vle *ov = NULL;
    vrent *vre = NULL;
    void *buf = NULL;
    
    {
	if (!XlateVid(&Fid->Volume)) {
	    SLog(0,  
		   "InstallVV: Coudnt Xlate VSG %x", Fid->Volume);
	    PROBE(tpinfo, CPHASE3END);
	    return(EINVAL);
	}
    }
    /* get objects */
    {
	ov = AddVLE(*vlist, Fid);
	if ( (errorCode = GetFsObj(Fid, &volptr, &ov->vptr, WRITE_LOCK, NO_LOCK, 0, 0, ov->d_inodemod)) ) {
	    SLog(0,  
		   "InstallVV: Error %d getting object %x.%x",
		   errorCode, Fid->Vnode, Fid->Unique);
	    goto Exit;
	}
    }
    /* get index of host */
    {
	vre = VRDB.find(V_groupId(volptr));
	CODA_ASSERT(vre);
	ix = vre->index(ThisHostAddr);
	CODA_ASSERT(ix >= 0);
    }
    
    /* if phase1 was successful update vv */
    SLog(9,  "InstallVV: Going to update vv");
    
    if ((&(VV->Versions.Site0))[ix] && COP2Pending(ov->vptr->disk.versionvector)) {
        ov->vptr->disk.versionvector.StoreId = VV->StoreId;
	(&(VV->Versions.Site0))[ix] = 0;
	UpdateVVs(&V_versionvector(volptr), &Vnode_vv(ov->vptr), VV);
	ClearCOP2Pending(ov->vptr->disk.versionvector);	
	(&(VV->Versions.Site0))[ix] = 1;
	
	// add a log entry that will be common for all hosts participating in this res
	SLog(0, "Going to check if logentry must be spooled\n");
	if (V_RVMResOn(volptr)) {
	    SLog(0, 
		   "Going to spool log entry for phase3\n");
	    CODA_ASSERT(SpoolVMLogRecord(vlist, ov, volptr, &(VV->StoreId), ResolveNULL_OP, 0) == 0);
	}
    }
    /* truncate log if success everywhere in phase 1 */
    {
	SLog(9, "InstallVV: Going to check if truncate log possible");
	unsigned long Hosts[VSG_MEMBERS];
	int i = 0;
	vv_t checkvv;

	vre->GetHosts(Hosts);
	vre->HostListToVV(Hosts, &checkvv);
	for (i = 0; i < VSG_MEMBERS; i++) 
	    if (((&(checkvv.Versions.Site0))[i]) ^ 
		((&(VV->Versions.Site0))[i]))
		break;
	if (i == VSG_MEMBERS) {
	    /* update set has 1 for all hosts */
	    SLog(9, "InstallVV: Success everywhere - truncating log");
	    if (AllowResolution && V_RVMResOn(volptr)) 
		ov->d_needslogtrunc = 1;
	}
    }
    
    /* return contents of directory in a buffer for coordinator to compare */
    {
	SE_Descriptor sid;
	int size;

	SLog(9,  "RS_InstallVV: Shipping dir contents ");

	/* Get directory contents */
	buf = Dir_n_ACL(ov->vptr, &size);

	/* And ship it back to the coordinator */
	memset(&sid, 0, sizeof(SE_Descriptor));
	sid.Tag = SMARTFTP;
	sid.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
	sid.Value.SmartFTPD.SeekOffset = 0;
	sid.Value.SmartFTPD.hashmark = (SrvDebugLevel > 2 ? '#' : '\0');
	sid.Value.SmartFTPD.Tag = FILEINVM;
	sid.Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqLen = size;
	sid.Value.SmartFTPD.FileInfo.ByAddr.vmfile.MaxSeqLen = size;
	sid.Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqBody = (RPC2_ByteSeq)buf;
	
	if((errorCode = RPC2_InitSideEffect(RPCid, &sid)) <= RPC2_ELIMIT)
	{
		SLog(0, "RS_InstallVV:  InitSE failed (%d)", errorCode);
		goto Exit;
	}
	
	if ((errorCode = RPC2_CheckSideEffect(RPCid, &sid, SE_AWAITLOCALSTATUS)) <= RPC2_ELIMIT)
	{
		SLog(0, "RS_InstallVV: CheckSE failed (%d)", errorCode);
		if (errorCode == RPC2_SEFAIL1) errorCode = EIO;
		goto Exit;
	}
    }

  Exit:
    if (buf) free(buf);

    PutObjects((int)errorCode, volptr, NO_LOCK, vlist, 0, 1);
    SLog(9,  "InstallVV returns %d", errorCode);
    PROBE(tpinfo, CPHASE3END);

    return(errorCode);
}


void MarkObjInc(ViceFid *fid, Vnode *vptr) {
    VolumeId VSGVolnum = fid->Volume;
    if (ReverseXlateVid(&VSGVolnum))  {
	CodaBreakCallBack(0, fid, VSGVolnum);
    }
    else {
	BreakCallBack(0, fid);
    }
    
    SetIncon(vptr->disk.versionvector);
}

/* Create an Object which we are going to mark inconsistent
 * 	If the object doesnt exist already then create it first 
 */
int CreateObjToMarkInc(Volume *vp, ViceFid *dFid, ViceFid *cFid, 
		       char *name, int vntype, dlist *vlist, int *blocks) {
    
    vle *pv = 0;
    vle *cv = 0;
    *blocks = 0;
    long errorCode = 0;
    SLog(9,
	   "CreateIncObject: Entering (parent %x.%x child %x.%x name %s type %d)",
	   dFid->Vnode, dFid->Unique, cFid->Vnode, cFid->Unique, name, vntype);

    /* get vnode of parent */
    {
	pv = FindVLE(*vlist, dFid);
	if (!pv || !pv->vptr) {
	    SLog(0,  "CreateIncObject: Parent(%x.%x) doesnt exist!", 
		    dFid->Vnode, dFid->Unique);
	    return(EINVAL);
	}
    }
    
    {
	
	PDirHandle dh;
	ViceFid newfid;
	dh = VN_SetDirHandle(pv->vptr);
	if (DH_Lookup(dh, name, &newfid, CLU_CASE_SENSITIVE) == 0) {
	    // parent has name 
	    if ((newfid.Vnode == cFid->Vnode) && (newfid.Unique == cFid->Unique)){
		cv = FindVLE(*vlist, cFid);
		CODA_ASSERT(cv); 
		CODA_ASSERT(cv->vptr);
		CODA_ASSERT((cv->vptr->disk.linkCount > 0) && !cv->vptr->delete_me);
		SLog(9,  "CreateIncObj Child (%x.%x)already exists",
		       cFid->Vnode, cFid->Unique);
		VN_PutDirHandle(pv->vptr);
		return(0);
	    }
	    else {
		// parent has name but different object 
		SLog(0,  
		       "CreateIncObject: Parent %x.%x already has name %s",
		       dFid->Vnode, dFid->Unique, name);
		SLog(0,  
		       "              with fid %x.%x - cant create %x.%x",
		       newfid.Vnode, newfid.Unique, cFid->Vnode, cFid->Unique);
		VN_PutDirHandle(pv->vptr);
		return(EINVAL);
	    }
	}

	/* We are about to modify the parent vnode. Keep the refcount, so
	 * that PutObjects can update the on-disk data. --JH */
	// VN_PutDirHandle(pv->vptr);
	pv->d_inodemod = 1;

	// name doesnt exist - look for object 
	{
	    cv = FindVLE(*vlist, cFid);
	    if (cv && cv->vptr) {
		// object exists 
		if ((cv->vptr->disk.vparent != dFid->Vnode) || 
		    (cv->vptr->disk.uparent != dFid->Unique)) {
		    SLog(0,  
			   "CreateIncObject: Object %x.%x already exists in %x.%x !",
			    cFid->Vnode, cFid->Unique, cv->vptr->disk.vparent, 
			    cv->vptr->disk.uparent);
		    return(EINVAL);
		}

		// object exists in same diretory with different name
		if (vntype != vFile) {
		    // cannot create link to object 
		    SLog(0, 
			   "CreateIncObject: Object %x.%x already exists in %x.%x !",
			    cFid->Vnode, cFid->Unique, dFid->Vnode, dFid->Unique);
		    return(EINVAL);
		}

		// have to create a link 
		CODA_ASSERT(vntype == vFile);
		VolumeId VSGVolnum = V_id(vp);
		if (!ReverseXlateVid(&VSGVolnum)) {
		    SLog(0,  "CreateIncObject: Couldnt RevXlateVid %x",
			    VSGVolnum);
		    return(EINVAL);
		}
		if ((errorCode = 
		    CheckLinkSemantics(NULL, &pv->vptr, &cv->vptr, name, &vp, 
				       0, NULL, NULL, NULL, NULL, NULL, 0))) {
		    SLog(0,  "CreateObjToMarkInc: Error 0x%x to create link %s",
			    errorCode, name);
		    return(errorCode);
		}
		ViceStoreId stid;
		AllocStoreId(&stid);

		PerformLink(NULL, VSGVolnum, vp, pv->vptr, cv->vptr, 
			    name, time(0), 0, &stid, &pv->d_cinode, blocks);
		if (cv->vptr->delete_me) {
		    /* it was deleted before the link was done */
		    SLog(0,  "Undeleting Vnode %s (%x.%x)",
			    name, cFid->Vnode, cFid->Unique);
		    CODA_ASSERT(cv->vptr->disk.linkCount);
		    cv->vptr->delete_me = 0;
/*
		    cv->vptr->disk.dataVersion = 1;
		    cv->f_finode = icreate(V_device(vp), 0, V_id(vp), 
					   cv->vptr->vnodeNumber, 
					   cv->vptr->disk.uniquifier, 
					   cv->vptr->disk.dataVersion);
		    CODA_ASSERT(cv->f_finode > 0);
		    cv->vptr->disk.inodeNumber = cv->f_finode;
*/
		    CODA_ASSERT(cv->f_sinode > 0);
		    cv->vptr->disk.inodeNumber = cv->f_sinode;
		    cv->f_sinode = 0;
		    if (AllowResolution && V_RVMResOn(vp)) {
			// spool log record for recoverable res logs 
			if ((errorCode = SpoolVMLogRecord(vlist, pv, vp, &stid,
                                                          ResolveViceCreate_OP, 
                                                          name, cFid->Vnode, cFid->Unique)))
			    SLog(0, 
				   "CreateObjToMarkInc: Error %d during SpoolVMLogRecord\n",
				   errorCode);
		    }
		}
		return(errorCode);
	    }
	}
	/* object is missing too - create the object */
	{
	    Vnode *cvptr = 0;
	    int tblocks = 0;
	    long errorCode = 0;
	    if ((errorCode = AllocVnode(&cvptr, vp, (ViceDataType)vntype,
				       cFid, dFid, pv->vptr->disk.owner,
				       1, &tblocks))) {
		SLog(0,  "CreateIncObj: Error %d in AllocVnode",
			errorCode);
		return(errorCode);
	    }
	    cv = AddVLE(*vlist, cFid);
	    cv->vptr = cvptr;
	    if ( cvptr->disk.type == vDirectory )
		cv->d_inodemod = 1;
	    *blocks += tblocks;

	    /* create name in parent */
	    {
		VolumeId VSGVolnum = V_id(vp);
		if (!ReverseXlateVid(&VSGVolnum)) {
		    SLog(0,  "CreateIncObject: Couldnt RevXlateVid %x",
			    VSGVolnum);
		    return(EINVAL);
		}
		ViceStoreId stid;
		AllocStoreId(&stid);
		int tblocks = 0;
		if (cv->vptr->disk.type == vFile) {
		    if ((errorCode = CheckCreateSemantics(NULL, &pv->vptr, 
							 &cv->vptr, name, &vp, 
							 0, NULL, NULL, NULL, 
							 NULL, NULL, 0))) {
			SLog(0,  "Error %d in CheckCreateSem(%x.%x %s)", 
				errorCode, cFid->Vnode, cFid->Unique, name);
			return(errorCode);
		    }
		    PerformCreate(NULL, VSGVolnum, vp, pv->vptr,
				  cv->vptr, name, 
				  pv->vptr->disk.unixModifyTime,
				  pv->vptr->disk.modeBits,
				  0, &stid, &pv->d_cinode, &tblocks);
		    *blocks += tblocks;
		    cv->vptr->disk.dataVersion = 1;
		    cv->f_finode = icreate((int) V_device(vp), 0, (int) V_id(vp),
					   (int) cv->vptr->vnodeNumber,
					   (int) cv->vptr->disk.uniquifier,
					   (int) cv->vptr->disk.dataVersion);
		    CODA_ASSERT(cv->f_finode > 0);
		    cv->vptr->disk.inodeNumber = cv->f_finode;
		    
		    // spool log record 
		    if (AllowResolution && V_RVMResOn(vp)) {
			if ((errorCode = SpoolVMLogRecord(vlist, pv, vp, &stid,
                                                          ResolveViceCreate_OP, 
                                                          name, cFid->Vnode, cFid->Unique)))
			    SLog(0, 
				   "CreateObjToMarkInc: Error %d during SpoolVMLogRecord\n",
				   errorCode);

		    }
		}
		else if (cv->vptr->disk.type == vSymlink) {
		    if ((errorCode = CheckSymlinkSemantics(NULL, &pv->vptr, 
							  &cv->vptr, name, &vp,
							  0, NULL, NULL, NULL,
							  NULL, NULL, 0))) {
			SLog(0,  "Error %d in CheckSymlinkSem(%x.%x %s)",
				errorCode, cFid->Vnode, cFid->Unique, name);
			return(errorCode);
		    }
		    int tblocks = 0;
		    PerformSymlink(NULL, VSGVolnum, vp, pv->vptr,
				   cv->vptr, name, 0, 0, 
				   pv->vptr->disk.unixModifyTime,
				   pv->vptr->disk.modeBits,
				   0, &stid, &pv->d_cinode, &tblocks);
		    *blocks += tblocks;
		    cv->vptr->disk.dataVersion = 1;
		    cv->f_finode = icreate((int) V_device(vp), 0, (int) V_id(vp),
					   (int) cv->vptr->vnodeNumber,
					   (int) cv->vptr->disk.uniquifier,
					   (int) cv->vptr->disk.dataVersion);
		    CODA_ASSERT(cv->f_finode > 0);
		    cv->vptr->disk.inodeNumber = cv->f_finode;
		    
		    if (AllowResolution && V_RVMResOn(vp)) {
			if ((errorCode = SpoolVMLogRecord(vlist, pv, vp, &stid, 
							 ResolveViceSymLink_OP, 
							 name, cFid->Vnode, cFid->Unique))) 
			    SLog(0, 
				   "CreateObjToMarkInc(symlink): Error %d in SpoolVMLogRecord\n",
				   errorCode);
		    }
		}
		else {
		    CODA_ASSERT(cv->vptr->disk.type == vDirectory);
		    if ((errorCode = CheckMkdirSemantics(NULL, &pv->vptr, 
							&cv->vptr, name, &vp,
							0, NULL, NULL, NULL, 
							NULL, NULL, 0))) {
			SLog(0,  "Error %d in CheckMkdirSem(%x.%x %s)",
				errorCode, cFid->Vnode, cFid->Unique, name);
			return(errorCode);
		    }
		    cv->d_inodemod = 1;
		    int tblocks = 0;
		    PerformMkdir(NULL, VSGVolnum, vp, pv->vptr,
				 cv->vptr, name,
				 pv->vptr->disk.unixModifyTime,
				 pv->vptr->disk.modeBits,
				 0, &stid, &pv->d_cinode, &tblocks);
		    *blocks += tblocks;
		    if (AllowResolution && V_RVMResOn(vp)) {
			if ((errorCode = SpoolVMLogRecord(vlist, pv, vp, &stid,
                                                         ResolveViceMakeDir_OP,
                                                         name, cFid->Vnode, cFid->Unique))) 
			    SLog(0, 
				   "CreateObjToMarkInc(Mkdir): Error %d during SpoolVMLogRecord for parent\n",
				   errorCode);
		    }
		}
	    }
	}
	return(errorCode);
    }
}


int GetPhase2Objects(ViceFid *pfid, dlist *vlist, dlist *inclist,
			     Volume **volptr) 
{
    long errorCode = 0;
    
    /* get volume */
    if ((errorCode = GetVolObj(pfid->Volume, volptr, 
			      VOL_NO_LOCK, 0, 0))) {
	SLog(0,  "GetPhase2Objects: Error %d in Getting volume (Parent)",
		errorCode);
	return(errorCode);
    }
    /* need vnode of parent */
    {
	if (ObjectExists(V_volumeindex(*volptr), vLarge, 
			 vnodeIdToBitNumber(pfid->Vnode), pfid->Unique, NULL))
	    AddVLE(*vlist, pfid);
	else {
	    SLog(0,  "GetPhase2Objects: Parent (%x.%x) is missing",
		    pfid->Vnode, pfid->Unique);
	    return(EINVAL);
	}
    }

    /* add existing vnode fids to vlist */
    {
	dlist_iterator next(*inclist);
	ilink *il;
	int count = 0;
	while((il = (ilink *)next())) {
	    count++;
	    if ((count & Yield_GetP2ObjFids_Mask) == 0) 
		PollAndYield();
	    ViceFid cfid;
	    cfid.Volume = pfid->Volume;
	    cfid.Vnode = il->vnode;
	    cfid.Unique = il->unique;
	    ViceFid vpfid;
	    vpfid.Volume = pfid->Volume;

	    if (ObjectExists(V_volumeindex(*volptr), 
			     ISDIR(cfid) ? vLarge : vSmall,
			     vnodeIdToBitNumber(cfid.Vnode),
			     cfid.Unique, &vpfid)) {
		AddVLE(*vlist, &cfid);
		ViceFid ipfid;
		ipfid.Volume = pfid->Volume;
		ipfid.Vnode = il->pvnode;
		ipfid.Unique = il->punique;
		
		if (vpfid.Vnode != pfid->Vnode || 
		    vpfid.Unique != pfid->Unique)
		    AddVLE(*vlist, &vpfid);
		if (vpfid.Vnode != ipfid.Vnode || 
		    vpfid.Unique != ipfid.Unique)
		    if ((ipfid.Vnode != pfid->Vnode ||
			 ipfid.Unique != pfid->Unique) &&
			ObjectExists(V_volumeindex(*volptr),
				     ISDIR(ipfid) ? vLarge : vSmall,
				     vnodeIdToBitNumber(ipfid.Vnode),
				     ipfid.Unique, NULL))
			AddVLE(*vlist, &ipfid);
	    }
	}
    }

    /* get existing objects in fid order */
    {	
	int count = 0;
	if (vlist->count() > 0) {
	    dlist_iterator next(*vlist);
	    vle *v;
	    while ((v = (vle *)next())) {
		count++;
		if ((count & Yield_GetP2Obj_Mask) == 0)
		    PollAndYield();
		SLog(9,  "GetPhase2Objects: acquiring (%x.%x.%x)",
			v->fid.Volume, v->fid.Vnode, v->fid.Unique);
		if ((errorCode = GetFsObj(&v->fid, volptr, &v->vptr, 
					 WRITE_LOCK, NO_LOCK, 1, 0, 0))) {
		    SLog(0,  "GetPhase2Objects: Error %d in getting object %x.%x",
			    errorCode, v->fid.Vnode, v->fid.Unique);
		    return(errorCode);
		}
	    }
	}
    }
    SLog(9,  "GetPhase2Objects: returns %d");
    return(0);
}

/* CreateResPhase2Objects:
 *	Create vnodes for objects to be marked inconsistent
 *	If vnode exists but name doesnt, create it in the parent 
 */
int CreateResPhase2Objects(ViceFid *pfid, dlist *vlist, dlist *inclist,
				   Volume *volptr, VolumeId VSGVolnum,
				   int *blocks) {
    long errorCode = 0;
    *blocks =  0;
    
    vle *pv = FindVLE(*vlist, pfid);
    CODA_ASSERT(pv);
    CODA_ASSERT(pv->vptr);

    ilink *il = 0;
    dlist_iterator next(*inclist);
    int count = 0;
    while ((il = (ilink *)next())) {
	int tmperrorCode;
	ViceFid ofid;
	ofid.Volume = pfid->Volume;
	ofid.Vnode = il->vnode;
	ofid.Unique = il->unique;

	ViceFid ipfid;
	ipfid.Volume = pfid->Volume;
	ipfid.Vnode = il->pvnode;
	ipfid.Unique = il->punique;
	
	int tblocks = 0;
	count++;
	if ((count & Yield_CreateP2Obj_Mask) == 0)
	    PollAndYield();
	if ((tmperrorCode = (int)CreateObjToMarkInc(volptr, &ipfid, &ofid, il->name, 
					      il->type, vlist, &tblocks))) {
	    SLog(0,  "CreateResPhase2Obj: Error %d in creating %x.%x %s in %x.%x",
		    errorCode, ofid.Vnode, ofid.Unique, il->name, ipfid.Vnode, 
		    ipfid.Unique);
	    if (!errorCode) errorCode = tmperrorCode;
	}
	else 
	    *blocks += tblocks;
    }
    SLog(9,  "CreateResPhase2Objects: returns %d", errorCode);
    return(errorCode);
}



/* warning: name must have size MAXNAMELEN or larger */
/* Find the name of a vnode in its parent directory:
   return 0 upon finding the name
   return 0 upon failure to find vnodes
   return 0 upon failure to find name in parent
   return 1 upon success
*/

int GetNameInParent(Vnode *vptr, dlist *vlist, Volume *volptr, char *name) 
{
	long errorCode = 0;
	PDirHandle dh;
	int rc;
	ViceFid pFid;
	ViceFid Fid;
	
	VN_VN2PFid(vptr, volptr, &pFid);
	VN_VN2Fid(vptr, volptr, &Fid);

	if (pFid.Unique == 0 || pFid.Vnode == 0) {
		SLog(0,"GetNameInParent %s: Parent had 0 in id; returning 0", 
		     FID_(&pFid));
		return 0;
	}
	vle *pv = FindVLE(*vlist, &pFid);
	if (!pv) {
		pv = AddVLE(*vlist, &pFid);
		/* THE LOCK NEED NOT BE WRITE_LOCK - PARANOIA FOR OTHER OPS
		   TOUCHING THIS VNODE */
		errorCode = GetFsObj(&pFid, &volptr, &pv->vptr, WRITE_LOCK, 
				     NO_LOCK, 1, 0, 0);
		if (errorCode) {
			SLog(0, "GetNameInParent for %s: Error %d getting parent %s",
			     FID_(&pFid), errorCode, FID_2(&Fid));
			if (!pv->vptr) 
				vlist->remove(pv);
			return 0;
		}
	}
	
	dh = VN_SetDirHandle(pv->vptr);
	rc = DH_LookupByFid(dh, name, &Fid);
	VN_PutDirHandle(pv->vptr);
	return ! rc;
}

