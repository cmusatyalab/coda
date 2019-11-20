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
 *    Hoard database (HDB) management.
 *
 *    The HDB supports the following commands:
 *       Add - add a new entry to the database
 *       Delete - remove an entry from the database
 *       Clear - delete all entries for a particular user (or all users)
 *       List - list all entries for a particular user (or all users)
 *       Walk - bring the database into equilibrium (wrt priorities) with the fso cache
 *       Verify - report any uncached or suspect objects mentioned in the database
 *	 Enable - Enable the periodic hoard walks
 *	 Disable - Disable the periodic hoard walks
 *
 *    The protection restrictions enforced by this code are that the real uid of the
 *    issuer must be root or an authorized user in order to add entries or walk the
 *    database or to delete, clear, list or verify entries other than his/her own.
 *    An authorized user is a user who is either logged into the console or who is
 *    considered the primary user of this workstation (as set by a runtime switch).
 *
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
#include <errno.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <struct.h>
#include <sys/param.h>
#include <unistd.h>
#include <stdlib.h>
#include <pwd.h>
#include <fcntl.h>
#ifndef __FreeBSD__
#include <utmp.h>
#endif

#include <codadir.h>

#ifdef __cplusplus
}
#endif

/* interfaces */
#include <vice.h>

/* from venus */
#include "fso.h"
#include "hdb.h"
#include "mariner.h"
#include "tallyent.h"
#include "user.h"
#include "venus.private.h"
#include "venusrecov.h"
#include "venusvol.h"
#include "vproc.h"
#include "worker.h"
#include "realmdb.h"

int MetaExpand(struct DirEntry *entry, long hook);

#ifdef VENUSDEBUG
int NameCtxt_allocs   = 0;
int NameCtxt_deallocs = 0;
#endif

/*  *****  Private Constants  *****  */

static const int HDB_YIELDMASK = 0x1; /* yield every 2 iterations */

/*  *****  Private Variables  *****  */

static int ValidCount          = 0; /* number of valid name-ctxts */
static int SuspectCount        = 0; /* number of suspect name-ctxts */
/* static */ int IndigentCount = 0; /* number of indigent name-ctxts */
/* no longer static (see fso0.c) */
static int InconsistentCount = 0; /* number of inconsistent name-ctxts */
static int MetaNameCtxts     = 0; /* number of outstanding meta name-ctxts */
static int MetaExpansions    = 0; /* number of meta-expansions performed */

static int SearchForNOreFind;

uid_t hdb::PrimaryUser = UNSET_PRIMARYUSER;

/*  *****  HDB Maintenance  ******  */

void HDB_Init()
{
    static bool InitMetaData = GetVenusConf().get_bool_value("initmetadata");
    SearchForNOreFind = GetVenusConf().get_int_value("SearchForNOreFind");
    /* Allocate the database if requested. */
    if (InitMetaData) { /* <==> HDB == 0 */
        Recov_BeginTrans();
        RVMLIB_REC_OBJECT(HDB);
        HDB = new hdb;
        Recov_EndTrans(0);
    }

    hdb::PrimaryUser = GetVenusConf().get_int_value("primaryuser");

    /* Initialize transient members. */
    HDB->ResetTransient();

    /* Scan the hdbent's. */
    {
        eprint("starting HDB scan");

        /* Check entries in the table. */
        {
            hdb_iterator next;
            hdbent *h;
            while ((h = next()))
                /* Initialize transient members. */
                h->ResetTransient();

            eprint("\t%d hdb entries in table", HDB->htab.count());
        }

        /* Check entries on the freelist. */
        {
            /* Nothing useful to do! */

            eprint("\t%d hdb entries on free-list", HDB->freelist.count());
        }
    }

    RecovFlush(1);
    RecovTruncate(1);

    /* Fire up the daemon. */
    HDBD_Init();
}

static int HDB_HashFN(const void *key)
{
    hdb_key *k = (hdb_key *)key;
    int value  = k->vid;
    int rlen   = strlen(k->realm);
    int len    = strlen(k->name);

    for (int i = 0; i < rlen; i++)
        value += (int)((k->realm)[i]);

    for (int i = 0; i < len; i++)
        value += (int)((k->name)[i]);

    return value;
}

static void HoardWalkProgress(int fetched, int total)
{
    static int lastPercentage = 0;
    int thisPercentage;

    if (total != 0)
        thisPercentage = (int)((double)fetched * (double)100 / (double)total);
    else
        thisPercentage = 100;

    if (thisPercentage == lastPercentage)
        return;
    lastPercentage = thisPercentage;

    LOG(0, ("HoardWalkProgress(%d)\n", thisPercentage));
    MarinerLog("progress::hoarding %d%%\n", thisPercentage);
}

/* Allocate database from recoverable store. */
void *hdb::operator new(size_t len)
{
    hdb *h = 0;

    /* Allocate recoverable store for the object. */
    h = (hdb *)rvmlib_rec_malloc((int)len);
    CODA_ASSERT(h);
    return (h);
}

hdb::hdb()
    : htab(HDB_NBUCKETS, HDB_HashFN)
{
    /* Initialize the persistent members. */
    RVMLIB_REC_OBJECT(*this);
    MagicNumber          = HDB_MagicNumber;
    MaxHDBEs             = GetVenusConf().get_int_value("hoard_entries");
    TimeOfLastDemandWalk = 0;
}

void hdb::ResetTransient()
{
    /* Sanity checks. */
    if (MagicNumber != HDB_MagicNumber)
        CHOKE("hdb::ResetTransient: bad magic number (%d)", MagicNumber);

    htab.SetHFn(HDB_HashFN);
    prioq              = new bstree(NC_PriorityFN);
    NumHoardWalkAdvice = 0;

    SolicitAdvice = -1;
}

void hdb::operator delete(void *deadobj)
{
    abort(); /* couldn't possibly get here! */
}

hdbent *hdb::Find(VolumeId vid, char *realm, char *name)
{
    class hdb_key key(vid, realm, name);
    hdb_iterator next(&key);
    hdbent *h;
    while ((h = next()))
        if (vid == h->vid && STREQ(realm, h->realm) && STREQ(name, h->name))
            return (h);

    return (0);
}

/* MUST NOT be called from within transaction! */
hdbent *hdb::Create(VolumeId vid, char *realm, char *name, uid_t local_id,
                    int priority, int expand_children, int expand_descendents)
{
    hdbent *h = 0;

    /* Check whether the key is already in the database. */
    if ((h = Find(vid, realm, name)) != 0) {
        h->print(GetLogFile());
        CHOKE("hdb::Create: key exists");
    }

    /* Fashion a new object. */
    Recov_BeginTrans();
    h = new hdbent(vid, realm, name, local_id, priority, expand_children,
                   expand_descendents);
    Recov_EndTrans(DMFP);

    if (h == 0)
        LOG(0, ("hdb::Create: (%x@%s, %s) failed\n", vid, realm, name));
    return (h);
}

int hdb::Add(hdb_add_msg *m, uid_t local_id)
{
    LOG(10, ("hdb::Add: <%x@%s, %s, %d, %d, %d>\n", m->vid, m->realm, m->name,
             m->priority, m->attributes, local_id));

    /* See if an entry already exists.  If it does, try to delete it. */
    hdbent *h;
    h = Find(m->vid, m->realm, m->name);

    if (h) {
        LOG(1, ("hdb::Add: (%x@%s, %s, %d) already exists (%d)\n", m->vid,
                m->realm, m->name, local_id, h->uid));

        hdb_delete_msg dm;
        dm.vid = m->vid;
        strcpy(dm.realm, m->realm);
        strcpy(dm.name, m->name);
        int code = Delete(&dm, local_id);
        if (code != 0)
            return (code);

        h = 0;
    }

    /* Create a new entry. */
    int expand_children    = 0;
    int expand_descendents = 0;
    if (m->attributes & H_INHERIT) {
        if (m->attributes & H_DESCENDENTS)
            expand_descendents = 1;
        else if (m->attributes & H_CHILDREN)
            expand_children = 1;
    }
    h = Create(m->vid, m->realm, m->name, local_id, m->priority,
               expand_children, expand_descendents);
    if (!h)
        return (ENOSPC);

    return (0);
}

int hdb::Delete(hdb_delete_msg *m, uid_t local_id)
{
    LOG(10, ("hdb::Delete: <%x@%s, %s, %d>\n", m->vid, m->realm, m->name,
             local_id));

    /* Look up the entry. */
    hdbent *h;
    h = Find(m->vid, m->realm, m->name);
    if (h == 0) {
        LOG(1, ("hdb::Delete: (%x@%s, %s, %d) not found\n", m->vid, m->realm,
                m->name, local_id));
        return (ENOENT);
    }

    /* Can only delete one's own entries unless root or authorized user. */
    if (local_id != h->uid && local_id != V_UID && !AuthorizedUser(local_id)) {
        LOG(1, ("hdb::Delete: (%x@%s, %s, %d) not authorized\n", m->vid,
                m->realm, m->name, local_id));
        return (EACCES);
    }

    Recov_BeginTrans();
    delete h;
    Recov_EndTrans(DMFP);

    return (0);
}

