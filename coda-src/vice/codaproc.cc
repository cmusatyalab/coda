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
/*  codaproc.c	- File Server Coda specific routines			*/
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

#include <lwp.h>
#include <rpc2.h>
#include <se.h>
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
#include <pdlist.h>
#include <reslog.h>
#include <ops.h>
#include <timing.h>

#define	EMPTYDIRBLOCKS	    2
#define NEWVNODEINODE -1

class TreeRmBlk;
class rmblk;

/* *****  Exported variables  ***** */

ViceVersionVector NullVV = {{0, 0, 0, 0, 0, 0, 0, 0}, {0, 0}, 0};

/* globals */
int OngoingRepairs = 0;

/* external routines */
extern int CmpPlus(AL_AccessEntry *a, AL_AccessEntry *b);

extern int CmpMinus(AL_AccessEntry *a, AL_AccessEntry *b);

extern int CheckWriteMode(ClientEntry *, Vnode *);

extern void ChangeDiskUsage(Volume *, int);

extern int FetchFileByName(RPC2_Handle, char *, ClientEntry *);

extern long FileResolve(res_mgrpent *, ViceFid *, ViceVersionVector **);

int GetSubTree(ViceFid *, Volume *, dlist *);

int PerformTreeRemoval(struct DirEntry *, void *);


/* ***** Private routines  ***** */
static int FidSort(ViceFid *);
void UpdateVVs(ViceVersionVector *, ViceVersionVector *, ViceVersionVector *);

static int PerformDirRepair(ClientEntry *, vle *, Volume *, 
			     VolumeId , ViceStatus *, ViceStoreId *,
			     struct repair *, int, dlist *, Rights, Rights, int *);

static int CheckDirRepairSemantics(vle *, dlist *, Volume *, ViceStatus *, 
				    ClientEntry *, Rights *, Rights *, int , 
				    struct repair *);

static int CheckRepairSetNACLSemantics(ClientEntry *, Vnode *, Volume *, char *,int);

static int CheckRepairSetACLSemantics(ClientEntry *, Vnode *, Volume *, char *, int);

static int CheckRepairACLSemantics(ClientEntry *, Vnode *, Volume *, 
				    AL_AccessList **, int *);

static int CheckFileRepairSemantics(vle *, vle *, Volume *, ViceStatus *, 
				     ClientEntry *, Rights *, Rights *);

static int CheckRepairSemantics(vle *, Volume *, dlist *, ViceStatus *, 
				 ClientEntry *, Rights *, Rights *, int , 
				 struct repair *);

static int PerformFileRepair(vle *, Volume *, VolumeId, ViceStatus *, 
			      ViceStoreId *, Rights, Rights);

static int GetMyRepairList(ViceFid *, struct listhdr *, int , 
			    struct repair **, int *);


static int CheckTreeRemoveSemantics(ClientEntry *, Volume *, 
				     ViceFid *, dlist *);

static int getFids(dlist *, char *, long , long );


static int GetRepairObjects(Volume *, vle *, dlist *, 
			     struct repair *, int );
static int GetResFlag(VolumeId );

static void COP2Update(Volume *, Vnode *, ViceVersionVector *, vmindex * =NULL);


/*
  ViceProbe: Empty routine - simply return the probe 
*/

long FS_ViceProbe(RPC2_Handle cid, RPC2_CountedBS *Vids, RPC2_CountedBS *VVs) 
{
    int errorCode = 0;

    VVs->SeqLen = 0;

    SLog(1,  "ViceProbe, ");

    return(errorCode);
}

const int MaxFidAlloc = 32;

/*
  ViceAllocFids: Allocated a range of fids
*/
long FS_ViceAllocFids(RPC2_Handle cid, VolumeId Vid,
		    ViceDataType Type, ViceFidRange *Range,
		    RPC2_Unsigned AllocHost, RPC2_CountedBS *PiggyBS) {
    long errorCode = 0;
    VolumeId VSGVolnum = Vid;
    Volume *volptr = 0;
    ClientEntry *client = 0;
    int	stride, ix;

START_TIMING(AllocFids_Total);
    SLog(1,  "ViceAllocFids: Vid = %x, Type = %d, Count = %d, AllocHost = %x",
	     Vid, Type, Range->Count, AllocHost);

    /* Validate parameters. */
    {
	if ((PiggyBS->SeqLen > 0) && (errorCode = FS_ViceCOP2(cid, PiggyBS)))
	    goto FreeLocks;

	/* Retrieve the client handle. */
	if ((errorCode = RPC2_GetPrivatePointer(cid, (char **)&client)) != RPC2_SUCCESS)
	    goto FreeLocks;
	if (!client) {
	    errorCode = EINVAL;
	    goto FreeLocks;
	}

	/* Translate the GroupVol into this host's RWVol. */
	/* This host's {stride, ix} are returned as side effects; they will be needed in the Alloc below. */
	if (!XlateVid(&Vid, &stride, &ix)) {
	    SLog(0,  "ViceAllocFids: XlateVid (%x) failed", VSGVolnum);
	    errorCode = EINVAL;
	    goto FreeLocks;
	}

	switch(Type) {
	    case File:
	    case Directory:
	    case SymbolicLink:
		break;

	    case Invalid:
	    default:
		errorCode = EINVAL;
		goto FreeLocks;
	}
    }

    /* Only AllocHost actually allocates the fids. */
    if (ThisHostAddr == AllocHost) {
	/* Get the volume. */
	if (errorCode = GetVolObj(Vid, &volptr, VOL_NO_LOCK, 0, 0)) {
	    SLog(0,  "ViceAllocFids: GetVolObj error %s", ViceErrorMsg((int) errorCode));
	    goto FreeLocks;
	}

	/* Allocate a contiguous range of fids. */
	if (Range->Count > MaxFidAlloc)
	    Range->Count = MaxFidAlloc;
	if (errorCode = VAllocFid(volptr, Type, Range, stride, ix)) {
	    SLog(0,  "ViceAllocFids: VAllocVnodes error %s", ViceErrorMsg((int) errorCode));
	    goto FreeLocks;
	}
    }
    else {
	Range->Count = 0;
    }

FreeLocks:
    PutVolObj(&volptr, VOL_NO_LOCK);

    SLog(2,  "ViceAllocFids returns %s (count = %d)",
	     ViceErrorMsg((int) errorCode), Range->Count);
END_TIMING(AllocFids_Total);
    return(errorCode);
}


static const int COP2EntrySize = (int)(sizeof(ViceStoreId) + sizeof(ViceVersionVector));
/*
  ViceCOP2: Update the VV that was modified during the first phase (COP1)
*/
long FS_ViceCOP2(RPC2_Handle cid, RPC2_CountedBS *PiggyBS) 
{
    int errorCode = 0;
    char *cp = (char *)PiggyBS->SeqBody;
    int count, i;

    if (PiggyBS->SeqLen % COP2EntrySize != 0) {
	errorCode = EINVAL;
	goto Exit;
    }

    count = PiggyBS->SeqLen / COP2EntrySize;
    
    SLog(1, "FS_ViceCOP2: about to process %d entries", count);

    for (i = 0; i < count ; i++) {
	ViceStoreId sid;
	ntohsid((ViceStoreId *)cp, &sid);
	ViceVersionVector vv;
	ntohvv((ViceVersionVector *)(cp + sizeof(ViceStoreId)), &vv);
	
	(void)InternalCOP2(cid, &sid, &vv);
	cp += COP2EntrySize;
    }

    SLog(1, "FS_ViceCOP2: returning success.");


Exit:
    return(errorCode);
}

/*
  ViceResolve: Resolve the diverging replicas of an object
*/
long FS_ViceResolve(RPC2_Handle cid, ViceFid *Fid) {
    int errorCode = 0;
    Volume *volptr = 0;	    /* the volume ptr */
    Vnode *vptr = 0;	    /* the vnode ptr */
    VolumeId VSGVolnum;
    int	status = 0;	    /* transaction status variable */
    ViceVersionVector VV;
    ResStatus rstatus;
    unsigned long hosts[VSG_MEMBERS];
    res_mgrpent *mgrp = 0;
    long resError = 0;
    long lockerror = 0;
    int logsizes = 0;
    int pathsize = 0;
    int reson = 0;
    VolumeId tmpvid = Fid->Volume;
    ResPathElem *pathelem_ptrs[VSG_MEMBERS];
    ResPathElem *pathelembuf = NULL;
    int j;

    if ( Fid ) { 
	    SLog(2,  "ViceResolve(%d, %s)", cid, FID_(Fid));
	    VSGVolnum = Fid->Volume;
    } else { 
	SLog(0, "ViceResolve: I was handed NULL Fid");
	CODA_ASSERT(0);
    }

       
    if (pathtiming && probingon && (!(ISDIR(*Fid)))) {
	FileresTPinfo = new timing_path(MAXPROBES);
	PROBE(FileresTPinfo, COORDSTARTVICERESOLVE);
    }
    

    /* get a mgroup */
    unsigned long vsgaddr;
    if (!(vsgaddr = XlateVidToVSG(Fid->Volume))){
	errorCode = EINVAL;
	goto FreeGroups;
    }
    if (!XlateVid(&tmpvid)) {
	SLog(0,  "ViceResolve: Couldn't xlate vid %x", tmpvid);
	errorCode = EINVAL;
	goto FreeGroups;
    }
    reson = GetResFlag(tmpvid);
    SLog(9,  "ViceResolve: Getting Mgroup for VSG %x", vsgaddr);
    if (GetResMgroup(&mgrp, vsgaddr, 0)){
	/* error getting mgroup */
	SLog(0,  "ViceResolve: Couldnt get mgroup for vsg %x", 
		vsgaddr);
	errorCode = EINVAL;
	goto FreeGroups;
    }

    /* lock volumes at all servers and fetch the vnode vv */
    SLog(9,  "ViceResolve: Going to fetch vv from different sites ");
    ARG_MARSHALL(OUT_MODE, ViceVersionVector, VVvar, VV, VSG_MEMBERS);
    ARG_MARSHALL(OUT_MODE, ResStatus, rstatusvar, rstatus, VSG_MEMBERS);
    ARG_MARSHALL(OUT_MODE, RPC2_Integer, logsizesvar, logsizes, VSG_MEMBERS);
    ARG_MARSHALL(OUT_MODE, RPC2_Integer, pathsizevar, pathsize, VSG_MEMBERS);
    // This is allocated via malloc instead of on the stack because the 
    // array gets too big for the LWP stack.
    // We use MAXPATHLEN/2 because that is the max number of components possible in 
    // a single path 
    pathelembuf = (ResPathElem *)malloc(sizeof(ResPathElem) * (MAXPATHLEN/2) 
					* VSG_MEMBERS);
    for (j = 0; j < VSG_MEMBERS; j++) 
	pathelem_ptrs[j] = &(pathelembuf[j * (MAXPATHLEN/2)]);
    MRPC_MakeMulti(LockAndFetch_OP, LockAndFetch_PTR, VSG_MEMBERS,
		   mgrp->rrcc.handles, mgrp->rrcc.retcodes, 
		   mgrp->rrcc.MIp, 0, 0, Fid, 
		   FetchStatus, VVvar_ptrs, rstatusvar_ptrs, logsizesvar_ptrs, 
		   MAXPATHLEN/2, pathsizevar_ptrs, pathelem_ptrs);
    // delete hosts from mgroup where rpc failed 
    errorCode = mgrp->CheckResult();

    // delete hosts from mgroup where rpc succeeded but call returned error 
    lockerror = CheckResRetCodes((unsigned long *)mgrp->rrcc.retcodes, 
			      mgrp->rrcc.hosts, hosts);
    errorCode = mgrp->GetHostSet(hosts);

    // call resolve on object  only if no locking errors 
    if ( errorCode ) 
	    goto UnlockExit;
    if ( lockerror && lockerror != VNOVNODE )
	    goto UnlockExit;

    if (ISDIR(*Fid)) {
	    SLog(9,  "ViceResolve: Going to call Dirresolve");
	    switch (reson) {
	    case VMRES:
		    errorCode = EOPNOTSUPP;
		    goto UnlockExit;
		    break;
		    
	    case RVMRES: 
		    resError = RecovDirResolve(mgrp, Fid, VVvar_ptrs, 
					       rstatusvar_ptrs, 
					       (int *)logsizesvar_bufs, 
					       (int *)pathsizevar_bufs, 
					       pathelem_ptrs, 1);
		    break;
	    default:
		    resError = OldDirResolve(mgrp, Fid, VVvar_ptrs);
	    }
    } else {
	    SLog(9, "ViceResolve: Going to call Fileresolve");
	    resError = FileResolve(mgrp, Fid, VVvar_ptrs);
    }

    
 UnlockExit:
    // reget the host set - want to unlock wherever we locked volume 
    mgrp->GetHostSet(hosts);
    MRPC_MakeMulti(UnlockVol_OP, UnlockVol_PTR, VSG_MEMBERS, 
		   mgrp->rrcc.handles, mgrp->rrcc.retcodes,
		   mgrp->rrcc.MIp, 0, 0, 
		   Fid->Volume);

 FreeGroups:
    if (pathelembuf) free(pathelembuf);
    if (mgrp)
	PutResMgroup(&mgrp);
    PROBE(FileresTPinfo, COORDENDVICERESOLVE);
    if (lockerror){
	    SLog(0,  "ViceResolve:Couldnt lock volume %x at all accessible servers",
		 Fid->Volume);
	if ( lockerror != VNOVNODE ) 
	    lockerror = EWOULDBLOCK;
	return(lockerror);
    }
    if (errorCode){
	if (errorCode == ETIMEDOUT) 
	    return(EWOULDBLOCK);
	else
	    return(errorCode);
    }
    return(resError);
}


