/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2016 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <lwp/lwp.h>
#include <lwp/lock.h>
#include <util.h>
#include <rvmlib.h>
#include <codadir.h>

#ifdef __cplusplus
}
#endif

#include <cvnode.h>
#include <volume.h>
#include <camprivate.h>
#include <vrdb.h>
#include <vutil.h>
#include <index.h>
#include <coda_globals.h>
#include "volutil.h"



/*
  S_VolRVMSize: Returns RVM usage by various components of a Volume
*/

long S_VolRVMSize(RPC2_Handle rpcid, VolumeId VolID, RVMSize_data *data) {
    Volume *vp;
    Error error;
    struct VolumeData *voldata;
    int status = 0;
    long size = 0;

    LogMsg(9, VolDebugLevel, stdout, "Entering VolRVMSize()");
    VInitVolUtil(volumeUtility);

    XlateVid(&VolID);	/* Translate Volid into Replica Id if necessary */
    
    vp = VGetVolume(&error, VolID);
    if (error) {
	LogMsg(0, VolDebugLevel, stdout, "S_VolRVMSize: failure attaching volume %d", VolID);
	if (error != VNOVOL) {
	    VPutVolume(vp);
	}
	VDisconnectFS();
	return error;
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
	(SIZEOF_LARGEDISKVNODE + sizeof(struct DirInode)));
    size += data->SmallVnodeSize + data->LargeVnodeSize;
    
    /* Find the number of DirPages by iterating through the large vnode array
     * For each vnode within, add PAGESIZE to both size and data->DirPagesSize
     * for each initialized pointer in vnode->Pages.
     */
    char buf[SIZEOF_LARGEDISKVNODE];
    VnodeDiskObject *vnode = (VnodeDiskObject *)&buf;
    
    vindex vol_index(V_id(vp), vLarge, V_device(vp), VnodeClassInfo_Array[vLarge].diskSize);
    vindex_iterator vnext(vol_index);
    data->DirPagesSize = 0;
    int vnodeindex;
    while ((vnodeindex = vnext(vnode)) != -1) {
	int tmp;
	PDirInode dip = vnode->node.dirNode;
	CODA_ASSERT(dip);
	tmp = DI_Pages(dip) * DIR_PAGESIZE;
	size +=  tmp;
	data->DirPagesSize += tmp;
	LogMsg(5, VolDebugLevel, stdout, "Dir Vnode %d had length %d.", vnodeindex, tmp);
    }
	
    data->VolumeSize = size;

    VPutVolume(vp);
    VDisconnectFS();
    if (status)
	LogMsg(0, VolDebugLevel, stdout, "S_VolRVMSize failed with %d", status);
    return(status);
}


