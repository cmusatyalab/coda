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







/* * Bugs: 1. Currently, the binding of volent <--> VSG is static
 *       (i.e., fixed at construction of the volent).  We need a
 *       mechanism for changing this binding over time, due to:
 *
 *          - commencement of a new "epoch" for a REPVOL (i.e.,
 *            adding/deleting a replica)  
 *          - movement of a {RWR,RO} volume from one host to another
 *          - addition/deletion of a host from a RO replica suite
 *       2. There is a horrible hack involving the derivation of VSGs
 *             for ROVOLs.  This can be fixed with some modification to
 * 	      the ViceGetVolumeInfo call. 
 */


/*
 *
 *    Implementation of the Venus Volume abstraction.
 *
 *
 *    Each volume is always in one of five states.  These states, and
 *    the next state table are: (there are some caveats, which are
 *    discussed prior to the TakeTransition function)
 *
 *    Hoarding	(H)	(|AVSG| > 0) ? (logv ? L : (CML_Count > 0) ? L : (res_cnt > 0) ? S : H) : E
 *    Resolving	(S)	(|AVSG| > 0) ? (logv ? L : (CML_Count > 0) ? L : H) : E
 *    Emulating	(E)	(|AVSG| > 0) ? (logv ? L : (CML_Count > 0) ? L : (res_cnt > 0) ? S : H) : E
 *    Logging   (L)     (|AVSG| > 0) ? ((res_cnt > 0) ? S : logv ? L : (CML_Count > 0) ? L : H) : E
 *
 *    State is initialized to Hoarding at startup, unless the volume
 *    is "dirty" in which case it is Emulating.
 *
 *    Logging state may be triggered by discovery of weak
 *    connectivity, an application-specific resolution, or the
 *    beginning of an IOT.  All of these are rolled into the flag
 *    "logv".  In this state, cache misses may be serviced, but modify
 *    activity (or other activity in the case of IOTs) is recorded in
 *    the CML.  Resolution must be permitted in logging state because
 *    references resulting in cache misses may require it.
 *
 *    Note that non-replicated volumes have only two states, {Hoarding, Emulating}.
 *
 *
 *    Events which prompt state transition are:
 *       1. Communications state change
 *       2. Begin/End of resolve session
 *       3. End of reintegration session
 *       4. Begin/End modify logging session
 *

 *    Volume state is an attempt to separate the logical state of the
 *    system---represented by the volume state---from the
 *    physical---represented by RPC connectivity.  It also serves to
 *    enforce some mutual exclusion constraints, such as the need to
 *    exclude read/write activity during resolution.
 *
 *    Separating connectivity state into volume and RPC levels has the
 *    advantage that changes in RPC (aka communication) connectivity,
 *    which are largely asynchronous, need not be immediately
 *    reflected throughout Venus.  Coping with the asynchrony is much
 *    easier if we can limit state changes to well-defined points.
 *

 * Our basic strategy is the following: * - the top layer, VFS,
 *          processes Vnode/VFS calls.  A VFS call begins by invoking
 *          volent::Enter on the appropriate volume.  This routine
 *          blocks if a state transition is already pending for the
 *          volume.  If the coast is clear, a count of "active users"
 *          for the volume is incremented.  The VFS call acquires
 *          file-system-objects (fsobjs) and operates on them via the
 *          CFS interface.  If a CFS call returns the internal error
 *          code ERETRY, the VFS routine is expected to release all
 *          fsobj's that it holds and call volent::Exit.  volent::Exit
 *          decrements the active user count and either returns a
 *          final code that is to be the result of the VFS call, or it
 *          indicates that the VFS call should be retried (i.e., start
 *          over with volent::Enter).
 *
 *        - the middle layer, CFS, acquires and performs Vice
 *        operations on fsobjs.  A CFS call uses an fsobj if it is
 *        valid.  An object is valid if it has a callback, is
 *        read-only, or if the volume is currently in disconnected
 *        mode (i.e., state = Emulating).  The CFS call attempts to
 *        validate the object (by fetching status and/or data) if it
 *        is invalid and the volume is connected.  Mutating CFS calls
 *        are written through to servers if in connected mode.
 *        Mutating operations are recorded on a (per-volume) ModifyLog
 *        if in disconnected mode.  Pending state changes are noticed
 *        by the Get{Conn,Mgrp} and certain Collate{...} calls.  They
 *        return ERETRY in such cases, which is to be passed up to the
 *        VFS layer.
 *
 *        - the bottom layer, Comms, immediately reflects RPC state
 *        changes by setting the "transition pending" flag in affected
 *        volumes.  It also will set a volume's "demotion pending"
 *        flag if the volume data needs to be demoted as a result of
 *        the Comms event.  The actions implied by these flags are
 *        performed either in volent::Exit when the last active user
 *        exits (as described above), or in the next volent::Enter
 *        call if there were no active users at the time the flags
 *        were set.  A daemon gratuitously enters every volume
 *        periodically so that transitions are taken within reasonable
 *        bounds.
 *
 *
 *    A volume in {Hoarding, Emulating, Logging} state may be entered
 *    in either Mutating or Observing mode. There may be only one
 *    mutating user in a volume at a time, although that user may have
 *    multiple mutating threads.  Observers do not conflict with each
 *    other, nor with the mutator (if present). Thus, this scheme
 *    differs from classical Shared/Exclusive locking.  Also note that
 *    no user threads, mutating or not, may enter a volume in
 *    {Reintegrating, Resolving} state. These restrictions do not
 *    apply to non-rw-replicated volumes, of course.
 * */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <struct.h>
#include <sys/param.h>

#include <unistd.h>
#include <stdlib.h>

#include <rpc2.h>
/* interfaces */
#include <vice.h>

#ifdef __cplusplus
}
#endif __cplusplus


/* from venus */
#include "advice_daemon.h"
#include "advice.h"
#include "comm.h"
#include "fso.h"
#include "local.h"
#include "mariner.h"
#include "user.h"
#include "venus.private.h"
#include "venuscb.h"
#include "venusrecov.h"
#include "venusvol.h"
#include "vproc.h"


int MLEs = UNSET_MLE;


/* local-repair modification */
void VolInit() {
    /* Allocate the database if requested. */
    if (InitMetaData) {					/* <==> VDB == 0 */
	Recov_BeginTrans();
	RVMLIB_REC_OBJECT(VDB);
	VDB = new vdb;
	Recov_EndTrans(0);

	/* Create the local fake volume */
	VolumeInfo LocalVol;
	bzero((void *)&LocalVol, (int)sizeof(VolumeInfo));
	FID_MakeVolFake(&LocalVol.Vid);
        LocalVol.Type = ROVOL;
	CODA_ASSERT(VDB->Create(&LocalVol, "Local"));
	
	/* allocate database for VCB usage. */
	Recov_BeginTrans();
	RVMLIB_REC_OBJECT(VCBDB);
	VCBDB = new vcbdb;
	Recov_EndTrans(0);
    }

    /* Initialize transient members. */
    VDB->ResetTransient();
    VCBDB->ResetTransient();

    /* Scan the database. */
    eprint("starting VDB scan");
    {
	/* Check entries in the table. */
	{
	    int FoundMLEs = 0;

	    vol_iterator next;
	    volent *v;
	    while ((v = next())) {
		/* Initialize transient members. */
		v->ResetTransient();

		/* Recover volume state. */
		v->Recover();

		FoundMLEs += v->CML.count();
	    }

	    eprint("\t%d vol entries in table (%d MLEs)",
		   VDB->htab.count(), VDB->AllocatedMLEs);
	    if (FoundMLEs != VDB->AllocatedMLEs)
		CHOKE("VolInit: MLE mismatch (%d != %d)",
		       FoundMLEs, VDB->AllocatedMLEs);
	}

	/* Check entries on the freelist. */
	{
	    /* Nothing useful to do! */

	    eprint("\t%d vol entries on free-list (%d MLEs)",
		   VDB->freelist.count(), VDB->mlefreelist.count());
	}

	if (VDB->htab.count() + VDB->freelist.count() > CacheFiles)
	    CHOKE("VolInit: too many vol entries (%d + %d > %d)",
		VDB->htab.count(), VDB->freelist.count(), CacheFiles);
	if (VDB->AllocatedMLEs + VDB->mlefreelist.count() > VDB->MaxMLEs)
	    CHOKE("VolInit: too many MLEs (%d + %d > %d)",
		VDB->AllocatedMLEs, VDB->mlefreelist.count(), VDB->MaxMLEs);
    }

    RecovFlush(1);
    RecovTruncate(1);

    /* Fire up the daemon. */
    VOLD_Init();
}


int VOL_HashFN(void *key) {
    return(*((VolumeId *)key));
}


int GetRootVolume() {
    /* If we don't already know the root volume name ask the server(s) for it. */
    if (RootVolName == UNSET_RV) {
	RPC2_BoundedBS RVN;
	RVN.MaxSeqLen = V_MAXVOLNAMELEN;
	RVN.SeqLen = 0;
	char buf[V_MAXVOLNAMELEN];
	RVN.SeqBody = (RPC2_ByteSeq)buf;

	/* Get the connection. */
	connent *c;
	int code = GetAdmConn(&c);
	if (code != 0) {
	    LOG(100, ("GetRootVolume: can't get SUConn!\n"));
	    RPCOpStats.RPCOps[ViceGetRootVolume_OP].bad++;
	    goto CheckRootVolume;
	}

	/* Make the RPC call. */
	MarinerLog("store::GetRootVolume\n");
	UNI_START_MESSAGE(ViceGetRootVolume_OP);
	code = (int) ViceGetRootVolume(c->connid, &RVN);
	UNI_END_MESSAGE(ViceGetRootVolume_OP);
	MarinerLog("store::getrootvolume done\n");
	code = c->CheckResult(code, 0);
	LOG(10, ("GetRootVolume: received name: %s, code: %d\n", RVN.SeqBody, code));
	UNI_RECORD_STATS(ViceGetRootVolume_OP);

	PutConn(&c);

CheckRootVolume:
	if (code != 0) {
	    /* Dig RVN out of recoverable store if possible. */
	    if (rvg->recov_RootVolName[0] != '\0') {
		strncpy((char *)RVN.SeqBody, rvg->recov_RootVolName, V_MAXVOLNAMELEN);
		RVN.SeqLen = V_MAXVOLNAMELEN;
	    }
	    else {
		eprint("GetRootVolume: can't get root volume name!");
		return(0);
	    }
	}
	RootVolName = new char[V_MAXVOLNAMELEN];
	strncpy(RootVolName, (char *)RVN.SeqBody, V_MAXVOLNAMELEN);
    }

    /* Record name in RVM. */
    if (rvg->recov_RootVolName[0] == '\0' ||
	 !STRNEQ(RootVolName, rvg->recov_RootVolName, V_MAXVOLNAMELEN)) {
	Recov_BeginTrans();
	    rvmlib_set_range(rvg->recov_RootVolName, V_MAXVOLNAMELEN);
	    strncpy(rvg->recov_RootVolName, RootVolName, V_MAXVOLNAMELEN);
	Recov_EndTrans(MAXFP);
    }

    /* Retrieve the volume. */
    volent *v;
    if (VDB->Get(&v, RootVolName) != 0) {
	eprint("GetRootVolume: can't get volinfo for root volume (%s)!", RootVolName);
	return(0);
    }
    rootfid.Volume = v->vid;
    FID_MakeRoot(&rootfid);
    VDB->Put(&v);

    return(1);
}



