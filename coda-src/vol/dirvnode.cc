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

#endif /*_BLURB_*/


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include <lwp.h>
#include <lock.h>
#include <util.h>
#include <rvmlib.h>
#include <codadir.h>
#include <vice.h>
#ifdef __cplusplus
}
#endif __cplusplus


#include "cvnode.h"
#include "volume.h"
#include <dirvnode.h>

/* 
   This file manages the directory handle cache in conjunction 
   with Vnodes.

   The Directory Handle cache is controlled by the Vnode cache. 
   When directory content is needed, VN_SetDirHandle must be called. 
   When the Vnode goes away is calls DC_Drop to eliminate the cache.
   It could call DC_Put but presently the calls VN_SetDirHandle is not
   paired with a corresponding VN_PutDirHandle call. 

   The reference counting done by DC is therefore ignored:
   i.e. VN_SetDirHandle can be called at any time and need not te
   matched by equally many "Puts", when VN_PutDirHandle is called the
   cache entry will go away.

   This would be a good thing to fix, since it would potentially enhance 
   performance.

*/

/* copies DirHandle data  into pages in recoverable storage */
/* Called from within a transaction */
int VN_DCommit(Vnode *vnp)
{   
	PDirInode   pdi = (PDirInode) vnp->disk.inodeNumber;
	PDCEntry    pdce = vnp->dh;

	if (!vnp || (vnp->disk.type != vDirectory) || !pdce) {
		DLog(29, "VN_DCommit: Vnode or dh not allocated/not a directory");
		return 0;
	}

	if (vnp->delete_me) {
		/* directory was deleted */
		DLog(29, "VN_DCommit: deleted directory, vnode = %d",  
		     vnp->vnodeNumber);
		vnp->disk.inodeNumber = 0;
		/* if this vnode was just cloned, there won't be a pdi upon 
		   removal */
		if (pdi)
			DI_Dec(pdi);
	} else if (vnp->changed) {
		/* directory was modified - commit the pages */
		DLog(29, "VN_DCommit: Commiting pages for dir vnode = %d", 
			vnp->vnodeNumber);
		/* copy the VM pages into RVM */
		DI_DhToDi(pdce);
		/* CODA_ASSERT the directory inode now exists... */
		CODA_ASSERT(DC_DC2DI(pdce));
		/* rehash just in case it is new */
		DC_Rehash(pdce);
		vnp->disk.inodeNumber = (long unsigned int) DC_DC2DI(pdce);
	}
	return 0;
}


/* Eliminate the VM Data of the directory */
int VN_DAbort(Vnode *vnp) 
{
	Volume *volume;
    
	if (!vnp || (vnp->disk.type != vDirectory) || !vnp->dh){
		DLog(29, "DAbort: Vnode not allocated, not a directory or no handle");
		return(0);
	}
    
#if 0    /* better safe than sorry */
	if (!vnp->changed)
		return(0);
#endif 
	DH_FreeData(DC_DC2DH(vnp->dh));

	return(0);
}

/* 
   this hashes the Directory Handle and copies inode pages into the DH. 
*/
PDirHandle VN_SetDirHandle(struct Vnode *vn)
{
	PDCEntry pdce = NULL;

	/* three cases:
	   - new not previously seen 
	   - not new, already in RVM
            - new, not yet in RVM, still on the new_list
	*/

	if ( vn->disk.inodeNumber == 0 && vn->dh == 0 ) {  
		pdce = DC_New();
		SLog(0, "VN_GetDirHandle NEW Vnode %#x Uniq %#x cnt %d\n",
		     vn->vnodeNumber, vn->disk.uniquifier, DC_Count(pdce));
		vn->dh = pdce;
	} else if ( vn->disk.inodeNumber ) {
		pdce = DC_Get((PDirInode)vn->disk.inodeNumber);
		SLog(0, "VN_GetDirHandle for Vnode %#x Uniq %#x cnt %d\n",
		     vn->vnodeNumber, vn->disk.uniquifier, DC_Count(pdce));
		vn->dh = pdce;
	} else {
		pdce = vn->dh;
		DC_SetCount(pdce, DC_Count(pdce) + 1);
		SLog(0, "VN_GetDirHandle NEW-seen Vnode %#x Uniq %#x cnt %d\n",
		     vn->vnodeNumber, vn->disk.uniquifier, DC_Count(pdce));
	}

	return DC_DC2DH(pdce);
}

/*
  VN_PutDirHandle: the Vnode is going away, clear the DC entry
 */
void VN_PutDirHandle(struct Vnode *vn)
{

	CODA_ASSERT(vn->dh);

	if (vn->dh) {
		SLog(0, "VN_PutDirHandle for Vnode %x Unique %x: count %d\n",
		     vn->vnodeNumber, vn->disk.uniquifier, DC_Count(vn->dh)-1);
		DC_Put(vn->dh);
		CODA_ASSERT(DC_Count(vn->dh) >= 0);
		if ( DC_Count(vn->dh) == 0 )
			vn->dh = 0;
	}
}

/* Drop DirHandle */
void VN_DropDirHandle(struct Vnode *vn)
{
	if (vn->dh) {
		SLog(0, "VN_DropDirHandle for Vnode %x Unique %x: count %d\n",
		     vn->vnodeNumber, vn->disk.uniquifier, DC_Count(vn->dh));
		DC_Drop(vn->dh);
	}
	vn->dh = NULL;
}

/*
   - directories: set the disk.inode field to 0 and 
     create a dcentry with the _old_ contents. 
     NOTE: afterwards the vptr->dh  will have VM data, 
     but no RVM data.
     new one is committed.
*/
void VN_CopyOnWrite(struct Vnode *vptr)
{
	PDCEntry pdce = DC_New();
	PDCEntry oldpdce;
	PDirHeader pdirh;
	PDirHandle pdh;
	
	CODA_ASSERT(pdce);
	CODA_ASSERT(vptr->disk.inodeNumber != 0);
	pdh = VN_SetDirHandle(vptr);
	pdirh = DH_Data(pdh);
	oldpdce = DC_DH2DC(pdh);
	DC_SetDirh(oldpdce, NULL);
	CODA_ASSERT(pdh);

	DC_SetDirh(pdce, pdirh);
	DC_SetCowpdi(pdce, (PDirInode)vptr->disk.inodeNumber);
	DC_SetDirty(pdce, 1);
	DC_SetCount(pdce, DC_Count(oldpdce));
	SLog(0, "CopyOnWrite: New dce= %p new dirheader = %p", 
	     (void *) pdce, (void *) pdirh);
	vptr->disk.inodeNumber = 0;
	vptr->disk.cloned = 0;
	/* put _both_ back decreasing the count */
	VN_PutDirHandle(vptr);
	DC_SetCount(oldpdce, DC_Count(oldpdce)-1);
	vptr->dh = pdce;

}
