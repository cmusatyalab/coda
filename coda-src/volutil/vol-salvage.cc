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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/volutil/vol-salvage.cc,v 4.23 1998/11/02 16:47:16 rvb Exp $";
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


/********************************
 * salvage.c			*
 * partition/volume salvager    *
 ********************************/

/* Problems (maybe?)

   1. Some ambiguity about which volumes are read/write and read-only.
   The assumption is that volume number == parent volume number ==>
   read-only.  This isn't always true.  The only problem this can
   cause is that a read-only volume might be salvaged like a
   read/write volume.

   2. Directories are examined, but not actually salvaged.  The
   directory salvage routine exists but the call is commented out, for
   now.

*/

#define SalvageVersion "3.0"


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>

#include <unistd.h>
#include <stdlib.h>

#include <stdio.h>

#include <lwp.h>
#include <lock.h>
#include <rpc2.h>
#include <util.h>
#include <partition.h>
#include <inodeops.h>
#include <rvmlib.h>
#include <codadir.h>

#include <vice.h>
#include <volutil.h>
#ifdef __cplusplus
}
#endif __cplusplus
#include <voltypes.h>
#include <errors.h>
#include <cvnode.h>
#include <volume.h>
#include <srvsignal.h>
#include <fssync.h>
#include <vutil.h>
#include <index.h>
#include <recov.h>
#include <camprivate.h>
#include <coda_globals.h>
#include <volhash.h>
#include <bitmap.h>
#include <recle.h>



#include "volutil.private.h"
#include "vol-salvage.private.h"


static int debug = 0;			/* -d flag */

static int ListInodeOption = 0;	/* -i flag */

static int ForceSalvage = 0;		/* If salvage should occur despite the
					   DONT_SALVAGE flag
					   in the volume header */


int nskipvols = 0;			/* volumes to be skipped during salvage */
VolumeId *skipvolnums = NULL;

int debarrenize = 0;			/* flag for debarrenizing vnodes on startup */

static Device fileSysDevice;		/* The device number of
					   partition being salvaged */
static char *fileSysDeviceName; 	/* The block device where the file system 
					   being salvaged was mounted */
static char    *fileSysPath;		/* The path of the mounted
					   partition currently being
					   salvaged, i.e. the
					   diqrectory containing the
					   volume headers */
int	VolumeChanged = 0;		/* Set by any routine which would change
					   the volume in a way which would require
					   callback to be broken if the volume was
					   put back on line by an active file server */
    
struct InodeSummary *inodeSummary;
static int nVolumesInInodeFile; 	/* Number of read-write volumes summarized */
static int inodeFd;			/* File descriptor for inode file */

struct VolumeSummary volumeSummary[MAXVOLS_PER_PARTITION];

static int nVolumes;			/* Number of volumes in volume summary */

static struct VnodeInfo  vnodeInfo[nVNODECLASSES];

/*
 *  S_VolSalvage is the RPC2 Volume Utility subsystem call used to salvage
 *  a particular volume. It is also called directly by the fileserver to
 *  perform full salvage when the fileserver starts up. The salvage is
 *  performed within a  transaction, although manipulations of inodes
 *  and inode data on the disk partition will naturally not be undone should
 *  the transaction abort.
 */

long S_VolSalvage(RPC2_Handle rpcid, RPC2_String path, 
		  VolumeId singleVolumeNumber,
		  RPC2_Integer force, RPC2_Integer Debug, 
		  RPC2_Integer list)
{
    long rc = 0;
    int UtilityOK = 0;	/* flag specifying whether the salvager may run as a volume utility */
    ProgramType *pt;  /* These are to keep C++ > 2.0 happy */

    VLog(9, 
	   "Entering S_VolSalvage (%d, %s, %x, %d, %d, %d)",
	   rpcid, path, singleVolumeNumber, force, Debug, list);
    CODA_ASSERT(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);

    zero_globals();

    ForceSalvage = force;
    debug = Debug;
    ListInodeOption = list;
    
    VLog(0, 
	   "Vice file system salvager, version %s.", 
	   SalvageVersion);

    /* Note:  if path and volume number are specified, we initialize this */
    /* as a standard volume utility: negotations have to be made with */
    /* the file server in this case to take the read write volume and */
    /* associated read-only volumes off line before salvaging */

    if (path != NULL && singleVolumeNumber != 0) 
	    UtilityOK = 1;

    rc = VInitVolUtil(UtilityOK? volumeUtility: salvager);
    if (rc != 0) {
	VLog(0, 
	       "S_VolSalvage: VInitVolUtil failed with %d.", rc);
	return(rc);
    }

    if (*pt == salvager) {
	    GetSkipVolumeNumbers();
	    SanityCheckFreeLists();
	    if (rc = DestroyBadVolumes())
		    return(rc);
    }
    
    if (path == NULL) {
	    struct DiskPartition *dp = DiskPartitionList;

	    do {
		    rc = SalvageFileSys(dp->name, 0);
		    if (rc != 0)
			    goto cleanup;
		    dp = dp->next;
	    } while ( dp ) ;
	    
    } else 
	    rc = SalvageFileSys(path, singleVolumeNumber);

    /* should put vol back on line if singleVolumeNumber */
    if (singleVolumeNumber)
	    AskOnline(singleVolumeNumber);

 cleanup:
    if (UtilityOK) {
	    VDisconnectFS();
    }
    release_locks(UtilityOK);
    zero_globals();	/* clean up state */
    VLog(9, "Leaving S_VolSalvage with rc = %d", rc);
    return(rc);
}

/*
 *  SalvageFileSys is the top level call for the main part of the salvage
 *  operation.  It proceeds as follows:
 *  1.  First it reads the locks and reads the disk partition to 
 *	obtain a list of all the inodes in that partition(GetInodeSummary).
 *	The inode list is sorted by volume number they belong to.
 *  2.  It then obtains the summary of all the volumes in the partition(GetVolumeSummary).
 *	This list is also sorted by volume number of the r/w volume,
 *	i.e. the same as the volume id if this is a r/w volume or
 *	the id of the parent if this is a backup or read-only clone.
 *  3.  Salvage each set of read-only volumes + r/w volume they correspond
 *	to(SalvageVolumeGroup), i.e. check that
 *	* each vnode has an inode (SalvageIndex), and 
 *	* a name exists in a directory for each vnode(SalvageVolume), and
 *	* a vnode exists for all names in each directory(SalvageVolume)
 */

