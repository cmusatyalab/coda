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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/time.h>
#include <ctype.h>
#include <varargs.h>

#include <unistd.h>
#include <stdlib.h>
#include "coda_string.h"

#include <struct.h>
#include <lwp/lwp.h>
#include <lwp/lock.h>
#include <rpc2/rpc2.h>
#include <rpc2/errors.h>
#include <util.h>
#include <codadir.h>
#include <inodeops.h>

#include <volutil.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <rvmlib.h>
#include <voltypes.h>
#include <vldb.h>
#include <vice.h>
#include <cvnode.h>
#include <volume.h>
#include <camprivate.h>
#include <recov.h>
#include <dump.h>
#include <fssync.h>
#include <al.h>
#include <rec_smolist.h>
#include <partition.h>
#include <viceinode.h>
#include <vutil.h>
#include <index.h>
#include <volhash.h>
#include <coda_globals.h>
#include <voldump.h>

static int RestoreVolume(DumpBuffer_t *, char *, char *, VolumeId *);
static int ReadLargeVnodeIndex(DumpBuffer_t *, Volume *);
static int ReadSmallVnodeIndex(DumpBuffer_t *, Volume *);
static int ReadVnodeDiskObject(DumpBuffer_t *, VnodeDiskObject *, DirInode **, Volume *, long *);

static int VnodePollPeriod = 16;  /* Number of vnodes restored per transaction */
extern void PollAndYield();
#define DUMPBUFSIZE 512000


/*
  S_VolRestore: Restore a volume from a dump file
*/ 
long S_VolRestore(RPC2_Handle rpcid, RPC2_String formal_partition,
		  RPC2_String formal_volname, RPC2_Unsigned *formal_volid)
{
    int status = 0;
    long rc = 0;
    ProgramType *pt;
    DumpBuffer_t *dbuf;
    char *DumpBuf;
    RPC2_HostIdent hid;
    RPC2_PortIdent pid;
    RPC2_SubsysIdent sid;
    RPC2_BindParms bparms;
    RPC2_Handle cid;
    RPC2_PeerInfo peerinfo;

    /* To keep C++ 2.0 happy */
    char *partition = (char *)formal_partition;
    char *volname = (char *)formal_volname;
    VolumeId *volid = (VolumeId *)formal_volid;
    
    
    CODA_ASSERT(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);

    VLog(9, "Entering S_VolRestore for %x--%s, partition %s", *volid, volname,
	partition);

    rc = VInitVolUtil(volumeUtility);
    if (rc != 0) {
	VLog(0, "S_VolRestore: VInitVolUtil failed with error code %d", rc);
	return(rc);
    }

    /* Avoid using a bogus partition. */
    if (DP_Get(partition) == NULL) {
	VLog(0, "VolRestore: %s is not in the partition list; not restored.", 
	     partition);
	VDisconnectFS();
	return VFAIL;
    }
    
    /* Verify that volid isn't already in use on this server. */
    if (*volid != 0) {
	if (HashLookup(*volid) != -1) {
	    VLog(0, "Restore: Volid %x already in use; restore aborted.",
		 *volid);
	    VDisconnectFS();
	    return VVOLEXISTS;
	}
    }

    /* To allow r/o replication reuse of volume names is allowed. */

    /* Set up a connection with the client. */
    if ((rc = RPC2_GetPeerInfo(rpcid, &peerinfo)) != RPC2_SUCCESS) {
	VLog(0,"VolRestore: GetPeerInfo failed with %s", 
	     RPC2_ErrorMsg((int)rc));
	VDisconnectFS();
	return rc;
    }

    hid = peerinfo.RemoteHost;
    pid = peerinfo.RemotePort;
    sid.Tag = RPC2_SUBSYSBYID;
    sid.Value.SubsysId = VOLDUMP_SUBSYSTEMID;
    bparms.SecurityLevel = RPC2_OPENKIMONO;
    bparms.SideEffectType = SMARTFTP;
    bparms.EncryptionType = 0;
    bparms.ClientIdent = NULL;
    bparms.SharedSecret = NULL;
    
    if ((rc = RPC2_NewBinding(&hid, &pid, &sid, &bparms, &cid))!=RPC2_SUCCESS) {
	VLog(0, "VolRestore: Bind to client failed, %s!", 
	     RPC2_ErrorMsg((int)rc));
	VDisconnectFS();
	return rc;
    }

    DumpBuf = (char *)malloc(DUMPBUFSIZE);
    if (!DumpBuf) {
	VLog(0, "VolRestore: Can't malloc buffer (%d)!", errno);
	VDisconnectFS();
	return VFAIL;
    }
    /* Initialize dump buffer. */
    dbuf = InitDumpBuf((byte *)DumpBuf, (long)DUMPBUFSIZE, *volid, cid);
    
    status = RestoreVolume(dbuf, partition, volname, volid);
    if (status != 0) {
	HashDelete(*volid);	       /* Just in case VCreateVolume was called */
    } 

    VDisconnectFS();
    VLog(2, "Restore took %d seconds to dump %d bytes.",
	 dbuf->secs, dbuf->nbytes);
    free(dbuf);
    free(DumpBuf);

    if (RPC2_Unbind(cid) != RPC2_SUCCESS) {
	    VLog(0, "S_VolNewDump: Can't close binding %s", 
	     RPC2_ErrorMsg((int)rc));
    }

    return(status?status:rc);
}

