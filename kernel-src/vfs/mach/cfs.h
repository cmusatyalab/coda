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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/kernel-src/vfs/mach/Attic/cfs.h,v 4.2 1997/02/26 16:04:33 rvb Exp $";
#endif /*_BLURB_*/


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
 * cfs.h,v
 * Revision 1.2  1996/01/02 16:56:31  bnoble
 * Added support for Coda MiniCache and raw inode calls (final commit)
 *
 * Revision 1.1.2.1  1995/12/20 01:56:51  bnoble
 * Added CFS-specific files
 *
 * 
 * Revision 3.1  1995/03/04  19:08:16  bnoble
 * Bump to major revision 3 to prepare for BSD port
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

/* Catch new _KERNEL defn for NetBSD */
#ifdef __NetBSD__
#ifdef _KERNEL
#define KERNEL
#endif /* _KERNEL */
#endif /* __NetBSD__ */

#ifdef KERNEL
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
#endif  /* KERNEL */

#ifdef	__linux__
#include <cfs/cfs_LINUX.h>	
#ifdef __KERNEL__
#define KERNEL
#endif
#endif	/* __linux__ */

#ifdef	__MACH__
#include <cfs/cfs_MACH.h>
#endif	/* __MACH__ */

#ifdef __NetBSD__
#include <cfs/cfs_NetBSD.h>
#endif /* __NetBSD__ */

#ifdef KERNEL
/*************** VFS operation prototypes */

/* These are used directly by BSD44, and wrapped for Mach. */

int cfs_mount     __P((VFS_T *, char *, caddr_t, struct nameidata *, 
		       struct proc *));
int cfs_start     __P((VFS_T *, int, struct proc *));
int cfs_unmount   __P((VFS_T *, int, struct proc *));
int cfs_root      __P((VFS_T *, struct vnode **));
int cfs_quotactl  __P((VFS_T *, int, uid_t, caddr_t, struct proc *));
int cfs_statfs    __P((VFS_T *, struct statfs *, struct proc *));
int cfs_sync      __P((VFS_T *, int, struct ucred *, struct proc *));
int cfs_vget      __P((VFS_T *, ino_t, struct vnode **));
int cfs_fhtovp    __P((VFS_T *, struct fid *, struct mbuf *, struct vnode **,
		       int *, struct ucred **));
int cfs_vptofh    __P((struct vnode *, struct fid *));
int cfs_init      __P((void));

#endif KERNEL

/*
 * Cfs constants
 */
#define CFS_MAXNAMLEN 256
#define CFS_MAXPATHLEN MAXPATHLEN
#define CFS_MAXARRAYSIZE 8192

#define CFS_RPOGRAM	((u_long)0x20202020)
#define CFS_VERSION	((u_long)1)	
	
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

#ifndef	C_ARGS
#ifdef	__STDC__
#define	C_ARGS(arglist)	arglist
#else	__STDC__
#define	C_ARGS(arglist)	()
#endif	__STDC__
#endif	C_ARGS

#define INIT_IN(in, op, ident) \
	  (in)->opcode = (op); \
	  (in)->pid = Process_pid; \
          (in)->pgid = Process_pgid; \
	  COPY_CRED_TO_CODACRED((in), (ident));

struct inputArgs {
    unsigned long opcode;
    unsigned long unique;	    /* Keep multiple outstanding msgs distinct */
    u_short pid;		    /* Common to all */
    u_short pgid;		    /* Common to all */
    struct CodaCred cred;	    /* Common to all */
    