static int SalvageFileSys(char *path, VolumeId singleVolumeNumber)
{
    struct stat status;
    struct ViceInodeInfo *ip = NULL;
    struct stat force;
    char inodeListPath[32];
    char forcepath[MAXNAMLEN];
    struct VolumeSummary *vsp;
    int i,rc, camstatus;

    VLog(9, 
	   "Entering SalvageFileSys (%s, %x)", path, singleVolumeNumber);

    if (stat(path,&status) == -1) {
	VLog(0, 
	       "Couldn't find file system \"%s\", aborting", path);
	return(VFAIL);
    }

    DP_LockPartition(path);

    /* house keeping to deal with FORCESALVAGE */
    if ( (strlen(path) + strlen("/FORCESALVAGE")) >= MAXPATHLEN ) {
	eprint("Fatal string operation detected.\n");
	CODA_ASSERT(0);
    } else {
	strcpy(forcepath, path);
	strcat(forcepath, "/FORCESALVAGE");
    }

    if (singleVolumeNumber || ForceSalvage)
	ForceSalvage = 1;
    else {
	if (stat(forcepath, &force) == 0)
	    ForceSalvage = 1;
    }

    if (singleVolumeNumber) {	/* not running in full salvage mode */
	if ((rc = AskOffline(singleVolumeNumber)) != 0)
	    return (rc);
    } else {
	VLog(0, 
	       "Salvaging file system partition %s", path);
	if (ForceSalvage)
	    VLog(0, 
		   "Force salvage of all volumes on this partition");
    }

    /* obtain a summary of all the inodes in the partition */
    fileSysDevice = status.st_dev;
    fileSysPath = path;
    strcpy(inodeListPath, "/tmp/salvage.inodes");
    rc = GetInodeSummary(path, inodeListPath, singleVolumeNumber);
    if (rc != 0) {
	if (rc == VNOVOL) {
	    return 0;
	}
	else {
	    VLog(9, 
		   "SalvageFileSys: GetInodeSummary failed with %d", rc);
	    return rc;
	}
    } else {
	VLog(9, "GetInodeSummary returns success");
    }

    /* open the summary file and unlink it for automatic cleanup */
    inodeFd = open(inodeListPath, O_RDONLY, 0);
    CODA_ASSERT(unlink(inodeListPath) != -1);
    if (inodeFd == -1) {
	VLog(0, "Temporary file %s is missing...",
		inodeListPath);
	return(VNOVNODE);
    }

    /* get volume summaries */
    if ((rc = GetVolumeSummary(singleVolumeNumber)) != 0) {
	VLog(0, 
	       "SalvageFileSys: GetVolumeSummary failed with %d", rc);
	return(rc);
    }
    if (nVolumes > nVolumesInInodeFile)
      VLog(0, 
	     "SFS: There are some volumes without any inodes in them");


    /* there we go: salvage it */
    RVMLIB_BEGIN_TRANSACTION(restore)

    for (i = 0, vsp = volumeSummary; i < nVolumesInInodeFile; i++){
	VolumeId rwvid = inodeSummary[i].RWvolumeId;
	while (nVolumes && (vsp->header.parent < rwvid)){
	    VLog(0,
		   "SFS:No Inode summary for volume 0x%x; skipping full salvage",  
		vsp->header.parent);
	    VLog(0, 
		   "SalvageFileSys: Therefore only resetting inUse flag");
	    ClearROInUseBit(vsp);
	    vsp->inSummary = NULL;
	    nVolumes--;
	    vsp++;
	}
	VLog(9, 
	       "SFS: nVolumes = %d, parent = 0x%x, id = 0x%x, rwvid = 0x%x", 
	       nVolumes, vsp->header.parent, vsp->header.id, rwvid);
	    
	if (nVolumes && vsp->header.parent == rwvid){
	    VLog(9, 
		   "SFS: Found a volume for Inodesummary %d", i);
	    VolumeSummary *startVsp = vsp;
	    int SalVolCnt = 0;
	    while (nVolumes && vsp->header.parent == rwvid){
		VLog(9, 
		       "SFS: Setting Volume 0x%x inodesummary to %d",
		    rwvid, i);
		vsp->inSummary = &(inodeSummary[i]);
		SalVolCnt++;
		vsp++;
		nVolumes--;
	    }
	    rc = SalvageVolumeGroup(startVsp, SalVolCnt);
	    if (rc) {
		VLog(9, "SalvageVolumeGroup failed with rc = %d, ABORTING", 
		     rc);
		rvmlib_abort(VFAIL);
		return VFAIL;
	    }
	    continue;
	} else {
	    VLog(0, "No Volume corresponding for inodes with vid 0x%lx", 
		 rwvid);
	    CleanInodes(&(inodeSummary[i]));
	}
    }
    while (nVolumes) {
	VLog(0, "SalvageFileSys:  unclaimed volume header file or no Inodes in volume %x",
	    vsp->header.id);
	VLog(0, "SalvageFileSys: Therefore only resetting inUse flag");
	ClearROInUseBit(vsp);
	nVolumes--;
	vsp++;
    }

    RVMLIB_END_TRANSACTION(flush, &(camstatus));
    if (camstatus){
	VLog(0, "SFS: aborting salvage with status %d", camstatus);
	return (camstatus);
    }

    if (ForceSalvage && !singleVolumeNumber) {
	if (stat(forcepath, &force) == 0)
	    unlink("forcepath");
    }
    VLog(0, "SalvageFileSys completed on %s", path);
    return (0);
}

static int SalvageVolumeGroup(register struct VolumeSummary *vsp, int nVols)
{
    struct ViceInodeInfo *inodes=0;
    int	size;
    int haveRWvolume = !(readOnly(vsp));
    VLog(9, "Entering SalvageVolumeGroup(%#08x, %d)", 
	 vsp->header.parent, nVols);
    VLog(9, "ForceSalvage = %d", ForceSalvage);

    /* if any of the volumes in this group are not to be salvaged
	then just return */
    if (skipvolnums != NULL){
	for (int i = 0; i < nVols; i++){
	    if (InSkipVolumeList(vsp[i].header.parent, skipvolnums, nskipvols)){
		VLog(9, "Volume %x is not to be salvaged",
		    vsp[i].header.parent);
		return 0;
	    }
	}
    }
    if (!ForceSalvage && QuickCheck(vsp, nVols)){
	VLog(9, "SVG: Leaving SVG with rc = 0");
	return 0;
    }

    /* get the list of inodes belonging to this 
       group of volumes from the inode file */
    struct InodeSummary *isp = vsp->inSummary;
    size = isp->nInodes * sizeof(struct ViceInodeInfo);
    inodes = (struct ViceInodeInfo *)malloc(size);
    CODA_ASSERT(inodes != 0);
    CODA_ASSERT(lseek(inodeFd, isp->index*
		 sizeof(struct ViceInodeInfo),L_SET) != -1);
    CODA_ASSERT(read(inodeFd,(char *)inodes,size) == size);
    
    for (int i = 0; i < nVols; i++){
	VLog(9, "SalvageVolumeGroup: Going to salvage Volume 0x%#08x header",
	    vsp[i].header.id);

	/* check volume head looks ok */
	if (SalvageVolHead(&(vsp[i])) == -1){
	    VLog(0, "SalvageVolumeGroup: Bad Volume 0x%#08x");
	    if (i == 0)
		haveRWvolume = 0;
	    continue;
	}
	VLog(9, "SVG: Going to salvage Volume 0x%#08x vnodes", vsp[i].header.id);

	/* make sure all small vnodes have a matching inode */
	if (VnodeInodeCheck(!(readOnly(&vsp[i])), inodes, 
			    vsp[i].inSummary->nInodes, &(vsp[i])) == -1) {
	    VLog(0, "SVG: Vnode/Inode correspondence not OK(0x%08x).... Aborting set", 
		vsp[i].header.parent);
	    if (inodes)	free(inodes);
	    return -1;
	}

	/* if debug mode check directory/vnode completeness */
	if (debug)
	    DirCompletenessCheck(&vsp[i]);
    }

    FixInodeLinkcount(inodes, isp);
    free((char *)inodes);
    
    VLog(9, "Leaving SalvageVolumeGroup(0x%#08x, %d)", 
	 vsp->header.parent, nVols);
    return 0;
}


/* Check to see if VolumeDiskData info looks ok */
static int QuickCheck(register struct VolumeSummary *vsp, register int nVols)
{
    register int i;
    Error ec;

    VLog(9, "Entering QuickCheck()");
    for (i = 0; i<nVols; i++) {
	VolumeDiskData volHeader;
	if (!vsp)
	    return 0;

	ExtractVolDiskInfo(&ec, vsp->volindex, &volHeader);

	if (ec == 0 && volHeader.dontSalvage == DONT_SALVAGE
	&& volHeader.needsSalvaged == 0 && volHeader.destroyMe == 0) {
	    if (volHeader.inUse == 1) {
		volHeader.inUse = 0;
		VLog(9, "Setting volHeader.inUse = %d for volume %x",
			volHeader.inUse, volHeader.id);
		ReplaceVolDiskInfo(&ec, vsp->volindex, &volHeader);
		if (ec != 0)
		    return 0;	// write back failed
	    }
	    else {
		VLog(9, "QuickCheck: inUse == %d", volHeader.inUse);
    }
	}
	else {
	    return 0;	// need to do a real salvage
	}
	vsp++;
    }
    return 1;	// ok to skip detailed salvage
}

static int SalvageVolHead(register struct VolumeSummary *vsp)
{
    Error ec = 0;
    VLog(9, "Entering SalvageVolHead(rw = %#08x, vid = %#08x)",
	 vsp->header.parent, vsp->header.id);
    if (readOnly(vsp)){
	ClearROInUseBit(vsp);
	VLog(9, "SalvageVolHead returning with rc = 0");
	return 0;
    }
    CheckVolData(&ec, vsp->volindex);
    if (ec) {
	VLog(0, "SalvageVolHead: bad VolumeData for volume %#08x",
	    vsp->header.id);
	VLog(0, "SalvageVolHead: returning with rc = -1");
	return -1;
    }
    VLog(9, "SalvageVolHead: returning with rc = 0");
    return 0;
}

