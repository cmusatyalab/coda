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

/* this file contains local-repair related fsobj methods */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus
#include <stdio.h>
#include <struct.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <codadir.h>
#include <fcntl.h>

/* interfaces */
#include <vcrcommon.h>
#include <vice.h>

#ifdef __cplusplus
}
#endif __cplusplus



/* from venus */
#include "fso.h"
#include "local.h"
#include "venuscb.h"
#include "venusrecov.h"
#include "venus.private.h"
#include "venusvol.h"
#include "worker.h"


/* ********** Mist Routines ********** */
int MakeDirList(struct DirEntry *de, void *hook)
{
	VnodeId vnode;
	Unique_t unique;
	FID_NFid2Int(&de->fid, &vnode, &unique);
	char *name = de->name;
	LOG(100, ("MakeDirList: Fid = 0x%x.%x.%x and Name = %s\n",
		  (int)hook, vnode, unique, name));
	LRDB->DirList_Insert((long )hook, vnode, unique, name);
	return 0;
}

int fsobj::RepairStore() 
{	
    /* same as ConnectedStore without perform simulation disconnection */
    FSO_ASSERT(this, HOARDING(this) || LOGGING(this));

    vproc *vp = VprocSelf();
    vuid_t vuid = CRTORUID(vp->u.u_cred);
    Date_t Mtime = Vtime();
    unsigned long NewLength = stat.Length;

    if (LRDB->repair_session_mode == REP_SCRATCH_MODE) {
	return DisconnectedStore(Mtime, vuid, NewLength, LRDB->repair_session_tid);
    }
    int code = 0;
    char prel_str[256];
    sprintf(prel_str, "store::Store %%s [%ld]\n", NBLOCKS(NewLength));

    /* Status parameters. */
    ViceStatus status;
    VenusToViceStatus(&stat, &status);
    {
	/* Temporary!  Until RPC interface is fixed!  -JJK */
	status.Date = Mtime;
    }

    /* COP2 Piggybacking. */
    char PiggyData[COP2SIZE];
    RPC2_CountedBS PiggyBS;
    PiggyBS.SeqLen = 0;
    PiggyBS.SeqBody = (RPC2_ByteSeq)PiggyData;

    /* Set up the SE descriptor. */
    SE_Descriptor dummysed;
    memset(&dummysed, 0, sizeof(SE_Descriptor));
    SE_Descriptor *sed = 0;
    {
	/* Must be a file! */
	sed = &dummysed;
	sed->Tag = SMARTFTP;
	struct SFTP_Descriptor *sei = &sed->Value.SmartFTPD;
	sei->TransmissionDirection = CLIENTTOSERVER;
	sei->hashmark = 0;
	sei->SeekOffset = 0;
	sei->ByteQuota = -1;

        /* and open a safe fd to the containerfile */
        int fd = data.file->Open(this, O_RDONLY);
        CODA_ASSERT(fd != -1);

        sei->Tag = FILEBYFD;
        sei->FileInfo.ByFD.fd = fd;
    }

    /* VCB Arguments */
    RPC2_Integer VS = 0;
    CallBackStatus VCBStatus = NoCallBack;
    RPC2_CountedBS OldVS; 
    OldVS.SeqLen = 0;
    OldVS.SeqBody = 0;

    if (flags.replicated) {
	ViceStoreId sid;
	mgrpent *m = 0;
	int asy_resolve = 0;

	/* Acquire an Mgroup. */
	code = vol->GetMgrp(&m, vuid, (PIGGYCOP2 ? &PiggyBS : 0));
	if (code != 0) goto RepExit;

	/* The COP1 call. */
	long cbtemp; cbtemp = cbbreaks;
	vv_t UpdateSet;

	Recov_BeginTrans();
	sid = vol->GenerateStoreId();
	Recov_EndTrans(MAXFP);
	{
	    /* Make multiple copies of the IN/OUT and OUT parameters. */
	    int ph_ix; unsigned long ph; ph = m->GetPrimaryHost(&ph_ix);
 	    vol->PackVS(m->nhosts, &OldVS);

	    ARG_MARSHALL(IN_OUT_MODE, ViceStatus, statusvar, status, VSG_MEMBERS);
	    ARG_MARSHALL(IN_OUT_MODE, SE_Descriptor, sedvar, *sed, VSG_MEMBERS);
	    ARG_MARSHALL(OUT_MODE, RPC2_Integer, VSvar, VS, VSG_MEMBERS);
	    ARG_MARSHALL(OUT_MODE, CallBackStatus, VCBStatusvar, VCBStatus, VSG_MEMBERS)

	    /* Make the RPC call. */
	    CFSOP_PRELUDE(prel_str, comp, fid);
	    MULTI_START_MESSAGE(ViceStore_OP);
	    code = (int)MRPC_MakeMulti(ViceStore_OP, ViceStore_PTR,
				       VSG_MEMBERS, m->rocc.handles,
				       m->rocc.retcodes, m->rocc.MIp, 0, 0,
				       &fid, statusvar_ptrs, NewLength, ph,
				       &sid, &OldVS, VSvar_ptrs,
				       VCBStatusvar_ptrs, &PiggyBS,
				       sedvar_bufs);
	    MULTI_END_MESSAGE(ViceStore_OP);
	    CFSOP_POSTLUDE("store::store done\n");

	    free(OldVS.SeqBody);
	    /* Collate responses from individual servers and decide what to do next. */
	    code = vol->Collate_COP1(m, code, &UpdateSet);
	    MULTI_RECORD_STATS(ViceStore_OP);
	    if (code == EASYRESOLVE) { asy_resolve = 1; code = 0; }
	    if (code != 0) goto RepExit;

	    /* Collate volume callback information */
	    if (cbtemp == cbbreaks)
		vol->CollateVCB(m, VSvar_bufs, VCBStatusvar_bufs);

	    /* Finalize COP2 Piggybacking. */
	    if (PIGGYCOP2)
		vol->ClearCOP2(&PiggyBS);

	    /* Manually compute the OUT parameters from the mgrpent::Store() call! -JJK */
	    int dh_ix; dh_ix = -1;
	    (void)m->DHCheck(0, ph_ix, &dh_ix);
	    ARG_UNMARSHALL(statusvar, status, dh_ix);
	    {
		long bytes = sedvar_bufs[dh_ix].Value.SmartFTPD.BytesTransferred;
		LOG(10, ("(Multi)ViceStore: stored %d bytes\n", bytes));
		if (bytes != (long)status.Length) {
		    print(logFile);
		    CHOKE("fsobj::Store: bytes mismatch (%d, %d)",
			bytes, status.Length);
		}
	    }
	}

	/* Do Store locally. */
	Recov_BeginTrans();
	LocalStore(Mtime, NewLength);
	UpdateStatus(&status, &UpdateSet, vuid);
	Recov_EndTrans(CMFP);
	if (ASYNCCOP2) ReturnEarly();

	/* Send the COP2 message or add an entry for piggybacking. */
	if (PIGGYCOP2)
	    vol->AddCOP2(&sid, &UpdateSet);
	else
	    (void)vol->COP2(m, &sid, &UpdateSet);

RepExit:
	PutMgrp(&m);
	switch(code) {
	    case 0:
		if (asy_resolve)
		    vol->ResSubmit(0, &fid);
		break;

	    default:
		break;
	}
    }
    else {
	/* Acquire a Connection. */
	connent *c;
	ViceStoreId Dummy;              /* ViceStore needs an address for indirection */
	code = vol->GetConn(&c, vuid);
	if (code != 0) goto NonRepExit;

	/* Make the RPC call. */
	CFSOP_PRELUDE(prel_str, comp, fid);
	UNI_START_MESSAGE(ViceStore_OP);
	code = (int) ViceStore(c->connid, &fid, &status, NewLength, 0,
				   &Dummy, &OldVS, &VS, &VCBStatus, &PiggyBS,
				   sed);
	UNI_END_MESSAGE(ViceStore_OP);
	CFSOP_POSTLUDE("store::store done\n");

	/* Examine the return code to decide what to do next. */
	code = vol->Collate(c, code);
	UNI_RECORD_STATS(ViceStore_OP);
	if (code != 0) goto NonRepExit;

	{
	    long bytes = sed->Value.SmartFTPD.BytesTransferred;
	    LOG(10, ("ViceStore: stored %d bytes\n", bytes));
	    if (bytes != (long)status.Length) {
		print(logFile);
		CHOKE("fsobj::Store: bytes mismatch (%d, %d)",
		      bytes, status.Length);
	    }
	}

	/* Do Store locally. */
	Recov_BeginTrans();
	LocalStore(Mtime, NewLength);
	UpdateStatus(&status, 0, vuid);
	Recov_EndTrans(CMFP);

NonRepExit:
	PutConn(&c);
    }
    
    data.file->Close();

    return(code);
}

