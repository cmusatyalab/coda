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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/venus/fso_cfscalls0.cc,v 4.1 1997/01/08 21:51:26 rvb Exp $";
#endif /*_BLURB_*/







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
#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#endif /* __MACH__ */
#ifdef __NetBSD__
#include <unistd.h>
#include <stdlib.h>
#endif __NetBSD__
#include <rpc2.h>
#include <se.h>

#ifdef __cplusplus
}
#endif __cplusplus

/* interfaces */
#include <vice.h>

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



/* This is defined in kernel's sys/inode.h, but there's no way to get that file
 * officially installed where it should be. So I'll use this ugly hack.
 */
#define IPREFETCH 0x8000  

/*  *****  Fetch  *****  */

/* local-repair modification */
int fsobj::Fetch(vuid_t vuid) {
    LOG(10, ("fsobj::Fetch: (%s), uid = %d\n", comp, vuid));

    if (IsLocalObj()) {
	LOG(10, ("fsobj::Fetach: (%s), uid = %d, local object\n", comp, vuid));
	/* set the valid RC status */
	if (HAVEDATA(this)) {
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
	    { print(logFile); Choke("fsobj::Fetch: !HAVESTATUS"); }

	/* We never fetch data if we already have some. */
	if (HAVEDATA(this))
	    { print(logFile); Choke("fsobj::Fetch: HAVEDATA"); }

	/* We never fetch data for fake objects. */
	if (IsFake())
	    { print(logFile); Choke("fsobj::Fetch: IsFake"); }
    }

    int code = 0;
    char prel_str[256];
    sprintf(prel_str, "fetch::Fetch %%s [%d]\n", BLOCKS(this));
    ViceFetchType fetchtype = (flags.rwreplica ? FetchDataRepair : FetchData);

    /* Dummy argument for ACL. */
    RPC2_BoundedBS dummybs;
    dummybs.MaxSeqLen = 0;
    dummybs.SeqLen = 0;
    RPC2_BoundedBS *acl = &dummybs;

    /* Status parameters. */
    ViceStatus status;
    bzero(&status, (int)sizeof(ViceStatus));

    /* COP2 Piggybacking. */
    char PiggyData[COP2SIZE];
    RPC2_CountedBS PiggyBS;
    PiggyBS.SeqLen = 0;
    PiggyBS.SeqBody = (RPC2_ByteSeq)PiggyData;

    /* Set up the SE descriptor. */
    SE_Descriptor dummysed;
    SE_Descriptor *sed = 0;

    /* C++ 3.0 whines if the following decls moved closer to use  -- Satya */
    int i, fd = 0, npages;  
    VenusDirPage *pageptr;
    {
	ATOMIC(
	    RVMLIB_REC_OBJECT(flags);
	    flags.fetching = 1;

	    sed = &dummysed;
	    sed->Tag = SMARTFTP;
	    struct SFTP_Descriptor *sei = &sed->Value.SmartFTPD;
	    sei->TransmissionDirection = SERVERTOCLIENT;
	    sei->hashmark = 0;
	    sei->SeekOffset = 0;
	    sei->ByteQuota = -1;
	    switch(stat.VnodeType) {
		case File:
		    RVMLIB_REC_OBJECT(data.file);
		    data.file = &cf;
		    /*
		     * If the operation is a prefetch and the open succeeds, use the open
		     * file. The IPREFETCH flag tells the kernel to manage the file's 
		     * buffers to avoid overrunning the buffer cache with prefetch data.
		     */
		    if ((VprocSelf())->prefetch) {
			fd = ::open(data.file->Name(), (IPREFETCH|O_WRONLY|O_CREAT|O_TRUNC),V_MODE);
			if (fd > 0) {
			    sei->Tag = FILEBYFD;
			    sei->FileInfo.ByFD.fd = fd;
			} else
			    fd = 0;
		    }

		    /* If the open failed (or wasn't tried) use the default mechanism */
		    if (fd == 0) {
			sei->Tag = FILEBYNAME;
			sei->FileInfo.ByName.ProtectionBits = V_MODE;
			strcpy(sei->FileInfo.ByName.LocalFileName, data.file->Name());
		    }

		    break;

		case Directory:
		    FSO_ASSERT(this, (stat.Length & (PAGESIZE - 1)) == 0);
		    RVMLIB_REC_OBJECT(data.dir);
		    data.dir = (VenusDirData *)RVMLIB_REC_MALLOC((int)sizeof(VenusDirData) + (unsigned) stat.Length);
		    RVMLIB_REC_OBJECT(*data.dir);
		    bzero(data.dir, (int)sizeof(VenusDirData));
		    npages = (int) stat.Length >> LOGPS;
		    pageptr = (VenusDirPage *)((char *)data.dir + (int)sizeof(VenusDirData));
		    for (i = 0; i < npages; i++, pageptr++)
			data.dir->pages[i] = pageptr;
		    sei->Tag = FILEINVM;
		    sei->FileInfo.ByAddr.vmfile.MaxSeqLen = stat.Length;
		    sei->FileInfo.ByAddr.vmfile.SeqBody = (RPC2_ByteSeq)(data.dir->pages[0]);
		    break;

		case SymbolicLink:
		    RVMLIB_REC_OBJECT(data.symlink);
		    /* Malloc one extra byte in case length is 0 (as for runts)! */
		    data.symlink = (char *)RVMLIB_REC_MALLOC((unsigned) stat.Length + 1);
		    sei->Tag = FILEINVM;
		    sei->FileInfo.ByAddr.vmfile.MaxSeqLen = stat.Length;
		    sei->FileInfo.ByAddr.vmfile.SeqBody = (RPC2_ByteSeq)data.symlink;
		    break;

		case Invalid:
		    FSO_ASSERT(this, 0);
	    }
	, CMFP)
    }

    if (flags.replicated) {
	mgrpent *m = 0;
	int asy_resolve = 0;

	/* Acquire an Mgroup. */
	code = vol->GetMgrp(&m, vuid, (PIGGYCOP2 ? &PiggyBS : 0));
	if (code != 0) goto RepExit;

	/* The COP:Fetch call. */
	long cbtemp; cbtemp = cbbreaks;
	{
	    /* Make multiple copies of the IN/OUT and OUT parameters. */
	    int ph_ix;
	    unsigned long ph; ph = m->GetPrimaryHost(&ph_ix);
	    if (acl->MaxSeqLen > VENUS_MAXBSLEN)
		Choke("fsobj::Fetch: BS len too large (%d)", acl->MaxSeqLen);
	    ARG_MARSHALL_BS(IN_OUT_MODE, RPC2_BoundedBS, aclvar, *acl, VSG_MEMBERS, VENUS_MAXBSLEN);
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
				  &fid, &NullFid, fetchtype, aclvar_ptrs,
				  statusvar_ptrs, ph, &PiggyBS, sedvar_bufs);
	    MULTI_END_MESSAGE(ViceFetch_OP);
	    CFSOP_POSTLUDE("fetch::fetch done\n");

	    /* Collate responses from individual servers and decide what to do next. */
	    code = vol->Collate_NonMutating(m, code);
	    MULTI_RECORD_STATS(ViceFetch_OP);
	    if (code == EASYRESOLVE) { asy_resolve = 1; code = 0; }
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

	    /* Compute the dominant host set.  The index of a dominant host is returned as a side-effect. */
	    int dh_ix; dh_ix = -1;
	    code = m->DHCheck(vv_ptrs, ph_ix, &dh_ix, 1);
	    if (code != 0) goto RepExit;

	    /* Manually compute the OUT parameters from the mgrpent::Fetch() call! -JJK */
	    ARG_UNMARSHALL(statusvar, status, dh_ix);
	    {
		long bytes = sedvar_bufs[dh_ix].Value.SmartFTPD.BytesTransferred;
		LOG(10, ("(Multi)ViceFetch: fetched %d bytes\n", bytes));
		if (bytes != status.Length) {
		    print(logFile);
		    Choke("fsobj::Fetch: bytes mismatch (%d, %d)",
			bytes, status.Length);
		}

		/* The following is needed until ViceFetch takes a version IN parameter! */
		if (NBLOCKS(bytes) != BLOCKS(this)) {
		    LOG(0, ("fsobj::Fetch: nblocks changed during fetch (%d, %d)\n",
			    NBLOCKS(bytes), BLOCKS(this)));
		    /* 
		     * for a directory, pages are allocated based on the original
		     * status block.  If there is a mismatch, the new data may be
		     * bogus, so we force a retry of the fetch.  We fall through 
		     * the remainder of the replicated case to install new status.
		     * Data is discarded just before return.  It is possible for 
		     * this case to recur; Venus will return EWOULDBLOCK if it
		     * exhausts its retries.
		     */
		    if (IsFile()) 
			FSDB->ChangeDiskUsage((int) (NBLOCKS(bytes) - BLOCKS(this)));
		    else 
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
		Demote(0);
	    }
	}

	ATOMIC(
	    UpdateStatus(&status, vuid);
	, CMFP)

	/* Read/Write Sharing Stat Collection */
	if (flags.discread) {	
	    ATOMIC(
		   RVMLIB_REC_OBJECT(flags);
		   flags.discread = 0;
	    , MAXFP)
	}

	if (flags.usecallback &&
	    status.CallBack == CallBackSet &&
	    cbtemp == cbbreaks)
	    SetRcRights(RC_STATUS | RC_DATA);

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
	long cbtemp; cbtemp = cbbreaks;
	CFSOP_PRELUDE(prel_str, comp, fid);
	UNI_START_MESSAGE(ViceFetch_OP);
	code = (int) ViceFetch(c->connid, &fid, &NullFid, fetchtype,
			 acl, &status, 0, &PiggyBS, sed);
	UNI_END_MESSAGE(ViceFetch_OP);
	CFSOP_POSTLUDE("fetch::fetch done\n");

	/* Examine the return code to decide what to do next. */
	code = vol->Collate(c, code);
	UNI_RECORD_STATS(ViceFetch_OP);
	if (code != 0) goto NonRepExit;

	{
	    long bytes = sed->Value.SmartFTPD.BytesTransferred;
	    LOG(10, ("ViceFetch: fetched %d bytes\n", bytes));
	    if (bytes != status.Length) {
		print(logFile);
		Choke("fsobj::Fetch: bytes mismatch (%d, %d)",
		    bytes, status.Length);
	    }

	    /* The following is needed until ViceFetch takes a version IN parameter! */
	    if (NBLOCKS(bytes) != BLOCKS(this)) {
		LOG(0, ("fsobj::Fetch: nblocks changed during fetch (%d, %d)\n",
			NBLOCKS(bytes), BLOCKS(this)));
		FSO_ASSERT(this, IsFile());
		FSDB->ChangeDiskUsage((int) (NBLOCKS(bytes) - BLOCKS(this)));
	    }
	}

	/* Handle failed validations. */
	if (HAVESTATUS(this) && status.DataVersion != stat.DataVersion) {
	    LOG(1, ("fsobj::Fetch: failed validation (%d, %d)\n",
		    status.DataVersion, stat.DataVersion));

	    Demote(0);
	}

	ATOMIC(
	    UpdateStatus(&status, vuid);
	, CMFP)

	/* Read/Write Sharing Stat Collection */
	if (flags.discread) {	
	    ATOMIC(
		   RVMLIB_REC_OBJECT(flags);
		   flags.discread = 0;
	    , MAXFP)
	}

	if (flags.usecallback &&
	    status.CallBack == CallBackSet &&
	    cbtemp == cbbreaks)
	    SetRcRights(RC_STATUS | RC_DATA);

NonRepExit:
	PutConn(&c);
    }

    if ((VprocSelf())->prefetch && sed->Value.SmartFTPD.FileInfo.ByFD.fd) {
	::close(sed->Value.SmartFTPD.FileInfo.ByFD.fd);
    }
    
    if (code == 0) {
	/* Note the presence of data. */
	ATOMIC(
	    RVMLIB_REC_OBJECT(flags);
	    flags.fetching = 0;

	    switch(stat.VnodeType) {
		case File:
		    data.file->SetLength((unsigned) stat.Length);
		    break;

		case Directory:
		    RVMLIB_SET_RANGE(((char *)data.dir + (int)sizeof(VenusDirData)), stat.Length);
		    break;

		case SymbolicLink:
		    RVMLIB_SET_RANGE(data.symlink, stat.Length);
		    break;

		case Invalid:
		    FSO_ASSERT(this, 0);
	    }
	, CMFP)
    }
    else {
       /* 
	* Return allocation and truncate. If a file, set the cache
	* file length so that DiscardData releases the correct
	* number of blocks (i.e., the number allocated in fsdb::Get).
	*/
	ATOMIC(
	    RVMLIB_REC_OBJECT(flags);
	    flags.fetching = 0;
	    if (IsFile()) 
	       data.file->SetLength((unsigned) stat.Length);
	    DiscardData();

	    /* Demote existing status. */
	    Demote();
	, CMFP)
    }
    return(code);
}


/*  *****  GetAttr/GetAcl  *****  */

/* local-repair modification */
int fsobj::GetAttr(vuid_t vuid, RPC2_BoundedBS *acl) {
    LOG(10, ("fsobj::GetAttr: (%s), uid = %d\n", comp, vuid));

    if (IsLocalObj()) {
	LOG(0, ("fsobj::GetAttr: (%s), uid = %d, local object\n", comp, vuid));
	/* set the valid RC status */
	if (HAVEDATA(this)) {
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
	    if (IsFakeDir() && !HAVEDATA(this))
		{ print(logFile); Choke("fsobj::GetAttr: IsFakeDir && !HAVEDATA"); }

	    /* We never fetch fake mtpts (covered or uncovered). */
	    if (IsFakeMtPt() || IsFakeMTLink())
		{ print(logFile); Choke("fsobj::GetAttr: IsFakeMtPt || IsFakeMTLink"); }
	}
    }

    int code = 0;
    int getacl = (acl != 0);
    char *prel_str = getacl ? "fetch::GetAcl %s\n" : "fetch::GetAttr %s\n";
    char *post_str = getacl ? "fetch::getacl done\n" : "fetch::getattr done\n";
    ViceFetchType fetchtype = (flags.rwreplica ? FetchNoDataRepair : FetchNoData);

    /* Dummy argument for ACL. */
    RPC2_BoundedBS dummybs;
    dummybs.MaxSeqLen = 0;
    dummybs.SeqLen = 0;
    if (!getacl)
	acl = &dummybs;

    /* Status parameters. */
    ViceStatus status;
    bzero(&status, (int)sizeof(ViceStatus));

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
		Choke("fsobj::Fetch: BS len too large (%d)", acl->MaxSeqLen);
	    ARG_MARSHALL_BS(IN_OUT_MODE, RPC2_BoundedBS, aclvar, *acl, VSG_MEMBERS, VENUS_MAXBSLEN);

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
			!BUSY(f) && !f->flags.rwreplica && !FID_EQ(f->fid, fid) &&
			!f->IsLocalObj()) {  

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
		RPC2_CountedBS VFlagBS;
		VFlagBS.SeqLen = 0;
		VFlagBS.SeqBody = (RPC2_ByteSeq)VFlags;

		ARG_MARSHALL_BS(IN_OUT_MODE, RPC2_CountedBS, VFlagvar, VFlagBS, VSG_MEMBERS, VENUS_MAXBSLEN);

		/* make the RPC */
		char val_prel_str[256];
		sprintf(val_prel_str, "fetch::ValidateAttrs %%s [%d]\n", numPiggyFids);
		CFSOP_PRELUDE(val_prel_str, comp, fid);
		MULTI_START_MESSAGE(ViceValidateAttrs_OP);
		code = (int) MRPC_MakeMulti(ViceValidateAttrs_OP, ViceValidateAttrs_PTR,
					    VSG_MEMBERS, m->rocc.handles,
					    m->rocc.retcodes, m->rocc.MIp, 0, 0,
					    ph, &fid, statusvar_ptrs, numPiggyFids, 
					    FAVs, VFlagvar_ptrs, &PiggyBS);
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
		    unsigned numVFlags = 0;

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

			if (pobj = FSDB->Find(&FAVs[i].Fid))
			    if (VFlags[i] && HAVESTATUS(pobj)) {
				LOG(1000, ("fsobj::GetAttr: ValidateAttrs (%s), fid (%x.%x.%x) valid\n",
					  pobj->comp, FAVs[i].Fid.Volume, 
					  FAVs[i].Fid.Vnode, FAVs[i].Fid.Unique));

				/* Read/Write Sharing Stat Collection */
				if (pobj->flags.discread) {
				    ATOMIC(
					   RVMLIB_REC_OBJECT(pobj->flags);
					   pobj->flags.discread = 0;
				    , MAXFP)
				}

				if (flags.usecallback && (cbtemp == cbbreaks)) {
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
				    ATOMIC(
					   RVMLIB_REC_OBJECT(vol->current_rws_cnt);
					   vol->current_rws_cnt++;
					   RVMLIB_REC_OBJECT(vol->current_disc_read_cnt);
					   vol->current_disc_read_cnt++;
					   RVMLIB_REC_OBJECT(pobj->flags);
					   pobj->flags.discread = 0;
				    , MAXFP)
				}

				if (REPLACEABLE(pobj) && !BUSY(pobj))
				    ATOMIC(
				      pobj->Kill(0);
				    , MAXFP)
				else
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
				if (HAVEDATA(pobj) && !WRITING(pobj) && !EXECUTING(pobj) && !pobj->IsFakeDir()) {
				    ATOMIC(
					   UpdateCacheStats((IsDir() ? &FSDB->DirDataStats 
							     : &FSDB->FileDataStats),
							    REPLACE, BLOCKS(pobj));
					   pobj->DiscardData();
					   , MAXFP)
				}
			    }
		    }
		}

		if (code != 0) goto RepExit;
	    } else {
		/* The COP:Fetch call. */
		CFSOP_PRELUDE(prel_str, comp, fid);
		MULTI_START_MESSAGE(ViceFetch_OP);
		code = (int) MRPC_MakeMulti(ViceFetch_OP, ViceFetch_PTR,
				      VSG_MEMBERS, m->rocc.handles,
				      m->rocc.retcodes, m->rocc.MIp, 0, 0,
				      &fid, &NullFid, fetchtype, aclvar_ptrs,
				      statusvar_ptrs, ph, &PiggyBS, 0);
		MULTI_END_MESSAGE(ViceFetch_OP);
		CFSOP_POSTLUDE(post_str);

		/* Collate responses from individual servers and decide what to do next. */
		code = vol->Collate_NonMutating(m, code);
		MULTI_RECORD_STATS(ViceFetch_OP);
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

		TRANSACTION(
		    Kill();
		)

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
		    ATOMIC(
			UpdateCacheStats((IsDir() ? &FSDB->DirDataStats : &FSDB->FileDataStats),
					 REPLACE, BLOCKS(this));
			DiscardData();
			code = ERETRY;
		    , CMFP)

		    goto RepExit;
		}
    	    }
	}

	ATOMIC(
	    UpdateStatus(&status, vuid);
	, CMFP)

	/* Read/Write Sharing Stat Collection */
	if (flags.discread) {	
	    ATOMIC(
		   RVMLIB_REC_OBJECT(flags);
		   flags.discread = 0;
	    , MAXFP)
	}

	if (flags.usecallback &&
	    status.CallBack == CallBackSet &&
	    cbtemp == cbbreaks &&
	    !asy_resolve)
	    SetRcRights(RC_STATUS | RC_DATA);

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
		    ATOMIC(
			   RVMLIB_REC_OBJECT(vol->current_rws_cnt);
			   vol->current_rws_cnt++;
		    , MAXFP)
		}
		ATOMIC(
		    Kill();
		, CMFP)
		break;

	    case ENOENT:
		/* Object no longer exists, discard if possible. */
		ATOMIC(
		   Kill();
		, CMFP)
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
	long cbtemp; cbtemp = cbbreaks;
	CFSOP_PRELUDE(prel_str, comp, fid);
	UNI_START_MESSAGE(ViceFetch_OP);
	code = (int) ViceFetch(c->connid, &fid, &NullFid, fetchtype,
			 acl, &status, 0, &PiggyBS, 0);
	UNI_END_MESSAGE(ViceFetch_OP);
	CFSOP_POSTLUDE(post_str);

	/* Examine the return code to decide what to do next. */
	code = vol->Collate(c, code);
	UNI_RECORD_STATS(ViceFetch_OP);
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
		ATOMIC(
		    UpdateCacheStats((IsDir() ? &FSDB->DirDataStats : &FSDB->FileDataStats),
				     REPLACE, BLOCKS(this));
		    DiscardData();
		    code = ERETRY;
		, CMFP)

		goto NonRepExit;
	    }
	}

	ATOMIC(
	    UpdateStatus(&status, vuid);
	, CMFP)

	/* Read/Write Sharing Stat Collection */
	if (flags.discread) {	
	    ATOMIC(
		   RVMLIB_REC_OBJECT(flags);
		   flags.discread = 0;
	    , MAXFP)
	}

	if (flags.usecallback &&
	    status.CallBack == CallBackSet &&
	    cbtemp == cbbreaks)
	    SetRcRights(RC_STATUS | RC_DATA);

