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
 *    CFS calls0.
 *
 *    ToDo:
 *       1. All mutating Vice calls should have the following IN arguments:
 *            NewSid, NewMutator (implicit from connection), NewMtime, 
 *            OldVV and DataVersion (for each object), NewStatus (for each object)
 */


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <sys/types.h>
#include <sys/file.h>
#include <unistd.h>
#include <stdlib.h>

#include <rpc2.h>
#include <se.h>
/* interfaces */
#include <vice.h>

#ifdef __cplusplus
}
#endif __cplusplus


/* from venus */
#include "comm.h"
#include "fso.h"
#include "mariner.h"
#include "venuscb.h"
#include "vproc.h"
#include "venus.private.h"
#include "venusrecov.h"
#include "venusvol.h"
#include "worker.h" 


/*  *****  Fetch  *****  */

/* C-stub to jump into the c++ method without compiler warnings */
static void FetchProgressIndicator_stub(void *up, unsigned int offset)
{
    ((fsobj *)up)->FetchProgressIndicator((long)offset);
}

void fsobj::FetchProgressIndicator(long offset)
{
    unsigned long last;
    unsigned long curr;
    
    if (stat.Length == 0) return;

    if      (offset == stat.Length) { last = 0; curr = 100; }
    else if (offset == 0)           { last = 100; curr = 0; }
    else {
	last = (GotThisData * 100) / stat.Length;
	curr = ((unsigned long)offset * 100) / stat.Length;
    }

    if (last != curr) {
	MarinerLog("progress::fetching (%s) %lux\n", comp, curr);
    }

    GotThisData = (unsigned long)offset;
}

int fsobj::Fetch(vuid_t vuid) {
    LOG(10, ("fsobj::Fetch: (%s), uid = %d\n", comp, vuid));

    if (IsLocalObj()) {
	LOG(10, ("fsobj::Fetach: (%s), uid = %d, local object\n", comp, vuid));
	/* set the valid RC status */
	if (HAVEALLDATA(this)) {
	    SetRcRights(RC_DATA | RC_STATUS);
	    return 0;
	} else {
	    ClearRcRights();
	    return ETIMEDOUT;
	}
    }

    /* Sanity checks. */
    {
	/* Better not be disconnected or dirty! */
	FSO_ASSERT(this, (HOARDING(this) || (LOGGING(this) && !DIRTY(this))));

	/* We never fetch data if we don't already have status. */
	if (!HAVESTATUS(this))
	    { print(logFile); CHOKE("fsobj::Fetch: !HAVESTATUS"); }

	/* We never fetch data if we already have some. */
	if (HAVEALLDATA(this))
	    { print(logFile); CHOKE("fsobj::Fetch: HAVEALLDATA"); }

	/* We never fetch data for fake objects. */
	if (IsFake())
	    { print(logFile); CHOKE("fsobj::Fetch: IsFake"); }
    }

    int code = 0;
    char prel_str[256];
    sprintf(prel_str, "fetch::Fetch %%s [%ld]\n", BLOCKS(this));
    int inconok = flags.rwreplica;

    /* Status parameters. */
    ViceStatus status;
    bzero((void *)&status, (int)sizeof(ViceStatus));

    /* COP2 Piggybacking. */
    char PiggyData[COP2SIZE];
    RPC2_CountedBS PiggyBS;
    PiggyBS.SeqLen = 0;
    PiggyBS.SeqBody = (RPC2_ByteSeq)PiggyData;

    /* Set up the SE descriptor. */
    SE_Descriptor dummysed;
    memset(&dummysed, 0, sizeof(SE_Descriptor));
    SE_Descriptor *sed = 0;

    long offset = IsFile() ? cf.ValidData() : 0;
    GotThisData = 0;

    /* C++ 3.0 whines if the following decls moved closer to use  -- Satya */
    {
	    Recov_BeginTrans();
	    RVMLIB_REC_OBJECT(flags);
	    flags.fetching = 1;

	    sed = &dummysed;
	    sed->Tag = SMARTFTP;

	    sed->XferCB = FetchProgressIndicator_stub;
	    sed->userp = this;

	    struct SFTP_Descriptor *sei = &sed->Value.SmartFTPD;
	    sei->TransmissionDirection = SERVERTOCLIENT;
	    sei->hashmark = 0;
	    sei->SeekOffset = offset;
	    sei->ByteQuota = -1;
	    switch(stat.VnodeType) {
		case File:
		    RVMLIB_REC_OBJECT(data.file);
		    data.file = &cf;
		    /* create a sparse file of the desired size */
		    data.file->Truncate(stat.Length);

		    /* but remember how much we actually have */
		    data.file->SetValidData(offset);

		    sei->Tag = FILEBYNAME;
		    sei->FileInfo.ByName.ProtectionBits = V_MODE;
		    strcpy(sei->FileInfo.ByName.LocalFileName, data.file->Name());

		    break;

		    /* I don't know how to lock the DH here, but it should
		       be done. */
		case Directory:
		    CODA_ASSERT(!data.dir);

		    RVMLIB_REC_OBJECT(data.dir);
		    data.dir = (VenusDirData *)rvmlib_rec_malloc(sizeof(VenusDirData));
		    CODA_ASSERT(data.dir);
		    bzero((void *)data.dir, sizeof(VenusDirData));
		    FSO_ASSERT(this, (stat.Length & (DIR_PAGESIZE - 1)) == 0);
		    RVMLIB_REC_OBJECT(*data.dir);
		    DH_Alloc(&data.dir->dh, stat.Length, DIR_DATA_IN_RVM);
		    sei->Tag = FILEINVM;
		    sei->FileInfo.ByAddr.vmfile.MaxSeqLen = stat.Length;
		    sei->FileInfo.ByAddr.vmfile.SeqBody = 
			    (RPC2_ByteSeq)(DH_Data(&data.dir->dh));
		    break;

		case SymbolicLink:
		    CODA_ASSERT(!data.symlink);

		    RVMLIB_REC_OBJECT(data.symlink);
		    /* Malloc one extra byte in case length is 0
		     * (as for runts)! */
		    data.symlink = (char *)rvmlib_rec_malloc((unsigned) stat.Length + 1);
		    sei->Tag = FILEINVM;
		    sei->FileInfo.ByAddr.vmfile.MaxSeqLen = stat.Length;
		    sei->FileInfo.ByAddr.vmfile.SeqBody = (RPC2_ByteSeq)data.symlink;
		    break;

		case Invalid:
		    FSO_ASSERT(this, 0);
	    }
	Recov_EndTrans(CMFP);
    }

    long cbtemp = cbbreaks;

    if (flags.replicated) {
	mgrpent *m = 0;
	int asy_resolve = 0;

	/* Acquire an Mgroup. */
	code = vol->GetMgrp(&m, vuid, (PIGGYCOP2 ? &PiggyBS : 0));
	if (code != 0) goto RepExit;

	/* The COP:Fetch call. */
	{
	    /* Make multiple copies of the IN/OUT and OUT parameters. */
	    int ph_ix;
	    unsigned long ph; ph = m->GetPrimaryHost(&ph_ix);
	    ARG_MARSHALL(OUT_MODE, ViceStatus, statusvar, status, VSG_MEMBERS);
	    ARG_MARSHALL(IN_OUT_MODE, SE_Descriptor, sedvar, *sed, VSG_MEMBERS);
	    {
		/* Omit Side-Effect for all hosts EXCEPT the primary. */
		for (int j = 0; j < VSG_MEMBERS; j++)
		    if (j != ph_ix) sedvar_bufs[j].Tag = OMITSE;
	    }

	    /* Make the RPC call. */
	    CFSOP_PRELUDE(prel_str, comp, fid);
	    MULTI_START_MESSAGE(ViceFetch_OP);
	    code = (int) MRPC_MakeMulti(ViceFetch_OP, ViceFetch_PTR,
				  VSG_MEMBERS, m->rocc.handles,
				  m->rocc.retcodes, m->rocc.MIp, 0, 0,
				  &fid, &stat.VV, inconok, statusvar_ptrs, ph,
				  offset, &PiggyBS, sedvar_bufs);
	    MULTI_END_MESSAGE(ViceFetch_OP);

	    CFSOP_POSTLUDE("fetch::fetch done\n");

	    /* Collate responses from individual servers and decide what to do
	     * next. */
	    code = vol->Collate_NonMutating(m, code);
	    MULTI_RECORD_STATS(ViceFetch_OP);
	    if (code == EASYRESOLVE) { asy_resolve = 1; code = 0; }

	    if (IsFile()) {
		Recov_BeginTrans();
		cf.SetValidData(GotThisData);
		Recov_EndTrans(CMFP);
	    }

	    if (code != 0) goto RepExit;

	    /* Finalize COP2 Piggybacking. */
	    if (PIGGYCOP2)
		vol->ClearCOP2(&PiggyBS);

	    /* Collect the OUT VVs in an array so that they can be checked. */
	    vv_t *vv_ptrs[VSG_MEMBERS];
	    for (int j = 0; j < VSG_MEMBERS; j++)
		vv_ptrs[j] = &((statusvar_ptrs[j])->VV);

	    /* Check the version vectors for consistency. */
	    code = m->RVVCheck(vv_ptrs, (int) ISDIR(fid));
	    if (code == EASYRESOLVE) { asy_resolve = 1; code = 0; }
	    if (code != 0) goto RepExit;

	    /* Compute the dominant host set.  The index of a dominant host is
	     * returned as a side-effect. */
	    int dh_ix; dh_ix = -1;
	    code = m->DHCheck(vv_ptrs, ph_ix, &dh_ix, 1);

	    if (code != 0) goto RepExit;

	    /* Manually compute the OUT parameters from the mgrpent::Fetch() call! -JJK */
	    /* we get the status from the dominant host, and validate it
	     * against the amount of data transferred from the primary host */
	    ARG_UNMARSHALL(statusvar, status, dh_ix);
	    {
		long bytes = sedvar_bufs[ph_ix].Value.SmartFTPD.BytesTransferred;
		LOG(10, ("(Multi)ViceFetch: fetched %d bytes\n", bytes));
		if (bytes != (status.Length - offset)) {
		    // print(logFile);
		    LOG(0, ("fsobj::Fetch: bytes mismatch (%d, %d)",
			    bytes, (status.Length - offset)));
		    code = ERETRY;
		}
	    }

	    /* Handle failed validations. */
	    if (VV_Cmp(&status.VV, &stat.VV) != VV_EQ) {
		if (LogLevel >= 1) {
		    dprint("fsobj::Fetch: failed validation\n");
		    int *r = ((int *)&status.VV);
		    dprint("\tremote = [%x %x %x %x %x %x %x %x] [%x %x] [%x]\n",
			   r[0], r[1], r[2], r[3], r[4],
			   r[5], r[6], r[7], r[8], r[9], r[10]);
		    int *l = ((int *)&stat.VV);
		    dprint("\tlocal = [%x %x %x %x %x %x %x %x] [%x %x] [%x]\n",
			   l[0], l[1], l[2], l[3], l[4],
			   l[5], l[6], l[7], l[8], l[9], l[10]);
		}
		code = ERETRY;
	    }
	}

	/* Directories might have different sizes on different servers. We
	 * _have_ to discard the data, and start over again if we fetched a
	 * different size than expected the wrong one. */
	if (!IsFile() && stat.Length != status.Length)
	    code = ERETRY;

	Recov_BeginTrans();
	UpdateStatus(&status, vuid);
	Recov_EndTrans(CMFP);

RepExit:
	PutMgrp(&m);
	switch(code) {
	    case 0:
		if (asy_resolve)
		    vol->ResSubmit(0, &fid);
		break;

	    case ETIMEDOUT:
	    case ESYNRESOLVE:
	    case EINCONS:
		code = ERETRY;
		break;

	    default:
		break;
	}
    }
    else {
	/* Acquire a Connection. */
	connent *c;
	code = vol->GetConn(&c, vuid);
	if (code != 0) goto NonRepExit;

	/* Make the RPC call. */
	CFSOP_PRELUDE(prel_str, comp, fid);
	UNI_START_MESSAGE(ViceFetch_OP);
	code = (int) ViceFetch(c->connid, &fid, &stat.VV, inconok,
				  &status, 0, offset, &PiggyBS, sed);
	UNI_END_MESSAGE(ViceFetch_OP);
	CFSOP_POSTLUDE("fetch::fetch done\n");

	/* Examine the return code to decide what to do next. */
	code = vol->Collate(c, code);
	UNI_RECORD_STATS(ViceFetch_OP);
	if (IsFile()) {
	    Recov_BeginTrans();
	    cf.SetValidData(GotThisData);
	    Recov_EndTrans(CMFP);
	}

	if (code != 0) goto NonRepExit;

	{
	    long bytes = sed->Value.SmartFTPD.BytesTransferred;
	    LOG(10, ("ViceFetch: fetched %d bytes\n", bytes));
	    if (bytes != ((long)status.Length - offset)) {
		//print(logFile);
		LOG(0, ("fsobj::Fetch: bytes mismatch (%d, %d)",
		        bytes, (status.Length - offset)));
		code = ERETRY;
	    }
	}

	/* Handle failed validations. */
	if (HAVESTATUS(this) && status.DataVersion != stat.DataVersion) {
	    LOG(1, ("fsobj::Fetch: failed validation (%d, %d)\n",
		    status.DataVersion, stat.DataVersion));
	    code = ERETRY;
	}

	Recov_BeginTrans();
	UpdateStatus(&status, vuid);
	Recov_EndTrans(CMFP);

NonRepExit:
	PutConn(&c);
    }

    if (code == 0) {
	/* Read/Write Sharing Stat Collection */
	if (flags.discread) {	
	    Recov_BeginTrans();
	    RVMLIB_REC_OBJECT(flags);
	    flags.discread = 0;
	    Recov_EndTrans(MAXFP);
	}

	if (flags.usecallback &&
	    status.CallBack == CallBackSet &&
	    cbtemp == cbbreaks)
	    SetRcRights(RC_STATUS | RC_DATA);

	/* Note the presence of data. */
	Recov_BeginTrans();
	RVMLIB_REC_OBJECT(flags);
	flags.fetching = 0;

	switch(stat.VnodeType) {
	case File:
		/* File is already `truncated' to the correct length */
		//data.file->SetLength((unsigned) stat.Length);
		break;
		
	case Directory:
		rvmlib_set_range(DH_Data(&data.dir->dh), stat.Length);
		break;
		
	case SymbolicLink:
		rvmlib_set_range(data.symlink, stat.Length);
		break;
		
	case Invalid:
		FSO_ASSERT(this, 0);
	}
	Recov_EndTrans(CMFP);
    }
    else {
       /* 
	* Return allocation and truncate. If a file, set the cache
	* file length so that DiscardData releases the correct
	* number of blocks (i.e., the number allocated in fsdb::Get).
	*/
	Recov_BeginTrans();
	RVMLIB_REC_OBJECT(flags);
	flags.fetching = 0;

	/* when the server responds with EAGAIN, the VersionVector was
	 * changed, so this should effectively be handled like a failed
	 * validation, and we can throw away the data */
	if (HAVEDATA(this) && (!IsFile() || code == EAGAIN))
	    DiscardData();

	/* ERETRY makes us drop back to the vproc_vfscalls level, and retry
	 * the whole FSDB->Get operation */
	if (code == EAGAIN) code = ERETRY;
	
	/* Demote existing status. */
	Demote();
	Recov_EndTrans(CMFP);
    }
    return(code);
}


