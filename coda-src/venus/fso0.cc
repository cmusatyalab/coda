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


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <struct.h>
#include <sys/param.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>

#ifdef __cplusplus
}
#endif __cplusplus

/* interfaces */
#include <vcrcommon.h>

/* from util */
#include <proc.h>

/* from venus */
#include "advice.h"
#include "advice_daemon.h"
#include "fso.h"
#include "hdb.h"
#include "local.h"
#include "mariner.h"
#include "user.h"
#include "venus.private.h"
#include "venusrecov.h"
#include "venusvol.h"
#include "vproc.h"
#include "worker.h"

int CacheFiles = UNSET_CF;
int FSO_SWT = UNSET_SWT;
int FSO_MWT = UNSET_MWT;
int FSO_SSF = UNSET_SSF;

static int FSO_HashFN(void *);


/* Call with CacheDir the current directory. */
void FSOInit() {
    int i; 

    /* Allocate the database if requested. */
    if (InitMetaData) {					/* <==> FSDB == 0 */
	    Recov_BeginTrans();
	    RVMLIB_REC_OBJECT(FSDB);
	    FSDB = new fsdb;
	    Recov_EndTrans(0);
    }

    /* Check the persistent, but separately initializable, members. */
    Recov_BeginTrans();
    RVMLIB_REC_OBJECT(*FSDB);

    if (InitMetaData || FSDB->MaxBlocks != CacheBlocks) {
	    if (!InitMetaData)
		    eprint("Warning: CacheBlocks changing from %d to %d",
			   FSDB->MaxBlocks, CacheBlocks);
	    
	    FSDB->MaxBlocks = CacheBlocks;
    }
    FSDB->FreeBlockMargin = FSDB->MaxBlocks / FREE_FACTOR;
    
    if (InitMetaData || (FSO_SWT != UNSET_SWT && FSDB->swt != FSO_SWT))
	    FSDB->swt = (FSO_SWT == UNSET_SWT ? DFLT_SWT : FSO_SWT);
    if (InitMetaData || (FSO_MWT != UNSET_MWT && FSDB->mwt != FSO_MWT))
	    FSDB->mwt = (FSO_MWT == UNSET_MWT ? DFLT_MWT : FSO_MWT);
    if (InitMetaData || (FSO_SSF != UNSET_SSF && FSDB->ssf != FSO_SSF))
	    FSDB->ssf = (FSO_SSF == UNSET_SSF ? DFLT_SSF : FSO_SSF);
    FSDB->maxpri = FSDB->MakePri(FSO_MAX_SPRI, FSO_MAX_MPRI);
    FSDB->stdpri = FSDB->MakePri(FSO_MAX_SPRI, FSO_MAX_MPRI / 2);
    FSDB->marginpri = FSDB->MakePri(FSO_MAX_SPRI, 0);
    Recov_EndTrans(0);

    /* Initialize transient members. */
    FSDB->ResetTransient();

    /* Recover and initialize the fsobj's. */
    /* Recovery is needed because cache files are in UFS, not RVM. */
    {
	/* Validate Meta- and Non-Meta-Data version stamps. */
	if (!InitMetaData) {
	    FILE *fp = fopen("CacheInfo", "r");
	    if (fp == NULL)
		eprint("Warning: no CacheInfo file");
	    else {
		int DataVersion = 0;
		(void)fscanf(fp, "%d", &DataVersion);
		(void)fclose(fp);

		if (DataVersion != FSDB->DataVersion)
		    eprint("Warning: data version mismatch (%d, %d)",
			   DataVersion, FSDB->DataVersion);
	    }
	}

	/* Get rid of garbage in cache dir.
	   The ifdef's are necessary because the directory operations
	   have changed completely between 4.3BSD and 4.4BSD  (Satya, 8/12/96)
        */
#ifdef __BSD44__
	struct dirent **namelist;
	int nentries;
	nentries = scandir(".", &namelist, 0, 0) ;
	if (nentries < 0) CHOKE("FSOInit: scandir");

	/* Examine each entry and decide to keep or delete it */
	for (i = 0; i < nentries; i++) {
	    /* Don't unlink special files. */
	    if (STREQ(namelist[i]->d_name, ".") ||
		STREQ(namelist[i]->d_name, "..") ||
		STREQ(namelist[i]->d_name, "lost+found") ||
		STREQ(namelist[i]->d_name, LOGFILE) ||
		STREQ(namelist[i]->d_name, LOGFILE_OLD) ||
		STREQ(namelist[i]->d_name, "pid") ||
		STREQ(namelist[i]->d_name, "core"))
		goto FREE_ENTRY;

	    /* Don't unlink cache files. */
	    int ix;
	    if (namelist[i]->d_name[0] == 'V' &&
		sscanf(namelist[i]->d_name, "V%d", &ix) == 1 &&
		(ix >= 0 && ix < FSDB->MaxFiles))
		goto FREE_ENTRY;

	    /* Garbage collect everything else. */
	    ::unlink(namelist[i]->d_name);

FREE_ENTRY: /* release entry from namelist */
	    free(namelist[i]);
	}
	/* Free the array allocated by scandir() */
	free(namelist);
#endif /* __BSD44__ */

	/* Allocate the fsobj's if requested. */
	if (InitMetaData) {
	    /* Do this in a loop to avoid one mongo transaction! */
		for (i = 0; i < FSDB->MaxFiles; i++) {
			Recov_BeginTrans();
			fsobj *f = new (FROMHEAP) fsobj(i);
			Recov_EndTrans(MAXFP);
		}
	}

	/* Recover the cache files (allocates as necessary). */
	{
	    /* This is done in the subsequent fsobj recovery loop because we statically associate */
	    /* fsobj's and cache-file descriptors (i.e., we embed one of the latter in the former). */
	    /* If we were to make the association dynamic instead, we would need to iterate */
	    /* through the cache-file descriptor handles here, validating and/or resetting them. */
	}

	/* Recover the fsobj's. */
	{
	    eprint("starting FSDB scan (%d, %d) (%d, %d, %d)",
		   FSDB->MaxFiles, FSDB->MaxBlocks,
		   FSDB->swt, FSDB->mwt, FSDB->ssf);

	    /* Check entries in the table. */
	    {
		fso_iterator next(NL);
		fsobj *f;
		while ((f = next())) {
		    /* Validate the cache-file, and record its blocks. */
		    f->cf.Validate();
		    FSDB->ChangeDiskUsage(NBLOCKS(f->cf.Length()));

		    /* Initialize transient members. */
		    f->ResetTransient();

		    /* Recover object state. */
		    f->Recover();
		}

		eprint("\t%d cache files in table (%d blocks)",
		       (FSDB->htab).count(), FSDB->blocks);
	    }

	    /* Check entries on the freelist. */
	    {
		rec_olist_iterator next(FSDB->freelist);
		rec_olink *o;
		while ((o = next())) {
		    fsobj *f = strbase(fsobj, o, primary_handle);

		    /* Reset the cache file. */
		    f->cf.Reset();
		}

		eprint("\t%d cache files on free-list",
		       (FSDB->freelist).count());
	    }

	    if ((FSDB->htab).count() + (FSDB->freelist).count() != FSDB->MaxFiles)
		CHOKE("FSOInit: missing %d cache files",
		    FSDB->MaxFiles - ((FSDB->htab).count() + (FSDB->freelist).count()));
	}

	/* Recover parent <--> child bindings. */
	/* This MUST wait until all fsobj's have been recovered/reset! */
	/* 
	 * Need not be in a transaction for the call to SetParent, because
	 * the parent vnode and unique arguments are the very ones in the fsobj 
	 * (no recoverable store gets changed).
	 */
	{
	    fso_iterator next(NL);
	    fsobj *cf;
	    while ((cf = next()))
		if (!cf->IsRoot())
		    cf->SetParent(cf->pfid.Vnode, cf->pfid.Unique);
	}

	/* Recover fsobj <--> cmlent bindings: a grid-like data structure. */
	/* This MUST wait until all fsobj's and cmlent's have been recovered/reset! */
	{
	    VDB->AttachFidBindings();

	    /* Sanity check. */
	    fso_iterator next(NL);
	    fsobj *f;
	    while ((f = next())) {
		FSO_ASSERT(f,
			   (DIRTY(f) && f->mle_bindings != 0 && f->mle_bindings->count() > 0) ||
			   (!DIRTY(f) && f->mle_bindings == 0));
	    }
	}

	/* Cancel store records for any dataless dirty file. */
	{
	    fso_iterator next(NL);
	    fsobj *f;
	    while ((f = next()))
		if (DIRTY(f) && !HAVEALLDATA(f))
		    f->CancelStores();
	}
    }

    /* Set new Data version stamps. */
    int DataVersion = (int) Vtime();

    FILE *fp = fopen("CacheInfo", "w+");
    if (fp == NULL)
	CHOKE("FSOInit: fopen(CacheInfo, WR)");
    fprintf(fp, "%d", DataVersion);
    fclose(fp);

    Recov_BeginTrans();
    RVMLIB_REC_OBJECT(FSDB->DataVersion);
    FSDB->DataVersion = DataVersion;
    Recov_EndTrans(0);

    RecovFlush(1);
    RecovTruncate(1);

    /* Fire up the daemon. */
    FSOD_Init();
}


