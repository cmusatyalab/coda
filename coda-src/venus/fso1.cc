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
 *
 *    Implementation of the Venus File-System Object (fso) abstraction.
 *
 *    ToDo:
 *       1. Need to allocate meta-data by priority (escpecially in the case of dir pages and modlog entries)
 */


/* Following block is shared with worker.c. */
/* It is needed to ensure that C++ makes up "anonymous types" in the same order.  It sucks! */
#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <struct.h>
#include <stdlib.h>
#include "coda_string.h"
#include <sys/param.h>
#include <unistd.h>
#include <stdlib.h>
#include <rpc2/rpc2.h>
#include <netdb.h>

#include <math.h>

#include <time.h>
#include <coda.h>

#ifdef __cplusplus
}
#endif __cplusplus

/* interfaces */
/* this is silly and only needed for the IsVirgin test! */
#include <cml.h>

/* from vicedep */
#include <venusioctl.h>

/* from venus */
#include "advice.h"
#include "advice_daemon.h"
#include "comm.h"
#include "fso.h"
#include "local.h"
#include "mariner.h"
#include "user.h"
#include "venus.private.h"
#include "venusrecov.h"
#include "venusvol.h"
#include "vproc.h"
#include "worker.h"


static int NullRcRights = 0;
static AcRights NullAcRights = { ALL_UIDS, 0, 0, 0 };


/*  *****  Constructors, Destructors  *****  */

/* Pre-allocation routine. */
/* MUST be called from within transaction! */

void *fsobj::operator new(size_t len, fso_alloc_t fromwhere){
    fsobj *f = 0;

    CODA_ASSERT(fromwhere == FROMHEAP);
    /* Allocate recoverable store for the object. */
    f = (fsobj *)rvmlib_rec_malloc((int)len);
    CODA_ASSERT(f);
    return(f);
}

void *fsobj::operator new(size_t len, fso_alloc_t fromwhere, int AllocPriority){
    fsobj *f = 0;
    int rc = 0;

    CODA_ASSERT(fromwhere == FROMFREELIST); 
    /* Find an existing object that can be reclaimed. */
    rc = FSDB->AllocFso(AllocPriority, &f);
    if (rc == ENOSPC)
      LOG(0, ("fsobj::new returns 0 (fsdb::AllocFso returned ENOSPC)\n"));
//    CODA_ASSERT(f);
    return(f);
}

void *fsobj::operator new(size_t len){
    abort(); /* should never be called */
}

fsobj::fsobj(int i) : cf(i) {

    RVMLIB_REC_OBJECT(*this);
    ix = i;

    /* Put ourselves on the free-list. */
    FSDB->FreeFso(this);
}


/* MUST be called from within transaction! */
fsobj::fsobj(ViceFid *key, char *name) : cf() {
    LOG(10, ("fsobj::fsobj: fid = (%s), comp = %s\n", FID_(key),
	     name == NULL ? "(no name)" : name));

    RVMLIB_REC_OBJECT(*this);
    ResetPersistent();
    fid = *key;
    {
	int len = (name ? (int) strlen(name) : 0) + 1;
	comp = (char *)rvmlib_rec_malloc(len);
	rvmlib_set_range(comp, len);
	if (name) strcpy(comp, name);
	else comp[0] = '\0';
    }
    if (FID_IsVolRoot(&fid))
	mvstat = ROOT;
    ResetTransient();

    /* Insert into hash table. */
    (FSDB->htab).append(&fid, &primary_handle);
}

/* local-repair modification */
/* MUST be called from within transaction! */
/* Caller sets range for whole object! */
void fsobj::ResetPersistent() {
    MagicNumber = FSO_MagicNumber;
    fid = NullFid;
    comp = 0;
    vol = 0;
    state = FsoRunt;
    stat.VnodeType = Invalid;
    stat.LinkCount = (unsigned char)-1;
    stat.Length = 0;
    stat.DataVersion = 0;
    stat.VV = NullVV;
    stat.VV.StoreId.Host = NO_HOST;
    stat.Date = (Date_t)-1;
    stat.Author = ALL_UIDS;
    stat.Owner = ALL_UIDS;
    stat.Mode = (unsigned short)-1;
    ClearAcRights(ALL_UIDS);
    flags.fake = 0;
    flags.owrite = 0;
    flags.fetching = 0;
    flags.dirty = 0;
    flags.local = 0;
    flags.discread = 0;	    /* Read/Write Sharing Stat Collection */
    mvstat = NORMAL;
    pfid = NullFid;
    CleanStat.Length = (unsigned long)-1;
    CleanStat.Date = (Date_t)-1;
    data.havedata = 0;
    DisconnectionsSinceUse = 0;
    DisconnectionsUnused = 0;
}

/* local-repair modification */
/* Needn't be called from within transaction. */
void fsobj::ResetTransient() {
    /* Sanity checks. */
    if (MagicNumber != FSO_MagicNumber)
	{ print(logFile); CHOKE("fsobj::ResetTransient: bogus MagicNumber"); }

    /* This is a horrible way of resetting handles! */
    bzero((void *)&vol_handle, (int)sizeof(vol_handle));
    bzero((void *)&prio_handle, (int)sizeof(prio_handle));
    bzero((void *)&del_handle, (int)sizeof(del_handle));
    bzero((void *)&owrite_handle, (int)sizeof(owrite_handle));

    if (HAVEDATA(this) && stat.VnodeType == Directory &&
	mvstat != MOUNTPOINT) {
	data.dir->udcfvalid = 0;
	data.dir->udcf = 0;
    }
    ClearRcRights();
    DemoteAcRights(ALL_UIDS);
    flags.backup = 0;
    flags.readonly = 0;
    flags.replicated = 0;
    flags.rwreplica = 0;
    flags.usecallback = 0;
    flags.replaceable = 0;
    flags.era = 1;
    flags.ckmtpt = 0;
    flags.random = ::random();

    bzero((void *)&u, (int)sizeof(u));

    pfso = 0;
    children = 0;
    bzero((void *)&child_link, (int)sizeof(child_link));

    priority = -1;
    HoardPri = 0;
    HoardVuid = HOARD_UID;
    hdb_bindings = 0;
    FetchAllowed = HF_DontFetch;
    AskingAllowed = HA_Ask;

    mle_bindings = 0;
    shadow = 0;
    
    /* 
     * sync doesn't need to be initialized. 
     * It's used only for LWP_Wait and LWP_Signal. 
     */
    readers = 0;
    writers = 0;
    openers = 0;
    Writers = 0;
    Execers = 0;
    refcnt = 0;

    cachehit.count = 0;
    cachehit.blocks = 0;
    cachemiss.count = 0;
    cachemiss.blocks = 0;
    cachenospace.count = 0;
    cachenospace.blocks = 0;

    lastresolved = 0;

    /* Link to volume, and initialize volume specific members. */
    {
	if ((vol = VDB->Find(fid.Volume)) == 0)
	    { print(logFile); CHOKE("fsobj::ResetTransient: couldn't find volume"); }
	vol->hold();
	if (vol->IsBackup()) flags.backup = 1;
	if (vol->IsReadOnly()) flags.readonly = 1;
	if (vol->IsReplicated()) flags.replicated = 1;
	if (vol->IsReadWriteReplica()) flags.rwreplica = 1;
	if (vol->flags.usecallback) flags.usecallback = 1;
    }

    /* Add to volume list */
    vol->fso_list->append(&vol_handle);

    if (flags.local == 1) {
	/* set valid RC status for local object */
	SetRcRights(RC_DATA | RC_STATUS);
    }
}


/* MUST be called from within transaction! */
fsobj::~fsobj() {
    RVMLIB_REC_OBJECT(*this);

#ifdef	VENUSDEBUG
    /* Sanity check. */
    if (!GCABLE(this) || DIRTY(this))
	{ print(logFile); CHOKE("fsobj::~fsobj: !GCABLE || DIRTY"); }
#endif	VENUSDEBUG

    LOG(10, ("fsobj::~fsobj: fid = (%s), comp = %s\n", FID_(&fid), comp));

    /* Reset reference counter for this slot. */
    FSDB->LastRef[ix] = 0;

    /* MLE bindings must already be detached! */
    if (mle_bindings) {
	if (mle_bindings->count() != 0)
	    { print(logFile); CHOKE("fsobj::~fsobj: mle_bindings->count() != 0"); }
	delete mle_bindings;
	mle_bindings = 0;
    }

    /* Detach hdb bindings. */
    if (hdb_bindings) {
	DetachHdbBindings();
	if (hdb_bindings->count() != 0)
	    { print(logFile); CHOKE("fsobj::~fsobj: hdb_bindings->count() != 0"); }
	delete hdb_bindings;
	hdb_bindings = 0;
    }

    /* Detach ourselves from our parent (if necessary). */
    if (pfso != 0) {
	pfso->DetachChild(this);
	pfso = 0;
    }

    /* Detach any children of our own. */
    if (children != 0) {
	dlink *d = 0;
	while ((d = children->first())) {
	    fsobj *cf = strbase(fsobj, d, child_link);

	    /* If this is a FakeDir delete all associated FakeMtPts since they are no longer useful! */
	    if (IsFakeDir())
		cf->Kill();

	    DetachChild(cf);
	    cf->pfso = 0;
	}
	if (children->count() != 0)
	    { print(logFile); CHOKE("fsobj::~fsobj: children->count() != 0"); }
	delete children;
    }

    /* Do mount cleanup. */
    switch(mvstat) {
	case NORMAL:
	    /* Nothing to do. */
	    break;

	case MOUNTPOINT:
	    /* Detach volume root. */
	    {
		fsobj *root_fso = u.root;
		if (root_fso == 0) {
		    print(logFile);
		    CHOKE("fsobj::~fsobj: root_fso = 0");
		}
		if (root_fso->u.mtpoint != this) {
		    print(logFile);
		    root_fso->print(logFile);
		    CHOKE("fsobj::~fsobj: rf->mtpt != mf");
		}
		root_fso->UnmountRoot();
		UncoverMtPt();
	    }
	    break;

	case ROOT:
	    /* Detach mtpt. */
	    if (u.mtpoint != 0) {
		fsobj *mtpt_fso = u.mtpoint;
		if (mtpt_fso->u.root != this) {
		    mtpt_fso->print(logFile);
		    print(logFile);
		    CHOKE("fsobj::~fsobj: mf->root != rf");
		}
		UnmountRoot();
		mtpt_fso->UncoverMtPt();
	    }
	    break;

	default:
	    print(logFile);
	    CHOKE("fsobj::~fsobj: bogus mvstat");
    }

    /* Remove from volume's fso list */
    if (vol->fso_list->remove(&vol_handle) != &vol_handle)
	{ print(logFile); CHOKE("fsobj::~fsobj: fso_list remove"); }

    /* Unlink from volume. */
    VDB->Put(&vol);

    /* Release data. */
    if (HAVEDATA(this))
	DiscardData();

    /* Remove from the delete queue. */
    if (FSDB->delq->remove(&del_handle) != &del_handle)
	{ print(logFile); CHOKE("fsobj::~fsobj: delq remove"); }

    /* Remove from the table. */
    if ((FSDB->htab).remove(&fid, &primary_handle) != &primary_handle)
	{ print(logFile); CHOKE("fsobj::~fsobj: htab remove"); }

    /* Notify waiters of dead runts. */
    if (!HAVESTATUS(this)) {
	LOG(10, ("fsobj::~fsobj: dead runt = (%s)\n", FID_(&fid)));

	FSDB->matriculation_count++;
	VprocSignal(&FSDB->matriculation_sync);
    }

    /* Return component string to heap. */
    rvmlib_rec_free(comp);

}

void fsobj::operator delete(void *deadobj, size_t len) {

    LOG(10, ("fsobj::operator delete()\n"));

    /* Stick on the free list. */
    FSDB->FreeFso((fsobj *)deadobj);
}