NonRepExit:
	PutConn(&c);
    }

    if (code != 0) {
	/* Read/Write Sharing Stat Collection */
	if (flags.discread) {
	    ATOMIC(
		   RVMLIB_REC_OBJECT(vol->current_rws_cnt);
		   vol->current_rws_cnt++;
	    , MAXFP)
	}
	ATOMIC(
	    /* Demote or discard existing status. */
	    if (HAVESTATUS(this) && code != ENOENT)
		Demote();
	    else
		Kill();
	, DMFP)
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
void fsobj::LocalStore(Date_t Mtime, unsigned long NewLength) {
    /* Update local state. */
    {
	FSO_ASSERT(this, !WRITING(this));

	RVMLIB_REC_OBJECT(*this);

	stat.DataVersion++;
	stat.Length = NewLength;
	stat.Date = Mtime;

	UpdateCacheStats((IsDir() ? &FSDB->DirAttrStats : &FSDB->FileAttrStats),
			 WRITE, NBLOCKS(sizeof(fsobj)));
    }
}


int fsobj::ConnectedStore(Date_t Mtime, vuid_t vuid, unsigned long NewLength) {
    FSO_ASSERT(this, HOARDING(this));

    int code = 0;
    char prel_str[256];
    sprintf(prel_str, "store::Store %%s [%d]\n", NBLOCKS(NewLength));

    /* Dummy argument for ACL. */
    RPC2_CountedBS dummybs;
    dummybs.SeqLen = 0;
    RPC2_CountedBS *acl = &dummybs;

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
	sid = vol->GenerateStoreId();
	{
	    /* Make multiple copies of the IN/OUT and OUT parameters. */
	    int ph_ix; unsigned long ph; ph = m->GetPrimaryHost(&ph_ix);
	    vol->PackVS(m->nhosts, &OldVS);

	    /* Shouldn't acl be IN rather than IN/OUT? -JJK */
	    if (acl->SeqLen > VENUS_MAXBSLEN)
		Choke("fsobj::Store: BS len too large (%d)", acl->SeqLen);
	    ARG_MARSHALL_BS(IN_OUT_MODE, RPC2_CountedBS, aclvar, *acl, VSG_MEMBERS, VENUS_MAXBSLEN);
	    ARG_MARSHALL(IN_OUT_MODE, ViceStatus, statusvar, status, VSG_MEMBERS);
	    ARG_MARSHALL(IN_OUT_MODE, SE_Descriptor, sedvar, *sed, VSG_MEMBERS);
	    ARG_MARSHALL(OUT_MODE, RPC2_Integer, VSvar, VS, VSG_MEMBERS);
	    ARG_MARSHALL(OUT_MODE, CallBackStatus, VCBStatusvar, VCBStatus, VSG_MEMBERS);

	    /* Make the RPC call. */
	    CFSOP_PRELUDE(prel_str, comp, fid);
	    MULTI_START_MESSAGE(ViceNewVStore_OP);
	    code = (int) MRPC_MakeMulti(ViceNewVStore_OP, ViceNewVStore_PTR,
				  VSG_MEMBERS, m->rocc.handles,
				  m->rocc.retcodes, m->rocc.MIp, 0, 0,
				  &fid, StoreStatusData, aclvar_ptrs,
				  statusvar_ptrs, NewLength, 0, /* NULL Mask */
				  ph, &sid, &OldVS, VSvar_ptrs, VCBStatusvar_ptrs,
				  &PiggyBS, sedvar_bufs);
	    MULTI_END_MESSAGE(ViceNewVStore_OP);
	    CFSOP_POSTLUDE("store::store done\n");

	    free(OldVS.SeqBody);
	    /* Collate responses from individual servers and decide what to do next. */
	    code = vol->Collate_COP1(m, code, &UpdateSet);
	    MULTI_RECORD_STATS(ViceNewVStore_OP);
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
		if (bytes != status.Length) {
		    print(logFile);
		    Choke("fsobj::Store: bytes mismatch (%d, %d)",
			bytes, status.Length);
		}
	    }
	}

	/* Do Store locally. */
	ATOMIC(
	    LocalStore(Mtime, NewLength);
	    UpdateStatus(&status, &UpdateSet, vuid);
	, CMFP)
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
		ATOMIC(
		    code = vol->LogStore(Mtime, vuid, &fid, NewLength);

		    if (code == 0) {
			LocalStore(Mtime, NewLength);
			vol->flags.transition_pending = 1;
		    }
		, DMFP)
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
	UNI_START_MESSAGE(ViceNewVStore_OP);
	code = (int) ViceNewVStore(c->connid, &fid, StoreStatusData,
				   acl, &status, NewLength, 0, /* Null Mask */
				   0, &Dummy, &OldVS, &VS, &VCBStatus,
				   &PiggyBS, sed);
	UNI_END_MESSAGE(ViceNewVStore_OP);
	CFSOP_POSTLUDE("store::store done\n");

	/* Examine the return code to decide what to do next. */
	code = vol->Collate(c, code);
	UNI_RECORD_STATS(ViceNewVStore_OP);
	if (code != 0) goto NonRepExit;

	{
	    long bytes = sed->Value.SmartFTPD.BytesTransferred;
	    LOG(10, ("ViceStore: stored %d bytes\n", bytes));
	    if (bytes != status.Length) {
		print(logFile);
		Choke("fsobj::Store: bytes mismatch (%d, %d)",
		    bytes, status.Length);
	    }
	}

	/* Do Store locally. */
	ATOMIC(
	    LocalStore(Mtime, NewLength);
	    UpdateStatus(&status, 0, vuid);
	, CMFP)

NonRepExit:
	PutConn(&c);
    }

    return(code);
}

