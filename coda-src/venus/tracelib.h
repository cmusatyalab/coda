#ifndef _BLURB_
#define _BLURB_
/*

    DFStrace: an Experimental File Reference Tracing Package

       Copyright (c) 1990-1995 Carnegie Mellon University
                      All Rights Reserved.

Permission  to use, copy, modify and distribute this software and
its documentation is hereby granted (including for commercial  or
for-profit use), provided that both the copyright notice and this
permission  notice  appear  in  all  copies  of   the   software,
derivative  works or modified versions, and any portions thereof,
and that both notices appear  in  supporting  documentation,  and
that  credit  is  given  to  Carnegie  Mellon  University  in all
publications reporting on direct or indirect use of this code  or
its derivatives.

DFSTRACE IS AN EXPERIMENTAL SOFTWARE PACKAGE AND IS KNOWN TO HAVE
BUGS, SOME OF WHICH MAY  HAVE  SERIOUS  CONSEQUENCES.    CARNEGIE
MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.
CARNEGIE MELLON DISCLAIMS ANY  LIABILITY  OF  ANY  KIND  FOR  ANY
DAMAGES  WHATSOEVER RESULTING DIRECTLY OR INDIRECTLY FROM THE USE
OF THIS SOFTWARE OR OF ANY DERIVATIVE WORK.

Carnegie Mellon encourages (but does not require) users  of  this
software to return any improvements or extensions that they make,
and to grant Carnegie Mellon the  rights  to  redistribute  these
changes  without  encumbrance.   Such improvements and extensions
should be returned to Software.Distribution@cs.cmu.edu.

*/

static char *rcsid = "$Header: /afs/cs.cmu.edu/project/coda-braam/src/coda-4.0.1/RCSLINK/./coda-src/venus/tracelib.h,v 1.1 1996/11/22 19:11:38 braam Exp $";
#endif _BLURB_

/* 
 * tracelib.h -- exported information about traces and the trace library: 
 * record formats, opcodes, error codes, functions etc.
 */

#ifndef _TRACELIB_H_
#define _TRACELIB_H_

#include <sys/types.h>
#include <sys/time.h>

/* record opcodes */
#define DFS_UNUSED	0x0
#define DFS_OPEN	0x01
#define DFS_CLOSE	0x02
#define DFS_STAT	0x03
#define DFS_LSTAT	0x04
#define DFS_SEEK        0x05
#define DFS_EXECVE	0x06
#define DFS_EXIT	0x07
#define DFS_FORK	0x08
#define DFS_CHDIR	0x09
#define DFS_UNLINK	0x0a
#define	DFS_ACCESS	0x0b
#define DFS_READLINK	0x0c
#define DFS_CREAT	0x0d
#define DFS_CHMOD	0x0e
#define DFS_SETREUID    0x0f
#define DFS_RENAME	0x10
#define DFS_RMDIR	0x11
#define DFS_LINK	0x12
#define DFS_CHOWN	0x13
#define DFS_MKDIR	0x14
#define DFS_SYMLINK	0x15
#define DFS_SETTIMEOFDAY 0x16
#define DFS_MOUNT	0x17
#define DFS_UNMOUNT	0x18
#define DFS_TRUNCATE	0x19
#define DFS_CHROOT	0x1a
#define DFS_MKNOD       0x1b
#define DFS_UTIMES      0x1c
#define DFS_READ        0x1d
#define DFS_WRITE       0x1e
#define DFS_LOOKUP      0x1f
#define DFS_GETSYMLINK  0x20
#define DFS_ROOT        0x21
#define DFS_SYSCALLDUMP 0x22
#define DFS_NOTE        0x23
#define DFS_SYNC        0x24   /* used in Ousterhout's traces */
#define DFS_FSYNC       0x25   

