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
                           none currently

#*/

/* this file contains local-repair related fsobj methods */

#ifdef __cplusplus
extern "C" {
#endif
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
#endif



/* from venus */
#include "fso.h"
#include "local.h"
#include "mgrp.h"
#include "venuscb.h"
#include "venusrecov.h"
#include "venus.private.h"
#include "venusvol.h"
#include "worker.h"

/* MUST be called from within a transaction */
void fsobj::SetComp(char *name)
{
    RVMLIB_REC_OBJECT(comp);
    if (comp) rvmlib_rec_free(comp);
    if (name && name[0] != '\0')
	 comp = rvmlib_rec_strdup(name);
    else comp = rvmlib_rec_strdup("");
}

const char *fsobj::GetComp(void)
{
    if (comp && comp[0] != '\0')
	 return comp;
    else return FID_(&fid);
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

/* need not be called from within a transaction */
cmlent *fsobj::FinalCmlent(int tid)
{
    /* return the last cmlent done by iot tid */
    LOG(100, ("fsobj::FinalCmlent: %s\n", FID_(&fid)));
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

int fsobj::RepairStore()
{
    /* same as ConnectedStore without perform simulation disconnection */
    FSO_ASSERT(this, HOARDING(this) || LOGGING(this));

    vproc *vp = VprocSelf();
    Date_t Mtime = Vtime();
    unsigned long NewLength = stat.Length;


#if 0 /*doesn't apply anymore */
    if (LRDB->repair_session_mode == REP_SCRATCH_MODE) {
	return DisconnectedStore(Mtime, vp->u.u_uid, NewLength, LRDB->repair_session_tid);
    }
#endif
    int code = 0, fd = -1;
    char prel_str[256];
    sprintf(prel_str, "store::Store %%s [%ld]\n", NBLOCKS(NewLength));

    /* Status parameters. */
    ViceStatus status;
    VenusToViceStatus(&stat, &status);
    {
	/* Temporary!  Until RPC interface is fixed!  -JJK */
	status.Date = Mtime;
    }

    LOG(0, ("fsobj::RepairStore: (%s)\n\tVV->StoreId.Host:%d  VV->StoreId.Uniquifier:%d\n", FID_(&fid), status.VV.StoreId.Host, status.VV.StoreId.Uniquifier));

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
        fd = data.file->Open(O_RDONLY);

        sei->Tag = FILEBYFD;
        sei->FileInfo.ByFD.fd = fd;
    }

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
        repvol *rvp = (repvol *)vol;

	/* Acquire an Mgroup. */
	code = rvp->GetMgrp(&m, vp->u.u_uid, (PIGGYCOP2 ? &PiggyBS : 0));
	if (code != 0) goto RepExit;

	/* The COP1 call. */
	long cbtemp; cbtemp = cbbreaks;
	vv_t UpdateSet;

	Recov_BeginTrans();
	sid = rvp->GenerateStoreId();
	Recov_EndTrans(MAXFP);
	{
	    /* Make multiple copies of the IN/OUT and OUT parameters. */
	    int ph_ix; unsigned long ph;
            ph = ntohl(m->GetPrimaryHost(&ph_ix)->s_addr);

	    rvp->PackVS(VSG_MEMBERS, &OldVS);

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
				       MakeViceFid(&fid), statusvar_ptrs,
				       NewLength, ph, &sid, &OldVS, VSvar_ptrs,
				       VCBStatusvar_ptrs, &PiggyBS,
				       sedvar_bufs);
	    MULTI_END_MESSAGE(ViceStore_OP);
	    CFSOP_POSTLUDE("store::store done\n");

	    free(OldVS.SeqBody);
	    /* Collate responses from individual servers and decide what to do next. */
	    code = rvp->Collate_COP1(m, code, &UpdateSet);
	    MULTI_RECORD_STATS(ViceStore_OP);
	    if (code == EASYRESOLVE) { asy_resolve = 1; code = 0; }
	    if (code != 0) {
	      LOG(0, ("fsobj::RepairStore: MRPC_MakeMulti failed! (%s)\n", FID_(&fid)));
	      goto RepExit;
	    }

	    /* Collate volume callback information */
	    if (cbtemp == cbbreaks)
		rvp->CollateVCB(m, VSvar_bufs, VCBStatusvar_bufs);

	    /* Finalize COP2 Piggybacking. */
	    if (PIGGYCOP2)
		rvp->ClearCOP2(&PiggyBS);

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
	UpdateStatus(&status, &UpdateSet, vp->u.u_uid);
	Recov_EndTrans(CMFP);
	if (ASYNCCOP2) ReturnEarly();

	/* Send the COP2 message or add an entry for piggybacking. */
        rvp->COP2(m, &sid, &UpdateSet);

RepExit:
        if (m) m->Put();
	switch(code) {
	    case 0:
		if (asy_resolve)
		    rvp->ResSubmit(0, &fid);
		break;

	    default:
	      LOG(0, ("fsobj::RepairStore: (%s) failed! code %d\n",
		      FID_(&fid), code));

	      break;
	}
    }
    else {
      LOG(0, ("fsobj::RepairStore: volrep\n", FID_(&fid)));

	/* Acquire a Connection. */
	connent *c;
	ViceStoreId Dummy;      /* ViceStore needs an address for indirection */
        volrep *rvp = (volrep *)vol;
	code = rvp->GetConn(&c, vp->u.u_uid);
	if (code != 0) {
	  LOG(0, ("fsobj::RepairStore: rvp->GetConn failed (%d)\n", code));
	  goto NonRepExit;
	}

	/* Make the RPC call. */
	CFSOP_PRELUDE(prel_str, comp, fid);
	UNI_START_MESSAGE(ViceStore_OP);
	code = (int) ViceStore(c->connid, MakeViceFid(&fid), &status,
			       NewLength, 0, &Dummy, &OldVS, &VS, &VCBStatus,
			       &PiggyBS, sed);
	UNI_END_MESSAGE(ViceStore_OP);
	CFSOP_POSTLUDE("store::store done\n");

	/* Examine the return code to decide what to do next. */
	code = rvp->Collate(c, code);
	UNI_RECORD_STATS(ViceStore_OP);
	if (code != 0) {
	  LOG(0, ("fsobj::RepairStore: rvp->Collate failed (%d)\n", code));
	  goto NonRepExit;
	}

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
	UpdateStatus(&status, NULL, vp->u.u_uid);
	Recov_EndTrans(CMFP);

NonRepExit:
	PutConn(&c);
    }

    data.file->Close(fd);

    return(code);
}

int fsobj::RepairSetAttr(unsigned long NewLength, Date_t NewDate,
			 uid_t NewOwner, unsigned short NewMode,
			 RPC2_CountedBS *acl)
{
    Date_t Mtime = Vtime();
    vproc *vp = VprocSelf();
#if 0
    if (LRDB->repair_session_mode == REP_SCRATCH_MODE)
      return DisconnectedSetAttr(Mtime, vp->u.u_uid, NewLength, NewDate, NewOwner, NewMode,
				 LRDB->repair_session_tid);
    else
#endif
      return ConnectedSetAttr(Mtime, vp->u.u_uid, NewLength, NewDate, NewOwner, NewMode, acl);
}

int fsobj::RepairCreate(fsobj **t_fso_addr, char *name, unsigned short Mode, int target_pri)
{
    Date_t Mtime = Vtime();
    vproc *vp = VprocSelf();
#if 0
    if (LRDB->repair_session_mode == REP_SCRATCH_MODE)
      return DisconnectedCreate(Mtime, vp->u.u_uid, t_fso_addr, name, Mode, target_pri,
				LRDB->repair_session_tid);
    else
#endif
      return ConnectedCreate(Mtime, vp->u.u_uid, t_fso_addr, name, Mode, target_pri);
}

int fsobj::RepairRemove(char *name, fsobj *target_fso) {
    Date_t Mtime = Vtime();
    vproc *vp = VprocSelf();
#if 0
    if (LRDB->repair_session_mode == REP_SCRATCH_MODE)
      return DisconnectedRemove(Mtime, vp->u.u_uid, name, target_fso, LRDB->repair_session_tid);
    else
#endif
      return ConnectedRemove(Mtime, vp->u.u_uid, name, target_fso);
}

int fsobj::RepairLink(char *name, fsobj *source_fso) {
    Date_t Mtime = Vtime();
    vproc *vp = VprocSelf();
#if 0
    if (LRDB->repair_session_mode == REP_SCRATCH_MODE)
      return DisconnectedLink(Mtime, vp->u.u_uid, name, source_fso, LRDB->repair_session_tid);
    else
#endif
      return ConnectedLink(Mtime, vp->u.u_uid, name, source_fso);
}

int fsobj::RepairRename(fsobj *s_parent_fso, char *s_name, fsobj *s_fso, char *t_name, fsobj *t_fso)
{
    Date_t Mtime = Vtime();
    vproc *vp = VprocSelf();
#if 0
    if (LRDB->repair_session_mode == REP_SCRATCH_MODE)
      return DisconnectedRename(Mtime, vp->u.u_uid, s_parent_fso, s_name, s_fso, t_name, t_fso,
				LRDB->repair_session_tid);
    else
#endif
      return ConnectedRename(Mtime, vp->u.u_uid, s_parent_fso, s_name, s_fso, t_name, t_fso);
}


int fsobj::RepairMkdir(fsobj **t_fso_addr, char *name, unsigned short Mode, int target_pri)
{
    Date_t Mtime = Vtime();
    vproc *vp = VprocSelf();
#if 0
    if (LRDB->repair_session_mode == REP_SCRATCH_MODE)
      return DisconnectedMkdir(Mtime, vp->u.u_uid, t_fso_addr, name, Mode, target_pri,
			       LRDB->repair_session_tid);
    else
#endif
      return ConnectedMkdir(Mtime, vp->u.u_uid, t_fso_addr, name, Mode, target_pri);
}

int fsobj::RepairRmdir(char *name, fsobj *target_fso)
{
    Date_t Mtime = Vtime();
    vproc *vp = VprocSelf();
#if 0
    if (LRDB->repair_session_mode == REP_SCRATCH_MODE)
      return DisconnectedRmdir(Mtime, vp->u.u_uid, name, target_fso, LRDB->repair_session_tid);
    else
#endif
      return ConnectedRmdir(Mtime, vp->u.u_uid, name, target_fso);
}

int fsobj::RepairSymlink(fsobj **t_fso_addr, char *name, char *contents,
			    unsigned short Mode, int target_pri)
{
    Date_t Mtime = Vtime();
    vproc *vp = VprocSelf();
#if 0
    if (LRDB->repair_session_mode == REP_SCRATCH_MODE)
      return DisconnectedSymlink(Mtime, vp->u.u_uid, t_fso_addr, name, contents, Mode, target_pri,
				 LRDB->repair_session_tid);
    else
#endif
      return ConnectedSymlink(Mtime, vp->u.u_uid, t_fso_addr, name, contents, Mode, target_pri);
}

/*  *****  SetLocalVV  *****  */

/* Used by Repair routines when sending local information to the servers! */
/* Call with object write-locked? */
int fsobj::SetLocalVV(ViceVersionVector *newvv)
{
    LOG(0, ("fsobj::SetLocalVV: (%s)\n", GetComp()));

    FSO_ASSERT(this, newvv != NULL);

    /* Do op locally. */
    Recov_BeginTrans();
    RVMLIB_REC_OBJECT(stat);
    stat.VV = *newvv;
    Recov_EndTrans(CMFP);

    return(0);
}
