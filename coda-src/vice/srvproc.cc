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

#*/

/*
                         IBM COPYRIGHT NOTICE

                          Copyright (C) 1986
             International Business Machines Corporation
                         All Rights Reserved

This  file  contains  some  code identical to or derived from the 1986
version of the Andrew File System ("AFS"), which is owned by  the  IBM
Corporation.   This  code is provided "AS IS" and IBM does not warrant
that it is free of infringement of  any  intellectual  rights  of  any
third  party.    IBM  disclaims  liability of any kind for any damages
whatsoever resulting directly or indirectly from use of this  software
or  of  any  derivative work.  Carnegie Mellon University has obtained
permission to  modify,  distribute and sublicense this code,  which is
based on Version 2  of  AFS  and  does  not  contain  the features and
enhancements that are part of  Version 3 of  AFS.  Version 3 of AFS is
commercially   available   and  supported  by   Transarc  Corporation,
Pittsburgh, PA.

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
#include <strings.h>
#include <inodeops.h>

#include <unistd.h>
#include <stdlib.h>
#include <util.h>
#include <rvmlib.h>
#include <partition.h>
#include <codadir.h>

#ifdef _TIMECALLS_
#include "histo.h"
#endif _TIMECALLS_
#include <partition.h>
#include <util.h>
#include <prs.h>
#include <al.h>
#include <callback.h>
#include <vice.h>
#include <cml.h>
#ifdef __cplusplus
}
#endif __cplusplus

#include <volume.h>
#include <srv.h>

#include <vmindex.h>
#include <voltypes.h>
#include <vicelock.h>
#include <vlist.h>
#include <vrdb.h>
#include <volume.h>
#include <repio.h>
#include <codadir.h>
#include <operations.h>
#include <lockqueue.h>
#include <resutil.h>
#include <ops.h>
#include <rsle.h>
#include <nettohost.h>
#include <cvnode.h>
#include <operations.h>
#include "coppend.h"

#ifdef _TIMECALLS_
#include "timecalls.h"
#endif _TIMECALLS_


/* From Vol package. */
extern void VCheckDiskUsage(Error *, Volume *, int );

extern void MakeLogNonEmpty(Vnode *);
extern void GetMaxVV(ViceVersionVector *, ViceVersionVector **, int);

extern int CheckReadMode(ClientEntry *, Vnode *);
extern int CheckWriteMode(ClientEntry *, Vnode *);
static void CopyOnWrite(Vnode *, Volume *);
extern int AdjustDiskUsage(Volume *, int);
extern int CheckDiskUsage(Volume *, int);
extern void ChangeDiskUsage(Volume *, int);
extern void HandleWeakEquality(Volume *, Vnode *, ViceVersionVector *);

/* These should be RPC routines. */
extern long FS_ViceGetAttr(RPC2_Handle, ViceFid *, int,
			 ViceStatus *, RPC2_Unsigned, RPC2_CountedBS *);
extern long FS_ViceGetACL(RPC2_Handle, ViceFid *, int, RPC2_BoundedBS *,
			ViceStatus *, RPC2_Unsigned, RPC2_CountedBS *);
extern long FS_ViceSetACL(RPC2_Handle, ViceFid *, RPC2_CountedBS *, ViceStatus *,
			RPC2_Unsigned, ViceStoreId *, RPC2_CountedBS *,
			RPC2_Integer *, CallBackStatus *, RPC2_CountedBS *);
extern long FS_ViceNewSetAttr(RPC2_Handle, ViceFid *, ViceStatus *, RPC2_Integer,
			   RPC2_Unsigned, ViceStoreId *, RPC2_CountedBS *, 
			   RPC2_Integer *, CallBackStatus *, RPC2_CountedBS *);

/* *****  Private routines  ***** */

static int GrabFsObj(ViceFid *, Volume **, Vnode **, int, int, int);
static int NormalVCmp(int, VnodeType, void *, void *);
static int StoreVCmp(int, VnodeType, void *, void *);
static int Check_CLMS_Semantics(ClientEntry *, Vnode **, Vnode **, char *, Volume **, VnodeType,
				  int, VCP, ViceStatus *, ViceStatus *, Rights *, Rights *, int);
static int Check_RR_Semantics(ClientEntry *, Vnode **, Vnode **, char *, Volume **, VnodeType,
				int, VCP, ViceStatus *, ViceStatus *, Rights *, Rights *, int);
static void Perform_CLMS(ClientEntry *, VolumeId, Volume *, Vnode *, Vnode *, int,
			   char *, Inode, RPC2_Unsigned, Date_t, RPC2_Unsigned,
			   int, ViceStoreId *, PDirInode *, int *, RPC2_Integer *);
static void Perform_RR(ClientEntry *, VolumeId, Volume *, Vnode *, Vnode *, char *,
			Date_t, int, ViceStoreId *, PDirInode *, int *, RPC2_Integer *);

/* Yield parameters (i.e., after how many loop iterations do I poll and yield). */
/* N.B.  Yield "periods" MUST all be power of two so that AND'ing can be used! */
const int Yield_PutObjects_Period = 8;
const int Yield_PutObjects_Mask = Yield_PutObjects_Period - 1;
const int Yield_PutInodes_Period = 16;
const int Yield_PutInodes_Mask = (Yield_PutInodes_Period - 1);
extern void PollAndYield();

/* From Writeback */
extern int CheckWriteBack(ViceFid * Fid, ClientEntry * client);

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
  ViceNewFetch":Fetch a file or directory
*/
long FS_ViceNewFetch(RPC2_Handle RPCid, ViceFid *Fid, ViceVersionVector *VV,
		ViceFetchType Request, RPC2_BoundedBS *AccessList,
		ViceStatus *Status, RPC2_Unsigned PrimaryHost,
		RPC2_Unsigned Offset, RPC2_Unsigned Quota,
		RPC2_CountedBS *PiggyBS, SE_Descriptor *BD)
{

    /* We should have separate RPC routines for these two! */
    if (Request == FetchNoData || Request == FetchNoDataRepair) {
	int inconok = (Request == FetchNoDataRepair);
	if (AccessList->MaxSeqLen == 0)
	    return(FS_ViceGetAttr(RPCid, Fid, inconok, Status, PrimaryHost, PiggyBS));
	else
	    return(FS_ViceGetACL(RPCid, Fid, inconok, AccessList, Status, PrimaryHost, PiggyBS));
    }

    int errorCode = 0;		/* return code to caller */
    Volume *volptr = 0;		/* pointer to the volume */
    ClientEntry *client = 0;	/* pointer to the client data */
    Rights rights = 0;		/* rights for this user */
    Rights anyrights = 0;	/* rights for any user */
    int inconok = 0;		/* flag to say whether Coda inconsistency is ok */
    VolumeId VSGVolnum = Fid->Volume;
    int ReplicatedOp;
    dlist *vlist = new dlist((CFN)VLECmp);
    vle *v;
    vle *av;

START_TIMING(Fetch_Total);
    SLog(1, "ViceNewFetch: Fid = %s, Repair = %d", FID_(Fid), 
	 (Request == FetchDataRepair));

  
    /* Validate parameters. */
    {
	if ((errorCode = ValidateParms(RPCid, &client, &ReplicatedOp,
				      &Fid->Volume, PiggyBS, NULL)))
	    goto FreeLocks;
	
	CheckWriteBack(Fid,client);

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
		SLog(0, "ViceNewFetch: illegal type %d", Request);
		errorCode = EINVAL;
		goto FreeLocks;
	}

	/* Deprecated/Inapplicable parameters. */
	if (AccessList->MaxSeqLen != 0) {
	    SLog(0, "ViceNewFetch: ACL != 0");
	    errorCode = EINVAL;
	    goto FreeLocks;
	}
    }

    /* Get objects. */
    {
	v = AddVLE(*vlist, Fid);
	if ((errorCode = GetFsObj(Fid, &volptr, &v->vptr, READ_LOCK, 
				 NO_LOCK, inconok, 0, 0)))
	    goto FreeLocks;

	/* This may violate locking protocol! -JJK */
	if (v->vptr->disk.type == vDirectory) {
	    av = v;
	} else {
	    ViceFid pFid;

	    VN_VN2PFid(v->vptr, volptr, &pFid);
	    av = AddVLE(*vlist, &pFid);
	    if ((errorCode = GetFsObj(&pFid, &volptr, &av->vptr, READ_LOCK, 
				     NO_LOCK, inconok, 0, 0)))
		goto FreeLocks;
	}
    }

    /* Check semantics. */
    {
	if ((errorCode = CheckFetchSemantics(client, &av->vptr, &v->vptr,
					    &volptr, &rights, &anyrights)))
	    goto FreeLocks;
    }

    /* Perform operation. */
    {
	if (!ReplicatedOp || PrimaryHost == ThisHostAddr)
	    if ((errorCode = FetchBulkTransfer(RPCid, client, volptr, v->vptr,
					      Offset, Quota, VV)))
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

    SLog(2, "ViceNewFetch returns %s", ViceErrorMsg(errorCode));
END_TIMING(Fetch_Total);
    return(errorCode);
}


/*
  ViceFetch":Fetch a file or directory
*/
long FS_ViceFetch(RPC2_Handle RPCid, ViceFid *Fid, ViceFid *BidFid,
		ViceFetchType Request, RPC2_BoundedBS *AccessList,
		ViceStatus *Status, RPC2_Unsigned PrimaryHost,
		RPC2_CountedBS *PiggyBS, SE_Descriptor *BD)
{
    return FS_ViceNewFetch(RPCid, Fid, NULL, Request, AccessList, Status,
			   PrimaryHost, 0, 0, PiggyBS, BD);
}


/*
 ViceGetAttr: Fetch the attributes for a file/directory
*/
long FS_ViceGetAttr(RPC2_Handle RPCid, ViceFid *Fid, 
		 int InconOK, ViceStatus *Status, 
		 RPC2_Unsigned PrimaryHost, 
		 RPC2_CountedBS *PiggyBS)
{
    int errorCode = 0;		/* return code to caller */
    Volume *volptr = 0;		/* pointer to the volume */
    ClientEntry *client = 0;	/* pointer to the client data */
    Rights rights = 0;		/* rights for this user */
    Rights anyrights = 0;	/* rights for any user */
    VolumeId VSGVolnum = Fid->Volume;
    int ReplicatedOp;
    dlist *vlist = new dlist((CFN)VLECmp);
    vle *v = 0;
    vle *av = 0;

START_TIMING(GetAttr_Total);

    SLog(1, "ViceGetAttr: Fid = %s, Repair = %d", FID_(Fid), InconOK);

    /* Validate parameters. */
    {
	if ((errorCode = ValidateParms(RPCid, &client, &ReplicatedOp, 
				      &Fid->Volume, PiggyBS, NULL)))
	    goto FreeLocks;
    }

    CheckWriteBack(Fid,client);


    /* Get objects. */
    {
	v = AddVLE(*vlist, Fid);
	if ((errorCode = GetFsObj(Fid, &volptr, &v->vptr, READ_LOCK, 
				 NO_LOCK, InconOK, 0, 0)))
	    goto FreeLocks;

	/* This may violate locking protocol! -JJK */
	if (v->vptr->disk.type == vDirectory) {
	    av = v;
	} else {
	    ViceFid pFid;

    VN_VN2PFid(v->vptr, volptr, &pFid);
	    av = AddVLE(*vlist, &pFid);
	    if ((errorCode = GetFsObj(&pFid, &volptr, &av->vptr, 
				     READ_LOCK, NO_LOCK, InconOK, 0, 0)))
		goto FreeLocks;
	}
    }

    /* Check semantics. */
    {
	if ((errorCode = CheckGetAttrSemantics(client, &av->vptr, &v->vptr,
					      &volptr, &rights, &anyrights)))
	    goto FreeLocks;
    }

    /* Perform operation. */
    {
	PerformGetAttr(client, volptr, v->vptr);

	SetStatus(v->vptr, Status, rights, anyrights);

	if(VolumeWriteable(volptr))
	    /* Until CVVV probes? -JJK */
	    if (1/*!ReplicatedOp || PrimaryHost == ThisHostAddr*/) 
		Status->CallBack = CodaAddCallBack(client->VenusId, Fid, 
						   VSGVolnum);
    }

FreeLocks:
    /* Put objects. */
    {
	PutObjects(errorCode, volptr, NO_LOCK, vlist, 0, 0);
    }

    SLog(2, "ViceGetAttr returns %s", ViceErrorMsg(errorCode));
END_TIMING(GetAttr_Total);
    return(errorCode);
}


/* 
 * assumes fids are given in order. a return of 1 in flags means that the 
 * client status is valid for that object, and that callback is set. 
 */

/*
  ViceValidateAttrs: A batched version of GetAttr
*/
long FS_ViceValidateAttrs(RPC2_Handle RPCid, RPC2_Unsigned PrimaryHost,
		       ViceFid *PrimaryFid, ViceStatus *Status, 
		       RPC2_Integer NumPiggyFids, ViceFidAndVV Piggies[],
		       RPC2_CountedBS *VFlagBS, RPC2_CountedBS *PiggyBS)
{
    long errorCode = 0;		/* return code to caller */
    Volume *volptr = 0;		/* pointer to the volume */
    ClientEntry *client = 0;	/* pointer to the client data */
    Rights rights = 0;		/* rights for this user */
    Rights anyrights = 0;	/* rights for any user */
    int ReplicatedOp;
    dlist *vlist = new dlist((CFN)VLECmp);
    vle *v = 0;
    vle *av = 0;
    int iErrorCode = 0;
    int i;

START_TIMING(ViceValidateAttrs_Total);
    SLog(1, "ViceValidateAttrs: Fid = %s, %d piggy fids", 
	 FID_(PrimaryFid), NumPiggyFids);

    /* Do a real getattr for primary fid. */
    {
	if ((errorCode = FS_ViceGetAttr(RPCid, PrimaryFid, 0, Status, 
				    PrimaryHost, PiggyBS)))
		goto Exit;
    }
 	
    if ( NumPiggyFids != VFlagBS->SeqLen ) {
	    SLog(0, "Client sending wrong output buffer while validating"
		 ": %s; SeqLen %d, should be %d", 
		 FID_(PrimaryFid), VFlagBS->SeqLen, NumPiggyFids);
	    errorCode = EINVAL;
	    goto Exit;
    }

    bzero((char *) VFlagBS->SeqBody, (int) NumPiggyFids);

    /* now check piggyback fids */
    for (i = 0; i < NumPiggyFids; i++) {

	/* save the replicated volume ID for the AddCallBack */    
	VolumeId VSGVolnum = Piggies[i].Fid.Volume;

	/* Validate parameters. */
        {
	    /* We've already dealt with the PiggyBS in the GetAttr above. */
	    if ((iErrorCode = ValidateParms(RPCid, &client, &ReplicatedOp, 
					   &Piggies[i].Fid.Volume, NULL, NULL)))
		goto InvalidObj;
	        CheckWriteBack(&Piggies[i].Fid,client);
        }

	/* Get objects. */
	{
	    v = AddVLE(*vlist, &Piggies[i].Fid);
	    if ((iErrorCode = GetFsObj(&Piggies[i].Fid, &volptr, 
				      &v->vptr, READ_LOCK, NO_LOCK, 0, 0, 0)))
		goto InvalidObj;

	    /* This may violate locking protocol! -JJK */
	    if (v->vptr->disk.type == vDirectory) {
		av = v;
	    } else {
		ViceFid pFid;
		pFid.Volume = Piggies[i].Fid.Volume;
		pFid.Vnode = v->vptr->disk.vparent;
		pFid.Unique = v->vptr->disk.uparent;
		av = AddVLE(*vlist, &pFid);
		if ((iErrorCode = GetFsObj(&pFid, &volptr, &av->vptr, 
					  READ_LOCK, NO_LOCK, 0, 0, 0)))
		    goto InvalidObj;

	    }
        }

	/* Check semantics. */
	{
	    if ((iErrorCode = CheckGetAttrSemantics(client, &av->vptr, &v->vptr,
						  &volptr, &rights, &anyrights)))
		goto InvalidObj;
	}

	/* Do it. */
	{
	    if (VV_Cmp(&Piggies[i].VV, &v->vptr->disk.versionvector) == VV_EQ) {
		    /* this is a writeable volume, o.w. we wouldn't be in this call */
		    /* Until CVVV probes? -JJK */
		    if (1/*!ReplicatedOp || PrimaryHost == ThisHostAddr*/) {
			    /* 
			     * we really should differentiate between 
			     * valid with no callback and invalid. that
			     * doesn't matter too much with this call, 
			     * because getting a callback is refetching.
			     */
			    VFlagBS->SeqBody[i] = (RPC2_Byte)
				    CodaAddCallBack(client->VenusId, 
						    &Piggies[i].Fid, 
						    VSGVolnum);
		    }

		    SLog(8, "ViceValidateAttrs: (%x.%x.%x) ok",
			   Piggies[i].Fid.Volume, 
			   Piggies[i].Fid.Vnode, 
			   Piggies[i].Fid.Unique);
		    continue;
	    }

InvalidObj:
	    SLog(0, "ViceValidateAttrs: (%x.%x.%x) failed!",
		   Piggies[i].Fid.Volume, Piggies[i].Fid.Vnode, 
		   Piggies[i].Fid.Unique);
	}
    }

    /* Put objects. */
    {
	PutObjects(iErrorCode, volptr, NO_LOCK, vlist, 0, 0);
    }

Exit:
    SLog(2, "ViceValidateAttrs returns %s, %d piggy fids checked", 
	   ViceErrorMsg((int)errorCode), VFlagBS->SeqLen);
END_TIMING(ViceValidateAttrs_Total);
    return(errorCode);
}

