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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/vice/srvproc.cc,v 4.5 1997/04/28 16:05:59 braam Exp $";
#endif /*_BLURB_*/


/*

                         IBM COPYRIGHT NOTICE

                          Copyright (C) 1986
             International Business Machines Corporation
                         All Rights Reserved

This  file  contains  some  code identical to or derived from the 1986
version of the Andrew File System ("AFS"), which is owned by  the  IBM
Corporation.    This  code is provded "AS IS" and IBM does not warrant
that it is free of infringement of  any  intellectual  rights  of  any
third  party.    IBM  disclaims  liability of any kind for any damages
whatsoever resulting directly or indirectly from use of this  software
or  of  any  derivative work.  Carnegie Mellon University has obtained
permission to distribute this code, which is based on Version 2 of AFS
and  does  not  contain the features and enhancements that are part of
Version 3 of AFS.  Version 3 of  AFS  is  commercially  available  and
supported by Transarc Corporation, Pittsburgh, PA.

*/


/************************************************************************/
/*									*/
/*  fileproc.c	- File Server request routines				*/
/*									*/
/*  Function	- A set of routines to handle the various File Server	*/
/*		  requests.  These routines are invoked via RP2GEN stubs*/
/*									*/
/************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <sysent.h>
#include <strings.h>
#include <inodeops.h>

#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif

#ifdef _TIMECALLS_
#include "histo.h"
#endif _TIMECALLS_

#ifdef __cplusplus
}
#endif __cplusplus


#include <util.h>
#include <rvmlib.h>
#include <vmindex.h>
#include <coda_dir.h>
#include <voltypes.h>
#include <partition.h>
#include <vicelock.h>
#include <srv.h>
#include <callback.h>
#include <vlist.h>
#include <vrdb.h>
#include <repio.h>
#include <rvmdir.h>
#include <operations.h>
#include <reslog.h>
#include <lockqueue.h>
#include <resutil.h>
#include <ops.h>
#include <rsle.h>
#include <nettohost.h>
#include <cvnode.h>
#include "coppend.h"

#ifdef _TIMECALLS_
#include "timecalls.h"
#endif _TIMECALLS_

/* From Vol package. */
extern void SetDirHandle(DirHandle *, Vnode *);
extern void VCheckDiskUsage(Error *, Volume *, int );

extern void MakeLogNonEmpty(Vnode *);
extern void GetMaxVV(ViceVersionVector *, ViceVersionVector **, int);

extern int CheckReadMode(ClientEntry *, Vnode *);
extern int CheckWriteMode(ClientEntry *, Vnode *);
extern void CopyOnWrite(Vnode *, Volume *);
extern int AdjustDiskUsage(Volume *, int);
extern int CheckDiskUsage(Volume *, int);
extern void ChangeDiskUsage(Volume *, int);
extern void HandleWeakEquality(Volume *, Vnode *, ViceVersionVector *);

/* These should be RPC routines. */
extern long ViceGetAttr(RPC2_Handle, ViceFid *, int,
			 ViceStatus *, RPC2_Unsigned, RPC2_CountedBS *);
extern long ViceGetACL(RPC2_Handle, ViceFid *, int, RPC2_BoundedBS *,
			ViceStatus *, RPC2_Unsigned, RPC2_CountedBS *);
extern long ViceSetACL(RPC2_Handle, ViceFid *, RPC2_CountedBS *, ViceStatus *,
			RPC2_Unsigned, ViceStoreId *, RPC2_CountedBS *,
			RPC2_Integer *, CallBackStatus *, RPC2_CountedBS *);
extern long ViceNewSetAttr(RPC2_Handle, ViceFid *, ViceStatus *, RPC2_Integer,
			   RPC2_Unsigned, ViceStoreId *, RPC2_CountedBS *, 
			   RPC2_Integer *, CallBackStatus *, RPC2_CountedBS *);

/* *****  Private routines  ***** */

PRIVATE int GrabFsObj(ViceFid *, Volume **, Vnode **, int, int, int);
PRIVATE int NormalVCmp(int, VnodeType, void *, void *);
PRIVATE int StoreVCmp(int, VnodeType, void *, void *);
PRIVATE int Check_CLMS_Semantics(ClientEntry *, Vnode **, Vnode **, char *, Volume **, VnodeType,
				  int, VCP, ViceStatus *, ViceStatus *, Rights *, Rights *, int);
PRIVATE int Check_RR_Semantics(ClientEntry *, Vnode **, Vnode **, char *, Volume **, VnodeType,
				int, VCP, ViceStatus *, ViceStatus *, Rights *, Rights *, int);
PRIVATE void Perform_CLMS(ClientEntry *, VolumeId, Volume *, Vnode *, Vnode *, int,
			   char *, Inode, RPC2_Unsigned, Date_t, RPC2_Unsigned,
			   int, ViceStoreId *, DirInode **, int *, RPC2_Integer *);
PRIVATE void Perform_RR(ClientEntry *, VolumeId, Volume *, Vnode *, Vnode *, char *,
			Date_t, int, ViceStoreId *, DirInode **, int *, RPC2_Integer *);

/* Yield parameters (i.e., after how many loop iterations do I poll and yield). */
/* N.B.  Yield "periods" MUST all be power of two so that AND'ing can be used! */
const int Yield_PutObjects_Period = 8;
const int Yield_PutObjects_Mask = Yield_PutObjects_Period - 1;
const int Yield_PutInodes_Period = 16;
const int Yield_PutInodes_Mask = (Yield_PutInodes_Period - 1);
extern void PollAndYield();

/*
 ***************************************************
 *
 *    The structure of each server operation is the following:
 *
 *        1. ValidateParameters
 *        2. GetObjects
 *        3. CheckSemantics
 *           - ConcurrencyControl
 *           - IntegrityConstraints
 *           - Permissions
 *        4. PerformOperation
 *           - BulkTransfer
 *           - UpdateObjects
 *           - SetOutParameters
 *        5. PutObjects
 *
 ***************************************************
 */


/*
  BEGIN_HTML
  <a name="ViceFetch"><strong>Fetch a file or directory</strong></a> 
  END_HTML
*/
long ViceFetch(RPC2_Handle RPCid, ViceFid *Fid, ViceFid *BidFid,
		ViceFetchType Request, RPC2_BoundedBS *AccessList,
		ViceStatus *Status, RPC2_Unsigned PrimaryHost,
		RPC2_CountedBS *PiggyBS, SE_Descriptor *BD)
{
    /* We should have separate RPC routines for these two! */
    if (Request == FetchNoData || Request == FetchNoDataRepair) {
	int inconok = (Request == FetchNoDataRepair);
	if (AccessList->MaxSeqLen == 0)
	    return(ViceGetAttr(RPCid, Fid, inconok, Status, PrimaryHost, PiggyBS));
	else
	    return(ViceGetACL(RPCid, Fid, inconok, AccessList, Status, PrimaryHost, PiggyBS));
    }

    int errorCode = 0;		/* return code to caller */
    Error fileCode = 0;		/* return code to do writes */
    Volume *volptr = 0;		/* pointer to the volume */
    ClientEntry *client = 0;	/* pointer to the client data */
    Rights rights = 0;		/* rights for this user */
    Rights anyrights = 0;	/* rights for any user */
    int inconok = 0;		/* flag to say whether Coda inconsistency is ok */
    VolumeId VSGVolnum = Fid->Volume;
    int ReplicatedOp = (PrimaryHost != 0);
    dlist *vlist = new dlist((CFN)VLECmp);
    vle *v;
    vle *av;

START_TIMING(Fetch_Total);
    LogMsg(1, SrvDebugLevel, stdout, "ViceFetch: Fid = (%x.%x.%x), Repair = %d",
	     Fid->Volume, Fid->Vnode, Fid->Unique, (Request == FetchDataRepair));

    /* Validate parameters. */
    {
	if (errorCode = ValidateParms(RPCid, &client, ReplicatedOp, &Fid->Volume, PiggyBS))
	    goto FreeLocks;

	/* Request type. */
	switch(Request) {
	    case FetchNoData:
	    case FetchData:
		break;

	    case FetchNoDataRepair:
	    case FetchDataRepair:
		inconok = 1;
		break;

	    default:
		LogMsg(0, SrvDebugLevel, stdout, "ViceFetch: illegal type %d", Request);
		errorCode = EINVAL;
		goto FreeLocks;
	}

	/* Deprecated/Inapplicable parameters. */
	if (!FID_EQ(NullFid, *BidFid)) {
	    LogMsg(0, SrvDebugLevel, stdout, "ViceFetch: non-Null BidFid (%x.%x.%x)",
		    BidFid->Volume, BidFid->Vnode, BidFid->Unique);
	    errorCode = EINVAL;
	    goto FreeLocks;
	}
	if (AccessList->MaxSeqLen != 0) {
	    LogMsg(0, SrvDebugLevel, stdout, "ViceFetch: ACL != 0");
	    errorCode = EINVAL;
	    goto FreeLocks;
	}
    }

    /* Get objects. */
    {
	v = AddVLE(*vlist, Fid);
	if (errorCode = GetFsObj(Fid, &volptr, &v->vptr, READ_LOCK, NO_LOCK, inconok, 0))
	    goto FreeLocks;

	/* This may violate locking protocol! -JJK */
	if (v->vptr->disk.type == vDirectory) {
	    av = v;
	}
	else {
	    ViceFid pFid;
	    pFid.Volume = Fid->Volume;
	    pFid.Vnode = v->vptr->disk.vparent;
	    pFid.Unique = v->vptr->disk.uparent;
	    av = AddVLE(*vlist, &pFid);
	    if (errorCode = GetFsObj(&pFid, &volptr, &av->vptr, READ_LOCK, NO_LOCK, inconok, 0))
		goto FreeLocks;
	}
    }

    /* Check semantics. */
    {
	if (errorCode = CheckFetchSemantics(client, &av->vptr, &v->vptr,
					    &volptr, &rights, &anyrights))
	    goto FreeLocks;
    }

    /* Perform operation. */
    {
	if (!ReplicatedOp || PrimaryHost == ThisHostAddr)
	    if (errorCode = FetchBulkTransfer(RPCid, client, volptr, v->vptr))
		goto FreeLocks;
	PerformFetch(client, volptr, v->vptr);

	SetStatus(v->vptr, Status, rights, anyrights);

	if(VolumeWriteable(volptr))
	    /* Until CVVV probes? -JJK */
	    if (1/*!ReplicatedOp || PrimaryHost == ThisHostAddr*/) 
		Status->CallBack = CodaAddCallBack(client->VenusId, Fid, VSGVolnum);
    }

FreeLocks:
    /* Put objects. */
    {
	PutObjects(errorCode, volptr, NO_LOCK, vlist, 0, 0);
    }

    LogMsg(2, SrvDebugLevel, stdout, "ViceFetch returns %s", ViceErrorMsg(errorCode));
END_TIMING(Fetch_Total);
    return(errorCode);
}


/*
  BEGIN_HTML
  <a name="ViceGetAttr"><strong>Fetch the attributes for a file/directory</strong></a> 
  END_HTML
*/
long ViceGetAttr(RPC2_Handle RPCid, ViceFid *Fid, int InconOK,
		  ViceStatus *Status, RPC2_Unsigned PrimaryHost, RPC2_CountedBS *PiggyBS)
{
    int errorCode = 0;		/* return code to caller */
    Error fileCode = 0;		/* return code to do writes */
    Volume *volptr = 0;		/* pointer to the volume */
    ClientEntry *client = 0;	/* pointer to the client data */
    Rights rights = 0;		/* rights for this user */
    Rights anyrights = 0;	/* rights for any user */
    VolumeId VSGVolnum = Fid->Volume;
    int ReplicatedOp = (PrimaryHost != 0);
    dlist *vlist = new dlist((CFN)VLECmp);
    vle *v = 0;
    vle *av = 0;

START_TIMING(GetAttr_Total);
    LogMsg(1, SrvDebugLevel, stdout, "ViceGetAttr: Fid = (%x.%x.%x), Repair = %d",
	     Fid->Volume, Fid->Vnode, Fid->Unique, InconOK);

    /* Validate parameters. */
    {
	if (errorCode = ValidateParms(RPCid, &client, ReplicatedOp, &Fid->Volume, PiggyBS))
	    goto FreeLocks;
    }

    /* Get objects. */
    {
	v = AddVLE(*vlist, Fid);
	if (errorCode = GetFsObj(Fid, &volptr, &v->vptr, READ_LOCK, NO_LOCK, InconOK, 0))
	    goto FreeLocks;

	/* This may violate locking protocol! -JJK */
	if (v->vptr->disk.type == vDirectory) {
	    av = v;
	}
	else {
	    ViceFid pFid;
	    pFid.Volume = Fid->Volume;
	    pFid.Vnode = v->vptr->disk.vparent;
	    pFid.Unique = v->vptr->disk.uparent;
	    av = AddVLE(*vlist, &pFid);
	    if (errorCode = GetFsObj(&pFid, &volptr, &av->vptr, READ_LOCK, NO_LOCK, InconOK, 0))
		goto FreeLocks;
	}
    }

    /* Check semantics. */
    {
	if (errorCode = CheckGetAttrSemantics(client, &av->vptr, &v->vptr,
					      &volptr, &rights, &anyrights))
	    goto FreeLocks;
    }

    /* Perform operation. */
    {
	PerformGetAttr(client, volptr, v->vptr);

	SetStatus(v->vptr, Status, rights, anyrights);

	if(VolumeWriteable(volptr))
	    /* Until CVVV probes? -JJK */
	    if (1/*!ReplicatedOp || PrimaryHost == ThisHostAddr*/) 
		Status->CallBack = CodaAddCallBack(client->VenusId, Fid, VSGVolnum);
    }

FreeLocks:
    /* Put objects. */
    {
	PutObjects(errorCode, volptr, NO_LOCK, vlist, 0, 0);
    }

    LogMsg(2, SrvDebugLevel, stdout, "ViceGetAttr returns %s", ViceErrorMsg(errorCode));
END_TIMING(GetAttr_Total);
    return(errorCode);
}


/* 
 * assumes fids are given in order. a return of 1 in flags means that the 
 * client status is valid for that object, and that callback is set. 
 */
/*
  BEGIN_HTML
  <a name="ViceValidateAttrs"><strong>A batched version of <tt>GetAttr</tt></strong></a> 
  END_HTML
*/
long ViceValidateAttrs(RPC2_Handle RPCid, RPC2_Unsigned PrimaryHost,
		       ViceFid *PrimaryFid, ViceStatus *Status, 
		       RPC2_Integer NumPiggyFids, ViceFidAndVV Piggies[],
		       RPC2_CountedBS *VFlagBS, RPC2_CountedBS *PiggyBS)
{
    long errorCode = 0;		/* return code to caller */
    Volume *volptr = 0;		/* pointer to the volume */
    ClientEntry *client = 0;	/* pointer to the client data */
    Rights rights = 0;		/* rights for this user */
    Rights anyrights = 0;	/* rights for any user */
    int ReplicatedOp = (PrimaryHost != 0);
    dlist *vlist = new dlist((CFN)VLECmp);
    vle *v = 0;
    vle *av = 0;
    int iErrorCode = 0;

START_TIMING(ViceValidateAttrs_Total);
    LogMsg(1, SrvDebugLevel, stdout, "ViceValidateAttrs: Fid = (%x.%x.%x), %d piggy fids",
	   PrimaryFid->Volume, PrimaryFid->Vnode, PrimaryFid->Unique, NumPiggyFids);

    /* Do a real getattr for primary fid. */
    {
	if (errorCode = ViceGetAttr(RPCid, PrimaryFid, 0, Status, PrimaryHost, PiggyBS))
		goto Exit;
    }

    bzero((char *) VFlagBS->SeqBody, (int) NumPiggyFids);

    /* now check piggyback fids */
    for (VFlagBS->SeqLen = 0; VFlagBS->SeqLen < NumPiggyFids; VFlagBS->SeqLen++) {

	/* save the replicated volume ID for the AddCallBack */    
	VolumeId VSGVolnum = Piggies[VFlagBS->SeqLen].Fid.Volume;

	/* Validate parameters. */
        {
	    /* We've already dealt with the PiggyBS in the GetAttr above. */
	    if (iErrorCode = ValidateParms(RPCid, &client, ReplicatedOp, 
					   &Piggies[VFlagBS->SeqLen].Fid.Volume, NULL))
		goto InvalidObj;
        }

	/* Get objects. */
	{
	    v = AddVLE(*vlist, &Piggies[VFlagBS->SeqLen].Fid);
	    if (iErrorCode = GetFsObj(&Piggies[VFlagBS->SeqLen].Fid, &volptr, 
				      &v->vptr, READ_LOCK, NO_LOCK, 0, 0))
		goto InvalidObj;

	    /* This may violate locking protocol! -JJK */
	    if (v->vptr->disk.type == vDirectory) {
		av = v;
	    } else {
		ViceFid pFid;
		pFid.Volume = Piggies[VFlagBS->SeqLen].Fid.Volume;
		pFid.Vnode = v->vptr->disk.vparent;
		pFid.Unique = v->vptr->disk.uparent;
		av = AddVLE(*vlist, &pFid);
		if (iErrorCode = GetFsObj(&pFid, &volptr, &av->vptr, 
					  READ_LOCK, NO_LOCK, 0, 0))
		    goto InvalidObj;

	    }
        }

	/* Check semantics. */
	{
	    if (iErrorCode = CheckGetAttrSemantics(client, &av->vptr, &v->vptr,
						  &volptr, &rights, &anyrights))
		goto InvalidObj;
	}

	/* Do it. */
	{
	    if (VV_Cmp(&Piggies[VFlagBS->SeqLen].VV, &v->vptr->disk.versionvector) == VV_EQ) {
		    /* this is a writeable volume, o.w. we wouldn't be in this call */
		    /* Until CVVV probes? -JJK */
		    if (1/*!ReplicatedOp || PrimaryHost == ThisHostAddr*/) {
			    /* 
			     * we really should differentiate between 
			     * valid with no callback and invalid. that
			     * doesn't matter too much with this call, 
			     * because getting a callback is refetching.
			     */
			    VFlagBS->SeqBody[VFlagBS->SeqLen] = (RPC2_Byte)
				    CodaAddCallBack(client->VenusId, 
						    &Piggies[VFlagBS->SeqLen].Fid, 
						    VSGVolnum);
		    }

		    LogMsg(8, SrvDebugLevel, stdout, "ViceValidateAttrs: (%x.%x.%x) ok",
			   Piggies[VFlagBS->SeqLen].Fid.Volume, 
			   Piggies[VFlagBS->SeqLen].Fid.Vnode, 
			   Piggies[VFlagBS->SeqLen].Fid.Unique);
		    continue;
	    }

InvalidObj:
	    LogMsg(0, SrvDebugLevel, stdout, "ViceValidateAttrs: (%x.%x.%x) failed!",
		   Piggies[VFlagBS->SeqLen].Fid.Volume, Piggies[VFlagBS->SeqLen].Fid.Vnode, 
		   Piggies[VFlagBS->SeqLen].Fid.Unique);
	}
    }

    /* Put objects. */
    {
	PutObjects(iErrorCode, volptr, NO_LOCK, vlist, 0, 0);
    }

Exit:
    LogMsg(2, SrvDebugLevel, stdout, "ViceValidateAttrs returns %s, %d piggy fids checked", 
	   ViceErrorMsg((int)errorCode), VFlagBS->SeqLen);
END_TIMING(ViceValidateAttrs_Total);
    return(errorCode);
}

/*
  BEGIN_HTML
  <a name="ViceGetACL"><strong>Fetch the acl of a directory</strong></a> 
  END_HTML
*/
long ViceGetACL(RPC2_Handle RPCid, ViceFid *Fid, int InconOK, RPC2_BoundedBS *AccessList,
		 ViceStatus *Status, RPC2_Unsigned PrimaryHost, RPC2_CountedBS *PiggyBS)
{
    int errorCode = 0;		/* return code to caller */
    Error fileCode = 0;		/* return code to do writes */
    Volume *volptr = 0;		/* pointer to the volume */
    ClientEntry *client = 0;	/* pointer to the client data */
    Rights rights = 0;		/* rights for this user */
    Rights anyrights = 0;	/* rights for any user */
    RPC2_String eACL = 0;
    VolumeId VSGVolnum = Fid->Volume;
    int ReplicatedOp = (PrimaryHost != 0);
    dlist *vlist = new dlist((CFN)VLECmp);
    vle *v = 0;

START_TIMING(GetACL_Total);
    LogMsg(1, SrvDebugLevel, stdout, "ViceGetACL: Fid = (%x.%x.%x), Repair = %d",
	     Fid->Volume, Fid->Vnode, Fid->Unique, InconOK);

    /* Validate parameters. */
    {
	if (errorCode = ValidateParms(RPCid, &client, ReplicatedOp, &Fid->Volume, PiggyBS))
	    goto FreeLocks;
    }

    /* Get objects. */
    {
	v = AddVLE(*vlist, Fid);
	if (errorCode = GetFsObj(Fid, &volptr, &v->vptr, READ_LOCK, NO_LOCK, InconOK, 0))
	    goto FreeLocks;
    }

    /* Check semantics. */
    {
	if (errorCode = CheckGetACLSemantics(client, &v->vptr, &volptr, &rights,
					     &anyrights, AccessList, &eACL))
	    goto FreeLocks;
    }

    /* Perform operation. */
    {
	PerformGetACL(client, volptr, v->vptr, AccessList, eACL);

	SetStatus(v->vptr, Status, rights, anyrights);
    }

FreeLocks:
    /* Put objects. */
    {
	if (eACL) AL_FreeExternalAlist((AL_ExternalAccessList *)&eACL);

	PutObjects(errorCode, volptr, NO_LOCK, vlist, 0, 0);
    }

    LogMsg(2, SrvDebugLevel, stdout, "ViceGetACL returns %s", ViceErrorMsg(errorCode));
END_TIMING(GetACL_Total);
    return(errorCode);
}


/*
  BEGIN_HTML
  <a name="ViceNewVStore"><strong>Store a file or directory</strong></a> 
  END_HTML
*/
long ViceNewVStore(RPC2_Handle RPCid, ViceFid *Fid, ViceStoreType Request,
		   RPC2_CountedBS *AccessList, ViceStatus *Status,
		   RPC2_Integer Length, RPC2_Integer Mask,
		   RPC2_Unsigned PrimaryHost,
		   ViceStoreId *StoreId, RPC2_CountedBS *OldVS, 
		   RPC2_Integer *NewVS, CallBackStatus *VCBStatus,
		   RPC2_CountedBS *PiggyBS, SE_Descriptor *BD)
{
    /* We should have separate RPC routines for these two! */
    if (Request == StoreStatus)
	return(ViceNewSetAttr(RPCid, Fid, Status, Mask, PrimaryHost, StoreId, 
			      OldVS, NewVS, VCBStatus, PiggyBS));
    if (Request == StoreNeither)
	return(ViceSetACL(RPCid, Fid, AccessList, Status, PrimaryHost, 
			  StoreId, OldVS, NewVS, VCBStatus, PiggyBS));

    int errorCode = 0;		/* return code for caller */
    Error fileCode = 0;		/* return code for writes */
    Volume *volptr = 0;		/* pointer to the volume header */
    ClientEntry *client = 0;	/* pointer to client structure */
    Rights rights = 0;		/* rights for this user */
    Rights anyrights = 0;	/* rights for any user */
    int deltablocks = 0;
    VolumeId VSGVolnum = Fid->Volume;
    int	ReplicatedOp = (PrimaryHost != 0);
    dlist *vlist = new dlist((CFN)VLECmp);
    vle *v = 0;
    vle *av = 0;

START_TIMING(Store_Total);
    LogMsg(1, SrvDebugLevel, stdout, "ViceNewVStore: Fid = (%x.%x.%x)",
	     Fid->Volume, Fid->Vnode, Fid->Unique);

    /* Validate parameters. */
    {
	if (errorCode = ValidateParms(RPCid, &client, ReplicatedOp, &Fid->Volume, PiggyBS))
	    goto FreeLocks;

	/* Request type. */
	switch(Request) {
	    case StoreStatus:
	    case StoreNeither:
		assert(FALSE);

	    case StoreData:
	    case StoreStatusData:
		break;

	    default:
		LogMsg(0, SrvDebugLevel, stdout, "ViceNewVStore: illegal type %d", Request);
		errorCode = EINVAL;
		goto FreeLocks;
	}

	/* Deprecated/Inapplicable parameters. */
	if (AccessList->SeqLen != 0) {
	    LogMsg(0, SrvDebugLevel, stdout, "ViceNewVStore: !StoreNeither && SeqLen != 0");
	    errorCode = EINVAL;
	    goto FreeLocks;
	}
    }

    /* Get objects. */
    {
	v = AddVLE(*vlist, Fid);
	if (errorCode = GetFsObj(Fid, &volptr, &v->vptr, WRITE_LOCK, SHARED_LOCK, 0, 0))
	    goto FreeLocks;

	/* This may violate locking protocol! -JJK */
	ViceFid pFid;
	pFid.Volume = Fid->Volume;
	pFid.Vnode = v->vptr->disk.vparent;
	pFid.Unique = v->vptr->disk.uparent;
	av = AddVLE(*vlist, &pFid);
	if (errorCode = GetFsObj(&pFid, &volptr, &av->vptr, READ_LOCK, NO_LOCK, 0, 0))
	    goto FreeLocks;
    }

    /* Check semantics. */
    {
	if (errorCode = CheckStoreSemantics(client, &av->vptr, &v->vptr, &volptr,
					    ReplicatedOp, StoreVCmp, &Status->VV,
					    Status->DataVersion, &rights, &anyrights))
	    goto FreeLocks;
    }

    /* Perform operation. */
    {
	int tblocks = (int) (nBlocks(Length) - nBlocks(v->vptr->disk.length));
	if (errorCode = AdjustDiskUsage(volptr, tblocks))
	    goto FreeLocks;
	deltablocks = tblocks;

	extern int OptimizeStore;
	if (OptimizeStore) {
	    //This is a signal to Venus to go ahead and let the close complete 
	    ViceFid VSGFid;
	    VSGFid.Volume = VSGVolnum;
	    VSGFid.Vnode = Fid->Vnode;
	    VSGFid.Unique = Fid->Unique;
	    (void)CallBackReceivedStore(client->VenusId->id, &VSGFid);
	}
	
	v->f_finode = icreate((int) V_device(volptr), 0, (int) V_id(volptr), 
			      (int) v->vptr->vnodeNumber, (int) v->vptr->disk.uniquifier, 
			      (int) v->vptr->disk.dataVersion + 1);
#if   defined(__linux__) 
	assert(v->f_finode > (unsigned long) 0);
#else
	assert(v->f_finode > 0);
#endif /* __linux__ */
	if (errorCode = StoreBulkTransfer(RPCid, client, volptr, v->vptr, v->f_finode, Length))
	    goto FreeLocks;
	v->f_sinode = v->vptr->disk.inodeNumber;
	if (ReplicatedOp) {
	    HandleWeakEquality(volptr, v->vptr, &Status->VV);
	    GetMyVS(volptr, OldVS, NewVS);
	}
	PerformStore(client, VSGVolnum, volptr, v->vptr, v->f_finode,
		     ReplicatedOp, Length, Status->Date, StoreId, NewVS);

	SetStatus(v->vptr, Status, rights, anyrights);
	if (ReplicatedOp)
	    SetVSStatus(client, volptr, NewVS, VCBStatus);
    }

FreeLocks:
    /* Put objects. */
    {
	if (errorCode) {
	    cpent *cpe = CopPendingMan->findanddeq(StoreId);
	    if (cpe) {
		CopPendingMan->remove(cpe);
		delete cpe;
	    }
	}

	PutObjects(errorCode, volptr, SHARED_LOCK, vlist, deltablocks, 1);
    }

    LogMsg(2, SrvDebugLevel, stdout, "ViceNewVStore returns %s", ViceErrorMsg(errorCode));
END_TIMING(Store_Total);
    return(errorCode);
}


