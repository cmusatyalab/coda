/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
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
#include "coda_string.h"
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <lwp/lwp.h>
#include <lwp/lock.h>
#include <rvmlib.h>
#include <util.h>
#include <codadir.h>
#include <vice.h>
#ifdef __cplusplus
}
#endif

#include "cvnode.h"
#include "volume.h"
#include <recov_vollog.h>
#include "vutil.h"
#include "recov.h"
#include "index.h"

/* VnoceCalssInfo for small and large separately */
struct VnodeClassInfo VnodeClassInfo_Array[nVNODECLASSES];
/* Change hashindex below if you change this constant */
#define VNODE_HASH_TABLE_SIZE 256
/* the VnodeHashTable: shared among small and large */
static Vnode *VnodeHashTable[VNODE_HASH_TABLE_SIZE];


extern int large, small;

extern void VAddToVolumeUpdateList(Error *ec, Volume *vp);
extern void VBumpVolumeUsage(Volume *vp);

static Vnode *VAllocVnodeCommon(Error *ec, Volume *vp, VnodeType type,
				  VnodeId vnode, Unique_t unique);
static void moveHash(Vnode *vnp, bit32 newHash);
static void StickOnLruChain(Vnode *vnp, struct VnodeClassInfo *vcp);

static void printvn(FILE *outfile, VnodeDiskObject *vnode, VnodeId vnodeNumber);
/* There are two separate vnode queue types defined here:
 * Each hash conflict chain -- is singly linked, with a single head
 * pointer. New entries are added at the beginning. Old
 * entries are removed by linear search, which generally
 * only occurs after a disk read).
 * LRU chain -- is doubly linked, single head pointer.
 * Entries are added at the head, reclaimed from the tail,
 * or removed from anywhere in the queue.  A Vnode on this list
 * should have use count 1.
 */


/* Vnode hash table.  Find hash chain by taking lower bits of
 * (volume_hash_offset + vnode).
 * This distributes the root inodes of the volumes over the
 * hash table entries and also distributes the vnodes of
 * volumes reasonably fairly.  The volume_hash_offset field
 * for each volume is established as the volume comes on line
 * by using the VOLUME_HASH_OFFSET macro.  This distributes the
 * volumes fairly among the cache entries, both when servicing
 * a small number of volumes and when servicing a large number.
 */


/* VolumeHashOffset -- returns a new value to be stored in the
 * volumeHashOffset of a Volume structure.  Called when a
 * volume is initialized.  Sets the volumeHashOffset so that
 * vnode cache entries are distributed reasonably between
 * volumes (the root vnodes of the volumes will hash to
 * different values, and spacing is maintained between volumes
 * when there are not many volumes represented), and spread
 * equally amongst vnodes within a single volume.
 */
int VolumeHashOffset() 
{
	static int nextVolumeHashOffset = 0;
	/* hashindex Must be power of two in size */
#   define hashShift 3
#   define hashMask ((1<<hashShift)-1)
	static byte hashindex[1<<hashShift] = {0,128,64,192,32,160,96,224};
	int offset;
	
	LogMsg(9, VolDebugLevel, stdout,  "Entering VolumeHashOffset()");
	offset = hashindex[nextVolumeHashOffset&hashMask]
		+ (nextVolumeHashOffset>>hashShift);
	nextVolumeHashOffset++;
	return offset;
}

#define VNODE_HASH(volumeptr,vnodenumber, unq)\
    ((volumeptr->vnodeHashOffset + vnodenumber+unq)&(VNODE_HASH_TABLE_SIZE-1))

