#ifndef _BLURB_
#define _BLURB_
/*

            Coda: an Experimental Distributed File System
                             Release 4.0

          Copyright (c) 1987-1996 Carnegie Mellon University
                         All Rights Reserved

Permission  to  use, copy, modify and distribute this software and its
documentation is hereby granted,  provided  that  both  the  copyright
notice  and  this  permission  notice  appear  in  all  copies  of the
software, derivative works or  modified  versions,  and  any  portions
thereof, and that both notices appear in supporting documentation, and
that credit is given to Carnegie Mellon University  in  all  documents
and publicity pertaining to direct or indirect use of this code or its
derivatives.

CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
ANY DERIVATIVE WORK.

Carnegie  Mellon  encourages  users  of  this  software  to return any
improvements or extensions that  they  make,  and  to  grant  Carnegie
Mellon the rights to redistribute these changes without encumbrance.
*/

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/venus/vol_vcb.cc,v 4.2 1997/02/26 16:03:36 rvb Exp $";
#endif /*_BLURB_*/




/*
 *  Code relating to volume callbacks.
 */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <sys/types.h>
#include <struct.h>
#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif
#include <netinet/in.h>
#ifdef __BSD44__
#include <machine/endian.h>
#endif

#include <rpc2.h>

#ifdef __cplusplus
}
#endif __cplusplus

/* interfaces */
#include <vice.h>
#include <mond.h>

/* from venus */
#include "comm.h"
#include "fso.h"
#include "mariner.h"
#include "venuscb.h"
#include "venusvol.h"
#include "venus.private.h"
#include "vproc.h"
#include "worker.h"


int vcbbreaks = 0;	/* count of broken volume callbacks */
char VCBEnabled = 1;	/* use VCBs by default */


int vdb::CallBackBreak(VolumeId vid) {
    int rc = 0;
    volent *v = VDB->Find(vid);

    if (v && (rc = v->CallBackBreak()))
	vcbbreaks++;    

    return(rc);
}


/*
 * GetVolAttr - Get a volume version stamp (or validate one if
 * present) and get a callback.  If validating, and there are
 * other volumes that need validating, do them too.
 *
 * Only applies to replicated volumes!
 */
