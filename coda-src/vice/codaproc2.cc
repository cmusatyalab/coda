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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/vice/codaproc2.cc,v 4.2 1997/01/28 11:54:35 satya Exp $";
#endif /*_BLURB_*/







/************************************************************************/
/*									*/
/*  codaproc2.c	- Additional File Server Coda specific routines		*/
/*									*/
/*  Function	-							*/
/*									*/
/*									*/
/*									*/
/************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/dir.h>
#include <sys/file.h>
#include <stdarg.h>
#include <sysent.h>
#ifdef __MACH__
#include <libc.h>
#endif
#include <netinet/in.h>
#include <inodefs.h>

#include <rpc2.h>
#include <se.h>

#ifdef __cplusplus
}
#endif __cplusplus



#include <util.h>
#include <rvmlib.h>
#include <coda_dir.h>
#include <srv.h>
#include <coppend.h>
#include <lockqueue.h>
#include <vldb.h>
#include <vrdb.h>
#include <repio.h>
#include <vlist.h>
#include <callback.h>
#include <codaproc.h>
#include <rvmdir.h>
#include <dlist.h>
#include <operations.h>
#include <reslog.h>
#include <resutil.h>
#include <ops.h>
#include <rsle.h>
#include <inconsist.h>

extern void MakeLogNonEmpty(Vnode *);
extern void HandleWeakEquality(Volume *, Vnode *, ViceVersionVector *);

/* From Vol package. */
extern void SetDirHandle(DirHandle *, Vnode *);


/* Yield parameters (i.e., after how many loop iterations do I poll and yield). */
/* N.B.  Yield "periods" MUST all be power of two so that AND'ing can be used! */
const int Yield_RLAlloc_Period = 256;
const int Yield_RLAlloc_Mask = (Yield_RLAlloc_Period - 1);
const int Yield_XlateVid_Period = 256;
const int Yield_XlateVid_Mask = (Yield_XlateVid_Period - 1);
const int Yield_AllocVnode_Period = 8;
const int Yield_AllocVnode_Mask = (Yield_AllocVnode_Period - 1);
const int Yield_GetFids_Period = 32;
const int Yield_GetFids_Mask = (Yield_GetFids_Period - 1);
const int Yield_GetObjects_Period = 8;
const int Yield_GetObjects_Mask = (Yield_GetObjects_Period - 1);
const int Yield_CheckAndPerform_Period = 8;
const int Yield_CheckAndPerform_Mask = (Yield_CheckAndPerform_Period - 1);
const int Yield_RLDealloc_Period = 256;
const int Yield_RLDealloc_Mask = (Yield_RLDealloc_Period - 1);
extern void PollAndYield();


/*  *****  Gross stuff for packing/unpacking RPC arguments  *****  */
#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include "multi.h"
extern void unpack(ARG *, PARM *, PARM **, long);
extern void unpack_struct(ARG *, PARM **, PARM **, long);

#ifdef __cplusplus
}
#endif __cplusplus

PRIVATE void RLE_Unpack(int, int, PARM **, ARG * ...);


/*  *****  Reintegration Log  *****  */

struct rle : public dlink {
    ViceStoreId sid;
    RPC2_Integer opcode;
    Date_t Mtime;
    union {
	struct {
	    ViceFid Fid;
	    ViceStoreType Request;
	    ViceStatus Status;
	    RPC2_Integer Length;
	    RPC2_Integer Mask;
	    ViceFid UntranslatedFid;	    /* in case we need to fetch this object! */
	    RPC2_Integer Inode;		    /* if data is already local */
	} u_store;
	struct {
	    ViceFid Did;
	    RPC2_String Name;
	    RPC2_Byte NameBuf[MAXNAMLEN + 1];
	    ViceStatus Status;
	    ViceFid Fid;
	    ViceStatus DirStatus;
	    RPC2_Unsigned AllocHost;
	} u_create;
	struct {
	    ViceFid Did;
	    RPC2_String Name;
	    RPC2_Byte NameBuf[MAXNAMLEN + 1];
	    ViceStatus DirStatus;
	    ViceStatus Status;
	    ViceFid TgtFid;
	} u_remove;
	struct {
	    ViceFid Did;
	    RPC2_String Name;
	    RPC2_Byte NameBuf[MAXNAMLEN + 1];
	    ViceFid Fid;
	    ViceStatus Status;
	    ViceStatus DirStatus;
	} u_link;
	struct {
	    ViceFid OldDid;
	    RPC2_String OldName;
	    RPC2_Byte OldNameBuf[MAXNAMLEN + 1];
	    ViceFid NewDid;
	    RPC2_String NewName;
	    RPC2_Byte NewNameBuf[MAXNAMLEN + 1];
	    ViceStatus OldDirStatus;
	    ViceStatus NewDirStatus;
	    ViceStatus SrcStatus;
	    ViceStatus TgtStatus;
	    ViceFid SrcFid;
	    ViceFid TgtFid;
	} u_rename;
	struct {
	    ViceFid Did;
	    RPC2_String Name;
	    RPC2_Byte NameBuf[MAXNAMLEN + 1];
	    ViceStatus Status;
	    ViceFid NewDid;
	    ViceStatus DirStatus;
	    RPC2_Unsigned AllocHost;
	} u_mkdir;
	struct {
	    ViceFid Did;
	    RPC2_String Name;
	    RPC2_Byte NameBuf[MAXNAMLEN + 1];
	    ViceStatus Status;
	    ViceStatus TgtStatus;
	    ViceFid TgtFid;
	} u_rmdir;
	struct {
	    ViceFid Did;
	    RPC2_String OldName;
	    RPC2_Byte OldNameBuf[MAXNAMLEN + 1];
	    RPC2_String NewName;
	    RPC2_Byte NewNameBuf[MAXNAMLEN + 1];
	    ViceFid Fid;
	    ViceStatus Status;
	    ViceStatus DirStatus;
	    RPC2_Unsigned AllocHost;
	} u_symlink;
    } u;
};


/*
 *
 *    Reintegration consists of four logical phases:
 *      1. Validating parameters
 *      2. Getting objects (volume, vnodes)
 *      3. Checking semantics of each operation, then performing it
 *      4. Putting objects
 *
 *    It uses two main data structures:
 *      1. A reintegration log, with an entry for each modify operation, in the order executed at the client.
 *      2. A list of vnodes (in Fid order).
 *
 *    Reintegration is currently atomic: either replay of all entries succeeds, or replay of none succeed.
 *
 *    ToDo:
 *      1. Perform routines need OUT parameter for changed-disk-usage (?)
 *      2. Retried reintegrations fail because vnodes allocated during reintegration aren't cleaned up properly
 *         (this should be fixed with the new fid allocation mechanism, separating fid and vnode allocation) (?)
 *
 */

PRIVATE int ValidateReintegrateParms(RPC2_Handle, VolumeId *, Volume **, 
				     ClientEntry **, int, dlist *, RPC2_Integer *,
				     ViceReintHandle *);
PRIVATE int GetReintegrateObjects(ClientEntry *, dlist *, dlist *, int *, 
				  RPC2_Integer *);
PRIVATE int CheckSemanticsAndPerform(ClientEntry *, VolumeId, VolumeId,
				      dlist *, dlist *, int *, RPC2_Integer *);
PRIVATE void PutReintegrateObjects(int, Volume *, dlist *, dlist *, int, ClientEntry *, 
				   RPC2_Integer, RPC2_Integer *, ViceFid *, 
				   RPC2_CountedBS *, RPC2_Integer *, CallBackStatus *);

PRIVATE int AllocReintegrateVnode(Volume **, dlist *, ViceFid *, ViceFid *,
				   ViceDataType, UserId, RPC2_Unsigned, int *);

PRIVATE int AddParent(Volume **, dlist *, ViceFid *);
PRIVATE int ReintNormalVCmp(int, VnodeType, void *, void *);
PRIVATE int ReintNormalVCmpNoRes(int, VnodeType, void *, void *);
PRIVATE void ReintPrelimCOP(vle *, ViceStoreId *, ViceStoreId *, Volume *);
PRIVATE void ReintFinalCOP(vle *, Volume *, RPC2_Integer *);
PRIVATE int ValidateRHandle(VolumeId, int, ViceReintHandle[], ViceReintHandle **);


/*
  BEGIN_HTML
  <a name="ViceVIncReintegrate"><strong>Reintegrate disconnected mutations in an incremental fashion</strong></a> 
  END_HTML
*/
long ViceVIncReintegrate(RPC2_Handle RPCid, VolumeId Vid, RPC2_Integer *Index,
		     RPC2_Integer LogSize, RPC2_CountedBS *OldVS, 
		     RPC2_Integer *NewVS, CallBackStatus *VCBStatus, 
		     RPC2_CountedBS *PiggyBS, SE_Descriptor *BD) {

    return(ViceReintegrate(RPCid, Vid, LogSize, Index, 0, NULL, NULL,
			   OldVS, NewVS, VCBStatus, PiggyBS, BD));
}

/*
  BEGIN_HTML
  <a name="ViceReintegrate"><strong>Reintegrate disconnected mutations</strong></a> 
  END_HTML
*/
long ViceReintegrate(RPC2_Handle RPCid, VolumeId Vid, RPC2_Integer LogSize,
		     RPC2_Integer *Index, RPC2_Integer MaxDirs, 
		     RPC2_Integer *NumDirs, ViceFid StaleDirs[],
		     RPC2_CountedBS *OldVS, RPC2_Integer *NewVS, 
		     CallBackStatus *VCBStatus, 
		     RPC2_CountedBS *PiggyBS, SE_Descriptor *BD) {
START_TIMING(Reintegrate_Total);
    LogMsg(1, SrvDebugLevel, stdout,  "ViceReintegrate: Volume = %x", Vid);

    int errorCode = 0;
    ClientEntry *client = 0;
    VolumeId VSGVolnum = Vid;
    Volume *volptr = 0;
    dlist *rlog = new dlist;
    dlist *vlist = new dlist((CFN)VLECmp);
    int	blocks = 0;

    if (NumDirs) *NumDirs = 0;	/* check for compatibility */

    /* Phase 0. */
    if ((PiggyBS->SeqLen > 0) && (errorCode = ViceCOP2(RPCid, PiggyBS))) {
	if (Index) *Index = -1;
	goto FreeLocks;
    }

    /* Phase I. */
    if (errorCode = ValidateReintegrateParms(RPCid, &Vid, &volptr, &client,
					     LogSize, rlog, Index, 0))
	goto FreeLocks;

    /* Phase II. */
    if (errorCode = GetReintegrateObjects(client, rlog, vlist, &blocks, Index))
	goto FreeLocks;

    /* Phase III. */
    if (errorCode = CheckSemanticsAndPerform(client, Vid, VSGVolnum, rlog, 
					     vlist, &blocks, Index))
	goto FreeLocks;

FreeLocks:
    /* Phase IV. */
    PutReintegrateObjects(errorCode, volptr, rlog, vlist, blocks, client, 
			  MaxDirs, NumDirs, StaleDirs, OldVS, NewVS, VCBStatus);

    LogMsg(2, SrvDebugLevel, stdout,  "ViceReintegrate returns %s", ViceErrorMsg(errorCode));
END_TIMING(Reintegrate_Total);
    return(errorCode);
}


/*
  BEGIN_HTML
  <a name="ViceOpenReintHandle"><strong>get a handle to store new data for
  an upcoming reintegration call.</strong></a> 
  END_HTML
*/
long ViceOpenReintHandle(RPC2_Handle RPCid, ViceFid *Fid, ViceReintHandle *RHandle)
{
    int errorCode = 0;		/* return code for caller */
    Volume *volptr = 0;		/* pointer to the volume header */
    ClientEntry *client = 0;	/* pointer to client structure */
    VolumeId VSGVolnum = Fid->Volume;
    dlist *vlist = new dlist((CFN)VLECmp);
    vle *v;

    LogMsg(0/*1*/, SrvDebugLevel, stdout, "ViceOpenReintHandle: Fid = (%x.%x.%x)",
	     Fid->Volume, Fid->Vnode, Fid->Unique);

    if (errorCode = ValidateParms(RPCid, &client, 1, &Fid->Volume, 0))
	goto FreeLocks;

    v = AddVLE(*vlist, Fid);
    if (errorCode = GetFsObj(Fid, &volptr, &v->vptr, READ_LOCK, NO_LOCK, 0, 0))
	goto FreeLocks;

    /* create a new inode */
    RHandle->BirthTime = (RPC2_Integer) StartTime;
    RHandle->Device = (RPC2_Integer) V_device(volptr);
    RHandle->Inode = icreate((int) V_device(volptr), 0, (int) V_id(volptr), 
		      (int) v->vptr->vnodeNumber, (int) v->vptr->disk.uniquifier, 
		      (int) v->vptr->disk.dataVersion + 1);
    assert(RHandle->Inode > 0);

FreeLocks:
    /* Put objects. */
    PutObjects(errorCode, volptr, NO_LOCK, vlist, 0, 0);
    LogMsg(0/*2*/, SrvDebugLevel, stdout, "ViceOpenReintHandle returns (%d,%d,%d), %s", 
	   RHandle->BirthTime, RHandle->Device, RHandle->Inode,
	   ViceErrorMsg(errorCode));

    return(errorCode);
}


