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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/volutil/vol-salvage.cc,v 4.1 1997/01/08 21:52:34 rvb Exp $";
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
   1. Some ambiguity about which volumes are read/write and read-only.  The assumption
      is that volume number == parent volume number ==> read-only.  This isn't always true.
      The only problem this can cause is that a read-only volume might be salvaged like
      a read/write volume.
   2. Directories are examined, but not actually salvaged.  The directory salvage routine
      exists but the call is commented out, for now.
*/

#define SalvageVersion "2.0"

#include <coda_dir.h>

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
#include <sys/dir.h>

#ifdef __MACH__
#include <libc.h>
#include <sysent.h>
#include <mach.h>
#endif /* __MACH__ */

#if defined(__linux__) || defined(__NetBSD__)
#include <unistd.h>
#include <stdlib.h>
#endif /* __NetBSD__ || LINUX */

/* If'defd includes below are highly platform-specific */

#ifdef __MACH__
#include <sys/inode.h>
#include <fstab.h>
#endif /* __MACH__ */

#ifdef	__linux__
#include <dirent.h>
#include <stdio.h>
#include <mntent.h>
#include <sys/vfs.h>
#include <linux/ext2_fs.h>
#endif

#ifdef	__NetBSD__
/* Not yet implemented */
#include <fstab.h>
#endif

#ifdef	__MACH__
#include <sys/inode.h>
#endif
#include <sysent.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/dir.h>

#include <lwp.h>
#include <lock.h>
#include <rpc2.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <vice.h>
#include <nfs.h>
#include <errors.h>
#include <cvnode.h>
#include <volume.h>
#include <partition.h>
#include <viceinode.h>
#include <srvsignal.h>
#include <fssync.h>
#include <vutil.h>
#include <index.h>
#include <recov.h>
#include <camprivate.h>
#include <coda_globals.h>
#include <volhash.h>
#include <util.h>
#include <rvmlib.h>
#include <bitmap.h>
#include <recle.h>



#include "volutil.private.h"
#include "vol-salvage.private.h"


static int debug = 0;			/* -d flag */

PRIVATE int ListInodeOption = 0;	/* -i flag */

PRIVATE int ForceSalvage = 0;		/* If salvage should occur despite the
					   DONT_SALVAGE flag
					   in the volume header */


int nskipvols = 0;			/* volumes to be skipped during salvage */
VolumeId *skipvolnums = NULL;

int debarrenize = 0;			/* flag for debarrenizing vnodes on startup */

PRIVATE Device fileSysDevice;		/* The device number of partition being salvaged */
PRIVATE char *fileSysDeviceName; 	/* The block device where the file system 
					   being salvaged was mounted */
PRIVATE char    *fileSysPath;		/* The path of the mounted partition currently
					   being salvaged, i.e. the directory
					   containing the volume headers */
int	VolumeChanged = 0;		/* Set by any routine which would change
					   the volume in a way which would require
					   callback to be broken if the volume was
					   put back on line by an active file server */
    
struct InodeSummary *inodeSummary;
PRIVATE int nVolumesInInodeFile; 	/* Number of read-write volumes summarized */
PRIVATE int inodeFd;			/* File descriptor for inode file */

struct VolumeSummary volumeSummary[MAXVOLS_PER_PARTITION];

PRIVATE int nVolumes;			/* Number of volumes in volume summary */

PRIVATE struct VnodeInfo  vnodeInfo[nVNODECLASSES];


/*
 *  S_VolSalvage is the RPC2 Volume Utility subsystem call used to salvage
 *  a particular volume. It is also called directly by the fileserver to
 *  perform full salvage when the fileserver starts up. The salvage is
 *  performed within a  transaction, although manipulations of inodes
 *  and inode data on the disk partition will naturally not be undone should
 *  the transaction abort.
 */

