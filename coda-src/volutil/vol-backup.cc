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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/volutil/vol-backup.cc,v 4.6 1998/01/10 18:39:54 braam Exp $";
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
 * vol-backup.c			*
 * Create a backup volume 	*
 ********************************/


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/time.h>

#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#include <mach.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif

#include <lwp.h>
#include <struct.h>
#include <lock.h>
#include <inodeops.h>
#ifdef __cplusplus
}
#endif __cplusplus

#include <volutil.h>
#include <util.h>
#include <rvmlib.h>
#include <voltypes.h>
#include <vice.h>
#include <cvnode.h>
#include <volume.h>
#include <camprivate.h>
#include <srv.h>
#include <vutil.h>
#include <index.h>
#include <recov.h>
#include <rvmdir.h>
#include <volhash.h>
#include <coda_globals.h>

extern void PollAndYield();
extern int CloneVnode(Volume *, Volume *, int, rec_smolist *,
		      VnodeDiskObject *, VnodeClass);

/* Temp check for debugging. */
void checklists(int vol_index)
{
    bit32 nlists = CAMLIB_REC(VolumeList[vol_index]).data.nsmallLists;
    bit32 nvnodes = CAMLIB_REC(VolumeList[vol_index]).data.nsmallvnodes;
    int *lists = (int *)CAMLIB_REC(VolumeList[vol_index]).data.smallVnodeLists;

    for (int i = 0; i < nlists; i++) {
	/* Make sure any lists that are single elements are circular */
	if (lists[i])
	    assert(*((int *)lists[i]));
    }
}

PRIVATE int MakeNewClone(Volume *rwvp, VolumeId *backupId, Volume **backupvp);
PRIVATE void ModifyIndex(Volume *rwvp, Volume *backupvp, VnodeClass vclass);
PRIVATE void purgeDeadVnodes(Volume *backupvp, rec_smolist *BackupLists,
		     rec_smolist *RWLists, VnodeClass vclass, bit32 *nBackupVnodes);
			  
PRIVATE void updateBackupVnodes(Volume *rwvp, Volume *backupvp,
				rec_smolist *BackupLists, VnodeClass vclass,
				bit32 *nBackupVnodes);

/* Numerous times we repeat exactly the same code, so MACRO-ize it */
#define CLEANUP(vp)	\
    if (vp) { \
	CAMLIB_BEGIN_TOP_LEVEL_TRANSACTION_2(CAM_TRAN_NV_SERVER_BASED) \
	VPutVolume(originalvp); \
	CAMLIB_END_TOP_LEVEL_TRANSACTION_2(CAM_PROT_TWO_PHASED, status) \
	assert(status == 0); \
    } \
    VDisconnectFS();