static int FSO_HashFN(void *key) {
    return(((ViceFid *)key)->Volume + ((ViceFid *)key)->Vnode);
}


int FSO_PriorityFN(bsnode *b1, bsnode *b2) {
    fsobj *f1 = strbase(fsobj, b1, prio_handle);
    fsobj *f2 = strbase(fsobj, b2, prio_handle);
    CODA_ASSERT((char *)f1 != (char *)f2);

    if (f1->priority > f2->priority) return(1);
    if (f1->priority < f2->priority) return(-1);

    /* Use "random" bits assigned at allocation to break (most) ties. */
    if (f1->flags.random > f2->flags.random) return(1);
    if (f1->flags.random < f2->flags.random) return(-1);

    /* The chance of this ever happening should be miniscule! -JJK */
/*
    eprint("FSO_PriorityFN: priorities tied (%d, %d)!",
	    f1->priority, f1->flags.random);
*/
    LOG(1, ("FSO_PriorityFN: priorities tied (%d, %d)!",
	     f1->priority, f1->flags.random));
    return(0);
}


void UpdateCacheStats(CacheStats *c, enum CacheEvent event, unsigned long blocks) {
    if (event < HIT || event > REPLACE)
	CHOKE("UpdateCacheStats: bogus event (%d)", event);

    struct CacheEventRecord *r = &c->events[event];
    r->count++;
    r->blocks += (int) blocks;
}


void PrintCacheStats(char *description, CacheStats *c, int fd) {
    fdprint(fd, "%s ", description);
    for (int i = 0/*HIT*/; i < 10/*REPLACE*/; i++)
	fdprint(fd, "[%4d : %6d]  ", c->events[i].count, c->events[i].blocks);
    fdprint(fd, "\n");
}


void VenusToViceStatus(VenusStat *venusstat, ViceStatus *vicestat/*, vuid_t vuid*/) {
    vicestat->InterfaceVersion = 1;
    vicestat->VnodeType = venusstat->VnodeType;
    vicestat->LinkCount = venusstat->LinkCount;
    vicestat->Length = venusstat->Length;
    vicestat->DataVersion = venusstat->DataVersion;
    vicestat->VV = venusstat->VV;
/*    ClearLocal(vicestat->VV);*/
    vicestat->Date = venusstat->Date;
    vicestat->Author = venusstat->Author;
    vicestat->Owner = venusstat->Owner;
/*    vicestat->CallBack = NoCallBack;*/
/*    vicestat->MyAccess = venusstat->SpecificUser[index(vuid)].rights;*/
/*    vicestat->AnyAccess = venusstat->AnyUser.rights;*/
    vicestat->Mode = venusstat->Mode;
/*    vicestat->vparent = venusstat->pfid.Vnode;*/
/*    vicestat->uparent = venusstat->pfid.Unique;*/
}


/*  *****  FSDB members  *****  */

/* Allocate database from recoverable store. */
void *fsdb::operator new(size_t len){
    fsdb *f = 0;

    /* Allocate recoverable store for the object. */
    f = (fsdb *)rvmlib_rec_malloc((int) len);
    CODA_ASSERT(f);
    return(f);
}



fsdb::fsdb() : htab(FSDB_NBUCKETS, FSO_HashFN) {

    /* Initialize the persistent members. */
    RVMLIB_REC_OBJECT(*this);
    MagicNumber = FSDB_MagicNumber;
    MaxFiles = CacheFiles;
    FreeFileMargin = MaxFiles / FREE_FACTOR;

    LastRef = (long *)rvmlib_rec_malloc(MaxFiles * (int)sizeof(long));
    rvmlib_set_range(LastRef, MaxFiles * (int)sizeof(long));
    bzero((void *)LastRef, (int)(MaxFiles * sizeof(long)));
}


void fsdb::ResetTransient() {
    /* Sanity checks. */
    if (MagicNumber != FSDB_MagicNumber)
	CHOKE("fsdb::ResetTransient: bad magic number (%d)", MagicNumber);

    /* MaxBlocks, FreeBlockMargin reset in FsoInit */
    blocks = 0;		    /* this will get updated in fsobj::Recover() */

    htab.SetHFn(FSO_HashFN);
    prioq = new bstree(FSO_PriorityFN);
    RefCounter = 0;
    for (int i = 0; i < MaxFiles; i++)
	if (LastRef[i] > RefCounter)
	    RefCounter = LastRef[i];
    RefCounter++;
    delq = new dlist;
    owriteq = new olist;

    bzero((void *)&DirAttrStats, (int)sizeof(CacheStats));
    bzero((void *)&DirDataStats, (int)sizeof(CacheStats));
    bzero((void *)&FileAttrStats, (int)sizeof(CacheStats));
    bzero((void *)&FileDataStats, (int)sizeof(CacheStats));
    Recomputes = 0;
    Reorders = 0;

    /* 
     * matriculation_sync doesn't need to be initialized. 
     * It's used only for LWP_Wait and LWP_Signal. 
     */
    matriculation_count = 0;

    struct stat tstat;
    if (::stat(".", &tstat) < 0) CHOKE("fsdb::ResetTransient: cachedir stat");
    device = tstat.st_dev;
}


void fsdb::operator delete(void *, size_t){
    abort(); /* how could we possibly get here? */
}


fsobj *fsdb::Find(ViceFid *key) {
    fso_iterator next(NL, key);
    fsobj *f;
    while ((f = next()))
	if (FID_EQ(key, &f->fid)) return(f);

    return(0);
}


/* MUST NOT be called from within transaction! */
/* Caller MUST guarantee that the volume is cached and stable! */
/* Should priority be an implicit argument? -JJK */
fsobj *fsdb::Create(ViceFid *key, LockLevel level, int priority, char *comp) {
    fsobj *f = 0;
    int rc = 0;

    /* Check whether the key is already in the database. */
    if ((f = Find(key)) != 0)
	{ f->print(logFile); CHOKE("fsdb::Create: key found"); }

    /* Fashion a new object.  This could be a long-running and wide-ranging transaction! */
    Recov_BeginTrans();
    /* try to make sure we have at least one object available, so we won't
     * crash on the C++-initializers. */
    rc = FSDB->AllocFso(priority, &f);
    if (rc != ENOSPC)
    {
	FSDB->FreeFso(f);
        f = new (FROMFREELIST, priority) fsobj(key, comp);
    }
    Recov_EndTrans(MAXFP);
    if (f != 0)
	if (level != NL) f->Lock(level);

    if (f == 0)
	LOG(0, ("fsdb::Create: (%x.%x.%x, %d) failed\n",
		key->Volume, key->Vnode, key->Unique, priority));
    return(f);
}


/* 
 * Problem here is that we *must* have a volent pointer.  If a miss is on the
 * volume itself, we don't have that pointer.  Where do we put the info?
 */

#define Hoard 1
#define NonHoard 0
#define UnknownHoard -1

#define FSOBJSIZE NBLOCKS(sizeof(fsobj))

/* Session-level STATS:                                                      *
 *                                                                           *
 * Session-level statistics are kept only for HIT, MISS and NOSPACE events.  *
 * For the purposes of these statistics, TIMEOUT and FAILURE events are      *
 * counted as MISS events.  No other events are recorded at the session      *
 * level.  Other events are recorded at the fsdb level.                      *
 *                                                                           *
 * Session-level statistics are stored at the volume-level.  Thus, if the    *
 * volent is NULL, we have nowhere to store the data.  In this situation, we *
 * store the data in the FSDB (in VolumeLevelMiss, since we assume this      *
 * situation will only occur for disconnected misses on uncached volumes).   *
 *                                                                           *
 * The Cache Statistics are maintained in the volent on a session basis.     *
 *                                                                           *
 * This routine MUST NOT be called from within a transaction!                */
