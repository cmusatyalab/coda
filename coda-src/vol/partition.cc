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

static char *rcsid = "$Header: /afs/cs.cmu.edu/project/coda-braam/ss/coda-src/vol/RCS/partition.cc,v 1.2 1996/12/07 18:28:43 braam Exp braam $";
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

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <ctype.h>
#include <sys/param.h>
#ifdef __MACH__
#include <sys/fs.h>
#endif __MACH__
#include <sys/errno.h>
#include <sys/stat.h>
#include <stdio.h>
#include <sys/file.h>
#include <sys/dir.h>
#ifndef LINUX
#include <fstab.h>
#endif
#ifdef __MACH__
#include <libc.h>
#include <sysent.h>
#endif __MACH__
#if __NetBSD__ || LINUX
#include <unistd.h>
#include <stdlib.h>
#endif __NetBSD__

#ifdef LINUX
#include <sys/vfs.h>
#endif

#include <lwp.h>
#include <lock.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>
#include <srv.h>
#include <vice.h>
#include "cvnode.h"
#include "volume.h"
#include "partition.h"
#include "vutil.h"

struct DiskPartition *DiskPartitionList;


PRIVATE void VSetPartitionDiskUsage(register struct DiskPartition *dp);
PRIVATE void VGetPartitionStatus(Volume *vp, int *totalBlocks, int *freeBlocks);


void VInitPartition(char *path, char *devname, Device dev)
{
    struct DiskPartition *dp, *op;
    dp = (struct DiskPartition *) malloc(sizeof (struct DiskPartition));
    /* Add it to the end, to preserve order when we print statistics */
    for (op = DiskPartitionList; op; op = op->next) {
	if (!op->next)
	    break;
    }
    if (op)
	op->next = dp;
    else
	DiskPartitionList = dp;
    dp->next = 0;
    strcpy(dp->name, path);
    strcpy(dp->devName, devname);
    dp->device = dev;
    dp->lock_fd = -1;
    VSetPartitionDiskUsage(dp);
}


struct DiskPartition *VGetPartition(char *name)
{
    register struct DiskPartition *dp;
    LogMsg(9, VolDebugLevel, stdout,  "Entering VGetPartition(%s)", name);
    for (dp = DiskPartitionList; dp; dp = dp->next) {
	if (strcmp(dp->name, name) == 0)
	    break;
    }
    if (dp == NULL)
	LogMsg(0, VolDebugLevel, stdout,  "VGetPartition Couldn't find partition %s", name);

    return dp;		/* Return null if name wasn't found.*/
}

PRIVATE void VSetPartitionDiskUsage(register struct DiskPartition *dp)
{
#ifdef LINUX
  struct statfs fsbuf;
  int rc;
#endif
#ifdef __MACH__
    /* Note:  we don't bother syncing because it's only an estimate, update
       is syncing every 30 seconds anyway, we only have to keep the disk
       approximately 10% from full--you just can't get the stuff in from
       the net fast enough to worry */
    static struct fs sblock;/*Static because of constraints on lwp proc size*/
    int fd, totalblks, free, used, availblks;
    fd = open(dp->devName, O_RDONLY, 0);
    if (fd < 0) perror("VSetPartitionDiskUsage: open failed\n");
/*    assert((fd = open(dp->devName, O_RDONLY, 0)) >= 0); */
    assert(fd >= 0);
    assert(lseek(fd, dbtob(SBLOCK), L_SET) >= 0);
    assert(read(fd, (char *)&sblock, (int)sizeof(sblock)) == sizeof(sblock));
    close(fd);
    /* Cribbed from df.c */
    totalblks = (int)sblock.fs_dsize;
    free = (int)(sblock.fs_cstotal.cs_nbfree * sblock.fs_frag +
	sblock.fs_cstotal.cs_nffree);
    used = totalblks - free;
    availblks = totalblks * (100 - (int)sblock.fs_minfree) / 100;

    dp->minFree = (int)sblock.fs_minfree;
    dp->totalUsable = availblks;
    dp->free = availblks - used; /* May be negative, which is OK */
#endif

#ifdef LINUX
 rc = statfs(dp->devName, &fsbuf);
 assert( rc == 0 );
 
 dp->free = fsbuf.f_bavail;  /* available free blocsk */
 dp->totalUsable = fsbuf.f_blocks * 9 /10; 
 dp->minFree = 10;

#else
    /* Satya (8/5/96): skipped porting this routine since it is not
    		used by Venus; needs to be ported for server; depends on sys/fs.h in Mach */
    LogMsg(0, VolDebugLevel, stdout,  "PORTING ERROR: VSetPartitionDiskUsage() not yet ported");
    assert(0); /* die horribly */
#endif __MACH__

}