static int RestoreVolume(DumpBuffer_t *buf, char *partition, 
			 char *volname, VolumeId *volid)
{
    VolumeDiskData vol;
    struct DumpHeader header;
    Error error;
    register Volume *vp = NULL;
    VolumeId parentid;
    rvm_return_t status = RVM_SUCCESS;

    /* Get header information from the dump buffer. */
    if (!ReadDumpHeader(buf, &header)) {
	VLog(0, "Error reading dump header; aborted");
	return VFAIL;
    }
    VLog(9, "Volume id = %x, Volume name = %s", 
	 header.volumeId, header.volumeName);
    VLog(9, "Dump from backup taken from %x at %s", 
	 header.parentId, ctime((long *)&header.backupDate));
    VLog(9, "Dump uniquifiers %x -> %x", 
	 header.oldest, header.latest);
    
    if (header.Incremental) {
	VLog(0, "VolRestore Error -- trying to restore incremental dump!");
	return VFAIL;
    }
    
    if (ReadTag(buf) != D_VOLUMEDISKDATA) {
	VLog(0, "VolRestore: Volume header missing from dump; not restored");
	return VFAIL;
    }

    bzero((void *)&vol, sizeof(VolumeDiskData));
    if (!ReadVolumeDiskData(buf, &vol)) {
	VLog(0, "VolRestore: ReadVolDiskData failed; not restored.");
	return VFAIL;
    }

    int volumeType = vol.type;
    if ((volumeType == readwriteVolume) || (volumeType == replicatedVolume)) {
	VLog(0, "VolRestore: Dumped volume has Illegal type; not restored.");
	return VFAIL;
    }

    rvmlib_begin_transaction(restore);
    if (*volid != 0) {
	/* update Maxid of the volumes so that the volume routines will work */
	unsigned long maxid = *volid & 0x00FFFFFF;
	if (maxid > (SRV_RVM(MaxVolId) & 0x00FFFFFF)){
	    VLog(0, "Restore: Updating MaxVolId; maxid = %#x MaxVolId = %#x", 
		maxid, SRV_RVM(MaxVolId));
	    RVMLIB_MODIFY(SRV_RVM(MaxVolId),
			  (maxid + 1) | (SRV_RVM(MaxVolId) & 0xFF000000));
	}
    } else {
	*volid = VAllocateVolumeId(&error);
	if (error) {
	    VLog(0, "Unable to allocate volume id; restore aborted");
	    rvmlib_abort(VFAIL);
	    goto error;
	}
    }
    /* NOTE:  Do NOT set the parentId of restore RO volumes to the ORIGINAL
       parentID. If they are the same, then the restored volume will look
       like a RO copy of the parent and it would start being used instead
       of the parent in some cases.- still true, true for backups? dcs */
    parentid = (volumeType == readonlyVolume) ? *volid : vol.parentId;

    vp = VCreateVolume(&error, partition, *volid, parentid, 0, volumeType);
    if (error) {
	VLog(0,"Unable to create volume %x; not restored", *volid);
	rvmlib_abort(-1);
    }

    V_blessed(vp) = 0;
    VUpdateVolume(&error, vp);
    if (error) {
	VLog(0, "S_VolClone: Trouble updating voldata for %#x!", *volid);
	rvmlib_abort(VFAIL);
    }

    rvmlib_end_transaction(flush, &status);
	    error:
    if (status == 0)
	VLog(9, "restore createvol of %#x completed successfully", *volid);
    else {
	VLog(0, "restore: createvol failed.");
	return status;
    }
    
    /* Read in and create the large vnode list. */
    if (ReadTag(buf) != D_LARGEINDEX) {
	VLog(0, "Large Vnode Index not found; restore aborted");
	return VNOVNODE;
    }
    if (!ReadLargeVnodeIndex(buf, vp)) 
	return VNOVNODE;	/* Assume message is already printed. */

    /* Read in and create the small vnode list. */
    if (ReadTag(buf) != D_SMALLINDEX) {
	VLog(0, "Small Vnode Index not found; restore aborted");
	return VNOVNODE;
    }
    if (!ReadSmallVnodeIndex(buf, vp))
	return VNOVNODE;	/* Assume message is already printed. */

    /* Verify that we're really at the end of the dump. */
    if (!EndOfDump(buf)){
	VLog(0, "End of dump doesnt look good; restore aborted");
	return VFAIL;
    }

    /* Modify vp (data from create) with vol (data from the dump.) */
    if (strlen(volname) == 0) {
	    AssignVolumeName(&vol, vol.name,
		    (volumeType==readonlyVolume?".readonly":".restored"));
    } else {
	    strcpy(vol.name, volname);
    }

    ClearVolumeStats(&vol);
    CopyVolumeHeader(&vol, &V_disk(vp));	/* Doesn't use RVM */

    strcpy(V_partname(vp), partition);
    V_restoredFromId(vp) = vol.parentId;
    V_needsSalvaged(vp) = V_destroyMe(vp) = 0; /* Make sure it gets online. */
    V_inService(vp) = V_blessed(vp) = 1;
    
    VLog(0, "partname -%s-", V_partname(vp));
    rvmlib_begin_transaction(restore);

    VUpdateVolume(&error, vp);
    if (error) {
	VLog(0, "restore: Unable to rewrite volume header; restore aborted");
	rvmlib_abort(-1);
	return -1;
    }

    VDetachVolume(&error, vp); /* Let file server get its hands on it */
    if (error) {
	VLog(0, "restore: Unable to detach volume; restore aborted");
	rvmlib_abort(-1);
	return -1;
    }

    rvmlib_end_transaction(flush, &status);
    if (status == 0)
	VLog(9, "S_VolRestore: VUpdateVolume completed successfully");
    else {
	VLog(0, "restore: VUpdateVolume failed ");
	return status;
    }

    VLog(0, "Volume %x (%s) restored successfully", *volid, vol.name);
    return 0;
}

