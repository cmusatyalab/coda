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
 * $Log: cfs_nbsd.c,v $
 * Revision 1.13  1997/02/18 23:46:25  bnoble
 * NetBSD swapped the order of arguments to VOP_LINK between 1.1 and 1.2.
 * This tracks that change.
 *
 * Revision 1.12  1997/02/18 22:23:38  bnoble
 * Rename lockdebug to cfs_lockdebug
 *
 * Revision 1.11  1997/02/13 18:46:14  rvb
 * Name CODA FS for df
 *
 * Revision 1.10  1997/02/12 15:32:05  rvb
 * Make statfs return values like for AFS
 *
 * Revision 1.9  1997/01/30 16:42:02  bnoble
 * Trace version as of SIGCOMM submission.  Minor fix in cfs_nb_open
 *
 * Revision 1.8  1997/01/13 17:11:05  bnoble
 * Coda statfs needs to return something other than -1 for blocks avail. and
 * files available for wabi (and other windowsish) programs to install
 * there correctly.
 *
 * Revision 1.6  1996/12/05 16:20:14  bnoble
 * Minor debugging aids
 *
 * Revision 1.5  1996/11/25 18:25:11  bnoble
 * Added a diagnostic check for cfs_nb_lock
 *
 * Revision 1.4  1996/11/13 04:14:19  bnoble
 * Merging BNOBLE_WORK_6_20_96 into main line
 *
 *
 * Revision 1.3  1996/11/08 18:06:11  bnoble
 * Minor changes in vnode operation signature, VOP_UPDATE signature, and
 * some newly defined bits in the include files.
 *
 * Revision 1.2.8.1  1996/06/26 16:28:26  bnoble
 * Minor bug fixes
 *
 * Revision 1.2  1996/01/02 16:56:52  bnoble
 * Added support for Coda MiniCache and raw inode calls (final commit)
 *
 * Revision 1.1.2.1  1995/12/20 01:57:17  bnoble
 * Added CFS-specific files
 *
 */

/* NetBSD-specific routines for the cfs code */
#ifdef __NetBSD__

#include <cfs/cfs.h>
#include <cfs/cfs_vnodeops.h>
#include <cfs/cnode.h>

#include <sys/fcntl.h>

/* What we are delaying for in printf */
int cfs_printf_delay = 0;  /* in microseconds */
static int cfs_lockdebug = 0;

/* Definition of the vfs operation vector */

/*
 * Some NetBSD details:
 * 
 *   cfs_start is called at the end of the mount syscall.
 *
 *   cfs_init is called at boot time.
 */

extern int cfsdebug;

int cfs_vnop_print_entry = 0;

#ifdef __GNUC__
#define ENTRY    \
   if(cfs_vnop_print_entry) myprintf(("Entered %s\n",__FUNCTION__))
#else
#define ENTRY
#endif 

/* NetBSD interface to statfs */
int cfs_nb_statfs    __P((VFS_T *, struct statfs *, struct proc *));

struct vfsops cfs_vfsops = {
    MOUNT_CFS,
    cfs_mount,
    cfs_start,
    cfs_unmount,
    cfs_root,
    cfs_quotactl,
    cfs_nb_statfs,
    cfs_sync,
    cfs_vget,
    (int (*) (struct mount *, struct fid *, struct mbuf *, struct vnode **,
	      int *, struct ucred **))
	eopnotsupp,
    (int (*) (struct vnode *, struct fid *)) eopnotsupp,
    cfs_init,
    0
};

/* NetBSD interfaces to the vnodeops */
int cfs_nb_open      __P((void *));
int cfs_nb_close     __P((void *));
int cfs_nb_read      __P((void *));
int cfs_nb_write     __P((void *));
int cfs_nb_ioctl     __P((void *));
int cfs_nb_select    __P((void *));
int cfs_nb_getattr   __P((void *));
int cfs_nb_setattr   __P((void *));
int cfs_nb_access    __P((void *));
int cfs_nb_readlink  __P((void *));
int cfs_nb_abortop   __P((void *));
int cfs_nb_fsync     __P((void *));
int cfs_nb_inactive  __P((void *));
int cfs_nb_lookup    __P((void *));
int cfs_nb_create    __P((void *));
int cfs_nb_remove    __P((void *));
int cfs_nb_link      __P((void *));
int cfs_nb_rename    __P((void *));
int cfs_nb_mkdir     __P((void *));
int cfs_nb_rmdir     __P((void *));
int cfs_nb_symlink   __P((void *));
int cfs_nb_readdir   __P((void *));
int cfs_nb_bmap      __P((void *));
int cfs_nb_strategy  __P((void *));
int cfs_nb_lock      __P((void *));
int cfs_nb_unlock    __P((void *));
int cfs_nb_islocked  __P((void *));
int nbsd_vop_error   __P((void *));
int nbsd_vop_nop     __P((void *));
int cfs_nb_reclaim   __P((void *));

