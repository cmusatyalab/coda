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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/vice/srvproc2.cc,v 4.5 1998/01/10 18:39:36 braam Exp $";
#endif /*_BLURB_*/


/*

                         IBM COPYRIGHT NOTICE

                          Copyright (C) 1986
             International Business Machines Corporation
                         All Rights Reserved

This  file  contains  some  code identical to or derived from the 1986
version of the Andrew File System ("AFS"), which is owned by  the  IBM
Corporation.    This  code is provded "AS IS" and IBM does not warrant
that it is free of infringement of  any  intellectual  rights  of  any
third  party.    IBM  disclaims  liability of any kind for any damages
whatsoever resulting directly or indirectly from use of this  software
or  of  any  derivative work.  Carnegie Mellon University has obtained
permission to distribute this code, which is based on Version 2 of AFS
and  does  not  contain the features and enhancements that are part of
Version 3 of AFS.  Version 3 of  AFS  is  commercially  available  and
supported by Transarc Corporation, Pittsburgh, PA.

*/


#define	TRAFFIC	    /* ? -JJK */

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
#ifdef __BSD44__
#include <netinet/if_ether.h>
#endif
#if defined(__GLIBC__) && __GLIBC__ >= 2
#include <libelf/nlist.h>
#else
#include <nlist.h>
/* nlist.h defines this function but it isnt getting included because it is
   guarded by an ifdef of CMU which isnt getting defined.  XXXXX pkumar 6/13/95 */ 
extern int nlist(const char*, struct nlist[]);
#endif

#include <strings.h>
#include <unistd.h>
#include <stdlib.h>

#include <lwp.h>
#include <lock.h>
#include <rpc2.h>
#include <util.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <rvmlib.h>
#include <errors.h>
#include <voltypes.h>
#include <partition.h>
#include <auth2.h>
#include <prs.h>
#include <al.h>
#include <vsg.h>
#include <vlist.h>
#include <vrdb.h>
#include <srv.h>
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

PRIVATE	unsigned Cont_Sws = 0;		/* Count context switches for LWP */
PRIVATE struct nlist RawStats[] =
{
#define CPTIME 0
    {
	"_cp_time"
    },
#define SWAPMAP 1
    {
	"_swapmap"
    },
#define BOOT 2
    {
	"_boottime"
    },
#define DISK 3
    {
	"_dk_xfer"
    },
#define NSWAPMAP 4
    {
	"_nswapmap"
    },
#define NSWAPBLKS 5
    {
	"_nswap"
    },
#define DMMAX 6
    {
	"_dmmax"
    },
    {
	0
    },
};


/* *****  External routines ***** */
extern int ValidateParms(RPC2_Handle, ClientEntry **, int, VolumeId *, 
			 RPC2_CountedBS *);
/* *****  Private routines  ***** */

PRIVATE long InternalRemoveCallBack(RPC2_Handle, ViceFid *);
PRIVATE void SetVolumeStatus(VolumeStatus *, RPC2_BoundedBS *,
			      RPC2_BoundedBS *, RPC2_BoundedBS *, Volume *);
PRIVATE void SetViceStats(ViceStatistics *);
PRIVATE void SetRPCStats(ViceStatistics *);
PRIVATE void SetVolumeStats(ViceStatistics *);
PRIVATE void SetSystemStats(ViceStatistics *);
PRIVATE void PrintVolumeStatus(VolumeStatus *);
PRIVATE void PrintUnusedComplaint(RPC2_Handle, RPC2_Integer, char *);


/*
  BEGIN_HTML
  <a name="ViceConnectFS"><strong>Client request to connect to file server</strong></a>
  END_HTML
*/ 
long ViceConnectFS(RPC2_Handle RPCid, RPC2_Unsigned ViceVersion, ViceClient *ClientId)
{
    LogMsg(1, SrvDebugLevel, stdout, "ViceConnectFS (version %d) for user %s at %s.%s",
	    ViceVersion, ClientId->UserName, ClientId->WorkStationName, ClientId->VenusName);

    long errorCode;
    ClientEntry *client;

    errorCode = RPC2_GetPrivatePointer(RPCid, (char **)&client);

    if (!client) 
	LogMsg(0, SrvDebugLevel, stdout, "No client structure built by ViceNewConnection");

    if (!errorCode && client) 
	/* set up a callback channel if there isn't one for this host */
	if (client->VenusId->id == 0) 
	    errorCode = MakeCallBackConn(client);

    LogMsg(2, SrvDebugLevel, stdout, "ViceConnectFS returns %s", 
	   ViceErrorMsg((int) errorCode));

    return(errorCode);
}


