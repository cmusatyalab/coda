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

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/kernel-src/vfs/mach/cfs_MACH.h,v 1.1.1.1 1996/12/09 19:17:51 rvb Exp";
#endif /*_BLURB_*/


/* Mach-specific support for Coda/Sets */
#ifndef _MACH_SPECIFIC_H_
#define _MACH_SPECIFIC_H_ 1

/************************ Need the __P macro */
#ifndef __P
#if (defined(__STDC__) || defined(__cplusplus))
#define __P(protos)  protos
#else
#define __P(protos)  ()
#endif
#endif __P


#include <sys/user.h>
#include <cfs/mach_vfs.h>
#include <cfs/mach_vnode.h> 

#ifdef KERNEL
/* External definitions needed by sets. */
/* These includes are overkill: the union of those needed by sets and cfs */
#include <mach_cfs.h>		/* Number of minor devices allowed. (mach configure switch) */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/inode.h>
#include <sys/proc.h>
#include <kern/queue.h>
#include <sys/uio.h>	/* Needed by pathname.c */
#include <kern/mfs.h>
#include <cfs/cnode.h>
#include <sys/conf.h>
#include <sys/systm.h>
/* #include <cfs/cfs_opstats.h> */

/* 
 * NetBSD, and code, expects number of minor devices to be NVCFS, but
 * for some reason our mach config stuff defines a funky name for the
 * device.
 */

#define NVCFS NMACH_CFS

/* Special printf - can use to add delays */
#define myprintf(args)   printf args

/* Pretty bizare, but these don't seemed to be defined in kernel .h files. */
/* And strcat in particular is defined in afs! */

#ifndef SUN4C 
extern char *index __P((char*, int));
extern char *strcpy __P((char*, char*));
extern char *strcat __P((char*, char*));
extern unsigned long strlen __P((char*));
#endif  SUN4C

/******************* Pseudo-device */

/* Which function is the entry point for open(/dev/cfs)? */
#define VCOPEN vcopen

/* Target functions for pseudo-device; wrapped in MACH */
int vc_nb_open    __P((dev_t, int, int, struct proc *));
int vc_nb_close   __P((dev_t, int, int, struct proc *));
int vc_nb_read    __P((dev_t, struct uio *, int));
int vc_nb_write   __P((dev_t, struct uio *, int));
int vc_nb_ioctl   __P((dev_t, int, caddr_t, int, struct proc *));
int vc_nb_select  __P((dev_t, int, struct proc *));

/*************** vnode operation gates */

#define VOP_DO_OPEN(vpp, flag, cred, p)   VOP_OPEN(vpp, flag, cred)

#define VOP_DO_CLOSE(vp, flag, cred, p)   VOP_CLOSE(vp, flag, cred)

#define VOP_DO_READ(vp, uiop, ioflag, cred)    \
               VOP_RDWR(vp, uiop, UIO_READ, ioflag, cred)

#define VOP_DO_WRITE(vp, uiop, ioflag, cred)    \
               VOP_RDWR(vp, uiop, UIO_WRITE, ioflag, cred)

#define VOP_DO_READDIR(vp, uiop, cred, eofflag, cookies, ncookies) \
            VOP_READDIR(vp, uiop, cred)

#define VOP_DO_UNLOCK(vp)   iunlock(VTOI(vp))

/********************** Lookup */

#define DO_LOOKUP(d, s, f, pvpp, vpp, proc, ndp, error)   \
do {                                                      \
    (error) = lookupname((d), (s), (f), (pvpp), (vpp));   \
} while (0)

/*
 * linux mem segment stuff that mach apparently doesn't need.  
 */

#define  put_user_long(val, addr) *(addr) = val;
#define  copyFromUserSpace(from, to, len) \
    { \
	int tmp, result; \
	result = copyinstr(from, to, len, &tmp);\
	if (result < 0)\
	    return result;\
    }

#define  copyToUserSpace(from, to, len) \
    { \
	int tmp, result; \
	result = copyoutstr(from, to, len, &tmp);\
	if (result < 0)\
	    return result;\
    }

/********************** uiomove */

#define UIOMOVE(data, size, flag, uiop, error) \
    (error) = uiomove((data),(size),(flag),(uiop))

#endif KERNEL
/*
 * Macros to hide differences in VFS layouts.
 */

/* 
 * First of all, what is a vfs? 
 */
#define VFS_T struct vfs

/*
 * vfs anon pointer name and type
 */
#define VFS_DATA  vfs_data
typedef caddr_t   VFS_ANON_T;
/*
 * some vfs/mount fields
 */
#define VFS_FSID(vfsp)  (vfsp)->vfs_fsid
#define VFS_BSIZE(vfsp) (vfsp)->vfs_bsize