int fsobj::RepairSetAttr(unsigned long NewLength, Date_t NewDate,
			 vuid_t NewOwner, unsigned short NewMode,
			 RPC2_CountedBS *acl) 
{
    Date_t Mtime = Vtime();
    vproc *vp = VprocSelf();
    vuid_t vuid = CRTORUID(vp->u.u_cred);
    if (LRDB->repair_session_mode == REP_SCRATCH_MODE)
      return DisconnectedSetAttr(Mtime, vuid, NewLength, NewDate, NewOwner, NewMode, 
				 LRDB->repair_session_tid);
    else
      return ConnectedSetAttr(Mtime, vuid, NewLength, NewDate, NewOwner, NewMode, acl);
}

int fsobj::RepairCreate(fsobj **t_fso_addr, char *name, unsigned short Mode, int target_pri) 
{
    Date_t Mtime = Vtime();
    vproc *vp = VprocSelf();
    vuid_t vuid = CRTORUID(vp->u.u_cred);
    if (LRDB->repair_session_mode == REP_SCRATCH_MODE)
      return DisconnectedCreate(Mtime, vuid, t_fso_addr, name, Mode, target_pri,
				LRDB->repair_session_tid);
    else
      return ConnectedCreate(Mtime, vuid, t_fso_addr, name, Mode, target_pri);
}

int fsobj::RepairRemove(char *name, fsobj *target_fso) {
    Date_t Mtime = Vtime();
    vproc *vp = VprocSelf();
    vuid_t vuid = CRTORUID(vp->u.u_cred);
    if (LRDB->repair_session_mode == REP_SCRATCH_MODE)
      return DisconnectedRemove(Mtime, vuid, name, target_fso, LRDB->repair_session_tid);
    else
      return ConnectedRemove(Mtime, vuid, name, target_fso);
}

int fsobj::RepairLink(char *name, fsobj *source_fso) {
    Date_t Mtime = Vtime();
    vproc *vp = VprocSelf();
    vuid_t vuid = CRTORUID(vp->u.u_cred);
    if (LRDB->repair_session_mode == REP_SCRATCH_MODE)
      return DisconnectedLink(Mtime, vuid, name, source_fso, LRDB->repair_session_tid);
    else
      return ConnectedLink(Mtime, vuid, name, source_fso);
}

int fsobj::RepairRename(fsobj *s_parent_fso, char *s_name, fsobj *s_fso, char *t_name, fsobj *t_fso) 
{
    Date_t Mtime = Vtime();
    vproc *vp = VprocSelf();
    vuid_t vuid = CRTORUID(vp->u.u_cred);
    if (LRDB->repair_session_mode == REP_SCRATCH_MODE)
      return DisconnectedRename(Mtime, vuid, s_parent_fso, s_name, s_fso, t_name, t_fso,
				LRDB->repair_session_tid);
    else
      return ConnectedRename(Mtime, vuid, s_parent_fso, s_name, s_fso, t_name, t_fso);
}


int fsobj::RepairMkdir(fsobj **t_fso_addr, char *name, unsigned short Mode, int target_pri) 
{
    Date_t Mtime = Vtime();
    vproc *vp = VprocSelf();
    vuid_t vuid = CRTORUID(vp->u.u_cred);
    if (LRDB->repair_session_mode == REP_SCRATCH_MODE)
      return DisconnectedMkdir(Mtime, vuid, t_fso_addr, name, Mode, target_pri, 
			       LRDB->repair_session_tid);
    else
      return ConnectedMkdir(Mtime, vuid, t_fso_addr, name, Mode, target_pri);
}

int fsobj::RepairRmdir(char *name, fsobj *target_fso)
{
    Date_t Mtime = Vtime();
    vproc *vp = VprocSelf();
    vuid_t vuid = CRTORUID(vp->u.u_cred);
    if (LRDB->repair_session_mode == REP_SCRATCH_MODE)
      return DisconnectedRmdir(Mtime, vuid, name, target_fso, LRDB->repair_session_tid);
    else
      return ConnectedRmdir(Mtime, vuid, name, target_fso);
}