int volent::GetVolAttr(vuid_t vuid) {
    LOG(100, ("volent::GetVolAttr: %s, vid = 0x%x\n", name, vid));

    VOL_ASSERT(this, (state == Hoarding || state == Logging));
    VOL_ASSERT(this, IsReplicated());

    int i, code = 0;

    /* Acquire an Mgroup. */
    mgrpent *m = 0;
    code = GetMgrp(&m, vuid);
    if (code != 0) goto RepExit;

    long cbtemp; cbtemp = cbbreaks;
    {
	/* 
	 * if we're fetching (as opposed to validating) volume state, 
	 * we must first ensure all cached file state from this volume 
	 * is valid (i.e., our cached state corresponds to the version 
	 * information we will get).  If the file state can't be 
	 * validated, we bail.
	 */
	if (VV_Cmp(&VVV, &NullVV) == VV_EQ) {
	    InitVCBData(vid);

	    if (code = ValidateFSOs()) 
		goto RepExit;

	    RPC2_Integer VS;
	    ARG_MARSHALL(OUT_MODE, RPC2_Integer, VSvar, VS, VSG_MEMBERS);

	    CallBackStatus CBStatus;
	    ARG_MARSHALL(OUT_MODE, CallBackStatus, CBStatusvar, CBStatus, VSG_MEMBERS);

	    /* Make the RPC call. */
	    MarinerLog("store::GetVolVS %s\n", name);
	    MULTI_START_MESSAGE(ViceGetVolVS_OP);
	    code = (int) MRPC_MakeMulti(ViceGetVolVS_OP, ViceGetVolVS_PTR,
					VSG_MEMBERS, m->rocc.handles,
					m->rocc.retcodes, m->rocc.MIp, 0, 0,
					vid, VSvar_ptrs, CBStatusvar_ptrs);
	    MULTI_END_MESSAGE(ViceGetVolVS_OP);
	    MarinerLog("store::getvolvs done\n");

	    /* Collate responses from individual servers and decide what to do next. */
	    code = Collate_NonMutating(m, code);
	    MULTI_RECORD_STATS(ViceGetVolVS_OP);

	    if (code != 0) goto RepExit;

	    if (cbtemp == cbbreaks) 
		CollateVCB(m, VSvar_bufs, CBStatusvar_bufs);

	    /* if we've acquired a vcb, report statistics */
	    if (VCBStatus == CallBackSet) 
		ReportVCBEvent(Acquire, vid);
        } else {
	    /* 
	     * Figure out how many volumes to validate.
	     * We can do this every call because there are a small number of volumes.
	     * We send the server its version stamp, it its slot and sends back yea or nay.
	     */
	    int nVols = 0;
	    ViceVolumeIdStruct VidList[PIGGY_VALIDATIONS];

	    /* 
	     * To minimize bandwidth, we should not send full version vectors to each server.  
	     * We could send each server its version stamp, but that would be extremely messy for
	     * multicast (which assumes the same message goes to all destinations).  We 
	     * compromise by sending all the version stamps to each server.  If we had true 
	     * multicast, this would be cheaper than sending a set of unicasts each
	     * with a different version stamp. The array is a BS because there isn't a 
	     * one to one correspondence between it and the volume ID list. Besides, 
	     * I hate introducing those damn structures.
	     */
	    RPC2_CountedBS VSBS;
	    VSBS.SeqLen = 0;
	    VSBS.SeqBody = (RPC2_ByteSeq) malloc(PIGGY_VALIDATIONS * m->nhosts * sizeof(RPC2_Integer));

	    /* 
	     * this is a BS instead of an array because the RPC2 array implementation requires 
	     * array elements to be structures. In the case of VFlags, that would be a real 
	     * waste of space (which is going over the wire).
	     */
	    char VFlags[PIGGY_VALIDATIONS];
	    RPC2_CountedBS VFlagBS;
	    VFlagBS.SeqLen = 0;
	    VFlagBS.SeqBody = (RPC2_ByteSeq) VFlags;

	    /* 
	     * validate volumes that:
	     * - are replicated
	     * - are in the same vsg
	     * - are in the hoarding state
	     * - want a volume callback (includes check for presence of one)
	     * - have a non-null version vector for comparison
	     *
	     * Note that we may not pick up a volume for validation after a partition if the 
	     * volume has not yet been demoted (i.e. the demotion_pending flag is set).  If 
	     * the volume is awaiting demotion, it may appear to still have a callback when 
	     * viewed "externally" as we do here. This does not violate correctness, because 
	     * if an object is referenced in the volume the demotion will be taken first.  
	     *
	     * We do not bother checking the stamps for volumes not in the hoarding state;
	     * when the transition is taken to the hoarding state the volume will be demoted
	     * and the callback cleared anyway.
	     */
	    vol_iterator next;
	    volent *v;

	    /* one of the following should be this volume. */
	    while ((v = next()) && (nVols < PIGGY_VALIDATIONS)) 
		if (v->IsReplicated() && 
		    (v->vsg->Addr == vsg->Addr) &&
		    ((v->state == Hoarding) || (v->state == Logging)) &&    
		     v->WantCallBack() && 
		    (VV_Cmp(&v->VVV, &NullVV) != VV_EQ)) {

		    LOG(1000, ("volent::GetVolAttr: packing volume %s, vid 0x%x, vvv:\n",
			v->name, v->vid));
		    if (LogLevel >= 1000) PrintVV(logFile, &v->VVV);

		    VidList[nVols].Vid = v->vid;
		    for (i = 0; i < m->nhosts; i++) {
			*((RPC2_Unsigned *)&((char *)VSBS.SeqBody)[VSBS.SeqLen]) =
				htonl((&v->VVV.Versions.Site0)[i]);
			VSBS.SeqLen += sizeof(RPC2_Unsigned);
		    }
		    nVols++;
		}

	    /* 
	     * nVols could be 0 here if someone else got into this routine and
	     * validated while we were descheduled...such as in getmgrp.
	     */
	    if (nVols == 0) {
		free(VSBS.SeqBody);
	        goto RepExit;
            }

	    LOG(100, ("volent::GetVolAttr: %s, sending %d version stamps\n", name, nVols));

	    ARG_MARSHALL_BS(IN_OUT_MODE, RPC2_CountedBS, VFlagvar, VFlagBS, VSG_MEMBERS, VENUS_MAXBSLEN);

	    /* Make the RPC call. */
	    MarinerLog("store::ValidateVols %s [%d]\n", name, nVols);
	    MULTI_START_MESSAGE(ViceValidateVols_OP);
	    code = (int) MRPC_MakeMulti(ViceValidateVols_OP, ViceValidateVols_PTR,
					VSG_MEMBERS, m->rocc.handles,
					m->rocc.retcodes, m->rocc.MIp, 0, 0,
					nVols, VidList, &VSBS, VFlagvar_ptrs);
	    MULTI_END_MESSAGE(ViceValidateVols_OP);
	    MarinerLog("store::validatevols done\n");

	    /* Collate responses from individual servers and decide what to do next. */
	    code = Collate_NonMutating(m, code);
	    MULTI_RECORD_STATS(ViceValidateVols_OP);
	    free(VSBS.SeqBody);

	    if (code) goto RepExit;

	    unsigned numVFlags = 0;
	    for (i = 0; i < m->nhosts; i++)
		if (m->rocc.hosts[i])
		    if (numVFlags == 0) {
			/* unset, copy in one response */
			ARG_UNMARSHALL_BS(VFlagvar, VFlagBS, i);
			numVFlags = (unsigned) VFlagBS.SeqLen;
		    } else {
			/* "and" in results from other servers. note VFlagBS.SeqBody == VFlags. */
			for (int j = 0; j < nVols; j++) {
			    if ((VFlags[j] == -1) || ((char) VFlagvar_bufs[i].SeqBody[j] == -1))
				VFlags[j] = -1;
			    else 
				VFlags[j] &= VFlagvar_bufs[i].SeqBody[j];
			}
		    }


	    LOG(10, ("volent::GetVolAttr: ValidateVols (%s), %d vids sent, %d checked\n",
		      name, nVols, numVFlags));

	    /* now set status of volumes */
	    for (i = 0; i < numVFlags; i++)  /* look up the object */
		if (v = VDB->Find(VidList[i].Vid)) {
		    fso_vol_iterator next(NL, v);
		    fsobj *f;
		    vcbevent ve(v->fso_list->count());

		    switch (VFlags[i]) {
		    case 1:  /* OK, callback */
			if (cbtemp == cbbreaks) {
			    LOG(1000, ("volent::GetVolAttr: vid 0x%x valid\n", v->vid));
	                    v->SetCallBack();

			    /* validate cached access rights for the caller */
			    while (f = next()) 
				if (f->IsDir()) {
				    f->PromoteAcRights(ALL_UIDS);
				    f->PromoteAcRights(vuid);
			        }
			    
			    ReportVCBEvent(Validate, v->vid, &ve);
		        } 
			break;
		    case 0:  /* OK, no callback */
			LOG(0, ("volent::GetVolAttr: vid 0x%x valid, no callback\n", v->vid));
			v->ClearCallBack();
			break;
		    default:  /* not OK */
			LOG(1, ("volent::GetVolAttr: vid 0x%x invalid\n", v->vid));
			v->ClearCallBack();
			ATOMIC(
			    RVMLIB_REC_OBJECT(v->VVV);
			    v->VVV = NullVV;   
			, MAXFP)

			ReportVCBEvent(FailedValidate, v->vid, &ve);

			break;
		    }
		} else {
		    LOG(0, ("volent::GetVolAttr: couldn't find vid 0x%x\n", 
			    VidList[i].Vid));
		}
        }
    }

RepExit:
    PutMgrp(&m);
    DeleteVCBData();
    
    return(code);
}


