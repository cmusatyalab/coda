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

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/time.h>
#include <netinet/in.h>
#include "coda_assert.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <rpc2.h>
#include <inodeops.h>
#include <util.h>
#include <rvmlib.h>
#include "coda_string.h"

#include <prs.h>
#include <al.h>
#ifdef __cplusplus
}
#endif __cplusplus

#include <olist.h>
#include <errors.h>
#include <vcrcommon.h>
#include <volume.h>
#include <srv.h>
#include <vlist.h>
#include <operations.h>
#include <dlist.h>
#include <res.h>
#include <vrdb.h>
#include <volume.h>
#include <lockqueue.h>

#include "rescomm.h"
#include "resutil.h"
#include "resforce.h"

#define	EMPTYDIRBLOCKS	    2

extern void ChangeDiskUsage(Volume *, int);
extern int FetchFileByName(RPC2_Handle, char *, ClientEntry *);

static int GetOpList(char *, olist *);
static int ObtainDirOps(PDirEntry, void *);
static int ForceDir(vle *, Volume *, VolumeId , olist *, dlist *, int *);
static int CheckForceDirSemantics(olist *, Volume *, Vnode *);


void UpdateRunts(res_mgrpent *mgrp, ViceVersionVector **VV,
		 ViceFid *Fid) {

    SLog(9,  "UpdateRunts: Entered for Fid %s", FID_(Fid));
    int runtexists = 0;
    int isrunt[VSG_MEMBERS];
    int nonruntdir;
    ViceStatus vstatus;
    RPC2_BoundedBS al;
    char buf[(SIZEOF_LARGEDISKVNODE - SIZEOF_SMALLDISKVNODE)];
    char filename[50];

    /* check if there are any runts */
    {
	runtexists = RuntExists(VV, VSG_MEMBERS, isrunt, &nonruntdir);
	if (!runtexists) {
	    SLog(9,  "UpdateRunts: no runt exists");
	    return;
	}
	if (nonruntdir == -1) {
	    SLog(0,  "UpdateRunts: No non-runt directory available ");
	    return;
	}
    }

    SLog(9,  "UpdateRunts runtexists = %d", runtexists);

    /* fetch directory ops from the non-runt site */
    {
	SE_Descriptor	sid;
	
	al.MaxSeqLen = (SIZEOF_LARGEDISKVNODE - SIZEOF_SMALLDISKVNODE);
	al.SeqLen  = 0;
	al.SeqBody = (RPC2_ByteSeq)buf;
	memset(&sid, 0, sizeof(SE_Descriptor));
	sid.Tag = SMARTFTP;
	sid.Value.SmartFTPD.Tag = FILEBYNAME;
	sid.Value.SmartFTPD.FileInfo.ByName.ProtectionBits = 0644;
	sid.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
	sid.Value.SmartFTPD.ByteQuota = -1;
	sprintf(filename, "/tmp/forceXXXXXX");
	mktemp(filename);
	strcpy(sid.Value.SmartFTPD.FileInfo.ByName.LocalFileName,
	       filename);
	SLog(9,  "UpdateRunts: Going to GetForceDirOps");
	if (Res_GetForceDirOps(mgrp->rrcc.handles[nonruntdir], Fid, &vstatus,
			       &al, &sid)) {
	    unlink(filename);
	    return;
	}
    }
    /* force directory ops onto runt sites */
    {
	SLog(9,  "UpdateRunts: Forcing Directories onto runts");
	int forceError;
	SE_Descriptor	sid;

	memset(&sid, 0, sizeof(SE_Descriptor));
	sid.Tag = SMARTFTP;
	sid.Value.SmartFTPD.Tag = FILEBYNAME;
	sid.Value.SmartFTPD.FileInfo.ByName.ProtectionBits = 0644;
	sid.Value.SmartFTPD.TransmissionDirection = CLIENTTOSERVER;
	sid.Value.SmartFTPD.ByteQuota = -1;
	strcpy(sid.Value.SmartFTPD.FileInfo.ByName.LocalFileName,
	       filename);
	ARG_MARSHALL(OUT_MODE, RPC2_Integer, forceErrorvar, forceError, VSG_MEMBERS);
	ARG_MARSHALL(IN_OUT_MODE, SE_Descriptor, sidvar, sid, VSG_MEMBERS);

	SLog(0, "UpdateRunts: Owner is %d\n", vstatus.Owner);
	/* SHOULD PROBABLY BLACK OUT NON RUNT OBJECTS FOR M-RPC */
	MRPC_MakeMulti(DoForceDirOps_OP, DoForceDirOps_PTR,
		       VSG_MEMBERS, mgrp->rrcc.handles, 
		       mgrp->rrcc.retcodes, mgrp->rrcc.MIp,
		       0, 0, Fid, &vstatus, &al,
		       forceErrorvar_ptrs, sidvar_bufs);
	mgrp->CheckResult();
	unlink(filename);
    }
    /* check return codes */
    {
	for (int i = 0; i < VSG_MEMBERS; i++)
	    if (VV[i] && isrunt[i]) {
		if (mgrp->rrcc.retcodes[i] == 0){
		    SLog(9,  "UpdateRunts: Succesfully forced runt %d", 
			    i);
		    /* update the vv[i] slot */
		    *(VV[i]) = vstatus.VV;
		}
		else 
		    SLog(0,  "UpdateRunts: Error %d from force[%d]", 
			    mgrp->rrcc.retcodes[i], i);
	    }
    }
}

