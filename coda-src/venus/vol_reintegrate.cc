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
 *    Implementation of the Venus Reintegrate facility.
 *
 *    Reintegration is essentially the merge phase of Davidson's
 *    Optimistic Protocol.  Our implementation is highly stylized,
 *    however, to account for the fact that our "database" is a
 *    Unix-like file system rather than a conventional database whose
 *    operations and integrity constraints are well specified.
 *


 * Specific details of our implementation are the following: 

 *       1. the unit of logging and reintegration is the volume; this
 *       follows from: - a "transaction" may reference objects in only
 *       one volume - a volume is contained within a single storage
 *       group

 *       2. the client, i.e., Venus, is always the coordinator for the
 *       merge

 *       3. the server does NOT maintain a log of its partitioned
 *       operations; instead, we rely on the client to supply version
 *       information with its "transactions" sufficient to allow the
 *       server to distinguish conflicts


 *       4. "transactions" map one-to-one onto Vice operations;
 *       non-mutating "transactions" are not logged since they can
 *       always be "serialized" before any conflicting mutations
 */


#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <stdio.h>
#include <struct.h>
#include <netdb.h>

#include <rpc2/errors.h>

#ifdef __cplusplus
}
#endif

/* from venus */
#include "local.h"
#include "user.h"
#include "venus.private.h"
#include "venusvol.h"
#include "vproc.h"
#include "adv_daemon.h"


/* must not be called from within a transaction */
void repvol::Reintegrate()
{
    userent *u;

    LOG(0, ("repvol::Reintegrate\n"));

    /* 
     * this flag keeps multiple reintegrators from interfering with
     * each other in the same volume.  This is necessary even with 
     * the cur_reint_tid field, because the latter is reset between
     * iterations of the loop below.  Without the flag, other threads
     * may slip in between commit or abort of a record.
     */
    if (IsReintegrating())
	return;

    GetUser(&u, CML.owner);
    CODA_ASSERT(u != NULL);
    /* if (SkkEnabled) u->NotifyReintegrationActive(name); */
    flags.reintegrating = 1;

    /* enter the volume */
    vproc *v = VprocSelf();

    VolFid vfid;
    vfid.Realm = realm->Id();
    vfid.Volume = vid;

    v->Begin_VFS(&vfid, CODA_REINTEGRATE);
    VOL_ASSERT(this, v->u.u_error == 0);

    /* prevent ASRs from slipping in and leaving records we might reintegrate. */
    DisableASR(V_UID);

    /* lock the CML to prevent a checkpointer from getting confused. */
    ObtainReadLock(&CML_lock);

    /* NOW we're ready.*/
    /* step 1.  scan the log, cancelling stores for open-for-write files. */
    CML.CancelStores();

    int nrecs, startedrecs, thisTid, code = 0;

    /* We do the actual reintegration steps in a loop, as we reintegrate in
     * blocks of 100 cmlents. JH */
    do {
        /* reset invariants */
        thisTid = -GetReintId();
        nrecs = 0;

        /*
         * step 2. Attempt to do partial reintegration for big stores at
         * the head of the CML.
         */
        code = PartialReintegrate(thisTid);

        /* PartialReintegrate returns ENOENT when there was no CML entry
         * available for partial reintegration */
        if (code != ENOENT) {
            eprint("Reintegrate: %s, partial record, result = %s", 
                   name, VenusRetStr(code));
            /* done for now */
            break;
        }
        /* clear the errorcode, ENOENT was not a fatal error. */
        code = 0;

        /*
         * step 3.
         * scan the log, gathering records that are ready to to reintegrate.
         */
        CML.GetReintegrateable(thisTid, &nrecs);

        /* nothing to reintegrate? jump out of the loop! */
        if (nrecs == 0) break;

        /*
         * step 4.
         * We've come up with something, reintegrate it.
         */

        /* Log how many entries we are going to reintegrate */
        startedrecs = CML.count();
        MarinerLog("reintegrate::%s, %d/%d\n", name, nrecs, startedrecs);

        code = IncReintegrate(thisTid);

	/* Log how many entries are left to reintegrate */
	MarinerLog("reintegrate::%s, 0/%d\n", name, CML.count());
        eprint("Reintegrate: %s, %d/%d records, result = %s", 
               name, nrecs, startedrecs, VenusRetStr(code));

    /*
     * Keep going as long as we managed to reintegrate records without errors,
     * but we don't want to interfere with trickle reintegration so we test
     * whether a full block has been sent (see also cmlent::GetReintegrateable)
     */
    } while(code == 0 && ((flags.writebackreint && nrecs) || nrecs == 100));

    flags.reintegrating = 0;

    /* 
     * clear CML owner if possible.  If there are still mutators in the volume,
     * the owner must remain set (it is overloaded).  In that case the last
     * mutator will clear the owner on volume exit.
     */
    if (CML.count() == 0  &&  mutator_count == 0)
        CML.owner = UNSET_UID;

    /* trigger a transition if necessary */
    if ((CML.count() == 0 || (CML.count() > 0 && !ContainUnrepairedCML()))
	&& flags.logv == 0 && state == Logging) {
	flags.transition_pending = 1;
    }

    /* if code was non-zero, return EINVAL to End_VFS to force this
       reintegration to inc fail count rather than success count */
    VOL_ASSERT(this, v->u.u_error == 0);
    if (code) v->u.u_error = EINVAL;

    EnableASR(V_UID);	
    ReleaseReadLock(&CML_lock);

    /* Surrender control of the volume. */
    v->End_VFS();

    /* reset it, 'cause we can't leave errors just laying around */
    v->u.u_error = 0;

    /* Let user know reintegration has completed */
    /*   if (SkkEnabled) u->NotifyReintegrationCompleted(name); */
}