int fsobj::RepairSymlink(fsobj **t_fso_addr, char *name, char *contents,
			    unsigned short Mode, int target_pri) 
{
    Date_t Mtime = Vtime();
    vproc *vp = VprocSelf();
    vuid_t vuid = CRTORUID(vp->u.u_cred);
    if (LRDB->repair_session_mode == REP_SCRATCH_MODE)
      return DisconnectedSymlink(Mtime, vuid, t_fso_addr, name, contents, Mode, target_pri,
				 LRDB->repair_session_tid);
    else
      return ConnectedSymlink(Mtime, vuid, t_fso_addr, name, contents, Mode, target_pri);
}

/*
  BEGIN_HTML
  <a name="discard"><strong> discard the fake-joint of a local-global
  conflict representation </strong></a> 
  END_HTML
*/
/* must not be call from within a transaction */
void fsobj::DeLocalRootParent(fsobj *RepairRoot, ViceFid *GlobalRootFid, fsobj *MtPt)
{
    FSO_ASSERT(this, RepairRoot != NULL && GlobalRootFid != NULL);
    LOG(100, ("fsobj::DeLocalRootParent: root-parent = 0x%x.%x.%x and repair-root = 0x%x.%x.%x\n",
	      fid.Volume, fid.Vnode, fid.Unique, RepairRoot->fid.Volume, 
	      RepairRoot->fid.Vnode, RepairRoot->fid.Unique));
    LOG(100, ("fsobj::DeLocalRootParent: GlobalRootFid = 0x%x.%x.%x MtPt = %x\n", 
	      GlobalRootFid->Volume, GlobalRootFid->Vnode, GlobalRootFid->Unique, MtPt));

    /* step 1: find out if the root parent root has other fake subtree as child */
    int shared_parent_count = 0;
    {	/* check if RootParentObj(this) is a shared root-parent-obj with another subtree */
	rfm_iterator next(LRDB->root_fid_map);
	rfment *rfm;
	while ((rfm = next())) {
	    if (rfm->RootCovered()) continue;
	    if (!bcmp((const void *)rfm->GetRootParentFid(), (const void *)&fid, 
		      (int)sizeof(ViceFid)))
	      shared_parent_count++;
	}
    }

    /* step 2: de-link root-parent and repair-root */
    if (MtPt == NULL) {
	Recov_BeginTrans();
	RVMLIB_REC_OBJECT(*this);
	RVMLIB_REC_OBJECT(*RepairRoot);
	dir_Delete(RepairRoot->comp);
	dir_Create(RepairRoot->comp, GlobalRootFid);
	if (shared_parent_count == 0) {
		flags.local = 0;
	}
	DetachChild(RepairRoot);
	RepairRoot->pfso = NULL;
	RepairRoot->pfid = NullFid;    
	Recov_EndTrans(MAXFP);
    } else {
	LOG(100, ("fsobj::DeLocalRootParent: Volume Root\n"));
	Recov_BeginTrans();
	RVMLIB_REC_OBJECT(*this);
	RVMLIB_REC_OBJECT(*RepairRoot);
	dir_Delete(RepairRoot->comp);
	dir_Create(RepairRoot->comp, &MtPt->fid);
	if (shared_parent_count == 0) {
		flags.local = 0;
	}
	DetachChild(RepairRoot);
	RepairRoot->pfso = NULL;
	RepairRoot->pfid = NullFid;    
	Recov_EndTrans(MAXFP);
    }

    /* step 3: re-link root-parent and global-root(if possible) */
    if (MtPt != NULL) {
	/* re-establish the child-parent relation between "this" and MtPt */
	Recov_BeginTrans();
	MtPt->pfso = this;
	MtPt->pfid = this->fid;
	this->AttachChild(MtPt);
	Recov_EndTrans(MAXFP);
    } else {
	/* try to get global-root cached as much as possible */
	fsobj *GlobalRootObj = NULL;
	/* first try to FSDB::Get GlobalRootObj(include possible fetching) */
	vproc *vp = VprocSelf();
	if (FSDB->Get(&GlobalRootObj, GlobalRootFid, CRTORUID(vp->u.u_cred), RC_STATUS) != 0) {
	    LOG(0, ("fsobj::DeLocalRootParent: FSDB::Get can't get GlobalRootObj\n"));
	} else {
	    /* FSDB::Get puts read-locked target, must unlock it.*/
	    GlobalRootObj->UnLock(RD);
	}
	/* no matter what happened to FSDB::Get(), search it from hash-table */
	GlobalRootObj = FSDB->Find(GlobalRootFid);
	if (GlobalRootObj) {
	    if (!bcmp((const void *)&fid, (const void *)&(GlobalRootObj->pfid), 
		      (int)sizeof(ViceFid)) &&
		GlobalRootObj->pfso == this) {
		/* 
		 * this is the case where the side-effect of FSDB::Get() in
		 * fetching the GlobalObject calls SetParent() which already
		 * established the child-parent relation between this and GlobalRootObj.
		 */
		LOG(0, ("fsobj::DeLocalRootParent:GlobalRoot already hooked\n"));
	    } else {
		/* re-establish child-parent relation between "this" and GlobalRootObj */
		Recov_BeginTrans();
		GlobalRootObj->pfso = this;
		GlobalRootObj->pfid = this->fid;
		this->AttachChild(GlobalRootObj);
		Recov_EndTrans(MAXFP);
	    }
	}
    }
}

