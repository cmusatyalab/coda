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
 * $Log: cfs_vnodeops.h,v $
 * Revision 1.4  1998/01/23 11:53:49  rvb
 * Bring RVB_CFS1_1 to HEAD
 *
 * Revision 1.3.2.3  98/01/23  11:21:13  rvb
 * Sync with 2.2.5
 * 
 * Revision 1.3.2.2  97/12/16  12:40:20  rvb
 * Sync with 1.3
 * 
 * Revision 1.3.2.1  97/12/10  14:08:34  rvb
 * Fix O_ flags; check result in cfscall
 * 
 * Revision 1.3  97/12/05  10:39:25  rvb
 * Read CHANGES
 * 
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


/* NetBSD interfaces to the vnodeops */
int cfs_open      __P((void *));
int cfs_close     __P((void *));
int cfs_read      __P((void *));
int cfs_write     __P((void *));
int cfs_ioctl     __P((void *));
/* 1.3 int cfs_select    __P((void *));*/
int cfs_getattr   __P((void *));
int cfs_setattr   __P((void *));
int cfs_access    __P((void *));
int cfs_abortop   __P((void *));
int cfs_readlink  __P((void *));
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
int cfs_reclaim   __P((void *));
int cfs_lock      __P((void *));
int cfs_unlock    __P((void *));
int cfs_islocked  __P((void *));
int nbsd_vop_error   __P((void *));
int nbsd_vop_nop     __P((void *));
#ifdef __FreeBSD__
int fbsd_vnotsup  __P((void *ap));
#endif

int (**cfs_vnodeop_p)(void *);
int cfs_rdwr(struct vnode *vp, struct uio *uiop, enum uio_rw rw,
    int ioflag, struct ucred *cred, struct proc *p);



int cfs_grab_vnode(dev_t dev, ino_t ino, struct vnode **vpp);
void print_vattr(struct vattr *attr);
void print_cred(struct ucred *cred);