/*
  BEGIN_HTML
  <a name="ViceQueryReintHandle"><strong> Get the status of a partially 
  transferred file for an upcoming reintegration.  Now returns a byte offset, 
  but could be expanded to handle negotiation.</strong></a> 
  END_HTML
*/
long ViceQueryReintHandle(RPC2_Handle RPCid, VolumeId Vid,
			  RPC2_Integer numHandles, ViceReintHandle RHandle[], 
			  RPC2_Unsigned *Length)
{
    int errorCode = 0;
    ClientEntry *client = 0;
    int fd = -1;
    struct stat status;
    ViceReintHandle *myHandle;

    LogMsg(0/*1*/, SrvDebugLevel, stdout, "ViceQueryReintHandle for volume 0x%x", Vid);

    /* Map RPC handle to client structure. */
    if ((errorCode = (int) RPC2_GetPrivatePointer(RPCid, (char **)&client)) != RPC2_SUCCESS) {
	LogMsg(0, SrvDebugLevel, stdout, "ViceQueryReintHandle: GetPrivatePointer failed (%d)", errorCode);
	goto Exit;
    }	

    if (errorCode = ValidateRHandle(Vid, numHandles, RHandle, &myHandle)) 
	goto Exit;

    LogMsg(0/*1*/, SrvDebugLevel, stdout, "ViceQueryReintHandle: Handle = (%d,%d,%d)",
	   myHandle->BirthTime, myHandle->Device, myHandle->Inode);
    
    /* open and stat the inode */
    if ((fd = iopen((int) myHandle->Device, (int) myHandle->Inode, O_RDONLY)) < 0) {
	LogMsg(0, SrvDebugLevel, stdout, "ViceReintQueryHandle: iopen(%d, %d) failed (%d)",
		myHandle->Device, myHandle->Inode, errno);
	errorCode = errno;
	goto Exit;
    }

    if (fstat(fd, &status) < 0) {
	LogMsg(0, SrvDebugLevel, stdout, "ViceReintQueryHandle: fstat(%d, %d) failed (%d)",
		myHandle->Device, myHandle->Inode, errno);
	errorCode = errno;
	goto Exit;
    }

    *Length = (RPC2_Unsigned) status.st_size;

 Exit:
    if (fd != -1) assert(close(fd) == 0);
    LogMsg(0/*2*/, SrvDebugLevel, stdout, "ViceQueryReintHandle returns length %d, %s",
	   status.st_size, ViceErrorMsg(errorCode));

    return(errorCode);
}


/*
  BEGIN_HTML
  <a name="ViceSendReintFragment"><strong> append file data corresponding to the 
  handle for  an upcoming reintegration.</strong></a> 
  END_HTML
*/
long ViceSendReintFragment(RPC2_Handle RPCid, VolumeId Vid,
			   RPC2_Integer numHandles, ViceReintHandle RHandle[], 
			   RPC2_Unsigned Length, SE_Descriptor *BD)
{
    int errorCode = 0;		/* return code for caller */
    ClientEntry *client = 0;	/* pointer to client structure */
    int fd = -1;
    struct stat status;
    SE_Descriptor sid;
    ViceReintHandle *myHandle;

    LogMsg(0/*1*/, SrvDebugLevel, stdout, "ViceSendReintFragment for volume 0x%x", Vid);

    /* Map RPC handle to client structure. */
    if ((errorCode = (int) RPC2_GetPrivatePointer(RPCid, (char **)&client)) != RPC2_SUCCESS) {
	LogMsg(0, SrvDebugLevel, stdout, "ViceSendReintFragment: GetPrivatePointer failed (%d)", errorCode);
	goto Exit;
    }	

    if (errorCode = ValidateRHandle(Vid, numHandles, RHandle, &myHandle)) 
	goto Exit;

    LogMsg(0/*1*/, SrvDebugLevel, stdout, "ViceSendReintFragment: Handle = (%d,%d,%d), Length = %d",
	     myHandle->BirthTime, myHandle->Device, myHandle->Inode, Length);

    /* open and stat the inode */
    if ((fd = iopen((int) myHandle->Device, (int) myHandle->Inode, O_RDWR)) < 0) {
	LogMsg(0, SrvDebugLevel, stdout, "ViceSendReintFragment: iopen(%d, %d) failed (%d)",
		myHandle->Device, myHandle->Inode, errno);
	errorCode = errno;
	goto Exit;
    }

    if (fstat(fd, &status) < 0) {
	LogMsg(0, SrvDebugLevel, stdout, "ViceSendReintFragment: fstat(%d, %d) failed (%d)",
		myHandle->Device, myHandle->Inode, errno);
	errorCode = errno;
	goto Exit;
    }

    /* transfer and append the data */
    sid.Tag = client->SEType;
    sid.Value.SmartFTPD.TransmissionDirection = CLIENTTOSERVER;
    sid.Value.SmartFTPD.SeekOffset = status.st_size;	
    sid.Value.SmartFTPD.hashmark = (SrvDebugLevel > 2 ? '#' : '\0');
    sid.Value.SmartFTPD.Tag = FILEBYINODE;
    sid.Value.SmartFTPD.ByteQuota = Length;
    sid.Value.SmartFTPD.FileInfo.ByInode.Device = myHandle->Device;
    sid.Value.SmartFTPD.FileInfo.ByInode.Inode = myHandle->Inode;

    if((errorCode = (int) RPC2_InitSideEffect(RPCid, &sid)) <= RPC2_ELIMIT) {
	LogMsg(0, SrvDebugLevel, stdout, "ViceSendReintFragment: InitSE failed (%d), (%d,%d,%d)",
	       errorCode, myHandle->BirthTime, myHandle->Device, myHandle->Inode);

	goto Exit;
    }

    if ((errorCode = (int) RPC2_CheckSideEffect(RPCid, &sid, SE_AWAITLOCALSTATUS)) <= RPC2_ELIMIT) {
	LogMsg(0, SrvDebugLevel, stdout, "ViceSendReintFragment: CheckSE failed (%d), (%d,%d,%d)",
	       errorCode, myHandle->BirthTime, myHandle->Device, myHandle->Inode);

	if (errorCode == RPC2_SEFAIL1) errorCode = EIO;

	/* restore original state */
	assert(ftruncate(fd, status.st_size) == 0);
	goto Exit;
    }

    if (sid.Value.SmartFTPD.BytesTransferred != Length) {
	LogMsg(0, SrvDebugLevel, stdout, "ViceSendReintFragment: length discrepancy (%d : %d), (%d,%d,%d), %s %s.%d",
	       Length, sid.Value.SmartFTPD.BytesTransferred, 
	       myHandle->BirthTime, myHandle->Device, myHandle->Inode,
	       client->UserName, client->VenusId->HostName, client->VenusId->port);
	errorCode = EINVAL;

	/* restore original state */
	assert(ftruncate(fd, status.st_size) == 0);
	goto Exit;
    }

 Exit:
    if (fd != -1) assert(close(fd) == 0);

    LogMsg(0/*2*/, SrvDebugLevel, stdout, "ViceSendReintFragment returns %s", ViceErrorMsg(errorCode));

    return(errorCode);
}

	
/*
 * ViceCloseReintHandle --  */
/*
  BEGIN_HTML
  <a name="ViceCloseReintHandle"><strong> Reintegrate data corresponding 
  to the reintegration handle.  This corresponds to the reintegration of
  a single store record.</strong></a> 
  END_HTML
*/
long ViceCloseReintHandle(RPC2_Handle RPCid, VolumeId Vid, RPC2_Integer LogSize, 
			  RPC2_Integer numHandles, ViceReintHandle RHandle[], 
			  RPC2_CountedBS *OldVS, RPC2_Integer *NewVS, 
			  CallBackStatus *VCBStatus,
			  RPC2_CountedBS *PiggyBS, SE_Descriptor *BD)
{
    int errorCode = 0;
    ClientEntry *client = 0;
    VolumeId VSGVolnum = Vid;
    Volume *volptr = 0;
    dlist *rlog = new dlist;
    dlist *vlist = new dlist((CFN)VLECmp);
    int	blocks = 0;
    ViceReintHandle *myHandle = 0;

    LogMsg(0/*1*/, SrvDebugLevel, stdout, "ViceCloseReintHandle for volume 0x%x", Vid);

    /* Phase 0. */
    if ((PiggyBS->SeqLen > 0) && (errorCode = ViceCOP2(RPCid, PiggyBS))) 
	goto FreeLocks;

    if (errorCode = ValidateRHandle(Vid, numHandles, RHandle, &myHandle))
	goto FreeLocks;

    /* Phase I. */
    if (errorCode = ValidateReintegrateParms(RPCid, &Vid, &volptr, &client,
					     LogSize, rlog, 0, myHandle))
	goto FreeLocks;

    /* Phase II. */
    if (errorCode = GetReintegrateObjects(client, rlog, vlist, &blocks, 0))
	goto FreeLocks;

    /* Phase III. */
    if (errorCode = CheckSemanticsAndPerform(client, Vid, VSGVolnum, rlog, 
					     vlist, &blocks, 0))
	goto FreeLocks;

 FreeLocks:
    /* Phase IV. */
    PutReintegrateObjects(errorCode, volptr, rlog, vlist, blocks, client,
			  0, NULL, NULL, OldVS, NewVS, VCBStatus);

 Exit:
    LogMsg(0/*2*/, SrvDebugLevel, stdout, "ViceCloseReintHandle returns %s", ViceErrorMsg(errorCode));

    return(errorCode);
}



/*
 *
 *    Phase I consists of the following steps:
 *      1. Translating the volume id from logical to physical
 *      2. Looking up the client entry
 *      3. Fetching over the client's representation of the reintegrate log
 *      4. Parsing the client log into a server version (the RL)
 *      5. Translating the volume ids in all the RL entries from logical to physical
 *      6. Acquiring the volume in exclusive mode
 *
 */
