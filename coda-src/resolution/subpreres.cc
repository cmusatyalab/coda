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





#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <stdio.h>
#include <rpc2.h>
#include <util.h>
#include <rvmlib.h>
#include <util.h>
#include <codadir.h>
#ifdef __cplusplus
}
#endif __cplusplus

#include <errors.h>
#include <res.h>
#include <srv.h>
#include <codadir.h>
#include <vrdb.h>
#include <remotelog.h>
#include <rescomm.h>

#include "resutil.h"
#include "rsle.h"
#include "parselog.h"
#include "ops.h"

/* subpreres.c
 	code executed at each subordinate before 
	proper phases of resolution begin
*/

/* fetch the status & contents of a directory
 used during a resolve just after a repair is completed
*/
long RS_FetchDirContents(RPC2_Handle RPCid, ViceFid *Fid, 
			 RPC2_Integer *length, ViceStatus *status,
			 SE_Descriptor *sed) 
{
    Volume *volptr = 0;
    Vnode *vptr = 0;
    int errorcode = 0;
    char *buf = NULL;
    PDirHandle dh;
    int size = 0;
    rvm_return_t camstatus = RVM_SUCCESS;
    SE_Descriptor sid;

    SLog(9, "RS_FetchDirContents: Fid = %s", FID_(Fid));


    /*get the object */
    {
	if (!XlateVid(&Fid->Volume)){
	    SLog(0, "RS_FetchDirContents: XlateVid %s failed", FID_(Fid));
	    return(EINVAL);
	}
	
	SLog(9, "RS_FetchDirContents: Going to Fetch Object %s", FID_(Fid));
	errorcode = GetFsObj(Fid, &volptr, &vptr, READ_LOCK, NO_LOCK, 1, 0, 0);
	if (errorcode) 
		goto Exit;
    }

    /* dump directory contents in a buffer */
    {
	size = 0;
	dh = VN_SetDirHandle(vptr);
	buf = (char *)malloc(vptr->disk.length + VAclSize(vptr) + 
			     (2 * sizeof(int))/* for quota */);
	CODA_ASSERT(buf);
	bcopy((const void *)DH_Data(dh), buf, vptr->disk.length);
	VN_PutDirHandle(vptr);
        size = vptr->disk.length;
    }

    /* dump the acl after the contents  */
    {
	AL_AccessList *aCL;
	int aCLSize = 0;
	SetAccessList(vptr, aCL, aCLSize);
	bcopy((const void *)VVnodeACL(vptr), (void *)&buf[size], VAclSize(vptr));
	bzero((void *)&buf[size + aCL->MySize - 1], aCLSize - aCL->MySize);
	size += VAclSize(vptr);

	SLog(9,"RS_FetchDirContents: dumpacl: aCL::MySize = %d aCL::TotalNoOfEntries = %d\n",
	       aCL->MySize, aCL->TotalNoOfEntries);
    }
    /* dump the volume quota information into the buffer too 
       for root directory */
    if ((Fid->Vnode == 1) && (Fid->Unique == 1)) {
	int tmp = htonl(V_maxquota(volptr));
	bcopy((char *)&tmp, &buf[size], sizeof(int));
	size += sizeof(int);
	tmp = htonl(V_minquota(volptr));
	bcopy((char *)&tmp, &buf[size], sizeof(int));
	size += sizeof(int);
    }
    /* ship the contents  */
    {
	SLog(9, "RS_FetchDirContents: Shipping dir contents %s", FID_(Fid));
	bzero((void *)&sid, sizeof(SE_Descriptor));
	sid.Tag = SMARTFTP;
	sid.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
	sid.Value.SmartFTPD.SeekOffset = 0;
	sid.Value.SmartFTPD.hashmark = (SrvDebugLevel > 2 ? '#' : '\0');
	sid.Value.SmartFTPD.Tag = FILEINVM;
	sid.Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqLen = size;
	sid.Value.SmartFTPD.FileInfo.ByAddr.vmfile.MaxSeqLen = size;
	sid.Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqBody = (RPC2_ByteSeq)buf;
	    
	if((errorcode = RPC2_InitSideEffect(RPCid, &sid)) 
	   <= RPC2_ELIMIT) {
	    SLog(0, "RS_FetchDirContents:  InitSE failed (%d) fid %s", 
		   errorcode, FID_(Fid));
	    goto Exit;
	}
	
	if ((errorcode = RPC2_CheckSideEffect(RPCid, &sid, SE_AWAITLOCALSTATUS)) 
	    <= RPC2_ELIMIT) {
	    SLog(0, "RS_FetchDirContents: CheckSE failed (%d) fid %s", 
		   errorcode, FID_(Fid));
	    if (errorcode == RPC2_SEFAIL1) 
		    errorcode = EIO;
	    goto Exit;
	}
    }

    // set out params
    {
	    *length = size;
	    SetStatus(vptr, status, 0, 0);
    }
  Exit:
    if (buf) 
	    free(buf);
    rvmlib_begin_transaction(restore);
    SLog(9, "RS_FetchDirContents: Putting back vnode and volume for %s", FID_(Fid));
    if (vptr){
	    Error fileCode = 0;
	    VPutVnode(&fileCode, vptr);
	    CODA_ASSERT(fileCode == 0);
    }
    PutVolObj(&volptr, NO_LOCK);
    rvmlib_end_transaction(flush, &(camstatus));
    CODA_ASSERT(camstatus == 0);
    SLog(2, "RS_FetchDirContents returns code %d, fid %s", errorcode, FID_(Fid));
    return(errorcode);
}