void VmonUpdateSession(vproc *vp, ViceFid *key, fsobj *f, volent *vol, vuid_t vuid, enum CacheType datatype, enum CacheEvent event, unsigned long blocks) {
    vsr *record;
    int hoardstatus;
    userent *u;

    /* Return if we're not maintaining reference statistics */
    int reference = (vp->u.u_flags & REFERENCE);
    if (!reference)
        return;

    /* Return if this is the hoard thread */
    CODA_ASSERT(vp != NULL);
    if (vp->type == VPT_HDBDaemon)
        return;

    /* Return if this thread is associated with the advice monitor */
    GetUser(&u, vuid);
    CODA_ASSERT(u != NULL);
    if (u->IsAdvicePGID(vp->u.u_pgid))
        return;

    /* Return if this is for a Local volume */
    if (FID_VolIsFake(key->Volume))
        return;

    /* Validate params */
    CODA_ASSERT((datatype == ATTR) || (datatype == DATA));
    CODA_ASSERT((event >= HIT) && (event <= FAILURE));
 
    /* Okay, let's get on with the *real* reason we're here... */
    LOG(100, ("VmonUpdateSession:  key=(%x.%x.%x), vp->type=%d, vuid=%d, datatype=%d, event=%d, blocks=%d)\n", key->Volume, key->Vnode, key->Unique, vp->type, vuid, datatype, event, blocks));

    /* Do the easy stuff first */
    if (datatype == ATTR) {
        CacheStats *c = (ISDIR(*key) ? &(FSDB->DirAttrStats) : &(FSDB->FileAttrStats));
        UpdateCacheStats(c, event, blocks);
    }
    else {
        CacheStats *c = (ISDIR(*key) ? &(FSDB->DirDataStats) : &(FSDB->FileDataStats));
        UpdateCacheStats(c, event, blocks);
    }

    /* If the volume is NULL, then                                                  *
     * we count these statistics against the fsdb rather than the individual volume */
    if (vol == NULL) {
	LOG(100, ("VmonUpdateSession:  vol == NULL\n"));
        CODA_ASSERT((event == MISS) || (event == TIMEOUT) || (event == FAILURE));
        Recov_BeginTrans();
	RVMLIB_REC_OBJECT(FSDB->VolumeLevelMiss);
	FSDB->VolumeLevelMiss++;
        Recov_EndTrans(0);       
        return;
    } 

    /* Get this user's session record */
    record = vol->GetVSR(vuid);
    CODA_ASSERT(record->cetime == 0);
    LOG(100, ("VmonUpdateSession:  starttime = %d\n",record->starttime));

    /* Determine hoardstatus:  1 if hoardable; 0 if not; -1 if unknown */
    if (f) {
        if (HOARDABLE(f))
            hoardstatus = 1;                                  /* OBJ has a hoard priority! */
        else if (IndigentCount == 0)
            hoardstatus = 0;          /* OBJ has no hoard priority; all namectxts expanded */
        else
            hoardstatus = -1;        /* OBJ has no hoard priority; some namectxts indigent */
    }
    else 
            hoardstatus = -1;
    CODA_ASSERT((hoardstatus >= -1) && (hoardstatus <= 1));

    /* Update the the appropriate event */
    switch (hoardstatus) {
        case Hoard:
            switch (datatype) {
                case ATTR:
                    switch (event) {
                        case HIT:
                            record->cachestats.HoardAttrHit.Count +=1;
                            record->cachestats.HoardAttrHit.Blocks += blocks;
                            break;
                        case MISS:
                        case TIMEOUT:
                        case FAILURE:
                            record->cachestats.HoardAttrMiss.Count++;
                            record->cachestats.HoardAttrMiss.Blocks += blocks;
                            break;
                        case NOSPACE:
                            record->cachestats.HoardAttrNoSpace.Count++;
                            record->cachestats.HoardAttrNoSpace.Blocks += blocks;
                            break;
                        default: 
                            break;
                    }
                    break;
                case DATA:
                    switch (event) {
                        case HIT:
                            record->cachestats.HoardDataHit.Count++;
                            record->cachestats.HoardDataHit.Blocks += blocks;
                            break;
                        case MISS:
                        case TIMEOUT:
                        case FAILURE:
                            record->cachestats.HoardDataMiss.Count++;
                            record->cachestats.HoardDataMiss.Blocks += blocks;
                            break;
                        case NOSPACE:
                            record->cachestats.HoardDataNoSpace.Count++;
                            record->cachestats.HoardDataNoSpace.Blocks += blocks;
                            break;
                        default: 
                            break;
                    }
                    break;
            }
            break;
        case NonHoard:
            switch (datatype) {
                case ATTR:
                    switch (event) {
                        case HIT:
                            record->cachestats.NonHoardAttrHit.Count++;
                            record->cachestats.NonHoardAttrHit.Blocks += blocks;
                            break;
                        case MISS:
                        case TIMEOUT:
                        case FAILURE:
                            record->cachestats.NonHoardAttrMiss.Count++;
                            record->cachestats.NonHoardAttrMiss.Blocks += blocks;
                            break;
                        case NOSPACE:
                            record->cachestats.NonHoardAttrNoSpace.Count++;
                            record->cachestats.NonHoardAttrNoSpace.Blocks += blocks;
                            break;
                        default: 
                            break;
                    }
                    break;
                case DATA:
                    switch (event) {
                        case HIT:
                            record->cachestats.NonHoardDataHit.Count++;
                            record->cachestats.NonHoardDataHit.Blocks += blocks;
                            break;
                        case MISS:
                        case TIMEOUT:
                        case FAILURE:
                            record->cachestats.NonHoardDataMiss.Count++;
                            record->cachestats.NonHoardDataMiss.Blocks += blocks;
                            break;
                        case NOSPACE:
                            record->cachestats.NonHoardDataNoSpace.Count++;
                            record->cachestats.NonHoardDataNoSpace.Blocks += blocks;
                            break;
                        default: 
                            break;
                    }
                    break;
            }
            break;
        case UnknownHoard:
            switch (datatype) {
                case ATTR:
                    switch (event) {
                        case HIT:
                            record->cachestats.UnknownHoardAttrHit.Count++;
                            record->cachestats.UnknownHoardAttrHit.Blocks += blocks;
                            break;
                        case MISS:
                        case TIMEOUT:
                        case FAILURE:
                            record->cachestats.UnknownHoardAttrMiss.Count++;
                            record->cachestats.UnknownHoardAttrMiss.Blocks += blocks;
                            break;
                        case NOSPACE:
                            record->cachestats.UnknownHoardAttrNoSpace.Count++;
                            record->cachestats.UnknownHoardAttrNoSpace.Blocks += blocks;
                            break;
                        default: 
                            break;
                    }
                    break;
                case DATA:
                    switch (event) {
                        case HIT:
                            record->cachestats.UnknownHoardDataHit.Count++;
                            record->cachestats.UnknownHoardDataHit.Blocks += blocks;
                            break;
                        case MISS:
                        case TIMEOUT:
                        case FAILURE:
                            record->cachestats.UnknownHoardDataMiss.Count++;
                            record->cachestats.UnknownHoardDataMiss.Blocks += blocks;
                            break;
                        case NOSPACE:
                            record->cachestats.UnknownHoardDataNoSpace.Count++;
                            record->cachestats.UnknownHoardDataNoSpace.Blocks += blocks;
                            break;
                        default: 
                            break;
                    }
                    break;
            }
            break;
    }
}


/* local-repair modification */
/* last argument "rcode" added for local-repair */
/* MUST NOT be called from within transaction! */
/* Returns object READ-locked on success. */
/* 
 * Should NOT call with FSO_HOLD on object.  Venus will be unable to 
 * correctly handle objects which go inconsistent incorrectly forcing a 
 * return value of ETOOMANYREFS to the user when there is *nothing* the 
 * poor user can do about it.
 */