/* must not be called from within a transaction */
void fsobj::MixedToGlobal(ViceFid *FakeRootFid, ViceFid *GlobalChildFid, char *Name)
{
    FSO_ASSERT(this, FakeRootFid != NULL && GlobalChildFid != NULL && Name != NULL);
    LOG(100, ("fsobj::MixedToGlobal: FakeRootFid = 0x%x.%x.%x GlobalChildFid = 0x%x.%x.%x\n",
	      FakeRootFid->Volume, FakeRootFid->Vnode, FakeRootFid->Unique,
	      GlobalChildFid->Volume, GlobalChildFid->Vnode, GlobalChildFid->Unique));
    LOG(100, ("fsobj::MixedToGlobal: RootParentFid = 0x%x.%x.%x Name = %s\n",
	      fid.Volume, fid.Vnode, fid.Unique, Name));
    /* "this" object is the RootParentObj in the LRDB->RFM map */
    fsobj *FakeRootObj = FSDB->Find(FakeRootFid);
    fsobj *GlobalChildObj = FSDB->Find(GlobalChildFid);
    FSO_ASSERT(this, FakeRootObj != NULL && GlobalChildObj != NULL);
    Recov_BeginTrans();
    RVMLIB_REC_OBJECT(*this);
    RVMLIB_REC_OBJECT(*FakeRootObj);
    RVMLIB_REC_OBJECT(*GlobalChildObj);
    this->dir_Delete(Name);			/* replace fid-name binding in RootParentObj's dir-pages */
    this->dir_Create(Name, GlobalChildFid);
    this->DetachChild(FakeRootObj);
    FakeRootObj->pfid = NullFid;
    FakeRootObj->pfso = NULL;
    FakeRootObj->DetachChild(GlobalChildObj);	/* preserve the fid-name binding in FakeRootObjs's dir-pages */
    this->AttachChild(GlobalChildObj);
    GlobalChildObj->pfid = this->fid;
    GlobalChildObj->pfso = this;	   
    Recov_EndTrans(MAXFP);
}

/* must not be called from within a transaction */
void fsobj::GlobalToMixed(ViceFid *FakeRootFid, ViceFid *GlobalChildFid, char *Name)
{
    FSO_ASSERT(this, FakeRootFid != NULL && GlobalChildFid != NULL && Name != NULL);
    LOG(100, ("fsobj::GlobalToMixed: FakeRootFid = 0x%x.%x.%x GlobalChildFid = 0x%x.%x.%x\n",
	      FakeRootFid->Volume, FakeRootFid->Vnode, FakeRootFid->Unique,
	      GlobalChildFid->Volume, GlobalChildFid->Vnode, GlobalChildFid->Unique));
    LOG(100, ("fsobj::GlobalToMixed: RootParentFid = 0x%x.%x.%x Name = %s\n",
	      fid.Volume, fid.Vnode, fid.Unique, Name));    
    /* "this" object is the RootParentObj in the LRDB->RFM map */
    fsobj *FakeRootObj = FSDB->Find(FakeRootFid);
    fsobj *GlobalChildObj = FSDB->Find(GlobalChildFid);
    FSO_ASSERT(this, FakeRootObj != NULL && GlobalChildObj != NULL);
    Recov_BeginTrans();
    RVMLIB_REC_OBJECT(*this);
    RVMLIB_REC_OBJECT(*FakeRootObj);
    RVMLIB_REC_OBJECT(*GlobalChildObj);
    this->dir_Delete(Name); /* replace fid-name binding in RootParentObj's dir-pages */
    this->dir_Create(Name, FakeRootFid);
    this->DetachChild(GlobalChildObj);
    this->AttachChild(FakeRootObj);
    FakeRootObj->pfid = this->fid;
    FakeRootObj->pfso = this;
    FakeRootObj->AttachChild(GlobalChildObj); /* the fid-name binding is aleady in the dir-pages */
    GlobalChildObj->pfid = *FakeRootFid;
    GlobalChildObj->pfso = FakeRootObj;
    Recov_EndTrans(MAXFP);
}

/* must not be called from within a transaction */
void fsobj::MixedToLocal(ViceFid *FakeRootFid, ViceFid *LocalChildFid, char *Name)
{
    FSO_ASSERT(this, FakeRootFid != NULL && LocalChildFid != NULL && Name != NULL);
    LOG(100, ("fsobj::MixToLocal: FakeRootFid = 0x%x.%x.%x LocallChildFid = 0x%x.%x.%x\n",
	      FakeRootFid->Volume, FakeRootFid->Vnode, FakeRootFid->Unique,
	      LocalChildFid->Volume, LocalChildFid->Vnode, LocalChildFid->Unique));
    LOG(100, ("fsobj::MixToLocal: RootParentFid = 0x%x.%x.%x Name = %s\n",
	      fid.Volume, fid.Vnode, fid.Unique, Name));
    /* "this" object is the RootParentObj in the LRDB->RFM map */
    fsobj *FakeRootObj = FSDB->Find(FakeRootFid);
    fsobj *LocalChildObj = FSDB->Find(LocalChildFid);
    FSO_ASSERT(this, FakeRootObj != NULL && LocalChildObj != NULL);
    Recov_BeginTrans();
    RVMLIB_REC_OBJECT(*this);
    RVMLIB_REC_OBJECT(*FakeRootObj);
    RVMLIB_REC_OBJECT(*LocalChildObj);
    this->dir_Delete(Name); /* replace fid-name binding in RootParentObj's dir-pages */
    this->dir_Create(Name, LocalChildFid);
    this->DetachChild(FakeRootObj);
    FakeRootObj->pfid = NullFid;
    FakeRootObj->pfso = NULL;
    FakeRootObj->DetachChild(LocalChildObj); /* preserve fid-name binding in FakeRootObjs's dir-pages */
    this->AttachChild(LocalChildObj);
    LocalChildObj->pfid = this->fid;
    LocalChildObj->pfso = this;
    Recov_EndTrans(MAXFP);
}

/* must not be called from within a transaction */
void fsobj::LocalToMixed(ViceFid *FakeRootFid, ViceFid *LocalChildFid, char *Name)
{
    FSO_ASSERT(this, FakeRootFid != NULL && LocalChildFid != NULL && Name != NULL);
    LOG(100, ("fsobj::LocalToMixed: FakeRootFid = 0x%x.%x.%x GlobalChildFid = 0x%x.%x.%x\n",
	      FakeRootFid->Volume, FakeRootFid->Vnode, FakeRootFid->Unique,
	      LocalChildFid->Volume, LocalChildFid->Vnode, LocalChildFid->Unique));
    LOG(100, ("fsobj::LocalToMixed: RootParentFid = 0x%x.%x.%x Name = %s\n",
	      fid.Volume, fid.Vnode, fid.Unique, Name));    
    /* "this" object is the RootParentObj in the LRDB->RFM map */
    fsobj *FakeRootObj = FSDB->Find(FakeRootFid);
    fsobj *LocalChildObj = FSDB->Find(LocalChildFid);
    FSO_ASSERT(this, FakeRootObj != NULL && LocalChildObj != NULL);
    Recov_BeginTrans();
    RVMLIB_REC_OBJECT(*this);
    RVMLIB_REC_OBJECT(*FakeRootObj);
    RVMLIB_REC_OBJECT(*LocalChildObj);
    this->dir_Delete(Name); /* replace fid-name binding in RootParentObj's dir-pages */
    this->dir_Create(Name, FakeRootFid);
    this->DetachChild(LocalChildObj);
    this->AttachChild(FakeRootObj);
    FakeRootObj->pfid = this->fid;
    FakeRootObj->pfso = this;
    FakeRootObj->AttachChild(LocalChildObj); /* the fid-name binding is aleady in the dir-pages */
    LocalChildObj->pfid = *FakeRootFid;
    LocalChildObj->pfso = FakeRootObj;
    Recov_EndTrans(MAXFP);
}

