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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/vol/recova.cc,v 4.8 1998/08/26 21:22:26 braam Exp $";
#endif /*_BLURB_*/








/*
 * recova.c:
 * Routines for accessing volume abstractions in recoverable storage
 */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <sys/time.h>
#include <string.h>
#include <stdio.h>
#include <setjmp.h>
#include <inodeops.h>
#include <unistd.h>
#include <stdlib.h>
#include <struct.h>
#include <lwp.h>
#include <lock.h>
#include <util.h>
#include <rvmlib.h>
#include <vice.h>
#include <util.h>
#include <codadir.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <rec_smolist.h>
#include "cvnode.h"
#include "volume.h"
#include "vutil.h"
#include "recov.h"
#include "camprivate.h"
#include "coda_globals.h"
#include "volhash.h"

extern void PrintCamVolume(int, int);
extern void DeleteVolumeFromHashTable(Volume *);
extern void PollAndYield();

static int DeleteVolHeader(int);
static int DeleteVolData(int);
static int DeleteVnodes(unsigned int, Device, VnodeClass);

/* Number of vnodes to delete per transaction. */
static int MaxVnodesPerTransaction = 8;

/* Find an empty slot to store a new volume header. Returns the index */
/* for the new volume, or -1 if all volume header slots are taken */
/* Dynamically allocates storage for the components of the VolumeData structure */
/* NOTE: this information will not change for the life of the volume on this server */
int NewVolHeader(struct VolumeHeader *header, Error *err)
{
    int i;
    struct VolumeData data;
    VolumeDiskData tmpinfo;
    char tmp_svnodes[sizeof(rec_smolist) * SMALLGROWSIZE];
    char tmp_lvnodes[sizeof(rec_smolist) * LARGEGROWSIZE];

    VLog(9,  "Entering NewVolHeader");
    if (HashLookup(header->id) != -1) {
	*err = VVOLEXISTS;
	return -1;
    }

    while(1) {
	for(i = 0; i < MAXVOLS; i++) {
	    if (SRV_RVM(VolumeList[i]).header.stamp.magic != VOLUMEHEADERMAGIC) {
		break;
	    }
	}
	if (i >= MAXVOLS) {
	    *err = VNOVOL;
	    VLog(0,  "NewVolHeader: No free volume slots in recoverable storage!!");
	    rvmlib_abort (VFAIL);
	}

    VLog(29,  "NewVolHeader: found empty slot %d", i);

	/* if someone else already grabbed the slot, release the lock and try again */
	if (SRV_RVM(VolumeList[i]).header.stamp.magic == VOLUMEHEADERMAGIC) {
	    VLog(0,  "NewVolHeader: releasing locks on slot %d", i);
	    continue;
	}
	else {
	    break;
	}
    }
    VLog(9,  "NewVolHeader: Going to stamp new header ");
    /* initialize header version stamp */
    header->stamp.magic = VOLUMEHEADERMAGIC;
    header->stamp.version = VOLUMEHEADERVERSION;

    /* Dynamically allocate the volumeDiskData */
    VLog(9,  "NewVolHeader: Going to Allocate VolumeDiskData ");
    bzero((void *)&data, sizeof(data));
    data.volumeInfo = (VolumeDiskData *)rvmlib_rec_malloc(sizeof(VolumeDiskData));
    /* zero out the allocated memory */
    bzero((char *)&tmpinfo, sizeof(tmpinfo));
    rvmlib_modify_bytes(data.volumeInfo, &tmpinfo, sizeof(tmpinfo));

    VLog(9,  "NewVolHeader: Going to allocate vnode arrays");
    /* Dynamically allocate the small vnode array and zero it out */
    data.smallVnodeLists = (rec_smolist *)
            rvmlib_rec_malloc(sizeof(rec_smolist) * SMALLGROWSIZE);
    bzero(tmp_svnodes, sizeof(tmp_svnodes));
    VLog(9,  "NewVolHeader: Zeroing out small vnode array of size %d", 
	 sizeof(tmp_svnodes));
    rvmlib_modify_bytes(data.smallVnodeLists, tmp_svnodes, sizeof(tmp_svnodes));
    data.nsmallvnodes = 0;
    data.nsmallLists = SMALLGROWSIZE;

    /* Dynamically allocate the large vnode array */
    data.largeVnodeLists = (rec_smolist *)
            rvmlib_rec_malloc(sizeof(rec_smolist) * LARGEGROWSIZE);
    bzero(tmp_lvnodes, sizeof(tmp_lvnodes));
    VLog(9,  "NewVolHeader: Zeroing out large vnode array of size %d",
	 sizeof(tmp_lvnodes));
    rvmlib_modify_bytes(data.largeVnodeLists, tmp_lvnodes, sizeof(tmp_lvnodes));
    data.nlargevnodes = 0;
    data.nlargeLists = LARGEGROWSIZE;

    /* write out new header and data in recoverable storage */
    VLog(9,  "NewVolHeader: Going to write header in recoverable storage ");
    rvmlib_modify_bytes(&(SRV_RVM(VolumeList[i]).header), header,
				    sizeof(struct VolumeHeader));
    rvmlib_modify_bytes(&(SRV_RVM(VolumeList[i]).data), &data,
				sizeof(struct VolumeData));

    VLog(29,  "NewVolHeader: adding new entry %x to slot %d", header->id, i);
    HashInsert(header->id, i);	    // add new entry to volume hash table
    assert(HashLookup(header->id) == i);
    PrintCamVolume(29, i);

    return(i);
}
/* Get a volume header from recoverable storage */
/* corresponding with the given volume id */
/* Returns 0 if successful, -1 otherwise */
int ExtractVolHeader(VolumeId volid, struct VolumeHeader *header)
{
    int myind = HashLookup(volid);

    VLog(9,  "Entering ExtractVolHeader for volume %x", volid);
    if (myind == -1) return (-1);  /* volume not found */
    return(VolHeaderByIndex(myind, header));
}

