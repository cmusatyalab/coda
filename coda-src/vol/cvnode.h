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

/********************************
 * cvnode.h			*
 ********************************/

#ifndef _CVNODE_H_
#define _CVNODE_H_ 1

#ifdef __cplusplus
extern "C" {
#endif

#include <lwp/lwp.h>
#include <lwp/lock.h>
#include <rpc2/errors.h>
#include <util.h>
#include <codadir.h>
#include <prs.h>
#include <al.h>

#ifdef __cplusplus
}
#endif

#include <voltypes.h>
#include <inconsist.h>
#include <rec_smolist.h>
#include <rec_dlist.h>
#include "vicelock.h"
#define ROOTVNODE 1

typedef int VnodeType;

/* more lock types -- leave small #s for lock.h */
#define TRY_READ_LOCK	16
#define TRY_WRITE_LOCK	32

/*typedef enum {vNull=0, vFile=1, vDirectory=2, vSymlink=3} VnodeType;*/
#define vNull 0
#define vFile 1
#define vDirectory 2
#define vSymlink 3

/*typedef enum {vLarge=0,vSmall=1} VnodeClass;*/
#define vLarge	0
#define vSmall	1

typedef int VnodeClass;
#define VNODECLASSWIDTH 1
#define VNODECLASSMASK	((1<<VNODECLASSWIDTH)-1)
#define nVNODECLASSES	(VNODECLASSMASK+1)

struct VnodeClassInfo {
    struct Vnode *lruHead;	/* Head of list of vnodes of this class */
    int diskSize;		/* size of vnode disk object, power of 2 */
    int residentSize;		/* resident size of vnode */
    int cacheSize;		/* Vnode cache size */
    bit32 magic;		/* Magic number for this type of vnode,
    				   for as long as we're using vnode magic
				   numbers */
    int	allocs;			/* Total number of successful allocation
    				   requests; this is the same as the number
				   of sanity checks on the vnode index */
    int gets,reads;		/* Number of VGetVnodes and corresponding
    				   reads */
    int writes;			/* Number of vnode writes */
};

extern struct VnodeClassInfo VnodeClassInfo_Array[nVNODECLASSES];

#define vnodeTypeToClass(type)  ((type) == vDirectory? vLarge: vSmall)
#define vnodeIdToClass(vnodeId) ((vnodeId-1)&VNODECLASSMASK)
#define vnodeIdToBitNumber(v) (((v)-1)>>VNODECLASSWIDTH)
#define bitNumberToVnodeNumber(b,vclass) (((b)<<VNODECLASSWIDTH)+(vclass)+1)
#define vnodeIsDirectory(vnodeNumber) (vnodeIdToClass(vnodeNumber) == vLarge)

/* VnodeDiskObject: Structure of vnode stored in RVM */
typedef struct VnodeDiskObjectStruct {
    VnodeType	  type:3;	/* Vnode is file, directory, symbolic link
    				   or not allocated */
    unsigned	  cloned:1;	/* This vnode was cloned--therefore the inode
    				   is copy-on-write; only set for directories*/
    unsigned	  modeBits:12;	/* Unix mode bits */
    bit16	  linkCount;	/* Number of directory references to vnode
    				   (from single directory only!) */
    bit32	  length;	/* Number of bytes in this file */
    Unique_t	  uniquifier;	/* Uniquifier for the vnode; assigned
				   from the volume uniquifier (actually
				   from nextVnodeUnique in the Volume
				   structure) */
    FileVersion   dataVersion;	/* version number of the data */
#define	NEWVNODEINODE -1	/* inode number for a vnode allocated 
                                   but not used for creation */
    Inode	  inodeNumber;	/* inode number of the data attached to
    				   this vnode */
    /* version vector is updated atomically with the data */
    vv_t	  versionvector;/* CODA file version vector for this vnode */
    int		  vol_index;	/* index of vnode's volume in recoverable volume array */
    Date_t	  unixModifyTime;/* set by user */
    UserId	  author;	/* Userid of the last user storing the file */
    UserId	  owner;	/* Userid of the user who created the file */
    VnodeId	  vparent;	/* Parent directory vnode */
    Unique_t	  uparent;	/* Parent directory uniquifier */
    bit32	  vnodeMagic;	/* Magic number--mainly for file server
    				   paranoia checks */
#define	  SMALLVNODEMAGIC	0xda8c041F
#define	  LARGEVNODEMAGIC	0xad8765fe
    /* Vnode magic can be removed, someday, if we run need the room.  Simply
       have to be sure that the thing we replace can be VNODEMAGIC, rather
       than 0 (in an old file system).  Or go through and zero the fields,
       when we notice a version change (the index version number) */
    ViceLock	  lock;		/* Advisory lock */
    Date_t	  serverModifyTime;	/* Used only by the server;
					   for incremental backup purposes */
    rec_smolink	  nextvn;	/* link to next vnode with same vnodeindex */
    rec_dlist	  *log;		/* resolution log in RVM */
    /* Missing:
       archiving/migration
       encryption key
     */
} VnodeDiskObject;

