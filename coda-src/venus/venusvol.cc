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

/* * Bugs: Currently, the binding of volent <--> VSG is static
 *         (i.e., fixed at construction of the volent).  We need a
 *         mechanism for changing this binding over time, due to:
 *
 *          - commencement of a new "epoch" for a ReplicatedVolume
 *            (i.e., adding/deleting a replica)  
 *          - movement of a volume from one host to another
 *
 *
 *  This is complicated by the fact that servers allocate fids in `strides'
 *  according to the current replication factor of the volume. --JH
 */

/*
 *    Implementation of the Venus Volume abstraction.
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
 *    Logging state may be triggered by discovery of weak connectivity, or an
 *    application-specific resolution.
 *    All of these are rolled into the flag "logv".  In this state, cache
 *    misses may be serviced, but modify activity is recorded in the CML.
 *    Resolution must be permitted in logging state because references
 *    resulting in cache misses may require it.
 *
 *    Note that non-replicated volumes have only two states, {Hoarding, Emulating}.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include "coda_string.h"
#include <sys/types.h>
#include <errno.h>
#include <struct.h>
#include <sys/param.h>

#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>

#include <rpc2/rpc2.h>
/* interfaces */
#include <vice.h>

#ifdef __cplusplus
}
#endif __cplusplus


/* from venus */
#include "adv_daemon.h"
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

int MLEs = 0;

/* local-repair modification */
void VolInit()
{
    /* Allocate the database if requested. */
    if (InitMetaData) {					/* <==> VDB == 0 */
	Recov_BeginTrans();
	RVMLIB_REC_OBJECT(VDB);
	VDB = new vdb;
	Recov_EndTrans(0);

	/* Create the local fake volume */
	VolumeInfo LocalVol;
	memset(&LocalVol, 0, sizeof(VolumeInfo));
	FID_MakeVolFake(&LocalVol.Vid);
        LocalVol.Type = BACKVOL; /* backup volume == read-only replica */
	CODA_ASSERT(VDB->Create(&LocalVol, "Local"));
    }

    /* Initialize transient members. */
    VDB->ResetTransient();

    /* Scan the database. */
    eprint("starting VDB scan");
    {
	/* Check entries in the table. */
	{
	    int FoundMLEs = 0;

            { /* Initialize transient members. */
                volrep_iterator next;
                volrep *v;
                while ((v = next()))
                    v->ResetTransient();
            }
            eprint("\t%d volume replicas", VDB->volrep_hash.count());
            {
                repvol_iterator next;
                repvol *v;
                while ((v = next())) {
                    /* Initialize transient members. */
                    v->ResetTransient();
                    FoundMLEs += v->CML.count();
                }
            }
            eprint("\t%d replicated volumes", VDB->repvol_hash.count());
	    eprint("\t%d CML entries allocated", VDB->AllocatedMLEs);

	    if (FoundMLEs != VDB->AllocatedMLEs)
		CHOKE("VolInit: MLE mismatch (%d != %d)",
		       FoundMLEs, VDB->AllocatedMLEs);
	}

	/* Check entries on the freelist. */
	{
	    /* Nothing useful to do! */
	    eprint("\t%d CML entries on free-list", VDB->mlefreelist.count());
	}

        int nvols = VDB->volrep_hash.count() + VDB->repvol_hash.count();

	if (nvols > CacheFiles)
	    CHOKE("VolInit: too many vol entries (%d > %d)", nvols, CacheFiles);
	if (VDB->AllocatedMLEs + VDB->mlefreelist.count() > VDB->MaxMLEs)
	    CHOKE("VolInit: too many MLEs (%d + %d > %d)",
		VDB->AllocatedMLEs, VDB->mlefreelist.count(), VDB->MaxMLEs);
    }

    RecovFlush(1);
    RecovTruncate(1);

    /* Grab a refcount on the local (repair) volume to avoid automatic garbage
     * collection */
    VolumeId LocalVid;
    FID_MakeVolFake(&LocalVid);
    VDB->Find(LocalVid)->hold();

    /* Fire up the daemon. */
    VOLD_Init();
}


int VOL_HashFN(void *key)
{
    return(*((VolumeId *)key));
}


int GetRootVolume()
{
    if (RootVolName)
	goto found_rootvolname;

    /* By using the copy of the rootvolume name stored in RVM we avoid the 15
     * second timeout when starting disconnected. But we do lose the ability to
     * verify whether the rootvolume has changed. This is not a real problem,
     * since the cached state often is wrong if the rootvolume changes without
     * reinitializing venus. */

    /* Dig RVN out of recoverable store if possible. */
    if (rvg->recov_RootVolName[0] != '\0') {
	RootVolName = new char[V_MAXVOLNAMELEN];
	strncpy(RootVolName, rvg->recov_RootVolName, V_MAXVOLNAMELEN);
	goto found_rootvolname;
    }

    /* If we don't already know the root volume name ask the servers for it. */
    {
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
	    goto failure;
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

failure:
	if (code != 0) {
	    eprint("GetRootVolume: can't get root volume name!");
	    return(0);
	}

	RootVolName = new char[V_MAXVOLNAMELEN];
	strncpy(RootVolName, (char *)RVN.SeqBody, V_MAXVOLNAMELEN);
    }

found_rootvolname:
    /* Make sure the rootvolume name is stored in RVM. */
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
	eprint("GetRootVolume: can't get volinfo for root volume (%s)!",
	       RootVolName);
	return(0);
    }

    rootfid.Volume = v->GetVid();
    FID_MakeRoot(&rootfid);
    VDB->Put(&v);

    return(1);
}



/* Allocate database from recoverable store. */
void *vdb::operator new(size_t len)
{
    vdb *v = 0;

    v = (vdb *)rvmlib_rec_malloc((int)len);
    CODA_ASSERT(v);
    return(v);
}

vdb::vdb() : volrep_hash(VDB_NBUCKETS, VOL_HashFN), repvol_hash(VDB_NBUCKETS, VOL_HashFN)
{
    /* Initialize the persistent members. */
    RVMLIB_REC_OBJECT(*this);
    MagicNumber = VDB_MagicNumber;
    MaxMLEs = MLEs;
    AllocatedMLEs = 0;
}


void vdb::ResetTransient()
{
    /* Sanity checks. */
    if (MagicNumber != VDB_MagicNumber)
	CHOKE("vdb::ResetTransient: bad magic number (%d)", MagicNumber);

    volrep_hash.SetHFn(VOL_HashFN);
    repvol_hash.SetHFn(VOL_HashFN);
}

void vdb::operator delete(void *deadobj, size_t len) {
    abort(); /* what else? */
}

volent *vdb::Find(VolumeId volnum)
{
    repvol_iterator rvnext(&volnum);
    volrep_iterator vrnext(&volnum);
    volent *v;

    while ((v = rvnext()) || (v = vrnext()))
        if (v->GetVid() == volnum) return(v);

    return(0);
}


volent *vdb::Find(char *volname)
{
    repvol_iterator rvnext;
    volrep_iterator vrnext;
    volent *v;

    while ((v = rvnext()) || (v = vrnext()))
        if (STREQ(v->GetName(), volname)) return(v);

    return(0);
}

