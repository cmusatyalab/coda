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
 * vol-backup.c			*
 * Create a backup volume 	*
 ********************************/


#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/time.h>

#include <unistd.h>
#include <stdlib.h>
#include "coda_string.h"

#include <lwp/lwp.h>
#include <lwp/lock.h>
#include <struct.h>
#include <inodeops.h>
#include <util.h>
#include <rvmlib.h>
#include <volutil.h>
#include <vice.h>

#ifdef __cplusplus
}
#endif

#include <voltypes.h>
#include <cvnode.h>
#include <volume.h>
#include <camprivate.h>
#include <srv.h>
#include <vutil.h>
#include <index.h>
#include <recov.h>
#include <codadir.h>
#include <volhash.h>
#include <coda_globals.h>

extern void PollAndYield();
extern int CloneVnode(Volume *, Volume *, int, rec_smolist *,
		      VnodeDiskObject *, VnodeClass);

/* Temp check for debugging. */
void checklists(int vol_index)
{
	unsigned int i;
    bit32 nlists = SRV_RVM(VolumeList[vol_index]).data.nsmallLists;
    int *lists = (int *)SRV_RVM(VolumeList[vol_index]).data.smallVnodeLists;

    for (i = 0; i < nlists; i++) {
	/* Make sure any lists that are single elements are circular */
	if (lists[i])
	    CODA_ASSERT(*((int *)lists[i]));
    }
}

static int MakeNewClone(Volume *rwvp, VolumeId *backupId, Volume **backupvp);
static void ModifyIndex(Volume *rwvp, Volume *backupvp, VnodeClass vclass);
static void purgeDeadVnodes(Volume *backupvp, rec_smolist *BackupLists,
		     rec_smolist *RWLists, VnodeClass vclass, bit32 *nBackupVnodes);
			  
static void updateBackupVnodes(Volume *rwvp, Volume *backupvp,
				rec_smolist *BackupLists, VnodeClass vclass,
				bit32 *nBackupVnodes);

static void cleanup(struct Volume *vp)
{
	rvm_return_t status;
	if (vp) { 
		rvmlib_begin_transaction(restore);
		VPutVolume(vp);
		rvmlib_end_transaction(flush, &(status)); 
		CODA_ASSERT(status == 0); 
	} 
	VDisconnectFS();
}
/*
  S_VolMakeBackups: Make backup of a volume
*/  

/* General Plan:
 * Cloning a volume takes lots of effort -- tons of transactions, spooling gobs
 * of data to the log, and mucho time. It also doesn't haven't to be done
 * every time, because most of the structure of a volume doesn't change.
 * So if a backup clone exists (and was successfully dumped), we can simply
 * modify the volume to reflect any possible changes to the volume.
 *
 * These changes have 3 flavors: creation, deletion, and modification.
 * Creations and Deletions are detected by stepping through the vnode lists
 * of the RW and Backup volumes simultaneously. A vnode in one list and not
 * the other means a creation (or a deletion) was done. If a vnode exists
 * in both lists, if the RW vnode doesn't have its CopyOnWrite bit set, then
 * it was modified. We also need to grow the vnode list for the backup volume
 * if necessary. This should be done before stepping through the list.
 *
 * As a simplification, unlock the orignial RW volume when we've completed or
 * failed the backup clone. This way volumes won't stay locked if the coordinator
 * fails. Only unlock if we've made it far enough to be convinced that the
 * original was locked.
 *
 * originalId: Volume Id of the volume to be cloned.
 * backupId (OUT) Id of (possibly new) backup volume.
 */