/*
  BEGIN_HTML
  <a name="ViceNewStore"><strong>Obsoleted by ViceNewVStore</strong></a> 
  <a href="#ViceNewVStore"><strong>ViceNewVStore</strong></a>
  END_HTML
*/
long ViceNewStore(RPC2_Handle RPCid, ViceFid *Fid, ViceStoreType Request,
		  RPC2_CountedBS *AccessList, ViceStatus *Status,
		  RPC2_Integer Length, RPC2_Integer Mask,
		  RPC2_Unsigned PrimaryHost,
		  ViceStoreId *StoreId, RPC2_CountedBS *PiggyBS,
		  SE_Descriptor *BD)
{
	CallBackStatus VCBStatus = NoCallBack;
	RPC2_Integer NewVS = 0;
	RPC2_CountedBS OldVS;
	OldVS.SeqLen = 0;
	OldVS.SeqBody = 0;

	return(ViceNewVStore(RPCid, Fid, Request, AccessList, Status,
			     Length, Mask, PrimaryHost, StoreId, &OldVS, &NewVS,
			     &VCBStatus, PiggyBS, BD));
}


/*
  BEGIN_HTML
  <a name="ViceNewSetAttr"><strong>Set attributes of an object?</strong></a> 
  END_HTML
*/
long ViceNewSetAttr(RPC2_Handle RPCid, ViceFid *Fid, ViceStatus *Status,
		    RPC2_Integer Mask, RPC2_Unsigned PrimaryHost,		    
		    ViceStoreId *StoreId, 
		    RPC2_CountedBS *OldVS, RPC2_Integer *NewVS,
		    CallBackStatus *VCBStatus, RPC2_CountedBS *PiggyBS)
{
    int errorCode = 0;		/* return code for caller */
    Error fileCode = 0;		/* return code for writes */
    Volume *volptr = 0;		/* pointer to the volume header */
    ClientEntry *client = 0;	/* pointer to client structure */
    Rights rights = 0;		/* rights for this user */
    Rights anyrights = 0;	/* rights for any user */
    int deltablocks = 0;
    VolumeId VSGVolnum = Fid->Volume;
    int	ReplicatedOp = (PrimaryHost != 0);
    dlist *vlist = new dlist((CFN)VLECmp);
    vle *v = 0;
    vle *av = 0;

START_TIMING(SetAttr_Total);
    LogMsg(1, SrvDebugLevel, stdout, "ViceNewSetAttr: Fid = (%x.%x.%x)",
	     Fid->Volume, Fid->Vnode, Fid->Unique);

    /* Validate parameters. */
    {
	if (errorCode = ValidateParms(RPCid, &client, ReplicatedOp, &Fid->Volume, PiggyBS))
	    goto FreeLocks;
    }

    /* Get objects. */
    {
	v = AddVLE(*vlist, Fid);
	if (errorCode = GetFsObj(Fid, &volptr, &v->vptr, WRITE_LOCK, SHARED_LOCK, 0, 0))
	    goto FreeLocks;

	/* This may violate locking protocol! -JJK */
	if (v->vptr->disk.type == vDirectory) {
	    av = v;
	}
	else {
	    ViceFid pFid;
	    pFid.Volume = Fid->Volume;
	    pFid.Vnode = v->vptr->disk.vparent;
	    pFid.Unique = v->vptr->disk.uparent;
	    av = AddVLE(*vlist, &pFid);
	    if (errorCode = GetFsObj(&pFid, &volptr, &av->vptr, READ_LOCK, NO_LOCK, 0, 0))
		goto FreeLocks;
	}
    }

    /* Check semantics. */
    {
	if (errorCode = CheckNewSetAttrSemantics(client, &av->vptr, &v->vptr, &volptr,
						 ReplicatedOp, NormalVCmp, Status->Length,
						 Status->Date, Status->Owner, Status->Mode,
						 Mask, &Status->VV, Status->DataVersion,
						 &rights, &anyrights))
	    goto FreeLocks;
    }

    /* Make sure resolution log is not empty */
    if (AllowResolution && V_VMResOn(volptr) &&
	ReplicatedOp && v->vptr->disk.type == vDirectory) 
	MakeLogNonEmpty(v->vptr);

    /* Perform operation. */
    {
	int truncp = 0;

	if (Mask & SET_LENGTH) {
	    truncp = 1;
	    int tblocks = (int) (nBlocks(Status->Length) - nBlocks(v->vptr->disk.length));
	    if (errorCode = AdjustDiskUsage(volptr, tblocks))
		goto FreeLocks;
	    deltablocks = tblocks;
	}

	/* f_sinode will be non-zero on return if COW occurred! */
	if (ReplicatedOp)
	    GetMyVS(volptr, OldVS, NewVS);
	PerformNewSetAttr(client, VSGVolnum, volptr, v->vptr, ReplicatedOp,
	  	          Status->Length, Status->Date, Status->Owner,	
		          Status->Mode, Mask, StoreId, &v->f_sinode, NewVS);
	if (v->f_sinode != 0) {
	    v->f_finode = v->vptr->disk.inodeNumber;
	    truncp = 0;
	}

	if (truncp) {
	    v->f_tinode = v->vptr->disk.inodeNumber;
	    v->f_tlength = v->vptr->disk.length;
	}

	SetStatus(v->vptr, Status, rights, anyrights);
	if (ReplicatedOp)
	    SetVSStatus(client, volptr, NewVS, VCBStatus);
    }

    if (AllowResolution && V_VMResOn(volptr)) {
	/* Create Log Record */
	if (ReplicatedOp && !errorCode &&
	    v->vptr->disk.type == vDirectory) {
	    int ind = InitVMLogRecord(V_volumeindex(volptr),
				      Fid, StoreId, ViceNewStore_OP,
				      STATUSStore, Status->Owner,
				      Status->Mode, Mask);
	    sle *SLE = new sle(ind);
	    v->sl.append(SLE);
	}
    }
    if (AllowResolution && V_RVMResOn(volptr)) {
	if (ReplicatedOp && !errorCode &&
	    v->vptr->disk.type == vDirectory) {
	    LogMsg(0, SrvDebugLevel, stdout,
		   "Going to spool store log record %u %u %u %u\n",
		   Status->Owner, Status->Mode, Status->Author, Status->Date);
	    if (errorCode = SpoolVMLogRecord(vlist, v, volptr, StoreId, 
					     ViceNewStore_OP, STSTORE, 
					     Status->Owner, Status->Mode,
					     Status->Author, Status->Date,
					     Mask, &Status->VV)) 
		LogMsg(0, SrvDebugLevel, stdout, 
		       "ViceNewSetAttr: Error %d during SpoolVMLogRecord\n", errorCode);
	}
    }

FreeLocks:
    /* Put objects. */
    {
	if (errorCode) {
	    cpent *cpe = CopPendingMan->findanddeq(StoreId);
	    if (cpe) {
		CopPendingMan->remove(cpe);
		delete cpe;
	    }
	}

	PutObjects(errorCode, volptr, SHARED_LOCK, vlist, deltablocks, 1);
    }

    LogMsg(2, SrvDebugLevel, stdout, "ViceNewSetAttr returns %s", ViceErrorMsg(errorCode));
END_TIMING(SetAttr_Total);
    return(errorCode);
}

/*
  BEGIN_HTML
  <a name="ViceSetACL"><strong>Set the Access Control List for a directory</strong></a> 
  END_HTML
*/
long ViceSetACL(RPC2_Handle RPCid, ViceFid *Fid, RPC2_CountedBS *AccessList,
		 ViceStatus *Status, RPC2_Unsigned PrimaryHost,
		 ViceStoreId *StoreId, 
		 RPC2_CountedBS *OldVS, RPC2_Integer *NewVS, 
		 CallBackStatus *VCBStatus, RPC2_CountedBS *PiggyBS)
{
    int errorCode = 0;		/* return code for caller */
    Error fileCode = 0;		/* return code for writes */
    Volume *volptr = 0;		/* pointer to the volume header */
    ClientEntry *client = 0;	/* pointer to client structure */
    Rights rights = 0;		/* rights for this user */
    Rights anyrights = 0;	/* rights for any user */
    AL_AccessList *newACL = 0;
    VolumeId VSGVolnum = Fid->Volume;
    int	ReplicatedOp = (PrimaryHost != 0);
    dlist *vlist = new dlist((CFN)VLECmp);
    vle *v = 0;

START_TIMING(SetACL_Total);
    LogMsg(1, SrvDebugLevel, stdout, "ViceSetACL: Fid = (%x.%x.%x)",
	     Fid->Volume, Fid->Vnode, Fid->Unique);

    /* Validate parameters. */
    {
	if (errorCode = ValidateParms(RPCid, &client, ReplicatedOp, &Fid->Volume, PiggyBS))
	    goto FreeLocks;
    }

    /* Get objects. */
    {
	v = AddVLE(*vlist, Fid);
	if (errorCode = GetFsObj(Fid, &volptr, &v->vptr, WRITE_LOCK, SHARED_LOCK, 0, 0))
	    goto FreeLocks;
    }

    /* Check semantics. */
    {
	if (errorCode = CheckSetACLSemantics(client, &v->vptr, &volptr, ReplicatedOp,
					      NormalVCmp, &Status->VV, Status->DataVersion,
					      &rights, &anyrights, AccessList, &newACL))
	    goto FreeLocks;
    }

    /* make sure resolution log is non-empty */
    if (AllowResolution && V_VMResOn(volptr) && ReplicatedOp) 
	MakeLogNonEmpty(v->vptr);

    /* Perform operation. */
    {
	if (ReplicatedOp)
	    GetMyVS(volptr, OldVS, NewVS);
	PerformSetACL(client, VSGVolnum, volptr, v->vptr,
		      ReplicatedOp, StoreId, newACL, NewVS);

	SetStatus(v->vptr, Status, rights, anyrights);
	if (ReplicatedOp)
	    SetVSStatus(client, volptr, NewVS, VCBStatus);
    }

    if (AllowResolution && V_VMResOn(volptr) && ReplicatedOp) {
	/* Create log entry for resolution logs in vm only */
	int ind;
	ind = InitVMLogRecord(V_volumeindex(volptr),
			      Fid, StoreId, ViceNewStore_OP, 
			      ACLStore, newACL);
	sle *SLE = new sle(ind);
	v->sl.append(SLE);
    }

    if (AllowResolution && V_RVMResOn(volptr)) 
	if (ReplicatedOp && !errorCode) {
	    assert(v->vptr->disk.type == vDirectory);
	    if (errorCode = SpoolVMLogRecord(vlist, v, volptr, StoreId, 
					     ViceNewStore_OP, ACLSTORE, newACL)) 
		LogMsg(0, SrvDebugLevel, stdout, 
		       "ViceSetACL: error %d during SpoolVMLogRecord\n", errorCode);
	}

FreeLocks:
    /* Put objects. */
    {
	if (newACL) AL_FreeAlist(&newACL);

	if (errorCode) {
	    cpent *cpe = CopPendingMan->findanddeq(StoreId);
	    if (cpe) {
		CopPendingMan->remove(cpe);
		delete cpe;
	    }
	}

	PutObjects(errorCode, volptr, SHARED_LOCK, vlist, 0, 1);
    }

    LogMsg(2, SrvDebugLevel, stdout, "ViceSetACL returns %s", ViceErrorMsg(errorCode));
END_TIMING(SetACL_Total);
    return(errorCode);
}

/*
  BEGIN_HTML
  <a name="ViceCreate"><strong>Obsoleted by ViceVCreate</strong></a> 
  <a href="#ViceVCreate"><strong>ViceVCreate</strong></a>
  END_HTML
*/
long ViceCreate(RPC2_Handle RPCid, ViceFid *Did, ViceFid *BidFid,
		RPC2_String Name, ViceStatus *Status, ViceFid *Fid,
		ViceStatus *DirStatus, RPC2_Unsigned PrimaryHost,
		ViceStoreId *StoreId, RPC2_CountedBS *PiggyBS)
{
	/* fake out other parameters to VStore */
	CallBackStatus VCBStatus = NoCallBack;
	RPC2_Integer NewVS = 0;
	RPC2_CountedBS OldVS;
	OldVS.SeqLen = 0;
	OldVS.SeqBody = 0;

	return(ViceVCreate(RPCid, Did, BidFid, Name, Status, Fid,
			   DirStatus, PrimaryHost, StoreId, 
			  &OldVS, &NewVS, &VCBStatus, PiggyBS));
}


/*
  BEGIN_HTML
  <a name="ViceVCreate"><strong>Create an object with given name in its parent's directory</strong></a> 
  END_HTML
*/
long ViceVCreate(RPC2_Handle RPCid, ViceFid *Did, ViceFid *BidFid,
		RPC2_String Name, ViceStatus *Status, ViceFid *Fid,
		ViceStatus *DirStatus, RPC2_Unsigned PrimaryHost,
		ViceStoreId *StoreId, RPC2_CountedBS *OldVS,
		RPC2_Integer *NewVS, CallBackStatus *VCBStatus,
		RPC2_CountedBS *PiggyBS)
{
    int errorCode = 0;		/* error code */
    Error fileCode = 0;		/* used when writing Vnodes */
    Volume *volptr = 0;		/* pointer to the volume header */
    ClientEntry *client = 0;	/* pointer to client structure */
    Rights rights = 0;		/* rights for this user */
    Rights anyrights = 0;	/* rights for any user */
    int deltablocks = 0;
    VolumeId VSGVolnum = Did->Volume;
    int ReplicatedOp = (PrimaryHost != 0);
    dlist *vlist = new dlist((CFN)VLECmp);
    vle *pv = 0;
    vle *cv = 0;

START_TIMING(Create_Total);
#ifdef _TIMECALLS_
    START_NSC_TIMING(Create_Total);
#endif _TIMECALLS_
    LogMsg(1, SrvDebugLevel, stdout,"ViceCreate: (%x.%x.%x), %s",
	    Did->Volume, Did->Vnode, Did->Unique, Name);

    /* Validate parameters. */
    {
	if (errorCode = ValidateParms(RPCid, &client, ReplicatedOp, &Did->Volume, PiggyBS))
	    goto FreeLocks;

	if (ReplicatedOp) {
	    /* Child/Parent volume match. */
	    if (Fid->Volume != VSGVolnum) {
		LogMsg(0, SrvDebugLevel, stdout, "ViceCreate: ChildVol (%x) != ParentVol (%x)",
			Fid->Volume, VSGVolnum);
		errorCode = EXDEV;
		goto FreeLocks;
	    }
	    Fid->Volume	= Did->Volume;	    /* manual XlateVid() */
	}

	/* Sanity. */
	if (FID_EQ(*Did, *Fid)) {
	    LogMsg(0, SrvDebugLevel, stdout, "ViceCreate: Did = Fid (%x.%x.%x)",
		    Did->Volume, Did->Vnode, Did->Unique);
	    errorCode = EINVAL;
	    goto FreeLocks;
	}

	/* Deprecated/Inapplicable parameters. */
	if (!FID_EQ(NullFid, *BidFid)) {
	    LogMsg(0, SrvDebugLevel, stdout, "ViceCreate: non-Null BidFid (%x.%x.%x)",
		    BidFid->Volume, BidFid->Vnode, BidFid->Unique);
	    errorCode = EINVAL;
	    goto FreeLocks;
	}
    }

    /* Get objects. */
    {
	pv = AddVLE(*vlist, Did);
	if (errorCode = GetFsObj(Did, &volptr, &pv->vptr, WRITE_LOCK, SHARED_LOCK, 0, 0))
	    goto FreeLocks;

	/* This may violate locking protocol! -JJK */
	Vnode *vptr = 0;
	if (errorCode = AllocVnode(&vptr, volptr, (ViceDataType)vFile, Fid, Did,
				   client->Id, PrimaryHost, &deltablocks)) {
	    assert(vptr == 0);
	    goto FreeLocks;
	}
	cv = AddVLE(*vlist, Fid);
	cv->vptr = vptr;
    }

    /* Check semantics. */
    {
	if (errorCode = CheckCreateSemantics(client, &pv->vptr, &cv->vptr, (char *)Name,
					      &volptr, ReplicatedOp, NormalVCmp,
					      DirStatus, Status, &rights, &anyrights, 1))
	    goto FreeLocks;
    }

    /* make sure resolution log is non-empty */
    if (AllowResolution && V_VMResOn(volptr) && ReplicatedOp) 
	MakeLogNonEmpty(pv->vptr);

    /* Perform operation. */
    {
	int tblocks = 0;

	if (ReplicatedOp)
	    GetMyVS(volptr, OldVS, NewVS);
	PerformCreate(client, VSGVolnum, volptr, pv->vptr, cv->vptr, (char *)Name,
		      DirStatus->Date, Status->Mode, ReplicatedOp, StoreId, 
		      &pv->d_cinode, &tblocks, NewVS);
	deltablocks += tblocks;
	SetStatus(pv->vptr, DirStatus, rights, anyrights);
	SetStatus(cv->vptr, Status, rights, anyrights);

	/* Until CVVV probes? -JJK */
	if (1/*!ReplicatedOp || PrimaryHost == ThisHostAddr*/) 
	    Status->CallBack = CodaAddCallBack(client->VenusId, Fid, VSGVolnum);
	if (ReplicatedOp)
	    SetVSStatus(client, volptr, NewVS, VCBStatus);
    }

    if (AllowResolution && V_VMResOn(volptr)) {
	/* Create Log Record */
	if (ReplicatedOp) {
	    int ind;
	    ind = InitVMLogRecord(V_volumeindex(volptr),
				 Did, StoreId,  ViceCreate_OP, 
				 Name, Fid->Vnode, Fid->Unique);
	    sle *SLE = new sle(ind);
	    pv->sl.append(SLE);
	}
    }
#ifdef _TIMECALLS_
    START_NSC_TIMING(SpoolVMLogRecord);
#endif _TIMECALLS_    
    if (AllowResolution && V_RVMResOn(volptr)) 
	/* Create Log Record */
	if (ReplicatedOp && !errorCode) 
	    if (errorCode = SpoolVMLogRecord(vlist, pv, volptr, StoreId,
					     ViceCreate_OP, 
					     Name, Fid->Vnode, Fid->Unique, 
					     client?client->Id:0))
		LogMsg(0, SrvDebugLevel, stdout, 
		       "ViceCreate: Error %d during SpoolVMLogRecord\n",
		       errorCode);
#ifdef _TIMECALLS_
    END_NSC_TIMING(SpoolVMLogRecord);
#endif _TIMECALLS_    

  FreeLocks:
    /* Put objects. */
    {
	if (errorCode) {
	    cpent *cpe = CopPendingMan->findanddeq(StoreId);
	    if (cpe) {
		CopPendingMan->remove(cpe);
		delete cpe;
	    }
	}

	PutObjects(errorCode, volptr, SHARED_LOCK, vlist, deltablocks, 1);
    }

    LogMsg(2, SrvDebugLevel, stdout,"ViceCreate returns %s", ViceErrorMsg(errorCode));
END_TIMING(Create_Total);
#ifdef _TIMECALLS_
    END_NSC_TIMING(Create_Total);
#endif _TIMECALLS_

    Fid->Volume	= VSGVolnum;	    /* Fid is an IN/OUT parameter; re-translate it. */
    return(errorCode);
}


/*
  BEGIN_HTML
  <a name="ViceRemove"><strong>Obsoleted by ViceVRemove</strong></a> 
  <a href="#ViceVRemove"><strong>ViceVRemove</strong></a>
  END_HTML
*/
long ViceRemove(RPC2_Handle RPCid, ViceFid *Did, RPC2_String Name,
		 ViceStatus *DirStatus, ViceStatus *Status,
		 RPC2_Unsigned PrimaryHost, ViceStoreId *StoreId,
		 RPC2_CountedBS *PiggyBS)
{
	CallBackStatus VCBStatus = NoCallBack;
	RPC2_Integer NewVS = 0;
	RPC2_CountedBS OldVS;
	OldVS.SeqLen = 0;
	OldVS.SeqBody = 0;

	return(ViceVRemove(RPCid, Did, Name, DirStatus, Status, PrimaryHost, 
			   StoreId, &OldVS, &NewVS, &VCBStatus, PiggyBS));
}


/*
  BEGIN_HTML
  <a name="ViceVRemove"><strong>Delete an object and its name </strong></a> 
  END_HTML
*/
long ViceVRemove(RPC2_Handle RPCid, ViceFid *Did, RPC2_String Name,
		ViceStatus *DirStatus, ViceStatus *Status,
		RPC2_Unsigned PrimaryHost, ViceStoreId *StoreId,
		RPC2_CountedBS *OldVS, RPC2_Integer *NewVS, 
		CallBackStatus *VCBStatus, RPC2_CountedBS *PiggyBS)
{
    int errorCode = 0;		/* error code */
    Error fileCode = 0;		/* used when writing Vnodes */
    Volume *volptr = 0;		/* pointer to the volume header */
    ViceFid Fid;		/* area for Fid from the directory */
    ClientEntry *client = 0;	/* pointer to client structure */
    Rights rights = 0;		/* rights for this user */
    Rights anyrights = 0;	/* rights for any user */
    int deltablocks = 0;
    VolumeId VSGVolnum = Did->Volume;
    int ReplicatedOp = (PrimaryHost != 0);
    dlist *vlist = new dlist((CFN)VLECmp);
    vle *pv = 0;
    vle *cv = 0;

START_TIMING(Remove_Total);
#ifdef _TIMECALLS_
    START_NSC_TIMING(Remove_Total);
#endif _TIMECALLS_

    LogMsg(1, SrvDebugLevel, stdout, "ViceRemove: (%x.%x.%x), %s",
	    Did->Volume, Did->Vnode, Did->Unique, Name);

    /* Validate parameters. */
    {
	if (errorCode = ValidateParms(RPCid, &client, ReplicatedOp, &Did->Volume, PiggyBS))
	    goto FreeLocks;
    }

    /* Get objects. */
    {
	pv = AddVLE(*vlist, Did);
	if (errorCode = GetFsObj(Did, &volptr, &pv->vptr, WRITE_LOCK, SHARED_LOCK, 0, 0))
	    goto FreeLocks;

	/* This may violate locking protocol! -JJK */
	DirHandle dh;
	SetDirHandle(&dh, pv->vptr);
	if (Lookup((long *)&dh, (char *)Name, (long *)&Fid) != 0) {
	    errorCode = ENOENT;
	    goto FreeLocks;
	}
	Fid.Volume = Did->Volume;
	cv = AddVLE(*vlist, &Fid);
	if (errorCode = GetFsObj(&Fid, &volptr, &cv->vptr, WRITE_LOCK, SHARED_LOCK, 0, 0))
	    goto FreeLocks;
    }

    /* Check semantics. */
    {
	if (errorCode = CheckRemoveSemantics(client, &pv->vptr, &cv->vptr, (char *)Name,
					      &volptr, ReplicatedOp, NormalVCmp,
					      DirStatus, Status, &rights, &anyrights, 1))
	    goto FreeLocks;
    }

    /* make sure resolution log is non-empty */
    if (AllowResolution && V_VMResOn(volptr) && ReplicatedOp) 
	MakeLogNonEmpty(pv->vptr);

    /* Perform operation. */
    {
	if (ReplicatedOp)
	    GetMyVS(volptr, OldVS, NewVS);
	PerformRemove(client, VSGVolnum, volptr, pv->vptr, cv->vptr, (char *)Name,
		      DirStatus->Date, ReplicatedOp, StoreId, &pv->d_cinode, 
		      &deltablocks, NewVS);
	if (cv->vptr->delete_me) {
	    int tblocks = (int) -nBlocks(cv->vptr->disk.length);
	    if (errorCode = AdjustDiskUsage(volptr, tblocks))
		goto FreeLocks;
	    deltablocks += tblocks;

	    cv->f_sinode = cv->vptr->disk.inodeNumber;
	    cv->vptr->disk.inodeNumber = 0;
	}

	SetStatus(pv->vptr, DirStatus, rights, anyrights);
	SetStatus(cv->vptr, Status, rights, anyrights);
	if (ReplicatedOp)
	    SetVSStatus(client, volptr, NewVS, VCBStatus);
    }

    if (AllowResolution && V_VMResOn(volptr)) {
	/* Create Log Record */
	if (ReplicatedOp) {
	    ViceVersionVector ghostVV = cv->vptr->disk.versionvector;
	    int ind;
	    ind = InitVMLogRecord(V_volumeindex(volptr),
				  Did, StoreId, ViceRemove_OP,
				  Name, Fid.Vnode, Fid.Unique, &ghostVV);
	    sle *SLE = new sle(ind);
	    pv->sl.append(SLE);
	}
    }

#ifdef _TIMECALLS_
    START_NSC_TIMING(SpoolVMLogRecord);
#endif _TIMECALLS_    
    if (AllowResolution && V_RVMResOn(volptr)) 
	if (ReplicatedOp && !errorCode) {
	    ViceVersionVector ghostVV = cv->vptr->disk.versionvector;	    
	    if (errorCode = SpoolVMLogRecord(vlist, pv, volptr, StoreId,
					     ViceRemove_OP, Name, Fid.Vnode, 
					     Fid.Unique, &ghostVV))
		LogMsg(0, SrvDebugLevel, stdout, 
		       "ViceRemove: error %d during SpoolVMLogRecord\n",
		       errorCode);
	}
#ifdef _TIMECALLS_
    END_NSC_TIMING(SpoolVMLogRecord);
#endif _TIMECALLS_    

FreeLocks:
    /* Put objects. */
    {
	if (errorCode) {
	    cpent *cpe = CopPendingMan->findanddeq(StoreId);
	    if (cpe) {
		CopPendingMan->remove(cpe);
		delete cpe;
	    }
	}

	PutObjects(errorCode, volptr, SHARED_LOCK, vlist, deltablocks, 1);
    }

    LogMsg(2, SrvDebugLevel, stdout, "ViceRemove returns %s", ViceErrorMsg(errorCode));
END_TIMING(Remove_Total);
#ifdef _TIMECALLS_
    END_NSC_TIMING(Remove_Total);
#endif _TIMECALLS_
    
    return(errorCode);
}


