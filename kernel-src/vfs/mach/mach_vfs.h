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

static char *rcsid = "$Header: /usr/rvb/XX/src/kernel-src/vfs/mach/RCS/mach_vfs.h,v 4.1 1997/01/08 21:53:32 rvb Exp $";
#endif /*_BLURB_*/


/* 
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * Copyright (c) 1988 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * $Log: mach_vfs.h,v $
 * Revision 4.1  1997/01/08 21:53:32  rvb
 * r = 4.1; fix $ HEADERS
 *
 * Revision 1.2  1996/12/09 19:17:44  braam
 * changes to includes
 *
 * Revision 1.1  1996/11/22 19:09:37  braam
 * First Checkin (pre-release)
 *
 * Revision 1.1  96/11/22  13:30:21  raiff
 * First Checkin (pre-release)
 * 
 * Revision 3.3.1.1  96/08/26  12:39:01  raiff
 * Branch for release beta-26Aug1996_41240
 * 
 * Revision 3.3  96/08/23  19:23:55  satya
 * vfs.h and vnode.h should come from /usr/include/sys on BSD.
 * Added #ifdef __MACH__ around code.  Also added bogus code surrounded
 * by #ifdef __BSD44__ to trap if these versions of vfs.h and vnode.h 
 * are included on BSD.
 * 
 * Revision 3.2.4.1  96/07/24  11:17:29  raiff
 * Branch for release beta-24Jul1996_36690
 * 
 * Revision 3.2  95/10/09  19:33:56  satya
 * Reblurbed with new CMU and IBM notices for SOSP-15 CD-ROM
 * 
 * Revision 3.1  95/06/08  16:11:37  satya
 * *** empty log message ***
 * 
 * Revision 2.1.7.1  95/05/11  11:40:35  raiff
 * Branch for release beta-11May1995_36561
 * 
 * Revision 2.1  94/07/21  16:50:16  satya
 * Conversion to C++ 3.0; start of Coda Release 2.0
 * 
 * Revision 1.1  92/04/03  17:41:56  satya
 * Initial revision
 * 
 * Revision 1.1  90/03/27  07:56:36  satya
 * Initial revision
 * 
 * Revision 2.6  89/08/02  08:11:14  jsb
 * 	Eliminated MACH conditionals, and freefid macro.
 * 	[89/07/31  16:02:03  jsb]
 * 
 * Revision 2.5  89/04/22  15:34:00  gm0w
 * 	Added macros for manipulating inode, filesystems and credentials.
 * 	[89/04/14            gm0w]
 * 
 * Revision 2.4  89/03/09  22:44:05  rpd
 * 	More cleanup.
 * 
 * Revision 2.3  89/02/26  11:12:42  gm0w
 * 	Changes for cleanup.
 * 
 * Revision 2.2  89/01/18  01:24:37  jsb
 * 	Declare rootvp and vattr_null;
 * 	redefine freefid to use kfree instead of kalloc.
 * 	[89/01/17  15:54:26  jsb]
 * 
 */
/*	@(#)vfs.h 1.1 86/09/25 SMI	*/

#ifndef	_VFS_VFS_H_
#define	_VFS_VFS_H_

/* The files vfs/vfs.h and vfs/vnode.h should only get included in Mach; on 
   BSD44 these definitions should be found in sys/mount.h and sys/vnode.h
   provided by the system.  The use of "#ifdef __MACH__" and "#ifdef __BSD44__"
   is only due to paranoia... (Satya, 8/9/96)
*/
#ifdef __BSD44__

xxxxx    /* generate a compiler error, so we know we are including this file
	when shouldn't */
#endif /* __BSD44__ */

#ifdef __MACH__
#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>

#ifdef __MACH__
#ifndef	vnode
#include <sys/inode.h>
#endif	vnode
#endif __MACH__

#ifdef __cplusplus
}
#endif __cplusplus


/* buf.h */
#define	b_vp		b_ip

/* dir.h */
#if 0
#define	d_fileno	d_ino
#endif
/* file.h */
#define	f_cred		f_identity

/* mount.h */

/*
 * mount filesystem type switch table
 */
extern struct vfsops *vfssw[];

/*
 * mount options
 */
#define M_RDONLY	0x01		/* mount fs read only */
#define M_NOSUID	0x02		/* mount fs with setuid not allowed */

/*
 * File system types
 */
#define MOUNT_UFS	0
#define MOUNT_NFS	1
#define MOUNT_PC	2
#define MOUNT_AFS	3
#define MOUNT_CFS	4
#define MOUNT_MAXTYPE	4

struct ufs_args {
        char    *fspec;
};

#ifdef	PCFS
struct pc_args {
	char	*fspec;
};
#endif	PCFS

/* uio.h */
#define	uio_seg		uio_segflg