long S_VolMakeBackups(RPC2_Handle rpcid, VolumeId originalId, 
		      VolumeId *backupId) 
{
    rvm_return_t status = RVM_SUCCESS;
    long result;
    Error error;
    int rc;
    Volume *originalvp;
    Volume *backupvp;
    VolumeId volid; 

    VLog(9, "Entering S_VolMakeBackups: RPCId=%d, Volume = %x", 
	 rpcid, originalId);

    /* Don't bother unlocking volume, Unlock would fail too */
    if ((rc = VInitVolUtil(volumeUtility))) 
	return rc;

    originalvp = VGetVolume(&error, originalId);
    /* Don't bother unlocking volume, Unlock would fail too */
    if (error) {	
	VLog(0,  "S_VolMakeBackups: failure attaching volume %x", originalId);
	cleanup(originalvp);
	return error;
    }

    if (V_type(originalvp) != readwriteVolume){
	VLog(0, "S_VolMakeBackups: Can only backup ReadWrite vols; aborting");
	cleanup(originalvp);
	return VFAIL;
    }

    /* If volutil doesn't hold the volume lock, return an error */
    if ((V_VolLock(originalvp).IPAddress != 5) || /* Use 5 for now...*/
	(V_VolLock(originalvp).WriteLockType != VolUtil)) {
	VLog(0, "S_VolMakeBackups: VolUtil does not hold Volume Lock for %x.",
	     originalId);
	cleanup(originalvp);
	return VFAIL;
    }

    /* Check to see if we should do a clone or modify the backup in place */
    backupvp = VGetVolume(&error, V_backupId(originalvp));
    if (error) {
	/* If the backup volume doesn't exists or hasn't been successfully
	 * dumped, we need to create a new clone (and delete the old one if
	 * it exists. This is basically old-style backup.
	 */
	result = MakeNewClone(originalvp, backupId, &backupvp);
	if (result) {
	    cleanup(originalvp);
	    S_VolUnlock(rpcid, originalId);
	    return result;
	}
    } else {

	/* If the backup volume exists and was successfully dumped, all we do
	 * is update the backup vnodes to match the r/w vnodes. First, take
	 * the volume offline for safety.
	 */
	 
	/* First do some sanity checking */
	CODA_ASSERT(V_id(backupvp) == V_backupId(originalvp));
	CODA_ASSERT(V_parentId(backupvp) == V_id(originalvp));

	/* force the backup volume offline, and delete it if we crash. */
	/* Alternative could be to compute bitmap for changed backupvp ourself */
	V_destroyMe(backupvp) = DESTROY_ME;
	V_blessed(backupvp) = 0;

	rvmlib_begin_transaction(restore);
	/* Disallow further use of this volume */
	VUpdateVolume(&error, backupvp);
	rvmlib_end_transaction(flush, &(status));
	if (error || status) {
	    VLog(0,  "S_VolMakeBackup: Couldn't force old backup clone offline, aborting!");
	    cleanup(originalvp);
	    S_VolUnlock(rpcid, originalId);
	    return status;
	}

	ModifyIndex(originalvp, backupvp, vSmall);
	ModifyIndex(originalvp, backupvp, vLarge);

	V_backupDate(originalvp) = time(0);

	/* Put the RW's VVV in the backupvp so vol-dump can create an ordering of dumps. */
	memcpy(&V_versionvector(backupvp), &V_versionvector(originalvp),
	       sizeof(vv_t));
	
	*backupId = V_backupId(originalvp);
    }

    /* Clean up: put backup volume back online, update original volume */
    V_destroyMe(backupvp) = 0;
    V_blessed(backupvp) = 1;		/* Volume is valid now. */
    
    rvmlib_begin_transaction(restore);
    volid = V_id(backupvp);

    VUpdateVolume(&error, backupvp);	/* Write new info to RVM */
    CODA_ASSERT(error == 0);
    VDetachVolume(&error, backupvp);   	/* causes vol to be attached(?) */
    CODA_ASSERT(error == 0);
    VUpdateVolume(&error, originalvp);	/* Update R/W vol data */
    CODA_ASSERT(error == 0);
    VPutVolume(originalvp);
    rvmlib_end_transaction(flush, &(status));

    backupvp = VGetVolume(&error, volid);
    CODA_ASSERT(error == 0);
    originalvp = VGetVolume(&error, originalId);
    CODA_ASSERT(error == 0);
    if (status == 0) {
	    VLog(0, "S_VolMakeBackups: backup (%x) made of volume %x ",
		 V_id(backupvp), V_id(originalvp));
    } else {
	    VLog(0, "S_VolMakeBackups: volume backup failed for volume %x",
		 V_id(originalvp));
    }
    VPutVolume(originalvp);
    VPutVolume(backupvp);

    VDisconnectFS();
    S_VolUnlock(rpcid, originalId);

    return (long) status;
}

/*
 * Create a new clone with a new id, delete the old clone, then renumber
 * the new clone with the old number so venus state doesn't need to be
 * flushed to access the new clone.
 */

