/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
              Copyright (c) 2002-2003 Intel Corporation
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
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include "coda_string.h"
#include <inodeops.h>

#include <unistd.h>
#include <stdlib.h>
#include <util.h>
#include <rvmlib.h>
#include <partition.h>
#include <codadir.h>

#ifdef _TIMECALLS_
#include "histo.h"
#endif

#include <partition.h>
#include <util.h>
#include <prs.h>
#include <al.h>
#include <callback.h>
#include <vice.h>
#include <cml.h>
#include <lka.h>
#include <copyfile.h>

#ifdef __cplusplus
}
#endif

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
#endif /* _TIMECALLS_ */

#ifndef O_BINARY
#define O_BINARY 0
#endif /* O_BINARY */

/* From Vol package. */
extern void VCheckDiskUsage(Error *, Volume *, int );

extern void MakeLogNonEmpty(Vnode *);
extern void GetMaxVV(ViceVersionVector *, ViceVersionVector **, int);

extern int CheckReadMode(ClientEntry *, Vnode *);
extern int CheckWriteMode(ClientEntry *, Vnode *);
static void CopyOnWrite(Vnode *, Volume *);
extern int AdjustDiskUsage(Volume *, int);
extern int CheckDiskUsage(Volume *, int);
extern void HandleWeakEquality(Volume *, Vnode *, ViceVersionVector *);

/* *****  Private routines  ***** */

static int GrabFsObj(ViceFid *, Volume **, Vnode **, int, int, int);
static int NormalVCmp(int, VnodeType, void *, void *);
static int StoreVCmp(int, VnodeType, void *, void *);

typedef enum { CLMS_Create, CLMS_Link, CLMS_MakeDir, CLMS_SymLink } CLMS_Op;
static int Check_CLMS_Semantics(ClientEntry *, Vnode **, Vnode **, char *, Volume **,
                                CLMS_Op opcode, int, VCP, void *dirVersion, void *Version,
                                Rights *, Rights *, int);
static int Check_RR_Semantics(ClientEntry *, Vnode **, Vnode **, char *, Volume **, VnodeType,
				int, VCP, void *, void *, Rights *, Rights *, int);
static int Perform_CLMS(ClientEntry *, VolumeId, Volume *, Vnode *, Vnode *, CLMS_Op opcode,
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
  ViceFetch: Fetch a file or directory
*/
long FS_ViceFetch(RPC2_Handle RPCid, ViceFid *Fid, ViceVersionVector *VV,
		  RPC2_Unsigned InconOK, ViceStatus *Status,
		  RPC2_Unsigned PrimaryHost, RPC2_Unsigned Offset,
		  RPC2_CountedBS *PiggyBS, SE_Descriptor *BD)
{
    int errorCode = 0;		/* return code to caller */
    Volume *volptr = 0;		/* pointer to the volume */
    ClientEntry *client = 0;	/* pointer to the client data */
    Rights rights = 0;		/* rights for this user */
    Rights anyrights = 0;	/* rights for any user */
    VolumeId VSGVolnum = Fid->Volume;
    int ReplicatedOp;
    dlist *vlist = new dlist((CFN)VLECmp);
    vle *v;
    vle *av;

START_TIMING(Fetch_Total);
    SLog(1, "ViceFetch: Fid = %s, Repair = %d", FID_(Fid), InconOK);

  
    /* Validate parameters. */
    {
	if ((errorCode = ValidateParms(RPCid, &client, &ReplicatedOp,
				      &Fid->Volume, PiggyBS, NULL)))
	    goto FreeLocks;
    }

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
	    // We only use the parent node for ACL checks, allow inconsistency
	    if ((errorCode = GetFsObj(&pFid, &volptr, &av->vptr, READ_LOCK, 
				     NO_LOCK, 1, 0, 0)))
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
					      Offset, VV)))
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

    SLog(2, "ViceFetch returns %s", ViceErrorMsg(errorCode));
END_TIMING(Fetch_Total);
    return(errorCode);
}

/*
 OBSOLETE:  Delete code once old clients are gone (Satya, 12/02)

 ViceGetAttr: Fetch the attributes for a file/directory
*/
long FS_ViceGetAttr(RPC2_Handle RPCid, ViceFid *Fid, RPC2_Unsigned InconOK,
		    ViceStatus *Status, RPC2_Unsigned PrimaryHost,
		    RPC2_CountedBS *PiggyBS)
{
  long rc;

  rc = FS_ViceGetAttrPlusSHA(RPCid, Fid, InconOK, Status, NULL,
			     PrimaryHost, PiggyBS);
  return(rc);
}


/* ViceGetAttrPlusSHA() is a replacement for ViceGetAttr().  It does
   everything that ViceGetAttr() does and, in addition, returns the SHA
   value of the object.   Original is retained temporarily for upward 
   compatibility with  old clients.  Delete ViceGetAttr() as soon
   as possible. (Satya, 12/02)
*/

long FS_ViceGetAttrPlusSHA(RPC2_Handle RPCid, ViceFid *Fid, RPC2_Unsigned InconOK,
		    ViceStatus *Status, RPC2_BoundedBS *MySHA,
		    RPC2_Unsigned PrimaryHost,RPC2_CountedBS *PiggyBS)
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

    SLog(1, "ViceGetAttrPlusSHA: Fid = %s, Repair = %d", FID_(Fid), InconOK);

    /* Validate parameters. */
    {
	if ((errorCode = ValidateParms(RPCid, &client, &ReplicatedOp, 
				      &Fid->Volume, PiggyBS, NULL)))
	    goto FreeLocks;
    }

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
	    // We only use the parent node for ACL checks, allow inconsistency
	    if ((errorCode = GetFsObj(&pFid, &volptr, &av->vptr, 
				     READ_LOCK, NO_LOCK, 1, 0, 0)))
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

    /* Obtain SHA if requested.  For now, we simplify the code by
       (a) re-computing SHA each time, and (b) only computing SHA for
       files.  Assumption (a) is wasteful of server CPU cycles, but 
       avoids changes to RVM data layout on servers.  A better approach
       would be to compute the SHA of a file lazily (i.e., on first 
       ViceGetAttrPlusSHA() for it) and then saving it in persistent state.
       A server could also check its CPU load and decline to compute
       the SHA if too heavily loaded.  Assumption (b) is less clear.
       Directories aren't usually that big, so there may not be much
       savings from avoiding their transmission.  Code below
       will get more complex since directory state will have to be
       copied from RVM (as in ViceFetch()).  But some directories can
       get large, so using SHA might be reasonable.  (Satya, 12/02)
    */
    if (MySHA)
	MySHA->SeqLen = 0;

    /* Only files use SHA checksums */
    if (!AllowSHA || !MySHA || v->vptr->disk.type != vFile)
	goto SkipSHA;

    if (IsZeroSHA(VnSHA(v->vptr)))
    {
	int fd = iopen(V_device(volptr), v->vptr->disk.node.inodeNumber, O_RDONLY);
	SLog(0, "GetAttrPlusSHA: Computing SHA %s, disk.inode=%x", 
	      FID_(Fid), v->vptr->disk.node.inodeNumber);
	if (fd == -1) goto FreeLocks;

	ComputeViceSHA(fd, VnSHA(v->vptr));
	close(fd);
    }

    if (MySHA->MaxSeqLen >= SHA_DIGEST_LENGTH) {
	MySHA->SeqLen = SHA_DIGEST_LENGTH;
	memcpy(MySHA->SeqBody, VnSHA(v->vptr), SHA_DIGEST_LENGTH);

	if (SrvDebugLevel > 1) {
	    char printbuf[2*SHA_DIGEST_LENGTH+1]; 
	    ViceSHAtoHex((unsigned char *)MySHA->SeqBody, printbuf, sizeof(printbuf));
	    fprintf(stdout, "ViceGetAttrPlusSHA: SHA = %s\n", printbuf);
	}
    }

SkipSHA:;
FreeLocks:
    /* Put objects. */
    {
	PutObjects(errorCode, volptr, NO_LOCK, vlist, 0, 0);
    }

    SLog(2, "ViceGetAttrPlusSHA returns %s", ViceErrorMsg(errorCode));
END_TIMING(GetAttr_Total);
    return(errorCode);
}


/* Obsolete version of ViceValidateAttrsPlusSHA() */

long FS_ViceValidateAttrs(RPC2_Handle RPCid, RPC2_Unsigned PrimaryHost,
		       ViceFid *PrimaryFid, ViceStatus *Status, 
		       RPC2_Integer NumPiggyFids, ViceFidAndVV Piggies[],
		       RPC2_BoundedBS *VFlagBS, RPC2_CountedBS *PiggyBS)
{
  long rc;

  rc = FS_ViceValidateAttrsPlusSHA(RPCid, PrimaryHost, PrimaryFid, Status, NULL,
				   NumPiggyFids, Piggies,VFlagBS, PiggyBS);
  return(rc);

}

/* 
 * assumes fids are given in order. a return of 1 in flags means that the 
 * client status is valid for that object, and that callback is set. 
 */

