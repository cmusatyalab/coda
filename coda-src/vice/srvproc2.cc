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
#endif __cplusplus

#include <sys/types.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#ifdef __linux__
#include <linux/if_ether.h>
#endif

#include <strings.h>
#include <unistd.h>
#include <stdlib.h>

#include <lwp.h>
#include <lock.h>
#include <rpc2.h>
#include <util.h>
#include <rvmlib.h>
#include <partition.h>
#include <auth2.h>
#include <prs.h>
#include <al.h>
#include <callback.h>
#include <vice.h>
#ifdef __cplusplus
}
#endif __cplusplus

#include <errors.h>
#include <voltypes.h>
#include <vsg.h>
#include <vrdb.h>
#include <vldb.h>
#include <srv.h>
#include <vlist.h>
#include <vice.private.h>
#include <operations.h>
#include <ops.h>
#include "coppend.h"

/* *****  Exported variables  ***** */

int supported = 1;
unsigned int etherWrites = 0;
unsigned int etherRetries = 0;
unsigned int etherInterupts = 0;
unsigned int etherGoodReads = 0;
unsigned int etherBytesRead = 0;
unsigned int etherBytesWritten = 0;

/* *****  Private variables  ***** */

static unsigned Cont_Sws = 0;		/* Count context switches for LWP */

/* *****  External routines ***** */
extern int ValidateParms(RPC2_Handle, ClientEntry **, int *ReplicatedOp,
			 VolumeId *, RPC2_CountedBS *, int *Nservers);
/* *****  Private routines  ***** */

static long InternalRemoveCallBack(RPC2_Handle, ViceFid *);
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
    ClientEntry * client;
    long   errorCode;

    SLog(1, "ViceDisconnectFS");
    errorCode = RPC2_GetPrivatePointer(RPCid, (char **)&client);

    if (!errorCode && client) {
	ObtainWriteLock(&client->VenusId->lock);
	CLIENT_Delete(client);
	ReleaseWriteLock(&client->VenusId->lock);
    }
    else {
	SLog(0, "GetPrivate failed in Disconnect rc = %d, client = %d",
		errorCode, client);
    }

    SLog(2, "ViceDisconnectFS returns success");
    return(0);
}


/*
ViceGetStatistics: Used by filestats to get general file server statistics
*/
long FS_ViceGetStatistics(RPC2_Handle RPCid, ViceStatistics *Statistics)
{
    int     errorCode;		/* return code for caller */
#ifdef PERFORMANCE 
    int	    i;
    struct  thread_basic_info	b_info;
    thread_basic_info_t	t_info;
    unsigned	int info_cnt = THREAD_BASIC_INFO_COUNT;
    time_value_t    utval, stval;
#endif PERFORMANCE

    SLog(3, "ViceGetStatistics Received");

    errorCode = 0;

    SetViceStats(Statistics);
    SetRPCStats(Statistics);
    SetVolumeStats(Statistics);
    SetSystemStats(Statistics);
    /* Set LWP statistics */
    Statistics->Spare1 = Cont_Sws;
#ifdef PERFORMANCE
    /* get timing info from thread_info */
    utval.seconds = utval.microseconds = 0;
    stval.seconds = stval.microseconds = 0;
    t_info = &b_info;
    for (i = 0; i < thread_count; i++){
	if (thread_info(thread_list[i], THREAD_BASIC_INFO, (thread_info_t)t_info, &info_cnt) != KERN_SUCCESS)
	    SLog(1, "Couldn't get info for thread %d", i);
	else{
	    time_value_add(&utval, &(t_info->user_time));
	    time_value_add(&stval, &(t_info->system_time));
	}
    }
    /* Convert times to hertz */
    Statistics->Spare2 = (int)(60 * utval.seconds) + (int)(6 * utval.microseconds/100000);
    Statistics->Spare3 = (int)(60 * stval.seconds) + (int)(6 * stval.microseconds/100000);
    
#endif PERFORMANCE

    SLog(3, "ViceGetStatistics returns %s", ViceErrorMsg(errorCode));
    return(errorCode);
}


