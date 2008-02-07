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
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/file.h>
#include <stdarg.h>
#include <netinet/in.h>
#include <inodeops.h>
#include "coda_string.h"

#include <rpc2/rpc2.h>
#include <rpc2/se.h>
#include <util.h>
#include <rvmlib.h>
#include <callback.h>
#include <vice.h>
#include <cml.h>

#ifdef __cplusplus
}
#endif

#include <volume.h>
#include <srv.h>
#include <volume.h>
#include <coppend.h>
#include <lockqueue.h>
#include <vldb.h>
#include <vrdb.h>
#include <repio.h>
#include <vlist.h>
#include <codaproc.h>
#include <codadir.h>
#include <operations.h>
#include <resutil.h>
#include <ops.h>
#include <rsle.h>
#include <inconsist.h>
#include <vice.private.h>
#include <dllist.h>

#ifndef O_BINARY
#define O_BINARY 0
#endif /* O_BINARY */

extern void MakeLogNonEmpty(Vnode *);
extern void HandleWeakEquality(Volume *, Vnode *, ViceVersionVector *);

/* Yield parameters (i.e., after how many loop iterations do I poll and yield). */
/* N.B.  Yield "periods" MUST all be power of two so that AND'ing can be used! */
const int Yield_RLAlloc_Period = 256;
const int Yield_RLAlloc_Mask = (Yield_RLAlloc_Period - 1);
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
#endif

#include <rpc2/multi.h>
extern void unpack(ARG *, PARM *, PARM **, char *_end, long);
extern void unpack_struct(ARG *, PARM **, PARM **, char *_end, long);

#ifdef __cplusplus
}
#endif

static void RLE_Unpack(PARM **, char *_end, ARG * ...);


/*  *****  Reintegration Log  *****  */

struct rle {
    struct dllist_head reint_log;
    ViceStoreId sid;
    RPC2_Integer opcode;
    Date_t Mtime;
    ViceFid Fid[3];
    ViceVersionVector VV[3];
    char *Name[2];
    union {
	struct {
            UserId Owner; // unused?
            RPC2_Unsigned Mode;
	} u_create;
	struct {
	    ViceStatus Status;
            UserId Owner; // unused?
            RPC2_Unsigned Mode;
	} u_mkdir;
	struct {
            UserId Owner; // unused?
            RPC2_Unsigned Mode;
	} u_symlink;
	struct {
	    ViceFid TgtFid;
	} u_remove;
	struct {
	    ViceFid TgtFid;
	} u_rmdir;
	struct {
	    RPC2_Integer Length;
	    ViceFid UntranslatedFid;	    /* in case we need to fetch 
					       this object! */
	    RPC2_Integer Inode;		    /* if data is already local */
	} u_store;
	struct {
	    Date_t Date;
	} u_utimes;
	struct {
	    RPC2_Unsigned Mode;
	} u_chmod;
	struct {
	    UserId Owner;
	} u_chown;
	struct {
	    ViceFid SrcFid;
	    ViceFid TgtFid;
	} u_rename;
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
 *      2. Retried reintegrations fail because vnodes allocated during 
           reintegration aren't cleaned up properly
 *         (this should be fixed with the new fid allocation mechanism, 
            separating fid and vnode allocation) (?)
 *
 */

static int ValidateReintegrateParms(RPC2_Handle, VolumeId *, Volume **, 
				    ClientEntry **, int, struct dllist_head *, 
				    RPC2_Integer *, ViceReintHandle *);
static int GetReintegrateObjects(ClientEntry *, struct dllist_head *, dlist *,
				 int *, RPC2_Integer *);
static int CheckSemanticsAndPerform(ClientEntry *, VolumeId, VolumeId,
				    struct dllist_head *, dlist *, int *,
				    RPC2_Integer *);
static void PutReintegrateObjects(int, Volume *, struct dllist_head *, dlist *, 
				  int, RPC2_Integer, ClientEntry *, 
				  RPC2_Integer, RPC2_Integer *, ViceFid *, 
				  RPC2_CountedBS *, RPC2_Integer *, 
				  CallBackStatus *);

static int AllocReintegrateVnode(Volume **, dlist *, ViceFid *, ViceFid *,
				 ViceDataType, UserId, int *);

static int AddParent(Volume **, dlist *, ViceFid *);
static int ReintNormalVCmp(int, VnodeType, void *, void *);
static void ReintPrelimCOP(vle *, const ViceStoreId *oldSID,
			   const ViceStoreId *newSID);
static void ReintFinalCOP(vle *, Volume *, RPC2_Integer *);
static int ValidateRHandle(VolumeId, ViceReintHandle *);


/*
  ViceReintegrate: Reintegrate disconnected mutations
*/
long FS_ViceReintegrate(RPC2_Handle RPCid, VolumeId Vid, RPC2_Integer LogSize,
		     RPC2_Integer *Index, RPC2_Integer OutOfOrder,
		     RPC2_Integer MaxDirs, RPC2_Integer *NumDirs,
		     ViceFid StaleDirs[], RPC2_CountedBS *OldVS,
		     RPC2_Integer *NewVS, CallBackStatus *VCBStatus,
		     RPC2_CountedBS *PiggyBS, SE_Descriptor *BD) 
{
	START_TIMING(Reintegrate_Total);
	SLog(1, "ViceReintegrate: Volume = %x", Vid);
	
	int errorCode = 0;
	ClientEntry *client = 0;
	VolumeId VSGVolnum = Vid;
	Volume *volptr = 0;
	INIT_LIST_HEAD(rlog);
	dlist *vlist = new dlist((CFN)VLECmp);
	int	blocks = 0;

	if (NumDirs) *NumDirs = 0;	/* check for compatibility */


	/* Phase 0. */
	if ((PiggyBS->SeqLen > 0) && (errorCode = FS_ViceCOP2(RPCid, PiggyBS))) {
		if (Index) 
			*Index = -1;
		goto FreeLocks;
	}

	SLog(1, "Starting ValidateReintegrateParms for %x", Vid);

	/* Phase I. */
	if ((errorCode = ValidateReintegrateParms(RPCid, &Vid, &volptr, &client,
						 LogSize, &rlog, Index, 0)))
		goto FreeLocks;
	
	SLog(1, "Starting GetReintegrateObjects for %x", Vid);

	/* Phase II. */
	if ((errorCode = GetReintegrateObjects(client, &rlog, vlist, 
					      &blocks, Index)))
		goto FreeLocks;

	SLog(1, "Starting CheckSemanticsAndPerform for %x", Vid);

	/* Phase III. */
	if ((errorCode = CheckSemanticsAndPerform(client, Vid, VSGVolnum, &rlog,
						 vlist, &blocks, Index)))
		goto FreeLocks;
	
 FreeLocks:
	/* Phase IV. */

	SLog(1, "Starting PutReintegrateObjects for %x", Vid);

	PutReintegrateObjects(errorCode, volptr, &rlog, vlist, blocks,
			      OutOfOrder, client, MaxDirs, NumDirs, StaleDirs,
			      OldVS, NewVS, VCBStatus);
	
	SLog(1, "ViceReintegrate returns %s", ViceErrorMsg(errorCode));
	END_TIMING(Reintegrate_Total);
	return(errorCode);
}


/*
  ViceOpenReintHandle:  get a handle to store new data for
  an upcoming reintegration call
*/
long FS_ViceOpenReintHandle(RPC2_Handle RPCid, ViceFid *Fid, 
			 ViceReintHandle *RHandle)
{
    int errorCode = 0;		/* return code for caller */
    Volume *volptr = 0;		/* pointer to the volume header */
    ClientEntry *client = 0;	/* pointer to client structure */
    dlist *vlist = new dlist((CFN)VLECmp);
    vle *v;

    SLog(0/*1*/, "ViceOpenReintHandle: Fid = %s", FID_(Fid));

    if ((errorCode = ValidateParms(RPCid, &client, NULL, &Fid->Volume, 0, NULL)))
	goto FreeLocks;

    v = AddVLE(*vlist, Fid);
    if ((errorCode = GetFsObj(Fid, &volptr, &v->vptr, READ_LOCK, 
			     NO_LOCK, 0, 0, 0)))
	goto FreeLocks;

    /* create a new inode */
    RHandle->BirthTime = (RPC2_Integer) StartTime;
    RHandle->Device = (RPC2_Integer) V_device(volptr);
    RHandle->Inode = icreate((int) V_device(volptr), (int) V_id(volptr), 
		      (int) v->vptr->vnodeNumber, (int) v->vptr->disk.uniquifier, 
		      (int) v->vptr->disk.dataVersion + 1);
    CODA_ASSERT(RHandle->Inode > 0);

FreeLocks:
    /* Put objects. */
    PutObjects(errorCode, volptr, NO_LOCK, vlist, 0, 0);
    SLog(0/*2*/, "ViceOpenReintHandle returns (%d,%d,%d), %s", 
	   RHandle->BirthTime, RHandle->Device, RHandle->Inode,
	   ViceErrorMsg(errorCode));

    return(errorCode);
}


/*
  ViceQueryReintHandle: Get the status of a partially 
  transferred file for an upcoming reintegration.  Now returns a byte offset, 
  but could be expanded to handle negotiation.
*/
long FS_ViceQueryReintHandle(RPC2_Handle RPCid, VolumeId Vid,
			     ViceReintHandle *RHandle, RPC2_Unsigned *Length)
{
    int errorCode = 0;
    ClientEntry *client = 0;
    int fd = -1;
    struct stat status;
    char *rock;

    SLog(0/*1*/, "ViceQueryReintHandle for volume %x", Vid);

    *Length = (RPC2_Unsigned)-1;

    /* Map RPC handle to client structure. */
    if ((errorCode = (int) RPC2_GetPrivatePointer(RPCid, &rock)) != RPC2_SUCCESS) {
	SLog(0, "ViceQueryReintHandle: GetPrivatePointer failed (%d)", errorCode);
	goto Exit;
    }
    client = (ClientEntry *)rock;

    if ( (errorCode = ValidateRHandle(Vid, RHandle)) )
	goto Exit;

    SLog(0/*1*/, "ViceQueryReintHandle: Handle = (%d,%d,%d)",
	   RHandle->BirthTime, RHandle->Device, RHandle->Inode);
    
    /* open and stat the inode */
    if ((fd = iopen((int) RHandle->Device, (int) RHandle->Inode, O_RDONLY)) < 0) {
	SLog(0, "ViceReintQueryHandle: iopen(%d, %d) failed (%d)",
		RHandle->Device, RHandle->Inode, errno);
	errorCode = errno;
	goto Exit;
    }

    if (fstat(fd, &status) < 0) {
	SLog(0, "ViceReintQueryHandle: fstat(%d, %d) failed (%d)",
		RHandle->Device, RHandle->Inode, errno);
	errorCode = errno;
	goto Exit;
    }

    *Length = (RPC2_Unsigned) status.st_size;

 Exit:
    if (fd != -1) {
	int ret = close(fd);
	CODA_ASSERT(ret == 0);
    }
    SLog(0/*2*/, "ViceQueryReintHandle returns length %d, %s",
	   *Length, ViceErrorMsg(errorCode));

    return(errorCode);
}


/*
  ViceSendReintFragment:  append file data corresponding to the 
  handle for  an upcoming reintegration.
*/
long FS_ViceSendReintFragment(RPC2_Handle RPCid, VolumeId Vid,
			      ViceReintHandle *RHandle, RPC2_Unsigned Length,
			      SE_Descriptor *BD)
{
    int errorCode = 0;		/* return code for caller */
    ClientEntry *client = 0;	/* pointer to client structure */
    int fd = -1;
    struct stat status;
    SE_Descriptor sid;
    char *rock;

    SLog(0/*1*/, "ViceSendReintFragment for volume %x", Vid);

    /* Map RPC handle to client structure. */
    if ((errorCode = (int) RPC2_GetPrivatePointer(RPCid, &rock)) != RPC2_SUCCESS) {
	SLog(0, "ViceSendReintFragment: GetPrivatePointer failed (%d)", errorCode);
	goto Exit;
    }	
    client = (ClientEntry *)rock;

    if ( (errorCode = ValidateRHandle(Vid, RHandle)) )
	goto Exit;

    SLog(0/*1*/, "ViceSendReintFragment: Handle = (%d,%d,%d), Length = %d",
	     RHandle->BirthTime, RHandle->Device, RHandle->Inode, Length);

    /* open and stat the inode */
    if ((fd = iopen((int) RHandle->Device, (int) RHandle->Inode, O_RDWR)) < 0) {
	SLog(0, "ViceSendReintFragment: iopen(%d, %d) failed (%d)",
		RHandle->Device, RHandle->Inode, errno);
	errorCode = errno;
	goto Exit;
    }

    if (fstat(fd, &status) < 0) {
	SLog(0, "ViceSendReintFragment: fstat(%d, %d) failed (%d)",
		RHandle->Device, RHandle->Inode, errno);
	errorCode = errno;
	goto Exit;
    }

    /* transfer and append the data */
    memset(&sid, 0, sizeof(SE_Descriptor));
    sid.Tag = client->SEType;
    sid.Value.SmartFTPD.TransmissionDirection = CLIENTTOSERVER;
    sid.Value.SmartFTPD.SeekOffset = status.st_size;	
    sid.Value.SmartFTPD.hashmark = (SrvDebugLevel > 2 ? '#' : '\0');
    sid.Value.SmartFTPD.Tag = FILEBYFD;
    sid.Value.SmartFTPD.ByteQuota = Length;
    sid.Value.SmartFTPD.FileInfo.ByFD.fd = fd;

    if((errorCode = (int) RPC2_InitSideEffect(RPCid, &sid)) <= RPC2_ELIMIT) {
	SLog(0, "ViceSendReintFragment: InitSE failed (%d), (%d,%d,%d)",
	       errorCode, RHandle->BirthTime, RHandle->Device, RHandle->Inode);

	goto Exit;
    }

    if ((errorCode = (int) RPC2_CheckSideEffect(RPCid, &sid, SE_AWAITLOCALSTATUS)) <= RPC2_ELIMIT) {
	SLog(0, "ViceSendReintFragment: CheckSE failed (%d), (%d,%d,%d)",
	       errorCode, RHandle->BirthTime, RHandle->Device, RHandle->Inode);

	if (errorCode == RPC2_SEFAIL1) errorCode = EIO;

	/* restore original state */
	int ret = ftruncate(fd, status.st_size);
	CODA_ASSERT(ret == 0);
	goto Exit;
    }

    if (sid.Value.SmartFTPD.BytesTransferred != (long)Length) {
	SLog(0, "ViceSendReintFragment: length discrepancy (%d : %d), (%d,%d,%d), %s %s.%d",
	       Length, sid.Value.SmartFTPD.BytesTransferred, 
	       RHandle->BirthTime, RHandle->Device, RHandle->Inode,
	       client->UserName, inet_ntoa(client->VenusId->host),
	       ntohs(client->VenusId->port));
	errorCode = EINVAL;

	/* restore original state */
	int ret = ftruncate(fd, status.st_size);
	CODA_ASSERT(ret == 0);
	goto Exit;
    }

 Exit:
    if (fd != -1) {
	int ret = close(fd);
	CODA_ASSERT(ret == 0);
    }

    SLog(0/*2*/, "ViceSendReintFragment returns %s", ViceErrorMsg(errorCode));

    return(errorCode);
}

/*
  ViceCloseReintHandle: Reintegrate data corresponding 
  to the reintegration handle.  This corresponds to the reintegration of
  a single store record.
*/
long FS_ViceCloseReintHandle(RPC2_Handle RPCid, VolumeId Vid,
			     RPC2_Integer LogSize, ViceReintHandle RHandle[], 
			     RPC2_CountedBS *OldVS, RPC2_Integer *NewVS, 
			     CallBackStatus *VCBStatus,
			     RPC2_CountedBS *PiggyBS, SE_Descriptor *BD)
{
    int errorCode = 0;
    ClientEntry *client = 0;
    VolumeId VSGVolnum = Vid;
    Volume *volptr = 0;
    INIT_LIST_HEAD(rlog);
    dlist *vlist = new dlist((CFN)VLECmp);
    int	blocks = 0;

    SLog(0/*1*/, "ViceCloseReintHandle for volume 0x%x", Vid);

    /* Phase 0. */
    if ((PiggyBS->SeqLen > 0) && (errorCode = FS_ViceCOP2(RPCid, PiggyBS))) 
	goto FreeLocks;

    if ( (errorCode = ValidateRHandle(Vid, RHandle)) )
	goto FreeLocks;

    /* Phase I. */
    if ((errorCode = ValidateReintegrateParms(RPCid, &Vid, &volptr, &client,
					     LogSize, &rlog, 0, RHandle)))
	goto FreeLocks;

    /* Phase II. */
    if ((errorCode = GetReintegrateObjects(client, &rlog, vlist, &blocks, 0)))
	goto FreeLocks;

    /* Phase III. */
    if ((errorCode = CheckSemanticsAndPerform(client, Vid, VSGVolnum, &rlog, 
					      vlist, &blocks, 0)))
	goto FreeLocks;

 FreeLocks:
    /* Phase IV. */
    PutReintegrateObjects(errorCode, volptr, &rlog, vlist, blocks, 0,
			  client, 0, NULL, NULL, OldVS, NewVS, VCBStatus);

    SLog(0/*2*/, "ViceCloseReintHandle returns %s", ViceErrorMsg(errorCode));

    return(errorCode);
}



/*
 *
 *    Phase I consists of the following steps:
 *      1. Translating the volume id from logical to physical
 *      2. Looking up the client entry
 *      3. Fetching over the client's representation of the reintegrate log
 *      4. Parsing the client log into a server version (the RL)
 *      5. Translating the volume ids in all the RL entries from 
 *         logical to physical
 *      6. Acquiring the volume in exclusive mode
 *
 */
static int ValidateReintegrateParms(RPC2_Handle RPCid, VolumeId *Vid,
				    Volume **volptr, ClientEntry **client,
				    int rlen, struct dllist_head *rlog,
				    RPC2_Integer *Index,
				    ViceReintHandle *RHandle) 
{
	START_TIMING(Reintegrate_ValidateParms);
	SLog(10, "ValidateReintegrateParms: RPCid = %d, *Vid = %x", 
	     RPCid, *Vid);

	int errorCode = 0;
	*volptr = 0;
	char *rfile = NULL;
	PARM *_ptr = 0;
	char *_end;
	int index;
	char *OldName = NULL;
	char *NewName = NULL;
	int rlog_len = 0;

	/* Translate the volume. */
	VolumeId VSGVolnum = *Vid;
	int count, ix;
	if (!XlateVid(Vid, &count, &ix)) {
		SLog(0, "ValidateReintegrateParms: failed to translate VSG %x",
		     VSGVolnum);
		errorCode = EINVAL;
		index = -1;
		goto Exit;
	}
	SLog(2, "ValidateReintegrateParms: %x --> %x", VSGVolnum, *Vid);

	/* Get the client entry. */
	if((errorCode = RPC2_GetPrivatePointer(RPCid, (char **)client)) 
	   != RPC2_SUCCESS) {
		SLog(0, "ValidateReintegrateParms: no private pointer for RPCid %x", RPCid);
		index = -1;
		goto Exit;
	}
	if(!*client) {
		SLog(0,  "ValidateReintegrateParms: NULL private pointer for RPCid %x", RPCid);
		errorCode = EINVAL;
		index = -1;
		goto Exit;
	}
    SLog(2,  "ValidateReintegrateParms: %s %s.%d",
	     (*client)->UserName, inet_ntoa((*client)->VenusId->host),
	     ntohs((*client)->VenusId->port));


    /* Fetch over the client's reintegrate log, and read it into memory. */
    {
	rfile = new char[rlen];
	CODA_ASSERT(rfile != 0);

	SE_Descriptor sid;
	memset(&sid, 0, sizeof(SE_Descriptor));
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
	    SLog(0,  "ValidateReintegrateParms: Init_SE failed (%d)", errorCode);
	    index = -1;
	    goto Exit;
	}

	if ((errorCode = RPC2_CheckSideEffect(RPCid, &sid, SE_AWAITLOCALSTATUS)) <= RPC2_ELIMIT) {
	    SLog(0,  "ValidateReintegrateParms: Check_SE failed (%d)", errorCode);
	    if (errorCode == RPC2_SEFAIL1) errorCode = EIO;
	    index = -1;
	    goto Exit;
	}

	SLog(1, "Reintegrate transferred %d bytes.", sid.Value.SmartFTPD.BytesTransferred);
    }

