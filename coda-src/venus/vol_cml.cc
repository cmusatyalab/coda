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
 *    Implementation of Venus' Client Modify Log.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <fcntl.h>
#include "coda_string.h"
#include <sys/types.h>
#include <stdarg.h>
#include <struct.h>

#include <unistd.h>
#include <stdlib.h>

#include <netinet/in.h>

#include <lwp/lock.h>
#include <rpc2/rpc2.h>
#include <rpc2/se.h>
#include <rpc2/multi.h>
#include <rpc2/pack_helper.h>

/* from dir */
#include <codadir.h>

/* interfaces */
#include <vice.h>
#include <cml.h>

#include "archive.h"

#ifdef __cplusplus
}
#endif

/* from util */
#include <dlist.h>

/* from venus */
#include "comm.h"
#include "fso.h"
#include "local.h"
#include "mariner.h"
#include "mgrp.h"
#include "venus.private.h"
#include "venuscb.h"
#include "venusvol.h"
#include "vproc.h"
#include "worker.h"

/*  *****  Client Modify Log Basic Routines  *****  */

void ClientModifyLog::ResetTransient()
{
    owner               = UNSET_UID;
    entries             = count();
    entriesHighWater    = entries;
    bytes               = _bytes();
    bytesHighWater      = bytes;
    cancelFrozenEntries = 0;

    if (count() > 0) {
        /* Schedule a transition (to Reintegrating or Emulating) if log is non-empty. */
        strbase(repvol, this, CML)->flags.transition_pending = 1;

        /* Set owner. */
        cml_iterator next(*this, CommitOrder);
        cmlent *m;
        while ((m = next())) {
            if (owner == UNSET_UID) {
                owner = m->uid;
            } else {
                CODA_ASSERT(owner == m->uid);
            }

            m->ResetTransient();
        }
        CODA_ASSERT(owner != UNSET_UID);
    }
}

/* MUST be called from within transaction! */
void ClientModifyLog::Clear()
{
    rec_dlink *d;

    while ((d = list.first()))
        delete strbase(cmlent, d, handle);
}

long ClientModifyLog::_bytes()
{
    cml_iterator next(*this);
    cmlent *e;
    long result = 0;

    while ((e = next()))
        result += e->bytes();

    return result;
}

void ClientModifyLog::IncGetStats(cmlstats &current, cmlstats &cancelled,
                                  int tid)
{
    /* First, compute current statistics. */
    cml_iterator next(*this, CommitOrder);
    cmlent *m;

    current.clear();
    cancelled.clear();

    while ((m = next())) {
        if (tid != UNSET_TID && m->GetTid() != tid)
            continue;
        if (m->opcode == CML_Store_OP) {
            current.store_count++;
            current.store_size += m->bytes();
            current.store_contents_size += (float)m->u.u_store.Length;
        } else {
            current.other_count++;
            current.other_size += m->bytes();
        }
    }

    cancelled = cancellations;
}

/*
 * called after a reintegration failure to remove cmlents that would
 * have been cancelled had reintegration not been in progress.
 * Unfreezes records; cancel requires this.  Since this routine is
 * called only if the failure involved receiving a response from the
 * server (i.e., outcome is known), it is safe to unfreeze the records.
 */
void ClientModifyLog::CancelPending()
{
    Recov_BeginTrans();
    int cancellation;
    do {
        cancellation = 0;
        cml_iterator next(*this);
        cmlent *m;

        while ((m = next())) {
            if (m->flags.cancellation_pending) {
                m->Thaw();

                CODA_ASSERT(m->cancel());
                cancellation = 1;
                break;
            }
        }

    } while (cancellation);
    Recov_EndTrans(MAXFP);
}

/*
 * called after reintegration success to clear cancellations
 * pending failure.  this is necessary because records in
 * the log tail (not involved in reintegration) may be marked.
 */
void ClientModifyLog::ClearPending()
{
    cml_iterator next(*this);
    cmlent *m;

    while ((m = next()))
        if (m->flags.cancellation_pending) {
            Recov_BeginTrans();
            RVMLIB_REC_OBJECT(m->flags);
            m->flags.cancellation_pending = 0;
            Recov_EndTrans(MAXFP);
        }
}

/*
 * Scans the log, cancelling stores for open-for-write files.
 * Note it might delete a record out from under itself.
 */
void ClientModifyLog::CancelStores()
{
    cmlent *m, *n;
    cml_iterator next(*this, CommitOrder);

    m = next();
    n = next();
    while (m) {
        m->cancelstore();
        m = n;
        n = next();
    }
}

/* MUST be called from within a transaction */
int cmlent::Freeze()
{
    int err;

    /* already frozen, nothing to do */
    if (flags.frozen)
        return 0;

    if (opcode == CML_Store_OP) {
        /* make sure there is only one object of the store */
        CODA_ASSERT(fid_bindings->count() == 1);

        dlink *d   = fid_bindings->first(); /* and only */
        binding *b = strbase(binding, d, binder_handle);
        fsobj *f   = (fsobj *)b->bindee;

        /* sanity checks, this better be an fso */
        CODA_ASSERT(f && (f->MagicNumber == FSO_MagicNumber));

        err = f->MakeShadow();
        if (err)
            return err;
    }

    RVMLIB_REC_OBJECT(flags);
    flags.frozen = 1;

    return 0;
}

/* MUST be called from within a transaction */
void cmlent::Thaw()
{
    if (!IsFrozen())
        return;

    if (opcode == CML_Store_OP) {
        /* make sure there is only one object of the store */
        CODA_ASSERT(fid_bindings->count() == 1);

        dlink *d   = fid_bindings->first(); /* and only */
        binding *b = strbase(binding, d, binder_handle);
        fsobj *f   = (fsobj *)b->bindee;

        /* sanity checks, this better be an fso */
        CODA_ASSERT(f && (f->MagicNumber == FSO_MagicNumber));

        /* no need to unlock, just get rid of shadow copy */
        if (f->shadow)
            f->RemoveShadow();
    }

    RVMLIB_REC_OBJECT(flags);
    flags.frozen = 0;
}

/*
 * Scan the log for reintegrateable records, subject to the
 * reintegration time limit, and mark them with the given
 * tid. Note the time limit does not apply to ASRs.
 * The routine returns the number of records marked.
 */
int ClientModifyLog::GetReintegrateable(int tid, unsigned long *reint_time,
                                        int *nrecs)
{
    static bool allow_backfetch =
        GetVenusConf().get_bool_value("allow_backfetch");
    reintvol *vol = strbase(reintvol, this, CML);
    cmlent *m;
    cml_iterator next(*this, CommitOrder);
    unsigned long this_time;
    unsigned long bw; /* bandwidth in bytes/sec */
    int err;
    int done = 1;

    *nrecs = 0;

    /* get the current bandwidth estimate */
    vol->GetBandwidth(&bw);

    while ((m = next())) {
        /* do not pack stores if we want to avoid backfetches */
        /* this has to be matched by a similar (but inverse) test in
	 * PartialReintegrate, otherwise we would never be able to
	 * reintegrate the Store operation */
        if (!allow_backfetch && m->opcode == CML_Store_OP) {
            done = 0;
            break;
        }

        if (m->ReintReady() != 0)
            break;

        this_time = m->ReintTime(bw);

        /* Only limit reintegration time when we are not forcing a
	 * synchronous reintegration and and we have at least
	 * one CML entry queued. --JH */
        if (!vol->IsSync() && *nrecs && this_time > *reint_time)
            break;

        /*
	 * freeze the record to prevent cancellation.  Note that
	 * reintegrating --> frozen, but the converse is not true.
	 * Records are frozen until the outcome of a reintegration
	 * is known; this may span multiple reintegration attempts
	 * and different transactions.
	 */
        Recov_BeginTrans();
        err = m->Freeze();
        Recov_EndTrans(MAXFP);
        if (err)
            break;

        /*
	 * don't use the settid call because it is transactional.
	 * Here the tid is transient.
	 */
        m->tid = tid;
        *reint_time -= this_time;

        /*
	 * By sending records in blocks of 100 CMLentries, we avoid
	 * overloading the server. JH
	 */
        if (++(*nrecs) == 100) {
            done = 0;
            break;
        }
    }

    LOG(0,
        ("ClientModifyLog::GetReintegrateable: (%s, %d) %d records, %d msec remaining\n",
         vol->name, tid, *nrecs, *reint_time));
    return done;
}

/*
 * check if there is a fat store blocking the head of the log.
 * if there is, mark it with the tid and return a pointer to it.
 * Note with a less pretty interface this could be rolled into
 * the routine above.
 */
cmlent *ClientModifyLog::GetFatHead(int tid)
{
    static bool allow_backfetch =
        GetVenusConf().get_bool_value("allow_backfetch");
    reintvol *vol = strbase(reintvol, this, CML);
    cmlent *m;
    cml_iterator next(*this, CommitOrder);
    unsigned long bw; /* bandwidth in bytes/sec */

    /* Get the first entry in the CML */
    m = next();

    /* The head of the CML must exists, and be a store operation */
    if (!m || m->opcode != CML_Store_OP)
        return NULL;

    /* If we already have a reintegration handle, or if the reintegration time
     * exceeds the limit, we need to do a partial reintegration of the store.
     * We always partially reintegrate if we want to avoid backfetches. */
    if (allow_backfetch && !m->HaveReintegrationHandle()) {
        /* get the current bandwidth estimate */
        vol->GetBandwidth(&bw);
        if (m->ReintTime(bw) <= vol->ReintLimit)
            return NULL;
    }

    /*
     * Don't use the settid call because it is transactional.
     * Here the tid is transient.
     */
    m->tid = tid;

    /*
     * freeze the record to prevent cancellation.  Note that
     * reintegrating --> frozen, but the converse is not true.
     * Records are frozen until the outcome of a reintegration
     * is known; this may span multiple reintegration attempts
     * and different transactions.
     */
    Recov_BeginTrans();
    CODA_ASSERT(m->Freeze() == 0);
    Recov_EndTrans(MAXFP);

    return m;
}

/*
 * Mark the offending mle designated by the index.
 * If the index is -1, something really bad happened.
 * in that case mark 'em all.
 */
void ClientModifyLog::MarkFailedMLE(int ix)
{
    repvol *vol = strbase(repvol, this, CML);
    int i       = 0;

    cml_iterator next(*this);
    cmlent *m;
    while ((m = next()))
        if (m->tid == vol->cur_reint_tid)
            if (i++ == ix || ix == -1) {
                char path[MAXPATHLEN], opmsg[1024];
                fsobj *conflict;

                m->GetLocalOpMsg(opmsg);

                LOG(0,
                    ("ClientModifyLog::MarkFailedMLE: failed reintegrating: %s\n",
                     opmsg));

                CODA_ASSERT((conflict = FSDB->Find(&m->u.u_repair.Fid)));
                conflict->GetPath(path, 1);
                //	    k_Purge(&conflict->pfid, 1);
                k_Purge(&m->u.u_repair.Fid, 0);

                m->flags.failed = 1;

                LOG(0,
                    ("ClientModifyLog::MarkFailedMLE: %s(%s) now in local/global conflict\n",
                     path, FID_(&m->u.u_repair.Fid)));
                MarinerLog(
                    "ClientModifyLog::CONFLICT (local/global): %s (%s)\n", path,
                    FID_(&m->u.u_repair.Fid));
            }
}

/*
 * Mark the record with the matching storeid-uniquifier
 * as already having been committed at the server.
 */
void ClientModifyLog::MarkCommittedMLE(RPC2_Unsigned Uniquifier)
{
    repvol *vol = strbase(repvol, this, CML);

    cml_iterator next(*this);
    cmlent *m;
    while ((m = next()))
        if (m->tid == vol->cur_reint_tid && m->sid.Uniquifier == Uniquifier)
            m->flags.committed = 1;
}

/*
  failedmle - the entry point of handling reintegration failure or
  local-global conflicts
*/
/*
 * Handle a non-retryable failure.  The offending record
 * was marked and may or may not still be there.
 * Note that an abort may delete a record out from under us.
 */
void ClientModifyLog::HandleFailedMLE(void)
{
    cmlent *m, *n;
    cml_iterator next(*this, CommitOrder);

    n = next();
    while ((m = n) != NULL) {
        n = next();

        if (!m->flags.failed)
            continue;

        m->flags.failed = 0; /* only do this once */
        m->SetRepairFlag();
    }
}

void ClientModifyLog::print(int fd)
{
    fdprint(fd, "\tClientModifyLog: owner = %d, count = %d\n", owner, count());

    cmlstats current, cancelled;
    IncGetStats(current, cancelled);
    fdprint(fd, "\t  current stats: %4d  %10.1f  %10.1f  %4d  %10.1f\n",
            current.store_count, current.store_size / 1024.0,
            current.store_contents_size / 1024.0, current.other_count,
            current.other_size / 1024.0);
    fdprint(fd, "\tcancelled stats: %4d  %10.1f  %10.1f  %4d  %10.1f\n",
            cancelled.store_count, cancelled.store_size / 1024.0,
            cancelled.store_contents_size / 1024.0, cancelled.other_count,
            cancelled.other_size / 1024.0);

    cml_iterator next(*this, CommitOrder);
    cmlent *m;
    while ((m = next()))
        if (m->tid != -1)
            m->print(fd);
}

/* MUST be called from within transaction! */
void *cmlent::operator new(size_t len)
{
    cmlent *c = 0;

    LOG(1, ("cmlent::operator new()\n"));

    CODA_ASSERT(VDB->AllocatedMLEs < VDB->MaxMLEs);

    /* Get entry from free list or heap */
    if (VDB->mlefreelist.count() > 0)
        c = strbase(cmlent, VDB->mlefreelist.get(), handle);
    else
        c = (cmlent *)rvmlib_rec_malloc((int)len);
    CODA_ASSERT(c);

    /* Do bookkeeping */
    RVMLIB_REC_OBJECT(VDB->AllocatedMLEs);
    VDB->AllocatedMLEs++;

    return (c);
}

/* MUST be called from within transaction! */
cmlent::cmlent(ClientModifyLog *Log, time_t Mtime, uid_t Uid, int op,
               int prepend...)
{
    LOG(1, ("cmlent::cmlent(...)\n"));
    RVMLIB_REC_OBJECT(*this);
    RPC2_String name, newname;

    log                        = Log;
    this->tid                  = -1;
    flags.to_be_repaired       = 0;
    flags.frozen               = 0;
    flags.cancellation_pending = 0;
    flags.prepended            = prepend;
    if (prepend)
        log->list.prepend(&handle);
    else
        log->list.append(&handle);

    Recov_GenerateStoreId(&sid);
    time = Mtime;
    uid  = Uid;

    opcode = op;
    name = newname = NULL;
    Name = NewName = NULL;
    va_list ap;
    va_start(ap, prepend);
    switch (op) {
    case CML_Store_OP:
        u.u_store.Fid    = *va_arg(ap, VenusFid *);
        u.u_store.Length = va_arg(ap, RPC2_Unsigned);
        memset(&u.u_store.RHandle, 0, sizeof(ViceReintHandle));
        u.u_store.Offset         = 0;
        u.u_store.ReintPH.s_addr = 0;
        u.u_store.ReintPHix      = -1;
        break;

    case CML_Utimes_OP:
        u.u_utimes.Fid  = *va_arg(ap, VenusFid *);
        u.u_utimes.Date = va_arg(ap, Date_t);
        break;

    case CML_Chown_OP:
        u.u_chown.Fid   = *va_arg(ap, VenusFid *);
        u.u_chown.Owner = va_arg(ap, UserId);
        break;

    case CML_Chmod_OP:
        u.u_chmod.Fid  = *va_arg(ap, VenusFid *);
        u.u_chmod.Mode = va_arg(ap, RPC2_Unsigned);
        break;

    case CML_Create_OP:
        u.u_create.PFid = *va_arg(ap, VenusFid *);
        name            = va_arg(ap, RPC2_String);
        u.u_create.CFid = *va_arg(ap, VenusFid *);
        u.u_create.Mode = va_arg(ap, RPC2_Unsigned);
        Name            = Copy_RPC2_String(name);
        break;

    case CML_Remove_OP:
        u.u_remove.PFid      = *va_arg(ap, VenusFid *);
        name                 = va_arg(ap, RPC2_String);
        u.u_remove.CFid      = *va_arg(ap, VenusFid *);
        u.u_remove.LinkCount = va_arg(ap, int);
        Name                 = Copy_RPC2_String(name);
        break;

    case CML_Link_OP:
        u.u_link.PFid = *va_arg(ap, VenusFid *);
        name          = va_arg(ap, RPC2_String);
        u.u_link.CFid = *va_arg(ap, VenusFid *);
        Name          = Copy_RPC2_String(name);
        break;

    case CML_Rename_OP:
        u.u_rename.SPFid = *va_arg(ap, VenusFid *);
        name             = va_arg(ap, RPC2_String);
        u.u_rename.TPFid = *va_arg(ap, VenusFid *);
        newname          = va_arg(ap, RPC2_String);
        u.u_rename.SFid  = *va_arg(ap, VenusFid *);
        Name             = Copy_RPC2_String(name);
        NewName          = Copy_RPC2_String(newname);
        break;

    case CML_MakeDir_OP:
        u.u_mkdir.PFid = *va_arg(ap, VenusFid *);
        name           = va_arg(ap, RPC2_String);
        u.u_mkdir.CFid = *va_arg(ap, VenusFid *);
        u.u_mkdir.Mode = va_arg(ap, RPC2_Unsigned);
        Name           = Copy_RPC2_String(name);
        break;

    case CML_RemoveDir_OP:
        u.u_rmdir.PFid = *va_arg(ap, VenusFid *);
        name           = va_arg(ap, RPC2_String);
        u.u_rmdir.CFid = *va_arg(ap, VenusFid *);
        Name           = Copy_RPC2_String(name);
        break;

    case CML_SymLink_OP:
        u.u_symlink.PFid = *va_arg(ap, VenusFid *);
        newname          = va_arg(ap, RPC2_String);
        name             = va_arg(ap, RPC2_String);
        u.u_symlink.CFid = *va_arg(ap, VenusFid *);
        u.u_symlink.Mode = va_arg(ap, RPC2_Unsigned);
        NewName          = Copy_RPC2_String(newname);
        Name             = Copy_RPC2_String(name); // content
        break;

    case CML_Repair_OP:
        u.u_repair.Fid    = *va_arg(ap, VenusFid *);
        u.u_repair.Length = va_arg(ap, RPC2_Unsigned);
        u.u_repair.Date   = va_arg(ap, Date_t);
        u.u_repair.Owner  = va_arg(ap, UserId);
        u.u_repair.Mode   = va_arg(ap, RPC2_Unsigned);
        break;

    default:
        print(GetLogFile());
        CHOKE("cmlent::cmlent: bogus opcode (%d)", op);
    }
    va_end(ap);

    ResetTransient();

    /* Attach to fsobj's.  */
    AttachFidBindings();

    /* Update statistics for this CML */
    log->entries++;
    if (log->entries > log->entriesHighWater)
        log->entriesHighWater = log->entries;
    log->bytes += bytes();
    if (log->bytes > log->bytesHighWater)
        log->bytesHighWater = log->bytes;

    LOG(1, ("cmlent::cmlent: tid = (%x.%d), uid = %d, op = %s\n", sid.HostId,
            sid.Uniquifier, uid, PRINT_MLETYPE(op)));
}

