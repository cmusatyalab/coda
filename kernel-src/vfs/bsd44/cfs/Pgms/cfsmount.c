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
 * $Log: cfsmount.c,v $
 * Revision 1.2  1996/01/02 16:57:17  bnoble
 * Added support for Coda MiniCache and raw inode calls (final commit)
 *
 * Revision 1.1.2.1  1995/12/20 01:57:44  bnoble
 * Added CFS-specific files
 *
 * Revision 3.1.1.1  1995/03/04  19:08:09  bnoble
 * Branch for NetBSD port revisions
 *
 * Revision 3.1  1995/03/04  19:08:08  bnoble
 * Bump to major revision 3 to prepare for NetBSD port
 *
 * Revision 2.1  1994/07/21  16:25:26  satya
 * Conversion to C++ 3.0; start of Coda Release 2.0
 *
 * Revision 1.2  92/10/27  17:58:33  lily
 * merge kernel/latest and alpha/src/cfs
 * 
 * Revision 2.2  90/07/05  11:27:00  mrt
 * 	Created
 * 	[90/07/05  10:14:37  mrt]
 * 
 * Revision 1.4  90/06/07  13:16:58  dcs
 * Changed procedure headings to oldstyle for Pmax port.
 * 
 * Revision 1.3  90/03/19  17:42:48  dcs
 * Eliminated MACH conditionals.
 * 
 * Revision 1.2  90/03/19  16:35:05  dcs
 * Changed Revision Log.
 * 
 */

#include <sys/syscall.h>
#include <sys/errno.h>
#include <libc.h>
#include <stdio.h>

/* Needed for MOUNT_CFS definition. */
#include <vfs/vfs.h>

main(argc, argv)
     int argc;
     char **argv;
{
    if (argc != 3) {
	printf("Usage: %s special directory\n", argv[0]);
	exit(-1);
    }

    if (syscall(SYS_vfsmount, MOUNT_CFS, argv[2], (caddr_t)0, argv[1]) < 0) {
	perror("vfsmount");
	exit(-1);
    }
}