/*
  ViceGetACL: Fetch the acl of a directory
*/
long FS_ViceGetACL(RPC2_Handle RPCid, ViceFid *Fid, int InconOK, RPC2_BoundedBS *AccessList,
		 ViceStatus *Status, RPC2_Unsigned PrimaryHost, RPC2_CountedBS *PiggyBS)
{
    int errorCode = 0;		/* return code to caller */
    Volume *volptr = 0;		/* pointer to the volume */
    ClientEntry *client = 0;	/* pointer to the client data */
    Rights rights = 0;		/* rights for this user */
    Rights anyrights = 0;	/* rights for any user */
    RPC2_String eACL = 0;
    int ReplicatedOp;
    dlist *vlist = new dlist((CFN)VLECmp);
    vle *v = 0;

START_TIMING(GetACL_Total);
    SLog(1, "ViceGetACL: Fid = (%x.%x.%x), Repair = %d",
	     Fid->Volume, Fid->Vnode, Fid->Unique, InconOK);

    /* Validate parameters. */
    {
	if ((errorCode = ValidateParms(RPCid, &client, &ReplicatedOp,
				      &Fid->Volume, PiggyBS, NULL)))
	    goto FreeLocks;
    }
    CheckWriteBack(Fid,client);

    /* Get objects. */
    {
	v = AddVLE(*vlist, Fid);
	if ((errorCode = GetFsObj(Fid, &volptr, &v->vptr, READ_LOCK, NO_LOCK, InconOK, 0, 0)))
	    goto FreeLocks;
    }

    /* Check semantics. */
    {
	if ((errorCode = CheckGetACLSemantics(client, &v->vptr, &volptr, &rights,
					     &anyrights, AccessList, &eACL)))
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

    SLog(2, "ViceGetACL returns %s", ViceErrorMsg(errorCode));
END_TIMING(GetACL_Total);
    return(errorCode);
}


/*
  ViceNewVStore: Store a file or directory
*/
long FS_ViceNewVStore(RPC2_Handle RPCid, ViceFid *Fid, ViceStoreType Request,
		   RPC2_CountedBS *AccessList, ViceStatus *Status,
		   RPC2_Integer Length, RPC2_Integer Mask,
		   RPC2_Unsigned PrimaryHost,
		   ViceStoreId *StoreId, RPC2_CountedBS *OldVS, 
		   RPC2_Integer *NewVS, CallBackStatus *VCBStatus,
		   RPC2_CountedBS *PiggyBS, SE_Descriptor *BD)
{
    /* We should have separate RPC routines for these two! */
    if (Request == StoreStatus)
	return(FS_ViceNewSetAttr(RPCid, Fid, Status, Mask, PrimaryHost, StoreId, 
			      OldVS, NewVS, VCBStatus, PiggyBS));
    if (Request == StoreNeither)
	return(FS_ViceSetACL(RPCid, Fid, AccessList, Status, PrimaryHost, 
			  StoreId, OldVS, NewVS, VCBStatus, PiggyBS));

    int errorCode = 0;		/* return code for caller */
    Volume *volptr = 0;		/* pointer to the volume header */
    ClientEntry *client = 0;	/* pointer to client structure */
    Rights rights = 0;		/* rights for this user */
    Rights anyrights = 0;	/* rights for any user */
    int deltablocks = 0;
    VolumeId VSGVolnum = Fid->Volume;
    int	ReplicatedOp;
    dlist *vlist = new dlist((CFN)VLECmp);
    vle *v = 0;
    vle *av = 0;

START_TIMING(Store_Total);
    SLog(1, "ViceNewVStore: Fid = (%x.%x.%x)",
	     Fid->Volume, Fid->Vnode, Fid->Unique);

    /* Validate parameters. */
    {
	if ((errorCode = ValidateParms(RPCid, &client, &ReplicatedOp,
				      &Fid->Volume, PiggyBS, NULL)))
	    goto FreeLocks;
    
	CheckWriteBack(Fid,client);


	/* Request type. */
	switch(Request) {
	    case StoreStatus:
	    case StoreNeither:
		CODA_ASSERT(FALSE);

	    case StoreData:
	    case StoreStatusData:
		break;

	    default:
		SLog(0, "ViceNewVStore: illegal type %d", Request);
		errorCode = EINVAL;
		goto FreeLocks;
	}

	/* Deprecated/Inapplicable parameters. */
	if (AccessList->SeqLen != 0) {
	    SLog(0, "ViceNewVStore: !StoreNeither && SeqLen != 0");
	    errorCode = EINVAL;
	    goto FreeLocks;
	}
    }

    /* Get objects. */
    {
	v = AddVLE(*vlist, Fid);
	if ((errorCode = GetFsObj(Fid, &volptr, &v->vptr, WRITE_LOCK, SHARED_LOCK, 0, 0, 0)))
	    goto FreeLocks;

	/* This may violate locking protocol! -JJK */
	ViceFid pFid;
	pFid.Volume = Fid->Volume;
	pFid.Vnode = v->vptr->disk.vparent;
	pFid.Unique = v->vptr->disk.uparent;
	av = AddVLE(*vlist, &pFid);
	if ((errorCode = GetFsObj(&pFid, &volptr, &av->vptr, READ_LOCK, NO_LOCK, 0, 0, 0)))
	    goto FreeLocks;
    }

    /* Check semantics. */
    {
	if ((errorCode = CheckStoreSemantics(client, &av->vptr, &v->vptr, &volptr,
					    ReplicatedOp, StoreVCmp, &Status->VV,
					    Status->DataVersion, &rights, &anyrights)))
	    goto FreeLocks;
    }

    /* Perform operation. */
    {
	int tblocks = (int) (nBlocks(Length) - nBlocks(v->vptr->disk.length));
	if ((errorCode = AdjustDiskUsage(volptr, tblocks)))
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
	CODA_ASSERT(v->f_finode > (unsigned long) 0);

	if ((errorCode = StoreBulkTransfer(RPCid, client, volptr, v->vptr, v->f_finode, Length)))
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

    SLog(2, "ViceNewVStore returns %s", ViceErrorMsg(errorCode));
END_TIMING(Store_Total);
    return(errorCode);
}


/*
  ViceNewSetAttr: Set attributes of an object
*/
long FS_ViceNewSetAttr(RPC2_Handle RPCid, ViceFid *Fid, ViceStatus *Status,
		    RPC2_Integer Mask, RPC2_Unsigned PrimaryHost,		    
		    ViceStoreId *StoreId, 
		    RPC2_CountedBS *OldVS, RPC2_Integer *NewVS,
		    CallBackStatus *VCBStatus, RPC2_CountedBS *PiggyBS)
{
    int errorCode = 0;		/* return code for caller */
    Volume *volptr = 0;		/* pointer to the volume header */
    ClientEntry *client = 0;	/* pointer to client structure */
    Rights rights = 0;		/* rights for this user */
    Rights anyrights = 0;	/* rights for any user */
    int deltablocks = 0;
    VolumeId VSGVolnum = Fid->Volume;
    int	ReplicatedOp;
    dlist *vlist = new dlist((CFN)VLECmp);
    vle *v = 0;
    vle *av = 0;

START_TIMING(SetAttr_Total);
    SLog(1, "ViceNewSetAttr: Fid = (%x.%x.%x)",
	     Fid->Volume, Fid->Vnode, Fid->Unique);

    /* Validate parameters. */
    {
	if ((errorCode = ValidateParms(RPCid, &client, &ReplicatedOp,
				      &Fid->Volume, PiggyBS, NULL)))
	    goto FreeLocks;
    }
    CheckWriteBack(Fid,client);


    /* Get objects. */
    {
	v = AddVLE(*vlist, Fid);
	if ((errorCode = GetFsObj(Fid, &volptr, &v->vptr, WRITE_LOCK, SHARED_LOCK, 0, 0, 0)))
	    goto FreeLocks;

	/* This may violate locking protocol! -JJK */
	if (v->vptr->disk.type == vDirectory) {
	    av = v;
	} else {
	    ViceFid pFid;
	    pFid.Volume = Fid->Volume;
	    pFid.Vnode = v->vptr->disk.vparent;
	    pFid.Unique = v->vptr->disk.uparent;
	    av = AddVLE(*vlist, &pFid);
	    if ((errorCode = GetFsObj(&pFid, &volptr, &av->vptr, READ_LOCK, NO_LOCK, 0, 0, 0)))
		goto FreeLocks;
	}
    }

    /* Check semantics. */
    {
	if ((errorCode = CheckNewSetAttrSemantics(client, &av->vptr, &v->vptr, &volptr,
						 ReplicatedOp, NormalVCmp, Status->Length,
						 Status->Date, Status->Owner, Status->Mode,
						 Mask, &Status->VV, Status->DataVersion,
						 &rights, &anyrights)))
	    goto FreeLocks;
    }


    /* Perform operation. */
    {
	int truncp = 0;

	if (Mask & SET_LENGTH) {
	    truncp = 1;
	    int tblocks = (int) (nBlocks(Status->Length) - nBlocks(v->vptr->disk.length));
	    if ((errorCode = AdjustDiskUsage(volptr, tblocks)))
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

    if (AllowResolution && V_RVMResOn(volptr)) {
	if (ReplicatedOp && !errorCode &&
	    v->vptr->disk.type == vDirectory) {
	    SLog(0, "Going to spool store log record %u %u %u %u\n",
		   Status->Owner, Status->Mode, Status->Author, Status->Date);
	    if ((errorCode = SpoolVMLogRecord(vlist, v, volptr, StoreId, 
					     RES_NewStore_OP, STSTORE, 
					     Status->Owner, Status->Mode,
					     Status->Author, Status->Date,
					     Mask, &Status->VV)) )
		SLog(0, "ViceNewSetAttr: Error %d during SpoolVMLogRecord\n", 
		     errorCode);
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

    SLog(2, "ViceNewSetAttr returns %s", ViceErrorMsg(errorCode));
END_TIMING(SetAttr_Total);
    return(errorCode);
}

/*
  BEGIN_HTML
  <a name="ViceSetACL"><strong>Set the Access Control List for a directory</strong></a> 
  END_HTML
*/
long FS_ViceSetACL(RPC2_Handle RPCid, ViceFid *Fid, RPC2_CountedBS *AccessList,
		 ViceStatus *Status, RPC2_Unsigned PrimaryHost,
		 ViceStoreId *StoreId, 
		 RPC2_CountedBS *OldVS, RPC2_Integer *NewVS, 
		 CallBackStatus *VCBStatus, RPC2_CountedBS *PiggyBS)
{
    int errorCode = 0;		/* return code for caller */
    Volume *volptr = 0;		/* pointer to the volume header */
    ClientEntry *client = 0;	/* pointer to client structure */
    Rights rights = 0;		/* rights for this user */
    Rights anyrights = 0;	/* rights for any user */
    AL_AccessList *newACL = 0;
    VolumeId VSGVolnum = Fid->Volume;
    int	ReplicatedOp;
    dlist *vlist = new dlist((CFN)VLECmp);
    vle *v = 0;

START_TIMING(SetACL_Total);
    SLog(1, "ViceSetACL: Fid = (%x.%x.%x)",
	     Fid->Volume, Fid->Vnode, Fid->Unique);

    /* Validate parameters. */
    {
	if ((errorCode = ValidateParms(RPCid, &client, &ReplicatedOp,
				      &Fid->Volume, PiggyBS, NULL)))
	    goto FreeLocks;
    }
    CheckWriteBack(Fid,client);

    /* Get objects. */
    {
	v = AddVLE(*vlist, Fid);
	if ((errorCode = GetFsObj(Fid, &volptr, &v->vptr, WRITE_LOCK, SHARED_LOCK, 0, 0, 0)))
	    goto FreeLocks;
    }

    /* Check semantics. */
    {
	if ((errorCode = CheckSetACLSemantics(client, &v->vptr, &volptr, ReplicatedOp,
					      NormalVCmp, &Status->VV, Status->DataVersion,
					      &rights, &anyrights, AccessList, &newACL)))
	    goto FreeLocks;
    }

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

    if (AllowResolution && V_RVMResOn(volptr)) 
	if (ReplicatedOp && !errorCode) {
	    CODA_ASSERT(v->vptr->disk.type == vDirectory);
	    if ((errorCode = SpoolVMLogRecord(vlist, v, volptr, StoreId, 
					     RES_NewStore_OP, ACLSTORE, newACL)) )
		SLog(0, 
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

    SLog(2, "ViceSetACL returns %s", ViceErrorMsg(errorCode));
END_TIMING(SetACL_Total);
    return(errorCode);
}

/*
  ViceVCreate: Create an object with given name in its parent's directory
*/
long FS_ViceVCreate(RPC2_Handle RPCid, ViceFid *Did, ViceFid *BidFid,
		RPC2_String Name, ViceStatus *Status, ViceFid *Fid,
		ViceStatus *DirStatus, RPC2_Unsigned PrimaryHost,
		ViceStoreId *StoreId, RPC2_CountedBS *OldVS,
		RPC2_Integer *NewVS, CallBackStatus *VCBStatus,
		RPC2_CountedBS *PiggyBS)
{
    int errorCode = 0;		/* error code */
    Volume *volptr = 0;		/* pointer to the volume header */
    ClientEntry *client = 0;	/* pointer to client structure */
    Rights rights = 0;		/* rights for this user */
    Rights anyrights = 0;	/* rights for any user */
    int deltablocks = 0;
    VolumeId VSGVolnum = Did->Volume;
    int ReplicatedOp;
    dlist *vlist = new dlist((CFN)VLECmp);
    vle *pv = 0;
    vle *cv = 0;

START_TIMING(Create_Total);
#ifdef _TIMECALLS_
    START_NSC_TIMING(Create_Total);
#endif _TIMECALLS_
    SLog(1, "ViceCreate: %s, %s", FID_(Did), Name);

    /* Validate parameters. */
    {
	if ((errorCode = ValidateParms(RPCid, &client, &ReplicatedOp, 
				      &Did->Volume, PiggyBS, NULL)))
	    goto FreeLocks;
	CheckWriteBack(Fid,client);
	
	if (ReplicatedOp) {
	    /* Child/Parent volume match. */
	    if (Fid->Volume != VSGVolnum) {
		SLog(0, "ViceCreate: ChildVol (%x) != ParentVol (%x)",
			Fid->Volume, VSGVolnum);
		errorCode = EXDEV;
		goto FreeLocks;
	    }
	    Fid->Volume	= Did->Volume;	    /* manual XlateVid() */
	}

	/* Sanity. */
	if (FID_EQ(Did, Fid)) {
	    SLog(0, "ViceCreate: Did = Fid (%x.%x.%x)",
		    Did->Volume, Did->Vnode, Did->Unique);
	    errorCode = EINVAL;
	    goto FreeLocks;
	}

	/* Deprecated/Inapplicable parameters. */
	if (!FID_EQ(&NullFid, BidFid)) {
	    SLog(0, "ViceCreate: non-Null BidFid (%x.%x.%x)",
		    BidFid->Volume, BidFid->Vnode, BidFid->Unique);
	    errorCode = EINVAL;
	    goto FreeLocks;
	}
    }

    /* Get objects. */
    {
	pv = AddVLE(*vlist, Did);
	pv->d_inodemod = 1;
	if ((errorCode = GetFsObj(Did, &volptr, &pv->vptr, 
				 WRITE_LOCK, SHARED_LOCK, 0, 0, 
				 pv->d_inodemod)))
	    goto FreeLocks;

	/* This may violate locking protocol! -JJK */
	Vnode *vptr = 0;
	if ((errorCode = AllocVnode(&vptr, volptr, (ViceDataType)vFile, 
				   Fid, Did,
				   client->Id, PrimaryHost, &deltablocks))) {
	    CODA_ASSERT(vptr == 0);
	    goto FreeLocks;
	}
	cv = AddVLE(*vlist, Fid);
	cv->vptr = vptr;
    }

    /* Check semantics. */
    {
	if ((errorCode = CheckCreateSemantics(client, &pv->vptr, &cv->vptr, 
					     (char *)Name,
					     &volptr, ReplicatedOp, 
					     NormalVCmp,
					     DirStatus, Status, 
					     &rights, &anyrights, 1)))
	    goto FreeLocks;
    }

    /* Perform operation. */
    {
	int tblocks = 0;

	if (ReplicatedOp)
	    GetMyVS(volptr, OldVS, NewVS);
	PerformCreate(client, VSGVolnum, volptr, pv->vptr, cv->vptr, 
		      (char *)Name,
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

    if (AllowResolution && V_RVMResOn(volptr)) 
	/* Create Log Record */
	if (ReplicatedOp && !errorCode) 
	    if ((errorCode = SpoolVMLogRecord(vlist, pv, volptr, StoreId,
					     RES_Create_OP, 
					     Name, Fid->Vnode, Fid->Unique, 
					     client?client->Id:0)))
		SLog(0, 
		       "ViceCreate: Error %d during SpoolVMLogRecord\n",
		       errorCode);

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

    SLog(2, "ViceCreate returns %s", ViceErrorMsg(errorCode));
END_TIMING(Create_Total);
#ifdef _TIMECALLS_
    END_NSC_TIMING(Create_Total);
#endif _TIMECALLS_

    Fid->Volume	= VSGVolnum;	    /* Fid is an IN/OUT parameter; re-translate it. */
    return(errorCode);
}


/*
  ViceVRemove: Delete an object and its name
*/
long FS_ViceVRemove(RPC2_Handle RPCid, ViceFid *Did, RPC2_String Name,
		ViceStatus *DirStatus, ViceStatus *Status,
		RPC2_Unsigned PrimaryHost, ViceStoreId *StoreId,
		RPC2_CountedBS *OldVS, RPC2_Integer *NewVS, 
		CallBackStatus *VCBStatus, RPC2_CountedBS *PiggyBS)
{
    int errorCode = 0;		/* error code */
    Volume *volptr = 0;		/* pointer to the volume header */
    ViceFid Fid;		/* area for Fid from the directory */
    ClientEntry *client = 0;	/* pointer to client structure */
    Rights rights = 0;		/* rights for this user */
    Rights anyrights = 0;	/* rights for any user */
    int deltablocks = 0;
    VolumeId VSGVolnum = Did->Volume;
    int ReplicatedOp;
    dlist *vlist = new dlist((CFN)VLECmp);
    vle *pv = 0;
    vle *cv = 0;

    START_TIMING(Remove_Total);
#ifdef _TIMECALLS_
    START_NSC_TIMING(Remove_Total);
#endif _TIMECALLS_

    SLog(1, "ViceRemove: %s, %s", FID_(Did), Name);

    /* Validate parameters. */
    {
	if ((errorCode = ValidateParms(RPCid, &client, &ReplicatedOp, 
				      &Did->Volume, PiggyBS, NULL)))
	    goto FreeLocks;
    }
    CheckWriteBack(Did,client);

    /* Get objects. */
    {
	PDirHandle dh;
	pv = AddVLE(*vlist, Did);
	pv->d_inodemod = 1;
	if ((errorCode = GetFsObj(Did, &volptr, &pv->vptr, 
				 WRITE_LOCK, SHARED_LOCK, 0, 0, 
				 pv->d_inodemod)))
		goto FreeLocks;

	dh = VN_SetDirHandle(pv->vptr);
	errorCode = DH_Lookup(dh, (char *)Name, &Fid, CLU_CASE_SENSITIVE);
	VN_PutDirHandle(pv->vptr);
	if ( errorCode != 0) {
		errorCode = ENOENT;
		goto FreeLocks;
	}

	/* This may violate locking protocol! -JJK */
	FID_CpyVol(&Fid, Did);
	cv = AddVLE(*vlist, &Fid);
	if ((errorCode = GetFsObj(&Fid, &volptr, &cv->vptr, 
				 WRITE_LOCK, SHARED_LOCK, 0, 0, 0)))
	    goto FreeLocks;
    }

    /* Check semantics. */
    {
	if ((errorCode = CheckRemoveSemantics(client, &pv->vptr, &cv->vptr, 
					     (char *)Name,
					     &volptr, ReplicatedOp, NormalVCmp,
					     DirStatus, Status, &rights, 
					     &anyrights, 1)))
	    goto FreeLocks;
    }


    /* Perform operation. */
    {
	if (ReplicatedOp)
		GetMyVS(volptr, OldVS, NewVS);
	PerformRemove(client, VSGVolnum, volptr, pv->vptr, cv->vptr, (char *)Name,
		      DirStatus->Date, ReplicatedOp, StoreId, &pv->d_cinode, 
		      &deltablocks, NewVS);
	if (cv->vptr->delete_me) {
		pv->d_inodemod = 1;
		int tblocks = (int) -nBlocks(cv->vptr->disk.length);
		if ((errorCode = AdjustDiskUsage(volptr, tblocks)))
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

    if (AllowResolution && V_RVMResOn(volptr)) 
	    if (ReplicatedOp && !errorCode) {
		    ViceVersionVector ghostVV = cv->vptr->disk.versionvector;	    
		    if ((errorCode = SpoolVMLogRecord(vlist, pv, volptr, StoreId,
						     RES_Remove_OP, Name, Fid.Vnode, 
						     Fid.Unique, &ghostVV)))
		SLog(0, "ViceRemove: error %d during SpoolVMLogRecord\n",
		       errorCode);
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

    SLog(2, "ViceRemove returns %s", ViceErrorMsg(errorCode));
END_TIMING(Remove_Total);
#ifdef _TIMECALLS_
    END_NSC_TIMING(Remove_Total);
#endif _TIMECALLS_
    
    return(errorCode);
}


/*
ViceVLink: Create a new name for an already existing file
*/
long FS_ViceVLink(RPC2_Handle RPCid, ViceFid *Did, RPC2_String Name,
	       ViceFid *Fid, ViceStatus *Status, ViceStatus *DirStatus,
	       RPC2_Unsigned PrimaryHost, ViceStoreId *StoreId,
	       RPC2_CountedBS *OldVS, RPC2_Integer *NewVS, 
	       CallBackStatus *VCBStatus, RPC2_CountedBS *PiggyBS)
{
    int errorCode = 0;		/* error code */
    Volume *volptr = 0;		/* pointer to the volume header */
    ClientEntry *client = 0;	/* pointer to client structure */
    Rights rights = 0;		/* rights for this user */
    Rights anyrights = 0;	/* rights for any user */
    VolumeId VSGVolnum = Did->Volume;
    int ReplicatedOp;
    dlist *vlist = new dlist((CFN)VLECmp);
    vle *pv = 0;
    vle *cv = 0;
    int deltablocks = 0;

START_TIMING(Link_Total);
#ifdef _TIMECALLS_
    START_NSC_TIMING(Link_Total);
#endif _TIMECALLS_
    
    SLog(1, "ViceLink: %s, %s --> %s", FID_(Did), Name, FID_2(Fid));

    /* Validate parameters. */
    {
	if ((errorCode = ValidateParms(RPCid, &client, &ReplicatedOp,
				      &Did->Volume, PiggyBS, NULL)))
	    goto FreeLocks;
	CheckWriteBack(Fid,client);
    
	/* Volume match. */
	if (Fid->Volume != VSGVolnum) {
	    SLog(0, "ViceLink: ChildVol (%x) != ParentVol (%x)",
		    Fid->Volume, VSGVolnum);
	    errorCode = EXDEV;
	    goto FreeLocks;
	}
	if (ReplicatedOp)
	    Fid->Volume	= Did->Volume;	    /* manual XlateVid() */

	/* Sanity. */
	if (FID_EQ(Did, Fid)) {
	    SLog(0, "ViceLink: Did = Fid (%x.%x.%x)",
		    Did->Volume, Did->Vnode, Did->Unique);
	    errorCode = EINVAL;
	    goto FreeLocks;
	}
    }

    /* Get objects. */
    {
	pv = AddVLE(*vlist, Did);
	pv->d_inodemod = 1;
	if ((errorCode = GetFsObj(Did, &volptr, &pv->vptr, 
				 WRITE_LOCK, SHARED_LOCK, 0, 0, 	
				 pv->d_inodemod)))
	    goto FreeLocks;

	/* This may violate locking protocol! -JJK */
	cv = AddVLE(*vlist, Fid);
	if ((errorCode = GetFsObj(Fid, &volptr, &cv->vptr, 
				 WRITE_LOCK, SHARED_LOCK, 0, 0, 0)))
	    goto FreeLocks;
    }

    /* Check semantics. */
    {
	if ((errorCode = CheckLinkSemantics(client, &pv->vptr, &cv->vptr, (char *)Name,
					    &volptr, ReplicatedOp, NormalVCmp,
					    DirStatus, Status, &rights, &anyrights, 1)))
	    goto FreeLocks;
    }

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

    if (AllowResolution && V_RVMResOn(volptr)) 
	if (ReplicatedOp && !errorCode) {
	    if ((errorCode = SpoolVMLogRecord(vlist, pv, volptr, StoreId,
					     RES_Link_OP, (char *)Name, Fid->Vnode, Fid->Unique, 
					     &(Vnode_vv(cv->vptr)))))
		SLog(0, 
		       "ViceLink: Error %d during SpoolVMLogRecord\n",
		       errorCode);
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

    SLog(2, "ViceLink returns %s", ViceErrorMsg(errorCode));
END_TIMING(Link_Total);
#ifdef _TIMECALLS_
    END_NSC_TIMING(Link_Total);
#endif _TIMECALLS_
    
    return(errorCode);
}

/*
  ViceVRename: rename a file or directory
*/
long FS_ViceVRename(RPC2_Handle RPCid, ViceFid *OldDid, RPC2_String OldName,
		 ViceFid *NewDid, RPC2_String NewName, ViceStatus *OldDirStatus,
		 ViceStatus *NewDirStatus, ViceStatus *SrcStatus,
		 ViceStatus *TgtStatus, RPC2_Unsigned PrimaryHost,
		 ViceStoreId *StoreId,  RPC2_CountedBS *OldVS,
		RPC2_Integer *NewVS, CallBackStatus *VCBStatus,
		RPC2_CountedBS *PiggyBS)
{
    int errorCode = 0;		/* error code */
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
    int ReplicatedOp;
    int SameParent = FID_EQ(OldDid, NewDid);
    dlist *vlist = new dlist((CFN)VLECmp);
    vle *spv = 0;
    vle *tpv = 0;
    vle *sv = 0;
    vle *tv = 0;
START_TIMING(Rename_Total);
#ifdef _TIMECALLS_
    START_NSC_TIMING(Rename_Total);
#endif _TIMECALLS_
    
    SLog(1, "ViceRename: (%x.%x.%x), %s --> (%x.%x.%x), %s",
	    OldDid->Volume, OldDid->Vnode, OldDid->Unique, OldName,
	    NewDid->Volume, NewDid->Vnode, NewDid->Unique, NewName);

    /* Validate parameters. */
    {
	if ((errorCode = ValidateParms(RPCid, &client, &ReplicatedOp,
				      &OldDid->Volume, PiggyBS, NULL)))
	    goto FreeLocks;
	CheckWriteBack(NewDid,client);
	CheckWriteBack(OldDid,client);

	/* Volume match. */
	if (NewDid->Volume != VSGVolnum) {
	    SLog(0, "ViceRename: TgtVol (%x) != SrcVol (%x)",
		    NewDid->Volume, VSGVolnum);
	    errorCode = EXDEV;
	    goto FreeLocks;
	}
	if (ReplicatedOp)
	    NewDid->Volume = OldDid->Volume;	    /* manual XlateVid() */
    }

    /* Get objects. */
    {
	PDirHandle sdh;
	PDirHandle tdh;

	spv = AddVLE(*vlist, OldDid);
	spv->d_inodemod = 1;
	if ((errorCode = GetFsObj(OldDid, &volptr, &spv->vptr, 
				 WRITE_LOCK, SHARED_LOCK, 0, 0,
				 spv->d_inodemod)))
	    goto FreeLocks;
	sdh = DC_DC2DH(spv->vptr->dh);

	/* This may violate locking protocol! -JJK */
	if (SameParent) {
	    tpv = spv;
	    tdh = sdh;
	} else {
	    tpv = AddVLE(*vlist, NewDid);
	    tpv->d_inodemod = 1;
	    errorCode = GetFsObj(NewDid, &volptr, &tpv->vptr, 
				 WRITE_LOCK, SHARED_LOCK, 0, 0, 	    
				 tpv->d_inodemod);
	    if (errorCode) {
		    VN_PutDirHandle(spv->vptr);
		    goto FreeLocks;
	    }
	    tdh = DC_DC2DH(tpv->vptr->dh);
	}

	/* This may violate locking protocol! -JJK */

	if (DH_Lookup(sdh, (char *)OldName, &SrcFid, CLU_CASE_SENSITIVE) != 0) {
		VN_PutDirHandle(spv->vptr);
		if ( tdh != sdh )
			VN_PutDirHandle(tpv->vptr);
		errorCode = ENOENT;
		goto FreeLocks;
	}

	FID_CpyVol(&SrcFid, OldDid);
	sv = AddVLE(*vlist, &SrcFid);
	if ( ISDIR(SrcFid) )
		sv->d_inodemod = 1;
	if ((errorCode = GetFsObj(&SrcFid, &volptr, &sv->vptr, 
				 WRITE_LOCK, SHARED_LOCK, 0, 0, 
				 sv->d_inodemod)))
	    goto FreeLocks;

	/* This may violate locking protocol! -JJK */

	errorCode = DH_Lookup(tdh, (char *)NewName, &TgtFid, CLU_CASE_SENSITIVE);
	if (errorCode == ENOENT ) {
		errorCode = 0;
	} else if ( errorCode ) {
		VN_PutDirHandle(tpv->vptr);
		if ( tdh != sdh )
			VN_PutDirHandle(spv->vptr);
		if ( sv->vptr->disk.type == vDirectory )
			VN_PutDirHandle(sv->vptr);
		goto FreeLocks;
	} else {
		FID_CpyVol(&TgtFid, NewDid);
		tv = AddVLE(*vlist, &TgtFid);
		if ( ISDIR(TgtFid) )
			tv->d_inodemod = 1;
		if ((errorCode = GetFsObj(&TgtFid, &volptr, &tv->vptr, 
					 WRITE_LOCK, 
					 SHARED_LOCK, 0, 0,
					 tv->d_inodemod)))
			goto FreeLocks;
	}
    }

    /* Check semantics. */
    {
	if ((errorCode = CheckRenameSemantics(client, &spv->vptr, &tpv->vptr, 
					     &sv->vptr,
					     (char *)OldName, 
					     (tv ? &tv->vptr : 0), 
					     (char *)NewName,
					     &volptr, ReplicatedOp, 
					     NormalVCmp, OldDirStatus,
					     NewDirStatus, SrcStatus, 
					     TgtStatus,
					     &sp_rights, &sp_anyrights, 
					     &tp_rights,
					     &tp_anyrights, &s_rights, 
					     &s_anyrights, 1, 0)))
	    goto FreeLocks;
    }

    /* Perform operation. */
    {
	if (ReplicatedOp)
		GetMyVS(volptr, OldVS, NewVS);
	PerformRename(client, VSGVolnum, volptr, spv->vptr, tpv->vptr, 
		      sv->vptr, (tv ? tv->vptr : 0), (char *)OldName, 
		      (char *)NewName, OldDirStatus->Date,
		      ReplicatedOp, StoreId, &spv->d_cinode, 
		      &tpv->d_cinode, &sv->d_cinode, NULL,
		      NewVS);
	if (tv && tv->vptr->delete_me) {
		int tblocks = (int) -nBlocks(tv->vptr->disk.length);
		if ((errorCode = AdjustDiskUsage(volptr, tblocks)))
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

    /* spool rename log record for recoverable rvm logs */
    if (AllowResolution && V_RVMResOn(volptr) && ReplicatedOp && !errorCode) 
	errorCode = SpoolRenameLogRecord((int) RES_Rename_OP,(dlist *)  vlist, sv->vptr, 
					 (Vnode *)(tv ? tv->vptr : NULL), spv->vptr, 
					 tpv->vptr, 
					 volptr, (char *)OldName, 
					 (char *)NewName, StoreId);

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

    SLog(2, "ViceRename returns %s", ViceErrorMsg(errorCode));
END_TIMING(Rename_Total);
#ifdef _TIMECALLS_
    END_NSC_TIMING(Rename_Total);
#endif _TIMECALLS_
    return(errorCode);
}

/*
  ViceVMakeDir: Make a new directory
*/
long FS_ViceVMakeDir(RPC2_Handle RPCid, ViceFid *Did, RPC2_String Name,
		  ViceStatus *Status, ViceFid *NewDid, ViceStatus *DirStatus,
		  RPC2_Unsigned PrimaryHost, ViceStoreId *StoreId,
		  RPC2_CountedBS *OldVS,
		  RPC2_Integer *NewVS, CallBackStatus *VCBStatus,
		  RPC2_CountedBS *PiggyBS)
{
    int errorCode = 0;		/* error code */
    Volume *volptr = 0;		/* pointer to the volume header */
    ClientEntry *client = 0;	/* pointer to client structure */
    Rights rights = 0;		/* rights for this user */
    Rights anyrights = 0;	/* rights for any user */
    int deltablocks = 0;
    VolumeId VSGVolnum = Did->Volume;
    int ReplicatedOp;
    dlist *vlist = new dlist((CFN)VLECmp);
    vle *pv = 0;
    vle *cv = 0;

START_TIMING(MakeDir_Total);
#ifdef _TIMECALLS_
    START_NSC_TIMING(MakeDir_Total);
#endif _TIMECALLS_
    SLog(1, "ViceMakeDir: %s, %s", FID_(Did), Name);
    
    /* Validate parameters. */
    {
	if ((errorCode = ValidateParms(RPCid, &client, &ReplicatedOp, 
				      &Did->Volume, PiggyBS, NULL)))
	    goto FreeLocks;
	CheckWriteBack(Did,client);

	/* Child/Parent volume match. */
	if (ReplicatedOp) {
	    if (NewDid->Volume != VSGVolnum) {
		SLog(0, "ViceMakeDir: ChildVol (%x) != ParentVol (%x)",
			NewDid->Volume, VSGVolnum);
		errorCode = EXDEV;
		goto FreeLocks;
	    }
	    NewDid->Volume = Did->Volume;	/* manual XlateVid() */
	}

	/* Sanity. */
	if (FID_EQ(Did, NewDid)) {
	    SLog(0, "ViceMakeDir: Did = NewDid %s", FID_(Did));
	    errorCode = EINVAL;
	    goto FreeLocks;
	}
    }

    /* Get objects. */
    {
	pv = AddVLE(*vlist, Did);
	pv->d_inodemod = 1;
	errorCode = GetFsObj(Did, &volptr, &pv->vptr, 
			     WRITE_LOCK, SHARED_LOCK, 0, 0,
			     pv->d_inodemod);
	if (errorCode)
	    goto FreeLocks;

	/* This may violate locking protocol! -JJK */
	Vnode *vptr = 0;
	if ((errorCode = AllocVnode(&vptr, volptr, (ViceDataType)vDirectory, 
				   NewDid,
				   Did, client->Id, PrimaryHost, 
				   &deltablocks))) {
	    CODA_ASSERT(vptr == 0);
	    goto FreeLocks;
	}
	cv = AddVLE(*vlist, NewDid);
	cv->vptr = vptr;
	/* mark the new directory pages as dirty */
	cv->d_inodemod = 1;
    }

    /* Check semantics. */
    {
	if ((errorCode = CheckMkdirSemantics(client, &pv->vptr, &cv->vptr, 
					    (char *)Name,
					     &volptr, ReplicatedOp, NormalVCmp,
					     DirStatus, Status, &rights, 
					    &anyrights, 1)))
	    goto FreeLocks;
    }

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
	if ( errorCode == 0 )
		CODA_ASSERT(DC_Dirty(cv->vptr->dh));

	/* Until CVVV probes? -JJK */
	if (1/*!ReplicatedOp || PrimaryHost == ThisHostAddr*/) 
	    Status->CallBack = CodaAddCallBack(client->VenusId, NewDid, VSGVolnum);
	if (ReplicatedOp)
	    SetVSStatus(client, volptr, NewVS, VCBStatus);
    }
	if ( errorCode == 0 )
		CODA_ASSERT(DC_Dirty(cv->vptr->dh));


    if (AllowResolution && V_RVMResOn(volptr)) 
	if (ReplicatedOp && !errorCode) {
	    if ((errorCode = SpoolVMLogRecord(vlist, pv, volptr, StoreId,
					     RES_MakeDir_OP, Name, 
					     NewDid->Vnode, NewDid->Unique, 
					     client?client->Id:0)) )
		SLog(0, "ViceMakeDir: Error %d during SpoolVMLogRecord for parent\n",
		     errorCode);
	    // spool child's log record 
	    if ( errorCode == 0 )
		    CODA_ASSERT(DC_Dirty(cv->vptr->dh));
	    if (!errorCode && (errorCode = SpoolVMLogRecord(vlist, cv, volptr, StoreId, 
							    RES_MakeDir_OP, ".",
							    NewDid->Vnode, 
							    NewDid->Unique,
							    client?client->Id:0)))
		SLog(0, "ViceMakeDir: Error %d during SpoolVMLogRecord for child\n",
		     errorCode);
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
	if ( errorCode == 0 )
		CODA_ASSERT(DC_Dirty(cv->vptr->dh));
	PutObjects(errorCode, volptr, SHARED_LOCK, vlist, deltablocks, 1);
    }

    SLog(2, "ViceMakeDir returns %s", ViceErrorMsg(errorCode));
END_TIMING(MakeDir_Total);
#ifdef _TIMECALLS_
    END_NSC_TIMING(MakeDir_Total);
#endif _TIMECALLS_
    
    NewDid->Volume = VSGVolnum;		/* NewDid is an IN/OUT paramter; re-translate it. */
    return(errorCode);
}


/*
  ViceVRemoveDir: Delete an empty directory
*/
long FS_ViceVRemoveDir(RPC2_Handle RPCid, ViceFid *Did, RPC2_String Name,
		    ViceStatus *DirStatus, ViceStatus *Status,
		    RPC2_Unsigned PrimaryHost, ViceStoreId *StoreId,
		    RPC2_CountedBS *OldVS,
		    RPC2_Integer *NewVS, CallBackStatus *VCBStatus,
		    RPC2_CountedBS *PiggyBS)
{
    int errorCode = 0;		/* error code */
    Volume *volptr = 0;		/* pointer to the volume header */
    ViceFid ChildDid;		/* area for Fid from the directory */
    ClientEntry *client = 0;	/* pointer to client structure */
    Rights rights = 0;		/* rights for this user */
    Rights anyrights = 0;	/* rights for any user */
    int deltablocks = 0;
    VolumeId VSGVolnum = Did->Volume;
    int ReplicatedOp;
    dlist *vlist = new dlist((CFN)VLECmp);
    vle *pv = 0;
    vle *cv = 0;

START_TIMING(RemoveDir_Total);
#ifdef _TIMECALLS_
    START_NSC_TIMING(RemoveDir_Total)
#endif _TIMECALLS_
	    SLog(1, "ViceRemoveDir: %s, %s", FID_(Did), Name);

    /* Validate parameters. */
    {
	if ((errorCode = ValidateParms(RPCid, &client, &ReplicatedOp, 
				      &Did->Volume, PiggyBS, NULL)))
	    goto FreeLocks;
    }
    CheckWriteBack(Did,client);

    /* Get objects. */
    {
	PDirHandle dh;
	pv = AddVLE(*vlist, Did);
	pv->d_inodemod = 1;
	if ((errorCode = GetFsObj(Did, &volptr, &pv->vptr, 
				 WRITE_LOCK, SHARED_LOCK, 0, 0,
				 pv->d_inodemod)))
	    goto FreeLocks;

	/* This may violate locking protocol! -JJK */
	dh = VN_SetDirHandle(pv->vptr);

	errorCode = DH_Lookup(dh, (char *)Name, &ChildDid, CLU_CASE_SENSITIVE);
	VN_PutDirHandle(pv->vptr);
	if ( errorCode != 0) {
		errorCode = ENOENT;
		goto FreeLocks;
	}

	FID_CpyVol(&ChildDid, Did);
	cv = AddVLE(*vlist, &ChildDid);
	cv->d_inodemod = 1;
	if ((errorCode = GetFsObj(&ChildDid, &volptr, &cv->vptr, 
				 WRITE_LOCK, SHARED_LOCK, 0, 0, 
				 cv->d_inodemod))) {
		VN_PutDirHandle(pv->vptr);
		goto FreeLocks;
	}

    }

    /* Check semantics. */
    {
	if ((errorCode = CheckRmdirSemantics(client, &pv->vptr, &cv->vptr, 
					    (char *)Name,
					    &volptr, ReplicatedOp, NormalVCmp,
					    DirStatus, Status, 
					    &rights, &anyrights, 1)))
		goto FreeLocks;
    }

    /* Perform operation. */
    {
	if (ReplicatedOp)
		GetMyVS(volptr, OldVS, NewVS);
	PerformRmdir(client, VSGVolnum, volptr, pv->vptr, cv->vptr, 
		     (char *)Name,
		     DirStatus->Date, ReplicatedOp, StoreId, &pv->d_cinode, 
		     &deltablocks, NewVS);
	{
	    CODA_ASSERT(cv->vptr->delete_me);
	    /* note that the directories are modified */
	    int tblocks = (int) -nBlocks(cv->vptr->disk.length);
	    if ((errorCode = AdjustDiskUsage(volptr, tblocks)))
		goto FreeLocks;
	    deltablocks += tblocks;
	}

	SetStatus(pv->vptr, DirStatus, rights, anyrights);
	SetStatus(cv->vptr, Status, rights, anyrights);
	if (ReplicatedOp)
	    SetVSStatus(client, volptr, NewVS, VCBStatus);
    }

    if (AllowResolution && V_RVMResOn(volptr)) {
	if (ReplicatedOp) 
	    if ((errorCode = SpoolVMLogRecord(vlist, pv, volptr, StoreId,
					     RES_RemoveDir_OP, Name, 
					     ChildDid.Vnode, ChildDid.Unique, 
					     VnLog(cv->vptr), &(Vnode_vv(cv->vptr).StoreId),
					     &(Vnode_vv(cv->vptr).StoreId))))
		SLog(0, 
		       "ViceRemoveDir: error %d in SpoolVMLogRecord\n",
		       errorCode);
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

    SLog(2, "ViceRemoveDir returns %s", ViceErrorMsg(errorCode));
END_TIMING(RemoveDir_Total);
#ifdef _TIMECALLS_
    END_NSC_TIMING(RemoveDir_Total);
#endif _TIMECALLS_
    
    return(errorCode);
}

/*
  ViceVSymLink: Create a symbolic link
*/
long FS_ViceVSymLink(RPC2_Handle RPCid, ViceFid *Did, RPC2_String NewName,
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
    Volume *volptr = 0;		/* pointer to the volume header */
    ClientEntry *client = 0;	/* pointer to client structure */
    Rights rights = 0;		/* rights for this user */
    Rights anyrights = 0;		/* rights for any user */
    int deltablocks = 0;
    VolumeId VSGVolnum = Did->Volume;
    int ReplicatedOp;
    dlist *vlist = new dlist((CFN)VLECmp);
    vle *pv = 0;
    vle *cv = 0;

START_TIMING(SymLink_Total);
#ifdef _TIMECALLS_
    START_NSC_TIMING(SymLink_Total);
#endif _TIMECALLS_

    SLog(1, "ViceSymLink: (%x.%x.%x), %s --> %s",
	    Did->Volume, Did->Vnode, Did->Unique, NewName, OldName);

    /* Validate parameters. */
    {
	if ((errorCode = ValidateParms(RPCid, &client, &ReplicatedOp,
				      &Did->Volume, PiggyBS, NULL)))
	    goto FreeLocks;
	CheckWriteBack(Fid,client);
	CheckWriteBack(Did,client);

	/* Child/Parent volume match. */
	if (ReplicatedOp) {
	    if (Fid->Volume != VSGVolnum) {
		SLog(0, "ViceSymLink: ChildVol (%x) != ParentVol (%x)",
			Fid->Volume, VSGVolnum);
		errorCode = EXDEV;
		goto FreeLocks;
	    }
	    Fid->Volume	= Did->Volume;	    /* manual XlateVid() */
	}

	/* Sanity. */
	if (FID_EQ(Did, Fid)) {
	    SLog(0, "ViceSymLink: Did = Fid (%x.%x.%x)",
		    Did->Volume, Did->Vnode, Did->Unique);
	    errorCode = EINVAL;
	    goto FreeLocks;
	}
    }

    /* Get objects. */
    {
	pv = AddVLE(*vlist, Did);
	pv->d_inodemod = 1;
	if ((errorCode = GetFsObj(Did, &volptr, &pv->vptr, 
				 WRITE_LOCK, SHARED_LOCK, 0, 0, 
				 pv->d_inodemod)))
	    goto FreeLocks;

	/* This may violate locking protocol! -JJK */
	Vnode *vptr = 0;
	if ((errorCode = AllocVnode(&vptr, volptr, (ViceDataType)vSymlink, 
				   Fid, Did,
				   client->Id, PrimaryHost, &deltablocks))) {
	    CODA_ASSERT(vptr == 0);
	    goto FreeLocks;
	}
	cv = AddVLE(*vlist, Fid);
	cv->vptr = vptr;
    }

    /* Check semantics. */
    {
	if ((errorCode = CheckSymlinkSemantics(client, &pv->vptr, &cv->vptr, 
					      (char *)NewName,
					      &volptr, ReplicatedOp, 
					      NormalVCmp, DirStatus, Status, 
					      &rights, &anyrights, 1)))
	    goto FreeLocks;
    }

    /* Perform operation. */
    {
	cv->f_finode = icreate((int) V_device(volptr), 0, (int) V_id(volptr),
			       (int) cv->vptr->vnodeNumber,
			       (int) cv->vptr->disk.uniquifier, 1);
	CODA_ASSERT(cv->f_finode > 0);
	int linklen = (int) strlen((char *)OldName);
	CODA_ASSERT(iwrite((int) V_device(volptr), (int) cv->f_finode, (int) V_parentId(volptr),
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

    if (AllowResolution && V_RVMResOn(volptr)) {
	/* Create Log Record */
	if (ReplicatedOp) 
	    if ((errorCode = SpoolVMLogRecord(vlist, pv, volptr, StoreId,
					     RES_SymLink_OP, 
					     NewName, Fid->Vnode, Fid->Unique,
					     client?client->Id:0)) )
		SLog(0, 
		       "ViceSymLink: Error %d in SpoolVMLogRecord\n",
		       errorCode);
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

    SLog(2, "ViceSymLink returns %s", ViceErrorMsg(errorCode));
END_TIMING(SymLink_Total);
#ifdef _TIMECALLS_
    END_NSC_TIMING(SymLink_Total);
#endif _TIMECALLS_
    Fid->Volume	= VSGVolnum;		/* Fid is an IN/OUT parameter; re-translate it. */
    return(errorCode);
}



/*  *****  Utility Routines  *****  */

/* Initialize return status from a vnode. */
void SetStatus(struct Vnode *vptr, ViceStatus *status, Rights rights, Rights anyrights)
{
    status->InterfaceVersion = 1;

    /* fill in VnodeType  */
    switch (vptr->disk.type) {
    case vFile:
	    status->VnodeType = File;
	    break;
    case vDirectory:
	    status->VnodeType = Directory;
	    break;
    case vSymlink:
	    status->VnodeType = SymbolicLink;
	    break;
    default:
	    status->VnodeType = Invalid; 
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
    static int  anyid = 0;
    static  PRS_InternalCPS * anyCPS = 0;

    if (anyid == 0) {
	if (AL_NameToId("System:AnyUser", &anyid) != 0) {
	    SLog(0, "UserID anyuser not known");
	} else if (AL_GetInternalCPS(anyid, &anyCPS) != 0) {
	    SLog(0, "UserID anyuser no CPS");
	}
    }

    if (anyid != 0) {
	if (AL_CheckRights(ACL, anyCPS, (int *)anyrights) != 0) {
		    SLog(0, "CheckRights failed");
		    anyrights = 0;
	}
    } else {
	    *anyrights = -1;
    }
    
    if (AL_CheckRights(ACL, CPS, (int *)rights) != 0) {
	    *rights = 0;
    }

    /* When a client can throw away it's tokens, and then perform some
     * operation, the client essentially has the same rights when
     * authenticated */
    *rights |= *anyrights;

    return(0);
}

/*

  GrabFsObj Get a pointer to (and lock) a particular volume and vnode

*/
static int GrabFsObj(ViceFid *fid, Volume **volptr, Vnode **vptr, 
		      int lock, int ignoreIncon, int VolumeLock) 
{

	int GotVolume = 0;
	/* Get the volume. */
	if ((*volptr) == 0) {
		int errorCode = GetVolObj(fid->Volume, volptr, VolumeLock, 0, 0);
		if (errorCode) {
			SLog(0, "GrabFsObj, GetVolObj error %s", 
			     ViceErrorMsg(errorCode));
			return(errorCode);
		}
		GotVolume = 1;
	}
    
	/* Get the vnode.  */
	if (*vptr == 0) {
		int errorCode = 0;
		*vptr = VGetVnode((Error *)&errorCode, *volptr, 
				  fid->Vnode, fid->Unique, lock, 
				  ignoreIncon, 0);
		if (errorCode) {
			SLog(1, "GrabFsObj: VGetVnode error %s", ViceErrorMsg(errorCode));
			if (GotVolume){
				SLog(1, "GrabFsObj: Releasing volume 0x%x", V_id(*volptr));
				PutVolObj(volptr, VolumeLock, 0);
			}
			return(errorCode);
		}
	}
    
	/* Sanity check the uniquifiers. */
	if ((*vptr)->disk.uniquifier != fid->Unique) {
		SLog(0, "GrabFsObj, uniquifier mismatch, disk = %x, fid = %x",
		     (*vptr)->disk.uniquifier, fid->Unique);
		if (GotVolume){
			SLog(0, "GrabFsObj: Releasing volume 0x%x", 
			     V_id(*volptr));
			PutVolObj(volptr, VolumeLock, 0);
		}
		return(EINVAL);
	}
	
	return(0);
}


/* Formerly CheckVnode(). */
/* ignoreBQ parameter is obsolete - not used any longer */
/*
  GetFsObj: Get a filesystem object
*/
int GetFsObj(ViceFid *fid, Volume **volptr, Vnode **vptr,
	     int lock, int VolumeLock, int ignoreIncon, int ignoreBQ, 
	     int getdirhandle) 
{
	int errorCode;
	PDirHandle pdh = NULL;

	/* Sanity check the Fid. */
	if (fid->Volume == 0 || fid->Vnode == 0 || fid->Unique == 0)
	    return(EINVAL);

	/* Grab the volume and vnode with the appropriate lock. */
	errorCode = GrabFsObj(fid, volptr, vptr, lock, ignoreIncon, 
			      VolumeLock);

	if ( errorCode )
		return errorCode;

	if ( ISDIR(*fid) && getdirhandle ) {
		pdh = VN_SetDirHandle(*vptr);
		if ( !pdh )
			return ENOENT;
	}
	return 0;
}


/* Permits only Strong Equality. */
static int NormalVCmp(int ReplicatedOp, VnodeType type, void *arg1, void *arg2) {
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
static int StoreVCmp(int ReplicatedOp, VnodeType type, void *arg1, void *arg2) {
    CODA_ASSERT(type == vFile);

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
	    bzero((void *)vvs, (int)(VSG_MEMBERS * sizeof(ViceVersionVector *)));
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



/* CopyOnWrite: copy out the inode for a cloned Vnode
   - directories: see dirvnode.cc
   - files: copy the file: the current way is disastrous.
     XXXXXXX
*/
static void CopyOnWrite(Vnode *vptr, Volume *volptr)
{
	Inode    ino;
	int	     rdlen;
	int      wrlen;
	int      size;
	char   * buff;

	if (vptr->disk.type == vDirectory) {

		SLog(0, "CopyOnWrite: Copying directory vnode = %d", 
		     vptr->vnodeNumber);
		VN_CopyOnWrite(vptr);

	} else {
		SLog(0, "CopyOnWrite: Copying inode for files (vnode%d)", 
		     vptr->vnodeNumber);
		size = (int) vptr->disk.length;
		ino = icreate((int) V_device(volptr), 0, (int) V_id(volptr),
			      (int) vptr->vnodeNumber, 
			      (int) vptr->disk.uniquifier, 
			      (int) vptr->disk.dataVersion);
		CODA_ASSERT(ino > 0);
		if (size > 0) {
			buff = (char *)malloc(size);
			CODA_ASSERT(buff != 0);
			rdlen = iread((int) V_device(volptr), 
				      (int) vptr->disk.inodeNumber, 
				      (int) V_parentId(volptr), 0, buff, size);
			START_TIMING(CopyOnWrite_iwrite);
			wrlen = iwrite((int) V_device(volptr), (int) ino, 
				       (int) V_parentId(volptr), 0, buff, size);
			END_TIMING(CopyOnWrite_iwrite);
			CODA_ASSERT(rdlen == wrlen);
			free(buff);
		}

		/*
		  START_TIMING(CopyOnWrite_idec);
		  CODA_ASSERT(!(idec(V_device(volptr), 
		  vptr->disk.inodeNumber, V_parentId(volptr))));
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
	    SLog(0, "Partition %s that contains volume %u is full",
		    volptr->partition->name, V_id(volptr));
	    return(rc);
	}
	if(rc == EDQUOT) {
	    SLog(0, "Volume %u (%s) is full", V_id(volptr), V_name(volptr));
	    return(rc);
	}
	SLog(0, "Got error return %s from VAdjustDiskUsage",
	     ViceErrorMsg(rc));
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
	SLog(0, "Partition %s that contains volume %u is full",
		    volptr->partition->name, V_id(volptr));
	return (rc);
    }
    else {
	SLog(0, "Volume %u (%s) is full",
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
  ValidateParms: Validate the parameters of the RPC
*/
int ValidateParms(RPC2_Handle RPCid, ClientEntry **client, int *ReplicatedOp,
		  VolumeId *Vidp, RPC2_CountedBS *PiggyBS, int *Nservers) 
{
    int errorCode = 0;
    int replicated;
    VolumeId GroupVid;
    int count, pos;

    /* 1. Apply PiggyBacked COP2 operations. */
    if (PiggyBS && PiggyBS->SeqLen > 0)
    {
	errorCode = (int)FS_ViceCOP2(RPCid, PiggyBS);
	if (errorCode)
	    return(errorCode);
    }

    /* 2. Map RPC handle to client structure. */
    errorCode = (int) RPC2_GetPrivatePointer(RPCid, (char **)client);
    if (errorCode != RPC2_SUCCESS) {
	SLog(0, "ValidateParms: GetPrivatePointer failed (%d)", errorCode);
	return(errorCode);
    }
    if (*client == 0 || (*client)->DoUnbind) {
	SLog(0, "ValidateParms: GetPrivatePointer --> Null client");
	return(EINVAL);
    }

    /* 3. Translate group to read/write volume id. */
    GroupVid = *Vidp;
    replicated = XlateVid(Vidp, &count, &pos);

    if (ReplicatedOp)
	*ReplicatedOp = replicated;

    if (Nservers)
	*Nservers = count;
	    
    if ( replicated ) {
	    SLog(10, "ValidateParms: %x --> %x", GroupVid, *Vidp);
    } else {
	    SLog(10, "ValidateParms: using replica %x", *Vidp);
    }

    return(0);
}


int AllocVnode(Vnode **vptr, Volume *volptr, ViceDataType vtype, ViceFid *Fid,
		ViceFid *pFid, UserId ClientId, RPC2_Unsigned AllocHost, int *blocks) 
{
    int errorCode = 0;
    Error fileCode = 0;
    int ReplicatedOp = (AllocHost != 0);
    *vptr = 0;
    *blocks = 0;

START_TIMING(AllocVnode_Total);
    SLog(10, "AllocVnode: Fid = (%x.%x.%x), type = %d, pFid = (%x.%x.%x), Owner = %d",
	     Fid->Volume, Fid->Vnode, Fid->Unique, vtype,
	     pFid->Volume, pFid->Vnode, pFid->Unique, ClientId);

    /* Validate parameters. */
    if (ReplicatedOp)
	CODA_ASSERT(V_id(volptr) == Fid->Volume);
    CODA_ASSERT(V_id(volptr) == pFid->Volume);
    CODA_ASSERT(vtype == vFile || vtype == vDirectory || vtype == vSymlink);

    /* Allocate/Retrieve the new vnode. */
    int tblocks = (vtype == vFile)
      ? EMPTYFILEBLOCKS
      : (vtype == vDirectory)
      ? EMPTYDIRBLOCKS
      : EMPTYSYMLINKBLOCKS;
    if (ReplicatedOp) {
	if (AllocHost == ThisHostAddr) {
	    if ((errorCode = GetFsObj(Fid, &volptr, vptr, WRITE_LOCK, SHARED_LOCK, 0, 0, 0)))
		goto FreeLocks;
	}
	else {
	    if ((errorCode = AdjustDiskUsage(volptr, tblocks)))
		goto FreeLocks;
	    *blocks = tblocks;

	    *vptr = VAllocVnode((Error *)&errorCode, volptr,
				vtype, Fid->Vnode, Fid->Unique);
	    if (errorCode != 0)
		goto FreeLocks;
	}
    }
    else {
	if ((errorCode = AdjustDiskUsage(volptr, tblocks)))
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
		SLog(0, "AllocVnode: AdjustDiskUsage(%x, %d) failed",
			V_id(volptr), -(*blocks));

	    *blocks = 0;
	}

	if (*vptr) {
START_TIMING(AllocVnode_Transaction);
	    rvm_return_t status = RVM_SUCCESS;
	    rvmlib_begin_transaction(restore);
	    VPutVnode(&fileCode, *vptr);
	    CODA_ASSERT(fileCode == 0);
	    *vptr = 0;
	    rvmlib_end_transaction(flush, &(status));
END_TIMING(AllocVnode_Transaction);
	}
    }

    SLog(2, "AllocVnode returns %s", ViceErrorMsg(errorCode));
END_TIMING(AllocVnode_Total);
    return(errorCode);
}


int CheckFetchSemantics(ClientEntry *client, Vnode **avptr, Vnode **vptr,
			 Volume **volptr, Rights *rights, Rights *anyrights)
{
    ViceFid Fid;
    Fid.Volume = V_id(*volptr);
    Fid.Vnode = (*vptr)->vnodeNumber;
    Fid.Unique = (*vptr)->disk.uniquifier;
    int IsOwner = (client->Id == (long)(*vptr)->disk.owner);

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
	CODA_ASSERT(GetRights(client->CPS, aCL, aCLSize, rights, anyrights) == 0);

	/* LOOKUP or READ permission (normally) required. */
	/* READ mode-bits also (normally) required for files. */
	if (SystemUser(client)) {
	    if (((*vptr)->disk.type == vDirectory && !(*rights & PRSFS_LOOKUP)) ||
		((*vptr)->disk.type != vDirectory && !(*rights & PRSFS_READ))) {
		SLog(1, "CheckFetchSemantics: rights violation (%x : %x) (%x.%x.%x)",
			*rights, *anyrights,
			Fid.Volume, Fid.Vnode, Fid.Unique);
		return(EACCES);
	    }

	    if (!IsOwner && (*vptr)->disk.type == vFile) {
		if (CheckReadMode(client, *vptr)) {
		    SLog(1, "CheckFetchSemantics: mode-bits violation (%x.%x.%x)",
			    Fid.Volume, Fid.Vnode, Fid.Unique);
		    return(EACCES);
		}
	    }
	}
    }

    return(0);
}


int CheckGetAttrSemantics(ClientEntry *client, Vnode **avptr, Vnode **vptr,
			   Volume **volptr, Rights *rights, Rights *anyrights)
{
    ViceFid Fid;

    VN_VN2Fid(*vptr, *volptr, &Fid);

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
	CODA_ASSERT(GetRights(client->CPS, aCL, aCLSize, rights, anyrights) == 0);
    }

    return(0);
}


int CheckGetACLSemantics(ClientEntry *client, Vnode **vptr,
			  Volume **volptr, Rights *rights, Rights *anyrights,
			  RPC2_BoundedBS *AccessList, RPC2_String *eACL) {
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
	    SLog(0, "CheckGetACLSemantics: non-directory (%x.%x.%x)",
		    Fid.Volume, Fid.Vnode, Fid.Unique);
	    return(ENOTDIR);
	}
	if (AccessList->MaxSeqLen == 0) {
	    SLog(0, "CheckGetACLSemantics: zero-len ACL (%x.%x.%x)",
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
	CODA_ASSERT(GetRights(client->CPS, aCL, aCLSize, rights, anyrights) == 0);

	CODA_ASSERT(AL_Externalize(aCL, (AL_ExternalAccessList *)eACL) == 0);
	int eACLlen = (int)(strlen((char *)*eACL) + 1);
	if (eACLlen > AccessList->MaxSeqLen) {
	    SLog(0, "CheckGetACLSemantics: eACLlen (%d) > ACL->MaxSeqLen (%d) (%x.%x.%x)",
		    eACLlen, AccessList->MaxSeqLen,
		    Fid.Volume, Fid.Vnode, Fid.Unique);
	    return(E2BIG);
	}
	SLog(10, "CheckGetACLSemantics: ACL is:\n%s", *eACL);
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
    int IsOwner = (client->Id == (long)(*vptr)->disk.owner);
    int Virginal = ((*vptr)->disk.inodeNumber == 0);

    /* Concurrency-control checks. */
    if (VCmpProc) {
	void *arg1 = (ReplicatedOp ? (void *)&Vnode_vv(*vptr) : (void *)&(*vptr)->disk.dataVersion);
	void *arg2 = (ReplicatedOp ? (void *)VV : (void *)&DataVersion);
	if ((errorCode = VCmpProc(ReplicatedOp, (*vptr)->disk.type, arg1, arg2))) {
	    SLog(0, "CheckStoreSemantics: (%x.%x.%x), VCP error (%d)",
		    Fid.Volume, Fid.Vnode, Fid.Unique, errorCode);
	    return(errorCode);
	}
    }

    /* Integrity checks. */
    {
	if ((*vptr)->disk.type != vFile) {
	    SLog(0, "CheckStoreSemantics: (%x.%x.%x) not a file",
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
	CODA_ASSERT(GetRights(client->CPS, aCL, aCLSize, rights, anyrights) == 0);

	/* WRITE permission (normally) required. */
	if (!(*rights & PRSFS_WRITE) && !(IsOwner && Virginal)) {
	    SLog(0, "CheckStoreSemantics: rights violation (%x : %x) (%x.%x.%x)",
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
    int IsOwner = (client->Id == (long)(*vptr)->disk.owner);
    int chmodp = Mask & SET_MODE;
    int chownp = Mask & SET_OWNER;
    int truncp = Mask & SET_LENGTH;
    int utimesp = Mask & SET_TIME;
    int Virginal = ((*vptr)->disk.inodeNumber == 0);

    /* Concurrency-control checks. */
    if (VCmpProc) {
	void *arg1 = (ReplicatedOp ? (void *)&Vnode_vv(*vptr) : (void *)&(*vptr)->disk.dataVersion);
	void *arg2 = (ReplicatedOp ? (void *)VV : (void *)&DataVersion);
	if ((errorCode = VCmpProc(ReplicatedOp, (*vptr)->disk.type, arg1, arg2))) {
	    SLog(0, "CheckNewSetAttrSemantics: (%x.%x.%x), VCP error (%d)",
		    Fid.Volume, Fid.Vnode, Fid.Unique, errorCode);
	    return(errorCode);
	}
    }

    /* Integrity checks. */
    {
	if (truncp) { 
	    if ((*vptr)->disk.type != vFile) {
		SLog(0, "CheckNewSetAttrSemantics: non-file truncate (%x.%x.%x)",
			Fid.Volume, Fid.Vnode, Fid.Unique);
		return(EISDIR);
	    }
	    if (Length > (long)(*vptr)->disk.length) {
		SLog(0, "CheckNewSetAttrSemantics: truncate length bad (%x.%x.%x)",
			Fid.Volume, Fid.Vnode, Fid.Unique);
		return(EINVAL);
	    }
	}

	/* just log a message if nothing is changing - don't be paranoid about returning an error */
	if (chmodp + chownp + truncp + utimesp == 0) {
	    SLog(0, "CheckNewSetAttrSemantics: no attr set (%x.%x.%x)",
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
	CODA_ASSERT(GetRights(client->CPS, aCL, aCLSize, rights, anyrights) == 0);

	if (IsOwner && Virginal) {
	    /* Bypass protection checks on first store after a create EXCEPT for chowns. */
	    if (chownp) {
		SLog(0, "CheckNewSetAttrSemantics: owner chown'ing virgin (%x.%x.%x)",
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
			SLog(0, "CheckNewSetAttrSemantics: rights violation (%x : %x) (%x.%x.%x)",
				*rights, *anyrights,
				Fid.Volume, Fid.Vnode, Fid.Unique);
			return(EACCES);
		    }
		}
		else {
		    if (!(*rights & PRSFS_WRITE)) {
			SLog(0, "CheckNewSetAttrSemantics: rights violation (%x : %x) (%x.%x.%x)",
				*rights, *anyrights,
				Fid.Volume, Fid.Vnode, Fid.Unique);
			return(EACCES);
		    }
		}

		if (chmodp) {
		    /* No further access checks. */
		}

		if (chownp && !IsOwner) {
		    SLog(0, "CheckNewSetAttrSemantics: non-owner chown'ing (%x.%x.%x)",
			    Fid.Volume, Fid.Vnode, Fid.Unique);
		    return(EACCES);
		}

		if (truncp && CheckWriteMode(client, (*vptr)) != 0) {
		    SLog(0, "CheckNewSetAttrSemantics: truncating (%x.%x.%x)",
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
    int IsOwner = (client->Id == (long)(*vptr)->disk.owner);

    /* Concurrency-control checks. */
    if (VCmpProc) {
	void *arg1 = (ReplicatedOp ? (void *)&Vnode_vv(*vptr) : (void *)&(*vptr)->disk.dataVersion);
	void *arg2 = (ReplicatedOp ? (void *)VV : (void *)&DataVersion);
	if ((errorCode = VCmpProc(ReplicatedOp, (*vptr)->disk.type, arg1, arg2))) {
	    SLog(0, "CheckSetACLSemantics: (%x.%x.%x), VCP error (%d)",
		    Fid.Volume, Fid.Vnode, Fid.Unique, errorCode);
	    return(errorCode);
	}
    }

    /* Integrity checks. */
    {
	if ((*vptr)->disk.type != vDirectory) {
	    SLog(0, "CheckSetACLSemantics: non-directory (%x.%x.%x)",
		    Fid.Volume, Fid.Vnode, Fid.Unique);
	    return(ENOTDIR);
	}
	if (AccessList->SeqLen == 0) {
	    SLog(0, "CheckSetACLSemantics: zero-len ACL (%x.%x.%x)",
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
	CODA_ASSERT(GetRights(client->CPS, aCL, aCLSize, rights, anyrights) == 0);

	/* ADMINISTER permission (normally) required. */
	if (!(*rights & PRSFS_ADMINISTER) && !IsOwner && SystemUser(client)) {
	    SLog(0, "CheckSetACLSemantics: rights violation (%x : %x) (%x.%x.%x)",
		    *rights, *anyrights,
		    Fid.Volume, Fid.Vnode, Fid.Unique);
	    return(EACCES);
	}
	if (AL_Internalize((AL_ExternalAccessList) AccessList->SeqBody, newACL) != 0) {
	    SLog(0, "CheckSetACLSemantics: ACL internalize failed (%x.%x.%x)",
		    Fid.Volume, Fid.Vnode, Fid.Unique);
	    return(EINVAL);
	}
	if ((*newACL)->MySize + 4 > aCLSize) {
	    SLog(0, "CheckSetACLSemantics: ACL too big (%x.%x.%x)",
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
    ViceFid TDid;			/* Target directory */
    ViceFid SFid;			/* Source object */
    ViceFid TFid;			/* Target object (if it exists) */

    SDid.Volume = V_id(*volptr);
    SDid.Vnode = (*s_dirvptr)->vnodeNumber;
    SDid.Unique = (*s_dirvptr)->disk.uniquifier;

    TDid.Volume = V_id(*volptr);
    TDid.Vnode = (*t_dirvptr)->vnodeNumber;
    TDid.Unique = (*t_dirvptr)->disk.uniquifier;

    int SameParent = (FID_EQ(&SDid, &TDid));
    SFid.Volume = V_id(*volptr);
    SFid.Vnode = (*s_vptr)->vnodeNumber;
    SFid.Unique = (*s_vptr)->disk.uniquifier;

    int TargetExists = (t_vptr != 0);
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
	    if ((errorCode = VCmpProc(ReplicatedOp, (*s_dirvptr)->disk.type, arg1, arg2))) {
		SLog(0, "CheckRenameSemantics: (%x.%x.%x), VCP error (%d)",
			SDid.Volume, SDid.Vnode, SDid.Unique, errorCode);
		return(errorCode);
	    }
	}


	/* Target directory. */
	{
	    arg1 = (ReplicatedOp ? (void *)&Vnode_vv(*t_dirvptr) : (void *)&(*t_dirvptr)->disk.dataVersion);
	    arg2 = (ReplicatedOp ? (void *)&t_dirstatus->VV : (void *)&t_dirstatus->DataVersion);
	    if ((errorCode = VCmpProc(ReplicatedOp, (*t_dirvptr)->disk.type, arg1, arg2))) {
		SLog(0, "CheckRenameSemantics: (%x.%x.%x), VCP error (%d)",
			TDid.Volume, TDid.Vnode, TDid.Unique, errorCode);
		return(errorCode);
	    }
	}

	/* Source object. */
	{
	    arg1 = (ReplicatedOp ? (void *)&Vnode_vv(*s_vptr) : (void *)&(*s_vptr)->disk.dataVersion);
	    arg2 = (ReplicatedOp ? (void *)&s_status->VV : (void *)&s_status->DataVersion);
	    if ((errorCode = VCmpProc(ReplicatedOp, (*s_vptr)->disk.type, arg1, arg2))) {
		SLog(0, "CheckRenameSemantics: (%x.%x.%x), VCP error (%d)",
			SFid.Volume, SFid.Vnode, SFid.Unique, errorCode);
		return(errorCode);
	    }
	}

	/* Target object. */
	if (TargetExists) {
	    arg1 = (ReplicatedOp ? (void *)&Vnode_vv(*t_vptr) : (void *)&(*t_vptr)->disk.dataVersion);
	    arg2 = (ReplicatedOp ? (void *)&t_status->VV : (void *)&t_status->DataVersion);
	    if ((errorCode = VCmpProc(ReplicatedOp, (*t_vptr)->disk.type, arg1, arg2))) {
		SLog(0, "CheckRenameSemantics: (%x.%x.%x), VCP error (%d)",
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
	    SLog(0, "CheckRenameSemantics: not directory src: %s tgt: %s", 
		    FID_(&SDid), FID_2(&TDid));
	    return(ENOTDIR);
	}

	/* Source object. */
	{
	    if (STREQ(OldName, ".") || STREQ(OldName, "..")) {
		SLog(0, "CheckRenameSemantics: illegal old-name (%s)", OldName);
		return(EINVAL);
	    }

	    if ((*s_vptr)->disk.vparent != SDid.Vnode ||
		(*s_vptr)->disk.uparent != SDid.Unique) {
		SLog(0, "CheckRenameSemantics: source (%x.%x.%x) not in parent (%x.%x.%x)", 
			SFid.Volume, SFid.Vnode, SFid.Unique,
			SDid.Volume, SDid.Vnode, SDid.Unique);
		return(EINVAL);
	    }

	    if (FID_EQ(&SDid, &SFid) || FID_EQ(&TDid, &SFid)) {
		SLog(0, "CheckRenameSemantics: (%x.%x.%x) (%x.%x.%x) (%x.%x.%x) (%x.%x.%x)", 
			SDid.Volume, SDid.Vnode, SDid.Unique,
			TDid.Volume, TDid.Vnode, TDid.Unique,
			SFid.Volume, SFid.Vnode, SFid.Unique,
			TFid.Volume, TFid.Vnode, TFid.Unique);
		return(ELOOP);
	    }

	    /* Cannot allow rename out of a directory if file has multiple links! */
	    if (!SameParent && (*s_vptr)->disk.type == vFile && (*s_vptr)->disk.linkCount > 1) {
		SLog(0, "CheckRenameSemantics: !SameParent and multiple links (%x.%x.%x)",   SFid.Volume, SFid.Vnode, SFid.Unique);
		return(EXDEV);
	    }
	}

	/* Target object. */
	{
	    if (STREQ(NewName, ".") || STREQ(NewName, "..")) {
		SLog(0, "CheckRenameSemantics: illegal new-name (%s)", NewName);
		return(EINVAL);
	    }

	    if (TargetExists) {
		if ((*t_vptr)->disk.vparent != TDid.Vnode ||
		    (*t_vptr)->disk.uparent != TDid.Unique) {
		    SLog(0, "CheckRenameSemantics: target (%x.%x.%x) not in parent (%x.%x.%x)", 
			    TFid.Volume, TFid.Vnode, TFid.Unique,
			    TDid.Volume, TDid.Vnode, TDid.Unique);
		    return(EINVAL);
		}

		if (FID_EQ(&SDid, &TFid) || FID_EQ(&TDid, &TFid) || FID_EQ(&SFid, &TFid)) {
		    SLog(0, "CheckRenameSemantics: (%x.%x.%x) (%x.%x.%x) (%x.%x.%x) (%x.%x.%x)", 
			    SDid.Volume, SDid.Vnode, SDid.Unique,
			    TDid.Volume, TDid.Vnode, TDid.Unique,
			    SFid.Volume, SFid.Vnode, SFid.Unique,
			    TFid.Volume, TFid.Vnode, TFid.Unique);
		    return(ELOOP);
		}

		if ((*t_vptr)->disk.type == vDirectory) {
		    if((*s_vptr)->disk.type != vDirectory) {
			SLog(0, "CheckRenameSemantics: target is dir, source is not");
			return(ENOTDIR);
		    }

		    PDirHandle dh;
		    dh = VN_SetDirHandle(*t_vptr);
		    if (!DH_IsEmpty(dh)) {
			SLog(0, "CheckRenameSemantics: target %s not empty", 
			     FID_(&TFid));
			if (!IgnoreTargetNonEmpty) {
			    SLog(0, "CheckRenameSemantics: Ignoring Non-Empty target directory");
			    VN_PutDirHandle(*t_vptr);
			    return(ENOTEMPTY);	
			}
		    }
		    VN_PutDirHandle(*t_vptr);
		}
		else {
		    if((*s_vptr)->disk.type == vDirectory) {
			SLog(0, "CheckRenameSemantics: source is dir, target is not");
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
		if (FID_EQ(&TestFid, &SDid)) {
		    TestFid.Vnode = (*s_dirvptr)->disk.vparent;
		    TestFid.Unique = (*s_dirvptr)->disk.uparent;
		    continue;
		}

		if (FID_EQ(&TestFid, &SFid) || FID_EQ(&TestFid, &TDid) ||
		    (TargetExists && FID_EQ(&TestFid, &TFid))) {
		    SLog(0, "CheckRenameSemantics: loop detected");
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
			CODA_ASSERT(errorCode == 0);
		    } else {
			CODA_ASSERT(errorCode == EWOULDBLOCK);
			/* 
			 * Someone has the object locked.  If this is part of a
			 * reintegration, check the supplied vlist for the vnode.
			 * If it has already been locked (by us) the vnode number
			 * and uniquefier may be copied out safely, and there is
			 * no need to call VPutVnode in that case.  If the vnode
			 * is not on the vlist, return rather than be antisocial.
			 */
			SLog(0, 
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
	    CODA_ASSERT(GetRights(client->CPS, aCL, aCLSize, sd_rights, sd_anyrights) == 0);

	    if (!(*sd_rights & PRSFS_DELETE)) {
		SLog(0, "CheckRenameSemantics: sd rights violation (%x : %x) (%x.%x.%x)", 
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
	    CODA_ASSERT(GetRights(client->CPS, aCL, aCLSize, td_rights, td_anyrights) == 0);

	    if (!(*td_rights & PRSFS_INSERT) ||
		(TargetExists && !(*td_rights & PRSFS_DELETE))) {
		SLog(0, "CheckRenameSemantics: td rights violation (%x : %x) (%x.%x.%x)", 
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
		CODA_ASSERT(GetRights(client->CPS, aCL, aCLSize, s_rights, s_anyrights) == 0);
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
  Check_CLMS_Semantics: Make semantic checks for the create, link,
  mkdir and symlink requests.
*/

/* {vptr,status} may be Null for {Create,Mkdir,Symlink}. */
static int Check_CLMS_Semantics(ClientEntry *client, Vnode **dirvptr, Vnode **vptr, char *Name,
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
	if ((errorCode = VCmpProc(ReplicatedOp, (*dirvptr)->disk.type, arg1, arg2))) {
	    SLog(0, "%s: (%x.%x.%x), VCP error (%d)",
		    ProcName, Did.Volume, Did.Vnode, Did.Unique, errorCode);
	    return(errorCode);
	}

	if (vptr != 0) {
	    arg1 = (ReplicatedOp ? (void *)&Vnode_vv(*vptr) : (void *)&(*vptr)->disk.dataVersion);
	    arg2 = (ReplicatedOp ? (void *)&status->VV : (void *)&status->DataVersion);
	    if ((errorCode = VCmpProc(ReplicatedOp, (*vptr)->disk.type, arg1, arg2))) {
		SLog(0, "%s: (%x.%x.%x), VCP error (%d)",
			ProcName, Fid.Volume, Fid.Vnode, Fid.Unique, errorCode);
		return(errorCode);
	    }
	}
    }

    /* Integrity checks. */
    {
	if ((*dirvptr)->disk.type != vDirectory){
	    SLog(0, "%s: (%x.%x.%x) not a directory",
		    ProcName, Did.Volume, Did.Vnode, Did.Unique);
	    return(ENOTDIR);
	}

	if (STREQ(Name, ".") || STREQ(Name, "..")) {
	    SLog(0, "%s: illegal name (%s)",
		    ProcName, Name);
	    return(EINVAL);
	}

	PDirHandle dh;
	dh = VN_SetDirHandle(*dirvptr);
	ViceFid Fid;

	if (DH_Lookup(dh, Name, &Fid, CLU_CASE_SENSITIVE) == 0) {
		SLog(0, "%s: %s already exists in %s", 
		     ProcName, Name, FID_(&Did));
		VN_PutDirHandle(*dirvptr);
		return(EEXIST);
	}
	VN_PutDirHandle(*dirvptr);
	SLog(9, "%s: Lookup of %s in %s failed", ProcName, Name,  FID_(&Did));

	if (vptr != 0) {
	    switch(type) {
		case vFile:
		    if ((*vptr)->disk.type != vFile) {
			SLog(0, "%s: (%x.%x.%x) not a file", 
				ProcName, Fid.Volume, Fid.Vnode, Fid.Unique);
			return(EISDIR);
		    }
		    break;

		case vDirectory:
		    if ((*vptr)->disk.type != vDirectory) {
			SLog(0, "%s: %s not a dir", ProcName, FID_(&Fid));
			return(ENOTDIR);
		    }
		    break;

		case vSymlink:
		    if ((*vptr)->disk.type != vSymlink) {
			SLog(0, "%s: (%x.%x.%x) not a symlink", 
				ProcName, Fid.Volume, Fid.Vnode, Fid.Unique);
			return(EISDIR);
		    }
		    break;

		default:
		    CODA_ASSERT(FALSE);
	    }

	    if (((*vptr)->disk.vparent != Did.Vnode) || ((*vptr)->disk.uparent != Did.Unique)){
		SLog(0, "%s: cross-directory link (%x.%x.%x), (%x.%x.%x)",
			ProcName, Did.Volume, Did.Vnode, Did.Unique,
			Fid.Volume, Fid.Vnode, Fid.Unique);
		return(EXDEV);
	    }
	}
    }

    /* Protection checks. */
    if (MakeProtChecks) {
	/* Get the access list. */
	SLog(9, "%s: Going to get acl (%x.%x.%x)",
		ProcName, Did.Volume, Did.Vnode, Did.Unique);
	AL_AccessList *aCL = 0;
	int aCLSize = 0;
	SetAccessList(*dirvptr, aCL, aCLSize);

	/* Get this client's rights. */
	SLog(9, "%s: Going to get rights (%x.%x.%x)",
		ProcName, Did.Volume, Did.Vnode, Did.Unique);
	Rights t_rights; if (rights == 0) rights = &t_rights;
	Rights t_anyrights; if (anyrights == 0) anyrights = &t_anyrights;
	CODA_ASSERT(GetRights(client->CPS, aCL, aCLSize, rights, anyrights) == 0);

	/* INSERT permission required. */
	if (!(*rights & PRSFS_INSERT)) {
	    SLog(0, "%s: rights violation (%x : %x) (%x.%x.%x)",
		    ProcName, *rights, *anyrights, Did.Volume, Did.Vnode, Did.Unique);
	    return(EACCES);
	}
    }
    
    return(0);
}


static int Check_RR_Semantics(ClientEntry *client, Vnode **dirvptr, Vnode **vptr, char *Name,
			       Volume **volptr, VnodeType type, int ReplicatedOp,
			       VCP VCmpProc, ViceStatus *dirstatus,
			       ViceStatus *status, Rights *rights, Rights *anyrights,
			       int MakeProtChecks) {
    char *ProcName = (type == vDirectory)
      ? "CheckRmdirSemantics"
      : "CheckRemoveSemantics";
    int errorCode = 0;
    ViceFid Did;
    ViceFid Fid;

    VN_VN2Fid(*dirvptr, *volptr, &Did);
    VN_VN2Fid(*vptr, *volptr, &Fid);

    /* Concurrency-control checks. */
    if (VCmpProc) {
	void *arg1 = 0;
	void *arg2 = 0;

	arg1 = (ReplicatedOp ? (void *)&Vnode_vv(*dirvptr) : (void *)&(*dirvptr)->disk.dataVersion);
	arg2 = (ReplicatedOp ? (void *)&dirstatus->VV : (void *)&dirstatus->DataVersion);
	if ((errorCode = VCmpProc(ReplicatedOp, (*dirvptr)->disk.type, 
				 arg1, arg2))) {
		SLog(0, "%s: %s, VCP error (%d)", ProcName, 
		     FID_(&Did), errorCode);
		return(errorCode);
	}

	arg1 = (ReplicatedOp ? (void *)&Vnode_vv(*vptr) : (void *)&(*vptr)->disk.dataVersion);
	arg2 = (ReplicatedOp ? (void *)&status->VV : (void *)&status->DataVersion);
	if ((errorCode = VCmpProc(ReplicatedOp, (*vptr)->disk.type, arg1, arg2))) {
	    SLog(0, "%s: %s, VCP error (%d)", ProcName, FID_(&Fid), errorCode);
	    return(errorCode);
	}
    }

    /* Integrity checks. */
    {
	if ((*dirvptr)->disk.type != vDirectory) {
	    SLog(0, "%s: parent (%x.%x.%x) not a directory", 
		    ProcName, Did.Volume, Did.Vnode, Did.Unique);
	    return(ENOTDIR);
	}

	if (STREQ(Name, ".") || STREQ(Name, "..")) {
	    SLog(0, "%s: illegal name (%s)",
		    ProcName, Name);
	    return(EINVAL);
	}

	if ((*vptr)->disk.vparent != Did.Vnode || 
	    (*vptr)->disk.uparent != Did.Unique) {
		SLog(0, "%s: child %s not in parent %s", ProcName, 
		     FID_(&Fid), FID_2(&Did));
		return(EINVAL);
	}

	if ((type == vDirectory) && ((*vptr)->disk.type != vDirectory)){
		SLog(0, "%s: child %s not a directory", ProcName, FID_(&Fid));
		return(ENOTDIR);
	}
	if ((type != vDirectory) && ((*vptr)->disk.type == vDirectory)) {
		SLog(0, "%s: child %s is a directory", ProcName, FID_(&Fid));
		return(EISDIR);
	}

	if (type == vDirectory && VCmpProc != 0) {
		PDirHandle dh;
		dh = VN_SetDirHandle(*vptr);
		if (!DH_IsEmpty(dh)) {
			SLog(0, "%s: child %s not empty ", ProcName, 
			     FID_(&Fid));
			VN_PutDirHandle(*vptr);
			return(ENOTEMPTY);
		}
		VN_PutDirHandle(*vptr);
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
	CODA_ASSERT(GetRights(client->CPS, aCL, aCLSize, rights, anyrights) == 0);

	/* DELETE permission required. */
	if (!(*rights & PRSFS_DELETE)) {
	    SLog(0, "%s: rights violation (%x : %x) (%x.%x.%x)", 
		    ProcName, *rights, *anyrights, Did.Volume, Did.Vnode, Did.Unique);
	    return(EACCES);
	}
    }

    return(errorCode);
}


void PerformFetch(ClientEntry *client, Volume *volptr, Vnode *vptr) {
    /* Nothing to do here. */
}


int FetchBulkTransfer(RPC2_Handle RPCid, ClientEntry *client, 
		      Volume *volptr, Vnode *vptr, RPC2_Unsigned Offset,
		      RPC2_Unsigned Quota, ViceVersionVector *VV)
{
    int errorCode = 0;
    ViceFid Fid;
    Fid.Volume = V_id(volptr);
    Fid.Vnode = vptr->vnodeNumber;
    Fid.Unique = vptr->disk.uniquifier;
    RPC2_Integer Length = vptr->disk.length;
    PDirHandle dh;
    PDirHeader buf = 0;
    int size = 0;

    {
	/* When we are continueing a trickle/interrupted fetch, the version
	 * vector must be the same */
	if (Offset && VV && (VV_Cmp(VV, &vptr->disk.versionvector) != VV_EQ))
	{
		SLog(1, "FetchBulkTransfer: Attempting resumed fetch on updated object");
		/* now what errorcode can we use for this case?? */
		return(EAGAIN);
	}
    }

    /* If fetching a directory first copy contents from rvm to a temp buffer. */
    if (vptr->disk.type == vDirectory){
	    dh = VN_SetDirHandle(vptr);
	    CODA_ASSERT(dh);
	    
	    buf = DH_Data(dh);
	    size = DH_Length(dh);
	    SLog(9, "FetchBulkTransfer: wrote directory contents" 
		 "%s (size %d )into buf", 
		 FID_(&Fid), size);
    }

    /* Do the bulk transfer. */
    {
	struct timeval StartTime, StopTime;
	TM_GetTimeOfDay(&StartTime, 0);

	SE_Descriptor sid;
	memset(&sid, 0, sizeof(SE_Descriptor));
	sid.Tag = client->SEType;
	sid.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
	sid.Value.SmartFTPD.SeekOffset = Offset;
	sid.Value.SmartFTPD.hashmark = (SrvDebugLevel > 2 ? '#' : '\0');
	sid.Value.SmartFTPD.ByteQuota = (Quota == 0) ? -1 : (long)Quota;
	if (vptr->disk.type != vDirectory) {
	    if (vptr->disk.inodeNumber) {
		sid.Value.SmartFTPD.Tag = FILEBYINODE;
		sid.Value.SmartFTPD.FileInfo.ByInode.Device = V_device(volptr);
		sid.Value.SmartFTPD.FileInfo.ByInode.Inode = vptr->disk.inodeNumber;
	    } else {
		sid.Value.SmartFTPD.Tag = FILEBYNAME;
		sid.Value.SmartFTPD.FileInfo.ByName.ProtectionBits = 0;
		strcpy(sid.Value.SmartFTPD.FileInfo.ByName.LocalFileName, "/dev/null");
	    }
	} else {
	    /* if it is a directory get the contents from the temp file */
	    sid.Value.SmartFTPD.Tag = FILEINVM;
	    sid.Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqLen = size;
	    sid.Value.SmartFTPD.FileInfo.ByAddr.vmfile.MaxSeqLen = size;
	    sid.Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqBody =
	      (RPC2_ByteSeq)buf;
	}
	
	/* debugging purposes */
	SLog(9, "FetchBulkTransfer: Printing se descriptor:");
	switch (sid.Value.SmartFTPD.Tag) {
	    case FILEBYINODE:
		SLog(9, "Tag = FILEBYINODE, device = %u, inode = %u",
			sid.Value.SmartFTPD.FileInfo.ByInode.Device,
			sid.Value.SmartFTPD.FileInfo.ByInode.Inode);
		break;
	    case FILEBYNAME:
		SLog(9, "Tag = FILEBYNAME, LocalFileName = %s",
			sid.Value.SmartFTPD.FileInfo.ByName.LocalFileName);
		break;
	    case FILEINVM:
		SLog(9, "Tag = FILEINVM, len = %d buf = %x",
			sid.Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqLen,
			sid.Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqBody);
		break;
	    default:
		SLog(9, "BOGUS TAG");
		CODA_ASSERT(0);
		break;
	}
	if((errorCode = (int) RPC2_InitSideEffect(RPCid, &sid)) <= RPC2_ELIMIT) {
	    SLog(0, "FetchBulkTransfer: InitSE failed (%d), %s",
		    errorCode, FID_(&Fid));
	    goto Exit;
	}

	if ((errorCode = (int) RPC2_CheckSideEffect(RPCid, &sid, SE_AWAITLOCALSTATUS)) <= RPC2_ELIMIT) {
	    SLog(0, "FetchBulkTransfer: CheckSE failed (%d), (%x.%x.%x)",
		    errorCode, Fid.Volume, Fid.Vnode, Fid.Unique);
	    if (errorCode == RPC2_SEFAIL1) errorCode = EIO;
	    goto Exit;
	}

	/* compensate Length for the data we skipped because of the requested
	 * Offset */
	Length -= Offset;

	RPC2_Integer len = sid.Value.SmartFTPD.BytesTransferred;
	if (len != Length) {
	    SLog(0, "FetchBulkTransfer: length discrepancy (%d : %d), (%x.%x.%x), %s %s.%d",
		 Length, len, Fid.Volume, Fid.Vnode, Fid.Unique,
		 client->UserName, client->VenusId->HostName,
		 client->VenusId->port);
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

	SLog(2, "FetchBulkTransfer: transferred %d bytes (%x.%x.%x)",
		Length, Fid.Volume, Fid.Vnode, Fid.Unique);
    }

Exit:
    if (vptr->disk.type == vDirectory)
	    VN_PutDirHandle(vptr);
    return(errorCode);
}

int FetchFileByName(RPC2_Handle RPCid, char *name, ClientEntry *client) {
    int errorCode = 0;
    SE_Descriptor sid;
    memset(&sid, 0, sizeof(SE_Descriptor));
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
	SLog(0, "FetchFileByName: InitSideEffect failed %d", 
		errorCode);
	return(errorCode);
    }

    if ((errorCode = (int) RPC2_CheckSideEffect(RPCid, &sid, SE_AWAITLOCALSTATUS)) 
	<= RPC2_ELIMIT) {
	if (errorCode == RPC2_SEFAIL1) errorCode = EIO;
	SLog(0, "FetchFileByName: CheckSideEffect failed %d",
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
	ViceFid fids[MAXFIDS]; bzero((void *)fids, (int)(MAXFIDS * sizeof(ViceFid)));
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
	memset(&sid, 0, sizeof(SE_Descriptor));
	sid.Tag = client->SEType;
	sid.Value.SmartFTPD.TransmissionDirection = CLIENTTOSERVER;
	sid.Value.SmartFTPD.SeekOffset = 0;
	sid.Value.SmartFTPD.hashmark = (SrvDebugLevel > 2 ? '#' : '\0');
	sid.Value.SmartFTPD.Tag = FILEBYINODE;
	sid.Value.SmartFTPD.ByteQuota = Length;
	sid.Value.SmartFTPD.FileInfo.ByInode.Device = V_device(volptr);
	sid.Value.SmartFTPD.FileInfo.ByInode.Inode = newinode;

	if((errorCode = (int) RPC2_InitSideEffect(RPCid, &sid)) <= RPC2_ELIMIT) {
	    SLog(0, "StoreBulkTransfer: InitSE failed (%d), (%x.%x.%x)",
		    errorCode, Fid.Volume, Fid.Vnode, Fid.Unique);
	    goto Exit;
	}

	if ((errorCode = (int) RPC2_CheckSideEffect(RPCid, &sid, SE_AWAITLOCALSTATUS)) <= RPC2_ELIMIT) {
	    SLog(0, "StoreBulkTransfer: CheckSE failed (%d), (%x.%x.%x)",
		    errorCode, Fid.Volume, Fid.Vnode, Fid.Unique);
	    if (errorCode == RPC2_SEFAIL1) errorCode = EIO;
	    goto Exit;
	}

	RPC2_Integer len = sid.Value.SmartFTPD.BytesTransferred;
	if (Length != -1 && Length != len) {
	    SLog(0, "StoreBulkTransfer: length discrepancy (%d : %d), (%x.%x.%x), %s %s.%d",
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

	SLog(2, "StoreBulkTransfer: transferred %d bytes (%x.%x.%x)",
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


    /* Truncate invokes COW - files only here */
    if (Mask & SET_LENGTH)
	if (vptr->disk.cloned) {
	    *CowInode = vptr->disk.inodeNumber;
	    CopyOnWrite(vptr, volptr);
	}

    if (ReplicatedOp) {
	NewCOP1Update(volptr, vptr, StoreId, vsptr);

	/* Await COP2 message. */
	ViceFid fids[MAXFIDS]; bzero((void *)fids, MAXFIDS * (int) sizeof(ViceFid));
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
	ViceFid fids[MAXFIDS]; bzero((void *)fids, (int) (MAXFIDS * sizeof(ViceFid)));
	fids[0] = Fid;
	CopPendingMan->add(new cpent(StoreId, fids));
    }
}


void PerformCreate(ClientEntry *client, VolumeId VSGVolnum, Volume *volptr,
		    Vnode *dirvptr, Vnode *vptr, char *Name, Date_t Mtime, RPC2_Unsigned Mode,
		    int ReplicatedOp, ViceStoreId *StoreId, 
		   DirInode **CowInode, int *blocks, RPC2_Integer *vsptr) {
    Perform_CLMS(client, VSGVolnum, volptr, dirvptr, vptr, OLDCML_Create_OP,
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
    Perform_CLMS(client, VSGVolnum, volptr, dirvptr, vptr, OLDCML_Link_OP,
		  Name, 0, 0, Mtime, 0, ReplicatedOp, StoreId, CowInode, 
		 blocks, vsptr);
}


void PerformRename(ClientEntry *client, VolumeId VSGVolnum, Volume *volptr,
		    Vnode *sd_vptr, Vnode *td_vptr, Vnode *s_vptr, Vnode *t_vptr,
		    char *OldName, char *NewName, Date_t Mtime,
		    int ReplicatedOp, ViceStoreId *StoreId, DirInode **sd_CowInode,
		    DirInode **td_CowInode, DirInode **s_CowInode, int *nblocks,
		   RPC2_Integer *vsptr) 
{
    ViceFid SDid;			/* Source directory */
    SDid.Volume = V_id(volptr);
    SDid.Vnode = sd_vptr->vnodeNumber;
    SDid.Unique = sd_vptr->disk.uniquifier;

    ViceFid TDid;			/* Target directory */
    TDid.Volume = V_id(volptr);
    TDid.Vnode = td_vptr->vnodeNumber;
    TDid.Unique = td_vptr->disk.uniquifier;
    int SameParent = (FID_EQ(&SDid, &TDid));

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
    PDirHandle sd_dh;
    if (sd_vptr->disk.cloned) {
	    *sd_CowInode = (DirInode *)sd_vptr->disk.inodeNumber;
	    CopyOnWrite(sd_vptr, volptr);
    }
    sd_dh = VN_SetDirHandle(sd_vptr);

    PDirHandle td_dh;
    if (!SameParent ) {
	    if (td_vptr->disk.cloned) {
		    *td_CowInode = (DirInode *)td_vptr->disk.inodeNumber;
		    CopyOnWrite(td_vptr, volptr);
	    } 
	    td_dh = VN_SetDirHandle(td_vptr);
    } else {
	    td_dh = sd_dh;
    }

    PDirHandle s_dh;
    if (s_vptr->disk.type == vDirectory) {
	    if ( s_vptr->disk.cloned) {
		    *s_CowInode = (DirInode *)s_vptr->disk.inodeNumber;
		    CopyOnWrite(s_vptr, volptr);
	    }
	    s_dh = VN_SetDirHandle(s_vptr);
    }

    /* Remove the source name from its parent. */
    CODA_ASSERT(DH_Delete(sd_dh, OldName) == 0);
    int sd_newlength = DH_Length(sd_dh);
    int sd_newblocks = (int) (nBlocks(sd_newlength) - nBlocks(sd_vptr->disk.length));
    if(sd_newblocks != 0) {
	    ChangeDiskUsage(volptr, sd_newblocks);
	    if (nblocks) *nblocks += sd_newblocks;
    }
    sd_vptr->disk.length = sd_newlength;

    /*XXXXX this seems against unix semantics: the target should only 
     be removed if it is not a directory. Probably the client's kernel
    will protect us, but it is worrying*/
    /* Remove the target name from its parent (if it exists). */
    if (TargetExists) {
	/* Remove the name. */
	/* Flush directory pages for deleted child. */
	if (t_vptr->disk.type == vDirectory) {
		SLog(0, "WARNING: rename is overwriting a directory!");
	}
	CODA_ASSERT(DH_Delete(td_dh, NewName) == 0);

	int td_newlength = DH_Length(td_dh);
	int td_newblocks = (int) (nBlocks(td_newlength) - 
				  nBlocks(td_vptr->disk.length));
	if(td_newblocks != 0) {
	    ChangeDiskUsage(volptr, td_newblocks);
	    if (nblocks) *nblocks += td_newblocks;
	}
	td_vptr->disk.length = td_newlength;

	/* Flush directory pages for deleted child. */
	if (t_vptr->disk.type == vDirectory) {
	    VN_PutDirHandle(t_vptr);
	    DC_Drop(t_vptr->dh);
	}
    }

    /* Create the target name in its parent. */
    CODA_ASSERT(DH_Create(td_dh, NewName, &SFid) == 0);

    int td_newlength = DH_Length(td_dh);
    int td_newblocks = (int) (nBlocks(td_newlength) - nBlocks(td_vptr->disk.length));
    if(td_newblocks != 0) {
	ChangeDiskUsage(volptr, td_newblocks);
	if (nblocks) *nblocks += td_newblocks;
    }
    td_vptr->disk.length = td_newlength;

    /* Alter ".." entry in source if necessary. */
    if (!SameParent && s_vptr->disk.type == vDirectory) {
	CODA_ASSERT(DH_Delete(s_dh, "..") == 0);
	sd_vptr->disk.linkCount--;
	CODA_ASSERT(DH_Create(s_dh, "..", &TDid) == 0);
	td_vptr->disk.linkCount++;

	int s_newlength = DH_Length(s_dh);
	int s_newblocks = (int) (nBlocks(s_newlength) - nBlocks(s_vptr->disk.length));
	if(s_newblocks != 0) {
	    ChangeDiskUsage(volptr, s_newblocks);
	    if (nblocks) *nblocks += s_newblocks;
	}
	s_vptr->disk.length = s_newlength;
    }

    if (s_vptr->disk.type == vDirectory) 
	    VN_PutDirHandle(s_vptr);

    VN_PutDirHandle(sd_vptr);
    if ( !SameParent ) 
	    VN_PutDirHandle(td_vptr);

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
	ViceFid fids[MAXFIDS]; bzero((void *)fids, (int) (MAXFIDS * sizeof(ViceFid)));
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
    Perform_CLMS(client, VSGVolnum, volptr, dirvptr, vptr, OLDCML_MakeDir_OP,
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
    Perform_CLMS(client, VSGVolnum, volptr, dirvptr, vptr, OLDCML_SymLink_OP,
		  Name, newinode, Length, Mtime, Mode, ReplicatedOp, StoreId, CowInode,
		 blocks, vsptr);
}


/*
  Perform_CLMS: Perform the create, link, mkdir or  
  symlink operation on a VM copy of the object.
*/

static void Perform_CLMS(ClientEntry *client, VolumeId VSGVolnum,
			   Volume *volptr, Vnode *dirvptr, Vnode *vptr,
			   int opcode, char *Name, Inode newinode,
			   RPC2_Unsigned Length, Date_t Mtime, RPC2_Unsigned Mode,
			   int ReplicatedOp, ViceStoreId *StoreId, DirInode **CowInode,
			  int *nblocks, RPC2_Integer *vsptr) 
{
    int error;
    *nblocks = 0;
    ViceFid Did;
    ViceFid Fid;
    VN_VN2Fid(vptr, volptr, &Fid);
    VN_VN2Fid(dirvptr, volptr, &Did);

    /* Break callback promises. */
    CodaBreakCallBack((client ? client->VenusId : 0), &Did, VSGVolnum);
    if (opcode == OLDCML_Link_OP)
	    CodaBreakCallBack((client ? client->VenusId : 0), &Fid, VSGVolnum);

    /* CLMS invokes COW! */
    PDirHandle dh;
    if (dirvptr->disk.cloned) {
	    *CowInode = (DirInode *)dirvptr->disk.inodeNumber;
	    CopyOnWrite(dirvptr, volptr);
    }
    dh = VN_SetDirHandle(dirvptr);

    /* Add the name to the parent. */
    error = DH_Create(dh, Name, &Fid);
    if ( error ) {
	    eprint("Create returns %d on %s %s", error, Name, FID_(&Fid));
	    VN_PutDirHandle(dirvptr);
	    CODA_ASSERT(0);
    }
    int newlength = DH_Length(dh);
    VN_PutDirHandle(dirvptr);
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
	case OLDCML_Create_OP:
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

	case OLDCML_Link_OP:
	    vptr->disk.linkCount++;
	    vptr->disk.author = client ? client->Id : 0;
	    break;

	case OLDCML_MakeDir_OP:
	    {
		    PDirHandle cdh;
		    vptr->disk.inodeNumber = 0;
		    vptr->dh = 0;
		    CODA_ASSERT(vptr->changed);

		    /* Create the child directory. */
		    cdh = VN_SetDirHandle(vptr);
		    CODA_ASSERT(cdh);
		    CODA_ASSERT(DH_MakeDir(cdh, &Fid, &Did) == 0);
		    CODA_ASSERT(DC_Dirty(vptr->dh));

		    vptr->disk.linkCount = 2;
		    vptr->disk.length = DH_Length(cdh);
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

	case OLDCML_SymLink_OP:
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
	    CODA_ASSERT(FALSE);
    }
    if (ReplicatedOp) 
	NewCOP1Update(volptr, vptr, StoreId, vsptr);

    /* Await COP2 message. */
    if (ReplicatedOp) {
	ViceFid fids[MAXFIDS]; bzero((void *)fids, (int) (MAXFIDS * sizeof(ViceFid)));
	fids[0] = Did;
	fids[1] = Fid;
	CopPendingMan->add(new cpent(StoreId, fids));
    }
}


static void Perform_RR(ClientEntry *client, VolumeId VSGVolnum, Volume *volptr,
			 Vnode *dirvptr, Vnode *vptr, char *Name, Date_t Mtime,
			 int ReplicatedOp, ViceStoreId *StoreId, DirInode **CowInode,
			 int *blocks, RPC2_Integer *vsptr) 
{
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
    PDirHandle pDir;
    if (dirvptr->disk.cloned) {
	    *CowInode = (DirInode *)dirvptr->disk.inodeNumber;
	    CopyOnWrite(dirvptr, volptr);
    }
    pDir = VN_SetDirHandle(dirvptr);

    /* Remove the name from the directory. */
    CODA_ASSERT(DH_Delete(pDir, Name) == 0);
    int newlength = DH_Length(pDir);
    CODA_ASSERT(DC_Dirty(dirvptr->dh));
    VN_PutDirHandle(dirvptr);
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

    /* PARANOIA: Flush directory pages for deleted child. */
    if (vptr->disk.type == vDirectory) {
	PDirHandle cDir;
	cDir = VN_SetDirHandle(vptr);
	/* put objects will detect this and DI_Dec the inode */
	DC_SetDirty(vptr->dh, 1);
	DH_FreeData(cDir);
	VN_PutDirHandle(vptr);
    }

    /* Update the child vnode. */
    SLog(3, "Perform_RR: LC = %d, TYPE = %d, delete_me = %d",
	 vptr->disk.linkCount, vptr->disk.type, vptr->delete_me);
    if (--vptr->disk.linkCount == 0 || vptr->disk.type == vDirectory) {
	    vptr->delete_me = 1;
	    DeleteFile(&Fid);
    } else  if (ReplicatedOp) 
	    NewCOP1Update(volptr, vptr, StoreId, vsptr);

    /* Await COP2 message. */
    if (ReplicatedOp) {
	    ViceFid fids[MAXFIDS]; 
	    bzero((void *)fids,  (MAXFIDS * sizeof(ViceFid)));
	    fids[0] = Did;
	    SLog(3, "Perform_RR: delete_me = %d, !delete_me = %d",
		 vptr->delete_me, !vptr->delete_me);
	if (!vptr->delete_me) 
		fids[1] = Fid;
	CopPendingMan->add(new cpent(StoreId, fids));
    }
}

#ifdef _TIMEPUTOBJS_
#undef RVMLIB_END_TOP_LEVEL_TRANSACTION_2 
#undef rvmlib_end_transaction 

#define	rvmlib_end_transaction(flush_mode, statusp)\
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
       CODA_ASSERT(0);\
    }\
END_NSC_TIMING(PutObjects_TransactionEnd);\
}

#define rvmlib_end_transaction(flush, &(status)); \
    rvmlib_end_transaction(flush, (&(status)))


#endif _TIMEPUTOBJS_

/*
  PutObjects: Update and release vnodes, volume, directory data
  ACL's etc.
  Flush changes in case of error.

  Added UpdateVolume flag for quota resolution.  If true, PutObjects will
  write out the volume header.
*/
void PutObjects(int errorCode, Volume *volptr, int LockLevel, 
	    dlist *vlist, int blocks, int TranFlag, int UpdateVolume) 
{
        SLog(10, "PutObjects: Vid = %x, errorCode = %d",
		volptr ? V_id(volptr) : 0, errorCode);

        vmindex freed_indices;		// for truncating /purging resolution logs 

        /* Back out new disk allocation on failure. */
        if (errorCode && volptr)
	        if (blocks != 0 && AdjustDiskUsage(volptr, -blocks) != 0)
	                SLog(0, "PutObjects: AdjustDiskUsage(%x, %d) failed", 
                               V_id(volptr), -blocks);

       /* Record these handles since we will need them after the
          objects are put! */
        Device device = (volptr ? V_device(volptr) : 0);
        VolumeId parentId = (volptr ? V_parentId(volptr) : 0);

START_TIMING(PutObjects_Transaction);
#ifdef _TIMEPUTOBJS_
    START_NSC_TIMING(PutObjects_Transaction);
#endif _TIMEPUTOBJS_

    /* Separate branches for Mutating and Non-Mutating cases are to
       avoid transaction in the latter. */
    if (TranFlag) {
	rvm_return_t status = RVM_SUCCESS;
	rvmlib_begin_transaction(restore);

	/* Put the directory pages, the vnodes, and the volume. */
	/* Don't mutate inodes until AFTER transaction commits. */
	if (vlist) {
	    dlist_iterator next(*vlist);
	    vle *v;
	    int count = 0;
	    while ((v = (vle *)next()))
		if (v->vptr) {
		    /* for resolution/reintegration yield every n vnodes */
		    if (LockLevel == NO_LOCK) {
			count++;
			if ((count & Yield_PutObjects_Mask) == 0)
			    PollAndYield();
		    }
		    /* Directory pages.  Be careful with cloned directories! */
                    SLog(10, "--PO: %s", FID_(&v->fid));
                    if (v->vptr->disk.type == vDirectory ) {
			/* sanity */
			if (errorCode)
			{
			   CODA_ASSERT(VN_DAbort(v->vptr) == 0);
			   if ( v->d_inodemod && v->vptr && v->vptr->dh) 
			       VN_PutDirHandle(v->vptr);
			}
			else if (v->d_inodemod)
			{
			    CODA_ASSERT(v->vptr->dh);
			    SLog(10, "--PO: %s dirty %d", 
				 FID_(&v->fid), DC_Dirty(v->vptr->dh));

			    if ( DC_Dirty(v->vptr->dh)) {
				/* Dec the Cow */
				PDCEntry pdce = v->vptr->dh;
				if (DC_Cowpdi(pdce)) {
				    DI_Dec(DC_Cowpdi(pdce));
				    DC_SetCowpdi(pdce, NULL);
				}
				CODA_ASSERT(VN_DCommit(v->vptr) == 0);

				DC_SetDirty(v->vptr->dh, 0);

				SLog(0, "--DC: %s ct: %d\n", 
				     FID_(&v->fid), DC_Count(v->vptr->dh));
			    } else {
				SLog(0, "--PO: d_inodemod and !DC_Dirty %s",
				     FID_(&v->fid));
			    }
			    VN_PutDirHandle(v->vptr);
			}
		    }
		    
		    if (AllowResolution && volptr && 
			V_RVMResOn(volptr) && (v->vptr->disk.type == vDirectory)) {
			// make sure log header exists 
			if (!errorCode && !VnLog(v->vptr))
			    CreateResLog(volptr, v->vptr);
			
			// log mutation into recoverable volume log
			olist_iterator next(v->rsl);
			rsle *vmle;
			while((vmle = (rsle *)next())) {
			    if (!errorCode) {
				SLog(9, 
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
				CODA_ASSERT(v->vptr->delete_me);
				if (VnLog(v->vptr)) {
				    PurgeLog(VnLog(v->vptr), volptr, &freed_indices);
				    VnLog(v->vptr) = NULL;
				}
			    }
			    else if (v->d_needslogtrunc) {
				CODA_ASSERT(!v->vptr->delete_me);
				TruncateLog(volptr, v->vptr, &freed_indices);
			    }
			}

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

			CODA_ASSERT(fileCode == 0);
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
	rvmlib_end_transaction(flush, &(status));
	CODA_ASSERT(status == 0);
    } else { 
/*  NO transaction */
	if (vlist) {
	    dlist_iterator next(*vlist);
	    vle *v;
	    while ((v = (vle *)next()))
		if (v->vptr) {
		    /* Directory pages.  Cloning cannot occur without mutation! */
		    if (v->vptr->disk.type == vDirectory) {
			CODA_ASSERT(v->d_cinode == 0);
		    }

		    /* Vnode. */
		    {
			/* Put rather than Flush even on failure since vnode wasn't mutated! */
			Error fileCode = 0;
			VPutVnode(&fileCode, v->vptr);
			CODA_ASSERT(fileCode == 0);
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
	while ((v = (vle *)vlist->get())) {
	    if (!ISDIR(v->fid)) {
		count++;
		if ((count && Yield_PutInodes_Mask) == 0)
		    PollAndYield();
		if (errorCode == 0) {
		    if (v->f_sinode)
			CODA_ASSERT(idec((int) device, (int) v->f_sinode, parentId) == 0);
		    if (v->f_tinode) {
			SLog(3, "PutObjects: truncating (%x.%x.%x, %d, %d)",
				v->fid.Volume, v->fid.Vnode, v->fid.Unique,
				v->f_tinode, v->f_tlength);

			int fd;
			if ((fd = iopen((int) device, (int) v->f_tinode, O_RDWR)) < 0) {
			    SLog(0, "PutObjects: iopen(%d, %d) failed (%d)",
				    device, v->f_tinode, errno);
			    CODA_ASSERT(0);
			}
			CODA_ASSERT(ftruncate(fd, v->f_tlength) == 0);
			CODA_ASSERT(close(fd) == 0);
		    }
		}
		else {
		    if (v->f_finode)
			CODA_ASSERT(idec((int) device, (int) v->f_finode, parentId) == 0);
		}
	    }
	    if (AllowResolution) {
		/* clean up spooled log record list */
		rsle *rs;
		while ((rs = (rsle *)v->rsl.get())) 
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

    SLog(10, "PutObjects: returning %s", ViceErrorMsg(0));
}

