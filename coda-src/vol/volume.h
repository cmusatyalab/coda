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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/vol/volume.h,v 4.2 1997/09/05 12:45:18 braam Exp $";
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

#ifndef VOLUME_INCLUDED
#define VOLUME_INCLUDED 1
#include <recov_vollog.h>
#include <vice.h>
#include "voldefs.h"


#define VolumeWriteable(vp)		(V_type(vp)==readwriteVolume)
#define VolumeWriteable2(vol)		(vol.type == readwriteVolume)

#define FSTAG	84597		/* Unique tag for fileserver lwp rocks */

/* volume flags indicating which type of resolution is turned on */
#define VMRES	1
#define RVMRES	4

typedef bit32				FileOffset; /* Offset in this file */
typedef enum {fileServer, volumeUtility, salvager, fsUtility} ProgramType;

struct versionStamp {		/* Version stamp for critical volume files */
    bit32	magic;		/* Magic number */
    bit32	version;	/* Version number of this file, or software
    				   that created this file */
};

/* Magic numbers and version stamps for each type of file */
#define VOLUMEHEADERMAGIC	0x88a1bb3c
#define VOLUMEINFOMAGIC		0x78a1b2c5
/*#define SMALLINDEXMAGIC		0x99776655 */
/*#define LARGEINDEXMAGIC		0x88664433 */
#define	MOUNTMAGIC		0x9a8b7c6d
#define ACLMAGIC		0x88877712

#define VOLUMEHEADERVERSION	1
#define VOLUMEINFOVERSION	1
#define	SMALLINDEXVERSION	1
#define	LARGEINDEXVERSION	1
#define	MOUNTVERSION		1
#define ACLVERSION		1

#define MAXVOLS_PER_PARTITION  1000     /* Max number of volumes per partition */

/* Volume header.  This used to be the contents of the named file representing
   the volume; now stored in recoverable storage.  Read-only by the file server! */
struct VolumeHeader {
    struct versionStamp	stamp;/* Must be first field */
    VolumeId	id;	      /* Volume number */
    VolumeId	parent;	      /* Read-write volume number (or this volume if readwrite) */
    int		type;	  /* volume type: RWVOL, ROVOL, BACKVOL */
};


/* A vnode index file header */
struct IndexFileHeader {
    struct versionStamp stamp;
};


/*
VolumeDiskData:Volume administrative data stored in RVM.
*/
#define VNAMESIZE 32		/* maximum volume name length */
#define VPARTSIZE 32            /* maximum partition name */

