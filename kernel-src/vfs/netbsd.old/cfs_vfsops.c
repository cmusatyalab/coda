/* 
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */

/*
 * This code was written for the Coda file system at Carnegie Mellon
 * University.  Contributers include David Steere, James Kistler, and
 * M. Satyanarayanan.  
 */

/*
 * HISTORY
 * cfs_vfsops.c,v
 * Revision 1.3  1996/11/08 18:06:12  bnoble
 * Minor changes in vnode operation signature, VOP_UPDATE signature, and
 * some newly defined bits in the include files.
 *
 * Revision 1.2  1996/01/02 16:57:04  bnoble
 * Added support for Coda MiniCache and raw inode calls (final commit)
 *
 * Revision 1.1.2.1  1995/12/20 01:57:32  bnoble
 * Added CFS-specific files
 *
 * Revision 3.1.1.1  1995/03/04  19:08:02  bnoble
 * Branch for NetBSD port revisions
 *
 * Revision 3.1  1995/03/04  19:08:01  bnoble
 * Bump to major revision 3 to prepare for NetBSD port
 *
 * Revision 2.4  1995/02/17  16:25:22  dcs
 * These versions represent several changes:
 * 1. Allow venus to restart even if outstanding references exist.
 * 2. Have only one ctlvp per client, as opposed to one per mounted cfs device.d
 * 3. Allow ody_expand to return many members, not just one.
 *
 * Revision 2.3  94/10/14  09:58:21  dcs
 * Made changes 'cause sun4s have braindead compilers
 * 
 * Revision 2.2  94/10/12  16:46:33  dcs
 * Cleaned kernel/venus interface by removing XDR junk, plus
 * so cleanup to allow this code to be more easily ported.
 * 
 * Revision 1.3  93/05/28  16:24:29  bnoble
 * *** empty log message ***
 * 
 * Revision 1.2  92/10/27  17:58:24  lily
 * merge kernel/latest and alpha/src/cfs
 * 
 * Revision 2.3  92/09/30  14:16:32  mja
 * 	Added call to cfs_flush to cfs_unmount.
 * 	[90/12/15            dcs]
 * 
 * 	Added contributors blurb.
 * 	[90/12/13            jjk]
 * 
 * Revision 2.2  90/07/05  11:26:40  mrt
 * 	Created for the Coda File System.
 * 	[90/05/23            dcs]
 * 
 * Revision 1.3  90/05/31  17:01:42  dcs
 * Prepare for merge with facilities kernel.
 * 
 * 
 */ 

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/types.h>
#include <cfs/cfs.h>
#include <cfs/cnode.h>
#include <cfs/cfs_opstats.h>

int cfsdebug = 0;

int cfs_vfsop_print_entry = 0;

#ifdef __GNUC__
#define ENTRY    \
    if(cfs_vfsop_print_entry) myprintf(("Entered %s\n",__FUNCTION__))
#else
#define ENTRY
#endif 


struct cfs_mntinfo cfs_mnttbl[NVCFS]; /* indexed by minor device number */

/* structure to keep statistics of internally generated/satisfied calls */

struct cfs_op_stats cfs_vfsopstats[CFS_VFSOPS_SIZE];

#define MARK_ENTRY(op) (cfs_vfsopstats[op].entries++)
#define MARK_INT_SAT(op) (cfs_vfsopstats[op].sat_intrn++)
#define MARK_INT_FAIL(op) (cfs_vfsopstats[op].unsat_intrn++)
#define MRAK_INT_GEN(op) (cfs_vfsopstats[op].gen_intrn++)

extern int cfsnc_initialized;     /* Set if cache has been initialized */

extern struct cdevsw cdevsw[];    /* For sanity check in cfs_mount */

cfs_vfsopstats_init()
{
	register int i;
	
	for (i=0;i<CFS_VFSOPS_SIZE;i++) {
		cfs_vfsopstats[i].opcode = i;
		cfs_vfsopstats[i].entries = 0;
		cfs_vfsopstats[i].sat_intrn = 0;
		cfs_vfsopstats[i].unsat_intrn = 0;
		cfs_vfsopstats[i].gen_intrn = 0;
	}
	
	return 0;
}
	

