/* 
 * Mach Operating System
 * Copyright (c) 1990 Carnegie-Mellon University
 * Copyright (c) 1989 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */

/*
 * This code was written for the Coda file system at Carnegie Mellon
 * University.  Contributers include David Steere, James Kistler,
 * M. Satyanarayanan, and Brian Noble.  
 */

/* 
 * HISTORY
 * $Log:	cfs.h,v $
 * Revision 1.5.18.4  97/11/26  15:28:55  rvb
 * Cant make downcall pbuf == union cfs_downcalls yet
 * 
 * Revision 1.5.18.3  97/11/24  15:44:42  rvb
 * Final cfs_venus.c w/o macros, but one locking bug
 * 
 * Revision 1.5.18.2  97/11/13  22:02:55  rvb
 * pass2 cfs_NetBSD.h mt
 * 
 * Revision 1.5.18.1  97/11/12  12:09:32  rvb
 * reorg pass1
 * 
 * Revision 1.5  96/12/12  22:10:54  bnoble
 * Fixed the "downcall invokes venus operation" deadlock in all known cases.  There may be more
 * 
 * Revision 1.4  1996/12/05 16:20:04  bnoble
 * Minor debugging aids
 *
 * Revision 1.3  1996/11/08 18:06:05  bnoble
 * Minor changes in vnode operation signature, VOP_UPDATE signature, and
 * some newly defined bits in the include files.
 *
 * Revision 1.2  1996/01/02 16:56:31  bnoble
 * Added support for Coda MiniCache and raw inode calls (final commit)
 *
 * Revision 1.1.2.1  1995/12/20 01:56:51  bnoble
 * Added CFS-specific files
 *
 * 
 * Revision 3.1  1995/03/04  19:08:16  bnoble
 * Bump to major revision 3 to prepare for NetBSD port
 *
 * Revision 2.8  1995/02/17  18:21:41  dcs
 * Small change. Assume venus is only interested in specifying the size
 * of things it is writing to kernel (i.e. "out" direction)
 *
 * Revision 2.7  95/02/17  16:25:07  dcs
 * These versions represent several changes:
 * 1. Allow venus to restart even if outstanding references exist.
 * 2. Have only one ctlvp per client, as opposed to one per mounted cfs device.d
 * 3. Allow ody_expand to return many members, not just one.
 * 
 * Revision 2.6  94/11/17  10:13:48  dcs
 * Small incremental changes to cfs_mach.c and cfs.h
 * 
 * Revision 2.5  94/10/18  10:46:26  dcs
 * Satya didn't like the name of 'sets/mach.h'
 * 
 * Revision 2.4  94/10/14  09:57:33  dcs
 * Made changes 'cause sun4s have braindead compilers
 * 
 * Revision 2.3  94/10/12  16:45:57  dcs
 * Cleaned kernel/venus interface by removing XDR junk, plus
 * so cleanup to allow this code to be more easily ported.
 * 
 * Revision 1.3  93/12/17  01:33:41  luqi
 * Changes made for kernel to pass process info to Venus:
 * 
 * (1) in file cfs.h
 * add process id and process group id in most of the cfs argument types.
 * 
 * (2) in file cfs_vnodeops.c
 * add process info passing in most of the cfs vnode operations.
 * 
 * (3) in file cfs_xdr.c
 * expand xdr routines according changes in (1). 
 * add variable pass_process_info to allow venus for kernel version checking.
 * 
 * Revision 1.2  92/10/27  17:58:20  lily
 * merge kernel/latest and alpha/src/cfs
 * 
 * Revision 2.3  92/09/30  14:16:11  mja
 * 	Reorganized procedure declarations, so that "arg" parameters could be 
 * 	fully specified.
 * 	[91/07/23            jjk]
 * 
 * 	Substituted rvb's history blurb so that we agree with Mach 2.5 sources.
 * 	[91/02/09 	     jjk]
 * 
 * 	Added contributors blurb.
 * 	[90/12/13            jjk]
 * 
 * Revision 2.2  90/07/05  11:26:23  mrt
 * 	Changed message size back to 4k. Also changed VC_MAXDATASIZE to reflect
 * 	the xdr implementation of treating shorts (2 bytes) as longs (4 bytes).
 * 	[90/05/23            dcs]
 * 
 * 	Added constants to support READDIR, IOCTL, and RDWR messages 
 * 	in an effort to compensate for the VFS exec bogusity.
 * 	[90/05/23            dcs]
 * 
 * 	Created for the Coda File System.
 * 	[90/05/23            dcs]
 * 
 * Revision 1.6  90/05/31  17:01:05  dcs
 * Prepare for merge with facilities kernel.
 * 
 * 
 */