// used by the lock queue manager to unlock expired locks 
void ForceUnlockVol(VolumeId Vid) {/* Vid is the rw id */
    int error = 0;
    Volume *volptr;
    if (GetVolObj(Vid, &volptr, VOL_NO_LOCK, 0, 0)) {
	SLog(0,  "ForceUnlockVol: GetVolObj %x error", Vid);
	return;
    }
    PutVolObj(&volptr, VOL_EXCL_LOCK, 0);
}


/*
  BEGIN_HTML
  <a name="ViceSetVV"><strong>Sets the version vector for an object</strong></a> 
  END_HTML
*/
// THIS CODE IS NEEDED TO DO A CLEARINC FROM THE REPAIR TOOL FOR QUOTA REPAIRS
long FS_ViceSetVV(RPC2_Handle cid, ViceFid *Fid, ViceVersionVector *VV, RPC2_CountedBS *PiggyBS)
{
    Vnode *vptr = 0;
    Volume *volptr = 0;
    ClientEntry *client = 0;
    long errorCode = 0;
    rvm_return_t camstatus = RVM_SUCCESS;

    SLog(9,  "Entering ViceSetVV(%u.%d.%d)", Fid->Volume, Fid->Vnode, Fid->Unique);
    
    /* translate replicated fid to rw fid */
    XlateVid(&Fid->Volume);		/* dont bother checking for errors */
    if (RPC2_GetPrivatePointer(cid, (char **)&client) != RPC2_SUCCESS)
	return(EINVAL);	

    if (!client) return EINVAL;
    if ((PiggyBS->SeqLen > 0) && (errorCode = FS_ViceCOP2(cid, PiggyBS)))
	goto FreeLocks;
    if (errorCode = GetFsObj(Fid, &volptr, &vptr, WRITE_LOCK, NO_LOCK, 1, 0, 0)){
	SLog(0,  "ViceSetVV: Error %d in GetFsObj", errorCode);
	goto FreeLocks;
    }
    
    /* if volume is being repaired check if repairer is same as client */
    if (V_VolLock(volptr).IPAddress){
	if (V_VolLock(volptr).IPAddress != client->VenusId->host){
	    SLog(0,  "ViceSetVV: Volume Repairer != Locker");
	    errorCode = EINVAL;
	    goto FreeLocks;
	}
    }
    bcopy((const void *)VV, (void *) &(Vnode_vv(vptr)), (int)sizeof(ViceVersionVector));
    
FreeLocks:
    rvmlib_begin_transaction(restore);
       int fileCode = 0;
       if (vptr){
	   VPutVnode((Error *)&fileCode, vptr);
	   CODA_ASSERT(fileCode == 0);
       }
       if (volptr)
	   PutVolObj(&volptr, NO_LOCK);
    rvmlib_end_transaction(flush, &(camstatus));       
    if (camstatus){
	SLog(0,  "ViceSetVV: Error during transaction");
	return(camstatus);
    }
    SLog(9,  
	   "ViceSetVV finished with errorcode %d", errorCode);
    return(errorCode);
}


/*
  ViceRepair: Manually resolve the object
*/
long FS_ViceRepair(RPC2_Handle cid, ViceFid *Fid, ViceStatus *status,
		 ViceStoreId *StoreId,  SE_Descriptor *BD)
{
    Volume  *volptr = 0;	    	/* pointer to the volume */
    ClientEntry	*client = 0;    	/* pointer to client data */
    Rights  rights;	    		/* rights for this user */
    Rights  anyrights;	    		/* rights for any user */
    int deltablocks = 0;
    int	    errorCode = 0;
    int Length = (int) status->Length;
    int	    DirRepairErrorCode = 0; 
    dlist *vlist = new dlist((CFN)VLECmp);
    vle *ov = 0;
    vle *pv = 0;
    VolumeId VSGVolnum = Fid->Volume;	
    int volindex = -1;
    int FRep;
    int myRepairCount = 0;
    struct repair *myRepairList = 0;
    char    filename[100];  		/* for transfer by name */
    int	vmresolutionOn = 0;

    SLog(1,  "ViceRepair: Fid = %u.%d.%d", Fid->Volume, Fid->Vnode, Fid->Unique);
    
    /* 1. validate parameters */
    {
	if (errorCode = ValidateParms(cid, &client, NULL, &Fid->Volume, NULL,
				      NULL))
	    goto FreeLocks;
    }
    
/*
  BEGIN_HTML
  <a name="ObtainObj"> <strong> Obtain the necessary objects for the
  repair actions </strong></a> 
  END_HTML
*/

    /* 2. Get top level object being repaired  */
    {
	/* Need to get volume in SHARED LOCK mode,
	   but vnode with a  READLOCK because later on we 
	   will get it with a WRITELOCK */
	ov = AddVLE(*vlist, Fid);
	if ( ISDIR(*Fid) ) 
		ov->d_inodemod = 1;
	errorCode = GetFsObj(Fid, &volptr, &ov->vptr, 
			     READ_LOCK, SHARED_LOCK, 
			     1, 0, ov->d_inodemod);
	if ( errorCode ) 
		goto FreeLocks;
	
	volindex = V_volumeindex(volptr);
	if (ov->vptr->disk.type == vFile ||
	    ov->vptr->disk.type == vSymlink)
	    FRep = TRUE;
	else
	    FRep = FALSE;

    }

    /* 3. Fetch Repair file */
    {
	if (FRep){
	    ov->f_finode  = icreate((int)V_device(volptr), 0, (int) V_id(volptr), 
			       (int) ov->vptr->vnodeNumber, (int)ov->vptr->disk.uniquifier, 
			       (int) ov->vptr->disk.dataVersion + 1);
	    CODA_ASSERT(ov->f_finode > 0);
	    int tblocks = (int) (nBlocks(Length) - nBlocks(ov->vptr->disk.length));
	    if (errorCode = AdjustDiskUsage(volptr, tblocks))
		goto FreeLocks;
	    deltablocks = tblocks;
	    if (errorCode = StoreBulkTransfer(cid, client, volptr, 
					      ov->vptr, ov->f_finode, Length))
		goto FreeLocks;

	}
	else {
	    /* directory repair - transfer by name */
	    strcpy(filename, "/tmp/repair.XXXXXX");
	    mktemp(filename);
	    if (errorCode = FetchFileByName(cid, filename, client)) {
		unlink(filename);
		goto FreeLocks;
	    }
	}
    }

    /* parse repair file */
    {
	if (!FRep) {
	    struct listhdr *replicaList = 0;
	    int replicaCount;
	    /* parse repair file */
	    if (errorCode = repair_getdfile(filename, &replicaCount, &replicaList)){
		SLog(0,  "CheckDirRepairSemantics: Error %d in getdfile", errorCode);
		goto FreeLocks;
	    }
	    unlink(filename);
	    if (errorCode = GetMyRepairList(&(ov->fid), replicaList, replicaCount, 
					    &myRepairList, &myRepairCount)) {
		SLog(0,  "CheckDirRepairSemantics: Error %d in getmyrepairlist",
			errorCode);
		if (replicaList) free(replicaList);
		goto FreeLocks;
	    }
	    if (replicaList) free(replicaList);
	}
    }

    /* get all objects under repair */
    {
    	if (errorCode = GetRepairObjects(volptr, ov, vlist, 
					 myRepairList, myRepairCount))
	    goto FreeLocks;
    }
    /* Check Semantics */
    {

	if (errorCode = CheckRepairSemantics(ov, volptr, vlist, status, 
					     client, &rights, &anyrights,
					     myRepairCount, myRepairList))
	    goto FreeLocks;
    }

    /* Peform Repair */
    {
	if (FRep) {
	    if (errorCode = PerformFileRepair(ov, volptr, VSGVolnum, 
					      status, StoreId, 
					      rights, anyrights))
		goto FreeLocks;
	}
	else if (errorCode = PerformDirRepair(client, ov, volptr, VSGVolnum,
					      status, StoreId, 
					      myRepairList, myRepairCount, 
					      vlist, rights, anyrights, &deltablocks))
	    goto FreeLocks;
    }
    /* add vnode to coppending table  */
    {
	ViceFid fids[MAXFIDS]; bzero((void *)fids, MAXFIDS * (int) sizeof(ViceFid));
	fids[0] = (ov->fid);
	CopPendingMan->add(new cpent(StoreId, fids));
    }

    if (AllowResolution && V_RVMResOn(volptr)) {
	// append a new log record 
	if (!FRep) {
	    SLog(9, 
		   "ViceRepair: Spooling Repair Log Record");
	    if (errorCode = SpoolVMLogRecord(vlist, ov, volptr, StoreId, 
					     ViceRepair_OP, 0)) {
		SLog(0, 
		       "ViceRepair: error %d in SpoolVMLogRecord for (0x%x.%x)\n",
		       errorCode, Fid->Vnode, Fid->Unique);
		goto FreeLocks;
	    }
	    // set log of dir being repaired for truncation 
	    ov->d_needslogtrunc = 1;
	}
	// mark all deleted children's logs for purging - (Note: not truncation)
	if (!FRep && !errorCode) {
	    dlist_iterator next(*vlist);
	    vle *v;
	    while (v = (vle *)next()) 
		if (v->vptr && 
		    v->vptr->delete_me &&
		    v->vptr->disk.type == vDirectory) 
		    v->d_needslogpurge = 1;
	}
	
    }

FreeLocks:
    if (myRepairList) free(myRepairList);
    PutObjects(errorCode, volptr, SHARED_LOCK, vlist, deltablocks, 1);

    /* truncate log of object being repaired - only leave repair record */
#if 0
    if (!errorCode && !FRep && AllowResolution && vmresolutionOn) {
	CODA_ASSERT(volindex != -1);
	TruncResLog(volindex, Fid->Vnode, Fid->Unique);
    }
#endif
    return(errorCode);
}