PRIVATE int ValidateReintegrateParms(RPC2_Handle RPCid, VolumeId *Vid,
				     Volume **volptr, ClientEntry **client,
				     int rlen, dlist *rlog, RPC2_Integer *Index,
				     ViceReintHandle *RHandle) {
START_TIMING(Reintegrate_ValidateParms);
    LogMsg(10, SrvDebugLevel, stdout,  "ValidateReintegrateParms: RPCid = %d, *Vid = %x", RPCid, *Vid);

    int errorCode = 0;
    *volptr = 0;
    char *rfile = 0;
    PARM *_ptr = 0;
    int index;

    /* Translate the volume. */
    VolumeId VSGVolnum = *Vid;
    int count, ix;
    if (!XlateVid(Vid, &count, &ix)) {
	LogMsg(0, SrvDebugLevel, stdout,  "ValidateReintegrateParms: failed to translate VSG %x", VSGVolnum);
	errorCode = EINVAL;
	index = -1;
	goto Exit;
    }
    LogMsg(2, SrvDebugLevel, stdout,  "ValidateReintegrateParms: %x --> %x", VSGVolnum, *Vid);

    /* Get the client entry. */
    if((errorCode = RPC2_GetPrivatePointer(RPCid, (char **)client)) != RPC2_SUCCESS) {
	LogMsg(0, SrvDebugLevel, stdout,  "ValidateReintegrateParms: no private pointer for RPCid %x", RPCid);
	index = -1;
	goto Exit;
    }
    if(!(*client) || (*client)->DoUnbind) {
	LogMsg(0, SrvDebugLevel, stdout,  "ValidateReintegrateParms: NULL private pointer for RPCid %x", RPCid);
	errorCode = EINVAL;
	index = -1;
	goto Exit;
    }
    LogMsg(2, SrvDebugLevel, stdout,  "ValidateReintegrateParms: %s %s.%d",
	     (*client)->UserName, (*client)->VenusId->HostName, (*client)->VenusId->port);


    /* Fetch over the client's reintegrate log, and read it into memory. */
    {
	assert((rfile = new char[rlen]) != 0);

	SE_Descriptor sid;
	sid.Tag = SMARTFTP;
	sid.Value.SmartFTPD.TransmissionDirection = CLIENTTOSERVER;
	sid.Value.SmartFTPD.SeekOffset = 0;
	sid.Value.SmartFTPD.hashmark = (SrvDebugLevel > 2 ? '#' : '\0');
	sid.Value.SmartFTPD.ByteQuota = -1;
	sid.Value.SmartFTPD.Tag = FILEINVM;
	sid.Value.SmartFTPD.FileInfo.ByAddr.vmfile.MaxSeqLen = rlen;
	sid.Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqLen = 0;
	sid.Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqBody = (RPC2_ByteSeq)rfile;

	if((errorCode = RPC2_InitSideEffect(RPCid, &sid)) <= RPC2_ELIMIT) {
	    LogMsg(0, SrvDebugLevel, stdout,  "ValidateReintegrateParms: Init_SE failed (%d)", errorCode);
	    index = -1;
	    goto Exit;
	}

	if ((errorCode = RPC2_CheckSideEffect(RPCid, &sid, SE_AWAITLOCALSTATUS)) <= RPC2_ELIMIT) {
	    LogMsg(0, SrvDebugLevel, stdout,  "ValidateReintegrateParms: Check_SE failed (%d)", errorCode);
	    if (errorCode == RPC2_SEFAIL1) errorCode = EIO;
	    index = -1;
	    goto Exit;
	}

	LogMsg(1, SrvDebugLevel, stdout,  "Reintegrate transferred %d bytes.",
		sid.Value.SmartFTPD.BytesTransferred);
    }

    /* Allocate/unpack entries and append them to the RL. */
    for (_ptr = (PARM *)rfile, index = 0; (char *)_ptr - rfile < rlen; index++) {
	RPC2_CountedBS DummyCBS;
	DummyCBS.SeqLen = 0;
	DummyCBS.SeqBody = 0;
	RPC2_Unsigned DummyPH;

	rle *r = new rle;
	r->opcode = ntohl(*((RPC2_Integer *)_ptr++));
	r->Mtime = ntohl(*((Date_t *)_ptr++));
	LogMsg(100, SrvDebugLevel, stdout,  "ValidateReintegrateParms: [B] Op = %d, Mtime = %d",
		r->opcode, r->Mtime);
	switch(r->opcode) {
	    case ViceNewStore_OP:
		RLE_Unpack(0, 0, &_ptr, ViceNewStore_PTR, &r->u.u_store.Fid,
			   &r->u.u_store.Request, &DummyCBS,
			   &r->u.u_store.Status, &r->u.u_store.Length,
			   &r->u.u_store.Mask, &DummyPH, 
			   &r->sid, &DummyCBS, 0);
		r->u.u_store.UntranslatedFid = r->u.u_store.Fid;
		r->u.u_store.Inode = 0;
		switch(r->u.u_store.Request) {
		    case StoreData:
		    case StoreStatusData:
		        if (RHandle) 
			    r->u.u_store.Inode = RHandle->Inode;
			break;

		    case StoreStatus:
			break;

		    case StoreNeither:
		    default:
			LogMsg(0, SrvDebugLevel, stdout,  "ValidateReintegrateParms: bogus store request (%d)",
				r->u.u_store.Request);
			errorCode = EINVAL;
			goto Exit;
		}
		break;

	    case ViceCreate_OP:
		{
		ViceFid DummyFid;
		r->u.u_create.Name = r->u.u_create.NameBuf;
		RLE_Unpack(0, 0, &_ptr, ViceCreate_PTR, &r->u.u_create.Did,
			   &DummyFid, r->u.u_create.Name,
			   &r->u.u_create.Status, &r->u.u_create.Fid,
			   &r->u.u_create.DirStatus, &r->u.u_create.AllocHost,
			   &r->sid, &DummyCBS);
		}
		break;

	    case ViceRemove_OP:
		r->u.u_remove.Name = r->u.u_remove.NameBuf;
		RLE_Unpack(0, 0, &_ptr, ViceRemove_PTR, &r->u.u_remove.Did,
			   r->u.u_remove.Name, &r->u.u_remove.DirStatus,
			   &r->u.u_remove.Status, &DummyPH, &r->sid, &DummyCBS);
		break;

	    case ViceLink_OP:
		r->u.u_link.Name = r->u.u_link.NameBuf;
		RLE_Unpack(0, 0, &_ptr, ViceLink_PTR, &r->u.u_link.Did,
			   r->u.u_link.Name, &r->u.u_link.Fid,
			   &r->u.u_link.Status, &r->u.u_link.DirStatus,
			   &DummyPH, &r->sid, &DummyCBS);
		break;

	    case ViceRename_OP:
		r->u.u_rename.OldName = r->u.u_rename.OldNameBuf;
		r->u.u_rename.NewName = r->u.u_rename.NewNameBuf;
		RLE_Unpack(0, 0, &_ptr, ViceRename_PTR, &r->u.u_rename.OldDid,
			   r->u.u_rename.OldName, &r->u.u_rename.NewDid,
			   r->u.u_rename.NewName, &r->u.u_rename.OldDirStatus,
			   &r->u.u_rename.NewDirStatus, &r->u.u_rename.SrcStatus,
			   &r->u.u_rename.TgtStatus, &DummyPH, &r->sid, &DummyCBS);
		break;

	    case ViceMakeDir_OP:
		r->u.u_mkdir.Name = r->u.u_mkdir.NameBuf;
		RLE_Unpack(0, 0, &_ptr, ViceMakeDir_PTR, &r->u.u_mkdir.Did,
			   r->u.u_mkdir.Name, &r->u.u_mkdir.Status,
			   &r->u.u_mkdir.NewDid, &r->u.u_mkdir.DirStatus,
			   &r->u.u_mkdir.AllocHost, &r->sid, &DummyCBS);
		break;

	    case ViceRemoveDir_OP:
		r->u.u_rmdir.Name = r->u.u_rmdir.NameBuf;
		RLE_Unpack(0, 0, &_ptr, ViceRemoveDir_PTR, &r->u.u_rmdir.Did,
			   r->u.u_rmdir.Name, &r->u.u_rmdir.Status,
			   &r->u.u_rmdir.TgtStatus, &DummyPH, &r->sid, &DummyCBS);
		break;

	    case ViceSymLink_OP:
		r->u.u_symlink.NewName = r->u.u_symlink.NewNameBuf;
		r->u.u_symlink.OldName = r->u.u_symlink.OldNameBuf;
		RLE_Unpack(0, 0, &_ptr, ViceSymLink_PTR, &r->u.u_symlink.Did,
			   r->u.u_symlink.NewName, r->u.u_symlink.OldName,
			   &r->u.u_symlink.Fid, &r->u.u_symlink.Status,
			   &r->u.u_symlink.DirStatus, &r->u.u_symlink.AllocHost,
			   &r->sid, &DummyCBS);
		break;

	    default:
		LogMsg(0, SrvDebugLevel, stdout,  "ValidateReintegrateParms: bogus opcode (%d)", r->opcode);
		errorCode = EINVAL;
		goto Exit;
	}

	LogMsg(100, SrvDebugLevel, stdout,  "ValidateReintegrateParms: [E] Op = %d, Mtime = %d",
		r->opcode, r->Mtime);
	rlog->append(r);

	/* Yield after every so many records. */
	if ((rlog->count() & Yield_RLAlloc_Mask) == 0)
	    PollAndYield();
    }
    if (rlog->count() < Yield_RLAlloc_Period - 1)
	PollAndYield();
    LogMsg(2, SrvDebugLevel, stdout,  "ValidateReintegrateParms: rlog count = %d", rlog->count());

    /* Translate the Vid for each Fid. */
    {
	dlist_iterator next(*rlog);
	rle *r;
	int count = 0;
	index = 0;
	while (r = (rle *)next()) {
	    switch(r->opcode) {
		case ViceNewStore_OP:
		    if (!XlateVid(&r->u.u_store.Fid.Volume) ||
			r->u.u_store.Fid.Volume != *Vid) {
			errorCode = EINVAL;
			goto Exit;
		    }
		    break;

		case ViceCreate_OP:
		    if (!XlateVid(&r->u.u_create.Did.Volume) ||
			r->u.u_create.Did.Volume != *Vid) {
			errorCode = EINVAL;
			goto Exit;
		    }
		    if (!XlateVid(&r->u.u_create.Fid.Volume) ||
			r->u.u_create.Fid.Volume != *Vid) {
			errorCode = EINVAL;
			goto Exit;
		    }
		    break;

		case ViceRemove_OP:
		    if (!XlateVid(&r->u.u_remove.Did.Volume) ||
			r->u.u_remove.Did.Volume != *Vid) {
			errorCode = EINVAL;
			goto Exit;
		    }
		    break;

		case ViceLink_OP:
		    if (!XlateVid(&r->u.u_link.Did.Volume) ||
			r->u.u_link.Did.Volume != *Vid) {
			errorCode = EINVAL;
			goto Exit;
		    }
		    if (!XlateVid(&r->u.u_link.Fid.Volume) ||
			r->u.u_link.Fid.Volume != *Vid) {
			errorCode = EINVAL;
			goto Exit;
		    }
		    break;

		case ViceRename_OP:
		    if (!XlateVid(&r->u.u_rename.OldDid.Volume) ||
			r->u.u_rename.OldDid.Volume != *Vid) {
			errorCode = EINVAL;
			goto Exit;
		    }
		    if (!XlateVid(&r->u.u_rename.NewDid.Volume) ||
			r->u.u_rename.NewDid.Volume != *Vid) {
			errorCode = EINVAL;
			goto Exit;
		    }
		    break;

		case ViceMakeDir_OP:
		    if (!XlateVid(&r->u.u_mkdir.Did.Volume) ||
			r->u.u_mkdir.Did.Volume != *Vid) {
			errorCode = EINVAL;
			goto Exit;
		    }
		    if (!XlateVid(&r->u.u_mkdir.NewDid.Volume) ||
			r->u.u_mkdir.NewDid.Volume != *Vid) {
			errorCode = EINVAL;
			goto Exit;
		    }
		    break;

		case ViceRemoveDir_OP:
		    if (!XlateVid(&r->u.u_rmdir.Did.Volume) ||
			r->u.u_rmdir.Did.Volume != *Vid) {
			errorCode = EINVAL;
			goto Exit;
		    }
		    break;

		case ViceSymLink_OP:
		    if (!XlateVid(&r->u.u_symlink.Did.Volume) ||
			r->u.u_symlink.Did.Volume != *Vid) {
			errorCode = EINVAL;
			goto Exit;
		    }
		    if (!XlateVid(&r->u.u_symlink.Fid.Volume) ||
			r->u.u_symlink.Fid.Volume != *Vid) {
			errorCode = EINVAL;
			goto Exit;
		    }
		    break;

		default:
		    assert(FALSE);
	    }

	    /* Yield after every so many records. */
	    count++;
	    index++;
	    if ((count & Yield_XlateVid_Mask) == 0)
		PollAndYield();
	}
	if (count < Yield_XlateVid_Period - 1)
	    PollAndYield();
    }

    /* Acquire the volume in exclusive mode. */
    {
	if (errorCode = GetVolObj(*Vid, volptr, VOL_EXCL_LOCK, 
				  0, ThisHostAddr)) {
	    index = -1;
	    goto Exit;
	}
    }

    /* 
     * Check that the first record has not been reintegrated before.
     * If it has, return VLOGSTALE and the identifier of the last
     * successfully reintegrated record.  The identifier is the 
     * uniquifier field from the storeid of the record.  Return it
     * in the index variable; saves a parameter for this special case.
     */
    {
	rle *r;
	r = (rle *)rlog->first();
	int i = 0;

	while (i < (*volptr)->nReintegrators) {
	    if ((r->sid.Host == (*volptr)->reintegrators[i].Host) &&
		(r->sid.Uniquifier <= (*volptr)->reintegrators[i].Uniquifier)) {
		errorCode = VLOGSTALE;
		index = (*volptr)->reintegrators[i].Uniquifier;
		goto Exit;
	    }
	    i++;
	}
    }

    /* if there is a reintegration handle, sanity check */
    if (RHandle) {
	LogMsg(0/*1*/, SrvDebugLevel, stdout, "ValidateReintegrateParms: Handle = (%d,%d,%d)",
		 RHandle->BirthTime, RHandle->Device, RHandle->Inode);

	/* 
	 * Currently, if an RHandle is supplied, the log must consist of only one
	 * new store record.  (The store record is sent only by old Venii.)
	 * Verify that is the case. 
	 */      
        {
	    assert(rlog->count() == 1);

	    rle *r;
	    r = (rle *)rlog->first();
	    assert(r->opcode == ViceNewStore_OP &&
		   (r->u.u_store.Request == StoreData || 
		    r->u.u_store.Request == StoreStatusData));
	}
    }

Exit:
    if (rfile) delete rfile;
    if (Index) *Index = (RPC2_Integer) index;
    LogMsg(10, SrvDebugLevel, stdout,  "ValidateReintegrateParms: returning %s", ViceErrorMsg(errorCode));
END_TIMING(Reintegrate_ValidateParms);
    return(errorCode);
}

/*
 *
 *    Phase II consists of the following steps:
 *      1. Allocating vnodes for "new" objects
 *      2. Parsing the RL entries to create an ordered data structure of "participant" Fids
 *      3. Acquiring all corresponding vnodes in Fid-order, and under write-locks
 *
 */