/* cuid = ANYUSER_UID is a wildcard meaning "clear all entries." */
extern FILE *GetLogFile();
int hdb::Clear(hdb_clear_msg *m, uid_t local_id)
{
    LOG(10, ("hdb::Clear: <%d, %d>\n", m->cuid, local_id));

    /* Can only clear one's own entries unless root or authorized user. */
    if (local_id != m->cuid && local_id != V_UID && !AuthorizedUser(local_id)) {
        LOG(1, ("hdb::Clear: (%d, %d) not authorized\n", m->cuid, local_id));
        return (EACCES);
    }

    /* Reorganized this loop to move the ATOMIC wrapper around the loop, rather
       than around the delete within.  Otherwise, clearing a large database of
       hoard entries is painfully slow!   mre (10/97) */
    Recov_BeginTrans();
    hdb_iterator next(m->cuid);
    hdbent *h = next();
    while (h) {
        hdbent *succ = next();
        delete h;
        h = succ;
    }
    Recov_EndTrans(MAXFP);
    Recov_SetBound(DMFP);

    return (0);
}

/* luid = ANYUSER_UID is a wildcard meaning "list all entries." */
int hdb::List(hdb_list_msg *m, uid_t local_id)
{
    LOG(10, ("hdb::List: <%s, %d, %d>\n", m->outfile, m->luid, local_id));

    /* Can only list one's own entries unless root. */
    if (local_id != m->luid && local_id != V_UID && !AuthorizedUser(local_id)) {
        LOG(1, ("hdb::List: (%s, %d) not authorized\n", m->outfile, m->luid,
                local_id));
        return (EACCES);
    }

    /* Open the list file. */
    int outfd =
        ::open(m->outfile, O_TRUNC | O_WRONLY | O_CREAT | O_BINARY, 0600);
    if (outfd < 0) {
        LOG(1, ("hdb::List: (%s, %d, %d) open failed (%d)\n", m->outfile,
                m->luid, local_id, errno));
        return (errno);
    }

    /* set ownership of file to actual owner */
#ifndef __CYGWIN32__
    ::fchown(outfd, local_id, (gid_t)-1);
#else
    ::chown(m->outfile, local_id, (gid_t)-1);
#endif

    /* Dump the entries. */
    hdb_iterator next(m->luid);
    hdbent *h;
    while ((h = next()))
        h->print(outfd);

    /* Close the list file. */
    if (::close(outfd) < 0)
        CHOKE("hdb::List: close(%s) failed (%d)\n", m->outfile, errno);

    return (0);
}

void hdb::SetDemandWalkTime()
{
    /* Potential problem:  What if user demands hoard walk while disconnected? */
    Recov_BeginTrans();
    RVMLIB_REC_OBJECT(HDB->TimeOfLastDemandWalk);
    TimeOfLastDemandWalk = Vtime();
    Recov_EndTrans(0);
}

long hdb::GetDemandWalkTime()
{
    return (HDB->TimeOfLastDemandWalk);
}

/* Ensure status is valid for all cached objects. */
void hdb::ValidateCacheStatus(vproc *vp, int *interrupt_failures,
                              int *statusBytesFetched)
{
    int validations = 0;

    fso_iterator next(NL);
    fsobj *f, *g;
    while ((f = next())) {
        if (STATUSVALID(f))
            continue;

        /* skip non-cacheable objects */
        if (!f->vol->IsReadWrite())
            continue;

        /* Set up uarea. */
        vp->u.Init();
        vp->u.u_uid      = f->HoardVuid;
        vp->u.u_priority = f->priority;

        /* Perform a vget(). */
        LOG(1, ("hdb::Walk: vget(%s, %d, %d, %d)\n", FID_(&f->fid), f->priority,
                f->HoardVuid, f->stat.Length));
        VenusFid tfid = f->fid;
        for (;;) {
            vp->Begin_VFS(&tfid, CODA_VGET);
            if (vp->u.u_error)
                break;

            fsobj *tf     = 0;
            vp->u.u_error = FSDB->Get(&tf, &tfid, vp->u.u_uid, RC_STATUS);

            if (tf)
                statusBytesFetched += tf->stat.Length;

            FSDB->Put(&tf);
            int retry_call;
            vp->End_VFS(&retry_call);

            if (!retry_call)
                break;
        }

        if (vp->u.u_error == EINCONS)
            k_Purge(&tfid, 1);
        LOG(1, ("hdb::Walk: vget returns %s\n", VenusRetStr(vp->u.u_error)));

        /* Yield periodically. */
        validations++;
        if ((validations & HDB_YIELDMASK) == 0)
            VprocYield();

        /* Find our fsobj and make sure it's still the same pointer. */
        g = FSDB->Find(&tfid);
        if ((g != NULL) && (f != NULL) && (g == f)) {
            if (FID_EQ(&g->fid, &f->fid))
                continue; /* Everything looks okay -- continue on our merry way */
        }

        /* Essentially, the else's of the above nested if's */
        (*interrupt_failures)++;
        if (g == NULL) {
            LOG(0, ("Hoard Walk interrupted -- object missing! <%s>\n",
                    FID_(&tfid)));
        } else {
            LOG(0, ("Hoard Walk interrupted -- object different! <%s>\n",
                    FID_(&g->fid)));
        }
        LOG(0, ("Number of interrupt failures = %d\n", *interrupt_failures));

        /*
	 * Find some interesting info.  My goal is to see if there
	 * might be a way we can test for the missing object without
	 * having to reFind the object.
	 */
        if (SearchForNOreFind) {
            if (!f)
                LOG(0, ("HoardWalk f was NULL!"));
            else {
                if (f->state == FsoRunt)
                    LOG(0, ("HoardWalk f is FsoRunt! tfid=<%s> f->fid=<%s>",
                            FID_(&tfid), FID_(&f->fid)));
                if (f->fid.Volume == 0)
                    LOG(0, ("HoardWalk vid=0, f->fid=<%s>", FID_(&f->fid)));
                f->print();
            }
        }

        /* Now do the reset. */
        next.Reset(); /* f now points to never-never-land. */
    }
}

void hdb::ListPriorityQueue()
{
    bstree_iterator next(*prioq, BstDescending);
    bsnode *b;
    while ((b = next())) {
        namectxt *n = strbase(namectxt, b, prio_handle);
        n->print(GetLogFile());
    }
}

/* Walk the priority queue.  Enter clean-up mode upon ENOSPC failure. */
void hdb::WalkPriorityQueue(vproc *vp, int *expansions, int *enospc_failure)
{
    bstree_iterator next(*prioq, BstDescending);
    bsnode *b, *bnext;
    namectxt *prev = NULL;
    int cleaning   = 0;

    bnext = next();
    while ((b = bnext)) {
        namectxt *n = strbase(namectxt, b, prio_handle);

        if (prev)
            prev->release();
        prev = n;
        n->hold();

        /* Yield periodically. */
        (*expansions)++;
        if (((*expansions) & HDB_YIELDMASK) == 0)
            VprocYield();

        /* Skip over indigent contexts in cleaning mode. */
        if (cleaning && n->state == PeIndigent) {
            bnext = next();
            continue;
        }

        /* Validate/Expand this context. */
        pestate next_state = n->CheckExpansion();

        /* Enter clean-up mode when a check fails due to ENOSPC! */
        if (vp->u.u_error == ENOSPC) {
            cleaning          = 1;
            (*enospc_failure) = 1;
        }

        /* Take transition and release context. */
        bnext = next();
        n->Transit(next_state);
    }
    if (prev)
        prev->release();
}

int hdb::CalculateTotalBytesToFetch()
{
    int total = 0;

    fso_iterator next(NL);
    fsobj *f;
    while ((f = next())) {
        CODA_ASSERT(f != NULL);
        if (DATAVALID(f))
            continue;
        total += f->stat.Length;
    }
    LOG(100, ("L hdb::CalculateTotalBytesToFetch() returning %d\n", total));
    return (total);
}