/*
  BEGIN_HTML
  <a name="S_VolMakeBackups"><strong>Make backup of a volume</strong></a> 
  END_HTML
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
long S_VolMakeBackups(RPC2_Handle rpcid, VolumeId originalId, VolumeId *backupId) {
    int status = 0;
    Error error;

    LogMsg(9, VolDebugLevel, stdout,
	   "Entering S_VolMakeBackups: RPCId=%d, Volume = %x", rpcid, originalId);

    int rc = VInitVolUtil(volumeUtility);
    if (rc != 0) 	/* Don't bother unlocking volume, Unlock would fail too */
	return rc;

    Volume *originalvp = VGetVolume(&error, originalId);
    if (error) {	/* Don't bother unlocking volume, Unlock would fail too */
	LogMsg(0, VolDebugLevel, stdout, "S_VolMakeBackups: failure attaching volume %x", originalId);
	CLEANUP(originalvp);
	return error;
    }

    if (V_type(originalvp) != readwriteVolume){
	LogMsg(0, VolDebugLevel, stdout, "S_VolMakeBackups: Can only backup ReadWrite volumes; aborting");
	CLEANUP(originalvp);
	return VFAIL;
    }

    /* If volutil doesn't hold the volume lock, return an error */
    if ((V_VolLock(originalvp).IPAddress != 5) || /* Use 5 for now...*/
	(V_VolLock(originalvp).WriteLockType != VolUtil)) {
	LogMsg(0, VolDebugLevel, stdout,
	       "S_VolMakeBackups: VolUtil does not hold Volume Lock for %x.",
	       originalId);

	CLEANUP(originalvp);
	return VFAIL;
    }

    /* Check to see if we should do a clone or modify the backup in place */
    Volume *backupvp = VGetVolume(&error, V_backupId(originalvp));
    if (error  /* Or the backup wasn't successfully dumped. */) {

	/* If the backup volume doesn't exists or hasn't been successfully
	 * dumped, we need to create a new clone (and delete the old one if
	 * it exists. This is basically old-style backup.
	 */
	status = MakeNewClone(originalvp, backupId, &backupvp);
	if (status) {
	    CLEANUP(originalvp);
	    S_VolUnlock(rpcid, originalId);
	    return status;
	}
    } else {

	/* If the backup volume exists and was successfully dumped, all we do
	 * is update the backup vnodes to match the r/w vnodes. First, take
	 * the volume offline for safety.
	 */
	 
	/* First do some sanity checking */
	assert(V_id(backupvp) == V_backupId(originalvp));
	assert(V_parentId(backupvp) == V_id(originalvp));

	/* force the backup volume offline, and delete it if we crash. */
	/* Alternative could be to compute bitmap for changed backupvp ourself */
	V_destroyMe(backupvp) = DESTROY_ME;
	V_blessed(backupvp) = 0;

	CAMLIB_BEGIN_TOP_LEVEL_TRANSACTION_2(CAM_TRAN_NV_SERVER_BASED)
	VUpdateVolume(&error, backupvp);/* Disallow further use of this volume */
	CAMLIB_END_TOP_LEVEL_TRANSACTION_2(CAM_PROT_TWO_PHASED, status)
	if (error || status) {
	    LogMsg(0, VolDebugLevel, stdout, "S_VolMakeBackup: Couldn't force old backup clone offline, aborting!");
	    CLEANUP(originalvp);
	    S_VolUnlock(rpcid, originalId);
	    return status;
	}

	ModifyIndex(originalvp, backupvp, vSmall);
	ModifyIndex(originalvp, backupvp, vLarge);

	V_backupDate(originalvp) = time(0);

	/* Put the RW's VVV in the backupvp so vol-dump can create an ordering of dumps. */
	bcopy((const void *)&V_versionvector(originalvp), (void *) &V_versionvector(backupvp), sizeof(vv_t));
	
	*backupId = V_backupId(originalvp);
    }

    /* Clean up: put backup volume back online, update original volume */
    V_destroyMe(backupvp) = 0;
    V_blessed(backupvp) = 1;		/* Volume is valid now. */
    
    CAMLIB_BEGIN_TOP_LEVEL_TRANSACTION_2(CAM_TRAN_NV_SERVER_BASED)
    VUpdateVolume(&error, backupvp);		/* Write new info to RVM */
    VDetachVolume(&error, backupvp);    	/* causes vol to be attached(?) */
    VUpdateVolume(&error, originalvp);		/* Update R/W vol data */
    assert(error == 0);
    VPutVolume(originalvp);
    CAMLIB_END_TOP_LEVEL_TRANSACTION_2(CAM_PROT_TWO_PHASED, status)
#ifndef __linux__
/* temporarily disable for linux, need to fix this */
    if (status == 0) {
	LogMsg(0, VolDebugLevel, stdout,
	       "S_VolMakeBackups: backup (%x) made of volume %x ",
	       V_id(backupvp), V_id(originalvp));
    }
    else {
	LogMsg(0, VolDebugLevel, stdout,
	       "S_VolMakeBackups: volume backup failed for volume %x",
	       V_id(originalvp));
    }
#endif
    VListVolumes();	     /* Really ugly, do this to update VolumeList file */
    VDisconnectFS();
    S_VolUnlock(rpcid, originalId);

    return (status? status : 0);
}

/*
 * Create a new clone with a new id, delete the old clone, then renumber
 * the new clone with the old number so venus state doesn't need to be
 * flushed to access the new clone.
 */