static const int RCBEntrySize = (int) sizeof(ViceFid);

/*
  ViceRemoveCallBack: Deletes callback for a fid
*/
long FS_ViceRemoveCallBack(RPC2_Handle RPCid, RPC2_CountedBS *RCBBS) 
{
    int errorCode = 0;
    char *cp = (char *)RCBBS->SeqBody;
    char *endp = cp + RCBBS->SeqLen;

    if (RCBBS->SeqLen % RCBEntrySize != 0) {
	errorCode = EINVAL;
	goto Exit;
    }

    for (; cp < endp; cp += RCBEntrySize)
	(void)InternalRemoveCallBack(RPCid, (ViceFid *)cp);

Exit:
    return(errorCode);
}


static long InternalRemoveCallBack(RPC2_Handle RPCid, ViceFid *fid)
{
    long   errorCode;
    ClientEntry * client;

    errorCode = 0;
    
    SLog(1,"ViceRemoveCallBack Fid = %u.%d.%d",
	    fid->Volume, fid->Vnode, fid->Unique);

    errorCode = RPC2_GetPrivatePointer(RPCid, (char **)&client);
    if(!errorCode && client)
	DeleteCallBack(client->VenusId, fid);
    else
	SLog(0, "Client pointer zero in ViceRemoveCallBack");
    
    SLog(2,"ViceRemoveCallBack returns %s", ViceErrorMsg((int) errorCode));
    return(errorCode);
}


