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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/volutil/vol-setlogparms.cc,v 4.7 1998/08/31 12:23:51 braam Exp $";
#endif /*_BLURB_*/



#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>
#include <sys/signal.h>

#include <unistd.h>
#include <stdlib.h>

#include <lwp.h>
#include <lock.h>
#include <util.h>
#include <rvmlib.h>

#include <volutil.h>
#ifdef __cplusplus
}
#endif __cplusplus

#include <vice.h>
#include <voltypes.h>
#include <cvnode.h>
#include <volume.h>
#include <vrdb.h>
#include <srv.h>
#include <vutil.h>
#include <logalloc.h>
#include <recov_vollog.h>

extern PMemMgr *LogStore[];

/*
S_VolSetLogParms: Set the parameters for the resolution log
*/
long S_VolSetLogParms(RPC2_Handle rpcid, VolumeId Vid, RPC2_Integer OnFlag, 
		      RPC2_Integer maxlogsize) 
{
    Volume *volptr = 0;
    Error error;
    ProgramType *pt;
    int rc = 0;
    int status = 0;

    VLog(9, "Entering S_VolSetLogParms: rpcid = %d, Volume = %x", 
	 rpcid, Vid);
    
    assert(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);

    RVMLIB_BEGIN_TRANSACTION(restore);
    rc = VInitVolUtil(volumeUtility);
    if (rc != 0){
	rvmlib_abort(rc);
    }
    XlateVid(&Vid);
    volptr = VGetVolume(&error, Vid);

    if (error) {
	LogMsg(0, SrvDebugLevel, stdout, "S_VolSetLogParms: VGetVolume error %d",error);
	rvmlib_abort(error);
    }

    LogMsg(9, SrvDebugLevel, stdout, "S_VolSetLogParms: Got Volume %x",Vid);
    switch ( OnFlag ) {
    case RVMRES:
	volptr->header->diskstuff.ResOn = OnFlag;
	LogMsg(0, SrvDebugLevel, stdout, "S_VolSetLogParms: res flag on volume 0x%x set to %d (resolution enabled)", 
	       Vid, volptr->header->diskstuff.ResOn);
	break;
    case 0:
	volptr->header->diskstuff.ResOn = 0;
	LogMsg(0, SrvDebugLevel, stdout, "S_VolSetLogParms: res flag on volume 0x%x set to %d (resolution disabled)", Vid, OnFlag);
	break;
    case VMRES:
	LogMsg(0, SrvDebugLevel, stdout, "S_VolSetLogParms: VM resolution no longer supported (volume %lx)", Vid);
    default:
	VPutVolume(volptr);
	rvmlib_abort(EINVAL);
    }

    if (maxlogsize != 0) {
	if ((maxlogsize & 0x1F) != 0) {
	    LogMsg(0, SrvDebugLevel, stdout, "S_VolSetLogParms: Log Size has to be a multiple of 32");
	    VPutVolume(volptr);
	    rvmlib_abort(EINVAL);
	}
	if (AllowResolution && V_VMResOn(volptr)) {
	    if (LogStore[V_volumeindex(volptr)]->maxRecordsAllowed > maxlogsize) {
		LogMsg(0, SrvDebugLevel, stdout, "S_VolSetLogParms: Cant reduce log size");
		VPutVolume(volptr);
		rvmlib_abort(EINVAL);
	    }
	    LogMsg(0, SrvDebugLevel, stdout, "S_VolSetLogParms: Changing log size from %d to %d\n", 
		   LogStore[V_volumeindex(volptr)]->maxRecordsAllowed,
		   maxlogsize);
	    LogStore[V_volumeindex(volptr)]->maxRecordsAllowed = maxlogsize;
	    V_maxlogentries(volptr) = maxlogsize;
	}
	if (AllowResolution && V_RVMResOn(volptr) && V_VolLog(volptr)){
	    V_VolLog(volptr)->Increase_Admin_Limit(maxlogsize);
	    LogMsg(0, SrvDebugLevel, stdout, "S_VolSetLogParms: Changed RVM log size to %d\n",
		   maxlogsize);
	}
    }
    VUpdateVolume(&error, volptr);
    if (error) {
	LogMsg(0, SrvDebugLevel, stdout, "S_VolSetLogParms: Error updating volume %x", Vid);
	VPutVolume(volptr);
	rvmlib_abort(error);
    }
    VPutVolume(volptr);
    RVMLIB_END_TRANSACTION(flush, &(status));
    VDisconnectFS();
    if (status == 0) 
	LogMsg(0, VolDebugLevel, stdout, "S_VolSetLogParms: volume %x log parms set", Vid);
    else 
	LogMsg(0, VolDebugLevel, stdout, "S_VolSetLogParms: set log parameters failed for %x", Vid);
    
    return(status?status:0);
}

