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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/venus/vproc.h,v 4.5 1997/03/06 21:04:54 lily Exp $";
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

#ifdef __FreeBSD__
# define KERNEL 1
# include <sys/uio.h>
# undef  KERNEL
#else 
#include <sys/uio.h>
#endif /* __FreeBSD__ */

#ifdef __NetBSD__
#define __attribute__(x)    /* dummied out because of machine/segments.h */
#include <sys/user.h>
#undef __attribute__
#else
#include <sys/user.h>
#endif /* __NetBSD__ */

#ifdef __MACH__
/* Pick up private versions of vnode headers from vicedep */
#include <cfs/mach_vfs.h>
#include <cfs/mach_vnode.h>
#endif /* __MACH__ */

#ifdef	__linux__
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

#endif	/* __linux__ */

#include <cfs/coda.h>

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
#include "venus_vnode.h"

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
#if 0
#define  MAXFIDSZ 16
struct fid {
        u_short         fid_len;                /* length of data in bytes */
        char            fid_data[MAXFIDSZ];     /* data (variable length) */
};
#endif
struct cfid {
    u_short     cfid_len;
    u_short     cfid_fill;
    ViceFid     cfid_fid;
};



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
    struct CodaCred u_cred;	/* implicit user identifier */
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
    int lwpri;
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
    void root(struct venus_vnode **);
    void statfs(struct statfs *);
    void sync();
    void vget(struct venus_vnode **, struct cfid *);
    void open(struct venus_vnode **, int);
    void close(struct venus_vnode *, int);
    void rdwr(struct venus_vnode *, struct uio *, enum uio_rw, int);
    void ioctl(struct venus_vnode *, unsigned int, struct ViceIoctl *, int);
    void select(struct venus_vnode *, int);
    void getattr(struct venus_vnode *, struct coda_vattr *);
    void setattr(struct venus_vnode *, struct coda_vattr *);
    void access(struct venus_vnode *, int);
    void lookup(struct venus_vnode *, char *, struct venus_vnode **);
    void create(struct venus_vnode *, char *, struct coda_vattr *, int, int, struct venus_vnode **);
    void remove(struct venus_vnode *, char *);
    void link(struct venus_vnode *, struct venus_vnode *, char *);
    void rename(struct venus_vnode *, char *, struct venus_vnode *, char *);
    void mkdir(struct venus_vnode *, char *, struct coda_vattr *, struct venus_vnode **);
    void rmdir(struct venus_vnode *, char *);
    void readdir(struct venus_vnode *, struct uio *);
    void symlink(struct venus_vnode *, char *, struct coda_vattr *, char *);
    void readlink(struct venus_vnode *, struct uio *);
    void fsync(struct venus_vnode *);
    void inactive(struct venus_vnode *);
    void fid(struct venus_vnode *, struct cfid	**);

    /* Pathname translation. */
    int namev(char *, int, struct venus_vnode **);
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

extern void va_init(struct coda_vattr *);
extern void VattrToStat(struct coda_vattr *, struct stat *);
extern long FidToNodeid(ViceFid *);

#ifdef __MACH__
#define	CRTOEUID(cred)	((vuid_t)((cred).cr_uid))
#define	CRTORUID(cred)	((vuid_t)((cred).cr_ruid))
#endif /* __MACH__ */

#if defined(__linux__) || defined(__BSD44__)
/* vnodes in BSD44 don't seem to store effective user & group ids.  So just
   coerce everything to uid */
#define	CRTOEUID(cred)	((vuid_t)((cred).cr_uid))
#define	CRTORUID(cred)	((vuid_t)((cred).cr_uid))
#endif /* __linux__ || __BSD44__ */

#define	FTTOVT(ft)	((ft) == (int)File ? VREG :\
			 (ft) == (int)Directory ? VDIR :\
			 (ft) == (int)SymbolicLink ? VLNK :\
			 VREG)

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
   to BSD44 caused the MAKE_VNODE() & DISCARD_VNODE() macros to become 
   too convoluted (Satya, 8/14/96) */
extern int vnode_allocs;
extern int vnode_deallocs;

/* Use of venus_vnodes by Venus (Satya, 8/14/96): 

   The use of struct venus_vnode by Venus is purely for convenience;
   the kernel never passes a venus_vnode up to Venus or vice versa.
   Venus allocates and deallocates venus_vnodes only because they are
   convenient structures to hold exactly the info needed about cached
   objects.  What actually gets allocated is a struct venus_cnode, that
   encapsulates a struct venus_vnode.
*/

#define CTOV(cn)  &((cn)->c_vnode)
#define VTOC(vp)  ((struct venus_cnode *)((vp)->v_data))
#define	MAKE_VNODE(vp, fid, type)\
{\
    struct venus_cnode *tcp = new venus_cnode;\
    bzero(tcp, (int) sizeof(struct venus_cnode));\
    tcp->c_fid = fid;\
    (vp) = CTOV(tcp);\
    (vp)->v_usecount = 1; /* Is this right? (Satya, 8/15/96) */\
    (vp)->v_type = (enum coda_vtype) type;\
    (vp)->v_data = tcp; /* backpointer for VTOC() */ \
    vnode_allocs++;\
}
#define	DISCARD_VNODE(vp)\
{\
    vnode_deallocs++;\
    delete (VTOC((vp)));\
}


#define	VFSOP_TO_VSE(vfsop)\
    (vfsop)


#define VA_ID(va)	(va)->va_fileid
#define VA_STORAGE(va)	(va)->va_bytes
#define VA_MTIME_1(va)	(va)->va_mtime.tv_sec
#define VA_MTIME_2(va)	(va)->va_mtime.tv_nsec
#define VA_ATIME_1(va)	(va)->va_atime.tv_sec
#define VA_ATIME_2(va)	(va)->va_atime.tv_nsec
#define VA_CTIME_1(va)	(va)->va_ctime.tv_sec
#define VA_CTIME_2(va)	(va)->va_ctime.tv_nsec

/* Definitions of the value -1 with correct cast for different
   platforms, to be used in struct vattr to indicate a field to be
   ignored.  Used mostly in vproc::setattr() */

#define	VA_IGNORE_FSID		((long)-1)
#define	VA_IGNORE_ID		((long)-1)
#define VA_IGNORE_NLINK		((short)-1)
#define VA_IGNORE_BLOCKSIZE	((long)-1)
#define VA_IGNORE_RDEV		((dev_t)-1)
#define VA_IGNORE_STORAGE	((long)-1)
#define VA_IGNORE_MODE		((u_short)-1)
#define VA_IGNORE_UID		((vuid_t) -1)
#define VA_IGNORE_TIME2		((long) -1)
#define VA_IGNORE_GID		((vgid_t) -1)
#define VA_IGNORE_SIZE		((u_quad_t)-1) 
#define VA_IGNORE_TIME1		((time_t)-1)
#define VA_IGNORE_FLAGS		((u_long) -1)

#endif /* _VENUS_PROC_H_ */
