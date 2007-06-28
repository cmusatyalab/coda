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

#*/

/*
                         IBM COPYRIGHT NOTICE

                          Copyright (C) 1986
             International Business Machines Corporation
                         All Rights Reserved

This  file  contains  some  code identical to or derived from the 1986
version of the Andrew File System ("AFS"), which is owned by  the  IBM
Corporation.   This  code is provided "AS IS" and IBM does not warrant
that it is free of infringement of  any  intellectual  rights  of  any
third  party.    IBM  disclaims  liability of any kind for any damages
whatsoever resulting directly or indirectly from use of this  software
or  of  any  derivative work.  Carnegie Mellon University has obtained
permission to  modify,  distribute and sublicense this code,  which is
based on Version 2  of  AFS  and  does  not  contain  the features and
enhancements that are part of  Version 3 of  AFS.  Version 3 of AFS is
commercially   available   and  supported  by   Transarc  Corporation,
Pittsburgh, PA.

*/



#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#ifdef __linux__
#include <linux/if_ether.h>
#endif

#include "coda_string.h"
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#include <lwp/lwp.h>
#include <lwp/lock.h>
#include <rpc2/rpc2.h>
#include <rpc2/errors.h>
#include <util.h>
#include <rvmlib.h>
#include <partition.h>
#include <auth2.h>
#include <prs.h>
#include <al.h>
#include <callback.h>
#include <vice.h>
#include "coda_flock.h"

#ifdef __cplusplus
}
#endif

#include <voltypes.h>
#include <vrdb.h>
#include <vldb.h>
#include <volume.h>
#include <srv.h>
#include <volume.h>
#include <vlist.h>
#include <vice.private.h>
#include <operations.h>
#include <ops.h>
#include <lockqueue.h>
#include <vice_file.h>
#include "coppend.h"

/* *****  Exported variables  ***** */

unsigned int etherWrites = 0;
unsigned int etherRetries = 0;
unsigned int etherInterupts = 0;
unsigned int etherGoodReads = 0;
unsigned int etherBytesRead = 0;
unsigned int etherBytesWritten = 0;

/* *****  External routines ***** */
extern int ValidateParms(RPC2_Handle, ClientEntry **, int *ReplicatedOp,
			 VolumeId *, RPC2_CountedBS *, int *Nservers);
/* *****  Private routines  ***** */

static void SetVolumeStatus(VolumeStatus *, RPC2_BoundedBS *,
			      RPC2_BoundedBS *, RPC2_BoundedBS *, Volume *);
static void SetViceStats(ViceStatistics *);
static void SetRPCStats(ViceStatistics *);
static void SetVolumeStats(ViceStatistics *);
static void SetSystemStats(ViceStatistics *);
static void PrintVolumeStatus(VolumeStatus *);


/*
  ViceDisconnectFS: Client request for termination
*/
long FS_ViceDisconnectFS(RPC2_Handle RPCid)
{
    ClientEntry *client = 0;
    long   errorCode;
    char *rock;

    SLog(1, "ViceDisconnectFS");
    errorCode = RPC2_GetPrivatePointer(RPCid, &rock);
    client = (ClientEntry *)rock;

    if (!errorCode && client) {
	struct Lock *lock = &client->VenusId->lock;
	ObtainWriteLock(lock);
	CLIENT_Delete(client);
	ReleaseWriteLock(lock);
    }
    else {
	SLog(0, "GetPrivate failed in Disconnect rc = %d, client = %d",
		errorCode, client);
    }

    SLog(2, "ViceDisconnectFS returns success");
    return(0);
}

/*
  TokenExpired: When the server detects token expiry between GetRequest
    and srv_ExecuteRequest, it converts the Header.Opcode into
    TokenExpired. This routine then returns an appropriate error to the
    client. (Ofcourse masochistic clients may call this routine
    themselves, and enjoy being thrown out :)
*/
long FS_TokenExpired(RPC2_Handle RPCid)
{
    ClientEntry *client = 0;
    long   errorCode;
    char *rock;

    SLog(100, "TokenExpired");
    errorCode = RPC2_GetPrivatePointer(RPCid, &rock);
    client = (ClientEntry *)rock;

    if (!errorCode && client)
	CLIENT_CleanUpHost(client->VenusId);

    return(RPC2_NAKED);
}

long FS_ViceGetOldStatistics(RPC2_Handle RPCid, ViceStatistics *Statistics)
{
    return(FS_ViceGetStatistics(RPCid, Statistics));
}

/* ViceGetStatistics: Used by filestats to get general file server statistics */
long FS_ViceGetStatistics(RPC2_Handle RPCid, ViceStatistics *Statistics)
{
    int     errorCode;		/* return code for caller */

    SLog(3, "ViceGetStatistics Received");

    errorCode = 0;

    memset(Statistics, 0, sizeof(ViceStatistics));

    SetViceStats(Statistics);
    SetRPCStats(Statistics);
    SetVolumeStats(Statistics);
    SetSystemStats(Statistics);

    SLog(3, "ViceGetStatistics returns %s", ViceErrorMsg(errorCode));
    return(errorCode);
}