int fsdb::Get(fsobj **f_addr, ViceFid *key, vuid_t vuid, int rights, char *comp, int *rcode) {
    CODA_ASSERT(rights != 0);
    int getdata = (rights & RC_DATA);

    LOG(100, ("fsdb::Get-mre: key = (%x.%x.%x), uid = %d, rights = %d, comp = %s\n",
	       key->Volume, key->Vnode, key->Unique, vuid, rights, (comp ? comp : "")));

    { 	/* a special check for accessing already localized object */
	volent *vol = VDB->Find(key->Volume);
	if (vol && !vol->IsUnderRepair(ALL_UIDS) && vol->flags.has_local_subtree) {
	    lgm_iterator next(LRDB->local_global_map);
	    lgment *lgm;
	    ViceFid *gfid;
	    while ((lgm = next())) {
		gfid = lgm->GetGlobalFid();
		if (!bcmp((const void *)gfid, (const void *)key, (int)sizeof(ViceFid))) {
		    LOG(0, ("fsdb::Get: trying to access localied object 0x%x.%x.%x\n",
			    key->Volume, key->Vnode, key->Unique));
		    return EACCES;
		}
	    }
	}
    }

    int code = 0;
    *f_addr = 0;				/* OUT parameter valid on success only. */
    vproc *vp = VprocSelf();

    if (vp->type != VPT_HDBDaemon)
      NotifyUserOfProgramAccess(vuid, vp->u.u_pid, vp->u.u_pgid, key);

    /* Volume state synchronization. */
    /* If a thread is already "in" one volume, we must switch contexts before entering another. */
    if (vp->u.u_vol != 0 && (vp->u.u_vol)->vid != key->Volume) {
	/* Preserve the user context. */
	struct uarea saved_ctxt = vp->u;
	vp->u.Init();
	vp->u.u_cred = saved_ctxt.u_cred;
	vp->u.u_priority = saved_ctxt.u_priority;
	vp->u.u_flags = saved_ctxt.u_flags;
	vp->u.u_pid = saved_ctxt.u_pid;
        vp->u.u_pgid = saved_ctxt.u_pgid;

	/* Do the Get on behalf of another volume. */
	for (;;) {
	    vp->Begin_VFS(key->Volume, CODA_VGET);
	    if (vp->u.u_error) break;

	    vp->u.u_error = Get(f_addr, key, vuid, rights, comp);

	    if (vp->u.u_error != 0)
		Put(f_addr);
	    int retry_call = 0;
	    vp->End_VFS(&retry_call);
	    if (!retry_call) break;
	}
	code = vp->u.u_error;

	/* Restore the user context. */
	vp->u = saved_ctxt;

	return(code);
    }

    fsobj *f = 0;
    int	reference = (vp->u.u_flags & REFERENCE);

    /* Find the fsobj, or create a fresh one. */
RestartFind:
    f = Find(key);
    if (f == 0) {
	/* 
	 * check if the key is a locally generated fid.  We should never send
	 * these to the server.  This check is not to be confused with
	 * the routine fsobj::IsLocalFid, which checks to see if the _volume_
	 * the object belongs to is the local volume.  yuck.  --lily
	 */
	if (FID_IsDisco(key)) {
		LOG(0, ("fsdb::Get: Locally created fid %s not found!\n", 
			FID_(key)));
		return ETIMEDOUT;
	}

	/* process possible un-cached local objects */
	if (FID_VolIsLocal(key)) {
		LOG(0, ("fsdb::Get: Un-cached Local object %s\n",
			FID_(key)));
		return ETIMEDOUT;
	}

        /* Must ensure that the volume is cached. */
        volent *v = 0;
        if (VDB->Get(&v, key->Volume) != 0) {
            LOG(100, ("Volume not cached and we couldn't get it...\n"));
            DisconnectedCacheMiss(vp, vuid, key, comp);
            VmonUpdateSession(vp, key, f /* NULL */, v /* NULL */, vuid, ATTR, TIMEOUT, FSOBJSIZE);
            return(ETIMEDOUT);
        }

	/* Retry the find, in case some other thread created the object while we blocked in vdb::Get(). */
	if (Find(key) != 0) {
	    VDB->Put(&v);
	    goto RestartFind;
	}

        /* Cut-out early if volume is disconnected! */
        if ((v->state != Hoarding) && (v->state != Logging)) {
            LOG(100, ("Volume disconnected and file not cached!\n"));
            v->DisconnectedCacheMiss(vp, vuid, key, comp);
            VmonUpdateSession(vp, key, f /* NULL */, v, vuid, ATTR, TIMEOUT, FSOBJSIZE);
            VDB->Put(&v);
            return(ETIMEDOUT);
        }

	/* Attempt the create. */
	f = Create(key, RD, vp->u.u_priority, comp);
	if (f == 0) {
            VmonUpdateSession(vp, key, f /*NULL*/, v, vuid, ATTR, NOSPACE, FSOBJSIZE);
	    VDB->Put(&v);
	    return(ENOSPC);
	}

	/* Release the volume. */
	VDB->Put(&v);

	/* Transform object into fake mtpt if necessary. */
	if (FID_IsFakeRoot(key)) {
	    f->PromoteLock();
	    if (f->Fakeify()) {
		LOG(0, ("fsdb::Get: can't transform %s (%x.%x.%x) into fake mt pt\n",
			f->comp, f->fid.Volume, f->fid.Vnode, f->fid.Unique));
                VmonUpdateSession(vp, key, f, f->vol, vuid, ATTR, FAILURE, FSOBJSIZE);
		Recov_BeginTrans();
		f->Kill();
		Recov_EndTrans(MAXFP);
		Put(&f);  		 /* will unlock and garbage collect */
		return(EIO);
	    }
	    f->DemoteLock();
	}
    }
    else {
	/* Object without status must be matriculating now.  Wait for it to complete. */
	int curr_matriculation_count = matriculation_count;
	if (!HAVESTATUS(f)) {
	    while (curr_matriculation_count == matriculation_count) {
		LOG(0, ("WAITING(MATRICULATION): count = %d\n", matriculation_count));
		START_TIMING();
		VprocWait(&matriculation_sync);
		END_TIMING();
		LOG(0, ("WAIT OVER, elapsed = %3.1f\n", elapsed));
	    }
	    goto RestartFind;
	}

	/* Perform GC if necessary. */
	if (GCABLE(f)) {
	    Recov_BeginTrans();
	    f->GC();
	    Recov_EndTrans(MAXFP);
	    goto RestartFind;
	}

	/* Read-lock the entry. */
	f->Lock(RD);

	/* Update component. */
	if (comp && comp[0] != '\0' && !STREQ(comp, ".") && !STREQ(comp, "..") &&
	    !STREQ(comp, f->comp)) {
		Recov_BeginTrans();
		RVMLIB_REC_OBJECT(f->comp);
		rvmlib_rec_free(f->comp);
		int len = (int) strlen(comp) + 1;
		f->comp = (char *)rvmlib_rec_malloc(len);
		rvmlib_set_range(f->comp, len);
		strcpy(f->comp, comp);
		Recov_EndTrans(MAXFP);
	}
    }

    /* Consider fetching status and/or data. */
    if (!getdata && STATUSVALID(f)) 
        VmonUpdateSession(vp, key, f, f->vol, vuid, ATTR, HIT, FSOBJSIZE);
    else if (getdata && DATAVALID(f)) {
        VmonUpdateSession(vp, key, f, f->vol, vuid, ATTR, HIT, FSOBJSIZE);
        VmonUpdateSession(vp, key, f, f->vol, vuid, DATA, HIT, BLOCKS(f));
    }
    else {	    /* (!getdata && !STATUSVALID(f)) || (getdata && !DATAVALID(f)) */
	/* Note that we CANNOT fetch, and must use whatever status/data we have, if : */
	/*     - the file is being exec'ed (or the VM system refuses to release its pages) */
	/*     - the file is open for write */
	/*     - the object has been deleted (it must also be open for read at this point) */
	/*     - the object's volume is disconnected */
 	/*     - the object's volume is in logging mode and the object is dirty */
	if (FETCHABLE(f)) {
	    f->PromoteLock();

	    /* Fetch status-only if we don't have any or if it is suspect. */
	    /* We do this even if we want data and we don't have any so that we */
	    /* ALWAYS know how many blocks to allocate when fetching data. */
	    if (STATUSVALID(f)) 
                VmonUpdateSession(vp, key, f, f->vol, vuid, ATTR, HIT, FSOBJSIZE);
	    else { 
		code = f->GetAttr(vuid);

		if (rcode) *rcode = code;	/* added for local-repair */
		/* Conjure a fake directory to represent an inconsistent object. */
		if (code == EINCONS) { 
		    userent *u;
		    char path[MAXPATHLEN];
		    
		    f->GetPath(path,1);
		    GetUser(&u, vuid);
		    CODA_ASSERT(u != NULL);

		    LOG(0, ("fsdb::Get: Object inconsistent. (key = <%x.%x.%x>)\n", 
			    key->Volume, key->Vnode, key->Unique));
		    /* We notify all users that objects are in conflict because it is
		       often the case that uid=-1, so we notify nobody.  It'd be better
		       if we could notify the user whose activities triggered this object
		       to go inconsistent.  However, that person is difficult to determine
		       and could be the hoard daemon.  Notifying everyone seems to be
		       a reasonable alternative, if not terribly satisfying. */
		    NotifyUsersObjectInConflict(path, key);

		    k_Purge(&f->fid, 1);
                    if (f->refcnt > 1) {
		        /* 
			 *  If refcnt is greater than 1, it means we aren't the only one
			 *  with an active reference to this object.  If this is the case, 
			 *  then the following Put cannot possibly clear all the references
			 *  to this file.  If we were to go ahead and call the Create in
			 *  this situation, we'd get a fatal error ("Create: key found").
			 *  So, we return ETOOMANYREFS and put an informative message in the
			 *  log rather than allowing the fatal error.
			 */
			f->ClearRcRights();
			Put(&f);
			LOG(0, ("fsdb::Get: Object with active reference has gone inconsistent.\n\t Cannot conjure fake directory until object is inactive. (key =  <%x.%x.%x>)\n",  key->Volume, key->Vnode, key->Unique));
			return(ETOOMANYREFS);
		    }

		    Put(&f);
		    code = 0;

		    /* Attempt the create. */
                    /* N.B. The volume should be explicitly pinned here! 
                          XXX mre 10/21/94 The volume is pinned prior to fsdb::Get call */
		    /* N.B. If preceding PUT didn't clear all references,
		       and if the refcnt test above didn't catch that
		       the Put wouldn't, we're hosed!  We'll most likely
		       get a "Create: key found" fatal error. */
		    f = Create(key, RD, vp->u.u_priority, comp);
		    if (f == 0) {
                        VmonUpdateSession(vp, key, f, f->vol, vuid, ATTR, NOSPACE, FSOBJSIZE);
			return(ENOSPC);
		    }

		    /* 
		     * Transform object into fake directory.  If that doesn't work,
		     * return EIO...NOT EINCONS, which will get passed back to the 
		     * user as ENOENT (too alarming).  We must kill the object here, 
		     * otherwise Venus will think it is "matriculating" and wait 
		     * (forever) for it to finish.
		     */
		    eprint("%s (%x.%x.%x) inconsistent!",
			   f->comp, f->fid.Volume, f->fid.Vnode, f->fid.Unique);
		    f->PromoteLock();
		    if (f->Fakeify())  {
                        VmonUpdateSession(vp, key, f, f->vol, vuid, ATTR, FAILURE, FSOBJSIZE);
			Recov_BeginTrans();
			f->Kill();
			Recov_EndTrans(MAXFP);
			Put(&f);
			return(EIO);
		    }
		}

		if (code != 0) {
                    enum CacheEvent event;
                    event = (code == ERETRY ? RETRY : code == ETIMEDOUT ? TIMEOUT : FAILURE);
                    VmonUpdateSession(vp, key, f, f->vol, vuid, ATTR, event, FSOBJSIZE);
                    if (code == ETIMEDOUT) {
                      LOG(100, ("(MARIA) Code is TIMEDOUT after GetAttr...\n"));
                      f->DisconnectedCacheMiss(vp, vuid, comp);
                    }
		    Put(&f);
		    return(code);
		}

                VmonUpdateSession(vp, key, f, f->vol, vuid, ATTR, MISS, FSOBJSIZE);
	   } 

	    /* If we want data and we don't have any then fetch new stuff. */
	    /* we have to re-check FETCHABLE because it may have changed as
	       a result of the inconsistent object manipulation above. */
	    if (getdata && FETCHABLE(f)) {
		if (HAVEALLDATA(f)) {
		    /* If we have data at this point, it MUST be valid! */
                    VmonUpdateSession(vp, key, f, f->vol, vuid, DATA, HIT, BLOCKS(f));
		}
		else {
		    CacheMissAdvice advice = CoerceToMiss;

   		    if (f->vol->IsWeaklyConnected()) {
			char pathname[MAXPATHLEN];
			int hoard_priority = 0;

			f->GetPath(pathname);
			if (f->HoardPri > 0)
			    hoard_priority = f->HoardPri;
			else
			    hoard_priority = HDB->GetSuspectPriority(f->fid.Volume, pathname, vuid);

 			int estimatedCost = f->EstimatedFetchCost();
		        /* If the fetch will take too long, coerce the request into a miss */
		        if (f->PredetermineFetchState(estimatedCost, hoard_priority) != 1) {
			    advice = f->WeaklyConnectedCacheMiss(vp, vuid);
			    if (advice == CoerceToMiss) {
 			        /* Should we record this as a MISS with Vmon? */
			        Put(&f);
			        LOG(0, ("Weak Miss Coersion:\n\tObject:  %s <%x,%x,%x>\n\tEstimated Fetch Cost:  %d seconds\n\tReturn code:  EFBIG\n", 
				  comp, key->Volume, key->Vnode, key->Unique, estimatedCost));
			        MarinerLog("Weak Miss Coersion on %s <%x,%x,%x>\n",
				           comp, key->Volume, key->Vnode, key->Unique);
			        return(EFBIG);
			    }
		        }
			/* Otherwise, let fsdb::Get go ahead and fetch the object */
		    }

		    int nblocks = (int) BLOCKS(f);
		    if (f->IsFile()) {
			code = AllocBlocks(vp->u.u_priority, nblocks);
			if (code != 0) {
                            VmonUpdateSession(vp, key, f, f->vol, vuid, DATA, NOSPACE, nblocks);
			    Put(&f);
			    return(code);
			}
		    }

		    /* Make cache misses non-transparent. */
		    if (advice == CoerceToMiss)
		        advice = f->ReadDisconnectedCacheMiss(vp, vuid);
		    switch (advice) {
		        case FetchFromServers:
                            LOG(10, ("The advice was to ReadDiscFetch --> Fetching.\n"));
                            break;
		        case CoerceToMiss:
			    LOG(0, ("Read Disconnected Miss Coersion:\n\tObject:  %s <%x,%x,%x>\n\tReturn code:  EFBIG\n", 
				   comp, key->Volume, key->Vnode, key->Unique));
		            MarinerLog("Read Disconnected Miss Coersion on %s <%x,%x,%x>\n",
				       comp, key->Volume, key->Vnode, key->Unique);
			    Put(&f);
			    return(ETIMEDOUT);
		        default:
			    LOG(0, ("The advice was Unrecognized --> Fetching anyway.\n"));
			    break;
                    }
		    /* Let fsdb::Get go ahead and fetch the object */

		    code = f->Fetch(vuid);

		    /* Restart operation in case of inconsistency. */
		    if (code == EINCONS)
			code = ERETRY;

		    if (code != 0) {
                        enum CacheEvent event;
                        event = (code == ERETRY ? RETRY : code == ETIMEDOUT ? TIMEOUT : FAILURE);
                        VmonUpdateSession(vp, key, f, f->vol, vuid, DATA, event, nblocks);
                        if (code == ETIMEDOUT) 
                            f->DisconnectedCacheMiss(vp, vuid, comp);
			Put(&f);
			return(code);
		    }

                    VmonUpdateSession(vp, key, f, f->vol, vuid, DATA, MISS, nblocks);
		}
	    }

	    f->DemoteLock();
	} else {	/* !FETCHABLE(f) */
	    if (HOARDING(f) || LOGGING(f) || (EMULATING(f) && f->flags.replicated)) {
		if (HAVESTATUS(f)) {
                    VmonUpdateSession(vp, key, f, f->vol, vuid, ATTR, HIT, FSOBJSIZE); /* sort of...*/
		    /*
		     * Unfortunately, trying to limit access to stale STATUS won't work because 
		     * in order to gracefully recover from the active reference to a now 
		     * inconsistent object, we have to be able to close the object.  In order 
		     * to close the object, we have to be able to get the STATUS of the object...  
		     * I guess we allow full access to the stale STATUS, but log that we did so.
		     *
		     *   if (DYING(f)) {
		     *     LOG(0, ("Active reference prevents refetching object!  Providing limited access to stale status!\n"));
		     *     *f_addr = f;
		     *     Put(&f);
		     *     return(ETOOMANYREFS);
		     *   }
		     */
		    if (DYING(f)) 
		      LOG(0, ("Active reference prevents refetching object!  Allowing access to stale status! (key = <%x.%x.%x>)\n", key->Volume, key->Vnode, key->Unique));
		    else if (!STATUSVALID(f))
		      LOG(0, ("Allowing access to stale status! (key = <%x.%x.%x>)\n",  key->Volume, key->Vnode, key->Unique));
		}
		else {
                    f->DisconnectedCacheMiss(vp, vuid, comp);
                    VmonUpdateSession(vp, key, f, f->vol, vuid, ATTR, TIMEOUT, FSOBJSIZE);
                    Put(&f);
                    return(ETIMEDOUT);
		}

		if (getdata) {
		    if (DYING(f)) { 
		      LOG(0, ("Active reference prevents refetching object!  Disallowing access to stale data! (key = <%x.%x.%x>)\n",  key->Volume, key->Vnode, key->Unique));
		      Put(&f);
		      return(ETOOMANYREFS);
		    } 
		    if (HAVEALLDATA(f)) {
                        VmonUpdateSession(vp, key, f, f->vol, vuid, DATA, HIT, BLOCKS(f)); /* sort of... */
			if (!DATAVALID(f))
			    LOG(0, ("Allowing access to stale data! (key = <%x.%x.%x>)\n",  key->Volume, key->Vnode, key->Unique));
		    }
		    else {
                        f->DisconnectedCacheMiss(vp, vuid, comp); 
                        VmonUpdateSession(vp, key, f, f->vol, vuid, DATA, TIMEOUT, BLOCKS(f));
			Put(&f);
			return(ETIMEDOUT);
		    }
		}
	    }
	    else {
                LOG(100, ("(MARIA) TIMEOUT after something...\n"));
                f->DisconnectedCacheMiss(vp, vuid, comp);
                VmonUpdateSession(vp, key, f, f->vol, vuid, ATTR, TIMEOUT, FSOBJSIZE);
                Put(&f);
                return(ETIMEDOUT);
	    }
	}
    }

    /* Finalize handling of fake objects. */
    if (f->IsFake() && !(f->vol)->IsUnderRepair(vuid)) {
        LOG(1, ("(Puneet)fsdb::Get:Volume (%u) NOT under repair and IsFake(%x.%x.%x)\n",
		vuid, f->fid.Volume, f->fid.Vnode, f->fid.Unique));
        char path[MAXPATHLEN];
        f->GetPath(path, 1);
        LOG(1, ("(Maria, Puneet) After GetPath, path = %s\n", path));

        // object is inconsistent - try running the ASR
        // check that ASR time interval has expired
        userent *u;
        GetUser(&u, vuid);
        struct timeval tv;
        gettimeofday(&tv, 0);
        int ASRinvocable = (AdviceEnabled && ASRallowed &&
			    (vp->type == VPT_Worker) &&
			    (f->vol->IsASRAllowed()) && 
                            (!ISDIR(f->fid)) && 
                            ((tv.tv_sec - f->lastresolved) > ASR_INTERVAL) &&
			    (!u->IsAdvicePGID(vp->u.u_pgid)) &&
                            (u->IsAdviceValid(InvokeASRID,1) == TRUE));
        if (ASRinvocable) {
            LOG(1, ("InvokeASR(%s, %d)\n", path, vuid));
            if (!u->RequestASRInvokation(path, vuid)) {
                gettimeofday(&tv, 0);
                f->lastresolved = tv.tv_sec;
                code = EASRSTARTED;
            }
        } else {
            if (!AdviceEnabled) 
                u->AdviceNotEnabled();
            if (!f->vol->IsASRAllowed())
                u->ASRnotAllowed();
            if ((tv.tv_sec - f->lastresolved) <= ASR_INTERVAL)
                u->ASRintervalNotReached();
        }
        
        if (code != EASRSTARTED) {
          LOG(1, ("fsdb::Get: returning %s\n",
                  (code == EINCONS) ? "EINCONS" : "ERETRY"));
          code = (f->IsFakeDir() ? EINCONS : ERETRY);
        }
        Put(&f);
        return(code);
    }

    /* Update priority. */
    if (reference)
	f->Reference();
    f->ComputePriority();

    *f_addr = f;
    return(0);
}