/* local-repair modification */
/* MUST NOT be called from within transaction. */
void fsobj::Recover()
{
    /* Validate state. */
    switch(state) {
	case FsoRunt:
	    /* Objects that hadn't matriculated can be safely discarded. */
	    eprint("\t(%s, %s) runt object being discarded...",
		   comp, FID_(&fid));
	    goto Failure;

	case FsoNormal:
	    break;

	case FsoDying:
	    /* Dying objects should shortly be deleted. */
	    FSDB->delq->append(&del_handle);
	    break;

	default:
	    print(logFile);
	    CHOKE("fsobj::Recover: bogus state");
    }

    /* Uncover mount points. */
    if (mvstat == MOUNTPOINT) {
	Recov_BeginTrans();
	RVMLIB_REC_OBJECT(stat.VnodeType);
	stat.VnodeType = SymbolicLink;
	RVMLIB_REC_OBJECT(mvstat);
	mvstat = NORMAL;
	Recov_EndTrans(MAXFP);
    }

    /* Rebuild priority queue. */
    ComputePriority();

    /* Garbage collect data that was in the process of being fetched. */
    if (flags.fetching != 0) {
	FSO_ASSERT(this, HAVEDATA(this));
	eprint("\t(%s, %s) freeing garbage data contents", comp, FID_(&fid));
	Recov_BeginTrans();
	RVMLIB_REC_OBJECT(flags);
	flags.fetching = 0;
	DiscardData();
	Recov_EndTrans(0);
    }

    /* Files that were open for write must be "closed" and discarded. */
    if (flags.owrite != 0) {
	FSO_ASSERT(this, HAVEDATA(this));
	eprint("\t(%s, %s) found owrite object, discarding", comp, FID_(&fid));
	if (IsFile()) {
	    char spoolfile[MAXPATHLEN];
	    int idx = 0;

	    do {
		snprintf(spoolfile,MAXPATHLEN,"%s/%s-%u",SpoolDir,comp,idx++);
	    } while (::access(spoolfile, F_OK) == 0 || errno != ENOENT); 

	    data.file->Copy(spoolfile, NULL, 1);
	    eprint("\t(lost file data backed up to %s)", spoolfile);
	}
	Recov_BeginTrans();
	RVMLIB_REC_OBJECT(flags);
	flags.owrite = 0;
	Recov_EndTrans(0);
	goto Failure;
    }

    /* Get rid of fake objects, and other objects that are not likely to be useful anymore. */
    if ((IsFake() && !LRDB->RFM_IsFakeRoot(&fid)) || flags.backup || flags.rwreplica) {
	goto Failure;
    }

    /* Get rid of a former mount-root whose fid is not a volume root and whose pfid is NullFid */
    if ((mvstat == NORMAL) && !FID_IsVolRoot(&fid) && 
	FID_EQ(&pfid, &NullFid) && !IsLocalObj()) {
	LOG(0, ("fsobj::Recover: (%s) is a non-volume root whose pfid is NullFid\n",
		FID_(&fid)));
	goto Failure;
    }

    /* Check the cache file. */
    switch(stat.VnodeType) {
	case File:
	    {

	    if ((HAVEDATA(this) && cf.IsPartial()) ||
                (!HAVEDATA(this) && cf.Length() != 0)) {
		eprint("\t(%s, %s) cache file validation failed",
		       comp, FID_(&fid));
		goto Failure;
	    }
	    }
	    break;

	case Directory:
	case SymbolicLink:
	    /* 
	     * Reclaim cache-file blocks. Since directory contents are stored
	     * in RVM (as are all fsobjs), cache file blocks for directories 
	     * are thrown out at startup because they are the ``Unix format'' 
	     * version of the object.  The stuff in RVM is the ``Vice format'' 
	     * version. 
	     */
	    if (cf.Length() != 0) {
		FSDB->FreeBlocks(NBLOCKS(cf.Length()));
		cf.Reset();
	    }
	    break;

	case Invalid:
	    CHOKE("fsobj::Recover: bogus VnodeType (%d)", stat.VnodeType);
    }

    if (LogLevel >= 1) print(logFile);
    return;

Failure:
    {
	LOG(0, ("fsobj::Recover: invalid fso (%s, %s), attempting to GC...",
		comp, FID_(&fid)));
	print(logFile);

	/* Scavenge data for dirty, bogus file. */
	/* Note that any store of this file in the CML must be cancelled (in
	 * later step of recovery). */
	if (DIRTY(this)) {
	    if (!IsFile()) {
		print(logFile);
		CHOKE("recovery failed on local, non-file object (%s, %s)",
		    comp, FID_(&fid));
	    }

	    if (HAVEDATA(this)) {
		    Recov_BeginTrans();
		    /* Normally we can't discard dirty files, but here we just
		     * decided that there is no other way. */
		    flags.dirty = 0;
		    DiscardData();
		    Recov_EndTrans(MAXFP);
	    }
	    else {
		/* Reclaim cache-file blocks. */
		if (cf.Length() != 0) {
		    FSDB->FreeBlocks(NBLOCKS(cf.Length()));
		    cf.Reset();
		}
	    }

	    return;
	}

	/* Kill bogus object. */
	/* Caution: Do NOT GC since linked objects may not be valid yet! */
	{
	    /* Reclaim cache-file blocks. */
	    if (cf.Length() != 0) {
		FSDB->FreeBlocks(NBLOCKS(cf.Length()));
		cf.Reset();
	    }

	    Recov_BeginTrans();
	    Kill();
	    Recov_EndTrans(MAXFP);
	}
    }
}


/*  *****  General Status  *****  */

/* MUST be called from within transaction! */
/* Call with object write-locked. */
void fsobj::Matriculate() {
    if (HAVESTATUS(this))
	{ print(logFile); CHOKE("fsobj::Matriculate: HAVESTATUS"); }

    LOG(10, ("fsobj::Matriculate: (%s)\n", FID_(&fid)));

    RVMLIB_REC_OBJECT(state);
    state = FsoNormal;

    /* Notify waiters. */
    FSDB->matriculation_count++;
    VprocSignal(&FSDB->matriculation_sync);	/* OK; we are in transaction, but signal is NO yield */
}


/* Need not be called from within transaction. */
/* Call with object write-locked. */
/* CallBack handler calls this with NoLock (to avoid deadlock)! -JJK */
void fsobj::Demote(void)
{
    if (!HAVESTATUS(this) || DYING(this)) return;
    if (flags.readonly || IsMtPt() || IsFakeMTLink()) return;

    LOG(10, ("fsobj::Demote: fid = (%s)\n", FID_(&fid)));

    ClearRcRights();

    if (IsDir())
	DemoteAcRights(ALL_UIDS);

    DemoteHdbBindings();

    /* Kernel demotion must be severe for non-directories (i.e., purge name- as well as attr-cache) */
    /* because pfid is suspect and the only way to revalidate it is via a cfs_lookup call. -JJK */
    int severely = (!IsDir() || IsFakeDir());
    k_Purge(&fid, severely);
}


/* MUST be called from within transaction! */
/* Call with object write-locked. */
void fsobj::Kill(int TellServers) 
{
	if (DYING(this)) 
		return;

	LOG(10, ("fsobj::Kill: (%s)\n", FID_(&fid)));

	DisableReplacement();
	
	FSDB->delq->append(&del_handle);
	RVMLIB_REC_OBJECT(state);
	state = FsoDying;

	ClearRcRights();
	
	if (IsDir())
		DemoteAcRights(ALL_UIDS);
	
	/* Inform advice servers of loss of availability of this object */
	NotifyUsersOfKillEvent(hdb_bindings, NBLOCKS(stat.Length));
	
	DetachHdbBindings();
	
	k_Purge(&fid, 1);
}


/* MUST be called from within transaction! */
void fsobj::GC() {
	/* Only GC the data now if the file has been locally modified! */
    if (DIRTY(this)) {
	DiscardData();
    }
    else
	delete this;
}


/* MUST NOT be called from within transaction! */
int fsobj::Flush() {
    /* Flush all children first. */
    /* N.B. Recursion here could overflow smallish stacks! */
    if (children != 0) {
	dlist_iterator next(*children);
	dlink *d = next();
	if (d != 0) {
	    do {
		fsobj *cf = strbase(fsobj, d, child_link);
		d = next();
		(void)cf->Flush();
	    } while(d != 0);
	}
    }

    if (!FLUSHABLE(this)) {
	LOG(10, ("fsobj::Flush: (%s) !FLUSHABLE\n", FID_(&fid)));
	Demote();
	return(EMFILE);
    }

    LOG(10, ("fsobj::Flush: flushed (%s)\n", FID_(&fid)));
    Recov_BeginTrans();
    Kill();
    GC();
    Recov_EndTrans(MAXFP);

    return(0);
}


/* MUST be called from within transaction! */
/* Call with object write-locked. */
/* Called as result of {GetAttr, ValidateAttr, GetData, ValidateData}. */
void fsobj::UpdateStatus(ViceStatus *vstat, vuid_t vuid) {
    /* Mount points are never updated. */
    if (IsMtPt())
	{ print(logFile); CHOKE("fsobj::UpdateStatus: IsMtPt!"); }
    /* Fake objects are never updated. */
    if (IsFake())
	{ print(logFile); CHOKE("fsobj::UpdateStatus: IsFake!"); }

    LOG(100, ("fsobj::UpdateStatus: (%s), uid = %d\n", FID_(&fid), vuid));

    if (HAVESTATUS(this)) {		/* {ValidateAttr, GetData, ValidateData} */
	if (!StatusEq(vstat, 0))
	    ReplaceStatus(vstat, 0);
    }
    else {				/* {GetAttr} */
	Matriculate();
	ReplaceStatus(vstat, 0);
    }

    /* Set access rights and parent (if they differ). */
    if (IsDir()) {
	SetAcRights(ALL_UIDS, vstat->AnyAccess);
	SetAcRights(vuid, vstat->MyAccess);
    }
    SetParent(vstat->vparent, vstat->uparent);
}


/* MUST be called from within transaction! */
/* Call with object write-locked. */
/* Called for mutating operations. */
void fsobj::UpdateStatus(ViceStatus *vstat, vv_t *UpdateSet, vuid_t vuid) {
    /* Mount points are never updated. */
    if (IsMtPt())
	{ print(logFile); CHOKE("fsobj::UpdateStatus: IsMtPt!"); }
    /* Fake objects are never updated. */
    if (IsFake())
	{ print(logFile); CHOKE("fsobj::UpdateStatus: IsFake!"); }

    LOG(100, ("fsobj::UpdateStatus: (%s), uid = %d\n", FID_(&fid), vuid));

    /* Install the new status block. */
    if (!StatusEq(vstat, 1))
	/* Ought to Die in this event! */;

    ReplaceStatus(vstat, UpdateSet);

    /* Set access rights and parent (if they differ). */
    /* N.B.  It should be a fatal error if they differ! */
    if (IsDir()) {
	SetAcRights(ALL_UIDS, vstat->AnyAccess);
	SetAcRights(vuid, vstat->MyAccess);
    }
    SetParent(vstat->vparent, vstat->uparent);
}