static const char *GetROOTVOLUME(void)
{
    static char rootvolume[V_MAXVOLNAMELEN];
    int fd, len;

    fd = open(vice_sharedfile("db/ROOTVOLUME"), O_RDONLY);
    if (fd < 0)
	return NULL;

    len = read(fd, rootvolume, V_MAXVOLNAMELEN-1);
    close(fd);
    if (len <= 0)
	return NULL;

    if (rootvolume[len-1] == '\n') len--;
    rootvolume[len] = '\0';

    return rootvolume;
}

/*
  ViceGetVolumeInfo: Used to get information about a particular volume
*/
long FS_ViceGetVolumeInfo(RPC2_Handle RPCid, RPC2_String VolName, VolumeInfo *Info)
{
    SLog(1,"ViceGetVolumeInfo volume = %s", VolName);

    int	errorCode = 0;	    /* error code */
    vrent *vre;
    
    if (VolName[0] == '\0' || (VolName[0] == '/' && VolName[1] == '\0')) {
	const char *rootvol = GetROOTVOLUME();
	if (rootvol)
	    VolName = (RPC2_String)rootvol;
    }

    vre = VRDB.find((char *)VolName);
    if (!vre) {
	VolumeId Vid = strtol((char *)VolName, NULL, 0);
	if (Vid) vre = VRDB.find(Vid);
    }
    if (vre)
	errorCode = vre->GetVolumeInfo(Info);
    else {
	VGetVolumeInfo((Error *)&errorCode, (char *)VolName, Info);

	if (errorCode == 0) {
	    if (Info->Type == ROVOL) {
		SLog(0, "GetVolumeInfo called for ROVOL");
		Info->VSGAddr = 0;
	    } else if (Info->Type == RWVOL) {
		/* Stuff the GroupId in the Info->Type[REPVOL] field. */
		VolumeId Vid = Info->Vid;
		if (ReverseXlateVid(&Vid))
		    (&(Info->Type0))[replicatedVolume] = Vid;
	    }
	}
    }

    SLog(2, "ViceGetVolumeInfo returns %s, Volume %u, type %x, servers %x %x %x %x...",
	    ViceErrorMsg(errorCode), Info->Vid, Info->Type,
	    Info->Server0, Info->Server1, Info->Server2, Info->Server3);
    return(errorCode);
}

/*
ViceGetVolumeLocation: Used to get the location (host:port) of a volume replica
*/
long FS_ViceGetVolumeLocation(RPC2_Handle RPCid, VolumeId Vid,
			      RPC2_BoundedBS *HostPort)
{
    const char *addr;
    size_t len;
    int	err = 0;

    SLog(1,"ViceGetVolumeLocation volume = %08x", Vid);
    HostPort->SeqLen = 0;

    addr = VGetVolumeLocation(Vid);
    if (!addr) {
	/* do we need more detailed errors? VNOVOL/ISREPLICATED/EWOULDBENICE? */
	err = VNOSERVICE;
	goto errout;
    }

    len = strlen(addr);
    if (len >= HostPort->MaxSeqLen) {
	err = ENAMETOOLONG;
	goto errout;
    }

    strcpy((char *)HostPort->SeqBody, addr);
    HostPort->SeqLen = len;

errout:
    SLog(2, "ViceGetVolumeLocation: Volume %08x, location %s, err %s",
	 Vid, addr ? addr : "", ViceErrorMsg(err));
    return(err);
}


