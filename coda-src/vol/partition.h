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

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/coda-src/vol/partition.h,v 1.1.1.1 1996/11/22 19:10:10 rvb Exp";
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


#ifndef _PARTITION_H_
#define _PARTITION_H_ 1

/* All Vice partitions on a server will have the following name prefix */
#define VICE_PARTITION_PREFIX	"/vicep"
#define VICE_PREFIX_SIZE	(sizeof(VICE_PARTITION_PREFIX)-1)

struct DiskPartition {
    struct DiskPartition *next;
    char	name[32];	/* Mounted partition name */
    char	devName[32];	/* Device mounted on */
    Device	device;		/* device number */
    int		lock_fd;	/* File descriptor of this partition if locked; otherwise -1;
    				   Not used by the file server */
    int		free;		/* Total number of blocks (1K) presumed
				   available on this partition (accounting
				   for the minfree parameter for the
				   partition).  This is adjusted
				   approximately by the sizes of files
				   and directories read/written, and
				   periodically the superblock is read and
				   this is recomputed.  This number can
				   be negative, if the partition starts
				   out too full */
    int		totalUsable;	/* Total number of blocks available on this
    				   partition, taking into account the minfree
				   parameter for the partition (see the
				   4.2bsd command tunefs, but note that the
				   bug mentioned there--that the superblock
				   is not reread--does not apply here.  The
				   superblock is re-read periodically by
				   VSetPartitionDiskUsage().) */
    int		minFree;	/* Percentage to be kept free, as last read
    				   from the superblock */
};

/* exported variables */
extern struct DiskPartition *DiskPartitionList;

/* exported routines */
extern void VInitPartition(char *path, char *devname, Device dev);
extern void VResetDiskUsage();
extern struct DiskPartition *VGetPartition(char *name);
extern void VLockPartition(char *name);
extern void VUnlockPartition(char *name);

#endif _PARTITION_H_