PRIVATE int GetReintegrateObjects(ClientEntry *client, dlist *rlog, dlist *vlist, 
				  int *blocks, RPC2_Integer *Index) {
START_TIMING(Reintegrate_GetObjects);
    LogMsg(10, SrvDebugLevel, stdout, 	"GetReintegrateObjects: client = %s", client->UserName);

    int errorCode = 0;
    Volume *volptr = 0;
    int index;

    /* Allocate Vnodes for objects created by/during the reintegration. */
    /* N.B.  Entries representing these vnodes go on the vlist BEFORE those representing vnodes */
    /* which are not created as part of the reintegration.  This is needed so that the "lookup" child */
    /* and parent routines can determine when an unsuccessful lookup is OK. */
    {
	dlist_iterator next(*rlog);
	rle *r;
	int count = 0;
        index = 0;
	while (r = (rle *)next()) {
	    switch(r->opcode) {
	        case ViceNewStore_OP:
		case ViceRemove_OP:
		case ViceLink_OP:
		case ViceRename_OP:
		case ViceRemoveDir_OP:
		    continue;

		case ViceCreate_OP:
		    {
		    int tblocks = 0;
		    if (errorCode = AllocReintegrateVnode(&volptr, vlist,
							  &r->u.u_create.Did,
							  &r->u.u_create.Fid,
							  File, client->Id,
							  r->u.u_create.AllocHost,
							  &tblocks))
			goto Exit;
		    *blocks += tblocks;
		    }
		    break;

		case ViceMakeDir_OP:
		    {
		    int tblocks = 0;
		    if (errorCode = AllocReintegrateVnode(&volptr, vlist,
							  &r->u.u_mkdir.Did,
							  &r->u.u_mkdir.NewDid,
							  Directory, client->Id,
							  r->u.u_mkdir.AllocHost,
							  &tblocks))
			goto Exit;
		    *blocks += tblocks;
		    }
		    break;

		case ViceSymLink_OP:
		    {
		    int tblocks = 0;
		    if (errorCode = AllocReintegrateVnode(&volptr, vlist,
							  &r->u.u_symlink.Did,
							  &r->u.u_symlink.Fid,
							  SymbolicLink, client->Id,
							  r->u.u_symlink.AllocHost,
							  &tblocks))
			goto Exit;
		    *blocks += tblocks;
		    }
		    break;

		default:
		    assert(FALSE);
	    }

	    /* Yield after every so many records. */
	    count++;
            index++;
	    if ((count & Yield_AllocVnode_Mask) == 0)
		PollAndYield();
	}
	if (count < Yield_AllocVnode_Period - 1)
	    PollAndYield();
    }

    /* Parse the RL entries, creating an ordered data structure of Fids. */
    /* N.B.  The targets of {unlink,rmdir,rename} are specified by <pfid,name> rather than fid, */
    /* so a lookup in the parent must be done to get the target fid.  Some notes re: lookup: */
    /*   1. If the target object is one that was created by an earlier reintegration op, the lookup will fail. */
    /*      This means that failed lookup here should not be fatal (but it will be when we do it again). */
    /*   2. If the parent was itself created during the reintegration, then lookup is presently illegal as */
    /*      the parent's directory pages (and entries) do not yet exist.  Lookup must *not* be attempted until */
    /*      later in this case (which does not create deadlock problems because the new object is not yet */
    /*      visible to any other call). */
    /*   3. If a name is inserted, deleted, and re-inserted in the course of reintegration, the binding */
    /*      of name to object will have changed.  Thus, we must ALWAYS look up again in CheckSemantics. */
    {
	dlist_iterator next(*rlog);
	rle *r;
	int count = 0;
	index = 0;
	while (r = (rle *)next()) {
	    switch(r->opcode) {
		case ViceNewStore_OP:
		    {
		    vle *v = AddVLE(*vlist, &r->u.u_store.Fid);

		    /* Add file's parent Fid to list for ACL purposes. */
		    /* (Parent MUST already be on list if child was just alloc'ed!) */
		    if (v->vptr == 0 && !(ISDIR(r->u.u_store.Fid)))
			if (errorCode = AddParent(&volptr, vlist, &r->u.u_store.Fid)) {
			    goto Exit;
			}
		    }
		    break;

		case ViceCreate_OP:
		    {
		    vle *v = AddVLE(*vlist, &r->u.u_create.Did);
		    v->d_reintupdate = 1;
		    }
		    break;

		case ViceRemove_OP:
		    {
		    vle *p_v = AddVLE(*vlist, &r->u.u_remove.Did);

		    /* Add the child object's fid to the vlist (if it presently exists). */
		    if (p_v->vptr == 0)
			if (errorCode = AddChild(&volptr, vlist,
						 &r->u.u_remove.Did, (char *)r->u.u_remove.Name, 0))
			    goto Exit;

		    p_v->d_reintupdate = 1;
		    }
		    break;

		case ViceLink_OP:
		    {
		    vle *v = AddVLE(*vlist, &r->u.u_link.Did);
		    v->d_reintupdate = 1;
		    (void)AddVLE(*vlist, &r->u.u_link.Fid);
		    }
		    break;

		case ViceRename_OP:
		    {
		    vle *sp_v = AddVLE(*vlist, &r->u.u_rename.OldDid);

		    /* Add the source object's fid to the vlist (if it presently exists). */
		    if (sp_v->vptr == 0) 
			if (errorCode = AddChild(&volptr, vlist,
						 &r->u.u_rename.OldDid, (char *)r->u.u_rename.OldName, 0))
			    goto Exit;

		    vle *tp_v = AddVLE(*vlist, &r->u.u_rename.NewDid);

		    /* Add the target object's fid to the vlist (if it presently exists). */
		    if (tp_v->vptr == 0)
			if (errorCode = AddChild(&volptr, vlist,
						 &r->u.u_rename.NewDid, (char *)r->u.u_rename.NewName, 0))
			    goto Exit;

		    sp_v->d_reintupdate = 1;
		    tp_v->d_reintupdate = 1;
		    }
		    break;

		case ViceMakeDir_OP:
		    {
		    vle *v = AddVLE(*vlist, &r->u.u_mkdir.Did);
		    v->d_reintupdate = 1;
		    }
		    break;

		case ViceRemoveDir_OP:
		    {
		    vle *p_v = AddVLE(*vlist, &r->u.u_rmdir.Did);

		    /* Add the child object's fid to the vlist (if it presently exists). */
		    if (p_v->vptr == 0)
			if (errorCode = AddChild(&volptr, vlist,
						 &r->u.u_rmdir.Did, (char *)r->u.u_rmdir.Name, 0))
			    goto Exit;

		    p_v->d_reintupdate = 1;
		    }
		    break;

		case ViceSymLink_OP:
		    {
		    vle *v = AddVLE(*vlist, &r->u.u_symlink.Did);
		    v->d_reintupdate = 1;
		    }
		    break;

		default:
		    assert(FALSE);
	    }

	    /* Yield after every so many records. */
	    count++;
            index++;
	    if ((count & Yield_GetFids_Mask) == 0)
		PollAndYield();
	}
	if (count < Yield_GetFids_Period - 1)
	    PollAndYield();
    }
    LogMsg(2, SrvDebugLevel, stdout,  "GetReintegrateObjects: vlist count = %d", vlist->count());

    /* Reacquire all the objects (except those just alloc'ed), this time in FID-order and under write locks. */
    {
	dlist_iterator next(*vlist);
	vle *v;
	int count = 0;
	while (v = (vle *)next()) {
	    if (v->vptr != 0) continue;

	    LogMsg(10, SrvDebugLevel, stdout,  "GetReintegrateObjects: acquiring (%x.%x.%x)",
		    v->fid.Volume, v->fid.Vnode, v->fid.Unique);
	    if (errorCode = GetFsObj(&v->fid, &volptr, &v->vptr, WRITE_LOCK, VOL_NO_LOCK, 0, 0)) {
                index = -1;
		goto Exit;
	    }

	    /* Yield after every so many records. */
	    count++;
	    if ((count & Yield_GetObjects_Mask) == 0)
		PollAndYield();
	}
	if (count < Yield_GetObjects_Period - 1)
	    PollAndYield();
    }

Exit:
    if (Index) *Index = (RPC2_Integer) index;
    PutVolObj(&volptr, VOL_NO_LOCK);
    LogMsg(10, SrvDebugLevel, stdout,  "GetReintegrateObjects:	returning %s", ViceErrorMsg(errorCode));
END_TIMING(Reintegrate_GetObjects);
    return(errorCode);
}


/*
 *
 *    Phase III consists of the following steps:
 *      1. Check the semantics of each operation, then perform it (delay bulk transfers)
 *      2. Do the bulk transfers
 *
 */