/* Need not be called from within transaction. */
int fsobj::StatusEq(ViceStatus *vstat, int Mutating) {
    int eq = 1;
    int log = (Mutating || HAVEDATA(this));

    if (stat.Length != (long)vstat->Length) {
	eq = 0;
	if (log)
	    LOG(0, ("fsobj::StatusEq: (%s), Length %d != %d\n",
		    FID_(&fid), stat.Length, vstat->Length));
    }
    /* DataVersion is a non-replicated value and different replicas may
     * legitimately return different dataversions. On a replicated volume we
     * use the VV, and shouldn't use the DataVersion at all. -JH
     */
    if (!flags.replicated && stat.DataVersion != vstat->DataVersion) {
	eq = 0;
	if (log)
	    LOG(0, ("fsobj::StatusEq: (%s), DataVersion %d != %d\n",
		    FID_(&fid), stat.DataVersion, vstat->DataVersion));
    }
    if (flags.replicated && !Mutating && VV_Cmp(&stat.VV, &vstat->VV) != VV_EQ) {
	eq = 0;
	if (log)
	    LOG(0, ("fsobj::StatusEq: (%s), VVs differ\n", FID_(&fid)));
    }
    if (stat.Date != vstat->Date) {
	eq = 0;
	if (log)
	    LOG(0, ("fsobj::StatusEq: (%s), Date %d != %d\n",
		    FID_(&fid), stat.Date, vstat->Date));
    }
    if (stat.Owner != vstat->Owner) {
	eq = 0;
	if (log)
	    LOG(0, ("fsobj::StatusEq: (%s), Owner %d != %d\n",
		    FID_(&fid), stat.Owner, vstat->Owner));
    }
    if (stat.Mode != vstat->Mode) {
	eq = 0;
	if (log)
	    LOG(0, ("fsobj::StatusEq: (%s), Mode %d != %d\n",
		    FID_(&fid), stat.Mode, vstat->Mode));
    }
    if (stat.LinkCount != vstat->LinkCount) {
	eq = 0;
	if (log)
	    LOG(0, ("fsobj::StatusEq: (%s), LinkCount %d != %d\n",
		    FID_(&fid), stat.LinkCount, vstat->LinkCount));
    }
    if (stat.VnodeType != (int)vstat->VnodeType) {
	eq = 0;
	if (log)
	    LOG(0, ("fsobj::StatusEq: (%s), VnodeType %d != %d\n",
		    FID_(&fid), stat.VnodeType, (int)vstat->VnodeType));
    }

    return(eq);
}


/* MUST be called from within transaction! */
/* Call with object write-locked. */
void fsobj::ReplaceStatus(ViceStatus *vstat, vv_t *UpdateSet) {
    RVMLIB_REC_OBJECT(stat);

    /* We're changing the length? 
     * Then the cached data is probably no longer useable! But try to fix up
     * the cachefile so that we can at least give a stale copy. */
    if (HAVEDATA(this) && stat.Length != vstat->Length) {
	LOG(0, ("fsobj::ReplaceStatus: (%s), changed stat.length %d->%d\n",
		FID_(&fid), stat.Length, vstat->Length));
	if (IsFile())
	    LocalSetAttr(-1, vstat->Length, -1, -1, -1);
	SetRcRights(RC_STATUS);
    }

    stat.Length = vstat->Length;
    stat.DataVersion = vstat->DataVersion;
    if (flags.replicated || flags.rwreplica) {
	if (UpdateSet == 0)
	    stat.VV = vstat->VV;
	else {
	    stat.VV.StoreId = vstat->VV.StoreId;
	    AddVVs(&stat.VV, UpdateSet);
	}
    }
    stat.Date = vstat->Date;
    stat.Owner = (vuid_t) vstat->Owner;
    stat.Mode = (short) vstat->Mode;
    stat.LinkCount = (unsigned char) vstat->LinkCount;
    stat.VnodeType = vstat->VnodeType;
}


int fsobj::CheckRcRights(int rights) {
    return((rights & RcRights) == rights);
}


void fsobj::SetRcRights(int rights) {
    if (flags.readonly || flags.backup  || IsFake())
	return;

    LOG(100, ("fsobj::SetRcRights: (%s), rights = %d\n", FID_(&fid), rights));

    if (flags.discread) {
	Recov_BeginTrans();
	RVMLIB_REC_OBJECT(flags);
	flags.discread = 0;
	Recov_EndTrans(MAXFP);
    }

    /* There is a problem if the rights are set that we have valid data,
     * but we actually don't have data yet. */
    FSO_ASSERT(this, !(rights & RC_DATA) ||
	             ((rights & RC_DATA) && HAVEALLDATA(this)));
    RcRights = rights;
}


void fsobj::ClearRcRights() {
    LOG(100, ("fsobj::ClearRcRights: (%s), rights = %d\n",
	      FID_(&fid), RcRights));

    RcRights = NullRcRights;
}


int fsobj::IsValid(int rcrights) {
    int haveit = 0;

    if (rcrights & RC_STATUS) 
        haveit = HAVESTATUS(this); /* (state != FsoRunt) */

    if (!haveit && (rcrights & RC_DATA))  /* should work if both set */
        haveit = HAVEALLDATA(this);

    /* If we don't have the object, it definitely is not valid. */
    if (!haveit) return 0;

    /* Replicated objects must be considered valid when we are either
     * disconnected or write-disconnected and the object is dirty. */
    if (flags.replicated) {
	if (vol->IsDisconnected())		       return 1;
	if (vol->IsWriteDisconnected() && flags.dirty) return 1;
    }

    /* Several other reasons that imply this object is valid */
    if (flags.readonly)		    return 1;
    if (CheckRcRights(rcrights))    return 1;
    if (IsMtPt() || IsFakeMTLink()) return 1;

    /* Now if we still have the volume callback, we can't lose.
     * also update VCB statistics -- valid due to VCB */
    if (vol->HaveCallBack()) {
	vol->VCBHits++;
	return 1;
    }

    /* Final conclusion, the object is not valid */
    return 0;
}


/* Returns {0, EACCES, ENOENT}. */
int fsobj::CheckAcRights(vuid_t vuid, long rights, int validp) {
    if (vuid == ALL_UIDS) {
	/* Do we have access via System:AnyUser? */
	if ((AnyUser.inuse) &&
	    (!validp || AnyUser.valid))
	    return((rights & AnyUser.rights) ? 0 : EACCES);
    }
    else {
	/* Do we have this user's rights in the cache? */
	for (int i = 0; i < CPSIZE; i++) {
	    if ((!SpecificUser[i].inuse) ||
		(validp && !SpecificUser[i].valid))
		continue;

	    if (vuid == SpecificUser[i].uid)
		return((rights & SpecificUser[i].rights) ? 0 : EACCES);
	}
    }

    LOG(10, ("fsobj::CheckAcRights: not found, (%s), (%d, %d, %d)\n",
	      FID_(&fid), vuid, rights, validp));
    return(ENOENT);
}


/* MUST be called from within transaction! */
void fsobj::SetAcRights(vuid_t vuid, long rights) {
    LOG(100, ("fsobj::SetAcRights: (%s), vuid = %d, rights = %d\n",
	       FID_(&fid), vuid, rights));

    if (vuid == ALL_UIDS) {
	if (AnyUser.rights != rights || !AnyUser.inuse) {
	    RVMLIB_REC_OBJECT(AnyUser);
	    AnyUser.rights = (unsigned char) rights;
	    AnyUser.inuse = 1;
	}
	AnyUser.valid = 1;
    }
    else {
	/* Don't record rights if we're really System:AnyUser! */
	userent *ue;
	GetUser(&ue, vuid);
	int tokensvalid = ue->TokensValid();
	PutUser(&ue);
	if (!tokensvalid) return;

	int i;
	int j = -1;
	int k = -1;
	for (i = 0; i < CPSIZE; i++) {
	    if (vuid == SpecificUser[i].uid) break;
	    if (!SpecificUser[i].inuse) j = i;
	    if (!SpecificUser[i].valid) k = i;
	}
	if (i == CPSIZE && j != -1) i = j;
	if (i == CPSIZE && k != -1) i = k;
	if (i == CPSIZE) i = (int) (Vtime() % CPSIZE);

	if (SpecificUser[i].uid != vuid ||
	    SpecificUser[i].rights != rights ||
	    !SpecificUser[i].inuse) {
	    RVMLIB_REC_OBJECT(SpecificUser[i]);
	    SpecificUser[i].uid = vuid;
	    SpecificUser[i].rights = (unsigned char) rights;
	    SpecificUser[i].inuse = 1;
	}
	SpecificUser[i].valid = 1;
    }
}


/* Need not be called from within transaction. */
void fsobj::DemoteAcRights(vuid_t vuid) {
    LOG(100, ("fsobj::DemoteAcRights: (%s), vuid = %d\n", FID_(&fid), vuid));

    if (vuid == ALL_UIDS && AnyUser.valid)
	AnyUser.valid = 0;

    for (int i = 0; i < CPSIZE; i++)
	if ((vuid == ALL_UIDS || SpecificUser[i].uid == vuid) && SpecificUser[i].valid)
	    SpecificUser[i].valid = 0;
}


/* Need not be called from within transaction. */
void fsobj::PromoteAcRights(vuid_t vuid) {
    LOG(100, ("fsobj::PromoteAcRights: (%s), vuid = %d\n", FID_(&fid), vuid));

    if (vuid == ALL_UIDS) {
	AnyUser.valid = 1;

	/* 
	 * if other users who have rights in the cache also have
	 * tokens, promote their rights too. 
	 */
	for (int i = 0; i < CPSIZE; i++)
	    if (SpecificUser[i].inuse && !SpecificUser[i].valid) {
		userent *ue;
		GetUser(&ue, SpecificUser[i].uid);
		int tokensvalid = ue->TokensValid();
		PutUser(&ue);
		if (tokensvalid) SpecificUser[i].valid = 1;
	    }
    } else {
	/* 
	 * Make sure tokens didn't expire for this user while
	 * the RPC was in progress. If we set them anyway, and
	 * he goes disconnected, he may have access to files he 
	 * otherwise wouldn't have because he lost tokens.
	 */
	userent *ue;
	GetUser(&ue, vuid);
	int tokensvalid = ue->TokensValid();
	PutUser(&ue);
	if (!tokensvalid) return;

	for (int i = 0; i < CPSIZE; i++)
	    if (SpecificUser[i].uid == vuid)
		SpecificUser[i].valid = 1;
    }
}


/* MUST be called from within transaction! */
void fsobj::ClearAcRights(vuid_t vuid) {
    LOG(100, ("fsobj::ClearAcRights: (%s), vuid = %d\n", FID_(&fid), vuid));

    if (vuid == ALL_UIDS) {
	RVMLIB_REC_OBJECT(AnyUser);
	AnyUser = NullAcRights;
    }

    for (int i = 0; i < CPSIZE; i++)
	if (vuid == ALL_UIDS || SpecificUser[i].uid == vuid) {
	    RVMLIB_REC_OBJECT(SpecificUser[i]);
	    SpecificUser[i] = NullAcRights;
	}
}


/* local-repair modification */
/* MUST be called from within transaction (at least if <vnode, unique> != pfid.<Vnode, Unique>)! */
void fsobj::SetParent(VnodeId vnode, Unique_t unique) {
    if (IsRoot() || (vnode == 0 && unique == 0) || LRDB->RFM_IsGlobalRoot(&fid))
	return;

    /* Update pfid if necessary. */
    if (pfid.Vnode != vnode || pfid.Unique != unique) {
	/* Detach from old parent if necessary. */
	if (pfso != 0) {
	    pfso->DetachChild(this);
	    pfso = 0;
	}

	/* Install new parent fid. */
	RVMLIB_REC_OBJECT(pfid);
	pfid.Volume = fid.Volume;
	pfid.Vnode = vnode;
	pfid.Unique = unique;
    }

    /* Attach to new parent if possible. */
    if (pfso == 0) {
	fsobj *pf = FSDB->Find(&pfid);
	if (pf != 0 && HAVESTATUS(pf) && !GCABLE(pf)) {
	    pfso = pf;
	    pfso->AttachChild(this);
	}
    }
}


/* MUST be called from within transaction! */
void fsobj::MakeDirty() {
    if (DIRTY(this)) return;

    LOG(1, ("fsobj::MakeDirty: (%s)\n", FID_(&fid)));

    /* We must have data here */
    /* Not really, we could have just created this object while disconnected */
    /* CODA_ASSERT(HAVEALLDATA(this)); */

    RVMLIB_REC_OBJECT(flags);
    flags.dirty = 1;
    RVMLIB_REC_OBJECT(CleanStat);
    CleanStat.Length = stat.Length;
    CleanStat.Date = stat.Date;

    DisableReplacement();
}


/* MUST be called from within transaction! */
void fsobj::MakeClean() {
    if (!DIRTY(this)) return;

    LOG(1, ("fsobj::MakeClean: (%s)\n", FID_(&fid)));

    RVMLIB_REC_OBJECT(flags);
    flags.dirty = 0;

    EnableReplacement();
}


