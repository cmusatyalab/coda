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



/* Clone a volume.  Assumes the new volume is already created */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <stdio.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/inode.h>
#include <sys/time.h>
#ifdef __BSD44__
#include <fstab.h>
#endif
#include <unistd.h>
#include <stdlib.h>
#include <lwp.h>
#include <lock.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>
#include <vice.h>
#include "voltypes.h"
#include "cvnode.h"
#include "volume.h"
#include "partition.h"
#include "viceinode.h"
#include "vutil.h"
#include "recov.h"
#include "index.h"
#include "coda_assert.h"


static void CloneIndex(Volume *ovp, Volume *cvp, Volume *dvp, VnodeClass vclass);
static void FinalDelete(Volume *vp);

void CloneVolume(Error *error, Volume *original, Volume *newv, Volume *old)
{
    *error = 0;
    CloneIndex(original,newv,old,vLarge);
    CloneIndex(original,newv,old,vSmall);
    CopyVolumeHeader(&V_disk(original), &V_disk(newv));
    FinalDelete(old);
}

/* Note: routine CopyInode was removed because it was commented out */

static void CloneIndex(Volume *ovp, Volume *cvp, Volume *dvp, VnodeClass vclass)
/*    Volume *ovp,	old volume */
/*	   *cvp,	new cloned backup volume */
/*	   *dvp;	old cloned backup which is to be deleted */
{
    int dfile = 1;
    long offset = 0;
    Inode dinode;
    struct VnodeClassInfo *vcp = &VnodeClassInfo[vclass];
    char buf[SIZEOF_LARGEDISKVNODE];
    struct VnodeDiskObject *vnode = (struct VnodeDiskObject *) buf;
    char dbuf[SIZEOF_LARGEDISKVNODE];
    struct VnodeDiskObject *dvnode = (struct VnodeDiskObject *) dbuf;
    Device device = ovp->device;
    int RWOriginal = VolumeWriteable(ovp);

    vindex oindex(V_id(ovp), vclass, device, vcp->diskSize);
    vindex_iterator onext(oindex);
    vindex cindex(V_id(cvp), vclass, device, vcp->diskSize);
    vindex dindex(V_id(dvp), vclass, device, vcp->diskSize);
    if (!dvp)
	dfile = 0;

    while((offset = onext(vnode)) != -1) {
	if (dfile && (dindex.oget(offset, dvnode) != -1))
	    dinode = dvnode->inodeNumber;
	else
	    dinode = 0;
        if (vnode->type != vNull) {
	    CODA_ASSERT(vnode->vnodeMagic == vcp->magic);
	    if (vnode->type == vDirectory && RWOriginal) {
	    	vnode->cloned = 1;
		/* NOTE:  the dataVersion++ is incredibly important!!!.
		   This will cause the inode created by the file server
		   on copy-on-write to be stamped with a dataVersion bigger
		   than the current one.  The salvager will then do the
		   right thing */
		vnode->dataVersion++;
		/* write out the changed old vnode */
		CODA_ASSERT(oindex.oput(offset, vnode) == 0);
		/* Turn clone flag off for the cloned volume, just for
		   cleanliness */
		vnode->cloned = 0;
		vnode->dataVersion--; /* Really needs to be set to the value in the inode,
					 for the read-only volume */
	    }
	    if (vnode->inodeNumber == dinode) {
	        dinode = 0;
	    }
	    else if (vnode->inodeNumber) {
		/* a new clone is also referencing this inode */
	        CODA_ASSERT(iinc(device, vnode->inodeNumber, V_parentId(ovp)) != -1);
	    }
	}
	if (dinode) {
	    /* the old backup volume is no longer referencing this vnode */
	    CODA_ASSERT(idec(device, dinode, V_parentId(ovp)) != -1);
	}
	/* write out the vnode to the new cloned volume index */
	CODA_ASSERT(cindex.oput(offset, vnode) == 0);
    }

    /* Isn't this redundant? ***ehs***/
    vindex_iterator dnext(dindex);
    while (dfile && ((offset = dnext(dvnode)) != -1)) {
	CODA_ASSERT(idec(device, dvnode->inodeNumber, V_parentId(ovp)) != -1);
    }
}

static void FinalDelete(Volume *vp)
{
    /* Delete old backup -- it's vnodes are already gone */
    if (vp) {
        Error error;
	CODA_ASSERT(DeleteVolume(V_id(vp)) == 0);
	VDetachVolume(&error, vp);
    }
}