/*
 * cfs mount vfsop
 * Set up mount info record and attach it to vfs struct.
 */
/*ARGSUSED*/
cfs_mount(vfsp, path, data, ndp, p)
    VFS_T *vfsp;           /* Allocated and initialized by mount(2) */
    char *path;            /* path covered: ignored by the fs-layer */
    caddr_t data;          /* Need to define a data type for this in netbsd? */
    struct nameidata *ndp; /* Clobber this to lookup the device name */
    struct proc *p;        /* The ever-famous proc pointer */
{
    struct vnode *dvp;
    struct cnode *cp;
    dev_t dev;
    struct cfs_mntinfo *mi;
    struct vnode *rootvp;
    ViceFid rootfid;
    int error;

    ENTRY;

    if (!cfsnc_initialized) {
	cfs_vfsopstats_init();
	cfs_vnodeopstats_init();
	cfsnc_init();
    }
    
    MARK_ENTRY(CFS_MOUNT_STATS);
    if (CFS_MOUNTED(vfsp)) {
	MARK_INT_FAIL(CFS_MOUNT_STATS);
	return(EBUSY);
    }
    
    /* Validate mount device.  Similar to getmdev(). */
    DO_LOOKUP(data, UIO_USERSPACE, FOLLOW_LINK, NULL,
	      &dvp, p, ndp, error);
    if (error) {
	MARK_INT_FAIL(CFS_MOUNT_STATS);
	return (error);
    }
    if (VN_TYPE(dvp) != VCHR) {
	MARK_INT_FAIL(CFS_MOUNT_STATS);
	VN_RELE(dvp);
	return(ENXIO);
    }
    dev = VN_RDEV(dvp);
    VN_RELE(dvp);
    if (major(dev) >= nchrdev || major(dev) < 0) {
	MARK_INT_FAIL(CFS_MOUNT_STATS);
	return(ENXIO);
    }
    
    /*
     * See if the device table matches our expectations.
     */
    if (cdevsw[major(dev)].d_open != VCOPEN) {
	MARK_INT_FAIL(CFS_MOUNT_STATS);
	return(ENXIO);
    }
    
    if (minor(dev) >= NVCFS || minor(dev) < 0) {
	MARK_INT_FAIL(CFS_MOUNT_STATS);
	return(ENXIO);
    }
    
    /*
     * Initialize the mount record and link it to the vfs struct
     */
    mi = &cfs_mnttbl[minor(dev)];
    
    if (!VC_OPEN(&mi->mi_vcomm)) {
	MARK_INT_FAIL(CFS_MOUNT_STATS);
	return(ENODEV);
    }
    
    mi->mi_refct = 0;
    
    /* No initialization (here) of mi_vcomm! */
    vfsp->VFS_DATA = (VFS_ANON_T)mi;
    VFS_FSID(vfsp).val[0] = 0;
    VFS_FSID(vfsp).val[1] = makefstype(MOUNT_CFS);
    mi->mi_vfschain.vfsp = vfsp;
    mi->mi_vfschain.next = NULL;
    
    /*
     * Make a root vnode to placate the Vnode interface, but don't
     * actually make the CFS_ROOT call to venus until the first call
     * to cfs_root in case a server is down while venus is starting.
     */
    rootfid.Volume = 0;
    rootfid.Vnode = 0;
    rootfid.Unique = 0;
    cp = makecfsnode(&rootfid, vfsp, VDIR);
    rootvp = CTOV(cp);
    rootvp->v_flag |= VROOT;
    
    /* Add vfs and rootvp to chain of vfs hanging off mntinfo */
    ADD_VFS_TO_MNTINFO(mi, vfsp, rootvp);
    
    /* set filesystem block size */
    VFS_BSIZE(vfsp)	= 8192;	    /* XXX -JJK */
    
