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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/resolution/reslock.cc,v 4.5 1998/08/31 12:23:20 braam Exp $";
#endif /*_BLURB_*/







/* reslock.c 
 * Implements volume locking for resolution
 */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>

#include <stdio.h>
#include <struct.h>
#include "rpc2.h"

#include <sys/ioctl.h>
#include <util.h>
#include <rvmlib.h>

#ifdef __cplusplus
}
#endif __cplusplus


#include <olist.h>
#include <errors.h>
#include <res.h>
#include <vrdb.h>
#include <srv.h>
#include <volume.h>

#include "lockqueue.h"
#include "rescomm.h"
#include "pdlist.h"
#include "reslog.h"
#include "resutil.h"
#include "timing.h"


long RS_LockAndFetch(RPC2_Handle RPCid, ViceFid *Fid,
		     ResFetchType Request, ViceVersionVector *VV, 
		     ResStatus *rstatus,
		     RPC2_Integer *logsize, RPC2_Integer maxcomponents, 
		     RPC2_Integer *ncomponents, ResPathElem *components) {
    LogMsg(1, SrvDebugLevel, stdout,
	   "Entering RS_LockAndFetch(0x%x.%x.%x)\n", 
	   Fid->Volume, Fid->Vnode, Fid->Unique);
    int errorcode = 0;
    int camstatus = 0;
    Volume *volptr = 0;
    Vnode *vptr = 0;
    int ObtainedLock = 0;
    assert(Request == FetchStatus);
    *logsize = 0;
    int nentries  = 0;

    // first set out parameters
    InitVV(VV);
    *logsize = 0;
    *ncomponents = 0;
    bzero((void *)rstatus, sizeof(ResStatus));

    // get info from connection
    conninfo *cip = NULL;
    cip = GetConnectionInfo(RPCid);
    if (cip == NULL){
	LogMsg(0, SrvDebugLevel, stdout,  
	       "RS_LockAndFetch: Couldnt get conn info");
	return(EINVAL);
    }

    // translate replicated VolumeId to rw id 
    if (!XlateVid(&Fid->Volume)){
	LogMsg(0, SrvDebugLevel, stdout,  
	       "RS_LockAndFetch: Couldnt translate Vid %x",
		Fid->Volume);
	return(EINVAL);
    }

#if 0
    unsigned long startgetvolume; 
    if (clockFD > 0) 
	ioctl(clockFD, NSC_GET_COUNTER, &startgetvolume);
    LogMsg(0, SrvDebugLevel, stdout, "Getting volume (timestamp %u) after %u usecs\n",
	   startgetvolume, (startgetvolume - startlockandfetch)/25);
#endif 0
    if (errorcode = GetVolObj(Fid->Volume, &volptr, 
			      VOL_EXCL_LOCK, 1, 
			      cip->GetRemoteHost())) {
	LogMsg(0, SrvDebugLevel, stdout,  "RS_LockAndFetch: Error %d during GetVolObj",
		errorcode);
	return(errorcode);
    }
    ObtainedLock = 1;

#if 0
    unsigned long startgetvnode; 
    if (clockFD > 0) 
	ioctl(clockFD, NSC_GET_COUNTER, &startgetvnode);
    LogMsg(0, SrvDebugLevel, stdout, "Getting vnode (timestamp %u) after %u usecs\n",
	   startgetvnode, (startgetvnode - startgetvolume)/25);
#endif 0

    if (errorcode = GetFsObj(Fid, &volptr, &vptr, 
			  READ_LOCK, NO_LOCK, 1, 0)){/*ignore incon*/
	LogMsg(0, SrvDebugLevel, stdout,  "LockAndFetch: GetFsObj returned error %d", 
		errorcode);
	goto FreeLocks;
    }
#if 0
    unsigned long finishgetobj; 
    if (clockFD > 0) 
	ioctl(clockFD, NSC_GET_COUNTER, &finishgetobj);
    LogMsg(0, SrvDebugLevel, stdout, "Finished getting objs (timestamp %u) after %u usecs\n",
	   finishgetobj, (finishgetobj - startgetvnode)/25);
#endif 0
    // set out parameter 
    bcopy((const void *)&(Vnode_vv(vptr)), (void *) VV, sizeof(ViceVersionVector));
    ObtainResStatus(rstatus, &(vptr->disk));

    /* set log size as the volume log size - 
       that is the max log size a client 
       can send in the collect logs phase */
    if (AllowResolution && V_VMResOn(volptr)) {
	nentries = LogStore[V_volumeindex(volptr)]->maxEntries;
	*logsize = ((nentries == 0) ? 1 : nentries) *
	    LogStore[V_volumeindex(volptr)]->classSize;
	LogMsg(39, SrvDebugLevel, stdout,  "RS_LockAndFetch: Returning logsize = %d", *logsize);
    }
    else if (AllowResolution && V_RVMResOn(volptr)) {
	// set size to max possible 
	// by using rename_rle we hope that all strings will fit 
	// in the buffer length being returned.
	nentries = V_VolLog(volptr)->size;
	assert(nentries > 0);
	
	// *logsize = nentries * (sizeof(recle) + sizeof(rename_rle));
	*logsize = nentries * 200;
	LogMsg(39, SrvDebugLevel, stdout, 
	       "RS_LockAndFetch: Returning recov. logsize = %d\n",
	       *logsize);
    }

    /* if probing and timing is on then init the array */
    if (pathtiming && probingon && vptr->disk.type == vDirectory ) {
	tpinfo = new timing_path(MAXPROBES);
	PROBE(tpinfo, RESBEGIN);
    }

#if 0
    unsigned long startgetpath; 
    if (clockFD > 0) 
	ioctl(clockFD, NSC_GET_COUNTER, &startgetpath);
    LogMsg(0, SrvDebugLevel, stdout, "Starting getpath (timestamp %u) after %u usecs\n",
	   startgetpath, (startgetpath - finishgetobj)/25);
#endif 0

    if (errorcode = GetPath(Fid, (int)maxcomponents, (int *)ncomponents, components)) 
	LogMsg(0, SrvDebugLevel, stdout, 
	       "RS_LockAndFetch:GetPath returns error %d\n", errorcode);
  FreeLocks:
#if 0
    unsigned long starttrans; 
    if (clockFD > 0) 
	ioctl(clockFD, NSC_GET_COUNTER, &starttrans);
    LogMsg(0, SrvDebugLevel, stdout, "Starting transaction (timestamp %u) after %u usecs\n",
	   starttrans, (starttrans - startgetpath)/25);
#endif 0
    
    RVMLIB_BEGIN_TRANSACTION(restore)
	int filecode = 0;
        if (vptr){
	    VPutVnode((Error *)&filecode, vptr);
	    assert(filecode == 0);
	    vptr = 0;
	}
        if (errorcode && volptr && ObtainedLock)
	    /* volume locked but error occured later */
	    /* release the lock */
	    PutVolObj(&volptr, VOL_EXCL_LOCK, 1);
        else if (volptr)
	    PutVolObj(&volptr, NO_LOCK);
    RVMLIB_END_TRANSACTION(flush, &(camstatus));
    if (camstatus){
	LogMsg(0, SrvDebugLevel, stdout,  
	       "LockAndFetch: Error during transaction");
	return(camstatus);
    }
#if 0
    if (clockFD > 0) 
	ioctl(clockFD, NSC_GET_COUNTER, &endlockandfetch);
    LogMsg(0, SrvDebugLevel, stdout,
	   "Returning at timestamp (%u) - transaction  took %u usecs\n",
	   endlockandfetch, (endlockandfetch - starttrans)/25);
    LogMsg(0, SrvDebugLevel, stdout,
	   "LockAndFetch: Took %u usecs\n", (endlockandfetch - startlockandfetch)/25);
#endif 0
    return(errorcode);
}