typedef struct VolumeDiskData {
    struct versionStamp stamp;	/* Must be first field */
    VolumeId	id;		/* Volume id--unique over all systems */
    char	partition[VPARTSIZE]; /* filesystem partition on which
					 volume data resides */
    char	name[VNAMESIZE];/* Unofficial name for the volume */
    byte	inUse;		/* Volume is being used (perhaps it is online),
    				   or the system crashed while it was used */
    byte	inService;	/* Volume in service, not necessarily
				   on line This bit is set by an
				   operator/system programmer.
				   Manually taking a volume offline
				   always clears the inService
				   bit. Taking it out of service also
				   takes it offline */
    byte	blessed;	/* Volume is administratively blessed with
    				   the ability to go on line.  Set by a system
				   administrator. Clearing this bit will
				   take the volume offline */
    byte	needsSalvaged;	/* Volume needs salvaged--an
				   unrecoverable error occured to the
				   volume.  Note: a volume may still
				   require salvage even if this flag
				   isn't set--e.g. if a system crash
				   occurred while the volume was on
				   line. */
    bit32	uniquifier;	/* Next vnode uniquifier for this volume */
    int		type;		/* RWVOL, ROVOL, BACKVOL */
    VolId	parentId;	/* Id of parent, if type==readonly */
    VolId	groupId;	/* Id of replication group, or 0 if not replicated */
    VolId	cloneId;	/* Latest read-only clone, if
    				   type==readwrite, 0 if the volume
    				   has never been cloned.  Note: the
    				   indicated volume does not
    				   necessarily exist (it may have been
    				   deleted since cloning). */
    VolId	backupId;	/* Latest backup copy of this read
				   write volume */
    VolId	restoredFromId; /* The id in the dump this volume was
				   restored from--used simply to make
				   sure that an incremental dump is
				   not restored on top of something
				   inappropriate: Note: this field
				   itself is NEVER dumped!!! */
    byte	needsCallback;	/* Set by the salvager if anything was
				   changed about the volume.  Note:
				   this is not set by
				   clone/makebackups when setting the
				   copy-on-write flag in directories;
				   this flag is not seen by the
				   clients. */
#define DESTROY_ME	0xD3
    byte	destroyMe;	/* If this is set to DESTROY_ME, then
				   the salvager should destroy this
				   volume; it is bogus (left over from
				   an aborted volume move, for
				   example).  Note: if this flag is
				   on, then inService should be
				   OFF--only the salvager checks this
				   flag */
#define DONT_SALVAGE	0xE5
    byte	dontSalvage;	/* If this is on, then don't bother
				   salvaging this volume*/
    byte	reserveb3;

    vv_t	versionvector;	/* CODA version vector for this volume */
    int		ResOn;		/* Flag to turn on resolution */
    int		maxlogentries;	/* max number of entries allowed in log */
    bit32	reserved1[4];


    /* Administrative stuff */
    int		maxquota;	/* Quota maximum, 1K blocks */
    int		minquota;	/* Quota minimum, 1K blocks */
    int		maxfiles;	/* Maximum number of files (i.e. inodes) */
    bit32	accountNumber;  /* Uninterpreted account number */
    bit32	owner;		/* The person administratively responsible
				   for this volume */
    int		reserved2[8];	/* Other administrative constraints */

    /* Resource usage & statistics */
    int		filecount;	/* Actual number of files */
    bit16	linkcount;	/* number of links */
    int		diskused;	/* Actual disk space used, 1K blocks */
    int		dayUse;		/* Metric for today's usage of this
				   volume so far */
    int		weekUse[7];	/* Usage of the volume for the last
				   week.  weekUse[0] is for most
				   recent complete 24 hour period of
				   measurement; week[6] is 7 days ago */
    Date_t	dayUseDate;	/* Date the dayUse statistics refer
				   to; the week use stats are the
				   preceding 7 days */
    int		reserved3[11];	/* Other stats here */
    
    /* Server supplied dates */
    Date_t	creationDate;   /* Creation date for a read/write
    				   volume; cloning date for original
    				   copy of a readonly volume (readonly
    				   replicas volumes have the same
    				   creation date) */
    Date_t	accessDate;	/* Last access time by a user, large
				   granularity */
    Date_t	updateDate;	/* Last modification by user */
    Date_t	expirationDate; /* 0 if it never expires */
    Date_t	backupDate;	/* last time a backup clone was taken */

    /* Time that this copy of this volume was made.  NEVER backed up.
       This field is only set when the copy is created */
    Date_t	copyDate;
    recov_vol_log *log;		/* Recoverable resolution log for this
				   volume */
    bit32	reserved4[7];

    /* messages */
#define VMSGSIZE 128
    char	offlineMessage[VMSGSIZE]; /* Why the volume is offline */
    char 	motd[VMSGSIZE];	 	  /* Volume "message of the day" */
} VolumeDiskData;


/**************************************/
/* Memory resident volume information */
/**************************************/

/*
VM bitmap that shows which vnodes are allocated
 */
struct vnodeIndex {
      byte      *bitmap;	/* Index bitmap */
      unsigned short	bitmapSize;	/* length of bitmap, in bytes */
      unsigned short	bitmapOffset;	/* Which byte address of the
					   first long to start search
					   from in bitmap */
    };

/*
 * Since write locks on volumes set by the resolution subsystem have different
 * semantics than those set by the volume utility, we added the field
 * WriteLockType for differentiation. -- dcs 10/10/90
 */

/*
Lock used to ensure exclusive use of volume during resolution.
 */
