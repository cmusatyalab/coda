/*
 * Venus interface for Coda.
 * Original version: (C) 1996 Peter Braam 
 * Rewritten for Linux 2.1: (C) 1997 Carnegie Mellon University
 *
 * Carnegie Mellon encourages users of this code to contribute improvements
 * to the Coda project. Contact Peter Braam <coda@cs.cmu.edu>.
 */

/*
 *
 * Based on cfs.h from Mach, but revamped for increased simplicity.
 * Linux modifications by Peter Braam, Aug 1996
 */

#ifndef _CFS_HEADER_
#define _CFS_HEADER_



/* Catch new _KERNEL defn for NetBSD */
#ifdef __NetBSD__
#include <sys/types.h>
#ifdef _KERNEL
#define KERNEL
#endif 
#endif 

#if 0
#ifndef _SCALAR_T_
#define _SCALAR_T_ 1
typedef unsigned long  u_int32_t;
typedef unsigned short u_int16_t;
typedef unsigned char  u_int8_t;
#endif 
#endif 

#ifdef __linux__
#ifndef _UQUAD_T_
#define _UQUAD_T_ 1
typedef unsigned long  u_quad_t;
#endif 

#ifdef __KERNEL__
#define KERNEL
#endif __KERNEL__
#endif

/*
 * Cfs constants
 */
#define CFS_MAXNAMLEN 256
#define CFS_MAXPATHLEN 256
#define CODA_MAXSYMLINK 10

/* types used in kernel and user mode */
#ifndef _VENUS_DIRENT_T_
#define _VENUS_DIRENT_T_ 1
struct venus_dirent {
        unsigned long d_fileno;             /* file number of entry */
        unsigned short d_reclen;             /* length of this record */
        char  d_type;               /* file type, see below */
        char  d_namlen;             /* length of string in d_name */
        char     d_name[CFS_MAXNAMLEN + 1];/* name must be no longer than this */
};
#undef DIRSIZ
#define DIRSIZ(dp)      ((sizeof (struct venus_dirent) - (CFS_MAXNAMLEN+1)) + \
                         (((dp)->d_namlen+1 + 3) &~ 3))

/*
 * File types
 */
#define	DT_UNKNOWN	 0
#define	DT_FIFO		 1
#define	DT_CHR		 2
#define	DT_DIR		 4
#define	DT_BLK		 6
#define	DT_REG		 8
#define	DT_LNK		10
#define	DT_SOCK		12
#define	DT_WHT		14

/*
 * Convert between stat structure types and directory types.
 */
#define	IFTODT(mode)	(((mode) & 0170000) >> 12)
#define	DTTOIF(dirtype)	((dirtype) << 12)

#endif

#ifndef	_FID_T_
#define _FID_T_	1
typedef u_long VolumeId;
typedef u_long VnodeId;
typedef u_long Unique_t;
typedef u_long FileVersion;
#endif 

#ifndef	_VICEFID_T_
#define _VICEFID_T_	1
typedef struct ViceFid {
    VolumeId Volume;
    VnodeId Vnode;
    Unique_t Unique;
} ViceFid;
#endif	/* VICEFID */

#ifndef _VUID_T_
#define _VUID_T_
typedef u_long vuid_t;
typedef u_long vgid_t;
#endif /*_VUID_T_ */

#ifndef _CODACRED_T_
#define _CODACRED_T_
#define NGROUPS 32
struct CodaCred {
    vuid_t cr_uid, cr_euid, cr_suid, cr_fsuid; /* Real, efftve, set, fs uid*/
    vgid_t cr_gid, cr_egid, cr_sgid, cr_fsgid; /* same for groups */
    vgid_t cr_groups[NGROUPS];	      /* Group membership for caller */
};
#endif 

#ifndef _VENUS_VATTR_T_
#define _VENUS_VATTR_T_
/*
 * Vnode types.  VNON means no type.
 */
enum coda_vtype	{ VNON, VREG, VDIR, VBLK, VCHR, VLNK, VSOCK, VFIFO, VBAD };

struct coda_vattr {
	enum coda_vtype	va_type;	/* vnode type (for create) */
	u_short		va_mode;	/* files access mode and type */
	short		va_nlink;	/* number of references to file */
	vuid_t		va_uid;		/* owner user id */
	vgid_t		va_gid;		/* owner group id */
	long		va_fsid;	/* file system id (dev for now) */
	long		va_fileid;	/* file id */
	u_quad_t	va_size;	/* file size in bytes */
	long		va_blocksize;	/* blocksize preferred for i/o */
	struct timespec	va_atime;	/* time of last access */
	struct timespec	va_mtime;	/* time of last modification */
	struct timespec	va_ctime;	/* time file changed */
	u_long		va_gen;		/* generation number of file */
	u_long		va_flags;	/* flags defined for file */
	dev_t		va_rdev;	/* device the special file represents */
	u_quad_t	va_bytes;	/* bytes of disk space held by file */
	u_quad_t	va_filerev;	/* file modification number */
	u_int		va_vaflags;	/* operations flags, see below */
	long		va_spare;	/* remain quad aligned */
};
#define VREAD 00400
#define VWRITE 00200

