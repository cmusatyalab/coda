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

static char *rcsid = "$Header: /afs/cs.cmu.edu/project/coda-braam/src/coda-4.0.1/coda-src/vol/RCS/recova.cc,v 1.1 1996/11/22 19:10:28 braam Exp $";
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
#ifdef __MACH__
#include <libc.h>
#include <sysent.h>
#endif /* __MACH__ */
#if defined(__linux__) || defined(__NetBSD__)
#include <unistd.h>
#include <stdlib.h>
#endif __NetBSD__
#include <struct.h>
#include <lwp.h>
#include <lock.h>

#ifdef __MACH__
#include <mach.h>
#endif /* __MACH__ */
#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>

#include <rvmlib.h>
#include <vice.h>
#include <callback.h>
#include <util.h>
#include <rec_smolist.h>
#include "cvnode.h"
#include "volume.h"
#include "vutil.h"
#include "recov.h"
#include "rvmdir.h"
#include "camprivate.h"
#include "coda_globals.h"
#include "volhash.h"

extern void PrintCamVolume(int, int);
extern void DeleteVolumeFromHashTable(Volume *);
extern void PollAndYield();

PRIVATE int DeleteVolHeader(int);
PRIVATE int DeleteVolData(int);
PRIVATE int DeleteVnodes(unsigned int, Device, VnodeClass);

/* Number of vnodes to delete per transaction. */
PRIVATE int MaxVnodesPerTransaction = 8;

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

    LogMsg(9, VolDebugLevel, stdout,  "Entering NewVolHeader");
    if (HashLookup(header->id) != -1) {
	*err = VVOLEXISTS;
	return -1;
    }

    while(1) {
	for(i = 0; i < MAXVOLS; i++) {
	    if (CAMLIB_REC(VolumeList[i]).header.stamp.magic != VOLUMEHEADERMAGIC) {
		break;
	    }
	}
	if (i >= MAXVOLS) {
	    *err = VNOVOL;
	    LogMsg(0, VolDebugLevel, stdout,  "NewVolHeader: No free volume slots in recoverable storage!!");
	    CAMLIB_ABORT (VFAIL);
	}

    LogMsg(29, VolDebugLevel, stdout,  "NewVolHeader: found empty slot %d", i);
	CAMLIB_LOCK(CAMLIB_LOCK_NAME(CAMLIB_REC(VolumeList[i])), CLS, CAMLIB_LOCK_MODE_WRITE);

	/* if someone else already grabbed the slot, release the lock and try again */
	if (CAMLIB_REC(VolumeList[i]).header.stamp.magic == VOLUMEHEADERMAGIC) {
	    LogMsg(0, VolDebugLevel, stdout,  "NewVolHeader: releasing locks on slot %d", i);
	    CAMLIB_UNLOCK(CAMLIB_LOCK_NAME(CAMLIB_REC(VolumeList[i])), CLS);
	    continue;
	}
	else {
	    break;
	}
    }
    LogMsg(9, VolDebugLevel, stdout,  "NewVolHeader: Going to stamp new header ");
    /* initialize header version stamp */
    header->stamp.magic = VOLUMEHEADERMAGIC;
    header->stamp.version = VOLUMEHEADERVERSION;

    /* Dynamically allocate the volumeDiskData */
    LogMsg(9, VolDebugLevel, stdout,  "NewVolHeader: Going to Allocate VolumeDiskData ");
    bzero(&data, sizeof(data));
    data.volumeInfo = (VolumeDiskData *)CAMLIB_REC_MALLOC(sizeof(VolumeDiskData));
    /* zero out the allocated memory */
    bzero((char *)&tmpinfo, sizeof(tmpinfo));
    CAMLIB_MODIFY_BYTES(data.volumeInfo, &tmpinfo, sizeof(tmpinfo));

    LogMsg(9, VolDebugLevel, stdout,  "NewVolHeader: Going to allocate vnode arrays");
    /* Dynamically allocate the small vnode array and zero it out */
    data.smallVnodeLists = (rec_smolist *)
            CAMLIB_REC_MALLOC(sizeof(rec_smolist) * SMALLGROWSIZE);
    bzero(tmp_svnodes, sizeof(tmp_svnodes));
    LogMsg(9, VolDebugLevel, stdout,  "NewVolHeader: Zeroing out small vnode array of size %d", 
	 sizeof(tmp_svnodes));
    CAMLIB_MODIFY_BYTES(data.smallVnodeLists, tmp_svnodes, sizeof(tmp_svnodes));
    data.nsmallvnodes = 0;
    data.nsmallLists = SMALLGROWSIZE;

    /* Dynamically allocate the large vnode array */
    data.largeVnodeLists = (rec_smolist *)
            CAMLIB_REC_MALLOC(sizeof(rec_smolist) * LARGEGROWSIZE);
    bzero(tmp_lvnodes, sizeof(tmp_lvnodes));
    LogMsg(9, VolDebugLevel, stdout,  "NewVolHeader: Zeroing out large vnode array of size %d",
	 sizeof(tmp_lvnodes));
    CAMLIB_MODIFY_BYTES(data.largeVnodeLists, tmp_lvnodes, sizeof(tmp_lvnodes));
    data.nlargevnodes = 0;
    data.nlargeLists = LARGEGROWSIZE;

    /* write out new header and data in recoverable storage */
    LogMsg(9, VolDebugLevel, stdout,  "NewVolHeader: Going to write header in recoverable storage ");
    CAMLIB_MODIFY_BYTES(&(CAMLIB_REC(VolumeList[i]).header), header,
				    sizeof(struct VolumeHeader));
    CAMLIB_MODIFY_BYTES(&(CAMLIB_REC(VolumeList[i]).data), &data,
				sizeof(struct VolumeData));

    LogMsg(29, VolDebugLevel, stdout,  "NewVolHeader: adding new entry %x to slot %d", header->id, i);
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

    LogMsg(9, VolDebugLevel, stdout,  "Entering ExtractVolHeader for volume %x", volid);
    if (myind == -1) return (-1);  /* volume not found */
    return(VolHeaderByIndex(myind, header));
}