/* must not be called from within a transaction */
void fsobj::SetComp(char *name)
{
    FSO_ASSERT(this, name != NULL);
    int len = (int) strlen(name) + 1;
    Recov_BeginTrans();
    if (comp != NULL) {
	    RVMLIB_REC_OBJECT(comp);
	    rvmlib_rec_free(comp);
	   }
    comp = (char *)rvmlib_rec_malloc(len);
    rvmlib_set_range(comp, len);
    strcpy(comp, name);
    Recov_EndTrans(MAXFP);
}

/* must not be called from within a transaction */
void fsobj::RecoverRootParent(ViceFid *FakeRootFid, char *Name)
{
    /* 
     * "this" RootParentObj and we put the pair (FakeRootFid, Name) in the
     * RVM dir-pages and build the child-parent relationship between the two.
     */
    FSO_ASSERT(this, FakeRootFid && Name);
    fsobj *FakeRootObj = FSDB->Find(FakeRootFid);
    FSO_ASSERT(this, FakeRootObj && FakeRootObj->IsLocalObj() && FakeRootObj->IsFake());
    Recov_BeginTrans();	   
    RVMLIB_REC_OBJECT(*this);
    RVMLIB_REC_OBJECT(*FakeRootObj);
    dir_Delete(Name);
    dir_Create(Name, FakeRootFid);
    FakeRootObj->pfid = this->fid;
    FakeRootObj->pfso = this;
    AttachChild(FakeRootObj);
    Recov_EndTrans(MAXFP);
}

/* need not be called from within a transaction */
cmlent *fsobj::FinalCmlent(int tid)
{
    /* return the last cmlent done by iot tid */
    LOG(100, ("fsobj::FinalCmlent: 0x%x.%x.%x\n",
	      fid.Volume, fid.Vnode, fid.Unique));
    FSO_ASSERT(this, mle_bindings);
    dlist_iterator next(*mle_bindings);
    dlink *d;
    cmlent *last = (cmlent *)0;

    while ((d = next())) {
	binding *b = strbase(binding, d, bindee_handle);
	cmlent *m = (cmlent *)b->binder;
	CODA_ASSERT(m);
	if (m->GetTid() != tid) continue;
	last = m;
    }
    CODA_ASSERT(last && last->GetTid() == tid);
    return last;
}

/* need not be called from within a transaction */
int fsobj::IsAncestor(ViceFid *Fid) 
{
    /* chec if "Fid" is an ancestor of "this" */
    FSO_ASSERT(this, Fid);
    LOG(100, ("fsobj::IsAncestor:(0x%x.%x.%x) 0x%x.%x.%x\n", fid.Volume, 
	      fid.Vnode, fid.Unique, Fid->Volume, Fid->Vnode, Fid->Unique));
    fsobj *cfo = this;
    while (cfo) {
	LOG(100, ("fsobj::IsAncestor: current node is (%s, 0x%x.%x.%x)\n",
		  cfo->comp, cfo->fid.Volume, cfo->fid.Vnode, cfo->fid.Unique));
	if (!bcmp((const void *)&cfo->fid, (const void *)Fid, (int)sizeof(ViceFid))) {
	    return 1;
	}
	if (cfo->IsRoot()) {
	    LOG(100, ("fsobj::IsAncestor: going up to through a mount point\n"));
	    cfo = cfo->u.mtpoint;
	} else {
	    cfo = cfo->pfso;
	}
    }
    return 0;
}

