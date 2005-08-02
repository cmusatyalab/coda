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
 *    Implementation of the Venus File-System Object (fso) abstraction.
 *
 *    ToDo:
 *       1. Need to allocate meta-data by priority (escpecially in the case of dir pages and modlog entries)
 */


/* Following block is shared with worker.c. */
/* It is needed to ensure that C++ makes up "anonymous types" in the same order.  It sucks! */
#ifdef __cplusplus
extern "C" {
#endif

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
#endif

/* interfaces */

/* from vicedep */
#include <venusioctl.h>

/* from venus */
#include "advice.h"
#include "adv_monitor.h"
#include "adv_daemon.h"
#include "cml.h"
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
#include "realmdb.h"


static int NullRcRights = 0;
static AcRights NullAcRights = { ANYUSER_UID, 0, 0, 0 };

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
fsobj::fsobj(VenusFid *key, char *name) : cf() {
    LOG(10, ("fsobj::fsobj: fid = (%s), comp = %s\n", FID_(key),
	     name == NULL ? "(no name)" : name));

    RVMLIB_REC_OBJECT(*this);
    ResetPersistent();
    fid = *key;
    SetComp(name);
    if (FID_IsVolRoot(&fid))
	mvstat = ROOT;
    ResetTransient();

    Lock(WR);

    /* Insert into hash table. */
    (FSDB->htab).append(&fid, &primary_handle);
}

/* local-repair modification */
/* MUST be called from within transaction! */
/* Caller sets range for whole object! */
void fsobj::ResetPersistent()
{
    MagicNumber = FSO_MagicNumber;
    fid = NullFid;
    if (comp) {
	rvmlib_rec_free(comp);
	comp = NULL;
    }
    vol = 0;
    state = FsoRunt;
    stat.VnodeType = Invalid;
    stat.LinkCount = (unsigned char)-1;
    stat.Length = 0;
    stat.DataVersion = 0;
    stat.VV = NullVV;
    stat.VV.StoreId.Host = NO_HOST;
    stat.Date = (Date_t)-1;
    stat.Author = ANYUSER_UID;
    stat.Owner = ANYUSER_UID;
    stat.Mode = (unsigned short)-1;
    ClearAcRights(ANYUSER_UID);
    flags.fake = 0;
    flags.owrite = 0;
    flags.dirty = 0;
    flags.local = 0;
    flags.expanded = 0;
    flags.modified = 0;
    mvstat = NORMAL;
    pfid = NullFid;
    CleanStat.Length = (unsigned long)-1;
    CleanStat.Date = (Date_t)-1;
    data.havedata = 0;
    memset(VenusSHA, 0, SHA_DIGEST_LENGTH);
}

/* local-repair modification */
/* Needn't be called from within transaction. */
void fsobj::ResetTransient()
{
    /* Sanity checks. */
    if (MagicNumber != FSO_MagicNumber)
	{ print(logFile); CHOKE("fsobj::ResetTransient: bogus MagicNumber"); }

    /* This is a horrible way of resetting handles! */
    list_head_init(&vol_handle);
    memset((void *)&prio_handle, 0, (int)sizeof(prio_handle));
    memset((void *)&del_handle, 0, (int)sizeof(del_handle));
    memset((void *)&owrite_handle, 0, (int)sizeof(owrite_handle));

    if (HAVEDATA(this) && IsDir()) {
	data.dir->udcfvalid = 0;
	data.dir->udcf = 0;
    }
    ClearRcRights();
    DemoteAcRights(ANYUSER_UID);
    flags.ckmtpt = 0;
    flags.fetching = 0;
    flags.random = ::random();

    memset((void *)&u, 0, (int)sizeof(u));

    pfso = 0;
    children = 0;
    child_link.clear();

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

    lastresolved = 0;

    /* Link to volume, and initialize volume specific members. */
    if ((vol = VDB->Find(MakeVolid(&fid))) == 0) {
	print(logFile);
	CHOKE("fsobj::ResetTransient: couldn't find volume");
    }

    /* Add to volume list */
    list_add(&vol_handle, &vol->fso_list);

    if (IsLocalObj()) {
	/* set valid RC status for local object */
	SetRcRights(RC_DATA | RC_STATUS);
    }
}


/* MUST be called from within transaction! */
fsobj::~fsobj() {
    RVMLIB_REC_OBJECT(*this);

#ifdef	VENUSDEBUG
    /* Sanity check. */
    if (!GCABLE(this))
	{ print(logFile); CHOKE("fsobj::~fsobj: !GCABLE"); }
#endif /* VENUSDEBUG */

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

	    /* If this is an expanded directory, delete all associated
	     * mountlinks since they are no longer useful! - Adam 5/17/05*/
	    if (IsExpandedDir())
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
		if (mtpt_fso->u.root == this)
		    mtpt_fso->UncoverMtPt();
		UnmountRoot();
	    }
	    break;

	default:
	    print(logFile);
	    CHOKE("fsobj::~fsobj: bogus mvstat");
    }

    /* Release data. */
    if (HAVEDATA(this))
	DiscardData();

    /* Remove from volume's fso list */
    list_del(&vol_handle);

    /* Unlink from volume. */
    VDB->Put(&vol);

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
    if (comp) {
	rvmlib_rec_free(comp);
	comp = NULL;
    }
}