/* collate version stamp and callback status out parameters from servers */
void volent::CollateVCB(mgrpent *m, RPC2_Integer *sbufs, CallBackStatus *cbufs) {
    int i;
    CallBackStatus collatedCB = CallBackSet;

    if (LogLevel >= 100) {
	fprintf(logFile, "volent::CollateVCB: vid 0x%x Current VVV:\n", vid);
    	PrintVV(logFile, &VVV);

	fprintf(logFile, "volent::CollateVCB: Version stamps returned:");
	for (i = 0; i < m->nhosts; i++)
	    if (m->rocc.hosts[i]) 
		fprintf(logFile, " %lu", sbufs[i]);

	fprintf(logFile, "\nvolent::CollateVCB: Callback status returned:");
	for (i = 0; i < m->nhosts; i++) 
	    if (m->rocc.hosts[i])
	    	fprintf(logFile, " %lu", cbufs[i]);

	fprintf(logFile, "\n");
	fflush(logFile);
    }

    for (i = 0; i < m->nhosts; i++) {
	if (m->rocc.hosts[i] && (cbufs[i] != CallBackSet))
	    collatedCB = NoCallBack;
    }

    if (collatedCB == CallBackSet) {
	SetCallBack();
	ATOMIC(
	    RVMLIB_REC_OBJECT(VVV);
	    for (i = 0; i < m->nhosts; i++)
	        if (m->rocc.hosts[i])
		   (&VVV.Versions.Site0)[i] = sbufs[i];
	, MAXFP)
    } else {
	ClearCallBack();

	/* check if any of the returned stamps are zero.
	   If so, server said stamp invalid. */
        for (i = 0; i < m->nhosts; i++)
	    if (m->rocc.hosts[i] && (sbufs[i] == 0)) {
		ATOMIC(
		   RVMLIB_REC_OBJECT(VVV);
		   VVV = NullVV;
		, MAXFP)
		break;
	    }
    }

    return;
}


