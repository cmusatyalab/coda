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
 * cfsnc_resize.c,v
 * Revision 1.2  1996/01/02 16:57:22  bnoble
 * Added support for Coda MiniCache and raw inode calls (final commit)
 *
 * Revision 1.1.2.1  1995/12/20 01:57:48  bnoble
 * Added CFS-specific files
 *
 * Revision 3.1.1.1  1995/03/04  19:08:11  bnoble
 * Branch for BSD port revisions
 *
 * Revision 3.1  1995/03/04  19:08:11  bnoble
 * Bump to major revision 3 to prepare for BSD port
 *
 * Revision 2.1  1994/07/21  16:25:30  satya
 * Conversion to C++ 3.0; start of Coda Release 2.0
 *
 * Revision 1.2  92/10/27  17:58:36  lily
 * merge kernel/latest and alpha/src/cfs
 * 
 * Revision 2.2  90/07/05  11:27:11  mrt
 * 	Created for Coda file system
 * 	[90/07/05  10:15:39  mrt]
 * 
 * Revision 1.2  90/03/19  16:35:26  dcs
 * Changed Revision Log.
 * 
 */

/*
 * This file will send an ioctl to the cfs device driver (vcioctl in cfs_subr.c)
 * which will cause the cfs name cache to change it's size.
 */

#include <sys/ioctl.h>
#include <cfs/cfsio.h>
#include <stdio.h>
#include <stdlib.h>

void
main(argc, argv)
     int argc;
     char *argv[];
{
  struct cfs_resize data;

  int fd, err;

  if (argc < 3) {
    printf("Usage: cfsnc_resize <newheapsize> <newhashsize>\n");
    exit(-1);
  }

  data.heapsize = atoi(argv[1]);
  data.hashsize = atoi(argv[2]);

  if ((data.heapsize % 2) || (data.hashsize %2)) {
    printf("The sizes must be multiples of 2\n");
    exit(-1);
  }

  fd = open("/dev/cfs1",0,0);
  if (fd < 0) {
    perror("Open /dev/cfs1");
    exit(-1);
  }

  err = ioctl(fd, CFSRESIZE, &data);
  if (err < 0) {
    perror("Ioctl /dev/cfs1");
    exit(-1);
  }

  close(fd);
}




