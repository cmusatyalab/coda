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

static char *rcsid = "$Header: /afs/cs/project/coda-nbsd-port/coda-4.0.1/OBJS/coda-src/venus/RCS/vproc.h,v 1.2 1996/11/25 17:50:14 braam Exp satya $";
#endif /*_BLURB_*/







/*
 *
 * Specification of the Venus process abstraction
 *
 */

#ifndef _VENUS_PROC_H_
#define _VENUS_PROC_H_ 1


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/uio.h>

#ifdef __NetBSD__
#define __attribute__(x)    /* dummied out because of machine/segments.h */
#include <sys/user.h>
#undef __attribute__
#else
#include <sys/user.h>
#endif __NetBSD__

#ifdef __MACH__
/* Pick up private versions of vnode headers from vicedep */
#include <vfs/vfs.h>
#include <vfs/vnode.h>
#endif __MACH__

#ifdef __NetBSD__
/* Pick up system versions of vnode headers from /usr/include */
/* #include <sys/mount.h> (Satya, 11/25/96) */
#include "venus_vnode.h"
#endif __NetBSD__

#ifdef LINUX
#include <sys/uio.h>
        /* hmm we need this, so let's define it. Where is it in BSD anyway? */
enum  uio_rw { UIO_READ, UIO_WRITE };
struct uio {
        struct  iovec *uio_iov;
        int     uio_iovcnt;
        off_t   uio_offset;
        int     uio_resid;
        enum    uio_rw uio_rw;
};

#define MAX(a,b)   ( (a) > (b) ? (a) : (b))
#define MIN(a,b)   ( (a) < (b) ? (a) : (b))

#endif LINUX

#include <cfs/cfs.h>
#include <cfs/cnode.h>

#include <lwp.h>
#include <lock.h>

#ifdef __cplusplus
}
#endif __cplusplus

/* interfaces */
#include <vice.h>

/* from rvm */
#include <rvmlib.h>

/* from util */
#include <olist.h>
#include <dlist.h>
#include <rec_dlist.h>

/* from vicedep */
#include <venusioctl.h>

/* from venus */
#include "venus.private.h"


/* Forward declarations. */
struct uarea;
class vproc;
class vproc_iterator;

/* C++ Bogosity. */
extern void PrintVprocs();
extern void PrintVprocs(FILE *);
extern void PrintVprocs(int);


/* *****  Exported constants  ***** */

const int VPROC_DEFAULT_STACK_SIZE = 16384;
const int RETRY_LIMIT = 10;


/* *****  Exported types  ***** */

/* local-repair modification */
enum vproctype {    VPT_Main,
		    VPT_Worker,
		    VPT_Mariner,
		    VPT_CallBack,
		    VPT_HDBDaemon,
		    VPT_Reintegrator,
		    VPT_Resolver,
		    VPT_FSODaemon,
		    VPT_ProbeDaemon,
		    VPT_VSGDaemon,
		    VPT_VolDaemon,
		    VPT_UserDaemon,
		    VPT_RecovDaemon,
		    VPT_VmonDaemon,
		    VPT_Simulator,
		    VPT_AdviceDaemon,
		    VPT_LRDaemon
};

/* Holds user/call specific context. */
class namectxt;
class volent;
struct uarea {
    int	u_error;		/* implicit return code */
    struct ucred u_cred;	/* implicit user identifier */
    int	u_priority;		/* to be used in resource requests */
    ViceFid u_cdir;		/* for name lookup */
    int	u_flags;		/*  "	" */
    namectxt *u_nc;		/*  "	" */
    volent *u_vol;		/* for volume-level concurrency control */
    int	u_volmode;		/*  "	" */
    int	u_vfsop;		/* vfs operation in progress */
#ifdef	TIMING
    struct timeval u_tv1;	/* for recording elapsed time */
    struct timeval u_tv2;	/*  "	" */
#endif	TIMING
    char *u_resblk;		/* block to wait on for resolves */
    int	u_rescnt;		/* safeguard against infinite retry loops */
    int	u_retrycnt;		/* safeguard against infinite retry loops */
    int	u_wdblkcnt;		/* safeguard against infinite retry loops */

    int u_pid;                  /* the process id of the calling process */ 
    int u_pgid;                 /* the process group id of the calling process */