typedef enum {Resolve = 0, VolUtil = 1} WriteLock_t;
struct ResVolLock {
    struct Lock VolumeLock;
    WriteLock_t WriteLockType;
    unsigned IPAddress;
};

/* VM structure maintained per volume */
struct Volume {
    struct	Volume 	*hashNext; /* Next in hash resolution table */
    VolumeId	hashid;		/* Volume number -- for hash table lookup */
    struct	volHeader *header; /* Cached disk data */
    Device	device;		/* Unix device for the volume */ 
    struct DiskPartition
    		*partition;	/* Information about the Unix partition */
    int		vol_index;	/* index of this volume in recoverable volume array */
    struct	vnodeIndex vnIndex[nVNODECLASSES];
    Unique_t	nextVnodeUnique;/* Derived originally from volume uniquifier.
			   	   This is the actual next version number to
			   	   assign; the uniquifier is bumped by 50 and
			   	   and written to disk every 50 file creates
			   	   If the volume is shutdown gracefully, the
				   uniquifier should be rewritten with the
				   value nextVnodeVersion*/
    bit16	vnodeHashOffset;/* Computed by HashOffset function in cvnode.h.
				   Assigned to the volume when initialized. 
				   Added to vnode number for hash table index */
    byte	shuttingDown;	/* This volume is going to be detached */
    byte	goingOffline;	/* This volume is going offline */
    bit16	cacheCheck;	/* Online sequence number to be used
				   to invalidate vnode cache entries
				   that stayed around while a volume
				   was offline */
    short	nUsers;		/* Number of users of this volume header */
    byte	specialStatus;	/* An error code to return on VGetVolume: the
				   volume is unavailable for the reason quoted,
				   currently VBUSY or VMOVED */
    long	updateTime;	/* Time that this volume was put on the updated
				   volume list--the list of volumes that will be
				   salvaged should the file server crash */
    struct	Lock lock;	/* internal lock */
    PROCESS	writer;		/* process id having write lock */
    struct  ResVolLock VolLock;	/* Volume level Lock for resolution/repair */
#define VNREINTEGRATORS 8	/* List size increment */
    int		nReintegrators;	/* Number of clients that have successfully
				   reintegrated with this volume. */
    ViceStoreId	*reintegrators;	/* List of identifiers representing the last
				   record reintegrated for each client. Could
				   be moved to recoverable store if necessary. */
};
typedef struct Volume Volume;

/*
  Cached version of the volume's administrative data</strong></a> 
*/
struct volHeader {
    struct volHeader *prev, *next;/* LRU pointers */
    VolumeDiskData diskstuff;	/* General volume info read from disk */
    Volume *back;		/* back pointer to current volume structure */
};

/* These macros are used to export fields within the volume header.
   This was added to facilitate changing the actual representation */

#define V_device(vp)		((vp)->device)
#define V_partition(vp)		((vp)->partition)
#define V_inode(vp)		((vp)->inode)
/*#define V_diskDataInode(vp)	((vp)->diskDataInode) */
#define V_vnodeIndex(vp)	((vp)->vnIndex)
#define V_nextVnodeUnique(vp)	((vp)->nextVnodeUnique)
#define V_volumeindex(vp)	((vp)->vol_index)
#define V_lock(vp)		((vp)->lock)
#define V_writer(vp)		((vp)->writer)
#define	V_VolLock(vp)		(((vp)->VolLock))

/* N.B. V_id must be this, rather than vp->id, or some programs will
   break, probably */