/*
  ViceGetVolumeInfo: Used to get information about a particular volume
*/
long FS_ViceGetVolumeInfo(RPC2_Handle RPCid, RPC2_String VolName, VolumeInfo *Info)
{
    SLog(1,"ViceGetVolumeInfo volume = %s", VolName);

    int	errorCode = 0;	    /* error code */
    vrent *vre;

    vre = VRDB.find((char *)VolName);
    if (!vre) {
	VolumeId Vid = atoi((char *)VolName);
	if (Vid != 0) vre = VRDB.find(Vid);
    }
    if (vre)
	errorCode = vre->GetVolumeInfo(Info);
    else {
	VGetVolumeInfo((Error *)&errorCode, (char *)VolName, Info);

	if (errorCode == 0) {
	    if (Info->Type == ROVOL) 
		Info->VSGAddr = GetVSGAddress(&(Info->Server0), (int) Info->ServerCount);
	    else if (Info->Type == RWVOL) {
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

    if( name->MaxSeqLen < VNAMESIZE || offlineMsg->MaxSeqLen < VMSGSIZE ||
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

    if((errorCode = RPC2_GetPrivatePointer(RPCid, (char **)&client)) != RPC2_SUCCESS)
	goto Final;

    if(!client) {
	errorCode = EFAULT;
	SLog(0, "Client pointer zero in GetVolumeStatus");
	goto Final;
    }

    CODA_ASSERT(GetRights(client->CPS, aCL, aCLSize, (Rights *)&rights, (Rights *)&anyrights) == 0);

    if((SystemUser(client)) && (!(rights & PRSFS_READ))) {
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
	ViceFid fids[MAXFIDS]; bzero((void *)fids, (int)(MAXFIDS * sizeof(ViceFid)));
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

    if( name->MaxSeqLen < VNAMESIZE || name->SeqLen > VNAMESIZE) {
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

    if(SystemUser(client)) {
	errorCode = EACCES;
	goto Final;
    }

    if(status->MinQuota > -1)
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

    CODA_ASSERT(!(AllowResolution && V_VMResOn(volptr)));

    // Only spool a log entry if the quota was set.
    if (AllowResolution && V_RVMResOn(volptr) && oldquota > -1) 
	if (ReplicatedOp && !errorCode) {
	    SLog(1, 
		   "ViceSetVolumeStatus: About to spool log record, oldquota = %d, new quota = %d\n",
		   oldquota, status->MaxQuota);
	    if ((errorCode = SpoolVMLogRecord(vlist, v, volptr, StoreId,
					     ViceSetVolumeStatus_OP, oldquota,
					     status->MaxQuota)))
		SLog(0, 
		       "ViceSetVolumeStatus: Error %d during SpoolVMLogRecord\n",
		       errorCode);
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


#define DEFAULTVOLUME "1"
/*
  ViceGetRootVolume: Return the name of the root volume
  (corresponding to /coda)
*/
long FS_ViceGetRootVolume(RPC2_Handle RPCid, RPC2_BoundedBS *volume)
{
    int   errorCode;		/* error code */
    int     fd;
    int     len;

    errorCode = 0;

    SLog(1, "ViceGetRootVolume");

    fd = open("/vice/db/ROOTVOLUME", O_RDONLY, 0666);
    if (fd <= 0) {
	strcpy((char *)volume->SeqBody, DEFAULTVOLUME);
	errorCode= VNOVOL;
    } else {
	flock(fd, LOCK_EX);
	len = read(fd, volume->SeqBody, (int) volume->MaxSeqLen);
	flock(fd, LOCK_UN);
	close(fd);
	if (volume->SeqBody[len-1] == '\n')
	    len--;
	volume->SeqBody[len] = '\0';
    }
    volume->SeqLen = strlen((char *)volume->SeqBody) + 1;
#if 0
    /* almost right: sadly we need to check the VRDB instead */
    if ( VLDBLookup(volume->SeqBody) == NULL ) {
	    SLog(0, 
		   "ViceGetRootVolume Volume = %s in ROOTVOLUME is bogus!",
		   volume->SeqBody);
	    SLog(0, SrvDebugLevel, stderr, 
		   "ViceGetRootVolume Volume = %s in ROOTVOLUME is bogus!",
		   volume->SeqBody);
	    errorCode = VNOVOL;
    }
#endif
    SLog(2, 
	   "ViceGetRootVolume returns %s, Volume = %s",
	   ViceErrorMsg(errorCode), volume->SeqBody);

    return(errorCode);
}


/* ViceSetRootVolume: Set the /vicde/db/ROOTVOLUME file to contain the
  name of the new root volume */
long FS_ViceSetRootVolume(RPC2_Handle RPCid, RPC2_String volume)
{
    long   errorCode;		/* error code */
    ClientEntry * client;	/* pointer to client entry */
    int   fd;

    errorCode = 0;

    SLog(1,"ViceSetRootVolume to %s",volume);

    if((errorCode = RPC2_GetPrivatePointer(RPCid, (char **)&client)) != RPC2_SUCCESS)
	goto Final;

    if(!client) {
	errorCode = EFAULT;
	SLog(0, "Client pointer is zero in SetRootVolume");
	goto Final;
    }

    if(SystemUser(client)) {
	errorCode = EACCES;
	goto Final;
    }

    fd = open("/vice/db/ROOTVOLUME", O_WRONLY+O_CREAT+O_TRUNC, 0666);
    CODA_ASSERT(fd > 0);
    flock(fd,LOCK_EX);
    write(fd, volume, (int) strlen((char *)volume));
    fsync(fd);
    flock(fd,LOCK_UN);
    close(fd);

 Final:

    SLog (2, "ViceSetRootVolume returns %s", ViceErrorMsg((int) errorCode));
    return(errorCode);
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

	TM_GetTimeOfDay(&tpl,&tspl);
	*seconds = tpl.tv_sec;
	*useconds = tpl.tv_usec;

	errorCode = RPC2_GetPrivatePointer(RPCid, (char **)&client);

	if (!client) { 
		SLog(0, "No client structure built by ViceNewConnection");
		return ENOTCONN;
	}

	SLog(1, "ViceGetTime for user %s at %s on conn %d.",
	     client->UserName, client->VenusId->HostName, RPCid);
	
	if (!errorCode && client) {
		/* set up a callback channel 
		   if there isn't one for this host */
		if (client->VenusId->id != 0) {
			errorCode = CallBack(client->VenusId->id, &NullFid);
			if ( errorCode != RPC2_SUCCESS ) {
				SLog(0, "GetTime: Destroying callback conn for %s",
				     client->VenusId->HostName);
				/* tear down nak'd connection */
				RPC2_Unbind(client->VenusId->id);
				client->VenusId->id = 0;
			}
		}
		if (client->VenusId->id == 0) {
			SLog(0, "GetTime: Building callback conn to %s.",
			     client->VenusId->HostName);
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
	ClientEntry *client = 0;
	SecretToken st;
	char    user[PRS_MAXNAMELEN+1];
	long errorCode;

	if (sl == RPC2_OPENKIMONO) {
		if (cid->SeqLen > 0)
			strncpy(user, (char *)cid->SeqBody, PRS_MAXNAMELEN);
		else
			strcpy(user, NEWCONNECT);
	} else {
		bcopy(cid->SeqBody, (char *)&st, (int)cid->SeqLen);
		SLog(1, "Authorized Connection for uid %d, Start %d, end %d, time %d",
		     st.ViceId, st.BeginTimestamp, st.EndTimestamp, time(0));
		if (AL_IdToName((int) st.ViceId, user))
			strcpy(user, "System:AnyUser");
	}

	errorCode = CLIENT_Build(RPCid, user, sl, &client);
	if (!errorCode) 
		client->SEType = (int) set;

	SLog(1,"New connection received RPCid %d, security level %d, remote cid %d returns %s",
	     RPCid, sl, cid, ViceErrorMsg((int) errorCode));
	return(errorCode);
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

/*
    stats->CurrentMsgNumber = MsgNumber;
    stats->OldestMsgNumber = OldMsgNumber;
*/
    stats->StartTime = StartTime;
    stats->CurrentConnections = CurrentConnections;
    stats->TotalViceCalls = Counters[TOTAL];
    stats->TotalFetchs = Counters[ViceFetch_OP];
    stats->FetchDatas = Counters[FETCHDATAOP];
    stats->FetchedBytes = Counters[FETCHDATA];
    seconds = Counters[FETCHTIME]/1000;
    if(seconds <= 0) seconds = 1;
    stats->FetchDataRate = Counters[FETCHDATA]/seconds;
    stats->TotalStores = Counters[ViceNewVStore_OP];
    stats->StoreDatas = Counters[STOREDATAOP];
    stats->StoredBytes = Counters[STOREDATA];
    seconds = Counters[STORETIME]/1000;
    if(seconds <= 0) seconds = 1;
    stats->StoreDataRate = Counters[STOREDATA]/seconds;
/*    stats->ProcessSize = sbrk(0) >> 10; */
    stats->ProcessSize = 0;
    CLIENT_GetWorkStats((int *)&(stats->WorkStations),(int *)&(stats->ActiveWorkStations),
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
	struct DiskPartition * part;
	struct dllist_head *tmp;
	struct ViceDisk *disk; 
	int i;

	tmp = DiskPartitionList.next;

	for ( i = 0 ; i < 10 ; i++ ) {
		part = get_part(tmp);
		/* beware: pointer arithmetic */
		disk = &(stats->Disk1) + i;
		if(part) {
			disk->TotalBlocks = part->totalUsable;
			disk->BlocksAvailable = part->free;
			bzero(disk->Name, 32);
			strncpy((char *)disk->Name, part->name, 32);
			tmp = tmp->next;
		} else
			disk->TotalBlocks = -1;
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


#include <sys/resource.h>
static struct rusage resource;

static void SetSystemStats(ViceStatistics *stats)
{
    struct timeval time;
    struct timeval thistime;

    stats->UserCPU = 0;
    stats->SystemCPU = 0;
    stats->NiceCPU = 0;
    stats->IdleCPU = 0;
    stats->BootTime = 0;
    stats->TotalIO = 0;
    stats->ActiveVM = 0;
    stats->TotalVM = 0;
    stats->ProcessSize = 0;
    stats->Spare1 = 0;
    stats->Spare2 = 0;
    stats->Spare3 = 0;
    stats->Spare4 = 0;
    stats->Spare5 = 0;
    stats->Spare6 = 0;
    stats->Spare7 = 0;
    stats->Spare8 = 0;
    TM_GetTimeOfDay(&time, 0);
    stats->CurrentTime = time.tv_sec;
    
    gettimeofday(&thistime, 0);
    getrusage(RUSAGE_SELF, &resource);

    /* leave Spare 1..3 will be used later */
    /* keeping time 100's of seconds wraps in 497 days */
    stats->Spare5   = resource.ru_utime.tv_sec * 100 + resource.ru_utime.tv_usec / 10000;
    stats->Spare6   = resource.ru_stime.tv_sec * 100 + resource.ru_stime.tv_usec / 10000;


#if	defined(__BSD44__)
    SetSystemStats_bsd44(stats);
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
  BEGIN_HTML
  <a name="ViceEnableGroup"><strong>Used to enable an authentication group</strong></a>
  END_HTML
*/
long FS_ViceEnableGroup(RPC2_Handle cid, RPC2_String GroupName)
    {
    int gid;
    ClientEntry *client;

    SLog(1,"ViceEnableGroup GroupName = %s", GroupName);

    if (AL_NameToId((char *)GroupName, &gid) < 0) return(EINVAL);
    if (RPC2_GetPrivatePointer(cid, (char **)&client) != RPC2_SUCCESS) return (EINVAL);
    if (client == NULL) return(EINVAL);
    if (AL_EnableGroup(gid, client->CPS) < 0) return(EINVAL);
    return(0);
    }


/*
  BEGIN_HTML
  <a name="ViceDisableGroup"><strong>Used to disable an authentication group</strong></a>
  END_HTML
*/
long FS_ViceDisableGroup(RPC2_Handle cid, RPC2_String GroupName)
    {
    int gid;
    ClientEntry *client;

    SLog(1,"ViceDisableGroup GroupName = %s", GroupName);

    if (AL_NameToId((char *)GroupName, &gid) < 0) return(EINVAL);
    if (RPC2_GetPrivatePointer(cid, (char **)&client) != RPC2_SUCCESS) return (EINVAL);
    if (client == NULL) return(EINVAL);
    if (AL_DisableGroup(gid, client->CPS) < 0) return(EINVAL);
    return(0);
    }


/*
  ViceNewConnectFS: Called by client (userent::Connect) after connection setup
*/
long FS_ViceNewConnectFS(RPC2_Handle RPCid, RPC2_Unsigned ViceVersion, 
			 ViceClient *ClientId)
{
    long errorCode;
    ClientEntry *client = NULL;

    SLog(1, "FS_ViceNewConnectFS (version %d) for user %s at %s.%s",
         ViceVersion, ClientId->UserName, ClientId->WorkStationName, 
         ClientId->VenusName);

    errorCode = RPC2_GetPrivatePointer(RPCid, (char **)&client);

    if (errorCode || !client) {
        SLog(0, "No client structure built by ViceNewConnection");
	return(ENOTCONN);
    }

    /* attempt to send a callback message to this host */
    if (client->VenusId->id != 0) {
	errorCode = CallBack(client->VenusId->id, &NullFid);
	if ( errorCode != RPC2_SUCCESS ) {
	    /* tear down nak'd connection */
	    RPC2_Unbind(client->VenusId->id);
	    client->VenusId->id = 0;
	}
    }			

    /* set up a callback channel if there isn't one for this host */
    if (client->VenusId->id == 0) {
	SLog(0, "Building callback conn.");
	errorCode = CLIENT_MakeCallBackConn(client);
    }

    SLog(2, "FS_ViceNewConnectFS returns %s", 
         ViceErrorMsg((int) errorCode));

    return(errorCode);
}