/*
 *
 *    Reintegration consists of the following phases:
 *       1. (late) prelude
 *       2. interlude
 *       3. postlude
 *
 */

/* must not be called from within a transaction */
int repvol::IncReintegrate(int tid)
{
    LOG(0, ("volent::IncReintegrate: (%s, %d) vuid = %d\n",
	    name, tid, CML.owner));
    /* check if transaction "tid" has any cmlent objects */
    if (!CML.HaveElements(tid)) {
	LOG(0, ("volent::IncReintegrate: transaction %d does not have any elements\n", 
		tid));
	return 0;
    }

    /* check if volume state is Logging or not */
    if (state != Logging) return ETIMEDOUT;	/* it must be Emulating */

    int code = 0;
    int done;

    /* Get the current CML stats for reporting diffs */
    int StartCancelled = RecordsCancelled;
    int StartCommitted = RecordsCommitted;
    int StartAborted = RecordsAborted;
    int StartRealloced = FidsRealloced;
    long StartBackFetched = BytesBackFetched;

    /* Get the NEW CML stats. */
    cmlstats current;
    cmlstats cancelled;
    CML.IncGetStats(current, cancelled, tid);	/* get incremental stats */

    START_TIMING();
    float pre_elapsed = 0.0, inter_elapsed = 0.0, post_elapsed = 0.0;
    do {
	char *buf = 0;
	int bufsize = 0;
	ViceVersionVector UpdateSet;
	code = 0;
	cur_reint_tid = tid; 
	done = 1;
	int outoforder;

	/* Steps 1-3 constitute the ``late prelude.'' */
	{
	    START_TIMING();
	    /* 
	     * Step 1 is to reallocate real fids for new client-log 
	     * objects that were created with "local" fids.
	     */
	    code = CML.IncReallocFids(tid);
	    if (code != 0) goto CheckResult;

	    /* 
	     * Step 3 is to "thread" the log and pack it into a buffer 
	     * (buffer is allocated by pack routine).
	     */
	    CML.IncThread(tid);
	    CML.IncPack(&buf, &bufsize, tid);

	    END_TIMING();
	    pre_elapsed = elapsed;
	}

	/* 
	 * Step 4 is to have the server(s) replay the client modify log 
	 * via a Reintegrate RPC. 
	 */
	{
	    START_TIMING();

	    outoforder = CML.OutOfOrder(tid);
	    code = CML.COP1(buf, bufsize, &UpdateSet, outoforder);

	    END_TIMING();
	    inter_elapsed = elapsed;
	}

	{
CheckResult:
	    START_TIMING();
	    if (buf != 0) delete buf;

	    switch(code) {
	    case 0 : 
	    case EALREADY:
		/* Commit logged mutations upon successful replay at server. */
		CML.IncCommit(&UpdateSet, tid);
		LOG(0, ("volent::IncReintegrate: committed\n"));

		CML.ClearPending();
		break;

	    case ETIMEDOUT:
		/*
		 * We cannot cancel pending records because we do not know if 
		 * the reintegration actually occurred at the server.  If the 
		 * RPC reply was lost it is possible that it succeeded.  Note
		 * the next attempt may involve a different set of records.
		 */
		break;

	    case ERETRY:
	    case EWOULDBLOCK:
		/* 
		 * if any cmlents we were working on are still around and 
		 * should now be cancelled, do it.
		 */
		CML.CancelPending();

		/* 
		 * We do our own retrying here, because the code in 
		 * vproc::End_VFS() causes an entirely new vproc to start 
		 * up for each transition into reintegrating state (and 
		 * thus it has no knowledge of how many "waits" have already 
		 * been done).  Unfortunately, that means we end up 
		 * duplicating some code here.  Finally, if a transition is 
		 * pending, we'd better take it. 
		 */
		if (flags.transition_pending) {
		    break;
		}
		else {
		    vproc *v = VprocSelf();

		    if (code == ERETRY) {
			v->u.u_retrycnt++;
extern int VprocRetryN;
			if (v->u.u_retrycnt <= VprocRetryN) {
extern struct timeval *VprocRetryBeta;
			    VprocSleep(&VprocRetryBeta[v->u.u_retrycnt]);
			    done = 0;
			    break;
                        }
		    }
		    if (code == EWOULDBLOCK) {
			v->u.u_wdblkcnt++;
			if (v->u.u_wdblkcnt <= 20) {	/* XXX */
			    eprint("Volume %s busy, waiting", name);
			    struct timeval delay;
			    delay.tv_sec = 20;		/* XXX */
			    delay.tv_usec = 0;
			    VprocSleep(&delay);
			    done = 0;
			    break;
			}
		    }
		    /* Fall through if retry/wdblk count was exceeded ... */
		}

	    case EINCOMPATIBLE:	
	    default:
		/* non-retryable failures */

		LOG(0, ("volent::IncReintegrate: fail code = %d\n", code));
		CML.print(logFile);
		/* 	
                 * checkpoint the log before localizing or aborting.
		 * release read lock; it will be boosted in CML.Checkpoint.
		 * Note that we may have to wait until other mutators finish 
		 * mucking with the volume/log, which means the state we checkpoint 
		 * might not be the state we had.  Note that we MUST unlock objects 
		 * before boosting this lock to prevent deadlock with mutator threads.
		 */
		ReleaseReadLock(&CML_lock);
		CML.CheckPoint(0);
		ObtainReadLock(&CML_lock);

		CML.CancelPending();       
		/*
		 * localize or abort the record if this is not a local repair
		 * session.  Local repair does its own error handling.
		 */
		if (tid != LRDB->GetRepairSessionTid())
		    CML.HandleFailedMLE();

		break;
	    }

	    END_TIMING();
	    post_elapsed = elapsed;
	}
    } while (!done);

    cur_reint_tid = UNSET_TID;
    END_TIMING();
    LOG(0, ("IncReintegrate: (%s,%d) result = %s, elapsed = %3.1f (%3.1f, %3.1f, %3.1f)\n", 
	    name, tid, VenusRetStr(code), elapsed, pre_elapsed, inter_elapsed, post_elapsed));
    LOG(100, ("\t old stats = [%d, %d, %d, %d, %d]\n",
	   RecordsCancelled - StartCancelled, 
	   RecordsCommitted - StartCommitted, 
	   RecordsAborted - StartAborted,
	   FidsRealloced - StartRealloced,
	   BytesBackFetched - StartBackFetched));
    LOG(0, ("\tnew stats = [%4d, %5.1f, %7.1f, %4d, %5.1f], [%4d, %5.1f, %7.1f, %4d, %5.1f]\n",
	   current.store_count, current.store_size / 1024.0, 
	   current.store_contents_size / 1024.0,
	   current.other_count, current.other_size / 1024.0,
	   cancelled.store_count, cancelled.store_size / 1024.0, 
	   cancelled.store_contents_size / 1024.0,
	   cancelled.other_count, cancelled.other_size / 1024.0));

    CML.cancellations.store_count = 0;
    CML.cancellations.store_size = 0.0;
    CML.cancellations.store_contents_size = 0.0;
    CML.cancellations.other_count = 0;
    CML.cancellations.other_size = 0.0;

    return (code);
}