#endif 

/*
 * opcode constants
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
#define	CFS_RDWR	((u_long) 31)
#define ODY_MOUNT	((u_long) 32) 
#define ODY_LOOKUP	((u_long) 33)
#define ODY_EXPAND	((u_long) 34)

#define CFS_NCALLS 35
#define DOWNCALL(opcode) (opcode >= CFS_REPLACE && opcode <= CFS_PURGEFID)

/*
 *        Venus <-> Coda  RPC arguments
 */

struct inputArgs {
    u_long opcode;
    u_long unique;     /* Keep multiple outstanding msgs distinct */
    u_short pid;		 /* Common to all */
    u_short pgid;		 /* Common to all */
    struct CodaCred cred;	 /* Common to all */
    
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
            struct coda_vattr attr;
	} cfs_getattr;
	struct cfs_setattr_in {
	    ViceFid VFid;
	    struct coda_vattr attr;
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
	    struct coda_vattr attr;
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
	    struct coda_vattr attr;
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
	    struct coda_vattr attr;
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
    
/*  Occasionally, don't cache the fid returned by CFS_LOOKUP. For
 * instance, if the fid is inconsistent. This case is handled by
 * setting the top bit of the return result parameter.  */
#define CFS_NOCACHE          0x80000000

#define INIT_OUT(out, opcode, result) \
    out->opcode = (opcode); out->result = (result);

/*  IMPORTANT: opcode and unique must be first two fields! */
struct outputArgs {
    u_long opcode;
    u_long unique;	 /* Keep multiple outstanding msgs distinct */
    u_long result;
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
	    struct coda_vattr attr;
	} cfs_getattr;
	/* Nothing needed for cfs_setattr */
	/* Nothing needed for cfs_access */
	struct cfs_lookup_out {
	    ViceFid VFid;
	    int	vtype;
	} cfs_lookup;
	struct cfs_create_out {
	    ViceFid VFid;
	    struct coda_vattr attr;
	} cfs_create;
	/* Nothing needed for cfs_remove */
	/* Nothing needed for cfs_link */
	/* Nothing needed for cfs_rename */
	struct cfs_mkdir_out {
	    ViceFid VFid;
	    struct coda_vattr attr;
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
 * how big are the inputArgs and outputArgs structures
 * for the varying types of calls?
 */
#define	VC_IN_NO_DATA	    (2 * (int)sizeof(u_long)    \
                             + 2 * (int)sizeof(u_short) \
			     + (int)sizeof(struct CodaCred))
#define	VC_OUT_NO_DATA	    (3 * (int)sizeof(u_long))
#define VC_INSIZE(member)   (VC_IN_NO_DATA + (int)sizeof(struct member))
#define VC_OUTSIZE(member)  (VC_OUT_NO_DATA + (int)sizeof(struct member))

/* Now for venus. C++ doesn't know what struct foo means. */
#define VC_SIZE(Thing, Member)   (VC_OUT_NO_DATA                    \
                                  + (int)sizeof((Thing)->d.Member))

#define VC_BIGGER_OF_IN_OR_OUT  (sizeof(struct outputArgs)   \
                                  > sizeof(struct inputArgs) \
                                ? sizeof(struct outputArgs)  \
                                : sizeof(struct inputArgs))
#define VC_DATASIZE	    8192
#define	VC_MAXMSGSIZE	    (VC_DATASIZE + VC_BIGGER_OF_IN_OR_OUT)

/*
 * Used for identifying usage of "Control" and pioctls
 */
struct ViceIoctl {
        caddr_t in, out;        /* Data to be transferred in, or out */
        short in_size;          /* Size of input buffer <= 2K */
        short out_size;         /* Maximum size of output buffer, <= 2K */
};

struct PioctlData {
        const char *path;
        int follow;
        struct ViceIoctl vi;
};






#define	CFS_CONTROL		".CONTROL"
#define CFS_CONTROLLEN           8
#define	CTL_VOL			-1
#define	CTL_VNO			-1
#define	CTL_UNI			-1
#define CTL_INO                 -1
#define	CTL_FILE    "/coda/.CONTROL"
#define IOCPARM_MASK 0x0000ffff


#define	IS_CTL_FID(fidp)	((fidp)->Volume == CTL_VOL &&\
				 (fidp)->Vnode == CTL_VNO &&\
				 (fidp)->Unique == CTL_UNI)
     /*#define	ISDIR(fid)		((fid).Vnode & 0x1) */

#endif 