void cmlent::ResetTransient()
{
    fid_bindings = 0;

    pred = 0;
    succ = 0;

    flags.failed    = 0;
    flags.committed = 0;

    expansions = 0;

    switch (opcode) {
    case CML_Store_OP:
        u.u_store.VV = NullVV;
        break;

    case CML_Truncate_OP:
        u.u_truncate.VV = NullVV;
        break;

    case CML_Utimes_OP:
        u.u_utimes.VV = NullVV;
        break;

    case CML_Chown_OP:
        u.u_chown.VV = NullVV;
        break;

    case CML_Chmod_OP:
        u.u_chmod.VV = NullVV;
        break;

    case CML_Create_OP:
        u.u_create.PVV = NullVV;
        break;

    case CML_Remove_OP:
        u.u_remove.PVV = NullVV;
        u.u_remove.CVV = NullVV;
        break;

    case CML_Link_OP:
        u.u_link.PVV = NullVV;
        u.u_link.CVV = NullVV;
        break;

    case CML_Rename_OP:
        u.u_rename.SPVV = NullVV;
        u.u_rename.TPVV = NullVV;
        u.u_rename.SVV  = NullVV;
        break;

    case CML_MakeDir_OP:
        u.u_mkdir.PVV = NullVV;
        break;

    case CML_RemoveDir_OP:
        u.u_rmdir.PVV = NullVV;
        u.u_rmdir.CVV = NullVV;
        break;

    case CML_SymLink_OP:
        u.u_symlink.PVV = NullVV;
        break;

    case CML_Repair_OP:
        u.u_repair.OVV = NullVV;
        break;

    default:
        print(GetLogFile());
        CHOKE("cmlent::ResetTransient: bogus opcode (%d)", opcode);
    }
}

/* MUST be called from within transaction! */
cmlent::~cmlent()
{
    LOG(1, ("cmlent::~cmlent: tid = (%x.%d), uid = %d, op = %s\n", sid.HostId,
            sid.Uniquifier, uid, PRINT_MLETYPE(opcode)));

    RVMLIB_REC_OBJECT(*this);
    long thisBytes = bytes();

    /* or should we assert on this cmlent not being frozen? -JH */
    Thaw();

    /* Detach from fsobj's. */
    DetachFidBindings();

    /* Free strings. */
    if (Name)
        Free_RPC2_String(Name);
    if (NewName)
        Free_RPC2_String(NewName);
    Name = NewName = NULL;

    CODA_ASSERT(log->list.remove(&handle) == &handle);
    /* update CML statistics */
    log->entries--;
    log->bytes -= thisBytes;

    log = 0;
}

/* MUST be called from within transaction! */
void cmlent::operator delete(void *deadobj)
{
    cmlent *c = (cmlent *)deadobj;

    LOG(1, ("cmlent::operator delete()\n"));

    /* Stick on free list or give back to heap. */
    if (VDB->mlefreelist.count() < MLENTMaxFreeEntries)
        VDB->mlefreelist.append((rec_dlink *)&c->handle);
    else
        rvmlib_rec_free(deadobj);

    RVMLIB_REC_OBJECT(VDB->AllocatedMLEs);
    VDB->AllocatedMLEs--;
}

long cmlent::bytes()
{
    long result = sizeof(*this);

    if (Name)
        result += strlen((char *)Name);
    if (NewName)
        result += strlen((char *)NewName);

    return result;
}

#define PRINTVV(fd, vv)                                                    \
    fdprint((fd), "[ %d %d %d %d %d %d %d %d ] [ %d %d ] [ %#x ]",         \
            (vv).Versions.Site0, (vv).Versions.Site1, (vv).Versions.Site2, \
            (vv).Versions.Site3, (vv).Versions.Site4, (vv).Versions.Site5, \
            (vv).Versions.Site6, (vv).Versions.Site7, (vv).StoreId.HostId, \
            (vv).StoreId.Uniquifier, (vv).Flags);

/* local-repair modification */
void cmlent::print(int afd)
{
    fdprint(afd,
            "\t%s : sid = (%x.%d), time = %d, uid = %d tid = %d bytes = %d\n",
            PRINT_MLETYPE(opcode), sid.HostId, sid.Uniquifier, time, uid, tid,
            bytes());
    fdprint(afd, "\t\tpred = (%x, %d), succ = (%x, %d)\n", pred,
            (pred == 0 ? 0 : pred->count()), succ,
            (succ == 0 ? 0 : succ->count()));
    fdprint(afd, "\t\tto_be_repaired = %d\n", flags.to_be_repaired);
    fdprint(afd, "\t\tfrozen = %d, cancel = %d, failed = %d, committed = %d\n",
            flags.frozen, flags.cancellation_pending, flags.failed,
            flags.committed);
    switch (opcode) {
    case CML_Store_OP:
        fdprint(afd, "\t\tfid = %s, length = %d\n", FID_(&u.u_store.Fid),
                u.u_store.Length);
        fdprint(afd, "\t\tvv = ");
        PRINTVV(afd, u.u_store.VV);
        fdprint(afd, "\n\t\trhandle = (%d,%d,%d)", u.u_store.RHandle.BirthTime,
                u.u_store.RHandle.Device, u.u_store.RHandle.Inode);
        fdprint(afd, "\tph = %s (%d)", inet_ntoa(u.u_store.ReintPH),
                u.u_store.ReintPHix);
        fdprint(afd, "\n");
        break;

    case CML_Utimes_OP:
        fdprint(afd, "\t\tfid = %s, utimes = %d\n", FID_(&u.u_utimes.Fid),
                u.u_utimes.Date);
        fdprint(afd, "\t\tvv = ");
        PRINTVV(afd, u.u_utimes.VV);
        fdprint(afd, "\n");
        break;

    case CML_Chown_OP:
        fdprint(afd, "\t\tfid = %s, chown = %d\n", FID_(&u.u_chown.Fid),
                u.u_chown.Owner);
        fdprint(afd, "\t\tvv = ");
        PRINTVV(afd, u.u_chown.VV);
        fdprint(afd, "\n");
        break;

    case CML_Chmod_OP:
        fdprint(afd, "\t\tfid = %s, chmod = %o\n", FID_(&u.u_chmod.Fid),
                u.u_chmod.Mode);
        fdprint(afd, "\t\tvv = ");
        PRINTVV(afd, u.u_chmod.VV);
        fdprint(afd, "\n");
        break;

    case CML_Create_OP:
        fdprint(afd, "\t\tpfid = %s, name = (%s)\n", FID_(&u.u_create.PFid),
                Name);
        fdprint(afd, "\t\tcfid = %s, mode = %o\n", FID_(&u.u_create.CFid),
                u.u_create.Mode);
        fdprint(afd, "\t\tpvv = ");
        PRINTVV(afd, u.u_create.PVV);
        fdprint(afd, "\n");
        break;

    case CML_Remove_OP:
        fdprint(afd, "\t\tpfid = %s, name = (%s)\n", FID_(&u.u_remove.PFid),
                Name);
        fdprint(afd, "\t\tcfid = %s\n", FID_(&u.u_remove.CFid));
        fdprint(afd, "\t\tpvv = ");
        PRINTVV(afd, u.u_remove.PVV);
        fdprint(afd, "\n\t\tcvv = ");
        PRINTVV(afd, u.u_remove.CVV);
        fdprint(afd, "\n");
        break;

    case CML_Link_OP:
        fdprint(afd, "\t\tpfid = %s, name = (%s)\n", FID_(&u.u_link.PFid),
                Name);
        fdprint(afd, "\t\tcfid = %s\n", FID_(&u.u_link.CFid));
        fdprint(afd, "\t\tpvv = ");
        PRINTVV(afd, u.u_link.PVV);
        fdprint(afd, "\n");
        fdprint(afd, "\t\tcvv = ");
        PRINTVV(afd, u.u_link.CVV);
        fdprint(afd, "\n");
        break;

    case CML_Rename_OP:
        fdprint(afd, "\t\tspfid = %s, sname = (%s)\n", FID_(&u.u_rename.SPFid),
                Name);
        fdprint(afd, "\t\ttpfid = %s, tname = (%s)\n", FID_(&u.u_rename.TPFid),
                NewName);
        fdprint(afd, "\t\tsfid = %s\n", FID_(&u.u_rename.SFid));
        fdprint(afd, "\t\tspvv = ");
        PRINTVV(afd, u.u_rename.SPVV);
        fdprint(afd, "\n");
        fdprint(afd, "\t\ttpvv = ");
        PRINTVV(afd, u.u_rename.TPVV);
        fdprint(afd, "\n");
        fdprint(afd, "\t\tsvv = ");
        PRINTVV(afd, u.u_rename.SVV);
        fdprint(afd, "\n");
        break;

    case CML_MakeDir_OP:
        fdprint(afd, "\t\tpfid = %s, name = (%s)\n", FID_(&u.u_mkdir.PFid),
                Name);
        fdprint(afd, "\t\tcfid = %s, mode = %o\n", FID_(&u.u_mkdir.CFid),
                u.u_mkdir.Mode);
        fdprint(afd, "\t\tpvv = ");
        PRINTVV(afd, u.u_mkdir.PVV);
        fdprint(afd, "\n");
        break;

    case CML_RemoveDir_OP:
        fdprint(afd, "\t\tpfid = %s, name = (%s)\n", FID_(&u.u_rmdir.PFid),
                Name);
        fdprint(afd, "\t\tcfid = %s\n", FID_(&u.u_rmdir.CFid));
        fdprint(afd, "\t\tpvv = ");
        PRINTVV(afd, u.u_rmdir.PVV);
        fdprint(afd, "\n");
        fdprint(afd, "\t\tcvv = ");
        PRINTVV(afd, u.u_rmdir.CVV);
        fdprint(afd, "\n");
        break;

    case CML_SymLink_OP:
        fdprint(afd, "\t\tpfid = %s, name = (%s)\n", FID_(&u.u_symlink.PFid),
                NewName);
        fdprint(afd, "\t\tcfid = %s, contents = (%s), mode = %o\n",
                FID_(&u.u_symlink.CFid), Name, u.u_symlink.Mode);
        fdprint(afd, "\t\tpvv = ");
        PRINTVV(afd, u.u_symlink.PVV);
        fdprint(afd, "\n");
        break;

    case CML_Repair_OP:
        fdprint(afd, "\t\tfid = %s, Length = %u\n", FID_(&u.u_repair.Fid),
                u.u_repair.Length);
        fdprint(afd, "\t\tattrs=[%d %d %o]\n", u.u_repair.Date,
                u.u_repair.Owner, u.u_repair.Mode);
        fdprint(afd, "\t\tOVV = ");
        PRINTVV(afd, u.u_repair.OVV);
        fdprint(afd, "\n");
        break;

    default:
        fdprint(afd, "cmlent::print: bogus opcode (%d)", opcode);
        break;
    }
}

/*  *****  Client Modify Log Emulation Routines  *****  */

/* There is a log routine corresponding to each of the (normal) mutating Vice
 * operations, {Store, Truncate, Utimes, Chown, Chmod, Create, Remove, Link,
 * Rename, Mkdir, Rmdir, Symlink}. Note that the only failure mode for these
 * routines is log space exhausted (ENOSPC). Each of these routines MUST be
 * called from within transaction! */

/* local-repair modification */
int reintvol::LogStore(time_t Mtime, uid_t uid, VenusFid *Fid,
                       RPC2_Unsigned NewLength, int prepend)
{
    LOG(1, ("reintvol::LogStore: %d, %d, (%s), %d %d\n", Mtime, uid, FID_(Fid),
            NewLength, prepend));

    if (LogOpts && !prepend) {
        /* Cancel stores, as long as they are not followed by chowns. */
        /* Cancel utimes'. */
        int cancellation;
        do {
            cancellation      = 0;
            cmlent *chown_mle = 0;
            cml_iterator next(CML, AbortOrder, Fid);
            cmlent *m;
            while (!cancellation && (m = next())) {
                switch (m->opcode) {
                case CML_Store_OP:
                    if (chown_mle == 0) {
                        cancellation = m->cancel();
                    }
                    break;

                case CML_Utimes_OP:
                    cancellation = m->cancel();
                    break;

                case CML_Chown_OP:
                    if (chown_mle == 0)
                        chown_mle = m;
                    break;
                }
            }
        } while (cancellation);
    }

    cmlent *store_mle =
        new cmlent(&CML, Mtime, uid, CML_Store_OP, prepend, Fid, NewLength);
    return (store_mle == 0 ? ENOSPC : 0);
}

/* local-repair modification */
int reintvol::LogSetAttr(time_t Mtime, uid_t uid, VenusFid *Fid,
                         RPC2_Unsigned NewLength, Date_t NewDate,
                         UserId NewOwner, RPC2_Unsigned NewMode, int prepend)
{
    /* Record a separate log entry for each attribute that is being set. */
    if (NewLength != (RPC2_Unsigned)-1) {
        int code = LogTruncate(Mtime, uid, Fid, NewLength, prepend);
        if (code != 0)
            return (code);
    }
    if (NewDate != (Date_t)-1) {
        int code = LogUtimes(Mtime, uid, Fid, NewDate, prepend);
        if (code != 0)
            return (code);
    }
    if (NewOwner != (UserId)-1) {
        int code = LogChown(Mtime, uid, Fid, NewOwner, prepend);
        if (code != 0)
            return (code);
    }
    if (NewMode != (RPC2_Unsigned)-1) {
        int code = LogChmod(Mtime, uid, Fid, NewMode, prepend);
        if (code != 0)
            return (code);
    }

    return (0);
}

/* local-repair modification */
int reintvol::LogTruncate(time_t Mtime, uid_t uid, VenusFid *Fid,
                          RPC2_Unsigned NewLength, int prepend)
{
    LOG(1, ("reintvol::LogTruncate: %d, %d, (%s), %d %d\n", Mtime, uid,
            FID_(Fid), NewLength, prepend));

    /* Treat truncates as stores for now. -JJK */
    return (LogStore(Mtime, uid, Fid, NewLength, prepend));
}

/* local-repair modification */
int reintvol::LogUtimes(time_t Mtime, uid_t uid, VenusFid *Fid, Date_t NewDate,
                        int prepend)
{
    LOG(1, ("reintvol::LogUtimes: %d, %d, (%s), %d %d\n", Mtime, uid, FID_(Fid),
            NewDate, prepend));

    if (LogOpts && !prepend) {
        int cancellation;
        do {
            cancellation = 0;
            cml_iterator next(CML, AbortOrder, Fid);
            cmlent *m;
            while (!cancellation && (m = next())) {
                switch (m->opcode) {
                case CML_Utimes_OP:
                    cancellation = m->cancel();
                    break;
                }
            }
        } while (cancellation);
    }

    cmlent *utimes_mle =
        new cmlent(&CML, Mtime, uid, CML_Utimes_OP, prepend, Fid, NewDate);
    return (utimes_mle == 0 ? ENOSPC : 0);
}