/*  *****  GetAttr/GetAcl  *****  */

int fsobj::GetAttr(vuid_t vuid, RPC2_BoundedBS *acl) {
    LOG(10, ("fsobj::GetAttr: (%s), uid = %d\n", comp, vuid));

    if (IsLocalObj()) {
	LOG(0, ("fsobj::GetAttr: (%s), uid = %d, local object\n", comp, vuid));
	/* set the valid RC status */
	if (HAVEALLDATA(this)) {
	    SetRcRights(RC_DATA | RC_STATUS);
	    return 0;
	} else {
	    ClearRcRights();
	    return ETIMEDOUT;
	}
    }

    /* Sanity checks. */
    {
	/* Better not be disconnected or dirty! */
	FSO_ASSERT(this, (HOARDING(this) || (LOGGING(this) && !DIRTY(this))));

	if (IsFake()) {
	    FSO_ASSERT(this, acl == 0);

	    /* We never fetch fake directory without having status and data. */
	    if (IsFakeDir() && !HAVEALLDATA(this))
		{ print(logFile); CHOKE("fsobj::GetAttr: IsFakeDir && !HAVEALLDATA"); }

	    /* We never fetch fake mtpts (covered or uncovered). */
	    if (IsFakeMtPt() || IsFakeMTLink())
		{ print(logFile); CHOKE("fsobj::GetAttr: IsFakeMtPt || IsFakeMTLink"); }
	}
    }

    int code = 0;
    int getacl = (acl != 0);
    int inconok = flags.rwreplica;
    const char *prel_str=getacl?"fetch::GetACL %s\n"  :"fetch::GetAttr %s\n";
    const char *post_str=getacl?"fetch::GetACL done\n":"fetch::GetAttr done\n";

    /* Dummy argument for ACL */
    RPC2_BoundedBS dummybs;
    dummybs.MaxSeqLen = 0;
    dummybs.SeqLen = 0;
    if (!getacl) acl = &dummybs;

    /* Status parameters. */
    ViceStatus status;

    /* COP2 Piggybacking. */
    char PiggyData[COP2SIZE];
    RPC2_CountedBS PiggyBS;
    PiggyBS.SeqLen = 0;
    PiggyBS.SeqBody = (RPC2_ByteSeq)PiggyData;

    if (flags.replicated) {
	mgrpent *m = 0;
	int asy_resolve = 0;

	/*
	 * these fields are for tracking vcb acquisition.  Since we
	 * use vcbs on replicated volumes only, the data collection
	 * goes in this branch of GetAttr.
	 */
	int nchecked = 0, nfailed = 0;

	/* Acquire an Mgroup. */
	code = vol->GetMgrp(&m, vuid, (PIGGYCOP2 ? &PiggyBS : 0));
	if (code != 0) goto RepExit;

	nchecked++;  /* we're going to check at least the primary fid */
	long cbtemp; cbtemp = cbbreaks;
	int i;
        {
	    /* Make multiple copies of the IN/OUT and OUT parameters. */
	    int ph_ix; unsigned long ph; ph = m->GetPrimaryHost(&ph_ix);

	    /* unneccesary in validation case but it beats duplicating code. */
	    if (acl->MaxSeqLen > VENUS_MAXBSLEN)
		CHOKE("fsobj::GetAttr: BS len too large (%d)", acl->MaxSeqLen);
	    ARG_MARSHALL_BS(IN_OUT_MODE, RPC2_BoundedBS, aclvar, *acl,
			    VSG_MEMBERS, VENUS_MAXBSLEN);
	    ARG_MARSHALL(OUT_MODE, ViceStatus, statusvar, status, VSG_MEMBERS);

	    if (HAVESTATUS(this) && !getacl) {
		ViceFidAndVV FAVs[PIGGY_VALIDATIONS];    

		/* 
		 * pack piggyback fids and version vectors from this volume. 
		 * We exclude busy objects because if their validation fails,
		 * they end up in the same state (demoted) that they are now.
		 * A nice optimization would be to pack them highest priority 
		 * first, from the priority queue. Unfortunately this may not 
		 * result in the most efficient packing because only _replaceable_ 
		 * objects are in the priority queue (duh).  So others that need 
		 * to be checked may be missed, resulting in more rpcs!
		 */
		fso_vol_iterator next(NL, vol);
		fsobj *f = 0;
		int numPiggyFids = 0;

		while ((f = next()) && (numPiggyFids < PIGGY_VALIDATIONS)) {
		    if (HAVESTATUS(f) && !STATUSVALID(f) && !DYING(f) &&
			!f->flags.rwreplica && 
			!FID_EQ(&f->fid, &fid) &&
			!f->IsLocalObj() &&
			!BUSY(f)) {  

			LOG(1000, ("fsobj::GetAttr: packing piggy fid (%x.%x.%x) comp = %s\n",
				   f->fid.Volume, f->fid.Vnode, f->fid.Unique, f->comp));

			FAVs[numPiggyFids].Fid = f->fid;
			FAVs[numPiggyFids].VV = f->stat.VV;
			numPiggyFids++;
		    }
	        }

		/* we need the fids in order */
	        (void) qsort((char *) FAVs, numPiggyFids, sizeof(ViceFidAndVV), 
			     (int (*)(const void *, const void *))FAV_Compare);

		/* 
		 * another OUT parameter. We don't use an array here
		 * because each char would be embedded in a struct that
		 * would be longword aligned. Ugh.
		 */
		char VFlags[PIGGY_VALIDATIONS];
		RPC2_BoundedBS VFlagBS;

		VFlagBS.MaxSeqLen = numPiggyFids;
		VFlagBS.SeqLen  = 0;
		VFlagBS.SeqBody = (RPC2_ByteSeq)VFlags;

		ARG_MARSHALL_BS(IN_OUT_MODE, RPC2_BoundedBS, VFlagvar, VFlagBS,
				VSG_MEMBERS, VENUS_MAXBSLEN);

		/* make the RPC */
		char val_prel_str[256];
		sprintf(val_prel_str, "fetch::ValidateAttrs %%s [%d]\n", numPiggyFids);
		CFSOP_PRELUDE(val_prel_str, comp, fid);
		MULTI_START_MESSAGE(ViceValidateAttrs_OP);
		code = (int) MRPC_MakeMulti(ViceValidateAttrs_OP,
					    ViceValidateAttrs_PTR, VSG_MEMBERS,
					    m->rocc.handles, m->rocc.retcodes,
					    m->rocc.MIp, 0, 0, ph, &fid,
					    statusvar_ptrs, numPiggyFids, FAVs,
					    VFlagvar_ptrs, &PiggyBS);
		MULTI_END_MESSAGE(ViceValidateAttrs_OP);
		CFSOP_POSTLUDE("fetch::ValidateAttrs done\n");

		/* Collate */
		code = vol->Collate_NonMutating(m, code);
		MULTI_RECORD_STATS(ViceValidateAttrs_OP);

		if (code == EASYRESOLVE) { asy_resolve = 1; code = 0; }
		else if (code == 0 || code == ERETRY) {
		    /* 
		     * collate flags from vsg members. even if the return
		     * is ERETRY we can (and should) grab the flags.
		     */
		    int numVFlags = 0;

		    for (i = 0; i < VSG_MEMBERS; i++)
			if (m->rocc.hosts[i])
			    if (numVFlags == 0) {
				/* unset, copy in one response */
				ARG_UNMARSHALL_BS(VFlagvar, VFlagBS, i);
				numVFlags = (unsigned) VFlagBS.SeqLen;
			    } else {
				/* 
				 * "and" in results from other servers. 
				 * Remember that VFlagBS.SeqBody == VFlags.
				 */
				for (int j = 0; j < numPiggyFids; j++)
				    VFlags[j] &= VFlagvar_bufs[i].SeqBody[j];
			    }

		    LOG(10, ("fsobj::GetAttr: ValidateAttrs (%s), %d fids sent, %d checked\n",
			      comp, numPiggyFids, numVFlags));

		    nchecked += numPiggyFids;
		    /* 
		     * now set status of piggybacked objects 
		     */
		    for (i = 0; i < numVFlags; i++) {
			/* 
			 * lookup this object. It may have been flushed and reincarnated
			 * as a runt in the while we were out, so we check status again.
			 */
			fsobj *pobj;

			if ((pobj = FSDB->Find(&FAVs[i].Fid)))
			    if (VFlags[i] && HAVESTATUS(pobj)) {
				LOG(1000, ("fsobj::GetAttr: ValidateAttrs (%s), fid (%x.%x.%x) valid\n",
					  pobj->comp, FAVs[i].Fid.Volume, 
					  FAVs[i].Fid.Vnode, FAVs[i].Fid.Unique));

				/* Read/Write Sharing Stat Collection */
				if (pobj->flags.discread) {
				    Recov_BeginTrans();
				    RVMLIB_REC_OBJECT(pobj->flags);
				    pobj->flags.discread = 0;
				    Recov_EndTrans(MAXFP);
				}

				if (flags.usecallback && (cbtemp == cbbreaks)) {
				    if (!HAVEALLDATA(pobj))
					pobj->SetRcRights(RC_STATUS);
				    else
					pobj->SetRcRights(RC_STATUS | RC_DATA);
				    /* 
				     * if the object matched, the access rights 
				     * cached for this object are still good.
				     */
				    if (pobj->IsDir()) {
					pobj->PromoteAcRights(ALL_UIDS);
					pobj->PromoteAcRights(vuid);
				    }
			        }
			    } else {
				/* invalidate status (and data) for this object */
				LOG(1, ("fsobj::GetAttr: ValidateAttrs (%s), fid (%x.%x.%x) validation failed\n",
					pobj->comp, FAVs[i].Fid.Volume, 
					FAVs[i].Fid.Vnode, FAVs[i].Fid.Unique));
				
				/* Read/Write Sharing Stat Collection */
				if (pobj->flags.discread) {
				    Recov_BeginTrans();
				    RVMLIB_REC_OBJECT(vol->current_rws_cnt);
				    vol->current_rws_cnt++;
				    RVMLIB_REC_OBJECT(vol->current_disc_read_cnt);
				    vol->current_disc_read_cnt++;
				    RVMLIB_REC_OBJECT(pobj->flags);
				    pobj->flags.discread = 0;
				    Recov_EndTrans(MAXFP);
				}

				if (REPLACEABLE(pobj) && !BUSY(pobj)) {
					Recov_BeginTrans();
					pobj->Kill(0);
					Recov_EndTrans(MAXFP);
				} else
				    pobj->Demote(0);

				nfailed++;	
				/* 
				 * If we have data, it is stale and must be discarded,
				 * unless someone is writing or executing it, or it is
				 * a fake directory.  In that case, we wait and rely on
				 * the destructor to discard the data.
				 *
				 * We don't restart from the beginning, since the
				 * validation of piggybacked fids is a side-effect.
				 */
				if (HAVEDATA(pobj) && !WRITING(pobj) &&
				    !EXECUTING(pobj) && !pobj->IsFakeDir()) {
				    Recov_BeginTrans();
				    UpdateCacheStats((IsDir() ? &FSDB->DirDataStats 
						      : &FSDB->FileDataStats),
						     REPLACE, BLOCKS(pobj));
				    pobj->DiscardData();
				    Recov_EndTrans(MAXFP);
				}
			    }
		    }
		}

		if (code != 0) goto RepExit;
	    } else {
		/* The COP:Fetch call. */
		CFSOP_PRELUDE(prel_str, comp, fid);
		if (getacl) {
		    MULTI_START_MESSAGE(ViceGetACL_OP);
		    code = (int)MRPC_MakeMulti(ViceGetACL_OP, ViceGetACL_PTR,
					       VSG_MEMBERS, m->rocc.handles,
					       m->rocc.retcodes, m->rocc.MIp,
					       0, 0, &fid, inconok,
					       aclvar_ptrs, statusvar_ptrs, ph,
					       &PiggyBS);
		    MULTI_END_MESSAGE(ViceGetACL_OP);
		    CFSOP_POSTLUDE(post_str);

		    /* Collate responses from individual servers and decide
		     * what to do next. */
		    code = vol->Collate_NonMutating(m, code);
		    MULTI_RECORD_STATS(ViceGetACL_OP);
		} else {
		    MULTI_START_MESSAGE(ViceGetAttr_OP);
		    code = (int)MRPC_MakeMulti(ViceGetAttr_OP, ViceGetAttr_PTR,
					       VSG_MEMBERS, m->rocc.handles,
					       m->rocc.retcodes, m->rocc.MIp,
					       0, 0, &fid, inconok,
					       statusvar_ptrs, ph, &PiggyBS);
		    MULTI_END_MESSAGE(ViceGetAttr_OP);
		    CFSOP_POSTLUDE(post_str);

		    /* Collate responses from individual servers and decide
		     * what to do next. */
		    code = vol->Collate_NonMutating(m, code);
		    MULTI_RECORD_STATS(ViceGetAttr_OP);
		}

		if (code == EASYRESOLVE) { asy_resolve = 1; code = 0; }
		if (code != 0) goto RepExit;
	    }

	    /* common code for replicated case */

	    /* Finalize COP2 Piggybacking. */
	    if (PIGGYCOP2)
		vol->ClearCOP2(&PiggyBS);

	    /* Collect the OUT VVs in an array so that they can be checked. */
	    vv_t *vv_ptrs[VSG_MEMBERS];
	    for (int j = 0; j < VSG_MEMBERS; j++)
		vv_ptrs[j] = &((statusvar_ptrs[j])->VV);

	    /* Check the version vectors for consistency. */
	    code = m->RVVCheck(vv_ptrs, (int) ISDIR(fid));
	    if (code == EASYRESOLVE) { asy_resolve = 1; code = 0; }
	    if (code != 0) goto RepExit;

	    /* 
	     * Compute the dominant host set.  
	     * The index of a dominant host is returned as a side-effect. 
	     */
	    int dh_ix; dh_ix = -1;
	    code = m->DHCheck(vv_ptrs, ph_ix, &dh_ix, 1);
	    if (code != 0) goto RepExit;

	    /* Manually compute the OUT parameters from the mgrpent::GetAttr() call! -JJK */
	    if (getacl) {
		ARG_UNMARSHALL_BS(aclvar, *acl, dh_ix);
	    }
	    ARG_UNMARSHALL(statusvar, status, dh_ix);

	    /* Handle successful validation of fake directory! */
	    if (IsFakeDir()) {
		LOG(0, ("fsobj::GetAttr: (%x.%x.%x) validated fake directory\n",
			fid.Volume, fid.Vnode, fid.Unique));

		Recov_BeginTrans();
		Kill();
		Recov_EndTrans(0);

		code = ERETRY;
		goto RepExit;
	    }

	    /* Handle failed validations. */
	    if (HAVESTATUS(this) && VV_Cmp(&status.VV, &stat.VV) != VV_EQ) {
		if (LogLevel >= 1) {
		    dprint("fsobj::GetAttr: failed validation\n");
		    int *r = ((int *)&status.VV);
		    dprint("\tremote = [%x %x %x %x %x %x %x %x] [%x %x] [%x]\n",
			   r[0], r[1], r[2], r[3], r[4],
			   r[5], r[6], r[7], r[8], r[9], r[10]);
		    int *l = ((int *)&stat.VV);
		    dprint("\tlocal = [%x %x %x %x %x %x %x %x] [%x %x] [%x]\n",
			   l[0], l[1], l[2], l[3], l[4],
			   l[5], l[6], l[7], l[8], l[9], l[10]);
		}

		Demote(0);
		nfailed++;

		/* If we have data, it is stale and must be discarded. */
		/* Operation MUST be restarted from beginning since, even though this */
		/* fetch was for status-only, the operation MAY be requiring data! */
		if (HAVEDATA(this)) {
		    Recov_BeginTrans();
		    UpdateCacheStats((IsDir() ? &FSDB->DirDataStats : &FSDB->FileDataStats),
				     REPLACE, BLOCKS(this));
		    DiscardData();
		    code = ERETRY;
		    Recov_EndTrans(CMFP);
		    
		    goto RepExit;
		}
    	    }
	}

	Recov_BeginTrans();
	UpdateStatus(&status, vuid);
	Recov_EndTrans(CMFP);

	/* Read/Write Sharing Stat Collection */
	if (flags.discread) {	
	    Recov_BeginTrans();
	    RVMLIB_REC_OBJECT(flags);
	    flags.discread = 0;
	    Recov_EndTrans(MAXFP);
	}

	if (flags.usecallback &&
	    status.CallBack == CallBackSet &&
	    cbtemp == cbbreaks &&
	    !asy_resolve)
	{
	    if (!HAVEALLDATA(this))
		SetRcRights(RC_STATUS);
	    else
		SetRcRights(RC_STATUS | RC_DATA);
	}

RepExit:
	PutMgrp(&m);
	AddVCBData(nchecked, nfailed);
	if (IsFakeDir() && code == EINCONS) {
	    code = 0;
	}
	switch(code) {
	    case 0:
		if (asy_resolve)
		    vol->ResSubmit(0, &fid);
		break;

	    case ESYNRESOLVE:
		vol->ResSubmit(&((VprocSelf())->u.u_resblk), &fid);
		break;

	    case EINCONS:
		/* Read/Write Sharing Stat Collection */
		if (flags.discread) {
		    Recov_BeginTrans();
		    RVMLIB_REC_OBJECT(vol->current_rws_cnt);
		    vol->current_rws_cnt++;
		    Recov_EndTrans(MAXFP);
		}
		Recov_BeginTrans();
		Kill();
		Recov_EndTrans(CMFP);
		break;

	    case ENXIO:
		/* VNOVOL is mapped to ENXIO, and when ViceValidateAttrs
		 * returns this error for all servers, we should get rid of
		 * all cached fsobjs for this volume. */
		if (vol) {
		    fso_vol_iterator next(NL, vol);
		    fsobj *f;
		    Recov_BeginTrans();
		    while ((f = next()))
			f->Kill(0);
		    Recov_EndTrans(CMFP);
		}
		break;

	    case ENOENT:
		/* Object no longer exists, discard if possible. */
		Recov_BeginTrans();
		Kill();
		Recov_EndTrans(CMFP);
		break;

	    default:
		break;
	}
    } else {
	/* Acquire a Connection. */
	connent *c;
	code = vol->GetConn(&c, vuid);
	if (code != 0) goto NonRepExit;

	/* Make the RPC call. */
	long cbtemp; cbtemp = cbbreaks;
	CFSOP_PRELUDE(prel_str, comp, fid);
	if (getacl) {
	    UNI_START_MESSAGE(ViceGetACL_OP);
	    code = (int)ViceGetACL(c->connid, &fid, inconok, acl, &status, 0,
				   &PiggyBS);
	    UNI_END_MESSAGE(ViceGetACL_OP);
	    CFSOP_POSTLUDE(post_str);

	    /* Examine the return code to decide what to do next. */
	    code = vol->Collate(c, code);
	    UNI_RECORD_STATS(ViceGetACL_OP);
	} else {
	    UNI_START_MESSAGE(ViceGetAttr_OP);
	    code = (int)ViceGetAttr(c->connid, &fid, inconok, &status, 0,
				    &PiggyBS);
	    UNI_END_MESSAGE(ViceGetAttr_OP);
	    CFSOP_POSTLUDE(post_str);

	    /* Examine the return code to decide what to do next. */
	    code = vol->Collate(c, code);
	    UNI_RECORD_STATS(ViceGetAttr_OP);
	}
	if (code != 0) goto NonRepExit;

	/* Handle failed validations. */
	if (HAVESTATUS(this) && status.DataVersion != stat.DataVersion) {
	    LOG(1, ("fsobj::GetAttr: failed validation (%d, %d)\n",
		    status.DataVersion, stat.DataVersion));

	    Demote();

	    /* If we have data, it is stale and must be discarded. */
	    /* Operation MUST be restarted from beginning since, even though this */
	    /* fetch was for status-only, the operation MAY be requiring data! */
	    if (HAVEDATA(this)) {
		Recov_BeginTrans();
		UpdateCacheStats((IsDir() ? &FSDB->DirDataStats : &FSDB->FileDataStats),
				 REPLACE, BLOCKS(this));
		DiscardData();
		code = ERETRY;
		Recov_EndTrans(CMFP);

		goto NonRepExit;
	    }
	}

	Recov_BeginTrans();
	UpdateStatus(&status, vuid);
	Recov_EndTrans(CMFP);

	/* Read/Write Sharing Stat Collection */
	if (flags.discread) {	
		Recov_BeginTrans();
		RVMLIB_REC_OBJECT(flags);
		flags.discread = 0;
		Recov_EndTrans(MAXFP);
	}

	if (flags.usecallback &&
	    status.CallBack == CallBackSet &&
	    cbtemp == cbbreaks)
	{
	    if (!HAVEALLDATA(this))
		SetRcRights(RC_STATUS);
	    else
		SetRcRights(RC_STATUS | RC_DATA);
	}

NonRepExit:
	PutConn(&c);
    }

    if (code != 0) {
	/* Read/Write Sharing Stat Collection */
	if (flags.discread) {
	    Recov_BeginTrans();
	    RVMLIB_REC_OBJECT(vol->current_rws_cnt);
	    vol->current_rws_cnt++;
	    Recov_EndTrans(MAXFP);
	}
	Recov_BeginTrans();
	/* Demote or discard existing status. */
	if (HAVESTATUS(this) && code != ENOENT)
		Demote();
	else
		Kill();
	Recov_EndTrans(DMFP);
    }
    return(code);
}