/*
  ViceGetVolumeStatus: Get the status of a particular volume
*/
long FS_ViceGetVolumeStatus(RPC2_Handle RPCid, VolumeId vid, VolumeStatus *status, RPC2_BoundedBS *name, RPC2_BoundedBS *offlineMsg, RPC2_BoundedBS *motd, RPC2_Unsigned PrimaryHost)
{
    Vnode * vptr;		/* vnode of the new file */
    ViceFid vfid;		/* fid of new file */
    long   errorCode;		/* error code */
    Error   fileCode;		/* used when writing Vnodes */
    Volume * volptr;		/* pointer to the volume header */
    AL_AccessList * aCL;	/* access list */
    ClientEntry * client;	/* pointer to client entry */
    int		aCLSize;
    int		rights;
    int		anyrights;
    char *rock;

    errorCode = fileCode = 0;
    vptr = 0;
    volptr = 0;
    VolumeId VSGVolnum = vid;

    SLog(1,"GetVolumeStatus for volume %u",vid);

    if (vid == 0) {
	errorCode = EINVAL;
	goto Final;
    }

    if (XlateVid(&vid)) {
	SLog(1, "XlateVid: %u --> %u", VSGVolnum, vid);
	if (PrimaryHost == 0) {
	    SLog(0, "Translated VSG but PrimaryHost == 0");
	    errorCode =	EINVAL;	    /* ??? -JJK */
	    goto Final;
	}
    }
    else {
	if (PrimaryHost != 0) {
	    SLog(0, "Failed to translate VSG but PrimaryHost != 0");
	    errorCode =	EINVAL;	    /* ??? -JJK */
	    goto Final;
	}
    }

    if( name->MaxSeqLen < V_MAXVOLNAMELEN || offlineMsg->MaxSeqLen < VMSGSIZE ||
	    motd->MaxSeqLen < VMSGSIZE) {
	errorCode = EINVAL;
	goto Final;
    }

    vfid.Volume = vid;
    vfid.Vnode = ROOTVNODE;
    vfid.Unique = 1;

    // get the root vnode even if it is inconsistent
    if ((errorCode = GetFsObj(&vfid, &volptr, &vptr, 
			     READ_LOCK, VOL_NO_LOCK, 1, 0, 0))) {
	goto Final;
    }

    SetAccessList(vptr, aCL, aCLSize);

    if((errorCode = RPC2_GetPrivatePointer(RPCid, &rock)) != RPC2_SUCCESS)
	goto Final;
    client = (ClientEntry *)rock;

    if(!client) {
	errorCode = EFAULT;
	SLog(0, "Client pointer zero in GetVolumeStatus");
	goto Final;
    }

    CODA_ASSERT(GetRights(client->CPS, aCL, aCLSize, (Rights *)&rights, (Rights *)&anyrights) == 0);

    if(!(rights & PRSFS_READ) && !SystemUser(client)) {
	errorCode = EACCES;
	goto Final;
    }

    SetVolumeStatus(status, name, offlineMsg, motd, volptr);

 Final:
    if (vptr) {
	VPutVnode(&fileCode, vptr);
	CODA_ASSERT(fileCode == 0);
    }
    PutVolObj(&volptr, VOL_NO_LOCK, 0);

    SLog(2,"GetVolumeStatus returns %s", ViceErrorMsg((int) errorCode));
    if(!errorCode) {
	SLog(5,"Name = %s, Motd = %s, offMsg = %s",
		name->SeqBody, motd->SeqBody, offlineMsg->SeqBody);
	PrintVolumeStatus(status);
    }
    return(errorCode);
}


void PerformSetQuota(ClientEntry *client, VolumeId VSGVolnum, Volume *volptr, Vnode *vptr, ViceFid *fid, int NewQuota, int ReplicatedOp, ViceStoreId *StoreId)
{
    CodaBreakCallBack((client ? client->VenusId : 0), fid, VSGVolnum);

    V_maxquota(volptr) = NewQuota;

    if (ReplicatedOp)
	NewCOP1Update(volptr, vptr, StoreId);

    /* Await COP2 message. */
    if (ReplicatedOp) {
	ViceFid fids[MAXFIDS];
	memset((void *)fids, 0, (int)(MAXFIDS * sizeof(ViceFid)));
	fids[0] = *fid;
	CopPendingMan->add(new cpent(StoreId, fids));
    }
}