/* local-repair modification */
int reintvol::LogChown(time_t Mtime, uid_t uid, VenusFid *Fid, UserId NewOwner,
                       int prepend)
{
    LOG(1, ("reintvol::LogChown: %d, %d, (%s), %d %d\n", Mtime, uid, FID_(Fid),
            NewOwner, prepend));

    if (LogOpts && !prepend) {
        int cancellation;
        do {
            cancellation = 0;
            cml_iterator next(CML, AbortOrder, Fid);
            cmlent *m;
            while (!cancellation && (m = next())) {
                switch (m->opcode) {
                case CML_Chown_OP:
                    cancellation = m->cancel();
                    break;
                }
            }
        } while (cancellation);
    }

    cmlent *chown_mle =
        new cmlent(&CML, Mtime, uid, CML_Chown_OP, prepend, Fid, NewOwner);
    return (chown_mle == 0 ? ENOSPC : 0);
}

/* local-repair modification */
int reintvol::LogChmod(time_t Mtime, uid_t uid, VenusFid *Fid,
                       RPC2_Unsigned NewMode, int prepend)
{
    LOG(1, ("reintvol::LogChmod: %d, %d, (%s), %o %d\n", Mtime, uid, FID_(Fid),
            NewMode, prepend));

    if (LogOpts && !prepend) {
        int cancellation;
        do {
            cancellation      = 0;
            cmlent *store_mle = 0;
            cml_iterator next(CML, AbortOrder, Fid);
            cmlent *m;
            while (!cancellation && (m = next())) {
                switch (m->opcode) {
                case CML_Store_OP:
                    if (store_mle == 0)
                        store_mle = m;
                    break;

                case CML_Chmod_OP:
                    if (store_mle == 0) {
                        cancellation = m->cancel();
                    }
                    break;
                }
            }
        } while (cancellation);
    }

    cmlent *chmod_mle =
        new cmlent(&CML, Mtime, uid, CML_Chmod_OP, prepend, Fid, NewMode);
    return (chmod_mle == 0 ? ENOSPC : 0);
}

/* local-repair modification */
int reintvol::LogCreate(time_t Mtime, uid_t uid, VenusFid *PFid, char *Name,
                        VenusFid *CFid, RPC2_Unsigned Mode, int prepend)
{
    LOG(1, ("reintvol::LogCreate: %d, %d, (%s), %s, (%s), %o %d\n", Mtime, uid,
            FID_(PFid), Name, FID_(CFid), Mode, prepend));

    cmlent *create_mle = new cmlent(&CML, Mtime, uid, CML_Create_OP, prepend,
                                    PFid, Name, CFid, Mode);
    return (create_mle == 0 ? ENOSPC : 0);
}

/* local-repair modification */
int reintvol::LogRemove(time_t Mtime, uid_t uid, VenusFid *PFid, char *Name,
                        const VenusFid *CFid, int LinkCount, int prepend)
{
    LOG(1, ("reintvol::LogRemove: %d, %d, (%s), %s, (%s), %d %d\n", Mtime, uid,
            FID_(PFid), Name, FID_(CFid), LinkCount, prepend));

    int ObjectCreated = 0;

    if (LogOpts && !prepend) {
        if (LinkCount == 1) {
            /*
             * if the object was created here, we may be able to do an
             * identity cancellation.  However, if the create is frozen,
             * we cannot cancel records involved in an identity cancellation,
             * because the create may have already become visible at the servers.
             * Mark such records in case reintegration fails.  Records for which
             * this remove is an overwrite may be cancelled either way. If they
             * are frozen cmlent::cancel does the right thing.
             */
            int CreateReintegrating = 0;
            {
                cml_iterator next(CML, CommitOrder, CFid);
                cmlent *m = next();
                if (m && (m->opcode == CML_Create_OP ||
                          m->opcode == CML_SymLink_OP)) {
                    ObjectCreated = 1;
                    if (m->IsFrozen() && !(m->IsToBeRepaired()))
                        CreateReintegrating = 1;
                }
                /*
                if (ObjectCreated) {
                    int code = LogUtimes(Mtime, uid, PFid, Mtime);
                    if (code != 0) return(code);
                }
                */
            }

            int cancellation;
            do {
                cancellation = 0;
                cml_iterator next(CML, AbortOrder, CFid);
                cmlent *m;
                while (!cancellation && (m = next())) {
                    switch (m->opcode) {
                    case CML_Store_OP:
                    case CML_Utimes_OP:
                    case CML_Chown_OP:
                    case CML_Chmod_OP:
                        cancellation = m->cancel();
                        break;

                    case CML_Create_OP:
                    case CML_Remove_OP:
                    case CML_Link_OP:
                    case CML_Rename_OP:
                    case CML_SymLink_OP:
                        if (ObjectCreated) {
                            if (CreateReintegrating) {
                                RVMLIB_REC_OBJECT(m->flags);
                                m->flags.cancellation_pending = 1;
                            } else
                                cancellation = m->cancel();
                        }
                        break;

                    case CML_Repair_OP:
                        break;

                    default:
                        CODA_ASSERT(0);
                    }
                }
            } while (cancellation);

            if (ObjectCreated && !CreateReintegrating) {
                int size = (int)(sizeof(cmlent) + strlen(Name));

                LOG(0 /*10*/,
                    ("reintvol::LogRemove: record cancelled, %s, size = %d\n",
                     Name, size));
                CML.cancellations.other_count++;
                CML.cancellations.other_size += size;
                return (0);
            }
        }
    }

    cmlent *unlink_mle = new cmlent(&CML, Mtime, uid, CML_Remove_OP, prepend,
                                    PFid, Name, CFid, LinkCount);
    if (ObjectCreated && unlink_mle) { /* must be reintegrating */
        RVMLIB_REC_OBJECT(unlink_mle->flags);
        unlink_mle->flags.cancellation_pending = 1;
    }

    return (unlink_mle == 0 ? ENOSPC : 0);
}

/* local-repair modification */
int reintvol::LogLink(time_t Mtime, uid_t uid, VenusFid *PFid, char *Name,
                      VenusFid *CFid, int prepend)
{
    LOG(1, ("reintvol::LogLink: %d, %d, (%s), %s, (%s) %d\n", Mtime, uid,
            FID_(PFid), Name, FID_(CFid), prepend));

    cmlent *link_mle =
        new cmlent(&CML, Mtime, uid, CML_Link_OP, prepend, PFid, Name, CFid);
    return (link_mle == 0 ? ENOSPC : 0);
}

/* local-repair modification */
int reintvol::LogRename(time_t Mtime, uid_t uid, VenusFid *SPFid, char *OldName,
                        VenusFid *TPFid, char *NewName, VenusFid *SFid,
                        const VenusFid *TFid, int LinkCount, int prepend)
{
    /* Record "target remove" as a separate log entry. */
    if (!FID_EQ(TFid, &NullFid)) {
        int code;
        if (ISDIR(*TFid))
            code = LogRmdir(Mtime, uid, TPFid, NewName, TFid, prepend);
        else
            code =
                LogRemove(Mtime, uid, TPFid, NewName, TFid, LinkCount, prepend);
        if (code != 0)
            return (code);
    }

    LOG(1,
        ("reintvol::LogRename: %d, %d, (%s), %s, (%s), %s, (%s) %d\n", Mtime,
         uid, FID_(SPFid), OldName, FID_(TPFid), NewName, FID_(SFid), prepend));

    cmlent *rename_mle = new cmlent(&CML, Mtime, uid, CML_Rename_OP, prepend,
                                    SPFid, OldName, TPFid, NewName, SFid);
    return (rename_mle == 0 ? ENOSPC : 0);
}

/* local-repair modification */
int reintvol::LogMkdir(time_t Mtime, uid_t uid, VenusFid *PFid, char *Name,
                       VenusFid *CFid, RPC2_Unsigned Mode, int prepend)
{
    LOG(1, ("reintvol::LogMkdir: %d, %d, (%s), %s, (%s), %o %d\n", Mtime, uid,
            FID_(PFid), Name, FID_(CFid), Mode, prepend));

    cmlent *mkdir_mle = new cmlent(&CML, Mtime, uid, CML_MakeDir_OP, prepend,
                                   PFid, Name, CFid, Mode);
    return (mkdir_mle == 0 ? ENOSPC : 0);
}

/* local-repair modification */
int reintvol::LogRmdir(time_t Mtime, uid_t uid, VenusFid *PFid, char *Name,
                       const VenusFid *CFid, int prepend)
{
    LOG(0, ("reintvol::LogRmdir: %d, %d, (%s), %s, (%s) %d\n", Mtime, uid,
            FID_(PFid), Name, FID_(CFid), prepend));

    int ObjectCreated     = 0;
    int DependentChildren = 0;

    if (LogOpts && !prepend) {
        int CreateReintegrating = 0; /* see comments in LogRemove */
        {
            cml_iterator next(CML, CommitOrder, CFid);
            cmlent *m = next();
            if (m && m->opcode == CML_MakeDir_OP) {
                ObjectCreated = 1;
                if (m->IsFrozen())
                    CreateReintegrating = 1;
            }
            if (ObjectCreated) {
                cml_iterator next(CML, AbortOrder, CFid);
                cmlent *m;
                while ((m = next()) && !DependentChildren) {
                    switch (m->opcode) {
                    case CML_Create_OP:
                    case CML_Remove_OP:
                    case CML_Link_OP:
                    case CML_RemoveDir_OP:
                    case CML_SymLink_OP:
                        DependentChildren = 1;
                        break;

                    case CML_Rename_OP:
                        if (!FID_EQ(CFid, &m->u.u_rename.SFid))
                            DependentChildren = 1;
                        break;

                    case CML_MakeDir_OP:
                        if (FID_EQ(CFid, &m->u.u_mkdir.PFid))
                            DependentChildren = 1;
                        break;
                    }
                }
            }
            /*
	    if (ObjectCreated && !DependentChildren) {
		int code = LogUtimes(Mtime, uid, PFid, Mtime);
		if (code != 0) return(code);
	    }
*/
        }

        int cancellation;
        do {
            cancellation = 0;
            cml_iterator next(CML, AbortOrder, CFid);
            cmlent *m;
            while (!cancellation && (m = next())) {
                switch (m->opcode) {
                case CML_Utimes_OP:
                case CML_Chown_OP:
                case CML_Chmod_OP:
                    cancellation = m->cancel();
                    break;

                case CML_Create_OP:
                case CML_Remove_OP:
                case CML_Link_OP:
                case CML_Rename_OP:
                case CML_MakeDir_OP:
                case CML_RemoveDir_OP:
                case CML_SymLink_OP:
                    if (ObjectCreated && !DependentChildren) {
#if 0 /* XXX: won't get optimized on reintegration conflict, cant repair! */
                            if (CreateReintegrating) {
                                RVMLIB_REC_OBJECT(m->flags);
                                m->flags.cancellation_pending = 1;
                            } else
#endif
                        cancellation = m->cancel();
                    }
                    break;

                case CML_Repair_OP:
                    break;

                default:
                    CODA_ASSERT(0);
                }
            }
        } while (cancellation);

        if (ObjectCreated && !DependentChildren && !CreateReintegrating) {
            int size = (int)(sizeof(cmlent) + strlen(Name));

            LOG(0 /*10*/,
                ("reintvol::LogRmdir: record cancelled, %s, size = %d\n", Name,
                 size));

            CML.cancellations.other_count++;
            CML.cancellations.other_size += size;
            return (0);
        }
    }

    cmlent *rmdir_mle = new cmlent(&CML, Mtime, uid, CML_RemoveDir_OP, prepend,
                                   PFid, Name, CFid);

    if (ObjectCreated && !DependentChildren && rmdir_mle) {
        RVMLIB_REC_OBJECT(rmdir_mle->flags);
        rmdir_mle->flags.cancellation_pending = 1;
    }
    return (rmdir_mle == 0 ? ENOSPC : 0);
}

/* local-repair modification */
int reintvol::LogSymlink(time_t Mtime, uid_t uid, VenusFid *PFid, char *Name,
                         char *Contents, VenusFid *CFid, RPC2_Unsigned Mode,
                         int prepend)
{
    LOG(1, ("reintvol::LogSymlink: %d, %d, (%s), %s, %s, (%s), %o %d\n", Mtime,
            uid, FID_(PFid), Name, Contents, FID_(CFid), Mode, prepend));

    cmlent *symlink_mle = new cmlent(&CML, Mtime, uid, CML_SymLink_OP, prepend,
                                     PFid, Name, Contents, CFid, Mode);
    return (symlink_mle == 0 ? ENOSPC : 0);
}

/* local-repair modification */
int reintvol::LogRepair(time_t Mtime, uid_t uid, VenusFid *Fid,
                        RPC2_Unsigned Length, Date_t Date, UserId Owner,
                        RPC2_Unsigned Mode, int prepend)
{
    LOG(1, ("reintvol::LogRepair: %d %d (%s) attrs [%u %d %u %o] %d\n", Mtime,
            uid, FID_(Fid), Length, Date, Owner, Mode, prepend));

    cmlent *repair_mle = new cmlent(&CML, Mtime, uid, CML_Repair_OP, prepend,
                                    Fid, Length, Date, Owner, Mode, prepend);
    return (repair_mle == 0 ? ENOSPC : 0);
}

/* restore ``old values'' for attributes in fsobj. */
/* call from within a transaction. */
void repvol::RestoreObj(VenusFid *Fid)
{
    fsobj *f = FSDB->Find(Fid);

    /* Length attribute. */
    unsigned long Length = 0;
    cmlent *lwriter      = CML.LengthWriter(Fid);
    if (!lwriter) {
        FSO_ASSERT(f, f->CleanStat.Length != (unsigned long)-1);
        Length = f->CleanStat.Length;
    } else {
        VOL_ASSERT(this, lwriter->opcode == CML_Store_OP);
        Length = lwriter->u.u_store.Length;
    }
    if (Length != f->stat.Length) {
        RVMLIB_REC_OBJECT(f->stat.Length);
        f->stat.Length = Length;
    }

    /* Mtime attribute. */
    Date_t Utimes;
    cmlent *uwriter = CML.UtimesWriter(Fid);
    if (uwriter == 0) {
        FSO_ASSERT(f, f->CleanStat.Date != (Date_t)-1);
        Utimes = f->CleanStat.Date;
    } else {
        switch (uwriter->opcode) {
        case CML_Store_OP:
        case CML_Create_OP:
        case CML_MakeDir_OP:
        case CML_SymLink_OP:
        case CML_Repair_OP:
        case CML_Remove_OP:
        case CML_Link_OP:
        case CML_Rename_OP:
        case CML_RemoveDir_OP:
            Utimes = uwriter->time;
            break;

        case CML_Utimes_OP:
            Utimes = uwriter->u.u_utimes.Date;
            break;

        default:
            Utimes = (Date_t)-1;
            VOL_ASSERT(this, 0);
        }
    }
    if (Utimes != f->stat.Date) {
        RVMLIB_REC_OBJECT(f->stat.Date);
        f->stat.Date = Utimes;
    }
}

cmlent *ClientModifyLog::LengthWriter(VenusFid *Fid)
{
    cml_iterator next(*this, AbortOrder, Fid);
    cmlent *m;
    while ((m = next())) {
        if (m->opcode == CML_Store_OP || m->opcode == CML_Repair_OP)
            return (m);
    }

    /* Not found. */
    return (0);
}

cmlent *ClientModifyLog::UtimesWriter(VenusFid *Fid)
{
    cml_iterator next(*this, AbortOrder, Fid);
    cmlent *m;
    while ((m = next())) {
        switch (m->opcode) {
        case CML_Store_OP:
        case CML_Utimes_OP:
        case CML_Create_OP:
        case CML_MakeDir_OP:
        case CML_SymLink_OP:
        case CML_Repair_OP:
            return (m);

        case CML_Remove_OP:
            if (FID_EQ(Fid, &m->u.u_remove.PFid))
                return (m);
            break;

        case CML_Link_OP:
            if (FID_EQ(Fid, &m->u.u_link.PFid))
                return (m);
            break;

        case CML_Rename_OP:
            if (FID_EQ(Fid, &m->u.u_rename.SPFid) ||
                FID_EQ(Fid, &m->u.u_rename.TPFid))
                return (m);
            break;

        case CML_RemoveDir_OP:
            if (FID_EQ(Fid, &m->u.u_rmdir.PFid))
                return (m);
            break;

        default:
            break;
        }
    }

    /* Not found. */
    return (0);
}