    union {
	/* Nothing needed for cfs_root */
	/* Nothing needed for cfs_sync */
	struct cfs_open_in {
	    ViceFid	VFid;
	    int	flags;
	} cfs_open;
	struct cfs_close_in {
	    ViceFid	VFid;
	    int	flags;
	} cfs_close;
	struct cfs_ioctl_in {
	    ViceFid VFid;
	    int	cmd;
	    int	len;
	    int	rwflag;
	    char *data;			/* Place holder for data. */
	} cfs_ioctl;
	struct cfs_getattr_in {
	    ViceFid VFid;
	} cfs_getattr;
	struct cfs_setattr_in {
	    ViceFid VFid;
	    struct vattr attr;
	} cfs_setattr;
	struct cfs_access_in {
	    ViceFid	VFid;
	    int	flags;
	} cfs_access;
	struct  cfs_lookup_in {
	    ViceFid	VFid;
	    char        *name;		/* Place holder for data. */
	} cfs_lookup;
	struct cfs_create_in {
	    ViceFid VFid;
	    struct vattr attr;
	    int excl;
	    int mode;
	    char	*name;		/* Place holder for data. */
	} cfs_create;
	struct cfs_remove_in {
	    ViceFid	VFid;
	    char	*name;		/* Place holder for data. */
	} cfs_remove;
	struct cfs_link_in {
	    ViceFid sourceFid;          /* cnode to link *to* */
	    ViceFid destFid;            /* Directory in which to place link */
	    char	*tname;		/* Place holder for data. */
	} cfs_link;
	struct cfs_rename_in {
	    ViceFid	sourceFid;
	    char	*srcname;
	    ViceFid destFid;
	    char	*destname;
	} cfs_rename;
	struct cfs_mkdir_in {
	    ViceFid	VFid;
	    struct vattr attr;
	    char	*name;		/* Place holder for data. */
	} cfs_mkdir;
	struct cfs_rmdir_in {
	    ViceFid	VFid;
	    char	*name;		/* Place holder for data. */
	} cfs_rmdir;
	struct cfs_readdir_in {
	    ViceFid	VFid;
	    int	count;
	    int	offset;
	} cfs_readdir;
	struct cfs_symlink_in {
	    ViceFid	VFid;          /* Directory to put symlink in */
	    char	*srcname;
	    struct vattr attr;
	    char	*tname;
	} cfs_symlink;
	struct cfs_readlink_in {
	    ViceFid VFid;
	} cfs_readlink;
	struct cfs_fsync_in {
	    ViceFid VFid;
	} cfs_fsync;
	struct cfs_inactive_in {
	    ViceFid VFid;
	} cfs_inactive;
	struct cfs_vget_in {
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
	    ViceFid	VFid;
	    int	rwflag;
	    int	count;
	    int	offset;
	    int	ioflag;
	    caddr_t	data;		/* Place holder for data. */	
	} cfs_rdwr;
	struct ody_mount_in {
	    char	*name;		/* Place holder for data. */
	} ody_mount;
	struct ody_lookup_in {
	    ViceFid	VFid;
	    char	*name;		/* Place holder for data. */
	} ody_lookup;
	struct ody_expand_in {
	    ViceFid VFid;
	    int size;			/* Size of buffer to return. */
	} ody_expand;
	/* CFS_REPLACE is a venus->kernel call */	
    } d;
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
struct outputArgs {
    unsigned long opcode;
    unsigned long unique;	    /* Keep multiple outstanding msgs distinct */
    unsigned long result;
    union {
	struct cfs_root_out {
	    ViceFid VFid;
	} cfs_root;
	/* Nothing needed for cfs_sync */
	struct cfs_open_out {
	    dev_t	dev;
	    ino_t	inode;
	} cfs_open;
	/* Nothing needed for cfs_close */
	struct cfs_ioctl_out {
	    int	len;
	    caddr_t	data;		/* Place holder for data. */
	} cfs_ioctl;
	struct cfs_getattr_out {
	    struct vattr attr;
	} cfs_getattr;
	/* Nothing needed for cfs_setattr */
	/* Nothing needed for cfs_access */
	struct cfs_lookup_out {
	    ViceFid VFid;
	    int	vtype;
	} cfs_lookup;
	struct cfs_create_out {
	    ViceFid VFid;
	    struct vattr attr;
	} cfs_create;
	/* Nothing needed for cfs_remove */
	/* Nothing needed for cfs_link */
	/* Nothing needed for cfs_rename */
	struct cfs_mkdir_out {
	    ViceFid VFid;
	    struct vattr attr;
	} cfs_mkdir;
	/* Nothing needed for cfs_rmdir */
	struct cfs_readdir_out {
	    int	size;
	    caddr_t	data;		/* Place holder for data. */
	} cfs_readdir;
	/* Nothing needed for cfs_symlink */
	struct cfs_readlink_out {
	    int	count;
	    caddr_t	data;		/* Place holder for data. */
	} cfs_readlink;
	/* Nothing needed for cfs_fsync */
	/* Nothing needed for cfs_inactive */
	struct cfs_vget_out {
	    ViceFid VFid;
	    int	vtype;
	} cfs_vget;
	/* CFS_SIGNAL is out-of-band, doesn't need data. */
	/* CFS_INVALIDATE is a venus->kernel call */
	/* CFS_FLUSH is a venus->kernel call */
	struct cfs_purgeuser_out {/* CFS_PURGEUSER is a venus->kernel call */
	    struct CodaCred cred;
	} cfs_purgeuser;
	struct cfs_zapfile_out {  /* CFS_ZAPFILE is a venus->kernel call */
	    ViceFid CodaFid;
	} cfs_zapfile;
	struct cfs_zapdir_out {	  /* CFS_ZAPDIR is a venus->kernel call */
	    ViceFid CodaFid;
	} cfs_zapdir;
	struct cfs_zapvnode_out { /* CFS_ZAPVNODE is a venus->kernel call */
	    struct CodaCred cred;
	    ViceFid VFid;
	} cfs_zapvnode;
	struct cfs_purgefid_out { /* CFS_PURGEFID is a venus->kernel call */	
	    ViceFid CodaFid;
	} cfs_purgefid;
	struct cfs_rdwr_out {
	    int	rwflag;
	    int	count;
	    caddr_t	data;	/* Place holder for data. */
	} cfs_rdwr;
	struct ody_mount_out {
	    ViceFid VFid;
	} ody_mount;
	struct ody_lookup_out {
	    ViceFid VFid;
	} ody_lookup;
	struct ody_expand_out {	/* Eventually it would be nice to get some */
	    char links[sizeof(int)];	/* Place holder for data. */
	} ody_expand;
	struct cfs_replace_out { /* cfs_replace is a venus->kernel call */
	    ViceFid NewFid;
	    ViceFid OldFid;
	} cfs_replace;
    } d;
};    
    
/*
 * Kernel <--> Venus communications.
 */

/* Put a cap on the size of messages. Some upcalls pass dynamic
 * amounts of data.  These macros cap that amount, and define the size
 * of the headers for the upcalls and returns.  
 */
#define	VC_IN_NO_DATA	    (2 * (int)sizeof(u_long)    \
                             + 2 * (int)sizeof(u_short) \
			     + (int)sizeof(struct CodaCred))
