/*
 * cfs_isyscalls.c
 * Created 5/18/95 -- Puneet Kumar
 */

/* This file contains definitions for the mach system calls: 
   iopen, icreate, iread, iwrite, iinc, idec, pioctl. 
   These calls are needed for the AFS/Coda servers to run on NetBSD. */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/filedesc.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ifs/ifs.h>

/* Find the root vnode for a given device.
   In mach, finding the fs corresponding to a device was easy
   because the device number was present in the mount structure.
   In NetBSD, we will have to get the root inode of each
   mounted fs and check if its device corresponds to dev. */
int
getroot(dev, rootvp) 
     int dev;
     struct vnode **rootvp; 
{
    int error; 
    struct mount *mp;
    struct vnode *vp;
    
    *rootvp = NULL; 

    mp = devtomp(dev); 
    
    if (!mp)	/* no mount struct corresponding to device */
	return(ENXIO); 

    if (error = VFS_ROOT(mp, (&vp)))
	return(ENXIO); 
	
    *rootvp = vp;
    return (0);
}

/* 
 * Get the vnode for a given inode.  
 * Common code for iopen, iread/write, iinc/dec.
 */
struct vnode *
igetvnode(dev, i_number)
	dev_t dev;
	ino_t i_number;
{
	register struct inode *ip;
	register struct fs *fs;
	register struct mount *mp;
	struct vnode *vp; 
	int error; 
	
	if (!(mp = devtomp(dev))) 
	    return NULL;

	/* get the vnode/inode.
	   Assume we are on ffs for now.			XXX */ 
	if (error = VFS_VGET(mp, i_number, &vp))
	    return NULL;
	ip = VTOI(vp); 

	if (ip->i_nlink == 0 || (ip->i_mode&IFMT) != IFREG) {
	    vput(vp);
	    return NULL;
	}

	return vp; 
}

struct icreate_args {
    int	dev;
    int	near_inode;	/* ignored */
    int	param1;
    int	param2;
    int	param3;
    int	param4;
};
/* icreate() system call to create a new inode on a given device.
   If successful the call returns the number of the newly created inode (> 0).
   Otherwise it returns -1 */ 
int sys_icreate(p, uap, retval)
     struct proc *p;
     register struct icreate_args *uap;
     int *retval;
{
    struct vnode *rootvp;
    struct vnode *vp;
    struct inode *newip; 
    struct timeval tv;
    struct timespec ts;
    int error = 0;
    
    printf("icreate(dev=%d inode=%d p1=%d p2=%d p3=%d p4=%d)called\n",
	   uap->dev, uap->near_inode, uap->param1, uap->param2,
	   uap->param3, uap->param4);
    
    /*
     * Must be super user
     */
    if (error = suser(p->p_ucred, &p->p_acflag)) {
	*retval = -1;
	return (error);
    }
    

    /* get vnode of root inode for given device */
    if (error = getroot(uap->dev, &rootvp)) {
	*retval = -1; 
	return(error);
    }
    
    /* allocate the inode on disk and get a vnode pointer */
    if (error = VOP_VALLOC(rootvp, IFREG, p->p_ucred, &vp)) {
	vput(rootvp);
	*retval = -1; 
	return(error); 
    }

    vp->v_type = VREG; /* Rest init'd in getnewvnode().*/
    
    /* initialize the inode */
    newip = VTOI(vp); 
    newip->i_flag |= IN_ACCESS | IN_CHANGE | IN_UPDATE;

    newip->i_nlink = 1;
    newip->i_uid = 0;
    newip->i_gid = -2;
    newip->i_mode = IFREG;
/*  newip->i_vicemagic = VICEMAGIC; Can't use generation number any more in NetBSD */
    newip->i_vicep1 = uap->param1;
    newip->i_vicep2 = uap->param2;
    newip->i_vicep3 = uap->param3;
    newip->i_vicep4 = uap->param4;
    
    /*
     * Make sure inode goes to disk.
     */ 
    tv = time;
    ts.tv_sec = tv.tv_sec;
    ts.tv_nsec = tv.tv_usec * 1000;
    error = VOP_UPDATE(vp, &ts, &ts, 1); 

    /* ignore writing out the parent vnode (rootvp?) XXX */
    
    *retval = error ? -1 : newip->i_number; 

    vput(rootvp);
    vput(vp); 
    return(error);
}


