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

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/kernel-src/vfs/mach/cfs_vnodeops.c,v 1.2 1997/01/07 18:44:16 rvb Exp";
#endif /*_BLURB_*/


/* 
 * Mach Operating System
 * Copyright (c) 1990 Carnegie-Mellon University
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
 * cfs_vnodeops.c,v
 * Revision 1.2  1996/01/02 16:57:07  bnoble
 * Added support for Coda MiniCache and raw inode calls (final commit)
 *
 * Revision 1.1.2.1  1995/12/20 01:57:34  bnoble
 * Added CFS-specific files
 *
 * Revision 3.1.1.1  1995/03/04  19:08:06  bnoble
 * Branch for NetBSD port revisions
 *
 * Revision 3.1  1995/03/04  19:08:04  bnoble
 * Bump to major revision 3 to prepare for NetBSD port
 *
 * Revision 2.6  1995/02/17  16:25:26  dcs
 * These versions represent several changes:
 * 1. Allow venus to restart even if outstanding references exist.
 * 2. Have only one ctlvp per client, as opposed to one per mounted cfs device.d
 * 3. Allow ody_expand to return many members, not just one.
 *
 * Revision 2.5  94/11/09  20:29:27  dcs
 * Small bug in remove dealing with hard links and link counts was fixed.
 * 
 * Revision 2.4  94/10/14  09:58:42  dcs
 * Made changes 'cause sun4s have braindead compilers
 * 
 * Revision 2.3  94/10/12  16:46:37  dcs
 * Cleaned kernel/venus interface by removing XDR junk, plus
 * so cleanup to allow this code to be more easily ported.
 * 
 * Revision 2.2  94/09/20  14:12:41  dcs
 * Fixed bug in rename when moving a directory.
 * 
 * Revision 2.1  94/07/21  16:25:22  satya
 * Conversion to C++ 3.0; start of Coda Release 2.0
 * 
 * Revision 1.4  93/12/17  01:38:01  luqi
 * Changes made for kernel to pass process info to Venus:
 * 
 * (1) in file cfs.h
 * add process id and process group id in most of the cfs argument types.
 * 
 * (2) in file cfs_vnodeops.c
 * add process info passing in most of the cfs vnode operations.
 * 
 * (3) in file cfs_xdr.c
 * expand xdr routines according changes in (1). 
 * add variable pass_process_info to allow venus for kernel version checking.
 * 
 * Revision 1.3  93/05/28  16:24:33  bnoble
 * *** empty log message ***
 * 
 * Revision 1.2  92/10/27  17:58:25  lily
 * merge kernel/latest and alpha/src/cfs
 * 
 * Revision 2.4  92/09/30  14:16:37  mja
 * 	Redid buffer allocation so that it does kmem_{alloc,free} for all
 * 	architectures.  Zone allocation, previously used on the 386, caused
 * 	panics if it was invoked repeatedly.  Stack allocation, previously
 * 	used on all other architectures, tickled some Mach bug that appeared
 * 	with large stack frames.
 * 	[91/02/09            jjk]
 * 
 * 	Added contributors blurb.
 * 	[90/12/13            jjk]
 * 
 * Revision 2.3  90/07/26  15:50:09  mrt
 * 	    Fixed fix to rename to remove .. from moved directories.
 * 	[90/06/28            dcs]
 * 
 * Revision 1.7  90/06/28  16:24:25  dcs
 * Fixed bug with moving directories, we weren't flushing .. for the moved directory.
 * 
 * Revision 1.6  90/05/31  17:01:47  dcs
 * Prepare for merge with facilities kernel.
 * 
 * 
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/acct.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <cfs/cfs.h>
#include <cfs/cnode.h>
#include <cfs/cfs_opstats.h>
#include <vm/vm_kern.h>


struct cnode *makecfsnode();
struct cnode *cfsnc_lookup();
struct cnode *cfs_find();

/* 
 * These flags select various performance enhancements.
 */
int cfs_intercept_rdwr = 1;    /* Set to handle read/write in the kernel */
int cfs_attr_cache  = 1;       /* Set to cache attributes in the kernel */
int cfs_symlink_cache = 1;     /* Set to cache symbolic link information */
int cfs_access_cache = 1;      /* Set to handle some access checks directly */

/* structure to keep track of vfs calls */

struct cfs_op_stats cfs_vnodeopstats[CFS_VNODEOPS_SIZE];

#define MARK_ENTRY(op) (cfs_vnodeopstats[op].entries++)
#define MARK_INT_SAT(op) (cfs_vnodeopstats[op].sat_intrn++)
#define MARK_INT_FAIL(op) (cfs_vnodeopstats[op].unsat_intrn++)
#define MARK_INT_GEN(op) (cfs_vnodeopstats[op].gen_intrn++)

int
cfs_vnodeopstats_init()
{
	register int i;
	
	for(i=0;i<CFS_VNODEOPS_SIZE;i++) {
		cfs_vnodeopstats[i].opcode = i;
		cfs_vnodeopstats[i].entries = 0;
		cfs_vnodeopstats[i].sat_intrn = 0;
		cfs_vnodeopstats[i].unsat_intrn = 0;
		cfs_vnodeopstats[i].gen_intrn = 0;
	}
	
	return 0;
}
		
/*ARGSUSED*/
/* 
 * cfs_open calls Venus to return the device, inode pair of the cache
 * file holding the data. Using iget, cfs_open finds the vnode of the
 * cache file, and then opens it.
 */

int
cfs_open(vpp, flag, cred, p)
    register struct vnode **vpp;
    int flag;
    struct ucred *cred;
    struct proc *p;
{
    struct inputArgs *inp = NULL;
    struct outputArgs *outp;
    int outSize = sizeof(struct outputArgs);
    struct cnode *cp = VTOC(*vpp);
    struct vnode *vp;
    int error;
    
    MARK_ENTRY(CFS_OPEN_STATS);
    
    /* Check for operation on a dying object */
    if (IS_DYING(cp)) {
	MARK_INT_FAIL(CFS_OPEN_STATS);
	COMPLAIN_BITTERLY(open, cp->c_fid);
	return(ENODEV);	/* Can't contact dead venus */
    }
    
    /* Check for open of control file. */
    if (IS_CTL_VP(*vpp)) {
	/* XXX */
	/* if (WRITEABLE(flag)) */ 
	if (flag & (FWRITE | FTRUNC | FCREAT | FEXCL)) {
	    MARK_INT_FAIL(CFS_OPEN_STATS);
	    return(EACCES);
	}
	MARK_INT_SAT(CFS_OPEN_STATS);
	return(0);
    }
    
    CFS_ALLOC(inp, struct inputArgs *, sizeof(struct inputArgs));
    outp = (struct outputArgs *)inp;
    /* Send the open to Venus. */
    INIT_IN(inp, CFS_OPEN, cred);
    
    inp->d.cfs_open.VFid.Volume = cp->c_fid.Volume;
    inp->d.cfs_open.VFid.Vnode  = cp->c_fid.Vnode;
    inp->d.cfs_open.VFid.Unique = cp->c_fid.Unique;
    inp->d.cfs_open.flags = flag;
    
    error = cfscall(vftomi(VN_VFS(*vpp)), sizeof(struct inputArgs), 
		    &outSize, (char *)inp);
    if (!error) {
	error = outp->result;
	CFSDEBUG( CFS_OPEN,myprintf(("open: dev %d inode %d result %d\n",
				  outp->d.cfs_open.dev,
				  outp->d.cfs_open.inode, error)); )
    }
    if (error) {
	goto exit;
    }
    
    /* Translate the <device, inode> pair for the cache file into
       an inode pointer. */
    error = cfs_grab_vnode(outp->d.cfs_open.dev, outp->d.cfs_open.inode, &vp);
    if (error) {
	goto exit;
    }
    /* We get the vnode back locked in both Mach and NetBSD.  Needs unlocked */
    VOP_DO_UNLOCK(vp);
    
    /* Keep a reference until the close comes in. */
    VN_HOLD(*vpp);                
    
    /* Save the vnode pointer for the cache file. */
    if (cp->c_ovp == NULL) {
	cp->c_ovp = vp;
    }
    else {
	if (cp->c_ovp != vp)
	    panic("cfs_open:  cp->c_ovp != ITOV(ip)");
    }
    cp->c_ocount++;
    
    /* Flush the attribute cached if writing the file. */
    if (flag & FWRITE) {
	cp->c_owrite++;
	cp->c_flags &= ~C_VATTR;
    }
    
    /* Save the <device, inode> pair for the cache file to speed
       up subsequent page_read's. */
    cp->c_device = outp->d.cfs_open.dev;
    cp->c_inode = outp->d.cfs_open.inode;
    
    /* Open the cache file. */
    error = VOP_DO_OPEN(&(cp->c_ovp), flag, cred, p); 
 exit:
    if (inp) CFS_FREE(inp,sizeof(struct inputArgs));
    return(error);
}

/*ARGSUSED*/
/*
 * Close the cache file used for I/O and notify Venus.
 */
int
cfs_close(vp, flag, cred, p)
    struct vnode *vp;
    int flag;
    struct ucred *cred;
    struct proc *p;
{ 
    struct inputArgs *inp = NULL;
    struct outputArgs *outp;
    int outSize = sizeof(struct outputArgs);
    struct cnode *cp = VTOC(vp);
    int error;
    
    MARK_ENTRY(CFS_CLOSE_STATS);
    