/* Definition of the vnode operation vector */

int (**cfs_vnodeop_p)();
struct vnodeopv_entry_desc cfs_vnodeop_entries[] = {
    { &vop_default_desc, nbsd_vop_error },
    { &vop_lookup_desc, cfs_nb_lookup },           /* lookup */
    { &vop_create_desc, cfs_nb_create },		/* create */
    { &vop_mknod_desc, nbsd_vop_error },	/* mknod */
    { &vop_open_desc, cfs_nb_open },		/* open */
    { &vop_close_desc, cfs_nb_close },		/* close */
    { &vop_access_desc, cfs_nb_access },		/* access */
    { &vop_getattr_desc, cfs_nb_getattr },		/* getattr */
    { &vop_setattr_desc, cfs_nb_setattr },		/* setattr */
    { &vop_read_desc, cfs_nb_read },		/* read */
    { &vop_write_desc, cfs_nb_write },		/* write */
    { &vop_lease_desc, nbsd_vop_nop },          /* lease */
    { &vop_ioctl_desc, cfs_nb_ioctl },		/* ioctl */
    { &vop_select_desc, cfs_nb_select },		/* select */
    { &vop_mmap_desc, nbsd_vop_error },	/* mmap */
    { &vop_fsync_desc, cfs_nb_fsync },		/* fsync */
    { &vop_seek_desc, nbsd_vop_error },	/* seek */
    { &vop_remove_desc, cfs_nb_remove },		/* remove */
    { &vop_link_desc, cfs_nb_link },		/* link */
    { &vop_rename_desc, cfs_nb_rename },		/* rename */
    { &vop_mkdir_desc, cfs_nb_mkdir },		/* mkdir */
    { &vop_rmdir_desc, cfs_nb_rmdir },		/* rmdir */
    { &vop_symlink_desc, cfs_nb_symlink },		/* symlink */
    { &vop_readdir_desc, cfs_nb_readdir },		/* readdir */
    { &vop_readlink_desc, cfs_nb_readlink },	/* readlink */
    { &vop_abortop_desc, cfs_nb_abortop },	/* abortop */
    { &vop_inactive_desc, cfs_nb_inactive },	/* inactive */
    { &vop_reclaim_desc, cfs_nb_reclaim },	/* reclaim */
    { &vop_lock_desc, cfs_nb_lock },	/* lock */
    { &vop_unlock_desc, cfs_nb_unlock },	/* unlock */
    { &vop_bmap_desc, cfs_nb_bmap },		/* bmap */
    { &vop_strategy_desc, cfs_nb_strategy },	/* strategy */
    { &vop_print_desc, nbsd_vop_error },	/* print */
    { &vop_islocked_desc, cfs_nb_islocked },	/* islocked */
    { &vop_pathconf_desc, nbsd_vop_error },	/* pathconf */
    { &vop_advlock_desc, nbsd_vop_nop },	/* advlock */
    { &vop_blkatoff_desc, nbsd_vop_error },	/* blkatoff */
    { &vop_valloc_desc, nbsd_vop_error },	/* valloc */
    { &vop_vfree_desc, nbsd_vop_error },	/* vfree */
    { &vop_truncate_desc, nbsd_vop_error },	/* truncate */
    { &vop_update_desc, nbsd_vop_error },	/* update */
    { &vop_bwrite_desc, nbsd_vop_error },	/* bwrite */
    { (struct vnodeop_desc*)NULL, (int(*)())NULL }
};

/* NetBSD statfs */
/*
 * Get file system statistics.
 */
int
cfs_nb_statfs(vfsp, sbp, p)
    register VFS_T *vfsp;
    struct statfs *sbp;
    struct proc *p;
{
    bzero(sbp, sizeof(struct statfs));
    /* XXX - what to do about f_flags, others? --bnoble */
    /* Below This is what AFS does */
    /* Note: Normal fs's have a bsize of 0x400 == 1024 */
    sbp->f_type = 0;
    sbp->f_bsize = 8192; /* XXX */
    sbp->f_iosize = 8192; /* XXX */
#define NB_SFS_SIZ 0x895440
    sbp->f_blocks = NB_SFS_SIZ;
    sbp->f_bfree = NB_SFS_SIZ;
    sbp->f_bavail = NB_SFS_SIZ;
    sbp->f_files = NB_SFS_SIZ;
    sbp->f_ffree = NB_SFS_SIZ;
    bcopy((caddr_t)&(VFS_FSID(vfsp)), (caddr_t)&(sbp->f_fsid),
	  sizeof (fsid_t));
    strncpy(sbp->f_fstypename, MOUNT_CFS, MFSNAMELEN-1);
    strcpy(sbp->f_mntonname, "/coda");
    strcpy(sbp->f_mntfromname, "CFS");
    return(0);
}