long RS_UnlockVol(RPC2_Handle RPCid, VolumeId Vid) {
    Volume *volptr = 0;
    int errorcode = 0;
    conninfo *cip;
    cip = GetConnectionInfo(RPCid);
    if (cip == NULL) {
	LogMsg(0, SrvDebugLevel, stdout,  "RS_UnlockVol: Couldnt get conn info");
	return(EINVAL);
    }
    
    if (!XlateVid(&Vid)){
	LogMsg(0, SrvDebugLevel, stdout,  "RS_UnlockVol: Couldnt XlateVid %x",
		Vid);
	return(EINVAL);
    }
    
    /* get volume and check if locked */
    if (errorcode = GetVolObj(Vid, &volptr, 
			      VOL_NO_LOCK, 0, 0)) {
	LogMsg(0, SrvDebugLevel, stdout,  "RS_UnlockVol: GetVolObj error %d", 
		errorcode);
	return(errorcode);
    }
    /* make sure unlocker is locker */
    if (V_VolLock(volptr).IPAddress != cip->GetRemoteHost()){
	LogMsg(0, SrvDebugLevel, stdout,  "RS_UnlockVol: unlocker != locker ");
	VPutVolume(volptr);
	return(EINVAL);	/* define new error codes */
    }
    PutVolObj(&volptr, VOL_EXCL_LOCK, 1);
    LogMsg(1, SrvDebugLevel, stdout,  "RS_UnlockVol finished successfully");

    PROBE(tpinfo, RESEND);
    return(0);
}




