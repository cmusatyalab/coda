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
                           none currently

#*/



/*
 * recovb.c:
 * Routines for accessing volume abstractions in recoverable storage
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/time.h>
#include "coda_string.h"
#include <stdio.h>
#include <setjmp.h>
#include <unistd.h>
#include <stdlib.h>

#include <struct.h>

#include <lwp/lwp.h>
#include <lwp/lock.h>
#include <rpc2/rpc2.h>
#include <util.h>

#include <rvmlib.h>
#ifdef __cplusplus
}
#endif

#include <vice.h>
#include <rec_smolist.h>
#include "cvnode.h"
#include "volume.h"
#include "vutil.h"
#include "recov.h"
#include "camprivate.h"
#include "coda_globals.h"
#include "volres.h"
#include "volhash.h"

extern void PrintCamVnode(int,int,int,VnodeId, Unique_t);
extern void print_VnodeDiskObject(VnodeDiskObject *);
extern void PrintCamDiskData(int,int,VolumeDiskData*);


static int DeleteVnode(int, int, VnodeId, Unique_t, VnodeDiskObject *);

/* Copy the specified vnode into the structure provided. Returns 0 if */
/* successful, -1 if an error occurs. */
int ExtractVnode(Error *ec, int volindex, int vclass, 
		  VnodeId vnodeindex, Unique_t uniquifier,
		  VnodeDiskObject *vnode)
{
	rec_smolist *vlist;
	VnodeDiskObject *vdo;
	VolumeId maxid;
	unsigned int size;

	VLog(9,  "Entering ExtractVnode(volindex = %d, vclass = %d, vnodeindex = %x, Unique = %x vnode = 0x%x)",
	       volindex, vclass, vnodeindex, uniquifier, vnode);

	*ec = 0;
	
	maxid = (SRV_RVM(MaxVolId) & 0x00FFFFFF);
	if (volindex < 0 || volindex > (int)maxid || volindex > MAXVOLS) {
		VLog(0,  "ExtractVnode: bogus volume index %d", volindex);
		return -1;
	}

	if (vclass == vSmall) {
	    if (vnodeindex >= SRV_RVM(VolumeList[volindex]).data.nsmallLists) {
		VLog(0,  "ExtractVnode: bogus small vnode index %d", vnodeindex);
		return -1;
	    }
	    vlist = &(SRV_RVM(VolumeList[volindex]).data.smallVnodeLists[vnodeindex]);
	    size = SIZEOF_SMALLDISKVNODE;
	} else {
	    if (vnodeindex >= SRV_RVM(VolumeList[volindex]).data.nlargeLists) {
		VLog(0,  "ExtractVnode: bogus large vnode index %d", vnodeindex);
		return -1;
	    }
	    vlist = &(SRV_RVM(VolumeList[volindex]).data.largeVnodeLists[vnodeindex]);
	    size = SIZEOF_LARGEDISKVNODE;
	}

	vdo = FindVnode(vlist, uniquifier);
	if (!vdo) {
	    VLog(9,  "ExtractVnode: NO object %x.%x", vnodeindex, uniquifier);
	    return -1;
	}

	memcpy(vnode, vdo, size);

	return 0;
}

/*
 * ObjectExists:
 * 	check if a vnode exists in a volume - 
 * 	return 1 if it does; 0 otherwise
 */
int ObjectExists(int volindex, int vclass, VnodeId vnodeindex, Unique_t u, 
		 ViceFid *ParentFid)
{
    rec_smolist *vlist;
    VolumeId maxid;
    VnodeDiskObject *vdo;

    VLog(9,  "Entering ObjectExists(volindex= %d, (%x.%x)",
	volindex, vnodeindex, u);

    /* check volume index */
    {    
	maxid = (SRV_RVM(MaxVolId) & 0x00FFFFFF);
	if (volindex < 0 || volindex > (int)maxid || volindex > MAXVOLS) {
	    VLog(0,  "ObjectExists: bogus volume index %d", volindex);
	    return 0;
	}
    }

    if (vclass == vSmall) {
	if (vnodeindex >= SRV_RVM(VolumeList[volindex]).data.nsmallLists) {
	    VLog(0,  "ObjectExists: bogus small vnode index %d", vnodeindex);
	    return 0;
	}
	vlist = &(SRV_RVM(VolumeList[volindex]).data.smallVnodeLists[vnodeindex]);
    } else {
	if (vnodeindex >= SRV_RVM(VolumeList[volindex]).data.nlargeLists) {
	    VLog(0,  "ObjectExists: bogus large vnode index %d", vnodeindex);
	    return 0;
	}
	vlist = &(SRV_RVM(VolumeList[volindex]).data.largeVnodeLists[vnodeindex]);
    }

    vdo = FindVnode(vlist, u);
    if (!vdo) {
	VLog(9,  "ObjectExists: NO object %x.%x", vnodeindex, u);
	return 0;
    }

    if (ParentFid) {
	if (vdo->uparent != 0) {
	    ParentFid->Vnode = vdo->vparent;
	    ParentFid->Unique = vdo->uparent;
	} else {
	    /* root vnode */
	    CODA_ASSERT(vclass == vLarge);
	    ParentFid->Vnode = bitNumberToVnodeNumber(vnodeindex, vLarge);
	    ParentFid->Unique = u;
	}
    }
    return 1;
}

