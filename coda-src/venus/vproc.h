/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include "coda_string.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#if 0
#ifdef __NetBSD__
#define __attribute__(x)    /* dummied out because of machine/segments.h */
#include <sys/user.h>
#undef __attribute__
#endif /* __NetBSD__ */
#endif

#include <coda.h>

#include <lwp/lwp.h>
#include <lwp/lock.h>
/* interfaces */
#include <vice.h>

/* from rvm */
#include <rvmlib.h>

#ifdef __cplusplus
}
#endif __cplusplus


/* from util */
#include <olist.h>
#include <dlist.h>
#include <rec_dlist.h>

/* from vicedep */
#include <venusioctl.h>

/* from venus */
#include "venus.private.h"


/* string with counts */
struct coda_string {
	int cs_len;
	int cs_maxlen;
	char *cs_buf;
};

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
		    VPT_AdviceDaemon,
		    VPT_LRDaemon,
		    VPT_WriteBack
};

/* Holds user/call specific context. */
class namectxt;
class volent;
struct uarea {
    int	u_error;		/* implicit return code */
    struct coda_cred u_cred;	/* implicit user identifier */
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
	memset((void *)this, 0, (int) sizeof(struct uarea));
	u_volmode = /*VM_UNSET*/-1;
	u_vfsop = /*VFSOP_UNSET*/-1;
    }
};

typedef void (*PROCBODY)(void);

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

    void init(void);

  protected:
    int lwpid;
    char *name;
    PROCBODY func; /* function should be set if vproc::main isn't overloaded */
    int vpid;
    rvm_perthread_t rvm_data;
    struct Lock init_lock;

    /* derived classes should call this function once they have finished their
     * constructor. */
    void start_thread(void);

    /* entry point, should be overloaded by derived classes */
    virtual void main(void);

  public:
    /* Public for the time being. -JJK */
    vproctype type;
    int stacksize;
    int lwpri;
    int seq;
    struct uarea u;
    unsigned idle : 1;
    unsigned interrupted : 1;
    struct vcbevent *ve;

    vproc(char *, PROCBODY, vproctype, int =VPROC_DEFAULT_STACK_SIZE, int =LWP_NORMAL_PRIORITY);
    vproc(vproc&);		// not supported
    int operator=(vproc&);	// not supported
    virtual ~vproc();

    /* Volume-level concurrency control. */
    void Begin_VFS(VolumeId, int, int =-1);
    void End_VFS(int * =0);

    /* The vproc interface: mostly matching kernel requests.  */
    void mount(char *, void *);
    void unmount();
    void root(struct venus_cnode *);
    void statfs(struct coda_statfs *);
    void sync();
    void vget(struct venus_cnode *, struct cfid *);
    void open(struct venus_cnode *, int);
    void close(struct venus_cnode *, int);
    void ioctl(struct venus_cnode *, unsigned int, struct ViceIoctl *, int);
    void select(struct venus_cnode *, int);
    void getattr(struct venus_cnode *, struct coda_vattr *);
    void setattr(struct venus_cnode *, struct coda_vattr *);
    void access(struct venus_cnode *, int);
    void lookup(struct venus_cnode *, char *, struct venus_cnode *, int);
    void create(struct venus_cnode *, char *, struct coda_vattr *, int, 
		int, struct venus_cnode *);
    void remove(struct venus_cnode *, char *);
    void link(struct venus_cnode *, struct venus_cnode *, char *);
    void rename(struct venus_cnode *, char *, struct venus_cnode *, char *);
    void mkdir(struct venus_cnode *, char *, struct coda_vattr *, 
	       struct venus_cnode *);
    void rmdir(struct venus_cnode *, char *);
    void symlink(struct venus_cnode *, char *, struct coda_vattr *, char *);
    void readlink(struct venus_cnode *, struct coda_string *);
    void fsync(struct venus_cnode *);
    void inactive(struct venus_cnode *);
    void fid(struct venus_cnode *, struct cfid	**);

    /* Pathname translation. */
    int namev(char *, int, struct venus_cnode *);
    void GetPath(ViceFid *, char *, int *, int =1);
    void verifyname(char *name, int flags);
