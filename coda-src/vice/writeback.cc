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

/************************************************************************/
/*									*/
/*  writeback.c	- Writeback caching specific routines			*/
/*									*/
/*  Function	-							*/
/*									*/
/*									*/
/*									*/
/************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <struct.h>
#include <inodeops.h>
#include <unistd.h>
#include <stdlib.h>

#include <lwp/lwp.h>
#include <rpc2/rpc2.h>
#include <rpc2/se.h>
#include <util.h>
#include <rvmlib.h>

#include <prs.h>
#include <al.h>
#include <vice.h>
#ifdef __cplusplus
}
#endif __cplusplus

#include <vmindex.h>
#include <srv.h>
#include <recov_vollog.h>
#include "coppend.h"
#include <lockqueue.h>
#include <vldb.h>
#include <vrdb.h>
#include <repio.h>
#include <vlist.h>
#include <callback.h>
#include "codaproc.h"
#include <codadir.h>
#include <nettohost.h>
#include <operations.h>
#include <res.h>
#include <resutil.h>
#include <rescomm.h>
#include <ops.h>
#include <timing.h>
#include <vice.private.h>
#include <writeback.h>

int NoWritebackConn;      /* don't open a wb conn to venus on connect */

/* Move this somewhere useful */
extern void WBClearReaders(HostTable * VenusId, VolumeId vid, VolumeId VSGVolnum);
long ReturnWBPermit(RPC2_Handle cid, VolumeId Vid, int revoke);

/* find the first WBConnEntry that isn't inuse */
WBConnEntry * findIdleWBConn(HostTable * VenusId) {
    WBConnEntry * conn;

    conn = (WBConnEntry*)VenusId->WBconns.next;
    if (!conn || list_empty(&VenusId->WBconns))
	return 0;

    while (conn != (WBConnEntry*)&VenusId->WBconns)
	if (conn->inuse)
	    conn = (WBConnEntry*)conn->others.next;
	else
	    return conn;

    return 0;
}

/* find a usable handle or create one */
WBConnEntry * getWBConn(HostTable * VenusId) {
    WBConnEntry * conn;

    conn = findIdleWBConn(VenusId);
    if (!conn)
	CLIENT_MakeWriteBackConn(VenusId);
    conn = findIdleWBConn(VenusId);
    return conn;
}    

/* find the holder entry that matches the venus given */
WBHolderEntry * matchWBHolder(Volume * volptr, HostTable * VenusId) {
    WBHolderEntry * holder;

    if (list_empty(&volptr->WriteBackHolders))
	return NULL;
    
    holder = (WBHolderEntry*)volptr->WriteBackHolders.next;
    while(holder != (WBHolderEntry*)&volptr->WriteBackHolders) {
	if (holder->VenusId == VenusId)
	    return holder;
	holder = (WBHolderEntry*)holder->others.next;
    }
    return NULL;
}

/* add a writeback entry to a volume */
int addWBHolder(Volume * volptr, HostTable * VenusId) {
    WBHolderEntry * holder;
	
    holder = (WBHolderEntry*)malloc(sizeof(WBHolderEntry));
    if (!holder)
	return ENOMEM;
    
    holder->VenusId = VenusId;
    list_add(&holder->others,&volptr->WriteBackHolders);
    return 0;
}