extern long RS_ForceVV(RPC2_Handle, ViceFid *, ViceVersionVector *, ViceStatus *);

/* obsolete routine - used by Weakly equal file resolution also */
long RS_ForceDirVV(RPC2_Handle RPCid, ViceFid *Fid, ViceVersionVector *VV, 
		   ViceStatus  *status) {
    return(RS_ForceVV(RPCid, Fid, VV, status));
}

long RS_DoForceDirOps(RPC2_Handle RPCid, ViceFid *Fid,
		      ViceStatus *status, 
		      RPC2_BoundedBS *AccessList,
		      RPC2_Integer *rstatus,
		      SE_Descriptor *BD) 
{
    Vnode *dirvptr = 0;
    Volume *volptr = 0;
    VolumeId VSGVolnum = Fid->Volume;
    long repvolid = Fid->Volume;
    int errorCode = 0;
    *rstatus = 0;
    olist *forceList = 0;
    dlist *vlist = new dlist((CFN)VLECmp);
    vle *pv = 0;
    int deltablocks = 0;
    char *filename = NULL;
    char buf[20];

    SLog(9,  "RS_DoForceDirOps: Enter Fid %s", FID_(Fid));

    conninfo *cip = GetConnectionInfo(RPCid);
    if (cip == NULL){
	SLog(0,  "RS_DoForceDirOps %s: Couldnt get conninfo", FID_(Fid));
	return(EINVAL);
    }    
    if (!XlateVid(&Fid->Volume)) {
	SLog(0,  "RS_DoForceDirOps: Couldnt Xlate VSG %x", Fid->Volume);
	*rstatus = EINVAL;
	return(EINVAL);
    }
    
    /* get objects */
    {
	pv = AddVLE(*vlist, Fid);
	pv->d_inodemod = 1;
	if ((errorCode =GetFsObj(Fid, &volptr, &pv->vptr, WRITE_LOCK, NO_LOCK,
				 0, 0, pv->d_inodemod))) {
	    *rstatus = EINVAL;
	    goto FreeLocks;
	}
	dirvptr = pv->vptr;
    }
    
    /* make sure volume is locked by coordinator */
    if (V_VolLock(volptr).IPAddress != cip->GetRemoteHost()) {
	SLog(0,  "RS_DoForceDirOps: Volume not locked by coordinator");
	errorCode = EWOULDBLOCK;
	goto FreeLocks;
    }
    if (!IsRunt(&Vnode_vv(dirvptr))) {
	/* only allowed to force over a runt object */
	errorCode = EINVAL;
	*rstatus = RES_NOTRUNT;
	SLog(0,  "RS_DoForceDirOps: Object being force (%x.%x.%x) not a runt ",
		Fid->Volume, Fid->Vnode, Fid->Unique);
	goto FreeLocks;
    }

    /* fetch dir op file */
    {
	strcpy(buf, "/tmp/dodir.XXXXXXX");
	filename = mktemp(buf);
	errorCode = FetchFileByName(RPCid, filename, NULL);
	if (errorCode) {
	    SLog(0,  "RS_DoForceDirOps: Error %d in fetching op file", errorCode);
	    goto FreeLocks;
	}
    }

    /* parse list of operations */
    {
	SLog(19,  "RS_DoForceDirOps: going to parse file %s", filename);
	forceList = new olist;
	if (GetOpList(filename, forceList) != 0) {
	    SLog(0,  "RS_DoForceDirOps: error during GetOpList");
	    errorCode = EINVAL;
	    *rstatus = RES_BADOPLIST;
	    goto FreeLocks;
	}
    }

    /* do semantic checking */
    {
	if ((errorCode = CheckForceDirSemantics(forceList, volptr, dirvptr))) {
	    SLog(0,  "RS_DoForceDirOps: error %d during Sem Checking");
	    *rstatus = EINVAL;
	    goto FreeLocks;
	}
    }

    /* set access list and status first; needed for creating runt child dirs */
    {
	CODA_ASSERT(AccessList->SeqLen == VAclSize(dirvptr));
	AL_ntohAlist((AL_AccessList *)(AccessList->SeqBody));
	bcopy((const void *)AccessList->SeqBody, (void *) VVnodeACL(dirvptr), VAclSize(dirvptr));
	dirvptr->disk.author = status->Author;
	dirvptr->disk.owner = status->Owner;
	dirvptr->disk.modeBits = status->Mode;
	dirvptr->disk.unixModifyTime = status->Date;
	CodaBreakCallBack(0, Fid, VSGVolnum);
    }

    SLog(0,
	   "RS_DoForceDirOps: Owner just before forcing dir contents is %d\n",
	   dirvptr->disk.owner);
    /* do the actual directory ops */
    {
	SLog(9,  "RS_DoForceDirOps: Going to force directory(%x.%x.%x)",
		repvolid, dirvptr->vnodeNumber, dirvptr->disk.uniquifier);
	if ((errorCode = ForceDir(pv, volptr, repvolid, forceList, 
				 vlist, &deltablocks))) {
	    SLog(0,  "Error %d in ForceDir", errorCode);
	    *rstatus = EINVAL;
	    goto FreeLocks;
	}
	/* set the vv of the top level directory and do the cop1 */
	pv->vptr->disk.versionvector = status->VV;
    }

    if (AllowResolution && V_RVMResOn(volptr) && !errorCode) {
	SLog(9,  
	       "RS_DoForceDirOps: Going to spool recoverable log record \n");
	if ((errorCode = SpoolVMLogRecord(vlist, pv->vptr, volptr, 
					 &status->VV.StoreId, ResolveNULL_OP, 0))) 
	    SLog(0, 
		   "RS_DoForceDirOps: Error %d during SpoolVMLogRecord\n", 
		   errorCode);
    }
	
  FreeLocks:
    if (forceList){
	olink *p;
	while((p = forceList->get()))
	    delete p;
	delete forceList;
    }
    if (dirvptr) {
	SLog(0, "RS_DoForceDirOps: Owner just before commiting is %d\n",
	     dirvptr->disk.owner);
    }
    PutObjects(errorCode, volptr, NO_LOCK, vlist, deltablocks, 1);
    if (filename && unlink(filename) == -1) 
	SLog(0,  "RS_DoForceDirOps: Error %d occured unlinking %s",
		filename);    
    return(errorCode);
}