/* record flags */
#define DFS_POST        0x01   /* post record, for records with two parts */
#define DFS_NOATTR      0x02   /* error getting attributes (VOP_GETATTR) */
#define DFS_NOFID       0x04   /* error getting fid (VOP_GETFID or VOP_GETATTR) */
#define DFS_NOPATH      0x08   /* error getting path (pn_get) */
#define DFS_TRUNCATED   0x10   /* pathname too long */
#define DFS_NOFID2      0x20   /* for records with two fids */
#define DFS_NOPATH2     0x40   /* for records with two paths */
#define DFS_TRUNCATED2  0x80   /* for records with two paths */

/* 
 * file system types.  These correspond to inode types for the
 * traces accepted by this library, but they may not correspond
 * to the same inode or file system codes on the system analyzing 
 * the traces.  We define them here and keep them out of the way.
 */
#define	DFS_ITYPE_UFS	0
#define	DFS_ITYPE_NFS	1
#define	DFS_ITYPE_AFS	2
#define	DFS_ITYPE_BDEV	3
#define	DFS_ITYPE_SPEC	4
#define DFS_ITYPE_CFS	5

/* file types - see comment above */
#define DFS_IFMT	0170000		/* type of file */
#define DFS_IFCHR	0020000		/* character special */
#define DFS_IFDIR	0040000		/* directory */
#define DFS_IFBLK	0060000		/* block special */
#define DFS_IFREG	0100000		/* regular */
#define DFS_IFLNK	0120000		/* symbolic link */
#define DFS_IFSOCK	0140000		/* socket */

/* open flags - see comment above */
#define DFS_FOPEN	(-1)
#define DFS_FREAD	00001		/* descriptor read/receive'able */
#define DFS_FWRITE	00002		/* descriptor write/send'able */
#define DFS_FNDELAY	00004		/* no delay */
#define DFS_FAPPEND	00010		/* append on each write */
#define DFS_FMARK	00020		/* mark during gc() */
#define DFS_FDEFER	00040		/* defer for next gc pass */
#define DFS_FASYNC	00100		/* signal pgrp when data ready */
#define DFS_FSHLOCK	00200		/* shared lock present */
#define DFS_FEXLOCK	00400		/* exclusive lock present */
#define DFS_FMASK	00113		/* bits to save after open */
#define DFS_FCNTLCANT	(FREAD|FWRITE|FMARK|FDEFER|FSHLOCK|FEXLOCK)
/* open only modes */
#define DFS_FCREAT	01000		/* create if nonexistant */
#define DFS_FTRUNC	02000		/* truncate to zero length */
#define DFS_FEXCL	04000		/* error if already created */
#define DFS_FNOSPC	010000		/* disable resource pause for file */
#define	DFS_FOKDIR	020000		/* >1 dirents ok for getdirentries */


/* other constants */
#define DFS_MAXFIDS 4	       /* max number of fids in a record */
#define DFS_MAXPATHS 2         /* max number of path names in a record */
#define DFS_MAXSYSCALL 0x1e    /* max number of syscalls we have, all versions */
#define DFS_MAXOPCODE 0x25     /* max number of opcodes we have, all versions */
#define DFS_MAXRECORDSIZE 1024
#define DFS_DUMPINTERVAL 60    /* seconds */

/* trace versions */
#define TRACE_VERSION_CMU1  912     /* year and month of the trace experiments */
#define TRACE_VERSION_CMU2  915
#define TRACE_VERSION_CMU3  919
#define TRACE_VERSION_UCB1  851     /* Ousterhout's traces, 1985 */
#define TRACE_VERSION_UR    863     /* Floyd's traces, 1986 */

/* library error codes */
#define TRACE_SUCCESS            0
#define TRACE_FILENOTFOUND       1 /* couldn't find or open the file */
#define TRACE_FILERECORDNOTFOUND 2 /* lib struct for trace file not found */
#define TRACE_FILEREADERROR      3 /* couldn't read chunk, header, etc. */
#define TRACE_BADVERSION         4 /* bad or unknown trace version */


/* 
 * All trace records contain the same 16-byte header. Opcodes and flags are defined 
 * above. The record length is the length of the record, including that
 * field. The error code is basically u.u_error.
 */