void hdb::StatusWalk(vproc *vp, int *TotalBytesToFetch, int *BytesFetched)
{
    MarinerLog("cache::BeginStatusWalk [%d]\n   [%d, %d, %d, %d] [%d]\n",
               FSDB->htab.count(), ValidCount, SuspectCount, IndigentCount,
               InconsistentCount, MetaNameCtxts);
    START_TIMING();
    int StartingMetaNameCtxts  = MetaNameCtxts;
    int StartingMetaExpansions = MetaExpansions;
    int iterations             = 0;
    int expansions             = 0;
    int enospc_failure;
    int interrupt_failures = 0; /* Count times object disappears during Yield */

    /* An iteration of this outer loop brings the cache into status equilibrium */
    /* PROVIDED that no new suspect transitions occurred in the process. */
#define MAX_SW_ITERATIONS 5 /* XXX - JJK */
    do {
        iterations++;
        enospc_failure = 0;

        /* Ensure status is valid for all cached objects. */
        ValidateCacheStatus(vp, &interrupt_failures, BytesFetched);

        /* Walk the priority queue.  Enter clean-up mode upon ENOSPC failure. */
        WalkPriorityQueue(vp, &expansions, &enospc_failure);

    } while (SuspectCount > 0 && iterations < MAX_SW_ITERATIONS);

    *TotalBytesToFetch = CalculateTotalBytesToFetch() + *BytesFetched;
    HoardWalkProgress(*BytesFetched, *TotalBytesToFetch);

    END_TIMING();
    LOG(100,
        ("hdb::StatusWalk: iters= %d, exps= %d, elapsed= %3.1f, intrpts= %d\n",
         iterations, expansions, elapsed, interrupt_failures));
    int DeltaMetaExpansions = MetaExpansions - StartingMetaExpansions;
    int DeltaMetaContractions =
        StartingMetaNameCtxts + DeltaMetaExpansions - MetaNameCtxts;

    /* pick up volume callbacks if needed */
    /* if all the objects in the cache had callbacks, might not have checked. */
    /* XXX write a lighter weight version of this! */
    VDB->TakeTransition();

    char ibuf[80 + MAXPATHLEN];
    strcpy(ibuf, "\n");
    if (enospc_failure) {
        /* Find first indigent namectxt (for informational purposes only). */
        Realm *realm;
        bstree_iterator next(*prioq, BstDescending);
        bsnode *b;
        namectxt *indigentnc = 0;
        while ((b = next())) {
            namectxt *n = strbase(namectxt, b, prio_handle);

            if (n->state == PeIndigent) {
                indigentnc = n;
                break;
            }
        }
        if (indigentnc == 0)
            CHOKE(
                "hdb::Walk: enospc_failure but no indigent namectxts on queue");

        realm = REALMDB->GetRealm(indigentnc->cdir.Realm);
        sprintf(ibuf, "\n   ENOSPC:  [%08x@%s, %s], [%d, %d]\n",
                indigentnc->cdir.Volume, realm->Name(), indigentnc->path,
                FSDB->MakePri(0, indigentnc->priority), indigentnc->uid);
        realm->PutRef();
    }
    MarinerLog(
        "cache::EndStatusWalk [%d]\n   [%d, %d, %d, %d] [%d, %d, %d] [%d, %d, %1.1f]%s",
        FSDB->htab.count(), ValidCount, SuspectCount, IndigentCount,
        InconsistentCount, MetaNameCtxts, DeltaMetaExpansions,
        DeltaMetaContractions, iterations, expansions, elapsed / 1000, ibuf);
    if (SuspectCount)
        eprint("MAX_SW_ITERATIONS reached, SuspectCount=%d!!!", SuspectCount);
}

void TallyAllHDBentries(dlist *hdb_bindings, int blocks, TallyStatus status)
{
    dlist_iterator next_hdbent(*hdb_bindings);
    dlink *d;

    while ((d = next_hdbent())) {
        binding *b   = strbase(binding, d, bindee_handle);
        namectxt *nc = (namectxt *)b->binder;
        Tally(nc->GetPriority(), nc->GetUid(), blocks, status);
    }
}

void hdb::DataWalk(vproc *vp, int TotalBytesToFetch, int BytesFetched)
{
    MarinerLog("cache::BeginDataWalk [%d]\n", FSDB->blocks);
    START_TIMING();
    int iterations          = 0;
    int prefetches          = 0;
    int s_prefetches        = 0;
    int s_prefetched_blocks = 0;
    int enospc_failure;

    for (int iterate = 1; iterate;) {
        iterations++;
        iterate        = 0;
        enospc_failure = 0;

        LOG(0,
            ("DataWalk:  Restarting Iterator!!!!  Reset availability status information.\n"));
        InitTally(); // Delete old list and start over
        TallyPrint(hdb::PrimaryUser);

        bstree_iterator next(*FSDB->prioq, BstDescending);
        bsnode *b = 0;
        while ((b = next())) {
            fsobj *f = strbase(fsobj, b, prio_handle);
            CODA_ASSERT(f != NULL);
            int blocks = BLOCKS(f);

            if (!HOARDABLE(f))
                continue;

            if (ISVASTRO(f)) {
                LOG(200,
                    ("Warning (%s) flagged as VASTRO, not being hoarded.\n",
                     f->comp));
                continue;
            }

            if (DATAVALID(f)) {
                LOG(200,
                    ("AVAILABLE:  fid=<%s> comp=%s priority=%d blocks=%d\n",
                     FID_(&f->fid), f->comp, f->priority, blocks));
                TallyAllHDBentries(f->hdb_bindings, blocks, TSavailable);
                continue;
            }

            /* Set up uarea. */
            vp->u.Init();
            vp->u.u_uid      = f->HoardVuid;
            vp->u.u_priority = f->priority;

            /* Prefetch the object.  This is like vproc::vget(), only we want the data. */
            LOG(1, ("hdb::Walk: prefetch(%s, %d, %d, %d)\n", FID_(&f->fid),
                    f->priority, f->HoardVuid, f->stat.Length));
            VenusFid tfid = f->fid;
            for (;;) {
                vp->Begin_VFS(&tfid, CODA_VGET);
                if (vp->u.u_error)
                    break;

                fsobj *tf     = 0;
                vp->u.u_error = FSDB->Get(&tf, &tfid, vp->u.u_uid, RC_DATA);
                FSDB->Put(&tf);
                int retry_call = 0;
                vp->End_VFS(&retry_call);
                if (!retry_call)
                    break;
            }
            if (vp->u.u_error == EINCONS)
                k_Purge(&tfid, 1);
            LOG(1, ("hdb::Walk: prefetch returns %s\n",
                    VenusRetStr(vp->u.u_error)));

            /* Yield periodically. */
            prefetches++;
            if ((prefetches & HDB_YIELDMASK) == 0)
                VprocYield();

            /* Reacquire reference to object. */
            f = FSDB->Find(&tfid);

            if (vp->u.u_error == 0) {
                s_prefetches++;
                if (f)
                    s_prefetched_blocks += (int)BLOCKS(f);
            }

            /* Abandon the iteration when a prefetch fails due to ENOSPC. */
            if (vp->u.u_error == ENOSPC) {
                enospc_failure = 1;
                break;
            }

            /* Object not on prioq --> iterator no longer valid */
            if (f == 0 || !REPLACEABLE(f)) {
                LOG(0,
                    ("hdb::Walk: (%s) !FOUND or !REPLACEABLE after prefetch\n",
                     FID_(&tfid)));
                iterate = 1;
                break;
            }

            /* Record availability of this object. */
            CODA_ASSERT(f != 0);
            blocks = BLOCKS(f);
            if (DATAVALID(f)) {
                LOG(100,
                    ("AVAILABLE (fetched):  fid=<%s> comp=%s priority=%d blocks=%d\n",
                     FID_(&f->fid), f->comp, f->priority, blocks));
                TallyAllHDBentries(f->hdb_bindings, blocks, TSavailable);
                BytesFetched += f->stat.Length;
                HoardWalkProgress(BytesFetched, TotalBytesToFetch);
            } else {
                LOG(100,
                    ("UNAVAILABLE (fetch failed):  fid=<%s> comp=%s priority=%d blocks=%d\n",
                     FID_(&f->fid), f->comp, f->priority, blocks));
                TallyAllHDBentries(f->hdb_bindings, blocks, TSunavailable);
            }
        }
    }

    END_TIMING();
    LOG(100,
        ("hdb::Walk(data): iterations = %d, prefetches = %d, elapsed = %3.1f\n",
         iterations, prefetches, elapsed));
    int indigent_fsobjs = 0;
    int indigent_blocks = 0;
    char ibuf[80 + MAXPATHLEN];
    strcpy(ibuf, "\n");
    if (enospc_failure) {
        /*
	 * Count the number of indigent fsobjs/blocks and find the
         * find first one (for informational purposes only).
	 */
        bstree_iterator next(*FSDB->prioq, BstDescending);
        bsnode *b = 0;
        InitTally();
        while ((b = next())) {
            fsobj *f = strbase(fsobj, b, prio_handle);
            CODA_ASSERT(f != NULL);
            int blocks = (int)BLOCKS(f);

            if (!HOARDABLE(f))
                continue;

            if (DATAVALID(f)) {
                LOG(200,
                    ("AVAILABLE:  fid=<%s> comp=%s priority=%d blocks=%d\n",
                     FID_(&f->fid), f->comp, f->priority, blocks));
                TallyAllHDBentries(f->hdb_bindings, blocks, TSavailable);
                continue;
            }

            LOG(200, ("UNAVAILABLE:  fid=<%s> comp=%s priority=%d blocks=%d\n",
                      FID_(&f->fid), f->comp, f->priority, blocks));
            TallyAllHDBentries(f->hdb_bindings, blocks, TSunavailable);

            if (indigent_fsobjs == 0) {
                char path[MAXPATHLEN];
                Realm *realm;
                f->GetPath(path);
                realm = REALMDB->GetRealm(f->fid.Realm);
                sprintf(ibuf, "\n   ENOSPC:  [%08x@%s, %s], [%d, %d]\n",
                        f->fid.Volume, realm->Name(), path, f->priority,
                        f->HoardVuid);
                realm->PutRef();
            }

            indigent_fsobjs++;
            indigent_blocks += blocks;
        }
        if (indigent_fsobjs == 0)
            eprint("hdb::Walk: enospc_failure but no indigent fsobjs on queue");
    }
    MarinerLog("cache::EndDataWalk [%d]\n   [%d, %d, %1.1f] [%d, %d, %d, %d]%s",
               FSDB->blocks, iterations, prefetches, elapsed / 1000,
               s_prefetches, s_prefetched_blocks, indigent_fsobjs,
               indigent_blocks, ibuf);
}

