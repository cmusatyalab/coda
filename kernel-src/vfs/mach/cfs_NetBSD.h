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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/kernel-src/vfs/mach/Attic/cfs_NetBSD.h,v 4.2 1997/02/26 16:04:33 rvb Exp $";
#endif /*_BLURB_*/


/* cfs_plat.h: platform-specific support for the CFS driver */

/*************** Include files */

/* BSD44 includes */
/* Overkill...     */
#ifdef KERNEL
#include <types.h>              /* Needed by malloc.h */
#include <time.h>
#include <systm.h>
#include <malloc.h>             /* No zalloc.h; malloc.h instead */
#include <param.h>
#include <mount.h>              /* no struct vfs, struct mount instead */
#include <uio.h>
#include <vnode.h>              /* vnode.h lives in sys, get via makefile */
#include <cfs/mach_vioctl.h>    /* No viceioctl.h on BSD44 */
#include <vcfs.h>               /* Number of minor devices */
#include <signal.h>
#include <proc.h>               /* Definition for struct proc */
#include <namei.h>              /* For lookup operations */
#include <miscfs/specfs/specdev.h>  /* wow.  BSD44 is screwed. */
#include <dir.h>
#include <ufs/ifs/ifs.h>
#else KERNEL
#include <sys/param.h>
#include <sys/ucred.h>
#include <sys/vnode.h>
#endif KERNEL

#ifdef KERNEL
/**** XXX!  I don't know why dir.h doesn't pick this up for us! */
#ifndef DIRBLKSIZ
#define DIRBLKSIZ  DEV_BSIZE
#endif DIRBLKSIZ

/* Special printf - can use to add delays */
extern int cfs_printf_delay;  /* In cfs_nbsd.c */

#define myprintf(args)          \
do {                            \
    if (cfs_printf_delay)       \
	delay(cfs_printf_delay);\
    printf args ;               \
} while (0)


/*************** Pseudo-device */

/* Which function is the entry point for open(/dev/cfs)? */
int vc_nb_open __P((dev_t, int, int, struct proc *));

#define VCOPEN vc_nb_open

/*************** vnode operation gates */

#define VOP_DO_OPEN(vpp, flag, cred, p)      VOP_OPEN(*(vpp), flag, cred, p)

#define VOP_DO_CLOSE(vp, flag, cred, p)      VOP_CLOSE(vp, flag, cred, p)

#define VOP_DO_READ(vp, uiop, ioflag, cred)  VOP_READ(vp, uiop, ioflag, cred)

#define VOP_DO_WRITE(vp, uiop, ioflag, cred) VOP_WRITE(vp, uiop, ioflag, cred)

#define VOP_DO_READDIR(vp, uiop, cred, eofflag, cookies, ncookies) \
              VOP_READDIR(vp, uiop, cred, eofflag, cookies, ncookies)

#define VOP_DO_UNLOCK(vp)   VOP_UNLOCK(vp)

/********************** Lookup */

#define FOLLOW_LINK   FOLLOW
#define NO_FOLLOW     NOFOLLOW

#define DO_LOOKUP(d, s, f, pvpp, vpp, proc, ndp, error) \
do {                                                    \
    NDINIT((ndp),LOOKUP,(f),(s),(d),(proc));            \
    (error) = namei(ndp);                               \
    *(vpp) = (ndp)->ni_vp;                              \
} while (0)

/********************** uiomove */

#define UIOMOVE(data, size, flag, uiop, error)        \
do {                                                  \
    (uiop)->uio_rw = (flag);                          \
    (error) = uiomove((data),(size),(uiop));          \
} while (0)
    

/********************** Mach flags/types that are missing */

enum vcexcl	{ NONEXCL, EXCL};		/* (non)excl create (create) */

#define FTRUNC       O_TRUNC
#define FCREAT       O_CREAT
#define FEXCL        O_EXCL

/*************** Per process information */
#define GLOBAL_PROC     curproc
/* WARNING 
 * These macros assume the presence of a process pointer p!
 * And, they're wrong to do so.  Phhht.
 */