/* Checks that every vnode has an inode that matches.
 * Special cases for vnodes with:
 * 	inodeNumber = NEWVNODEINODE 
 *		vnode was allocated but the server crashed; delete the vnode 
 * 	inodeNumber = 0 
 *		vnode was created but no store was done 
 * 		create an empty object for this vnode 
 */
static int VnodeInodeCheck(int RW, struct ViceInodeInfo *ip, int nInodes, 
			    struct VolumeSummary *vsp) {

    VLog(9, "Entering VnodeInodeCheck()");    
    VolumeId volumeNumber = vsp->header.id;
    char buf[SIZEOF_SMALLDISKVNODE];
    struct VnodeDiskObject *vnode = (struct VnodeDiskObject *)buf;
    int vnodeIndex = 0;
    int nVnodes = 0;
    vindex v_index(vsp->header.id, vSmall, fileSysDevice, SIZEOF_SMALLDISKVNODE);
    vindex_iterator vnext(v_index);
    int	foundinode;

    nVnodes = v_index.vnodes();
    for (vnodeIndex = 0; 
	 nVnodes && ((vnodeIndex = vnext(vnode)) != -1);
	 nVnodes--){
	
	int vnodeChanged = 0;
	int vnodeNumber = bitNumberToVnodeNumber(vnodeIndex, vSmall);

	/* take care of special cases first */
	{
	    if (vnode->inodeNumber == NEWVNODEINODE){
		vnode->type = vNull;
		CODA_ASSERT(v_index.put(vnodeNumber, vnode->uniquifier, vnode) == 0);
		VolumeChanged = 1;
		continue;
	    }
	    else if (vnode->inodeNumber == 0){
		VLog(0, "SalvageIndex:  Vnode 0x%x has no inodeNumber", 
		     vnodeNumber);
		CODA_ASSERT(RW);
		CODA_ASSERT(vnode->dataVersion == 0); // inodenumber == 0 only after create 
		VLog(0, "SalvageIndex: Creating an empty object for it");
		vnode->inodeNumber = icreate(fileSysDevice, 0,
					     vsp->header.id, vnodeNumber,
					     vnode->uniquifier, 0);
		CODA_ASSERT(vnode->inodeNumber > 0);
		if (vnode->cloned)
		    vnode->cloned = 0;		// invoke COW - XXX added 9/30/92 Puneet Kumar
		CODA_ASSERT(v_index.put(vnodeNumber, vnode->uniquifier, vnode) == 0);
		VolumeChanged = 1;
		continue;
	    }
	}

	/* find an inode with matching vnodeNumber */
	while (nInodes && ip->VnodeNumber < vnodeNumber){
	    ip++;
	    nInodes--;
	}
	foundinode = 0;
	register struct ViceInodeInfo *lip = ip;
	register int lnInodes = nInodes;
	/* skip over old inodes with same vnodeNumber */
	while (lnInodes && lip->VnodeNumber == vnodeNumber){
	    if (vnode->inodeNumber == lip->InodeNumber){
		foundinode = 1;
		break;
	    }
	    else {
		VLog(0, "VICheck: Found old inode %d for vnode number %d", 
		    lip->InodeNumber, vnodeNumber);	
		lip++;
		lnInodes--;
	    }
	}

	if (foundinode) {	/* lip points to matching inode summary*/
	    if (RW) { /* check uniquifier and data versions for RW volumes */
		Unique_t vu,iu;
		FileVersion vd,id;
		vu = vnode->uniquifier;
		iu = lip->VnodeUniquifier;
		vd = vnode->dataVersion;
		id = lip->InodeDataVersion;
		if ((vu != iu) ||(vd != id)) {
		    VLog(0, "SI: Vnode (0x%x.%x.%x) uniquifier(0x%x)/dataversion(%d) doesn't match with inode(0x%x/%d); marking BARREN",
			vsp->header.id, vnodeNumber, vnode->uniquifier, vu, vd, iu, id);
		    SetBarren(vnode->versionvector);
		    CODA_ASSERT(v_index.put(vnodeNumber,vnode->uniquifier,vnode) == 0);
		    VolumeChanged = 1;
		    continue;
		}
	    }
	    if (lip->ByteCount != vnode->length) {
		VLog(0, "Vnode (%x.%x.%x): length incorrect; can't happen!",
		    vsp->header.id, vnodeNumber, vnode->uniquifier);
		VLog(0, "Marking as BARREN ");
		SetBarren(vnode->versionvector);
		CODA_ASSERT(v_index.put(vnodeNumber, vnode->uniquifier, vnode) == 0);
		VolumeChanged = 1;
		continue;
	    }
	    /* everything is fine - update inode linkcount */
	    lip->LinkCount--;
	}
	else {
	    // didn't find an inode - 
	    // create a new inode for readonly volumes
	    // mark as BARREN for r/w volumes
	    if (readOnly(vsp)) {
		VLog(0, 
		       "Vnode 0x%x.%x in a ro volume has no inode - creating one\n",
		       vnodeNumber, vnode->uniquifier);
		vnode->inodeNumber = icreate(fileSysDevice, 0,
					     vsp->header.parent, vnodeNumber,
					     vnode->uniquifier, vnode->dataVersion);
		CODA_ASSERT(vnode->inodeNumber > 0);
		vnode->length = 0;
		CODA_ASSERT(v_index.put(vnodeNumber,vnode->uniquifier,vnode) == 0);
		VolumeChanged = 1;
	    }
	    else {
		if (!IsBarren(vnode->versionvector)){
		    if (!debarrenize) {
			VLog(0, 
			       "Vnode (%x.%x.%x) incorrect inode - marking as BARREN",
			       vsp->header.id, vnodeNumber, vnode->uniquifier);
			
			SetBarren(vnode->versionvector);
			
		    }
		    else {
			VLog(0, 
			       "Vnode 0x%x.%x.%x incorrect inode - Correcting\n",
			       vsp->header.id, vnodeNumber, vnode->uniquifier);
			vnode->inodeNumber = icreate(fileSysDevice, 0,
						     vsp->header.parent, vnodeNumber,
						     vnode->uniquifier, vnode->dataVersion);
			CODA_ASSERT(vnode->inodeNumber > 0);
			vnode->length = 0;
			extern ViceVersionVector NullVV;
			vnode->versionvector = NullVV;
		    }
		    CODA_ASSERT(v_index.put(vnodeNumber, vnode->uniquifier, vnode) == 0);
		    VolumeChanged = 1;
		}
		else {	// Barren vnode 
		    if (debarrenize) {
			VLog(0, 
			       "Vnode 0x%x.%x.%x is BARREN - Debarrenizing\n",
			       vsp->header.id, vnodeNumber, vnode->uniquifier);
			vnode->inodeNumber = icreate(fileSysDevice, 0,
						     vsp->header.parent, vnodeNumber,
						     vnode->uniquifier, vnode->dataVersion);
			CODA_ASSERT(vnode->inodeNumber > 0);
			vnode->length = 0;
			extern ViceVersionVector NullVV;
			vnode->versionvector = NullVV;
			CODA_ASSERT(v_index.put(vnodeNumber,vnode->uniquifier,vnode) == 0);
			VolumeChanged = 1;
		    }
		}
	    }
	    continue;
	}
    }

    CODA_ASSERT(vnext(vnode) == -1);
    CODA_ASSERT(nVnodes == 0);
    return 0;

}

/* inodes corresponding to a volume that has been blown away.
 * We need to idec them
 */