/* Attempt to get a WriteBack permit on a volume */
long FS_ViceGetWBPermit(RPC2_Handle cid, VolumeId Vid, 
			ViceFid *fid, RPC2_Integer *Permit)
{
    long errorCode = 0;
    Volume *volptr;
    VolumeId rwVid;
    ClientEntry *client = 0;
    int ix, count;

    SLog(1, "ViceGetWBPermit for volume 0x%x", Vid);
    *Permit = 0;

    errorCode = RPC2_GetPrivatePointer(cid, (char **)&client);
    if(!client || errorCode) {
	SLog(0, "No client structure built by ViceConnectFS");
	return(errorCode);
    }

    /* now get the version stamp for Vid */
    rwVid = Vid;
    if (!XlateVid(&rwVid, &count, &ix)) {
	SLog(1, "GetVolVV: Couldn't translate VSG %x", Vid);
	errorCode = EINVAL;
	goto Exit;
    }

    SLog(9, "GetWBPermit: Going to get volume %x pointer", rwVid);
    volptr = VGetVolume((Error *) &errorCode, rwVid);
    SLog(1, "GetWBPermit: Got volume %x: error = %d", rwVid, errorCode);
    if (errorCode) {
	SLog(0,  "ViceGetWBPermit, VGetVolume error %s",
	     ViceErrorMsg((int)errorCode));
	goto Exit;
    }

    /* Check to see if we can enable WB caching */
    if (((volptr->header)->diskstuff.WriteBackEnable) && !NoWritebackConn) {
	CODA_ASSERT(client->VenusId);
	if (!list_empty(&volptr->WriteBackHolders)) {	  /* at least 1 client has it  */
	    if (matchWBHolder(volptr,client->VenusId)) {  /* this client has it already*/
	    SLog(1, "GetWBPermit: Client %x (us) already has it",
		 ((WBHolderEntry*)volptr->WriteBackHolders.next)->VenusId);
	    *Permit =  WB_PERMIT_GRANTED;
	    }
	    else {                                        /* some other client has it  */
		SLog(1, "GetWBPermit: Another client %x already has it",
		     ((WBHolderEntry*)volptr->WriteBackHolders.next)->VenusId);
		*Permit =  WB_OTHERCLIENT;
	    }
	}
	else {                                            /* no others have permits    */
	    SLog(1, "GetWBPermit: Clearing off other readers ...");
	    WBClearReaders(client->VenusId,Vid,Vid);
	    errorCode = addWBHolder(volptr,client->VenusId);/* store client who has it*/
	    if (errorCode) {
		SLog(1, "GetWBPermit: Granting permit failed.");
		*Permit = WB_LOOKUPFAILED;
	    }
	    else {
		*Permit =  WB_PERMIT_GRANTED;
		SLog(1, "GetWBPermit: Granting WB Permit to WBid %x",
		     client->VenusId);
	    }
	}
    }
    else {
	*Permit = WB_DISABLED;
	SLog(1, "GetWBPermit: WriteBack disabled on Volume %x",Vid);
    }

    VPutVolume(volptr);

 Exit:
    SLog(2, "ViceGetWBPermit returns %s\n", ViceErrorMsg((int)errorCode));

    return(errorCode);
}


/* Revoke Permit, blocking repeated revocations */
long RevokeWBPermitSafely(HostTable * VenusId, VolumeId Vid, Volume * volptr) {
    int rc;
    WBConnEntry * WBconn;
    
    
    SLog(2, "RevokeWBPermitSafely for volume %x",Vid);
    
    if (volptr->WriteBackRevokeInProgress) {
	SLog(2, "RevokeWBPermitSafely: Revocation in progress already, waiting...");
	LWP_WaitProcess(&volptr->WriteBackRevokeDone);
	SLog(2, "RevokeWBPermitSafely: Got RevokeDone signal, continuing...");
	return 0;
    }
    else {
	SLog(2, "RevokeWBPermitSafely: No Revocation in progress, starting one ...");
	WBconn = getWBConn(VenusId);
	if (!WBconn) {                      /* couldn't allocate a connection */
	    SLog(2, "RevokeWBPermitSafely: Couldn't alocate connection, retry.");
	    return(EAGAIN);
	}

	CODA_ASSERT(!WBconn->inuse);
	WBconn->inuse = 1;
	volptr->WriteBackRevokeInProgress = 1;
	SLog(2, "RevokeWBPermitSafely: Allocated WBcid %x",WBconn->id);
	rc = RevokeWBPermit(WBconn->id,Vid);
	volptr->WriteBackRevokeInProgress = 0;
	WBconn->inuse = 0;
	SLog(2, "RevokeWBPermitSafely: Revocation completed with %d, signalling  ...",rc);
	LWP_NoYieldSignal(&volptr->WriteBackRevokeDone);
	return rc;
    }
}


/* Attempt to return an unused permit */
long FS_ViceRejectWBPermit(RPC2_Handle cid, VolumeId Vid) {
    return ReturnWBPermit(cid,Vid,0);
}

/* Attempt to give up a WriteBack permit on a volume */
long FS_ViceTossWBPermit(RPC2_Handle cid, VolumeId Vid, ViceFid *Fid) {
    return ReturnWBPermit(cid,Vid,1);
}