    /* Initialization. */
    void Init() {
	bzero(this, (int) sizeof(struct uarea));
	u_volmode = /*VM_UNSET*/-1;
	u_vfsop = /*VFSOP_UNSET*/-1;
    }
};

typedef void (*PROCBODY)(void *);

class vproc : public olink {
  friend void VprocInit();
  friend void Rtry_Wait();
  friend void Rtry_Signal();
  friend vproc *FindVproc(int);
  friend void VprocPreamble(struct Lock *);
  friend vproc *VprocSelf();
  friend int VprocIdle();
  friend int VprocInterrupted();
  friend void PrintVprocs(int);
  friend class vproc_iterator;
  friend void PrintWorkers(int);
  friend void PrintMariners(int);

  private:
    static olist tbl;
    static int counter;
    static char rtry_sync;

    void do_ioctl(ViceFid *, unsigned int, struct ViceIoctl *);

  protected:
    int lwpid;
    char *name;
    PROCBODY func;
    int vpid;
    rvm_perthread_t rvm_data;
    struct Lock init_lock;

  public:
    /* Public for the time being. -JJK */
    vproctype type;
    int seq;
    struct uarea u;
    unsigned idle : 1;
    unsigned interrupted : 1;
    unsigned prefetch : 1;	/* For SETS-style prefetching */
    struct vcbevent *ve;

    vproc(char *, PROCBODY, vproctype, int =VPROC_DEFAULT_STACK_SIZE, int =LWP_NORMAL_PRIORITY);
    operator=(vproc&);	/* not supported! */
    virtual ~vproc();

    /* Volume-level concurrency control. */
    void Begin_VFS(VolumeId, int, int =-1);
    void End_VFS(int * =0);

    /* The VFS interface.  */

/*  Note: all references to struct vfs eliminated; see vproc_vfscalls.c (Satya, 8/15/96) */
    void mount(char *, void *);
    void unmount();
    void root(struct vnode **);
    void statfs(struct statfs *);
    void sync();
    void vget(struct vnode **, struct fid *);
    void open(struct vnode **, int);
    void close(struct vnode *, int);
    void rdwr(struct vnode *, struct uio *, enum uio_rw, int);
    void ioctl(struct vnode *, unsigned int, struct ViceIoctl *, int);
    void select(struct vnode *, int);
    void getattr(struct vnode *, struct vattr *);
    void setattr(struct vnode *, struct vattr *);
    void access(struct vnode *, int);
    void lookup(struct vnode *, char *, struct vnode **);
    void create(struct vnode *, char *, struct vattr *, int, int, struct vnode **);
    void remove(struct vnode *, char *);
    void link(struct vnode *, struct vnode *, char *);
    void rename(struct vnode *, char *, struct vnode *, char *);
    void mkdir(struct vnode *, char *, struct vattr *, struct vnode **);
    void rmdir(struct vnode *, char *);
    void readdir(struct vnode *, struct uio *);
    void symlink(struct vnode *, char *, struct vattr *, char *);
    void readlink(struct vnode *, struct uio *);
    void fsync(struct vnode *);
    void inactive(struct vnode *);
    void fid(struct vnode *, struct fid	**);

    /* Pathname translation. */
    int namev(char *, int, struct vnode **);
    void GetPath(ViceFid *, char *, int *, int =1);

    void GetStamp(char *);
    void print();
    void print(FILE *);
    void print(int);
};


class vproc_iterator : public olist_iterator {
    vproctype type;

  public:
    vproc_iterator(vproctype =(vproctype)-1);
    vproc *operator()();
};


/* *****  Exported routines  ***** */

extern void VprocInit();
extern void Rtry_Wait();
extern void Rtry_Signal();
extern vproc *FindVproc(int);
extern void VprocPreamble(struct Lock *);
extern vproc *VprocSelf();
extern void VprocWait(char *);
extern void VprocMwait(int, char **);
extern void VprocSignal(char *, int=0);
extern void VprocSleep(struct timeval *);
extern void VprocYield();
extern int VprocSelect(int, int *, int *, int *, struct timeval *);
extern void VprocSetRetry(int =-1, struct timeval * =0);
extern int VprocIdle();
extern int VprocInterrupted();
//extern void PrintVprocs();
//extern void PrintVprocs(FILE *);
//extern void PrintVprocs(int);