    /* error is currently guaranteed to be zero, but in case some
       code changes... */
    CFSDEBUG(1,
	     myprintf(("cfs_mount returned %d\n",error)););
    if (error)
	MARK_INT_FAIL(CFS_MOUNT_STATS);
    else
	MARK_INT_SAT(CFS_MOUNT_STATS);
    
    return(error);
}

int
cfs_start(vfsp, flags, p)
    VFS_T *vfsp;
    int flags;
    struct proc *p;
{
    ENTRY;
    return (0);
}

int
cfs_unmount(vfsp, mntflags, p)
    VFS_T *vfsp;
    int mntflags;
    struct proc *p;
{
    struct cfs_mntinfo *mi = vftomi(vfsp);
    struct ody_mntinfo *op, *pre;
    int active, error = 0;
    
    ENTRY;
    MARK_ENTRY(CFS_UMOUNT_STATS);
    if (!CFS_MOUNTED(vfsp)) {
	MARK_INT_FAIL(CFS_UMOUNT_STATS);
	return(EINVAL);
    }
    
    if (mi->mi_refct == 0) {
	/* Someone already unmounted this device. */
	myprintf(("Ackk! unmount called on ody-style vfsp!\n"));
	return EINVAL;
    }
    
    for (pre = NULL, op = &mi->mi_vfschain; op; pre = op, op = op->next) {
	if (op->vfsp == vfsp) {	/* We found the victim */
	    if (!IS_DYING(VTOC(op->rootvp)))
		return (EBUSY); 	/* Venus is still running */
	    
	    active = cfs_kill(vfsp);
	    
	    if (active > 1) {	/* 1 is for rootvp */
		error = EBUSY;
		
		/* HACK! Just for fun, if venus is dying/dead,
		 * let's fake the unmount to allow a new venus to
		 * start. All operations on outstanding cnodes
		 * will effectively fail, but shouldn't crash
		 * either the kernel or venus. One to blaim: --
		 * DCS 11/29/94 */
#ifndef __NetBSD__             /* XXX - NetBSD venii cannot fake unmount */
		FAKE_UNMOUNT(vfsp);
#else __NetBSD__
		return (error);
#endif __NetBSD__

		myprintf(("CFS_UNMOUNT: faking unmount, vfsp %x active == %d\n", vfsp, active));
	    } else {
		/* Do nothing, caller of cfs_unmount will do the unmounting */
	    }
	    
	    VN_RELE(op->rootvp);
	    /* Kill them again...we have to get rid of the rootvp */
	    active = cfs_kill(vfsp);
	    if (active) {
		panic("cfs_unmount: couldn't kill root vnode");
	    }
	    
	    /* I'm going to take this out to allow lookups to go through. I'm
	     * not sure it's important anyway. -- DCS 2/2/94
	     */
	    /* vfsp->VFS_DATA = NULL; */
	    
	    /* Remove the vfs from our list of valid coda-like vfs */
	    if (pre) {
		pre->next = op->next;
		CFS_FREE(op, sizeof(struct ody_mntinfo));
	    } else
		if (mi->mi_vfschain.next) {
		    mi->mi_vfschain.vfsp = (mi->mi_vfschain.next)->vfsp;
		    mi->mi_vfschain.rootvp = (mi->mi_vfschain.next)->rootvp;
		    mi->mi_vfschain.next = (mi->mi_vfschain.next)->next;
		} else {
			/* No more vfsp's to hold onto */
		    mi->mi_vfschain.vfsp = NULL;
		    mi->mi_vfschain.rootvp = NULL;
		}
	    
	    if (error)
		MARK_INT_FAIL(CFS_UMOUNT_STATS);
	    else
		MARK_INT_SAT(CFS_UMOUNT_STATS);
	    
	    return(error);
	}
    }
    
    MARK_INT_FAIL(CFS_UMOUNT_STATS);
    return(EINVAL);
}

/*
 * find root of cfs
 */
int
cfs_root(vfsp, vpp)
	VFS_T *vfsp;
	struct vnode **vpp;
{
    struct cfs_mntinfo *mi = vftomi(vfsp);
    struct ody_mntinfo *op;
    struct vnode *rvp, **result;
    struct inputArgs *inp;
    struct outputArgs *outp;
    int error, size;
    struct proc *p = GLOBAL_PROC;    /* XXX - bnoble */