void fsobj::operator delete(void *deadobj) {

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
	    if(IsFake()) /* Don't toss away our localcache! */
	      break;

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
    if (IsMtPt()) {
	Recov_BeginTrans();

	/* XXX this can probably be removed */
	RVMLIB_REC_OBJECT(stat.VnodeType);
	stat.VnodeType = SymbolicLink;
	/* XXX */

	RVMLIB_REC_OBJECT(mvstat);
	mvstat = NORMAL;
	Recov_EndTrans(MAXFP);
    }

    /* Rebuild priority queue. */
    ComputePriority();

    /* Garbage collect data that was in the process of being fetched. */
    if (flags.fetching) {
	FSO_ASSERT(this, HAVEDATA(this));
	eprint("\t(%s, %s) freeing garbage data contents", comp, FID_(&fid));
	Recov_BeginTrans();
	RVMLIB_REC_OBJECT(flags);
	flags.fetching = 0;

    /* Could we trust the value of 'validdata' which was stored in RVM?
     * there is no real way of telling when exactly we crashed and whether
     * the datablocks hit the disk. The whole recovery seems to make the
     * assumption that this is true, however... I'm hesitant to remove the
     * call to 'DiscardData' here. It is probably ok when venus crashed, but
     * probably not when the OS died. --JH */
	DiscardData();
	Recov_EndTrans(0);
    }

    /* Files that were open for write must be "closed" and discarded. */
    if (flags.owrite) {
	FSO_ASSERT(this, HAVEDATA(this));
	eprint("\t(%s, %s) found owrite object, discarding", comp, FID_(&fid));
	if (IsFile()) {
	    char spoolfile[MAXPATHLEN];
	    int idx = 0;

	    do {
		snprintf(spoolfile,MAXPATHLEN,"%s/%s-%u",SpoolDir,GetComp(),idx++);
	    } while (::access(spoolfile, F_OK) == 0 || errno != ENOENT); 

	    data.file->Copy(spoolfile, 1);
	    eprint("\t(lost file data backed up to %s)", spoolfile);
	}
	Recov_BeginTrans();
	RVMLIB_REC_OBJECT(flags);
	flags.owrite = 0;
	Recov_EndTrans(0);
	goto Failure;
    }

    if (!IsFake() && !vol->IsReplicated() && !IsLocalObj()) {
	LOG(0, ("fsobj::Recover: (%s) is probably in a backup volume\n",
		FID_(&fid)));
	goto Failure;
    }

    if(IsFake()) {
      k_Purge(&fid, 1);
    }

    /* Get rid of a former mount-root whose fid is not a volume root and whose
     * pfid is NullFid */
    if (IsNormal() && !FID_IsVolRoot(&fid) && 
	FID_EQ(&pfid, &NullFid) && !IsLocalObj()) {
	LOG(0, ("fsobj::Recover: (%s) is a non-volume root whose pfid is NullFid\n",
		FID_(&fid)));
	goto Failure;
    }

    /* Check the cache file. */
    if(!IsFake()) {
      switch(stat.VnodeType) {
        case File:
	    if (!HAVEDATA(this) && cf.Length() != 0) {
		eprint("\t(%s, %s) cache file validation failed",
		       comp, FID_(&fid));
		FSDB->FreeBlocks(NBLOCKS(cf.Length()));
		cf.Reset();
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
    }

    if (LogLevel >= 1) print(logFile);
    return;

Failure:
    {
	LOG(0, ("fsobj::Recover: invalid fso (%s, %s), attempting to GC...\n",
		comp, FID_(&fid)));
	print(logFile);

        /* Scavenge data for bogus objects. */
        /* Note that any store of this file in the CML must be cancelled (in
         * later step of recovery). */
        {
	    if (HAVEDATA(this)) {
                Recov_BeginTrans();
                /* Normally we can't discard dirty files, but here we just
                 * decided that there is no other way. */
                flags.dirty = 0;
                DiscardData();
                Recov_EndTrans(MAXFP);
	    }
            if (cf.Length()) {
                /* Reclaim cache-file blocks. */
                FSDB->FreeBlocks(NBLOCKS(cf.Length()));
		cf.Reset();
	    }
        }

	/* Kill bogus object. */
	/* Caution: Do NOT GC since linked objects may not be valid yet! */
	{
	    Recov_BeginTrans();
	    Kill();
	    Recov_EndTrans(MAXFP);
	}
    }
}


/*  *****  General Status  *****  */

/* MUST be called from within transaction! */
/* Call with object write-locked. */
void fsobj::Matriculate()
{
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
/* >> Demote should not yield or destroy the object << */
void fsobj::Demote(void)
{
    if (!HAVESTATUS(this) || DYING(this)) return;
    //if (IsMtPt() || IsFakeMTLink()) return;
    if (IsFakeMTLink() || IsExpandedMTLink()) return;

    LOG(10, ("fsobj::Demote: fid = (%s)\n", FID_(&fid)));

    ClearRcRights();

    if (IsDir())
	DemoteAcRights(ANYUSER_UID);

    DemoteHdbBindings();

    /* Kernel demotion must be severe for non-directories (i.e., purge name- as well as attr-cache) */
    /* because pfid is suspect and the only way to revalidate it is via a cfs_lookup call. -JJK */
    int severely = (!IsDir() || IsFakeDir() || IsExpandedDir()); /* Adam 5/17/05 */
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

	/* Inform advice servers of loss of availability of this object */
	/* NotifyUsersOfKillEvent(hdb_bindings, NBLOCKS(stat.Length)); */
	Demote();
}


/* MUST be called from within transaction! */
void fsobj::GC() {
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

    if (IsMtPt())
	(void)u.root->Flush();

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
void fsobj::UpdateStatus(ViceStatus *vstat, vv_t *UpdateSet, uid_t uid)
{
    int isrunt = !HAVESTATUS(this);

    /* Mount points are never updated. */
    if (IsMtPt())
	{ print(logFile); CHOKE("fsobj::UpdateStatus: IsMtPt!"); }
    /* Fake objects are never updated. */
    if (IsFake())
	{ print(logFile); CHOKE("fsobj::UpdateStatus: IsFake!"); }

    LOG(100, ("fsobj::UpdateStatus: (%s), uid = %d\n", FID_(&fid), uid));

    /*
     * We have to update the status when we have,
     *  - runt objects (first GetAttr for the fso)
     *  - UpdateSet is non-NULL (Store/SetAttr/Create/..., mutating operations)
     *  - vstat differs (ValidateAttrs)
     */
    if (isrunt || UpdateSet || !StatusEq(vstat))
	ReplaceStatus(vstat, UpdateSet);

    /* If this object is a runt, there may be others waiting for the create
     * to finalize */
    if (isrunt)
	Matriculate();

    /* Update access rights and parent (if they differ). */
    if (IsDir())
	SetAcRights(uid, vstat->MyAccess, vstat->AnyAccess);

    SetParent(vstat->vparent, vstat->uparent);
}


/* Need not be called from within transaction. */
int fsobj::StatusEq(ViceStatus *vstat)
{
    int eq = 1;
    int log = HAVEDATA(this);

    if (stat.Length != vstat->Length) {
	eq = 0;
	if (log)
	    LOG(0, ("fsobj::StatusEq: (%s), Length %d != %d\n",
		    FID_(&fid), stat.Length, vstat->Length));
    }
    /* DataVersion is a non-replicated value and different replicas may
     * legitimately return different dataversions. On a replicated volume we
     * use the VV, and shouldn't use the DataVersion at all. -JH
     */
    if (!vol->IsReplicated()) {
	if (stat.DataVersion != vstat->DataVersion) {
	    eq = 0;
	    if (log)
		LOG(0, ("fsobj::StatusEq: (%s), DataVersion %d != %d\n",
			FID_(&fid), stat.DataVersion, vstat->DataVersion));
	}
    } else {
	if (VV_Cmp(&stat.VV, &vstat->VV) != VV_EQ) {
	    eq = 0;
	    if (log)
		LOG(0, ("fsobj::StatusEq: (%s), VVs differ\n", FID_(&fid)));
	}
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
void fsobj::ReplaceStatus(ViceStatus *vstat, vv_t *UpdateSet)
{
    RVMLIB_REC_OBJECT(stat);

    /* We're changing the length? 
     * Then the cached data is probably no longer useable! But try to fix up
     * the cachefile so that we can at least give a stale copy. */
    if (HAVEDATA(this) && stat.Length != vstat->Length) {
	LOG(0, ("fsobj::ReplaceStatus: (%s), changed stat.length %d->%d\n",
		FID_(&fid), stat.Length, vstat->Length));
	if (IsFile())
	    LocalSetAttr((Date_t)-1, vstat->Length, (Date_t)-1,
			 (uid_t)-1, (unsigned short)-1);
	SetRcRights(RC_STATUS);
    }

    stat.Length = vstat->Length;
    stat.DataVersion = vstat->DataVersion;

    /* nice optimization, but repair is looking at version vectors in not
     * necessarily replicated volumes (although the IsReadWriteRep test should
     * have matched in that case) */
    //if (!vol->IsReplicated() && !vol->IsReadWriteReplica())
    if (UpdateSet) {
	stat.VV.StoreId = vstat->VV.StoreId;
	AddVVs(&stat.VV, UpdateSet);
    } else
	stat.VV = vstat->VV;

    stat.Date = vstat->Date;
    stat.Owner = (uid_t) vstat->Owner;
    stat.Mode = (short) vstat->Mode;
    stat.LinkCount = (unsigned char) vstat->LinkCount;
    if (vstat->VnodeType)
	stat.VnodeType = vstat->VnodeType;
}


int fsobj::CheckRcRights(int rights) {
    return((rights & RcRights) == rights);
}


void fsobj::SetRcRights(int rights)
{
    if (!vol->IsReplicated() || IsFake())
	return;

    if (!HAVEALLDATA(this))
	rights &= ~RC_DATA;

    LOG(100, ("fsobj::SetRcRights: (%s), rights = %d\n", FID_(&fid), rights));

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
    if (vol->IsReplicated()) {
	if (vol->IsDisconnected())		       return 1;
	if (vol->IsWriteDisconnected() && flags.dirty) return 1;
    }

    /* Several other reasons that imply this object is valid */
    if (CheckRcRights(rcrights))    return 1;
    if (IsMtPt() || IsFakeMTLink()) return 1;

    /* Now if we still have the volume callback, we can't lose.
     * also update VCB statistics -- valid due to VCB */
    if (!vol->IsReplicated()) return 0;

    repvol *vp = (repvol *)vol;
    if (vp->HaveCallBack()) {
        vp->VCBHits++;
        return 1;
    }

    /* Final conclusion, the object is not valid */
    return 0;
}


/* Returns {0, EACCES, ENOENT}. */
int fsobj::CheckAcRights(uid_t uid, long rights, int connected)
{
    userent *ue = vol->realm->GetUser(uid);
    long allowed;

    if (ue != vol->realm->system_anyuser) {
	CODA_ASSERT(uid == ue->GetUid());
	/* Do we have this user's rights in the cache? */
	for (int i = 0; i < CPSIZE; i++) {
	    if (SpecificUser[i].inuse && (!connected || SpecificUser[i].valid)
		&& uid == SpecificUser[i].uid) {
		allowed = SpecificUser[i].rights;
		goto exit_found;
	    }
	}
    }
    if (ue == vol->realm->system_anyuser || !connected) {
	/* Do we have access via System:AnyUser? */
	if (AnyUser.inuse && (!connected || AnyUser.valid)) {
	    allowed = AnyUser.rights;
	    goto exit_found;
	}
    }
    PutUser(&ue);

    LOG(10, ("fsobj::CheckAcRights: not found, (%s), (%d, %d, %d)\n",
	      FID_(&fid), uid, rights, connected));
    return(ENOENT);

exit_found:
    PutUser(&ue);
    return (!rights || (rights & allowed)) ? 0 : EACCES;
}


/* MUST be called from within transaction! */
void fsobj::SetAcRights(uid_t uid, long my_rights, long any_rights)
{
    userent *ue = vol->realm->GetUser(uid);

    LOG(100, ("fsobj::SetAcRights: (%s), uid = %d, my_rights = %d, any_rights = %d\n",
	       FID_(&fid), uid, my_rights, any_rights));

    if (!AnyUser.inuse || AnyUser.rights != any_rights) {
	RVMLIB_REC_OBJECT(AnyUser);
	AnyUser.rights = (unsigned char) any_rights;
	AnyUser.inuse = 1;
    }
    AnyUser.valid = 1;

    /* Don't record my_rights if we're system_anyuser! */
    if (ue == vol->realm->system_anyuser) {
	PutUser(&ue);
	return;
    }

    CODA_ASSERT(uid == ue->GetUid());
    PutUser(&ue);

    int i;
    int j = -1;
    int k = -1;
    for (i = 0; i < CPSIZE; i++) {
	if (uid == SpecificUser[i].uid) break;
	if (!SpecificUser[i].inuse) j = i;
	if (!SpecificUser[i].valid) k = i;
    }
    if (i == CPSIZE && j != -1) i = j;
    if (i == CPSIZE && k != -1) i = k;
    if (i == CPSIZE) i = (int) (Vtime() % CPSIZE);

    if (!SpecificUser[i].inuse || SpecificUser[i].uid != uid ||
	SpecificUser[i].rights != my_rights)
    {
	RVMLIB_REC_OBJECT(SpecificUser[i]);
	SpecificUser[i].uid = uid;
	SpecificUser[i].rights = (unsigned char) my_rights;
	SpecificUser[i].inuse = 1;
    }
    SpecificUser[i].valid = 1;
}


/* Need not be called from within transaction. */
void fsobj::DemoteAcRights(uid_t uid)
{
    LOG(100, ("fsobj::DemoteAcRights: (%s), uid = %d\n", FID_(&fid), uid));

    if (uid == ANYUSER_UID)
	AnyUser.valid = 0;

    for (int i = 0; i < CPSIZE; i++)
	if (uid == ANYUSER_UID || SpecificUser[i].uid == uid)
	    SpecificUser[i].valid = 0;
}


/* Need not be called from within transaction. */
void fsobj::PromoteAcRights(uid_t uid)
{
    LOG(100, ("fsobj::PromoteAcRights: (%s), uid = %d\n", FID_(&fid), uid));

    if (uid == ANYUSER_UID) {
	AnyUser.valid = 1;

	/* 
	 * if other users who have rights in the cache also have
	 * tokens, promote their rights too. 
	 */
	for (int i = 0; i < CPSIZE; i++)
	    if (SpecificUser[i].inuse && !SpecificUser[i].valid) {
		userent *ue = vol->realm->GetUser(SpecificUser[i].uid);
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
	userent *ue = vol->realm->GetUser(uid);
	int tokensvalid = ue->TokensValid();
	PutUser(&ue);
	if (!tokensvalid) return;

	for (int i = 0; i < CPSIZE; i++)
	    if (SpecificUser[i].uid == uid)
		SpecificUser[i].valid = 1;
    }
}


/* MUST be called from within transaction! */
void fsobj::ClearAcRights(uid_t uid)
{
    LOG(100, ("fsobj::ClearAcRights: (%s), uid = %d\n", FID_(&fid), uid));

    if (uid == ANYUSER_UID) {
	RVMLIB_REC_OBJECT(AnyUser);
	AnyUser = NullAcRights;
    }

    for (int i = 0; i < CPSIZE; i++)
	if (uid == ANYUSER_UID || SpecificUser[i].uid == uid) {
	    RVMLIB_REC_OBJECT(SpecificUser[i]);
	    SpecificUser[i] = NullAcRights;
	}
}


/* local-repair modification */
/* MUST be called from within transaction (at least if <vnode, unique> != pfid.<Vnode, Unique>)! */
void fsobj::SetParent(VnodeId vnode, Unique_t unique) {
    if (IsRoot() || (vnode == 0 && unique == 0))
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
	pfid = fid;
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
int fsobj::TryToCover(VenusFid *inc_fid, uid_t uid)
{
    if (!HAVEALLDATA(this))
	{ print(logFile); CHOKE("fsobj::TryToCover: called without data"); }

    LOG(10, ("fsobj::TryToCover: fid = (%s)\n", FID_(&fid)));

    int code = 0;

    /* Don't cover mount points in backup volumes! */
    if (!IsLocalObj() && vol->IsBackup())
	return(ENOENT); /* ELOOP? */

    /* Check for bogosities. */
    int len = (int) stat.Length;
    if (len < 2) {
	eprint("TryToCover: bogus link length");
	return(EINVAL);
    }

    char type = data.symlink[0];

    /* Turn volume name into a proper string. */
    data.symlink[len-1] = '\0';

    VenusFid root_fid;
    FID_MakeRoot(MakeViceFid(&root_fid));

    /* Look up the volume that is to be mounted on us. */

    volent *tvol = 0;

    switch(type) {
    case '#':
      code = VDB->Get(&tvol, vol->realm, &data.symlink[1], this);
      break;

    case '@':
      if (!IsExpandedObj()) {
	LOG(0, ("fsobj::TryToCover: (%s) -> %s wasn't expanded! Dangling symlink?\n",FID_(&fid),data.symlink));
	code = ENOENT;
	break;
      }
    case '$':
      {
	Volid vid;
	char *realmname, tmp;
	int n;
	Realm *r = vol->realm;

	n = sscanf(data.symlink, "%*c%lx.%lx.%lx@%c", &vid.Volume,
		   &root_fid.Vnode, &root_fid.Unique, &tmp);
	if (n < 3) {
	  LOG(0,("fsobj::TryToCover: (%s) -> %s failed parse.\n",
		 FID_(&fid), data.symlink));
	  print(logFile);
	  CHOKE("fsobj::TryToCover: couldn't parse mountlink");
	}

	r->GetRef();

	if (n == 4) {
	  /* strrchr should succeed now because sscanf succeeded. */
	  realmname = strrchr(data.symlink, '@')+1;

	  r->PutRef();
	  r = REALMDB->GetRealm(realmname);
        }
        vid.Realm = r->Id();
	r->PutRef();

        code = VDB->Get(&tvol, &vid);
      }
      break;

    default:
      eprint("TryToCover: bogus mount point type (%c)", type);
      return(EINVAL);
    }

    if (code != 0) {
	LOG(0, ("fsobj::TryToCover: vdb::Get(%s) failed (%d)\n", data.symlink, code));
	return(code);
    }

    /* Don't allow a volume to be mounted inside itself! */
    /* but only when its mount root is the global-root-obj of a local subtree */
    if (fid.Realm == tvol->GetRealmId() && fid.Volume == tvol->GetVolumeId()) {
	eprint("TryToCover(%s): recursive mount!", data.symlink);
	VDB->Put(&tvol);
	return(ELOOP);
    }

    /* Only allow cross-realm mountpoints to or from the local realm. */
    if (vol->GetRealmId() != tvol->GetRealmId()) {
	if (vol->GetRealmId() != LocalRealm->Id() &&
	    tvol->GetRealmId() != LocalRealm->Id()) {
	    VDB->Put(&tvol);
	    return ELOOP;
	}
    }

    /* Get volume root. */
    fsobj *rf = 0;
    root_fid.Realm = tvol->GetRealmId();
    root_fid.Volume = tvol->vid;

    code = FSDB->Get(&rf, &root_fid, uid, RC_STATUS, comp, NULL, NULL, 1);
    if (code != 0) {
	LOG(0, ("fsobj::TryToCover: Get root (%s) failed (%d)\n",
		  FID_(&root_fid), code));

	VDB->Put(&tvol);
	if (code == EINCONS && inc_fid != 0) {
	  LOG(0, ("fsobj::TryToCover: returning inconsistent fid. (%d)\n",
		  FID_(&root_fid), code));
	  *inc_fid = root_fid;
	}
	return(code);
    }
    rf->PromoteLock();

    /* If root is currently mounted, uncover the mount point and unmount. */
    Recov_BeginTrans();
    fsobj *mf = rf->u.mtpoint;
    if (mf != 0) {
	    if (mf == this) {
		    eprint("TryToCover: re-mounting (%s) on (%s)", tvol->name, GetComp());
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

    RVMLIB_REC_OBJECT(rf->mvstat);
    CODA_ASSERT(rf->u.mtpoint == 0);
    rf->mvstat = ROOT;
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
    if (!IsNormal())
	{ print(logFile); CHOKE("fsobj::CoverMtPt: mvstat != NORMAL"); }
    if (!data.symlink)
	{ print(logFile); CHOKE("fsobj::CoverMtPt: no data.symlink!"); }

    LOG(10, ("fsobj::CoverMtPt: fid = (%s), rootfid = (%s)\n",
	     FID_(&fid), FID_(&root_fso->fid)));

    RVMLIB_REC_OBJECT(*this);

    /* Exit old state (NORMAL). */
    k_Purge(&fid, 1);

    /* Enter new state (MOUNTPOINT). */
    mvstat = MOUNTPOINT;
    u.root = root_fso;
    DisableReplacement();
}


/* MUST be called from within transaction! */
/* Call with object write-locked. */
void fsobj::UncoverMtPt() {
    if (!IsMtPt()) 
	{ print(logFile); CHOKE("fsobj::UncoverMtPt: mvstat != MOUNTPOINT"); }
    if (!u.root)
	{ print(logFile); CHOKE("fsobj::UncoverMtPt: no u.root!"); }

    LOG(10, ("fsobj::UncoverMtPt: fid = (%s), rootfid = (%s)\n",
	      FID_(&fid), FID_(&u.root->fid)));

    RVMLIB_REC_OBJECT(*this);

    /* Exit old state (MOUNTPOINT). */
    u.root = 0;
    k_Purge(&fid, 1);			/* I don't think this is necessary. */
    k_Purge(&pfid, 1);			/* This IS necessary. */

    /* Enter new state (NORMAL). */
    mvstat = NORMAL;
    EnableReplacement();
}


/* MUST be called from within transaction! */
/* Call with object write-locked. */
void fsobj::MountRoot(fsobj *mtpt_fso) {
    if (!IsRoot())
	{ print(logFile); CHOKE("fsobj::MountRoot: mvstat != ROOT"); }
    if (u.mtpoint)
	{ print(logFile); CHOKE("fsobj::MountRoot: u.mtpoint exists!"); }

    LOG(10, ("fsobj::MountRoot: fid = %s, mtptfid = %s\n",
	     FID_(&fid), FID_(&mtpt_fso->fid)));

    RVMLIB_REC_OBJECT(*this);

    /* Exit old state (ROOT, without link). */
    k_Purge(&fid);

    /* Enter new state (ROOT, with link). */
    u.mtpoint = mtpt_fso;
}


/* MUST be called from within transaction! */
/* Call with object write-locked. */
void fsobj::UnmountRoot() {
  fsobj *mtpoint;
    if (!IsRoot())
	{ print(logFile); CHOKE("fsobj::UnmountRoot: mvstat != ROOT"); }
    if (!u.mtpoint)
	{ print(logFile); CHOKE("fsobj::UnmountRoot: no u.mtpoint!"); }

    LOG(10, ("fsobj::UnmountRoot: fid = (%s), mtptfid = (%s)\n",
	      FID_(&fid), FID_(&u.mtpoint->fid)));

    RVMLIB_REC_OBJECT(*this);

    /* Exit old state (ROOT, with link). */
    mtpoint = u.mtpoint;
    u.mtpoint = 0;
    k_Purge(&fid);

    /* Enter new state (ROOT, without link). */
    if (!FID_IsVolRoot(&fid)) {
	mvstat = NORMAL;	    /* couldn't be mount point, could it? */
	/* this object could be a server replica from an expand */
	/*if (FID_EQ(&pfid, &NullFid) && !IsLocalObj()) {
	if(mtpoint->IsExpandedObj() && !IsLocalObj()) {
	  //	    LOG(0, ("fsobj::UnmountRoot: (%s) a previous mtroot without pfid, kill it\n", FID_(&fid)));
	  LOG(10, ("fsobj::UnmountRoot: (%s) Server replica from previous expand, kill it\n", FID_(&fid)));
	    Kill();
	    }*/
	/* it could also just get garbage collected */

    }
}


/*  *****  Child/Parent Linkage  *****  */

/* Need not be called from within transaction. */
void fsobj::AttachChild(fsobj *child) {
    if (!IsDir())
	{ print(logFile); child->print(logFile); CHOKE("fsobj::AttachChild: not dir"); }

    LOG(100, ("fsobj::AttachChild: (%s), (%s)\n",
	       FID_(&fid), FID_(&child->fid)));

    DisableReplacement();

    if (child->child_link.is_linked())
	{ print(logFile); child->print(logFile); CHOKE("fsobj::AttachChild: bad child"); }
    if (!children)
	children = new dlist;
    children->prepend(&child->child_link);

    DemoteHdbBindings();	    /* in case an expansion would now be satisfied! */
}


/* Need not be called from within transaction. */
void fsobj::DetachChild(fsobj *child) {
    if (!IsDir())
	{ print(logFile); child->print(logFile); CHOKE("fsobj::DetachChild: not dir"); }

    LOG(100, ("fsobj::DetachChild: (%s), (%s)\n",
	       FID_(&fid), FID_(&child->fid)));

    DemoteHdbBindings();	    /* in case an expansion would no longer be satisfied! */

    if (child->pfso != this || !child->child_link.is_linked() ||
	!children || children->count() == 0)
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
void fsobj::ComputePriority(int Force) {
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

    /* Force is only set by RecomputePriorities when called by the Hoard
     * daemon (once every 10 minutes). By forcefully taking all FSO's off
     * the priority queue and requeueing them the random seed is perturbed
     * to avoid cache pollution by unreferenced low priority objects which
     * happen to have a high random seed */
    if (Force || priority == -1 || new_priority != priority) {
	FSDB->Reorders++;		    /* transient value; punt set_range */

	DisableReplacement();		/* remove... */
	priority = new_priority;    	/* update key... */
	EnableReplacement();	 	/* reinsert... */
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
#endif /* VENUSDEBUG */

    /* Already replaceable? */
    if (REPLACEABLE(this))
	return;

    /* Are ALL conditions for replaceability met? */
    if (DYING(this) || !HAVESTATUS(this) || DIRTY(this) || IsLocalObj() ||
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
#endif

    FSDB->prioq->insert(&prio_handle);

#ifdef	VENUSDEBUG
    if (LogLevel >= 10000 && !(FSDB->prioq->IsOrdered()))
	{ print(logFile); FSDB->prioq->print(logFile); CHOKE("fsobj::EnableReplacement: !IsOrdered after insert"); }
#endif
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
#endif

    /* Already not replaceable? */
    if (!REPLACEABLE(this))
	return;

    LOG(1000,("fsobj::DisableReplacement: (%s), priority = [%d (%d) %d %d]\n",
	      FID_(&fid), priority, flags.random, HoardPri, FSDB->LastRef[ix]));

#ifdef	VENUSDEBUG
    if (LogLevel >= 10000)
	FSDB->prioq->print(logFile);
#endif

    if (FSDB->prioq->remove(&prio_handle) != &prio_handle)
	{ print(logFile); CHOKE("fsobj::DisableReplacement: prioq remove"); }

#ifdef	VENUSDEBUG
    if (LogLevel >= 10000 && !(FSDB->prioq->IsOrdered()))
	{ print(logFile); FSDB->prioq->print(logFile); CHOKE("fsobj::DisableReplacement: !IsOrdered after remove"); }
#endif
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
    b->IncrRefCount();

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
	HoardVuid = nc->uid;
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
    b->DecrRefCount();
    if (IsSymLink() && hdb_bindings->count() == 0)
	EnableReplacement();

    /* Update the state of the binder. */
    namectxt *nc = (namectxt *)b->binder;
    if (DemoteNameCtxt)
	nc->Demote();

    /* Recompute our priority if necessary. */
    if (nc->priority == HoardPri) {
	int new_HoardPri = 0;
	uid_t new_HoardVuid = HOARD_UID;
    gettimeofday(&StartTV, 0);
    LOG(10, ("Detach: hdb_binding list contains %d namectxts\n", hdb_bindings->count()));
	dlist_iterator next(*hdb_bindings);
	dlink *d;
	while ((d = next())) {
	    binding *b = strbase(binding, d, bindee_handle);
	    namectxt *nc = (namectxt *)b->binder;
	    if (nc->priority > new_HoardPri) {
		new_HoardPri = nc->priority;
		new_HoardVuid = nc->uid;
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

CacheMissAdvice fsobj::ReadDisconnectedCacheMiss(vproc *vp, uid_t uid)
{
    char pathname[MAXPATHLEN];
    CacheMissAdvice advice;

    LOG(100, ("E fsobj::ReadDisconnectedCacheMiss\n"));

    /* If advice not enabled, simply return */
    if (!SkkEnabled) {
        LOG(100, ("ADVSKK STATS:  RDCM Advice NOT enabled.\n"));
        return(FetchFromServers);
    }

    /* Check that:                                                     *
     *     (a) the request did NOT originate from the Hoard Daemon     *
     *     (b) the request did NOT originate from that AdviceMonitor,  *
     * and (c) the user is running an AdviceMonitor,                   */
    CODA_ASSERT(vp != NULL);
    if (vp->type == VPT_HDBDaemon) {
	LOG(100, ("ADVSKK STATS:  RDCM Advice inappropriate.\n"));
        return(FetchFromServers);
    }
    if (adv_mon.skkPgid(vp->u.u_pgid)) {
        LOG(100, ("ADVSKK STATS:  RDCM Advice inappropriate.\n"));
        return(FetchFromServers);
    }
    if (!(adv_mon.ConnValid())) {
        LOG(100, ("ADVSKK STATS:  RDCM Advice NOT valid. (uid = %d)\n", uid));
        return(FetchFromServers);
    }

    GetPath(pathname, 1);

    LOG(100, ("Requesting ReadDisconnected CacheMiss Advice for path=%s, pid=%d...\n", pathname, vp->u.u_pid));
    advice = adv_mon.ReadDisconnectedAdvice(&fid, pathname, vp->u.u_pgid);
    return(advice);
}

CacheMissAdvice fsobj::WeaklyConnectedCacheMiss(vproc *vp, uid_t uid)
{
    char pathname[MAXPATHLEN];
    CacheMissAdvice advice;
    unsigned long CurrentBandwidth;

    LOG(100, ("E fsobj::WeaklyConnectedCacheMiss\n"));

    /* If advice not enabled, simply return */
    if (!SkkEnabled) {
        LOG(100, ("ADVSKK STATS:  WCCM Advice NOT enabled.\n"));
        return(FetchFromServers);
    }

    /* Check that:                                                     *
     *     (a) the request did NOT originate from the Hoard Daemon     *
     *     (b) the request did NOT originate from that AdviceMonitor,  *
     * and (c) the user is running an AdviceMonitor,                   */
    CODA_ASSERT(vp != NULL);
    if (vp->type == VPT_HDBDaemon) {
	LOG(100, ("ADVSKK STATS:  WCCM Advice inappropriate.\n"));
        return(FetchFromServers);
    }
    if (adv_mon.skkPgid(vp->u.u_pgid)) {
        LOG(100, ("ADVSKK STATS:  WCCM Advice inappropriate.\n"));
        return(FetchFromServers);
    }
    if (!(adv_mon.ConnValid())) {
        LOG(100, ("ADVSKK STATS:  WCCM Advice NOT valid. (uid = %d)\n", uid));
        return(FetchFromServers);
    }

    GetPath(pathname, 1);

    LOG(100, ("Requesting WeaklyConnected CacheMiss Advice for path=%s, pid=%d...\n", 
	      pathname, vp->u.u_pid));
    vol->GetBandwidth(&CurrentBandwidth);
    advice = adv_mon.WeaklyConnectedAdvice(&fid, pathname, vp->u.u_pid,
					   stat.Length, CurrentBandwidth,
					   cf.Name());
    return(advice);
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
    b->IncrRefCount();

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
    b->DecrRefCount();

    /* Clear our "dirty" flag if this was the last binding. */
    if (mle_bindings->count() == 0) {
	MakeClean();
    }
}


/*  *****  Data Contents  *****  */

/* MUST be called from within transaction! */
/* Call with object write-locked. */
/* If there are readers of this file, they will have it change from underneath them! */
void fsobj::DiscardData() {
    if (!HAVEDATA(this))
	{ print(logFile); CHOKE("fsobj::DiscardData: !HAVEDATA"); }
    if (ACTIVE(this))
	{ print(logFile); CHOKE("fsobj::DiscardData: ACTIVE"); }

    LOG(10, ("fsobj::DiscardData: (%s)\n", FID_(&fid)));

    CODA_ASSERT(!DIRTY(this));

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
int fsobj::Fakeify()
{
    VenusFid fakefid;
    LOG(10, ("fsobj::Fakeify: %s, (%s)\n", comp, FID_(&fid)));

    fsobj *pf = 0;
    if (!IsRoot()) {
	if (fid.Volume == FakeRootVolumeId)
	    pf = FSDB->Find(&rootfid);

	else {
	    /* Laboriously scan database to find our parent! */
	    struct dllist_head *p;
	    list_for_each(p, vol->fso_list) {
		pf = list_entry_plusplus(p, fsobj, vol_handle);

		if (!pf->IsDir() || pf->IsMtPt()) continue;
		if (!HAVEALLDATA(pf)) continue;

		if (pf->dir_IsParent(&fid))
		    break; /* Found! */
	    }
	    if (p == &vol->fso_list) {
		LOG(10, ("fsobj::Fakeify: %s, (%s), parent not found\n",
			comp, FID_(&fid)));
		Recov_BeginTrans();
		Matriculate();
		Recov_EndTrans(MAXFP);
		return ENOENT;
	    }
	}
    }

    // Either (pf == 0 and this is the volume root) OR (pf != 0 and it isn't)

    Recov_BeginTrans();
    RVMLIB_REC_OBJECT(*this);

    /* Initialize status. */
    stat.DataVersion = 1;
    stat.Owner = V_UID;
    stat.Date = Vtime();
    
    if (pf) {
	pfid = pf->fid;
	pfso = pf;
	pf->AttachChild(this);
    }

    fakefid.Realm  = LocalRealm->Id();
    fakefid.Volume = FakeRootVolumeId;
    fakefid.Vnode  = 1;
    fakefid.Unique = 1;

    /* Are we creating objects for the fake root volume? */
    if (FID_VolEQ(&fid, &fakefid)) {
	if (FID_EQ(&fid, &fakefid)) { /* actual root directory? */
	    struct dllist_head *p;

	    stat.Mode = 0555;
	    stat.LinkCount = 2;
	    stat.VnodeType = Directory;
	    /* Access rights are not needed, this whole volume is readonly! */

	    /* Create the target directory. */
	    dir_MakeDir();
	    list_for_each(p, REALMDB->realms) {
		Realm *realm = list_entry_plusplus(p, Realm, realms);
		if (!realm->rootservers) continue;

		fakefid.Vnode = 0xfffffffc;
		fakefid.Unique = realm->Id();

		dir_Create(realm->Name(), &fakefid);
		LOG(10, ("fsobj::Fakeify: created fake codaroot entry for %s\n",
			 realm->Name()));
	    }
	    LOG(10, ("fsobj::Fakeify: created fake codaroot directory\n"));
	} else {

	    stat.Mode = 0644;
	    stat.LinkCount = 1;
	    stat.VnodeType = SymbolicLink;

	    /* "#@RRRRRRRRR." */
	    /* realm = comp */
	    stat.Length = strlen(comp) + 3;
	    data.symlink = (char *)rvmlib_rec_malloc(stat.Length+1);
	    rvmlib_set_range(data.symlink, stat.Length+1);
	    sprintf(data.symlink, "#@%s.", comp);

	    UpdateCacheStats(&FSDB->FileDataStats, CREATE, BLOCKS(this));

	    LOG(10, ("fsobj::Fakeify: created realm mountlink %s\n",
		    data.symlink));
	}
	flags.local = 1;
	goto done;
    }

    fakefid.Volume = FakeRepairVolumeId;
    /* Are we creating objects in the fake repair volume? */
    if (FID_VolEQ(&fid, &fakefid)) {
	if (!FID_IsFakeRoot(MakeViceFid(&fid))) {
	    struct in_addr volumehosts[VSG_MEMBERS];
	    VolumeId volumeids[VSG_MEMBERS];
	    volent *pvol = u.mtpoint->vol;
	    repvol *vp = (repvol *)pvol;

	    CODA_ASSERT(pvol->IsReplicated());

	    stat.Mode = 0555;
	    stat.LinkCount = 2;
	    stat.VnodeType = Directory;
	    /* Access rights are not needed, this whole volume is readonly! */

	    /* Create the target directory. */
	    dir_MakeDir();

	    fakefid.Vnode = 0xfffffffc;

	    /* testing 1..2..3.. trying to show the local copy as well */
	    fakefid.Unique = vp->GetVolumeId();
	    dir_Create("localhost", &fakefid);

	    LOG(10, ("fsobj::Fakeify: new entry (localhost, %s)\n", FID_(&fakefid)));

	    /* Make entries for each of the rw-replicas. */
	    vp->GetHosts(volumehosts);
	    vp->GetVids(volumeids);
	    for (int i = 0; i < VSG_MEMBERS; i++) {
		if (!volumehosts[i].s_addr) continue;
		srvent *s = FindServer(&volumehosts[i]);

		fakefid.Unique = volumeids[i];
		dir_Create(s->name, &fakefid);

		LOG(10, ("fsobj::Fakeify: new entry (%s, %s)\n", s->name, FID_(&fakefid)));
	    }
	} else {
	    /* get the actual object we're mounted on */
	    fsobj *real_obj = pfso->u.mtpoint;
	    ViceFid LinkFid;
	    const char *realmname;

	    CODA_ASSERT(real_obj->vol->IsReplicated());

	    stat.Mode = 0644;
	    stat.LinkCount = 1;
	    stat.VnodeType = SymbolicLink;

	    LinkFid.Volume = fid.Unique;
	    LinkFid.Vnode  = real_obj->fid.Vnode;
	    LinkFid.Unique = real_obj->fid.Unique;
	    /* should really be vp->volrep[i]->realm->Name(), but
	     * cross-realm replication should probably not be attempted
	     * anyways, with authentication issues and all -JH */
	    realmname = real_obj->vol->realm->Name();

	    /* Write out the link contents. */
	    /* "@XXXXXXXX.YYYYYYYY.ZZZZZZZZ@RRRRRRRRR." */
	    stat.Length = 29 + strlen(realmname);
	    data.symlink = (char *)rvmlib_rec_malloc(stat.Length+1);
	    rvmlib_set_range(data.symlink, stat.Length+1);
	    sprintf(data.symlink, "@%08x.%08x.%08x@%s.",
		    LinkFid.Volume, LinkFid.Vnode, LinkFid.Unique, realmname);

	    LOG(10, ("fsobj::Fakeify: making %s a symlink %s\n",
		    FID_(&fid), data.symlink));

	    UpdateCacheStats(&FSDB->FileDataStats, CREATE, BLOCKS(this));
	}
	flags.local = 1;
	goto done;
    }

done:
    DisableReplacement();
    /* notify blocked threads that the fso is ready. */
    Matriculate();
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

void fsobj::GetVattr(struct coda_vattr *vap)
{
    /* Most attributes are derived from the VenusStat structure. */
    vap->va_type = FTTOVT(stat.VnodeType);
    vap->va_mode = stat.Mode;

    if (stat.VnodeType == File) /* strip the setuid bits on files! */
        vap->va_mode &= ~(S_ISUID | S_ISGID);

    vap->va_uid = (uid_t) stat.Owner;
    vap->va_gid = V_GID;

    vap->va_fileid = (IsRoot() && u.mtpoint)
		       ? FidToNodeid(&u.mtpoint->fid)
		       : FidToNodeid(&fid);

    /* hack to avoid an optimization where find skips subdirectories if
     * there are any mountpoints in the current directory (since the
     * mountlinks don't are not represented in the link count). */
    vap->va_nlink = (stat.VnodeType == Directory) ? 1 : stat.LinkCount;
    vap->va_blocksize = V_BLKSIZE;
    vap->va_rdev = 1;

    /* If the object is currently open for writing we must physically 
       stat it to get its size and time info. */
    if (WRITING(this))
    {
	struct stat tstat;
	cf.Stat(&tstat);

	vap->va_size = tstat.st_size;
	vap->va_mtime.tv_sec = tstat.st_mtime;
	vap->va_mtime.tv_nsec = 0;
    }
    else
    {
	vap->va_size = (u_quad_t) stat.Length;
	vap->va_mtime.tv_sec = (time_t)stat.Date;
	vap->va_mtime.tv_nsec = 0;
    }

    /* Convert size of file to bytes of storage after getting size! */
    vap->va_bytes = NBLOCKS_BYTES(vap->va_size);

    /* We don't keep track of atime/ctime, so keep them identical to mtime */
    vap->va_atime = vap->va_ctime = vap->va_mtime;

    VPROC_printvattr(vap);
}


void fsobj::ReturnEarly() {
    /* Only mutations on replicated objects can return early. */
    if (!vol->IsReplicated()) return;

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
	    out->coda_create.Fid = *VenusToKernelFid(&fid);
	    DemoteLock();
	    GetVattr(&out->coda_create.attr);
	    PromoteLock();
	    w->Return(w->msg, sizeof (struct coda_create_out));
	    break;
	    }

	case CODA_CLOSE:
	    {
	    /* Don't return early here if we already did so in a callback handler! */
	    if (!FID_EQ(&w->StoreFid, &NullFid))
		w->Return(0);
	    break;
	    }

	case CODA_IOCTL:
	    {
	    /* Huh. IOCTL in the kernel thinks there may be return data. Assume not. */
	    out = (union outputArgs *)w->msg->msg_buf;
	    out->coda_ioctl.data = (char *)sizeof(struct coda_ioctl_out); 
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
void fsobj::GetPath(char *buf, int scope)
{
    if (IsRoot()) {
	if (scope == PATH_VOLUME) {
	    buf[0] = '\0';
	    return;
	}

	if (scope == PATH_REALM &&
	    (!u.mtpoint || vol->GetRealmId() != u.mtpoint->vol->GetRealmId()))
	{
	    buf[0] = '\0';
	    return;
	}

	if (IsVenusRoot()) {
  	    LOG(100, ("fsobj::GetPath (%s): venusRoot.\n", FID_(&fid)));
	    strcpy(buf, venusRoot);
	    return;
	}

	if (!u.mtpoint) {
  	    LOG(100, ("fsobj::GetPath (%s): Root, but no mountpoint found.\n", FID_(&fid)));
	    strcpy(buf, "???");
	    return;
	}

	u.mtpoint->GetPath(buf, scope);
	return;
    }

    if (!pfso && !FID_EQ(&pfid, &NullFid)) {
	fsobj *pf = FSDB->Find(&pfid);
	if (pf != 0 && HAVESTATUS(pf) && !GCABLE(pf)) {
	    pfso = pf;
	    pfso->AttachChild(this);
	}
    }

    if (pfso) {
	pfso->GetPath(buf, scope);
    }
    else {
      if(fid.Volume == FakeRootVolumeId) {
	LOG(0, ("fsobj::GetPath (%s): In the local realm without a parent. "
		"Realm mountpoint?\n", FID_(&fid)));	
	strcpy(buf, venusRoot);
      }
      else {
	LOG(0, ("fsobj::GetPath (%s): Couldn't find parent.\n", FID_(&fid)));
	strcpy(buf, "???");
      }
    }

    strcat(buf, "/");
    strcat(buf, comp);
}

/* Virginal state may cause some access checks to be avoided. */
int fsobj::IsVirgin()
{
    int virginal = 0;
    int i;

    if (vol->IsReplicated()) {
	for (i = 0; i < VSG_MEMBERS; i++)
	    if ((&(stat.VV.Versions.Site0))[i] != 0)
		break;
	if (i == VSG_MEMBERS) virginal = 1;

	/* If file is dirty, it's really virginal only if there are no stores in the CML! */
	if (virginal && vol->IsReplicated() && IsFile() && DIRTY(this))
	{
	    repvol *rv = (repvol *)vol;
	    cml_iterator next(rv->CML, CommitOrder, &fid);
	    cmlent *m;
	    while ((m = next()))
		if (m->opcode == CML_Store_OP)
		    break;
	    if (m) virginal = 0;
	}
    } else {
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
	u.root->CacheReport(fd, level);
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
int fsobj::EstimatedFetchCost(int type)
{
    unsigned long bw;	/* bandwidth, in bytes/sec */

    LOG(100, ("E fsobj::EstimatedFetchCost(%d)\n", type));

    vol->GetBandwidth(&bw);

    LOG(100, ("stat.Length = %d; Bandwidth = %d\n", stat.Length, bw));
    LOG(100, ("EstimatedFetchCost = %d\n", (int)stat.Length/bw));

    return( (int)stat.Length/bw ); 
}

void fsobj::RecordReplacement(int status, int data)
{
#if 0
    char mountpath[MAXPATHLEN];
    char path[MAXPATHLEN];

    if (!SkkEnabled) return;

    LOG(10, ("RecordReplacement(%d,%d)\n", status, data));

    CODA_ASSERT(vol != NULL);
    vol->GetMountPath(mountpath, 0);
    GetPath(path, 1);    
    NotifyUserOfReplacement(&fid, path, status, (data ? 1 : 0));
#endif
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
    fdprint(fdes, "\tvoltype = [%d %d %d], fake = %d, fetching = %d local = %d, expanded = %d\n",
	     vol->IsBackup(), vol->IsReplicated(), vol->IsReadWriteReplica(),
	     flags.fake, flags.fetching, flags.local);
    fdprint(fdes, "\trep = %d, data = %d, owrite = %d, dirty = %d, shadow = %d ckmtpt\n",
	     REPLACEABLE(this), HAVEDATA(this), flags.owrite, flags.dirty,
	     shadow != 0, flags.ckmtpt);

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
	  nc->print(fdes, this);
      }

    }

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
}

void fsobj::ListCache(FILE *fp, int long_format, unsigned int valid)
{
  /* list in long format, if long_format == 1;
     list fsobjs
          such as valid (valid == 1), non-valid (valid == 2) or all (valid == 3) */

  int isvalid = DATAVALID(this) && STATUSVALID(this);
  char path[MAXPATHLEN];
  GetPath(path, 0);		/* Get relative pathname. */    

  if (valid == 1 && !isvalid) return;
  if (valid == 2 && isvalid) return;

  if (!long_format)
      ListCacheShort(fp);
  else
      ListCacheLong(fp);
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

int fsobj::LaunchASR(int conflict_type, int object_type) {
  int uid, rc, pfd[2];
  char path[MAXPATHLEN], rootPath[MAXPATHLEN];
  pid_t pid;
  userent *ue;
  vproc *vp;
  SecretToken st;
  ClearToken ct;
  repvol *v;

  vp = VprocSelf();
  if(vp == NULL)
    return -1;

  switch(conflict_type) {
  case SERVER_SERVER:
	uid = vp->u.u_uid;
	break;
  case LOCAL_GLOBAL:
  case MIXED_CONFLICT:
	uid = WhoIsLastAuthor();
	break;

  default:
	LOG(0, ("fsobj::LaunchASR: Bad conflict type!\n"));
	return -1;
  }

  v = (repvol *) vol;

  /* Prepare args for launch. */
  { 
    /* Conflict path is the first argument to ASRLauncher. */

    GetPath(path, 1);

    /* Volume root's path is the second argument to ASRLauncher. */

    VenusFid rootFid;
    fsobj *root = NULL;

    /* Prepare a fid identical to the root's to ->Find it without locking. */
    rootFid.Realm = fid.Realm;
    rootFid.Volume = fid.Volume;
    rootFid.Vnode = 1;
    rootFid.Unique = 1;
    
    root = FSDB->Find(&rootFid);
    if(root == NULL) {
      LOG(0, ("fsobj::LaunchASR: ASR's Volume Root not cached!\n"));
      return -1;
    }

    root->GetPath(rootPath, 1);
  }

  LOG(0, ("fsobj::LaunchASR:\n  Conflict path: %s\n  Volume Root: %s\n",
	  path, rootPath));

  /* Obtain the user and his tokens to assign them to the ASRLauncher uid. */

  ue = v->realm->GetUser(uid);
  CODA_ASSERT(ue != NULL);


  /* Find tokens associated with this user */

  rc = ue->GetTokens(&st, &ct);
  if(rc != 0) {
    LOG(0, ("fsobj::LaunchASR: ASR cannot launch: No valid"
			" tokens exist for user: %d\n", uid));	    
    return -1;
  }	  

  switch(object_type) {

  case DIRECTORY_CONFLICT:

	if((path[strlen(path)-1]) != '/')
	  strcat(path, "/");

    LOG(0, ("fsobj::LaunchASR: Directory conflict! Pathname: %s\n", path));
	break;

  case FILE_CONFLICT:

    LOG(0, ("fsobj::LaunchASR: File conflict! Pathname: %s\n", path));
	break;
	
  default:
	CODA_ASSERT(0);
  }

  /* At this point, we have all of the information required to begin the
   * launch sequence. */

	  
  /* Begin launch by locking other ASR's out of the volume under repair. */

  v->lock_asr();  
          
	  
  /* Store the ASRLauncher's pid in the ASRLauncher table, to give the
   * SIGCHLD signal handler some information to work with later. */

  /* Not exactly thread-safe, but rarely do ASR's start concurrently. */
  

  if(ASRpid > 0) {
    LOG(0, ("fsobj::LaunchASR: ASR in progress in another volume!\n"));
	return -1; /* or should we yield() for a while? */
  }
	
  ASRpid = 1; /* reserve our place */


  /* Tell the user what is in conflict, and what launcherfile's handling it. */

  MarinerLog("ASRLauncher(%s) HANDLING %s\n", ASRLauncherFile, path);
	  

  /* Assign Coda tokens to Venus' uid. */

  LOG(0, ("fsobj::LaunchASR: Assigning tokens from uid %d to uid %d\n",
	  uid, getuid()));

  v->realm->NewUserToken(uid, &st, &ct);


  /* Fork child ASRLauncher and exec() the file in the ASRLaunchFile global. */
  if(pipe(pfd) == -1) { perror("pipe"); exit(EXIT_FAILURE); }

  pid = fork();
  if(pid == 0) {
	int error;
    char *arg[6], buf[3];
    char confstr[4];
    
	close(pfd[1]);
    if(setpgrp() < 0) { perror("setpgrp"); exit(EXIT_FAILURE); }
    
    sprintf(confstr, "%d", conflict_type);

    /* Set up argument array. */

    arg[0] = ASRLauncherFile; /* extracted from venus.conf */
    arg[1] = path;
    arg[2] = rootPath;
    arg[3] = confstr;
    arg[4] = ASRPolicyFile;   /* extracted from venus.conf */
    arg[5] = NULL;

	while((error = read(pfd[0], (void *)buf, 2)) == 0)
	  continue;

	if(error < 0) { perror("read"); exit(EXIT_FAILURE); }

	if(setuid(uid) < 0) { perror("setuid"); exit(EXIT_FAILURE); }

	close(pfd[0]);

    if(execve(arg[0], arg, 0) < 0) { perror("exec"); exit(EXIT_FAILURE); }
  }
  
  close(pfd[0]);

  /* Restrict access to this volume by process group id. */

  v->asr_pgid(pid);
  
  
  /* Store relevant information globally, and make sure it is stored before
   * we exec().
   * This is used when we receive a SIGCHLD at the end of our launch. */
  
  ASRpid = pid;
  ASRfid = fid;
  ASRuid = uid;

  if(write(pfd[1], (void *) "go", 2) < 0)
	{ perror("write"); exit(EXIT_FAILURE); }

  close(pfd[1]);

  {
    struct timeval tv;
    gettimeofday(&tv, 0);
    lastresolved = tv.tv_sec; /* This is much easier to do here than SIGASR. */
  }
  
  return 0;
}

/* *****  Iterator  ***** */

fso_iterator::fso_iterator(LockLevel level, const VenusFid *key) : rec_ohashtab_iterator(FSDB->htab, key) {
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