/* MUST NOT be called from within transaction! */
void fsdb::Put(fsobj **f_addr) {
    if (!(*f_addr)) { LOG(100, ("fsdb::Put: Null FSO\n")); return; }

    fsobj *f = *f_addr;
    LOG(100, ("fsdb::Put: (%x.%x.%x), refcnt = %d, readers = %d, writers = %d, openers = %d\n",
	     f->fid.Volume, f->fid.Vnode, f->fid.Unique,
	     f->refcnt, f->readers, f->writers, f->openers));

    if (f->readers == 0 && f->writers == 0)
	{ f->print(logFile); CHOKE("fsdb::Put: no locks!"); }
    LockLevel level = (f->readers > 0 ? RD : WR);
    f->UnLock(level);

    /* Perform GC if necessary. */
    if (GCABLE(f)) {
	LOG(10, ("fsdb::Put: GC (%x.%x.%x)\n",
		 f->fid.Volume, f->fid.Vnode, f->fid.Unique));

	Recov_BeginTrans();
	f->GC();
	Recov_EndTrans(MAXFP);
    }

    (*f_addr) = 0;
}


/* MUST NOT be called from within transaction! */
void fsdb::Flush() {
    /*
     * don't flush volume root only because some cached objects may
     * not be reachable.  If the flush actually works, the object
     * will disappear, and some number of descendants may 
     * disappear as well.  In this case, the iterator must be 
     * restarted. We're done when there's nothing flushable left.
     */
    int restart = 1;
    while (restart) {
	fsobj *f;
	fso_iterator next(NL);
	
	restart = 0;
	while ((f = next())) 
	    if (f->Flush() == 0) {
		restart = 1;
		break;
	    }
    }
}