    ENTRY;
    MARK_ENTRY(CFS_ROOT_STATS);
    result = NULL;
    
    for (op = &mi->mi_vfschain; op; op = op->next) {
	/* Look for a match between vfsp and op->vfsp */
	if (vfsp == op->vfsp) {
	    if ((VTOC(op->rootvp)->c_fid.Volume != 0) ||
		(VTOC(op->rootvp)->c_fid.Vnode != 0) ||
		(VTOC(op->rootvp)->c_fid.Unique != 0))
		{ /* Found valid root. */
		    *vpp = op->rootvp;
		    /* On Mach, this is VN_HOLD.  On NetBSD, VN_LOCK */
		    CFS_ROOT_REF(*vpp);
		    MARK_INT_SAT(CFS_ROOT_STATS);
		    return(0);
		}
	    else	/* Found the vfs, but the vnode not inited yet. */
		break;
	}
    }

    if (op == NULL) {
	/* Huh, didn't find the vfsp. Noone called cfs_mount? Should
           we panic? */
	return (EINVAL);
    }
    
    CFS_ALLOC(inp, struct inputArgs *, sizeof(struct inputArgs));
    outp = (struct outputArgs *) inp;

    /* Didn't find the root, try sending up to a warden for it. */
    INIT_IN(inp, CFS_ROOT, GLOBAL_CRED);  

    size = sizeof(struct inputArgs);
    error = cfscall(vftomi(vfsp), VC_IN_NO_DATA, &size, (char *)inp);
    if (!error)
	error = outp->result;

    if (!error) {
	/*
	 * Save the new rootfid in the cnode, and rehash the cnode into the
	 * cnode hash with the new fid key.
	 */
	cfs_unsave(VTOC(op->rootvp));
	VTOC(op->rootvp)->c_fid = outp->d.cfs_root.VFid;
	cfs_save(VTOC(op->rootvp));

	*vpp = op->rootvp;
	CFS_ROOT_REF(*vpp);
	MARK_INT_SAT(CFS_ROOT_STATS);
	goto exit;
    } else if (error == ENODEV) {
	/* Gross hack here! */
	/*
	 * If Venus fails to respond to the CFS_ROOT call, cfscall returns
	 * ENODEV. Return the uninitialized root vnode to allow vfs
	 * operations such as unmount to continue. Without this hack,
	 * there is no way to do an unmount if Venus dies before a 
	 * successful CFS_ROOT call is done. All vnode operations 
	 * will fail.
	 */
	*vpp = op->rootvp;
	CFS_ROOT_REF(*vpp);
	MARK_INT_FAIL(CFS_ROOT_STATS);
	error = 0;
	goto exit;
    } else {
	CFSDEBUG( CFS_ROOT, myprintf(("error %d in CFS_ROOT\n", error)); );
	MARK_INT_FAIL(CFS_ROOT_STATS);
		
	goto exit;
    }
 exit:
    CFS_FREE(inp, sizeof(struct inputArgs));
    return(error);
}

#ifndef __NetBSD__

/* Locate a type-specific FS based on a name (tome) */
int findTome(tome, data, coveredvp, vpp, p)
    char *tome; 
    char *data; 
    struct vnode *coveredvp; 
    struct vnode **vpp;
    struct proc *p;
{
    struct vnode *rootvp;
    struct cnode *cp;
    char buf[VC_INSIZE(ody_mount_in) + CFS_MAXPATHLEN];
    struct inputArgs *in;
    struct outputArgs *out;
    VFS_T *vfsp;
    int i, size, error;

    ENTRY;
    *vpp = 0;
    
    if (!cfsnc_initialized) {
	cfs_vfsopstats_init();
	cfs_vnodeopstats_init();
	cfsnc_init();
    }
    