    OldName = new char[MAXNAMELEN+1];
    NewName = new char[MAXNAMELEN+1];
    if (!OldName || !NewName) {
	    errorCode = ENOMEM;
	    index = -1;
	    goto Exit;
    }

    /* Allocate/unpack entries and append them to the RL. */
    _end = rfile + rlen;
    for (_ptr = (PARM *)rfile, index = 0; (char *)_ptr < _end; index++) {
	RPC2_CountedBS DummyCBS;
	DummyCBS.SeqLen = 0;
	DummyCBS.SeqBody = 0;
	RPC2_Unsigned DummyPH;
        ViceFid DummyFid;
        RPC2_Unsigned DummyAllocHost;
        ViceStatus DummyStatus;

        ViceStatus OldDirStatus, DirStatus, Status;

	struct rle *r = (struct rle *)malloc(sizeof(struct rle));
	if (!r) {
	    errorCode = ENOMEM;
	    goto Exit;
	}

	list_head_init(&r->reint_log);
	r->Name[0] = r->Name[1] = NULL;
	for (int i = 0; i < 3; i++) {
	    r->Fid[i] = NullFid;
	    r->VV[i] = NullVV;
	}

	r->opcode = ntohl(*(RPC2_Integer *)_ptr);
	_ptr = (PARM *)((char *)_ptr + sizeof(RPC2_Integer));
	r->Mtime = ntohl(*(Date_t *)_ptr);
	_ptr = (PARM *)((char *)_ptr + sizeof(Date_t));

	SLog(100,  "ValidateReintegrateParms: [B] Op = %d, Mtime = %d",
		r->opcode, r->Mtime);

	switch(r->opcode) {
	    case OLDCML_Create_OP:
		RLE_Unpack(&_ptr, _end, OLDCML_Create_PTR, &r->Fid[0],
			   &DummyFid, NewName, &Status,
                           &r->Fid[1], &DirStatus, &DummyAllocHost,
			   &r->sid, &DummyCBS);

		r->Name[0] = strdup(NewName);
		if (!r->Name[0]) {
		    errorCode = ENOMEM;
		    goto Exit;
		}

                r->opcode = CML_Create_OP;
                r->VV[0] = DirStatus.VV;
                r->u.u_create.Owner = Status.Owner;
                r->u.u_create.Mode  = Status.Mode;
		break;

	    case OLDCML_Link_OP:
		RLE_Unpack(&_ptr, _end, OLDCML_Link_PTR, &r->Fid[0], NewName,
			   &r->Fid[1], &Status, &DirStatus, &DummyPH,
                           &r->sid, &DummyCBS);

		r->Name[0] = strdup(NewName);
		if (!r->Name[0]) {
		    errorCode = ENOMEM;
		    goto Exit;
		}

                r->opcode = CML_Link_OP;
                r->VV[0] = DirStatus.VV;
                r->VV[1] = Status.VV;
		break;

	    case OLDCML_MakeDir_OP:
		RLE_Unpack(&_ptr, _end, OLDCML_MakeDir_PTR, &r->Fid[0],
			   NewName, &Status, &r->Fid[1], &DirStatus,
			   &DummyAllocHost, &r->sid, &DummyCBS);

		r->Name[0] = strdup(NewName);
		if (!r->Name[0]) {
		    errorCode = ENOMEM;
		    goto Exit;
		}

                r->opcode = CML_MakeDir_OP;
                r->VV[0] = DirStatus.VV;
                r->u.u_mkdir.Owner = Status.Owner;
                r->u.u_mkdir.Mode  = Status.Mode;
		break;

	    case OLDCML_SymLink_OP:
		RLE_Unpack(&_ptr, _end, OLDCML_SymLink_PTR, &r->Fid[0],
			   NewName, OldName,
			   &r->Fid[1], &Status, &DirStatus,
                           &DummyAllocHost, &r->sid, &DummyCBS);

		// NewName contains link name, OldName contains link content

		r->Name[0] = strdup(NewName);
		if (!r->Name[0]) {
		    errorCode = ENOMEM;
		    goto Exit;
		}

		r->Name[1] = strdup(OldName);
		if (!r->Name[1]) {
		    free(r->Name[0]);
		    r->Name[0] = NULL;
		    errorCode = ENOMEM;
		    goto Exit;
		}

                r->opcode = CML_SymLink_OP;
                r->VV[0] = DirStatus.VV;
                r->u.u_symlink.Owner = Status.Owner;
                r->u.u_symlink.Mode  = Status.Mode;
		break;

	    case OLDCML_Remove_OP:
		RLE_Unpack(&_ptr, _end, OLDCML_Remove_PTR, &r->Fid[0], OldName,
			   &DirStatus, &Status, &DummyPH, &r->sid, &DummyCBS);

		r->Name[0] = strdup(OldName);
		if (!r->Name[0]) {
		    errorCode = ENOMEM;
		    goto Exit;
		}

                r->opcode = CML_Remove_OP;
                r->VV[0] = DirStatus.VV;
                r->VV[1] = Status.VV;
		break;

	    case OLDCML_RemoveDir_OP:
		RLE_Unpack(&_ptr, _end, OLDCML_RemoveDir_PTR, &r->Fid[0], OldName,
			   &DirStatus, &Status, &DummyPH, &r->sid, &DummyCBS);

		r->Name[0] = strdup(OldName);
		if (!r->Name[0]) {
		    errorCode = ENOMEM;
		    goto Exit;
		}

                r->opcode = CML_RemoveDir_OP;
                r->VV[0] = DirStatus.VV;
                r->VV[1] = Status.VV;
		break;

	    case OLDCML_NewStore_OP:
	    	OLDCML_StoreType Request;
	    	RPC2_Integer Length;
	    	RPC2_Unsigned Mask;
		RLE_Unpack(&_ptr, _end, OLDCML_NewStore_PTR, &r->Fid[0],
			   &Request, &DummyCBS, &Status, &Length, &Mask,
			   &DummyPH, &r->sid, &DummyCBS, 0);

		r->VV[0] = Status.VV;

		if (Request == StoreStatus) {
		    switch(Mask) {
		    case SET_TIME:
			r->u.u_utimes.Date = Status.Date;
			r->opcode = CML_Utimes_OP;
			break;

		    case SET_MODE:
			r->u.u_chmod.Mode = Status.Mode;
			r->opcode = CML_Chmod_OP;
			break;

		    case SET_OWNER:
			r->u.u_chown.Owner = Status.Owner;
			r->opcode = CML_Chown_OP;
			break;

		    default:
			SLog(0,  "ValidateReintegrateParms: bogus store status request (%d)", Mask);
			errorCode = EINVAL;
			goto Exit;
		    }
		} else if (Request == StoreStatusData || Request == StoreData) {
		    r->u.u_store.Length = Length;
		    r->u.u_store.UntranslatedFid = r->Fid[0];
		    r->u.u_store.Inode = RHandle ? RHandle->Inode : 0;
		    r->opcode = CML_Store_OP;
		} else {
		    SLog(0,  "ValidateReintegrateParms: bogus store request (%d)", Request);
		    errorCode = EINVAL;
		    goto Exit;
		}
		break;

	    case OLDCML_Rename_OP:
		RLE_Unpack(&_ptr, _end, OLDCML_Rename_PTR, &r->Fid[0], OldName,
			   &r->Fid[1], NewName, &OldDirStatus,
			   &DirStatus, &Status, &DummyStatus, &DummyPH,
                           &r->sid, &DummyCBS);

		r->Name[0] = strdup(OldName);
		if (!r->Name[0]) {
		    errorCode = ENOMEM;
		    goto Exit;
		}

		r->Name[1] = strdup(NewName);
		if (!r->Name[1]) {
		    free(r->Name[0]);
		    r->Name[0] = NULL;
		    errorCode = ENOMEM;
		    goto Exit;
		}

                r->opcode = CML_Rename_OP;
                r->VV[0] = OldDirStatus.VV;
                r->VV[1] = DirStatus.VV;
                r->VV[2] = Status.VV;
		break;

            /* new, more efficient CML packing */

	    case CML_Create_OP:
		RLE_Unpack(&_ptr, _end, CML_Create_PTR, &r->Fid[0], &r->VV,
			   NewName, &r->u.u_create.Owner,
			   &r->u.u_create.Mode, &r->Fid[1], &r->sid);

		r->Name[0] = strdup(NewName);
		if (!r->Name[0]) {
		    errorCode = ENOMEM;
		    goto Exit;
		}
                break;

	    case CML_Link_OP:
		RLE_Unpack(&_ptr, _end, CML_Link_PTR, &r->Fid[0], &r->VV[0],
			   NewName, &r->Fid[1], &r->VV[1], &r->sid);

		r->Name[0] = strdup(NewName);
		if (!r->Name[0]) {
		    errorCode = ENOMEM;
		    goto Exit;
		}
		break;

	    case CML_MakeDir_OP:
		RLE_Unpack(&_ptr, _end, CML_MakeDir_PTR, &r->Fid[0], &r->VV[0],
			   NewName, &r->Fid[1], &r->u.u_mkdir.Owner,
                           &r->u.u_mkdir.Mode, &r->sid);

		r->Name[0] = strdup(NewName);
		if (!r->Name[0]) {
		    errorCode = ENOMEM;
		    goto Exit;
		}
		break;

	    case CML_SymLink_OP:
		RLE_Unpack(&_ptr, _end, CML_SymLink_PTR, &r->Fid[0],
                           &r->VV[0], NewName, OldName, &r->Fid[1],
                           &r->u.u_symlink.Owner, &r->u.u_symlink.Mode,
                           &r->sid);

		// NewName contains link name, OldName contains link content

		r->Name[0] = strdup(NewName);
		if (!r->Name[0]) {
		    errorCode = ENOMEM;
		    goto Exit;
		}

		r->Name[1] = strdup(OldName);
		if (!r->Name[1]) {
		    free(r->Name[0]);
		    r->Name[0] = NULL;
		    errorCode = ENOMEM;
		    goto Exit;
		}
		break;

	    case CML_BrokenRemove_OP:
		RLE_Unpack(&_ptr, _end, CML_BrokenRemove_PTR, &r->Fid[0],
			   &r->VV[0], OldName, &r->VV[1], &r->sid);

		memset(&r->VV[0], 0, sizeof(ViceVersionVector));
		r->Name[0] = strdup(OldName);
		if (!r->Name[0]) {
		    errorCode = ENOMEM;
		    goto Exit;
		}
                r->opcode = CML_Remove_OP;
		break;

	    case CML_Remove_OP:
		RLE_Unpack(&_ptr, _end, CML_Remove_PTR, &r->Fid[0],
			   &r->VV[0], OldName, &r->VV[1], &r->sid);

		r->Name[0] = strdup(OldName);
		if (!r->Name[0]) {
		    errorCode = ENOMEM;
		    goto Exit;
		}
		break;

	    case CML_RemoveDir_OP:
		RLE_Unpack(&_ptr, _end, CML_RemoveDir_PTR, &r->Fid[0], &r->VV[0],
			   OldName, &r->VV[1], &r->sid);

		r->Name[0] = strdup(OldName);
		if (!r->Name[0]) {
		    errorCode = ENOMEM;
		    goto Exit;
		}
		break;

	    case CML_Store_OP:
		RLE_Unpack(&_ptr, _end, CML_Store_PTR, &r->Fid[0],
			   &r->VV[0], &r->u.u_store.Length,
			   &r->sid);
		r->u.u_store.UntranslatedFid = r->Fid[0];
		r->u.u_store.Inode = RHandle ? RHandle->Inode : 0;
		break;

	    case CML_Utimes_OP:
		RLE_Unpack(&_ptr, _end, CML_Utimes_PTR, &r->Fid[0], &r->VV[0],
			   &r->u.u_utimes.Date, &r->sid);
		break;

	    case CML_Chmod_OP:
		RLE_Unpack(&_ptr, _end, CML_Chmod_PTR, &r->Fid[0], &r->VV[0],
			   &r->u.u_chmod.Mode, &r->sid);
		break;

	    case CML_Chown_OP:
		RLE_Unpack(&_ptr, _end, CML_Chown_PTR, &r->Fid[0], &r->VV[0],
			   &r->u.u_chown.Owner, &r->sid);
		break;

	    case CML_Rename_OP:
                RLE_Unpack(&_ptr, _end, CML_Rename_PTR,
			   &r->Fid[0], &r->VV[0], OldName,
                           &r->Fid[1], &r->VV[1], NewName,
			   &r->VV[2], &r->sid);

		r->Name[0] = strdup(OldName);
		if (!r->Name[0]) {
		    errorCode = ENOMEM;
		    goto Exit;
		}

		r->Name[1] = strdup(NewName);
		if (!r->Name[1]) {
		    free(r->Name[0]);
		    r->Name[0] = NULL;
		    errorCode = ENOMEM;
		    goto Exit;
		}
		break;

	    default:
		SLog(0,  "ValidateReintegrateParms: bogus opcode (%d)", r->opcode);
		errorCode = EINVAL;
		goto Exit;
	}

	SLog(100,  "ValidateReintegrateParms: [E] Op = %d, Mtime = %d",
		r->opcode, r->Mtime);
	list_add(&r->reint_log, rlog->prev);
	rlog_len++;

	/* Translate the Vid for each Fid. */
	for (int i = 0; i < 3; i++) {
	    if (!FID_EQ(&r->Fid[i], &NullFid)) {
		if (!XlateVid(&r->Fid[i].Volume) || r->Fid[i].Volume != *Vid) {
		    errorCode = EINVAL;
		    goto Exit;
		}
	    }
	}

	/* Yield after every so many records. */
	if ((rlog_len & Yield_RLAlloc_Mask) == 0)
	    PollAndYield();
    }

