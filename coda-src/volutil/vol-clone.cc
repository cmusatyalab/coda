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


/********************************
 * vol-clone.c			*
 * Clone a volume		*
 ********************************/

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

#include <struct.h>
#include <lwp/lwp.h>
#include <lwp/lock.h>
#include <rpc2/rpc2.h>
#include <inodeops.h>
#include <util.h>
#include <rvmlib.h>
#include <codadir.h>

#include <volutil.h>
#include <vice.h>
#include <rpc2/errors.h>
#include <partition.h>

#ifdef __cplusplus
}
#endif

#include <voltypes.h>
#include <cvnode.h>
#include <volume.h>
#include <srv.h>
#include <viceinode.h>
#include <vutil.h>
#include <index.h>
#include <recov.h>
#include <camprivate.h>
#include <coda_globals.h>
#include <vldb.h>
#include <rec_smolist.h>
#include <inconsist.h>
    
extern void PollAndYield();

static void VUCloneIndex(Error *, Volume *, Volume *, VnodeClass);

int CloneVnode(Volume *rwVp, Volume *cloneVp, int vnodeIndex, rec_smolist *vlist,
	       VnodeDiskObject *rwVnode, VnodeClass vclass);

/*
    S_VolClone: Create a new readonly clone of a volume.
*/
/* ovolid: Volume Id of the volume to be cloned 
 * cloneId: OUT Parameter; Id of cloned volume returned in that param.
 */