/* local-repair modification */
int fsobj::DisconnectedStore(Date_t Mtime, vuid_t vuid, unsigned long NewLength, int Tid) {
    FSO_ASSERT(this, (EMULATING(this) || LOGGING(this)));

    int code = 0;

    if (!flags.replicated) {
	code = ETIMEDOUT;
	goto Exit;
    }

    ATOMIC(
	/* Failure to log a store would be most unpleasant for the user! */
	/* Probably we should try to guarantee that it never happens (e.g., by reserving a record at open). */
	code = vol->LogStore(Mtime, vuid, &fid, NewLength, Tid);

	if (code == 0)
	    LocalStore(Mtime, NewLength);
    , DMFP)

Exit:
    return(code);
}

/* local-repair modifcation */
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
	ATOMIC(
	    /* Stores cannot be retried, so we have no choice but to nuke the file. */
	    if (code == ERETRY) code = EINVAL;
	    Kill();
	, DMFP)
    }
    return(code);
}


/*  *****  SetAttr/SetAcl  *****  */

/* MUST be called from within transaction! */
void fsobj::LocalSetAttr(Date_t Mtime, unsigned long NewLength,
			  Date_t NewDate, vuid_t NewOwner, unsigned short NewMode) {
    /* Update local state. */
    {
	RVMLIB_REC_OBJECT(*this);

	if (NewLength != (unsigned long)-1) {
	    FSO_ASSERT(this, !WRITING(this));

	    if (HAVEDATA(this)) {
		int delta_blocks = (int) (BLOCKS(this) - NBLOCKS(NewLength));
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
}

/* local-repair modification */
int fsobj::ConnectedSetAttr(Date_t Mtime, vuid_t vuid, unsigned long NewLength,
			     Date_t NewDate, vuid_t NewOwner, unsigned short NewMode,
			     RPC2_CountedBS *acl) {
    FSO_ASSERT(this, HOARDING(this));

    int code = 0;
    int setacl = (acl != 0);
    char *prel_str = setacl ? "store::SetAcl %s\n" : "store::SetAttr %s\n";
    char *post_str = setacl ? "store::setacl done\n" : "store::setattr done\n";

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
	sid = vol->GenerateStoreId();
	{
	    /* Make multiple copies of the IN/OUT and OUT parameters. */
	    int ph_ix; unsigned long ph; ph = m->GetPrimaryHost(&ph_ix);
	    vol->PackVS(m->nhosts, &OldVS);

	    /* Shouldn't acl be IN rather than IN/OUT? -JJK */
	    if (acl->SeqLen > VENUS_MAXBSLEN)
		Choke("fsobj::Store: BS len too large (%d)", acl->SeqLen);
	    ARG_MARSHALL_BS(IN_OUT_MODE, RPC2_CountedBS, aclvar, *acl, VSG_MEMBERS, VENUS_MAXBSLEN);
	    ARG_MARSHALL(IN_OUT_MODE, ViceStatus, statusvar, status, VSG_MEMBERS);
	    ARG_MARSHALL(OUT_MODE, RPC2_Integer, VSvar, VS, VSG_MEMBERS);
	    ARG_MARSHALL(OUT_MODE, CallBackStatus, VCBStatusvar, VCBStatus, VSG_MEMBERS)

	    /* Make the RPC call. */
	    CFSOP_PRELUDE(prel_str, comp, fid);
	    MULTI_START_MESSAGE(ViceNewVStore_OP);
	    code = (int) MRPC_MakeMulti(ViceNewVStore_OP, ViceNewVStore_PTR,
					VSG_MEMBERS, m->rocc.handles,
					m->rocc.retcodes, m->rocc.MIp, 0, 0,
					&fid, (setacl ? StoreNeither : StoreStatus),
					aclvar_ptrs, statusvar_ptrs, 0, Mask,
					ph, &sid, &OldVS, VSvar_ptrs, VCBStatusvar_ptrs, 
					&PiggyBS, 0);
	    MULTI_END_MESSAGE(ViceNewVStore_OP);
	    CFSOP_POSTLUDE(post_str);

	    free(OldVS.SeqBody);
	    /* Collate responses from individual servers and decide what to do next. */
	    code = vol->Collate_COP1(m, code, &UpdateSet);
	    MULTI_RECORD_STATS(ViceNewVStore_OP);
	    if (code == EASYRESOLVE) { asy_resolve = 1; code = 0; }
	    if (code != 0) goto RepExit;

	    /* Collate volume callback information */
	    if (cbtemp == cbbreaks)
		vol->CollateVCB(m, VSvar_bufs, VCBStatusvar_bufs);

	    /* Finalize COP2 Piggybacking. */
	    if (PIGGYCOP2)
		vol->ClearCOP2(&PiggyBS);

	    /* Manually compute the OUT parameters from the mgrpent::SetAttr() call! -JJK */
	    int dh_ix; dh_ix = -1;
	    (void)m->DHCheck(0, ph_ix, &dh_ix);
	    if (setacl)
		ARG_UNMARSHALL_BS(aclvar, *acl, dh_ix);
	    ARG_UNMARSHALL(statusvar, status, dh_ix);
	}

	/* Do setattr locally. */
	ATOMIC(
	    if (!setacl)
		LocalSetAttr(Mtime, NewLength, NewDate, NewOwner, NewMode);
	    UpdateStatus(&status, &UpdateSet, vuid);
	, CMFP)
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
	UNI_START_MESSAGE(ViceNewVStore_OP);
	code = (int) ViceNewVStore(c->connid, &fid, 
				   (setacl ? StoreNeither : StoreStatus),
				   acl, &status, 0, Mask, 0, &Dummy, 
				   &OldVS, &VS, &VCBStatus, &PiggyBS, 0);
	UNI_END_MESSAGE(ViceNewVStore_OP);
	CFSOP_POSTLUDE("store::setattr done\n");

	/* Examine the return code to decide what to do next. */
	code = vol->Collate(c, code);
	UNI_RECORD_STATS(ViceNewVStore_OP);
	if (code != 0) goto NonRepExit;

	/* Do setattr locally. */
	ATOMIC(
	    if (!setacl)
		LocalSetAttr(Mtime, NewLength, NewDate, NewOwner, NewMode);
	    UpdateStatus(&status, 0, vuid);
	, CMFP)

NonRepExit:
	PutConn(&c);
    }

    return(code);
}

/* local-repair modification */
int fsobj::DisconnectedSetAttr(Date_t Mtime, vuid_t vuid, unsigned long NewLength, Date_t NewDate, 
			       vuid_t NewOwner, unsigned short NewMode, int Tid) {
    FSO_ASSERT(this, (EMULATING(this) || LOGGING(this)));

    int code = 0;

    ATOMIC(
	RPC2_Integer tNewMode =	(short)NewMode;	    /* sign-extend!!! */
	code = vol->LogSetAttr(Mtime, vuid, &fid, NewLength, NewDate, NewOwner, 
			       (RPC2_Unsigned)tNewMode, Tid);
	if (code == 0)
	    LocalSetAttr(Mtime, NewLength, NewDate, NewOwner, NewMode);
    , DMFP)

    return(code);
}

/* local-repair modification */
int fsobj::SetAttr(struct vattr *vap, vuid_t vuid, RPC2_CountedBS *acl) {
    LOG(10, ("fsobj::SetAttr: (%s), uid = %d\n",
	      comp, vuid));

    if (LogLevel >= 1000) {
	dprint("\tmode = %#o, uid = %d, gid = %d, fsid = %d, rdev = %d\n",
	       vap->va_mode, vap->va_uid, vap->va_gid,
	       vap->va_fsid, vap->va_rdev);
	dprint("\tid = %d, nlink = %d, size = %d, blocksize = %d, storage = %d\n",
	       VA_ID(vap), vap->va_nlink, vap->va_size,
	       vap->va_blocksize, VA_STORAGE(vap));
	dprint("\tatime = <%d, %d>, mtime = <%d, %d>, ctime = <%d, %d>\n",
	       VA_ATIME_1(vap), VA_ATIME_2(vap), 
	       VA_MTIME_1(vap), VA_MTIME_2(vap), 
	       VA_CTIME_1(vap), VA_CTIME_2(vap));
    }

    unsigned long NewLength = (vap->va_size != (u_long)-1 && vap->va_size < stat.Length)
      ? vap->va_size : (unsigned long)-1;
    Date_t NewDate = (VA_MTIME_1(vap) != (long)-1 && VA_MTIME_1(vap) != stat.Date)
      ? VA_MTIME_1(vap) : (Date_t)-1;
    vuid_t NewOwner = (vap->va_uid != (short)-1 && vap->va_uid != stat.Owner)
      ? vap->va_uid : (vuid_t)-1;
    unsigned short NewMode = (vap->va_mode != (u_short)-1 && (vap->va_mode & 0777) != stat.Mode)
      ? (vap->va_mode & 0777) : (unsigned short)-1;

    /* Only update cache file when truncating and open for write! */
    if (NewLength != (unsigned long)-1 && WRITING(this)) {
	ATOMIC(
	    data.file->Truncate((unsigned) NewLength);
	, MAXFP)
	NewLength = (unsigned long)-1;
    }

    /* Avoid performing action where possible. */
    if (NewLength == (unsigned long)-1 && NewDate == (Date_t)-1 &&
	 NewOwner == (vuid_t)-1 && NewMode == (unsigned short)-1) {
	if (acl == 0) return(0);
    }
    else {
	FSO_ASSERT(this, acl == 0);
    }

    /* Cannot chown a file until the first store has been done! */
    if (NewOwner != (vuid_t)-1 && IsVirgin()) {
	return(EINVAL);
    }

    int code = 0;
    Date_t Mtime = Vtime();

    int conn, tid;
    GetOperationState(&conn, &tid);

    if (conn == 0) {
	code = DisconnectedSetAttr(Mtime, vuid, NewLength, NewDate, NewOwner, NewMode, tid);
    }
    else {
	code = ConnectedSetAttr(Mtime, vuid, NewLength, NewDate, NewOwner, NewMode, acl);
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

    struct vattr va;
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
	target_fso->flags.created = 1;
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
	sid = vol->GenerateStoreId();
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
	ATOMIC(
	    LocalCreate(Mtime, target_fso, name, vuid, Mode);
	    UpdateStatus(&parent_status, &UpdateSet, vuid);
	    target_fso->UpdateStatus(&target_status, &UpdateSet, vuid);
	, CMFP)
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
	ATOMIC(
	    LocalCreate(Mtime, target_fso, name, vuid, Mode);
	    UpdateStatus(&parent_status, 0, vuid);
	    target_fso->UpdateStatus(&target_status, 0, vuid);
	, CMFP)
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
	    ATOMIC(
		target_fso->Kill();
	    , DMFP);
	    FSDB->Put(&target_fso);
	}
    }
    return(code);
}

/* local-repair modification */
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

    ATOMIC(
	code = vol->LogCreate(Mtime, vuid, &fid, name, &target_fso->fid, Mode, Tid);

	   if (code == 0) {
	    /* This MUST update second-class state! */
	    LocalCreate(Mtime, target_fso, name, vuid, Mode);

	    /* target_fso->stat is not initialized until LocalCreate */
	    RVMLIB_REC_OBJECT(target_fso->CleanStat);
	    target_fso->CleanStat.Length = target_fso->stat.Length;
	    target_fso->CleanStat.Date = target_fso->stat.Date;
	   }
    , DMFP)

Exit:
    if (code == 0) {
	*t_fso_addr = target_fso;
    }
    else {
	if (target_fso != 0) {
	    FSO_ASSERT(target_fso, !HAVESTATUS(target_fso));
	    ATOMIC(
		target_fso->Kill();
	    , DMFP);
	    FSDB->Put(&target_fso);
	}
    }
    return(code);
}


/* local-repair modification */
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
