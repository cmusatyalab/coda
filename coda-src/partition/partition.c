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

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <unistd.h>
#include "coda_string.h"
#include <sys/file.h>
#include "coda_flock.h"
#ifdef HAVE_SYS_STATVFS_H
#include <sys/statvfs.h>
#elif defined(HAVE_SYS_STATFS_H)
#include <sys/statfs.h>
#elif defined(HAVE_SYS_VFS_H)
#include <sys/vfs.h>
#elif defined(HAVE_SYS_MOUNT_H)
#include <sys/param.h>
#include <sys/mount.h>
#endif

#ifdef __cplusplus
}
#endif

#include <util.h>
#include <lwp/lwp.h>
#include <lwp/lock.h>
#include <util.h>
#include "partition.h"

static void DP_InitPartition(Partent entry, struct inodeops *operations,
	       union PartitionData *data, Device devno);

/* 
 * operations on partitions not involving volumes
 * this depends on vicetab.h and inodeops.h
 */

struct dllist_head DiskPartitionList;

static struct inodeops *DP_InodeOpsByType(char *type);
/* InitPartitions reads the vicetab file. For each server partition it finds
   on the invoking host it initializes this partition with the correct
   method.  When the partition has been initialized successfully, it is
   inserted in the DiskPartitionList, a linked list of DiskPartitions.
   */
void DP_Init(const char *tabfile, const char *hostname)
{
    int rc;
    Partent entry;
    FILE *tabhandle;
    struct inodeops *operations;
    union PartitionData *data;
    Device  devno, codadev;
    char host[MAXHOSTNAMELEN];

    if (!hostname) {
	gethostname(host, MAXHOSTNAMELEN);
	hostname = host;
    }

    codadev = 1;
    list_head_init(&DiskPartitionList);

    tabhandle = Partent_set(tabfile, "r");
    if ( !tabhandle ) {
	eprint("No file vicetab file %s found.\n", tabfile);
	CODA_ASSERT(0);
    }

    while ( (entry = Partent_get(tabhandle)) ) {
        if ( ! UtilHostEq(hostname, Partent_host(entry)) ) {
	    Partent_free(&entry);
	    continue;
	}

	operations = DP_InodeOpsByType(Partent_type(entry));
	if ( !operations ) {
	    eprint("Partition entry %s, %s has unknown type %s.\n",
		   Partent_host(entry), Partent_dir(entry), 
		   Partent_type(entry));
	    CODA_ASSERT(0);
	}

	if ( operations->init ) {
	    rc = operations->init(&data, entry, &devno);
	    if ( rc != 0 ) {
		eprint("Partition entry %s, %s had initialization error.\n");
		CODA_ASSERT(0);
	    }
	}

	/* the devno is written to RVM storage in the vnodes - 
	   whatever scheme for numbering partitions is used should 
	   take note of this */
        DP_InitPartition(entry, operations, data, devno);
	codadev++; 
    }
    Partent_end(tabhandle);

    /* log the status */
    DP_PrintStats(stdout);
}

static void
DP_InitPartition(Partent entry, struct inodeops *operations,
	       union PartitionData *data, Device devno)
{
    struct DiskPartition *dp, *pp;
    struct dllist_head *tmp;

    /* allocate */
    dp = (struct DiskPartition *) malloc(sizeof (struct DiskPartition));
    if ( ! dp ) {
	eprint("Out of memory\n");
	CODA_ASSERT(0);
    }
    memset(dp, 0, sizeof(struct DiskPartition));
    list_head_init(&dp->dp_chain);

    
    tmp = &DiskPartitionList;
    while ((tmp = tmp->next) != &DiskPartitionList) {
	    pp = list_entry(tmp, struct DiskPartition, dp_chain);
	    if ( pp->device == devno ) {
		    eprint("Device %d requested by partition %s used by %s!\n",
			   devno, Partent_dir(entry), pp->name);
		    CODA_ASSERT(0);
	    }
    }

    /* Add it to the end.  Preserve order for printing. Check devno. */
    list_add(&dp->dp_chain, DiskPartitionList.prev);
    /*  fill in the structure */
    strncpy(dp->name, Partent_dir(entry), MAXPATHLEN);
    dp->device = devno;
    dp->ops = operations;
    dp->d = data;

    DP_SetUsage(dp);
}

struct DiskPartition *
DP_Find(Device devno)
{
    struct DiskPartition *dp = NULL;
    struct dllist_head *tmp;

    tmp = &DiskPartitionList;
    while( (tmp = tmp->next) != &DiskPartitionList) {
	    dp = list_entry(tmp, struct DiskPartition, dp_chain);
	    if (dp->device == devno)
		    break;
    }
    if (dp == NULL) {
	    SLog(0, "FindPartition Couldn't find partition %d", devno);
    }
    return dp;	
}

