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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/resolution/reslock.cc,v 4.7 1998/11/02 16:45:10 rvb Exp $";
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
		     RPC2_Integer *ncomponents, ResPathElem *components) 
{
    int errorcode = 0;
    Volume *volptr = 0;
    Vnode *vptr = 0;
    int ObtainedLock = 0;
    *logsize = 0;
    int nentries  = 0;

    SLog(1, "Entering RS_LockAndFetch %s\n", FID_(Fid));
    CODA_ASSERT(Request == FetchStatus);

    // first set out parameters
    InitVV(VV);
    *logsize = 0;
    *ncomponents = 0;
    bzero((void *)rstatus, sizeof(ResStatus));

    // get info from connection
    conninfo *cip = NULL;
    cip = GetConnectionInfo(RPCid);
    if (cip == NULL){
	SLog(0,  "RS_LockAndFetch: Couldnt get conn info for %s", FID_(Fid));
	return(EINVAL);
    }

    // translate replicated VolumeId to rw id 
    if (!XlateVid(&Fid->Volume)){
	    SLog(0,  "RS_LockAndFetch: Couldnt translate Vid for %s", 
		 FID_(Fid));
	    return EINVAL;
    }

    if (errorcode = GetVolObj(Fid->Volume, &volptr, 
			      VOL_EXCL_LOCK, 1, 
			      cip->GetRemoteHost())) {
	    SLog(0,  "RS_LockAndFetch: Error %d during GetVolObj for %s",
		 errorcode, FID_(Fid));
	    return errorcode;
    }
    ObtainedLock = 1;

    /* this Vnode is obtained to get at its logs, and then put away */
    if (errorcode = GetFsObj(Fid, &volptr, &vptr, 
			  READ_LOCK, NO_LOCK, 1, 0, 0)){/*ignore incon*/
	    SLog(0,  "RS_LockAndFetch: GetFsObj for %s returned error %d", 
		 FID_(Fid), errorcode);
	    goto FreeLocks;
    }

    // set out parameter 
    bcopy((const void *)&(Vnode_vv(vptr)), (void *) VV, 
	  sizeof(ViceVersionVector));
    ObtainResStatus(rstatus, &(vptr->disk));

    /* set log size as the volume log size - 
       that is the max log size a client 
       can send in the collect logs phase */
    if (AllowResolution && V_VMResOn(volptr)) {
	    nentries = LogStore[V_volumeindex(volptr)]->maxEntries;
	    *logsize = ((nentries == 0) ? 1 : nentries) *
		    LogStore[V_volumeindex(volptr)]->classSize;
	    SLog(39,  "RS_LockAndFetch: Returning logsize = %d", *logsize);
    } else if (AllowResolution && V_RVMResOn(volptr)) {
	    // set size to max possible 
	    // by using rename_rle we hope that all strings will fit 
	    // in the buffer length being returned.
	    nentries = V_VolLog(volptr)->size;
	    CODA_ASSERT(nentries > 0);
	
	    // *logsize = nentries * (sizeof(recle) + sizeof(rename_rle));
	    *logsize = nentries * 200;
	    SLog(39, "RS_LockAndFetch: Returning recov. logsize = %d\n",
		 *logsize);
    }

    /* if probing and timing is on then init the array */
    if (pathtiming && probingon && vptr->disk.type == vDirectory ) {
	    tpinfo = new timing_path(MAXPROBES);
	    PROBE(tpinfo, RESBEGIN);
    }


    if (errorcode = GetPath(Fid, (int)maxcomponents, (int *)ncomponents, 
			    components)) 
	    SLog(0, "RS_LockAndFetch:GetPath for %s returns error %d\n", 
		 FID_(Fid), errorcode);
 FreeLocks:
    
    int filecode = 0;
    if (vptr){
	    VPutVnode((Error *)&filecode, vptr);
	    CODA_ASSERT(filecode == 0);
	    vptr = 0;
    }
    if (errorcode && volptr && ObtainedLock)
	    /* volume locked but error occured later */
	    /* release the lock */
	    PutVolObj(&volptr, VOL_EXCL_LOCK, 1);
    else if (volptr)
	    PutVolObj(&volptr, NO_LOCK);


    return(errorcode);
}


long RS_UnlockVol(RPC2_Handle RPCid, VolumeId Vid) 
{
    Volume *volptr = 0;
    int errorcode = 0;
    conninfo *cip;
    cip = GetConnectionInfo(RPCid);
    if (cip == NULL) {
	SLog(0,  "RS_UnlockVol: Couldnt get conn info");
	return(EINVAL);
    }
    
    if (!XlateVid(&Vid)){
	SLog(0,  "RS_UnlockVol: Couldnt XlateVid %x",
		Vid);
	return(EINVAL);
    }
    
    /* get volume and check if locked */
    if (errorcode = GetVolObj(Vid, &volptr, 
			      VOL_NO_LOCK, 0, 0)) {
	SLog(0,  "RS_UnlockVol: GetVolObj error %d for %x", 
		errorcode, Vid);
	return(errorcode);
    }
    /* make sure unlocker is locker */
    if (V_VolLock(volptr).IPAddress != cip->GetRemoteHost()){
	    SLog(0,  "RS_UnlockVol: unlocker != locker for %x", Vid);
	    VPutVolume(volptr);
	    return(EINVAL);	/* define new error codes */
    }
    PutVolObj(&volptr, VOL_EXCL_LOCK, 1);
    SLog(1,  "RS_UnlockVol finished successfully for %x", Vid);

    PROBE(tpinfo, RESEND);
    return(0);
}