/*
 * Ensure all cached state from volume "vol" is valid.
 * Returns success if it is able to do this, an errno otherwise.
 *
 * Error handling is simple: if one occurs, quit and propagate.
 * There's no volume synchronization because we've already
 * done it. 
 * 
 * complications: 
 * - this can't be called from fsdb::Get (a reasonable place)
 *   unless the target fid is known, because this routine calls
 *   fsdb::Get on potentially everything.
 */
int volent::ValidateFSOs() {
    fsobj *f;
    int code = 0;

    LOG(100, ("volent::ValidateFSOs: vid = 0x%x\n", vid));

    vproc *vp = VprocSelf();
    fso_vol_iterator next(NL, this);

    while (f = next()) {
	if ((HAVESTATUS(f) && STATUSVALID(f)) && (DATAVALID(f) || !HAVEDATA(f)))
	    continue;

	int whatToGet = 0;
	if (!HAVESTATUS(f) || !STATUSVALID(f)) 
	    whatToGet = RC_STATUS;
	if (HAVEDATA(f) && !DATAVALID(f)) 
	    whatToGet |= RC_DATA;

	LOG(100, ("volent::ValidateFSOs: vget(%x.%x.%x, %x, %d)\n",
		f->fid.Volume, f->fid.Vnode, f->fid.Unique,
		whatToGet, f->stat.Length));

	fsobj *tf = 0;
	code = FSDB->Get(&tf, &f->fid, CRTORUID(vp->u.u_cred), whatToGet);
	FSDB->Put(&tf);

	LOG(100, ("volent::ValidateFSOs: vget returns %s\n", VenusRetStr(code)));

	if (code == EINCONS)
	    k_Purge(&f->fid, 1);
	if (code) 
	    break;
    }
    return(code);
}


void volent::PackVS(int nstamps, RPC2_CountedBS *BS) {
    BS->SeqLen = 0;
    BS->SeqBody = (RPC2_ByteSeq) malloc(nstamps * sizeof(RPC2_Integer));

    for (int i = 0; i < nstamps; i++) {
	*((RPC2_Unsigned *)&((char *)BS->SeqBody)[BS->SeqLen]) =
		(&VVV.Versions.Site0)[i];
	BS->SeqLen += sizeof(RPC2_Unsigned);
    }
    return;
}


int volent::CallBackBreak() {
    /*
     * Track vcb's broken for this volume. Total vcb's broken is 
     * accumulated in vdb::CallbackBreak.
     */

    int rc = (VCBStatus == CallBackSet);

    if (rc) {
	VCBStatus = NoCallBack;
    
	ATOMIC(
	    RVMLIB_REC_OBJECT(VVV);
	    VVV = NullVV;   
	, MAXFP)
    }

    return(rc);    
}