/*
  PerformFileRepair: Perform the actions for
  reparing a file object
*/
static int PerformFileRepair(vle *ov, Volume *volptr, VolumeId VSGVolnum, 
			      ViceStatus *status, ViceStoreId *StoreId, 
			      Rights rights, Rights anyrights)
{
    /* break callback first */
    {
	ViceFid *Fid;
	Fid = &(ov->fid);
	CodaBreakCallBack(0, Fid, VSGVolnum);
    }

    if (ov->vptr->disk.inodeNumber != 0)
	ov->f_sinode = ov->vptr->disk.inodeNumber;
    
    {
	Vnode *vptr = ov->vptr;
	vptr->disk.inodeNumber = ov->f_finode;
	vptr->disk.cloned = 0;	// invoke COW - added 9/30/92 - Puneet Kumar
	vptr->disk.length = status->Length;
	vptr->disk.owner = status->Owner;

	vptr->disk.dataVersion++;
	vptr->disk.author = status->Author;
	vptr->disk.unixModifyTime = status->Date;
	vptr->disk.modeBits = status->Mode;
	
	ViceVersionVector DiffVV;
	DiffVV = status->VV;
	SubVVs(&DiffVV, &Vnode_vv(vptr));
	AddVVs(&Vnode_vv(vptr), &DiffVV);	
 	AddVVs(&V_versionvector(volptr), &DiffVV);
	NewCOP1Update(volptr, vptr, StoreId);
	ClearIncon(vptr->disk.versionvector);
	SetStatus(vptr, status, rights, anyrights);
    }
    return(0);
}

int GetMyRepairList(ViceFid *Fid, struct listhdr *replicaList, int 
		     replicaCount, struct repair **myList, int *myCount)
{
    int	i;
    int found = -1;
    for (i = 0; i < replicaCount; i++)
	if (bcmp((const void *)&(replicaList[i].replicaId), (const void *) &(Fid->Volume), (int)sizeof(VolumeId)))
	    continue;
	else
	    /* found an entry */
	    if (found == -1)
		found = i;
	    else /* duplicate entry */
		return EINVAL;
    if (found != -1){
	SLog(9,  "GetMyRepairList found an entry ");
	*myCount = replicaList[found].repairCount;
	*myList = replicaList[found].repairList;
    }
    else{
	*myCount = 0;
	*myList = NULL;
    }
    return 0;
}

/*
  BEGIN_HTML
  <a name="CheckRepairSemantics"> <strong> Check whether the repair
  mutation operations satisfy the necessary semantic requirements. 
  </strong> </a> 
  END_HTML
*/
int CheckRepairSemantics(vle *ov, Volume *volptr, dlist *vlist,
			 ViceStatus *status, ClientEntry *client,
			 Rights *rights, Rights *anyrights,
			 int repCount, struct repair *repList) {
			 
    /* check if object is really inconsistent */
    Vnode *vptr = ov->vptr;
    if (!IsIncon(vptr->disk.versionvector)) {
	SLog(0,  "CheckRepairSemantics: Object(%x.%x)under repair is not inc",
		vptr->vnodeNumber, vptr->disk.uniquifier);
	return(EINVAL);
    }
    
    /* check if new vv is legal */
    int res = VV_Cmp_IgnoreInc(&Vnode_vv(vptr), &status->VV);
    if (res != VV_EQ && res != VV_SUB) {
	SLog(0,  "CheckRepairSemantics: %x.%x VV's are in conflict",
		vptr->vnodeNumber, vptr->disk.uniquifier);
	return(EINCOMPATIBLE);
    }
    
    /* check type of object */
    if (vptr->disk.type != vFile && 
	vptr->disk.type != vDirectory && 
	vptr->disk.type != vSymlink) {
	SLog(0,  "CheckRepairSemantics: Unknown Object type(%x.%x)",
		vptr->vnodeNumber, vptr->disk.uniquifier);
	return(EINVAL);
    }

    if (vptr->disk.type == vFile || vptr->disk.type == vSymlink) {
	ViceFid parentFid;
	parentFid.Volume = V_id(volptr);
	parentFid.Vnode = ov->vptr->disk.vparent;
	parentFid.Unique = ov->vptr->disk.uparent;
	vle *pv = FindVLE(*vlist, &parentFid);
	CODA_ASSERT(pv != 0);
	return(CheckFileRepairSemantics(ov, pv, volptr, status, 
					client, rights, anyrights));
    }
    else
	return(CheckDirRepairSemantics(ov, vlist, volptr, status, 
				       client, rights, anyrights, 
				       repCount, repList));
}

/*
  BEGIN_HTML
  <a name="CheckFileRepairSemantics"> <strong> Check whether semantic
  constraints are satisfied for file repair operation </strong></a> 
  END_HTML
*/
int CheckFileRepairSemantics(vle *ov, vle *pv, Volume *volptr, 
			     ViceStatus *status, ClientEntry *client,
			     Rights *rights, Rights *anyrights) {
    int errorCode = 0;
    AL_AccessList   *aCL = 0;   
    int	    aCLSize = 0;	    

    SetAccessList(pv->vptr, aCL, aCLSize);

    CODA_ASSERT(GetRights(client->CPS, aCL, aCLSize, rights, anyrights) == 0);
    
    /* Perform the following check:
	  Client must have write access in the parent directory.
	  Client shouldnt be changing the ownership of the object 
	  unless client is system administrator.
	  File should be writable by the client.
	  */
    if (!(*rights & PRSFS_WRITE)){
	SLog(0,  "CheckFileRepairSem: Client doesnt have write rights");
	return(EACCES);
    }
    // I am taking this out for now... it doesn't seem to make sense.
    // Resolution doesn't know the owner all the time and normal users then can't
    // repair the file... a nuisance.   XXX Puneet
#ifdef notdef
    if (ov->vptr->disk.owner != status->Owner){
	/* client is trying to modify the owner */
	if (SystemUser(client)){
	    SLog(0,  "CheckFileRepairSem: Ownership violation ");
	    return(EACCES);
	}
    }
#endif notdef
    return(0);
}

/* checks if client has rights to modify the acl ; vptr must be a directory vnode pointer */
int CheckRepairACLSemantics(ClientEntry *client, Vnode *vptr, Volume *volptr,  
		      AL_AccessList **aCL, int *aCLSize)
{
    Rights  rights, anyrights;
    
    SLog(9,  "Entering CheckACLSemantics");

    SetAccessList(vptr, *aCL, *aCLSize);

    CODA_ASSERT(GetRights(client->CPS, *aCL, *aCLSize, &rights, &anyrights) == 0);
    if (!(rights & PRSFS_ADMINISTER) &&
	 (client->Id != vptr->disk.owner) &&
	 (SystemUser(client))){
	SLog(0,  "CheckACLSemantics: No access ");
	return(EACCES);
    }
    return(0);
}
    
/* checks if client has administer rights and newentry can be inserted */
int CheckRepairSetACLSemantics(ClientEntry *client, Vnode *vptr, Volume *volptr, 
			       char *name, int rights) {
    int	Id;
    int	errorCode = 0;
    AL_AccessList *aCL = 0;
    int	aCLSize = 0;

    /* protection checks */
    {
	if (errorCode = CheckRepairACLSemantics(client, vptr, volptr, &aCL, &aCLSize))
	    return(errorCode);
    }

    /* acl integrity checks */
    {
	if (AL_NameToId(name, &Id) < 0){
	    SLog(0,  "CheckRepairSetACLSemantics: Couldnt convert name to id ");
	    return(-1);
	}

	/* check if new entry can be accomodated in list */
	for (int i = 0; i < aCL->PlusEntriesInUse; i++)
	    if (aCL->ActualEntries[i].Id == Id){
		SLog(9,  "CheckRepairSetACLSemantics: Found Id for %s in access list", name);
		return(0);
	    }

	/* didnt find the entry - if we are deleting rights then it is an error; */
	/* else, see if one can be inserted */
	if (!rights) return(EINVAL);
	SLog(9,  "CheckRepairSetACLSemantics: +entries = %d, -entries = %d, total = %d", 
		aCL->PlusEntriesInUse, aCL->MinusEntriesInUse, aCL->TotalNoOfEntries);
	if ((aCL->MySize + sizeof(AL_AccessEntry)) < VAclSize(vptr)) 
	    return(0);
	else 
	    return(E2BIG);
    }
}

int CheckRepairSetNACLSemantics(ClientEntry *client, Vnode *vptr, Volume *volptr, 
			  char *name,int rights) {
    int	Id;
    int	errorCode = 0;
    AL_AccessList *aCL = 0;
    int	aCLSize = 0;

    /* protection checks */
    {
	if (errorCode = CheckRepairACLSemantics(client, vptr, volptr, &aCL, &aCLSize))
	    return(errorCode);
    }

    /* acl integrity checks */
    {
	if (AL_NameToId(name, &Id) < 0){
	    SLog(0,  "CheckSetACLSemantics: Couldnt convert name to id ");
	    return(-1);
	}
	/* check if new entry can be accomodated in negative rights list */
	for (int i = aCL->TotalNoOfEntries - 1; 
	     i >=  aCL->TotalNoOfEntries - aCL->MinusEntriesInUse; 
	     i--)
	    if (aCL->ActualEntries[i].Id == Id)
		return(0);

	/* didnt find the entry - if we are deleting rights then it is an error; */
	/* else, see if one can be inserted */
	if(!rights) return(EINVAL);
	if ((aCL->MySize + sizeof(AL_AccessEntry)) < VAclSize(vptr)) return(0);
	else return(E2BIG);
    }
}

static int CheckRepairRenameSemantics(ClientEntry *client, Volume *volptr, vle *sdv, 
				       vle *tdv, vle *sv, vle *tv, char *name, 
				       char *newname) {
    int errorCode;
    if (errorCode = CheckRenameSemantics(client, &(sdv->vptr), &(tdv->vptr),
					 &(sv->vptr), name, tv ? &(tv->vptr) : NULL, 
					 newname, &volptr, 0, NULL, NULL, NULL, 
					 NULL, NULL, NULL, NULL, NULL, NULL,
					 NULL, NULL, 1, 0))
	return(errorCode);
    
    // make sure atleast one of src/target is incon
    if (!IsIncon(sdv->vptr->disk.versionvector) &&  
	!IsIncon(tdv->vptr->disk.versionvector)) {
	SLog(0,  
	       "CheckRepairRenameSemantics: Neither of src/target are inconsistent\n");
	return(EINVAL);
    }
    return(0);
}
		  
/* Set positive rights for user "name"; zero means delete */
int SetRights(Vnode *vptr, char *name, int rights)
{
    int Id;
    AL_AccessList *aCL = 0;
    int	aCLSize = 0;

    SLog(9,  "Entering SetRights(%s %d)", name, rights);
    if (AL_NameToId(name, &Id) < 0){
	SLog(0,  "SetRights: couldnt get id for %s ", name);
	return -1;
    }
    /* set the ACL */
    aCL = VVnodeACL(vptr);
    aCLSize = VAclSize(vptr);

    /* find the entry */
    for(int i = 0; i < aCL->PlusEntriesInUse; i++){
	if (aCL->ActualEntries[i].Id == Id){
	    if (rights)
		aCL->ActualEntries[i].Rights = rights;
	    else {
		/* remove this entry since rights are zero */
		for (int j = i; j < (aCL->PlusEntriesInUse - 1); j++)
		    bcopy((const void *)&(aCL->ActualEntries[j+1]), (void *) &(aCL->ActualEntries[j]),
			  (int) sizeof(AL_AccessEntry));
		aCL->PlusEntriesInUse--;
		aCL->TotalNoOfEntries--;
		aCL->MySize -= (int) sizeof(AL_AccessEntry);
	    }
	    return(0);
	}
    }

    /* didnt find the entry - create one */
    if (aCL->PlusEntriesInUse + aCL->MinusEntriesInUse == aCL->TotalNoOfEntries){
	/* allocate some more entries */
	for (int i = aCL->TotalNoOfEntries - 1; 
	     i > (aCL->TotalNoOfEntries - aCL->MinusEntriesInUse - 1); 
	     i--)
	    bcopy((const void *)&(aCL->ActualEntries[i]), (void *) &(aCL->ActualEntries[i+1]), (int) sizeof(AL_AccessEntry));
	aCL->TotalNoOfEntries++;
	aCL->MySize += (int) sizeof(AL_AccessEntry);
    }

    aCL->ActualEntries[aCL->PlusEntriesInUse].Id = Id;
    aCL->ActualEntries[aCL->PlusEntriesInUse].Rights = rights;
    aCL->PlusEntriesInUse++;

    /* sort the entries */
    qsort( (char *)&(aCL->ActualEntries[0]), aCL->PlusEntriesInUse, 
	  sizeof(AL_AccessEntry), (int (*)(const void *, const void *)) CmpPlus);
    printf("The accessList after setting rights is \n");
    AL_PrintAlist(aCL);
    return(0);
}