/* Get a volume header from recoverable storage given the appropriate index */
/* Returns 0 if successful, -1 otherwise */
int VolHeaderByIndex(int myind, struct VolumeHeader *header) {
    VolumeId maxid = 0;
    int status = 0;	/* transaction status variable */

    LogMsg(9, VolDebugLevel, stdout,  "Entering VolHeaderByIndex for index %d", myind);

    maxid = (CAMLIB_REC(MaxVolId) & 0x00FFFFFF);
    if ((myind < 0) || (myind >= maxid) || (myind >= MAXVOLS)) {
	LogMsg(1, VolDebugLevel, stdout,  "VolHeaderByIndex: bogus volume index %d", myind);
	return(-1);
    }
    bcopy(&(CAMLIB_REC(VolumeList[myind]).header), header,
					    sizeof(struct VolumeHeader));
    if (header->stamp.magic != VOLUMEHEADERMAGIC) {
	LogMsg(19, VolDebugLevel, stdout,  "VolHeaderByIndex: stamp.magic = %u, VHMAGIC = %u",
		header->stamp.magic, VOLUMEHEADERMAGIC);
    }
    if (header->stamp.version != VOLUMEHEADERVERSION) {
	LogMsg(19, VolDebugLevel, stdout,  "VolHeaderByIndex: stamp.version = %u, VHVERSION = %u",
		header->stamp.version, VOLUMEHEADERVERSION);
    }

    return(0);
}

