/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
              Copyright (c) 2002-2003 Intel Corporation

#*/

/*
 *
 *    CFS calls1.
 *
 *    ToDo:
 *       1. All mutating Vice calls should have the following IN arguments:
 *            NewSid, NewMutator (implicit from connection), NewMtime, 
 *            OldVV and DataVersion (for each object), NewStatus (for each object)
 */


#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/file.h>

#include <rpc2/rpc2.h>
/* interfaces */
#include <vice.h>

#ifdef __cplusplus
}
#endif


#include "comm.h"
#include "fso.h"
#include "mariner.h"
#include "mgrp.h"
#include "venuscb.h"
#include "venus.private.h"
#include "venusrecov.h"
#include "venusvol.h"
#include "vproc.h"
#include "worker.h"


/*  *****  Remove  *****  */

/* MUST be called from within transaction! */
void fsobj::LocalRemove(Date_t Mtime, char *name, fsobj *target_fso) {
    /* Update parent status. */
    {
	/* Delete the target name from the directory. */
	dir_Delete(name);

	/* Update the status to reflect the delete. */
	RVMLIB_REC_OBJECT(stat);
	stat.DataVersion++;
	stat.Length = dir_Length();
	stat.Date = Mtime;
    }

    /* Update the target status. */
    {
	RVMLIB_REC_OBJECT(target_fso->stat);
	target_fso->stat.LinkCount--;
	if (target_fso->stat.LinkCount == 0) {
	    UpdateCacheStats(&FSDB->FileAttrStats, REMOVE,
			     NBLOCKS(sizeof(fsobj)));
	    UpdateCacheStats(&FSDB->FileDataStats, REMOVE,
			     BLOCKS(target_fso));
	    target_fso->Kill();
	}
	else {
	    target_fso->stat.DataVersion++;
	    target_fso->DetachHdbBindings();
	}
    }
}


int fsobj::ConnectedRemove(Date_t Mtime, uid_t uid, char *name, fsobj *target_fso) {
    FSO_ASSERT(this, HOARDING(this));

    int code = 0;

    /* Status parameters. */
    ViceStatus parent_status;
    VenusToViceStatus(&stat, &parent_status);
    ViceStatus target_status;
    VenusToViceStatus(&target_fso->stat, &target_status);
    {
	/* Temporary!  Until RPC interface is fixed!  -JJK */
	parent_status.Date = Mtime;
    }

    /* COP2 Piggybacking. */
    char PiggyData[COP2SIZE];
    RPC2_CountedBS PiggyBS;
    PiggyBS.SeqLen = 0;
    PiggyBS.SeqBody = (RPC2_ByteSeq)PiggyData;

    /* VCB Arguments */
    RPC2_Integer VS = 0;
    CallBackStatus VCBStatus = NoCallBack;
    RPC2_CountedBS OldVS; 
    OldVS.SeqLen = 0;
    OldVS.SeqBody = 0;

    if (vol->IsReplicated()) {
	ViceStoreId sid;
	mgrpent *m = 0;
	int asy_resolve = 0;
        repvol *vp = (repvol *)vol;

	/* Acquire an Mgroup. */
	code = vp->GetMgrp(&m, uid, (PIGGYCOP2 ? &PiggyBS : 0));
	if (code != 0) goto RepExit;

	/* The COP1 call. */
	long cbtemp; cbtemp = cbbreaks;
	vv_t UpdateSet;

	Recov_BeginTrans();
	sid = vp->GenerateStoreId();
	Recov_EndTrans(MAXFP);
	{
	    /* Make multiple copies of the IN/OUT and OUT parameters. */
	    int ph_ix; unsigned long ph;
            ph = ntohl(m->GetPrimaryHost(&ph_ix)->s_addr);

	    vp->PackVS(VSG_MEMBERS, &OldVS);

	    ARG_MARSHALL(IN_OUT_MODE, ViceStatus, parent_statusvar, parent_status, VSG_MEMBERS);
	    ARG_MARSHALL(IN_OUT_MODE, ViceStatus, target_statusvar, target_status, VSG_MEMBERS);
	    ARG_MARSHALL(OUT_MODE, RPC2_Integer, VSvar, VS, VSG_MEMBERS);
	    ARG_MARSHALL(OUT_MODE, CallBackStatus, VCBStatusvar, VCBStatus, VSG_MEMBERS);

	    /* Make the RPC call. */
	    CFSOP_PRELUDE("store::Remove %-30s\n", name, target_fso->fid);
	    MULTI_START_MESSAGE(ViceVRemove_OP);
	    code = (int) MRPC_MakeMulti(ViceVRemove_OP, ViceVRemove_PTR,
				  VSG_MEMBERS, m->rocc.handles,
				  m->rocc.retcodes, m->rocc.MIp, 0, 0,
				  MakeViceFid(&fid), name,
				  parent_statusvar_ptrs, target_statusvar_ptrs,
				  ph, &sid, &OldVS, VSvar_ptrs,
				  VCBStatusvar_ptrs, &PiggyBS);
	    MULTI_END_MESSAGE(ViceVRemove_OP);
	    CFSOP_POSTLUDE("store::remove done\n");

	    free(OldVS.SeqBody);
	    /* Collate responses from individual servers and decide what to do next. */
	    code = vp->Collate_COP1(m, code, &UpdateSet);
	    MULTI_RECORD_STATS(ViceVRemove_OP);
	    if (code == EASYRESOLVE) { asy_resolve = 1; code = 0; }
	    if (code != 0) goto RepExit;

	    /* Collate volume callback information */
	    if (cbtemp == cbbreaks)
		vp->CollateVCB(m, VSvar_bufs, VCBStatusvar_bufs);

	    /* Finalize COP2 Piggybacking. */
	    if (PIGGYCOP2)
		vp->ClearCOP2(&PiggyBS);

	    /* Manually compute the OUT parameters from the mgrpent::Remove() call! -JJK */
	    int dh_ix; dh_ix = -1;
	    (void)m->DHCheck(0, ph_ix, &dh_ix);
	    ARG_UNMARSHALL(parent_statusvar, parent_status, dh_ix);
	    ARG_UNMARSHALL(target_statusvar, target_status, dh_ix);
	}

	/* Do Remove locally. */
	Recov_BeginTrans();
	LocalRemove(Mtime, name, target_fso);
	UpdateStatus(&parent_status, &UpdateSet, uid);
	target_fso->UpdateStatus(&target_status, &UpdateSet, uid);
	Recov_EndTrans(CMFP);
	if (ASYNCCOP2) ReturnEarly();

	/* Send the COP2 message or add an entry for piggybacking. */
        vp->COP2(m, &sid, &UpdateSet);

RepExit:
	if (m) m->Put();
	switch(code) {
	    case 0:
		if (asy_resolve) {
		    vp->ResSubmit(0, &fid);
		    vp->ResSubmit(0, &target_fso->fid);
		}
		break;

	    case ETIMEDOUT:
	    case ESYNRESOLVE:
	    case EINCONS:
		code = ERETRY;
		break;

	    default:
		break;
	}
    }
    else {
	/* Acquire a Connection. */
	connent *c;
	ViceStoreId Dummy;                  /* Need an address for ViceRemove */
        volrep *vp = (volrep *)vol;
	code = vp->GetConn(&c, uid);
	if (code != 0) goto NonRepExit;

	/* Make the RPC call. */
	CFSOP_PRELUDE("store::Remove %-30s\n", name, target_fso->fid);
	UNI_START_MESSAGE(ViceVRemove_OP);
	code = (int) ViceVRemove(c->connid, MakeViceFid(&fid), (RPC2_String)name, 
				 &parent_status, &target_status, 0, &Dummy, 
				 &OldVS, &VS, &VCBStatus, &PiggyBS);
	UNI_END_MESSAGE(ViceVRemove_OP);
	CFSOP_POSTLUDE("store::remove done\n");

	/* Examine the return code to decide what to do next. */
	code = vp->Collate(c, code);
	UNI_RECORD_STATS(ViceVRemove_OP);
	if (code != 0) goto NonRepExit;

	/* Do Remove locally. */
	Recov_BeginTrans();
	LocalRemove(Mtime, name, target_fso);
	UpdateStatus(&parent_status, 0, uid);
	target_fso->UpdateStatus(&target_status, 0, uid);
	Recov_EndTrans(CMFP);

NonRepExit:
	PutConn(&c);
    }

    return(code);
}


/* local-repair modification */
int fsobj::DisconnectedRemove(Date_t Mtime, uid_t uid, char *name, fsobj *target_fso, int Tid) {
    FSO_ASSERT(this, (EMULATING(this) || LOGGING(this)));

    int code = 0;

    if (!vol->IsReplicated()) {
	code = ETIMEDOUT;
	goto Exit;
    }

    Recov_BeginTrans();
    code = ((repvol *)vol)->LogRemove(Mtime, uid, &fid, name,
                                      &target_fso->fid,
                                      target_fso->stat.LinkCount, Tid);

    if (code == 0)
	    /* This MUST update second-class state! */
	    LocalRemove(Mtime, name, target_fso);
    Recov_EndTrans(DMFP);

Exit:
    return(code);
}