/* Given the contents of a directory, derive the ops needed to force this
   directory onto a runt version */
long RS_GetForceDirOps(RPC2_Handle RPCid, ViceFid *Fid, ViceStatus *status,
		       RPC2_BoundedBS *AccessList, SE_Descriptor *BD) 
{
    Vnode *vptr = 0;
    Volume *volptr = 0;
    long errorcode = 0;
    SE_Descriptor sid;
    int fd = 0;
    char buf[20];
    char *filename = 0;
    PDirHandle dir;
    struct getdiropParm gdop;
    diroplink *dop;

    AccessList->SeqLen = 0;

    if (!XlateVid(&Fid->Volume)) {
       SLog(0,  "RS_GetForceDirOps: Couldnt Xlate VSG %x",
               Fid->Volume);
       return(EINVAL);
    }

    if ((errorcode =GetFsObj(Fid, &volptr, &vptr, READ_LOCK, NO_LOCK, 0, 0, 0))) {
	SLog(0,  "RS_GetForceDirOps:GetFsObj returns error %d",
		errorcode);
	errorcode = EINVAL;
	goto FreeLocks;
    }

    if (VAclSize(vptr) > AccessList->MaxSeqLen) {
	SLog(0,  "RS_GetForceDirOps: VAclSize %d too big for the Accesslist %d",
	     VAclSize(vptr), AccessList->MaxSeqLen);
	errorcode = EINVAL;
	goto FreeLocks;
    }

    /* get the ops */
    dir = VN_SetDirHandle(vptr);
    gdop.volptr = volptr;
    gdop.oplist = new olist();
    DH_EnumerateDir(dir, ObtainDirOps, (void *) &gdop);
    VN_PutDirHandle(vptr);
    /* open file to store directory ops */
    strcpy(buf, "/tmp/dirop.XXXXXXX");
    filename = mktemp(buf);
    fd = open(filename, O_RDWR | O_TRUNC | O_CREAT, 0777);
    if (fd < 0) {
	SLog(0,  "RS_GetForceDirOps: Error creating file %s",
		filename);
	errorcode = EIO;
	goto FreeLocks;
    }
    while((dop = (diroplink *) gdop.oplist->get())){
	dop->hton();
	CODA_ASSERT(dop->write(fd) == 0);
	delete dop;
    }
    delete gdop.oplist;
    close(fd);
    
    /* set up vicestatus */
    SetStatus(vptr, status, 0, 0);

    /* convert acl into network order */
    bcopy((const void *)VVnodeACL(vptr), (void *)AccessList->SeqBody, 
	  VAclSize(vptr));
    AccessList->SeqLen = VAclSize(vptr);
    AL_htonAlist((AL_AccessList *)(AccessList->SeqBody));

    /* transfer back the file */
    memset(&sid, 0, sizeof(SE_Descriptor));
    sid.Tag = SMARTFTP;
    sid.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
    sid.Value.SmartFTPD.Tag = FILEBYNAME;
    sid.Value.SmartFTPD.FileInfo.ByName.ProtectionBits = 0777;
    strcpy(sid.Value.SmartFTPD.FileInfo.ByName.LocalFileName, 
	   filename);
    if ((errorcode = RPC2_InitSideEffect(RPCid, &sid))
	<= RPC2_ELIMIT) {
	SLog(0, "RS_GetForceDirOps: InitSideEffect failed %d", errorcode);
	errorcode = EINVAL;
	goto FreeLocks;
    }

    if ((errorcode = RPC2_CheckSideEffect(RPCid, &sid, SE_AWAITLOCALSTATUS)) 
	<= RPC2_ELIMIT) {
	if (errorcode == RPC2_SEFAIL1) errorcode = EIO;
	SLog(0,  "RS_GetForceDirOps: CheckSideEffect failed %d",
		errorcode);
	goto FreeLocks;
    }
  FreeLocks:
    /* release lock on vnode and put the volume */
    Error filecode = 0;
    if (vptr) {
	VPutVnode(&filecode, vptr);
	CODA_ASSERT(filecode == 0);
    }
    PutVolObj(&volptr, NO_LOCK);
    if (filename && unlink(filename) == -1) 
	SLog(0,  "RS_GetForceDirOps: Error %d occured unlinking %s",
		filename);    
    SLog(9,  "RS_GetDirOps: returns %d", errorcode);
    return(errorcode);
}