/*
  BEGIN_HTML
  <a name="ViceDisconnectFS"><strong>Client request for termination</strong></a> 
  END_HTML
*/
long ViceDisconnectFS(RPC2_Handle RPCid)
{
    ClientEntry * client;
    long   errorCode;

    LogMsg(1, SrvDebugLevel, stdout, "ViceDisconnectFS");
    errorCode = RPC2_GetPrivatePointer(RPCid, (char **)&client);

    if (!errorCode && client) {
	ObtainWriteLock(&client->VenusId->lock);
	DeleteClient(client);
	ReleaseWriteLock(&client->VenusId->lock);
    }
    else {
	LogMsg(0, SrvDebugLevel, stdout, "GetPrivate failed in Disconnect rc = %d, client = %d",
		errorCode, client);
    }

    LogMsg(2, SrvDebugLevel, stdout, "ViceDisconnectFS returns success");
    return(0);
}


/*
  BEGIN_HTML
  <a name="ViceGetStatistics"><strong>Used by filestats to get general file server
  statistics</strong></a> 
  END_HTML
*/
long ViceGetStatistics(RPC2_Handle RPCid, ViceStatistics *Statistics)
{
    int     errorCode;		/* return code for caller */
#ifdef PERFORMANCE 
    int	    i;
    struct  thread_basic_info	b_info;
    thread_basic_info_t	t_info;
    unsigned	int info_cnt = THREAD_BASIC_INFO_COUNT;
    time_value_t    utval, stval;
#endif PERFORMANCE

    LogMsg(3, SrvDebugLevel, stdout, "ViceGetStatistics Received");

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
	    LogMsg(1, SrvDebugLevel, stdout, "Couldn't get info for thread %d", i);
	else{
	    time_value_add(&utval, &(t_info->user_time));
	    time_value_add(&stval, &(t_info->system_time));
	}
    }
    /* Convert times to hertz */
    Statistics->Spare2 = (int)(60 * utval.seconds) + (int)(6 * utval.microseconds/100000);
    Statistics->Spare3 = (int)(60 * stval.seconds) + (int)(6 * stval.microseconds/100000);
    
#endif PERFORMANCE

    LogMsg(3, SrvDebugLevel, stdout, "ViceGetStatistics returns %s", ViceErrorMsg(errorCode));
    return(errorCode);
}


PRIVATE const int RCBEntrySize = (int) sizeof(ViceFid);

