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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/volutil/vol-salvage.private.h,v 4.1 1997/01/08 21:52:35 rvb Exp $";
#endif /*_BLURB_*/





#define ROOTINODE	2	/* Root inode of a 4.2 Unix file system
				   partition */
#define	DONTSALVVOLS	"/vice/vol/skipsalvage"
#define readOnly(vsp)	((vsp)->header.type==ROVOL || (vsp)->header.type == BACKVOL)

#include <rec_dlist.h>
#include <bitmap.h>
#include <recov_vollog.h>

struct InodeSummary {		/* Inode summary file: an entry for each volume in a partition */
    VolId	volumeId;	/* Volume id */
    VolId	RWvolumeId;	/* RW volume associated */
    int		index;		/* index into inode file (0, 1, 2 ...) */
    int		nInodes;	/* Number of inodes for this volume */
    Unique_t	maxUniquifier;	/* The max. uniquifier in all the inodes for this volume */
};

struct VolumeSummary {		/* one entry for each volume */
    char	*fileName;	/* File name on the partition for the volume header */
    int		volindex;	/* volume's index in recoverable storage */
    struct VolumeHeader header; /* volume number, rw volume number */
    byte	wouldNeedCallback; /* set if the file server should issue
				    call backs for all the files in this volume when
				    the volume goes back on line */
    struct  InodeSummary *inSummary; /* this volume's inodes */
    recov_vol_log *vollog;	// log storage for volume
    bitmap	*logbm;		// bitmap for checking if volume log has no leaks
};

struct VnodeEssence {
    short 	count; 		/* Number of references to vnode; MUST BE SIGNED */
    unsigned 	claimed:1; 	/* Set when a parent directory containing an entry
				   referencing this vnode is found.  The claim
				   is that the parent in "parent" can point to
				   this vnode, and no other */
    unsigned 	changed:1; 	/* Set if any parameters (other than the count)
				   in the vnode change.   It is determined if the
				   link count has changed by noting whether it is
				   0 after scanning all directories */
    unsigned short blockCount;  /* Number of blocks (1K) used by this vnode, approximately */
    VnodeId	vparent;    	/* parent's id */
    Unique_t	uparent;    	/* uniquifier of parent */
    Unique_t  	unique;	    	/* own uniquifier; Must match entry! */
    VnodeId	vid;	    	/* own vnode number */
    rec_dlist	*log;		// for directories only
};

struct VnodeInfo {
    int 	nVnodes;    	/* Total number of vnodes in index */
    int 	nAllocatedVnodes;/* Total number actually used */
    int 	volumeBlockCount;/* Total number of blocks used by volume */
    Inode 	*inodes;	/* Directory only */
    struct VnodeEssence *vnodes;
};

struct DirSummary {
    struct DirHandle dirHandle;
    VnodeId 	vnodeNumber;
    Unique_t 	unique;
    unsigned 	haveDot, haveDotDot;
    VolumeId 	Vid;
    int 	copied;		/* If the copy-on-write stuff has been applied */
    VnodeId 	vparent;
    Unique_t  	uparent;
};


/* routines that get summaries */
PRIVATE int GetInodeSummary(char *fspath, char *path, 
			    VolumeId singleVolumeNumber);
PRIVATE int GetVolumeSummary(VolumeId singleVolumeNumber);
PRIVATE void DistilVnodeEssence(VnodeClass vclass, Inode indexInode);

/* the checking routines */
PRIVATE int SalvageFileSys(char *path, VolumeId singleVolumeNumber);
PRIVATE int SalvageVolumeGroup(struct VolumeSummary *vsp, int nVols);
PRIVATE int QuickCheck(struct VolumeSummary *vsp, int nVols);
PRIVATE int SalvageVolHead(register struct VolumeSummary *vsp);
#if 0
PRIVATE int SalvageHeader(register struct stuff *sp,
			struct InodeSummary *isp, int check, int *deleteMe);
#endif 
PRIVATE int VnodeInodeCheck(int, struct ViceInodeInfo *, int, struct VolumeSummary *);
PRIVATE void DirCompletenessCheck(struct VolumeSummary *vsp);
PRIVATE void JudgeEntry(struct DirSummary *dir, char *name,
		VnodeId vnodeNumber, Unique_t unique);
PRIVATE void SanityCheckFreeLists();

/* correcting/action routines */
PRIVATE int MaybeZapVolume(struct InodeSummary *isp,
			char *message, int deleteMe);
PRIVATE void CleanInodes(struct InodeSummary *);
PRIVATE void ClearROInUseBit(struct VolumeSummary *summary);
PRIVATE int CopyInode(Device device, Inode inode1, Inode inode2);
PRIVATE void FixInodeLinkcount(struct ViceInodeInfo *, struct InodeSummary *);
PRIVATE int DestroyBadVolumes();


/* misc routines */
extern long time(long *);
int OnlyOneVolume(struct ViceInodeInfo *, VolumeId);
int InSkipVolumeList(VolumeId, VolumeId *, int);
PRIVATE char *devName(unsigned int dev);
PRIVATE struct VnodeEssence *CheckVnodeNumber(VnodeId vnodeNumber, Unique_t);
PRIVATE int AskOffline(VolumeId volumeId);
PRIVATE int AskOnline(VolumeId volumeId);
PRIVATE void PrintInodeList();
PRIVATE void release_locks(int);
PRIVATE void GetSkipVolumeNumbers();
PRIVATE void zero_globals();