/*  *****  Mount State  *****  */
/* local-repair modification */
/* MUST NOT be called from within transaction! */
/* Call with object write-locked. */
int fsobj::TryToCover(ViceFid *inc_fid, vuid_t vuid) {
    if (!HAVEALLDATA(this))
	{ print(logFile); CHOKE("fsobj::TryToCover: called without data"); }

    LOG(10, ("fsobj::TryToCover: fid = (%s)\n", FID_(&fid)));

    int code = 0;

    /* Don't cover mount points in backup volumes! */
    if (flags.backup)
	return(ENOENT);

    /* Check for bogosities. */
    int len = (int) stat.Length;
    if (len < 2) {
	eprint("TryToCover: bogus link length");
	return(EINVAL);
    }
    char type = data.symlink[0];
    switch(type) {
	case '%':
	    eprint("TryToCover: '%'-style mount points no longer supported");
	    return(EOPNOTSUPP);

	case '#':
	case '@':
	    break;

	default:
	    eprint("TryToCover: bogus mount point type (%c)", type);
	    return(EINVAL);
    }

    /* Look up the volume that is to be mounted on us. */
    volent *tvol = 0;
    if (IsFake()) {
	VolumeId tvid;
	if (sscanf(data.symlink, "@%lx.%*x.%*x", &tvid) != 1)
	    { print(logFile); CHOKE("fsobj::TryToCover: couldn't get tvid"); }
	code = VDB->Get(&tvol, tvid);
    }
    else {
	/* Turn volume name into a proper string. */
	data.symlink[len - 1] = 0;				/* punt transaction! */
	code = VDB->Get(&tvol, &data.symlink[1]);
    }
    if (code != 0) {
/*
	 eprint("TryToCover(%s) failed (%d)", data.symlink, code);
*/
	LOG(100, ("fsobj::TryToCover: vdb::Get(%s) failed (%d)\n", data.symlink, code));
	return(code);
    }

    /* Don't allow a volume to be mounted inside itself! */
    /* but only when its mount root is the global-root-obj of a local subtree */
    if ((fid.Volume == tvol->vid) && !LRDB->RFM_IsGlobalChild(&fid)) {
	eprint("TryToCover(%s): recursive mount!", data.symlink);
	VDB->Put(&tvol);
	return(ELOOP);
    }

    /* Don't allow backup vols in backup vols (e.g., avoid OldFiles/OldFiles
     * problem).
     * Isn't this already dealt with by not covering mountpoints in backup
     * volumes? --JH */
    if (tvol->IsBackup()) {
	if (vol->IsBackup()) {
	    eprint("Mount of BU volume (%s) detected inside BU volume (%s)",
		   tvol->name, vol->name);
	    VDB->Put(&tvol);
	    return(ELOOP);
	}
    }

    /* Get volume root. */
    fsobj *rf = 0;
    ViceFid root_fid;
    root_fid.Volume = tvol->vid;
    if (IsFake()) {
	if (sscanf(data.symlink, "@%*x.%lx.%lx", &root_fid.Vnode, &root_fid.Unique) != 2)
	    { print(logFile); CHOKE("fsobj::TryToCover: couldn't get <tvolid, tunique>"); }
    }
    else {
	    FID_MakeRoot(&root_fid);
    }
    code = FSDB->Get(&rf, &root_fid, vuid, RC_STATUS, comp);
    if (code != 0) {
	LOG(100, ("fsobj::TryToCover: Get root (%s) failed (%d)\n",
		  FID_(&root_fid), code));

	VDB->Put(&tvol);
	if (code == EINCONS && inc_fid != 0) *inc_fid = root_fid;
	return(code);
    }
    rf->PromoteLock();

    /* If root is currently mounted, uncover the mount point and unmount. */
    Recov_BeginTrans();
    fsobj *mf = rf->u.mtpoint;
    if (mf != 0) {
	    if (mf == this) {
		    eprint("TryToCover: re-mounting (%s) on (%s)", tvol->name, comp);
		    UncoverMtPt();
	    } else {
		    if (mf->u.root != rf)
			    { mf->print(logFile); rf->print(logFile); CHOKE("TryToCover: mf->root != rf"); }
		    mf->UncoverMtPt();
	    }
	    rf->UnmountRoot();
    }
    Recov_EndTrans(MAXFP);

    /* Do the mount magic. */
    Recov_BeginTrans();
    if (IsFake() && rf->mvstat != ROOT) {
	    RVMLIB_REC_OBJECT(rf->mvstat);
	    RVMLIB_REC_OBJECT(rf->u.mtpoint);
	    rf->mvstat = ROOT;
	    rf->u.mtpoint = 0;
    }
    rf->MountRoot(this);
    CoverMtPt(rf);
    Recov_EndTrans(MAXFP);

    FSDB->Put(&rf);
    VDB->Put(&tvol);
    return(0);
}


/* MUST be called from within transaction! */
/* Call with object write-locked. */
void fsobj::CoverMtPt(fsobj *root_fso) {
    if (mvstat != NORMAL)
	{ print(logFile); CHOKE("fsobj::CoverMtPt: mvstat != NORMAL"); }
    if (!data.symlink)
	{ print(logFile); CHOKE("fsobj::CoverMtPt: no data.symlink!"); }

    LOG(10, ("fsobj::CoverMtPt: fid = (%s), rootfid = (%s)\n",
	     FID_(&fid), FID_2(&root_fso->fid)));

    RVMLIB_REC_OBJECT(*this);

    /* Exit old state (NORMAL). */
    k_Purge(&fid, 1);

    /* Enter new state (MOUNTPOINT). */
    stat.VnodeType = Directory;
    mvstat = MOUNTPOINT;
    u.root = root_fso;
    DisableReplacement();
}


/* MUST be called from within transaction! */
/* Call with object write-locked. */
void fsobj::UncoverMtPt() {
    if (mvstat != MOUNTPOINT) 
	{ print(logFile); CHOKE("fsobj::UncoverMtPt: mvstat != MOUNTPOINT"); }
    if (!u.root)
	{ print(logFile); CHOKE("fsobj::UncoverMtPt: no u.root!"); }

    LOG(10, ("fsobj::UncoverMtPt: fid = (%s), rootfid = (%s)\n",
	      FID_(&fid), FID_2(&u.root->fid)));

    RVMLIB_REC_OBJECT(*this);

    /* Exit old state (MOUNTPOINT). */
    u.root = 0;
    k_Purge(&fid, 1);			/* I don't think this is necessary. */
    k_Purge(&pfid, 1);			/* This IS necessary. */

    /* Enter new state (NORMAL). */
    stat.VnodeType = SymbolicLink;
    mvstat = NORMAL;
    EnableReplacement();
}


/* MUST be called from within transaction! */
/* Call with object write-locked. */
void fsobj::MountRoot(fsobj *mtpt_fso) {
    if (mvstat != ROOT)
	{ print(logFile); CHOKE("fsobj::MountRoot: mvstat != ROOT"); }
    if (u.mtpoint)
	{ print(logFile); CHOKE("fsobj::MountRoot: u.mtpoint exists!"); }

    LOG(10, ("fsobj::MountRoot: fid = %s, mtptfid = %s\n",
	     FID_(&fid), FID_2(&mtpt_fso->fid)));

    RVMLIB_REC_OBJECT(*this);

    /* Exit old state (ROOT, without link). */
    k_Purge(&fid);

    /* Enter new state (ROOT, with link). */
    u.mtpoint = mtpt_fso;
}


/* MUST be called from within transaction! */
/* Call with object write-locked. */
void fsobj::UnmountRoot() {
    if (mvstat != ROOT) 
	{ print(logFile); CHOKE("fsobj::UnmountRoot: mvstat != ROOT"); }
    if (!u.mtpoint)
	{ print(logFile); CHOKE("fsobj::UnmountRoot: no u.mtpoint!"); }

    LOG(10, ("fsobj::UnmountRoot: fid = (%s), mtptfid = (%s)\n",
	      FID_(&fid), FID_2(&u.mtpoint->fid)));

    RVMLIB_REC_OBJECT(*this);

    /* Exit old state (ROOT, with link). */
    u.mtpoint = 0;
    k_Purge(&fid);

    /* Enter new state (ROOT, without link). */
    if (!FID_IsVolRoot(&fid)) {
	mvstat = NORMAL;	    /* couldn't be mount point, could it? */       
	/* this object could be the global root of a local/global subtree */
	if (FID_EQ(&pfid, &NullFid) && !IsLocalObj()) {
	    LOG(0, ("fsobj::UnmountRoot: (%s) a previous mtroot without pfid, kill it\n",
		    FID_(&fid)));
	    Kill();
	}
    }
}


/*  *****  Child/Parent Linkage  *****  */

/* Need not be called from within transaction. */
void fsobj::AttachChild(fsobj *child) {
    if (!IsDir())
	{ print(logFile); child->print(logFile); CHOKE("fsobj::AttachChild: not dir"); }

    LOG(100, ("fsobj::AttachChild: (%s), (%s)\n",
	       FID_(&fid), FID_2(&child->fid)));

    DisableReplacement();

    if (*((dlink **)&child->child_link) != 0)
	{ print(logFile); child->print(logFile); CHOKE("fsobj::AttachChild: bad child"); }
    if (children == 0)
	children = new dlist;
    children->prepend(&child->child_link);

    DemoteHdbBindings();	    /* in case an expansion would now be satisfied! */
}


/* Need not be called from within transaction. */
void fsobj::DetachChild(fsobj *child) {
    if (!IsDir())
	{ print(logFile); child->print(logFile); CHOKE("fsobj::DetachChild: not dir"); }

    LOG(100, ("fsobj::DetachChild: (%s), (%s)\n",
	       FID_(&fid), FID_2(&child->fid)));

    DemoteHdbBindings();	    /* in case an expansion would no longer be satisfied! */

    if (child->pfso != this || *((dlink **)&child->child_link) == 0 ||
	 children == 0 || children->count() == 0)
	{ print(logFile); child->print(logFile); CHOKE("fsobj::DetachChild: bad child"); }
    if (children->remove(&child->child_link) != &child->child_link)
	{ print(logFile); child->print(logFile); CHOKE("fsobj::DetachChild: remove failed"); }

    EnableReplacement();
}


/*  *****  Priority State  *****  */

/* Need not be called from within transaction. */
void fsobj::Reference() {
    LOG(100, ("fsobj::Reference: (%s), old = %d, new = %d\n",
	       FID_(&fid), FSDB->LastRef[ix], FSDB->RefCounter));

    FSDB->LastRef[ix] = FSDB->RefCounter++;
}

/* local-repair modification */
/* Need not be called from within transaction. */
void fsobj::ComputePriority() {
    LOG(1000, ("fsobj::ComputePriority: (%s)\n", FID_(&fid)));

    if (IsLocalObj()) {
	LOG(1000, ("fsobj::ComputePriority: local object\n"));
	return;
    }
    FSDB->Recomputes++;

    int new_priority = 0;
    {
	/* Short-term priority (spri) is a function of how recently object was used. */
	/* Define "spread" to be the difference between the most recent */
	/* reference to any object and the most recent reference to this object. */
	/* Let "rank" be a function which scales "spread" over 0 to FSO_MAX_SPRI - 1. */
	/* Then, spri(f) :: FSO_MAX_SPRI - rank(spread(f)) */
	int spri = 0;
	int LastRef = (int) FSDB->LastRef[ix];
	if (LastRef > 0) {
	    int spread = (int) FSDB->RefCounter - LastRef - 1;
	    int rank = spread;
	    {
		/* "rank" depends upon FSO_MAX_SPRI, fsdb::MaxFiles, and a scaling factor. */
		static int initialized = 0;
		static int RightShift;
		if (!initialized) {
#define	log2(x)\
    (ffs(binaryfloor((x) + (x) - 1) - 1))
		    int LOG_MAXFILES = log2(FSDB->MaxFiles);
		    int	LOG_SSF = log2(FSDB->ssf);
		    int LOG_MAX_SPRI = log2(FSO_MAX_SPRI);
		    RightShift = (LOG_MAXFILES + LOG_SSF - LOG_MAX_SPRI);
		    initialized = 1;
		}
		if (RightShift > 0) rank = spread >> RightShift;
		else if (RightShift < 0) rank = spread << (-RightShift);
		if (rank >= FSO_MAX_SPRI) rank = FSO_MAX_SPRI - 1;
	    }
	    spri = FSO_MAX_SPRI - rank;
	}

	/* Medium-term priority (mpri) is just the current Hoard priority. */
	int mpri = HoardPri;

	new_priority = FSDB->MakePri(spri, mpri);
    }

    if (priority == -1 || new_priority != priority) {
	FSDB->Reorders++;		    /* transient value; punt set_range */

	DisableReplacement();		    /* remove... */
	priority = new_priority;	    /* update key... */
	EnableReplacement();		    /* reinsert... */
    }
}