/*
 * Reintegrate some portion of the store record at the head
 * of the log.
 */
int repvol::PartialReintegrate(int tid)
{
    cmlent *m;
    int code = 0;
    ViceVersionVector UpdateSet;

    /* Is there an entry ready for partial reintegration at the head? */
    m = CML.GetFatHead(tid);

    /* Indicate that there was nothing to do partial reintegration on. */
    if (!m) return(ENOENT);

    cur_reint_tid = tid; 
    LOG(0, ("volent::PartialReintegrate: (%s, %d) vuid = %d\n",
	    name, tid, CML.owner));

    /* perform some late prelude functions. */
    {
	code = m->realloc();
	if (code != 0) goto CheckResult;
    }

    /* 
     * If we have a handle, check the status.
     * If this is a new transfer, get a handle from the server.
     */
    {
	if (m->HaveReintegrationHandle()) 
	     code = m->ValidateReintegrationHandle();
	else code = EBADF;

	if (code)
	    code = m->GetReintegrationHandle();

	if (code) goto CheckResult;
    }

    /* send some file data to the server */
    {
	while (!m->DoneSending() && (code == 0)) 
	    code = m->WriteReintegrationHandle();

	if (code != 0) goto CheckResult;
    }

    /* reintegrate the changes if all of the data is there */
    if (m->DoneSending()) {
	char *buf = 0;
	int bufsize = 0;

	CML.IncThread(tid);
	CML.IncPack(&buf, &bufsize, tid);

	code = m->CloseReintegrationHandle(buf, bufsize, &UpdateSet);
	if (buf != 0) delete buf;

	if (code != 0) goto CheckResult;
    }

CheckResult:
    /* 
     * if this record was being partially reintegrated, but was
     * not sent completely, then the partial results are invalid.
     */
    if (m->HaveReintegrationHandle() && !m->DoneSending())
	m->ClearReintegrationHandle();		/* start over! */

    /* allow log optimizations to go through. */
    cur_reint_tid = UNSET_TID;

    /*
     * In the following, calls to CancelPending may cancel the record
     * out from under us.
     */
    switch (code) {
    case 0:
    case EALREADY:
	if (m->DoneSending()) {
	    /* Commit logged mutations upon successful replay at server. */
	    CML.IncCommit(&UpdateSet, tid);
	    LOG(0, ("volent::PartialReintegrate: committed\n"));

	    CML.ClearPending();
	} else {
	    /* allow an incompletely sent record to be cancelled. */
	    CML.CancelPending();
	}
	break;

    case ETIMEDOUT: 
    case EWOULDBLOCK:
    case ERETRY:
	CML.CancelPending();
	break;

    case EBADF:		/* bad handle */
	m->ClearReintegrationHandle();
	CML.CancelPending();
	code = ERETRY;	/* try again later */
	break;

    case EINCOMPATIBLE:
    default:
	/* non-retryable failures -- see IncReintegrate for comments */

	LOG(0, ("volent::PartialReintegrate: fail code = %d\n", code));
	CML.print(logFile);

	/* checkpoint the log */
	ReleaseReadLock(&CML_lock);
	CML.CheckPoint(0);
	ObtainReadLock(&CML_lock);

	/* cancel, localize, or abort the offending record */
	CML.CancelPending();       
	CML.HandleFailedMLE();

	break;
    }

    return(code);
}