/*
  BEGIN_HTML
  <a name="ViceSetVolumeStatus"><strong>Set the status(e.g. quota) for a volume </strong></a> 
  END_HTML
*/
long FS_ViceSetVolumeStatus(RPC2_Handle RPCid, VolumeId vid, VolumeStatus *status, RPC2_BoundedBS *name, RPC2_BoundedBS *offlineMsg, RPC2_BoundedBS *motd, RPC2_Unsigned PrimaryHost, ViceStoreId *StoreId, RPC2_CountedBS *PiggyCOP2)
{
    ViceFid vfid;		/* fid of new file */
    long   errorCode;		/* error code */
    Error   fileCode;		/* used when writing Vnodes */
    Volume * volptr;		/* pointer to the volume header */
    AL_AccessList * aCL;	/* access list */
    ClientEntry * client;	/* pointer to client entry */
    int		aCLSize;
    int		rights;
    int		anyrights;
    int oldquota = -1;               /* Old quota if it was changed */
    int ReplicatedOp;
    vle *v = 0;
    dlist *vlist = new dlist((CFN)VLECmp);

    errorCode = fileCode = 0;
    volptr = 0;
    VolumeId VSGVolnum = vid;

    SLog(1,"ViceSetVolumeStatus for volume %u", vid);
    SLog(5,"Min = %d Max = %d, Name = %d.%d %s, Offline Msg = %d.%d.%s, motd = %d.%d.%s",
	    status->MinQuota, status->MaxQuota,
	    name->MaxSeqLen, name->SeqLen, name->SeqBody,
	    offlineMsg->MaxSeqLen, offlineMsg->SeqLen, offlineMsg->SeqBody,
	    motd->MaxSeqLen, motd->SeqLen, motd->SeqBody);

    /* Validate parameters. */

    if (vid == 0) {
	errorCode = EINVAL;
	goto Final;
    }

    {
	if ((errorCode = ValidateParms(RPCid, &client, &ReplicatedOp, &vid, 
				      PiggyCOP2, NULL)))
	    goto Final;
    }

    if( name->MaxSeqLen < V_MAXVOLNAMELEN || name->SeqLen > V_MAXVOLNAMELEN) {
	errorCode = EINVAL;
	goto Final;
    }

    if( offlineMsg->MaxSeqLen < VMSGSIZE || motd->MaxSeqLen < VMSGSIZE) {
	errorCode = EINVAL;
	goto Final;
    }

    if( offlineMsg->SeqLen > VMSGSIZE || motd->SeqLen > VMSGSIZE) {
	errorCode = EINVAL;
	goto Final;
    }
    

    vfid.Volume = vid;
    vfid.Vnode = ROOTVNODE;
    vfid.Unique = 1;

    if ((errorCode = GetVolObj(vid, &volptr, VOL_EXCL_LOCK, 0, 1 /* check this */))) {
	SLog(0, "Error locking volume in ViceSetVolumeStatus: %s", ViceErrorMsg((int) errorCode));
	goto Final ;
    }


    v = AddVLE(*vlist, &vfid);
    if ((errorCode = GetFsObj(&vfid, &volptr, &v->vptr, READ_LOCK, 
			     VOL_NO_LOCK, 0, 0, 0))) {
	goto Final;
    }

    SetAccessList(v->vptr, aCL, aCLSize);

    if(!client) {
	errorCode = EFAULT;
	SLog(0, "Client pointer is zero in ViceSetVolumeStatus");
	goto Final;
    }

    CODA_ASSERT(GetRights(client->CPS, aCL, aCLSize, (Rights *)&rights, (Rights *)&anyrights) == 0);

    if (!SystemUser(client)) {
	errorCode = EACCES;
	goto Final;
    }

    if (status->MinQuota > -1)
	V_minquota(volptr) = (int) status->MinQuota;

    if(status->MaxQuota > -1) {
        oldquota = V_maxquota(volptr);
	PerformSetQuota(client, VSGVolnum, volptr, v->vptr, &vfid, 
			(int)status->MaxQuota, ReplicatedOp, StoreId);
    }

    if(offlineMsg->SeqLen > 1)
	strcpy(V_offlineMessage(volptr), (char *)offlineMsg->SeqBody);

    if(name->SeqLen > 1)
	strcpy(V_name(volptr), (char *)name->SeqBody);

    if(motd->SeqLen > 1)
	strcpy(V_motd(volptr), (char *)motd->SeqBody);

    // Only spool a log entry if the quota was set.
    if (oldquota > -1) 
	if (ReplicatedOp && !errorCode) {
	    SLog(1, "ViceSetVolumeStatus: About to spool log record, oldquota = %d, new quota = %d\n", oldquota, status->MaxQuota);
	    if ((errorCode = SpoolVMLogRecord(vlist, v, volptr, StoreId,
					     ViceSetVolumeStatus_OP, oldquota,
					     status->MaxQuota)))
		SLog(0, "ViceSetVolumeStatus: Error %d during SpoolVMLogRecord\n", errorCode);
	}

 Final:

    if (!errorCode) 
        SetVolumeStatus(status, name, offlineMsg, motd, volptr);

    PutObjects((int) errorCode, volptr, VOL_EXCL_LOCK, vlist, 0, 1, TRUE);

    SLog(2,"ViceSetVolumeStatus returns %s", ViceErrorMsg((int) errorCode));
    if(!errorCode) {
	SLog(5,"Name = %s, Motd = %s, offMsg = %s",
		name->SeqBody, motd->SeqBody, offlineMsg->SeqBody);
	PrintVolumeStatus(status);
    }
    return(errorCode);
}


#define DEFAULTVOLUME "/"
/*
  ViceGetRootVolume: Return the name of the root volume
  (corresponding to /coda/<realm>/)
*/
long FS_ViceGetRootVolume(RPC2_Handle RPCid, RPC2_BoundedBS *volume)
{
    SLog(1, "ViceGetRootVolume");

    if ((unsigned int)volume->MaxSeqLen <= strlen(DEFAULTVOLUME))
    {
	volume->SeqLen = 0;
	SLog(1, "ViceGetRootVolume: not enough space in query");
	return VNOVOL;
    }

    strcpy((char *)volume->SeqBody, DEFAULTVOLUME);
    volume->SeqLen = strlen(DEFAULTVOLUME) + 1;

    return 0;
}


/* ViceSetRootVolume: Set the .../db/ROOTVOLUME file to contain the
  name of the new root volume */