    SLog(2,  "ValidateReintegrateParms: rlog count = %d", rlog_len);

    if (rlog_len < Yield_RLAlloc_Period - 1)
	PollAndYield();

    /* Acquire the volume in exclusive mode. */
    {
	if ((errorCode = GetVolObj(*Vid, volptr, VOL_EXCL_LOCK, 0, ThisHostAddr))) {
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
    if (!list_empty(rlog) && check_reintegration_retry)
    {
	int i = 0;
	rle *r = list_entry(rlog->next, struct rle, reint_log);

	while (i < (*volptr)->nReintegrators) {
	    if ((r->sid.Host == (*volptr)->reintegrators[i].Host) &&
		((long)r->sid.Uniquifier - (long)(*volptr)->reintegrators[i].Uniquifier <= 0)) {
		SLog(0, "ValidateReintegrateParms: Already seen id %u < %u",
		     r->sid.Uniquifier, (*volptr)->reintegrators[i].Uniquifier);
		errorCode = VLOGSTALE;
		index = (*volptr)->reintegrators[i].Uniquifier;
		goto Exit;
	    }
	    i++;
	}
    }

    /* if there is a reintegration handle, sanity check */
    if (RHandle) {
	SLog(0, "ValidateReintegrateParms: Handle = (%d,%d,%d)",
	     RHandle->BirthTime, RHandle->Device, RHandle->Inode);

	/*  Currently, if an RHandle is supplied, the log must consist
	 * of only one new store record.  (The store record is sent
	 * only by old Venii.)  Verify that is the case.  */
        {
		CODA_ASSERT(rlog_len == 1);
		
		struct rle *r = list_entry(rlog->next, struct rle, reint_log);
		CODA_ASSERT(r->opcode == CML_Store_OP);
	}
    }

Exit:
    if (rfile) delete [] rfile;
    if (Index) *Index = (RPC2_Integer) index;
    SLog(10,  "ValidateReintegrateParms: returning %s", ViceErrorMsg(errorCode));
END_TIMING(Reintegrate_ValidateParms);

    if (OldName) delete [] OldName;
    if (NewName) delete [] NewName;

    return(errorCode);
}

/*
 *
 *    Phase II consists of the following steps:
 *      1. Allocating vnodes for "new" objects
 *      2. Parsing the RL entries to create an ordered data 
 *         structure of "participant" Fids
 *      3. Acquiring all corresponding vnodes in Fid-order, 
 *         and under write-locks
 *
 */
static int GetReintegrateObjects(ClientEntry *client, struct dllist_head *rlog, 
				 dlist *vlist, int *blocks, RPC2_Integer *Index)
{
START_TIMING(Reintegrate_GetObjects);
    SLog(10, 	"GetReintegrateObjects: client = %s", client->UserName);

    int errorCode = 0;
    Volume *volptr = 0;
    int index;

    /* Allocate Vnodes for objects created by/during the
       reintegration. */
    /* N.B.  Entries representing these vnodes go on the vlist BEFORE
       those representing vnodes */
    /* which are not created as part of the reintegration.  This is
       needed so that the "lookup" child */
    /* and parent routines can determine when an unsuccessful lookup
       is OK. */

    {
	struct dllist_head *p;
	int count = 0;
        index = 0;
	list_for_each(p, *rlog) {
	    struct rle *r = list_entry(p, struct rle, reint_log);
	    ViceDataType type =
	    	(r->opcode == CML_Create_OP) ? File :
	    	(r->opcode == CML_MakeDir_OP) ? Directory :
	    	(r->opcode == CML_SymLink_OP) ? SymbolicLink : Invalid;

	    if (type != Invalid) {
		int tblocks = 0;
		if ((errorCode = AllocReintegrateVnode(&volptr, vlist,
						       &r->Fid[0],
						       &r->Fid[1],
						       type, client->Id,
						       &tblocks)))
		    goto Exit;
		*blocks += tblocks;
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

    /* 
     Parse the RL entries, creating an ordered data structure of
     Fids.  N.B.  The targets of {unlink,rmdir,rename} are specified
     by <pfid,name> rather than fid, so a lookup in the parent must be
     done to get the target fid.

     Some notes re: lookup:

     1. If the target object is one that was created by an earlier
     reintegration op, the lookup will fail.  This means that failed
     lookup here should not be fatal (but it will be when we do it
     again).

     2. If the parent was itself created during the reintegration,
     then lookup is presently illegal as the parent's directory pages
     (and entries) do not yet exist.  Lookup must *not* be attempted
     until later in this case (which does not create deadlock problems
     because the new object is not yet visible to any other call).
     
     3. If a name is inserted, deleted, and re-inserted in the course
     of reintegration, the binding of name to object will have
     changed.  Thus, we must ALWAYS look up again in
     CheckSemantics.

    */

    SLog(1, "GetReintegrateObjects: AllocReintVnodes done\n");

    {
	struct dllist_head *p;
	int count = 0;
	index = 0;
	list_for_each(p, *rlog) {
	    struct rle *r = list_entry(p, struct rle, reint_log);
	    switch(r->opcode) {
		case CML_Create_OP:
		case CML_Link_OP:
		case CML_MakeDir_OP:
		case CML_SymLink_OP:
		    {
		    vle *v = AddVLE(*vlist, &r->Fid[0]);
		    v->d_inodemod = 1;
		    v->d_reintupdate = 1;

		    if (!FID_EQ(&r->Fid[1], &NullFid))
			(void)AddVLE(*vlist, &r->Fid[1]);
		    }
		    break;

		case CML_Store_OP:
		case CML_Utimes_OP:
		case CML_Chmod_OP:
		case CML_Chown_OP:
		    {
		    vle *v = AddVLE(*vlist, &r->Fid[0]);

		    /* Add file's parent Fid to list for ACL purposes. */
		    /* (Parent MUST already be on list if child was 
		       just alloc'ed!) */
		    if (!v->vptr && !ISDIR(r->Fid[0]))
			if ((errorCode = AddParent(&volptr, vlist, &r->Fid[0])))
			    goto Exit;
		    }
		    break;

		case CML_Remove_OP:
		case CML_RemoveDir_OP:
		    {
		    vle *p_v = AddVLE(*vlist, &r->Fid[0]);

		    /* Add the child object's fid to the vlist 
		       (if it presently exists). */
		    if (!p_v->vptr)
			if ((errorCode = AddChild(&volptr, vlist, &r->Fid[0], 
						  r->Name[0], 0)))
			    goto Exit;

		    p_v->d_inodemod = 1;
		    p_v->d_reintupdate = 1;
		    }
		    break;

		case CML_Rename_OP:
		    {
		    vle *sp_v = AddVLE(*vlist, &r->Fid[0]);

		    /* Add the source object's fid to the vlist 
		       (if it presently exists). */
		    if (sp_v->vptr == 0) 
			if ((errorCode = AddChild(&volptr, vlist, &r->Fid[0], 
						  r->Name[0], 0)))
			    goto Exit;

		    vle *tp_v = AddVLE(*vlist, &r->Fid[1]);

		    /* Add the target object's fid to the vlist (if it presently exists). */
		    if (tp_v->vptr == 0)
			if ((errorCode = AddChild(&volptr, vlist, &r->Fid[1], 
						  r->Name[1], 0)))
			    goto Exit;

		    sp_v->d_reintupdate = 1;
		    sp_v->d_inodemod = 1;
		    tp_v->d_reintupdate = 1;
		    tp_v->d_inodemod = 1;
		    }
		    break;

		default:
		    CODA_ASSERT(FALSE);
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
    SLog(1,  "GetReintegrateObjects: added parent/children, vlist count = %d",
	 vlist->count());

    /* Reacquire all the objects (except those just alloc'ed), this time in FID-order and under write locks. */
    {
	dlist_iterator next(*vlist);
	vle *v;
	int count = 0;
	while ((v = (vle *)next())) {
	    if (v->vptr != 0) continue;

	    SLog(10,  "GetReintegrateObjects: acquiring %s", FID_(&v->fid));
	    if ((errorCode = GetFsObj(&v->fid, &volptr, &v->vptr, 
				     WRITE_LOCK, VOL_NO_LOCK, 0, 0,
				     v->d_inodemod))) {
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
    SLog(10,  "GetReintegrateObjects:	returning %s", ViceErrorMsg(errorCode));
END_TIMING(Reintegrate_GetObjects);
    return(errorCode);
}


/*
 *    Phase III consists of the following steps:
 *      1. Check the semantics of each operation, then perform it 
 *         (delay bulk transfers)
 *      2. Do the bulk transfers
 *
 */
static int CheckSemanticsAndPerform(ClientEntry *client, VolumeId Vid, 
				    VolumeId VSGVolnum,
				    struct dllist_head *rlog, dlist *vlist, 
				    int *blocks, RPC2_Integer *Index) 
{
START_TIMING(Reintegrate_CheckSemanticsAndPerform);
    SLog(10, 	"CheckSemanticsAndPerform: Vid = %x, client = %s",
	     Vid, client->UserName);

    int errorCode = 0;
    Volume *volptr = 0;
    HostTable *he;
    int index;

    /* Get a no-lock reference to the volume for use in this routine (only). */
    if ((errorCode = GetVolObj(Vid, &volptr, VOL_NO_LOCK))) {
        index = -1;
	goto Exit;
    }

    /* Check each operation and perform it. */
    /* Note: the data transfer part of stores is delayed until all other operations have completed. */
	SLog(1, "Starting CheckSemanticsAndPerform for %x", Vid);

    {
	struct dllist_head *p;
	int count = 0;
        index = 0;
	list_for_each(p, *rlog) {
	    struct rle *r = list_entry(p, struct rle, reint_log);
	    switch(r->opcode) {
		case CML_Store_OP:
		    {
		    vle *v = FindVLE(*vlist, &r->Fid[0]);
		    vle	*a_v = 0;	/* ACL object */
		    if (v->vptr->disk.type == vDirectory)
			a_v = v;
		    else {
			ViceFid pFid;
			pFid.Volume = v->fid.Volume;
			pFid.Vnode = v->vptr->disk.vparent;
			pFid.Unique = v->vptr->disk.uparent;
			a_v = FindVLE(*vlist, &pFid);
			CODA_ASSERT(a_v != 0);
		    }
		    int deltablocks = nBlocks(r->u.u_store.Length) - 
		      nBlocks(v->vptr->disk.length);
		    if ((errorCode = CheckStoreSemantics(client, &a_v->vptr,
							 &v->vptr, &volptr, 1,
							 ReintNormalVCmp,
							 &r->VV[0], 0, 0, 0)))
		    {
			goto Exit;
		    }
		    /* Perform. */
		    SLog(9, "CML_Store %s", FID_(&v->fid));

		    if (v->f_finode == 0) {
			/* First StoreData; record pre-reintegration inode. */
			v->f_sinode = v->vptr->disk.node.inodeNumber;
		    }
		    else {
			/* Nth StoreData; discard previous inode. */
			int n = idec(V_device(volptr), v->f_finode,
				     V_parentId(volptr));
			CODA_ASSERT(n == 0);
		    }

		    if (r->u.u_store.Inode) {
			/* inode already allocated, use it. */
			v->f_finode = r->u.u_store.Inode;
		    } else {
			v->f_finode = icreate(V_device(volptr), V_id(volptr),
					      v->vptr->vnodeNumber, v->vptr->disk.uniquifier,
					      v->vptr->disk.dataVersion + 1);
		    }
		    CODA_ASSERT(v->f_finode > 0);
		    /* Bulk transfer is deferred until all ops have been checked/performed. */
		    HandleWeakEquality(volptr, v->vptr, &r->VV[0]);
		    PerformStore(client, VSGVolnum, volptr, v->vptr, v->f_finode,
				 0, r->u.u_store.Length, r->Mtime, &r->sid);
		    ReintPrelimCOP(v, &r->VV[0].StoreId, &r->sid);

		    /* Cancel previous StoreData. */
		    v->f_sid = r->sid;

		    /* Cancel previous truncate. */
		    SLog(3,  "CheckSemanticsAndPerform: cancelling truncate (%s, %d, %d)",
			 FID_(&v->fid), v->f_tinode, v->f_tlength);
		    v->f_tinode = 0;
		    v->f_tlength = 0;

		    if ((errorCode = AdjustDiskUsage(volptr, deltablocks))) {
			goto Exit;
		    }
		    *blocks += deltablocks;
		    }
		    break;

		case CML_Utimes_OP:
		case CML_Chmod_OP:
		case CML_Chown_OP:
/* There is and never was a truncate CML operation the related code is still
 * present, but disabled --JH */
		    {
		    RPC2_Unsigned Mask =
			(r->opcode == CML_Utimes_OP) ?  SET_TIME :
			(r->opcode == CML_Chmod_OP) ?   SET_MODE :
							SET_OWNER;

		    vle *v = FindVLE(*vlist, &r->Fid[0]);
		    vle	*a_v = 0;	/* ACL object */
		    if (v->vptr->disk.type == vDirectory)
			a_v = v;
		    else {
			ViceFid pFid;
			pFid.Volume = v->fid.Volume;
			pFid.Vnode = v->vptr->disk.vparent;
			pFid.Unique = v->vptr->disk.uparent;
			a_v = FindVLE(*vlist, &pFid);
			CODA_ASSERT(a_v != 0);
		    }
#if 0
		    int truncp = 0;
		    int deltablocks = nBlocks(r->u.u_truncate.Length) - 
		      nBlocks(v->vptr->disk.length);

		    /* XXX */
		    /* We don't want to mistakenly assume there is a truncate
		     * on a directory just because the client doesn't know the
		     * length of the replica at the server! This wouldn't be an
		     * issue if our interface provided a "don't set" value for
		     * each attribute, as it should!  -JJK */
		    if (v->vptr->disk.type != vFile)
			r->u.u_truncate.Length = v->vptr->disk.length;
#endif

		    /* Check. */
		    /* The passed in arguments are a bit of a hack, because
		     * only one of SET_TIME, SET_MODE, or SET_OWNER is set the
		     * invalid attributes are ignored --JH */
		    if ((errorCode = CheckSetAttrSemantics(client,
							   &a_v->vptr, &v->vptr, &volptr, 1,
							   ReintNormalVCmp, 0, // r->u.u_truncate.Length,
							   r->u.u_utimes.Date,
							   r->u.u_chown.Owner,
							   r->u.u_chmod.Mode,
							   Mask, &r->VV[0], 0, 0, 0))) {
			goto Exit;
		    }

		    /* Perform. */
		    SLog(9, "CML_SetAttr %s", FID_(&v->fid));
#if 0
		    //if (r->u.u_truncate.Length != v->vptr->disk.length)
		    //   truncp = 1;
		    if (Mask & SET_LENGTH)
			truncp = 1;
#endif

		    Inode c_inode = 0;
		    HandleWeakEquality(volptr, v->vptr, &r->VV[0]);
		    /* The passed in arguments are a bit of a hack, because
		     * only one of SET_TIME, SET_MODE, or SET_OWNER is set the
		     * invalid attributes are ignored --JH */
		    PerformSetAttr(client, VSGVolnum, volptr, v->vptr,
				   0, 0, // r->u.u_truncate.Length,
				   r->u.u_utimes.Date, r->u.u_chown.Owner,
				   r->u.u_chmod.Mode, Mask, &r->sid,
				   &c_inode);
		    ReintPrelimCOP(v, &r->VV[0].StoreId, &r->sid);

		    {
			int opcode = (v->vptr->disk.type == vDirectory && v->d_needsres)
			    ? ResolveViceNewStore_OP : RES_NewStore_OP;
			SLog(5, "Spooling Reintegration newstore record \n");
			if ((errorCode = 
			     SpoolVMLogRecord(vlist, v, volptr,
					      &r->sid, opcode, STSTORE,
					      r->u.u_chown.Owner,
					      r->u.u_chmod.Mode, 
					      0, // r->u.u_store.Author, 
					      r->u.u_utimes.Date, 
					      Mask, r->VV))) {
			    SLog(0, "Reint: Error %d for spool of Store Op\n", errorCode);
			    goto Exit;
			}
		    }

#if 0
/* COW is only invoked by truncate, which we don't use support at the moment
 * (truncates are shipped as new stores in the CML) --JH */
		    /* Note occurrence of COW. */
		    if (c_inode) {
			CODA_ASSERT(v->f_sinode == 0);
			v->f_sinode = c_inode;
			v->f_finode = v->vptr->disk.node.inodeNumber;
			truncp = 0;
		    }

		    /* Note need to truncate later. */
		    if (truncp) {
			SLog(3,  "CheckSemanticsAndPerform: noting truncation (%s, %d, %d), (%d, %d)",
			     FID_(&v->fid), v->f_tinode, v->f_tlength,
			     v->vptr->disk.node.inodeNumber, v->vptr->disk.length);
			v->f_tinode = v->vptr->disk.node.inodeNumber;
			v->f_tlength = v->vptr->disk.length;
		    }

		    if ((errorCode = AdjustDiskUsage(volptr, deltablocks))) {
			goto Exit;
		    }
		    *blocks += deltablocks;
#endif
		    }
		    break;

		case CML_Create_OP:
		    {
			int deltablocks = 0;
			
			/* Check. */
			vle *parent_v = FindVLE(*vlist, &r->Fid[0]);
			vle *child_v = FindVLE(*vlist, &r->Fid[1]);
			errorCode = CheckCreateSemantics(client,
					&parent_v->vptr, &child_v->vptr,
					r->Name[0], &volptr, 1, ReintNormalVCmp,
					&r->VV[0], &NullVV, 0, 0);

#if 0
			if ( errorCode == EEXIST  &&
			     child_v->vptr->disk.length == 0 ) {
				/* don't do anything, go to next log entry */
				errorCode = 0;
				break; 
			}
#endif			
			/* all other errors */
			if ( errorCode )
				goto Exit;
			
			/* directory concurrency check */
			if (VV_Cmp(&Vnode_vv(parent_v->vptr), &r->VV[0]) != VV_EQ)
			    parent_v->d_reintstale = 1;

			/* Perform. */
			SLog(9, "CML_Create %s/%s", FID_(&parent_v->fid), FID_(&child_v->fid));

			HandleWeakEquality(volptr, parent_v->vptr, &r->VV[0]);
			errorCode = PerformCreate(client, VSGVolnum, volptr,
						  parent_v->vptr,
						  child_v->vptr, r->Name[0],
						  r->Mtime, r->u.u_create.Mode,
						  0, &r->sid,
						  &parent_v->d_cinode,
						  &deltablocks);
			CODA_ASSERT(errorCode == 0);

			ReintPrelimCOP(parent_v, &r->VV[0].StoreId, &r->sid);
			ReintPrelimCOP(child_v, &NullSid, &r->sid);

			{
			    int opcode = (parent_v->d_needsres)
				? ResolveViceCreate_OP : RES_Create_OP;
			    UserId owner = child_v->vptr->disk.owner;
			    errorCode = SpoolVMLogRecord(vlist, parent_v,
							 volptr, &r->sid,
							 opcode, r->Name[0], 
							 r->Fid[1].Vnode,
							 r->Fid[1].Unique,
							 owner);
			    if (errorCode) {
				SLog(0, "Reint(CSAP): Error %d during spooling log record for create\n",
				       errorCode);
				goto Exit;
			    }
			}
			*blocks += deltablocks;
		    }
		    break;
		    
		  case CML_Remove_OP:
		    {
		    /* Check. */
		    vle *parent_v = FindVLE(*vlist, &r->Fid[0]);
		    vle *child_v;
		    if ((errorCode = LookupChild(volptr, parent_v->vptr,
						r->Name[0],
						&r->u.u_remove.TgtFid)))
			goto Exit;
		    if (!(child_v = FindVLE(*vlist, &r->u.u_remove.TgtFid))) {
			errorCode = EINVAL;
			goto Exit;
		    }
		    if ((errorCode = CheckRemoveSemantics(client, &parent_v->vptr,
							  &child_v->vptr,
							  r->Name[0],
							  &volptr, 1,
							  ReintNormalVCmp,
							  &r->VV[0],
							  &r->VV[1],
							  0, 0)))
			goto Exit;

		    /* directory concurrency check */
		    if (VV_Cmp(&Vnode_vv(parent_v->vptr), &r->VV[0]) != VV_EQ)
			parent_v->d_reintstale = 1;

		    /* Perform. */
		    SLog(9, "CML_Remove %s/%s", FID_(&parent_v->fid), FID_(&child_v->fid));

		    int tblocks = 0;
		    HandleWeakEquality(volptr, parent_v->vptr, &r->VV[0]);
		    HandleWeakEquality(volptr, child_v->vptr, &r->VV[1]);
		    PerformRemove(client, VSGVolnum, volptr, parent_v->vptr,
				  child_v->vptr, r->Name[0], r->Mtime,
				  0, &r->sid, &parent_v->d_cinode, &tblocks);
		    ReintPrelimCOP(parent_v, &r->VV[0].StoreId, &r->sid);
		    ReintPrelimCOP(child_v, &r->VV[1].StoreId, &r->sid);

		    {
			int opcode = (parent_v->d_needsres)
			  ? ResolveViceRemove_OP : RES_Remove_OP;
			ViceVersionVector *ghostVV = &Vnode_vv(child_v->vptr);	/* ??? -JJK */
			if ((errorCode = SpoolVMLogRecord(vlist, parent_v, volptr, 
							 &r->sid, opcode, 
							 r->Name[0], 
							 r->u.u_remove.TgtFid.Vnode, 
							 r->u.u_remove.TgtFid.Unique, 
							 ghostVV))) {
			    SLog(0, "Reint: Error %d during spool log record for remove op\n",
				   errorCode);
			    goto Exit;
			}
		    }
		    *blocks += tblocks;
		    if (child_v->vptr->delete_me) {
			int deltablocks = -nBlocks(child_v->vptr->disk.length);
			if ((errorCode = AdjustDiskUsage(volptr, deltablocks)))
			    goto Exit;
			*blocks += deltablocks;

			child_v->f_sid = NullSid;
			child_v->f_sinode = child_v->vptr->disk.node.inodeNumber;
			child_v->vptr->disk.node.inodeNumber = 0;

			/* Cancel previous truncate. */
			SLog(3,  "CheckSemanticsAndPerform: cancelling truncate (%s, %d, %d)",
				FID_(&child_v->fid), child_v->f_tinode, child_v->f_tlength);
			child_v->f_tinode = 0;
			child_v->f_tlength = 0;
		    }
		    }
		    break;

		case CML_Link_OP:
		    {
		    int deltablocks = 0;

		    /* Check. */
		    vle *parent_v = FindVLE(*vlist, &r->Fid[0]);
		    vle *child_v = FindVLE(*vlist, &r->Fid[1]);
		    if ((errorCode = CheckLinkSemantics(client, &parent_v->vptr,
							&child_v->vptr,
							r->Name[0],
							&volptr, 1,
							ReintNormalVCmp,
							&r->VV[0], &r->VV[1],
							0, 0)))
			goto Exit;

		    /* directory concurrency check */
		    if (VV_Cmp(&Vnode_vv(parent_v->vptr), &r->VV[0]) != VV_EQ)
			parent_v->d_reintstale = 1;

		    /* Perform. */
		    SLog(9, "CML_Link %s/%s", FID_(&parent_v->fid), FID_(&child_v->fid));

		    HandleWeakEquality(volptr, parent_v->vptr, &r->VV[0]);
		    HandleWeakEquality(volptr, child_v->vptr, &r->VV[1]);
		    errorCode = PerformLink(client, VSGVolnum, volptr,
					    parent_v->vptr, child_v->vptr,
					    r->Name[0], r->Mtime, 0, &r->sid,
					    &parent_v->d_cinode, &deltablocks);
		    CODA_ASSERT(errorCode == 0);
		    ReintPrelimCOP(parent_v, &r->VV[0].StoreId, &r->sid);
		    ReintPrelimCOP(child_v, &r->VV[1].StoreId, &r->sid);

		    {
			int opcode = (parent_v->d_needsres)
			  ? ResolveViceLink_OP
			  : RES_Link_OP;
			if ((errorCode = SpoolVMLogRecord(vlist, parent_v, volptr, 
							 &r->sid, opcode, 
							 r->Name[0], 
							 r->Fid[1].Vnode,
							 r->Fid[1].Unique,
							 &(Vnode_vv(child_v->vptr))))) {
			    SLog(0, "Reint: error %d during spool log record for ViceLink\n",
				   errorCode);
			    goto Exit;
			}
		    }
		    *blocks += deltablocks;
		    }
		    break;

		case CML_Rename_OP:
		    {
		    /* Check. */
		    vle *sd_v = FindVLE(*vlist, &r->Fid[0]);
		    vle *td_v = FindVLE(*vlist, &r->Fid[1]);
		    int SameParent = (sd_v == td_v);
		    vle *s_v;
		    if ((errorCode = LookupChild(volptr, sd_v->vptr, r->Name[0],
						&r->u.u_rename.SrcFid)))
			goto Exit;
		    if (!(s_v = FindVLE(*vlist, &r->u.u_rename.SrcFid))) {
			errorCode = EINVAL;
			goto Exit;
		    }
		    vle *t_v = 0;
		    errorCode = LookupChild(volptr, td_v->vptr, r->Name[1],
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
		    if ((errorCode = CheckRenameSemantics(client, &sd_v->vptr,
							  &td_v->vptr, &s_v->vptr,
							  r->Name[0],
							  TargetExists ? &t_v->vptr : 0,
							  r->Name[1],
							  &volptr, 1,
							  ReintNormalVCmp,
							  &r->VV[0], &r->VV[1],
							  &r->VV[2],
							  &NullVV, /* XXX wrong? */
							  0, 0, 0, 0, 0, 0, 1, 0, vlist)))
			goto Exit;

		    /* directory concurrency checks */
		    if (VV_Cmp(&Vnode_vv(sd_v->vptr), &r->VV[0]) != VV_EQ)
			sd_v->d_reintstale = 1;

		    if (!SameParent &&
			(VV_Cmp(&Vnode_vv(td_v->vptr), &r->VV[1]) != VV_EQ))
			    td_v->d_reintstale = 1;

		    /* Perform. */
		    SLog(9, "CML_Rename %s/%s -> %s/%s",
			 FID_(&sd_v->fid), FID_(&s_v->fid),
			 FID_(&td_v->fid), TargetExists ? FID_(&t_v->fid):"-");

		    HandleWeakEquality(volptr, sd_v->vptr, &r->VV[0]);
		    if (!SameParent)
			HandleWeakEquality(volptr, td_v->vptr, &r->VV[1]);
		    HandleWeakEquality(volptr, s_v->vptr, &r->VV[2]);
		    if (TargetExists)
			HandleWeakEquality(volptr, t_v->vptr, &NullVV); /* XXX wrong? */
		    PerformRename(client, VSGVolnum, volptr, sd_v->vptr, td_v->vptr,
				  s_v->vptr, TargetExists ? t_v->vptr : 0,
				  r->Name[0], r->Name[1],
				  r->Mtime, 0, &r->sid, &sd_v->d_cinode, &td_v->d_cinode,
				  (s_v->vptr->disk.type == vDirectory ? &s_v->d_cinode : 0), NULL);
		    ReintPrelimCOP(sd_v, &r->VV[0].StoreId, &r->sid);
		    if (!SameParent)
			ReintPrelimCOP(td_v, &r->VV[1].StoreId, &r->sid);
		    ReintPrelimCOP(s_v, &r->VV[2].StoreId, &r->sid);
		    if (TargetExists)
			ReintPrelimCOP(t_v, &NullSid, &r->sid); /* XXX wrong? */
		    {
			if (!SameParent) {
			    /* SpoolRenameLogRecord() only allows one opcode, so we must */
			    /* coerce "non-resolve-needing" parent to "resolve-needing"! */
			    if (sd_v->d_needsres && !td_v->d_needsres)
				td_v->d_needsres = 1;
			    if (!sd_v->d_needsres && td_v->d_needsres)
				sd_v->d_needsres = 1;
			}
			int sd_opcode = (sd_v->d_needsres)
			  ? ResolveViceRename_OP : RES_Rename_OP;
			// rvm resolution is on 
			if ((errorCode = SpoolRenameLogRecord(sd_opcode, vlist, 
							     s_v, t_v, sd_v,
                                                             td_v, volptr,
							     r->Name[0],
							     r->Name[1],
							     &r->sid))) {
				SLog(0, "Reint: Error %d during spool log record for rename\n",
				     errorCode);
				goto Exit;
			    }
		    }

		    if (TargetExists && t_v->vptr->delete_me) {
			int deltablocks = -nBlocks(t_v->vptr->disk.length);
			if ((errorCode = AdjustDiskUsage(volptr, deltablocks)))
			    goto Exit;
			*blocks += deltablocks;

			if (t_v->vptr->disk.type != vDirectory) {
			    t_v->f_sid = NullSid;
			    t_v->f_sinode = t_v->vptr->disk.node.inodeNumber;
			    t_v->vptr->disk.node.inodeNumber = 0;
			}
		    }
		    }
		    break;

		case CML_MakeDir_OP:
		    {
		    int deltablocks = 0;

		    /* Check. */
		    vle *parent_v = FindVLE(*vlist, &r->Fid[0]);
		    vle *child_v = FindVLE(*vlist, &r->Fid[1]);
		    if ((errorCode = CheckMkdirSemantics(client, &parent_v->vptr,
							 &child_v->vptr,
							 r->Name[0],
							 &volptr, 1,
							 ReintNormalVCmp,
							 &r->VV[0],
							 &NullVV,
							 0, 0)))
			goto Exit;

		    /* directory concurrency check */
		    if (VV_Cmp(&Vnode_vv(parent_v->vptr), &r->VV[0]) != VV_EQ)
			parent_v->d_reintstale = 1;

		    /* Perform. */
		    SLog(9, "CML_MakeDir %s/%s", FID_(&parent_v->fid), FID_(&child_v->fid));

		    HandleWeakEquality(volptr, parent_v->vptr, &r->VV[0]);
		    errorCode = PerformMkdir(client, VSGVolnum, volptr,
					     parent_v->vptr, child_v->vptr,
					     r->Name[0], r->Mtime,
					     r->u.u_mkdir.Mode, 0, &r->sid,
					     &parent_v->d_cinode,
					     &deltablocks);
		    CODA_ASSERT(errorCode == 0);
		    ReintPrelimCOP(parent_v, &r->VV[0].StoreId, &r->sid);
		    ReintPrelimCOP(child_v, &NullSid, &r->sid);

		    {
			int p_opcode = (parent_v->d_needsres)
			  ? ResolveViceMakeDir_OP : RES_MakeDir_OP;
			int c_opcode = RES_MakeDir_OP;
			UserId owner = child_v->vptr->disk.owner;
			errorCode = SpoolVMLogRecord(vlist, parent_v, volptr,
						     &r->sid, p_opcode,
						     r->Name[0],
						     r->Fid[1].Vnode,
						     r->Fid[1].Unique,
						     owner);
			if (errorCode) {
				SLog(0, "Reint: Error %d during SpoolVMLogRecord for parent in MakeDir_OP\n", errorCode);
				goto Exit;
			}
			errorCode = SpoolVMLogRecord(vlist, child_v, volptr,
						     &r->sid, c_opcode, ".", 
						     r->Fid[1].Vnode,
						     r->Fid[1].Unique,
						     owner);
			if (errorCode) {
				SLog(0, "Reint:  error %d during SpoolVMLogRecord for child in MakeDir_OP\n", errorCode);
				goto Exit;
			}
		    }
		    *blocks += deltablocks;
		    }
		    break;

		case CML_RemoveDir_OP:
		    {
		    /* Check. */
		    vle *parent_v = FindVLE(*vlist, &r->Fid[0]);
		    vle *child_v;
		    if ((errorCode = LookupChild(volptr, parent_v->vptr,
						r->Name[0],
						&r->u.u_rmdir.TgtFid)))
			goto Exit;
		    if (!(child_v = FindVLE(*vlist, &r->u.u_rmdir.TgtFid))) {
			errorCode = EINVAL;
			goto Exit;
		    }
		    if ((errorCode = CheckRmdirSemantics(client, &parent_v->vptr,
							 &child_v->vptr,
							 r->Name[0],
							 &volptr, 1,
							 ReintNormalVCmp,
							 &r->VV[0], &r->VV[1],
							 0, 0)))
			goto Exit;

		    /* directory concurrency check */
		    if (VV_Cmp(&Vnode_vv(parent_v->vptr), &r->VV[0]) != VV_EQ)
			parent_v->d_reintstale = 1;

		    /* Perform. */
		    SLog(9, "CML_RemoveDir %s/%s", FID_(&parent_v->fid), FID_(&child_v->fid));

		    int tblocks = 0;
		    HandleWeakEquality(volptr, parent_v->vptr, &r->VV[0]);
		    HandleWeakEquality(volptr, child_v->vptr, &r->VV[1]);
		    PerformRmdir(client, VSGVolnum, volptr, parent_v->vptr,
				 child_v->vptr, r->Name[0], r->Mtime,
				 0, &r->sid, &parent_v->d_cinode, &tblocks);
		    ReintPrelimCOP(parent_v, &r->VV[0].StoreId, &r->sid);
		    ReintPrelimCOP(child_v, &r->VV[1].StoreId, &r->sid);

		    {
			int opcode = (parent_v->d_needsres)
			  ? ResolveViceRemoveDir_OP : RES_RemoveDir_OP;
			if ((errorCode = SpoolVMLogRecord(vlist, parent_v, 
							 volptr, &r->sid, 
							 opcode, 
							 r->Name[0],
							 r->u.u_rmdir.TgtFid.Vnode,
							 r->u.u_rmdir.TgtFid.Unique,
							 VnLog(child_v->vptr), 
							 &(Vnode_vv(child_v->vptr).StoreId),
							 &(Vnode_vv(child_v->vptr).StoreId)))) {
			    SLog(0, 
				   "Reint(CSAP): Error %d during SpoolVMLogRecord for RmDir_OP\n",
				   errorCode);
			    goto Exit;
			}
		    }
		    *blocks += tblocks;
		    CODA_ASSERT(child_v->vptr->delete_me);
		    int deltablocks = -nBlocks(child_v->vptr->disk.length);
		    if ((errorCode = AdjustDiskUsage(volptr, deltablocks)))
			goto Exit;
		    *blocks += deltablocks;
		    }
		    break;

		case CML_SymLink_OP:
		    {
		    int deltablocks = 0;

		    /* Check. */
		    vle *parent_v = FindVLE(*vlist, &r->Fid[0]);
		    vle *child_v = FindVLE(*vlist, &r->Fid[1]);
		    if ((errorCode = CheckSymlinkSemantics(client, &parent_v->vptr,
							   &child_v->vptr,
							   r->Name[0],
							   &volptr, 1,
							   ReintNormalVCmp,
							   &r->VV[0],
							   &NullVV,
							   0, 0)))
			goto Exit;

		    /* directory concurrency check */
		    if (VV_Cmp(&Vnode_vv(parent_v->vptr), &r->VV[0]) != VV_EQ)
			parent_v->d_reintstale = 1;

		    /* Perform. */
		    SLog(9, "CML_SymLink %s/%s", FID_(&parent_v->fid), FID_(&child_v->fid));

		    CODA_ASSERT(child_v->f_finode == 0);
		    child_v->f_finode = icreate(V_device(volptr), V_id(volptr),
					       child_v->vptr->vnodeNumber,
					       child_v->vptr->disk.uniquifier, 1);
		    CODA_ASSERT(child_v->f_finode > 0);
		    int linklen = strlen(r->Name[1]);

		    int n = iwrite(V_device(volptr), child_v->f_finode,
				   V_parentId(volptr), 0, r->Name[1], linklen);
		    CODA_ASSERT(n == linklen);

		    HandleWeakEquality(volptr, parent_v->vptr, &r->VV[0]);
		    errorCode = PerformSymlink(client, VSGVolnum, volptr,
					       parent_v->vptr, child_v->vptr,
					       r->Name[0], child_v->f_finode,
					       linklen, r->Mtime,
					       r->u.u_symlink.Mode, 0, &r->sid,
					       &parent_v->d_cinode,
					       &deltablocks);
		    CODA_ASSERT(errorCode == 0);
		    ReintPrelimCOP(parent_v, &r->VV[0].StoreId, &r->sid);
		    ReintPrelimCOP(child_v, &NullSid, &r->sid);

		    {
			int opcode = (parent_v->d_needsres)
			  ? ResolveViceSymLink_OP : RES_SymLink_OP;
			UserId owner = child_v->vptr->disk.owner;
			errorCode = SpoolVMLogRecord(vlist, parent_v, volptr,
						     &r->sid, opcode,
						     r->Name[0],
						     r->Fid[1].Vnode,
						     r->Fid[1].Unique,
						     owner);
			if (errorCode) {
			    SLog(0, "Reint: Error %d during spool log record for ViceSymLink\n", errorCode);
			    goto Exit;
			}
		    }
		    *blocks += deltablocks;
		    }
		    break;

		default:
		    CODA_ASSERT(FALSE);
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
    SLog(1, "Starting  BulkTransfers for %x", Vid);

    /* Make sure we don't get killed by accident */
    he = client->VenusId;
    ObtainReadLock(&he->lock);
    if (!he || he->id == 0) {
	errorCode = RPC2_FAIL;	/* ??? -JJK */
	index = -1;
	goto LockExit;
    }

    /* Now do bulk transfers. */
    {
	struct dllist_head *p;
	list_for_each(p, *rlog) {
	    struct rle *r = list_entry(p, struct rle, reint_log);
            if (r->opcode == CML_Store_OP) {
                if (r->u.u_store.Inode)	/* data already here */
                    continue;

                /* Poll and yield here. */
                /* This wouldn't be necessary if we had multiple CB connections to the client! */
                PollAndYield();

                vle *v = FindVLE(*vlist, &r->Fid[0]);
                if (!SID_EQ(v->f_sid, r->sid))
                    /* Don't fetch intermediate versions. */
                    continue;

                SE_Descriptor sid;
                memset(&sid, 0, sizeof(SE_Descriptor));
                sid.Tag = client->SEType;
                sid.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
                sid.Value.SmartFTPD.SeekOffset = 0;
                sid.Value.SmartFTPD.hashmark = (SrvDebugLevel > 2 ? '#' : '\0');
                sid.Value.SmartFTPD.ByteQuota = r->u.u_store.Length;
                sid.Value.SmartFTPD.Tag = FILEBYFD;

                {
                    int fd = iopen(V_device(volptr), v->f_finode,
                                   O_WRONLY | O_TRUNC);

                    sid.Value.SmartFTPD.FileInfo.ByFD.fd = fd;

                    errorCode = CallBackFetch(he->id,
                                              &r->u.u_store.UntranslatedFid,
                                              &sid);
                    close(fd);
                }

                if ( errorCode < RPC2_ELIMIT ) {
                    /* We have to release the lock, because
                     * CLIENT_CleanUpHost wants to grab an exclusive lock */
                    ReleaseReadLock(&he->lock);
                    CLIENT_CleanUpHost(he);
                    index = -1;
                    goto Exit;
                }

                if (errorCode) {
                    index = -1;
                    goto LockExit;
                }

                RPC2_Integer len = sid.Value.SmartFTPD.BytesTransferred;
                if (r->u.u_store.Length != len) {
                    SLog(0,  "CBFetch: length discrepancy (%d : %d), (%s), %s %s.%d",
                         r->u.u_store.Length, len, FID_(&v->fid),
                         client->UserName,
                         inet_ntoa(client->VenusId->host),
                         ntohs(client->VenusId->port));
                    errorCode = EINVAL;
                    index = -1;
                    goto LockExit;
                }

                SLog(2,  "CBFetch: transferred %d bytes (%s)",
                     r->u.u_store.Length, FID_(&v->fid));
            }
	}
    }

    /* If we still have the hosttable entry locked, release it now */
LockExit:
    ReleaseReadLock(&he->lock);

Exit:
    if (Index)  
	    *Index = (RPC2_Integer) index;
    PutVolObj(&volptr, VOL_NO_LOCK);
    SLog(10,  
	   "CheckSemanticsAndPerform: returning %s", ViceErrorMsg(errorCode));
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
static void PutReintegrateObjects(int errorCode, Volume *volptr,
				  struct dllist_head *rlog, 
				  dlist *vlist, int blocks,
				  RPC2_Integer OutOfOrder, ClientEntry *client, 
				  RPC2_Integer MaxDirs, RPC2_Integer *NumDirs,
				  ViceFid *StaleDirs, RPC2_CountedBS *OldVS, 
				  RPC2_Integer *NewVS, 
				  CallBackStatus *VCBStatus) 
{
START_TIMING(Reintegrate_PutObjects);
    SLog(10, 	"PutReintegrateObjects: Vid = %x, errorCode = %d",
	     volptr ? V_id(volptr) : 0, errorCode);

    ViceStoreId sid = { 0, };
    struct dllist_head *p;
    int count = 0;

    /* Pre-transaction: release RL, then update version state. */
    for (p = rlog->next; p != rlog;) {
	struct rle *r = list_entry(p, struct rle, reint_log);
	p = p->next;
	list_del(&r->reint_log);

	if (list_empty(rlog)) 	/* last one -- save sid */
	    sid = r->sid;
	if (r->Name[0]) free(r->Name[0]);
	if (r->Name[1]) free(r->Name[1]);

	free(r);

	/* Yield after every so many records. */
	count++;
	if ((count & Yield_RLDealloc_Mask) == 0)
	    PollAndYield();
    }
    if (count < Yield_RLDealloc_Period - 1)
	PollAndYield();

    if (errorCode == 0 && vlist && volptr) {
	GetMyVS(volptr, OldVS, NewVS);

	dlist_iterator next(*vlist);
	vle *v;
	while ((v = (vle *)next())) {
	    if ((v->vptr->disk.type != vDirectory || v->d_reintupdate) &&
		!v->vptr->delete_me) {
		ReintFinalCOP(v, volptr, NewVS);
	    } else {
		SLog(2, "PutReintegrateObjects: un-mutated or deleted fid %s",
		       FID_(&v->fid));
	    }

	    /* write down stale directory fids */
	    if (StaleDirs && v->vptr->disk.type == vDirectory && 
		(v->d_reintstale || v->d_needsres)) { /* compatibility check */
		ViceFid fid = v->fid;
		fid.Volume = V_groupId(volptr);
		SLog(0, "PutReintegrateObjects: stale directory fid %s, num %d, max %d",
		     FID_(&fid), *NumDirs, MaxDirs);
		if (*NumDirs < MaxDirs) {
		    StaleDirs[(*NumDirs)] = v->fid;
		    /* send back replicated ID */
		    StaleDirs[(*NumDirs)++].Volume = V_groupId(volptr);
		}
	    }
        }
	SetVSStatus(client, volptr, NewVS, VCBStatus);
    }

    /* Release the objects. */
    if (volptr) {
	Volume *tvolptr = 0;
	int ret = GetVolObj(V_id(volptr), &tvolptr, VOL_NO_LOCK);
	CODA_ASSERT(ret == 0);

	PutObjects(errorCode, tvolptr, VOL_NO_LOCK, vlist, blocks, 1);

	/* save the sid of the last successfully reintegrated record */
	if (errorCode == 0 && !OutOfOrder) {
	    int i = 0;

	    /* replace the entry for this client if one exists */
	    for (i = 0; i < volptr->nReintegrators; i++)
		if (volptr->reintegrators[i].Host == sid.Host)
		    break;

	    /* no entry for this client, make one */
	    if (i == volptr->nReintegrators) {
		/* (re)allocate space if necessary */
		if ((i % VNREINTEGRATORS) == 0)	{ 
		    ViceStoreId *newlist = (ViceStoreId *) 
			malloc(sizeof(ViceStoreId) * (i + VNREINTEGRATORS));
		    if (volptr->reintegrators) {
			memcpy(newlist, volptr->reintegrators,
			       sizeof(ViceStoreId) * i);
			free(volptr->reintegrators);
		    }
		    volptr->reintegrators = newlist;
		}
		volptr->nReintegrators++;
	    }
	    volptr->reintegrators[i] = sid;
	}
    }

    /* Finally, release the exclusive-mode volume reference acquired at the beginning. */
    PutVolObj(&volptr, VOL_EXCL_LOCK);

    SLog(10,  "PutReintegrateObjects: returning %s", ViceErrorMsg(0));
END_TIMING(Reintegrate_PutObjects);
}


static int AllocReintegrateVnode(Volume **volptr, dlist *vlist, 
				 ViceFid *pFid, ViceFid *cFid,
                                 ViceDataType Type, UserId ClientId,
                                 int *blocks) 
{
    int errorCode = 0;
    Vnode *vptr = 0;
    *blocks = 0;

    /* Get volptr. */
    /* We assume that volume has already been locked in exclusive mode! */
    if (*volptr == 0) {
	int ret = GetVolObj(pFid->Volume, volptr, VOL_NO_LOCK, 0, 0);
	CODA_ASSERT(ret == 0);
    }

    /* Allocate/Retrieve the vnode. */
    if ((errorCode = AllocVnode(&vptr, *volptr, Type, cFid,
				pFid, ClientId, 1, blocks)))
	goto Exit;

    /* Create a new vle for this vnode and add it to the vlist. */
    /*    CODA_ASSERT(FindVLE(*vlist, cFid) == 0);*/
    vle *v; v = AddVLE(*vlist, cFid);
    CODA_ASSERT(v->vptr == 0);
    v->vptr = vptr;
    if (v->vptr->disk.type == vDirectory)
	    v->d_inodemod = 1;

Exit:
    /* Sanity check. */
    if (errorCode)
	CODA_ASSERT(vptr == 0);

    SLog(2,  "AllocReintegrateVnode returns %s", ViceErrorMsg(errorCode));
    return(errorCode);
}

/* here we ALWAYS add the directory inode of the parent to the list of
   objects.  We also add the child's inode to deal with rename and
   makedir.  */

int AddChild(Volume **volptr, dlist *vlist, ViceFid *Did, 
	     char *Name, int IgnoreInc) 
{
    int errorCode = 0;
    Vnode *vptr = 0;
    struct vle *vle;

    /* Get volptr. */
    /* We assume that volume has already been locked in exclusive mode! */
    if (*volptr == 0) {
	int ret = GetVolObj(Did->Volume, volptr, VOL_NO_LOCK, 0, 0);
	CODA_ASSERT(ret == 0);
    }

    /* Parent must NOT have just been alloc'ed, else this will deadlock! */
    /* Notice that the vlist->d_inodemod field must be 1 or we lose 
       refcounts on this directory 
    */
    if ((errorCode = GetFsObj(Did, volptr, &vptr, READ_LOCK, 
			     VOL_NO_LOCK, IgnoreInc, 0, 1)))
	goto Exit;

    /* Look up the child, and add a vle if found. */
    ViceFid Fid;
    errorCode = LookupChild(*volptr, vptr, Name, &Fid);
    switch(errorCode) {
	case 0:
	    vle = AddVLE(*vlist, &Fid);
	    if (ISDIR(Fid))
		vle->d_inodemod = 1;
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
	VN_PutDirHandle(vptr);
	VPutVnode(&fileCode, vptr);
	CODA_ASSERT(fileCode == 0);
    }

    SLog(2,  "AddChild returns %s", ViceErrorMsg(errorCode));
    return(errorCode);
}


int LookupChild(Volume *volptr, Vnode *vptr, char *Name, ViceFid *Fid) 
{
	int errorCode = 0;

    	PDirHandle dh;

	dh = DC_DC2DH(vptr->dh);
	errorCode = DH_Lookup(dh, Name, Fid, CLU_CASE_SENSITIVE);
	if ( errorCode != 0) {
		errorCode = ENOENT;
		goto Exit;
	}
	Fid->Volume = V_id(volptr);

 Exit:
	SLog(10,  
	       "LookupChild returns %s", ViceErrorMsg(errorCode));
	return(errorCode);
}


static int AddParent(Volume **volptr, dlist *vlist, ViceFid *Fid) {
    int errorCode = 0;
    Vnode *vptr = 0;

    /* Get volptr. */
    /* We assume that volume has already been locked in exclusive mode! */
    if (*volptr == 0) {
	int ret = GetVolObj(Fid->Volume, volptr, VOL_NO_LOCK, 0, 0);
	CODA_ASSERT(ret == 0);
    }

    /* Child must NOT have just been alloc'ed, else this will deadlock! */
    if ((errorCode = GetFsObj(Fid, volptr, &vptr, READ_LOCK, 
			     VOL_NO_LOCK, 0, 0, 0)))
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
	CODA_ASSERT(fileCode == 0);
    }

    SLog(2,  "AddParent returns %s", ViceErrorMsg(errorCode));
    return(errorCode);
}


/* Makes no version check for directories. */
/* Permits only Strong and Weak Equality for files. */
static int ReintNormalVCmp(int ReplicatedOp, VnodeType type, 
			   void *arg1, void *arg2) 
{
    CODA_ASSERT(ReplicatedOp == 1);

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
	    CODA_ASSERT(0);
    }
    return 0;
}


/* This probably ought to be folded into the PerformXXX routines!  -JJK */
static void ReintPrelimCOP(vle *v, const ViceStoreId *OldSid,
			   const ViceStoreId *NewSid) 
{
    ViceStoreId *current = &Vnode_vv(v->vptr).StoreId;

    /* Directories which are not identical to "old" contents MUST be
       stamped with unique Sid at end! */
    if (v->vptr->disk.type == vDirectory && !SID_EQ(*current, *OldSid)) {
	SLog(10,  "ReintPrelimCOP: %s needsres (sid %x.%x != oldsid %x.%x)", FID_(&v->fid),
	     current->Host, current->Uniquifier, OldSid->Host, OldSid->Uniquifier);
	v->d_needsres = 1;
    }

    *current = *NewSid;
}


static void ReintFinalCOP(vle *v, Volume *volptr, RPC2_Integer *VS) 
{
	ViceStoreId *FinalSid;
	ViceStoreId UniqueSid;
	if (v->vptr->disk.type == vDirectory && v->d_needsres && 
	    AllowResolution && V_RVMResOn(volptr)) {
		AllocStoreId(&UniqueSid);
		FinalSid = &UniqueSid;
	} else {
		FinalSid = &Vnode_vv(v->vptr).StoreId;
	}

	/* 1. Record COP1 (for final update). */
	NewCOP1Update(volptr, v->vptr, FinalSid, VS);

	/* 2. Record COP2 pending (for final update). */
	/* Note that for directories that "need-resolved", 
	   (1) there is no point in recording a COP2 pending 
	   (since it would be ignored), and 
	   (2) we must log a ResolveNULL_OP so that resolution 
	   works correctly. 
	*/
	if (v->vptr->disk.type == vDirectory && v->d_needsres &&
	    AllowResolution && V_RVMResOn(volptr))
	{
	    int ret = SpoolVMLogRecord(NULL, v, volptr, FinalSid,
				       ResolveNULL_OP, 0);
	    CODA_ASSERT(ret == 0);
	}
	else {
	    AddPairToCopPendingTable(FinalSid, &v->fid);
	}
}


/*  *****  Gross stuff for packing/unpacking RPC arguments  *****  */

/* Unpack a ReintegrationLog Entry. */
/* Patterned after code in MRPC_MakeMulti(). */
static void RLE_Unpack(PARM **ptr, char *_end, ARG *ArgTypes ...) 
{
	SLog(100,  "RLE_Unpack: ptr = %x, ArgTypes = %x", ptr, ArgTypes);
	
	va_list ap;
	va_start(ap, ArgTypes);
	for (ARG *a_types = ArgTypes; a_types->mode != C_END; a_types++) {
		PARM *args = (va_arg(ap, PARM*));
		if (a_types->mode != IN_MODE && a_types->mode != IN_OUT_MODE)
			continue;
		
/*
  SLog(100,  "\ta_types = [%d %d %d %x], ptr = (%x %x %x), args = %x ",
  a_types->mode, a_types->type, a_types->size, a_types->field,
  ptr, *ptr, **ptr, args);
*/
		
		/* Extra level of indirection, since unpack routines are from MRPC. */
		/* something with va_args needed this.. clean this up later!!
		   -- Troy <hozer@drgw.net> */
		PARM *tmp = (PARM *)&args;
		PARM *xargs = (PARM *)&tmp;
		
		/*
		  if (a_types->type == RPC2_COUNTEDBS_TAG) {
		  SLog(100,  "\t&xargs->cbsp[0]->SeqLen = %x, * = %d, ntohl((*_ptr)->integer) = %d",
		  &(xargs->cbsp[0]->SeqLen), xargs->cbsp[0]->SeqLen, ntohl((*(ptr))->integer));
		  SLog(100,  "\t&xargs->cbsp[0]->SeqBody = %x, * = %x, (*_ptr) = %x",
		  &(xargs->cbsp[0]->SeqBody), xargs->cbsp[0]->SeqBody, *(ptr + 1));
		  }
		*/
		
		if (a_types->type == RPC2_STRUCT_TAG) {
			PARM *str = (PARM *)xargs->structpp[0];
			unpack_struct(a_types, &str, (PARM **)ptr, _end, 0);
		}
		else {
			if (a_types->type == RPC2_STRING_TAG)
				/* Temporary!  Fix an "extra dereference" bug in unpack!  -JJK */
				unpack(a_types, (PARM *)&xargs, (PARM **)ptr, _end, 0);
			else
				unpack(a_types, xargs, (PARM **)ptr, _end, 0);
		}
	}
	
	va_end(ap);
	SLog(100,  "RLE_Unpack: returning");
}


/* 
 * Extract and validate the reintegration handle.  Handle errors are
 * propagated back to the client as EBADF.
 */
static int ValidateRHandle(VolumeId Vid, ViceReintHandle *RHandle)
{
    SLog(10,  "ValidateRHandle: Vid = %x", Vid);

    /* get the volume and sanity check */
    int error, count, ix;
    Volume *volptr;
    VolumeId rwVid = Vid;

    if (!XlateVid(&rwVid, &count, &ix)) {
	SLog(1, "ValidateRHandle: Couldn't translate VSG %x", 
	       Vid);
	error = EINVAL;
	goto Exit;
    }

    SLog(9,  "ValidateRHandle: Going to get volume %x pointer", 
	   rwVid);
    volptr = VGetVolume((Error *) &error, rwVid);
    SLog(1, "ValidateRHandle: Got volume %x: error = %d ", 
	   rwVid, error);

    if (error){
	SLog(0,  "ValidateRHandle, VgetVolume error %s", 
	       ViceErrorMsg((int)error));
	/* should we check to see if we must do a putvolume here */
	goto Exit;
    }

    /* check device */
    if ((long)V_device(volptr) != RHandle->Device) {
	SLog(0, "ValidateRHandle: Bad device (%d,%d)",
	       V_device(volptr), RHandle->Device);
	error = EBADF;
	goto FreeObj;
    }

    /* check age of handle */
    if ((long)StartTime != RHandle->BirthTime) {
	SLog(0, "ValidateRHandle: Old handle (%d,%d,%d)",
	       RHandle->BirthTime, RHandle->Device, RHandle->Inode);
	error = EBADF;
	goto FreeObj;
    }

 FreeObj:
    VPutVolume(volptr);

 Exit:
    return(error);
}