/* Allocate database from recoverable store. */
void *vdb::operator new(size_t len) {
    vdb *v = 0;

    v = (vdb *)rvmlib_rec_malloc((int)len);
    CODA_ASSERT(v);
    return(v);
}

vdb::vdb() : htab(VDB_NBUCKETS, VOL_HashFN) {
    /* Initialize the persistent members. */
    RVMLIB_REC_OBJECT(*this);
    MagicNumber = VDB_MagicNumber;
    MaxMLEs = MLEs;
    AllocatedMLEs = 0;
}


void vdb::ResetTransient() {
    /* Sanity checks. */
    if (MagicNumber != VDB_MagicNumber)
	CHOKE("vdb::ResetTransient: bad magic number (%d)", MagicNumber);

    htab.SetHFn(VOL_HashFN);
}

void vdb::operator delete(void *deadobj, size_t len) {
    abort(); /* what else? */
}

volent *vdb::Find(VolumeId volnum) {
    vol_iterator next(&volnum);
    volent *v;
    while ((v = next()))
	if (v->vid == volnum) return(v);

    return(0);
}


volent *vdb::Find(char *volname) {
    vol_iterator next;
    volent *v;
    while ((v = next()))
	if (STREQ(v->name, volname)) return(v);

    return(0);
}


/* MUST NOT be called from within transaction! */
volent *vdb::Create(VolumeInfo *volinfo, char *volname) {
    volent *v = 0;

    /* Check whether the key is already in the database. */
    if ((v = Find(volinfo->Vid)) != 0) {
/*	{ v->print(logFile); CHOKE("vdb::Create: key exists"); }*/
	eprint("reinstalling volume %s (%s)", v->name, volname);

	Recov_BeginTrans();
	rvmlib_set_range(v->name, V_MAXVOLNAMELEN);
	strcpy(v->name, volname);
	/* Other fields? -JJK */
	Recov_EndTrans(0);

	return(v);
    }

    /* Fashion a new object. */
    Recov_BeginTrans();
	v = new volent(volinfo, volname);
    Recov_EndTrans(MAXFP);

    if (v == 0)
	LOG(0, ("vdb::Create: (%x, %s, %d) failed\n", volinfo->Vid, volname, 0/*AllocPriority*/));
    return(v);
}


/* MUST NOT be called from within transaction! */
int vdb::Get(volent **vpp, VolumeId vid) {
    LOG(100, ("vdb::Get: vid = %x\n", vid));

    *vpp = 0;

    if (vid == 0) {
	LOG(0, ("vdb::Get: vid == 0\n"));
	return(VNOVOL);	    /* ??? -JJK */
    }

    /* First see if it's already in the table by number. */
    volent *v = Find(vid);
    if (v) {
	v->hold();
	*vpp = v;
	return(0);
    }

    /* If not, get it by name (synonym). */
    char volname[20];
    sprintf(volname, "%lu", vid);
    return(Get(vpp, volname));
}


/* MUST NOT be called from within transaction! */
/* This call ALWAYS goes through to servers! */
int vdb::Get(volent **vpp, char *volname) {
    LOG(100, ("vdb::Get: volname = %s\n", volname));

    *vpp = 0;
    int code = 0;

    VolumeInfo volinfo;

    for (;;) {
	/* Get a connection to any server. */
	connent *c;
	code = GetAdmConn(&c);
	if (code != 0) break;

	/* Make the RPC call. */
	MarinerLog("store::GetVolumeInfo %s\n", volname);
	UNI_START_MESSAGE(ViceGetVolumeInfo_OP);
	code = (int) ViceGetVolumeInfo(c->connid, (RPC2_String)volname, &volinfo);
	UNI_END_MESSAGE(ViceGetVolumeInfo_OP);
	MarinerLog("store::getvolumeinfo done\n");

	code = c->CheckResult(code, 0);
	UNI_RECORD_STATS(ViceGetVolumeInfo_OP);

	PutConn(&c);

	if (code == 0) break; /* used to || with ENXIO (VNOVOL) */

	if (code != 0 && code != ETIMEDOUT) return(code);
    }

    /* Look for existing volent with the desired name. */
    volent *v = Find(volname);
    if (code == ETIMEDOUT) {
	/* Completely disconnected case. */
	if (v) goto Exit;
	return(ETIMEDOUT);
    }
    if (v) {
	if (v->vid == volinfo.Vid) {
	    /* Mapping unchanged. */
	    /* Should we see whether any other info has changed (e.g., VSG)?. */
	    goto Exit;
	}
	else {
	    eprint("Mapping changed for volume %s (%x --> %x)",
		   volname, v->vid, volinfo.Vid);

	    /* Put a (unique) fakename in the old volent. */
	    char fakename[V_MAXVOLNAMELEN];
	    sprintf(fakename, "%lu", v->vid);
	    CODA_ASSERT(Find(fakename) == 0);
	    Recov_BeginTrans();
		rvmlib_set_range(v->name, V_MAXVOLNAMELEN);
		strcpy(v->name, fakename);
	    Recov_EndTrans(MAXFP);

	    /* Should we flush the old volent? */

	    /* Invalidate HDB entries bound to the volname. XXX */

	    /* Forget about the old volent now. */
	    v = 0;
	}
    }

    /* Must ensure that the vsg entry is cached for replicated volumes. */
    vsgent *vsg; vsg = 0;
    /*    if (volinfo.Type == ROVOL || volinfo.Type == REPVOL) { */
    if (volinfo.Type == REPVOL) {
	/* HACK! VolumeInfo doesn't yet contain vsg info for ROVOLs; fake it! */
	if (volinfo.Type == ROVOL && volinfo.VSGAddr == 0)
		CHOKE("Fix GetVolInfo to return a VSGAddr for ROVOLS\n");

	/* Pin the vsg entry. */
	if (VSGDB->Get(&vsg, volinfo.VSGAddr, (unsigned long *)&(volinfo.Server0)) != 0)
	    CHOKE("vdb::Get: couldn't get vsg (%x)", volinfo.VSGAddr);
    }

    /* Attempt the create. */
    v = Create(&volinfo, volname);
    if (v == 0)
	CHOKE("vdb::Get: Create (%x, %s) failed", volinfo.Vid, volname);

    /* Unpin the vsg entry. */
    if (vsg != 0)
	VSGDB->Put(&vsg);

Exit:
    v->hold();
    *vpp = v;
    return(0);
}


void vdb::Put(volent **vpp) {
    if (!(*vpp)) { LOG(100, ("vdb::Put: Null vpp\n")); return; }

    volent *v = *vpp;
    LOG(100, ("vdb::Put: (%x, %s), refcnt = %d\n",
	       v->vid, v->name, v->refcnt));

    v->release();
    *vpp = 0;
}


void vdb::FlushVolume() {
    vol_iterator next;
    volent *v;
    while ((v = next())) {
	if (FID_VolIsFake(v->vid)) 
		continue;
	v->FlushVSRs(VSR_FLUSH_HARD);
    }
}


/* MUST be called from within transaction! */
void vdb::AttachFidBindings() {
    vol_iterator next;
    volent *v;
    while ((v = next()))
	v->CML.AttachFidBindings();
}


int vdb::WriteDisconnect(unsigned age, unsigned time) {
    vol_iterator next;
    volent *v;
    int code = 0;

    while ((v = next())) {
	if (v->IsReplicated()) {
	    code = v->WriteDisconnect(age, time); 
	    if (code) break;
        }
    }
    return(code);
}


int vdb::WriteReconnect() {
    vol_iterator next;
    volent *v;
    int code = 0;

    while ((v = next())) {
	if (v->IsReplicated()) {
            code = v->WriteReconnect();
	    if (code) break;
        }
    }
    return(code);
}


void vdb::GetCmlStats(cmlstats& total_current, cmlstats& total_cancelled) {
    /* N.B.  We assume that caller has passed in zeroed-out structures! */
    vol_iterator next;
    volent *v;
    while ((v = next())) {
	cmlstats current;
	cmlstats cancelled;
	v->CML.IncGetStats(current, cancelled);
	total_current += current;
	total_cancelled += cancelled;
    }
}


void vdb::SaveCacheInfo(VolumeId volnum, int uid, int hits, int misses) {
    volent *vol;

    vol = Find(volnum);
    vol->SaveCacheInfo(uid, hits, misses);
}


void vdb::print(int fd, int SummaryOnly) {
    if (this == 0) return;

    fdprint(fd, "VDB:\n");
    fdprint(fd, "tbl count = %d, fl count = %d, mlefl count = %d\n",
	     htab.count(), freelist.count(), mlefreelist.count());
    fdprint(fd, "volume callbacks broken = %d, total callbacks broken = %d\n",
	    vcbbreaks, cbbreaks);
    if (!SummaryOnly) {
	vol_iterator next;
	volent *v;
	while ((v = next())) v->print(fd);
    }

    fdprint(fd, "\n");
}


void vdb::ListCache(FILE *fp, int long_format, unsigned int valid)
{
  volent *v = 0;
  vol_iterator next;
  while ((v = next()))
    v->ListCache(fp, long_format, valid);
}


/* local-repair modification */

/* MUST be called from within transaction! */
void *volent::operator new(size_t len) {
    volent *v = 0;

    if (VDB->freelist.count() > 0)
	v = strbase(volent, VDB->freelist.get(), handle);
    else 
        v = (volent *)rvmlib_rec_malloc((int)len);
    CODA_ASSERT(v);
    return(v);
}

/* MUST be called from within transaction! */
volent::volent(VolumeInfo *volinfo, char *volname) {

    LOG(10, ("volent::volent: (%x, %s), type = %d\n",
	      volinfo->Vid, volname, volinfo->Type));

    RVMLIB_REC_OBJECT(*this);
    MagicNumber = VOLENT_MagicNumber;
    strcpy(name, volname);
    vid = volinfo->Vid;
    reint_id_gen = 100;
    type = (int) volinfo->Type;
    {
	/* Read-Write-Replicas should be identified by the server! -JJK */
	if (type == RWVOL && (&volinfo->Type0)[replicatedVolume] != 0)
	    type = RWRVOL;

	switch(type) {
	    case RWVOL:
	    case BACKVOL:
		{
		host = volinfo->Server0;
		break;
		}

	    case ROVOL:
		{
		host = volinfo->VSGAddr;
		break;
		}

	    case REPVOL:
		{
		host = volinfo->VSGAddr;
		for (int i = 0; i < MAXHOSTS; i++)
		    u.rep.RWVols[i] = (&volinfo->RepVolMap.Volume0)[i];
		break;
		}

	    case RWRVOL:
		{
		host = volinfo->Server0;
		u.rwr.REPVol = (&volinfo->Type0)[replicatedVolume];
		break;
		}

	    default:
		CHOKE("volent::volent: (%x, %s), bogus type (%d)", vid, name, type);
	}
    }
    flags.reserved = 0;
    flags.has_local_subtree = 0;
    flags.logv = 0;
    VVV = NullVV;
    AgeLimit = V_UNSETAGE;
    ReintLimit = V_UNSETREINTLIMIT;
    DiscoRefCounter = -1;

    /* The uniqifiers should also survive shutdowns, f.i. the server
       remembers the last sid we successfully reintegrated. And in
       disconnected mode the unique fids map to unique inodes. The
       1<<19 shift is to avoid collisions with inodes derived from
       non-local generated fids -- JH */
    FidUnique = 1 << 19;
    SidUnique = 0;

    ResetTransient();

    /* Insert into hash table. */
    VDB->htab.insert(&vid, &handle);
    
    /* Read/Write Sharing Stat Collection */
    current_disc_time = 0;
    current_reco_time = 0;	
    current_rws_cnt = 0;
    current_disc_read_cnt = 0;
    bzero((void *)&rwsq, (int)sizeof(rec_dlist));
}