    /* Check for operation on a dying object */
    if (IS_DYING(cp)) {
	MARK_INT_FAIL(CFS_CLOSE_STATS);
	COMPLAIN_BITTERLY(close, cp->c_fid);
	return(ENODEV);	/* Can't contact dead venus */
    }
    
    /* Check for close of control file. */
    if (IS_CTL_VP(vp)) {
	MARK_INT_SAT(CFS_CLOSE_STATS);
	return(0);
    }
    
    VOP_DO_CLOSE(cp->c_ovp, flag, cred, p); /* Do errors matter here? */
    VN_RELE(cp->c_ovp);
    if (--cp->c_ocount == 0)
	cp->c_ovp = NULL;
    
    if (flag & FWRITE)                    /* file was opened for write */
	--cp->c_owrite;
    
    CFS_ALLOC(inp, struct inputArgs *, sizeof(struct inputArgs));
    outp = (struct outputArgs *) inp;

    INIT_IN(inp, CFS_CLOSE, cred);
    inp->d.cfs_close.VFid.Volume = cp->c_fid.Volume;
    inp->d.cfs_close.VFid.Vnode  = cp->c_fid.Vnode;
    inp->d.cfs_close.VFid.Unique = cp->c_fid.Unique;
    inp->d.cfs_close.flags = flag;
    
    error = cfscall(vftomi(VN_VFS(vp)), sizeof(struct inputArgs), 
		    &outSize, (char *)inp);
    if (!error) 
	error = outp->result;
    
    VN_RELE(CTOV(cp));

    if (inp) CFS_FREE(inp,sizeof(struct inputArgs));
    CFSDEBUG(CFS_CLOSE, myprintf(("close: result %d\n",error)); )
    return(error);
}

int
cfs_rdwr(vp, uiop, rw, ioflag, cred, p)
    struct vnode *vp;
    struct uio *uiop;
    enum uio_rw rw;
    int ioflag;
    struct ucred *cred;
    struct proc *p;
{ 
    struct cnode *cp = VTOC(vp);
    int error = 0;
    
    MARK_ENTRY(CFS_RDWR_STATS);
    
    CFSDEBUG(CFS_RDWR, myprintf(("cfs_rdwr(%d, %x, %d, %d, %d)\n", rw, 
			      uiop->uio_iov->iov_base, uiop->uio_resid, 
			      uiop->uio_offset, uiop->uio_segflg)); )
	
    /* Check for operation on a dying object */
    if (IS_DYING(cp)) {
	MARK_INT_FAIL(CFS_RDWR_STATS);
	COMPLAIN_BITTERLY(rdwr, cp->c_fid);
	return(ENODEV);	/* Can't contact dead venus */
    }
    
    /* Check for rdwr of control object. */
    if (IS_CTL_VP(vp)) {
	MARK_INT_FAIL(CFS_RDWR_STATS);
	return(EINVAL);
    }
    
    if (cfs_intercept_rdwr) {
	/* Redirect the request to UFS. */
	
	/* 
	 * If file is not already open this must be a page
	 * {read,write} request.  Iget the cache file's inode
	 * pointer if we still have its <device, inode> pair.
	 * Otherwise, we must do an internal open to derive the
	 * pair. 
	 */
	struct vnode *cfvp = cp->c_ovp;
	int igot_internally = 0;
	int opened_internally = 0;
	if (cfvp == NULL) {
	    /* 
	     * If we're dumping core, do the internal open. Otherwise
	     * venus won't have the correct size of the core when
	     * it's completely written.
	     */
	    if (cp->c_inode != 0 && !DUMPING_CORE) { 
		igot_internally = 1;
		error = cfs_grab_vnode(cp->c_device, cp->c_inode, &cfvp);
		if (error) {
		    MARK_INT_FAIL(CFS_RDWR_STATS);
		    return(error);
		}
		/* 
		 * We get the vnode back locked in both Mach and
		 * NetBSD.  Needs unlocked 
		 */
		VOP_DO_UNLOCK(cfvp);
	    }
	    else {
		opened_internally = 1;
		MARK_INT_GEN(CFS_OPEN_STATS);
		error = cfs_open(&vp, (rw == UIO_READ ? FREAD : FWRITE), 
				 cred, p);
		if (error) {
		    MARK_INT_FAIL(CFS_RDWR_STATS);
		    return(error);
		}
		cfvp = cp->c_ovp;
	    }
	}
	
	/* Have UFS handle the call. */
	CFSDEBUG(CFS_RDWR, myprintf(("indirect rdwr: fid = (%x.%x.%x), refcnt = %d\n",
				  cp->c_fid.Volume, cp->c_fid.Vnode, 
				  cp->c_fid.Unique, CNODE_COUNT(cp))); )
	if (rw == UIO_READ) {
	    error = VOP_DO_READ(cfvp, uiop, ioflag, cred);
	} else {
	    error = VOP_DO_WRITE(cfvp, uiop, ioflag, cred);
	}
	
	if (error)
	    MARK_INT_FAIL(CFS_RDWR_STATS);
	else
	    MARK_INT_SAT(CFS_RDWR_STATS);
	
	/* Do an internal close if necessary. */
	if (opened_internally) {
	    MARK_INT_GEN(CFS_CLOSE_STATS);
	    (void)cfs_close(vp, (rw == UIO_READ ? FREAD : FWRITE), cred);
	}
    }
    else {
	/* Read/Write the blocks from/to Venus. */
	/* I'm putting this here so the malloc doesn't occur every iteration. */
	char *buf = NULL;

	CFS_ALLOC(buf, char *, VC_MAXMSGSIZE);
	
	while (uiop->uio_resid > 0) {
	    struct inputArgs *in = (struct inputArgs *)buf;
	    struct outputArgs *out = (struct outputArgs *)buf;
	    int size;
	    struct iovec *iovp = uiop->uio_iov;
	    unsigned count = iovp->iov_len;
	    
	    if (count == 0) {
		uiop->uio_iov++;
		uiop->uio_iovcnt--;
		continue;
	    }
	    if (count > VC_DATASIZE)
		count = VC_DATASIZE;
	    
	    INIT_IN(in, CFS_RDWR, cred);
	    in->d.cfs_rdwr.VFid = cp->c_fid;
	    in->d.cfs_rdwr.rwflag = (int)rw;
	    in->d.cfs_rdwr.count = count;
	    in->d.cfs_rdwr.data = (char *)(VC_INSIZE(cfs_rdwr_in));
	    if (rw == UIO_WRITE) {
		bcopy(iovp->iov_base, (char *)in 
		      + (int)in->d.cfs_rdwr.data, count);
		size = sizeof(struct outputArgs);
	    } else {
		size = sizeof(struct outputArgs) + count;
	    }
	    
	    in->d.cfs_rdwr.offset = uiop->uio_offset;
	    in->d.cfs_rdwr.ioflag = ioflag;
	    
	    error = cfscall(vftomi(VN_VFS(CTOV(cp))), sizeof(struct inputArgs) 
			    + count, &size, buf);
	    
	    if (!error) error = out->result;
	    CFSDEBUG(CFS_RDWR, myprintf(("cfs_rdwr(%d, %d, %d, %d) returns (%d, %d)\n",
				      rw, count, uiop->uio_offset,
				      uiop->uio_segflg, error,
				      out->d.cfs_rdwr.count)); );
	    
	    if (error) break;
	    
	    if (rw == UIO_READ)
		bcopy((char *)out + (int)out->d.cfs_rdwr.data, 
		      iovp->iov_base, out->d.cfs_rdwr.count);
	    iovp->iov_base += out->d.cfs_rdwr.count;
	    iovp->iov_len -= out->d.cfs_rdwr.count;
	    uiop->uio_resid -= out->d.cfs_rdwr.count;
	    uiop->uio_offset += out->d.cfs_rdwr.count;
	    
	    /* Exit the loop if Venus R/W fewer bytes than we specified. */
	    /* Maybe we should continue if ANY bytes were R/W? -JJK */
	    if (out->d.cfs_rdwr.count != count)
		break;
	}
	if (buf) CFS_FREE(buf, VC_MAXMSGSIZE);
    }
    
    /* Invalidate cached attributes if writing. */
    if (rw == UIO_WRITE)
	cp->c_flags &= ~C_VATTR;
    return(error);
}

/*ARGSUSED*/
int
cfs_ioctl(vp, com, data, flag, cred, p)
    struct vnode *vp;
    int com;
    caddr_t data;
    int flag;
    struct ucred *cred;
    struct proc  *p;
{ 
    struct inputArgs *in = NULL;
    struct outputArgs *out;
    int error, size;
    struct vnode *tvp;
    register struct a {
	char *path;
	struct ViceIoctl vidata;
	int follow;
    } *iap = (struct a *)data;
    char *buf;
    struct nameidata *ndp;
    
    MARK_ENTRY(CFS_IOCTL_STATS);
    
    CFSDEBUG(CFS_IOCTL, myprintf(("in cfs_ioctl on %s\n", iap->path));)
	
    /* Don't check for operation on a dying object, for ctlvp it
       shouldn't matter */
	
    /* Must be control object to succeed. */
    if (!IS_CTL_VP(vp)) {
	MARK_INT_FAIL(CFS_IOCTL_STATS);
	CFSDEBUG(CFS_IOCTL, myprintf(("cfs_ioctl error: vp != ctlvp"));)
	    return (EOPNOTSUPP);
    }
    /* Look up the pathname. */
    
    /* Should we use the name cache here? It would get it from
       lookupname sooner or later anyway, right? */
    