static void CleanInodes(struct InodeSummary *isp) 
{
    int size;
    struct ViceInodeInfo *inodes = 0;

    size = isp->nInodes * sizeof(struct ViceInodeInfo);
    inodes = (struct ViceInodeInfo *)malloc(size);
    CODA_ASSERT(inodes != 0);
    CODA_ASSERT(lseek(inodeFd, isp->index * sizeof(struct ViceInodeInfo), 
		  L_SET) != -1);
    CODA_ASSERT(read(inodeFd, (char *)inodes, size) == size);
    VLog(0, 
	   "Inodes found from destroyed volumes: scavenging.");    
    for(int i = 0; i < isp->nInodes; i++) {
	ViceInodeInfo *ip = &inodes[i];
	CODA_ASSERT(ip->LinkCount > 0);
	VLog(1, 
	       "Scavenging inode %u, size %u, p=(%lx,%lx,%lx,%lx)",
	       ip->InodeNumber, ip->ByteCount,
	       ip->VolumeNo, ip->VnodeNumber, ip->VnodeUniquifier, 
	       ip->InodeDataVersion);
	while(ip->LinkCount > 0) {
	    CODA_ASSERT(idec(fileSysDevice, ip->InodeNumber, 
			ip->VolumeNo) == 0);
	    ip->LinkCount--;
	}
    }
    free(inodes);

}


static struct VnodeEssence *CheckVnodeNumber(VnodeId vnodeNumber, Unique_t unq)
{
    VnodeClass vclass;
    struct VnodeInfo *vip;

    VLog(39,  "Entering CheckVnodeNumber(%d)", vnodeNumber);
    vclass = vnodeIdToClass(vnodeNumber);
    vip = &vnodeInfo[vclass];
    for(int i = 0; i < vip->nVnodes; i++){
	if ((vip->vnodes[i].vid == vnodeNumber) &&
	    (vip->vnodes[i].unique == unq))
	    return(&vip->vnodes[i]);
    }
    return(NULL);
}

/* iterate through entries in a directory and check them for */
/* validity. This routine is passed as a paramter to EnumerateDir 
   Checks performed:
   - is there a vnode for the entry with matching unique
   - if the entry is "." is the vnode number right
   - if the entry is ".." and the vnode has a parent 
     is the vnode number right
   - if the entry is ".." and the vnode has no parent 
     is the vnode number right
   - does the vnode of other entries point to the correct parent
   - finally reduce the linkcount in the vnodeEssence by one, indicating
     that a directory entry for the vnode has been found (eventually the 
     link count in the vnodeEssence should be 0).
*/

static int JudgeEntry(struct DirEntry *de, void *data)
{
	struct DirSummary *dir = (struct DirSummary *)data;
	char *name = de->name;
	VnodeId vnodeNumber;
	Unique_t unique;
	int rc = 0;
	struct VnodeEssence *vnodeEssence;

	FID_NFid2Int(&de->fid, &vnodeNumber, &unique);

	VLog(39, 
	       "Entering JudgeEntry(%s (%#x.%x.%x))", 
	       name, dir->Vid, vnodeNumber, unique);

	vnodeEssence = CheckVnodeNumber(vnodeNumber, unique);
	if (vnodeEssence == NULL || vnodeEssence->unique != unique) {
		VLog(0, 
	       "JE: directory vnode %#x.%x.%x: invalid entry %s; ",
	       dir->Vid, dir->vnodeNumber, dir->unique, name);
		VLog(0, 
	       "JE: child vnode not allocated or uniqfiers dont match; cannot happen");
		CODA_ASSERT(0);
	}
	if (strcmp(name,".") == 0) {
		if (dir->vnodeNumber != vnodeNumber || dir->unique != unique) {
			SLog(0, "JE:directory vnode %#x.%x.%x: bad '.' entry (was 0x%x.%x); ",
			     dir->Vid, dir->vnodeNumber, dir->unique, vnodeNumber, unique);
			VLog(0, "JE: Bad '.' - cannot happen ");
			CODA_ASSERT(0);
		}
		dir->haveDot = 1;
	}  else if (strcmp(name,"..") == 0) {
		ViceFid pa;
	
		if (dir->vparent) {
			struct VnodeEssence *dotdot;
			pa.Vnode = dir->vparent;
			dotdot = CheckVnodeNumber(pa.Vnode, dir->uparent);
			CODA_ASSERT(dotdot != NULL); /* XXX Should not be assert */
			pa.Unique = dotdot->unique;
			CODA_ASSERT(pa.Unique == dir->uparent);
		} else {
			pa.Vnode = dir->vnodeNumber;
			pa.Unique = dir->unique;
		}
		if (pa.Vnode != vnodeNumber || pa.Unique != unique) {
			SLog(0, "JE: directory vnode 0x%#08x.%x.%x: bad '..' entry (was 0x%x.%x); Shouldnt Happen ",
			     dir->Vid, dir->vnodeNumber, dir->unique, vnodeNumber, unique);
			CODA_ASSERT(0);
		}
		dir->haveDotDot = 1;
	} else {
		if ((vnodeEssence->vparent != dir->vnodeNumber)	||
		    (vnodeEssence->uparent != dir->unique)){
			SLog(0, "JE: parent = %#x.%x.%x ; child thinks parent is 0x%x.%x; Shouldnt Happen", 
			     dir->Vid, dir->vnodeNumber, dir->unique, 
			     vnodeEssence->vparent, vnodeEssence->uparent);
			CODA_ASSERT(0);
		}
		vnodeEssence->claimed = 1;
	}
    
	SLog(39, "JE: resetting copy of linkcount from %d to %d", 
	     vnodeEssence->count, vnodeEssence->count - 1);
	vnodeEssence->count--;
	VLog(39, "Leaving JudgeEntry(%s (0x%#08x.%x.%x))", 
	     name, dir->Vid, vnodeNumber, unique);

	return 0;

}

static void MarkLogEntries(rec_dlist *loglist, VolumeSummary *vsp) 
{
    VLog(9, "Entering MarkLogEntries....\n");
    if (!loglist) {
	VLog(0, "MarkLogEntries: loglist was NULL ... Not good\n");
	CODA_ASSERT(0);
    }
    CODA_ASSERT(vsp->logbm);
    rec_dlist_iterator next(*loglist);
    recle *r;
    while (r = (recle *)next()) {
	if (vsp->logbm->Value(r->index))  {
	    VLog(0, "MarkLogEntries: This index %d already set\n",
		   r->index);
	    r->print();
	    CODA_ASSERT(0);
	}
	else 
	    vsp->logbm->SetIndex(r->index);
	rec_dlist *childlist;
	if (childlist = r->HasList()) {
	    VLog(9, "MarkLogEntries: Looking recursively.....\n");
	    MarkLogEntries(childlist, vsp);
	}
    }
    VLog(9, "Leaving MarkLogEntries....\n");
}


static void DistilVnodeEssence(VnodeClass vclass, VolumeId volid) {

    register struct VnodeInfo *vip = &vnodeInfo[vclass];
    struct VnodeClassInfo *vcp = &VnodeClassInfo_Array[vclass];
    char buf[SIZEOF_LARGEDISKVNODE];
    struct VnodeDiskObject *vnode = (struct VnodeDiskObject *) buf;
    vindex v_index(volid, vclass, fileSysDevice, vcp->diskSize);
    vindex_iterator vnext(v_index);

    VLog(9, "Entering DistilVnodeEssence(%d, %u)", vclass, volid);

    vip->nVnodes = v_index.vnodes();

    if (vip->nVnodes > 0) {
	vip->vnodes = (struct VnodeEssence *)calloc(vip->nVnodes, sizeof(struct VnodeEssence));
	CODA_ASSERT(vip->vnodes != NULL);
	if (vclass == vLarge) {	    /* alloc space for directory entries */
	    vip->inodes = (Inode *) calloc(vip->nVnodes, sizeof (Inode));
	    CODA_ASSERT(vip->inodes != NULL);
	}
	else {
	    vip->inodes = NULL;
	}
    }
    else {  /* no vnodes of this class in this volume */
	vip->nVnodes = 0;
	vip->vnodes = NULL;
	vip->inodes = NULL;
    }
    vip->volumeBlockCount = vip->nAllocatedVnodes = 0;

    /* iterate through all vnodes in specified class index */
    /* note: empty slots are assumed to be zeroed by calloc */
    for (int v = 0, vnodeIndex = 0, nVnodes = vip->nVnodes;
      nVnodes && ((vnodeIndex = vnext(vnode)) != -1); nVnodes--,v++) {
	if (vnode->type != vNull) {
	    register struct VnodeEssence *vep = &vip->vnodes[v];
	    vip->nAllocatedVnodes++;
	    vep->count = vnode->linkCount;
	    vep->blockCount = nBlocks(vnode->length);
	    vip->volumeBlockCount += vep->blockCount;
	    vep->vparent = vnode->vparent;
	    vep->uparent = vnode->uparent;
	    vep->unique = vnode->uniquifier;
	    vep->vid = bitNumberToVnodeNumber(vnodeIndex, vclass);
	    if (vnode->type == vDirectory) {
		CODA_ASSERT(vclass == vLarge);
		/* for directory vnodes inode can never be zero */
		/* if the inode number is NEWVNODEINODE blow away vnode */
		CODA_ASSERT(vnode->inodeNumber != 0);
		if (vnode->inodeNumber == NEWVNODEINODE){
		    /* delete the vnode */
		    VLog(0, "DistilVnodeEssence: Found a Directory"
			 "vnode %d that has a special inode ... deleting vnode ",
			vnodeIndex);
		    vip->nAllocatedVnodes--;
		    vip->volumeBlockCount -= vep->blockCount;
		    bzero((void *)vep, sizeof(struct VnodeEssence));
		    vnode->type = vNull;
		    v_index.oput(vnodeIndex, vnode->uniquifier, vnode);
		}
		else 
		    vip->inodes[v] = vnode->inodeNumber;
		vep->log = vnode->log;
	    }
	}
    }
}