/* 
 * determine if a volume has updates that may be reintegrated,
 * and return the number. humongous predicate check here.  
 */
int repvol::ReadyToReintegrate()
{
    int ready = 0;
    userent *u = 0;

    GetUser(&u, CML.owner);	/* if the CML is non-empty, u != 0 */
    /* 
     * we're a bit draconian about ASRs.  We want to avoid reintegrating
     * while an ASR is in progress, because the ASR uses the write
     * disconnected state in place of transactional support -- it requires
     * control of when its updates are sent back.  The ASRinProgress flag 
     * is Venus-wide, so the check is correct but more conservative than 
     * we would like.
     */
    if (IsWriteDisconnected() && 
	(CML.count() > 0) && u->TokensValid() &&
	!asr_running() && !IsReintegrating()) {
	    cmlent *m;
	    cml_iterator next(CML, CommitOrder);

	    while ((m = next()) && m->ReintReady()) 
		ready++;
    }

    PutUser(&u);
    return(ready);
}


/* need not be called from within a transaction */
int cmlent::ReintReady()
{
    /* check volume state */
    repvol *vol = strbase(repvol, log, CML);
    if (!(vol->IsWriteDisconnected())) {
	LOG(100, ("cmlent::ReintReady: not write-disconnected\n"));
	return 0;
    }

    if (vol->flags.transition_pending) {
	LOG(100, ("cmlent::ReintReady: transition_pending\n"));
	return 0;
    }

    /* check if its repair flag is set */
    if (flags.to_be_repaired || flags.repair_mutation) {
	LOG(100, ("cmlent::ReintReady: this is a repair related cmlent\n"));
	return 0;
    }

    if (ContainLocalFid()) {
	LOG(100, ("cmlent::ReintReady: contains local fid\n"));
	/* set its to_be_repaired flag */
	SetRepairFlag();
	return 0;
    }

    /* check if this record is part of a transaction (IOT, etc.) */
    if (tid > 0) {
	LOG(100, ("cmlent::ReintReady: transactional cmlent\n"));
	return 0; 
    }

    /* if vol staying write disconnected, check age. does not apply to ASRs */
    /* nor when returning from writeback */
    if (!(vol->asr_running()) && vol->flags.logv && !Aged() && 
	!vol->flags.writebackreint) {
	LOG(100, ("cmlent::ReintReady: record too young\n"));
	return 0; 
    }

    /* if cmlent is beyond the last one needing to be flushed after writeback
     * revocation */
    if ((this == vol->reintegrate_done) && (vol->flags.sync_reintegrate) && (vol->flags.writebackreint)){
	LOG(100, ("cmlent::ReintReady: beyond last record needing to be written synchronously\n"));
	return 0;
    }

    return 1;
}


