/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
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
  S_VolSetWBParms: Set the WBEnable flag ... modified from S_VolSetLogParms
*/
long S_VolSetWBParms(RPC2_Handle rpcid, VolumeId Vid, RPC2_Integer newWBFlag) 
{
    Volume *volptr = 0;
    Error error;
    ProgramType *pt;
    int rc = 0;
    rvm_return_t status = RVM_SUCCESS;
    VLog(9, "Entering S_VolSetWBParms: rpcid = %d, Volume = %x", 
	 rpcid, Vid);
    
    CODA_ASSERT(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);

    rc = VInitVolUtil(volumeUtility);
    if (rc != 0){
	    return rc;
    }
    XlateVid(&Vid);
    volptr = VGetVolume(&error, Vid);

    if (error) {
	VLog(0, "S_VolSetWBParms: VGetVolume error %d", error);
	return error;
    }


    rvmlib_begin_transaction(restore);

    VLog(9, "S_VolSetWBParms: Got Volume %x", Vid);
    VLog(0, "S_VolSetWBParms: WriteBackEnable flag on volume 0x%x set to %d from %d", Vid, newWBFlag,volptr->header->diskstuff.WriteBackEnable);
    volptr->header->diskstuff.WriteBackEnable = newWBFlag;
    
    VUpdateVolume(&error, volptr);
    if (error) {
	VLog(0, "S_VolSetWBParms: Error updating volume %x", Vid);
	VPutVolume(volptr);
	rvmlib_abort(error);
	goto exit;
    }

    rvmlib_end_transaction(flush, &(status));

 exit:
    VPutVolume(volptr);
    VDisconnectFS();
    if (status == 0) 
	VLog(0, "S_VolSetWBParms: volume %x WB flag set", Vid);
    else 
	VLog(0, "S_VolSetWBParms: set WB flag failed for %x", Vid);
    
    return status;
}