/* Check that all directory entries have a corresponding vnode,
 * and all vnodes have a directory entry pointing at them.
 * For now we are calling this inspite of directories being in rvm.
 * This is not necessary and in later versions should be eliminated
 */
void DirCompletenessCheck(struct VolumeSummary *vsp)
{
    int BlocksInVolume = 0, FilesInVolume = 0;
    int	i;
    int doassert = 0; 
    VolumeId vid;
    register VnodeClass vclass;
    VolumeDiskData volHeader;
    struct DirSummary dir;
    struct VnodeInfo *dirVnodeInfo;
    int RecoverableResLogs = (AllowResolution && vsp->vollog != NULL);
    Error ec = 0;

    vid = vsp->header.id;
    VLog(0, "Entering DCC(0x%x)", vsp->header.id);
    ExtractVolDiskInfo(&ec, vsp->volindex, &volHeader);
    if (ec != 0) {
	    VLog(0, "DCC: Error during ExtractVolDiskInfo(%#08x)", vid);
	    return;
    }
    
    CODA_ASSERT(volHeader.destroyMe != DESTROY_ME);
    DistilVnodeEssence(vLarge, vid);
    DistilVnodeEssence(vSmall, vid);

    if (RecoverableResLogs) 
	    vsp->logbm = new bitmap(vsp->vollog->bmsize(), 0);
    dir.Vid = vid;
    dirVnodeInfo = &vnodeInfo[vLarge];
    /* iterate through all directory vnodes in this volume */
    for (i = 0; i < dirVnodeInfo->nVnodes; i++) {
	    if (dirVnodeInfo->inodes[i] == 0)
		    continue;	/* Not allocated to any directory */
	    dir.vnodeNumber = dirVnodeInfo->vnodes[i].vid;
	    dir.unique = dirVnodeInfo->vnodes[i].unique;
	    dir.copied = 0;
	    dir.vparent = dirVnodeInfo->vnodes[i].vparent;
	    dir.uparent = dirVnodeInfo->vnodes[i].uparent;
	    dir.haveDot = dir.haveDotDot = 0;

	    dir.dirCache = DC_Get((PDirInode) dirVnodeInfo->inodes[i]);

	    /* dir inode is data inode of directory vnode */
	    VLog(9, "DCC: Going to check Directory (%#x.%x.%x)", vid, 
		 dir.vnodeNumber, dir.unique);

	    if (!DH_DirOK(DC_DC2DH(dir.dirCache))) {
		    VLog(0, "DCC: Bad Dir(0x%x.%x.%x) in rvm...Aborting", 
			 vsp->header.id, dir.vnodeNumber, dir.unique);
		    doassert = 1;
	    }

	    DH_EnumerateDir(DC_DC2DH(dir.dirCache), JudgeEntry, (void *)&dir);
	    /* XXX We could call DC_Put here equally well and 
	       retain some cache */
	    DC_Put(dir.dirCache);
	    VLog(9, "DCC: Finished checking directory(%#x.%x.%x)",
		   vsp->header.id, dir.vnodeNumber, dir.unique);

	    if (RecoverableResLogs) {
		    SLog(9, "DCC: Marking log entries for %#x.%x.%x\n",
			 vid, dirVnodeInfo->vnodes[i].vid, 
			 dirVnodeInfo->vnodes[i].unique);
		    MarkLogEntries(dirVnodeInfo->vnodes[i].log, vsp);
	    }
    }

    // salvage the resolution logs 
    if (RecoverableResLogs) {
	    SLog(0, "DCC: Salvaging Logs for volume 0x%x\n", vid);
	    vsp->vollog->SalvageLog(vsp->logbm);
	    delete vsp->logbm;
    }
    /* check link counts, parent pointers */
    for (vclass = 0; vclass < nVNODECLASSES; vclass++) {
	    int nVnodes = vnodeInfo[vclass].nVnodes;
	    struct VnodeClassInfo *vcp = &VnodeClassInfo_Array[vclass];
	    struct VnodeEssence *vnodes = vnodeInfo[vclass].vnodes;
	    FilesInVolume += vnodeInfo[vclass].nAllocatedVnodes;
	    BlocksInVolume += vnodeInfo[vclass].volumeBlockCount;
	    
	    for (i = 0; i<nVnodes; i++) {
		    register struct VnodeEssence *vnp = &vnodes[i];
		    VLog(29, "DCC: Check Link counts:");
		    VLog(29, "\t linkcount = %d, index = %d, parent = 0x%x, unique = 0x%x",
			 vnp->count, i, vnp->vparent, vnp->unique);
		    if (vnp->changed || vnp->count != 0) {
			    VLog(0, "DCC: For Vnode (%#x %#x %#x) parent (%#x %#x %#x): linkcount is %d bigger than count of directory entries.",
				 vid, vnp->vid, vnp->unique, vid, 
				 vnp->vparent, 
				 vnp->uparent, vnp->count);
			    doassert = 1;
			    /* You can bring up the server by forcing
			     * this volume off-line edit
			     * /vice/vol/skipsalvage.  The format of
			     * this file is the fist line has the
			     * number of volumes to skip, and each
			     * subsequent line has a volume number.
			     */
		    }
	    }
    }
    
    if  ( doassert ) {
	    VLog(0, "Salvage vol 0x%x: fatal error.", vid);
	    CODA_ASSERT(0);
    }
    /* clean up state */
    for (vclass = 0; vclass < nVNODECLASSES; vclass++) {
	register struct VnodeInfo *vip = &vnodeInfo[vclass];
	if (vip->vnodes)
		free((char *)vip->vnodes);
	if (vip->inodes)
		free((char *)vip->inodes);
    }
    
    /* Set correct resource utilization statistics */
    volHeader.filecount = FilesInVolume;
    volHeader.diskused = BlocksInVolume;
    
    /* Make sure the uniquifer is big enough */
    if (volHeader.uniquifier < (vsp->inSummary->maxUniquifier + 1)) {
	    VLog(0, "DCC: Warning - uniquifier is too low for volume (0x%x)", vid);
    }
    
    /* Turn off the inUse bit; the volume's been salvaged! */
    volHeader.inUse = 0;
    VLog(9, "DCC: setting volHeader.inUse = %d for volume 0x%#08x",
	   volHeader.inUse, volHeader.id);
    volHeader.needsSalvaged = 0;
    volHeader.needsCallback = (VolumeChanged != 0);    
    volHeader.dontSalvage = DONT_SALVAGE;
    VolumeChanged = 0;
    ReplaceVolDiskInfo(&ec, vsp->volindex, &volHeader);
    if (ec != 0){
	    VLog(0, "DCC: Couldnt write the volHeader for volume (%#08x)",
		   vsp->header.id);
	    return;    /* couldn't write out the volHeader */
    }
    VLog(0, "done:\t%d files/dirs,\t%d blocks", FilesInVolume, BlocksInVolume);
    VLog(9, "Leaving DCC()");
}