/* 
   Not normally called by general client; called by volume.c 
   set up the VnodeClassIfno structure and LRU lists. 
*/
void VInitVnodes(VnodeClass vclass, int nVnodes)
{
	byte *va;
	struct VnodeClassInfo *vcp = &VnodeClassInfo_Array[vclass];

	SLog(9,  "Entering VInitVnodes(vclass = %d, vnodes = %d)", 
	     vclass, nVnodes);

	/* shouldn't these be set to 0? ***/
	vcp->allocs = vcp->gets = vcp->reads = vcp->writes = 0;
	vcp->cacheSize = nVnodes;
	switch(vclass) {
	case vSmall:
		SLog(29, "VInitVnodes: VnodeDiskObject = %d, SIZEOF_SMALLVNODE = %d", 
		     sizeof(VnodeDiskObject), SIZEOF_SMALLDISKVNODE);
		vcp->lruHead = NULL;
		vcp->residentSize = SIZEOF_SMALLVNODE;
		vcp->diskSize = SIZEOF_SMALLDISKVNODE;
		vcp->magic = SMALLVNODEMAGIC;
		break;
	case vLarge:
		vcp->lruHead = NULL;
		vcp->residentSize = SIZEOF_LARGEVNODE;
		vcp->diskSize = SIZEOF_LARGEDISKVNODE;
		vcp->magic = LARGEVNODEMAGIC;
		break;
	}

	va = (byte *) calloc(nVnodes,vcp->residentSize);
	CODA_ASSERT (va != NULL);
	while (nVnodes--) {
		Vnode *vnp = (Vnode *) va;
		vnp->nUsers = 1;
		Lock_Init(&vnp->lock);
		vnp->changed = 0;
		vnp->volumePtr = NULL;
		vnp->cacheCheck = 0;
		vnp->hashIndex = 0;
		if (vcp->lruHead == NULL)
			vcp->lruHead = vnp->lruNext = vnp->lruPrev = vnp;
		else {
			vnp->lruNext = vcp->lruHead;
			vnp->lruPrev = vcp->lruHead->lruPrev;
			vcp->lruHead->lruPrev = vnp;
			vnp->lruPrev->lruNext = vnp;
			vcp->lruHead = vnp;
		}
		va += vcp->residentSize;
	}
}

static void GrowVnLRUCache(VnodeClass vclass, int nVnodes)
{
	byte *va;
	struct VnodeClassInfo *vcp = &VnodeClassInfo_Array[vclass];
	
	SLog(9, "Entering GrowVnLRUCache(vclass = %d, vnodes = %d)", vclass, nVnodes);
	va = (byte *) calloc(nVnodes,vcp->residentSize);
	CODA_ASSERT (va != NULL);
	vcp->cacheSize += nVnodes;
	while (nVnodes--) {
		Vnode *vnp = (Vnode *) va;
		vnp->nUsers = 1;
		Lock_Init(&vnp->lock);
		vnp->changed = 0;
		vnp->volumePtr = NULL;
		vnp->cacheCheck = 0;
		vnp->hashIndex = 0;
		CODA_ASSERT(vcp->lruHead != NULL);
		vnp->lruNext = vcp->lruHead;
		vnp->lruPrev = vcp->lruHead->lruPrev;
		vcp->lruHead->lruPrev = vnp;
		vnp->lruPrev->lruNext = vnp;
		vcp->lruHead = vnp;
		va += vcp->residentSize;
	}
}



/* 
   Allocate range->Count "contiguous" fids, starting at <range->Vnode,
   range->Unique> and continuing with strides of <range->Stride,
   1>. 
*/
int VAllocFid(Volume *vp, VnodeType type, ViceFidRange *range, int stride, int ix)
{
	Error ec = 0;
	int count = range->Count;
	
	SLog(9, "VAllocFid: volume = %x, type = %d, count = %d, stride = %d, ix = %d",
	     V_id(vp), type, count, stride, ix);

	/* Sanity checks. */
	{
		ProgramType *pt;
		CODA_ASSERT(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);
		if (*pt == fileServer && !V_inUse(vp))
			return(VOFFLINE);
		
		if (!VolumeWriteable(vp))
			return(VREADONLY);
	}
	
	/* Determine uniquifier base, and increment VM counter beyond end
       of range being allocated. Extend RVM counter by another chunk
       if VM counter has now reached or exceeded it! */
	Unique_t BaseUnique = vp->nextVnodeUnique;
	vp->nextVnodeUnique += count;
	if (vp->nextVnodeUnique > V_uniquifier(vp)) {
		rvm_return_t status;
		SLog(0, "VAllocFid: volume disk uniquifier being extended");
		V_uniquifier(vp) = vp->nextVnodeUnique + 200;
		
		rvmlib_begin_transaction(restore);
		VUpdateVolume(&ec, vp);
		rvmlib_end_transaction(flush,  &(status));

		if (status) {
			SLog(0, "VallocFid: rvm Error %d\n", status);
			ec = VNOVNODE;
		}
		if (ec) 
			return(ec);
	}

	/* Find and set a suitable range in the bitmap. */
	VnodeClass vclass = vnodeTypeToClass(type);
	int BaseBitNumber = VAllocBitmapEntry(&ec, vp, &vp->vnIndex[vclass],
					      stride, ix, count);
	if (ec) 
		return(ec);
	VnodeId BaseVnode = bitNumberToVnodeNumber(BaseBitNumber, vclass);
	
	/* Complete the range descriptor. */
	range->Vnode = BaseVnode;
	range->Unique = BaseUnique;
	range->Stride = stride * nVNODECLASSES;
	
	return(0);
}