    DO_LOOKUP(iap->path, UIO_USERSPACE, 
	      (iap->follow ? FOLLOW_LINK : NO_FOLLOW),
	      (struct vnode **)0, &tvp, p, ndp, error);

    if (error) {
	MARK_INT_FAIL(CFS_IOCTL_STATS);
	CFSDEBUG(CFS_IOCTL, myprintf(("cfs_ioctl error: lookup returns %d\n",
				   error));)
	return(error);
    }
    
    /* 
     * Make sure this is a coda style cnode, but it may be a
     * different vfsp 
     */
    /* XXX: this totally violates the comment about vtagtype in vnode.h */
    if (!IS_CODA_VNODE(tvp)) {
	VN_RELE(tvp);
	MARK_INT_FAIL(CFS_IOCTL_STATS);
	CFSDEBUG(CFS_IOCTL, 
		 myprintf(("cfs_ioctl error: %s not a coda object\n", 
			iap->path));)
	return(EINVAL);
    }
    
    /* Copy in the IN buffer. */
    if (iap->vidata.in_size > VC_DATASIZE) {
	VN_RELE(tvp);
	return(EINVAL);
    }
    
    CFS_ALLOC(buf, char *, VC_MAXMSGSIZE);
    
    in = (struct inputArgs *)buf;
    out = (struct outputArgs *)buf;	
    INIT_IN(in, CFS_IOCTL, cred);
    in->d.cfs_ioctl.VFid = (VTOC(tvp))->c_fid;
    
    /* Command was mutated by increasing its size field to reflect the  
     * path and follow args. We need to subtract that out before sending
     * the command to Venus.
     */
    in->d.cfs_ioctl.cmd = (com & ~(IOCPARM_MASK << 16));	
    size = ((com >> 16) & IOCPARM_MASK) - sizeof(char *) - sizeof(int);
    in->d.cfs_ioctl.cmd |= (size & IOCPARM_MASK) <<	16;	
    
    in->d.cfs_ioctl.rwflag = flag;
    in->d.cfs_ioctl.len = iap->vidata.in_size;
    in->d.cfs_ioctl.data = (char *)(VC_INSIZE(cfs_ioctl_in));
    
    error = copyin(iap->vidata.in, (char*)in + (int)in->d.cfs_ioctl.data, 
		   iap->vidata.in_size);
    if (error) {
	VN_RELE(tvp);
	CFS_FREE(buf, VC_MAXMSGSIZE);
	MARK_INT_FAIL(CFS_IOCTL_STATS);
	return(error);
    }
    
    size = VC_MAXMSGSIZE;
    error = cfscall(vftomi(VN_VFS(tvp)), 
		    VC_INSIZE(cfs_ioctl_in) + iap->vidata.in_size, 
		    &size, buf);
    
    if (!error) {
	error = out->result;
	CFSDEBUG(CFS_IOCTL, myprintf(("Ioctl returns %d \n", out->result)); )
    }
    
	/* Copy out the OUT buffer. */
    if (!error) {
	if (out->d.cfs_ioctl.len > iap->vidata.out_size) {
	    CFSDEBUG(CFS_IOCTL, myprintf(("return len %d <= request len %d\n",
				       out->d.cfs_ioctl.len, 
				       iap->vidata.out_size)); );
	    error = EINVAL;
	}
	else {
	    error = copyout((char *)out + (int)out->d.cfs_ioctl.data, 
			    iap->vidata.out, iap->vidata.out_size);
	}
    }
    
    VN_RELE(tvp);
    if (buf) CFS_FREE(buf, VC_MAXMSGSIZE);
    return(error);
}

/*ARGSUSED*/
int
cfs_select(vp, which, cred, p)
    struct vnode *vp;
    int which;
    struct ucred *cred;
    struct proc *p;
{
	MARK_ENTRY(CFS_SELECT_STATS);

	myprintf(("in cfs_select\n"));
	MARK_INT_FAIL(CFS_SELECT_STATS);
	return (EOPNOTSUPP);
}

/*
 * To reduce the cost of a user-level venus, we cache attributes in
 * the kernel.  Each cnode has storage allocated for an attribute. If
 * c_vattr is valid, return a reference to it. Otherwise, get the
 * attributes from venus and store them in the cnode.  There is some
 * question if this method is a security leak. But I think that in
 * order to make this call, the user must have done a lookup and
 * opened the file, and therefore should already have access.  
 */

int
cfs_getattr(vp, vap, cred, p)
    struct vnode *vp;
    struct vattr *vap;
    struct ucred *cred;
    struct proc *p;
{
    struct cnode *scp = NULL, *cp = VTOC(vp);
    int error, size;
    struct inputArgs *inp=NULL;
    struct outputArgs *outp;
    
    MARK_ENTRY(CFS_GETATTR_STATS);
    
    /* Check for operation on a dying object */
    if (IS_DYING(cp)) {
	COMPLAIN_BITTERLY(getattr, cp->c_fid);
	scp = cp;	/* Save old cp */
	/* If no error, gives a valid vnode with which to work. */
	error = getNewVnode(&vp);	
	if (error) {
	    MARK_INT_FAIL(CFS_GETATTR_STATS);
	    return(error);	/* Can't contact dead venus */
	}
	cp = VTOC(vp);
    }
    
    /* Check for getattr of control object. */
    if (IS_CTL_VP(vp)) {
	MARK_INT_FAIL(CFS_GETATTR_STATS);
	return(ENOENT);
    }
    
    /* Check to see if the attributes have already been cached */
    if (VALID_VATTR(cp)) { 
	CFSDEBUG(CFS_GETATTR, { myprintf(("attr cache hit: (%x.%x.%x)\n",
				       cp->c_fid.Volume,
				       cp->c_fid.Vnode,
				       cp->c_fid.Unique));});
	CFSDEBUG(CFS_GETATTR, if (!(cfsdebug & ~CFS_GETATTR))
		 print_vattr(&cp->c_vattr); );
	
	*vap = cp->c_vattr;
	MARK_INT_SAT(CFS_GETATTR_STATS);
	if (scp) VN_RELE(vp);
	return(0);
    }

    CFS_ALLOC(inp, struct inputArgs *, sizeof(struct inputArgs));
    outp = (struct outputArgs *) inp;
    
    INIT_IN(inp, CFS_GETATTR, cred);
    inp->d.cfs_getattr.VFid = cp->c_fid;
    size = VC_OUTSIZE(cfs_getattr_out);
    error = cfscall(vftomi(VN_VFS(vp)), VC_INSIZE(cfs_getattr_in),
		    &size, (char *)inp);
    
    if (!error) 
	error = outp->result;
    
    if (!error) {
	CFSDEBUG(CFS_GETATTR, myprintf(("getattr miss (%x.%x.%x): result %d\n",
				     cp->c_fid.Volume,
				     cp->c_fid.Vnode,
				     cp->c_fid.Unique,
				     outp->result)); )
	    
	CFSDEBUG(CFS_GETATTR, if (!(cfsdebug & ~CFS_GETATTR))
		 print_vattr(&outp->d.cfs_getattr.attr);	);
	
	/* If not open for write, store attributes in cnode */   
	if ((cp->c_owrite == 0) && (cfs_attr_cache)) {  
	    cp->c_vattr = outp->d.cfs_getattr.attr; 
	    cp->c_flags |= C_VATTR; 
	}
	
	*vap = outp->d.cfs_getattr.attr;
    }
    if (scp) VN_RELE(vp);
    if (inp) CFS_FREE(inp, sizeof(struct inputArgs));
    return(error);
}

int
cfs_setattr(vp, vap, cred, p)
    register struct vnode *vp;
    register struct vattr *vap;
    struct ucred *cred;
    struct proc *p;
{ 
    struct cnode *cp = VTOC(vp);
    struct inputArgs *inp =NULL;
    struct outputArgs *outp;
    int size;
    int error;
    
    MARK_ENTRY(CFS_SETATTR_STATS);
    
    /* Check for operation on a dying object */
    if (IS_DYING(cp)) {
	MARK_INT_FAIL(CFS_SETATTR_STATS);
	COMPLAIN_BITTERLY(setattr, cp->c_fid);
	return(ENODEV);	/* Can't contact dead venus */
    }
    
    /* Check for setattr of control object. */
    if (IS_CTL_VP(vp)) {
	MARK_INT_FAIL(CFS_SETATTR_STATS);
	return(ENOENT);
    }
    
    CFS_ALLOC(inp, struct inputArgs *, sizeof (struct inputArgs));
    outp = (struct outputArgs *)inp;

    INIT_IN(inp, CFS_SETATTR, cred);
    inp->d.cfs_setattr.VFid = cp->c_fid;
    inp->d.cfs_setattr.attr = *vap;
    size = VC_OUT_NO_DATA;        

    if (cfsdebug & CFSDBGMSK(CFS_SETATTR)) {
	print_vattr(vap);
    }
    error = cfscall(vftomi(VN_VFS(vp)), VC_INSIZE(cfs_setattr_in),
		    &size, (char *)inp);
    
    if (!error) 
	error = outp->result;
    
    if (!error)
	cp->c_flags &= ~C_VATTR;
    
    if (inp) CFS_FREE(inp,sizeof(struct inputArgs));
    CFSDEBUG(CFS_SETATTR,	myprintf(("setattr %d\n", error)); )
    return(error);
}

