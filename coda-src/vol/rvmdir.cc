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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/vol/rvmdir.cc,v 4.3 1998/01/10 18:39:42 braam Exp $";
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

#ifdef __cplusplus
}
#endif __cplusplus


#include <dhash.h>
#include <vice.h>
#include "cvnode.h"
#include "volume.h"
#include "codadir.h"


/* copies all the pages of a directory from the commit hash table */
/* into recoverable storage */
/* Called from within a transaction */
int VN_DCommit(Vnode *vnp)
{   
    struct VFid fid;
    DirInode	shadowInArr;
    int	    InArrModified = 0;
    shadowDirPage *sdp;
    Volume *volume;
    
    if (!vnp || (vnp->disk.type != vDirectory)){
	LogMsg(29, DirDebugLevel, stdout, "DCommit: Vnode not allocated or not a directory");
	return 0;
    }

    if (vnp->delete_me){
	/* directory was deleted */
	DLog(29, "DCommit: deleted directory, vnode = %d", vnp->vnodeNumber);
	DI_Dec((DirInode *)(vnp->disk.inodeNumber));
	vnp->disk.inodeNumber = 0;
	return 0;
    }
    
    else if (!vnp->delete_me && vnp->changed){
	/* directory was modified - commit the pages */
	DLog(29, "DCommit: Commiting pages for dir vnode = %d", vnp->vnodeNumber);
	if (!vnp->disk.inodeNumber){
	    /* recoverable inode array not allocated - make one */
	    LogMsg(29, DirDebugLevel, stdout, "DCommit: Allocating inode for dir vnode = %d", 
		   vnp->vnodeNumber);
	    vnp->disk.inodeNumber = (Inode)rvmlib_rec_malloc(sizeof(DirInode));
	    bzero((void *)&shadowInArr, sizeof(DirInode));
	    shadowInArr.refcount = 1;
	    InArrModified = 1;
	}
	else
	    bcopy((const void *)(vnp->disk.inodeNumber), (void *)&shadowInArr, sizeof(DirInode));

	/* get pages of the directory from the hash table */
	volume = vnp->volumePtr;
	fid.volume = V_parentId(volume);
	fid.vnode = vnp->vnodeNumber;
	fid.vunique = vnp->disk.uniquifier;
	LogMsg(29, DirDebugLevel, stdout, "DCommit: Going to get pages for (%u.%d.%d)", 
	       fid.volume, fid.vnode, fid.vunique);
	dlist *dirpages = GetDirShadowPages(&fid, DirHtb);

	/* keep an array of flags to see if a page has been */
	/* modified.  Abort Transaction if multiple pages exist */
	int Committed[MAXPAGES];
	for(int b = 0; b < MAXPAGES; b++)
	    Committed[b] = 0;

	/* process each page of the directory */
	while (sdp = (shadowDirPage *)(dirpages->get())){
	    if (!shadowInArr.Pages[sdp->PageNum]){
		/* this directory page never allocated before */
		LogMsg(29, DirDebugLevel, stdout, "DCommit: Allocating page %d for dir vnode = %d",
		       sdp->PageNum, vnp->vnodeNumber);
		shadowInArr.Pages[sdp->PageNum] = (long *) rvmlib_rec_malloc(PAGESIZE);
		InArrModified = 1;
	    }
	    LogMsg(29, DirDebugLevel, stdout, "DCommit: Modifying page %d for dir vnode %d",
		   sdp->PageNum, vnp->vnodeNumber);
	    if (Committed[sdp->PageNum] == 1){
		LogMsg(0, DirDebugLevel, stdout, "DCommit: Multiple pages (number %d) for directory (%u.%u)",
		       sdp->PageNum, vnp->vnodeNumber, vnp->disk.uniquifier);
		assert(0);
	    }
	    rvmlib_modify_bytes(shadowInArr.Pages[sdp->PageNum], sdp->Data, PAGESIZE);
	    Committed[sdp->PageNum] = 1;
	    delete sdp;
	}
	delete dirpages;
	if (InArrModified){
	    /* modify the recoverable copy  of the inode array */
	    LogMsg(29, DirDebugLevel, stdout, "DCommit: Modifying Inode for dir vnode %d",
		   vnp->vnodeNumber);
	    rvmlib_modify_bytes(vnp->disk.inodeNumber, &shadowInArr, sizeof(DirInode));
	}
    }
    return 0;
}

/* Remove all pages for this vnode from the hash table */
/* Do not write to recoverable storage - simulate a transaction ABORT */
int VN_DAbort(Vnode *vnp) {
    struct VFid fid;
    shadowDirPage *sdp;
    Volume *volume;
    
    if (!vnp || (vnp->disk.type != vDirectory)){
	LogMsg(29, DirDebugLevel, stdout, "DAbort: Vnode not allocated or not a directory");
	return(0);
    }
    
    if (!vnp->changed)
	return(0);
    
    volume = vnp->volumePtr;
    fid.volume = V_parentId(volume);
    fid.vnode = vnp->vnodeNumber;
    fid.vunique = vnp->disk.uniquifier;
    LogMsg(29, DirDebugLevel, stdout, "DAbort: Going to get pages for (%u.%d.%d)", 
	   fid.volume, fid.vnode, fid.vunique);
    dlist *dirpages = GetDirShadowPages(&fid, DirHtb);
    
    while (sdp = (shadowDirPage *)(dirpages->get())) 
	delete sdp;
    delete dirpages;
    return(0);
}