long FS_ViceSetRootVolume(RPC2_Handle RPCid, RPC2_String volume)
{
    long errorCode = 0;
    ClientEntry * client;	/* pointer to client entry */
    int fd, rc, len;
    char *rock;

    SLog(1,"ViceSetRootVolume to %s", (char *)volume);

    errorCode = RPC2_GetPrivatePointer(RPCid, &rock);
    if (errorCode != RPC2_SUCCESS)
	goto exit;
    client = (ClientEntry *)rock;

    if (!client) {
	errorCode = EFAULT;
	SLog(0, "Client pointer is zero in SetRootVolume");
	goto exit;
    }

    if (!SystemUser(client)) {
	errorCode = EACCES;
	goto exit;
    }

    fd = open(vice_sharedfile("db/ROOTVOLUME.new"), O_WRONLY|O_CREAT|O_EXCL, 0644);
    if (fd < 0) {
	errorCode = errno;
	SLog(0, "SetRootVolume failed to open ROOTVOLUME.new");
	goto exit;
    }

    len = strlen((char *)volume);
    rc = write(fd, (char *)volume, len);
    close(fd);
    if (rc != len) {
	errorCode = ENOSPC;
	SLog(0, "SetRootVolume failed to write ROOTVOLUME.new");
	goto exit;
    }

    rc = rename(vice_sharedfile("db/ROOTVOLUME.new"),
		vice_sharedfile("db/ROOTVOLUME"));
    if (rc == -1) {
	errorCode = errno;
	SLog(0, "SetRootVolume failed to rename ROOTVOLUME.new");
    }

exit:
    SLog (2, "ViceSetRootVolume returns %s", ViceErrorMsg(errorCode));
    return errorCode;
}


/*
  ViceGetTime: Returns time of day (a time ping)
*/

long FS_ViceGetTime(RPC2_Handle RPCid, RPC2_Unsigned *seconds, 
		 RPC2_Integer *useconds)
{
	struct timeval tpl;
	struct timezone tspl;
	ClientEntry *client = NULL;
	long errorCode;
	char *rock;

	TM_GetTimeOfDay(&tpl,&tspl);
	*seconds = tpl.tv_sec;
	*useconds = tpl.tv_usec;

	errorCode = RPC2_GetPrivatePointer(RPCid, &rock);
	client = (ClientEntry *)rock;

	if (errorCode || !client) { 
		SLog(0, "No client structure built by ViceNewConnection");
		return ENOTCONN;
	}

	SLog(1, "ViceGetTime for user %s at %s:%d on conn %d.",
	     client->UserName, inet_ntoa(client->VenusId->host),
	     ntohs(client->VenusId->port), RPCid);
	
	if (!errorCode && client) {
	    /* we need a lock, because we cannot do concurrent RPC2 calls on
	     * the same connection */
	    ObtainWriteLock(&client->VenusId->lock);

	    /* set up a callback channel if there isn't one for this host */
	    if (client->VenusId->id != 0) {
		errorCode = CallBack(client->VenusId->id, (ViceFid *)&NullFid);
		if ( errorCode != RPC2_SUCCESS ) {
		    SLog(0, "GetTime: Destroying callback conn for %s:%d",
			 inet_ntoa(client->VenusId->host),
			 ntohs(client->VenusId->port));
		    /* tear down nak'd connection */
		    RPC2_Unbind(client->VenusId->id);
		    client->VenusId->id = 0;
		}
	    }

	    ReleaseWriteLock(&client->VenusId->lock);
	    /* we don't need the lock here anymore, because MakeCallBackConn
	     * does it's own locking */

	    if (!client->VenusId->id && !client->DoUnbind) {
		SLog(0, "GetTime: Building callback conn to %s:%d.",
		     inet_ntoa(client->VenusId->host),
		     ntohs(client->VenusId->port));
		errorCode = CLIENT_MakeCallBackConn(client);
	    }
	}

	SLog(2, "GetTime returns %d, %d, errorCode %d", 
	     *seconds, *useconds, errorCode);
	
	return(errorCode);
}


/*
ViceNewConnection: Called after a new bind request is received.
*/

long FS_ViceNewConnection(RPC2_Handle RPCid, RPC2_Integer set, 
			  RPC2_Integer sl,  RPC2_Integer et, 
			  RPC2_Integer at, RPC2_CountedBS *cid)
{
	char username[PRS_MAXNAMELEN+1] = NEWCONNECT;
	SecretToken *st = NULL;
	ClientEntry *client = NULL;
	long errorCode;

	if (sl == RPC2_OPENKIMONO) {
	    strncpy(username, (char *)cid->SeqBody, PRS_MAXNAMELEN);
	    username[PRS_MAXNAMELEN] = '\0';
	} else
	    st = (SecretToken *)cid->SeqBody;

	errorCode = CLIENT_Build(RPCid, username, sl, st, &client);
	if (errorCode) {
	    SLog(0,"New connection setup FAILED, RPCid %d, security level %d, "
		   "remote cid %d returns %s",
		 RPCid, sl, cid, ViceErrorMsg((int) errorCode));
	    return (errorCode);
	}
	
	client->SEType = (int) set;
	SLog(1,"New connection received RPCid %d, security lvl %d, rem id %d",
	     RPCid, sl, cid);

	return(0);
}