/* local-repair modification */
int fsobj::Remove(char *name, fsobj *target_fso, uid_t uid)
{
    LOG(10, ("fsobj::Remove: (%s, %s), uid = %d\n", GetComp(), name, uid));

    int code = 0;
    Date_t Mtime = Vtime();

    int conn, tid;
    GetOperationState(&conn, &tid);
    if (conn == 0) {
	code = DisconnectedRemove(Mtime, uid, name, target_fso, tid);
    }
    else {
	code = ConnectedRemove(Mtime, uid, name, target_fso);
    }

    if (code != 0) {
	Demote();
	target_fso->Demote();
    }
    return(code);
}


/*  *****  Link  *****  */

/* MUST be called from within transaction! */
void fsobj::LocalLink(Date_t Mtime, char *name, fsobj *source_fso) {
    /* Update parent status. */
    {
	/* Add the new <name, fid> to the directory. */
	dir_Create(name, &source_fso->fid);

	/* Update the status to reflect the create. */
	RVMLIB_REC_OBJECT(stat);
	stat.DataVersion++;
	stat.Length = dir_Length();
	stat.Date = Mtime;
	if (source_fso->IsDir())
	    stat.LinkCount++;
	DemoteHdbBindings();	    /* in case an expansion would now be satisfied! */
    }

    /* Update source status. */
    {
	RVMLIB_REC_OBJECT(source_fso->stat);
/*    source_fso->stat.DataVersion++;*/
	source_fso->stat.LinkCount++;
    }
}


int fsobj::ConnectedLink(Date_t Mtime, uid_t uid, char *name, fsobj *source_fso)
{
    FSO_ASSERT(this, HOARDING(this));

    int code = 0;

    /* Status parameters. */
    ViceStatus parent_status;
    VenusToViceStatus(&stat, &parent_status);
    ViceStatus source_status;
    VenusToViceStatus(&source_fso->stat, &source_status);
    {
	/* Temporary!  Until RPC interface is fixed!  -JJK */
	parent_status.Date = Mtime;
    }

    /* COP2 Piggybacking. */
    char PiggyData[COP2SIZE];
    RPC2_CountedBS PiggyBS;
    PiggyBS.SeqLen = 0;
    PiggyBS.SeqBody = (RPC2_ByteSeq)PiggyData;

    /* VCB Arguments */
    RPC2_Integer VS = 0;
    CallBackStatus VCBStatus = NoCallBack;
    RPC2_CountedBS OldVS; 
    OldVS.SeqLen = 0;
    OldVS.SeqBody = 0;

    if (vol->IsReplicated()) {
	ViceStoreId sid;
	mgrpent *m = 0;
	int asy_resolve = 0;
        repvol *vp = (repvol *)vol;

	/* Acquire an Mgroup. */
	code = vp->GetMgrp(&m, uid, (PIGGYCOP2 ? &PiggyBS : 0));
	if (code != 0) goto RepExit;

	/* The COP1 call. */
	long cbtemp; cbtemp = cbbreaks;
	vv_t UpdateSet;

	Recov_BeginTrans();
	sid = vp->GenerateStoreId();
	Recov_EndTrans(MAXFP);
	{
	    /* Make multiple copies of the IN/OUT and OUT parameters. */
	    int ph_ix; unsigned long ph;
            ph = ntohl(m->GetPrimaryHost(&ph_ix)->s_addr);

 	    vp->PackVS(VSG_MEMBERS, &OldVS);
	    ARG_MARSHALL(IN_OUT_MODE, ViceStatus, source_statusvar, source_status, VSG_MEMBERS);
	    ARG_MARSHALL(IN_OUT_MODE, ViceStatus, parent_statusvar, parent_status, VSG_MEMBERS);
	    ARG_MARSHALL(OUT_MODE, RPC2_Integer, VSvar, VS, VSG_MEMBERS);
	    ARG_MARSHALL(OUT_MODE, CallBackStatus, VCBStatusvar, VCBStatus, VSG_MEMBERS);

	    /* Make the RPC call. */
	    CFSOP_PRELUDE("store::Link %-30s\n", name, source_fso->fid);
	    MULTI_START_MESSAGE(ViceVLink_OP);
	    code = (int) MRPC_MakeMulti(ViceVLink_OP, ViceVLink_PTR,
				  VSG_MEMBERS, m->rocc.handles,
				  m->rocc.retcodes, m->rocc.MIp, 0, 0,
				  MakeViceFid(&fid), name,
				  MakeViceFid(&source_fso->fid),
				  source_statusvar_ptrs, parent_statusvar_ptrs,
				  ph, &sid, &OldVS, VSvar_ptrs,
				  VCBStatusvar_ptrs, &PiggyBS);
	    MULTI_END_MESSAGE(ViceVLink_OP);
	    CFSOP_POSTLUDE("store::link done\n");

	    free(OldVS.SeqBody);
	    /* Collate responses from individual servers and decide what to do next. */
	    code = vp->Collate_COP1(m, code, &UpdateSet);
	    MULTI_RECORD_STATS(ViceVLink_OP);
	    if (code == EASYRESOLVE) { asy_resolve = 1; code = 0; }
	    if (code != 0) goto RepExit;

	    /* Collate volume callback information */
	    if (cbtemp == cbbreaks)
		vp->CollateVCB(m, VSvar_bufs, VCBStatusvar_bufs);

	    /* Finalize COP2 Piggybacking. */
	    if (PIGGYCOP2)
		vp->ClearCOP2(&PiggyBS);

	    /* Manually compute the OUT parameters from the mgrpent::Link() call! -JJK */
	    int dh_ix; dh_ix = -1;
	    (void)m->DHCheck(0, ph_ix, &dh_ix);
	    ARG_UNMARSHALL(source_statusvar, source_status, dh_ix);
	    ARG_UNMARSHALL(parent_statusvar, parent_status, dh_ix);
	}

	/* Do Link locally. */
	Recov_BeginTrans();
	LocalLink(Mtime, name, source_fso);
	UpdateStatus(&parent_status, &UpdateSet, uid);
	source_fso->UpdateStatus(&source_status, &UpdateSet, uid);
	Recov_EndTrans(CMFP);
	if (ASYNCCOP2) ReturnEarly();

	/* Send the COP2 message or add an entry for piggybacking. */
        vp->COP2(m, &sid, &UpdateSet);

RepExit:
	if (m) m->Put();
	switch(code) {
	    case 0:
		if (asy_resolve) {
		    vp->ResSubmit(0, &fid);
		    vp->ResSubmit(0, &source_fso->fid);
		}
		break;

	    case ETIMEDOUT:
	    case ESYNRESOLVE:
	    case EINCONS:
		code = ERETRY;
		break;

	    default:
		break;
	}
    }
    else {
	/* Acquire a Connection. */
	connent *c;
	ViceStoreId Dummy;                   /* Need an address for ViceLink */
        volrep *vp = (volrep *)vol;
	code = vp->GetConn(&c, uid);
	if (code != 0) goto NonRepExit;

	/* Make the RPC call. */
	CFSOP_PRELUDE("store::Link %-30s\n", name, source_fso->fid);
	UNI_START_MESSAGE(ViceVLink_OP);
	code = (int) ViceVLink(c->connid, MakeViceFid(&fid), (RPC2_String)name,
			      MakeViceFid(&source_fso->fid), &source_status,
			      &parent_status, 0, &Dummy, 
			      &OldVS, &VS, &VCBStatus, &PiggyBS);
	UNI_END_MESSAGE(ViceVLink_OP);
	CFSOP_POSTLUDE("store::link done\n");

	/* Examine the return code to decide what to do next. */
	code = vp->Collate(c, code);
	UNI_RECORD_STATS(ViceVLink_OP);
	if (code != 0) goto NonRepExit;

	/* Do Link locally. */
	Recov_BeginTrans();
	LocalLink(Mtime, name, source_fso);
	UpdateStatus(&parent_status, NULL, uid);
	source_fso->UpdateStatus(&source_status, NULL, uid);
	Recov_EndTrans(CMFP);

NonRepExit:
	PutConn(&c);
    }

    return(code);
}


/* local-repair modification */
int fsobj::DisconnectedLink(Date_t Mtime, uid_t uid, char *name, fsobj *source_fso, int Tid)
{
    FSO_ASSERT(this, (EMULATING(this) || LOGGING(this)));

    int code = 0;

    if (!vol->IsReplicated()) {
	code = ETIMEDOUT;
	goto Exit;
    }

    Recov_BeginTrans();
    code = ((repvol *)vol)->LogLink(Mtime, uid, &fid, name, &source_fso->fid, Tid);
    
    if (code == 0)
	    /* This MUST update second-class state! */
	    LocalLink(Mtime, name, source_fso);
    Recov_EndTrans(DMFP);

Exit:
    return(code);
}