int fsobj::GetACL(RPC2_BoundedBS *acl, vuid_t vuid) {
    LOG(10, ("fsobj::GetACL: (%s), uid = %d\n",
	      comp, vuid));

    if (!HOARDING(this) && !LOGGING(this)) {
	FSO_ASSERT(this, EMULATING(this));

	/* We don't cache ACLs! */
	return(ETIMEDOUT);
    }

    if (IsFake()) 
	return(EINVAL);

    /* check if the object is FETCHABLE first! */
    if (FETCHABLE(this))
	return(GetAttr(vuid, acl));
    else 
	return(ETIMEDOUT);
}


/*  *****  Store  *****  */

/* MUST be called from within transaction! */
void fsobj::LocalStore(Date_t Mtime, unsigned long NewLength)
{
    /* Update local state. */
    FSO_ASSERT(this, !WRITING(this));

    RVMLIB_REC_OBJECT(*this);

    stat.DataVersion++;
    stat.Length = NewLength;
    stat.Date = Mtime;

    UpdateCacheStats((IsDir() ? &FSDB->DirAttrStats : &FSDB->FileAttrStats),
                     WRITE, NBLOCKS(sizeof(fsobj)));
}


int fsobj::ConnectedStore(Date_t Mtime, vuid_t vuid, unsigned long NewLength)
{
    FSO_ASSERT(this, HOARDING(this));

    int code = 0;
    char prel_str[256];
    sprintf(prel_str, "store::Store %%s [%ld]\n", NBLOCKS(NewLength));

    /* Status parameters. */
    ViceStatus status;
    VenusToViceStatus(&stat, &status);
    {
	/* Temporary!  Until RPC interface is fixed!  -JJK */
	status.Date = Mtime;
    }

    /* COP2 Piggybacking. */
    char PiggyData[COP2SIZE];
    RPC2_CountedBS PiggyBS;
    PiggyBS.SeqLen = 0;
    PiggyBS.SeqBody = (RPC2_ByteSeq)PiggyData;

    /* Set up the SE descriptor. */
    SE_Descriptor dummysed;
    memset(&dummysed, 0, sizeof(SE_Descriptor));
    SE_Descriptor *sed = 0;
    {
	/* Must be a file! */
	sed = &dummysed;
	sed->Tag = SMARTFTP;
	struct SFTP_Descriptor *sei = &sed->Value.SmartFTPD;
	sei->TransmissionDirection = CLIENTTOSERVER;
	sei->hashmark = 0;
	sei->SeekOffset = 0;
	sei->ByteQuota = -1;
	sei->Tag = FILEBYNAME;
	sei->FileInfo.ByName.ProtectionBits = V_MODE;
	strcpy(sei->FileInfo.ByName.LocalFileName, data.file->Name());
    }

    /* VCB arguments */
    RPC2_Integer VS = 0;
    CallBackStatus VCBStatus = NoCallBack;
    RPC2_CountedBS OldVS;
    OldVS.SeqLen = 0;
    OldVS.SeqBody = 0;

    if (flags.replicated) {
	ViceStoreId sid;
	mgrpent *m = 0;
	int asy_resolve = 0;

	/* Acquire an Mgroup. */
	code = vol->GetMgrp(&m, vuid, (PIGGYCOP2 ? &PiggyBS : 0));
	if (code != 0) goto RepExit;

	/* The COP1 call. */
	long cbtemp; cbtemp = cbbreaks;
	vv_t UpdateSet;

	Recov_BeginTrans();
	sid = vol->GenerateStoreId();
	Recov_EndTrans(MAXFP);
	{
	    /* Make multiple copies of the IN/OUT and OUT parameters. */
	    int ph_ix; unsigned long ph; ph = m->GetPrimaryHost(&ph_ix);
	    vol->PackVS(m->nhosts, &OldVS);

	    ARG_MARSHALL(IN_OUT_MODE, ViceStatus, statusvar, status, VSG_MEMBERS);
	    ARG_MARSHALL(IN_OUT_MODE, SE_Descriptor, sedvar, *sed, VSG_MEMBERS);
	    ARG_MARSHALL(OUT_MODE, RPC2_Integer, VSvar, VS, VSG_MEMBERS);
	    ARG_MARSHALL(OUT_MODE, CallBackStatus, VCBStatusvar, VCBStatus, VSG_MEMBERS);

	    /* Make the RPC call. */
	    CFSOP_PRELUDE(prel_str, comp, fid);
	    MULTI_START_MESSAGE(ViceStore_OP);
	    code = (int) MRPC_MakeMulti(ViceStore_OP, ViceStore_PTR,
					VSG_MEMBERS, m->rocc.handles,
					m->rocc.retcodes, m->rocc.MIp, 0, 0,
					&fid, statusvar_ptrs, NewLength,
					ph, &sid, &OldVS, VSvar_ptrs,
					VCBStatusvar_ptrs, &PiggyBS,
					sedvar_bufs);
	    MULTI_END_MESSAGE(ViceStore_OP);
	    CFSOP_POSTLUDE("store::store done\n");

	    free(OldVS.SeqBody);
	    /* Collate responses from individual servers and decide what to do next. */
	    code = vol->Collate_COP1(m, code, &UpdateSet);
	    MULTI_RECORD_STATS(ViceStore_OP);
	    if (code == EASYRESOLVE) { asy_resolve = 1; code = 0; }
	    if (code != 0) goto RepExit;

	    /* Collate volume callback information */
	    if (cbtemp == cbbreaks)
		vol->CollateVCB(m, VSvar_bufs, VCBStatusvar_bufs);

	    /* Finalize COP2 Piggybacking. */
	    if (PIGGYCOP2)
		vol->ClearCOP2(&PiggyBS);

	    /* Manually compute the OUT parameters from the mgrpent::Store() call! -JJK */
	    int dh_ix; dh_ix = -1;
	    (void)m->DHCheck(0, ph_ix, &dh_ix);
	    ARG_UNMARSHALL(statusvar, status, dh_ix);
	    {
		long bytes = sedvar_bufs[dh_ix].Value.SmartFTPD.BytesTransferred;
		LOG(10, ("(Multi)ViceStore: stored %d bytes\n", bytes));
		if (bytes != (long)status.Length) {
		    print(logFile);
		    CHOKE("fsobj::Store: bytes mismatch (%d, %d)",
			bytes, status.Length);
		}
	    }
	}

	/* Do Store locally. */
	Recov_BeginTrans();
	LocalStore(Mtime, NewLength);
	UpdateStatus(&status, &UpdateSet, vuid);
	Recov_EndTrans(CMFP);
	if (ASYNCCOP2) ReturnEarly();

	/* Send the COP2 message or add an entry for piggybacking. */
	if (PIGGYCOP2)
	    vol->AddCOP2(&sid, &UpdateSet);
	else
	    (void)vol->COP2(m, &sid, &UpdateSet);

RepExit:
	PutMgrp(&m);
	switch(code) {
	    case 0:
		if (asy_resolve)
		    vol->ResSubmit(0, &fid);
		break;

	    default:
		/* Simulate a disconnection, to be followed by reconnection/reintegration. */
		Recov_BeginTrans();
		code = vol->LogStore(Mtime, vuid, &fid, NewLength);

		if (code == 0) {
			LocalStore(Mtime, NewLength);
			vol->flags.transition_pending = 1;
		}
		Recov_EndTrans(DMFP);
		break;
	}
    }
    else {
	/* Acquire a Connection. */
	connent *c;
	ViceStoreId Dummy;     /* ViceStore needs an address for indirection */
	code = vol->GetConn(&c, vuid);
	if (code != 0) goto NonRepExit;

	/* Make the RPC call. */
	CFSOP_PRELUDE(prel_str, comp, fid);
	UNI_START_MESSAGE(ViceStore_OP);
	code = (int) ViceStore(c->connid, &fid, &status, NewLength, 0, &Dummy,
			       &OldVS, &VS, &VCBStatus, &PiggyBS, sed);
	UNI_END_MESSAGE(ViceStore_OP);
	CFSOP_POSTLUDE("store::store done\n");

	/* Examine the return code to decide what to do next. */
	code = vol->Collate(c, code);
	UNI_RECORD_STATS(ViceStore_OP);
	if (code != 0) goto NonRepExit;

	{
	    long bytes = sed->Value.SmartFTPD.BytesTransferred;
	    LOG(10, ("ViceStore: stored %d bytes\n", bytes));
	    if (bytes != (long)status.Length) {
		print(logFile);
		CHOKE("fsobj::Store: bytes mismatch (%d, %d)",
		    bytes, status.Length);
	    }
	}

	/* Do Store locally. */
	Recov_BeginTrans();
	LocalStore(Mtime, NewLength);
	UpdateStatus(&status, 0, vuid);
	Recov_EndTrans(CMFP);

NonRepExit:
	PutConn(&c);
    }

    return(code);
}