void volent::ClearCallBack()
{
    /*
     * Count vcb's cleared on this volume because of connectivity
     * changes. 
     */
    if (VCBStatus == CallBackSet) {
	vcbevent ve(fso_list->count());
	ReportVCBEvent(Clear, vid, &ve);
    }

    VCBStatus = NoCallBack;
}


void volent::SetCallBack()
{
    VCBStatus = CallBackSet;
}


int volent::WantCallBack()
{
    /* 
     * This is a policy module that decides if a volume 
     * callback is worth acquiring.  This is a naive policy,
     * with a minimal threshold for files.  One could use
     * CallBackClears as an approximation to the partition
     * rate (p), and CallbackBreaks as an approximation
     * to the mutation rate (m). 
     */
    int getit = 0;

    if ((VCBStatus == NoCallBack) &&
	(fso_list->count() > 1))
	getit = 1;

    return(getit);
}

/* *****  VCB data collection ***** */

/* 
 * to collect data on volume callback usage, we maintain
 * a vdb/volent-like arrangement in RVM.  Eventually, 
 * this should go into mond.  This data must be maintained
 * separately from the vdb/volent because volents can
 * be deleted.
 */

/* Allocate database from recoverable store. */
void *vcbdb::operator new(size_t len) {
    vcbdb *v = 0;

    /* Allocate recoverable store for the object. */
    v = (vcbdb *)RVMLIB_REC_MALLOC((int) len);
    assert(v);
    return(v);
}

vcbdb::vcbdb() : htab(VCBDB_NBUCKETS, VOL_HashFN) {

    RVMLIB_REC_OBJECT(*this);
    MagicNumber = VCBDB_MagicNumber;
}


void vcbdb::ResetTransient() {
    /* Sanity checks. */
    if (MagicNumber != VCBDB_MagicNumber)
	Choke("vcbdb::ResetTransient: bad magic number (%d)", MagicNumber);

    htab.SetHFn(VOL_HashFN);
}


/* MUST NOT be called from within transaction! */
vcbdent *vcbdb::Create(VolumeId vid, char *volname) {
    vcbdent *v = 0;

    /* Check whether the key is already in the database. */
    if ((v = Find(vid)) != 0) {
	{ v->print(logFile); Choke("vcbdb::Create: key exists"); }
    }

    /* Fashion a new object. */
    ATOMIC(
	v = new vcbdent(vid, volname);
    , MAXFP)

    if (v == 0)
	LOG(0, ("vcbdb::Create: (%x, %s) failed\n", vid, volname));
    return(v);
}


vcbdent *vcbdb::Find(VolumeId volnum) {
    vcbd_iterator next(&volnum);
    vcbdent *v;
    while (v = next())
	if (v->vid == volnum) return(v);

    return(0);
}


void vcbdb::print(int fd) {
    vcbd_iterator next;
    vcbdent *v;

    fdprint(fd, "\n***** VCB Statistics *****\n");
    while (v = next())
	v->print(fd);
}


/* MUST be called from within transaction! */
void *vcbdent::operator new(size_t len){
    vcbdent *v = 0;

    v = (vcbdent *)RVMLIB_REC_MALLOC((int) len);
    assert(v);
    return(v);
}


vcbdent::vcbdent(VolumeId Vid, char *volname) {

    LOG(10, ("vcbdent::vcbdent: (%x, %s)\n", vid, volname));

    RVMLIB_REC_OBJECT(*this);
    MagicNumber = VCBDENT_MagicNumber;
    strcpy(name, volname);
    vid = Vid;
    bzero((void *)&data, (int)sizeof(VCBStatistics));

    VCBDB->htab.insert(&vid, &handle);
}
	