/* local-repair modification */
int fsobj::Link(char *name, fsobj *source_fso, uid_t uid)
{
    LOG(10, ("fsobj::Link: (%s/%s, %s), uid = %d\n",
	      GetComp(), source_fso->comp, name, uid));

    int code = 0;
    Date_t Mtime = Vtime();

    int conn, tid;
    GetOperationState(&conn, &tid);
    if (conn == 0) {
	code = DisconnectedLink(Mtime, uid, name, source_fso, tid);
    }
    else {
	code = ConnectedLink(Mtime, uid, name, source_fso);
    }

    if (code != 0) {
	Demote();
	source_fso->Demote();
    }
    return(code);
}


/*  *****  Rename  *****  */

/* MUST be called from within transaction! */
void fsobj::LocalRename(Date_t Mtime, fsobj *s_parent_fso, char *s_name,
			 fsobj *s_fso, char *t_name, fsobj *t_fso) {
    int SameParent = (s_parent_fso == this);
    int TargetExists = (t_fso != 0);

    /* Update local status. */
    {
	RVMLIB_REC_OBJECT(stat);
	if (!SameParent)
	    RVMLIB_REC_OBJECT(s_parent_fso->stat);
	RVMLIB_REC_OBJECT(s_fso->stat);
	if (TargetExists)
	    RVMLIB_REC_OBJECT(t_fso->stat);

	/*Remove the source <name, fid> from its directory. */
	s_parent_fso->dir_Delete(s_name);

	/* Remove the target <name, fid> from its directory (if it exists). */
	if (TargetExists) {
	    dir_Delete(t_name);

	    t_fso->DetachHdbBindings();
	    if (t_fso->IsDir()) {
		stat.LinkCount--;

		/* Delete the target object. */
		UpdateCacheStats(&FSDB->DirAttrStats, REMOVE,
				 NBLOCKS(sizeof(fsobj)));
		UpdateCacheStats(&FSDB->DirDataStats, REMOVE,
				 BLOCKS(t_fso));
		t_fso->Kill();
	    }
	    else {
		/* Update the target status. */
		t_fso->stat.LinkCount--;
		if (t_fso->stat.LinkCount == 0) {
		    UpdateCacheStats(&FSDB->FileAttrStats, REMOVE,
				     NBLOCKS(sizeof(fsobj)));
		    UpdateCacheStats(&FSDB->FileDataStats, REMOVE,
				     BLOCKS(t_fso));
		    t_fso->Kill();
		}
		else {
		    t_fso->stat.DataVersion++;
		}
	    }
	}

	/* Create the target <name, fid> in the target directory. */
	dir_Create(t_name, &s_fso->fid);

	/* Alter ".." entry in source if necessary. */
	if (!SameParent && s_fso->IsDir()) {
	    s_fso->dir_Delete("..");
	    s_parent_fso->stat.LinkCount--;
	    s_fso->dir_Create("..", &fid);
	    stat.LinkCount++;
	}

	/* Update parents' status to reflect the create(s) and delete(s). */
	stat.DataVersion++;
	stat.Length = dir_Length();
	stat.Date = Mtime;
	if (SameParent) {
	    DemoteHdbBindings();	    /* in case an expansion would now be satisfied! */
	}
	else {
	    s_parent_fso->stat.DataVersion++;
	    s_parent_fso->stat.Length = s_parent_fso->dir_Length();
	    s_parent_fso->stat.Date = Mtime;
	}

	/* Update the source status to reflect the rename and possible create/delete. */
	if (t_name && !STREQ(s_fso->comp, t_name))
	    s_fso->SetComp(t_name);
	s_fso->DetachHdbBindings();
/*    s_fso->stat.DataVersion++;*/
	if (!SameParent)
	    s_fso->SetParent(fid.Vnode, fid.Unique);
    }
}


int fsobj::ConnectedRename(Date_t Mtime, uid_t uid, fsobj *s_parent_fso,
			    char *s_name, fsobj *s_fso, char *t_name, fsobj *t_fso)
{
    FSO_ASSERT(this, HOARDING(this));

    int code = 0;
    int SameParent = (s_parent_fso == this);
    int TargetExists = (t_fso != 0);

    /* Status parameters. */
    ViceStatus t_parent_status;
    VenusToViceStatus(&stat, &t_parent_status);
    ViceStatus s_parent_status;
    VenusToViceStatus(&s_parent_fso->stat, &s_parent_status);
    ViceStatus source_status;
    VenusToViceStatus(&s_fso->stat, &source_status);
    ViceStatus target_status;
    if (TargetExists) VenusToViceStatus(&t_fso->stat, &target_status);
    {
	/* Temporary!  Until RPC interface is fixed!  -JJK */
	s_parent_status.Date = Mtime;
    }

    /* COP2 Piggybacking. */
    char PiggyData[COP2SIZE];
    RPC2_CountedBS PiggyBS;
    PiggyBS.SeqLen = 0;
    PiggyBS.SeqBody = (RPC2_ByteSeq)PiggyData;

    /* VCB Arguments */
    RPC2_Integer VS = 0;
    CallBackStatus VCBStatus = NoCallBack;
    RPC2_CountedBS OldVS; 
    OldVS.SeqLen = 0;
    OldVS.SeqBody = 0;

    if (vol->IsReplicated()) {
	ViceStoreId sid;
	mgrpent *m = 0;
	int asy_resolve = 0;
        repvol *vp = (repvol *)vol;

	/* Acquire an Mgroup. */
	code = vp->GetMgrp(&m, uid, (PIGGYCOP2 ? &PiggyBS : 0));
	if (code != 0) goto RepExit;

	/* The COP1 call. */
	long cbtemp; cbtemp = cbbreaks;
	vv_t UpdateSet;

	Recov_BeginTrans();
	sid = vp->GenerateStoreId();
	Recov_EndTrans(MAXFP);
	{
	    /* Make multiple copies of the IN/OUT and OUT parameters. */
	    int ph_ix; unsigned long ph;
            ph = ntohl(m->GetPrimaryHost(&ph_ix)->s_addr);

 	    vp->PackVS(VSG_MEMBERS, &OldVS);
	    ARG_MARSHALL(IN_OUT_MODE, ViceStatus, t_parent_statusvar, t_parent_status, VSG_MEMBERS);
	    ARG_MARSHALL(IN_OUT_MODE, ViceStatus, s_parent_statusvar, s_parent_status, VSG_MEMBERS);
	    ARG_MARSHALL(IN_OUT_MODE, ViceStatus, source_statusvar, source_status, VSG_MEMBERS);
	    ARG_MARSHALL(IN_OUT_MODE, ViceStatus, target_statusvar, target_status, VSG_MEMBERS);
	    ARG_MARSHALL(OUT_MODE, RPC2_Integer, VSvar, VS, VSG_MEMBERS);
	    ARG_MARSHALL(OUT_MODE, CallBackStatus, VCBStatusvar, VCBStatus, VSG_MEMBERS);

	    /* Make the RPC call. */
	    CFSOP_PRELUDE("store::Rename %-30s\n", s_name, s_fso->fid);
	    MULTI_START_MESSAGE(ViceVRename_OP);
	    code = (int) MRPC_MakeMulti(ViceVRename_OP, ViceVRename_PTR,
				  VSG_MEMBERS, m->rocc.handles,
				  m->rocc.retcodes, m->rocc.MIp, 0, 0,
				  MakeViceFid(&s_parent_fso->fid), s_name,
				  MakeViceFid(&fid), t_name,
				  s_parent_statusvar_ptrs,
				  t_parent_statusvar_ptrs,
				  source_statusvar_ptrs, target_statusvar_ptrs,
				  ph, &sid, &OldVS, VSvar_ptrs,
				  VCBStatusvar_ptrs, &PiggyBS);
	    MULTI_END_MESSAGE(ViceVRename_OP);
	    CFSOP_POSTLUDE("store::rename done\n");

	    free(OldVS.SeqBody);
	    /* Collate responses from individual servers and decide what to do next. */
	    code = vp->Collate_COP1(m, code, &UpdateSet);
	    MULTI_RECORD_STATS(ViceVRename_OP);
	    if (code == EASYRESOLVE) { asy_resolve = 1; code = 0; }
	    if (code != 0) goto RepExit;

	    /* Collate volume callback information */
	    if (cbtemp == cbbreaks)
		vp->CollateVCB(m, VSvar_bufs, VCBStatusvar_bufs);

	    /* Finalize COP2 Piggybacking. */
	    if (PIGGYCOP2)
		vp->ClearCOP2(&PiggyBS);

	    /* Manually compute the OUT parameters from the mgrpent::Rename() call! -JJK */
	    int dh_ix; dh_ix = -1;
	    (void)m->DHCheck(0, ph_ix, &dh_ix);
	    ARG_UNMARSHALL(t_parent_statusvar, t_parent_status, dh_ix);
	    ARG_UNMARSHALL(s_parent_statusvar, s_parent_status, dh_ix);
	    ARG_UNMARSHALL(source_statusvar, source_status, dh_ix);
	    ARG_UNMARSHALL(target_statusvar, target_status, dh_ix);
	}

	/* Do Rename locally. */
	Recov_BeginTrans();
	LocalRename(Mtime, s_parent_fso, s_name, s_fso, t_name, t_fso);
	UpdateStatus(&t_parent_status, &UpdateSet, uid);
	if (!SameParent)
		s_parent_fso->UpdateStatus(&s_parent_status, &UpdateSet, uid);
	s_fso->UpdateStatus(&source_status, &UpdateSet, uid);
	if (TargetExists)
		t_fso->UpdateStatus(&target_status, &UpdateSet, uid);
	Recov_EndTrans(CMFP);
	if (ASYNCCOP2) ReturnEarly();

	/* Send the COP2 message or add an entry for piggybacking. */
        vp->COP2(m, &sid, &UpdateSet);

RepExit:
	if (m) m->Put();
	switch(code) {
	    case 0:
		if (asy_resolve) {
		    vp->ResSubmit(0, &fid);
		    if (!SameParent)
			vp->ResSubmit(0, &s_parent_fso->fid);
		    vp->ResSubmit(0, &s_fso->fid);
		    if (TargetExists)
			vp->ResSubmit(0, &t_fso->fid);
		}
		break;

	    case ETIMEDOUT:
	    case ESYNRESOLVE:
	    case EINCONS:
		code = ERETRY;
		break;

	    default:
		break;
	}
    }
    else {
	/* Acquire a Connection. */
	connent *c;
	ViceStoreId Dummy;                  /* Need an address for ViceRename */
        volrep *vp = (volrep *)vol;
	code = vp->GetConn(&c, uid);
	if (code != 0) goto NonRepExit;

	/* Make the RPC call. */
	CFSOP_PRELUDE("store::Rename %-30s\n", s_name, s_fso->fid);
	UNI_START_MESSAGE(ViceVRename_OP);
	code = (int) ViceVRename(c->connid, MakeViceFid(&s_parent_fso->fid),
				 (RPC2_String)s_name, MakeViceFid(&fid),
				 (RPC2_String)t_name, &s_parent_status,
				 &t_parent_status, &source_status,
				 &target_status, 0, &Dummy, &OldVS, &VS,
				 &VCBStatus, &PiggyBS);
	UNI_END_MESSAGE(ViceVRename_OP);
	CFSOP_POSTLUDE("store::rename done\n");

	/* Examine the return code to decide what to do next. */
	code = vp->Collate(c, code);
	UNI_RECORD_STATS(ViceVRename_OP);
	if (code != 0) goto NonRepExit;

	/* Release the Connection. */
	PutConn(&c);

	/* Do Rename locally. */
	Recov_BeginTrans();
	LocalRename(Mtime, s_parent_fso, s_name, s_fso, t_name, t_fso);
	UpdateStatus(&t_parent_status, NULL, uid);
	if (!SameParent)
		s_parent_fso->UpdateStatus(&s_parent_status, NULL, uid);
	s_fso->UpdateStatus(&source_status, NULL, uid);
	if (TargetExists)
		t_fso->UpdateStatus(&target_status, NULL, uid);
	Recov_EndTrans(CMFP);

NonRepExit:
	PutConn(&c);
    }

    return(code);
}