/* Get a volume header from recoverable storage given the appropriate index */
/* Returns 0 if successful, -1 otherwise */
int VolHeaderByIndex(int myind, struct VolumeHeader *header) {
    VolumeId maxid = 0;
    int status = 0;	/* transaction status variable */

    VLog(9,  "Entering VolHeaderByIndex for index %d", myind);

    maxid = (SRV_RVM(MaxVolId) & 0x00FFFFFF);
    if ((myind < 0) || (myind >= maxid) || (myind >= MAXVOLS)) {
	VLog(1,  "VolHeaderByIndex: bogus volume index %d - maxid %d (ok if volume was purged or deleted)", myind, maxid);
	return(-1);
    }
    bcopy((const void *)&(SRV_RVM(VolumeList[myind]).header), (void *) header,
					    sizeof(struct VolumeHeader));
    if (header->stamp.magic != VOLUMEHEADERMAGIC) {
	VLog(19,  "VolHeaderByIndex: stamp.magic = %u, VHMAGIC = %u",
		header->stamp.magic, VOLUMEHEADERMAGIC);
    }
    if (header->stamp.version != VOLUMEHEADERVERSION) {
	VLog(19,  "VolHeaderByIndex: stamp.version = %u, VHVERSION = %u",
		header->stamp.version, VOLUMEHEADERVERSION);
    }

    return(0);
}

/* remove a volume from recoverable storage. Free any allocated storage and */
/* zero out the slot in the recoverable volume list. Returns -1 if volume not */
/* found, 0 otherwise */
int DeleteVolume(Volume *vp) {
    int status = 0;

    VLog(9,  "Entering DeleteVolume for volume %x", V_id(vp));
    unsigned int myind = V_volumeindex(vp);
    Device dev = V_device(vp);

    HashDelete(V_id(vp));		/* remove volume from volume hash table */
    FreeVolume(vp);			/* Clear up all vm traces. */
    
    /* Mark the volume for destruction. */
    RVMLIB_BEGIN_TRANSACTION(restore)
    RVMLIB_MODIFY(SRV_RVM(VolumeList[myind]).data.volumeInfo->destroyMe,
		  DESTROY_ME);
    RVMLIB_MODIFY(SRV_RVM(VolumeList[myind]).data.volumeInfo->blessed,
		  0);
    RVMLIB_END_TRANSACTION(flush, &(status));
    if (status) return status;
    
    if ((status = DeleteVnodes(myind, dev, vSmall)) != 0) return status;
    if ((status = DeleteVnodes(myind, dev, vLarge)) != 0) return status;
    if ((status = DeleteVolData(myind)) != 0) return status;
    if ((status = DeleteVolHeader(myind)) != 0) return status;

    PrintCamVolume(29, myind);

    return(0);
}