/* MUST NOT be called from within transaction! */
void fsdb::Flush(VolumeId vid) {
    volent *v;
    v = VDB->Find(vid);
    CODA_ASSERT(v);
    
    /* comment in fsdb::Flush applies here */
    int restart = 1;
    while (restart) {
	fsobj *f;
	fso_vol_iterator next(NL, v);
	
	restart = 0;
	while ((f = next())) 
	    if (f->Flush() == 0) {
		restart = 1;
		break;
	    }
    }
}


/* This is supports translation of "local" to "remote" Fids during 
   reintegration.  Note that given a fid it can appear in several
   directories:
   - in itself (if a directory fid) for the "." entries
   - in its directory children as ".."
   - in its parent as the named entry 
   so we must do 3 replacements

   MUST be called from within transaction! 

   This routine can als replace local fids with global ones and
   then, exceptionally, they appear as cross volume replacements.

   Returns: 0 on success, ENOENT if OldFid cannot be found in fsdb.

*/

int fsdb::TranslateFid(ViceFid *OldFid, ViceFid *NewFid) 
{
	fsobj *f = 0;
	ViceFid pFid;

	LOG(100, ("fsdb::TranslateFid: %s --> %s", FID_(OldFid), 
		  FID_2(NewFid)));

	/* cross volume replacements are for local fids */
	if (OldFid->Volume != NewFid->Volume && !FID_VolIsLocal(NewFid))
		CHOKE("fsdb::TranslateFid: X-VOLUME, %s --> %s",
		      FID_(OldFid), FID_2(NewFid));


	/* First, change the object itself. */
	f = Find(OldFid);
	if (f == 0) {
		LOG(0, ("fsdb::TranslateFid: %s not found\n",
			FID_(OldFid)));
		return(ENOENT);
	}
	
	/* Can't handle any case but reintegration. */
	/* in old versions we would choke too  if (!DIRTY(f)) */
	if (!HAVESTATUS(f)) {
		f->print(logFile); 
		CHOKE("fsdb::TranslateFid: !HAVESTATUS"); 
	}

	/* Replace the fids in kernel name cache for this object */
	k_Replace(OldFid, NewFid);
	
	/* Check that the NewFid is not already known! */
	fsobj *Newf = Find(NewFid);
	if (Newf != 0) {
		f->print(logFile); 
		Newf->print(logFile); 
		CHOKE("fsdb::TranslateFid: NewFid found"); 
	}
	
	/* Remove OldObject from table. */
	if (htab.remove(&f->fid, &f->primary_handle) != 
	    &f->primary_handle) {
		f->print(logFile); 
		CHOKE("fsdb::TranslateFid: old object remove"); 
	}

	/* Change Fid, update dir and reinsert into table. */
	RVMLIB_REC_OBJECT(f->fid);
	f->fid = *NewFid;
	
	/* replace "." and its hardlinks if f is dir */
	if (f->IsDir() && HAVEALLDATA(f) && (!f->IsMtPt())) 
		f->dir_TranslateFid(OldFid, NewFid);
	
	/* replace f in the hash table */
	htab.append(&f->fid, &f->primary_handle);
	
	/* Update the Parent. */
	pFid = f->pfid;
	fsobj *pf = Find(&pFid);
	if (pf)
		pf->dir_TranslateFid(OldFid, NewFid);

	/* Update the children, if we f is a parent. */
	if (! ISDIR(*OldFid))
		return 0; 
	
	fso_iterator next(NL);
	fsobj *cf;
	while ((cf = next())) { 
		/* this is probably not supposed to happen. Can it? (pjb) */ 
		if (! FID_EQ(&cf->pfid, OldFid))
			continue ;

		RVMLIB_REC_OBJECT(cf->pfid);
		cf->pfid = *NewFid;

		if (cf->IsDir() && HAVEALLDATA(cf) && (!cf->IsMtPt()))
			cf->dir_TranslateFid(OldFid, NewFid);
	}
	return 0;
}


/* Called in event of callback message from server. */
/* We assume this means that object IS INVALID, and therefore kill it */
/* (unless object is not replaceable, in which case we only demote it). */
/* Perhaps there should be a "MUTATED" parameter in the RPC from the server. -JJK */
int fsdb::CallBackBreak(ViceFid *fid) {

    fsobj *f = FSDB->Find(fid);
    if (f == 0 || !HAVESTATUS(f)) return(0);

    if (REPLACEABLE(f) && !BUSY(f)) {
	    Recov_BeginTrans();
	    f->Kill(0);
	    Recov_EndTrans(CMFP);
    }  else
	    f->Demote(0);

    return(1);
}