/* set negative rights for user "name"; zero means delete */
int SetNRights(Vnode *vptr, char *name, int rights)
{
    int Id;
    AL_AccessList *aCL = 0;
    int	aCLSize = 0;
    int p, m, t;

    SLog(9,  "Entering SetNRights(%s %d)", name, rights);
    if (AL_NameToId(name, &Id) < 0){
	SLog(0,  "SetRights: couldnt get id for %s ", name);
	return -1;
    }
    /* set the ACL */
    aCL = VVnodeACL(vptr);
    aCLSize = VAclSize(vptr);

    p = aCL->PlusEntriesInUse;
    m = aCL->MinusEntriesInUse;
    t = aCL->TotalNoOfEntries;

    /* find the entry */
    for(int i = t - 1; i >= t - m; i--){
	if (aCL->ActualEntries[i].Id == Id){
	    if (rights)
		aCL->ActualEntries[i].Rights = rights;
	    else {
		/* remove this entry since rights are zero */
		for (int j = i; j > t - m; j--)
		    bcopy((const void *)&(aCL->ActualEntries[j-1]), (void *) &(aCL->ActualEntries[j]), 
			  (int) sizeof(AL_AccessEntry));
		aCL->MinusEntriesInUse--;
		aCL->TotalNoOfEntries--;
		aCL->MySize -= (int) sizeof(AL_AccessEntry);
	    }
	    return(0);
	}
    }
    /* didnt find the entry - create one */
    if ((m + p) == t){
	/* all entries are taken - create one */
	for (int i = t - 1; i > t - m - 1; i--)
	    bcopy((const void *)&(aCL->ActualEntries[i]), (void *) &(aCL->ActualEntries[i+1]), 
		  (int) sizeof(AL_AccessEntry));
	t = ++aCL->TotalNoOfEntries;
	aCL->MySize += (int)sizeof(AL_AccessEntry);
    }
    aCL->ActualEntries[t - m - 1].Id = Id;
    aCL->ActualEntries[t - m - 1].Rights = rights;
    aCL->MinusEntriesInUse++;

    /* sort the entry */
    qsort((char *)&(aCL->ActualEntries[t-m-1]), aCL->MinusEntriesInUse, 
	  sizeof(AL_AccessEntry), (int (*)(const void *, const void *)) CmpMinus);
    printf("The accessList after setting rights is \n");
    AL_PrintAlist(aCL);
    return 0;
}

/*
  BEGIN_HTML
  <a name="CheckDirRepairSemantics"> <strong> Check semantic
  constraints for the directory repair operations </strong></a> 
  END_HTML
*/
int CheckDirRepairSemantics(vle *ov, dlist *vlist, Volume *volptr, 
			    ViceStatus *status, ClientEntry *client, 
			    Rights *rights, Rights *anyrights, 
			    int repCount, struct repair *repList) {
    int errorCode = 0;
    AL_AccessList   *aCL = 0;   
    int	    aCLSize = 0;	    
    int deltablocks = 0;

    SetAccessList(ov->vptr, aCL, aCLSize);

    CODA_ASSERT(GetRights(client->CPS, aCL, aCLSize, rights, anyrights) == 0);

    for (int i = 0; i < repCount; i++) {
	struct repair repairent = repList[i];
	ViceFid ParentFid;
	switch(repairent.opcode){
	  case REPAIR_CREATEF:
	    if (errorCode = CheckCreateSemantics(client, &(ov->vptr), NULL, 
						 repairent.name, &volptr, 0, 
						 NULL, NULL, NULL, NULL, NULL))
		return(errorCode);
	    ParentFid.Volume = ov->fid.Volume;
	    if (ObjectExists(V_volumeindex(volptr), vSmall, 
			     vnodeIdToBitNumber(repairent.parms[1]),
			     repairent.parms[2], &ParentFid)) {
		SLog(0,  "Object %s(%x.%x) already exists in parent %x.%x",
			repairent.name, repairent.parms[1], repairent.parms[2],
			ParentFid.Vnode, ParentFid.Unique);
		return(EINVAL);
	    }
	    deltablocks += nBlocks(0);
	    break;
	  case REPAIR_CREATED:
	    if (errorCode = CheckMkdirSemantics(client, &(ov->vptr), NULL, 
						 repairent.name, &volptr, 
						 0, NULL, NULL, NULL, 
						 NULL, NULL))
		return(errorCode);
	    if (ObjectExists(V_volumeindex(volptr), vLarge, 
				 vnodeIdToBitNumber(repairent.parms[1]),
				 repairent.parms[2], &ParentFid)) {
		SLog(0,  "Object %s(%x.%x) already exists in parent %x.%x",
			repairent.name, repairent.parms[1], repairent.parms[2],
			ParentFid.Vnode, ParentFid.Unique);
		return(EINVAL);
	    }
	    deltablocks += EMPTYDIRBLOCKS;
	    break;
	  case REPAIR_CREATES:
	    if (errorCode = CheckSymlinkSemantics(client, &(ov->vptr), NULL, 
						   repairent.name, &volptr, 
						   0, NULL, NULL, NULL, 
						   NULL, NULL))
		return(errorCode);
	    if (ObjectExists(V_volumeindex(volptr), vSmall, 
			     vnodeIdToBitNumber(repairent.parms[1]),
			     repairent.parms[2], &ParentFid)) {
		SLog(0,  "Object %s(%x.%x) already exists in parent %x.%x",
			repairent.name, repairent.parms[1], repairent.parms[2],
			ParentFid.Vnode, ParentFid.Unique);
		return(EINVAL);
	    }
	    deltablocks += nBlocks(0);
	    break;
	  case REPAIR_CREATEL: 
	    {
		vle *v = FindVLE(*vlist, (ViceFid *)&repairent.parms[0]);
		Vnode *childvptr = (v == 0) ? 0 : v->vptr;
		if (errorCode = CheckLinkSemantics(client, &(ov->vptr), 
						    childvptr? &childvptr : 0,
						    repairent.name, &volptr, 
						    0, NULL, NULL, NULL, 
						    NULL, NULL))
		    return(errorCode);
	    }
	    break;
	  case REPAIR_REMOVEFSL:
	    {
		/* get the object first */
		vle *cv = FindVLE(*vlist, (ViceFid *)&repairent.parms[0]);
		CODA_ASSERT(cv != 0);
		if (errorCode = CheckRemoveSemantics(client, &(ov->vptr), &cv->vptr,
						      repairent.name, &volptr, 
						      0, NULL, NULL, NULL, 0, 0))
		    return(errorCode);
		deltablocks -= (int) nBlocks(cv->vptr->disk.length);
	    }
	    break;
	  case REPAIR_REMOVED:
	    {
		/* get the object first */
		vle *cv = FindVLE(*vlist, (ViceFid *)&repairent.parms[0]);
		CODA_ASSERT(cv != 0);
		if (errorCode = CheckRmdirSemantics(client, &(ov->vptr), &(cv->vptr), 
						     repairent.name, &volptr,
						     0, NULL, NULL, NULL, NULL, NULL))
		    return(errorCode);
		
		PDirHandle cDir;
		cDir = VN_SetDirHandle(cv->vptr);
		if (!DH_IsEmpty(cDir)) {
		    /* do semantic checking recursively */
		    if (errorCode = CheckTreeRemoveSemantics(client, volptr, 
							     (ViceFid *)(&repairent.parms[0]),
							     vlist))
			return(errorCode);
		}
	    }
	    break;
	  case REPAIR_RENAME:
	    {
		// get the objects first
		ViceFid sdfid, tdfid, sfid, tfid;
		sdfid.Volume = tdfid.Volume = repairent.parms[0];
		sdfid.Vnode = repairent.parms[1];
		sdfid.Unique = repairent.parms[2];
		tdfid.Vnode = repairent.parms[3];
		tdfid.Unique = repairent.parms[4];
			
		vle *sdv = FindVLE(*vlist, &sdfid);
		vle *tdv = FindVLE(*vlist, &tdfid);
		CODA_ASSERT(sdv->vptr);
		CODA_ASSERT(tdv->vptr);

		// get source vnode ptr
		PDirHandle sdh;
		sdh = VN_SetDirHandle(sdv->vptr);

		CODA_ASSERT(DH_Lookup(sdh, repairent.name, &sfid, CLU_CASE_SENSITIVE) == 0);
		sfid.Volume = repairent.parms[0];
		vle *sv = FindVLE(*vlist, &sfid);
		CODA_ASSERT(sv); CODA_ASSERT(sv->vptr);

		// get target vnode 
		PDirHandle tdh;
		vle *tv = NULL;
		tdh = VN_SetDirHandle(tdv->vptr);
		if (DH_Lookup(tdh, repairent.newname, &tfid, CLU_CASE_SENSITIVE) == 0) {
		    tfid.Volume = repairent.parms[0];
		    tv = FindVLE(*vlist, &tfid);
		    CODA_ASSERT(tv); CODA_ASSERT(tv->vptr);
		}
		if (errorCode = CheckRepairRenameSemantics(client, volptr, sdv, tdv, 
							   sv, tv, repairent.name, repairent.newname))
		    return(errorCode);
	    }	
	    break;
	  case REPAIR_SETACL:
	    if (errorCode = CheckRepairSetACLSemantics(client, ov->vptr, volptr, 
						       repairent.name, repairent.parms[0]))
		return(errorCode);
	    break;	  
	  case REPAIR_SETNACL:
	    if (errorCode = CheckRepairSetNACLSemantics(client, ov->vptr, volptr, 
						  repairent.name, repairent.parms[0]))
		return(errorCode);
	    break;
	  case REPAIR_SETOWNER:
	    /* must be a system administrator */
	    /* POLICY ISSUE - maybe allow the user to set ownership to himself
	     * if he has administrative acl rights on the directory */
	    if (SystemUser(client)) {
		SLog(0,  "DirRepairSemantics: Error for REPAIR_SETOWNER; need to be system administrator");
		return(EACCES);
	    }
	    break;
	  case REPAIR_SETMODE:
	  case REPAIR_SETMTIME:
	    if ((client)->Id == ov->vptr->disk.owner)
		break;
	    /* protection checks */
	    {
		if (!(*rights & PRSFS_WRITE)){
		    SLog(0,  "DirRepairSemantics: Insufficient rights for REPAIR_SETMODE(TIME)");
		    return(EACCES);
		}
		if (!(*rights & PRSFS_DELETE) && !(*rights & PRSFS_INSERT)){
		    SLog(0,  "DirRepairSemantics: Insufficient rights for REPAIR_SETMODE(TIME)");
		    return(EACCES);
		}
	    }
	    break;
	  default:
	    break;
	}
    }

    /* check if there is enough place on disk */
    if (deltablocks) {
	if (errorCode = CheckDiskUsage(volptr, deltablocks))
	    return(errorCode);
    }
    return(0);
}
/*
 * data structure used to pass arguments 
 * for the recursive tree removal routines 
 */