/* local-repair modification */
/* Need not be called from within transaction. */
void fsobj::EnableReplacement() {
#ifdef	VENUSDEBUG
    /* Sanity checks. */
/*
    if (DYING(this)) {
	if (*((dlink **)&del_handle) == 0)
	    { print(logFile); CHOKE("fsobj::EnableReplacement: dying && del_handle = 0"); }
	return;
    }
*/
    if ((!REPLACEABLE(this) && prio_handle.tree() != 0) ||
	 (REPLACEABLE(this) && prio_handle.tree() != FSDB->prioq))
	{ print(logFile); CHOKE("fsobj::EnableReplacement: state != link"); }
#endif	VENUSDEBUG

    /* Already replaceable? */
    if (REPLACEABLE(this))
	return;

    /* Are ALL conditions for replaceability met? */
    if (DYING(this) || !HAVESTATUS(this) || DIRTY(this) ||
	 READING(this) || WRITING(this) || (children && children->count() > 0) ||
	 IsMtPt() || (IsSymLink() && hdb_bindings && hdb_bindings->count() > 0))
	return;

    /* Sanity check. */
    if (priority == -1 && !IsLocalObj())
	eprint("EnableReplacement(%s): priority unset", FID_(&fid));

    LOG(1000,("fsobj::EnableReplacement: (%s), priority = [%d (%d) %d %d]\n",
	      FID_(&fid), priority, flags.random, HoardPri, FSDB->LastRef[ix]));

#ifdef	VENUSDEBUG
    if (LogLevel >= 10000)
	FSDB->prioq->print(logFile);
#endif	VENUSDEBUG

    FSDB->prioq->insert(&prio_handle);
    flags.replaceable = 1;

#ifdef	VENUSDEBUG
    if (LogLevel >= 10000 && !(FSDB->prioq->IsOrdered()))
	{ print(logFile); FSDB->prioq->print(logFile); CHOKE("fsobj::EnableReplacement: !IsOrdered after insert"); }
#endif	VENUSDEBUG
}


/* Need not be called from within transaction. */
void fsobj::DisableReplacement() {
#ifdef	VENUSDEBUG
    /* Sanity checks. */
/*
    if (DYING(this)) {
	if (*((dlink **)&del_handle) == 0)
	    { print(logFile); CHOKE("fsobj::DisableReplacement: dying && del_handle = 0"); }
	return;
    }
*/
    if ((!REPLACEABLE(this) && prio_handle.tree() != 0) ||
	 (REPLACEABLE(this) && prio_handle.tree() != FSDB->prioq))
	{ print(logFile); CHOKE("fsobj::DisableReplacement: state != link"); }
#endif	VENUSDEBUG

    /* Already not replaceable? */
    if (!REPLACEABLE(this))
	return;

    LOG(1000,("fsobj::DisableReplacement: (%s), priority = [%d (%d) %d %d]\n",
	      FID_(&fid), priority, flags.random, HoardPri, FSDB->LastRef[ix]));

#ifdef	VENUSDEBUG
    if (LogLevel >= 10000)
	FSDB->prioq->print(logFile);
#endif	VENUSDEBUG

    flags.replaceable = 0;
    if (FSDB->prioq->remove(&prio_handle) != &prio_handle)
	{ print(logFile); CHOKE("fsobj::DisableReplacement: prioq remove"); }

#ifdef	VENUSDEBUG
    if (LogLevel >= 10000 && !(FSDB->prioq->IsOrdered()))
	{ print(logFile); FSDB->prioq->print(logFile); CHOKE("fsobj::DisableReplacement: !IsOrdered after remove"); }
#endif	VENUSDEBUG
}


binding *CheckForDuplicates(dlist *hdb_bindings_list, void *binder)
{
    /* If the list is empty, this can't be a duplicate */
    if (hdb_bindings_list == NULL)
        return(0);

    /* Look for this binder */
    dlist_iterator next(*hdb_bindings_list);
    dlink *d;
    while ((d = next())) {
	binding *b = strbase(binding, d, bindee_handle);
	if (b->binder == binder) {
	  /* Found it! */
	  return(b);
	}
    }

    /* If we had found it, we wouldn't have gotten here! */
    return(NULL);
}

/* Need not be called from within transaction. */
void fsobj::AttachHdbBinding(binding *b)
{
    binding *dup;

    /* Sanity checks. */
    if (b->bindee != 0) {
	print(logFile);
	b->print(logFile);
	CHOKE("fsobj::AttachHdbBinding: bindee != 0");
    }

    /* Check for duplicates */
    if ((dup = CheckForDuplicates(hdb_bindings, b->binder))) {
	dup->IncrRefCount();
	LOG(100, ("This is a duplicate binding...skip it.\n"));
	return;
    }

    if (LogLevel >= 1000) {
	dprint("fsobj::AttachHdbBinding:\n");
	print(logFile);
    }

    /* Attach ourselves to the binding. */
    if (!hdb_bindings)
	hdb_bindings = new dlist;
    hdb_bindings->insert(&b->bindee_handle);
    b->bindee = this;

    if (LogLevel >= 10) {
      dprint("fsobj::AttachHdbBinding:\n");
      print(logFile);
      b->print(logFile);
    }

    if (IsSymLink())
	DisableReplacement();

    /* Recompute our priority if necessary. */
    namectxt *nc = (namectxt *)b->binder;
    if (nc->priority > HoardPri) {
	HoardPri = nc->priority;
	HoardVuid = nc->vuid;
	ComputePriority();
    }
}


/* Need not be called from within transaction. */
void fsobj::DemoteHdbBindings() {
    if (hdb_bindings == 0) return;

    dlist_iterator next(*hdb_bindings);
    dlink *d;
    while ((d = next())) {
	binding *b = strbase(binding, d, bindee_handle);
	DemoteHdbBinding(b);
    }
}


/* Need not be called from within transaction. */
void fsobj::DemoteHdbBinding(binding *b) {
    /* Sanity checks. */
    if (b->bindee != this) {
	print(logFile);
	if (b != 0) b->print(logFile);
	CHOKE("fsobj::DemoteHdbBinding: bindee != this");
    }
    if (LogLevel >= 1000) {
	dprint("fsobj::DemoteHdbBinding:\n");
	print(logFile);
	b->print(logFile);
    }

    /* Update the state of the binder. */
    namectxt *nc = (namectxt *)b->binder;
    nc->Demote();
}


/* Need not be called from within transaction. */
void fsobj::DetachHdbBindings() {
    if (hdb_bindings == 0) return;

    dlink *d;
    while ((d = hdb_bindings->first())) {
	binding *b = strbase(binding, d, bindee_handle);
	DetachHdbBinding(b, 1);
    }
}


/* Need not be called from within transaction. */
void fsobj::DetachHdbBinding(binding *b, int DemoteNameCtxt) {
  struct timeval StartTV;
  struct timeval EndTV;
  float elapsed;

    /* Sanity checks. */
    if (b->bindee != this) {
	print(logFile);
	if (b != 0) b->print(logFile);
	CHOKE("fsobj::DetachHdbBinding: bindee != this");
    }
    if (LogLevel >= 1000) {
	dprint("fsobj::DetachHdbBinding:\n");
	print(logFile);
	b->print(logFile);
    }

    /* Detach ourselves from the binding. */
    if (hdb_bindings->remove(&b->bindee_handle) != &b->bindee_handle)
	{ print(logFile); b->print(logFile); CHOKE("fsobj::DetachHdbBinding: bindee remove"); }
    b->bindee = 0;
    if (IsSymLink() && hdb_bindings->count() == 0)
	EnableReplacement();

    /* Update the state of the binder. */
    namectxt *nc = (namectxt *)b->binder;
    if (DemoteNameCtxt)
	nc->Demote();

    /* Recompute our priority if necessary. */
    if (nc->priority == HoardPri) {
	int new_HoardPri = 0;
	vuid_t new_HoardVuid = HOARD_UID;
    gettimeofday(&StartTV, 0);
    LOG(10, ("Detach: hdb_binding list contains %d namectxts\n", hdb_bindings->count()));
	dlist_iterator next(*hdb_bindings);
	dlink *d;
	while ((d = next())) {
	    binding *b = strbase(binding, d, bindee_handle);
	    namectxt *nc = (namectxt *)b->binder;
	    if (nc->priority > new_HoardPri) {
		new_HoardPri = nc->priority;
		new_HoardVuid = nc->vuid;
	    }
	}
    gettimeofday(&EndTV, 0);
    elapsed = SubTimes(&EndTV, &StartTV);
    LOG(10, ("fsobj::DetachHdbBinding: recompute, elapsed= %3.1f\n", elapsed));

	if (new_HoardPri < HoardPri) {
	    HoardPri = new_HoardPri;
	    HoardVuid = new_HoardVuid;
	    ComputePriority();
	}
    }
}

/*
 * This routine attempts to automatically decide whether or not the hoard
 * daemon should fetch an object.  There are three valid return values:
 *	-1: the object should definitely NOT be fetched
 *	 0: the routine cannot automatically determine the fate of this 
 *	    object; the user should be given the option
 *	 1: the object should definitely be fetched
 *
 * As a first guess to the "real" function, we will use 
 * 		ALPHA + (BETA * e^(priority/GAMMA)).
 * An object's priority ranges as high as 100,000 (use formula in Jay's thesis 
 * but instead use 75 for the value of alpha and 25 for the value of 1-alpha).
 * This is (presumably) to keep all of the priorities integers.
 * 
 */
int fsobj::PredetermineFetchState(int estimatedCost, int hoard_priority) {
    double acceptableCost;
    double x;

    if (estimatedCost == -1)
        return(0);

    /* Scale it up correctly... from range 1-1000 to 1-100000 */
    hoard_priority = hoard_priority * 100;

    LOG(100, ("fsobj::PredetermineFetchState(%d)\n",estimatedCost));
    LOG(100, ("PATIENCE_ALPHA = %d, PATIENCE_BETA = %d; PATIENCE_GAMMA = %d\n", 
	      PATIENCE_ALPHA, PATIENCE_BETA, PATIENCE_GAMMA));
    LOG(100, ("priority = %d; HoardPri = %d, hoard_priority = %d\n",
	      priority, HoardPri, hoard_priority));

    x = (double)hoard_priority / (double)PATIENCE_GAMMA;
    acceptableCost = (double)PATIENCE_ALPHA + ((double)PATIENCE_BETA * exp(x));

    if ((hoard_priority == 100000) || (estimatedCost < acceptableCost)) {
        LOG(100, ("fsobj::PredetermineFetchState returns 1 (definitely fetch) \n"));
        return(1);
    }
    else {
	LOG(100, ("fsobj::PredetermineFetchState returns 0 (ask the user) \n"));
        return(0);
    }
}