/* called by EnumerateDir - adds dir op entry to the list(passed in gdop) */
int ObtainDirOps(PDirEntry de, void *data) 
{
    struct getdiropParm *gdop = (struct getdiropParm *)data;
    char *name = de->name;
    VnodeId vnode;
    Unique_t unique;
    FID_NFid2Int(&de->fid, &vnode, &unique);

    dirop_t op = CreateF;
    SLog(9,  "Entering ObtainDirOps for %s(%x.%x)",
	    name, vnode, unique);
    /* skip over "." and ".." entries */
    if (!strcmp(".", name) || !strcmp("..", name)) return(0);

    /* check what kind of vnode it is */
    ViceFid Fid;
    Vnode *vptr = 0;
    Fid.Volume = V_id(gdop->volptr);
    Fid.Vnode = vnode;
    Fid.Unique = unique;
    CODA_ASSERT(GetFsObj(&Fid, &(gdop->volptr), &vptr, READ_LOCK, NO_LOCK, 1, 1, 0) == 0);
    
    if (vptr->disk.type == vDirectory)
	op = CreateD;
    else if (vptr->disk.type == vSymlink)
	op = CreateS;
    else if (vptr->disk.type == vFile) {
	/* check if it should be actual file creation or just a link */
	olist_iterator	next(*(gdop->oplist));
	diroplink	*dopl;
	while((dopl = (diroplink *)next())) {
	    if ((long)vnode == dopl->vnode && (long)unique == dopl->unique){
		CODA_ASSERT(dopl->op == CreateF || dopl->op == CreateL);
		break;
	    }
	}
	if (dopl)
	    /* found an entry */
	    op = CreateL;
	else
	    op = CreateF;
    }
    /* put the vnode back */
    int error = 0;
    VPutVnode((Error *)&error, vptr);
    CODA_ASSERT(error == 0);

    CODA_ASSERT(strlen(name) < (DIROPNAMESIZE));
    diroplink	*direntry = new diroplink(op, vnode, unique, name);

    /* now insert the entry into the list */
    gdop->oplist->append(direntry);
    
    return(0);
}

