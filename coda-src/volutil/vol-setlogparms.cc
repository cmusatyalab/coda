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
#endif

#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>
#include <sys/signal.h>

#include <unistd.h>
#include <stdlib.h>

#include <lwp/lwp.h>
#include <lwp/lock.h>
#include <util.h>
#include <rvmlib.h>

#include <volutil.h>

#ifdef __cplusplus
}
#endif

#include <vice.h>
#include <voltypes.h>
#include <cvnode.h>
#include <volume.h>
#include <vrdb.h>
#include <srv.h>
#include <vutil.h>
#include <recov_vollog.h>

#if 0
extern PMemMgr *LogStore[];
#endif 

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
    rvm_return_t status = RVM_SUCCESS;

    VLog(9, "Entering S_VolSetLogParms: rpcid = %d, Volume = %x", 
	 rpcid, Vid);
    
    CODA_ASSERT(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);

    rc = VInitVolUtil(volumeUtility);
    if (rc != 0){
	    return rc;
    }
    XlateVid(&Vid);
    volptr = VGetVolume(&error, Vid);

    if (error) {
	VLog(0, "S_VolSetLogParms: VGetVolume error %d", error);
	return error;
    }

    VLog(9, "S_VolSetLogParms: Got Volume %x", Vid);
    switch ( OnFlag ) {
    case RVMRES:
	volptr->header->diskstuff.ResOn = OnFlag;
	VLog(0, "S_VolSetLogParms: res flag on volume 0x%x set to %d (resolution enabled)", 
	       Vid, volptr->header->diskstuff.ResOn);
	break;
    case 0:
	volptr->header->diskstuff.ResOn = 0;
	VLog(0, "S_VolSetLogParms: res flag on volume 0x%x set to %d (resolution disabled)", Vid, OnFlag);
	break;
    case VMRES:
	VLog(0, "S_VolSetLogParms: VM resolution no longer supported (volume %lx)", Vid);
    default:
	VPutVolume(volptr);
	return EINVAL;
    }

    rvmlib_begin_transaction(restore);

    if (maxlogsize != 0) {
	if ((maxlogsize & 0x1F) != 0) {
	    VLog(0, "S_VolSetLogParms: Log Size has to be a multiple of 32");
	    rvmlib_abort(EINVAL);
	    goto exit;
	}
	if (AllowResolution && V_RVMResOn(volptr) && V_VolLog(volptr)){
#if 0
            /* admin_limit is private, can't check it here (yet) --JH */
	    if (V_VolLog(volptr)->admin_limit > maxlogsize) {
		VLog(0, "S_VolSetLogParms: Cant reduce log size");
		rvmlib_abort(EINVAL);
                goto exit;
            }
#endif
	    V_VolLog(volptr)->Increase_Admin_Limit(maxlogsize);
	    VLog(0, "S_VolSetLogParms: Changed RVM log size to %d\n",
		   maxlogsize);
	}
    }
    VUpdateVolume(&error, volptr);
    if (error) {
	VLog(0, "S_VolSetLogParms: Error updating volume %x", Vid);
	rvmlib_abort(error);
	goto exit;
    }

    rvmlib_end_transaction(flush, &(status));

 exit:
    VPutVolume(volptr);
    VDisconnectFS();
    if (status == 0) 
	VLog(0, "S_VolSetLogParms: volume %x log parms set", Vid);
    else 
	VLog(0, "S_VolSetLogParms: set log parameters failed for %x", Vid);
    
    return status;
}