struct DiskPartition *
DP_Get(char *name)
{
    struct DiskPartition *dp = NULL;
    struct dllist_head *tmp;

    tmp = &DiskPartitionList;
    dp = list_entry(tmp, struct DiskPartition, dp_chain);
    
    while((dp) && (strcmp(dp->name, name) != 0) &&
	  ((tmp = tmp->next) != &DiskPartitionList)) {
	    dp = list_entry(tmp, struct DiskPartition, dp_chain);
    }

    if ((strcmp(dp->name,name)) != 0)
	    dp = NULL;
    if (dp == NULL) {
	VLog(0, "VGetPartition Couldn't find partition %s", name);
    }
    return dp;	
}


void 
DP_SetUsage(struct DiskPartition *dp)
{
#if defined(HAVE_SYS_STATVFS_H)
    struct statvfs vfsbuf;
    int rc;
    int reserved_blocks, scale;

    rc = statvfs(dp->name, &vfsbuf);
    if ( rc != 0 ) {
	eprint("Error in statvfs of %s\n", dp->name);
	perror("");
	CODA_ASSERT( 0 );
    }
#elif defined(HAVE_STATFS)
    struct statfs vfsbuf;
    int rc;
    long reserved_blocks, scale;

    rc = statfs(dp->name, &vfsbuf);
    if ( rc != 0 ) {
	eprint("Error in statfs of %s\n", dp->name);
	perror("");
	CODA_ASSERT( 0 );
    }
#else
#error "Need statvfs or statfs"
#endif

    /* scale values to # of 512 byte blocks, further fixup is later-on */
    scale = vfsbuf.f_bsize / 512;
    reserved_blocks = vfsbuf.f_bfree-vfsbuf.f_bavail; /* reserved for s-user */
    dp->free = vfsbuf.f_bavail * scale;  /* free blocks for non s-users */
    dp->totalUsable = (vfsbuf.f_blocks - reserved_blocks) * scale; 
    dp->minFree = 100 * reserved_blocks / vfsbuf.f_blocks;

    /* and scale values to the expected 1K per block */
    dp->free /= 2;
    dp->totalUsable /= 2;
}

void 
DP_ResetUsage() 
{
    struct DiskPartition *dp;
    struct dllist_head *tmp;

    tmp = &DiskPartitionList;
    while( (tmp = tmp->next) != &DiskPartitionList) {
	    dp = list_entry(tmp, struct DiskPartition, dp_chain);
	    DP_SetUsage(dp);
	    LWP_DispatchProcess();
    }
}

void 
DP_PrintStats(FILE *fp) 
{
    struct DiskPartition *dp;
    struct dllist_head *tmp;

    tmp = &DiskPartitionList;
    while( (tmp = tmp->next) != &DiskPartitionList) {
	    dp = list_entry(tmp, struct DiskPartition, dp_chain);
	    if (dp->free >= 0)
		    SLog(0, "Partition %s: %dK available (minfree=%d%%), %dK free.",
			 dp->name, dp->totalUsable, dp->minFree, dp->free);
	    else
		    SLog(0, "Partition %s: %dK available (minfree=%d%%), overallocated %dK.",
			 dp->name, dp->totalUsable, dp->minFree, -dp->free);
    }
}

void 
DP_LockPartition(char *name)
{
    register struct DiskPartition *dp = DP_Get(name);
    CODA_ASSERT(dp != NULL);
    if (dp->lock_fd == -1) {
	/* Cannot writelock a directory using fcntl, disabling for now --JH */
#if 0
	dp->lock_fd = open(dp->name, O_RDONLY, 0);
	CODA_ASSERT(dp->lock_fd != -1);
	CODA_ASSERT (myflock(dp->lock_fd, MYFLOCK_EX, MYFLOCK_BL) == 0);
#endif
    }
}

void 
DP_UnlockPartition(char *name)
{
    register struct DiskPartition *dp = DP_Get(name);
    CODA_ASSERT(dp != NULL);
    if (dp->lock_fd != -1)
	close(dp->lock_fd);
    dp->lock_fd = -1;
}


static struct inodeops *DP_InodeOpsByType(char *type) 
{
  
    if ( strcmp(type, "simple") == 0  ) {
	return &inodeops_simple;
    }

    if( strcmp(type, "ftree") == 0 ) {
	return &inodeops_ftree;
    }

    if( strcmp(type, "backup") == 0 ) {
	return &inodeops_backup;
    }

    return NULL;
}