#ifndef _CFS_HEADER_
#define _CFS_HEADER_

/* 
 * Sigh, rp2gen can't deal with #defines, so I can't use this test to
 * define ViceFid here, where it should be defined. Sigh.  This needs
 * to be before the #include of OS-specific headers, 'cause it is used
 * there.
 */
#include <sys/types.h>
#include <sys/vnode.h>
#include <sys/ucred.h>

#define CodaCred ucred

#ifdef _KERNEL
#ifndef	VICEFID_DEFINED
#define	VICEFID_DEFINED	1
typedef u_long VolumeId;
typedef u_long VnodeId;
typedef u_long Unique;
typedef struct ViceFid {
    VolumeId Volume;
    VnodeId Vnode;
    Unique Unique;
} ViceFid;
#endif	/* not VICEFID_DEFINED */
#endif  /* _KERNEL */

/*
 * cfid structure:
 * This overlays the fid structure (see vfs.h)
 */
struct cfid {
    u_short	cfid_len;
    u_short     padding;
    ViceFid	cfid_fid;
};

#ifdef __NetBSD__
#include <cfs/cfs_NetBSD.h>
#endif /* __NetBSD__ */

/*
 * Cfs constants
 */
#define CFS_MAXNAMLEN 256
#define CFS_MAXPATHLEN MAXPATHLEN
#define CFS_MAXARRAYSIZE 8192

/*
#define CFS_MOUNT	((u_long) 1)
#define CFS_UNMOUNT	((u_long) 2)
*/
#define CFS_ROOT	((u_long) 2)
#define CFS_SYNC	((u_long) 3)
#define CFS_OPEN	((u_long) 4)
#define CFS_CLOSE	((u_long) 5)
#define CFS_IOCTL	((u_long) 6)
#define CFS_GETATTR	((u_long) 7)
#define CFS_SETATTR	((u_long) 8)
#define CFS_ACCESS	((u_long) 9)
#define CFS_LOOKUP	((u_long) 10)
#define CFS_CREATE	((u_long) 11)
#define CFS_REMOVE	((u_long) 12)
#define CFS_LINK	((u_long) 13)
#define CFS_RENAME	((u_long) 14)
#define CFS_MKDIR	((u_long) 15)
#define CFS_RMDIR	((u_long) 16)
#define CFS_READDIR	((u_long) 17)
#define CFS_SYMLINK	((u_long) 18)
#define CFS_READLINK	((u_long) 19)
#define CFS_FSYNC	((u_long) 20)
#define CFS_INACTIVE	((u_long) 21)
#define	CFS_VGET	((u_long) 22)
#define	CFS_SIGNAL	((u_long) 23)
#define CFS_REPLACE	((u_long) 24)
#define CFS_FLUSH       ((u_long) 25)
#define CFS_PURGEUSER   ((u_long) 26)
#define CFS_ZAPFILE     ((u_long) 27)
#define CFS_ZAPDIR      ((u_long) 28)
#define CFS_ZAPVNODE    ((u_long) 29)
#define CFS_PURGEFID    ((u_long) 30)
#define DOWNCALL(opcode) (opcode >= CFS_REPLACE && opcode <= CFS_PURGEFID)
#define	CFS_RDWR	((u_long) 31)
#define ODY_MOUNT	((u_long) 32) /* Don't use DEBUG, it uses these as bits */
#define ODY_LOOKUP	((u_long) 33)
#define ODY_EXPAND	((u_long) 34)
/* #define	CFS_INVALIDATE	((u_long) 35) Is this used anywhere? */
#define CFS_NCALLS 35

struct cfs_in_hdr {
    unsigned long opcode;
    unsigned long unique;	    /* Keep multiple outstanding msgs distinct */
    u_short pid;		    /* Common to all */
    u_short pgid;		    /* Common to all */
    struct CodaCred cred;	    /* Common to all */
};