/* header */
typedef struct dfs_header {
	u_char 	opcode;			
	u_char  flags;    /* tell you if various fields are bad */
	u_char  error;    /* usually u.u_error */
	short   pid; 
	struct  timeval	time;
} dfs_header_t;

/* 
 * File Ids (fids). Fids are either 8, 12, or 16 bytes long, depending on
 * whether the file is local, in coda, or in afs.  The types of various
 * fids in the record are stored in the tag field of the generic_fid structure 
 * (defined below) by the trace I/O routines. If the fid is not present (valid), 
 * the tag is -1. 
 */

struct local_fid {
        long   device;   /* from vattr.va_fsid, a long */
	long   number;   /* from vattr.va_nodeid, a long */
};
  
struct vice_fid {
	u_long Volume;
	u_long Vnode;
	u_long Unique;
};

struct venus_fid {
	long Cell;
	struct vice_fid Fid;
};

typedef struct generic_fid {
	char tag;
	union {
		struct local_fid local;  /* device and inode number for local files. */
		struct vice_fid cfs;  /* coda fids */
		struct venus_fid afs;
	} value;
} generic_fid_t;

/* 
 * record bodies.
 */

/* tallies of system calls */
struct dfs_call {   
	dfs_header_t header;
	u_int  count[DFS_MAXSYSCALL+1];  /* for easy indexing */
};

/* close */
struct dfs_close {
	dfs_header_t  header;
	generic_fid_t fid;
	short 	      fd;
	u_short	      numReads;
	u_short	      numWrites;
	u_short	      numSeeks;
	u_long	      bytesRead;
	u_long	      bytesWritten;
	u_long        oldSize; /* size at open */
	u_long	      size;    /* size at close */
	u_long        offset;
	u_short       fileType;
	short         refCount;
	short         flags;   /* open flags (-FOPEN) */
	short         whence;  /* how the file was closed (as part of exit, etc) */
	short         findex;
	char         *path;
};

/* rmdir */
struct dfs_rmdir {
	dfs_header_t  header;
	generic_fid_t fid;
	generic_fid_t dirFid;
	u_long        size;
	u_short       fileType;
	short         numLinks;
	u_short       pathLength;
	char         *path;
};

/* unlink -- same as rmdir */

/* unmount */
struct dfs_unmount {
	dfs_header_t  header;
	generic_fid_t fid;
	u_short       pathLength;
	char         *path;
};

/* open */
struct dfs_open {
	dfs_header_t  header;
	u_short       flags;     /* file mode (-FOPEN) */
	u_short       mode;      /* create mode */
	short         fd;
	u_short       fileType;  /* file type (dir, link, etc.) and mode */
	uid_t         uid;       /* file owner */
	long          oldSize;   /* if file already there */
	u_long        size;
	generic_fid_t fid;
	generic_fid_t dirFid;
	short         findex;
	u_short       pathLength;
	char         *path;
};
	
/* stat */
struct dfs_stat {
	dfs_header_t  header;
	generic_fid_t fid;
	u_short       fileType;  /* file type (dir, link, etc.) and mode */
	u_short	      pathLength;
	char         *path;
};

/* lstat -- same as stat */

/* chdir */
struct dfs_chdir {
	dfs_header_t  header;
	generic_fid_t fid;
	u_short	      pathLength;
	char         *path;
};

/* chroot -- same as chdir */
/* readlink -- same as chdir */

/* execve */
struct dfs_execve {
	dfs_header_t  header;
	u_long	      size;
	generic_fid_t fid;
	uid_t	      owner;  /* owner uid */
	uid_t         euid;   /* for ucb traces */
	uid_t         ruid;   
	u_short	      pathLength;
	char         *path;
};

/* access */
struct dfs_access { 
	dfs_header_t  header;
	generic_fid_t fid;
	u_short       mode;
	u_short       fileType;  /* file type (dir, link, etc) */
	u_short       pathLength;
	char         *path;
};

/* chmod -- same as access */

