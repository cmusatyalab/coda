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

static char *rcsid = "$Header: /usr/rvb/XX/src/kernel-src/vfs/mach/RCS/mach_vnode.h,v 4.1 1997/01/08 21:53:33 rvb Exp $";
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
 * $Log: mach_vnode.h,v $
 * Revision 4.1  1997/01/08 21:53:33  rvb
 * r = 4.1; fix $ HEADERS
 *
 * Revision 1.1  1996/11/22 19:09:38  braam
 * First Checkin (pre-release)
 *
 * Revision 1.1  96/11/22  13:30:23  raiff
 * First Checkin (pre-release)
 * 
 * Revision 3.3.1.1  96/08/26  12:39:15  raiff
 * Branch for release beta-26Aug1996_41240
 * 
 * Revision 3.3  96/08/23  19:23:57  satya
 * vfs.h and vnode.h should come from /usr/include/sys on BSD.
 * Added #ifdef __MACH__ around code.  Also added bogus code surrounded
 * by #ifdef __BSD44__ to trap if these versions of vfs.h and vnode.h 
 * are included on BSD.
 * 
 * Revision 3.2.4.1  96/07/24  11:17:34  raiff
 * Branch for release beta-24Jul1996_36690
 * 
 * Revision 3.2  95/10/09  19:33:58  satya
 * Reblurbed with new CMU and IBM notices for SOSP-15 CD-ROM
 * 
 * Revision 3.1  95/06/08  16:11:38  satya
 * *** empty log message ***
 * 
 * Revision 2.1.7.1  95/05/11  11:40:57  raiff
 * Branch for release beta-11May1995_36561
 * 
 * Revision 2.1  94/07/21  16:51:27  satya
 * Conversion to C++ 3.0; start of Coda Release 2.0
 * 
 * Revision 1.1  92/04/03  18:26:47  satya
 * Initial revision
 * 
 * Revision 1.1  90/03/27  07:56:56  satya
 * Initial revision
 * 
 * Revision 2.10  89/09/05  20:45:56  jsb
 * 	Made more VOP routines handle @sys expansion to fix bogus semantics.
 * 	[89/09/05  15:41:04  jsb]
 * 
 * Revision 2.9  89/08/02  08:13:20  jsb
 * 	Eliminated MACH conditionals. Added VOP_FREEFID macro.
 * 	[89/07/31  16:14:36  jsb]
 * 
 * Revision 2.8  89/06/25  00:00:51  jsb
 * 	This file didn't get merged in last time?
 * 	[89/06/24  23:45:35  jsb]
 * 
 * Revision 2.7.1.3  89/06/12  14:52:54  jsb
 * 	Replaced VOP_PAGE_INIT with VOP_PAGE_WRITE.
 * 	[89/06/12  09:45:00  jsb]
 * 
 * Revision 2.7  89/06/03  15:42:58  jsb
 * 	Redefined VOP_LOOKUP to support @sys expansion in all filesystems.
 * 	[89/05/26  16:55:10  jsb]
 * 
 * Revision 2.6  89/05/11  14:45:28  gm0w
 * 	Added VOP_READ1DIR.
 * 	[89/05/11            gm0w]
 * 
 * Revision 2.5  89/04/22  15:35:00  gm0w
 * 	Place enum vtype mftovt_tab under !MACH.
 * 	[89/04/19            gm0w]
 * 	Created macros for manipulating inodes/vnodes.
 * 	[89/04/14            gm0w]
 * 
 * Revision 2.4  89/03/09  22:45:21  rpd
 * 	More cleanup.
 * 
 * Revision 2.3  89/02/26  11:13:52  gm0w
 * 	Changes for cleanup.
 * 
 * Revision 2.2  89/01/18  01:27:31  jsb
 * 	Protect against multiple inclusion; MACH_RFS: add rfs support.
 * 	[89/01/17  17:34:07  jsb]
 * 
 */
/*	@(#)vnode.h 1.1 86/09/25 SMI	*/

#ifndef	_VNODE_ 
#define _VNODE_	1

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
#include <sys/types.h>
#include <sys/time.h>
#ifdef	KERNEL
#include <kern/zalloc.h>
#endif	KERNEL

#define	vnode		inode_header

#define	i_vnode		i_header

#define	v_next		ih_chain[0]
#define	v_flag		ih_flag
#define	v_count		ih_count
#define	v_shlockc	ih_shlockc
#define	v_exlockc	ih_exlockc
#define	v_vfsmountedhere ih_mountedfs
#define	v_socket	ih_socket
#define	v_vfsp		ih_fs
#define	v_type		ih_type
#define	v_mode		ih_mode
#define	v_rdev		ih_rdev
#define	v_vm_info	ih_vm_info

#define	VTOI(vp)	((struct inode *)(vp))
#define	ITOV(ip)	((struct vnode *)(ip))

/* flags */
#define	VROOT		IROOT
#define	VTEXT		ITEXT
#define VSHLOCK		ISHLOCK
#define VEXLOCK		IEXLOCK
#define VLWAIT		ILWAIT