/*
  BEGIN_HTML
  <a name="ViceLink"><strong>Obsoleted by ViceVLink</strong></a> 
  <a href="#ViceVLink"><strong>ViceVLink</strong></a>
  END_HTML
*/
long ViceLink(RPC2_Handle RPCid, ViceFid *Did, RPC2_String Name,
	       ViceFid *Fid, ViceStatus *Status, ViceStatus *DirStatus,
	       RPC2_Unsigned PrimaryHost, ViceStoreId *StoreId,
	       RPC2_CountedBS *PiggyBS)
{
	CallBackStatus VCBStatus = NoCallBack;
	RPC2_Integer NewVS = 0;
	RPC2_CountedBS OldVS;
	OldVS.SeqLen = 0;
	OldVS.SeqBody = 0;

	return(ViceVLink(RPCid, Did, Name, Fid, Status, DirStatus,
			 PrimaryHost, StoreId, &OldVS, &NewVS, 
			 &VCBStatus, PiggyBS));
}


/*
  BEGIN_HTML
  <a name="ViceVLink"><strong>Create a new name for an already existing file </strong></a> 
  END_HTML
*/
long ViceVLink(RPC2_Handle RPCid, ViceFid *Did, RPC2_String Name,
	       ViceFid *Fid, ViceStatus *Status, ViceStatus *DirStatus,
	       RPC2_Unsigned PrimaryHost, ViceStoreId *StoreId,
	       RPC2_CountedBS *OldVS, RPC2_Integer *NewVS, 
	       CallBackStatus *VCBStatus, RPC2_CountedBS *PiggyBS)
{
    int errorCode = 0;		/* error code */
    Error fileCode = 0;		/* used when writing Vnodes */
    Volume *volptr = 0;		/* pointer to the volume header */
    ClientEntry *client = 0;	/* pointer to client structure */
    Rights rights = 0;		/* rights for this user */
    Rights anyrights = 0;	/* rights for any user */
    VolumeId VSGVolnum = Did->Volume;
    int ReplicatedOp = (PrimaryHost != 0);
    dlist *vlist = new dlist((CFN)VLECmp);
    vle *pv = 0;
    vle *cv = 0;
    int deltablocks = 0;

START_TIMING(Link_Total);
#ifdef _TIMECALLS_
    START_NSC_TIMING(Link_Total);
#endif _TIMECALLS_
    
    LogMsg(1, SrvDebugLevel, stdout,"ViceLink: (%x.%x.%x), %s --> (%x.%x.%x)",
	    Did->Volume, Did->Vnode, Did->Unique, Name,
	    Fid->Volume, Fid->Vnode, Fid->Unique);

    /* Validate parameters. */
    {
	if (errorCode = ValidateParms(RPCid, &client, ReplicatedOp, &Did->Volume, PiggyBS))
	    goto FreeLocks;

	/* Volume match. */
	if (Fid->Volume != VSGVolnum) {
	    LogMsg(0, SrvDebugLevel, stdout, "ViceLink: ChildVol (%x) != ParentVol (%x)",
		    Fid->Volume, VSGVolnum);
	    errorCode = EXDEV;
	    goto FreeLocks;
	}
	if (ReplicatedOp)
	    Fid->Volume	= Did->Volume;	    /* manual XlateVid() */

	/* Sanity. */
	if (FID_EQ(*Did, *Fid)) {
	    LogMsg(0, SrvDebugLevel, stdout, "ViceLink: Did = Fid (%x.%x.%x)",
		    Did->Volume, Did->Vnode, Did->Unique);
	    errorCode = EINVAL;
	    goto FreeLocks;
	}
    }

    /* Get objects. */
    {
	pv = AddVLE(*vlist, Did);
	if (errorCode = GetFsObj(Did, &volptr, &pv->vptr, WRITE_LOCK, SHARED_LOCK, 0, 0))
	    goto FreeLocks;

	/* This may violate locking protocol! -JJK */
	cv = AddVLE(*vlist, Fid);
	if (errorCode = GetFsObj(Fid, &volptr, &cv->vptr, WRITE_LOCK, SHARED_LOCK, 0, 0))
	    goto FreeLocks;
    }

    /* Check semantics. */
    {
	if (errorCode = CheckLinkSemantics(client, &pv->vptr, &cv->vptr, (char *)Name,
					    &volptr, ReplicatedOp, NormalVCmp,
					    DirStatus, Status, &rights, &anyrights, 1))
	    goto FreeLocks;
    }

    /* make sure resolution log is non-empty */
    if (AllowResolution && V_VMResOn(volptr) && ReplicatedOp) 
	MakeLogNonEmpty(pv->vptr);

    /* Perform operation. */
    {
	if (ReplicatedOp)
	    GetMyVS(volptr, OldVS, NewVS);
	PerformLink(client, VSGVolnum, volptr, pv->vptr, cv->vptr, (char *)Name,
		    DirStatus->Date, ReplicatedOp, StoreId, 
		    &pv->d_cinode, &deltablocks, NewVS);

	SetStatus(pv->vptr, DirStatus, rights, anyrights);
	SetStatus(cv->vptr, Status, rights, anyrights);
	if (ReplicatedOp)
	    SetVSStatus(client, volptr, NewVS, VCBStatus);
    }
    if (AllowResolution && V_VMResOn(volptr)) {
	/* spool log record */
	if (ReplicatedOp) {
	    int ind;
	    ind = InitVMLogRecord(V_volumeindex(volptr), Did, 
				 StoreId, ViceLink_OP, 
				 Name, Fid->Vnode, Fid->Unique);
	    assert(ind != -1);
	    sle *SLE = new sle(ind);
	    pv->sl.append(SLE);
	}
    }
#ifdef _TIMECALLS_
    START_NSC_TIMING(SpoolVMLogRecord);
#endif _TIMECALLS_    
    if (AllowResolution && V_RVMResOn(volptr)) 
	if (ReplicatedOp && !errorCode) {
	    if (errorCode = SpoolVMLogRecord(vlist, pv, volptr, StoreId,
					     ViceLink_OP, (char *)Name, Fid->Vnode, Fid->Unique, 
					     &(Vnode_vv(cv->vptr))))
		LogMsg(0, SrvDebugLevel, stdout, 
		       "ViceLink: Error %d during SpoolVMLogRecord\n",
		       errorCode);
	}
#ifdef _TIMECALLS_
    END_NSC_TIMING(SpoolVMLogRecord);
#endif _TIMECALLS_    

FreeLocks:
    /* Put objects. */
    {
	if (errorCode) {
	    cpent *cpe = CopPendingMan->findanddeq(StoreId);
	    if (cpe) {
		CopPendingMan->remove(cpe);
		delete cpe;
	    }
	}

	PutObjects(errorCode, volptr, SHARED_LOCK, vlist, deltablocks, 1);
    }

    LogMsg(2, SrvDebugLevel, stdout,"ViceLink returns %s", ViceErrorMsg(errorCode));
END_TIMING(Link_Total);
#ifdef _TIMECALLS_
    END_NSC_TIMING(Link_Total);
#endif _TIMECALLS_
    
    return(errorCode);
}


/*
  BEGIN_HTML
  <a name="ViceRename"><strong>Obsoleted by ViceVRename</strong></a> 
  <a href="#ViceVRename"><strong>ViceVRename</strong></a>
  END_HTML
*/
long ViceRename(RPC2_Handle RPCid, ViceFid *OldDid, RPC2_String OldName,
		 ViceFid *NewDid, RPC2_String NewName, ViceStatus *OldDirStatus,
		 ViceStatus *NewDirStatus, ViceStatus *SrcStatus,
		 ViceStatus *TgtStatus, RPC2_Unsigned PrimaryHost,
		 ViceStoreId *StoreId, RPC2_CountedBS *PiggyBS)
{
	CallBackStatus VCBStatus = NoCallBack;
	RPC2_Integer NewVS = 0;
	RPC2_CountedBS OldVS;
	OldVS.SeqLen = 0;
	OldVS.SeqBody = 0;

	return(ViceVRename(RPCid, OldDid, OldName, NewDid, NewName, 
			   OldDirStatus, NewDirStatus, SrcStatus,
			   TgtStatus, PrimaryHost, StoreId, &OldVS,
			   &NewVS, &VCBStatus, PiggyBS));
}

/*
  BEGIN_HTML
  <a name="ViceVRename"><strong>Rename a file or directory</strong></a> 
  END_HTML
*/
long ViceVRename(RPC2_Handle RPCid, ViceFid *OldDid, RPC2_String OldName,
		 ViceFid *NewDid, RPC2_String NewName, ViceStatus *OldDirStatus,
		 ViceStatus *NewDirStatus, ViceStatus *SrcStatus,
		 ViceStatus *TgtStatus, RPC2_Unsigned PrimaryHost,
		 ViceStoreId *StoreId,  RPC2_CountedBS *OldVS,
		RPC2_Integer *NewVS, CallBackStatus *VCBStatus,
		RPC2_CountedBS *PiggyBS)
{
    int errorCode = 0;		/* error code */
    Error fileCode = 0;		/* used when writing Vnodes */
    Volume *volptr = 0;		/* pointer to the volume header */
    ViceFid SrcFid;		/* Fid of file to move */
    ViceFid TgtFid;		/* Fid of new file */
    ClientEntry * client;	/* pointer to client structure */
    Rights sp_rights;		/* rights for this user */
    Rights sp_anyrights;	/* rights for any user */
    Rights tp_rights;		/* rights for this user */
    Rights tp_anyrights;	/* rights for any user */
    Rights s_rights;		/* rights for this user */
    Rights s_anyrights;		/* rights for any user */
    int deltablocks = 0;
    VolumeId VSGVolnum = OldDid->Volume;
    int ReplicatedOp = (PrimaryHost != 0);
    int SameParent = FID_EQ(*OldDid, *NewDid);
    dlist *vlist = new dlist((CFN)VLECmp);
    vle *spv = 0;
    vle *tpv = 0;
    vle *sv = 0;
    vle *tv = 0;

START_TIMING(Rename_Total);
#ifdef _TIMECALLS_
    START_NSC_TIMING(Rename_Total);
#endif _TIMECALLS_
    
    LogMsg(1, SrvDebugLevel, stdout, "ViceRename: (%x.%x.%x), %s --> (%x.%x.%x), %s",
	    OldDid->Volume, OldDid->Vnode, OldDid->Unique, OldName,
	    NewDid->Volume, NewDid->Vnode, NewDid->Unique, NewName);

    /* Validate parameters. */
    {
	if (errorCode = ValidateParms(RPCid, &client, ReplicatedOp, &OldDid->Volume, PiggyBS))
	    goto FreeLocks;

	/* Volume match. */
	if (NewDid->Volume != VSGVolnum) {
	    LogMsg(0, SrvDebugLevel, stdout, "ViceRename: TgtVol (%x) != SrcVol (%x)",
		    NewDid->Volume, VSGVolnum);
	    errorCode = EXDEV;
	    goto FreeLocks;
	}
	if (ReplicatedOp)
	    NewDid->Volume = OldDid->Volume;	    /* manual XlateVid() */
    }

    /* Get objects. */
    {
	spv = AddVLE(*vlist, OldDid);
	if (errorCode = GetFsObj(OldDid, &volptr, &spv->vptr, WRITE_LOCK, SHARED_LOCK, 0, 0))
	    goto FreeLocks;

	/* This may violate locking protocol! -JJK */
	if (SameParent) {
	    tpv = spv;
	}
	else {
	    tpv = AddVLE(*vlist, NewDid);
	    if (errorCode = GetFsObj(NewDid, &volptr, &tpv->vptr, WRITE_LOCK, SHARED_LOCK, 0, 0))
		goto FreeLocks;
	}

	/* This may violate locking protocol! -JJK */
	DirHandle sdh;
	SetDirHandle(&sdh, spv->vptr);
	if (Lookup((long *)&sdh, (char *)OldName, (long *)&SrcFid) != 0) {
	    errorCode = ENOENT;
	    goto FreeLocks;
	}
	SrcFid.Volume = OldDid->Volume;
	sv = AddVLE(*vlist, &SrcFid);
	if (errorCode = GetFsObj(&SrcFid, &volptr, &sv->vptr, WRITE_LOCK, SHARED_LOCK, 0, 0))
	    goto FreeLocks;

	/* This may violate locking protocol! -JJK */
	DirHandle tdh;
	SetDirHandle(&tdh, tpv->vptr);
	if (errorCode = Lookup((long *)&tdh, (char *)NewName, (long *)&TgtFid)) {
	    if (errorCode != ENOENT)
		goto FreeLocks;

	    errorCode = 0;
	}
	else {
	    TgtFid.Volume = NewDid->Volume;
	    tv = AddVLE(*vlist, &TgtFid);
	    if (errorCode = GetFsObj(&TgtFid, &volptr, &tv->vptr, WRITE_LOCK, SHARED_LOCK, 0, 0))
		goto FreeLocks;
	}
    }

    /* Check semantics. */
    {
	if (errorCode = CheckRenameSemantics(client, &spv->vptr, &tpv->vptr, &sv->vptr,
					      (char *)OldName, (tv ? &tv->vptr : 0), (char *)NewName,
					      &volptr, ReplicatedOp, NormalVCmp, OldDirStatus,
					      NewDirStatus, SrcStatus, TgtStatus,
					      &sp_rights, &sp_anyrights, &tp_rights,
					      &tp_anyrights, &s_rights, &s_anyrights, 1, 0))
	    goto FreeLocks;
/*
	LogMsg(0, SrvDebugLevel, stdout, "ViceRename: (%x.%x.%x), [%d %d %d %d %d %d %d %d] : [%d %d %d %d %d %d %d %d]",
		V_id(volptr), spv->vptr->vnodeNumber, spv->vptr->disk.uniquifier,
		(Vnode_vv(spv->vptr)).Versions.Site0, (Vnode_vv(spv->vptr)).Versions.Site1,
		(Vnode_vv(spv->vptr)).Versions.Site2, (Vnode_vv(spv->vptr)).Versions.Site3,
		(Vnode_vv(spv->vptr)).Versions.Site4, (Vnode_vv(spv->vptr)).Versions.Site5,
		(Vnode_vv(spv->vptr)).Versions.Site6, (Vnode_vv(spv->vptr)).Versions.Site7,
		OldDirStatus->VV.Versions.Site0, OldDirStatus->VV.Versions.Site1,
		OldDirStatus->VV.Versions.Site2, OldDirStatus->VV.Versions.Site3,
		OldDirStatus->VV.Versions.Site4, OldDirStatus->VV.Versions.Site5,
		OldDirStatus->VV.Versions.Site6, OldDirStatus->VV.Versions.Site7);
	LogMsg(0, SrvDebugLevel, stdout, "ViceRename: (%x.%x.%x), [%d %d %d %d %d %d %d %d] : [%d %d %d %d %d %d %d %d]",
		V_id(volptr), tpv->vptr->vnodeNumber, tpv->vptr->disk.uniquifier,
		(Vnode_vv(tpv->vptr)).Versions.Site0, (Vnode_vv(tpv->vptr)).Versions.Site1,
		(Vnode_vv(tpv->vptr)).Versions.Site2, (Vnode_vv(tpv->vptr)).Versions.Site3,
		(Vnode_vv(tpv->vptr)).Versions.Site4, (Vnode_vv(tpv->vptr)).Versions.Site5,
		(Vnode_vv(tpv->vptr)).Versions.Site6, (Vnode_vv(tpv->vptr)).Versions.Site7,
		NewDirStatus->VV.Versions.Site0, NewDirStatus->VV.Versions.Site1,
		NewDirStatus->VV.Versions.Site2, NewDirStatus->VV.Versions.Site3,
		NewDirStatus->VV.Versions.Site4, NewDirStatus->VV.Versions.Site5,
		NewDirStatus->VV.Versions.Site6, NewDirStatus->VV.Versions.Site7);
	LogMsg(0, SrvDebugLevel, stdout, "ViceRename: (%x.%x.%x), [%d %d %d %d %d %d %d %d] : [%d %d %d %d %d %d %d %d]",
		V_id(volptr), sv->vptr->vnodeNumber, sv->vptr->disk.uniquifier,
		(Vnode_vv(sv->vptr)).Versions.Site0, (Vnode_vv(sv->vptr)).Versions.Site1,
		(Vnode_vv(sv->vptr)).Versions.Site2, (Vnode_vv(sv->vptr)).Versions.Site3,
		(Vnode_vv(sv->vptr)).Versions.Site4, (Vnode_vv(sv->vptr)).Versions.Site5,
		(Vnode_vv(sv->vptr)).Versions.Site6, (Vnode_vv(sv->vptr)).Versions.Site7,
		SrcStatus->VV.Versions.Site0, SrcStatus->VV.Versions.Site1,
		SrcStatus->VV.Versions.Site2, SrcStatus->VV.Versions.Site3,
		SrcStatus->VV.Versions.Site4, SrcStatus->VV.Versions.Site5,
		SrcStatus->VV.Versions.Site6, SrcStatus->VV.Versions.Site7);
	if (tv)
	    LogMsg(0, SrvDebugLevel, stdout, "ViceRename: (%x.%x.%x), [%d %d %d %d %d %d %d %d] : [%d %d %d %d %d %d %d %d]",
		    V_id(volptr), tv->vptr->vnodeNumber, tv->vptr->disk.uniquifier,
		    (Vnode_vv(tv->vptr)).Versions.Site0, (Vnode_vv(tv->vptr)).Versions.Site1,
		    (Vnode_vv(tv->vptr)).Versions.Site2, (Vnode_vv(tv->vptr)).Versions.Site3,
		    (Vnode_vv(tv->vptr)).Versions.Site4, (Vnode_vv(tv->vptr)).Versions.Site5,
		    (Vnode_vv(tv->vptr)).Versions.Site6, (Vnode_vv(tv->vptr)).Versions.Site7,
		    TgtStatus->VV.Versions.Site0, TgtStatus->VV.Versions.Site1,
		    TgtStatus->VV.Versions.Site2, TgtStatus->VV.Versions.Site3,
		    TgtStatus->VV.Versions.Site4, TgtStatus->VV.Versions.Site5,
		    TgtStatus->VV.Versions.Site6, TgtStatus->VV.Versions.Site7);
	LogMsg(0, SrvDebugLevel, stdout, "ViceRename: (%x.%x.%x), %d : %d",
		V_id(volptr), spv->vptr->vnodeNumber, spv->vptr->disk.uniquifier,
		spv->vptr->disk.dataVersion, OldDirStatus->DataVersion);
	LogMsg(0, SrvDebugLevel, stdout, "ViceRename: (%x.%x.%x), %d : %d",
		V_id(volptr), tpv->vptr->vnodeNumber, tpv->vptr->disk.uniquifier,
		tpv->vptr->disk.dataVersion, NewDirStatus->DataVersion);
	LogMsg(0, SrvDebugLevel, stdout, "ViceRename: (%x.%x.%x), %d : %d",
		V_id(volptr), sv->vptr->vnodeNumber, sv->vptr->disk.uniquifier,
		sv->vptr->disk.dataVersion, SrcStatus->DataVersion);
	if (tv)
	    LogMsg(0, SrvDebugLevel, stdout, "ViceRename: (%x.%x.%x), %d : %d",
		    V_id(volptr), tv->vptr->vnodeNumber, tv->vptr->disk.uniquifier,
		    tv->vptr->disk.dataVersion, TgtStatus->DataVersion);
*/
    }

    /* make sure resolution logs are non-empty */
    if (AllowResolution && V_VMResOn(volptr) && ReplicatedOp) {
	if (tpv->vptr) MakeLogNonEmpty(tpv->vptr);
	if (spv->vptr) MakeLogNonEmpty(spv->vptr);
    }

    /* Perform operation. */
    {
	if (ReplicatedOp)
	    GetMyVS(volptr, OldVS, NewVS);
	PerformRename(client, VSGVolnum, volptr, spv->vptr, tpv->vptr, sv->vptr,
		      (tv ? tv->vptr : 0), (char *)OldName, (char *)NewName, OldDirStatus->Date,
		      ReplicatedOp, StoreId, &spv->d_cinode, &tpv->d_cinode, &sv->d_cinode, NULL,
		      NewVS);
	if (tv && tv->vptr->delete_me) {
	    int tblocks = (int) -nBlocks(tv->vptr->disk.length);
	    if (errorCode = AdjustDiskUsage(volptr, tblocks))
		goto FreeLocks;
	    deltablocks = tblocks;

	    if (tv->vptr->disk.type != vDirectory) {
		tv->f_sinode = tv->vptr->disk.inodeNumber;
		tv->vptr->disk.inodeNumber = 0;
	    }
	}

	SetStatus(spv->vptr, OldDirStatus, sp_rights, sp_anyrights);
	SetStatus(tpv->vptr, NewDirStatus, tp_rights, tp_anyrights);
	SetStatus(sv->vptr, SrcStatus, s_rights, s_anyrights);
	if (tv && !tv->vptr->delete_me) {
	    /* If tv->vptr still exists then it must not have been a directory (and therefore it has no ACL). */
	    SetStatus(tv->vptr, TgtStatus, 0, 0);
	}
	if (ReplicatedOp)
	    SetVSStatus(client, volptr, NewVS, VCBStatus);
    }

    /* spool log record - one for src and one for target if different */
    if (AllowResolution && V_VMResOn(volptr)) {
	if (ReplicatedOp) 
	    SpoolRenameLogRecord(ViceRename_OP, sv, tv, spv, tpv, volptr,
				 (char *)OldName, (char *)NewName, StoreId);
    }

    /* spool rename log record for recoverable rvm logs */
#ifdef _TIMECALLS_
    START_NSC_TIMING(SpoolVMLogRecord);
#endif _TIMECALLS_    
    if (AllowResolution && V_RVMResOn(volptr) && ReplicatedOp && !errorCode) 
	errorCode = SpoolRenameLogRecord((int) ViceRename_OP,(dlist *)  vlist, sv->vptr, 
					 (Vnode *)(tv ? tv->vptr : NULL), spv->vptr, 
					 tpv->vptr, 
					 volptr, (char *)OldName, 
					 (char *)NewName, StoreId);


#ifdef _TIMECALLS_
    END_NSC_TIMING(SpoolVMLogRecord);
#endif _TIMECALLS_    

FreeLocks: 
    /* Put objects. */
    {
	if (errorCode) {
	    cpent *cpe = CopPendingMan->findanddeq(StoreId);
	    if (cpe) {
		CopPendingMan->remove(cpe);
		delete cpe;
	    }
	}

	PutObjects(errorCode, volptr, SHARED_LOCK, vlist, deltablocks, 1);
    }

    LogMsg(2, SrvDebugLevel, stdout, "ViceRename returns %s", ViceErrorMsg(errorCode));
END_TIMING(Rename_Total);
#ifdef _TIMECALLS_
    END_NSC_TIMING(Rename_Total);
#endif _TIMECALLS_

    return(errorCode);
}


/*
  BEGIN_HTML
  <a name="ViceMakeDir"><strong>Obsoleted by ViceVMakeDir</strong></a> 
  <a href="#ViceVMakeDir"><strong>ViceVMakeDir</strong></a>
  END_HTML
*/
long ViceMakeDir(RPC2_Handle RPCid, ViceFid *Did, RPC2_String Name,
		  ViceStatus *Status, ViceFid *NewDid, ViceStatus *DirStatus,
		  RPC2_Unsigned PrimaryHost, ViceStoreId *StoreId,
		  RPC2_CountedBS *PiggyBS)
{
	CallBackStatus VCBStatus = NoCallBack;
	RPC2_Integer NewVS = 0;
	RPC2_CountedBS OldVS;
	OldVS.SeqLen = 0;
	OldVS.SeqBody = 0;

	return(ViceVMakeDir(RPCid, Did, Name, Status, NewDid, DirStatus,
			    PrimaryHost, StoreId, &OldVS, &NewVS,
			    &VCBStatus, PiggyBS));
}