static int MakeNewClone(Volume *rwvp, VolumeId *backupId, Volume **backupvp)
{
    rvm_return_t status = RVM_SUCCESS;
    Error error;
    VolumeId newId;
    Volume *newvp;

    *backupId = 0;
    rvmlib_begin_transaction(restore);
    
    newId = VAllocateVolumeId(&error);

    if (error){
	VLog(0, "VolMakeBackups: Unable to allocate volume number; ABORTING");
	rvmlib_abort(VNOVOL);
	return VNOVOL;
    } else
	VLog(29, "Backup: VAllocateVolumeId returns %x", newId);
	
    
    newvp = VCreateVolume(&error, V_partname(rwvp), newId, V_id(rwvp), 
			  0, backupVolume);
			  
    if (error) {
	VLog(0, "S_VolMakeBackups:Unable to create the volume; aborted");
	rvmlib_abort(VNOVOL);
	return VNOVOL;
    }

    /* Don't let other users get at new volume until clone is done. */
    V_blessed(newvp) = 0;
    VUpdateVolume(&error, newvp);
    if (error) {
	VLog(0, "S_VolMakeBackups: Volume %x can't be unblessed!", newId);
	rvmlib_abort(VFAIL);
	return VFAIL;
    }
    
    rvmlib_end_transaction(flush, &(status));
    if (status != 0) {
	VLog(0, "S_VolMakeBackups: volume backup creation failed for %#x",
	     V_id(rwvp));
	return status;
    }
    
    VLog(9, "S_VolMakeBackups: Created Volume %x; going to clone from %x",
	 newId, V_id(rwvp));
    checklists(V_volumeindex(rwvp));
    checklists(V_volumeindex(newvp));

    /* Should I putvolume on newvp if this fails? */
    VUCloneVolume(&error, rwvp, newvp);
    if (error){
	VLog(0, "S_VolMakeBackups: Error while cloning volume %x -> %x",
	     V_id(rwvp), newId);
	return error;
    }

    checklists(V_volumeindex(rwvp));
    checklists(V_volumeindex(newvp));

    /* Modify backup stats and pointers in the original volume, this includes 
     * the BackupId, BackupDate, and deleting the old Backup volume.
     */
    VLog(0, "S_VolMakeBackups: Deleting the old backup volume");
    VolumeId oldId = V_backupId(rwvp);
    Volume *oldbackupvp = VGetVolume(&error, oldId);
    if (error) {
	VLog(0, "S_VolMakeBackups: Unable to get the old backup volume %x %d",
	     V_backupId(rwvp), error);
	V_backupId(rwvp) = oldId = newId;
    } else {
	checklists(V_volumeindex(rwvp));
	checklists(V_volumeindex(newvp));
	checklists(V_volumeindex(oldbackupvp));
	
	CODA_ASSERT(DeleteVolume(oldbackupvp) == 0);	/* Delete the volume */

	checklists(V_volumeindex(rwvp));
	checklists(V_volumeindex(newvp));


	/* Renumber new clone to that of old clone. */	
	/* Remove newId from hash tables */
	HashDelete(newId);  	
	DeleteVolumeFromHashTable(newvp);
	/* Modify the Id in the vm cache and rvm */
	newId = V_id(newvp) = oldId;  

	rvmlib_begin_transaction(restore);
	RVMLIB_MODIFY(SRV_RVM(VolumeList[V_volumeindex(newvp)]).header.id, 
		      oldId);
	rvmlib_end_transaction(flush, &(status));
	CODA_ASSERT(status == 0);

	HashInsert(oldId, V_volumeindex(newvp));
	
	VLog(5, "S_VolMakeBackups: Finished deleting old volume.");
    }

    /* Finalize operation by setting up VolData */
    
    V_backupDate(rwvp) = V_creationDate(newvp);
    /* assign a name to the clone by appending ".backup" 
       to the original name. */
    AssignVolumeName(&V_disk(newvp), V_name(rwvp), ".backup");

    V_type(newvp) = backupVolume;
    V_creationDate(newvp) = V_copyDate(newvp);
    ClearVolumeStats(&V_disk(newvp));  /* Should we do this? */

    *backupId = newId;	    /* set value of out parameter */
    *backupvp = newvp;
    return status;
}