long S_VolClone(RPC2_Handle rpcid, RPC2_Unsigned formal_ovolid,
		RPC2_String formal_newname, RPC2_Unsigned *formal_cloneId)
{
    /* To keep C++ 2.0 happy */
    VolumeId ovolid = (VolumeId)formal_ovolid;
    VolumeId *cloneId = (VolumeId *)formal_cloneId;

    VolumeId	newId, originalId = ovolid;
    Volume  *originalvp = NULL;
    Volume  *newvp = NULL;
    struct vldb *vldp = NULL;
    char *newvolname = (char *)formal_newname;

    rvm_return_t status = RVM_SUCCESS;
    int rc = 0;
    Error error;

    VLog(9, "Entering S_VolClone: rpcid = %d, OldVolume = %x", rpcid, ovolid);
    rc = VInitVolUtil(volumeUtility);
    if (rc != 0){
	VLog(0, "S_VolClone: VInitVolUtil failed!");
	return rc;
    }
    
    if (newvolname[0]){		/* It's '\000' if user didn't specify a name */
	VLog(0, "VolClone: Looking up %s in VLDB", newvolname);
	/* check if volume with requested new name already exists */
	vldp = VLDBLookup(newvolname);

	if (vldp != NULL){
	    VLog(0, "S_VolClone: Volume with new volume name already exists");
	    VLog(0, "S_VolClone: the vldb record looks like ");
	    VLog(0, "key = %s \t voltype = %d \n \tnServers = %d\tvolid = %x", 
		vldp->key, vldp->volumeType, vldp->nServers, vldp->volumeId[0]);
	    VDisconnectFS();
	    return -123;	/* Picked a random error code see vol-restore.c */
	}
    }

    VLog(29, "Vol_Clone: Going to call VGetVolume");
    originalvp = VGetVolume(&error, originalId);
    if (error) {
	VLog(0, "S_VolClone: failure attaching volume %x", originalId);
	if (originalvp) {
	    rvmlib_begin_transaction(restore);
		VPutVolume(originalvp);	/* Do these need transactions? */
	    rvmlib_end_transaction(flush, &(status));
	    CODA_ASSERT(status == 0);
	}
	VDisconnectFS();
	return error;
    }
    
    if (V_type(originalvp) != readonlyVolume && 
	V_type(originalvp) != readwriteVolume){
	VLog(0, "S_VolClone: volume to be cloned must be RW or RO; aborting");
	VPutVolume(originalvp);
	VDisconnectFS();
	return VFAIL;
    }
    if (V_type(originalvp) == readwriteVolume){
	VLog(29, "Cloning RW volume - trying to lock ");
	/* lock the whole volume for the duration of the clone */
	if (V_VolLock(originalvp).IPAddress){
	    VLog(0, "S_VolClone: old volume already locked; Aborting... ");
	    rvmlib_begin_transaction(restore);
	    VPutVolume(originalvp);	/* Do these need transactions? */
	    rvmlib_end_transaction(flush, &(status));
	    CODA_ASSERT(status == 0);
	    VDisconnectFS();
	    return EWOULDBLOCK;
	}

	/* Should this be fixed to use the correct IPAddress, etc? */	
	V_VolLock(originalvp).IPAddress = (unsigned int)1;

	VLog(9, "S_VolClone: Goint to obtain write lock on old volume");
	ObtainWriteLock(&(V_VolLock(originalvp).VolumeLock));
	VLog(9, "S_VolClone:Obtained write lock on old volume");
    }

    rvmlib_begin_transaction(restore);
    newId = VAllocateVolumeId(&error);
    VLog(9, "VolClone: VAllocateVolumeId returns %x", newId);
    if (error){
	VLog(0, "S_VolClone:Unable to allocate volume number;" 
	     "volume not cloned");
	VPutVolume(originalvp);
	rvmlib_abort(VNOVOL);
	status = VNOVOL;
	goto exit1;
    }
    newvp = VCreateVolume(&error, V_partname(originalvp), newId, 
			  originalId, 0, readonlyVolume);
    if (error) {
	VLog(0, "S_VolClone:Unable to create the volume; aborted");
	VPutVolume(originalvp);
	rvmlib_abort(VNOVOL);
	status = VNOVOL;
	goto exit1;
    }

    V_blessed(newvp) = 0;
    VUpdateVolume(&error, newvp);
    if (error) {
	VLog(0, "S_VolClone: Volume %x can't be unblessed!", newId);
	VPutVolume(originalvp);
	rvmlib_abort(VFAIL);
	status = VFAIL;
	goto exit1;
    }
    
    rvmlib_end_transaction(flush, &(status));
 exit1:
    if (status != 0) {
	VLog(0, "S_VolClone: volume creation failed for volume %x", originalId);
	V_VolLock(originalvp).IPAddress = 0;
	ReleaseWriteLock(&(V_VolLock(originalvp).VolumeLock));
	VDisconnectFS();
	return status;
    }
    
    VLog(9, "S_VolClone: Created Volume %x; going to clone from %x", newId, originalId);

    VUCloneVolume( &error, originalvp, newvp);
    if (error){
	VLog(0, "S_VolClone: Error while cloning volume %x -> %x",
	    originalId, newId);
	V_VolLock(originalvp).IPAddress = 0;
	ReleaseWriteLock(&(V_VolLock(originalvp).VolumeLock));
	rvmlib_begin_transaction(restore);
	    VPutVolume(originalvp);	/* Do these need transactions? */
	rvmlib_end_transaction(flush, &(status));
	CODA_ASSERT(status == 0);
	VDisconnectFS();
	return error;
    }
    
    if (V_type(originalvp) == readwriteVolume)
	V_cloneId(originalvp) = newId;
    
    /* assign a name to the clone. if the user requested a name then
     * that is used.  otherwise the new name is obtained by appending
     * ".readonly" to the original name.  If it already has
     * ".readonly" then it doesnt change */

    if (newvolname){
	char name[V_MAXVOLNAMELEN];
	strncpy(name, newvolname, V_MAXVOLNAMELEN);
	char *dot;
	dot = strrchr(name, '.');
	if (dot && !strcmp(dot, ".readonly"))
	    AssignVolumeName(&V_disk(newvp), name, ".readonly");
	else
	    AssignVolumeName(&V_disk(newvp), name, NULL);
    }
    else
	AssignVolumeName(&V_disk(newvp), V_name(originalvp), ".readonly");
    V_type(newvp) = readonlyVolume;
    V_creationDate(newvp) = V_copyDate(newvp);
    ClearVolumeStats(&V_disk(newvp));
    V_destroyMe(newvp) = 0;
    V_blessed(newvp) = 1;

    rvmlib_begin_transaction(restore);
    VUpdateVolume(&error, newvp);
    VDetachVolume(&error, newvp);
    VUpdateVolume(&error, originalvp);
    CODA_ASSERT(error == 0);
    V_VolLock(originalvp).IPAddress = 0;
    ReleaseWriteLock(&(V_VolLock(originalvp).VolumeLock)); 
    VPutVolume(originalvp);
    rvmlib_end_transaction(flush, &(status));
    VDisconnectFS();

    if (status == 0) {
	    VLog(0, "S_VolClone: volume %x cloned", originalId);
	    *cloneId = newId;	    /* set value of out parameter */
    } else {
	    VLog(0, "S_VolClone: volume clone failed for volume %x", 
		 originalId);
    }

    return status;
}

/* Clones the Volume contents from rwVp to cloneVp */
void VUCloneVolume(Error *error, Volume *rwVp, Volume *cloneVp)
{
    *error = 0;
    VUCloneIndex(error, rwVp, cloneVp, vLarge);
    CODA_ASSERT(*error == 0);
    VUCloneIndex(error, rwVp, cloneVp, vSmall);
    CODA_ASSERT(*error == 0);
    CopyVolumeHeader(&V_disk(rwVp), &V_disk(cloneVp));	/* Doesn't use RVM */
}