CacheMissAdvice 
fsobj::ReadDisconnectedCacheMiss(vproc *vp, vuid_t vuid) {
    userent *u;
    char pathname[MAXPATHLEN];
    CacheMissAdvice advice;

    LOG(100, ("E fsobj::ReadDisconnectedCacheMiss\n"));

    GetUser(&u, vuid);
    CODA_ASSERT(u != NULL);

    /* If advice not enabled, simply return */
    if (!AdviceEnabled) {
        LOG(100, ("ADMON STATS:  RDCM Advice NOT enabled.\n"));
        u->AdviceNotEnabled();
        return(FetchFromServers);
    }

    /* Check that:                                                     *
     *     (a) the request did NOT originate from the Hoard Daemon     *
     *     (b) the request did NOT originate from that AdviceMonitor,  *
     * and (c) the user is running an AdviceMonitor,                   */
    CODA_ASSERT(vp != NULL);
    if (vp->type == VPT_HDBDaemon) {
	LOG(100, ("ADMON STATS:  RDCM Advice inappropriate.\n"));
        return(FetchFromServers);
    }
    if (u->IsAdvicePGID(vp->u.u_pgid)) {
        LOG(100, ("ADMON STATS:  RDCM Advice inappropriate.\n"));
        return(FetchFromServers);
    }
    if (u->IsAdviceValid(ReadDisconnectedCacheMissEventID,1) != TRUE) {
        LOG(100, ("ADMON STATS:  RDCM Advice NOT valid. (uid = %d)\n", vuid));
        return(FetchFromServers);
    }

    GetPath(pathname, 1);

    LOG(100, ("Requesting ReadDisconnected CacheMiss Advice for path=%s, pid=%d...\n", pathname, vp->u.u_pid));
    advice = u->RequestReadDisconnectedCacheMissAdvice(&fid, pathname, vp->u.u_pgid);
    return(advice);
}

CacheMissAdvice fsobj::WeaklyConnectedCacheMiss(vproc *vp, vuid_t vuid) {
    userent *u;
    char pathname[MAXPATHLEN];
    CacheMissAdvice advice;
    unsigned long CurrentBandwidth;

    LOG(100, ("E fsobj::WeaklyConnectedCacheMiss\n"));

    GetUser(&u, vuid);
    CODA_ASSERT(u != NULL);

    /* If advice not enabled, simply return */
    if (!AdviceEnabled) {
        LOG(100, ("ADMON STATS:  WCCM Advice NOT enabled.\n"));
        u->AdviceNotEnabled();
        return(FetchFromServers);
    }

    /* Check that:                                                     *
     *     (a) the request did NOT originate from the Hoard Daemon     *
     *     (b) the request did NOT originate from that AdviceMonitor,  *
     * and (c) the user is running an AdviceMonitor,                   */
    CODA_ASSERT(vp != NULL);
    if (vp->type == VPT_HDBDaemon) {
	LOG(100, ("ADMON STATS:  WCCM Advice inappropriate.\n"));
        return(FetchFromServers);
    }
    if (u->IsAdvicePGID(vp->u.u_pgid)) {
        LOG(100, ("ADMON STATS:  WCCM Advice inappropriate.\n"));
        return(FetchFromServers);
    }
    if (u->IsAdviceValid(WeaklyConnectedCacheMissEventID, 1) != TRUE) {
        LOG(100, ("ADMON STATS:  WCCM Advice NOT valid. (uid = %d)\n", vuid));
        return(FetchFromServers);
    }

    GetPath(pathname, 1);

    LOG(100, ("Requesting WeaklyConnected CacheMiss Advice for path=%s, pid=%d...\n", 
	      pathname, vp->u.u_pid));
    vol->vsg->GetBandwidth(&CurrentBandwidth);
    advice = u->RequestWeaklyConnectedCacheMissAdvice(&fid, pathname, vp->u.u_pid, stat.Length, 
						      CurrentBandwidth, cf.Name());
    return(advice);
}

void fsobj::DisconnectedCacheMiss(vproc *vp, vuid_t vuid, char *comp)
{
    userent *u;
    char pathname[MAXPATHLEN];

    GetUser(&u, vuid);
    CODA_ASSERT(u != NULL);

    /* If advice not enabled, simply return */
    if (!AdviceEnabled) {
        LOG(100, ("ADMON STATS:  DMQ Advice NOT enabled.\n"));
        u->AdviceNotEnabled();
        return;
    }

    /* Check that:                                                     *
     *     (a) the request did NOT originate from the Hoard Daemon     *
     *     (b) the request did NOT originate from that AdviceMonitor,  *
     *     (c) the user is running an AdviceMonitor,                   *
     * and (d) the volent is non-NULL.                                 */
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
        LOG(100, ("ADMON STATS:  DMQ Advice NOT valid. (uid = %d)\n", vuid));
        return;
    }
    if (vol == NULL) {
        LOG(100, ("ADMON STATS:  DMQ volent is NULL.\n"));
        u->VolumeNull();
        return;
    }

    /* Get the pathname */
    GetPath(pathname, 1);

    if (comp) {
        strcat(pathname, "/");
	strcat(pathname,comp);
    }

    /* Make the request */
    LOG(100, ("Requesting Disconnected CacheMiss Questionnaire...1\n"));
    u->RequestDisconnectedQuestionnaire(&fid, pathname, vp->u.u_pid, vol->GetDisconnectionTime());
}




/*  *****  MLE Linkage  *****  */

/* MUST be called from within transaction! */
void fsobj::AttachMleBinding(binding *b) {
    /* Sanity checks. */
    if (b->bindee != 0) {
	print(logFile);
	b->print(logFile);
	CHOKE("fsobj::AttachMleBinding: bindee != 0");
    }
    if (LogLevel >= 1000) {
	dprint("fsobj::AttachMleBinding:\n");
	print(logFile);
	b->print(logFile);
    }

    /* Attach ourselves to the binding. */
    if (mle_bindings == 0)
	mle_bindings = new dlist;
    mle_bindings->append(&b->bindee_handle);
    b->bindee = this;

    /* Set our "dirty" flag if this is the first binding. (i.e. this fso has an mle) */
    if (mle_bindings->count() == 1) {
	MakeDirty();
    }
    else {
	FSO_ASSERT(this, DIRTY(this));
    }
}


/* MUST be called from within transaction! */
void fsobj::DetachMleBinding(binding *b) {
    /* Sanity checks. */
    if (b->bindee != this) {
	print(logFile);
	if (b != 0) b->print(logFile);
	CHOKE("fsobj::DetachMleBinding: bindee != this");
    }
    if (LogLevel >= 1000) {
	dprint("fsobj::DetachMleBinding:\n");
	print(logFile);
	b->print(logFile);
    }
    FSO_ASSERT(this, DIRTY(this));
    FSO_ASSERT(this, mle_bindings != 0);

    /* Detach ourselves from the binding. */
    if (mle_bindings->remove(&b->bindee_handle) != &b->bindee_handle)
	{ print(logFile); b->print(logFile); CHOKE("fsobj::DetachMleBinding: bindee remove"); }
    b->bindee = 0;

    /* Clear our "dirty" flag if this was the last binding. */
    if (mle_bindings->count() == 0) {
	MakeClean();
    }
}


/* MUST NOT be called from within transaction! */
void fsobj::CancelStores() {
    if (!DIRTY(this))
	{ print(logFile); CHOKE("fsobj::CancelStores: !DIRTY"); }

    vol->CancelStores(&fid);
}


/*  *****  Data Contents  *****  */

/* MUST be called from within transaction! */
/* Call with object write-locked. */
/* If there are readers of this file, they will have it change from underneath them! */
void fsobj::DiscardData() {
    if (!HAVEDATA(this))
	{ print(logFile); CHOKE("fsobj::DiscardData: !HAVEDATA"); }
    if (WRITING(this) || EXECUTING(this))
	{ print(logFile); CHOKE("fsobj::DiscardData: WRITING || EXECUTING"); }

    LOG(10, ("fsobj::DiscardData: (%s)\n", FID_(&fid)));

    CODA_ASSERT(!flags.dirty);

    RVMLIB_REC_OBJECT(data);
    switch(stat.VnodeType) {
	case File:
	    {
	    /* stat.Length() might have been changed, only data.file->Length()
	     * can be trusted */
	    FSDB->FreeBlocks(NBLOCKS(data.file->Length()));
	    data.file->Truncate(0);
	    data.file = 0;
	    }
	    break;

	case Directory:
	    {
	    /* Mount points MUST be unmounted before their data can be discarded! */
	    FSO_ASSERT(this, !IsMtPt());

	    /* Return cache-file blocks associated with Unix-format directory. */
	    if (data.dir->udcf) {
		FSDB->FreeBlocks(NBLOCKS(data.dir->udcf->Length()));
		data.dir->udcf->Truncate(0);
		data.dir->udcf = 0;
		data.dir->udcfvalid = 0;
	    }

	    /* Get rid of RVM data. */
	    DH_FreeData(&data.dir->dh);
	    rvmlib_rec_free(data.dir);
	    data.dir = 0;

	    break;
	    }
	case SymbolicLink:
	    {
	    /* Get rid of RVM data. */
	    rvmlib_rec_free(data.symlink);
	    data.symlink = 0;
	    }
	    break;

	case Invalid:
	    CHOKE("fsobj::DiscardData: bogus VnodeType (%d)", stat.VnodeType);
    }
}


/*  *****  Fake Object Management  *****  */

/* local-repair modification */
/* MUST NOT be called from within transaction! */
/* Transform a fresh fsobj into a fake directory or MTLink. */
/* Call with object write-locked. */
/* returns 0 if successful, ENOENT if the parent cannot
   be found. */