/* remove a volume from recoverable storage. Free any allocated storage and */
/* zero out the slot in the recoverable volume list. Returns -1 if volume not */
/* found, 0 otherwise */
int DeleteVolume(Volume *vp) {
    int status = 0;

    LogMsg(9, VolDebugLevel, stdout,  "Entering DeleteVolume for volume %x", V_id(vp));
    unsigned int myind = V_volumeindex(vp);
    Device dev = V_device(vp);

    HashDelete(V_id(vp));		/* remove volume from volume hash table */
    FreeVolume(vp);			/* Clear up all vm traces. */
    
    /* Mark the volume for destruction. */
    CAMLIB_BEGIN_TOP_LEVEL_TRANSACTION_2(CAM_TRAN_NV_SERVER_BASED)
    CAMLIB_MODIFY(CAMLIB_REC(VolumeList[myind]).data.volumeInfo->destroyMe,
		  DESTROY_ME);
    CAMLIB_MODIFY(CAMLIB_REC(VolumeList[myind]).data.volumeInfo->blessed,
		  0);
    CAMLIB_END_TOP_LEVEL_TRANSACTION_2(CAM_PROT_TWO_PHASED, status)
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

    LogMsg(9, VolDebugLevel, stdout,  "Entering DeleteRvmVolume for volume %x", CAMLIB_REC(VolumeList[myind].header.id));

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
PRIVATE int DeleteVnodes(unsigned int myind, Device dev, VnodeClass vclass) {
    struct VnodeClassInfo *vcp = &VnodeClassInfo_Array[vclass];
    char zerobuf[SIZEOF_LARGEDISKVNODE];
    struct VnodeDiskObject *zerovn = (struct VnodeDiskObject *) zerobuf;
    struct VolumeData *vdata;
    rec_smolist *vnlist;
    int status = 0;
    bit32 nLists;

    vdata = &(CAMLIB_REC(VolumeList[myind]).data);    
    if (vclass == vSmall) {
	vnlist = vdata->smallVnodeLists;
	nLists = vdata->nsmallLists;
    } else {	/* Large */
	vnlist = vdata->largeVnodeLists;
	nLists = vdata->nlargeLists;
    }

    /* Check integrity of volume. */
    if (vnlist == NULL) {
	LogMsg(0, VolDebugLevel, stdout,  "Volume to be deleted didn't have a %s VnodeIndex.",
	    (vclass == vSmall) ? "small" : "large");
	return 0;
    }
    if (vdata->volumeInfo == NULL) return -1; /* WRONG! no VolumeDiskData! */
    
    bzero(zerovn, SIZEOF_LARGEDISKVNODE);

    int i = 0;
    rec_smolink *p;
    VnodeDiskObject *vdo;
    int moreVnodes = TRUE;

    /* Only idec inodes after the transaction has commited in case of abort */
    /* I assume an Inode is the same basic type as an int (e.g. a pointer) */
    int *DeadInodes = (int*)malloc((MaxVnodesPerTransaction + 1) * sizeof(Inode));

    while (moreVnodes) {
	int count = 0;
	CAMLIB_BEGIN_TOP_LEVEL_TRANSACTION_2(CAM_TRAN_NV_SERVER_BASED)

	bzero(DeadInodes, sizeof(Inode) * (MaxVnodesPerTransaction + 1));

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
		LogMsg(0, VolDebugLevel, stdout, "DeleteVnodes:VnodeMagic field incorrect for vnode %d",i);
		assert(0);
	    }

	    if (vdo->inodeNumber){
		/* decrement the reference count by one */
		if (vdo->type != vDirectory) {
		    assert(vclass == vSmall);
		    DeadInodes[count] = (int)vdo->inodeNumber; // Delay the idec.
		} else 
		    DDec((DirInode *)vdo->inodeNumber);
	    }	

	    /* Delete the vnode */
	    if ((vclass == vSmall) &&
	        (CAMLIB_REC(SmallVnodeIndex) < SMALLFREESIZE - 1)) {
		LogMsg(29, VolDebugLevel, stdout, "DeleteVnodes: Adding small vnode index %d to free list", i);
		CAMLIB_MODIFY_BYTES(vdo, zerovn, SIZEOF_SMALLDISKVNODE);
		CAMLIB_MODIFY(CAMLIB_REC(SmallVnodeIndex),
			      CAMLIB_REC(SmallVnodeIndex) + 1);
		CAMLIB_MODIFY(CAMLIB_REC(SmallVnodeFreeList[CAMLIB_REC(SmallVnodeIndex)]), vdo);
	    }
	    else if ((vclass == vLarge) &&
		     (CAMLIB_REC(LargeVnodeIndex) < LARGEFREESIZE - 1)) {
		LogMsg(29, VolDebugLevel, stdout, 	"DeleteVnodes:	Adding large vnode index %d to free list",i);
		CAMLIB_MODIFY_BYTES(vdo, zerovn, SIZEOF_LARGEDISKVNODE);
		CAMLIB_MODIFY(CAMLIB_REC(LargeVnodeIndex),
			      CAMLIB_REC(LargeVnodeIndex) + 1);
		CAMLIB_MODIFY(CAMLIB_REC(LargeVnodeFreeList[CAMLIB_REC(LargeVnodeIndex)]), vdo);
	    } else {
		CAMLIB_REC_FREE((char *)vdo);
		LogMsg(29, VolDebugLevel, stdout,  "DeleteVnodes: Freeing small vnode index %d", i);
	    }
	}	    

	CAMLIB_END_TOP_LEVEL_TRANSACTION_2(CAM_PROT_TWO_PHASED, status)
	assert(status == 0);		/* Should never abort... */

	/* Now delete the inodes for the vnodes we already purged. */
	if (vclass == vSmall) {
	    for (int j = 0; j < count; j++) {
		/* Assume inodes for backup volumes are stored with
		 * the VolumeId of the original read/write volume.
		 */
		if (DeadInodes[j])
		    if (idec((int)dev, DeadInodes[j], (vdata->volumeInfo)->parentId))
			LogMsg(0, 0, stdout,
			       "VolBackup: idec failed with %d", errno);
	    }	
	}
	
	LogMsg(9, VolDebugLevel, stdout,  "DeleteVnodes: finished deleting %d %s vnodes", count,
	    (vclass == vLarge?"Large":"Small"));
	PollAndYield();
    }

    free(DeadInodes);

    LogMsg(9, VolDebugLevel, stdout,  "DeleteVnodes: Deleted all %s vnodes.",
	(vclass==vLarge?"Large":"Small"));

    /* free the empty array (of pointers) for the vnodes */
    CAMLIB_BEGIN_TOP_LEVEL_TRANSACTION_2(CAM_TRAN_NV_SERVER_BASED)
    if (vclass == vSmall) {
       CAMLIB_REC_FREE((char *)CAMLIB_REC(VolumeList[myind]).data.smallVnodeLists);
       CAMLIB_MODIFY(CAMLIB_REC(VolumeList[myind]).data.smallVnodeLists, NULL);

    } else {
       CAMLIB_REC_FREE((char *)CAMLIB_REC(VolumeList[myind]).data.largeVnodeLists);
       CAMLIB_MODIFY(CAMLIB_REC(VolumeList[myind]).data.largeVnodeLists, NULL);
    }

    CAMLIB_END_TOP_LEVEL_TRANSACTION_2(CAM_PROT_TWO_PHASED, status)
    assert(status == 0);		/* Should never abort... */
    
    return 0;
}