PRIVATE int MakeNewClone(Volume *rwvp, VolumeId *backupId, Volume **backupvp)
{
    int status = 0;
    Error error;
    VolumeId newId;
    Volume *newvp;

    *backupId = 0;
    CAMLIB_BEGIN_TOP_LEVEL_TRANSACTION_2(CAM_TRAN_NV_SERVER_BASED)
    
    newId = VAllocateVolumeId(&error);

    if (error){
	LogMsg(0, VolDebugLevel, stdout,
	       "VolMakeBackups: Unable to allocate a volume number; ABORTING");
	CAMLIB_ABORT(VNOVOL);
    } else
	LogMsg(29, VolDebugLevel, stdout, "Backup: VAllocateVolumeId returns %x",
	       newId);
	
    
    newvp = VCreateVolume(&error, V_partname(rwvp), newId, V_id(rwvp), 0, backupVolume);
			  
    if (error) {
	LogMsg(0, VolDebugLevel, stdout,
	       "S_VolMakeBackups:Unable to create the volume; aborted");
	CAMLIB_ABORT(VNOVOL);
    }

    /* Don't let other users get at new volume until clone is done. */
    V_blessed(newvp) = 0;
    VUpdateVolume(&error, newvp);
    if (error) {
	LogMsg(0, VolDebugLevel, stdout, "S_VolMakeBackups: Volume %x can't be unblessed!", newId);
	CAMLIB_ABORT(VFAIL);
    }
    
    CAMLIB_END_TOP_LEVEL_TRANSACTION_2(CAM_PROT_TWO_PHASED, status)
    if (status != 0) {
	LogMsg(0, VolDebugLevel, stdout, "S_VolMakeBackups: volume backup creation failed for volume %x",
	    V_id(rwvp));
	return status;
    }
    
    LogMsg(9, VolDebugLevel, stdout,
	   "S_VolMakeBackups: Created Volume %x; going to clone from %x",
	   newId, V_id(rwvp));

    checklists(V_volumeindex(rwvp));
    checklists(V_volumeindex(newvp));

    /* Should I putvolume on newvp if this fails? */
    VUCloneVolume(&error, rwvp, newvp);
    if (error){
	LogMsg(0, VolDebugLevel, stdout, "S_VolMakeBackups: Error while cloning volume %x -> %x",
	    V_id(rwvp), newId);
	return error;
    }

    checklists(V_volumeindex(rwvp));
    checklists(V_volumeindex(newvp));

    /* Modify backup stats and pointers in the original volume, this includes 
     * the BackupId, BackupDate, and deleting the old Backup volume.
     */
    LogMsg(0, VolDebugLevel, stdout, "S_VolMakeBackups: Deleting the old backup volume");
    VolumeId oldId = V_backupId(rwvp);
    Volume *oldbackupvp = VGetVolume(&error, oldId);
    if (error) {
	LogMsg(0, VolDebugLevel, stdout, "S_VolMakeBackups: Unable to get the old backup volume %x %d",
	    V_backupId(rwvp), error);
	V_backupId(rwvp) = oldId = newId;
    } else {
	checklists(V_volumeindex(rwvp));
	checklists(V_volumeindex(newvp));
	checklists(V_volumeindex(oldbackupvp));
	
	assert(DeleteVolume(oldbackupvp) == 0);	/* Delete the volume */

	checklists(V_volumeindex(rwvp));
	checklists(V_volumeindex(newvp));


	/* Renumber new clone to that of old clone. */	
	HashDelete(newId);  	/* Remove newId from hash tables */
	DeleteVolumeFromHashTable(newvp);
	newId = V_id(newvp) = oldId;  /* Modify the Id in the vm cache and rvm */

	CAMLIB_BEGIN_TOP_LEVEL_TRANSACTION_2(CAM_TRAN_NV_SERVER_BASED)
	    CAMLIB_MODIFY(CAMLIB_REC(VolumeList[V_volumeindex(newvp)]).header.id, oldId);
	CAMLIB_END_TOP_LEVEL_TRANSACTION_2(CAM_PROT_TWO_PHASED, status)
	assert(status == 0);

	HashInsert(oldId, V_volumeindex(newvp));
	
	LogMsg(5, VolDebugLevel, stdout, "S_VolMakeBackups: Finished deleting old volume.");
    }

    /* Finalize operation by setting up VolData */
    
    V_backupDate(rwvp) = V_creationDate(newvp);
    /* assign a name to the clone by appending ".backup" to the original name. */
    AssignVolumeName(&V_disk(newvp), V_name(rwvp), ".backup");

    V_type(newvp) = backupVolume;
    V_creationDate(newvp) = V_copyDate(newvp);
    ClearVolumeStats(&V_disk(newvp));  /* Should we do this? */

    *backupId = newId;	    /* set value of out parameter */
    *backupvp = newvp;
    return(status?status:0);
}