/* Things which should be in vnode.h? -JJK */

extern void va_init(struct vattr *);
extern void VattrToStat(struct vattr *, struct stat *);
extern long FidToNodeid(ViceFid *);

#ifdef __MACH__
#define	CRTOEUID(cred)	((vuid_t)((cred).cr_uid))
#define	CRTORUID(cred)	((vuid_t)((cred).cr_ruid))
#endif __MACH__

#if __NetBSD__ || LINUX
/* vnodes in NetBSD don't seem to store effective user & group ids.  So just
   coerce everything to uid */
#define	CRTOEUID(cred)	((vuid_t)((cred).cr_uid))
#define	CRTORUID(cred)	((vuid_t)((cred).cr_uid))
#endif __NetBSD__

#define	FTTOVT(ft)	((ft) == (int)File ? VREG :\
			 (ft) == (int)Directory ? VDIR :\
			 (ft) == (int)SymbolicLink ? VLNK :\
			 VREG)

/* VN_INIT() only defined for Mach; not used in NetBSD */
#ifdef __MACH__
#define	VN_INIT(VP, VFSP, TYPE,	DEV)	{\
    (VP)->v_flag = 0;\
    (VP)->v_count = 1;\
    (VP)->v_shlockc = (VP)->v_exlockc = 0;\
    (VP)->v_vfsp = (VFSP);\
    (VP)->v_mode = (/*0 | */(TYPE & VFMT));\
    (VP)->v_rdev = (DEV);\
    (VP)->v_socket = 0;\
}
#endif __MACH__

#define	VFSOP_UNSET	-1
#define	VFSOP_MOUNT	/*CFS_MOUNT*/VFSOP_UNSET
#define	VFSOP_UNMOUNT	/*CFS_UNMOUNT*/VFSOP_UNSET
#define	VFSOP_ROOT	CFS_ROOT
#define	VFSOP_STATFS	/*CFS_STATFS*/VFSOP_UNSET
#define	VFSOP_SYNC	CFS_SYNC
#define	VFSOP_VGET	CFS_VGET
#define	VFSOP_OPEN	CFS_OPEN
#define	VFSOP_CLOSE	CFS_CLOSE
#define	VFSOP_RDWR	CFS_RDWR
#define	VFSOP_IOCTL	CFS_IOCTL
#define	VFSOP_SELECT	/*CFS_SELECT*/VFSOP_UNSET
#define	VFSOP_GETATTR	CFS_GETATTR
#define	VFSOP_SETATTR	CFS_SETATTR
#define	VFSOP_ACCESS	CFS_ACCESS
#define	VFSOP_LOOKUP	CFS_LOOKUP
#define	VFSOP_CREATE	CFS_CREATE
#define	VFSOP_REMOVE	CFS_REMOVE
#define	VFSOP_LINK	CFS_LINK
#define	VFSOP_RENAME	CFS_RENAME
#define	VFSOP_MKDIR	CFS_MKDIR
#define	VFSOP_RMDIR	CFS_RMDIR
#define	VFSOP_READDIR	CFS_READDIR
#define	VFSOP_SYMLINK	CFS_SYMLINK
#define	VFSOP_READLINK	CFS_READLINK
#define	VFSOP_FSYNC	CFS_FSYNC
#define	VFSOP_INACTIVE	CFS_INACTIVE
#define	VFSOP_LOCKCTL	/*CFS_LOCKCTL*/VFSOP_UNSET
#define	VFSOP_FID	/*CFS_FID*/VFSOP_UNSET
#define	VFSOP_RESOLVE	32
#define	VFSOP_REINTEGRATE   33

/* vnode_{allocs,deallocs} used to be inside #ifdef VENUSDEBUG, but the port 
   to NetBSD caused the MAKE_VNODE() & DISCARD_VNODE() macros to become 
   too convoluted (Satya, 8/14/96) */
extern int vnode_allocs;
extern int vnode_deallocs;