static void SetVolumeStatus(VolumeStatus *status, RPC2_BoundedBS *name, RPC2_BoundedBS *offMsg,
		RPC2_BoundedBS *motd, Volume *volptr)
{
    status->Vid = V_id(volptr);
    status->ParentId = V_parentId(volptr);
    status->Online = V_inUse(volptr);
    status->InService = V_inService(volptr);
    status->Blessed = V_blessed(volptr);
    status->NeedsSalvage = V_needsSalvaged(volptr);
    if (VolumeWriteable(volptr))
	status->Type = ReadWrite;
    else
	status->Type = ReadOnly;
    status->MinQuota = V_minquota(volptr);
    status->MaxQuota = V_maxquota(volptr);
    status->BlocksInUse = V_diskused(volptr);
    status->PartBlocksAvail = volptr->partition->free;
    status->PartMaxBlocks = volptr->partition->totalUsable;
    strncpy((char *)name->SeqBody, V_name(volptr), (int)name->MaxSeqLen);
    name->SeqLen = strlen(V_name(volptr)) + 1;
    if(name->SeqLen > name->MaxSeqLen) name->SeqLen = name -> MaxSeqLen;
    strncpy((char *)offMsg->SeqBody, V_offlineMessage(volptr), (int)name->MaxSeqLen);
    offMsg->SeqLen = strlen(V_offlineMessage(volptr)) + 1;
    if(offMsg->SeqLen > offMsg->MaxSeqLen) offMsg->SeqLen = offMsg -> MaxSeqLen;
    strncpy((char *)motd->SeqBody, V_motd(volptr), (int)offMsg->MaxSeqLen);
    motd->SeqLen = strlen(V_motd(volptr)) + 1;
    if(motd->SeqLen > motd->MaxSeqLen) motd->SeqLen = motd -> MaxSeqLen;
}


static void SetViceStats(ViceStatistics *stats)
{
    int	seconds;

    /* FetchDataRate & StoreDataRate have wrap-around problems */

    stats->StartTime = StartTime;
    stats->CurrentConnections = CurrentConnections;
    stats->TotalViceCalls = Counters[TOTAL];

    stats->TotalFetches = Counters[GETATTRPLUSSHA]+Counters[GETATTR]+Counters[GETACL]+Counters[FETCH];
    stats->FetchDatas = Counters[FETCH];
    stats->FetchedBytes = Counters[FETCHDATA];
    seconds = Counters[FETCHTIME]/1000;
    if(seconds <= 0) seconds = 1;
    stats->FetchDataRate = Counters[FETCHDATA]/seconds;

    stats->TotalStores = Counters[SETACL];
    stats->StoreDatas = 0;
    stats->StoredBytes = Counters[STOREDATA];
    seconds = Counters[STORETIME]/1000;
    if(seconds <= 0) seconds = 1;
    stats->StoreDataRate = Counters[STOREDATA]/seconds;

    CLIENT_GetWorkStats((int *)&(stats->WorkStations),
			(int *)&(stats->ActiveWorkStations),
			(unsigned)(time(0)-15*60));
}


static void SetRPCStats(ViceStatistics *stats)
{
    /* get send/receive statistics from rpc, multirpc, and sftp */
    stats->TotalRPCBytesSent = rpc2_Sent.Bytes + rpc2_MSent.Bytes + 
	    sftp_Sent.Bytes + sftp_MSent.Bytes;
    stats->TotalRPCBytesReceived = rpc2_Recvd.Bytes + rpc2_MRecvd.Bytes + 
	    sftp_Recvd.Bytes + sftp_MRecvd.Bytes;
    stats->TotalRPCPacketsSent = rpc2_Sent.Total + rpc2_MSent.Total + 
	    sftp_Sent.Total + sftp_MSent.Total;
    stats->TotalRPCPacketsReceived = rpc2_Recvd.Total + rpc2_MRecvd.Total + 
	    sftp_Recvd.Total + sftp_MRecvd.Total;

    /* 
     * Retries and busies appear only in rpc2_Send and rpc2_Recvd, because
     * they aren't multicasted.
     * Sftp is harder -- retries occur because of packet loss _and_ timeouts.
     * Punt that.
     */
    stats->TotalRPCPacketsLost = rpc2_Sent.Retries - rpc2_Recvd.GoodBusies;
    stats->TotalRPCBogusPackets = rpc2_Recvd.Total + rpc2_MRecvd.Total - 
	    rpc2_Recvd.GoodRequests - rpc2_Recvd.GoodReplies - 
		    rpc2_Recvd.GoodBusies - rpc2_MRecvd.GoodRequests;

    stats->EtherNetTotalErrors = 0;
    stats->EtherNetTotalWrites = 0;
    stats->EtherNetTotalInterupts = 0;
    stats->EtherNetGoodReads = 0;
    stats->EtherNetTotalBytesWritten = 0;
    stats->EtherNetTotalBytesRead = 0;
}


