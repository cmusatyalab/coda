#ifndef _BLURB_
#define _BLURB_
/*

            Coda: an Experimental Distributed File System
                             Release 3.1

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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/kernel-src/vfs/mach/Attic/cfsncstat.c,v 4.2 1997/02/26 16:04:39 rvb Exp $";
#endif /*_BLURB_*/


/* 
 * Mach Operating System
 * Copyright (c) 1990 Carnegie-Mellon University
 * Copyright (c) 1989 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */

/*
 * This code was written for the Coda file system at Carnegie Mellon University.
 * Contributers include David Steere, James Kistler, and M. Satyanarayanan.
 */

/*
 * HISTORY
 * cfsncstat.c,v
 * Revision 1.2  1996/01/02 16:57:23  bnoble
 * Added support for Coda MiniCache and raw inode calls (final commit)
 *
 * Revision 1.1.2.1  1995/12/20 01:57:50  bnoble
 * Added CFS-specific files
 *
 * Revision 3.1.1.1  1995/03/04  19:08:13  bnoble
 * Branch for BSD port revisions
 *
 * Revision 3.1  1995/03/04  19:08:12  bnoble
 * Bump to major revision 3 to prepare for BSD port
 *
 * Revision 2.1  1994/07/21  16:25:30  satya
 * Conversion to C++ 3.0; start of Coda Release 2.0
 *
 * Revision 1.3  93/05/28  16:24:35  bnoble
 * *** empty log message ***
 * 
 * Revision 1.2  92/10/27  17:58:37  lily
 * merge kernel/latest and alpha/src/cfs
 * 
 * Revision 1.1  92/04/03  17:35:37  satya
 * Initial revision
 * 
 * Revision 1.2  90/03/19  16:35:50  dcs
 * Changed Revision Log.
 * 
 */

/* 
 * This program will report on the current cfsnamecache statistics.
 * It obtains the stats by reading /dev/kmem.
 */

#include <nlist.h>
#include <sys/ioctl.h>
#include "cfs_opstats.h"
#ifdef KERNEL
#include <cfs/cfsio.h>
#else
#include "cfsio.h"
#endif KERNEL

struct cfsnc_statistics {
	unsigned	hits;
	unsigned	misses;
	unsigned	enters;
	unsigned	dbl_enters;
	unsigned	long_name_enters;
	unsigned	long_name_lookups;
	unsigned	long_remove;
	unsigned	lru_rm;
	unsigned	zapPfids;
	unsigned	zapFids;
	unsigned	zapFile;
	unsigned	zapUsers;
	unsigned	Flushes;
	unsigned        Sum_bucket_len;
	unsigned        Sum2_bucket_len;
	unsigned        Max_bucket_len;
	unsigned        Num_zero_len;
	unsigned        Search_len;
} cfsnc_stat;

struct cfs_op_stats cfs_vfsopstats[CFS_VFSOPS_SIZE];
struct cfs_op_stats cfs_vnodeopstats[CFS_VNODEOPS_SIZE];

char *vfsop_names[CFS_VFSOPS_SIZE] = {"Mount",
				      "Unmount",
				      "Root",
				      "Statfs",
				      "Sync",
				      "Vget"};

char *vnodeop_names[CFS_VNODEOPS_SIZE] = {"Open",
					  "Close",
					  "RdWr",
					  "Ioctl",
					  "Select",
					  "GetAttr",
					  "SetAttr",
					  "Access",
					  "ReadLink",
					  "Fsync",
					  "Inactive",
					  "Lookup",
					  "Create",
					  "Remove",
					  "Link",
					  "Rename",
					  "MkDir",
					  "RmDir",
					  "SymLink",
					  "ReadDir"};


print_op_stats(names, stats, size)
    char *names[];
    struct cfs_op_stats stats[];
    int size;
{
    int i;

    printf("\n");
    printf("%12s%12s%12s%12s%12s\n","Operation","Entries","Satisfied",
	   "Failed","Generated");
    printf("------------------------------------------------------------\n");
    
    for (i=0; i<size; i++) {
	printf("%11s:%12ld%12ld%12ld%12ld\n",
               names[i], stats[i].entries, stats[i].sat_intrn,
	       stats[i].unsat_intrn, stats[i].gen_intrn);
    }
    printf("\n");
}