/* local-repair modification */
/* MUST be called from within transaction! */
/* returns 1 if record was actually removed from log, 0 if not. */
int cmlent::cancel()
{
    time_t curTime = Vtime();

    if (IsToBeRepaired()) {
        if (log->cancelFrozenEntries && IsFrozen()) {
            LOG(0,
                ("cmlent::cancel: frozen cmlent with local fid, thawing and cancelling\n"));
            Thaw();
        } else {
            LOG(0, ("cmlent::cancel: to_be_repaired cmlent, skip\n"));
            return 0;
        }
    }

    /*
     * If this record is being reintegrated, just mark it for
     * cancellation and we'll get to it later.
     */
    if (IsFrozen()) {
        LOG(0, ("cmlent::cancel: cmlent frozen, skip\n"));
        RVMLIB_REC_OBJECT(flags); /* called from within transaction */
        flags.cancellation_pending = 1;
        return 0;
    }

    LOG(10, ("cmlent::cancel: age = %d\n", curTime - time));
    if (GetLogLevel() >= 10)
        print(GetLogFile());

    /* Parameters for possible utimes to be done AFTER cancelling this record. */
    struct {
        int DoUtimes;
        uid_t Vuid;
        VenusFid Fid;
        Date_t Mtime;
    } Utimes __attribute__((unused)) = { 0 };

    switch (opcode) {
    case CML_Store_OP: {
        /* Cancelling store may permit cancellation of earlier chmod. */

        cmlent *pre_chmod_mle  = 0;
        cmlent *post_chmod_mle = 0;

        {
            cml_iterator next(*(ClientModifyLog *)log, CommitOrder,
                              &u.u_store.Fid, this);
            cmlent *m;
            while ((m = next())) {
                if (m->opcode == CML_Chmod_OP) {
                    post_chmod_mle = m;
                    break;
                }
            }
        }

        if (post_chmod_mle) {
            cml_iterator next(*(ClientModifyLog *)log, AbortOrder,
                              &u.u_store.Fid, this);
            cmlent *m;
            while ((m = next())) {
                if (m->opcode == CML_Chmod_OP) {
                    pre_chmod_mle = m;
                    break;
                }
            }
        }

        if (pre_chmod_mle && post_chmod_mle)
            pre_chmod_mle->cancel();
    } break;

    case CML_Chown_OP: {
        /* Cancelling chown may permit cancellation of earlier store. */

        cmlent *pre_store_mle  = 0;
        cmlent *post_store_mle = 0;

        {
            cml_iterator next(*(ClientModifyLog *)log, CommitOrder,
                              &u.u_chown.Fid, this);
            cmlent *m;
            while ((m = next())) {
                if (m->opcode == CML_Store_OP) {
                    post_store_mle = m;
                    break;
                }
            }
        }

        if (post_store_mle) {
            cml_iterator next(*(ClientModifyLog *)log, AbortOrder,
                              &u.u_chown.Fid, this);
            cmlent *m;
            while ((m = next())) {
                if (m->opcode == CML_Store_OP) {
                    pre_store_mle = m;
                    break;
                }
            }
        }

        if (pre_store_mle && post_store_mle)
            (void)pre_store_mle->cancel();
    } break;

    case CML_Create_OP: {
        cmlent *m = ((ClientModifyLog *)log)->UtimesWriter(&u.u_create.PFid);
        CODA_ASSERT(m != 0);
        if (m != this) {
            Utimes.DoUtimes = 1;
            Utimes.Vuid     = uid;
            Utimes.Fid      = u.u_create.PFid;
            Utimes.Mtime    = time;
        }
    } break;

    case CML_Remove_OP: {
        cmlent *m = ((ClientModifyLog *)log)->UtimesWriter(&u.u_remove.PFid);
        CODA_ASSERT(m != 0);
        if (m != this) {
            Utimes.DoUtimes = 1;
            Utimes.Vuid     = uid;
            Utimes.Fid      = u.u_remove.PFid;
            Utimes.Mtime    = time;
        }
    } break;

    case CML_Link_OP: {
        cmlent *m = ((ClientModifyLog *)log)->UtimesWriter(&u.u_link.PFid);
        CODA_ASSERT(m != 0);
        if (m != this) {
            Utimes.DoUtimes = 1;
            Utimes.Vuid     = uid;
            Utimes.Fid      = u.u_link.PFid;
            Utimes.Mtime    = time;
        }
    } break;

    case CML_Rename_OP: {
        cmlent *m = ((ClientModifyLog *)log)->UtimesWriter(&u.u_rename.SPFid);
        CODA_ASSERT(m != 0);
        if (m != this) {
            Utimes.DoUtimes = 1;
            Utimes.Vuid     = uid;
            Utimes.Fid      = u.u_rename.SPFid;
            Utimes.Mtime    = time;
        }

        if (!FID_EQ(&u.u_rename.SPFid, &u.u_rename.TPFid)) {
            cmlent *m =
                ((ClientModifyLog *)log)->UtimesWriter(&u.u_rename.TPFid);
            CODA_ASSERT(m != 0);
#if 0
		if (m != this) {
		    /* Don't get uptight if this can't be done! */
		    repvol *vol = strbase(repvol, log, CML);
		    (void)vol->LogUtimes(time, uid, &u.u_rename.TPFid, time);
		}
#endif
        }
    } break;

    case CML_MakeDir_OP: {
        cmlent *pre_store_mle  = 0;
        cmlent *post_store_mle = 0;

        {
            cml_iterator next(*(ClientModifyLog *)log, CommitOrder,
                              &u.u_chown.Fid, this);
            cmlent *m;
            while ((m = next())) {
                if (m->opcode == CML_Store_OP) {
                    post_store_mle = m;
                    break;
                }
            }
        }

        if (post_store_mle) {
            cml_iterator next(*(ClientModifyLog *)log, AbortOrder,
                              &u.u_chown.Fid, this);
            cmlent *m;
            while ((m = next())) {
                if (m->opcode == CML_Store_OP) {
                    pre_store_mle = m;
                    break;
                }
            }
        }

        if (pre_store_mle && post_store_mle)
            (void)pre_store_mle->cancel();
    } break;

    case CML_RemoveDir_OP: {
        cmlent *m = ((ClientModifyLog *)log)->UtimesWriter(&u.u_rmdir.PFid);
        CODA_ASSERT(m != 0);
        if (m != this) {
            Utimes.DoUtimes = 1;
            Utimes.Vuid     = uid;
            Utimes.Fid      = u.u_rmdir.PFid;
            Utimes.Mtime    = time;
        }
    } break;

    case CML_SymLink_OP: {
        cmlent *m = ((ClientModifyLog *)log)->UtimesWriter(&u.u_symlink.PFid);
        CODA_ASSERT(m != 0);
        if (m != this) {
            Utimes.DoUtimes = 1;
            Utimes.Vuid     = uid;
            Utimes.Fid      = u.u_symlink.PFid;
            Utimes.Mtime    = time;
        }
    } break;

    case CML_Repair_OP:
        CODA_ASSERT(0);
        break;
    }

    if (opcode == CML_Store_OP) {
        log->cancellations.store_count++;
        log->cancellations.store_size += bytes();
        log->cancellations.store_contents_size += u.u_store.Length;
    } else {
        log->cancellations.other_count++;
        log->cancellations.other_size += bytes();
    }

    repvol *vol = strbase(repvol, log, CML);
    vol->RecordsCancelled++;
    delete this;

#if 0
    if (Utimes.DoUtimes) {
	int code = vol->LogUtimes(Utimes.Mtime, Utimes.Vuid, &Utimes.Fid, Utimes.Mtime);
	CODA_ASSERT(code == 0);
        vol->RecordsCancelled--;
    }
#endif
    return 1;
}

/*
 * If this record is a store corresponding to an open-for-write file,
 * cancel it and restore the object's attributes to their old values.
 */
int cmlent::cancelstore()
{
    int cancelled = 0;
    repvol *vol   = strbase(repvol, log, CML);

    if (opcode == CML_Store_OP) {
        dlink *d   = fid_bindings->first(); /* and only */
        binding *b = strbase(binding, d, binder_handle);
        fsobj *f   = (fsobj *)b->bindee;

        if (WRITING(f)) {
            Recov_BeginTrans();
            /* we could be partial/trickle reintegrating, so cancel may fail */
            if (cancel())
                vol->RestoreObj(&f->fid);
            Recov_EndTrans(MAXFP);
            cancelled = 1;
        }
    }
    return cancelled;
}

/*  *****  Client Modify Log Reintegration Routines  *****  */

/* must not be called from within a transaction */
/* Add timing and statistics gathering! */
int ClientModifyLog::IncReallocFids(int tid)
{
    reintvol *vol = strbase(reintvol, this, CML);
    LOG(1, ("ClientModifyLog::IncReallocFids: (%s) and tid = %d\n", vol->name,
            tid));

    int code = 0;
    cml_iterator next(*this, CommitOrder);
    cmlent *m;
    while ((m = next())) {
        if (m->GetTid() == tid)
            code = m->realloc();
        if (code != 0)
            break;
    }
    LOG(0, ("ClientModifyLog::IncReallocFids: (%s)\n", vol->name));
    return (code);
}

/* MUST be called from within transaction! */
void ClientModifyLog::TranslateFid(VenusFid *OldFid, VenusFid *NewFid)
{
    cml_iterator next(*this, CommitOrder, OldFid);
    cmlent *m;
    while ((m = next()))
        m->translatefid(OldFid, NewFid);
}

/* need not be called from within a transaction */
void ClientModifyLog::IncThread(int tid)
{
    reintvol *vol = strbase(reintvol, this, CML);
    LOG(1, ("ClientModifyLog::IncThread: (%s) tid = %d\n", vol->name, tid));

    /* Initialize "threading" state in dirty fsobj's. */
    {
        cml_iterator next(*this, CommitOrder);
        cmlent *m;
        while ((m = next()))
            if (m->GetTid() == tid) {
                /* we may do this more than once per object */
                dlist_iterator next(*m->fid_bindings);
                dlink *d;

                while ((d = next())) {
                    binding *b = strbase(binding, d, binder_handle);
                    fsobj *f   = (fsobj *)b->bindee;

                    /* sanity checks -- better be an fso */
                    CODA_ASSERT(f && (f->MagicNumber == FSO_MagicNumber));
                    f->tSid = f->stat.VV.StoreId;
                }
            }
    }

    /* Thread version state according to commit order. */
    {
        cml_iterator next(*this, CommitOrder);
        cmlent *m;
        while ((m = next()))
            if (m->GetTid() == tid)
                m->thread();
    }

    if (GetLogLevel() >= 10)
        print(GetLogFile());

    LOG(0, ("ClientModifyLog::IncThread: (%s)\n", vol->name));
}

/* Try to figure out if the cml is reintegrated `out of order', in that case we
 * skip a test on the server that tries to avoid duplicate reintegrations. As a
 * result such a reintegration will generate conflicts when it fails, but that
 * is still better than the previous behaviour where cml entries were dropped.*/
int ClientModifyLog::OutOfOrder(int tid)
{
    cml_iterator next(*this, CommitOrder);
    cmlent *m;
    int suspect = 0;

    while ((m = next())) {
        if (m->flags.prepended)
            return 1;
        if (m->GetTid() != tid) { /* detect skipped entries */
            suspect = 1;
        } else if (suspect) /* found a tid after we skipped any cmlentries? */
            return 1;
    }
    return 0;
}

/* need not be called from within a transaction */
/* Munge the ClientModifyLog into a format suitable for reintegration. */
/* Caller is responsible for deallocating buffer! */
void ClientModifyLog::IncPack(char **bufp, int *bufsizep, int tid)
{
    reintvol *vol = strbase(reintvol, this, CML);
    LOG(1, ("ClientModifyLog::IncPack: (%s) and tid = %d\n", vol->name, tid));

    BUFFER buffer = {};
    buffer.who    = RP2_CLIENT;

    /* Compute size of buffer needed. */
    {
        cml_iterator next(*this, CommitOrder);
        cmlent *mle;
        while ((mle = next()))
            if (mle->GetTid() == tid)
                mle->pack(&buffer);

        *bufsizep = (intptr_t)buffer.buffer;
    }

    /* Allocate such a buffer. */
    *bufp = new char[*bufsizep];

    /* Pack according to commit order. */
    {
        buffer.buffer = *bufp;
        buffer.eob    = *bufp + *bufsizep;

        cml_iterator next(*this, CommitOrder);
        cmlent *mle;
        while ((mle = next()))
            if (mle->GetTid() == tid)
                mle->pack(&buffer);
    }

    LOG(0, ("ClientModifyLog::IncPack: (%s)\n", vol->name));
}

#define UNSET_INDEX -2
#define MAXSTALEDIRS 50