/* Definitions of NetBSD vnodeop interfaces */

/* A generic panic: we were called with something we didn't define yet */
int
nbsd_vop_error(void *anon) {
    struct vnodeop_desc **desc = (struct vnodeop_desc **)anon;

    myprintf(("Vnode operation %s called, but not defined\n",
	      (*desc)->vdesc_name));
    panic("nbsd_vop_error");
    return 0;
}

/* A generic do-nothing.  For lease_check, advlock */
int
nbsd_vop_nop(void *anon) {
    struct vnodeop_desc **desc = (struct vnodeop_desc **)anon;

    if (cfsdebug) {
	myprintf(("Vnode operation %s called, but unsupported\n",
		  (*desc)->vdesc_name));
    }
    return (0);
}

int
cfs_nb_open(v)
    void *v;
{
    struct vop_open_args *ap = v;

    ENTRY;

     /* 
      * NetBSD can pass the O_EXCL flag in mode, even though the check
      * has already happened.  Venus defensively assumes that if open
      * is passed the EXCL, it must be a bug.  We strip the flag here.
      */
     return (cfs_open(&(ap->a_vp), ap->a_mode & (~O_EXCL), ap->a_cred, 
                    ap->a_p));
}

int
cfs_nb_close(v)
    void *v;
{
    struct vop_close_args *ap = v;

    ENTRY;
    return (cfs_close(ap->a_vp, ap->a_fflag, ap->a_cred, ap->a_p));
}

int
cfs_nb_read(v)
    void *v;
{
    struct vop_read_args *ap = v;

    ENTRY;
    return(cfs_rdwr(ap->a_vp, ap->a_uio, UIO_READ,
		    ap->a_ioflag, ap->a_cred, ap->a_uio->uio_procp));
}

int
cfs_nb_write(v)
    void *v;
{
    struct vop_write_args *ap = v;

    ENTRY;
    return(cfs_rdwr(ap->a_vp, ap->a_uio, UIO_WRITE,
		    ap->a_ioflag, ap->a_cred, ap->a_uio->uio_procp));
}

int
cfs_nb_ioctl(v)
    void *v;
{
    struct vop_ioctl_args *ap = v;

    ENTRY;
    return (cfs_ioctl(ap->a_vp, ap->a_command, ap->a_data, ap->a_fflag,
		      ap->a_cred, ap->a_p));
}

int
cfs_nb_select(v)
    void *v;
{
    struct vop_select_args *ap = v;

    ENTRY;
    return (cfs_select(ap->a_vp, ap->a_which, ap->a_cred, ap->a_p));
}

int
cfs_nb_getattr(v)
    void *v;
{
    struct vop_getattr_args *ap = v;

    ENTRY;
    return (cfs_getattr(ap->a_vp, ap->a_vap, ap->a_cred, ap->a_p));
}

int
cfs_nb_setattr(v)
    void *v;
{
    struct vop_setattr_args *ap = v;

    ENTRY;
    return (cfs_setattr(ap->a_vp, ap->a_vap, ap->a_cred, ap->a_p));
}

int
cfs_nb_access(v)
    void *v;
{
    struct vop_access_args *ap = v;

    ENTRY;
    return (cfs_access(ap->a_vp, ap->a_mode, ap->a_cred, ap->a_p));
}

int
cfs_nb_readlink(v)
    void *v;
{
    struct vop_readlink_args *ap = v;

    ENTRY;
    return (cfs_readlink(ap->a_vp, ap->a_uio, ap->a_cred, 
			 ap->a_uio->uio_procp));
}


/*
 * CFS abort op, called after namei() when a CREATE/DELETE isn't actually
 * done. If a buffer has been saved in anticipation of a cfs_create or
 * a cfs_remove, delete it.
 */
/* ARGSUSED */
int
cfs_nb_abortop(v)
    void *v;
{
	struct vop_abortop_args /* {
		struct vnode *a_dvp;
		struct componentname *a_cnp;
	} */ *ap = v;

	if ((ap->a_cnp->cn_flags & (HASBUF | SAVESTART)) == HASBUF)
		free(ap->a_cnp->cn_pnbuf, M_NAMEI);
	return (0);
}