/* Allocate a specific fid. */
int VAllocFid(Volume *vp, VnodeType type, VnodeId vnode, Unique_t unique)
{
	Error ec = 0;

	SLog(9, "VAllocFid: fid = (%x.%x.%x)", V_id(vp), vnode, unique);

	/* Sanity checks. */
	{
		ProgramType *pt;
		CODA_ASSERT(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);
		if (*pt == fileServer && !V_inUse(vp))
			return(VOFFLINE);
		
		if (!VolumeWriteable(vp))
			return(VREADONLY);
	}
	
	/* Extend VM counter beyond specified uniquifier if necessary.
       Extend RVM counter by another chunk if VM counter has now
       reached or exceeded it! */
	if (unique >= vp->nextVnodeUnique)
		vp->nextVnodeUnique = unique + 1;
	if (vp->nextVnodeUnique > V_uniquifier(vp)) {
		SLog(0, "VAllocFid: volume disk uniquifier being extended");
		V_uniquifier(vp) = vp->nextVnodeUnique + 200;

		rvm_return_t camstatus;
		rvmlib_begin_transaction(restore);
		VUpdateVolume(&ec, vp);
		rvmlib_end_transaction(flush, &(camstatus));
		
		if (ec) return(ec);
	}

	/* Set the proper bit in the bitmap. */
	VnodeClass vclass = vnodeTypeToClass(type);
	int bitNumber;
	bitNumber = VAllocBitmapEntry(&ec, vp, &vp->vnIndex[vclass], vnode);

	return ec;

}

Vnode *VAllocVnode(Error *ec, Volume *vp, VnodeType type, int stride, int ix)
{
	*ec = 0;

	SLog(9, "VAllocVnode: volume = %x, type = %d, stride = %d, ix = %d",
	     V_id(vp), type, stride, ix);

	/* Allocate a fid with the specified characteristics. */
	ViceFidRange range;
	range.Count = 1;
	*ec = VAllocFid(vp, type, &range, stride, ix);
	if (*ec) 
		return(NULL);
	VnodeId vnode = range.Vnode;
	Unique_t unique = range.Unique;
	
	return(VAllocVnodeCommon(ec, vp, type, vnode, unique));
}


Vnode *VAllocVnode(Error *ec, Volume *vp, VnodeType type, VnodeId vnode, Unique_t unique)
{
	*ec = 0;
	
	SLog(9, "VAllocVnode: fid = (%x.%x.%x)", V_id(vp), vnode, unique);
	
	/* Ensure that the specified fid is allocated. */
	*ec = VAllocFid(vp, type, vnode, unique);
	if (*ec) 
		return(NULL);

	return(VAllocVnodeCommon(ec, vp, type, vnode, unique));
}
    