/* opens the file <filename> and returns the list of dir operations in List */
int GetOpList(char *filename, olist *List) 
{
    SLog(49, "In GetOpList: Filename (%s), List(0x%x)",
	    filename, List);
    int fd = ::open(filename, O_RDONLY, 0644);
    if (fd < 0) return(-1);
    diroplink	*direntry = (diroplink *)malloc(sizeof(diroplink));
    long error;

    while((error = ::read(fd, direntry, 
			  (int) sizeof(diroplink))) == sizeof(diroplink)) {
	direntry->ntoh();
	diroplink *newlink = new diroplink(direntry->op, direntry->vnode, 
					   direntry->unique, direntry->name);
	List->append(newlink);
    }
    free(direntry);
    close(fd);
    SLog(49,  "GetOpList: returns(%d)", error == 0 ? 0 : -1);
    if (error == 0) return(0);
    else return(-1);
}

/* check semantics for force dir */
static int CheckForceDirSemantics(olist *flist, Volume *volptr, Vnode *dirvptr) {
    SLog(19,  "Entering CheckForceDirSemantics %x.%x", dirvptr->vnodeNumber, 
	    dirvptr->disk.uniquifier);
    int deltablocks = 0;
    int volindex = V_volumeindex(volptr);
    olist_iterator next(*flist);
    diroplink *p;

    while ((p = (diroplink *)next()) != NULL) 
	switch (p->op) {
	  case CreateD:
	    if (ObjectExists(volindex, vLarge, 
			     vnodeIdToBitNumber(p->vnode),
			     p->unique))
		return(EINVAL);
	    deltablocks += EMPTYDIRBLOCKS;
	    break;
	  case CreateF:
	  case CreateS:
	    if (ObjectExists(volindex, vSmall, 
			     vnodeIdToBitNumber(p->vnode),
			     p->unique))
		return(EINVAL);
	    deltablocks += nBlocks(0);
	    break;
	  case CreateL:
	    if (ObjectExists(volindex, vSmall, 
			     vnodeIdToBitNumber(p->vnode),
			     p->unique))
		return(EINVAL);
	    break;
	  default:
	    SLog(0,  "CheckForceDirSemantics: Illegal op %d", p->op);
	    return(EINVAL);
	}
    /* check if there is enough disk space */
    if (deltablocks) {
	int errorCode;
	if ((errorCode = CheckDiskUsage(volptr, deltablocks)))
	    return(errorCode);
    }

    SLog(19,  "CheckForceDirSemantics: Returning 0");
    return(0);
}