/* MUST NOT be called from within transaction! */
volent *vdb::Create(VolumeInfo *volinfo, char *volname)
{
    volent *v = 0;

    /* Check whether the key is already in the database. */
    if ((v = Find(volinfo->Vid))) {
/*	{ v->print(logFile); CHOKE("vdb::Create: key exists"); }*/
	eprint("reinstalling volume %s (%s)", v->GetName(), volname);

	Recov_BeginTrans();
	rvmlib_set_range(v->name, V_MAXVOLNAMELEN);
	strcpy(v->name, volname);

        /* add code to support growing/shrinking VSG's for replicated volumes
         * and moving volume replicas between hosts */

	Recov_EndTrans(0);

	return(v);
    }

    /* Fashion a new object. */
    switch(volinfo->Type) {
    case REPVOL:
        {
        repvol *vp;
        volrep *vsg[VSG_MEMBERS];
        int i, err = 0;

        memset(vsg, 0, VSG_MEMBERS * sizeof(volrep *));
        for (i = 0; i < VSG_MEMBERS && !err; i++) {
            if (!(&volinfo->RepVolMap.Volume0)[i]) continue;
            
            err = VDB->Get((volent **)&vsg[i],(&volinfo->RepVolMap.Volume0)[i]);
            if (!err) err = vsg[i]->IsReplicated();
        }

        if (!err) {
            /* instantiate the new replicated volume */
            Recov_BeginTrans();
            vp = new repvol(volinfo->Vid, volname, vsg);
            v = vp;
            Recov_EndTrans(MAXFP);
        }
        /* we can safely put the replicas as the new repvol has grabbed
         * refcounts on all of them */
        for (i = 0; i < VSG_MEMBERS; i++)
            if (vsg[i]) VDB->Put((volent **)&vsg[i]);
        break;
        }

    case ROVOL: /* Readonly replicated volume are not supported */
        break;

    case RWVOL:
    case BACKVOL:
        {
        volrep *vp;
        struct in_addr srvaddr;
        srvaddr.s_addr = htonl(volinfo->Server0);
        
        /* instantiate the new volume replica */
        Recov_BeginTrans();
        vp = new volrep(volinfo->Vid, volname, &srvaddr,
                        volinfo->Type == BACKVOL,
                        (&volinfo->Type0)[replicatedVolume]);
        v = vp;
        Recov_EndTrans(MAXFP);
        break;
        }
    }

    if (v == 0)
	LOG(0, ("vdb::Create: (%x, %s, %d) failed\n", volinfo->Vid, volname, 0/*AllocPriority*/));
    return(v);
}


/* MUST NOT be called from within transaction! */
int vdb::Get(volent **vpp, VolumeId vid)
{
    LOG(100, ("vdb::Get: vid = %x\n", vid));

    *vpp = 0;

    if (vid == 0)
	return(VNOVOL);

    /* First see if it's already in the table by number. */
    volent *v = Find(vid);
    if (v) {
	v->hold();
	*vpp = v;
	return(0);
    }

    /* If not, get it by name (synonym). */
    char volname[20];

#if 1 /* XXX change this to hex after 5.3.9 servers are deployed */
    sprintf(volname, "%lu", vid);
#else
    sprintf(volname, "%#08lx", vid);
#endif

    return(Get(vpp, volname));
}


