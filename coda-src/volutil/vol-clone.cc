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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/volutil/vol-clone.cc,v 4.7 1998/08/31 12:23:46 braam Exp $";
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


/********************************
 * vol-clone.c			*
 * Clone a volume		*
 ********************************/

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <sys/time.h>

#include <stdio.h>
#include <sys/signal.h>
#include <unistd.h>
#include <stdlib.h>

#include <struct.h>
#include <lwp.h>
#include <lock.h>
#include <rpc2.h>
#include <inodeops.h>
#include <util.h>
#include <rvmlib.h>
#include <codadir.h>

#include <volutil.h>
#include <vice.h>
#ifdef __cplusplus
}
#endif __cplusplus

#include <voltypes.h>
#include <cvnode.h>
#include <volume.h>
#include <srv.h>
#include <errors.h>
#include <partition.h>
#include <viceinode.h>
#include <vutil.h>
#include <index.h>
#include <recov.h>
#include <codadir.h>
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

    int status = 0;
    int rc = 0;
    Error error;

    LogMsg(9, VolDebugLevel, stdout,
	   "Entering S_VolClone: rpcid = %d, OldVolume = %x", rpcid, ovolid);
    rc = VInitVolUtil(volumeUtility);
    if (rc != 0){
	LogMsg(0, VolDebugLevel, stdout, "S_VolClone: VInitVolUtil failed!");
	return rc;
    }
    
    if (newvolname[0]){		/* It's '\000' if user didn't specify a name */
	LogMsg(0, 0, stdout, "VolClone: Looking up %s in VLDB", newvolname);
	/* check if volume with requested new name already exists */
	vldp = VLDBLookup(newvolname);

	if (vldp != NULL){
	    LogMsg(0, VolDebugLevel, stdout, "S_VolClone: Volume with new volume name already exists");
	    LogMsg(0, VolDebugLevel, stdout, "S_VolClone: the vldb record looks like ");
	    LogMsg(0, VolDebugLevel, stdout, "key = %s \t voltype = %d \n \tnServers = %d\tvolid = %x", 
		vldp->key, vldp->volumeType, vldp->nServers, vldp->volumeId[0]);
	    VDisconnectFS();
	    return -123;	/* Picked a random error code see vol-restore.c */
	}
    }

    LogMsg(29, VolDebugLevel, stdout, "Vol_Clone: Going to call VGetVolume");
    originalvp = VGetVolume(&error, originalId);
    if (error) {
	LogMsg(0, VolDebugLevel, stdout, "S_VolClone: failure attaching volume %x", originalId);
	if (originalvp) {
	    RVMLIB_BEGIN_TRANSACTION(restore)
		VPutVolume(originalvp);	/* Do these need transactions? */
	    RVMLIB_END_TRANSACTION(flush, &(status));
	    assert(status == 0);
	}
	VDisconnectFS();
	return error;
    }
    
    if (V_type(originalvp) != readonlyVolume && V_type(originalvp) != readwriteVolume){
	LogMsg(0, VolDebugLevel, stdout, "S_VolClone: The volume to be cloned must be RW or R/o; aborting");
	VPutVolume(originalvp);
	VDisconnectFS();
	return VFAIL;
    }
    if (V_type(originalvp) == readwriteVolume){
	LogMsg(29, VolDebugLevel, stdout, "Cloning RW volume - trying to lock ");
	/* lock the whole volume for the duration of the clone */
	if (V_VolLock(originalvp).IPAddress){
	    LogMsg(0, VolDebugLevel, stdout, "S_VolClone: old volume already locked; Aborting... ");
	    RVMLIB_BEGIN_TRANSACTION(restore)
		VPutVolume(originalvp);	/* Do these need transactions? */
	    RVMLIB_END_TRANSACTION(flush, &(status));
	    assert(status == 0);
	    VDisconnectFS();
	    return EWOULDBLOCK;
	}

	/* Should this be fixed to use the correct IPAddress, etc? */	
	V_VolLock(originalvp).IPAddress = (unsigned int)1;
	V_VolLock(originalvp).WriteLockType = VolUtil;

	LogMsg(9, VolDebugLevel, stdout, "S_VolClone: Goint to obtain write lock on old volume");
	ObtainWriteLock(&(V_VolLock(originalvp).VolumeLock));
	LogMsg(9, VolDebugLevel, stdout, "S_VolClone:Obtained write lock on old volume");
    }

    RVMLIB_BEGIN_TRANSACTION(restore)
    newId = VAllocateVolumeId(&error);
    LogMsg(9, VolDebugLevel, stdout, "VolClone: VAllocateVolumeId returns %x", newId);
    if (error){
	LogMsg(0, VolDebugLevel, stdout, "S_VolClone:Unable to allocate a volume number; volume not cloned");
	VPutVolume(originalvp);
	rvmlib_abort(VNOVOL);
    }
    newvp = VCreateVolume(&error, V_partname(originalvp), newId, originalId, 0, readonlyVolume);
    if (error) {
	LogMsg(0, VolDebugLevel, stdout, "S_VolClone:Unable to create the volume; aborted");
	VPutVolume(originalvp);
	rvmlib_abort(VNOVOL);
    }

    V_blessed(newvp) = 0;
    VUpdateVolume(&error, newvp);
    if (error) {
	LogMsg(0, VolDebugLevel, stdout, "S_VolClone: Volume %x can't be unblessed!", newId);
	VPutVolume(originalvp);
	rvmlib_abort(VFAIL);
    }
    
    RVMLIB_END_TRANSACTION(flush, &(status));
    if (status != 0) {
	LogMsg(0, VolDebugLevel, stdout, "S_VolClone: volume creation failed for volume %x", originalId);
	V_VolLock(originalvp).IPAddress = 0;
	ReleaseWriteLock(&(V_VolLock(originalvp).VolumeLock));
	VDisconnectFS();
	return status;
    }
    
    LogMsg(9, VolDebugLevel, stdout, "S_VolClone: Created Volume %x; going to clone from %x", newId, originalId);

    VUCloneVolume( &error, originalvp, newvp);
    if (error){
	LogMsg(0, VolDebugLevel, stdout, "S_VolClone: Error while cloning volume %x -> %x",
	    originalId, newId);
	V_VolLock(originalvp).IPAddress = 0;
	ReleaseWriteLock(&(V_VolLock(originalvp).VolumeLock));
	RVMLIB_BEGIN_TRANSACTION(restore)
	    VPutVolume(originalvp);	/* Do these need transactions? */
	RVMLIB_END_TRANSACTION(flush, &(status));
	assert(status == 0);
	VDisconnectFS();
	return error;
    }
    
    if (V_type(originalvp) == readwriteVolume)
	V_cloneId(originalvp) = newId;
    
    /* assign a name to the clone. if the user requested a name then that is used.
     * otherwise the new name is obtained by appending ".readonly" to the
     * original name.  If it already has ".readonly" then it doesnt change */
    if (newvolname){
	char name[VNAMESIZE];
	strncpy(name, newvolname, VNAMESIZE);
	char *dot;
	dot = rindex(name, '.');
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

    RVMLIB_BEGIN_TRANSACTION(restore)
    VUpdateVolume(&error, newvp);
    VDetachVolume(&error, newvp);
    VUpdateVolume(&error, originalvp);
    assert(error == 0);
    V_VolLock(originalvp).IPAddress = 0;
    ReleaseWriteLock(&(V_VolLock(originalvp).VolumeLock)); 
    VPutVolume(originalvp);
    VListVolumes();			/* Create updated /vice/vol/VolumeList */
    RVMLIB_END_TRANSACTION(flush, &(status));
    VDisconnectFS();
    if (status == 0) {
	LogMsg(0, VolDebugLevel, stdout, "S_VolClone: volume %x cloned", originalId);
	*cloneId = newId;	    /* set value of out parameter */
    }
    else {
	LogMsg(0, VolDebugLevel, stdout, "S_VolClone: volume clone failed for volume %x", originalId);
    }
    return(status?status:0);

}