#define SIZEOF_SMALLDISKVNODE	112	/* used to be 64 */
#define CHECKSIZE_SMALLVNODE\
	(sizeof(VnodeDiskObject) == SIZEOF_SMALLDISKVNODE)
/* must be a power of 2! */
#define SIZEOF_LARGEDISKVNODE	512	/* used to be 256 */

typedef struct Vnode {
    struct	Vnode *hashNext;/* Next vnode on hash conflict chain */
    struct	Vnode *lruNext;	/* Less recently used vnode than this one */
    struct	Vnode *lruPrev;	/* More recently used vnode than this one */
				/* The lruNext, lruPrev fields are not
				   meaningful if the vnode is in use */
    bit16	hashIndex;	/* Hash table index */
    unsigned short changed:1;	/* 1 if the vnode has been changed */
    unsigned short delete_me:1;	/* 1 if the vnode should be deleted; in
    				 this case, changed must also be 1 */
    VnodeId	vnodeNumber;
    struct Volume
		*volumePtr;	/* Pointer to the volume containing this file*/
    PDCEntry    dh;             /* Directory cache handle (used for dirs) */
    int         dh_refc;        /* Refcount of this vnode to dh */
    byte	nUsers;		/* Number of lwp's who have done a VGetVnode */
    bit16	cacheCheck;	/* Must equal the value in the volume Header
    				   for the cache entry to be valid */
    struct	Lock lock;	/* Internal lock */
    PROCESS	writer;		/* Process id having write lock */
    VnodeDiskObject disk;	/* The actual disk data for the vnode */
} Vnode;

#define Vnode_vv(vptr)		((vptr)->disk.versionvector)

#define SIZEOF_LARGEVNODE \
	(sizeof(struct Vnode) - sizeof(VnodeDiskObject) + SIZEOF_LARGEDISKVNODE)
#define SIZEOF_SMALLVNODE	(sizeof (struct Vnode))

#define VVnodeDiskACL(v)     /* Only call this with large (dir) vnode!! */ \
	((AL_AccessList *) (((byte *)(v))+SIZEOF_SMALLDISKVNODE))
#define  VVnodeACL(vnp) (VVnodeDiskACL(&(vnp)->disk))

/* note that there is currently room between the end of the 
small vnode and the ACL because 
SIZOEOF_SMALLDISKVNODE > sizeof(VnodeDiskData)
*/

#define VAclSize(vnp)		(SIZEOF_LARGEVNODE - SIZEOF_SMALLVNODE)
#define VAclDiskSize(v)		(SIZEOF_LARGEDISKVNODE - SIZEOF_SMALLDISKVNODE)
#define VnLog(vnp)		((vnp)->disk.log)

PDirHandle SetDirHandle(struct Vnode *);
extern int VolumeHashOffset();
extern void VInitVnodes(VnodeClass, int);
extern Vnode *VGetVnode(Error *, Volume *, VnodeId, Unique_t, int, int, int =0);
extern void VPutVnode(Error *ec, Vnode *vnp);
extern void VFlushVnode(Error *, Vnode *);
extern int VAllocFid(Volume *vp, VnodeType type,
		      ViceFidRange *Range, int stride =1, int ix =0);
extern int VAllocFid(Volume *vp, VnodeType type, VnodeId vnode, Unique_t unique);
extern Vnode *VAllocVnode(Error *ec, Volume *vp, VnodeType type,
			   int stride =1, int ix =0);
extern Vnode *VAllocVnode(Error *ec, Volume *vp, VnodeType type,
			   VnodeId vnode, Unique_t unique);
extern int ObjectExists(int, int, VnodeId, Unique_t, ViceFid * =NULL);

int VN_DCommit(Vnode *vnp);
int VN_DAbort(Vnode *vnp);
PDirHandle VN_SetDirHandle(struct Vnode *vn);
void VN_PutDirHandle(struct Vnode *vn);
void VN_DropDirHandle(struct Vnode *vn);
void VN_CopyOnWrite(struct Vnode *vptr);

void VN_VN2Fid(struct Vnode *, struct Volume *, struct ViceFid *);
void VN_VN2PFid(struct Vnode *, struct Volume *, struct ViceFid *);

#endif /* _CVNODE_H_ */