/* local-repair modification */
int fsobj::DisconnectedRename(Date_t Mtime, uid_t uid, fsobj *s_parent_fso, char *s_name, 
			      fsobj *s_fso, char *t_name, fsobj *t_fso, int Tid)
{
    FSO_ASSERT(this, (EMULATING(this) || LOGGING(this)));

    int code = 0;
    int TargetExists = (t_fso != 0);

    if (!vol->IsReplicated()) {
	code = ETIMEDOUT;
	goto Exit;
    }

    Recov_BeginTrans();
    code = ((repvol *)vol)->LogRename(Mtime, uid, &s_parent_fso->fid, s_name,
                                      &fid, t_name, &s_fso->fid,
                                      (TargetExists ? &t_fso->fid : &NullFid),
                                      (TargetExists ? t_fso->stat.LinkCount:0),
                                      Tid);

    if (code == 0)
	    /* This MUST update second-class state! */
	    LocalRename(Mtime, s_parent_fso, s_name, s_fso, t_name, t_fso);
    Recov_EndTrans(DMFP);

Exit:
    return(code);
}


/* local-repair modification */
int fsobj::Rename(fsobj *s_parent_fso, char *s_name, fsobj *s_fso,
		   char *t_name, fsobj *t_fso, uid_t uid)
{
    LOG(10, ("fsobj::Rename : (%s/%s, %s/%s), uid = %d\n",
	      (s_parent_fso ? s_parent_fso->GetComp() : GetComp()),
	      s_name, GetComp(), t_name, uid));

    int code = 0;
    Date_t Mtime = Vtime();
    int SameParent = (s_parent_fso == 0);
    if (SameParent) s_parent_fso = this;
    int TargetExists = (t_fso != 0);

    int conn, tid;
    GetOperationState(&conn, &tid);
    if (conn == 0) {
	code = DisconnectedRename(Mtime, uid, s_parent_fso, s_name, s_fso, 
				  t_name, t_fso, tid);
    }
    else {
	code = ConnectedRename(Mtime, uid, s_parent_fso,
			       s_name, s_fso, t_name, t_fso);
    }

    if (code != 0) {
	Demote();
	if (!SameParent) s_parent_fso->Demote();
	s_fso->Demote();
	if (TargetExists) t_fso->Demote();
    }
    return(code);
}


/*  *****  Mkdir  *****  */

/* MUST be called from within transaction! */
void fsobj::LocalMkdir(Date_t Mtime, fsobj *target_fso, char *name,
			uid_t Owner, unsigned short Mode)
{
    /* Update parent status. */
    {
	/* Add the new <name, fid> to the directory. */
	dir_Create(name, &target_fso->fid);

	/* Update the status to reflect the create. */
	RVMLIB_REC_OBJECT(stat);
	stat.DataVersion++;
	stat.Length = dir_Length();
	stat.Date = Mtime;
	stat.LinkCount++;
    }

    /* Set target status and data. */
    {
	/* What about ACL? -JJK */
	RVMLIB_REC_OBJECT(*target_fso);
	target_fso->stat.VnodeType = Directory;
	target_fso->stat.LinkCount = 2;
	target_fso->stat.Length = 0;
	target_fso->stat.DataVersion = 1;
	target_fso->stat.Date = Mtime;
	target_fso->stat.Owner = Owner;
	target_fso->stat.Mode = Mode;
	target_fso->AnyUser = AnyUser;
	memcpy(target_fso->SpecificUser, SpecificUser, (CPSIZE * sizeof(AcRights)));
	target_fso->SetParent(fid.Vnode, fid.Unique);

	/* Create the target directory. */
	target_fso->dir_MakeDir();

	target_fso->Matriculate();
	target_fso->Reference();
	target_fso->ComputePriority();
    }
}


