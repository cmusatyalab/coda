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
                           none currently

#*/




extern void PrintVolumesInHashTable();


ACKKK!!!! As I understand things this file should not be compiled as part
    of the coda file system!!!!!! It requires camelot include files. GackkK!!!!
    (dcs 2/10/93)


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <sys/time.h>
#include <ctype.h>
#include <sys/param.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdio.h>
#include <sys/file.h>
#include "coda_assert.h"
#include <unistd.h>
#include <stdlib.h>

#include <camprivate.h>
#include <lwp.h>
#include <lock.h>

#include <util.h>
#include <rvmlib.h>
#include <vice.h>
#ifdef __cplusplus
}
#endif __cplusplus

#include "vol-rcvheap.h"

#include <coda_globals.h>
#include <cvnode.h>
#include <volume.h>
#include <partition.h>
#include <viceinode.h>
#include <vutil.h>
#include <vrdb.h>


static int ChkRecAddr(char *address);
static int ChkRecSeg(int volindex);
static int ChkRecObj(char *addr, int size);

/*
  S_VolChkRec: Check the recoverable heap for leaks/corruptions
*/
long S_VolChkRec(RPC2_Handle rpcid, VolumeId volid)
{
    Volume *vp;
    Error error;
    int status;	    // transaction status variable
    long rc = 0;
    ProgramType *pt;

    VLog(9, "Checking lwp rock in S_VolChkRec");
    CODA_ASSERT(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);

    VLog(9, "Entering VolChkRec()");
    rvmlib_begin_transaction(restore)
    VInitVolUtil(volumeUtility);
    if (volid){
	vp = VGetVolume(&error, volid);
	if (error) {
	    VLOG(0, "S_VolChkRec: failure attaching volume %d", volid);
	    if (error != VNOVOL) {
		    VPutVolume(vp);
	    }
	    rvmlib_abort(error);
	    return error;
	}
	
	int volindex = V_volumeindex(vp);
	rc = ChkRecSeg(volindex);
	VPutVolume(vp);
    } else {
	    for (int i = 0; i < MAXVOLS; i++){
		    VLOG(9, "S_VolChkRec: Checking Volume Index %#x", i);
		    int rcode = ChkRecSeg(i);
		    if (rcode == -1)
			    rc = -1;
		    VLOG(9, "S_VolChkRec: Finished Checking Volume %#x", i);
	}
	/* check the free vnode lists */
	struct VnodeDiskObject **vnlist;
	vnlist = &(SRV_RVM(SmallVnodeFreeList[0]));
	for (i = 0; i < SMALLFREESIZE; i++){
	    if (vnlist[i] && ChkRecAddr((char *)vnlist[i])){
		VLOG(0, "S_VolChkRec: Bad Small Free Vnode %d", i);
		rc = -1;
	    }
	}
	vnlist = &(SRV_RVM(LargeVnodeFreeList[0]));
	for (i = 0; i < LARGEFREESIZE; i++){
	    if (vnlist[i] && ChkRecAddr((char *)vnlist[i])){
		VLOG(0, "S_VolChkRec: Bad Large Free Vnode %d", i);
		rc = -1;
	    }
	}
    }
    rvmlib_end_transaction(flush, &(status));
    VDisconnectFS();
    VLOG(0, "VolChkRec: printing Volume Hash table\n");
    PrintVolumesInHashTable();
    if (status)
	VLOG(0, "S_VolShowVnode failed with %d", status);
    return (status?status:rc);
}

static int ChkRecSeg(int volindex)
{   int	rc = 0;
    struct VolHead *vol;
    struct VolumeData *voldata;
    vol = &(SRV_RVM(VolumeList[volindex]));
    voldata = &(vol->data);
    if (voldata->volumeInfo && ChkRecAddr((char *)(voldata->volumeInfo)) == -1){
	VLog(0, "ChkRecSeg: Disk Data for volume index %d is corrupted", volindex);
	rc = -1;
    }
    if (voldata->volumeInfo && ChkRecObj((char *)(voldata->volumeInfo), sizeof(VolumeDiskData)) == -1){
	VLog(0, "ChkRecSeg: VolumeInfo object 0x%x for index %d is corrupt",
	    voldata->volumeInfo, volindex);
	rc = -1;
    }
    if (voldata->smallVnodeList && ChkRecAddr((char *)voldata->smallVnodeList) == -1){
	VLog(0, "ChkRecSeg: Small Vnode List for volume index %d is corrupted", volindex);
	rc = -1;
    }
    if (voldata->largeVnodeList && ChkRecAddr((char *)voldata->largeVnodeList) == -1){
	VLog(0, "ChkRecSeg: Large Vnode List for volume index %d is corrupted", volindex);
	rc = -1;
    }
    for (int i = 0; i < voldata->smallListSize; i++){
	/* check vnode recoverable storage */
	if (voldata->smallVnodeList[i]){
	    if (ChkRecAddr((char *)(voldata->smallVnodeList[i])) == -1){
		VLog(0, "ChkRecSeg: SmallVnode %d for volume index %d is corrupt", i, volindex);
		rc = -1;
	    }
	}
    }
    for (i = 0; i < voldata->largeListSize; i++){
	/* check vnode recoverable storage */
	if (voldata->largeVnodeList[i]){
	    if (ChkRecAddr((char *)(voldata->largeVnodeList[i])) == -1){
		VLog(0, "ChkRecSeg: LargeVnode %d for volume index %d is corrupt", i, volindex);
		rc = -1;
	    }
	}
    }
    return rc;
}

extern u_int camlibRecSegLow, camlibRecSegHigh;
extern rcv_heap_free_list_t *rcv_heap_free_list;

/* Does the same checks that RECFREE does 
 * returns -1 if there is an error; 0 otherwise
 */
static int ChkRecAddr(char *address)
{
    rcv_heap_header_t *hPtr;
    char           *free_list_ptr;

    hPtr = (rcv_heap_header_t *) (address -
				   sizeof(rcv_heap_header_t));
    if (((u_int) hPtr < camlibRecSegLow)
	 || ((u_int) hPtr > camlibRecSegHigh)){
	VLog(0, "ChkRecAddr: Address 0x%x out of Recoverable heap range", 
	    address);
	return -1;
    }
    free_list_ptr = (char *) (hPtr->fl);
    if ((free_list_ptr < (char *) rcv_heap_free_list)
	 || (free_list_ptr > (char *) (rcv_heap_free_list + NBUCKETS - 1))){
	VLog(0, "ChkRecAddr: Invalid Header for 0x%x", address);
	return -1;
    }
    
    return 0;
}

static int ChkRecObj(char *address, int length)
{
    if (((u_int) (address) < camlibRecSegLow)				    
	 || (((u_int) (address) + (length)) > camlibRecSegHigh)
	 || (((u_int) (length)) == 0)){
	VLog(0, "ChkRecObj: Bad Address 0x%x for object - cant fit in camlib segment;", address);
	VLog(0, "ChkRecObj: High = 0x%x, Low = 0x%x, length = %d",
	    camlibRecSegLow, camlibRecSegHigh, length);
	return -1;
    }
    return 0;
}