PRIVATE int CheckSemanticsAndPerform(ClientEntry *client, VolumeId Vid, VolumeId VSGVolnum,
				        dlist *rlog, dlist *vlist, int *blocks, RPC2_Integer *Index) {
START_TIMING(Reintegrate_CheckSemanticsAndPerform);
    LogMsg(10, SrvDebugLevel, stdout, 	"CheckSemanticsAndPerform: Vid = %x, client = %s",
	     Vid, client->UserName);

    int errorCode = 0;
    Volume *volptr = 0;
    VCP NormalVCmp = (VCP)0;
    int index;

    /* Get a no-lock reference to the volume for use in this routine (only). */
    if (errorCode = GetVolObj(Vid, &volptr, VOL_NO_LOCK)) {
        index = -1;
	goto Exit;
    }

    /* NormalVCmp routine depends on resolution! */
    if (AllowResolution && (V_VMResOn(volptr) || V_RVMResOn(volptr)))
	NormalVCmp = ReintNormalVCmp;
    else
	NormalVCmp = ReintNormalVCmpNoRes;

    /* Check each operation and perform it. */
    /* Note: the data transfer part of stores is delayed until all other operations have completed. */
    {
	dlist_iterator next(*rlog);
	rle *r;
	int count = 0;
        index = 0;
	while (r = (rle *)next()) {
	    switch(r->opcode) {
		case ViceNewStore_OP:
		    {
		    vle *v = FindVLE(*vlist, &r->u.u_store.Fid);
		    vle	*a_v = 0;	/* ACL object */
		    if (v->vptr->disk.type == vDirectory)
			a_v = v;
		    else {
			ViceFid pFid;
			pFid.Volume = v->fid.Volume;
			pFid.Vnode = v->vptr->disk.vparent;
			pFid.Unique = v->vptr->disk.uparent;
			assert((a_v = FindVLE(*vlist, &pFid)) != 0);
		    }
		    int deltablocks = nBlocks(r->u.u_store.Length) - 
		      nBlocks(v->vptr->disk.length);
		    switch(r->u.u_store.Request) {
			case StoreData:
			case StoreStatusData:
			    /* Check. */
			    if (errorCode = CheckStoreSemantics(client, &a_v->vptr,
								&v->vptr, &volptr, 1,
								NormalVCmp,
								&r->u.u_store.Status.VV,
								r->u.u_store.Status.DataVersion,
								0, 0)) {
				goto Exit;
			    }
			    /* Perform. */
			    if (v->f_finode == 0) {
				/* First StoreData; record pre-reintegration inode. */
				v->f_sinode = v->vptr->disk.inodeNumber;
			    }
			    else {
				/* Nth StoreData; discard previous inode. */
				assert(idec(V_device(volptr), v->f_finode,
					    V_parentId(volptr)) == 0);
			    }

			    if (r->u.u_store.Inode) {
				/* inode already allocated, use it. */
				v->f_finode = r->u.u_store.Inode;
			    } else {
				v->f_finode = icreate(V_device(volptr), 0, V_id(volptr),
						      v->vptr->vnodeNumber, v->vptr->disk.uniquifier,
						      v->vptr->disk.dataVersion + 1);
			    }
			    assert(v->f_finode > 0);
			    /* Bulk transfer is deferred until all ops have been checked/performed. */
			    HandleWeakEquality(volptr, v->vptr, &r->u.u_store.Status.VV);
			    PerformStore(client, VSGVolnum, volptr, v->vptr, v->f_finode,
					 0, r->u.u_store.Length, r->Mtime, &r->sid);
			    ReintPrelimCOP(v, &r->u.u_store.Status.VV.StoreId,
					   &r->sid, volptr);

			    /* Cancel previous StoreData. */
			    v->f_sid = r->sid;

			    /* Cancel previous truncate. */
			    LogMsg(3, SrvDebugLevel, stdout,  "CheckSemanticsAndPerform: cancelling truncate (%x.%x.%x, %d, %d)",
				    v->fid.Volume, v->fid.Vnode, v->fid.Unique,
				    v->f_tinode, v->f_tlength);
			    v->f_tinode = 0;
			    v->f_tlength = 0;
			    break;

			case StoreStatus:
			    {
			    int truncp = 0;

			    /* XXX */
			    /* We don't want to mistakenly assume there is a truncate on a directory */
			    /* just because the client doesn't know the length of the replica at the server! */
			    /* This wouldn't be an issue if our interface provided a "don't set" value for */
			    /* each attribute, as it should!  -JJK */
			    if (v->vptr->disk.type != vFile)
				r->u.u_store.Status.Length = v->vptr->disk.length;

			    /* Check. */
			    if (errorCode = CheckNewSetAttrSemantics(client, &a_v->vptr,
								     &v->vptr, &volptr, 1,
								     NormalVCmp,
								     r->u.u_store.Status.Length,
								     r->u.u_store.Status.Date,
								     r->u.u_store.Status.Owner,
								     r->u.u_store.Status.Mode,
								     r->u.u_store.Mask,
								     &r->u.u_store.Status.VV,
								     r->u.u_store.Status.DataVersion,
								     0, 0)) {
				goto Exit;
			    }
			    // Make sure log is non-empty
			    if (v->vptr->disk.type == vDirectory &&
				AllowResolution && V_VMResOn(volptr)) 
				MakeLogNonEmpty(v->vptr);

			    /* Perform. */
			    // if (r->u.u_store.Status.Length != v->vptr->disk.length)
			    // 	   truncp = 1;
			    if (r->u.u_store.Mask & SET_LENGTH)
			        truncp = 1;
			    Inode c_inode = 0;
			    HandleWeakEquality(volptr, v->vptr, &r->u.u_store.Status.VV);
			    PerformNewSetAttr(client, VSGVolnum, volptr, v->vptr,
					      0, r->u.u_store.Status.Length, r->Mtime,
					      r->u.u_store.Status.Owner, r->u.u_store.Status.Mode,
					      r->u.u_store.Mask, &r->sid, &c_inode);
			    ReintPrelimCOP(v, &r->u.u_store.Status.VV.StoreId,
					   &r->sid, volptr);
			    if (v->vptr->disk.type == vDirectory &&
				AllowResolution && V_VMResOn(volptr)) {
				int opcode = (v->d_needsres)
				  ? ResolveViceNewStore_OP
				  : ViceNewStore_OP;
				v->sl.append(new sle(InitVMLogRecord(V_volumeindex(volptr),
								     &v->fid, &r->sid,
								     opcode, STATUSStore,
								     r->u.u_store.Status.Owner, 
								     r->u.u_store.Status.Mode,
								     r->u.u_store.Mask)));
			    }

			    if (v->vptr->disk.type == vDirectory &&
				AllowResolution && V_RVMResOn(volptr)) {
				int opcode = (v->d_needsres)
				  ? ResolveViceNewStore_OP
				  : ViceNewStore_OP;
				LogMsg(5, SrvDebugLevel, stdout, 
				       "Spooling Reintegration newstore record \n");
				if (errorCode = 
				    SpoolVMLogRecord(vlist, v, volptr,
						     &r->sid, opcode, STSTORE,
						     r->u.u_store.Status.Owner,
						     r->u.u_store.Status.Mode, 
						     r->u.u_store.Status.Author, 
						     r->u.u_store.Status.Date, 
						     r->u.u_store.Mask,
						     &(r->u.u_store.Status.VV))) {
				    LogMsg(0, SrvDebugLevel, stdout,
					   "Reint: Error %d for spool of Store Op\n",
					   errorCode);
				    goto Exit;
				}
			    }

			    /* Note occurrence of COW. */
			    if (c_inode != 0) {
				assert(v->f_sinode == 0);
				v->f_sinode = c_inode;
				v->f_finode = v->vptr->disk.inodeNumber;
				truncp = 0;
			    }

			    /* Note need to truncate later. */
			    if (truncp) {
				LogMsg(3, SrvDebugLevel, stdout,  "CheckSemanticsAndPerform: noting truncation (%x.%x.%x, %d, %d), (%d, %d)",
					v->fid.Volume, v->fid.Vnode, v->fid.Unique,
					v->f_tinode, v->f_tlength,
					v->vptr->disk.inodeNumber, v->vptr->disk.length);
				v->f_tinode = v->vptr->disk.inodeNumber;
				v->f_tlength = v->vptr->disk.length;
			    }
			    }
			    break;

			case StoreNeither:
			default:
			    assert(FALSE);
		    }
		    if (errorCode = AdjustDiskUsage(volptr, deltablocks)) {
			goto Exit;
		    }
		    *blocks += deltablocks;
		    }
		    break;

		case ViceCreate_OP:
		    {
			int deltablocks = 0;
			
			/* Check. */
			vle *parent_v = FindVLE(*vlist, &r->u.u_create.Did);
			vle *child_v = FindVLE(*vlist, &r->u.u_create.Fid);
			if (errorCode = CheckCreateSemantics(client, &parent_v->vptr,
							     &child_v->vptr,
							     (char *)r->u.u_create.Name,
							     &volptr, 1, NormalVCmp, 
							     &r->u.u_create.DirStatus,
							     &r->u.u_create.Status,
							     0, 0))
			    goto Exit;
			
			/* make resolution log non-empty if necessary */
			if (AllowResolution && V_VMResOn(volptr)) 
			    MakeLogNonEmpty(parent_v->vptr);
			
			/* directory concurrency check */
			if (VV_Cmp(&Vnode_vv(parent_v->vptr), 
				   &r->u.u_create.DirStatus.VV) != VV_EQ)
			    parent_v->d_reintstale = 1;

			/* Perform. */
			HandleWeakEquality(volptr, parent_v->vptr, &r->u.u_create.DirStatus.VV);
			PerformCreate(client, VSGVolnum, volptr, parent_v->vptr,
				      child_v->vptr, (char *)r->u.u_create.Name, r->Mtime,
				      r->u.u_create.Status.Mode, 0, &r->sid,
				      &parent_v->d_cinode, &deltablocks);
			ReintPrelimCOP(parent_v, &r->u.u_create.DirStatus.VV.StoreId,
				       &r->sid, volptr);
			ReintPrelimCOP(child_v, &r->u.u_create.Status.VV.StoreId,
				       &r->sid, volptr);
			if (AllowResolution && V_VMResOn(volptr)) {
			    int opcode = (parent_v->d_needsres)
				? ResolveViceCreate_OP
				    : ViceCreate_OP;
			    parent_v->sl.append(new sle(InitVMLogRecord(V_volumeindex(volptr),
									&parent_v->fid, &r->sid,
									opcode,
									r->u.u_create.Name,
									r->u.u_create.Fid.Vnode,
									r->u.u_create.Fid.Unique)));
			}
			if (AllowResolution && V_RVMResOn(volptr)) {
			    int opcode = (parent_v->d_needsres)
				? ResolveViceCreate_OP
				    : ViceCreate_OP;
			    if (errorCode = SpoolVMLogRecord(vlist, parent_v, volptr, 
							     &r->sid, opcode, 
							     r->u.u_create.Name, 
							     r->u.u_create.Fid.Vnode,
							     r->u.u_create.Fid.Unique)) {
				LogMsg(0, SrvDebugLevel, stdout, 
				       "Reint(CSAP): Error %d during spooling log record for create\n",
				       errorCode);
				goto Exit;
			    }
			}
			*blocks += deltablocks;
		    }
		    break;
		    
		  case ViceRemove_OP:
		    {
		    /* Check. */
		    vle *parent_v = FindVLE(*vlist, &r->u.u_remove.Did);
		    vle *child_v;
		    if (errorCode = LookupChild(volptr, parent_v->vptr,
						(char *)r->u.u_remove.Name,
						&r->u.u_remove.TgtFid))
			goto Exit;
		    if (!(child_v = FindVLE(*vlist, &r->u.u_remove.TgtFid))) {
			errorCode = EINVAL;
			goto Exit;
		    }
		    if (errorCode = CheckRemoveSemantics(client, &parent_v->vptr,
							  &child_v->vptr,
							  (char *)r->u.u_remove.Name,
							  &volptr, 1, NormalVCmp,
							  &r->u.u_remove.DirStatus,
							  &r->u.u_remove.Status,
							  0, 0))
			goto Exit;

		    // make resolution log non-empty if necessary 
		    if (AllowResolution && V_VMResOn(volptr)) 
			MakeLogNonEmpty(parent_v->vptr);

		    /* directory concurrency check */
		    if (VV_Cmp(&Vnode_vv(parent_v->vptr), 
			       &r->u.u_remove.DirStatus.VV) != VV_EQ)
			parent_v->d_reintstale = 1;

		    /* Perform. */
		    int tblocks = 0;
		    HandleWeakEquality(volptr, parent_v->vptr, &r->u.u_remove.DirStatus.VV);
		    HandleWeakEquality(volptr, child_v->vptr, &r->u.u_remove.Status.VV);
		    PerformRemove(client, VSGVolnum, volptr, parent_v->vptr,
				  child_v->vptr, (char *)r->u.u_remove.Name, r->Mtime,
				  0, &r->sid, &parent_v->d_cinode, &tblocks);
		    ReintPrelimCOP(parent_v, &r->u.u_remove.DirStatus.VV.StoreId,
				   &r->sid, volptr);
		    ReintPrelimCOP(child_v, &r->u.u_remove.Status.VV.StoreId,
				   &r->sid, volptr);
		    if (AllowResolution && V_VMResOn(volptr)) {
			int opcode = (parent_v->d_needsres)
			  ? ResolveViceRemove_OP
			  : ViceRemove_OP;
			ViceVersionVector *ghostVV = &Vnode_vv(child_v->vptr);	/* ??? -JJK */
			parent_v->sl.append(new sle(InitVMLogRecord(V_volumeindex(volptr),
								    &parent_v->fid, &r->sid,
								    opcode,
								    r->u.u_remove.Name,
								    r->u.u_remove.TgtFid.Vnode,
								    r->u.u_remove.TgtFid.Unique,
								    ghostVV)));
		    }
		    if (AllowResolution && V_RVMResOn(volptr)) {
			int opcode = (parent_v->d_needsres)
			  ? ResolveViceRemove_OP
			  : ViceRemove_OP;
			ViceVersionVector *ghostVV = &Vnode_vv(child_v->vptr);	/* ??? -JJK */
			if (errorCode = SpoolVMLogRecord(vlist, parent_v, volptr, 
							 &r->sid, opcode, 
							 r->u.u_remove.Name, 
							 r->u.u_remove.TgtFid.Vnode, 
							 r->u.u_remove.TgtFid.Unique, 
							 ghostVV)) {
			    LogMsg(0, SrvDebugLevel, stdout,
				   "Reint: Error %d during spool log record for remove op\n",
				   errorCode);
			    goto Exit;
			}
		    }
		    *blocks += tblocks;
		    if (child_v->vptr->delete_me) {
			int deltablocks = -nBlocks(child_v->vptr->disk.length);
			if (errorCode = AdjustDiskUsage(volptr, deltablocks))
			    goto Exit;
			*blocks += deltablocks;

			child_v->f_sid = NullSid;
			child_v->f_sinode = child_v->vptr->disk.inodeNumber;
			child_v->vptr->disk.inodeNumber = 0;

			/* Cancel previous truncate. */
			LogMsg(3, SrvDebugLevel, stdout,  "CheckSemanticsAndPerform: cancelling truncate (%x.%x.%x, %d, %d)",
				child_v->fid.Volume, child_v->fid.Vnode, child_v->fid.Unique,
				child_v->f_tinode, child_v->f_tlength);
			child_v->f_tinode = 0;
			child_v->f_tlength = 0;
		    }
		    }
		    break;

		case ViceLink_OP:
		    {
		    int deltablocks = 0;

		    /* Check. */
		    vle *parent_v = FindVLE(*vlist, &r->u.u_link.Did);
		    vle *child_v = FindVLE(*vlist, &r->u.u_link.Fid);
		    if (errorCode = CheckLinkSemantics(client, &parent_v->vptr,
							&child_v->vptr,
							(char *)r->u.u_link.Name,
							&volptr, 1, NormalVCmp,
							&r->u.u_link.DirStatus,
							&r->u.u_link.Status,
							0, 0))
			goto Exit;

		    // make resolution log non-empty if necessary 
		    if (AllowResolution && V_VMResOn(volptr)) 
			MakeLogNonEmpty(parent_v->vptr);

		    /* directory concurrency check */
		    if (VV_Cmp(&Vnode_vv(parent_v->vptr), 
			       &r->u.u_link.DirStatus.VV) != VV_EQ)
			parent_v->d_reintstale = 1;

		    /* Perform. */
		    HandleWeakEquality(volptr, parent_v->vptr, &r->u.u_link.DirStatus.VV);
		    HandleWeakEquality(volptr, child_v->vptr, &r->u.u_link.Status.VV);
		    PerformLink(client, VSGVolnum, volptr, parent_v->vptr,
				child_v->vptr, (char *)r->u.u_link.Name, r->Mtime,
				0, &r->sid, &parent_v->d_cinode, &deltablocks);
		    ReintPrelimCOP(parent_v, &r->u.u_link.DirStatus.VV.StoreId,
				   &r->sid, volptr);
		    ReintPrelimCOP(child_v, &r->u.u_link.Status.VV.StoreId,
				   &r->sid, volptr);
		    if (AllowResolution && V_VMResOn(volptr)) {
			int opcode = (parent_v->d_needsres)
			  ? ResolveViceLink_OP
			  : ViceLink_OP;
			parent_v->sl.append(new sle(InitVMLogRecord(V_volumeindex(volptr),
								    &parent_v->fid, &r->sid,
								    opcode,
								    r->u.u_link.Name,
								    r->u.u_link.Fid.Vnode,
								    r->u.u_link.Fid.Unique)));
		    }
		    if (AllowResolution && V_RVMResOn(volptr)) {
			int opcode = (parent_v->d_needsres)
			  ? ResolveViceLink_OP
			  : ViceLink_OP;
			if (errorCode = SpoolVMLogRecord(vlist, parent_v, volptr, 
							 &r->sid, opcode, 
							 r->u.u_link.Name, 
							 r->u.u_link.Fid.Vnode,
							 r->u.u_link.Fid.Unique,
							 &(Vnode_vv(child_v->vptr)))) {
			    LogMsg(0, SrvDebugLevel, stdout,
				   "Reint: error %d during spool log record for ViceLink\n",
				   errorCode);
			    goto Exit;
			}
		    }
		    *blocks += deltablocks;
		    }
		    break;

		case ViceRename_OP:
		    {
		    /* Check. */
		    vle *sd_v = FindVLE(*vlist, &r->u.u_rename.OldDid);
		    vle *td_v = FindVLE(*vlist, &r->u.u_rename.NewDid);
		    int SameParent = (sd_v == td_v);
		    vle *s_v;
		    if (errorCode = LookupChild(volptr, sd_v->vptr,
						(char *)r->u.u_rename.OldName,
						&r->u.u_rename.SrcFid))
			goto Exit;
		    if (!(s_v = FindVLE(*vlist, &r->u.u_rename.SrcFid))) {
			errorCode = EINVAL;
			goto Exit;
		    }
		    vle *t_v = 0;
		    errorCode = LookupChild(volptr, td_v->vptr,
					    (char *)r->u.u_rename.NewName,
					    &r->u.u_rename.TgtFid);
		    switch(errorCode) {
			case 0:
			    if (!(t_v = FindVLE(*vlist, &r->u.u_rename.TgtFid))) {
				errorCode = EINVAL;
				goto Exit;
			    }
			    break;

			case ENOENT:
			    errorCode = 0;
			    break;

			default:
			    goto Exit;
		    }
		    int TargetExists = (t_v != 0);
		    if (errorCode = CheckRenameSemantics(client, &sd_v->vptr,
							  &td_v->vptr, &s_v->vptr,
							  (char *)r->u.u_rename.OldName,
							  (TargetExists ? &t_v->vptr : 0),
							  (char *)r->u.u_rename.NewName,
							  &volptr, 1, NormalVCmp,
							  &r->u.u_rename.OldDirStatus,
							  &r->u.u_rename.NewDirStatus,
							  &r->u.u_rename.SrcStatus,
							  &r->u.u_rename.TgtStatus,
							  0, 0, 0, 0, 0, 0, 1, 0, vlist))
			goto Exit;

		    // make resolution log non-empty if necessary 
		    if (AllowResolution && V_VMResOn(volptr)) {
			MakeLogNonEmpty(sd_v->vptr);
			if (!SameParent) MakeLogNonEmpty(td_v->vptr);
		    }

		    /* directory concurrency checks */
		    if (VV_Cmp(&Vnode_vv(sd_v->vptr), 
			       &r->u.u_rename.OldDirStatus.VV) != VV_EQ)
			sd_v->d_reintstale = 1;

		    if (!SameParent && 
			(VV_Cmp(&Vnode_vv(td_v->vptr), 
				&r->u.u_rename.NewDirStatus.VV) != VV_EQ))
			    td_v->d_reintstale = 1;

		    /* Perform. */
		    HandleWeakEquality(volptr, sd_v->vptr, &r->u.u_rename.OldDirStatus.VV);
		    if (!SameParent)
			HandleWeakEquality(volptr, td_v->vptr, &r->u.u_rename.NewDirStatus.VV);
		    HandleWeakEquality(volptr, s_v->vptr, &r->u.u_rename.SrcStatus.VV);
		    if (TargetExists)
			HandleWeakEquality(volptr, t_v->vptr, &r->u.u_rename.TgtStatus.VV);
		    PerformRename(client, VSGVolnum, volptr, sd_v->vptr, td_v->vptr,
				  s_v->vptr, (TargetExists ? t_v->vptr : 0),
				  (char *)r->u.u_rename.OldName, (char *)r->u.u_rename.NewName,
				  r->Mtime, 0, &r->sid, &sd_v->d_cinode, &td_v->d_cinode,
				  (s_v->vptr->disk.type == vDirectory ? &s_v->d_cinode : 0), NULL);
		    ReintPrelimCOP(sd_v, &r->u.u_rename.OldDirStatus.VV.StoreId,
				   &r->sid, volptr);
		    if (!SameParent)
			ReintPrelimCOP(td_v, &r->u.u_rename.NewDirStatus.VV.StoreId,
				       &r->sid, volptr);
		    ReintPrelimCOP(s_v, &r->u.u_rename.SrcStatus.VV.StoreId,
				   &r->sid, volptr);
		    if (TargetExists)
			ReintPrelimCOP(t_v, &r->u.u_rename.TgtStatus.VV.StoreId,
				       &r->sid, volptr);
		    if (AllowResolution && (V_VMResOn(volptr) || V_RVMResOn(volptr))) {
			if (!SameParent) {
			    /* SpoolRenameLogRecord() only allows one opcode, so we must */
			    /* coerce "non-resolve-needing" parent to "resolve-needing"! */
			    if (sd_v->d_needsres && !td_v->d_needsres)
				td_v->d_needsres = 1;
			    if (!sd_v->d_needsres && td_v->d_needsres)
				sd_v->d_needsres = 1;
			}
			int sd_opcode = (sd_v->d_needsres)
			  ? ResolveViceRename_OP
			  : ViceRename_OP;
			int td_opcode = (td_v->d_needsres)
			  ? ResolveViceRename_OP
			  : ViceRename_OP;
			if (V_VMResOn(volptr))
			    SpoolRenameLogRecord(sd_opcode, /*td_opcode, */s_v, t_v, sd_v,
						 td_v, volptr, (char *)r->u.u_rename.OldName,
						 (char *)r->u.u_rename.NewName, &r->sid);
			else {
			    // rvm resolution is on 
			    if (errorCode = SpoolRenameLogRecord((int) sd_opcode, (dlist *) vlist, 
								 (Vnode *)s_v->vptr,(Vnode *)( t_v ? t_v->vptr : NULL) ,
								 (Vnode *)sd_v->vptr, (Vnode *)td_v->vptr, (Volume *) volptr, 
								 (char *)r->u.u_rename.OldName,
								 (char *)r->u.u_rename.NewName, 
								(ViceStoreId *) &r->sid)) {
				LogMsg(0, SrvDebugLevel, stdout,
				       "Reint: Error %d during spool log record for rename\n",
				       errorCode);
				goto Exit;
			    }
			}
		    }
		    if (TargetExists && t_v->vptr->delete_me) {
			int deltablocks = -nBlocks(t_v->vptr->disk.length);
			if (errorCode = AdjustDiskUsage(volptr, deltablocks))
			    goto Exit;
			*blocks += deltablocks;

			if (t_v->vptr->disk.type != vDirectory) {
			    t_v->f_sid = NullSid;
			    t_v->f_sinode = t_v->vptr->disk.inodeNumber;
			    t_v->vptr->disk.inodeNumber = 0;
			}
		    }
		    }
		    break;

		case ViceMakeDir_OP:
		    {
		    int deltablocks = 0;

		    /* Check. */
		    vle *parent_v = FindVLE(*vlist, &r->u.u_mkdir.Did);
		    vle *child_v = FindVLE(*vlist, &r->u.u_mkdir.NewDid);
		    if (errorCode = CheckMkdirSemantics(client, &parent_v->vptr,
							 &child_v->vptr,
							 (char *)r->u.u_mkdir.Name,
							 &volptr, 1, NormalVCmp,
							 &r->u.u_mkdir.DirStatus,
							 &r->u.u_mkdir.Status,
							 0, 0))
			goto Exit;

		    // make resolution log non-empty if necessary 
		    if (AllowResolution && V_VMResOn(volptr)) 
			MakeLogNonEmpty(parent_v->vptr);
		    
		    /* directory concurrency check */
		    if (VV_Cmp(&Vnode_vv(parent_v->vptr), 
			       &r->u.u_mkdir.DirStatus.VV) != VV_EQ)
			parent_v->d_reintstale = 1;

		    /* Perform. */
		    HandleWeakEquality(volptr, parent_v->vptr, &r->u.u_mkdir.DirStatus.VV);
		    PerformMkdir(client, VSGVolnum, volptr, parent_v->vptr,
				 child_v->vptr, (char *)r->u.u_mkdir.Name, r->Mtime,
				 r->u.u_mkdir.Status.Mode, 0, &r->sid,
				 &parent_v->d_cinode, &deltablocks);
		    ReintPrelimCOP(parent_v, &r->u.u_mkdir.DirStatus.VV.StoreId,
				   &r->sid, volptr);
		    ReintPrelimCOP(child_v, &r->u.u_mkdir.Status.VV.StoreId,
				   &r->sid, volptr);
		    if (AllowResolution && V_VMResOn(volptr)) {
			int p_opcode = (parent_v->d_needsres)
			  ? ResolveViceMakeDir_OP
			  : ViceMakeDir_OP;
			int c_opcode = ViceMakeDir_OP;
			parent_v->sl.append(new sle(InitVMLogRecord(V_volumeindex(volptr),
								    &parent_v->fid, &r->sid,
								    p_opcode,
								    (char *)r->u.u_mkdir.Name,
								    r->u.u_mkdir.NewDid.Vnode,
								    r->u.u_mkdir.NewDid.Unique)));
			child_v->sl.append(new sle(InitVMLogRecord(V_volumeindex(volptr),
								   &child_v->fid, &r->sid,
								   c_opcode, ".",
								   r->u.u_mkdir.NewDid.Vnode,
								   r->u.u_mkdir.NewDid.Unique)));
		    }

		    if (AllowResolution && V_RVMResOn(volptr)) {
			int p_opcode = (parent_v->d_needsres)
			  ? ResolveViceMakeDir_OP
			  : ViceMakeDir_OP;
			int c_opcode = ViceMakeDir_OP;
			if (!errorCode) {
			    if (errorCode = SpoolVMLogRecord(vlist, parent_v, volptr, 
							     &r->sid, p_opcode, 
							     (char *)r->u.u_mkdir.Name,
							     r->u.u_mkdir.NewDid.Vnode,
							     r->u.u_mkdir.NewDid.Unique)) {
				LogMsg(0, SrvDebugLevel, stdout,
				       "Reint: Error %d during SpoolVMLogRecord for parent in MakeDir_OP\n",
				       errorCode);
				goto Exit;
			    }
			    if (errorCode = SpoolVMLogRecord(vlist, child_v, 
							     volptr, &r->sid, 
							     c_opcode, ".", 
							     r->u.u_mkdir.NewDid.Vnode,
							     r->u.u_mkdir.NewDid.Unique)) {
				LogMsg(0, SrvDebugLevel, stdout, 
				       "Reint:  error %d during SpoolVMLogRecord for child in MakeDir_OP\n",
				       errorCode);
				goto Exit;
			    }
			}
		    }
		    *blocks += deltablocks;
		    }
		    break;

		case ViceRemoveDir_OP:
		    {
		    /* Check. */
		    vle *parent_v = FindVLE(*vlist, &r->u.u_rmdir.Did);
		    vle *child_v;
		    if (errorCode = LookupChild(volptr, parent_v->vptr,
						(char *)r->u.u_rmdir.Name,
						&r->u.u_rmdir.TgtFid))
			goto Exit;
		    if (!(child_v = FindVLE(*vlist, &r->u.u_rmdir.TgtFid))) {
			errorCode = EINVAL;
			goto Exit;
		    }
		    if (errorCode = CheckRmdirSemantics(client, &parent_v->vptr,
							 &child_v->vptr,
							 (char *)r->u.u_rmdir.Name,
							 &volptr, 1, NormalVCmp,
							 &r->u.u_rmdir.Status,
							 &r->u.u_rmdir.TgtStatus,
							 0, 0))
			goto Exit;

		    // make resolution log non-empty if necessary 
		    if (AllowResolution && V_VMResOn(volptr)) 
			MakeLogNonEmpty(parent_v->vptr);

		    /* directory concurrency check */
		    if (VV_Cmp(&Vnode_vv(parent_v->vptr), 
			       &r->u.u_rmdir.Status.VV) != VV_EQ)
			parent_v->d_reintstale = 1;

		    /* Perform. */
		    int tblocks = 0;
		    HandleWeakEquality(volptr, parent_v->vptr, &r->u.u_rmdir.Status.VV);
		    HandleWeakEquality(volptr, child_v->vptr, &r->u.u_rmdir.TgtStatus.VV);
		    PerformRmdir(client, VSGVolnum, volptr, parent_v->vptr,
				 child_v->vptr, (char *)r->u.u_rmdir.Name, r->Mtime,
				 0, &r->sid, &parent_v->d_cinode, &tblocks);
		    ReintPrelimCOP(parent_v, &r->u.u_rmdir.Status.VV.StoreId,
				   &r->sid, volptr);
		    ReintPrelimCOP(child_v, &r->u.u_rmdir.TgtStatus.VV.StoreId,
				   &r->sid, volptr);
		    if (AllowResolution && V_VMResOn(volptr)) {
			int opcode = (parent_v->d_needsres)
			  ? ResolveViceRemoveDir_OP
			  : ViceRemoveDir_OP;
			VNResLog *vnlog;
			pdlist *pl = GetResLogList(child_v->vptr->disk.vol_index,
						   child_v->fid.Vnode, child_v->fid.Unique, &vnlog);
			assert(pl != NULL);
			ViceStoreId *ghostSid =	&(Vnode_vv(child_v->vptr).StoreId); /* ??? -JJK */
			parent_v->sl.append(new sle(InitVMLogRecord(V_volumeindex(volptr),
								    &parent_v->fid, &r->sid,
								    opcode,
								    (char *)r->u.u_rmdir.Name,
								    r->u.u_rmdir.TgtFid.Vnode,
								    r->u.u_rmdir.TgtFid.Unique,
								    (int)pl->head,
								    pl->cnt + child_v->sl.count(),
								    &vnlog->LCP,
								    ghostSid)));
		    }
		    if (AllowResolution && V_RVMResOn(volptr)) {
			int opcode = (parent_v->d_needsres)
			  ? ResolveViceRemoveDir_OP
			  : ViceRemoveDir_OP;
			if (errorCode = SpoolVMLogRecord(vlist, parent_v, 
							 volptr, &r->sid, 
							 opcode, 
							 (char *)r->u.u_rmdir.Name,
							 r->u.u_rmdir.TgtFid.Vnode,
							 r->u.u_rmdir.TgtFid.Unique,
							 VnLog(child_v->vptr), 
							 &(Vnode_vv(child_v->vptr).StoreId),
							 &(Vnode_vv(child_v->vptr).StoreId))) {
			    LogMsg(0, SrvDebugLevel, stdout, 
				   "Reint(CSAP): Error %d during SpoolVMLogRecord for RmDir_OP\n",
				   errorCode);
			    goto Exit;
			}
		    }
		    *blocks += tblocks;
		    assert(child_v->vptr->delete_me);
		    int deltablocks = -nBlocks(child_v->vptr->disk.length);
		    if (errorCode = AdjustDiskUsage(volptr, deltablocks))
			goto Exit;
		    *blocks += deltablocks;
		    }
		    break;

		case ViceSymLink_OP:
		    {
		    int deltablocks = 0;

		    /* Check. */
		    vle *parent_v = FindVLE(*vlist, &r->u.u_symlink.Did);
		    vle *child_v = FindVLE(*vlist, &r->u.u_symlink.Fid);
		    if (errorCode = CheckSymlinkSemantics(client, &parent_v->vptr,
							   &child_v->vptr,
							   (char *)r->u.u_symlink.NewName,
							   &volptr, 1, NormalVCmp,
							   &r->u.u_symlink.DirStatus,
							   &r->u.u_symlink.Status,
							   0, 0))
			goto Exit;

		    /* make resolution log non-empty if necessary */
		    if (AllowResolution && V_VMResOn(volptr)) 
			MakeLogNonEmpty(parent_v->vptr);

		    /* directory concurrency check */
		    if (VV_Cmp(&Vnode_vv(parent_v->vptr), 
			       &r->u.u_symlink.DirStatus.VV) != VV_EQ)
			parent_v->d_reintstale = 1;

		    /* Perform. */
		    assert(child_v->f_finode == 0);
		    child_v->f_finode = icreate(V_device(volptr), 0, V_id(volptr),
					       child_v->vptr->vnodeNumber,
					       child_v->vptr->disk.uniquifier, 1);
		    assert(child_v->f_finode > 0);
		    int linklen = strlen((char *)r->u.u_symlink.OldName);

		    assert(iwrite(V_device(volptr), child_v->f_finode, V_parentId(volptr), 0,
				  (char *)r->u.u_symlink.OldName, linklen) == linklen);
		    HandleWeakEquality(volptr, parent_v->vptr, &r->u.u_symlink.DirStatus.VV);
		    PerformSymlink(client, VSGVolnum, volptr, parent_v->vptr,
				   child_v->vptr, (char *)r->u.u_symlink.NewName,
				   child_v->f_finode, linklen, r->Mtime,
				   r->u.u_symlink.Status.Mode, 0, &r->sid,
				   &parent_v->d_cinode, &deltablocks);
		    ReintPrelimCOP(parent_v, &r->u.u_symlink.DirStatus.VV.StoreId,
				   &r->sid, volptr);
		    ReintPrelimCOP(child_v, &r->u.u_symlink.Status.VV.StoreId,
				   &r->sid, volptr);
		    if (AllowResolution && V_VMResOn(volptr)) {
			int opcode = (parent_v->d_needsres)
			  ? ResolveViceSymLink_OP
			  : ViceSymLink_OP;
			parent_v->sl.append(new sle(InitVMLogRecord(V_volumeindex(volptr),
								    &parent_v->fid, &r->sid,
								    opcode,
								    r->u.u_symlink.NewName,
								    r->u.u_symlink.Fid.Vnode,
								    r->u.u_symlink.Fid.Unique)));
		    }
		    if (AllowResolution && V_RVMResOn(volptr)) {
			int opcode = (parent_v->d_needsres)
			  ? ResolveViceSymLink_OP
			  : ViceSymLink_OP;
			if (errorCode = SpoolVMLogRecord(vlist, parent_v, 
							 volptr, &r->sid,
							 opcode, 
							 r->u.u_symlink.NewName,
							 r->u.u_symlink.Fid.Vnode,
							 r->u.u_symlink.Fid.Unique)) {
			    LogMsg(0, SrvDebugLevel, stdout, 
				   "Reint: Error %d during spool log record for ViceSymLink\n",
				   errorCode);
			    goto Exit;
			}
		    }
		    *blocks += deltablocks;
		    }
		    break;

		default:
		    assert(FALSE);
	    }

	    /* Yield after every so many records. */
	    count++;
            index++;
	    if ((count & Yield_CheckAndPerform_Mask) == 0)
		PollAndYield();
	}
	if (count < Yield_CheckAndPerform_Period - 1)
	    PollAndYield();
    }

    /* Now do bulk transfers. */
    {
	dlist_iterator next(*rlog);
	rle *r;
	while (r = (rle *)next()) {
	    switch(r->opcode) {
		case ViceCreate_OP:
		case ViceRemove_OP:
		case ViceLink_OP:
		case ViceRename_OP:
		case ViceMakeDir_OP:
		case ViceRemoveDir_OP:
		case ViceSymLink_OP:
		    break;

		case ViceNewStore_OP:
		    {
		    if (r->u.u_store.Request != StoreData &&
			r->u.u_store.Request != StoreStatusData)
			break;

		    if (r->u.u_store.Inode)	/* data already here */
			break;

		    /* Poll and yield here. */
		    /* This wouldn't be necessary if we had multiple CB connections to the client! */
		    PollAndYield();

		    vle *v = FindVLE(*vlist, &r->u.u_store.Fid);
		    if (!SID_EQ(v->f_sid, r->sid)) {
			/* Don't fetch intermediate versions. */
			break;
		    }
		    RPC2_Handle CBCid = client->VenusId->id;
		    if (CBCid == 0) {
			errorCode = RPC2_FAIL;	/* ??? -JJK */
                        index = -1;
			goto Exit;
		    }
		    SE_Descriptor sid;
		    sid.Tag = client->SEType;
		    sid.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
		    sid.Value.SmartFTPD.SeekOffset = 0;
		    sid.Value.SmartFTPD.hashmark = (SrvDebugLevel > 2 ? '#' : '\0');
		    sid.Value.SmartFTPD.Tag = FILEBYINODE;
		    sid.Value.SmartFTPD.ByteQuota = r->u.u_store.Length;
		    sid.Value.SmartFTPD.FileInfo.ByInode.Device = V_device(volptr);
		    sid.Value.SmartFTPD.FileInfo.ByInode.Inode = v->f_finode;
		    if (errorCode = CallBackFetch(CBCid, &r->u.u_store.UntranslatedFid, &sid)) {
                        index = -1;
			goto Exit;
		    }
		    RPC2_Integer len = sid.Value.SmartFTPD.BytesTransferred;
		    if (r->u.u_store.Length != len) {
			LogMsg(0, SrvDebugLevel, stdout,  "CBFetch: length discrepancy (%d : %d), (%x.%x.%x), %s %s.%d",
				r->u.u_store.Length, len,
				v->fid.Volume, v->fid.Vnode, v->fid.Unique,
				client->UserName, client->VenusId->HostName,
				client->VenusId->port);
			errorCode = EINVAL;
                        index = -1;
			goto Exit;
		    }

		    LogMsg(2, SrvDebugLevel, stdout,  "CBFetch: transferred %d bytes (%x.%x.%x)",
			    r->u.u_store.Length, v->fid.Volume, v->fid.Vnode, v->fid.Unique);
		    }
		    break;

		default:
		    assert(FALSE);
	    }
	}
    }

Exit:
    if (Index)  *Index = (RPC2_Integer) index;
    PutVolObj(&volptr, VOL_NO_LOCK);
    LogMsg(10, SrvDebugLevel, stdout,  "CheckSemanticsAndPerform: returning %s", ViceErrorMsg(errorCode));
END_TIMING(Reintegrate_CheckSemanticsAndPerform);
    return(errorCode);
}