/* local-repair modification */
void volent::ResetTransient() {
    if ((type == ROVOL || type == REPVOL) && !FID_VolIsFake(vid) ) {
	/* don't need to set VSG for non-replicated volumes */
	if ((vsg = VSGDB->Find(host)) == 0)
	    { print(logFile); CHOKE("volent::ResetTransient: couldn't find vsg"); }
	vsg->hold();
    }

    state = Hoarding;
    observer_count = 0;
    mutator_count = 0;
    waiter_count = 0;
    shrd_count = 0;
    excl_count = 0;
    excl_pgid = 0;
    resolver_count = 0;
    cur_reint_tid = UNSET_TID;
    flags.transition_pending = 0;
    flags.demotion_pending = 0;
    flags.valid = 0;
    flags.online = 1;
    flags.usecallback = 1;
    flags.allow_asrinvocation = 1;
    flags.reintegratepending = 0;
    flags.reintegrating = 0;
    flags.repair_mode = 0;		    /* normal mode */
    flags.resolve_me = 0;
    flags.weaklyconnected = 0;

    fso_list = new olist;

    cop2_list = new dlist;

    CML.ResetTransient();
    Lock_Init(&CML_lock);
    OpenAndDirtyCount = 0;
    // Added 8/23/92 by bnoble - now transient
    RecordsCancelled = 0;
    RecordsCommitted = 0;
    RecordsAborted = 0;
    FidsRealloced = 0;
    BytesBackFetched = 0;

    res_list = new olist;

    VCBStatus = NoCallBack;
    VCBHits = 0;

    vsr_list = new olist;
    if (!FID_VolIsFake(vid))
      VsrUnique = GetAVSG((unsigned long *)&AVSG);

    saved_uid = -1;
    saved_hits = -1;
    saved_misses = -1;

    /* 
     * sync doesn't need to be initialized. 
     * It's used only for LWP_Wait and LWP_Signal. 
     */
    refcnt = 0;
}


/* MUST be called from within transaction! */
volent::~volent() {
    LOG(10, ("volent::~volent: name = %s, volume = %x, type = %d, refcnt = %d\n",
	      name, vid, type, refcnt));

    /* Unlink from VSG (if applicable). */
    if (type == ROVOL || type == REPVOL)
	VSGDB->Put(&vsg);

    /* Drain and delete transient lists. */
    {
	if (fso_list->count() != 0)
	    CHOKE("volent::~volent: fso_list not empty");
	delete fso_list;
    }
    {
	if (cop2_list->count() != 0)
	    CHOKE("volent::~volent: cop2_list not empty");
	delete cop2_list;
    }
    {
	if (res_list->count() != 0)
	    CHOKE("volent::~volent: res_list not empty");
	delete res_list;
    }
    {
	if (!FID_VolIsFake(vid)) {
	    FlushVSRs(VSR_FLUSH_HARD);
	    delete vsr_list;
	}
    }

    if (CML.count() != 0)
	{ print(logFile); CHOKE("volent::~volent: CML not empty"); }

    if (refcnt != 0)
	{ print(logFile); CHOKE("volent::~volent: non-zero refcnt"); }

    /* Remove from hash table. */
    if (VDB->htab.remove(&vid, &handle) != &handle)
	{ print(logFile); CHOKE("volent::~volent: htab remove"); }

}

/* MUST be called from within transaction! */
void volent::operator delete(void *deadobj, size_t len) {
    volent *v = (volent *)deadobj;
    
    LOG(10, ("volent::operator delete()\n"));

    /* Stick on free list or give back to heap. */
    if (VDB->freelist.count() < VOLENTMaxFreeEntries)
	VDB->freelist.append(&v->handle);
    else
	rvmlib_rec_free(deadobj);
}

/*MUST NOT be called from within transaction. */
void volent::Recover() {
    /* Pre-allocated Fids MUST be discarded if last shutdown was dirty (because one of them may have */
    /* actually been used in an object creation at the servers, but we crashed before we took the fid off */
    /* of our queue). */
    if (!CleanShutDown &&
	 (FileFids.Count != 0 || DirFids.Count != 0 || SymlinkFids.Count != 0)) {
	Recov_BeginTrans();
	    RVMLIB_REC_OBJECT(FileFids);
	    FileFids.Count = 0;
	    RVMLIB_REC_OBJECT(DirFids);
	    DirFids.Count = 0;
	    RVMLIB_REC_OBJECT(SymlinkFids);
	    SymlinkFids.Count = 0;
	Recov_EndTrans(MAXFP);
    }
}


void volent::hold() {
    refcnt++;
}


void volent::release() {
    refcnt--;

    if (refcnt < 0)
	{ print(logFile); CHOKE("volent::release: refcnt < 0"); }
}


/* See the notes on volume synchronization at the head of this file.
 *
 *    It's important to note that transitions and/or demotions may be
 *    pending at Enter time even though there are no "active users".
 *    Also note that a "transition pending" situation does not mean
 *    that the next state won't be the same as the current.  This can
 *    happen, for instance, if multiple transitions are signalled
 *    while a volume is inactive.  If the volume is active with
 *    transition pending we can rely on the last exiter taking the
 *    transition and resetting the flag.
 *
 *    Replicated volumes are complicated by two additional factors.
 *        1. transitions to two new states, reintegrating and
 *        resolving, may be indicated; user threads need to be
 *        excluded from the volume during these states
 *        2. only one mutating user, the "owner," is allowed in a
 *        volume at a time; this is to avoid nasty interdependencies
 *        that might otherwise arise during reintegration; note that
 *        a user is still the owner as long as he has active mutating
 *        threads or records in the ModifyLog.  
*/

#define	VOLBUSY(vol)\
    ((vol)->resolver_count > 0 || (vol)->mutator_count > 0 || (vol)->observer_count > 0)
/* MUST NOT be called from within transaction! */
int volent::Enter(int mode, vuid_t vuid) {
    LOG(1000, ("volent::Enter: vol = %x, state = %s, t_p = %d, d_p = %d, mode = %s, vuid = %d\n",
		vid, PRINT_VOLSTATE(state), flags.transition_pending,
		flags.demotion_pending, PRINT_VOLMODE(mode), vuid));

    int just_transitioned = 0;
    /*  Step 1 is to demote objects in volume if AVSG enlargement or
     * shrinking has made this necessary.  The two cases that require
     * this are:
     *    1. |AVSG| for read-write replicated volume increasing. 
     *    2. |AVSG| for non-replicated volume falling to 0.  */
    if (flags.demotion_pending) {
	LOG(1, ("volent::Enter: demoting %s\n", name));
	flags.demotion_pending = 0;

	ClearCallBack();

 	fso_vol_iterator next(NL, this);
	fsobj *f;
	while ((f = next()))
	    f->Demote(0);

	just_transitioned = 1;
    } 

    /* Step 2 is to take pending transition for the volume IF no thread is active in it. */
    while (flags.transition_pending && !VOLBUSY(this)) {
	TakeTransition();
	just_transitioned = 1;
    }

    /* Step 3 is to try to get a volume callback. */
    /* We allow only the hoard thread to fetch new version stamps if
     * we do not already have one.  If we do have stamps, we let other
     * threads validate them with one condition.  
     * The wierd condition below is to prevent the vol daemon from validating
     * volumes one at a time.  That is, if the volume has just taken a transition
     * or was just demoted, there is a good chance some other volumes have as well.  
     * We'd like them all to take transitions/demotions first so we can check
     * them en masse.  We risk sticking a real request with this overhead, but 
     * only if a request arrives in the next few (5) seconds!  
     */
    vproc *vp = VprocSelf();
    if (VCBEnabled && IsReplicated() && 
	(state == Hoarding || state == Logging) && 
	WantCallBack()) {
	if (just_transitioned && !HaveStamp()) {    
	    /* we missed an opportunity */
	    vcbevent ve(fso_list->count());
	    ReportVCBEvent(NoStamp, vid, &ve);
	}
	if ((!HaveStamp() && (vp->type == VPT_HDBDaemon)) ||
	    (HaveStamp() && ((vp->type != VPT_VolDaemon) || !just_transitioned))) {
	    int code = GetVolAttr(vuid);
	    LOG(100, ("volent::Enter: GetVolAttr(0x%x) returns %s\n", vid, VenusRetStr(code)));
        }
    }

    /* Step 4 is to attempt "entry" into this volume. */
    switch(mode & (VM_MUTATING | VM_OBSERVING | VM_RESOLVING)) {
	case VM_MUTATING:
	    {
	    for (;;) {
		/*
		 * acquire "shared" volume-lock for MUTATING or OBSERVING, also
		 * block while resolution is going on, while the CML is locked,
		 * or volume state transition is pending.
		 */
		vproc *vp = VprocSelf();
		int proc_key = vp->u.u_pgid;
		while ((excl_count > 0 && proc_key != excl_pgid) || state == Resolving
		        || WriteLocked(&CML_lock) || flags.transition_pending) {
		    if (mode & VM_NDELAY) return (EWOULDBLOCK);
		    LOG(0, ("volent::Enter: mutate or observe with proc_key = %d\n",
			    proc_key));
		    Wait();
		    if (VprocInterrupted()) return (EINTR);
		}

		/*
		 * mutator needs to aquire exclusive CML ownership
		 */
		if (type == REPVOL) {
		    /* 
		     * Claim ownership if the volume is free. 
		     * The CML lock is used to prevent checkpointers and mutators
		     * from executing in the volume simultaneously, because
		     * the CML must not change during a checkpoint.  We want
		     * shared/exclusive behavior, so all mutators obtain a shared
		     * (read) lock on the CML to prevent the checkpointer from entering.
		     * Note observers don't lock at all. 
		     */
		    if (CML.owner == UNSET_UID) {
			if (mutator_count != 0 || CML.count() != 0 || IsReintegrating())
			    { print(logFile); CHOKE("volent::Enter: mutating, CML owner == %d\n", CML.owner); }

			mutator_count++;
			CML.owner = vuid;
			shrd_count++;
			ObtainReadLock(&CML_lock);
			return(0);
		    }

		    /* Continue using the volume if possible. */
		    /* We might need to do something about fairness here eventually! -JJK */
		    if (CML.owner == vuid) {
			if (mutator_count == 0 && CML.count() == 0 && !IsReintegrating())
			    { print(logFile); CHOKE("volent::Enter: mutating, CML owner == %d\n", CML.owner); }

			mutator_count++;
			shrd_count++;
			ObtainReadLock(&CML_lock);
			return(0);
		    }

		    /* Wait until the volume becomes free again. */
		    {
			if (mode & VM_NDELAY) return(EWOULDBLOCK);

			Wait();
			if (VprocInterrupted()) return(EINTR);
			continue;
		    }
		}
		else {
		    mutator_count++;
		    shrd_count++;
		    return(0);
	        }
	    }
            }
        case VM_OBSERVING:
	    {
	    for (;;) {
		/*
		 * acquire "shared" volume-lock for MUTATING or OBSERVING, also
		 * block while resolution is going on, or volume state 
		 * transition is pending.
		 */
		vproc *vp = VprocSelf();
		int proc_key = vp->u.u_pgid;
		while ((excl_count > 0 && proc_key != excl_pgid) || state == Resolving
		        || flags.transition_pending) {
		    if (mode & VM_NDELAY) return (EWOULDBLOCK);
		    LOG(0, ("volent::Enter: mutate or observe with proc_key = %d\n",
			    proc_key));
		    Wait();
		    if (VprocInterrupted()) return (EINTR);
		}

		observer_count++;
		shrd_count++;
		return(0);
	    }
	    }

	case VM_RESOLVING:
	    {
	    CODA_ASSERT(type == REPVOL);
	    if (state != Resolving || resolver_count != 0 || 
		mutator_count != 0 || observer_count != 0 ||
		flags.transition_pending)
		{ print(logFile); CHOKE("volent::Enter: resolving"); }

	    /* acquire exclusive volume-pgid-lock for RESOLVING */
	    vproc *vp = VprocSelf();
	    int proc_key = vp->u.u_pgid;
	    while (shrd_count > 0 || excl_count > 0 && proc_key != excl_pgid) {
		/* 
		 * must wait until all the volume-pgid-locks are released.
		 * no need to check for VM_NDELAY and excl_pgid here.
		 * But, should we check for interrupt here?	-luqi
		 */
		LOG(0, ("volent::Enter: Wait--Resolving\n"));
		Wait();
	    }
	    excl_pgid = proc_key;
	    excl_count++;
	    resolver_count++;
	    return(0);
	    }

	default:
	    print(logFile); CHOKE("volent::Enter: bogus mode %d", mode);
    }

    return -1;
}

