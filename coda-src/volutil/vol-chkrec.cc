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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/volutil/Attic/vol-chkrec.cc,v 4.1 1997/01/08 21:52:28 rvb Exp $";
#endif /*_BLURB_*/






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
#include <sys/errno.h>
#include <sys/stat.h>
#include <stdio.h>
#include <sys/file.h>
#include <assert.h>

#ifdef __MACH__
#include <libc.h>
#include <sysent.h>
#endif /* __MACH__ */

#if defined(__linux__) || defined(__NetBSD__)
#include <unistd.h>
#include <stdlib.h>
#endif /* __NetBSD__ || LINUX */

#include <camprivate.h>
#include <lwp.h>
#include <lock.h>

#include <mach.h>
#ifdef __cplusplus
}
#endif __cplusplus

#include "vol-rcvheap.h"


#include <util.h>
#include <rvmlib.h>
#include <coda_globals.h>
#include <vice.h>
#include <cvnode.h>
#include <volume.h>
#include <partition.h>
#include <viceinode.h>
#include <vutil.h>
#include <vrdb.h>


PRIVATE int ChkRecAddr(char *address);
PRIVATE int ChkRecSeg(int volindex);
PRIVATE int ChkRecObj(char *addr, int size);

/*
  BEGIN_HTML
  <a name="S_VolChkRec"><strong>Check the recoverable heap for leaks/corruptions</strong></a>
  END_HTML
*/
long S_VolChkRec(RPC2_Handle rpcid, VolumeId volid){
    Volume *vp;
    Error error;
    int status;	    // transaction status variable
    long rc = 0;
    ProgramType *pt;

    LogMsg(9, VolDebugLevel, stdout, "Checking lwp rock in S_VolChkRec");
    assert(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);

    LogMsg(9, VolDebugLevel, stdout, "Entering VolChkRec()");
    CAMLIB_BEGIN_TOP_LEVEL_TRANSACTION_2(CAM_TRAN_NV_SERVER_BASED)
    VInitVolUtil(volumeUtility);
    if (volid){
	vp = VGetVolume(&error, volid);
	if (error) {
	    LogMsg(0, VolDebugLevel, stdout, "S_VolChkRec: failure attaching volume %d", volid);
	    if (error != VNOVOL) {
		VPutVolume(vp);
	    }
	    CAMLIB_ABORT(error);
	}
	
	int volindex = V_volumeindex(vp);
	rc = ChkRecSeg(volindex);
	VPutVolume(vp);
    }
    else {
	for (int i = 0; i < MAXVOLS; i++){
	    LogMsg(9, VolDebugLevel, stdout, "S_VolChkRec: Checking Volume Index %d", i);
	    int rcode = ChkRecSeg(i);
	    if (rcode == -1)
		rc = -1;
	    LogMsg(9, VolDebugLevel, stdout, "S_VolChkRec: Finished Checking Volume %d", i);
	}
	/* check the free vnode lists */
	struct VnodeDiskObject **vnlist;
	vnlist = &(CAMLIB_REC(SmallVnodeFreeList[0]));
	for (i = 0; i < SMALLFREESIZE; i++){
	    if (vnlist[i] && ChkRecAddr((char *)vnlist[i])){
		LogMsg(0, VolDebugLevel, stdout, "S_VolChkRec: Bad Small Free Vnode %d", i);
		rc = -1;
	    }
	}
	vnlist = &(CAMLIB_REC(LargeVnodeFreeList[0]));
	for (i = 0; i < LARGEFREESIZE; i++){
	    if (vnlist[i] && ChkRecAddr((char *)vnlist[i])){
		LogMsg(0, VolDebugLevel, stdout, "S_VolChkRec: Bad Large Free Vnode %d", i);
		rc = -1;
	    }
	}
    }
    CAMLIB_END_TOP_LEVEL_TRANSACTION_2(CAM_PROT_TWO_PHASED, status)
    VDisconnectFS();
    printf("VolChkRec: printing Volume Hash table\n");
    extern void PrintVolumesInHashTable();
    PrintVolumesInHashTable();
    if (status)
	LogMsg(0, VolDebugLevel, stdout, "S_VolShowVnode failed with %d", status);
    return (status?status:rc);
}