/*
  BEGIN_HTML
  <a name="ViceVMakeDir"><strong>Make a new directory</strong></a> 
  END_HTML
*/
long ViceVMakeDir(RPC2_Handle RPCid, ViceFid *Did, RPC2_String Name,
		  ViceStatus *Status, ViceFid *NewDid, ViceStatus *DirStatus,
		  RPC2_Unsigned PrimaryHost, ViceStoreId *StoreId,
		  RPC2_CountedBS *OldVS,
		  RPC2_Integer *NewVS, CallBackStatus *VCBStatus,
		  RPC2_CountedBS *PiggyBS)
{
    int errorCode = 0;		/* error code */
    Error fileCode = 0;		/* used when writing Vnodes */
    Volume *volptr = 0;		/* pointer to the volume header */
    ClientEntry *client = 0;	/* pointer to client structure */
    Rights rights = 0;		/* rights for this user */
    Rights anyrights = 0;	/* rights for any user */
    int deltablocks = 0;
    VolumeId VSGVolnum = Did->Volume;
    int ReplicatedOp = (PrimaryHost != 0);
    dlist *vlist = new dlist((CFN)VLECmp);
    vle *pv = 0;
    vle *cv = 0;

START_TIMING(MakeDir_Total);
#ifdef _TIMECALLS_
    START_NSC_TIMING(MakeDir_Total);
#endif _TIMECALLS_
    LogMsg(1, SrvDebugLevel, stdout,"ViceMakeDir: (%x.%x.%x), %s",
	    Did->Volume, Did->Vnode, Did->Unique, Name);
    
    /* Validate parameters. */
    {
	if (errorCode = ValidateParms(RPCid, &client, ReplicatedOp, &Did->Volume, PiggyBS))
	    goto FreeLocks;

	/* Child/Parent volume match. */
	if (ReplicatedOp) {
	    if (NewDid->Volume != VSGVolnum) {
		LogMsg(0, SrvDebugLevel, stdout, "ViceMakeDir: ChildVol (%x) != ParentVol (%x)",
			NewDid->Volume, VSGVolnum);
		errorCode = EXDEV;
		goto FreeLocks;
	    }
	    NewDid->Volume = Did->Volume;	/* manual XlateVid() */
	}

	/* Sanity. */
	if (FID_EQ(*Did, *NewDid)) {
	    LogMsg(0, SrvDebugLevel, stdout, "ViceMakeDir: Did = NewDid (%x.%x.%x)",
		    Did->Volume, Did->Vnode, Did->Unique);
	    errorCode = EINVAL;
	    goto FreeLocks;
	}
    }

    /* Get objects. */
    {
	pv = AddVLE(*vlist, Did);
	if (errorCode = GetFsObj(Did, &volptr, &pv->vptr, WRITE_LOCK, SHARED_LOCK, 0, 0))
	    goto FreeLocks;

	/* This may violate locking protocol! -JJK */
	Vnode *vptr = 0;
	if (errorCode = AllocVnode(&vptr, volptr, (ViceDataType)vDirectory, NewDid,
				   Did, client->Id, PrimaryHost, &deltablocks)) {
	    assert(vptr == 0);
	    goto FreeLocks;
	}
	cv = AddVLE(*vlist, NewDid);
	cv->vptr = vptr;
    }

    /* Check semantics. */
    {
	if (errorCode = CheckMkdirSemantics(client, &pv->vptr, &cv->vptr, (char *)Name,
					     &volptr, ReplicatedOp, NormalVCmp,
					     DirStatus, Status, &rights, &anyrights, 1))
	    goto FreeLocks;
    }

    /* make sure resolution log is non-empty */
    if (AllowResolution && V_VMResOn(volptr) && ReplicatedOp) 
	MakeLogNonEmpty(pv->vptr);

    /* Perform operation. */
    {
	int tblocks = 0;
	if (ReplicatedOp)
	    GetMyVS(volptr, OldVS, NewVS);
	PerformMkdir(client, VSGVolnum, volptr, pv->vptr, cv->vptr, (char *)Name,
		     DirStatus->Date, Status->Mode, ReplicatedOp, StoreId, 
		     &pv->d_cinode, &tblocks, NewVS);
	deltablocks += tblocks;
	SetStatus(pv->vptr, DirStatus, rights, anyrights);
	SetStatus(cv->vptr, Status, rights, anyrights);

	/* Until CVVV probes? -JJK */
	if (1/*!ReplicatedOp || PrimaryHost == ThisHostAddr*/) 
	    Status->CallBack = CodaAddCallBack(client->VenusId, NewDid, VSGVolnum);
	if (ReplicatedOp)
	    SetVSStatus(client, volptr, NewVS, VCBStatus);
    }

    if (AllowResolution && V_VMResOn(volptr)) {
	/* Create Log Record */
	if (ReplicatedOp) {
	    int ind;
	    ind = InitVMLogRecord(V_volumeindex(volptr),
				  Did, StoreId, ViceMakeDir_OP, 
				  (char *)Name, NewDid->Vnode, NewDid->Unique);
	    sle *SLE = new sle(ind);
	    pv->sl.append(SLE);

	    /* spool a record for child too */
	    ind = InitVMLogRecord(V_volumeindex(volptr),
				 NewDid, StoreId, ViceMakeDir_OP, 
				 ".", NewDid->Vnode, NewDid->Unique);
	    SLE = new sle(ind);
	    cv->sl.append(SLE);
	}
    }
#ifdef _TIMECALLS_
    START_NSC_TIMING(SpoolVMLogRecord);
#endif _TIMECALLS_    
    if (AllowResolution && V_RVMResOn(volptr)) 
	if (ReplicatedOp && !errorCode) {
	    if (errorCode = SpoolVMLogRecord(vlist, pv, volptr, StoreId,
					     ViceMakeDir_OP, Name, 
					     NewDid->Vnode, NewDid->Unique, 
					     client?client->Id:0)) 
		LogMsg(0, SrvDebugLevel, stdout, 
		       "ViceMakeDir: Error %d during SpoolVMLogRecord for parent\n",
		       errorCode);
	    // spool child's log record 
	    if (!errorCode && (errorCode = SpoolVMLogRecord(vlist, cv, volptr, StoreId, 
							    ViceMakeDir_OP, ".",
							    NewDid->Vnode, 
							    NewDid->Unique,
							    client?client->Id:0)))
		LogMsg(0, SrvDebugLevel, stdout, 
		       "ViceMakeDir: Error %d during SpoolVMLogRecord for child\n",
		       errorCode);
	}
#ifdef _TIMECALLS_
    END_NSC_TIMING(SpoolVMLogRecord);
#endif _TIMECALLS_    
    

FreeLocks: 
    /* Put objects. */
    {
	if (errorCode) {
	    cpent *cpe = CopPendingMan->findanddeq(StoreId);
	    if (cpe) {
		CopPendingMan->remove(cpe);
		delete cpe;
	    }
	}

	PutObjects(errorCode, volptr, SHARED_LOCK, vlist, deltablocks, 1);
    }

    LogMsg(2, SrvDebugLevel, stdout,"ViceMakeDir returns %s", ViceErrorMsg(errorCode));
END_TIMING(MakeDir_Total);
#ifdef _TIMECALLS_
    END_NSC_TIMING(MakeDir_Total);
#endif _TIMECALLS_
    
    NewDid->Volume = VSGVolnum;		/* NewDid is an IN/OUT paramter; re-translate it. */
    return(errorCode);
}


/*
  BEGIN_HTML
  <a name="ViceRemoveDir"><strong>Obsoleted by ViceVRemoveDir</strong></a> 
  <a href="#ViceVRemoveDir"><strong>ViceVRemoveDir</strong></a>
  END_HTML
*/
long ViceRemoveDir(RPC2_Handle RPCid, ViceFid *Did, RPC2_String Name,
		    ViceStatus *DirStatus, ViceStatus *Status,
		    RPC2_Unsigned PrimaryHost, ViceStoreId *StoreId,
		    RPC2_CountedBS *PiggyBS)
{
	CallBackStatus VCBStatus = NoCallBack;
	RPC2_Integer NewVS = 0;
	RPC2_CountedBS OldVS;
	OldVS.SeqLen = 0;
	OldVS.SeqBody = 0;

	return(ViceVRemoveDir(RPCid, Did, Name, DirStatus, Status,
			      PrimaryHost, StoreId, &OldVS, &NewVS,
			      &VCBStatus, PiggyBS));
}

/*
  BEGIN_HTML
  <a name="ViceVRemoveDir"><strong>Delete an empty directory</strong></a> 
  END_HTML
*/
long ViceVRemoveDir(RPC2_Handle RPCid, ViceFid *Did, RPC2_String Name,
		    ViceStatus *DirStatus, ViceStatus *Status,
		    RPC2_Unsigned PrimaryHost, ViceStoreId *StoreId,
		    RPC2_CountedBS *OldVS,
		    RPC2_Integer *NewVS, CallBackStatus *VCBStatus,
		    RPC2_CountedBS *PiggyBS)
{
    int errorCode = 0;		/* error code */
    Error fileCode = 0;		/* used when writing Vnodes */
    Volume *volptr = 0;		/* pointer to the volume header */
    ViceFid ChildDid;		/* area for Fid from the directory */
    ClientEntry *client = 0;	/* pointer to client structure */
    Rights rights = 0;		/* rights for this user */
    Rights anyrights = 0;	/* rights for any user */
    int deltablocks = 0;
    VolumeId VSGVolnum = Did->Volume;
    int ReplicatedOp = (PrimaryHost != 0);
    dlist *vlist = new dlist((CFN)VLECmp);
    vle *pv = 0;
    vle *cv = 0;

START_TIMING(RemoveDir_Total);
#ifdef _TIMECALLS_
    START_NSC_TIMING(RemoveDir_Total)
#endif _TIMECALLS_
    LogMsg(1, SrvDebugLevel, stdout, "ViceRemoveDir: (%x.%x.%x), %s",
	     Did->Volume, Did->Vnode, Did->Unique, Name);

    /* Validate parameters. */
    {
	if (errorCode = ValidateParms(RPCid, &client, ReplicatedOp, &Did->Volume, PiggyBS))
	    goto FreeLocks;
    }

    /* Get objects. */
    {
	pv = AddVLE(*vlist, Did);
	if (errorCode = GetFsObj(Did, &volptr, &pv->vptr, WRITE_LOCK, SHARED_LOCK, 0, 0))
	    goto FreeLocks;

	/* This may violate locking protocol! -JJK */
	DirHandle dh;
	SetDirHandle(&dh, pv->vptr);
	if (Lookup((long *)&dh, (char *)Name, (long *)&ChildDid) != 0) {
	    errorCode = ENOENT;
	    goto FreeLocks;
	}
	ChildDid.Volume = Did->Volume;
	cv = AddVLE(*vlist, &ChildDid);
	if (errorCode = GetFsObj(&ChildDid, &volptr, &cv->vptr, WRITE_LOCK, SHARED_LOCK, 0, 0))
	    goto FreeLocks;
    }

    /* Check semantics. */
    {
	if (errorCode = CheckRmdirSemantics(client, &pv->vptr, &cv->vptr, (char *)Name,
					     &volptr, ReplicatedOp, NormalVCmp,
					     DirStatus, Status, &rights, &anyrights, 1))
	    goto FreeLocks;
    }

    /* make sure resolution log is non-empty */
    if (AllowResolution && V_VMResOn(volptr) && ReplicatedOp) 
	MakeLogNonEmpty(pv->vptr);

    /* Perform operation. */
    {
	if (ReplicatedOp)
	    GetMyVS(volptr, OldVS, NewVS);
	PerformRmdir(client, VSGVolnum, volptr, pv->vptr, cv->vptr, (char *)Name,
		      DirStatus->Date, ReplicatedOp, StoreId, &pv->d_cinode, 
		     &deltablocks, NewVS);
	{
	    assert(cv->vptr->delete_me);
	    int tblocks = (int) -nBlocks(cv->vptr->disk.length);
	    if (errorCode = AdjustDiskUsage(volptr, tblocks))
		goto FreeLocks;
	    deltablocks += tblocks;
	}

	SetStatus(pv->vptr, DirStatus, rights, anyrights);
	SetStatus(cv->vptr, Status, rights, anyrights);
	if (ReplicatedOp)
	    SetVSStatus(client, volptr, NewVS, VCBStatus);
    }

    if (AllowResolution && V_VMResOn(volptr)) {
	if (ReplicatedOp) {
	    /* THIS SHOULD GO WHEN dir logs headers are in vnodes */
	    VNResLog *vnlog;
	    ViceStoreId ghostsid;
	    pdlist *pl = GetResLogList(cv->vptr->disk.vol_index, 
				       ChildDid.Vnode, ChildDid.Unique, 
				       &vnlog);
	    assert(pl != NULL);
	    ghostsid = cv->vptr->disk.versionvector.StoreId;
	    int ind;
	    ind = InitVMLogRecord(V_volumeindex(volptr),
				 Did, StoreId, ViceRemoveDir_OP,
				 (char *)Name, ChildDid.Vnode, ChildDid.Unique, 
				 pl->head, pl->count(), &(vnlog->LCP),
				 &ghostsid);
	    sle *SLE = new sle(ind);
	    pv->sl.append(SLE);
	}
    }
#ifdef _TIMECALLS_
    START_NSC_TIMING(SpoolVMLogRecord);
#endif _TIMECALLS_    
    if (AllowResolution && V_RVMResOn(volptr)) {
	if (ReplicatedOp) 
	    if (errorCode = SpoolVMLogRecord(vlist, pv, volptr, StoreId,
					     ViceRemoveDir_OP, Name, 
					     ChildDid.Vnode, ChildDid.Unique, 
					     VnLog(cv->vptr), &(Vnode_vv(cv->vptr).StoreId),
					     &(Vnode_vv(cv->vptr).StoreId)))
		LogMsg(0, SrvDebugLevel, stdout, 
		       "ViceRemoveDir: error %d in SpoolVMLogRecord\n",
		       errorCode);
    }
#ifdef _TIMECALLS_
    END_NSC_TIMING(SpoolVMLogRecord);
#endif _TIMECALLS_    

FreeLocks: 
    /* Put objects. */
    {
	if (errorCode) {
	    cpent *cpe = CopPendingMan->findanddeq(StoreId);
	    if (cpe) {
		CopPendingMan->remove(cpe);
		delete cpe;
	    }
	}

	PutObjects(errorCode, volptr, SHARED_LOCK, vlist, deltablocks, 1);
    }

    LogMsg(2, SrvDebugLevel, stdout,"ViceRemoveDir returns %s", ViceErrorMsg(errorCode));
END_TIMING(RemoveDir_Total);
#ifdef _TIMECALLS_
    END_NSC_TIMING(RemoveDir_Total);
#endif _TIMECALLS_
    
    return(errorCode);
}

/*
  BEGIN_HTML
  <a name="ViceSymLink"><strong>Obsoleted by ViceVSymLink</strong></a> 
  <a href="#ViceVSymLink"><strong>ViceVSymLink</strong></a>
  END_HTML
*/
long ViceSymLink(RPC2_Handle RPCid, ViceFid *Did, RPC2_String NewName,
		 RPC2_String OldName, ViceFid *Fid, ViceStatus *Status,
		 ViceStatus *DirStatus, RPC2_Unsigned PrimaryHost,
		 ViceStoreId *StoreId, RPC2_CountedBS *PiggyBS)
{
	CallBackStatus VCBStatus = NoCallBack;
	RPC2_Integer NewVS = 0;
	RPC2_CountedBS OldVS;
	OldVS.SeqLen = 0;
	OldVS.SeqBody = 0;

	return(ViceVSymLink(RPCid, Did, NewName, OldName, Fid,
			    Status, DirStatus, PrimaryHost, StoreId,
			    &OldVS, &NewVS, &VCBStatus, PiggyBS));
}


/*
  BEGIN_HTML
  <a name="ViceVSymLink"><strong>Create a symbolic link</strong></a> 
  END_HTML
*/
long ViceVSymLink(RPC2_Handle RPCid, ViceFid *Did, RPC2_String NewName,
		  RPC2_String OldName, ViceFid *Fid, ViceStatus *Status,
		  ViceStatus *DirStatus, RPC2_Unsigned PrimaryHost,
		  ViceStoreId *StoreId,  RPC2_CountedBS *OldVS,
		  RPC2_Integer *NewVS, CallBackStatus *VCBStatus,
		  RPC2_CountedBS *PiggyBS)
/*RPC2_Handle	  RPCid;		handle of the user */
/*ViceFid	 *Did;		 Directory to add name to */
/*RPC2_String	  NewName;	 Name of the link */
/*RPC2_String	  OldName;	 Name of existing file */
/*ViceFid	 *Fid;		 New Fid */
/*ViceStatus	 *Status;		Status for new fid */
/*ViceStatus	 *DirStatus;	 Status for the directory */
{
    int errorCode = 0;		/* error code */
    Error fileCode = 0;		/* used when writing Vnodes */
    Volume *volptr = 0;		/* pointer to the volume header */
    ClientEntry *client = 0;	/* pointer to client structure */
    Rights rights = 0;		/* rights for this user */
    Rights anyrights = 0;		/* rights for any user */
    int deltablocks = 0;
    VolumeId VSGVolnum = Did->Volume;
    int ReplicatedOp = (PrimaryHost != 0);
    dlist *vlist = new dlist((CFN)VLECmp);
    vle *pv = 0;
    vle *cv = 0;

START_TIMING(SymLink_Total);
#ifdef _TIMECALLS_
    START_NSC_TIMING(SymLink_Total);
#endif _TIMECALLS_

    LogMsg(1, SrvDebugLevel, stdout, "ViceSymLink: (%x.%x.%x), %s --> %s",
	    Did->Volume, Did->Vnode, Did->Unique, NewName, OldName);

    /* Validate parameters. */
    {
	if (errorCode = ValidateParms(RPCid, &client, ReplicatedOp, &Did->Volume, PiggyBS))
	    goto FreeLocks;

	/* Child/Parent volume match. */
	if (ReplicatedOp) {
	    if (Fid->Volume != VSGVolnum) {
		LogMsg(0, SrvDebugLevel, stdout, "ViceSymLink: ChildVol (%x) != ParentVol (%x)",
			Fid->Volume, VSGVolnum);
		errorCode = EXDEV;
		goto FreeLocks;
	    }
	    Fid->Volume	= Did->Volume;	    /* manual XlateVid() */
	}

	/* Sanity. */
	if (FID_EQ(*Did, *Fid)) {
	    LogMsg(0, SrvDebugLevel, stdout, "ViceSymLink: Did = Fid (%x.%x.%x)",
		    Did->Volume, Did->Vnode, Did->Unique);
	    errorCode = EINVAL;
	    goto FreeLocks;
	}
    }

    /* Get objects. */
    {
	pv = AddVLE(*vlist, Did);
	if (errorCode = GetFsObj(Did, &volptr, &pv->vptr, WRITE_LOCK, SHARED_LOCK, 0, 0))
	    goto FreeLocks;

	/* This may violate locking protocol! -JJK */
	Vnode *vptr = 0;
	if (errorCode = AllocVnode(&vptr, volptr, (ViceDataType)vSymlink, Fid, Did,
				   client->Id, PrimaryHost, &deltablocks)) {
	    assert(vptr == 0);
	    goto FreeLocks;
	}
	cv = AddVLE(*vlist, Fid);
	cv->vptr = vptr;
    }

    /* Check semantics. */
    {
	if (errorCode = CheckSymlinkSemantics(client, &pv->vptr, &cv->vptr, (char *)NewName,
					       &volptr, ReplicatedOp, NormalVCmp,
					       DirStatus, Status, &rights, &anyrights, 1))
	    goto FreeLocks;
    }

    /* make sure resolution log is non-empty */
    if (AllowResolution && V_VMResOn(volptr) && ReplicatedOp) 
	MakeLogNonEmpty(pv->vptr);

    /* Perform operation. */
    {
	cv->f_finode = icreate((int) V_device(volptr), 0, (int) V_id(volptr),
			       (int) cv->vptr->vnodeNumber,
			       (int) cv->vptr->disk.uniquifier, 1);
	assert(cv->f_finode > 0);
	int linklen = (int) strlen((char *)OldName);
	assert(iwrite((int) V_device(volptr), (int) cv->f_finode, (int) V_parentId(volptr),
		      0, (char *)OldName, linklen) == linklen);
	if (ReplicatedOp)
	    GetMyVS(volptr, OldVS, NewVS);

	int tblocks = 0;
	PerformSymlink(client, VSGVolnum, volptr, pv->vptr, cv->vptr,
		       (char *)NewName, cv->f_finode, linklen, DirStatus->Date,
		       Status->Mode, ReplicatedOp, StoreId, 
		       &pv->d_cinode, &tblocks, NewVS);
	deltablocks += tblocks;

	SetStatus(pv->vptr, DirStatus, rights, anyrights);
	SetStatus(cv->vptr, Status, rights, anyrights);

	/* Until CVVV probes? -JJK */
	if (1/*!ReplicatedOp || PrimaryHost == ThisHostAddr*/) 
	    Status->CallBack = CodaAddCallBack(client->VenusId, Fid, VSGVolnum);
	if (ReplicatedOp)
	    SetVSStatus(client, volptr, NewVS, VCBStatus);
    }

    if (AllowResolution && V_VMResOn(volptr)) {
	/* Create Log Record */
	if (ReplicatedOp) {
	    int ind;
	    ind = InitVMLogRecord(V_volumeindex(volptr),
				  Did, StoreId, ViceSymLink_OP,
				  (char *)NewName, Fid->Vnode, Fid->Unique);
	    sle *SLE = new sle(ind);
	    pv->sl.append(SLE);
	}
    }
#ifdef _TIMECALLS_
    START_NSC_TIMING(SpoolVMLogRecord);
#endif _TIMECALLS_    
    if (AllowResolution && V_RVMResOn(volptr)) {
	/* Create Log Record */
	if (ReplicatedOp) 
	    if (errorCode = SpoolVMLogRecord(vlist, pv, volptr, StoreId,
					     ViceSymLink_OP, 
					     NewName, Fid->Vnode, Fid->Unique,
					     client?client->Id:0)) 
		LogMsg(0, SrvDebugLevel, stdout, 
		       "ViceSymLink: Error %d in SpoolVMLogRecord\n",
		       errorCode);
    }
#ifdef _TIMECALLS_
    END_NSC_TIMING(SpoolVMLogRecord);
#endif _TIMECALLS_    
    
FreeLocks: 
    /* Put objects. */
    {
	if (errorCode) {
	    cpent *cpe = CopPendingMan->findanddeq(StoreId);
	    if (cpe) {
		CopPendingMan->remove(cpe);
		delete cpe;
	    }
	}

	PutObjects(errorCode, volptr, SHARED_LOCK, vlist, deltablocks, 1);
    }

    LogMsg(2, SrvDebugLevel, stdout, "ViceSymLink returns %s", ViceErrorMsg(errorCode));
END_TIMING(SymLink_Total);
#ifdef _TIMECALLS_
    END_NSC_TIMING(SymLink_Total);
#endif _TIMECALLS_
    Fid->Volume	= VSGVolnum;		/* Fid is an IN/OUT parameter; re-translate it. */
    return(errorCode);
}



/*  *****  Utility Routines  *****  */

/* Initialize return status from a vnode. */
void SetStatus(Vnode *vptr, ViceStatus *status, Rights rights, Rights anyrights)
{
    status->InterfaceVersion = 1;

    /* fill in VnodeType  */
    if (vptr->disk.type == vFile) {
	status->VnodeType = File;
    }
    else {
	if (vptr->disk.type == vDirectory) {
	    status->VnodeType = Directory;
	}
	else {
	    if (vptr->disk.type == vSymlink) {
		status->VnodeType = SymbolicLink;
	    }
	    else {
		status->VnodeType = Invalid;	/*invalid type field */
	    }
	}
    }

    status->LinkCount = vptr->disk.linkCount;
    status->Length = vptr->disk.length;
    status->DataVersion = vptr->disk.dataVersion;
    status->VV = vptr->disk.versionvector;
    status->Date = vptr->disk.unixModifyTime;
    status->Author = vptr->disk.author;
    status->Owner = vptr->disk.owner;
    status->Mode = vptr->disk.modeBits;
    status->MyAccess = rights;
    status->AnyAccess = anyrights;
    status->CallBack = NoCallBack;
    status->vparent = vptr->disk.vparent;
    status->uparent = vptr->disk.uparent;

}


int GetRights(PRS_InternalCPS *CPS, AL_AccessList *ACL, int ACLSize,
	       Rights *rights, Rights *anyrights )
{
    static int  anyid = -1;
    static  PRS_InternalCPS * anyCPS = 0;

    if (anyid == -1) {
	if (AL_NameToId("anyuser", &anyid) != 0) {
	    LogMsg(0, SrvDebugLevel, stdout,"UserID anyuser not known");
	    anyid = 0;
	}
	else {
	    if (AL_GetInternalCPS(anyid, &anyCPS) != 0) {
		LogMsg(0, SrvDebugLevel, stdout,"UserID anyuser no CPS");
		anyid = 0;
	    }
	}
    }
    if (anyid != 0) {
	if (AL_CheckRights(ACL, anyCPS, (int *)anyrights) != 0) {
	    LogMsg(0, SrvDebugLevel, stdout,"CheckRights failed");
	    anyrights = 0;
	}
    }
    else {
	*anyrights = -1;
    }

    if (AL_CheckRights(ACL, CPS, (int *)rights) != 0) {
	*rights = 0;
    }
    return(0);
}

/*
  BEGIN_HTML
  <a name="GrabFsObj"><strong>Get a pointer to (and lock) a particular volume and vnode </strong></a> 
  END_HTML
*/
PRIVATE int GrabFsObj(ViceFid *fid, Volume **volptr, Vnode **vptr, 
		      int lock, int ignoreIncon, int VolumeLock) {

    int GotVolume = 0;
    /* Get the volume. */
    if ((*volptr) == 0) {
	int errorCode = GetVolObj(fid->Volume, volptr, VolumeLock, 0, 0);
	if (errorCode) {
	    LogMsg(0, SrvDebugLevel, stdout, "GrabFsObj, GetVolObj error %s", 
		    ViceErrorMsg(errorCode));
	    return(errorCode);
	}
	GotVolume = 1;
    }
    
    /* Get the vnode.  */
    if ((*vptr == 0)) {
	int errorCode = 0;
	*vptr = VGetVnode((Error *)&errorCode, *volptr, fid->Vnode, fid->Unique, lock, ignoreIncon, 0);
	if (errorCode) {
	    LogMsg(1, SrvDebugLevel, stdout, "GrabFsObj: VGetVnode error %s", ViceErrorMsg(errorCode));
	    if (GotVolume){
		LogMsg(1, SrvDebugLevel, stdout, "GrabFsObj: Releasing volume 0x%x", V_id(*volptr));
		PutVolObj(volptr, VolumeLock, 0);
	    }
	    return(errorCode);
	}
    }
    
    /* Sanity check the uniquifiers. */
    if ((*vptr)->disk.uniquifier != fid->Unique) {
	LogMsg(0, SrvDebugLevel, stdout, "GrabFsObj, uniquifier mismatch, disk = %x, fid = %x",
		(*vptr)->disk.uniquifier, fid->Unique);
	if (GotVolume){
	    LogMsg(0, SrvDebugLevel, stdout, "GrabFsObj: Releasing volume 0x%x", V_id(*volptr));
	    PutVolObj(volptr, VolumeLock, 0);
	}
	return(EINVAL);
    }
    
    return(0);
}


/* Formerly CheckVnode(). */
/* ignoreBQ parameter is obsolete - not used any longer */
/*
  BEGIN_HTML
  <a name="GetFsObj"><strong>Get a filesystem object</strong></a> 
  END_HTML
*/
int GetFsObj(ViceFid *fid, Volume **volptr, Vnode **vptr,
	     int lock, int VolumeLock, int ignoreIncon, int ignoreBQ) {
    /* Sanity check the Fid. */
    if (fid->Volume == 0 || fid->Vnode == 0 || fid->Unique == 0)
	return(EINVAL);

	/* Grab the volume and vnode with the appropriate lock. */
    int errorCode = GrabFsObj(fid, volptr, vptr, lock, ignoreIncon, VolumeLock);
    if (errorCode) return(errorCode);

    return(0);
}
int GetVolObj(VolumeId Vid, Volume **volptr, 
	      int LockLevel, int Enque, int LockerAddress) {
    
    int errorCode = 0;
    *volptr = 0;

    *volptr = VGetVolume((Error *)&errorCode, Vid);
    if (errorCode) {
	LogMsg(0, SrvDebugLevel, stdout, "GetVolObj: VGetVolume(%x) error %s",
		Vid, ViceErrorMsg(errorCode));
	goto FreeLocks;
    }

    switch(LockLevel) {
	case VOL_NO_LOCK:
	    break;

	case VOL_SHARED_LOCK:
	    if (V_VolLock(*volptr).IPAddress != 0) {
		LogMsg(0, SrvDebugLevel, stdout, "GetVolObj: Volume (%x) already write locked", Vid);
		VPutVolume(*volptr);
		*volptr = 0;
		errorCode = EWOULDBLOCK;
		goto FreeLocks;
	    }
	    ObtainReadLock(&(V_VolLock(*volptr).VolumeLock));
	    break;

	case VOL_EXCL_LOCK:
	    assert(LockerAddress);
	    if (V_VolLock(*volptr).IPAddress != 0) {
		LogMsg(0, SrvDebugLevel, stdout, "GetVolObj: Volume (%x) already write locked", Vid);
		VPutVolume(*volptr);
		*volptr = 0;
		errorCode = EWOULDBLOCK;
		goto FreeLocks;
	    }
	    V_VolLock(*volptr).IPAddress = LockerAddress;
	    ObtainWriteLock(&(V_VolLock(*volptr).VolumeLock));
	    assert(V_VolLock(*volptr).IPAddress == LockerAddress);
	    if (Enque) {
		lqent *lqep = new lqent(Vid);
		LockQueueMan->add(lqep);
	    }
	    break;
	default:
	    assert(0);
    }

FreeLocks:
    LogMsg(9, SrvDebugLevel, stdout, "GetVolObj: returns %d", errorCode);

    return(errorCode);
}