print_cfsnc_stats()
{
    printf("\nSTATISTICS\n");
    printf("cfsnc_hits : %d\n", cfsnc_stat.hits);
    printf("cfsnc_misses : %d\n", cfsnc_stat.misses);
    printf("cfsnc_enters : %d\n", cfsnc_stat.enters);
    printf("cfsnc_dbl_enters : %d\n", cfsnc_stat.dbl_enters);
    printf("cfsnc_long_name_enters : %d\n", cfsnc_stat.long_name_enters);
    printf("cfsnc_long_name_lookups : %d\n", cfsnc_stat.long_name_lookups);
    printf("cfsnc_long_remove : %d\n", cfsnc_stat.long_remove);
    printf("cfsnc_lru_rm : %d\n", cfsnc_stat.lru_rm);
    printf("cfsnc_zapPfids : %d\n", cfsnc_stat.zapPfids);
    printf("cfsnc_zapFids : %d\n", cfsnc_stat.zapFids);
    printf("cfsnc_zapFile : %d\n", cfsnc_stat.zapFile);
    printf("cfsnc_zapUsers : %d\n", cfsnc_stat.zapUsers);
    printf("cfsnc_Flushes : %d\n", cfsnc_stat.Flushes);
    printf("cfsnc_SumLen : %d\n", cfsnc_stat.Sum_bucket_len);
    printf("cfsnc_Sum2Len : %d\n", cfsnc_stat.Sum2_bucket_len);
    printf("cfsnc_# 0 len : %d\n", cfsnc_stat.Num_zero_len);
    printf("cfsnc_MaxLen : %d\n", cfsnc_stat.Max_bucket_len);
    printf("cfsnc_SearchLen : %d\n", cfsnc_stat.Search_len);
}

main()
{
  struct nlist RawStats[6];
  int kmem;
  struct cfsnc_statistics *cfsnc_statptr = &cfsnc_stat;
  int fd, err;
  int cachesize, hashsize;

  fd = open("/dev/cfs1",0,0);
  if (fd < 0) {
    perror("Open /dev/cfs1");
    exit(-1);
  }

  err = ioctl(fd, CFSSTATS);
  if (err < 0) {
    perror("Ioctl /dev/cfs1");
  }

  close(fd);

  RawStats[0].n_name = "_cfsnc_stat";
  RawStats[1].n_name = "_cfsnc_size";
  RawStats[2].n_name = "_cfsnc_hashsize";
  RawStats[3].n_name = "_cfs_vfsopstats";
  RawStats[4].n_name = "_cfs_vnodeopstats";
  RawStats[5].n_name = 0;

  if (nlist("/vmunix",RawStats) == -1) {
    printf("-1 returned from nlist\n");
    exit(-1);
  }

  if (RawStats[0].n_type == 0) {
    printf("Could not find the symbol in the namelist in VMUNIX\n");
    exit(-1);
  }

  if (RawStats[4].n_type == 0) {
      printf("WARNING: Running pre-vfs-statistics kernel\n");
  }

  kmem = open("/dev/kmem",0,0);
  if (kmem <= 0) {
    perror("open /dev/kmem");
    exit(-1);
  }

  
  if ((lseek(kmem, (long)RawStats[1].n_value, 0)) &&
      (read(kmem, (char *)&cachesize, sizeof(int)) > 0)) {
    printf("Cache size : %d ",cachesize);
  } else {
    printf("Nothing Read!\n");
  }

  if ((lseek(kmem, (long)RawStats[2].n_value, 0)) &&
      (read(kmem, (char *)&hashsize, sizeof(int)) > 0)) {
    printf("Hash size : %d\n", hashsize);
  } else {
    printf("Nothing Read!\n");
  }

  lseek(kmem, (long)RawStats[0].n_value, 0);
  read(kmem, (char *)cfsnc_statptr, sizeof(struct cfsnc_statistics));

  lseek(kmem, (long)RawStats[3].n_value, 0);
  read(kmem, (char *)cfs_vfsopstats, 
       sizeof(struct cfs_op_stats)*CFS_VFSOPS_SIZE);

  lseek(kmem, (long)RawStats[4].n_value, 0);
  read(kmem, (char*)cfs_vnodeopstats,
       sizeof(struct cfs_op_stats)*CFS_VNODEOPS_SIZE);

  close(kmem);

  print_cfsnc_stats();
  print_op_stats(vfsop_names,cfs_vfsopstats,CFS_VFSOPS_SIZE);
  print_op_stats(vnodeop_names,cfs_vnodeopstats,CFS_VNODEOPS_SIZE);

}

  