/* Forces the ops specified in forceList onto the directory.
 * Adds the newly created vnode pointers to the commitvlist.
 */
int ForceDir(vle *pv, Volume *volptr, VolumeId repvolid, 
	     olist *forceList, dlist *vlist, int *deltablocks) 
{
    SLog(9,  "Entering ForceDir(%x.%x.%x)", 
	    repvolid, pv->vptr->vnodeNumber, pv->vptr->disk.uniquifier);
    diroplink *p;
    ViceFid parentFid;
    int errorCode = 0;
    
    *deltablocks = 0;
    parentFid.Volume = V_id(volptr);
    parentFid.Vnode = pv->vptr->vnodeNumber;
    parentFid.Unique = pv->vptr->disk.uniquifier;

    olist_iterator next(*forceList);
    while (!errorCode && (p = (diroplink *)next())) {
	ViceFid cFid;

	cFid.Volume = parentFid.Volume;
	cFid.Vnode = p->vnode;
	cFid.Unique = p->unique;

	switch(p->op) {
	  case CreateD:
	    {
		    SLog(9,  "ForceDir: CreateD: %x.%x.%x %s",
			 cFid.Volume, cFid.Vnode, cFid.Unique, p->name);
		int tblocks = 0;
		vle *cv = AddVLE(*vlist, &cFid);
		cv->d_inodemod = 1;
		if ((errorCode = AllocVnode(&cv->vptr, volptr, 
					   (ViceDataType)vDirectory, &cFid, 
					   &parentFid, 0, 1, &tblocks)))
		    return(errorCode);
		*deltablocks += tblocks;
		tblocks = 0;
		
		PerformMkdir(0, repvolid, volptr, pv->vptr, cv->vptr, p->name, 
			     time(0), 0666, 0, NULL, &pv->d_cinode, &tblocks);
		*deltablocks += tblocks;
		
		cv->vptr->delete_me = 1;
	    }
	    break;
	  case CreateF:
	    {
		SLog(9,  "ForceDir: CreateF: %x.%x.%x %s",
			cFid.Volume, cFid.Vnode, cFid.Unique, p->name);
		int tblocks = 0;
		vle *cv = AddVLE(*vlist, &cFid);
		if ((errorCode = AllocVnode(&cv->vptr, volptr, (ViceDataType)vFile, &cFid,
					   &parentFid, 0, 1, &tblocks)))
		    return(errorCode);
		*deltablocks += tblocks;
		tblocks = 0;
		
		PerformCreate(0, repvolid, volptr, pv->vptr, cv->vptr, p->name,
			      time(0), 0666, 0, NULL, &pv->d_cinode, &tblocks);
		*deltablocks += tblocks;
		
		/*create the inode */
		cv->vptr->disk.dataVersion = 1;
		cv->f_finode = icreate((int) V_device(volptr), 0, (int) V_id(volptr),
				       (int) cv->vptr->vnodeNumber, 
				       (int) cv->vptr->disk.uniquifier,
				       (int) cv->vptr->disk.dataVersion);
		CODA_ASSERT(cv->f_finode > 0);
		cv->vptr->disk.inodeNumber = cv->f_finode;

		cv->vptr->delete_me = 1;
	    }
	    break;
	  case CreateL:
	    {
		SLog(9,  "ForceDir: CreateL: %x.%x.%x %s",
			cFid.Volume, cFid.Vnode, cFid.Unique, p->name);    
		int tblocks = 0;
		vle *cv = FindVLE(*vlist, &cFid);
		CODA_ASSERT(cv != 0);
		CODA_ASSERT(cv->vptr != 0);
		
		CODA_ASSERT(cv->vptr->disk.linkCount > 0);
		PerformLink(0, repvolid, volptr, pv->vptr, cv->vptr, p->name, 
			    time(0), 0, NULL, &pv->d_cinode, &tblocks);
		*deltablocks += tblocks;
	    }
	    break;
	  case CreateS:
	    {
		SLog(9,  "ForceDir: CreateS: %x.%x.%x %s",
			cFid.Volume, cFid.Vnode, cFid.Unique, p->name);
		int tblocks = 0;
		vle *cv = AddVLE(*vlist, &cFid);
		if ((errorCode = AllocVnode(&cv->vptr, volptr, (ViceDataType)vSymlink, &cFid, 
					   &parentFid, 0, 1, &tblocks)))
		    return(errorCode);
		*deltablocks += tblocks;
		tblocks = 0;
		
		PerformSymlink(0, repvolid, volptr, pv->vptr, cv->vptr, p->name, 
			       0, 0, time(0), 0666, 0, NULL, &pv->d_cinode, 
			       &tblocks);
		*deltablocks += tblocks;
		
		/*create the inode */
		cv->vptr->disk.dataVersion = 1;
		cv->f_finode = icreate((int) V_device(volptr), 0, (int) V_id(volptr),
				       (int) cv->vptr->vnodeNumber, 
				       (int) cv->vptr->disk.uniquifier,
				       (int) cv->vptr->disk.dataVersion);
		CODA_ASSERT(cv->f_finode > 0);
		cv->vptr->disk.inodeNumber = cv->f_finode;

		cv->vptr->delete_me = 1;
	    }
	    break;
	  default:
	    SLog(0,  "ForceDir: Illegal opcode %d", p->op);
	    return(EINVAL);
	}
    }
    /* undelete newly created vnodes */
    {
	dlist_iterator next(*vlist);
	vle *v;
	if (!errorCode)
	    while ((v = (vle *)next())) 
		if ((v->vptr->delete_me == 1) &&
		    (v->vptr->disk.linkCount > 0))
		    v->vptr->delete_me = 0;
    }
    return(errorCode);
}