PRIVATE void ModifyIndex(Volume *rwvp, Volume *backupvp, VnodeClass vclass)
{
    int rwIndex = V_volumeindex(rwvp);
    int backupIndex = V_volumeindex(backupvp);
    bit32 nBackupVnodes, nRWVnodes;
    rec_smolist *backupVnodes;	/* Pointer to list of vnodes in backup clone. */
    rec_smolist *rwVnodes;		/* Pointer to list of vnodes in RW clone. */
    int status = 0;
    
    /* Make sure there are as many backup lists as there are rw lists. */
    if (vclass == vSmall) {
	if (CAMLIB_REC(VolumeList[rwIndex]).data.nsmallLists >
	    CAMLIB_REC(VolumeList[backupIndex]).data.nsmallLists) {

	    /* Growvnodes for backupvolume */
	    CAMLIB_BEGIN_TOP_LEVEL_TRANSACTION_2(CAM_TRAN_NV_SERVER_BASED)
	    GrowVnodes(V_id(backupvp), vclass, rwvp->vnIndex[vclass].bitmapSize);
	    CAMLIB_END_TOP_LEVEL_TRANSACTION_2(CAM_PROT_TWO_PHASED, status)
	    assert(status == 0);
	}

	/* Since lists never shrink, how could there be more backup lists? */
	assert(CAMLIB_REC(VolumeList[rwIndex]).data.nsmallLists ==
	       CAMLIB_REC(VolumeList[backupIndex]).data.nsmallLists);

	nBackupVnodes = CAMLIB_REC(VolumeList[backupIndex]).data.nsmallvnodes;	
	backupVnodes = CAMLIB_REC(VolumeList[backupIndex]).data.smallVnodeLists;

	nRWVnodes = CAMLIB_REC(VolumeList[rwIndex]).data.nsmallvnodes;	
	rwVnodes = CAMLIB_REC(VolumeList[rwIndex]).data.smallVnodeLists;

    } else {
	if (CAMLIB_REC(VolumeList[rwIndex]).data.nlargeLists >
	    CAMLIB_REC(VolumeList[backupIndex]).data.nlargeLists) {

	    /* Growvnodes for backupvolume */
	    CAMLIB_BEGIN_TOP_LEVEL_TRANSACTION_2(CAM_TRAN_NV_SERVER_BASED)
	    GrowVnodes(V_id(backupvp), vclass, rwvp->vnIndex[vclass].bitmapSize);
	    CAMLIB_END_TOP_LEVEL_TRANSACTION_2(CAM_PROT_TWO_PHASED, status)
	    assert(status == 0);
	}

	/* Since lists never shrink, how could there be more backup lists? */
	assert(CAMLIB_REC(VolumeList[rwIndex]).data.nlargeLists ==
	       CAMLIB_REC(VolumeList[backupIndex]).data.nlargeLists);

	nBackupVnodes = CAMLIB_REC(VolumeList[backupIndex]).data.nlargevnodes;	
	backupVnodes = CAMLIB_REC(VolumeList[backupIndex]).data.largeVnodeLists;

	nRWVnodes = CAMLIB_REC(VolumeList[rwIndex]).data.nlargevnodes;	
	rwVnodes = CAMLIB_REC(VolumeList[rwIndex]).data.largeVnodeLists;
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
    assert(nBackupVnodes == nRWVnodes);

    /* Update the number of vnodes in the backup volume header */
    CAMLIB_BEGIN_TOP_LEVEL_TRANSACTION_2(CAM_TRAN_NV_SERVER_BASED)
    if (vclass == vSmall)
	CAMLIB_MODIFY(CAMLIB_REC(VolumeList[backupIndex]).data.nsmallvnodes, nBackupVnodes);
    else
	CAMLIB_MODIFY(CAMLIB_REC(VolumeList[backupIndex]).data.nlargevnodes, nBackupVnodes);
	
    CAMLIB_END_TOP_LEVEL_TRANSACTION_2(CAM_PROT_TWO_PHASED, status)
    assert(status == 0);
}

/*
 * Step through the list of backup vnodes. Delete any that don't have 
 * corresponding vnodes (i.e. matching uniquifiers) in the read/write volume.
 * Must be called from within a transaction.
 */
extern int MaxVnodesPerTransaction;

PRIVATE void deleteDeadVnode(rec_smolist *list,
			     VnodeDiskObject *vdop,
			     bit32 *nBackupVnodes)
{
    /* Make sure remove doesn't return null */
    assert(list->remove(&(vdop->nextvn)));

    /* decrement the reference count by one */
    if (vdop->inodeNumber) {
	if (vdop->type == vDirectory)
	    DDec((DirInode *)vdop->inodeNumber); 
    }
    
    /* Don't bother with the vnode free list */
    CAMLIB_REC_FREE((char *)strbase(VnodeDiskObject, &(vdop->nextvn), nextvn)); 
    
    /* decrement the vnode count on the volume. */
    (*nBackupVnodes)--;
}


PRIVATE void purgeDeadVnodes(Volume *backupvp, rec_smolist *BackupLists,
		      rec_smolist *RWLists, VnodeClass vclass, bit32 *nBackupVnodes)
{
    int status = 0;
    
    /* Set up an iterator for the backup vnodes. */
    struct VnodeClassInfo *vcp = &VnodeClassInfo_Array[vclass];
    vindex vol_index(V_id(backupvp), vclass, backupvp->device, vcp->diskSize);
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
	bzero((void *)DeadInodes, sizeof(Inode) * (MaxVnodesPerTransaction + 1));

	/* Bunch vnode operations into groups of at most 8 per transaction */
	/* Right now we might have transactions with no operations, oh well. */
	CAMLIB_BEGIN_TOP_LEVEL_TRANSACTION_2(CAM_TRAN_NV_SERVER_BASED)

	for (count = 0; count < MaxVnodesPerTransaction; count++) {

	    if ((vnodeIndex = vnext(bVnode)) == -1) 
		break;					/* No more vnodes. */

	    LogMsg(1, VolDebugLevel, tmpdebug, "Got bVnode (%08x.%x.%x)\n",
		   V_id(backupvp), bitNumberToVnodeNumber(vnodeIndex, vclass),
		   bVnode->uniquifier);

	    /* Find the rwvnode. If it doesn't exist, delete this vnode */
	    if (FindVnode(&RWLists[vnodeIndex], bVnode->uniquifier) == NULL) {
		/* Get a pointer to the backupVnode in rvm. */
		VnodeDiskObject *bvdop;
		bvdop = FindVnode(&BackupLists[vnodeIndex], bVnode->uniquifier);
		assert(bvdop);

		/* Delete the backup vnode. */
		LogMsg(9,VolDebugLevel,stdout,"purgeDeadVnodes: purging %x.%x.%x",
		       V_id(backupvp), bitNumberToVnodeNumber(vnodeIndex, vclass),
		       bvdop->uniquifier);
		
		LogMsg(1, VolDebugLevel, tmpdebug,"purgeDeadVnodes: purging %x.%x.%x",
		       V_id(backupvp), bitNumberToVnodeNumber(vnodeIndex, vclass),
		       bvdop->uniquifier);
		
		if (vclass == vSmall)
		    DeadInodes[count] = bvdop->inodeNumber;
		
		deleteDeadVnode(&BackupLists[vnodeIndex], bvdop, nBackupVnodes);
	    }
	}
	CAMLIB_END_TOP_LEVEL_TRANSACTION_2(CAM_PROT_TWO_PHASED, status)
	assert(status == 0);		/* Should never abort... */

	/* Now delete the inodes for the vnodes we already purged. */
	if (vclass == vSmall) 
	    for (int i = 0; i < count; i++) {
		/* Assume inodes for backup volumes are stored with
		 * the VolumeId of the original read/write volume.
		 */
		if (DeadInodes[i])
		    if (idec((int)V_device(backupvp), (int)DeadInodes[i],
			     V_parentId(backupvp)))
			LogMsg(0,0,stdout,"VolBackup: idec failed with %d",errno);
	    }	
	    
	PollAndYield();	/* Give someone else a chance */
    }

    free(DeadInodes);
    if (tmpdebug != stdout) fclose(tmpdebug);
}