/* This is sort of conservative. When a volume is created, two null vnode
 * lists are created as well, but no vnodes should be added. So this routine
 * will free up the empty indexes and would delete any vnodes if there are any.
 */
static void FreeVnodeIndex(Volume *vp, VnodeClass vclass)
{
    rec_smolist *list = NULL;
    bit32 listsize = 0;
    int volindex = V_volumeindex(vp);
    VnodeDiskObject *vdo;
    rvm_return_t status = RVM_SUCCESS;

    rvmlib_begin_transaction(restore);

    if (vclass == vSmall){
	list = SRV_RVM(VolumeList[volindex]).data.smallVnodeLists;
	listsize = SRV_RVM(VolumeList[volindex]).data.nsmallLists;
	RVMLIB_MODIFY(SRV_RVM(VolumeList[volindex]).data.smallVnodeLists, 0); 
	RVMLIB_MODIFY(SRV_RVM(VolumeList[volindex]).data.nsmallvnodes, 0);
	RVMLIB_MODIFY(SRV_RVM(VolumeList[volindex]).data.nsmallLists, 0);
    } else if (vclass == vLarge){
	list = SRV_RVM(VolumeList[volindex]).data.largeVnodeLists;
	listsize = SRV_RVM(VolumeList[volindex]).data.nlargeLists;
	RVMLIB_MODIFY(SRV_RVM(VolumeList[volindex]).data.largeVnodeLists, 0); 
	RVMLIB_MODIFY(SRV_RVM(VolumeList[volindex]).data.nlargevnodes, 0);
	RVMLIB_MODIFY(SRV_RVM(VolumeList[volindex]).data.nlargeLists, 0);
    } else 
	CODA_ASSERT(0);
    /* now free the list */
    for (int i = 0; i < (int)listsize; i++){
	    rec_smolink *p;
	    while ((p = list[i].get())){
	     VLog(0, "Vol_Restore: Found a vnode on the new volume's %s list!",
		  (vclass == vLarge) ? "Large" : "Small");
	     vdo = strbase(VnodeDiskObject, p, nextvn);
	     rvmlib_rec_free((char *)vdo);
	     
	    }
    }	
    rvmlib_rec_free(((char *)list));

    rvmlib_end_transaction(flush, &status);
    CODA_ASSERT(status == 0);	/* Never aborts ... */
}