/*
 *
 *    Phase IV consists of the following steps:
 *      1. Release the RL
 *      2. Finalize COP version state (on success)
 *      3. Put back objects/resources as with any Vice call
 *      4. Releasing the exclusive-mode volume reference
 *
 */
PRIVATE void PutReintegrateObjects(int errorCode, Volume *volptr, dlist *rlog, 
 	                           dlist *vlist, int blocks, ClientEntry *client, 
				   RPC2_Integer MaxDirs, RPC2_Integer *NumDirs,
				   ViceFid *StaleDirs, RPC2_CountedBS *OldVS, 
				   RPC2_Integer *NewVS, CallBackStatus *VCBStatus) {
START_TIMING(Reintegrate_PutObjects);
    LogMsg(10, SrvDebugLevel, stdout, 	"PutReintegrateObjects: Vid = %x, errorCode = %d",
	     volptr ? V_id(volptr) : 0, errorCode);

    ViceStoreId sid;

    /* Pre-transaction: release RL, then update version state. */
    if (rlog) {
	rle *r;
	int count = 0;
	while (r = (rle *)rlog->get()) {
	    if (rlog->count() == 0) 	/* last one -- save sid */
		sid = r->sid;
	    delete r;

	    /* Yield after every so many records. */
	    count++;
	    if ((count & Yield_RLDealloc_Mask) == 0)
		PollAndYield();
	}
	delete rlog;
	if (count < Yield_RLDealloc_Period - 1)
	    PollAndYield();
    }

    if (errorCode == 0 && vlist && volptr) {
	GetMyVS(volptr, OldVS, NewVS);

	dlist_iterator next(*vlist);
	vle *v;
	while (v = (vle *)next()) {
	    if ((!ISDIR(v->fid) || v->d_reintupdate) && !v->vptr->delete_me) {
		ReintFinalCOP(v, volptr, NewVS);
	    } else {
		LogMsg(2, SrvDebugLevel, stdout, "PutReintegrateObjects: un-mutated or deleted fid 0x%x.%x.%x",
		       v->fid.Volume, v->fid.Vnode, v->fid.Unique);
	    }

	    /* write down stale directory fids */
	    if (ISDIR(v->fid) && v->d_reintstale && StaleDirs) { /* compatibility check */
		LogMsg(0, SrvDebugLevel, stdout, "PutReintegrateObjects: stale directory fid 0x%x.%x.%x, num %d, max %d",
		       V_groupId(volptr), v->fid.Vnode, v->fid.Unique,
		       *NumDirs, MaxDirs);
		if (*NumDirs < MaxDirs) {
		    StaleDirs[(*NumDirs)] = v->fid;	/* send back replicated ID */
		    StaleDirs[(*NumDirs)++].Volume = V_groupId(volptr);
		}
	    }
        }
	SetVSStatus(client, volptr, NewVS, VCBStatus);
    }

    /* Release the objects. */
    if (volptr) {
	Volume *tvolptr = 0;
	assert(GetVolObj(V_id(volptr), &tvolptr, VOL_NO_LOCK) == 0);
	PutObjects(errorCode, tvolptr, VOL_NO_LOCK, vlist, blocks, 1);

	/* save the sid of the last successfully reintegrated record */
	if (errorCode == 0) {
	    int i = 0;

	    /* replace the entry for this client if one exists */
	    while (i < volptr->nReintegrators) {
		if (volptr->reintegrators[i].Host == sid.Host) {
		    volptr->reintegrators[i] = sid;
		    break;
		}
		i++;
	    }

	    /* no entry for this client, make one */
	    if (i == volptr->nReintegrators) {
		/* (re)allocate space if necessary */
		if ((i % VNREINTEGRATORS) == 0)	{ 
		    ViceStoreId *newlist = (ViceStoreId *) 
			malloc(sizeof(ViceStoreId) * (i + VNREINTEGRATORS));
		    if (volptr->reintegrators) {
			bcopy((char *)volptr->reintegrators, (char *)newlist,
			      (int) sizeof(ViceStoreId) * (i + VNREINTEGRATORS));
			free(volptr->reintegrators);
		    }
		    volptr->reintegrators = newlist;
		}
		volptr->reintegrators[i] = sid;
		volptr->nReintegrators++;
	    }
	}
    }

    /* Finally, release the exclusive-mode volume reference acquired at the beginning. */
    PutVolObj(&volptr, VOL_EXCL_LOCK);

    LogMsg(10, SrvDebugLevel, stdout,  "PutReintegrateObjects: returning %s", ViceErrorMsg(0));
END_TIMING(Reintegrate_PutObjects);
}


