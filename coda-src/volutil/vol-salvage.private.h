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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/volutil/vol-salvage.private.h,v 4.4 1998/08/31 12:23:50 braam Exp $";
#endif /*_BLURB_*/





#define	DONTSALVVOLS	"/vice/vol/skipsalvage"
#define readOnly(vsp)	((vsp)->header.type==ROVOL || (vsp)->header.type == BACKVOL)

#include <rec_dlist.h>
#include <bitmap.h>
#include <recov_vollog.h>

struct InodeSummary {		/* Inode summary file: an entry for each volume in a partition */
    VolumeId	volumeId;	/* Volume id */
    VolumeId	RWvolumeId;	/* RW volume associated */
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
    PDCEntry    dirCache;
    VnodeId 	vnodeNumber;
    Unique_t 	unique;
    unsigned 	haveDot, haveDotDot;
    VolumeId 	Vid;
    int 	copied;		/* If the copy-on-write stuff has been applied */
    VnodeId 	vparent;
    Unique_t  	uparent;
};


/* routines that get summaries */
static int GetInodeSummary(char *fspath, char *path, 
			    VolumeId singleVolumeNumber);
static int GetVolumeSummary(VolumeId singleVolumeNumber);
static void DistilVnodeEssence(VnodeClass vclass, Inode indexInode);

/* the checking routines */
static int SalvageFileSys(char *path, VolumeId singleVolumeNumber);
static int SalvageVolumeGroup(struct VolumeSummary *vsp, int nVols);
static int QuickCheck(struct VolumeSummary *vsp, int nVols);
static int SalvageVolHead(register struct VolumeSummary *vsp);
#if 0
static int SalvageHeader(register struct stuff *sp,
			struct InodeSummary *isp, int check, int *deleteMe);
#endif 
static int VnodeInodeCheck(int, struct ViceInodeInfo *, int, struct VolumeSummary *);
static void DirCompletenessCheck(struct VolumeSummary *vsp);
static void JudgeEntry(struct DirSummary *dir, char *name,
		VnodeId vnodeNumber, Unique_t unique);
static void SanityCheckFreeLists();

/* correcting/action routines */
static int MaybeZapVolume(struct InodeSummary *isp,
			char *message, int deleteMe);
static void CleanInodes(struct InodeSummary *);
static void ClearROInUseBit(struct VolumeSummary *summary);
static int CopyInode(Device device, Inode inode1, Inode inode2);
static void FixInodeLinkcount(struct ViceInodeInfo *, struct InodeSummary *);
static int DestroyBadVolumes();


/* misc routines */
extern long time(long *);
int OnlyOneVolume(struct ViceInodeInfo *, VolumeId);
int InSkipVolumeList(VolumeId, VolumeId *, int);
static char *devName(unsigned int dev);
static struct VnodeEssence *CheckVnodeNumber(VnodeId vnodeNumber, Unique_t);
static int AskOffline(VolumeId volumeId);
static int AskOnline(VolumeId volumeId);
static void PrintInodeList();
static void release_locks(int);
static void GetSkipVolumeNumbers();
static void zero_globals();