static Vnode *VAllocVnodeCommon(Error *ec, Volume *vp, VnodeType type,
				  VnodeId vnode, Unique_t unique) 
{
	LogMsg(19, VolDebugLevel, stdout,  "Entering VAllocVnodeCommon: ");
	VnodeClass vclass = vnodeTypeToClass(type);
	struct VnodeClassInfo *vcp = &VnodeClassInfo_Array[vclass];
	vindex vol_index(V_id(vp), vclass, V_device(vp), vcp->diskSize);
	int newHash = VNODE_HASH(vp, vnode, unique);
	Vnode *vnp = NULL;

	/* Grow vnode array if necessary. */
	LogMsg(19, VolDebugLevel, stdout,  "vol_index.elts = %d", vol_index.elts());
	if ((vp->vnIndex[vclass].bitmapSize << 3) > vol_index.elts()) {
		rvm_return_t camstatus;
		VLog(1, "VAllocVnode: growing %s vnode array", vclass ? "small" : "large");
		rvmlib_begin_transaction(restore);
		GrowVnodes(V_id(vp), vclass, vp->vnIndex[vclass].bitmapSize);
		rvmlib_end_transaction(flush,  &(camstatus));
	}
	
	/* Check that object does not already exist in RVM. */
	if (ObjectExists(V_volumeindex(vp), vclass, 
			 vnodeIdToBitNumber(vnode), unique)) {
		VLog(0, "VAllocVnode: object (%x.%x.%x) found in RVM",
		       V_id(vp), vnode, unique);
		*ec = EEXIST;
		return(NULL);
	}
	
	/* Check that object does not already exist in VM. */
	for (vnp = VnodeHashTable[newHash];
	     vnp && (vnp->vnodeNumber != vnode ||
		     vnp->volumePtr != vp ||
		     vnp->disk.uniquifier != unique ||
		     vnp->volumePtr->cacheCheck != vnp->cacheCheck);
	     vnp = vnp->hashNext)
		;
	if (vnp != NULL) {
		LogMsg(0, VolDebugLevel, stdout,  "VAllocVnode: object (%x.%x.%x) found in VM",
		       V_id(vp), vnode, unique);
		*ec = EEXIST;
		return(NULL);
	}
	
	/* Get vnode off LRU chain and move it to the new hash bucket. */
	if (vcp->lruHead->lruPrev == vcp->lruHead) {
		LogMsg(0, VolDebugLevel, stdout,  "VAllocVnode: LRU cache has only one entry - growing cache dynamically");
		GrowVnLRUCache(vclass, vclass == vSmall ? small : large);
	}
	vnp = vcp->lruHead->lruPrev;
	moveHash(vnp, newHash);
	
	/* Initialize the VM copy of the vnode. */
	memset((char *)&vnp->disk, 0, sizeof(vnp->disk));
	vnp->changed = 1;	/* Eventually write this thing out */
	vnp->delete_me = 0;
	vnp->vnodeNumber = vnode;
	vnp->volumePtr = vp;
	vnp->cacheCheck = vp->cacheCheck;

	/* First user.  Remove it from the LRU chain. */
	/* We can assume that there is at least one item in the queue */
	vnp->nUsers = 1;
	if (vnp == vcp->lruHead)
		vcp->lruHead = vcp->lruHead->lruNext;
	vnp->lruPrev->lruNext = vnp->lruNext;
	vnp->lruNext->lruPrev = vnp->lruPrev;
	if (vnp == vcp->lruHead || vcp->lruHead == NULL) {
		LogMsg(-1, 0, stdout, "VAllocVnode: lru chain addled!");
		CODA_ASSERT(0);
	}
	vnp->disk.vnodeMagic = vcp->magic;
	vnp->disk.type = type;
	vnp->disk.uniquifier = unique;
	vnp->disk.vol_index = vp->vol_index;
	vnp->disk.log = NULL;
	ObtainWriteLock(&vnp->lock);
	LWP_CurrentProcess(&vnp->writer);
	
	vcp->allocs++;
	LogMsg(19, VolDebugLevel, stdout,  "VAllocVnode: printing vnode %x after allocation:", vnode);
	if (VolDebugLevel >= 19)
		printvn(stdout, &vnp->disk, vnode);
	
	return(vnp);
}


/*    int locktype; READ_LOCK or WRITE_LOCK, as defined in lock.h
      TRY_READ_LOCK or TRY_WRITE_LOCK, as defined in cvnode.h.  The
      latter are non-blocking calls.  They return the vnode locked as
      appropriate if the vnode is available, otherwise they return a
      NULL vnode and error EWOULDBLOCK.  int ignoreIncon TRUE
      (non-zero) iff it is ok for inconsistency flag to be set in
      vnode.  int ignoreBarren TRUE (non-zero) iff it is ok for barren
      flag to be set in vnode */
Vnode *VGetVnode(Error *ec, Volume *vp, VnodeId vnodeNumber,
		  Unique_t unq, int locktype, int ignoreIncon, 
		 int ignoreBarren)