int
cfs_nb_fsync(v)
    void *v;
{
    struct vop_fsync_args *ap = v;

    ENTRY;
    return (cfs_fsync(ap->a_vp, ap->a_cred, ap->a_p));
}

int
cfs_nb_inactive(v)
    void *v;
{
    struct vop_inactive_args *ap = v;

    ENTRY;
    /* XXX - at the moment, inactive doesn't look at cred, and doesn't
       have a proc pointer.  Oops. */
    return (cfs_inactive(ap->a_vp, NULL, GLOBAL_PROC));
}

int
cfs_nb_lookup(v)
    void *v;
{
    struct vop_lookup_args *ap = v;

    /* 
     * It looks as though ap->a_cnp->ni_cnd->cn_nameptr holds the rest
     * of the string to xlate, and that we must try to get at least
     * ap->a_cnp->ni_cnd->cn_namelen of those characters to macth.  I
     * could be wrong. 
     */
    char                   tname[MAXPATHLEN];   /* overkill, but... */
    struct componentname  *cnp = ap->a_cnp;
    int                    result;
    
    ENTRY;
    bcopy(cnp->cn_nameptr, tname,  cnp->cn_namelen);
    tname[cnp->cn_namelen] = '\0';

    result = cfs_lookup(ap->a_dvp, tname, ap->a_vpp, cnp->cn_cred, 
			cnp->cn_proc);

    /* 
     * If we are creating, and this was the last name to be looked up,
     * and the error was ENOENT, then there really shouldn't be an
     * error and we can make the leaf NULL and return success.  Since
     * this is supposed to work under Mach as well as NetBSD, we're
     * leaving this fn wrapped.  We also must tell lookup/namei that
     * we need to save the last component of the name.  (Create will
     * have to free the name buffer later...lucky us...)
     */
    if (((cnp->cn_nameiop == CREATE) || (cnp->cn_nameiop == RENAME))
	&& (cnp->cn_flags & ISLASTCN)
	&& (result == ENOENT))
    {
	result = EJUSTRETURN;
	cnp->cn_flags |= SAVENAME;
	*ap->a_vpp = NULL;
    }

    /* 
     * If we are removing, and we are at the last element, and we
     * found it, then we need to keep the name around so that the
     * removal will go ahead as planned.  Unfortunately, this will
     * probably also lock the to-be-removed vnode, which may or may
     * not be a good idea.  I'll have to look at the bits of
     * cfs_remove to make sure.  We'll only save the name if we did in
     * fact find the name, otherwise cfs_nb_remove won't have a chance
     * to free the pathname.  
     */
    if ((cnp->cn_nameiop == DELETE)
	&& (cnp->cn_flags & ISLASTCN)
	&& !result)
    {
	cnp->cn_flags |= SAVENAME;
    }

    /* 
     * If the lookup went well, we need to (potentially?) unlock the
     * parent, and lock the child.  We are only responsible for
     * checking to see if the parent is supposed to be unlocked before
     * we return.  We must always lock the child (provided there is
     * one, and (the parent isn't locked or it isn't the same as the
     * parent.)  Simple, huh?  We can never leave the parent locked unless
     * we are ISLASTCN
     */
    if (!result || (result == EJUSTRETURN)) {
	if (!(cnp->cn_flags & LOCKPARENT) || !(cnp->cn_flags & ISLASTCN)) {
	    if ((result = VOP_UNLOCK(ap->a_dvp))) {
		return result; 
	    }	    
	    /* 
	     * The parent is unlocked.  As long as there is a child,
	     * lock it without bothering to check anything else. 
	     */
	    if (*ap->a_vpp) {
		if ((result = VOP_LOCK(*ap->a_vpp))) {
		    printf("cfs_nb_lookup: ");
		    panic("unlocked parent but couldn't lock child");
		}
	    }
	} else {
	    /* The parent is locked, and may be the same as the child */
	    if (*ap->a_vpp && (*ap->a_vpp != ap->a_dvp)) {
		/* Different, go ahead and lock it. */
		if ((result = VOP_LOCK(*ap->a_vpp))) {
		    printf("cfs_nb_lookup: ");
		    panic("unlocked parent but couldn't lock child");
		}
	    }
	}
    } else {
	/* If the lookup failed, we need to ensure that the leaf is NULL */
	/* Don't change any locking? */
	*ap->a_vpp = NULL;
    }
    return result;
}