int MaxVnodesPerTransaction = 8;

static void VUCloneIndex(Error *error, Volume *rwVp, Volume *cloneVp, VnodeClass vclass)
{
    rvm_return_t status;
    unsigned int i;
    bit32 nvnodes = 0;
    bit32 vnlistSize = 0;
    rec_smolist *rvlist;
    int ovolInd = V_volumeindex(rwVp);
    int cvolInd = V_volumeindex(cloneVp);
    
    if (vclass == vSmall){
	nvnodes = SRV_RVM(VolumeList[ovolInd]).data.nsmallvnodes;
	vnlistSize = SRV_RVM(VolumeList[ovolInd]).data.nsmallLists;
    }
    else if (vclass == vLarge){
	nvnodes = SRV_RVM(VolumeList[ovolInd]).data.nlargevnodes;
	vnlistSize = SRV_RVM(VolumeList[ovolInd]).data.nlargeLists;
    }
    else 
	CODA_ASSERT(0);

    rvmlib_begin_transaction(restore);
    
    /* free vnodes allocated earlier by VCreateVolume. */
    bit32 onVnodes, onLists;
    rec_smolist *ovList;
    
    if (vclass == vSmall) {
	onVnodes = SRV_RVM(VolumeList[cvolInd]).data.nsmallvnodes;
	ovList = SRV_RVM(VolumeList[cvolInd]).data.smallVnodeLists;
	onLists = SRV_RVM(VolumeList[cvolInd]).data.nsmallLists;
    } else { /* vclass == vLarge */
	onVnodes = SRV_RVM(VolumeList[cvolInd]).data.nlargevnodes;
	ovList = SRV_RVM(VolumeList[cvolInd]).data.largeVnodeLists;
	onLists = SRV_RVM(VolumeList[cvolInd]).data.nlargeLists;
    }

    CODA_ASSERT(ovList);	/* How can it not have a list? */
    for (i = 0; (i < onLists && onVnodes > 0); i++){
	rec_smolink *p;
	while( (p = ovList[i].get()) ) {
	    VnodeDiskObject *vdo;
	    vdo = strbase(VnodeDiskObject, p, nextvn);
	    rvmlib_rec_free((char *)vdo);
	    onVnodes--;
	}
    }
    rvmlib_rec_free((char *)ovList);

    /* initialize a new list of vnodes */
    rvlist = (rec_smolist *)(rvmlib_rec_malloc(sizeof(rec_smolist) * vnlistSize));
    rec_smolist *tmpvlist = (rec_smolist *)malloc((int)(sizeof(rec_smolist) * vnlistSize));
    CODA_ASSERT(tmpvlist != 0);
    memset((void *)tmpvlist, 0, (int)(sizeof(rec_smolist) * vnlistSize));
    rvmlib_modify_bytes(rvlist, tmpvlist, sizeof(rec_smolist)*vnlistSize);
    free(tmpvlist);

    /* Store the new list in the volume structure. Do this now so that
     * if an abort happens, already commited vnodes will be scavanged.
     */
    if (vclass == vSmall) {
	RVMLIB_MODIFY(SRV_RVM(VolumeList[cvolInd]).data.smallVnodeLists,rvlist);
	RVMLIB_MODIFY(SRV_RVM(VolumeList[cvolInd]).data.nsmallvnodes, nvnodes);
	RVMLIB_MODIFY(SRV_RVM(VolumeList[cvolInd]).data.nsmallLists,vnlistSize);
    } else { /* vclass == vLarge */
	RVMLIB_MODIFY(SRV_RVM(VolumeList[cvolInd]).data.largeVnodeLists,rvlist);
	RVMLIB_MODIFY(SRV_RVM(VolumeList[cvolInd]).data.nlargevnodes, nvnodes);
	RVMLIB_MODIFY(SRV_RVM(VolumeList[cvolInd]).data.nlargeLists,vnlistSize);
    }

    rvmlib_end_transaction(flush, &(status));
    CODA_ASSERT(status == 0);				/* Never aborts... */
	
    char buf[SIZEOF_LARGEDISKVNODE];
    VnodeDiskObject *vnode = (VnodeDiskObject *)buf;
    struct VnodeClassInfo *vcp = &VnodeClassInfo_Array[vclass];

    vindex vol_index(V_id(rwVp), vclass, V_device(rwVp), vcp->diskSize);
    vindex_iterator vnext(vol_index);

    int vnodeindex;
    int moreVnodes = TRUE;
    *error = 0;
    
    while(moreVnodes) {
	
	rvmlib_begin_transaction(restore);
	for (int count = 0; count < MaxVnodesPerTransaction; count++) {
	    if ((vnodeindex = vnext(vnode)) == -1) {
		moreVnodes = FALSE;
		break;
	    }

	    *error = CloneVnode(rwVp, cloneVp, vnodeindex, rvlist, vnode, vclass);
	    if (*error) {
		    status = *error;
		    rvmlib_abort(VFAIL);
		    goto error;
	    }


	} 
	rvmlib_end_transaction(flush, &(status));
    error:
	if (status != 0) {
	    VLog(0, "CloneIndex: abort for RW %x RO %x",
		   V_id(rwVp), V_id(cloneVp));
	    return;	/* Error is already set... */
	}

	if (moreVnodes) {
	    VLog(9, "Finished %d %s vnode clones.",
		   MaxVnodesPerTransaction, (vclass == vLarge?"Large":"Small"));
	} else {
	    VLog(9, "Finished cloning %s vnodes.",vclass==vLarge?"Large":"Small");
	}

	if (!rvm_no_yield)  /* DEBUG */
	    PollAndYield();

	
    }		/* moreVnodes */
}