PRIVATE int AllocReintegrateVnode(Volume **volptr, dlist *vlist, ViceFid *pFid,
				   ViceFid *cFid, ViceDataType Type, UserId ClientId,
				   RPC2_Unsigned AllocHost, int *blocks) {
    int errorCode = 0;
    Vnode *vptr = 0;
    *blocks = 0;

    /* Get volptr. */
    /* We assume that volume has already been locked in exclusive mode! */
    if (*volptr == 0)
	assert(GetVolObj(pFid->Volume, volptr, VOL_NO_LOCK, 0, 0) == 0);

    /* Allocate/Retrieve the vnode. */
    if (errorCode = AllocVnode(&vptr, *volptr, Type, cFid,
				pFid, ClientId, AllocHost, blocks))
	goto Exit;

    /* Create a new vle for this vnode and add it to the vlist. */
/*    assert(FindVLE(*vlist, cFid) == 0);*/
    vle *v; v = AddVLE(*vlist, cFid);
    assert(v->vptr == 0);
    v->vptr = vptr;

Exit:
    /* Sanity check. */
    if (errorCode)
	assert(vptr == 0);

    LogMsg(2, SrvDebugLevel, stdout,  "AllocReintegrateVnode returns %s", ViceErrorMsg(errorCode));
    return(errorCode);
}