/* 
 * what's the name/type of the vnodeop vector?
 */
extern struct vnodeops cfs_vnodeops;

/* 
 * MOUNT_CFS is already an integer
 */
#define makefstype(TYPE)       (TYPE)

/* 
 * some vnode fields 
 */
#define VN_VFS(vp)         (vp)->v_vfsp
#define VN_TYPE(vp)        ((vp)->v_mode & VFMT)
#define VN_RDEV(vp)        (vp)->v_rdev
#define IS_CODA_VNODE(vp)  ((vp)->v_type == ITYPE_CFS)

/*
 * cfid structure:
 * This overlays the fid structure (see vfs.h)
 */

struct cfid {
    u_short	cfid_len;
    ViceFid	cfid_fid;
};


/*
 * How cnode's track vnodes
 */

/* On mach, a cnode is a vnode on steriods */
typedef struct vnode C_VNODE_T;

/* 
 * getting from one to the other
 */
#define	VTOC(vp)	((struct cnode *)(vp))
#define	CTOV(cp)	((struct vnode *)(cp))

/* 
 * link structures for cnode free lists.  Steals from vnode's.
 */
#define LINKS           /* MT */

#ifdef KERNEL
#define	CNODE_NEXT(cp)	(VTOC(CTOV(cp)->v_next))

/* 
 * dounmount: on Mach uses u.u_error and u-dot doesn't exist on some
 * systems
 */

#define DOUNMOUNT(vfsp)  u.u_error = 0, dounmount(vfsp), u.u_uerror

#define GET_DIR(name, dir, error) error = sets_get_dir(name, dir)
#define DIR_ADVANCE(dp) dp = (struct direct *)((caddr_t)dp + dp->d_reclen)
#define DIR_PAGE_SIZE DEV_BSIZE

#include <dfstrace.h>
#if DFSTRACE > 0
#include <dfs/dfs_log.h>
#define OPEN_OBJECT(name, flags, vp, err) \
    err = vn_open(name, UIO_SYSSPACE, flags, 0, vp, DFS_OPEN);
#else DFSTRACE
#define OPEN_OBJECT(name, flags, vp, err) \
    err = vn_open(name, UIO_SYSSPACE, flags, 0, vp);
#endif DFSTRACE

/* Call close and rele on a vnode that hasn't yet been seen by iterate. */
#define CLOSE_VP(ERR, VP) \
    { \
	  ERR = vn_close(VP, FREAD);  /* What other flags should I use? -- DCS */ \
          VN_RELE(VP); \
    }

/* Managing file descriptors */
#define CLOSE_FILE(file) 	closef(file)
#define GET_FP(thing, fd)	thing = (char *)u.u_ofile[fd];
#define HOLD_FP(filep)		((struct file *)filep)->f_count++

#define RELEASE_FP(thing) \
    { \
	struct file *fp = (struct file *)*thing; \
        *thing = fp->f_data; \
        closef(fp); \
    }

#endif KERNEL
/*********** cnodes/vnodes */

/* Initialize a cnode */
#ifdef	KERNEL
#define CN_INIT(CP) {\
    bzero((CP), (int)sizeof(struct cnode));\
}
#else	KERNEL
#define CN_INIT(CP, FID, DEV, INO)            \
do {					      \
    struct vm_info *vitmp;                    \
                                              \
    vitmp = CTOV(CP)->v_vm_info;              \
    bzero((CP), (int)sizeof(struct cnode));   \
    VN_INIT(CTOV((CP)), 0, 0, 0);             \
    (CP)->c_fid = FID;                        \
    (CP)->c_device = DEV;                     \
    (CP)->c_inode = INO;                      \
    CTOV(CP)->v_vm_info = vitmp;              \
} while (0)
#endif	KERNEL

#ifdef KERNEL
#define CFS_CLEAN_VNODE(vp)    cfs_free(VTOC(vp))

/* cnodes *are* vnodes, just bigger. */
#define VNODE_ALLOC(cp)              /* MT */

/* Allocate a vfs structure */
#define VFS_ALLOC(ptr)         ZALLOC(vfs_vfs_zone, (ptr), struct vfs *)

/* Cnode reference count */
#define CNODE_COUNT(cp)    CTOV(cp)->v_count

/* Mach inodes have Mach Pager info in them */

#define VNODE_VM_INFO_INIT(vp)           \
do {                                     \
    (vp)->v_vm_info = VM_INFO_NULL;      \
    vm_info_init(vp);                    \
} while(0)