/* Get fid of parent of a given fid - the child fid exists
 * This violates locking - but we are assuming this is called only from 
 * resolution where the volume is locked.  So no mutations can occur.
 */
int GetParentFid(Volume *vp, ViceFid *cFid, ViceFid *pFid)
{
    rec_smolist *vlist;
    VolumeId maxid;
    VnodeDiskObject *vdo;

    VLog(9,  "Entering GetParentFid(%x.%x)", cFid->Vnode, cFid->Unique);

    int volindex = V_volumeindex(vp);
    /* check volume index */
    {    
	maxid = (SRV_RVM(MaxVolId) & 0x00FFFFFF);
	if (volindex < 0 || volindex > (int)maxid || volindex > MAXVOLS) {
	    VLog(0,  "GetParentFid: bogus volume index %d", volindex);
	    return 0;
	}
    }

    unsigned long vclass = vnodeIdToClass(cFid->Vnode);
    unsigned long vnodeindex = vnodeIdToBitNumber(cFid->Vnode);
    if (vclass == vSmall) {
	if (vnodeindex >= SRV_RVM(VolumeList[volindex]).data.nsmallLists) {
	    VLog(0,  "GetParentFid: bogus small vnode index %x", vnodeindex);
	    return 0;
	}
	vlist = &(SRV_RVM(VolumeList[volindex]).data.smallVnodeLists[vnodeindex]);
    } else {
	if (vnodeindex >= SRV_RVM(VolumeList[volindex]).data.nlargeLists) {
	    VLog(0,  "GetParentFid: bogus large vnode index %x", vnodeindex);
	    return 0;
	}
	vlist = &(SRV_RVM(VolumeList[volindex]).data.largeVnodeLists[vnodeindex]);
    }

    vdo = FindVnode(vlist, cFid->Unique);
    if (!vdo) {
	VLog(9,  "GetParentFid: NO object %x.%x", cFid->Vnode, cFid->Unique);
	return 0;
    }

    if (vdo->uparent != 0) {
	pFid->Volume = cFid->Volume;
	pFid->Vnode = vdo->vparent;
	pFid->Unique = vdo->uparent;
    } else {
	/* root vnode */
	CODA_ASSERT(vclass == vLarge);
	*pFid = *cFid;
    }

    return 1;
}