int
cfs_access(vp, mode, cred, p)
    struct vnode *vp;
    int mode;
    struct ucred *cred;
    struct proc *p;
{ 
    struct cnode *cp = VTOC(vp);
    struct inputArgs *inp =NULL;
    struct outputArgs *outp;
    int size;
    int error;
    
    MARK_ENTRY(CFS_ACCESS_STATS);
    
    /* Check for operation on a dying object */
    if (IS_DYING(cp)) {
	MARK_INT_FAIL(CFS_ACCESS_STATS);
	COMPLAIN_BITTERLY(access, cp->c_fid);
	return(ENODEV);	/* Can't contact dead venus */
    }
    
    /* Check for access of control object.  Only read access is
       allowed on it. */
    if (IS_CTL_VP(vp)) {
	/* bogus hack - all will be marked as successes */
	MARK_INT_SAT(CFS_ACCESS_STATS);
	return(((mode & VREAD) && !(mode & (VWRITE | VEXEC))) 
	       ? 0 : EACCES);
    }
    
    /*
     * if the file is a directory, and we are checking exec (eg lookup) 
     * access, and the file is in the namecache, then the user must have 
     * lookup access to it.
     */
    if (cfs_access_cache) {
	if ((VN_TYPE(vp) == VDIR) && (mode & VEXEC)) {
	    if (cfsnc_lookup(cp, ".", cred)) {
		MARK_INT_SAT(CFS_ACCESS_STATS);
		return(0);                     /* it was in the cache */
	    }
	}
    }

    CFS_ALLOC(inp, struct inputArgs *, sizeof(struct inputArgs));
    outp = (struct outputArgs *)inp;
    
    INIT_IN(inp, CFS_ACCESS, cred);
    inp->d.cfs_access.VFid = cp->c_fid;
    inp->d.cfs_access.flags = mode;
    size = VC_OUT_NO_DATA;
    
    error = cfscall(vftomi(VN_VFS(vp)), VC_INSIZE(cfs_access_in),
		    &size, (char *)inp);
    
    if (inp) CFS_FREE(inp, sizeof(struct inputArgs));

    if (!error) 
	error = outp->result;
    
    return(error);
}

int
cfs_readlink(vp, uiop, cred, p)
    struct vnode *vp;
    struct uio *uiop;
    struct ucred *cred;
    struct proc *p;
{ 
    int error, size;
    struct inputArgs *in;
    struct outputArgs *out;
    struct cnode *cp = VTOC(vp);
    char *buf=NULL; /*[CFS_MAXPATHLEN + VC_INSIZE(cfs_readlink_in)];*/
    
    MARK_ENTRY(CFS_READLINK_STATS);
    
    /* Check for operation on a dying object */
    if (IS_DYING(cp)) {
	MARK_INT_FAIL(CFS_READLINK_STATS);
	COMPLAIN_BITTERLY(readlink, cp->c_fid);
	return(ENODEV);	/* Can't contact dead venus */
    }
    
    /* Check for readlink of control object. */
    if (IS_CTL_VP(vp)) {
	MARK_INT_FAIL(CFS_READLINK_STATS);
	return(ENOENT);
    }
    
    if ((cfs_symlink_cache) && (VALID_SYMLINK(cp))) { /* symlink was cached */
	UIOMOVE(cp->c_symlink, (int)cp->c_symlen, UIO_READ, uiop, error);
	if (error)
	    MARK_INT_FAIL(CFS_READLINK_STATS);
	else
	    MARK_INT_SAT(CFS_READLINK_STATS);
	return(error);
    }
    
    CFS_ALLOC(buf, char*, CFS_MAXPATHLEN + VC_INSIZE(cfs_readlink_in));
    in = (struct inputArgs *)buf;
    out = (struct outputArgs *)buf;
    
    INIT_IN(in, CFS_READLINK, cred);
    in->d.cfs_readlink.VFid = cp->c_fid;
    
    size = CFS_MAXPATHLEN + VC_OUTSIZE(cfs_readlink_out);
    error = cfscall(vftomi(VN_VFS(vp)), VC_INSIZE(cfs_readlink_in),
		    &size, buf);
    
    error = error ? error : out->result;
    
    if (!error) {
	if (cfs_symlink_cache) {
	    CFS_ALLOC(cp->c_symlink, char *, out->d.cfs_readlink.count);
	    cp->c_symlen = out->d.cfs_readlink.count;
	    bcopy((char *)out + (int)out->d.cfs_readlink.data, 
		  cp->c_symlink, out->d.cfs_readlink.count);
	    cp->c_flags |= C_SYMLINK;
	}
	
	UIOMOVE((char *)out + (int)out->d.cfs_readlink.data,
			out->d.cfs_readlink.count, UIO_READ, uiop, error);
    }
    
    if (buf) CFS_FREE(buf, CFS_MAXPATHLEN + VC_INSIZE(cfs_readlink_in));

    CFSDEBUG(CFS_READLINK, myprintf(("in readlink result %d\n",error));)
    return(error);
}

/*ARGSUSED*/
int
cfs_fsync(vp, cred, p)
    struct vnode *vp;
    struct ucred *cred;
    struct proc *p;
{ 
    struct inputArgs *inp=NULL;
    struct outputArgs *outp;
    struct cnode *cp = VTOC(vp);
    int size;
    int error;
    
    MARK_ENTRY(CFS_FSYNC_STATS);

    /* Check for fsync on an unmounting object */
    /* The NetBSD kernel, in it's infinite wisdom, can try to fsync
     * after an unmount has been initiated.  This is a Bad Thing,
     * which we have to avoid.  Not a legitimate failure for stats.
     */
    if (IS_UNMOUNTING(cp)) {
	return(ENODEV);
    }
       
    /* Check for operation on a dying object */
    /* We can expect fsync on the root vnode if we are in the midst
       of unmounting (in NetBSD), so silently ignore it. */
    if (IS_DYING(cp)) {
	if (!IS_ROOT_VP(vp)) {
	    COMPLAIN_BITTERLY(fsync, cp->c_fid);
	    MARK_INT_FAIL(CFS_FSYNC_STATS);
	}
	return(ENODEV);	/* Can't contact dead venus */
    }
    
    /* Check for fsync of control object. */
    if (IS_CTL_VP(vp)) {
	MARK_INT_SAT(CFS_FSYNC_STATS);
	return(0);
    }

    CFS_ALLOC(inp, struct inputArgs *, sizeof(struct inputArgs));
    outp = (struct outputArgs *)inp;

    INIT_IN(inp, CFS_FSYNC, cred);
    inp->d.cfs_fsync.VFid = cp->c_fid;
    size = VC_INSIZE(cfs_fsync_in);
    
    error = cfscall(vftomi(VN_VFS(vp)), size, &size, (char *)inp);
    if (!error) 
	error = outp->result;
    
    CFSDEBUG(CFS_FSYNC, myprintf(("in fsync result %d\n",error)); );
    
    if (inp) CFS_FREE(inp, sizeof(struct inputArgs));
    return(error);
}

/*ARGSUSED*/
int
cfs_inactive(vp, cred, p)
    struct vnode *vp;
    struct ucred *cred;
    struct proc *p;
{ 
    struct cnode *cp = VTOC(vp);
    
    /* We don't need to send inactive to venus - DCS */
    MARK_ENTRY(CFS_INACTIVE_STATS);
    
    if (IS_CTL_VP(vp)) {
	MARK_INT_SAT(CFS_INACTIVE_STATS);
	return 0;
    }
    
    CFSDEBUG(CFS_INACTIVE, myprintf(("in inactive, %x.%x.%x. vfsp %x\n",
				  cp->c_fid.Volume, cp->c_fid.Vnode, 
				  cp->c_fid.Unique, VN_VFS(vp)));)
	
    /* If an array has been allocated to hold the symlink, deallocate it */
    if ((cfs_symlink_cache) && (VALID_SYMLINK(cp))) {
	if (cp->c_symlink == NULL)
	    panic("cfs_inactive: null symlink pointer in cnode");
	
	CFS_FREE(cp->c_symlink, cp->c_symlen);
	cp->c_flags &= ~C_SYMLINK;
	cp->c_symlen = 0;
    }
    
    /* Remove it from the table so it can't be found. */
    cfs_unsave(cp);
    if (cp->c_ovp != NULL)
	panic("cfs_inactive:  cp->ovp != NULL");
    if ((struct cfs_mntinfo *)(VN_VFS(vp)->VFS_DATA) == NULL) {
	if (!IS_DYING(cp)) {
	    myprintf(("Help! vfsp->vfs_data was NULL, but vnode %x wasn't dying\n", vp));
	    panic("badness in cfs_inactive\n");
	}
    } else {
	((struct cfs_mntinfo *)(VN_VFS(vp)->VFS_DATA))->mi_refct--;
    }
    
#ifdef DIAGNOSTIC
    if (CNODE_COUNT(cp)) {
	panic("cfs_inactive: nonzero reference count");
    }
#endif
    CFS_CLEAN_VNODE(vp);

    MARK_INT_SAT(CFS_INACTIVE_STATS);
    return(0);
}

/*
 * Remote file system operations having to do with directory manipulation.
 */

/* 
 * It appears that in NetBSD, lookup is supposed to return the vnode locked
 */

int
cfs_lookup(dvp, nm, vpp, cred, p)
    struct vnode *dvp;
    char *nm;
    struct vnode **vpp;
    struct ucred *cred;
    struct proc *p;
{ 
    struct cnode *cp;
    struct cnode *scp = NULL, *dcp = VTOC(dvp);
    char *buf = NULL; /*[VC_INSIZE(cfs_lookup_in) + CFS_MAXNAMLEN + 1];*/
    struct inputArgs *in;
    struct outputArgs *out;
    int error = 0;
    int s, size;