    for (i = 0; i < NVCFS; i++) {
	if (cfs_mnttbl[i].mi_name) {
	    if (strcmp(cfs_mnttbl[i].mi_name, tome) == 0) {
		CFSDEBUG( CFS_ROOT, myprintf(("Mounting a tome for %s.%s\n", 
					   tome, data)); )
		/* We need to do the work of both a mount and a root
		 * call here.  Since these funny fs's are never
		 * explicity "mounted", but we want to treat them as
		 * if they are.  */
		    
		/* Setup enough of vfs to allow ODY_MOUNT to go through */
                VFS_ALLOC(vfsp);
		VFS_INIT(vfsp, &cfs_vfsops, (VFS_ANON_T)0);

		/* Initialize the cfs part of the vfs */
		vfsp->VFS_DATA = (VFS_ANON_T)&cfs_mnttbl[i];
		VFS_FSID(vfsp).val[0] = 0;
		VFS_FSID(vfsp).val[1] = makefstype(MOUNT_CFS);

		/* set filesystem block size */
		VFS_BSIZE(vfsp)	= 8192;	    /* XXX -JJK */

		/* Do the work to mount this thing */
		FAKE_MOUNT(vfsp, coveredvp);
		
		/* Get a root vnode for the "volume" (like cfs_root) */
		in = (struct inputArgs *)buf;
		out = (struct outputArgs *)buf;

		INIT_IN(in, ODY_MOUNT, GLOBAL_CRED);
		in->d.ody_mount.name = (char*)VC_INSIZE(ody_mount_in);
		size = strlen(data) + 1;
		strncpy((char*)in + (int)in->d.ody_mount.name, data, size);

		size +=  VC_INSIZE(ody_mount_in);
		error = cfscall(vftomi(vfsp), size, &size, (char *)in);

		if (error || out->result) {
		    CFSDEBUG(CFS_ROOT, myprintf(("error %d in ODY_MOUNT\n",
					      error? error : out->result)); )
		    return( error? error : out->result );
		}

		CFSDEBUG( CFS_ROOT, myprintf(("ODY_MOUNT returns %x.%x.%x\n",
					  out->d.ody_mount.VFid.Volume,
					  out->d.ody_mount.VFid.Vnode,
					  out->d.ody_mount.VFid.Unique)); )
		    
		cp = makecfsnode(&out->d.ody_mount.VFid, vfsp, VDIR);
		rootvp = CTOV(cp);
		rootvp->v_flag |= VROOT;
		*vpp = rootvp;
		VN_HOLD(*vpp);

		/* Add this to the list of mounted things. */
		ADD_VFS_TO_MNTINFO(vftomi(vfsp), vfsp, rootvp);

		return (0);
	    }	
	}
    }
    return ENOENT;	/* Indicate that no matching warden was found */
}

#endif /* __NetBSD__ */

int
cfs_quotactl(vfsp, cmd, uid, arg, p)
    VFS_T *vfsp;
    int cmd;
    uid_t uid;
    caddr_t arg;
    struct proc *p;
{
    ENTRY;
    return (EOPNOTSUPP);
}
    

/*
 * Get file system statistics.
 */
int
cfs_statfs(vfsp, sbp, p)
    register VFS_T *vfsp;
    struct statfs *sbp;
    struct proc *p;
{
    ENTRY;
    MARK_ENTRY(CFS_STATFS_STATS);
    sbp->f_type = 0;
    sbp->f_bsize = 8192;	/* XXX -JJK */
    sbp->f_blocks = -1;
    sbp->f_bfree = -1;
    sbp->f_bavail = -1;
    sbp->f_files = -1;
    sbp->f_ffree = -1;
    bcopy((caddr_t)&(VFS_FSID(vfsp)), (caddr_t)&(sbp->f_fsid),
	  sizeof (fsid_t));
    
    MARK_INT_SAT(CFS_STATFS_STATS);
    return(0);
}

/*
 * Flush any pending I/O.
 */
int
cfs_sync(vfsp, waitfor, cred, p)
    VFS_T *vfsp;
    int    waitfor;
    struct ucred *cred;
    struct proc *p;
{
    ENTRY;
    MARK_ENTRY(CFS_SYNC_STATS);
    MARK_INT_SAT(CFS_SYNC_STATS);
    return(0);
}