/* creat */
struct dfs_creat {	
	dfs_header_t  header;
	generic_fid_t fid;
	generic_fid_t dirFid;
	long          oldSize;   /* if object already there. at end size is 0. */
	short	      fd; 
	short         flags;    /* for ucb traces */
	uid_t         uid;      /* ditto */
	u_short       mode;
	short         findex;
	u_short       pathLength;
	char         *path;
};

/* mkdir */
struct dfs_mkdir {	
	dfs_header_t  header;
	generic_fid_t fid;
	generic_fid_t dirFid;
	u_short       mode;
	u_short       pathLength;
	char         *path;
};

/* chown */
struct dfs_chown {
	dfs_header_t  header;
	uid_t	      owner;
	gid_t         group;
	generic_fid_t fid;
	u_short       fileType;  /* file type (dir, link, etc) */
	u_short	      pathLength;
	char         *path;
};

/* rename */
struct dfs_rename {
	dfs_header_t  header;
	generic_fid_t fromFid;
	generic_fid_t fromDirFid;
	generic_fid_t toFid;
	generic_fid_t toDirFid;
	u_long        size;
	u_short       fileType;  /* file type (dir, link, etc.) */
	short         numLinks; 
	u_short       fromPathLength;
	u_short       toPathLength;
	char         *fromPath;
	char         *toPath;
};

/* link */
struct dfs_link {
	dfs_header_t  header;
	generic_fid_t fromFid;
	generic_fid_t fromDirFid;
	generic_fid_t toDirFid;
	u_short       fileType;
	u_short       fromPathLength;
	u_short       toPathLength;
	char         *fromPath;
	char         *toPath;
};

/* symlink */
struct dfs_symlink {
	dfs_header_t  header;
	generic_fid_t dirFid;
	generic_fid_t fid;
	u_short       targetPathLength;
	u_short       linkPathLength;
	char         *targetPath;
	char         *linkPath;
};

/* truncate */
struct dfs_truncate { 
	dfs_header_t  header;
	generic_fid_t fid;
	long          oldSize;
	u_long	      newSize;
	u_short	      pathLength;
	char         *path;
};

/* utimes */
struct dfs_utimes {
	dfs_header_t   header;
	generic_fid_t  fid;
	struct timeval atime;
	struct timeval mtime;
	u_short        fileType;  /* file type (dir, link, etc) */
	u_short	       pathLength;
	char          *path;
};

/* mknod */
struct dfs_mknod { 
	dfs_header_t  header;
	generic_fid_t fid;
	generic_fid_t dirFid;
	int           dev;
	u_short       mode;
	u_short       pathLength;
	char         *path;
};
	
/* mount */
struct dfs_mount {
	dfs_header_t  header;
	generic_fid_t fid;
	int	      rwflag;
	u_short       pathLength;
	char         *path;
};

/* seek */
struct dfs_seek {
	dfs_header_t  header;
	generic_fid_t fid;
	short 	      fd;
	short         findex;
	u_short	      numReads;
	u_short	      numWrites;
	u_int	      bytesRead;
	u_int	      bytesWritten;
	u_int         offset;
	u_int         oldOffset;
	char         *path;
};

/* settimeofday */
struct dfs_settimeofday {
	dfs_header_t header;
};

/* exit */
struct dfs_exit {
	dfs_header_t header;
};

/* fork */
struct dfs_fork {
	dfs_header_t header;
	short	     childPid;
	uid_t 	     userId;
};

/* setreuid */
struct dfs_setreuid {
	dfs_header_t header;
	uid_t	     ruid;
	uid_t	     euid;
};

/* pathname lookup records */
/* lookup */
struct dfs_lookup {
	dfs_header_t  header;
        generic_fid_t compFid;
	generic_fid_t parentFid;
	u_short       fileType;
        u_short       pathLength;
	char         *path;
};

/* getsymlink */
struct dfs_getsymlink {
	dfs_header_t  header;
	generic_fid_t fid;
	u_short       compPathLength;
	u_short       pathLength;  
	char         *compPath;
	char         *path;  /* link name */
};

/* mount point */
struct dfs_root {
	dfs_header_t  header;
	generic_fid_t compFid;
	generic_fid_t targetFid;
        u_short       pathLength;
	char         *path;
};