#define NAME_NO_DOTS      1 /* don't allow '.', '..', '/' */
#define NAME_NO_CONFLICT  2 /* don't allow @XXXXXXXX.YYYYYYYY.ZZZZZZZZ */
#define NAME_NO_EXPANSION 4 /* don't allow @cpu / @sys */

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
void VPROC_printvattr(struct coda_vattr *vap);
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

/* Explanation: 
   CRTOEUID is only used by HDBD_Request (hdb_deamon.cc)
   and allows root to always make hoard requests
  
   For all filesystem use CRTORUID is used: for Linux we definitely want to 
   fsuid to be used for filesystem access.  This however breaks the old AFS
   semantics that if an "su" is performed you retain tokens.

   To make things more complicated, reintegration and resolve (which
   is in fact repair :) ) use the coda_cred's directly. 
   XXXXX Let's straighten this out. (pjb/jh)


*/

#ifdef __linux__

#define	CRTOEUID(cred)	((vuid_t)((cred).cr_fsuid))
#define	CRTORUID(cred)	((vuid_t)((cred).cr_fsuid))
#else

/* XXX BSD needs to think through what they want!!!! 
   The current behaviour has "AFS semantics" but allows no
   fileserver to access Coda (since since it will come in with 
   ruid 0 (at least for samba, nfs etc). */

#define	CRTOEUID(cred)	((vuid_t)((cred).cr_uid))
#define	CRTORUID(cred)	((vuid_t)((cred).cr_uid))
#endif

#define	FTTOVT(ft)	((ft) == (int)File ? C_VREG :\
			 (ft) == (int)Directory ? C_VDIR :\
			 (ft) == (int)SymbolicLink ? C_VLNK :\
			 C_VREG)


/* vnode_{allocs,deallocs} used to be inside #ifdef VENUSDEBUG, but the port 
   to BSD44 caused the MAKE_VNODE() & DISCARD_VNODE() macros to become 
   too convoluted (Satya, 8/14/96) */
extern int vnode_allocs;
extern int vnode_deallocs;


/* Venus cnodes are a small placeholder structure to pass arguments
   into the output buffer back to the kernel without clobbering the
   inputbuffer, which is the same pointer as the output buffer.
*/

struct venus_cnode {
	u_short	    c_flags;	/* flags (see below) */
	ViceFid	    c_fid;	/* file handle */
	dev_t	    c_device;	/* associated vnode device */
	ino_t	    c_inode;	/* associated vnode inode */
	char        c_cfname[128]; /* container file name */
	int         c_type;
};

#define	MAKE_CNODE(vp, fid, type)\
{\
    (vp).c_fid = fid;\
    (vp).c_type = type;\
    (vp).c_flags = 0;\
}


/* Definitions of the value -1 with correct cast for different
   platforms, to be used in struct vattr to indicate a field to be
   ignored.  Used mostly in vproc::setattr() */

#define	VA_IGNORE_FSID		((long)-1)
#define	VA_IGNORE_ID		((long)-1)
#define VA_IGNORE_NLINK		((short)-1)
#define VA_IGNORE_BLOCKSIZE	((long)-1)
#define VA_IGNORE_RDEV		(-1)
#define VA_IGNORE_STORAGE	((u_quad_t) -1)
#define VA_IGNORE_MODE		((u_short)-1)
#define VA_IGNORE_UID		((vuid_t) -1)
#define VA_IGNORE_TIME2		((long) -1)
#define VA_IGNORE_GID		((vgid_t) -1)
#define VA_IGNORE_SIZE		((u_quad_t)-1) 
#define VA_IGNORE_TIME1		((time_t)-1)
#define VA_IGNORE_FLAGS		((u_long) -1)

#endif /* _VENUS_PROC_H_ */