/* Zero inUse and needsSalvaged fields in VolumeDiskData */
static void ClearROInUseBit(struct VolumeSummary *summary)
{
    Error ec;
    VolumeId headerVid = summary->header.id;
    VolumeDiskData volHeader;

    VLog(9, "Entering ClearROInUseBit()");

    ExtractVolDiskInfo(&ec, summary->volindex, &volHeader);
    CODA_ASSERT(ec == 0);

    if (volHeader.destroyMe == DESTROY_ME)
	return;
    volHeader.inUse = 0;
    VLog(9, "ClearROInUseBit: setting volHeader.inUse = %d for volume %x",
		volHeader.inUse, volHeader.id);
    volHeader.needsSalvaged = 0;
    volHeader.dontSalvage = DONT_SALVAGE;

    ReplaceVolDiskInfo(&ec, summary->volindex, &volHeader);
    CODA_ASSERT(ec == 0);
}

/* "ask" the fileserver to take a volume offline */
static int AskOffline(VolumeId volumeId)
{
    ProgramType *pt, tmp;
    int rc = 0;

    VLog(9, "Entering AskOffline(%x)", volumeId);

    /* Note:  we're depending upon file server to put the volumes online
       after salvaging */
    /* masquerade as fileserver for FSYNC_askfs call */
    CODA_ASSERT(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);
    tmp = *pt;
    *pt = fileServer;
    rc = FSYNC_askfs(volumeId, FSYNC_OFF, FSYNC_SALVAGE);
    *pt = tmp;
    if (rc == FSYNC_DENIED) {
	VLog(0, "AskOffline:  file server denied offline request; a general salvage is required.");
        VLog(0, "Salvage aborted");
	return(VNOVOL);
    }
    return (0);
}

/* "ask" the fileserver to put a volume back online */
static int AskOnline(VolumeId volumeId)
{
    ProgramType *pt, tmp;
    int rc = 0;

    VLog(9, "Entering AskOnline(%x)", volumeId);

    /* Note:  we're depending upon file server to put the volumes online
       after salvaging */
    /* masquerade as fileserver for FSYNC_askfs call */
    CODA_ASSERT(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);
    tmp = *pt;
    *pt = fileServer;
    rc = FSYNC_askfs(volumeId, FSYNC_ON, 0);
    *pt = tmp;
    if (rc == FSYNC_DENIED) {
	VLog(0, "AskOnline:  file server denied online request; a general salvage is required.");
    }
    return (0);
}

/* copy data from one inode to another */
static int CopyInode(Device device, Inode inode1, Inode inode2)
{
    char buf[4096];
    register int fd1, fd2, n;

    VLog(9, "Entering CopyInode()");

    fd1 = iopen(device, inode1, O_RDONLY);
    fd2 = iopen(device, inode2, O_WRONLY);
    CODA_ASSERT(fd1 != -1);
    CODA_ASSERT(fd2 != -1);
    while ((n = read(fd1, buf, sizeof(buf))) > 0)
	CODA_ASSERT(write(fd2, buf, n) == n);
    CODA_ASSERT (n == 0);
    close(fd1);
    close(fd2);
    return 0;
}

/* Prints out a list of all inodes into the Log */
static void PrintInodeList() {
    register struct ViceInodeInfo *ip;
    struct ViceInodeInfo *buf;
    struct stat status;
    register nInodes;

    VLog(9, "Entering PrintInodeList()");

    CODA_ASSERT(fstat(inodeFd, &status) == 0);
    buf = (struct ViceInodeInfo *) malloc(status.st_size);
    CODA_ASSERT(buf != NULL);
    nInodes = status.st_size / sizeof(struct ViceInodeInfo);
    CODA_ASSERT(read(inodeFd, (char *)buf, status.st_size) == status.st_size);
    for(ip = buf; nInodes--; ip++) {
	VLog(0, 
	       "Inode:%u, linkCount=%d, size=%u, p=(%lx,%lx,%lx,%lx)",
	       ip->InodeNumber, ip->LinkCount, ip->ByteCount,
	       ip->VolumeNo, ip->VnodeNumber, ip->VnodeUniquifier, 
	       ip->InodeDataVersion);
    }
    free((char *)buf);
}

/* release file server and volume utility locks (for full salvage only) */
static void release_locks(int volUtil) {
    int fslock;

    if (volUtil) {  /* not running full salvage */
	return;
    }
    fslock = open("/vice/vol/fs.lock", O_CREAT|O_RDWR, 0666);
    CODA_ASSERT(fslock >= 0);
    if (flock(fslock, LOCK_UN) != 0) {
	VLog(0, "release_locks: unable to release file server lock");
    }
    else {
	VLog(9, "release_locks: released file server lock");
    }
    close(fslock);

    fslock = open ("/vice/vol/volutil.lock", O_CREAT|O_RDWR, 0666);
    CODA_ASSERT(fslock >= 0);
    if (flock(fslock, LOCK_UN) != 0) {
	VLog(0, "release_locks: unable to release volume utility lock");
    }
    else {
	VLog(9, "release_locks: released volume utility lock");
    }
    close(fslock);
}

/* if some volumes shouldnt be salvaged their number is 
   placed in a file DONTSALVVOLS.  Check if file exists
   and read in the volume numbers.  The format of the 
   file is "<number of volumes> \n <volume numbers in hex>"
*/
static void GetSkipVolumeNumbers() {
    struct stat s1;
    FILE *skipsalv;

    if (stat(DONTSALVVOLS, &s1) == 0){
	/* file exists */
	skipsalv = fopen(DONTSALVVOLS, "r");
	CODA_ASSERT(skipsalv != NULL);
	fscanf(skipsalv, "%d\n", &nskipvols);
	skipvolnums = (VolumeId *)malloc(nskipvols * sizeof(VolumeId));
    { /* drop scope for int i below; to avoid identifier clash */
	for (int i = 0; i < nskipvols; i++)
	    fscanf(skipsalv, "%x\n", &(skipvolnums[i]));
    } /* drop scope for int i above; to avoid identifier clash */

	fclose(skipsalv);
	VLog(1, "The Volume numbers to be skipped salvaging are :");
	for (int i = 0; i < nskipvols; i++){
	    VLog(1, "Volume %x", skipvolnums[i]);
	}
    }
}

/* check if a volume must be skipped during salvage */    
int InSkipVolumeList(VolumeId v, VolumeId *vl, int nvols)
{
    if (vl){
	for (int i = 0; i < nvols; i++){
	    if (vl[i] == v)
		return 1;
	}
    }
    return 0;
}

/* Check to make sure the Vnode Free lists aren't corrupt. */
static void SanityCheckFreeLists() {
    int i,j;
    char zerobuf[SIZEOF_LARGEDISKVNODE];
    struct VnodeDiskObject *zerovn = (struct VnodeDiskObject *) zerobuf;
    bzero((void *)zerovn, SIZEOF_LARGEDISKVNODE);
    
    VLog(0, "SanityCheckFreeLists: Checking RVM Vnode Free lists.");
    for (i = 0; i < SRV_RVM(SmallVnodeIndex); i++) {
	if (bcmp((const void *)SRV_RVM(SmallVnodeFreeList[i]), (const void *) zerovn,
		 SIZEOF_SMALLDISKVNODE) != 0) {
	    VLog(0,"Small Free Vnode at index %d not zero!", i);
	    CODA_ASSERT(0);
	}
	
	for (j = i + 1; j < SRV_RVM(SmallVnodeIndex); j++)
	    if (SRV_RVM(SmallVnodeFreeList[i]) ==
		SRV_RVM(SmallVnodeFreeList[j])) {
		VLog(0, "Vdo 0x%x appears twice (%d and %d) in smallfreelist!",
		       SRV_RVM(SmallVnodeFreeList[i]), i, j);
		CODA_ASSERT(0);
	    }
    }
    
    for (i = 0; i < SRV_RVM(LargeVnodeIndex); i++) {
	if (bcmp((const void *)SRV_RVM(LargeVnodeFreeList[i]), (const void *) zerovn,
		 SIZEOF_LARGEDISKVNODE) != 0) {
	    VLog(0, "Large Free Vnode at index %d not zero!", i);
	    CODA_ASSERT(0);
	}
	
	for (j = i + 1; j < SRV_RVM(LargeVnodeIndex); j++)
	    if (SRV_RVM(LargeVnodeFreeList[i]) ==
		SRV_RVM(LargeVnodeFreeList[j])) {
		VLog(0, "Vdo 0x%x appears twice (%d and %d) in largefreelist!",
		       SRV_RVM(LargeVnodeFreeList[i]), i, j);
		CODA_ASSERT(0);
	    }		    
    }
}

	
/* iterate through the VolumeList in RVM, destroying any volumes which
 * have the destroyMe flag set in VolDiskData. Current opinion feels that
 * the volume shouldn't be destroyed if it's in the skipsalvage file.
 */
