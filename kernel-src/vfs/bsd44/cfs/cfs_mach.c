/* 
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */

/*
 * This code was written for the Coda file system at Carnegie Mellon
 * University.  Contributers include David Steere, James Kistler, and
 * M. Satyanarayanan.  */

/*
 * HISTORY
 * $Log:	cfs_mach.c,v $
 * Revision 1.3.14.1  97/11/12  12:09:35  rvb
 * reorg pass1
 * 
 * Revision 1.3  97/01/13  17:11:02  bnoble
 * Coda statfs needs to return something other than -1 for blocks avail. and
 * files available for wabi (and other windowsish) programs to install
 * there correctly.
 * 
 * Revision 1.2  1996/01/02 16:56:49  bnoble
 * Added support for Coda MiniCache and raw inode calls (final commit)
 *
 * Revision 1.1.2.1  1995/12/20 01:57:14  bnoble
 * Added CFS-specific files
 *
 */

/* Mach-specific routines for the cfs code */
#ifdef MACH

#include <cfs/cfs.h>
#include <cfs/cfs_vnodeops.h>

/* Forward declarations for the vfs operations */
int cfs_mount_mach   __P((VFS_T *, char *, caddr_t));
int cfs_unmount_mach __P((VFS_T *));
int cfs_statfs_mach  __P((VFS_T *, struct statfs *));
int cfs_sync_mach    __P((VFS_T *));
int cfs_vget_mach    __P((VFS_T *, struct vnode **, struct fid *));

/* Definition of the vfs operation vector */

struct vfsops cfs_vfsops = {
	cfs_mount_mach,
	cfs_unmount_mach,
	cfs_root,            /* cfs_root is of correct type */
	cfs_statfs_mach,
	cfs_sync_mach,
	cfs_vget_mach
};

/* Definition of the vnode operation vector */

struct vnodeops cfs_vnodeops = {
	cfs_mk_open,
	cfs_mk_close,
	cfs_mk_rdwr,
	cfs_mk_ioctl,
	cfs_mk_select,
	cfs_mk_getattr,
	cfs_mk_setattr,
	cfs_mk_access,
	cfs_mk_lookup,
	cfs_mk_create,
	cfs_mk_remove,
	cfs_mk_link,
	cfs_mk_rename,
	cfs_mk_mkdir,
	cfs_mk_rmdir,
	cfs_mk_readdir,
	cfs_mk_symlink,
	cfs_mk_readlink,
	cfs_mk_fsync,
	cfs_mk_inactive,
	cfs_mk_bmap,
	cfs_mk_strategy,
	cfs_mk_bread,
	cfs_mk_brelse,
	cfs_mk_lockctl,
	cfs_mk_fid,
	cfs_mk_page_read,
	cfs_mk_page_write,
	cfs_mk_readdir,		/* read1dir */
	cfs_mk_freefid
};

/************************ Pseudo-device wrappers */

int
vcopen(dev)
    dev_t dev;
{
    return vc_nb_open(dev, 0, 0, GLOBAL_PROC);
}

int
vcclose(dev)
    dev_t dev;
{
    return vc_nb_close(dev, 0, 0, GLOBAL_PROC);
}

int 
vcread(dev, uiop)
    dev_t       dev;
    struct uio *uiop;
{
    return vc_nb_read(dev,uiop,0);
}

int
vcwrite(dev, uiop)
    dev_t       dev;
    struct uio *uiop;
{
    return vc_nb_write(dev,uiop,0);
}

int
vcioctl(dev, cmd, addr, flag)
    dev_t    dev;
    int      cmd;
    caddr_t  addr;
    int      flag;
{
    return vc_nb_ioctl(dev,cmd,addr,flag,GLOBAL_PROC);
}

int
vcselect(dev, flag)
    dev_t  dev;
    int    flag;
{
    return vc_nb_select(dev,flag,GLOBAL_PROC);
}

/************************ VFS operation wrappers */

int
cfs_mount_mach(vfsp, path, data)
    VFS_T *vfsp;
    char *path;
    caddr_t data;
{
    return cfs_mount(vfsp, path, data, NULL, GLOBAL_PROC);
}

int
cfs_unmount_mach(vfsp)
    VFS_T *vfsp;
{
    return cfs_unmount(vfsp, 0, GLOBAL_PROC);
}

int
cfs_statfs_mach(vfsp, sbp)
    register VFS_T *vfsp;
    struct statfs *sbp;
{
    sbp->f_type = 0;
    sbp->f_bsize = 8192;	/* XXX -JJK */
    sbp->f_blocks = -1;
    sbp->f_bfree = -1;
    sbp->f_bavail = -1;
    sbp->f_files = -1;
    sbp->f_ffree = -1;
    bcopy((caddr_t)&(VFS_FSID(vfsp)), (caddr_t)&(sbp->f_fsid),
	  sizeof (fsid_t));
}