/* local-repair modification */
/* MUST NOT be called from within transaction! */
void volent::Exit(int mode, vuid_t vuid) {
    LOG(1000, ("volent::Exit: vol = %x, state = %s, t_p = %d, d_p = %d, mode = %s, vuid = %d\n",
		vid, PRINT_VOLSTATE(state), flags.transition_pending,
		flags.demotion_pending, PRINT_VOLMODE(mode), vuid));

    /* 
     * Step 1 is to demote objects in volume if AVSG enlargement or shrinking has made this 
     * necessary.  The two cases that require this are: 
     *    1. |AVSG| for read-write replicated volume increasing. 
     *    2. |AVSG| for non-replicated volume falling to 0. 
     */
    if (flags.demotion_pending) {
	LOG(1, ("volent::Exit: demoting %s\n", name));
	flags.demotion_pending = 0;

	ClearCallBack();

	fso_vol_iterator next(NL, this);
	fsobj *f;
	while ((f = next()))
	    f->Demote(0);
    }

    /* Step 2 is to "exit" this volume. */
    /* Non-replicated volumes are straightforward. */
    switch(mode & (VM_MUTATING | VM_OBSERVING | VM_RESOLVING)) {
	case VM_MUTATING:
	    {
 	    if (state == Resolving || mutator_count <= 0)
		{ print(logFile); CHOKE("volent::Exit: mutating"); }

	    mutator_count--;
	    shrd_count--;
	    ReleaseReadLock(&CML_lock);
	    if (type == REPVOL && mutator_count == 0 && CML.count() == 0 && !IsReintegrating()) {
		/* Special-case here. */
		/* If we just cancelled the last log record for a volume that was being kept in */
		/* Emulating state due to auth-token absence, we need to provoke a transition! */
		if (state == Emulating && !flags.transition_pending)
		    flags.transition_pending = 1;

		CML.owner = UNSET_UID;
	    }
	    break;
	    }

	case VM_OBSERVING:
	    {
 	    if (state == Resolving || observer_count <= 0)
		{ print(logFile); CHOKE("volent::Exit: observing"); }

	    observer_count--;
	    shrd_count--;
	    break;
	    }

	case VM_RESOLVING:
	    {
	    CODA_ASSERT(type == REPVOL);
	    if (state != Resolving || resolver_count != 1 || !flags.transition_pending)
		{ print(logFile); CHOKE("volent::Exit: resolving"); }

	    resolver_count--;
	    excl_count--;
	    if (0 == excl_count) 
	      /* reset the volume-pgid-lock key value */
	      excl_pgid = 0;
	    break;
	    }

	default:
	    print(logFile); CHOKE("volent::Exit: bogus mode %d", mode);
    }

    /* Step 3 is to take pending transition IF there is (now) no thread in it, */
    /* or poke waiting threads if no transition is pending. */
    if (flags.transition_pending) {
	if (!VOLBUSY(this))
	    TakeTransition();
    }
    else {
	if (waiter_count > 0)
	    Signal();
    }
}

void volent::GetVolInfoForAdvice(int *unique_references, int *unique_unreferenced) {
    *unique_references = 0;
    *unique_unreferenced = 0;

    /* Iterator through the volume's fsobj's to count 
     *    1) number of unique fsobj's referenced
     *    2) number of unique fsobj's not referenced
     * during this disconnected session.
     */
    {
        fso_vol_iterator next(NL, this);
        fsobj *f = 0;
        while ((f = next())) {
            if (FSDB->LastRef[f->ix] > DiscoRefCounter)
                /* If object was reference after the point of disconnection */
                (*unique_references)++;
            else
                /* If object was not reference during the disconnected session */
                (*unique_unreferenced)++;
        }
    }
}


void volent::SetDisconnectionTime() {
    Recov_BeginTrans();
        RVMLIB_REC_OBJECT(DisconnectionTime);
        DisconnectionTime = Vtime();
    Recov_EndTrans(0);
}


void volent::UnsetDisconnectionTime() {
    Recov_BeginTrans();
        RVMLIB_REC_OBJECT(DisconnectionTime);
        DisconnectionTime = 0;
    Recov_EndTrans(0);
}

void volent::SetDiscoRefCounter() {
    Recov_BeginTrans();
        RVMLIB_REC_OBJECT(DiscoRefCounter);
        DiscoRefCounter = FSDB->RefCounter;
    Recov_EndTrans(0);

  LOG(0, ("DiscoRef= %d\n", DiscoRefCounter));
}


void volent::UnsetDiscoRefCounter() {
    Recov_BeginTrans();
        RVMLIB_REC_OBJECT(DiscoRefCounter);
        DiscoRefCounter = 0;
    Recov_EndTrans(0);
  LOG(0, ("DiscoRef= %d\n", DiscoRefCounter));
}

void volent::SaveCacheInfo(int uid, int hits, int misses) {
    saved_uid = uid;
    saved_hits = hits;
    saved_misses = misses;
}


void volent::GetCacheInfo(int uid, int *hits, int *misses) {
    /* Only return values if the uid matches up */
    if (uid == saved_uid) {
        *hits = saved_hits;
        *misses = saved_misses;
        saved_uid = -1;
        saved_hits = -1;
        saved_misses = -1;
    } else {
        *hits = -1;
        *misses = -1;
    }
}

void volent::DisconnectedCacheMiss(vproc *vp, vuid_t vuid, ViceFid *fid, char *comp) {
    userent *u;
    char pathname[MAXPATHLEN];

    GetUser(&u, vuid);
    CODA_ASSERT(u != NULL);

    /* If advice not enabled, simply return */
    if (!AdviceEnabled) {
        LOG(0, ("ADMON STATS:  DMQ Advice NOT enabled.\n"));
        u->AdviceNotEnabled();
        return;
    }

    /* Check that:                                                     *
     *     (a) the request did NOT originate from the Hoard Daemon     *
     *     (b) the request did NOT originate from that AdviceMonitor,  *
     * and (c) the user is running an AdviceMonitor,                   */
    CODA_ASSERT(vp != NULL);
    if (vp->type == VPT_HDBDaemon) {
        LOG(100, ("ADMON STATS:  DMQ Advice inappropriate.\n"));
        return;
    }
    if (u->IsAdvicePGID(vp->u.u_pgid)) {
        LOG(100, ("ADMON STATS:  DMQ Advice inappropriate.\n"));
        return;
    }
    if (u->IsAdviceValid(DisconnectedCacheMissEventID, 1) != TRUE) {
        LOG(0, ("ADMON STATS:  DMQ Advice NOT valid. (uid = %d)\n", vuid));
        return;
    }

    /* Get the pathname */
    GetMountPath(pathname, 0);
    CODA_ASSERT(fid != NULL);
    if (!FID_IsVolRoot(fid)) 
        strcat(pathname, "/???");
    if (comp) {
        strcat(pathname, "/");
        strcat(pathname,comp);
    }

    /* Make the request */
    LOG(100, ("Requesting Disconnected CacheMiss Questionnaire...1\n"));
    u->RequestDisconnectedQuestionnaire(fid, pathname, vp->u.u_pid, GetDisconnectionTime());

}

void volent::TriggerReconnectionQuestionnaire() {
    user_iterator next;
    userent *u;
    vsr *record;
    int hits = 0;         int saved_hits = 0;
    int misses = 0;       int saved_misses = 0;
    int unique_hits = 0;  int unique_notreferenced = 0;

    LOG(100, ("TriggerReconnectionQuestionnaire:  vid=%x, name=%s.\n", vid, name));

    while ((u = next())) {
        if (u->IsAdviceValid(ReconnectionID, 0) == TRUE) {
            unique_hits = 0;
            unique_notreferenced = 0;

             GetVolInfoForAdvice(&unique_hits, &unique_notreferenced); 

            /* Get saved_hits and saved_misses on this volume */
            GetCacheInfo(u->GetUid(), &saved_hits, &saved_misses);

            /* Get this user's session record */
            record = GetVSR(u->GetUid());
            hits = (int)(record->cachestats.HoardDataHit.Count + 
                              record->cachestats.NonHoardDataHit.Count + 
                              record->cachestats.UnknownHoardDataHit.Count + 
                              record->cachestats.HoardAttrHit.Count + 
                              record->cachestats.NonHoardAttrHit.Count + 
                              record->cachestats.UnknownHoardAttrHit.Count);
            misses = (int)(record->cachestats.HoardDataMiss.Count +
                              record->cachestats.NonHoardDataMiss.Count +
                              record->cachestats.UnknownHoardDataMiss.Count +
                              record->cachestats.HoardAttrMiss.Count +
                              record->cachestats.NonHoardAttrMiss.Count +
                              record->cachestats.UnknownHoardAttrMiss.Count);
            PutVSR(record);

            LOG(100, ("saved_hits = %d saved_misses = %d hits = %d misses = %d\n", saved_hits, saved_misses, hits, misses));
            if ((saved_hits >= 0) && (saved_misses >= 0)) {
                LOG(100, ("Using saved values\n"));
                hits = saved_hits;
                misses = saved_misses;
            } 
	    else 
                LOG(0, ("TriggerReconnectionQuestionnaire:  Using calculated values\n"));

            LOG(100, ("TriggerReconnectionQuestionnaire:  userid = %d\n", u->GetUid()));
            u->RequestReconnectionQuestionnaire(name,
						vid,
						CML.count(),
					        DisconnectionTime,
					        HDB->GetDemandWalkTime(),
					        0,
					        hits,
					        misses,
					        (int)(unique_hits),
					        (int)(unique_notreferenced));
        }
    }
    UnsetDiscoRefCounter();   
    UnsetDisconnectionTime();
}


void volent::NotifyStateChange() {
    LOG(100, ("NotifyStateChange:  vid=%x, name=%s.\n", vid, name));
    /*
    user_iterator next;
    userent *u;

    while (u = next()) {
        if (u->IsAdviceValid(VolumeTransitionEventID, 0) == TRUE) {
	    switch(state) {
	        case Hoarding:
	            u->NotifyHoarding(name,vid);
		    break;	
		case Emulating:
		    u->NotifyEmulating(name,vid);
		    break;
		case Logging:
		    u->NotifyLogging(name,vid);
		    break;
		case Resolving:
		    u->NotifyResolving(name,vid);
		    break;
	    }
	}
    }
    */
}

