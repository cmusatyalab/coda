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
 *    Implementation of Venus' Client Modify Log.
 *
 */


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <stdarg.h>
#include <struct.h>

#include <unistd.h>
#include <stdlib.h>

#include <netinet/in.h>
#ifdef __BSD44__
#include <machine/endian.h>
#endif

#include <lock.h>
#include <rpc2.h>
#include <se.h>
#include <multi.h>

/* from dir */
#include <codadir.h>

extern int get_len(ARG **, PARM **, MODE);
extern int struct_len(ARG **, PARM **);
extern void pack(ARG *, PARM **, PARM **);
extern void pack_struct(ARG *, PARM **, PARM **);
/* interfaces */
#include <vice.h>	
#include <cml.h>	

#ifdef __cplusplus
}
#endif __cplusplus

/* from util */
#include <dlist.h>

/* from venus */
#include "advice.h"
#include "comm.h"
#include "fso.h"
#include "local.h"
#include "mariner.h"
#include "venus.private.h"
#include "venuscb.h"
#include "venusvol.h"
#include "vproc.h"


static int RLE_Size(ARG * ...);
static void RLE_Pack(PARM **, ARG * ...);

int LogOpts = 1;	/* perform log optimizations? */

/*  *****  Client Modify Log Basic Routines  *****  */

void ClientModifyLog::ResetTransient() {
    owner = UNSET_UID;
    entries = count();
    entriesHighWater = entries;
    bytes = _bytes();
    bytesHighWater = bytes;

    if (count() > 0) {
	/* Schedule a transition (to Reintegrating or Emulating) if log is non-empty. */
	volent *vol = strbase(volent, this, CML);
	vol->flags.transition_pending = 1;

	/* Set owner. */
	cml_iterator next(*this, CommitOrder);
	cmlent *m;
	while ((m = next())) {
	    if (owner == UNSET_UID) {
		owner = (vuid_t) m->uid;
	    }
	    else {
		CODA_ASSERT(owner == m->uid);
	    }

	    m->ResetTransient();
	}
	CODA_ASSERT(owner != UNSET_UID);
    }
}


/* MUST be called from within transaction! */
void ClientModifyLog::Clear() {
    rec_dlink *d;

    while ((d = list.first()))
	delete strbase(cmlent, d, handle);
}


long ClientModifyLog::_bytes() {
    cml_iterator next(*this);
    cmlent *e;
    long result = 0;

    while ((e = next()))
	result += e->bytes();

    return result;
}


void ClientModifyLog::IncGetStats(cmlstats& current, cmlstats& cancelled, int tid) {
    /* First, compute current statistics. */
    cml_iterator next(*this, CommitOrder);
    cmlent *m;
    while ((m = next())) {
	if (tid != UNSET_TID && m->GetTid() != tid) continue;
	if (m->opcode == OLDCML_NewStore_OP) {
	    current.store_count++;
	    current.store_size += m->bytes();
	    current.store_contents_size += (float)m->u.u_store.Length;
	}
	else {
	    current.other_count++;
	    current.other_size += m->bytes();
	}
    }

    cancelled = cancellations;
}


#define INITFIDLISTLENGTH 50

/* 
 * lock objects in the CML corresponding to store records.
 * objects must be locked in fid order.
 */
void ClientModifyLog::LockObjs(int tid) {
    int i;
    volent *vol = strbase(volent, this, CML);
    LOG(1, ("ClientModifyLog::LockObjs: (%s) and tid = %d\n", vol->name, tid));

    int max = INITFIDLISTLENGTH;
    ViceFid *backFetchList;
    int nFids = 0;

    /* get some space for the list.  will extend later if needed. */
    backFetchList = (ViceFid *) malloc(sizeof(ViceFid) * max);
    bzero((char *) backFetchList, (int) sizeof(ViceFid) * max);

    cml_iterator next(*this);
    cmlent *m;
    while ((m = next())) 
	if (m->GetTid() == tid) 
	    if (m->opcode == OLDCML_NewStore_OP) {
		backFetchList[nFids++] = m->u.u_store.Fid;

                if (nFids == max) {  /* get more space */
                    /* I am terrified of realloc. */
                    max += INITFIDLISTLENGTH;
                    ViceFid *newBFList = (ViceFid *) malloc(sizeof(ViceFid) * max);
                    bcopy((char *) backFetchList, (char *) newBFList, 
			  (int) sizeof(ViceFid) * nFids);
                    free(backFetchList);
                    backFetchList = newBFList;
                }
	    }

    /* now sort them. we must lock in this order */
    (void) qsort((char *) backFetchList, nFids, sizeof(ViceFid), 
		 (int (*)(const void *, const void *))Fid_Compare);

    /* remove duplicates */
    for (i = 0; i < nFids; i++) 
	for (int j = i+1; j < nFids; j++) 
	    if (Fid_Compare(&backFetchList[i], &backFetchList[j]) || 
		FID_EQ(&backFetchList[i], &NullFid))
		break;  /* no match or null */
	    else /* they match -- zero out duplicate */
		backFetchList[j] = NullFid;

    /* find and lock objects corresponding to non-null fids */
    for (i = 0; i < nFids; i++) 
	if (!FID_EQ(&backFetchList[i], &NullFid)) {
	    LOG(100, ("ClientModifyLog::LockObjs: Locking %x.%x.%x\n",
		    backFetchList[i].Volume, backFetchList[i].Vnode, backFetchList[i].Unique));

	    fsobj *f = FSDB->Find(&backFetchList[i]);
	    if (f == NULL)
		CHOKE("ClientModifyLog::LockObjs: Object in CML not cached! (%x.%x.%x)\n",
		      backFetchList[i].Volume, backFetchList[i].Vnode, backFetchList[i].Unique);
	    f->Lock(RD);
        }

    free(backFetchList);
}


/* Unlock objects that were referenced in STORE records */
void ClientModifyLog::UnLockObjs(int tid) {
    volent *vol = strbase(volent, this, CML);
    LOG(1, ("ClientModifyLog::UnLockObjs: (%s) and tid = %d\n", vol->name, tid));

    /* 
     * Because CML records can be frozen, there may be multiple
     * store records for an object.  The cancellation_pending 
     * field is not a reliable indicator because of early store 
     * cancellation.  To unlock only once, find the first store
     * store record for the object, and unlock only if the record 
     * in hand is that one.
     */
    cml_iterator next(*this);
    cmlent *m;
    while ((m = next())) 
	if ((m->GetTid() == tid) && (m->opcode == OLDCML_NewStore_OP)) {
	    /* get the fso */
	    dlink *d = m->fid_bindings->first();   /* and only */
	    binding *b = strbase(binding, d, binder_handle);
	    fsobj *f = (fsobj *)b->bindee;
	    CODA_ASSERT(f && (f->MagicNumber == FSO_MagicNumber));  /* better be an fso */

	    /* check the other mle bindings for the fso */
	    dlist_iterator next(*f->mle_bindings);
	    dlink *fd;
	    cmlent *store_mle = 0;

	    while((fd = next())) {
		binding *fb = strbase(binding, fd, bindee_handle);
		cmlent *fm = (cmlent *)fb->binder;
		if ((fm->GetTid() == tid) && (fm->opcode == OLDCML_NewStore_OP)) {
		    store_mle = fm;	/* first store record for this fso */
		    break;	
		}
	    }
	    if (m == store_mle)
		m->UnLockObj();
	}
}


/* lock object of a store */
void cmlent::LockObj() {
    CODA_ASSERT(opcode == OLDCML_NewStore_OP);

    /* make sure there is only one object of the store */    
    CODA_ASSERT(fid_bindings->count() == 1); 

    dlink *d = fid_bindings->first();   /* and only */
    binding *b = strbase(binding, d, binder_handle);
    fsobj *f = (fsobj *)b->bindee;

    /* sanity checks */
    CODA_ASSERT(f && (f->MagicNumber == FSO_MagicNumber));  /* better be an fso */

    f->Lock(RD);
}


/* unlock object of a store */
void cmlent::UnLockObj() {
    CODA_ASSERT(opcode == OLDCML_NewStore_OP);

    /* make sure there is only one object of the store */    
    CODA_ASSERT(fid_bindings->count() == 1); 

    dlink *d = fid_bindings->first();   /* and only */
    binding *b = strbase(binding, d, binder_handle);
    fsobj *f = (fsobj *)b->bindee;

    /* sanity checks */
    CODA_ASSERT(f && (f->MagicNumber == FSO_MagicNumber));  /* better be an fso */

    if (f->shadow) {
        /* no need to unlock, just get rid of shadow copy */
        f->RemoveShadow();
	/* 
	 * if this record was being partially reintegrated, but was
	 * not sent completely, then the partial results are invalid.
	 */
	if (HaveReintegrationHandle() && !DoneSending())
	    ClearReintegrationHandle();		/* start over! */

    } else {
        LOG(100, ("cmlent::UnlockObj: Unlocking %x.%x.%x\n",
	    f->fid.Volume, f->fid.Vnode, f->fid.Unique));
        f->UnLock(RD);
    }
}


/* 
 * called after a reintegration failure to remove cmlents that would
 * have been cancelled had reintegration not been in progress.
 * Unfreezes records; cancel requires this.  Since this routine is
 * called only if the failure involved receiving a response from the
 * server (i.e., outcome is known), it is safe to unfreeze the records.  
 */