union inputArgs {
    /* Nothing needed for cfs_root */
    /* Nothing needed for cfs_sync */
    struct cfs_open_in {
	struct cfs_in_hdr ih;
	ViceFid	VFid;
	int	flags;
    } cfs_open;
    struct cfs_close_in {
	struct cfs_in_hdr ih;
	ViceFid	VFid;
	int	flags;
    } cfs_close;
    struct cfs_ioctl_in {
	struct cfs_in_hdr ih;
	ViceFid VFid;
	int	cmd;
	int	len;
	int	rwflag;
	char *data;			/* Place holder for data. */
    } cfs_ioctl;
    struct cfs_getattr_in {
	struct cfs_in_hdr ih;
	ViceFid VFid;
    } cfs_getattr;
    struct cfs_setattr_in {
	struct cfs_in_hdr ih;
	ViceFid VFid;
	struct vattr attr;
    } cfs_setattr;
    struct cfs_access_in {
	struct cfs_in_hdr ih;
	ViceFid	VFid;
	int	flags;
    } cfs_access;
    struct  cfs_lookup_in {
	struct cfs_in_hdr ih;
	ViceFid	VFid;
	char        *name;		/* Place holder for data. */
    } cfs_lookup;
    struct cfs_create_in {
	struct cfs_in_hdr ih;
	ViceFid VFid;
	struct vattr attr;
	int excl;
	int mode;
	char	*name;		/* Place holder for data. */
    } cfs_create;
    struct cfs_remove_in {
	struct cfs_in_hdr ih;
	ViceFid	VFid;
	char	*name;		/* Place holder for data. */
    } cfs_remove;
    struct cfs_link_in {
	struct cfs_in_hdr ih;
	ViceFid sourceFid;          /* cnode to link *to* */
	ViceFid destFid;            /* Directory in which to place link */
	char	*tname;		/* Place holder for data. */
    } cfs_link;
    struct cfs_rename_in {
	struct cfs_in_hdr ih;
	ViceFid	sourceFid;
	char	*srcname;
	ViceFid destFid;
	char	*destname;
    } cfs_rename;
    struct cfs_mkdir_in {
	struct cfs_in_hdr ih;
	ViceFid	VFid;
	struct vattr attr;
	char	*name;		/* Place holder for data. */
    } cfs_mkdir;
    struct cfs_rmdir_in {
	struct cfs_in_hdr ih;
	ViceFid	VFid;
	char	*name;		/* Place holder for data. */
    } cfs_rmdir;
    struct cfs_readdir_in {
	struct cfs_in_hdr ih;
	ViceFid	VFid;
	int	count;
	int	offset;
    } cfs_readdir;
    struct cfs_symlink_in {
	struct cfs_in_hdr ih;
	ViceFid	VFid;          /* Directory to put symlink in */
	char	*srcname;
	struct vattr attr;
	char	*tname;
    } cfs_symlink;
    struct cfs_readlink_in {
	struct cfs_in_hdr ih;
	ViceFid VFid;
    } cfs_readlink;
    struct cfs_fsync_in {
	struct cfs_in_hdr ih;
	ViceFid VFid;
    } cfs_fsync;
    struct cfs_inactive_in {
	struct cfs_in_hdr ih;
	ViceFid VFid;
    } cfs_inactive;
    struct cfs_vget_in {
	struct cfs_in_hdr ih;
	ViceFid VFid;
    } cfs_vget;
    /* CFS_SIGNAL is out-of-band, doesn't need data. */
    /* CFS_INVALIDATE is a venus->kernel call */
    /* CFS_FLUSH is a venus->kernel call */
    /* CFS_PURGEUSER is a venus->kernel call */
    /* CFS_ZAPFILE is a venus->kernel call */
    /* CFS_ZAPDIR is a venus->kernel call */	
    /* CFS_ZAPVNODE is a venus->kernel call */	
    /* CFS_PURGEFID is a venus->kernel call */	
    struct cfs_rdwr_in {
	struct cfs_in_hdr ih;
	ViceFid	VFid;
	int	rwflag;
	int	count;
	int	offset;
	int	ioflag;
	caddr_t	data;		/* Place holder for data. */	
    } cfs_rdwr;
    /* CFS_REPLACE is a venus->kernel call */	
};

/* 
 * Occasionally, don't cache the fid returned by CFS_LOOKUP. For instance, if
 * the fid is inconsistent. This case is handled by setting the top bit of the
 * return result parameter.
 */
#define CFS_NOCACHE          0x80000000

#define INIT_OUT(out, opcode, result) \
    out->opcode = (opcode); out->result = (result);

/* Used to structure buffer in ody_expand_out */
/* the link in LinkT is just the 1st 4 characters of the actual link */
typedef struct linktype {
    int next;		/* Offset into buffer of next element */
    char link[sizeof(char *)];	/* Place holder for data */
} linkT;