/* credentials are just user identities */
#define	ucred		identity
#define	u_cred		u_identity
#define	cr_ref		id_ref
#define	cr_uid		id_uid
#define	cr_gid		id_gid
#define	cr_groups	id_groups
#define	cr_ruid		id_ruid
#define	cr_rgid		id_rgid
#define	cr_pag		id_pag
#define	crhold(cr)	((cr)->cr_ref++)
#define	crget(cr)	alloc_identity(cr)
#define	crfree(cr)	free_identity(cr)
#define	crcopy(cr)	change_identity(cr)
#define	crdup(cr)	copy_identity(cr)

/* vnode/inode mapping */
#define	DTYPE_VNODE	DTYPE_INODE

/*
 * File system identifier. Should be unique (at least per machine).
 */
typedef struct {
	long val[2];			/* file system id type */
} fsid_t;

/*
 * File identifier. Should be unique per filesystem on a single machine.
 */
#define MAXFIDSZ	16

struct fid {
	u_short		fid_len;		/* length of data in bytes */
	char		fid_data[MAXFIDSZ];	/* data (variable length) */
};

/*
 * Structure per mounted file system.
 * Each mounted file system has an array of
 * operations and an instance record.
 * The file systems are put on a singly linked list.
 */
struct vfs {
	struct vfs	*vfs_next;		/* next vfs in vfs list */
	struct vfsops	*vfs_op;		/* operations on vfs */
	struct vnode	*vfs_vnodecovered;	/* vnode we mounted on */
	int		vfs_flag;		/* flags */
	int		vfs_bsize;		/* native block size */
	fsid_t		vfs_fsid;		/* file system id */
	u_short		vfs_exroot;		/* exported fs uid 0 mapping */
	short		vfs_exflags;		/* exported fs flags */
	caddr_t		vfs_data;		/* private data */
};

/*
 * vfs flags.
 * VFS_MLOCK lock the vfs so that name lookup cannot proceed past the vfs.
 * This keeps the subtree stable during mounts and unmounts.
 */
#define VFS_RDONLY	0x01		/* read only vfs */
#define VFS_MLOCK	0x02		/* lock vfs so that subtree is stable */
#define VFS_MWAIT	0x04		/* someone is waiting for lock */
#define VFS_NOSUID	0x08		/* someone is waiting for lock */
#define VFS_EXPORTED	0x10		/* file system is exported (NFS) */

/*
 * exported vfs flags.
 */
#define EX_RDONLY	0x01		/* exported read only */

/*
 * Operations supported on virtual file system.
 */
struct vfsops {
	int	(*vfs_mount)();		/* mount file system */
	int	(*vfs_unmount)();	/* unmount file system */
	int	(*vfs_root)();		/* get root vnode */
	int	(*vfs_statfs)();	/* get fs statistics */
	int	(*vfs_sync)();		/* flush fs buffers */
	int	(*vfs_vget)();		/* get vnode from fid */
};

#define VFS_MOUNT(VFSP, PATH, DATA) \
				 (*(VFSP)->vfs_op->vfs_mount)(VFSP, PATH, DATA)
#define VFS_UNMOUNT(VFSP)	 (*(VFSP)->vfs_op->vfs_unmount)(VFSP)
#define VFS_ROOT(VFSP, VPP)	 (*(VFSP)->vfs_op->vfs_root)(VFSP,VPP)
#define VFS_STATFS(VFSP, SBP)	 (*(VFSP)->vfs_op->vfs_statfs)(VFSP,SBP)
#define VFS_SYNC(VFSP)		 (*(VFSP)->vfs_op->vfs_sync)(VFSP)
#define VFS_VGET(VFSP, VPP, FIDP) (*(VFSP)->vfs_op->vfs_vget)(VFSP, VPP, FIDP)

/*
 * file system statistics
 */
struct statfs {
	long f_type;			/* type of info, zero for now */
	long f_bsize;			/* fundamental file system block size */
	long f_blocks;			/* total blocks in file system */
	long f_bfree;			/* free block in fs */
	long f_bavail;			/* free blocks avail to non-superuser */
	long f_files;			/* total file nodes in file system */
	long f_ffree;			/* free file nodes in fs */
	fsid_t f_fsid;			/* file system id */
	long f_spare[7];		/* spare for later */
};

#ifdef	KERNEL
/*
 * public operations
 */
extern void	vfs_mountroot();	/* mount the root */
extern int	vfs_add();		/* add a new vfs to mounted vfs list */
extern void	vfs_remove();		/* remove a vfs from mounted vfs list */
extern int	vfs_lock();		/* lock a vfs */
extern void	vfs_unlock();		/* unlock a vfs */
extern struct vfs *getvfs();		/* return vfs given fsid */
extern void	vattr_null();		/* set vattr to null -- MACH_NFS */

#define VFS_INIT(VFSP, OP, DATA)	{ \
	(VFSP)->vfs_next = (struct vfs *)0; \
	(VFSP)->vfs_op = (OP); \
	(VFSP)->vfs_flag = 0; \
	(VFSP)->vfs_exflags = 0; \
	(VFSP)->vfs_data = (DATA); \
}

/*
 * globals
 */
extern struct vfs *rootvfs;		/* ptr to root vfs structure */

extern struct vnode *rootvp;		/* MACH_NFS */
#endif

#endif __MACH__
#endif	_VFS_VFS_H_