/* Clone any new or changed vnodes in the RWVolume. We know a vnode is new
 * or changed because the clone bit isn't set.
 */

PRIVATE void updateBackupVnodes(Volume *rwvp, Volume *backupvp,
				rec_smolist *BackupLists, VnodeClass vclass, 
				bit32 *nBackupVnodes)
{
    int status = 0;

    /* setup an iterator for the read/write vnodes */
    struct VnodeClassInfo *vcp = &VnodeClassInfo_Array[vclass];
    vindex vol_index(V_id(rwvp), vclass, rwvp->device, vcp->diskSize);
    vindex_iterator next(vol_index);
    char buf[SIZEOF_LARGEDISKVNODE];
    VnodeDiskObject *rwVnode = (VnodeDiskObject *)buf;

    /*
     * The basic idea is to check to see if the rwVnode has changed with respect
     * to the backupVnode. There are two tests for this:
     * 1. If the cloned flag is no longer set, then the file's data has changed.
     * 2. If the VV flag has changed (but the cloned flag hasn't), the file's
     *	  meta data has changed (as in the case of a res ForceVV). In this case
     *    we don't need to play with the inode, just copy the vnode.
     */
    
    /* Only idec inodes after the transaction has commited in case of abort */
    Inode *DeadInodes = (Inode *)malloc((MaxVnodesPerTransaction + 1) * sizeof(Inode));

    int vnodeIndex = 0;
    while (vnodeIndex != -1) {
	int count = 0;

	bzero((void *)DeadInodes, sizeof(Inode) * (MaxVnodesPerTransaction + 1));

	CAMLIB_BEGIN_TOP_LEVEL_TRANSACTION_2(CAM_TRAN_NV_SERVER_BASED)

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
		    LogMsg(0,0,stdout, "VolBackup: RW and RO types don't match (%x.%x.%x) %d != %d\n",
			   V_id(rwvp),
			   bitNumberToVnodeNumber(vnodeIndex, vclass),
			   rwVnode->uniquifier,
			   rwVnode->type, bvdop->type);
		
		if (rwVnode->length != bvdop->length) // If this changes, COW should happen.
		    LogMsg(0,0,stdout, "VolBackup: RW and RO lengths don't match (%x.%x.%x) %d != %d\n",
			   V_id(rwvp),
			   bitNumberToVnodeNumber(vnodeIndex, vclass),
			   rwVnode->uniquifier,
			   rwVnode->length, bvdop->length);
		
		if (rwVnode->uniquifier != bvdop->uniquifier)
		    LogMsg(0,0,stdout, "VolBackup: RW and RO uniquifiers don't match (%x.%x.%x) %d != %d\n",
			   V_id(rwvp),
			   bitNumberToVnodeNumber(vnodeIndex, vclass),
			   rwVnode->uniquifier,
			   rwVnode->uniquifier, bvdop->uniquifier);
		
		if (rwVnode->dataVersion != bvdop->dataVersion)
		    LogMsg(0,0,stdout, "VolBackup: RW and RO dataVersions don't match (%x.%x.%x) %d != %d\n",
			   V_id(rwvp),
			   bitNumberToVnodeNumber(vnodeIndex, vclass),
			   rwVnode->uniquifier,
			   rwVnode->dataVersion, bvdop->dataVersion);
		
		if (rwVnode->inodeNumber != bvdop->inodeNumber) {
		    /* This can't change without a COW!!! */
		    assert(IsBarren(rwVnode->versionvector));
		    assert(bvdop->inodeNumber);	/* RO vnode Can't be barren */
		}		    


		assert(bvdop->vol_index != rwVnode->vol_index);
		
		if (rwVnode->vnodeMagic != bvdop->vnodeMagic)
		    LogMsg(0,0,stdout, "VolBackup: RW and RO vnodeMagics don't match (%x.%x.%x) %d != %d\n",
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
		    
		    LogMsg(9,VolDebugLevel,stdout,
			   "VolBackup: RW Vnode (%08x.%x.%x) metadata changed, recloning.\n",
			   V_id(rwvp), bitNumberToVnodeNumber(vnodeIndex, vclass),
			   rwVnode->uniquifier);

		    /* Save some fields from the backup vnode */
		    rwVnode->cloned = 0;	// backupVnodes can't be cloned.
		    rwVnode->versionvector.Flags = 0; // R/O vnodes shouldn't be inconsistent.
		    rwVnode->vol_index = bvdop->vol_index;
		    rwVnode->lock = bvdop->lock;	// Use the backupVnode's lock
		    rwVnode->nextvn = bvdop->nextvn;

		    CAMLIB_MODIFY_BYTES(bvdop, rwVnode, (vclass == vLarge) ?
					SIZEOF_LARGEDISKVNODE : SIZEOF_SMALLDISKVNODE);
		}

		continue;
	    }

	    if (bvdop) {
		/* rwVnode must have been modified */
		/* This test only works on replicated volumes. Should I make it
		 * at all or should I test for replicated first? -dcs 2/22/92
		 */
/*	assert(VV_Cmp(&rwVnode->versionvector, &bvdop->versionvector)!= VV_EQ); */

		/* Delete the backup vnode rather than modify it. */
		LogMsg(9, VolDebugLevel, stdout,
		       "UpdateBackupVnode: purging %x.%x.%x",
		       V_id(backupvp), bitNumberToVnodeNumber(vnodeIndex, vclass),
		       bvdop->uniquifier);

		/* Directory Inodes are in RVM so DDecs get undone on abort. */
		if (vclass == vSmall)	
		    DeadInodes[count] = bvdop->inodeNumber;
		
		deleteDeadVnode(&BackupLists[vnodeIndex], bvdop, nBackupVnodes);
	    }

	    /* Create a new clone of the vnode. */
	    LogMsg(9, VolDebugLevel, stdout,
		   "UpdateBackupVnode: cloning %x.%x.%x", V_id(backupvp),
		   bitNumberToVnodeNumber(vnodeIndex, vclass),
		   rwVnode->uniquifier);
	    CloneVnode(rwvp, backupvp, vnodeIndex, BackupLists, rwVnode, vclass);
	    (*nBackupVnodes)++;
	    
	} /* Inner loop -> less than MaxVnodesPerTransaction times around */
	CAMLIB_END_TOP_LEVEL_TRANSACTION_2(CAM_PROT_TWO_PHASED, status)
	assert(status == 0);  /* Do we ever abort? */

	/* Now delete the inodes for the vnodes we already purged. */
	if (vclass == vSmall) 
	    for (int i = 0; i < count; i++) {
		/* Assume inodes for backup volumes are stored with
		 * the VolumeId of the original read/write volume.
		 */
		if (DeadInodes[i])
		    if (idec((int)V_device(backupvp), (int)DeadInodes[i],
			     V_parentId(backupvp)))
			LogMsg(0, 0, stdout,
			       "VolBackup: idec failed with %d", errno);
	    }	
	
	PollAndYield();	/* Give someone else a chance */
    }

    free(DeadInodes);
}


