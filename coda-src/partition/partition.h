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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/partition/partition.h,v 1.2 1997/11/14 21:26:33 braam Exp $";
#endif /*_BLURB_*/

#ifndef PARTITION_INCLUDED
#define PARTITION_INCLUDED 1
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <fcntl.h>
#include <dirent.h>
#include <vcrcommon.h>
#include <voltypes.h>

#include <vicetab.h>
/* #include <cbitmap.h> */
#include <viceinode.h>

/* exported variables */
extern struct DiskPartition *DiskPartitionList;

typedef struct DiskPartition DiskPartition;

struct DiskPartition {
    struct DiskPartition *next;
    char  name[MAXPATHLEN];       /* Directory where partition can be found */
    char  devName[MAXPATHLEN];    /* Device mounted on name */
    Device	device;		  /* device number MUST be unique */
    int		lock_fd;	  /* lock fd if locked; otherwise -1;
    				   Not used by the file server */
    int		free;		  /* Total number of blocks (1K) presumed
				   available on this partition (accounting
				   for the minfree parameter for the
				   partition).  This is adjusted
				   approximately by the sizes of files
				   and directories read/written, and
				   periodically the superblock is read and
				   this is recomputed.  This number can
				   be negative, if the partition starts
				   out too full */
    int		totalUsable;	  /* Total number of blocks available on this
    				   partition, taking into account the minfree
				   parameter for the partition  The
				   superblock is re-read periodically by
				   VSetPartitionDiskUsage().) */
    int		minFree;	  /* Percentage to be kept free, as last read
    				   from the superblock */
    struct inodeops *ops;         /* methods to access partition */
    union PartitionData *d;       /* private data stored with the partition */
};

void InitPartitions(const char *tabfile);
void VInitPartition(Partent entry, struct inodeops *operations,
		    union PartitionData *data, Device devno);
struct DiskPartition *FindPartition(Device devno);
struct DiskPartition *VGetPartition(char *name);
void VSetPartitionDiskUsage(register struct DiskPartition *dp);
void VResetDiskUsage();
void VPrintDiskStats(FILE *fp);
void VLockPartition(char *name);
void VUnlockPartition(char *name);

#include <simpleifs.h>
#include <ftreeifs.h>

union PartitionData {
    struct part_ftree_opts ftree;
    struct part_simple_opts simple;
    /*    struct part_linuxext2_opts ext2;
    struct part_rawmach_opts mach; */
};

struct inodeops {
    Inode (*icreate) (struct DiskPartition *, Inode, u_long, u_long, 
		    u_long, u_long);
    int (*iopen)   (struct DiskPartition *, Inode, int);
    int (*iread)   (struct DiskPartition *, Inode inode_number, 
		    Inode parent_vol, 
		    int offset, char *buf, int count);
    int (*iwrite)  (struct DiskPartition *, Inode inode_number,
		    Inode  parent_vol, int  offset, char *buf, int count);
    int (*iinc)    (struct DiskPartition *, Inode  inode_number, 
		    Inode parent_vol);
    int (*idec)    (struct DiskPartition *, Inode inode_number, 
		    Inode parent_vol);
    int (*get_header)(struct DiskPartition *, struct i_header *header, 
		      Inode ino);
    int (*put_header)(struct DiskPartition *, struct i_header *header, 
		      Inode ino);
    int (*init)(union PartitionData **data, Partent partent, Device *dev);
    int (*magic)();
    int (*ListCodaInodes)(struct DiskPartition *, char *resultFile,
		       int (*judgeInode)(struct ViceInodeInfo*, VolumeId), 
		       int judgeParam);
};




extern struct inodeops inodeops_simple;
extern struct inodeops inodeops_ftree;
extern struct inodeops inodeops_backup;



#endif PARTITION_INCLUDED