{
	Vnode *vnp;
	int newHash;
	VnodeClass vclass;
	struct VnodeClassInfo *vcp;
	ProgramType *pt;

	SLog(9, "Entering VGetVnode(vol %x, vnode %lx, lock %d, ignoreIncon %d)",
	     V_id(vp), vnodeNumber, locktype, ignoreIncon);
	*ec = 0;

	if (vnodeNumber == 0) {
		*ec = VNOVNODE;
		SLog(0, "VGetVnode: Bogus vnodenumber %lx", vnodeNumber);
		return NULL;
	}

	CODA_ASSERT(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);
	if (*pt == fileServer && !V_inUse(vp)) {
		*ec = VOFFLINE;
		SLog(9, "VGetVnode: volume 0x%x is offline", V_id(vp));
		return NULL;
	}
	vclass = vnodeIdToClass(vnodeNumber);
	vcp = &VnodeClassInfo_Array[vclass];

	if ( (locktype == WRITE_LOCK || locktype == TRY_WRITE_LOCK) && 
	     !VolumeWriteable(vp)) {
		*ec = VREADONLY;
		SLog(0, "VGetVnode: attempt to write lock readonly volume %x", 
		     V_id(vp));
		return NULL;
	}

	/* See whether the vnode is in the cache. */
	newHash = VNODE_HASH(vp, vnodeNumber, unq);
	SLog(19,  "VGetVnode: newHash = %d, vp = 0x%x, vnodeNumber = %lx Unique = %x",
	     newHash, vp, vnodeNumber, unq);
	for (vnp = VnodeHashTable[newHash];
	     vnp && (vnp->vnodeNumber!=vnodeNumber ||
		     vnp->volumePtr!=vp ||
		     vnp->disk.uniquifier != unq ||
		     vnp->volumePtr->cacheCheck!=vnp->cacheCheck);
	     vnp = vnp->hashNext
	     );
	vcp->gets++;

	if (vnp == NULL) {
		int     n;
		/* Not in cache; tentatively grab most distantly used
		   one from the LRU chain */
		SLog(1,  "VGetVnode: going to rvm for vnode %x.%lx", V_id(vp), vnodeNumber);
		vcp->reads++;
		if (vcp->lruHead == vcp->lruHead->lruPrev) {
			SLog(0,  "VGetVode: Only 1 entry left in lru cache - growing cache");
			GrowVnLRUCache(vclass, vclass == vSmall ? small : large);
		}
		vnp = vcp->lruHead->lruPrev;
		if ( vnp->dh ) {
			SLog(0, "VGetVnode: DROPPING dh of vn %x un %x"
			     "total count %d, dh_refc: %d\n",
			     vnp->vnodeNumber, vnp->disk.uniquifier, 
			     DC_Count(vnp->dh), vnp->dh_refc);
			VN_DropDirHandle(vnp);
		}

		/* Read vnode from volume index */
		vindex v_index(V_id(vp), vclass, V_device(vp), vcp->diskSize);
		if ((n = v_index.get(vnodeNumber, unq, &vnp->disk)) != 0) {
			/* Vnode is not allocated */
			*ec = VNOVNODE;
			SLog(0, "VGetVnode: vnode %x.%lx is not allocated", 
			     V_id(vp), vnodeNumber);
			return NULL;
		}
		/* Quick check to see that the data is reasonable */
		if (vnp->disk.type == vNull) {
			*ec = VNOVNODE;
			SLog(0, "VGetVnode: vnode %x.%lx not allocated", 
			     V_id(vp), vnodeNumber);
			return NULL;
		}
		if (vnp->disk.vnodeMagic != vcp->magic) {
			SLog(0, "VGetVnode: Bad vnodeMagic, vnode %x.%lx, (%s); volume needs salvage",  
			     V_id(vp), vnodeNumber, V_name(vp));
			SLog(0, "VGetVnode: magic = %u, LVNODEMAGIC = %u, vcp->magic = %u",
			     vnp->disk.vnodeMagic, LARGEVNODEMAGIC, vcp->magic);
			VOffline(vp, "");
			*ec = VSALVAGE;
			return NULL;
		}
		/* Remove it from the old hash chain */
		moveHash(vnp, newHash);
		/* Initialize */
		vnp->changed = (byte)0;
		vnp->delete_me = (byte)0;
		vnp->nUsers = 0;
		vnp->vnodeNumber = vnodeNumber;
		vnp->volumePtr = vp;
		vnp->cacheCheck = vp->cacheCheck;
	}

	/* Check for inconsistency */
	if (IsIncon(vnp->disk.versionvector) && !ignoreIncon) {
		*ec = EINCONS;
		return (NULL);	
	}
	/* Check for barren flag */
	if (IsBarren(vnp->disk.versionvector) && !ignoreBarren){
		*ec = EIO;
		return(NULL);
	}

	if (++vnp->nUsers == 1) {
		int cdn1, cdn2, cdn3;
		/* First user.  Remove it from the LRU chain.  We can
		   assume that there is at least one item in the queue */
		if (vnp == vcp->lruHead)
			vcp->lruHead = vcp->lruHead->lruNext;
		cdn1 = (vnp == vcp->lruHead);
		cdn2 = (vcp->lruHead == NULL);
		cdn3 = (cdn1 || cdn2) ; 
		/* g++ goes haywire here? Why? 
		   if ( (vnp == vcp->lruHead) || (vcp->lruHead == NULL) ) */
		if ( cdn3 ) {
			LogMsg(-1, 0, stdout, "VGetVnode: lru chain addled!");
			CODA_ASSERT(0);
		}
		vnp->lruPrev->lruNext = vnp->lruNext;
		vnp->lruNext->lruPrev = vnp->lruPrev;
	}

	if (locktype == READ_LOCK || locktype == TRY_READ_LOCK) {
		if (CheckLock(&vnp->lock) == -1) {
			SLog(1, "VGetVnode (readlock): vnode %x.%u is write locked!",
			     V_id(vp), vnodeNumber);
			if (locktype == TRY_READ_LOCK) {
				*ec = EWOULDBLOCK;
				return(NULL);
			}
		}
		ObtainReadLock(&vnp->lock);
	} else {
		if (CheckLock(&vnp->lock) != 0) {
			SLog(1, "VGetVnode (writelock): vnode %x.%u is not unlocked!",
			     V_id(vp), vnodeNumber);
			if (locktype == TRY_WRITE_LOCK) {
				*ec = EWOULDBLOCK;
				return(NULL);
			}
		}
		ObtainWriteLock(&vnp->lock);
		vnp->changed = 1;	/* assume the vnode will change */
		LWP_CurrentProcess(&vnp->writer);
	}
	/* Check that the vnode hasn't been removed while we were obtaining
	   the lock */
	if (vnp->disk.type == vNull) {
		if (vnp->nUsers-- == 1) 
			StickOnLruChain(vnp,vcp);
		
		if (locktype == READ_LOCK || locktype == TRY_READ_LOCK)
			ReleaseReadLock(&vnp->lock);
		else
			ReleaseWriteLock(&vnp->lock);
		*ec = VNOVNODE;
		SLog(0, "VGetVnode: memory vnode was snatched away");
		return NULL;
	}
	/* Hack; don't know where this should be */
	if (*pt == fileServer)
  		VBumpVolumeUsage(vnp->volumePtr);
	return vnp;
}