static int ReadLargeVnodeIndex(DumpBuffer_t *buf, Volume *vp)
{
    int num_vnodes, list_size = 0;
    rvm_return_t status = RVM_SUCCESS;
    long vnodenumber;
    int	nvnodes = 0;
    char    vbuf[SIZEOF_LARGEDISKVNODE];
    VnodeDiskObject *vdo = (VnodeDiskObject *)vbuf;
    rec_smolist *rlist;
    DirInode *dinode = NULL;
    DirInode *camdInode = NULL;
    register signed char tag;
    int volindex = V_volumeindex(vp);

    VLog(9, "Restore: Reading in large vnode array.");
    FreeVnodeIndex(vp, vLarge);
    while ((tag = ReadTag(buf)) > D_MAX && tag != EOF) {
	switch(tag) {
	    case 'v':
		ReadInt32(buf, (unsigned int *)&num_vnodes);
		break;
	    case 's':
		ReadInt32(buf, (unsigned int *)&list_size);
		break;
	    default:
		VLog(0, "Unexpected field of Vnode found; restore aborted");
		return FALSE;
	}
    }
    CODA_ASSERT(list_size > 0);
    PutTag(tag, buf);

    /* Set up a list structure. */
    rvmlib_begin_transaction(restore);

    rlist = (rec_smolist *)(rvmlib_rec_malloc(sizeof(rec_smolist) * list_size));
    rec_smolist *vmrlist = (rec_smolist *)malloc(sizeof(rec_smolist) * list_size);
    bzero((void *)vmrlist, sizeof(rec_smolist) * list_size);
    rvmlib_modify_bytes(rlist, vmrlist, sizeof(rec_smolist) * list_size);
    free(vmrlist);

    /* Update Volume structure to point to new vnode list. 
     * Doing this here so that if an abort happens during the loop the
     * commited vnodes will be scavanged.
     */
    RVMLIB_MODIFY(SRV_RVM(VolumeList[volindex]).data.nlargevnodes, num_vnodes);
    RVMLIB_MODIFY(SRV_RVM(VolumeList[volindex]).data.nlargeLists, list_size);
    RVMLIB_MODIFY(SRV_RVM(VolumeList[volindex]).data.largeVnodeLists, rlist);
    
    rvmlib_end_transaction(flush, &status);
    CODA_ASSERT(status == 0);			/* Never aborts... */

    VnodeDiskObject *camvdo;
    long tmp = 0, i = 0;
    while (i < num_vnodes) {
	rvmlib_begin_transaction(restore);
	do {
		int rc; 
		rc = ReadVnodeDiskObject(buf, vdo, &dinode, vp, &vnodenumber);
		if (rc) {
			status = -1;
			rvmlib_abort(status);
			return -1;
		}
	    VLog(19, "Just read vnode for index %d.", vnodenumber);
	    if (vdo->type != vNull){
		/* this vnode is allocated */
		/* copy the directory pages into rec storage */
		dinode->di_refcount = 1;
		DI_Copy(dinode, &camdInode);
		DI_VMFree(dinode);
		vdo->inodeNumber = (Inode)camdInode;
		camvdo=(VnodeDiskObject *)rvmlib_rec_malloc(SIZEOF_LARGEDISKVNODE);
		rvmlib_modify_bytes(&(camvdo->nextvn), &tmp, sizeof(long));
		rlist[vnodeIdToBitNumber(vnodenumber)].append(&(camvdo->nextvn));
		vdo->cloned = 0;
		vdo->vol_index = volindex;
		vdo->vnodeMagic = LARGEVNODEMAGIC;
		ViceLockClear((&vdo->lock));
		bcopy((const void *)&(camvdo->nextvn), (void *) &(vdo->nextvn), sizeof(rec_smolink));
		rvmlib_modify_bytes(camvdo, vdo, SIZEOF_LARGEDISKVNODE);
		nvnodes ++;
	    }
	} while ((i++ < num_vnodes) && (i % VnodePollPeriod));
	rvmlib_end_transaction(flush, &status);
	CODA_ASSERT(status == 0);			/* Never aborts... */
	VLog(9, "S_VolRestore: Did another series of Vnode restores.");
	PollAndYield();
    }
    VLog(9, "S_VolRestore: Done with vnode restores for large vnodes.");

    if (nvnodes != num_vnodes){	
	VLog(0, "ReadLargeVnodeIndex: #vnodes read != #vnodes dumped");
	return FALSE;
    }
    return TRUE;
}