/* Really important that opcode and unique are 1st two fields! */
struct cfs_out_hdr {
    unsigned long opcode;
    unsigned long unique;	    /* Keep multiple outstanding msgs distinct */
    unsigned long result;
};

union outputArgs {
    struct cfs_root_out {
	struct cfs_out_hdr oh;
	ViceFid VFid;
    } cfs_root;
    /* Nothing needed for cfs_sync */
    struct cfs_open_out {
	struct cfs_out_hdr oh;
	dev_t	dev;
	ino_t	inode;
    } cfs_open;
    /* Nothing needed for cfs_close */
    struct cfs_ioctl_out {
	struct cfs_out_hdr oh;
	int	len;
	caddr_t	data;		/* Place holder for data. */
    } cfs_ioctl;
    struct cfs_getattr_out {
	struct cfs_out_hdr oh;
	struct vattr attr;
    } cfs_getattr;
    /* Nothing needed for cfs_setattr */
    /* Nothing needed for cfs_access */
    struct cfs_lookup_out {
	struct cfs_out_hdr oh;
	ViceFid VFid;
	int	vtype;
    } cfs_lookup;
    struct cfs_create_out {
	struct cfs_out_hdr oh;
	ViceFid VFid;
	struct vattr attr;
    } cfs_create;
    /* Nothing needed for cfs_remove */
    /* Nothing needed for cfs_link */
    /* Nothing needed for cfs_rename */
    struct cfs_mkdir_out {
	struct cfs_out_hdr oh;
	ViceFid VFid;
	struct vattr attr;
    } cfs_mkdir;
    /* Nothing needed for cfs_rmdir */
    struct cfs_readdir_out {
	struct cfs_out_hdr oh;
	int	size;
	caddr_t	data;		/* Place holder for data. */
    } cfs_readdir;
    /* Nothing needed for cfs_symlink */
    struct cfs_readlink_out {
	struct cfs_out_hdr oh;
	int	count;
	caddr_t	data;		/* Place holder for data. */
    } cfs_readlink;
    /* Nothing needed for cfs_fsync */
    /* Nothing needed for cfs_inactive */
    struct cfs_vget_out {
	struct cfs_out_hdr oh;
	ViceFid VFid;
	int	vtype;
    } cfs_vget;
    /* CFS_SIGNAL is out-of-band, doesn't need data. */
    /* CFS_INVALIDATE is a venus->kernel call */
    /* CFS_FLUSH is a venus->kernel call */
    struct cfs_purgeuser_out {/* CFS_PURGEUSER is a venus->kernel call */
	struct cfs_out_hdr oh;
	struct CodaCred cred;
    } cfs_purgeuser;
    struct cfs_zapfile_out {  /* CFS_ZAPFILE is a venus->kernel call */
	struct cfs_out_hdr oh;
	ViceFid CodaFid;
    } cfs_zapfile;
    struct cfs_zapdir_out {	  /* CFS_ZAPDIR is a venus->kernel call */
	struct cfs_out_hdr oh;
	ViceFid CodaFid;
    } cfs_zapdir;
    struct cfs_zapvnode_out { /* CFS_ZAPVNODE is a venus->kernel call */
	struct cfs_out_hdr oh;
	struct CodaCred cred;
	ViceFid VFid;
    } cfs_zapvnode;
    struct cfs_purgefid_out { /* CFS_PURGEFID is a venus->kernel call */	
	struct cfs_out_hdr oh;
	ViceFid CodaFid;
    } cfs_purgefid;
    struct cfs_rdwr_out {
	struct cfs_out_hdr oh;
	int	rwflag;
	int	count;
	caddr_t	data;	/* Place holder for data. */
    } cfs_rdwr;
    struct cfs_replace_out { /* cfs_replace is a venus->kernel call */
	struct cfs_out_hdr oh;
	ViceFid NewFid;
	ViceFid OldFid;
    } cfs_replace;
};    

union cfs_downcalls {
    /* CFS_INVALIDATE is a venus->kernel call */
    /* CFS_FLUSH is a venus->kernel call */
    struct cfs_purgeuser_out purgeuser;
    struct cfs_zapfile_out zapfile;
    struct cfs_zapdir_out zapdir;
    struct cfs_zapvnode_out zapvnode;
    struct cfs_purgefid_out purgefid;
    struct cfs_replace_out replace;
};