/* local-repair modification */
void volent::TakeTransition() {
    CODA_ASSERT(flags.transition_pending && !VOLBUSY(this));

    int size = AvsgSize();
    LOG(1, ("volent::TakeTransition: %s, state = %s, |AVSG| = %d, CML = %d, Res = %d, logv = %d\n",
	     name, PRINT_VOLSTATE(state), size, CML.count(), res_list->count(), flags.logv));

    /* Compute next state. */
    VolumeStateType nextstate;
    VolumeStateType prevstate = state;
    switch(state) {
	case Hoarding:
	case Emulating:
  	     nextstate = (size == 0) ? Emulating : ((flags.logv || CML.count() > 0) ?
		    Logging : ((res_list->count() > 0) ? Resolving : Hoarding));
	     break;
        case Logging:
	     nextstate = (size == 0) ? Emulating : ((res_list->count() > 0) ? Resolving : 
		    ((flags.logv || CML.count() > 0) ? Logging : Hoarding));
	     break;

	case Resolving:
	    CODA_ASSERT(res_list->count() == 0);
	    nextstate = (size == 0) ? Emulating : ((flags.logv || CML.count() > 0) ?
		    Logging : Hoarding);
	    break;
	default:
	    CODA_ASSERT(0);
    }

    /* Special cases here. */
    /*
     * 1.  If the volume is transitioning  _to_ emulating, any reintegations will not
     *     be stopped because of lack of tokens.
     */
    if (nextstate == Emulating)
	ClearReintegratePending();

    /* 2.  We refuse to transit to reintegration unless owner has auth tokens. */
    /* 3.  We force "zombie" volumes to emulation state until they are un-zombied. */
    if (nextstate == Logging && CML.count() > 0) {
	userent *u = 0;
	GetUser(&u, CML.owner);
	if (!u->TokensValid()) {
	    SetReintegratePending();
	    nextstate = Emulating;
	}
	PutUser(&u);
    }

    /* Take corresponding action. */
    state = nextstate;
    flags.transition_pending = 0;
    switch(state) {
        case Hoarding:
            {
	      if ((prevstate != Hoarding) && (AdviceEnabled) && 
		  (!FID_VolIsFake(vid)))
		      NotifyStateChange();

	      /* If:                                                                         *
	       *     we were disconnected                                                    *
	       *     the user has made some (read-only) references since disconnecting, and  *
	       *     the volume is not a local fake volume,                                  *
	       * then we want to collect some usage statistics for the purposes of providing *
	       * hoard advice.                                                               *
	       *                                                                             */
	      if ((prevstate == Emulating) && 
		  (FSDB->RefCounter > DiscoRefCounter) &&
		  (!FID_VolIsFake(vid))) {
		  FSDB->UpdateDisconnectedUseStatistics(this);
	      }

              /*  We trigger a reconnection questionnaire request to the advice monitor      *
               *  if we are transitioning from the Emulating state into the Hoarding state.  * 
               *  This is to catch volumes which have no modifications.  We let the advice   *
               *  monitor decide whether or not to actually question the user.               */
	      if ((prevstate == Emulating) && (AdviceEnabled) && 
		  (!FID_VolIsFake(vid))) 
		  TriggerReconnectionQuestionnaire();


	      /* Collect Read/Write Sharing Stats */
	      if (prevstate == Emulating)
		RwStatUp();

              Signal();
              break;
	    }

        case Emulating:
            {
	      if ((prevstate != Emulating) && (AdviceEnabled) && 
		  (!FID_VolIsFake(vid)))
		      NotifyStateChange();

              if (prevstate == Hoarding) {
                SetDiscoRefCounter();
                SetDisconnectionTime();
              }

	      /* Read/Write Sharing Stats Collection */
	      if (prevstate == Hoarding || prevstate == Logging)
		RwStatDown();

              Signal();
              break;
            }

        case Logging:
	    if ((prevstate != Logging) && (AdviceEnabled) && 
		(!FID_VolIsFake(vid)))
		    NotifyStateChange();

	    if (ReadyToReintegrate()) 
		::Reintegrate(this);

            /* If:                                                                         *
	     *     we were disconnected                                                    *
	     *     the user has made some references since disconnecting, and              *
	     *     the volume is not a local fake volume,                                  *
	     * then we want to collect some usage statistics for the purposes of providing *
	     * hoard advice.                                                               *
             *                                                                             *
	     * N.B. Must occur *before* call to TriggerReconnectionQuestionnaire           */
	    if ((prevstate == Emulating) && 
		(FSDB->RefCounter > DiscoRefCounter) &&
		(!FID_VolIsFake(vid))) {
	            FSDB->UpdateDisconnectedUseStatistics(this);
	    }


            /*  We trigger a reconnection questionnaire request to the advice monitor     *
             *  if we are transitioning from the Emulating state into the Logging state.  * 
             *  This is to catch volumes which have modifications.  We let the advice     *
             *  monitor decide whether or not to actually question the user.              */
	    if ((prevstate == Emulating) && (AdviceEnabled) && (!FID_VolIsFake(vid)))
		TriggerReconnectionQuestionnaire();  


	    /* Read/Write Sharing Stats Collection */
	    if (prevstate == Emulating)
	      RwStatUp();
	    
	    Signal();
	    break;

	case Resolving:
	    if ((prevstate != Resolving) && (AdviceEnabled) && 
		(!FID_VolIsFake(vid)))
	        NotifyStateChange();

	    ::Resolve(this);
	    break;

	default:
	    CODA_ASSERT(0);
    }

    /* Bound RVM persistence out of paranoia. */
    Recov_SetBound(DMFP);
}


void volent::DownMember(long eTime) {
    /* set the event time */
    /* eventTime = eTime; */
    /* 
     * We no longer want to set the event time, b/c this volume may not be touched
     * for quite some time.  Instead, we must iterate throught the vsr list, and
     * set the end time for each one.  Comm events end sessions.
     */
    olist_iterator next(*vsr_list);
    vsr *vsrp;
    while ((vsrp = (vsr *)next()))
	vsrp->cetime = eTime;
    /*
     * XXX - we are flushing all of the vsr's at comm event time.
     * This seems to be a better idea than letting the vsr's get
     * flushed at a begin_vfs (or end_vfs), as that is guaranteed
     * to add latency to a user request, where this way, we have a
     * chance of the vsr's being flushed when an independent deamon
     * discovers the new server state.
     */
    if (!FID_VolIsFake(vid)) 
	    this->FlushVSRs(VSR_FLUSH_HARD);
    ResetStats();
    /* Consider transitting to Emulating state. */
    if (AvsgSize() == 0) {
	flags.transition_pending = 1;
	
	/* Coherence is now suspect for all objects in RW, REP, and RWR volumes. */
	if (type == RWVOL || type == REPVOL || type == RWRVOL)
	    flags.demotion_pending = 1;
    }
}


void volent::UpMember(long eTime) {
    /* set the event time */
    /* eventTime = eTime; */
    /*  We no longer want to set the event time, b/c this volume may
     * not be touched for quite some time.  Instead, we must iterate
     * throught the vsr list, and set the end time for each one.  Comm
     * events end sessions.  */
    olist_iterator next(*vsr_list);
    vsr *vsrp;
    while ((vsrp = (vsr *)next()))
	if (vsrp->cetime == 0) vsrp->cetime = eTime;
    /*
     * XXX - we are flushing all of the vsr's at comm event time.
     * This seems to be a better idea than letting the vsr's get
     * flushed at a begin_vfs (or end_vfs), as that is guaranteed
     * to add latency to a user request, where this way, we have a
     * chance of the vsr's being flushed when an independent deamon
     * discovers the new server state.
     */
    if (!FID_VolIsFake(vid))
      this->FlushVSRs(VSR_FLUSH_HARD);
    ResetStats();
    
    /* Consider transitting to Hoarding state. */
    if (AvsgSize() == 1)
	flags.transition_pending = 1;

    /* Coherence is now suspect for all objects in REP volumes. */
    if (type == REPVOL)
	flags.demotion_pending = 1;
}


/* 
 * volent::{Weak,Strong} member.  Cope with a change in 
 * connectivity for a VSG member by  setting the volume
 * weakly connected, and by write-disconnecting or write-reconnecting 
 * the volume.  Note that both of these indications are necessary;
 * while for replicated volumes 
 * 	weakly connected --> write-disconnected 
 * it is not the case that write-disconnected --> weakly connected.
 * In particular, the volume may be write-disconnected for an ASR.
 * Note that write disconnection does not apply to read-only and
 * non-replicated volumes.
 */
void volent::WeakMember() {
    /* should extend volume session records for this */
    if (!IsWeaklyConnected()) {
	if (type == REPVOL)
	    WriteDisconnect();	/* sets transition pending */
	else 
	    flags.transition_pending = 1;

	flags.weaklyconnected = 1;
    }
}


void volent::StrongMember() {
    /* should extend volume session records for this */
    /* vsg check is for 0, not 1, because the conn is already strong */
    if (IsWeaklyConnected() && WeakVSGSize() == 0) {
	if (type == REPVOL)
	    WriteReconnect();	/* sets transition pending */
	else
	    flags.transition_pending = 1;

	flags.weaklyconnected = 0;
    }
}


int volent::WriteDisconnect(unsigned age, unsigned time) {
    int code = 0;

    if (type == REPVOL) {
	Recov_BeginTrans();
	    RVMLIB_REC_OBJECT(*this);
	    flags.logv = 1;
	    if (age != AgeLimit || time != ReintLimit ||
		age == V_UNSETAGE || time == V_UNSETREINTLIMIT) {
		   if (age == V_UNSETAGE) 
		       AgeLimit = V_DEFAULTAGE;
		   else 
		       AgeLimit = age;

		   if (time == V_UNSETREINTLIMIT) 
		       ReintLimit = V_DEFAULTREINTLIMIT;
		   else 
		       ReintLimit = time*1000;
	    }
        Recov_EndTrans(MAXFP);
	flags.transition_pending = 1;
    } else
	code = EINVAL;

    return(code);
}


int volent::WriteReconnect() {
    int code = 0;

    if (type == REPVOL) {
	Recov_BeginTrans();
	       RVMLIB_REC_OBJECT(*this);
	       flags.logv = 0;
	       AgeLimit = V_UNSETAGE;
	       ReintLimit = V_UNSETREINTLIMIT;
	Recov_EndTrans(MAXFP);
	flags.transition_pending = 1;
    } else
	code = EINVAL;

    return(code);
}


void volent::SetReintegratePending() {
    flags.reintegratepending = 1;
    CheckReintegratePending();
}


void volent::ClearReintegratePending() {
    flags.reintegratepending = 0;

    if (AdviceEnabled) {
        userent *u;
        GetUser(&u, CML.owner);
        CODA_ASSERT(u != NULL);
        u->NotifyReintegrationEnabled(name);
    }
}


void volent::CheckReintegratePending() {
    if (flags.reintegratepending && CML.count() > 0) {
        eprint("Reintegrate %s pending tokens for uid = %d", name, CML.owner);
        if (AdviceEnabled) {
            userent *u;
            GetUser(&u, CML.owner);
            CODA_ASSERT(u != NULL);
            u->NotifyReintegrationPending(name);
        }
    }
}