/* reads and writes */
struct dfs_read {
	dfs_header_t  header;
	generic_fid_t fid;
	short         fd;
	short         findex;
	u_int         amount;
	char         *path;
};

/* notes */
struct dfs_note {
	dfs_header_t header;
	short        length;
	char        *note;
};

/* sync and fsync -- for ucb traces */
struct dfs_sync {
	dfs_header_t header;
};

struct dfs_fsync {
	dfs_header_t header;
	short        findex;
};


/* variables */
extern char verbose;
extern char debug;

typedef struct trace_stat {
	struct timeval     firstTraceRecordTime;
	struct timeval     lastTraceRecordTime; /* time of last record read */
	u_long             totalRecords;        /* records read, including splits */
	u_long             recordsRead;         /* user-presentable records read */
	u_long             recordsUsed;         /* records not filtered out */
	u_long             totalBytes;          /* total bytes in trace */
	u_long             recordBytes;         /* bytes in all records thus far */
} trace_stat_t;

/* functions */
#ifndef C_ARGS
#if c_plusplus
#define C_ARGS(arglist) arglist
#else c_plusplus
#define C_ARGS(arglist) ()
#endif c_plusplus
#endif C_ARGS

extern FILE *Trace_Open C_ARGS((char *name));
extern int Trace_Close C_ARGS((FILE *fp));
extern dfs_header_t *Trace_GetRecord C_ARGS((FILE *fp));
extern int Trace_FreeRecord C_ARGS((FILE *fp, dfs_header_t *recPtr));
extern int Trace_SetFilter C_ARGS((FILE *fp, char *fileName));
extern int Trace_Stats C_ARGS((FILE *fp, trace_stat_t *statPtr));
extern int Trace_PrintPreamble C_ARGS((FILE *fp));
extern int Trace_GetVersion C_ARGS((FILE *fp, char *vp));

extern char *Trace_NodeIdToStr C_ARGS((int addr));
extern char *Trace_OpcodeToStr C_ARGS((u_char opcode));
extern char *Trace_FlagsToStr 	C_ARGS((u_char flags));
extern char *Trace_InodeTypeToStr C_ARGS((int type));
extern char *Trace_FidPtrToStr C_ARGS((generic_fid_t *fidPtr));
extern char *Trace_OpenFlagsToStr C_ARGS((u_short flags));
extern char *Trace_RecTimeToStr C_ARGS((dfs_header_t *recPtr));
extern char *Trace_FileTypeToStr C_ARGS((u_short type));
extern void Trace_PrintRecord C_ARGS((dfs_header_t *recPtr));
extern void Trace_DumpRecord C_ARGS((dfs_header_t *recPtr));

extern int Trace_FidsEqual C_ARGS((generic_fid_t *f1, generic_fid_t *f2));
extern void Trace_CopyRecord C_ARGS((dfs_header_t *sp, dfs_header_t *dpp));

extern int Trace_GetUser C_ARGS((FILE *fp, short pid, uid_t *uidp));
extern short Trace_GetFileType C_ARGS((dfs_header_t *recPtr));
extern short Trace_GetFileIndex C_ARGS((dfs_header_t *recPtr));
extern short Trace_GetRefCount C_ARGS((dfs_header_t *recPtr));
extern void Trace_GetFid C_ARGS((dfs_header_t *recPtr, 
				  generic_fid_t **fidpList, int *num));
extern void Trace_GetPath C_ARGS((dfs_header_t *recPtr,
				  char **pathplist, int *num));