void VResetDiskUsage() {
    struct DiskPartition *dp;
    for (dp = DiskPartitionList; dp; dp = dp->next) {
	VSetPartitionDiskUsage(dp);
	LWP_DispatchProcess();
    }
}

void VAdjustDiskUsage(Error *ec, Volume *vp, int blocks)
{
    *ec = 0;
    if (blocks > 0) {
	if (vp->partition->free - blocks < 0)
	    *ec = VDISKFULL;
	/* This check doesn't work because a file that goes over the
	 * quota gets zeroes out.  This is because venus does not
	 * detect the error until close() is called and most programs
	 * don't check the return of close.
	 * Solution:  Just check that we are currently under the quota
	 *	      rather than if we will go over the quota,
	 * 	      programs will detect we have exceeded our quota
	 *	      at open().
	 * else if (V_maxquota(vp) && V_diskused(vp) + blocks > V_maxquota(vp))
	 */
	else if (V_maxquota(vp) && (V_diskused(vp) >= V_maxquota(vp)))
	    *ec = VOVERQUOTA;
    }    
    vp->partition->free -= blocks;
    V_diskused(vp) += blocks;
}

void VCheckDiskUsage(Error *ec, Volume *vp, int blocks)
{
    *ec = 0;
    if (blocks > 0){
	if (vp->partition->free - blocks < 0)
	    *ec = VDISKFULL;
	/* See quota comment in VAdjustDiskUsage
	 *
	 * else if (V_maxquota(vp) && (V_diskused(vp) + blocks > V_maxquota(vp)))
	 */
	else if (V_maxquota(vp) && (V_diskused(vp) >= V_maxquota(vp)))	
	    *ec = VOVERQUOTA;
    }
}

PRIVATE void VGetPartitionStatus(Volume *vp, int *totalBlocks, int *freeBlocks)
{
    *totalBlocks = vp->partition->totalUsable;
    *freeBlocks = vp->partition->free;
}

void VPrintDiskStats(FILE *fp) {
    struct DiskPartition *dp;
    for (dp = DiskPartitionList; dp; dp = dp->next) {
	LogMsg(0, 0, fp,  
	       "Partition %s: %d available 1K blocks (minfree=%d%%), ",
	       dp->name, dp->totalUsable, dp->minFree);
	if (dp->free < 0)
	    LogMsg(0, 0, fp, 
		   "overallocated by %d blocks", -dp->free);
	else
	    LogMsg(0, 0, fp,  
		   "%d free blocks", dp->free);
    }
}

void VLockPartition(char *name)
{
    register struct DiskPartition *dp = VGetPartition(name);
    assert(dp != NULL);
    if (dp->lock_fd == -1) {
	dp->lock_fd = open(dp->name, O_RDONLY, 0);
	assert(dp->lock_fd != -1);
	assert (flock(dp->lock_fd, LOCK_EX) == 0);
    }
}

void VUnlockPartition(char *name)
{
    register struct DiskPartition *dp = VGetPartition(name);
    assert(dp != NULL);
    close(dp->lock_fd);
    dp->lock_fd = -1;
}