static int ReadSmallVnodeIndex(DumpBuffer_t *buf, Volume *vp)
{
    long vnodenumber;
    int num_vnodes, list_size = 0;
    rvm_return_t status = RVM_SUCCESS;
    int	nvnodes = 0;
    char    vbuf[SIZEOF_SMALLDISKVNODE];
    VnodeDiskObject *vdo = (VnodeDiskObject *)vbuf;
    register signed char tag;
    rec_smolist *rlist;
    int volindex = V_volumeindex(vp);

    VLog(9, "Restore: Reading in small vnode array.");
    FreeVnodeIndex(vp, vSmall);
    while ((tag = ReadTag(buf)) > D_MAX && tag != EOF) {
	switch(tag) {
	    case 'v':
		ReadInt32(buf, (unsigned int *)&num_vnodes);
		break;
	    case 's':
		ReadInt32(buf, (unsigned int *)&list_size);
		break;
	    default:
		VLog(0, "Unexpected field of Vnode found; restore aborted");
		return -1;
	}
    }
    PutTag(tag, buf);

    /* Set up a list structure. */
    rvmlib_begin_transaction(restore);

    rlist = (rec_smolist *)(rvmlib_rec_malloc(sizeof(rec_smolist) * list_size));
    rec_smolist *vmrlist = (rec_smolist *)malloc(sizeof(rec_smolist) * list_size);
    bzero((void *)vmrlist, sizeof(rec_smolist) * list_size);
    rvmlib_modify_bytes(rlist, vmrlist, sizeof(rec_smolist) * list_size);
    free(vmrlist);

    /* Update Volume structure to point to new vnode list. 
     * Doing this here so that if an abort happens during the loop the
     * commited vnodes will be scavanged.
     */
    RVMLIB_MODIFY(SRV_RVM(VolumeList[volindex]).data.nsmallvnodes, num_vnodes);
    RVMLIB_MODIFY(SRV_RVM(VolumeList[volindex]).data.nsmallLists, list_size);
    RVMLIB_MODIFY(SRV_RVM(VolumeList[volindex]).data.smallVnodeLists, rlist);

    rvmlib_end_transaction(flush, &status);
    CODA_ASSERT(status == 0);			/* Never aborts... */
    
    VnodeDiskObject *camvdo;
    long    tmp = 0, i = 0;
    while (i < num_vnodes) {
	rvmlib_begin_transaction(restore);
	do {
	    ReadVnodeDiskObject(buf, vdo, NULL, vp, &vnodenumber);
	    VLog(19, "Just read vnode for index %d.", vnodenumber);
	    if (vdo->type != vNull){
		/* this vnode is allocated */
		camvdo=(VnodeDiskObject *)rvmlib_rec_malloc(SIZEOF_SMALLDISKVNODE);
		rvmlib_modify_bytes(&(camvdo->nextvn), &tmp, sizeof(long));
		rlist[vnodeIdToBitNumber(vnodenumber)].append(&(camvdo->nextvn));
		vdo->cloned = 0;
		vdo->vol_index = volindex;
		vdo->vnodeMagic = SMALLVNODEMAGIC;
		ViceLockClear((&vdo->lock));
		bcopy((const void *)&(camvdo->nextvn), (void *) &(vdo->nextvn), sizeof(rec_smolink));
		rvmlib_modify_bytes(camvdo, vdo, SIZEOF_SMALLDISKVNODE);
		nvnodes ++;
	    }
 	} while ((i++ < num_vnodes) && (i % VnodePollPeriod));

	rvmlib_end_transaction(flush, &status);
	if (status != 0)
	    return FALSE;
	VLog(9, "S_VolRestore: Did another series of Vnode restores.");
	PollAndYield();
    }
    VLog(9, "S_VolRestore: Done with vnode restores for small vnodes.");
    
    if (nvnodes != num_vnodes){
	VLog(0, "ReadSmallVnodeIndex: #vnodes read != #vnodes dumped");
	return FALSE;
    }
    return TRUE;
}