/* Clones the Volume contents from rwVp to cloneVp */
void VUCloneVolume(Error *error, Volume *rwVp, Volume *cloneVp)
{
    *error = 0;
    VUCloneIndex(error, rwVp, cloneVp, vLarge);
    assert(*error == 0);
    VUCloneIndex(error, rwVp, cloneVp, vSmall);
    assert(*error == 0);
    CopyVolumeHeader(&V_disk(rwVp), &V_disk(cloneVp));	/* Doesn't use RVM */
}

int MaxVnodesPerTransaction = 8;

#ifdef DCS
#include <rvmtesting.h>
#endif DCS

static void VUCloneIndex(Error *error, Volume *rwVp, Volume *cloneVp, VnodeClass vclass)
{
    int status;
    bit32 nvnodes;
    bit32 vnlistSize;
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
	assert(0);

    RVMLIB_BEGIN_TRANSACTION(restore)
    
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

    assert(ovList);	/* How can it not have a list? */
    for (int i = 0; (i < onLists && onVnodes > 0); i++){
	rec_smolink *p;
	while(p = ovList[i].get()) {
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
    assert(tmpvlist != 0);
    bzero((void *)tmpvlist, (int)(sizeof(rec_smolist) * vnlistSize));
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

    RVMLIB_END_TRANSACTION(flush, &(status));
    assert(status == 0);				/* Never aborts... */
	
    char buf[SIZEOF_LARGEDISKVNODE];
    VnodeDiskObject *vnode = (VnodeDiskObject *)buf;
    struct VnodeClassInfo *vcp = &VnodeClassInfo_Array[vclass];

    vindex vol_index(V_id(rwVp), vclass, rwVp->device, vcp->diskSize);
    vindex_iterator vnext(vol_index);

    int vnodeindex;
    int moreVnodes = TRUE;
    *error = 0;
    
    while(moreVnodes) {
#ifdef DCS
	rec_smolist_iterator *DecrementBug;
#endif DCS
	
	RVMLIB_BEGIN_TRANSACTION(restore)
	for (int count = 0; count < MaxVnodesPerTransaction; count++) {
	    if ((vnodeindex = vnext(vnode)) == -1) {
		moreVnodes = FALSE;
		break;
	    }
#ifdef DCS	    
	    DecrementBug = /*vnext.nextlink*/(rec_smolist_iterator *)((int *)&vnext)[4];
#endif DCS

	    *error = CloneVnode(rwVp, cloneVp, vnodeindex, rvlist, vnode, vclass);
	    if (*error)
		rvmlib_abort(VFAIL);

#ifdef	DCS
	    /* Code to check for corruption of rec_smolist iterator. -JJK */
	    {
		/*DecrementBug->clist*/		
		assert(((unsigned int)(((int *)DecrementBug)[0]) & 3) == 0);
		/*DecrementBug->clink*/
		assert(((unsigned int)(((int *)DecrementBug)[1]) & 3) == 0);
		/*DecrementBug->plink*/
		assert(((unsigned int)(((int *)DecrementBug)[2]) & 3) == 0);
	    }
#endif DCS

	} 
	RVMLIB_END_TRANSACTION(flush, &(status));
	if (status != 0) {
	    LogMsg(0, VolDebugLevel, stdout, "CloneIndex: abort for RW %x RO %x",
		   V_id(rwVp), V_id(cloneVp));
	    return;	/* Error is already set... */
	}

	if (moreVnodes) {
	    LogMsg(9, VolDebugLevel, stdout, "Finished %d %s vnode clones.",
		   MaxVnodesPerTransaction, (vclass == vLarge?"Large":"Small"));
	} else {
	    LogMsg(9, VolDebugLevel, stdout, "Finished cloning %s vnodes.",vclass==vLarge?"Large":"Small");
	}

#ifdef	DCS
	    /* Code to check for corruption of rec_smolist iterator. -JJK */
	    {
		/*DecrementBug->clist*/		
		assert(((unsigned int)(((int *)DecrementBug)[0]) & 3) == 0);
		/*DecrementBug->clink*/
		assert(((unsigned int)(((int *)DecrementBug)[1]) & 3) == 0);
		/*DecrementBug->plink*/
		assert(((unsigned int)(((int *)DecrementBug)[2]) & 3) == 0);
		protect_page((int)DecrementBug);
	    }
#endif	DCS

	if (!rvm_no_yield)  /* DEBUG */
	    PollAndYield();

#ifdef	DCS
        /* Code to check for corruption of rec_smolist iterator. -JJK */
        {
	    /*DecrementBug->clist*/		
	    assert(((unsigned int)(((int *)DecrementBug)[0]) & 3) == 0);
	    /*DecrementBug->clink*/
	    assert(((unsigned int)(((int *)DecrementBug)[1]) & 3) == 0);
	    /*DecrementBug->plink*/
	    assert(((unsigned int)(((int *)DecrementBug)[2]) & 3) == 0);
	    unprotect_page((int)DecrementBug);
	}
#endif	DCS
	
    }		/* moreVnodes */
}




/* This must be called from within a transaction! */
/* Create a new vnode in the the new clone volume from the copy of the
 * r/w vnode stored in (vnode). Mark the RW Vnode as cloned.
 */
int CloneVnode(Volume *rwVp, Volume *cloneVp, int vnodeIndex, rec_smolist *rvlist,
	       VnodeDiskObject *vnode, VnodeClass vclass)
{
    Error error = 0;

    int vnodeNum = bitNumberToVnodeNumber(vnodeIndex, vclass);
    LogMsg(9, VolDebugLevel, stdout, "CloneVnode: Cloning %s vnode %x.%x.%x\n",
	   (vclass == vLarge)?"Large":"Small",
	   V_id(rwVp), vnodeNum, vnode->uniquifier);
    
    int size=(vclass==vSmall)?SIZEOF_SMALLDISKVNODE:SIZEOF_LARGEDISKVNODE;
    VnodeDiskObject *vdo = (VnodeDiskObject *) rvmlib_rec_malloc(size);

    bzero((void *)&(vnode->nextvn), sizeof(rec_smolink));
    vnode->vol_index = V_volumeindex(cloneVp);
    
    /* update inode */

    /* If RWvnode doesn't have an inode (or is BARREN), create a new inode
     * for the clone. Otherwise increment the reference count on the inode.
     * If the iinc fails, something must have trashed it since the server
     * started (otherwise the rwVnode would be BARREN).
     * Inodes for Large Vnodes are in RVM, so should never disappear.
     */

    int docreate = FALSE;
	
    if (vclass == vLarge) { /* Directory -- no way it can be BARREN */
	int linkcount = DI_Count((PDirInode)(vnode->inodeNumber));
	assert(linkcount > 0);
	DI_Inc((PDirInode)(vnode->inodeNumber));
    } else {	/* Small Vnode -- file or symlink. */

	if (vnode->inodeNumber == 0) {
	    LogMsg(0,0,stdout,"CloneVolume: VNODE %d HAD ZERO INODE.\n",vnodeNum);
	    assert(vnode->type != vNull);
	    docreate = TRUE;
	} else
	    docreate = (int)IsBarren(vnode->versionvector);
	
	if (docreate) {
	    vnode->inodeNumber = icreate((int)cloneVp->device, 0,
					 (int)V_id(rwVp), vnodeNum,
					 (int)vnode->uniquifier, 0);
	    vnode->length = 0;	/* Reset length since we have a new null inode. */
	} else	
	    /* Inodes should not disappear while the server is running. */
	    assert(iinc((int)rwVp->device, (int)vnode->inodeNumber,
			(int)V_parentId(rwVp)) == 0);
    } 
    
    /* Mark the RW vnode as cloned, !docreate ==> vnode was cloned. */
    if (VolumeWriteable(rwVp) && !docreate) {

	Vnode *tmp = VGetVnode(&error, rwVp, vnodeNum,
			       vnode->uniquifier, WRITE_LOCK, 1, 1);
	if (error) {
	    LogMsg(0,VolDebugLevel, stdout, "CloneVnode(%s): Error %d getting vnode index %d",
		(vclass == vLarge)?"Large":"Small", error, vnodeIndex);
	    return error;	
	}

	tmp->disk.cloned = 1;
	VPutVnode(&error, tmp);	/* Check error? */
	assert(error == 0);
    }
    
    /* R/O vnodes should never be marked inconsistent. */
    vnode->versionvector.Flags = 0;
    vnode->cloned = 0;		/* R/O Vnode should not be marked as cloned. */
    
    rvmlib_modify_bytes(vdo, vnode, size);
    rvlist[vnodeIndex].append(&vdo->nextvn);
    return 0;
}