struct iopen_args {
    int	dev;
    int	inode;
    int	usermode;
};
/*
 * iopen system call -- open an inode for reading/writing
 * Restricted to super user. Any IFREG files.
 * On success, return the filedescriptor number (> 0)
 * Otherwise return -1 
 */
int
sys_iopen(p, uap, retval)
    struct proc *p;
    register struct iopen_args *uap;
    int *retval;
{
    register struct inode *ip;
    register struct file *fp;
    struct file *nfp;
    int indx;
    struct vnode *vp;
    int error; 
    extern struct fileops vnops;
    
    printf("iopen(dev=%d inode=%d usermode=%o) called\n",
	   uap->dev, uap->inode, uap->usermode);
    /*
     * Must be super user
     */
    if (error = suser(p->p_ucred, &p->p_acflag)) {
	*retval = -1;
	return (error);
    }


    /*
     * Get the inode and a vnode for the inode,dev pair
     */
    if (!(vp = igetvnode((dev_t)uap->dev, (ino_t)uap->inode))) {
	printf("Error in igetvnode\n"); 
	*retval = -1;
	return(ENOENT); 
    }
    ip = VTOI(vp); 

    /*
     * Check the inode is a vice inode 
     * Note that we couldn't do the vicemagic checks done in mach
     * because we are running out of fields in the dinode structure
     * and vicemagic is no longer used. 
     */ 
    if (ip->i_gid != (gid_t)-2) {
	vput(vp);
	*retval = -1;
	return (EPERM);
    }

    if ((uap->usermode&O_EXCL) && ip->i_count != 1) {
	vput(vp);
	*retval = -1;
	return EBUSY;
    }

    /* allocate and set up a file structure needed to do i/o */ 
    if (error = falloc(p, &nfp, &indx)) {
	vput(vp);
	*retval = -1; 
	return (error);
    }
    fp = nfp;    
    if (fp == NULL) {
	vput(vp);
	*retval = -1;
	return ENOENT;
    }
    /* If we've made it this far, the open will succeed */
    /* iunlock(ip); */ 
    fp->f_flag = FFLAGS(uap->usermode)&FMASK;
    /* If we're opening for writing, we must tell the vnode that */
    if (fp->f_flag & FWRITE) {
	vp->v_writecount++;
    }
    fp->f_type = DTYPE_VNODE;
    fp->f_ops = &vnops;
    fp->f_data = (caddr_t)vp;

    VOP_UNLOCK(vp); /* not sure about this - XXX pkumar 5/95 */
    *retval = indx;
    return 0; 
}

int 
readwritevnode(p, rw, vp, base, len, offset, segflg, aresid)
     struct proc *p; 
     enum uio_rw rw;
     struct vnode *vp;
     caddr_t base;
     int len;
     off_t offset;
     int segflg; 
     int *aresid;
{
    struct uio auio;
    struct iovec aiov;
    int error;
    long cnt; 
    struct file *nfp;
    register struct file *fp; 
    int indx; 

    return(EOPNOTSUPP); 
#ifdef 0    
    /* From Mach.  We don't understand this yet.  Figure it out! */

    /* allocate a file struct to do the i/o */ 
    if (error = falloc(p, &nfp, &indx)) {
	*retval = error; 
 	return (error);
    }
    fp = nfp;    
    if (fp == NULL) {
	*retval = ENOENT;
	return (-1); 
    }

    
    /* initialize uio structure */ 
    aiov.iov_base = base;
    aiov.iov_len = len;

    auio.uio_iov = &aiov;
    auio.uio_iovcnt = 1;
    auio.uio_offset = offset;
    auio.uio_resid = len;
    auio.uio_segflg = segflg; 
    auio.uio_rw = rw;
    auio.uio_procp = p;



    error = rwip(vp, &auio, rw);
    if (aresid)
	*aresid = auio.uio_resid;
    else
	if (auio.uio_resid)
	    if (error == 0)
		error = EIO;

    /* need to free the file structure that was allocated */
    look at close, closef() in kern_descrip.c; 
    return (error);

#ifdef NETBSD_READCALL
	if (((u_int)uap->fd) >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[uap->fd]) == NULL ||
	    (fp->f_flag & FREAD) == 0)
		return (EBADF);

	if (auio.uio_resid < 0)
		return EINVAL;

	cnt = uap->nbyte;
	if (error = (*fp->f_ops->fo_read)(fp, &auio, fp->f_cred))
		if (auio.uio_resid != cnt && (error == ERESTART ||
		    error == EINTR || error == EWOULDBLOCK))
			error = 0;
	cnt -= auio.uio_resid;
	*retval = cnt;
	return (error);

