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


#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>
#include <sys/signal.h>
#include <unistd.h>
#include <stdlib.h>
#include "coda_string.h"

#include <lwp/lwp.h>
#include <lwp/lock.h>
#include <rpc2/rpc2.h>
#include <rpc2/errors.h>
#include <util.h>
#include <rvmlib.h>
#include <codadir.h>

#include <partition.h>

#include <vice.h>
#include <volutil.h>
#include <prs.h>

#ifdef __cplusplus
}
#endif

#include <voltypes.h>
#include <cvnode.h>
#include <volume.h>
#include <viceinode.h>
#include <vutil.h>
#include <index.h>
#include <recov.h>
#include <codadir.h>
#include <camprivate.h>
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
	rvm_return_t status = RVM_SUCCESS;    /* transaction status variable */
	int rc = 0;
	int volidx;
	ProgramType *pt;
	int resflag = repvol ? RVMRES : 0;
        int rvmlogsize = 4096; /* when resolution is enabled, default to 4k
                                  log entries */

	/* To keep C++ 2.0 happy */
	char *partition = (char *)formal_partition;
	char *volname = (char *)formal_volname;    
	char *rock;

	error = 0;

	CODA_ASSERT(LWP_GetRock(FSTAG, &rock) == LWP_SUCCESS);
	pt = (ProgramType *)rock;

	VLog(9, "Entering S_VolCreate: rpcid = %d, partition = %s," 
	     "volname = %s, volumeid = %x, repvol = %d, grpid = %x",
	     rpcid, partition, volname, volid ? *volid : 0, repvol, grpId);
	rvmlib_begin_transaction(restore);

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
			   repvol ? grpId : 0, readwriteVolume, 
			   resflag ? rvmlogsize : 0);
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
	/* must include both ViceCreateRoot and VUpdateVolume for vv atomicity */
	if (!ViceCreateRoot(vp)) {
		VLog(0, "Unable to create the volume root; aborted");
		rvmlib_abort(VNOVOL);
		status = VNOVOL;
		goto exit;
	}

	V_destroyMe(vp) = V_needsSalvaged(vp) = 0;
	V_linkcount(vp) = 1;
	volidx = V_volumeindex(vp);
	VUpdateVolume(&error, vp);
	VDetachVolume(&error, vp);	/* Allow file server to grab it */
	CODA_ASSERT(error == 0);
	rvmlib_end_transaction(flush, &(status));
 exit: 

	/* to make sure that rvm records are getting flushed 
	   - to find this bug */
	CODA_ASSERT(rvm_flush() == RVM_SUCCESS);
	VDisconnectFS();
	if (status == 0) {
		VLog(0, "create: volume %x (%s) created", volumeId, volname);
		*volid = volumeId;	    /* set value of out parameter */
	}
	else {
		VLog(0, "create: volume creation failed for volume %x", volumeId);
		VLog(0, "status = (%d) ", status);
	}
	return(status?status:rc);
}

/* Adapted from the file server; create a root directory for a new volume */
static int ViceCreateRoot(Volume *vp)
{
    PDirHandle dir;
    AL_AccessList * ACL;
    ViceFid	did;
    char buf[SIZEOF_LARGEDISKVNODE];
    VnodeDiskObject *vnode = (VnodeDiskObject *) buf;
    struct VnodeClassInfo *vcp = &VnodeClassInfo_Array[vLarge];
    char buf3[sizeof(Vnode)];
    Vnode *vn = (Vnode *)buf3;
    vindex v_index(V_id(vp), vLarge, V_device(vp), SIZEOF_LARGEDISKVNODE);
    int adminid, anyuserid, adminindex;

    if (AL_NameToId(PRS_ADMINGROUP, &adminid) == -1) {
	fprintf(stderr, "Cannot find group id for '" PRS_ADMINGROUP
		        "', check pdb databases!\n");
	return 0;
    }
    if (AL_NameToId(PRS_ANYUSERGROUP, &anyuserid) == -1) {
	fprintf(stderr, "Cannot find group id for '" PRS_ANYUSERGROUP
		        "', check pdb databases!\n");
	return 0;
    }

    memset((void *)vn, 0, sizeof(Vnode));
    memset((char *)vnode, 0, SIZEOF_LARGEDISKVNODE);    

    /* SetSalvageDirHandle(&dir, V_id(vp), vp->device, 0); */
    dir = VN_SetDirHandle(vn);

    did.Vnode = (VnodeId)bitNumberToVnodeNumber(0, vLarge);
    VLog(29, "ViceCreateRoot: did.Vnode = %d", did.Vnode);
    did.Unique = 1;

    /* set up the physical directory */
    CODA_ASSERT(!(DH_MakeDir(dir, &did, &did)));

    /* build a two entry ACL that gives all rights to System:Administrators
     * and read-lookup rights to System:AnyUser */
    ACL = VVnodeDiskACL(vnode);
    ACL->MySize = sizeof(AL_AccessList);
    ACL->Version = AL_ALISTVERSION;
    ACL->TotalNoOfEntries = 2;
    ACL->PlusEntriesInUse = 2;
    ACL->MinusEntriesInUse = 0;

    /* ACL's are assumed to be going from lower to higher id number. This
     * makes the AL_CheckRights function more efficient. However, we now have
     * to insert the admin and anyuser ACL's in the correct order. */
    adminindex = adminid < anyuserid ? 0 : 1;
    ACL->ActualEntries[adminindex].Id = adminid;
    ACL->ActualEntries[adminindex].Rights = PRSFS_ALL;
    ACL->ActualEntries[1 - adminindex].Id = anyuserid;
    ACL->ActualEntries[1 - adminindex].Rights = PRSFS_READ | PRSFS_LOOKUP;

    /* set up vnode info */
    vnode->type = vDirectory;
    vnode->cloned = 0;
    vnode->modeBits = 0755;
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
    memcpy((void *)&vn->disk, (const void *)vnode, sizeof(VnodeDiskObject));
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