PRIVATE int ChkRecSeg(int volindex)
{   int	rc = 0;
    struct VolHead *vol;
    struct VolumeData *voldata;
    vol = &(CAMLIB_REC(VolumeList[volindex]));
    voldata = &(vol->data);
    if (voldata->volumeInfo && ChkRecAddr((char *)(voldata->volumeInfo)) == -1){
	LogMsg(0, VolDebugLevel, stdout, "ChkRecSeg: Disk Data for volume index %d is corrupted", volindex);
	rc = -1;
    }
    if (voldata->volumeInfo && ChkRecObj((char *)(voldata->volumeInfo), sizeof(VolumeDiskData)) == -1){
	LogMsg(0, VolDebugLevel, stdout, "ChkRecSeg: VolumeInfo object 0x%x for index %d is corrupt",
	    voldata->volumeInfo, volindex);
	rc = -1;
    }
    if (voldata->smallVnodeList && ChkRecAddr((char *)voldata->smallVnodeList) == -1){
	LogMsg(0, VolDebugLevel, stdout, "ChkRecSeg: Small Vnode List for volume index %d is corrupted", volindex);
	rc = -1;
    }
    if (voldata->largeVnodeList && ChkRecAddr((char *)voldata->largeVnodeList) == -1){
	LogMsg(0, VolDebugLevel, stdout, "ChkRecSeg: Large Vnode List for volume index %d is corrupted", volindex);
	rc = -1;
    }
    for (int i = 0; i < voldata->smallListSize; i++){
	/* check vnode recoverable storage */
	if (voldata->smallVnodeList[i]){
	    if (ChkRecAddr((char *)(voldata->smallVnodeList[i])) == -1){
		LogMsg(0, VolDebugLevel, stdout, "ChkRecSeg: SmallVnode %d for volume index %d is corrupt", i, volindex);
		rc = -1;
	    }
	}
    }
    for (i = 0; i < voldata->largeListSize; i++){
	/* check vnode recoverable storage */
	if (voldata->largeVnodeList[i]){
	    if (ChkRecAddr((char *)(voldata->largeVnodeList[i])) == -1){
		LogMsg(0, VolDebugLevel, stdout, "ChkRecSeg: LargeVnode %d for volume index %d is corrupt", i, volindex);
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
PRIVATE int ChkRecAddr(char *address)
{
    rcv_heap_header_t *hPtr;
    char           *free_list_ptr;

    hPtr = (rcv_heap_header_t *) (address -
				   sizeof(rcv_heap_header_t));
    if (((u_int) hPtr < camlibRecSegLow)
	 || ((u_int) hPtr > camlibRecSegHigh)){
	LogMsg(0, VolDebugLevel, stdout, "ChkRecAddr: Address 0x%x out of Recoverable heap range", 
	    address);
	return -1;
    }
    free_list_ptr = (char *) (hPtr->fl);
    if ((free_list_ptr < (char *) rcv_heap_free_list)
	 || (free_list_ptr > (char *) (rcv_heap_free_list + NBUCKETS - 1))){
	LogMsg(0, VolDebugLevel, stdout, "ChkRecAddr: Invalid Header for 0x%x", address);
	return -1;
    }
    
    return 0;
}

PRIVATE int ChkRecObj(char *address, int length)
{
    if (((u_int) (address) < camlibRecSegLow)				    
	 || (((u_int) (address) + (length)) > camlibRecSegHigh)
	 || (((u_int) (length)) == 0)){
	LogMsg(0, VolDebugLevel, stdout, "ChkRecObj: Bad Address 0x%x for object - cant fit in camlib segment;", address);
	LogMsg(0, VolDebugLevel, stdout, "ChkRecObj: High = 0x%x, Low = 0x%x, length = %d",
	    camlibRecSegLow, camlibRecSegHigh, length);
	return -1;
    }
    return 0;
}