#endif NETBSD_READCALL
#endif 0
}

struct ireadwrite_args {
    int			dev;
    int			inode;
    long		inode_p1;
    unsigned int 	offset;
    char		*cbuf;
    unsigned int 	count;
};
/*
 * Support for iread/iwrite system calls.
 * Restricted to super user.
 * Only inodes with owner and group == -1.  (BIG LIE -bnoble XXX)
 * NB:  VICEMAGIC inodes default to this owner and group.
 */
int 
ireadwrite(p, uap, retval, rw)
     struct proc *p;
     struct ireadwrite_args *uap; 
     int *retval; 
     enum uio_rw	rw;
{
    register struct inode *ip;
    struct vnode *vp; 
    unsigned int resid;
    daddr_t	db[NDADDR], ib[NIADDR];
    int size, error; 

    /*
     * Must be super user
     */
    if (error = suser(p->p_ucred, &p->p_acflag)) {
	*retval = -1;
	return (error);
    }

    /*
     * Get the inode and a vnode for the inode,dev pair
     */
    if (!(vp = igetvnode((dev_t)uap->dev, (ino_t)uap->inode))) {
	*retval = ENOENT;
	return(-1); 
    }
    ip = VTOI(vp); 

    /*
     * Check the inode is a vice inode 
     * Note that we couldn't do the vicemagic checks done in mach
     * because we are running out of fields in the dinode structure
     * and vicemagic is no longer used. 
     */ 
    if (ip->i_gid != (gid_t)-2) {
	vput(vp);
	*retval = EPERM;
	return (-1);
    }
    
    if (ip->i_vicep1 != uap->inode_p1) {
	vput(vp);
	*retval = ENXIO;
	return (-1);
    }

    if (error = readwritevnode(p, rw, vp, (caddr_t) uap->cbuf, uap->count, 
			       uap->offset, UIO_USERSPACE, &resid)) {
	vput(vp);
	*retval = error;
	return (-1);
    }

#ifdef 0
    /* From Mach.  We don't understand it yet, so we aren't running with
       it.  Perhaps we should lobotomize i{read/write}? Slow...*/

    bcopy((caddr_t)ip->i_db, (caddr_t)db, sizeof db);
    bcopy((caddr_t)ip->i_ib, (caddr_t)ib, sizeof ib);
    size = ip->i_size;
    resid = 0;
    u.u_error = rdwri(rw, ip, (caddr_t) uap->cbuf, uap->count, uap->offset, 0, &resid);
    u.u_r.r_val1 = uap->count - resid;
    if (size == ip->i_size
	&& bcmp((caddr_t)ip->i_db, (caddr_t)db, sizeof db) == 0
	&& bcmp((caddr_t)ip->i_ib, (caddr_t)ib, sizeof ib) == 0) {
	/* Don t write out the inode if it hasn t really changed.
	   We don t care about inode dates in file server files */
	ip->i_flag &= ~(IUPD|IACC|ICHG);
    }
#endif 0

    vput(vp);
    *retval = 0; 
    return(0); 
}

/*
 * In Mach the iread and iwrite system calls call the inode operation
 * rdwri() directly.  There is no need for a file struct pointer. 
 * In NetBSD, all the i/o routines expect a vnode pointer as well as
 * file struct.  Therefore, we are going to falloc() and ffree() a
 * file struct here.  This might be wasteful, but it is the fastest
 * way to get these routines ported. -- pkumar 5/95 
 */