void fsdb::ResetVolume(VolumeId volume, int usecallback) {
    fso_iterator next(NL);
    fsobj *f;
    while ((f = next()))
	if (f->fid.Volume == volume) {
	    f->Demote();
	    f->flags.usecallback = usecallback;
	}
}


void fsdb::ResetUser(vuid_t vuid) {
    /* Demote access rights for the user. */
    fso_iterator next(NL);
    fsobj *f;
    while ((f = next()))
	if (f->IsDir())
	    f->DemoteAcRights(vuid);
}


void fsdb::ClearPriorities() {
    fso_iterator next(NL);
    fsobj *f;
    while ((f = next())) {
	LastRef[f->ix] = 0;
	f->ComputePriority();
    }
}


void fsdb::InvalidateMtPts() {
    fso_iterator next(NL);
    fsobj *f;
    while ((f = next()))
	if (f->IsMtPt()) {
	    f->flags.ckmtpt = 1;
	    k_Purge(&f->pfid, 1);	/* force kernel to reevaluate! */
	}

    /* N.B.  We currently have no way of invalidating the mount point for the Coda root volume. */
    /* That mount point is a VFS mount, which is known to the kernel (i.e., the MiniCache).   The */
    /* proper thing to do is have another "back-call" into the MiniCache which unsaves the cnode */
    /* thought by the kernel to be the current root, and makes a forward cfs_root call to Venus asking */
    /* for the (new) root.  Of course, Venus would also have to make another ViceGetRootVolume call */
    /* and update its global value (rootfid) as well.  Someday, someone should make the necessary */
    /* changes to the MiniCache and Venus.  In the meantime, the only way for a client to reevaluate */
    /* the Coda root is to shut down and restart Venus.  -JJK */
}


int fsdb::FreeFsoCount() {
    int count = (MaxFiles - htab.count());
    if (count != freelist.count())
	LOG(0, ("fsdb::FreeFsoCount: counts disagree (%d - %d != %d)\n",
		MaxFiles, htab.count(), freelist.count()));

    return(count);
}


/* MUST be called from within transaction! */
int fsdb::AllocFso(int priority, fsobj **outf) {
    /* Satisfy with free fso where possible. */
    if (GrabFreeFso(priority, outf))
	return(0);

    /* Maybe a garbage collection would help. */
    if (delq->count() > 0) {
	GarbageCollect();
	if (GrabFreeFso(priority, outf))
	    return(0);
    }

    /* Reclaim an object if possible. */
    /* Try regular GetDown first, specific replacement second. */
    GetDown();
    if (GrabFreeFso(priority, outf))
	return(0);
    ReclaimFsos(priority, 1);
    if (GrabFreeFso(priority, outf))
	return(0);

    /* No luck. */
    *outf = 0;
    return(ENOSPC);
}


/* MUST be called from within transaction! */
int fsdb::GrabFreeFso(int priority, fsobj **f) {
    int free_fsos = FreeFsoCount();
    if (free_fsos > FreeFileMargin ||
	 (free_fsos > 0 && priority >= MarginPri())) {
	*f = strbase(fsobj, freelist.get(), primary_handle);
	return(1);
    }

    return(0);
}


/* MUST be called from within transaction! */
void fsdb::ReclaimFsos(int priority, int count) {
    vproc *vp = VprocSelf();
    int reclaimed = 0;
    bstree_iterator next(*prioq);
    bsnode *b;
    int readahead = 0;
    while (readahead || (b = next())) {
	readahead = 0;
	fsobj *f = strbase(fsobj, b, prio_handle);

	if (!REPLACEABLE(f))
	    { f->print(logFile); CHOKE("fsdb::ReclaimFsos: !REPLACEABLE"); }

	/* Remaining replaceable entries have equal or higher priority! */
	if (vp->type == VPT_HDBDaemon) 
	    { if (priority <= f->priority) break; }

	/* Can't reclaim if busy. */
	if (BUSY(f)) continue;

	/* Reclaim fso and data. */
	readahead = ((b = next()) != 0);
	MarinerLog("cache::Replace [%s] %s [%d, %d]\n",
		   (HAVEDATA(f) ? "status/data" : "status"),
		   f->comp, f->priority, NBLOCKS(f->cf.Length()));
	UpdateCacheStats((f->IsDir() ? &DirAttrStats : &FileAttrStats),
			 REPLACE, NBLOCKS(sizeof(fsobj)));
	if (HAVEDATA(f))
	    UpdateCacheStats((f->IsDir() ? &DirDataStats : &FileDataStats),
			     REPLACE, BLOCKS(f));

	if (AdviceEnabled)
	  f->RecordReplacement(TRUE, HAVEDATA(f));

	f->Kill();
	f->GC();

	reclaimed++;
	if (reclaimed == count) break;
    }
}


/* MUST be called from within transaction! */
void fsdb::FreeFso(fsobj *f) {
    freelist.append(&f->primary_handle);
}


int fsdb::FreeBlockCount() {
    int count = MaxBlocks - blocks;

    /* Subtract out blocks belonging to objects currently open for write. */
    if (owriteq->count() > 0) {
	olist_iterator onext(*owriteq);
	olink *o;
	while ((o = onext())) {
	    fsobj *f = strbase(fsobj, o, owrite_handle);

	    if (f->flags.owrite == 0)
	    { f->print(logFile); CHOKE("fsdb::FreeBlockCount: on owriteq && !owrite"); }

	    struct stat tstat;
	    f->cf.Stat(&tstat);
	    count -= (int) NBLOCKS(tstat.st_size);
	}
    }

    return(count);
}


/* MUST NOT be called from within transaction! */
int fsdb::AllocBlocks(int priority, int nblocks) {
    /* Satisfy with free blocks where possible. */
    if (GrabFreeBlocks(priority, nblocks))
	return(0);

    /* Maybe a garbage collection would help. */
    if (delq->count() > 0) {
	Recov_BeginTrans();
	GarbageCollect();
	Recov_EndTrans(MAXFP);
	if (GrabFreeBlocks(priority, nblocks))
	    return(0);
    }

    /* Reclaim blocks if possible. */
    /* Try regular GetDown first, specific replacement second. */
    Recov_BeginTrans();
    GetDown();
    Recov_EndTrans(MAXFP);
    if (GrabFreeBlocks(priority, nblocks))
	return(0);
    Recov_BeginTrans();
    int BlocksNeeded = nblocks +
	    (priority >= MarginPri() ? 0 : FreeBlockMargin) - FreeBlockCount();
    ReclaimBlocks(priority, BlocksNeeded);
    Recov_EndTrans(MAXFP);
    if (GrabFreeBlocks(priority, nblocks))
	return(0);

    /* No luck. */
    return(ENOSPC);
}


/* Needn't be called from within transaction. */
int fsdb::GrabFreeBlocks(int priority, int nblocks) {
    int free_blocks = FreeBlockCount();
    if ((free_blocks - nblocks) >= FreeBlockMargin ||
	 (free_blocks - nblocks) >= 0 && priority >= MarginPri()) {
	ChangeDiskUsage(nblocks);
	return(1);
    }

    return(0);
}


/* MUST be called from within transaction! */
void fsdb::ReclaimBlocks(int priority, int nblocks) {
    int reclaimed = 0;
    bstree_iterator next(*prioq);
    bsnode *b;
    while ((b = next())) {
	fsobj *f = strbase(fsobj, b, prio_handle);

	if (!REPLACEABLE(f))
	    { f->print(logFile); CHOKE("fsdb::ReclaimBlocks: !REPLACEABLE"); }

	/* Remaining replaceable entries have higher priority! */
	if (priority <= f->priority) break;

	/* No point in reclaiming entries without data! */
	int ufs_blocks = NBLOCKS(f->cf.Length());
	if (ufs_blocks == 0) continue;

	/* Can't reclaim if busy. */
	if (BUSY(f)) continue;

	/* Reclaim data.  Return if we've got enough. */
	MarinerLog("cache::Replace [data] %s [%d, %d]\n",
		   f->comp, f->priority, ufs_blocks);
	UpdateCacheStats((f->IsDir() ? &DirDataStats : &FileDataStats),
			 REPLACE, BLOCKS(f));


	if (AdviceEnabled)
	  f->RecordReplacement(FALSE, TRUE);

	f->DiscardData();

	reclaimed += ufs_blocks;
	if (reclaimed >= nblocks) break;
    }
}


