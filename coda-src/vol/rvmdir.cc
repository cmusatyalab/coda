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

static char *rcsid = "$Header: /afs/cs.cmu.edu/project/coda-braam/src/coda-4.0.1/coda-src/vol/RCS/rvmdir.cc,v 1.1 1996/11/22 19:10:30 braam Exp $";
#endif /*_BLURB_*/






#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#ifdef __MACH__
#include <libc.h>
#include <sysent.h>
#endif /* __MACH__ */
#ifdef __NetBSD__
#include <unistd.h>
#include <stdlib.h>
#endif __NetBSD__
#include <lwp.h>
#include <lock.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>
#include <rvmlib.h>

#include <dhash.h>
#include <vice.h>
#include "cvnode.h"
#include "volume.h"
#include "rvmdir.h"

dhashtab	*DirHtb = 0;  /* initialized by DirHtbInit() */



shadowDirPage::shadowDirPage(struct VFid vfid, int pagenum, char *data)
{
    Fid = vfid;
    PageNum = pagenum;
    bcopy((void *)data, (void *)&Data, PAGESIZE);
}

shadowDirPage::~shadowDirPage()
{
}

void shadowDirPage::print()
{
    print(stdout);
}
void shadowDirPage::print(FILE *fp)
{
    fflush(fp);
    print(fileno(fp));
}

void shadowDirPage::print(int fd)
{
    char buf[80];
    sprintf(buf, "Address of Dirpage:%#08x\n", (long)this);
    write(fd, buf, strlen(buf));

    sprintf(buf, "Fid = %#08x.%#08x.%#08x Page = %d\n", Fid.volume, Fid.vnode, Fid.vunique, PageNum);
    write(fd, buf, strlen(buf));
}


int DirHtbHash(void *key)
{
    VFid    *fid = (VFid *)key;
    return(fid->volume + fid->vnode + fid->vunique);
}

int FidCmp(shadowDirPage *a, shadowDirPage *b)
{
    if (a->Fid.volume < b->Fid.volume) return -1;
    if (a->Fid.volume > b->Fid.volume) return 1;
    if (a->Fid.vnode < b->Fid.vnode) return -1;
    if (a->Fid.vnode > b->Fid.vnode) return 1;
    if (a->Fid.vunique < b->Fid.vunique) return -1;
    if (a->Fid.vunique > b->Fid.vunique) return 1;
    if (a->PageNum < b->PageNum) return -1;
    if (a->PageNum > b->PageNum) return 1;
    return 0;
}

int DirHtbInit()
{
    /* fill in the global hashtable for dir in rvm */
    if (!DirHtb)
	DirHtb = new dhashtab(HTBSIZE, DirHtbHash, (CFN)FidCmp);
    return (0);
    
}

dlist *GetDirShadowPages(struct VFid *fid, dhashtab *htb)
{
    shadowDirPage   *sdp;


    dlist   *dirlist = new dlist((CFN)FidCmp);
    dhashtab_iterator	next(*htb, fid);

    int readahead = 0;
    while (readahead || (sdp = (shadowDirPage *)next())){
	readahead = 0;
	if (fid->volume == sdp->Fid.volume && 
	    fid->vnode == sdp->Fid.vnode && 
	    fid->vunique == sdp->Fid.vunique) {
	    /* found a page - remove from hash tbl and add to the list */
	    LogMsg(29, DirDebugLevel, stdout, "GetDirShadowPages:  Found page %d of fid(%u.%d.%d)", 
		   sdp->PageNum, fid->volume, fid->vnode, fid->vunique);
	    shadowDirPage *tmpsdp = sdp;
	    readahead = ((sdp = (shadowDirPage *)next()) != 0);
	    htb->remove((void *)fid, tmpsdp);
	    dirlist->insert(tmpsdp);
	}
    }
    return(dirlist);
}