int fsobj::ConnectedMkdir(Date_t Mtime, uid_t uid, fsobj **t_fso_addr,
			   char *name, unsigned short Mode, int target_pri)
{
    FSO_ASSERT(this, HOARDING(this));

    int code = 0;
    fsobj *target_fso = 0;
    VenusFid target_fid;
    RPC2_Unsigned AllocHost = 0;

    /* Status parameters. */
    ViceStatus parent_status;
    VenusToViceStatus(&stat, &parent_status);
    ViceStatus target_status;
    target_status.Mode = Mode;
    {
	/* Temporary!  Until RPC interface is fixed!  -JJK */
	parent_status.Date = Mtime;
	target_status.DataVersion = 1;
	target_status.VV = NullVV;
    }

    /* COP2 Piggybacking. */
    char PiggyData[COP2SIZE];
    RPC2_CountedBS PiggyBS;
    PiggyBS.SeqLen = 0;
    PiggyBS.SeqBody = (RPC2_ByteSeq)PiggyData;

    /* VCB Arguments */
    RPC2_Integer VS = 0;
    CallBackStatus VCBStatus = NoCallBack;
    RPC2_CountedBS OldVS; 
    OldVS.SeqLen = 0;
    OldVS.SeqBody = 0;

    if (vol->IsReplicated()) {
	ViceStoreId sid;
	mgrpent *m = 0;
	int asy_resolve = 0;
        repvol *vp = (repvol *)vol;

	/* Allocate a fid for the new object. */
	code = vp->AllocFid(Directory, &target_fid, &AllocHost, uid);
	if (code != 0) goto RepExit;

	/* Allocate the fsobj. */
	target_fso = FSDB->Create(&target_fid, target_pri, name);
	if (target_fso == 0) {
	    UpdateCacheStats(&FSDB->DirAttrStats, NOSPACE,
			     NBLOCKS(sizeof(fsobj)));
	    code = ENOSPC;
	    goto RepExit;
	}
	UpdateCacheStats(&FSDB->DirAttrStats, CREATE,
			 NBLOCKS(sizeof(fsobj)));

	/* Acquire an Mgroup. */
	code = vp->GetMgrp(&m, uid, (PIGGYCOP2 ? &PiggyBS : 0));
	if (code != 0) goto RepExit;

	/* The COP1 call. */
	long cbtemp; cbtemp = cbbreaks;
	vv_t UpdateSet;

	Recov_BeginTrans();
	sid = vp->GenerateStoreId();
	Recov_EndTrans(MAXFP);
	{
	    ViceFid target = *MakeViceFid(&target_fid);

	    /* Make multiple copies of the IN/OUT and OUT parameters. */
 	    vp->PackVS(VSG_MEMBERS, &OldVS);
	    ARG_MARSHALL(IN_OUT_MODE, ViceStatus, target_statusvar, target_status, VSG_MEMBERS);
	    ARG_MARSHALL(IN_OUT_MODE, ViceFid, targetvar, target, VSG_MEMBERS);
	    ARG_MARSHALL(IN_OUT_MODE, ViceStatus, parent_statusvar, parent_status, VSG_MEMBERS);
	    ARG_MARSHALL(OUT_MODE, RPC2_Integer, VSvar, VS, VSG_MEMBERS);
	    ARG_MARSHALL(OUT_MODE, CallBackStatus, VCBStatusvar, VCBStatus, VSG_MEMBERS);

	    /* Make the RPC call. */
	    CFSOP_PRELUDE("store::Mkdir %-30s\n", name, NullFid);
	    MULTI_START_MESSAGE(ViceVMakeDir_OP);
	    code = (int) MRPC_MakeMulti(ViceVMakeDir_OP, ViceVMakeDir_PTR,
					VSG_MEMBERS, m->rocc.handles,
					m->rocc.retcodes, m->rocc.MIp, 0, 0,
					MakeViceFid(&fid), name,
					target_statusvar_ptrs,
					targetvar_ptrs,
					parent_statusvar_ptrs, AllocHost, &sid,
					&OldVS, VSvar_ptrs, VCBStatusvar_ptrs,
					&PiggyBS);
	    MULTI_END_MESSAGE(ViceVMakeDir_OP);
	    CFSOP_POSTLUDE("store::mkdir done\n");

	    free(OldVS.SeqBody);
	    /* Collate responses from individual servers and decide what to do next. */
	    code = vp->Collate_COP1(m, code, &UpdateSet);
	    MULTI_RECORD_STATS(ViceVMakeDir_OP);
	    if (code == EASYRESOLVE) { asy_resolve = 1; code = 0; }
	    if (code != 0) goto RepExit;

	    /* Collate volume callback information */
	    if (cbtemp == cbbreaks)
		vp->CollateVCB(m, VSvar_bufs, VCBStatusvar_bufs);

	    /* Finalize COP2 Piggybacking. */
	    if (PIGGYCOP2)
		vp->ClearCOP2(&PiggyBS);

	    /* Manually compute the OUT parameters from the mgrpent::Mkdir() call! -JJK */
	    int dh_ix; dh_ix = -1;
	    (void)m->DHCheck(0, -1, &dh_ix);
	    ARG_UNMARSHALL(target_statusvar, target_status, dh_ix);
	    ARG_UNMARSHALL(targetvar, target, dh_ix);
	    ARG_UNMARSHALL(parent_statusvar, parent_status, dh_ix);
	    MakeVenusFid(&target_fid, target_fid.Realm, &target);
	}

	/* Do Mkdir locally. */
	Recov_BeginTrans();
	LocalMkdir(Mtime, target_fso, name, uid, Mode);
	UpdateStatus(&parent_status, &UpdateSet, uid);
	target_fso->UpdateStatus(&target_status, &UpdateSet, uid);
	Recov_EndTrans(CMFP);
	if (target_status.CallBack == CallBackSet && cbtemp == cbbreaks)
	    target_fso->SetRcRights(RC_STATUS | RC_DATA);
	if (ASYNCCOP2) target_fso->ReturnEarly();

	/* Send the COP2 message or add an entry for piggybacking. */
        vp->COP2(m, &sid, &UpdateSet);

RepExit:
	if (m) m->Put();
	switch(code) {
	    case 0:
		if (asy_resolve) {
		    vp->ResSubmit(0, &fid);
		    if (target_fso != 0)
			vp->ResSubmit(0, &target_fso->fid);
		}
		break;

	    case ETIMEDOUT:
	    case ESYNRESOLVE:
	    case EINCONS:
		code = ERETRY;
		break;

	    default:
		break;
	}
    }
    else {
	/* Acquire a Connection. */
	connent *c;
	ViceStoreId Dummy;                 /* Need an address for ViceMakeDir */
        volrep *vp = (volrep *)vol;
	code = vp->GetConn(&c, uid);
	if (code != 0) goto NonRepExit;

	/* Make the RPC call. */
	long cbtemp; cbtemp = cbbreaks;
	CFSOP_PRELUDE("store::Mkdir %-30s\n", name, NullFid);
	UNI_START_MESSAGE(ViceVMakeDir_OP);
	code = (int) ViceVMakeDir(c->connid, MakeViceFid(&fid), (RPC2_String)
				  name, &target_status,
				  MakeViceFid(&target_fid), &parent_status, 0,
				  &Dummy, &OldVS, &VS, &VCBStatus, &PiggyBS);
	UNI_END_MESSAGE(ViceVMakeDir_OP);
	CFSOP_POSTLUDE("store::mkdir done\n");

	/* Examine the return code to decide what to do next. */
	code = vp->Collate(c, code);
	UNI_RECORD_STATS(ViceVMakeDir_OP);
	if (code != 0) goto NonRepExit;

	/* Allocate the fsobj. */
	target_fso = FSDB->Create(&target_fid, target_pri, name);
	if (target_fso == 0) {
	    UpdateCacheStats(&FSDB->DirAttrStats, NOSPACE,
			     NBLOCKS(sizeof(fsobj)));
	    code = ENOSPC;
	    goto NonRepExit;
	}
	UpdateCacheStats(&FSDB->DirAttrStats, CREATE,
			 NBLOCKS(sizeof(fsobj)));

	/* Do Mkdir locally. */
	Recov_BeginTrans();
	LocalMkdir(Mtime, target_fso, name, uid, Mode);
	UpdateStatus(&parent_status, NULL, uid);
	target_fso->UpdateStatus(&target_status, NULL, uid);
	Recov_EndTrans(CMFP);
	if (target_status.CallBack == CallBackSet && cbtemp == cbbreaks)
	    target_fso->SetRcRights(RC_STATUS | RC_DATA);

NonRepExit:
	PutConn(&c);
    }

    if (code == 0) {
	*t_fso_addr = target_fso;
    }
    else {
	if (target_fso != 0) {
	    FSO_ASSERT(target_fso, !HAVESTATUS(target_fso));
	    Recov_BeginTrans();
	    target_fso->Kill();
	    Recov_EndTrans(DMFP);
	    FSDB->Put(&target_fso);
	}
    }
    return(code);
}


/* local-repair modification */
int fsobj::DisconnectedMkdir(Date_t Mtime, uid_t uid, fsobj **t_fso_addr, char *name, 
			     unsigned short Mode, int target_pri, int Tid)
{
    FSO_ASSERT(this, (EMULATING(this) || LOGGING(this)));

    int code = 0;
    fsobj *target_fso = 0;
    VenusFid target_fid;
    RPC2_Unsigned AllocHost = 0;

    if (!vol->IsReplicated()) {
	code = ETIMEDOUT;
	goto Exit;
    }
    
    /* Allocate a fid for the new object. */
    /* if we time out, return so we will try again with a local fid. */
    code = ((repvol *)vol)->AllocFid(Directory, &target_fid, &AllocHost, uid);
    if (code != 0) goto Exit;

    /* Allocate the fsobj. */
    target_fso = FSDB->Create(&target_fid, target_pri, name);
    if (target_fso == 0) {
	UpdateCacheStats(&FSDB->DirAttrStats, NOSPACE,
			 NBLOCKS(sizeof(fsobj)));
	code = ENOSPC;
	goto Exit;
    }
    UpdateCacheStats(&FSDB->DirAttrStats, CREATE,
		      NBLOCKS(sizeof(fsobj)));

    Recov_BeginTrans();
    code = ((repvol *)vol)->LogMkdir(Mtime, uid, &fid, name,
                                     &target_fso->fid, Mode, Tid);

    if (code == 0) {
	    /* This MUST update second-class state! */
	    LocalMkdir(Mtime, target_fso, name, uid, Mode);

	    /* target_fso->stat is not initialized until LocalMkdir */
	    RVMLIB_REC_OBJECT(target_fso->CleanStat);
	    target_fso->CleanStat.Length = target_fso->stat.Length;
	    target_fso->CleanStat.Date = target_fso->stat.Date;
	   }
    Recov_EndTrans(DMFP);

Exit:
    if (code == 0) {
	*t_fso_addr = target_fso;
    }
    else {
	if (target_fso != 0) {
	    FSO_ASSERT(target_fso, !HAVESTATUS(target_fso));
	    Recov_BeginTrans();
	    target_fso->Kill();
	    Recov_EndTrans(DMFP);
	    FSDB->Put(&target_fso);
	}
    }
    return(code);
}


/* local-repair modification */
/* Returns target object write-locked (on success). */
int fsobj::Mkdir(char *name, fsobj **target_fso_addr,
		  uid_t uid, unsigned short Mode, int target_pri)
{
    LOG(10, ("fsobj::Mkdir: (%s, %s, %d), uid = %d\n",
	      GetComp(), name, target_pri, uid));

    int code = 0;
    Date_t Mtime = Vtime();
    *target_fso_addr = 0;

    int conn, tid;
    GetOperationState(&conn, &tid);
    if (conn == 0) {
	code = DisconnectedMkdir(Mtime, uid, target_fso_addr,
				 name, Mode, target_pri, tid);
    }
    else {
	code = ConnectedMkdir(Mtime, uid, target_fso_addr,
			      name, Mode, target_pri);
    }

    if (code != 0) {
	Demote();
    }
    return(code);
}