/*
  BEGIN_HTML
  <a name="ViceRemoveCallBack"><strong>Deletes callback for a fid</strong></a> 
  END_HTML
*/
long ViceRemoveCallBack(RPC2_Handle RPCid, RPC2_CountedBS *RCBBS) {
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


PRIVATE long InternalRemoveCallBack(RPC2_Handle RPCid, ViceFid *fid)
{
    long   errorCode;
    ClientEntry * client;

    errorCode = 0;
    
    LogMsg(1, SrvDebugLevel, stdout,"ViceRemoveCallBack Fid = %u.%d.%d",
	    fid->Volume, fid->Vnode, fid->Unique);

    errorCode = RPC2_GetPrivatePointer(RPCid, (char **)&client);
    if(!errorCode && client)
	DeleteCallBack(client->VenusId, fid);
    else
	LogMsg(0, SrvDebugLevel, stdout, "Client pointer zero in ViceRemoveCallBack");
    
    LogMsg(2, SrvDebugLevel, stdout,"ViceRemoveCallBack returns %s", ViceErrorMsg((int) errorCode));
    return(errorCode);
}


/*
  BEGIN_HTML
  <a name="ViceGetVolumeInfo"><strong>Used to get information about a particular volume </strong></a> 
  END_HTML
*/
long ViceGetVolumeInfo(RPC2_Handle RPCid, RPC2_String VolName, VolumeInfo *Info)
{
    LogMsg(1, SrvDebugLevel, stdout,"ViceGetVolumeInfo volume = %s", VolName);

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

    LogMsg(2, SrvDebugLevel, stdout, "ViceGetVolumeInfo returns %s, Volume %u, type %x, servers %x %x %x %x...",
	    ViceErrorMsg(errorCode), Info->Vid, Info->Type,
	    Info->Server0, Info->Server1, Info->Server2, Info->Server3);
    return(errorCode);
}


/*
  BEGIN_HTML
  <a name="ViceGetVolumeStatus"><strong>Get the status of a particular volume</strong></a> 
  END_HTML
*/
long ViceGetVolumeStatus(RPC2_Handle RPCid, VolumeId vid, VolumeStatus *status, RPC2_BoundedBS *name, RPC2_BoundedBS *offlineMsg, RPC2_BoundedBS *motd, RPC2_Unsigned PrimaryHost)
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
    int tstatus = 0;		/* transaction status variable */

    errorCode = fileCode = 0;
    vptr = 0;
    volptr = 0;
    VolumeId VSGVolnum = vid;

    LogMsg(1, SrvDebugLevel, stdout,"GetVolumeStatus for volume %u",vid);

    if (vid == 0) {
	errorCode = EINVAL;
	goto Final;
    }

    if (XlateVid(&vid)) {
	LogMsg(1, SrvDebugLevel, stdout, "XlateVid: %u --> %u", VSGVolnum, vid);
	if (PrimaryHost == 0) {
	    LogMsg(0, SrvDebugLevel, stdout, "Translated VSG but PrimaryHost == 0");
	    errorCode =	EINVAL;	    /* ??? -JJK */
	    goto Final;
	}
    }
    else {
	if (PrimaryHost != 0) {
	    LogMsg(0, SrvDebugLevel, stdout, "Failed to translate VSG but PrimaryHost != 0");
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
    if (errorCode = GetFsObj(&vfid, &volptr, &vptr, READ_LOCK, VOL_NO_LOCK, 1, 0)) {
	goto Final;
    }

    SetAccessList(vptr, aCL, aCLSize);

    if((errorCode = RPC2_GetPrivatePointer(RPCid, (char **)&client)) != RPC2_SUCCESS)
	goto Final;

    if(!client) {
	errorCode = EFAULT;
	LogMsg(0, SrvDebugLevel, stdout, "Client pointer zero in GetVolumeStatus");
	goto Final;
    }

    assert(GetRights(client->CPS, aCL, aCLSize, (Rights *)&rights, (Rights *)&anyrights) == 0);

    if((SystemUser(client)) && (!(rights & PRSFS_READ))) {
	errorCode = EACCES;
	goto Final;
    }

    SetVolumeStatus(status, name, offlineMsg, motd, volptr);

 Final:
    if (vptr) {
	VPutVnode(&fileCode, vptr);
	assert(fileCode == 0);
    }
    PutVolObj(&volptr, VOL_NO_LOCK, 0);

    LogMsg(2, SrvDebugLevel, stdout,"GetVolumeStatus returns %s", ViceErrorMsg((int) errorCode));
    if(!errorCode) {
	LogMsg(5, SrvDebugLevel, stdout,"Name = %s, Motd = %s, offMsg = %s",
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
long ViceSetVolumeStatus(RPC2_Handle RPCid, VolumeId vid, VolumeStatus *status, RPC2_BoundedBS *name, RPC2_BoundedBS *offlineMsg, RPC2_BoundedBS *motd, RPC2_Unsigned PrimaryHost, ViceStoreId *StoreId, RPC2_CountedBS *PiggyCOP2)
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
    int tstatus = 0;		/* transaction status variable */
    int oldquota = -1;               /* Old quota if it was changed */
    int ReplicatedOp = (PrimaryHost != 0);
    vle *v = 0;
    dlist *vlist = new dlist((CFN)VLECmp);

    errorCode = fileCode = 0;
    volptr = 0;
    VolumeId VSGVolnum = vid;

    LogMsg(1, SrvDebugLevel, stdout,"ViceSetVolumeStatus for volume %u", vid);
    LogMsg(5, SrvDebugLevel, stdout,"Min = %d Max = %d, Name = %d.%d %s, Offline Msg = %d.%d.%s, motd = %d.%d.%s",
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
	if (errorCode = ValidateParms(RPCid, &client, ReplicatedOp, &vid, 
				      PiggyCOP2))
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

    if (errorCode = GetVolObj(vid, &volptr, VOL_EXCL_LOCK, 0, 1 /* check this */)) {
	LogMsg(0, SrvDebugLevel, stdout, "Error locking volume in ViceSetVolumeStatus: %s", ViceErrorMsg((int) errorCode));
	goto Final ;
    }


    v = AddVLE(*vlist, &vfid);
    if (errorCode = GetFsObj(&vfid, &volptr, &v->vptr, READ_LOCK, VOL_NO_LOCK, 0, 0)) {
	goto Final;
    }

    SetAccessList(v->vptr, aCL, aCLSize);

    if(!client) {
	errorCode = EFAULT;
	LogMsg(0, SrvDebugLevel, stdout, "Client pointer is zero in ViceSetVolumeStatus");
	goto Final;
    }

    assert(GetRights(client->CPS, aCL, aCLSize, (Rights *)&rights, (Rights *)&anyrights) == 0);

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

    assert(!(AllowResolution && V_VMResOn(volptr)));

    // Only spool a log entry if the quota was set.
    if (AllowResolution && V_RVMResOn(volptr) && oldquota > -1) 
	if (ReplicatedOp && !errorCode) {
	    LogMsg(1, SrvDebugLevel, stdout, 
		   "ViceSetVolumeStatus: About to spool log record, oldquota = %d, new quota = %d\n",
		   oldquota, status->MaxQuota);
	    if (errorCode = SpoolVMLogRecord(vlist, v, volptr, StoreId,
					     ViceSetVolumeStatus_OP, oldquota,
					     status->MaxQuota))
		LogMsg(0, SrvDebugLevel, stdout, 
		       "ViceSetVolumeStatus: Error %d during SpoolVMLogRecord\n",
		       errorCode);
	}

 Final:

    if (!errorCode) 
        SetVolumeStatus(status, name, offlineMsg, motd, volptr);

    PutObjects((int) errorCode, volptr, VOL_EXCL_LOCK, vlist, 0, 1, TRUE);

    LogMsg(2, SrvDebugLevel, stdout,"ViceSetVolumeStatus returns %s", ViceErrorMsg((int) errorCode));
    if(!errorCode) {
	LogMsg(5, SrvDebugLevel, stdout,"Name = %s, Motd = %s, offMsg = %s",
		name->SeqBody, motd->SeqBody, offlineMsg->SeqBody);
	PrintVolumeStatus(status);
    }
    return(errorCode);
}


#define DEFAULTVOLUME "1"
/*
  BEGIN_HTML
  <a name="ViceGetRootVolume"><strong>Return the name of the root volume
  (corresponding to <tt>/coda</tt>)</strong></a> 
  END_HTML
*/
long ViceGetRootVolume(RPC2_Handle RPCid, RPC2_BoundedBS *volume)
{
    int   errorCode;		/* error code */
    int     fd;
    int     len;

    errorCode = 0;

    LogMsg(1, SrvDebugLevel, stdout, "ViceGetRootVolume");

    fd = open("/ROOTVOLUME", O_RDONLY, 0666);
    if (fd <= 0) {
	strcpy((char *)volume->SeqBody, DEFAULTVOLUME);
    }
    else {
	flock(fd, LOCK_EX);
	len = read(fd, volume->SeqBody, (int) volume->MaxSeqLen);
	flock(fd, LOCK_UN);
	close(fd);
	if (volume->SeqBody[len-1] == '\n')
	    len--;
	volume->SeqBody[len] = '\0';
    }
    volume->SeqLen = strlen((char *)volume->SeqBody) + 1;

    LogMsg(2, SrvDebugLevel, stdout, "ViceGetRootVolume returns %s, Volume = %s",
	    ViceErrorMsg(errorCode), volume->SeqBody);

    return(errorCode);
}


/*
  BEGIN_HTML
  <a name="ViceSetRootVolume">
  <strong>Set the /ROOTVOLUME file to contain the name of the new root volume </strong></a> 
  END_HTML
*/
long ViceSetRootVolume(RPC2_Handle RPCid, RPC2_String volume)
{
    long   errorCode;		/* error code */
    ClientEntry * client;	/* pointer to client entry */
    int   fd;

    errorCode = 0;

    LogMsg(1, SrvDebugLevel, stdout,"ViceSetRootVolume to %s",volume);

    if((errorCode = RPC2_GetPrivatePointer(RPCid, (char **)&client)) != RPC2_SUCCESS)
	goto Final;

    if(!client) {
	errorCode = EFAULT;
	LogMsg(0, SrvDebugLevel, stdout, "Client pointer is zero in SetRootVolume");
	goto Final;
    }

    if(SystemUser(client)) {
	errorCode = EACCES;
	goto Final;
    }

    fd = open("/ROOTVOLUME", O_WRONLY+O_CREAT+O_TRUNC, 0666);
    assert(fd > 0);
    flock(fd,LOCK_EX);
    write(fd, volume, (int) strlen((char *)volume));
    fsync(fd);
    flock(fd,LOCK_UN);
    close(fd);

 Final:

    LogMsg (2, SrvDebugLevel, stdout, "ViceSetRootVolume returns %s", ViceErrorMsg((int) errorCode));
    return(errorCode);
}


/*
  BEGIN_HTML
  <a name="ViceGetTime"><strong>Returns time of day (a time ping) </strong></a> 
  END_HTML
*/

long ViceGetTime(RPC2_Handle RPCid, RPC2_Unsigned *seconds, RPC2_Integer *useconds)
{
    struct timeval tpl;
    struct timezone tspl;

    LogMsg(1, SrvDebugLevel, stdout, "Received GetTime");
    TM_GetTimeOfDay(&tpl,&tspl);
    *seconds = tpl.tv_sec;
    *useconds = tpl.tv_usec;
    LogMsg(2, SrvDebugLevel, stdout, "GetTime returns %d, %d", *seconds, *useconds);
    return(0);
}


/*
  BEGIN_HTML
  <a name="ViceNewConnection"><strong>Called the first time a client contacts a server</strong></a> 
  END_HTML
*/

long ViceNewConnection(RPC2_Handle RPCid, RPC2_Integer set, RPC2_Integer sl,
			 RPC2_Integer et, RPC2_CountedBS *cid)
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
	LogMsg(1, SrvDebugLevel, stdout, "Authorized Connection for uid %d, Start %d, end %d, time %d",
		st.ViceId, st.BeginTimestamp, st.EndTimestamp, time(0));
	if (AL_IdToName((int) st.ViceId, user))
	    strcpy(user, "System:AnyUser");
    }

    errorCode = BuildClient(RPCid, user, sl, &client);
    if (!errorCode) client->SEType = (int) set;

    LogMsg(1, SrvDebugLevel, stdout, "New connection received RPCid %d, security level %d, remote cid %d returns %s",
	    RPCid, sl, cid, ViceErrorMsg((int) errorCode));
    return(errorCode);
}


PRIVATE void SetVolumeStatus(VolumeStatus *status, RPC2_BoundedBS *name, RPC2_BoundedBS *offMsg,
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


PRIVATE void SetViceStats(ViceStatistics *stats)
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
    stats->TotalStores = Counters[ViceNewStore_OP] + Counters[ViceNewVStore_OP];
    stats->StoreDatas = Counters[STOREDATAOP];
    stats->StoredBytes = Counters[STOREDATA];
    seconds = Counters[STORETIME]/1000;
    if(seconds <= 0) seconds = 1;
    stats->StoreDataRate = Counters[STOREDATA]/seconds;
/*    stats->ProcessSize = sbrk(0) >> 10; */
    stats->ProcessSize = 0;
    GetWorkStats((int *)&(stats->WorkStations),(int *)&(stats->ActiveWorkStations),
	    (unsigned)(time(0)-15*60));
}


PRIVATE void SetRPCStats(ViceStatistics *stats)
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
#ifdef sun3
#ifndef sunV3
    if (supported && !GetEtherStats()) {
	stats->EtherNetTotalErrors = etherRetries;
	stats->EtherNetTotalWrites = etherWrites;
	stats->EtherNetTotalInterupts = etherInterupts;
	stats->EtherNetGoodReads = etherGoodReads;
	stats->EtherNetTotalBytesWritten = etherBytesWritten;
	stats->EtherNetTotalBytesRead = etherBytesRead;
    }
#endif
#endif
}


int GetEtherStats()
{
#ifdef sun3
#ifndef sunV3
    int     rc;
    struct  etherstats	ether;
    struct  ifreq	ifr;
    static  int		etherFD = 0;

    if (!etherFD) {
	etherFD = socket(AF_INET, SOCK_DGRAM, 0);
	if (etherFD <= 0) {
	    LogMsg(0, SrvDebugLevel, stdout, "Open for Socket failed with %s", ViceErrorMsg(errno));
	    etherFD = 0;
	    supported = 0;
	    return(-1);
	}
    }
    strcpy(ifr.ifr_name, "ec0");
    ifr.ifr_cptr = (char *) & ether;
    rc = ioctl(etherFD, SIOCQTRAFFIC, &ifr);
    if (rc) {
	LogMsg(0, SrvDebugLevel, stdout, "IOCTL for SIOCQTRAFFIC not supported");
	close(etherFD);
	supported = 0;
	return(-1);
    }
    etherRetries = ether.WriteRetries;
    etherWrites = ether.WriteAttempts;
    etherInterupts = ether.InterruptsHandled;
    etherGoodReads = ether.ReadSuccesses;
    etherBytesRead = ether.BytesWritten;
    etherBytesWritten = ether.BytesRead;
    return(0);
#endif
#endif
return(0);
}


PRIVATE void SetVolumeStats(ViceStatistics *stats)
{
    struct DiskPartition * part;

    part = DiskPartitionList;
    if(part) {
	stats->Disk1.TotalBlocks = part->totalUsable;
	stats->Disk1.BlocksAvailable = part->free;
	bzero(stats->Disk1.Name,32);
	strncpy((char *)stats->Disk1.Name,part->name,32);
	part = part->next;
    }
    else
	stats->Disk1.TotalBlocks = -1;
    if(part) {
	stats->Disk2.TotalBlocks = part->totalUsable;
	stats->Disk2.BlocksAvailable = part->free;
	bzero(stats->Disk2.Name,32);
	strncpy((char *)stats->Disk2.Name,part->name,32);
	part = part->next;
    }
    else
	stats->Disk2.TotalBlocks = -1;
    if(part) {
	stats->Disk3.TotalBlocks = part->totalUsable;
	stats->Disk3.BlocksAvailable = part->free;
	bzero(stats->Disk3.Name,32);
	strncpy((char *)stats->Disk3.Name,part->name,32);
	part = part->next;
    }
    else
	stats->Disk3.TotalBlocks = -1;
    if(part) {
	stats->Disk4.TotalBlocks = part->totalUsable;
	stats->Disk4.BlocksAvailable = part->free;
	bzero(stats->Disk4.Name,32);
	strncpy((char *)stats->Disk4.Name,part->name,32);
	part = part->next;
    }
    else
	stats->Disk4.TotalBlocks = -1;
    if(part) {
	stats->Disk5.TotalBlocks = part->totalUsable;
	stats->Disk5.BlocksAvailable = part->free;
	bzero(stats->Disk5.Name,32);
	strncpy((char *)stats->Disk5.Name,part->name,32);
	part = part->next;
    }
    else
	stats->Disk5.TotalBlocks = -1;
    if(part) {
	stats->Disk6.TotalBlocks = part->totalUsable;
	stats->Disk6.BlocksAvailable = part->free;
	bzero(stats->Disk6.Name,32);
	strncpy((char *)stats->Disk6.Name,part->name,32);
	part = part->next;
    }
    else
	stats->Disk6.TotalBlocks = -1;
    if(part) {
	stats->Disk7.TotalBlocks = part->totalUsable;
	stats->Disk7.BlocksAvailable = part->free;
	bzero(stats->Disk7.Name,32);
	strncpy((char *)stats->Disk7.Name,part->name,32);
	part = part->next;
    }
    else
	stats->Disk7.TotalBlocks = -1;
    if(part) {
	stats->Disk8.TotalBlocks = part->totalUsable;
	stats->Disk8.BlocksAvailable = part->free;
	bzero(stats->Disk8.Name,32);
	strncpy((char *)stats->Disk8.Name,part->name,32);
	part = part->next;
    }
    else
	stats->Disk8.TotalBlocks = -1;
    if(part) {
	stats->Disk9.TotalBlocks = part->totalUsable;
	stats->Disk9.BlocksAvailable = part->free;
	bzero(stats->Disk9.Name,32);
	strncpy((char *)stats->Disk9.Name,part->name,32);
	part = part->next;
    }
    else
	stats->Disk9.TotalBlocks = -1;
    if(part) {
	stats->Disk10.TotalBlocks = part->totalUsable;
	stats->Disk10.BlocksAvailable = part->free;
	bzero(stats->Disk10.Name,32);
	strncpy((char *)stats->Disk10.Name,part->name,32);
	part = part->next;
    }
    else
	stats->Disk10.TotalBlocks = -1;
}


PRIVATE void SetSystemStats(ViceStatistics *stats)
{
#ifdef __MACH__
    struct	timeval	time;
    static	int	kmem = 0;
/*  static	struct	mapent	*swapMap = 0;
    static	int	swapMapAddr = 0;
    static	int	swapMapSize = 0;
    static	int	numSwapBlks = 0;
    int		numSwapEntries,
		dmmax;
    struct	mapent	* sp;
*/
    register	int	i;
    long	busy[CPUSTATES];
    long	xfer[DK_NDRIVE];
    struct	timeval	bootTime;

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

    if(kmem == -1) {
	return;
    }
    
    if(kmem == 0) {
	nlist("/vmunix", RawStats);
	if(RawStats[0].n_type == 0) {
	    LogMsg(0, SrvDebugLevel, stdout, "Could not get a namelist from VMUNIX");
	    kmem = -1;
	    return;
	}
	kmem = open("/dev/kmem",0,0);
	if (kmem <= 0) {
	    LogMsg(0, SrvDebugLevel, stdout, "Could not open /dev/kmem");
	    kmem = -1;
	    return;
	}
    }

    lseek(kmem, (long) RawStats[CPTIME].n_value,0);
    read(kmem, (char *)busy, (int)sizeof(busy));
    stats->SystemCPU = busy[CP_SYS];
    stats->UserCPU = busy[CP_USER];
    stats->NiceCPU = busy[CP_NICE];
    stats->IdleCPU = busy[CP_IDLE];
    lseek(kmem, (long) RawStats[BOOT].n_value,0);
    read(kmem, (char *)&bootTime, (int)sizeof(bootTime));
    stats->BootTime = bootTime.tv_sec;
    lseek(kmem, (long) RawStats[DISK].n_value,0);
    read(kmem, (char *)xfer, (int)sizeof(xfer));
    stats->TotalIO = 0;
    for(i = 0; i < DK_NDRIVE; i++) {
	stats->TotalIO += xfer[i];
    }
#endif  
/*
 * Mach doesn't do swapping.  Later we should use an alternative means of finding
 * memory usage and process size; for now we will consider them 0.
  if(!swapMap) {
	lseek(kmem, RawStats[SWAPMAP].n_value,0);
	read(kmem, (char *)&swapMapAddr, sizeof(swapMapAddr));
	swapMapAddr += sizeof(struct map);
	lseek(kmem, RawStats[NSWAPMAP].n_value,0);
	read(kmem, (char *)&numSwapEntries, sizeof(numSwapEntries));
	swapMapSize = (--numSwapEntries)*sizeof(struct mapent);
	lseek(kmem, RawStats[NSWAPBLKS].n_value,0);
	read(kmem, (char *)&numSwapBlks, sizeof(numSwapBlks));
	lseek(kmem, RawStats[DMMAX].n_value,0);
	read(kmem, (char *)&dmmax, sizeof(dmmax));
	numSwapBlks -= dmmax/2;
	swapMap = (struct mapent *)malloc(swapMapSize);
    }
    sp = (struct mapent *)swapMap;
    lseek(kmem, swapMapAddr, 0);
    read(kmem, (char *)sp, swapMapSize);
    for(stats->TotalVM = stats->ActiveVM = numSwapBlks; sp->m_size; sp++) {
	stats->ActiveVM -= sp->m_size;
    }
    stats->ProcessSize = sbrk(0) >> 10;
*/
}


PRIVATE void PrintVolumeStatus(VolumeStatus *status)
{
    LogMsg(5, SrvDebugLevel, stdout,"Volume header contains:");
    LogMsg(5, SrvDebugLevel, stdout,"Vid = %u, Parent = %u, Online = %d, InService = %d, Blessed = %d, NeedsSalvage = %d",
	    status->Vid, status->ParentId, status->Online, status->InService,
	    status->Blessed, status->NeedsSalvage);
    LogMsg(5, SrvDebugLevel, stdout,"MinQuota = %d, MaxQuota = %d", status->MinQuota, status->MaxQuota);
    LogMsg(5, SrvDebugLevel, stdout,"Type = %d, BlocksInUse = %d, PartBlocksAvail = %d, PartMaxBlocks = %d",
	    status->Type, status->BlocksInUse, status->PartBlocksAvail, status->PartMaxBlocks);
}


/*
  BEGIN_HTML
  <a name="ViceEnableGroup"><strong>Used to enable an authentication group</strong></a>
  END_HTML
*/
long ViceEnableGroup(RPC2_Handle cid, RPC2_String GroupName)
    {
    int gid;
    ClientEntry *client;

    LogMsg(1, SrvDebugLevel, stdout,"ViceEnableGroup GroupName = %s", GroupName);

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
long ViceDisableGroup(RPC2_Handle cid, RPC2_String GroupName)
    {
    int gid;
    ClientEntry *client;

    LogMsg(1, SrvDebugLevel, stdout,"ViceDisableGroup GroupName = %s", GroupName);

    if (AL_NameToId((char *)GroupName, &gid) < 0) return(EINVAL);
    if (RPC2_GetPrivatePointer(cid, (char **)&client) != RPC2_SUCCESS) return (EINVAL);
    if (client == NULL) return(EINVAL);
    if (AL_DisableGroup(gid, client->CPS) < 0) return(EINVAL);
    return(0);
    }


/*
  BEGIN_HTML
  <a name="ViceNewConnectFS"><strong>Called by client after connection setup</strong></a> 
  END_HTML
*/
long ViceNewConnectFS(RPC2_Handle RPCid, RPC2_Unsigned ViceVersion, ViceClient *ClientId)
{
    return(ViceConnectFS(RPCid, ViceVersion, ClientId));
}


/* stubs for obsolete calls */

long ViceUnused1(RPC2_Handle RPCid)
{
    PrintUnusedComplaint(RPCid, ViceUnused1_OP, "ViceStore");
    return(RPC2_INVALIDOPCODE);
}


long ViceUnused2(RPC2_Handle RPCid)
{
    PrintUnusedComplaint(RPCid, ViceUnused2_OP, "ViceUnused1");
    return(RPC2_INVALIDOPCODE);
}


long ViceUnused3(RPC2_Handle RPCid)
{
    PrintUnusedComplaint(RPCid, ViceUnused3_OP, "ViceSetLock");
    return(RPC2_INVALIDOPCODE);
}


long ViceUnused4(RPC2_Handle RPCid)
{
    PrintUnusedComplaint(RPCid, ViceUnused4_OP, "ViceReleaseLock");
    return(RPC2_INVALIDOPCODE);
}


long ViceUnused5(RPC2_Handle RPCid)
{
    PrintUnusedComplaint(RPCid, ViceUnused5_OP, "ViceGetMessage");
    return(RPC2_INVALIDOPCODE);
}


long ViceUnused6(RPC2_Handle RPCid)
{
    PrintUnusedComplaint(RPCid, ViceUnused6_OP, "ViceUnused2");
    return(RPC2_INVALIDOPCODE);
}


long ViceUnused7(RPC2_Handle RPCid)
{
    PrintUnusedComplaint(RPCid, ViceUnused7_OP, "ViceAllocFid");
    return(RPC2_INVALIDOPCODE);
}


long ViceUnused8(RPC2_Handle RPCid)
{
    PrintUnusedComplaint(RPCid, ViceUnused8_OP, "ViceLockVol");
    return(RPC2_INVALIDOPCODE);
}


long ViceUnused9(RPC2_Handle RPCid)
{
    PrintUnusedComplaint(RPCid, ViceUnused9_OP, "ViceUnlockVol");
    return(RPC2_INVALIDOPCODE);
}


long ViceUnused10(RPC2_Handle RPCid)
{
    PrintUnusedComplaint(RPCid, ViceUnused10_OP, "ViceReintegrate");
    return(RPC2_INVALIDOPCODE);
}


long ViceUnused11(RPC2_Handle RPCid)
{
    PrintUnusedComplaint(RPCid, ViceUnused11_OP, "ViceNewReintegrate");
    return(RPC2_INVALIDOPCODE);
}


long ViceUnused12(RPC2_Handle RPCid)
{
    PrintUnusedComplaint(RPCid, ViceUnused12_OP, "ViceGetVolVV");
    return(RPC2_INVALIDOPCODE);
}


long ViceUnused13(RPC2_Handle RPCid)
{
    PrintUnusedComplaint(RPCid, ViceUnused13_OP, "ViceVStore");
    return(RPC2_INVALIDOPCODE);
}


long ViceUnused14(RPC2_Handle RPCid)
{
    PrintUnusedComplaint(RPCid, ViceUnused13_OP, "ViceVReintegrate");
    return(RPC2_INVALIDOPCODE);
}


PRIVATE void PrintUnusedComplaint(RPC2_Handle RPCid, RPC2_Integer Opcode, char *OldName) {
    ClientEntry *client = 0;
    long   errorCode;

    (void) RPC2_GetPrivatePointer(RPCid, (char **)&client);

    LogMsg(0, SrvDebugLevel, stdout, "Obsolete request %d (%s) on cid %d for %s at %s",
	   Opcode, OldName, RPCid, client ? client->UserName:"???",
	   (client && client->VenusId) ? client->VenusId->HostName:"???");
}