static int DestroyBadVolumes() {
    VLog(0, "DestroyBadVolumes: Checking for destroyed volumes.");
    for (int i = 0; i <= MAXVOLS; i++) {
	struct VolumeHeader header;

	if (VolHeaderByIndex(i, &header) == -1) {
	    break;	/* index has exceeded maxvolid */
	}
	
	/* eliminate bogus cases */
	if (header.stamp.magic != VOLUMEHEADERMAGIC)
	    continue; 	/* corresponds to purged volume */
	
	if (SRV_RVM(VolumeList[i].data.volumeInfo)->destroyMe==DESTROY_ME){
	    VLog(0, "Salvage: Removing destroyed volume %x", header.id);
	    
	    /* Need to get device */
	    struct stat status;
	    char *part = SRV_RVM(VolumeList[i].data.volumeInfo)->partition;
	    if (stat(part, &status) == -1) {
		VLog(0, "Couldn't find partition \"%s\" for destroy", part);
		return(VFAIL);
	    }
	    /* Remove the volume */
	    CODA_ASSERT(DeleteRvmVolume(i, status.st_dev) == 0);
	}
    }
    return(0);
}

static void FixInodeLinkcount(struct ViceInodeInfo *inodes, 
			       struct InodeSummary *isp) {
    struct ViceInodeInfo *ip;
    int totalInodes = isp->nInodes;
    for (ip = inodes; totalInodes; ip++,totalInodes--) {
	static TraceBadLinkCounts = 25;
	if (ip->LinkCount != 0 && TraceBadLinkCounts) {
	    TraceBadLinkCounts--; 
	    VLog(0, 
		   "Link count incorrect by %d; inode %u, size %u, p=(%lx,%lx,%lx,%lx)",
		   ip->LinkCount, ip->InodeNumber, ip->ByteCount,
		   ip->VolumeNo, ip->VnodeNumber, ip->VnodeUniquifier, 
		   ip->InodeDataVersion);
	}

	/* Delete any links that are still unaccounted for */
	while (ip->LinkCount > 0) {
	   CODA_ASSERT(idec(fileSysDevice,ip->InodeNumber, ip->VolumeNo) == 0);
	   ip->LinkCount--;
	}
	while (ip->LinkCount < 0) {
	   CODA_ASSERT(iinc(fileSysDevice,ip->InodeNumber, ip->VolumeNo) == 0);
	   ip->LinkCount++;
	}
    }
}
/* zero out global variables */
static void zero_globals() 
{
    debug = 0;
    ListInodeOption = 0;
    ForceSalvage = 0;
    VolumeChanged = 0;
    nVolumes = 0;
    nVolumesInInodeFile = 0;	
    inodeFd = -1;

    if (inodeSummary) {
	free(inodeSummary);
        inodeSummary = NULL;
    }
    bzero((void *)volumeSummary,
	sizeof(struct VolumeSummary) * MAXVOLS_PER_PARTITION);
}

/* routines that get inode and volume summaries */


static void CountVolumeInodes(register struct ViceInodeInfo *ip,
		int maxInodes, register struct InodeSummary *summary)
{
    int volume = ip->VolumeNo;
    int rwvolume = volume;
    register n, nSpecial;
    register Unique_t maxunique;

    VLog(9, "Entering CountVolumeInodes()");

    n = nSpecial = 0;
    maxunique = 0;
    while (maxInodes-- && volume == ip->VolumeNo) {
	n++;
	if (ip->VnodeNumber == INODESPECIAL) {
	    VLog(0, "CountVolumeInodes: Bogus specialinode; can't happen");
	    CODA_ASSERT(0);
	}
	else {
	    if (maxunique < ip->VnodeUniquifier)
		maxunique = ip->VnodeUniquifier;
	}
	ip++;
    }
    summary->volumeId = volume;
    summary->RWvolumeId = rwvolume;
    summary->nInodes = n;
    summary->maxUniquifier = maxunique;
}

int OnlyOneVolume(struct ViceInodeInfo *inodeinfo,
			VolumeId singleVolumeNumber)
{

    if (inodeinfo->VnodeNumber == INODESPECIAL) {
	VLog(0, "OnlyOneVolume: tripped over INODESPECIAL- can't happen!");
	CODA_ASSERT(FALSE);
    }
    return (inodeinfo->VolumeNo == singleVolumeNumber);
}

/* Comparison routine for inode sort. Inodes are sorted */
/* by volume and vnode number */
static int CompareInodes(struct ViceInodeInfo *p1, struct ViceInodeInfo *p2)
{
    int i;
    if ( (p1->VnodeNumber == INODESPECIAL) ||
	 (p2->VnodeNumber == INODESPECIAL)) {
	VLog(0, "CompareInodes: found special inode! Aborting");
	CODA_ASSERT(0);
	return -1;
    }
    if ( (p1->VolumeNo) < (p2->VolumeNo))
	return -1;
    if ( (p1->VolumeNo) > (p2->VolumeNo))
	return 1;
    if (p1->VnodeNumber < p2->VnodeNumber)
	return -1;
    if (p1->VnodeNumber > p2->VnodeNumber)
	return 1;
    /* The following tests are reversed, so that the most desirable
       of several similar inodes comes first */
    if (p1->VnodeUniquifier > p2->VnodeUniquifier)
	return -1;
    if (p1->VnodeUniquifier < p2->VnodeUniquifier)
	return 1;
    if (p1->InodeDataVersion > p2->InodeDataVersion)
	return -1;
    if (p1->InodeDataVersion < p2->InodeDataVersion)
	return 1;
    return 0;
}

/*  
 *  Gets an inode summary by examining the vice disk partition directly.
 *  The inodes are sorted with qsort by order of the volume and vnode,
 *  respectively, with which they are associated.
 */