#include "treeremove.h"
/*
  BEGIN_HTML
  <a name="PerformDirRepair"> <strong> Perform the actions for
  reparing a directory object </strong></a> 
  END_HTML
*/
static int PerformDirRepair(ClientEntry *client, vle *ov, Volume *volptr, 
			     VolumeId VSGVolnum,
			     ViceStatus *status, ViceStoreId *StoreId,
			     struct repair *repList, int repCount, 
			     dlist *vlist, Rights rights, Rights anyrights,
			     int *deltablocks) {

    int errorCode = 0;

    /* break Callback on directory */
    {
	ViceFid *Fid;
	Fid = &(ov->fid);
	CodaBreakCallBack(0, Fid, VSGVolnum);
    }
    /* peform ops */
    for (int i = 0; i < repCount; i++) {
	struct repair repairent = repList[i];
	switch (repairent.opcode) {
	  case REPAIR_CREATEF:
	    {
		ViceFid cFid = *((ViceFid *)&(repairent.parms[0]));

		int tblocks = 0;

		/* create the vnode */
		vle *cv = AddVLE(*vlist, &cFid);
		if (errorCode = AllocVnode(&cv->vptr, volptr, (ViceDataType)vFile, &cFid,
					   &(ov->fid), client->Id, 1, &tblocks))
		    return(errorCode);
		*deltablocks += tblocks;
		tblocks = 0;

		/* add name to parent */
		PerformCreate(client, VSGVolnum, volptr, ov->vptr,
			      cv->vptr, repairent.name, status->Date,
			      status->Mode, 0, StoreId, 
			      &ov->d_cinode, &tblocks);
		*deltablocks += tblocks;

		/* create the inode */
		cv->vptr->disk.dataVersion = 1;
		cv->f_finode = icreate((int)V_device(volptr), 0, (int)V_id(volptr),
				       (int)cv->vptr->vnodeNumber, 
				       (int)cv->vptr->disk.uniquifier,
				       (int)cv->vptr->disk.dataVersion);
		CODA_ASSERT(cv->f_finode > 0);
		cv->vptr->disk.inodeNumber = cv->f_finode;

		/* set the delete flag to true - for abort case */
		cv->vptr->delete_me = 1;
	    }
	    break;
	  case REPAIR_CREATED:
	    {
		ViceFid cFid = *((ViceFid *)&(repairent.parms[0]));
		int tblocks = 0;

		/* allocate the vnode */
		vle *cv = AddVLE(*vlist, &cFid);
		if (errorCode = AllocVnode(&cv->vptr, volptr, (ViceDataType)vDirectory, &cFid,
					   &(ov->fid), client->Id, 1, &tblocks))
		    return(errorCode);		
		*deltablocks += tblocks;
		tblocks = 0;

		/* make the child directory and insert name in parent */
		PerformMkdir(client, VSGVolnum, volptr, ov->vptr,
			     cv->vptr, repairent.name, status->Date,
			     status->Mode, 0, StoreId, 
			     &ov->d_cinode, &tblocks);
		*deltablocks += tblocks;

		/* set the delete flag to true - for abort case */
		cv->vptr->delete_me = 1;
	    }
	    break;
	  case REPAIR_CREATES:
	    {
		ViceFid cFid = *((ViceFid *)&(repairent.parms[0]));
		int tblocks = 0;

		/* create the vnode */
		vle *cv = AddVLE(*vlist, &cFid);
		if (errorCode = AllocVnode(&cv->vptr, volptr, (ViceDataType)vSymlink, &cFid,
					   &(ov->fid), client->Id, 1, &tblocks))
		    return(errorCode);
		*deltablocks += tblocks;
		tblocks = 0;

		/* create the symlink */
		PerformSymlink(client, VSGVolnum, volptr, ov->vptr,
			       cv->vptr, repairent.name, 0, 0, 
			       status->Date, status->Mode,
			       0, StoreId, &ov->d_cinode, &tblocks);
		*deltablocks += tblocks;
		
		/* create the inode */
		cv->vptr->disk.dataVersion = 1;
		cv->f_finode = icreate((int)V_device(volptr), 0, (int)V_id(volptr),
				       (int)cv->vptr->vnodeNumber, 
				       (int)cv->vptr->disk.uniquifier,
				       (int)cv->vptr->disk.dataVersion);
		CODA_ASSERT(cv->f_finode > 0);
		cv->vptr->disk.inodeNumber = cv->f_finode;

		/* set the delete flag to true - for abort case */
		cv->vptr->delete_me = 1;
	    }
	    break;
	  case REPAIR_CREATEL:
	    {
		int tblocks = 0;
		ViceFid cFid = *((ViceFid *)&(repairent.parms[0]));
		vle *cv = FindVLE(*vlist, &cFid);
		CODA_ASSERT(cv != 0);
		CODA_ASSERT(cv->vptr != 0);

		/* make sure vnode hasnt been deleted */
		if (cv->vptr->disk.linkCount <= 0) {
		    SLog(0,  "ViceRepair: Createl - LINKING TO A DELETED VNODE");
		    return(EINVAL);
		}

		/* add name to parent */
		PerformLink(client, VSGVolnum, volptr, ov->vptr, cv->vptr,
			    repairent.name, status->Date, 0, StoreId, 
			    &ov->d_cinode, &tblocks);
		*deltablocks += tblocks;
	    }
	    break;
	  case REPAIR_REMOVEFSL:
	    {
		ViceFid cFid = *((ViceFid *)&(repairent.parms[0]));
		vle *cv = FindVLE(*vlist, &cFid);
		CODA_ASSERT(cv != 0);
		int tblocks = 0;
		PerformRemove(client, VSGVolnum, volptr, ov->vptr, 
			      cv->vptr, repairent.name, status->Date,
			      0, StoreId, &ov->d_cinode, &tblocks);
		*deltablocks += tblocks;
		if (cv->vptr->delete_me) {
		    int tblocks = (int) -nBlocks(cv->vptr->disk.length);
		    CODA_ASSERT(AdjustDiskUsage(volptr, tblocks) == 0);
		    *deltablocks += tblocks;
		    cv->f_sinode = cv->vptr->disk.inodeNumber;
		    cv->vptr->disk.inodeNumber = 0;
		}
	    }
	    break;
	  case REPAIR_REMOVED:
	    {
		ViceFid cFid = *((ViceFid *)&(repairent.parms[0]));
		vle *cv = FindVLE(*vlist, &cFid);
		CODA_ASSERT(cv != 0);
		
		PDirHandle cdir;
		cdir = VN_SetDirHandle(cv->vptr);
		int tblocks = 0;
		/* first make the directory empty */
		{
		    if (!DH_IsEmpty(cdir)) {
			/* remove children first */
			TreeRmBlk	pkdparm;
			pkdparm.init(client, VSGVolnum, volptr,
				     status, StoreId, vlist, 0, 
				     (olist *)NULL, 0, &tblocks);
			DH_EnumerateDir(cdir, 
					(int (*) (PDirEntry, void *))PerformTreeRemoval, 
					(void *)&pkdparm);
			*deltablocks += tblocks;
		    }
		}
		
		/* remove the empty directory */
		{
		    CODA_ASSERT(DH_IsEmpty(cdir));
		    tblocks = 0;
		    PerformRmdir(client, VSGVolnum, volptr, 
				 ov->vptr, cv->vptr, repairent.name, status->Date,
				 0, StoreId, &ov->d_cinode, &tblocks);
		    *deltablocks += tblocks;
		    CODA_ASSERT(cv->vptr->delete_me);
		    tblocks = (int)-nBlocks(cv->vptr->disk.length);
		    CODA_ASSERT(AdjustDiskUsage(volptr, tblocks) == 0);
		    *deltablocks += tblocks;
		}
	    }
	    break;
	  case REPAIR_RENAME:
	    {
		// get the objects first
		ViceFid sdfid, tdfid, sfid, tfid;
		sdfid.Volume = tdfid.Volume = repairent.parms[0];
		sdfid.Vnode = repairent.parms[1];
		sdfid.Unique = repairent.parms[2];
		tdfid.Vnode = repairent.parms[3];
		tdfid.Unique = repairent.parms[4];
			
		vle *sdv = FindVLE(*vlist, &sdfid);
		vle *tdv = FindVLE(*vlist, &tdfid);
		CODA_ASSERT(sdv->vptr);
		CODA_ASSERT(tdv->vptr);

		// get source vnode ptr
		PDirHandle sdh;
		sdh = VN_SetDirHandle(sdv->vptr);

		CODA_ASSERT(DH_Lookup(sdh, repairent.name, &sfid, CLU_CASE_SENSITIVE) == 0);
		sfid.Volume = repairent.parms[0];
		vle *sv = FindVLE(*vlist, &sfid);
		CODA_ASSERT(sv); CODA_ASSERT(sv->vptr);

		// get target vnode 
		PDirHandle tdh;
		vle *tv = NULL;
		tdh = VN_SetDirHandle(tdv->vptr);
		if (DH_Lookup(tdh, repairent.newname, &tfid, CLU_CASE_SENSITIVE) == 0) {
		    tfid.Volume = repairent.parms[0];
		    tv = FindVLE(*vlist, &tfid);
		    CODA_ASSERT(tv); CODA_ASSERT(tv->vptr);
		}
		
		int tblocks = 0;
		PerformRename(client, VSGVolnum, volptr,
			      sdv->vptr, tdv->vptr, sv->vptr, 
			      tv ? tv->vptr : NULL, repairent.name, 
			      repairent.newname, status->Date, 0, 
			      StoreId, &sdv->d_cinode, &tdv->d_cinode, 
			      (sv->vptr->disk.type == vDirectory) ?  
			      &sv->d_cinode : NULL, 
			      &tblocks);
		*deltablocks += tblocks;

		if (tv && tv->vptr->disk.type != vDirectory) {
		    tv->f_sinode = tv->vptr->disk.inodeNumber;
		    tv->vptr->disk.inodeNumber = 0;
		}		

		// make sure both parents are inconsistent
		if (!IsIncon(sdv->vptr->disk.versionvector))
		    SetIncon(sdv->vptr->disk.versionvector);
		if (!IsIncon(tdv->vptr->disk.versionvector))
		    SetIncon(tdv->vptr->disk.versionvector);

		// make sure a repair record gets spooled for both parents
		// the ov's repair record gets spooled at the end 
		if (tdv != ov) {
		    if (AllowResolution && V_RVMResOn(volptr)) {
			SLog(0, 
			       "PerformRepair: Spooling Repair(rename - target) Log Record");
			if (errorCode = SpoolVMLogRecord(vlist, tdv, volptr, StoreId, 
							 ViceRepair_OP, 0)) {
			    SLog(0, 
				   "ViceRepair: error %d in SpoolVMLogRecord for (0x%x.%x)\n",
				   errorCode, tdv->vptr->vnodeNumber, tdv->vptr->disk.uniquifier);
			    return(errorCode);
			}
		    }
		}
		if (sdv != ov) {
		    if (AllowResolution && V_RVMResOn(volptr)) {
			SLog(0, 
			       "PerformRepair: Spooling Repair(rename - source) Log Record");
			if (errorCode = SpoolVMLogRecord(vlist, sdv, volptr, StoreId, 
							 ViceRepair_OP, 0)) {
			    SLog(0, 
				   "ViceRepair: error %d in SpoolVMLogRecord for (0x%x.%x)\n",
				   errorCode, sdv->vptr->vnodeNumber, sdv->vptr->disk.uniquifier);
			    return(errorCode);
			}
		    }
		}
	    }
	    break;
	  case REPAIR_SETACL:
	    CODA_ASSERT(SetRights(ov->vptr, repairent.name, repairent.parms[0]) == 0);
	    break;
	  case REPAIR_SETNACL:
	    CODA_ASSERT(SetNRights(ov->vptr, repairent.name, repairent.parms[0]) == 0);
	    break;
	  case REPAIR_SETOWNER:
	    ov->vptr->disk.owner = repairent.parms[0];
	    break;
	  case REPAIR_SETMODE:
	    ov->vptr->disk.modeBits = repairent.parms[0];
	    break;
	  case REPAIR_SETMTIME:
	    ov->vptr->disk.unixModifyTime = repairent.parms[0];
	    break;
	  default:
	    return(EINVAL);
	}
    }
    
    /* undelete the newly created vnodes */
    for (int j = 0; j < repCount; j++) {
	struct repair repairent = repList[j];
	vle *cv;
	ViceFid cFid;
	switch (repairent.opcode) {
	  case REPAIR_CREATEF:
	  case REPAIR_CREATED:
	  case REPAIR_CREATES:
	    cFid = *((ViceFid *)&(repairent.parms[0]));
	    cv = FindVLE(*vlist, &cFid);
	    CODA_ASSERT(cv != 0);
	    CODA_ASSERT(cv->vptr->delete_me == 1);
	    cv->vptr->delete_me = 0;
	    break;
	  default: 
	    break;
	}
    }
    
    /* set status of directory being repaired */
    {
	ov->vptr->disk.author = status->Author;
	ov->vptr->disk.unixModifyTime = status->Date;
	ov->vptr->disk.modeBits = status->Mode;
	ViceVersionVector DiffVV;
	DiffVV = status->VV;
	SubVVs(&DiffVV, &Vnode_vv(ov->vptr));
	AddVVs(&Vnode_vv(ov->vptr), &DiffVV);
 	AddVVs(&V_versionvector(volptr), &DiffVV); 
	NewCOP1Update(volptr, ov->vptr, StoreId);
	SetStatus(ov->vptr, status, rights, anyrights);
    }
    return(0);
}