/* MUST NOT be called from within transaction! */
int ClientModifyLog::COP1(char *buf, int bufsize, ViceVersionVector *UpdateSet,
                          int outoforder)
{
    repvol *vol    = strbase(repvol, this, CML);
    int code       = 0;
    unsigned int i = 0;
    mgrpent *m     = 0;

    /* Set up the SE descriptor. */
    SE_Descriptor sed;
    memset(&sed, 0, sizeof(SE_Descriptor));
    sed.Tag                             = SMARTFTP;
    struct SFTP_Descriptor *sei         = &sed.Value.SmartFTPD;
    sei->TransmissionDirection          = CLIENTTOSERVER;
    sei->hashmark                       = 0;
    sei->SeekOffset                     = 0;
    sei->ByteQuota                      = -1;
    sei->Tag                            = FILEINVM;
    sei->FileInfo.ByAddr.vmfile.SeqLen  = bufsize;
    sei->FileInfo.ByAddr.vmfile.SeqBody = (RPC2_ByteSeq)buf;

    /* COP2 Piggybacking. */
    long cbtemp;
    cbtemp = cbbreaks;
    char PiggyData[COP2SIZE];
    RPC2_CountedBS PiggyBS;
    PiggyBS.SeqLen  = 0;
    PiggyBS.SeqBody = (RPC2_ByteSeq)PiggyData;

    /* VCB maintenance */
    RPC2_Integer VS          = 0;
    CallBackStatus VCBStatus = NoCallBack;

    RPC2_Integer Index = UNSET_INDEX;
    ViceFid StaleDirs[MAXSTALEDIRS];
    RPC2_Unsigned MaxStaleDirs = MAXSTALEDIRS;
    RPC2_Unsigned NumStaleDirs = 0;

    /* Acquire an Mgroup. */
    code = vol->GetMgrp(&m, owner, (PIGGYCOP2 ? &PiggyBS : 0));
    if (code != 0)
        goto Exit;

    /* abort reintegration if the user is not authenticated */
    if (!m->IsAuthenticated()) {
        code = ETIMEDOUT; /* treat this as an `spurious disconnection' */
        goto Exit;
    }

    /* XXX when we do not use backfetches the CML should be relatively small,
     * so we can just as well reintegrate to all servers. Or is there still
     * a point where weak reintegration is worth it? */
    if (0 /* vol->IsWeaklyConnected() && m->vsg->NHosts() > 1 */) {
        /* Pick a server and get a connection to it. */
        int ph_ix;
        struct in_addr *phost;
        phost = m->GetPrimaryHost(&ph_ix);
        CODA_ASSERT(phost->s_addr != 0);

        connent *c = 0;
        srvent *s  = GetServer(phost, vol->GetRealmId());
        code       = s->GetConn(&c, owner);
        PutServer(&s);
        if (code != 0)
            goto Exit;

        /* don't bother with VCBs, will lose them on resolve anyway */
        RPC2_CountedBS OldVS;
        OldVS.SeqLen = 0;
        vol->ClearCallBack();

        /* Make the RPC call. */
        MarinerLog("store::Reintegrate %s, (%d, %d)\n", vol->name, count(),
                   bufsize);

        /* We do not piggy the COP2 entries in PiggyBS when talking to only a
	 * single server _as a result of weak connectivity_. We could do an
	 * explicit COP2 call here to ship the PiggyBS array. Or simply ignore
	 * them, so they will eventually be sent automatically or piggied on
	 * the next multirpc. --JH */

        if (PiggyBS.SeqLen && vol->IsReplicated()) {
            code           = vol->COP2(m, &PiggyBS);
            PiggyBS.SeqLen = 0;
        }

        UNI_START_MESSAGE(ViceReintegrate_OP);
        code = (int)ViceReintegrate(c->connid, vol->vid, bufsize, &Index,
                                    outoforder, MaxStaleDirs, &NumStaleDirs,
                                    StaleDirs, &OldVS, &VS, &VCBStatus,
                                    &PiggyBS, &sed);
        UNI_END_MESSAGE(ViceReintegrate_OP);
        MarinerLog("store::reintegrate done\n");

        code = vol->Collate(c, code, 0);
        UNI_RECORD_STATS(ViceReintegrate_OP);

        /*
         * if the return code is EALREADY, the log records up to and
         * including the one with the storeid that matches the
         * uniquifier in Index have been committed at the server.
         * Mark the last of those records.
         */
        if (code == EALREADY)
            MarkCommittedMLE((RPC2_Unsigned)Index);

        /* if there is a semantic failure, mark the offending record */
        if (code != 0 && code != EALREADY && code != ERETRY &&
            code != EWOULDBLOCK && code != ETIMEDOUT)
            MarkFailedMLE((int)Index);

        if (code != 0) {
            PutConn(&c);
            goto Exit;
        }

        bufsize += sed.Value.SmartFTPD.BytesTransferred;
        LOG(10, ("ViceReintegrate: transferred %d bytes\n",
                 sed.Value.SmartFTPD.BytesTransferred));

        /* Purge off stale directory fids, if any. fsobj::Kill is idempotent. */
        LOG(0, ("ClientModifyLog::COP1: %d stale dirs\n", NumStaleDirs));

        /* server may have found more stale dirs */
        if (NumStaleDirs == MaxStaleDirs)
            vol->ClearCallBack();

        for (unsigned int d = 0; d < NumStaleDirs; d++) {
            VenusFid StaleDir;
            MakeVenusFid(&StaleDir, vol->GetRealmId(), &StaleDirs[d]);
            LOG(0, ("ClientModifyLog::COP1: stale dir %s\n", FID_(&StaleDir)));
            fsobj *f = FSDB->Find(&StaleDir);
            if (!f)
                continue;

            Recov_BeginTrans();
            f->Kill();
            Recov_EndTrans(DMFP);
        }

        /* Fashion the update set. */
        InitVV(UpdateSet);
        (&(UpdateSet->Versions.Site0))[ph_ix] = 1;

        /* Indicate that objects should be resolved on commit. */
        vol->flags.resolve_me = 1;
        PutConn(&c);
    } else {
        RPC2_CountedBS OldVS;
        vol->PackVS(VSG_MEMBERS, &OldVS);

        /* Make multiple copies of the IN/OUT and OUT parameters. */
        ARG_MARSHALL(IN_OUT_MODE, SE_Descriptor, sedvar, sed, VSG_MEMBERS);
        ARG_MARSHALL(OUT_MODE, RPC2_Integer, VSvar, VS, VSG_MEMBERS);
        ARG_MARSHALL(OUT_MODE, CallBackStatus, VCBStatusvar, VCBStatus,
                     VSG_MEMBERS);
        ARG_MARSHALL(OUT_MODE, RPC2_Integer, Indexvar, Index, VSG_MEMBERS);
        ARG_MARSHALL(OUT_MODE, RPC2_Integer, NumStaleDirsvar, NumStaleDirs,
                     VSG_MEMBERS);
        ARG_MARSHALL_ARRAY(OUT_MODE, ViceFid, StaleDirsvar, 0, MAXSTALEDIRS,
                           StaleDirs, VSG_MEMBERS);

        /* Make the RPC call. */
        MarinerLog("store::Reintegrate %s, (%d, %d)\n", vol->name, count(),
                   bufsize);

        MULTI_START_MESSAGE(ViceReintegrate_OP);
        code = (int)MRPC_MakeMulti(ViceReintegrate_OP, ViceReintegrate_PTR,
                                   VSG_MEMBERS, m->rocc.handles,
                                   m->rocc.retcodes, m->rocc.MIp, 0, 0,
                                   vol->vid, bufsize, Indexvar_ptrs, outoforder,
                                   MaxStaleDirs, NumStaleDirsvar_ptrs,
                                   StaleDirsvar_ptrs, &OldVS, VSvar_ptrs,
                                   VCBStatusvar_ptrs, &PiggyBS, sedvar_bufs);
        MULTI_END_MESSAGE(ViceReintegrate_OP);

        MarinerLog("store::reintegrate done\n");

        /* Examine the return code to decide what to do next. */
        code = vol->Collate_Reintegrate(m, code, UpdateSet);
        MULTI_RECORD_STATS(ViceReintegrate_OP);

        free(OldVS.SeqBody);

        /* Collate the failure index.  Grab the smallest one. Take special
         * care to treat the index different when an error is returned.
         * This double usage of the index is really asking for trouble! */
        for (i = 0; i < VSG_MEMBERS; i++) {
            if (m->rocc.hosts[i].s_addr != 0) {
                if ((code != EALREADY || m->rocc.retcodes[i] == EALREADY) &&
                    (Index == UNSET_INDEX || Index > Indexvar_bufs[i]))
                    Index = Indexvar_bufs[i];
            }
        }

        /*
         * if the return code is EALREADY, the log records up to and
         * including the one with the storeid that matches the
         * uniquifier in Index have been committed at the server.
         * Mark the last of those records.
         */
        if (code == EALREADY)
            MarkCommittedMLE((RPC2_Unsigned)Index);

        /* if there is a semantic failure, mark the offending record */
        if (code != 0 && code != EALREADY && code != ERETRY &&
            code != EWOULDBLOCK && code != ETIMEDOUT)
            MarkFailedMLE((int)Index);

        if (code == EASYRESOLVE) {
            code = 0;
        }
        if (code != 0)
            goto Exit;

        /* Collate volume callback information */
        if (cbtemp == cbbreaks)
            vol->CollateVCB(m, VSvar_bufs, VCBStatusvar_bufs);

        /* Finalize COP2 Piggybacking. */
        if (PIGGYCOP2 && vol->IsReplicated())
            vol->ClearCOP2(&PiggyBS);

        /* Manually compute the OUT parameters from the mgrpent::Reintegrate() call! -JJK */
        int dh_ix;
        dh_ix = -1;
        (void)m->DHCheck(0, -1, &dh_ix);
        bufsize = 0;
        bufsize += sedvar_bufs[dh_ix].Value.SmartFTPD.BytesTransferred;
        LOG(10, ("ViceReintegrate: transferred %d bytes\n",
                 sedvar_bufs[dh_ix].Value.SmartFTPD.BytesTransferred));

        /*
         * Deal with stale directory fids, if any.  If the client
         * has a volume callback, stale directories must be purged.
         * If not, purging the directories saves an inevitable
         * validation.  Finally, if the number of stale directories
         * found is at maximum, clear the volume callback to be safe.
         */
        for (unsigned int rep = 0; rep < VSG_MEMBERS; rep++)
            /* did this server participate? */
            if (m->rocc.hosts[rep].s_addr != 0) {
                /* must look at all server feedback */
                ARG_UNMARSHALL(NumStaleDirsvar, NumStaleDirs, rep);
                ARG_UNMARSHALL_ARRAY(StaleDirsvar, NumStaleDirs, StaleDirs,
                                     rep);
                LOG(0, ("ClientModifyLog::COP1: (replica %d) %d stale dirs\n",
                        rep, NumStaleDirs));

                /* server may have found more stale dirs */
                if (NumStaleDirs == MaxStaleDirs)
                    vol->ClearCallBack();

                /* purge them off.  fsobj::Kill is idempotent. */
                for (unsigned int d = 0; d < NumStaleDirs; d++) {
                    VenusFid StaleDir;
                    MakeVenusFid(&StaleDir, vol->GetRealmId(), &StaleDirs[d]);
                    LOG(0, ("ClientModifyLog::COP1: stale dir %s\n",
                            FID_(&StaleDir)));
                    fsobj *f = FSDB->Find(&StaleDir);
                    if (!f)
                        continue;

                    Recov_BeginTrans();
                    f->Kill();
                    Recov_EndTrans(DMFP);
                }
            }
    }

Exit:
    if (m)
        m->Put();
    LOG(0, ("ClientModifyLog::COP1: (%s), %d bytes, returns %d, index = %d\n",
            vol->name, bufsize, code, Index));
    return (code);
}

int ClientModifyLog::COP1_NR(char *buf, int bufsize,
                             ViceVersionVector *UpdateSet, int outoforder)
{
    reintvol *vol = strbase(reintvol, this, CML);
    int code      = 0;

    /* Set up the SE descriptor. */
    SE_Descriptor sed;
    memset(&sed, 0, sizeof(SE_Descriptor));
    sed.Tag                             = SMARTFTP;
    struct SFTP_Descriptor *sei         = &sed.Value.SmartFTPD;
    sei->TransmissionDirection          = CLIENTTOSERVER;
    sei->hashmark                       = 0;
    sei->SeekOffset                     = 0;
    sei->ByteQuota                      = -1;
    sei->Tag                            = FILEINVM;
    sei->FileInfo.ByAddr.vmfile.SeqLen  = bufsize;
    sei->FileInfo.ByAddr.vmfile.SeqBody = (RPC2_ByteSeq)buf;

    /* COP2 Piggybacking. */
    long cbtemp = cbbreaks;
    char PiggyData[COP2SIZE];
    RPC2_CountedBS PiggyBS;
    PiggyBS.SeqLen  = 0;
    PiggyBS.SeqBody = (RPC2_ByteSeq)PiggyData;

    /* VCB maintenance */
    RPC2_Integer VS          = 0;
    CallBackStatus VCBStatus = NoCallBack;

    RPC2_Integer Index = UNSET_INDEX;
    ViceFid StaleDirs[MAXSTALEDIRS];
    RPC2_Unsigned MaxStaleDirs = MAXSTALEDIRS;
    RPC2_Unsigned NumStaleDirs = 0;

    connent *c = NULL;
    code       = vol->GetConn(&c, owner, NULL, NULL, NULL);

    // /* abort reintegration if the user is not authenticated */
    // if (!m->IsAuthenticated()) {
    //     code = ETIMEDOUT; /* treat this as an `spurious disconnection' */
    //     goto ExitNonRep;
    // }

    if (code != 0)
        goto ExitNonRep;

    /* don't bother with VCBs, will lose them on resolve anyway */
    RPC2_CountedBS OldVS;
    OldVS.SeqLen = 0;
    vol->PackVS(1, &OldVS);

    /* Make the RPC call. */
    MarinerLog("store::Reintegrate %s, (%d, %d)\n", vol->name, count(),
               bufsize);

    UNI_START_MESSAGE(ViceReintegrate_OP);
    code = (int)ViceReintegrate(c->connid, vol->vid, bufsize, &Index,
                                outoforder, MaxStaleDirs, &NumStaleDirs,
                                StaleDirs, &OldVS, &VS, &VCBStatus, &PiggyBS,
                                &sed);
    UNI_END_MESSAGE(ViceReintegrate_OP);
    MarinerLog("store::reintegrate done\n");

    code = vol->Collate(c, code, 0);
    UNI_RECORD_STATS(ViceReintegrate_OP);

    free(OldVS.SeqBody);

    /*
     * if the return code is EALREADY, the log records up to and
     * including the one with the storeid that matches the
     * uniquifier in Index have been committed at the server.
     * Mark the last of those records.
     */
    if (code == EALREADY)
        MarkCommittedMLE((RPC2_Unsigned)Index);

    /* if there is a semantic failure, mark the offending record */
    if (code != 0 && code != EALREADY && code != ERETRY &&
        code != EWOULDBLOCK && code != ETIMEDOUT)
        MarkFailedMLE((int)Index);

    if (code == EASYRESOLVE) {
        code = 0;
    }
    if (code != 0) {
        PutConn(&c);
        goto ExitNonRep;
    }

    /* Update volume callback information */
    if (cbtemp == cbbreaks) {
        vol->UpdateVCBInfo(VS, VCBStatus);
    }

    bufsize += sed.Value.SmartFTPD.BytesTransferred;
    LOG(10, ("ViceReintegrate: transferred %d bytes\n",
             sed.Value.SmartFTPD.BytesTransferred));

    /* Purge off stale directory fids, if any. fsobj::Kill is idempotent. */
    LOG(0, ("ClientModifyLog::COP1_NR: %d stale dirs\n", NumStaleDirs));

    /* server may have found more stale dirs */
    if (NumStaleDirs == MaxStaleDirs)
        vol->ClearCallBack();

    for (unsigned int d = 0; d < NumStaleDirs; d++) {
        VenusFid StaleDir;
        MakeVenusFid(&StaleDir, vol->GetRealmId(), &StaleDirs[d]);
        LOG(0, ("ClientModifyLog::COP1_NR: stale dir %s\n", FID_(&StaleDir)));
        fsobj *f = FSDB->Find(&StaleDir);
        if (!f)
            continue;

        Recov_BeginTrans();
        f->Kill();
        Recov_EndTrans(DMFP);
    }

    /* Fashion the update set. */
    InitVV(UpdateSet);
    (&(UpdateSet->Versions.Site0))[0] = 1;

    PutConn(&c);

ExitNonRep:
    LOG(0,
        ("ClientModifyLog::COP1_NR: (%s), %d bytes, returns %d, index = %d\n",
         vol->name, bufsize, code, Index));
    return (code);
}

/* Update the version state of fsobj's following successful reintegration. */
/* MUST NOT be called from within transaction! */
void ClientModifyLog::IncCommit(ViceVersionVector *UpdateSet, int Tid)
{
    reintvol *vol = strbase(reintvol, this, CML);

    LOG(1, ("ClientModifyLog::IncCommit: (%s) tid = %d\n", vol->name, Tid));

    CODA_ASSERT(count() > 0);

    Recov_BeginTrans();
    rec_dlist_iterator next(list);
    rec_dlink *d = next(); /* get first element */

    while (d) {
        cmlent *m = strbase(cmlent, d, handle);
        d         = next(); /* advance d before it is un-listed by m->commit */
        if (m->GetTid() == Tid) {
            /* special case -- last of records already committed at server
		 * exit the loop when this has been committed */
            if (m->flags.committed)
                d = NULL;
            m->commit(UpdateSet);
        }
    }
    Recov_EndTrans(DMFP);

    /* flush COP2 for this volume */
    if (vol->IsReplicated())
        ((repvol *)vol)->FlushCOP2();
    vol->flags.resolve_me = 0;
    LOG(0, ("ClientModifyLog::IncCommit: (%s)\n", vol->name));
}

/* Allocate a real fid for a locally created one, and translate all
   references. */
/* Must NOT be called from within transaction! */
int cmlent::realloc()
{
    reintvol *vol = strbase(reintvol, log, CML);
    int code      = 0;

    VenusFid OldFid;
    VenusFid NewFid;
    ViceDataType type;

    switch (opcode) {
    case CML_Create_OP:
        if (!FID_IsLocalFile(MakeViceFid(&u.u_create.CFid)))
            goto Exit;
        OldFid = u.u_create.CFid;
        type   = File;
        break;

    case CML_MakeDir_OP:
        if (!FID_IsLocalDir(MakeViceFid(&u.u_mkdir.CFid)))
            goto Exit;
        OldFid = u.u_mkdir.CFid;
        type   = Directory;
        break;

    case CML_SymLink_OP:
        if (!FID_IsLocalFile(MakeViceFid(&u.u_symlink.CFid)))
            goto Exit;
        OldFid = u.u_symlink.CFid;
        type   = SymbolicLink;
        break;

    default:
        goto Exit;
    }

    code = vol->AllocFid(type, &NewFid, uid, 1);
    if (code == 0) {
        Recov_BeginTrans();
        vol->FidsRealloced++;

        /* Translate fids in log records. */
        log->TranslateFid(&OldFid, &NewFid);

        /* Translate fid in the FSDB. */
        if ((code = FSDB->TranslateFid(&OldFid, &NewFid)) != 0)
            CHOKE("cmlent::realloc: couldn't translate %s -> %s (%d)",
                  FID_(&OldFid), FID_(&NewFid), code);
        Recov_EndTrans(MAXFP);
    }

Exit:
    return (code);
}

/* MUST be called from within transaction! */
void cmlent::translatefid(VenusFid *OldFid, VenusFid *NewFid)
{
    int found = 0; /* sanity checking */
    RVMLIB_REC_OBJECT(u);
    switch (opcode) {
    case CML_Store_OP:
        if (FID_EQ(&u.u_store.Fid, OldFid)) {
            u.u_store.Fid = *NewFid;
            found         = 1;
        }
        break;

    case CML_Utimes_OP:
        if (FID_EQ(&u.u_utimes.Fid, OldFid)) {
            u.u_utimes.Fid = *NewFid;
            found          = 1;
        }
        break;

    case CML_Chown_OP:
        if (FID_EQ(&u.u_chown.Fid, OldFid)) {
            u.u_chown.Fid = *NewFid;
            found         = 1;
        }
        break;

    case CML_Chmod_OP:
        if (FID_EQ(&u.u_chmod.Fid, OldFid)) {
            u.u_chmod.Fid = *NewFid;
            found         = 1;
        }
        break;

    case CML_Create_OP:
        if (FID_EQ(&u.u_create.PFid, OldFid)) {
            u.u_create.PFid = *NewFid;
            found           = 1;
        }
        if (FID_EQ(&u.u_create.CFid, OldFid)) {
            u.u_create.CFid = *NewFid;
            found           = 1;
        }
        break;

    case CML_Remove_OP:
        if (FID_EQ(&u.u_remove.PFid, OldFid)) {
            u.u_remove.PFid = *NewFid;
            found           = 1;
        }
        if (FID_EQ(&u.u_remove.CFid, OldFid)) {
            u.u_remove.CFid = *NewFid;
            found           = 1;
        }
        break;

    case CML_Link_OP:
        if (FID_EQ(&u.u_link.PFid, OldFid)) {
            u.u_link.PFid = *NewFid;
            found         = 1;
        }
        if (FID_EQ(&u.u_link.CFid, OldFid)) {
            u.u_link.CFid = *NewFid;
            found         = 1;
        }
        break;

    case CML_Rename_OP:
        if (FID_EQ(&u.u_rename.SPFid, OldFid)) {
            u.u_rename.SPFid = *NewFid;
            found            = 1;
        }
        if (FID_EQ(&u.u_rename.TPFid, OldFid)) {
            u.u_rename.TPFid = *NewFid;
            found            = 1;
        }
        if (FID_EQ(&u.u_rename.SFid, OldFid)) {
            u.u_rename.SFid = *NewFid;
            found           = 1;
        }
        break;

    case CML_MakeDir_OP:
        if (FID_EQ(&u.u_mkdir.PFid, OldFid)) {
            u.u_mkdir.PFid = *NewFid;
            found          = 1;
        }
        if (FID_EQ(&u.u_mkdir.CFid, OldFid)) {
            u.u_mkdir.CFid = *NewFid;
            found          = 1;
        }
        break;

    case CML_RemoveDir_OP:
        if (FID_EQ(&u.u_rmdir.PFid, OldFid)) {
            u.u_rmdir.PFid = *NewFid;
            found          = 1;
        }
        if (FID_EQ(&u.u_rmdir.CFid, OldFid)) {
            u.u_rmdir.CFid = *NewFid;
            found          = 1;
        }
        break;

    case CML_SymLink_OP:
        if (FID_EQ(&u.u_symlink.PFid, OldFid)) {
            u.u_symlink.PFid = *NewFid;
            found            = 1;
        }
        if (FID_EQ(&u.u_symlink.CFid, OldFid)) {
            u.u_symlink.CFid = *NewFid;
            found            = 1;
        }
        break;

    case CML_Repair_OP: /* Shouldn't be called for repair */
    default:
        CHOKE("cmlent::translatefid: bogus opcode (%d)", opcode);
    }
    if (!found) {
        print(GetLogFile());
        CHOKE("cmlent::translatefid: (%s) not matched", FID_(OldFid));
    }
}