/*
  BEGIN_HTML
  <a name="PutVolObj"><strong>Unlock a volume</strong></a> 
  END_HTML
*/
void PutVolObj(Volume **volptr, int LockLevel, int Dequeue)
{
    if (*volptr == 0) return;
    switch (LockLevel) {
      case VOL_NO_LOCK:
	break;
      case VOL_SHARED_LOCK:
	LogMsg(9, SrvDebugLevel, stdout, "PutVolObj: One less locker");
	ReleaseReadLock(&(V_VolLock(*volptr).VolumeLock));
	break;
      case VOL_EXCL_LOCK:
	if (Dequeue) {
	    lqent *lqep = LockQueueMan->findanddeq(V_id(*volptr));
	    if (!lqep) 
		LogMsg(0, SrvDebugLevel, stdout, "PutVolObj: Couldn't find entry %x on lock queue", 
			V_id(*volptr));
	    else {
		LockQueueMan->remove(lqep);
		delete lqep;
	    }
	}
	if (V_VolLock(*volptr).IPAddress) {
	    V_VolLock(*volptr).IPAddress = 0;
	    ReleaseWriteLock(&(V_VolLock(*volptr).VolumeLock));
	}
	break;
      default:
	assert(0);
    }

    VPutVolume(*volptr);
    *volptr = 0;
}


/* Permits only Strong Equality. */
PRIVATE int NormalVCmp(int ReplicatedOp, VnodeType type, void *arg1, void *arg2) {
    int errorCode = 0;

    if (ReplicatedOp) {
	ViceVersionVector *vva = (ViceVersionVector *)arg1;
	ViceVersionVector *vvb = (ViceVersionVector *)arg2;

	if (VV_Cmp(vva, vvb) != VV_EQ)
	    errorCode = EINCOMPATIBLE;
    }
    else {
	FileVersion fva = *((FileVersion *)arg1);
	FileVersion fvb = *((FileVersion *)arg2);

	if (fva != fvb)
	    errorCode = EINCOMPATIBLE;
    }

    return(errorCode);
}


/* Permits Strong or Weak Equality (replicated case). */
/* Permits anything (non-replicated case). */
PRIVATE int StoreVCmp(int ReplicatedOp, VnodeType type, void *arg1, void *arg2) {
    assert(type == vFile);

    int errorCode = 0;

    if (ReplicatedOp) {
	ViceVersionVector *vva = (ViceVersionVector *)arg1;
	ViceVersionVector *vvb = (ViceVersionVector *)arg2;

	if (!(vva->StoreId.Host == vvb->StoreId.Host &&
	      vva->StoreId.Uniquifier == vvb->StoreId.Uniquifier))
	    errorCode = EINCOMPATIBLE;
    }
    else {
	/* No checks. */
    }

    return(errorCode);
}


/* This ought to be folded into the CheckSemantics or the Perform routines!  -JJK */
void HandleWeakEquality(Volume *volptr, Vnode *vptr, ViceVersionVector *vv) {
    ViceVersionVector *vva = &Vnode_vv(vptr);
    ViceVersionVector *vvb = vv;

    if ((vva->StoreId.Host == vvb->StoreId.Host &&
	  vva->StoreId.Uniquifier == vvb->StoreId.Uniquifier) &&
	 (VV_Cmp(vva, vvb) != VV_EQ)) {
	/* Derive "difference vector" and apply it to both vnode and volume vectors. */
	ViceVersionVector DiffVV;
	{
	    ViceVersionVector *vvs[VSG_MEMBERS];
	    bzero(vvs, (int)(VSG_MEMBERS * sizeof(ViceVersionVector *)));
	    vvs[0] = vva;
	    vvs[1] = vvb;
	    GetMaxVV(&DiffVV, vvs, -1);
	}
	SubVVs(&DiffVV, vva);
	AddVVs(&Vnode_vv(vptr), &DiffVV);
	AddVVs(&V_versionvector(volptr), &DiffVV);
    }
}


#define OWNERREAD 0400
#define OWNERWRITE 0200
#define ANYREAD 0004
#define ANYWRITE 0002

int CheckWriteMode(ClientEntry *client, Vnode *vptr)
{
    if(vptr->disk.type != vDirectory)
	if (!(OWNERWRITE & vptr->disk.modeBits))
	    return(EACCES);
    return(0);
}


int CheckReadMode(ClientEntry *client, Vnode *vptr)
{
    if(vptr->disk.type != vDirectory)
	if (!(OWNERREAD & vptr->disk.modeBits))
	    return(EACCES);
    return(0);
}


/*
  BEGIN_HTML
  <a name="CopyOnWrite"><strong>Make a copy of a vnode if it is cloned and it needs to be updated </strong></a> 
  END_HTML
*/
void CopyOnWrite(Vnode *vptr, Volume *volptr)
{
    Inode    ino;
    int	     rdlen;
    int      wrlen;
    int      size;
    char   * buff;

    if (vptr->disk.type == vDirectory){
	DirInode *newInode;
	assert(vptr->disk.inodeNumber != 0);
	LogMsg(0, SrvDebugLevel, stdout, "CopyOnWrite: Copying directory vnode = %d", 
	    vptr->vnodeNumber);
	VMCopyDirInode((DirInode *)(vptr->disk.inodeNumber), &newInode);
	LogMsg(0, SrvDebugLevel, stdout, "CopyOnWrite: Old dir inode = 0x%x new vm dinode = 0x%x", 
	    vptr->disk.inodeNumber, newInode);
	newInode->refcount = 1;
	vptr->disk.inodeNumber = (Inode)newInode;
	vptr->disk.cloned = 0;
    }
    else{
	LogMsg(0, SrvDebugLevel, stdout, "CopyOnWrite: Copying inode for files (vnode%d)",
	    vptr->vnodeNumber);
	size = (int) vptr->disk.length;
	ino = icreate((int) V_device(volptr), 0, (int) V_id(volptr),
		      (int) vptr->vnodeNumber, (int) vptr->disk.uniquifier, 
		      (int) vptr->disk.dataVersion);
	assert(ino > 0);
	if (size > 0) {
	  buff = (char *)malloc(size);
	  assert(buff != 0);
	  rdlen = iread((int) V_device(volptr), (int) vptr->disk.inodeNumber, 
			(int) V_parentId(volptr), 0, buff, size);
START_TIMING(CopyOnWrite_iwrite);
	  wrlen = iwrite((int) V_device(volptr), (int) ino, 
			 (int) V_parentId(volptr), 0, buff, size);
END_TIMING(CopyOnWrite_iwrite);
	  assert(rdlen == wrlen);
	  free(buff);
	}

/*
	START_TIMING(CopyOnWrite_idec);
	assert(!(idec(V_device(volptr), vptr->disk.inodeNumber, V_parentId(volptr))));
	END_TIMING(CopyOnWrite_idec);
*/

	vptr->disk.inodeNumber = ino;
	vptr->disk.cloned = 0;
    }
}


int SystemUser(ClientEntry *client)
{
    if(AL_IsAMember(SystemId,client->CPS) == 0) {
	return(0);
    }
    return(1);
}


int AdjustDiskUsage(Volume *volptr, int length)
{
    int		rc;
    int		nc;
    VAdjustDiskUsage((Error *)&rc, volptr, length);
    if(rc) {
	VAdjustDiskUsage((Error *)&nc, volptr, -length);
	if(rc == ENOSPC) {
	    LogMsg(0, SrvDebugLevel, stdout,"Partition %s that contains volume %u is full",
		    volptr->partition->name, V_id(volptr));
	    return(rc);
	}
	if(rc == EDQUOT) {
	    LogMsg(0, SrvDebugLevel, stdout,"Volume %u (%s) is full",
		    V_id(volptr), V_name(volptr));
	    return(rc);
	}
	LogMsg(0, SrvDebugLevel, stdout,"Got error return %s from VAdjustDiskUsage",ViceErrorMsg(rc));
	return(rc);
    }
    return(0);
}

int CheckDiskUsage(Volume *volptr, int length)
{
    int	rc;
    VCheckDiskUsage((Error *)&rc, volptr, length);
    if (rc == 0) return 0;
    if (rc == ENOSPC){
	LogMsg(0, SrvDebugLevel, stdout,"Partition %s that contains volume %u is full",
		    volptr->partition->name, V_id(volptr));
	return (rc);
    }
    else {
	LogMsg(0, SrvDebugLevel, stdout, "Volume %u (%s) is full",
		    V_id(volptr), V_name(volptr));
	return(rc);
    }
}


void ChangeDiskUsage(Volume *volptr, int length)
{
    int rc;
    VAdjustDiskUsage((Error *)&rc, volptr, length);
}


/* ************************************************** */

/*
  BEGIN_HTML
  <a name="ValidateParms"><strong>Validate the parameters of the RPC</strong></a> 
  END_HTML
*/
int ValidateParms(RPC2_Handle RPCid, ClientEntry **client,
		   int ReplicatedOp, VolumeId *Vidp, RPC2_CountedBS *PiggyBS) {
    int errorCode = 0;

    /* 1. Apply PiggyBacked COP2 operations. */
    if (ReplicatedOp)
	if ((PiggyBS) && (PiggyBS->SeqLen > 0) && (errorCode = (int)ViceCOP2(RPCid, PiggyBS)))
	    return(errorCode);

    /* 2. Map RPC handle to client structure. */
    if ((errorCode = (int) RPC2_GetPrivatePointer(RPCid, (char **)client)) != RPC2_SUCCESS) {
	LogMsg(0, SrvDebugLevel, stdout, "ValidateParms: GetPrivatePointer failed (%d)", errorCode);
	return(errorCode);
    }
    if (*client == 0 || (*client)->DoUnbind) {
	LogMsg(0, SrvDebugLevel, stdout, "ValidateParms: GetPrivatePointer --> Null client");
	return(EINVAL);
    }

    /* 3. Translate group to read/write volume id. */
    if (ReplicatedOp) {
	VolumeId GroupVid = *Vidp;
	if (!XlateVid(Vidp)) {
	    LogMsg(1, SrvDebugLevel, stdout, "ValidateParms: failed to translate VSG %x", GroupVid);
	    return(EINVAL);
	}
	LogMsg(10, SrvDebugLevel, stdout, "ValidateParms: %x --> %x", GroupVid, *Vidp);
    }

    return(0);
}


int AllocVnode(Vnode **vptr, Volume *volptr, ViceDataType vtype, ViceFid *Fid,
		ViceFid *pFid, UserId ClientId, RPC2_Unsigned AllocHost, int *blocks) {
    int errorCode = 0;
    Error fileCode = 0;
    VolumeId VSGVolnum = 0;
    int ReplicatedOp = (AllocHost != 0);
    *vptr = 0;
    *blocks = 0;

START_TIMING(AllocVnode_Total);
    LogMsg(10, SrvDebugLevel, stdout, "AllocVnode: Fid = (%x.%x.%x), type = %d, pFid = (%x.%x.%x), Owner = %d",
	     Fid->Volume, Fid->Vnode, Fid->Unique, vtype,
	     pFid->Volume, pFid->Vnode, pFid->Unique, ClientId);

    /* Validate parameters. */
    if (ReplicatedOp)
	assert(V_id(volptr) == Fid->Volume);
    assert(V_id(volptr) == pFid->Volume);
    assert(vtype == vFile || vtype == vDirectory || vtype == vSymlink);

    /* Allocate/Retrieve the new vnode. */
    int tblocks = (vtype == vFile)
      ? EMPTYFILEBLOCKS
      : (vtype == vDirectory)
      ? EMPTYDIRBLOCKS
      : EMPTYSYMLINKBLOCKS;
    if (ReplicatedOp) {
	if (AllocHost == ThisHostAddr) {
	    if (errorCode = GetFsObj(Fid, &volptr, vptr, WRITE_LOCK, SHARED_LOCK, 0, 0))
		goto FreeLocks;
	}
	else {
	    if (errorCode = AdjustDiskUsage(volptr, tblocks))
		goto FreeLocks;
	    *blocks = tblocks;

	    *vptr = VAllocVnode((Error *)&errorCode, volptr,
				vtype, Fid->Vnode, Fid->Unique);
	    if (errorCode != 0)
		goto FreeLocks;
	}
    }
    else {
	if (errorCode = AdjustDiskUsage(volptr, tblocks))
	    goto FreeLocks;
	*blocks = tblocks;

	*vptr = VAllocVnode((Error *)&errorCode, volptr, vtype);
	if (errorCode != 0)
	    goto FreeLocks;

	/* Set up Fid from the new vnode. */
	Fid->Volume = V_id(volptr);
	Fid->Vnode = (*vptr)->vnodeNumber;
	Fid->Unique = (*vptr)->disk.uniquifier;
    }

    /* Initialize the new vnode. */
    (*vptr)->disk.inodeNumber = (Inode)NEWVNODEINODE;
    (*vptr)->disk.dataVersion = (vtype == vFile ? 0 : 1);
    InitVV(&Vnode_vv((*vptr)));
    (*vptr)->disk.vparent = pFid->Vnode;
    (*vptr)->disk.uparent = pFid->Unique;
    (*vptr)->disk.length = 0;
    (*vptr)->disk.linkCount = (vtype == vDirectory ? 2 : 1);
    (*vptr)->disk.author = ClientId;
    (*vptr)->disk.unixModifyTime = time(0);
    (*vptr)->disk.owner = ClientId;
    (*vptr)->disk.modeBits = 0;

FreeLocks:
    /* Put new vnode ONLY on error. */
    if (errorCode) {
	if (*blocks != 0) {
	    if (AdjustDiskUsage(volptr, -(*blocks)) != 0)
		LogMsg(0, SrvDebugLevel, stdout, "AllocVnode: AdjustDiskUsage(%x, %d) failed",
			V_id(volptr), -(*blocks));

	    *blocks = 0;
	}

	if (*vptr) {
START_TIMING(AllocVnode_Transaction);
	    int status = 0;
	    CAMLIB_BEGIN_TOP_LEVEL_TRANSACTION_2(CAM_TRAN_NV_SERVER_BASED);
	    VPutVnode(&fileCode, *vptr);
	    assert(fileCode == 0);
	    *vptr = 0;
	    CAMLIB_END_TOP_LEVEL_TRANSACTION_2(CAM_PROT_TWO_PHASED, status);
END_TIMING(AllocVnode_Transaction);
	}
    }

    LogMsg(2, SrvDebugLevel, stdout, "AllocVnode returns %s", ViceErrorMsg(errorCode));
END_TIMING(AllocVnode_Total);
    return(errorCode);
}


int CheckFetchSemantics(ClientEntry *client, Vnode **avptr, Vnode **vptr,
			 Volume **volptr, Rights *rights, Rights *anyrights) {
    int errorCode = 0;
    ViceFid Fid;
    Fid.Volume = V_id(*volptr);
    Fid.Vnode = (*vptr)->vnodeNumber;
    Fid.Unique = (*vptr)->disk.uniquifier;
    int IsOwner = (client->Id == (*vptr)->disk.owner);

    /* Concurrency-control checks. */
    {
	/* Not applicable. */
    }

    /* Integrity checks. */
    {
	/* Not applicable. */
    }

    /* Protection checks. */
    {
	/* Get the access list. */
	AL_AccessList *aCL = 0;
	int aCLSize = 0;
	SetAccessList(*avptr, aCL, aCLSize);

	/* Get this client's rights. */
	Rights t_rights; if (rights == 0) rights = &t_rights;
	Rights t_anyrights; if (anyrights == 0) anyrights = &t_anyrights;
	assert(GetRights(client->CPS, aCL, aCLSize, rights, anyrights) == 0);

	/* LOOKUP or READ permission (normally) required. */
	/* READ mode-bits also (normally) required for files. */
	if (SystemUser(client)) {
	    if (((*vptr)->disk.type == vDirectory && !(*rights & PRSFS_LOOKUP)) ||
		((*vptr)->disk.type != vDirectory && !(*rights & PRSFS_READ))) {
		LogMsg(1, SrvDebugLevel, stdout, "CheckFetchSemantics: rights violation (%x : %x) (%x.%x.%x)",
			*rights, *anyrights,
			Fid.Volume, Fid.Vnode, Fid.Unique);
		return(EACCES);
	    }

	    if (!IsOwner && (*vptr)->disk.type == vFile) {
		if (CheckReadMode(client, *vptr)) {
		    LogMsg(1, SrvDebugLevel, stdout, "CheckFetchSemantics: mode-bits violation (%x.%x.%x)",
			    Fid.Volume, Fid.Vnode, Fid.Unique);
		    return(EACCES);
		}
	    }
	}
    }

    return(0);
}


int CheckGetAttrSemantics(ClientEntry *client, Vnode **avptr, Vnode **vptr,
			   Volume **volptr, Rights *rights, Rights *anyrights) {
    int errorCode = 0;
    ViceFid Fid;
    Fid.Volume = V_id(*volptr);
    Fid.Vnode = (*vptr)->vnodeNumber;
    Fid.Unique = (*vptr)->disk.uniquifier;

    /* Concurrency-control checks. */
    {
	/* Not applicable. */
    }

    /* Integrity checks. */
    {
	/* Not applicable. */
    }

    /* Protection checks. */
    {
	/* Get the access list. */
	AL_AccessList *aCL = 0;
	int aCLSize = 0;
	SetAccessList(*avptr, aCL, aCLSize);

	/* Get this client's rights. */
	Rights t_rights; if (rights == 0) rights = &t_rights;
	Rights t_anyrights; if (anyrights == 0) anyrights = &t_anyrights;
	assert(GetRights(client->CPS, aCL, aCLSize, rights, anyrights) == 0);
    }

    return(0);
}


int CheckGetACLSemantics(ClientEntry *client, Vnode **vptr,
			  Volume **volptr, Rights *rights, Rights *anyrights,
			  RPC2_BoundedBS *AccessList, RPC2_String *eACL) {
    int errorCode = 0;
    ViceFid Fid;
    Fid.Volume = V_id(*volptr);
    Fid.Vnode = (*vptr)->vnodeNumber;
    Fid.Unique = (*vptr)->disk.uniquifier;

    /* Concurrency-control checks. */
    {
	/* Not applicable. */
    }

    /* Integrity checks. */
    {
	if ((*vptr)->disk.type != vDirectory) {
	    LogMsg(0, SrvDebugLevel, stdout, "CheckGetACLSemantics: non-directory (%x.%x.%x)",
		    Fid.Volume, Fid.Vnode, Fid.Unique);
	    return(ENOTDIR);
	}
	if (AccessList->MaxSeqLen == 0) {
	    LogMsg(0, SrvDebugLevel, stdout, "CheckGetACLSemantics: zero-len ACL (%x.%x.%x)",
		    Fid.Volume, Fid.Vnode, Fid.Unique);
	    return(EINVAL);
	}
    }

    /* Protection checks. */
    {
	/* Get the access list. */
	AL_AccessList *aCL = 0;
	int aCLSize = 0;
	SetAccessList(*vptr, aCL, aCLSize);

	/* Get this client's rights. */
	Rights t_rights; if (rights == 0) rights = &t_rights;
	Rights t_anyrights; if (anyrights == 0) anyrights = &t_anyrights;
	assert(GetRights(client->CPS, aCL, aCLSize, rights, anyrights) == 0);

	assert(AL_Externalize(aCL, (AL_ExternalAccessList *)eACL) == 0);
	int eACLlen = (int)(strlen((char *)*eACL) + 1);
	if (eACLlen > AccessList->MaxSeqLen) {
	    LogMsg(0, SrvDebugLevel, stdout, "CheckGetACLSemantics: eACLlen (%d) > ACL->MaxSeqLen (%d) (%x.%x.%x)",
		    eACLlen, AccessList->MaxSeqLen,
		    Fid.Volume, Fid.Vnode, Fid.Unique);
	    return(E2BIG);
	}
	LogMsg(10, SrvDebugLevel, stdout, "CheckGetACLSemantics: ACL is:\n%s", *eACL);
    }

    return(0);
}


int CheckStoreSemantics(ClientEntry *client, Vnode **avptr, Vnode **vptr,
			 Volume **volptr, int ReplicatedOp, VCP VCmpProc,
			 ViceVersionVector *VV, FileVersion DataVersion,
			 Rights *rights, Rights *anyrights) {
    int errorCode = 0;
    ViceFid Fid;
    Fid.Volume = V_id(*volptr);
    Fid.Vnode = (*vptr)->vnodeNumber;
    Fid.Unique = (*vptr)->disk.uniquifier;
    int IsOwner = (client->Id == (*vptr)->disk.owner);
    int Virginal = ((*vptr)->disk.inodeNumber == 0);

    /* Concurrency-control checks. */
    if (VCmpProc) {
	void *arg1 = (ReplicatedOp ? (void *)&Vnode_vv(*vptr) : (void *)&(*vptr)->disk.dataVersion);
	void *arg2 = (ReplicatedOp ? (void *)VV : (void *)&DataVersion);
	if (errorCode = VCmpProc(ReplicatedOp, (*vptr)->disk.type, arg1, arg2)) {
	    LogMsg(0, SrvDebugLevel, stdout, "CheckStoreSemantics: (%x.%x.%x), VCP error (%d)",
		    Fid.Volume, Fid.Vnode, Fid.Unique, errorCode);
	    return(errorCode);
	}
    }

    /* Integrity checks. */
    {
	if ((*vptr)->disk.type != vFile) {
	    LogMsg(0, SrvDebugLevel, stdout, "CheckStoreSemantics: (%x.%x.%x) not a file",
		    Fid.Volume, Fid.Vnode, Fid.Unique);
	    return(EISDIR);
	}
    }

    /* Protection checks. */
    {
	/* Get the access list. */
	AL_AccessList *aCL = 0;
	int aCLSize = 0;
	SetAccessList(*avptr, aCL, aCLSize);

	/* Get this client's rights. */
	Rights t_rights; if (rights == 0) rights = &t_rights;
	Rights t_anyrights; if (anyrights == 0) anyrights = &t_anyrights;
	assert(GetRights(client->CPS, aCL, aCLSize, rights, anyrights) == 0);

	/* WRITE permission (normally) required. */
	if (!(*rights & PRSFS_WRITE) && !(IsOwner && Virginal)) {
	    LogMsg(0, SrvDebugLevel, stdout, "CheckStoreSemantics: rights violation (%x : %x) (%x.%x.%x)",
		    *rights, *anyrights, Fid.Volume, Fid.Vnode, Fid.Unique);
	    return(EACCES);
	}
    }

    return(0);
}


int CheckNewSetAttrSemantics(ClientEntry *client, Vnode **avptr, Vnode **vptr,
			     Volume **volptr, int ReplicatedOp, VCP VCmpProc,
			     RPC2_Integer Length, Date_t Mtime,
			     UserId Owner, RPC2_Unsigned Mode, 
			     RPC2_Integer Mask,
			     ViceVersionVector *VV, FileVersion DataVersion,
			     Rights *rights, Rights *anyrights) {
    int errorCode = 0;
    ViceFid Fid;
    Fid.Volume = V_id(*volptr);
    Fid.Vnode = (*vptr)->vnodeNumber;
    Fid.Unique = (*vptr)->disk.uniquifier;
    int IsOwner = (client->Id == (*vptr)->disk.owner);
    int chmodp = Mask & SET_MODE;
    int chownp = Mask & SET_OWNER;
    int truncp = Mask & SET_LENGTH;
    int utimesp = Mask & SET_TIME;
    int Virginal = ((*vptr)->disk.inodeNumber == 0);

    /* Concurrency-control checks. */
    if (VCmpProc) {
	void *arg1 = (ReplicatedOp ? (void *)&Vnode_vv(*vptr) : (void *)&(*vptr)->disk.dataVersion);
	void *arg2 = (ReplicatedOp ? (void *)VV : (void *)&DataVersion);
	if (errorCode = VCmpProc(ReplicatedOp, (*vptr)->disk.type, arg1, arg2)) {
	    LogMsg(0, SrvDebugLevel, stdout, "CheckNewSetAttrSemantics: (%x.%x.%x), VCP error (%d)",
		    Fid.Volume, Fid.Vnode, Fid.Unique, errorCode);
	    return(errorCode);
	}
    }

    /* Integrity checks. */
    {
	if (truncp) { 
	    if ((*vptr)->disk.type != vFile) {
		LogMsg(0, SrvDebugLevel, stdout, "CheckNewSetAttrSemantics: non-file truncate (%x.%x.%x)",
			Fid.Volume, Fid.Vnode, Fid.Unique);
		return(EISDIR);
	    }
	    if (Length > (*vptr)->disk.length) {
		LogMsg(0, SrvDebugLevel, stdout, "CheckNewSetAttrSemantics: truncate length bad (%x.%x.%x)",
			Fid.Volume, Fid.Vnode, Fid.Unique);
		return(EINVAL);
	    }
	}

	/* just log a message if nothing is changing - don't be paranoid about returning an error */
	if (chmodp + chownp + truncp + utimesp == 0) {
	    LogMsg(0, SrvDebugLevel, stdout, "CheckNewSetAttrSemantics: no attr set (%x.%x.%x)",
		    Fid.Volume, Fid.Vnode, Fid.Unique);
	}
    }

    /* Protection checks. */
    {
	/* Get the access list. */
	AL_AccessList *aCL = 0;
	int aCLSize = 0;
	SetAccessList(*avptr, aCL, aCLSize);

	/* Get this client's rights. */
	Rights t_rights; if (rights == 0) rights = &t_rights;
	Rights t_anyrights; if (anyrights == 0) anyrights = &t_anyrights;
	assert(GetRights(client->CPS, aCL, aCLSize, rights, anyrights) == 0);

	if (IsOwner && Virginal) {
	    /* Bypass protection checks on first store after a create EXCEPT for chowns. */
	    if (chownp) {
		LogMsg(0, SrvDebugLevel, stdout, "CheckNewSetAttrSemantics: owner chown'ing virgin (%x.%x.%x)",
			Fid.Volume, Fid.Vnode, Fid.Unique);
		return(EACCES);
	    }
	}
	else {
	    if (SystemUser(client)) {
		/* System users are subject to no further permission checks. */
		/* Other users require WRITE permission for file, */
		/* INSERT | DELETE for directories. */
		if ((*vptr)->disk.type == vDirectory) {
		    if (!(*rights & (PRSFS_INSERT | PRSFS_DELETE))) {
			LogMsg(0, SrvDebugLevel, stdout, "CheckNewSetAttrSemantics: rights violation (%x : %x) (%x.%x.%x)",
				*rights, *anyrights,
				Fid.Volume, Fid.Vnode, Fid.Unique);
			return(EACCES);
		    }
		}
		else {
		    if (!(*rights & PRSFS_WRITE)) {
			LogMsg(0, SrvDebugLevel, stdout, "CheckNewSetAttrSemantics: rights violation (%x : %x) (%x.%x.%x)",
				*rights, *anyrights,
				Fid.Volume, Fid.Vnode, Fid.Unique);
			return(EACCES);
		    }
		}

		if (chmodp) {
		    /* No further access checks. */
		}

		if (chownp && !IsOwner) {
		    LogMsg(0, SrvDebugLevel, stdout, "CheckNewSetAttrSemantics: non-owner chown'ing (%x.%x.%x)",
			    Fid.Volume, Fid.Vnode, Fid.Unique);
		    return(EACCES);
		}

		if (truncp && CheckWriteMode(client, (*vptr)) != 0) {
		    LogMsg(0, SrvDebugLevel, stdout, "CheckNewSetAttrSemantics: truncating (%x.%x.%x)",
			    Fid.Volume, Fid.Vnode, Fid.Unique);
		    return(EACCES);
		}

		if (utimesp) {
		    /* No further access checks. */
		}
	    }
	}
    }

    return(0);
}