void hdb::PostWalkStatus()
{
    bstree_iterator prioq_next(*prioq, BstDescending);
    bsnode *b = 0;
    while ((b = prioq_next())) {
        namectxt *n = strbase(namectxt, b, prio_handle);

        switch (n->state) {
        case PeValid:
            LOG(200, ("Valid: uid=%d priority=d\n", n->uid, n->priority));
            break;
        case PeSuspect:
            LOG(200, ("Suspect: uid=%d priority=d\n", n->uid, n->priority));
            Tally(n->priority, n->uid, 0, TSunknown);
            break;
        case PeIndigent:
            LOG(200, ("Indigent: uid=%d priority=d\n", n->uid, n->priority));
            break;
        case PeInconsistent:
            LOG(200,
                ("Inconsistent: uid=%d priority=d\n", n->uid, n->priority));
            Tally(n->priority, n->uid, 0, TSunknown);
            break;
        }
    }

    TallyPrint(hdb::PrimaryUser);

    /* NotifyUsersTaskAvailability(); */

    return;
}

int hdb::Walk(hdb_walk_msg *m, uid_t local_id)
{
    LOG(10, ("hdb::Walk: <%d>\n", local_id));

    int TotalBytesToFetch = 0, BytesFetched = 0;

    /* NotifyUsersOfHoardWalkBegin(); */

    vproc *vp = VprocSelf();

    /* Set the time of the last demand hoard walk */
    if (local_id == V_UID || AuthorizedUser(local_id))
        SetDemandWalkTime();

    /* 1. Start with fso priorities at their correct values. */
    FSDB->RecomputePriorities(1);

    /* 2. Bring the cache into STATUS equilibrium. */
    /*    (i.e., validate/expand hoard entries s.t. priority and resource
     *    constraints) */
    StatusWalk(vp, &TotalBytesToFetch, &BytesFetched);

    /* 2b.  Request advice regarding what to fetch
     *  RequestHoardWalkAdvice(); */

    /* 3. Bring the cache into DATA equilibrium. */
    /*    (i.e., fetch data for hoardable, cached, dataless objects, s.t.
     *    priority and resource constraints) */
    DataWalk(vp, TotalBytesToFetch, BytesFetched);

    /* make sure files are really here. */
    RecovFlush(1);

    /* Determine the post-walk status. */
    PostWalkStatus();

    /* NotifyUsersOfHoardWalkEnd(); */

    return (0);
}

int hdb::Verify(hdb_verify_msg *m, uid_t local_id)
{
    LOG(0, ("hdb::Verify: <%s, %d, %d, %d>\n", m->outfile, m->verbosity,
            m->luid, local_id));

    /* Can only list one's own entries unless root. */
    if (local_id != m->luid && local_id != V_UID && !AuthorizedUser(local_id)) {
        LOG(1, ("hdb::List: (%s, %d, %d) not authorized\n", m->outfile, m->luid,
                local_id));
        return (EACCES);
    }

    /* Open the list file. */
    int outfd =
        ::open(m->outfile, O_TRUNC | O_WRONLY | O_CREAT | O_BINARY, 0600);
    if (outfd < 0) {
        LOG(1, ("hdb::Verify: (%s, %d, %d, %d) open failed (%d)\n", m->outfile,
                m->verbosity, m->luid, local_id, errno));
        return (errno);
    }

    /* set ownership of file to actual owner */
#ifndef __CYGWIN32__
    int err = ::fchown(outfd, local_id, (gid_t)-1);
#else
    int err = ::chown(m->outfile, local_id, (gid_t)-1);

#endif
    if (err) {
        LOG(1, ("hdb::Verify: (%s, %d) fchown failed (%d)\n", m->outfile,
                local_id, errno));
        return (errno);
    }

    /* Print suspicious entries. */
    hdb_iterator next(m->luid);
    hdbent *h;
    while ((h = next())) {
        h->printsuspect(outfd, m->verbosity);
    }
    /* Close the list file. */
    if (::close(outfd) < 0)
        CHOKE("hdb::Verify: close(%s) failed (%d)\n", m->outfile, errno);

    return (0);
}

int hdb::Enable(hdb_walk_msg *m, uid_t local_id)
{
    LOG(10, ("hdb::Enable: <%d>\n", local_id));

    eprint("Enabling periodic hoard walks");
    GetVenusConf().set("nowalk", "0");
    /* NotifyUsersOfHoardWalkPeriodicOn(); */
    return 0;
}

int hdb::Disable(hdb_walk_msg *m, uid_t local_id)
{
    LOG(10, ("hdb::Disable: <%d>\n", local_id));

    eprint("Disabling periodic hoard walks");
    GetVenusConf().set("nowalk", "1");
    /* NotifyUsersOfHoardWalkPeriodicOff(); */
    return 0;
}

/* Demote expansions belonging to this user. */
void hdb::ResetUser(uid_t uid)
{
    hdb_iterator next;
    hdbent *h;
    LOG(100, ("E hdb::ResetUser()\n"));
    while ((h = next())) {
        CODA_ASSERT(h != NULL);
        if (h->uid == uid) {
            CODA_ASSERT(h->nc != NULL);
            h->nc->Demote(1);
        }
    }

    LOG(100, ("L hdb::ResetUser()\n"));
}

void hdb::print(int fd, int SummaryOnly)
{
    fdprint(fd, "HDB:\n");
    fdprint(fd,
            "counts = [%d, %d, %d], namectxts = [%d, %d, %d, %d] [%d, %d]\n",
            MaxHDBEs, htab.count(), freelist.count(), ValidCount, SuspectCount,
            IndigentCount, InconsistentCount, MetaExpansions, MetaNameCtxts);

    if (!SummaryOnly) {
        hdb_iterator next;
        hdbent *h;
        while ((h = next())) {
            h->print(fd);
            /*
	      fdprint(fd, "\tList of namectxts for this hdbent:\n");
	      if (h->nc != NULL)
	        h->nc->print(fd);
	     */
        }
    }

    fdprint(fd, "\n");
}

/*  *****  HDB Entries  *****  */

/* MUST be called from within transaction! */
void *hdbent::operator new(size_t len)
{
    hdbent *h = 0;

    CODA_ASSERT(HDB->htab.count() <
                HDB->MaxHDBEs); /* fix this to be more graceful */

    if (HDB->freelist.count() > 0)
        h = strbase(hdbent, HDB->freelist.get(), tbl_handle);
    else
        h = (hdbent *)rvmlib_rec_malloc((int)len);

    CODA_ASSERT(h);
    return (h);
}

/* MUST be called from within transaction! */
hdbent::hdbent(VolumeId Vid, char *Realm, char *Name, uid_t Vuid, int Priority,
               int Children, int Descendents)
{
    RVMLIB_REC_OBJECT(*this);
    MagicNumber = HDBENT_MagicNumber;
    vid         = Vid;
    {
        int len = (int)strlen(Realm) + 1;
        realm   = (char *)rvmlib_rec_malloc(len);
        rvmlib_set_range(realm, len);
        strcpy(realm, Realm);
    }
    {
        int len = (int)strlen(Name) + 1;
        name    = (char *)rvmlib_rec_malloc(len);
        rvmlib_set_range(name, len);
        strcpy(name, Name);
    }
    uid                = Vuid;
    priority           = Priority;
    expand_children    = Children;
    expand_descendents = Descendents;
    time               = Vtime();
    ResetTransient();

    /* Insert into hash table. */
    hdb_key key(vid, realm, name);
    HDB->htab.append(&key, &tbl_handle);
}

void hdbent::ResetTransient()
{
    /* Sanity checks. */
    if (MagicNumber != HDBENT_MagicNumber) {
        print(GetLogFile());
        CHOKE("hdbent::ResetTransient: bogus MagicNumber");
    }

    Realm *r = REALMDB->GetRealm(realm);
    VenusFid cdir;
    cdir.Realm  = r->Id();
    cdir.Volume = vid;
    FID_MakeRoot(MakeViceFid(&cdir));
    nc = new namectxt(&cdir, name, uid, priority, expand_children,
                      expand_descendents);
    r->PutRef();
}

/* MUST be called from within transaction! */
hdbent::~hdbent()
{
    LOG(10, ("hdbent::~hdbent: (%x@%s, %s)\n", vid, realm, name));

    /* Shut down the name-context. */
    nc->Kill();

    /* Remove from the hash table. */
    hdb_key key(vid, realm, name);
    if (HDB->htab.remove(&key, &tbl_handle) != &tbl_handle) {
        print(GetLogFile());
        CHOKE("hdbent::~hdbent: htab remove");
    }

    /* Release path. */
    rvmlib_rec_free(name);
    rvmlib_rec_free(realm);

    /* Stick on free list or give back to heap. */
    if (HDB->freelist.count() < HDBMaxFreeEntries)
        HDB->freelist.append(&tbl_handle);
    else
        rvmlib_rec_free(this);
}

