#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/ioctl.h>

#include <cfs/cfs.h>
#include <cfs/cfsk.h>
#include <cfs/pioctl.h>

enum vcexcl	{ NONEXCL, EXCL};		/* (non)excl create (create) */

/* NOTES:
	cfsk.h should not be here!!!
	(then proc.h is not necessary ?? maybe)

	The biggy, of course, is that I should use codacreds, but I don't
	want to break venus yet.
 */

#define DECL_NO_IN(name) \
    struct cfs_in_hdr *inp; \
    struct name ## _out *outp; \
    union name * name ## _buf; \
    int name ## _size = sizeof (union name); \
    int Isize = sizeof (struct cfs_in_hdr); \
    int Osize = sizeof (struct name ## _out); \
    int error

#define DECL(name) \
    struct name ## _in *inp; \
    struct name ## _out *outp; \
    union name * name ## _buf; \
    int name ## _size = sizeof (union name); \
    int Isize = sizeof (struct name ## _in); \
    int Osize = sizeof (struct name ## _out); \
    int error

#define DECL_NO_OUT(name) \
    struct name ## _in *inp; \
    struct cfs_out_hdr *outp; \
    union name * name ## _buf; \
    int name ## _size = sizeof (union name); \
    int Isize = sizeof (struct name ## _in); \
    int Osize = sizeof (struct cfs_out_hdr); \
    int error

#define ALLOC(name) \
    CFS_ALLOC(name ## _buf, union name *, name ## _size); \
    inp = &name ## _buf->in; \
    outp = &name ## _buf->out

#define STRCPY(struc, name, len) \
    strncpy((char *)inp + (int)inp->struc, name, len); \
    ((char*)inp + (int)inp->struc)[len++] = 0; \
    Isize += len

venus_root(void *mdp,
	struct ucred *cred, struct proc *p,
/*out*/	ViceFid *VFid)
{
    DECL_NO_IN(cfs_root);		/* sets Isize & Osize */
    ALLOC(cfs_root);			/* sets inp & outp */

    /* send the open to venus. */
    INIT_IN(inp, CFS_ROOT, cred);  

    error = cfscall(mdp, Isize, &Osize, (char *)inp);
    if (!error)
    	error = outp->oh.result;
    if (!error)
	*VFid = outp->VFid;

    CFS_FREE(cfs_root_buf, cfs_root_size);
    return error;
}

venus_open(void *mdp, ViceFid *fid, int flag,
	struct ucred *cred, struct proc *p,
/*out*/	dev_t *dev, ino_t *inode)
{
    DECL(cfs_open);			/* sets Isize & Osize */
    ALLOC(cfs_open);			/* sets inp & outp */

    /* send the open to venus. */
    INIT_IN(&inp->ih, CFS_OPEN, cred);
    inp->VFid = *fid;
    inp->flags = flag;

    error = cfscall(mdp, Isize, &Osize, (char *)inp);

    if (!error)
    	error = outp->oh.result;
    if (!error) {
	*dev =  outp->dev;
	*inode = outp->inode;
    }

    CFS_FREE(cfs_open_buf, cfs_open_size);
    return error;
}

venus_close(void *mdp, ViceFid *fid, int flag,
	struct ucred *cred, struct proc *p)
{
    DECL_NO_OUT(cfs_close);		/* sets Isize & Osize */
    ALLOC(cfs_close);			/* sets inp & outp */

    INIT_IN(&inp->ih, CFS_CLOSE, cred);
    inp->VFid = *fid;
    inp->flags = flag;

    error = cfscall(mdp, Isize, &Osize, (char *)inp);
    if (!error) 
	error = outp->result;

    CFS_FREE(cfs_close_buf, cfs_close_size);
    return error;
}

/*
 * these two calls will not exist!!!  the container file is read/written
 * directly.
 */
venus_read()
{
}

venus_write()
{
}

/*
 * this is a bit sad too.  the ioctl's are for the control file, not for
 * normal files.
 */
venus_ioctl(void *mdp, ViceFid *fid,
	int com, int flag, caddr_t data,
	struct ucred *cred, struct proc *p)
{
    DECL(cfs_ioctl);			/* sets Isize & Osize */
    register struct a {
	char *path;
	struct ViceIoctl vidata;
	int follow;
    } *iap = (struct a *)data;
    int tmp;

    cfs_ioctl_size = VC_MAXMSGSIZE;
    ALLOC(cfs_ioctl);			/* sets inp & outp */

    INIT_IN(&inp->ih, CFS_IOCTL, cred);
    inp->VFid = *fid;

    /* command was mutated by increasing its size field to reflect the  
     * path and follow args. we need to subtract that out before sending
     * the command to venus.
     */
    inp->cmd = (com & ~(IOCPARM_MASK << 16));
    tmp = ((com >> 16) & IOCPARM_MASK) - sizeof (char *) - sizeof (int);
    inp->cmd |= (tmp & IOCPARM_MASK) <<	16;

    inp->rwflag = flag;
    inp->len = iap->vidata.in_size;
    inp->data = (char *)(sizeof (struct cfs_ioctl_in));

    error = copyin(iap->vidata.in, (char*)inp + (int)inp->data, 
		   iap->vidata.in_size);
    if (error) {
	CFS_FREE(cfs_ioctl_buf, cfs_ioctl_size);
	return(error);
    }

    Osize = VC_MAXMSGSIZE;
    error = cfscall(mdp, Isize + iap->vidata.in_size, &Osize, (char *)inp);

    if (!error)
    	error = outp->oh.result;

	/* copy out the out buffer. */
    if (!error) {
	if (outp->len > iap->vidata.out_size) {
	    error = EINVAL;
	} else {
	    error = copyout((char *)outp + (int)outp->data, 
			    iap->vidata.out, iap->vidata.out_size);
	}
    }

    CFS_FREE(cfs_ioctl_buf, cfs_ioctl_size);
    return error;
}

venus_getattr(void *mdp, ViceFid *fid,
	struct ucred *cred, struct proc *p,
/*out*/	struct vattr *vap)
{
    DECL(cfs_getattr);			/* sets Isize & Osize */
    ALLOC(cfs_getattr);			/* sets inp & outp */

    /* send the open to venus. */
    INIT_IN(&inp->ih, CFS_GETATTR, cred);
    inp->VFid = *fid;

    error = cfscall(mdp, Isize, &Osize, (char *)inp);

    if (!error)
    	error = outp->oh.result;
    if (!error)
	*vap = outp->attr;

    CFS_FREE(cfs_getattr_buf, cfs_getattr_size);
    return error;
}

venus_setattr(void *mdp, ViceFid *fid, struct vattr *vap,
	struct ucred *cred, struct proc *p)
{
    DECL_NO_OUT(cfs_setattr);		/* sets Isize & Osize */
    ALLOC(cfs_setattr);			/* sets inp & outp */

    /* send the open to venus. */
    INIT_IN(&inp->ih, CFS_SETATTR, cred);
    inp->VFid = *fid;
    inp->attr = *vap;

    error = cfscall(mdp, Isize, &Osize, (char *)inp);
    if (!error) 
	error = outp->result;

    CFS_FREE(cfs_setattr_buf, cfs_setattr_size);
    return error;
}

venus_access(void *mdp, ViceFid *fid, int mode,
	struct ucred *cred, struct proc *p)
{
    DECL_NO_OUT(cfs_access);		/* sets Isize & Osize */
    ALLOC(cfs_access);			/* sets inp & outp */

    /* send the open to venus. */
    INIT_IN(&inp->ih, CFS_ACCESS, cred);
    inp->VFid = *fid;
    inp->flags = mode;

    error = cfscall(mdp, Isize, &Osize, (char *)inp);
    if (!error) 
	error = outp->result;

    CFS_FREE(cfs_access_buf, cfs_access_size);
    return error;
}

venus_readlink(void *mdp, ViceFid *fid,
	struct ucred *cred, struct proc *p,
/*out*/	char **str, int *len)
{
    DECL(cfs_readlink);			/* sets Isize & Osize */
    cfs_readlink_size += CFS_MAXPATHLEN;
    ALLOC(cfs_readlink);		/* sets inp & outp */

    /* send the open to venus. */
    INIT_IN(&inp->ih, CFS_READLINK, cred);
    inp->VFid = *fid;

    Osize += CFS_MAXPATHLEN;
    error = cfscall(mdp, Isize, &Osize, (char *)inp);

    if (!error)
    	error = outp->oh.result;
    if (!error) {
	    CFS_ALLOC(*str, char *, outp->count);
	    *len = outp->count;
	    bcopy((char *)outp + (int)outp->data, *str, *len);
    }

    CFS_FREE(cfs_readlink_buf, cfs_readlink_size);
    return error;
}

venus_fsync(void *mdp, ViceFid *fid,
	struct ucred *cred, struct proc *p)
{
    DECL_NO_OUT(cfs_fsync);		/* sets Isize & Osize */
    ALLOC(cfs_fsync);			/* sets inp & outp */

    /* send the open to venus. */
    INIT_IN(&inp->ih, CFS_FSYNC, cred);
    inp->VFid = *fid;

    error = cfscall(mdp, Isize, &Osize, (char *)inp);
    if (!error) 
	error = outp->result;

    CFS_FREE(cfs_fsync_buf, cfs_fsync_size);
    return error;
}

venus_lookup(void *mdp, ViceFid *fid,
    	char *nm, int len,
	struct ucred *cred, struct proc *p,
/*out*/	ViceFid *VFid, int *vtype)
{
    DECL(cfs_lookup);			/* sets Isize & Osize */
    cfs_lookup_size += len + 1;
    ALLOC(cfs_lookup);			/* sets inp & outp */

    /* send the open to venus. */
    INIT_IN(&inp->ih, CFS_LOOKUP, cred);
    inp->VFid = *fid;

    inp->name = (char *)Isize;
    STRCPY(name, nm, len);		/* increments Isize */

    error = cfscall(mdp, Isize, &Osize, (char *)inp);
    if (!error)
    	error = outp->oh.result;
    if (!error) {
	*VFid = outp->VFid;
	*vtype = outp->vtype;
    }

    CFS_FREE(cfs_lookup_buf, cfs_lookup_size);
    return error;
}

venus_create(void *mdp, ViceFid *fid,
    	char *nm, int len, enum vcexcl exclusive, int mode, struct vattr *va,
	struct ucred *cred, struct proc *p,
/*out*/	ViceFid *VFid, struct vattr *attr)
{
    DECL(cfs_create);			/* sets Isize & Osize */
    cfs_create_size += len + 1;
    ALLOC(cfs_create);			/* sets inp & outp */

    /* send the open to venus. */
    INIT_IN(&inp->ih, CFS_CREATE, cred);
    inp->VFid = *fid;
    inp->excl = exclusive;
    inp->mode = mode;
    inp->attr = *va;

    inp->name = (char *)Isize;
    STRCPY(name, nm, len);		/* increments Isize */

    error = cfscall(mdp, Isize, &Osize, (char *)inp);
    if (!error)
    	error = outp->oh.result;
    if (!error) {
	*VFid = outp->VFid;
	*attr = outp->attr;
    }

    CFS_FREE(cfs_create_buf, cfs_create_size);
    return error;
}

venus_remove(void *mdp, ViceFid *fid,
        char *nm, int len,
	struct ucred *cred, struct proc *p)
{
    DECL_NO_OUT(cfs_remove);		/* sets Isize & Osize */
    cfs_remove_size += len + 1;
    ALLOC(cfs_remove);			/* sets inp & outp */

    /* send the open to venus. */
    INIT_IN(&inp->ih, CFS_REMOVE, cred);
    inp->VFid = *fid;

    inp->name = (char *)Isize;
    STRCPY(name, nm, len);		/* increments Isize */

    error = cfscall(mdp, Isize, &Osize, (char *)inp);
    if (!error)
    	error = outp->result;

    CFS_FREE(cfs_remove_buf, cfs_remove_size);
    return error;
}

venus_link(void *mdp, ViceFid *fid, ViceFid *tfid,
        char *nm, int len,
	struct ucred *cred, struct proc *p)
{
    DECL_NO_OUT(cfs_link);		/* sets Isize & Osize */
    cfs_link_size += len + 1;
    ALLOC(cfs_link);			/* sets inp & outp */

    /* send the open to venus. */
    INIT_IN(&inp->ih, CFS_LINK, cred);
    inp->sourceFid = *fid;
    inp->destFid = *tfid;

    inp->tname = (char *)Isize;
    STRCPY(tname, nm, len);		/* increments Isize */

    error = cfscall(mdp, Isize, &Osize, (char *)inp);
    if (!error)
    	error = outp->result;

    CFS_FREE(cfs_link_buf, cfs_link_size);
    return error;
}

venus_rename(void *mdp, ViceFid *fid, ViceFid *tfid,
        char *nm, int len, char *tnm, int tlen,
	struct ucred *cred, struct proc *p)
{
    DECL_NO_OUT(cfs_rename);		/* sets Isize & Osize */
    cfs_rename_size += len + 1 + tlen + 1;
    ALLOC(cfs_rename);			/* sets inp & outp */

    /* send the open to venus. */
    INIT_IN(&inp->ih, CFS_RENAME, cred);
    inp->sourceFid = *fid;
    inp->destFid = *tfid;

    inp->srcname = (char*)Isize;
    STRCPY(srcname, nm, len);		/* increments Isize */

    inp->destname = (char *)Isize;
    STRCPY(destname, tnm, tlen);	/* increments Isize */

    error = cfscall(mdp, Isize, &Osize, (char *)inp);

    if (!error)
    	error = outp->result;

    CFS_FREE(cfs_rename_buf, cfs_rename_size);
    return error;
}

venus_mkdir(void *mdp, ViceFid *fid,
    	char *nm, int len, struct vattr *va,
	struct ucred *cred, struct proc *p,
/*out*/	ViceFid *VFid, struct vattr *ova)
{
    DECL(cfs_mkdir);			/* sets Isize & Osize */
    cfs_mkdir_size += len + 1;
    ALLOC(cfs_mkdir);			/* sets inp & outp */

    /* send the open to venus. */
    INIT_IN(&inp->ih, CFS_MKDIR, cred);
    inp->VFid = *fid;
    inp->attr = *va;

    inp->name = (char *)Isize;
    STRCPY(name, nm, len);		/* increments Isize */

    error = cfscall(mdp, Isize, &Osize, (char *)inp);
    if (!error)
    	error = outp->oh.result;
    if (!error) {
	*VFid = outp->VFid;
	*ova = outp->attr;
    }

    CFS_FREE(cfs_mkdir_buf, cfs_mkdir_size);
    return error;
}

venus_rmdir(void *mdp, ViceFid *fid,
    	char *nm, int len,
	struct ucred *cred, struct proc *p)
{
    DECL_NO_OUT(cfs_rmdir);		/* sets Isize & Osize */
    cfs_rmdir_size += len + 1;
    ALLOC(cfs_rmdir);			/* sets inp & outp */

    /* send the open to venus. */
    INIT_IN(&inp->ih, CFS_RMDIR, cred);
    inp->VFid = *fid;

    inp->name = (char *)Isize;
    STRCPY(name, nm, len);		/* increments Isize */

    error = cfscall(mdp, Isize, &Osize, (char *)inp);
    if (!error)
    	error = outp->result;

    CFS_FREE(cfs_rmdir_buf, cfs_rmdir_size);
    return error;
}

venus_symlink(void *mdp, ViceFid *fid,
        char *lnm, int llen, char *nm, int len, struct vattr *va,
	struct ucred *cred, struct proc *p)
{
    DECL_NO_OUT(cfs_symlink);		/* sets Isize & Osize */
    cfs_symlink_size += llen + 1 + len + 1;
    ALLOC(cfs_symlink);			/* sets inp & outp */

    /* send the open to venus. */
    INIT_IN(&inp->ih, CFS_SYMLINK, cred);
    inp->VFid = *fid;
    inp->attr = *va;

    inp->srcname =(char*)Isize;
    STRCPY(srcname, lnm, llen);		/* increments Isize */

    inp->tname = (char *)Isize;
    STRCPY(tname, nm, len);		/* increments Isize */

    error = cfscall(mdp, Isize, &Osize, (char *)inp);

    if (!error)
    	error = outp->result;

    CFS_FREE(cfs_symlink_buf, cfs_symlink_size);
    return error;
}

venus_readdir(void *mdp, ViceFid *fid,
    	int count, int offset,
	struct ucred *cred, struct proc *p,
/*out*/	char *buffer, int *len)
{
    DECL(cfs_readdir);			/* sets Isize & Osize */
    cfs_readdir_size = VC_MAXMSGSIZE;
    ALLOC(cfs_readdir);			/* sets inp & outp */

    /* send the open to venus. */
    INIT_IN(&inp->ih, CFS_READDIR, cred);
    inp->VFid = *fid;
    inp->count = count;
    inp->offset = offset;

    Osize = VC_MAXMSGSIZE;
    error = cfscall(mdp, Isize, &Osize, (char *)inp);
    if (!error)
    	error = outp->oh.result;
    if (!error) {
	bcopy((char *)outp + (int)outp->data, buffer, outp->size);
	*len = outp->size;
    }

    CFS_FREE(cfs_readdir_buf, cfs_readdir_size);
    return error;
}

venus_fhtovp(void *mdp, ViceFid *fid,
	struct ucred *cred, struct proc *p,
/*out*/	ViceFid *VFid, int *vtype)
{
    DECL(cfs_vget);			/* sets Isize & Osize */
    ALLOC(cfs_vget);			/* sets inp & outp */

    /* Send the open to Venus. */
    INIT_IN(&inp->ih, CFS_VGET, cred);
    inp->VFid = *fid;

    error = cfscall(mdp, Isize, &Osize, (char *)inp);

    if (!error)
    	error = outp->oh.result;
    if (!error) {
	*VFid = outp->VFid;
	*vtype = outp->vtype;
    }

    CFS_FREE(cfs_vget_buf, cfs_vget_size);
    return error;
}