static int GetInodeSummary(char *fspath, char *path, VolumeId singleVolumeNumber)
{
    struct stat status;
    int summaryFd;
    int rc = 0;
    int nInodes = 0;; 
    struct ViceInodeInfo *ip, *tmp_ip;
    struct InodeSummary summary;
    FILE *summaryFile;
    char *dev = fileSysDeviceName;
    struct DiskPartition *dp = DP_Get(fspath);

    if ( dp == NULL ) {
	VLog(0, 
	       "Cannot find partition %s\n",
	       path);
	return VFAIL;
    }

    VLog(9, 
	   "Entering GetInodeSummary (%s, %x)", path, singleVolumeNumber);

    if(singleVolumeNumber)
	rc = dp->ops->ListCodaInodes(dp, path, OnlyOneVolume, 
				     singleVolumeNumber);
    else
	rc = dp->ops->ListCodaInodes(dp, path, NULL, 
				     singleVolumeNumber);

    if (rc == 0) {
	VLog(9, 
	       "ListViceInodes returns success");
    }


    if(rc == -1) {
	VLog(0, "Unable to get inodes for \"%s\"; not salvaged", dev);
	return(VFAIL);
    }
    inodeFd = open(path, O_RDWR, 0);
    if (inodeFd == -1 || fstat(inodeFd, &status) == -1) {
	VLog(0, "No inode description file for \"%s\"; not salvaged", dev);
	return(VFAIL);
    }
    summaryFd = open("/tmp/salvage.temp", O_RDWR|O_CREAT|O_TRUNC, 0600);
    if (summaryFd == -1) {
	VLog(0, "GetInodeSummary: Unable to create inode summary file");
	return(VFAIL);
    }

    summaryFile = fdopen(summaryFd,"w");
    CODA_ASSERT(summaryFile != NULL);
    nInodes = status.st_size / sizeof(ViceInodeInfo);

    ip = tmp_ip = (struct ViceInodeInfo *) malloc(status.st_size);
    if (ip == NULL) {
	VLog(0, "Unable to allocate enough space to read inode table; %s not salvaged", dev);
	return(VFAIL);
    }
    if (read(inodeFd, (char *)ip, status.st_size) != status.st_size) {
	VLog(0, "Unable to read inode table; %s not salvaged", dev);
	return(VFAIL);
    }
    VLog(9, "entering qsort(0x%x, %d, %d, 0x%x)", ip, nInodes,
		    sizeof(struct ViceInodeInfo), CompareInodes);
    qsort((void *)ip, nInodes, sizeof(struct ViceInodeInfo), 
	  (int (*)(const void *, const void *))CompareInodes);
    VLog(9, "returned from qsort");
    if (lseek(inodeFd,0,L_SET) == -1 ||
	write(inodeFd, (char *)ip, status.st_size) != status.st_size) {
	VLog(0, "Unable to rewrite inode table; %s not salvaged", dev);
	return(VFAIL);
    }
    summary.index = 0;	    /* beginning index for each volume */
    while (nInodes) {
	CountVolumeInodes(ip, nInodes, &summary);
	VLog(9, "returned from CountVolumeInodes");
	if (fwrite((char *)&summary, sizeof (summary), 1, summaryFile) != 1) {
	    VLog(0, "Difficulty writing summary file; %s not salvaged",dev);
	    return(VNOVNODE);
	}
	summary.index += (summary.nInodes);
	nInodes -= summary.nInodes;
	ip += summary.nInodes;
    }
    fflush(summaryFile);	/* Not fclose, because debug would not work */

    CODA_ASSERT(fstat(summaryFd,&status) != -1);
    /* store inode info in global array */
    VLog(9, "about to malloc %d bytes for inodeSummary", status.st_size);
    inodeSummary = (struct InodeSummary *) malloc(status.st_size);
    CODA_ASSERT(inodeSummary != NULL);
    CODA_ASSERT(lseek(summaryFd, 0, L_SET) != -1);
    CODA_ASSERT(read(summaryFd, (char *)inodeSummary, status.st_size) == status.st_size);
    nVolumesInInodeFile = status.st_size / sizeof (struct InodeSummary);
    close(summaryFd);
    close(inodeFd);
    unlink("/tmp/salvage.temp");
    free(tmp_ip);
    VLog(9, "Leaving GetInodeSummary()");
    return (0);
}

/* Comparison routine for volume sort. This is setup so */
/* that a read-write volume comes immediately before */
/* any read-only clones of that volume */
static int CompareVolumes(register struct VolumeSummary *p1,
				register struct VolumeSummary *p2)
{

    VLog(9, "Entering CompareVolumes()");

    if (p1->header.parent != p2->header.parent)
	return p1->header.parent < p2->header.parent? -1: 1;
    if (p1->header.id == p1->header.parent) /* p1 is rw volume */
	return -1;
    if (p2->header.id == p2->header.parent) /* p2 is rw volume */
	return 1;
    return p1->header.id < p2->header.id ? -1: 1; /* Both read-only */
}


/*  this is inefficiently structured. it has to iterate through all
 *  volumes once for each partition since there is only one global
 *  volume list. maybe reorganize volumes into per-partition lists?
 *  Gets a volume summary list based on the contents of recoverable
 *  storage.
 */
static int GetVolumeSummary(VolumeId singleVolumeNumber) {

    DIR *dirp;
    Error ec;
    int rc = 0;
    int i;

    VLog(9, 
	   "Entering GetVolumeSummary(%x)", singleVolumeNumber);

    /* Make sure the disk partition is readable */
    if ( access(fileSysPath, R_OK) != 0  ) {
	VLog(0, 
	       "Can't read directory %s; not salvaged", fileSysPath);
	return(VNOVNODE);
    }

    /* iterate through all the volumes on this partition, and try to */
    /* match with the desired volumeid */
    VLog(39, 
	   "GetVolSummary: filesyspath = %s nVolumes = %d", 
	   fileSysPath, nVolumes);
    for (i = 0; i <= MAXVOLS; i++) {
	char thispartition[64];
	struct VolumeSummary *vsp = &volumeSummary[nVolumes];
    	if (VolHeaderByIndex(i, &vsp->header) == -1) {
	    break;	/* index has exceeded maxvolid */
	}

	/* eliminate bogus cases */
	if (vsp->header.stamp.magic != VOLUMEHEADERMAGIC)
	    continue; 	/* corresponds to purged volume */

	/* Sanity checks */
	CODA_ASSERT(vsp->header.id == SRV_RVM(VolumeList[i]).data.volumeInfo->id);
	CODA_ASSERT(vsp->header.parent ==
	       SRV_RVM(VolumeList[i]).data.volumeInfo->parentId);

	for (int j = i+1; j < MAXVOLS; j++) 
	    if (vsp->header.id == SRV_RVM(VolumeList[j]).header.id) CODA_ASSERT(0);
	
	/* reject volumes from other partitions */
	VLog(39, 
	       "Partition for vol-index %d, (id 0x%x) is (%s)",
	       i, vsp->header.id, 
	       (SRV_RVM(VolumeList[i]).data.volumeInfo)->partition);
	GetVolPartition(&ec, vsp->header.id, i, thispartition);
	VLog(39, "GetVolSummary: For Volume id 0x%x GetVolPartition returns %s",
	    vsp->header.id, thispartition);
	if ((ec != 0) || (strcmp(thispartition, fileSysPath) != 0))
	    continue;	    /* bogus volume */

	/* Make sure volume is in the volid hashtable so SalvageInodes works */
	VLog(9, "GetVolSummary: inserting volume %x into hashtable for index %d",
	    vsp->header.id, i);
	HashInsert(vsp->header.id, i);

	// prepare for checking resolution logs
	vsp->logbm = NULL;
	if ((SRV_RVM(VolumeList[i]).data.volumeInfo->ResOn & RVMRES) && AllowResolution &&
	    (vsp->header.type == readwriteVolume))
	    vsp->vollog = SRV_RVM(VolumeList[i]).data.volumeInfo->log;
	else 
	    vsp->vollog = NULL;
	
	/* Is this the specific volume we're looking for? */
	if (singleVolumeNumber && vsp->header.id == singleVolumeNumber) {
	    if (vsp->header.type == readonlyVolume) {	// skip readonly volumes
		VLog(0, "%d is a read-only volume; not salvaged", singleVolumeNumber);
		return(VREADONLY);
	    }

	    if (vsp->header.type == readwriteVolume) {	// simple or replicated readwrite
		vsp->volindex = i;			// set the volume index
		rc = AskOffline(vsp->header.id);	// have the fileserver take it offline
		if (rc) {
		    VLog(0, "GetVolumeSummary: AskOffline failed, returning");
		    return(rc);				/* we can't have it; return */
		}
		nVolumes = 1;		// in case we need this set
	    }
	}
	else {	    /* take all volumes for full salvage; increment volume count */
	    if (!singleVolumeNumber) {
		vsp->volindex = i;			// set the volume index
		if (nVolumes++ > MAXVOLS_PER_PARTITION) {
		    VLog(0, 
			"More than %d volumes in partition %s; partition not salvaged\n", 
			MAXVOLS_PER_PARTITION);
		    return (VFAIL);
		}
	    }
	}
    }

    VLog(9, "GetVolumeSummary: entering qsort for %d volumes", nVolumes);
    qsort((char *)volumeSummary, nVolumes, sizeof(struct VolumeSummary),
	  (int (*)(const void *, const void *)) CompareVolumes);
    return (0);
}