/*
  BEGIN_HTML
  <a name="localize"><strong> the process of creating a local subtree
  for an inconsistent object </strong></a>
  END_HTML
*/
/* must not be called from within a transaction */
int fsobj::ReplaceLocalFakeFid()
{
    /*
     * this method replaces fid of objects in the subtree rooted at "this" with 
     * newly generated local fake fids. it uses a stack to avoid recursion.
     */
    LOG(100, ("fsobj::ReplaceLocalFakeFid: subtree root--(%s, 0x%x.%x.%x)\n",
	      comp, fid.Volume, fid.Vnode, fid.Unique));
    dlist stack;
    optent *opt = new optent(this);
    stack.prepend(opt);			/* INIT the stack with the subtree root */
    while (stack.count() > 0) {
	opt = (optent *)stack.get();	/* POP the stack */
	fsobj *obj = opt->GetFso();	/* get the current fsobj object */
	FSO_ASSERT(this, obj);
	obj->Lock(WR);
	
	delete opt;			/* GC the optent object */
	if (obj->IsLocalObj() && LRDB->RFM_IsFakeRoot(&(obj->fid))) {
	    LOG(100, ("fsobj::ReplaceLocalFakeFid: (%s, 0x%x.%x.%x)faked! merge tree\n",
		      obj->comp, obj->fid.Volume, obj->fid.Vnode, obj->fid.Unique));
	    /*
	     * Here we need to remove FakeRoot(which is "obj"), its "local" and "global" 
	     * children. Hookup the relationship of obj's parent and LocalRoot
	     * as child-parent and leave the GlobalRoot alone. Later, we may need to
	     * keep track of this merge-point for the de-localization process. 
	     */
	    LRDB->RFM_CoverRoot(&obj->fid);
	    ViceFid *LocalChildFid = LRDB->RFM_LookupLocalChild(&obj->fid);
	    ViceFid *GlobalChildFid = LRDB->RFM_LookupGlobalChild(&obj->fid);
	    ViceFid *LocalRootFid = LRDB->RFM_LookupLocalRoot(&obj->fid);
	    ViceFid *GlobalRootFid = LRDB->RFM_LookupGlobalRoot(&obj->fid);
	    ViceFid *RootParentFid = LRDB->RFM_LookupRootParent(&obj->fid);
	    FSO_ASSERT(this, LocalChildFid && GlobalChildFid);
	    FSO_ASSERT(this, LocalRootFid && GlobalRootFid && RootParentFid);
	    fsobj *LocalChildObj = FSDB->Find(LocalChildFid);
	    fsobj *GlobalChildObj = FSDB->Find(GlobalChildFid);	   
	    fsobj *LocalRootObj = FSDB->Find(LocalRootFid);
	    fsobj *GlobalRootObj = FSDB->Find(GlobalRootFid);
	    /*
	     * Note that objects LocalChildObj, GlobalChildObj and GlobalRootObj
	     * may or may not exist in FSDB depends on the situation. We can only
	     * be sure that LocalRootObj, FakeRootObj(==obj) exit.
	     *              ^^^^^^^^^^^^^ ^^^^^^^^^^^
	     */
	    FSO_ASSERT(this, LocalRootObj);

	    /* 
	     * Note that obj's parent node must not exit now since it was
	     * just localized. So we need to map the ParentRootFid into 
	     * its local from, which must exist in the LGM-map, and Find the fsobj.
	     */
	    ViceFid *CurrentRootParentFid = LRDB->LGM_LookupLocal(RootParentFid);
	    OBJ_ASSERT(this, CurrentRootParentFid);
	    fsobj *RootParentObj = FSDB->Find(CurrentRootParentFid);
	    OBJ_ASSERT(this, RootParentObj);

	    /* detach the FakeRootObj from RootParentObj */
	    Recov_BeginTrans();
	    RVMLIB_REC_OBJECT(*RootParentObj);
	    RVMLIB_REC_OBJECT(*obj);
	    RVMLIB_REC_OBJECT(*LocalRootObj);
	    RootParentObj->dir_Delete(obj->comp);
	    RootParentObj->dir_Create(obj->comp, LocalRootFid);
	    RootParentObj->DetachChild(obj);
	    obj->pfso = (fsobj *)NULL;
	    obj->pfid = NullFid;
	    RootParentObj->AttachChild(LocalRootObj);
	    if (LocalRootObj->IsRoot())
		    LocalRootObj->UnmountRoot();
	    LocalRootObj->pfso = RootParentObj;
	    LocalRootObj->pfid = RootParentObj->fid;
	    if (LocalChildObj != NULL && LocalChildObj->IsMtPt())
		    LocalChildObj->UncoverMtPt();
	    if (GlobalRootObj != NULL && GlobalRootObj->IsRoot())
		    GlobalRootObj->UnmountRoot();
	    if (GlobalChildObj != NULL && GlobalChildObj->IsMtPt())
		    GlobalChildObj->UncoverMtPt();

	    obj->Kill(0);			/* GC FakeRootObj */
	    if (LocalChildObj != NULL)
		    LocalChildObj->Kill(0);		/* GC LocalChildObj */
	      if (GlobalChildObj != NULL)
		      GlobalChildObj->Kill(0);	/* GC GlobalChildObj */
	      Recov_EndTrans(MAXFP);
	      
	      obj->UnLock(WR);
	      continue;
	}
	if (obj->IsDir() && HAVEALLDATA(obj) && (!obj->IsMtPt())) {
	    /* deal with possible un-cached children under "obj" */
	    LRDB->DirList_Clear();
	    ViceFid *DirFid = &obj->fid;
	    VenusData *DirData = &obj->data;

	    DH_EnumerateDir(&DirData->dir->dh, MakeDirList, (void *)DirFid->Volume);
	    LRDB->DirList_Process(obj);
	}
	if (obj->children != 0) {	/* Try to PUSH the stack if appropriate */
	    if (!HAVEALLDATA(obj)) {
		/* 
		 * In theory this should not happen at all, but there is rare sequence
		 * of actions that can discard the data for a directory object while
		 * some of its children are still cached. The code here attempts to
		 * treat this directory as if it does not have its children cached and
		 * do the local-fake-fid replacement for it after de-link all the current
		 * children from it so that none of them can point to the dir object. We
		 * will have very few chances to test whether this fix works well or not.
		 */
		LOG(0, ("fsobj::ReplaceLocalFakeFid: directory 0x%x.%x.%x have children but no data\n",
			obj->fid.Volume, obj->fid.Vnode, obj->fid.Unique));
		/* need to skip expanding DFS search tree for obj's children, and de-link them */
		dlink *d = 0;
		while ((d = obj->children->first())) {
		    fsobj *cf = strbase(fsobj, d, child_link);
		    obj->DetachChild(cf);
		    cf->pfso = 0;
		}
		CODA_ASSERT(obj->children->count() == 0);
		delete obj->children;
		obj->children = 0;
	    } else {
		/* expand the DFS search tree */
		dlist_iterator next(*(obj->children));
		dlink *d;
		while ((d = next())) {
		    fsobj *cf = strbase(fsobj, d, child_link);
		    /* fid-replacement is needed for any object that is not GCABLE()! */
		    if (GCABLE(cf)) continue;
		    opt = new optent(cf);
		    stack.prepend(opt);	/* PUSH the stack */
		}
	    }
	} else {
	    /* check for covered mount point */
	    if (obj->IsMtPt()) {
		/* PUSH the mount root into the stack */
		FSO_ASSERT(this, obj->u.root);
		opt = new optent(obj->u.root);
		stack.prepend(opt);
	    }
	}
	/* process the current fsobj object pointed to by "obj" */
	LOG(100, ("fsobj::ReplaceLocalFakeFid:  current node -- comp = %s, fid = %x.%x.%x\n", 
		  obj->comp, obj->fid.Volume, obj->fid.Vnode, obj->fid.Unique));

	/* do the actual fid replacement */
	ViceFid LocalFid;
	ViceFid GlobalFid;
	Recov_BeginTrans();
	LocalFid = LRDB->GenerateLocalFakeFid(stat.VnodeType);
	bcopy((const void *)&obj->fid, (void *)&GlobalFid, (int)sizeof(ViceFid));
	/* insert the local-global fid mapping */
	LRDB->LGM_Insert(&LocalFid, &GlobalFid);
	/* globally replace the global-fid with the local-fid */
	FSO_ASSERT(this, FSDB->TranslateFid(&GlobalFid, &LocalFid) == 0);
	LRDB->TranslateFid(&GlobalFid, &LocalFid);
	obj->vol->TranslateCMLFid(&GlobalFid, &LocalFid);
	obj->SetLocalObj();
	Recov_EndTrans(MAXFP);
	if (HAVEALLDATA(obj) && !DYING(obj))
		obj->SetRcRights(RC_DATA | RC_STATUS);
	else
		obj->ClearRcRights();
	
	obj->UnLock(WR);
    }
    FSO_ASSERT(this, stack.count() == 0);
    return 0;
}