/* *****  Reintegrator  ***** */

static const int ReintegratorStackSize = 65536;
static const int MaxFreeReintegrators = 2;
static const int ReintegratorPriority = LWP_NORMAL_PRIORITY-2;

/* local-repair modification */
class reintegrator : public vproc {
  friend void Reintegrate(repvol *);

    static olist freelist;
    olink handle;
    struct Lock run_lock;

    reintegrator();
    reintegrator(reintegrator&);			/* not supported! */
    int operator=(reintegrator&) { abort();	return(0); }	/* not supported! */
    ~reintegrator();

  protected:
    virtual void main(void);
};

olist reintegrator::freelist;


/* This is the entry point for reintegration. */
/* It finds a free reintegrator (or creates a new one), 
   sets up its context, and gives it a poke. */
void Reintegrate(repvol *v)
{
    if (v->flags.sync_reintegrate) {
	LOG(0,("Reintegrate synchronously\n\n"));
	v->Reintegrate();
    }
    else {
	LOG(0, ("Reintegrate\n"));
	/* Get a free reintegrator. */
	reintegrator *r;
	olink *o = reintegrator::freelist.get();
	r = (o == 0)
	    ? new reintegrator
	    : strbase(reintegrator, o, handle);
	CODA_ASSERT(r->idle);
	
	/* Set up context for reintegrator. */
	r->u.Init();
#ifdef __BSD44__
	r->u.u_cred.cr_uid = v->CML.Owner();
#endif /* __BSD44__ */
	r->u.u_vol = v;
	v->hold();		    /* vproc::End_VFS() will do release */
	
	/* Set it going. */
	r->idle = 0;
	VprocSignal((char *)r);	/* ignored for new reintegrators */
    }
}


reintegrator::reintegrator() :
	vproc("Reintegrator", NULL, VPT_Reintegrator, ReintegratorStackSize,
	      ReintegratorPriority)
{
    LOG(100, ("reintegrator::reintegrator(%#x): %-16s : lwpid = %d\n",
	       this, name, lwpid));

    idle = 1;
    start_thread();
}


reintegrator::reintegrator(reintegrator& r) : vproc((vproc&)r) {
    abort();
}


reintegrator::~reintegrator() {
    LOG(100, ("reintegrator::~reintegrator: %-16s : lwpid = %d\n", name, lwpid));

}


/* 
 * N.B. Vproc synchronization is not done in the usual way, with
 * the constructor signalling, and the new vproc waiting in its
 * main procedure for the constructor to poke it.  This handshake 
 * assumes that the new vproc has a thread priority greater than or
 * equal to its creator; only then does the a thread run when 
 * LWP_CreateProcess is called.  If the creator has a higher 
 * priority, the new reintegrator runs only when the creator is
 * suspended.  Otherwise, the reintegrator will run when created.
 * This can happen if a reintegrator creates another reintegrator.
 * In this case, the yield below allows the creator to fill in
 * the context.
 */
void reintegrator::main(void)
{
    /* Hack!  Vproc must yield before data members become valid! */
    VprocYield();

    for (;;) {
	if (idle) CHOKE("reintegrator::main: signalled but not dispatched!");
	if (!u.u_vol) CHOKE("reintegrator::main: no volume!");

	/* Do the reintegration. */
	((repvol *)u.u_vol)->Reintegrate();
	seq++;
	idle = 1;

	/* Commit suicide if we already have enough free reintegrators. */
	if (freelist.count() == MaxFreeReintegrators) 
	    delete VprocSelf();

	/* Else put ourselves on free list. */
	freelist.append(&handle);

	/* Wait for new request. */
	VprocWait((char *)this);
    }
}
