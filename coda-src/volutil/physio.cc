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

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/coda-src/volutil/physio.cc,v 1.3 1997/01/07 18:43:34 rvb Exp";
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



/************************************************************************/
/*  physio.c	- Physical I/O routines for the buffer package		*/
/*  *********THIS VERSION ADAPTED FOR USE BY THE SALVAGER****************/
/************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#ifdef __MACH__
#include <libc.h>
#include <sysent.h>
#endif /* __MACH__ */

#if defined(__linux__) || defined(__NetBSD__)
#include <unistd.h>
#include <stdlib.h>
#endif /* __NetBSD__ || LINUX */

#include <lwp.h>
#include <lock.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>
#include <vice.h>
#include <cvnode.h>
#include <volume.h>
#include <rvmdir.h>

#define PAGESIZE 2048

void SetDirHandle(DirHandle *dir, Vnode *vnode);


void SetDirHandle(register DirHandle *dir, register Vnode *vnode)
{
    register Volume *volume = vnode->volumePtr;
    dir->inode = vnode->disk.inodeNumber;
    dir->device = (unsigned short) volume->device;
    dir->cacheCheck = volume->cacheCheck;
    dir->volume = V_parentId(volume);
    dir->unique = vnode->disk.uniquifier;
    dir->vnode = vnode->vnodeNumber;
}

void SetSalvageDirHandle(register DirHandle *dir, int volume, int device, int inode)
{
    private SalvageCacheCheck = 1;
    bzero((char *)dir, sizeof(DirHandle));
    dir->inode = inode;
    dir->device = device;
    dir->volume = volume;
    dir->cacheCheck = SalvageCacheCheck++;  /* Always re-read for a new dirhandle */
}


int ReallyRead (void *formal_file, long block, char *data)
{
    DirHandle *file = (DirHandle *)formal_file; /* keeps C++ happy */

    if (!file->inode){
	printf("ReallyRead: no inode allocated for vnode %d\n", file->vnode);
	return 0;
    }
    /* check hash table for page first */
    {
	struct VFid fid;
	fid.volume = file->volume;
	fid.vnode = file->vnode;
	fid.vunique = file->unique;	
	    
	shadowDirPage tempsdp(fid, block, data);
	dhashtab_iterator next(*DirHtb, (void *)&fid);
	shadowDirPage *nsdp;
	while (nsdp = (shadowDirPage *)next()) {
	    if (!FidCmp(nsdp, &tempsdp)) {
		printf("WARNING: ReallyRead page %d for directory %x.%x in hashtable\n",
		       block, file->vnode, file->unique);
		break;
	    }
	}
	if (nsdp) {
	    bcopy(nsdp->Data, data, PAGESIZE);
	    return PAGESIZE;
	}
    }

    DirInode *dinode = (DirInode *)(file->inode);
    if ((dinode->Pages)[block]){
/*	printf("ReallyRead: page number	%d, vnode %d \n", block,
	       file->vnode);
*/	
	bcopy((void *)((dinode->Pages)[block]), data, PAGESIZE);
	return PAGESIZE;
    }
    else {
	printf("ReallyRead: vnode %d pagenum %d not allocated\n", file->vnode, block);
	return 0;
    }
}

/* Inserts the page into the commit hash table from which pages are */
/* written to recoverable storage on a DCommit */
int ReallyWrite (void *formal_file, long block, char *data)
{
    DirHandle *file = (DirHandle *)formal_file; /* keeps C++ happy */

    extern VolumeChanged;
    struct VFid	fid;
    fid.volume = file->volume;
    fid.vnode = file->vnode;
    fid.vunique = file->unique;
    shadowDirPage *sdp = new shadowDirPage(fid, block, data);
    /* add page to hashtable */

    if (DirHtb->IsMember(&fid, sdp)) {
	/* first remove old page */
	dhashtab_iterator next(*DirHtb, (void *)&fid);
	shadowDirPage *tmpsdp;
	while (tmpsdp = (shadowDirPage *)next()) {
	    if (!FidCmp(tmpsdp, sdp))
		break;
	}
	assert(tmpsdp);
	DirHtb->remove(&fid, tmpsdp);
	delete tmpsdp;
    }
    DirHtb->insert(&fid, sdp);

    VolumeChanged = 1;
    return 1;
    /* the volume changed flag should be set in the ReallyFlush call */
}

void FidZap (void *formal_file)
{
    DirHandle *file = (DirHandle *)formal_file;
    bzero((char *)file, sizeof(DirHandle));
}

int FidEq (void *formal_afile, void *formal_bfile)
{
    DirHandle *afile = (DirHandle *)formal_afile;
    DirHandle *bfile = (DirHandle *)formal_bfile;
    return (bcmp((char *)afile, (char *)bfile, sizeof(DirHandle)) == 0);
}

void FidCpy (void *formal_tofile, void *formal_fromfile)
{
    DirHandle *tofile = (DirHandle *)formal_tofile;
    DirHandle *fromfile = (DirHandle *)formal_fromfile;
    *tofile = *fromfile;
}

/* 
void Die (char *msg)
{
    printf("%s\n", msg);
    assert(0);
}
*/


