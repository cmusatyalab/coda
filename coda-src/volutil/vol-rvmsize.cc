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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/volutil/vol-rvmsize.cc,v 4.2 1997/02/26 16:04:12 rvb Exp $";
#endif /*_BLURB_*/






#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <stdio.h>

#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif

#include <lwp.h>
#include <lock.h>
#include <util.h>
#include <rvmlib.h>
#ifdef __cplusplus
}
#endif __cplusplus

#include <cvnode.h>
#include <volume.h>
#include <camprivate.h>
#include <vrdb.h>
#include <vutil.h>
#include <index.h>
#include <codadir.h>
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
    RVMLIB_BEGIN_TRANSACTION(restore)
    VInitVolUtil(volumeUtility);

    XlateVid(&VolID);	/* Translate Volid into Replica Id if necessary */
    
    vp = VGetVolume(&error, VolID);
    if (error) {
	LogMsg(0, VolDebugLevel, stdout, "S_VolRVMSize: failure attaching volume %d", VolID);
	if (error != VNOVOL) {
	    VPutVolume(vp);
	}
	rvmlib_abort((int)error);
    }

    /* Location of the volume's data in RVM */
    voldata = &(SRV_RVM(VolumeList[V_volumeindex(vp)]).data);

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
	int tmp;
	assert(vnode->inodeNumber != 0);
	DirInode *dip = (DirInode *)(vnode->inodeNumber);
	tmp = DI_Pages(dip) * DIR_PAGESIZE;
	size +=  tmp;
	data->DirPagesSize += tmp;
	LogMsg(5, VolDebugLevel, stdout, "Dir Vnode %d had length %d.", vnodeindex, tmp);
    }
	
    data->VolumeSize = size;

    VPutVolume(vp);
    RVMLIB_END_TRANSACTION(flush, &(status));
    VDisconnectFS();
    if (status)
	LogMsg(0, VolDebugLevel, stdout, "S_VolRVMSize failed with %d", status);
    return(status);
}