static int ReadVnodeDiskObject(DumpBuffer_t *buf, VnodeDiskObject *vdop, 
				DirInode **dinode, Volume *vp, 
				long *vnodeNumber)
{
    register signed char tag;
    *vnodeNumber = -1;
    tag = ReadTag(buf);
    if (tag == D_NULLVNODE){
	vdop->type = vNull;
	return 0;
    } else if (tag != D_VNODE){	/* Shouldn't this be an error? */
	PutTag(tag, buf);
	vdop->type = vNull;
	return 0;
    }
    
    if (!ReadInt32(buf, (unsigned int *)vnodeNumber) || 
	!ReadInt32(buf, (unsigned int *)&vdop->uniquifier)) {
	    VLog(0, "ReadVnodeDiskObject: Readstuff failed, aborting.");
	    return -1;
    }

    bzero((void *)&(vdop->nextvn), sizeof(rec_smolink));
    /* Let the following ReadTag catch any errors. */
    while ((tag = ReadTag(buf)) > D_MAX && tag) {
	switch (tag) {
	  case 't':
	    vdop->type = (VnodeType) ReadTag(buf);
	    break;
	  case 'b': 
	    short modeBits;
	    ReadShort(buf, (unsigned short *)&modeBits);
	    vdop->modeBits = modeBits;
	    break;
	  case 'l':
	    ReadShort(buf, (unsigned short *)&vdop->linkCount);
	    break;
	  case 'L':
	    ReadInt32(buf, (unsigned int *)&vdop->length);
	    break;
	  case 'v':
	    ReadInt32(buf, (unsigned int *)&vdop->dataVersion);
	    break;
	  case 'V':
	    ReadVV(buf, &vdop->versionvector);
	    break;
	  case 'm':
	    ReadInt32(buf, (unsigned int *)&vdop->unixModifyTime);
	    break;
	  case 'a':
	    ReadInt32(buf, (unsigned int *)&vdop->author);
	    break;
	  case 'o':
	    ReadInt32(buf, (unsigned int *)&vdop->owner);
	    break;
	  case 'p':
	    ReadInt32(buf, (unsigned int *)&vdop->vparent);
	    break;
	  case 'q':
	    ReadInt32(buf, (unsigned int *)&vdop->uparent);
	    break;
	  case 'A':
	    CODA_ASSERT(vdop->type == vDirectory);
	    ReadByteString(buf, (byte *)VVnodeDiskACL(vdop), VAclDiskSize(vdop));
	    break;
	}
    }

    if (!tag) {
	VLog(0, "ReadVnodeDiskObject: Readstuff failed, aborting.");
	return -1;
    }

    PutTag(tag, buf);
    
    if (vdop->type == vDirectory) {
	int npages = 0;
	CODA_ASSERT(ReadTag(buf) == D_DIRPAGES);
	if (!ReadInt32(buf, (unsigned int *)&npages) || 
	    (npages > DIR_MAXPAGES)) {
	    VLog(0, "Restore: Dir has to many pages for vnode %d", 
		 *vnodeNumber);
	    return -1;
	}
	*dinode = (DirInode *)malloc(sizeof(DirInode));
	bzero((void *)*dinode, sizeof(DirInode));
	for (int i = 0; i < npages; i++){
	    (*dinode)->di_pages[i] = (long *)malloc(DIR_PAGESIZE);
	    int tmp = ReadTag(buf);
	    if ((byte)tmp != 'P'){
		VLog(0, "Restore: Dir page does not have a P tag");
		return -1;
	    }
	    if (!ReadByteString(buf, (byte *)((*dinode)->di_pages[i]), DIR_PAGESIZE)) {
		VLog(0, "Restore: Failure reading dir page, aborting.");
		return -1;
	    }
		
	}
	vdop->inodeNumber = (Inode)(*dinode);
	(*dinode)->di_refcount = 1;
    } else {
	tag = ReadTag(buf);
	if (tag == D_FILEDATA) {
	    int fd;
	
	    vdop->inodeNumber = icreate((int)V_device(vp), 0, 
					(int)V_parentId(vp),
					(int)*vnodeNumber, 
					(int)vdop->uniquifier,
					(int)vdop->dataVersion);
	    if (vdop->inodeNumber < 0) {
		VLog(0,"Unable to allocate inode for vnode %#x: aborted",
		     *vnodeNumber);
		return -1;
	    }
	    fd = iopen((int)V_device(vp), (int)vdop->inodeNumber, O_WRONLY);
	    if (fd == -1){
		VLog(0, "Failure to open inode for writing %d", errno);
		return -1;
	    }

	    FILE *ifile = fdopen(fd, "w");
	    if (ifile == NULL) {
		VLog(0, "Failure to open inode file stream %d", errno);
		return -1;
	    }

	    vdop->length = ReadFile(buf, ifile);
	    if ((int)vdop->length == -1) {
		VLog(0, "Failure reading in data for vnode %d: aborted", 
		     *vnodeNumber);
		return -1;
	    }

	    fclose(ifile);
	    close(fd);
	} else if (tag == D_BADINODE) {
	    /* Create a null inode. */
	    vdop->inodeNumber = icreate((int)V_device(vp), 0, 
					(int)V_parentId(vp),
					(int)*vnodeNumber, 
					(int)vdop->uniquifier,
					(int)vdop->dataVersion);
	    if (vdop->inodeNumber < 0) {
		VLog(0,
		       "Unable to allocate inode for vnode %d: aborted",
		       *vnodeNumber);
		return -1;
	    }
	} else {
	    VLog(0, "Restore: FileData not properly tagged, aborting.");
	    return -1;
	}
    }
    return 0;
}