#define	VC_OUT_NO_DATA	    (3 * (int)sizeof(u_long))

#define VC_INSIZE(member)   (VC_IN_NO_DATA + (int)sizeof(struct member))
#define VC_OUTSIZE(member)  (VC_OUT_NO_DATA + (int)sizeof(struct member))

/* This one's for venus, since C++ doesn't know what struct foo means. */
#define VC_SIZE(Thing, Member)   (VC_OUT_NO_DATA                    \
                                  + (int)sizeof((Thing)->d.Member))


#define VC_BIGGER_OF_IN_OR_OUT  (sizeof(struct outputArgs)   \
                                  > sizeof(struct inputArgs) \
                                ? sizeof(struct outputArgs)  \
                                : sizeof(struct inputArgs))

#define VC_DATASIZE	    8192
#define	VC_MAXMSGSIZE	    (VC_DATASIZE + VC_BIGGER_OF_IN_OR_OUT)

#ifdef	KERNEL

/* Do we use the namecache? */
extern int cfsnc_use;

/* Macros to manipulate the queue */
#ifndef INIT_QUEUE
struct queue {
    struct queue *forw, *back;
};

#define INIT_QUEUE(head)                     \
do {                                         \
    (head).forw = (struct queue *)&(head);   \
    (head).back = (struct queue *)&(head);   \
} while (0)

#define GETNEXT(head) (head).forw

#define EMPTY(head) ((head).forw == &(head))

#define EOQ(el, head) ((struct queue *)(el) == (struct queue *)&(head))
		   
#define INSQUE(el, head)                             \
do {                                                 \
	(el).forw = ((head).back)->forw;             \
	(el).back = (head).back;                     \
	((head).back)->forw = (struct queue *)&(el); \
	(head).back = (struct queue *)&(el);         \
} while (0)

#define REMQUE(el)                         \
do {                                       \
	((el).forw)->back = (el).back;     \
	(el).back->forw = (el).forw;       \
}  while (0)

#endif INIT_QUEUE

struct vmsg {
    struct queue vm_chain;
    caddr_t	 vm_data;
    u_short	 vm_flags;
    u_short      vm_inSize;	/* Size is at most 5000 bytes */
    u_short	 vm_outSize;
    u_short	 vm_opcode; 	/* copied from data to save ptr lookup */
    int		 vm_unique;
    CONDITION	 vm_sleep;	/* Not used by Mach. */
};

#define	VM_READ	    1
#define	VM_WRITE    2
#define	VM_INTR	    4

struct vcomm {
	u_long		vc_seq;
	SELPROC		vc_selproc;
	struct queue	vc_requests;
	struct queue	vc_replys;
};

#define	VC_OPEN(vcp)	    ((vcp)->vc_requests.forw != NULL)
#define MARK_VC_CLOSED(vcp) (vcp)->vc_requests.forw = NULL;
/* Do nothing, since vc_nb_open() already sets this. */
#define MARK_VC_OPEN(vcp)    /* MT */

/*
 * Odyssey can have multiple volumes mounted per device (warden). Need
 * to track both the vfsp *and* the root vnode for that volume. Since
 * there is no way of doing that, I felt trading efficiency for
 * understanding was good and hence this structure, which must be
 * malloc'd on every mount.  But hopefully mounts won't be all that
 * frequent (?). -- DCS 11/29/94 
 */

struct ody_mntinfo {
        struct vnode 	   *rootvp;
	VFS_T              *vfsp;
	struct ody_mntinfo *next;
};