int CheckSetACLSemantics(ClientEntry *client, Vnode **vptr, Volume **volptr,
			   int ReplicatedOp, VCP VCmpProc,
			   ViceVersionVector *VV, FileVersion DataVersion,
			   Rights *rights, Rights *anyrights,
			   RPC2_CountedBS *AccessList, AL_AccessList **newACL) {
    int errorCode = 0;
    ViceFid Fid;
    Fid.Volume = V_id(*volptr);
    Fid.Vnode = (*vptr)->vnodeNumber;
    Fid.Unique = (*vptr)->disk.uniquifier;
    int IsOwner = (client->Id == (*vptr)->disk.owner);
    int Virginal = ((*vptr)->disk.inodeNumber == 0);

    /* Concurrency-control checks. */
    if (VCmpProc) {
	void *arg1 = (ReplicatedOp ? (void *)&Vnode_vv(*vptr) : (void *)&(*vptr)->disk.dataVersion);
	void *arg2 = (ReplicatedOp ? (void *)VV : (void *)&DataVersion);
	if (errorCode = VCmpProc(ReplicatedOp, (*vptr)->disk.type, arg1, arg2)) {
	    LogMsg(0, SrvDebugLevel, stdout, "CheckSetACLSemantics: (%x.%x.%x), VCP error (%d)",
		    Fid.Volume, Fid.Vnode, Fid.Unique, errorCode);
	    return(errorCode);
	}
    }

    /* Integrity checks. */
    {
	if ((*vptr)->disk.type != vDirectory) {
	    LogMsg(0, SrvDebugLevel, stdout, "CheckSetACLSemantics: non-directory (%x.%x.%x)",
		    Fid.Volume, Fid.Vnode, Fid.Unique);
	    return(ENOTDIR);
	}
	if (AccessList->SeqLen == 0) {
	    LogMsg(0, SrvDebugLevel, stdout, "CheckSetACLSemantics: zero-len ACL (%x.%x.%x)",
		    Fid.Volume, Fid.Vnode, Fid.Unique);
	    return(EINVAL);
	}
    }

    /* Protection checks. */
    {
	/* Get the access list. */
	AL_AccessList *aCL = 0;
	int aCLSize = 0;
	SetAccessList(*vptr, aCL, aCLSize);

	/* Get this client's rights. */
	Rights t_rights; if (rights == 0) rights = &t_rights;
	Rights t_anyrights; if (anyrights == 0) anyrights = &t_anyrights;
	assert(GetRights(client->CPS, aCL, aCLSize, rights, anyrights) == 0);

	/* ADMINISTER permission (normally) required. */
	if (!(*rights & PRSFS_ADMINISTER) && !IsOwner && SystemUser(client)) {
	    LogMsg(0, SrvDebugLevel, stdout, "CheckSetACLSemantics: rights violation (%x : %x) (%x.%x.%x)",
		    *rights, *anyrights,
		    Fid.Volume, Fid.Vnode, Fid.Unique);
	    return(EACCES);
	}
	if (AL_Internalize((AL_ExternalAccessList) AccessList->SeqBody, newACL) != 0) {
	    LogMsg(0, SrvDebugLevel, stdout, "CheckSetACLSemantics: ACL internalize failed (%x.%x.%x)",
		    Fid.Volume, Fid.Vnode, Fid.Unique);
	    return(EINVAL);
	}
	if ((*newACL)->MySize + 4 > aCLSize) {
	    LogMsg(0, SrvDebugLevel, stdout, "CheckSetACLSemantics: ACL too big (%x.%x.%x)",
		    Fid.Volume, Fid.Vnode, Fid.Unique);
	    return(E2BIG);
	}
    }

    return(0);
}


int CheckCreateSemantics(ClientEntry *client, Vnode **dirvptr, Vnode **vptr, char *Name,
			   Volume **volptr, int ReplicatedOp, VCP VCmpProc,
			   ViceStatus *dirstatus, ViceStatus *status,
			   Rights *rights, Rights *anyrights, int MakeProtChecks) {
    return(Check_CLMS_Semantics(client, dirvptr, vptr, Name, volptr, vFile, ReplicatedOp,
				 VCmpProc, dirstatus, status, rights, anyrights, MakeProtChecks));
}


int CheckRemoveSemantics(ClientEntry *client, Vnode **dirvptr, Vnode **vptr, char *Name,
			   Volume **volptr, int ReplicatedOp, VCP VCmpProc,
			   ViceStatus *dirstatus, ViceStatus *status,
			   Rights *rights, Rights *anyrights, int MakeProtChecks) {
    return(Check_RR_Semantics(client, dirvptr, vptr, Name, volptr, vFile, ReplicatedOp,
			       VCmpProc, dirstatus, status, rights, anyrights, MakeProtChecks));
}


int CheckLinkSemantics(ClientEntry *client, Vnode **dirvptr, Vnode **vptr, char *Name,
			 Volume **volptr, int ReplicatedOp, VCP VCmpProc,
			 ViceStatus *dirstatus, ViceStatus *status,
			 Rights *rights, Rights *anyrights, int MakeProtChecks) {
    return(Check_CLMS_Semantics(client, dirvptr, vptr, Name, volptr, vFile, ReplicatedOp,
				VCmpProc, dirstatus, status, rights, anyrights, MakeProtChecks));
}


int CheckRenameSemantics(ClientEntry *client, Vnode **s_dirvptr, Vnode **t_dirvptr,
			 Vnode **s_vptr, char *OldName, Vnode **t_vptr, char *NewName,
			 Volume **volptr, int ReplicatedOp, VCP VCmpProc,
			 ViceStatus *s_dirstatus, ViceStatus *t_dirstatus,
			 ViceStatus *s_status, ViceStatus *t_status,
			 Rights *sd_rights, Rights *sd_anyrights,
			 Rights *td_rights, Rights *td_anyrights,
			 Rights *s_rights, Rights *s_anyrights, 
			 int MakeProtChecks, int IgnoreTargetNonEmpty,
			 dlist *vlist) {
    int errorCode = 0;
    ViceFid SDid;			/* Source directory */
    SDid.Volume = V_id(*volptr);
    SDid.Vnode = (*s_dirvptr)->vnodeNumber;
    SDid.Unique = (*s_dirvptr)->disk.uniquifier;
    ViceFid TDid;			/* Target directory */
    TDid.Volume = V_id(*volptr);
    TDid.Vnode = (*t_dirvptr)->vnodeNumber;
    TDid.Unique = (*t_dirvptr)->disk.uniquifier;
    int SameParent = (FID_EQ(SDid, TDid));
    ViceFid SFid;			/* Source object */
    SFid.Volume = V_id(*volptr);
    SFid.Vnode = (*s_vptr)->vnodeNumber;
    SFid.Unique = (*s_vptr)->disk.uniquifier;
    int TargetExists = (t_vptr != 0);
    ViceFid TFid;			/* Target object (if it exists) */
    if (TargetExists) {
	TFid.Volume = V_id(*volptr);
	TFid.Vnode = (*t_vptr)->vnodeNumber;
	TFid.Unique = (*t_vptr)->disk.uniquifier;
    }
    else
	TFid = NullFid;

    /* Concurrency-control checks. */
    if (VCmpProc) {
	void *arg1 = 0;
	void *arg2 = 0;

	/* Source directory. */
	{
	    arg1 = (ReplicatedOp ? (void *)&Vnode_vv(*s_dirvptr) : (void *)&(*s_dirvptr)->disk.dataVersion);
	    arg2 = (ReplicatedOp ? (void *)&s_dirstatus->VV : (void *)&s_dirstatus->DataVersion);
	    if (errorCode = VCmpProc(ReplicatedOp, (*s_dirvptr)->disk.type, arg1, arg2)) {
		LogMsg(0, SrvDebugLevel, stdout, "CheckRenameSemantics: (%x.%x.%x), VCP error (%d)",
			SDid.Volume, SDid.Vnode, SDid.Unique, errorCode);
		return(errorCode);
	    }
	}


	/* Target directory. */
	{
	    arg1 = (ReplicatedOp ? (void *)&Vnode_vv(*t_dirvptr) : (void *)&(*t_dirvptr)->disk.dataVersion);
	    arg2 = (ReplicatedOp ? (void *)&t_dirstatus->VV : (void *)&t_dirstatus->DataVersion);
	    if (errorCode = VCmpProc(ReplicatedOp, (*t_dirvptr)->disk.type, arg1, arg2)) {
		LogMsg(0, SrvDebugLevel, stdout, "CheckRenameSemantics: (%x.%x.%x), VCP error (%d)",
			TDid.Volume, TDid.Vnode, TDid.Unique, errorCode);
		return(errorCode);
	    }
	}

	/* Source object. */
	{
	    arg1 = (ReplicatedOp ? (void *)&Vnode_vv(*s_vptr) : (void *)&(*s_vptr)->disk.dataVersion);
	    arg2 = (ReplicatedOp ? (void *)&s_status->VV : (void *)&s_status->DataVersion);
	    if (errorCode = VCmpProc(ReplicatedOp, (*s_vptr)->disk.type, arg1, arg2)) {
		LogMsg(0, SrvDebugLevel, stdout, "CheckRenameSemantics: (%x.%x.%x), VCP error (%d)",
			SFid.Volume, SFid.Vnode, SFid.Unique, errorCode);
		return(errorCode);
	    }
	}

	/* Target object. */
	if (TargetExists) {
	    arg1 = (ReplicatedOp ? (void *)&Vnode_vv(*t_vptr) : (void *)&(*t_vptr)->disk.dataVersion);
	    arg2 = (ReplicatedOp ? (void *)&t_status->VV : (void *)&t_status->DataVersion);
	    if (errorCode = VCmpProc(ReplicatedOp, (*t_vptr)->disk.type, arg1, arg2)) {
		LogMsg(0, SrvDebugLevel, stdout, "CheckRenameSemantics: (%x.%x.%x), VCP error (%d)",
			TFid.Volume, TFid.Vnode, TFid.Unique, errorCode);
		return(errorCode);
	    }
	}
    }

    /* Integrity checks. */
    {
	/* Source, Target directories. */
	if ((*s_dirvptr)->disk.type != vDirectory ||
	    (*t_dirvptr)->disk.type != vDirectory) {
	    LogMsg(0, SrvDebugLevel, stdout, "CheckRenameSemantics: not directory (%x.%x.%x) (%x.%x.%x)", 
		    SDid.Volume, SDid.Vnode, SDid.Unique,
		    TDid.Volume, TDid.Vnode, TDid.Unique);
	    return(ENOTDIR);
	}

	/* Source object. */
	{
	    if (STREQ(OldName, ".") || STREQ(OldName, "..")) {
		LogMsg(0, SrvDebugLevel, stdout, "CheckRenameSemantics: illegal old-name (%s)", OldName);
		return(EINVAL);
	    }

	    if ((*s_vptr)->disk.vparent != SDid.Vnode ||
		(*s_vptr)->disk.uparent != SDid.Unique) {
		LogMsg(0, SrvDebugLevel, stdout, "CheckRenameSemantics: source (%x.%x.%x) not in parent (%x.%x.%x)", 
			SFid.Volume, SFid.Vnode, SFid.Unique,
			SDid.Volume, SDid.Vnode, SDid.Unique);
		return(EINVAL);
	    }

	    if (FID_EQ(SDid, SFid) || FID_EQ(TDid, SFid)) {
		LogMsg(0, SrvDebugLevel, stdout, "CheckRenameSemantics: (%x.%x.%x) (%x.%x.%x) (%x.%x.%x) (%x.%x.%x)", 
			SDid.Volume, SDid.Vnode, SDid.Unique,
			TDid.Volume, TDid.Vnode, TDid.Unique,
			SFid.Volume, SFid.Vnode, SFid.Unique,
			TFid.Volume, TFid.Vnode, TFid.Unique);
		return(ELOOP);
	    }

	    /* Cannot allow rename out of a directory if file has multiple links! */
	    if (!SameParent && (*s_vptr)->disk.type == vFile && (*s_vptr)->disk.linkCount > 1) {
		LogMsg(0, SrvDebugLevel, stdout, "CheckRenameSemantics: !SameParent and multiple links (%x.%x.%x)",   SFid.Volume, SFid.Vnode, SFid.Unique);
		return(EXDEV);
	    }
	}

	/* Target object. */
	{
	    if (STREQ(NewName, ".") || STREQ(NewName, "..")) {
		LogMsg(0, SrvDebugLevel, stdout, "CheckRenameSemantics: illegal new-name (%s)", NewName);
		return(EINVAL);
	    }

	    if (TargetExists) {
		if ((*t_vptr)->disk.vparent != TDid.Vnode ||
		    (*t_vptr)->disk.uparent != TDid.Unique) {
		    LogMsg(0, SrvDebugLevel, stdout, "CheckRenameSemantics: target (%x.%x.%x) not in parent (%x.%x.%x)", 
			    TFid.Volume, TFid.Vnode, TFid.Unique,
			    TDid.Volume, TDid.Vnode, TDid.Unique);
		    return(EINVAL);
		}

		if (FID_EQ(SDid, TFid) || FID_EQ(TDid, TFid) || FID_EQ(SFid, TFid)) {
		    LogMsg(0, SrvDebugLevel, stdout, "CheckRenameSemantics: (%x.%x.%x) (%x.%x.%x) (%x.%x.%x) (%x.%x.%x)", 
			    SDid.Volume, SDid.Vnode, SDid.Unique,
			    TDid.Volume, TDid.Vnode, TDid.Unique,
			    SFid.Volume, SFid.Vnode, SFid.Unique,
			    TFid.Volume, TFid.Vnode, TFid.Unique);
		    return(ELOOP);
		}

		if ((*t_vptr)->disk.type == vDirectory) {
		    if((*s_vptr)->disk.type != vDirectory) {
			LogMsg(0, SrvDebugLevel, stdout, "CheckRenameSemantics: target is dir, source is not");
			return(ENOTDIR);
		    }

		    DirHandle dh;
		    SetDirHandle(&dh, *t_vptr);
		    if (IsEmpty((long *)&dh) != 0) {
			LogMsg(0, SrvDebugLevel, stdout, "CheckRenameSemantics: target (%x.%x.%x) not empty", 
				TFid.Volume, TFid.Vnode, TFid.Unique);
			if (!IgnoreTargetNonEmpty) {
			    LogMsg(0, SrvDebugLevel, stdout, "CheckRenameSemantics: Ignoring Non-Empty target directory");
			    return(ENOTEMPTY);	
			}
		    }
		}
		else {
		    if((*s_vptr)->disk.type == vDirectory) {
			LogMsg(0, SrvDebugLevel, stdout, "CheckRenameSemantics: source is dir, target is not");
			return(EISDIR);
		    }
		}
	    }
	}

	/* Check that the source is not above the target in the directory structure. */
	/* This is to prevent detaching a subtree. */
	/* This violates locking/busyqueue requirements! -JJK */
	if ((!SameParent) && ((*s_vptr)->disk.type == vDirectory)) {
	    ViceFid TestFid;
	    TestFid.Volume = TDid.Volume;
	    TestFid.Vnode = (*t_dirvptr)->disk.vparent;
	    TestFid.Unique = (*t_dirvptr)->disk.uparent;

	    for (; TestFid.Vnode != 0;) {
		if (FID_EQ(TestFid, SDid)) {
		    TestFid.Vnode = (*s_dirvptr)->disk.vparent;
		    TestFid.Unique = (*s_dirvptr)->disk.uparent;
		    continue;
		}

		if (FID_EQ(TestFid, SFid) || FID_EQ(TestFid, TDid) ||
		    (TargetExists && FID_EQ(TestFid, TFid))) {
		    LogMsg(0, SrvDebugLevel, stdout, "CheckRenameSemantics: loop detected");
		    return(ELOOP);
		}

		{
		    /* deadlock avoidance */
		    Vnode *testvptr = VGetVnode((Error *)&errorCode, *volptr,
						TestFid.Vnode, TestFid.Unique, 
						TRY_READ_LOCK, 0, 0);
		    if (errorCode == 0) {  
			TestFid.Vnode = testvptr->disk.vparent;
			TestFid.Unique = testvptr->disk.uparent;
			/* this should be unmodified */
			VPutVnode((Error *)&errorCode, testvptr);
			assert(errorCode == 0);
		    } else {
			assert(errorCode == EWOULDBLOCK);
			/* 
			 * Someone has the object locked.  If this is part of a
			 * reintegration, check the supplied vlist for the vnode.
			 * If it has already been locked (by us) the vnode number
			 * and uniquefier may be copied out safely, and there is
			 * no need to call VPutVnode in that case.  If the vnode
			 * is not on the vlist, return rather than be antisocial.
			 */
			LogMsg(0, SrvDebugLevel, stdout, 
			       "CheckRenameSemantics: avoiding deadlock on %x.%x.%x",
			       V_id(*volptr), TestFid.Vnode, TestFid.Unique);
			if (vlist) {
			    vle *v = FindVLE(*vlist, &TestFid);
			    if (v) {
				errorCode = 0;
				TestFid.Vnode = v->vptr->disk.vparent;
				TestFid.Unique = v->vptr->disk.uparent;
			    }
			} 
			if (errorCode) 
			    return(errorCode);
		    }
		}
	    }
	}
    }

    /* Protection checks. */
    if (MakeProtChecks) {
	/* Source directory. */
	{
	    AL_AccessList *aCL = 0;
	    int aCLSize = 0;
	    SetAccessList(*s_dirvptr, aCL, aCLSize);

	    Rights t_sd_rights; if (sd_rights == 0) sd_rights = &t_sd_rights;
	    Rights t_sd_anyrights; if (sd_anyrights == 0) sd_anyrights = &t_sd_anyrights;
	    assert(GetRights(client->CPS, aCL, aCLSize, sd_rights, sd_anyrights) == 0);

	    if (!(*sd_rights & PRSFS_DELETE)) {
		LogMsg(0, SrvDebugLevel, stdout, "CheckRenameSemantics: sd rights violation (%x : %x) (%x.%x.%x)", 
			*sd_rights, *sd_anyrights, SDid.Volume, SDid.Vnode, SDid.Unique);
		return(EACCES);
	    }
	}

	/* Target directory. */
	{
	    AL_AccessList *aCL = 0;
	    int aCLSize = 0;
	    SetAccessList(*t_dirvptr, aCL, aCLSize);

	    Rights t_td_rights; if (td_rights == 0) td_rights = &t_td_rights;
	    Rights t_td_anyrights; if (td_anyrights == 0) td_anyrights = &t_td_anyrights;
	    assert(GetRights(client->CPS, aCL, aCLSize, td_rights, td_anyrights) == 0);

	    if (!(*td_rights & PRSFS_INSERT) ||
		(TargetExists && !(*td_rights & PRSFS_DELETE))) {
		LogMsg(0, SrvDebugLevel, stdout, "CheckRenameSemantics: td rights violation (%x : %x) (%x.%x.%x)", 
			*td_rights, *td_anyrights, TDid.Volume, TDid.Vnode, TDid.Unique);
		return(EACCES);
	    }
	}

	/* Source object. */
	{
	    if ((*s_vptr)->disk.type == vDirectory) {
		AL_AccessList *aCL = 0;
		int aCLSize = 0;
		SetAccessList(*s_vptr, aCL, aCLSize);

		Rights t_s_rights; if (s_rights == 0) s_rights = &t_s_rights;
		Rights t_s_anyrights; if (s_anyrights == 0) s_anyrights = &t_s_anyrights;
		assert(GetRights(client->CPS, aCL, aCLSize, s_rights, s_anyrights) == 0);
	    }
	    else {
		if (s_rights != 0) *s_rights = 0;
		if (s_anyrights != 0) *s_anyrights = 0;
	    }
	}
    }

    return(errorCode);
}


int CheckMkdirSemantics(ClientEntry *client, Vnode **dirvptr, Vnode **vptr, char *Name,
			  Volume **volptr, int ReplicatedOp, VCP VCmpProc,
			  ViceStatus *dirstatus, ViceStatus *status,
			  Rights *rights, Rights *anyrights, int MakeProtChecks) {
    return(Check_CLMS_Semantics(client, dirvptr, vptr, Name, volptr, vDirectory, ReplicatedOp,
				 VCmpProc, dirstatus, status, rights, anyrights, MakeProtChecks));
}


int CheckRmdirSemantics(ClientEntry *client, Vnode **dirvptr, Vnode **vptr, char *Name,
			  Volume **volptr, int ReplicatedOp, VCP VCmpProc,
			  ViceStatus *dirstatus, ViceStatus *status,
			  Rights *rights, Rights *anyrights, int MakeProtChecks) {
    return(Check_RR_Semantics(client, dirvptr, vptr, Name, volptr, vDirectory, ReplicatedOp,
			       VCmpProc, dirstatus, status, rights, anyrights, MakeProtChecks));
}


int CheckSymlinkSemantics(ClientEntry *client, Vnode **dirvptr, Vnode **vptr, char *Name,
			    Volume **volptr, int ReplicatedOp, VCP VCmpProc,
			    ViceStatus *dirstatus, ViceStatus *status,
			    Rights *rights, Rights *anyrights, int MakeProtChecks) {
    return(Check_CLMS_Semantics(client, dirvptr, vptr, Name, volptr, vSymlink, ReplicatedOp,
				 VCmpProc, dirstatus, status, rights, anyrights, MakeProtChecks));
}

/*
  BEGIN_HTML
  <a name="Check_CLMS_Semantics"><strong>Make semantic checks for the create, link,
  mkdir and symlink requests. </a>
  END_HTML 
*/

/* {vptr,status} may be Null for {Create,Mkdir,Symlink}. */
PRIVATE int Check_CLMS_Semantics(ClientEntry *client, Vnode **dirvptr, Vnode **vptr, char *Name,
				  Volume **volptr, VnodeType type, int ReplicatedOp,
				  VCP VCmpProc, ViceStatus *dirstatus,
				  ViceStatus *status, Rights *rights, Rights *anyrights, 
				 int MakeProtChecks) {
    char *ProcName = (type == vFile)
      ? "CheckCreateOrLinkSemantics"
      : (type == vDirectory)
      ? "CheckMkdirSemantics"
      : "CheckSymlinkSemantics";
    int errorCode = 0;
    ViceFid Did;
    Did.Volume = V_id(*volptr);
    Did.Vnode = (*dirvptr)->vnodeNumber;
    Did.Unique = (*dirvptr)->disk.uniquifier;
    ViceFid Fid;
    if (type == vFile && vptr != 0) {
	Fid.Volume = V_id(*volptr);
	Fid.Vnode = (*vptr)->vnodeNumber;
	Fid.Unique = (*vptr)->disk.uniquifier;
    }
    else
	Fid = NullFid;

    /* Concurrency-control checks. */
    if (VCmpProc) {
	void *arg1 = (ReplicatedOp ? (void *)&Vnode_vv(*dirvptr) : (void *)&(*dirvptr)->disk.dataVersion);
	void *arg2 = (ReplicatedOp ? (void *)&dirstatus->VV : (void *)&dirstatus->DataVersion);
	if (errorCode = VCmpProc(ReplicatedOp, (*dirvptr)->disk.type, arg1, arg2)) {
	    LogMsg(0, SrvDebugLevel, stdout, "%s: (%x.%x.%x), VCP error (%d)",
		    ProcName, Did.Volume, Did.Vnode, Did.Unique, errorCode);
	    return(errorCode);
	}

	if (vptr != 0) {
	    arg1 = (ReplicatedOp ? (void *)&Vnode_vv(*vptr) : (void *)&(*vptr)->disk.dataVersion);
	    arg2 = (ReplicatedOp ? (void *)&status->VV : (void *)&status->DataVersion);
	    if (errorCode = VCmpProc(ReplicatedOp, (*vptr)->disk.type, arg1, arg2)) {
		LogMsg(0, SrvDebugLevel, stdout, "%s: (%x.%x.%x), VCP error (%d)",
			ProcName, Fid.Volume, Fid.Vnode, Fid.Unique, errorCode);
		return(errorCode);
	    }
	}
    }

    /* Integrity checks. */
    {
	if ((*dirvptr)->disk.type != vDirectory){
	    LogMsg(0, SrvDebugLevel, stdout, "%s: (%x.%x.%x) not a directory",
		    ProcName, Did.Volume, Did.Vnode, Did.Unique);
	    return(ENOTDIR);
	}

	if (STREQ(Name, ".") || STREQ(Name, "..")) {
	    LogMsg(0, SrvDebugLevel, stdout, "%s: illegal name (%s)",
		    ProcName, Name);
	    return(EINVAL);
	}

	DirHandle dh;
	SetDirHandle(&dh, *dirvptr);
	ViceFid Fid;
	if (Lookup((long *)&dh, Name, (long *)&Fid) == 0) {
	    LogMsg(0, SrvDebugLevel, stdout, "%s: %s already exists in (%x.%x.%x)",
		    ProcName, Name, Did.Volume, Did.Vnode, Did.Unique);
	    return(EEXIST);
	}
	LogMsg(9, SrvDebugLevel, stdout, "%s: Lookup of %s in (%x.%x.%x) failed",
		ProcName, Name, Did.Volume, Did.Vnode, Did.Unique);

	if (vptr != 0) {
	    switch(type) {
		case vFile:
		    if ((*vptr)->disk.type != vFile) {
			LogMsg(0, SrvDebugLevel, stdout, "%s: (%x.%x.%x) not a file", 
				ProcName, Fid.Volume, Fid.Vnode, Fid.Unique);
			return(EISDIR);
		    }
		    break;

		case vDirectory:
		    if ((*vptr)->disk.type != vDirectory) {
			LogMsg(0, SrvDebugLevel, stdout, "%s: (%x.%x.%x) not a directory", 
				ProcName, Fid.Volume, Fid.Vnode, Fid.Unique);
			return(ENOTDIR);
		    }
		    break;

		case vSymlink:
		    if ((*vptr)->disk.type != vSymlink) {
			LogMsg(0, SrvDebugLevel, stdout, "%s: (%x.%x.%x) not a symlink", 
				ProcName, Fid.Volume, Fid.Vnode, Fid.Unique);
			return(EISDIR);
		    }
		    break;

		default:
		    assert(FALSE);
	    }

	    if (((*vptr)->disk.vparent != Did.Vnode) || ((*vptr)->disk.uparent != Did.Unique)){
		LogMsg(0, SrvDebugLevel, stdout, "%s: cross-directory link (%x.%x.%x), (%x.%x.%x)",
			ProcName, Did.Volume, Did.Vnode, Did.Unique,
			Fid.Volume, Fid.Vnode, Fid.Unique);
		return(EXDEV);
	    }
	}
    }

    /* Protection checks. */
    if (MakeProtChecks) {
	/* Get the access list. */
	LogMsg(9, SrvDebugLevel, stdout, "%s: Going to get acl (%x.%x.%x)",
		ProcName, Did.Volume, Did.Vnode, Did.Unique);
	AL_AccessList *aCL = 0;
	int aCLSize = 0;
	SetAccessList(*dirvptr, aCL, aCLSize);

	/* Get this client's rights. */
	LogMsg(9, SrvDebugLevel, stdout, "%s: Going to get rights (%x.%x.%x)",
		ProcName, Did.Volume, Did.Vnode, Did.Unique);
	Rights t_rights; if (rights == 0) rights = &t_rights;
	Rights t_anyrights; if (anyrights == 0) anyrights = &t_anyrights;
	assert(GetRights(client->CPS, aCL, aCLSize, rights, anyrights) == 0);

	/* INSERT permission required. */
	if (!(*rights & PRSFS_INSERT)) {
	    LogMsg(0, SrvDebugLevel, stdout, "%s: rights violation (%x : %x) (%x.%x.%x)",
		    ProcName, *rights, *anyrights, Did.Volume, Did.Vnode, Did.Unique);
	    return(EACCES);
	}
    }
    
    return(0);
}