/* Use of vnodes by Venus (Satya, 8/14/96): 

   The use of struct vnode by Venus is purely for convenience; the kernel never
   passes a vnode up to Venus or vice versa.  Venus allocates and deallocates vnodes
   only because they are convenient structures to hold exactly the info needed about
   cached objects.  What actually gets allocated is a struct cnode, that encapsulates
   a struct vnode.
   
   On Mach, all the data is directly embedded in the encapsulating cnode.
   
   On NetBSD, the vnode corresponding to a cnode has to be explicitly allocated; 
   the v_data field of the vnode points to its associated cnode.

   All this makes the code trickier to understand, but it does keep down the 
   profileration of "ifdef's" in the mainline code.   Hopefully this approach is a 
   net win.  The Linux port should help clarify whether it is.
*/

#ifdef __MACH__
#define	MAKE_VNODE(vp, fid, type)\
{\
    struct cnode *tcp = new cnode;\
    CN_INIT(tcp, (fid), 0, 0);\
    VN_INIT(CTOV(tcp), 0, (type), 0);\
    (vp) = CTOV(tcp);\
    vnode_allocs++;\
}

#define	DISCARD_VNODE(vp)\
{\
    vnode_deallocs++;\
    delete (VTOC((vp)));\
}
#endif __MACH__

#if  __NetBSD__ || LINUX
#define	MAKE_VNODE(vp, fid, type)\
{\
    struct cnode *tcp = new cnode;\
    bzero(tcp, (int) sizeof(struct cnode));\
    tcp->c_fid = fid;\
    tcp->c_vnode = new vnode;\
    (vp) = CTOV(tcp);\
    bzero((vp), (int) sizeof(struct vnode));\
    (vp)->v_usecount = 1; /* Is this right? (Satya, 8/15/96) */\
    (vp)->v_type = (enum vtype) type;\
    (vp)->v_data = tcp; /* point back so VTOC() will work */ \
    vnode_allocs++;\
}
#define	DISCARD_VNODE(vp)\
{\
    vnode_deallocs++;\
    delete (VTOC((vp))); /* Has to happen BEFORE delete of vp; else VTOC() won't work!! -- Satya */ \
    delete (vp);\
}
#endif __NetBSD__


#define	VFSOP_TO_VSE(vfsop)\
    (vfsop)


/* Macros for fields of struct vattr that are different in Mach and NetBSD;
   use of these reduces ugly #ifdef's in mainline code (Satya, 8/15/96) */

#ifdef __MACH__
#define VA_ID(va)	(va)->va_nodeid
#define VA_STORAGE(va)	(va)->va_blocks
#define VA_MTIME_1(va)	(va)->va_mtime.tv_sec
#define VA_MTIME_2(va)	(va)->va_mtime.tv_usec
#define VA_ATIME_1(va)	(va)->va_atime.tv_sec
#define VA_ATIME_2(va)	(va)->va_atime.tv_usec
#define VA_CTIME_1(va)	(va)->va_ctime.tv_sec
#define VA_CTIME_2(va)	(va)->va_ctime.tv_sec
#endif __MACH__

#ifdef __NetBSD__ 
#define VA_ID(va)	(va)->va_fileid
#define VA_STORAGE(va)	(va)->va_bytes
#define VA_MTIME_1(va)	(va)->va_mtime.tv_sec
#define VA_MTIME_2(va)	(va)->va_mtime.tv_nsec
#define VA_ATIME_1(va)	(va)->va_atime.tv_sec
#define VA_ATIME_2(va)	(va)->va_atime.tv_nsec
#define VA_CTIME_1(va)	(va)->va_ctime.tv_sec
#define VA_CTIME_2(va)	(va)->va_ctime.tv_nsec
#endif __NetBSD__


#ifdef LINUX
#define VA_ID(va)	(va)->va_fileid
#define VA_STORAGE(va)	(va)->va_bytes
#define VA_MTIME_1(va)	(va)->va_mtime.tv_sec
#define VA_MTIME_2(va)	(va)->va_mtime.tv_nsec
#define VA_ATIME_1(va)	(va)->va_atime.tv_sec
#define VA_ATIME_2(va)	(va)->va_atime.tv_nsec
#define VA_CTIME_1(va)	(va)->va_ctime.tv_sec
#define VA_CTIME_2(va)	(va)->va_ctime.tv_nsec
#endif LINUX


#endif	not _VENUS_PROC_H_