long S_VolSalvage(RPC2_Handle rpcid, RPC2_String formal_path, VolumeId singleVolumeNumber,
		  RPC2_Integer force, RPC2_Integer Debug, RPC2_Integer list)
{
    long rc = 0;
    int UtilityOK = 0;	/* flag specifying whether the salvager may run as a volume utility */
    ProgramType *pt;  /* These are to keep C++ > 2.0 happy */
    char *path = (char *) formal_path;

#ifdef __NetBSD__
      LogMsg(0, VolDebugLevel, stdout, "Arrrgh.... S_VolSalvage() not yet implemented for NetBSD!!");
      assert(0);
#endif /* __NetBSD__ */

    LogMsg(9, VolDebugLevel, stdout, "Entering S_VolSalvage (%d, %s, %x, %d, %d, %d)",
			rpcid, path, singleVolumeNumber, force, Debug, list);
    assert(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);

    zero_globals();

    ForceSalvage = force;
    debug = Debug;
    ListInodeOption = list;
    
    LogMsg(0, VolDebugLevel, stdout, "Vice file system salvager, version %s.", SalvageVersion);

    /* Note:  if path and volume number are specified, we initialize this */
    /* as a standard volume utility: negotations have to be made with */
    /* the file server in this case to take the read write volume and */
    /* associated read-only volumes off line before salvaging */

    if (path != NULL && singleVolumeNumber != 0) UtilityOK = 1;

    rc = VInitVolUtil(UtilityOK? volumeUtility: salvager);
    if (rc != 0) {
	LogMsg(0, VolDebugLevel, stdout, "S_VolSalvage: VInitVolUtil failed with %d.", rc);
	return(rc);
    }

    if (*pt == salvager) {
	GetSkipVolumeNumbers();
	SanityCheckFreeLists();
	if (rc = DestroyBadVolumes())
	    return(rc);
    }
    
    if (path == NULL) {
      int didSome = 0;
#ifdef __MACH__
      struct fstab *fsent;
      setfsent();
      while (fsent = getfsent()) {
	if (strncmp(fsent->fs_file, VICE_PARTITION_PREFIX, 
		    VICE_PREFIX_SIZE)== 0) {
	  rc = SalvageFileSys(fsent->fs_file, 0);
	  if (rc != 0)
	    goto cleanup;
	  didSome++;
	}
      }
#endif /* __MACH__ */

#ifdef	__linux__
      struct mntent *mntent;
      FILE *mnt_handle;
      mnt_handle = setmntent("/etc/mtab", "r");
      while ( mntent = getmntent(mnt_handle)) { 
	if (strncmp(mntent->mnt_dir, VICE_PARTITION_PREFIX, 
		    VICE_PREFIX_SIZE)== 0) {
	  rc = SalvageFileSys(mntent->mnt_dir, 0);
	  if (rc != 0)
	    goto cleanup;
	  didSome++;
	}
      }
#endif /* LINUX */



      if (!didSome) {
	LogMsg(0, VolDebugLevel, stdout, 
	       "No partitions named %s* found; not salvaged",
	       VICE_PARTITION_PREFIX);
	goto cleanup;
      }
    }
    else rc = SalvageFileSys(path, singleVolumeNumber);

    /* should put vol back on line if singleVolumeNumber */
    if (singleVolumeNumber)
	AskOnline(singleVolumeNumber);

cleanup:
    if (UtilityOK) {
	VDisconnectFS();
    }
    release_locks(UtilityOK);
    zero_globals();	/* clean up state */
    LogMsg(9, VolDebugLevel, stdout, "Leaving S_VolSalvage with rc = %d", rc);
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

PRIVATE int SalvageFileSys(char *path, VolumeId singleVolumeNumber)
{
    struct stat status;
    struct ViceInodeInfo *ip = NULL;
    char *name;
    char inodeListPath[32];
    struct VolumeSummary *vsp;
    int i,rc, camstatus;

    LogMsg(9, VolDebugLevel, stdout, "Entering SalvageFileSys (%s, %x)", path, singleVolumeNumber);

    if (stat(path,&status) == -1) {
	LogMsg(0, VolDebugLevel, stdout, "Couldn't find file system \"%s\", aborting", path);
	return(VFAIL);
    }
    if (status.st_ino != ROOTINODE || (name = devName(status.st_dev)) == NULL) {
	LogMsg(0, VolDebugLevel, stdout, "\"%s\" is not a mounted file system", path);
	return(VNOVNODE);
    }
    VLockPartition(path);
    if (singleVolumeNumber || ForceSalvage)
	ForceSalvage = 1;
    else {
	struct stat force;
	assert(chdir(path) != -1);
	if (stat("FORCESALVAGE", &force) == 0)
	    ForceSalvage = 1;
    }
    if (singleVolumeNumber) {	// not running in full salvage mode
	if ((rc = AskOffline(singleVolumeNumber)) != 0)
	    return (rc);
    }
    else {
	LogMsg(0, VolDebugLevel, stdout, "Salvaging file system partition %s (device=%s)", path, name);
	if (ForceSalvage)
	    LogMsg(0, VolDebugLevel, stdout, "Force salvage of all volumes on this partition");
    }

    /* obtain a summary of all the inodes in the partition */
    fileSysDevice = status.st_dev;
    fileSysPath = path;
    fileSysDeviceName = name;
    strcpy(inodeListPath, "/tmp/salvage.inodes.");
    strcat(inodeListPath, name); 
    rc = GetInodeSummary(inodeListPath, singleVolumeNumber);
    if (rc != 0) {
	if (rc == VNOVOL) {
	    return 0;
	}
	else {
	    LogMsg(9, VolDebugLevel, stdout, "SalvageFileSys: GetInodeSummary failed with %d", rc);
	    return rc;
	}
    }
    else {
	LogMsg(9, VolDebugLevel, stdout, "GetInodeSummary returns success");
    }
    inodeFd = open(inodeListPath, O_RDONLY, 0);
    assert(unlink(inodeListPath) != -1);
    if (inodeFd == -1) {
	LogMsg(0, VolDebugLevel, stdout, "Temporary file %s is missing...",
		inodeListPath);
	return(VNOVNODE);
    }

    if ((rc = GetVolumeSummary(singleVolumeNumber)) != 0) {
	LogMsg(0, VolDebugLevel, stdout, "SalvageFileSys: GetVolumeSummary failed with %d", rc);
	return(rc);
    }
    if (nVolumes > nVolumesInInodeFile)
      LogMsg(0, VolDebugLevel, stdout, "SFS: There are some volumes without any inodes in them");

    CAMLIB_BEGIN_TOP_LEVEL_TRANSACTION_2(CAM_TRAN_NV_SERVER_BASED)

    for (i = 0, vsp = volumeSummary; i < nVolumesInInodeFile; i++){
	VolumeId rwvid = inodeSummary[i].RWvolumeId;
	while (nVolumes && (vsp->header.parent < rwvid)){
	    LogMsg(0, VolDebugLevel, stdout,"SFS:No Inode summary for volume %#08x; skipping full salvage",  
		vsp->header.parent);
	    LogMsg(0, VolDebugLevel, stdout, "SalvageFileSys: Therefore only resetting inUse flag");
	    ClearROInUseBit(vsp);
	    vsp->inSummary = NULL;
	    nVolumes--;
	    vsp++;
	}
	LogMsg(9, VolDebugLevel, stdout, "SFS: nVolumes = %d, parent = 0x%x, id = 0x%x, rwvid = 0x%x", 
	    nVolumes, vsp->header.parent, vsp->header.id, rwvid);
	    
	if (nVolumes && vsp->header.parent == rwvid){
	    LogMsg(9, VolDebugLevel, stdout, "SFS: Found a volume for Inodesummary %d", i);
	    VolumeSummary *startVsp = vsp;
	    int SalVolCnt = 0;
	    while (nVolumes && vsp->header.parent == rwvid){
		LogMsg(9, VolDebugLevel, stdout, "SFS: Setting Volume 0x%#08x inodesummary to %d",
		    rwvid, i);
		vsp->inSummary = &(inodeSummary[i]);
		SalVolCnt++;
		vsp++;
		nVolumes--;
	    }
	    rc = SalvageVolumeGroup(startVsp, SalVolCnt);
	    if (rc) {
		LogMsg(9, VolDebugLevel, stdout, "SalvageVolumeGroup failed with rc = %d, ABORTING", rc);
		CAMLIB_ABORT(VFAIL);
	    }
	    continue;
	}
	else {
	    LogMsg(0, VolDebugLevel, stdout, "*****No Volume corresponding to inodes with rwvid %#08x", 
		rwvid);
	    CleanInodes(&(inodeSummary[i]));
	}
    }
    while (nVolumes) {
	LogMsg(0, VolDebugLevel, stdout, "SalvageFileSys:  unclaimed volume header file or no Inodes in volume %x",
	    vsp->header.id);
	LogMsg(0, VolDebugLevel, stdout, "SalvageFileSys: Therefore only resetting inUse flag");
	ClearROInUseBit(vsp);
	nVolumes--;
	vsp++;
    }

    CAMLIB_END_TOP_LEVEL_TRANSACTION_2(CAM_PROT_TWO_PHASED, camstatus)
    if (camstatus){
	LogMsg(0, VolDebugLevel, stdout, "SFS: aborting salvage with status %d", camstatus);
	return (camstatus);
    }

    if (ForceSalvage && !singleVolumeNumber) {
	if (chdir(path) == 0)
	    unlink("FORCESALVAGE");
    }
    LogMsg(9, VolDebugLevel, stdout, "Leaving SalvageFileSys with rc = 0");
    return (0);
}

/* Return the name of the indicated block device */
PRIVATE char *devName(unsigned int dev)
{
#ifdef	__MACH__
    struct direct *dp;
    static char name[32];
    int rc;
    DIR *dirp = opendir("/dev");

    LogMsg(9, VolDebugLevel, stdout, "Entering devName(%u)", dev);
    assert(dirp != NULL);
    rc = chdir("/dev");
    assert(rc != -1);
    for (dp=readdir(dirp); dp!=NULL; dp=readdir(dirp)) {
	struct stat status;
	if (stat(dp->d_name, &status) != -1 &&
	    (status.st_mode & IFMT) == IFBLK &&
	    (status.st_rdev == dev)) {
		strcpy(name, dp->d_name);
		closedir(dirp);
		return name;
	    }
    }
    closedir(dirp);
    return NULL;
#endif /* __MACH__ */

#ifdef	__linux__
    return NULL;
#endif /* LINUX */

#ifdef __NetBSD__
    LogMsg(0, VolDebugLevel, stdout, "Arrghhh... devName() not implemented for NetBSD yet");
    assert(0);
#endif /* __NetBSD__ */
}

PRIVATE	int SalvageVolumeGroup(register struct VolumeSummary *vsp, int nVols)
{
    struct ViceInodeInfo *inodes=0;
    int	size;
    int haveRWvolume = !(readOnly(vsp));
    LogMsg(9, VolDebugLevel, stdout, "Entering SalvageVolumeGroup(%#08x, %d)", 
	 vsp->header.parent, nVols);
    LogMsg(9, VolDebugLevel, stdout, "ForceSalvage = %d", ForceSalvage);

    /* if any of the volumes in this group are not to be salvaged
	then just return */
    if (skipvolnums != NULL){
	for (int i = 0; i < nVols; i++){
	    if (InSkipVolumeList(vsp[i].header.parent, skipvolnums, nskipvols)){
		LogMsg(9, VolDebugLevel, stdout, "Volume %x is not to be salvaged",
		    vsp[i].header.parent);
		return 0;
	    }
	}
    }
    if (!ForceSalvage && QuickCheck(vsp, nVols)){
	LogMsg(9, VolDebugLevel, stdout, "SVG: Leaving SVG with rc = 0");
	return 0;
    }

    /* get the list of inodes belonging to this 
       group of volumes from the inode file */
    struct InodeSummary *isp = vsp->inSummary;
    size = isp->nInodes * sizeof(struct ViceInodeInfo);
    inodes = (struct ViceInodeInfo *)malloc(size);
    assert(inodes != 0);
    assert(lseek(inodeFd, isp->index*
		 sizeof(struct ViceInodeInfo),L_SET) != -1);
    assert(read(inodeFd,(char *)inodes,size) == size);
    
    for (int i = 0; i < nVols; i++){
	LogMsg(9, VolDebugLevel, stdout, "SalvageVolumeGroup: Going to salvage Volume 0x%#08x header",
	    vsp[i].header.id);

	/* check volume head looks ok */
	if (SalvageVolHead(&(vsp[i])) == -1){
	    LogMsg(0, VolDebugLevel, stdout, "SalvageVolumeGroup: Bad Volume 0x%#08x");
	    if (i == 0)
		haveRWvolume = 0;
	    continue;
	}
	LogMsg(9, VolDebugLevel, stdout, "SVG: Going to salvage Volume 0x%#08x vnodes", vsp[i].header.id);

	/* make sure all small vnodes have a matching inode */
	if (VnodeInodeCheck(!(readOnly(&vsp[i])), inodes, 
			    vsp[i].inSummary->nInodes, &(vsp[i])) == -1) {
	    LogMsg(0, VolDebugLevel, stdout, "SVG: Vnode/Inode correspondence not OK(0x%08x).... Aborting set", 
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
    
    LogMsg(9, VolDebugLevel, stdout, "Leaving SalvageVolumeGroup(0x%#08x, %d)", 
	 vsp->header.parent, nVols);
    return 0;
}


/* Check to see if VolumeDiskData info looks ok */
PRIVATE int QuickCheck(register struct VolumeSummary *vsp, register int nVols)
{
    register int i;
    Error ec;

    LogMsg(9, VolDebugLevel, stdout, "Entering QuickCheck()");
    for (i = 0; i<nVols; i++) {
	VolumeDiskData volHeader;
	if (!vsp)
	    return 0;

	ExtractVolDiskInfo(&ec, vsp->volindex, &volHeader);

	if (ec == 0 && volHeader.dontSalvage == DONT_SALVAGE
	&& volHeader.needsSalvaged == 0 && volHeader.destroyMe == 0) {
	    if (volHeader.inUse == 1) {
		volHeader.inUse = 0;
		LogMsg(9, VolDebugLevel, stdout, "Setting volHeader.inUse = %d for volume %x",
			volHeader.inUse, volHeader.id);
		ReplaceVolDiskInfo(&ec, vsp->volindex, &volHeader);
		if (ec != 0)
		    return 0;	// write back failed
	    }
	    else {
		LogMsg(9, VolDebugLevel, stdout, "QuickCheck: inUse == %d", volHeader.inUse);
    }
	}
	else {
	    return 0;	// need to do a real salvage
	}
	vsp++;
    }
    return 1;	// ok to skip detailed salvage
}

#include "volinodes.h"	/* header magic number, etc. stuff */
PRIVATE int SalvageVolHead(register struct VolumeSummary *vsp)
{
    Error ec = 0;
    LogMsg(9, VolDebugLevel, stdout, "Entering SalvageVolHead(rw = %#08x, vid = %#08x)",
	 vsp->header.parent, vsp->header.id);
    if (readOnly(vsp)){
	ClearROInUseBit(vsp);
	LogMsg(9, VolDebugLevel, stdout, "SalvageVolHead returning with rc = 0");
	return 0;
    }
    CheckVolData(&ec, vsp->volindex);
    if (ec) {
	LogMsg(0, VolDebugLevel, stdout, "SalvageVolHead: bad VolumeData for volume %#08x",
	    vsp->header.id);
	LogMsg(0, VolDebugLevel, stdout, "SalvageVolHead: returning with rc = -1");
	return -1;
    }
    LogMsg(9, VolDebugLevel, stdout, "SalvageVolHead: returning with rc = 0");
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
PRIVATE int VnodeInodeCheck(int RW, struct ViceInodeInfo *ip, int nInodes, 
			    struct VolumeSummary *vsp) {
#ifdef	__MACH__
    LogMsg(9, VolDebugLevel, stdout, "Entering VnodeInodeCheck()");    
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
		assert(v_index.put(vnodeNumber, vnode->uniquifier, vnode) == 0);
		VolumeChanged = 1;
		continue;
	    }
	    else if (vnode->inodeNumber == 0){
		LogMsg(0, VolDebugLevel, stdout, 
		       "SalvageIndex:  Vnode 0x%x has no inodeNumber", vnodeNumber);
		assert(RW);
		assert(vnode->dataVersion == 0); // inodenumber == 0 only after create 
		LogMsg(0, VolDebugLevel, stdout, 
		       "SalvageIndex: Creating an empty object for it");
		vnode->inodeNumber = icreate(fileSysDevice, 0,
					     vsp->header.id, vnodeNumber,
					     vnode->uniquifier, 0);
		assert(vnode->inodeNumber > 0);
		if (vnode->cloned)
		    vnode->cloned = 0;		// invoke COW - XXX added 9/30/92 Puneet Kumar
		assert(v_index.put(vnodeNumber, vnode->uniquifier, vnode) == 0);
		VolumeChanged = 1;
		continue;
	    }
	}

	/* find an inode with matching vnodeNumber */
	while (nInodes && ip->u.vnode.vnodeNumber < vnodeNumber){
	    ip++;
	    nInodes--;
	}
	foundinode = 0;
	register struct ViceInodeInfo *lip = ip;
	register int lnInodes = nInodes;
	/* skip over old inodes with same vnodeNumber */
	while (lnInodes && lip->u.vnode.vnodeNumber == vnodeNumber){
	    if (vnode->inodeNumber == lip->inodeNumber){
		foundinode = 1;
		break;
	    }
	    else {
		LogMsg(0, VolDebugLevel, stdout, "VICheck: Found old inode %d for vnode number %d", 
		    lip->inodeNumber, vnodeNumber);	
		lip++;
		lnInodes--;
	    }
	}

	if (foundinode) {	/* lip points to matching inode summary*/
	    if (RW) { /* check uniquifier and data versions for RW volumes */
		Unique_t vu,iu;
		FileVersion vd,id;
		vu = vnode->uniquifier;
		iu = lip->u.vnode.vnodeUniquifier;
		vd = vnode->dataVersion;
		id = lip->u.vnode.inodeDataVersion;
		if ((vu != iu) ||(vd != id)) {
		    LogMsg(0, VolDebugLevel, stdout, 
			   "SI: Vnode (0x%x.%x.%x) uniquifier(0x%x)/dataversion(%d) doesn't match with inode(0x%x/%d); marking BARREN",
			vsp->header.id, vnodeNumber, vnode->uniquifier, vu, vd, iu, id);
		    SetBarren(vnode->versionvector);
		    assert(v_index.put(vnodeNumber,vnode->uniquifier,vnode) == 0);
		    VolumeChanged = 1;
		    continue;
		}
	    }
	    if (lip->byteCount != vnode->length) {
		LogMsg(0, VolDebugLevel, stdout, "Vnode (%x.%x.%x): length incorrect; can't happen!",
		    vsp->header.id, vnodeNumber, vnode->uniquifier);
		LogMsg(0, VolDebugLevel, stdout, "Marking as BARREN ");
		SetBarren(vnode->versionvector);
		assert(v_index.put(vnodeNumber, vnode->uniquifier, vnode) == 0);
		VolumeChanged = 1;
		continue;
	    }
	    /* everything is fine - update inode linkcount */
	    lip->linkCount--;
	}
	else {
	    // didn't find an inode - 
	    // create a new inode for readonly volumes
	    // mark as BARREN for r/w volumes
	    if (readOnly(vsp)) {
		LogMsg(0, VolDebugLevel, stdout, 
		       "Vnode 0x%x.%x in a ro volume has no inode - creating one\n",
		       vnodeNumber, vnode->uniquifier);
		vnode->inodeNumber = icreate(fileSysDevice, 0,
					     vsp->header.parent, vnodeNumber,
					     vnode->uniquifier, vnode->dataVersion);
		assert(vnode->inodeNumber > 0);
		vnode->length = 0;
		assert(v_index.put(vnodeNumber,vnode->uniquifier,vnode) == 0);
		VolumeChanged = 1;
	    }
	    else {
		if (!IsBarren(vnode->versionvector)){
		    if (!debarrenize) {
			LogMsg(0, VolDebugLevel, stdout, 
			       "Vnode (%x.%x.%x) incorrect inode - marking as BARREN",
			       vsp->header.id, vnodeNumber, vnode->uniquifier);
			
			SetBarren(vnode->versionvector);
			
		    }
		    else {
			LogMsg(0, VolDebugLevel, stdout, 
			       "Vnode 0x%x.%x.%x incorrect inode - Correcting\n",
			       vsp->header.id, vnodeNumber, vnode->uniquifier);
			vnode->inodeNumber = icreate(fileSysDevice, 0,
						     vsp->header.parent, vnodeNumber,
						     vnode->uniquifier, vnode->dataVersion);
			assert(vnode->inodeNumber > 0);
			vnode->length = 0;
			extern ViceVersionVector NullVV;
			vnode->versionvector = NullVV;
		    }
		    assert(v_index.put(vnodeNumber, vnode->uniquifier, vnode) == 0);
		    VolumeChanged = 1;
		}
		else {	// Barren vnode 
		    if (debarrenize) {
			LogMsg(0, VolDebugLevel, stdout, 
			       "Vnode 0x%x.%x.%x is BARREN - Debarrenizing\n",
			       vsp->header.id, vnodeNumber, vnode->uniquifier);
			vnode->inodeNumber = icreate(fileSysDevice, 0,
						     vsp->header.parent, vnodeNumber,
						     vnode->uniquifier, vnode->dataVersion);
			assert(vnode->inodeNumber > 0);
			vnode->length = 0;
			extern ViceVersionVector NullVV;
			vnode->versionvector = NullVV;
			assert(v_index.put(vnodeNumber,vnode->uniquifier,vnode) == 0);
			VolumeChanged = 1;
		    }
		}
	    }
	    continue;
	}
    }

    assert(vnext(vnode) == -1);
    assert(nVnodes == 0);
    return 0;
#endif /* __MACH__ */

#if defined(__linux__) || defined(__NetBSD__)
    LogMsg(0, VolDebugLevel, stdout, "Arrrgghhh... VnodeInodeCheck() not yet implemented");
    assert(0);
#endif /* __NetBSD__ || LINUX */
}
/* inodes corresponding to a volume that has been blown away.
 * We need to idec them
 */
PRIVATE void CleanInodes(struct InodeSummary *isp) {
#ifdef	__MACH__
    int size;
    struct ViceInodeInfo *inodes = 0;

    size = isp->nInodes * sizeof(struct ViceInodeInfo);
    inodes = (struct ViceInodeInfo *)malloc(size);
    assert(inodes != 0);
    assert(lseek(inodeFd, isp->index * sizeof(struct ViceInodeInfo), 
		  L_SET) != -1);
    assert(read(inodeFd, (char *)inodes, size) == size);
    
    for(int i = 0; i < isp->nInodes; i++) {
	ViceInodeInfo *ip = &inodes[i];
	assert(ip->linkCount > 0);
	LogMsg(0, VolDebugLevel, stdout, "#####Scavanging inode %u, size %u, p=(%u,%u,%u,%u)",
	     ip->inodeNumber, ip->byteCount,
	    ip->u.param[0], ip->u.param[1], ip->u.param[2], ip->u.param[3]);
	while(ip->linkCount > 0) {
	    assert(idec(fileSysDevice, ip->inodeNumber, 
			ip->u.param[0]) == 0);
	    ip->linkCount--;
	}
    }
    free(inodes);
#endif /* __MACH__ */

#if defined(__linux__) || defined(__NetBSD__)
    LogMsg(0, VolDebugLevel, stdout, "Arrrghh... CleanInodes() not yet implemented");
    assert(0);
#endif
    
}

PRIVATE struct VnodeEssence *CheckVnodeNumber(VnodeId vnodeNumber, Unique_t unq)
{
    VnodeClass vclass;
    struct VnodeInfo *vip;

    LogMsg(39, VolDebugLevel, stdout, "Entering CheckVnodeNumber(%d)", vnodeNumber);
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
/* validity. This routine is passed as a paramter to EnumerateDir */
PRIVATE void JudgeEntry(struct DirSummary *dir, char *name,
		VnodeId vnodeNumber, Unique_t unique)
{
    int rc = 0;
    struct VnodeEssence *vnodeEssence;

    LogMsg(39, VolDebugLevel, stdout, "Entering JudgeEntry(%s (0x%#08x.%x.%x))", name, dir->Vid, vnodeNumber, unique);

    vnodeEssence = CheckVnodeNumber(vnodeNumber, unique);
    if (vnodeEssence == NULL || vnodeEssence->unique != unique) {
	LogMsg(0, VolDebugLevel, stdout, "JE: directory vnode 0x%#08x.%x.%x: invalid entry %s; ",
	    dir->Vid, dir->vnodeNumber, dir->unique, name);
	LogMsg(0, VolDebugLevel, stdout, "JE: child vnode not allocated or uniqfiers dont match; cannot happen");
	assert(0);
      }
    if (strcmp(name,".") == 0) {
	if (dir->vnodeNumber != vnodeNumber || dir->unique != unique) {
	    LogMsg(0, VolDebugLevel, stdout, "JE:directory vnode 0x%#08x.%x.%x: bad '.' entry (was 0x%x.%x); ",
	        dir->Vid, dir->vnodeNumber, dir->unique, vnodeNumber, unique);
	    LogMsg(0, VolDebugLevel, stdout, "JE: Bad '.' - cannot happen ");
	    assert(0);
	}
	dir->haveDot = 1;
    }
    else if (strcmp(name,"..") == 0) {
	ViceFid pa;
	
	if (dir->vparent) {
	    struct VnodeEssence *dotdot;
	    pa.Vnode = dir->vparent;
	    dotdot = CheckVnodeNumber(pa.Vnode, dir->uparent);
	    assert(dotdot != NULL); /* XXX Should not be assert */
	    pa.Unique = dotdot->unique;
	    assert(pa.Unique == dir->uparent);
	}
	else {
	    pa.Vnode = dir->vnodeNumber;
	    pa.Unique = dir->unique;
	}
	if (pa.Vnode != vnodeNumber || pa.Unique != unique) {
	    LogMsg(0, VolDebugLevel, stdout, "JE: directory vnode 0x%#08x.%x.%x: bad '..' entry (was 0x%x.%x); Shouldnt Happen ",
	        dir->Vid, dir->vnodeNumber, dir->unique, vnodeNumber, unique);
	    assert(0);
	}
	dir->haveDotDot = 1;
    }
    else {
	if ((vnodeEssence->vparent != dir->vnodeNumber)	||
	    (vnodeEssence->uparent != dir->unique)){
	    LogMsg(0, VolDebugLevel, stdout, "JE: parent = 0x%#08x.%x.%x ; child thinks parent is 0x%x.%x; Shouldnt Happen", 
		dir->Vid, dir->vnodeNumber, dir->unique, 
		vnodeEssence->vparent, vnodeEssence->uparent);
	    assert(0);
	}
	vnodeEssence->claimed = 1;
    }
    
    LogMsg(39, VolDebugLevel, stdout, "JE: resetting copy of linkcount from %d to %d", 
	 vnodeEssence->count, vnodeEssence->count - 1);
    vnodeEssence->count--;
    LogMsg(39, VolDebugLevel, stdout, "Leaving JudgeEntry(%s (0x%#08x.%x.%x))", name, dir->Vid, vnodeNumber, unique);

}

PRIVATE void MarkLogEntries(rec_dlist *loglist, VolumeSummary *vsp) {
    LogMsg(9, SrvDebugLevel, stdout,
	   "Entering MarkLogEntries....\n");
    if (!loglist) {
	LogMsg(0, SrvDebugLevel, stdout,
	       "MarkLogEntries: loglist was NULL ... Not good\n");
	assert(0);
    }
    assert(vsp->logbm);
    rec_dlist_iterator next(*loglist);
    recle *r;
    while (r = (recle *)next()) {
	if (vsp->logbm->Value(r->index))  {
	    LogMsg(0, SrvDebugLevel, stdout,
		   "MarkLogEntries: This index %d already set\n",
		   r->index);
	    r->print();
	    assert(0);
	}
	else 
	    vsp->logbm->SetIndex(r->index);
	rec_dlist *childlist;
	if (childlist = r->HasList()) {
	    LogMsg(9, SrvDebugLevel, stdout, 
		   "MarkLogEntries: Looking recursively.....\n");
	    MarkLogEntries(childlist, vsp);
	}
    }
    LogMsg(9, SrvDebugLevel, stdout,
	   "Leaving MarkLogEntries....\n");
}


PRIVATE void DistilVnodeEssence(VnodeClass vclass, VolumeId volid) {

    register struct VnodeInfo *vip = &vnodeInfo[vclass];
    struct VnodeClassInfo *vcp = &VnodeClassInfo_Array[vclass];
    char buf[SIZEOF_LARGEDISKVNODE];
    struct VnodeDiskObject *vnode = (struct VnodeDiskObject *) buf;
    vindex v_index(volid, vclass, fileSysDevice, vcp->diskSize);
    vindex_iterator vnext(v_index);

    LogMsg(9, VolDebugLevel, stdout, "Entering DistilVnodeEssence(%d, %u)", vclass, volid);

    vip->nVnodes = v_index.vnodes();

    if (vip->nVnodes > 0) {
	vip->vnodes = (struct VnodeEssence *)calloc(vip->nVnodes, sizeof(struct VnodeEssence));
	assert(vip->vnodes != NULL);
	if (vclass == vLarge) {	    /* alloc space for directory entries */
	    vip->inodes = (Inode *) calloc(vip->nVnodes, sizeof (Inode));
	    assert(vip->inodes != NULL);
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
		assert(vclass == vLarge);
		/* for directory vnodes inode can never be zero */
		/* if the inode number is NEWVNODEINODE blow away vnode */
		assert(vnode->inodeNumber != 0);
		if (vnode->inodeNumber == NEWVNODEINODE){
		    /* delete the vnode */
		    LogMsg(0, VolDebugLevel, stdout, "DistilVnodeEssence: Found a Directory vnode %d that has a special inode ... deleting vnode ",
			vnodeIndex);
		    vip->nAllocatedVnodes--;
		    vip->volumeBlockCount -= vep->blockCount;
		    bzero(vep, sizeof(struct VnodeEssence));
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
    VolumeId vid;
    register VnodeClass vclass;
    VolumeDiskData volHeader;
    struct DirSummary dir;
    struct DirHandle dirHandle;
    struct VnodeInfo *dirVnodeInfo;
    int RecoverableResLogs = (AllowResolution && vsp->vollog != NULL);
    Error ec = 0;

    LogMsg(0, VolDebugLevel, stdout, "Entering DCC(0x%x)", vsp->header.id);
    vid = vsp->header.id;
    LogMsg(29, VolDebugLevel, stdout, "DCC: Extracting DiskInfo for 0x%#08x", vid);
    ExtractVolDiskInfo(&ec, vsp->volindex, &volHeader);
    if (ec != 0){
	LogMsg(0, VolDebugLevel, stdout, "DCC: Error during ExtractVolDiskInfo(%#08x)",
	    vid);
	return;
    }
    
    assert(volHeader.destroyMe != DESTROY_ME);
    DistilVnodeEssence(vLarge, vid);
    DistilVnodeEssence(vSmall, vid);

    if (RecoverableResLogs) vsp->logbm = new bitmap(vsp->vollog->bmsize(), 0);
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
	/* dir inode is data inode of directory vnode */
	SetSalvageDirHandle(&dir.dirHandle, dir.Vid, fileSysDevice, dirVnodeInfo->inodes[i]);
	dir.dirHandle.vnode = dir.vnodeNumber;
	dir.dirHandle.unique = dir.unique;
	LogMsg(9, VolDebugLevel, stdout, "DCC: Going to check Directory (0x%#08x.%x.%x)", 
	    vid, dir.vnodeNumber, dir.unique);
	if (!DirOK((long *)&dir.dirHandle)) {
	    LogMsg(0, VolDebugLevel, stdout, "DCC: Bad Dir(0x%#08x.%x.%x) in rvm...Aborting", 
		dir.dirHandle.volume, dir.vnodeNumber, dir.unique);
	    assert(0);
	}
	dirHandle = dir.dirHandle;
	EnumerateDir((long *)&dirHandle, (int (*)(void * ...))JudgeEntry, (long)&dir);
	LogMsg(9, VolDebugLevel, stdout, "DCC: Finished checking directory(%#08x.%x.%x)",
	    vsp->header.id, dir.vnodeNumber, dir.unique);
	if (RecoverableResLogs) {
	    LogMsg(9, SrvDebugLevel, stdout, 
		   "DCC: Marking log entries for 0x%x.%x.%x\n",
		   vid, dirVnodeInfo->vnodes[i].vid, 
		   dirVnodeInfo->vnodes[i].unique);
	    MarkLogEntries(dirVnodeInfo->vnodes[i].log, vsp);
	}
    }

    // salvage the resolution logs 
    if (RecoverableResLogs) {
	LogMsg(0, SrvDebugLevel, stdout, 
	       "DCC: Salvaging Logs for volume 0x%x\n",
	       vid);
	vsp->vollog->SalvageLog(vsp->logbm);
	delete vsp->logbm;
    }
    /* check link counts, parent pointers */
    for (vclass = 0; vclass < nVNODECLASSES; vclass++){
	int nVnodes = vnodeInfo[vclass].nVnodes;
	struct VnodeClassInfo *vcp = &VnodeClassInfo_Array[vclass];
	struct VnodeEssence *vnodes = vnodeInfo[vclass].vnodes;
	FilesInVolume += vnodeInfo[vclass].nAllocatedVnodes;
	BlocksInVolume += vnodeInfo[vclass].volumeBlockCount;

	for (i = 0; i<nVnodes; i++) {
	    register struct VnodeEssence *vnp = &vnodes[i];
	    LogMsg(29, VolDebugLevel, stdout, "DCC: Check Link counts:");
	    LogMsg(29, VolDebugLevel, stdout, "\t linkcount = %d, index = %d, parent = 0x%x, unique = 0x%x",
		vnp->count, i, vnp->vparent, vnp->unique);
	    if (vnp->changed || vnp->count != 0) {
		LogMsg(0, VolDebugLevel, stdout, "DCC: For Vnode (0x%#08x.%x.%x) parent (0x%x.%x): linkcount is %d",
		    vid, vnp->vid, vnp->unique, vnp->vparent, vnp->uparent, vnp->count);
		assert(0);
		/* You can bring up the server by forcing this volume off-line
		 * edit /vice/vol/skipsalvage.  The format of this file is
		 * the fist line has the number of volumes to skip, and
		 * each subsequent line has a volume number.  
		 */
	    }
	}
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
	LogMsg(0, VolDebugLevel, stdout, "DCC: Warning - uniquifier is too low for volume (0x%x)", vid);
    }

    /* Turn off the inUse bit; the volume's been salvaged! */
    volHeader.inUse = 0;
    LogMsg(9, VolDebugLevel, stdout, "DCC: setting volHeader.inUse = %d for volume 0x%#08x",
	 volHeader.inUse, volHeader.id);
    volHeader.needsSalvaged = 0;
    volHeader.needsCallback = (VolumeChanged != 0);    
    volHeader.dontSalvage = DONT_SALVAGE;
    VolumeChanged = 0;
    ReplaceVolDiskInfo(&ec, vsp->volindex, &volHeader);
    if (ec != 0){
	LogMsg(0, VolDebugLevel, stdout, "DCC: Couldnt write the volHeader for volume (%#08x)",
	    vsp->header.id);
	return;    /* couldn't write out the volHeader */
    }
    LogMsg(0, VolDebugLevel, stdout, "done:\t%d files/dirs,\t%d blocks", FilesInVolume, BlocksInVolume);
    LogMsg(9, VolDebugLevel, stdout, "Leaving DCC()");
}

/* Zero inUse and needsSalvaged fields in VolumeDiskData */
PRIVATE void ClearROInUseBit(struct VolumeSummary *summary)
{
    Error ec;
    VolumeId headerVid = summary->header.id;
    VolumeDiskData volHeader;

    LogMsg(9, VolDebugLevel, stdout, "Entering ClearROInUseBit()");

    ExtractVolDiskInfo(&ec, summary->volindex, &volHeader);
    assert(ec == 0);

    if (volHeader.destroyMe == DESTROY_ME)
	return;
    volHeader.inUse = 0;
    LogMsg(9, VolDebugLevel, stdout, "ClearROInUseBit: setting volHeader.inUse = %d for volume %x",
		volHeader.inUse, volHeader.id);
    volHeader.needsSalvaged = 0;
    volHeader.dontSalvage = DONT_SALVAGE;

    ReplaceVolDiskInfo(&ec, summary->volindex, &volHeader);
    assert(ec == 0);
}

/* "ask" the fileserver to take a volume offline */
PRIVATE int AskOffline(VolumeId volumeId)
{
    ProgramType *pt, tmp;
    int rc = 0;

    LogMsg(9, VolDebugLevel, stdout, "Entering AskOffline(%x)", volumeId);

    /* Note:  we're depending upon file server to put the volumes online
       after salvaging */
    /* masquerade as fileserver for FSYNC_askfs call */
    assert(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);
    tmp = *pt;
    *pt = fileServer;
    rc = FSYNC_askfs(volumeId, FSYNC_OFF, FSYNC_SALVAGE);
    *pt = tmp;
    if (rc == FSYNC_DENIED) {
	LogMsg(0, VolDebugLevel, stdout, "AskOffline:  file server denied offline request; a general salvage is required.");
        LogMsg(0, VolDebugLevel, stdout, "Salvage aborted");
	return(VNOVOL);
    }
    return (0);
}

/* "ask" the fileserver to put a volume back online */
PRIVATE int AskOnline(VolumeId volumeId)
{
    ProgramType *pt, tmp;
    int rc = 0;

    LogMsg(9, VolDebugLevel, stdout, "Entering AskOnline(%x)", volumeId);

    /* Note:  we're depending upon file server to put the volumes online
       after salvaging */
    /* masquerade as fileserver for FSYNC_askfs call */
    assert(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);
    tmp = *pt;
    *pt = fileServer;
    rc = FSYNC_askfs(volumeId, FSYNC_ON, 0);
    *pt = tmp;
    if (rc == FSYNC_DENIED) {
	LogMsg(0, VolDebugLevel, stdout, "AskOnline:  file server denied online request; a general salvage is required.");
    }
    return (0);
}

/* copy data from one inode to another */
PRIVATE int CopyInode(Device device, Inode inode1, Inode inode2)
{
    char buf[4096];
    register int fd1, fd2, n;

    LogMsg(9, VolDebugLevel, stdout, "Entering CopyInode()");

    fd1 = iopen(device, inode1, O_RDONLY);
    fd2 = iopen(device, inode2, O_WRONLY);
    assert(fd1 != -1);
    assert(fd2 != -1);
    while ((n = read(fd1, buf, sizeof(buf))) > 0)
	assert(write(fd2, buf, n) == n);
    assert (n == 0);
    close(fd1);
    close(fd2);
    return 0;
}

/* Prints out a list of all inodes into the Log */
PRIVATE void PrintInodeList() {
    register struct ViceInodeInfo *ip;
    struct ViceInodeInfo *buf;
    struct stat status;
    register nInodes;

    LogMsg(9, VolDebugLevel, stdout, "Entering PrintInodeList()");

    assert(fstat(inodeFd, &status) == 0);
    buf = (struct ViceInodeInfo *) malloc(status.st_size);
    assert(buf != NULL);
    nInodes = status.st_size / sizeof(struct ViceInodeInfo);
    assert(read(inodeFd, (char *)buf, status.st_size) == status.st_size);
    for(ip = buf; nInodes--; ip++) {
	LogMsg(0, VolDebugLevel, stdout, "Inode:%u, linkCount=%d, size=%u, p=(%u,%u,%u,%u)",
	    ip->inodeNumber, ip->linkCount, ip->byteCount,
	    ip->u.param[0], ip->u.param[1], ip->u.param[2], ip->u.param[3]);
    }
    free((char *)buf);
}

/* release file server and volume utility locks (for full salvage only) */
PRIVATE void release_locks(int volUtil) {
    int fslock;

    if (volUtil) {  /* not running full salvage */
	return;
    }
    fslock = open("/vice/vol/fs.lock", O_CREAT|O_RDWR, 0666);
    assert(fslock >= 0);
    if (flock(fslock, LOCK_UN) != 0) {
	LogMsg(0, VolDebugLevel, stdout, "release_locks: unable to release file server lock");
    }
    else {
	LogMsg(9, VolDebugLevel, stdout, "release_locks: released file server lock");
    }
    close(fslock);

    fslock = open ("/vice/vol/volutil.lock", O_CREAT|O_RDWR, 0666);
    assert(fslock >= 0);
    if (flock(fslock, LOCK_UN) != 0) {
	LogMsg(0, VolDebugLevel, stdout, "release_locks: unable to release volume utility lock");
    }
    else {
	LogMsg(9, VolDebugLevel, stdout, "release_locks: released volume utility lock");
    }
    close(fslock);
}

/* if some volumes shouldnt be salvaged their number is 
   placed in a file DONTSALVVOLS.  Check if file exists
   and read in the volume numbers.  The format of the 
   file is "<number of volumes> \n <volume numbers in hex>"
*/
PRIVATE void GetSkipVolumeNumbers() {
    struct stat s1;
    FILE *skipsalv;

    if (stat(DONTSALVVOLS, &s1) == 0){
	/* file exists */
	skipsalv = fopen(DONTSALVVOLS, "r");
	assert(skipsalv != NULL);
	fscanf(skipsalv, "%d\n", &nskipvols);
	skipvolnums = (VolumeId *)malloc(nskipvols * sizeof(VolumeId));
    { /* drop scope for int i below; to avoid identifier clash */
	for (int i = 0; i < nskipvols; i++)
	    fscanf(skipsalv, "%x\n", &(skipvolnums[i]));
    } /* drop scope for int i above; to avoid identifier clash */

	fclose(skipsalv);
	LogMsg(1, VolDebugLevel, stdout, "The Volume numbers to be skipped salvaging are :");
	for (int i = 0; i < nskipvols; i++){
	    LogMsg(1, VolDebugLevel, stdout, "Volume %x", skipvolnums[i]);
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
PRIVATE void SanityCheckFreeLists() {
    int i,j;
    char zerobuf[SIZEOF_LARGEDISKVNODE];
    struct VnodeDiskObject *zerovn = (struct VnodeDiskObject *) zerobuf;
    bzero(zerovn, SIZEOF_LARGEDISKVNODE);
    
    LogMsg(0, VolDebugLevel, stdout, "SanityCheckFreeLists: Checking RVM Vnode Free lists.");
    for (i = 0; i < CAMLIB_REC(SmallVnodeIndex); i++) {
	if (bcmp(CAMLIB_REC(SmallVnodeFreeList[i]), zerovn,
		 SIZEOF_SMALLDISKVNODE) != 0) {
	    LogMsg(0, VolDebugLevel, stdout,"Small Free Vnode at index %d not zero!", i);
	    assert(0);
	}
	
	for (j = i + 1; j < CAMLIB_REC(SmallVnodeIndex); j++)
	    if (CAMLIB_REC(SmallVnodeFreeList[i]) ==
		CAMLIB_REC(SmallVnodeFreeList[j])) {
		LogMsg(0, VolDebugLevel, stdout, "Vdo 0x%x appears twice (%d and %d) in smallfreelist!",
		       CAMLIB_REC(SmallVnodeFreeList[i]), i, j);
		assert(0);
	    }
    }
    
    for (i = 0; i < CAMLIB_REC(LargeVnodeIndex); i++) {
	if (bcmp(CAMLIB_REC(LargeVnodeFreeList[i]), zerovn,
		 SIZEOF_LARGEDISKVNODE) != 0) {
	    LogMsg(0, VolDebugLevel, stdout, "Large Free Vnode at index %d not zero!", i);
	    assert(0);
	}
	
	for (j = i + 1; j < CAMLIB_REC(LargeVnodeIndex); j++)
	    if (CAMLIB_REC(LargeVnodeFreeList[i]) ==
		CAMLIB_REC(LargeVnodeFreeList[j])) {
		LogMsg(0, VolDebugLevel, stdout, "Vdo 0x%x appears twice (%d and %d) in largefreelist!",
		       CAMLIB_REC(LargeVnodeFreeList[i]), i, j);
		assert(0);
	    }		    
    }
}

	
/* iterate through the VolumeList in RVM, destroying any volumes which
 * have the destroyMe flag set in VolDiskData. Current opinion feels that
 * the volume shouldn't be destroyed if it's in the skipsalvage file.
 */
PRIVATE int DestroyBadVolumes() {
    LogMsg(0, VolDebugLevel, stdout, "DestroyBadVolumes: Checking for destroyed volumes.");
    for (int i = 0; i <= MAXVOLS; i++) {
	struct VolumeHeader header;

	if (VolHeaderByIndex(i, &header) == -1) {
	    break;	/* index has exceeded maxvolid */
	}
	
	/* eliminate bogus cases */
	if (header.stamp.magic != VOLUMEHEADERMAGIC)
	    continue; 	/* corresponds to purged volume */
	
	if (CAMLIB_REC(VolumeList[i].data.volumeInfo)->destroyMe==DESTROY_ME){
	    LogMsg(0, VolDebugLevel, stdout, "Salvage: Removing destroyed volume %x", header.id);
	    
	    /* Need to get device */
	    struct stat status;
	    char *part = CAMLIB_REC(VolumeList[i].data.volumeInfo)->partition;
	    if (stat(part, &status) == -1) {
		LogMsg(0, VolDebugLevel, stdout, "Couldn't find partition \"%s\" for destroy", part);
		return(VFAIL);
	    }
	    /* Remove the volume */
	    assert(DeleteRvmVolume(i, status.st_dev) == 0);
	}
    }
    return(0);
}

PRIVATE void FixInodeLinkcount(struct ViceInodeInfo *inodes, 
			       struct InodeSummary *isp) {
    struct ViceInodeInfo *ip;
    int totalInodes = isp->nInodes;
    for (ip = inodes; totalInodes; ip++,totalInodes--) {
	static TraceBadLinkCounts = 25;
	if (ip->linkCount != 0 && TraceBadLinkCounts) {
	    TraceBadLinkCounts--; 
	    LogMsg(0, VolDebugLevel, stdout, "#### DEBUG #### Link count incorrect by %d; inode %u, size %u, p=(%u,%u,%u,%u)",
	    ip->linkCount, ip->inodeNumber, ip->byteCount,
	    ip->u.param[0], ip->u.param[1], ip->u.param[2], ip->u.param[3]);
	}

	/* Delete any links that are still unaccounted for */
	while (ip->linkCount > 0) {
	   assert(idec(fileSysDevice,ip->inodeNumber, ip->u.param[0]) == 0);
	   ip->linkCount--;
	}
	while (ip->linkCount < 0) {
	   assert(iinc(fileSysDevice,ip->inodeNumber, ip->u.param[0]) == 0);
	   ip->linkCount++;
	}
    }
}

/* zero out global variables */
PRIVATE void zero_globals() {
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
    bzero(volumeSummary,
	sizeof(struct VolumeSummary) * MAXVOLS_PER_PARTITION);
}



/* routines that get inode and volume summaries */

/* Comparison routine for inode sort. Inodes are sorted */
/* by volume and vnode number */
PRIVATE int CompareInodes(register struct ViceInodeInfo *p1,
				register struct ViceInodeInfo *p2)
{
    if (p1->u.vnode.vnodeNumber == INODESPECIAL ||
	p2->u.vnode.vnodeNumber == INODESPECIAL) {
	LogMsg(0, VolDebugLevel, stdout, "CompareInodes: found special inode! Aborting");
	assert(0);
    }

    if (p1->u.vnode.volumeId<p2->u.vnode.volumeId)
	return -1;
    if (p1->u.vnode.volumeId>p2->u.vnode.volumeId)
	return 1;
    if (p1->u.vnode.vnodeNumber < p2->u.vnode.vnodeNumber)
	return -1;
    if (p1->u.vnode.vnodeNumber > p2->u.vnode.vnodeNumber)
	return 1;
    /* The following tests are reversed, so that the most desirable
       of several similar inodes comes first */
    if (p1->u.vnode.vnodeUniquifier > p2->u.vnode.vnodeUniquifier)
	return -1;
    if (p1->u.vnode.vnodeUniquifier < p2->u.vnode.vnodeUniquifier)
	return 1;
    if (p1->u.vnode.inodeDataVersion > p2->u.vnode.inodeDataVersion)
	return -1;
    if (p1->u.vnode.inodeDataVersion < p2->u.vnode.inodeDataVersion)
	return 1;
    return 0;
}

PRIVATE void CountVolumeInodes(register struct ViceInodeInfo *ip,
		int maxInodes, register struct InodeSummary *summary)
{
    int volume = ip->u.vnode.volumeId;
    int rwvolume = volume;
    register n, nSpecial;
    register Unique_t maxunique;

    LogMsg(9, VolDebugLevel, stdout, "Entering CountVolumeInodes()");

    n = nSpecial = 0;
    maxunique = 0;
    while (maxInodes-- && volume == ip->u.vnode.volumeId) {
	n++;
	if (ip->u.vnode.vnodeNumber == INODESPECIAL) {
	    LogMsg(0, VolDebugLevel, stdout, "CountVolumeInodes: Bogus specialinode; can't happen");
	    assert(0);
	}
	else {
	    if (maxunique < ip->u.vnode.vnodeUniquifier)
		maxunique = ip->u.vnode.vnodeUniquifier;
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

    if (inodeinfo->u.vnode.vnodeNumber == INODESPECIAL) {
	LogMsg(0, VolDebugLevel, stdout, "OnlyOneVolume: tripped over INODESPECIAL- can't happen!");
	assert(FALSE);
    }
    return (inodeinfo->u.vnode.volumeId == singleVolumeNumber);
}

/*  
 *  Gets an inode summary by examining the vice disk partition directly.
 *  The inodes are sorted with qsort by order of the volume and vnode,
 *  respectively, with which they are associated.
 */

PRIVATE int GetInodeSummary(char *path, VolumeId singleVolumeNumber)
{
    struct stat status;
    int summaryFd;
    int rc = 0;
    int nInodes = 0;; 
    struct ViceInodeInfo *ip, *tmp_ip;
    struct InodeSummary summary;
    FILE *summaryFile;
    char *dev = fileSysDeviceName;

    LogMsg(9, VolDebugLevel, stdout, "Entering GetInodeSummary (%s, %x)", path, singleVolumeNumber);
    if(singleVolumeNumber)
	rc = ListViceInodes(dev, fileSysPath, path, OnlyOneVolume, singleVolumeNumber);
    else
	rc = ListViceInodes(dev, fileSysPath, path, NULL, singleVolumeNumber);

    if (rc == 0) {
	LogMsg(9, VolDebugLevel, stdout, "ListViceInodes returns success");
    }
    if(rc == -1) {
	LogMsg(0, VolDebugLevel, stdout, "Unable to get inodes for \"%s\"; not salvaged", dev);
	return(VFAIL);
    }
    inodeFd = open(path, O_RDWR, 0);
    if (inodeFd == -1 || fstat(inodeFd, &status) == -1) {
	LogMsg(0, VolDebugLevel, stdout, "No inode description file for \"%s\"; not salvaged", dev);
	return(VFAIL);
    }
    summaryFd = open("/tmp/salvage.temp", O_RDWR|O_CREAT|O_TRUNC, 0600);
    if (summaryFd == -1) {
	LogMsg(0, VolDebugLevel, stdout, "GetInodeSummary: Unable to create inode summary file");
	return(VFAIL);
    }

    summaryFile = fdopen(summaryFd,"w");
    assert(summaryFile != NULL);
    nInodes = status.st_size / sizeof(ViceInodeInfo);
    if (nInodes == 0) {
	LogMsg(0, VolDebugLevel, stdout, "%s vice inodes on %s; not salvaged", singleVolumeNumber? "No applicable": "No", dev);
	close(summaryFd);
	return(VNOVOL);
    }
    ip = tmp_ip = (struct ViceInodeInfo *) malloc(status.st_size);
    if (ip == NULL) {
	LogMsg(0, VolDebugLevel, stdout, "Unable to allocate enough space to read inode table; %s not salvaged", dev);
	return(VFAIL);
    }
    if (read(inodeFd, (char *)ip, status.st_size) != status.st_size) {
	LogMsg(0, VolDebugLevel, stdout, "Unable to read inode table; %s not salvaged", dev);
	return(VFAIL);
    }
    LogMsg(9, VolDebugLevel, stdout, "entering qsort(0x%x, %d, %d, 0x%x)", ip, nInodes,
		    sizeof(struct ViceInodeInfo), CompareInodes);
    qsort((void *)ip, nInodes, sizeof(struct ViceInodeInfo), 
	  (int (*)(const void *, const void *))CompareInodes);
    LogMsg(9, VolDebugLevel, stdout, "returned from qsort");
    if (lseek(inodeFd,0,L_SET) == -1 ||
	write(inodeFd, (char *)ip, status.st_size) != status.st_size) {
	LogMsg(0, VolDebugLevel, stdout, "Unable to rewrite inode table; %s not salvaged", dev);
	return(VFAIL);
    }
    summary.index = 0;	    /* beginning index for each volume */
    while (nInodes) {
	CountVolumeInodes(ip, nInodes, &summary);
	LogMsg(9, VolDebugLevel, stdout, "returned from CountVolumeInodes");
	if (fwrite((char *)&summary, sizeof (summary), 1, summaryFile) != 1) {
	    LogMsg(0, VolDebugLevel, stdout, "Difficulty writing summary file; %s not salvaged",dev);
	    return(VNOVNODE);
	}
	summary.index += (summary.nInodes);
	nInodes -= summary.nInodes;
	ip += summary.nInodes;
    }
    fflush(summaryFile);	/* Not fclose, because debug would not work */

    assert(fstat(summaryFd,&status) != -1);
    /* store inode info in global array */
    LogMsg(9, VolDebugLevel, stdout, "about to malloc %d bytes for inodeSummary", status.st_size);
    inodeSummary = (struct InodeSummary *) malloc(status.st_size);
    assert(inodeSummary != NULL);
    assert(lseek(summaryFd, 0, L_SET) != -1);
    assert(read(summaryFd, (char *)inodeSummary, status.st_size) == status.st_size);
    nVolumesInInodeFile = status.st_size / sizeof (struct InodeSummary);
    close(summaryFd);
    close(inodeFd);
    unlink("/tmp/salvage.temp");
    free(tmp_ip);
    LogMsg(9, VolDebugLevel, stdout, "Leaving GetInodeSummary()");
    return (0);
}

/* Comparison routine for volume sort. This is setup so */
/* that a read-write volume comes immediately before */
/* any read-only clones of that volume */
PRIVATE int CompareVolumes(register struct VolumeSummary *p1,
				register struct VolumeSummary *p2)
{

    LogMsg(9, VolDebugLevel, stdout, "Entering CompareVolumes()");

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
PRIVATE int GetVolumeSummary(VolumeId singleVolumeNumber) {

    DIR *dirp;
    Error ec;
    int rc = 0;
    int i;

    LogMsg(9, VolDebugLevel, stdout, "Entering GetVolumeSummary(%x)", singleVolumeNumber);

    /* Make sure the disk partition is readable */
    if (chdir(fileSysPath) == -1 || (dirp = opendir(".")) == NULL) {
	LogMsg(0, VolDebugLevel, stdout, "Can't read directory %s; not salvaged", fileSysPath);
	return(VNOVNODE);
    }

    /* iterate through all the volumes on this partition, and try to */
    /* match with the desired volumeid */
    LogMsg(39, VolDebugLevel, stdout, "GetVolSummary: filesyspath = %s nVolumes = %d", 
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
	assert(vsp->header.id == CAMLIB_REC(VolumeList[i]).data.volumeInfo->id);
	assert(vsp->header.parent ==
	       CAMLIB_REC(VolumeList[i]).data.volumeInfo->parentId);

	for (int j = i+1; j < MAXVOLS; j++) 
	    if (vsp->header.id == CAMLIB_REC(VolumeList[j]).header.id) assert(0);
	
	/* reject volumes from other partitions */
	LogMsg(39, VolDebugLevel, stdout, "Partition for vol-index %d, (id 0x%x) is (%s)",
	    i, vsp->header.id, (CAMLIB_REC(VolumeList[i]).data.volumeInfo)->partition);
	GetVolPartition(&ec, vsp->header.id, i, thispartition);
	LogMsg(39, VolDebugLevel, stdout, "GetVolSummary: For Volume id 0x%x GetVolPartition returns %s",
	    vsp->header.id, thispartition);
	if ((ec != 0) || (strcmp(thispartition, fileSysPath) != 0))
	    continue;	    /* bogus volume */

	/* Make sure volume is in the volid hashtable so SalvageInodes works */
	LogMsg(9, VolDebugLevel, stdout, "GetVolSummary: inserting volume %x into hashtable for index %d",
	    vsp->header.id, i);
	HashInsert(vsp->header.id, i);

	// prepare for checking resolution logs
	vsp->logbm = NULL;
	if ((CAMLIB_REC(VolumeList[i]).data.volumeInfo->ResOn & RVMRES) && AllowResolution &&
	    (vsp->header.type == readwriteVolume))
	    vsp->vollog = CAMLIB_REC(VolumeList[i]).data.volumeInfo->log;
	else 
	    vsp->vollog = NULL;
	
	/* Is this the specific volume we're looking for? */
	if (singleVolumeNumber && vsp->header.id == singleVolumeNumber) {
	    if (vsp->header.type == readonlyVolume) {	// skip readonly volumes
		LogMsg(0, VolDebugLevel, stdout, "%d is a read-only volume; not salvaged", singleVolumeNumber);
		return(VREADONLY);
	    }

	    if (vsp->header.type == readwriteVolume) {	// simple or replicated readwrite
		vsp->volindex = i;			// set the volume index
		rc = AskOffline(vsp->header.id);	// have the fileserver take it offline
		if (rc) {
		    LogMsg(0, VolDebugLevel, stdout, "GetVolumeSummary: AskOffline failed, returning");
		    return(rc);				/* we can't have it; return */
		}
		nVolumes = 1;		// in case we need this set
	    }
	}
	else {	    /* take all volumes for full salvage; increment volume count */
	    if (!singleVolumeNumber) {
		vsp->volindex = i;			// set the volume index
		if (nVolumes++ > MAXVOLS_PER_PARTITION) {
		    LogMsg(0, VolDebugLevel, stdout, 
			"More than %d volumes in partition %s; partition not salvaged\n", 
			MAXVOLS_PER_PARTITION);
		    return (VFAIL);
		}
	    }
	}
    }

    LogMsg(9, VolDebugLevel, stdout, "GetVolumeSummary: entering qsort for %d volumes", nVolumes);
    qsort((char *)volumeSummary, nVolumes, sizeof(struct VolumeSummary),
	  (int (*)(const void *, const void *)) CompareVolumes);
    return (0);
}
