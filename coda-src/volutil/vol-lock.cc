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

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/coda-src/volutil/vol-lock.cc,v 1.3 1997/01/07 18:43:41 rvb Exp";
#endif /*_BLURB_*/






/*
 * This module holds routines which lock and unlock volumes for volutil.
 * Currently these routines are only used as part of the backup mechanism.
 */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>

#ifdef __MACH__
#include <libc.h>
#include <sysent.h>
#endif /* __MACH__ */

#if defined(__linux__) || defined(__NetBSD__)
#include <unistd.h>
#include <stdlib.h>
#endif /* __NetBSD__ || LINUX */

#include <lwp.h>
#include <lock.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>
#include <vice.h>
#include <nfs.h>
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
    
    assert(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);
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
    bcopy(&(V_versionvector(volptr)), VolVV, sizeof(ViceVersionVector));
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

    assert(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);
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

