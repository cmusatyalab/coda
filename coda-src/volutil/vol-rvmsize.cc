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

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/coda-src/volutil/vol-rvmsize.cc,v 1.3 1997/01/07 18:43:48 rvb Exp";
#endif /*_BLURB_*/






#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <stdio.h>

#ifdef __MACH__
#include <libc.h>
#include <sysent.h>
#include <mach.h>
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
#include <rvmlib.h>
#include <cvnode.h>
#include <volume.h>
#include <camprivate.h>
#include <vrdb.h>
#include <vutil.h>
#include <index.h>
#include <rvmdir.h>
#include <coda_globals.h>
#include "volutil.h"



/* This routine */
/*
  BEGIN_HTML
  <a name="S_VolRVMSize"><strong>Returns RVM usage by various components of a Volume</strong></a> 
  END_HTML
*/

long S_VolRVMSize(RPC2_Handle rpcid, VolumeId VolID, RVMSize_data *data) {
    Volume *vp;
    Error error;
    struct VolumeData *voldata;
    int status = 0;
    long size = 0;
    ProgramType *pt;

    LogMsg(9, VolDebugLevel, stdout, "Checking lwp rock in S_VolRVMSize");
    assert(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);

    LogMsg(9, VolDebugLevel, stdout, "Entering VolRVMSize()");
    CAMLIB_BEGIN_TOP_LEVEL_TRANSACTION_2(CAM_TRAN_NV_SERVER_BASED)
    VInitVolUtil(volumeUtility);

    XlateVid(&VolID);	/* Translate Volid into Replica Id if necessary */
    
    vp = VGetVolume(&error, VolID);
    if (error) {
	LogMsg(0, VolDebugLevel, stdout, "S_VolRVMSize: failure attaching volume %d", VolID);
	if (error != VNOVOL) {
	    VPutVolume(vp);
	}
	CAMLIB_ABORT((int)error);
    }

    /* Location of the volume's data in RVM */
    voldata = &(CAMLIB_REC(VolumeList[V_volumeindex(vp)]).data);

    /* Size of volume header information */
    size = sizeof(struct VolHead) + sizeof(struct VolumeDiskData); 
    LogMsg(5, VolDebugLevel, stdout, "RVM Size after header %d.", size);

    /* Storage taken by the arrays of list pointers */
    size += sizeof(rec_smolist) * voldata->nsmallLists;
    size += sizeof(rec_smolist) * voldata->nlargeLists;
    LogMsg(5, VolDebugLevel, stdout, "RVM Size after vnode arrays %d.", size);

    /* Storage taken by the Vnodes themselves. A directory consists of the
     * VnodeDiskObject and an RVM "Inode", a file of just the VnodeDiskObject.
     */

    data->nSmallVnodes = voldata->nsmallvnodes;
    data->nLargeVnodes = voldata->nlargevnodes;

    data->SmallVnodeSize = voldata->nsmallvnodes * SIZEOF_SMALLDISKVNODE;
    data->LargeVnodeSize = (voldata->nlargevnodes * 
	(SIZEOF_LARGEDISKVNODE + sizeof(DirInode)));
    size += data->SmallVnodeSize + data->LargeVnodeSize;
    
    /* Find the number of DirPages by iterating through the large vnode array
     * For each vnode within, add PAGESIZE to both size and data->DirPagesSize
     * for each initialized pointer in vnode->Pages.
     */
    char buf[SIZEOF_LARGEDISKVNODE];
    VnodeDiskObject *vnode = (VnodeDiskObject *)&buf;
    
    vindex vol_index(V_id(vp), vLarge, vp->device, VnodeClassInfo_Array[vLarge].diskSize);
    vindex_iterator vnext(vol_index);
    data->DirPagesSize = 0;
    int vnodeindex;
    while ((vnodeindex = vnext(vnode)) != -1) {
	int i;
	assert(vnode->inodeNumber != 0);
	DirInode *dip = (DirInode *)(vnode->inodeNumber);
	for (i = 0; i < MAXPAGES && dip->Pages[i]; i++) ;
	size += (i * PAGESIZE);
	data->DirPagesSize += (i * PAGESIZE);
	LogMsg(5, VolDebugLevel, stdout, "Vnode %d had %d DirPages.", vnodeindex, i);
    }
	
    data->VolumeSize = size;

    VPutVolume(vp);
    CAMLIB_END_TOP_LEVEL_TRANSACTION_2(CAM_PROT_TWO_PHASED, status)
    VDisconnectFS();
    if (status)
	LogMsg(0, VolDebugLevel, stdout, "S_VolRVMSize failed with %d", status);
    return(status);
}