/*
  BEGIN_HTML
  <a name="fakeify"><strong> the fakeify process to create
  representation for an object detected to be in local/global conflict
  </strong></a>
  END_HTML
*/
/* must not be called from within a transaction */
/* this method will be called when the volume is exclusively locked */
int fsobj::LocalFakeify()
{
    LOG(100, ("fsobj::LocalFakeify: %s, %x.%x.%x\n",
	      comp, fid.Volume, fid.Vnode, fid.Unique));
    int code = 0;

    /* 
     * step 0. purge kernel and scan FSDB to make sure child-parent
     * relation is properly maintained for every object.
     */
    (void)k_Purge();
    {
	fso_iterator next(NL);
	fsobj *obj;
	while ((obj = next())) {
	    if (obj->IsRoot()) continue;
	    if (GCABLE(obj)) continue;
	    if (obj->pfso == NULL && !FID_EQ(&obj->pfid, &NullFid)) {
		fsobj *pf = FSDB->Find(&obj->pfid);
		if (pf != 0 && HAVESTATUS(pf) && !GCABLE(pf)) {
		    /* re-estacblish the parent-chile linkage between pf and obj */
		    LOG(0, ("fsobj::LocalFakeify: relink 0x%x.%x.%x and 0x%x.%x.%x\n",
			    obj->fid.Volume, obj->fid.Vnode, obj->fid.Unique,
			    pf->fid.Volume, pf->fid.Vnode, pf->fid.Unique));
		    obj->pfso = pf;
		    pf->AttachChild(obj);
		}
	    }
	}
    }
    
    /* step 1. find the parent object */
    fsobj *pf = 0;
    if (!IsRoot()) {
	/* Laboriously scan database */
	fso_vol_iterator next(NL, vol);
	while ((pf = next())) {
	    if (!pf->IsDir() || pf->IsMtPt()) continue;
	    if (!HAVEALLDATA(pf)) continue;
	    if (!pf->dir_IsParent(&fid)) continue;
	    /* Found! */
	    break;
	}
	if (pf == 0) {
	    LOG(0, ("fsobj::LocalFakeify: %s, %x.%x.%x, parent not found\n",
		    comp, fid.Volume, fid.Vnode, fid.Unique));
	    return(ENOENT);
	}
    }

    FSO_ASSERT(this, (pf != NULL) || (NULL == pf && IsRoot()));
    if (NULL == pf) {
	LOG(0, ("fsobj::LocalFakeify: mount-point\n"));
	return LocalFakeifyRoot();
    }

    /* 
     * step 2. replace fid of objects in the subtree rooted at "this" 
     * object with "local fake fid", and mark them RC valid.
     */
    /* preserve the original global fid for "this" object */
    ViceFid GlobalRootFid;
    bcopy((const void *)&fid, (void *)&GlobalRootFid, (int)sizeof(ViceFid));

    if ((code = ReplaceLocalFakeFid()) != 0) {
	CHOKE("fsobj::LocalFakeify: replace local fake fid failed");
    }
    /* 
     * step 3. create a new object as FakeRoot with the newly generated 
     * FakeRootFid and make it a "fake directory" with two children named
     * "local" and "global", which will later be used as the mountpoint
     * pointing to the global and local subtrees.
     */
    ViceFid FakeRootFid, LocalChildFid, GlobalChildFid;
    RPC2_Unsigned AllocHost = 0;
    code = vol->AllocFid(Directory, &FakeRootFid, &AllocHost, V_UID);
    if (code != 0) {
	LOG(0, ("fsobj::LocalFakeify: can not alloc fid for the root object\n"));
	return code;
    }

    Recov_BeginTrans();
    GlobalChildFid = vol->GenerateFakeFid();
    LocalChildFid = vol->GenerateFakeFid();
    Recov_EndTrans(MAXFP);

    vproc *vp = VprocSelf();
    fsobj *FakeRoot = FSDB->Create(&FakeRootFid, NL, vp->u.u_priority, comp);
    if (NULL == FakeRoot) {
	LOG(0, ("fsobj::LocalFakeify: can not create Fake Root for %x.%x.%x\n",
		GlobalRootFid.Volume, GlobalRootFid.Vnode, GlobalRootFid.Unique));
	return (ENOSPC);
    }
    LOG(100, ("fsobj::LocalFakeify: created a new fake-root node\n"));
    Recov_BeginTrans();
    /* 
     * replace the (comp, GlobalRootFid) pair with the (comp, FakeRootFid)
     * pair in the parent directory structure. Note that "this" has become
     * the LocalRootFid's fsobj object.
     */
    RVMLIB_REC_OBJECT(*pf);
    RVMLIB_REC_OBJECT(*this);
    RVMLIB_REC_OBJECT(*FakeRoot);	   
    pf->dir_Delete(comp);
    pf->dir_Create(comp, &FakeRootFid);
    /* 
     * note that "pf" may not always have "this" in its children list and we
     * need to test this before we call the DetachChild routine
     */
    pf->DetachChild(this);
    pf->AttachChild(FakeRoot);
    this->pfso = NULL;
    this->pfid = NullFid;
    pf->RcRights = RC_DATA | RC_STATUS;		/* set RootParentObj in valid and non-mutatable status */
    pf->flags.local = 1;				/* it will be killed when repair is done */
    
	   /* Initialize status for the new fake-dir object */
    FakeRoot->flags.fake = 1;
    FakeRoot->flags.local = 1;
    FakeRoot->stat.DataVersion = 1;
    FakeRoot->stat.Mode = 0444;
    FakeRoot->stat.Owner = V_UID;
    FakeRoot->stat.Length = 0;
    FakeRoot->stat.Date = Vtime();
    FakeRoot->stat.LinkCount = 2;
    FakeRoot->stat.VnodeType = Directory;
    FakeRoot->Matriculate();		/* need this ??? -luqi */
    FakeRoot->pfid = pf->fid;
    FakeRoot->pfso = pf;
    /* Create the target directory. */
    FakeRoot->dir_MakeDir();
    FakeRoot->stat.Length = FakeRoot->dir_Length();
    FakeRoot->RcRights = RC_DATA | RC_STATUS;
    UpdateCacheStats(&FSDB->DirDataStats, CREATE, BLOCKS(FakeRoot));
    
    /* Create the "global" and "local" children. */
    
    FakeRoot->dir_Create("global", &GlobalChildFid);
    FakeRoot->dir_Create("local", &LocalChildFid);
    
    /* add an new entry to the LRDB maintained fid-map */
    LRDB->RFM_Insert(&FakeRootFid, &GlobalRootFid, &fid, &pf->fid,
		     &GlobalChildFid, &LocalChildFid, comp);
    Recov_EndTrans(MAXFP);

    LRDB->GetSubtreeStats(&fid);
    return(0);
}