/* local-repair modification */
void cmlent::thread()
{
    VenusFid *fids[3];
    ViceVersionVector *vvs[3];

    GetVVandFids(vvs, fids);
    for (int i = 0; i < 3; i++) {
        VenusFid *fidp = fids[i];
        if (fidp == 0)
            break;

        fsobj *f = FSDB->Find(fidp);
        if (f == 0) {
            print(GetLogFile());
            (strbase(repvol, log, CML))->print(GetLogFile());
            CHOKE("cmlent::thread: can't find (%s)", FID_(fidp));
        }

        /* Thread the VVs. */
        if (vvs[i] != 0) {
            *(vvs[i])       = f->stat.VV;
            vvs[i]->StoreId = f->tSid;
        }
        f->tSid = sid;
    }
}

int cmlent::size()
{
    BUFFER buffer = {};
    buffer.who    = RP2_CLIENT;

    pack(&buffer);
    return (intptr_t)buffer.buffer;
}

/* local-repair modification */
/* Pack this record into an RPC buffer for transmission to the server. */
void cmlent::pack(BUFFER *bufptr)
{
    ViceVersionVector TPVV;

    pack_integer(bufptr, opcode); /* Stick in opcode. */
    pack_unsigned(bufptr, time); /* Stick in modify time. */

    switch (opcode) {
    case CML_Create_OP:
        pack_CML_Create_request(bufptr, MakeViceFid(&u.u_create.PFid),
                                &u.u_create.PVV, Name, (vuid_t)uid,
                                u.u_create.Mode, MakeViceFid(&u.u_create.CFid),
                                &sid);
        break;

    case CML_Link_OP:
        pack_CML_Link_request(bufptr, MakeViceFid(&u.u_link.PFid),
                              &u.u_link.PVV, Name, MakeViceFid(&u.u_link.CFid),
                              &u.u_link.CVV, &sid);
        break;

    case CML_MakeDir_OP:
        pack_CML_MakeDir_request(bufptr, MakeViceFid(&u.u_mkdir.PFid),
                                 &u.u_mkdir.PVV, Name,
                                 MakeViceFid(&u.u_mkdir.CFid), (vuid_t)uid,
                                 u.u_mkdir.Mode, &sid);
        break;

    case CML_SymLink_OP:
        pack_CML_SymLink_request(bufptr, MakeViceFid(&u.u_symlink.PFid),
                                 &u.u_symlink.PVV, NewName, Name,
                                 MakeViceFid(&u.u_symlink.CFid), (vuid_t)uid,
                                 u.u_symlink.Mode, &sid);
        break;

    case CML_Remove_OP:
        pack_CML_Remove_request(bufptr, MakeViceFid(&u.u_remove.PFid),
                                &u.u_remove.PVV, Name, &u.u_remove.CVV, &sid);
        break;

    case CML_RemoveDir_OP:
        pack_CML_RemoveDir_request(bufptr, MakeViceFid(&u.u_rmdir.PFid),
                                   &u.u_rmdir.PVV, Name, &u.u_rmdir.CVV, &sid);
        break;

    case CML_Store_OP:
        pack_CML_Store_request(bufptr, MakeViceFid(&u.u_store.Fid),
                               &u.u_store.VV, u.u_store.Length, &sid);
        break;

    case CML_Utimes_OP:
        pack_CML_Utimes_request(bufptr, MakeViceFid(&u.u_utimes.Fid),
                                &u.u_utimes.VV, u.u_utimes.Date, &sid);
        break;

    case CML_Chown_OP:
        pack_CML_Chown_request(bufptr, MakeViceFid(&u.u_chown.Fid),
                               &u.u_chown.VV, (vuid_t)u.u_chown.Owner, &sid);
        break;

    case CML_Chmod_OP:
        pack_CML_Chmod_request(bufptr, MakeViceFid(&u.u_chmod.Fid),
                               &u.u_chmod.VV, u.u_chmod.Mode, &sid);
        break;

    case CML_Rename_OP:
        TPVV = FID_EQ(&u.u_rename.SPFid, &u.u_rename.TPFid) ? u.u_rename.SPVV :
                                                              u.u_rename.TPVV;
        pack_CML_Rename_request(bufptr, MakeViceFid(&u.u_rename.SPFid),
                                &u.u_rename.SPVV, Name,
                                MakeViceFid(&u.u_rename.TPFid), &TPVV, NewName,
                                &u.u_rename.SVV, &sid);
        break;

    case CML_Repair_OP:
        pack_CML_Repair_request(bufptr, MakeViceFid(&u.u_repair.Fid),
                                u.u_repair.Length, u.u_repair.Date,
                                (vuid_t)u.u_repair.Owner,
                                (vuid_t)u.u_repair.Owner, u.u_repair.Mode,
                                &sid);
        break;

    default:
        CHOKE("cmlent::pack: bogus opcode (%d)", opcode);
    }
}

/* local-repair modification */
/* MUST be called from within transaction! */
void cmlent::commit(ViceVersionVector *UpdateSet)
{
    LOG(1, ("cmlent::commit: (%d)\n", tid));

    reintvol *vol = strbase(reintvol, log, CML);
    repvol *rv    = (repvol *)vol;
    vol->RecordsCommitted++;
    ViceVersionVector renameUpdateSet = *UpdateSet;

    /*
     * Record StoreId/UpdateSet for objects involved in this operation ONLY
     * when this is the  FINAL mutation of the object. Record a COP2 entry
     * only if this operation was final for ANY object!
     * Because of the addition of incremental reintegration, the final
     * mutation should be checked only within the bound of a single unit
     * (identified by cmlent::tid) -luqi
     */
    /* Interesting, in the git history this seems to have piggybacked on an
     * RPC2 removal commit (db328a2d60389a57ff12bb20eff14c6961468083), but it
     * looks like it was related to some effort to make sure we correctly
     * resolved any pending conflicts. I feel like the original code was
     * probably doing something right, so I am not going to leave this unused
     * variable around and just squelch the compiler warnings for now. -JH */
    int FinalMutationForAnyObject __attribute__((unused)) = 0;

    dlist_iterator next(*fid_bindings);
    dlink *d;
    while ((d = next())) {
        binding *b = strbase(binding, d, binder_handle);
        fsobj *f   = (fsobj *)b->bindee;

        /* better be an fso */
        CODA_ASSERT(f && (f->MagicNumber == FSO_MagicNumber));

        if (vol->IsReplicated()) {
            if (rv->flags.resolve_me && rv->AVSGsize() > 1)
                rv->ResSubmit(NULL, &f->fid);
        }

        cmlent *FinalCmlent = f->FinalCmlent(tid);
        if (vol->IsNonReplicated()) {
            RVMLIB_REC_OBJECT(f->stat.VV);

            switch (opcode) {
            case CML_MakeDir_OP:
            case CML_SymLink_OP:
                /* Compensate for the version being initialized to NullVV insted of
                     * 1 for the first slot as the server does. */
                if (FID_EQ(&u.u_mkdir.CFid, &f->fid)) {
                    AddVVs(&f->stat.VV, UpdateSet);
                }
                break;
            case CML_Rename_OP:
                /* For rename within same directory only increment version once */
                if (FID_EQ(&u.u_rename.TPFid, &u.u_rename.SPFid) &&
                    FID_EQ(&u.u_rename.TPFid, &f->fid)) {
                    f->stat.VV.StoreId = sid;
                    AddVVs(&f->stat.VV, &renameUpdateSet);
                    renameUpdateSet = NullVV;
                    continue;
                }
            default:
                break;
            }

            f->stat.VV.StoreId = sid;
            AddVVs(&f->stat.VV, UpdateSet);

        } else if (FinalCmlent == this) {
            LOG(10, ("cmlent::commit: FinalCmlent for %s\n", FID_(&f->fid)));
            /*
             * if the final update removed the object, don't bother adding the
             * COP2, but do update the version vector as in connected mode.
             */
            if (!(opcode == CML_Remove_OP &&
                  FID_EQ(&u.u_remove.CFid, &f->fid)) &&
                !(opcode == CML_RemoveDir_OP &&
                  FID_EQ(&u.u_rmdir.CFid, &f->fid)))
                FinalMutationForAnyObject = 1;

            RVMLIB_REC_OBJECT(f->stat.VV);
            f->stat.VV.StoreId = sid;
            AddVVs(&f->stat.VV, UpdateSet);
        }
    }

    if (vol->IsReplicated() /* FinalMutationForAnyObject */) {
        ((repvol *)vol)->AddCOP2(&sid, UpdateSet);
        LOG(10, ("cmlent::commit: Add COP2 with sid = 0x%x.%x\n", sid.HostId,
                 sid.Uniquifier));
    }

    delete this;
}

int cmlent::HaveReintegrationHandle()
{
    return (opcode == CML_Store_OP && u.u_store.RHandle.BirthTime);
}

/* MUST NOT be called from within transaction! */
void cmlent::ClearReintegrationHandle()
{
    CODA_ASSERT(opcode == CML_Store_OP);

    Recov_BeginTrans();
    RVMLIB_REC_OBJECT(u);
    memset(&u.u_store.RHandle, 0, sizeof(ViceReintHandle));
    u.u_store.Offset         = 0;
    u.u_store.ReintPH.s_addr = 0;
    u.u_store.ReintPHix      = -1;
    Recov_EndTrans(MAXFP);
}

int cmlent::DoneSending()
{
    int done = 0;

    if (HaveReintegrationHandle() && (u.u_store.Offset == u.u_store.Length))
        done = 1;

    return (done);
}

int cmlent::GetReintegrationHandle()
{
    reintvol *vol = strbase(reintvol, log, CML);
    int code      = 0;
    mgrpent *m    = 0;
    int ph_ix;
    struct in_addr phost;
    connent *c = NULL;

    /* Eventhough it might seem like a good idea, we should NOT! clear the
     * reintegration handle here. There are 2 failure cases, one is that one
     * replica is unreachable, the other is that all replicas died.
     *
     * In the first case, the SendReintegrateFragment rpc times out, we try to
     * revalidate the handle with the same server which fails as well. We end
     * up here trying to connect to another replica and if that succeeds we
     * replace the existing handle with the new data and retry the
     * reintegration from the beginning. On the second case everything is
     * unreachable and we still have a usable handle to reconnect to the
     * original server when the network comes back.
     */

    code = vol->GetConn(&c, log->owner, &m, &ph_ix, &phost);
    if (code != 0)
        goto Exit;

    CODA_ASSERT(vol->IsReadWrite());

    {
        ViceReintHandle VR;

        /* Make the RPC call. */
        MarinerLog("store::OpenReintHandle %s\n", vol->name);
        UNI_START_MESSAGE(ViceOpenReintHandle_OP);
        code = (int)ViceOpenReintHandle(c->connid, MakeViceFid(&u.u_store.Fid),
                                        &VR);
        UNI_END_MESSAGE(ViceOpenReintHandle_OP);
        MarinerLog("store::openreinthandle done\n");

        /* Examine the return code to decide what to do next. */
        code = vol->Collate(c, code);
        UNI_RECORD_STATS(ViceOpenReintHandle_OP);

        if (code != 0)
            goto Exit;

        /* Store the handle and the primary host */
        Recov_BeginTrans();
        RVMLIB_REC_OBJECT(u);
        u.u_store.RHandle   = VR;
        u.u_store.Offset    = 0;
        u.u_store.ReintPH   = phost;
        u.u_store.ReintPHix = ph_ix;
        Recov_EndTrans(MAXFP);
    }

Exit:
    PutConn(&c);
    if (m)
        m->Put();
    LOG(0, ("cmlent::GetReintegrationHandle: (%s), returns %s\n", vol->name,
            VenusRetStr(code)));
    return (code);
}

int cmlent::ValidateReintegrationHandle()
{
    reintvol *vol        = strbase(reintvol, log, CML);
    int code             = 0;
    connent *c           = 0;
    RPC2_Unsigned Offset = 0;

    /* Acquire a connection. */
    srvent *s = GetServer(&u.u_store.ReintPH, vol->GetRealmId());
    code      = s->GetConn(&c, log->owner);
    PutServer(&s);
    if (code != 0)
        goto Exit;

    {
        /* Make the RPC call. */
        MarinerLog("store::QueryReintHandle %s\n", vol->name);
        UNI_START_MESSAGE(ViceQueryReintHandle_OP);
        code = (int)ViceQueryReintHandle(c->connid, vol->vid,
                                         &u.u_store.RHandle, &Offset);
        UNI_END_MESSAGE(ViceQueryReintHandle_OP);
        MarinerLog("store::queryreinthandle done\n");

        /* Examine the return code to decide what to do next. */
        code = vol->Collate(c, code);
        UNI_RECORD_STATS(ViceQueryReintHandle_OP);

        if (code != 0)
            goto Exit;

        if (Offset > u.u_store.Length)
            CHOKE(
                "cmlent::QueryReintegrationHandle: offset > length! (%d, %d)\n",
                Offset, u.u_store.Length);

        Recov_BeginTrans();
        RVMLIB_REC_OBJECT(u);
        u.u_store.Offset = Offset;
        Recov_EndTrans(MAXFP);
    }

Exit:
    PutConn(&c);
    LOG(0, ("cmlent::QueryReintegrationHandle: (%s), returns %s, offset %d\n",
            vol->name, VenusRetStr(code), Offset));
    return (code);
}

int cmlent::WriteReintegrationHandle(unsigned long *reint_time)
{
    CODA_ASSERT(opcode == CML_Store_OP);
    repvol *vol = strbase(repvol, log, CML);
    int code = 0, fd = -1;
    connent *c           = 0;
    fsobj *f             = NULL;
    RPC2_Unsigned length = u.u_store.Length - u.u_store.Offset;

    if (!vol->IsSync())
        length = ReintAmount(reint_time);

    /* stop reintegration loop if we ran out of available reintegration time */
    if (length == 0 && u.u_store.Offset != u.u_store.Length)
        return ERETRY;

    /* Acquire a connection. */
    srvent *s = GetServer(&u.u_store.ReintPH, vol->GetRealmId());
    code      = s->GetConn(&c, log->owner);
    PutServer(&s);
    if (code != 0)
        goto Exit;

    {
        /* get the fso associated with this record */
        binding *b = strbase(binding, fid_bindings->first(), binder_handle);
        f          = (fsobj *)b->bindee;
        if (f == 0) {
            code = ENOENT;
            goto Exit;
        }

        if (!f->shadow)
            CHOKE("cmlent::WriteReintegrationHandle: no shadow file! (%s)\n",
                  FID_(&f->fid));

        /* Sanity checks. */
        if (!f->IsFile() || !HAVEALLDATA(f)) {
            code = EINVAL;
            goto Exit;
        }

        /* Set up the SE descriptor. */
        SE_Descriptor sed;
        memset(&sed, 0, sizeof(SE_Descriptor));
        {
            sed.Tag                     = SMARTFTP;
            struct SFTP_Descriptor *sei = &sed.Value.SmartFTPD;
            sei->TransmissionDirection  = CLIENTTOSERVER;
            sei->hashmark               = 0;
            sei->SeekOffset             = u.u_store.Offset;
            sei->ByteQuota              = length;

            /* and open the containerfile */
            fd = f->shadow->Open(O_RDONLY);

            sei->Tag              = FILEBYFD;
            sei->FileInfo.ByFD.fd = fd;
        }

        /* Notify Codacon */
        MarinerLog("store::SendReintFragment %s, %s [%d] (%d/%d)\n", vol->name,
                   f->GetComp(), NBLOCKS(length), u.u_store.Offset,
                   u.u_store.Length);

        /* Make the RPC call. */
        UNI_START_MESSAGE(ViceSendReintFragment_OP);
        code = (int)ViceSendReintFragment(c->connid, vol->vid,
                                          &u.u_store.RHandle, length, &sed);
        UNI_END_MESSAGE(ViceSendReintFragment_OP);
        MarinerLog("store::sendreintfragment done\n");

        code = vol->Collate(c, code);
        UNI_RECORD_STATS(ViceSendReintFragment_OP);

        if (code != 0)
            goto Exit;

        if ((long)length != sed.Value.SmartFTPD.BytesTransferred)
            CHOKE("cmlent::WriteReintegrateHandle: bytes mismatch (%d, %d)\n",
                  length, sed.Value.SmartFTPD.BytesTransferred);

        Recov_BeginTrans();
        RVMLIB_REC_OBJECT(u);
        u.u_store.Offset += length;
        Recov_EndTrans(MAXFP);
    }

Exit:
    if (fd != -1)
        f->shadow->Close(fd);

    PutConn(&c);
    LOG(0,
        ("cmlent::WriteReintegrateHandle: (%s), %d bytes, returns %s, new offset %d\n",
         vol->name, length, VenusRetStr(code), u.u_store.Offset));
    return (code);
}