/*  *****  Rmdir  *****  */

/* MUST be called from within transaction! */
void fsobj::LocalRmdir(Date_t Mtime, char *name, fsobj *target_fso) {
    /* Update parent status. */
    {
	/* Delete the target name from the directory.. */
	dir_Delete(name);

	/* Update the status to reflect the delete. */
	RVMLIB_REC_OBJECT(stat);
	stat.DataVersion++;
	stat.Length = dir_Length();
	stat.Date = Mtime;
	stat.LinkCount--;
    }

    /* Update target status. */
    {
	/* Delete the target object. */
	RVMLIB_REC_OBJECT(target_fso->stat);
	target_fso->stat.LinkCount--;
	target_fso->DetachHdbBindings();
	UpdateCacheStats(&FSDB->DirAttrStats, REMOVE,
			 NBLOCKS(sizeof(fsobj)));
	UpdateCacheStats(&FSDB->DirDataStats, REMOVE,
			 BLOCKS(target_fso));
	target_fso->Kill();
    }
}


int fsobj::ConnectedRmdir(Date_t Mtime, uid_t uid, char *name, fsobj *target_fso) {
    FSO_ASSERT(this, HOARDING(this));

    int code = 0;

    /* Status parameters. */
    ViceStatus parent_status;
    VenusToViceStatus(&stat, &parent_status);
    ViceStatus target_status;
    VenusToViceStatus(&target_fso->stat, &target_status);
    {
	/* Temporary!  Until RPC interface is fixed!  -JJK */
	parent_status.Date = Mtime;
    }

    /* COP2 Piggybacking. */
    char PiggyData[COP2SIZE];
    RPC2_CountedBS PiggyBS;
    PiggyBS.SeqLen = 0;
    PiggyBS.SeqBody = (RPC2_ByteSeq)PiggyData;

    /* VCB Arguments */
    RPC2_Integer VS = 0;
    CallBackStatus VCBStatus = NoCallBack;
    RPC2_CountedBS OldVS; 
    OldVS.SeqLen = 0;
    OldVS.SeqBody = 0;

    if (vol->IsReplicated()) {
	ViceStoreId sid;
	mgrpent *m = 0;
	int asy_resolve = 0;
        repvol *vp = (repvol *)vol;

	/* Acquire an Mgroup. */
	code = vp->GetMgrp(&m, uid, (PIGGYCOP2 ? &PiggyBS : 0));
	if (code != 0) goto RepExit;

	/* The COP1 call. */
	long cbtemp; cbtemp = cbbreaks;
	vv_t UpdateSet;

	Recov_BeginTrans();
	sid = vp->GenerateStoreId();
	Recov_EndTrans(MAXFP);
	{
	    /* Make multiple copies of the IN/OUT and OUT parameters. */
	    int ph_ix; unsigned long ph;
            ph = ntohl(m->GetPrimaryHost(&ph_ix)->s_addr);

 	    vp->PackVS(VSG_MEMBERS, &OldVS);
	    ARG_MARSHALL(IN_OUT_MODE, ViceStatus, parent_statusvar, parent_status, VSG_MEMBERS);
	    ARG_MARSHALL(IN_OUT_MODE, ViceStatus, target_statusvar, target_status, VSG_MEMBERS);
	    ARG_MARSHALL(OUT_MODE, RPC2_Integer, VSvar, VS, VSG_MEMBERS);
	    ARG_MARSHALL(OUT_MODE, CallBackStatus, VCBStatusvar, VCBStatus, VSG_MEMBERS);

	    /* Make the RPC call. */
	    CFSOP_PRELUDE("store::Rmdir %-30s\n", name, target_fso->fid);
	    MULTI_START_MESSAGE(ViceVRemoveDir_OP);
	    code = (int) MRPC_MakeMulti(ViceVRemoveDir_OP, ViceVRemoveDir_PTR,
				  VSG_MEMBERS, m->rocc.handles,
				  m->rocc.retcodes, m->rocc.MIp, 0, 0,
				  MakeViceFid(&fid), name,
				  parent_statusvar_ptrs, target_statusvar_ptrs,
				  ph, &sid, &OldVS, VSvar_ptrs,
				  VCBStatusvar_ptrs, &PiggyBS);
	    MULTI_END_MESSAGE(ViceVRemoveDir_OP);
	    CFSOP_POSTLUDE("store::rmdir done\n");

	    free(OldVS.SeqBody);
	    /* Collate responses from individual servers and decide what to do next. */
	    code = vp->Collate_COP1(m, code, &UpdateSet);
	    MULTI_RECORD_STATS(ViceVRemoveDir_OP);
	    if (code == EASYRESOLVE) { asy_resolve = 1; code = 0; }
	    if (code != 0) goto RepExit;

	    /* Collate volume callback information */
	    if (cbtemp == cbbreaks)
		vp->CollateVCB(m, VSvar_bufs, VCBStatusvar_bufs);

	    /* Finalize COP2 Piggybacking. */
	    if (PIGGYCOP2)
		vp->ClearCOP2(&PiggyBS);

	    /* Manually compute the OUT parameters from the mgrpent::Rmdir() call! -JJK */
	    int dh_ix; dh_ix = -1;
	    (void)m->DHCheck(0, ph_ix, &dh_ix);
	    ARG_UNMARSHALL(parent_statusvar, parent_status, dh_ix);
	    ARG_UNMARSHALL(target_statusvar, target_status, dh_ix);
	}

	/* Do Rmdir locally. */
	Recov_BeginTrans();
	LocalRmdir(Mtime, name, target_fso);
	UpdateStatus(&parent_status, &UpdateSet, uid);
	target_fso->UpdateStatus(&target_status, &UpdateSet, uid);
	Recov_EndTrans(CMFP);
	if (ASYNCCOP2) ReturnEarly();

	/* Send the COP2 message or add an entry for piggybacking. */
        vp->COP2(m, &sid, &UpdateSet);

RepExit:
	if (m) m->Put();
	switch(code) {
	    case 0:
		if (asy_resolve) {
		    vp->ResSubmit(0, &fid);
		    vp->ResSubmit(0, &target_fso->fid);
		}
		break;

	    case ETIMEDOUT:
	    case ESYNRESOLVE:
	    case EINCONS:
		code = ERETRY;
		break;

	    default:
		break;
	}
    }
    else {
	/* Acquire a Connection. */
	connent *c;
	ViceStoreId Dummy;               /* Need an address for ViceRemoveDir */
        volrep *vp = (volrep *)vol;
	code = vp->GetConn(&c, uid);
	if (code != 0) goto NonRepExit;

	/* Make the RPC call. */
	CFSOP_PRELUDE("store::Rmdir %-30s\n", name, target_fso->fid);
	UNI_START_MESSAGE(ViceVRemoveDir_OP);
	code = (int) ViceVRemoveDir(c->connid, MakeViceFid(&fid),
				    (RPC2_String)name, &parent_status,
				    &target_status, 0, &Dummy, &OldVS, &VS,
				    &VCBStatus, &PiggyBS);
	UNI_END_MESSAGE(ViceVRemoveDir_OP);
	CFSOP_POSTLUDE("store::rmdir done\n");

	/* Examine the return code to decide what to do next. */
	code = vp->Collate(c, code);
	UNI_RECORD_STATS(ViceVRemoveDir_OP);
	if (code != 0) goto NonRepExit;

	/* Do Rmdir locally. */
	Recov_BeginTrans();
	LocalRmdir(Mtime, name, target_fso);
	UpdateStatus(&parent_status, NULL, uid);
	target_fso->UpdateStatus(&target_status, NULL, uid);
	Recov_EndTrans(CMFP);

NonRepExit:
	PutConn(&c);
    }

    return(code);
}


/* local-repair modification */
int fsobj::DisconnectedRmdir(Date_t Mtime, uid_t uid, char *name, fsobj *target_fso, int Tid) {
    FSO_ASSERT(this, (EMULATING(this) || LOGGING(this)));

    int code = 0;

    if (!vol->IsReplicated()) {
	code = ETIMEDOUT;
	goto Exit;
    }

    Recov_BeginTrans();
    code = ((repvol *)vol)->LogRmdir(Mtime, uid, &fid, name,
                                     &target_fso->fid, Tid);

    if (code == 0)
	    /* This MUST update second-class state! */
	    LocalRmdir(Mtime, name, target_fso);
    Recov_EndTrans(DMFP);

Exit:
    return(code);
}


/* local-repair modification */
int fsobj::Rmdir(char *name, fsobj *target_fso, uid_t uid)
{
    LOG(10, ("fsobj::Rmdir: (%s, %s), uid = %d\n", GetComp(), name, uid));

    int code = 0;
    Date_t Mtime = Vtime();

    int conn, tid;
    GetOperationState(&conn, &tid);
    if (conn == 0) {
	code = DisconnectedRmdir(Mtime, uid, name, target_fso, tid);
    }
    else {
	code = ConnectedRmdir(Mtime, uid, name, target_fso);
    }

    if (code != 0) {
	Demote();
	target_fso->Demote();
    }
    return(code);
}