/* must be called from within a transaction */
void fsobj::SetLocalObj()
{
    RVMLIB_REC_OBJECT(flags);
    flags.local = 1;
}

/* must be called from within a transaction */
void fsobj::UnsetLocalObj()
{
    RVMLIB_REC_OBJECT(flags);
    flags.local = 0;
}

/*
  BEGIN_HTML
  <a name="rootfakeify"><strong> the fakeify process to create
  representation for volume root object detected to be in local/global
  conflict </strong></a>
  END_HTML
*/
/* must not be called from within a transaction */
/* this method will be called when the volume is exclusively locked */
int fsobj::LocalFakeifyRoot()
{   
    LOG(100, ("fsobj::LocalFakeifyRoot: %s, %x.%x.%x\n",
	      comp, fid.Volume, fid.Vnode, fid.Unique));

    /* step 1: sanity checks */
    fsobj *MtPt = u.mtpoint;
    fsobj *pf = MtPt->pfso;
    if (pf == NULL) {
	LOG(100, ("fsobj::LocalFakeifyRoot: can not find parent\n"));
	return ENOENT;
    }

    /* 
     * step 2. replace fid of objects in the subtree rooted at "this" 
     * object with "local fake fid", and mark them RC valid as well.
     */
    int code = 0;
    ViceFid GlobalRootFid;
    bcopy((const void *)&fid, (void *)&GlobalRootFid, (int)sizeof(ViceFid));

    if ((code = ReplaceLocalFakeFid()) != 0) {
	CHOKE("fsobj::LocalFakeifyRoot: replace local fake fid failed");
    }

    Recov_BeginTrans();
    RVMLIB_REC_OBJECT(vol->flags);
    vol->flags.has_local_subtree = 1;
    Recov_EndTrans(MAXFP);

    /* 
     * step 3. create a new object as FakeRoot with the newly generated 
     * FakeRootFid and make it a "fake directory" with two children named
     * "local" and "global", which will later be used as the mountpoint
     * pointing to the global and local subtrees.
     */
    ViceFid FakeRootFid, LocalChildFid, GlobalChildFid;
    RPC2_Unsigned AllocHost = 0;
    code = vol->AllocFid(Directory, &FakeRootFid, &AllocHost, V_UID);
    if (code != 0) {
	LOG(0, ("fsobj::LocalFakeifyRoot: can not alloc fid for root object\n"));
	return code;
    }

    Recov_BeginTrans();
    GlobalChildFid = vol->GenerateFakeFid();
    LocalChildFid = vol->GenerateFakeFid();
    Recov_EndTrans(MAXFP);

    FakeRootFid.Volume = pf->fid.Volume;
    GlobalChildFid.Volume = pf->fid.Volume;
    LocalChildFid.Volume = pf->fid.Volume;
    vproc *vp = VprocSelf();
    fsobj *FakeRoot = FSDB->Create(&FakeRootFid, NL, vp->u.u_priority, comp);
    if (NULL == FakeRoot) {
	LOG(0, ("fsobj::LocalFakeifyRoot: can not create FakeRoot for %x.%x.%x\n",
		GlobalRootFid.Volume, GlobalRootFid.Vnode, GlobalRootFid.Unique));
	return (ENOSPC);
    }
    LOG(100, ("fsobj::LocalFakeifyRoot: created a new fake-root node\n"));
    Recov_BeginTrans();
    /* 
     * replace the (comp, MtPt->fid) pair with the (comp, FakeRootFid)
     * pair in the parent directory structure. Note that "this" has become
     * the LocalRootFid's fsobj object.
     */
    RVMLIB_REC_OBJECT(*pf);
    RVMLIB_REC_OBJECT(*this);
    RVMLIB_REC_OBJECT(*FakeRoot);	   
    pf->dir_Delete(comp);
    pf->dir_Create(comp, &FakeRootFid);
    /* 
     * note that "pf" may not always have "this" in its children list and we
     * need to test this before we call the DetachChild routine
     */
    pf->DetachChild(MtPt);
    pf->AttachChild(FakeRoot);
    this->pfso = NULL;
    this->pfid = NullFid;
    MtPt->pfso = NULL;
    MtPt->pfid = NullFid;
    pf->RcRights = RC_DATA | RC_STATUS;	/* set RootParentObj in valid and non-mutatable status */
    pf->flags.local = 1;			/* it will be killed when repair is done */

    /* Initialize status for the new fake-dir object */
    FakeRoot->flags.fake = 1;
    FakeRoot->flags.local = 1;
    FakeRoot->stat.DataVersion = 1;
    FakeRoot->stat.Mode = 0444;
    FakeRoot->stat.Owner = V_UID;
    FakeRoot->stat.Length = 0;
    FakeRoot->stat.Date = Vtime();
    FakeRoot->stat.LinkCount = 2;
    FakeRoot->stat.VnodeType = Directory;
    FakeRoot->Matriculate();		/* need this ??? -luqi */
    FakeRoot->pfid = pf->fid;
    FakeRoot->pfso = pf;
    /* Create the target directory. */
    FakeRoot->dir_MakeDir();
    FakeRoot->stat.Length = FakeRoot->dir_Length();
    FakeRoot->RcRights = RC_DATA | RC_STATUS;
    UpdateCacheStats(&FSDB->DirDataStats, CREATE, BLOCKS(FakeRoot));
    
    /* Create the "global" and "local" children. */
    
    FakeRoot->dir_Create("global", &GlobalChildFid);
    FakeRoot->dir_Create("local", &LocalChildFid);
    
    /* add an new entry to the LRDB maintained fid-map */
    rfment *rfm = new rfment(&FakeRootFid, &GlobalRootFid, &fid, &pf->fid,  
			     &GlobalChildFid, &LocalChildFid, comp);
    rfm->SetRootMtPt(MtPt);
    LRDB->root_fid_map.insert(rfm);
    Recov_EndTrans(MAXFP);

    LRDB->GetSubtreeStats(&fid);
    return(0);
}