PRIVATE int Check_RR_Semantics(ClientEntry *client, Vnode **dirvptr, Vnode **vptr, char *Name,
			       Volume **volptr, VnodeType type, int ReplicatedOp,
			       VCP VCmpProc, ViceStatus *dirstatus,
			       ViceStatus *status, Rights *rights, Rights *anyrights,
			       int MakeProtChecks) {
    char *ProcName = (type == vDirectory)
      ? "CheckRmdirSemantics"
      : "CheckRemoveSemantics";
    int errorCode = 0;
    ViceFid Did;
    Did.Volume = V_id(*volptr);
    Did.Vnode = (*dirvptr)->vnodeNumber;
    Did.Unique = (*dirvptr)->disk.uniquifier;
    ViceFid Fid;
    Fid.Volume = V_id(*volptr);
    Fid.Vnode = (*vptr)->vnodeNumber;
    Fid.Unique = (*vptr)->disk.uniquifier;

    /* Concurrency-control checks. */
    if (VCmpProc) {
	void *arg1 = 0;
	void *arg2 = 0;

	arg1 = (ReplicatedOp ? (void *)&Vnode_vv(*dirvptr) : (void *)&(*dirvptr)->disk.dataVersion);
	arg2 = (ReplicatedOp ? (void *)&dirstatus->VV : (void *)&dirstatus->DataVersion);
	if (errorCode = VCmpProc(ReplicatedOp, (*dirvptr)->disk.type, arg1, arg2)) {
	    LogMsg(0, SrvDebugLevel, stdout, "%s: (%x.%x.%x), VCP error (%d)",
		    ProcName, Did.Volume, Did.Vnode, Did.Unique, errorCode);
	    return(errorCode);
	}

	arg1 = (ReplicatedOp ? (void *)&Vnode_vv(*vptr) : (void *)&(*vptr)->disk.dataVersion);
	arg2 = (ReplicatedOp ? (void *)&status->VV : (void *)&status->DataVersion);
	if (errorCode = VCmpProc(ReplicatedOp, (*vptr)->disk.type, arg1, arg2)) {
	    LogMsg(0, SrvDebugLevel, stdout, "%s: (%x.%x.%x), VCP error (%d)",
		    ProcName, Fid.Volume, Fid.Vnode, Fid.Unique, errorCode);
	    return(errorCode);
	}
    }

    /* Integrity checks. */
    {
	if ((*dirvptr)->disk.type != vDirectory) {
	    LogMsg(0, SrvDebugLevel, stdout, "%s: parent (%x.%x.%x) not a directory", 
		    ProcName, Did.Volume, Did.Vnode, Did.Unique);
	    return(ENOTDIR);
	}

	if (STREQ(Name, ".") || STREQ(Name, "..")) {
	    LogMsg(0, SrvDebugLevel, stdout, "%s: illegal name (%s)",
		    ProcName, Name);
	    return(EINVAL);
	}

	if ((*vptr)->disk.vparent != Did.Vnode || (*vptr)->disk.uparent != Did.Unique) {
	    LogMsg(0, SrvDebugLevel, stdout, "%s: child (%x.%x.%x) not in parent (%x.%x.%x)", 
		    ProcName, Fid.Volume, Fid.Vnode, Fid.Unique,
		    Did.Volume, Did.Vnode, Did.Unique);
	    return(EINVAL);
	}

	if ((type == vDirectory) && ((*vptr)->disk.type != vDirectory)){
	    LogMsg(0, SrvDebugLevel, stdout, "%s: child (%x.%x.%x) not a directory", 
		    ProcName, Fid.Volume, Fid.Vnode, Fid.Unique);
	    return(ENOTDIR);
	}
	if ((type != vDirectory) && ((*vptr)->disk.type == vDirectory)) {
	    LogMsg(0, SrvDebugLevel, stdout, "%s: child (%x.%x.%x) is a directory", 
		    ProcName, Fid.Volume, Fid.Vnode, Fid.Unique); 
	    return(EISDIR);
	}

	if (type == vDirectory && VCmpProc != 0) {
	    DirHandle dh;
	    SetDirHandle(&dh, *vptr);
	    if (IsEmpty((long *)&dh) != 0) {
		LogMsg(0, SrvDebugLevel, stdout, "%s: child (%x.%x.%x) not empty ",
			ProcName, Fid.Volume, Fid.Vnode, Fid.Unique);
		return(ENOTEMPTY);
	    }
	}
    }

    /* Protection checks. */
    if (MakeProtChecks) {
	/* Get the access list. */
	AL_AccessList *aCL = 0;
	int aCLSize = 0;
	SetAccessList(*dirvptr, aCL, aCLSize);

	/* Get this client's rights. */
	Rights t_rights; if (rights == 0) rights = &t_rights;
	Rights t_anyrights; if (anyrights == 0) anyrights = &t_anyrights;
	assert(GetRights(client->CPS, aCL, aCLSize, rights, anyrights) == 0);

	/* DELETE permission required. */
	if (!(*rights & PRSFS_DELETE)) {
	    LogMsg(0, SrvDebugLevel, stdout, "%s: rights violation (%x : %x) (%x.%x.%x)", 
		    ProcName, *rights, *anyrights, Did.Volume, Did.Vnode, Did.Unique);
	    return(EACCES);
	}
    }

    return(errorCode);
}


void PerformFetch(ClientEntry *client, Volume *volptr, Vnode *vptr) {
    /* Nothing to do here. */
}


int FetchBulkTransfer(RPC2_Handle RPCid, ClientEntry *client, Volume *volptr, Vnode *vptr) {
    int errorCode = 0;
    ViceFid Fid;
    Fid.Volume = V_id(volptr);
    Fid.Vnode = vptr->vnodeNumber;
    Fid.Unique = vptr->disk.uniquifier;
    RPC2_Integer Length = vptr->disk.length;
    char *buf = 0;
    int size = 0;
    int i;
START_TIMING(Fetch_Xfer);

    /* If fetching a directory first copy contents from rvm to a temp buffer. */
    if (vptr->disk.type == vDirectory){
	assert(vptr->disk.inodeNumber != 0);
	DirInode *inArr = (DirInode *)(vptr->disk.inodeNumber);
	for (i = 0; i < MAXPAGES; i++) {
	    if (inArr->Pages[i]) 
		size += PAGESIZE;
	    else 
		break;
	}
	buf = (char *)malloc(size);
	
	for (i = 0; i < MAXPAGES; i++) {
	    if (inArr->Pages[i] == 0) break;

	    bcopy((char *)(inArr->Pages[i]), &buf[i * PAGESIZE], PAGESIZE);
	}

	LogMsg(9, SrvDebugLevel, stdout, "FetchBulkTransfer: wrote directory contents (%x.%x.%x) into buf",
		Fid.Volume, Fid.Vnode, Fid.Unique);
    }

    /* Do the bulk transfer. */
    {
	struct timeval StartTime, StopTime;
	TM_GetTimeOfDay(&StartTime, 0);

	SE_Descriptor sid;
	sid.Tag = client->SEType;
	sid.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
	sid.Value.SmartFTPD.SeekOffset = 0;
	sid.Value.SmartFTPD.hashmark = (SrvDebugLevel > 2 ? '#' : '\0');
	sid.Value.SmartFTPD.ByteQuota = -1;
	if (vptr->disk.type != vDirectory) {
	    if (vptr->disk.inodeNumber) {
		sid.Value.SmartFTPD.Tag = FILEBYINODE;
		sid.Value.SmartFTPD.FileInfo.ByInode.Device = V_device(volptr);
		sid.Value.SmartFTPD.FileInfo.ByInode.Inode = vptr->disk.inodeNumber;
	    }
	    else {
		sid.Value.SmartFTPD.Tag = FILEBYNAME;
		sid.Value.SmartFTPD.FileInfo.ByName.ProtectionBits = 0;
		strcpy(sid.Value.SmartFTPD.FileInfo.ByName.LocalFileName, "/dev/null");
	    }
	}
	else {
	    /* if it is a directory get the contents from the temp file */
	    sid.Value.SmartFTPD.Tag = FILEINVM;
	    sid.Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqLen = size;
	    sid.Value.SmartFTPD.FileInfo.ByAddr.vmfile.MaxSeqLen = size;
	    sid.Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqBody =
	      (RPC2_ByteSeq)buf;
	}
	
	/* debugging purposes */
	LogMsg(9, SrvDebugLevel, stdout, "FetchBulkTransfer: Printing se descriptor:");
	switch (sid.Value.SmartFTPD.Tag) {
	    case FILEBYINODE:
		LogMsg(9, SrvDebugLevel, stdout, "Tag = FILEBYINODE, device = %u, inode = %u",
			sid.Value.SmartFTPD.FileInfo.ByInode.Device,
			sid.Value.SmartFTPD.FileInfo.ByInode.Inode);
		break;
	    case FILEBYNAME:
		LogMsg(9, SrvDebugLevel, stdout, "Tag = FILEBYNAME, LocalFileName = %s",
			sid.Value.SmartFTPD.FileInfo.ByName.LocalFileName);
		break;
	    case FILEINVM:
		LogMsg(9, SrvDebugLevel, stdout, "Tag = FILEINVM, len = %d buf = %x",
			sid.Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqLen,
			sid.Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqBody);
		break;
	    default:
		LogMsg(9, SrvDebugLevel, stdout, "BOGUS TAG");
		assert(0);
		break;
	}
	if((errorCode = (int) RPC2_InitSideEffect(RPCid, &sid)) <= RPC2_ELIMIT) {
	    LogMsg(0, SrvDebugLevel, stdout, "FetchBulkTransfer: InitSE failed (%d), (%x.%x.%x)",
		    errorCode, Fid.Volume, Fid.Vnode, Fid.Unique);
	    goto Exit;
	}

	if ((errorCode = (int) RPC2_CheckSideEffect(RPCid, &sid, SE_AWAITLOCALSTATUS)) <= RPC2_ELIMIT) {
	    LogMsg(0, SrvDebugLevel, stdout, "FetchBulkTransfer: CheckSE failed (%d), (%x.%x.%x)",
		    errorCode, Fid.Volume, Fid.Vnode, Fid.Unique);
	    if (errorCode == RPC2_SEFAIL1) errorCode = EIO;
	    goto Exit;
	}

	RPC2_Integer len = sid.Value.SmartFTPD.BytesTransferred;
	if (Length != len) {
	    LogMsg(0, SrvDebugLevel, stdout, "FetchBulkTransfer: length discrepancy (%d : %d), (%x.%x.%x), %s %s.%d",
		    Length, len, Fid.Volume, Fid.Vnode, Fid.Unique,
		    client->UserName, client->VenusId->HostName, client->VenusId->port);
	    errorCode = EINVAL;
	    goto Exit;
	}

	TM_GetTimeOfDay(&StopTime, 0);

	Counters[FETCHDATAOP]++;
	Counters[FETCHTIME] += (int) (((StopTime.tv_sec - StartTime.tv_sec) * 1000) +
	  ((StopTime.tv_usec - StartTime.tv_usec) / 1000));
	Counters[FETCHDATA] += (int) Length;
	if (Length < SIZE1)
	    Counters[FETCHD1]++;
	else if (Length < SIZE2)
	    Counters[FETCHD2]++;
	else if (Length < SIZE3)
	    Counters[FETCHD3]++;
	else if (Length < SIZE4)
	    Counters[FETCHD4]++;
	else
	    Counters[FETCHD5]++;

	LogMsg(2, SrvDebugLevel, stdout, "FetchBulkTransfer: transferred %d bytes (%x.%x.%x)",
		Length, Fid.Volume, Fid.Vnode, Fid.Unique);
    }

Exit:
    if (vptr->disk.type == vDirectory && buf != 0)
	free(buf);
END_TIMING(Fetch_Xfer);
    return(errorCode);
}

int FetchFileByName(RPC2_Handle RPCid, char *name, ClientEntry *client) {
    int errorCode = 0;
    SE_Descriptor sid;
    bzero(&sid, (int) sizeof(SE_Descriptor));
    sid.Tag = client ? client->SEType : SMARTFTP;
    sid.Value.SmartFTPD.TransmissionDirection = CLIENTTOSERVER;
    sid.Value.SmartFTPD.Tag = FILEBYNAME;
    sid.Value.SmartFTPD.FileInfo.ByName.ProtectionBits = 0777;
    strcpy(sid.Value.SmartFTPD.FileInfo.ByName.LocalFileName, name);
    sid.Value.SmartFTPD.ByteQuota = -1;
    sid.Value.SmartFTPD.SeekOffset = 0;
    sid.Value.SmartFTPD.hashmark = 0;
    if ((errorCode = (int) RPC2_InitSideEffect(RPCid, &sid))
	<= RPC2_ELIMIT) {
	LogMsg(0, SrvDebugLevel, stdout, "FetchFileByName: InitSideEffect failed %d", 
		errorCode);
	return(errorCode);
    }

    if ((errorCode = (int) RPC2_CheckSideEffect(RPCid, &sid, SE_AWAITLOCALSTATUS)) 
	<= RPC2_ELIMIT) {
	if (errorCode == RPC2_SEFAIL1) errorCode = EIO;
	LogMsg(0, SrvDebugLevel, stdout, "FetchFileByName: CheckSideEffect failed %d",
		errorCode);
	return(errorCode);
    }    
    return(0);
}

void PerformGetAttr(ClientEntry *client, Volume *volptr, Vnode *vptr) {
    /* Nothing to do here. */
}


void PerformGetACL(ClientEntry *client, Volume *volptr, Vnode *vptr,
		    RPC2_BoundedBS *AccessList, RPC2_String eACL) {
    strcpy((char *)(AccessList->SeqBody), (char *)eACL);
    AccessList->SeqLen = strlen((char *)(AccessList->SeqBody)) + 1;
}


void PerformStore(ClientEntry *client, VolumeId VSGVolnum, Volume *volptr,
		  Vnode *vptr, Inode newinode, int ReplicatedOp,
		  RPC2_Integer Length, Date_t Mtime, ViceStoreId *StoreId,
		  RPC2_Integer *vsptr) {
    ViceFid Fid;
    Fid.Volume = V_id(volptr);
    Fid.Vnode = vptr->vnodeNumber;
    Fid.Unique = vptr->disk.uniquifier;

    CodaBreakCallBack(client->VenusId, &Fid, VSGVolnum);

    vptr->disk.cloned =	0;		/* installation of shadow copy here effectively does COW! */
    vptr->disk.inodeNumber = newinode;
    vptr->disk.length = Length;
    vptr->disk.unixModifyTime = Mtime;
    vptr->disk.author = client->Id;
    vptr->disk.dataVersion++;
    if (ReplicatedOp) {
	NewCOP1Update(volptr, vptr, StoreId, vsptr);

	/* Await COP2 message. */
	ViceFid fids[MAXFIDS]; bzero(fids, (int)(MAXFIDS * sizeof(ViceFid)));
	fids[0] = Fid;
	CopPendingMan->add(new cpent(StoreId, fids));
    }
}


int StoreBulkTransfer(RPC2_Handle RPCid, ClientEntry *client, Volume *volptr,
		       Vnode *vptr, Inode newinode, RPC2_Integer Length) {
    int errorCode = 0;
    ViceFid Fid;
    Fid.Volume = V_id(volptr);
    Fid.Vnode = vptr->vnodeNumber;
    Fid.Unique = vptr->disk.uniquifier;

START_TIMING(Store_Xfer);

    /* Do the bulk transfer. */
    {
	struct timeval StartTime, StopTime;
	TM_GetTimeOfDay(&StartTime, 0);

	SE_Descriptor sid;
	sid.Tag = client->SEType;
	sid.Value.SmartFTPD.TransmissionDirection = CLIENTTOSERVER;
	sid.Value.SmartFTPD.SeekOffset = 0;
	sid.Value.SmartFTPD.hashmark = (SrvDebugLevel > 2 ? '#' : '\0');
	sid.Value.SmartFTPD.Tag = FILEBYINODE;
	sid.Value.SmartFTPD.ByteQuota = Length;
	sid.Value.SmartFTPD.FileInfo.ByInode.Device = V_device(volptr);
	sid.Value.SmartFTPD.FileInfo.ByInode.Inode = newinode;

	if((errorCode = (int) RPC2_InitSideEffect(RPCid, &sid)) <= RPC2_ELIMIT) {
	    LogMsg(0, SrvDebugLevel, stdout, "StoreBulkTransfer: InitSE failed (%d), (%x.%x.%x)",
		    errorCode, Fid.Volume, Fid.Vnode, Fid.Unique);
	    goto Exit;
	}

	if ((errorCode = (int) RPC2_CheckSideEffect(RPCid, &sid, SE_AWAITLOCALSTATUS)) <= RPC2_ELIMIT) {
	    LogMsg(0, SrvDebugLevel, stdout, "StoreBulkTransfer: CheckSE failed (%d), (%x.%x.%x)",
		    errorCode, Fid.Volume, Fid.Vnode, Fid.Unique);
	    if (errorCode == RPC2_SEFAIL1) errorCode = EIO;
	    goto Exit;
	}

	RPC2_Integer len = sid.Value.SmartFTPD.BytesTransferred;
	if (Length != -1 && Length != len) {
	    LogMsg(0, SrvDebugLevel, stdout, "StoreBulkTransfer: length discrepancy (%d : %d), (%x.%x.%x), %s %s.%d",
		    Length, len, Fid.Volume, Fid.Vnode, Fid.Unique,
		    client->UserName, client->VenusId->HostName, client->VenusId->port);
	    errorCode = EINVAL;
	    goto Exit;
	}

	TM_GetTimeOfDay(&StopTime, 0);

	Counters[STOREDATAOP]++;
	Counters[STORETIME] += (int) (((StopTime.tv_sec - StartTime.tv_sec) * 1000) +
	  ((StopTime.tv_usec - StartTime.tv_usec) / 1000));
	Counters[STOREDATA] += (int) Length;
	if (Length < SIZE1)
	    Counters[STORED1]++;
	else if (Length < SIZE2)
	    Counters[STORED2]++;
	else if (Length < SIZE3)
	    Counters[STORED3]++;
	else if (Length < SIZE4)
	    Counters[STORED4]++;
	else
	    Counters[STORED5]++;

	LogMsg(2, SrvDebugLevel, stdout, "StoreBulkTransfer: transferred %d bytes (%x.%x.%x)",
		Length, Fid.Volume, Fid.Vnode, Fid.Unique);
    }

Exit:
END_TIMING(Store_Xfer);
    return(errorCode);
}


void PerformNewSetAttr(ClientEntry *client, VolumeId VSGVolnum, Volume *volptr,
		       Vnode *vptr, int ReplicatedOp, RPC2_Integer Length,
		       Date_t Mtime, UserId Owner, RPC2_Unsigned Mode,
		       RPC2_Integer Mask, ViceStoreId *StoreId, Inode *CowInode,
		       RPC2_Integer *vsptr) {
    ViceFid Fid;
    Fid.Volume = V_id(volptr);
    Fid.Vnode = vptr->vnodeNumber;
    Fid.Unique = vptr->disk.uniquifier;

    CodaBreakCallBack(client->VenusId, &Fid, VSGVolnum);

    if (Mask & SET_LENGTH) 
	vptr->disk.length = Length;

    if (Mask & SET_TIME)
        vptr->disk.unixModifyTime = Mtime;
    
    if (Mask & SET_OWNER)	
        vptr->disk.owner = Owner;

    if (Mask & SET_MODE)
	vptr->disk.modeBits = Mode;

    vptr->disk.author = client->Id;


    /* Truncate invokes COW! */
    if (Mask & SET_LENGTH)
	if (vptr->disk.cloned) {
	    *CowInode = vptr->disk.inodeNumber;
	    CopyOnWrite(vptr, volptr);
	}

    if (ReplicatedOp) {
	NewCOP1Update(volptr, vptr, StoreId, vsptr);

	/* Await COP2 message. */
	ViceFid fids[MAXFIDS]; bzero(fids, MAXFIDS * (int) sizeof(ViceFid));
	fids[0] = Fid;
	CopPendingMan->add(new cpent(StoreId, fids));
    }
}


void PerformSetACL(ClientEntry *client, VolumeId VSGVolnum, Volume *volptr,
		    Vnode *vptr, int ReplicatedOp, ViceStoreId *StoreId,
		    AL_AccessList *newACL, RPC2_Integer *vsptr) {
    ViceFid Fid;
    Fid.Volume = V_id(volptr);
    Fid.Vnode = vptr->vnodeNumber;
    Fid.Unique = vptr->disk.uniquifier;

    CodaBreakCallBack(client->VenusId, &Fid, VSGVolnum);

    vptr->disk.author = client->Id;
    AL_AccessList *aCL = 0;
    int aCLSize = 0;
    SetAccessList(vptr, aCL, aCLSize);
    bcopy((char *)newACL, (char *)aCL, (int)(newACL->MySize));

    if (ReplicatedOp) {
	NewCOP1Update(volptr, vptr, StoreId, vsptr);

	/* Await COP2 message. */
	ViceFid fids[MAXFIDS]; bzero(fids, (int) (MAXFIDS * sizeof(ViceFid)));
	fids[0] = Fid;
	CopPendingMan->add(new cpent(StoreId, fids));
    }
}


void PerformCreate(ClientEntry *client, VolumeId VSGVolnum, Volume *volptr,
		    Vnode *dirvptr, Vnode *vptr, char *Name, Date_t Mtime, RPC2_Unsigned Mode,
		    int ReplicatedOp, ViceStoreId *StoreId, 
		   DirInode **CowInode, int *blocks, RPC2_Integer *vsptr) {
    Perform_CLMS(client, VSGVolnum, volptr, dirvptr, vptr, ViceCreate_OP,
		  Name, 0, 0, Mtime, Mode, ReplicatedOp, StoreId, CowInode,
		 blocks, vsptr);
}


void PerformRemove(ClientEntry *client, VolumeId VSGVolnum, Volume *volptr,
		    Vnode *dirvptr, Vnode *vptr, char *Name, Date_t Mtime,
		    int ReplicatedOp, ViceStoreId *StoreId, DirInode **CowInode, 
		   int *blocks, RPC2_Integer *vsptr) {
    Perform_RR(client, VSGVolnum, volptr, dirvptr, vptr,
		Name, Mtime, ReplicatedOp, StoreId, CowInode, blocks, vsptr);
}


void PerformLink(ClientEntry *client, VolumeId VSGVolnum, Volume *volptr,
		  Vnode *dirvptr, Vnode *vptr, char *Name, Date_t Mtime,
		  int ReplicatedOp, ViceStoreId *StoreId, DirInode **CowInode,
		 int *blocks, RPC2_Integer *vsptr) {
    Perform_CLMS(client, VSGVolnum, volptr, dirvptr, vptr, ViceLink_OP,
		  Name, 0, 0, Mtime, 0, ReplicatedOp, StoreId, CowInode, 
		 blocks, vsptr);
}