#define Process_pid     (p ? p->p_pid : -1)
#define Process_pgid    (p ? p->p_pgid : -1)

/******************* Accounting flags */

#define DUMPING_CORE    (p && (p->p_acflag & ACORE))


/*************** User Credentials */
#endif /* KERNEL */
#define CodaCred ucred
#ifdef KERNEL
#define GLOBAL_CRED  p->p_cred->pc_ucred  /* type (ucred *) */

#define COPY_CRED_TO_CODACRED(in, ident)                \
do {                                                    \
    if (ident != NOCRED) {                              \
	(in)->cred = *(ident);                          \
    } else {                                            \
	bzero(&((in)->cred),sizeof(struct CodaCred));   \
	(in)->cred.cr_uid = -1;                         \
	(in)->cred.cr_gid = -1;                         \
    }                                                   \
} while (0)
#endif KERNEL

/*************** VFS differences. */
/* 
 * First of all, what is a vfs? 
 */
#define VFS_T   struct mount

/*
 * vfs anon pointer name and type
 */
#define VFS_DATA  mnt_data
typedef qaddr_t   VFS_ANON_T;

/*
 * some vfs/mount fields
 */
#define VFS_FSID(vfsp)  (vfsp)->mnt_stat.f_fsid
#define VFS_BSIZE(vfsp) (vfsp)->mnt_stat.f_bsize

/*
 * Setting up a new vfs entry 
 */
#define VFS_INIT(VFSP, OP, DATA)	                  \
do {                                                      \
    bzero((char *)(VFSP), (u_long)sizeof(struct mount));  \
    (VFSP)->mnt_op = (OP);                                \
    (VFSP)->mnt_data = (DATA);                            \
} while (0)

/* 
 * what's the name/type of the vnodeop vector?
 */
extern int (**cfs_vnodeop_p)();

/* 
 * some vnode fields
 */
#define VN_VFS(vp)         (vp)->v_mount
#define VN_TYPE(vp)        (vp)->v_type
#define VN_RDEV(vp)        (vp)->v_specinfo->si_rdev
#define IS_CODA_VNODE(vp)  ((vp)->v_tag == VT_CFS) /* XXX: we shouldn't look */

/*
 * cfid structure:
 * This overlays the fid structure (see vfs.h)
 */

struct cfid {
    u_short	cfid_len;
    u_short     padding;
    ViceFid	cfid_fid;
};

/* 
 * How cnode's track vnodes
 */

/* On BSD44, just a pointer to a vnode */
typedef struct vnode *C_VNODE_T;           /* needed in cnode.h */

/* 
 * getting from one to the other
 */
#define	VTOC(vp)	((struct cnode *)(vp)->v_data)
#define	CTOV(cp)	((struct vnode *)((cp)->c_vnode))

/* 
 * link structures for cnode free lists.  We need our own.
 */
#define LINKS           struct cnode *c_next     /* needed in cnode.h */
#ifdef KERNEL 
/* returns a cnode, not a vnode! */
#define	CNODE_NEXT(cp)	((cp)->c_next)

/* requires in-scope definitions for flag (int) and p (struct proc *) */
#define DOUNMOUNT(vfsp)        dounmount(vfsp, flag, p)

/*************** cnodes/vnodes. */

/* Initialize a cnode */
#define CN_INIT(CP)        bzero((CP), (int)sizeof(struct cnode))

/* Nuke a cnode/vnode */
#define CFS_CLEAN_VNODE(vp)  vgone(vp)

/* Cnode reference count */
/* XXX - is this right? */
#define CNODE_COUNT(cp)    CTOV(cp)->v_usecount

/* BSD44 vnodes don't have any Pager info in them ('cause there are
   no external pagers, duh!) */
#define VNODE_VM_INFO_INIT(vp)         /* MT */

