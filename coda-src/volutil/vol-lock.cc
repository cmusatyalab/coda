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






/*
 * This module holds routines which lock and unlock volumes for volutil.
 * Currently these routines are only used as part of the backup mechanism.
 */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "coda_string.h"

#include <lwp/lwp.h>
#include <lwp/lock.h>
#include <volutil.h>
#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>
#include <vice.h>
#include <voltypes.h>
#include <cvnode.h>
#include <volume.h>
#include <srv.h>
#include <vutil.h>


/*
  BEGIN_HTML
  <a name="S_VolLock"><strong>Lock the volume for backups.
  Return the VVV for the volume if successful  </strong></a> 
  END_HTML
*/
long S_VolLock(RPC2_Handle rpcid, VolumeId Vid, ViceVersionVector *VolVV) {
    Volume *volptr = 0;
    ProgramType *pt;
    Error error;
    int rc = 0;
    
    CODA_ASSERT(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);
    LogMsg(2, VolDebugLevel, stdout, "Entering S_VolLock: rpcid = %d, Volume = %x", rpcid, Vid);

    rc = VInitVolUtil(volumeUtility);
    if (rc != 0)
	return rc;

    volptr = VGetVolume(&error, Vid);
    if (error) {
	LogMsg(0, SrvDebugLevel, stdout, "S_VolLock: VGetVolume error %d",error);
	VDisconnectFS();
	return(error);
    }

    LogMsg(9, SrvDebugLevel, stdout, "S_VolLock: Got Volume %x",Vid);

    if (V_VolLock(volptr).IPAddress) {
	/* Lock is taken by somebody else, return EWOULDBLOCK */
	/* Treat locks for backup as exclusive locks. */
	LogMsg(0, 0, stdout, "S_VolLock:Volume %x already locked by %x",
	       Vid, V_VolLock(volptr).IPAddress);
	VPutVolume(volptr);
	volptr = 0;	
	VDisconnectFS();
	return(EWOULDBLOCK);
    }
    
    /* Lock the volume */
    V_VolLock(volptr).WriteLockType = VolUtil;
    V_VolLock(volptr).IPAddress = 5;			/* NEEDS CHANGING */
    LogMsg(3, SrvDebugLevel, stdout, "S_VolLock: Obtaining WriteLock....");
    ObtainWriteLock(&(V_VolLock(volptr).VolumeLock));
    LogMsg(3, SrvDebugLevel, stdout, "S_VolLock: Obtained WriteLock.");

    /* Since these values might have been cleared by ObtainWriteLock --
     * I was forced to block and somebody else did an unlock, reset them again. */
    V_VolLock(volptr).WriteLockType = VolUtil; 
    V_VolLock(volptr).IPAddress = 5;			/* NEEDS CHANGING */

    /* Put volume on lock queue to have it time out? */
    /* lqent *lqep = new lqent(Vid); */
    /* LockQueueMan->add(lqep); */

    /* Return the volume's VVV */
    bcopy((const void *)&(V_versionvector(volptr)), (void *)VolVV, sizeof(ViceVersionVector));
    VPutVolume(volptr);
    VDisconnectFS();
    return(0);
}

    
/*
  BEGIN_HTML
  <a name="S_VolUnlock"><strong>Unlock the volume</strong></a> 
  END_HTML
*/
long S_VolUnlock(RPC2_Handle rpcid, VolumeId Vid) {
    Volume *volptr = 0;
    ProgramType *pt;
    int rc = 0;
    Error error;

    CODA_ASSERT(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);
    LogMsg(2, VolDebugLevel, stdout, "Entering S_VolUnlock: rpcid = %d, Volume = %x", rpcid, Vid);

    rc = VInitVolUtil(volumeUtility);
    if (rc != 0)
	return rc;

    /* get volume and check if locked */
    volptr = VGetVolume(&error, Vid);
    LogMsg(9, SrvDebugLevel, stdout, "S_VolUnlock: Got Volume %x", Vid);

    if (error) {
	LogMsg(0, SrvDebugLevel, stdout, "S_VolUnlock: VGetVolume error %d, volume %x", error, Vid);
	VDisconnectFS();
	return(error);
    }

    if (V_VolLock(volptr).IPAddress == 0){
	LogMsg(0, VolDebugLevel, stdout, "Unlock: Locker Id doesn't match Id of lock!");
	VPutVolume(volptr);
	VDisconnectFS();
	return(EINVAL);
    }

    if ((V_VolLock(volptr).WriteLockType != VolUtil)) {
	LogMsg(0, VolDebugLevel, stdout, "S_VolUnlock: unlocker != locker ");
	VPutVolume(volptr);
	VDisconnectFS();
	return(EINVAL);	/* define new error codes */
    }

    V_VolLock(volptr).IPAddress = 0;
    ReleaseWriteLock(&(V_VolLock(volptr).VolumeLock));
    VPutVolume(volptr);

    LogMsg(2, SrvDebugLevel, stdout, "S_VolUnlock finished successfully");
    VDisconnectFS();
	
    return(0);
}