void volent::Wait() {
    waiter_count++;
    LOG(0, ("WAITING(VOL): %s, state = %s, [%d, %d], counts = [%d %d %d %d]\n",
	     name, PRINT_VOLSTATE(state), flags.transition_pending, flags.demotion_pending,
 	     observer_count, mutator_count, waiter_count, resolver_count));
    LOG(0, ("CML= [%d, %d], Res = %d\n", CML.count(), CML.owner, res_list->count()));
    LOG(0, ("WAITING(VOL): shrd_count = %d, excl_count = %d, excl_pgid = %d\n",
	    shrd_count, excl_count, excl_pgid));
    START_TIMING();
    VprocWait(&sync);
    END_TIMING();
    LOG(0, ("WAIT OVER, elapsed = %3.1f\n", elapsed));
    waiter_count--;
}


void volent::Signal() {
    VprocSignal(&sync);
}

void volent::Lock(VolLockType l, int pgid)
{
    /* Sanity Check */
    if (l != EXCLUSIVE && l != SHARED) {
	print(logFile); 
	CHOKE("volent::Lock: bogus lock type");
    }

    /* the default pgid(when the argument pgid == 0) is the vproc's */
    if (0 == pgid) {
	vproc *vp = VprocSelf();
	pgid = vp->u.u_pgid;
    }
    LOG(100, ("volent::Lock: (%s) lock = %d pgid = %d excl = %d shrd = %d\n",
	    name, l, pgid, excl_count, shrd_count));

    while (l == SHARED ? (excl_count > 0 && excl_pgid != pgid) : 
	   (shrd_count > 0 || (excl_count > 0 && excl_pgid != pgid))) {
	LOG(0, ("volent::Lock: wait\n"));
	Wait();
    }
    l == EXCLUSIVE ? (excl_count++, excl_pgid = pgid) : (shrd_count++);
}

void volent::UnLock(VolLockType l)
{	
    LOG(100, ("volent::UnLock: (%s) lock = %d pgid = %d excl = %d shrd = %d\n",
	      name, l, excl_pgid, excl_count, shrd_count));

    /* Sanity Check */
    if (l != EXCLUSIVE && l != SHARED) {
	print(logFile); 
	CHOKE("volent::UnLock bogus lock type");
    }

    if (excl_count < 0 || shrd_count < 0) {
	print(logFile); 
	CHOKE("volent::UnLock pgid = %d excl_count = %d shrd_count = %d",
	      excl_pgid, excl_count, shrd_count);
    }
    l == EXCLUSIVE ? (excl_count--) : (shrd_count--);
    if (0 == excl_count) 
      excl_pgid = 0;
    Signal();
}

int volent::GetConn(connent **c, vuid_t vuid) {
    if (IsReplicated())
	{ print(logFile); CHOKE("volent::GetConn: IsReplicated"); }

    int code = ETIMEDOUT;
    *c = 0;

    /* Get a connection to any custodian. */
    unsigned long Hosts[MAXHOSTS];
    GetHosts(Hosts);
    for (int i = 0; i < MAXHOSTS  && !flags.transition_pending; i++)
	if (Hosts[i]) {
	    do {
		code = ::GetConn(c, Hosts[i], vuid, 0);
		if (code < 0)
		    CHOKE("volent::GetConn: bogus code (%d)", code);
	    } while (code == ERETRY && !flags.transition_pending);

	    if (code != ETIMEDOUT)
		break;
	}

    if (flags.transition_pending)
	code = ERETRY;
    return(code);
}


int volent::Collate(connent *c, int code) {
    code = c->CheckResult(code, vid);

    /* when the operation has failed miserably, but we have a pending volume
     * transition, just retry the operation */
    if (code && flags.transition_pending)
	code = ERETRY;
    return(code);
}


int volent::GetMgrp(mgrpent **m, vuid_t vuid, RPC2_CountedBS *PiggyBS) {
    if (!IsReplicated())
	{ print(logFile); CHOKE("volent::GetMgrp: !IsReplicated"); }

    *m = 0;
    int code = 0;

    if (!flags.transition_pending) {
	/* Get an mgrp for this VSG and user. */
	code = ::GetMgrp(m, host, vuid);
	if (code < 0)
	    CHOKE("volent::GetMgrp: bogus code (%d)", code);

	/* Get PiggyCOP2 buffer if requested. */
	if (code == 0 && PiggyBS != 0 && !FID_VolIsFake(vid))
	    code = FlushCOP2(*m, PiggyBS);
    }

    if (flags.transition_pending)
	code = ERETRY;
    if (code != 0)
	::PutMgrp(m);
    return(code);
}


int volent::Collate_NonMutating(mgrpent *m, int code) {
    code = m->CheckNonMutating(code);

    /* when the operation has failed miserably, but we have a pending volume
     * transition, just retry the operation */
    if (code && flags.transition_pending)
	code = ERETRY;
    return(code);
}


int volent::Collate_COP1(mgrpent *m, int code, ViceVersionVector *UpdateSet) {
    code = m->CheckCOP1(code, UpdateSet);

    return(code);
}


int volent::Collate_Reintegrate(mgrpent *m, int code, ViceVersionVector *UpdateSet) {
    code = m->CheckReintegrate(code, UpdateSet);

    return(code);
}


int volent::Collate_COP2(mgrpent *m, int code) {
    code = m->CheckNonMutating(code);

    /* Nothing useful we can do with an EASYRESOLVE response. */
    if (code == EASYRESOLVE)
	code = 0;

    return(code);
}


int volent::AllocFid(ViceDataType Type, ViceFid *target_fid,
		      RPC2_Unsigned *AllocHost, vuid_t vuid, int force) {
    VOL_ASSERT(this, type == REPVOL);
    LOG(10, ("volent::AllocFid: (%x, %d), uid = %d\n", vid, Type, vuid));

    /* Use a preallocated Fid if possible. */
    {
	FidRange *Fids = 0;
	switch(Type) {
	    case File:
		Fids = &FileFids;
		break;

	    case Directory:
		Fids = &DirFids;
		break;

	    case SymbolicLink:
		Fids = &SymlinkFids;
		break;

	    case Invalid:
	    default:
		print(logFile);
		CHOKE("volent::AllocFid: bogus Type (%d)", Type);
	}
	if (Fids->Count > 0) {
	    target_fid->Volume = vid;
	    target_fid->Vnode = Fids->Vnode;
	    target_fid->Unique = Fids->Unique;
	    *AllocHost = Fids->AllocHost;

	    Recov_BeginTrans();
		   RVMLIB_REC_OBJECT(*Fids);
		   Fids->Vnode += Fids->Stride;
		   Fids->Unique++;
		   Fids->Count--;
	    Recov_EndTrans(MAXFP);

	    LOG(100, ("volent::AllocFid: target_fid = (%x.%x.%x)\n",
		      target_fid->Volume, target_fid->Vnode, target_fid->Unique));
	    return(0);
	}
    }

    int code = 0;
    *AllocHost = 0;

    /* 
     * While write-disconnected we usually want to generate a local fid.
     * This defers the latency of contacting the servers for fids until
     * reintegrate time.  However, reintegrators MUST contact the servers
     * for global fids to translate local fids in the CML records being
     * reintegrated. The "force" parameter defaults to 0, in which case the
     * decision is based on state.  Reintegrators call this routine with
     * "force" set.  Note that we do not simply check IsReintegrating because
     * a mutator executing during reintegration need not (and should not) 
     * be required to contact the servers.
     */
    if (state == Emulating || (state == Logging && !force)) {
	*target_fid = GenerateLocalFid(Type);
    }
    else {
	VOL_ASSERT(this, (state == Hoarding || (state == Logging && force)));

	/* COP2 Piggybacking. */
	char PiggyData[COP2SIZE];
	RPC2_CountedBS PiggyBS;
	PiggyBS.SeqLen = 0;
	PiggyBS.SeqBody = (RPC2_ByteSeq)PiggyData;

	{
	    /* Acquire an Mgroup. */
	    mgrpent *m = 0;
	    code = GetMgrp(&m, vuid, (PIGGYCOP2 ? &PiggyBS : 0));
	    if (code != 0) goto Exit;

	    /* The Remote AllocFid call. */
	    {
		ViceFidRange NewFids;
		NewFids.Count = 32;

		/* Make multiple copies of the IN/OUT and OUT parameters. */
		int ph_ix; unsigned long ph = m->GetPrimaryHost(&ph_ix);
		ARG_MARSHALL(IN_OUT_MODE, ViceFidRange, NewFidsvar, NewFids, VSG_MEMBERS);

		/* Make the RPC call. */
		MarinerLog("store::AllocFids %s\n", name);
		MULTI_START_MESSAGE(ViceAllocFids_OP);
		code = (int) MRPC_MakeMulti(ViceAllocFids_OP, ViceAllocFids_PTR,
				      VSG_MEMBERS, m->rocc.handles,
				      m->rocc.retcodes, m->rocc.MIp, 0, 0,
				      vid, Type, NewFidsvar_ptrs, ph, &PiggyBS);
		MULTI_END_MESSAGE(ViceAllocFids_OP);
		MarinerLog("store::allocfids done\n");

		/* Collate responses from individual servers and decide what to do next. */
		code = Collate_NonMutating(m, code);
		MULTI_RECORD_STATS(ViceAllocFids_OP);
		if (code == EASYRESOLVE) code = 0;
		if (code != 0) goto Exit;

		/* Finalize COP2 Piggybacking. */
		if (PIGGYCOP2)
		    ClearCOP2(&PiggyBS);

		/* Manually compute the OUT parameters from the mgrpent::AllocFids() call! -JJK */
		int dh_ix = -1;
		code = m->DHCheck(0, ph_ix, &dh_ix, 1);
		if (code != 0) goto Exit;
		ARG_UNMARSHALL(NewFidsvar, NewFids, dh_ix);

		if (NewFids.Count <= 0)
		    { code = EINVAL; goto Exit; }
		Recov_BeginTrans();
		    FidRange *Fids = 0;
		    switch(Type) {
			case File:
			    Fids = &FileFids;
			    break;

			case Directory:
			    Fids = &DirFids;
			    break;

			case SymbolicLink:
			    Fids = &SymlinkFids;
			    break;

			case Invalid:
			default:
			    print(logFile);
			    CHOKE("volent::AllocFid: bogus Type (%d)", Type);
		    }

		    RVMLIB_REC_OBJECT(*Fids);
		    Fids->Vnode = NewFids.Vnode;
		    Fids->Unique = NewFids.Unique;
		    Fids->Stride = NewFids.Stride;
		    Fids->Count = NewFids.Count;
		    Fids->AllocHost = /*ph*/(unsigned long)-1;
		       
		    target_fid->Volume = vid;
		    target_fid->Vnode = Fids->Vnode;
		    target_fid->Unique = Fids->Unique;
		    *AllocHost = Fids->AllocHost;

		    Fids->Vnode += Fids->Stride;
		    Fids->Unique++;
		    Fids->Count--;
		Recov_EndTrans(MAXFP);
	    }

Exit:
	    PutMgrp(&m);
	}
    }

    if (code == 0)
	LOG(10, ("volent::AllocFid: target_fid = (%x.%x.%x)\n",
		 target_fid->Volume, target_fid->Vnode, target_fid->Unique));
    return(code);
}


ViceFid volent::GenerateLocalFid(ViceDataType fidtype) {
    ViceFid fid;

    if ( fidtype == Directory ) 
	    FID_MakeDiscoDir(&fid, vid, FidUnique);
    else 
	    FID_MakeDiscoFile(&fid, vid, FidUnique);

    Recov_BeginTrans();
    RVMLIB_REC_OBJECT(FidUnique);
    FidUnique++;
    Recov_EndTrans(MAXFP);

    return(fid);
}