/*
  ViceValidateAttrsPlusSHA: A batched version of GetAttrPlusSHA
*/
long FS_ViceValidateAttrsPlusSHA(RPC2_Handle RPCid, RPC2_Unsigned PrimaryHost,
			  ViceFid *PrimaryFid, ViceStatus *Status, 
        		  RPC2_BoundedBS *MySHA,
			  RPC2_Integer NumPiggyFids, ViceFidAndVV Piggies[],
			  RPC2_BoundedBS *VFlagBS, RPC2_CountedBS *PiggyBS)
{
    long errorCode = 0;		/* return code to caller */
    VolumeId VSGVolnum = PrimaryFid->Volume;
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
    char why_failed[25] = "";

START_TIMING(ViceValidateAttrs_Total);
    SLog(1, "ViceValidateAttrs: Fid = %s, %d piggy fids", 
	 FID_(PrimaryFid), NumPiggyFids);

    VFlagBS->SeqLen = 0;
    memset(VFlagBS->SeqBody, 0, VFlagBS->MaxSeqLen);

    /* Do a real getattr for primary fid. */
    {
	if ((errorCode = FS_ViceGetAttrPlusSHA(RPCid, PrimaryFid, 0, Status,
					       MySHA, PrimaryHost, PiggyBS)))
		goto Exit;
    }
 	
    if (VFlagBS->MaxSeqLen < (RPC2_Unsigned)NumPiggyFids) {
	    SLog(0, "Client sending wrong output buffer while validating"
		 ": %s; MaxSeqLen %d, should be %d", 
		 FID_(PrimaryFid), VFlagBS->MaxSeqLen, NumPiggyFids);
	    errorCode = EINVAL;
	    goto Exit;
    }

    /* now check piggyback fids */
    for (i = 0; i < NumPiggyFids; i++) {

	if (Piggies[i].Fid.Volume != VSGVolnum) {
	    strcpy(why_failed, "Wrong Volume Id");
	    goto InvalidObj;
	}

	/* Validate parameters. */
        {
	    /* We've already dealt with the PiggyBS in the GetAttr above. */
	    if ((iErrorCode = ValidateParms(RPCid, &client, &ReplicatedOp, 
					   &Piggies[i].Fid.Volume, NULL, NULL))) {
		strcpy(why_failed, "ValidateParms");
		goto InvalidObj;
	    }
        }

	/* Get objects. */
	{
	    v = AddVLE(*vlist, &Piggies[i].Fid);
	    if ((iErrorCode = GetFsObj(&Piggies[i].Fid, &volptr, 
				      &v->vptr, READ_LOCK, NO_LOCK, 0, 0, 0))) {
		strcpy(why_failed, "GetFsObj 1");
		goto InvalidObj;
	    }

	    /* This may violate locking protocol! -JJK */
	    if (v->vptr->disk.type == vDirectory) {
		av = v;
	    } else {
		ViceFid pFid;
		pFid.Volume = Piggies[i].Fid.Volume;
		pFid.Vnode = v->vptr->disk.vparent;
		pFid.Unique = v->vptr->disk.uparent;
		av = AddVLE(*vlist, &pFid);
		// We only use the parent node for ACL checks, allow inconsistency
		if ((iErrorCode = GetFsObj(&pFid, &volptr, &av->vptr, 
					  READ_LOCK, NO_LOCK, 1, 0, 0))) {
		    strcpy(why_failed, "GetFsObj 2");
		    goto InvalidObj;
		}
	    }
        }

	/* Check semantics. */
	if ((iErrorCode = CheckGetAttrSemantics(client, &av->vptr, &v->vptr,
						&volptr, &rights, &anyrights))) {
	    strcpy(why_failed, "CheckGetAttrSemantics");
	    goto InvalidObj;
	}

	/* Do it. */
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
		    CodaAddCallBack(client->VenusId, &Piggies[i].Fid, VSGVolnum);
	    }

	    SLog(8, "ViceValidateAttrs: %s ok", FID_(&Piggies[i].Fid));
	    continue;
	}

InvalidObj:
	SLog(1, "ViceValidateAttrs: %s failed (%s)!",
	     FID_(&Piggies[i].Fid), why_failed);
    }
    VFlagBS->SeqLen = NumPiggyFids;

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
long FS_ViceGetACL(RPC2_Handle RPCid, ViceFid *Fid, RPC2_Unsigned InconOK,
		   RPC2_BoundedBS *AccessList, ViceStatus *Status,
		   RPC2_Unsigned PrimaryHost, RPC2_CountedBS *PiggyBS)
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
    SLog(1, "ViceGetACL: Fid = %s, Repair = %d", FID_(Fid), InconOK);

    /* Validate parameters. */
    {
	if ((errorCode = ValidateParms(RPCid, &client, &ReplicatedOp,
				      &Fid->Volume, PiggyBS, NULL)))
	    goto FreeLocks;
    }

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
  ViceStore: Store a file or directory
*/
long FS_ViceStore(RPC2_Handle RPCid, ViceFid *Fid,
		  ViceStatus *Status, RPC2_Integer Length,
		  RPC2_Unsigned PrimaryHost,
		  ViceStoreId *StoreId, RPC2_CountedBS *OldVS, 
		  RPC2_Integer *NewVS, CallBackStatus *VCBStatus,
		  RPC2_CountedBS *PiggyBS, SE_Descriptor *BD)
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

START_TIMING(Store_Total);
    SLog(1, "ViceStore: Fid = %s", FID_(Fid));

    /* Validate parameters. */
    {
	if ((errorCode = ValidateParms(RPCid, &client, &ReplicatedOp,
				      &Fid->Volume, PiggyBS, NULL)))
	    goto FreeLocks;
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
	// We only use the parent node for ACL checks, allow inconsistency
	if ((errorCode = GetFsObj(&pFid, &volptr, &av->vptr, READ_LOCK, NO_LOCK, 1, 0, 0)))
	    goto FreeLocks;
    }

    /* Check semantics. */
    {
	if ((errorCode = CheckStoreSemantics(client, &av->vptr, &v->vptr,
					     &volptr, ReplicatedOp, StoreVCmp,
					     &Status->VV, Status->DataVersion,
					     &rights, &anyrights)))
	    goto FreeLocks;
    }

    /* Perform operation. */
    {
	int tblocks = (int) (nBlocks(Length) - nBlocks(v->vptr->disk.length));
	if ((errorCode = AdjustDiskUsage(volptr, tblocks)))
	    goto FreeLocks;
	deltablocks = tblocks;

	v->f_finode = icreate(V_device(volptr), V_id(volptr),
			      v->vptr->vnodeNumber,
			      v->vptr->disk.uniquifier,
			      v->vptr->disk.dataVersion + 1);
	CODA_ASSERT(v->f_finode > 0);

	if ((errorCode = StoreBulkTransfer(RPCid, client, volptr, v->vptr,
					   v->f_finode, Length)))
	    goto FreeLocks;
	v->f_sinode = v->vptr->disk.node.inodeNumber;
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

    SLog(2, "ViceStore returns %s", ViceErrorMsg(errorCode));
END_TIMING(Store_Total);
    return(errorCode);
}