int cmlent::CloseReintegrationHandle(char *buf, int bufsize,
                                     ViceVersionVector *UpdateSet)
{
    reintvol *vol = strbase(reintvol, log, CML);
    repvol *rv    = strbase(repvol, log, CML);
    volrep *vr    = NULL;
    int code      = 0;
    connent *c    = NULL;

    /* Set up the SE descriptor. */
    SE_Descriptor sed;
    memset(&sed, 0, sizeof(SE_Descriptor));
    sed.Tag                             = SMARTFTP;
    struct SFTP_Descriptor *sei         = &sed.Value.SmartFTPD;
    sei->TransmissionDirection          = CLIENTTOSERVER;
    sei->hashmark                       = 0;
    sei->SeekOffset                     = 0;
    sei->ByteQuota                      = -1;
    sei->Tag                            = FILEINVM;
    sei->FileInfo.ByAddr.vmfile.SeqLen  = bufsize;
    sei->FileInfo.ByAddr.vmfile.SeqBody = (RPC2_ByteSeq)buf;

    RPC2_Integer VS          = 0;
    CallBackStatus VCBStatus = NoCallBack;

    /* COP2 Piggybacking. */
    char PiggyData[COP2SIZE];
    RPC2_CountedBS empty_PiggyBS;
    empty_PiggyBS.SeqLen  = 0;
    empty_PiggyBS.SeqBody = (RPC2_ByteSeq)PiggyData;

    if (vol->IsReplicated()) {
        /* Get a connection to the server. */
        srvent *s = GetServer(&u.u_store.ReintPH, vol->GetRealmId());
        code      = s->GetConn(&c, log->owner);
        PutServer(&s);
        if (code != 0)
            goto Exit;
    }

    if (vol->IsNonReplicated()) {
        vr                  = (volrep *)vol;
        code                = vr->GetConn(&c, log->owner);
        u.u_store.ReintPHix = 0;
        if (code != 0)
            goto Exit;
    }

    CODA_ASSERT(vol->IsReadWrite());

    /* don't bother with VCBs, will lose them on resolve anyway */
    RPC2_CountedBS OldVS;
    OldVS.SeqLen = 0;
    if (vol->IsReadWrite())
        rv->ClearCallBack();

    /* Make the RPC call. */
    MarinerLog("store::CloseReintHandle %s, (%d)\n", vol->name, bufsize);
    UNI_START_MESSAGE(ViceCloseReintHandle_OP);
    code = (int)ViceCloseReintHandle(c->connid, vol->vid, bufsize,
                                     &u.u_store.RHandle, &OldVS, &VS,
                                     &VCBStatus, &empty_PiggyBS, &sed);
    UNI_END_MESSAGE(ViceCloseReintHandle_OP);
    MarinerLog("store::closereinthandle done\n");

    code = vol->Collate(c, code, 0);
    UNI_RECORD_STATS(ViceCloseReintHandle_OP);

    if (code != 0)
        goto Exit;

    LOG(0 /*10*/, ("ViceCloseReintegrationHandle: transferred %d bytes\n",
                   sed.Value.SmartFTPD.BytesTransferred));

    /* Fashion the update set. */
    InitVV(UpdateSet);
    (&(UpdateSet->Versions.Site0))[u.u_store.ReintPHix] = 1;

    /* Indicate that objects should be resolved on commit. */
    vol->flags.resolve_me = 1;

Exit:
    PutConn(&c);
    LOG(0, ("cmlent::CloseReintegrationHandle: (%s), %d bytes, returns %s\n",
            vol->name, bufsize, VenusRetStr(code)));
    return (code);
}

/*  *****  Routines for Handling Inconsistencies and Safeguarding Against Catastrophe  *****  */

int PathAltered(VenusFid *cfid, char *suffix, ClientModifyLog *CML,
                cmlent *starter)
{
    char buf[MAXPATHLEN];
    cml_iterator next(*CML, CommitOrder);
    cmlent *m;

    /* can't use cml_iterator's prelude because we need to start from starter! */
    while ((m = next())) {
        if (m == starter)
            break;
    }

    while (m) {
        if (m->opcode == CML_Remove_OP && FID_EQ(&m->u.u_remove.CFid, cfid)) {
            /* when the cfid is removed, prepend suffix and replace cfid with its father */
            if (suffix[0])
                sprintf(buf, "%s/%s", m->Name, suffix);
            else
                sprintf(buf, "%s", m->Name);
            strcpy(suffix, buf);
            *cfid = m->u.u_remove.PFid;
            return 1;
        }

        if (m->opcode == CML_RemoveDir_OP && FID_EQ(&m->u.u_rmdir.CFid, cfid)) {
            /*
	     * when the current fid(directory) is removed, prepend suffix
	     * replace cfid with its father.
	     */
            if (suffix[0])
                sprintf(buf, "%s/%s", m->Name, suffix);
            else
                sprintf(buf, "%s", m->Name);
            strcpy(suffix, buf);
            *cfid = m->u.u_rmdir.PFid;
            return 1;
        }

        if (m->opcode == CML_Rename_OP && FID_EQ(&m->u.u_rename.SFid, cfid)) {
            /*
	     * when the current fid is renamed, prepend the original name to
	     * suffix and replace cfid with the original father fid.
	     */
            if (suffix[0])
                sprintf(buf, "%s/%s", m->Name, suffix);
            else
                sprintf(buf, "%s", m->Name);
            strcpy(suffix, buf);
            *cfid = m->u.u_rename.SPFid;
            return 1;
        }
        m = next();
    }
    return 0;
}

/* local-repair modification */
void RecoverPathName(char *path, VenusFid *fid, ClientModifyLog *CML,
                     cmlent *starter)
{
    /* this algorithm is single-volume based */
    CODA_ASSERT(path && fid && CML && starter);
    LOG(100, ("RecoverPathName: fid = %s\n", FID_(fid)));

    VenusFid cfid = *fid;
    char suffix[MAXPATHLEN];
    char buf[MAXPATHLEN];

    suffix[0] = '\0';

    /* the loog invariant is "path(cfid)/suffix == path(fid)" */
    while (!FID_IsVolRoot(&cfid)) {
        /* while the current fid is root of the volume */
        if (!PathAltered(&cfid, suffix, CML, starter)) {
            /*
	     * only deal with the situation when cfid has been not removed or
	     * renamed. otherwise, cfid and suffix has alread been adjusted by
	     * PathAltered to maintain the loop invariant. Note that the object
	     * corresponding to cfid now is guaranteed to be alive.
	     */
            fsobj *f = FSDB->Find(&cfid);
            if (f == NULL) {
                LOG(0, ("RecoverPathName: fid = %s object no cached\n",
                        FID_(&cfid)));
                /* gcc-3.2 barfs about trigraps when it sees ? ? /, by
		 * splitting it up in two strings that are joined by the
		 * preprocessor we avoid this warning. */
                sprintf(path,
                        "??"
                        "?/%s",
                        suffix);
                return;
            }
            if (suffix[0])
                sprintf(buf, "%s/%s", f->comp, suffix);
            else
                sprintf(buf, "%s", f->comp);
            strcpy(suffix, buf);

            /* going up to its parent */
            if (f->IsRoot() && f->u.mtpoint) {
                /* this must be the global-root-node of a local-repair subtree */
                cfid = f->u.mtpoint->pfid;
            } else {
                cfid = f->pfid;
            }
        }
    }
    char prefix[MAXPATHLEN];
    fsobj *f =
        FSDB->Find(&cfid); /* find the root object of the lowest volume */
    if (f == NULL) {
        LOG(0, ("RecoverPathName: volume root %s not cached\n", FID_(&cfid)));
        /* gcc-3.2 barfs about trigraps when it sees ? ? / */
        sprintf(path,
                "??"
                "?/%s",
                suffix);
        return;
    }
    f->GetPath(prefix, 1);
    if (suffix[0])
        sprintf(path, "%s/%s", prefix, suffix);
    else
        sprintf(path, "%s", prefix);
}

int reintvol::CheckPointMLEs(uid_t uid, char *ckpdir)
{
    if (CML.count() == 0)
        return (ENOENT);
    if (CML.owner != uid && uid != V_UID)
        return (EACCES);

    if (rvmlib_in_transaction()) {
        CHOKE("CheckPointMLEs started while in transaction!");
    }

    int code = CML.CheckPoint(ckpdir);
    return (code);
}

/* MUST NOT be called from within transaction! */
int reintvol::PurgeMLEs(uid_t uid)
{
    if (CML.count() == 0)
        return (ENOENT);
    if (CML.owner != uid && uid != V_UID)
        return (EACCES);
    if (IsReadWrite() && ((reintvol *)this)->IsReintegrating())
        return EACCES;

    LOG(0, ("volent::PurgeMLEs:(%s) (%x.%x)\n", name, realm->Id(), vid));

    /*
     * Step 1 was removed on 7/8/05
     */

    { /*
	 * Step 2: cleanup everything in the CML, even there are records
	 * marked as to-be-repaired or repair-mutation.
	 */
        cmlent *m;
        rec_dlist_iterator next(CML.list, AbortOrder);
        rec_dlink *d = next(); /* get the first (last) element */
        while (1) {
            if (!d)
                break;
            m = strbase(cmlent, d, handle);
            d = next();
            Recov_BeginTrans();
            if (m->IsToBeRepaired())
                /*
                 * this record must be associated with
                 * some local objects whose subtree root
                 * is not in this volume. since we kill the
                 * local objects later, we use cmlent destructor
                 * instead of the cmlent::abort().
                 */
                delete m;
            else
                m->abort();
            Recov_EndTrans(MAXFP);
        }
        VOL_ASSERT(this, CML.count() == 0);
    }

    /*
     * Step 3 was removed on 7/8/05
     */

    /* trigger a volume state transition */
    flags.transition_pending = 1;

    return (0);
}

int reintvol::LastMLETime(unsigned long *time)
{
    if (CML.count() == 0)
        return (ENOENT);

    cmlent *lastmle = strbase(cmlent, CML.list.last(), handle);
    *time           = lastmle->time;

    return (0);
}

struct WriteLinksHook {
    VnodeId vnode;
    Unique_t vunique;
    const char *name;
    ino_t inode;
    uid_t uid;
    nlink_t nlink;
    time_t time;
    FILE *fp;
    int code;
};

static int WriteLinks(struct DirEntry *de, void *arg)
{
    VnodeId vnode;
    Unique_t vunique;
    char *name = de->name;
    const char *comp;
    size_t prefixlen;
    char namebuf[CODA_MAXPATHLEN];
    struct WriteLinksHook *hook = (struct WriteLinksHook *)arg;

    if (hook->code)
        return 0;

    FID_NFid2Int(&de->fid, &vnode, &vunique);
    if (vnode != hook->vnode || vunique != hook->vunique)
        return 0;

    comp = strrchr(hook->name, '/') + 1;
    CODA_ASSERT(comp != NULL);

    if (!!STREQ(comp, name))
        return 0;

    prefixlen = comp - hook->name;

    strncpy(namebuf, hook->name, prefixlen);
    strcpy(&namebuf[prefixlen], name);

    hook->code = archive_write_entry(hook->fp, hook->inode, 0100644, hook->uid,
                                     hook->nlink, hook->time, 0, namebuf,
                                     hook->name);
    return 0;
}

int cmlent::checkpoint(FILE *fp)
{
    /* counter to create unique inode numbers in the generated archive file */
    static int inode = 1;
    char name[CODA_MAXPATHLEN];
    size_t linklen;
    nlink_t nlink;
    fsobj *f;
    int err;

    /* make sure our 'inode numbers' stay within in the valid range for cpio
     * archives */
    if (archive_type == CPIO_ODC && inode >= 01000000)
        inode = 1;

    switch (opcode) {
    case CML_Store_OP: {
        /* Only checkpoint LAST store! */
        cml_iterator next(*(ClientModifyLog *)log, AbortOrder, &u.u_store.Fid);
        cmlent *m;
        while ((m = next()) && m->opcode != CML_Store_OP) /* loop */
            ;
        CODA_ASSERT(m);
        if (m != this)
            break;
    }

        f = FSDB->Find(&u.u_store.Fid);
        CODA_ASSERT(f);

        if (!HAVEALLDATA(f)) {
            eprint("can't checkpoint (%s), no data", FID_(&u.u_store.Fid));
            break;
        }

        f->GetPath(name);

        /* Are we going to add hard-link entries for alternate names? */
        nlink = f->pfso ? f->stat.LinkCount : 1;

        err = archive_write_entry(fp, inode, 0100644, uid, nlink, time,
                                  u.u_store.Length, name, NULL);
        if (err)
            return err;

        /* write out file contents */
        if (u.u_store.Length) {
            err = archive_write_data(fp, f->data.file->Name());
            if (err)
                return err;
        }

        if (nlink > 1) {
            struct WriteLinksHook hook;

            hook.vnode   = f->fid.Vnode;
            hook.vunique = f->fid.Unique;
            hook.name    = name;
            hook.inode   = inode;
            hook.uid     = uid;
            hook.time    = time;
            hook.fp      = fp;
            hook.code    = 0;

            DH_EnumerateDir(&f->pfso->data.dir->dh, WriteLinks, (void *)&hook);

            if (hook.code)
                return hook.code;
        }

        inode++;
        break;

    case CML_MakeDir_OP:
        f = FSDB->Find(&u.u_mkdir.CFid);
        CODA_ASSERT(f);

        f->GetPath(name);

        err =
            archive_write_entry(fp, inode, 040755, uid, 1, time, 0, name, NULL);
        if (err)
            return err;

        inode++;
        break;

    case CML_SymLink_OP:
        f = FSDB->Find(&u.u_symlink.CFid);
        CODA_ASSERT(f);

        f->GetPath(name);

        linklen = strlen((char *)Name);

        err = archive_write_entry(fp, inode, 0120777, uid, 1, time, linklen,
                                  name, (const char *)Name);
        if (err)
            return err;

        inode++;
        break;

    case CML_Repair_OP:
        eprint("Not checkpointing file (%s) that was repaired\n",
               FID_(&u.u_repair.Fid));
        break;

    default:
        break;
    }
    return 0;
}

static void BackupOldFile(const char *name)
{
    char oldname[MAXPATHLEN];

    CODA_ASSERT(strlen(name) < (MAXPATHLEN - 4));
    strcpy(oldname, name);
    strcat(oldname, ".old");

    ::rename(name, oldname);
}