/* MUST be called from within a transaction */
ViceFid volent::GenerateFakeFid() 
{
    ViceFid fid;
    FID_MakeSubtreeRoot(&fid, vid, FidUnique);

    RVMLIB_REC_OBJECT(FidUnique);
    FidUnique++;

    return(fid);
}

/* MUST be called from within a transaction */
ViceStoreId volent::GenerateStoreId() {
    ViceStoreId sid;

    /* VenusGenID, is randomly chosen whenever rvm is reinitialized, it
     * should be a 128-bit UUID (re-generated whenever rvm is reinitialized).
     * But that would require changing in the venus-vice protocol to either
     * add this UUID to every operation, or send it once per (volume-)
     * connection setup with ViceNewConnectFS. -JH */
    sid.Host = (RPC2_Unsigned)VenusGenID;

    RVMLIB_REC_OBJECT(SidUnique);
    sid.Uniquifier = (RPC2_Unsigned)SidUnique++;

    return(sid);
}


/* local-repair modification */
int volent::GetVolStat(VolumeStatus *volstat, RPC2_BoundedBS *Name,
			RPC2_BoundedBS *msg, RPC2_BoundedBS *motd, vuid_t vuid) {
    LOG(100, ("volen::GetVolStat: vid = %x, vuid = %d\n", vid, vuid));

    if (FID_VolIsFake(vid)) {
	/* make up some numbers for the local-fake volume */
	LOG(100, ("volent::GetVolStat: Local Volume vuid = %d\n", vuid));	
	FID_MakeVolFake(&volstat->Vid);
	volstat->ParentId = 0xfffffffe; /* NONSENSE, but what is right? pjb */
	volstat->InService = 1;
    	volstat->Blessed = 1;
	volstat->NeedsSalvage = 1;
	volstat->Type = ReadOnly;		/* OK ? */
	volstat->MinQuota = 0;
	volstat->MaxQuota = 0;
	volstat->BlocksInUse = 5000;
	volstat->PartBlocksAvail = 5000;
	volstat->PartMaxBlocks = 10000;
	return 0;
    }

    int code = 0;

    if (state == Emulating) {
	/* We do not cache this data! */
	code = ETIMEDOUT;
    }
    else {
	VOL_ASSERT(this, (state == Hoarding || state == Logging));

	if (IsReplicated()) {
	    /* Acquire an Mgroup. */
	    mgrpent *m = 0;
	    code = GetMgrp(&m, vuid);
	    if (code != 0) goto RepExit;

	    {
		/* Make multiple copies of the IN/OUT and OUT parameters. */
		int ph_ix; unsigned long ph; ph = m->GetPrimaryHost(&ph_ix);
		if (Name->MaxSeqLen > VENUS_MAXBSLEN ||
		    msg->MaxSeqLen > VENUS_MAXBSLEN ||
		    motd->MaxSeqLen > VENUS_MAXBSLEN)
		    CHOKE("volent::GetVolStat: BS len too large");
		ARG_MARSHALL(IN_OUT_MODE, VolumeStatus, volstatvar, *volstat, VSG_MEMBERS);
		ARG_MARSHALL_BS(IN_OUT_MODE, RPC2_BoundedBS, Namevar, *Name, VSG_MEMBERS, VENUS_MAXBSLEN);
		ARG_MARSHALL_BS(IN_OUT_MODE, RPC2_BoundedBS, msgvar, *msg, VSG_MEMBERS, VENUS_MAXBSLEN);
		ARG_MARSHALL_BS(IN_OUT_MODE, RPC2_BoundedBS, motdvar, *motd, VSG_MEMBERS, VENUS_MAXBSLEN);

		/* Make the RPC call. */
		MarinerLog("store::GetVolStat %s\n", name);
		MULTI_START_MESSAGE(ViceGetVolumeStatus_OP);
		code = (int) MRPC_MakeMulti(ViceGetVolumeStatus_OP, ViceGetVolumeStatus_PTR,
				      VSG_MEMBERS, m->rocc.handles,
				      m->rocc.retcodes, m->rocc.MIp, 0, 0,
				      vid, volstatvar_ptrs, Namevar_ptrs,
				      msgvar_ptrs, motdvar_ptrs, ph);
		MULTI_END_MESSAGE(ViceGetVolumeStatus_OP);
		MarinerLog("store::getvolstat done\n");

		/* Collate responses from individual servers and decide what to do next. */
		code = Collate_NonMutating(m, code);
		MULTI_RECORD_STATS(ViceGetVolumeStatus_OP);
		if (code == EASYRESOLVE) code = 0;
		if (code != 0) goto RepExit;

		/* Copy out OUT parameters. */
		int dh_ix; dh_ix = -1;
		code = m->DHCheck(0, ph_ix, &dh_ix);
		if (code != 0) CHOKE("volent::GetVolStat: no dh");
		ARG_UNMARSHALL(volstatvar, *volstat, dh_ix);
		ARG_UNMARSHALL_BS(Namevar, *Name, dh_ix);
		ARG_UNMARSHALL_BS(msgvar, *msg, dh_ix);
		ARG_UNMARSHALL_BS(motdvar, *motd, dh_ix);
	    }

	    /* Translate Vid and Name to Replicated values. */
	    volstat->Vid = vid;
	    strcpy((char *)Name->SeqBody, name);
	    Name->SeqLen = strlen((char *)name) + 1;

RepExit:
	    PutMgrp(&m);
	}
	else {
	    /* Acquire a Connection. */
	    connent *c;
	    code = GetConn(&c, vuid);
	    if (code != 0) goto NonRepExit;

	    /* Make the RPC call. */
	    MarinerLog("store::GetVolStat %s\n", name);
	    UNI_START_MESSAGE(ViceGetVolumeStatus_OP);
	    code = (int) ViceGetVolumeStatus(c->connid, vid,
				       volstat, Name, msg, motd, 0);
	    UNI_END_MESSAGE(ViceGetVolumeStatus_OP);
	    MarinerLog("store::getvolstat done\n");

	    /* Examine the return code to decide what to do next. */
	    code = Collate(c, code);
	    UNI_RECORD_STATS(ViceGetVolumeStatus_OP);

NonRepExit:
	    PutConn(&c);
	}
    }

    return(code);
}


int volent::SetVolStat(VolumeStatus *volstat, RPC2_BoundedBS *Name,
		RPC2_BoundedBS *msg, RPC2_BoundedBS *motd, vuid_t vuid) {
    LOG(100, ("volent::SetVolStat: vid = %x, vuid = %d\n", vid, vuid));

    int code = 0;

    if (state == Emulating) {
	/* We do not cache this data! */
	code = ETIMEDOUT;
    }
    else {
	VOL_ASSERT(this, (state == Hoarding || state == Logging));  /* let this go in wcc? -lily */

        /* COP2 Piggybacking. */
	char PiggyData[COP2SIZE];
	RPC2_CountedBS PiggyBS;
	PiggyBS.SeqLen = 0;
	PiggyBS.SeqBody = (RPC2_ByteSeq)PiggyData;

	if (IsReplicated()) {
	    /* Acquire an Mgroup. */
	    mgrpent *m = 0;
	    vv_t UpdateSet;

	    Recov_BeginTrans();
            ViceStoreId sid = GenerateStoreId();
	    Recov_EndTrans(MAXFP);

  	    code = GetMgrp(&m, vuid, (PIGGYCOP2 ? &PiggyBS : 0));
	    if (code != 0) goto RepExit;

	    {
		/* Make multiple copies of the IN/OUT and OUT parameters. */
		int ph_ix; unsigned long ph; ph = m->GetPrimaryHost(&ph_ix);
		if (Name->MaxSeqLen > VENUS_MAXBSLEN ||
		    msg->MaxSeqLen > VENUS_MAXBSLEN ||
		    motd->MaxSeqLen > VENUS_MAXBSLEN)
		    CHOKE("volent::SetVolStat: BS len too large");
		ARG_MARSHALL(IN_OUT_MODE, VolumeStatus, volstatvar, *volstat, VSG_MEMBERS);
		ARG_MARSHALL_BS(IN_OUT_MODE, RPC2_BoundedBS, Namevar, *Name, VSG_MEMBERS, VENUS_MAXBSLEN);
		ARG_MARSHALL_BS(IN_OUT_MODE, RPC2_BoundedBS, msgvar, *msg, VSG_MEMBERS, VENUS_MAXBSLEN);
		ARG_MARSHALL_BS(IN_OUT_MODE, RPC2_BoundedBS, motdvar, *motd, VSG_MEMBERS, VENUS_MAXBSLEN);

		/* Make the RPC call. */
		MarinerLog("store::SetVolStat %s\n", name);
		MULTI_START_MESSAGE(ViceSetVolumeStatus_OP);
		code = (int) MRPC_MakeMulti(ViceSetVolumeStatus_OP, ViceSetVolumeStatus_PTR,
				      VSG_MEMBERS, m->rocc.handles,
				      m->rocc.retcodes, m->rocc.MIp, 0, 0,
				      vid, volstatvar_ptrs, Namevar_ptrs,
				      msgvar_ptrs, motdvar_ptrs, ph,
				      &sid, &PiggyBS);
		MULTI_END_MESSAGE(ViceSetVolumeStatus_OP);
		MarinerLog("store::setvolstat done\n");

		/* Collate responses from individual servers and decide what to do next. */
		code = Collate_COP1(m, code, &UpdateSet);
		MULTI_RECORD_STATS(ViceSetVolumeStatus_OP);
		if (code != 0) goto RepExit;

		/* Finalize COP2 Piggybacking. */
		if (PIGGYCOP2)
		    ClearCOP2(&PiggyBS);

		/* Copy out OUT parameters. */
		int dh_ix; dh_ix = -1;
		code = m->DHCheck(0, ph_ix, &dh_ix);
		if (code != 0) CHOKE("volent::SetVolStat: no dh");
		ARG_UNMARSHALL(volstatvar, *volstat, dh_ix);
		ARG_UNMARSHALL_BS(Namevar, *Name, dh_ix);
		ARG_UNMARSHALL_BS(msgvar, *msg, dh_ix);
		ARG_UNMARSHALL_BS(motdvar, *motd, dh_ix);
	    }

            /* Send the COP2 message or add an entry for piggybacking. */
            if (PIGGYCOP2)
	        AddCOP2(&sid, &UpdateSet);
 	    else
	        (void)COP2(m, &sid, &UpdateSet);

RepExit:
	    PutMgrp(&m);
	}
	else {
	    /* Acquire a Connection. */
	    connent *c;
 	    ViceStoreId Dummy;          /* Need an address for ViceSetVolStat */
	    code = GetConn(&c, vuid);
	    if (code != 0) goto NonRepExit;

	    /* Make the RPC call. */
	    MarinerLog("store::SetVolStat %s\n", name);
	    UNI_START_MESSAGE(ViceSetVolumeStatus_OP);
	    code = (int) ViceSetVolumeStatus(c->connid, vid,
				       volstat, Name, msg, motd, 0, &Dummy, 
				       &PiggyBS);
	    UNI_END_MESSAGE(ViceSetVolumeStatus_OP);
	    MarinerLog("store::setvolstat done\n");

	    /* Examine the return code to decide what to do next. */
	    code = Collate(c, code);
	    UNI_RECORD_STATS(ViceSetVolumeStatus_OP);

NonRepExit:
	    PutConn(&c);
	}
    }

    return(code);
}


void volent::UseCallBack(int flag) {
    flags.usecallback = flag;
    FSDB->ResetVolume(vid, flags.usecallback);
}


