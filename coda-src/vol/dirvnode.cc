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
		DI_Dec(pdi);
	} else if (vnp->changed) {
		/* directory was modified - commit the pages */
		DLog(29, "VN_DCommit: Commiting pages for dir vnode = %d", 
			vnp->vnodeNumber);
		/* copy the VM pages into RVM */
		pdi = DI_DhToDi(pdce, pdi);
		assert(pdi);
		/* rehash just in case it is new */
		DC_Commit(pdce);
		vnp->disk.inodeNumber = (long unsigned int) pdi;
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

	if ( vn->disk.inodeNumber == 0 )
		pdce = DC_New();
	else
		pdce = DC_Get((PDirInode)vn->disk.inodeNumber);
	vn->dh = pdce;

	return DC_DC2DH(pdce);
}

/*
  VN_PutDirHandle: the Vnode is going away, clear the DC entry
 */
void VN_PutDirHandle(struct Vnode *vn)
{
	if (vn->dh) 
		DC_Put(vn->dh);
	vn->dh = NULL;
}

/* Drop DirHandle */
void VN_DropDirHandle(struct Vnode *vn)
{
	if (vn->dh) 
		DC_Drop(vn->dh);
	vn->dh = NULL;
}