#define V_stamp(vp)		((vp)->header->diskstuff.stamp)
#define V_partname(vp)		((vp)->header->diskstuff.partition)
#define V_id(vp)		((vp)->header->diskstuff.id)
#define V_name(vp)		((vp)->header->diskstuff.name)
#define V_inUse(vp)		((vp)->header->diskstuff.inUse)
#define V_inService(vp)		((vp)->header->diskstuff.inService)
#define V_blessed(vp)		((vp)->header->diskstuff.blessed)
#define V_needsSalvaged(vp)	((vp)->header->diskstuff.needsSalvaged)
#define V_uniquifier(vp)	((vp)->header->diskstuff.uniquifier)
#define V_type(vp)		((vp)->header->diskstuff.type)
#define V_parentId(vp)		((vp)->header->diskstuff.parentId)
#define V_groupId(vp)		((vp)->header->diskstuff.groupId)
#define V_cloneId(vp)		((vp)->header->diskstuff.cloneId)
#define V_backupId(vp)		((vp)->header->diskstuff.backupId)
#define V_restoredFromId(vp)	((vp)->header->diskstuff.restoredFromId)
#define V_needsCallback(vp)	((vp)->header->diskstuff.needsCallback)
#define V_destroyMe(vp)		((vp)->header->diskstuff.destroyMe)
#define V_versionvector(vp)	((vp)->header->diskstuff.versionvector)
#define V_dontSalvage(vp)	((vp)->header->diskstuff.dontSalvage)
#define V_maxquota(vp)		((vp)->header->diskstuff.maxquota)
#define V_minquota(vp)		((vp)->header->diskstuff.minquota)
#define V_maxfiles(vp)		((vp)->header->diskstuff.maxfiles)
#define V_accountNumber(vp)	((vp)->header->diskstuff.accountNumber)
#define V_owner(vp)		((vp)->header->diskstuff.owner)
#define V_filecount(vp)		((vp)->header->diskstuff.filecount)
#define V_linkcount(vp)		((vp)->header->diskstuff.linkcount)
#define V_diskused(vp)		((vp)->header->diskstuff.diskused)
#define V_dayUse(vp)		((vp)->header->diskstuff.dayUse)
#define V_weekUse(vp)		((vp)->header->diskstuff.weekUse)
#define V_dayUseDate(vp)	((vp)->header->diskstuff.dayUseDate)
#define V_creationDate(vp)	((vp)->header->diskstuff.creationDate)
#define V_accessDate(vp)	((vp)->header->diskstuff.accessDate)
#define V_updateDate(vp)	((vp)->header->diskstuff.updateDate)
#define V_expirationDate(vp)	((vp)->header->diskstuff.expirationDate)
#define V_backupDate(vp)	((vp)->header->diskstuff.backupDate)
#define V_copyDate(vp)		((vp)->header->diskstuff.copyDate)
#define V_offlineMessage(vp)	((vp)->header->diskstuff.offlineMessage)
#define V_motd(vp)		((vp)->header->diskstuff.motd)
#define V_disk(vp)		((vp)->header->diskstuff)
#define V_VMResOn(vp)		((vp)->header->diskstuff.ResOn & VMRES)
#define V_RVMResOn(vp)		((vp)->header->diskstuff.ResOn & RVMRES)
#define V_maxlogentries(vp)	((vp)->header->diskstuff.maxlogentries)
#define V_VolLog(vp)		((vp)->header->diskstuff.log)

/* File offset computations.  The offset values in the volume header are
   computed with these macros -- when the file is written only!! */
#define VOLUME_MOUNT_TABLE_OFFSET(Volume)	(sizeof (VolumeDiskData))
#define VOLUME_BITMAP_OFFSET(Volume)	\
	(sizeof (VolumeDiskData) + (Volume)->disk.mountTableSize)


extern char *ThisHost;		/* This machine's hostname */
extern int ThisServerId;	/* this server id, as found in
				   /vice/db/servers */
extern bit32 HostAddress[];	/* Assume host addresses are 32 bits */
extern int VInit;		/* Set to 1 when the volume package is
				   initialized */
extern int HInit;		/* Set to 1 when the volid hash table
				   is initialized */
extern char *VSalvageMessage;   /* Common message used when the volume
				   goes off line */
extern int VolDebugLevel;	/* Controls level of debugging information */
extern char *VSalvageMessage;	/* Canonical message when a volume is forced
				   offline */