/* Write vnode back to recoverable storage if dirty */
void VPutVnode(Error *ec,Vnode *vnp)
{
	int writeLocked;
	VnodeClass vclass;
	struct VnodeClassInfo *vcp;

	SLog(9, "Entering VPutVnode for vnode %u", vnp->vnodeNumber);
	*ec = 0;
	CODA_ASSERT (vnp->nUsers != 0);
	vclass = vnodeIdToClass(vnp->vnodeNumber);
	vcp = &VnodeClassInfo_Array[vclass];
	CODA_ASSERT(vnp->disk.vnodeMagic == vcp->magic);
	writeLocked = WriteLocked(&vnp->lock);
	if (writeLocked) {
		PROCESS thisProcess;
		LWP_CurrentProcess(&thisProcess);
		if (thisProcess != vnp->writer){
			SLog(-1, "VPutVnode: Vnode at 0x%x locked by another process!",vnp);
			CODA_ASSERT(0);
		}
		
		if (vnp->changed || vnp->delete_me) {
			Volume *vp = vnp->volumePtr;
			vindex v_index(V_id(vp), vclass, V_device(vp),
				       vcp->diskSize);
			long now = FT_ApproxTime();
			CODA_ASSERT(vnp->cacheCheck == vnp->cacheCheck);
			if (vnp->delete_me) 
				vnp->disk.type = vNull;	    /*  mark it for deletion */
			else {
				vnp->disk.serverModifyTime = (Date_t) now;
			}
			V_updateDate(vp) = (Date_t) now;
			/* The inode has been changed.  Write it out to disk */
			if (!V_inUse(vp)) {
				CODA_ASSERT(V_needsSalvaged(vp));
				*ec = VSALVAGE;
			} else {
				SLog(9, "VPutVnode: about to write vnode %d, type %d",
				       vnp->vnodeNumber, vnp->disk.type);
				if (VnLog(vnp) == NULL && 
				    vnp->disk.type == vDirectory) {
					/* large vnode - need to allocate the resolution log */
					if (AllowResolution && V_RVMResOn(vp)) {
						SLog(9, "VPutVnode: Creating resolution log for (0x%x.%x.%x)\n",
						     V_id(vp), vnp->vnodeNumber, vnp->disk.uniquifier);
						CreateResLog(vp, vnp);
					}
				}
				if (v_index.put(vnp->vnodeNumber, 
						vnp->disk.uniquifier, &vnp->disk) != 0) {
					LogMsg(0, VolDebugLevel, stdout,  "VPutVnode: Couldn't write vnode %x.%u (%s)",
					       V_id(vnp->volumePtr), vnp->vnodeNumber,
					       V_name(vnp->volumePtr));
					VForceOffline(vp);
					*ec = VSALVAGE;
				} else
					VAddToVolumeUpdateList(ec, vp);
				if (vnp->delete_me &&
				    v_index.IsEmpty(vnp->vnodeNumber))
					VFreeBitMapEntry(ec, &vp->vnIndex[vclass],
							 vnodeIdToBitNumber(vnp->vnodeNumber));
				
			}
			vcp->writes++;
			vnp->changed = 0;
		}
	} else { /* Not write locked */
		if (vnp->changed || vnp->delete_me){
			SLog(-1, "VPutVnode: Change or delete flag for vnode 0x%x is set but vnode is not write locked!", vnp);
			CODA_ASSERT(0);
		}
	}

	/* Do not look at disk portion of vnode after this point; it may
	   have been deleted above; also clear the DirHandle (this could
	   be postponed, by clearing it when we reuse a Vnode from the LRU) */
	if (vnp->nUsers-- == 1) {
		StickOnLruChain(vnp,vcp);
		/* VN_PutDirHandle(vnp);  */
	}
	vnp->delete_me = 0;
	if (writeLocked)
		ReleaseWriteLock(&vnp->lock);
	else
		ReleaseReadLock(&vnp->lock);
}
/*
 * put back a vnode but dont write it to RVM - 
 * simulate an abort with release lock 
 */