int fsobj::DisconnectedStore(Date_t Mtime, vuid_t vuid, unsigned long NewLength, int Tid) {
    FSO_ASSERT(this, (EMULATING(this) || LOGGING(this)));

    int code = 0;

    if (!flags.replicated) {
	code = ETIMEDOUT;
	goto Exit;
    }

    Recov_BeginTrans();
	/* Failure to log a store would be most unpleasant for the user! */
	/* Probably we should try to guarantee that it never happens (e.g., by reserving a record at open). */
    code = vol->LogStore(Mtime, vuid, &fid, NewLength, Tid);
    
    if (code == 0)
	    LocalStore(Mtime, NewLength);
    Recov_EndTrans(DMFP);

Exit:
    return(code);
}

int fsobj::Store(unsigned long NewLength, Date_t Mtime, vuid_t vuid) {
    LOG(10, ("fsobj::Store: (%s), uid = %d\n",
	      comp, vuid));

    int code = 0;

    int conn, tid;
    GetOperationState(&conn, &tid);
    if (conn == 0) {
	code = DisconnectedStore(Mtime, vuid, NewLength, tid);
    }
    else {
	code = ConnectedStore(Mtime, vuid, NewLength);
    }

    if (code != 0) {
	Recov_BeginTrans();
	/* Stores cannot be retried, so we have no choice but to nuke the file. */
	if (code == ERETRY) code = EINVAL;
	Kill();
	Recov_EndTrans(DMFP);
    }
    return(code);
}