int ReplaceVnode(int volindex, int vclass, VnodeId vnodeindex, 
		 Unique_t u, VnodeDiskObject *vnode)
{
    VolumeId maxid = 0;
    rec_smolist *vlist;
    bit32 *nvnodes;
    unsigned int size;
    char *name;

    VLog(9,  "Entering ReplaceVnode(%u, %u, %u, %ld)", volindex, vclass,
			    vnodeindex, vnode);
    /* if it's been zeroed out, delete the slot */
    if (vnode->type == vNull) {
	VLog(9,  "ReplaceVnode: bogus vnode %u.%u, deleting");
	return(DeleteVnode(volindex, vclass, vnodeindex, u, vnode));
    }

    maxid = (SRV_RVM(MaxVolId) & 0x00FFFFFF);
    if (volindex < 0 || volindex > (int)maxid || volindex > MAXVOLS) {
	VLog(0,  "ReplaceVnode: bogus volume index %d", volindex);
	rvmlib_abort(VFAIL);	// invalid volume index
	return VNOVOL;
    }

    /* if vnodeindex is larger than array, need to alloc a new one and */
    /* copy over the data (must grow by same amount as bitvector!) */

    if (vclass == vSmall) {
	if (vnodeindex >= SRV_RVM(VolumeList[volindex]).data.nsmallLists) {
	    VLog(0,  "ReplaceVnode: bogus small vnode index %d", vnodeindex);
	    rvmlib_abort(VFAIL);	// invalid vnode index
	}
	vlist = &(SRV_RVM(VolumeList[volindex]).data.smallVnodeLists[vnodeindex]);
	nvnodes = &SRV_RVM(VolumeList[volindex]).data.nsmallvnodes;
	size = SIZEOF_SMALLDISKVNODE;
	name = "small";
    } else {
	if (vnodeindex >= SRV_RVM(VolumeList[volindex]).data.nlargeLists) {
	    VLog(0,  "ReplaceVnode: bogus large vnode index %d", vnodeindex);
	    rvmlib_abort(VFAIL);	// invalid vnode index
	}
	vlist = &(SRV_RVM(VolumeList[volindex]).data.largeVnodeLists[vnodeindex]);
	nvnodes = &SRV_RVM(VolumeList[volindex]).data.nlargevnodes;
	size = SIZEOF_LARGEDISKVNODE;
	name = "large";
    }

    /* check if vnode already exists */
    VnodeDiskObject *vdo = FindVnode(vlist, u);
    if (vdo == NULL) {
	VLog(39,  "ReplaceVnode: no %s vnode at index %d; allocating", name, vnodeindex);

	VLog(9,  "ReplaceVnode: malloc'ing %s vnode", name);
	vdo = (VnodeDiskObject *)rvmlib_rec_malloc(size);
	rvmlib_set_range(vdo, size);
	memset(vdo, 0, size);

	/* increment vnode count */
	RVMLIB_MODIFY(*nvnodes, *nvnodes + 1);

	/* append vnode into the appropriate rec_smolist */
	char buf[sizeof(struct rec_smolink)];
	memset(buf, 0, sizeof(struct rec_smolink));
	if (memcmp(&(vdo->nextvn), buf, sizeof(struct rec_smolink)) != 0) {
	    VLog(0,  "ERROR: REC_SMOLINK ON VNODE DURING ALLOCATION WAS NOT ZERO");
	    rvmlib_modify_bytes(&(vdo->nextvn), buf, sizeof(struct rec_smolink));
	}
	vlist->append(&(vdo->nextvn));
    }
    memcpy(&(vnode->nextvn), &(vdo->nextvn), sizeof(struct rec_smolink));
    rvmlib_modify_bytes(vdo, vnode, size);

    VLog(19, "Replace vnode - VnodeDiskObject passed to rtn:");
    if (VolDebugLevel > 19)  
	print_VnodeDiskObject(vnode);
    PrintCamVnode(19, volindex, vclass, vnodeindex, u);

    return 0;
}

static int DeleteVnode(int volindex, int vclass, VnodeId vnodeindex, 
			 Unique_t u, VnodeDiskObject *vnode)
{
    VolumeId maxid = 0;
    rec_smolist *vlist;
    bit32 *nvnodes;
    char *name;

    VLog(9, "Entering DeleteVnode(%d, %d, %d, <struct>)", 
	   volindex, vclass, vnodeindex);
    maxid = (SRV_RVM(MaxVolId) & 0x00FFFFFF);
    if (volindex < 0 || volindex > (int)maxid || volindex > MAXVOLS) {
	VLog(0,  "DeleteVnode: bogus volume index %d", volindex);
	rvmlib_abort(VFAIL);	// invalid volume index
    }

    if (vclass == vSmall) {
	if (vnodeindex >= SRV_RVM(VolumeList[volindex]).data.nsmallLists) {
	    VLog(0,  "DeleteVnode: deleting nonexistent vnode (index %d)", vnodeindex);
	    rvmlib_abort(VFAIL);
	}
	vlist = &(SRV_RVM(VolumeList[volindex]).data.smallVnodeLists[vnodeindex]);
	nvnodes = &SRV_RVM(VolumeList[volindex]).data.nsmallvnodes;
	name = "small";
    } else {
	if (vnodeindex >= SRV_RVM(VolumeList[volindex]).data.nlargeLists) {
	    VLog(0,  "DeleteVnode: deleting nonexistent vnode (index %d)", vnodeindex);
	    rvmlib_abort(VFAIL);
	}
	vlist = &(SRV_RVM(VolumeList[volindex]).data.largeVnodeLists[vnodeindex]);
	nvnodes = &SRV_RVM(VolumeList[volindex]).data.nlargevnodes;
	name = "large";
    }

    VnodeDiskObject *vdo = FindVnode(vlist, u);
    if (vdo != NULL) {
	/* remove vnode from index */
	vlist->remove(&(vdo->nextvn));

	VLog(9, "DeleteVnode: freeing %s vnode", name);
	rvmlib_rec_free((char *)vdo);

	/* decrement vnode count */
	RVMLIB_MODIFY(*nvnodes, *nvnodes - 1);
    }
    PrintCamVnode(19, volindex, vclass, vnodeindex, u);
    return 0;
}