/*  *****  Symlink  *****  */

/* MUST be called from within transaction! */
void fsobj::LocalSymlink(Date_t Mtime, fsobj *target_fso, char *name,
			  char *contents, uid_t Owner, unsigned short Mode)
{
    /* Update parent status. */
    {
	/* Add the new <name, fid> to the directory. */
	dir_Create(name, &target_fso->fid);

	/* Update the status to reflect the create. */
	RVMLIB_REC_OBJECT(stat);
	stat.DataVersion++;
	stat.Length = dir_Length();
	stat.Date = Mtime;
    }

    /* Set target status. */
    {
	/* Initialize the target fsobj. */
	RVMLIB_REC_OBJECT(*target_fso);
	target_fso->stat.VnodeType = SymbolicLink;
	target_fso->stat.LinkCount = 1;
	target_fso->stat.Length = 0;
	target_fso->stat.DataVersion = 1;
	target_fso->stat.Date = Mtime;
	target_fso->stat.Owner = Owner;
	target_fso->stat.Mode = Mode;
	target_fso->Matriculate();
	target_fso->SetParent(fid.Vnode, fid.Unique);

	/* Write out the link contents. */
	int linklen = (int) strlen(contents);
	target_fso->stat.Length = linklen;
	target_fso->data.symlink = (char *)rvmlib_rec_malloc(linklen + 1);
	rvmlib_set_range(target_fso->data.symlink, linklen);
	memcpy(target_fso->data.symlink, contents, linklen);
	UpdateCacheStats(&FSDB->FileDataStats, CREATE,
			 NBLOCKS(linklen));

	target_fso->Reference();
	target_fso->ComputePriority();
    }
}


int fsobj::ConnectedSymlink(Date_t Mtime, uid_t uid, fsobj **t_fso_addr,
			     char *name, char *contents,
			     unsigned short Mode, int target_pri)
{
    FSO_ASSERT(this, HOARDING(this));

    int code = 0;
    fsobj *target_fso = 0;
    VenusFid target_fid = NullFid;
    RPC2_Unsigned AllocHost = 0;

    /* Status parameters. */
    ViceStatus parent_status;
    VenusToViceStatus(&stat, &parent_status);
    ViceStatus target_status;
    target_status.Mode = Mode;
    {
	/* Temporary!  Until RPC interface is fixed!  -JJK */
	parent_status.Date = Mtime;
	target_status.DataVersion = 1;
	target_status.VV = NullVV;
    }

    /* COP2 Piggybacking. */
    char PiggyData[COP2SIZE];
    RPC2_CountedBS PiggyBS;
    PiggyBS.SeqLen = 0;
    PiggyBS.SeqBody = (RPC2_ByteSeq)PiggyData;

    /* VCB Arguments */
    RPC2_Integer VS = 0;
    CallBackStatus VCBStatus = NoCallBack;
    RPC2_CountedBS OldVS; 
    OldVS.SeqLen = 0;
    OldVS.SeqBody = 0;

    if (vol->IsReplicated()) {
	ViceStoreId sid;
	mgrpent *m = 0;
	int asy_resolve = 0;
        repvol *vp = (repvol *)vol;

	/* Allocate a fid for the new object. */
	code = vp->AllocFid(SymbolicLink, &target_fid, &AllocHost, uid);
	if (code != 0) goto RepExit;

	/* Allocate the fsobj. */
	target_fso = FSDB->Create(&target_fid, target_pri, name);
	if (target_fso == 0) {
	    UpdateCacheStats(&FSDB->FileAttrStats, NOSPACE,
			     NBLOCKS(sizeof(fsobj)));
	    code = ENOSPC;
	    goto RepExit;
	}
	UpdateCacheStats(&FSDB->FileAttrStats, CREATE,
			 NBLOCKS(sizeof(fsobj)));

	/* Acquire an Mgroup. */
	code = vp->GetMgrp(&m, uid, (PIGGYCOP2 ? &PiggyBS : 0));
	if (code != 0) goto RepExit;

	/* The COP1 call. */
	long cbtemp; cbtemp = cbbreaks;
	vv_t UpdateSet;

	Recov_BeginTrans();
	sid = vp->GenerateStoreId();
	Recov_EndTrans(MAXFP);
	{
	    ViceFid target = *MakeViceFid(&target_fid);

	    /* Make multiple copies of the IN/OUT and OUT parameters. */
 	    vp->PackVS(VSG_MEMBERS, &OldVS);
	    ARG_MARSHALL(IN_OUT_MODE, ViceFid, targetvar, target, VSG_MEMBERS);
	    ARG_MARSHALL(IN_OUT_MODE, ViceStatus, target_statusvar, target_status, VSG_MEMBERS);
	    ARG_MARSHALL(IN_OUT_MODE, ViceStatus, parent_statusvar, parent_status, VSG_MEMBERS);
	    ARG_MARSHALL(OUT_MODE, RPC2_Integer, VSvar, VS, VSG_MEMBERS);
	    ARG_MARSHALL(OUT_MODE, CallBackStatus, VCBStatusvar, VCBStatus, VSG_MEMBERS);

	    /* Make the RPC call. */
	    CFSOP_PRELUDE("store::Symlink %-30s\n", contents, NullFid);
	    MULTI_START_MESSAGE(ViceVSymLink_OP);
	    code = (int) MRPC_MakeMulti(ViceVSymLink_OP, ViceVSymLink_PTR,
				  VSG_MEMBERS, m->rocc.handles,
				  m->rocc.retcodes, m->rocc.MIp, 0, 0,
				  MakeViceFid(&fid), name, contents,
				  targetvar_ptrs, target_statusvar_ptrs,
				  parent_statusvar_ptrs, AllocHost, &sid,
				  &OldVS, VSvar_ptrs, VCBStatusvar_ptrs, &PiggyBS);
	    MULTI_END_MESSAGE(ViceVSymLink_OP);
	    CFSOP_POSTLUDE("store::symlink done\n");

	    free(OldVS.SeqBody);
	    /* Collate responses from individual servers and decide what to do next. */
	    code = vp->Collate_COP1(m, code, &UpdateSet);
	    MULTI_RECORD_STATS(ViceVSymLink_OP);
	    if (code == EASYRESOLVE) { asy_resolve = 1; code = 0; }
	    if (code != 0) goto RepExit;

	    /* Collate volume callback information */
	    if (cbtemp == cbbreaks)
		vp->CollateVCB(m, VSvar_bufs, VCBStatusvar_bufs);

	    /* Finalize COP2 Piggybacking. */
	    if (PIGGYCOP2)
		vp->ClearCOP2(&PiggyBS);

	    /* Manually compute the OUT parameters from the mgrpent::Mkdir() call! -JJK */
	    int dh_ix; dh_ix = -1;
	    (void)m->DHCheck(0, -1, &dh_ix);
	    ARG_UNMARSHALL(target_statusvar, target_status, dh_ix);
	    ARG_UNMARSHALL(targetvar, target, dh_ix);
	    ARG_UNMARSHALL(parent_statusvar, parent_status, dh_ix);
	    MakeVenusFid(&target_fid, target_fid.Realm, &target);
	}

	/* Do Symlink locally. */
	Recov_BeginTrans();
	LocalSymlink(Mtime, target_fso, name, contents, uid, Mode);
	UpdateStatus(&parent_status, &UpdateSet, uid);
	target_fso->UpdateStatus(&target_status, &UpdateSet, uid);
	Recov_EndTrans(CMFP);
	if (target_status.CallBack == CallBackSet && cbtemp == cbbreaks)
	    target_fso->SetRcRights(RC_STATUS | RC_DATA);
	if (ASYNCCOP2) target_fso->ReturnEarly();

	/* Send the COP2 message or add an entry for piggybacking. */
        vp->COP2(m, &sid, &UpdateSet);

RepExit:
	if (m) m->Put();
	switch(code) {
	    case 0:
		if (asy_resolve) {
		    vp->ResSubmit(0, &fid);
		    if (target_fso != 0)
			vp->ResSubmit(0, &target_fso->fid);
		}
		break;

	    case ETIMEDOUT:
	    case ESYNRESOLVE:
	    case EINCONS:
		code = ERETRY;
		break;

	    default:
		break;
	}
    }
    else {
	/* Acquire a Connection. */
	connent *c;
	ViceStoreId Dummy;               /* Need an address for ViceSymLink */
        volrep *vp = (volrep *)vol;
	code = vp->GetConn(&c, uid);
	if (code != 0) goto NonRepExit;

	/* Make the RPC call. */
	long cbtemp; cbtemp = cbbreaks;
	CFSOP_PRELUDE("store::Symlink %-30s\n", contents, NullFid);
	UNI_START_MESSAGE(ViceVSymLink_OP);
	code = (int) ViceVSymLink(c->connid, MakeViceFid(&fid),
				  (RPC2_String)name, (RPC2_String)contents,
				  MakeViceFid(&target_fid), &target_status,
				  &parent_status, 0, &Dummy, &OldVS, &VS,
				  &VCBStatus, &PiggyBS);
	UNI_END_MESSAGE(ViceVSymLink_OP);
	CFSOP_POSTLUDE("store::symlink done\n");

	/* Examine the return code to decide what to do next. */
	code = vp->Collate(c, code);
	UNI_RECORD_STATS(ViceVSymLink_OP);
	if (code != 0) goto NonRepExit;

	/* Allocate the fsobj. */
	target_fso = FSDB->Create(&target_fid, target_pri, name);
	if (target_fso == 0) {
	    UpdateCacheStats(&FSDB->FileAttrStats, NOSPACE,
			     NBLOCKS(sizeof(fsobj)));
	    code = ENOSPC;
	    goto NonRepExit;
	}
	UpdateCacheStats(&FSDB->FileAttrStats, CREATE,
			 NBLOCKS(sizeof(fsobj)));

	/* Do Symlink locally. */
	Recov_BeginTrans();
	LocalSymlink(Mtime, target_fso, name, contents, uid, Mode);
	UpdateStatus(&parent_status, NULL, uid);
	target_fso->UpdateStatus(&target_status, NULL, uid);
	Recov_EndTrans(CMFP);
	if (target_status.CallBack == CallBackSet && cbtemp == cbbreaks)
	    target_fso->SetRcRights(RC_STATUS | RC_DATA);

NonRepExit:
	PutConn(&c);
    }

    if (code == 0) {
	*t_fso_addr = target_fso;
    }
    else {
	if (target_fso != 0) {
	    FSO_ASSERT(target_fso, !HAVESTATUS(target_fso));
	    Recov_BeginTrans();
	    target_fso->Kill();
	    Recov_EndTrans(DMFP);
	    FSDB->Put(&target_fso);
	}
    }
    return(code);
}