void hdbent::operator delete(void *deadobj)
{
    /* Nothing to do; storage already relinquished in ~hdbent() */
}

void hdbent::print(int afd)
{
    fdprint(afd, "<%x@%s, %s>, %d, %d%s\n", vid, realm, name, uid, priority,
            (expand_children ? ":c+" : expand_descendents ? ":d+" : ""));
}

void hdbent::printsuspect(int afd, int verbosity)
{
    if (!nc)
        return; /* no expansion present */
    else
        nc->printsuspect(afd, verbosity);
}

/*  *****  HDB Entry Iterator  *****  */

/* Iterator through all entries. */
hdb_iterator::hdb_iterator()
    : rec_ohashtab_iterator(HDB->htab)
{
    uid = ANYUSER_UID;
}

/* Iterate through all entries belonging to a particular user. */
hdb_iterator::hdb_iterator(uid_t Vuid)
    : rec_ohashtab_iterator(HDB->htab)
{
    uid = Vuid;
}

/* Iterator through all entries in a particular hash bucket (used for Find). */
hdb_iterator::hdb_iterator(hdb_key *key)
    : rec_ohashtab_iterator(HDB->htab, key)
{
    uid = ANYUSER_UID;
}

hdbent *hdb_iterator::operator()()
{
    rec_olink *o;
    while ((o = rec_ohashtab_iterator::operator()())) {
        hdbent *h = strbase(hdbent, o, tbl_handle);
        if (uid == ANYUSER_UID || uid == h->uid)
            return (h);
    }

    return (0);
}

/*  *****  Name Contexts  *****  */

/*
 *    Hoarding requires that the cache be in "priority equilibrium" with
 *    respect to explicitly and implicitly defined working-sets.  The explicit
 *    part requires binding of "hoard entries" to fso status blocks, and
 *    prefetching/replacement of fso contents to maintain both "status" and
 *    "data" equilibrium.  The binding of hoard entries is complicated by the
 *    fact that bindings may change over time, due to both local and remote
 *    directory operations.  Further, equilibrium is potentially perturbed as a
 *    result of every object reference (since the implicit component of an
 *    object's priority is reduced as a function of its time since last usage).
 *    Finally, binding maintenance should be computationally efficient, since
 *    the number of hoard entries may be very large.  We address the efficiency
 *    issue by retaining binding state (in VM), and employing an event-driven
 *    philosophy towards hoard entry expansion/validation.  Binding state is
 *    organized into a data structure called a "name-context", one of which is
 *    associated with each hoard entry.  Meta-expansion may cause additional
 *    name-contexts to be associated with a root hoard entry.
 *
 *    Name-contexts are partitioned into four classes:  {Valid, Suspect,
 *    Indigent, Inconsistent}.  Valid contexts are those for which a
 *    "CheckExpansion" is not needed (i.e., a CheckExpansion would not
 *    add/delete any bindings).  Note that Valid does _not_ imply that the
 *    context is completely bound.  Suspect contexts are those for which a
 *    CheckExpansion _might_ add/delete some bindings.  Hence, Suspect contexts
 *    are linked together on a queue and each will be checked in the first pass
 *    of the next hoard walk.  Indigent contexts are a special case of Suspect
 *    which, for efficiency reasons, are managed separately.  An Indigent
 *    context is suspect _solely_ because a trailing component could not be
 *    bound during a previous hoard walk due to low priority.  Rather than
 *    checking each Indigent context at the next walk, we maintain them on a
 *    priority queue and check in decreasing priority order only as long as the
 *    check does not (again) fail due to low priority.  Checking lower priority
 *    contexts would be futile, and the effort of doing so is avoided.
 *
 *    The contexts of each state have an (informal) invariant associated with
 *    them:
 *       Valid:
 *          - the context is fully bound, or
 *          - the opportunity to bind the "first unbound" component will be
 *            signalled by:
 *		- local mutation of the last bound component, or
 *		- remote mutation of the last bound component (signified by
 *		  callback break), or
 *		- AVSG enlargement (with respect to one or more bound
 *		  components), or
 *		- acquisition of new authentication tokens for the
 *		  context-owner
 *		(each of these events induces a transition of the context to
 *		Suspect state)
 *       Suspect:
 *          - the context has yet to be expanded (i.e., it was created after
 *            the last hoard walk), or
 *          - the context was Valid or Indigent at conclusion of the last hoard
 *            walk and a corresponding "suspect-signalling" event has been
 *            registered since then
 *       Indigent:
 *          - the context is not fully bound, and
 *          - full expansion was impeded at conclusion of the last hoard walk
 *            due to low priority, and
 *          - the opportunity to bind the "first unbound" component will be
 *            signalled by:
 *		- successful expansion of the preceding highest priority
 *		  context, or
 *		- demand-fetching of the "first unbound" component
 *		(each of these events induces a transition of the context to
 *		 Suspect state)
 *       Inconsistent:
 *          - the context is not fully bound, and
 *          - full expansion was impeded at conclusion of the last hoard walk
 *            due to inconsistency, and
 *          - the opportunity to bind the "first unbound" component will not be
 *            signalled
 *
 *    Meta-expansion complicates the hoard maintenance task, since the system
 *    must 1) decide when to meta-expand contexts, and 2) when to destroy
 *    meta-expanded contexts that are no longer useful.  Our strategy for the
 *    first requirement is to attempt meta-expansion only when a so-designated
 *    entry transits from Suspect or Indigent state to Valid.  Meta-expansion
 *    creates a name-context for every entry of the directory being expanded.
 *    These new contexts start off in Suspect state, just as a new,
 *    non-meta-expanded context does.  A meta-expanded context which fails
 *    validation is discarded, UNLESS its parent is in the Valid state.  This
 *    exceptional handling avoids continual re-validation of a directory which
 *    is valid, but all or some of whose children are not.  Discarding a
 *    meta-expanded context causes recursive discarding of its own expandees.
 *    The ranking function for name-contexts guarantees that the priority of an
 *    expanded context will be less than its parent.  This fact is needed for
 *    the above-mentioned check in the discard decision as to whether a
 *    context's parent is valid or not.
 *
 */

static const int MaxFreeNameCtxts = 32;
static dlist freenamectxts;

void *namectxt::operator new(size_t len)
{
    namectxt *n = 0;
    dlink *d    = freenamectxts.get();

    if (d == 0) {
        n = (namectxt *)new char[len];
        memset((void *)n, 0, (int)len);
    } else {
        n = strbase(namectxt, d, fl_handle);
    }
    CODA_ASSERT(n);
    return (n);
}

namectxt::namectxt(VenusFid *Cdir, char *Path, uid_t Vuid, int Priority,
                   int Children, int Descendents)
{
    LOG(10, ("namectxt::namectxt: (%s, %s), %d, %d\n", FID_(Cdir), Path, Vuid,
             Priority));

    cdir = *Cdir;
    path = Path;

    uid                = Vuid;
    priority           = Priority;
    state              = PeSuspect;
    inuse              = 0;
    dying              = 0;
    demote_pending     = 0;
    meta_expanded      = 0;
    expand_children    = Children;
    expand_descendents = Descendents;
    depth              = 0;
    random             = ::random();
    next               = 0;

    children = 0;
    if (expand_children || expand_descendents) {
        children     = new dlist;
        expander_fid = NullFid;
        expander_vv  = NullVV;
        expander_dv  = (unsigned long)-1;
    }

    parent = 0;

    /* Mustn't insert into priority queue until "depth" and "random" have been
     * initialized! */
    HDB->prioq->insert(&prio_handle);
    SuspectCount++;

#ifdef VENUSDEBUG
    NameCtxt_allocs++;
#endif
}

/* This constructor is called for meta-expanded contexts! */
/* It should share code with the constructor for non-meta-expanded contexts! */
namectxt::namectxt(namectxt *Parent, char *Component)
{
    LOG(10, ("namectxt::namectxt: (%s, %s/%s), %d, %d\n", FID_(&Parent->cdir),
             Parent->path, Component, Parent->uid, Parent->priority));

    cdir = Parent->cdir;
    path = new char[strlen(Parent->path) + 1 + strlen(Component) + 1];
    strcpy(path, Parent->path);
    strcat(path, "/");
    strcat(path, Component);

    uid                = Parent->uid;
    priority           = Parent->priority;
    state              = PeSuspect;
    inuse              = 0;
    dying              = 0;
    demote_pending     = 0;
    meta_expanded      = 1;
    expand_children    = 0;
    expand_descendents = Parent->expand_descendents;
    depth              = Parent->depth + 1;
    random             = ::random();
    next               = 0;

    children = 0;
    if (expand_descendents) {
        children     = new dlist;
        expander_fid = NullFid;
        expander_vv  = NullVV;
        expander_dv  = (unsigned long)-1;
    }

    parent = Parent;
    /* caller will link our child_link into appropriate list! */

    /* Mustn't insert into priority queue until "depth" and "random" have been
     * initialized! */
    HDB->prioq->insert(&prio_handle);
    SuspectCount++;

    MetaNameCtxts++;
    MetaExpansions++;

#ifdef VENUSDEBUG
    NameCtxt_allocs++;
#endif
}