/*
  ViceSetAttr: Set attributes of an object
*/
long FS_ViceSetAttr(RPC2_Handle RPCid, ViceFid *Fid, ViceStatus *Status,
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
    SLog(1, "ViceSetAttr: Fid = %s", FID_(Fid));

    /* Validate parameters. */
    {
	if ((errorCode = ValidateParms(RPCid, &client, &ReplicatedOp,
				      &Fid->Volume, PiggyBS, NULL)))
	    goto FreeLocks;
    }

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
	    // We only use the parent node for ACL checks, allow inconsistency
	    if ((errorCode = GetFsObj(&pFid, &volptr, &av->vptr, READ_LOCK, NO_LOCK, 1, 0, 0)))
		goto FreeLocks;
	}
    }

    /* Check semantics. */
    {
	if ((errorCode = CheckSetAttrSemantics(client, &av->vptr, &v->vptr,
					       &volptr, ReplicatedOp,
					       NormalVCmp, Status->Length,
					       Status->Date, Status->Owner,
					       Status->Mode, Mask, &Status->VV,
					       Status->DataVersion, &rights,
					       &anyrights)))
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
	PerformSetAttr(client, VSGVolnum, volptr, v->vptr, ReplicatedOp,
		       Status->Length, Status->Date, Status->Owner,
		       Status->Mode, Mask, StoreId, &v->f_sinode, NewVS);
	if (v->f_sinode != 0) {
	    /* COW only happens on truncation, which can only be a file,
	     * but just in case... */
	    CODA_ASSERT(v->vptr->disk.type != vDirectory);
	    v->f_finode = v->vptr->disk.node.inodeNumber;
	    truncp = 0;
	}

	if (truncp) {
	    /* already checked by CheckSetAttrSemantics, but just in case... */
	    CODA_ASSERT(v->vptr->disk.type != vDirectory);
	    v->f_tinode = v->vptr->disk.node.inodeNumber;
	    v->f_tlength = v->vptr->disk.length;
	}

	SetStatus(v->vptr, Status, rights, anyrights);
	if (ReplicatedOp)
	    SetVSStatus(client, volptr, NewVS, VCBStatus);
    }

    {
	if (!errorCode && ReplicatedOp) {
	    SLog(9, "Going to spool store log record %u %o %u %u\n",
		   Status->Owner, Status->Mode, Status->Author, Status->Date);
	    if ((errorCode = SpoolVMLogRecord(vlist, v, volptr, StoreId, 
					     RES_NewStore_OP, STSTORE, 
					     Status->Owner, Status->Mode,
					     Status->Author, Status->Date,
					     Mask, &Status->VV)) )
		SLog(0, "ViceSetAttr: Error %d during SpoolVMLogRecord\n", 
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

    SLog(2, "ViceSetAttr returns %s", ViceErrorMsg(errorCode));
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
    SLog(1, "ViceSetACL: Fid = %s", FID_(Fid));

    /* Validate parameters. */
    {
	if ((errorCode = ValidateParms(RPCid, &client, &ReplicatedOp,
				      &Fid->Volume, PiggyBS, NULL)))
	    goto FreeLocks;
    }

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

    if (ReplicatedOp && !errorCode) {
	if ((errorCode = SpoolVMLogRecord(vlist, v, volptr, StoreId, 
					  RES_NewStore_OP, ACLSTORE, newACL)) )
	    SLog(0, "ViceSetACL: error %d during SpoolVMLogRecord\n", errorCode);
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

    SLog(1, "ViceCreate: %s, %s", FID_(Did), Name);

    /* Validate parameters. */
    {
	if ((errorCode = ValidateParms(RPCid, &client, &ReplicatedOp, 
				      &Did->Volume, PiggyBS, NULL)))
	    goto FreeLocks;
	
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
	    SLog(0, "ViceCreate: Did = Fid %s", FID_(Did));
	    errorCode = EINVAL;
	    goto FreeLocks;
	}

	/* Deprecated/Inapplicable parameters. */
	if (!FID_EQ(&NullFid, BidFid)) {
	    SLog(0, "ViceCreate: non-Null BidFid %s", FID_(BidFid));
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
			    (char *)Name, &volptr, ReplicatedOp, NormalVCmp,
                            ReplicatedOp ? (void *)&DirStatus->VV : (void *)&DirStatus->DataVersion,
                            ReplicatedOp ? (void *)&Status->VV : (void *)&Status->DataVersion,
                            &rights, &anyrights)))
	    goto FreeLocks;
    }

    /* Perform operation. */
    {
	int tblocks = 0;

	if (ReplicatedOp)
	    GetMyVS(volptr, OldVS, NewVS);

	errorCode = PerformCreate(client, VSGVolnum, volptr, pv->vptr,
				  cv->vptr, (char *)Name, DirStatus->Date,
				  Status->Mode, ReplicatedOp, StoreId,
				  &pv->d_cinode, &tblocks, NewVS);
	deltablocks += tblocks;
	if (errorCode)
	    goto FreeLocks;

	SetStatus(pv->vptr, DirStatus, rights, anyrights);
	SetStatus(cv->vptr, Status, rights, anyrights);

	/* Until CVVV probes? -JJK */
	if (1/*!ReplicatedOp || PrimaryHost == ThisHostAddr*/) 
	    Status->CallBack = CodaAddCallBack(client->VenusId, Fid, VSGVolnum);
	if (ReplicatedOp)
	    SetVSStatus(client, volptr, NewVS, VCBStatus);
    }

    /* Create Log Record */
    if (ReplicatedOp && !errorCode) 
	if ((errorCode = SpoolVMLogRecord(vlist, pv, volptr, StoreId,
					  RES_Create_OP, Name,
					  Fid->Vnode, Fid->Unique, 
					  client ? client->Id : 0)))
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

    SLog(1, "ViceRemove: %s, %s", FID_(Did), Name);

    /* Validate parameters. */
    {
	if ((errorCode = ValidateParms(RPCid, &client, &ReplicatedOp, 
				      &Did->Volume, PiggyBS, NULL)))
	    goto FreeLocks;
    }

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
		            (char *)Name, &volptr, ReplicatedOp, NormalVCmp,
                            ReplicatedOp ? (void *)&DirStatus->VV : (void *)&DirStatus->DataVersion,
                            ReplicatedOp ? (void *)&Status->VV : (void *)&Status->DataVersion,
			    &rights, &anyrights)))
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

		cv->f_sinode = cv->vptr->disk.node.inodeNumber;
		cv->vptr->disk.node.inodeNumber = 0;
	}

	SetStatus(pv->vptr, DirStatus, rights, anyrights);
	SetStatus(cv->vptr, Status, rights, anyrights);
	if (ReplicatedOp)
		SetVSStatus(client, volptr, NewVS, VCBStatus);
    }

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
    
    SLog(1, "ViceLink: %s, %s --> %s", FID_(Did), Name, FID_(Fid));

    /* Validate parameters. */
    {
	if ((errorCode = ValidateParms(RPCid, &client, &ReplicatedOp,
				      &Did->Volume, PiggyBS, NULL)))
	    goto FreeLocks;
    
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
	    SLog(0, "ViceLink: Did = Fid %s", FID_(Did));
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
	if ((errorCode = CheckLinkSemantics(client, &pv->vptr, &cv->vptr,
                            (char *)Name, &volptr, ReplicatedOp, NormalVCmp,
                            ReplicatedOp ? (void *)&DirStatus->VV : (void *)&DirStatus->DataVersion,
                            ReplicatedOp ? (void *)&Status->VV : (void *)&Status->DataVersion,
                            &rights, &anyrights)))
	    goto FreeLocks;
    }

    /* Perform operation. */
    {
	if (ReplicatedOp)
	    GetMyVS(volptr, OldVS, NewVS);

	errorCode = PerformLink(client, VSGVolnum, volptr, pv->vptr, cv->vptr,
				(char *)Name, DirStatus->Date, ReplicatedOp,
				StoreId, &pv->d_cinode, &deltablocks, NewVS);
	if (errorCode)
	    goto FreeLocks;

	SetStatus(pv->vptr, DirStatus, rights, anyrights);
	SetStatus(cv->vptr, Status, rights, anyrights);
	if (ReplicatedOp)
	    SetVSStatus(client, volptr, NewVS, VCBStatus);
    }

    if (ReplicatedOp && !errorCode) {
	if ((errorCode = SpoolVMLogRecord(vlist, pv, volptr, StoreId,
					  RES_Link_OP, (char *)Name,
					  Fid->Vnode, Fid->Unique, 
					  &(Vnode_vv(cv->vptr)))))
	    SLog(0, "ViceLink: Error %d during SpoolVMLogRecord\n", errorCode);
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
    
    SLog(1, "ViceRename: %s, %s --> %s, %s",
	 FID_(OldDid), OldName, FID_(NewDid), NewName);

    /* Validate parameters. */
    {
	if ((errorCode = ValidateParms(RPCid, &client, &ReplicatedOp,
				      &OldDid->Volume, PiggyBS, NULL)))
	    goto FreeLocks;

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
			    &sv->vptr, (char *)OldName, (tv ? &tv->vptr : 0), 
			    (char *)NewName, &volptr, ReplicatedOp, NormalVCmp,
                            ReplicatedOp ? (void *)&OldDirStatus->VV : (void*)&OldDirStatus->DataVersion,
                            ReplicatedOp ? (void *)&NewDirStatus->VV : (void*)&NewDirStatus->DataVersion,
                            ReplicatedOp ? (void *)&SrcStatus->VV : (void*)&SrcStatus->DataVersion,
                            ReplicatedOp ? (void *)&TgtStatus->VV : (void*)&TgtStatus->DataVersion,
                            &sp_rights, &sp_anyrights, &tp_rights,
                            &tp_anyrights, &s_rights, &s_anyrights,
                            1, 0)))
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
			tv->f_sinode = tv->vptr->disk.node.inodeNumber;
			tv->vptr->disk.node.inodeNumber = 0;
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
    if (ReplicatedOp && !errorCode) 
        errorCode = SpoolRenameLogRecord(RES_Rename_OP, vlist, sv, tv, spv,
                                         tpv, volptr, (char *)OldName,
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

    SLog(1, "ViceMakeDir: %s, %s", FID_(Did), Name);
    
    /* Validate parameters. */
    {
	if ((errorCode = ValidateParms(RPCid, &client, &ReplicatedOp, 
				      &Did->Volume, PiggyBS, NULL)))
	    goto FreeLocks;

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
			    (char *)Name, &volptr, ReplicatedOp, NormalVCmp,
                            ReplicatedOp ? (void *)&DirStatus->VV : (void *)&DirStatus->DataVersion,
                            ReplicatedOp ? (void *)&Status->VV : (void *)&Status->DataVersion,
                            &rights, &anyrights)))
	    goto FreeLocks;
    }

    /* Perform operation. */
    {
	int tblocks = 0;

	if (ReplicatedOp)
	    GetMyVS(volptr, OldVS, NewVS);

	errorCode = PerformMkdir(client, VSGVolnum, volptr, pv->vptr, cv->vptr,
				 (char *)Name, DirStatus->Date, Status->Mode,
				 ReplicatedOp, StoreId, &pv->d_cinode,
				 &tblocks, NewVS);
	deltablocks += tblocks;
	if (errorCode)
	    goto FreeLocks;

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


	if (ReplicatedOp && !errorCode) {
	    if ((errorCode = SpoolVMLogRecord(vlist, pv, volptr, StoreId,
					      RES_MakeDir_OP, Name, 
					      NewDid->Vnode, NewDid->Unique, 
					      client?client->Id:0)) )
		SLog(0, "ViceMakeDir: Error %d during SpoolVMLogRecord for parent\n", errorCode);
	    // spool child's log record 
	    if ( errorCode == 0 )
		CODA_ASSERT(DC_Dirty(cv->vptr->dh));
	    if (!errorCode && (errorCode = SpoolVMLogRecord(vlist, cv, volptr,
							    StoreId, 
							    RES_MakeDir_OP, ".",
							    NewDid->Vnode, 
							    NewDid->Unique,
							    client?client->Id:0)))
		SLog(0, "ViceMakeDir: Error %d during SpoolVMLogRecord for child\n", errorCode);
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

    SLog(1, "ViceRemoveDir: %s, %s", FID_(Did), Name);

    /* Validate parameters. */
    {
	if ((errorCode = ValidateParms(RPCid, &client, &ReplicatedOp, 
				      &Did->Volume, PiggyBS, NULL)))
	    goto FreeLocks;
    }

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
			    (char *)Name, &volptr, ReplicatedOp, NormalVCmp,
                            ReplicatedOp ? (void *)&DirStatus->VV : (void *)&DirStatus->DataVersion,
                            ReplicatedOp ? (void *)&Status->VV : (void *)&Status->DataVersion,
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

    if (ReplicatedOp) 
	if ((errorCode = SpoolVMLogRecord(vlist, pv, volptr, StoreId,
					  RES_RemoveDir_OP, Name, 
					  ChildDid.Vnode, ChildDid.Unique, 
					  VnLog(cv->vptr), &(Vnode_vv(cv->vptr).StoreId),
					  &(Vnode_vv(cv->vptr).StoreId))))
	    SLog(0, "ViceRemoveDir: error %d in SpoolVMLogRecord\n", errorCode);

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

    SLog(1, "ViceSymLink: %s, %s --> %s", FID_(Did), NewName, OldName);

    /* Validate parameters. */
    {
	if ((errorCode = ValidateParms(RPCid, &client, &ReplicatedOp,
				      &Did->Volume, PiggyBS, NULL)))
	    goto FreeLocks;

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
	    SLog(0, "ViceSymLink: Did = Fid %s", FID_(Did));
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
			    (char *)NewName, &volptr, ReplicatedOp, NormalVCmp,
                            ReplicatedOp ? (void *)&DirStatus->VV : (void *)&DirStatus->DataVersion,
                            ReplicatedOp ? (void *)&Status->VV : (void *)&Status->DataVersion,
                            &rights, &anyrights)))
	    goto FreeLocks;
    }

    /* Perform operation. */
    {
	cv->f_finode = icreate((int) V_device(volptr), (int) V_id(volptr),
			       (int) cv->vptr->vnodeNumber,
			       (int) cv->vptr->disk.uniquifier, 1);
	CODA_ASSERT(cv->f_finode > 0);
	int linklen = (int) strlen((char *)OldName);
	CODA_ASSERT(iwrite((int) V_device(volptr), (int) cv->f_finode, (int) V_parentId(volptr),
		      0, (char *)OldName, linklen) == linklen);
	if (ReplicatedOp)
	    GetMyVS(volptr, OldVS, NewVS);

	int tblocks = 0;
	errorCode = PerformSymlink(client, VSGVolnum, volptr, pv->vptr,
				   cv->vptr, (char *)NewName, cv->f_finode,
				   linklen, DirStatus->Date, Status->Mode,
				   ReplicatedOp, StoreId, &pv->d_cinode,
				   &tblocks, NewVS);
	deltablocks += tblocks;
	if (errorCode)
	    goto FreeLocks;

	SetStatus(pv->vptr, DirStatus, rights, anyrights);
	SetStatus(cv->vptr, Status, rights, anyrights);

	/* Until CVVV probes? -JJK */
	if (1/*!ReplicatedOp || PrimaryHost == ThisHostAddr*/) 
	    Status->CallBack = CodaAddCallBack(client->VenusId, Fid, VSGVolnum);
	if (ReplicatedOp)
	    SetVSStatus(client, volptr, NewVS, VCBStatus);
    }

    /* Create Log Record */
    if (ReplicatedOp) 
	if ((errorCode = SpoolVMLogRecord(vlist, pv, volptr, StoreId,
					  RES_SymLink_OP, 
					  NewName, Fid->Vnode, Fid->Unique,
					  client?client->Id:0)) )
	    SLog(0, "ViceSymLink: Error %d in SpoolVMLogRecord\n", errorCode);
    
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
    static PRS_InternalCPS * anyCPS = 0;

    if (!anyCPS && AL_GetInternalCPS(AnyUserId, &anyCPS) != 0) {
	SLog(0, "'" PRS_ANYUSERGROUP "' no CPS");
    }

    if (AL_CheckRights(ACL, anyCPS, (int *)anyrights) != 0) {
	SLog(0, "CheckRights failed");
	*anyrights = 0;
    }
    
    if (AL_CheckRights(ACL, CPS, (int *)rights) != 0) {
	*rights = 0;
    }

    /* When a client can throw away it's tokens, and then perform some
     * operation, the client essentially has the same rights while
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
	    memset((void *)vvs, 0, (int)(VSG_MEMBERS * sizeof(ViceVersionVector *)));
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
	int      size;

	if (vptr->disk.type == vDirectory) {

		SLog(0, "CopyOnWrite: Copying directory vnode = %x", 
		     vptr->vnodeNumber);
		VN_CopyOnWrite(vptr);

	} else {
		SLog(0, "CopyOnWrite: Copying inode for files (vnode%d)", 
		     vptr->vnodeNumber);
		size = (int) vptr->disk.length;
		ino = icreate(V_device(volptr), V_id(volptr),
			      vptr->vnodeNumber,
			      vptr->disk.uniquifier,
			      vptr->disk.dataVersion);
		CODA_ASSERT(ino > 0);
		if (size > 0) {
		    int infd, outfd, rc;
		    infd = iopen(V_device(volptr), vptr->disk.node.inodeNumber,
				 O_RDONLY);
		    outfd = iopen(V_device(volptr), ino, O_WRONLY);
		    CODA_ASSERT(infd && outfd);

		    START_TIMING(CopyOnWrite_iwrite);
		    rc = copyfile(infd, outfd);
		    END_TIMING(CopyOnWrite_iwrite);

		    CODA_ASSERT(rc != -1);

		    close(infd);
		    close(outfd);
		}

		/*
		  START_NSC_TIMING(CopyOnWrite_idec);
		  CODA_ASSERT(!(idec(V_device(volptr),
		  vptr->disk.node.inodeNumber, V_parentId(volptr))));
		  END_NSC_TIMING(CopyOnWrite_idec);
		*/

		vptr->disk.node.inodeNumber = ino;
		vptr->disk.cloned = 0;
	}
}