    MARK_ENTRY(CFS_LOOKUP_STATS);

    CFSDEBUG(CFS_LOOKUP, myprintf(("lookup: %s in %d.%d.%d\n",
				   nm, dcp->c_fid.Volume,
				   dcp->c_fid.Vnode, dcp->c_fid.Unique)););
    
    /* Check for operation on a dying object */
    if (IS_DYING(dcp)) {
	COMPLAIN_BITTERLY(lookup, dcp->c_fid);
	scp = dcp;	/* Save old dcp */
	/* If no error, gives a valid vnode with which to work. */
	error = getNewVnode(&dvp);	
	if (error) {
	    MARK_INT_FAIL(CFS_LOOKUP_STATS);
	    return(error);	/* Can't contact dead venus */
	}
	dcp = VTOC(dvp);
    }
    
    /* Check for lookup of control object. */
    if (IS_CTL_NAME(dvp, nm)) {
	*vpp = CFS_CTL_VP;
	VN_HOLD(*vpp);
	MARK_INT_SAT(CFS_LOOKUP_STATS);
	if (scp) VN_RELE(dvp);
	return(0);
    }
    
    /* First try to look the file up in the cfs name cache */
    /* lock the parent vnode? */
    
    cp = cfsnc_lookup(dcp, nm, cred);
    if (cp) {
	*vpp = CTOV(cp);
	VN_HOLD(*vpp);
	CFSDEBUG(CFS_LOOKUP, 
		 myprintf(("lookup result %d vpp 0x%x\n",error,*vpp));)
    } else {
	
	/* The name wasn't cached, so we need to contact Venus */
	CFS_ALLOC(buf, char *, (VC_INSIZE(cfs_lookup_in) + CFS_MAXNAMLEN + 1));
	in = (struct inputArgs *)buf;
	out = (struct outputArgs *)buf;
	INIT_IN(in, CFS_LOOKUP, cred);
	
	in->d.cfs_lookup.VFid = dcp->c_fid;
	size = VC_INSIZE(cfs_lookup_in);
	in->d.cfs_lookup.name = (char *)size;
	s = strlen(nm) + 1;
	if (s > CFS_MAXNAMLEN) {
	    MARK_INT_FAIL(CFS_LOOKUP_STATS);
	    CFSDEBUG(CFS_LOOKUP, myprintf(("name too long: lookup, %x.%x.%x(%s)\n",
					dcp->c_fid.Volume, dcp->c_fid.Vnode,
					dcp->c_fid.Unique, nm)););
	    *vpp = (struct vnode *)0;
	    error = EINVAL;
	    goto exit;
	}
	
	strncpy((char *)in + (int)in->d.cfs_lookup.name, nm, s);
	size += s;
	
	error = cfscall(vftomi(VN_VFS(dvp)), size, &size, buf);
	
	if (!error) 
	    error = out->result;
	
	if (error) {
	    MARK_INT_FAIL(CFS_LOOKUP_STATS);
	    CFSDEBUG(CFS_LOOKUP, myprintf(("lookup error on %x.%x.%x(%s)%d\n",
					dcp->c_fid.Volume, dcp->c_fid.Vnode, dcp->c_fid.Unique, nm, error));)
	    *vpp = (struct vnode *)0;
	} else {
	    MARK_INT_SAT(CFS_LOOKUP_STATS);
	    CFSDEBUG(CFS_LOOKUP, 
		     myprintf(("lookup: vol %x vno %x uni %x type %o result %d\n",
			    out->d.cfs_lookup.VFid.Volume, 
			    out->d.cfs_lookup.VFid.Vnode,
			    out->d.cfs_lookup.VFid.Unique,
			    out->d.cfs_lookup.vtype,
			    out->result)); )
		
	    cp = makecfsnode(&out->d.cfs_lookup.VFid, VN_VFS(dvp), 
			      out->d.cfs_lookup.vtype);
	    *vpp = CTOV(cp);
	    /*LOOKUP_LOCK(*vpp);*/  /* XXX - broken! */
	    
	    /* enter the new vnode in the Name Cache only if the top bit isn't set */
	    /* And don't enter a new vnode for an invalid one! */
	    if (!(out->d.cfs_lookup.vtype & CFS_NOCACHE) && scp == 0)
		cfsnc_enter(VTOC(dvp), nm, cred, VTOC(*vpp));
	}
    }

 exit:
    if (buf) CFS_FREE(buf, (VC_INSIZE(cfs_lookup_in) + CFS_MAXNAMLEN + 1));
    if (scp) VN_RELE(dvp);
    return(error);
}

/*ARGSUSED*/
int
cfs_create(dvp, nm, va, exclusive, mode, vpp, cred, p)
    struct vnode *dvp;
    char *nm;
    struct vattr *va;
    enum vcexcl exclusive;
    int mode;
    struct vnode **vpp;
    struct ucred *cred;
    struct proc *p;
{
    struct cnode *cp;
    struct inputArgs *in;
    struct outputArgs *out;
    char *buf=NULL; /*[CFS_MAXNAMLEN + VC_INSIZE(cfs_create_in)];*/
    struct cnode *dcp = VTOC(dvp);	
    int error, size, s;
    
    MARK_ENTRY(CFS_CREATE_STATS);
    
    /* Check for operation on a dying object */
    if (IS_DYING(dcp)) {
	MARK_INT_FAIL(CFS_CREATE_STATS);
	COMPLAIN_BITTERLY(create, dcp->c_fid);
	return(ENODEV);	/* Can't contact dead venus */
    }
    
    /* Check for create of control object. */
    if (IS_CTL_NAME(dvp, nm)) {
	*vpp = (struct vnode *)0;
	MARK_INT_FAIL(CFS_CREATE_STATS);
	return(EACCES);
    }

    CFS_ALLOC(buf, char *, (CFS_MAXNAMLEN + VC_INSIZE(cfs_create_in)));
    in = (struct inputArgs *)buf;
    out = (struct outputArgs *)buf;
    INIT_IN(in, CFS_CREATE, cred);
    in->d.cfs_create.VFid = dcp->c_fid;
    in->d.cfs_create.excl = exclusive;
    in->d.cfs_create.mode = mode;
    in->d.cfs_create.attr = *va;
    
    size = VC_INSIZE(cfs_create_in);
    in->d.cfs_create.name = (char *)size;
    
    s = strlen(nm) + 1;
    strncpy((char*)in + (int)in->d.cfs_create.name, nm, s);
    size += s;
    
    error = cfscall(vftomi(VN_VFS(dvp)), size, &size, buf);
    
    if (!error) 
	error = out->result;
    
    if (!error) {
	
	/* If this is an exclusive create, panic if the file already exists. */
	/* Venus should have detected the file and reported EEXIST. */

	if ((exclusive == EXCL) &&
	    (cfs_find(&out->d.cfs_create.VFid, VN_VFS(dvp)) != NULL))
	    panic("cnode existed for newly created file!");
	
	cp = makecfsnode(&out->d.cfs_create.VFid, VN_VFS(dvp),
			 VATTR_TYPE((struct vattr *)
				    &(out->d.cfs_create.attr)));
	*vpp = CTOV(cp);
	
	/* Update va to reflect the new attributes. */
	(*va) = out->d.cfs_create.attr;
	
	/* Update the attribute cache and mark it as valid */
	if (cfs_attr_cache) {
	    VTOC(*vpp)->c_vattr = out->d.cfs_create.attr;  
	    VTOC(*vpp)->c_flags |= C_VATTR;       
	}
	
	/* Invalidate the parent's attr cache, the modification time has changed */
	VTOC(dvp)->c_flags &= ~C_VATTR;
	
	/* enter the new vnode in the Name Cache */
	cfsnc_enter(VTOC(dvp), nm, cred, VTOC(*vpp));
	
	CFSDEBUG(CFS_CREATE, 
		 myprintf(("create: (%x.%x.%x), result %d\n",
			out->d.cfs_create.VFid.Volume,
			out->d.cfs_create.VFid.Vnode,
			out->d.cfs_create.VFid.Unique,
			out->result)); )
    }
    else {
	*vpp = (struct vnode *)0;
	CFSDEBUG(CFS_CREATE, myprintf(("create error %d\n",error));)
    }

    if (buf) CFS_FREE(buf, (CFS_MAXNAMLEN + VC_INSIZE(cfs_create_in)));
    return(error);
}

int
cfs_remove(dvp, nm, cred, p)
    struct vnode *dvp;
    char *nm;
    struct ucred *cred;
    struct proc *p;
{ 
    struct cnode *tp, *cp = VTOC(dvp);
    char *buf=NULL; /*[CFS_MAXNAMLEN + sizeof(struct inputArgs)];*/
    struct inputArgs *in;
    struct outputArgs *out;
    int error, s;
    
    MARK_ENTRY(CFS_REMOVE_STATS);
    
    CFSDEBUG(CFS_REMOVE, myprintf(("remove: %s in %d.%d.%d\n",
				   nm, cp->c_fid.Volume, cp->c_fid.Vnode,
				   cp->c_fid.Unique)););

    /* Check for operation on a dying object */
    if (IS_DYING(cp)) {
	MARK_INT_FAIL(CFS_REMOVE_STATS);
	COMPLAIN_BITTERLY(remove, cp->c_fid);
	return(ENODEV);	/* Can't contact dead venus */
    }
    