/*  *****  SetAttr/SetAcl  *****  */

/* MUST be called from within transaction! */
void fsobj::LocalSetAttr(Date_t Mtime, unsigned long NewLength,
			  Date_t NewDate, vuid_t NewOwner,
                          unsigned short NewMode)
{
    /* Update local state. */
    RVMLIB_REC_OBJECT(*this);

    if (NewLength != (unsigned long)-1) {
        FSO_ASSERT(this, !WRITING(this));

        if (HAVEDATA(this)) {
            int delta_blocks = (int) (BLOCKS(this) - NBLOCKS(NewLength));
            if (delta_blocks < 0) {
                eprint("LocalSetAttr: %d\n", delta_blocks);
            }
            UpdateCacheStats(&FSDB->FileDataStats, REMOVE, delta_blocks);
            FSDB->FreeBlocks(delta_blocks);

	    data.file->Truncate((unsigned) NewLength);
        }
        stat.Length = NewLength;
        stat.Date = Mtime;
    }
    if (NewDate != (Date_t)-1) stat.Date = NewDate;
    if (NewOwner != (vuid_t)-1) stat.Owner = NewOwner;
    if (NewMode != (unsigned short)-1) stat.Mode = NewMode;

    UpdateCacheStats((IsDir() ? &FSDB->DirAttrStats : &FSDB->FileAttrStats),
                     WRITE, NBLOCKS(sizeof(fsobj)));
}