int
cfs_sync_mach(vfsp)
    VFS_T *vfsp;
{
    return cfs_sync(vfsp,0,GLOBAL_CRED,GLOBAL_PROC);
}

int
cfs_vget_mach(vfsp, vpp, fidp)
    VFS_T         *vfsp;
    struct vnode **vpp;
    struct fid    *fidp;
{
    return (cfs_fhtovp(vfsp, fidp, NULL, vpp, NULL, NULL));
}

/* Vnode operation wrappers */

int
cfs_mk_open(vpp, flag, cred)
    register struct vnode **vpp;
    int flag;
    struct ucred *cred;
{
    return cfs_open(vpp, flag, cred, GLOBAL_PROC);
}

/*ARGSUSED*/
/*
 * Close the cache file used for I/O and notify Venus.
 */
int
cfs_mk_close(vp, flag, cred)
    struct vnode *vp;
    int flag;
    struct ucred *cred;
{ 
    return cfs_close(vp, flag, cred, GLOBAL_PROC);
}

int
cfs_mk_rdwr(vp, uiop, rw, ioflag, cred)
    struct vnode *vp;
    struct uio *uiop;
    enum uio_rw rw;
    int ioflag;
    struct ucred *cred;
{ 
    return cfs_rdwr(vp, uiop, rw, ioflag, cred, GLOBAL_PROC);
}

/*ARGSUSED*/
int
cfs_mk_ioctl(vp, com, data, flag, cred)
    struct vnode *vp;
    int com;
    caddr_t data;
    int flag;
    struct ucred *cred;
    struct proc  *p;
{ 
    return cfs_ioctl(vp, com, data, flag, cred, GLOBAL_PROC);
}

/*ARGSUSED*/
int
cfs_mk_select(vp, which, cred)
    struct vnode *vp;
    int which;
    struct ucred *cred;
{
    return cfs_select(vp, which, cred, GLOBAL_PROC);
}

int
cfs_mk_getattr(vp, vap, cred)
    struct vnode *vp;
    struct vattr *vap;
    struct ucred *cred;
{
    return cfs_getattr(vp, vap, cred, GLOBAL_PROC);
}

int
cfs_mk_setattr(vp, vap, cred)
    register struct vnode *vp;
    register struct vattr *vap;
    struct ucred *cred;
{ 
    return cfs_setattr(vp, vap, cred, GLOBAL_PROC);
}

int
cfs_mk_access(vp, mode, cred)
    struct vnode *vp;
    int mode;
    struct ucred *cred;
{ 
    return cfs_access(vp, mode, cred, GLOBAL_PROC);
}

int
cfs_mk_readlink(vp, uiop, cred)
    struct vnode *vp;
    struct uio *uiop;
    struct ucred *cred;
{ 
    return cfs_readlink(vp, uiop, cred, GLOBAL_PROC);
}

/*ARGSUSED*/
int
cfs_mk_fsync(vp, cred)
    struct vnode *vp;
    struct ucred *cred;
{ 
    return cfs_fsync(vp, cred, GLOBAL_PROC);
}

/*ARGSUSED*/
int
cfs_mk_inactive(vp, cred)
    struct vnode *vp;
    struct ucred *cred;
{ 
    return cfs_inactive(vp, cred, GLOBAL_PROC);
}

/*
 * Remote file system operations having to do with directory manipulation.
 */

int
cfs_mk_lookup(dvp, nm, vpp, cred)
    struct vnode *dvp;
    char *nm;
    struct vnode **vpp;
    struct ucred *cred;
    sruct proc *p;
{ 
    return cfs_lookup(dvp, nm, vpp, cred, GLOBAL_PROC);
}

/*ARGSUSED*/
int
cfs_mk_create(dvp, nm, va, exclusive, mode, vpp, cred)
    struct vnode *dvp;
    char *nm;
    struct vattr *va;
    enum vcexcl exclusive;
    int mode;
    struct vnode **vpp;
    struct ucred *cred;
{
    return cfs_create(dvp, nm, va, exclusive, mode, vpp, cred, GLOBAL_PROC);
}

int
cfs_mk_remove(dvp, nm, cred)
    struct vnode *dvp;
    char *nm;
    struct ucred *cred;
{ 
    return cfs_remove(dvp, nm, cred, GLOBAL_PROC);
}