/* macros */
/* fid conversion macros */
#define MAKE_AFS_FID(_gfid_, _afid_) /* _gfid_ a generic_fid_t, _afid_ a venus_fid */ \
	switch (_gfid_.tag) {                                                 \
	case DFS_ITYPE_AFS:  /* copy */                                       \
		_afid_.Cell = _gfid_.value.afs.Cell;                          \
		_afid_.Fid.Volume = _gfid_.value.afs.Fid.Volume;              \
		_afid_.Fid.Vnode = _gfid_.value.afs.Fid.Vnode;                \
		_afid_.Fid.Unique = _gfid_.value.afs.Fid.Unique;              \
		break;                                                        \
	case DFS_ITYPE_CFS:                                                   \
		_afid_.Cell = 0xffffffff;  /* fake a cell */                  \
		_afid_.Fid.Volume = _gfid_.value.cfs.Volume;                  \
		_afid_.Fid.Vnode = _gfid_.value.cfs.Vnode;                    \
		_afid_.Fid.Unique = _gfid_.value.cfs.Unique;                  \
		break;                                                        \
	case DFS_ITYPE_UFS:                                                   \
	case DFS_ITYPE_NFS:                                                   \
		_afid_.Cell = 0xffffffff;  /* fake a cell */                  \
		_afid_.Fid.Volume = _gfid_.value.local.device;                \
		_afid_.Fid.Vnode = _gfid_.value.local.number;                 \
		_afid_.Fid.Unique = 0xffffffff; /* fake a uniquifier */       \
		break;                                                        \
	default:  /* fake everything. we shouldn't get here. */               \
		_afid_.Cell = 0xffffffff;                                     \
		_afid_.Fid.Volume = 0xffffffff;                               \
		_afid_.Fid.Vnode = 0xffffffff;                                \
		_afid_.Fid.Unique = 0xffffffff; /* fake a uniquifier */       \
		break;                                                        \
	}

#define MAKE_CFS_FID(_gfid_, _cfid_) /* _gfid_ a generic_fid_t, _cfid_ a vice_fid */  \
	switch (_gfid_.tag) {                                                 \
	case DFS_ITYPE_AFS: /* lop off the cell. */                           \
		_cfid_.Volume = _gfid_.value.afs.Fid.Volume;                  \
		_cfid_.Vnode = _gfid_.value.afs.Fid.Vnode;                    \
		_cfid_.Unique = _gfid_.value.afs.Fid.Unique;                  \
		break;                                                        \
	case DFS_ITYPE_CFS:                                                   \
		_cfid_.Volume = _gfid_.value.cfs.Volume;                      \
		_cfid_.Vnode = _gfid_.value.cfs.Vnode;                        \
		_cfid_.Unique = _gfid_.value.cfs.Unique;                      \
		break;                                                        \
	case DFS_ITYPE_UFS:                                                   \
	case DFS_ITYPE_NFS:                                                   \
		_cfid_.Volume = _gfid_.value.local.device;                    \
		_cfid_.Vnode = _gfid_.value.local.number;                     \
		_cfid_.Unique = 0xffffffff;                                   \
		break;                                                        \
	default:                                                              \
		_cfid_.Volume = 0xffffffff;                                   \
		_cfid_.Vnode = 0xffffffff;                                    \
		_cfid_.Unique = 0xffffffff;                                   \
		break;                                                        \
	}

#define MAKE_UFS_FID(_gfid_, _ufid_) /* _gfid_ a generic_fid_t, _ufid_ a local_fid */ \
	switch (_gfid_.tag) { /* I don't seriously expect anyone to do this */\
	case DFS_ITYPE_AFS:                                                   \
		_ufid_.device = _gfid_.value.afs.Fid.Volume;                  \
		_ufid_.number = _gfid_.value.afs.Fid.Vnode;                   \
		break;                                                        \
	case DFS_ITYPE_CFS:                                                   \
		_ufid_.device = _gfid_.value.cfs.Volume;                      \
                _ufid_.number = _gfid_.value.cfs.Vnode;                       \
		break;                                                        \
	case DFS_ITYPE_UFS:                                                   \
	case DFS_ITYPE_NFS:                                                   \
                _ufid_.device = _gfid_.value.local.device;                    \
		_ufid_.number = _gfid_.value.local.number;                    \
		break;                                                        \
	default:                                                              \
                _ufid_.device = 0xffffffff;                                   \
                _ufid_.number = 0xffffffff;                                   \
		break;                                                        \
	}

#endif _TRACELIB_H_