/* Needn't be called from within transaction. */
void fsdb::FreeBlocks(int nblocks) {
    if (nblocks < 0)
	eprint("fsdb::FreeBlocks: nblocks = %d\n", nblocks);
    ChangeDiskUsage(-nblocks);
}


/*
 *    Eventually, this should only be called by {Alloc,Free}Blocks.
 *    We currently have the following exceptions:
 *       1. When valid data is discovered at startup in the fsobj constructor
 *       2. When (re)derivating a udir
 *       3. When FetchData retrieves more data than expected (due to a
 *          write-sharing race)
 *       4. When a file opened for write is closed.
 *
 *    Items 1-2 are either inconsequential or will disappear when we switch to
 *    RVM. Item 3 is a rare event (presumably), and can be fixed either by
 *    implementing the ByteQuota feature of SFTP, or by making the version of
 *    the data requested an IN parameter to ViceFetch.  Item 4 is the sticky
 *    one.  What I have in mind is to pass back an "allowance" parameter to
 *    the VFS_OPEN(WR) call.  If the MiniCache (specifically, cfs_rdwr)
 *    detects that a write would cause the allowance to be exceeded, it makes
 *    a new call (an ioctl?) asking for more.  On VFS_CLOSE(WR), any unused
 *    allowance would be reclaimed.
 */
void fsdb::ChangeDiskUsage(int delta_blocks) {
    LOG(10, ("fsdb::ChangeDiskUsage: %d blocks\n", delta_blocks));

    blocks += delta_blocks;
}


/* fsobj and volent are both missing */
void fsdb::DisconnectedCacheMiss(vproc *vp, vuid_t vuid, ViceFid *fid, char *comp) {
    userent *u;

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
    if (u->IsAdviceValid(DisconnectedCacheMissEventID,1) != TRUE) {
        LOG(100, ("ADMON STATS:  DMQ Advice NOT valid. (uid = %d)\n", vuid));
        return;
    }

    LOG(100, ("ADMON STATS:  DMQ volent is NULL.\n"));
    u->VolumeNull();

    /* Make the request */
    LOG(100, ("Requesting Disconnected CacheMiss Questionnaire...1\n"));
    u->RequestDisconnectedQuestionnaire(fid, comp, vp->u.u_pid, Vtime());
    return;
}

void fsdb::UpdateDisconnectedUseStatistics(volent *v) {

    CODA_ASSERT(v);

    // Verify that this disconnected session is eligible.  A disconnected
    // session is eligible if there have been references to any object
    // in any volume since this volume became disconnected.  If this
    // disconnected session is not eligible, we don't want to update the
    // statistics.  (Also sanity check the disconnected reference counter.)
    CODA_ASSERT(v->DiscoRefCounter <= RefCounter);
    if (v->DiscoRefCounter == RefCounter)
        return;

  {
    LOG(3, ("Locking UPGRADE:  Please add write locks\n"));

    // This code modifies data structures in the fsobj so we should
    // iterator through the volume's fsobjs grabbing a write lock for
    // the object on which we're currently working.
    //
    // Assuming that the iterator automatically read locks objects it needs,
    // we just need to promote that lock to a write lock and demote the
    // lock when we're done changing it (as indicated below).

    fso_vol_iterator next(NL, v);
    fsobj *f;
    while ((f = next())) {
        CODA_ASSERT(f);
	CODA_ASSERT(f->vol == v);

	// Promote f to write-lock here.

	Recov_BeginTrans();
	RVMLIB_REC_OBJECT(f->DisconnectionsSinceUse);
	if (LastRef[f->ix] > v->DiscoRefCounter) {
	        // This object was used during the disconnected session
	        f->DisconnectionsUsed++;
	        f->DisconnectionsSinceUse = 0;
	} else {
	        // This object was not used during the disconnected session
	        RVMLIB_REC_OBJECT(f->DisconnectionsUnused);
	        f->DisconnectionsUnused++;
		f->DisconnectionsSinceUse++;
	}
	Recov_EndTrans(MAXFP);

	// Demote f back to read-lock here.
    }
  }
}

void fsdb::OutputDisconnectedUseStatistics(char *StatisticsFileName, int discosSinceLastUse, int percentDiscosUsed, int totalDiscosUsed) {
    FILE *StatsFILE;
    int totalUse;
    double percentUse;

    StatsFILE = fopen(StatisticsFileName, "w");
    CODA_ASSERT(StatsFILE != NULL);

    LOG(3, ("Locking UPGRADE:  Please add read locks\n"));

    // This code reads every fsobj in the fsdb.  The iterator
    // should read lock the objects as it iterates through them.

    VprocYield();
  {
    fsobj *f;
    fso_iterator next(NL);

    fprintf(StatsFILE, "<FID> priority discosSinceLastUse discosUsed discosUnused \n");

    while ((f = next())) {
      CODA_ASSERT(f);

      totalUse = f->DisconnectionsUsed + f->DisconnectionsUnused;
      LOG(0, ("%d + %d = %d\n", 
	      f->DisconnectionsUsed, f->DisconnectionsUnused, totalUse));

      if (totalUse == 0)
	percentUse = (double)0;
      else
	percentUse = (double)(f->DisconnectionsUsed * (double)100) / (double)totalUse;
      LOG(0, ("totalUse = %d ; percentUse = %g\n", totalUse, percentUse));

      if (
	  (f->DisconnectionsSinceUse > discosSinceLastUse) ||
	  ((percentUse < percentDiscosUsed) && (totalUse >= totalDiscosUsed))
	  ) {
	fprintf(StatsFILE, "%s %d %ld %ld %ld\n", 
		FID_(&f->fid),
		f->HoardPri,
		f->DisconnectionsSinceUse, 
		f->DisconnectionsUsed,
		f->DisconnectionsUnused);
      }
    }
  }
    VprocYield();

    fflush(StatsFILE);
    fclose(StatsFILE);
    return;
}

void fsdb::print(int fd, int SummaryOnly) {
    if (this == 0) return;

    fdprint(fd, "FSDB: magic = %d, data version = %d\n",
	     MagicNumber, DataVersion);
    fdprint(fd, "Files = (%d, %d, %d), Blocks = (%d, %d, %d)\n",
	     MaxFiles, htab.count(), FreeFileMargin, MaxBlocks, blocks, FreeBlockMargin);
    fdprint(fd, "Counts: fl = %d, prioq = %d, delq = %d, owq = %d\n",
	     freelist.count(), prioq->count(), delq->count(), owriteq->count());
#ifdef	VENUSDEBUG
    {
	int normal_blocks = 0;
	int got_blocks = 0;
	int udir_blocks = 0;
	int ow_blocks = 0;
	fso_iterator next(NL);
	fsobj *f;
	while ((f = next()))
	    if (HAVEDATA(f)) {
		switch(f->stat.VnodeType) {
		    case File:
			if (f->flags.owrite) {
			    struct stat tstat;
			    f->cf.Stat(&tstat);
			    ow_blocks += (int) NBLOCKS(tstat.st_size);
			} else {
			    normal_blocks += NBLOCKS(f->cf.Length());
			    got_blocks += NBLOCKS(f->stat.GotThisData);
			}
			break;

		    case Directory:
			udir_blocks += NBLOCKS(f->cf.Length());
			break;

		    case SymbolicLink:
			CODA_ASSERT(NBLOCKS(f->cf.Length()) == 0);
			break;

		    case Invalid:
			f->print(logFile);
			CHOKE("fsdb::print: bogus vnode type");
		}
	    }
	fdprint(fd, "Real Blocks: %d, %d, %d\n", got_blocks, normal_blocks, udir_blocks, ow_blocks);
    }
#endif	VENUSDEBUG
    fdprint(fd, "Cache Statistics:  [ count : blocks ]\n");
    fdprint(fd,     "                [     HIT     ]  [    MISS     ]  [    RETRY    ]  [   TIMEOUT   ]  [   NOSPACE   ]  [   FAILURE   ]  [   CREATE    ]  [    WRITE    ]  [   REMOVE    ]  [   REPLACE   ]\n");
    PrintCacheStats("Directory ATTR:", &DirAttrStats, fd);
    PrintCacheStats("Directory DATA:", &DirDataStats, fd);
    PrintCacheStats("File ATTR:     ", &FileAttrStats, fd);
    PrintCacheStats("File DATA:     ", &FileDataStats, fd);
    fdprint(fd, "VolumeLevelMisses = %d\n", VolumeLevelMiss);
    fdprint(fd, "recomputes = %d, reorders = %d, matr count = %d, device = %d\n",
	     Recomputes, Reorders, matriculation_count, device);

    if (!SummaryOnly) {
	fso_iterator next(NL);
	fsobj *f;
	while ((f = next()))
	    f->print(fd);
    }

    fdprint(fd, "\n");
}