int fsobj::ConnectedSetAttr(Date_t Mtime, vuid_t vuid, unsigned long NewLength,
			     Date_t NewDate, vuid_t NewOwner,
                             unsigned short NewMode, RPC2_CountedBS *acl)
{
    FSO_ASSERT(this, HOARDING(this));

    int code = 0;
    int setacl = (acl != 0);
    const char *prel_str=setacl?"store::setacl %s\n"  :"store::setattr %s\n";
    const char *post_str=setacl?"store::setacl done\n":"store::setattr done\n";

    RPC2_Integer Mask = 0; 

    /* Dummy argument for ACL. */
    RPC2_CountedBS dummybs;
    dummybs.SeqLen = 0;
    if (!setacl)
	acl = &dummybs;

    /* Status parameters. */
    ViceStatus status;
    VenusToViceStatus(&stat, &status);

    status.Date = NewDate;
    status.Owner = NewOwner;
    status.Mode = NewMode;
    status.Length = NewLength; 

    if (NewDate != (Date_t)-1)
      Mask |= SET_TIME;

    if (NewOwner != (vuid_t)-1)
      Mask |= SET_OWNER;

    if (NewMode != (unsigned short)-1)
      Mask |= SET_MODE;

    if (NewLength != (unsigned long)-1) {
	Mask |= SET_LENGTH;
	Mask |= SET_TIME;
	status.Date = Mtime;
    }
    LOG(100,("fsobj::ConnectedSetAttr Mask = %o\n", Mask));

    /* COP2 Piggybacking. */
    char PiggyData[COP2SIZE];
    RPC2_CountedBS PiggyBS;
    PiggyBS.SeqLen = 0;
    PiggyBS.SeqBody = (RPC2_ByteSeq)PiggyData;

    /* VCB arguments */
    RPC2_Integer VS = 0;
    CallBackStatus VCBStatus = NoCallBack;
    RPC2_CountedBS OldVS; 
    OldVS.SeqLen = 0;
    OldVS.SeqBody = 0;

    if (flags.replicated) {
	ViceStoreId sid;
	mgrpent *m = 0;
	int asy_resolve = 0;

	/* Acquire an Mgroup. */
	code = vol->GetMgrp(&m, vuid, (PIGGYCOP2 ? &PiggyBS : 0));
	if (code != 0) goto RepExit;

	/* The COP1 call. */
	long cbtemp; cbtemp = cbbreaks;
	vv_t UpdateSet;

	Recov_BeginTrans();
	sid = vol->GenerateStoreId();
	Recov_EndTrans(MAXFP);
	{
	    /* Make multiple copies of the IN/OUT and OUT parameters. */
	    int ph_ix; unsigned long ph; ph = m->GetPrimaryHost(&ph_ix);
	    vol->PackVS(m->nhosts, &OldVS);

	    ARG_MARSHALL(IN_OUT_MODE, ViceStatus, statusvar, status, VSG_MEMBERS);
	    ARG_MARSHALL(OUT_MODE, RPC2_Integer, VSvar, VS, VSG_MEMBERS);
	    ARG_MARSHALL(OUT_MODE, CallBackStatus, VCBStatusvar, VCBStatus, VSG_MEMBERS)

	    /* Make the RPC call. */
	    CFSOP_PRELUDE(prel_str, comp, fid);
	    if (setacl) {
		MULTI_START_MESSAGE(ViceSetACL_OP);
		code = (int)MRPC_MakeMulti(ViceSetACL_OP, ViceSetACL_PTR,
					   VSG_MEMBERS, m->rocc.handles,
					   m->rocc.retcodes, m->rocc.MIp, 0, 0,
					   &fid, acl, statusvar_ptrs, ph, &sid,
					   &OldVS, VSvar_ptrs,
					   VCBStatusvar_ptrs, &PiggyBS);
		MULTI_END_MESSAGE(ViceSetACL_OP);
		CFSOP_POSTLUDE(post_str);

		free(OldVS.SeqBody);
		/* Collate responses from individual servers and decide what to
		 * do next. */
		code = vol->Collate_COP1(m, code, &UpdateSet);
		MULTI_RECORD_STATS(ViceSetACL_OP);
	    } else {
		MULTI_START_MESSAGE(ViceSetAttr_OP);
		code = (int)MRPC_MakeMulti(ViceSetAttr_OP, ViceSetAttr_PTR,
					   VSG_MEMBERS, m->rocc.handles,
					   m->rocc.retcodes, m->rocc.MIp, 0, 0,
					   &fid, statusvar_ptrs, Mask, ph,
					   &sid, &OldVS, VSvar_ptrs,
					   VCBStatusvar_ptrs, &PiggyBS);
		MULTI_END_MESSAGE(ViceSetAttr_OP);
		CFSOP_POSTLUDE(post_str);

		free(OldVS.SeqBody);
		/* Collate responses from individual servers and decide what to
		 * do next. */
		code = vol->Collate_COP1(m, code, &UpdateSet);
		MULTI_RECORD_STATS(ViceSetAttr_OP);
	    }
	    if (code == EASYRESOLVE) { asy_resolve = 1; code = 0; }
	    if (code != 0) goto RepExit;

	    /* Collate volume callback information */
	    if (cbtemp == cbbreaks)
		vol->CollateVCB(m, VSvar_bufs, VCBStatusvar_bufs);

	    /* Finalize COP2 Piggybacking. */
	    if (PIGGYCOP2)
		vol->ClearCOP2(&PiggyBS);

	    /* Manually compute the OUT parameters from the mgrpent::SetAttr()
	     * call! -JJK */
	    int dh_ix; dh_ix = -1;
	    (void)m->DHCheck(0, ph_ix, &dh_ix);
	    ARG_UNMARSHALL(statusvar, status, dh_ix);
	}

	/* Do setattr locally. */
	Recov_BeginTrans();
	if (!setacl)
		LocalSetAttr(Mtime, NewLength, NewDate, NewOwner, NewMode);
	UpdateStatus(&status, &UpdateSet, vuid);
	Recov_EndTrans(CMFP);
	if (ASYNCCOP2) ReturnEarly();

	/* Send the COP2 message or add an entry for piggybacking. */
	if (PIGGYCOP2)
	    vol->AddCOP2(&sid, &UpdateSet);
	else
	    (void)vol->COP2(m, &sid, &UpdateSet);

RepExit:
	PutMgrp(&m);
	switch(code) {
	    case 0:
		if (asy_resolve)
		    vol->ResSubmit(0, &fid);
		break;

	    case ETIMEDOUT:
	    case ESYNRESOLVE:
	    case EINCONS:
		code = ERETRY;
		break;

	    default:
		break;
	}
    }
    else {
	/* Acquire a Connection. */
	connent *c;
	ViceStoreId Dummy;              /* ViceStore needs an address for indirection */
	code = vol->GetConn(&c, vuid);
	if (code != 0) goto NonRepExit;

	/* Make the RPC call. */
	CFSOP_PRELUDE(prel_str, comp, fid);
	if (setacl) {
	    UNI_START_MESSAGE(ViceSetACL_OP);
	    code = (int) ViceSetACL(c->connid, &fid, acl, &status, 0, &Dummy,
				    &OldVS, &VS, &VCBStatus, &PiggyBS);
	    UNI_END_MESSAGE(ViceSetACL_OP);
	    CFSOP_POSTLUDE("store::setacl done\n");

	    /* Examine the return code to decide what to do next. */
	    code = vol->Collate(c, code);
	    UNI_RECORD_STATS(ViceSetACL_OP);
	} else {
	    UNI_START_MESSAGE(ViceSetAttr_OP);
	    code = (int) ViceSetAttr(c->connid, &fid, &status, Mask, 0, &Dummy,
				     &OldVS, &VS, &VCBStatus, &PiggyBS);
	    UNI_END_MESSAGE(ViceSetAttr_OP);
	    CFSOP_POSTLUDE("store::setattr done\n");

	    /* Examine the return code to decide what to do next. */
	    code = vol->Collate(c, code);
	    UNI_RECORD_STATS(ViceSetAttr_OP);
	}
	if (code != 0) goto NonRepExit;

	/* Do setattr locally. */
	Recov_BeginTrans();
	if (!setacl)
		LocalSetAttr(Mtime, NewLength, NewDate, NewOwner, NewMode);
	UpdateStatus(&status, 0, vuid);
	Recov_EndTrans(CMFP);

NonRepExit:
	PutConn(&c);
    }

    return(code);
}