/* modes */
#define VFMT		IFMT
#define VCHR		IFCHR
#define VDIR		IFDIR
#define VBLK		IFBLK
#define VREG		IFREG
#define VLNK		IFLNK
#define VSOCK		IFSOCK
#define VSUID		ISUID
#define VSGID		ISGID
#define VSVTX		ISVTX
#define VREAD		IREAD
#define VWRITE		IWRITE
#define VEXEC		IEXEC

#define ESAME	(-1)		/* trying to rename linked files (special) */

/*
 * Check that file is owned by current user or user is su.
 */
#define OWNER(CR, IP)	\
	(((CR)->cr_uid == (IP)->i_uid)? 0: (suser()? 0: u.u_error))

/*
 * enums
 */
enum de_op	{ DE_CREATE, DE_LINK, DE_RENAME };	/* direnter ops */

/*
 * This overlays the fid structure (see vfs.h)
 */
struct ufid {
	u_short	ufid_len;
	ino_t	ufid_ino;
	long	ufid_gen;
};

/*
 * Operations on vnodes.
 */
struct vnodeops {
	int	(*vn_open)();
	int	(*vn_close)();
	int	(*vn_rdwr)();
	int	(*vn_ioctl)();
	int	(*vn_select)();
	int	(*vn_getattr)();
	int	(*vn_setattr)();
	int	(*vn_access)();
	int	(*vn_lookup)();
	int	(*vn_create)();
	int	(*vn_remove)();
	int	(*vn_link)();
	int	(*vn_rename)();
	int	(*vn_mkdir)();
	int	(*vn_rmdir)();
	int	(*vn_readdir)();
	int	(*vn_symlink)();
	int	(*vn_readlink)();
	int	(*vn_fsync)();
	int	(*vn_inactive)();
	int	(*vn_bmap)();
	int	(*vn_strategy)();
	int	(*vn_bread)();
	int	(*vn_brelse)();
	int	(*vn_lockctl)();
	int	(*vn_fid)();
	int	(*vn_page_read)();
	int	(*vn_page_write)();
	int	(*vn_read1dir)();
	int	(*vn_freefid)();
};

#ifdef	KERNEL

extern struct vnodeops *vnodesw[];
#define	_VOP_(VP)	(vnodesw[(VP)->v_type])

#define VOP_OPEN(VPP,F,C)		(*_VOP_(*(VPP))->vn_open)(VPP, F, C)
#define VOP_CLOSE(VP,F,C)		(*_VOP_(VP)->vn_close)(VP,F,C)
#define VOP_RDWR(VP,UIOP,RW,F,C)	(*_VOP_(VP)->vn_rdwr)(VP,UIOP,RW,F,C)
#define VOP_IOCTL(VP,C,D,F,CR)		(*_VOP_(VP)->vn_ioctl)(VP,C,D,F,CR)
#define VOP_SELECT(VP,W,C)		(*_VOP_(VP)->vn_select)(VP,W,C)
#define VOP_GETATTR(VP,VA,C)		(*_VOP_(VP)->vn_getattr)(VP,VA,C)
#define VOP_SETATTR(VP,VA,C)		(*_VOP_(VP)->vn_setattr)(VP,VA,C)
#define VOP_ACCESS(VP,M,C)		(*_VOP_(VP)->vn_access)(VP,M,C)
#define VOP_LOOKUP(VP,NM,VPP,C)		vop_lookup(VP,NM,VPP,C)
#define VOP_CREATE(VP,NM,VA,E,M,VPP,C)	vop_create(VP,NM,VA,E,M,VPP,C)
#define VOP_REMOVE(VP,NM,C)		vop_remove(VP,NM,C)
#define VOP_LINK(VP,TDVP,TNM,C)		vop_link(VP,TDVP,TNM,C)
#define VOP_RENAME(VP,NM,TDVP,TNM,C)	vop_rename(VP,NM,TDVP,TNM,C)
#define VOP_MKDIR(VP,NM,VA,VPP,C)	vop_mkdir(VP,NM,VA,VPP,C)
#define VOP_RMDIR(VP,NM,C)		vop_rmdir(VP,NM,C)
#define VOP_READDIR(VP,UIOP,C)		(*_VOP_(VP)->vn_readdir)(VP,UIOP,C)
#define VOP_SYMLINK(VP,LNM,VA,TNM,C)	vop_symlink(VP,LNM,VA,TNM,C)
#define VOP_READLINK(VP,UIOP,C)		(*_VOP_(VP)->vn_readlink)(VP,UIOP,C)
#define VOP_FSYNC(VP,C)			(*_VOP_(VP)->vn_fsync)(VP,C)
#define VOP_INACTIVE(VP,C)		(*_VOP_(VP)->vn_inactive)(VP,C)
#define VOP_BMAP(VP,BN,VPP,BNP)		(*_VOP_(VP)->vn_bmap)(VP,BN,VPP,BNP)
#define VOP_STRATEGY(BP)		(*_VOP_(ITOV((BP)->b_vp))->vn_strategy)(BP)
#define VOP_BREAD(VP,BN,BPP)		(*_VOP_(VP)->vn_bread)(VP,BN,BPP)
#define VOP_BRELSE(VP,BP)		(*_VOP_(VP)->vn_brelse)(VP,BP)
#define VOP_LOCKCTL(VP,LD,CMD,C)	(*_VOP_(VP)->vn_lockctl) \
						(VP,LD,CMD,C)