    /* Remove the file's entry from the CFS Name Cache */
    /* We're being conservative here, it might be that this person
     * doesn't really have sufficient access to delete the file
     * but we feel zapping the entry won't really hurt anyone -- dcs
     */
    /* I'm gonna go out on a limb here. If a file and a hardlink to it
     * exist, and one is removed, the link count on the other will be
     * off by 1. We could either invalidate the attrs if cached, or
     * fix them. I'll try to fix them. DCS 11/8/94
     */
    tp = cfsnc_lookup(VTOC(dvp), nm, cred);
    if (tp) {
	if (VALID_VATTR(tp)) {	/* If attrs are cached */
	    if (tp->c_vattr.va_nlink > 1) {	/* If it's a hard link */
		tp->c_vattr.va_nlink--;
	    }
	}
	
	cfsnc_zapfile(VTOC(dvp), nm); 
	/* No need to flush it if it doesn't exist! */
    }
    /* Invalidate the parent's attr cache, the modification time has changed */
    VTOC(dvp)->c_flags &= ~C_VATTR;
    
    /* Check for remove of control object. */
    if (IS_CTL_NAME(dvp, nm)) {
	MARK_INT_FAIL(CFS_REMOVE_STATS);
	return(ENOENT);
    }

    CFS_ALLOC(buf, char *, CFS_MAXNAMLEN + sizeof(struct inputArgs));
    in = (struct inputArgs *) buf;
    out = (struct outputArgs *) buf;
    
    INIT_IN(in, CFS_REMOVE, cred);
    in->d.cfs_remove.VFid = cp->c_fid;
    in->d.cfs_remove.name = (char *)(VC_INSIZE(cfs_remove_in));
    s = strlen(nm) + 1;
    strncpy((char *)in + (int)in->d.cfs_remove.name, nm, s);
    s += VC_INSIZE(cfs_remove_in);
    
    error = cfscall(vftomi(VN_VFS(dvp)), s, &s, (char *)in);
    
    if (!error) 
	error = out->result;
    
    if (buf) CFS_FREE(buf, CFS_MAXNAMLEN + sizeof(struct inputArgs));
    CFSDEBUG(CFS_REMOVE, myprintf(("in remove result %d\n",error)); )
    return(error);
}

int
cfs_link(vp, tdvp, tnm, cred, p)
    struct vnode *vp;
    struct vnode *tdvp;
    char *tnm;
    struct ucred *cred;
    struct proc *p;
{
    char *buf=NULL; /*[CFS_MAXNAMLEN + sizeof(struct inputArgs)];*/
    struct inputArgs *in;
    struct outputArgs *out;
    struct cnode *cp = VTOC(vp);
    struct cnode *tdcp = VTOC(tdvp);
    int error, s;
    
    MARK_ENTRY(CFS_LINK_STATS);
    
    if (cfsdebug & CFSDBGMSK(CFS_LINK)) {
	myprintf(("link:   vp fid: (%d.%d.%d)\n",
		  cp->c_fid.Volume, cp->c_fid.Vnode, cp->c_fid.Unique));
	myprintf(("link: tdvp fid: (%d.%d.%d)\n",
		  tdcp->c_fid.Volume, tdcp->c_fid.Vnode, tdcp->c_fid.Unique));

    }

    /* Check for operation on a dying object */
    if (IS_DYING(cp) || IS_DYING(tdcp)) {
	MARK_INT_FAIL(CFS_LINK_STATS);
	COMPLAIN_BITTERLY(link, cp->c_fid);
	COMPLAIN_BITTERLY(link, tdcp->c_fid);
	return(ENODEV);	/* Can't contact dead venus */
    }
    
    /* Check for link to/from control object. */
    if (IS_CTL_NAME(tdvp, tnm) || IS_CTL_VP(vp)) {
	MARK_INT_FAIL(CFS_LINK_STATS);
	return(EACCES);
    }

    CFS_ALLOC(buf, char *, CFS_MAXNAMLEN + sizeof(struct inputArgs));
    in = (struct inputArgs *) buf;
    out = (struct outputArgs *) buf;

    INIT_IN(in, CFS_LINK, cred);
    in->d.cfs_link.sourceFid = cp->c_fid;
    in->d.cfs_link.destFid = tdcp->c_fid;
    in->d.cfs_link.tname = (char *)(VC_INSIZE(cfs_link_in));
    s = strlen(tnm) + 1;
    strncpy((char *)in + (int)in->d.cfs_link.tname, tnm, s);
    s += VC_INSIZE(cfs_link_in);
    
    error = cfscall(vftomi(VN_VFS(vp)), s, &s, (char *)in);
    if (!error) 
	error = out->result;
    
    /* Invalidate the parent's attr cache, the modification time has changed */
    VTOC(tdvp)->c_flags &= ~C_VATTR;
    VTOC(vp)->c_flags &= ~C_VATTR;
    
    CFSDEBUG(CFS_LINK,	myprintf(("in link result %d\n",error)); )
    if (buf) CFS_FREE(buf, CFS_MAXNAMLEN + sizeof(struct inputArgs));
    return(error);
}

int
cfs_rename(odvp, onm, ndvp, nnm, cred, p)
    struct vnode *odvp;
    char *onm;
    struct vnode *ndvp;
    char *nnm;
    struct ucred *cred;
    struct proc *p;
{
    /* Buffer to hold the basic message, 2 names, and padding to word align them */
    char *buf=NULL; /*[VC_INSIZE(cfs_rename_in) + 2 * CFS_MAXNAMLEN + 8];*/
    struct inputArgs *in;
    struct outputArgs *out;
    struct cnode *odcp = VTOC(odvp);
    struct cnode *ndcp = VTOC(ndvp);
    int s, size, error;
    
    MARK_ENTRY(CFS_RENAME_STATS);
    
    
    /* Check for operation on a dying object */
    if (IS_DYING(ndcp) || IS_DYING(odcp)) {
	MARK_INT_FAIL(CFS_RENAME_STATS);
	COMPLAIN_BITTERLY(rename, ndcp->c_fid);
	COMPLAIN_BITTERLY(rename, odcp->c_fid);
	return(ENODEV);	/* Can't contact dead venus */
    }
    
    /* Check for rename involving control object. */ 
    if (IS_CTL_NAME(odvp, onm) || IS_CTL_NAME(ndvp, nnm)) {
	MARK_INT_FAIL(CFS_RENAME_STATS);
	return(EACCES);
    }
    
    /* Problem with moving directories -- need to flush entry for .. */
    if (odvp != ndvp) {
	struct vnode *ovp = CTOV( cfsnc_lookup(VTOC(odvp), onm, cred) );
	if ((ovp) &&
	    (VN_TYPE(ovp) == VDIR)) /* If it's a directory */
	    cfsnc_zapfile(VTOC(ovp),"..");
    }
    
    /* Remove the entries for both source and target files */
    cfsnc_zapfile(VTOC(odvp), onm);
    cfsnc_zapfile(VTOC(ndvp), nnm);
    
    /* Invalidate the parent's attr cache, the modification time has changed */
    VTOC(odvp)->c_flags &= ~C_VATTR;
    VTOC(ndvp)->c_flags &= ~C_VATTR;
    
    CFS_ALLOC(buf, char *, VC_INSIZE(cfs_rename_in) + 2 * CFS_MAXNAMLEN + 8);
    in = (struct inputArgs *)buf;
    out = (struct outputArgs *)buf;

    INIT_IN(in, CFS_RENAME, cred);
    
    in->d.cfs_rename.sourceFid = odcp->c_fid;
    in->d.cfs_rename.destFid = ndcp->c_fid;
    
    size = VC_INSIZE(cfs_rename_in);	
    in->d.cfs_rename.srcname = (char*)size;
    s = (strlen(onm) & ~0x3) + 4;	/* Round up to word boundary. */
    if (s > CFS_MAXNAMLEN) {
	MARK_INT_FAIL(CFS_RENAME_STATS);
	error = EINVAL;
	goto exit;
    }
    strncpy((char *)in + (int)in->d.cfs_rename.srcname, onm, s);
    
    size += s;
    in->d.cfs_rename.destname = (char *)size;
    
    s = (strlen(nnm) & ~0x3) + 4;	/* Round up to word boundary. */
    if (s > CFS_MAXNAMLEN) {
	MARK_INT_FAIL(CFS_RENAME_STATS);
	error = EINVAL;
	goto exit;
    }
    strncpy((char *)in + (int)in->d.cfs_rename.destname, nnm, s);
    
    size += s;
    error = cfscall(vftomi(VN_VFS(odvp)), size, &size, (char *)in);
    if (!error) 
	error = out->result;
    
 exit:
    if (buf) CFS_FREE(buf, VC_INSIZE(cfs_rename_in) + 2 * CFS_MAXNAMLEN + 8);
    CFSDEBUG(CFS_RENAME, myprintf(("in rename result %d\n",error));)
    return(error);
}

int
cfs_mkdir(dvp, nm, va, vpp, cred, p)
    struct vnode *dvp;
    char *nm;
    register struct vattr *va;
    struct vnode **vpp;
    struct ucred *cred;
    struct proc *p;
{ 
    struct cnode *cp;
    char *buf=NULL; /*[CFS_MAXNAMLEN + VC_INSIZE(cfs_mkdir_in)];*/
    struct inputArgs *in;
    struct outputArgs *out;
    struct cnode *dcp = VTOC(dvp);	
    int error, size;
    