int fsobj::DisconnectedSetAttr(Date_t Mtime, vuid_t vuid, unsigned long NewLength, Date_t NewDate, 
			       vuid_t NewOwner, unsigned short NewMode, int Tid) {
    FSO_ASSERT(this, (EMULATING(this) || LOGGING(this)));

    int code = 0;

    Recov_BeginTrans();
    RPC2_Integer tNewMode =	(short)NewMode;	    /* sign-extend!!! */
    code = vol->LogSetAttr(Mtime, vuid, &fid, NewLength, NewDate, NewOwner, 
			   (RPC2_Unsigned)tNewMode, Tid);
    if (code == 0)
	    LocalSetAttr(Mtime, NewLength, NewDate, NewOwner, NewMode);
    Recov_EndTrans(DMFP);

    return(code);
}

int fsobj::SetAttr(struct coda_vattr *vap, vuid_t vuid, RPC2_CountedBS *acl) 
{
	Date_t NewDate = (Date_t) -1;
	unsigned long NewLength = (unsigned long) -1;
	vuid_t NewOwner = (vuid_t) -1;
	unsigned short NewMode = (unsigned short )-1;


	LOG(10, ("fsobj::SetAttr: (%s), uid = %d\n", comp, vuid));
	VPROC_printvattr(vap);
    
	if ( vap->va_size != VA_IGNORE_SIZE ) 
		NewLength = vap->va_size;

	if ( (vap->va_mtime.tv_sec != VA_IGNORE_TIME1) && 
	      (vap->va_mtime.tv_sec != stat.Date))
		NewDate = vap->va_mtime.tv_sec;

        if (vap->va_uid != VA_IGNORE_UID && vap->va_uid != stat.Owner)
		NewOwner = vap->va_uid;

	if ((vap->va_mode != VA_IGNORE_MODE) && 
	    ((vap->va_mode & 04777) != stat.Mode) )
		NewMode= (vap->va_mode & 04777);

	/* Cannot chown a file until the first store has been done! */
	if (NewOwner != (vuid_t)-1 && IsFile() && IsVirgin()) {
		return(EINVAL);
	}

	/* Only update cache file when truncating and open for write! */
	if (NewLength != (unsigned long)-1 && WRITING(this)) {
		Recov_BeginTrans();
		data.file->Truncate((unsigned) NewLength);
		Recov_EndTrans(MAXFP);
		NewLength = (unsigned long)-1;
	}

	/* If we are truncating to zero length, we have valid data */
	if (NewLength == 0) {
	    Recov_BeginTrans();
	    RVMLIB_REC_OBJECT(data.file);
	    data.file = &cf;
	    data.file->Truncate((unsigned) NewLength);
	    Recov_EndTrans(MAXFP);
	}
	
	/* Avoid performing action where possible. */
	if (NewLength == (unsigned long)-1 && NewDate == (Date_t)-1 &&
	    NewOwner == (vuid_t)-1 && NewMode == (unsigned short)-1) {
		if (acl == 0) return(0);
	} else {
		FSO_ASSERT(this, acl == 0);
	}

	int code = 0;
	Date_t Mtime = Vtime();
	
	int conn, tid;
	GetOperationState(&conn, &tid);
	
	if (conn == 0) {
                code = DisconnectedSetAttr(Mtime, vuid, NewLength, NewDate,
                                           NewOwner, NewMode, tid);
	} else {
		code = ConnectedSetAttr(Mtime, vuid, NewLength, NewDate, 
                                        NewOwner, NewMode, acl);
	}

	if (code != 0) {
		Demote();
	}
	return(code);
}


int fsobj::SetACL(RPC2_CountedBS *acl, vuid_t vuid) {
    LOG(10, ("fsobj::SetACL: (%s), uid = %d\n",
	      comp, vuid));

    if (!HOARDING(this)) {
	FSO_ASSERT(this, (EMULATING(this) || LOGGING(this)));

	/* We don't cache ACLs! */
	return(ETIMEDOUT);
    }

    struct coda_vattr va;
    va_init(&va);
    int code = SetAttr(&va, vuid, acl);

    if (code == 0) {
	/* Cached rights are suspect now! */
	Demote();
    }

    return(code);
}


/*  *****  Create  *****  */

/* MUST be called from within transaction! */
void fsobj::LocalCreate(Date_t Mtime, fsobj *target_fso, char *name,
			 vuid_t Owner, unsigned short Mode) {
    /* Update parent status. */
    {
	/* Add the new <name, fid> to the directory. */
	dir_Create(name, &target_fso->fid);

	/* Update the status to reflect the create. */
	RVMLIB_REC_OBJECT(stat);
	stat.DataVersion++;
	stat.Length = dir_Length();
	stat.Date = Mtime;
    }

    /* Set target status and data. */
    {
	/* Initialize the target fsobj. */
	RVMLIB_REC_OBJECT(*target_fso);
	target_fso->stat.VnodeType = File;
	target_fso->stat.LinkCount = 1;
	target_fso->stat.Length = 0;
	target_fso->stat.DataVersion = 0;
	target_fso->stat.Date = Mtime;
	target_fso->stat.Owner = Owner;
	target_fso->stat.Mode = Mode;
	target_fso->Matriculate();
	target_fso->SetParent(fid.Vnode, fid.Unique);

	/* Contents are already initialized to null. */
	target_fso->data.file = &target_fso->cf;
	/* We don't bother doing a ChangeDiskUsage() here since NBLOCKS(target_fso->stat.Length) == 0. */

	target_fso->Reference();
	target_fso->ComputePriority();
    }
}