int
cfs_nb_create(v)
    void *v;
{
    struct vop_create_args *ap = v;
    char                   tname[MAXPATHLEN];   /* overkill, but... */
    struct componentname  *cnp = ap->a_cnp;
    int                    result;

    ENTRY;
    /* All creates are exclusive XXX */
    /* I'm assuming the 'mode' argument is the file mode bits XXX */
    bcopy(cnp->cn_nameptr, tname,  cnp->cn_namelen);
    tname[cnp->cn_namelen] = '\0';

    result = cfs_create(ap->a_dvp, tname, ap->a_vap, EXCL, 
			ap->a_vap->va_mode, ap->a_vpp, cnp->cn_cred,
			cnp->cn_proc);

    /* Locking strategy. */
    /*
     * In NetBSD, all creates must explicitly vput their dvp's.  We'll
     * go ahead and use the LOCKLEAF flag of the cnp argument.
     * However, I'm pretty sure that create must return the leaf
     * locked; so there is a DIAGNOSTIC check to ensure that this is
     * true.
     */
    vput(ap->a_dvp);
    if (!result) {
	if (cnp->cn_flags & LOCKLEAF) {
	    if ((result = VOP_LOCK(*ap->a_vpp))) {
		printf("cfs_nb_create: ");
		panic("unlocked parent but couldn't lock child");
	    }
	}
#ifdef DIAGNOSTIC
	else {
	    printf("cfs_nb_create: LOCKLEAF not set!\n");
	}
#endif /* DIAGNOSTIC */
    }
    /* Have to free the previously saved name */
    /* 
     * This condition is stolen from ufs_makeinode.  I have no idea
     * why it's here, but what the hey...
     */
    if ((cnp->cn_flags & SAVESTART) == 0) {
	FREE(cnp->cn_pnbuf, M_NAMEI);
    }
    return result;
}

int
cfs_nb_remove(v)
    void *v;
{
    struct vop_remove_args *ap = v;
    char                   tname[MAXPATHLEN];   /* overkill, but... */
    struct componentname  *cnp = ap->a_cnp;
    int                    result;

    ENTRY;
    bcopy(cnp->cn_nameptr, tname,  cnp->cn_namelen);
    tname[cnp->cn_namelen] = '\0';

    result = cfs_remove(ap->a_dvp, tname, cnp->cn_cred, cnp->cn_proc);

    /* 
     * Regardless of what happens, we have to unconditionally drop
     * locks/refs on parent and child.  (I hope).  This is based on
     * what ufs_remove seems to be doing.
     */
    if (ap->a_dvp == ap->a_vp) {
	vrele(ap->a_vp);
    } else {
	vput(ap->a_vp);
    }
    vput(ap->a_dvp);

    if ((cnp->cn_flags & SAVESTART) == 0) {
	FREE(cnp->cn_pnbuf, M_NAMEI);
    }
    return (result);
}

int
cfs_nb_link(v)
    void *v;
{
    struct vop_link_args *ap = v;
    char                   tname[MAXPATHLEN];   /* overkill, but... */
    struct componentname  *cnp = ap->a_cnp;
    int                    result;

    ENTRY;

    if (cfsdebug & CFSDBGMSK(CFS_LINK)) {
	struct cnode *cp;
	struct cnode *tdcp;

	cp = VTOC(ap->a_vp);
	tdcp = VTOC(ap->a_dvp);
	myprintf(("nb_link:   vp fid: (%x.%x.%x)\n",
		  cp->c_fid.Volume, cp->c_fid.Vnode, cp->c_fid.Unique));
	myprintf(("nb_link: tdvp fid: (%x.%x.%x)\n",
		  tdcp->c_fid.Volume, tdcp->c_fid.Vnode, tdcp->c_fid.Unique));
	
    }

    bcopy(cnp->cn_nameptr, tname,  cnp->cn_namelen);
    tname[cnp->cn_namelen] = '\0';

    /*
     * According to the ufs_link operation here's the locking situation:
     *     We enter with the thing called "dvp" (the directory) locked.
     *     We must unconditionally drop locks on "dvp"
     *
     *     We enter with the thing called "vp" (the linked-to) unlocked,
     *       but ref'd (?)
     *     We seem to need to lock it before calling cfs_link, and
     *       unconditionally unlock it after.
     */
    
    if ((ap->a_vp != ap->a_dvp) && (result = VOP_LOCK(ap->a_vp))) {
	goto exit;
    }
	
    result = cfs_link(ap->a_vp, ap->a_dvp, tname, cnp->cn_cred, 
		      cnp->cn_proc);

 exit:

    if (ap->a_vp != ap->a_dvp) {
	VOP_UNLOCK(ap->a_vp);
    }
    vput(ap->a_dvp);

    /* Drop the name buffer if we don't need to SAVESTART */
    if ((cnp->cn_flags & SAVESTART) == 0) {
	FREE(cnp->cn_pnbuf, M_NAMEI);
    }
    
    return result;
}