#define ADD_VFS_TO_MNTINFO(MI, VFS, VP)                                   \
do {                                                                      \
    if ((MI)->mi_vfschain.next) {                                         \
	struct ody_mntinfo *op;                                           \
	                                                                  \
        CFS_ALLOC(op, struct ody_mntinfo *, sizeof (struct ody_mntinfo)); \
	op->vfsp = (VFS);                                                 \
	op->rootvp = (VP);                                                \
	op->next = (MI)->mi_vfschain.next;                                \
	(MI)->mi_vfschain.next = op;                                      \
    } else { /* First entry, add it straight to mnttbl */                 \
	(MI)->mi_vfschain.vfsp = (VFS);                                   \
	(MI)->mi_vfschain.rootvp = (VP);                                  \
    }                                                                     \
} while (0)

/*
 * CFS structure to hold mount/file system information
 */
struct cfs_mntinfo {
    int			mi_refct;
    /*	struct vnode    *mi_ctlvp; */
    struct vcomm	mi_vcomm;
    char		*mi_name;      /* FS-specific name for this device */
    struct ody_mntinfo	mi_vfschain;   /* List of vfs mounted on this device */
};

extern struct cfs_mntinfo cfs_mnttbl[]; /* indexed by minor device number */


/*
 * vfs pointer to mount info
 */
#define vftomi(vfsp)    ((struct cfs_mntinfo *)((vfsp)->VFS_DATA))

/*
 * vnode pointer to mount info
 */
#define vtomi(vp)       ((struct cfs_mntinfo *)((VN_VFS(vp))->VFS_DATA))

#define	CFS_MOUNTED(vfsp)   (vftomi((vfsp)) != (struct cfs_mntinfo *)0)


/*
 * Used for identifying usage of "Control" object
 */
extern struct vnode *cfs_ctlvp;

#define	CFS_CONTROL		".CONTROL"
#define	CTL_VOL			-1
#define	CTL_VNO			-1
#define	CTL_UNI			-1

/* Acckkk! IS_ROOT_VP is currently a hack that assumes coda venus is
   only vfs on this mnttbl */

#define	IS_ROOT_VP(vp)		((vp) == vtomi((vp))->mi_vfschain.rootvp)
#define	IS_CTL_VP(vp)		((vp) == cfs_ctlvp)
#define CFS_CTL_VP		cfs_ctlvp

#define	IS_CTL_NAME(dvp, name)	(IS_ROOT_VP((dvp))                   \
				 && strcmp(name, CFS_CONTROL) == 0)

#define	IS_CTL_FID(fidp)	((fidp)->Volume == CTL_VOL &&\
				 (fidp)->Vnode == CTL_VNO &&\
				 (fidp)->Unique == CTL_UNI)

#define	ISDIR(fid)		((fid).Vnode & 0x1)

/* Some declarations of local utility routines */

extern int cfscall C_ARGS((struct cfs_mntinfo *, int , int *, char *));
extern struct cnode *makecfsnode  C_ARGS((ViceFid *, VFS_T *, short));
extern int handleDownCall C_ARGS((int opcode, struct outputArgs *out));
extern int cfs_grab_vnode C_ARGS((dev_t, ino_t, struct vnode **));
/*
 * Used to select debugging statements throughout the cfs code.
 */
extern int cfsdebug;
#define CFSDBGMSK(N)            (1 << N)
#define CFSDEBUG(N, STMT)       { if (cfsdebug & CFSDBGMSK(N)) { STMT } }

/* Prototypes of functions exported within cfs */
extern int  cfs_vmflush __P(());
extern void print_cfsnc __P(());
extern void cfsnc_init __P(());
extern int  cfsnc_resize __P((int, int));
extern void cfsnc_gather_stats __P(());
extern void cfs_flush __P(());
extern void cfs_testflush __P(());
extern void cfsnc_purge_user __P((struct ucred *));
extern void cfsnc_zapParentfid __P((ViceFid *));
extern void cfsnc_zapvnode __P((ViceFid *, struct ucred *));
extern void cfsnc_zapfid __P((ViceFid *));
extern void cfsnc_replace __P((ViceFid *, ViceFid *));
extern void cfs_save __P((struct cnode *));
extern void cfsnc_flush __P(());
extern int  cfs_vnodeopstats_init __P(());
extern int  cfs_kill __P((VFS_T *));
extern void cfs_unsave __P((struct cnode *));
extern int  getNewVnode __P((struct vnode **));
extern void print_vattr __P((struct vattr *));
extern void cfs_free __P((struct cnode *));
extern void cfsnc_enter __P((struct cnode *, char *, struct ucred *, 
			     struct cnode *));
extern void cfsnc_zapfile __P((struct cnode *, char *));

#endif	KERNEL

#endif !_CFS_HEADER_