int GetEtherStats()
{
return(0);
}

static struct DiskPartition *get_part(struct dllist_head *hd)
{
	if ( hd == &DiskPartitionList )
		return NULL;

	return list_entry(hd, struct DiskPartition, dp_chain);
}

static void SetVolumeStats(ViceStatistics *stats)
{
	struct DiskPartition *part;
	struct dllist_head *tmp;
	struct ViceDisk *disk; 
	int i;

	tmp = DiskPartitionList.next;

	for ( i = 0 ; i < 10 ; i++ ) {
		part = get_part(tmp);
		/* beware: pointer arithmetic */
		disk = &(stats->Disk1) + i;
		memset(disk, 0, sizeof(struct ViceDisk));
		if(part) {
			disk->TotalBlocks = part->totalUsable;
			disk->BlocksAvailable = part->free;
			strncpy((char *)disk->Name, part->name, V_MAXPARTNAMELEN-1);
			disk->Name[V_MAXPARTNAMELEN-1] = '\0';
			tmp = tmp->next;
		}
	}
}

#ifdef __BSD44__

#ifdef	__NetBSD__
#define KERNEL "/netbsd"
#else	/* __FreeBSD__ */
#define KERNEL "/kernel"
#endif

#include <kvm.h>
#include <sys/dkstat.h>
#if defined(__NetBSD_Version__) && (__NetBSD_Version__ >= 104250000)
#include <sys/sched.h>
#endif
#include <limits.h>

static struct nlist RawStats[] = 
{
#define CPTIME	0
    {"_cp_time" },
#define BOOT	1
    {"_boottime" },
#define HZ	2
    {"_hz" },
    /* if we ever cared ... */
#define DISK	3
    {"_disk_count" },
#define DISKHD	4
    {"_disklist" },
    {0 },
};

static char kd_buf[_POSIX2_LINE_MAX];
static void SetSystemStats_bsd44(ViceStatistics *stats)
{
    static	kvm_t	*kd = (kvm_t *) NULL;
    static	int	kd_opened = 0;
    register	int	i;
    long	busy[CPUSTATES];
    struct	timeval	bootTime;
    int		dk_ndrive;
    int		hz;

    if (kd_opened == -1) {
	return;
    }
    
    if(kd_opened == 0) {
	kd = kvm_openfiles(KERNEL, "/dev/mem", NULL, O_RDONLY, kd_buf);
	if (kd == NULL) {
	    LogMsg(0, SrvDebugLevel, stdout, "kvm_openfiles: %s", kd_buf);
	    kd_opened = -1;
	    return;
	}
	i = kvm_nlist(kd, RawStats);
	if (i != 0) {
	    LogMsg(0, SrvDebugLevel, stdout, "Could not nlist symbols from kernel: %s", kvm_geterr(kd));
	    kd_opened = -1;
	    return;
	}
	kd_opened=1;
    }

#define KVM_READ(kd, offset, buf, size) (kvm_read(kd, (u_long)RawStats[offset].n_value, buf, size) != size)

    if (KVM_READ(kd, CPTIME, busy, sizeof busy)) {
	LogMsg(0, SrvDebugLevel, stdout, "Could not read CPTIME data from kernel: %s", kvm_geterr(kd));
	return;
    }
    stats->SystemCPU = busy[CP_SYS];
    stats->UserCPU = busy[CP_USER];
    stats->NiceCPU = busy[CP_NICE];
    stats->IdleCPU = busy[CP_IDLE];

    if (KVM_READ(kd, BOOT, &bootTime, sizeof (bootTime))) {
	LogMsg(0, SrvDebugLevel, stdout, "Could not read BOOT data from kernel: %s", kvm_geterr(kd));
	return;
    }
    stats->BootTime = bootTime.tv_sec;

    if (KVM_READ(kd, HZ, &hz, sizeof (hz))) {
	LogMsg(0, SrvDebugLevel, stdout, "Could not read HZ data from kernel: %s", kvm_geterr(kd));
	return;
    }
    stats->Spare4 = hz;

    if (KVM_READ(kd, DISK, &dk_ndrive, sizeof (dk_ndrive))) {
	LogMsg(0, SrvDebugLevel, stdout, "Could not read DISK data from kernel: %s", kvm_geterr(kd));
	return;
    }
}
#endif

#ifdef __linux__
/* Actually, most of these statistics could also be read from an snmp daemon
 * running on the server host. */