static void ModifyIndex(Volume *rwvp, Volume *backupvp, VnodeClass vclass)
{
    int rwIndex = V_volumeindex(rwvp);
    int backupIndex = V_volumeindex(backupvp);
    bit32 nBackupVnodes, nRWVnodes;
    rec_smolist *backupVnodes;	/* Pointer to list of vnodes in backup clone.*/
    rec_smolist *rwVnodes;	/* Pointer to list of vnodes in RW clone. */
    rvm_return_t status = RVM_SUCCESS;
    
    /* Make sure there are as many backup lists as there are rw lists. */
    if (vclass == vSmall) {
	if (SRV_RVM(VolumeList[rwIndex]).data.nsmallLists >
	    SRV_RVM(VolumeList[backupIndex]).data.nsmallLists) {

	    /* Growvnodes for backupvolume */
	    rvmlib_begin_transaction(restore);
	    GrowVnodes(V_id(backupvp), vclass, rwvp->vnIndex[vclass].bitmapSize);
	    rvmlib_end_transaction(flush, &(status));
	    CODA_ASSERT(status == 0);
	}

	/* Since lists never shrink, how could there be more backup lists? */
	CODA_ASSERT(SRV_RVM(VolumeList[rwIndex]).data.nsmallLists ==
	       SRV_RVM(VolumeList[backupIndex]).data.nsmallLists);

	nBackupVnodes = SRV_RVM(VolumeList[backupIndex]).data.nsmallvnodes;	
	backupVnodes = SRV_RVM(VolumeList[backupIndex]).data.smallVnodeLists;

	nRWVnodes = SRV_RVM(VolumeList[rwIndex]).data.nsmallvnodes;	
	rwVnodes = SRV_RVM(VolumeList[rwIndex]).data.smallVnodeLists;

    } else {
	if (SRV_RVM(VolumeList[rwIndex]).data.nlargeLists >
	    SRV_RVM(VolumeList[backupIndex]).data.nlargeLists) {

	    /* Growvnodes for backupvolume */
	    rvmlib_begin_transaction(restore);
	    GrowVnodes(V_id(backupvp), vclass, rwvp->vnIndex[vclass].bitmapSize);
	    rvmlib_end_transaction(flush, &(status));
	    CODA_ASSERT(status == 0);
	}

	/* Since lists never shrink, how could there be more backup lists? */
	CODA_ASSERT(SRV_RVM(VolumeList[rwIndex]).data.nlargeLists ==
	       SRV_RVM(VolumeList[backupIndex]).data.nlargeLists);

	nBackupVnodes = SRV_RVM(VolumeList[backupIndex]).data.nlargevnodes;	
	backupVnodes = SRV_RVM(VolumeList[backupIndex]).data.largeVnodeLists;

	nRWVnodes = SRV_RVM(VolumeList[rwIndex]).data.nlargevnodes;	
	rwVnodes = SRV_RVM(VolumeList[rwIndex]).data.largeVnodeLists;
    }
    
    /* First, purge any backup vnodes corresponding to rw vnodes that have
     * been deleted since the last backup.
     */
    purgeDeadVnodes(backupvp, backupVnodes, rwVnodes, vclass, &nBackupVnodes); 

    /* Next, update/create backup vnodes for any rw vnodes that have been
     * been modified or created since the last backup.
     */
    updateBackupVnodes(rwvp, backupvp, backupVnodes, vclass, &nBackupVnodes);

    /* At this point the volumes should be the same. */
    CODA_ASSERT(nBackupVnodes == nRWVnodes);

    /* Update the number of vnodes in the backup volume header */
    rvmlib_begin_transaction(restore);
    if (vclass == vSmall)
	RVMLIB_MODIFY(SRV_RVM(VolumeList[backupIndex]).data.nsmallvnodes, nBackupVnodes);
    else
	RVMLIB_MODIFY(SRV_RVM(VolumeList[backupIndex]).data.nlargevnodes, nBackupVnodes);
	
    rvmlib_end_transaction(flush, &(status));
    CODA_ASSERT(status == 0);
}

/*
 * Step through the list of backup vnodes. Delete any that don't have 
 * corresponding vnodes (i.e. matching uniquifiers) in the read/write volume.
 * Must be called from within a transaction.
 */
extern int MaxVnodesPerTransaction;