void ClientModifyLog::CancelPending() {
    Recov_BeginTrans();
    int cancellation;
    do {
	    cancellation = 0;
	    cml_iterator next(*this);
	    cmlent *m;

	    while ((m = next())) {
		    if (m->flags.frozen) {
			    RVMLIB_REC_OBJECT(m->flags);
			    m->flags.frozen = 0;
		    }
		    if (m->flags.cancellation_pending) {
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
void ClientModifyLog::ClearPending() {
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
void ClientModifyLog::CancelStores() {
    cmlent *m, *n;
    cml_iterator next(*this, CommitOrder);

    m = next(); n = next();
    while (m) {
	m->cancelstore();
	m = n; 
	n = next();
    }
}


/* 
 * Scan the log for reintegrateable records, subject to the
 * reintegration time limit, and mark them with the given
 * tid. Note the time limit does not apply to ASRs.
 * The routine returns the number of records marked.
 */
void ClientModifyLog::GetReintegrateable(int tid, int *nrecs) {
    volent *vol = strbase(volent, this, CML);
    cmlent *m;
    cml_iterator next(*this, CommitOrder);
    unsigned long cur_reintegration_time = 0, this_time;
    unsigned long bw; /* bandwidth in bytes/sec */

    /* get the current bandwidth estimate */
    vol->vsg->GetBandwidth(&bw);

    while ((m = next())) {
	if (!m->ReintReady())
	    break;    

	this_time = m->ReintTime(bw);

	/* Only limit on reintegration time if the logv flag is set.
	 * otherwise we are trying get back to connected state. --JH */
	if (!ASRinProgress && vol->flags.logv &&
	    (this_time + cur_reintegration_time > vol->ReintLimit)) 
	    break;

	/* 
	 * freeze the record to prevent cancellation.  Note that
	 * reintegrating --> frozen, but the converse is not true.
	 * Records are frozen until the outcome of a reintegration
	 * is known; this may span multiple reintegration attempts
	 * and different transactions.
	 */
	Recov_BeginTrans();
	       RVMLIB_REC_OBJECT(m->flags);
	       m->flags.frozen = 1;
	Recov_EndTrans(MAXFP);

	/* 
	 * don't use the settid call because it is transactional.
	 * Here the tid is transient.
	 */
	m->tid = tid;    
	cur_reintegration_time += this_time;

	/*
	 * By sending records in blocks of 100 CMLentries, we avoid
	 * overloading the server. JH
	 */
	if (++(*nrecs) == 100)
	    break;
    }

    LOG(0, ("ClientModifyLog::GetReintegrateable: (%s, %d) %d records, %d msec\n", 
	vol->name, tid, *nrecs, cur_reintegration_time));
}


/* 
 * check if there is a fat store blocking the head of the log.
 * if there is, mark it with the tid and return a pointer to it.
 * Note with a less pretty interface this could be rolled into
 * the routine above.
 */
cmlent *ClientModifyLog::GetFatHead(int tid) {
    volent *vol = strbase(volent, this, CML);
    cmlent *m;
    cml_iterator next(*this, CommitOrder);
    unsigned long bw; /* bandwidth in bytes/sec */

    /* get the current bandwidth estimate */
    vol->vsg->GetBandwidth(&bw);

    /* Get the first entry in the CML */
    m = next();

    /* The head of the CML must exists, be a store operation and ready
     * for reintegration. */
    if (ASRinProgress || !m ||
        (m->opcode != OLDCML_NewStore_OP) ||
        !m->ReintReady())
        return((cmlent *)0);


    /* If we already have a reintegration handle, or if the reintegration time
     * exceeds the limit, we need to do a partial reintegration of the store. */
    if (m->HaveReintegrationHandle() || m->ReintTime(bw) > vol->ReintLimit) {
        /*
         * Don't use the settid call because it is transactional.
         * Here the tid is transient.
         * We also do not mark it frozen here because the partial file
         * transfer protocol checks the status of the object after a failure.
         */
        m->tid = tid;
        return(m);
    }

    return((cmlent *)0);
}


/*
 * Mark the offending mle designated by the index.
 * If the index is -1, something really bad happened.
 * in that case mark 'em all.
 */
void ClientModifyLog::MarkFailedMLE(int ix) {
    volent *vol = strbase(volent, this, CML);
    int i = 0;

    cml_iterator next(*this);
    cmlent *m;
    while ((m = next())) 
	if (m->tid == vol->cur_reint_tid) 
	    if (i++ == ix || ix == -1)
		m->flags.failed = 1;
}


/*
 * Mark the record with the matching storeid-uniquifier
 * as already having been committed at the server.
 */
void ClientModifyLog::MarkCommittedMLE(RPC2_Unsigned Uniquifier) {
    volent *vol = strbase(volent, this, CML);

    cml_iterator next(*this);
    cmlent *m;
    while ((m = next())) 
	if (m->tid == vol->cur_reint_tid && 
	    m->sid.Uniquifier == Uniquifier)
	    m->flags.committed = 1;
}

/*
  BEGIN_HTML
  <a name="failedmle"><strong> the entry point of handling reintegration failure or local-global conflicts </strong></a>
  END_HTML
*/
/* 
 * Handle a non-retryable failure.  The offending record
 * was marked and may or may not still be there. 
 * Note that an abort may delete a record out from under us.
 */
void ClientModifyLog::HandleFailedMLE() {
    volent *vol = strbase(volent, this, CML);
    cmlent *m, *n;
    cml_iterator next(*this, CommitOrder);

    m = next(); n = next();
    while (m) {
	if (m->flags.failed) {
	    m->flags.failed = 0;	/* only do this once */

	    /* 
	     * this record may already have been localized because of
	     * a cascading failure, i.e. a retry finding another
	     * failure earlier in the log.
	     */
	    if (m->ContainLocalFid())
		continue;

	    /* localize or abort */
	    if ((m->LocalFakeify() != 0) && (!m->IsToBeRepaired())) {
		    Recov_BeginTrans();			       
		    m->abort();
		    Recov_EndTrans(MAXFP);
	    } else {
		    Recov_BeginTrans();
		    RVMLIB_REC_OBJECT(vol->flags);
		    vol->flags.has_local_subtree = 1;
		Recov_EndTrans(MAXFP);

		/* tell the user where the localized object is */
		LRDB->CheckLocalSubtree();
	    }
	}
	m = n;
	n = next();
    }
}


void ClientModifyLog::print(int fd) {
    fdprint(fd, "\tClientModifyLog: owner = %d, count = %d\n",
	     owner, count());

    cmlstats current, cancelled;
    IncGetStats(current, cancelled);
    fdprint(fd, "\t  current stats: %4d  %10.1f  %10.1f  %4d  %10.1f\n",
	    current.store_count, current.store_size / 1024.0,
	    current.store_contents_size / 1024.0,
	    current.other_count, current.other_size / 1024.0);
    fdprint(fd, "\tcancelled stats: %4d  %10.1f  %10.1f  %4d  %10.1f\n",
	    cancelled.store_count, cancelled.store_size / 1024.0,
	    cancelled.store_contents_size / 1024.0,
	    cancelled.other_count, cancelled.other_size / 1024.0);

    cml_iterator next(*this, CommitOrder);
    cmlent *m;
    while ((m = next()))
	m->print(fd);
}


/* MUST be called from within transaction! */
void *cmlent::operator new(size_t len) {
    cmlent *c = 0;

    LOG(1, ("cmlent::operator new()\n"));

    CODA_ASSERT(VDB->AllocatedMLEs < VDB->MaxMLEs);

    /* Get entry from free list or heap */
    if (VDB->mlefreelist.count() > 0)
	c = strbase(cmlent, VDB->mlefreelist.get(), handle);
    else
	c = (cmlent *)rvmlib_rec_malloc((int) len);
    CODA_ASSERT(c);

    /* Do bookkeeping */
    RVMLIB_REC_OBJECT(VDB->AllocatedMLEs);
    VDB->AllocatedMLEs++;

    return(c);
}


/* MUST be called from within transaction! */
cmlent::cmlent(ClientModifyLog *Log, time_t Mtime, vuid_t vuid, int op, int Tid ...) {

    LOG(1, ("cmlent::cmlent(...)\n"));
    RVMLIB_REC_OBJECT(*this);

    log = Log;
    this->tid = Tid;
    flags.to_be_repaired = 0;
    flags.repair_mutation = 0;
    flags.frozen = 0;
    flags.cancellation_pending = 0;
    log->list.append(&handle);

    volent *vol = strbase(volent, log, CML);
    sid = vol->GenerateStoreId();
    time = Mtime;
    uid = vuid;

    opcode = op;
    va_list ap;
    va_start(ap, Tid);
    switch(op) {
	case OLDCML_NewStore_OP:
	    u.u_store.Fid = *va_arg(ap, ViceFid *);
	    u.u_store.Length = va_arg(ap, RPC2_Unsigned);
	    bzero((void *)u.u_store.RHandle, (int) sizeof(ViceReintHandle)*VSG_MEMBERS);
	    u.u_store.Offset = -1;
	    break;

	case OLDCML_Utimes_OP:
	    u.u_utimes.Fid = *va_arg(ap, ViceFid *);
	    u.u_utimes.Date = va_arg(ap, Date_t);
	    break;

	case OLDCML_Chown_OP:
	    u.u_chown.Fid = *va_arg(ap, ViceFid *);
	    u.u_chown.Owner = va_arg(ap, UserId);
	    break;

	case OLDCML_Chmod_OP:
	    u.u_chmod.Fid = *va_arg(ap, ViceFid *);
	    u.u_chmod.Mode = va_arg(ap, RPC2_Unsigned);
	    break;

	case OLDCML_Create_OP:
	    u.u_create.PFid = *va_arg(ap, ViceFid *);
	    u.u_create.Name = Copy_RPC2_String(va_arg(ap, RPC2_String));
	    u.u_create.CFid = *va_arg(ap, ViceFid *);
	    u.u_create.Mode = va_arg(ap, RPC2_Unsigned);
	    break;

	case OLDCML_Remove_OP:
	    u.u_remove.PFid = *va_arg(ap, ViceFid *);
	    u.u_remove.Name = Copy_RPC2_String(va_arg(ap, RPC2_String));
	    u.u_remove.CFid = *va_arg(ap, ViceFid *);
	    u.u_remove.LinkCount = va_arg(ap, int);
	    break;

	case OLDCML_Link_OP:
	    u.u_link.PFid = *va_arg(ap, ViceFid *);
	    u.u_link.Name = Copy_RPC2_String(va_arg(ap, RPC2_String));
	    u.u_link.CFid = *va_arg(ap, ViceFid *);
	    break;

	case OLDCML_Rename_OP:
	    u.u_rename.SPFid = *va_arg(ap, ViceFid *);
	    u.u_rename.OldName = Copy_RPC2_String(va_arg(ap, RPC2_String));
	    u.u_rename.TPFid = *va_arg(ap, ViceFid *);
	    u.u_rename.NewName = Copy_RPC2_String(va_arg(ap, RPC2_String));
	    u.u_rename.SFid = *va_arg(ap, ViceFid *);
	    break;

	case OLDCML_MakeDir_OP:
	    u.u_mkdir.PFid = *va_arg(ap, ViceFid *);
	    u.u_mkdir.Name = Copy_RPC2_String(va_arg(ap, RPC2_String));
	    u.u_mkdir.CFid = *va_arg(ap, ViceFid *);
	    u.u_mkdir.Mode = va_arg(ap, RPC2_Unsigned);
	    break;

	case OLDCML_RemoveDir_OP:
	    u.u_rmdir.PFid = *va_arg(ap, ViceFid *);
	    u.u_rmdir.Name = Copy_RPC2_String(va_arg(ap, RPC2_String));
	    u.u_rmdir.CFid = *va_arg(ap, ViceFid *);
	    break;

	case OLDCML_SymLink_OP:
	    u.u_symlink.PFid = *va_arg(ap, ViceFid *);
	    u.u_symlink.NewName = Copy_RPC2_String(va_arg(ap, RPC2_String));
	    u.u_symlink.OldName = Copy_RPC2_String(va_arg(ap, RPC2_String));
	    u.u_symlink.CFid = *va_arg(ap, ViceFid *);
	    u.u_symlink.Mode = va_arg(ap, RPC2_Unsigned);
	    break;

	case OLDCML_Repair_OP:
	    u.u_repair.Fid = *va_arg(ap, ViceFid *);
	    u.u_repair.Length = va_arg(ap, RPC2_Unsigned);
	    u.u_repair.Date = va_arg(ap, Date_t);
	    u.u_repair.Owner = va_arg(ap, UserId);
	    u.u_repair.Mode = va_arg(ap, RPC2_Unsigned);
	    break;

	default:
	    print(logFile);
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

    if (Tid == LRDB->repair_session_tid) {
	/* this mutation is done for the current local-global repair session */
	flags.repair_mutation = 1;
    }

    LOG(1, ("cmlent::cmlent: tid = (%x.%d), uid = %d, op = %s\n",
	    sid.Host, sid.Uniquifier, uid, PRINT_MLETYPE(op)));
}


void cmlent::ResetTransient() {
    fid_bindings = 0;

    pred = 0;
    succ = 0;
    
    flags.failed = 0;
    flags.committed = 0;

    switch(opcode) {
	case OLDCML_NewStore_OP:
	    u.u_store.VV = NullVV;
	    break;

        case OLDCML_Truncate_OP:
	    u.u_truncate.VV = NullVV;
	    break;

	case OLDCML_Utimes_OP:
	    u.u_utimes.VV = NullVV;
	    break;

	case OLDCML_Chown_OP:
	    u.u_chown.VV = NullVV;
	    break;

	case OLDCML_Chmod_OP:
	    u.u_chmod.VV = NullVV;
	    break;

	case OLDCML_Create_OP:
	    u.u_create.PVV = NullVV;
	    break;

	case OLDCML_Remove_OP:
	    u.u_remove.PVV = NullVV;
	    u.u_remove.CVV = NullVV;
	    break;

	case OLDCML_Link_OP:
	    u.u_link.PVV = NullVV;
	    u.u_link.CVV = NullVV;
	    break;

	case OLDCML_Rename_OP:
	    u.u_rename.SPVV = NullVV;
	    u.u_rename.TPVV = NullVV;
	    u.u_rename.SVV = NullVV;
	    break;

	case OLDCML_MakeDir_OP:
	    u.u_mkdir.PVV = NullVV;
	    break;

	case OLDCML_RemoveDir_OP:
	    u.u_rmdir.PVV = NullVV;
	    u.u_rmdir.CVV = NullVV;
	    break;

	case OLDCML_SymLink_OP:
	    u.u_symlink.PVV = NullVV;
	    break;

        case OLDCML_Repair_OP:
	    u.u_repair.OVV = NullVV;
	    break;

	default:
	    print(logFile);
	    CHOKE("cmlent::ResetTransient: bogus opcode (%d)", opcode);
    }
}


/* MUST be called from within transaction! */
cmlent::~cmlent() {
    LOG(1, ("cmlent::~cmlent: tid = (%x.%d), uid = %d, op = %s\n",
	     sid.Host, sid.Uniquifier, uid, PRINT_MLETYPE(opcode)));

    RVMLIB_REC_OBJECT(*this);
    long thisBytes = bytes();

    /* Detach from fsobj's. */
    DetachFidBindings();

    /* Free strings. */
    switch(opcode) {
	case OLDCML_NewStore_OP:
	case OLDCML_Utimes_OP:
	case OLDCML_Chown_OP:
	case OLDCML_Chmod_OP:
	    break;

	case OLDCML_Create_OP:
	    Free_RPC2_String(u.u_create.Name);
	    break;

	case OLDCML_Remove_OP:
	    Free_RPC2_String(u.u_remove.Name);
	    break;

	case OLDCML_Link_OP:
	    Free_RPC2_String(u.u_link.Name);
	    break;

	case OLDCML_Rename_OP:
	    Free_RPC2_String(u.u_rename.OldName);
	    Free_RPC2_String(u.u_rename.NewName);
	    break;

	case OLDCML_MakeDir_OP:
	    Free_RPC2_String(u.u_mkdir.Name);
	    break;

	case OLDCML_RemoveDir_OP:
	    Free_RPC2_String(u.u_rmdir.Name);
	    break;

	case OLDCML_SymLink_OP:
	    Free_RPC2_String(u.u_symlink.OldName);
	    Free_RPC2_String(u.u_symlink.NewName);
	    break;

        case OLDCML_Repair_OP:
	    break;

	default:
	    print(logFile);
	    CHOKE("cmlent::~cmlent: bogus opcode (%d)", opcode);
    }

    CODA_ASSERT(log->list.remove(&handle) == &handle);
    /* update CML statistics */
    log->entries--;
    log->bytes -= thisBytes;

    log = 0;

}

/* MUST be called from within transaction! */
void cmlent::operator delete(void *deadobj, size_t len) {
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

long cmlent::bytes() {
    long result = sizeof(*this);
    switch (opcode) {
    case OLDCML_NewStore_OP:
    case OLDCML_Utimes_OP:
    case OLDCML_Chown_OP:
    case OLDCML_Chmod_OP:
    case OLDCML_Repair_OP:
	break;
    case OLDCML_Create_OP:
	result += strlen((char *)u.u_create.Name);
	break;
    case OLDCML_Remove_OP:
	result += strlen((char *)u.u_remove.Name);
	break;
    case OLDCML_Link_OP:
	result += strlen((char *)u.u_link.Name);
	break;
    case OLDCML_Rename_OP:
	result += strlen((char *)u.u_rename.OldName)
                 +strlen((char *)u.u_rename.NewName);
	break;
    case OLDCML_MakeDir_OP:
	result += strlen((char *)u.u_mkdir.Name);
	break;
    case OLDCML_RemoveDir_OP:
	result += strlen((char *)u.u_rmdir.Name);
	break;
    case OLDCML_SymLink_OP:
	result += strlen((char *)u.u_symlink.OldName)
	         +strlen((char *)u.u_symlink.NewName);
	break;
    default:
	CODA_ASSERT(0);
    }
    return result;
}


#define	PRINTFID(fd, fid)\
    fdprint((fd), "(%x.%x.%x)", (fid).Volume, (fid).Vnode, (fid).Unique);
#define	PRINTVV(fd, vv)\
    fdprint((fd), "[ %d %d %d %d %d %d %d %d ] [ %d %d ] [ %#x ]",\
	(vv).Versions.Site0, (vv).Versions.Site1,\
	(vv).Versions.Site2, (vv).Versions.Site3,\
	(vv).Versions.Site4, (vv).Versions.Site5,\
	(vv).Versions.Site6, (vv).Versions.Site7,\
	(vv).StoreId.Host, (vv).StoreId.Uniquifier, (vv).Flags);


/* local-repair modification */
void cmlent::print(int afd) {
    fdprint(afd, "\t%s : sid = (%x.%d), time = %d, uid = %d tid = %d bytes = %d\n",
	     PRINT_MLETYPE(opcode), sid.Host, sid.Uniquifier, time, uid, tid, bytes());
    fdprint(afd, "\t\tpred = (%x, %d), succ = (%x, %d)\n",
	     pred, (pred == 0 ? 0 : pred->count()),
	     succ, (succ == 0 ? 0 : succ->count()));
    fdprint(afd, "\t\tto_be_repaired = %d\n", flags.to_be_repaired);
    fdprint(afd, "\t\trepair_mutation = %d\n", flags.repair_mutation);
    fdprint(afd, "\t\tfrozen = %d, cancel = %d, failed = %d, committed = %d\n", 
	    flags.frozen, flags.cancellation_pending, flags.failed,
	    flags.committed);
    switch(opcode) {
	case OLDCML_NewStore_OP:
	    fdprint(afd, "\t\tfid = ");
	    PRINTFID(afd, u.u_store.Fid);
	    fdprint(afd, ", length = %d\n", u.u_store.Length);
	    fdprint(afd, "\t\tvv = ");
	    PRINTVV(afd, u.u_store.VV);
	    for (int i = 0; i < VSG_MEMBERS; i++) 
		fdprint(afd, "\n\t\trhandle[%d] = (%d,%d,%d)", i,
			u.u_store.RHandle[i].BirthTime,
			u.u_store.RHandle[i].Device,
			u.u_store.RHandle[i].Inode);
	    fdprint(afd, "\n");
	    break;

	case OLDCML_Utimes_OP:
	    fdprint(afd, "\t\tfid = ");
	    PRINTFID(afd, u.u_utimes.Fid);
	    fdprint(afd, ", utimes = %d\n", u.u_utimes.Date);
	    fdprint(afd, "\t\tvv = ");
	    PRINTVV(afd, u.u_utimes.VV);
	    fdprint(afd, "\n");
	    break;

	case OLDCML_Chown_OP:
	    fdprint(afd, "\t\tfid = ");
	    PRINTFID(afd, u.u_chown.Fid);
	    fdprint(afd, ", chown = %d\n", u.u_chown.Owner);
	    fdprint(afd, "\t\tvv = ");
	    PRINTVV(afd, u.u_chown.VV);
	    fdprint(afd, "\n");
	    break;

	case OLDCML_Chmod_OP:
	    fdprint(afd, "\t\tfid = ");
	    PRINTFID(afd, u.u_chmod.Fid);
	    fdprint(afd, ", chmod = %o\n", u.u_chmod.Mode);
	    fdprint(afd, "\t\tvv = ");
	    PRINTVV(afd, u.u_chmod.VV);
	    fdprint(afd, "\n");
	    break;

	case OLDCML_Create_OP:
	    fdprint(afd, "\t\tpfid = ");
	    PRINTFID(afd, u.u_create.PFid);
	    fdprint(afd, ", name = (%s)\n\t\tcfid = ", u.u_create.Name);
	    PRINTFID(afd, u.u_create.CFid);
	    fdprint(afd, ", mode = %o\n", u.u_create.Mode);
	    fdprint(afd, "\t\tpvv = ");
	    PRINTVV(afd, u.u_create.PVV);
	    fdprint(afd, "\n");
	    break;

	case OLDCML_Remove_OP:
	    fdprint(afd, "\t\tpfid = ");
	    PRINTFID(afd, u.u_remove.PFid);
	    fdprint(afd, ", name = (%s)\n\t\tcfid = ", u.u_remove.Name);
	    PRINTFID(afd, u.u_remove.CFid);
	    fdprint(afd, "\n");
	    fdprint(afd, "\t\tpvv = ");
	    PRINTVV(afd, u.u_remove.PVV);
	    fdprint(afd, "\n");
	    fdprint(afd, "\t\tcvv = ");
	    PRINTVV(afd, u.u_remove.CVV);
	    fdprint(afd, "\n");
	    break;

	case OLDCML_Link_OP:
	    fdprint(afd, "\t\tpfid = ");
	    PRINTFID(afd, u.u_link.PFid);
	    fdprint(afd, ", name = (%s)\n\t\tcfid = ", u.u_link.Name);
	    PRINTFID(afd, u.u_link.CFid);
	    fdprint(afd, "\n");
	    fdprint(afd, "\t\tpvv = ");
	    PRINTVV(afd, u.u_link.PVV);
	    fdprint(afd, "\n");
	    fdprint(afd, "\t\tcvv = ");
	    PRINTVV(afd, u.u_link.CVV);
	    fdprint(afd, "\n");
	    break;

	case OLDCML_Rename_OP:
	    fdprint(afd, "\t\tspfid = ");
	    PRINTFID(afd, u.u_rename.SPFid);
	    fdprint(afd, ", sname = (%s)\n\t\ttpfid = ", u.u_rename.OldName);
	    PRINTFID(afd, u.u_rename.TPFid);
	    fdprint(afd, ", tname = (%s)\n\t\tsfid = ", u.u_rename.NewName);
	    PRINTFID(afd, u.u_rename.SFid);
	    fdprint(afd, "\n");
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

	case OLDCML_MakeDir_OP:
	    fdprint(afd, "\t\tpfid = ");
	    PRINTFID(afd, u.u_mkdir.PFid);
	    fdprint(afd, ", name = (%s)\n\t\tcfid = ", u.u_mkdir.Name);
	    PRINTFID(afd, u.u_mkdir.CFid);
	    fdprint(afd, ", mode = %o\n", u.u_mkdir.Mode);
	    fdprint(afd, "\t\tpvv = ");
	    PRINTVV(afd, u.u_mkdir.PVV);
	    fdprint(afd, "\n");
	    break;

	case OLDCML_RemoveDir_OP:
	    fdprint(afd, "\t\tpfid = ");
	    PRINTFID(afd, u.u_rmdir.PFid);
	    fdprint(afd, ", name = (%s)\n\t\tcfid = ", u.u_rmdir.Name);
	    PRINTFID(afd, u.u_rmdir.CFid);
	    fdprint(afd, "\n");
	    fdprint(afd, "\t\tpvv = ");
	    PRINTVV(afd, u.u_rmdir.PVV);
	    fdprint(afd, "\n");
	    fdprint(afd, "\t\tcvv = ");
	    PRINTVV(afd, u.u_rmdir.CVV);
	    fdprint(afd, "\n");
	    break;

	case OLDCML_SymLink_OP:
	    fdprint(afd, "\t\tpfid = ");
	    PRINTFID(afd, u.u_symlink.PFid);
	    fdprint(afd, ", name = (%s)\n\t\tcfid = ", u.u_symlink.NewName);
	    PRINTFID(afd, u.u_symlink.CFid);
	    fdprint(afd, ", contents = (%s), mode = %o\n",
		    u.u_symlink.OldName, u.u_symlink.Mode);
	    fdprint(afd, "\t\tpvv = ");
	    PRINTVV(afd, u.u_symlink.PVV);
	    fdprint(afd, "\n");
	    break;

        case OLDCML_Repair_OP:
	    fdprint(afd, "\t\tfid = ");
	    PRINTFID(afd, u.u_repair.Fid);
	    fdprint(afd, ", Length = %u\n\t\tattrs=[%d %d %o]\n",
		    u.u_repair.Length, u.u_repair.Date, u.u_repair.Owner,
		    u.u_repair.Mode);
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

/* There is a log routine corresponding to each of the (normal) mutating Vice operations, */
/* {Store, Truncate, Utimes, Chown, Chmod, Create, Remove, Link, Rename, Mkdir, Rmdir, Symlink}. */
/* Note that the only failure mode for these routines is log space exhausted (ENOSPC). */
/* Each of these routines MUST be called from within transaction! */

/* local-repair modification */
int volent::LogStore(time_t Mtime, vuid_t vuid,
		      ViceFid *Fid, RPC2_Unsigned NewLength, int tid) {
    LOG(1, ("volent::LogStore: %d, %d, (%x.%x.%x), %d %d\n",
	     Mtime, vuid, Fid->Volume, Fid->Vnode, Fid->Unique, NewLength, tid));

    if (LogOpts) {
	/* Cancel stores, as long as they are not followed by chowns. */
	/* Cancel utimes'. */
	int cancellation;
	do {
	    cancellation = 0;
	    cmlent *chown_mle = 0;
	    cml_iterator next(CML, AbortOrder, Fid);
	    cmlent *m;
	    while (!cancellation && (m = next())) {
		switch(m->opcode) {
		    case OLDCML_NewStore_OP:
			if (chown_mle == 0) {
			    cancellation = m->cancel();
			}
			break;

		    case OLDCML_Utimes_OP:
			cancellation = m->cancel();
			break;

		    case OLDCML_Chown_OP:
			if (chown_mle == 0)
			    chown_mle = m;
			break;
		}
	    }
	} while (cancellation);
    }

    cmlent *store_mle = new cmlent(&CML, Mtime, vuid, OLDCML_NewStore_OP, tid, Fid, NewLength);
    return(store_mle == 0 ? ENOSPC : 0);
}


/* local-repair modification */
int volent::LogSetAttr(time_t Mtime, vuid_t vuid, ViceFid *Fid,
			RPC2_Unsigned NewLength, Date_t NewDate,
			UserId NewOwner, RPC2_Unsigned NewMode, int tid) {
    /* Record a separate log entry for each attribute that is being set. */
    if (NewLength != (RPC2_Unsigned)-1) {
	int code = LogTruncate(Mtime, vuid, Fid, NewLength, tid);
	if (code != 0) return(code);
    }
    if (NewDate != (Date_t)-1) {
	int code = LogUtimes(Mtime, vuid, Fid, NewDate, tid);
	if (code != 0) return(code);
    }
    if (NewOwner != (UserId)-1) {
	int code = LogChown(Mtime, vuid, Fid, NewOwner, tid);
	if (code != 0) return(code);
    }
    if (NewMode != (RPC2_Unsigned)-1) {
	int code = LogChmod(Mtime, vuid, Fid, NewMode, tid);
	if (code != 0) return(code);
    }

    return(0);
}


/* local-repair modification */
int volent::LogTruncate(time_t Mtime, vuid_t vuid,
			 ViceFid *Fid, RPC2_Unsigned NewLength, int tid) {
    LOG(1, ("volent::LogTruncate: %d, %d, (%x.%x.%x), %d %d\n",
	     Mtime, vuid, Fid->Volume, Fid->Vnode, Fid->Unique, NewLength, tid));

    /* Treat truncates as stores for now. -JJK */
    return(LogStore(Mtime, vuid, Fid, NewLength, tid));
}


/* local-repair modification */
int volent::LogUtimes(time_t Mtime, vuid_t vuid,
		       ViceFid *Fid, Date_t NewDate, int tid) {
    LOG(1, ("volent::LogUtimes: %d, %d, (%x.%x.%x), %d %d\n",
	     Mtime, vuid, Fid->Volume, Fid->Vnode, Fid->Unique, NewDate, tid));

    if (LogOpts) {
	int cancellation;
	do {
	    cancellation = 0;
	    cml_iterator next(CML, AbortOrder, Fid);
	    cmlent *m;
	    while (!cancellation && (m = next())) {
		switch(m->opcode) {
		    case OLDCML_Utimes_OP:
			cancellation = m->cancel();
			break;
		}
	    }
	} while (cancellation);
    }

    cmlent *utimes_mle = new cmlent(&CML, Mtime, vuid, OLDCML_Utimes_OP, tid, Fid, NewDate);
    return(utimes_mle == 0 ? ENOSPC : 0);
}


/* local-repair modification */
int volent::LogChown(time_t Mtime, vuid_t vuid,
		      ViceFid *Fid, UserId NewOwner, int tid) {
    LOG(1, ("volent::LogChown: %d, %d, (%x.%x.%x), %d %d\n",
	     Mtime, vuid, Fid->Volume, Fid->Vnode, Fid->Unique, NewOwner, tid));

    if (LogOpts) {
	int cancellation;
	do {
	    cancellation = 0;
	    cml_iterator next(CML, AbortOrder, Fid);
	    cmlent *m;
	    while (!cancellation && (m = next())) {
		switch(m->opcode) {
		    case OLDCML_Chown_OP:
			cancellation = m->cancel();
			break;
		}
	    }
	} while (cancellation);
    }

    cmlent *chown_mle = new cmlent(&CML, Mtime, vuid, OLDCML_Chown_OP, tid, Fid, NewOwner);
    return(chown_mle == 0 ? ENOSPC : 0);
}


/* local-repair modification */
int volent::LogChmod(time_t Mtime, vuid_t vuid,
		      ViceFid *Fid, RPC2_Unsigned NewMode, int tid) {
    LOG(1, ("volent::LogChmod: %d, %d, (%x.%x.%x), %o %d\n",
	     Mtime, vuid, Fid->Volume, Fid->Vnode, Fid->Unique, NewMode, tid));

    if (LogOpts) {
	int cancellation;
	do {
	    cancellation = 0;
	    cmlent *store_mle = 0;
	    cml_iterator next(CML, AbortOrder, Fid);
	    cmlent *m;
	    while (!cancellation && (m = next())) {
		switch(m->opcode) {
		    case OLDCML_NewStore_OP:
			if (store_mle == 0)
			    store_mle = m;
			break;

		    case OLDCML_Chmod_OP:
			if (store_mle == 0) {
			    cancellation = m->cancel();
			}
			break;
		}
	    }
	} while (cancellation);
    }

    cmlent *chmod_mle = new cmlent(&CML, Mtime, vuid, OLDCML_Chmod_OP, tid, Fid, NewMode);
    return(chmod_mle == 0 ? ENOSPC : 0);
}


/* local-repair modification */
int volent::LogCreate(time_t Mtime, vuid_t vuid, ViceFid *PFid,
		       char *Name, ViceFid *CFid, RPC2_Unsigned Mode, int tid) {
    LOG(1, ("volent::LogCreate: %d, %d, (%x.%x.%x), %s, (%x.%x.%x), %o %d\n",
	     Mtime, vuid, PFid->Volume, PFid->Vnode, PFid->Unique,
	     Name, CFid->Volume, CFid->Vnode, CFid->Unique, Mode, tid));

    cmlent *create_mle = new cmlent(&CML, Mtime, vuid, OLDCML_Create_OP, tid,
				     PFid, Name, CFid, Mode);
    return(create_mle == 0 ? ENOSPC : 0);
}


/* local-repair modification */
int volent::LogRemove(time_t Mtime, vuid_t vuid, ViceFid *PFid,
		       char *Name, ViceFid *CFid, int LinkCount, int tid) {
    LOG(1, ("volent::LogRemove: %d, %d, (%x.%x.%x), %s, (%x.%x.%x), %d %d\n",
	     Mtime, vuid, PFid->Volume, PFid->Vnode, PFid->Unique,
	     Name, CFid->Volume, CFid->Vnode, CFid->Unique, LinkCount, tid));

    int ObjectCreated = 0;

    if (LogOpts) {
	if (LinkCount == 1) {
	    /* 
	     * if the object was created here, we may be able to do an 
	     * identity cancellation.  However, if the create is frozen,
	     * we cannot cancel records involved in an identity cancellation,
	     * because the create may have already become visible at the servers.
	     * Mark such records in case reintegration fails.  Records for which 
	     * this remove is an overwrite may be cancelled either way.  If they 
	     * are frozen cmlent::cancel does the right thing.
	     */
	    int CreateReintegrating = 0;
	    {
		cml_iterator next(CML, CommitOrder, CFid);
		cmlent *m = next();
		if (m != 0 && (m->opcode == OLDCML_Create_OP || m->opcode ==
                               OLDCML_SymLink_OP)) {
		    ObjectCreated = 1;
		    if (m->IsFrozen())
			CreateReintegrating = 1;
                }    

		
/*
		if (ObjectCreated) {
		    int code = LogUtimes(Mtime, vuid, PFid, Mtime);
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
		    switch(m->opcode) {
			case OLDCML_NewStore_OP:
			case OLDCML_Utimes_OP:
			case OLDCML_Chown_OP:
			case OLDCML_Chmod_OP:
			    cancellation = m->cancel();
			    break;

			case OLDCML_Create_OP:
			case OLDCML_Remove_OP:
			case OLDCML_Link_OP:
			case OLDCML_Rename_OP:
			case OLDCML_SymLink_OP:
			    if (ObjectCreated) {
				if (CreateReintegrating) {
				    RVMLIB_REC_OBJECT(m->flags);
				    m->flags.cancellation_pending = 1;
				} else 
				    cancellation = m->cancel();
			    }
			    break;

		        case OLDCML_Repair_OP:
			    break;

			default:
			    CODA_ASSERT(0);
		    }
		}
	    } while (cancellation);

	    if (ObjectCreated && !CreateReintegrating) {
		int size = (int) (sizeof(cmlent) + strlen(Name));    

		LOG(0/*10*/, ("volent::LogRemove: record cancelled, %s, size = %d\n", 
				Name, size));
		CML.cancellations.other_count++;
		CML.cancellations.other_size += size;
		return(0);
	    }
	}
    }

    cmlent *unlink_mle = new cmlent(&CML, Mtime, vuid, OLDCML_Remove_OP, tid,
				     PFid, Name, CFid, LinkCount);
    if (ObjectCreated && unlink_mle) {	/* must be reintegrating */
	RVMLIB_REC_OBJECT(unlink_mle->flags);
	unlink_mle->flags.cancellation_pending = 1;    
    }

    return(unlink_mle == 0 ? ENOSPC : 0);
}


/* local-repair modification */
int volent::LogLink(time_t Mtime, vuid_t vuid, ViceFid *PFid,
		     char *Name, ViceFid *CFid, int tid) {
    LOG(1, ("volent::LogLink: %d, %d, (%x.%x.%x), %s, (%x.%x.%x) %d\n",
	     Mtime, vuid, PFid->Volume, PFid->Vnode, PFid->Unique,
	     Name, CFid->Volume, CFid->Vnode, CFid->Unique, tid));

    cmlent *link_mle = new cmlent(&CML, Mtime, vuid, OLDCML_Link_OP, tid,
				   PFid, Name, CFid);
    return(link_mle == 0 ? ENOSPC : 0);
}


/* local-repair modification */
int volent::LogRename(time_t Mtime, vuid_t vuid, ViceFid *SPFid,
		       char *OldName, ViceFid *TPFid, char *NewName,
		       ViceFid *SFid, ViceFid *TFid, int LinkCount, int tid) {
    /* Record "target remove" as a separate log entry. */
    if (!FID_EQ(TFid, &NullFid)) {
	int code;
	if (ISDIR(*SFid))
	    code = LogRmdir(Mtime, vuid, TPFid, NewName, TFid, tid);
	else
	    code = LogRemove(Mtime, vuid, TPFid, NewName, TFid, LinkCount, tid);
	if (code != 0) return(code);

    }

    LOG(1, ("volent::LogRename: %d, %d, (%x.%x.%x), %s, (%x.%x.%x), %s, (%x.%x.%x) %d\n",
	     Mtime, vuid, SPFid->Volume, SPFid->Vnode, SPFid->Unique,
	     OldName, TPFid->Volume, TPFid->Vnode, TPFid->Unique,
	     NewName, SFid->Volume, SFid->Vnode, SFid->Unique, tid));

    cmlent *rename_mle = new cmlent(&CML, Mtime, vuid, OLDCML_Rename_OP, tid,
				     SPFid, OldName, TPFid, NewName, SFid);
    return(rename_mle == 0 ? ENOSPC : 0);
}


/* local-repair modification */
int volent::LogMkdir(time_t Mtime, vuid_t vuid, ViceFid *PFid,
		      char *Name, ViceFid *CFid, RPC2_Unsigned Mode, int tid) {
    LOG(1, ("volent::LogMkdir: %d, %d, (%x.%x.%x), %s, (%x.%x.%x), %o %d\n",
	     Mtime, vuid, PFid->Volume, PFid->Vnode, PFid->Unique,
	     Name, CFid->Volume, CFid->Vnode, CFid->Unique, Mode, tid));

    cmlent *mkdir_mle = new cmlent(&CML, Mtime, vuid, OLDCML_MakeDir_OP, tid,
				    PFid, Name, CFid, Mode);
    return(mkdir_mle == 0 ? ENOSPC : 0);
}


/* local-repair modification */
int volent::LogRmdir(time_t Mtime, vuid_t vuid, ViceFid *PFid,
		      char *Name, ViceFid *CFid, int tid) {
    LOG(1, ("volent::LogRmdir: %d, %d, (%x.%x.%x), %s, (%x.%x.%x) %d\n",
	     Mtime, vuid, PFid->Volume, PFid->Vnode, PFid->Unique,
	     Name, CFid->Volume, CFid->Vnode, CFid->Unique, tid));

    int ObjectCreated = 0;
    int DependentChildren = 0;

    if (LogOpts) {
	int CreateReintegrating = 0;	/* see comments in LogRemove */
	{
	    cml_iterator next(CML, CommitOrder, CFid);
	    cmlent *m = next();
	    if (m != 0 && m->opcode == OLDCML_MakeDir_OP) {
		ObjectCreated = 1;
		if (m->IsFrozen())
		    CreateReintegrating = 1;
	    }
	    if (ObjectCreated) {
		cml_iterator next(CML, AbortOrder, CFid);
		cmlent *m;
		while ((m = next()) && !DependentChildren) {
		    switch(m->opcode) {
			case OLDCML_Create_OP:
			case OLDCML_Remove_OP:
			case OLDCML_Link_OP:
			case OLDCML_RemoveDir_OP:
			case OLDCML_SymLink_OP:
			    DependentChildren = 1;
			    break;

			case OLDCML_Rename_OP:
			    if (!FID_EQ(CFid, &m->u.u_rename.SFid))
				DependentChildren = 1;
			    break;

			case OLDCML_MakeDir_OP:
			    if (FID_EQ(CFid, &m->u.u_mkdir.PFid))
				DependentChildren = 1;
			    break;
		    }
		}
	    }

/*
	    if (ObjectCreated && !DependentChildren) {
		int code = LogUtimes(Mtime, vuid, PFid, Mtime);
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
		switch(m->opcode) {
		    case OLDCML_Utimes_OP:
		    case OLDCML_Chown_OP:
		    case OLDCML_Chmod_OP:
			cancellation = m->cancel();
			break;

		    case OLDCML_Create_OP:
		    case OLDCML_Remove_OP:
		    case OLDCML_Link_OP:
		    case OLDCML_Rename_OP:
		    case OLDCML_MakeDir_OP:
		    case OLDCML_RemoveDir_OP:
		    case OLDCML_SymLink_OP:
			if (ObjectCreated && !DependentChildren) {
			    if (CreateReintegrating) {
				RVMLIB_REC_OBJECT(m->flags);
				m->flags.cancellation_pending = 1;
			    } else 
				cancellation = m->cancel();
			}
			break;

		    case OLDCML_Repair_OP:
			break;

		    default:
			CODA_ASSERT(0);
		}
	    }
	} while (cancellation);

	if (ObjectCreated && !DependentChildren && !CreateReintegrating) {
	    int size = (int) (sizeof(cmlent) + strlen(Name));    

	    LOG(0/*10*/, ("volent::LogRmdir: record cancelled, %s, size = %d\n", 
				Name, size));

	    CML.cancellations.other_count++;
	    CML.cancellations.other_size += size;
	    return(0);
	}
    }

    cmlent *rmdir_mle = new cmlent(&CML, Mtime, vuid, OLDCML_RemoveDir_OP, tid,
				    PFid, Name, CFid);

    if (ObjectCreated && !DependentChildren && rmdir_mle) {
	RVMLIB_REC_OBJECT(rmdir_mle->flags);
	rmdir_mle->flags.cancellation_pending = 1;
    }
    return(rmdir_mle == 0 ? ENOSPC : 0);
}


/* local-repair modification */
int volent::LogSymlink(time_t Mtime, vuid_t vuid, ViceFid *PFid,
			char *OldName, char *NewName, 
			ViceFid *CFid, RPC2_Unsigned Mode, int tid) {
    LOG(1, ("volent::LogSymlink: %d, %d, (%x.%x.%x), %s, %s, (%x.%x.%x), %o %d\n",
	     Mtime, vuid, PFid->Volume, PFid->Vnode, PFid->Unique,
	     OldName, NewName, CFid->Volume, CFid->Vnode, CFid->Unique, Mode, tid));

    cmlent *symlink_mle = new cmlent(&CML, Mtime, vuid, OLDCML_SymLink_OP, tid,
				      PFid, OldName, NewName, CFid, Mode);
    return(symlink_mle == 0 ? ENOSPC : 0);
}

/* local-repair modification */
int volent::LogRepair(time_t Mtime, vuid_t vuid, ViceFid *Fid,
		      RPC2_Unsigned Length, Date_t Date, UserId Owner,
		      RPC2_Unsigned Mode, int tid) {
    LOG(1, ("volent::LogRepair: %d %d (%x.%x.%x) attrs [%u %d %u %o] %d\n",
	    Mtime, vuid, Fid->Volume, Fid->Vnode, Fid->Unique, Length,
	    Date, Owner, Mode, tid));
    cmlent *repair_mle = new cmlent(&CML, Mtime, vuid, OLDCML_Repair_OP, tid,
				    Fid, Length, Date, Owner, Mode, tid);
    return(repair_mle == 0 ? ENOSPC : 0);
}

/* 
 * cancel all stores corresponding to the given Fid.
 * MUST NOT be called from within transaction! 
 */
void volent::CancelStores(ViceFid *Fid) {
    LOG(1, ("volent::CancelStores: (%x.%x.%x)\n",
	    Fid->Volume, Fid->Vnode, Fid->Unique));

    /* this routine should be called at startup only */
    CODA_ASSERT(!IsReintegrating());

    Recov_BeginTrans();
	int cancellation;
	do {
	    cancellation = 0;
	    cml_iterator next(CML, AbortOrder, Fid);
	    cmlent *m;
	    while ((m = next())) {
		if (m->opcode == OLDCML_NewStore_OP && m->cancel()) {    
		    cancellation = 1;
		    /* 
		     * Since cancelled store is not being overwritten, we
		     * must restore ``old values'' for attributes in fsobj. 
		     */
		    RestoreObj(Fid);
		    break;
		}
	    }
	} while (cancellation);
    Recov_EndTrans(MAXFP);

    /* we may have cancelled the last record. */
    if (CML.count() == 0)
        CML.owner = UNSET_UID;
}


/* restore ``old values'' for attributes in fsobj. */
/* call from within a transaction. */
void volent::RestoreObj(ViceFid *Fid) {
    fsobj *f = FSDB->Find(Fid);

    /* Length attribute. */
    long Length;
    cmlent *lwriter = CML.LengthWriter(Fid);
    if (lwriter == 0) {
	FSO_ASSERT(f, f->CleanStat.Length != -1);
	Length = f->CleanStat.Length;
    }
    else {
	switch(lwriter->opcode) {
	    case OLDCML_NewStore_OP:
		Length = lwriter->u.u_store.Length;
		break;

	    default:
		VOL_ASSERT(this, 0);
	}
    }
    if (Length != f->stat.Length) {
	RVMLIB_REC_OBJECT(f->stat.Length);
	f->stat.Length = f->stat.GotThisData = Length;
    }

    /* Mtime attribute. */
    Date_t Utimes;
    cmlent *uwriter = CML.UtimesWriter(Fid);
    if (uwriter == 0) {
	FSO_ASSERT(f, f->CleanStat.Date != (Date_t)-1);
	Utimes = f->CleanStat.Date;
    }
    else {
	switch(uwriter->opcode) {
	    case OLDCML_NewStore_OP:
	    case OLDCML_Create_OP:
	    case OLDCML_MakeDir_OP:
	    case OLDCML_SymLink_OP:
	    case OLDCML_Repair_OP:
	    case OLDCML_Remove_OP:
	    case OLDCML_Link_OP:
	    case OLDCML_Rename_OP:
	    case OLDCML_RemoveDir_OP:
		Utimes = uwriter->time;
		break;

	    case OLDCML_Utimes_OP:
		Utimes = uwriter->u.u_utimes.Date;
		break;

	    default:
		VOL_ASSERT(this, 0);
	}
    }
    if (Utimes != f->stat.Date) {
	RVMLIB_REC_OBJECT(f->stat.Date);
	f->stat.Date = Utimes;
    }
}


cmlent *ClientModifyLog::LengthWriter(ViceFid *Fid) {
    cml_iterator next(*this, AbortOrder, Fid);
    cmlent *m;
    while ((m = next())) {
	switch(m->opcode) {
	    case OLDCML_NewStore_OP:
	    case OLDCML_Repair_OP:
		return(m);
	}
    }

    /* Not found. */
    return(0);
}


cmlent *ClientModifyLog::UtimesWriter(ViceFid *Fid) {
    cml_iterator next(*this, AbortOrder, Fid);
    cmlent *m;
    while ((m = next())) {
	switch(m->opcode) {
	    case OLDCML_NewStore_OP:
	    case OLDCML_Utimes_OP:
	    case OLDCML_Create_OP:
	    case OLDCML_MakeDir_OP:
	    case OLDCML_SymLink_OP:
	    case OLDCML_Repair_OP:
		return(m);

	    case OLDCML_Remove_OP:
		if (FID_EQ(Fid, &m->u.u_remove.PFid))
		    return(m);
		break;

	    case OLDCML_Link_OP:
		if (FID_EQ(Fid, &m->u.u_link.PFid))
		    return(m);
		break;

	    case OLDCML_Rename_OP:
		if (FID_EQ(Fid, &m->u.u_rename.SPFid) ||
		    FID_EQ(Fid, &m->u.u_rename.TPFid))
		    return(m);
		break;

	    case OLDCML_RemoveDir_OP:
		if (FID_EQ(Fid, &m->u.u_rmdir.PFid))
		    return(m);
		break;

	    default:
		break;
	}
    }

    /* Not found. */
    return(0);
}


/* local-repair modification */
/* MUST be called from within transaction! */
/* returns 1 if record was actually removed from log, 0 if not. */
int cmlent::cancel() {
    volent *vol = strbase(volent, log, CML);
    time_t curTime = Vtime();

    if (flags.to_be_repaired) {
	LOG(0, ("cmlent::cancel: to_be_repaired cmlent, skip\n"));
	return 0;
    }

    /* 
     * If this record is being reintegrated, just mark it for
     * cancellation and we'll get to it later.
     */
    if (flags.frozen) {
	LOG(0, ("cmlent::cancel: cmlent frozen, skip\n"));
	RVMLIB_REC_OBJECT(flags);	/* called from within transaction */
	flags.cancellation_pending = 1;
	return 0;
    }

    LOG(0/*10*/, ("cmlent::cancel: age = %d\n", curTime-time));
    if (LogLevel >= 0/*10*/) print(logFile);

    /* Parameters for possible utimes to be done AFTER cancelling this record. */
    int DoUtimes = 0;
    vuid_t UtimesVuid;
    ViceFid UtimesFid;
    Date_t UtimesMtime;

    switch(opcode) {
	case OLDCML_NewStore_OP:
	    {
	    /* Cancelling store may permit cancellation of earlier chmod. */

	    cmlent *pre_chmod_mle = 0;
	    cmlent *post_chmod_mle = 0;

	    {
		cml_iterator next(*(ClientModifyLog *)log, CommitOrder, &u.u_store.Fid, this);
		cmlent *m;
		while ((m = next()) && post_chmod_mle == 0)
		    switch(m->opcode) {
			case OLDCML_Chmod_OP:
			    post_chmod_mle = m;
			    break;
		    }
	    }

	    if (post_chmod_mle != 0) {
		cml_iterator next(*(ClientModifyLog *)log, AbortOrder, &u.u_store.Fid, this);
		cmlent *m;
		while ((m = next()) && pre_chmod_mle == 0)
		    switch(m->opcode) {
			case OLDCML_Chmod_OP:
			    pre_chmod_mle = m;
			    break;
		    }
	    }

	    if (pre_chmod_mle != 0 && post_chmod_mle != 0)
		pre_chmod_mle->cancel();
	    }
	    break;

	case OLDCML_Chown_OP:
	    {
	    /* Cancelling chown may permit cancellation of earlier store. */

	    cmlent *pre_store_mle = 0;
	    cmlent *post_store_mle = 0;

	    {
		cml_iterator next(*(ClientModifyLog *)log, CommitOrder, &u.u_chown.Fid, this);
		cmlent *m;
		while ((m = next()) && post_store_mle == 0)
		    switch(m->opcode) {
			case OLDCML_NewStore_OP:
			    post_store_mle = m;
			    break;
		    }
	    }

	    if (post_store_mle != 0) {
		cml_iterator next(*(ClientModifyLog *)log, AbortOrder, &u.u_chown.Fid, this);
		cmlent *m;
		while ((m = next()) && pre_store_mle == 0)
		    switch(m->opcode) {
			case OLDCML_NewStore_OP:
			    pre_store_mle = m;
			    break;
		    }
	    }

	    if (pre_store_mle != 0 && post_store_mle != 0)
		(void) pre_store_mle->cancel();
	    }
	    break;

	case OLDCML_Create_OP:
	    {
	    cmlent *m = ((ClientModifyLog *)log)->UtimesWriter(&u.u_create.PFid);
	    CODA_ASSERT(m != 0);
	    if (m != this) {
		DoUtimes = 1;
		UtimesVuid = (vuid_t) uid;
		UtimesFid = u.u_create.PFid;
		UtimesMtime = time;
	    }
	    }
	    break;

	case OLDCML_Remove_OP:
	    {
	    cmlent *m = ((ClientModifyLog *)log)->UtimesWriter(&u.u_remove.PFid);
	    CODA_ASSERT(m != 0);
	    if (m != this) {
		DoUtimes = 1;
		UtimesVuid = (vuid_t) uid;
		UtimesFid = u.u_remove.PFid;
		UtimesMtime = time;
	    }
	    }
	    break;

	case OLDCML_Link_OP:
	    {
	    cmlent *m = ((ClientModifyLog *)log)->UtimesWriter(&u.u_link.PFid);
	    CODA_ASSERT(m != 0);
	    if (m != this) {
		DoUtimes = 1;
		UtimesVuid = (vuid_t) uid;
		UtimesFid = u.u_link.PFid;
		UtimesMtime = time;
	    }
	    }
	    break;

	case OLDCML_Rename_OP:
	    {
	    cmlent *m = ((ClientModifyLog *)log)->UtimesWriter(&u.u_rename.SPFid);
	    CODA_ASSERT(m != 0);
	    if (m != this) {
		DoUtimes = 1;
		UtimesVuid = (vuid_t) uid;
		UtimesFid = u.u_rename.SPFid;
		UtimesMtime = time;
	    }

	    if (!FID_EQ(&u.u_rename.SPFid, &u.u_rename.TPFid)) {
		cmlent *m = ((ClientModifyLog *)log)->UtimesWriter(&u.u_rename.TPFid);
		CODA_ASSERT(m != 0);
		if (m != this) {
		    /* Don't get uptight if this can't be done! */
/*
		    volent *vol = strbase(volent, log, CML);
		    (void)vol->LogUtimes(time, (vuid_t) uid, &u.u_rename.TPFid, time);
*/
		}
	    }
	    }
	    break;

	case OLDCML_MakeDir_OP:
	    {
	    cmlent *m = ((ClientModifyLog *)log)->UtimesWriter(&u.u_mkdir.PFid);
	    CODA_ASSERT(m != 0);
	    if (m != this) {
		DoUtimes = 1;
		UtimesVuid = (vuid_t) uid;
		UtimesFid = u.u_mkdir.PFid;
		UtimesMtime = time;
	    }
	    }
	    break;

	case OLDCML_RemoveDir_OP:
	    {
	    cmlent *m = ((ClientModifyLog *)log)->UtimesWriter(&u.u_rmdir.PFid);
	    CODA_ASSERT(m != 0);
	    if (m != this) {
		DoUtimes = 1;
		UtimesVuid = (vuid_t) uid;
		UtimesFid = u.u_rmdir.PFid;
		UtimesMtime = time;
	    }
	    }
	    break;

	case OLDCML_SymLink_OP:
	    {
	    cmlent *m = ((ClientModifyLog *)log)->UtimesWriter(&u.u_symlink.PFid);
	    CODA_ASSERT(m != 0);
	    if (m != this) {
		DoUtimes = 1;
		UtimesVuid = (vuid_t) uid;
		UtimesFid = u.u_symlink.PFid;
		UtimesMtime = time;
	    }
	    }
	    break;

        case OLDCML_Repair_OP:
	    CODA_ASSERT(0);
	    break;
    }

    if (opcode == OLDCML_NewStore_OP) {
	log->cancellations.store_count++;
	log->cancellations.store_size += bytes();
	log->cancellations.store_contents_size += u.u_store.Length;
    }
    else {
	log->cancellations.other_count++;
	log->cancellations.other_size += bytes();
    }
    vol->RecordsCancelled++;
    delete this;

    if (DoUtimes) {
/*
	int code = vol->LogUtimes(UtimesMtime, UtimesVuid, &UtimesFid, UtimesMtime);
	CODA_ASSERT(code == 0);
        vol->RecordsCancelled--;
*/
    }
    return 1;
}


/* 
 * If this record is a store corresponding to an open-for-write file,
 * cancel it and restore the object's attributes to their old values.
 */
int cmlent::cancelstore() {
    int cancelled = 0;
    volent *vol = strbase(volent, log, CML);

    if (opcode == OLDCML_NewStore_OP) {
	dlink *d = fid_bindings->first();   /* and only */
	binding *b = strbase(binding, d, binder_handle);
	fsobj *f = (fsobj *)b->bindee;

	if (WRITING(f)) {
	    Recov_BeginTrans();
		/* shouldn't be reintegrating, so cancel must go through */   
		CODA_ASSERT(cancel());
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
int ClientModifyLog::IncReallocFids(int tid) {
    volent *vol = strbase(volent, this, CML);
    LOG(1, ("ClientModifyLog::IncReallocFids: (%s) and tid = %d\n", 
	    vol->name, tid));

    int code = 0;
    cml_iterator next(*this, CommitOrder);
    cmlent *m;
    while ((m = next())) {
	if (m->GetTid() == tid)
	  code = m->realloc();
	if (code != 0) break;
    }
    LOG(0, ("ClientModifyLog::IncReallocFids: (%s)\n", vol->name));
    return(code);
}


/* MUST be called from within transaction! */
void ClientModifyLog::TranslateFid(ViceFid *OldFid, ViceFid *NewFid) 
{
    cml_iterator next(*this, CommitOrder, OldFid);
    cmlent *m;
    while ((m = next()))
	m->translatefid(OldFid, NewFid);
}


/* need not be called from within a transaction */
void ClientModifyLog::IncThread(int tid) {
    volent *vol = strbase(volent, this, CML);
    LOG(1, ("ClientModifyLog::IncThread: (%s) tid = %d\n", 
	    vol->name, tid));

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
		    fsobj *f = (fsobj *)b->bindee;

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

    if (LogLevel >= 10)
	print(logFile);

    LOG(0, ("ClientModifyLog::IncThread: (%s)\n", vol->name));
}


/* need not be called from within a transaction */
/* Munge the ClientModifyLog into a format suitable for reintegration. */
/* Caller is responsible for deallocating buffer! */
void ClientModifyLog::IncPack(char **bufp, int *bufsizep, int tid) {
    volent *vol = strbase(volent, this, CML);
    LOG(1, ("ClientModifyLog::IncPack: (%s) and tid = %d\n", vol->name, tid));

    /* Compute size of buffer needed. */
    {
	int len = 0;
	cml_iterator next(*this, CommitOrder);
	cmlent *mle;
	while ((mle = next()))
	  if (mle->GetTid() == tid)
	    len += mle->size();

	*bufsizep = len;
    }

    /* Allocate such a buffer. */
    *bufp = new char[*bufsizep];

    /* Pack according to commit order. */
    {
	PARM *_ptr = (PARM *)*bufp;
	cml_iterator next(*this, CommitOrder);
	cmlent *m;
	while ((m = next()))
	  if (m->GetTid() == tid)
	    m->pack(&_ptr);
    }

    LOG(0, ("ClientModifyLog::IncPack: (%s)\n", vol->name));
}


#define UNSET_INDEX -2
#define MAXSTALEDIRS 50

/* MUST NOT be called from within transaction! */
int ClientModifyLog::COP1(char *buf, int bufsize, ViceVersionVector *UpdateSet) {
    volent *vol = strbase(volent, this, CML);
    int code = 0;
    unsigned int i = 0;
    mgrpent *m = 0;
    
    /* Set up the SE descriptor. */
    SE_Descriptor sed;
    memset(&sed, 0, sizeof(SE_Descriptor));
    sed.Tag = SMARTFTP;
    struct SFTP_Descriptor *sei = &sed.Value.SmartFTPD;
    sei->TransmissionDirection = CLIENTTOSERVER;
    sei->hashmark = 0;
    sei->SeekOffset = 0;
    sei->ByteQuota = -1;
    sei->Tag = FILEINVM;
    sei->FileInfo.ByAddr.vmfile.SeqLen = bufsize;
    sei->FileInfo.ByAddr.vmfile.SeqBody = (RPC2_ByteSeq)buf;

    /* COP2 Piggybacking. */
    long cbtemp; cbtemp = cbbreaks;
    char PiggyData[COP2SIZE];
    RPC2_CountedBS PiggyBS;
    PiggyBS.SeqLen = 0;
    PiggyBS.SeqBody = (RPC2_ByteSeq)PiggyData;

    /* VCB maintenance */
    RPC2_Integer VS = 0;
    CallBackStatus VCBStatus = NoCallBack;

    RPC2_Integer Index = UNSET_INDEX;
    ViceFid StaleDirs[MAXSTALEDIRS];
    RPC2_Integer MaxStaleDirs = MAXSTALEDIRS;
    RPC2_Integer NumStaleDirs = 0;

    /* Acquire an Mgroup. */
    code = vol->GetMgrp(&m, owner, (PIGGYCOP2 ? &PiggyBS : 0));
    if (code != 0) goto Exit;

    if (vol->IsWeaklyConnected() && m->rocc.HowMany > 1) {
	/* Pick a server and get a connection to it. */
	int ph_ix;
	unsigned long ph = m->GetPrimaryHost(&ph_ix);
	CODA_ASSERT(ph != 0);

	connent *c = 0;
	code = ::GetConn(&c, ph, owner, 0);
	if (code != 0) goto Exit;
	
	/* don't bother with VCBs, will lose them on resolve anyway */
	RPC2_CountedBS OldVS; 
	OldVS.SeqLen = 0;
	vol->ClearCallBack();

	/* Make the RPC call. */
	MarinerLog("store::Reintegrate %s, (%d, %d)\n", 
		   vol->name, count(), bufsize);

	UNI_START_MESSAGE(ViceReintegrate_OP);
	code = (int) ViceReintegrate(c->connid, vol->vid, bufsize, &Index,
				     MaxStaleDirs, &NumStaleDirs,
				     StaleDirs, &OldVS, &VS,
				     &VCBStatus, &PiggyBS, &sed);
	UNI_END_MESSAGE(ViceReintegrate_OP);
	MarinerLog("store::reintegrate done\n");

	code = vol->Collate(c, code);
	UNI_RECORD_STATS(ViceReintegrate_OP);
	
	/* 
	 * if the return code is EALREADY, the log records up to and
	 * including the one with the storeid that matches the 
	 * uniquifier in Index have been committed at the server.  
	 * Mark the last of those records.
	 */
	if (code == EALREADY) 
	    MarkCommittedMLE((RPC2_Unsigned) Index);

	/* if there is a semantic failure, mark the offending record */
	if (code != 0 && code != EALREADY &&
	    code != ERETRY && code != EWOULDBLOCK && code != ETIMEDOUT) 
	    MarkFailedMLE((int) Index);
	
	if (code != 0) goto Exit;

	/* Finalize COP2 Piggybacking. */
	if (PIGGYCOP2)
	    vol->ClearCOP2(&PiggyBS);

	bufsize += sed.Value.SmartFTPD.BytesTransferred;
	LOG(10, ("ViceReintegrate: transferred %d bytes\n",
		 sed.Value.SmartFTPD.BytesTransferred));

	/* Purge off stale directory fids, if any. fsobj::Kill is idempotent. */
	LOG(0, ("ClientModifyLog::COP1: %d stale dirs\n", NumStaleDirs));
	for (int d = 0; d < NumStaleDirs; d++) {
	    LOG(0, ("ClientModifyLog::COP1: stale dir 0x%x.0x%x.0x%x\n", 
		    StaleDirs[d].Volume, StaleDirs[d].Vnode, StaleDirs[d].Unique));
	    fsobj *f = FSDB->Find(&StaleDirs[d]);
	    CODA_ASSERT(f);
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
	vol->PackVS(m->nhosts, &OldVS);

	/* Make multiple copies of the IN/OUT and OUT parameters. */
	ARG_MARSHALL(IN_OUT_MODE, SE_Descriptor, sedvar, sed, m->nhosts);
	ARG_MARSHALL(OUT_MODE, RPC2_Integer, VSvar, VS, m->nhosts);
	ARG_MARSHALL(OUT_MODE, CallBackStatus, VCBStatusvar, VCBStatus, m->nhosts);
	ARG_MARSHALL(OUT_MODE, RPC2_Integer, Indexvar, Index, m->nhosts);
	ARG_MARSHALL(OUT_MODE, RPC2_Integer, NumStaleDirsvar, NumStaleDirs, m->nhosts);
	ARG_MARSHALL_ARRAY(OUT_MODE, ViceFid, StaleDirsvar, 0, MaxStaleDirs, StaleDirs, m->nhosts);

	/* Make the RPC call. */
	MarinerLog("store::Reintegrate %s, (%d, %d)\n", 
		   vol->name, count(), bufsize);

	MULTI_START_MESSAGE(ViceReintegrate_OP);
	code = (int) MRPC_MakeMulti(ViceReintegrate_OP,
				    ViceReintegrate_PTR,
				    m->nhosts, m->rocc.handles,
				    m->rocc.retcodes, m->rocc.MIp, 0, 0,
				    vol->vid, bufsize, Indexvar_ptrs,
				    MaxStaleDirs, NumStaleDirsvar_ptrs, 
				    StaleDirsvar_ptrs,
				    &OldVS, VSvar_ptrs, VCBStatusvar_ptrs,
				    &PiggyBS, sedvar_bufs);
	MULTI_END_MESSAGE(ViceReintegrate_OP);

	MarinerLog("store::reintegrate done\n");

	/* Examine the return code to decide what to do next. */
	code = vol->Collate_Reintegrate(m, code, UpdateSet);
	MULTI_RECORD_STATS(ViceReintegrate_OP);

	free(OldVS.SeqBody);

	/* Collate the failure index.  Grab the smallest one. */
	for (i = 0; i < m->nhosts; i++) 
	    if (m->rocc.hosts[i]) 
		if (Index == UNSET_INDEX || Index > Indexvar_bufs[i]) 
		    Index = Indexvar_bufs[i];
	
	/* 
	 * if the return code is EALREADY, the log records up to and
	 * including the one with the storeid that matches the 
	 * uniquifier in Index have been committed at the server.  
	 * Mark the last of those records.
	 */
	if (code == EALREADY) 
	    MarkCommittedMLE((RPC2_Unsigned) Index);

	/* if there is a semantic failure, mark the offending record */
	if (code != 0 && code != EALREADY &&
	    code != ERETRY && code != EWOULDBLOCK && code != ETIMEDOUT) 
	    MarkFailedMLE((int) Index);

	if (code == EASYRESOLVE) { code = 0; }
	if (code != 0) goto Exit;

	/* Collate volume callback information */
	if (cbtemp == cbbreaks)
	    vol->CollateVCB(m, VSvar_bufs, VCBStatusvar_bufs);

	/* Finalize COP2 Piggybacking. */
	if (PIGGYCOP2) 
	    vol->ClearCOP2(&PiggyBS);

	/* Manually compute the OUT parameters from the mgrpent::Reintegrate() call! -JJK */
	int dh_ix; dh_ix = -1;
	(void)m->DHCheck(0, -1, &dh_ix);
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
	for (unsigned int rep = 0; rep < m->nhosts; rep++) 
	    if (m->rocc.hosts[rep]) {	/* did this server participate? */
		/* must look at all server feedback */
		ARG_UNMARSHALL(NumStaleDirsvar, NumStaleDirs, rep);
		ARG_UNMARSHALL_ARRAY(StaleDirsvar, NumStaleDirs, StaleDirs, rep);
		LOG(0, ("ClientModifyLog::COP1: (replica %d) %d stale dirs\n", 
			rep, NumStaleDirs));

		/* server may have found more stale dirs */
		if (NumStaleDirs == MaxStaleDirs)
		    vol->ClearCallBack();

		/* purge them off.  fsobj::Kill is idempotent. */
		for (int d = 0; d < NumStaleDirs; d++) {
		    LOG(0, ("ClientModifyLog::COP1: stale dir 0x%x.0x%x.0x%x\n", 
			    StaleDirs[d].Volume, StaleDirs[d].Vnode, StaleDirs[d].Unique));
		    fsobj *f = FSDB->Find(&StaleDirs[d]);
		    CODA_ASSERT(f);
		    Recov_BeginTrans();
			f->Kill();
		    Recov_EndTrans(DMFP);
		}
	    }
    }

Exit:
    PutMgrp(&m);
    LOG(0, ("ClientModifyLog::COP1: (%s), %d bytes, returns %d, index = %d\n",
	     vol->name, bufsize, code, Index));
    return(code);
}


/* Update the version state of fsobj's following successful reintegration. */
/* MUST NOT be called from within transaction! */
void ClientModifyLog::IncCommit(ViceVersionVector *UpdateSet, int Tid) {
    volent *vol = strbase(volent, this, CML);
    LOG(1, ("ClientModifyLog::IncCommit: (%s) tid = %d\n", 
	    vol->name, Tid));

    CODA_ASSERT(count() > 0);

    Recov_BeginTrans();
	rec_dlist_iterator next(list);
	rec_dlink *d = next();			/* get first element */

	while (1) {
	    if (!d) break;			/* list exhausted */
	    cmlent *m = strbase(cmlent, d, handle);
	    if (m->GetTid() == Tid) {
		/* special case -- last of records already committed at server */
		if (m->flags.committed) 	/* commit this and return */
		    d = NULL;
		else 
		    d = next();	/* advance d before it is un-listed by m->commit */
		m->commit(UpdateSet);
	    } else {
		d = next();
	    }
	}
    Recov_EndTrans(DMFP);

    /* flush COP2 for this volume */
    vol->FlushCOP2();
    vol->flags.resolve_me = 0;
    LOG(0, ("ClientModifyLog::IncCommit: (%s)\n", vol->name));
}


/* Allocate a real fid for a locally created one, and translate all 
   references. */
/* Must NOT be called from within transaction! */
int cmlent::realloc() 
{
    volent *vol = strbase(volent, log, CML);

    int code = 0;

    ViceFid OldFid;
    ViceFid NewFid;
    ViceDataType type;
    RPC2_Unsigned AllocHost;
    switch(opcode) {
	case OLDCML_Create_OP:
	    if (!FID_IsLocalFile(&u.u_create.CFid))
		goto Exit;
	    OldFid = u.u_create.CFid;
	    type = File;
	    break;

	case OLDCML_MakeDir_OP:
	    if (!FID_IsLocalDir(&u.u_mkdir.CFid))
		goto Exit;
	    OldFid = u.u_mkdir.CFid;
	    type = Directory;
	    break;

	case OLDCML_SymLink_OP:
	    if (!FID_IsLocalFile(&u.u_symlink.CFid))
		goto Exit;
	    OldFid = u.u_symlink.CFid;
	    type = SymbolicLink;
	    break;

	default:
	    goto Exit;
    }

    code = vol->AllocFid(type, &NewFid, &AllocHost, (vuid_t) uid, 1);
    if (code == 0) {
	    Recov_BeginTrans();
	    vol->FidsRealloced++;

	    /* Translate fids in log records. */
	    log->TranslateFid(&OldFid, &NewFid);

	    /* Translate fid in the FSDB. */
	    if ((code = FSDB->TranslateFid(&OldFid, &NewFid)) != 0)
		    CHOKE("cmlent::realloc: couldn't translate %s -> %s (%d)",
		    FID_(&OldFid), FID_2(&NewFid), code);
	    Recov_EndTrans(MAXFP);
    }

Exit:
    return(code);
}


/* MUST be called from within transaction! */
void cmlent::translatefid(ViceFid *OldFid, ViceFid *NewFid) {
    int found = 0;		    /* sanity checking */
    RVMLIB_REC_OBJECT(u);
    switch(opcode) {
	case OLDCML_NewStore_OP:
	    if (FID_EQ(&u.u_store.Fid, OldFid))
	    { u.u_store.Fid = *NewFid; found = 1; }
	    break;

	case OLDCML_Utimes_OP:
	    if (FID_EQ(&u.u_utimes.Fid, OldFid))
	    { u.u_utimes.Fid = *NewFid; found = 1; }
	    break;

	case OLDCML_Chown_OP:
	    if (FID_EQ(&u.u_chown.Fid, OldFid))
	    { u.u_chown.Fid = *NewFid; found = 1; }
	    break;

	case OLDCML_Chmod_OP:
	    if (FID_EQ(&u.u_chmod.Fid, OldFid))
	    { u.u_chmod.Fid = *NewFid; found = 1; }
	    break;

	case OLDCML_Create_OP:
	    if (FID_EQ(&u.u_create.PFid, OldFid))
	    { u.u_create.PFid = *NewFid; found = 1; }
	    if (FID_EQ(&u.u_create.CFid, OldFid))
	    { u.u_create.CFid = *NewFid; found = 1; }
	    break;

	case OLDCML_Remove_OP:
	    if (FID_EQ(&u.u_remove.PFid, OldFid))
	    { u.u_remove.PFid = *NewFid; found = 1; }
	    if (FID_EQ(&u.u_remove.CFid, OldFid))
	    { u.u_remove.CFid = *NewFid; found = 1; }
	    break;

	case OLDCML_Link_OP:
	    if (FID_EQ(&u.u_link.PFid, OldFid))
	    { u.u_link.PFid = *NewFid; found = 1; }
	    if (FID_EQ(&u.u_link.CFid, OldFid))
	    { u.u_link.CFid = *NewFid; found = 1; }
	    break;

	case OLDCML_Rename_OP:
	    if (FID_EQ(&u.u_rename.SPFid, OldFid))
	    { u.u_rename.SPFid = *NewFid; found = 1; }
	    if (FID_EQ(&u.u_rename.TPFid, OldFid))
	    { u.u_rename.TPFid = *NewFid; found = 1; }
	    if (FID_EQ(&u.u_rename.SFid, OldFid))
	    { u.u_rename.SFid = *NewFid; found = 1; }
	    break;

	case OLDCML_MakeDir_OP:
	    if (FID_EQ(&u.u_mkdir.PFid, OldFid))
	    { u.u_mkdir.PFid = *NewFid; found = 1; }
	    if (FID_EQ(&u.u_mkdir.CFid, OldFid))
	    { u.u_mkdir.CFid = *NewFid; found = 1; }
	    break;

	case OLDCML_RemoveDir_OP:
	    if (FID_EQ(&u.u_rmdir.PFid, OldFid))
	    { u.u_rmdir.PFid = *NewFid; found = 1; }
	    if (FID_EQ(&u.u_rmdir.CFid, OldFid))
	    { u.u_rmdir.CFid = *NewFid; found = 1; }
	    break;

	case OLDCML_SymLink_OP:
	    if (FID_EQ(&u.u_symlink.PFid, OldFid))
	    { u.u_symlink.PFid = *NewFid; found = 1; }
	    if (FID_EQ(&u.u_symlink.CFid, OldFid))
	    { u.u_symlink.CFid = *NewFid; found = 1; }
	    break;

        case OLDCML_Repair_OP: /* Shouldn't be called for repair */ 
	default:
	    CHOKE("cmlent::translatefid: bogus opcode (%d)", opcode);
    }
    if (!found) {
	print(logFile);
	CHOKE("cmlent::translatefid: (%x.%x.%x) not matched",
	      OldFid->Volume, OldFid->Vnode, OldFid->Unique);
    }
}


/* local-repair modification */
void cmlent::thread() {
    ViceFid *fids[3];
    ViceVersionVector *vvs[3];

    GetVVandFids(vvs, fids);
    for (int i = 0; i < 3; i++) {
	ViceFid *fidp = fids[i];
	if (fidp == 0) break;

	fsobj *f = FSDB->Find(fidp);
	if (f == 0) {
	    print(logFile);
	    (strbase(volent, log, CML))->print(logFile);
	    CHOKE("cmlent::thread: can't find (%x.%x.%x)",
		fidp->Volume, fidp->Vnode, fidp->Unique);
	}

	/* Thread the VVs. */
	if (vvs[i] != 0) {
	    *(vvs[i]) = f->stat.VV;
	    vvs[i]->StoreId = f->tSid;
	}
	f->tSid = sid;
    }
}


/* local-repair modification */
/* Computes amount of space a record will require when packed 
   into an RPC buffer. */
int cmlent::size() 
{
    int len = 0;
    ViceStatus DummyStatus;
    RPC2_CountedBS DummyCBS;
    DummyCBS.SeqLen = 0;
    DummyCBS.SeqBody = 0;
    RPC2_Unsigned DummyAllocHost = (unsigned long)-1;

    len	+= (int) sizeof(RPC2_Integer);	/* Leave room for opcode. */
    len	+= (int) sizeof(Date_t);		/* Leave room for modify time. */
    switch(opcode) {
	case OLDCML_NewStore_OP:
	case OLDCML_Utimes_OP:
	case OLDCML_Chown_OP:
	case OLDCML_Chmod_OP:
	    len += RLE_Size(OLDCML_NewStore_PTR, &NullFid,
			    0, &DummyCBS, &DummyStatus, 0, 0, /* account for Mask */
			    0, &sid, &DummyCBS, 0);
	    break;

	case OLDCML_Create_OP:
	    len += RLE_Size(OLDCML_Create_PTR, &u.u_create.PFid,
			    &NullFid, u.u_create.Name, &DummyStatus,
			    &u.u_create.CFid, &DummyStatus,
			    DummyAllocHost, &sid, &DummyCBS);
	    break;

	case OLDCML_Remove_OP:
	    len += RLE_Size(OLDCML_Remove_PTR, &u.u_remove.PFid,
			    u.u_remove.Name, &DummyStatus,
			    &DummyStatus, 0, &sid, &DummyCBS);
	    break;

	case OLDCML_Link_OP:
	    len += RLE_Size(OLDCML_Link_PTR, &u.u_link.PFid,
			    u.u_link.Name, &u.u_link.CFid,
			    &DummyStatus, &DummyStatus,
			    0, &sid, &DummyCBS);
	    break;

	case OLDCML_Rename_OP:
	    len += RLE_Size(OLDCML_Rename_PTR, &u.u_rename.SPFid,
			    u.u_rename.OldName, &u.u_rename.TPFid,
			    u.u_rename.NewName, &DummyStatus,
			    &DummyStatus, &DummyStatus,
			    &DummyStatus, 0, &sid, &DummyCBS);
	    break;

	case OLDCML_MakeDir_OP:
	    len += RLE_Size(OLDCML_MakeDir_PTR, &u.u_mkdir.PFid,
			    u.u_mkdir.Name, &DummyStatus,
			    &u.u_mkdir.CFid, &DummyStatus,
			    DummyAllocHost, &sid, &DummyCBS);
	    break;

	case OLDCML_RemoveDir_OP:
	    len += RLE_Size(OLDCML_RemoveDir_PTR, &u.u_rmdir.PFid,
			    u.u_rmdir.Name, &DummyStatus,
			    &DummyStatus, 0, &sid, &DummyCBS);
	    break;

	case OLDCML_SymLink_OP:
	    len += RLE_Size(OLDCML_SymLink_PTR, &u.u_symlink.PFid,
			    u.u_symlink.NewName, u.u_symlink.OldName,
			    &u.u_symlink.CFid, &DummyStatus,
			    &DummyStatus, DummyAllocHost, &sid, &DummyCBS);
	    break;

        case OLDCML_Repair_OP:
	    len += RLE_Size(OLDCML_Repair_PTR, &u.u_repair.Fid,
			    &DummyStatus, 0);
	    break;

	default:
	    CHOKE("cmlent::size: bogus opcode (%d)", opcode);
    }

    return(len);
}


/* local-repair modification */
/* Pack this record into an RPC buffer for transmission to the server. */
void cmlent::pack(PARM **_ptr) {
    /* We MUST recompute the size here since the MRPC size-computing routines */
    /* modify static variables which are used in packing (i.e., XXX_PTR)! */
    (void)size();

    RPC2_CountedBS DummyCBS;
    DummyCBS.SeqLen = 0;
    DummyCBS.SeqBody = 0;
    RPC2_Unsigned DummyAllocHost = (unsigned long)-1;

    /* Vice interface does not yet know about {Truncate, Utimes, Chown, Chmod}, */
    /* so we must make them look like (StatusOnly) Stores. XXX */
    if (opcode == OLDCML_Utimes_OP || opcode == OLDCML_Chown_OP || opcode ==
        OLDCML_Chmod_OP)
	*((RPC2_Integer *)(*_ptr)++) = htonl(OLDCML_NewStore_OP);
    else
	*((RPC2_Integer *)(*_ptr)++) = htonl(opcode);	/* Stick in opcode. */
    *((Date_t	*)(*_ptr)++) = htonl(time);			/* Stick in modify time. */
    switch(opcode) {
	case OLDCML_NewStore_OP:
	    {
	    ViceStatus Status;
	    Status.VV = u.u_store.VV;
	    Status.Date = time;
	    RLE_Pack(_ptr, OLDCML_NewStore_PTR, &u.u_store.Fid,
		     StoreStatusData, &DummyCBS, &Status,
		     u.u_store.Length, 0, 0/* Mask */, &sid, &DummyCBS, 0);
	    }
	    break;

	case OLDCML_Utimes_OP:
	    {
	    ViceStatus Status;
	    Status.VV = u.u_utimes.VV;
	    Status.Date = u.u_utimes.Date;
	    RLE_Pack(_ptr, OLDCML_Utimes_PTR, &u.u_utimes.Fid,
		     StoreStatus, &DummyCBS, &Status, 0, SET_TIME/* Mask */,
		     0, &sid, &DummyCBS, 0);
	    }
	    break;

	case OLDCML_Chown_OP:
	    {
	    ViceStatus Status;
	    Status.VV = u.u_chown.VV;
	    Status.Owner = u.u_chown.Owner;
	    RLE_Pack(_ptr, OLDCML_Chown_PTR, &u.u_chown.Fid,
		     StoreStatus, &DummyCBS, &Status, 0, SET_OWNER/* Mask */,
		     0, &sid, &DummyCBS, 0);
	    }
	    break;

	case OLDCML_Chmod_OP:
	    {
	    ViceStatus Status;
	    Status.VV = u.u_chmod.VV;
	    Status.Mode = u.u_chmod.Mode;
	    RLE_Pack(_ptr, OLDCML_Chmod_PTR, &u.u_chmod.Fid,
		     StoreStatus, &DummyCBS, &Status, 0, SET_MODE/* Mask */,
		     0, &sid, &DummyCBS, 0);
	    }
	    break;

	case OLDCML_Create_OP:
	    {
	    ViceStatus PStatus;
	    PStatus.VV = u.u_create.PVV;
	    PStatus.Date = time;
	    ViceStatus CStatus;
	    CStatus.VV = NullVV;
	    CStatus.DataVersion = 0;
	    CStatus.Length = 0;
	    CStatus.Date = time;
	    CStatus.Owner = uid;
	    CStatus.Mode = u.u_create.Mode;
	    RLE_Pack(_ptr, OLDCML_Create_PTR, &u.u_create.PFid,
		     &NullFid, u.u_create.Name, &CStatus,
		     &u.u_create.CFid, &PStatus, DummyAllocHost,
		     &sid, &DummyCBS);
	    }
	    break;

	case OLDCML_Remove_OP:
	    {
	    ViceStatus PStatus;
	    PStatus.VV = u.u_remove.PVV;
	    PStatus.Date = time;
	    ViceStatus CStatus;
	    CStatus.VV = u.u_remove.CVV;
	    RLE_Pack(_ptr, OLDCML_Remove_PTR, &u.u_remove.PFid,
		     u.u_remove.Name, &PStatus, &CStatus,
		     0, &sid, &DummyCBS);
	    }
	    break;

	case OLDCML_Link_OP:
	    {
	    ViceStatus PStatus;
	    PStatus.VV = u.u_link.PVV;
	    PStatus.Date = time;
	    ViceStatus CStatus;
	    CStatus.VV = u.u_link.CVV;
	    RLE_Pack(_ptr, OLDCML_Link_PTR, &u.u_link.PFid,
		     u.u_link.Name, &u.u_link.CFid, &CStatus, &PStatus,
		     0, &sid, &DummyCBS);
	    }
	    break;

	case OLDCML_Rename_OP:
	    {
	    ViceStatus SPStatus;
	    SPStatus.VV = u.u_rename.SPVV;
	    SPStatus.Date = time;
	    ViceStatus TPStatus;
	    TPStatus.VV = (FID_EQ(&u.u_rename.SPFid, &u.u_rename.TPFid)
			   ? u.u_rename.SPVV : u.u_rename.TPVV);
	    ViceStatus SStatus;
	    SStatus.VV = u.u_rename.SVV;
	    ViceStatus TStatus;
	    RLE_Pack(_ptr, OLDCML_Rename_PTR, &u.u_rename.SPFid,
		     u.u_rename.OldName, &u.u_rename.TPFid,
		     u.u_rename.NewName, &SPStatus, &TPStatus,
		     &SStatus, &TStatus, 0, &sid, &DummyCBS);
	    }
	    break;

	case OLDCML_MakeDir_OP:
	    {
	    ViceStatus PStatus;
	    PStatus.VV = u.u_mkdir.PVV;
	    PStatus.Date = time;
	    ViceStatus CStatus;
	    CStatus.VV = NullVV;
	    CStatus.DataVersion = 1;
	    CStatus.Length = 0;
	    CStatus.Date = time;
	    CStatus.Owner = uid;
	    CStatus.Mode = u.u_mkdir.Mode;
	    RLE_Pack(_ptr, OLDCML_MakeDir_PTR, &u.u_mkdir.PFid,
		     u.u_mkdir.Name, &CStatus, &u.u_mkdir.CFid, &PStatus,
		     DummyAllocHost, &sid, &DummyCBS);
	    }
	    break;

	case OLDCML_RemoveDir_OP:
	    {
	    ViceStatus PStatus;
	    PStatus.VV = u.u_rmdir.PVV;
	    PStatus.Date = time;
	    ViceStatus CStatus;
	    CStatus.VV = u.u_rmdir.CVV;
	    RLE_Pack(_ptr, OLDCML_RemoveDir_PTR, &u.u_rmdir.PFid,
		     u.u_rmdir.Name, &PStatus, &CStatus, 0, &sid, &DummyCBS);
	    }
	    break;

	case OLDCML_SymLink_OP:
	    {
	    ViceStatus PStatus;
	    PStatus.VV = u.u_symlink.PVV;
	    PStatus.Date = time;
	    ViceStatus CStatus;
	    CStatus.VV = NullVV;
	    CStatus.DataVersion = 1;
	    CStatus.Length = 0;
	    CStatus.Date = time;
	    CStatus.Owner = uid;
	    CStatus.Mode = u.u_symlink.Mode;
	    RLE_Pack(_ptr, OLDCML_SymLink_PTR, &u.u_symlink.PFid,
		     u.u_symlink.NewName, u.u_symlink.OldName,
		     &u.u_symlink.CFid, &CStatus, &PStatus,
		     DummyAllocHost, &sid, &DummyCBS);
	    }
	    break;

        case OLDCML_Repair_OP:
	    {
		ViceStatus Status;
		Status.Length = u.u_repair.Length;
		Status.Date = u.u_repair.Date;
		Status.Author = Status.Owner = u.u_repair.Owner;
		Status.Mode = u.u_repair.Mode;
		RLE_Pack(_ptr, OLDCML_Repair_PTR, &u.u_repair.Fid,
			 &Status, &sid, 0);
	    }
	    break;

	default:
	    CHOKE("cmlent::pack: bogus opcode (%d)", opcode);
    }
}


/* local-repair modification */
/* MUST be called from within transaction! */
void cmlent::commit(ViceVersionVector *UpdateSet) {
    LOG(1, ("cmlent::commit: (%d)\n", tid));

    volent *vol = strbase(volent, log, CML);
    vol->RecordsCommitted++;

    /* 
     * Record StoreId/UpdateSet for objects involved in this operation ONLY 
     * when this is the  FINAL mutation of the object.  Record a COP2 entry 
     * only if this operation was final for ANY object! 
     * Because of the addition of incremental reintegration, the final 
     * mutation should be checked only within the bound of a single unit
     * (identified by cmlent::tid) -luqi
     */
    int FinalMutationForAnyObject = 0;

    dlist_iterator next(*fid_bindings);
    dlink *d;
    while ((d = next())) {
	binding *b = strbase(binding, d, binder_handle);
	fsobj *f = (fsobj *)b->bindee;
	CODA_ASSERT(f && (f->MagicNumber == FSO_MagicNumber));  /* better be an fso */

	cmlent *FinalCmlent = f->FinalCmlent(tid);
	if (FinalCmlent == this) {
	    LOG(10, ("cmlent::commit: FinalCmlent for 0x%x.%x.%x\n",
		     f->fid.Volume, f->fid.Vnode, f->fid.Unique));
	    /* 
	     * if the final update removed the object, don't bother adding the
	     * COP2, but do update the version vector as in connected mode.
	     */
	    if (!(opcode == OLDCML_Remove_OP && FID_EQ(&u.u_remove.CFid, &f->fid)) &&
		!(opcode == OLDCML_RemoveDir_OP && FID_EQ(&u.u_rmdir.CFid, &f->fid)))
		FinalMutationForAnyObject = 1;

	    RVMLIB_REC_OBJECT(f->stat.VV);
	    f->stat.VV.StoreId = sid;
	    AddVVs(&f->stat.VV, UpdateSet);

	    if (vol->flags.resolve_me) 
		vol->ResSubmit(0, &f->fid);
	}
    }
    if (FinalMutationForAnyObject) {
	LOG(10, ("cmlent::commit: Add COP2 with sid = 0x%x.%x\n",
		 sid.Host, sid.Uniquifier));	
	vol->AddCOP2(&sid, UpdateSet);
    }

    delete this;
}


int cmlent::HaveReintegrationHandle() {
    int haveit = 0;

    if (opcode == OLDCML_NewStore_OP)
	for (int i = 0; i < VSG_MEMBERS; i++) 
	    if (u.u_store.RHandle[i].BirthTime)
		haveit++;

    return(haveit);
}


/* MUST NOT be called from within transaction! */
void cmlent::ClearReintegrationHandle() {
    CODA_ASSERT(opcode == OLDCML_NewStore_OP);

    Recov_BeginTrans();
	RVMLIB_REC_OBJECT(u);

        bzero((void *)u.u_store.RHandle, (int) sizeof(ViceReintHandle)*VSG_MEMBERS);
	u.u_store.Offset = -1;
   Recov_EndTrans(MAXFP);
}


int cmlent::DoneSending() { 
    int done = 0;

    if (HaveReintegrationHandle() &&
	(u.u_store.Offset == u.u_store.Length))
	done = 1;
    
    return(done);
}


int cmlent::GetReintegrationHandle() {
    volent *vol = strbase(volent, log, CML);
    int code = 0;
    mgrpent *m = 0;
    int i;
    
    /* Acquire an Mgroup. */
    code = vol->GetMgrp(&m, log->owner);
    if (code != 0) goto Exit;

    {
	ViceReintHandle VR;	/* dummy variable */
	ARG_MARSHALL(OUT_MODE, ViceReintHandle, VR, VR, m->nhosts);

	/* Make the RPC call. */
	MarinerLog("store::OpenReintHandle %s\n", vol->name);
	MULTI_START_MESSAGE(ViceOpenReintHandle_OP);
	code = (int) MRPC_MakeMulti(ViceOpenReintHandle_OP,
				    ViceOpenReintHandle_PTR,
				    m->nhosts, m->rocc.handles,
				    m->rocc.retcodes, m->rocc.MIp, 0, 0,
				    &u.u_store.Fid, VR_ptrs);
	MULTI_END_MESSAGE(ViceOpenReintHandle_OP);
	MarinerLog("store::openreinthandle done\n");

	/* Examine the return code to decide what to do next. */
	code = vol->Collate_NonMutating(m, code);
	MULTI_RECORD_STATS(ViceOpenReintHandle_OP);

	if (code != 0) goto Exit;

	/* unmarshall all of the handles */
	Recov_BeginTrans();
	    RVMLIB_REC_OBJECT(u);
	    for (i = 0; i < m->nhosts; i++)
	        u.u_store.RHandle[i] = VR_bufs[i];
	Recov_EndTrans(MAXFP);
    }

Exit:
    PutMgrp(&m);
    LOG(0, ("cmlent::GetReintegrationHandle: (%s), returns %s\n",
	     vol->name, VenusRetStr(code)));
    return(code);
}


int cmlent::ValidateReintegrationHandle() {
    volent *vol = strbase(volent, log, CML);
    int code = 0;
    unsigned int i = 0;
    mgrpent *m = 0;
    RPC2_Integer Offset = -1;
    
    /* Acquire an Mgroup. */
    code = vol->GetMgrp(&m, log->owner);
    if (code != 0) goto Exit;

    {
	ARG_MARSHALL(OUT_MODE, RPC2_Integer, Offsetvar, Offset, m->nhosts);

	/* Make the RPC call. */
	MarinerLog("store::QueryReintHandle %s\n", vol->name);
	MULTI_START_MESSAGE(ViceQueryReintHandle_OP);
	code = (int) MRPC_MakeMulti(ViceQueryReintHandle_OP,
				    ViceQueryReintHandle_PTR,
				    m->nhosts, m->rocc.handles,
				    m->rocc.retcodes, m->rocc.MIp, 0, 0,
				    vol->vid, m->nhosts, u.u_store.RHandle, 
				    Offsetvar_ptrs);
	MULTI_END_MESSAGE(ViceQueryReintHandle_OP);
	MarinerLog("store::queryreinthandle done\n");

	/* Examine the return code to decide what to do next. */
	code = vol->Collate_NonMutating(m, code);
	MULTI_RECORD_STATS(ViceQueryReintHandle_OP);

	if (code != 0) goto Exit;

	for (i = 0; i < m->nhosts; i++)
	    if (m->rocc.hosts[i]) 
		if (Offset == -1 || Offset > Offsetvar_bufs[i])
		    Offset = Offsetvar_bufs[i];

	if (Offset > u.u_store.Length)
	    CHOKE("cmlent::QueryReintegrationHandle: offset > length! (%d, %d)\n",
		  Offset, u.u_store.Length);

	Recov_BeginTrans();
	    RVMLIB_REC_OBJECT(u);   
	    u.u_store.Offset = Offset;
	Recov_EndTrans(MAXFP);
    }

Exit:
	PutMgrp(&m);
    LOG(0, ("cmlent::QueryReintegrationHandle: (%s), returns %s, offset %d\n",
	     vol->name, VenusRetStr(code), Offset));
    return(code);
}


int cmlent::WriteReintegrationHandle() {
    CODA_ASSERT(opcode == OLDCML_NewStore_OP);

    volent *vol = strbase(volent, log, CML);
    int code = 0, bytes = 0;
    mgrpent *m = 0;
    RPC2_Unsigned length = ReintAmount();

    /* Acquire an Mgroup. */
    code = vol->GetMgrp(&m, log->owner);
    if (code != 0) goto Exit;

    {
	/* get the fso associated with this record */
	binding *b = strbase(binding, fid_bindings->first(), binder_handle);
	fsobj *f = (fsobj *)b->bindee;
	if (f == 0)
	    { code = ENOENT; goto Exit; }

	if (f->readers <= 0 && !f->shadow) 
	    CHOKE("cmlent::WriteReintegrationHandle: object not locked! (%x.%x.%x)\n",
		  f->fid.Volume, f->fid.Vnode, f->fid.Unique);

	/* Sanity checks. */
	if (!f->IsFile() || !HAVEALLDATA(f)) {
	    code = EINVAL;
	    goto Exit;
	}

	/* Set up the SE descriptor. */
	SE_Descriptor sed;
        memset(&sed, 0, sizeof(SE_Descriptor));
	{
	    sed.Tag = SMARTFTP;
	    struct SFTP_Descriptor *sei = &sed.Value.SmartFTPD;
	    sei->TransmissionDirection = CLIENTTOSERVER;
	    sei->hashmark = 0;
	    sei->SeekOffset = u.u_store.Offset;
	    sei->ByteQuota = length;
	    sei->Tag = FILEBYNAME;
	    sei->FileInfo.ByName.ProtectionBits = 0666;
	    /* if the object has already been written, use the shadow */
	    if (f->shadow) 
		strcpy(sei->FileInfo.ByName.LocalFileName, f->shadow->Name());
	    else
		strcpy(sei->FileInfo.ByName.LocalFileName, f->data.file->Name());
	}

	/* Notify Codacon */
	{
	    char *comp = f->comp;
	    char buf[CODA_MAXNAMLEN];
	    if (comp[0] == '\0') {
		sprintf(buf, "%s", FID_(&f->fid));
		comp = buf;
	    }

	    MarinerLog("store::SendReintFragment %s, %s [%d] (%d/%d)\n", 
		       vol->name, comp, NBLOCKS(length), 
		       u.u_store.Offset, u.u_store.Length);
	}

	/* 
	 * if the volume is weakly connected and more than one server
	 * is available, send data to one.
	 */
	if (vol->IsWeaklyConnected() && m->rocc.HowMany > 1) {
	    /* Pick a server and get a connection to it. */
	    int ph_ix;
	    unsigned long ph = m->GetPrimaryHost(&ph_ix);
	    CODA_ASSERT(ph != 0);

	    connent *c = 0;
	    code = ::GetConn(&c, ph, log->owner, 0);
	    if (code != 0) goto Exit;

	    /* Make the RPC call. */
	    UNI_START_MESSAGE(ViceSendReintFragment_OP);
	    code = (int) ViceSendReintFragment(c->connid, vol->vid, 
					       m->nhosts, u.u_store.RHandle, 
					       length, &sed);
	    UNI_END_MESSAGE(ViceSendReintFragment_OP);
	    MarinerLog("store::sendreintfragment done\n");

	    code = vol->Collate(c, code);
	    UNI_RECORD_STATS(ViceSendReintFragment_OP);

	    if (code != 0) goto Exit;

	    PutConn(&c);
	    bytes = sed.Value.SmartFTPD.BytesTransferred;

	} else {
	    /* Make multiple copies of the IN/OUT and OUT parameters. */
	    ARG_MARSHALL(IN_OUT_MODE, SE_Descriptor, sedvar, sed, m->nhosts);

	    /* Make the RPC call. */
	    MULTI_START_MESSAGE(ViceSendReintFragment_OP);
	    code = (int) MRPC_MakeMulti(ViceSendReintFragment_OP,
					ViceSendReintFragment_PTR,
					m->nhosts, m->rocc.handles,
					m->rocc.retcodes, m->rocc.MIp, 0, 0,
					vol->vid, m->nhosts, u.u_store.RHandle, 
					length, sedvar_bufs);
	    MULTI_END_MESSAGE(ViceSendReintFragment_OP);
	    MarinerLog("store::sendreintfragment done\n");

	    /* Examine the return code to decide what to do next. */
	    code = vol->Collate_NonMutating(m, code);
	    MULTI_RECORD_STATS(ViceSendReintFragment_OP);

	    if (code != 0) goto Exit;

	    int dh_ix; dh_ix = -1;
	    (void)m->DHCheck(0, -1, &dh_ix);
	    bytes = sedvar_bufs[dh_ix].Value.SmartFTPD.BytesTransferred;
	}

	if ((long)length != bytes) 
	    CHOKE("cmlent::WriteReintegrateHandle: bytes mismatch (%d, %d)\n",
		    length, bytes);

	Recov_BeginTrans();
	    RVMLIB_REC_OBJECT(u);
	    if (u.u_store.Offset == -1) 
	       u.u_store.Offset = length;
	    else 
	       u.u_store.Offset += length;
	Recov_EndTrans(MAXFP);
    }

 Exit:
    PutMgrp(&m);
    LOG(0, ("cmlent::WriteReintegrateHandle: (%s), %d bytes, returns %s, new offset %d\n",
	     vol->name, length, VenusRetStr(code), u.u_store.Offset));
    return(code);
}


int cmlent::CloseReintegrationHandle(char *buf, int bufsize, 
				     ViceVersionVector *UpdateSet) {
    volent *vol = strbase(volent, log, CML);
    int code = 0;
    mgrpent *m = 0;
    
    /* Set up the SE descriptor. */
    SE_Descriptor sed;
    memset(&sed, 0, sizeof(SE_Descriptor));
    sed.Tag = SMARTFTP;
    struct SFTP_Descriptor *sei = &sed.Value.SmartFTPD;
    sei->TransmissionDirection = CLIENTTOSERVER;
    sei->hashmark = 0;
    sei->SeekOffset = 0;
    sei->ByteQuota = -1;
    sei->Tag = FILEINVM;
    sei->FileInfo.ByAddr.vmfile.SeqLen = bufsize;
    sei->FileInfo.ByAddr.vmfile.SeqBody = (RPC2_ByteSeq)buf;

    /* COP2 Piggybacking. */
    long cbtemp; cbtemp = cbbreaks;
    char PiggyData[COP2SIZE];
    RPC2_CountedBS PiggyBS;
    PiggyBS.SeqLen = 0;
    PiggyBS.SeqBody = (RPC2_ByteSeq)PiggyData;
    RPC2_Integer VS = 0;
    CallBackStatus VCBStatus = NoCallBack;

    /* Acquire an Mgroup. */
    code = vol->GetMgrp(&m, log->owner, (PIGGYCOP2 ? &PiggyBS : 0));
    if (code != 0) goto Exit;

    /* 
     * if the volume is weakly connected and more than one server
     * is available, pick one to reintegrate with and resolve
     * objects with the AVSG if successful.
     */
    if (vol->IsWeaklyConnected() && m->rocc.HowMany > 1) {
	/* Pick a server and get a connection to it. */
	int ph_ix;
	unsigned long ph = m->GetPrimaryHost(&ph_ix);
	CODA_ASSERT(ph != 0);

	connent *c = 0;
	code = ::GetConn(&c, ph, log->owner, 0);
	if (code != 0) goto Exit;
	
	/* don't bother with VCBs, will lose them on resolve anyway */
	RPC2_CountedBS OldVS; 
	OldVS.SeqLen = 0;
	vol->ClearCallBack();

	/* Make the RPC call. */
	MarinerLog("store::CloseReintHandle %s, (%d)\n", vol->name, bufsize);
	UNI_START_MESSAGE(ViceCloseReintHandle_OP);
	code = (int) ViceCloseReintHandle(c->connid, vol->vid, bufsize,
				    m->nhosts, u.u_store.RHandle, 
				    &OldVS, &VS, &VCBStatus, &PiggyBS, &sed);
	UNI_END_MESSAGE(ViceCloseReintHandle_OP);
	MarinerLog("store::closereinthandle done\n");

	code = vol->Collate(c, code);
	UNI_RECORD_STATS(ViceCloseReintHandle_OP);

	if (code != 0) goto Exit;

	/* Finalize COP2 Piggybacking. */
	if (PIGGYCOP2)
	    vol->ClearCOP2(&PiggyBS);

	LOG(0/*10*/, ("ViceCloseReintegrationHandle: transferred %d bytes\n",
		      sed.Value.SmartFTPD.BytesTransferred));

	/* Fashion the update set. */
	InitVV(UpdateSet);
	(&(UpdateSet->Versions.Site0))[ph_ix] = 1;

	/* Indicate that objects should be resolved on commit. */
	vol->flags.resolve_me = 1;

	PutConn(&c);

    } else {
	RPC2_CountedBS OldVS;
	vol->PackVS(m->nhosts, &OldVS);

	/* Make multiple copies of the IN/OUT and OUT parameters. */
	ARG_MARSHALL(IN_OUT_MODE, SE_Descriptor, sedvar, sed, m->nhosts);
	ARG_MARSHALL(OUT_MODE, RPC2_Integer, VSvar, VS, m->nhosts);
	ARG_MARSHALL(OUT_MODE, CallBackStatus, VCBStatusvar, VCBStatus, m->nhosts);
	/* Make the RPC call. */
	MarinerLog("store::CloseReintHandle %s, (%d)\n", vol->name, bufsize);
	MULTI_START_MESSAGE(ViceCloseReintHandle_OP);
	code = (int) MRPC_MakeMulti(ViceCloseReintHandle_OP,
				    ViceCloseReintHandle_PTR,
				    m->nhosts, m->rocc.handles,
				    m->rocc.retcodes, m->rocc.MIp, 0, 0,
				    vol->vid, bufsize, m->nhosts,
				    u.u_store.RHandle, 
				    &OldVS, VSvar_ptrs, VCBStatusvar_ptrs,
				    &PiggyBS, sedvar_bufs);
	MULTI_END_MESSAGE(ViceCloseReintHandle_OP);
	MarinerLog("store::closereinthandle done\n");

	free(OldVS.SeqBody);

	/* Examine the return code to decide what to do next. */
	code = vol->Collate_Reintegrate(m, code, UpdateSet);
	MULTI_RECORD_STATS(ViceCloseReintHandle_OP);

	if (code == EASYRESOLVE) { code = 0; }
	if (code != 0) goto Exit;

	/* Collate volume callback information */
	if (cbtemp == cbbreaks)
	    vol->CollateVCB(m, VSvar_bufs, VCBStatusvar_bufs);

	/* Finalize COP2 Piggybacking. */
	if (PIGGYCOP2) {
	    vol->ClearCOP2(&PiggyBS);
	}

	/* Manually compute the OUT parameters from the mgrpent::Reintegrate() call! -JJK */
	int dh_ix; dh_ix = -1;
	(void)m->DHCheck(0, -1, &dh_ix);
	LOG(0/*10*/, ("ViceCloseReintegrationHandle: transferred %d bytes\n",
		      sedvar_bufs[dh_ix].Value.SmartFTPD.BytesTransferred));
    }

Exit:
    PutMgrp(&m);
    LOG(0, ("cmlent::CloseReintegrationHandle: (%s), %d bytes, returns %s\n", 
	    vol->name, bufsize, VenusRetStr(code)));
    return(code);
}


/* Compute the size required for a ReintegrationLog Entry. */
/* Patterned after code in MRPC_MakeMulti(). */
static int RLE_Size(ARG *ArgTypes ...) 
{
    int len = 0;

    va_list ap;
    va_start(ap, ArgTypes);
    /*  In GNU C, unions are not passed on the stack. Not even four
     * byte ones.  If we try to get a PARM from va_arg, GNU C will
     * treat the four bytes on the stack as a pointer (because unions
     * are "big"!).  So we mislead it to get the four bytes off the
     * stack sans dereferencing.  */

    PARM *args = (PARM *) &(va_arg(ap, unsigned long));
    for	(ARG *a_types =	ArgTypes; a_types->mode	!= C_END; a_types++, args = (PARM *) &(va_arg(ap, unsigned long))) {
	LOG(1000, ("RLE_Size: a_types = [%d %d %d %x], args = (%x %x)\n",
		   a_types->mode, a_types->type, a_types->size, a_types->field,
		   args, *args));

	if (a_types->mode != IN_MODE && a_types->mode != IN_OUT_MODE)
	    continue;

	/* Extra level of indirection for IN/OUT args! */
	PARM *targs = (PARM *)&args;
	PARM *xargs = ( a_types->mode == IN_OUT_MODE) ? targs : args;

	a_types->bound = 0;
	if (a_types->type == RPC2_STRUCT_TAG)
	    len += struct_len(&a_types, &xargs);
	else
	    len += get_len(&a_types, &xargs, a_types->mode);
    }

    va_end(ap);
    return(len);
}


/* Pack a ReintegrationLog Entry. */
/* Patterned after code in MRPC_MakeMulti(). */
static void RLE_Pack(PARM **ptr, ARG *ArgTypes ...) {
    va_list ap;
    va_start(ap, ArgTypes);

    /* see comment about GNU C above. */
    PARM *args = (PARM *) &(va_arg(ap, unsigned long));
    for	(ARG *a_types =	ArgTypes; a_types->mode	!= C_END; a_types++, args = (PARM *) &(va_arg(ap, unsigned long))) {
	LOG(1000, ("RLE_Pack: a_types = [%d %d %d %x], ptr = (%x %x %x), args = (%x %x)\n",
		   a_types->mode, a_types->type, a_types->size, a_types->field,
		   ptr, *ptr, **ptr, args, *args));

	if (a_types->mode != IN_MODE && a_types->mode != IN_OUT_MODE)
	    continue;

	/* Extra level of indirection for IN/OUT args! */
	PARM *targs = (PARM *)&args;
	PARM *xargs = (a_types->mode == IN_OUT_MODE) ? targs : args;

	if (a_types->type == RPC2_STRUCT_TAG)
	    pack_struct(a_types, &xargs, (PARM **)ptr);
	else
	    pack(a_types, &xargs, (PARM **)ptr);
    }

    va_end(ap);
}


/*  *****  Routines for Handling Inconsistencies and Safeguarding Against Catastrophe  *****  */

/* These definitions are stolen from tar.c. */
#define	TBLOCK	512
#define NBLOCK	20
#define	NAMSIZ	100

union hblock {
    char dummy[TBLOCK];
    struct header {
	char name[NAMSIZ];
	char mode[8];
	char uid[8];
	char gid[8];
	char size[12];
	char mtime[12];
	char chksum[8];
	char linkflag;
	char linkname[NAMSIZ];
    } dbuf;
};

static void GetPath(char *, ViceFid *, char * =0);
static int WriteHeader(FILE *, hblock&);
static int WriteData(FILE *, char *);
static int WriteTrailer(FILE *);

int PathAltered(ViceFid *cfid, char *suffix, ClientModifyLog *CML, cmlent *starter)
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
	if (m->opcode == OLDCML_Remove_OP && FID_EQ(&m->u.u_remove.CFid, cfid)) {
	    /* when the cfid is removed, prepend suffix and replace cfid with its father */
	    if (suffix[0]) 
	      sprintf(buf, "%s/%s", m->u.u_remove.Name, suffix);
	    else 
	      sprintf(buf, "%s", m->u.u_remove.Name);
	    strcpy(suffix, buf);
	    bcopy((const void *)&m->u.u_remove.PFid, (void *) cfid, (int) sizeof(ViceFid));
	    return 1;
	}

	if (m->opcode == OLDCML_RemoveDir_OP && FID_EQ(&m->u.u_rmdir.CFid, cfid)) {
	    /*
	     * when the current fid(directory) is removed, prepend suffix
	     * replace cfid with its father.
	     */
	    if (suffix[0])
	      sprintf(buf, "%s/%s", m->u.u_rmdir.Name, suffix);
	    else
	      sprintf(buf, "%s", m->u.u_rmdir.Name);
	    strcpy(suffix, buf);
	    bcopy((const void *)&m->u.u_rmdir.PFid, (void *) cfid, (int) sizeof(ViceFid));
	    return 1;
	}

	if (m->opcode == OLDCML_Rename_OP && FID_EQ(&m->u.u_rename.SFid, cfid)) {
	    /*
	     * when the current fid is renamed, prepend the original name to
	     * suffix and replace cfid with the original father fid.
	     */
	    if (suffix[0])
	      sprintf(buf, "%s/%s", m->u.u_rename.OldName, suffix);
	    else
	      sprintf(buf, "%s", m->u.u_rename.OldName);
	    strcpy(suffix, buf);
	    bcopy((const void *)&m->u.u_rename.SPFid, (void *) cfid, (int) sizeof(ViceFid));
	    return 1;
	}
	m = next();
    }
    return 0;
}

/* local-repair modification */
void RecoverPathName(char *path, ViceFid *fid, ClientModifyLog *CML, cmlent *starter)
{
    /* this algorithm is single-volume based */
    CODA_ASSERT(path && fid && CML && starter);
    LOG(100, ("RecoverPathName: fid = 0x.%x.%x.%x\n", fid->Volume, fid->Vnode, fid->Unique));

    ViceFid cfid;
    char suffix[MAXPATHLEN];
    char buf[MAXPATHLEN];

    bcopy((const void *)fid, (void *) &cfid, (int)sizeof(ViceFid));
    suffix[0] = '\0';

    /* the loog invariant is "path(cfid)/suffix == path(fid)" */
    while (! FID_IsVolRoot(&cfid) ) {
	/* while the current fid is root of the volume */
	if (!PathAltered(&cfid, suffix, CML, starter)) {
	    /*
	     * only deal with the situation when cfid has been not removed or renamed.
	     * otherwise, cfid and suffix has alread been adjusted by PathAltered to 	
	     * maintain the loop invariant. Note that the object corresponding to cfid
	     * now is guaranteed to be alive.
	     */
	    fsobj *f = FSDB->Find(&cfid);
	    if (f == NULL) {
		LOG(0, ("RecoverPathName: fid = 0x%x.%x.%x object no cached\n",
			cfid.Volume, cfid.Vnode, cfid.Unique));
		sprintf(path, "???/%s", suffix);
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
		bcopy((const void *)&((f->u.mtpoint)->pfid), (void *) &cfid, (int)sizeof(ViceFid));
	    } else {
		bcopy((const void *)&f->pfid, (void *) &cfid, (int)sizeof(ViceFid));
	    }
	}
    }
    char prefix[MAXPATHLEN];
    fsobj *f = FSDB->Find(&cfid);	/* find the root object of the lowest volume */
    if (f == NULL) {
	LOG(0, ("RecoverPathName: volume root 0x%x.%x.%x not cached\n",
		cfid.Volume, cfid.Vnode, cfid.Unique));
	sprintf(path, "???/%s", suffix);
	return;
    }
    f->GetPath(prefix, 1);
    if (suffix[0])
      sprintf(path, "%s/%s", prefix, suffix);
    else
      sprintf(path, "%s", prefix);
}


int volent::CheckPointMLEs(vuid_t vuid, char *ckpdir) 
{
    if (CML.count() == 0)
	return(ENOENT);
    if (CML.owner != vuid && vuid != V_UID)
	return(EACCES);

    if ( rvmlib_in_transaction() ) {
	    CHOKE("CheckPointMLEs started while in transaction!");
    }

    int code = CML.CheckPoint(ckpdir);
    return(code);
}


/* MUST NOT be called from within transaction! */
int volent::PurgeMLEs(vuid_t vuid) {
    if (CML.count() == 0)
	return(ENOENT);
    if (CML.owner != vuid && vuid != V_UID)
	return(EACCES);
    if (IsReintegrating())
      return EACCES;

    if (LRDB->repair_root_fid && LRDB->repair_root_fid->Volume == vid)
      /* 
       * check if there is on-going local/global repair session that
       * is working on a subtree in this volume.
       * do not proceed if so because we can't remove the subtreee.
       */
      return EACCES;

    LOG(0, ("volent::PurgeMLEs:(%s) (%x)\n", name, vid));

    {
	/* 
	 * Step 1: cleanup every localized subtree whose root object
	 * belongs to this volume.
	 */

	{
	    /* count and record the number of fid-map entries to be removed */
	    int fid_map_entry_cnt = 0;
	    lgm_iterator next(LRDB->local_global_map);
	    lgment *lgm;
	    while ((lgm = next())) {
		if ((lgm->GetGlobalFid())->Volume == vid) fid_map_entry_cnt++;
	    }
	    LOG(0, ("volent::PurgeMLEs: there are %d local-global-map entries to be cleaned\n",
		    fid_map_entry_cnt));
	}

	int subtree_removal;
	do {
	    subtree_removal = 0;
	    rfm_iterator next(LRDB->root_fid_map);
	    rfment *rfm;
	    
	    while ((rfm = next())) {
		if (rfm->RootCovered()) continue;
		ViceFid *RootFid = rfm->GetFakeRootFid();
		if (RootFid->Volume != vid) continue;
		LOG(0, ("volent::PurgeMLEs: remove subtree rooted at 0x%x.%x.%x\n", 
			RootFid->Volume, RootFid->Vnode, RootFid->Unique));
		LRDB->RemoveSubtree(RootFid);
		subtree_removal = 1;
		break;
	    }
	} while (subtree_removal);

	{
	    /* double check to make sure that there is no left over entries for the volume */
	    int left_over_entry;
	    do {
		left_over_entry = 0;
		lgm_iterator next(LRDB->local_global_map);
		lgment *lgm;
		while ((lgm = next())) {
		    if ((lgm->GetGlobalFid())->Volume != vid) continue;
		    LOG(0, ("volent::PurgeMLEs: found a left over entry\n"));
		    lgm->print(logFile);
		    fflush(logFile);
		    left_over_entry = 1;
		    Recov_BeginTrans();
			   OBJ_ASSERT(this, LRDB->local_global_map.remove(lgm) == lgm);
			   delete lgm;
		    Recov_EndTrans(MAXFP);
		    break;
		}
	    } while (left_over_entry);
	}
    }
    {	/* 
	 * Step 2: cleanup everything in the CML, even there are records
	 * marked as to-be-repaired or repair-mutation.
	 */
	cmlent *m;
	rec_dlist_iterator next(CML.list, AbortOrder);
	rec_dlink *d = next();	/* get the first (last) element */
	while (1) {
	    if (!d) break;
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

    {	/*
	 * Step 3: cleanup remaining local object that belongs to this
	 * volume. this is necessary because there could subtree rooted
	 * at another volume but containing local objects within this volume.
	 */
	lgm_iterator next(LRDB->local_global_map);
	lgment *lgm, *to_be_removed = NULL;
	while ((lgm = next())) {
	    if (to_be_removed) {
		VOL_ASSERT(this, LRDB->local_global_map.remove(to_be_removed) == to_be_removed);
		delete to_be_removed;
		to_be_removed = NULL;
	    }
	    ViceFid *gfid = lgm->GetGlobalFid();
	    if (gfid->Volume != vid) continue;
	    ViceFid *lfid = lgm->GetLocalFid();	    
	    fsobj *lobj;
	    VOL_ASSERT(this, lobj = FSDB->Find(lfid));
	    /* kill the local object */
	    Recov_BeginTrans();
		   lobj->Kill(0);
	    Recov_EndTrans(CMFP);
	    to_be_removed = lgm;
	}
    }

    /* trigger a volume state transition */
    flags.transition_pending = 1;
    
    return(0);
}


int volent::LastMLETime(unsigned long *time) {
    if (CML.count() == 0)
	return(ENOENT);

    cmlent *lastmle = strbase(cmlent, CML.list.last(), handle);
    *time = lastmle->time;

    return(0);
}


/* Returns {0, ENOSPC}. */
int ClientModifyLog::CheckPoint(char *ckpdir) {
    volent *vol = strbase(volent, this, CML);
    LOG(1, ("ClientModifyLog::CheckPoint: (%s), cdir = %s\n",
	     vol->name, (ckpdir ? ckpdir : "")));

    int code = 0;

#ifdef DJGPP
    return 0;
#endif

    /* Create the CKP file. */
    /* The last component of the name will be "<volname>@<mountpath>". */
    FILE *dfp = NULL, *ofp = NULL;
    char ckpname[MAXPATHLEN], lname[MAXPATHLEN];

    if (ckpdir)
	strcpy(ckpname, ckpdir);
    else
	MakeUserSpoolDir(ckpname, owner);
    strcat(ckpname, "/");
    strcat(ckpname, vol->name);
    strcat(ckpname, "@");
    char mountpath[MAXPATHLEN];
    vol->GetMountPath(mountpath);
    {
	/* Substitute |'s in mount path for /'s. */
	/* Of course, we're assuming that | is never legitimately used in a mount path! */
	for (char *cp = mountpath; *cp; cp++)
	    if (*cp == '/') *cp = '|';
    }
    strncat(ckpname, mountpath, CODA_MAXNAMLEN - (int) strlen(vol->name) - 1 - 1);
    (void) strcpy(lname, ckpname);
    (void) strcat(ckpname, ".tar");
    (void) strcat(lname, ".cml");

    {
	/* rename the old checkpoint file out of the way, if possible */
	char oldname[MAXPATHLEN];
	(void) strcat(strcpy(oldname, ckpname), ".old");
	(void) ::rename(ckpname, oldname);
	(void) strcat(strcpy(oldname, lname), ".old");
	(void) ::rename(lname, oldname);
    }

    if ((dfp = fopen(ckpname, "w+")) == NULL) {
	eprint("Couldn't open %s for checkpointing", ckpname);
	return(ENOENT);
    }
#ifndef DJGPP
#ifndef __CYGWIN32__
    ::fchown(fileno(dfp), owner, V_GID);
#else
    ::chown(ckpname, owner, V_GID);
#endif
    ::fchmod(fileno(dfp), 0600);
#endif

    if ((ofp = fopen(lname, "w+")) == NULL) {
	eprint("Couldn't open %s for checkpointing", lname);
	return(ENOENT);
    }
#ifndef DJGPP
#ifndef __CYGWIN32__
    ::fchown(fileno(ofp), owner, V_GID);
#else
    ::chown(lname, owner, V_GID);
#endif
   ::fchmod(fileno(ofp), 0600);
#endif

    /* 
     * Iterate through the MLEs (in commit order), checkpointing each in turn. 
     * Lock the CML exclusively to prevent changes during checkpoint.  This is
     * necessary because the thread yields during file write.  If at the time 
     * there is another thread doing mutations to the volume causing some of the 
     * elements in the CML being iterated to be canceled, venus will assertion fail.
     */
    ObtainWriteLock(&vol->CML_lock);
    eprint("Checkpointing %s", vol->name);
    eprint("to %s", ckpname);
    eprint("and %s", lname);
    cml_iterator next(*this, CommitOrder);
    cmlent *m;    
    while ((m = next())) {
	m->writeops(ofp);
	if (code) continue;
	code = m->checkpoint(dfp);
    }
    if (code != 0) { 
	LOG(0, ("checkpointing of %s to %s failed (%d)",
		vol->name, ckpname, code));
	eprint("checkpointing of %s to %s failed (%d)",
	       vol->name, ckpname, code);
    };
    ReleaseWriteLock(&vol->CML_lock);

    /* Write the trailer block and flush the data. */
    if (code == 0) {
	code = WriteTrailer(dfp);
	if (code == 0)
	    code = (fflush(dfp) == EOF ? ENOSPC : 0);
    }

    /* Close the CKP file.  Unlink in the event of any error. */
    if (code == 0) {
	(void)fclose(dfp);
	(void)fclose(ofp);
    }
    else {
	/* fclose() may return EOF in this case, but it will DEFINITELY close() the file descriptor! */
	(void)fclose(dfp);
	(void)fclose(ofp);

	(void)::unlink(ckpname);
	(void)::unlink(lname);

	eprint("Couldn't successfully checkpoint %s and %s", ckpname, lname);
    }

    return(code);
}


/* Invalidate the fsobj's following unsuccessful reintegration. */
/* MUST NOT be called from within transaction! */
void ClientModifyLog::IncAbort(int Tid) {
    volent *vol = strbase(volent, this, CML);
    LOG(0, ("ClientModifyLog::IncAbort: (%s) and tid = %d\n", vol->name, Tid));
    /* eprint("IncAbort CML for %s and tid %d\n", vol->name, Tid); */

    CODA_ASSERT(count() > 0);

    Recov_BeginTrans();
	rec_dlist_iterator next(list, AbortOrder);
	rec_dlink *d = next();		/* get the first (last) element */

	while (1) {
	    if (!d) break;			/* list exhausted */
	    cmlent *m = strbase(cmlent, d, handle);
	    if (m->GetTid() == Tid) {
		m->print(logFile);
		d = next();	/* advance d before it is un-listed by m->abort() */
		m->abort();
	    } else {
		d = next();
	    }
	}
    Recov_EndTrans(DMFP);
}


struct WriteLinksHook {
    VnodeId vnode;
    Unique_t vunique;
    hblock *hdr;
    FILE *fp;
    int code;
};


static int WriteLinks(struct DirEntry *de, void * hook)
{
	VnodeId vnode;
	Unique_t vunique;
	FID_NFid2Int(&de->fid, &vnode, &vunique);

	char *name = de->name;

	struct WriteLinksHook *wl_hook = (struct WriteLinksHook *)hook;

	if (wl_hook->code) 
		return 0;

	if (vnode == wl_hook->vnode && vunique == wl_hook->vunique) {
		char *comp = rindex(wl_hook->hdr->dbuf.linkname, '/') + 1;
		CODA_ASSERT(comp != NULL);
		if (!STREQ(comp, name)) {
			/* Use the same hblock, overwriting the name
                           field and stashing the return code. */
			int prefix_count = comp - wl_hook->hdr->dbuf.linkname;
			strncpy(wl_hook->hdr->dbuf.name, wl_hook->hdr->dbuf.linkname, 
				prefix_count);
			wl_hook->hdr->dbuf.name[prefix_count] = '\0';
			strcat(wl_hook->hdr->dbuf.name, name);
			wl_hook->code = WriteHeader(wl_hook->fp, *(wl_hook->hdr));
		}
	}
	return 0;
}


int cmlent::checkpoint(FILE *fp) {
    int code = 0;

    hblock hdr; bzero((void *)&hdr, (int) sizeof(hblock));
    switch(opcode) {
	case OLDCML_NewStore_OP:
	    {
	    /* Only checkpoint LAST store! */
	    {
		cml_iterator next(*(ClientModifyLog *)log, AbortOrder, &u.u_store.Fid);
		cmlent *m;
		while ((m = next()) && m->opcode != OLDCML_NewStore_OP)
		    ;
		CODA_ASSERT(m != 0);
		if (m != this) break;
	    }

	    GetPath(hdr.dbuf.name, &u.u_store.Fid);
	    char CacheFileName[CODA_MAXNAMLEN];
	    {
		fsobj *f = FSDB->Find(&u.u_store.Fid);
		CODA_ASSERT(f != 0);
		if (!HAVEALLDATA(f)) {
		    eprint("can't checkpoint (%s), no data", hdr.dbuf.name);
		    break;
		}
		strcpy(CacheFileName, f->data.file->Name());
	    }
	    sprintf(hdr.dbuf.mode, "%6o ", 0644);
	    sprintf(hdr.dbuf.uid, "%6o ", uid);
	    sprintf(hdr.dbuf.gid, "%6o ", -1);
	    sprintf(hdr.dbuf.size, "%11lo ", u.u_store.Length);
	    sprintf(hdr.dbuf.mtime, "%11lo ", time);
	    hdr.dbuf.linkflag = '\0';
	    if ((code = WriteHeader(fp, hdr)) != 0) break;
	    if (u.u_store.Length != 0)
		if ((code = WriteData(fp, CacheFileName)) != 0) break;

	    /* Make hard-links for other names. */
	    {
		fsobj *f = FSDB->Find(&u.u_store.Fid);
		CODA_ASSERT(f != 0);
		if (f->stat.LinkCount > 1 && f->pfso != 0) {
		    hdr.dbuf.linkflag = '1';
		    strcpy(hdr.dbuf.linkname, hdr.dbuf.name);
		    struct WriteLinksHook hook;
		    hook.vnode = f->fid.Vnode;
		    hook.vunique = f->fid.Unique;
		    hook.hdr = &hdr;
		    hook.fp = fp;
		    hook.code = 0;
		    DH_EnumerateDir(&f->pfso->data.dir->dh, WriteLinks, (void *)(&hook));
		    code = hook.code;
		}
	    }
	    }
	    break;

	case OLDCML_MakeDir_OP:
	    {
	    GetPath(hdr.dbuf.name, &u.u_mkdir.CFid);
	    strcat(hdr.dbuf.name, "/");
	    sprintf(hdr.dbuf.mode, "%6o ", 0755);
	    sprintf(hdr.dbuf.uid, "%6o ", uid);
	    sprintf(hdr.dbuf.gid, "%6o ", -1);
	    sprintf(hdr.dbuf.size, "%11lo ", 0);
	    sprintf(hdr.dbuf.mtime, "%11lo ", time);
	    hdr.dbuf.linkflag = '\0';
	    if ((code = WriteHeader(fp, hdr)) != 0) break;
	    }
	    break;

	case OLDCML_SymLink_OP:
	    {
	    GetPath(hdr.dbuf.name, &u.u_symlink.CFid);
	    sprintf(hdr.dbuf.mode, "%6o ", 0755);
	    sprintf(hdr.dbuf.uid, "%6o ", uid);
	    sprintf(hdr.dbuf.gid, "%6o ", -1);
	    sprintf(hdr.dbuf.size, "%11lo ", 0);
	    sprintf(hdr.dbuf.mtime, "%11lo ", time);
	    hdr.dbuf.linkflag = '2';
	    strcpy(hdr.dbuf.linkname, (char *)u.u_symlink.OldName);
	    if ((code = WriteHeader(fp, hdr)) != 0) break;
	    }
	    break;

        case OLDCML_Repair_OP:
	    eprint("Not checkpointing file (%x.%x.%x)that was repaired\n",
		   u.u_repair.Fid.Volume, u.u_repair.Fid.Vnode, u.u_repair.Fid.Unique);
	    break;

	default:
	    break;
    }

    return(code);
}


/* MUST be called from within transaction! */
static void GetPath(char *path, ViceFid *fid, char *lastcomp) {
    fsobj *f = FSDB->Find(fid);
    if (!f) CHOKE("GetPath: %x.%x.%x", fid->Volume, fid->Vnode, fid->Unique);
    char buf[MAXPATHLEN];
    f->GetPath(buf);
    if (lastcomp)
	{ strcat(buf, "/"); strcat(buf, lastcomp); }
    strcpy(path, buf);
}


static int WriteHeader(FILE *fp, hblock& hdr) {
    char *cp;
    for (cp = hdr.dbuf.chksum; cp < &hdr.dbuf.chksum[sizeof(hdr.dbuf.chksum)]; cp++)
	*cp = ' ';
    int i = 0;
    for (cp = hdr.dummy; cp < &hdr.dummy[TBLOCK]; cp++)
	i += *cp;
    sprintf(hdr.dbuf.chksum, "%6o", i);

    if (fwrite((char *)&hdr, (int) sizeof(hblock), 1, fp) != 1) {
	LOG(0, ("WriteHeader: fwrite failed (%d)", errno));
	return(errno ? errno : ENOSPC);
    }
    return(0);
}


static int WriteData(FILE *wrfp, char *rdfn) {
    VprocYield();		/* Yield at least once per dumped file! */

    FILE *rdfp = fopen(rdfn, "r");
    if (rdfp == NULL)
	CHOKE("WriteData:: fopen(%s) failed", rdfn);

    int code = 0;
    for (int i = 0; ; i++) {
	if ((i % 32) == 0)
	    VprocYield();	/* Yield every so often */

	char buf[TBLOCK];
	int cc = fread(buf, (int) sizeof(char), TBLOCK, rdfp);
	if (cc < TBLOCK)
	    bzero((char *)buf + cc, TBLOCK - cc);
	if (fwrite(buf, TBLOCK, 1, wrfp) != 1) {
	    LOG(0, ("WriteData: (%s) fwrite (%d)", rdfn, errno));
	    code = (errno ? errno : ENOSPC);
	    break;
	}
	if (cc < TBLOCK) break;
    }
    fclose(rdfp);
    return(code);
}


static int WriteTrailer(FILE *fp) {
    char buf[TBLOCK];
    bzero((void *)buf, TBLOCK);
    for (int i = 0; i < 2; i++)
	if (fwrite(buf, TBLOCK, 1, fp) != 1) {
	    LOG(0, ("WriteTrailer: fwrite (%d)", errno));
	    return(errno ? errno : ENOSPC);
	}
    return(0);
}


/* MUST be called from within transaction! */
void cmlent::abort() {
    volent *vol = strbase(volent, log, CML);
    vol->RecordsAborted++;

    /* Step 1:  CODA_ASSERT that there are no edges emanating from this record. */
    CODA_ASSERT(succ == 0 || succ->count() == 0);

    /* Step 2:  Kill fsos linked into this record */
    dlist_iterator next(*fid_bindings);
    dlink *d;

    while ((d = next())) {
	binding *b = strbase(binding, d, binder_handle);
	fsobj *f = (fsobj *)b->bindee;
	    
	/* sanity checks */
	CODA_ASSERT(f && (f->MagicNumber == FSO_MagicNumber));  /* better be an fso */

	f->Lock(WR);
	f->Kill();

	FSDB->Put(&f);
    }

    delete this;
}


/*  *****  Routines for Maintaining fsobj <--> cmlent Bindings  *****  */

/* MUST be called from within transaction! */
void ClientModifyLog::AttachFidBindings() {
    cml_iterator next(*this);
    cmlent *m;
    while ((m = next()))
	m->AttachFidBindings();
}


/* MUST be called from within transaction! */
void cmlent::AttachFidBindings() {
    ViceFid *fids[3];
    ViceVersionVector *vvs[3];
    GetVVandFids(vvs, fids);

    for (int i = 0; i < 3; i++) {
	ViceFid *fidp = fids[i];
	if (fidp == 0) break;

	fsobj *f = FSDB->Find(fidp);
	if (f == 0) {
	    print(logFile);
	    (strbase(volent, log, CML))->print(logFile);
	    CHOKE("cmlent::AttachFidBindings: can't find (%x.%x.%x)",
		fidp->Volume, fidp->Vnode, fidp->Unique);
	}

	binding *b = new binding;
	b->binder = this;
	if (fid_bindings == 0)
	    fid_bindings = new dlist;
	fid_bindings->append(&b->binder_handle);
	f->AttachMleBinding(b);
    }
}


void cmlent::DetachFidBindings() {
    if (fid_bindings == 0) return;

    dlink *d;
    while ((d = fid_bindings->get())) {
	binding *b = strbase(binding, d, binder_handle);
	fsobj *f = (fsobj *)b->bindee;
	f->DetachMleBinding(b);
	b->binder = 0;
	delete b;
    }
    delete fid_bindings;
    fid_bindings = 0;
}

void cmlent::writeops(FILE *fp) {
    char path[MAXPATHLEN], path2[MAXPATHLEN];
    char msg[2 * MAXPATHLEN + 100]; 	// this is enough for writing one entry

    switch(opcode) {
    case OLDCML_NewStore_OP:
	RecoverPathName(path, &u.u_store.Fid, log, this);
	sprintf(msg, "Store \t%s (length = %ld)", path, u.u_store.Length);
	break;

    case OLDCML_Utimes_OP:
	RecoverPathName(path, &u.u_utimes.Fid, log, this);
	sprintf(msg, "Utime \t%s", path);
	break;

    case OLDCML_Chown_OP:
	RecoverPathName(path, &u.u_chown.Fid, log, this);
	sprintf(msg, "Chown \t%s (owner = %d)", path, u.u_chown.Owner);
	break;

    case OLDCML_Chmod_OP:
	RecoverPathName(path, &u.u_chmod.Fid, log, this);
	sprintf(msg, "Chmod \t%s (mode = %o)", path, u.u_chmod.Mode);
	break;

    case OLDCML_Create_OP:
	RecoverPathName(path, &u.u_create.CFid, log, this);
	sprintf(msg, "Create \t%s", path);
	break;

    case OLDCML_Remove_OP:
	RecoverPathName(path, &u.u_remove.CFid, log, this);
	sprintf(msg, "Remove \t%s", path);
	break;

    case OLDCML_Link_OP:
	RecoverPathName(path, &u.u_link.CFid, log, this);
	sprintf(msg, "Link \t%s to %s", path, u.u_link.Name);
	break;

    case OLDCML_Rename_OP:
	RecoverPathName(path, &u.u_rename.SPFid, log, this);
	RecoverPathName(path2, &u.u_rename.TPFid, log, this);
	sprintf(msg, "Rename \t%s/%s (to: %s/%s)", path, u.u_rename.OldName, path2,
		u.u_rename.NewName);
	break;

    case OLDCML_MakeDir_OP:
	RecoverPathName(path, &u.u_mkdir.CFid, log, this);
	sprintf(msg, "Mkdir \t%s", path);
	break;

    case OLDCML_RemoveDir_OP:
	RecoverPathName(path, &u.u_rmdir.CFid, log, this);
	sprintf(msg, "Rmdir \t%s", path);
	break;

    case OLDCML_SymLink_OP:
	RecoverPathName(path, &u.u_symlink.CFid, log, this);
	sprintf(msg, "Symlink %s (--> %s)", path, u.u_symlink.OldName);
	break;

    case OLDCML_Repair_OP:
	sprintf(msg, "Disconnected Repair by an ASR for %s",
		FID_(&u.u_repair.Fid));
	break;
    default:
	break;
    }
    
    fprintf(fp, "%s\n", msg);
}


/* this routine determines if a cmlent is old enough to reintegrate. */
int cmlent::Aged() {
    int oldenough = 0;
    volent *vol = strbase(volent, log, CML);

    time_t curTime = Vtime();
    if ((curTime - time) >= vol->AgeLimit)
	oldenough = 1;

    return oldenough;
}


/* 
 * simpleminded routine to estimate the amount of time to reintegrate
 * this record (in milleseconds), given an estimate of bandwidth in 
 * bytes/second.
 */
unsigned long cmlent::ReintTime(unsigned long bw) {
    volent *vol = strbase(volent, log, CML);
    double time = 0;

    if (bw > 0) {
	time = (double) size();
	if (opcode == OLDCML_NewStore_OP) 
	    time += u.u_store.Length;  /* might be large */

	time = time * 1000.0/ (double) bw;
    }

    LOG(0, ("cmlent::ReintTime: bandwidth = %d bytes/sec, time = %d msec\n",
	    bw, (unsigned long) time));
    if (LogLevel >= 0/*10*/) print(logFile);

    return((unsigned long) time);
}


unsigned long cmlent::ReintAmount() {
    volent *vol = strbase(volent, log, CML);
    int amount;
    unsigned long bw;	/* bandwidth, in bytes/sec */

    CODA_ASSERT(opcode == OLDCML_NewStore_OP);

    /* 
     * try to get a dynamic bw estimate.  If that doesn't
     * work, fall back on the static estimate.
     */
    vol->vsg->GetBandwidth(&bw);

    if (bw > 0) 
	amount = vol->ReintLimit/1000 * bw;
    else
	amount = u.u_store.Length;

    if (u.u_store.Offset + amount > u.u_store.Length)
	amount = u.u_store.Length - u.u_store.Offset;

    return amount;
}


/* reintegrating --> frozen */
int cmlent::IsReintegrating() 
{
    volent *vol = strbase(volent, log, CML);

    if (vol->flags.reintegrating && IsFrozen() &&
	(tid != UNSET_TID) && (tid == vol->cur_reint_tid))
	    return 1;

    return 0;
}


/*  *****  Modify Log Iterator  *****  */

/*
 *    1. This implementation assumes that a dlist_iterator can correctly iterate over a rec_dlist as well as a dlist!
 *    2. Iterating over records referencing a particular fid is grossly inefficient and needs to be improved!
 *    3. Iterating starting after a prelude is inefficient and needs to be improved (by augmenting dlist_iterator)!
 */

cml_iterator::cml_iterator(ClientModifyLog& Log, CmlIterOrder Order,
			    ViceFid *Fid, cmlent *Prelude) {
    log = &Log;
    order = Order;
    fidp = Fid;
    if (fidp == 0) {
	next = new dlist_iterator(*((dlist *)&log->list), order);
    }
    else {
	fid = *Fid;
	fsobj *f = FSDB->Find(&fid);
	if (f == 0) {
	    CHOKE("cml_iterator::cml_iterator: can't find (%x.%x.%x)",
		fid.Volume, fid.Vnode, fid.Unique);
	}
	if (f->mle_bindings == 0)
	    next = 0;
	else
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


cml_iterator::~cml_iterator() {
    if (next != 0)
	delete next;
}


cmlent *cml_iterator::operator()() {
    for (;;) {
	if (fidp == 0) {
	    dlink *d = (*next)();
	    if (d == 0) return(0);
	    cmlent *m = strbase(cmlent, d, handle);
	    return(m);
	}
	else {
	    if (next == 0) return(0);
	    dlink *d = (*next)();
	    if (d == 0) return(0);
	    binding *b = strbase(binding, d, bindee_handle);
	    cmlent *m = (cmlent *)b->binder;
	    switch(m->opcode) {
		case OLDCML_NewStore_OP:
		    if (FID_EQ(&m->u.u_store.Fid, &fid)) return(m);
		    break;

		case OLDCML_Utimes_OP:
		    if (FID_EQ(&m->u.u_utimes.Fid, &fid)) return(m);
		    break;

		case OLDCML_Chown_OP:
		    if (FID_EQ(&m->u.u_chown.Fid, &fid)) return(m);
		    break;

		case OLDCML_Chmod_OP:
		    if (FID_EQ(&m->u.u_chmod.Fid, &fid)) return(m);
		    break;

		case OLDCML_Create_OP:
		    if (FID_EQ(&m->u.u_create.PFid, &fid)) return(m);
		    if (FID_EQ(&m->u.u_create.CFid, &fid)) return(m);
		    break;

		case OLDCML_Remove_OP:
		    if (FID_EQ(&m->u.u_remove.PFid, &fid)) return(m);
		    if (FID_EQ(&m->u.u_remove.CFid, &fid)) return(m);
		    break;

		case OLDCML_Link_OP:
		    if (FID_EQ(&m->u.u_link.PFid, &fid)) return(m);
		    if (FID_EQ(&m->u.u_link.CFid, &fid)) return(m);
		    break;

		case OLDCML_Rename_OP:
		    if (FID_EQ(&m->u.u_rename.SPFid, &fid)) return(m);
		    if (FID_EQ(&m->u.u_rename.TPFid, &fid)) return(m);
		    if (FID_EQ(&m->u.u_rename.SFid, &fid)) return(m);
		    break;

		case OLDCML_MakeDir_OP:
		    if (FID_EQ(&m->u.u_mkdir.PFid, &fid)) return(m);
		    if (FID_EQ(&m->u.u_mkdir.CFid, &fid)) return(m);
		    break;

		case OLDCML_RemoveDir_OP:
		    if (FID_EQ(&m->u.u_rmdir.PFid, &fid)) return(m);
		    if (FID_EQ(&m->u.u_rmdir.CFid, &fid)) return(m);
		    break;

		case OLDCML_SymLink_OP:
		    if (FID_EQ(&m->u.u_symlink.PFid, &fid)) return(m);
		    if (FID_EQ(&m->u.u_symlink.CFid, &fid)) return(m);
		    break;

	        case OLDCML_Repair_OP:
		    if (FID_EQ(&m->u.u_repair.Fid, &fid)) return(m);
		    break;

		default:
		    CODA_ASSERT(0);
	    }
	}
    }
}