int
cfs_nb_rename(v)
    void *v;
{
    struct vop_rename_args *ap = v;
    char                   fname[MAXPATHLEN];   /* overkill, but... */
    char                   tname[MAXPATHLEN];   /* overkill, but... */
    struct componentname  *fcnp = ap->a_fcnp;
    struct componentname  *tcnp = ap->a_tcnp;
    int                    result;

    ENTRY;
    bcopy(fcnp->cn_nameptr, fname,  fcnp->cn_namelen);
    bcopy(tcnp->cn_nameptr, tname,  tcnp->cn_namelen);
    fname[fcnp->cn_namelen] = '\0';
    tname[tcnp->cn_namelen] = '\0';

    /* Hmmm.  The vnodes are already looked up.  Perhaps they are locked?
       This could be Bad. XXX */
#ifdef DIAGNOSTIC
    if ((fcnp->cn_cred != tcnp->cn_cred)
	|| (fcnp->cn_proc != tcnp->cn_proc))
    {
	panic("cfs_nb_rename: component names don't agree");
    }
#endif DIAGNOSTIC
    result = cfs_rename(ap->a_fdvp, fname, ap->a_tdvp, tname, fcnp->cn_cred,
			fcnp->cn_proc);
    
    /* XXX - do we need to call cache pureg on the moved vnode? */
    cache_purge(ap->a_fvp);

    /* It seems to be incumbent on us to drop locks on all four vnodes */
    /* From-vnodes are not locked, only ref'd.  To-vnodes are locked. */
    
    vrele(ap->a_fvp);
    vrele(ap->a_fdvp);

    if (ap->a_tvp) {
	if (ap->a_tvp == ap->a_tdvp) {
	    vrele(ap->a_tvp);
	} else {
	    vput(ap->a_tvp);
	}
    }

    vput(ap->a_tdvp);
    
    return result;
}

int
cfs_nb_mkdir(v)
    void *v;
{
    struct vop_mkdir_args *ap = v;
    char                   tname[MAXPATHLEN];   /* overkill, but... */
    struct componentname  *cnp = ap->a_cnp;
    int                    result;

    ENTRY;
    bcopy(cnp->cn_nameptr, tname,  cnp->cn_namelen);
    tname[cnp->cn_namelen] = '\0';

    result = cfs_mkdir(ap->a_dvp, tname, ap->a_vap, ap->a_vpp, cnp->cn_cred,
		       cnp->cn_proc);
    /*
     * Currently, all mkdirs explicitly vput their dvp's.
     * It also appears that we *must* lock the vpp, since
     * lockleaf isn't set, but someone down the road is going
     * to try to unlock the new directory.
     */
    vput(ap->a_dvp);
    if (!result) {
	if ((result = VOP_LOCK(*ap->a_vpp))) {
	    panic("cfs_nb_mkdir: couldn't lock child");
	}
    }
    /* Have to free the previously saved name */
    /* 
     * ufs_mkdir doesn't check for SAVESTART before freeing the
     * pathname buffer, but ufs_create does.  For the moment, I'll
     * follow their lead, but this seems like it is probably
     * incorrect.  
     */
    FREE(cnp->cn_pnbuf, M_NAMEI);
    return (result);
}

int
cfs_nb_rmdir(v)
    void *v;
{
    struct vop_rmdir_args *ap = v;
    char                   tname[MAXPATHLEN];   /* overkill, but... */
    struct componentname  *cnp = ap->a_cnp;
    int                    result;

    ENTRY;
    bcopy(cnp->cn_nameptr, tname,  cnp->cn_namelen);
    tname[cnp->cn_namelen] = '\0';

    result = cfs_rmdir(ap->a_dvp, tname, cnp->cn_cred, cnp->cn_proc);
    
    /*
     * regardless of what happens, we need to drop locks/refs on the 
     * parent and child.  I think. 
     */
    if (ap->a_dvp == ap->a_vp) {
	vrele(ap->a_vp);
    } else {
	vput(ap->a_vp);
    }
    vput(ap->a_dvp);

    if ((cnp->cn_flags & SAVESTART) == 0) {
	FREE(cnp->cn_pnbuf, M_NAMEI);
    }
    return (result);
}