/* RuntExists:
 *	Return 1 if runt exists in the group;
 *	       0 if no runt exists
 *	nonruntvv contains index of first non runt vv
 */
int RuntExists(ViceVersionVector **VV, int maxvvs, int *isrunt, 
		       int *NonRuntIndex) 
{
    int runtexists = 0;
    *NonRuntIndex = -1;

    for (int i = 0; i < maxvvs; i++) 
	isrunt[i] = 0;

  { /* drop scope for int i below; to avoid identifier clash */
    /* check if there are any runts */
    for (int i = 0; i < maxvvs; i++) {
	if (VV[i]){
	    if (IsRunt(VV[i])) {
		SLog(19,  "UpdateRunt: VV[%d] is a runt VV", i);
		runtexists = 1;
		isrunt[i] = 1;
		/* could break here */
	    }
	    else {
		SLog(19,  "UpdateRunt: VV[%d] is not a runt", i);
		if (*NonRuntIndex == -1)
		    *NonRuntIndex = i;
	    }
	}
    }
  } /* drop scope for int i above; to avoid identifier clash */

    return(runtexists);
}

/* implementation of dirop class */
diroplink::diroplink(dirop_t dop, long vn, long unq, char *entname) {
    op = dop;
    vnode = vn;
    unique = unq;
    strcpy(name, entname);
}

diroplink::~diroplink() {

}

void diroplink::hton() {
    this->op = (dirop_t)htonl(this->op);
    this->vnode = htonl(this->vnode);
    this->unique = htonl(this->unique);
    /* name remains unchanged */
}

void diroplink::ntoh() {
    this->op = (dirop_t)ntohl(this->op);
    this->vnode = ntohl(this->vnode);
    this->unique = ntohl(this->unique);
    /* name remains unchanged */
}    

int diroplink::write(int fd) {
    if (::write(fd, this, (int) sizeof(diroplink)) != sizeof(diroplink)) return(-1);
    return(0);
}

/* 
commitlink::commitlink(ViceFid *fid, Vnode *v) {
    Fid = *fid;
    vptr = v;
}

commitlink::~commitlink() {
}
*/