int SystemUser(ClientEntry *client)
{
    return client && AL_IsAMember(SystemId, client->CPS);
}


int AdjustDiskUsage(Volume *volptr, int length)
{
    Error rc = VAdjustDiskUsage(volptr, length);

    if (rc == ENOSPC)
	SLog(0, "Partition %s that contains volume %u is full",
	     volptr->partition->name, V_id(volptr));

    else if(rc == EDQUOT)
	SLog(0, "Volume %u (%s) is full (quota reached)",
	     V_id(volptr), V_name(volptr));

    else if (rc)
	SLog(0, "Got error return %s from VAdjustDiskUsage", ViceErrorMsg(rc));

    return(rc);
}

int CheckDiskUsage(Volume *volptr, int length)
{
    Error rc = VCheckDiskUsage(volptr, length);

    if (rc == ENOSPC)
	SLog(0, "Partition %s that contains volume %u is full",
	     volptr->partition->name, V_id(volptr));

    else if (rc == EDQUOT)
	SLog(0, "Volume %u (%s) is full (quota reached)",
	     V_id(volptr), V_name(volptr));

    else if (rc)
	SLog(0, "Got error return %s from VCheckDiskUsage", ViceErrorMsg(rc));

    return(rc);
}

/* This function seems to be called when directory entries are added/removed.
 * But directory data is stored in RVM, and not on-disk.
 * Something seems wrong here --JH */