int
sys_iread(p, uap, retval)
    struct proc *p;
    register struct ireadwrite_args *uap;
    int *retval;
{
    printf("iread(dev=%d inode=%d inode_p1=%d offset=%u cbuf=%x count=%u) called\n",
	   uap->dev, uap->inode, uap->inode_p1, uap->offset,
	   uap->cbuf, uap->count);
    
    return(ireadwrite(p, uap, retval, UIO_READ));
}

int
sys_iwrite(p, uap, retval)
    struct proc *p;
    register struct ireadwrite_args *uap;
    int *retval;
{
    printf("iwrite(dev=%d inode=%d inode_p1=%d offset=%u cbuf=%x count=%u) called\n", 
	   uap->dev, uap->inode, uap->inode_p1, uap->offset,
	   uap->cbuf, uap->count);
    return(ireadwrite(p, uap, retval, UIO_WRITE)); 
}

/*
 * Support for iinc() and idec() system calls--increment or decrement
 * count on inode.
 * Restricted to super user.
 * Return 0 on success; non-zero otherwise. 
 */
struct iincdec_args {
    int		dev;
    int		inode;
    long	inode_p1;
};
int 
iincdec(p, uap, retval, amount)
     struct proc *p;
     register struct iincdec_args *uap;
     int *retval;
     int amount;
{

    register struct vnode *vp; 
    register struct inode *ip;
    struct timeval tv;
    struct timespec ts;
    int error; 
    
    /*
     * Must be super user
     */
    if (error = suser(p->p_ucred, &p->p_acflag)) {
	*retval = error; 
	return (error);
    }
    
    /*
     * Get the inode and a vnode for the inode,dev pair
     */
    if (!(vp = igetvnode((dev_t)uap->dev, (ino_t)uap->inode))) {
	*retval = ENOENT; 
	return(ENOENT); 
    }
    ip = VTOI(vp); 


    /*
     * Check the inode is a vice inode. See comment above. 
     */ 
    if (ip->i_gid != (gid_t)-2) {
	vput(vp);
	*retval = EPERM;
	return (EPERM);
    }
    
    if (ip->i_vicep1 != uap->inode_p1) {
	vput(vp);
	*retval = ENXIO;
	return ENXIO;
    }

    ip->i_nlink += amount;
    
    if (ip->i_nlink == 0)
	/* ip->i_vicemagic = 0;	Not possible to use VICEMAGIC in NetBSD*/
	ip->i_gid = 0;

    /* write out the inode */ 
    ip->i_flag |= IN_CHANGE;
    tv = time; 
    ts.tv_sec = tv.tv_sec;
    ts.tv_nsec = tv.tv_usec * 1000;
    if (error = VOP_UPDATE(vp, &ts, &ts, 1)) {
	*retval = EIO;
	return(EIO); 
    }

    /* unlock the vnode and write out the inode */ 
    vput(vp); 
    *retval = 0;
    return(0);
}




int
sys_iinc(p, uap, retval)
    struct proc *p;
    register struct iincdec_args *uap;
    int *retval;
{
    printf("iinc(dev=%d inode=%d inode_p1=%d) called\n",
	   uap->dev, uap->inode, uap->inode_p1);
    return(iincdec(p, uap, retval, 1)); 
}
int
sys_idec(p, uap, retval)
    struct proc *p;
    register struct iincdec_args *uap;
    int *retval;
{
    printf("idec(dev=%d inode=%d inode_p1=%d) called\n",
	   uap->dev, uap->inode, uap->inode_p1);
    return(iincdec(p, uap, retval, -1)); 
}

/* Grab from Mach vice-only pioctl. */
struct pioctl_args {
    char	*path;
    int		com;
    caddr_t	comarg;
    int		follow;
};
int
sys_pioctl(p, uap, retval)
    struct proc *p;
    register struct pioctl_args *uap;
    int *retval;
{
    printf("pioctl(path=%s com=%d comarg=%s follow=%d) called\n",
	   uap->path, uap->com, uap->comarg, uap->follow);
    *retval = 0;
    return(0);
}

/*
int setpag(p, uap, retval)
    struct proc *p;
    void *uap; 
    int *retval; 
{
    Figure out what to do with this, then, figure out how to fix
	the name clash with AFS, then do something .XXX -bnoble

  printf("setpag() called \n"); 
  *retval = EOPNOTSUPP; 
  return(EOPNOTSUPP); 
}
*/


