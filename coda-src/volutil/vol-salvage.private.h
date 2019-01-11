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

#define readOnly(vsp) \
    ((vsp)->header.type == ROVOL || (vsp)->header.type == BACKVOL)

#include <rec_dlist.h>
#include <bitmap.h>
#include <recov_vollog.h>

struct InodeSummary { /* Inode summary file: an entry for each volume in a partition */
    VolumeId volumeId; /* Volume id */
    VolumeId RWvolumeId; /* RW volume associated */
    int index; /* index into inode file (0, 1, 2 ...) */
    int nInodes; /* Number of inodes for this volume */
    Unique_t
        maxUniquifier; /* The max. uniquifier in all the inodes for this volume */
};

struct VolumeSummary { /* one entry for each volume */
    char *fileName; /* File name on the partition for the volume header */
    int volindex; /* volume's index in recoverable storage */
    struct VolumeHeader header; /* volume number, rw volume number */
    byte wouldNeedCallback; /* set if the file server should issue
				    call backs for all the files in this volume when
				    the volume goes back on line */
    struct InodeSummary *inSummary; /* this volume's inodes */
    recov_vol_log *vollog; // log storage for volume
    bitmap *logbm; // bitmap for checking if volume log has no leaks
};

struct VnodeEssence {
    short count; /* Number of references to vnode; MUST BE SIGNED */
    unsigned claimed : 1; /* Set when a parent directory containing an entry
				   referencing this vnode is found.  The claim
				   is that the parent in "parent" can point to
				   this vnode, and no other */
    unsigned changed : 1; /* Set if any parameters (other than the count)
				   in the vnode change.   It is determined if the
				   link count has changed by noting whether it is
				   0 after scanning all directories */
    unsigned short
        blockCount; /* Number of blocks (1K) used by this vnode, approximately */
    VnodeId vparent; /* parent's id */
    Unique_t uparent; /* uniquifier of parent */
    Unique_t unique; /* own uniquifier; Must match entry! */
    VnodeId vid; /* own vnode number */
    rec_dlist *log; // for directories only
};

struct VnodeInfo {
    int nVnodes; /* Total number of vnodes in index */
    int nAllocatedVnodes; /* Total number actually used */
    int volumeBlockCount; /* Total number of blocks used by volume */
    PDirInode *dirnodes; /* Directory only */
    struct VnodeEssence *vnodes;
};

struct DirSummary {
    PDCEntry dirCache;
    VnodeId vnodeNumber;
    Unique_t unique;
    unsigned haveDot, haveDotDot;
    VolumeId Vid;
    int copied; /* If the copy-on-write stuff has been applied */
    VnodeId vparent;
    Unique_t uparent;
};

/* routines that get summaries */
static int GetInodeSummary(char *fspath, char *path,
                           VolumeId singleVolumeNumber);
static int GetVolumeSummary(VolumeId singleVolumeNumber);

/* the checking routines */
static int SalvageFileSys(char *path, VolumeId singleVolumeNumber);
static int SalvageVolumeGroup(struct VolumeSummary *vsp, int nVols);
static int QuickCheck(struct VolumeSummary *vsp, int nVols);
static int SalvageVolHead(register struct VolumeSummary *vsp);
static int VnodeInodeCheck(int, struct ViceInodeInfo *, int,
                           struct VolumeSummary *);
static void DirCompletenessCheck(struct VolumeSummary *vsp);
static void SanityCheckFreeLists();

/* correcting/action routines */
static void CleanInodes(struct InodeSummary *);
static void ClearROInUseBit(struct VolumeSummary *summary);
static void FixInodeLinkcount(struct ViceInodeInfo *, struct InodeSummary *);
static int DestroyBadVolumes();

/* misc routines */
int OnlyOneVolume(struct ViceInodeInfo *, VolumeId);
int InSkipVolumeList(VolumeId, VolumeId *, int);
static struct VnodeEssence *CheckVnodeNumber(VnodeId vnodeNumber, Unique_t);
static int AskOffline(VolumeId volumeId);
static int AskOnline(VolumeId volumeId);
static void release_locks(int);
static void GetSkipVolumeNumbers();
static void zero_globals();