#define VOP_FID(VP,FIDPP)		(*_VOP_(VP)->vn_fid)(VP, FIDPP)
#define VOP_PAGE_READ(VP,PG,SZ,OF,C)	(*_VOP_(VP)->vn_page_read) \
						(VP,PG,SZ,OF,C)
#define VOP_PAGE_WRITE(VP,PG,SZ,OF,C,I)	(*_VOP_(VP)->vn_page_write) \
						(VP,PG,SZ,OF,C,I)
#define VOP_READ1DIR(VP,UIOP,C)		(*_VOP_(VP)->vn_read1dir)(VP,UIOP,C)
#define	VOP_FREEFID(VP,FIDP)		(*_VOP_(VP)->vn_freefid)(VP,FIDP)

/*
 * flags for above
 */
#define IO_UNIT		0x01		/* do io as atomic unit for VOP_RDWR */
#define IO_APPEND	0x02		/* append write for VOP_RDWR */
#define IO_SYNC		0x04		/* sync io for VOP_RDWR */
#define IO_NDELAY	0x08		/* non-blocking i/o for fifos */

#endif	KERNEL

/*
 * Vnode attributes.  A field value of -1
 * represents a field whose value is unavailable
 * (getattr) or which is not to be changed (setattr).
 */
struct vattr {
	u_short		va_mode;	/* files access mode and type */
	short		va_uid;		/* owner user id */
	short		va_gid;		/* owner group id */
	long		va_fsid;	/* file system id (dev for now) */
	long		va_nodeid;	/* node id */
	short		va_nlink;	/* number of references to file */
	u_long		va_size;	/* file size in bytes (quad?) */
	long		va_blocksize;	/* blocksize preferred for i/o */
	struct timeval	va_atime;	/* time of last access */
	struct timeval	va_mtime;	/* time of last modification */
	struct timeval	va_ctime;	/* time file ``created */
	dev_t		va_rdev;	/* device the file represents */
	long		va_blocks;	/* kbytes of disk space held by file */
};

#ifdef	KERNEL
#include <mach_rfs.h>

/*
 * public vnode manipulation functions
 */
extern int vn_open();			/* open vnode */
extern int vn_create();			/* creat/mkdir vnode */
extern int vn_rdwr();			/* read or write vnode */
extern int vn_close();			/* close vnode */
extern void vn_rele();			/* release vnode */
extern int vn_link();			/* make hard link */
extern int vn_rename();			/* rename (move) */
extern int vn_remove();			/* remove/rmdir */
extern void vattr_null();		/* set attributes to null */
extern int getvnodefp();		/* get fp from vnode fd */

#define VN_HOLD(VP)	{ \
	(VP)->v_count++; \
}

#define VN_RELE(VP)	{ \
	vn_rele(VP); \
}

#define VN_INIT(VP, VFSP, TYPE, DEV)	{ \
	CLEAR_VNODE_FLAGS(VP); \
	(VP)->v_count = 1; \
	(VP)->v_shlockc = (VP)->v_exlockc = 0; \
	(VP)->v_vfsp = (VFSP); \
	(VP)->v_mode = ((TYPE)&VFMT); \
	(VP)->v_rdev = (DEV); \
	(VP)->v_socket = 0; \
}

/*
 * flags for above
 */
enum rm		{ FILE, DIRECTORY };		/* rmdir or rm (remove) */
enum symfollow	{ NO_FOLLOW, FOLLOW_LINK };	/* follow symlinks (lookuppn) */
enum vcexcl	{ NONEXCL, EXCL};		/* (non)excl create (create) */

/*
 * Global vnode data.
 */

#if	MACH_RFS
/*
 *  In the spirit of the namei/rnamei/cnamei hack, we hide the
 *  extra (rfs-support) parameters to lookupname and lookuppn.
 *  The default definition of lookup* sets both okremote and
 *  keepnamebuf to 0.
 *  Whereever namei was defined as rnamei, we define lookup* as rlookup*,
 *  which sets okremote to 1 and keepnamebuf to 0.
 *  The real definitions are availible as extended_lookup*.
 */
#define lookupname(p1, p2, p3, p4, p5) \
	extended_lookupname(p1, p2, p3, p4, p5, 0, (struct pathname *)0)
#define lookuppn(p1, p2, p3, p4) \
	extended_lookuppn(p1, p2, p3, p4, 0)

#define rlookupname(p1, p2, p3, p4, p5) \
	extended_lookupname(p1, p2, p3, p4, p5, 1, (struct pathname *)0)
#define rlookuppn(p1, p2, p3, p4) \
	extended_lookuppn(p1, p2, p3, p4, 1)
#endif	MACH_RFS

extern zone_t vfs_name_zone;
extern zone_t vfs_vfs_zone;

#endif	KERNEL
#endif __MACH__
#endif	_VNODE_