#define ISCREATE(op)	(((op) == REPAIR_CREATEF) || \
			 ((op) == REPAIR_CREATES) || \
			 ((op) == REPAIR_CREATED) || \
			 ((op) == REPAIR_CREATEL))
			 
static int GetRepairObjects(Volume *volptr, vle *ov, dlist *vlist, 
			     struct repair *repList, int repCount)
{
    int errorCode = 0;

    /* first get fids */
    {    
	if (ov->vptr->disk.type == vFile || 
	    ov->vptr->disk.type == vSymlink) {
	    /* File Repair just get parent */
	    ViceFid pFid;
	    pFid.Volume = V_id(volptr);
	    pFid.Vnode = ov->vptr->disk.vparent;
	    pFid.Unique = ov->vptr->disk.uparent;
	    AddVLE(*vlist, &pFid);
	} else {
	    VolumeId vid = V_id(volptr);
	    /* parse list and insert fids into vlist */
	    for (int i = 0; i < repCount; i++) {
		struct repair repairent;
		repairent = repList[i];
		if (ISCREATE(repairent.opcode) || 
		    (repairent.opcode == REPAIR_RENAME)){
			if (!XlateVid((VolumeId *) &(repairent.parms[0]))){
				SLog(0,  "GetRepairObjects: Couldnt translate VSG ");
			return(EINVAL);
		    }
		    if (vid != repairent.parms[0]) {
			SLog(0,  "GetRepairObjects: Vid is not correct");
			return(EINVAL);
		    }
		}
		switch (repairent.opcode) {
		  case REPAIR_CREATEF:
		  case REPAIR_CREATED:
		  case REPAIR_CREATES:
		    break;
		  case REPAIR_CREATEL:
		    {
			
			/* check if createf exists before */
			int createfexists = 0;
			for(int j = 0; j < i; j++) 
			    if ((bcmp((const void *)&(repList[j].parms[0]), (const void *) 
				      &(repairent.parms[0]),
				      (int)sizeof(ViceFid)) == 0) &&
				repList[j].opcode == REPAIR_CREATEF){
				createfexists = 1;
				break;
			    }
			if (!createfexists) 
			    /* object already exists - add to fid list */
			    AddVLE(*vlist, (ViceFid *)&(repairent.parms[0]));
		    }       
		    break;
		  case REPAIR_REMOVEFSL:
		    {
			PDirHandle dh;
			dh = VN_SetDirHandle(ov->vptr);
			if (DH_Lookup(dh, repairent.name, 
				   (struct ViceFid *)&(repairent.parms[0]), CLU_CASE_SENSITIVE) != 0) {
			    SLog(0,  "REMOVEFSL: No name %s in directory",
				    repairent.name);
			    return(ENOENT);
			}
			repairent.parms[0] = (unsigned int)vid;
			AddVLE(*vlist, (ViceFid *)&(repairent.parms[0]));
		    }
		    break;
		  case REPAIR_REMOVED:
		    {
			PDirHandle dh;
			dh = VN_SetDirHandle(ov->vptr);
			if (DH_Lookup(dh, repairent.name, 
				      (struct ViceFid *)&(repairent.parms[0]), CLU_CASE_SENSITIVE) != 0) {
			    SLog(0,  
				   "REMOVED: No name %s in directory",
				    repairent.name);
			    return(ENOENT);
			}
			repairent.parms[0] = (unsigned int)vid;
			int errorCode = 0;
			errorCode = GetSubTree((ViceFid *)&(repairent.parms[0]),
					       volptr, vlist);
			if (errorCode) {
			    SLog(0,  "GetRepairObjects: Error %d in GetSubTree",
				    errorCode);
			    return(errorCode);
			}
		    }
		    break;
		  case REPAIR_RENAME:
		    {
			ViceFid spfid, tpfid, sfid, tfid;
			spfid.Volume = tpfid.Volume = sfid.Volume = tfid.Volume = repairent.parms[0];
			spfid.Vnode = repairent.parms[1];
			spfid.Unique = repairent.parms[2];
			tpfid.Vnode = repairent.parms[3];
			tpfid.Unique = repairent.parms[4];
			
			AddVLE(*vlist, &spfid);
			AddVLE(*vlist, &tpfid);
			AddChild(&volptr, vlist, &spfid, repairent.name, 1);
			AddChild(&volptr, vlist, &tpfid, repairent.newname, 1);
		    }
		    break;
		  case REPAIR_SETACL:
		  case REPAIR_SETNACL:
		  case REPAIR_SETMODE:
		  case REPAIR_SETOWNER:
		  case REPAIR_SETMTIME:
		    break;
		  default:
		    {
			SLog(0,  "Illegal OPCODE for GetRepairObjects");
			return(EINVAL);
		    }
		}
		repList[i] = repairent;
	    }
	}
    }

    /* put back object being repaired */
    {
	Error fileCode = 0;
	VPutVnode(&fileCode, ov->vptr);
	CODA_ASSERT(fileCode == 0);
	ov->vptr = 0;
    }
    
    /* Now, get all objects in fid order */
    {
	dlist_iterator next(*vlist);
	vle *v;
	while (v = (vle *)next()) {
	    SLog(10,  "GetRepairObjects: acquiring (%x.%x.%x)",
		    v->fid.Volume, v->fid.Vnode, v->fid.Unique);
	    if (errorCode = GetFsObj(&v->fid, &volptr, &v->vptr, 
				     WRITE_LOCK, SHARED_LOCK, 1, 0, 
				     v->d_inodemod))
		return(errorCode);
	}	
    }
    
    return(errorCode);
}

/* 
    Get all the fids in a subtree - deadlock free solution
    add the fids to the vlist
*/
int GetSubTree(ViceFid *fid, Volume *volptr, dlist *vlist) {
    Vnode *vptr = 0;
    dlist *tmplist = new dlist((CFN)VLECmp);
    int errorCode = 0;

    CODA_ASSERT(volptr != 0);

    /* get root vnode */
    {
	if (errorCode = GetFsObj(fid, &volptr, &vptr,
				 READ_LOCK, NO_LOCK, 1, 0, 0)) 
	    goto Exit;
	
	CODA_ASSERT(vptr->disk.type == vDirectory);
    }
	
    /* obtain fids of immediate children */
    {
	PDirHandle dh;
	dh = VN_SetDirHandle(vptr);
	
	if (!DH_IsEmpty(dh))
	    DH_EnumerateDir(dh, (int (*) (PDirEntry, void *))getFids, 
			    (void *)tmplist);
	VN_PutDirHandle(vptr);
    }
    
    /* put root's vnode */
    {
	Error error = 0;
	VPutVnode(&error, vptr);
	CODA_ASSERT(error == 0);
	vptr = 0;
    }

    /* put fids of sub-subtrees in list */
    {
	vle *v;
	while (v = (vle *)tmplist->get()) {
	    ViceFid cFid = v->fid;
	    delete v;
	    cFid.Volume = fid->Volume;
	    if (!ISDIR(cFid)) 
		AddVLE(*vlist, &cFid);
	    else {
		errorCode = GetSubTree(&cFid, volptr, vlist);
		if (errorCode)
		    goto Exit;
	    }
	}
    }

    /* add object's fid into list */
    AddVLE(*vlist, fid);
  Exit:
    {
	vle *v;
	while (v = (vle *)tmplist->get()) delete v;
	delete tmplist;
    }
    if (vptr) {
	Error error = 0;
	VPutVnode(&error, vptr);
	CODA_ASSERT(error = 0);
    }
    return(errorCode);
}

static int getFids(dlist *flist, char *name, long vnode, long unique)
{
    SLog(9,  "Entering GetFid for %s", name);
    if (!strcmp(name, ".") || !strcmp(name, ".."))
	return 0;

    ViceFid fid;
    fid.Volume = 0;
    fid.Vnode = vnode;
    fid.Unique = unique;
    AddVLE(*flist, &fid);

    SLog(9,  "Leaving GetFid for %s ", name);
    return(0);
}

class rmblk {
  public:
    dlist *vlist;
    Volume *volptr;
    ClientEntry *client;
    int	result;

    rmblk(dlist *vl, Volume *vp, ClientEntry *cl) {
	vlist = vl;
	volptr = vp;
	client = cl;
	result = 0;
    }
};

static int RecursiveCheckRemoveSemantics(PDirEntry de, void * data) 
{
	rmblk *rb = (rmblk *)data;
	VnodeId vnode;
	Unique_t unique;
	char *name = de->name;
	FID_NFid2Int(&de->fid, &vnode, &unique);

	if (rb->result) 
		return(rb->result);
	if (!strcmp(name, ".") || !strcmp(name, "..")) 
		return(0);

	int errorCode = 0;
	ViceFid fid;
	vle *pv = 0;
	vle *ov = 0;
	vle *tv = 0;
	/* form the fid */
	{
		fid.Volume = V_id(rb->volptr);
		fid.Vnode = vnode;
		fid.Unique = unique;
	}

	/* get the object and its parent */
	{
		ov = FindVLE(*(rb->vlist), &fid);
		CODA_ASSERT(ov != NULL);
	
		ViceFid pfid;
		pfid.Volume = fid.Volume;
		pfid.Vnode = ov->vptr->disk.vparent;
		pfid.Unique = ov->vptr->disk.uparent;
	
		pv = FindVLE(*(rb->vlist), &pfid);
		CODA_ASSERT(pv != NULL);
	}
	/* Check Semantics for the object's removal */
	{
		if (ov->vptr->disk.type == vFile ||
		    ov->vptr->disk.type == vSymlink) {
	    errorCode = CheckRemoveSemantics(rb->client, &(pv->vptr), &(ov->vptr),
					      name, &(rb->volptr), 0, NULL,
					      NULL, NULL, NULL, NULL);
	        if (errorCode) {
			rb->result = errorCode;
			return(errorCode);
		}
		return(0);
		} else {
	    /* child is a directory */
	    errorCode = CheckRmdirSemantics(rb->client, &(pv->vptr), &(ov->vptr),
					     name, &(rb->volptr), 0, NULL, 
					     NULL, NULL, NULL, NULL);
	    if (errorCode) {
		rb->result = errorCode;
		return(errorCode);
	    }
	    
	    PDirHandle td;
	    td = VN_SetDirHandle(ov->vptr);
	    if (!DH_IsEmpty(td)) 
		DH_EnumerateDir(td, RecursiveCheckRemoveSemantics, (void *)rb);
	}
    }

    return(rb->result);
}

/*
  CheckTreeRemoveSemantics: Check the semantic constraints for 
  removing a subtree 
*/
static int CheckTreeRemoveSemantics(ClientEntry *client, Volume *volptr, 
				     ViceFid *tFid, dlist *vlist) 
{
	int errorCode = 0;
	vle *tv = 0;

	/* get the root's vnode */
	{
		tv = FindVLE(*vlist, tFid);
		CODA_ASSERT(tv != 0);
	}

	/* recursively check semantics */
	{
		PDirHandle td;
		td = VN_SetDirHandle(tv->vptr);
		rmblk enumparm(vlist, volptr, client); 
		if (!DH_IsEmpty(td)) 
			DH_EnumerateDir(td, RecursiveCheckRemoveSemantics, 
					(void *)&enumparm);
		return(enumparm.result);
	}
}


/*
 *  PerformTreeRemove: Perform the actions for removing a subtree
 */