int fsobj::ConnectedCreate(Date_t Mtime, vuid_t vuid, fsobj **t_fso_addr,
			    char *name, unsigned short Mode, int target_pri) {
    FSO_ASSERT(this, HOARDING(this));

    int code = 0;
    fsobj *target_fso = 0;
    ViceFid target_fid;
    RPC2_Unsigned AllocHost = 0;

    /*Status parameters. */
    ViceStatus parent_status;
    VenusToViceStatus(&stat, &parent_status);
    ViceStatus target_status;
    target_status.Mode = Mode;
    {
	/* Temporary!  Until RPC interface is fixed!  -JJK */
	parent_status.Date = Mtime;
	target_status.DataVersion = 0;
	target_status.VV = NullVV;
    }

    /* COP2 Piggybacking. */
    char PiggyData[COP2SIZE];
    RPC2_CountedBS PiggyBS;
    PiggyBS.SeqLen = 0;
    PiggyBS.SeqBody = (RPC2_ByteSeq)PiggyData;

    /* VCB Arguments */
    RPC2_Integer VS = 0;
    CallBackStatus VCBStatus = NoCallBack;
    RPC2_CountedBS OldVS; 
    OldVS.SeqLen = 0;
    OldVS.SeqBody = 0;

    if (flags.replicated) {
	ViceStoreId sid;
	mgrpent *m = 0;
	int asy_resolve = 0;

	/* Allocate a fid for the new object. */
	code = vol->AllocFid(File, &target_fid, &AllocHost, vuid);
	if (code != 0) goto RepExit;

	/* Allocate the fsobj. */
	target_fso = FSDB->Create(&target_fid, WR, target_pri, name);
	if (target_fso == 0) {
	    UpdateCacheStats(&FSDB->FileAttrStats, NOSPACE,
			     NBLOCKS(sizeof(fsobj)));
	    code = ENOSPC;
	    goto RepExit;
	}
	UpdateCacheStats(&FSDB->FileAttrStats, CREATE,
			 NBLOCKS(sizeof(fsobj)));

	/* Acquire an Mgroup. */
	code = vol->GetMgrp(&m, vuid, (PIGGYCOP2 ? &PiggyBS : 0));
	if (code != 0) goto RepExit;

	/* The COP1 call. */
	long cbtemp; cbtemp = cbbreaks;
	vv_t UpdateSet;

	Recov_BeginTrans();
	sid = vol->GenerateStoreId();
	Recov_EndTrans(MAXFP);
	{
	    /* Make multiple copies of the IN/OUT and OUT parameters. */
	    vol->PackVS(m->nhosts, &OldVS);
	    ARG_MARSHALL(IN_OUT_MODE, ViceStatus, target_statusvar, target_status, VSG_MEMBERS);
	    ARG_MARSHALL(IN_OUT_MODE, ViceFid, target_fidvar, target_fid, VSG_MEMBERS);
	    ARG_MARSHALL(IN_OUT_MODE, ViceStatus, parent_statusvar, parent_status, VSG_MEMBERS);
	    ARG_MARSHALL(OUT_MODE, RPC2_Integer, VSvar, VS, VSG_MEMBERS);
	    ARG_MARSHALL(OUT_MODE, CallBackStatus, VCBStatusvar, VCBStatus, VSG_MEMBERS);

	    /* Make the RPC call. */
	    CFSOP_PRELUDE("store::Create %-30s\n", name, NullFid);
	    MULTI_START_MESSAGE(ViceVCreate_OP);
	    code = (int) MRPC_MakeMulti(ViceVCreate_OP, ViceVCreate_PTR,
					VSG_MEMBERS, m->rocc.handles,
					m->rocc.retcodes, m->rocc.MIp, 0, 0,
					&fid, &NullFid, name,
					target_statusvar_ptrs, target_fidvar_ptrs,
					parent_statusvar_ptrs, AllocHost, &sid,
					&OldVS, VSvar_ptrs, VCBStatusvar_ptrs,
					&PiggyBS);
	    MULTI_END_MESSAGE(ViceVCreate_OP);
	    CFSOP_POSTLUDE("store::create done\n");

	    free(OldVS.SeqBody);
	    /* Collate responses from individual servers and decide what to do next. */
	    code = vol->Collate_COP1(m, code, &UpdateSet);
	    MULTI_RECORD_STATS(ViceVCreate_OP);
	    if (code == EASYRESOLVE) { asy_resolve = 1; code = 0; }
	    if (code != 0) goto RepExit;

	    /* Collate volume callback information */
	    if (cbtemp == cbbreaks)
		vol->CollateVCB(m, VSvar_bufs, VCBStatusvar_bufs);

	    /* Finalize COP2 Piggybacking. */
	    if (PIGGYCOP2)
		vol->ClearCOP2(&PiggyBS);

	    /* Manually compute the OUT parameters from the mgrpent::Create() call! -JJK */
	    int dh_ix; dh_ix = -1;
	    (void)m->DHCheck(0, -1, &dh_ix);
	    ARG_UNMARSHALL(target_statusvar, target_status, dh_ix);
	    ARG_UNMARSHALL(target_fidvar, target_fid, dh_ix);
	    ARG_UNMARSHALL(parent_statusvar, parent_status, dh_ix);
	}

	/* Do Create locally. */
	Recov_BeginTrans();
	LocalCreate(Mtime, target_fso, name, vuid, Mode);
	UpdateStatus(&parent_status, &UpdateSet, vuid);
	target_fso->UpdateStatus(&target_status, &UpdateSet, vuid);
	Recov_EndTrans(CMFP);
	if (target_fso->flags.usecallback &&
	    target_status.CallBack == CallBackSet &&
	    cbtemp == cbbreaks)
	    target_fso->SetRcRights(RC_STATUS | RC_DATA);
	if (ASYNCCOP2) target_fso->ReturnEarly();

	/* Send the COP2 message or add an entry for piggybacking. */
	if (PIGGYCOP2)
	    vol->AddCOP2(&sid, &UpdateSet);
	else
	    (void)vol->COP2(m, &sid, &UpdateSet);

RepExit:
	PutMgrp(&m);
	switch(code) {
	    case 0:
		if (asy_resolve) {
		    vol->ResSubmit(0, &fid);
		    if (target_fso != 0)
			vol->ResSubmit(0, &target_fso->fid);
		}
		break;

	    case ETIMEDOUT:
	    case ESYNRESOLVE:
	    case EINCONS:
		code = ERETRY;
		break;

	    default:
		break;
	}
    }
    else {
	/* Acquire a Connection. */
	connent *c;
	ViceStoreId Dummy;                   /* Need an address for ViceCreate */
	code = vol->GetConn(&c, vuid);
	if (code != 0) goto NonRepExit;

	/* Make the RPC call. */
	long cbtemp; cbtemp = cbbreaks;
	CFSOP_PRELUDE("store::Create %-30s\n", name, NullFid);
	UNI_START_MESSAGE(ViceVCreate_OP);
	code = (int) ViceVCreate(c->connid, &fid, &NullFid,
				 (RPC2_String)name, &target_status, 
				 &target_fid, &parent_status, 0, &Dummy, 
				 &OldVS, &VS, &VCBStatus, &PiggyBS);
	UNI_END_MESSAGE(ViceVCreate_OP);
	CFSOP_POSTLUDE("store::create done\n");

	/* Examine the return code to decide what to do next. */
	code = vol->Collate(c, code);
	UNI_RECORD_STATS(ViceVCreate_OP);
	if (code != 0) goto NonRepExit;

	/* Allocate the fsobj. */
	target_fso = FSDB->Create(&target_fid, WR, target_pri, name);
	if (target_fso == 0) {
	    UpdateCacheStats(&FSDB->FileAttrStats, NOSPACE,
			     NBLOCKS(sizeof(fsobj)));
	    code = ENOSPC;
	    goto NonRepExit;
	}
	UpdateCacheStats(&FSDB->FileAttrStats, CREATE,
			 NBLOCKS(sizeof(fsobj)));

	/* Do Create locally. */
	Recov_BeginTrans();
	LocalCreate(Mtime, target_fso, name, vuid, Mode);
	UpdateStatus(&parent_status, 0, vuid);
	target_fso->UpdateStatus(&target_status, 0, vuid);
	Recov_EndTrans(CMFP);
	if (target_fso->flags.usecallback &&
	    target_status.CallBack == CallBackSet &&
	    cbtemp == cbbreaks)
	    target_fso->SetRcRights(RC_STATUS | RC_DATA);

NonRepExit:
	PutConn(&c);
    }

    if (code == 0) {
	*t_fso_addr = target_fso;
    }
    else {
	if (target_fso != 0) {
	    FSO_ASSERT(target_fso, !HAVESTATUS(target_fso));
	    Recov_BeginTrans();
	    target_fso->Kill();
	    Recov_EndTrans(DMFP);
	    FSDB->Put(&target_fso);
	}
    }
    return(code);
}

int fsobj::DisconnectedCreate(Date_t Mtime, vuid_t vuid, fsobj **t_fso_addr, char *name, 
			      unsigned short Mode, int target_pri, int Tid) {
    FSO_ASSERT(this, (EMULATING(this) || LOGGING(this)));

    int code = 0;
    fsobj *target_fso = 0;
    ViceFid target_fid;
    RPC2_Unsigned AllocHost = 0;

    if (!flags.replicated) {
	code = ETIMEDOUT;
	goto Exit;
    }

    /* Allocate a fid for the new object. */
    /* if we time out, return so we will try again with a local fid. */
    code = vol->AllocFid(File, &target_fid, &AllocHost, vuid);
    if (code != 0) goto Exit;

    /* Allocate the fsobj. */
    target_fso = FSDB->Create(&target_fid, WR, target_pri, name);
    if (target_fso == 0) {
	UpdateCacheStats(&FSDB->FileAttrStats, NOSPACE,
			 NBLOCKS(sizeof(fsobj)));
	code = ENOSPC;
	goto Exit;
    }
    UpdateCacheStats(&FSDB->FileAttrStats, CREATE,
		      NBLOCKS(sizeof(fsobj)));

    Recov_BeginTrans();
    code = vol->LogCreate(Mtime, vuid, &fid, name, &target_fso->fid, Mode, Tid);

    if (code == 0) {
	    /* This MUST update second-class state! */
	    LocalCreate(Mtime, target_fso, name, vuid, Mode);

	    /* target_fso->stat is not initialized until LocalCreate */
	    RVMLIB_REC_OBJECT(target_fso->CleanStat);
	    target_fso->CleanStat.Length = target_fso->stat.Length;
	    target_fso->CleanStat.Date = target_fso->stat.Date;
	   }
    Recov_EndTrans(DMFP);

Exit:
    if (code == 0) {
	*t_fso_addr = target_fso;
    }
    else {
	if (target_fso != 0) {
	    FSO_ASSERT(target_fso, !HAVESTATUS(target_fso));
	    Recov_BeginTrans();
	    target_fso->Kill();
	    Recov_EndTrans(DMFP);
	    FSDB->Put(&target_fso);
	}
    }
    return(code);
}


/* Returns target object write-locked (on success). */
int fsobj::Create(char *name, fsobj **target_fso_addr,
		   vuid_t vuid, unsigned short Mode, int target_pri) {
    LOG(10, ("fsobj::Create: (%s, %s, %d), uid = %d\n",
	      comp, name, target_pri, vuid));

    int code = 0;
    Date_t Mtime = Vtime();
    *target_fso_addr = 0;

    int conn, tid;
    GetOperationState(&conn, &tid);

    if (conn == 0) {
	code = DisconnectedCreate(Mtime, vuid, target_fso_addr,
				  name, Mode, target_pri, tid);
    }
    else {
	code = ConnectedCreate(Mtime, vuid, target_fso_addr,
			       name, Mode, target_pri);
    }

    if (code != 0) {
	Demote();
    }
    return(code);
}