void SetSystemStats_linux(ViceStatistics *stats)
{
    uint32_t d1, d2, d3, d4;
    static char line[1024];
    FILE *f;

#define PARSELINE(file, pattern, args...) do { int i; \
        for (i = 0; i < 16; i++) { \
	    if (fscanf(file, pattern, ## args) != 0) break; \
	    fgets(line, 1024, file); \
	} } while(0);

    f = fopen("/proc/stat", "r");
    if (f) {
	PARSELINE(f, "cpu %u %u %u %u", &stats->UserCPU, &stats->NiceCPU,
		  &stats->SystemCPU, &stats->IdleCPU);
	PARSELINE(f, "disk %u %u %u %u", &d1, &d2, &d3, &d4);
	stats->TotalIO = d1 + d2 + d3 + d4;
        PARSELINE(f, "btime %u", &stats->BootTime);
	fclose(f);
    }

    f = fopen("/proc/self/status", "r");
    if (f) {
	PARSELINE(f, "VmSize: %u kB", &stats->ProcessSize);
	PARSELINE(f, "VmRSS: %u kB",  &stats->VmRSS);
	PARSELINE(f, "VmData: %u kB", &stats->VmData);
	fclose(f);
    }
}
#endif

static struct rusage resource;

static void SetSystemStats(ViceStatistics *stats)
{
    stats->CurrentTime = time(0);
    
    getrusage(RUSAGE_SELF, &resource);

    stats->MinFlt = resource.ru_minflt;
    stats->MajFlt = resource.ru_majflt;
    stats->NSwaps = resource.ru_nswap;

    /* keeping time 100's of seconds wraps in 497 days */
    stats->UsrTime = (resource.ru_utime.tv_sec * 100 +
		      resource.ru_utime.tv_usec / 10000);
    stats->SysTime = (resource.ru_stime.tv_sec * 100 +
		      resource.ru_stime.tv_usec / 10000);

#ifdef __BSD44__
    SetSystemStats_bsd44(stats);
#endif
#ifdef __linux__
    SetSystemStats_linux(stats);
#endif
}

static void PrintVolumeStatus(VolumeStatus *status)
{
    SLog(5,"Volume header contains:");
    SLog(5,"Vid = %u, Parent = %u, Online = %d, InService = %d, Blessed = %d, NeedsSalvage = %d",
	    status->Vid, status->ParentId, status->Online, status->InService,
	    status->Blessed, status->NeedsSalvage);
    SLog(5,"MinQuota = %d, MaxQuota = %d", status->MinQuota, status->MaxQuota);
    SLog(5,"Type = %d, BlocksInUse = %d, PartBlocksAvail = %d, PartMaxBlocks = %d",
	    status->Type, status->BlocksInUse, status->PartBlocksAvail, status->PartMaxBlocks);
}


/*
  ViceNewConnectFS: Called by client (userent::Connect) after connection setup
*/
long FS_ViceNewConnectFS(RPC2_Handle RPCid, RPC2_Unsigned ViceVersion, 
			 ViceClient *ClientId)
{
    long errorCode;
    ClientEntry *client = NULL;
    char *rock;

    SLog(1, "FS_ViceNewConnectFS (version %d) for user %s at %s.%s",
         ViceVersion, ClientId->UserName, ClientId->WorkStationName, 
         ClientId->VenusName);

    errorCode = RPC2_GetPrivatePointer(RPCid, &rock);
    client = (ClientEntry *)rock;

    if (errorCode || !client) {
        SLog(0, "No client structure built by ViceNewConnection");
	return(RPC2_FAIL);
    }

    if (ViceVersion != VICE_VERSION) {
	CLIENT_CleanUpHost(client->VenusId);
	return(RPC2_FAIL);
    }

    /* we need a lock, because we cannot do concurrent RPC2 calls on the same
     * connection */
    ObtainWriteLock(&client->VenusId->lock);

    /* attempt to send a callback message to this host */
    if (client->VenusId->id != 0) {
	errorCode = CallBack(client->VenusId->id, (ViceFid *)&NullFid);
	if (errorCode) {
	    /* tear down nak'd connection */
	    RPC2_Unbind(client->VenusId->id);
	    client->VenusId->id = 0;
	}
    }			

    ReleaseWriteLock(&client->VenusId->lock);
    /* we don't need the lock here anymore, because MakeCallBackConn
     * does it's own locking */

    /* set up a callback channel if there isn't one for this host */
    if (client->VenusId->id == 0) {
	SLog(0, "Building callback conn.");
	errorCode = CLIENT_MakeCallBackConn(client);
    }

    if (errorCode)
	CLIENT_CleanUpHost(client->VenusId);

    SLog(2, "FS_ViceNewConnectFS returns %s", ViceErrorMsg((int) errorCode));
    
    return(errorCode);
}


