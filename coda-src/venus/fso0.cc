/* BLURB gpl

                           Coda File System
                              Release 7

          Copyright (c) 1987-2019 Carnegie Mellon University
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
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include "coda_string.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <struct.h>
#include <sys/param.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>

#ifdef __cplusplus
}
#endif

/* interfaces */
#include <vcrcommon.h>

/* from venus */
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

unsigned int PartialCacheFilesRatio = 0;
uint64_t WholeFileMaxSize           = 0;
uint64_t WholeFileMinSize           = 0;
uint64_t WholeFileMaxStall          = 0;
int FSO_SWT                         = UNSET_SWT;
int FSO_MWT                         = UNSET_MWT;
int FSO_SSF                         = UNSET_SSF;

/* Call with CacheDir the current directory. */
void FSOInit()
{
    unsigned int i;
    uint64_t CacheBlocks = GetVenusConf().get_int_value("cacheblocks");

    /* Allocate the database if requested. */
    if (InitMetaData) { /* <==> FSDB == 0 */
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
    FSDB->maxpri    = FSDB->MakePri(FSO_MAX_SPRI, FSO_MAX_MPRI);
    FSDB->stdpri    = FSDB->MakePri(FSO_MAX_SPRI, FSO_MAX_MPRI / 2);
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

        /* Allocate the fsobj's if requested. */
        if (InitMetaData) {
            /* Do this in a loop to avoid one mongo transaction! */
            for (i = 0; i < FSDB->MaxFiles; i++) {
                Recov_BeginTrans();
                (void)new (FROMHEAP) fsobj(i);
                Recov_EndTrans(MAXFP);
            }
        }

        /* Recover the cache files (allocates as necessary). */
        {
            /* This is done in the subsequent fsobj recovery loop because we
             * statically associate fsobj's and cache-file descriptors (i.e.,
             * we embed one of the latter in the former). If we were to make
             * the association dynamic instead, we would need to iterate
             * through the cache-file descriptor handles here, validating
             * and/or resetting them. */
        }

        /* Recover the fsobj's. */
        {
            eprint("starting FSDB scan (%d, %d) (%d, %d, %d)", FSDB->MaxFiles,
                   FSDB->MaxBlocks, FSDB->swt, FSDB->mwt, FSDB->ssf);

            /* Check entries in the table. */
            {
                fso_iterator next(NL);
                fsobj *f;
                while ((f = next())) {
                    /* Validate the cache-file, and record its blocks. */
                    f->cf.Validate();
                    FSDB->ChangeDiskUsage(NBLOCKS(f->cf.ValidData()));

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

            if (FSDB->htab.count() + FSDB->freelist.count() != FSDB->MaxFiles)
                CHOKE("FSOInit: missing %d cache files",
                      FSDB->MaxFiles -
                          (FSDB->htab.count() + FSDB->freelist.count()));
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
                if (!cf->IsExpandedObj())
                    cf->SetParent(cf->pfid.Vnode, cf->pfid.Unique);
                else
                    /* expanded objects need to be pinned down */
                    FSO_HOLD(cf);
        }

        /* Recover fsobj <--> cmlent bindings: a grid-like data structure. */
        /* This MUST wait until all fsobj's and cmlent's have been recovered/reset! */
        {
            Recov_BeginTrans();
            VDB->AttachFidBindings();
            Recov_EndTrans(0);

            /* Sanity check. */
            fso_iterator next(NL);
            fsobj *f;
            while ((f = next())) {
                FSO_ASSERT(f, (DIRTY(f) && f->mle_bindings != 0 &&
                               f->mle_bindings->count() > 0) ||
                                  (!DIRTY(f) && f->mle_bindings == 0));
            }
        }
    }

    /* Set new Data version stamps. */
    unsigned long DataVersion = (unsigned long)Vtime();

    FILE *fp = fopen("CacheInfo", "w+");
    if (fp == NULL)
        CHOKE("FSOInit: fopen(CacheInfo, WR)");
    fprintf(fp, "%ld", DataVersion);
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

static int FSO_HashFN(const void *key)
{
    VenusFid *fid = (VenusFid *)key;
    return (fid->Realm + fid->Volume + fid->Vnode);
}

int FSO_PriorityFN(bsnode *b1, bsnode *b2)
{
    fsobj *f1 = strbase(fsobj, b1, prio_handle);
    fsobj *f2 = strbase(fsobj, b2, prio_handle);
    CODA_ASSERT((char *)f1 != (char *)f2);

    if (f1->priority > f2->priority)
        return (1);
    if (f1->priority < f2->priority)
        return (-1);

    /* Use "random" bits assigned at allocation to break (most) ties. */
    if (f1->flags.random > f2->flags.random)
        return (1);
    if (f1->flags.random < f2->flags.random)
        return (-1);

    /* The chance of this ever happening should be miniscule! -JJK */
    /*
    eprint("FSO_PriorityFN: priorities tied (%d, %d)!\n",
	    f1->priority, f1->flags.random);
*/
    LOG(1, ("FSO_PriorityFN: priorities tied (%d, %d)!\n", f1->priority,
            f1->flags.random));
    return (0);
}

void UpdateCacheStats(CacheStats *c, enum CacheEvent event,
                      unsigned long blocks)
{
    if (event < HIT || event > REPLACE)
        CHOKE("UpdateCacheStats: bogus event (%d)", event);

    struct CacheEventRecord *r = &c->events[event];
    r->count++;
    r->blocks += (int)blocks;
}

void PrintCacheStats(const char *description, CacheStats *c, int fd)
{
    fdprint(fd, "%s ", description);
    for (int i = 0 /*HIT*/; i < 10 /*REPLACE*/; i++)
        fdprint(fd, "[%4d : %6d]  ", c->events[i].count, c->events[i].blocks);
    fdprint(fd, "\n");
}

void VenusToViceStatus(VenusStat *venusstat,
                       ViceStatus *vicestat /*, uid_t uid*/)
{
    vicestat->InterfaceVersion = 1;
    vicestat->VnodeType        = venusstat->VnodeType;
    vicestat->LinkCount        = venusstat->LinkCount;
    vicestat->Length           = venusstat->Length;
    vicestat->DataVersion      = venusstat->DataVersion;
    vicestat->VV               = venusstat->VV;
    /*    ClearLocal(vicestat->VV);*/
    vicestat->Date   = venusstat->Date;
    vicestat->Author = (vuid_t)venusstat->Author;
    vicestat->Owner  = (vuid_t)venusstat->Owner;
    /*    vicestat->CallBack = NoCallBack;*/
    /*    vicestat->MyAccess = venusstat->SpecificUser[index(uid)].rights;*/
    /*    vicestat->AnyAccess = venusstat->AnyUser.rights;*/
    vicestat->Mode = venusstat->Mode;
    /*    vicestat->vparent = venusstat->pfid.Vnode;*/
    /*    vicestat->uparent = venusstat->pfid.Unique;*/
}

/*  *****  FSDB members  *****  */

/* Allocate database from recoverable store. */
void *fsdb::operator new(size_t len)
{
    fsdb *f = 0;

    /* Allocate recoverable store for the object. */
    f = (fsdb *)rvmlib_rec_malloc((int)len);
    CODA_ASSERT(f);
    return (f);
}

fsdb::fsdb()
    : htab(FSDB_NBUCKETS, FSO_HashFN)
{
    /* Initialize the persistent members. */
    RVMLIB_REC_OBJECT(*this);
    MagicNumber             = FSDB_MagicNumber;
    MaxFiles                = GetVenusConf().get_int_value("cachefiles");
    WholeFileCachingMaxSize = WholeFileMaxSize;
    FreeFileMargin          = MaxFiles / FREE_FACTOR;

    LastRef = (long *)rvmlib_rec_malloc(MaxFiles * (int)sizeof(long));
    rvmlib_set_range(LastRef, MaxFiles * (int)sizeof(long));
    memset((void *)LastRef, 0, (int)(MaxFiles * sizeof(long)));
}

void fsdb::ResetTransient()
{
    /* Sanity checks. */
    if (MagicNumber != FSDB_MagicNumber)
        CHOKE("fsdb::ResetTransient: bad magic number (%d)", MagicNumber);

    /* MaxBlocks, FreeBlockMargin reset in FsoInit */
    blocks = 0; /* this will get updated in fsobj::Recover() */

    htab.SetHFn(FSO_HashFN);
    prioq      = new bstree(FSO_PriorityFN);
    RefCounter = 0;
    for (unsigned int i = 0; i < MaxFiles; i++)
        if (LastRef[i] > RefCounter)
            RefCounter = LastRef[i];
    RefCounter++;
    delq    = new dlist;
    owriteq = new olist;

    memset((void *)&DirAttrStats, 0, (int)sizeof(CacheStats));
    memset((void *)&DirDataStats, 0, (int)sizeof(CacheStats));
    memset((void *)&FileAttrStats, 0, (int)sizeof(CacheStats));
    memset((void *)&FileDataStats, 0, (int)sizeof(CacheStats));
    Recomputes = 0;
    Reorders   = 0;

    /*
     * matriculation_sync doesn't need to be initialized.
     * It's used only for LWP_Wait and LWP_Signal.
     */
    matriculation_count = 0;
}

void fsdb::operator delete(void *)
{
    abort(); /* how could we possibly get here? */
}

fsobj *fsdb::Find(const VenusFid *key)
{
    VenusFid OldFid;

    fso_iterator next(NL, key);
    fsobj *f;
    while ((f = next()))
        if (FID_EQ(key, &f->fid))
            return f;

    /* If we were looking for a local fid, do a full search because we may have
     * translated it while the upcall was in transit. */
    OldFid = *key;
    if (FID_IsDisco(MakeViceFid(&OldFid))) {
        fso_iterator full_search(NL);
        while ((f = full_search())) {
            OldFid.Realm  = f->fid.Realm;
            OldFid.Volume = f->fid.Volume;
            OldFid.Vnode  = f->LocalFid_Vnode;
            OldFid.Unique = f->LocalFid_Unique;

            if (FID_EQ(key, &OldFid)) {
                k_Replace(&OldFid, &f->fid);
                return f;
            }
        }
    }
    return 0;
}

/* MUST NOT be called from within transaction! */
/* Caller MUST guarantee that the volume is cached and stable! */
/* Should priority be an implicit argument? -JJK */
fsobj *fsdb::Create(VenusFid *key, int priority, const char *comp,
                    VenusFid *parent)
{
    fsobj *f = 0;
    int rc   = 0;

    /* Check whether the key is already in the database. */
    if ((f = Find(key)) != NULL) {
        f->print(logFile);
        CHOKE("fsdb::Create: key found");
    }

    /* Fashion a new object.  This could be a long-running and wide-ranging transaction! */
    Recov_BeginTrans();
    /* try to make sure we have at least one object available, so we won't
     * crash on the C++-initializers. */
    rc = FSDB->AllocFso(priority, &f);
    if (rc != ENOSPC) {
        FSDB->FreeFso(f);
        f = new (FROMFREELIST, priority) fsobj(key, comp);
    }

    if (f && !f->IsRoot()) {
        if (!parent) {
            /* Laboriously scan database to find our parent! */
            struct dllist_head *p;
            list_for_each(p, f->vol->fso_list)
            {
                fsobj *pf = list_entry_plusplus(p, fsobj, vol_handle);

                if (!pf->IsDir() || pf->IsMtPt())
                    continue;
                if (!HAVEALLDATA(pf))
                    continue;
                if (!pf->dir_IsParent(&f->fid))
                    continue;

                parent = &pf->fid;
                break; /* Found! */
            }
        }
        if (parent)
            f->SetParent(parent->Vnode, parent->Unique);
    }
    Recov_EndTrans(MAXFP);

    if (!f)
        LOG(0, ("fsdb::Create: (%s, %d) failed\n", FID_(key), priority));
    else
        LOG(100, ("fsdb::Create: (%s, %d) succeeded\n", FID_(key), priority));

    return (f);
}

/*
 * Problem here is that we *must* have a volent pointer.  If a miss is on the
 * volume itself, we don't have that pointer.  Where do we put the info?
 */

#define Hoard 1
#define NonHoard 0
#define UnknownHoard -1

#define FSOBJSIZE NBLOCKS(sizeof(fsobj))

/* local-repair modification */
/* argument "rcode" added for local-repair */
/* MUST NOT be called from within transaction! */
/* Returns object READ-locked on success. */
/*
 * Should NOT call with FSO_HOLD on object.  Venus will be unable to
 * correctly handle objects which go inconsistent incorrectly forcing a
 * return value of ETOOMANYREFS to the user when there is *nothing* the
 * poor user can do about it.
 */
int fsdb::Get(fsobj **f_addr, VenusFid *key, uid_t uid, int rights,
              const char *comp, VenusFid *parent, int *rcode,
              int GetInconsistent)
{
    int getdata = (rights & RC_DATA);
    int code    = 0;
    *f_addr     = 0; /* OUT parameter valid on success only. */
    vproc *vp   = VprocSelf();

    LOG(100, ("fsdb::Get: key = (%s), uid = %d, rights = %d, comp = %s\n",
              FID_(key), uid, rights, comp));

    /* if (vp->type != VPT_HDBDaemon)
     *  NotifyUserOfProgramAccess(uid, vp->u.u_pid, vp->u.u_pgid, key); */

    /* Volume state synchronization. */
    /* If a thread is already "in" one volume, we must switch contexts
     * before entering another. */
    if (vp->u.u_vol && !(vp->u.u_vol->GetRealmId() == key->Realm &&
                         vp->u.u_vol->GetVolumeId() == key->Volume)) {
        /* Preserve the user context. */
        struct uarea saved_ctxt = vp->u;
        vp->u.Init();
        vp->u.u_uid      = saved_ctxt.u_uid;
        vp->u.u_priority = saved_ctxt.u_priority;
        vp->u.u_flags    = saved_ctxt.u_flags;
        vp->u.u_pid      = saved_ctxt.u_pid;
        vp->u.u_pgid     = saved_ctxt.u_pgid;

        /* Do the Get on behalf of another volume. */
        for (;;) {
            int retry_call;

            vp->Begin_VFS(MakeVolid(key), CODA_VGET);
            if (vp->u.u_error)
                break;

            vp->u.u_error = Get(f_addr, key, uid, rights, comp, parent, rcode,
                                GetInconsistent);

            if (vp->u.u_error != 0)
                Put(f_addr);

            retry_call = 0;
            vp->End_VFS(&retry_call);
            if (!retry_call)
                break;
        }
        code = vp->u.u_error;

        /* Restore the user context. */
        vp->u = saved_ctxt;

        return (code);
    }

    fsobj *f      = 0;
    int reference = (vp->u.u_flags & REFERENCE);

    /* Find the fsobj, or create a fresh one. */
RestartFind:
    f = Find(key);

    if (f == NULL) {
        /* if it's in the local realm and the repair vol, and is a fake root,
	   it must have left a dangling reference during a collapse */
        if (FID_IsExpandedDir(key)) {
            LOG(0, ("Failed to get (%s), probably a collapsed expansion dir!\n",
                    FID_(key)));

            return (EIO);
        }

        /*
	 * check if the key is a locally generated fid.  We should never send
	 * these to the server.  This check is not to be confused with
	 * the routine fsobj::IsLocalFid, which checks to see if the _volume_
	 * the object belongs to is the local volume.  yuck.  --lily
	 */
        if (FID_IsDisco(MakeViceFid(key))) {
            LOG(0,
                ("fsdb::Get: Locally created fid %s not found!\n", FID_(key)));
            return ETIMEDOUT;
        }

        /* Must ensure that the volume is cached. */
        volent *v = 0;
        if (VDB->Get(&v, MakeVolid(key))) {
            LOG(100, ("Volume not cached and we couldn't get it...\n"));
            return (ETIMEDOUT);
        }

        /* Retry the find, in case some other thread created the object while
         * we blocked in vdb::Get(). */
        if (Find(key)) {
            VDB->Put(&v);
            goto RestartFind;
        }

        if (v->IsResolving()) {
            LOG(0,
                ("Volume resolving and file not cached, retrying VDB->Get!\n"));
            VDB->Put(&v);
            return (ERETRY);
        }

        /* Cut-out early if volume is unreachable! */
        if (v->IsUnreachable()) {
            LOG(100, ("Volume unreachable and file not cached!\n"));
            VDB->Put(&v);
            return (ETIMEDOUT);
        }

        /* Attempt the create. */
        f = Create(key, vp->u.u_priority, comp, parent);

        /* Release the volume. */
        VDB->Put(&v);

        if (!f)
            return (ENOSPC);

        /* Transform object into fake mtpt if necessary (for /coda) */
        /* The first clause catches the /coda root, the second catches
	   realms as they are demand loaded */
        if (FID_IsLocalFake(key) || FID_IsFakeRoot(MakeViceFid(key))) {
            LOG(0, ("fsdb::Get: transforming %s (%s) with Fakeify()\n",
                    f->GetComp(), FID_(&f->fid)));
            if (f->Fakeify(uid)) {
                LOG(0, ("fsdb::Get: can't transform %s (%s) into fake mt pt\n",
                        f->GetComp(), FID_(&f->fid)));
                Recov_BeginTrans();
                f->Kill();
                Recov_EndTrans(MAXFP);
                Put(&f); /* will unlock and garbage collect */
                return (EIO);
            }
        }
        f->DemoteLock();
    } else {
        /* Object without status must be matriculating now.
         * Wait for it to complete. */
        int curr_matriculation_count = matriculation_count;
        if (!HAVESTATUS(f) && !f->IsFake()) {
            while (curr_matriculation_count == matriculation_count) {
                LOG(0, ("WAITING(MATRICULATION): count = %d\n",
                        matriculation_count));
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
        if (comp && comp[0] != '\0' && !STREQ(comp, ".") &&
            !STREQ(comp, "..") && !STREQ(comp, f->comp)) {
            Recov_BeginTrans();
            f->SetComp(comp);
            Recov_EndTrans(MAXFP);
        }

        /* Update parent linkage */
        if (parent && !f->pfso) {
            Recov_BeginTrans();
            f->SetParent(parent->Vnode, parent->Unique);
            Recov_EndTrans(MAXFP);
        }
    }

    /* Consider fetching status and/or data. */
    if ((!getdata && !STATUSVALID(f)) || (getdata && !DATAVALID(f))) {
        /* Note that we CANNOT fetch, and must use whatever status/data we
         * have, if : */
        /*     - the file is being exec'ed (or the VM system refuses to release
         *       its pages) */
        /*     - the file is open for write */
        /*     - the object has been deleted (it must also be open for read at
         *       this point) */
        /*     - the object's volume is unreachable */
        /*     - the object's volume is reachable, but the object is dirty */
        if (FETCHABLE(f)) {
            f->PromoteLock();

            /* Fetch status-only if we don't have any or if it is suspect. We
	     * do this even if we want data and we don't have any so that we
	     * ALWAYS know how many blocks to allocate when fetching data. */
            if (!STATUSVALID(f)) {
                code = f->GetAttr(uid);

                if (rcode)
                    *rcode = code; /* added for local-repair */

                /* Mark fsobj in server/server conflict */
                if (code == EINCONS && !f->IsFake()) {
                    char path[MAXPATHLEN];
                    f->GetPath(path, PATH_FULL);

                    Recov_BeginTrans();
                    RVMLIB_REC_OBJECT(f->flags);
                    f->flags.fake = 1;
                    Recov_EndTrans(MAXFP);

                    k_Purge(&f->fid, 0);

                    LOG(0, ("fsdb::Get: %s (%s) in server/server conflict\n",
                            path, FID_(key)));
                    MarinerLog("fsobj::CONFLICT (server/server): %s (%s)\n",
                               path, FID_(key));
                } /* s/s conflict objs fall through if(GetInconsistent) */

                if (code) {
                    /* wakeup threads that may still be stuck waiting for
		     * matriculation of this object */
                    matriculation_count++;
                    VprocSignal(&matriculation_sync);
                }

                if (code && !(code == EINCONS && GetInconsistent)) {
                    if (code == EINCONS)
                        LOG(0, ("fsdb::Get: EINCONS after GetAttr\n"));
                    if (code == ETIMEDOUT)
                        LOG(100, ("fsdb::Get: TIMEDOUT after GetAttr\n"));
                    Put(&f);
                    return (code);
                }
            }

            f->UpdateVastroFlag(uid);

            /* If we want data and we don't have any then fetch new stuff. */
            /* we have to re-check FETCHABLE because it may have changed as
	       a result of the inconsistent object manipulation above. */
            if (getdata && FETCHABLE(f) && !f->IsFake() && !HAVEALLDATA(f)) {
                /* If we haven't got any data yet, allocate enough for the
		 * whole file. When we have a partial file, we should
		 * already have reserved enough blocks. */
                if (f->IsFile() && !HAVEDATA(f) && !ISVASTRO(f)) {
                    code = AllocBlocks(vp->u.u_priority, BLOCKS(f));
                    if (code != 0) {
                        Put(&f);
                        return (code);
                    }
                }

                code = 0;
                /* first try the LookAside cache */
                if (!f->LookAside()) {
                    if (ISVASTRO(
                            f)) { /* Create empty container file for VASTROS */
                        Recov_BeginTrans();
                        LOG(0,
                            ("fsdb::Get: Creating Container file for VASTRO Comp: %s, Length: %d\n",
                             f->comp, f->stat.Length));
                        int fd = f->GetContainerFD();
                        CODA_ASSERT(fd != -1);
                        f->cf.Close(fd);
                        Recov_EndTrans(MAXFP);
                    } else {
                        /* Let fsobj::Fetch go ahead and fetch the object */
                        code = f->Fetch(uid);
                    }
                }

                /* Restart operation in case of inconsistency. */
                if (code == EINCONS)
                    code = ERETRY;

                if (code != 0) {
                    Put(&f);
                    return (code);
                }
            }

            f->DemoteLock();
        } else { /* !FETCHABLE(f) */
            if (RESOLVING(f)) {
                LOG(100, ("(MARIA) TIMEOUT !fetchable and resolving...\n"));
                Put(&f);
                return (ETIMEDOUT);
            }

            if (!HAVESTATUS(f) && !f->IsFake()) {
                Put(&f);
                return (ETIMEDOUT);
            }

            /*
	     * Unfortunately, trying to limit access to stale STATUS
	     * won't work because in order to gracefully recover from
	     * the active reference to a now inconsistent object, we
	     * have to be able to close the object.  In order to close
	     * the object, we have to be able to get the STATUS of the
	     * object...  I guess we allow full access to the stale
	     * STATUS, but log that we did so.
	     *
	     *   if (DYING(f)) {
	     *     LOG(0, ("Active reference prevents refetching object!  Providing limited access to stale status!\n"));
	     *     *f_addr = f;
	     *     Put(&f);
	     *     return(ETOOMANYREFS);
	     *   }
	     */
            if (DYING(f))
                LOG(0, ("Active reference prevents refetching object! "
                        "Allowing access to stale status! (key = <%s>)\n",
                        FID_(key)));

            else if (!STATUSVALID(f) && !f->IsLocalObj())
                LOG(0, ("Allowing access to stale status! (key = <%s>)\n",
                        FID_(key)));

            if (getdata) {
                if (DYING(f)) {
                    LOG(0, ("Active reference prevents refetching object! "
                            "Disallowing access to stale data! (key = <%s>)\n",
                            FID_(key)));
                    Put(&f);
                    return (ETOOMANYREFS);
                }

                if (!HAVEALLDATA(f)) {
                    int found = 0;

                    /* try the lookaside cache */
                    if (!f->IsLocalObj() && !f->IsFake()) {
                        f->PromoteLock();
                        found = f->LookAside();
                        f->DemoteLock();
                    }

                    if (!found) {
                        Put(&f);
                        return (ETIMEDOUT);
                    }
                }

                if (!DATAVALID(f) && !f->IsLocalObj())
                    LOG(0, ("Allowing access to stale data! (key = <%s>)\n",
                            FID_(key)));
            }
        }
    }

    /* Examine the possibility of executing an ASR. */
    if (!GetInconsistent && f->IsFake() && f->vol->IsReplicated() &&
        !f->IsExpandedObj()) {
        int ASRInvokable;
        repvol *v;
        struct timeval tv;
        fsobj *realobj;

        LOG(0, ("fsdb::Get:Volume NOT under repair and IsFake(%s)\n",
                FID_(&f->fid)));

        /* At this point, we have Fakeify'd the real object, and f is the
	 * fake object served in its place. Unfortunately, it is not linked
	 * into the directory structure and therefore calls to GetPath fail.
	 * Find the underlying object, and call LaunchASR on that to get
	 * the required information. */

        realobj = Find(key);
        if (!realobj) {
            LOG(0, ("fsdb::Get:Find failed!\n"));
            Put(&f);
            return EINCONS;
        }

        v = (repvol *)realobj->vol;
        gettimeofday(&tv, 0);

        /* Check that:
	 * 1.) An ASRLauncher path was parsed in venus.conf.
	 * 2.) This thread is a worker.
	 * 3.) ASR's are allowed and enabled to execute within this volume.
	 * 4.) An ASR is not currently running within this volume.
	 * 5.) The timeout interval for ASR launching has expired.
	 */

        ASRInvokable = ((ASRLauncherFile != NULL) && (vp->type == VPT_Worker) &&
                        v->IsASRAllowed() && !v->asr_running() &&
                        ((tv.tv_sec - realobj->lastresolved) > ASR_INTERVAL) &&
                        v->IsASREnabled() && (ASRPolicyFile != NULL));

        if (ASRLauncherFile == NULL)
            LOG(0,
                ("fsdb::Get: asrlauncher_file not specified in venus.conf!\n"));

        if (ASRPolicyFile == NULL)
            LOG(0,
                ("fsdb::Get: asrpolicy_file not specified in venus.conf!\n"));

        if (vp->type != VPT_Worker)
            LOG(0, ("fsdb::Get: Non-worker Thread\n"));

        if (!v->IsASREnabled())
            LOG(0, ("fsdb::Get: ASRs disabled by the system at the moment \n"));

        if (v->asr_running())
            LOG(0, ("fsdb::Get: ASR already running in this volume\n"));

        if (((tv.tv_sec - realobj->lastresolved) <= ASR_INTERVAL))
            LOG(0, ("fsdb::Get: ASR executed too recently for this object\n"
                    "fsdb::Get: New time: %d\tOld time: %d\tDiff:%d\n",
                    tv.tv_sec, realobj->lastresolved,
                    tv.tv_sec - realobj->lastresolved));

        if (!v->IsASRAllowed())
            LOG(0, ("fsdb::Get: ASRs disabled in this volume by some user\n"));

        if (v->asr_running() && vp->u.u_pgid != v->asr_pgid())
            code = ERETRY; /* Bounce out anything which tries to hold
				* kernel locks while repairing. */

        else if (ASRInvokable) { /* Execute ASR. */
            LOG(0, ("fsdb::Get: Launching for (%s)... \n", FID_(key)));
            if (realobj->LaunchASR(SERVER_SERVER, realobj->IsDir() ?
                                                      DIRECTORY_CONFLICT :
                                                      FILE_CONFLICT) == 0)
                code = ERETRY; /* wait a short duration and retry */
            else
                code = EINCONS;
        } else {
            LOG(0, ("fsdb::Get: ASR not invokable for %s\n", FID_(key)));
            code = EINCONS;
        }

        Put(&f);
        return (code);
    }

    /* Update priority. */
    if (reference)
        f->Reference();
    f->ComputePriority();

    *f_addr = f;
    return (0);
}

/* MUST NOT be called from within transaction! */
void fsdb::Put(fsobj **f_addr)
{
    if (!(*f_addr)) {
        LOG(100, ("fsdb::Put: Null FSO\n"));
        return;
    }

    fsobj *f = *f_addr;
    LOG(100,
        ("fsdb::Put: (%s), refcnt = %d, readers = %d, writers = %d, openers = %d\n",
         FID_(&f->fid), f->refcnt, f->readers, f->writers, f->openers));

    if (f->readers == 0 && f->writers == 0) {
        f->print(logFile);
        CHOKE("fsdb::Put: no locks!");
    }
    LockLevel level = (f->readers > 0 ? RD : WR);
    f->UnLock(level);

    /* Perform GC if necessary. */
    if (GCABLE(f)) {
        LOG(10, ("fsdb::Put: GC (%s)\n", FID_(&f->fid)));

        Recov_BeginTrans();
        f->GC();
        Recov_EndTrans(MAXFP);
    }

    (*f_addr) = 0;
}

/* MUST NOT be called from within transaction! */
void fsdb::Flush()
{
    /*
     * don't flush volume root only because some cached objects may
     * not be reachable.  If the flush actually works, the object
     * will disappear, and some number of descendants may
     * disappear as well.  In this case, the iterator must be
     * restarted. We're done when there's nothing flushable left.
     */
    int restart = 1;
    while (restart) {
        fsobj *f = NULL;
        fso_iterator next(NL);

        restart = 0;
        while ((f = next())) {
            /* If the fsobj is an active VASTRO don't flush it!
             * Flushing an fsobj destroys the fsobj's state and
             * removes it from the fsdb when Kill() is called.
             * Simply discard the data and skip it. */
            if (ISVASTRO(f) && ACTIVE(f)) {
                Recov_BeginTrans();
                f->DiscardData();
                Recov_EndTrans(MAXFP);

                /* Since the fsobj wasn't removed from the fsdb
                 * there's no need to restart. Continue with next
                 * object instead. Breaking here could prevent us
                 * to flush further fsobj from the fsdb. */
                continue;
            }

            if (f->Flush() == 0) {
                restart = 1;
                break;
            }
        }
    }
}

/* MUST NOT be called from within transaction! */
void fsdb::Flush(Volid *vid)
{
    volent *v;
    v = VDB->Find(vid);
    if (!v)
        return;

    /* comment in fsdb::Flush applies here */
    int restart = 1;
    while (restart) {
        struct dllist_head *p, *next;
        restart = 0;

        for (p = v->fso_list.next; p != &v->fso_list; p = next) {
            fsobj *n = NULL, *f = list_entry_plusplus(p, fsobj, vol_handle);
            next = p->next;

            if (next != &v->fso_list) {
                n = list_entry_plusplus(next, fsobj, vol_handle);
                FSO_HOLD(n);
            }

            if (ISVASTRO(f) && ACTIVE(f)) {
                Recov_BeginTrans();
                f->DiscardData();
                Recov_EndTrans(MAXFP);
            } else if (f->Flush() == 0) {
                restart = 1;
            }

            if (n)
                FSO_RELE(n);
        }
    }
    v->release();
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

int fsdb::TranslateFid(VenusFid *OldFid, VenusFid *NewFid)
{
    fsobj *f = 0;
    VenusFid pFid;

    LOG(100, ("fsdb::TranslateFid: %s --> %s\n", FID_(OldFid), FID_(NewFid)));

    /* cross volume replacements are for local fids */
    if (!FID_VolEQ(OldFid, NewFid) && NewFid->Realm != LocalRealm->Id())
        CHOKE("fsdb::TranslateFid: X-VOLUME, %s --> %s", FID_(OldFid),
              FID_(NewFid));

    /* First, change the object itself. */
    f = Find(OldFid);
    if (f == NULL) {
        LOG(0, ("fsdb::TranslateFid: %s not found\n", FID_(OldFid)));
        return (ENOENT);
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
    if (htab.remove(&f->fid, &f->primary_handle) != &f->primary_handle) {
        f->print(logFile);
        CHOKE("fsdb::TranslateFid: old object remove");
    }

    /* An upcall may already be queued with the old local fid. Or we may
     * already be reintegrating before the local fid has been passed back
     * to the kernel.
     *
     * To avoid this race we have to remember the value of OldFid. If it
     * was a localfid (reintegration related) it will have the same volume
     * id, a special file or directory vnode value, and we only need to
     * remember the uniquifier value.
     *
     * We may need the vnode for repair related objects. */
    f->LocalFid_Vnode  = OldFid->Vnode;
    f->LocalFid_Unique = OldFid->Unique;

    /* Change Fid, update dir and reinsert into table. */
    RVMLIB_REC_OBJECT(f->fid);
    f->fid = *NewFid;

    /* replace "." and its hardlinks if f is dir */
    if (f->IsDir() && HAVEALLDATA(f) && !f->IsMtPt())
        f->dir_TranslateFid(OldFid, NewFid);

    /* replace f in the hash table */
    htab.append(&f->fid, &f->primary_handle);

    /* Update the Parent. */
    pFid      = f->pfid;
    fsobj *pf = Find(&pFid);
    if (pf)
        pf->dir_TranslateFid(OldFid, NewFid);

    /* Update the children, if we are a directory. */
    if (ISDIR(*OldFid) && f->children) {
        dlist_iterator next(*(f->children));
        dlink *d;
        while ((d = next())) {
            fsobj *cf = strbase(fsobj, d, child_link);
            CODA_ASSERT(FID_EQ(&cf->pfid, OldFid));
            RVMLIB_REC_OBJECT(cf->pfid);
            cf->pfid = *NewFid;

            if (cf->IsDir() && HAVEALLDATA(cf) && !cf->IsMtPt())
                cf->dir_TranslateFid(OldFid, NewFid);
        }
    }
    return 0;
}

/* Called in event of callback message from server. */
/* We assume this means that the object needs to be revalidated on the next
 * access and remove the callback status flags (Demote it). -JH */
/* Perhaps there should be a "MUTATED" parameter in the RPC from the server.
 * -JJK */
int fsdb::CallBackBreak(const VenusFid *fid)
{
    fsobj *f = Find(fid);
    if (!f || !HAVESTATUS(f))
        return (0);

    f->Demote();

    return (1);
}

void fsdb::ResetUser(uid_t uid)
{
    /* Clear access rights for the user. */
    fso_iterator next(NL);
    fsobj *f;

    Recov_BeginTrans();
    while ((f = next()))
        if (f->IsDir())
            f->ClearAcRights(uid);
    Recov_EndTrans(0);
}

void fsdb::ClearPriorities()
{
    fso_iterator next(NL);
    fsobj *f;
    while ((f = next())) {
        LastRef[f->ix] = 0;
        f->ComputePriority();
    }
}

void fsdb::InvalidateMtPts()
{
    fso_iterator next(NL);
    fsobj *f;
    while ((f = next()))
        if (f->IsMtPt()) {
            f->flags.ckmtpt = 1;
            k_Purge(&f->pfid, 1); /* force kernel to reevaluate! */
        }

    /* N.B.  We currently have no way of invalidating the mount point
     * for the Coda root volume. That mount point is a VFS mount, which
     * is known to the kernel (i.e., the MiniCache).   The proper thing
     * to do is have another "back-call" into the MiniCache which
     * unsaves the cnode thought by the kernel to be the current root,
     * and makes a forward cfs_root call to Venus asking for the (new)
     * root.  Of course, Venus would also have to make another
     * ViceGetRootVolume call and update its global value (rootfid) as
     * well.  Someday, someone should make the necessary changes to the
     * MiniCache and Venus.  In the meantime, the only way for a client
     * to reevaluate the Coda root is to shut down and restart Venus.
     * -JJK */
}

int fsdb::FreeFsoCount()
{
    int count = (MaxFiles - htab.count());
    if (count != freelist.count())
        LOG(0, ("fsdb::FreeFsoCount: counts disagree (%d - %d != %d)\n",
                MaxFiles, htab.count(), freelist.count()));

    return (count);
}

/* MUST be called from within transaction! */
int fsdb::AllocFso(int priority, fsobj **outf)
{
    /* Satisfy with free fso where possible. */
    if (GrabFreeFso(priority, outf))
        return (0);

    /* Maybe a garbage collection would help. */
    if (delq->count() > 0) {
        GarbageCollect();
        if (GrabFreeFso(priority, outf))
            return (0);
    }

    /* Reclaim an object if possible. */
    /* Try regular GetDown first, specific replacement second. */
    GetDown();
    if (GrabFreeFso(priority, outf))
        return (0);
    ReclaimFsos(priority, 1);
    if (GrabFreeFso(priority, outf))
        return (0);

    /* No luck. */
    *outf = 0;
    return (ENOSPC);
}

/* MUST be called from within transaction! */
int fsdb::GrabFreeFso(int priority, fsobj **f)
{
    int free_fsos = FreeFsoCount();
    if (free_fsos > FreeFileMargin ||
        (free_fsos > 0 && priority >= MarginPri())) {
        *f = strbase(fsobj, freelist.get(), primary_handle);
        return (1);
    }

    return (0);
}

/* MUST be called from within transaction! */
void fsdb::ReclaimFsos(int priority, int count)
{
    vproc *vp     = VprocSelf();
    int reclaimed = 0;
    bstree_iterator next(*prioq);
    bsnode *b, *bnext;

    bnext = next();
    while ((b = bnext) != NULL) {
        bnext    = next();
        fsobj *f = strbase(fsobj, b, prio_handle);

        if (!REPLACEABLE(f)) {
            f->print(logFile);
            CHOKE("fsdb::ReclaimFsos: !REPLACEABLE");
        }

        /* Remaining replaceable entries have equal or higher priority! */
        if (vp->type == VPT_HDBDaemon) {
            if (priority <= f->priority)
                break;
        }

        /* Can't reclaim if busy. */
        if (BUSY(f))
            continue;

        /* Reclaim fso and data. */
        MarinerLog("cache::Replace [%s] %s [%d, %d]\n",
                   (HAVEDATA(f) ? "status/data" : "status"), f->GetComp(),
                   f->priority, NBLOCKS(f->cf.ValidData()));
        UpdateCacheStats((f->IsDir() ? &DirAttrStats : &FileAttrStats), REPLACE,
                         NBLOCKS(sizeof(fsobj)));
        if (HAVEDATA(f))
            UpdateCacheStats((f->IsDir() ? &DirDataStats : &FileDataStats),
                             REPLACE, BLOCKS(f));

        f->Kill();
        f->GC();

        reclaimed++;
        if (reclaimed == count)
            break;
    }
}

/* MUST be called from within transaction! */
void fsdb::FreeFso(fsobj *f)
{
    freelist.append(&f->primary_handle);
}

int fsdb::FreeBlockCount()
{
    int count = MaxBlocks - blocks;

    /* Subtract out blocks belonging to objects currently open for write. */
    if (owriteq->count() > 0) {
        olist_iterator onext(*owriteq);
        olink *o;
        while ((o = onext())) {
            fsobj *f = strbase(fsobj, o, owrite_handle);

            if (f->flags.owrite == 0) {
                f->print(logFile);
                CHOKE("fsdb::FreeBlockCount: on owriteq && !owrite");
            }

            struct stat tstat;
            f->cf.Stat(&tstat);
            count -= (int)NBLOCKS(tstat.st_size);
        }
    }

    return (count);
}

int fsdb::DirtyBlockCount()
{
    unsigned int count = 0;

    /* Count blocks belonging to non replaceable objects. */
    fso_iterator next(NL);
    fsobj *f;
    while ((f = next())) {
        if ((!REPLACEABLE(f) && !f->IsSymLink()) || ISVASTRO(f)) {
            count += NBLOCKS(FS_BLOCKS_ALIGN(f->cf.ValidData()));
        }
    }

    return (count);
}

/* MUST NOT be called from within transaction! */
int fsdb::AllocBlocks(int priority, int nblocks)
{
    /* Satisfy with free blocks where possible. */
    if (GrabFreeBlocks(priority, nblocks))
        return (0);

    /* Maybe a garbage collection would help. */
    if (delq->count() > 0) {
        Recov_BeginTrans();
        GarbageCollect();
        Recov_EndTrans(MAXFP);
        if (GrabFreeBlocks(priority, nblocks))
            return (0);
    }

    /* Reclaim blocks if possible. */
    /* Try regular GetDown first, specific replacement second. */
    Recov_BeginTrans();
    GetDown();
    Recov_EndTrans(MAXFP);
    if (GrabFreeBlocks(priority, nblocks))
        return (0);
    Recov_BeginTrans();
    int BlocksNeeded = nblocks +
                       (priority >= MarginPri() ? 0 : FreeBlockMargin) -
                       FreeBlockCount();
    ReclaimBlocks(priority, BlocksNeeded);
    Recov_EndTrans(MAXFP);
    if (GrabFreeBlocks(priority, nblocks))
        return (0);

    /* No luck. */
    return (ENOSPC);
}

/* Needn't be called from within transaction. */
int fsdb::GrabFreeBlocks(int priority, int nblocks)
{
    int remaining_blocks = FreeBlockCount() - nblocks;
    if (remaining_blocks >= FreeBlockMargin ||
        (remaining_blocks >= 0 && priority >= MarginPri())) {
        ChangeDiskUsage(nblocks);
        return (1);
    }

    return (0);
}

/* MUST be called from within transaction! */
void fsdb::ReclaimBlocks(int priority, int nblocks)
{
    int reclaimed = 0;
    bstree_iterator next(*prioq);
    bsnode *b;
    while ((b = next())) {
        fsobj *f = strbase(fsobj, b, prio_handle);

        if (!REPLACEABLE(f)) {
            f->print(logFile);
            CHOKE("fsdb::ReclaimBlocks: !REPLACEABLE");
        }

        /* Remaining replaceable entries have higher priority! */
        if (priority <= f->priority)
            break;

        /* No point in reclaiming entries without data! */
        int ufs_blocks = NBLOCKS(f->cf.ValidData());
        if (ufs_blocks == 0)
            continue;

        /* Can't reclaim if it's local */
        if (f->IsLocalObj())
            continue;

        /* Can't reclaim if busy. */
        if (BUSY(f) && !ISVASTRO(f))
            continue;

        /* Reclaim data.  Return if we've got enough. */
        MarinerLog("cache::Replace [data] %s [%d, %d]\n", f->GetComp(),
                   f->priority, ufs_blocks);

        f->DiscardData();

        reclaimed += ufs_blocks - f->cf.ValidData();

        UpdateCacheStats((f->IsDir() ? &DirDataStats : &FileDataStats), REPLACE,
                         reclaimed);

        if (reclaimed >= nblocks)
            break;
    }
}

/* Needn't be called from within transaction. */
void fsdb::FreeBlocks(int nblocks)
{
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
void fsdb::ChangeDiskUsage(int delta_blocks)
{
    LOG(10, ("fsdb::ChangeDiskUsage: %d blocks\n", delta_blocks));

    blocks += delta_blocks;
}

void fsdb::print(int fd, int SummaryOnly)
{
    fdprint(fd, "FSDB: magic = %d, data version = %d\n", MagicNumber,
            DataVersion);
    fdprint(fd, "Files = (%d, %d, %d), Blocks = (%d, %d, %d)\n", MaxFiles,
            htab.count(), FreeFileMargin, MaxBlocks, blocks, FreeBlockMargin);
    fdprint(fd, "Counts: fl = %d, prioq = %d, delq = %d, owq = %d\n",
            freelist.count(), prioq->count(), delq->count(), owriteq->count());
#ifdef VENUSDEBUG
    {
        int normal_blocks = 0;
        int got_blocks    = 0;
        int udir_blocks   = 0;
        int ow_blocks     = 0;
        fso_iterator next(NL);
        fsobj *f;
        while ((f = next()))
            if (HAVEDATA(f)) {
                switch (f->stat.VnodeType) {
                case File:
                    if (f->flags.owrite) {
                        struct stat tstat;
                        f->cf.Stat(&tstat);
                        ow_blocks += (int)NBLOCKS(tstat.st_size);
                    } else {
                        normal_blocks += NBLOCKS(f->cf.Length());
                        got_blocks += NBLOCKS(f->cf.ValidData());
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
        fdprint(
            fd,
            "Real Blocks: validdata %d, allocated %d, directory %d, owrite %d\n",
            got_blocks, normal_blocks, udir_blocks, ow_blocks);
    }
#endif /* VENUSDEBUG */
    fdprint(fd, "Cache Statistics:  [ count : blocks ]\n");
    fdprint(fd,
            "                [     HIT     ]  [    MISS     ]"
            "  [    RETRY    ]  [   TIMEOUT   ]  [   NOSPACE   ]"
            "  [   FAILURE   ]  [   CREATE    ]  [    WRITE    ]"
            "  [   REMOVE    ]  [   REPLACE   ]\n");
    PrintCacheStats("Directory ATTR:", &DirAttrStats, fd);
    PrintCacheStats("Directory DATA:", &DirDataStats, fd);
    PrintCacheStats("File ATTR:     ", &FileAttrStats, fd);
    PrintCacheStats("File DATA:     ", &FileDataStats, fd);
    fdprint(fd, "VolumeLevelMisses = %d\n", VolumeLevelMiss);
    fdprint(fd, "recomputes = %d, reorders = %d, matr count = %d\n", Recomputes,
            Reorders, matriculation_count);

    if (!SummaryOnly) {
        fso_iterator next(NL);
        fsobj *f;
        while ((f = next()))
            f->print(fd);
    }

    fdprint(fd, "\n");
}