int fsobj::Fakeify() {
    LOG(1, ("fsobj::Fakeify: %s, (%s)\n", comp, FID_(&fid)));

    fsobj *pf = 0;
    if (!IsRoot()) {
	/* Laboriously scan database to find our parent! */
	fso_vol_iterator next(NL, vol);
	while ((pf = next())) {
	    if (!pf->IsDir() || pf->IsMtPt()) continue;
	    if (!HAVEALLDATA(pf)) continue;
	    if (!pf->dir_IsParent(&fid)) continue;

	    /* Found! */
	    break;
	}
	if (pf == 0) {
	    LOG(0, ("fsobj::Fakeify: %s, (%s), parent not found\n",
		    comp, FID_(&fid)));
	    return(ENOENT);
	}
    }

    // Either (pf == 0 and this is the volume root) OR (pf != 0 and it isn't)

    Recov_BeginTrans();
    RVMLIB_REC_OBJECT(*this);
    
    flags.fake = 1;
    if (!IsRoot())  // If we're not the root, pf != 0
	    pf->AttachChild(this);

    unsigned long volumehosts[VSG_MEMBERS];
    srvent *s;
    int i;
    if (FID_IsFakeRoot(&fid)) {		/* Fake MTLink */
	    /* Initialize status. */
	    stat.DataVersion = 1;
	    stat.Mode = 0644;
	    stat.Owner = V_UID;
	    stat.Length = 27; /* "@XXXXXXXX.YYYYYYYY.ZZZZZZZZ" */
	    stat.Date = Vtime();
	    stat.LinkCount = 1;
	    stat.VnodeType = SymbolicLink;
	    Matriculate();
	    pfid = pf->fid;
	    pfso = pf;

	    /* local-repair modification */
	    if (!strcmp(comp, "local")) {
		/* the first specical case, fake link for a local object */
		LOG(100,("fsobj::Fakeify: fake link for a local object %s\n",
			 FID_(&fid)));
		LOG(100,("fsobj::Fakeify: parent fid for the fake link is %s\n",
			 FID_(&pfid)));
		flags.local = 1;
		ViceFid *LocalFid = LRDB->RFM_LookupLocalRoot(&pfid);
		FSO_ASSERT(this, LocalFid);
		/* Write out the link contents. */
		data.symlink = (char *)rvmlib_rec_malloc((unsigned) stat.Length);
		rvmlib_set_range(data.symlink, stat.Length);
		sprintf(data.symlink, "@%08lx.%08lx.%08lx", LocalFid->Volume, 
			LocalFid->Vnode, LocalFid->Unique);
		LOG(100, ("fsobj::Fakeify: making %s a symlink %s\n",
			  FID_(&fid), data.symlink));
	    } else {
		if (!strcmp(comp, "global")) {
		    /* the second specical case, fake link for a global object */
		    LOG(100, ("fsobj::Fakeify: fake link for a global object %s\n",
			      FID_(&fid)));
		    LOG(100, ("fsobj::Fakeify: parent fid for the fake link is %s\n",
			      FID_(&pfid)));
		    flags.local = 1;
		    ViceFid *GlobalFid = LRDB->RFM_LookupGlobalRoot(&pfid);
		    FSO_ASSERT(this, GlobalFid);
		    /* Write out the link contents. */
		    data.symlink = (char *)rvmlib_rec_malloc((unsigned) stat.Length);
		    rvmlib_set_range(data.symlink, stat.Length);
		    sprintf(data.symlink, "@%08lx.%08lx.%08lx",
			    GlobalFid->Volume, GlobalFid->Vnode,
			    GlobalFid->Unique);
		    LOG(100, ("fsobj::Fakeify: making %s a symlink %s\n",
			      FID_(&fid), data.symlink));
		} else {
		    /* the normal fake link */
		    /* get the volumeid corresponding to the server name */
		    /* A big assumption here is that the host order in the
		       server array returned by GetHosts is the
		       same as the volume id order in the vdb.*/
		    vol->vsg->GetHosts(volumehosts);
		    /* find the server first */
		    for (i = 0; i < VSG_MEMBERS; i++) {
			if (volumehosts[i] == 0) continue;
			if ((s = FindServer(volumehosts[i])) &&
			    (s->name)) {
			    if (!strcmp(s->name, comp)) break;
			}
			else {
			    unsigned long vh = atol(comp);
			    if (vh == volumehosts[i]) break;
			}
		    }
		    if (i == VSG_MEMBERS) 
		      // server not found 
		      CHOKE("fsobj::fakeify couldn't find the server for %s\n", 
			    comp);
		    unsigned long rwvolumeid = vol->u.rep.RWVols[i];
		    
		    /* Write out the link contents. */
		    data.symlink = (char *)rvmlib_rec_malloc((unsigned) stat.Length);
		    rvmlib_set_range(data.symlink, stat.Length);
		    sprintf(data.symlink, "@%08lx.%08lx.%08lx", rwvolumeid, pfid.Vnode,
			    pfid.Unique);
		    LOG(1, ("fsobj::Fakeify: making %s a symlink %s\n",
			    FID_(&fid), data.symlink));
		}
	    }
	    UpdateCacheStats(&FSDB->FileDataStats, CREATE, BLOCKS(this));
	}
	else {				/* Fake Directory */
	    /* Initialize status. */
	    stat.DataVersion = 1;
	    stat.Mode = 0444;
	    stat.Owner = V_UID;
	    stat.Length = 0;
	    stat.Date = Vtime();
	    stat.LinkCount = 2;
	    stat.VnodeType = Directory;
	    /* Access rights are not needed! */
	    Matriculate();
	    if (pf != 0) {
		pfid = pf->fid;
		pfso = pf;
	    }

	    /* Create the target directory. */
	    dir_MakeDir();
	    stat.Length = dir_Length();
	    UpdateCacheStats(&FSDB->DirDataStats, CREATE, BLOCKS(this));

	    /* Make entries for each of the rw-replicas. */
	    vol->vsg->GetHosts(volumehosts);
	    for (i = 0; i < VSG_MEMBERS; i++) {
		if (volumehosts[i] == 0) continue;
		srvent *s;
		char Name[CODA_MAXNAMLEN];
		if ((s = FindServer(volumehosts[i])) &&
		    (s->name))
		    sprintf(Name, "%s", s->name);
		else
		    sprintf(Name, "%08lx", volumehosts[i]);
		ViceFid FakeFid = vol->GenerateFakeFid();
		LOG(1, ("fsobj::Fakeify: new entry (%s, %s)\n",
			Name, FID_(&FakeFid)));
		dir_Create(Name, &FakeFid);
	    }
	}

    Reference();
    ComputePriority();
    Recov_EndTrans(CMFP);

    return(0);
}


/*  *****  Local Synchronization  *****  */

void fsobj::Lock(LockLevel level) {
    LOG(1000, ("fsobj::Lock: (%s) level = %s\n",
	       FID_(&fid), ((level == RD)?"RD":"WR")));

    if (level != RD && level != WR)
	{ print(logFile); CHOKE("fsobj::Lock: bogus lock level %d", level); }

    FSO_HOLD(this);
    while (level == RD ? (writers > 0) : (writers > 0 || readers > 0))
    {
	LOG(0, ("WAITING(%s): level = %s, readers = %d, writers = %d\n",
		FID_(&fid), lvlstr(level), readers, writers));
	START_TIMING();
	VprocWait(&fso_sync);
	END_TIMING();
	LOG(0, ("WAIT OVER, elapsed = %3.1f\n", elapsed));
    }
    level == RD ? (readers++) : (writers++);
}


void fsobj::PromoteLock() {
    FSO_HOLD(this);
    UnLock(RD);
    Lock(WR);
    FSO_RELE(this);
}


void fsobj::DemoteLock() {
    FSO_HOLD(this);
    UnLock(WR);
    Lock(RD);
    FSO_RELE(this);
}


void fsobj::UnLock(LockLevel level) {
    LOG(1000, ("fsobj::UnLock: (%s) level = %s\n",
	       FID_(&fid), ((level == RD)?"RD":"WR")));

    if (level != RD && level != WR)
	{ print(logFile); CHOKE("fsobj::UnLock: bogus lock level %d", level); }

    if (refcnt <= 0)
	{ print(logFile); CHOKE("fsobj::UnLock: refcnt <= 0"); }
    (level == RD) ? (readers--) : (writers--);
    if (readers < 0 || writers < 0)
	{ print(logFile); CHOKE("fsobj::UnLock: readers = %d, writers = %d", readers, writers); }
    if (level == RD ? (readers == 0) : (writers == 0))
	VprocSignal(&fso_sync);
    FSO_RELE(this);
}


/*  *****  Miscellaneous Utility Routines  *****  */

void fsobj::GetVattr(struct coda_vattr *vap) {
    /* Most attributes are derived from the VenusStat structure. */
    vap->va_type = FTTOVT(stat.VnodeType);
    vap->va_mode = stat.Mode ;

    vap->va_uid = (uid_t) stat.Owner;
    vap->va_gid = (vgid_t)V_GID;

    vap->va_fileid = (IsRoot() && u.mtpoint && !IsVenusRoot())
		       ? FidToNodeid(&u.mtpoint->fid)
		       : FidToNodeid(&fid);

    vap->va_nlink = stat.LinkCount;
    vap->va_size = (u_quad_t) stat.Length;
    vap->va_blocksize = V_BLKSIZE;
    vap->va_mtime.tv_sec = (time_t)stat.Date;
    vap->va_mtime.tv_nsec = 0;

    vap->va_atime = vap->va_mtime;
    vap->va_ctime.tv_sec = 0;
    vap->va_ctime.tv_nsec = 0;
    vap->va_flags = 0;
    vap->va_rdev = 1;
    vap->va_bytes = vap->va_size;

    /* If the object is currently open for writing we must physically 
       stat it to get its size and time info. */
    if (WRITING(this)) {
	struct stat tstat;
	cf.Stat(&tstat);

	vap->va_size = tstat.st_size;
	vap->va_mtime.tv_sec = tstat.st_mtime;
	vap->va_mtime.tv_nsec = 0;
	vap->va_atime = vap->va_mtime;
	vap->va_ctime.tv_sec = 0;
	vap->va_ctime.tv_nsec = 0;
    }

    VPROC_printvattr(vap);
}


void fsobj::ReturnEarly() {
    /* Only mutations on replicated objects can return early. */
    if (!flags.replicated) return;

    /* Only makes sense to return early to user requests. */
    vproc *v = VprocSelf();
    if (v->type != VPT_Worker) return;

    /* Oh man is this ugly. Why is this here and not in worker? -- DCS */
    /* Assumption: the opcode and unique fields of the w->msg->msg_ent are already filled in */
    worker *w = (worker *)v;
    switch (w->opcode) {
	union outputArgs *out;
	case CODA_CREATE:
	case CODA_MKDIR:
	    {	/* create and mkdir use exactly the same sized output structure */
	    if (w->msg == 0) CHOKE("fsobj::ReturnEarly: w->msg == 0");

	    out = (union outputArgs *)w->msg->msg_buf;
	    out->coda_create.oh.result = 0;
	    out->coda_create.VFid = fid;
	    DemoteLock();
	    GetVattr(&out->coda_create.attr);
	    PromoteLock();
	    w->Return(w->msg, sizeof (struct coda_create_out));
	    break;
	    }

	case CODA_CLOSE:
	    {
	    /* Don't return early here if we already did so in a callback handler! */
	    if (!(flags.era && FID_EQ(&w->StoreFid, &NullFid)))
		w->Return(0);
	    break;
	    }

	case CODA_IOCTL:
	    {
	    /* Huh. IOCTL in the kernel thinks there may be return data. Assume not. */
	    out = (union outputArgs *)w->msg->msg_buf;
	    out->coda_ioctl.len = 0; 
	    out->coda_ioctl.oh.result = 0;
	    w->Return(w->msg, sizeof (struct coda_ioctl_out));
	    break;
	    }

	case CODA_LINK:
	case CODA_REMOVE:
	case CODA_RENAME:
	case CODA_RMDIR:
	case CODA_SETATTR:
	case CODA_SYMLINK:
	    w->Return(0);
	    break;

	default:
	    CHOKE("fsobj::ReturnEarly: bogus opcode (%d)", w->opcode);
    }
}


/* Need not be called from within transaction! */
void fsobj::GetPath(char *buf, int fullpath) {
    if (IsRoot()) {
	if (!fullpath)
	    { strcpy(buf, "."); return; }

	if (fid.Volume == rootfid.Volume)
	    { strcpy(buf, venusRoot); return; }

	if (u.mtpoint == 0)
	    { strcpy(buf, "???"); return; }

	u.mtpoint->GetPath(buf, fullpath);
	return;
    }

    if (pfso == 0 && !FID_EQ(&pfid, &NullFid)) {
	fsobj *pf = FSDB->Find(&pfid);
	if (pf != 0 && HAVESTATUS(pf) && !GCABLE(pf)) {
	    pfso = pf;
	    pfso->AttachChild(this);
	}
    }

    if (pfso != 0)
	pfso->GetPath(buf, fullpath);
    else
	strcpy(buf, "???");
    strcat(buf, "/");
    strcat(buf, comp);
}


/* Virginal state may cause some access checks to be avoided. */
int fsobj::IsVirgin() {
    int virginal = 0;
    int i;

    if (flags.replicated) {
	for (i = 0; i < VSG_MEMBERS; i++)
	    if ((&(stat.VV.Versions.Site0))[i] > 1) break;
	if (i == VSG_MEMBERS) virginal = 1;

	/* If file is dirty, it's really virginal only if there are no stores in the CML! */
	if (virginal && IsFile() && DIRTY(this)) {
	    cml_iterator next(vol->CML, CommitOrder, &fid);
	    cmlent *m;
	    while ((m = next()))
		if (m->opcode == OLDCML_NewStore_OP)
		    break;
	    if (m != 0) virginal = 0;
	}
    }
    else {
	if (stat.DataVersion == 0) virginal = 1;
    }

    return(virginal);
}


int fsobj::MakeShadow()
{
    int err = 0;
    /*
     * Create a shadow, using a name both distinctive and that will
     * be garbage collected at startup.
     */
    if (!shadow) shadow = new CacheFile(-(ix+1));
    else	 shadow->IncRef();

    if (!shadow) return -1;

    /* As we only call MakeShadow during the freezing, and there is only one
     * reintegration at a time, we can sync the shadow copy with the lastest
     * version of the real file. -JH */
    /* As an optimization (to avoid blocking the reintegration too much) we
     * might want to do this only when we just created the shadow file or when
     * there are no writers to the real container file... Maybe later. -JH */
    Lock(RD);
    err = cf.Copy(shadow);
    UnLock(RD);

    return(err);
}


void fsobj::RemoveShadow()
{
    if (shadow->DecRef() == 0)
    {
	delete shadow;
	shadow = 0;
    }
}