int
cfs_nb_symlink(v)
    void *v;
{
    struct vop_symlink_args *ap = v;
    /* 
     * XXX I'm assuming the following things about cfs_symlink's
     * arguments: 
     *       t(foo) is the new name/parent/etc being created.
     *       lname is the contents of the new symlink. 
     */
    char                   tname[MAXPATHLEN];   /* overkill, but... */
    struct componentname  *cnp = ap->a_cnp;
    int                    result;

    ENTRY;
    bcopy(cnp->cn_nameptr, tname,  cnp->cn_namelen);
    tname[cnp->cn_namelen] = '\0';

    /* XXX What about the vpp argument?  Do we need it? */
    /* 
     * Here's the strategy for the moment: perform the symlink, then
     * do a lookup to grab the resulting vnode.  I know this requires
     * two communications with Venus for a new sybolic link, but
     * that's the way the ball bounces.  I don't yet want to change
     * the way the Mach symlink works.  When Mach support is
     * deprecated, we should change symlink so that the common case
     * returns the resultant vnode in a vpp argument.
     */

    result = cfs_symlink(ap->a_dvp, tname, ap->a_vap, ap->a_target,
			cnp->cn_cred, cnp->cn_proc);

    if (!result) {
	result = cfs_lookup(ap->a_dvp, tname, ap->a_vpp, cnp->cn_cred,
			    cnp->cn_proc);
    }
    
    /* 
     * Okay, now we have to drop locks on dvp.  vpp is unlocked, but
     * ref'd.  It doesn't matter what happens in either symlink or
     * lookup.  Furthermore, there isn't any way for (dvp == *vpp), so
     * we don't bother checking.  
     */
    
    vput(ap->a_dvp);
    if (*ap->a_vpp) VN_RELE(*ap->a_vpp);

    /* 
     * Free the name buffer 
     */
    if ((cnp->cn_flags & SAVESTART) == 0) {
	FREE(cnp->cn_pnbuf, M_NAMEI);
    }
    return result;
}

int
cfs_nb_readdir(v)
    void *v;
{
    struct vop_readdir_args *ap = v;

    ENTRY;
    return (cfs_readdir(ap->a_vp, ap->a_uio, ap->a_cred, ap->a_eofflag,
			ap->a_cookies, ap->a_ncookies, ap->a_uio->uio_procp));
}

int
cfs_nb_bmap(v)
    void *v;
{
    struct vop_bmap_args *ap = v;

    /* XXX on the global proc */
    ENTRY;

    return (cfs_bmap(ap->a_vp, ap->a_bn, ap->a_vpp, ap->a_bnp, GLOBAL_PROC));
}

int
cfs_nb_strategy(v)
    void *v;
{
    struct vop_strategy_args *ap = v;

    ENTRY;
    /* XXX  for the GLOBAL_PROC */
    return (cfs_strategy(ap->a_bp, GLOBAL_PROC));
}

/***************************** NetBSD-only vnode operations */
int
cfs_nb_reclaim(v) 
    void *v;
{
    struct vop_reclaim_args *ap = v;
    struct vnode *vp = ap->a_vp;

    ENTRY;
#ifdef DIAGNOSTIC
    if (vp->v_usecount != 0) 
	vprint("cfs_nb_reclaim: pushing active", vp);
    if (VTOC(vp)->c_ovp)
	panic("cfs_nb_reclaim: c_ovp not void");
#endif DIAGNOSTIC
    cache_purge(vp);
    cfs_free(VTOC(vp));
    VTOC(vp) = NULL;
    return (0);
}

int
cfs_nb_lock(v)
    void *v;
{
    struct vop_lock_args *ap = v;
    struct vnode *vp = ap->a_vp;
    struct cnode *cp;
    struct proc  *p = curproc; /* XXX */
    
    ENTRY;
    cp = VTOC(vp);

    if (cfs_lockdebug) {
	myprintf(("Attempting lock on %d.%d.%d\n",
		  cp->c_fid.Volume, cp->c_fid.Vnode, cp->c_fid.Unique));
    }
 start:
    while (vp->v_flag & VXLOCK) {
	vp->v_flag |= VXWANT;
	sleep((caddr_t)vp, PINOD);
    }
    if (vp->v_tag == VT_NON)
	return (ENOENT);

    if (cp->c_flags & CN_LOCKED) {
	cp->c_flags |= CN_WANTED;
#ifdef DIAGNOSTIC
	myprintf(("cfs_nb_lock: lock contention\n"));
#endif
	(void) sleep((caddr_t)cp, PINOD);
#ifdef DIAGNOSTIC
	myprintf(("cfs_nb_lock: contention resolved\n"));
#endif
	goto start;
    }
    cp->c_flags |= CN_LOCKED;
    return (0);
}