/* The salvager doesn't have access to the vp, so need another hook */
int DeleteRvmVolume(unsigned int myind, Device dev) {
    int status = 0;

    VLog(9,  "Entering DeleteRvmVolume for volume %x", SRV_RVM(VolumeList[myind].header.id));

    if ((status = DeleteVnodes(myind, dev, vSmall)) != 0) return status;
    if ((status = DeleteVnodes(myind, dev, vLarge)) != 0) return status;
    if ((status = DeleteVolData(myind)) != 0) return status;
    if ((status = DeleteVolHeader(myind)) != 0) return status;

    PrintCamVolume(29, myind);

    return(0);
}

/* This routine should remove all the vnodes for a volume. To avoid long
 * running transactions, this operation spans many transactions. Although
 * atomicity is lost, the volume should never get into an illegal state.
 * In addition, it is assumed that this volume is marked for salvaging
 * so on startup it would be deleted anyway.
 */
static int DeleteVnodes(unsigned int myind, Device dev, VnodeClass vclass) 
{
    struct VnodeClassInfo *vcp = &VnodeClassInfo_Array[vclass];
    char zerobuf[SIZEOF_LARGEDISKVNODE];
    struct VnodeDiskObject *zerovn = (struct VnodeDiskObject *) zerobuf;
    struct VolumeData *vdata;
    rec_smolist *vnlist;
    int status = 0;
    bit32 nLists;


    assert( (myind >= 0) && (myind < MAXVOLS) );
    vdata = &(SRV_RVM(VolumeList[myind]).data);    

    if (vclass == vSmall) {
	vnlist = vdata->smallVnodeLists;
	nLists = vdata->nsmallLists;
    } else {	/* Large */
	vnlist = vdata->largeVnodeLists;
	nLists = vdata->nlargeLists;
    }

    /* Check integrity of volume. */
    if (vnlist == NULL) {
	VLog(0,  "Volume to be deleted didn't have a %s VnodeIndex.",
	    (vclass == vSmall) ? "small" : "large");
	return 0;
    }
    if (vdata->volumeInfo == NULL) return -1; /* WRONG! no VolumeDiskData! */
    
    bzero((void *)zerovn, SIZEOF_LARGEDISKVNODE);

    int i = 0;
    rec_smolink *p;
    VnodeDiskObject *vdo;
    int moreVnodes = TRUE;

    /* Only idec inodes after the transaction has committed in case of abort */
    /* I assume an Inode is the same basic type as an int (e.g. a pointer) */
    int *DeadInodes = (int*)malloc((MaxVnodesPerTransaction + 1) * sizeof(Inode));

    while (moreVnodes) {
	int count = 0;
	RVMLIB_BEGIN_TRANSACTION(restore)

	bzero((void *)DeadInodes, sizeof(Inode) * (MaxVnodesPerTransaction + 1));

	while (count < MaxVnodesPerTransaction) {
	    p = vnlist[i].get();	/* Pull the vnode off the list. */

	    if (p == NULL) {
		if (++i < nLists) 
		    continue;

		moreVnodes = FALSE;
		break;
	    }
	    count++;
	    vdo = strbase(VnodeDiskObject, p, nextvn);
	    
	    if ((vdo->type != vNull) && (vdo->vnodeMagic != vcp->magic)){
		VLog(0, "DeleteVnodes:VnodeMagic field incorrect for vnode %d",i);
		assert(0);
	    }

	    if (vdo->inodeNumber){
		/* decrement the reference count by one */
		if (vdo->type != vDirectory) {
		    assert(vclass == vSmall);
		    DeadInodes[count] = (int)vdo->inodeNumber; // Delay the idec.
		} else 
		    DI_Dec((DirInode *)vdo->inodeNumber);
	    }	

	    /* Delete the vnode */
	    if ((vclass == vSmall) &&
	        (SRV_RVM(SmallVnodeIndex) < SMALLFREESIZE - 1)) {
		VLog(29, "DeleteVnodes: Adding small vnode index %d to free list", i);
		rvmlib_modify_bytes(vdo, zerovn, SIZEOF_SMALLDISKVNODE);
		RVMLIB_MODIFY(SRV_RVM(SmallVnodeIndex),
			      SRV_RVM(SmallVnodeIndex) + 1);
		RVMLIB_MODIFY(SRV_RVM(SmallVnodeFreeList[SRV_RVM(SmallVnodeIndex)]), vdo);
	    }  else if ((vclass == vLarge) &&
			(SRV_RVM(LargeVnodeIndex) < LARGEFREESIZE - 1)) {
		VLog(29, 	"DeleteVnodes:	Adding large vnode index %d to free list",i);
		rvmlib_modify_bytes(vdo, zerovn, SIZEOF_LARGEDISKVNODE);
		RVMLIB_MODIFY(SRV_RVM(LargeVnodeIndex),
			      SRV_RVM(LargeVnodeIndex) + 1);
		RVMLIB_MODIFY(SRV_RVM(LargeVnodeFreeList[SRV_RVM(LargeVnodeIndex)]), vdo);
	    } else {
		rvmlib_rec_free((char *)vdo);
		VLog(29,  "DeleteVnodes: Freeing small vnode index %d", i);
	    }
	}	    

	RVMLIB_END_TRANSACTION(flush, &(status));
	assert(status == 0);		/* Should never abort... */

	/* Now delete the inodes for the vnodes we already purged. */
	if (vclass == vSmall) {
	    for (int j = 0; j < count; j++) {
		/* Assume inodes for backup volumes are stored with
		 * the VolumeId of the original read/write volume.
		 */
		if (DeadInodes[j])
		    if (idec((int)dev, DeadInodes[j], (vdata->volumeInfo)->parentId))
			VLog(0, 0, stdout,
			       "VolBackup: idec failed with %d", errno);
	    }	
	}
	
	VLog(9,  "DeleteVnodes: finished deleting %d %s vnodes", count,
	    (vclass == vLarge?"Large":"Small"));
	PollAndYield();
    }

    free(DeadInodes);

    VLog(9,  "DeleteVnodes: Deleted all %s vnodes.",
	(vclass==vLarge?"Large":"Small"));

    /* free the empty array (of pointers) for the vnodes */
    RVMLIB_BEGIN_TRANSACTION(restore)
    if (vclass == vSmall) {
       rvmlib_rec_free((char *)SRV_RVM(VolumeList[myind]).data.smallVnodeLists);
       RVMLIB_MODIFY(SRV_RVM(VolumeList[myind]).data.smallVnodeLists, NULL);

    } else {
       rvmlib_rec_free((char *)SRV_RVM(VolumeList[myind]).data.largeVnodeLists);
       RVMLIB_MODIFY(SRV_RVM(VolumeList[myind]).data.largeVnodeLists, NULL);
    }

    RVMLIB_END_TRANSACTION(flush, &(status));
    assert(status == 0);		/* Should never abort... */
    
    return 0;
}