/* copies all the pages of a directory from the commit hash table */
/* into recoverable storage */
/* Called from within a transaction */
int DCommit(Vnode *vnp)
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
	LogMsg(29, DirDebugLevel, stdout, "DCommit: deleted directory, vnode = %d", 
	       vnp->vnodeNumber);
	DDec((DirInode *)(vnp->disk.inodeNumber));
	vnp->disk.inodeNumber = 0;
	return 0;
    }
    
    else if (!vnp->delete_me && vnp->changed){
	/* directory was modified - commit the pages */
	LogMsg(29, DirDebugLevel, stdout, "DCommit: Commiting pages for dir vnode = %d", 
	       vnp->vnodeNumber);
	if (!vnp->disk.inodeNumber){
	    /* recoverable inode array not allocated - make one */
	    LogMsg(29, DirDebugLevel, stdout, "DCommit: Allocating inode for dir vnode = %d", 
		   vnp->vnodeNumber);
	    vnp->disk.inodeNumber = (Inode)CAMLIB_REC_MALLOC(sizeof(DirInode));
	    bzero(&shadowInArr, sizeof(DirInode));
	    shadowInArr.refcount = 1;
	    InArrModified = 1;
	}
	else
	    bcopy((void *)(vnp->disk.inodeNumber), &shadowInArr, sizeof(DirInode));

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
		shadowInArr.Pages[sdp->PageNum] = (long *) CAMLIB_REC_MALLOC(PAGESIZE);
		InArrModified = 1;
	    }
	    LogMsg(29, DirDebugLevel, stdout, "DCommit: Modifying page %d for dir vnode %d",
		   sdp->PageNum, vnp->vnodeNumber);
	    if (Committed[sdp->PageNum] == 1){
		LogMsg(0, DirDebugLevel, stdout, "DCommit: Multiple pages (number %d) for directory (%u.%u)",
		       sdp->PageNum, vnp->vnodeNumber, vnp->disk.uniquifier);
		assert(0);
	    }
	    CAMLIB_MODIFY_BYTES(shadowInArr.Pages[sdp->PageNum], sdp->Data, PAGESIZE);
	    Committed[sdp->PageNum] = 1;
	    delete sdp;
	}
	delete dirpages;
	if (InArrModified){
	    /* modify the recoverable copy  of the inode array */
	    LogMsg(29, DirDebugLevel, stdout, "DCommit: Modifying Inode for dir vnode %d",
		   vnp->vnodeNumber);
	    CAMLIB_MODIFY_BYTES(vnp->disk.inodeNumber, &shadowInArr, sizeof(DirInode));
	}
    }
    return 0;
}

/* Remove all pages for this vnode from the hash table */
/* Do not write to recoverable storage - simulate a transaction ABORT */
int DAbort(Vnode *vnp) {
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

  bcopy(inode, &shadowInode, sizeof(DirInode));
  LogMsg(29, DirDebugLevel, stdout, "ICommit: going to get pages for (%u.%d.%d)",
	 fid->volume, fid->vnode, fid->vunique);
  dlist *dirpages = GetDirShadowPages(fid, DirHtb);
  while (sdp = (shadowDirPage *)(dirpages->get())){
    if (!shadowInode.Pages[sdp->PageNum]){
      /* this directory page never allocated before */
      LogMsg(29, DirDebugLevel, stdout, "ICommit: Allocating page %d for dir vnode %d", 
	     sdp->PageNum, fid->vnode);
      shadowInode.Pages[sdp->PageNum] = (long *)CAMLIB_REC_MALLOC(PAGESIZE);
      shadowInodeMod = 1;
    }
    LogMsg(29, DirDebugLevel, stdout, "ICommit: Modifying page %d for vnode %d",
	   sdp->PageNum, fid->vnode);
    CAMLIB_MODIFY_BYTES(shadowInode.Pages[sdp->PageNum], sdp->Data, PAGESIZE);
    delete sdp;
  }
  if (shadowInodeMod)
    CAMLIB_MODIFY_BYTES(inode, &shadowInode, sizeof(DirInode));
  
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
		    CAMLIB_REC_FREE((char *)(inode->Pages[i]));
		}
	    LogMsg(29, DirDebugLevel, stdout, "Deleting inode ");
	    CAMLIB_REC_FREE((char *)inode);
	}
	else 
	    CAMLIB_MODIFY(inode->refcount, --lcount);
    }
    else 
	LogMsg(29, DirDebugLevel, stdout, "Trying to delete a null inode!!!!!");
}

void DInc(DirInode *inode)
{
    int linkcount = inode->refcount;
    CAMLIB_MODIFY(inode->refcount, ++linkcount);
}

void VMDDec(DirInode *inode)
{
    if (inode)
	(inode->refcount)--;
}
void VMDInc(DirInode *inode)
{
    if (inode)
	(inode->refcount)++;
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
    *newinode = (DirInode *)CAMLIB_REC_MALLOC(sizeof(DirInode));
    bzero((void *)&shadowInode, sizeof(DirInode));
    for(int i = 0; i < MAXPAGES; i++)
	if (oldinode->Pages[i]){
	    LogMsg(29, DirDebugLevel, stdout, "CopyDirInode: Copying page %d", i);
	    shadowInode.Pages[i] = (long *)CAMLIB_REC_MALLOC(PAGESIZE);
	    CAMLIB_MODIFY_BYTES(shadowInode.Pages[i], oldinode->Pages[i], PAGESIZE);
	}
    shadowInode.refcount = oldinode->refcount;
    CAMLIB_MODIFY_BYTES(*newinode, &shadowInode, sizeof(DirInode));
    return 0;
}

/* copies a directory inode into virtual memory */
int VMCopyDirInode(DirInode *oldinode, DirInode **newinode)
{
    *newinode = (DirInode *)malloc(sizeof(DirInode));
    if (*newinode == NULL) 
	return -1;
    bzero(*newinode, sizeof(DirInode));
    for (int i = 0; i < MAXPAGES; i++)
	if (oldinode->Pages[i]){
	    if (((*newinode)->Pages[i] = (long *)malloc(PAGESIZE)) == NULL)
		return -1;
	    bcopy(oldinode->Pages[i], (*newinode)->Pages[i], PAGESIZE);
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
