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

#ifndef PARTITION_INCLUDED
#define PARTITION_INCLUDED 1
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <fcntl.h>
#include <dirent.h>

#include <dllist.h>
#include <vcrcommon.h>
#include <voltypes.h>
#include <lwp/lwp.h>
#include <lwp/lock.h>
#include <vicetab.h>
#include <viceinode.h>

/* exported variables */
extern struct dllist_head DiskPartitionList;

typedef struct DiskPartition DiskPartition;

struct DiskPartition {
    struct dllist_head dp_chain;
    char name[MAXPATHLEN]; /* Directory where partition can be found */
    char devName[MAXPATHLEN]; /* Device mounted on name */
    Device device; /* device number MUST be unique */
    int lock_fd; /* lock fd if locked; otherwise -1;
    				   Not used by the file server */
    unsigned long free; /* Total number of blocks (1K) presumed
				   available on this partition (accounting
				   for the minfree parameter for the
				   partition). This is adjusted
				   approximately by the sizes of files
				   and directories read/written, and
				   periodically the superblock is read and
				   this is recomputed. */
    unsigned long totalUsable; /* Total number of blocks available on this
    				   partition, taking into account the minfree
				   parameter for the partition  The
				   superblock is re-read periodically by
				   VSetPartitionDiskUsage().) */
    unsigned int minFree; /* Percentage to be kept free, as last read
    				   from the superblock (not used?) */
    struct inodeops *ops; /* methods to access partition */
    union PartitionData *d; /* private data stored with the partition */
};

void DP_Init(const char *tabfile, const char *hostname);
void DP_LockPartition(char *name);
void DP_UnlockPartition(char *name);

struct DiskPartition *DP_Find(Device devno);
struct DiskPartition *DP_Get(char *name);
void DP_SetUsage(struct DiskPartition *dp);
void DP_ResetUsage();
void DP_PrintStats(FILE *fp);

#include <simpleifs.h>
#include <ftreeifs.h>

union PartitionData {
    struct part_ftree_opts ftree;
    struct part_simple_opts simple;
    /*    struct part_linuxext2_opts ext2; */
};

struct inodeops {
    Inode (*icreate)(struct DiskPartition *, u_long, u_long, u_long, u_long);
    int (*iopen)(struct DiskPartition *, Inode, int);
    int (*iread)(struct DiskPartition *, Inode inode_number, Inode parent_vol,
                 int offset, char *buf, int count);
    int (*iwrite)(struct DiskPartition *, Inode inode_number, Inode parent_vol,
                  int offset, char *buf, int count);
    int (*iinc)(struct DiskPartition *, Inode inode_number, Inode parent_vol);
    int (*idec)(struct DiskPartition *, Inode inode_number, Inode parent_vol);
    int (*get_header)(struct DiskPartition *, struct i_header *header,
                      Inode ino);
    int (*put_header)(struct DiskPartition *, struct i_header *header,
                      Inode ino);
    int (*init)(union PartitionData **data, Partent partent, Device *dev);
    int (*magic)();
    int (*ListCodaInodes)(struct DiskPartition *, char *resultFile,
                          int (*judgeInode)(struct ViceInodeInfo *, VolumeId),
                          int judgeParam);
};

extern struct inodeops inodeops_simple;
extern struct inodeops inodeops_ftree;
extern struct inodeops inodeops_backup;

#endif /* PARTITION_INCLUDED */