#define SYS_VN_INIT(cp, vfsp, type)                  \
do {                                                 \
    struct vm_info *vitmp;                           \
    struct vnode   *vp = CTOV(vp);                   \
                                                     \
    vitmp = vp->v_vm_info;                           \
    VN_INIT(vp, vfsp, type, 0);                      \
    vp->v_vm_info = vitmp;                           \
    vp->v_vm_info->pager = MEMORY_OBJECT_NULL;       \
    vp->v_type = ITYPE_CFS;                          \
} while (0)

/* locking/unlocking: ain't no such beast in Mach, but there is in NetBSD */
#define VN_LOCK(vp)             /* MT */
#define VN_UNLOCK(vp)           /* MT */

/* Mach wants roots returned ref'd, NetBSD wants them locked */
#define CFS_ROOT_REF(vp)        VN_HOLD(vp)

/* Mach wants lookups unlocked, NetBSD wants them locked */
#define LOOKUP_LOCK(vp)         /* MT */

/********************************* struct vattr differences */

#define VATTR_TYPE(vap)      (vap)->va_mode

/* Sometimes when venus dies, there are outstanding references to coda
 * objects.  It would be bad to allow these references to continue to
 * use the objects, so venus can only restart if there are no such
 * objects. But since these references are hard to find, it would be
 * nice if we could orphan the objects such that future operations on
 * them fail (ENODEV), but don't kill venus or the
 * kernel. FAKE_UNMOUNT will set most of this up. The idea is to
 * remove the mount point without freeing the vfsp ('cause the orphans
 * still need it). But the writer of vfs.c was braindead so I have to
 * do the VN_RELE of the mounted-on vnode, and re-lock the vfsp so
 * that the caller of cfs_unmount (dounmount in vfs.c) won't panic
 * when it tries to unlock the vfsp. -- DCS 11/29/94 
 */

#define FAKE_UNMOUNT(vfsp) \
    VN_RELE(vfsp->vfs_vnodecovered);	/* Why doesn't remove do this? */ \
    vfs_remove(vfsp); \
    /* really ugly: dounmount will try to unlock this sucker. so lock it. */ \
    vfs_lock(vfsp);

/* Add it to list of vfsp's in the system, allows one to call unmount on it */
#define FAKE_MOUNT(vfsp, cvp) \
    { \
	int err; \
	if ((cvp)->v_count != 2) { \
	    /* Should be 2. 1 from the lookup that got us here, 1 from cfs_namecache */ \
	    printf("Help, ody_mounting on a vnode with count == %d\n", (cvp)->v_count); \
	} \
	err = vfs_add((cvp), (vfsp), 0); \
	if (err) { \
	    printf("Help! got an error %d from vfs_add\n", err); \
	    panic("calling vfs_add()\n"); \
	}\
	vfs_unlock(vfsp);\
    }


#endif KERNEL
/**** Wierdness with the identity structure. Needed by venus and kernel */
#define CodaCred ucred
#ifdef KERNEL
#define GLOBAL_CRED  u.u_cred
#define COPY_CRED_TO_CODACRED(in, ident) (in)->cred = *(ident)

/* per-process definitions. A hack to allow the sets worker thread to run on
 * behalf of the caller thread.
 */

#define GLOBAL_PROC     u.u_procp
#define Process_pid 	u.u_procp->p_pid 
#define Process_pgid 	u.u_procp->p_pgrp
#define Process_cdir ITOV(u.u_cdir)
#define Process_root ITOV(u.u_rdir)
#define Process_setStuff u.u_setStuff

/********************* Accounting Flags */

#define DUMPING_CORE       (u.u_acflag & ACORE)

/* Wierdness with the identity structure. */
#define CodaCred ucred
    
#define COPY_CRED_TO_CODACRED(in, ident) (in)->cred = *(ident)

#define FREE_ID(myid)  free_identity(myid)
#define SAVE_ID(id) id = u.u_identity; (id)->id_ref++;
#define CHANGE_ID(id) u.u_identity = (id)

/* Turns out this won't work with Mach. Causes segv in free_identity when setsd exits :-( */
#define FREE_MY_IDENTITY() 

/* I need the operation to run on behalf of whomever spawned the msg, so 
 * use its curdirectory and identity.
 */
#define BECOME_CALLER(msg) \
    free_identity(u.u_identity); /* Cease being whoever we were */ \
    u.u_identity = (msg)->set->identity; /* Become id of caller */ \
    u.u_identity->id_ref++; \
    /*HACK*/ u.u_cdir = VTOI(msg->set->curdir); \
    /*HACK*/ u.u_rdir = VTOI(msg->set->rootdir); 

/* Mach chokes if it can't release the cdir of a process, so give it one. */
#define BECOME_MYSELF(mycdir) \
    /*HACK*/ u.u_cdir = VTOI((mycdir));