/* MUST NOT be called from within transaction! */
/* This call ALWAYS goes through to servers! */
int vdb::Get(volent **vpp, char *volname)
{
    int code = 0;

    LOG(100, ("vdb::Get: volname = %s\n", volname));

    *vpp = 0;

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
	if (v->GetVid() == volinfo.Vid) {
	    /* Mapping unchanged. */
	    /* Should we see whether any other info has changed (e.g., VSG)?. */

            /* add code to support growing/shrinking VSG's for replicated
             * volumes and moving volume replicas between hosts */
	    goto Exit;
	}
	else {
	    eprint("Mapping changed for volume %s (%x --> %x)",
		   volname, v->GetVid(), volinfo.Vid);

	    /* Put a (unique) fakename in the old volent. */
	    char fakename[V_MAXVOLNAMELEN];
	    sprintf(fakename, "%lu", v->GetVid());
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

    /* Attempt the create. */
    v = Create(&volinfo, volname);

    if (!v) {
	LOG(0, ("vdb::Get: Create (%x, %s) failed\n", volinfo.Vid, volname));
	*vpp = NULL;
	return EIO;
    }

Exit:
    v->hold();
    *vpp = v;
    return(0);
}


void vdb::Put(volent **vpp)
{
    if (!*vpp) return;

    volent *v = *vpp;
    LOG(100, ("vdb::Put: (%x, %s), refcnt = %d\n",
	       v->GetVid(), v->GetName(), v->refcnt));
    *vpp = 0;

    v->release();
}


void vdb::DownEvent(struct in_addr *host)
{
    LOG(10, ("vdb::DownEvent: host = %s\n", inet_ntoa(*host)));

    /* Notify each volume of its failure. We only need to notify underlying
     * replicas, they will notify their replicated parent */
    volrep_iterator next;
    volrep *v;
    while ((v = next()))
        if (v->IsHostedBy(host))
            v->DownMember();

    /* provoke state transitions now */
    VprocSignal(&voldaemon_sync);
}

void vdb::UpEvent(struct in_addr *host)
{
    LOG(10, ("vdb::UpEvent: host = %s\n", inet_ntoa(*host)));

    /* Notify each volume of its success. We only need to notify underlying
     * replicas, they will notify their replicated parent */
    volrep_iterator next;
    volrep *v;
    while ((v = next()))
        if (v->IsHostedBy(host))
            v->UpMember();

    /* provoke state transitions now */
    VprocSignal(&voldaemon_sync);
}

void vdb::WeakEvent(struct in_addr *host)
{
    LOG(10, ("vdb::WeakEvent: host = %s\n", inet_ntoa(*host)));

    /* Notify each volume of its failure. We only need to notify underlying
     * replicas, they will notify their replicated parent */
    volrep_iterator next;
    volrep *v;
    while ((v = next()))
        if (v->IsHostedBy(host))
            v->WeakMember();

    /* provoke state transitions now */
    VprocSignal(&voldaemon_sync);
}

void vdb::StrongEvent(struct in_addr *host)
{
    LOG(10, ("vdb::StrongEvent: host = %s\n", inet_ntoa(*host)));

    /* Notify each volume of its failure. We only need to notify underlying
     * replicas, they will notify their replicated parent */
    volrep_iterator next;
    volrep *v;
    while ((v = next()))
        if (v->IsHostedBy(host))
            v->StrongMember();

    /* provoke state transitions now */
    VprocSignal(&voldaemon_sync);
}


/* MUST be called from within transaction! */
void vdb::AttachFidBindings()
{
    repvol_iterator next;
    repvol *v;
    while ((v = next()))
	v->CML.AttachFidBindings();
}


int vdb::WriteDisconnect(unsigned age, unsigned time)
{
    repvol_iterator next;
    repvol *v;
    int code = 0;

    while ((v = next())) {
        code = v->WriteDisconnect(age, time); 
        if (code) break;
    }
    return(code);
}


int vdb::WriteReconnect()
{
    repvol_iterator next;
    repvol *v;
    int code = 0;

    while ((v = next())) {
        code = v->WriteReconnect();
        if (code) break;
    }
    return(code);
}


void vdb::GetCmlStats(cmlstats& total_current, cmlstats& total_cancelled)
{
    /* N.B.  We assume that caller has passed in zeroed-out structures! */
    repvol_iterator next;
    repvol *v;
    while ((v = next())) {
	cmlstats current;
	cmlstats cancelled;
	v->CML.IncGetStats(current, cancelled);
	total_current += current;
	total_cancelled += cancelled;
    }
}


void vdb::print(int fd, int SummaryOnly)
{
    if (this == 0) return;

    fdprint(fd, "VDB:\n");
    fdprint(fd, "volrep count = %d, repvol count = %d, mlefl count = %d\n",
            volrep_hash.count(), repvol_hash.count(), mlefreelist.count());
    fdprint(fd, "volume callbacks broken = %d, total callbacks broken = %d\n",
	    vcbbreaks, cbbreaks);
    if (!SummaryOnly) {
        repvol_iterator rvnext;
        volrep_iterator vrnext;
        volent *v;
        while ((v = rvnext()) || (v = vrnext())) v->print(fd);
    }

    fdprint(fd, "\n");
}


void vdb::ListCache(FILE *fp, int long_format, unsigned int valid)
{
    repvol_iterator rvnext;
    volrep_iterator vrnext;
    volent *v = 0;

    while ((v = rvnext()) || (v = vrnext()))
        v->ListCache(fp, long_format, valid);
}

void vdb::MgrpPrint(int fd)
{
    repvol *v = 0;
    repvol_iterator next;

    fdprint(fd, "Mgroups:\n");
    while ((v = next()))
        v->MgrpPrint(fd);
    fdprint(fd, "\n");

}

void repvol::MgrpPrint(int fd)
{
    struct dllist_head *p;

    list_for_each(p, mgrpents) {
        mgrpent *m = list_entry(p, mgrpent, volhandle);
        m->print(fd);
    }
}


/* local-repair modification */

/* MUST be called from within transaction! */
void *volent::operator new(size_t len)
{
    volent *v;
    v = (volent *)rvmlib_rec_malloc((int)len);
    CODA_ASSERT(v);
    return(v);
}

/* MUST be called from within transaction! */
volent::volent(VolumeId volid, char *volname)
{
    LOG(10, ("volent::volent: (%x, %s)\n", volid, volname));

    int ret;

    RVMLIB_REC_OBJECT(*this);
    MagicNumber = VOLENT_MagicNumber;
    vid = volid;
    strcpy(name, volname);

    flags.reserved = 0;
    flags.has_local_subtree = 0;
    flags.logv = 0;
    lc_asr = -1;

    /* Writeback */
    flags.writebacking = 0;
    flags.writebackreint = 0;
    flags.sync_reintegrate = 0;
    flags.staylogging = 0;
    flags.autowriteback = 0;
}


/* local-repair modification */
void volent::ResetVolTransients()
{
    state = Hoarding;
    observer_count = 0;
    mutator_count = 0;
    waiter_count = 0;
    shrd_count = 0;
    excl_count = 0;
    excl_pgid = 0;
    resolver_count = 0;
    flags.transition_pending = 0;
    flags.demotion_pending = 0;
    flags.allow_asrinvocation = 1;
    flags.asr_running = 0;
    flags.reintegratepending = 0;
    flags.reintegrating = 0;
    flags.repair_mode = 0;		    /* normal mode */
    flags.resolve_me = 0;
    flags.weaklyconnected = 0;
    flags.available = 1;

    fso_list = new olist;

    /* 
     * sync doesn't need to be initialized. 
     * It's used only for LWP_Wait and LWP_Signal. 
     */
    refcnt = 0;
}


/* MUST be called from within transaction! */
volent::~volent()
{
    LOG(10, ("volent::~volent: name = %s, volume = %x, refcnt = %d\n",
	      name, vid, refcnt));

    /* Drain and delete transient lists. */
    {
	if (fso_list->count() != 0)
	    CHOKE("volent::~volent: fso_list not empty");
	delete fso_list;
    }

    if (refcnt != 0)
	{ print(logFile); CHOKE("volent::~volent: non-zero refcnt"); }
}

/* MUST be called from within transaction! */
void volent::operator delete(void *deadobj, size_t len)
{
    LOG(10, ("volent::operator delete()\n"));
    rvmlib_rec_free(deadobj);
}

void volent::hold() {
    refcnt++;
}

void volent::release() {
    refcnt--;

    CODA_ASSERT(refcnt >= 0);
}

int volent::IsReadWriteReplica()
{
    if (IsReplicated()) return 0;

    return (((volrep *)this)->ReplicatedVol() != 0);
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
int volent::Enter(int mode, vuid_t vuid)
{
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

        if (IsReplicated()) ((repvol *)this)->ClearCallBack();

        fso_vol_iterator next(NL, this);
        fsobj *f;
        while ((f = next()))
            f->Demote();

        just_transitioned = 1;
    } 

    /* Step 2 is to take pending transition for the volume IF no thread is active in it. */
    while (flags.transition_pending && !VOLBUSY(this)) {
        TakeTransition();
        just_transitioned = 1;
    }

    /* Step 3 is to try to get a volume callback. */
    /* We allow only the hoard thread to fetch new version stamps if we do not
     * already have one. If we do have stamps, we let other threads validate
     * them with one condition.  
     * The wierd condition below is to prevent the vol daemon from validating
     * volumes one at a time.  That is, if the volume has just taken a
     * transition or was just demoted, there is a good chance some other
     * volumes have as well.
     * We'd like them all to take transitions/demotions first so we can check
     * them en masse.  We risk sticking a real request with this overhead, but
     * only if a request arrives in the next few (5) seconds!  
     */
    vproc *vp = VprocSelf();
    if (VCBEnabled && IsReplicated() && 
        (state == Hoarding || state == Logging) && 
        ((repvol *)this)->WantCallBack())
    {
        repvol *rv = (repvol *)this;
	if ((!rv->HaveStamp() && vp->type == VPT_HDBDaemon) ||
            (rv->HaveStamp() && (vp->type != VPT_VolDaemon || !just_transitioned)))
        {
            // we already checked if this volume is replicated
	    int code = rv->GetVolAttr(vuid);
	    LOG(100, ("volent::Enter: GetVolAttr(0x%x) returns %s\n",
		      vid, VenusRetStr(code)));
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
		int proc_key = vp->u.u_pgid;
		while ((excl_count > 0 && proc_key != excl_pgid) ||
                       state == Resolving ||
                       (IsReplicated() && WriteLocked(&((repvol *)this)->CML_lock)) ||
                       flags.transition_pending) {
		    if (mode & VM_NDELAY) return (EWOULDBLOCK);
		    LOG(0, ("volent::Enter: mutate with proc_key = %d\n",
			    proc_key));
		    Wait();
		    if (VprocInterrupted()) return (EINTR);
		}

		/*
		 * mutator needs to aquire exclusive CML ownership
		 */
		if (IsReplicated()) {
                    repvol *rv = (repvol *)this;
		    /* 
		     * Claim ownership if the volume is free. 
		     * The CML lock is used to prevent checkpointers and
		     * mutators from executing in the volume simultaneously,
		     * because the CML must not change during a checkpoint.
		     * We want shared/exclusive behavior, so all mutators
		     * obtain a shared (read) lock on the CML to prevent the
		     * checkpointer from entering. Note observers don't lock at
		     * all. 
		     */
		    if (rv->GetCML()->Owner() == UNSET_UID) {
                        if (mutator_count != 0 || rv->GetCML()->count() != 0 ||
                            rv->IsReintegrating())
			    { print(logFile); CHOKE("volent::Enter: mutating, CML owner == %d\n", rv->GetCML()->Owner()); }

			mutator_count++;
			rv->GetCML()->owner = vuid;
			shrd_count++;
			ObtainReadLock(&rv->CML_lock);
			return(0);
		    }

		    /* Continue using the volume if possible. */
		    /* We might need to do something about fairness here
		     * eventually! -JJK */
		    if (rv->GetCML()->Owner() == vuid) {
                        if (mutator_count == 0 && rv->GetCML()->count() == 0 &&
                            !rv->IsReintegrating())
			    { print(logFile); CHOKE("volent::Enter: mutating, CML owner == %d\n", rv->GetCML()->Owner()); }

			mutator_count++;
			shrd_count++;
			ObtainReadLock(&rv->CML_lock);
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
		int proc_key = vp->u.u_pgid;
		while ((excl_count > 0 && proc_key != excl_pgid) || state == Resolving
		        || flags.transition_pending) {
		    if (mode & VM_NDELAY) return (EWOULDBLOCK);
		    LOG(0, ("volent::Enter: observe with proc_key = %d\n",
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
	    CODA_ASSERT(IsReplicated());
	    if (state != Resolving || resolver_count != 0 || 
		mutator_count != 0 || observer_count != 0 ||
		flags.transition_pending)
		{ print(logFile); CHOKE("volent::Enter: resolving"); }

	    /* acquire exclusive volume-pgid-lock for RESOLVING */
	    int proc_key = vp->u.u_pgid;
	    while (shrd_count > 0 || (excl_count > 0 && proc_key != excl_pgid)) {
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
void volent::Exit(int mode, vuid_t vuid)
{
    LOG(1000, ("volent::Exit: vol = %x, state = %s, t_p = %d, d_p = %d, mode = %s, vuid = %d\n",
		vid, PRINT_VOLSTATE(state), flags.transition_pending,
		flags.demotion_pending, PRINT_VOLMODE(mode), vuid));

    /* 
     * Step 1 is to demote objects in volume if AVSG enlargement or shrinking
     * has made this necessary.  The two cases that require this are: 
     *    1. |AVSG| for read-write replicated volume increasing. 
     *    2. |AVSG| for non-replicated volume falling to 0. 
     */
    if (flags.demotion_pending) {
	LOG(1, ("volent::Exit: demoting %s\n", name));
	flags.demotion_pending = 0;

        if (IsReplicated()) ((repvol *)this)->ClearCallBack();

	fso_vol_iterator next(NL, this);
	fsobj *f;
	while ((f = next()))
	    f->Demote();
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
            if (IsReplicated())
                ReleaseReadLock(&((repvol *)this)->CML_lock);
            if (IsReplicated() && mutator_count == 0 &&
                ((repvol *)this)->GetCML()->count() == 0 &&
                !((repvol *)this)->IsReintegrating()) {
		/* Special-case here. */
                /* If we just cancelled the last log record for a volume that
                 * was being kept in Emulating state due to auth-token
                 * absence, we need to provoke a transition! */
		if (state == Emulating && !flags.transition_pending)
		    flags.transition_pending = 1;

		((repvol *)this)->GetCML()->owner = UNSET_UID;
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
	    CODA_ASSERT(IsReplicated());
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
    if (!VOLBUSY(this))                     /* signal when volume is not busy */
	VprocSignal((char*)&(this->shrd_count),0);
}

/* local-repair modification */
void volent::TakeTransition()
{
    CODA_ASSERT(flags.transition_pending && !VOLBUSY(this));

    int size = AVSGsize();
    repvol *rv = NULL;
    if (IsReplicated()) rv = (repvol *)this;

    LOG(1, ("volent::TakeTransition: %s, state = %s, |AVSG| = %d, logv = %d\n",
	     name, PRINT_VOLSTATE(state), size, flags.logv));
    if (IsReplicated())
        LOG(1, ("\tCML = %d, Res = %d\n",
                rv->GetCML()->count(), rv->ResListCount()));

    /* Compute next state. */
    VolumeStateType nextstate;
    VolumeStateType prevstate = state;

    CODA_ASSERT(state == Hoarding || state == Emulating || state == Logging ||
		state == Resolving);

    nextstate = Hoarding;
    /* if the AVSG is empty we are disconnected */
    if (size == 0) nextstate = Emulating;

    else if (IsReplicated()) {
        if (rv->ResListCount())
            nextstate = Resolving;

        else if (flags.logv || flags.staylogging || rv->GetCML()->count())
            nextstate = Logging;
    }

    /* Special cases here. */
    /*
     * 1.  If the volume is transitioning _to_ emulating, any reintegations
     *     will not be stopped because of lack of tokens.
     */
    if (nextstate == Emulating && IsReplicated())
	rv->ClearReintegratePending();

    /* 2. We refuse to transit to reintegration unless owner has auth tokens.
     * 3. We force "zombie" volumes to emulation state until they are
     *    un-zombied. */
    if (nextstate == Logging && rv->GetCML()->count() > 0) {
	userent *u = 0;
	GetUser(&u, rv->GetCML()->Owner());
	if (!u->TokensValid()) {
	    rv->SetReintegratePending();
	    nextstate = Emulating;
	}
	PutUser(&u);
    }

    /* Take corresponding action. */
    state = nextstate;
    flags.transition_pending = 0;

#if 0
    if (SkkEnabled && state != prevstate)
        NotifyStateChange();
#endif

    switch(state) {
        case Logging:
	    if (rv->ReadyToReintegrate()) 
		::Reintegrate(rv);
            // Fall through

        case Hoarding:
        case Emulating:
	    Signal();
	    break;

	case Resolving:
	    ::Resolve(this);
	    break;

	default:
	    CODA_ASSERT(0);
    }

    /* Bound RVM persistence out of paranoia. */
    Recov_SetBound(DMFP);
}

void volrep::DownMember(void)
{
    LOG(10, ("volrep::DownMember: vid = %08x\n", vid));

    flags.transition_pending = 1;
    flags.available = 0;
	
    /* Coherence is now suspect for all objects in read-write volumes. */
    if (!IsBackup())
        flags.demotion_pending = 1;

    /* if we are a volume replica notify our replicated parent */
    if (IsReadWriteReplica()) {
        volent *v = VDB->Find(ReplicatedVol());
        if (v) {
            CODA_ASSERT(v->IsReplicated());
            ((repvol *)v)->DownMember();
        }
    }
}

void repvol::DownMember(void)
{
    /* ignore events from VSG servers when a staging server is used */
    if (ro_replica && !ro_replica->TransitionPending())
	return;

    LOG(10, ("repvol::DownMember: vid = %08x\n", vid));

    ResetStats();

    /* Consider transitioning to Emulating state. */
    if (AVSGsize() == 0) {
	flags.transition_pending = 1;
        /* Coherence is now suspect */
        flags.demotion_pending = 1;
    }
}

void volrep::UpMember(void)
{
    LOG(10, ("volrep::UpMember: vid = %08x\n", vid));
    flags.transition_pending = 1;
    flags.available = 1;

    /* if we are a volume replica notify our replicated parent */
    if (IsReadWriteReplica()) {
        volent *v = VDB->Find(ReplicatedVol());
        if (v) {
            CODA_ASSERT(v->IsReplicated());
            ((repvol *)v)->UpMember();
        }
    }
}

void repvol::UpMember(void)
{
    /* ignore events from VSG servers when a staging server is used */
    if (ro_replica && !ro_replica->TransitionPending())
	return;

    LOG(10, ("repvol::UpMember: vid = %08x\n", vid));

    /* Consider transitting to Hoarding state. */
    if (AVSGsize() == 1)
	flags.transition_pending = 1;

    /* Coherence is now suspect for all objects in replicated volumes. */
    flags.demotion_pending = 1;
    ResetStats();
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
void volrep::WeakMember()
{
    /* if we are a volume replica notify our replicated parent */
    if (IsReadWriteReplica()) {
        volent *v = VDB->Find(ReplicatedVol());
        if (v) {
            CODA_ASSERT(v->IsReplicated());
            ((repvol *)v)->WeakMember();
        }
    }
}

void repvol::WeakMember()
{
    /* Normally a weakmember event implies that WeakVSGSize > 0, however
     * we might get weak events from VSG servers when using a staging server. */
    if (!IsWeaklyConnected() && WeakVSGSize() > 0) {
        WriteDisconnect();

        flags.transition_pending = 1;
	flags.weaklyconnected = 1;
    }
}

void volrep::StrongMember()
{
    /* if we are a volume replica notify our replicated parent */
    if (IsReadWriteReplica()) {
        volent *v = VDB->Find(ReplicatedVol());
        if (v) {
            CODA_ASSERT(v->IsReplicated());
            ((repvol *)v)->StrongMember();
        }
    }
}

void repvol::StrongMember()
{
    /* vsg check is for 0, not 1, because the conn is already strong */
    if (WeakVSGSize() == 0) {
        WriteReconnect();

        flags.transition_pending = 1;
	flags.weaklyconnected = 0;
    }
}

int repvol::WriteDisconnect(unsigned age, unsigned time)
{
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

    return 0;
}


int repvol::WriteReconnect()
{
    Recov_BeginTrans();
        RVMLIB_REC_OBJECT(*this);
        flags.logv = 0;
        AgeLimit = V_UNSETAGE;
        ReintLimit = V_UNSETREINTLIMIT;
    Recov_EndTrans(MAXFP);
    flags.transition_pending = 1;

    return 0;
}

int repvol::EnterWriteback(vuid_t vuid)
{
    // already in writebacking mode
    if (flags.writebacking) return 0;

    LOG(1, ("volent::EnterWriteback()\n"));

    /* request a permit */
    if (!GetPermit(vuid)) return ERETRY; // probably not the right error

    Recov_BeginTrans();
        RVMLIB_REC_OBJECT(*this);
        flags.writebacking = 1;
        /* copied from initializing WriteDisconnect */
        flags.logv = 1;
        flags.staylogging = 1;
        AgeLimit = V_DEFAULTAGE; // do i want this?
        ReintLimit = V_DEFAULTREINTLIMIT;
    Recov_EndTrans(MAXFP);
    flags.transition_pending = 1;

    return 0;
}

int repvol::LeaveWriteback(vuid_t vuid)
{
    LOG(1, ("volent::LeaveWriteback()\n"));

    StopWriteback(NULL);
    ReturnPermit(vuid);
    ClearPermit();

    return 0;
}

int repvol::SyncCache(ViceFid * fid)
{
    LOG(1,("volent::SyncCache()\n"));

    //	    if (flags.transition_pending && VOLBUSY(this)) { 
    // wait until they get out of our volume

    while (VOLBUSY(this))
        VprocWait((char*)&shrd_count);

    flags.transition_pending = 1;
    flags.sync_reintegrate = 1;
    reintegrate_done = NULL;   //do the whole volume	    
    /* reintegrate_done = findDepenents(fid); */

    if (flags.transition_pending) { 
        // make sure no one else took our transition
        TakeTransition();
    }
    ::Reintegrate(this);       // this is safe to call
    flags.sync_reintegrate = 0;

    return 0;
}

int repvol::StopWriteback(ViceFid *fid)
{
    int code = 0;

    LOG(1, ("volent::StopWriteback()\n"));

    /* not writebacking?  nothing to do! */
    if (!flags.writebacking) return 0;

    ClearPermit();
    Recov_BeginTrans();
        RVMLIB_REC_OBJECT(*this);
        flags.writebacking = 0;
        flags.writebackreint = 1;
        /* copied from WriteReconnect */
        flags.logv = 0;
        flags.staylogging = 0;
        AgeLimit = V_UNSETAGE;
        ReintLimit = V_UNSETREINTLIMIT;
    Recov_EndTrans(MAXFP);

    code = SyncCache(fid);
    flags.writebackreint = 0;
    /* Signal done */

    return (code);
}

void repvol::SetReintegratePending() {
    flags.reintegratepending = 1;
    CheckReintegratePending();
}


void repvol::ClearReintegratePending() {
    flags.reintegratepending = 0;
    /* if (SkkEnabled) {
     *    userent *u;
     *    GetUser(&u, CML.owner);
     *    CODA_ASSERT(u != NULL);
     *    u->NotifyReintegrationEnabled(name);
     * } */
}


void repvol::CheckReintegratePending() {
    if (flags.reintegratepending && CML.count() > 0) {
        eprint("Reintegrate %s pending tokens for uid = %d", name, CML.owner);
	/* if (SkkEnabled) {
         *    userent *u;
         *    GetUser(&u, CML.owner);
         *    CODA_ASSERT(u != NULL);
         *    u->NotifyReintegrationPending(name);
	 * } */
    }
}


void volent::Wait()
{
    waiter_count++;
    LOG(0, ("WAITING(VOL): %s, state = %s, [%d, %d], counts = [%d %d %d %d]\n",
	     name, PRINT_VOLSTATE(state), flags.transition_pending, flags.demotion_pending,
 	     observer_count, mutator_count, waiter_count, resolver_count));
    if (IsReplicated()) {
        repvol *rv = (repvol *)this;
        LOG(0, ("CML= [%d, %d], Res = %d\n", rv->GetCML()->count(),
                rv->GetCML()->Owner(), rv->ResListCount()));
    }
    LOG(0, ("WAITING(VOL): shrd_count = %d, excl_count = %d, excl_pgid = %d\n",
	    shrd_count, excl_count, excl_pgid));
    START_TIMING();
    VprocWait(&vol_sync);
    END_TIMING();
    LOG(0, ("WAIT OVER, elapsed = %3.1f\n", elapsed));
    waiter_count--;
}


void volent::Signal() {
    VprocSignal(&vol_sync);
}

void volent::Lock(VolLockType l, int pgid)
{
    /* Sanity Check */
    if (l != EX_VOL_LK && l != SH_VOL_LK) {
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

    while (l == SH_VOL_LK ? (excl_count > 0 && excl_pgid != pgid) : 
	   (shrd_count > 0 || (excl_count > 0 && excl_pgid != pgid))) {
	LOG(0, ("volent::Lock: wait\n"));
	Wait();
    }
    l == EX_VOL_LK ? (excl_count++, excl_pgid = pgid) : (shrd_count++);
}

void volent::UnLock(VolLockType l)
{	
    LOG(100, ("volent::UnLock: (%s) lock = %d pgid = %d excl = %d shrd = %d\n",
	      name, l, excl_pgid, excl_count, shrd_count));

    /* Sanity Check */
    if (l != EX_VOL_LK && l != SH_VOL_LK) {
	print(logFile); 
	CHOKE("volent::UnLock bogus lock type");
    }

    if (excl_count < 0 || shrd_count < 0) {
	print(logFile); 
	CHOKE("volent::UnLock pgid = %d excl_count = %d shrd_count = %d",
	      excl_pgid, excl_count, shrd_count);
    }
    l == EX_VOL_LK ? (excl_count--) : (shrd_count--);
    if (0 == excl_count) 
      excl_pgid = 0;
    Signal();
}

volrep::volrep(VolumeId vid, char *name, struct in_addr *addr, int readonly,
               VolumeId parent) : volent(vid, name)
{
    LOG(10, ("volrep::volrep: host: %s readonly: %d parent: %#08x)\n",
             inet_ntoa(*addr), readonly, parent));

    RVMLIB_REC_OBJECT(*this);
    host = *addr;
    flags.readonly = readonly;
    replicated = parent;
    flags.replicated = 0;

    ResetTransient();

    /* Insert into hash table. */
    VDB->volrep_hash.insert(&vid, &handle);
}

volrep::~volrep()
{
    /* Remove from hash table. */
    if (VDB->volrep_hash.remove(&vid, &handle) != &handle)
	{ print(logFile); CHOKE("volrep::~volrep: htab remove"); }
}

void volrep::ResetTransient(void)
{
    list_head_init(&vollist);

    ResetVolTransients();
}

void repvol::ResetTransient(void)
{
    res_list = new olist;
    cop2_list = new dlist;
    list_head_init(&mgrpents);

    Lock_Init(&CML_lock);
    CML.ResetTransient();

    RecordsCancelled = 0;
    RecordsCommitted = 0;
    RecordsAborted = 0;
    FidsRealloced = 0;
    BytesBackFetched = 0;
    cur_reint_tid = UNSET_TID;

    VCBStatus = NoCallBack;
    VCBHits = 0;

    /* Pre-allocated Fids MUST be discarded if last shutdown was dirty
     * (because one of them may have actually been used in an object creation
     * at the servers, but we crashed before we took the fid off of our
     * queue). */
    if (!CleanShutDown &&
        (FileFids.Count != 0 || DirFids.Count != 0 || SymlinkFids.Count != 0))
    {
	Recov_BeginTrans();
	    RVMLIB_REC_OBJECT(FileFids);
	    FileFids.Count = 0;
	    RVMLIB_REC_OBJECT(DirFids);
	    DirFids.Count = 0;
	    RVMLIB_REC_OBJECT(SymlinkFids);
	    SymlinkFids.Count = 0;
	Recov_EndTrans(MAXFP);
    }

    /* grab a refcount on the underlying replicas */
    for (int i = 0; i < VSG_MEMBERS; i++)
        if (vsg[i]) vsg[i]->hold();

    /* don't grab a refcount to the staging server volume, it will get
     * purged by the VolDaemon automatically */
    ro_replica = NULL;

    ResetVolTransients();
}

int volent::Collate(connent *c, int code, int TranslateEINCOMP)
{
    code = c->CheckResult(code, vid, TranslateEINCOMP);

    /* when the operation has failed miserably, but we have a pending volume
     * transition, just retry the operation */
    if (code && flags.transition_pending)
	code = ERETRY;

    return(code);
}

repvol::repvol(VolumeId vid, char *name, volrep *volreps[VSG_MEMBERS]) : volent(vid, name)
{
    LOG(10, ("repvol::repvol %08x %08x %08x %08x %08x %08x %08x %08x\n",
             volreps[0], volreps[1], volreps[2], volreps[3],
             volreps[4], volreps[5], volreps[6], volreps[7]));

    RVMLIB_REC_OBJECT(*this);
    memcpy(vsg, volreps, VSG_MEMBERS * sizeof(volrep *));

    VVV = NullVV;
    AgeLimit = V_UNSETAGE;
    ReintLimit = V_UNSETREINTLIMIT;
    reint_id_gen = 100;
    flags.replicated = 1;

    /* The uniqifiers should also survive shutdowns, f.i. the server
       remembers the last sid we successfully reintegrated. And in
       disconnected mode the unique fids map to unique inodes. The
       1<<19 shift is to avoid collisions with inodes derived from
       non-local generated fids -- JH */
    FidUnique = 1 << 19;
    SidUnique = 0;

    ResetTransient();

    /* Insert into hash table. */
    VDB->repvol_hash.insert(&vid, &handle);
}

repvol::~repvol()
{
    LOG(10, ("repvol::~repvol: name = %s, volume = %x, type = ReplicatedVolume\n", name, vid));
    
    int i;

    /* Remove from hash table. */
    if (VDB->repvol_hash.remove(&vid, &handle) != &handle)
	{ print(logFile); CHOKE("repvol::~repvol: htab remove"); }

    /* Unlink from VSG (if applicable). */
    for (i = 0; i < VSG_MEMBERS; i++)
        VDB->Put((volent **)&vsg[i]);

    if (ro_replica)
	VDB->Put((volent **)&ro_replica);

    if (CML.count() != 0)
        CHOKE("volent::~volent: CML not empty");

    if (res_list->count() != 0)
        CHOKE("repvol::~repvol: res_list not empty");
    delete res_list;

    if (cop2_list->count() != 0)
        CHOKE("volent::~volent: cop2_list not empty");
    delete cop2_list;
    
    KillMgrps();
}

void repvol::KillMgrps(void)
{
    LOG(10, ("repvol::KillMgrps volume = %x\n", vid));

    while (mgrpents.next != &mgrpents) {
        mgrpent *m = list_entry(mgrpents.next, mgrpent, volhandle);
        m->Suicide(1); /* takes `m' out of the mgrpents list */
    }
}

int volrep::GetConn(connent **c, vuid_t vuid)
{
    int code = ERETRY;
    *c = 0;

    while (code == ERETRY && !flags.transition_pending) {
        code = ::GetConn(c, &host, vuid, 0);
        if (code < 0)
            CHOKE("volent::GetConn: bogus code (%d)", code);
    }
    if (flags.transition_pending)
	code = ERETRY;

    return(code);
}

#if 0
int repvol::GetConn(connent **c, vuid_t vuid)
{
    int code = ETIMEDOUT;
    unsigned long Hosts[VSG_MEMBERS];
    *c = 0;

    /* Get a connection to any custodian. */
    GetHosts(Hosts);
    for (int i = 0; i < VSG_MEMBERS && !flags.transition_pending; i++) {
	if (replica[i]->host) {
	    do {
		code = ::GetConn(c, replica[i]->host, vuid, 0);
		if (code < 0)
		    CHOKE("volent::GetConn: bogus code (%d)", code);
	    } while (code == ERETRY && !flags.transition_pending);

	    if (code != ETIMEDOUT)
		break;
	}
    }

    if (flags.transition_pending)
	code = ERETRY;
    return(code);
}
#endif

const int MAXMGRPSPERUSER = 3;  /* Max simultaneous mgrps per user per vol. */

int repvol::GetMgrp(mgrpent **m, vuid_t vuid, RPC2_CountedBS *PiggyBS)
{
    *m = 0;
    int code = 0;

    if (flags.transition_pending)
        return ERETRY;

    /* Get an mgrp for this user. */ 
try_again:
    /* Check whether there is already a free mgroup. */
    struct dllist_head *p;
    int count = 0;

    list_for_each(p, mgrpents) {
        *m = list_entry(p, mgrpent, volhandle);
        if (vuid != ALL_UIDS && vuid != (*m)->uid) continue;
        count++;
        if ((*m)->inuse) continue;

        (*m)->inuse = 1;
        goto got_mgrp;
    }
    
    /* Wait here if MAX mgrps are already in use. */
    if (count >= MAXMGRPSPERUSER) {
        *m = 0;
	if (VprocInterrupted()) { return(EINTR); }
        Mgrp_Wait();
	if (VprocInterrupted()) { return(EINTR); }
        goto try_again;
    }

    /* Try to connect to the VSG on behalf of the user. */
    {
        RPC2_Handle MgrpHandle = 0;
        int auth = 1;
        userent *u = 0;
        struct in_addr mgrpaddr;
        mgrpaddr.s_addr = INADDR_ANY; /* Request to form an mgrp */

	/* set up unauthenticated connections to the staging server */
	if (ro_replica) auth = 0;

        GetUser(&u, vuid);
        code = u->Connect(&MgrpHandle, &auth, &mgrpaddr);
        PutUser(&u);

        if (code < 0)
            CHOKE("repvol::GetMgrp: bogus code (%d) from u->Connect", code);

        if (code)
            return(code);

        /* Create and install the new mgrpent. */
        *m = new mgrpent(this, vuid, MgrpHandle, auth);
        list_add(&(*m)->volhandle, &mgrpents);
    }

got_mgrp:
    /* Form the host set. */
    code = (*m)->GetHostSet();
    if (code < 0)
        CHOKE("repvol::GetMgrp: bogus code (%d) from GetHostSet", code);

    if ((*m)->dying || code) {
        if (!code) code = ERETRY;
        goto exit;
    }

    /* Choose whether to multicast or not. XXX probably broken --JH */
    (*m)->rocc.MIp = (UseMulticast) ? &(*m)->McastInfo : 0;
    
/*--- We should have a usable mgrp now */

    /* Get PiggyCOP2 buffer if requested. */
    if (PiggyBS)
        code = FlushCOP2(*m, PiggyBS);

    if (flags.transition_pending)
	code = ERETRY;
exit: 
    if (code)
        ::PutMgrp(m);

    return(code);
}

void volrep::KillMgrpMember(struct in_addr *member)
{
    volent *v = VDB->Find(ReplicatedVol());
    if (v) {
        CODA_ASSERT(v->IsReplicated());
        ((repvol *)v)->KillMgrpMember(member);
    }
}

void repvol::KillMgrpMember(struct in_addr *member)
{
    struct dllist_head *p;

    list_for_each(p, mgrpents) {
        mgrpent *m = list_entry(p, mgrpent, volhandle);
        m->KillMember(member, 0);
    }
}

void repvol::KillUserMgrps(vuid_t uid)
{
    struct dllist_head *p;
    LOG(10, ("repvol::KillUserMgrps volume = %x, uid = %d\n", vid, uid));

again:
    for(p = mgrpents.next; p != &mgrpents;) {
        mgrpent *m = list_entry(p, mgrpent, volhandle);
        p = m->volhandle.next;

        if (m->uid != uid) continue;
        if (m->Suicide(1))
		/* We yielded in m->Suicide, have to restart the scan */
		goto again;
    }
}

/* returns minimum bandwidth in Bytes/sec, or INIT_BW if none obtainable */
void volent::GetBandwidth(unsigned long *bw)
{
    if (IsReplicated()) ((repvol *)this)->GetBandwidth(bw);
    else                ((volrep *)this)->GetBandwidth(bw);
}

void volrep::GetBandwidth(unsigned long *bw)
{
    srvent *s;
    *bw = INIT_BW;
    GetServer(&s, &host);
    s->GetBandwidth(bw);
    PutServer(&s);
}

void repvol::GetBandwidth(unsigned long *bw)
{
    struct in_addr host;
    srvent *s;
    *bw = 0;

    if (ro_replica) {
        ro_replica->GetBandwidth(bw);
        return;
    }

    for (int i = 0; i < VSG_MEMBERS; i++) {
        unsigned long tmpbw = 0;
        if (!vsg[i]) continue;

        vsg[i]->Host(&host);
        if (!host.s_addr) continue;

        vsg[i]->GetBandwidth(&tmpbw);

        if (tmpbw && (!*bw || tmpbw < *bw))
            *bw = tmpbw;
    }
    if (*bw == 0) *bw = INIT_BW;
}

int repvol::AllocFid(ViceDataType Type, ViceFid *target_fid,
		      RPC2_Unsigned *AllocHost, vuid_t vuid, int force)
{
    LOG(10, ("repvol::AllocFid: (%x, %d), uid = %d\n", vid, Type, vuid));

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
		CHOKE("repvol::AllocFid: bogus Type (%d)", Type);
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

	    LOG(100, ("repvol::AllocFid: target_fid = (%x.%x.%x)\n",
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
		int ph_ix; unsigned long ph;
                ph = ntohl(m->GetPrimaryHost(&ph_ix)->s_addr);

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
			    CHOKE("repvol::AllocFid: bogus Type (%d)", Type);
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
	LOG(10, ("repvol::AllocFid: target_fid = (%x.%x.%x)\n",
		 target_fid->Volume, target_fid->Vnode, target_fid->Unique));
    return(code);
}

void volent::GetHosts(struct in_addr hosts[VSG_MEMBERS])
{
    if (IsReplicated()) {
        ((repvol *)this)->GetHosts(hosts);
    } else {
        memset(hosts, 0, VSG_MEMBERS * sizeof(struct in_addr));
        ((volrep *)this)->Host(hosts);
    }
}

void repvol::GetHosts(struct in_addr hosts[VSG_MEMBERS])
{
    memset(hosts, 0, VSG_MEMBERS * sizeof(struct in_addr));

    if (ro_replica) {
	ro_replica->Host(&hosts[0]);
	return;
    }

    for (int i = 0; i < VSG_MEMBERS; i++)
        if (vsg[i])
            vsg[i]->Host(&hosts[i]);
}

int repvol::IsHostedBy(const struct in_addr *host)
{
    if (ro_replica)
	return ro_replica->IsHostedBy(host);

    for (int i = 0; i < VSG_MEMBERS; i++)
        if (vsg[i] && vsg[i]->IsHostedBy(host))
            return 1;

    return 0;
}

int volent::AVSGsize(void)
{
    if (IsReplicated()) return ((repvol *)this)->AVSGsize();
    else                return ((volrep *)this)->IsAvailable();
}

int repvol::AVSGsize(void)
{
    int avsgsize = 0;

    if (ro_replica)
	return ro_replica->IsAvailable() ? 1 : 0;
    
    for (int i = 0; i < VSG_MEMBERS; i++)
        if (vsg[i] && vsg[i]->IsAvailable())
            avsgsize++;

    return avsgsize;
}

int repvol::WeakVSGSize()
{
    int count = 0;
    
    if (ro_replica)
	return ro_replica->IsWeaklyConnected() ? 1 : 0;

    for (int i = 0; i < VSG_MEMBERS; i++)
        if (vsg[i] && vsg[i]->IsWeaklyConnected())
            count++;

    return(count);
}

/* volume id's for staging volumes */
static VolumeId stagingvid = 0xFF000000;
void repvol::SetStagingServer(struct in_addr *srvr)
{
    VolumeInfo StagingVol;
    char stagingname[V_MAXVOLNAMELEN];

    /* Disconnect existing mgrps and force a cache-revalidation */
    KillMgrps();
    flags.transition_pending = 1;
    flags.demotion_pending = 1;

    if (ro_replica)
	VDB->Put((volent **)&ro_replica);

    if (srvr->s_addr == 0) return;

    strcpy(stagingname, name);
    /* make sure we don't overflow stagingname when appending ".ro" */
    stagingname[V_MAXVOLNAMELEN-4] = '\0';
    strcat(stagingname, ".ro");

    memset(&StagingVol, 0, sizeof(VolumeInfo));
    StagingVol.Vid = stagingvid++;
    StagingVol.Type = BACKVOL;
    (&StagingVol.Type0)[replicatedVolume] = vid;
    StagingVol.Server0 = ntohl(srvr->s_addr);

    ro_replica = (volrep *)VDB->Create(&StagingVol, stagingname);
    if (ro_replica) ro_replica->hold();

    /* fake a CB-connection */
    {
	srvent *s;
	GetServer(&s, srvr);
	if (s) s->connid = 666;
	PutServer(&s);
    }
}

int repvol::Collate_NonMutating(mgrpent *m, int code)
{
    code = m->CheckNonMutating(code);

    /* when the operation has failed miserably, but we have a pending volume
     * transition, just retry the operation */
    if (code && flags.transition_pending)
	code = ERETRY;
    return(code);
}


int repvol::Collate_COP1(mgrpent *m, int code, ViceVersionVector *UpdateSet)
{
    code = m->CheckCOP1(code, UpdateSet);

    return(code);
}


int repvol::Collate_Reintegrate(mgrpent *m, int code, ViceVersionVector *UpdateSet)
{
    code = m->CheckReintegrate(code, UpdateSet);

    return(code);
}


int repvol::Collate_COP2(mgrpent *m, int code)
{
    code = m->CheckNonMutating(code);

    /* Nothing useful we can do with an EASYRESOLVE response. */
    if (code == EASYRESOLVE)
	code = 0;

    return(code);
}


ViceFid repvol::GenerateLocalFid(ViceDataType fidtype)
{
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
ViceFid repvol::GenerateFakeFid() 
{
    ViceFid fid;
    FID_MakeSubtreeRoot(&fid, vid, FidUnique);

    RVMLIB_REC_OBJECT(FidUnique);
    FidUnique++;

    return(fid);
}

/* MUST be called from within a transaction */
ViceStoreId repvol::GenerateStoreId()
{
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

/* Helper routine to help convert code for (int)(_volent.type) to code
   for (ViceVolumeType)(_VolumeStatus.Type).  This conversion is needed
   by volent::GetVolStat().  When connected, GetVolStat get the type
   from the servers, the code can be {ReadOnly, ReadWrite} ("Backup"
   and "Replicated" are never returned by server
   (c.f. SetVolumeStatus())).  When disconnected, GetVolStat get the
   type from volent.
   -- Clement
*/
ViceVolumeType volent::VolStatType(void)
{
    if (IsReplicated())
        return Replicated;
       
    if (IsBackup())
        return Backup;

    return ReadWrite;
}

/* local-repair modification */
int volent::GetVolStat(VolumeStatus *volstat, RPC2_BoundedBS *Name,
		       VolumeStateType *conn_state, int *conflict,
                       int*cml_count, RPC2_BoundedBS *msg,
                       RPC2_BoundedBS *motd, vuid_t vuid)
{
    LOG(100, ("volent::GetVolStat: vid = %x, vuid = %d\n", vid, vuid));

    *conn_state = state;
    *conflict = 0;
    *cml_count = 0;
    if (IsReplicated()) {
        repvol *rv = (repvol *)this;
        rv->CheckLocalSubtree();       /* unset has_local_subtree if possible */
        *conflict = rv->HasLocalSubtree();
        *cml_count = rv->GetCML()->count();
    }
    if (state == Hoarding && *conflict && *cml_count > 0) {
	LOG(0, ("volent::GetVolStat: Strange! A connected volume 0x%x has "
		"conflict or cml_count != 0 (=%d)?\n", vid, *cml_count));
    }

    if (IsFake()) {
	/* make up some numbers for the local-fake volume */
	LOG(100, ("volent::GetVolStat: Local Volume vuid = %d\n", vuid));	
	FID_MakeVolFake(&volstat->Vid);
	volstat->ParentId = 0xfffffffe; /* NONSENSE, but what is right? pjb */
	volstat->InService = 1;
    	volstat->Blessed = 1;
	volstat->NeedsSalvage = 1;
	volstat->Type = ReadOnly;
	volstat->MinQuota = 0;
	volstat->MaxQuota = 0;
	volstat->BlocksInUse = 5000;
	volstat->PartBlocksAvail = 5000;
	volstat->PartMaxBlocks = 10000;
	const char fakevolname[]="A_Local_Fake_Volume";
	strcpy((char *)Name->SeqBody, fakevolname);
	Name->SeqLen = strlen((char *)fakevolname) + 1;
	/* Overload offline message to print some more message for users */
	const char fakemsg[]=
	    "This directory is made a fake volume because there is a conflict";
	strcpy((char *)msg->SeqBody, fakemsg);
	msg->SeqLen = strlen((char *)fakemsg) + 1;
	motd->SeqBody[0] = 0;
	motd->SeqLen = 1;
	
	return 0;
    }

    int code = 0;

    if (state == Emulating) {
	memset(volstat, 0, sizeof(VolumeStatus));
	volstat->Vid = vid;
	volstat->Type = VolStatType();	
	/* We do not know are about quota and block usage, but should be ok. */
	strcpy((char *)Name->SeqBody, name);
	Name->SeqLen = strlen((char *)name) + 1;
	msg->SeqBody[0] = 0;
	msg->SeqLen = 1;
	motd->SeqBody[0] = 0;
	motd->SeqLen = 1;
	code = 0;
    }
    else {
	VOL_ASSERT(this, (state == Hoarding || state == Logging));

	if (IsReplicated()) {
	    /* Acquire an Mgroup. */
	    mgrpent *m = 0;
            repvol *vp = (repvol *)this;
	    code = vp->GetMgrp(&m, vuid);
	    if (code != 0) goto RepExit;

	    {
		/* Make multiple copies of the IN/OUT and OUT parameters. */
		int ph_ix; unsigned long ph;
                ph = ntohl(m->GetPrimaryHost(&ph_ix)->s_addr);

		if (Name->MaxSeqLen > VENUS_MAXBSLEN ||
		    msg->MaxSeqLen > VENUS_MAXBSLEN ||
		    motd->MaxSeqLen > VENUS_MAXBSLEN)
		    CHOKE("volent::GetVolStat: BS len too large");
		ARG_MARSHALL(OUT_MODE, VolumeStatus, volstatvar, *volstat, VSG_MEMBERS);
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
		code = vp->Collate_NonMutating(m, code);
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
            volrep *vol = (volrep *)this;
	    code = vol->GetConn(&c, vuid);
	    if (code != 0) goto NonRepExit;

	    /* Make the RPC call. */
	    MarinerLog("store::GetVolStat %s\n", name);
	    UNI_START_MESSAGE(ViceGetVolumeStatus_OP);
	    code = (int) ViceGetVolumeStatus(c->connid, vid,
				       volstat, Name, msg, motd, 0);
	    UNI_END_MESSAGE(ViceGetVolumeStatus_OP);
	    MarinerLog("store::getvolstat done\n");

	    /* Examine the return code to decide what to do next. */
	    code = vol->Collate(c, code);
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
            repvol *vp = (repvol *)this;

	    Recov_BeginTrans();
            ViceStoreId sid = vp->GenerateStoreId();
	    Recov_EndTrans(MAXFP);

  	    code = vp->GetMgrp(&m, vuid, (PIGGYCOP2 ? &PiggyBS : 0));
	    if (code != 0) goto RepExit;

	    {
		/* Make multiple copies of the IN/OUT and OUT parameters. */
		int ph_ix; unsigned long ph;
                ph = ntohl(m->GetPrimaryHost(&ph_ix)->s_addr);

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
		code = vp->Collate_COP1(m, code, &UpdateSet);
		MULTI_RECORD_STATS(ViceSetVolumeStatus_OP);
		if (code != 0) goto RepExit;

		/* Finalize COP2 Piggybacking. */
		if (PIGGYCOP2)
		    vp->ClearCOP2(&PiggyBS);

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
	        vp->AddCOP2(&sid, &UpdateSet);
 	    else
	        vp->COP2(m, &sid, &UpdateSet);

RepExit:
	    PutMgrp(&m);
	}
	else {
	    /* Acquire a Connection. */
	    connent *c;
 	    ViceStoreId Dummy;          /* Need an address for ViceSetVolStat */
            volrep *vol = (volrep *)this;
	    code = vol->GetConn(&c, vuid);
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
	    code = vol->Collate(c, code);
	    UNI_RECORD_STATS(ViceSetVolumeStatus_OP);

NonRepExit:
	    PutConn(&c);
	}
    }

    return(code);
}


void volent::GetMountPath(char *buf, int ok_to_assert)
{
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


void volent::print(int afd)
{
    fdprint(afd, "%#08x : %-16s : vol = %x\n", (long)this, name, vid);

    fdprint(afd, "\trefcnt = %d, fsos = %d, logv = %d, weak = %d\n",
	    refcnt, fso_list->count(), flags.logv, flags.weaklyconnected);
    fdprint(afd, "\tstate = %s, t_p = %d, d_p = %d, counts = [%d %d %d %d], repair = %d\n",
	    PRINT_VOLSTATE(state), flags.transition_pending, flags.demotion_pending,
	    observer_count, mutator_count, waiter_count, resolver_count, 
	    flags.repair_mode);
    fdprint(afd, "\tshrd_count = %d and excl_count = %d excl_pgid = %d\n",
	    shrd_count, excl_count, excl_pgid);

    if (IsReplicated())
        ((repvol *)this)->print_repvol(afd);
    else
        ((volrep *)this)->print_volrep(afd);
}

void volrep::print_volrep(int afd)
{
    fdprint(afd, "\thost: %s, available %d", inet_ntoa(host), flags.available);
    fdprint(afd, "replicated parent volume = %x\n", replicated);
}

void repvol::print_repvol(int afd)
{
    fdprint(afd, "\tage limit = %u (sec), reint limit = %u (msec)\n", 
	    AgeLimit, ReintLimit);
    fdprint(afd, "\tasr allowed %d, running %d\n", 
	    flags.allow_asrinvocation, flags.asr_running);
    fdprint(afd, "\treintegrate pending %d, reintegrating = %d\n", 
	    flags.reintegratepending, flags.reintegrating);
    fdprint(afd, "\thas_local_subtree = %d, resolve_me = %d\n", 
	    flags.has_local_subtree, flags.resolve_me);

    /* Replicas */
    fdprint(afd, "\tvolume replicas: [ ");
    for (int i = 0; i < VSG_MEMBERS; i++)
        fdprint(afd, "%#08x, ", vsg[i] ? vsg[i]->GetVid() : 0);
    fdprint(afd, "]\n");
    fdprint(afd, "\tstaging volume: %#08x\n",ro_replica?ro_replica->GetVid():0);

    fdprint(afd, "\tVersion stamps = [");
    for (int i = 0; i < VSG_MEMBERS; i++)
	fdprint(afd, " %d", (&(VVV.Versions.Site0))[i]);
    fdprint(afd, " ]\n");
    fdprint(afd, "\tCB status = %d, reint_id_gen = %d\n", VCBStatus,
            reint_id_gen);

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

repvol_iterator::repvol_iterator(void *key) : rec_ohashtab_iterator(VDB->repvol_hash, key) { }

volrep_iterator::volrep_iterator(void *key) : rec_ohashtab_iterator(VDB->volrep_hash, key) { }

repvol *repvol_iterator::operator()()
{
    rec_olink *o = rec_ohashtab_iterator::operator()();
    if (!o) return(0);

    repvol *v = strbase(repvol, o, handle);
    return(v);
}

volrep *volrep_iterator::operator()()
{
    rec_olink *o = rec_ohashtab_iterator::operator()();
    if (!o) return(0);

    volrep *v = strbase(volrep, o, handle);
    return(v);
}