/* Returns {0, ENOSPC}. */
int ClientModifyLog::CheckPoint(char *ckpdir)
{
    repvol *vol = strbase(repvol, this, CML);
    LOG(1, ("ClientModifyLog::CheckPoint: (%s), cdir = %s\n", vol->name,
            (ckpdir ? ckpdir : "")));

    int code = 0, n;

    /* the spool directory name */
    char spoolname[MAXPATHLEN], *volname;

    if (ckpdir)
        strcpy(spoolname, ckpdir);
    else
        MakeUserSpoolDir(spoolname, owner);

    strcat(spoolname, "/");

    n       = strlen(spoolname);
    volname = spoolname + n;

    /* The last component of the name will be "<realm>_<volname>". */
    n = snprintf(volname, MAXPATHLEN - n, "%s_%s", vol->realm->Name(),
                 vol->name);
    if (n < 0)
        return 0;

    /* remove characters with possibly unwanted side effects from the last
     * component */
    for (char *cp = volname; *cp; cp++)
        if (*cp == ':' || *cp == '/' || *cp == '|' || *cp == '@')
            *cp = '_';

    char ckpname[MAXPATHLEN], lname[MAXPATHLEN];
    /* append .tar/.cpio and .cml */
    strcpy(ckpname, spoolname);
    if (archive_type == TAR_TAR || archive_type == TAR_USTAR)
        strcat(ckpname, ".tar");
    else
        strcat(ckpname, ".cpio");

    strcpy(lname, spoolname);
    strcat(lname, ".cml");

    /* rename the old checkpoint file, if possible */
    BackupOldFile(ckpname);
    BackupOldFile(lname);

    FILE *dfp = NULL, *ofp = NULL;

    if ((dfp = fopen(ckpname, "w+")) == NULL) {
        eprint("Couldn't open %s for checkpointing", ckpname);
        return (ENOENT);
    }
#ifndef __CYGWIN32__
    ::fchown(fileno(dfp), owner, V_GID);
#else
    ::chown(ckpname, owner, V_GID);
#endif
    ::fchmod(fileno(dfp), 0600);

    if ((ofp = fopen(lname, "w+")) == NULL) {
        eprint("Couldn't open %s for checkpointing", lname);
        return (ENOENT);
    }
#ifndef __CYGWIN32__
    ::fchown(fileno(ofp), owner, V_GID);
#else
    ::chown(lname, owner, V_GID);
#endif
    ::fchmod(fileno(ofp), 0600);

    /*
     * Iterate through the MLEs (in commit order), checkpointing each in turn.
     * Lock the CML exclusively to prevent changes during checkpoint.  This is
     * necessary because the thread yields during file write.  If at the time
     * there is another thread doing mutations to the volume causing some of
     * the elements in the CML being iterated to be canceled, venus will
     * assertion fail.
     */
    ObtainWriteLock(&vol->CML_lock);
    eprint("Checkpointing %s to %s and %s", vol->name, ckpname, lname);
    cml_iterator next(*this, CommitOrder);
    cmlent *m;
    while ((m = next())) {
        m->writeops(ofp);
        if (code)
            continue;
        code = m->checkpoint(dfp);
    }
    if (code) {
        LOG(0, ("checkpointing of %s to %s failed (%d)", vol->name, ckpname,
                code));
        eprint("checkpointing of %s to %s failed (%d)", vol->name, ckpname,
               code);
    };
    ReleaseWriteLock(&vol->CML_lock);

    /* Write the trailer block and flush the data. */
    if (code == 0)
        code = archive_write_trailer(dfp);

    /* Close the CKP file. */
    fclose(dfp);
    fclose(ofp);

    /* Unlink in the event of any error. */
    if (code != 0) {
        ::unlink(ckpname);
        ::unlink(lname);
        eprint("Couldn't successfully checkpoint %s and %s", ckpname, lname);
    }

    return (code);
}

/* Invalidate the fsobj's following unsuccessful reintegration. */
/* MUST NOT be called from within transaction! */
void ClientModifyLog::IncAbort(int Tid)
{
    repvol *vol = strbase(repvol, this, CML);
    LOG(0, ("ClientModifyLog::IncAbort: (%s) and tid = %d\n", vol->name, Tid));
    /* eprint("IncAbort CML for %s and tid %d\n", vol->name, Tid); */

    CODA_ASSERT(count() > 0);

    Recov_BeginTrans();
    rec_dlist_iterator next(list, AbortOrder);
    rec_dlink *d = next(); /* get the first (last) element */

    while (1) {
        if (!d)
            break; /* list exhausted */
        cmlent *m = strbase(cmlent, d, handle);
        if (m->GetTid() == Tid) {
            m->print(GetLogFile());
            d = next(); /* advance d before it is un-listed by m->abort() */
            m->abort();
        } else {
            d = next();
        }
    }
    Recov_EndTrans(DMFP);
}

/* MUST be called from within transaction! */
void cmlent::abort()
{
    repvol *vol = strbase(repvol, log, CML);
    vol->RecordsAborted++;

    /* Step 1:  CODA_ASSERT that there are no edges emanating from this record. */
    CODA_ASSERT(succ == 0 || succ->count() == 0);

    /* Step 2:  Kill fsos linked into this record */
    dlist_iterator next(*fid_bindings);
    dlink *d;

    while ((d = next())) {
        binding *b = strbase(binding, d, binder_handle);
        fsobj *f   = (fsobj *)b->bindee;

        /* sanity checks */
        CODA_ASSERT(f &&
                    (f->MagicNumber == FSO_MagicNumber)); /* better be an fso */

        f->Lock(WR);
        f->Kill();

        FSDB->Put(&f);
    }

    delete this;
}

/*  *****  Routines for Maintaining fsobj <--> cmlent Bindings  *****  */

/* MUST be called from within transaction! */
void ClientModifyLog::AttachFidBindings()
{
    cml_iterator next(*this);
    cmlent *m;
    while ((m = next()))
        m->AttachFidBindings();
}

/* MUST be called from within transaction! */
void cmlent::AttachFidBindings()
{
    VenusFid *fids[3];
    GetAllFids(fids);

    for (int i = 0; i < 3; i++) {
        VenusFid *fidp = fids[i];
        if (fidp == 0)
            break;

        fsobj *f = FSDB->Find(fidp);
        if (f == 0) {
            print(GetLogFile());
            (strbase(reintvol, log, CML))->print(GetLogFile());
            CHOKE("cmlent::AttachFidBindings: can't find (%s)", FID_(fidp));
        }

        binding *b = new binding;
        b->binder  = this;
        if (fid_bindings == 0)
            fid_bindings = new dlist;
        fid_bindings->append(&b->binder_handle);
        f->AttachMleBinding(b);

        if (opcode == CML_Store_OP && IsFrozen())
            f->MakeShadow();
    }
}

void cmlent::DetachFidBindings()
{
    if (fid_bindings == 0)
        return;

    dlink *d;
    while ((d = fid_bindings->get())) {
        binding *b = strbase(binding, d, binder_handle);
        fsobj *f   = (fsobj *)b->bindee;
        f->DetachMleBinding(b);
        b->binder = 0;
        delete b;
    }
    delete fid_bindings;
    fid_bindings = 0;
}

void cmlent::getfids(VenusFid fid[3])
{
    fid[0] = fid[1] = fid[2] = NullFid;
    switch (opcode) {
    case CML_Store_OP:
        fid[0] = u.u_store.Fid;
        break;
    case CML_Utimes_OP:
        fid[0] = u.u_utimes.Fid;
        break;
    case CML_Chown_OP:
        fid[0] = u.u_chown.Fid;
        break;
    case CML_Chmod_OP:
        fid[0] = u.u_chmod.Fid;
        break;
    case CML_Create_OP:
        fid[0] = u.u_create.PFid;
        fid[1] = u.u_create.CFid;
        break;
    case CML_Remove_OP:
        fid[0] = u.u_remove.PFid;
        fid[1] = u.u_remove.CFid;
        break;
    case CML_Link_OP:
        fid[0] = u.u_link.PFid;
        fid[1] = u.u_link.CFid;
        break;
    case CML_Rename_OP:
        fid[0] = u.u_rename.SPFid;
        fid[1] = u.u_rename.TPFid;
        fid[2] = u.u_rename.SFid;
        break;
    case CML_MakeDir_OP:
        fid[0] = u.u_mkdir.PFid;
        fid[1] = u.u_mkdir.CFid;
        break;
    case CML_RemoveDir_OP:
        fid[0] = u.u_rmdir.PFid;
        fid[1] = u.u_rmdir.CFid;
        break;
    case CML_SymLink_OP:
        fid[0] = u.u_symlink.PFid;
        fid[1] = u.u_symlink.CFid;
        break;
    case CML_Repair_OP:
        fid[0] = u.u_repair.Fid;
        break;
    default:
        break;
    }
}

void cmlent::writeops(FILE *fp)
{
    char path[MAXPATHLEN], path2[MAXPATHLEN];
    char msg[2 * MAXPATHLEN + 100]; // this is enough for writing one entry

    switch (opcode) {
    case CML_Store_OP:
        RecoverPathName(path, &u.u_store.Fid, log, this);
        sprintf(msg, "Store \t%s (length = %d)", path, u.u_store.Length);
        break;

    case CML_Utimes_OP:
        RecoverPathName(path, &u.u_utimes.Fid, log, this);
        sprintf(msg, "Utime \t%s", path);
        break;

    case CML_Chown_OP:
        RecoverPathName(path, &u.u_chown.Fid, log, this);
        sprintf(msg, "Chown \t%s (owner = %d)", path, u.u_chown.Owner);
        break;

    case CML_Chmod_OP:
        RecoverPathName(path, &u.u_chmod.Fid, log, this);
        sprintf(msg, "Chmod \t%s (mode = %o)", path, u.u_chmod.Mode);
        break;

    case CML_Create_OP:
        RecoverPathName(path, &u.u_create.CFid, log, this);
        sprintf(msg, "Create \t%s", path);
        break;

    case CML_Remove_OP:
        RecoverPathName(path, &u.u_remove.CFid, log, this);
        sprintf(msg, "Remove \t%s", path);
        break;

    case CML_Link_OP:
        RecoverPathName(path, &u.u_link.CFid, log, this);
        sprintf(msg, "Link \t%s to %s", path, Name);
        break;

    case CML_Rename_OP:
        RecoverPathName(path, &u.u_rename.SPFid, log, this);
        RecoverPathName(path2, &u.u_rename.TPFid, log, this);
        sprintf(msg, "Rename \t%s/%s (to: %s/%s)", path, Name, path2, NewName);
        break;

    case CML_MakeDir_OP:
        RecoverPathName(path, &u.u_mkdir.CFid, log, this);
        sprintf(msg, "Mkdir \t%s", path);
        break;

    case CML_RemoveDir_OP:
        RecoverPathName(path, &u.u_rmdir.CFid, log, this);
        sprintf(msg, "Rmdir \t%s", path);
        break;

    case CML_SymLink_OP:
        RecoverPathName(path, &u.u_symlink.CFid, log, this);
        sprintf(msg, "Symlink %s (--> %s)", path, Name);
        break;

    case CML_Repair_OP:
        sprintf(msg, "Disconnected Repair by an ASR for %s",
                FID_(&u.u_repair.Fid));
        break;
    default:
        break;
    }

    fprintf(fp, "%s\n", msg);
}

/* this routine determines if a cmlent is old enough to reintegrate. */
int cmlent::Aged()
{
    time_t curTime = Vtime();
    repvol *vol    = strbase(repvol, log, CML);

    if (vol->IsSync())
        return 1;

    if ((curTime - time) >= vol->AgeLimit)
        return 1;

    return 0;
}

/*
 * simpleminded routine to estimate the amount of time to reintegrate
 * this record (in milleseconds), given an estimate of bandwidth in
 * bytes/second.
 */
unsigned long cmlent::ReintTime(unsigned long bw)
{
    double time = 0;

    if (bw > 0) {
        time = (double)size();
        if (opcode == CML_Store_OP)
            time += u.u_store.Length; /* might be large */

        time = time * 1000.0 / (double)bw;
    }

    LOG(10, ("cmlent::ReintTime: bandwidth = %d bytes/sec, time = %d msec\n",
             bw, (unsigned long)time));
    if (GetLogLevel() >= 10)
        print(GetLogFile());

    return ((unsigned long)time);
}

unsigned long cmlent::ReintAmount(unsigned long *reint_time)
{
    reintvol *vol = strbase(reintvol, log, CML);
    unsigned long amount, offset;
    unsigned long bw; /* bandwidth, in bytes/sec */

    CODA_ASSERT(opcode == CML_Store_OP);

    /*
     * try to get a dynamic bw estimate.  If that doesn't
     * work, fall back on the static estimate.
     */
    vol->GetBandwidth(&bw);
    bw /= 8; /* scaled to reduce overflows */

    //amount = (*reint_time / 1000) * bw;
    amount = (bw > 0) ? (*reint_time / 125) * bw : u.u_store.Length;

    offset = u.u_store.Offset;
    if (offset + amount > u.u_store.Length)
        amount = u.u_store.Length - offset;

    //*reint_time -= (amount / bw) * 1000;
    *reint_time -= (bw > 0) ? (amount * 125) / bw : 0;
    return amount;
}

/* reintegrating --> frozen */
int cmlent::IsReintegrating()
{
    repvol *vol = strbase(repvol, log, CML);

    if (vol->flags.reintegrating && IsFrozen() && (tid != UNSET_TID) &&
        (tid == vol->cur_reint_tid))
        return 1;

    return 0;
}

/*  *****  Modify Log Iterator  *****  */

/*
 *    1. This implementation assumes that a dlist_iterator can correctly iterate over a rec_dlist as well as a dlist!
 *    2. Iterating over records referencing a particular fid is grossly inefficient and needs to be improved!
 *    3. Iterating starting after a prelude is inefficient and needs to be improved (by augmenting dlist_iterator)!
 */

cml_iterator::cml_iterator(ClientModifyLog &Log, CmlIterOrder Order,
                           const VenusFid *Fid, cmlent *Prelude)
{
    log      = &Log;
    order    = Order;
    fidp     = Fid;
    next     = 0;
    rec_next = 0;
    if (fidp == 0) {
        rec_next = new rec_dlist_iterator(log->list, order);
    } else {
        fid      = *Fid;
        fsobj *f = FSDB->Find(&fid);
        if (f == 0) {
            CHOKE("cml_iterator::cml_iterator: can't find (%s)", FID_(&fid));
        }
        if (f->mle_bindings)
            next = new dlist_iterator(*f->mle_bindings, order);
    }

    /* Skip over prelude. */
    prelude = Prelude;
    if (prelude != 0) {
        cmlent *m;
        while ((m = (*this)()) && m != prelude)
            ;
        CODA_ASSERT(m != 0);
    }
}

cml_iterator::~cml_iterator()
{
    if (next != 0)
        delete next;
    if (rec_next != 0)
        delete rec_next;
}

cmlent *cml_iterator::operator()()
{
    for (;;) {
        if (rec_next) {
            rec_dlink *d = (*rec_next)();
            if (d == 0)
                return (0);
            cmlent *m = strbase(cmlent, d, handle);
            return (m);
        } else {
            if (next == 0)
                return (0);
            dlink *d = (*next)();
            if (d == 0)
                return (0);
            binding *b = strbase(binding, d, bindee_handle);
            cmlent *m  = (cmlent *)b->binder;
            switch (m->opcode) {
            case CML_Store_OP:
                if (FID_EQ(&m->u.u_store.Fid, &fid))
                    return (m);
                break;

            case CML_Utimes_OP:
                if (FID_EQ(&m->u.u_utimes.Fid, &fid))
                    return (m);
                break;

            case CML_Chown_OP:
                if (FID_EQ(&m->u.u_chown.Fid, &fid))
                    return (m);
                break;

            case CML_Chmod_OP:
                if (FID_EQ(&m->u.u_chmod.Fid, &fid))
                    return (m);
                break;

            case CML_Create_OP:
                if (FID_EQ(&m->u.u_create.PFid, &fid))
                    return (m);
                if (FID_EQ(&m->u.u_create.CFid, &fid))
                    return (m);
                break;

            case CML_Remove_OP:
                if (FID_EQ(&m->u.u_remove.PFid, &fid))
                    return (m);
                if (FID_EQ(&m->u.u_remove.CFid, &fid))
                    return (m);
                break;

            case CML_Link_OP:
                if (FID_EQ(&m->u.u_link.PFid, &fid))
                    return (m);
                if (FID_EQ(&m->u.u_link.CFid, &fid))
                    return (m);
                break;

            case CML_Rename_OP:
                if (FID_EQ(&m->u.u_rename.SPFid, &fid))
                    return (m);
                if (FID_EQ(&m->u.u_rename.TPFid, &fid))
                    return (m);
                if (FID_EQ(&m->u.u_rename.SFid, &fid))
                    return (m);
                break;

            case CML_MakeDir_OP:
                if (FID_EQ(&m->u.u_mkdir.PFid, &fid))
                    return (m);
                if (FID_EQ(&m->u.u_mkdir.CFid, &fid))
                    return (m);
                break;

            case CML_RemoveDir_OP:
                if (FID_EQ(&m->u.u_rmdir.PFid, &fid))
                    return (m);
                if (FID_EQ(&m->u.u_rmdir.CFid, &fid))
                    return (m);
                break;

            case CML_SymLink_OP:
                if (FID_EQ(&m->u.u_symlink.PFid, &fid))
                    return (m);
                if (FID_EQ(&m->u.u_symlink.CFid, &fid))
                    return (m);
                break;

            case CML_Repair_OP:
                if (FID_EQ(&m->u.u_repair.Fid, &fid))
                    return (m);
                break;

            default:
                CODA_ASSERT(0);
            }
        }
    }
}

/*
 * Unmark all cmlent's to_be_repaired flag. Useful after DoRepair.
 */
void ClientModifyLog::ClearToBeRepaired(void)
{
    cmlent *m;
    cml_iterator next(*this, CommitOrder);
    int num = 0;

    while ((m = next()))
        if (m->flags.to_be_repaired) {
            Recov_BeginTrans();
            RVMLIB_REC_OBJECT(m->flags);
            m->flags.to_be_repaired = 0;
            Recov_EndTrans(MAXFP);
            num++;
        }
    LOG(0, ("ClientModifyLog::ClearRepairFlags: cleared %d entries\n", num));
}