int
cfs_mk_link(vp, tdvp, tnm, cred)
    struct vnode *vp;
    struct vnode *tdvp;
    char *tnm;
    struct ucred *cred;
{
    return cfs_link(vp, tdvp, tnm, cred, GLOBAL_PROC);
}

int
cfs_mk_rename(odvp, onm, ndvp, nnm, cred)
    struct vnode *odvp;
    char *onm;
    struct vnode *ndvp;
    char *nnm;
    struct ucred *cred;
{
    return cfs_rename(odvp, onm, ndvp, nnm, cred, GLOBAL_PROC);
}

int
cfs_mk_mkdir(dvp, nm, va, vpp, cred)
    struct vnode *dvp;
    char *nm;
    register struct vattr *va;
    struct vnode **vpp;
    struct ucred *cred;
{ 
    return cfs_mkdir(dvp, nm, va, vpp, cred, GLOBAL_PROC);
}

int
cfs_mk_rmdir(dvp, nm, cred)
    struct vnode *dvp;
    char *nm;
    struct ucred *cred;
{ 
    return cfs_rmdir(dvp, nm, cred, GLOBAL_PROC);
}

int
cfs_mk_symlink(tdvp, tnm, tva, lnm, cred)
    struct vnode *tdvp;
    char *tnm;
    struct vattr *tva;
    char *lnm;
    struct ucred *cred;
{
    return cfs_symlink(tdvp, tnm, tva, lnm, cred, GLOBAL_PROC);
}

/*
 * Read directory entries.
 */
int
cfs_mk_readdir(vp, uiop, cred)
    struct vnode *vp;
    register struct uio *uiop;
    struct ucred *cred;
{ 
    int fake_eofflag;
    u_long fake_cookies;
    int    ncookies = 0;

    return cfs_readdir(vp, uiop, cred, &fake_eofflag, &fake_cookies,
		       ncookies, GLOBAL_PROC);
}

/*
 * Convert from file system blocks to device blocks
 */
int
cfs_mk_bmap(vp, bn, vpp, bnp)
    struct vnode *vp;	/* file's vnode */
    daddr_t bn;		/* fs block number */
    struct vnode **vpp;	/* RETURN vp of device */
    daddr_t *bnp;		/* RETURN device block number */
{ 
    return cfs_bmap(vp, bn, vpp, bnp, GLOBAL_PROC);
}

int
cfs_mk_strategy(bp)
    register struct buf *bp;
{ 
    return cfs_strategy(bp, GLOBAL_PROC);
}


/* How one looks up a vnode given a device/inode pair: */

int
cfs_grab_vnode(dev, ino, vpp)
	 dev_t dev; ino_t ino; struct vnode **vpp;
{
    /* This is like VFS_VGET() or igetinode()! */
    struct fs *fs = igetfs(dev);
    if (fs == NULL) {
	printf("cfs_grab_vnode: igetfs(%d) returns NULL\n", dev);
	return(ENXIO);
    }
    *vpp = iget(dev, fs, ino);
    if (*vpp == NULL) {
	printf("cfs_grab_vnode: iget(%d, %x, %d) returns NULL\n", 
	       dev, fs, ino);
	return(ENOENT);
    }
    return(0);
}

/* How to print out the attributes of a vnode. */

void
print_vattr( attr )
	struct vattr *attr;
{
	printf("getattr: mode %d uid %d gid %d fsid %d rdev %d\n",
		(int)attr->va_mode, (int)attr->va_uid,
		(int)attr->va_gid, (int)attr->va_fsid, (int)attr->va_rdev);

	printf("	nodeid %d nlink %d size %d blocksize %d blocks %d\n",
		(int)attr->va_nodeid, (int)attr->va_nlink, (int)attr->va_size,
		(int)attr->va_blocksize,(int)attr->va_blocks);

	printf("	atime sec %d usec %d",
		(int)attr->va_atime.tv_sec, (int)attr->va_atime.tv_usec);
	printf("	mtime sec %d usec %d",
		(int)attr->va_mtime.tv_sec, (int)attr->va_mtime.tv_usec);
	printf("	ctime sec %d usec %d\n",
		(int)attr->va_ctime.tv_sec, (int)attr->va_ctime.tv_usec);
}

/* How to print a ucred */
print_cred(cred)
	struct ucred *cred;
{

	int i;

	printf("ref %d uid %d ruid %d gid %d rgid %d pag %d\n",
		cred->id_ref,cred->id_uid,cred->id_ruid,
		cred->id_gid,cred->id_rgid,cred->id_pag);

	for (i=0; i < 16; i++)
		printf("%d groups %d ",i,cred->id_groups[i]);
	printf("\n");

}


#endif /* MACH */