int PerformTreeRemoval(PDirEntry de, void *data)
{
	TreeRmBlk *pkdparm = (TreeRmBlk *)data;
	VnodeId vnode;
	Unique_t unique;
	char *name = de->name;
	ViceFid cFid;
	ViceFid pFid;
	vle *cv, *pv;

	FID_NFid2Int(&de->fid, &vnode, &unique);

	if (!strcmp(name, ".") || !strcmp(name, "..")) 
		return(0);
	/* get vnode of object */
	{
		cFid.Volume = V_id(pkdparm->volptr);
		cFid.Vnode = vnode;
		cFid.Unique = unique;
		
		cv = FindVLE(*(pkdparm->vlist), &cFid);
		CODA_ASSERT(cv != 0);
	}
	/* get vnode of parent */
	{
		pFid.Volume = cFid.Volume;
		pFid.Vnode = cv->vptr->disk.vparent;
		pFid.Unique = cv->vptr->disk.uparent;
		
		pv = FindVLE(*(pkdparm->vlist), &pFid);
		CODA_ASSERT(pv != 0);
	}

	/* delete children first */
	{
		if (cv->vptr->disk.type == vDirectory) {
			PDirHandle cDir;
			cDir = VN_SetDirHandle(cv->vptr);
			if (!DH_IsEmpty(cDir)) 
				DH_EnumerateDir(cDir, PerformTreeRemoval, 
						(void *) pkdparm);
		}
	}

    /* delete the object */
	{
		int nblocks = 0;
		if (cv->vptr->disk.type == vDirectory) {
			PerformRmdir(pkdparm->client, pkdparm->VSGVnum, 
				     pkdparm->volptr, 
				     pv->vptr, cv->vptr, name, 
				     pkdparm->status?pkdparm->status->Date : 
				     pv->vptr->disk.unixModifyTime,
				     0, pkdparm->storeid, 
				     &pv->d_cinode, &nblocks);
			*(pkdparm->blocks) += nblocks;
			CODA_ASSERT(cv->vptr->delete_me);
			nblocks = (int)-nBlocks(cv->vptr->disk.length);
			CODA_ASSERT(AdjustDiskUsage(pkdparm->volptr, nblocks) == 0);
			*(pkdparm->blocks) += nblocks;
	    if (AllowResolution && V_RVMResOn(pkdparm->volptr)) {
		//spool log record for resolution 
		if (pkdparm->IsResolve) {
		    // find log record for original remove operation - extract storeid 
		    ViceStoreId stid;
		    ViceStoreId *rmtstid = GetRemoteRemoveStoreId(pkdparm->hvlog, pkdparm->srvrid,
								  &pFid, &cFid, name);
		    if (!rmtstid) {
			SLog(0,
			       "PerformTreeRemoval: No rm record found for %s 0x%x.%x.%x\n",
			       name, V_id(pkdparm->volptr), vnode, unique);
			AllocStoreId(&stid);
		    }
		    else stid = *rmtstid;

		    SLog(9,  
			   "TreeRemove: Spooling Log Record for removing dir %s",
			   name);
		    int errorCode = 0;
		    if (errorCode = SpoolVMLogRecord(pkdparm->vlist, pv, 
						     pkdparm->volptr, &stid,
						     ResolveViceRemoveDir_OP, name, 
						     vnode, unique, 
						     VnLog(cv->vptr), &(Vnode_vv(cv->vptr).StoreId),
						     &(Vnode_vv(cv->vptr).StoreId)))
			SLog(0, 
			       "PerformTreeRemoval: error %d in SpoolVMLogRecord for (0x%x.%x)\n",
			       errorCode, vnode, unique);
		}
	    }
	}
	else {
	    PerformRemove(pkdparm->client, pkdparm->VSGVnum, pkdparm->volptr, 
			  pv->vptr, cv->vptr, name, 
			  pkdparm->status ? pkdparm->status->Date :
			  pv->vptr->disk.unixModifyTime,
			  0, pkdparm->storeid, &pv->d_cinode, &nblocks);
	    *(pkdparm->blocks) += nblocks;
	    if (cv->vptr->delete_me){
		nblocks = (int)-nBlocks(cv->vptr->disk.length);
		CODA_ASSERT(AdjustDiskUsage(pkdparm->volptr, nblocks) == 0);
		*(pkdparm->blocks) += nblocks;
		cv->f_sinode = cv->vptr->disk.inodeNumber;
		cv->vptr->disk.inodeNumber = 0;
	    }
	    if (AllowResolution && V_RVMResOn(pkdparm->volptr)) {
		//spool log record for resolution 
		if (pkdparm->IsResolve) {
		    // find log record for original remove operation - extract storeid 
		    ViceStoreId stid;
		    ViceStoreId *rmtstid = GetRemoteRemoveStoreId(pkdparm->hvlog, pkdparm->srvrid,
								  &pFid, &cFid, name);
		    if (!rmtstid) {
			SLog(0,
			       "PerformTreeRemoval: No rm record found for %s 0x%x.%x.%x\n",
			       name, V_id(pkdparm->volptr), vnode, unique);
			AllocStoreId(&stid);
		    }
		    else stid = *rmtstid;

		    SLog(9,  "TreeRemove: Spooling Log Record for removing %s",
			 name);
		    int errorCode = 0;
		    if (errorCode = SpoolVMLogRecord(pkdparm->vlist, pv, 
						     pkdparm->volptr, &stid,
						     ResolveViceRemove_OP, name, 
						     vnode, unique, 
						     &(Vnode_vv(cv->vptr))))
			SLog(0, "PerformTreeRemoval: error %d in"
			     " SpoolVMLogRecord for (0x%x.%x)\n",
			     errorCode, vnode, unique);
		}
	    }
	}
    }
	

}


long InternalCOP2(RPC2_Handle cid, ViceStoreId *StoreId, ViceVersionVector *UpdateSet) 
{
    int errorCode = 0;
    int i;
    Volume *volptr = 0;	    /* the volume ptr */
    Vnode *vptrs[MAXFIDS];  /* local array of vnode ptrs */
    for (i = 0; i < MAXFIDS; i++) vptrs[i] = 0;
    cpent *cpe = 0;
    int nfids = 0;
    rvm_return_t status = RVM_SUCCESS;
    vmindex freed_indices;
    recov_vol_log *vollog = NULL;

START_TIMING(COP2_Total);


    SLog(1,  "InternalCOP2, StoreId = (%x.%x), UpdateSet = []",
	     StoreId->Host, StoreId->Uniquifier);

    /* Dequeue the cop pending entry and sort the fids. */
    cpe = CopPendingMan->findanddeq(StoreId);
    if (!cpe) 
	errorCode = ENOENT;	/* ??? -JJK */
    
    if (!errorCode) {
	nfids = FidSort(cpe->fids);
	
	/* Get the volume and vnodes.  */
	/* Ignore inconsistency flag - this is necessary since  */
	/* ViceRepair no longer clears the flag and COP2 might be */
	/* called before the flag is cleared by the user */
	/* No need to lock the volume, because this doesnt change the file structure */
	for (i = 0; i < nfids; i++) {
	    errorCode = GetFsObj(&cpe->fids[i], &volptr, &vptrs[i], WRITE_LOCK, 
				 NO_LOCK, 1, 1, 0);
	    /* Don't complain about vnodes that were deleted by COP1 */
	    if (errorCode == VNOVNODE)
		errorCode = 0;
	    
	    if (errorCode)
		break;
	}
    }

    if (!errorCode && volptr && V_RVMResOn(volptr)) vollog = V_VolLog(volptr);

START_TIMING(COP2_Transaction);
    rvmlib_begin_transaction(restore);
    if (!errorCode) {
	/* Update the version vectors. */
	for (i = 0; i < nfids; i++)
	    if (vptrs[i])
		COP2Update(volptr, vptrs[i], UpdateSet, &freed_indices);
    }
    /* Put the vnodes. */
    for (i = 0; i < nfids; i++)
	if (vptrs[i]) {
	    Error fileCode;
	    VPutVnode(&fileCode, vptrs[i]);
	    CODA_ASSERT(fileCode == 0);
	}

    /* Put the volume. */
    PutVolObj(&volptr, NO_LOCK);
    rvmlib_end_transaction(flush, &(status));
END_TIMING(COP2_Transaction);

    if (cpe) {
	CopPendingMan->remove(cpe);
	delete cpe;
    }


    if ((status == 0) && !errorCode && vollog) {
	/* the transaction was successful -
	   free up vm bitmap corresponding to 
	   log records that were truncated */
	vmindex_iterator next(&freed_indices);
	unsigned long ind;
	while ((ind = next()) != -1) 
	    vollog->DeallocRecord((int)ind);
    }

    SLog(2,  "InternalCOP2 returns %s", ViceErrorMsg(errorCode));
END_TIMING(COP2_Total);

    return(errorCode);
}

/* get resolution flags for a given volume */
static int GetResFlag(VolumeId Vid) {
    int error = 0;
    Volume *volptr = 0;

    if (!AllowResolution) return(0);
    if (error = GetVolObj(Vid, &volptr, VOL_NO_LOCK, 0, 0)) {
	SLog(0,  "GetResFlag:: GetVolObj failed (%d) for %x",
		error, Vid);
	return(0);
    }
    int reson = volptr->header->diskstuff.ResOn;
    PutVolObj(&volptr, VOL_NO_LOCK, 0);
    return(reson);
}

static int FidSort(ViceFid *fids) {
    int nfids = 0;
    int i, j;

    if (SrvDebugLevel >= 9) {
	SLog(9,  "FidSort: nfids = %d", nfids);
	for (int k = 0; k < MAXFIDS; k++)
	    SLog(9,  ", Fid[%d] = (%x.%x.%x)",
		    k, fids[k].Volume, fids[k].Vnode, fids[k].Unique);
	SLog(9,  "");
    }

    /* First remove any duplicates.  Also determine the number of unique, non-null fids. */
    for (i = 0; i < MAXFIDS; i++)
	if (!(FID_EQ(&fids[i], &NullFid))) {
	    nfids++;
	    for (j = 0; j < i; j++)
		if (FID_EQ(&fids[i], &fids[j])) {
		    fids[j] = NullFid;
		    nfids--;
		    break;
		}
	}

    /* Now sort the fids in increasing order (null fids are HIGHEST). */
    for (i = 0; i < MAXFIDS - 1; i++)
	for (j = i + 1; j < MAXFIDS; j++)
	    if (!(FID_EQ(&fids[j], &NullFid)) &&
		(FID_EQ(&fids[i], &NullFid) || !(FID_LTE(fids[i], fids[j])))) {
		ViceFid tmpfid = fids[i];
		fids[i] = fids[j];
		fids[j] = tmpfid;
	    }

    if (SrvDebugLevel >= 9) {
	SLog(9,  "FidSort: nfids = %d", nfids);
	for (int k = 0; k < MAXFIDS; k++)
	    SLog(9,  ", Fid[%d] = (%x.%x.%x)",
		    k, fids[k].Volume, fids[k].Vnode, fids[k].Unique);
	SLog(9,  "");
    }

    return(nfids);
}

/*
  NewCOP1Update: Increment the version number and update the
  storeid of an object.

  Only the version number of this replica is incremented.  The
  other replicas's version numbers are incremented by COP2Update
*/
void NewCOP1Update(Volume *volptr, Vnode *vptr, 
		   ViceStoreId *StoreId, RPC2_Integer *vsptr) 
{
    /* Look up the VRDB entry. */
    vrent *vre = VRDB.find(V_groupId(volptr));
    if (!vre) Die("COP1Update: VSG not found!");

    /* Look up the index of this host. */
    int ix = vre->index(ThisHostAddr);
    if (ix < 0) 
	    Die("COP1Update: this host not found!");

    SLog(2, "COP1Update: Fid = (%x),(%x.%x.%x), StoreId = (%x.%x)",
	 V_groupId(volptr), V_id(volptr), vptr->vnodeNumber, 
	 vptr->disk.uniquifier, StoreId->Host, StoreId->Uniquifier);

    /* If a volume version stamp was sent in, and if it matches, update it. */
    if (vsptr) {
	    SLog(2, "COP1Update: client VS %d", *vsptr);
	    if (*vsptr == (&(V_versionvector(volptr).Versions.Site0))[ix])
		    (*vsptr)++;
	    else 
		    *vsptr = 0;
    }
		
    /* Fashion an UpdateSet using just ThisHost. */
    ViceVersionVector UpdateSet = NullVV;
    (&(UpdateSet.Versions.Site0))[ix] = 1;

    /* Install the new StoreId in the Vnode. */
    Vnode_vv(vptr).StoreId = *StoreId;

    /* Update the Volume and Vnode VVs. */
    UpdateVVs(&V_versionvector(volptr), &Vnode_vv(vptr), &UpdateSet);
    
    SetCOP2Pending(Vnode_vv(vptr));
}

