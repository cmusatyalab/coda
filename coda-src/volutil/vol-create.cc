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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/volutil/vol-create.cc,v 4.12 1998/11/25 19:23:37 braam Exp $";
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


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>
#include <sys/signal.h>
#include <unistd.h>
#include <stdlib.h>

#include <lwp.h>
#include <lock.h>
#include <rpc2.h>
#include <util.h>
#include <rvmlib.h>
#include <codadir.h>

#include <partition.h>

#include <vice.h>
#include <volutil.h>
#include <prs_fs.h>
#include <prs.h>
#ifdef __cplusplus
}
#endif __cplusplus

#include <voltypes.h>
#include <cvnode.h>
#include <volume.h>
#include <errors.h>
#include <viceinode.h>
#include <vutil.h>
#include <index.h>
#include <recov.h>
#include <codadir.h>
#include <camprivate.h>
#include <logalloc.h>
#include <reslog.h>
#include <coda_globals.h>
#include <recov_vollog.h>
#include "volutil.private.h"

Error error;
extern void PrintVnode(FILE *outfile, VnodeDiskObject *vnode, VnodeId vnodeNumber);
static int ViceCreateRoot(Volume *vp);

/* Create a new volume (readwrite or replicated). Invoked through rpc2
   by volume utility. */
/*
  S_VolCreate: Service request to create a volume
  Note: Puneet envisaged this having an extra parameter to set
        the resolution flag (RPC2_Integer resflag)
*/

long S_VolCreate(RPC2_Handle rpcid, RPC2_String formal_partition,
		 RPC2_String formal_volname, VolumeId *volid, 
		 RPC2_Integer repvol,
		 VolumeId grpId) 
{
	VolumeId volumeId = 0;
	VolumeId parentId = 0;
	Volume *vp = NULL;
	int status = 0;    /* transaction status variable */
	int rc = 0;
	int volidx;
	ProgramType *pt;
	int resflag = RVMRES;

	/* To keep C++ 2.0 happy */
	char *partition = (char *)formal_partition;
	char *volname = (char *)formal_volname;    

	error = 0;

	CODA_ASSERT(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);

	VLog(9, "Entering S_VolCreate: rpcid = %d, partition = %s," 
	     "volname = %s, volumeid = %x, repvol = %d, grpid = %x",
	     rpcid, partition, volname, volid ? *volid : 0, repvol, grpId);
	RVMLIB_BEGIN_TRANSACTION(restore);

	rc = VInitVolUtil(volumeUtility);
	if (rc != 0) {
		rvmlib_abort(rc);
		status = rc;
		goto exit;
	}

	/* Use a new volumeId only if the user didn't specify any */
	if (!volid  || !(*volid) )
		volumeId = VAllocateVolumeId(&error);
	else {
		volumeId = *volid;
		/* check that volume id is legal */
		if (volumeId > VGetMaxVolumeId()) {
			SLog(0, "Warning: %x is > MaxVolID; setting MaxVolID to %x\n",
			     volumeId, volumeId);
			VSetMaxVolumeId(volumeId);
		}
	}
	VLog(9, "VolCreate: VAllocateVolumeId returns %x", volumeId);
	if (error) {
		VLog(0, "Unable to allocate a volume number; not created");
		rvmlib_abort(VNOVOL);
		status = VNOVOL;
		goto exit;
	}

	/* we are creating a readwrite (or replicated) volume */
	parentId = volumeId;    

	if (repvol && grpId == 0) {
		VLog(0, "S_VolCreate: can't create replicated volume without group id");
		rvmlib_abort(VFAIL);
		status = VFAIL;
		goto exit;
	}

    /* If we are creating a replicated volume, pass along group id */
	vp = VCreateVolume(&error, partition, volumeId, parentId, 
			   repvol?grpId:0, readwriteVolume, 
			   repvol? resflag : 0);
	if (error) {
		VLog(0, "Unable to create the volume; aborted");
		rvmlib_abort(VNOVOL);
		status = VNOVOL;
		goto exit;
	}
	V_uniquifier(vp) = 1;
	V_creationDate(vp) = V_copyDate(vp);
	V_inService(vp) = V_blessed(vp) = 1;
	V_type(vp) = readwriteVolume;
	AssignVolumeName(&V_disk(vp), volname, 0);

	/* could probably begin transaction here instead of at beginning */
	/* must include both ViceCreateRoot and 
	   VUpdateVolume for vv atomicity */
	ViceCreateRoot(vp);
	V_destroyMe(vp) = V_needsSalvaged(vp) = 0;
	V_linkcount(vp) = 1;
	volidx = V_volumeindex(vp);
	VUpdateVolume(&error, vp);
	VDetachVolume(&error, vp);	/* Allow file server to grab it */
	CODA_ASSERT(error == 0);
	RVMLIB_END_TRANSACTION(flush, &(status));
 exit: 

	/* to make sure that rvm records are getting flushed 
	   - to find this bug */
	CODA_ASSERT(rvm_flush() == RVM_SUCCESS);
	VDisconnectFS();
	if (status == 0) {
		VLog(0, "create: volume %x (%s) created", volumeId, volname);
		*volid = volumeId;	    /* set value of out parameter */
		if (SRV_RVM(VolumeList[volidx]).data.volumeInfo)
			if (SRV_RVM(VolumeList[volidx]).data.volumeInfo->maxlogentries)
				LogStore[volidx] = new PMemMgr(sizeof(rlent), 0, volidx,
							       SRV_RVM(VolumeList[volidx]).data.volumeInfo->maxlogentries);
			else
				LogStore[volidx] = new PMemMgr(sizeof(rlent), 0, volidx, MAXLOGSIZE);
		else
			CODA_ASSERT(0);
	}
	else {
		VLog(0, "create: volume creation failed for volume %x", volumeId);
		VLog(0, "status = (%d) ", status);
	}
	return(status?status:rc);
}