/* This must be called from within a transaction! */
/* Create a new vnode in the the new clone volume from the copy of the
 * r/w vnode stored in (vnode). Mark the RW Vnode as cloned.
 */
int CloneVnode(Volume *rwVp, Volume *cloneVp, int vnodeIndex, 
	       rec_smolist *rvlist, VnodeDiskObject *vnode, VnodeClass vclass)
{
    Error error = 0;
    int vnodeNum = bitNumberToVnodeNumber(vnodeIndex, vclass);
    int size=(vclass==vSmall) ? SIZEOF_SMALLDISKVNODE : SIZEOF_LARGEDISKVNODE;
    VnodeDiskObject *vdo = (VnodeDiskObject *) rvmlib_rec_malloc(size);
    int docreate = FALSE;

    VLog(9, "CloneVnode: Cloning %s vnode %x.%x.%x\n",
	   (vclass == vLarge)?"Large":"Small",
	   V_id(rwVp), vnodeNum, vnode->uniquifier);

    memset((void *)&(vnode->nextvn), 0, sizeof(rec_smolink));
    vnode->vol_index = V_volumeindex(cloneVp);
    
    /* update inode */

    /* If RWvnode doesn't have an inode (or is BARREN), create a new inode
     * for the clone. Otherwise increment the reference count on the inode.
     * If the iinc fails, something must have trashed it since the server
     * started (otherwise the rwVnode would be BARREN).
     * Inodes for Large Vnodes are in RVM, so should never disappear.
     */
	
    if (vclass == vLarge) { /* Directory -- no way it can be BARREN */
	int linkcount = DI_Count(vnode->node.dirNode);
	CODA_ASSERT(linkcount > 0);
	DI_Inc(vnode->node.dirNode);
    } else {	/* Small Vnode -- file or symlink. */

	if (vnode->node.inodeNumber == 0) {
	    VLog(0, "CloneVolume: VNODE %d HAD ZERO INODE.\n",vnodeNum);
	    CODA_ASSERT(vnode->type != vNull);
	    docreate = TRUE;
	} else
	    docreate = (int)IsBarren(vnode->versionvector);

	if (docreate) {
	    vnode->node.inodeNumber = icreate(V_device(cloneVp),
					      V_id(rwVp), vnodeNum,
					      vnode->uniquifier, 0);
	    vnode->length = 0;	/* Reset length since we have a new null inode. */
	} else
	    /* Inodes should not disappear while the server is running. */
	    CODA_ASSERT(iinc(V_device(rwVp), vnode->node.inodeNumber,
			     V_parentId(rwVp)) == 0);
    }
    
    /* Mark the RW vnode as cloned, !docreate ==> vnode was cloned. */
    if (VolumeWriteable(rwVp) && !docreate) {

	Vnode *tmp = VGetVnode(&error, rwVp, vnodeNum,
			       vnode->uniquifier, WRITE_LOCK, 1, 1);
	if (error) {
	    VLog(0, "CloneVnode(%s): Error %d getting vnode index %d",
		(vclass == vLarge)?"Large":"Small", error, vnodeIndex);
	    return error;	
	}

	tmp->disk.cloned = 1;
	VPutVnode(&error, tmp);	/* Check error? */
	CODA_ASSERT(error == 0);
    }
    
    /* R/O vnodes should never be marked inconsistent. */
    vnode->versionvector.Flags = 0;
    vnode->cloned = 0;		/* R/O Vnode should not be marked as cloned. */
    
    rvmlib_modify_bytes(vdo, vnode, size);
    rvlist[vnodeIndex].append(&vdo->nextvn);
    return 0;
}