void vcbdent::print(int fd) {
    fdprint(fd, "%#08x : %-16s : vol = %x\n\tEvent\t\t    num   objs   check    fail   no-ofail\n",
	    (long) this, name, vid);
    
    fdprint(fd, "\tAcquire\t\t%6d\t%6d\t%6d\t%6d\t%6d\n",
	    data.Acquires, data.AcquireObjs, data.AcquireChecked, 
	    data.AcquireFailed, data.AcquireNoObjFails);
    fdprint(fd,"\tValidate\t%6d\t%6d\n\tFailedValidate\t%6d\t%6d\n",
	    data.Validates, data.ValidateObjs, data.FailedValidates, 
	    data.FailedValidateObjs);
    fdprint(fd,"\tBreaks\t\t%6d\t%6d\t%6d volonly\t%6d refs\n",
	    data.Breaks, data.BreakObjs, data.BreakVolOnly, data.BreakRefs);
    fdprint(fd,"\tClears\t\t%6d\t%6d\t%6d refs\n\tNoStamp\t\t%6d\t%6d\n",
	    data.Clears, data.ClearObjs, data.ClearRefs, data.NoStamp, 
	    data.NoStampObjs);
}


vcbd_iterator::vcbd_iterator(void *key) : rec_ohashtab_iterator(VCBDB->htab, key) {
}


vcbdent *vcbd_iterator::operator()() {
    rec_olink *o = rec_ohashtab_iterator::operator()();
    if (!o) return(0);

    vcbdent *v = strbase(vcbdent, o, handle);
    return(v);
}



/* manipulation of vcbevents */

/* tag a vproc with a vcb data block for volume vid. */
void InitVCBData(VolumeId vid) {
    vproc *vp = VprocSelf();

    volent *vol = VDB->Find(vid);
    if (!vol) Choke("InitVCBData: Can't find volume 0x%x!", vid);

    vp->ve = new vcbevent(vol->fso_list->count());
}    


/* add data fields in vcb data block. */
void AddVCBData(unsigned nc, unsigned nf) {
    vproc *vp = VprocSelf();
    if (vp->ve) {
	vp->ve->nchecked += nc;
	vp->ve->nfailed += nf;
    }
}


/* remove vcb data block from vproc */
void DeleteVCBData() {
    vproc *vp = VprocSelf();
    if (vp->ve) {
	delete vp->ve;
	vp->ve = NULL;
    }
}


/* 
 * Accumulates the VCB event into the RVM database.
 * For now, also prints into the log.
 * The event data may be sent in explicitly, or if
 * not, taken from the stash on the vproc.
 */
void ReportVCBEvent(VCBEventType event, VolumeId vid, vcbevent *ve) {
    /* if ve == NULL, look for data on the vproc. */
    if (ve == NULL) {
	vproc *vp = VprocSelf();
	if (vp->ve) ve = vp->ve;
	else return;	/* nothing to report! */
    }

    volent *vol = VDB->Find(vid);

    /* find (or create) entry for this volume */
    vcbdent *v = VCBDB->Find(vid);
    if (!v) {
	if (!vol) Choke("ReportVCBEvent: Can't find volume 0x%x!", vid);
    
	v = VCBDB->Create(vol->vid, vol->name);
    }

    ATOMIC(
	RVMLIB_REC_OBJECT(v->data);
	switch(event) {
	case Acquire:
	    v->data.Acquires++;
	    v->data.AcquireObjs += ve->nobjs;
	    v->data.AcquireChecked += ve->nchecked;
	    v->data.AcquireFailed += ve->nfailed;
	    /* need the following for false sharing calculations */
	    if (ve->nfailed == 0)
		v->data.AcquireNoObjFails++;

	    break;

	case Validate:
	    v->data.Validates++;
	    v->data.ValidateObjs += ve->nobjs;
	    break;

	case FailedValidate:
	    v->data.FailedValidates++;
	    v->data.FailedValidateObjs += ve->nobjs;
	    break;

	case Break:
	    v->data.Breaks++;
	    v->data.BreakObjs += ve->nobjs;
	    v->data.BreakVolOnly += ve->volonly;
	    ASSERT(vol);
	    v->data.BreakRefs += vol->VCBHits;
	    vol->VCBHits = 0;
	    break;

	case Clear:
	    v->data.Clears++;
	    v->data.ClearObjs += ve->nobjs;
	    ASSERT(vol);
	    v->data.ClearRefs += vol->VCBHits;
	    vol->VCBHits = 0;
	    break;

	case NoStamp:
	    v->data.NoStamp++;
	    v->data.NoStampObjs += ve->nobjs;
	    break;

	default:
	    Choke("ReportVCBEvent: Unknown event %d!", event);	
	    break;
	}
    , MAXFP)

}