void PerformRename(ClientEntry *client, VolumeId VSGVolnum, Volume *volptr,
		    Vnode *sd_vptr, Vnode *td_vptr, Vnode *s_vptr, Vnode *t_vptr,
		    char *OldName, char *NewName, Date_t Mtime,
		    int ReplicatedOp, ViceStoreId *StoreId, DirInode **sd_CowInode,
		    DirInode **td_CowInode, DirInode **s_CowInode, int *nblocks,
		   RPC2_Integer *vsptr) {
    ViceFid SDid;			/* Source directory */
    SDid.Volume = V_id(volptr);
    SDid.Vnode = sd_vptr->vnodeNumber;
    SDid.Unique = sd_vptr->disk.uniquifier;
    ViceFid TDid;			/* Target directory */
    TDid.Volume = V_id(volptr);
    TDid.Vnode = td_vptr->vnodeNumber;
    TDid.Unique = td_vptr->disk.uniquifier;
    int SameParent = (FID_EQ(SDid, TDid));
    ViceFid SFid;			/* Source object */
    SFid.Volume = V_id(volptr);
    SFid.Vnode = s_vptr->vnodeNumber;
    SFid.Unique = s_vptr->disk.uniquifier;
    int TargetExists = (t_vptr != 0);
    ViceFid TFid;			/* Target object (if it exists) */
    if (TargetExists) {
	TFid.Volume = V_id(volptr);
	TFid.Vnode = t_vptr->vnodeNumber;
	TFid.Unique = t_vptr->disk.uniquifier;
    }
    else
	TFid = NullFid;

    if (nblocks) *nblocks = 0;

    CodaBreakCallBack(client ? client->VenusId : 0, &SDid, VSGVolnum);
    if (!SameParent)
	CodaBreakCallBack(client ? client->VenusId : 0, &TDid, VSGVolnum);
    CodaBreakCallBack(client ? client->VenusId : 0, &SFid, VSGVolnum);
    if (TargetExists)
	CodaBreakCallBack(client ? client->VenusId : 0 , &TFid, VSGVolnum);

    /* Rename invokes COW! */
    if (sd_vptr->disk.cloned) {
	*sd_CowInode = (DirInode *)sd_vptr->disk.inodeNumber;
	CopyOnWrite(sd_vptr, volptr);
    }
    if (!SameParent && td_vptr->disk.cloned) {
	*td_CowInode = (DirInode *)td_vptr->disk.inodeNumber;
	CopyOnWrite(td_vptr, volptr);
    }
    if (s_vptr->disk.type == vDirectory && s_vptr->disk.cloned) {
	*s_CowInode = (DirInode *)s_vptr->disk.inodeNumber;
	CopyOnWrite(s_vptr, volptr);
    }

    /* Remove the source name from its parent. */
    DirHandle sd_dh;
    SetDirHandle(&sd_dh, sd_vptr);
    assert(Delete((long *)&sd_dh, OldName) == 0);
    DFlush();
    int sd_newlength = ::Length((long *)&sd_dh);
    int sd_newblocks = (int) (nBlocks(sd_newlength) - nBlocks(sd_vptr->disk.length));
    if(sd_newblocks != 0) {
	ChangeDiskUsage(volptr, sd_newblocks);
	if (nblocks) *nblocks += sd_newblocks;
    }
    sd_vptr->disk.length = sd_newlength;

    /* Remove the target name from its parent (if it exists). */
    if (TargetExists) {
	/* Remove the name. */
	DirHandle td_dh;
	SetDirHandle(&td_dh, td_vptr);
	assert(Delete((long *)&td_dh, NewName) == 0);
	DFlush();
	int td_newlength = ::Length((long *)&td_dh);
	int td_newblocks = (int) (nBlocks(td_newlength) - nBlocks(td_vptr->disk.length));
	if(td_newblocks != 0) {
	    ChangeDiskUsage(volptr, td_newblocks);
	    if (nblocks) *nblocks += td_newblocks;
	}
	td_vptr->disk.length = td_newlength;

	/* Flush directory pages for deleted child. */
	if (t_vptr->disk.type == vDirectory) {
	    DirHandle t_dh;
	    SetDirHandle(&t_dh, t_vptr);
	    DZap((long *)&t_dh);
	}
    }

    /* Create the target name in its parent. */
    DirHandle td_dh;
    SetDirHandle(&td_dh, td_vptr);
    assert(Create((long *)&td_dh, (char *)NewName, (long *)&SFid) == 0);
    DFlush();
    int td_newlength = ::Length((long *)&td_dh);
    int td_newblocks = (int) (nBlocks(td_newlength) - nBlocks(td_vptr->disk.length));
    if(td_newblocks != 0) {
	ChangeDiskUsage(volptr, td_newblocks);
	if (nblocks) *nblocks += td_newblocks;
    }
    td_vptr->disk.length = td_newlength;

    /* Alter ".." entry in source if necessary. */
    if (!SameParent && s_vptr->disk.type == vDirectory) {
	DirHandle s_dh;
	SetDirHandle(&s_dh, s_vptr);
	assert(Delete((long *)&s_dh, "..") == 0);
	sd_vptr->disk.linkCount--;
	assert(Create((long *)&s_dh, "..", (long *)&TDid) == 0);
	DFlush();
	td_vptr->disk.linkCount++;
	int s_newlength = ::Length((long *)&s_dh);
	int s_newblocks = (int) (nBlocks(s_newlength) - nBlocks(s_vptr->disk.length));
	if(s_newblocks != 0) {
	    ChangeDiskUsage(volptr, s_newblocks);
	    if (nblocks) *nblocks += s_newblocks;
	}
	s_vptr->disk.length = s_newlength;
    }

    /* Update source parent vnode. */
    sd_vptr->disk.unixModifyTime = Mtime;
    sd_vptr->disk.author = client ? client->Id : sd_vptr->disk.author;
    sd_vptr->disk.dataVersion++;
    if (ReplicatedOp)
	NewCOP1Update(volptr, sd_vptr, StoreId, vsptr);

    /* Update target parent vnode. */
    if (!SameParent) {
	td_vptr->disk.unixModifyTime = Mtime;
	td_vptr->disk.author = client ? client->Id : td_vptr->disk.author;
	td_vptr->disk.dataVersion++;
	if (ReplicatedOp) 
	    NewCOP1Update(volptr, td_vptr, StoreId, vsptr);
    }
    if (TargetExists && t_vptr->disk.type == vDirectory)
	td_vptr->disk.linkCount--;

    /* Update source vnode. */
    if (!SameParent){
	s_vptr->disk.vparent = td_vptr->vnodeNumber;
	s_vptr->disk.uparent = td_vptr->disk.uniquifier;
    }
    if (ReplicatedOp) 
	NewCOP1Update(volptr, s_vptr, StoreId, vsptr);

    /* Update target vnode. */
    if (TargetExists) 
	if (--t_vptr->disk.linkCount == 0 || t_vptr->disk.type == vDirectory) {
	    t_vptr->delete_me = 1;
	    DeleteFile(&TFid);
	}
	else 
	    if (ReplicatedOp) 
		NewCOP1Update(volptr, t_vptr, StoreId, vsptr);


    /* Await COP2 message. */
    if (ReplicatedOp) {
	ViceFid fids[MAXFIDS]; bzero(fids, (int) (MAXFIDS * sizeof(ViceFid)));
	fids[0] = SDid;
	if (!SameParent) fids[1] = TDid;
	fids[2] = SFid;
	if (TargetExists && !t_vptr->delete_me) fids[3] = TFid;
	CopPendingMan->add(new cpent(StoreId, fids));
    }
}


void PerformMkdir(ClientEntry *client, VolumeId VSGVolnum, Volume *volptr,
		   Vnode *dirvptr, Vnode *vptr, char *Name, Date_t Mtime, RPC2_Unsigned Mode,
		   int ReplicatedOp, ViceStoreId *StoreId, DirInode **CowInode,
		  int *blocks, RPC2_Integer *vsptr) {
    Perform_CLMS(client, VSGVolnum, volptr, dirvptr, vptr, ViceMakeDir_OP,
		  Name, 0, 0, Mtime, Mode, ReplicatedOp, StoreId, CowInode,
		 blocks, vsptr);
}


void PerformRmdir(ClientEntry *client, VolumeId VSGVolnum, Volume *volptr,
		   Vnode *dirvptr, Vnode *vptr, char *Name, Date_t Mtime,
		   int ReplicatedOp, ViceStoreId *StoreId, DirInode **CowInode, 
		  int *blocks, RPC2_Integer *vsptr) {
    Perform_RR(client, VSGVolnum, volptr, dirvptr, vptr,
		Name, Mtime, ReplicatedOp, StoreId, CowInode, blocks, vsptr);
}


void PerformSymlink(ClientEntry *client, VolumeId VSGVolnum, Volume *volptr,
		     Vnode *dirvptr, Vnode *vptr, char *Name, Inode newinode,
		     RPC2_Unsigned Length, Date_t Mtime, RPC2_Unsigned Mode,
		     int ReplicatedOp, ViceStoreId *StoreId, DirInode **CowInode,
		    int *blocks, RPC2_Integer *vsptr) {
    Perform_CLMS(client, VSGVolnum, volptr, dirvptr, vptr, ViceSymLink_OP,
		  Name, newinode, Length, Mtime, Mode, ReplicatedOp, StoreId, CowInode,
		 blocks, vsptr);
}


/*
  BEGIN_HTML
  <a name="Perform_CLMS"> <strong>Perform the create, link, mkdir or
  symlink operation on a VM copy of the object.
  </strong></a>
  END_HTML 
*/

PRIVATE void Perform_CLMS(ClientEntry *client, VolumeId VSGVolnum,
			   Volume *volptr, Vnode *dirvptr, Vnode *vptr,
			   int opcode, char *Name, Inode newinode,
			   RPC2_Unsigned Length, Date_t Mtime, RPC2_Unsigned Mode,
			   int ReplicatedOp, ViceStoreId *StoreId, DirInode **CowInode,
			  int *nblocks, RPC2_Integer *vsptr) {
    *nblocks = 0;
    ViceFid Did;
    Did.Volume = V_id(volptr);
    Did.Vnode = dirvptr->vnodeNumber;
    Did.Unique = dirvptr->disk.uniquifier;
    ViceFid Fid;
    Fid.Volume = V_id(volptr);
    Fid.Vnode = vptr->vnodeNumber;
    Fid.Unique = vptr->disk.uniquifier;

    /* Break callback promises. */
    CodaBreakCallBack((client ? client->VenusId : 0), &Did, VSGVolnum);
    if (opcode == ViceLink_OP)
	CodaBreakCallBack((client ? client->VenusId : 0), &Fid, VSGVolnum);

    /* CLMS invokes COW! */
    if (dirvptr->disk.cloned) {
	*CowInode = (DirInode *)dirvptr->disk.inodeNumber;
	CopyOnWrite(dirvptr, volptr);
    }

    /* Add the name to the parent. */
    DirHandle dh;
    SetDirHandle(&dh, dirvptr);
    assert(Create((long *)&dh, (char *)Name, (long *)&Fid) == 0);
    DFlush();
    int newlength = ::Length((long *)&dh);
    int newblocks = (int) (nBlocks(newlength) - nBlocks(dirvptr->disk.length));
    if(newblocks != 0) {
	ChangeDiskUsage(volptr, newblocks);
	*nblocks = newblocks;
    }

    /* Update the parent vnode. */
    if (vptr->disk.type == vDirectory)
	dirvptr->disk.linkCount++;
    dirvptr->disk.length = newlength;
    dirvptr->disk.unixModifyTime = Mtime;
    dirvptr->disk.author = client ? client->Id : 0;
    dirvptr->disk.dataVersion++;
    if (ReplicatedOp) 
	NewCOP1Update(volptr, dirvptr, StoreId, vsptr);

    /* Initialize/Update the child vnode. */
    switch(opcode) {
	case ViceCreate_OP:
	    vptr->disk.inodeNumber = 0;
	    vptr->disk.linkCount = 1;
	    vptr->disk.length = 0;
	    vptr->disk.unixModifyTime = Mtime;
	    vptr->disk.author = client ? client->Id : 0;
	    vptr->disk.owner = client ? client->Id : 0;
	    vptr->disk.modeBits = Mode;
	    vptr->disk.vparent = Did.Vnode;
	    vptr->disk.uparent = Did.Unique;
	    vptr->disk.dataVersion = 0;
	    InitVV(&Vnode_vv(vptr));
	    break;

	case ViceLink_OP:
	    vptr->disk.linkCount++;
	    vptr->disk.author = client ? client->Id : 0;
	    break;

	case ViceMakeDir_OP:
	    {
	    vptr->disk.inodeNumber = 0;

	    /* Create the child directory. */
	    DirHandle cdh;
	    SetDirHandle(&cdh, vptr);
	    assert(MakeDir((long *)&cdh, (long *)&Fid, (long *)&Did) == 0);
	    DFlush();

	    vptr->disk.linkCount = 2;
	    vptr->disk.length = ::Length((long *)&cdh);
	    vptr->disk.unixModifyTime = Mtime;
	    vptr->disk.author = client ? client->Id : 0;
	    vptr->disk.owner = client ? client->Id : 0;
	    vptr->disk.modeBits = Mode;
	    vptr->disk.vparent = Did.Vnode;
	    vptr->disk.uparent = Did.Unique;
	    vptr->disk.dataVersion = 1;
	    InitVV(&Vnode_vv(vptr));

	    /* Child inherits access list. */
	    {
		AL_AccessList *aCL = 0;
		int aCLSize = 0;
		SetAccessList(dirvptr, aCL, aCLSize);

		AL_AccessList *newACL = 0;
		int newACLSize = 0;
		SetAccessList(vptr, newACL, newACLSize);

		bcopy((char *)aCL, (char *)newACL, aCLSize);
	    }
	    }
	    break;

	case ViceSymLink_OP:
	    vptr->disk.inodeNumber = newinode;
	    vptr->disk.linkCount = 1;
	    vptr->disk.length = Length;
	    vptr->disk.unixModifyTime = Mtime;
	    vptr->disk.author = client ? client->Id : 0;
	    vptr->disk.owner = client ? client->Id : 0;
	    vptr->disk.modeBits = Mode;
	    vptr->disk.vparent = Did.Vnode;
	    vptr->disk.uparent = Did.Unique;
	    vptr->disk.dataVersion = 1;
	    InitVV(&Vnode_vv(vptr));
	    break;

	default:
	    assert(FALSE);
    }
    if (ReplicatedOp) 
	NewCOP1Update(volptr, vptr, StoreId, vsptr);

    /* Await COP2 message. */
    if (ReplicatedOp) {
	ViceFid fids[MAXFIDS]; bzero(fids, (int) (MAXFIDS * sizeof(ViceFid)));
	fids[0] = Did;
	fids[1] = Fid;
	CopPendingMan->add(new cpent(StoreId, fids));
    }
}


PRIVATE void Perform_RR(ClientEntry *client, VolumeId VSGVolnum, Volume *volptr,
			 Vnode *dirvptr, Vnode *vptr, char *Name, Date_t Mtime,
			 int ReplicatedOp, ViceStoreId *StoreId, DirInode **CowInode,
			 int *blocks, RPC2_Integer *vsptr) {
    *blocks = 0;
    ViceFid Did;
    Did.Volume = V_id(volptr);
    Did.Vnode = dirvptr->vnodeNumber;
    Did.Unique = dirvptr->disk.uniquifier;
    ViceFid Fid;
    Fid.Volume = V_id(volptr);
    Fid.Vnode = vptr->vnodeNumber;
    Fid.Unique = vptr->disk.uniquifier;

    CodaBreakCallBack(client ? client->VenusId : 0, &Did, VSGVolnum);
    CodaBreakCallBack(client ? client->VenusId : 0, &Fid, VSGVolnum);

    /* RR invokes COW! */
    if (dirvptr->disk.cloned) {
	*CowInode = (DirInode *)dirvptr->disk.inodeNumber;
	CopyOnWrite(dirvptr, volptr);
    }

    /* Remove the name from the directory. */
    DirHandle pDir;
    SetDirHandle(&pDir, dirvptr);
    assert(Delete((long *)&pDir, Name) == 0);
    DFlush();
    int newlength = Length((long *)&pDir);
    int newblocks = (int) (nBlocks(newlength) - nBlocks(dirvptr->disk.length));
    if(newblocks != 0) {
	ChangeDiskUsage(volptr, newblocks);
	*blocks = newblocks;
    }

    /* Update the directory vnode. */
    dirvptr->disk.length = newlength;
    dirvptr->disk.author = client ? client->Id : 0;
    dirvptr->disk.unixModifyTime = Mtime;
    if (vptr->disk.type == vDirectory)
	dirvptr->disk.linkCount--;
    dirvptr->disk.dataVersion++;
    if (ReplicatedOp) 
	NewCOP1Update(volptr, dirvptr, StoreId, vsptr);

    /* Flush directory pages for deleted child. */
    if (vptr->disk.type == vDirectory) {
	DirHandle cDir;
	SetDirHandle(&cDir, vptr);
	DZap((long *)&cDir);
    }

    /* Update the child vnode. */
    LogMsg(3, SrvDebugLevel, stdout, "Perform_RR: LC = %d, TYPE = %d, delete_me = %d",
	     vptr->disk.linkCount, vptr->disk.type, vptr->delete_me);	    /* DEBUG - JJK */
    if (--vptr->disk.linkCount == 0 || vptr->disk.type == vDirectory) {
	vptr->delete_me = 1;
	DeleteFile(&Fid);
    }
    else 
	if (ReplicatedOp) 
	    NewCOP1Update(volptr, vptr, StoreId, vsptr);

    /* Await COP2 message. */
    if (ReplicatedOp) {
	ViceFid fids[MAXFIDS]; bzero(fids, (int) (MAXFIDS * sizeof(ViceFid)));
	fids[0] = Did;
	LogMsg(3, SrvDebugLevel, stdout, "Perform_RR: delete_me = %d, !delete_me = %d",
		vptr->delete_me, !vptr->delete_me);			    /* DEBUG - JJK */
	if (!vptr->delete_me) fids[1] = Fid;
	CopPendingMan->add(new cpent(StoreId, fids));
    }
}
#ifdef _TIMEPUTOBJS_
#undef CAMLIB_END_TOP_LEVEL_TRANSACTION_2 
#undef RVMLIB_END_TRANSACTION 

#define	RVMLIB_END_TRANSACTION(flush_mode, statusp)\
	/* User code goes in this block. */\
    }\
\
START_NSC_TIMING(PutObjects_TransactionEnd);\
    if (RvmType == RAWIO || RvmType == UFS) {\
	/* End the transaction. */\
	if (_status == 0/*RVM_SUCCESS*/) {\
	   if (flush_mode == no_flush) {\
		_status = rvm_end_transaction(_rvm_data->tid, flush_mode);\
                if ((_status == RVM_SUCCESS) && (_rvm_data->list.table != NULL))\
                _status = (rvm_return_t)rds_do_free(&_rvm_data->list, flush_mode);\
		}\
           else {\
              /* flush mode */\
              if (_rvm_data->list.table != NULL) {\
		_status = rvm_end_transaction(_rvm_data->tid, no_flush);\
                if (_status == RVM_SUCCESS) \
                _status = (rvm_return_t)rds_do_free(&_rvm_data->list, flush);\
	      }\
              else \
                 _status = rvm_end_transaction(_rvm_data->tid, flush);\
           }\
	}\
	if (statusp)\
	    *(statusp) = _status;\
	else {\
	    if (_status != RVM_SUCCESS) (_rvm_data->die)("EndTransaction: _status = %d", _status);\
	}\
\
	/* De-initialize the rvm_perthread_t object. */\
	_rvm_data->tid = 0;\
        if (_rvm_data->list.table) free(_rvm_data->list.table);\
    }\
    else if (RvmType == VM) {\
	if (statusp)\
	    *(statusp) = RVM_SUCCESS;\
    }\
    else {\
       assert(0);\
    }\
END_NSC_TIMING(PutObjects_TransactionEnd);\
}

#define CAMLIB_END_TOP_LEVEL_TRANSACTION_2(commitProt, status) \
    RVMLIB_END_TRANSACTION(flush, (&(status)))

#endif _TIMEPUTOBJS_

/*
  BEGIN_HTML
  <a name="PutObjects"><strong>Update and release vnodes and volume.  <br>
  	Flush changes in case of error.  </strong></a> 
  END_HTML
*/

/* Added UpdateVolume flag for quota resolution.  If true, PutObjects will
 * write out the volume header.
 */
void PutObjects(int errorCode, Volume *volptr, int LockLevel, 
		dlist *vlist, int blocks, int TranFlag, int UpdateVolume) {
    LogMsg(10, SrvDebugLevel, stdout,	"PutObjects: Vid = %x, errorCode = %d",
	     volptr ? V_id(volptr) : 0, errorCode);

    vmindex freed_indices;		// for truncating /purging resolution logs 
    /* Back out new disk allocation on failure. */
    if (errorCode && volptr)
	if (blocks != 0 && AdjustDiskUsage(volptr, -blocks) != 0)
	    LogMsg(0, SrvDebugLevel, stdout, "PutObjects: AdjustDiskUsage(%x, %d) failed", V_id(volptr), -blocks);

    /* Record these handles since we will need them after the objects are put! */
    Device device = (volptr ? V_device(volptr) : 0);
    VolumeId parentId = (volptr ? V_parentId(volptr) : 0);

START_TIMING(PutObjects_Transaction);
#ifdef _TIMEPUTOBJS_
    START_NSC_TIMING(PutObjects_Transaction);
#endif _TIMEPUTOBJS_

    /* Separate branches for Mutating and Non-Mutating cases are to avoid transaction in the latter. */
    if (TranFlag) {
	int status = 0;
	CAMLIB_BEGIN_TOP_LEVEL_TRANSACTION_2(CAM_TRAN_NV_SERVER_BASED);

	/* Put the directory pages, the vnodes, and the volume. */
	/* Don't mutate inodes until AFTER transaction commits. */
	if (vlist) {
	    dlist_iterator next(*vlist);
	    vle *v;
	    int count = 0;
	    while (v = (vle *)next())
		if (v->vptr) {
		    /* for resolution/reintegration yield every n vnodes */
		    if (LockLevel == NO_LOCK) {
			count++;
			if ((count & Yield_PutObjects_Mask) == 0)
			    PollAndYield();
		    }
		    /* Directory pages.  Be careful with cloned directories! */
		    if (v->vptr->disk.type == vDirectory) {
			if (errorCode == 0) {
			    if (v->d_cinode) {
				/* Discard ref to guy that was cloned. */
				DDec(v->d_cinode);

				/* Copy the VM inode to RVM, and free it. */
				DirInode *newinode = 0;
				assert(CopyDirInode((DirInode *)(v->vptr->disk.inodeNumber),
						    &newinode) == 0);
				assert(newinode != 0);
				VMFreeDirInode((DirInode *)(v->vptr->disk.inodeNumber));
				v->vptr->disk.inodeNumber = (Inode)newinode;
			    }

			    assert(DCommit(v->vptr) == 0);
			}
			else {
			    if (v->d_cinode) {
				/* Free the VM inode that was allocated in CopyOnWrite(). */
				VMFreeDirInode((DirInode *)(v->vptr->disk.inodeNumber));
				v->vptr->disk.inodeNumber = 0;
			    }

			    DirHandle dh;
			    SetDirHandle(&dh, v->vptr);
			    DZap((long *)&dh);

			    assert(DAbort(v->vptr) == 0);
			}
		    }

		    if (AllowResolution && volptr && V_VMResOn(volptr)) {
			/* Log mutation into volume log */
			olist_iterator next(v->sl);
			sle *SLE;
			int volindex = V_volumeindex(volptr);
			while(SLE = (sle *)next()) {
			    if (!errorCode) {
				LogMsg(49, SrvDebugLevel, stdout, "PutObjects: Appending log record");
				AppendRVMLogRecord(v->vptr, SLE->rec_index);	
			    }
			    else {
				char *rle = (char *)LogStore[volindex]->IndexToAddr(SLE->rec_index);
				LogStore[volindex]->FreeMem(rle);
			    }
			    SLE->rec_index = -1;
			}
			
		    }
		    if (AllowResolution && volptr && 
			V_RVMResOn(volptr) && (v->vptr->disk.type == vDirectory)) {
#ifdef _TIMEPUTOBJS_
    START_NSC_TIMING(PutObjects_RVM);
#endif _TIMEPUTOBJS_
			// make sure log header exists 
			if (!errorCode && !VnLog(v->vptr))
			    CreateResLog(volptr, v->vptr);
			
			// log mutation into recoverable volume log
			olist_iterator next(v->rsl);
			rsle *vmle;
			int volindex = V_volumeindex(volptr);
			while(vmle = (rsle *)next()) {
			    if (!errorCode) {
				LogMsg(9, SrvDebugLevel, stdout, 
				       "PutObjects: Appending recoverable log record");
				if (SrvDebugLevel > 39) vmle->print();
				vmle->CommitInRVM(volptr, v->vptr);
			    }
			    else 
				/* free up slot in vm bitmap */
				vmle->Abort(volptr);
			}

			// truncate/purge log if necessary and no errors have occured 
			if (!errorCode) {
			    if (v->d_needslogpurge) {
				assert(v->vptr->delete_me);
				if (VnLog(v->vptr)) {
				    PurgeLog(VnLog(v->vptr), volptr, &freed_indices);
				    VnLog(v->vptr) = NULL;
				}
			    }
			    else if (v->d_needslogtrunc) {
				assert(!v->vptr->delete_me);
				TruncateLog(volptr, v->vptr, &freed_indices);
			    }
			}

#ifdef _TIMEPUTOBJS_
    END_NSC_TIMING(PutObjects_RVM);
#endif _TIMEPUTOBJS_
		    }



		    /* Vnode. */
		    {
			Error fileCode = 0;
			/* Make sure that "allocated and abandoned" vnodes get deleted. */
			if (v->vptr->disk.inodeNumber == (Inode)NEWVNODEINODE) {
			    v->vptr->delete_me = 1;
			    v->vptr->disk.inodeNumber = 0;
			}
			if (errorCode == 0)
			    VPutVnode(&fileCode, v->vptr);
			else
			    VFlushVnode(&fileCode, v->vptr);

			assert(fileCode == 0);
		    }

		    v->vptr = 0;
		}
	}

	// for logs that have been truncated/purged deallocated entries in vm bitmap
	// should be done after transaction commits but here we are asserting 
	// if Transaction end status is not success
	if (errorCode == 0) 
	    FreeVMIndices(volptr, &freed_indices);

	/* Volume Header. */
        if (!errorCode && UpdateVolume) 
	    VUpdateVolume((Error *)&errorCode, volptr);

	/* Volume. */
	PutVolObj(&volptr, LockLevel);

	
	CAMLIB_END_TOP_LEVEL_TRANSACTION_2(CAM_PROT_TWO_PHASED, status);
	assert(status == 0);
    }
    else {
	if (vlist) {
	    dlist_iterator next(*vlist);
	    vle *v;
	    while (v = (vle *)next())
		if (v->vptr) {
		    /* Directory pages.  Cloning cannot occur without mutation! */
		    if (v->vptr->disk.type == vDirectory) {
			assert(v->d_cinode == 0);
		    }

		    /* Vnode. */
		    {
			/* Put rather than Flush even on failure since vnode wasn't mutated! */
			Error fileCode = 0;
			VPutVnode(&fileCode, v->vptr);
			assert(fileCode == 0);
		    }

		    v->vptr = 0;
		}
	}

	/* Volume. */
	PutVolObj(&volptr, LockLevel);
    }
END_TIMING(PutObjects_Transaction);
#ifdef _TIMEPUTOBJS_
    END_NSC_TIMING(PutObjects_Transaction);
#endif _TIMEPUTOBJS_    

    /* Post-transaction: handle inodes and clean-up the vlist. */
START_TIMING(PutObjects_Inodes); 
#ifdef _TIMEPUTOBJS_
    START_NSC_TIMING(PutObjects_Inodes);
#endif _TIMEPUTOBJS_    
    if (vlist) {
	vle *v;
	int count = 0;
	while (v = (vle *)vlist->get()) {
	    if (!ISDIR(v->fid)) {
		count++;
		if ((count && Yield_PutInodes_Mask) == 0)
		    PollAndYield();
		if (errorCode == 0) {
		    if (v->f_sinode)
			assert(idec((int) device, (int) v->f_sinode, parentId) == 0);
		    if (v->f_tinode) {
			LogMsg(3, SrvDebugLevel, stdout, "PutObjects: truncating (%x.%x.%x, %d, %d)",
				v->fid.Volume, v->fid.Vnode, v->fid.Unique,
				v->f_tinode, v->f_tlength);

			int fd;
			if ((fd = iopen((int) device, (int) v->f_tinode, O_RDWR)) < 0) {
			    LogMsg(0, SrvDebugLevel, stdout, "PutObjects: iopen(%d, %d) failed (%d)",
				    device, v->f_tinode, errno);
			    assert(0);
			}
			assert(ftruncate(fd, v->f_tlength) == 0);
			assert(close(fd) == 0);
		    }
		}
		else {
		    if (v->f_finode)
			assert(idec((int) device, (int) v->f_finode, parentId) == 0);
		}
	    }
	    if (AllowResolution) {
		/* clean up spooled log record list */
		sle *s;
		rsle *rs;
		while (s = (sle *)v->sl.get()) {
		    s->rec_index = -1;
		    delete s;
		}
		while (rs = (rsle *)v->rsl.get()) 
		    delete rs;
	    }
	    delete v;
	}

	delete vlist;
	vlist = 0;
    }
#ifdef _TIMEPUTOBJS_
    END_NSC_TIMING(PutObjects_Inodes);
#endif _TIMEPUTOBJS_    
END_TIMING(PutObjects_Inodes);

    LogMsg(10, SrvDebugLevel, stdout, "PutObjects: returning %s", ViceErrorMsg(0));
}