/* local-repair modification */
int fsobj::DisconnectedSymlink(Date_t Mtime, uid_t uid, fsobj **t_fso_addr,
			       char *name, char *contents, unsigned short Mode, 
			       int target_pri, int Tid)
{
    FSO_ASSERT(this, (EMULATING(this) || LOGGING(this)));

    int code = 0;
    fsobj *target_fso = 0;
    VenusFid target_fid = NullFid;
    RPC2_Unsigned AllocHost = 0;

    if (!vol->IsReplicated()) {
	code = ETIMEDOUT;
	goto Exit;
    }
    
    /* Allocate a fid for the new object. */
    /* if we time out, return so we will try again with a local fid. */
    code = ((repvol *)vol)->AllocFid(SymbolicLink, &target_fid, &AllocHost, uid);
    if (code != 0) goto Exit;

    /* Allocate the fsobj. */
    target_fso = FSDB->Create(&target_fid, target_pri, name);
    if (target_fso == 0) {
	UpdateCacheStats(&FSDB->FileAttrStats, NOSPACE,
			 NBLOCKS(sizeof(fsobj)));
	code = ENOSPC;
	goto Exit;
    }
    UpdateCacheStats(&FSDB->FileAttrStats, CREATE,
		      NBLOCKS(sizeof(fsobj)));

    Recov_BeginTrans();
    code = ((repvol *)vol)->LogSymlink(Mtime, uid, &fid, name, contents,
                                       &target_fso->fid, Mode, Tid);
    
    if (code == 0) {
	    /* This MUST update second-class state! */
	    LocalSymlink(Mtime, target_fso, name, contents, uid, Mode);

	    /* target_fso->stat is not initialized until LocalSymlink */
	    RVMLIB_REC_OBJECT(target_fso->CleanStat);
	    target_fso->CleanStat.Length = target_fso->stat.Length;
	    target_fso->CleanStat.Date = target_fso->stat.Date;
	   }
    Recov_EndTrans(DMFP);

Exit:
    if (code == 0) {
	*t_fso_addr = target_fso;
    }
    else {
	if (target_fso != 0) {
	    FSO_ASSERT(target_fso, !HAVESTATUS(target_fso));
	    Recov_BeginTrans();
	    target_fso->Kill();
	    Recov_EndTrans(DMFP);
	    FSDB->Put(&target_fso);
	}
    }
    return(code);
}


/* local-repair modification */
int fsobj::Symlink(char *s_name, char *t_name, uid_t uid, unsigned short Mode,
		   int target_pri)
{
    LOG(10, ("fsobj::Symlink: (%s, %s, %s, %d), uid = %d\n",
	      GetComp(), s_name, t_name, target_pri, uid));

    int code = 0;
    Date_t Mtime = Vtime();
    fsobj *target_fso = 0;

    int conn, tid;
    GetOperationState(&conn, &tid);
    if (conn == 0) {
	code = DisconnectedSymlink(Mtime, uid, &target_fso, t_name, s_name,
				   Mode, target_pri, tid);
    }
    else {
	code = ConnectedSymlink(Mtime, uid, &target_fso, t_name, s_name, Mode,
				target_pri);
    }

    if (code == 0) {
	/* Target is NOT an OUT parameter. */
	FSDB->Put(&target_fso);
    }
    else {
	Demote();
    }
    return(code);
}


/*  *****  SetVV  *****  */

/* This should eventually disappear to be a side-effect of the Repair call! -JJK */
/* Call with object write-locked. */
int fsobj::SetVV(ViceVersionVector *newvv, uid_t uid)
{
    LOG(10, ("fsobj::SetVV: (%s), uid = %d\n",
	      GetComp(), uid));

    int code = 0;

    if (EMULATING(this) || LOGGING(this)) {
	/* This is a connected-mode only routine! */
	code = ETIMEDOUT;
    }
    else {
	FSO_ASSERT(this, HOARDING(this));

	/* COP2 Piggybacking. */
	char PiggyData[COP2SIZE];
	RPC2_CountedBS PiggyBS;
	PiggyBS.SeqLen = 0;
	PiggyBS.SeqBody = (RPC2_ByteSeq)PiggyData;

	if (vol->IsReplicated()) {
	    mgrpent *m = 0;
            repvol *vp = (repvol *)vol;

	    /* Acquire an Mgroup. */
	    code = vp->GetMgrp(&m, uid, (PIGGYCOP2 ? &PiggyBS : 0));
	    if (code != 0) goto RepExit;

	    /* The SetVV call. */
	    {
		/* Make the RPC call. */
		CFSOP_PRELUDE("store::SetVV %-30s\n", comp, fid);
		MULTI_START_MESSAGE(ViceSetVV_OP);
		code = (int) MRPC_MakeMulti(ViceSetVV_OP, ViceSetVV_PTR,
				      VSG_MEMBERS, m->rocc.handles,
				      m->rocc.retcodes, m->rocc.MIp, 0, 0,
				      MakeViceFid(&fid), newvv, &PiggyBS);
		MULTI_END_MESSAGE(ViceSetVV_OP);
		CFSOP_POSTLUDE("store::setvv done\n");

		/* Collate responses from individual servers and decide what to do next. */
		code = vp->Collate_COP2(m, code);
		MULTI_RECORD_STATS(ViceSetVV_OP);
		if (code != 0) goto RepExit;

		/* Finalize COP2 Piggybacking. */
		if (PIGGYCOP2)
		    vp->ClearCOP2(&PiggyBS);
	    }

	    /* Do op locally. */
	    Recov_BeginTrans();
	    RVMLIB_REC_OBJECT(stat);
            stat.VV = *newvv;
            Recov_EndTrans(CMFP);

RepExit:
            if (m) m->Put();
            switch(code) {
            case 0:
                break;

            case ETIMEDOUT:
                code = ERETRY;
                break;

            case ESYNRESOLVE:
            case EINCONS:
                CHOKE("fsobj::SetVV: code = %d", code);
                break;

            default:
                break;
            }
        }
	else {
	    /* Acquire a Connection. */
	    connent *c;
            volrep *vp = (volrep *)vol;
	    code = vp->GetConn(&c, uid);
	    if (code != 0) goto NonRepExit;

	    /* Make the RPC call. */
	    CFSOP_PRELUDE("store::SetVV %-30s\n", comp, fid);
	    UNI_START_MESSAGE(ViceSetVV_OP);
	    code = (int) ViceSetVV(c->connid, MakeViceFid(&fid), newvv, &PiggyBS);
	    UNI_END_MESSAGE(ViceSetVV_OP);
	    CFSOP_POSTLUDE("store::setvv done\n");

	    /* Examine the return code to decide what to do next. */
	    code = vp->Collate(c, code);
	    UNI_RECORD_STATS(ViceSetVV_OP);
	    if (code != 0) goto NonRepExit;

	    /* Do op locally. */
	    Recov_BeginTrans();
	    RVMLIB_REC_OBJECT(stat);
	    stat.VV = *newvv;
	    Recov_EndTrans(CMFP);

NonRepExit:
	    PutConn(&c);
	}
    }

    /* Replica control rights are invalid in any case. */
    Demote();

  LOG(0, ("MARIA:  We just SetVV'd.\n"));

    return(code);
}