/* Voluntarily give up a permit */   
long ReturnWBPermit(RPC2_Handle cid, VolumeId Vid, int revoke)
{
    long errorCode = 0;
    Volume *volptr;
    VolumeId rwVid;
    ClientEntry *client = 0;
    WBHolderEntry * WBHolder;
    int ix, count;

    SLog(1, "ReturnWBPermit for volume 0x%x", Vid);
 

    errorCode = RPC2_GetPrivatePointer(cid, (char **)&client);
    if(!client || errorCode) {
	SLog(0, "No client structure built by ViceConnectFS");
	return(errorCode);
    }

    /* now get the version stamp for Vid */
    rwVid = Vid;
    if (!XlateVid(&rwVid, &count, &ix)) {
	SLog(1, "GetVolVV: Couldn't translate VSG %x", Vid);
	errorCode = EINVAL;
	goto Exit;
    }
    
    SLog(9, "ReturnWBPermit: Going to get volume %x pointer", rwVid);
    volptr = VGetVolume((Error *) &errorCode, rwVid);
    SLog(1, "ReturnWBPermit: Got volume %x: error = %d", rwVid, errorCode);

    if (errorCode) {
	SLog(0,  "ReturnWBPermit, VgetVolume error %s", ViceErrorMsg((int)errorCode));
	goto Exit;
    }

    /* Check to see that we have the permit */
    WBHolder = matchWBHolder(volptr,client->VenusId);
    if (WBHolder) {
      SLog(1, "ReturnWBPermit: Destroying WB Permit for VenusId %x",WBHolder->VenusId);
      if (revoke) {
	  SLog(1, "ReturnWBPermit: Destroying WB Permit for VenusId %x",WBHolder->VenusId);
	  errorCode = (RevokeWBPermitSafely(WBHolder->VenusId,Vid,volptr) != 0);
      }
      list_del(&WBHolder->others);
    }
    else {
	SLog(1, "ReturnWBPermit: VenusId %x tried to destroy a permit not its.",client->VenusId);
	
    }

    VPutVolume(volptr);

 Exit:
    SLog(2, "ReturnWBPermit returns %s\n", ViceErrorMsg((int)errorCode));

    return(errorCode);
}

/* Check that the latest version of a WB cached object has been returned */
int CheckWriteBack(ViceFid * Fid,ClientEntry * client) {
    VolumeId Vid,VenusVid,myVid;
    WBHolderEntry * WBHolder;
    RPC2_Integer rc = 0;
    VolumeId rwVid;
    long errorCode = 0;
    Volume *volptr;
    int count,ix;

    Vid = Fid->Volume;
    VenusVid = Vid;
    rwVid = Vid;
    myVid = Vid;

    if (XlateVid(&rwVid, &count, &ix)) {
	VenusVid = Vid;
	myVid = rwVid;
	SLog(1, "CheckWriteBack: Translated VSG %x to vol %x",myVid,VenusVid);
    }
    else {
	rwVid = Vid;
	if (ReverseXlateVid(&rwVid)) {
	    VenusVid = rwVid;
	    myVid = Vid;
	    SLog(1, "CheckWriteBack: Translated vol %x to VSG %x",myVid,VenusVid);
	}
	else
	    SLog(1, "CheckWriteBack: Couldn't translate %x",Vid);
    }
	
    
    volptr = VGetVolume((Error *) &errorCode, myVid);
    if (errorCode) {
	SLog(1, "CheckWriteBack: Error getting volume %x",myVid);
	return -1;
    }
    
    if (list_empty(&volptr->WriteBackHolders)) {  /* noone has a permit */
	SLog(1, "CheckWriteBack: Nobody has permit on %x",myVid);
	goto FreeLocks;
    }

    /* Someone(s) has a writeback permit out on the volume */
    
    /* XXX for file level sharing  */
    /* Find callbacks open for FID */
    /* if I have a callback on it, let it go
       if I have a permit, and noone else has a callback, let it go.
       if I have a permit and someone has a callback
          if they have a permit and no contention
	    set contention, fetch file
	  if they have a permit and contention
	    revoke all permits
	  break the callback
       if I don't have a permit and someone has a callback
          if they have a permit, fetch file, break their callback, set contention
	  if they don't have a permit, let it go. -- ordinary case */

    if (matchWBHolder(volptr,client->VenusId)) {  /* I already have it*/
	SLog(1, "CheckWriteBack: %x is writeback holder for volume %x",
	     client->VenusId, myVid);
	goto FreeLocks;
    }
    /* someone else has permit    */
    WBHolder = (WBHolderEntry*)volptr->WriteBackHolders.next;
    SLog(1, "CheckWriteBack: Revoking %x's permit for volume %x",
	 WBHolder->VenusId,VenusVid);
    rc = RevokeWBPermitSafely(WBHolder->VenusId,VenusVid,volptr);
    list_del(&WBHolder->others);

    if (rc == RPC2_SUCCESS) {
	SLog(1, "CheckWriteBack: Modifications reintegrated to %x",myVid);
    } else {
	SLog(1, "CheckWriteBack: Permit revocation failed on %x with error
	     %d",myVid,rc);
    }

FreeLocks:
    VPutVolume(volptr);

    return(rc);
}
  