/*
 * we don't support assignments to objects of this type.
 * bomb in an obvious way if it inadvertently happens.
 */
namectxt::namectxt(namectxt &nc)
{
    abort();
}

int namectxt::operator=(namectxt &nc)
{
    abort();
    return (0);
}

namectxt::~namectxt()
{
#ifdef VENUSDEBUG
    NameCtxt_deallocs++;
#endif

    LOG(10, ("namectxt::~namectxt: (%s, %s), %d, %d\n", FID_(&cdir), path, uid,
             priority));

    /* Context must not be busy! */
    if (inuse) {
        print(GetLogFile());
        CHOKE("namectxt::~namectxt: context inuse");
    }

    /* Path was allocated in ctor (and hence only requires free'ing here) if
     * context was meta_expanded. */
    if (meta_expanded) {
        MetaNameCtxts--;
        delete[] path;
    }

    /* Dequeue from appropriate priority queue. */
    switch (state) {
    case PeValid:
        ValidCount--;
        break;

    case PeSuspect:
    case PeIndigent:
    case PeInconsistent:
        if (HDB->prioq->remove(&prio_handle) != &prio_handle) {
            print(GetLogFile());
            CHOKE("namectxt::~namectxt: prioq remove");
        }
        if (state == PeSuspect)
            SuspectCount--;
        else if (state == PeIndigent)
            IndigentCount--;
        else
            InconsistentCount--;
        break;

    default:
        print(GetLogFile());
        CHOKE("namectxt::~namectxt: bogus state");
    }

    /* Discard expansion. */
    {
        dlink *d = 0;
        while ((d = expansion.first())) {
            binding *b = strbase(binding, d, binder_handle);
            CODA_ASSERT(b != NULL);

            /* Detach the bindee if necessary. */
            fsobj *f = (fsobj *)b->bindee;
            if (f != 0)
                f->DetachHdbBinding(b);

            /* Detach the binder. */
            if (expansion.remove(&b->binder_handle) != &b->binder_handle) {
                print(GetLogFile());
                CHOKE("namectxt::~namectxt: remove failed");
            }

            if (children != NULL) {
                if (children->IsMember(d) == 1) {
                    LOG(0,
                        ("namectxt::~namectxt:  why don't we remove elements off of the children list??\n"));
                }
            }

            b->binder = 0;

            delete b;
        }

        if (next != 0) {
            print(GetLogFile());
            CHOKE("namectxt::~namectxt: next != 0");
        }
    }

    if (children != 0 || parent != 0) {
        print(GetLogFile());
        CHOKE("namectxt::~namectxt: links still active");
    }
}

void namectxt::operator delete(void *deadobj)
{
    namectxt *n;
    n = (namectxt *)deadobj;

    /* Give back to free-list or malloc pool. */
    if (freenamectxts.count() < MaxFreeNameCtxts)
        freenamectxts.append(&n->fl_handle);
    else
        delete[]((char *)deadobj);
}

void namectxt::hold()
{
    if (inuse) {
        print(GetLogFile());
        CHOKE("namectxt::hold: already inuse");
    }

    if (dying) {
        LOG(0, ("namectxt::hold: dead or dying object %p\n", this));
    }

    inuse = 1;
}

void namectxt::release()
{
    if (!inuse) {
        print(GetLogFile());
        CHOKE("namectxt::release: not inuse");
    }

    inuse = 0;

    /* Commit suicide if pending. */
    if (dying) {
        delete this;
        return;
    }

    /* Perform pending transition. */
    if (demote_pending)
        Demote();
}

void namectxt::Transit(enum pestate new_state)
{
    switch (state) {
    case PeValid: {
        if (new_state == PeSuspect) {
            ValidCount--;
            state = PeSuspect;
            HDB->prioq->insert(&prio_handle);
            SuspectCount++;
            return;
        }

        break;
    }

    case PeSuspect: {
        if (new_state == PeSuspect)
            return;

        if (new_state == PeValid || new_state == PeIndigent ||
            new_state == PeInconsistent) {
            SuspectCount--;
            if (new_state == PeValid) {
                if (HDB->prioq->remove(&prio_handle) != &prio_handle) {
                    print(GetLogFile());
                    CHOKE("namectxt::Transit: prioq remove");
                }
                state = PeValid;
                ValidCount++;
            } else if (new_state == PeIndigent) {
                state = PeIndigent;
                IndigentCount++;
            } else {
                state = PeInconsistent;
                InconsistentCount++;
            }
            return;
        }

        break;
    }

    case PeIndigent: {
        if (new_state == PeIndigent)
            return;

        if (new_state == PeValid || new_state == PeSuspect ||
            new_state == PeInconsistent) {
            IndigentCount--;
            if (new_state == PeValid) {
                if (HDB->prioq->remove(&prio_handle) != &prio_handle) {
                    print(GetLogFile());
                    CHOKE("namectxt::Transit: prioq remove");
                }
                state = PeValid;
                ValidCount++;
            } else if (new_state == PeSuspect) {
                state = PeSuspect;
                SuspectCount++;
            } else {
                state = PeInconsistent;
                InconsistentCount++;
            }
            return;
        }

        break;
    }

    case PeInconsistent: {
        if (new_state == PeInconsistent)
            return;

        if (new_state == PeValid || new_state == PeSuspect ||
            new_state == PeIndigent) {
            InconsistentCount--;
            if (new_state == PeValid) {
                if (HDB->prioq->remove(&prio_handle) != &prio_handle) {
                    print(GetLogFile());
                    CHOKE("namectxt::Transit: prioq remove");
                }
                state = PeValid;
                ValidCount++;
            } else if (new_state == PeSuspect) {
                state = PeSuspect;
                SuspectCount++;
            } else {
                state = PeIndigent;
                IndigentCount++;
            }
            return;
        }

        break;
    }

    default:
        print(GetLogFile());
        CHOKE("namectxt::Transit: bogus state");
    }

    print(GetLogFile());
    CHOKE("namectxt::Transit: illegal transition %s --> %s",
          PRINT_PESTATE(state), PRINT_PESTATE(new_state));
}

/* Recursively kills expanded children. */
void namectxt::Kill()
{
    if (dying)
        return;
    dying = 1;

    /* Murder all children (even if this context is still in use). */
    if (expand_children || expand_descendents) {
        KillChildren();

        delete children;
        children = 0;
    }

    /* Discard association between this context and its parent. */
    if (meta_expanded) {
        if (parent == 0) {
            print(GetLogFile());
            CHOKE("namectxt::Kill: parent == 0");
        }

        if (parent->children->remove(&child_link) != &child_link) {
            print(GetLogFile());
            parent->print(GetLogFile());
            CHOKE("namectxt::Kill: parent->children remove");
        }
        parent = 0;
    }

    if (inuse)
        return;

    LOG(10, ("namectxt::Kill(%p)\n", this));
    delete this;
}

void namectxt::KillChildren()
{
    if (children == 0) {
        print(GetLogFile());
        CHOKE("namectxt::KillChildren: children == 0");
    }

    dlink *d;
    while ((d = children->first())) {
        namectxt *child = strbase(namectxt, d, child_link);
        CODA_ASSERT(child != NULL);
        child->Kill();
    }

    expander_fid = NullFid;
    expander_vv  = NullVV;
    expander_dv  = (unsigned long)-1;
}

void namectxt::Demote(int recursive)
{
    if (demote_pending)
        return;
    demote_pending = 1;

    if (recursive) {
        if (expand_children || expand_descendents) {
            if (children == 0) {
                print(GetLogFile());
                CHOKE("namectxt::Demote: children == 0");
            }

            dlist_iterator cnext(*children);
            dlink *d;
            while ((d = cnext())) {
                namectxt *child = strbase(namectxt, d, child_link);
                child->Demote(1);
            }
        }
    }

    /* force an expansion when this namectxt is examined next */
    expander_fid = NullFid;
    expander_vv  = NullVV;
    expander_dv  = (unsigned long)-1;

    if (inuse)
        return;

    Transit(PeSuspect);
    demote_pending = 0;
}

/*
 * CheckExpansion()expands the path of the namectxt.  The call to namev
 * does a component-by-component lookup of the path.  Each of these
 * lookups causes us to call CheckComponent.  Each call to CheckComponent
 * results in...???
 * If the call to namev succeeds, then we attempt to MetaExpand this
 * namectxt.  After meta-expansion, we transition to a new state (if
 * appropriate) and clean-up if we encountered any serious errors.
 */