/* I need os-specific macros for sleep/wakeup. */
#define SELPROC struct proc *
#define SELPROC_INIT(selproc)    (selproc) = 0;
#define SELWAKEUP(selproc) if (selproc) selwakeup((struct proc *)(selproc), 0)
#define SELRECORD(selinfo) (selinfo) = (SELPROC)current_thread()
#define WAKEUP(cond) wakeup(cond)
#define SLEEP(cond, priority)  tsleep(cond, priority, 0)	 /* Don't use timeout */

/* Definitions of alloc/free macros */
#define MYALLOC(ptr, cast, size) \
do { \
	ptr = (cast)kalloc((int)(size)); \
	if (ptr == 0)  \
	  panic("kernel malloc returns 0 at %s:%d\n", __FILE__, __LINE__); \
 } while (0)

#define MYFREE(ptr, size) \
do { \
	kfree(ptr, size); \
 } while (0)

#define CFS_ALLOC(ptr, cast, size)  MYALLOC(ptr, cast, size)
#define CFS_FREE(ptr, size)         MYFREE (ptr, size)

/* vfs_name_zone is defined and set in vfs/vfs.c and vfs/vnode.h */
#define STRALLOC(ptr) \
    { \
	ZALLOC(vfs_name_zone, ptr, char *); \
	if (ptr == 0) panic("stralloc returns 0 at %s:%d\n", __FILE__, __LINE__); \
    }

#define STRFREE(ptr) \
    { \
	ZFREE(vfs_name_zone, ptr); \
    }
    
/*
 * Finding out about current signals
 */

#define SIGLIST    u.u_procp->p_cursig
#define THECURSIG  u.u_procp->p_sig

/*
 * Mach-specific definitions for thread syncronization control.
 */
    
#include <kern/lock.h>
#include <sys/param.h>
    
#define MUTEX				lock_data_t
#define MUTEX_T				lock_data_t *
#define CONDITION			char 
#define CONDITION_T			char *

#define MUTEX_INIT(m)			(lock_init(m,1))

#ifndef DEBUGCTHREAD

#define cthread_yield() { printf("Yielding %s:%d\n", __FILE__, __LINE__); }

#define condition_signal(cond) \
    { \
       DEBUG(THREAD,printf("Signaling condition 0x%x %s:%d\n", (cond),__FILE__,__LINE__););\
       wakeup((cond)); \
    }


#define mutex_unlock(m)\
    { \
	DEBUG(THREAD,printf("mutex_unlock RL(0x%x)%s:%d...", (m), __FILE__, __LINE__);); \
	lock_done((m)); \
        DEBUG(THREAD,printf("done\n");); \
    }

#define mutex_lock(m)\
    { \
	DEBUG(THREAD,printf("mutex_lock OL(0x%x)%s:%d...", (m), __FILE__, __LINE__);); \
	lock_write((m)); \
        DEBUG(THREAD,printf("done\n");); \
    }

#define condition_wait(cond, lock) \
    { \
       DEBUG(THREAD,printf("Waiting on condition 0x%x %s:%d\n", (cond),__FILE__,__LINE__););\
       if (lock) mutex_unlock((lock)); \
       sleep((cond), PWAIT); \
       if (lock) mutex_lock((lock)); \
    } 
    
#else DEBUGCTHREAD
#define mutex_lock(m)			lock_write((m));
#define mutex_unlock(m)			lock_done((m));


#define cthread_yield() { DEBUG(THREAD,printf("Yielding %s:%d\n", __FILE__, __LINE__);); }

#define condition_signal(cond) \
    { \
       wakeup((cond)); \
    }


#define condition_wait(cond, lock)\
    { \
       if (lock) mutex_unlock((lock)); \
       sleep((cond), PWAIT); \
       if (lock) mutex_lock((lock)); \
    } 


#endif DEBUGCTHREAD

#ifdef assert
#undef assert
#endif assert

#define assert(cond) 							\
    if (!(cond)) {    							\
	printf("Assert at line \"%s\", line %d\n", __FILE__, __LINE__); \
	Debugger("assertion failure");					\
    }

/* Hack required by linux but not mach... */
#define CHECK_USER_BUFFER(buf, size)

/* Stuff in the cfs needs to indicate failures more strongly than return codes. */
#define COMPLAIN_BITTERLY(OP, FID) \
    printf("Acck! OP on dead vnode %x.%x.%x attempted by Process %d (parent %d)\n",  \
	   (FID).Volume, (FID).Vnode, (FID).Unique, u.u_procp->p_pid, u.u_procp->p_ppid);

#endif KERNEL

#endif _MACH_SPECIFIC_H_
