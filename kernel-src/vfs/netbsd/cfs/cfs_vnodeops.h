/* 
 * Mach Operating System
 * Copyright (c) 1990 Carnegie-Mellon University
 * Copyright (c) 1989 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */

/*
 * This code was written for the Coda file system at Carnegie Mellon
 * University.  Contributers include David Steere, James Kistler, and
 * M. Satyanarayanan.  
 */

/*
 * HISTORY
 * $Log:	cfs_vnodeops.h,v $
 * Revision 1.2.34.2  97/11/20  11:46:54  rvb
 * Capture current cfs_venus
 * 
 * Revision 1.2.34.1  97/11/13  22:03:04  rvb
 * pass2 cfs_NetBSD.h mt
 * 
 * Revision 1.2  96/01/02  16:57:14  bnoble
 * Added support for Coda MiniCache and raw inode calls (final commit)
 * 
 * Revision 1.1.2.1  1995/12/20 01:57:40  bnoble
 * Added CFS-specific files
 *
 */


enum vcexcl	{ NONEXCL, EXCL};		/* (non)excl create (create) */


/* NetBSD interfaces to the vnodeops */
int cfs_open      __P((void *));
int cfs_close     __P((void *));
int cfs_read      __P((void *));
int cfs_write     __P((void *));
int cfs_ioctl     __P((void *));
int cfs_select    __P((void *));
int cfs_getattr   __P((void *));
int cfs_setattr   __P((void *));
int cfs_access    __P((void *));
int cfs_readlink  __P((void *));
int cfs_abortop   __P((void *));
int cfs_fsync     __P((void *));
int cfs_inactive  __P((void *));
int cfs_lookup    __P((void *));
int cfs_create    __P((void *));
int cfs_remove    __P((void *));
int cfs_link      __P((void *));
int cfs_rename    __P((void *));
int cfs_mkdir     __P((void *));
int cfs_rmdir     __P((void *));
int cfs_symlink   __P((void *));
int cfs_readdir   __P((void *));
int cfs_bmap      __P((void *));
int cfs_strategy  __P((void *));
int cfs_lock      __P((void *));
int cfs_unlock    __P((void *));
int cfs_islocked  __P((void *));
int nbsd_vop_error   __P((void *));
int nbsd_vop_nop     __P((void *));
int cfs_reclaim   __P((void *));