    MARK_ENTRY(CFS_MKDIR_STATS);
    
    /* Check for operation on a dying object */
    if (IS_DYING(dcp)) {
	MARK_INT_FAIL(CFS_MKDIR_STATS);
	COMPLAIN_BITTERLY(mkdir, dcp->c_fid);
	return(ENODEV);	/* Can't contact dead venus */
    }
    
    /* Check for mkdir of target object. */
    if (IS_CTL_NAME(dvp, nm)) {
	*vpp = (struct vnode *)0;
	MARK_INT_FAIL(CFS_MKDIR_STATS);
	return(EACCES);
    }
    
    size = strlen(nm) + 1;
    
    if (size > CFS_MAXNAMLEN) {
	*vpp = (struct vnode *)0;
	MARK_INT_FAIL(CFS_MKDIR_STATS);
	return(EACCES);
    }

    CFS_ALLOC(buf, char *, CFS_MAXNAMLEN + VC_INSIZE(cfs_mkdir_in));
    in = (struct inputArgs *)buf;
    out = (struct outputArgs *)buf;
    INIT_IN(in, CFS_MKDIR, cred);
    
    in->d.cfs_mkdir.VFid = dcp->c_fid;
    in->d.cfs_mkdir.attr = *va;
    in->d.cfs_mkdir.name = (char *)(VC_INSIZE(cfs_mkdir_in));
    strncpy((char *)in + (int)in->d.cfs_mkdir.name, nm, size);
    
    size += VC_INSIZE(cfs_mkdir_in);
    
    error = cfscall(vftomi(VN_VFS(dvp)), size, &size, (char *)in);
    
    if (!error) 
	error = out->result;
    
    if (!error) {
	if (cfs_find(&out->d.cfs_mkdir.VFid, VN_VFS(dvp)) != NULL)
	    panic("cnode existed for newly created directory!");
	
	
	cp =  makecfsnode(&out->d.cfs_mkdir.VFid, 
			  VN_VFS(dvp), VATTR_TYPE(va));
	*vpp = CTOV(cp);
	
	/* enter the new vnode in the Name Cache */
	cfsnc_enter(VTOC(dvp), nm, cred, VTOC(*vpp));
	
	/* as a side effect, enter "." and ".." for the directory */
	cfsnc_enter(VTOC(*vpp), ".", cred, VTOC(*vpp));
	cfsnc_enter(VTOC(*vpp), "..", cred, VTOC(dvp));
	
	if (cfs_attr_cache) {
	    VTOC(*vpp)->c_vattr = out->d.cfs_mkdir.attr;/* update the attr cache */
	    VTOC(*vpp)->c_flags |= C_VATTR;   /* Valid attributes in cnode */
	}
	
	/* Invalidate the parent's attr cache, the modification time has changed */
	VTOC(dvp)->c_flags &= ~C_VATTR;
	
	CFSDEBUG( CFS_MKDIR, myprintf(("mkdir: (%x.%x.%x) result %d\n",
				    out->d.cfs_mkdir.VFid.Volume,
				    out->d.cfs_mkdir.VFid.Vnode,
				    out->d.cfs_mkdir.VFid.Unique,
				    out->result)); )
    }
    else {
	*vpp = (struct vnode *)0;
	CFSDEBUG(CFS_MKDIR, myprintf(("mkdir error %d\n",error));)
    }
    
    if (buf) CFS_FREE(buf, CFS_MAXNAMLEN + VC_INSIZE(cfs_mkdir_in));
    return(error);
}

int
cfs_rmdir(dvp, nm, cred, p)
    struct vnode *dvp;
    char *nm;
    struct ucred *cred;
    struct proc *p;
{ 
    char *buf=NULL; /*[CFS_MAXNAMLEN + VC_INSIZE(cfs_rmdir_in)];*/
    struct inputArgs *in;
    struct outputArgs *out;
    struct cnode *dcp = VTOC(dvp);
    struct cnode *cp;
    int error, size;
    
    MARK_ENTRY(CFS_RMDIR_STATS);
    
    
    /* Check for operation on a dying object */
    if (IS_DYING(dcp)) {
	MARK_INT_FAIL(CFS_RMDIR_STATS);
	COMPLAIN_BITTERLY(rmdir, dcp->c_fid);
	return(ENODEV);	/* Can't contact dead venus */
    }
    
    /* Check for rmdir of control object. */
    if (IS_CTL_NAME(dvp, nm)) {
	MARK_INT_FAIL(CFS_RMDIR_STATS);
	return(ENOENT);
    }
    
    /* We're being conservative here, it might be that this person
     * doesn't really have sufficient access to delete the file
     * but we feel zapping the entry won't really hurt anyone -- dcs
     */
    /*
     * As a side effect of the rmdir, remove any entries for children of
     * the directory, especially "." and "..".
     */
    cp = cfsnc_lookup(dcp, nm, cred);
    if (cp) cfsnc_zapParentfid(&(cp->c_fid));
    
    /* Remove the file's entry from the CFS Name Cache */
    cfsnc_zapfile(dcp, nm);
    
    /* Invalidate the parent's attr cache, the modification time has changed */
    dcp->c_flags &= ~C_VATTR;

    CFS_ALLOC(buf, char *, CFS_MAXNAMLEN + VC_INSIZE(cfs_rmdir_in));
    in = (struct inputArgs *)buf;
    out = (struct outputArgs *)buf;
    INIT_IN(in, CFS_RMDIR, cred);
    
    in->d.cfs_rmdir.VFid = dcp->c_fid;
    in->d.cfs_rmdir.name = (char *)(VC_INSIZE(cfs_rmdir_in));
    
    size = strlen(nm) + 1;
	
    strncpy((char *)in + (int)in->d.cfs_rmdir.name, nm, size);
    size = VC_INSIZE(cfs_rmdir_in) + size;
    
    error = cfscall(vftomi(VN_VFS(dvp)), size, &size, (char *)in);
    if (!error) 
	error = out->result;
    
    CFSDEBUG(CFS_RMDIR, myprintf(("in rmdir result %d\n",error)); )

    if (buf) CFS_FREE(buf, CFS_MAXNAMLEN + VC_INSIZE(cfs_rmdir_in));
    return(error);
}

int
cfs_symlink(tdvp, tnm, tva, lnm, cred, p)
    struct vnode *tdvp;
    char *tnm;
    struct vattr *tva;
    char *lnm;
    struct ucred *cred;
    struct proc *p;
{
    /* allocate space for regular input, plus 1 path and 1 name, plus padding */
    char *buf=NULL; /*[sizeof(struct inputArgs) + CFS_MAXPATHLEN + CFS_MAXNAMLEN + 8];*/
    struct inputArgs *in;
    struct outputArgs *out;
    struct cnode *tdcp = VTOC(tdvp);	
    int error, size, s;
    
    MARK_ENTRY(CFS_SYMLINK_STATS);
    
    /* Check for operation on a dying object */
    if (IS_DYING(tdcp)) {
	MARK_INT_FAIL(CFS_SYMLINK_STATS);
	COMPLAIN_BITTERLY(symlink, tdcp->c_fid);
	return(ENODEV);	/* Can't contact dead venus */
    }
    
    /* Check for symlink of control object. */
    if (IS_CTL_NAME(tdvp, tnm)) {
	MARK_INT_FAIL(CFS_SYMLINK_STATS);
	return(EACCES);
    }
    
    CFS_ALLOC(buf, char *, sizeof(struct inputArgs) + CFS_MAXPATHLEN 
	                   + CFS_MAXNAMLEN + 8);
    in = (struct inputArgs *)buf;
    out = (struct outputArgs *)buf;
    INIT_IN(in, CFS_SYMLINK, cred);
    
    in->d.cfs_symlink.VFid = tdcp->c_fid;
    in->d.cfs_symlink.attr = *tva;
    
    size = VC_INSIZE(cfs_symlink_in);
    in->d.cfs_symlink.srcname =(char*)size;
    
    s = (strlen(lnm) & ~0x3) + 4;	/* Round up to word boundary. */
    if (s > CFS_MAXPATHLEN) {
	MARK_INT_FAIL(CFS_SYMLINK_STATS);
	return(EINVAL);
    }
    strncpy((char *)in + (int)in->d.cfs_symlink.srcname, lnm, s);
    
    size += s;
    in->d.cfs_symlink.tname = (char *)size;
    s = (strlen(tnm) & ~0x3) + 4;	/* Round up to word boundary. */
    if (s > CFS_MAXNAMLEN) {
	MARK_INT_FAIL(CFS_SYMLINK_STATS);
	error = EINVAL;
	goto exit;
    }
    strncpy((char *)in + (int)in->d.cfs_symlink.tname, tnm, s);
    
    size += s;
    error = cfscall(vftomi(VN_VFS(tdvp)), size, &size, (char *)in);
    if (!error)
	error = out->result; 
    
    /* Invalidate the parent's attr cache, the modification time has changed */
    tdcp->c_flags &= ~C_VATTR;

 exit:    
    if (buf) {
	CFS_FREE(buf, sizeof(struct inputArgs) + CFS_MAXPATHLEN 
		 + CFS_MAXNAMLEN + 8);
    }
    CFSDEBUG(CFS_SYMLINK, myprintf(("in symlink result %d\n",error)); )
    return(error);
}

/*
 * Read directory entries.
 */