void ChangeDiskUsage(Volume *volptr, int length)
{
    VAdjustDiskUsage(volptr, length);
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
    SLog(10, "AllocVnode: Fid = %s, type = %d, pFid = %s, Owner = %d",
	 FID_(Fid), vtype, FID_(pFid), ClientId);

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
    (*vptr)->disk.node.dirNode = NEWVNODEINODE;
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
	if (!SystemUser(client)) {
	    if (((*vptr)->disk.type == vDirectory && !(*rights & PRSFS_LOOKUP)) ||
		((*vptr)->disk.type != vDirectory && !(*rights & PRSFS_READ))) {
		SLog(1, "CheckFetchSemantics: rights violation (%x : %x) %s",
		     *rights, *anyrights, FID_(&Fid));
		return(EACCES);
	    }

	    if (!IsOwner && (*vptr)->disk.type == vFile) {
		if (CheckReadMode(client, *vptr)) {
		    SLog(1, "CheckFetchSemantics: mode-bits violation %s",
			 FID_(&Fid));
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
	    SLog(0, "CheckGetACLSemantics: non-directory %s", FID_(&Fid));
	    return(ENOTDIR);
	}
	if (AccessList->MaxSeqLen == 0) {
	    SLog(0, "CheckGetACLSemantics: zero-len ACL %s", FID_(&Fid));
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
	unsigned int eACLlen = strlen((char *)*eACL) + 1;
	if (AccessList->MaxSeqLen < eACLlen) {
	    SLog(0, "CheckGetACLSemantics: eACLlen (%u) > ACL->MaxSeqLen (%u) %s",
		 eACLlen, AccessList->MaxSeqLen, FID_(&Fid));
	    return(E2BIG);
	}
	SLog(10, "CheckGetACLSemantics: ACL is:\n%s", *eACL);
    }

    return(0);
}

static int IsVirgin(Vnode *vptr)
{
    return (vptr->disk.type != vDirectory) && !vptr->disk.node.inodeNumber;
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

    /* Concurrency-control checks. */
    if (VCmpProc) {
	void *arg1 = (ReplicatedOp ? (void *)&Vnode_vv(*vptr) : (void *)&(*vptr)->disk.dataVersion);
	void *arg2 = (ReplicatedOp ? (void *)VV : (void *)&DataVersion);
	if ((errorCode = VCmpProc(ReplicatedOp, (*vptr)->disk.type, arg1, arg2))) {
	    SLog(0, "CheckStoreSemantics: %s, VCP error (%d)",
		 FID_(&Fid), errorCode);
	    return(errorCode);
	}
    }

    /* Integrity checks. */
    {
	if ((*vptr)->disk.type != vFile) {
	    SLog(0, "CheckStoreSemantics: %s not a file", FID_(&Fid));
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
	if (!(*rights & PRSFS_WRITE) && !(IsOwner && IsVirgin(*vptr))) {
	    SLog(0, "CheckStoreSemantics: rights violation (%x : %x) %s",
		 *rights, *anyrights, FID_(&Fid));
	    return(EACCES);
	}
    }

    return(0);
}


int CheckSetAttrSemantics(ClientEntry *client, Vnode **avptr, Vnode **vptr,
			  Volume **volptr, int ReplicatedOp, VCP VCmpProc,
			  RPC2_Integer Length, Date_t Mtime,
			  UserId Owner, RPC2_Unsigned Mode, 
			  RPC2_Integer Mask,
			  ViceVersionVector *VV, FileVersion DataVersion,
			  Rights *rights, Rights *anyrights)
{
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

    /* Concurrency-control checks. */
    if (VCmpProc) {
	void *arg1 = (ReplicatedOp ? (void *)&Vnode_vv(*vptr) : (void *)&(*vptr)->disk.dataVersion);
	void *arg2 = (ReplicatedOp ? (void *)VV : (void *)&DataVersion);
	if ((errorCode = VCmpProc(ReplicatedOp, (*vptr)->disk.type, arg1, arg2))) {
	    SLog(0, "CheckSetAttrSemantics: %s, VCP error (%d)",
		 FID_(&Fid), errorCode);
	    return(errorCode);
	}
    }

    /* Integrity checks. */
    {
	/* chmod should not be allowed on symlinks, in fact it shouldn't even
	 * be possible since the kernel will have already followed the link
	 * in chmod or fchmod. Still, it is better not to assume the client
	 * does the right thing. --JH */
	if (chmodp && (*vptr)->disk.type == vSymlink)
	    return EACCES;

	if (truncp) { 
	    if ((*vptr)->disk.type != vFile) {
		SLog(0, "CheckSetAttrSemantics: non-file truncate %s",
		     FID_(&Fid));
		return(EISDIR);
	    }
	    if (Length > (long)(*vptr)->disk.length) {
		SLog(0, "CheckSetAttrSemantics: truncate length bad %s",
		     FID_(&Fid));
		return(EINVAL);
	    }
	}

	/* just log a message if nothing is changing - don't be paranoid about returning an error */
	if (chmodp + chownp + truncp + utimesp == 0) {
	    SLog(0, "CheckSetAttrSemantics: no attr set %s", FID_(&Fid));
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

	if (IsOwner && IsVirgin(*vptr)) {
	    /* Bypass protection checks on first store after a create EXCEPT for chowns. */
	    if (chownp) {
		SLog(0, "CheckSetAttrSemantics: owner chown'ing virgin %s",
		     FID_(&Fid));
		return(EACCES);
	    }
	}
	else {
	    if (!SystemUser(client)) {
		/* System users are subject to no further permission checks. */
		/* Other users require WRITE permission for file, */
		/* INSERT | DELETE for directories. */
		if ((*vptr)->disk.type == vDirectory) {
		    if (!(*rights & (PRSFS_INSERT | PRSFS_DELETE))) {
			SLog(0, "CheckSetAttrSemantics: rights violation (%x : %x) %s",
				*rights, *anyrights, FID_(&Fid));
			return(EACCES);
		    }
		}
		else {
		    if (!(*rights & PRSFS_WRITE)) {
			SLog(0, "CheckSetAttrSemantics: rights violation (%x : %x) %s",
				*rights, *anyrights, FID_(&Fid));
			return(EACCES);
		    }
		}

		if (chmodp) {
		    /* No further access checks. */
		}

		if (chownp && !IsOwner) {
		    SLog(0, "CheckSetAttrSemantics: non-owner chown'ing %s",
			 FID_(&Fid));
		    return(EACCES);
		}

		if (truncp && CheckWriteMode(client, (*vptr)) != 0) {
		    SLog(0, "CheckSetAttrSemantics: truncating %s", FID_(&Fid));
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
			   RPC2_CountedBS *AccessList, AL_AccessList **newACL)
{
    int rc = 0;
    ViceFid Fid;
    Fid.Volume = V_id(*volptr);
    Fid.Vnode = (*vptr)->vnodeNumber;
    Fid.Unique = (*vptr)->disk.uniquifier;
    int IsOwner = (client->Id == (long)(*vptr)->disk.owner);

    /* Concurrency-control checks. */
    if (VCmpProc) {
	void *arg1 = (ReplicatedOp ? (void *)&Vnode_vv(*vptr) : (void *)&(*vptr)->disk.dataVersion);
	void *arg2 = (ReplicatedOp ? (void *)VV : (void *)&DataVersion);
	rc = VCmpProc(ReplicatedOp, (*vptr)->disk.type, arg1, arg2);
	if (rc) {
	    SLog(0, "CheckSetACLSemantics: %s, VCP error (%d)", FID_(&Fid), rc);
	    return rc;
	}
    }

    /* Integrity checks. */
    {
	if ((*vptr)->disk.type != vDirectory) {
	    SLog(0, "CheckSetACLSemantics: %s not a directory", FID_(&Fid));
	    return ENOTDIR;
	}
	if (AccessList->SeqLen == 0) {
	    SLog(0, "CheckSetACLSemantics: %s zero-len ACL", FID_(&Fid));
	    return EINVAL;
	}
    }

    /* Protection checks. */
    {
	/* Get the access list. */
	AL_AccessList *aCL = 0;
	int aCLSize = 0;
	SetAccessList(*vptr, aCL, aCLSize);

	/* Get this client's rights. */
	Rights t_rights, t_anyrights;
	if (!rights)    rights = &t_rights;
	if (!anyrights) anyrights = &t_anyrights;
	CODA_ASSERT(GetRights(client->CPS, aCL, aCLSize, rights, anyrights) == 0);

	/* ADMINISTER permission (normally) required. */
	if (!(*rights & PRSFS_ADMINISTER) && !IsOwner && !SystemUser(client)) {
	    SLog(0, "CheckSetACLSemantics: %s Rights violation (%x : %x)",
		 FID_(&Fid), *rights, *anyrights);
	    return EACCES;
	}
	rc = AL_Internalize((AL_ExternalAccessList)AccessList->SeqBody, newACL);
	if (rc) {
	    SLog(0, "CheckSetACLSemantics: %s ACL internalize failed (%s)",
		 FID_(&Fid), strerror(rc));
	    return rc;
	}
	if ((*newACL)->MySize + 4 > aCLSize) {
	    SLog(0, "CheckSetACLSemantics: %s Not enough space to store ACL",
		 FID_(&Fid));
	    return ENOSPC;
	}
    }
    return 0;
}


int CheckCreateSemantics(ClientEntry *client, Vnode **dirvptr, Vnode **vptr, char *Name,
			   Volume **volptr, int ReplicatedOp, VCP VCmpProc,
			   void *dirVersion, void *Version,
			   Rights *rights, Rights *anyrights, int MakeProtChecks) {
    return(Check_CLMS_Semantics(client, dirvptr, vptr, Name, volptr, CLMS_Create, ReplicatedOp,
				 VCmpProc, dirVersion, Version, rights, anyrights, MakeProtChecks));
}


int CheckRemoveSemantics(ClientEntry *client, Vnode **dirvptr, Vnode **vptr, char *Name,
			   Volume **volptr, int ReplicatedOp, VCP VCmpProc,
			   void *dirVersion, void *Version,
			   Rights *rights, Rights *anyrights, int MakeProtChecks) {
    return(Check_RR_Semantics(client, dirvptr, vptr, Name, volptr, vFile, ReplicatedOp,
			       VCmpProc, dirVersion, Version, rights, anyrights, MakeProtChecks));
}


int CheckLinkSemantics(ClientEntry *client, Vnode **dirvptr, Vnode **vptr, char *Name,
			 Volume **volptr, int ReplicatedOp, VCP VCmpProc,
			 void *dirVersion, void *Version,
			 Rights *rights, Rights *anyrights, int MakeProtChecks) {
    return(Check_CLMS_Semantics(client, dirvptr, vptr, Name, volptr, CLMS_Link, ReplicatedOp,
				VCmpProc, dirVersion, Version, rights, anyrights, MakeProtChecks));
}


int CheckRenameSemantics(ClientEntry *client, Vnode **s_dirvptr, Vnode **t_dirvptr,
			 Vnode **s_vptr, char *OldName, Vnode **t_vptr, char *NewName,
			 Volume **volptr, int ReplicatedOp, VCP VCmpProc,
			 void *s_dirVersion, void *t_dirVersion,
			 void *s_Version, void *t_Version,
			 Rights *sd_rights, Rights *sd_anyrights,
			 Rights *td_rights, Rights *td_anyrights,
			 Rights *s_rights, Rights *s_anyrights, 
			 int MakeProtChecks, int IgnoreTargetNonEmpty,
			 dlist *vlist)
{
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

	/* Source directory. */
	{
	    arg1 = (ReplicatedOp ? (void *)&Vnode_vv(*s_dirvptr) : (void *)&(*s_dirvptr)->disk.dataVersion);
	    if ((errorCode = VCmpProc(ReplicatedOp, (*s_dirvptr)->disk.type, arg1, s_dirVersion))) {
		SLog(0, "CheckRenameSemantics: %s, VCP error (%d)",
		     FID_(&SDid), errorCode);
		return(errorCode);
	    }
	}


	/* Target directory. */
	{
	    arg1 = (ReplicatedOp ? (void *)&Vnode_vv(*t_dirvptr) : (void *)&(*t_dirvptr)->disk.dataVersion);
	    if ((errorCode = VCmpProc(ReplicatedOp, (*t_dirvptr)->disk.type, arg1, t_dirVersion))) {
		SLog(0, "CheckRenameSemantics: %s, VCP error (%d)",
		     FID_(&TDid), errorCode);
		return(errorCode);
	    }
	}

	/* Source object. */
	{
	    arg1 = (ReplicatedOp ? (void *)&Vnode_vv(*s_vptr) : (void *)&(*s_vptr)->disk.dataVersion);
	    if ((errorCode = VCmpProc(ReplicatedOp, (*s_vptr)->disk.type, arg1, s_Version))) {
		SLog(0, "CheckRenameSemantics: %s, VCP error (%d)",
		     FID_(&SFid), errorCode);
		return(errorCode);
	    }
	}

	/* Target object. */
	if (TargetExists) {
	    arg1 = (ReplicatedOp ? (void *)&Vnode_vv(*t_vptr) : (void *)&(*t_vptr)->disk.dataVersion);
	    if ((errorCode = VCmpProc(ReplicatedOp, (*t_vptr)->disk.type, arg1, t_Version))) {
		SLog(0, "CheckRenameSemantics: %s, VCP error (%d)",
		     FID_(&TFid), errorCode);
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
		    FID_(&SDid), FID_(&TDid));
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
		SLog(0, "CheckRenameSemantics: source %s not in parent %s", 
		     FID_(&SFid), FID_(&SDid));
		return(EINVAL);
	    }

	    if (FID_EQ(&SDid, &SFid) || FID_EQ(&TDid, &SFid)) {
		SLog(0, "CheckRenameSemantics: %s %s %s %s", 
		     FID_(&SDid), FID_(&TDid), FID_(&SFid), FID_(&TFid));
		return(ELOOP);
	    }

	    /* Cannot allow rename out of a directory if file has multiple links! */
	    if (!SameParent && (*s_vptr)->disk.type == vFile && (*s_vptr)->disk.linkCount > 1) {
		SLog(0, "CheckRenameSemantics: !SameParent and multiple links %s",
		    FID_(&SFid));
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
		    SLog(0, "CheckRenameSemantics: target %s not in parent %s", 
			 FID_(&TFid), FID_(&TDid));
		    return(EINVAL);
		}

		if (FID_EQ(&SDid, &TFid) || FID_EQ(&TDid, &TFid) || FID_EQ(&SFid, &TFid)) {
		    SLog(0, "CheckRenameSemantics: %s %s %s %s", 
			 FID_(&SDid), FID_(&TDid), FID_(&SFid), FID_(&TFid));
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
			ViceFid fid = TestFid;
			fid.Volume = V_id(*volptr);
			SLog(0, "CheckRenameSemantics: avoiding deadlock on %s",
			     FID_(&fid));
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
		SLog(0, "CheckRenameSemantics: sd rights violation (%x : %x) %s", 
		     *sd_rights, *sd_anyrights, FID_(&SDid));
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
		SLog(0, "CheckRenameSemantics: td rights violation (%x : %x) %s", 
		     *td_rights, *td_anyrights, FID_(&TDid));
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
			  void *dirVersion, void *Version,
			  Rights *rights, Rights *anyrights, int MakeProtChecks) {
    return(Check_CLMS_Semantics(client, dirvptr, vptr, Name, volptr, CLMS_MakeDir, ReplicatedOp,
				 VCmpProc, dirVersion, Version, rights, anyrights, MakeProtChecks));
}


int CheckRmdirSemantics(ClientEntry *client, Vnode **dirvptr, Vnode **vptr, char *Name,
			  Volume **volptr, int ReplicatedOp, VCP VCmpProc,
			  void *dirVersion, void *Version,
			  Rights *rights, Rights *anyrights, int MakeProtChecks) {
    return(Check_RR_Semantics(client, dirvptr, vptr, Name, volptr, vDirectory, ReplicatedOp,
			       VCmpProc, dirVersion, Version, rights, anyrights, MakeProtChecks));
}


int CheckSymlinkSemantics(ClientEntry *client, Vnode **dirvptr, Vnode **vptr, char *Name,
			    Volume **volptr, int ReplicatedOp, VCP VCmpProc,
			    void *dirVersion, void *Version,
			    Rights *rights, Rights *anyrights, int MakeProtChecks) {
    return(Check_CLMS_Semantics(client, dirvptr, vptr, Name, volptr, CLMS_SymLink, ReplicatedOp,
				 VCmpProc, dirVersion, Version, rights, anyrights, MakeProtChecks));
}

/*
  Check_CLMS_Semantics: Make semantic checks for the create, link,
  mkdir and symlink requests.
*/

/* {vptr,status} may be Null for {Create,Mkdir,Symlink}. */
static int Check_CLMS_Semantics(ClientEntry *client, Vnode **dirvptr, Vnode **vptr, char *Name,
                                Volume **volptr, CLMS_Op opcode, int ReplicatedOp,
                                VCP VCmpProc, void *dirVersion, void *Version,
                                Rights *rights, Rights *anyrights, int MakeProtChecks)
{
    const char *ProcName =
        (opcode == CLMS_Create)  ? "CheckCreateSemantics" :
        (opcode == CLMS_MakeDir) ? "CheckMkdirSemantics" :
        (opcode == CLMS_SymLink) ? "CheckSymlinkSemantics" :
                                   "CheckLinkSemantics" ;
    int errorCode = 0;
    ViceFid Did;
    Did.Volume = V_id(*volptr);
    Did.Vnode = (*dirvptr)->vnodeNumber;
    Did.Unique = (*dirvptr)->disk.uniquifier;
    ViceFid Fid;
    if (vptr && (opcode == CLMS_Create || opcode == CLMS_Link)) {
	Fid.Volume = V_id(*volptr);
	Fid.Vnode = (*vptr)->vnodeNumber;
	Fid.Unique = (*vptr)->disk.uniquifier;
    }
    else
	Fid = NullFid;

    /* Concurrency-control checks. */
    if (VCmpProc) {
	void *arg1 = (ReplicatedOp ? (void *)&Vnode_vv(*dirvptr) : (void *)&(*dirvptr)->disk.dataVersion);
	if ((errorCode = VCmpProc(ReplicatedOp, (*dirvptr)->disk.type, arg1, dirVersion))) {
	    SLog(0, "%s: %s, VCP error (%d)", ProcName, FID_(&Did), errorCode);
	    return(errorCode);
	}

	if (vptr) {
	    arg1 = (ReplicatedOp ? (void *)&Vnode_vv(*vptr) : (void *)&(*vptr)->disk.dataVersion);
	    if ((errorCode = VCmpProc(ReplicatedOp, (*vptr)->disk.type, arg1, Version))) {
		SLog(0, "%s: %s, VCP error (%d)",
		     ProcName, FID_(&Fid), errorCode);
		return(errorCode);
	    }
	}
    }

    /* Integrity checks. */
    {
	if ((*dirvptr)->disk.type != vDirectory){
	    SLog(0, "%s: %s not a directory", ProcName, FID_(&Did));
	    return(ENOTDIR);
	}

	if (STREQ(Name, ".") || STREQ(Name, "..")) {
	    SLog(0, "%s: illegal name (%s)", ProcName, Name);
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
	SLog(9, "%s: Lookup of %s in %s failed", ProcName, Name, FID_(&Did));

	if (vptr) {
	    switch(opcode) {
		case CLMS_Create:
		case CLMS_Link:
		    if ((*vptr)->disk.type != vFile) {
			SLog(0, "%s: %s not a file", ProcName, FID_(&Fid));
			return(EISDIR);
		    }
		    break;

		case CLMS_MakeDir:
		    if ((*vptr)->disk.type != vDirectory) {
			SLog(0, "%s: %s not a dir", ProcName, FID_(&Fid));
			return(ENOTDIR);
		    }
		    break;

		case CLMS_SymLink:
		    if ((*vptr)->disk.type != vSymlink) {
			SLog(0, "%s: %s not a symlink", ProcName, FID_(&Fid));
			return(EISDIR);
		    }
		    break;
	    }

	    if (((*vptr)->disk.vparent != Did.Vnode) || ((*vptr)->disk.uparent != Did.Unique)){
		SLog(0, "%s: cross-directory link %s, %s",
		     ProcName, FID_(&Did), FID_(&Fid));
		return(EXDEV);
	    }
	}
    }

    /* Protection checks. */
    if (MakeProtChecks) {
	/* Get the access list. */
	SLog(9, "%s: Going to get acl %s", ProcName, FID_(&Did));
	AL_AccessList *aCL = 0;
	int aCLSize = 0;
	SetAccessList(*dirvptr, aCL, aCLSize);

	/* Get this client's rights. */
	SLog(9, "%s: Going to get rights %s", ProcName, FID_(&Did));
	Rights t_rights; if (rights == 0) rights = &t_rights;
	Rights t_anyrights; if (anyrights == 0) anyrights = &t_anyrights;
	CODA_ASSERT(GetRights(client->CPS, aCL, aCLSize, rights, anyrights) == 0);

	/* INSERT permission required. */
	if (!(*rights & PRSFS_INSERT)) {
	    SLog(0, "%s: rights violation (%x : %x) %s",
		 ProcName, *rights, *anyrights, FID_(&Did));
	    return(EACCES);
	}
    }
    
    return(0);
}


static int Check_RR_Semantics(ClientEntry *client, Vnode **dirvptr, Vnode **vptr, char *Name,
			       Volume **volptr, VnodeType type, int ReplicatedOp,
			       VCP VCmpProc, void *dirVersion, void *Version,
                               Rights *rights, Rights *anyrights, int MakeProtChecks)
{
    const char *ProcName = (type == vDirectory)
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

	arg1 = (ReplicatedOp ? (void *)&Vnode_vv(*dirvptr) : (void *)&(*dirvptr)->disk.dataVersion);
	if ((errorCode = VCmpProc(ReplicatedOp, (*dirvptr)->disk.type, arg1, dirVersion))) {
		SLog(0, "%s: %s, VCP error (%d)", ProcName, 
		     FID_(&Did), errorCode);
		return(errorCode);
	}

	arg1 = (ReplicatedOp ? (void *)&Vnode_vv(*vptr) : (void *)&(*vptr)->disk.dataVersion);
	if ((errorCode = VCmpProc(ReplicatedOp, (*vptr)->disk.type, arg1, Version))) {
	    SLog(0, "%s: %s, VCP error (%d)", ProcName, FID_(&Fid), errorCode);
	    return(errorCode);
	}
    }

    /* Integrity checks. */
    {
	if ((*dirvptr)->disk.type != vDirectory) {
	    SLog(0, "%s: parent %s not a directory", ProcName, FID_(&Did));
	    return(ENOTDIR);
	}

	if (STREQ(Name, ".") || STREQ(Name, "..")) {
	    SLog(0, "%s: illegal name (%s)", ProcName, Name);
	    return(EINVAL);
	}

	if ((*vptr)->disk.vparent != Did.Vnode || 
	    (*vptr)->disk.uparent != Did.Unique) {
		SLog(0, "%s: child %s not in parent %s", ProcName, 
		     FID_(&Fid), FID_(&Did));
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
	    SLog(0, "%s: rights violation (%x : %x) %s", 
		 ProcName, *rights, *anyrights, FID_(&Did));
	    return(EACCES);
	}

	/* Only System:Administrators are allowed to remove the magic symlinks
	 * we use for mountpoints (mode 0644 instead of 0777). We only test the
	 * user execute bit because we used to create symlinks with the
	 * modebits set to 0755. If we are too strict people won't be able to
	 * remove those symlinks. */
	if ((*vptr)->disk.type == vSymlink &&
	    ((*vptr)->disk.modeBits & S_IXUSR) == 0 &&
	    !SystemUser(client))
	{
	    return EACCES;
	}
    }

    return(errorCode);
}


void PerformFetch(ClientEntry *client, Volume *volptr, Vnode *vptr) {
    /* Nothing to do here. */
}


int FetchBulkTransfer(RPC2_Handle RPCid, ClientEntry *client, 
		      Volume *volptr, Vnode *vptr, RPC2_Unsigned Offset,
		      ViceVersionVector *VV)
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
    int fd = -1;

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
	sid.Value.SmartFTPD.ByteQuota = -1;
	if (vptr->disk.type != vDirectory) {
	    if (vptr->disk.node.inodeNumber) {
		fd = iopen(V_device(volptr), vptr->disk.node.inodeNumber, O_RDONLY);
		sid.Value.SmartFTPD.Tag = FILEBYFD;
		sid.Value.SmartFTPD.FileInfo.ByFD.fd = fd;
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
	    case FILEBYFD:
		SLog(9, "Tag = FILEBYFD, fd = %d",
			sid.Value.SmartFTPD.FileInfo.ByFD.fd);
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
	    SLog(0, "FetchBulkTransfer: CheckSE failed (%d), %s",
		 errorCode, FID_(&Fid));
	    if (errorCode == RPC2_SEFAIL1) errorCode = EIO;
	    goto Exit;
	}

	/* compensate Length for the data we skipped because of the requested
	 * Offset */
	Length -= Offset;

	RPC2_Integer len = sid.Value.SmartFTPD.BytesTransferred;
	if (len != Length) {
	    SLog(0, "FetchBulkTransfer: length discrepancy (%d : %d), %s, %s %s.%d",
		 Length, len, FID_(&Fid),
		 client->UserName, inet_ntoa(client->VenusId->host),
		 ntohs(client->VenusId->port));
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

	SLog(2, "FetchBulkTransfer: transferred %d bytes %s",
	     Length, FID_(&Fid));
    }

Exit:
    if (fd != -1) close(fd);

    if (vptr->disk.type == vDirectory)
	    VN_PutDirHandle(vptr);
    return(errorCode);
}

int FetchFileByName(RPC2_Handle RPCid, char *name, ClientEntry *client)
{
    int errorCode = 0;
    SE_Descriptor sid;
    memset(&sid, 0, sizeof(SE_Descriptor));
    sid.Tag = client ? client->SEType : SMARTFTP;
    sid.Value.SmartFTPD.TransmissionDirection = CLIENTTOSERVER;
    sid.Value.SmartFTPD.Tag = FILEBYNAME;
    sid.Value.SmartFTPD.FileInfo.ByName.ProtectionBits = 0600;
    strncpy(sid.Value.SmartFTPD.FileInfo.ByName.LocalFileName, name, 255);
    sid.Value.SmartFTPD.ByteQuota = -1;
    sid.Value.SmartFTPD.SeekOffset = 0;
    sid.Value.SmartFTPD.hashmark = 0;
    if ((errorCode = (int) RPC2_InitSideEffect(RPCid, &sid))
	<= RPC2_ELIMIT) {
	SLog(0, "FetchFileByFD: InitSideEffect failed %d", 
		errorCode);
	return(errorCode);
    }

    if ((errorCode = (int) RPC2_CheckSideEffect(RPCid, &sid, SE_AWAITLOCALSTATUS)) 
	<= RPC2_ELIMIT) {
	if (errorCode == RPC2_SEFAIL1) errorCode = EIO;
	SLog(0, "FetchFileByFD: CheckSideEffect failed %d",
		errorCode);
	return(errorCode);
    }    
    return(0);
}

int FetchFileByFD(RPC2_Handle RPCid, int fd, ClientEntry *client)
{
    int errorCode = 0;
    SE_Descriptor sid;
    memset(&sid, 0, sizeof(SE_Descriptor));
    sid.Tag = client ? client->SEType : SMARTFTP;
    sid.Value.SmartFTPD.TransmissionDirection = CLIENTTOSERVER;
    sid.Value.SmartFTPD.Tag = FILEBYFD;
    sid.Value.SmartFTPD.FileInfo.ByFD.fd = fd;
    sid.Value.SmartFTPD.ByteQuota = -1;
    sid.Value.SmartFTPD.SeekOffset = 0;
    sid.Value.SmartFTPD.hashmark = 0;
    if ((errorCode = (int) RPC2_InitSideEffect(RPCid, &sid))
	<= RPC2_ELIMIT) {
	SLog(0, "FetchFileByFD: InitSideEffect failed %d", 
		errorCode);
	return(errorCode);
    }

    if ((errorCode = (int) RPC2_CheckSideEffect(RPCid, &sid, SE_AWAITLOCALSTATUS)) 
	<= RPC2_ELIMIT) {
	if (errorCode == RPC2_SEFAIL1) errorCode = EIO;
	SLog(0, "FetchFileByFD: CheckSideEffect failed %d",
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
    vptr->disk.node.inodeNumber = newinode;
    vptr->disk.length = Length;
    vptr->disk.unixModifyTime = Mtime;
    vptr->disk.author = client->Id;
    vptr->disk.dataVersion++;
    if (ReplicatedOp) {
	NewCOP1Update(volptr, vptr, StoreId, vsptr);

	/* Await COP2 message. */
	ViceFid fids[MAXFIDS];
	memset((void *)fids, 0, (int)(MAXFIDS * sizeof(ViceFid)));
	fids[0] = Fid;
	CopPendingMan->add(new cpent(StoreId, fids));
    }
}


int StoreBulkTransfer(RPC2_Handle RPCid, ClientEntry *client, Volume *volptr,
		       Vnode *vptr, Inode newinode, RPC2_Integer Length)
{
    int errorCode = 0;
    ViceFid Fid;
    int fd = -1;

    Fid.Volume = V_id(volptr);
    Fid.Vnode = vptr->vnodeNumber;
    Fid.Unique = vptr->disk.uniquifier;

START_TIMING(Store_Xfer);

    /* Do the bulk transfer. */
    {
	struct timeval StartTime, StopTime;
	TM_GetTimeOfDay(&StartTime, 0);

	fd = iopen(V_device(volptr), newinode, O_WRONLY | O_TRUNC);

	SE_Descriptor sid;
	memset(&sid, 0, sizeof(SE_Descriptor));
	sid.Tag = client->SEType;
	sid.Value.SmartFTPD.TransmissionDirection = CLIENTTOSERVER;
	sid.Value.SmartFTPD.SeekOffset = 0;
	sid.Value.SmartFTPD.hashmark = (SrvDebugLevel > 2 ? '#' : '\0');
	sid.Value.SmartFTPD.Tag = FILEBYFD;
	sid.Value.SmartFTPD.ByteQuota = Length;
	sid.Value.SmartFTPD.FileInfo.ByFD.fd = fd;

	if((errorCode = (int) RPC2_InitSideEffect(RPCid, &sid)) <= RPC2_ELIMIT) {
	    SLog(0, "StoreBulkTransfer: InitSE failed (%d), %s",
		 errorCode, FID_(&Fid));
	    goto Exit;
	}

	if ((errorCode = (int) RPC2_CheckSideEffect(RPCid, &sid, SE_AWAITLOCALSTATUS)) <= RPC2_ELIMIT) {
	    SLog(0, "StoreBulkTransfer: CheckSE failed (%d), %s",
		 errorCode, FID_(&Fid));
	    if (errorCode == RPC2_SEFAIL1) errorCode = EIO;
	    goto Exit;
	}

	RPC2_Integer len = sid.Value.SmartFTPD.BytesTransferred;
	if (Length != -1 && Length != len) {
	    SLog(0, "StoreBulkTransfer: length discrepancy (%d : %d), %s, %s %s.%d",
		 Length, len, FID_(&Fid), client->UserName,
		 inet_ntoa(client->VenusId->host),ntohs(client->VenusId->port));
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

	SLog(2, "StoreBulkTransfer: transferred %d bytes %s",
	     Length, FID_(&Fid));
    }

Exit:
    if (fd != -1) close(fd);

END_TIMING(Store_Xfer);
    return(errorCode);
}


void PerformSetAttr(ClientEntry *client, VolumeId VSGVolnum, Volume *volptr,
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
	    CODA_ASSERT(vptr->disk.type != vDirectory);
	    *CowInode = vptr->disk.node.inodeNumber;
	    CopyOnWrite(vptr, volptr);
	}

    if (ReplicatedOp) {
	NewCOP1Update(volptr, vptr, StoreId, vsptr);

	/* Await COP2 message. */
	ViceFid fids[MAXFIDS];
	memset((void *)fids, 0, MAXFIDS * (int) sizeof(ViceFid));
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
    memcpy(aCL, newACL, newACL->MySize);

    if (ReplicatedOp) {
	NewCOP1Update(volptr, vptr, StoreId, vsptr);

	/* Await COP2 message. */
	ViceFid fids[MAXFIDS];
	memset((void *)fids, 0, (int) (MAXFIDS * sizeof(ViceFid)));
	fids[0] = Fid;
	CopPendingMan->add(new cpent(StoreId, fids));
    }
}

int PerformCreate(ClientEntry *client, VolumeId VSGVolnum, Volume *volptr,
                   Vnode *dirvptr, Vnode *vptr, char *Name, Date_t Mtime, RPC2_Unsigned Mode,
                   int ReplicatedOp, ViceStoreId *StoreId,
                   PDirInode *CowInode, int *blocks, RPC2_Integer *vsptr)
{
    return Perform_CLMS(client, VSGVolnum, volptr, dirvptr, vptr, CLMS_Create,
			Name, 0, 0, Mtime, Mode, ReplicatedOp, StoreId,
			CowInode, blocks, vsptr);
}


void PerformRemove(ClientEntry *client, VolumeId VSGVolnum, Volume *volptr,
                   Vnode *dirvptr, Vnode *vptr, char *Name, Date_t Mtime,
                   int ReplicatedOp, ViceStoreId *StoreId, PDirInode *CowInode,
                   int *blocks, RPC2_Integer *vsptr)
{
    Perform_RR(client, VSGVolnum, volptr, dirvptr, vptr,
               Name, Mtime, ReplicatedOp, StoreId, CowInode, blocks, vsptr);
}


int PerformLink(ClientEntry *client, VolumeId VSGVolnum, Volume *volptr,
                 Vnode *dirvptr, Vnode *vptr, char *Name, Date_t Mtime,
                 int ReplicatedOp, ViceStoreId *StoreId, PDirInode *CowInode,
                 int *blocks, RPC2_Integer *vsptr)
{
    return Perform_CLMS(client, VSGVolnum, volptr, dirvptr, vptr, CLMS_Link,
			Name, 0, 0, Mtime, 0, ReplicatedOp, StoreId, CowInode,
			blocks, vsptr);
}


void PerformRename(ClientEntry *client, VolumeId VSGVolnum, Volume *volptr,
                   Vnode *sd_vptr, Vnode *td_vptr, Vnode *s_vptr, Vnode *t_vptr,
                   char *OldName, char *NewName, Date_t Mtime,
                   int ReplicatedOp, ViceStoreId *StoreId, PDirInode *sd_CowInode,
                   PDirInode *td_CowInode, PDirInode *s_CowInode, int *nblocks,
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
	    *sd_CowInode = sd_vptr->disk.node.dirNode;
	    CopyOnWrite(sd_vptr, volptr);
    }
    sd_dh = VN_SetDirHandle(sd_vptr);

    PDirHandle td_dh;
    if (!SameParent ) {
	    if (td_vptr->disk.cloned) {
		    *td_CowInode = td_vptr->disk.node.dirNode;
		    CopyOnWrite(td_vptr, volptr);
	    }
	    td_dh = VN_SetDirHandle(td_vptr);
    } else {
	    td_dh = sd_dh;
    }

    PDirHandle s_dh = NULL;
    if (s_vptr->disk.type == vDirectory) {
	    if ( s_vptr->disk.cloned) {
		    *s_CowInode = s_vptr->disk.node.dirNode;
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
    will protect us, but it is worrying */
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
	ViceFid fids[MAXFIDS];
	memset((void *)fids, 0, (int) (MAXFIDS * sizeof(ViceFid)));
	fids[0] = SDid;
	if (!SameParent) fids[1] = TDid;
	fids[2] = SFid;
	if (TargetExists && !t_vptr->delete_me) fids[3] = TFid;
	CopPendingMan->add(new cpent(StoreId, fids));
    }
}


int PerformMkdir(ClientEntry *client, VolumeId VSGVolnum, Volume *volptr,
                  Vnode *dirvptr, Vnode *vptr, char *Name, Date_t Mtime, RPC2_Unsigned Mode,
                  int ReplicatedOp, ViceStoreId *StoreId, PDirInode *CowInode,
                  int *blocks, RPC2_Integer *vsptr)
{
    return Perform_CLMS(client, VSGVolnum, volptr, dirvptr, vptr, CLMS_MakeDir,
			Name, 0, 0, Mtime, Mode, ReplicatedOp, StoreId,
			CowInode, blocks, vsptr);
}


void PerformRmdir(ClientEntry *client, VolumeId VSGVolnum, Volume *volptr,
                  Vnode *dirvptr, Vnode *vptr, char *Name, Date_t Mtime,
                  int ReplicatedOp, ViceStoreId *StoreId, PDirInode *CowInode,
                  int *blocks, RPC2_Integer *vsptr)
{
    Perform_RR(client, VSGVolnum, volptr, dirvptr, vptr,
               Name, Mtime, ReplicatedOp, StoreId, CowInode, blocks, vsptr);
}


int PerformSymlink(ClientEntry *client, VolumeId VSGVolnum, Volume *volptr,
                    Vnode *dirvptr, Vnode *vptr, char *Name, Inode newinode,
                    RPC2_Unsigned Length, Date_t Mtime, RPC2_Unsigned Mode,
                    int ReplicatedOp, ViceStoreId *StoreId, PDirInode *CowInode,
                    int *blocks, RPC2_Integer *vsptr)
{
    return Perform_CLMS(client, VSGVolnum, volptr, dirvptr, vptr, CLMS_SymLink,
			Name, newinode, Length, Mtime, Mode, ReplicatedOp,
			StoreId, CowInode, blocks, vsptr);
}


/*
  Perform_CLMS: Perform the create, link, mkdir or  
  symlink operation on a VM copy of the object.
*/

static int Perform_CLMS(ClientEntry *client, VolumeId VSGVolnum,
                         Volume *volptr, Vnode *dirvptr, Vnode *vptr,
                         CLMS_Op opcode, char *Name, Inode newinode,
                         RPC2_Unsigned Length, Date_t Mtime, RPC2_Unsigned Mode,
                         int ReplicatedOp, ViceStoreId *StoreId, PDirInode *CowInode,
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
    /* XXX Is the following callback really needed? the contents of the object
     * do change, however it's only the linkcount in the metadata which isn't
     * really critical --JH */
    if (opcode == CLMS_Link)
	    CodaBreakCallBack((client ? client->VenusId : 0), &Fid, VSGVolnum);

    /* CLMS invokes COW! */
    PDirHandle dh;
    if (dirvptr->disk.cloned) {
	    *CowInode = dirvptr->disk.node.dirNode;
	    CopyOnWrite(dirvptr, volptr);
    }
    dh = VN_SetDirHandle(dirvptr);

    /* Add the name to the parent. */
    error = DH_Create(dh, Name, &Fid);
    if ( error ) {
	    eprint("Create returns %d on %s %s", error, Name, FID_(&Fid));
	    VN_PutDirHandle(dirvptr);
	    return error;
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
    dirvptr->disk.dataVersion++;

    /* If we are called from resolution (client == NULL), the mtime and author
     * fields are already set correctly */
    if (client) {
	dirvptr->disk.unixModifyTime = Mtime;
	dirvptr->disk.author = client->Id;
    }

    if (ReplicatedOp) 
	NewCOP1Update(volptr, dirvptr, StoreId, vsptr);

    /* Initialize/Update the child vnode. */
    switch(opcode) {
	case CLMS_Create:
	    vptr->disk.node.inodeNumber = 0;
	    vptr->disk.linkCount = 1;
	    vptr->disk.length = 0;
	    vptr->disk.unixModifyTime = Mtime;
	    /* If resolving, inherit from the parent */
	    vptr->disk.author = client ? client->Id : dirvptr->disk.author;
	    vptr->disk.owner = client ? client->Id : dirvptr->disk.owner;
	    vptr->disk.modeBits = Mode;
	    vptr->disk.vparent = Did.Vnode;
	    vptr->disk.uparent = Did.Unique;
	    vptr->disk.dataVersion = 0;
	    InitVV(&Vnode_vv(vptr));
	    break;

	case CLMS_Link:
	    vptr->disk.linkCount++;
	    /* If resolving, inherit from the parent */
	    vptr->disk.author = client ? client->Id : dirvptr->disk.author;
	    break;

	case CLMS_MakeDir:
	    PDirHandle cdh;
	    vptr->disk.node.dirNode = NULL;
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
	    /* If resolving, inherit from the parent */
	    vptr->disk.author = client ? client->Id : dirvptr->disk.author;
	    vptr->disk.owner = client ? client->Id : dirvptr->disk.owner;
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

		memcpy(newACL, aCL, aCLSize);
	    }
	    break;

	case CLMS_SymLink:
	    vptr->disk.node.inodeNumber = newinode;
	    vptr->disk.linkCount = 1;
	    vptr->disk.length = Length;
	    vptr->disk.unixModifyTime = Mtime;
	    /* If resolving, inherit from the parent */
	    vptr->disk.author = client ? client->Id : dirvptr->disk.author;
	    vptr->disk.owner = client ? client->Id : dirvptr->disk.owner;

	    /* Symlinks should have their modebits set to 0777, except for
	     * special mountlinks which can only be created by members of
	     * system:administrators */
	    vptr->disk.modeBits = SystemUser(client) ? Mode : 0777;
	    vptr->disk.vparent = Did.Vnode;
	    vptr->disk.uparent = Did.Unique;
	    vptr->disk.dataVersion = 1;
	    InitVV(&Vnode_vv(vptr));
	    break;
    }
    if (ReplicatedOp) 
	NewCOP1Update(volptr, vptr, StoreId, vsptr);

    /* Await COP2 message. */
    if (ReplicatedOp) {
	ViceFid fids[MAXFIDS];
	memset((void *)fids, 0, (int) (MAXFIDS * sizeof(ViceFid)));
	fids[0] = Did;
	fids[1] = Fid;
	CopPendingMan->add(new cpent(StoreId, fids));
    }
    return 0;
}


static void Perform_RR(ClientEntry *client, VolumeId VSGVolnum, Volume *volptr,
                       Vnode *dirvptr, Vnode *vptr, char *Name, Date_t Mtime,
                       int ReplicatedOp, ViceStoreId *StoreId, PDirInode *CowInode,
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
	    *CowInode = dirvptr->disk.node.dirNode;
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
	    memset((void *)fids, 0, (MAXFIDS * sizeof(ViceFid)));
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

#endif /* _TIMEPUTOBJS_ */

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
    START_NSCTIMING(PutObjects_Transaction);
#endif

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

				SLog(5, "--DC: %s ct: %d\n", 
				     FID_(&v->fid), DC_Count(v->vptr->dh));
			    } else {
				SLog(5, "--PO: d_inodemod and !DC_Dirty %s",
				     FID_(&v->fid));
			    }
			    VN_PutDirHandle(v->vptr);
			}
		    }
		    
		    if (AllowResolution && volptr && V_RVMResOn(volptr) &&
			v->vptr->disk.type == vDirectory) {
			
			// log mutation into recoverable volume log
			olist_iterator next(v->rsl);
			rsle *vmle;
			while((vmle = (rsle *)next())) {
			    if (!errorCode) {
				SLog(9, "PutObjects: Appending recoverable log record");
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
			/* node.inodeNumber is initialized to (intptr_t)-1 */
			if (v->vptr->disk.node.dirNode == NEWVNODEINODE) {
			    v->vptr->delete_me = 1;

			    if (v->vptr->disk.type == vDirectory)
				 v->vptr->disk.node.dirNode = NULL;
			    else v->vptr->disk.node.inodeNumber = 0;
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

	// for logs that have been truncated/purged deallocated entries in vm
	// bitmap should be done after transaction commits but here we are
	// asserting if Transaction end status is not success
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
#endif

    /* Post-transaction: handle inodes and clean-up the vlist. */
START_TIMING(PutObjects_Inodes); 
#ifdef _TIMEPUTOBJS_
START_NSC_TIMING(PutObjects_Inodes);
#endif

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
			SLog(3, "PutObjects: truncating (%s, %d, %d)",
			     FID_(&v->fid), v->f_tinode, v->f_tlength);

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
END_TIMING(PutObjects_Inodes);
#ifdef _TIMEPUTOBJS_
END_NSC_TIMING(PutObjects_Inodes);
#endif

    SLog(10, "PutObjects: returning %s", ViceErrorMsg(0));
}