/* Name-context MUST be held upon entry! */
/* Result is returned in u.u_error. */
pestate namectxt::CheckExpansion()
{
    int iterations;
    vproc *vp = VprocSelf();

#define MAX_CE_ITERATIONS 3
    for (iterations = 0; iterations < MAX_CE_ITERATIONS; iterations++) {
        /* Set up uarea. */
        vp->u.Init();
        vp->u.u_uid      = uid;
        vp->u.u_priority = FSDB->MakePri(0, priority);
        vp->u.u_cdir     = cdir;
        vp->u.u_nc       = this;

        /* Expand/validate the context. */
        if (expansion.count() > 0)
            next = new dlist_iterator(expansion);
        struct venus_cnode vnp;
        if (vp->namev(path, FOLLOW_SYMLINKS, &vnp)) {
        }
        if (next != 0) {
            delete next;
            next = 0;
        }

        /* Meta-expand if appropriate. */
        if (vp->u.u_error == 0 && !dying && !demote_pending)
            MetaExpand();

        /* Abandon a context that was murdered during last namev. */
        if (dying) {
            LOG(0, ("namectxt::CheckExpansion: dying context\n"));
            vp->u.u_error = ESHUTDOWN;
            return (state);
        }

        /* Retry namev for a context that became suspect during the last
	 * attempt. */
        /* This if statement cause a looping bug in HoardWalks.  The namev
	 * succeeds but the following MetaExpand results in a TIMEOUT error and
	 * causes the object to be Demoted.  Because a demote is pending, we
	 * then transition into the PeSuspect state, unset the demote_pending
	 * flag and try again.  This causes us to retry the namev and the
	 * MetaExpand ad infinitum...  I do not understand under what
	 * conditions retrying the namev/MetaExpand will succeed.  However, if
	 * the retry repeatably does not succeed, we will clearly loop
	 * indefinitely.  I've reorganized the loop to prevent the indefinite
	 * loop.                                             mre 6/14/94 */
        if (demote_pending) {
            LOG(10, ("namectxt::CheckExpansion: demote_pending context\n"));
            Transit(PeSuspect);
            demote_pending = 0;
            continue;
        }

        /* If we get here, we want to get out of the loop */
        break;
    }
    if (iterations == MAX_CE_ITERATIONS)
        LOG(0, ("MAX_CE_ITERATIONS reached.  vp->u.u_error = %d\n",
                vp->u.u_error));

    /* Compute the next state. */
    pestate next_state;
    switch (vp->u.u_error) {
    case 0:
        /* Context is fully valid. */
        next_state = PeValid;
        break;

    case ENXIO:
        /* Volume has disappeared. */
        Kill();
        vp->u.u_error = ESHUTDOWN;
        return (state);

    case ENOENT:
    case ENOTDIR: {
        /* Context is partially valid. */

        /* Inconsistent expansions are represented by ENOENT! */
        if (expansion.count() > 0 &&
            (strbase(binding, expansion.last(), binder_handle))->bindee == 0) {
            vp->u.u_error = EINCONS;
            next_state    = PeInconsistent;
            break;
        }

        /* Next opportunity for full validation will be signalled by */
        /* callback break, AVSG expansion, or local name creation/deletion */
        /* (inducing a state change to "suspect" for this namectxt). */
        next_state = PeValid;
    } break;

    case EACCES: {
        /* Context is partially valid. */

        /* Next opportunity for full validation will be signalled by */
        /* ResetUser event, callback break, or AVSG expansion. */
        /* (inducing a state change to "suspect" for this namectxt). */
        LOG(10, ("Received EACCESS during CheckExpansion.\n"));
        next_state = PeValid;
    } break;

    case ETIMEDOUT: {
        /* Context is partially valid.
         * In most cases, the next opportunity for full validation will
         * be signalled (by AVSG expansion).  However, there are some
         * exceptions.
         *   1. expansion.count() == 0
         *   2. the first "unbound component" evaluates to a different
         *      volume
         * These cases are sufficiently obscure that I will ignore them
         * for now, but they should be accounted for eventually. */
        LOG(10, ("Received ETIMEOUT during CheckExpansion.\n"));
        next_state = PeValid;
    } break;

    case ETOOMANYREFS: {
        /* Cannot fully validate context because there is an outstanding
	       * reference. */
        LOG(10, ("Received ETOOMANYREFS during CheckExpansion.\n"));
        next_state = PeValid;
    } break;

    case ENOSPC: {
        /* Context is too "poor" to be fully validated. */

        /* Unlike other partially valid contexts, next opportunity for
         * full validation will NOT be signalled.  Hence, the next hoard
         * walk will poll for that condition.  The polling is done in
         * order of decreasing priority (i.e., from least to most
         * "poor"), and terminates when an indigent context remains
         * indigent after namev. */
        next_state = PeIndigent;
    } break;

    case ELOOP:
    case EINVAL:
    case ENAMETOOLONG:
    case EWOULDBLOCK:
    case EIO: {
        /* Obscure and/or transient errors!  Coerce them to EINCONS. */
        vp->u.u_error = EINCONS;
        next_state    = PeInconsistent;
    } break;

    default:
        next_state = PeInconsistent;
        CHOKE("namectxt::CheckExpansion: bogus return from vproc::namev (%d)",
              vp->u.u_error);
    }

    /* Meta-contract if appropriate. */
    if (vp->u.u_error != 0 && meta_expanded) {
        /* Only nuke children in case of {ENOSPC, EINCONS}. */
        if (vp->u.u_error == ENOSPC || vp->u.u_error == EINCONS) {
            if (expand_children || expand_descendents)
                KillChildren();
        } else {
            /* {ENOENT, ENOTDIR, EACCES, ETIMEDOUT} */
            Kill();
            vp->u.u_error = ESHUTDOWN;
        }
    }

    return (next_state);
}

struct MetaExpandHook {
    namectxt *nc;
    dlist *new_children;
};

/*  This MetaExpand routine takes the MetaExpandHook data structure
 * (which contains a parent namectxt and a new list of children
 * expansions) and a pathname.  (Yes, it completely ignores the vnode
 * and vunique parameters.)
 *
 * It then iterates through the parent namectxt's list of children
 * looking for a match between the final component of the child's path
 * and the pathname sent in as a parameter.  If it finds such a match,
 * it moves the child namectxt off of the parent's list of children
 * and onto what will become the parent's new list of children, and
 * then it returns.  If not such match is found, it creates a new
  * namectxt for this name and inserts it into what will become the
 * parent's new list of children.
 *
 * This routine is called from DH_EnumerateDir (which appears in ../dir/dir.c).
 */
int MetaExpand(PDirEntry entry, void *hook)
{
    char *name = entry->name;

    /* Skip "." and "..". */
    if (STREQ(name, ".") || STREQ(name, ".."))
        return 0;

    struct MetaExpandHook *me_hook = (struct MetaExpandHook *)hook;
    namectxt *parent               = me_hook->nc;
    dlist *new_children            = me_hook->new_children;

    /* Lookup the child namectxt corresponding to this directory entry. */
    /* If found, move the namectxt to the new_children list and return. */
    dlist_iterator next(*(parent->children));
    dlink *d;
    while ((d = next())) {
        namectxt *child = strbase(namectxt, d, child_link);

        char *c = strrchr(child->path, '/');
        if (c == 0)
            c = child->path;
        else
            c++;
        if (STREQ(name, c)) {
            if (parent->children->remove(&child->child_link) !=
                &child->child_link) {
                parent->print(GetLogFile());
                child->print(GetLogFile());
                CHOKE("MetaExpand: children->remove failed");
            }
            new_children->insert(&child->child_link);
            return 0;
        }
    }

    /* Not found.  Create a new namectxt for this directory entry
           and stick it on the new_children list. */
    namectxt *child = new namectxt(parent, name);
    new_children->insert(&child->child_link);

    return 0;
}

/*
 * MetaExpand() controls the meta-expansion of a namectxt.  We only expand
 * an entry when there is good reason to do so.  The real work of meta-expansion
 * is done by dir/dir.c's DH_EnumerateDir() routine, which calls MetaExpand(<with args>)
 * on each child of the directory (see above).
 */