/* Commit all the pages of a directory - assuming Inode exists */
void ICommit(struct VFid *fid, long *inode)
{
  shadowDirPage *sdp;
  DirInode shadowInode;
  int   shadowInodeMod = 0;

  bcopy((const void *)inode, (void *)&shadowInode, sizeof(DirInode));
  LogMsg(29, DirDebugLevel, stdout, "ICommit: going to get pages for (%u.%d.%d)",
	 fid->volume, fid->vnode, fid->vunique);
  dlist *dirpages = GetDirShadowPages(fid, DirHtb);
  while (sdp = (shadowDirPage *)(dirpages->get())){
    if (!shadowInode.Pages[sdp->PageNum]){
      /* this directory page never allocated before */
      LogMsg(29, DirDebugLevel, stdout, "ICommit: Allocating page %d for dir vnode %d", 
	     sdp->PageNum, fid->vnode);
      shadowInode.Pages[sdp->PageNum] = (long *)rvmlib_rec_malloc(PAGESIZE);
      shadowInodeMod = 1;
    }
    LogMsg(29, DirDebugLevel, stdout, "ICommit: Modifying page %d for vnode %d",
	   sdp->PageNum, fid->vnode);
    rvmlib_modify_bytes(shadowInode.Pages[sdp->PageNum], sdp->Data, PAGESIZE);
    delete sdp;
  }
  if (shadowInodeMod)
    rvmlib_modify_bytes(inode, &shadowInode, sizeof(DirInode));
  
}

void DDec(DirInode *inode)
{
    int lcount;
    if (inode){
	lcount = inode->refcount;
	if (lcount == 1){
	    /* Last vnode referencing directory inode - delete it */
	    for (int i = 0; i < MAXPAGES; i++)
		if (inode->Pages[i]){
		    LogMsg(29, DirDebugLevel, stdout, "Deleting page %d for directory", i);
		    rvmlib_rec_free((char *)(inode->Pages[i]));
		}
	    LogMsg(29, DirDebugLevel, stdout, "Deleting inode ");
	    rvmlib_rec_free((char *)inode);
	}
	else 
	    RVMLIB_MODIFY(inode->refcount, --lcount);
    }
    else 
	LogMsg(29, DirDebugLevel, stdout, "Trying to delete a null inode!!!!!");
}

/* copies oldinode to newinode (first allocating space in
 * recoverable storage)
 */
int CopyDirInode(DirInode *oldinode, DirInode **newinode)
{
    DirInode    shadowInode;

    LogMsg(29, DirDebugLevel, stdout, "Entering CopyDirInode(%#08x , %#08x)", oldinode, newinode);
    if (!oldinode){
       LogMsg(29, DirDebugLevel, stdout, "CopyDirInode: Null oldinode");
       return -1;
    }
    *newinode = (DirInode *)rvmlib_rec_malloc(sizeof(DirInode));
    bzero((void *)&shadowInode, sizeof(DirInode));
    for(int i = 0; i < MAXPAGES; i++)
	if (oldinode->Pages[i]){
	    LogMsg(29, DirDebugLevel, stdout, "CopyDirInode: Copying page %d", i);
	    shadowInode.Pages[i] = (long *)rvmlib_rec_malloc(PAGESIZE);
	    rvmlib_modify_bytes(shadowInode.Pages[i], oldinode->Pages[i], PAGESIZE);
	}
    shadowInode.refcount = oldinode->refcount;
    rvmlib_modify_bytes(*newinode, &shadowInode, sizeof(DirInode));
    return 0;
}

/* copies a directory inode into virtual memory */
int VMCopyDirInode(DirInode *oldinode, DirInode **newinode)
{
    *newinode = (DirInode *)malloc(sizeof(DirInode));
    if (*newinode == NULL) 
	return -1;
    bzero((void *)*newinode, sizeof(DirInode));
    for (int i = 0; i < MAXPAGES; i++)
	if (oldinode->Pages[i]){
	    if (((*newinode)->Pages[i] = (long *)malloc(PAGESIZE)) == NULL)
		return -1;
	    bcopy((const void *)oldinode->Pages[i], (void *) (*newinode)->Pages[i], PAGESIZE);
	}
    (*newinode)->refcount = oldinode->refcount;
    return 0;
}

void VMFreeDirInode(DirInode *inode)
{
    if (inode){
	for (int i = 0; i < MAXPAGES; i++)
	    if (inode->Pages[i])
		free(inode->Pages[i]);
	    else 
		continue;
	free(inode);
    }
}
