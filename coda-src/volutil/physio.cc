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



/************************************************************************/
/*  physio.c	- Physical I/O routines for the buffer package		*/
/*  *********THIS VERSION ADAPTED FOR USE BY THE SALVAGER****************/
/************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <unistd.h>
#include <stdlib.h>

#include <lwp.h>
#include <lock.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>
#include <vice.h>
#include <cvnode.h>
#include <volume.h>
#include <dirvnode.h>

#define PAGESIZE 2048

void SetDirHandle(DirHandle *dir, Vnode *vnode);


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
	    memcpy(data, nsdp->Data, PAGESIZE);
	    return PAGESIZE;
	}
    }

    DirInode *dinode = (DirInode *)(file->inode);
    if ((dinode->Pages)[block]){
/*	printf("ReallyRead: page number	%d, vnode %d \n", block,
	       file->vnode);
*/	
	memcpy(data, (dinode->Pages)[block], PAGESIZE);
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
	CODA_ASSERT(tmpsdp);
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
    memset((char *)file, 0, sizeof(DirHandle));
}

int FidEq (void *formal_afile, void *formal_bfile)
{
    DirHandle *afile = (DirHandle *)formal_afile;
    DirHandle *bfile = (DirHandle *)formal_bfile;
    return (memcmp((char *)afile, (char *)bfile, sizeof(DirHandle)) == 0);
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
    CODA_ASSERT(0);
}
*/