void VFlushVnode(Error *ec, Vnode *vnp) 
{
    VLog(0,  "Entering VFlushVnode for vnode 0x%x", vnp->vnodeNumber);
    *ec = 0;

    /* Sanity checks. */
    CODA_ASSERT (vnp->nUsers != 0);

    /* if not write locked  same as VPutVnode */
    if (!WriteLocked(&vnp->lock)) {
	CODA_ASSERT(!vnp->changed);
	VPutVnode(ec, vnp);
	return;
    }

    CODA_ASSERT(vnp->changed);
    PROCESS thisProcess;
    LWP_CurrentProcess(&thisProcess);
    if (thisProcess != vnp->writer){
	VLog(-1, "VFlushVnode: Vnode at 0x%x locked by other thread!", 
	     vnp->vnodeNumber);
	CODA_ASSERT(0);
    }

    /* Get the vnode class info. */
    VnodeClass vclass = vnodeIdToClass(vnp->vnodeNumber);
    struct VnodeClassInfo *vcp = &VnodeClassInfo_Array[vclass];
    CODA_ASSERT(vnp->disk.vnodeMagic == vcp->magic);

    /* refresh the vm copy of vnode */
    Volume *vp = vnp->volumePtr;
    CODA_ASSERT(V_inUse(vp));
    CODA_ASSERT(vp->cacheCheck == vnp->cacheCheck);
    vindex v_index(V_id(vp), vclass, V_device(vp), vcp->diskSize);
    /* Re-read the disk part of the vnode if it exists in rvm */
    if (ObjectExists(V_volumeindex(vp), vclass, 
		     vnodeIdToBitNumber(vnp->vnodeNumber),
		     vnp->disk.uniquifier)) {
	CODA_ASSERT(v_index.get(vnp->vnodeNumber, vnp->disk.uniquifier, &vnp->disk) == 0);
    } else {
	/* Ensure that lock waiters (in VGetVnode) abandon this object. */
	vnp->disk.type = vNull;

	/* Ensure that object cannot be found in VM cache by future VGet'ers. */
	vnp->vnodeNumber = 0;
	vnp->disk.uniquifier = 0;

	if (v_index.IsEmpty(vnp->vnodeNumber))
	    VFreeBitMapEntry(ec, &vp->vnIndex[vclass],
			     vnodeIdToBitNumber(vnp->vnodeNumber));
    }
	
    /* Re-init the in-memory part of the vnode and unlock. */
    vnp->changed = 0;
    vnp->delete_me = 0;
    if (vnp->nUsers-- == 1) 
	StickOnLruChain(vnp, vcp);

    ReleaseWriteLock(&vnp->lock);
}