int AddChild(Volume **volptr, dlist *vlist, ViceFid *Did, char *Name, int IgnoreInc) {
    int errorCode = 0;
    Vnode *vptr = 0;

    /* Get volptr. */
    /* We assume that volume has already been locked in exclusive mode! */
    if (*volptr == 0)
	assert(GetVolObj(Did->Volume, volptr, VOL_NO_LOCK, 0, 0) == 0);

    /* Parent must NOT have just been alloc'ed, else this will deadlock! */
    if (errorCode = GetFsObj(Did, volptr, &vptr, READ_LOCK, VOL_NO_LOCK, IgnoreInc, 0))
	goto Exit;

    /* Look up the child, and add a vle if found. */
    ViceFid Fid;
    errorCode = LookupChild(*volptr, vptr, Name, &Fid);
    switch(errorCode) {
	case 0:
	    (void)AddVLE(*vlist, &Fid);
	    break;

	case ENOENT:
	    errorCode = 0;
	    break;

	default:
	    goto Exit;
    }

Exit:
    if (vptr) {
	Error fileCode = 0;
	VPutVnode(&fileCode, vptr);
	assert(fileCode == 0);
    }

    LogMsg(2, SrvDebugLevel, stdout,  "AddChild returns %s", ViceErrorMsg(errorCode));
    return(errorCode);
}


int LookupChild(Volume *volptr, Vnode *vptr, char *Name, ViceFid *Fid) {
    int errorCode = 0;
    
    DirHandle dh;
    SetDirHandle(&dh, vptr);
    if (Lookup((long *)&dh, Name, (long *)Fid) != 0) {
	errorCode = ENOENT;
	goto Exit;
    }
    Fid->Volume = V_id(volptr);

Exit:
    LogMsg(10, SrvDebugLevel, stdout,  "LookupChild returns %s", ViceErrorMsg(errorCode));
    return(errorCode);
}


PRIVATE int AddParent(Volume **volptr, dlist *vlist, ViceFid *Fid) {
    int errorCode = 0;
    Vnode *vptr = 0;

    /* Get volptr. */
    /* We assume that volume has already been locked in exclusive mode! */
    if (*volptr == 0)
	assert(GetVolObj(Fid->Volume, volptr, VOL_NO_LOCK, 0, 0) == 0);

    /* Child must NOT have just been alloc'ed, else this will deadlock! */
    if (errorCode = GetFsObj(Fid, volptr, &vptr, READ_LOCK, VOL_NO_LOCK, 0, 0))
	goto Exit;
    
    /* Look up the parent, and add a vle. */
    ViceFid Did;
    Did.Volume = Fid->Volume;
    Did.Vnode = vptr->disk.vparent;
    Did.Unique = vptr->disk.uparent;
    (void)AddVLE(*vlist, &Did);

Exit:
    if (vptr) {
	Error fileCode = 0;
	VPutVnode(&fileCode, vptr);
	assert(fileCode == 0);
    }

    LogMsg(2, SrvDebugLevel, stdout,  "AddParent returns %s", ViceErrorMsg(errorCode));
    return(errorCode);
}


/* Makes no version check for directories. */
/* Permits only Strong and Weak Equality for files. */
PRIVATE int ReintNormalVCmp(int ReplicatedOp, VnodeType type, void *arg1, void *arg2) {
    assert(ReplicatedOp == 1);

    switch(type) {
	case vDirectory:
	    return(0);

	case vFile:
	case vSymlink:
	    {
	    ViceVersionVector *vva = (ViceVersionVector *)arg1;
	    ViceVersionVector *vvb = (ViceVersionVector *)arg2;

	    int SameSid = SID_EQ(vva->StoreId, vvb->StoreId);
	    return(SameSid ? 0 : EINCOMPATIBLE);
	    }

	case vNull:
	default:
	    assert(0);
    }
}


/* Permits only Strong and Weak Equality for both files and directories. */
PRIVATE int ReintNormalVCmpNoRes(int ReplicatedOp, VnodeType type, void *arg1, void *arg2) {
    assert(ReplicatedOp == 1);
    ViceVersionVector *vva = (ViceVersionVector *)arg1;
    ViceVersionVector *vvb = (ViceVersionVector *)arg2;

    int SameSid = SID_EQ(vva->StoreId, vvb->StoreId);
    return(SameSid ? 0 : EINCOMPATIBLE);
}


/* This probably ought to be folded into the PerformXXX routines!  -JJK */
PRIVATE void ReintPrelimCOP(vle *v, ViceStoreId *OldSid,
			     ViceStoreId *NewSid, Volume *volptr) {
    /* Directories which are not identical to "old" contents MUST be stamped with unique Sid at end! */
    if (!SID_EQ(Vnode_vv(v->vptr).StoreId, *OldSid)) {
	assert(v->vptr->disk.type == vDirectory && AllowResolution && 
	       (V_VMResOn(volptr) || V_RVMResOn(volptr)));
	v->d_needsres = 1;
    }

    Vnode_vv(v->vptr).StoreId = *NewSid;
}


PRIVATE void ReintFinalCOP(vle *v, Volume *volptr, RPC2_Integer *VS) {
    ViceStoreId *FinalSid;
    ViceStoreId UniqueSid;
    if (v->vptr->disk.type == vDirectory && v->d_needsres) {
	assert(AllowResolution && 
	       (V_VMResOn(volptr) || V_RVMResOn(volptr)));
	AllocStoreId(&UniqueSid);
	FinalSid = &UniqueSid;
	MakeLogNonEmpty(v->vptr);
    }
    else {
	FinalSid = &Vnode_vv(v->vptr).StoreId;
    }

    /* 1. Record COP1 (for final update). */
    NewCOP1Update(volptr, v->vptr, FinalSid, VS);

    /* 2. Record COP2 pending (for final update). */
    /* Note that for directories that "need-resolved", (1) there is no point in recording a COP2 pending */
    /* (since it would be ignored), and (2) we must log a ResolveNULL_OP so that resolution works correctly. */
    if (v->vptr->disk.type == vDirectory && v->d_needsres) {
	assert(AllowResolution && (V_VMResOn(volptr) || V_RVMResOn(volptr)));
	if (V_VMResOn(volptr))
	    v->sl.append(new sle(InitVMLogRecord(V_volumeindex(volptr), &v->fid,
						 FinalSid, ResolveNULL_OP, 0)));
	if (V_RVMResOn(volptr))
	    assert(SpoolVMLogRecord(v, volptr, FinalSid, ResolveNULL_OP, 0) == 0);
    }
    else {
	AddPairToCopPendingTable(FinalSid, &v->fid);
    }
}


/*  *****  Gross stuff for packing/unpacking RPC arguments  *****  */

/* Unpack a ReintegrationLog Entry. */
/* Patterned after code in MRPC_MakeMulti(). */
PRIVATE void RLE_Unpack(int dummy1, int dummy2, PARM **ptr, ARG *ArgTypes ...) {
    LogMsg(100, SrvDebugLevel, stdout,  "RLE_Unpack: ptr = %x, ArgTypes = %x", ptr, ArgTypes);

    va_list ap;
    va_start(ap, ArgTypes);
    PARM *args = &(va_arg(ap, PARM));
    for (ARG *a_types = ArgTypes; a_types->mode != C_END; a_types++, args++) {
	if (a_types->mode != IN_MODE && a_types->mode != IN_OUT_MODE)
	    continue;

/*
	LogMsg(100, SrvDebugLevel, stdout,  "\ta_types = [%d %d %d %x], ptr = (%x %x %x), args = (%x %x)",
		a_types->mode, a_types->type, a_types->size, a_types->field,
		ptr, *ptr, **ptr, args, *args);
*/

	/* Extra level of indirection, since unpack routines are from MRPC. */
	PARM *xargs = (PARM *)&args;

/*
	if (a_types->type == RPC2_COUNTEDBS_TAG) {
	    LogMsg(100, SrvDebugLevel, stdout,  "\t&xargs->cbsp[0]->SeqLen = %x, * = %d, ntohl((*_ptr)->integer) = %d",
		    &(xargs->cbsp[0]->SeqLen), xargs->cbsp[0]->SeqLen, ntohl((*(ptr))->integer));
	    LogMsg(100, SrvDebugLevel, stdout,  "\t&xargs->cbsp[0]->SeqBody = %x, * = %x, (*_ptr) = %x",
		    &(xargs->cbsp[0]->SeqBody), xargs->cbsp[0]->SeqBody, *(ptr + 1));
	}
*/

	if (a_types->type == RPC2_STRUCT_TAG) {
	    PARM *str = (PARM *)xargs->structpp[0];
	    unpack_struct(a_types, &str, (PARM **)ptr, 0);
	}
	else {
	    if (a_types->type == RPC2_STRING_TAG)
		/* Temporary!  Fix an "extra dereference" bug in unpack!  -JJK */
		unpack(a_types, (PARM *)&xargs, (PARM **)ptr, 0);
	    else
		unpack(a_types, xargs, (PARM **)ptr, 0);
	}
    }

    va_end(ap);
    LogMsg(100, SrvDebugLevel, stdout,  "RLE_Unpack: returning");
}


/* 
 * Extract and validate the reintegration handle.  Handle errors are
 * propagated back to the client as EBADF.
 */
PRIVATE int ValidateRHandle(VolumeId Vid, int numHandles, 
			   ViceReintHandle RHandle[], ViceReintHandle **MyHandle) {

    LogMsg(10, SrvDebugLevel, stdout,  "ValidateRHandle: Vid = %x", Vid);

    /* get the volume and sanity check */
    int error, count, ix;
    Volume *volptr;
    VolumeId rwVid = Vid;

    if (!XlateVid(&rwVid, &count, &ix)) {
	LogMsg(1, SrvDebugLevel, stdout, "ValidateRHandle: Couldn't translate VSG %x", 
	       Vid);
	error = EINVAL;
	goto Exit;
    }

    if (ix > numHandles) {
	LogMsg(0, SrvDebugLevel, stdout, "ValidateRHandle: Not enough handles! (%d, %d)",
	       ix, numHandles);
	error = EBADF;
	goto Exit;
    }
	
    LogMsg(9, SrvDebugLevel, stdout,  "ValidateRHandle: Going to get volume %x pointer", 
	   rwVid);
    volptr = VGetVolume((Error *) &error, rwVid);
    LogMsg(1, SrvDebugLevel, stdout, "ValidateRHandle: Got volume %x: error = %d ", 
	   rwVid, error);

    if (error){
	LogMsg(0, SrvDebugLevel, stdout,  "ValidateRHandle, VgetVolume error %s", 
	       ViceErrorMsg((int)error));
	/* should we check to see if we must do a putvolume here */
	goto Exit;
    }

    /* check device */
    if (V_device(volptr) != RHandle[ix].Device) {
	LogMsg(0, SrvDebugLevel, stdout, "ValidateRHandle: Bad device (%d,%d)",
	       V_device(volptr), RHandle[ix].Device);
	error = EBADF;
	goto Exit;
    }

    /* check age of handle */
    if (StartTime != RHandle[ix].BirthTime) {
	LogMsg(0, SrvDebugLevel, stdout, "ValidateRHandle: Old handle (%d,%d,%d)",
	       RHandle[ix].BirthTime, RHandle[ix].Device, RHandle[ix].Inode);
	error = EBADF;
	goto Exit;
    }

    *MyHandle = &RHandle[ix];

 Exit:
    return(error);
}