extern int AllowResolution;	/* global flag to turn on dir. resolution */
extern void VInitVolumePackage(int nLargeVnodes, int nSmallVnodes, int DoSalvage);
extern int VInitVolUtil(ProgramType pt);
extern void VInitServerList();
extern int VConnectFS();
extern void VDisconnectFS();
extern void VCheckVolumes();
extern void VUCloneVolume(Error *, Volume *, Volume *);
extern void VListVolumes();
extern void VGetVolumeInfo(Error *ec, char *key, register VolumeInfo *info);
extern Volume * VGetVolume(Error *ec, VolId volumeId);
extern void VPutVolume(Volume *vp);
extern Volume * VAttachVolume(Error *ec, VolumeId volumeId, int mode);
extern void VDetachVolume(Error *ec, Volume *vp);
extern void VUpdateVolume(Error *ec,Volume *vp);
extern int VAllocBitmapEntry(Error *ec, Volume *vp, struct vnodeIndex *index,
			      int stride, int ix, int count);
extern int VAllocBitmapEntry(Error *ec, Volume *vp, struct vnodeIndex *index, VnodeId vnode);
extern void VFreeBitMapEntry(Error *ec, struct vnodeIndex *index, int bitNumber);
extern int VolumeNumber(char *name);
extern char * VolumeExternalName(VolumeId volumeId);
extern Volume * VAttachVolumeById(Error *ec, char *partition, VolumeId volid, int mode);
extern void VOffline(Volume *vp, char *message);
extern void VForceOffline(Volume *vp);
extern void VPurgeVolume(Volume *vp);
extern void VShutdown();
extern void VSetDiskUsage();
extern void SetVolDebugLevel(int);
extern void FreeVolume(Volume *vp);
extern void DeleteVolumeFromHashTable(Volume *vp);
extern void PrintVolumesInHashTable();
extern void InitLRU(int howmany);

/* Naive formula relating number of file size to number of 1K blocks in file */
/* Note:  we charge 1 block for 0 length files so the user can't store
   an inifite number of them; for most files, we give him the inode, vnode,
   and indirect block overhead, for FREE! */
#define nBlocks(bytes) ((bytes) == 0? 1: ((bytes)+1023)/1024)

/* Client process id -- file server sends a Check volumes signal back
   to the client at this pid */
#define CLIENTPID	"/vice/vol/clientpid"

/* Modes of attachment, for VAttachVolume[ByName] to convey to the
   file server */
#define	V_READONLY 1	/* Absolutely no updates will be done to the volume */
#define V_CLONE	   2	/* Cloning the volume: if it is read/write,
			   then directory version numbers will change.
			   Header will be updated.  If the volume is
			   read-only, the file server may continue to
			   server it; it may also continue to server
			   it in read/write mode if the writes are
			   deferred */
#define V_UPDATE   3	/* General update or volume purge is possible.
			   Volume must go offline */
#define V_DUMP	   4	/* A dump of the volume is requested; the
			   volume can be served read-only during this
			   time */
#define V_SECRETLY 5	/* Secret attach of the volume.  This is used
			   to attach a volume which the file server
			   doesn't know about--and which it shouldn't
			   know about yet, since the volume has just
			   been created and is somewhat bogus.
			   Required to make sure that a file server
			   never knows about more than one copy of the
			   same volume--when a volume is moved from
			   one partition to another on a single server */

/* moved from vice/file.h to remove circular dependency */
typedef struct DirHandle {
    /* device+inode+vid are low level disk addressing + validity check */
    /* vid+vnode+unique+cacheCheck are to guarantee validity of cached copy */
    /* ***NOTE*** size of this stucture must not exceed size in buffer
       package (dir/buffer.cc) */
    bit16	device;
    bit16 	cacheCheck;
    Inode	inode;
    VolumeId 	volume;
    Unique_t 	unique;
    VnodeId	vnode;	/* Not really needed; conservative AND
			   protects us against non-unique uniquifiers
			   that were generated in days of old */
} DirHandle;


/* partition disk usage related routines */

/* exported routines */
void VAdjustDiskUsage(Error *ec, Volume *vp, int blocks);
void VCheckDiskUsage(Error *ec, Volume *vp, int blocks);
void VGetPartitionStatus(Volume *vp, int *totalBlocks, int *freeBlocks);


#endif VOLUME_INCLUDED