/* Move the vnode, vnp, to the new hash table given by the
   hash table index, newHash */
static void moveHash(Vnode *vnp, bit32 newHash)
{
	Vnode *tvnp;
	/* Remove it from the old hash chain */

	LogMsg(9, VolDebugLevel, stdout,  "Entering moveHash(vnode %u)", vnp->vnodeNumber);
	tvnp = VnodeHashTable[vnp->hashIndex];
	if (tvnp == vnp) {
		SLog(9,  "moveHash: setting VnodeHashTable[%d] = 0x%x",
		     vnp->hashIndex, vnp->hashNext);
		VnodeHashTable[vnp->hashIndex] = vnp->hashNext;
	} else {
		while (tvnp && tvnp->hashNext != vnp)
			tvnp = tvnp->hashNext;
		if (tvnp)
			tvnp->hashNext = vnp->hashNext;
	}
	/* Add it to the new hash chain */
	vnp->hashNext = VnodeHashTable[newHash];
	SLog(9, "moveHash: setting VnodeHashTable[%d] = 0x%x", newHash, vnp);
	VnodeHashTable[newHash] = vnp;
	vnp->hashIndex = newHash;
}

static void StickOnLruChain(Vnode *vnp, struct VnodeClassInfo *vcp)
{
	/* Add it to the circular LRU list */
	SLog(9, "Entering StickOnLruChain for vnode %u", vnp->vnodeNumber);
	if (vcp->lruHead == NULL){
		SLog(-1, "StickOnLruChain: vcp->lruHead==NULL");
		CODA_ASSERT(0);
	} else {
		vnp->lruNext = vcp->lruHead;
		vnp->lruPrev = vcp->lruHead->lruPrev;
		vcp->lruHead->lruPrev = vnp;
		vnp->lruPrev->lruNext = vnp;
		vcp->lruHead = vnp;
	}
	/* If the vnode was just deleted, put it at the end of the
	   chain so it will be reused immediately */
	if (vnp->delete_me)
		vcp->lruHead = vnp->lruNext;
}

void VN_VN2Fid(Vnode *vptr, Volume *volptr, ViceFid *fid)
{
	fid->Volume = V_id(volptr);
	fid->Vnode = vptr->vnodeNumber;
	fid->Unique = vptr->disk.uniquifier;
}

void VN_VN2PFid(Vnode *vptr, Volume *volptr, ViceFid *fid)
{
	fid->Volume = V_id(volptr);
	fid->Vnode = vptr->disk.vparent;
	fid->Unique = vptr->disk.uparent;
}




/* temporary debugging stuff */

static void printvn(FILE *outfile, VnodeDiskObject *vnode, VnodeId vnodeNumber)
{
    fprintf(outfile, "Vnode %lu.%lu.%lu, cloned = %u, length = %lu, inode = %lu\n",
        vnodeNumber, vnode->uniquifier, vnode->dataVersion, vnode->cloned,
	vnode->length, vnode->inodeNumber);
    fprintf(outfile, "link count = %u, type = %u, volume index = %d\n", vnode->linkCount, vnode->type, vnode->vol_index);
    fprintf(outfile, "parent = %lx.%lx\n", vnode->vparent, vnode->uparent);
    PrintVV(outfile, &(vnode->versionvector));
    return;
}