/* zero out the VolumeData structure at the given index in recoverable storage */
/* We assume the vnodes for the volume have already been freed. */
/* Note: this is a heavyweight operation with lots of modifies to rvm */
static int DeleteVolData(int myind) {
    struct VolumeData *vdata, tmpdata;
    int status = 0;
    VLog(9,  "Entering DeleteVolData for index %d", myind);

    RVMLIB_BEGIN_TRANSACTION(restore)
    
    /* make some abbreviations */
    vdata = &(SRV_RVM(VolumeList[myind]).data);

    /* dynamically free the volume disk data structure */
    VLog(9,  "DeleteVolData: Freeing VolumeDiskObject for volume 0x%x",
	 vdata->volumeInfo->id);
    rvmlib_rec_free((char *)vdata->volumeInfo);

    /* zero out VolumeData structure */
    VLog(9,  "DeleteVolData: Zeroing out VolumeData at index %d", myind);
    bzero((void *)&tmpdata, sizeof(struct VolumeData));
    rvmlib_modify_bytes(&(SRV_RVM(VolumeList[myind]).data), &tmpdata,
				sizeof(struct VolumeData));

    RVMLIB_END_TRANSACTION(flush, &(status));
    
    VLog(9,  "Leaving DeleteVolData()");
    return status;
}

/* zero out the Volume Header at the specified index in recoverable storage */
/* since this is PRIVATE, we assume the index has already been checked for validity */
/* and that the routine is wrapped in a transaction by the caller */
static int DeleteVolHeader(int myind) {
    VolumeHeader tmpheader;
    int status = 0;
    
    VLog(9,  "Entering DeleteVolHeader for index %d", myind);

    RVMLIB_BEGIN_TRANSACTION(restore)
	/* Sanity check */
	assert(SRV_RVM(VolumeList[myind]).header.stamp.magic
	       == VOLUMEHEADERMAGIC);
	bzero((void *)&tmpheader, sizeof(struct VolumeHeader));
        RVMLIB_MODIFY(SRV_RVM(VolumeList[myind]).header, tmpheader);
    RVMLIB_END_TRANSACTION(flush, &(status));
    return status;
}
    