int
cfs_vget(vfsp, ino, vpp)
    VFS_T *vfsp;
    ino_t ino;
    struct vnode **vpp;
{
    ENTRY;
    return (EOPNOTSUPP);
}

/* 
 * fhtovp is now what vget used to be in 4.3-derived systems.  For
 * some silly reason, vget is now keyed by a 32 bit ino_t, rather than
 * a type-specific fid.  
 */
int
cfs_fhtovp(vfsp, fhp, nam, vpp, exflagsp, creadanonp)
    register VFS_T *vfsp;    
    struct fid *fhp;
    struct mbuf *nam;
    struct vnode **vpp;
    int *exflagsp;
    struct ucred **creadanonp;
{
    struct cfid *cfid = (struct cfid *)fhp;
    struct cnode *cp = 0;
    struct inputArgs in;
    struct outputArgs *out = (struct outputArgs *)&in; /* Reuse space */
    int error, size;
    struct proc *p = GLOBAL_PROC; /* XXX -mach */

    ENTRY;
    
    MARK_ENTRY(CFS_VGET_STATS);
    /* Check for vget of control object. */
    if (IS_CTL_FID(&cfid->cfid_fid)) {
	*vpp = CFS_CTL_VP;
	VN_HOLD(CFS_CTL_VP);
	MARK_INT_SAT(CFS_VGET_STATS);
	return(0);
    }
    
    INIT_IN(&in, CFS_VGET, GLOBAL_CRED);
    in.d.cfs_vget.VFid = cfid->cfid_fid;
    
    size = VC_INSIZE(cfs_vget_in);
    error = cfscall(vftomi(vfsp), size, &size, (char *)&in);
    
    if (!error) 
	error = out->result;
    
    if (error) {
	CFSDEBUG(CFS_VGET, myprintf(("vget error %d\n",error));)
	    *vpp = (struct vnode *)0;
    }
    else {
	CFSDEBUG(CFS_VGET, 
		 myprintf(("vget: vol %u vno %d uni %d type %d result %d\n",
			out->d.cfs_vget.VFid.Volume,
			out->d.cfs_vget.VFid.Vnode,
			out->d.cfs_vget.VFid.Unique,
			out->d.cfs_vget.vtype,
			out->result)); )
	    
	cp = makecfsnode(&out->d.cfs_vget.VFid, vfsp,
			 out->d.cfs_vget.vtype);
	*vpp = CTOV(cp);
    }
    return(error);
}

int
cfs_vptofh(vnp, fidp)
    struct vnode *vnp;
    struct fid   *fidp;
{
    ENTRY;
    return (EOPNOTSUPP);
}
 
void
cfs_init()
{
    ENTRY;
}

/*
 * To allow for greater ease of use, some vnodes may be orphaned when
 * Venus dies.  Certain operations should still be allowed to go
 * through, but without propagating ophan-ness.  So this function will
 * get a new vnode for the file from the current run of Venus.  */
 
int getNewVnode(vpp)
     struct vnode **vpp;
{
    struct ody_mntinfo *op;
    struct cfid cfid;
    struct cfs_mntinfo *mi = vftomi(VN_VFS(*vpp));
    
    ENTRY;

    for (op = &mi->mi_vfschain; op; op = op->next) {
	/* Look for a match between vfsp and op->vfsp */
	if (VN_VFS(*vpp) == op->vfsp) {
	    break;
	}
    }

    if (op)
	return EINVAL;

    cfid.cfid_len = (short)sizeof(ViceFid);
    cfid.cfid_fid = VTOC(*vpp)->c_fid;	/* Structure assignment. */
    /* XXX ? */

    /* We're guessing that if set, the 1st element on the list is a
     * valid vnode to use. If not, return ENODEV as venus is dead.
     */
    if (mi->mi_vfschain.vfsp == NULL)
	return ENODEV;
    
    return cfs_fhtovp(mi->mi_vfschain.vfsp, (struct fid*)&cfid, NULL, vpp,
		      NULL, NULL);
}
     