union cfs_root {
    struct cfs_in_hdr in;
    struct cfs_root_out out;
};

union cfs_open {
    struct cfs_open_in in;
    struct cfs_open_out out;
};

union cfs_close {
    struct cfs_close_in in;
    struct cfs_out_hdr out;
};

union cfs_ioctl {
    struct cfs_ioctl_in in;
    struct cfs_ioctl_out out;
};

union cfs_getattr {
    struct cfs_getattr_in in;
    struct cfs_getattr_out out;
};

union cfs_setattr {
    struct cfs_setattr_in in;
    struct cfs_out_hdr out;
};

union cfs_access {
    struct cfs_access_in in;
    struct cfs_out_hdr out;
};

union cfs_lookup {
    struct cfs_lookup_in in;
    struct cfs_lookup_out out;
};

union cfs_create {
    struct cfs_create_in in;
    struct cfs_create_out out;
};

union cfs_remove {
    struct cfs_remove_in in;
    struct cfs_out_hdr out;
};

union cfs_link {
    struct cfs_link_in in;
    struct cfs_out_hdr out;
};

union cfs_rename {
    struct cfs_rename_in in;
    struct cfs_out_hdr out;
};

union cfs_mkdir {
    struct cfs_mkdir_in in;
    struct cfs_mkdir_out out;
};

union cfs_rmdir {
    struct cfs_rmdir_in in;
    struct cfs_out_hdr out;
};

union cfs_readdir {
    struct cfs_readdir_in in;
    struct cfs_readdir_out out;
};

union cfs_symlink {
    struct cfs_symlink_in in;
    struct cfs_out_hdr out;
};

union cfs_readlink {
    struct cfs_readlink_in in;
    struct cfs_readlink_out out;
};

union cfs_fsync {
    struct cfs_fsync_in in;
    struct cfs_out_hdr out;
};

union cfs_inactive {
    struct cfs_inactive_in in;
    struct cfs_out_hdr out;
};

union cfs_vget {
    struct cfs_vget_in in;
    struct cfs_vget_out out;
};

	/* CFS_SIGNAL is out-of-band, doesn't need data. */
	/* CFS_INVALIDATE is a venus->kernel call */
	/* CFS_FLUSH is a venus->kernel call */
	/* CFS_PURGEUSER is a venus->kernel call */
	/* CFS_ZAPFILE is a venus->kernel call */
	/* CFS_ZAPDIR is a venus->kernel call */	
	/* CFS_ZAPVNODE is a venus->kernel call */	
	/* CFS_PURGEFID is a venus->kernel call */	

union cfs_rdwr {
    struct cfs_rdwr_in in;
    struct cfs_rdwr_out out;
};

	/* CFS_REPLACE is a venus->kernel call */	

    
/*
 * Kernel <--> Venus communications.
 */

/* Put a cap on the size of messages. Some upcalls pass dynamic
 * amounts of data.  These macros cap that amount, and define the size
 * of the headers for the upcalls and returns.  
 */
#define	VC_IN_NO_DATA	    sizeof (struct cfs_in_hdr)
#define	VC_OUT_NO_DATA	    sizeof (struct cfs_out_hdr)

#define VC_INSIZE(member)   ((int) sizeof (struct member))
#define VC_OUTSIZE(member)  ((int) sizeof (struct member))

/* This one's for venus, since C++ doesn't know what struct foo means. */
#define VC_SIZE(Thing, Member)   (VC_OUT_NO_DATA                    \
                                  + (int)sizeof((Thing)->d.Member))

#define VC_BIGGER_OF_IN_OR_OUT  (sizeof(union outputArgs)   \
                                  > sizeof(union inputArgs) \
                                ? sizeof(union outputArgs)  \
                                : sizeof(union inputArgs))

#define VC_DATASIZE	    8192
#define	VC_MAXMSGSIZE	    (VC_DATASIZE + VC_BIGGER_OF_IN_OR_OUT)


#define	CFS_CONTROL		".CONTROL"
#define	CTL_VOL			-1
#define	CTL_VNO			-1
#define	CTL_UNI			-1

#define	IS_CTL_FID(fidp)	((fidp)->Volume == CTL_VOL &&\
				 (fidp)->Vnode == CTL_VNO &&\
				 (fidp)->Unique == CTL_UNI)

#define	ISDIR(fid)		((fid).Vnode & 0x1)

#endif !_CFS_HEADER_