/* zero out the VolumeData structure at the given index in recoverable storage */
/* We assume the vnodes for the volume have already been freed. */
/* Note: this is a heavyweight operation with lots of modifies to rvm */
PRIVATE int DeleteVolData(int myind) {
    struct VolumeData *vdata, tmpdata;
    int status = 0;
    LogMsg(9, VolDebugLevel, stdout,  "Entering DeleteVolData for index %d", myind);

    CAMLIB_BEGIN_TOP_LEVEL_TRANSACTION_2(CAM_TRAN_NV_SERVER_BASED)
    
    /* make some abbreviations */
    vdata = &(CAMLIB_REC(VolumeList[myind]).data);

    /* dynamically free the volume disk data structure */
    LogMsg(9, VolDebugLevel, stdout,  "DeleteVolData: Freeing VolumeDiskObject for volume 0x%x",
	 vdata->volumeInfo->id);
    CAMLIB_REC_FREE((char *)vdata->volumeInfo);

    /* zero out VolumeData structure */
    LogMsg(9, VolDebugLevel, stdout,  "DeleteVolData: Zeroing out VolumeData at index %d", myind);
    bzero(&tmpdata, sizeof(struct VolumeData));
    CAMLIB_MODIFY_BYTES(&(CAMLIB_REC(VolumeList[myind]).data), &tmpdata,
				sizeof(struct VolumeData));

    CAMLIB_END_TOP_LEVEL_TRANSACTION_2(CAM_PROT_TWO_PHASED, status)
    
    LogMsg(9, VolDebugLevel, stdout,  "Leaving DeleteVolData()");
    return status;
}

/* zero out the Volume Header at the specified index in recoverable storage */
/* since this is PRIVATE, we assume the index has already been checked for validity */
/* and that the routine is wrapped in a transaction by the caller */
PRIVATE int DeleteVolHeader(int myind) {
    VolumeHeader tmpheader;
    int status = 0;
    
    LogMsg(9, VolDebugLevel, stdout,  "Entering DeleteVolHeader for index %d", myind);

    CAMLIB_BEGIN_TOP_LEVEL_TRANSACTION_2(CAM_TRAN_NV_SERVER_BASED)
	/* Sanity check */
	assert(CAMLIB_REC(VolumeList[myind]).header.stamp.magic
	       == VOLUMEHEADERMAGIC);
	bzero(&tmpheader, sizeof(struct VolumeHeader));
        CAMLIB_MODIFY(CAMLIB_REC(VolumeList[myind]).header, tmpheader);
    CAMLIB_END_TOP_LEVEL_TRANSACTION_2(CAM_PROT_TWO_PHASED, status)
    return status;
}
    