/* initialize a new volumediskinfo structure and store it in */
/* the appropriate slot in recoverable storage */
/* Note: volindex is checked in ReplaceVolDiskInfo */
void NewVolDiskInfo(Error *ec, int volindex, VolumeDiskData *vol)
{
    VLog(9,  "Entering NewVolDiskInfo for index %d, volume %x",
						volindex, vol->id);
    /* how much needs to be initalized here? */
    InitVV(&(vol->versionvector));
    vol->stamp.magic = VOLUMEINFOMAGIC;
    vol->stamp.version = VOLUMEINFOVERSION;
    ReplaceVolDiskInfo(ec, volindex, vol);
}

/* Extracts the VolumeDiskInfo for the specified volume, and returns the */
/* volume's index in recoverable storage */
int VolDiskInfoById(Error *ec, VolumeId volid, VolumeDiskData *vol)
{
    int myind = -1;

    *ec = 0;

    VLog(9,  "Entering VolDiskInfoById for volume %x", volid);
    myind = HashLookup(volid);
    if (myind == -1) {
	VLog(0,  "VolDiskInfoById: HashLookup failed for volume %x", volid);
	*ec = VNOVOL;  /* volume not found */
    } else {
	ExtractVolDiskInfo(ec, myind, vol);
    }

    VLog(29,  "VolDiskInfoById: vol->stamp.magic = %u, vol->stamp.version = %u",
					vol->stamp.magic, vol->stamp.version);
    return (myind);
}

/* Must be called from within a transaction */
void ReplaceVolDiskInfo(Error *ec, int volindex, VolumeDiskData *vol)
{
    VolumeId maxid = 0;
    *ec = 0;

    VLog(9,  "Entering ReplaceVolDiskInfo for volume index %d", volindex);
    /* consistency check */
    CODA_ASSERT(vol->stamp.magic == VOLUMEINFOMAGIC);
    CODA_ASSERT(vol->stamp.version == VOLUMEINFOVERSION);

    maxid = (SRV_RVM(MaxVolId) & 0x00FFFFFF);
    if ((volindex < 0) || (volindex > (int)maxid) || (volindex > MAXVOLS)) {
	char volname[V_MAXVOLNAMELEN];
	sprintf(volname, VFORMAT, vol->id);
	VLog(0,  "ReplaceVolDiskInfo: bogus volume index %d for volume %s",
			    volindex, volname);
	*ec = VNOVOL;	// invalid volume index
	rvmlib_abort(VFAIL);
    }

    VLog(1,  "ReplaceVolDiskInfo: about to acquire locks");
    VLog(1,  "ReplacevolDiskInfo: got locks!");
    rvmlib_modify_bytes(SRV_RVM(VolumeList[volindex]).data.volumeInfo,
				    vol, sizeof(VolumeDiskData));
    VLog(29,  "ReplaceVolDiskInfo: recoverable stamp = %u, %u",
	    SRV_RVM(VolumeList[volindex]).data.volumeInfo->stamp.magic,
	    SRV_RVM(VolumeList[volindex]).data.volumeInfo->stamp.version);

    PrintCamDiskData(29, volindex,
			SRV_RVM(VolumeList[volindex].data.volumeInfo));
}


/* find a vnode with uniquifier u in a given index */
VnodeDiskObject *FindVnode(rec_smolist *vnlist, Unique_t u) {
    rec_smolist_iterator next(*vnlist);
    struct rec_smolink *p;
    VnodeDiskObject *vdo;
    while ((p = next())) {
	vdo = strbase(VnodeDiskObject, p, nextvn);
	if (vdo->uniquifier == u)
	    return(vdo);
    }
    return(NULL);
}