int
cfs_nb_unlock(v)
    void *v;
{
    struct vop_unlock_args *ap = v;
    struct cnode *cp = VTOC(ap->a_vp);

    ENTRY;
    if (cfs_lockdebug) {
	myprintf(("Attempting unlock on %d.%d.%d\n",
		  cp->c_fid.Volume, cp->c_fid.Vnode, cp->c_fid.Unique));
    }
#ifdef DIAGNOSTIC
    if ((cp->c_flags & CN_LOCKED) == 0) 
	panic("cfs_unlock: not locked");
#endif
    cp->c_flags &= ~CN_LOCKED;
    if (cp->c_flags & CN_WANTED) {
	cp->c_flags &= ~CN_WANTED;
	wakeup((caddr_t)cp);
    }
    return (0);
}

int
cfs_nb_islocked(v)
    void *v;
{
    struct vop_islocked_args *ap = v;

    ENTRY;
    if (VTOC(ap->a_vp)->c_flags & CN_LOCKED)
	return (1);
    return (0);
}

struct vnodeopv_desc cfs_vnodeop_opv_desc = 
        { &cfs_vnodeop_p, cfs_vnodeop_entries };

/* How one looks up a vnode given a device/inode pair: */

int
cfs_grab_vnode(dev_t dev, ino_t ino, struct vnode **vpp)
{
    /* This is like VFS_VGET() or igetinode()! */
    int           error;
    struct mount *mp;

    if (!(mp = devtomp(dev))) {
	myprintf(("cfs_grab_vnode: devtomp(%d) returns NULL\n", dev));
	return(ENXIO);
    }

    /* XXX - ensure that nonzero-return means failure */
    error = VFS_VGET(mp,ino,vpp);
    if (error) {
	myprintf(("cfs_grab_vnode: iget/vget(%d, %d) returns %x, err %d\n", 
		  dev, ino, *vpp, error));
	return(ENOENT);
    }
    return(0);
}

void
print_vattr( attr )
	struct vattr *attr;
{
    char *typestr;

    switch (attr->va_type) {
    case VNON:
	typestr = "VNON";
	break;
    case VREG:
	typestr = "VREG";
	break;
    case VDIR:
	typestr = "VDIR";
	break;
    case VBLK:
	typestr = "VBLK";
	break;
    case VCHR:
	typestr = "VCHR";
	break;
    case VLNK:
	typestr = "VLNK";
	break;
    case VSOCK:
	typestr = "VSCK";
	break;
    case VFIFO:
	typestr = "VFFO";
	break;
    case VBAD:
	typestr = "VBAD";
	break;
    default:
	typestr = "????";
	break;
    }


    myprintf(("attr: type %s mode %d uid %d gid %d fsid %d rdev %d\n",
	      typestr, (int)attr->va_mode, (int)attr->va_uid,
	      (int)attr->va_gid, (int)attr->va_fsid, (int)attr->va_rdev));
    
    myprintf(("      fileid %d nlink %d size %d blocksize %d bytes %d\n",
	      (int)attr->va_fileid, (int)attr->va_nlink, 
	      (int)attr->va_size,
	      (int)attr->va_blocksize,(int)attr->va_bytes));
    myprintf(("      gen %ld flags %ld vaflags %d\n",
	      attr->va_gen, attr->va_flags, attr->va_vaflags));
    myprintf(("      atime sec %d nsec %d\n",
	      (int)attr->va_atime.tv_sec, (int)attr->va_atime.tv_nsec));
    myprintf(("      mtime sec %d nsec %d\n",
	      (int)attr->va_mtime.tv_sec, (int)attr->va_mtime.tv_nsec));
    myprintf(("      ctime sec %d nsec %d\n",
	      (int)attr->va_ctime.tv_sec, (int)attr->va_ctime.tv_nsec));
}

/* How to print a ucred */
print_cred(cred)
	struct ucred *cred;
{

	int i;

	myprintf(("ref %d\tuid %d\n",cred->cr_ref,cred->cr_uid));

	for (i=0; i < cred->cr_ngroups; i++)
		myprintf(("\tgroup %d: (%d)\n",i,cred->cr_groups[i]));
	myprintf(("\n"));

}


/* vcfsattach: do nothing */
void
vcfsattach(n)
    int n;
{
}

#endif __NetBSD__