/* Adapted from the file server; create a root directory for */
/* a new volume */
static int ViceCreateRoot(Volume *vp)
{
    PDirHandle dir;
    AL_AccessList * ACL;
    ViceFid	did;
    Inode inodeNumber;
    char buf[SIZEOF_LARGEDISKVNODE];
    struct VnodeDiskObject *vnode = (struct VnodeDiskObject *) buf;
    struct VnodeClassInfo *vcp = &VnodeClassInfo_Array[vLarge];
    char buf3[sizeof(Vnode)];
    Vnode *vn = (Vnode *)buf3;
    vindex v_index(V_id(vp), vLarge, vp->device, SIZEOF_LARGEDISKVNODE);

    bzero((void *)vn, sizeof(Vnode));
    bzero((char *)vnode, SIZEOF_LARGEDISKVNODE);    

    /* SetSalvageDirHandle(&dir, V_id(vp), vp->device, 0); */
    dir = VN_SetDirHandle(vn);

    did.Vnode = (VnodeId)bitNumberToVnodeNumber(0, vLarge);
    VLog(29, "ViceCreateRoot: did.Vnode = %d", did.Vnode);
    did.Unique = 1;

    /* set up the physical directory */
    CODA_ASSERT(!(DH_MakeDir(dir, &did, &did)));

    /* build a single entry ACL that gives all rights to everyone */
    ACL = VVnodeDiskACL(vnode);
    ACL->MySize = sizeof(AL_AccessList);
    ACL->Version = AL_ALISTVERSION;
    ACL->TotalNoOfEntries = 2;
    ACL->PlusEntriesInUse = 2;
    ACL->MinusEntriesInUse = 0;
    ACL->ActualEntries[0].Id = PRS_SYSTEMADMINID;
    ACL->ActualEntries[0].Rights = PRSFS_ALL;
    ACL->ActualEntries[1].Id = PRS_ANYUSERID;
    ACL->ActualEntries[1].Rights = PRSFS_READ | PRSFS_LOOKUP;

    /* set up vnode info */
    vnode->type = vDirectory;
    vnode->cloned = 0;
    vnode->modeBits = 0777;
    vnode->linkCount = 2;
    vnode->length = DH_Length(dir);
    vnode->uniquifier = 1;
    V_uniquifier(vp) = vnode->uniquifier+1;
    vnode->dataVersion = 1;
    vnode->inodeNumber = 0;
    /* we need to simultaneously update vv in VolumeDiskData struct ***/
    InitVV(&vnode->versionvector);
    vnode->vol_index = vp->vol_index;	/* index of vnode's volume in recoverable storage */
    vnode->unixModifyTime = vnode->serverModifyTime = V_creationDate(vp);
    vnode->author = 0;
    vnode->owner = 0;
    vnode->vparent = 0;
    vnode->uparent = 0;
    vnode->vnodeMagic = vcp->magic;

    /* write out the directory in rvm - that will create the inode */
    /* set up appropriate fields in a vnode for DCommit */
    vn->changed = 1;
    vn->delete_me = 0;
    vn->vnodeNumber = (VnodeId)bitNumberToVnodeNumber(0, vLarge);
    vn->volumePtr = vp;
    memcpy((const void *)&vn->disk, (void *)vnode, sizeof(VnodeDiskObject));
    VN_DCommit(vn);   
    DC_SetDirty(vn->dh, 0);
    VN_PutDirHandle(vn);

    memcpy((void *) vnode, (const void *)&(vn->disk), sizeof(VnodeDiskObject));
    CODA_ASSERT(vnode->inodeNumber != 0);
    CODA_ASSERT(vnode->uniquifier == 1);

    /* create the resolution log for this vnode if rvm resolution is
       turned on */
    if (AllowResolution && V_RVMResOn(vp)) {
	VLog(0, "Creating new log for root vnode\n");
	CreateRootLog(vp, vn);
	vnode->log = VnLog(vn);
    }
    /* write out the new vnode */
    if (VolDebugLevel >= 9) {
	PrintVnode(stdout, vnode, bitNumberToVnodeNumber(0, vLarge));
    }
    CODA_ASSERT(v_index.oput((bit32) 0, 1, vnode) == 0);

    V_diskused(vp) = nBlocks(vnode->length);
    
    return (1);
}