long RS_ClearIncon(RPC2_Handle RPCid, ViceFid *Fid, 
		   ViceVersionVector *VV) 
{
    Vnode *vptr = 0;
    Volume *volptr = 0;
    VolumeId VSGVolnum = Fid->Volume;
    rvm_return_t status = RVM_SUCCESS;
    int errorcode = 0;

    conninfo *cip = GetConnectionInfo(RPCid);
    if (cip == NULL){
	SLog(0, "RS_ClearIncon: Couldnt get conninfo %s", FID_(Fid));
	return(EINVAL);
    }

    if (!XlateVid(&Fid->Volume)) {
	SLog(0, "RS_ClearIncon: Couldnt Xlate VSG for %s", FID_(Fid));
	return(EINVAL);
    }
    
    // get the object 
    if (errorcode = GetFsObj(Fid, &volptr, &vptr, WRITE_LOCK, NO_LOCK, 1, 0, 0)) {
	SLog(0, "RS_ClearIncon: GetFsObj returns error %d, fid %s", 
	       errorcode, FID_(Fid));
	errorcode = EINVAL;
	goto FreeLocks;
    }
    
    // make sure volume is locked by coordinator
    if (V_VolLock(volptr).IPAddress != cip->GetRemoteHost()) {
	SLog(0, "RS_ClearIncon: Volume not locked by coordinator, fid %s", FID_(Fid));
	errorcode = EWOULDBLOCK;
	goto FreeLocks;
    }

    if (SrvDebugLevel >= 9) {
	SLog(9, "ClearIncon: vector passed in is (fid %s):", FID_(Fid));
	PrintVV(stdout, VV);
	SLog(9, "ClearIncon: vector in the vnode is (fid %s) :", FID_(Fid));
	PrintVV(stdout, &Vnode_vv(vptr));
    }
    
    // make sure vectors are equal
    if (VV_Cmp_IgnoreInc(&Vnode_vv(vptr), VV) == VV_EQ) {
	ClearIncon(Vnode_vv(vptr));
	CodaBreakCallBack(0, Fid, VSGVolnum);
    } else {
	errorcode = EINCOMPATIBLE;
	SLog(0, "RS_ClearIncon: Version Vectors are incompatible for %s", FID_(Fid));
	PrintVV(stdout, VV);
	PrintVV(stdout, &Vnode_vv(vptr));
	goto FreeLocks;
    }

FreeLocks:
    rvmlib_begin_transaction(restore);
    /* release lock on vnode and put the volume */
    Error filecode = 0;
    if (vptr) {
	VPutVnode(&filecode, vptr);
	CODA_ASSERT(filecode == 0);
    }
    PutVolObj(&volptr, NO_LOCK);
    rvmlib_end_transaction(flush, &(status));
    SLog(9, "RS_ClearIncon returns %d, fid %s", errorcode, FID_(Fid));
    return(errorcode);
}