/* This is called only when a fully bound context has just transitted to PeValid! */
void namectxt::MetaExpand()
{
    if (!expand_children && !expand_descendents)
        return;

    LOG(10, ("namectxt::MetaExpand: (%s, %s)\n", FID_(&cdir), path));

    /* ?MARIA?  So what's the order of the expansion list.
       Why is the last element the interesting one?
    */
    dlink *d = expansion.last();
    if (d == 0) {
        print(GetLogFile());
        CHOKE("namectxt::MetaExpand: no bindings");
    }
    binding *b = strbase(binding, d, binder_handle);
    CODA_ASSERT(b != NULL);
    fsobj *f = (fsobj *)b->bindee;
    CODA_ASSERT(f != NULL);

    /* Clean-up non-directories and return. */
    if (!f->IsDir()) {
        KillChildren();
        return;
    }

    /* Discard expandee info if bound object has changed. */
    if (!FID_EQ(&expander_fid, &f->fid))
        KillChildren();

    /* ?MARIA?  Is it possible for the fid's to be different but the VV's to be
     * the same or the data versions be the same?
    */

    /* Meta-expand only if current version of directory differs from last
     * expanded version. */
    if ((f->vol->IsReplicated() &&
         VV_Cmp(&expander_vv, &f->stat.VV) != VV_EQ) ||
        (!f->vol->IsReplicated() && expander_dv != f->stat.DataVersion)) {
        /*
	 ?MARIA?:  Why doesn't the KillChildren above appear here???
	 Hmm.. Perhaps the difference is that if the FID has changed, we can't
	 believe the old children list.  If the fid hasn't changed but the vv
	 has (or the data version has), we have some reason to suspect that the
	 new children list will be almost identical to the old one so we can
	 move the old children onto a new list and save some work???
       */

        LOG(1, ("namectxt::MetaExpand: enumerating (%s)\n", FID_(&f->fid)));

        /* Iterate through current contents of directory.
	 * The idea is to make a new children list of namectxts corresponding
	 * to directory entries. The elements of the new list are either moved
	 * from the current list (if found), or created fresh (if not found).
	 */
        VenusFid tfid = f->fid;
        {
            /* Must have valid data for directory! */
            /* XXX There should be a generic "prefetch" routine! */
            vproc *vp        = VprocSelf();
            vp->u.u_priority = f->priority;
            for (;;) {
                vp->Begin_VFS(&tfid, CODA_VGET);
                if (vp->u.u_error)
                    break;

                fsobj *tf     = 0;
                vp->u.u_error = FSDB->Get(&tf, &tfid, vp->u.u_uid, RC_DATA);
                FSDB->Put(&tf);
                int retry_call = 0;
                vp->End_VFS(&retry_call);
                if (!retry_call)
                    break;
            }
            if (vp->u.u_error == EINCONS)
                k_Purge(&tfid, 1);

            if (vp->u.u_error != 0) {
                /* Following statement commented out so that caller can handle
		* errors. */
                // vp->u.u_error = 0;
                Demote();
                return;
            }
        }
        /* Reacquire reference to object. */
        f = FSDB->Find(&tfid);
        if (f == 0)
            CHOKE(
                "hdb.c: Reacquire reference to object failed!"
                "  Don't know what to do so we die in an obvious way..."
                "  Sorry for the inconvenience.\n");

        dlist *new_children = new dlist;
        struct MetaExpandHook hook;
        hook.nc           = this;
        hook.new_children = new_children;
        DH_EnumerateDir(&f->data.dir->dh, ::MetaExpand, (void *)(&hook));

        /* Kill anything left on the current list, and discard it. */
        /* Install new_children as the current children list. */
        KillChildren();
        delete children;
        children = new_children;

        /* Remember version state so that we can avoid this process unless
         * absolutely necessary. */
        expander_fid = f->fid;
        expander_vv  = f->stat.VV;
        expander_dv  = f->stat.DataVersion;
    }
}

/* This is called within vproc::vget or vproc::lookup upon a successful operation. */
void namectxt::CheckComponent(fsobj *f)
{
    LOG(100, ("namectxt::CheckComponent: (%s, %s), %s, f = %x\n", FID_(&cdir),
              path, PRINT_PESTATE(state), f));

    if (state != PeSuspect && state != PeIndigent && state != PeInconsistent) {
        print(GetLogFile());
        CHOKE("namectxt::CheckComponent: bogus state");
    }

    /*
     * Note that next was setup before CheckExpansion called namev, which called
     * lookup, which called us.  Next is an iterator over the expansion list of
     * this namectxt.
     */
    dlink *d   = (next ? (*next)() : 0);
    binding *b = (d ? strbase(binding, d, binder_handle) : 0);

    /* See whether existing binding matches this object. */
    if (b != 0) {
        if ((fsobj *)b->bindee == f)
            return;

        /* Mismatch!  Discard expansion suffix. */
        binding *old_b;
        do {
            old_b = strbase(binding, expansion.last(), binder_handle);

            /* Detach the bindee if necessary. */
            fsobj *f = (fsobj *)old_b->bindee;
            if (f != 0)
                f->DetachHdbBinding(old_b);

            /* Detach the binder. */
            if (expansion.remove(&old_b->binder_handle) !=
                &old_b->binder_handle) {
                print(GetLogFile());
                CHOKE("namectxt::CheckComponent: remove failed");
            }
            old_b->binder = 0;

            delete old_b;
        } while (old_b != b);

        delete next;
        next = 0;
    }

    /* We've discovered a new terminal component. */
    if (f) {
        /* Attach new binding. */
        b = f->AttachHdbBinding(this);

        /* did we get bound successfully? */
        if (b)
            expansion.append(&b->binder_handle);
    }
}

void namectxt::print(int fd, void *filter)
{
    fdprint(fd, "\tcdir = (%s), path = %s, uid = %d, priority = %d (%d, %d)\n",
            FID_(&cdir), path, uid, priority, depth, random);
    fdprint(fd, "\tstate = %s, flags = [%d %d %d %d %d %d]\n",
            PRINT_PESTATE(state), inuse, dying, demote_pending, meta_expanded,
            expand_children, expand_descendents);
    fdprint(fd, "\tnext = %x, parent = %x, children = %x\n", next, parent,
            children);

    // MARIA:  Delete this stuff???
    fdprint(fd, "\tcount of expansion list = %d\n", expansion.count());
    if (expansion.count() > 0) {
        dlist_iterator enext(expansion);
        dlink *e;
        while ((e = enext())) {
            binding *b = strbase(binding, e, binder_handle);
            if (filter && b->binder && b->binder != filter && b->bindee &&
                b->bindee != filter)
                continue;

            fdprint(fd, "\t\t");
            b->print(fd);
            if (b->binder && b->binder != this) {
                fdprint(fd, "\t\tunexpected binder\n");
                if (((namectxt *)(b->binder))->path)
                    fdprint(fd, "\t\tnamectxt.path = %s\n",
                            ((namectxt *)(b->binder))->path);
            }
            if (b->bindee)
                fdprint(fd, "\t\tfsobj.comp = %s\n",
                        ((fsobj *)(b->bindee))->GetComp());
        }
    }
}

void namectxt::getpath(char *buf)
{
    volent *v = VDB->Find(MakeVolid(&cdir));
    if (v)
        v->GetMountPath(buf, 0);
    if (!v || STREQ(buf, "???")) {
        Realm *realm = REALMDB->GetRealm(cdir.Realm);
        sprintf(buf, "%08x@%s", cdir.Volume, realm->Name());
        realm->PutRef();
    }

    strcat(buf, "/");
    if (path[0] == '.')
        strcat(buf, &path[2]); /* strip leading "./" */
    else
        strcat(buf, path);
    if (v)
        v->release();
    return;
}

void namectxt::putmsg(int fd, const char *reason, int include_modifier)
{
    char fullpath[MAXPATHLEN];
    const char *modifier =
        !include_modifier ?
            "" :
            (expand_descendents ? " d+" : (expand_children ? " c+" : "   "));

    getpath(fullpath);

    fdprint(fd, "%25s  %s%s\n", reason, fullpath, modifier);
}

void namectxt::printsuspect(int fd, int verbosity)
{
    /* verbosity = 0		silence except for errors
       0 < verbosity <= 100	errors and confirmation of items in hoard database
       100 < verbosity		errors and confirmation of all expanded items
    */

    /* Deal with this node first */
    dlink *d = expansion.last();
    if (d == 0) {
        putmsg(fd, "*** Not bound ***", 1);
        return;
    } else {
        binding *b = strbase(binding, d, binder_handle);
        fsobj *f   = (fsobj *)b->bindee;

        if (!f || !STATUSVALID(f) || !DATAVALID(f)) {
            putmsg(fd, "*** Missing/Invalid ***", 1);
            return;
        }
    }

    if (verbosity)
        putmsg(fd, "OK", 0);

    /* Then recursively deal with children */
    if (children) {
        dlist_iterator cnext(*children);
        dlink *d;
        while ((d = cnext())) {
            namectxt *child = strbase(namectxt, d, child_link);
            child->printsuspect(fd, ((verbosity > 100) ? verbosity : 0));
        }
    }
}

int NC_PriorityFN(bsnode *b1, bsnode *b2)
{
    namectxt *n1 = strbase(namectxt, b1, prio_handle);
    namectxt *n2 = strbase(namectxt, b2, prio_handle);
    /*
    if ((char *)n1 == (char *)n2)
	{ n1->print(GetLogFile()); CHOKE("NC_PriorityFN: n1 == n2\n"); }
    */

    /* First determinant is explicit priority. */
    if (n1->priority > n2->priority)
        return (1);
    if (n1->priority < n2->priority)
        return (-1);

    /* Second is meta-expansion depth. */
    if (n1->depth < n2->depth)
        return (1);
    if (n1->depth > n2->depth)
        return (-1);

    /* Third is "random" bits assigned at creation. */
    if (n1->random > n2->random)
        return (1);
    if (n1->random < n2->random)
        return (-1);

    /* The chance of this ever happening should be miniscule! -JJK */
    /*
    eprint("NC_PriorityFN: priorities tied (%d, %d, %d)!\n",
	    n1->priority, n1->depth, n1->random);
    */
    LOG(1, ("NC_PriorityFN: priorities tied (%d, %d, %d)!\n", n1->priority,
            n1->depth, n1->random));
    return (0);
}

hdb_key::hdb_key(VolumeId Vid, char *Realm, char *Name)
{
    vid   = Vid;
    realm = Realm;
    name  = Name;
}