int
cfs_readdir(vp, uiop, cred, eofflag, cookies, ncookies, p)
    struct vnode *vp;
    register struct uio *uiop;
    struct ucred *cred;
    int *eofflag;
    u_long *cookies;
    int ncookies;
    struct proc *p;
{ 
    struct cnode *cp = VTOC(vp);
    int error = 0;
    
    MARK_ENTRY(CFS_READDIR_STATS);
    
    CFSDEBUG(CFS_READDIR, myprintf(("cfs_readdir(%x, %d, %d, %d)\n", uiop->uio_iov->iov_base, uiop->uio_resid, uiop->uio_offset, uiop->uio_segflg)); )
	
    /* Check for operation on a dying object */
    if (IS_DYING(cp)) {
	MARK_INT_FAIL(CFS_READDIR_STATS);
	COMPLAIN_BITTERLY(readdir, cp->c_fid);
	return(ENODEV);	/* Can't contact dead venus */
    }
    
    /* Check for readdir of control object. */
    if (IS_CTL_VP(vp)) {
	MARK_INT_FAIL(CFS_READDIR_STATS);
	return(ENOENT);
    }
    
    if (cfs_intercept_rdwr) {
	/* Redirect the request to UFS. */
	
	/* If directory is not already open do an "internal open" on it. */
	int opened_internally = 0;
	if (cp->c_ovp == NULL) {
	    opened_internally = 1;
	    MARK_INT_GEN(CFS_OPEN_STATS);
	    error = cfs_open(&vp, FREAD, cred, p);
	    if (error) return(error);
	}
	
	/* Have UFS handle the call. */
	CFSDEBUG(CFS_READDIR, myprintf(("indirect readdir: fid = (%x.%x.%x), refcnt = %d\n",cp->c_fid.Volume, cp->c_fid.Vnode, cp->c_fid.Unique, CNODE_COUNT(VTOC(vp)))); )
	error = VOP_DO_READDIR(cp->c_ovp, uiop, cred, eofflag, cookies,
			       ncookies);
	
	if (error)
	    MARK_INT_FAIL(CFS_READDIR_STATS);
	else
	    MARK_INT_SAT(CFS_READDIR_STATS);
	
	/* Do an "internal close" if necessary. */ 
	if (opened_internally) {
	    MARK_INT_GEN(CFS_CLOSE_STATS);
	    (void)cfs_close(vp, FREAD, cred, p);
	}
    }
    else {
	/* Read the block from Venus. */
	struct inputArgs *in;
	struct outputArgs *out;
	struct iovec *iovp = uiop->uio_iov;
	unsigned count = iovp->iov_len;
	char *buf=NULL;
	int size;
	
	
	/* Make the count a multiple of DIRBLKSIZ (borrowed from ufs_readdir). */	
	if ((uiop->uio_iovcnt != 1) || (count < DIRBLKSIZ) ||
	    (uiop->uio_offset & (DIRBLKSIZ - 1)))
	    return (EINVAL);
	count &= ~(DIRBLKSIZ - 1);
	uiop->uio_resid -= iovp->iov_len - count;
	iovp->iov_len = count;
	if (count > VC_DATASIZE)
	    return(EINVAL);
	
	CFS_ALLOC(buf, char *, VC_MAXMSGSIZE);
	in = (struct inputArgs *)buf;
	out = (struct outputArgs *)buf;
	INIT_IN(in, CFS_READDIR, cred);
	
	in->d.cfs_readdir.VFid = cp->c_fid;
	in->d.cfs_readdir.count = count;
	in->d.cfs_readdir.offset = uiop->uio_offset;
	
	size = VC_MAXMSGSIZE;
	error = cfscall(vftomi(VN_VFS(CTOV(cp))), VC_INSIZE(cfs_readdir_in),
			&size, (char *)in);
	
	if (!error) error = out->result;
	
	CFSDEBUG(CFS_READDIR,
		 myprintf(("cfs_readdir(%x, %d, %d, %d) returns (%d, %d)\n",
			(char *)out + (int)out->d.cfs_readdir.data, in->d.cfs_readdir.count,
			in->d.cfs_readdir.offset, uiop->uio_segflg, error,
			out->d.cfs_readdir.size)); )
	    
	if (!error) {
	    bcopy((char *)out + (int)out->d.cfs_readdir.data, iovp->iov_base, out->d.cfs_readdir.size);
	    iovp->iov_base += out->d.cfs_readdir.size;
	    iovp->iov_len -= out->d.cfs_readdir.size;
	    uiop->uio_resid -= out->d.cfs_readdir.size;
	    uiop->uio_offset += out->d.cfs_readdir.size;
	}
	if (buf) CFS_FREE(buf, VC_MAXMSGSIZE);
	
    }
    
    return(error);
}

/*
 * Convert from file system blocks to device blocks
 */
int
cfs_bmap(vp, bn, vpp, bnp, p)
    struct vnode *vp;	/* file's vnode */
    daddr_t bn;		/* fs block number */
    struct vnode **vpp;	/* RETURN vp of device */
    daddr_t *bnp;		/* RETURN device block number */
    struct proc *p;
{ 
	*vpp = (struct vnode *)0;
	myprintf(("cfs_bmap called!\n"));
	return(EINVAL);
}

/*
 * I don't think the following two things are used anywhere, so I've
 * commented them out 
 * 
 * struct buf *async_bufhead; 
 * int async_daemon_count;
 */

int
cfs_strategy(bp, p)
    register struct buf *bp;
    struct proc *p;
{ 
	myprintf(("cfs_strategy called!\n"));
	return(EINVAL);
}


/* The following calls are MACH only:
   bread()
   brelse()
   badop()
   noop()
   fid()
   freefid()
   lockctl()
   page_read()
   page_write()
*/

#ifdef	__MACH__
/*
 * read a logical block and return it in a buffer */
int
cfs_bread(vp, lbn, bpp)
    struct vnode *vp;
    daddr_t lbn;
    struct buf **bpp; 
{
    myprintf(("cfs_bread called!\n"));
    return(EINVAL);
}

/*
 * release a block returned by cfs_bread
 */
int
cfs_brelse(vp, bp)
    struct vnode *vp;
    struct buf *bp; 
{

    myprintf(("cfs_brelse called!\n"));
    return(EINVAL);
}

int
cfs_badop()
{
	panic("cfs_badop");
}

int
cfs_noop()
{
	return (EINVAL);
}

int
cfs_fid(vp, fidpp)
	struct vnode *vp;
	struct fid **fidpp;
{
	struct cfid *cfid;

	cfid = (struct cfid *)kalloc(sizeof(struct cfid));
	bzero((caddr_t)cfid, sizeof(struct cfid));
	cfid->cfid_len = sizeof(struct cfid) - (sizeof(struct fid) - MAXFIDSZ);
	cfid->cfid_fid = VTOC(vp)->c_fid;
	*fidpp = (struct fid *)cfid;
	return (0);
}

int
cfs_freefid(vp, fidp)
	struct vnode *vp;
	struct fid *fidp;
{
	kfree((struct cfid *)fidp, sizeof(struct cfid));
	return (0);
}

/*
 * Record-locking requests are passed to the local Lock-Manager daemon.
 */
int
cfs_lockctl(vp, ld, cmd, cred)
	struct vnode *vp;
	struct flock *ld;
	int cmd;
	struct ucred *cred;
{ 
	myprintf(("cfs_lockctl called!\n"));
	return(EINVAL);
}

cfs_page_read(vp, buffer, size, offset, cred)
	struct vnode	*vp;
	caddr_t		buffer;
	int		size;
	vm_offset_t	offset;
	struct ucred *cred;
{ 
	struct cnode *cp = VTOC(vp);
	struct uio uio;
	struct iovec iov;
	int error = 0;

	CFSDEBUG(CFS_RDWR, myprintf(("cfs_page_read(%x, %d, %d), fid = (%x.%x.%x), refcnt = %d\n", buffer, size, offset, VTOC(vp)->c_fid.Volume, VTOC(vp)->c_fid.Vnode, VTOC(vp)->c_fid.Unique, vp->v_count)); )

	iov.iov_base = buffer;
	iov.iov_len = size;
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = offset;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_resid = size;
	error = cfs_rdwr(vp, &uio, UIO_READ, 0, cred);
	if (error) {
	    myprintf(("error %d on pagein (cfs_rdwr)\n", error));
	    error = EIO;
	}

/*
	if (!error && (cp->states & CWired) == 0)
	    cfs_Wire(cp);
*/

	return(error);
}

cfs_page_write(vp, buffer, size, offset, cred, init)
	struct vnode	*vp;
	caddr_t buffer;
	int size;
	vm_offset_t	offset;
	struct ucred *cred;
	boolean_t init;
{
	struct cnode *cp = VTOC(vp);
	struct uio uio;
	struct iovec iov;
	int error = 0;

	CFSDEBUG(CFS_RDWR, myprintf(("cfs_page_write(%x, %d, %d), fid = (%x.%x.%x), refcnt = %d\n", buffer, size, offset, VTOC(vp)->c_fid.Volume, VTOC(vp)->c_fid.Vnode, VTOC(vp)->c_fid.Unique, vp->v_count)); )

	if (init) {
	    panic("cfs_page_write: called from data_initialize");
	}

	iov.iov_base = buffer;
	iov.iov_len = size;
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = offset;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_resid = size;
	error = cfs_rdwr(vp, &uio, UIO_WRITE, 0, cred);
	if (error) {
	    myprintf(("error %d on pageout (cfs_rdwr)\n", error));
	    error = EIO;
	}

	return(error);
}

#endif