/* Only call this on directory objects (or mount points)! */
/* Locking is irrelevant, but this routine MUST NOT yield! */
void fsobj::CacheReport(int fd, int level) {
    FSO_ASSERT(this, IsDir());

    /* Indirect over mount points. */
    if (IsMtPt()) {
	u.mtpoint->CacheReport(fd, level);
	return;
    }

    /* Report [slots, blocks] for this directory and its children. */
    int slots = 0;
    int blocks = 0;
    if (children != 0) {
	/* N.B. Recursion here could overflow smallish stacks! */
	dlist_iterator next(*children);
	dlink *d;
	while ((d = next())) {
	    fsobj *cf = strbase(fsobj, d, child_link);

	    slots++;
	    blocks += NBLOCKS(cf->cf.Length());
	}
    }
    fdprint(fd, "[ %3d  %5d ]      ", slots, blocks);
    for (int i = 0; i < level; i++) fdprint(fd, "   ");
    fdprint(fd, "%s\n", comp);

    /* Report child directories. */
    if (children != 0) {
	/* N.B. Recursion here could overflow smallish stacks! */
	dlist_iterator next(*children);
	dlink *d;
	while ((d = next())) {
	    fsobj *cf = strbase(fsobj, d, child_link);

	    if (cf->IsDir())
		cf->CacheReport(fd, level + 1);
	}
    }
}

/* 
 * This is a simple-minded routine that estimates the cost of fetching an
 * object.  It assumes that the fsobj has a reasonable estimate as to the 
 * size of the object stored in stat.Length.
 *
 * The routine takes one argument -- whether the status block (type == 0) 
 * or the actual data (type == 1) is being fetched.  The default is data.
 */
int fsobj::EstimatedFetchCost(int type) {
    LOG(100, ("E fsobj::EstimatedFetchCost(%d)\n", type));

    unsigned long bw;	/* bandwidth, in bytes/sec */
    vol->vsg->GetBandwidth(&bw);

    LOG(100, ("stat.Length = %d; Bandwidth = %d\n", stat.Length, bw));
    LOG(100, ("EstimatedFetchCost = %d\n", (int)stat.Length/bw));

    return( (int)stat.Length/bw ); 
}

void fsobj::RecordReplacement(int status, int data) {
    char mountpath[MAXPATHLEN];
    char path[MAXPATHLEN];

    LOG(0, ("RecordReplacement(%d,%d)\n", status, data));

    CODA_ASSERT(vol != NULL);
    vol->GetMountPath(mountpath, 0);
    GetPath(path, 1);    

    if (data)
      NotifyUserOfReplacement(&fid, path, status, 1);
    else
      NotifyUserOfReplacement(&fid, path, status, 0);

    LOG(0, ("RecordReplacement complete.\n"));
}

/* local-repair modification */
void fsobj::print(int fdes) {
    /* < address, fid, comp, vol > */
    fdprint(fdes, "%#08x : fid = (%s), comp = %s, vol = %x\n",
	     (long)this, FID_(&fid), comp, vol);

    /* < FsoState, VenusStat, Replica Control Rights, Access Rights, flags > */
    fdprint(fdes, "\tstate = %s, stat = { %d, %d, %d, %d, %#o, %d, %s }, rc rights = %d\n",
	     PrintFsoState(state), stat.Length, stat.DataVersion,
	     stat.Date, stat.Owner, stat.Mode, stat.LinkCount,
	     PrintVnodeType(stat.VnodeType), RcRights);
    fdprint(fdes, "\tVV = {[");
    for (int i = 0; i < VSG_MEMBERS; i++)
	fdprint(fdes, " %d", (&(stat.VV.Versions.Site0))[i]);
    fdprint(fdes, " ] [ %#x %d ] [ %#x ]}\n",
	     stat.VV.StoreId.Host, stat.VV.StoreId.Uniquifier, stat.VV.Flags);
    if (IsDir()) {
	fdprint(fdes, "\tac rights = { [%x %x%x]",
		AnyUser.rights, AnyUser.inuse, AnyUser.valid);
	for (int i = 0; i < CPSIZE; i++)
	    fdprint(fdes, " [%d %x %x%x]",
		    SpecificUser[i].uid, SpecificUser[i].rights,
		    SpecificUser[i].inuse, SpecificUser[i].valid);
	fdprint(fdes, " }\n");
    }
    fdprint(fdes, "\tvoltype = [%d %d %d %d], ucb = %d, fake = %d, fetching = %d local = %d\n",
	     flags.backup, flags.readonly, flags.replicated, flags.rwreplica,
	     flags.usecallback, flags.fake, flags.fetching, flags.local);
    fdprint(fdes, "\trep = %d, data = %d, owrite = %d, era = %d, dirty = %d, shadow = %d\n",
	     flags.replaceable, HAVEDATA(this), flags.owrite, flags.era, flags.dirty,
	     shadow != 0);

    /* < mvstat [rootfid | mtptfid] > */
    fdprint(fdes, "\tmvstat = %s", PrintMvStat(mvstat));
    if (IsMtPt())
	fdprint(fdes, ", root = (%s)", FID_(&u.root->fid));
    if (IsRoot() && u.mtpoint)
	fdprint(fdes, ", mtpoint = (%s)", FID_(&u.mtpoint->fid));
    fdprint(fdes, "\n");

    /* < parent_fid, pfso, child count > */
    fdprint(fdes, "\tparent = (%s, %x), children = %d\n",
	     FID_(&pfid), pfso, (children ? children->count() : 0));

    /* < priority, HoardPri, HoardVuid, hdb_bindings, LastRef > */
    fdprint(fdes, "\tpriority = %d (%d), hoard = [%d, %d, %d], lastref = %d\n",
	     priority, flags.random, HoardPri, HoardVuid,
	     (hdb_bindings ? hdb_bindings->count() : 0), FSDB->LastRef[ix]);
    if (hdb_bindings) {
      dlist_iterator next(*hdb_bindings);
      dlink *d;
      while ((d = next())) {
	binding *b = strbase(binding, d, bindee_handle);
	namectxt *nc = (namectxt *)b->binder;
	if (nc != NULL) 
	  nc->print(fdes);
      }

    }

    fdprint(fdes, "\tDisconnectionStatistics:  Used = %d - Unused = %d - SinceLastUse = %d\n",
	    DisconnectionsUsed, DisconnectionsUnused, DisconnectionsSinceUse);

    /* < mle_bindings, CleanStat > */
    fdprint(fdes, "\tmle_bindings = (%x, %d), cleanstat = [%d, %d]\n",
	     mle_bindings, (mle_bindings ? mle_bindings->count() : 0),
	     CleanStat.Length, CleanStat.Date);

    /* < cachefile, [directory | symlink] contents > */
    fdprint(fdes, "\tcachefile = ");
    cf.print(fdes);
    if (IsDir() && !IsMtPt()) {
	if (data.dir == 0) {
	    fdprint(fdes, "\tdirectory = 0\n");
	}
	else {
	    int pagecount = -1;
	    fdprint(fdes, "\tdirectory = %x, udcf = [%x, %d]\n",
		    data.dir, data.dir->udcf, data.dir->udcfvalid);
	    fdprint(fdes, "\tpages = %d, malloc bitmap = [ ", pagecount);
	    fdprint(fdes, "data at %p ", DH_Data(&(data.dir->dh)));
	    fdprint(fdes, "]\n");
	}
    }
    if (IsSymLink() || IsMtPt()) {
	fdprint(fdes, "\tlink contents: %s\n",
		data.symlink ? data.symlink : "N/A");
    }

    /* < references, openers > */
    fdprint(fdes, "\trefs = [%d %d %d], openers = [%d %d %d]",
	     readers, writers, refcnt,
	     (openers - Writers - Execers), Writers, Execers);
    fdprint(fdes, "\tlastresolved = %u\n", lastresolved);
    fdprint(fdes, "\tdiscread = %d\n", flags.discread);
}

void fsobj::ListCache(FILE *fp, int long_format, unsigned int valid)
{
  /* list in long format, if long_format == 1;
     list fsobjs
          such as valid (valid == 1), non-valid (valid == 2) or all (valid == 3) */

  char path[MAXPATHLEN];
  GetPath(path, 0);		/* Get relative pathname. */    

  switch (valid) {
  case 1: /* only valid */
    if ( DATAVALID(this) && STATUSVALID(this) )
      if (!long_format)
	ListCacheShort(fp);
      else
	ListCacheLong(fp);
    break;
  case 2: /* only non-valid */
    if ( !DATAVALID(this) || !STATUSVALID(this) )
      if (!long_format)
	ListCacheShort(fp);
      else
	ListCacheLong(fp);
    break;
  case 3: /* all */
  default:
      if (!long_format)
	ListCacheShort(fp);
      else
	ListCacheLong(fp);
  }
}

void fsobj::ListCacheShort(FILE* fp)
{
  char path[MAXPATHLEN];
  GetPath(path, 0);		/* Get relative pathname. */
  char valid = ((DATAVALID(this) && STATUSVALID(this) && !DIRTY(this)) ? ' ' : '*');

  fprintf(fp, "%c %s\n", valid, path);
  fflush(fp);
}

void fsobj::ListCacheLong(FILE* fp)
{
  char path[MAXPATHLEN];
  GetPath(path, 0);		/* Get relative pathname. */    
  char valid = ((DATAVALID(this) && STATUSVALID(this) && !DIRTY(this)) ? ' ' : '*');
  char type = ( IsDir() ? 'd' : ( IsSymLink() ? 's' : 'f') );
  char linktype = ' ';
  if ( type != 'f' )
    linktype = (IsMtPt() ? 'm' :
		(IsMTLink() ? 'l' :
		 (IsRoot() ? '/' :
		  (IsVenusRoot() ? 'v': ' '))));

  fprintf(fp, "%c %c%c %s  %s\n", valid, type, linktype, path, FID_(&fid));	
  fflush(fp);
}


/* *****  Iterator  ***** */

fso_iterator::fso_iterator(LockLevel level, ViceFid *key) : rec_ohashtab_iterator(FSDB->htab, key) {
    clevel = level;
    cvol = 0;
}

/* Returns entry locked as specified. */
fsobj *fso_iterator::operator()() {
    for (;;) {
	rec_olink *o = rec_ohashtab_iterator::operator()();
	if (!o) return(0);

	fsobj *f = strbase(fsobj, o, primary_handle);
	if (cvol == 0 || cvol == f->vol) {
	    if (clevel != NL) f->Lock(clevel);
	    return(f);
	}
    }
}

/* for iteration by volume */
fso_vol_iterator::fso_vol_iterator(LockLevel level, volent *vol) : olist_iterator(*vol->fso_list) {
    clevel = level;
}

fsobj *fso_vol_iterator::operator()() {
    olink *o = olist_iterator::operator()();
    if (!o) return(0);

    fsobj *f = strbase(fsobj, o, vol_handle);
    if (clevel != NL) f->Lock(clevel);
    return(f);
}

void fsobj::GetOperationState(int *conn, int *tid)
{
    if (HOARDING(this)) {
	*conn = 1;
	*tid = 0;
	return;
    }
    if (EMULATING(this)) {
	*conn = 0;
	*tid = -1;
	return;
    }

    OBJ_ASSERT(this, LOGGING(this));
    /* 
     * check to see if the object is within the global portion of a subtree
     * that is currently being repaired. (only when a repair session is on)
     */
    *tid = -1;
    int repair_mutation = 0;
    if (LRDB->repair_root_fid != NULL) {
	fsobj *cfo = this;
	while (cfo != NULL) {
	    if (cfo->IsLocalObj())
	      break;
	    if (cfo->IsRoot())
	      cfo = cfo->u.mtpoint;
	    else
	      cfo = cfo->pfso;
	}
	if ((cfo != NULL) && (!bcmp((const void *)&(cfo->pfid), (const void *)LRDB->repair_root_fid, (int)sizeof(ViceFid)) ||
            !bcmp((const void *)&(cfo->pfid), (const void *)LRDB->RFM_FakeRootToParent(LRDB->repair_root_fid), (int)sizeof(ViceFid)))) {
	    repair_mutation = 1;
	}
    }
    if (repair_mutation) {
	*tid = LRDB->repair_session_tid;
	if (LRDB->repair_session_mode == REP_SCRATCH_MODE) {
	    *conn = 0;
	} else {
	    *conn = 1;
	}
    } else {
	*conn = 0;
    }
}