void volent::GetHosts(unsigned long *hosts) {
    switch(type) {
	case RWVOL:
	case BACKVOL:
	case RWRVOL:
	    bzero((void *)hosts, (int)(MAXHOSTS * sizeof(unsigned long)));
	    hosts[0] = host;
	    return;

	case ROVOL:
	case REPVOL:
	    vsg->GetHosts(hosts);
	    return;

	default:
	    CHOKE("volent::GetHosts: %x, bogus type (%d)", vid, type);
    }
}


unsigned long volent::GetAVSG(unsigned long *hosts) {
    CODA_ASSERT( !FID_VolIsFake(vid));
    unsigned long AvsgId = 0;

    /* Always return AVSG identifier. */
    switch(type) {
	case RWVOL:
	case BACKVOL:
	case RWRVOL:
	    {
	    srvent *s = 0;
	    GetServer(&s, host);
	    AvsgId = s->GetEventCounter();
	    PutServer(&s);
	    break;
	    }

	case ROVOL:
	case REPVOL:
	    AvsgId = vsg->GetEventCounter();
	    break;

	default:
	    CHOKE("volent::GetAVSG: %x, bogus type (%d)", vid, type);
    }

    /* Optionally return contents of AVSG. */
    if (hosts != 0) {
	GetHosts(hosts);

	for (int i = 0; i < MAXHOSTS; i++)
	    if (hosts[i] != 0) {
		srvent *s = 0;
		GetServer(&s, hosts[i]);
		if (!s->ServerIsUp()) hosts[i] = 0;
		PutServer(&s);
	    }
    }

    return(AvsgId);
}


int volent::AvsgSize() {
    int count = 0;
    unsigned long hosts[MAXHOSTS];
    GetHosts(hosts);

    for (int i = 0; i < MAXHOSTS; i++)
	if (hosts[i] != 0) {
	    srvent *s = 0;
	    GetServer(&s, hosts[i]);
	    if (s->ServerIsUp()) count++;
	    PutServer(&s);
	}

    return(count);
}


int volent::WeakVSGSize() {
    int count = 0;
    unsigned long hosts[MAXHOSTS];
    GetHosts(hosts);

    for (int i = 0; i < MAXHOSTS; i++)
	if (hosts[i] != 0) {
	    srvent *s = 0;
	    GetServer(&s, hosts[i]);
	    if (s->ServerIsWeak()) count++;
	    PutServer(&s);
	}

    return(count);
}


int volent::IsHostedBy(unsigned long Host) {
    switch(type) {
	case RWVOL:
	case BACKVOL:
	case RWRVOL:
	    return(Host == host);

	case ROVOL:
	case REPVOL:
	    return(vsg->IsMember(Host));

	default:
	    CHOKE("volent::IsHostedBy: %x, bogus type (%d)", vid, type);
	    return(0); /* dummy for g++ */
    }
}


void volent::GetMountPath(char *buf, int ok_to_assert) {
    /* Need to suppress assertion when called via hoard verify,
       because expected invariants may not always be true */

    ViceFid fid;
    fid.Volume = vid;
    FID_MakeRoot(&fid);
    fsobj *f = FSDB->Find(&fid);
    if (f) {
	f->GetPath(buf, 1);
    }
    else {
	if (ok_to_assert) {
	    VOL_ASSERT(this, f != 0);
	}
	else strcpy(buf, "???");
    }

}


void volent::print(int afd) {
    fdprint(afd, "%#08x : %-16s : vol = %x, type = %s, host = %x\n",
	     (long)this, name, vid, PRINT_VOLTYPE(type), host);
    switch(type) {
	case RWVOL:
	case ROVOL:
	case BACKVOL:
	    {
	    break;
	    }

	case REPVOL:
	    {
	    fdprint(afd, "\tRWVols: [");
	    for (int i = 0; i < MAXHOSTS; i++)
		fdprint(afd, " %x, ", u.rep.RWVols[i]);
	    fdprint(afd, " ]\n");
	    break;
	    }

	case RWRVOL:
	    {
	    fdprint(afd, "\tREPVOL = %x\n", u.rwr.REPVol);
	    break;
	    }

	default:
	    CHOKE("volent::print: %x, bogus type (%d)", vid, type);
    }
    fdprint(afd, "\trefcnt = %d, fsos = %d, valid = %d, online = %d, logv = %d, weak = %d\n",
	    refcnt, fso_list->count(), flags.valid, flags.online, flags.logv,
	    flags.weaklyconnected);
    fdprint(afd, "\tstate = %s, t_p = %d, d_p = %d, counts = [%d %d %d %d], repair = %d\n",
	    PRINT_VOLSTATE(state), flags.transition_pending, flags.demotion_pending,
	    observer_count, mutator_count, waiter_count, resolver_count, 
	    flags.repair_mode);
    fdprint(afd, "\tshrd_count = %d and excl_count = %d excl_pgid = %d\n",
	    shrd_count, excl_count, excl_pgid);

    fdprint(afd, "\tCB status = %d", VCBStatus);
    fdprint(afd, "\tVersion stamps = [");
    for (int i = 0; i < VSG_MEMBERS; i++)
	fdprint(afd, " %d", (&(VVV.Versions.Site0))[i]);
    fdprint(afd, " ]\n");

    fdprint(afd, "\treint_id_gen = %d, age limit = %u (sec), reint limit = %u (msec)\n", 
	    reint_id_gen, AgeLimit, ReintLimit);
    fdprint(afd, "\thas_local_subtree = %d, resolve_me = %d\n", 
	    flags.has_local_subtree, flags.resolve_me);

    /* Modify Log. */
    CML.print(afd);

    /* Resolve List. */
    if (res_list != 0) {
	fdprint(afd, "\tResList: count = %d\n", res_list->count());
	olist_iterator rnext(*res_list);
	resent *r;
	while ((r = (resent *)rnext()))
	    r->print(afd);
    }

    /* Volume Session Statistics */ 
    if ( !FID_VolIsFake(vid) && (LogLevel >= 100)) {
        vsr *record;
        record = GetVSR(2660);
        CODA_ASSERT(record->cetime == 0);
        fdprint(afd, "\tVSR[2660]:\n");
        fdprint(afd, "\t\tHoard: DATA: Hit=%d, Miss=%d, NoSpace=%d;  ATTR: Hit=%d, Miss=%d, NoSpace=%d. \n", record->cachestats.HoardDataHit.Count, record->cachestats.HoardDataMiss.Count, record->cachestats.HoardDataNoSpace.Count, record->cachestats.HoardAttrHit.Count, record->cachestats.HoardAttrMiss.Count, record->cachestats.HoardAttrNoSpace.Count);
        fdprint(afd, "\t\tNonHoard: DATA: Hit=%d, Miss=%d, NoSpace=%d;  ATTR: Hit=%d, Miss=%d, NoSpace=%d. \n", record->cachestats.NonHoardDataHit.Count, record->cachestats.NonHoardDataMiss.Count, record->cachestats.NonHoardDataNoSpace.Count, record->cachestats.NonHoardAttrHit.Count, record->cachestats.NonHoardAttrMiss.Count, record->cachestats.NonHoardAttrNoSpace.Count);
        fdprint(afd, "\t\tUnknownHoard: DATA: Hit=%d, Miss=%d, NoSpace=%d;  ATTR: Hit=%d, Miss=%d, NoSpace=%d. \n", record->cachestats.UnknownHoardDataHit.Count, record->cachestats.UnknownHoardDataMiss.Count, record->cachestats.UnknownHoardDataNoSpace.Count, record->cachestats.UnknownHoardAttrHit.Count, record->cachestats.UnknownHoardAttrMiss.Count, record->cachestats.UnknownHoardAttrNoSpace.Count);
        PutVSR(record);    
    }

    fdprint(afd, "\tDiscoRefCounter=%d\n", DiscoRefCounter);

    fdprint(afd, "\tcurrent_rws_cnt = %d\n", current_rws_cnt);
    fdprint(afd, "\tcurrent_disc_read_cnt = %d\n", current_disc_read_cnt);
    if (rwsq.count() > 0) {
	rec_dlink *d;
	rec_dlist_iterator next(rwsq);
	rwsent *rws;
	while ((d = next())) {
	    rws = (rwsent *)d;
	    fdprint(afd, "\tsharing_count = %d disc_read_count = %d read_duration = %d\n",
		    rws->sharing_count, rws->disc_read_count, rws->disc_duration);
	}	
    } else {
	fdprint(afd, "\trwsq is empty\n");
    }
}


void volent::ListCache(FILE* fp, int long_format, unsigned int valid)
{
  char mountpath[MAXPATHLEN];
  GetMountPath(mountpath, 0);
  fprintf(fp, "%s: %lx\n", mountpath, vid);
  fsobj *f = 0;
  fso_vol_iterator next(NL, this);
  while ((f = next())) {
    if ( !FSDB->Find(&f->fid) )
      fprintf(fp, "Not Cached.");
    else 
      f->ListCache(fp, long_format, valid);
  }
  fprintf(fp, "\n");
  fflush(fp);
}


/*  *****  Volume Iterator  *****  */

vol_iterator::vol_iterator(void *key) : rec_ohashtab_iterator(VDB->htab, key) {
}


volent *vol_iterator::operator()() {
    rec_olink *o = rec_ohashtab_iterator::operator()();
    if (!o) return(0);

    volent *v = strbase(volent, o, handle);
    return(v);
}


/*  *****  Read/Write Sharing Stat Collection  *****  */

/* must be called from within a transaction */
void *rwsent::operator new(size_t len) {
    rwsent *rws = 0;

    rws = (rwsent *)rvmlib_rec_malloc((int)sizeof(rwsent));
    CODA_ASSERT(rws);
    return(rws);
}

rwsent::rwsent(short rws_count, short rd_count, int duration)
{

    RVMLIB_REC_OBJECT(*this);
    sharing_count = rws_count;
    disc_read_count = rd_count;
    disc_duration = duration;
}

/* must be called from within a transaction */
rwsent::~rwsent(){ /* nothing to do */ }

/* MUST be called from within transaction! */
void rwsent::operator delete(void *deadobj, size_t len) {
    rvmlib_rec_free(deadobj);
}


/* must not be called from within a transaction */
void volent::RwStatUp()
{
    Recov_BeginTrans();
	   RVMLIB_REC_OBJECT(current_reco_time);
	   current_reco_time = Vtime();
    Recov_EndTrans(MAXFP);
}

/* must not be called from within a transaction */
void volent::RwStatDown()
{
    /* reset the previously read-dirty objects in the volume */
    fso_vol_iterator next(NL, this);
    fsobj *fso;
    int count = 0;
    while ((fso = next())) {
	if (fso->flags.discread) {
	    count++;
	    Recov_BeginTrans();
		   RVMLIB_REC_OBJECT(fso->flags);
		   fso->flags.discread = 0;
	    Recov_EndTrans(MAXFP);
	}
    }

    Recov_BeginTrans();
	   /* needs to be changed into Vmon Reporting Later */
	   rwsent *rws = new rwsent(current_rws_cnt, current_disc_read_cnt + count, 
				    current_reco_time - current_disc_time);
	   rwsq.insert(rws);
	   RVMLIB_REC_OBJECT(current_disc_time);
	   current_disc_time = Vtime();
	   RVMLIB_REC_OBJECT(current_rws_cnt);
	   current_rws_cnt = 0;
	   RVMLIB_REC_OBJECT(current_disc_read_cnt);
	   current_disc_read_cnt = 0;
    Recov_EndTrans(MAXFP);
}

/* must not be called from within a transaction */
rec_dlist *volent::GetRwQueue()
{
    return &rwsq;
}