static void deleteDeadVnode(rec_smolist *list,
			     VnodeDiskObject *vdop,
			     bit32 *nBackupVnodes)
{
    /* Make sure remove doesn't return null */
    CODA_ASSERT(list->remove(&(vdop->nextvn)));

    /* decrement the reference count by one */
    if (vdop->inodeNumber) {
	if (vdop->type == vDirectory)
	    DI_Dec((DirInode *)vdop->inodeNumber); 
    }
    
    /* Don't bother with the vnode free list */
    rvmlib_rec_free((char *)strbase(VnodeDiskObject, &(vdop->nextvn), nextvn)); 
    
    /* decrement the vnode count on the volume. */
    (*nBackupVnodes)--;
}


static void purgeDeadVnodes(Volume *backupvp, rec_smolist *BackupLists,
		      rec_smolist *RWLists, VnodeClass vclass, bit32 *nBackupVnodes)
{
    rvm_return_t status = RVM_SUCCESS;
    
    /* Set up an iterator for the backup vnodes. */
    struct VnodeClassInfo *vcp = &VnodeClassInfo_Array[vclass];
    vindex vol_index(V_id(backupvp), vclass, V_device(backupvp), vcp->diskSize);
    vindex_iterator vnext(vol_index);
    char buf[SIZEOF_LARGEDISKVNODE];
    VnodeDiskObject *bVnode = (VnodeDiskObject *)buf;

    /* If we can't open this file, dump the stuff to the Log. */
    FILE *tmpdebug = NULL;
    if (VolDebugLevel) 
	tmpdebug = fopen("/vicepa/dcstest", "w+");
    if (tmpdebug == NULL) tmpdebug = stdout;

    /* Only idec inodes after the transaction has commited in case of abort */
    Inode *DeadInodes = (Inode *)malloc((MaxVnodesPerTransaction + 1) * sizeof(Inode));

    int vnodeIndex = 0;
    while (vnodeIndex != -1) {			     /* More vnodes to process */
	int count = 0;
	memset((void *)DeadInodes, 0, sizeof(Inode) * MaxVnodesPerTransaction);

	/* Bunch vnode operations into groups of at most 8 per transaction */
	/* Right now we might have transactions with no operations, oh well. */
	rvmlib_begin_transaction(restore);

	for (count = 0; count < MaxVnodesPerTransaction; count++) {

	    if ((vnodeIndex = vnext(bVnode)) == -1) 
		break;					/* No more vnodes. */

	    VLog(1, "Got bVnode (%08x.%x.%x)\n",
		   V_id(backupvp), bitNumberToVnodeNumber(vnodeIndex, vclass),
		   bVnode->uniquifier);

	    /* Find the rwvnode. If it doesn't exist, delete this vnode */
	    if (FindVnode(&RWLists[vnodeIndex], bVnode->uniquifier) == NULL) {
		/* Get a pointer to the backupVnode in rvm. */
		VnodeDiskObject *bvdop;
		bvdop = FindVnode(&BackupLists[vnodeIndex], bVnode->uniquifier);
		CODA_ASSERT(bvdop);

		/* Delete the backup vnode. */
		VLog(9, "purgeDeadVnodes: purging %x.%x.%x",
		       V_id(backupvp), bitNumberToVnodeNumber(vnodeIndex, vclass),
		       bvdop->uniquifier);
		
		VLog(1, "purgeDeadVnodes: purging %x.%x.%x",
		       V_id(backupvp), bitNumberToVnodeNumber(vnodeIndex, vclass),
		       bvdop->uniquifier);
		
		if (vclass == vSmall)
		    DeadInodes[count] = bvdop->inodeNumber;
		
		deleteDeadVnode(&BackupLists[vnodeIndex], bvdop, nBackupVnodes);
	    }
	}
	rvmlib_end_transaction(flush, &(status));
	CODA_ASSERT(status == 0);		/* Should never abort... */

	/* Now delete the inodes for the vnodes we already purged. */
	if (vclass == vSmall) 
	    for (int i = 0; i < count; i++) {
		/* Assume inodes for backup volumes are stored with
		 * the VolumeId of the original read/write volume.
		 */
		if (DeadInodes[i])
		    if (idec((int)V_device(backupvp), (int)DeadInodes[i],
			     V_parentId(backupvp)))
			VLog(0,0,stdout,"VolBackup: idec failed with %d",errno);
	    }	
	    
	PollAndYield();	/* Give someone else a chance */
    }

    free(DeadInodes);
    if (tmpdebug != stdout) fclose(tmpdebug);
}