/*
  COP2Update: Increment the version vector of an object.
  Only increment slots for servers that succeeded in COP1. 
*/

static void COP2Update(Volume *volptr, Vnode *vptr, 
			ViceVersionVector *UpdateSet, 
		       vmindex *freed_indices) 
{
    /* Look up the VRDB entry. */
    vrent *vre = VRDB.find(V_groupId(volptr));
    if (!vre) 
	    Die("COP2Update: VSG not found!");

    /* Look up the index of this host. */
    int ix = vre->index(ThisHostAddr);
    if (ix < 0) 
	    Die("COP2Update: this host not found!");

    SLog(2,  "COP2Update: Fid = (%x),(%x.%x.%x)",
	 V_groupId(volptr), V_id(volptr), vptr->vnodeNumber, 
	 vptr->disk.uniquifier);

    /* if the result was success everywhere, truncate the log */
    int i;
    if (vptr->disk.type == vDirectory) {
	unsigned long Hosts[VSG_MEMBERS];
	vv_t checkvv;
	vre->GetHosts(Hosts);
	vre->HostListToVV(Hosts, &checkvv);
	for (i = 0; i < VSG_MEMBERS; i++) 
	    if (((&(checkvv.Versions.Site0))[i]) ^ 
		((&(UpdateSet->Versions.Site0))[i])) {
		    SLog(0, "Incomplete host set in COP2.\n");
		    break;
	    }

	if (i == VSG_MEMBERS && AllowResolution && V_RVMResOn(volptr) 
	    && freed_indices) 
	    TruncateLog(volptr, vptr, freed_indices);

    }
    
    /* do a cop2 only if the cop2 pending flag is set */
    if (COP2Pending(Vnode_vv(vptr))) {
	SLog(1,  "Cop2 is pending for fid 0x%x.%x.%x", 
		V_id(volptr), vptr->vnodeNumber, vptr->disk.uniquifier);
	/* Extract ThisHost from the UpdateSet. */
	int tmp = (int)(&(UpdateSet->Versions.Site0))[ix];
	(&(UpdateSet->Versions.Site0))[ix] = 0;
	
	/* Update the Volume and Vnode VVs. */
	UpdateVVs(&V_versionvector(volptr), &Vnode_vv(vptr), UpdateSet);
	(&(UpdateSet->Versions.Site0))[ix] = tmp;
	
	/* clear the pending flag */
	ClearCOP2Pending(Vnode_vv(vptr));
    } else {
	SLog(1,  "Cop2 is not pending for 0x%x.%x.%x",
	     V_id(volptr), vptr->vnodeNumber, vptr->disk.uniquifier);
    }

}


void UpdateVVs(ViceVersionVector *VVV, ViceVersionVector *VV, ViceVersionVector *US) {
    if (SrvDebugLevel >= 2) {
	SLog(2,  "\tVVV = [%d %d %d %d %d %d %d %d]",
		VVV->Versions.Site0, VVV->Versions.Site1,
		VVV->Versions.Site2, VVV->Versions.Site3,
		VVV->Versions.Site4, VVV->Versions.Site5,
		VVV->Versions.Site6, VVV->Versions.Site7);
	SLog(2,  "\tVV = [%d %d %d %d %d %d %d %d]",
		VV->Versions.Site0, VV->Versions.Site1,
		VV->Versions.Site2, VV->Versions.Site3,
		VV->Versions.Site4, VV->Versions.Site5,
		VV->Versions.Site6, VV->Versions.Site7);
	SLog(2,  "\tUS = [%d %d %d %d %d %d %d %d]",
		US->Versions.Site0, US->Versions.Site1,
		US->Versions.Site2, US->Versions.Site3,
		US->Versions.Site4, US->Versions.Site5,
		US->Versions.Site6, US->Versions.Site7);
    }

    AddVVs(VVV, US);
    AddVVs(VV, US);
}

extern int pollandyield;
void PollAndYield() {
    if (!pollandyield) 
	return;

    /* Do a polling select first to make threads with pending I/O runnable. */
    (void)IOMGR_Poll();

    SLog(100,  "Thread Yielding");
    int lwprc = LWP_DispatchProcess();
    if (lwprc != LWP_SUCCESS)
	SLog(0,  
		"PollAndYield: LWP_DispatchProcess failed (%d)", 
		lwprc);
    SLog(100,  "Thread Yield Returning");
}


/*
  BEGIN_HTML
  <a name="ViceGetVolVS"><strong>Return the volume version vector for the specified
  volume, and establish a volume callback on it</strong></a> 
  END_HTML
*/
long FS_ViceGetVolVS(RPC2_Handle cid, VolumeId Vid, RPC2_Integer *VS,
		  CallBackStatus *CBStatus)
{
    long errorCode = 0;
    Volume *volptr;
    VolumeId rwVid;
    ViceFid fid;
    ClientEntry *client = 0;
    int ix, count;

    SLog(1, "ViceGetVolVS for volume 0x%x", Vid);

    errorCode = RPC2_GetPrivatePointer(cid, (char **)&client);
    if(!client || errorCode) {
	SLog(0, "No client structure built by ViceConnectFS");
	return(errorCode);
    }

    /* now get the version stamp for Vid */
    rwVid = Vid;
    if (!XlateVid(&rwVid, &count, &ix)) {
	SLog(1, "GetVolVV: Couldn't translate VSG %u", Vid);
	errorCode = EINVAL;
	goto Exit;
    }

    SLog(9, "GetVolVS: Going to get volume %u pointer", rwVid);
    volptr = VGetVolume((Error *) &errorCode, rwVid);
    SLog(1, "GetVolVS: Got volume %u: error = %d", rwVid, errorCode);
    if (errorCode) {
	SLog(0,  "ViceGetVolVV, VgetVolume error %s", ViceErrorMsg((int)errorCode));
	/* should we check to see if we must do a putvolume here */
	goto Exit;
    }

    *VS = (&(V_versionvector(volptr).Versions.Site0))[ix];
    VPutVolume(volptr);

    /* 
     * add a volume callback. don't need to use CodaAddCallBack
     * because we always send in the VSG volume id.
     */
    *CBStatus = NoCallBack;

    fid.Volume = Vid; 
    fid.Vnode = fid.Unique = 0;
    if (AddCallBack(client->VenusId, &fid))
	*CBStatus = CallBackSet;

 Exit:
    SLog(2, "ViceGetVolVS returns %s\n",
	   ViceErrorMsg((int)errorCode));

    return(errorCode);
}

void GetMyVS(Volume *volptr, RPC2_CountedBS *VSList, RPC2_Integer *MyVS) {
    vrent *vre;

    *MyVS = 0;
    if (VSList->SeqLen == 0) return;

    /* Look up the VRDB entry. */
    vre = VRDB.find(V_groupId(volptr));
    if (!vre) Die("GetMyVS: VSG not found!");

    /* Look up the index of this host. */
    int ix = vre->index(ThisHostAddr);
    if (ix < 0) Die("GetMyVS: this host not found!");

    /* get the version stamp from our slot in the vector */
    *MyVS = ((RPC2_Unsigned *) VSList->SeqBody)[ix];

    SLog(1, "GetMyVS: 0x%x, incoming stamp %d", 
	   V_id(volptr), *MyVS);

    return;
}

void SetVSStatus(ClientEntry *client, Volume *volptr, RPC2_Integer *NewVS, 
		 CallBackStatus *VCBStatus) {

    /* Look up the VRDB entry. */
    vrent *vre = VRDB.find(V_groupId(volptr));
    if (!vre) Die("SetVSStatus: VSG not found!");

    /* Look up the index of this host. */
    int ix = vre->index(ThisHostAddr);
    if (ix < 0) Die("SetVSStatus: this host not found!");

    *VCBStatus = NoCallBack;

    SLog(1, "SetVSStatus: 0x%x, client %d, server %d", 
	   V_id(volptr), *NewVS, (&(V_versionvector(volptr).Versions.Site0))[ix]);

    /* check the version stamp in our slot in the vector */
    if (*NewVS == (&(V_versionvector(volptr).Versions.Site0))[ix]) {
	/* 
	 * add a volume callback. don't need to use CodaAddCallBack because 
	 * we always send in the VSG volume id.  
	 */
	ViceFid fid;
	fid.Volume = V_id(volptr);
	fid.Vnode = fid.Unique = 0;
	*VCBStatus = AddCallBack(client->VenusId, &fid);
    } else 
	*NewVS = 0;

    SLog(1, "SetVSStatus: 0x%x, NewVS %d, CBstatus %d", 
		V_id(volptr), *NewVS, *VCBStatus);
    return;
}


/*
  BEGIN_HTML
  <a name="ViceValidateVols"><strong>Takes a list of volumes and
  corresponding version stamps from a client, and returns a vector indicating
  for each volume whether or the version stamp supplied is current, and whether or not 
  a callback was established for it.</strong></a> 
  END_HTML
*/
long FS_ViceValidateVols(RPC2_Handle cid, RPC2_Integer numVids,
		      ViceVolumeIdStruct Vids[], RPC2_CountedBS *VSBS,
		      RPC2_CountedBS *VFlagBS)
{
    long errorCode = 0;
    ClientEntry *client = 0;

    SLog(1, "ViceValidateVols, (%d volumes)", numVids);

    errorCode = RPC2_GetPrivatePointer(cid, (char **)&client);
    if(!client || errorCode) {
	SLog(0, "No client structure built by ViceConnectFS");
	return(errorCode);
    }

    /* check the piggybacked volumes */
    VFlagBS->SeqLen = 0;
    VFlagBS->SeqBody = (RPC2_ByteSeq) malloc((int) numVids);
    bzero((char *) VFlagBS->SeqBody, (int) numVids);
    VFlagBS->SeqLen = numVids;

    for (int i = 0; i < numVids; i++) {
	int error, index, ix, count;
	Volume *volptr;
	VolumeId rwVid;
	RPC2_Integer myVS;

    	rwVid = Vids[i].Vid;
	if (!XlateVid(&rwVid, &count, &ix)) {
	    SLog(1, "ValidateVolumes: Couldn't translate VSG %x", 
		   Vids[i].Vid);
	    goto InvalidVolume;
        }

	SLog(9,  "ValidateVolumes: Going to get volume %x pointer", 
	       rwVid);
	volptr = VGetVolume((Error *) &error, rwVid);
	SLog(1, "ValidateVolumes: Got volume %x: error = %d ", 
	       rwVid, error);
	if (error){
	    SLog(0,  "ViceValidateVolumes, VgetVolume error %s", 
		   ViceErrorMsg((int)error));
	    /* should we check to see if we must do a putvolume here */
	    goto InvalidVolume;
        }

	myVS = (&(V_versionvector(volptr).Versions.Site0))[ix];
	VPutVolume(volptr);

	/* check the version stamp in our slot in the vector */
	index = i * count + ix;
	if (ntohl(((RPC2_Unsigned *) VSBS->SeqBody)[index]) == myVS) {
	    SLog(8, "ValidateVolumes: 0x%x ok, adding callback", 
		   Vids[i].Vid);
	    /* 
	     * add a volume callback. don't need to use CodaAddCallBack because 
	     * we always send in the VSG volume id.  
	     */
	    ViceFid fid;
	    fid.Volume = Vids[i].Vid;
	    fid.Vnode = fid.Unique = 0;
	    VFlagBS->SeqBody[i] = AddCallBack(client->VenusId, &fid);

	    continue;
        }

 InvalidVolume:
	SLog(0, "ValidateVolumes: 0x%x failed!", Vids[i].Vid);
	VFlagBS->SeqBody[i] = 255;
    }

    SLog(2, "ValidateVolumes returns %s\n", 
	   ViceErrorMsg((int)errorCode));

    return(errorCode);
}
