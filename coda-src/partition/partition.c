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
#endif

#ifdef __cplusplus
extern "C" {
#endif __cplusplus


#include <unistd.h>
#include <string.h>
#ifdef __linux__
#include <sys/vfs.h>
#endif
#ifdef  __BSD44__
#include <sys/param.h>
#include <sys/mount.h>
#endif
#include <sys/file.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>
#include <lwp.h>
#include <lock.h>
#include <util.h>
#include "partition.h"

/* operations on partitions not involving volumes
 * this depends on vicetab.h and inodeops.h
 */

struct DiskPartition *DiskPartitionList;

static struct inodeops *DP_InodeOpsByType(char *type);
/* InitPartitions reads the vicetab file. For each server partition it finds
   on the invoking host it initializes this partition with the correct
   method.  When the partition has been initialized successfully, it is
   inserted in the DiskPartitionList, a linked list of DiskPartitions.
   */
void DP_Init(const char *tabfile)
{
    char myname[256];
    int rc;
    Partent entry;
    FILE *tabhandle;
    struct inodeops *operations;
    union PartitionData *data;
    Device  devno;

    tabhandle = Partent_set(tabfile, "r");
    if ( !tabhandle ) {
	eprint("No file vicetab file %s found.\n", tabfile);
	CODA_ASSERT(0);
    }

    while ( (entry = Partent_get(tabhandle)) ) {
        if ( ! UtilHostEq(hostname(myname), Partent_host(entry)) ) {
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


        DP_InitPartition(entry, operations, data, devno);
    }
    Partent_end(tabhandle);

    /* log the status */
    DP_PrintStats(stdout);
}

void
DP_InitPartition(Partent entry, struct inodeops *operations,
	       union PartitionData *data, Device devno)
{
    struct DiskPartition *dp, *pp;

    /* allocate */
    dp = (struct DiskPartition *) malloc(sizeof (struct DiskPartition));
    if ( ! dp ) {
	eprint("Out of memory\n");
	CODA_ASSERT(0);
    }
    bzero(dp, sizeof(struct DiskPartition));

    /* Add it to the end.  Preserve order for printing. Check devno. */
    for (pp = DiskPartitionList; pp; pp = pp->next) {
	if ( pp->device == devno ) {
	    eprint("Device %d requested by partition %s in use by %s!\n",
		   devno, Partent_dir(entry), pp->name);
	    CODA_ASSERT(0);
	}
	if (!pp->next)
	    break;
    }
    if (pp)
	pp->next = dp;
    else
	DiskPartitionList = dp;
    dp->next = NULL;

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
    register struct DiskPartition *dp;

    for (dp = DiskPartitionList; dp; dp = dp->next) {
	if (dp->device == devno)
	    break;
    }
    if (dp == NULL) {
	LogMsg(0, VolDebugLevel, stdout,  
	       "FindPartition Couldn't find partition %d", devno);
    }
    return dp;	
}

struct DiskPartition *
DP_Get(char *name)
{
    register struct DiskPartition *dp;

    for (dp = DiskPartitionList; dp; dp = dp->next) {
	if (strcmp(dp->name, name) == 0)
	    break;
    }
    if (dp == NULL) {
	LogMsg(0, VolDebugLevel, stdout,  
	       "VGetPartition Couldn't find partition %s", name);
    }
    return dp;	
}


void 
DP_SetUsage(register struct DiskPartition *dp)
{
#if defined(__CYGWIN32__) || defined(DJGPP)
    dp->free = 10000000;  /* free blocks for non s-users */
    dp->totalUsable = 10000000; 
    dp->minFree = 10;
#else
    struct statfs fsbuf;
    int rc;
    long reserved_blocks;

    rc = statfs(dp->name, &fsbuf);
    if ( rc != 0 ) {
	eprint("Error in statfs of %s\n", dp->name);
	SystemError("");
	CODA_ASSERT( 0 );
    }
    
    reserved_blocks = fsbuf.f_bfree - fsbuf.f_bavail; /* reserved for s-user */
    dp->free = fsbuf.f_bavail;  /* free blocks for non s-users */
    dp->totalUsable = fsbuf.f_blocks - reserved_blocks; 
    dp->minFree = 100 * reserved_blocks / fsbuf.f_blocks;

#endif
}

void 
DP_ResetUsage() 
{
    struct DiskPartition *dp;
    for (dp = DiskPartitionList; dp; dp = dp->next) {
	DP_SetUsage(dp);
	LWP_DispatchProcess();
    }
}

void 
DP_PrintStats(FILE *fp) 
{
    struct DiskPartition *dp;
    for (dp = DiskPartitionList; dp; dp = dp->next) {
	if (dp->free >= 0)
	LogMsg(0, 0, fp,  
	       "Partition %s: %d available size (1K blocks, minfree=%d%%), %d free blocks.",
	       dp->name, dp->totalUsable, dp->minFree, dp->free);
	else
	LogMsg(0, 0, fp,  
	       "Partition %s: %d available size (1K blocks, minfree=%d%%), overallocated by %d blocks.",
	       dp->name, dp->totalUsable, dp->minFree, dp->free);
    }
}

void 
DP_LockPartition(char *name)
{
    register struct DiskPartition *dp = DP_Get(name);
    CODA_ASSERT(dp != NULL);
    if (dp->lock_fd == -1) {
	dp->lock_fd = open(dp->name, O_RDONLY, 0);
	CODA_ASSERT(dp->lock_fd != -1);
	CODA_ASSERT (flock(dp->lock_fd, LOCK_EX) == 0);
    }
}

void 
DP_UnlockPartition(char *name)
{
    register struct DiskPartition *dp = DP_Get(name);
    CODA_ASSERT(dp != NULL);
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

#if 0  
    if ( strcmp(type, "raw_Mach") ) {
        return &inodeops_raw_mach;
    }
#endif
    return NULL;
}