/* Clone any new or changed vnodes in the RWVolume. We know a vnode is new
 * or changed because the clone bit isn't set.

 * The basic idea is to check to see if the rwVnode has changed with respect
 * to the backupVnode. There are two tests for this:
 * 1. If the cloned flag is no longer set, then the file's data has changed.
 * 2. If the VV flag has changed (but the cloned flag hasn't), the file's
 *	  meta data has changed (as in the case of a res ForceVV). In this case
 *    we don't need to play with the inode, just copy the vnode.

 * Only idec inodes after the transaction has commited in case of abort */

static void updateBackupVnodes(Volume *rwvp, Volume *backupvp,
				rec_smolist *BackupLists, VnodeClass vclass, 
				bit32 *nBackupVnodes)
{
    rvm_return_t status = RVM_SUCCESS;

    /* setup an iterator for the read/write vnodes */
    struct VnodeClassInfo *vcp = &VnodeClassInfo_Array[vclass];
    vindex vol_index(V_id(rwvp), vclass, V_device(rwvp), vcp->diskSize);
    vindex_iterator next(vol_index);
    char buf[SIZEOF_LARGEDISKVNODE];
    VnodeDiskObject *rwVnode = (VnodeDiskObject *)buf;

    Inode *DeadInodes = (Inode *)malloc((MaxVnodesPerTransaction + 1) * sizeof(Inode));

    int vnodeIndex = 0;
    while (vnodeIndex != -1) {
	int count = 0;

	memset((void *)DeadInodes, 0, sizeof(Inode) * (MaxVnodesPerTransaction + 1));

	rvmlib_begin_transaction(restore);

	/* break out every 8 vnodes, regardless of if they've changed.*/
	for (count = 0; count < MaxVnodesPerTransaction; count++) {

	    if ((vnodeIndex = next(rwVnode)) == -1)	/* No more vnodes */
		break;
	    
	    VnodeDiskObject *bvdop;

	    /* Find the backupVnode in rvm */
	    bvdop = FindVnode(&BackupLists[vnodeIndex], rwVnode->uniquifier);

	    /* If the rwVnode is cloned and has a backup vnode, the inode can't
	     * have changed, but various fields in the vnode could have. If so,
	     * use the rwVnode to update the backupVnode, preserving certain fields.
	     */
	    
	    if (rwVnode->cloned && bvdop) {
		if (rwVnode->type != bvdop->type)
		    VLog(0,0,stdout, "VolBackup: RW and RO types don't match (%x.%x.%x) %d != %d\n",
			   V_id(rwvp),
			   bitNumberToVnodeNumber(vnodeIndex, vclass),
			   rwVnode->uniquifier,
			   rwVnode->type, bvdop->type);
		
		if (rwVnode->length != bvdop->length) // If this changes, COW should happen.
		    VLog(0,0,stdout, "VolBackup: RW and RO lengths don't match (%x.%x.%x) %d != %d\n",
			   V_id(rwvp),
			   bitNumberToVnodeNumber(vnodeIndex, vclass),
			   rwVnode->uniquifier,
			   rwVnode->length, bvdop->length);
		
		if (rwVnode->uniquifier != bvdop->uniquifier)
		    VLog(0,0,stdout, "VolBackup: RW and RO uniquifiers don't match (%x.%x.%x) %d != %d\n",
			   V_id(rwvp),
			   bitNumberToVnodeNumber(vnodeIndex, vclass),
			   rwVnode->uniquifier,
			   rwVnode->uniquifier, bvdop->uniquifier);
		
		if (rwVnode->dataVersion != bvdop->dataVersion)
		    VLog(0,0,stdout, "VolBackup: RW and RO dataVersions don't match (%x.%x.%x) %d != %d\n",
			   V_id(rwvp),
			   bitNumberToVnodeNumber(vnodeIndex, vclass),
			   rwVnode->uniquifier,
			   rwVnode->dataVersion, bvdop->dataVersion);
		
		if (rwVnode->inodeNumber != bvdop->inodeNumber) {
		    /* This can't change without a COW!!! */
		    CODA_ASSERT(IsBarren(rwVnode->versionvector));
		    CODA_ASSERT(bvdop->inodeNumber);	/* RO vnode Can't be barren */
		}		    


		CODA_ASSERT(bvdop->vol_index != rwVnode->vol_index);
		
		if (rwVnode->vnodeMagic != bvdop->vnodeMagic)
		    VLog(0,0,stdout, "VolBackup: RW and RO vnodeMagics don't match (%x.%x.%x) %d != %d\n",
			   V_id(rwvp),
			   bitNumberToVnodeNumber(vnodeIndex, vclass),
			   rwVnode->uniquifier,
			   rwVnode->vnodeMagic, bvdop->vnodeMagic);


		/* If anything changed, update the backup Vnode */
		/* If the rwVnode is inconsistent, the VV_Cmp would fail
		 * since the backupVnode can never be inconsistent.
		 */
		if ((rwVnode->modeBits != bvdop->modeBits) ||
		    (rwVnode->linkCount != bvdop->linkCount) ||
		    (VV_Cmp_IgnoreInc(&rwVnode->versionvector,&bvdop->versionvector)!=VV_EQ) ||
		    (bvdop->unixModifyTime != rwVnode->unixModifyTime) ||
		    (bvdop->author != rwVnode->author) ||
		    (bvdop->owner != rwVnode->owner) ||
		    (bvdop->vparent != rwVnode->vparent) ||
		    (bvdop->uparent != rwVnode->uparent) ||
		    (bvdop->serverModifyTime != rwVnode->serverModifyTime)) {
		    
		    VLog(9, "VolBackup: RW Vnode (%08x.%x.%x) metadata changed, recloning.\n",
			   V_id(rwvp), bitNumberToVnodeNumber(vnodeIndex, vclass),
			   rwVnode->uniquifier);

		    /* Save some fields from the backup vnode */
		    rwVnode->cloned = 0;	// backupVnodes can't be cloned.
		    rwVnode->versionvector.Flags = 0; // R/O vnodes shouldn't be inconsistent.
		    rwVnode->vol_index = bvdop->vol_index;
		    rwVnode->nextvn = bvdop->nextvn;

		    rvmlib_modify_bytes(bvdop, rwVnode, (vclass == vLarge) ?
					SIZEOF_LARGEDISKVNODE : SIZEOF_SMALLDISKVNODE);
		}

		continue;
	    }

	    if (bvdop) {
		/* rwVnode must have been modified */
		/* This test only works on replicated volumes. Should I make it
		 * at all or should I test for replicated first? -dcs 2/22/92
		 */
/*	CODA_ASSERT(VV_Cmp(&rwVnode->versionvector, &bvdop->versionvector)!= VV_EQ); */

		/* Delete the backup vnode rather than modify it. */
		VLog(9, 
		       "UpdateBackupVnode: purging %x.%x.%x",
		       V_id(backupvp), bitNumberToVnodeNumber(vnodeIndex, vclass),
		       bvdop->uniquifier);

		/* Directory Inodes are in RVM so DI_Decs get undone on abort. */
		if (vclass == vSmall)	
		    DeadInodes[count] = bvdop->inodeNumber;
		
		deleteDeadVnode(&BackupLists[vnodeIndex], bvdop, nBackupVnodes);
	    }

	    /* Create a new clone of the vnode. */
	    VLog(9, "UpdateBackupVnode: cloning %x.%x.%x", V_id(backupvp),
		   bitNumberToVnodeNumber(vnodeIndex, vclass),
		   rwVnode->uniquifier);
	    CloneVnode(rwvp, backupvp, vnodeIndex, BackupLists, rwVnode, vclass);
	    (*nBackupVnodes)++;
	    
	} /* Inner loop -> less than MaxVnodesPerTransaction times around */
	rvmlib_end_transaction(flush, &(status));
	CODA_ASSERT(status == 0);  /* Do we ever abort? */

	/* Now delete the inodes for the vnodes we already purged. */
	if (vclass == vSmall) 
	    for (int i = 0; i < count; i++) {
		/* Assume inodes for backup volumes are stored with
		 * the VolumeId of the original read/write volume.
		 */
		if (DeadInodes[i])
		    if (idec((int)V_device(backupvp), (int)DeadInodes[i],
			     V_parentId(backupvp)))
			VLog(0, "VolBackup: idec failed with %d", errno);
	    }	
	
	PollAndYield();	/* Give someone else a chance */
    }

    free(DeadInodes);
}