#define SYS_VN_INIT(cp, vfsp, type)                           \
do {                                                          \
    struct vnode *vp;                                         \
    int           err;                                        \
                                                              \
    (err) = getnewvnode(VT_CFS, (vfsp), cfs_vnodeop_p, &vp);  \
    if (err) {                                                \
	panic("cfs: getnewvnode returned error %d\n", err);   \
    }                                                         \
    vp->v_data = cp;                                          \
    vp->v_type = (type);                                      \
    cp->c_vnode = vp;                                         \
} while (0)

extern int print_hold_release;

#define VN_HOLD(vp)          vref(vp);
#define VN_RELE(vp)          vrele(vp);  
#define VN_LOCK(vp)          VOP_LOCK(vp)
#define VN_UNLOCK(vp)        VOP_UNLOCK(vp)

/* BSD44 wants roots returned locked/ref'd.  Mach wants them vref'd */

#define CFS_ROOT_REF(vp) \
do {                     \
    VN_HOLD(vp);         \
    VN_LOCK(vp);         \
} while (0)

/* BSD44 wants lookups returned locked.  Mach doesn't */

#define LOOKUP_LOCK(vp)     VOP_LOCK(vp);

/*************** struct vattr differences */

#define VATTR_TYPE(vap)      (vap)->va_type

/*************** Conditions. */
/* Why are these here?  I don't know */
typedef char  CONDITION;
typedef char *CONDITION_T;

/*************** sleep/wakeup. */

#define SELPROC struct selinfo
#define SELPROC_INIT(selproc) bzero(&selproc, sizeof(SELPROC))
#define SELWAKEUP(selproc)    selwakeup(&selproc)
#define SELRECORD(selinfo)    selrecord(p, &(selinfo))
#define WAKEUP(cond)          wakeup(cond)
#define SLEEP(cond, priority) tsleep((cond),(priority),"cfscall",0)

/*************** allocation. */

#define CFS_ALLOC(ptr, cast, size)                                        \
do {                                                                      \
    ptr = (cast)malloc((unsigned long) size, M_CFS, M_WAITOK);            \
    if (ptr == 0) {                                                       \
	panic("kernel malloc returns 0 at %s:%d\n", __FILE__, __LINE__);  \
    }                                                                     \
} while (0)

#define VFS_ALLOC(ptr)  CFS_ALLOC(ptr, struct mount *, sizeof(struct mount));

#define CFS_FREE(ptr, size)  free((ptr),M_CFS)

/*************** Finding out about signals.  We can't reliably */

#define SIGLIST    -1
#define THECURSIG  -1


/**************** Diagnostics */

#ifdef __STDC__
#define COMPLAIN_BITTERLY(OP, FID)                             \
do {                                                           \
    myprintf(("Acck! %s on dead vnode %x.%x.%x attempted ",    \
           #OP, (FID).Volume, (FID).Vnode, (FID).Unique));     \
    myprintf(("by Process %d (parent %d)\n",                   \
	   GLOBAL_PROC->p_pid, GLOBAL_PROC->p_pptr->p_pid));   \
} while (0)
#else  __STDC__
#define COMPLAIN_BITTERLY(OP, FID)                             \
do {                                                           \
    myprintf(("Acck! OP on dead vnode %x.%x.%x attempted ",    \
           (FID).Volume, (FID).Vnode, (FID).Unique));          \
    myprintf(("by Process %d (parent %d)\n",                   \
	   GLOBAL_PROC->p_pid, GLOBAL_PROC->p_pptr->p_pid));   \
} while (0)
#endif __STDC__

/****************** Prototypes missing from BSD44 includes */

extern int _remque __P((caddr_t));
extern int _insque __P((caddr_t, caddr_t));
extern int vfinddev __P((dev_t, enum vtype, struct vnode **));
extern int dounmount __P((struct mount *, int, struct proc *));
extern int uiomove  __P((caddr_t, int, struct uio *));
extern long makefstype __P((char *));
extern int cache_purge __P((struct vnode *));
extern int delay __P((int));

#endif /* KERNEL */
