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

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/coda-src/venus/vnode_if.h,v 1.1.1.1 1996/11/22 19:11:58 rvb Exp";
#endif /*_BLURB_*/


/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


/*
 * Warning: This file is generated automatically.
 * (Modifications made here may easily be lost!)
 *
 * Created by the script:
 *	$NetBSD: vnode_if.sh,v 1.8 1995/03/10 04:13:52 chopps Exp $
 */


extern struct vnodeop_desc vop_default_desc;


struct vop_lookup_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct vnode **a_vpp;
	struct componentname *a_cnp;
};
extern struct vnodeop_desc vop_lookup_desc;
static __inline int VOP_LOOKUP __P((struct vnode *, struct vnode **, 
    struct componentname *));
static __inline int VOP_LOOKUP(dvp, vpp, cnp)
	struct vnode *dvp;
	struct vnode **vpp;
	struct componentname *cnp;
{
	struct vop_lookup_args a;
	a.a_desc = VDESC(vop_lookup);
	a.a_dvp = dvp;
	a.a_vpp = vpp;
	a.a_cnp = cnp;
	return (VCALL(dvp, VOFFSET(vop_lookup), &a));
}

struct vop_create_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct vnode **a_vpp;
	struct componentname *a_cnp;
	struct vattr *a_vap;
};
extern struct vnodeop_desc vop_create_desc;
static __inline int VOP_CREATE __P((struct vnode *, struct vnode **, 
    struct componentname *, struct vattr *));
static __inline int VOP_CREATE(dvp, vpp, cnp, vap)
	struct vnode *dvp;
	struct vnode **vpp;
	struct componentname *cnp;
	struct vattr *vap;
{
	struct vop_create_args a;
	a.a_desc = VDESC(vop_create);
	a.a_dvp = dvp;
	a.a_vpp = vpp;
	a.a_cnp = cnp;
	a.a_vap = vap;
	return (VCALL(dvp, VOFFSET(vop_create), &a));
}

struct vop_mknod_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct vnode **a_vpp;
	struct componentname *a_cnp;
	struct vattr *a_vap;
};
extern struct vnodeop_desc vop_mknod_desc;
static __inline int VOP_MKNOD __P((struct vnode *, struct vnode **, 
    struct componentname *, struct vattr *));
static __inline int VOP_MKNOD(dvp, vpp, cnp, vap)
	struct vnode *dvp;
	struct vnode **vpp;
	struct componentname *cnp;
	struct vattr *vap;
{
	struct vop_mknod_args a;
	a.a_desc = VDESC(vop_mknod);
	a.a_dvp = dvp;
	a.a_vpp = vpp;
	a.a_cnp = cnp;
	a.a_vap = vap;
	return (VCALL(dvp, VOFFSET(vop_mknod), &a));
}

struct vop_open_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	int a_mode;
	struct ucred *a_cred;
	struct proc *a_p;
};
extern struct vnodeop_desc vop_open_desc;
static __inline int VOP_OPEN __P((struct vnode *, int, struct ucred *, 
    struct proc *));
static __inline int VOP_OPEN(vp, mode, cred, p)
	struct vnode *vp;
	int mode;
	struct ucred *cred;
	struct proc *p;
{
	struct vop_open_args a;
	a.a_desc = VDESC(vop_open);
	a.a_vp = vp;
	a.a_mode = mode;
	a.a_cred = cred;
	a.a_p = p;
	return (VCALL(vp, VOFFSET(vop_open), &a));
}

struct vop_close_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	int a_fflag;
	struct ucred *a_cred;
	struct proc *a_p;
};
extern struct vnodeop_desc vop_close_desc;
static __inline int VOP_CLOSE __P((struct vnode *, int, struct ucred *, 
    struct proc *));
static __inline int VOP_CLOSE(vp, fflag, cred, p)
	struct vnode *vp;
	int fflag;
	struct ucred *cred;
	struct proc *p;
{
	struct vop_close_args a;
	a.a_desc = VDESC(vop_close);
	a.a_vp = vp;
	a.a_fflag = fflag;
	a.a_cred = cred;
	a.a_p = p;
	return (VCALL(vp, VOFFSET(vop_close), &a));
}

struct vop_access_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	int a_mode;
	struct ucred *a_cred;
	struct proc *a_p;
};
extern struct vnodeop_desc vop_access_desc;
static __inline int VOP_ACCESS __P((struct vnode *, int, struct ucred *, 
    struct proc *));
static __inline int VOP_ACCESS(vp, mode, cred, p)
	struct vnode *vp;
	int mode;
	struct ucred *cred;
	struct proc *p;
{
	struct vop_access_args a;
	a.a_desc = VDESC(vop_access);
	a.a_vp = vp;
	a.a_mode = mode;
	a.a_cred = cred;
	a.a_p = p;
	return (VCALL(vp, VOFFSET(vop_access), &a));
}

struct vop_getattr_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct vattr *a_vap;
	struct ucred *a_cred;
	struct proc *a_p;
};
extern struct vnodeop_desc vop_getattr_desc;
static __inline int VOP_GETATTR __P((struct vnode *, struct vattr *, 
    struct ucred *, struct proc *));
static __inline int VOP_GETATTR(vp, vap, cred, p)
	struct vnode *vp;
	struct vattr *vap;
	struct ucred *cred;
	struct proc *p;
{
	struct vop_getattr_args a;
	a.a_desc = VDESC(vop_getattr);
	a.a_vp = vp;
	a.a_vap = vap;
	a.a_cred = cred;
	a.a_p = p;
	return (VCALL(vp, VOFFSET(vop_getattr), &a));
}

struct vop_setattr_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct vattr *a_vap;
	struct ucred *a_cred;
	struct proc *a_p;
};
extern struct vnodeop_desc vop_setattr_desc;
static __inline int VOP_SETATTR __P((struct vnode *, struct vattr *, 
    struct ucred *, struct proc *));
static __inline int VOP_SETATTR(vp, vap, cred, p)
	struct vnode *vp;
	struct vattr *vap;
	struct ucred *cred;
	struct proc *p;
{
	struct vop_setattr_args a;
	a.a_desc = VDESC(vop_setattr);
	a.a_vp = vp;
	a.a_vap = vap;
	a.a_cred = cred;
	a.a_p = p;
	return (VCALL(vp, VOFFSET(vop_setattr), &a));
}

struct vop_read_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct uio *a_uio;
	int a_ioflag;
	struct ucred *a_cred;
};
extern struct vnodeop_desc vop_read_desc;
static __inline int VOP_READ __P((struct vnode *, struct uio *, int, 
    struct ucred *));
static __inline int VOP_READ(vp, uio, ioflag, cred)
	struct vnode *vp;
	struct uio *uio;
	int ioflag;
	struct ucred *cred;
{
	struct vop_read_args a;
	a.a_desc = VDESC(vop_read);
	a.a_vp = vp;
	a.a_uio = uio;
	a.a_ioflag = ioflag;
	a.a_cred = cred;
	return (VCALL(vp, VOFFSET(vop_read), &a));
}

struct vop_write_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct uio *a_uio;
	int a_ioflag;
	struct ucred *a_cred;
};
extern struct vnodeop_desc vop_write_desc;
static __inline int VOP_WRITE __P((struct vnode *, struct uio *, int, 
    struct ucred *));
static __inline int VOP_WRITE(vp, uio, ioflag, cred)
	struct vnode *vp;
	struct uio *uio;
	int ioflag;
	struct ucred *cred;
{
	struct vop_write_args a;
	a.a_desc = VDESC(vop_write);
	a.a_vp = vp;
	a.a_uio = uio;
	a.a_ioflag = ioflag;
	a.a_cred = cred;
	return (VCALL(vp, VOFFSET(vop_write), &a));
}

struct vop_ioctl_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	u_long a_command;
	caddr_t a_data;
	int a_fflag;
	struct ucred *a_cred;
	struct proc *a_p;
};
extern struct vnodeop_desc vop_ioctl_desc;
static __inline int VOP_IOCTL __P((struct vnode *, u_long, caddr_t, int, 
    struct ucred *, struct proc *));
static __inline int VOP_IOCTL(vp, command, data, fflag, cred, p)
	struct vnode *vp;
	u_long command;
	caddr_t data;
	int fflag;
	struct ucred *cred;
	struct proc *p;
{
	struct vop_ioctl_args a;
	a.a_desc = VDESC(vop_ioctl);
	a.a_vp = vp;
	a.a_command = command;
	a.a_data = data;
	a.a_fflag = fflag;
	a.a_cred = cred;
	a.a_p = p;
	return (VCALL(vp, VOFFSET(vop_ioctl), &a));
}

struct vop_select_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	int a_which;
	int a_fflags;
	struct ucred *a_cred;
	struct proc *a_p;
};
extern struct vnodeop_desc vop_select_desc;
static __inline int VOP_SELECT __P((struct vnode *, int, int, 
    struct ucred *, struct proc *));
static __inline int VOP_SELECT(vp, which, fflags, cred, p)
	struct vnode *vp;
	int which;
	int fflags;
	struct ucred *cred;
	struct proc *p;
{
	struct vop_select_args a;
	a.a_desc = VDESC(vop_select);
	a.a_vp = vp;
	a.a_which = which;
	a.a_fflags = fflags;
	a.a_cred = cred;
	a.a_p = p;
	return (VCALL(vp, VOFFSET(vop_select), &a));
}

struct vop_mmap_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	int a_fflags;
	struct ucred *a_cred;
	struct proc *a_p;
};
extern struct vnodeop_desc vop_mmap_desc;
static __inline int VOP_MMAP __P((struct vnode *, int, struct ucred *, 
    struct proc *));
static __inline int VOP_MMAP(vp, fflags, cred, p)
	struct vnode *vp;
	int fflags;
	struct ucred *cred;
	struct proc *p;
{
	struct vop_mmap_args a;
	a.a_desc = VDESC(vop_mmap);
	a.a_vp = vp;
	a.a_fflags = fflags;
	a.a_cred = cred;
	a.a_p = p;
	return (VCALL(vp, VOFFSET(vop_mmap), &a));
}

struct vop_fsync_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct ucred *a_cred;
	int a_waitfor;
	struct proc *a_p;
};
extern struct vnodeop_desc vop_fsync_desc;
static __inline int VOP_FSYNC __P((struct vnode *, struct ucred *, int, 
    struct proc *));
static __inline int VOP_FSYNC(vp, cred, waitfor, p)
	struct vnode *vp;
	struct ucred *cred;
	int waitfor;
	struct proc *p;
{
	struct vop_fsync_args a;
	a.a_desc = VDESC(vop_fsync);
	a.a_vp = vp;
	a.a_cred = cred;
	a.a_waitfor = waitfor;
	a.a_p = p;
	return (VCALL(vp, VOFFSET(vop_fsync), &a));
}

struct vop_seek_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	off_t a_oldoff;
	off_t a_newoff;
	struct ucred *a_cred;
};
extern struct vnodeop_desc vop_seek_desc;
static __inline int VOP_SEEK __P((struct vnode *, off_t, off_t, 
    struct ucred *));
static __inline int VOP_SEEK(vp, oldoff, newoff, cred)
	struct vnode *vp;
	off_t oldoff;
	off_t newoff;
	struct ucred *cred;
{
	struct vop_seek_args a;
	a.a_desc = VDESC(vop_seek);
	a.a_vp = vp;
	a.a_oldoff = oldoff;
	a.a_newoff = newoff;
	a.a_cred = cred;
	return (VCALL(vp, VOFFSET(vop_seek), &a));
}

struct vop_remove_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct vnode *a_vp;
	struct componentname *a_cnp;
};
extern struct vnodeop_desc vop_remove_desc;
static __inline int VOP_REMOVE __P((struct vnode *, struct vnode *, 
    struct componentname *));
static __inline int VOP_REMOVE(dvp, vp, cnp)
	struct vnode *dvp;
	struct vnode *vp;
	struct componentname *cnp;
{
	struct vop_remove_args a;
	a.a_desc = VDESC(vop_remove);
	a.a_dvp = dvp;
	a.a_vp = vp;
	a.a_cnp = cnp;
	return (VCALL(dvp, VOFFSET(vop_remove), &a));
}

struct vop_link_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct vnode *a_tdvp;
	struct componentname *a_cnp;
};
extern struct vnodeop_desc vop_link_desc;
static __inline int VOP_LINK __P((struct vnode *, struct vnode *, 
    struct componentname *));
static __inline int VOP_LINK(vp, tdvp, cnp)
	struct vnode *vp;
	struct vnode *tdvp;
	struct componentname *cnp;
{
	struct vop_link_args a;
	a.a_desc = VDESC(vop_link);
	a.a_vp = vp;
	a.a_tdvp = tdvp;
	a.a_cnp = cnp;
	return (VCALL(vp, VOFFSET(vop_link), &a));
}

struct vop_rename_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_fdvp;
	struct vnode *a_fvp;
	struct componentname *a_fcnp;
	struct vnode *a_tdvp;
	struct vnode *a_tvp;
	struct componentname *a_tcnp;
};
extern struct vnodeop_desc vop_rename_desc;
static __inline int VOP_RENAME __P((struct vnode *, struct vnode *, 
    struct componentname *, struct vnode *, struct vnode *, 
    struct componentname *));
static __inline int VOP_RENAME(fdvp, fvp, fcnp, tdvp, tvp, tcnp)
	struct vnode *fdvp;
	struct vnode *fvp;
	struct componentname *fcnp;
	struct vnode *tdvp;
	struct vnode *tvp;
	struct componentname *tcnp;
{
	struct vop_rename_args a;
	a.a_desc = VDESC(vop_rename);
	a.a_fdvp = fdvp;
	a.a_fvp = fvp;
	a.a_fcnp = fcnp;
	a.a_tdvp = tdvp;
	a.a_tvp = tvp;
	a.a_tcnp = tcnp;
	return (VCALL(fdvp, VOFFSET(vop_rename), &a));
}

struct vop_mkdir_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct vnode **a_vpp;
	struct componentname *a_cnp;
	struct vattr *a_vap;
};
extern struct vnodeop_desc vop_mkdir_desc;
static __inline int VOP_MKDIR __P((struct vnode *, struct vnode **, 
    struct componentname *, struct vattr *));
static __inline int VOP_MKDIR(dvp, vpp, cnp, vap)
	struct vnode *dvp;
	struct vnode **vpp;
	struct componentname *cnp;
	struct vattr *vap;
{
	struct vop_mkdir_args a;
	a.a_desc = VDESC(vop_mkdir);
	a.a_dvp = dvp;
	a.a_vpp = vpp;
	a.a_cnp = cnp;
	a.a_vap = vap;
	return (VCALL(dvp, VOFFSET(vop_mkdir), &a));
}

struct vop_rmdir_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct vnode *a_vp;
	struct componentname *a_cnp;
};
extern struct vnodeop_desc vop_rmdir_desc;
static __inline int VOP_RMDIR __P((struct vnode *, struct vnode *, 
    struct componentname *));
static __inline int VOP_RMDIR(dvp, vp, cnp)
	struct vnode *dvp;
	struct vnode *vp;
	struct componentname *cnp;
{
	struct vop_rmdir_args a;
	a.a_desc = VDESC(vop_rmdir);
	a.a_dvp = dvp;
	a.a_vp = vp;
	a.a_cnp = cnp;
	return (VCALL(dvp, VOFFSET(vop_rmdir), &a));
}

struct vop_symlink_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct vnode **a_vpp;
	struct componentname *a_cnp;
	struct vattr *a_vap;
	char *a_target;
};
extern struct vnodeop_desc vop_symlink_desc;
static __inline int VOP_SYMLINK __P((struct vnode *, struct vnode **, 
    struct componentname *, struct vattr *, char *));
static __inline int VOP_SYMLINK(dvp, vpp, cnp, vap, target)
	struct vnode *dvp;
	struct vnode **vpp;
	struct componentname *cnp;
	struct vattr *vap;
	char *target;
{
	struct vop_symlink_args a;
	a.a_desc = VDESC(vop_symlink);
	a.a_dvp = dvp;
	a.a_vpp = vpp;
	a.a_cnp = cnp;
	a.a_vap = vap;
	a.a_target = target;
	return (VCALL(dvp, VOFFSET(vop_symlink), &a));
}

struct vop_readdir_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct uio *a_uio;
	struct ucred *a_cred;
	int *a_eofflag;
	u_long *a_cookies;
	int a_ncookies;
};
extern struct vnodeop_desc vop_readdir_desc;
static __inline int VOP_READDIR __P((struct vnode *, struct uio *, 
    struct ucred *, int *, u_long *, int));
static __inline int VOP_READDIR(vp, uio, cred, eofflag, cookies, ncookies)
	struct vnode *vp;
	struct uio *uio;
	struct ucred *cred;
	int *eofflag;
	u_long *cookies;
	int ncookies;
{
	struct vop_readdir_args a;
	a.a_desc = VDESC(vop_readdir);
	a.a_vp = vp;
	a.a_uio = uio;
	a.a_cred = cred;
	a.a_eofflag = eofflag;
	a.a_cookies = cookies;
	a.a_ncookies = ncookies;
	return (VCALL(vp, VOFFSET(vop_readdir), &a));
}

struct vop_readlink_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct uio *a_uio;
	struct ucred *a_cred;
};
extern struct vnodeop_desc vop_readlink_desc;
static __inline int VOP_READLINK __P((struct vnode *, struct uio *, 
    struct ucred *));
static __inline int VOP_READLINK(vp, uio, cred)
	struct vnode *vp;
	struct uio *uio;
	struct ucred *cred;
{
	struct vop_readlink_args a;
	a.a_desc = VDESC(vop_readlink);
	a.a_vp = vp;
	a.a_uio = uio;
	a.a_cred = cred;
	return (VCALL(vp, VOFFSET(vop_readlink), &a));
}

struct vop_abortop_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct componentname *a_cnp;
};
extern struct vnodeop_desc vop_abortop_desc;
static __inline int VOP_ABORTOP __P((struct vnode *, struct componentname *));
static __inline int VOP_ABORTOP(dvp, cnp)
	struct vnode *dvp;
	struct componentname *cnp;
{
	struct vop_abortop_args a;
	a.a_desc = VDESC(vop_abortop);
	a.a_dvp = dvp;
	a.a_cnp = cnp;
	return (VCALL(dvp, VOFFSET(vop_abortop), &a));
}

struct vop_inactive_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
};
extern struct vnodeop_desc vop_inactive_desc;
static __inline int VOP_INACTIVE __P((struct vnode *));
static __inline int VOP_INACTIVE(vp)
	struct vnode *vp;
{
	struct vop_inactive_args a;
	a.a_desc = VDESC(vop_inactive);
	a.a_vp = vp;
	return (VCALL(vp, VOFFSET(vop_inactive), &a));
}

struct vop_reclaim_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
};
extern struct vnodeop_desc vop_reclaim_desc;
static __inline int VOP_RECLAIM __P((struct vnode *));
static __inline int VOP_RECLAIM(vp)
	struct vnode *vp;
{
	struct vop_reclaim_args a;
	a.a_desc = VDESC(vop_reclaim);
	a.a_vp = vp;
	return (VCALL(vp, VOFFSET(vop_reclaim), &a));
}

struct vop_lock_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
};
extern struct vnodeop_desc vop_lock_desc;
static __inline int VOP_LOCK __P((struct vnode *));
static __inline int VOP_LOCK(vp)
	struct vnode *vp;
{
	struct vop_lock_args a;
	a.a_desc = VDESC(vop_lock);
	a.a_vp = vp;
	return (VCALL(vp, VOFFSET(vop_lock), &a));
}

struct vop_unlock_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
};
extern struct vnodeop_desc vop_unlock_desc;
static __inline int VOP_UNLOCK __P((struct vnode *));
static __inline int VOP_UNLOCK(vp)
	struct vnode *vp;
{
	struct vop_unlock_args a;
	a.a_desc = VDESC(vop_unlock);
	a.a_vp = vp;
	return (VCALL(vp, VOFFSET(vop_unlock), &a));
}

struct vop_bmap_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	daddr_t a_bn;
	struct vnode **a_vpp;
	daddr_t *a_bnp;
	int *a_runp;
};
extern struct vnodeop_desc vop_bmap_desc;
static __inline int VOP_BMAP __P((struct vnode *, daddr_t, struct vnode **, 
    daddr_t *, int *));
static __inline int VOP_BMAP(vp, bn, vpp, bnp, runp)
	struct vnode *vp;
	daddr_t bn;
	struct vnode **vpp;
	daddr_t *bnp;
	int *runp;
{
	struct vop_bmap_args a;
	a.a_desc = VDESC(vop_bmap);
	a.a_vp = vp;
	a.a_bn = bn;
	a.a_vpp = vpp;
	a.a_bnp = bnp;
	a.a_runp = runp;
	return (VCALL(vp, VOFFSET(vop_bmap), &a));
}

struct vop_print_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
};
extern struct vnodeop_desc vop_print_desc;
static __inline int VOP_PRINT __P((struct vnode *));
static __inline int VOP_PRINT(vp)
	struct vnode *vp;
{
	struct vop_print_args a;
	a.a_desc = VDESC(vop_print);
	a.a_vp = vp;
	return (VCALL(vp, VOFFSET(vop_print), &a));
}

struct vop_islocked_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
};
extern struct vnodeop_desc vop_islocked_desc;
static __inline int VOP_ISLOCKED __P((struct vnode *));
static __inline int VOP_ISLOCKED(vp)
	struct vnode *vp;
{
	struct vop_islocked_args a;
	a.a_desc = VDESC(vop_islocked);
	a.a_vp = vp;
	return (VCALL(vp, VOFFSET(vop_islocked), &a));
}

struct vop_pathconf_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	int a_name;
	register_t *a_retval;
};
extern struct vnodeop_desc vop_pathconf_desc;
static __inline int VOP_PATHCONF __P((struct vnode *, int, register_t *));
static __inline int VOP_PATHCONF(vp, name, retval)
	struct vnode *vp;
	int name;
	register_t *retval;
{
	struct vop_pathconf_args a;
	a.a_desc = VDESC(vop_pathconf);
	a.a_vp = vp;
	a.a_name = name;
	a.a_retval = retval;
	return (VCALL(vp, VOFFSET(vop_pathconf), &a));
}

struct vop_advlock_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	caddr_t a_id;
	int a_op;
	struct flock *a_fl;
	int a_flags;
};
extern struct vnodeop_desc vop_advlock_desc;
static __inline int VOP_ADVLOCK __P((struct vnode *, caddr_t, int, 
    struct flock *, int));
static __inline int VOP_ADVLOCK(vp, id, op, fl, flags)
	struct vnode *vp;
	caddr_t id;
	int op;
	struct flock *fl;
	int flags;
{
	struct vop_advlock_args a;
	a.a_desc = VDESC(vop_advlock);
	a.a_vp = vp;
	a.a_id = id;
	a.a_op = op;
	a.a_fl = fl;
	a.a_flags = flags;
	return (VCALL(vp, VOFFSET(vop_advlock), &a));
}

struct vop_blkatoff_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	off_t a_offset;
	char **a_res;
	struct buf **a_bpp;
};
extern struct vnodeop_desc vop_blkatoff_desc;
static __inline int VOP_BLKATOFF __P((struct vnode *, off_t, char **, 
    struct buf **));
static __inline int VOP_BLKATOFF(vp, offset, res, bpp)
	struct vnode *vp;
	off_t offset;
	char **res;
	struct buf **bpp;
{
	struct vop_blkatoff_args a;
	a.a_desc = VDESC(vop_blkatoff);
	a.a_vp = vp;
	a.a_offset = offset;
	a.a_res = res;
	a.a_bpp = bpp;
	return (VCALL(vp, VOFFSET(vop_blkatoff), &a));
}

struct vop_valloc_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_pvp;
	int a_mode;
	struct ucred *a_cred;
	struct vnode **a_vpp;
};
extern struct vnodeop_desc vop_valloc_desc;
static __inline int VOP_VALLOC __P((struct vnode *, int, struct ucred *, 
    struct vnode **));
static __inline int VOP_VALLOC(pvp, mode, cred, vpp)
	struct vnode *pvp;
	int mode;
	struct ucred *cred;
	struct vnode **vpp;
{
	struct vop_valloc_args a;
	a.a_desc = VDESC(vop_valloc);
	a.a_pvp = pvp;
	a.a_mode = mode;
	a.a_cred = cred;
	a.a_vpp = vpp;
	return (VCALL(pvp, VOFFSET(vop_valloc), &a));
}

struct vop_reallocblks_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct cluster_save *a_buflist;
};
extern struct vnodeop_desc vop_reallocblks_desc;
static __inline int VOP_REALLOCBLKS __P((struct vnode *, 
    struct cluster_save *));
static __inline int VOP_REALLOCBLKS(vp, buflist)
	struct vnode *vp;
	struct cluster_save *buflist;
{
	struct vop_reallocblks_args a;
	a.a_desc = VDESC(vop_reallocblks);
	a.a_vp = vp;
	a.a_buflist = buflist;
	return (VCALL(vp, VOFFSET(vop_reallocblks), &a));
}

struct vop_vfree_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_pvp;
	ino_t a_ino;
	int a_mode;
};
extern struct vnodeop_desc vop_vfree_desc;
static __inline int VOP_VFREE __P((struct vnode *, ino_t, int));
static __inline int VOP_VFREE(pvp, ino, mode)
	struct vnode *pvp;
	ino_t ino;
	int mode;
{
	struct vop_vfree_args a;
	a.a_desc = VDESC(vop_vfree);
	a.a_pvp = pvp;
	a.a_ino = ino;
	a.a_mode = mode;
	return (VCALL(pvp, VOFFSET(vop_vfree), &a));
}

struct vop_truncate_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	off_t a_length;
	int a_flags;
	struct ucred *a_cred;
	struct proc *a_p;
};
extern struct vnodeop_desc vop_truncate_desc;
static __inline int VOP_TRUNCATE __P((struct vnode *, off_t, int, 
    struct ucred *, struct proc *));
static __inline int VOP_TRUNCATE(vp, length, flags, cred, p)
	struct vnode *vp;
	off_t length;
	int flags;
	struct ucred *cred;
	struct proc *p;
{
	struct vop_truncate_args a;
	a.a_desc = VDESC(vop_truncate);
	a.a_vp = vp;
	a.a_length = length;
	a.a_flags = flags;
	a.a_cred = cred;
	a.a_p = p;
	return (VCALL(vp, VOFFSET(vop_truncate), &a));
}

struct vop_update_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct timeval *a_access;
	struct timeval *a_modify;
	int a_waitfor;
};
extern struct vnodeop_desc vop_update_desc;
static __inline int VOP_UPDATE __P((struct vnode *, struct timeval *, 
    struct timeval *, int));
static __inline int VOP_UPDATE(vp, access, modify, waitfor)
	struct vnode *vp;
	struct timeval *access;
	struct timeval *modify;
	int waitfor;
{
	struct vop_update_args a;
	a.a_desc = VDESC(vop_update);
	a.a_vp = vp;
	a.a_access = access;
	a.a_modify = modify;
	a.a_waitfor = waitfor;
	return (VCALL(vp, VOFFSET(vop_update), &a));
}

struct vop_lease_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct proc *a_p;
	struct ucred *a_cred;
	int a_flag;
};
extern struct vnodeop_desc vop_lease_desc;
static __inline int VOP_LEASE __P((struct vnode *, struct proc *, 
    struct ucred *, int));
static __inline int VOP_LEASE(vp, p, cred, flag)
	struct vnode *vp;
	struct proc *p;
	struct ucred *cred;
	int flag;
{
	struct vop_lease_args a;
	a.a_desc = VDESC(vop_lease);
	a.a_vp = vp;
	a.a_p = p;
	a.a_cred = cred;
	a.a_flag = flag;
	return (VCALL(vp, VOFFSET(vop_lease), &a));
}

struct vop_whiteout_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct componentname *a_cnp;
	int a_flags;
};
extern struct vnodeop_desc vop_whiteout_desc;
static __inline int VOP_WHITEOUT __P((struct vnode *, 
    struct componentname *, int));
static __inline int VOP_WHITEOUT(dvp, cnp, flags)
	struct vnode *dvp;
	struct componentname *cnp;
	int flags;
{
	struct vop_whiteout_args a;
	a.a_desc = VDESC(vop_whiteout);
	a.a_dvp = dvp;
	a.a_cnp = cnp;
	a.a_flags = flags;
	return (VCALL(dvp, VOFFSET(vop_whiteout), &a));
}

struct vop_whiteout_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct componentname *a_cnp;
	int a_flags;
	 a_;
	 a_;
	braam+@pachelbel.coda.cs.cmu.edu Wed Oct 9 18:08:18 a_1996;
	braam+@pachelbel.coda.cs.cmu.edu a_;
	from pachelbel.coda.cs.cmu.edu (PACHELBEL.CODA.CS.CMU.EDU [128.2.222.26]) by telemann.coda.cs.cmu.edu (8.7.4/8.7.3) with SMTP id SAA09451 for <braam@telemann.coda.cs.cmu.edu> Wed, 9 Oct 1996 18:08:17 a_-0400;
	braam+@pachelbel.coda.cs.cmu.edu a_;
	<199610092208.SAA09451@telemann.coda.cs.cmu.edu> a_;
	braam@pachelbel.coda.cs.cmu.edu a_;
	Wed, 9 Oct 96 18:07:56 a_EDT;
	braam+@pachelbel.coda.cs.cmu.edu a_;
	 a_;
	 a_;
	 a_;
	Copyright (c) 1992, a_1993;
	The Regents of the University of California. All rights a_reserved.;
	 a_;
	Redistribution and use in source and binary forms, with or a_without;
	modification, are permitted provided that the following a_conditions;
	are a_met:;
	1. Redistributions of source code must retain the above a_copyright;
	notice, this list of conditions and the following a_disclaimer.;
	2. Redistributions in binary form must reproduce the above a_copyright;
	notice, this list of conditions and the following disclaimer in a_the;
	documentation and/or other materials provided with the a_distribution.;
	3. All advertising materials mentioning features or use of this a_software;
	must display the following a_acknowledgement:;
	This product includes software developed by the University a_of;
	California, Berkeley and its a_contributors.;
	4. Neither the name of the University nor the names of its a_contributors;
	may be used to endorse or promote products derived from this a_software;
	without specific prior written a_permission.;
	 a_;
	THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' a_AND;
	ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, a_THE;
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR a_PURPOSE;
	ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE a_LIABLE;
	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR a_CONSEQUENTIAL;
	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE a_GOODS;
	OR SERVICES LOSS OF USE, DATA, OR PROFITS; OR BUSINESS a_INTERRUPTION);
	HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, a_STRICT;
	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY a_WAY;
	OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY a_OF;
	SUCH a_DAMAGE.;
	 a_;
	 a_;
	vnode_if.sh,v 1.8 1995/03/10 04:13:52 chopps Exp a_$';
	 a_;
	 a_;
	[ $# -ne 1 ] a_then;
	'usage: vnode_if.sh a_srcfile';
	1 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	'BEGIN { print toupper("true") exit; }' a_2>/dev/null`;
	 a_;
	[ "$isgawk" = TRUE ] a_then;
	GNU awk provides a_it.;
	 a_;
	 a_;
	Provide our own a_toupper();
	 a_;
	toupper(str) a_{;
	= "echo "str" |tr a-z a_A-Z";
	| getline a__toupper_str;
	 a_;
	_toupper_str a_;
};
extern struct vnodeop_desc vop_whiteout_desc;
static __inline int VOP_WHITEOUT __P((struct vnode *, 
    struct componentname *, int, , , 
    braam+@pachelbel.coda.cs.cmu.edu Wed Oct 9 18:08:18, 
    braam+@pachelbel.coda.cs.cmu.edu, 
    from pachelbel.coda.cs.cmu.edu (PACHELBEL.CODA.CS.CMU.EDU [128.2.222.26]) by telemann.coda.cs.cmu.edu (8.7.4/8.7.3) with SMTP id SAA09451 for <braam@telemann.coda.cs.cmu.edu> Wed, 9 Oct 1996 18:08:17, 
    braam+@pachelbel.coda.cs.cmu.edu, 
    <199610092208.SAA09451@telemann.coda.cs.cmu.edu>, 
    braam@pachelbel.coda.cs.cmu.edu, Wed, 9 Oct 96 18:07:56, 
    braam+@pachelbel.coda.cs.cmu.edu, , , , Copyright (c) 1992,, 
    The Regents of the University of California. All rights, , 
    Redistribution and use in source and binary forms, with or, 
    modification, are permitted provided that the following, are, 
    1. Redistributions of source code must retain the above, 
    notice, this list of conditions and the following, 
    2. Redistributions in binary form must reproduce the above, 
    notice, this list of conditions and the following disclaimer in, 
    documentation and/or other materials provided with the, 
    3. All advertising materials mentioning features or use of this, 
    must display the following, 
    This product includes software developed by the University, 
    California, Berkeley and its, 
    4. Neither the name of the University nor the names of its, 
    may be used to endorse or promote products derived from this, 
    without specific prior written, , 
    THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'', 
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,, 
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR, 
    ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE, 
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR, 
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE, 
    OR SERVICES LOSS OF USE, DATA, OR PROFITS; OR BUSINESS, 
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,, 
    LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY, 
    OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY, 
    SUCH, , , vnode_if.sh,v 1.8 1995/03/10 04:13:52 chopps Exp, , , 
    [ $# -ne 1 ], 'usage: vnode_if.sh, 1, , , , , , , , , , 
    'BEGIN { print toupper("true") exit; }', , [ "$isgawk" = TRUE ], 
    GNU awk provides, , , Provide our own, , toupper(str), 
    = "echo "str" |tr a-z, | getline, , _toupper_str));
static __inline int VOP_WHITEOUT(dvp, cnp, flags, , , 1996, , -0400, , , , EDT, , , , , 1993, reserved., , without, conditions, met:, copyright, disclaimer., copyright, the, distribution., software, acknowledgement:, of, contributors., contributors, software, permission., , AND, THE, PURPOSE, LIABLE, CONSEQUENTIAL, GOODS, INTERRUPTION), STRICT, WAY, OF, DAMAGE., , , $', , , then, srcfile', , , , , , , , , , , 2>/dev/null`, , then, it., , , toupper(), , {, A-Z", _toupper_str, , )
	struct vnode *dvp;
	struct componentname *cnp;
	int flags;
	 ;
	 ;
	braam+@pachelbel.coda.cs.cmu.edu Wed Oct 9 18:08:18 1996;
	braam+@pachelbel.coda.cs.cmu.edu ;
	from pachelbel.coda.cs.cmu.edu (PACHELBEL.CODA.CS.CMU.EDU [128.2.222.26]) by telemann.coda.cs.cmu.edu (8.7.4/8.7.3) with SMTP id SAA09451 for <braam@telemann.coda.cs.cmu.edu> Wed, 9 Oct 1996 18:08:17 -0400;
	braam+@pachelbel.coda.cs.cmu.edu ;
	<199610092208.SAA09451@telemann.coda.cs.cmu.edu> ;
	braam@pachelbel.coda.cs.cmu.edu ;
	Wed, 9 Oct 96 18:07:56 EDT;
	braam+@pachelbel.coda.cs.cmu.edu ;
	 ;
	 ;
	 ;
	Copyright (c) 1992, 1993;
	The Regents of the University of California. All rights reserved.;
	 ;
	Redistribution and use in source and binary forms, with or without;
	modification, are permitted provided that the following conditions;
	are met:;
	1. Redistributions of source code must retain the above copyright;
	notice, this list of conditions and the following disclaimer.;
	2. Redistributions in binary form must reproduce the above copyright;
	notice, this list of conditions and the following disclaimer in the;
	documentation and/or other materials provided with the distribution.;
	3. All advertising materials mentioning features or use of this software;
	must display the following acknowledgement:;
	This product includes software developed by the University of;
	California, Berkeley and its contributors.;
	4. Neither the name of the University nor the names of its contributors;
	may be used to endorse or promote products derived from this software;
	without specific prior written permission.;
	 ;
	THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND;
	ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE;
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE;
	ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE;
	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL;
	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS;
	OR SERVICES LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION);
	HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT;
	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY;
	OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF;
	SUCH DAMAGE.;
	 ;
	 ;
	vnode_if.sh,v 1.8 1995/03/10 04:13:52 chopps Exp $';
	 ;
	 ;
	[ $# -ne 1 ] then;
	'usage: vnode_if.sh srcfile';
	1 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	'BEGIN { print toupper("true") exit; }' 2>/dev/null`;
	 ;
	[ "$isgawk" = TRUE ] then;
	GNU awk provides it.;
	 ;
	 ;
	Provide our own toupper();
	 ;
	toupper(str) {;
	= "echo "str" |tr a-z A-Z";
	| getline _toupper_str;
	 ;
	_toupper_str ;
{
	struct vop_whiteout_args a;
	a.a_desc = VDESC(vop_whiteout);
	a.a_dvp = dvp;
	a.a_cnp = cnp;
	a.a_flags = flags;
	a.a_ = ;
	a.a_ = ;
	a.a_1996 = 1996;
	a.a_ = ;
	a.a_-0400 = -0400;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_EDT = EDT;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_1993 = 1993;
	a.a_reserved. = reserved.;
	a.a_ = ;
	a.a_without = without;
	a.a_conditions = conditions;
	a.a_met: = met:;
	a.a_copyright = copyright;
	a.a_disclaimer. = disclaimer.;
	a.a_copyright = copyright;
	a.a_the = the;
	a.a_distribution. = distribution.;
	a.a_software = software;
	a.a_acknowledgement: = acknowledgement:;
	a.a_of = of;
	a.a_contributors. = contributors.;
	a.a_contributors = contributors;
	a.a_software = software;
	a.a_permission. = permission.;
	a.a_ = ;
	a.a_AND = AND;
	a.a_THE = THE;
	a.a_PURPOSE = PURPOSE;
	a.a_LIABLE = LIABLE;
	a.a_CONSEQUENTIAL = CONSEQUENTIAL;
	a.a_GOODS = GOODS;
	a.a_INTERRUPTION) = INTERRUPTION);
	a.a_STRICT = STRICT;
	a.a_WAY = WAY;
	a.a_OF = OF;
	a.a_DAMAGE. = DAMAGE.;
	a.a_ = ;
	a.a_ = ;
	a.a_$' = $';
	a.a_ = ;
	a.a_ = ;
	a.a_then = then;
	a.a_srcfile' = srcfile';
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_2>/dev/null` = 2>/dev/null`;
	a.a_ = ;
	a.a_then = then;
	a.a_it. = it.;
	a.a_ = ;
	a.a_ = ;
	a.a_toupper() = toupper();
	a.a_ = ;
	a.a_{ = {;
	a.a_A-Z" = A-Z";
	a.a__toupper_str = _toupper_str;
	a.a_ = ;
	a.a_ = ;
	return (VCALL(dvp, VOFFSET(vop_whiteout), &a));
}

struct vop_whiteout_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct componentname *a_cnp;
	int a_flags;
	 a_;
	 a_;
	braam+@pachelbel.coda.cs.cmu.edu Wed Oct 9 18:08:18 a_1996;
	braam+@pachelbel.coda.cs.cmu.edu a_;
	from pachelbel.coda.cs.cmu.edu (PACHELBEL.CODA.CS.CMU.EDU [128.2.222.26]) by telemann.coda.cs.cmu.edu (8.7.4/8.7.3) with SMTP id SAA09451 for <braam@telemann.coda.cs.cmu.edu> Wed, 9 Oct 1996 18:08:17 a_-0400;
	braam+@pachelbel.coda.cs.cmu.edu a_;
	<199610092208.SAA09451@telemann.coda.cs.cmu.edu> a_;
	braam@pachelbel.coda.cs.cmu.edu a_;
	Wed, 9 Oct 96 18:07:56 a_EDT;
	braam+@pachelbel.coda.cs.cmu.edu a_;
	 a_;
	 a_;
	 a_;
	Copyright (c) 1992, a_1993;
	The Regents of the University of California. All rights a_reserved.;
	 a_;
	Redistribution and use in source and binary forms, with or a_without;
	modification, are permitted provided that the following a_conditions;
	are a_met:;
	1. Redistributions of source code must retain the above a_copyright;
	notice, this list of conditions and the following a_disclaimer.;
	2. Redistributions in binary form must reproduce the above a_copyright;
	notice, this list of conditions and the following disclaimer in a_the;
	documentation and/or other materials provided with the a_distribution.;
	3. All advertising materials mentioning features or use of this a_software;
	must display the following a_acknowledgement:;
	This product includes software developed by the University a_of;
	California, Berkeley and its a_contributors.;
	4. Neither the name of the University nor the names of its a_contributors;
	may be used to endorse or promote products derived from this a_software;
	without specific prior written a_permission.;
	 a_;
	THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' a_AND;
	ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, a_THE;
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR a_PURPOSE;
	ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE a_LIABLE;
	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR a_CONSEQUENTIAL;
	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE a_GOODS;
	OR SERVICES LOSS OF USE, DATA, OR PROFITS; OR BUSINESS a_INTERRUPTION);
	HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, a_STRICT;
	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY a_WAY;
	OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY a_OF;
	SUCH a_DAMAGE.;
	 a_;
	 a_;
	vnode_if.sh,v 1.8 1995/03/10 04:13:52 chopps Exp a_$';
	 a_;
	 a_;
	[ $# -ne 1 ] a_then;
	'usage: vnode_if.sh a_srcfile';
	1 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	'BEGIN { print toupper("true") exit; }' a_2>/dev/null`;
	 a_;
	[ "$isgawk" = TRUE ] a_then;
	GNU awk provides a_it.;
	 a_;
	 a_;
	Provide our own a_toupper();
	 a_;
	toupper(str) a_{;
	= "echo "str" |tr a-z a_A-Z";
	| getline a__toupper_str;
	 a_;
	_toupper_str a_;
	 a_;
	 a_;
	\1:g a_;
	/ a_/';
	 a_;
	{ next a_};
	{ a_;
	 a_;
	 a_;
	 a_;
};
extern struct vnodeop_desc vop_whiteout_desc;
static __inline int VOP_WHITEOUT __P((struct vnode *, 
    struct componentname *, int, , , 
    braam+@pachelbel.coda.cs.cmu.edu Wed Oct 9 18:08:18, 
    braam+@pachelbel.coda.cs.cmu.edu, 
    from pachelbel.coda.cs.cmu.edu (PACHELBEL.CODA.CS.CMU.EDU [128.2.222.26]) by telemann.coda.cs.cmu.edu (8.7.4/8.7.3) with SMTP id SAA09451 for <braam@telemann.coda.cs.cmu.edu> Wed, 9 Oct 1996 18:08:17, 
    braam+@pachelbel.coda.cs.cmu.edu, 
    <199610092208.SAA09451@telemann.coda.cs.cmu.edu>, 
    braam@pachelbel.coda.cs.cmu.edu, Wed, 9 Oct 96 18:07:56, 
    braam+@pachelbel.coda.cs.cmu.edu, , , , Copyright (c) 1992,, 
    The Regents of the University of California. All rights, , 
    Redistribution and use in source and binary forms, with or, 
    modification, are permitted provided that the following, are, 
    1. Redistributions of source code must retain the above, 
    notice, this list of conditions and the following, 
    2. Redistributions in binary form must reproduce the above, 
    notice, this list of conditions and the following disclaimer in, 
    documentation and/or other materials provided with the, 
    3. All advertising materials mentioning features or use of this, 
    must display the following, 
    This product includes software developed by the University, 
    California, Berkeley and its, 
    4. Neither the name of the University nor the names of its, 
    may be used to endorse or promote products derived from this, 
    without specific prior written, , 
    THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'', 
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,, 
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR, 
    ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE, 
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR, 
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE, 
    OR SERVICES LOSS OF USE, DATA, OR PROFITS; OR BUSINESS, 
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,, 
    LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY, 
    OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY, 
    SUCH, , , vnode_if.sh,v 1.8 1995/03/10 04:13:52 chopps Exp, , , 
    [ $# -ne 1 ], 'usage: vnode_if.sh, 1, , , , , , , , , , 
    'BEGIN { print toupper("true") exit; }', , [ "$isgawk" = TRUE ], 
    GNU awk provides, , , Provide our own, , toupper(str), 
    = "echo "str" |tr a-z, | getline, , _toupper_str, , , \1:g, /, , { next, 
    {, , , ));
static __inline int VOP_WHITEOUT(dvp, cnp, flags, , , 1996, , -0400, , , , EDT, , , , , 1993, reserved., , without, conditions, met:, copyright, disclaimer., copyright, the, distribution., software, acknowledgement:, of, contributors., contributors, software, permission., , AND, THE, PURPOSE, LIABLE, CONSEQUENTIAL, GOODS, INTERRUPTION), STRICT, WAY, OF, DAMAGE., , , $', , , then, srcfile', , , , , , , , , , , 2>/dev/null`, , then, it., , , toupper(), , {, A-Z", _toupper_str, , , , , , /', , }, , , , )
	struct vnode *dvp;
	struct componentname *cnp;
	int flags;
	 ;
	 ;
	braam+@pachelbel.coda.cs.cmu.edu Wed Oct 9 18:08:18 1996;
	braam+@pachelbel.coda.cs.cmu.edu ;
	from pachelbel.coda.cs.cmu.edu (PACHELBEL.CODA.CS.CMU.EDU [128.2.222.26]) by telemann.coda.cs.cmu.edu (8.7.4/8.7.3) with SMTP id SAA09451 for <braam@telemann.coda.cs.cmu.edu> Wed, 9 Oct 1996 18:08:17 -0400;
	braam+@pachelbel.coda.cs.cmu.edu ;
	<199610092208.SAA09451@telemann.coda.cs.cmu.edu> ;
	braam@pachelbel.coda.cs.cmu.edu ;
	Wed, 9 Oct 96 18:07:56 EDT;
	braam+@pachelbel.coda.cs.cmu.edu ;
	 ;
	 ;
	 ;
	Copyright (c) 1992, 1993;
	The Regents of the University of California. All rights reserved.;
	 ;
	Redistribution and use in source and binary forms, with or without;
	modification, are permitted provided that the following conditions;
	are met:;
	1. Redistributions of source code must retain the above copyright;
	notice, this list of conditions and the following disclaimer.;
	2. Redistributions in binary form must reproduce the above copyright;
	notice, this list of conditions and the following disclaimer in the;
	documentation and/or other materials provided with the distribution.;
	3. All advertising materials mentioning features or use of this software;
	must display the following acknowledgement:;
	This product includes software developed by the University of;
	California, Berkeley and its contributors.;
	4. Neither the name of the University nor the names of its contributors;
	may be used to endorse or promote products derived from this software;
	without specific prior written permission.;
	 ;
	THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND;
	ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE;
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE;
	ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE;
	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL;
	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS;
	OR SERVICES LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION);
	HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT;
	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY;
	OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF;
	SUCH DAMAGE.;
	 ;
	 ;
	vnode_if.sh,v 1.8 1995/03/10 04:13:52 chopps Exp $';
	 ;
	 ;
	[ $# -ne 1 ] then;
	'usage: vnode_if.sh srcfile';
	1 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	'BEGIN { print toupper("true") exit; }' 2>/dev/null`;
	 ;
	[ "$isgawk" = TRUE ] then;
	GNU awk provides it.;
	 ;
	 ;
	Provide our own toupper();
	 ;
	toupper(str) {;
	= "echo "str" |tr a-z A-Z";
	| getline _toupper_str;
	 ;
	_toupper_str ;
	 ;
	 ;
	\1:g ;
	/ /';
	 ;
	{ next };
	{ ;
	 ;
	 ;
	 ;
{
	struct vop_whiteout_args a;
	a.a_desc = VDESC(vop_whiteout);
	a.a_dvp = dvp;
	a.a_cnp = cnp;
	a.a_flags = flags;
	a.a_ = ;
	a.a_ = ;
	a.a_1996 = 1996;
	a.a_ = ;
	a.a_-0400 = -0400;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_EDT = EDT;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_1993 = 1993;
	a.a_reserved. = reserved.;
	a.a_ = ;
	a.a_without = without;
	a.a_conditions = conditions;
	a.a_met: = met:;
	a.a_copyright = copyright;
	a.a_disclaimer. = disclaimer.;
	a.a_copyright = copyright;
	a.a_the = the;
	a.a_distribution. = distribution.;
	a.a_software = software;
	a.a_acknowledgement: = acknowledgement:;
	a.a_of = of;
	a.a_contributors. = contributors.;
	a.a_contributors = contributors;
	a.a_software = software;
	a.a_permission. = permission.;
	a.a_ = ;
	a.a_AND = AND;
	a.a_THE = THE;
	a.a_PURPOSE = PURPOSE;
	a.a_LIABLE = LIABLE;
	a.a_CONSEQUENTIAL = CONSEQUENTIAL;
	a.a_GOODS = GOODS;
	a.a_INTERRUPTION) = INTERRUPTION);
	a.a_STRICT = STRICT;
	a.a_WAY = WAY;
	a.a_OF = OF;
	a.a_DAMAGE. = DAMAGE.;
	a.a_ = ;
	a.a_ = ;
	a.a_$' = $';
	a.a_ = ;
	a.a_ = ;
	a.a_then = then;
	a.a_srcfile' = srcfile';
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_2>/dev/null` = 2>/dev/null`;
	a.a_ = ;
	a.a_then = then;
	a.a_it. = it.;
	a.a_ = ;
	a.a_ = ;
	a.a_toupper() = toupper();
	a.a_ = ;
	a.a_{ = {;
	a.a_A-Z" = A-Z";
	a.a__toupper_str = _toupper_str;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_/' = /';
	a.a_ = ;
	a.a_} = };
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	return (VCALL(dvp, VOFFSET(vop_whiteout), &a));
}

struct vop_whiteout_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct componentname *a_cnp;
	int a_flags;
	 a_;
	 a_;
	braam+@pachelbel.coda.cs.cmu.edu Wed Oct 9 18:08:18 a_1996;
	braam+@pachelbel.coda.cs.cmu.edu a_;
	from pachelbel.coda.cs.cmu.edu (PACHELBEL.CODA.CS.CMU.EDU [128.2.222.26]) by telemann.coda.cs.cmu.edu (8.7.4/8.7.3) with SMTP id SAA09451 for <braam@telemann.coda.cs.cmu.edu> Wed, 9 Oct 1996 18:08:17 a_-0400;
	braam+@pachelbel.coda.cs.cmu.edu a_;
	<199610092208.SAA09451@telemann.coda.cs.cmu.edu> a_;
	braam@pachelbel.coda.cs.cmu.edu a_;
	Wed, 9 Oct 96 18:07:56 a_EDT;
	braam+@pachelbel.coda.cs.cmu.edu a_;
	 a_;
	 a_;
	 a_;
	Copyright (c) 1992, a_1993;
	The Regents of the University of California. All rights a_reserved.;
	 a_;
	Redistribution and use in source and binary forms, with or a_without;
	modification, are permitted provided that the following a_conditions;
	are a_met:;
	1. Redistributions of source code must retain the above a_copyright;
	notice, this list of conditions and the following a_disclaimer.;
	2. Redistributions in binary form must reproduce the above a_copyright;
	notice, this list of conditions and the following disclaimer in a_the;
	documentation and/or other materials provided with the a_distribution.;
	3. All advertising materials mentioning features or use of this a_software;
	must display the following a_acknowledgement:;
	This product includes software developed by the University a_of;
	California, Berkeley and its a_contributors.;
	4. Neither the name of the University nor the names of its a_contributors;
	may be used to endorse or promote products derived from this a_software;
	without specific prior written a_permission.;
	 a_;
	THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' a_AND;
	ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, a_THE;
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR a_PURPOSE;
	ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE a_LIABLE;
	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR a_CONSEQUENTIAL;
	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE a_GOODS;
	OR SERVICES LOSS OF USE, DATA, OR PROFITS; OR BUSINESS a_INTERRUPTION);
	HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, a_STRICT;
	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY a_WAY;
	OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY a_OF;
	SUCH a_DAMAGE.;
	 a_;
	 a_;
	vnode_if.sh,v 1.8 1995/03/10 04:13:52 chopps Exp a_$';
	 a_;
	 a_;
	[ $# -ne 1 ] a_then;
	'usage: vnode_if.sh a_srcfile';
	1 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	'BEGIN { print toupper("true") exit; }' a_2>/dev/null`;
	 a_;
	[ "$isgawk" = TRUE ] a_then;
	GNU awk provides a_it.;
	 a_;
	 a_;
	Provide our own a_toupper();
	 a_;
	toupper(str) a_{;
	= "echo "str" |tr a-z a_A-Z";
	| getline a__toupper_str;
	 a_;
	_toupper_str a_;
	 a_;
	 a_;
	\1:g a_;
	/ a_/';
	 a_;
	{ next a_};
	{ a_;
	 a_;
	 a_;
	 a_;
	{ a_;
	 a_;
	 a_;
};
extern struct vnodeop_desc vop_whiteout_desc;
static __inline int VOP_WHITEOUT __P((struct vnode *, 
    struct componentname *, int, , , 
    braam+@pachelbel.coda.cs.cmu.edu Wed Oct 9 18:08:18, 
    braam+@pachelbel.coda.cs.cmu.edu, 
    from pachelbel.coda.cs.cmu.edu (PACHELBEL.CODA.CS.CMU.EDU [128.2.222.26]) by telemann.coda.cs.cmu.edu (8.7.4/8.7.3) with SMTP id SAA09451 for <braam@telemann.coda.cs.cmu.edu> Wed, 9 Oct 1996 18:08:17, 
    braam+@pachelbel.coda.cs.cmu.edu, 
    <199610092208.SAA09451@telemann.coda.cs.cmu.edu>, 
    braam@pachelbel.coda.cs.cmu.edu, Wed, 9 Oct 96 18:07:56, 
    braam+@pachelbel.coda.cs.cmu.edu, , , , Copyright (c) 1992,, 
    The Regents of the University of California. All rights, , 
    Redistribution and use in source and binary forms, with or, 
    modification, are permitted provided that the following, are, 
    1. Redistributions of source code must retain the above, 
    notice, this list of conditions and the following, 
    2. Redistributions in binary form must reproduce the above, 
    notice, this list of conditions and the following disclaimer in, 
    documentation and/or other materials provided with the, 
    3. All advertising materials mentioning features or use of this, 
    must display the following, 
    This product includes software developed by the University, 
    California, Berkeley and its, 
    4. Neither the name of the University nor the names of its, 
    may be used to endorse or promote products derived from this, 
    without specific prior written, , 
    THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'', 
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,, 
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR, 
    ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE, 
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR, 
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE, 
    OR SERVICES LOSS OF USE, DATA, OR PROFITS; OR BUSINESS, 
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,, 
    LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY, 
    OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY, 
    SUCH, , , vnode_if.sh,v 1.8 1995/03/10 04:13:52 chopps Exp, , , 
    [ $# -ne 1 ], 'usage: vnode_if.sh, 1, , , , , , , , , , 
    'BEGIN { print toupper("true") exit; }', , [ "$isgawk" = TRUE ], 
    GNU awk provides, , , Provide our own, , toupper(str), 
    = "echo "str" |tr a-z, | getline, , _toupper_str, , , \1:g, /, , { next, 
    {, , , , {, , ));
static __inline int VOP_WHITEOUT(dvp, cnp, flags, , , 1996, , -0400, , , , EDT, , , , , 1993, reserved., , without, conditions, met:, copyright, disclaimer., copyright, the, distribution., software, acknowledgement:, of, contributors., contributors, software, permission., , AND, THE, PURPOSE, LIABLE, CONSEQUENTIAL, GOODS, INTERRUPTION), STRICT, WAY, OF, DAMAGE., , , $', , , then, srcfile', , , , , , , , , , , 2>/dev/null`, , then, it., , , toupper(), , {, A-Z", _toupper_str, , , , , , /', , }, , , , , , , )
	struct vnode *dvp;
	struct componentname *cnp;
	int flags;
	 ;
	 ;
	braam+@pachelbel.coda.cs.cmu.edu Wed Oct 9 18:08:18 1996;
	braam+@pachelbel.coda.cs.cmu.edu ;
	from pachelbel.coda.cs.cmu.edu (PACHELBEL.CODA.CS.CMU.EDU [128.2.222.26]) by telemann.coda.cs.cmu.edu (8.7.4/8.7.3) with SMTP id SAA09451 for <braam@telemann.coda.cs.cmu.edu> Wed, 9 Oct 1996 18:08:17 -0400;
	braam+@pachelbel.coda.cs.cmu.edu ;
	<199610092208.SAA09451@telemann.coda.cs.cmu.edu> ;
	braam@pachelbel.coda.cs.cmu.edu ;
	Wed, 9 Oct 96 18:07:56 EDT;
	braam+@pachelbel.coda.cs.cmu.edu ;
	 ;
	 ;
	 ;
	Copyright (c) 1992, 1993;
	The Regents of the University of California. All rights reserved.;
	 ;
	Redistribution and use in source and binary forms, with or without;
	modification, are permitted provided that the following conditions;
	are met:;
	1. Redistributions of source code must retain the above copyright;
	notice, this list of conditions and the following disclaimer.;
	2. Redistributions in binary form must reproduce the above copyright;
	notice, this list of conditions and the following disclaimer in the;
	documentation and/or other materials provided with the distribution.;
	3. All advertising materials mentioning features or use of this software;
	must display the following acknowledgement:;
	This product includes software developed by the University of;
	California, Berkeley and its contributors.;
	4. Neither the name of the University nor the names of its contributors;
	may be used to endorse or promote products derived from this software;
	without specific prior written permission.;
	 ;
	THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND;
	ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE;
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE;
	ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE;
	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL;
	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS;
	OR SERVICES LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION);
	HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT;
	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY;
	OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF;
	SUCH DAMAGE.;
	 ;
	 ;
	vnode_if.sh,v 1.8 1995/03/10 04:13:52 chopps Exp $';
	 ;
	 ;
	[ $# -ne 1 ] then;
	'usage: vnode_if.sh srcfile';
	1 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	'BEGIN { print toupper("true") exit; }' 2>/dev/null`;
	 ;
	[ "$isgawk" = TRUE ] then;
	GNU awk provides it.;
	 ;
	 ;
	Provide our own toupper();
	 ;
	toupper(str) {;
	= "echo "str" |tr a-z A-Z";
	| getline _toupper_str;
	 ;
	_toupper_str ;
	 ;
	 ;
	\1:g ;
	/ /';
	 ;
	{ next };
	{ ;
	 ;
	 ;
	 ;
	{ ;
	 ;
	 ;
{
	struct vop_whiteout_args a;
	a.a_desc = VDESC(vop_whiteout);
	a.a_dvp = dvp;
	a.a_cnp = cnp;
	a.a_flags = flags;
	a.a_ = ;
	a.a_ = ;
	a.a_1996 = 1996;
	a.a_ = ;
	a.a_-0400 = -0400;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_EDT = EDT;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_1993 = 1993;
	a.a_reserved. = reserved.;
	a.a_ = ;
	a.a_without = without;
	a.a_conditions = conditions;
	a.a_met: = met:;
	a.a_copyright = copyright;
	a.a_disclaimer. = disclaimer.;
	a.a_copyright = copyright;
	a.a_the = the;
	a.a_distribution. = distribution.;
	a.a_software = software;
	a.a_acknowledgement: = acknowledgement:;
	a.a_of = of;
	a.a_contributors. = contributors.;
	a.a_contributors = contributors;
	a.a_software = software;
	a.a_permission. = permission.;
	a.a_ = ;
	a.a_AND = AND;
	a.a_THE = THE;
	a.a_PURPOSE = PURPOSE;
	a.a_LIABLE = LIABLE;
	a.a_CONSEQUENTIAL = CONSEQUENTIAL;
	a.a_GOODS = GOODS;
	a.a_INTERRUPTION) = INTERRUPTION);
	a.a_STRICT = STRICT;
	a.a_WAY = WAY;
	a.a_OF = OF;
	a.a_DAMAGE. = DAMAGE.;
	a.a_ = ;
	a.a_ = ;
	a.a_$' = $';
	a.a_ = ;
	a.a_ = ;
	a.a_then = then;
	a.a_srcfile' = srcfile';
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_2>/dev/null` = 2>/dev/null`;
	a.a_ = ;
	a.a_then = then;
	a.a_it. = it.;
	a.a_ = ;
	a.a_ = ;
	a.a_toupper() = toupper();
	a.a_ = ;
	a.a_{ = {;
	a.a_A-Z" = A-Z";
	a.a__toupper_str = _toupper_str;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_/' = /';
	a.a_ = ;
	a.a_} = };
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	return (VCALL(dvp, VOFFSET(vop_whiteout), &a));
}

struct vop_whiteout_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct componentname *a_cnp;
	int a_flags;
	 a_;
	 a_;
	braam+@pachelbel.coda.cs.cmu.edu Wed Oct 9 18:08:18 a_1996;
	braam+@pachelbel.coda.cs.cmu.edu a_;
	from pachelbel.coda.cs.cmu.edu (PACHELBEL.CODA.CS.CMU.EDU [128.2.222.26]) by telemann.coda.cs.cmu.edu (8.7.4/8.7.3) with SMTP id SAA09451 for <braam@telemann.coda.cs.cmu.edu> Wed, 9 Oct 1996 18:08:17 a_-0400;
	braam+@pachelbel.coda.cs.cmu.edu a_;
	<199610092208.SAA09451@telemann.coda.cs.cmu.edu> a_;
	braam@pachelbel.coda.cs.cmu.edu a_;
	Wed, 9 Oct 96 18:07:56 a_EDT;
	braam+@pachelbel.coda.cs.cmu.edu a_;
	 a_;
	 a_;
	 a_;
	Copyright (c) 1992, a_1993;
	The Regents of the University of California. All rights a_reserved.;
	 a_;
	Redistribution and use in source and binary forms, with or a_without;
	modification, are permitted provided that the following a_conditions;
	are a_met:;
	1. Redistributions of source code must retain the above a_copyright;
	notice, this list of conditions and the following a_disclaimer.;
	2. Redistributions in binary form must reproduce the above a_copyright;
	notice, this list of conditions and the following disclaimer in a_the;
	documentation and/or other materials provided with the a_distribution.;
	3. All advertising materials mentioning features or use of this a_software;
	must display the following a_acknowledgement:;
	This product includes software developed by the University a_of;
	California, Berkeley and its a_contributors.;
	4. Neither the name of the University nor the names of its a_contributors;
	may be used to endorse or promote products derived from this a_software;
	without specific prior written a_permission.;
	 a_;
	THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' a_AND;
	ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, a_THE;
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR a_PURPOSE;
	ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE a_LIABLE;
	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR a_CONSEQUENTIAL;
	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE a_GOODS;
	OR SERVICES LOSS OF USE, DATA, OR PROFITS; OR BUSINESS a_INTERRUPTION);
	HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, a_STRICT;
	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY a_WAY;
	OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY a_OF;
	SUCH a_DAMAGE.;
	 a_;
	 a_;
	vnode_if.sh,v 1.8 1995/03/10 04:13:52 chopps Exp a_$';
	 a_;
	 a_;
	[ $# -ne 1 ] a_then;
	'usage: vnode_if.sh a_srcfile';
	1 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	'BEGIN { print toupper("true") exit; }' a_2>/dev/null`;
	 a_;
	[ "$isgawk" = TRUE ] a_then;
	GNU awk provides a_it.;
	 a_;
	 a_;
	Provide our own a_toupper();
	 a_;
	toupper(str) a_{;
	= "echo "str" |tr a-z a_A-Z";
	| getline a__toupper_str;
	 a_;
	_toupper_str a_;
	 a_;
	 a_;
	\1:g a_;
	/ a_/';
	 a_;
	{ next a_};
	{ a_;
	 a_;
	 a_;
	 a_;
	{ a_;
	 a_;
	 a_;
	 a_;
	= $1 a_i=2;;
	($2 == "WILLRELE") a_{;
	= a_1;
	 a_;
	else a_;
	= a_0;
	= $i a_i++;;
	(i < NF) a_{;
	= argtype[argc]" a_"$i;
	 a_;
	 a_;
	= a_$i;
	 a_;
	 a_;
};
extern struct vnodeop_desc vop_whiteout_desc;
static __inline int VOP_WHITEOUT __P((struct vnode *, 
    struct componentname *, int, , , 
    braam+@pachelbel.coda.cs.cmu.edu Wed Oct 9 18:08:18, 
    braam+@pachelbel.coda.cs.cmu.edu, 
    from pachelbel.coda.cs.cmu.edu (PACHELBEL.CODA.CS.CMU.EDU [128.2.222.26]) by telemann.coda.cs.cmu.edu (8.7.4/8.7.3) with SMTP id SAA09451 for <braam@telemann.coda.cs.cmu.edu> Wed, 9 Oct 1996 18:08:17, 
    braam+@pachelbel.coda.cs.cmu.edu, 
    <199610092208.SAA09451@telemann.coda.cs.cmu.edu>, 
    braam@pachelbel.coda.cs.cmu.edu, Wed, 9 Oct 96 18:07:56, 
    braam+@pachelbel.coda.cs.cmu.edu, , , , Copyright (c) 1992,, 
    The Regents of the University of California. All rights, , 
    Redistribution and use in source and binary forms, with or, 
    modification, are permitted provided that the following, are, 
    1. Redistributions of source code must retain the above, 
    notice, this list of conditions and the following, 
    2. Redistributions in binary form must reproduce the above, 
    notice, this list of conditions and the following disclaimer in, 
    documentation and/or other materials provided with the, 
    3. All advertising materials mentioning features or use of this, 
    must display the following, 
    This product includes software developed by the University, 
    California, Berkeley and its, 
    4. Neither the name of the University nor the names of its, 
    may be used to endorse or promote products derived from this, 
    without specific prior written, , 
    THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'', 
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,, 
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR, 
    ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE, 
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR, 
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE, 
    OR SERVICES LOSS OF USE, DATA, OR PROFITS; OR BUSINESS, 
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,, 
    LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY, 
    OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY, 
    SUCH, , , vnode_if.sh,v 1.8 1995/03/10 04:13:52 chopps Exp, , , 
    [ $# -ne 1 ], 'usage: vnode_if.sh, 1, , , , , , , , , , 
    'BEGIN { print toupper("true") exit; }', , [ "$isgawk" = TRUE ], 
    GNU awk provides, , , Provide our own, , toupper(str), 
    = "echo "str" |tr a-z, | getline, , _toupper_str, , , \1:g, /, , { next, 
    {, , , , {, , , , = $1, ($2 == "WILLRELE"), =, , else, =, = $i, 
    (i < NF), = argtype[argc]", , , =, , ));
static __inline int VOP_WHITEOUT(dvp, cnp, flags, , , 1996, , -0400, , , , EDT, , , , , 1993, reserved., , without, conditions, met:, copyright, disclaimer., copyright, the, distribution., software, acknowledgement:, of, contributors., contributors, software, permission., , AND, THE, PURPOSE, LIABLE, CONSEQUENTIAL, GOODS, INTERRUPTION), STRICT, WAY, OF, DAMAGE., , , $', , , then, srcfile', , , , , , , , , , , 2>/dev/null`, , then, it., , , toupper(), , {, A-Z", _toupper_str, , , , , , /', , }, , , , , , , , , i=2;, {, 1, , , 0, i++;, {, "$i, , , $i, , )
	struct vnode *dvp;
	struct componentname *cnp;
	int flags;
	 ;
	 ;
	braam+@pachelbel.coda.cs.cmu.edu Wed Oct 9 18:08:18 1996;
	braam+@pachelbel.coda.cs.cmu.edu ;
	from pachelbel.coda.cs.cmu.edu (PACHELBEL.CODA.CS.CMU.EDU [128.2.222.26]) by telemann.coda.cs.cmu.edu (8.7.4/8.7.3) with SMTP id SAA09451 for <braam@telemann.coda.cs.cmu.edu> Wed, 9 Oct 1996 18:08:17 -0400;
	braam+@pachelbel.coda.cs.cmu.edu ;
	<199610092208.SAA09451@telemann.coda.cs.cmu.edu> ;
	braam@pachelbel.coda.cs.cmu.edu ;
	Wed, 9 Oct 96 18:07:56 EDT;
	braam+@pachelbel.coda.cs.cmu.edu ;
	 ;
	 ;
	 ;
	Copyright (c) 1992, 1993;
	The Regents of the University of California. All rights reserved.;
	 ;
	Redistribution and use in source and binary forms, with or without;
	modification, are permitted provided that the following conditions;
	are met:;
	1. Redistributions of source code must retain the above copyright;
	notice, this list of conditions and the following disclaimer.;
	2. Redistributions in binary form must reproduce the above copyright;
	notice, this list of conditions and the following disclaimer in the;
	documentation and/or other materials provided with the distribution.;
	3. All advertising materials mentioning features or use of this software;
	must display the following acknowledgement:;
	This product includes software developed by the University of;
	California, Berkeley and its contributors.;
	4. Neither the name of the University nor the names of its contributors;
	may be used to endorse or promote products derived from this software;
	without specific prior written permission.;
	 ;
	THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND;
	ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE;
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE;
	ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE;
	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL;
	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS;
	OR SERVICES LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION);
	HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT;
	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY;
	OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF;
	SUCH DAMAGE.;
	 ;
	 ;
	vnode_if.sh,v 1.8 1995/03/10 04:13:52 chopps Exp $';
	 ;
	 ;
	[ $# -ne 1 ] then;
	'usage: vnode_if.sh srcfile';
	1 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	'BEGIN { print toupper("true") exit; }' 2>/dev/null`;
	 ;
	[ "$isgawk" = TRUE ] then;
	GNU awk provides it.;
	 ;
	 ;
	Provide our own toupper();
	 ;
	toupper(str) {;
	= "echo "str" |tr a-z A-Z";
	| getline _toupper_str;
	 ;
	_toupper_str ;
	 ;
	 ;
	\1:g ;
	/ /';
	 ;
	{ next };
	{ ;
	 ;
	 ;
	 ;
	{ ;
	 ;
	 ;
	 ;
	= $1 i=2;;
	($2 == "WILLRELE") {;
	= 1;
	 ;
	else ;
	= 0;
	= $i i++;;
	(i < NF) {;
	= argtype[argc]" "$i;
	 ;
	 ;
	= $i;
	 ;
	 ;
{
	struct vop_whiteout_args a;
	a.a_desc = VDESC(vop_whiteout);
	a.a_dvp = dvp;
	a.a_cnp = cnp;
	a.a_flags = flags;
	a.a_ = ;
	a.a_ = ;
	a.a_1996 = 1996;
	a.a_ = ;
	a.a_-0400 = -0400;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_EDT = EDT;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_1993 = 1993;
	a.a_reserved. = reserved.;
	a.a_ = ;
	a.a_without = without;
	a.a_conditions = conditions;
	a.a_met: = met:;
	a.a_copyright = copyright;
	a.a_disclaimer. = disclaimer.;
	a.a_copyright = copyright;
	a.a_the = the;
	a.a_distribution. = distribution.;
	a.a_software = software;
	a.a_acknowledgement: = acknowledgement:;
	a.a_of = of;
	a.a_contributors. = contributors.;
	a.a_contributors = contributors;
	a.a_software = software;
	a.a_permission. = permission.;
	a.a_ = ;
	a.a_AND = AND;
	a.a_THE = THE;
	a.a_PURPOSE = PURPOSE;
	a.a_LIABLE = LIABLE;
	a.a_CONSEQUENTIAL = CONSEQUENTIAL;
	a.a_GOODS = GOODS;
	a.a_INTERRUPTION) = INTERRUPTION);
	a.a_STRICT = STRICT;
	a.a_WAY = WAY;
	a.a_OF = OF;
	a.a_DAMAGE. = DAMAGE.;
	a.a_ = ;
	a.a_ = ;
	a.a_$' = $';
	a.a_ = ;
	a.a_ = ;
	a.a_then = then;
	a.a_srcfile' = srcfile';
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_2>/dev/null` = 2>/dev/null`;
	a.a_ = ;
	a.a_then = then;
	a.a_it. = it.;
	a.a_ = ;
	a.a_ = ;
	a.a_toupper() = toupper();
	a.a_ = ;
	a.a_{ = {;
	a.a_A-Z" = A-Z";
	a.a__toupper_str = _toupper_str;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_/' = /';
	a.a_ = ;
	a.a_} = };
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_i=2; = i=2;;
	a.a_{ = {;
	a.a_1 = 1;
	a.a_ = ;
	a.a_ = ;
	a.a_0 = 0;
	a.a_i++; = i++;;
	a.a_{ = {;
	a.a_"$i = "$i;
	a.a_ = ;
	a.a_ = ;
	a.a_$i = $i;
	a.a_ = ;
	a.a_ = ;
	return (VCALL(dvp, VOFFSET(vop_whiteout), &a));
}

struct vop_whiteout_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct componentname *a_cnp;
	int a_flags;
	 a_;
	 a_;
	braam+@pachelbel.coda.cs.cmu.edu Wed Oct 9 18:08:18 a_1996;
	braam+@pachelbel.coda.cs.cmu.edu a_;
	from pachelbel.coda.cs.cmu.edu (PACHELBEL.CODA.CS.CMU.EDU [128.2.222.26]) by telemann.coda.cs.cmu.edu (8.7.4/8.7.3) with SMTP id SAA09451 for <braam@telemann.coda.cs.cmu.edu> Wed, 9 Oct 1996 18:08:17 a_-0400;
	braam+@pachelbel.coda.cs.cmu.edu a_;
	<199610092208.SAA09451@telemann.coda.cs.cmu.edu> a_;
	braam@pachelbel.coda.cs.cmu.edu a_;
	Wed, 9 Oct 96 18:07:56 a_EDT;
	braam+@pachelbel.coda.cs.cmu.edu a_;
	 a_;
	 a_;
	 a_;
	Copyright (c) 1992, a_1993;
	The Regents of the University of California. All rights a_reserved.;
	 a_;
	Redistribution and use in source and binary forms, with or a_without;
	modification, are permitted provided that the following a_conditions;
	are a_met:;
	1. Redistributions of source code must retain the above a_copyright;
	notice, this list of conditions and the following a_disclaimer.;
	2. Redistributions in binary form must reproduce the above a_copyright;
	notice, this list of conditions and the following disclaimer in a_the;
	documentation and/or other materials provided with the a_distribution.;
	3. All advertising materials mentioning features or use of this a_software;
	must display the following a_acknowledgement:;
	This product includes software developed by the University a_of;
	California, Berkeley and its a_contributors.;
	4. Neither the name of the University nor the names of its a_contributors;
	may be used to endorse or promote products derived from this a_software;
	without specific prior written a_permission.;
	 a_;
	THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' a_AND;
	ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, a_THE;
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR a_PURPOSE;
	ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE a_LIABLE;
	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR a_CONSEQUENTIAL;
	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE a_GOODS;
	OR SERVICES LOSS OF USE, DATA, OR PROFITS; OR BUSINESS a_INTERRUPTION);
	HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, a_STRICT;
	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY a_WAY;
	OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY a_OF;
	SUCH a_DAMAGE.;
	 a_;
	 a_;
	vnode_if.sh,v 1.8 1995/03/10 04:13:52 chopps Exp a_$';
	 a_;
	 a_;
	[ $# -ne 1 ] a_then;
	'usage: vnode_if.sh a_srcfile';
	1 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	'BEGIN { print toupper("true") exit; }' a_2>/dev/null`;
	 a_;
	[ "$isgawk" = TRUE ] a_then;
	GNU awk provides a_it.;
	 a_;
	 a_;
	Provide our own a_toupper();
	 a_;
	toupper(str) a_{;
	= "echo "str" |tr a-z a_A-Z";
	| getline a__toupper_str;
	 a_;
	_toupper_str a_;
	 a_;
	 a_;
	\1:g a_;
	/ a_/';
	 a_;
	{ next a_};
	{ a_;
	 a_;
	 a_;
	 a_;
	{ a_;
	 a_;
	 a_;
	 a_;
	= $1 a_i=2;;
	($2 == "WILLRELE") a_{;
	= a_1;
	 a_;
	else a_;
	= a_0;
	= $i a_i++;;
	(i < NF) a_{;
	= argtype[argc]" a_"$i;
	 a_;
	 a_;
	= a_$i;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	Warning: This file is generated a_automatically.;
	(Modifications made here may easily be a_lost!);
	 a_;
	Created by the a_script:;
	${SCRIPT_ID} a_;
	 a_;
	 a_;
	 a_;
	:\1:g' a_;
	 a_;
	"$0: Creating $out_h" a_1>&2;
	> a_$out_h;
	 a_;
	"$copyright" a_;
	"$warning" a_;
	' a_;
	struct vnodeop_desc a_vop_default_desc;
	 a_;
	 a_;
	-e "$sed_prep" $src | $awk a_"$toupper"';
	doit() a_{;
	Declare arg struct, a_descriptor.;
	%s_args {\n", a_name);
	vnodeop_desc *a_desc a_\n");;
	(i=0 i<argc; i++) a_{;
	a_%s \n", argtype[i], a_argname[i]);;
	 a_;
	\n"); a_;
	struct vnodeop_desc %s_desc \n", a_name);;
	Prototype a_it.;
	= sprintf("static __inline int %s __P((", a_toupper(name));
	= a_length(protoarg);
	protoarg) a_;
	(i=0 i<argc; i++) a_{;
	= sprintf("%s", a_argtype[i]);
	(i < (argc-1)) protoarg = (protoarg ", a_");
	= a_length(protoarg);
	((protolen + arglen) > 77) a_{;
	= ("\n " a_protoarg);
	+= a_4;
	= a_0;
	 a_;
	protoarg) a_;
	+= a_arglen;
	 a_;
	\n"); a_;
	Define inline a_function.;
	__inline int %s(", a_toupper(name));
	(i=0 i<argc; i++) a_{;
	argname[i]) a_;
	(i < (argc-1)) printf(", a_");
	 a_;
	 a_;
	(i=0 i<argc; i++) a_{;
	%s \n", argtype[i], a_argname[i]);;
	 a_;
	%s_args a \n", a_name);;
	= VDESC(%s) \n", a_name);;
	(i=0 i<argc; i++) a_{;
	= %s \n", argname[i], a_argname[i]);;
	 a_;
	(VCALL(%s%s, VOFFSET(%s), &a)) a_\n}\n",;
	arg0special, a_name);
};
extern struct vnodeop_desc vop_whiteout_desc;
static __inline int VOP_WHITEOUT __P((struct vnode *, 
    struct componentname *, int, , , 
    braam+@pachelbel.coda.cs.cmu.edu Wed Oct 9 18:08:18, 
    braam+@pachelbel.coda.cs.cmu.edu, 
    from pachelbel.coda.cs.cmu.edu (PACHELBEL.CODA.CS.CMU.EDU [128.2.222.26]) by telemann.coda.cs.cmu.edu (8.7.4/8.7.3) with SMTP id SAA09451 for <braam@telemann.coda.cs.cmu.edu> Wed, 9 Oct 1996 18:08:17, 
    braam+@pachelbel.coda.cs.cmu.edu, 
    <199610092208.SAA09451@telemann.coda.cs.cmu.edu>, 
    braam@pachelbel.coda.cs.cmu.edu, Wed, 9 Oct 96 18:07:56, 
    braam+@pachelbel.coda.cs.cmu.edu, , , , Copyright (c) 1992,, 
    The Regents of the University of California. All rights, , 
    Redistribution and use in source and binary forms, with or, 
    modification, are permitted provided that the following, are, 
    1. Redistributions of source code must retain the above, 
    notice, this list of conditions and the following, 
    2. Redistributions in binary form must reproduce the above, 
    notice, this list of conditions and the following disclaimer in, 
    documentation and/or other materials provided with the, 
    3. All advertising materials mentioning features or use of this, 
    must display the following, 
    This product includes software developed by the University, 
    California, Berkeley and its, 
    4. Neither the name of the University nor the names of its, 
    may be used to endorse or promote products derived from this, 
    without specific prior written, , 
    THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'', 
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,, 
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR, 
    ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE, 
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR, 
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE, 
    OR SERVICES LOSS OF USE, DATA, OR PROFITS; OR BUSINESS, 
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,, 
    LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY, 
    OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY, 
    SUCH, , , vnode_if.sh,v 1.8 1995/03/10 04:13:52 chopps Exp, , , 
    [ $# -ne 1 ], 'usage: vnode_if.sh, 1, , , , , , , , , , 
    'BEGIN { print toupper("true") exit; }', , [ "$isgawk" = TRUE ], 
    GNU awk provides, , , Provide our own, , toupper(str), 
    = "echo "str" |tr a-z, | getline, , _toupper_str, , , \1:g, /, , { next, 
    {, , , , {, , , , = $1, ($2 == "WILLRELE"), =, , else, =, = $i, 
    (i < NF), = argtype[argc]", , , =, , , , , , , 
    Warning: This file is generated, (Modifications made here may easily be, 
    , Created by the, ${SCRIPT_ID}, , , , :\1:g', , "$0: Creating $out_h", 
    >, , "$copyright", "$warning", ', struct vnodeop_desc, , , 
    -e "$sed_prep" $src | $awk, doit(), Declare arg struct,, %s_args {\n",, 
    vnodeop_desc *a_desc, (i=0 i<argc; i++), a_%s \n", argtype[i],, , 
    \n");, struct vnodeop_desc %s_desc \n",, Prototype, 
    = sprintf("static __inline int %s __P((",, =, protoarg), 
    (i=0 i<argc; i++), = sprintf("%s",, 
    (i < (argc-1)) protoarg = (protoarg ",, =, ((protolen + arglen) > 77), 
    = ("\n ", +=, =, , protoarg), +=, , \n");, Define inline, 
    __inline int %s(",, (i=0 i<argc; i++), argname[i]), 
    (i < (argc-1)) printf(",, , , (i=0 i<argc; i++), %s \n", argtype[i],, , 
    %s_args a \n",, = VDESC(%s) \n",, (i=0 i<argc; i++), 
    = %s \n", argname[i],, , (VCALL(%s%s, VOFFSET(%s), &a)), arg0special,));
static __inline int VOP_WHITEOUT(dvp, cnp, flags, , , 1996, , -0400, , , , EDT, , , , , 1993, reserved., , without, conditions, met:, copyright, disclaimer., copyright, the, distribution., software, acknowledgement:, of, contributors., contributors, software, permission., , AND, THE, PURPOSE, LIABLE, CONSEQUENTIAL, GOODS, INTERRUPTION), STRICT, WAY, OF, DAMAGE., , , $', , , then, srcfile', , , , , , , , , , , 2>/dev/null`, , then, it., , , toupper(), , {, A-Z", _toupper_str, , , , , , /', , }, , , , , , , , , i=2;, {, 1, , , 0, i++;, {, "$i, , , $i, , , , , , , automatically., lost!), , script:, , , , , , , 1>&2, $out_h, , , , , vop_default_desc, , , "$toupper"', {, descriptor., name), \n");, {, argname[i]);, , , name);, it., toupper(name)), length(protoarg), , {, argtype[i]), "), length(protoarg), {, protoarg), 4, 0, , , arglen, , , function., toupper(name)), {, , "), , , {, argname[i]);, , name);, name);, {, argname[i]);, , \n}\n",, name))
	struct vnode *dvp;
	struct componentname *cnp;
	int flags;
	 ;
	 ;
	braam+@pachelbel.coda.cs.cmu.edu Wed Oct 9 18:08:18 1996;
	braam+@pachelbel.coda.cs.cmu.edu ;
	from pachelbel.coda.cs.cmu.edu (PACHELBEL.CODA.CS.CMU.EDU [128.2.222.26]) by telemann.coda.cs.cmu.edu (8.7.4/8.7.3) with SMTP id SAA09451 for <braam@telemann.coda.cs.cmu.edu> Wed, 9 Oct 1996 18:08:17 -0400;
	braam+@pachelbel.coda.cs.cmu.edu ;
	<199610092208.SAA09451@telemann.coda.cs.cmu.edu> ;
	braam@pachelbel.coda.cs.cmu.edu ;
	Wed, 9 Oct 96 18:07:56 EDT;
	braam+@pachelbel.coda.cs.cmu.edu ;
	 ;
	 ;
	 ;
	Copyright (c) 1992, 1993;
	The Regents of the University of California. All rights reserved.;
	 ;
	Redistribution and use in source and binary forms, with or without;
	modification, are permitted provided that the following conditions;
	are met:;
	1. Redistributions of source code must retain the above copyright;
	notice, this list of conditions and the following disclaimer.;
	2. Redistributions in binary form must reproduce the above copyright;
	notice, this list of conditions and the following disclaimer in the;
	documentation and/or other materials provided with the distribution.;
	3. All advertising materials mentioning features or use of this software;
	must display the following acknowledgement:;
	This product includes software developed by the University of;
	California, Berkeley and its contributors.;
	4. Neither the name of the University nor the names of its contributors;
	may be used to endorse or promote products derived from this software;
	without specific prior written permission.;
	 ;
	THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND;
	ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE;
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE;
	ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE;
	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL;
	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS;
	OR SERVICES LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION);
	HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT;
	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY;
	OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF;
	SUCH DAMAGE.;
	 ;
	 ;
	vnode_if.sh,v 1.8 1995/03/10 04:13:52 chopps Exp $';
	 ;
	 ;
	[ $# -ne 1 ] then;
	'usage: vnode_if.sh srcfile';
	1 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	'BEGIN { print toupper("true") exit; }' 2>/dev/null`;
	 ;
	[ "$isgawk" = TRUE ] then;
	GNU awk provides it.;
	 ;
	 ;
	Provide our own toupper();
	 ;
	toupper(str) {;
	= "echo "str" |tr a-z A-Z";
	| getline _toupper_str;
	 ;
	_toupper_str ;
	 ;
	 ;
	\1:g ;
	/ /';
	 ;
	{ next };
	{ ;
	 ;
	 ;
	 ;
	{ ;
	 ;
	 ;
	 ;
	= $1 i=2;;
	($2 == "WILLRELE") {;
	= 1;
	 ;
	else ;
	= 0;
	= $i i++;;
	(i < NF) {;
	= argtype[argc]" "$i;
	 ;
	 ;
	= $i;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	Warning: This file is generated automatically.;
	(Modifications made here may easily be lost!);
	 ;
	Created by the script:;
	${SCRIPT_ID} ;
	 ;
	 ;
	 ;
	:\1:g' ;
	 ;
	"$0: Creating $out_h" 1>&2;
	> $out_h;
	 ;
	"$copyright" ;
	"$warning" ;
	' ;
	struct vnodeop_desc vop_default_desc;
	 ;
	 ;
	-e "$sed_prep" $src | $awk "$toupper"';
	doit() {;
	Declare arg struct, descriptor.;
	%s_args {\n", name);
	vnodeop_desc *a_desc \n");;
	(i=0 i<argc; i++) {;
	a_%s \n", argtype[i], argname[i]);;
	 ;
	\n"); ;
	struct vnodeop_desc %s_desc \n", name);;
	Prototype it.;
	= sprintf("static __inline int %s __P((", toupper(name));
	= length(protoarg);
	protoarg) ;
	(i=0 i<argc; i++) {;
	= sprintf("%s", argtype[i]);
	(i < (argc-1)) protoarg = (protoarg ", ");
	= length(protoarg);
	((protolen + arglen) > 77) {;
	= ("\n " protoarg);
	+= 4;
	= 0;
	 ;
	protoarg) ;
	+= arglen;
	 ;
	\n"); ;
	Define inline function.;
	__inline int %s(", toupper(name));
	(i=0 i<argc; i++) {;
	argname[i]) ;
	(i < (argc-1)) printf(", ");
	 ;
	 ;
	(i=0 i<argc; i++) {;
	%s \n", argtype[i], argname[i]);;
	 ;
	%s_args a \n", name);;
	= VDESC(%s) \n", name);;
	(i=0 i<argc; i++) {;
	= %s \n", argname[i], argname[i]);;
	 ;
	(VCALL(%s%s, VOFFSET(%s), &a)) \n}\n",;
	arg0special, name);
{
	struct vop_whiteout_args a;
	a.a_desc = VDESC(vop_whiteout);
	a.a_dvp = dvp;
	a.a_cnp = cnp;
	a.a_flags = flags;
	a.a_ = ;
	a.a_ = ;
	a.a_1996 = 1996;
	a.a_ = ;
	a.a_-0400 = -0400;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_EDT = EDT;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_1993 = 1993;
	a.a_reserved. = reserved.;
	a.a_ = ;
	a.a_without = without;
	a.a_conditions = conditions;
	a.a_met: = met:;
	a.a_copyright = copyright;
	a.a_disclaimer. = disclaimer.;
	a.a_copyright = copyright;
	a.a_the = the;
	a.a_distribution. = distribution.;
	a.a_software = software;
	a.a_acknowledgement: = acknowledgement:;
	a.a_of = of;
	a.a_contributors. = contributors.;
	a.a_contributors = contributors;
	a.a_software = software;
	a.a_permission. = permission.;
	a.a_ = ;
	a.a_AND = AND;
	a.a_THE = THE;
	a.a_PURPOSE = PURPOSE;
	a.a_LIABLE = LIABLE;
	a.a_CONSEQUENTIAL = CONSEQUENTIAL;
	a.a_GOODS = GOODS;
	a.a_INTERRUPTION) = INTERRUPTION);
	a.a_STRICT = STRICT;
	a.a_WAY = WAY;
	a.a_OF = OF;
	a.a_DAMAGE. = DAMAGE.;
	a.a_ = ;
	a.a_ = ;
	a.a_$' = $';
	a.a_ = ;
	a.a_ = ;
	a.a_then = then;
	a.a_srcfile' = srcfile';
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_2>/dev/null` = 2>/dev/null`;
	a.a_ = ;
	a.a_then = then;
	a.a_it. = it.;
	a.a_ = ;
	a.a_ = ;
	a.a_toupper() = toupper();
	a.a_ = ;
	a.a_{ = {;
	a.a_A-Z" = A-Z";
	a.a__toupper_str = _toupper_str;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_/' = /';
	a.a_ = ;
	a.a_} = };
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_i=2; = i=2;;
	a.a_{ = {;
	a.a_1 = 1;
	a.a_ = ;
	a.a_ = ;
	a.a_0 = 0;
	a.a_i++; = i++;;
	a.a_{ = {;
	a.a_"$i = "$i;
	a.a_ = ;
	a.a_ = ;
	a.a_$i = $i;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_automatically. = automatically.;
	a.a_lost!) = lost!);
	a.a_ = ;
	a.a_script: = script:;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_1>&2 = 1>&2;
	a.a_$out_h = $out_h;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_vop_default_desc = vop_default_desc;
	a.a_ = ;
	a.a_ = ;
	a.a_"$toupper"' = "$toupper"';
	a.a_{ = {;
	a.a_descriptor. = descriptor.;
	a.a_name) = name);
	a.a_\n"); = \n");;
	a.a_{ = {;
	a.a_argname[i]); = argname[i]);;
	a.a_ = ;
	a.a_ = ;
	a.a_name); = name);;
	a.a_it. = it.;
	a.a_toupper(name)) = toupper(name));
	a.a_length(protoarg) = length(protoarg);
	a.a_ = ;
	a.a_{ = {;
	a.a_argtype[i]) = argtype[i]);
	a.a_") = ");
	a.a_length(protoarg) = length(protoarg);
	a.a_{ = {;
	a.a_protoarg) = protoarg);
	a.a_4 = 4;
	a.a_0 = 0;
	a.a_ = ;
	a.a_ = ;
	a.a_arglen = arglen;
	a.a_ = ;
	a.a_ = ;
	a.a_function. = function.;
	a.a_toupper(name)) = toupper(name));
	a.a_{ = {;
	a.a_ = ;
	a.a_") = ");
	a.a_ = ;
	a.a_ = ;
	a.a_{ = {;
	a.a_argname[i]); = argname[i]);;
	a.a_ = ;
	a.a_name); = name);;
	a.a_name); = name);;
	a.a_{ = {;
	a.a_argname[i]); = argname[i]);;
	a.a_ = ;
	a.a_\n}\n", = \n}\n",;
	a.a_name) = name);
	return (VCALL(dvp, VOFFSET(vop_whiteout), &a));
}

struct vop_whiteout_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct componentname *a_cnp;
	int a_flags;
	 a_;
	 a_;
	braam+@pachelbel.coda.cs.cmu.edu Wed Oct 9 18:08:18 a_1996;
	braam+@pachelbel.coda.cs.cmu.edu a_;
	from pachelbel.coda.cs.cmu.edu (PACHELBEL.CODA.CS.CMU.EDU [128.2.222.26]) by telemann.coda.cs.cmu.edu (8.7.4/8.7.3) with SMTP id SAA09451 for <braam@telemann.coda.cs.cmu.edu> Wed, 9 Oct 1996 18:08:17 a_-0400;
	braam+@pachelbel.coda.cs.cmu.edu a_;
	<199610092208.SAA09451@telemann.coda.cs.cmu.edu> a_;
	braam@pachelbel.coda.cs.cmu.edu a_;
	Wed, 9 Oct 96 18:07:56 a_EDT;
	braam+@pachelbel.coda.cs.cmu.edu a_;
	 a_;
	 a_;
	 a_;
	Copyright (c) 1992, a_1993;
	The Regents of the University of California. All rights a_reserved.;
	 a_;
	Redistribution and use in source and binary forms, with or a_without;
	modification, are permitted provided that the following a_conditions;
	are a_met:;
	1. Redistributions of source code must retain the above a_copyright;
	notice, this list of conditions and the following a_disclaimer.;
	2. Redistributions in binary form must reproduce the above a_copyright;
	notice, this list of conditions and the following disclaimer in a_the;
	documentation and/or other materials provided with the a_distribution.;
	3. All advertising materials mentioning features or use of this a_software;
	must display the following a_acknowledgement:;
	This product includes software developed by the University a_of;
	California, Berkeley and its a_contributors.;
	4. Neither the name of the University nor the names of its a_contributors;
	may be used to endorse or promote products derived from this a_software;
	without specific prior written a_permission.;
	 a_;
	THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' a_AND;
	ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, a_THE;
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR a_PURPOSE;
	ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE a_LIABLE;
	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR a_CONSEQUENTIAL;
	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE a_GOODS;
	OR SERVICES LOSS OF USE, DATA, OR PROFITS; OR BUSINESS a_INTERRUPTION);
	HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, a_STRICT;
	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY a_WAY;
	OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY a_OF;
	SUCH a_DAMAGE.;
	 a_;
	 a_;
	vnode_if.sh,v 1.8 1995/03/10 04:13:52 chopps Exp a_$';
	 a_;
	 a_;
	[ $# -ne 1 ] a_then;
	'usage: vnode_if.sh a_srcfile';
	1 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	'BEGIN { print toupper("true") exit; }' a_2>/dev/null`;
	 a_;
	[ "$isgawk" = TRUE ] a_then;
	GNU awk provides a_it.;
	 a_;
	 a_;
	Provide our own a_toupper();
	 a_;
	toupper(str) a_{;
	= "echo "str" |tr a-z a_A-Z";
	| getline a__toupper_str;
	 a_;
	_toupper_str a_;
	 a_;
	 a_;
	\1:g a_;
	/ a_/';
	 a_;
	{ next a_};
	{ a_;
	 a_;
	 a_;
	 a_;
	{ a_;
	 a_;
	 a_;
	 a_;
	= $1 a_i=2;;
	($2 == "WILLRELE") a_{;
	= a_1;
	 a_;
	else a_;
	= a_0;
	= $i a_i++;;
	(i < NF) a_{;
	= argtype[argc]" a_"$i;
	 a_;
	 a_;
	= a_$i;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	Warning: This file is generated a_automatically.;
	(Modifications made here may easily be a_lost!);
	 a_;
	Created by the a_script:;
	${SCRIPT_ID} a_;
	 a_;
	 a_;
	 a_;
	:\1:g' a_;
	 a_;
	"$0: Creating $out_h" a_1>&2;
	> a_$out_h;
	 a_;
	"$copyright" a_;
	"$warning" a_;
	' a_;
	struct vnodeop_desc a_vop_default_desc;
	 a_;
	 a_;
	-e "$sed_prep" $src | $awk a_"$toupper"';
	doit() a_{;
	Declare arg struct, a_descriptor.;
	%s_args {\n", a_name);
	vnodeop_desc *a_desc a_\n");;
	(i=0 i<argc; i++) a_{;
	a_%s \n", argtype[i], a_argname[i]);;
	 a_;
	\n"); a_;
	struct vnodeop_desc %s_desc \n", a_name);;
	Prototype a_it.;
	= sprintf("static __inline int %s __P((", a_toupper(name));
	= a_length(protoarg);
	protoarg) a_;
	(i=0 i<argc; i++) a_{;
	= sprintf("%s", a_argtype[i]);
	(i < (argc-1)) protoarg = (protoarg ", a_");
	= a_length(protoarg);
	((protolen + arglen) > 77) a_{;
	= ("\n " a_protoarg);
	+= a_4;
	= a_0;
	 a_;
	protoarg) a_;
	+= a_arglen;
	 a_;
	\n"); a_;
	Define inline a_function.;
	__inline int %s(", a_toupper(name));
	(i=0 i<argc; i++) a_{;
	argname[i]) a_;
	(i < (argc-1)) printf(", a_");
	 a_;
	 a_;
	(i=0 i<argc; i++) a_{;
	%s \n", argtype[i], a_argname[i]);;
	 a_;
	%s_args a \n", a_name);;
	= VDESC(%s) \n", a_name);;
	(i=0 i<argc; i++) a_{;
	= %s \n", argname[i], a_argname[i]);;
	 a_;
	(VCALL(%s%s, VOFFSET(%s), &a)) a_\n}\n",;
	arg0special, a_name);
	{ a_;
	 a_;
};
extern struct vnodeop_desc vop_whiteout_desc;
static __inline int VOP_WHITEOUT __P((struct vnode *, 
    struct componentname *, int, , , 
    braam+@pachelbel.coda.cs.cmu.edu Wed Oct 9 18:08:18, 
    braam+@pachelbel.coda.cs.cmu.edu, 
    from pachelbel.coda.cs.cmu.edu (PACHELBEL.CODA.CS.CMU.EDU [128.2.222.26]) by telemann.coda.cs.cmu.edu (8.7.4/8.7.3) with SMTP id SAA09451 for <braam@telemann.coda.cs.cmu.edu> Wed, 9 Oct 1996 18:08:17, 
    braam+@pachelbel.coda.cs.cmu.edu, 
    <199610092208.SAA09451@telemann.coda.cs.cmu.edu>, 
    braam@pachelbel.coda.cs.cmu.edu, Wed, 9 Oct 96 18:07:56, 
    braam+@pachelbel.coda.cs.cmu.edu, , , , Copyright (c) 1992,, 
    The Regents of the University of California. All rights, , 
    Redistribution and use in source and binary forms, with or, 
    modification, are permitted provided that the following, are, 
    1. Redistributions of source code must retain the above, 
    notice, this list of conditions and the following, 
    2. Redistributions in binary form must reproduce the above, 
    notice, this list of conditions and the following disclaimer in, 
    documentation and/or other materials provided with the, 
    3. All advertising materials mentioning features or use of this, 
    must display the following, 
    This product includes software developed by the University, 
    California, Berkeley and its, 
    4. Neither the name of the University nor the names of its, 
    may be used to endorse or promote products derived from this, 
    without specific prior written, , 
    THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'', 
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,, 
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR, 
    ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE, 
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR, 
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE, 
    OR SERVICES LOSS OF USE, DATA, OR PROFITS; OR BUSINESS, 
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,, 
    LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY, 
    OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY, 
    SUCH, , , vnode_if.sh,v 1.8 1995/03/10 04:13:52 chopps Exp, , , 
    [ $# -ne 1 ], 'usage: vnode_if.sh, 1, , , , , , , , , , 
    'BEGIN { print toupper("true") exit; }', , [ "$isgawk" = TRUE ], 
    GNU awk provides, , , Provide our own, , toupper(str), 
    = "echo "str" |tr a-z, | getline, , _toupper_str, , , \1:g, /, , { next, 
    {, , , , {, , , , = $1, ($2 == "WILLRELE"), =, , else, =, = $i, 
    (i < NF), = argtype[argc]", , , =, , , , , , , 
    Warning: This file is generated, (Modifications made here may easily be, 
    , Created by the, ${SCRIPT_ID}, , , , :\1:g', , "$0: Creating $out_h", 
    >, , "$copyright", "$warning", ', struct vnodeop_desc, , , 
    -e "$sed_prep" $src | $awk, doit(), Declare arg struct,, %s_args {\n",, 
    vnodeop_desc *a_desc, (i=0 i<argc; i++), a_%s \n", argtype[i],, , 
    \n");, struct vnodeop_desc %s_desc \n",, Prototype, 
    = sprintf("static __inline int %s __P((",, =, protoarg), 
    (i=0 i<argc; i++), = sprintf("%s",, 
    (i < (argc-1)) protoarg = (protoarg ",, =, ((protolen + arglen) > 77), 
    = ("\n ", +=, =, , protoarg), +=, , \n");, Define inline, 
    __inline int %s(",, (i=0 i<argc; i++), argname[i]), 
    (i < (argc-1)) printf(",, , , (i=0 i<argc; i++), %s \n", argtype[i],, , 
    %s_args a \n",, = VDESC(%s) \n",, (i=0 i<argc; i++), 
    = %s \n", argname[i],, , (VCALL(%s%s, VOFFSET(%s), &a)), arg0special,, 
    {, ));
static __inline int VOP_WHITEOUT(dvp, cnp, flags, , , 1996, , -0400, , , , EDT, , , , , 1993, reserved., , without, conditions, met:, copyright, disclaimer., copyright, the, distribution., software, acknowledgement:, of, contributors., contributors, software, permission., , AND, THE, PURPOSE, LIABLE, CONSEQUENTIAL, GOODS, INTERRUPTION), STRICT, WAY, OF, DAMAGE., , , $', , , then, srcfile', , , , , , , , , , , 2>/dev/null`, , then, it., , , toupper(), , {, A-Z", _toupper_str, , , , , , /', , }, , , , , , , , , i=2;, {, 1, , , 0, i++;, {, "$i, , , $i, , , , , , , automatically., lost!), , script:, , , , , , , 1>&2, $out_h, , , , , vop_default_desc, , , "$toupper"', {, descriptor., name), \n");, {, argname[i]);, , , name);, it., toupper(name)), length(protoarg), , {, argtype[i]), "), length(protoarg), {, protoarg), 4, 0, , , arglen, , , function., toupper(name)), {, , "), , , {, argname[i]);, , name);, name);, {, argname[i]);, , \n}\n",, name), , )
	struct vnode *dvp;
	struct componentname *cnp;
	int flags;
	 ;
	 ;
	braam+@pachelbel.coda.cs.cmu.edu Wed Oct 9 18:08:18 1996;
	braam+@pachelbel.coda.cs.cmu.edu ;
	from pachelbel.coda.cs.cmu.edu (PACHELBEL.CODA.CS.CMU.EDU [128.2.222.26]) by telemann.coda.cs.cmu.edu (8.7.4/8.7.3) with SMTP id SAA09451 for <braam@telemann.coda.cs.cmu.edu> Wed, 9 Oct 1996 18:08:17 -0400;
	braam+@pachelbel.coda.cs.cmu.edu ;
	<199610092208.SAA09451@telemann.coda.cs.cmu.edu> ;
	braam@pachelbel.coda.cs.cmu.edu ;
	Wed, 9 Oct 96 18:07:56 EDT;
	braam+@pachelbel.coda.cs.cmu.edu ;
	 ;
	 ;
	 ;
	Copyright (c) 1992, 1993;
	The Regents of the University of California. All rights reserved.;
	 ;
	Redistribution and use in source and binary forms, with or without;
	modification, are permitted provided that the following conditions;
	are met:;
	1. Redistributions of source code must retain the above copyright;
	notice, this list of conditions and the following disclaimer.;
	2. Redistributions in binary form must reproduce the above copyright;
	notice, this list of conditions and the following disclaimer in the;
	documentation and/or other materials provided with the distribution.;
	3. All advertising materials mentioning features or use of this software;
	must display the following acknowledgement:;
	This product includes software developed by the University of;
	California, Berkeley and its contributors.;
	4. Neither the name of the University nor the names of its contributors;
	may be used to endorse or promote products derived from this software;
	without specific prior written permission.;
	 ;
	THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND;
	ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE;
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE;
	ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE;
	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL;
	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS;
	OR SERVICES LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION);
	HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT;
	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY;
	OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF;
	SUCH DAMAGE.;
	 ;
	 ;
	vnode_if.sh,v 1.8 1995/03/10 04:13:52 chopps Exp $';
	 ;
	 ;
	[ $# -ne 1 ] then;
	'usage: vnode_if.sh srcfile';
	1 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	'BEGIN { print toupper("true") exit; }' 2>/dev/null`;
	 ;
	[ "$isgawk" = TRUE ] then;
	GNU awk provides it.;
	 ;
	 ;
	Provide our own toupper();
	 ;
	toupper(str) {;
	= "echo "str" |tr a-z A-Z";
	| getline _toupper_str;
	 ;
	_toupper_str ;
	 ;
	 ;
	\1:g ;
	/ /';
	 ;
	{ next };
	{ ;
	 ;
	 ;
	 ;
	{ ;
	 ;
	 ;
	 ;
	= $1 i=2;;
	($2 == "WILLRELE") {;
	= 1;
	 ;
	else ;
	= 0;
	= $i i++;;
	(i < NF) {;
	= argtype[argc]" "$i;
	 ;
	 ;
	= $i;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	Warning: This file is generated automatically.;
	(Modifications made here may easily be lost!);
	 ;
	Created by the script:;
	${SCRIPT_ID} ;
	 ;
	 ;
	 ;
	:\1:g' ;
	 ;
	"$0: Creating $out_h" 1>&2;
	> $out_h;
	 ;
	"$copyright" ;
	"$warning" ;
	' ;
	struct vnodeop_desc vop_default_desc;
	 ;
	 ;
	-e "$sed_prep" $src | $awk "$toupper"';
	doit() {;
	Declare arg struct, descriptor.;
	%s_args {\n", name);
	vnodeop_desc *a_desc \n");;
	(i=0 i<argc; i++) {;
	a_%s \n", argtype[i], argname[i]);;
	 ;
	\n"); ;
	struct vnodeop_desc %s_desc \n", name);;
	Prototype it.;
	= sprintf("static __inline int %s __P((", toupper(name));
	= length(protoarg);
	protoarg) ;
	(i=0 i<argc; i++) {;
	= sprintf("%s", argtype[i]);
	(i < (argc-1)) protoarg = (protoarg ", ");
	= length(protoarg);
	((protolen + arglen) > 77) {;
	= ("\n " protoarg);
	+= 4;
	= 0;
	 ;
	protoarg) ;
	+= arglen;
	 ;
	\n"); ;
	Define inline function.;
	__inline int %s(", toupper(name));
	(i=0 i<argc; i++) {;
	argname[i]) ;
	(i < (argc-1)) printf(", ");
	 ;
	 ;
	(i=0 i<argc; i++) {;
	%s \n", argtype[i], argname[i]);;
	 ;
	%s_args a \n", name);;
	= VDESC(%s) \n", name);;
	(i=0 i<argc; i++) {;
	= %s \n", argname[i], argname[i]);;
	 ;
	(VCALL(%s%s, VOFFSET(%s), &a)) \n}\n",;
	arg0special, name);
	{ ;
	 ;
{
	struct vop_whiteout_args a;
	a.a_desc = VDESC(vop_whiteout);
	a.a_dvp = dvp;
	a.a_cnp = cnp;
	a.a_flags = flags;
	a.a_ = ;
	a.a_ = ;
	a.a_1996 = 1996;
	a.a_ = ;
	a.a_-0400 = -0400;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_EDT = EDT;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_1993 = 1993;
	a.a_reserved. = reserved.;
	a.a_ = ;
	a.a_without = without;
	a.a_conditions = conditions;
	a.a_met: = met:;
	a.a_copyright = copyright;
	a.a_disclaimer. = disclaimer.;
	a.a_copyright = copyright;
	a.a_the = the;
	a.a_distribution. = distribution.;
	a.a_software = software;
	a.a_acknowledgement: = acknowledgement:;
	a.a_of = of;
	a.a_contributors. = contributors.;
	a.a_contributors = contributors;
	a.a_software = software;
	a.a_permission. = permission.;
	a.a_ = ;
	a.a_AND = AND;
	a.a_THE = THE;
	a.a_PURPOSE = PURPOSE;
	a.a_LIABLE = LIABLE;
	a.a_CONSEQUENTIAL = CONSEQUENTIAL;
	a.a_GOODS = GOODS;
	a.a_INTERRUPTION) = INTERRUPTION);
	a.a_STRICT = STRICT;
	a.a_WAY = WAY;
	a.a_OF = OF;
	a.a_DAMAGE. = DAMAGE.;
	a.a_ = ;
	a.a_ = ;
	a.a_$' = $';
	a.a_ = ;
	a.a_ = ;
	a.a_then = then;
	a.a_srcfile' = srcfile';
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_2>/dev/null` = 2>/dev/null`;
	a.a_ = ;
	a.a_then = then;
	a.a_it. = it.;
	a.a_ = ;
	a.a_ = ;
	a.a_toupper() = toupper();
	a.a_ = ;
	a.a_{ = {;
	a.a_A-Z" = A-Z";
	a.a__toupper_str = _toupper_str;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_/' = /';
	a.a_ = ;
	a.a_} = };
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_i=2; = i=2;;
	a.a_{ = {;
	a.a_1 = 1;
	a.a_ = ;
	a.a_ = ;
	a.a_0 = 0;
	a.a_i++; = i++;;
	a.a_{ = {;
	a.a_"$i = "$i;
	a.a_ = ;
	a.a_ = ;
	a.a_$i = $i;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_automatically. = automatically.;
	a.a_lost!) = lost!);
	a.a_ = ;
	a.a_script: = script:;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_1>&2 = 1>&2;
	a.a_$out_h = $out_h;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_vop_default_desc = vop_default_desc;
	a.a_ = ;
	a.a_ = ;
	a.a_"$toupper"' = "$toupper"';
	a.a_{ = {;
	a.a_descriptor. = descriptor.;
	a.a_name) = name);
	a.a_\n"); = \n");;
	a.a_{ = {;
	a.a_argname[i]); = argname[i]);;
	a.a_ = ;
	a.a_ = ;
	a.a_name); = name);;
	a.a_it. = it.;
	a.a_toupper(name)) = toupper(name));
	a.a_length(protoarg) = length(protoarg);
	a.a_ = ;
	a.a_{ = {;
	a.a_argtype[i]) = argtype[i]);
	a.a_") = ");
	a.a_length(protoarg) = length(protoarg);
	a.a_{ = {;
	a.a_protoarg) = protoarg);
	a.a_4 = 4;
	a.a_0 = 0;
	a.a_ = ;
	a.a_ = ;
	a.a_arglen = arglen;
	a.a_ = ;
	a.a_ = ;
	a.a_function. = function.;
	a.a_toupper(name)) = toupper(name));
	a.a_{ = {;
	a.a_ = ;
	a.a_") = ");
	a.a_ = ;
	a.a_ = ;
	a.a_{ = {;
	a.a_argname[i]); = argname[i]);;
	a.a_ = ;
	a.a_name); = name);;
	a.a_name); = name);;
	a.a_{ = {;
	a.a_argname[i]); = argname[i]);;
	a.a_ = ;
	a.a_\n}\n", = \n}\n",;
	a.a_name) = name);
	a.a_ = ;
	a.a_ = ;
	return (VCALL(dvp, VOFFSET(vop_whiteout), &a));
}

struct vop_whiteout_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct componentname *a_cnp;
	int a_flags;
	 a_;
	 a_;
	braam+@pachelbel.coda.cs.cmu.edu Wed Oct 9 18:08:18 a_1996;
	braam+@pachelbel.coda.cs.cmu.edu a_;
	from pachelbel.coda.cs.cmu.edu (PACHELBEL.CODA.CS.CMU.EDU [128.2.222.26]) by telemann.coda.cs.cmu.edu (8.7.4/8.7.3) with SMTP id SAA09451 for <braam@telemann.coda.cs.cmu.edu> Wed, 9 Oct 1996 18:08:17 a_-0400;
	braam+@pachelbel.coda.cs.cmu.edu a_;
	<199610092208.SAA09451@telemann.coda.cs.cmu.edu> a_;
	braam@pachelbel.coda.cs.cmu.edu a_;
	Wed, 9 Oct 96 18:07:56 a_EDT;
	braam+@pachelbel.coda.cs.cmu.edu a_;
	 a_;
	 a_;
	 a_;
	Copyright (c) 1992, a_1993;
	The Regents of the University of California. All rights a_reserved.;
	 a_;
	Redistribution and use in source and binary forms, with or a_without;
	modification, are permitted provided that the following a_conditions;
	are a_met:;
	1. Redistributions of source code must retain the above a_copyright;
	notice, this list of conditions and the following a_disclaimer.;
	2. Redistributions in binary form must reproduce the above a_copyright;
	notice, this list of conditions and the following disclaimer in a_the;
	documentation and/or other materials provided with the a_distribution.;
	3. All advertising materials mentioning features or use of this a_software;
	must display the following a_acknowledgement:;
	This product includes software developed by the University a_of;
	California, Berkeley and its a_contributors.;
	4. Neither the name of the University nor the names of its a_contributors;
	may be used to endorse or promote products derived from this a_software;
	without specific prior written a_permission.;
	 a_;
	THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' a_AND;
	ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, a_THE;
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR a_PURPOSE;
	ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE a_LIABLE;
	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR a_CONSEQUENTIAL;
	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE a_GOODS;
	OR SERVICES LOSS OF USE, DATA, OR PROFITS; OR BUSINESS a_INTERRUPTION);
	HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, a_STRICT;
	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY a_WAY;
	OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY a_OF;
	SUCH a_DAMAGE.;
	 a_;
	 a_;
	vnode_if.sh,v 1.8 1995/03/10 04:13:52 chopps Exp a_$';
	 a_;
	 a_;
	[ $# -ne 1 ] a_then;
	'usage: vnode_if.sh a_srcfile';
	1 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	'BEGIN { print toupper("true") exit; }' a_2>/dev/null`;
	 a_;
	[ "$isgawk" = TRUE ] a_then;
	GNU awk provides a_it.;
	 a_;
	 a_;
	Provide our own a_toupper();
	 a_;
	toupper(str) a_{;
	= "echo "str" |tr a-z a_A-Z";
	| getline a__toupper_str;
	 a_;
	_toupper_str a_;
	 a_;
	 a_;
	\1:g a_;
	/ a_/';
	 a_;
	{ next a_};
	{ a_;
	 a_;
	 a_;
	 a_;
	{ a_;
	 a_;
	 a_;
	 a_;
	= $1 a_i=2;;
	($2 == "WILLRELE") a_{;
	= a_1;
	 a_;
	else a_;
	= a_0;
	= $i a_i++;;
	(i < NF) a_{;
	= argtype[argc]" a_"$i;
	 a_;
	 a_;
	= a_$i;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	Warning: This file is generated a_automatically.;
	(Modifications made here may easily be a_lost!);
	 a_;
	Created by the a_script:;
	${SCRIPT_ID} a_;
	 a_;
	 a_;
	 a_;
	:\1:g' a_;
	 a_;
	"$0: Creating $out_h" a_1>&2;
	> a_$out_h;
	 a_;
	"$copyright" a_;
	"$warning" a_;
	' a_;
	struct vnodeop_desc a_vop_default_desc;
	 a_;
	 a_;
	-e "$sed_prep" $src | $awk a_"$toupper"';
	doit() a_{;
	Declare arg struct, a_descriptor.;
	%s_args {\n", a_name);
	vnodeop_desc *a_desc a_\n");;
	(i=0 i<argc; i++) a_{;
	a_%s \n", argtype[i], a_argname[i]);;
	 a_;
	\n"); a_;
	struct vnodeop_desc %s_desc \n", a_name);;
	Prototype a_it.;
	= sprintf("static __inline int %s __P((", a_toupper(name));
	= a_length(protoarg);
	protoarg) a_;
	(i=0 i<argc; i++) a_{;
	= sprintf("%s", a_argtype[i]);
	(i < (argc-1)) protoarg = (protoarg ", a_");
	= a_length(protoarg);
	((protolen + arglen) > 77) a_{;
	= ("\n " a_protoarg);
	+= a_4;
	= a_0;
	 a_;
	protoarg) a_;
	+= a_arglen;
	 a_;
	\n"); a_;
	Define inline a_function.;
	__inline int %s(", a_toupper(name));
	(i=0 i<argc; i++) a_{;
	argname[i]) a_;
	(i < (argc-1)) printf(", a_");
	 a_;
	 a_;
	(i=0 i<argc; i++) a_{;
	%s \n", argtype[i], a_argname[i]);;
	 a_;
	%s_args a \n", a_name);;
	= VDESC(%s) \n", a_name);;
	(i=0 i<argc; i++) a_{;
	= %s \n", argname[i], a_argname[i]);;
	 a_;
	(VCALL(%s%s, VOFFSET(%s), &a)) a_\n}\n",;
	arg0special, a_name);
	{ a_;
	 a_;
	{ a_;
	Special cases: */\n#include a_<sys/buf.h>\n");
	 a_;
	buf *a_";
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
};
extern struct vnodeop_desc vop_whiteout_desc;
static __inline int VOP_WHITEOUT __P((struct vnode *, 
    struct componentname *, int, , , 
    braam+@pachelbel.coda.cs.cmu.edu Wed Oct 9 18:08:18, 
    braam+@pachelbel.coda.cs.cmu.edu, 
    from pachelbel.coda.cs.cmu.edu (PACHELBEL.CODA.CS.CMU.EDU [128.2.222.26]) by telemann.coda.cs.cmu.edu (8.7.4/8.7.3) with SMTP id SAA09451 for <braam@telemann.coda.cs.cmu.edu> Wed, 9 Oct 1996 18:08:17, 
    braam+@pachelbel.coda.cs.cmu.edu, 
    <199610092208.SAA09451@telemann.coda.cs.cmu.edu>, 
    braam@pachelbel.coda.cs.cmu.edu, Wed, 9 Oct 96 18:07:56, 
    braam+@pachelbel.coda.cs.cmu.edu, , , , Copyright (c) 1992,, 
    The Regents of the University of California. All rights, , 
    Redistribution and use in source and binary forms, with or, 
    modification, are permitted provided that the following, are, 
    1. Redistributions of source code must retain the above, 
    notice, this list of conditions and the following, 
    2. Redistributions in binary form must reproduce the above, 
    notice, this list of conditions and the following disclaimer in, 
    documentation and/or other materials provided with the, 
    3. All advertising materials mentioning features or use of this, 
    must display the following, 
    This product includes software developed by the University, 
    California, Berkeley and its, 
    4. Neither the name of the University nor the names of its, 
    may be used to endorse or promote products derived from this, 
    without specific prior written, , 
    THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'', 
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,, 
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR, 
    ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE, 
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR, 
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE, 
    OR SERVICES LOSS OF USE, DATA, OR PROFITS; OR BUSINESS, 
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,, 
    LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY, 
    OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY, 
    SUCH, , , vnode_if.sh,v 1.8 1995/03/10 04:13:52 chopps Exp, , , 
    [ $# -ne 1 ], 'usage: vnode_if.sh, 1, , , , , , , , , , 
    'BEGIN { print toupper("true") exit; }', , [ "$isgawk" = TRUE ], 
    GNU awk provides, , , Provide our own, , toupper(str), 
    = "echo "str" |tr a-z, | getline, , _toupper_str, , , \1:g, /, , { next, 
    {, , , , {, , , , = $1, ($2 == "WILLRELE"), =, , else, =, = $i, 
    (i < NF), = argtype[argc]", , , =, , , , , , , 
    Warning: This file is generated, (Modifications made here may easily be, 
    , Created by the, ${SCRIPT_ID}, , , , :\1:g', , "$0: Creating $out_h", 
    >, , "$copyright", "$warning", ', struct vnodeop_desc, , , 
    -e "$sed_prep" $src | $awk, doit(), Declare arg struct,, %s_args {\n",, 
    vnodeop_desc *a_desc, (i=0 i<argc; i++), a_%s \n", argtype[i],, , 
    \n");, struct vnodeop_desc %s_desc \n",, Prototype, 
    = sprintf("static __inline int %s __P((",, =, protoarg), 
    (i=0 i<argc; i++), = sprintf("%s",, 
    (i < (argc-1)) protoarg = (protoarg ",, =, ((protolen + arglen) > 77), 
    = ("\n ", +=, =, , protoarg), +=, , \n");, Define inline, 
    __inline int %s(",, (i=0 i<argc; i++), argname[i]), 
    (i < (argc-1)) printf(",, , , (i=0 i<argc; i++), %s \n", argtype[i],, , 
    %s_args a \n",, = VDESC(%s) \n",, (i=0 i<argc; i++), 
    = %s \n", argname[i],, , (VCALL(%s%s, VOFFSET(%s), &a)), arg0special,, 
    {, , {, Special cases: */\n#include, , buf *, , , , , , ));
static __inline int VOP_WHITEOUT(dvp, cnp, flags, , , 1996, , -0400, , , , EDT, , , , , 1993, reserved., , without, conditions, met:, copyright, disclaimer., copyright, the, distribution., software, acknowledgement:, of, contributors., contributors, software, permission., , AND, THE, PURPOSE, LIABLE, CONSEQUENTIAL, GOODS, INTERRUPTION), STRICT, WAY, OF, DAMAGE., , , $', , , then, srcfile', , , , , , , , , , , 2>/dev/null`, , then, it., , , toupper(), , {, A-Z", _toupper_str, , , , , , /', , }, , , , , , , , , i=2;, {, 1, , , 0, i++;, {, "$i, , , $i, , , , , , , automatically., lost!), , script:, , , , , , , 1>&2, $out_h, , , , , vop_default_desc, , , "$toupper"', {, descriptor., name), \n");, {, argname[i]);, , , name);, it., toupper(name)), length(protoarg), , {, argtype[i]), "), length(protoarg), {, protoarg), 4, 0, , , arglen, , , function., toupper(name)), {, , "), , , {, argname[i]);, , name);, name);, {, argname[i]);, , \n}\n",, name), , , , <sys/buf.h>\n"), , ", , , , , , )
	struct vnode *dvp;
	struct componentname *cnp;
	int flags;
	 ;
	 ;
	braam+@pachelbel.coda.cs.cmu.edu Wed Oct 9 18:08:18 1996;
	braam+@pachelbel.coda.cs.cmu.edu ;
	from pachelbel.coda.cs.cmu.edu (PACHELBEL.CODA.CS.CMU.EDU [128.2.222.26]) by telemann.coda.cs.cmu.edu (8.7.4/8.7.3) with SMTP id SAA09451 for <braam@telemann.coda.cs.cmu.edu> Wed, 9 Oct 1996 18:08:17 -0400;
	braam+@pachelbel.coda.cs.cmu.edu ;
	<199610092208.SAA09451@telemann.coda.cs.cmu.edu> ;
	braam@pachelbel.coda.cs.cmu.edu ;
	Wed, 9 Oct 96 18:07:56 EDT;
	braam+@pachelbel.coda.cs.cmu.edu ;
	 ;
	 ;
	 ;
	Copyright (c) 1992, 1993;
	The Regents of the University of California. All rights reserved.;
	 ;
	Redistribution and use in source and binary forms, with or without;
	modification, are permitted provided that the following conditions;
	are met:;
	1. Redistributions of source code must retain the above copyright;
	notice, this list of conditions and the following disclaimer.;
	2. Redistributions in binary form must reproduce the above copyright;
	notice, this list of conditions and the following disclaimer in the;
	documentation and/or other materials provided with the distribution.;
	3. All advertising materials mentioning features or use of this software;
	must display the following acknowledgement:;
	This product includes software developed by the University of;
	California, Berkeley and its contributors.;
	4. Neither the name of the University nor the names of its contributors;
	may be used to endorse or promote products derived from this software;
	without specific prior written permission.;
	 ;
	THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND;
	ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE;
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE;
	ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE;
	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL;
	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS;
	OR SERVICES LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION);
	HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT;
	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY;
	OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF;
	SUCH DAMAGE.;
	 ;
	 ;
	vnode_if.sh,v 1.8 1995/03/10 04:13:52 chopps Exp $';
	 ;
	 ;
	[ $# -ne 1 ] then;
	'usage: vnode_if.sh srcfile';
	1 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	'BEGIN { print toupper("true") exit; }' 2>/dev/null`;
	 ;
	[ "$isgawk" = TRUE ] then;
	GNU awk provides it.;
	 ;
	 ;
	Provide our own toupper();
	 ;
	toupper(str) {;
	= "echo "str" |tr a-z A-Z";
	| getline _toupper_str;
	 ;
	_toupper_str ;
	 ;
	 ;
	\1:g ;
	/ /';
	 ;
	{ next };
	{ ;
	 ;
	 ;
	 ;
	{ ;
	 ;
	 ;
	 ;
	= $1 i=2;;
	($2 == "WILLRELE") {;
	= 1;
	 ;
	else ;
	= 0;
	= $i i++;;
	(i < NF) {;
	= argtype[argc]" "$i;
	 ;
	 ;
	= $i;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	Warning: This file is generated automatically.;
	(Modifications made here may easily be lost!);
	 ;
	Created by the script:;
	${SCRIPT_ID} ;
	 ;
	 ;
	 ;
	:\1:g' ;
	 ;
	"$0: Creating $out_h" 1>&2;
	> $out_h;
	 ;
	"$copyright" ;
	"$warning" ;
	' ;
	struct vnodeop_desc vop_default_desc;
	 ;
	 ;
	-e "$sed_prep" $src | $awk "$toupper"';
	doit() {;
	Declare arg struct, descriptor.;
	%s_args {\n", name);
	vnodeop_desc *a_desc \n");;
	(i=0 i<argc; i++) {;
	a_%s \n", argtype[i], argname[i]);;
	 ;
	\n"); ;
	struct vnodeop_desc %s_desc \n", name);;
	Prototype it.;
	= sprintf("static __inline int %s __P((", toupper(name));
	= length(protoarg);
	protoarg) ;
	(i=0 i<argc; i++) {;
	= sprintf("%s", argtype[i]);
	(i < (argc-1)) protoarg = (protoarg ", ");
	= length(protoarg);
	((protolen + arglen) > 77) {;
	= ("\n " protoarg);
	+= 4;
	= 0;
	 ;
	protoarg) ;
	+= arglen;
	 ;
	\n"); ;
	Define inline function.;
	__inline int %s(", toupper(name));
	(i=0 i<argc; i++) {;
	argname[i]) ;
	(i < (argc-1)) printf(", ");
	 ;
	 ;
	(i=0 i<argc; i++) {;
	%s \n", argtype[i], argname[i]);;
	 ;
	%s_args a \n", name);;
	= VDESC(%s) \n", name);;
	(i=0 i<argc; i++) {;
	= %s \n", argname[i], argname[i]);;
	 ;
	(VCALL(%s%s, VOFFSET(%s), &a)) \n}\n",;
	arg0special, name);
	{ ;
	 ;
	{ ;
	Special cases: */\n#include <sys/buf.h>\n");
	 ;
	buf *";
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
{
	struct vop_whiteout_args a;
	a.a_desc = VDESC(vop_whiteout);
	a.a_dvp = dvp;
	a.a_cnp = cnp;
	a.a_flags = flags;
	a.a_ = ;
	a.a_ = ;
	a.a_1996 = 1996;
	a.a_ = ;
	a.a_-0400 = -0400;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_EDT = EDT;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_1993 = 1993;
	a.a_reserved. = reserved.;
	a.a_ = ;
	a.a_without = without;
	a.a_conditions = conditions;
	a.a_met: = met:;
	a.a_copyright = copyright;
	a.a_disclaimer. = disclaimer.;
	a.a_copyright = copyright;
	a.a_the = the;
	a.a_distribution. = distribution.;
	a.a_software = software;
	a.a_acknowledgement: = acknowledgement:;
	a.a_of = of;
	a.a_contributors. = contributors.;
	a.a_contributors = contributors;
	a.a_software = software;
	a.a_permission. = permission.;
	a.a_ = ;
	a.a_AND = AND;
	a.a_THE = THE;
	a.a_PURPOSE = PURPOSE;
	a.a_LIABLE = LIABLE;
	a.a_CONSEQUENTIAL = CONSEQUENTIAL;
	a.a_GOODS = GOODS;
	a.a_INTERRUPTION) = INTERRUPTION);
	a.a_STRICT = STRICT;
	a.a_WAY = WAY;
	a.a_OF = OF;
	a.a_DAMAGE. = DAMAGE.;
	a.a_ = ;
	a.a_ = ;
	a.a_$' = $';
	a.a_ = ;
	a.a_ = ;
	a.a_then = then;
	a.a_srcfile' = srcfile';
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_2>/dev/null` = 2>/dev/null`;
	a.a_ = ;
	a.a_then = then;
	a.a_it. = it.;
	a.a_ = ;
	a.a_ = ;
	a.a_toupper() = toupper();
	a.a_ = ;
	a.a_{ = {;
	a.a_A-Z" = A-Z";
	a.a__toupper_str = _toupper_str;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_/' = /';
	a.a_ = ;
	a.a_} = };
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_i=2; = i=2;;
	a.a_{ = {;
	a.a_1 = 1;
	a.a_ = ;
	a.a_ = ;
	a.a_0 = 0;
	a.a_i++; = i++;;
	a.a_{ = {;
	a.a_"$i = "$i;
	a.a_ = ;
	a.a_ = ;
	a.a_$i = $i;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_automatically. = automatically.;
	a.a_lost!) = lost!);
	a.a_ = ;
	a.a_script: = script:;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_1>&2 = 1>&2;
	a.a_$out_h = $out_h;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_vop_default_desc = vop_default_desc;
	a.a_ = ;
	a.a_ = ;
	a.a_"$toupper"' = "$toupper"';
	a.a_{ = {;
	a.a_descriptor. = descriptor.;
	a.a_name) = name);
	a.a_\n"); = \n");;
	a.a_{ = {;
	a.a_argname[i]); = argname[i]);;
	a.a_ = ;
	a.a_ = ;
	a.a_name); = name);;
	a.a_it. = it.;
	a.a_toupper(name)) = toupper(name));
	a.a_length(protoarg) = length(protoarg);
	a.a_ = ;
	a.a_{ = {;
	a.a_argtype[i]) = argtype[i]);
	a.a_") = ");
	a.a_length(protoarg) = length(protoarg);
	a.a_{ = {;
	a.a_protoarg) = protoarg);
	a.a_4 = 4;
	a.a_0 = 0;
	a.a_ = ;
	a.a_ = ;
	a.a_arglen = arglen;
	a.a_ = ;
	a.a_ = ;
	a.a_function. = function.;
	a.a_toupper(name)) = toupper(name));
	a.a_{ = {;
	a.a_ = ;
	a.a_") = ");
	a.a_ = ;
	a.a_ = ;
	a.a_{ = {;
	a.a_argname[i]); = argname[i]);;
	a.a_ = ;
	a.a_name); = name);;
	a.a_name); = name);;
	a.a_{ = {;
	a.a_argname[i]); = argname[i]);;
	a.a_ = ;
	a.a_\n}\n", = \n}\n",;
	a.a_name) = name);
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_<sys/buf.h>\n") = <sys/buf.h>\n");
	a.a_ = ;
	a.a_" = ";
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	return (VCALL(dvp, VOFFSET(vop_whiteout), &a));
}

struct vop_whiteout_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct componentname *a_cnp;
	int a_flags;
	 a_;
	 a_;
	braam+@pachelbel.coda.cs.cmu.edu Wed Oct 9 18:08:18 a_1996;
	braam+@pachelbel.coda.cs.cmu.edu a_;
	from pachelbel.coda.cs.cmu.edu (PACHELBEL.CODA.CS.CMU.EDU [128.2.222.26]) by telemann.coda.cs.cmu.edu (8.7.4/8.7.3) with SMTP id SAA09451 for <braam@telemann.coda.cs.cmu.edu> Wed, 9 Oct 1996 18:08:17 a_-0400;
	braam+@pachelbel.coda.cs.cmu.edu a_;
	<199610092208.SAA09451@telemann.coda.cs.cmu.edu> a_;
	braam@pachelbel.coda.cs.cmu.edu a_;
	Wed, 9 Oct 96 18:07:56 a_EDT;
	braam+@pachelbel.coda.cs.cmu.edu a_;
	 a_;
	 a_;
	 a_;
	Copyright (c) 1992, a_1993;
	The Regents of the University of California. All rights a_reserved.;
	 a_;
	Redistribution and use in source and binary forms, with or a_without;
	modification, are permitted provided that the following a_conditions;
	are a_met:;
	1. Redistributions of source code must retain the above a_copyright;
	notice, this list of conditions and the following a_disclaimer.;
	2. Redistributions in binary form must reproduce the above a_copyright;
	notice, this list of conditions and the following disclaimer in a_the;
	documentation and/or other materials provided with the a_distribution.;
	3. All advertising materials mentioning features or use of this a_software;
	must display the following a_acknowledgement:;
	This product includes software developed by the University a_of;
	California, Berkeley and its a_contributors.;
	4. Neither the name of the University nor the names of its a_contributors;
	may be used to endorse or promote products derived from this a_software;
	without specific prior written a_permission.;
	 a_;
	THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' a_AND;
	ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, a_THE;
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR a_PURPOSE;
	ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE a_LIABLE;
	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR a_CONSEQUENTIAL;
	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE a_GOODS;
	OR SERVICES LOSS OF USE, DATA, OR PROFITS; OR BUSINESS a_INTERRUPTION);
	HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, a_STRICT;
	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY a_WAY;
	OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY a_OF;
	SUCH a_DAMAGE.;
	 a_;
	 a_;
	vnode_if.sh,v 1.8 1995/03/10 04:13:52 chopps Exp a_$';
	 a_;
	 a_;
	[ $# -ne 1 ] a_then;
	'usage: vnode_if.sh a_srcfile';
	1 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	'BEGIN { print toupper("true") exit; }' a_2>/dev/null`;
	 a_;
	[ "$isgawk" = TRUE ] a_then;
	GNU awk provides a_it.;
	 a_;
	 a_;
	Provide our own a_toupper();
	 a_;
	toupper(str) a_{;
	= "echo "str" |tr a-z a_A-Z";
	| getline a__toupper_str;
	 a_;
	_toupper_str a_;
	 a_;
	 a_;
	\1:g a_;
	/ a_/';
	 a_;
	{ next a_};
	{ a_;
	 a_;
	 a_;
	 a_;
	{ a_;
	 a_;
	 a_;
	 a_;
	= $1 a_i=2;;
	($2 == "WILLRELE") a_{;
	= a_1;
	 a_;
	else a_;
	= a_0;
	= $i a_i++;;
	(i < NF) a_{;
	= argtype[argc]" a_"$i;
	 a_;
	 a_;
	= a_$i;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	Warning: This file is generated a_automatically.;
	(Modifications made here may easily be a_lost!);
	 a_;
	Created by the a_script:;
	${SCRIPT_ID} a_;
	 a_;
	 a_;
	 a_;
	:\1:g' a_;
	 a_;
	"$0: Creating $out_h" a_1>&2;
	> a_$out_h;
	 a_;
	"$copyright" a_;
	"$warning" a_;
	' a_;
	struct vnodeop_desc a_vop_default_desc;
	 a_;
	 a_;
	-e "$sed_prep" $src | $awk a_"$toupper"';
	doit() a_{;
	Declare arg struct, a_descriptor.;
	%s_args {\n", a_name);
	vnodeop_desc *a_desc a_\n");;
	(i=0 i<argc; i++) a_{;
	a_%s \n", argtype[i], a_argname[i]);;
	 a_;
	\n"); a_;
	struct vnodeop_desc %s_desc \n", a_name);;
	Prototype a_it.;
	= sprintf("static __inline int %s __P((", a_toupper(name));
	= a_length(protoarg);
	protoarg) a_;
	(i=0 i<argc; i++) a_{;
	= sprintf("%s", a_argtype[i]);
	(i < (argc-1)) protoarg = (protoarg ", a_");
	= a_length(protoarg);
	((protolen + arglen) > 77) a_{;
	= ("\n " a_protoarg);
	+= a_4;
	= a_0;
	 a_;
	protoarg) a_;
	+= a_arglen;
	 a_;
	\n"); a_;
	Define inline a_function.;
	__inline int %s(", a_toupper(name));
	(i=0 i<argc; i++) a_{;
	argname[i]) a_;
	(i < (argc-1)) printf(", a_");
	 a_;
	 a_;
	(i=0 i<argc; i++) a_{;
	%s \n", argtype[i], a_argname[i]);;
	 a_;
	%s_args a \n", a_name);;
	= VDESC(%s) \n", a_name);;
	(i=0 i<argc; i++) a_{;
	= %s \n", argname[i], a_argname[i]);;
	 a_;
	(VCALL(%s%s, VOFFSET(%s), &a)) a_\n}\n",;
	arg0special, a_name);
	{ a_;
	 a_;
	{ a_;
	Special cases: */\n#include a_<sys/buf.h>\n");
	 a_;
	buf *a_";
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	| sed -e a_"$anal_retentive";
	 a_;
	' a_;
	End of special cases. a_*/';
	 a_;
	 a_;
	"$0: Creating $out_c" a_1>&2;
	> a_$out_c;
	 a_;
	"$copyright" a_;
	"$warning" a_;
	' a_;
	 a_;
	vnodeop_desc vop_default_desc = a_{;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
};
extern struct vnodeop_desc vop_whiteout_desc;
static __inline int VOP_WHITEOUT __P((struct vnode *, 
    struct componentname *, int, , , 
    braam+@pachelbel.coda.cs.cmu.edu Wed Oct 9 18:08:18, 
    braam+@pachelbel.coda.cs.cmu.edu, 
    from pachelbel.coda.cs.cmu.edu (PACHELBEL.CODA.CS.CMU.EDU [128.2.222.26]) by telemann.coda.cs.cmu.edu (8.7.4/8.7.3) with SMTP id SAA09451 for <braam@telemann.coda.cs.cmu.edu> Wed, 9 Oct 1996 18:08:17, 
    braam+@pachelbel.coda.cs.cmu.edu, 
    <199610092208.SAA09451@telemann.coda.cs.cmu.edu>, 
    braam@pachelbel.coda.cs.cmu.edu, Wed, 9 Oct 96 18:07:56, 
    braam+@pachelbel.coda.cs.cmu.edu, , , , Copyright (c) 1992,, 
    The Regents of the University of California. All rights, , 
    Redistribution and use in source and binary forms, with or, 
    modification, are permitted provided that the following, are, 
    1. Redistributions of source code must retain the above, 
    notice, this list of conditions and the following, 
    2. Redistributions in binary form must reproduce the above, 
    notice, this list of conditions and the following disclaimer in, 
    documentation and/or other materials provided with the, 
    3. All advertising materials mentioning features or use of this, 
    must display the following, 
    This product includes software developed by the University, 
    California, Berkeley and its, 
    4. Neither the name of the University nor the names of its, 
    may be used to endorse or promote products derived from this, 
    without specific prior written, , 
    THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'', 
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,, 
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR, 
    ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE, 
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR, 
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE, 
    OR SERVICES LOSS OF USE, DATA, OR PROFITS; OR BUSINESS, 
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,, 
    LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY, 
    OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY, 
    SUCH, , , vnode_if.sh,v 1.8 1995/03/10 04:13:52 chopps Exp, , , 
    [ $# -ne 1 ], 'usage: vnode_if.sh, 1, , , , , , , , , , 
    'BEGIN { print toupper("true") exit; }', , [ "$isgawk" = TRUE ], 
    GNU awk provides, , , Provide our own, , toupper(str), 
    = "echo "str" |tr a-z, | getline, , _toupper_str, , , \1:g, /, , { next, 
    {, , , , {, , , , = $1, ($2 == "WILLRELE"), =, , else, =, = $i, 
    (i < NF), = argtype[argc]", , , =, , , , , , , 
    Warning: This file is generated, (Modifications made here may easily be, 
    , Created by the, ${SCRIPT_ID}, , , , :\1:g', , "$0: Creating $out_h", 
    >, , "$copyright", "$warning", ', struct vnodeop_desc, , , 
    -e "$sed_prep" $src | $awk, doit(), Declare arg struct,, %s_args {\n",, 
    vnodeop_desc *a_desc, (i=0 i<argc; i++), a_%s \n", argtype[i],, , 
    \n");, struct vnodeop_desc %s_desc \n",, Prototype, 
    = sprintf("static __inline int %s __P((",, =, protoarg), 
    (i=0 i<argc; i++), = sprintf("%s",, 
    (i < (argc-1)) protoarg = (protoarg ",, =, ((protolen + arglen) > 77), 
    = ("\n ", +=, =, , protoarg), +=, , \n");, Define inline, 
    __inline int %s(",, (i=0 i<argc; i++), argname[i]), 
    (i < (argc-1)) printf(",, , , (i=0 i<argc; i++), %s \n", argtype[i],, , 
    %s_args a \n",, = VDESC(%s) \n",, (i=0 i<argc; i++), 
    = %s \n", argname[i],, , (VCALL(%s%s, VOFFSET(%s), &a)), arg0special,, 
    {, , {, Special cases: */\n#include, , buf *, , , , , , , | sed -e, , ', 
    End of special cases., , , "$0: Creating $out_c", >, , "$copyright", 
    "$warning", ', , vnodeop_desc vop_default_desc =, , , , , , , , , ));
static __inline int VOP_WHITEOUT(dvp, cnp, flags, , , 1996, , -0400, , , , EDT, , , , , 1993, reserved., , without, conditions, met:, copyright, disclaimer., copyright, the, distribution., software, acknowledgement:, of, contributors., contributors, software, permission., , AND, THE, PURPOSE, LIABLE, CONSEQUENTIAL, GOODS, INTERRUPTION), STRICT, WAY, OF, DAMAGE., , , $', , , then, srcfile', , , , , , , , , , , 2>/dev/null`, , then, it., , , toupper(), , {, A-Z", _toupper_str, , , , , , /', , }, , , , , , , , , i=2;, {, 1, , , 0, i++;, {, "$i, , , $i, , , , , , , automatically., lost!), , script:, , , , , , , 1>&2, $out_h, , , , , vop_default_desc, , , "$toupper"', {, descriptor., name), \n");, {, argname[i]);, , , name);, it., toupper(name)), length(protoarg), , {, argtype[i]), "), length(protoarg), {, protoarg), 4, 0, , , arglen, , , function., toupper(name)), {, , "), , , {, argname[i]);, , name);, name);, {, argname[i]);, , \n}\n",, name), , , , <sys/buf.h>\n"), , ", , , , , , , "$anal_retentive", , , */', , , 1>&2, $out_c, , , , , , {, , , , , , , , , )
	struct vnode *dvp;
	struct componentname *cnp;
	int flags;
	 ;
	 ;
	braam+@pachelbel.coda.cs.cmu.edu Wed Oct 9 18:08:18 1996;
	braam+@pachelbel.coda.cs.cmu.edu ;
	from pachelbel.coda.cs.cmu.edu (PACHELBEL.CODA.CS.CMU.EDU [128.2.222.26]) by telemann.coda.cs.cmu.edu (8.7.4/8.7.3) with SMTP id SAA09451 for <braam@telemann.coda.cs.cmu.edu> Wed, 9 Oct 1996 18:08:17 -0400;
	braam+@pachelbel.coda.cs.cmu.edu ;
	<199610092208.SAA09451@telemann.coda.cs.cmu.edu> ;
	braam@pachelbel.coda.cs.cmu.edu ;
	Wed, 9 Oct 96 18:07:56 EDT;
	braam+@pachelbel.coda.cs.cmu.edu ;
	 ;
	 ;
	 ;
	Copyright (c) 1992, 1993;
	The Regents of the University of California. All rights reserved.;
	 ;
	Redistribution and use in source and binary forms, with or without;
	modification, are permitted provided that the following conditions;
	are met:;
	1. Redistributions of source code must retain the above copyright;
	notice, this list of conditions and the following disclaimer.;
	2. Redistributions in binary form must reproduce the above copyright;
	notice, this list of conditions and the following disclaimer in the;
	documentation and/or other materials provided with the distribution.;
	3. All advertising materials mentioning features or use of this software;
	must display the following acknowledgement:;
	This product includes software developed by the University of;
	California, Berkeley and its contributors.;
	4. Neither the name of the University nor the names of its contributors;
	may be used to endorse or promote products derived from this software;
	without specific prior written permission.;
	 ;
	THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND;
	ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE;
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE;
	ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE;
	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL;
	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS;
	OR SERVICES LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION);
	HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT;
	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY;
	OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF;
	SUCH DAMAGE.;
	 ;
	 ;
	vnode_if.sh,v 1.8 1995/03/10 04:13:52 chopps Exp $';
	 ;
	 ;
	[ $# -ne 1 ] then;
	'usage: vnode_if.sh srcfile';
	1 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	'BEGIN { print toupper("true") exit; }' 2>/dev/null`;
	 ;
	[ "$isgawk" = TRUE ] then;
	GNU awk provides it.;
	 ;
	 ;
	Provide our own toupper();
	 ;
	toupper(str) {;
	= "echo "str" |tr a-z A-Z";
	| getline _toupper_str;
	 ;
	_toupper_str ;
	 ;
	 ;
	\1:g ;
	/ /';
	 ;
	{ next };
	{ ;
	 ;
	 ;
	 ;
	{ ;
	 ;
	 ;
	 ;
	= $1 i=2;;
	($2 == "WILLRELE") {;
	= 1;
	 ;
	else ;
	= 0;
	= $i i++;;
	(i < NF) {;
	= argtype[argc]" "$i;
	 ;
	 ;
	= $i;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	Warning: This file is generated automatically.;
	(Modifications made here may easily be lost!);
	 ;
	Created by the script:;
	${SCRIPT_ID} ;
	 ;
	 ;
	 ;
	:\1:g' ;
	 ;
	"$0: Creating $out_h" 1>&2;
	> $out_h;
	 ;
	"$copyright" ;
	"$warning" ;
	' ;
	struct vnodeop_desc vop_default_desc;
	 ;
	 ;
	-e "$sed_prep" $src | $awk "$toupper"';
	doit() {;
	Declare arg struct, descriptor.;
	%s_args {\n", name);
	vnodeop_desc *a_desc \n");;
	(i=0 i<argc; i++) {;
	a_%s \n", argtype[i], argname[i]);;
	 ;
	\n"); ;
	struct vnodeop_desc %s_desc \n", name);;
	Prototype it.;
	= sprintf("static __inline int %s __P((", toupper(name));
	= length(protoarg);
	protoarg) ;
	(i=0 i<argc; i++) {;
	= sprintf("%s", argtype[i]);
	(i < (argc-1)) protoarg = (protoarg ", ");
	= length(protoarg);
	((protolen + arglen) > 77) {;
	= ("\n " protoarg);
	+= 4;
	= 0;
	 ;
	protoarg) ;
	+= arglen;
	 ;
	\n"); ;
	Define inline function.;
	__inline int %s(", toupper(name));
	(i=0 i<argc; i++) {;
	argname[i]) ;
	(i < (argc-1)) printf(", ");
	 ;
	 ;
	(i=0 i<argc; i++) {;
	%s \n", argtype[i], argname[i]);;
	 ;
	%s_args a \n", name);;
	= VDESC(%s) \n", name);;
	(i=0 i<argc; i++) {;
	= %s \n", argname[i], argname[i]);;
	 ;
	(VCALL(%s%s, VOFFSET(%s), &a)) \n}\n",;
	arg0special, name);
	{ ;
	 ;
	{ ;
	Special cases: */\n#include <sys/buf.h>\n");
	 ;
	buf *";
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	| sed -e "$anal_retentive";
	 ;
	' ;
	End of special cases. */';
	 ;
	 ;
	"$0: Creating $out_c" 1>&2;
	> $out_c;
	 ;
	"$copyright" ;
	"$warning" ;
	' ;
	 ;
	vnodeop_desc vop_default_desc = {;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
{
	struct vop_whiteout_args a;
	a.a_desc = VDESC(vop_whiteout);
	a.a_dvp = dvp;
	a.a_cnp = cnp;
	a.a_flags = flags;
	a.a_ = ;
	a.a_ = ;
	a.a_1996 = 1996;
	a.a_ = ;
	a.a_-0400 = -0400;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_EDT = EDT;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_1993 = 1993;
	a.a_reserved. = reserved.;
	a.a_ = ;
	a.a_without = without;
	a.a_conditions = conditions;
	a.a_met: = met:;
	a.a_copyright = copyright;
	a.a_disclaimer. = disclaimer.;
	a.a_copyright = copyright;
	a.a_the = the;
	a.a_distribution. = distribution.;
	a.a_software = software;
	a.a_acknowledgement: = acknowledgement:;
	a.a_of = of;
	a.a_contributors. = contributors.;
	a.a_contributors = contributors;
	a.a_software = software;
	a.a_permission. = permission.;
	a.a_ = ;
	a.a_AND = AND;
	a.a_THE = THE;
	a.a_PURPOSE = PURPOSE;
	a.a_LIABLE = LIABLE;
	a.a_CONSEQUENTIAL = CONSEQUENTIAL;
	a.a_GOODS = GOODS;
	a.a_INTERRUPTION) = INTERRUPTION);
	a.a_STRICT = STRICT;
	a.a_WAY = WAY;
	a.a_OF = OF;
	a.a_DAMAGE. = DAMAGE.;
	a.a_ = ;
	a.a_ = ;
	a.a_$' = $';
	a.a_ = ;
	a.a_ = ;
	a.a_then = then;
	a.a_srcfile' = srcfile';
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_2>/dev/null` = 2>/dev/null`;
	a.a_ = ;
	a.a_then = then;
	a.a_it. = it.;
	a.a_ = ;
	a.a_ = ;
	a.a_toupper() = toupper();
	a.a_ = ;
	a.a_{ = {;
	a.a_A-Z" = A-Z";
	a.a__toupper_str = _toupper_str;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_/' = /';
	a.a_ = ;
	a.a_} = };
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_i=2; = i=2;;
	a.a_{ = {;
	a.a_1 = 1;
	a.a_ = ;
	a.a_ = ;
	a.a_0 = 0;
	a.a_i++; = i++;;
	a.a_{ = {;
	a.a_"$i = "$i;
	a.a_ = ;
	a.a_ = ;
	a.a_$i = $i;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_automatically. = automatically.;
	a.a_lost!) = lost!);
	a.a_ = ;
	a.a_script: = script:;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_1>&2 = 1>&2;
	a.a_$out_h = $out_h;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_vop_default_desc = vop_default_desc;
	a.a_ = ;
	a.a_ = ;
	a.a_"$toupper"' = "$toupper"';
	a.a_{ = {;
	a.a_descriptor. = descriptor.;
	a.a_name) = name);
	a.a_\n"); = \n");;
	a.a_{ = {;
	a.a_argname[i]); = argname[i]);;
	a.a_ = ;
	a.a_ = ;
	a.a_name); = name);;
	a.a_it. = it.;
	a.a_toupper(name)) = toupper(name));
	a.a_length(protoarg) = length(protoarg);
	a.a_ = ;
	a.a_{ = {;
	a.a_argtype[i]) = argtype[i]);
	a.a_") = ");
	a.a_length(protoarg) = length(protoarg);
	a.a_{ = {;
	a.a_protoarg) = protoarg);
	a.a_4 = 4;
	a.a_0 = 0;
	a.a_ = ;
	a.a_ = ;
	a.a_arglen = arglen;
	a.a_ = ;
	a.a_ = ;
	a.a_function. = function.;
	a.a_toupper(name)) = toupper(name));
	a.a_{ = {;
	a.a_ = ;
	a.a_") = ");
	a.a_ = ;
	a.a_ = ;
	a.a_{ = {;
	a.a_argname[i]); = argname[i]);;
	a.a_ = ;
	a.a_name); = name);;
	a.a_name); = name);;
	a.a_{ = {;
	a.a_argname[i]); = argname[i]);;
	a.a_ = ;
	a.a_\n}\n", = \n}\n",;
	a.a_name) = name);
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_<sys/buf.h>\n") = <sys/buf.h>\n");
	a.a_ = ;
	a.a_" = ";
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_"$anal_retentive" = "$anal_retentive";
	a.a_ = ;
	a.a_ = ;
	a.a_*/' = */';
	a.a_ = ;
	a.a_ = ;
	a.a_1>&2 = 1>&2;
	a.a_$out_c = $out_c;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_{ = {;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	return (VCALL(dvp, VOFFSET(vop_whiteout), &a));
}

struct vop_whiteout_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct componentname *a_cnp;
	int a_flags;
	 a_;
	 a_;
	braam+@pachelbel.coda.cs.cmu.edu Wed Oct 9 18:08:18 a_1996;
	braam+@pachelbel.coda.cs.cmu.edu a_;
	from pachelbel.coda.cs.cmu.edu (PACHELBEL.CODA.CS.CMU.EDU [128.2.222.26]) by telemann.coda.cs.cmu.edu (8.7.4/8.7.3) with SMTP id SAA09451 for <braam@telemann.coda.cs.cmu.edu> Wed, 9 Oct 1996 18:08:17 a_-0400;
	braam+@pachelbel.coda.cs.cmu.edu a_;
	<199610092208.SAA09451@telemann.coda.cs.cmu.edu> a_;
	braam@pachelbel.coda.cs.cmu.edu a_;
	Wed, 9 Oct 96 18:07:56 a_EDT;
	braam+@pachelbel.coda.cs.cmu.edu a_;
	 a_;
	 a_;
	 a_;
	Copyright (c) 1992, a_1993;
	The Regents of the University of California. All rights a_reserved.;
	 a_;
	Redistribution and use in source and binary forms, with or a_without;
	modification, are permitted provided that the following a_conditions;
	are a_met:;
	1. Redistributions of source code must retain the above a_copyright;
	notice, this list of conditions and the following a_disclaimer.;
	2. Redistributions in binary form must reproduce the above a_copyright;
	notice, this list of conditions and the following disclaimer in a_the;
	documentation and/or other materials provided with the a_distribution.;
	3. All advertising materials mentioning features or use of this a_software;
	must display the following a_acknowledgement:;
	This product includes software developed by the University a_of;
	California, Berkeley and its a_contributors.;
	4. Neither the name of the University nor the names of its a_contributors;
	may be used to endorse or promote products derived from this a_software;
	without specific prior written a_permission.;
	 a_;
	THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' a_AND;
	ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, a_THE;
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR a_PURPOSE;
	ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE a_LIABLE;
	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR a_CONSEQUENTIAL;
	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE a_GOODS;
	OR SERVICES LOSS OF USE, DATA, OR PROFITS; OR BUSINESS a_INTERRUPTION);
	HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, a_STRICT;
	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY a_WAY;
	OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY a_OF;
	SUCH a_DAMAGE.;
	 a_;
	 a_;
	vnode_if.sh,v 1.8 1995/03/10 04:13:52 chopps Exp a_$';
	 a_;
	 a_;
	[ $# -ne 1 ] a_then;
	'usage: vnode_if.sh a_srcfile';
	1 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	'BEGIN { print toupper("true") exit; }' a_2>/dev/null`;
	 a_;
	[ "$isgawk" = TRUE ] a_then;
	GNU awk provides a_it.;
	 a_;
	 a_;
	Provide our own a_toupper();
	 a_;
	toupper(str) a_{;
	= "echo "str" |tr a-z a_A-Z";
	| getline a__toupper_str;
	 a_;
	_toupper_str a_;
	 a_;
	 a_;
	\1:g a_;
	/ a_/';
	 a_;
	{ next a_};
	{ a_;
	 a_;
	 a_;
	 a_;
	{ a_;
	 a_;
	 a_;
	 a_;
	= $1 a_i=2;;
	($2 == "WILLRELE") a_{;
	= a_1;
	 a_;
	else a_;
	= a_0;
	= $i a_i++;;
	(i < NF) a_{;
	= argtype[argc]" a_"$i;
	 a_;
	 a_;
	= a_$i;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	Warning: This file is generated a_automatically.;
	(Modifications made here may easily be a_lost!);
	 a_;
	Created by the a_script:;
	${SCRIPT_ID} a_;
	 a_;
	 a_;
	 a_;
	:\1:g' a_;
	 a_;
	"$0: Creating $out_h" a_1>&2;
	> a_$out_h;
	 a_;
	"$copyright" a_;
	"$warning" a_;
	' a_;
	struct vnodeop_desc a_vop_default_desc;
	 a_;
	 a_;
	-e "$sed_prep" $src | $awk a_"$toupper"';
	doit() a_{;
	Declare arg struct, a_descriptor.;
	%s_args {\n", a_name);
	vnodeop_desc *a_desc a_\n");;
	(i=0 i<argc; i++) a_{;
	a_%s \n", argtype[i], a_argname[i]);;
	 a_;
	\n"); a_;
	struct vnodeop_desc %s_desc \n", a_name);;
	Prototype a_it.;
	= sprintf("static __inline int %s __P((", a_toupper(name));
	= a_length(protoarg);
	protoarg) a_;
	(i=0 i<argc; i++) a_{;
	= sprintf("%s", a_argtype[i]);
	(i < (argc-1)) protoarg = (protoarg ", a_");
	= a_length(protoarg);
	((protolen + arglen) > 77) a_{;
	= ("\n " a_protoarg);
	+= a_4;
	= a_0;
	 a_;
	protoarg) a_;
	+= a_arglen;
	 a_;
	\n"); a_;
	Define inline a_function.;
	__inline int %s(", a_toupper(name));
	(i=0 i<argc; i++) a_{;
	argname[i]) a_;
	(i < (argc-1)) printf(", a_");
	 a_;
	 a_;
	(i=0 i<argc; i++) a_{;
	%s \n", argtype[i], a_argname[i]);;
	 a_;
	%s_args a \n", a_name);;
	= VDESC(%s) \n", a_name);;
	(i=0 i<argc; i++) a_{;
	= %s \n", argname[i], a_argname[i]);;
	 a_;
	(VCALL(%s%s, VOFFSET(%s), &a)) a_\n}\n",;
	arg0special, a_name);
	{ a_;
	 a_;
	{ a_;
	Special cases: */\n#include a_<sys/buf.h>\n");
	 a_;
	buf *a_";
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	| sed -e a_"$anal_retentive";
	 a_;
	' a_;
	End of special cases. a_*/';
	 a_;
	 a_;
	"$0: Creating $out_c" a_1>&2;
	> a_$out_c;
	 a_;
	"$copyright" a_;
	"$warning" a_;
	' a_;
	 a_;
	vnodeop_desc vop_default_desc = a_{;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	-e "$sed_prep" $src | $awk a_';
	do_offset(typematch) a_{;
	(i=0 i<argc; i++) a_{;
	(argtype[i] == typematch) a_{;
	%s_args, a_a_%s),\n",;
	argname[i]) a_;
	i a_;
	 a_;
	 a_;
	"\tVDESC_NO_OFFSET," a_;
	-1 a_;
};
extern struct vnodeop_desc vop_whiteout_desc;
static __inline int VOP_WHITEOUT __P((struct vnode *, 
    struct componentname *, int, , , 
    braam+@pachelbel.coda.cs.cmu.edu Wed Oct 9 18:08:18, 
    braam+@pachelbel.coda.cs.cmu.edu, 
    from pachelbel.coda.cs.cmu.edu (PACHELBEL.CODA.CS.CMU.EDU [128.2.222.26]) by telemann.coda.cs.cmu.edu (8.7.4/8.7.3) with SMTP id SAA09451 for <braam@telemann.coda.cs.cmu.edu> Wed, 9 Oct 1996 18:08:17, 
    braam+@pachelbel.coda.cs.cmu.edu, 
    <199610092208.SAA09451@telemann.coda.cs.cmu.edu>, 
    braam@pachelbel.coda.cs.cmu.edu, Wed, 9 Oct 96 18:07:56, 
    braam+@pachelbel.coda.cs.cmu.edu, , , , Copyright (c) 1992,, 
    The Regents of the University of California. All rights, , 
    Redistribution and use in source and binary forms, with or, 
    modification, are permitted provided that the following, are, 
    1. Redistributions of source code must retain the above, 
    notice, this list of conditions and the following, 
    2. Redistributions in binary form must reproduce the above, 
    notice, this list of conditions and the following disclaimer in, 
    documentation and/or other materials provided with the, 
    3. All advertising materials mentioning features or use of this, 
    must display the following, 
    This product includes software developed by the University, 
    California, Berkeley and its, 
    4. Neither the name of the University nor the names of its, 
    may be used to endorse or promote products derived from this, 
    without specific prior written, , 
    THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'', 
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,, 
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR, 
    ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE, 
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR, 
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE, 
    OR SERVICES LOSS OF USE, DATA, OR PROFITS; OR BUSINESS, 
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,, 
    LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY, 
    OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY, 
    SUCH, , , vnode_if.sh,v 1.8 1995/03/10 04:13:52 chopps Exp, , , 
    [ $# -ne 1 ], 'usage: vnode_if.sh, 1, , , , , , , , , , 
    'BEGIN { print toupper("true") exit; }', , [ "$isgawk" = TRUE ], 
    GNU awk provides, , , Provide our own, , toupper(str), 
    = "echo "str" |tr a-z, | getline, , _toupper_str, , , \1:g, /, , { next, 
    {, , , , {, , , , = $1, ($2 == "WILLRELE"), =, , else, =, = $i, 
    (i < NF), = argtype[argc]", , , =, , , , , , , 
    Warning: This file is generated, (Modifications made here may easily be, 
    , Created by the, ${SCRIPT_ID}, , , , :\1:g', , "$0: Creating $out_h", 
    >, , "$copyright", "$warning", ', struct vnodeop_desc, , , 
    -e "$sed_prep" $src | $awk, doit(), Declare arg struct,, %s_args {\n",, 
    vnodeop_desc *a_desc, (i=0 i<argc; i++), a_%s \n", argtype[i],, , 
    \n");, struct vnodeop_desc %s_desc \n",, Prototype, 
    = sprintf("static __inline int %s __P((",, =, protoarg), 
    (i=0 i<argc; i++), = sprintf("%s",, 
    (i < (argc-1)) protoarg = (protoarg ",, =, ((protolen + arglen) > 77), 
    = ("\n ", +=, =, , protoarg), +=, , \n");, Define inline, 
    __inline int %s(",, (i=0 i<argc; i++), argname[i]), 
    (i < (argc-1)) printf(",, , , (i=0 i<argc; i++), %s \n", argtype[i],, , 
    %s_args a \n",, = VDESC(%s) \n",, (i=0 i<argc; i++), 
    = %s \n", argname[i],, , (VCALL(%s%s, VOFFSET(%s), &a)), arg0special,, 
    {, , {, Special cases: */\n#include, , buf *, , , , , , , | sed -e, , ', 
    End of special cases., , , "$0: Creating $out_c", >, , "$copyright", 
    "$warning", ', , vnodeop_desc vop_default_desc =, , , , , , , , , , , , 
    -e "$sed_prep" $src | $awk, do_offset(typematch), (i=0 i<argc; i++), 
    (argtype[i] == typematch), %s_args,, argname[i]), i, , , 
    "\tVDESC_NO_OFFSET,", -1));
static __inline int VOP_WHITEOUT(dvp, cnp, flags, , , 1996, , -0400, , , , EDT, , , , , 1993, reserved., , without, conditions, met:, copyright, disclaimer., copyright, the, distribution., software, acknowledgement:, of, contributors., contributors, software, permission., , AND, THE, PURPOSE, LIABLE, CONSEQUENTIAL, GOODS, INTERRUPTION), STRICT, WAY, OF, DAMAGE., , , $', , , then, srcfile', , , , , , , , , , , 2>/dev/null`, , then, it., , , toupper(), , {, A-Z", _toupper_str, , , , , , /', , }, , , , , , , , , i=2;, {, 1, , , 0, i++;, {, "$i, , , $i, , , , , , , automatically., lost!), , script:, , , , , , , 1>&2, $out_h, , , , , vop_default_desc, , , "$toupper"', {, descriptor., name), \n");, {, argname[i]);, , , name);, it., toupper(name)), length(protoarg), , {, argtype[i]), "), length(protoarg), {, protoarg), 4, 0, , , arglen, , , function., toupper(name)), {, , "), , , {, argname[i]);, , name);, name);, {, argname[i]);, , \n}\n",, name), , , , <sys/buf.h>\n"), , ", , , , , , , "$anal_retentive", , , */', , , 1>&2, $out_c, , , , , , {, , , , , , , , , , , , ', {, {, {, a_%s),\n",, , , , , , )
	struct vnode *dvp;
	struct componentname *cnp;
	int flags;
	 ;
	 ;
	braam+@pachelbel.coda.cs.cmu.edu Wed Oct 9 18:08:18 1996;
	braam+@pachelbel.coda.cs.cmu.edu ;
	from pachelbel.coda.cs.cmu.edu (PACHELBEL.CODA.CS.CMU.EDU [128.2.222.26]) by telemann.coda.cs.cmu.edu (8.7.4/8.7.3) with SMTP id SAA09451 for <braam@telemann.coda.cs.cmu.edu> Wed, 9 Oct 1996 18:08:17 -0400;
	braam+@pachelbel.coda.cs.cmu.edu ;
	<199610092208.SAA09451@telemann.coda.cs.cmu.edu> ;
	braam@pachelbel.coda.cs.cmu.edu ;
	Wed, 9 Oct 96 18:07:56 EDT;
	braam+@pachelbel.coda.cs.cmu.edu ;
	 ;
	 ;
	 ;
	Copyright (c) 1992, 1993;
	The Regents of the University of California. All rights reserved.;
	 ;
	Redistribution and use in source and binary forms, with or without;
	modification, are permitted provided that the following conditions;
	are met:;
	1. Redistributions of source code must retain the above copyright;
	notice, this list of conditions and the following disclaimer.;
	2. Redistributions in binary form must reproduce the above copyright;
	notice, this list of conditions and the following disclaimer in the;
	documentation and/or other materials provided with the distribution.;
	3. All advertising materials mentioning features or use of this software;
	must display the following acknowledgement:;
	This product includes software developed by the University of;
	California, Berkeley and its contributors.;
	4. Neither the name of the University nor the names of its contributors;
	may be used to endorse or promote products derived from this software;
	without specific prior written permission.;
	 ;
	THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND;
	ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE;
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE;
	ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE;
	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL;
	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS;
	OR SERVICES LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION);
	HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT;
	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY;
	OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF;
	SUCH DAMAGE.;
	 ;
	 ;
	vnode_if.sh,v 1.8 1995/03/10 04:13:52 chopps Exp $';
	 ;
	 ;
	[ $# -ne 1 ] then;
	'usage: vnode_if.sh srcfile';
	1 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	'BEGIN { print toupper("true") exit; }' 2>/dev/null`;
	 ;
	[ "$isgawk" = TRUE ] then;
	GNU awk provides it.;
	 ;
	 ;
	Provide our own toupper();
	 ;
	toupper(str) {;
	= "echo "str" |tr a-z A-Z";
	| getline _toupper_str;
	 ;
	_toupper_str ;
	 ;
	 ;
	\1:g ;
	/ /';
	 ;
	{ next };
	{ ;
	 ;
	 ;
	 ;
	{ ;
	 ;
	 ;
	 ;
	= $1 i=2;;
	($2 == "WILLRELE") {;
	= 1;
	 ;
	else ;
	= 0;
	= $i i++;;
	(i < NF) {;
	= argtype[argc]" "$i;
	 ;
	 ;
	= $i;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	Warning: This file is generated automatically.;
	(Modifications made here may easily be lost!);
	 ;
	Created by the script:;
	${SCRIPT_ID} ;
	 ;
	 ;
	 ;
	:\1:g' ;
	 ;
	"$0: Creating $out_h" 1>&2;
	> $out_h;
	 ;
	"$copyright" ;
	"$warning" ;
	' ;
	struct vnodeop_desc vop_default_desc;
	 ;
	 ;
	-e "$sed_prep" $src | $awk "$toupper"';
	doit() {;
	Declare arg struct, descriptor.;
	%s_args {\n", name);
	vnodeop_desc *a_desc \n");;
	(i=0 i<argc; i++) {;
	a_%s \n", argtype[i], argname[i]);;
	 ;
	\n"); ;
	struct vnodeop_desc %s_desc \n", name);;
	Prototype it.;
	= sprintf("static __inline int %s __P((", toupper(name));
	= length(protoarg);
	protoarg) ;
	(i=0 i<argc; i++) {;
	= sprintf("%s", argtype[i]);
	(i < (argc-1)) protoarg = (protoarg ", ");
	= length(protoarg);
	((protolen + arglen) > 77) {;
	= ("\n " protoarg);
	+= 4;
	= 0;
	 ;
	protoarg) ;
	+= arglen;
	 ;
	\n"); ;
	Define inline function.;
	__inline int %s(", toupper(name));
	(i=0 i<argc; i++) {;
	argname[i]) ;
	(i < (argc-1)) printf(", ");
	 ;
	 ;
	(i=0 i<argc; i++) {;
	%s \n", argtype[i], argname[i]);;
	 ;
	%s_args a \n", name);;
	= VDESC(%s) \n", name);;
	(i=0 i<argc; i++) {;
	= %s \n", argname[i], argname[i]);;
	 ;
	(VCALL(%s%s, VOFFSET(%s), &a)) \n}\n",;
	arg0special, name);
	{ ;
	 ;
	{ ;
	Special cases: */\n#include <sys/buf.h>\n");
	 ;
	buf *";
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	| sed -e "$anal_retentive";
	 ;
	' ;
	End of special cases. */';
	 ;
	 ;
	"$0: Creating $out_c" 1>&2;
	> $out_c;
	 ;
	"$copyright" ;
	"$warning" ;
	' ;
	 ;
	vnodeop_desc vop_default_desc = {;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	-e "$sed_prep" $src | $awk ';
	do_offset(typematch) {;
	(i=0 i<argc; i++) {;
	(argtype[i] == typematch) {;
	%s_args, a_%s),\n",;
	argname[i]) ;
	i ;
	 ;
	 ;
	"\tVDESC_NO_OFFSET," ;
	-1 ;
{
	struct vop_whiteout_args a;
	a.a_desc = VDESC(vop_whiteout);
	a.a_dvp = dvp;
	a.a_cnp = cnp;
	a.a_flags = flags;
	a.a_ = ;
	a.a_ = ;
	a.a_1996 = 1996;
	a.a_ = ;
	a.a_-0400 = -0400;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_EDT = EDT;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_1993 = 1993;
	a.a_reserved. = reserved.;
	a.a_ = ;
	a.a_without = without;
	a.a_conditions = conditions;
	a.a_met: = met:;
	a.a_copyright = copyright;
	a.a_disclaimer. = disclaimer.;
	a.a_copyright = copyright;
	a.a_the = the;
	a.a_distribution. = distribution.;
	a.a_software = software;
	a.a_acknowledgement: = acknowledgement:;
	a.a_of = of;
	a.a_contributors. = contributors.;
	a.a_contributors = contributors;
	a.a_software = software;
	a.a_permission. = permission.;
	a.a_ = ;
	a.a_AND = AND;
	a.a_THE = THE;
	a.a_PURPOSE = PURPOSE;
	a.a_LIABLE = LIABLE;
	a.a_CONSEQUENTIAL = CONSEQUENTIAL;
	a.a_GOODS = GOODS;
	a.a_INTERRUPTION) = INTERRUPTION);
	a.a_STRICT = STRICT;
	a.a_WAY = WAY;
	a.a_OF = OF;
	a.a_DAMAGE. = DAMAGE.;
	a.a_ = ;
	a.a_ = ;
	a.a_$' = $';
	a.a_ = ;
	a.a_ = ;
	a.a_then = then;
	a.a_srcfile' = srcfile';
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_2>/dev/null` = 2>/dev/null`;
	a.a_ = ;
	a.a_then = then;
	a.a_it. = it.;
	a.a_ = ;
	a.a_ = ;
	a.a_toupper() = toupper();
	a.a_ = ;
	a.a_{ = {;
	a.a_A-Z" = A-Z";
	a.a__toupper_str = _toupper_str;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_/' = /';
	a.a_ = ;
	a.a_} = };
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_i=2; = i=2;;
	a.a_{ = {;
	a.a_1 = 1;
	a.a_ = ;
	a.a_ = ;
	a.a_0 = 0;
	a.a_i++; = i++;;
	a.a_{ = {;
	a.a_"$i = "$i;
	a.a_ = ;
	a.a_ = ;
	a.a_$i = $i;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_automatically. = automatically.;
	a.a_lost!) = lost!);
	a.a_ = ;
	a.a_script: = script:;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_1>&2 = 1>&2;
	a.a_$out_h = $out_h;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_vop_default_desc = vop_default_desc;
	a.a_ = ;
	a.a_ = ;
	a.a_"$toupper"' = "$toupper"';
	a.a_{ = {;
	a.a_descriptor. = descriptor.;
	a.a_name) = name);
	a.a_\n"); = \n");;
	a.a_{ = {;
	a.a_argname[i]); = argname[i]);;
	a.a_ = ;
	a.a_ = ;
	a.a_name); = name);;
	a.a_it. = it.;
	a.a_toupper(name)) = toupper(name));
	a.a_length(protoarg) = length(protoarg);
	a.a_ = ;
	a.a_{ = {;
	a.a_argtype[i]) = argtype[i]);
	a.a_") = ");
	a.a_length(protoarg) = length(protoarg);
	a.a_{ = {;
	a.a_protoarg) = protoarg);
	a.a_4 = 4;
	a.a_0 = 0;
	a.a_ = ;
	a.a_ = ;
	a.a_arglen = arglen;
	a.a_ = ;
	a.a_ = ;
	a.a_function. = function.;
	a.a_toupper(name)) = toupper(name));
	a.a_{ = {;
	a.a_ = ;
	a.a_") = ");
	a.a_ = ;
	a.a_ = ;
	a.a_{ = {;
	a.a_argname[i]); = argname[i]);;
	a.a_ = ;
	a.a_name); = name);;
	a.a_name); = name);;
	a.a_{ = {;
	a.a_argname[i]); = argname[i]);;
	a.a_ = ;
	a.a_\n}\n", = \n}\n",;
	a.a_name) = name);
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_<sys/buf.h>\n") = <sys/buf.h>\n");
	a.a_ = ;
	a.a_" = ";
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_"$anal_retentive" = "$anal_retentive";
	a.a_ = ;
	a.a_ = ;
	a.a_*/' = */';
	a.a_ = ;
	a.a_ = ;
	a.a_1>&2 = 1>&2;
	a.a_$out_c = $out_c;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_{ = {;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_' = ';
	a.a_{ = {;
	a.a_{ = {;
	a.a_{ = {;
	a.a_a_%s),\n", = a_%s),\n",;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	return (VCALL(dvp, VOFFSET(vop_whiteout), &a));
}

struct vop_whiteout_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct componentname *a_cnp;
	int a_flags;
	 a_;
	 a_;
	braam+@pachelbel.coda.cs.cmu.edu Wed Oct 9 18:08:18 a_1996;
	braam+@pachelbel.coda.cs.cmu.edu a_;
	from pachelbel.coda.cs.cmu.edu (PACHELBEL.CODA.CS.CMU.EDU [128.2.222.26]) by telemann.coda.cs.cmu.edu (8.7.4/8.7.3) with SMTP id SAA09451 for <braam@telemann.coda.cs.cmu.edu> Wed, 9 Oct 1996 18:08:17 a_-0400;
	braam+@pachelbel.coda.cs.cmu.edu a_;
	<199610092208.SAA09451@telemann.coda.cs.cmu.edu> a_;
	braam@pachelbel.coda.cs.cmu.edu a_;
	Wed, 9 Oct 96 18:07:56 a_EDT;
	braam+@pachelbel.coda.cs.cmu.edu a_;
	 a_;
	 a_;
	 a_;
	Copyright (c) 1992, a_1993;
	The Regents of the University of California. All rights a_reserved.;
	 a_;
	Redistribution and use in source and binary forms, with or a_without;
	modification, are permitted provided that the following a_conditions;
	are a_met:;
	1. Redistributions of source code must retain the above a_copyright;
	notice, this list of conditions and the following a_disclaimer.;
	2. Redistributions in binary form must reproduce the above a_copyright;
	notice, this list of conditions and the following disclaimer in a_the;
	documentation and/or other materials provided with the a_distribution.;
	3. All advertising materials mentioning features or use of this a_software;
	must display the following a_acknowledgement:;
	This product includes software developed by the University a_of;
	California, Berkeley and its a_contributors.;
	4. Neither the name of the University nor the names of its a_contributors;
	may be used to endorse or promote products derived from this a_software;
	without specific prior written a_permission.;
	 a_;
	THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' a_AND;
	ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, a_THE;
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR a_PURPOSE;
	ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE a_LIABLE;
	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR a_CONSEQUENTIAL;
	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE a_GOODS;
	OR SERVICES LOSS OF USE, DATA, OR PROFITS; OR BUSINESS a_INTERRUPTION);
	HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, a_STRICT;
	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY a_WAY;
	OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY a_OF;
	SUCH a_DAMAGE.;
	 a_;
	 a_;
	vnode_if.sh,v 1.8 1995/03/10 04:13:52 chopps Exp a_$';
	 a_;
	 a_;
	[ $# -ne 1 ] a_then;
	'usage: vnode_if.sh a_srcfile';
	1 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	'BEGIN { print toupper("true") exit; }' a_2>/dev/null`;
	 a_;
	[ "$isgawk" = TRUE ] a_then;
	GNU awk provides a_it.;
	 a_;
	 a_;
	Provide our own a_toupper();
	 a_;
	toupper(str) a_{;
	= "echo "str" |tr a-z a_A-Z";
	| getline a__toupper_str;
	 a_;
	_toupper_str a_;
	 a_;
	 a_;
	\1:g a_;
	/ a_/';
	 a_;
	{ next a_};
	{ a_;
	 a_;
	 a_;
	 a_;
	{ a_;
	 a_;
	 a_;
	 a_;
	= $1 a_i=2;;
	($2 == "WILLRELE") a_{;
	= a_1;
	 a_;
	else a_;
	= a_0;
	= $i a_i++;;
	(i < NF) a_{;
	= argtype[argc]" a_"$i;
	 a_;
	 a_;
	= a_$i;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	Warning: This file is generated a_automatically.;
	(Modifications made here may easily be a_lost!);
	 a_;
	Created by the a_script:;
	${SCRIPT_ID} a_;
	 a_;
	 a_;
	 a_;
	:\1:g' a_;
	 a_;
	"$0: Creating $out_h" a_1>&2;
	> a_$out_h;
	 a_;
	"$copyright" a_;
	"$warning" a_;
	' a_;
	struct vnodeop_desc a_vop_default_desc;
	 a_;
	 a_;
	-e "$sed_prep" $src | $awk a_"$toupper"';
	doit() a_{;
	Declare arg struct, a_descriptor.;
	%s_args {\n", a_name);
	vnodeop_desc *a_desc a_\n");;
	(i=0 i<argc; i++) a_{;
	a_%s \n", argtype[i], a_argname[i]);;
	 a_;
	\n"); a_;
	struct vnodeop_desc %s_desc \n", a_name);;
	Prototype a_it.;
	= sprintf("static __inline int %s __P((", a_toupper(name));
	= a_length(protoarg);
	protoarg) a_;
	(i=0 i<argc; i++) a_{;
	= sprintf("%s", a_argtype[i]);
	(i < (argc-1)) protoarg = (protoarg ", a_");
	= a_length(protoarg);
	((protolen + arglen) > 77) a_{;
	= ("\n " a_protoarg);
	+= a_4;
	= a_0;
	 a_;
	protoarg) a_;
	+= a_arglen;
	 a_;
	\n"); a_;
	Define inline a_function.;
	__inline int %s(", a_toupper(name));
	(i=0 i<argc; i++) a_{;
	argname[i]) a_;
	(i < (argc-1)) printf(", a_");
	 a_;
	 a_;
	(i=0 i<argc; i++) a_{;
	%s \n", argtype[i], a_argname[i]);;
	 a_;
	%s_args a \n", a_name);;
	= VDESC(%s) \n", a_name);;
	(i=0 i<argc; i++) a_{;
	= %s \n", argname[i], a_argname[i]);;
	 a_;
	(VCALL(%s%s, VOFFSET(%s), &a)) a_\n}\n",;
	arg0special, a_name);
	{ a_;
	 a_;
	{ a_;
	Special cases: */\n#include a_<sys/buf.h>\n");
	 a_;
	buf *a_";
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	| sed -e a_"$anal_retentive";
	 a_;
	' a_;
	End of special cases. a_*/';
	 a_;
	 a_;
	"$0: Creating $out_c" a_1>&2;
	> a_$out_c;
	 a_;
	"$copyright" a_;
	"$warning" a_;
	' a_;
	 a_;
	vnodeop_desc vop_default_desc = a_{;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	-e "$sed_prep" $src | $awk a_';
	do_offset(typematch) a_{;
	(i=0 i<argc; i++) a_{;
	(argtype[i] == typematch) a_{;
	%s_args, a_a_%s),\n",;
	argname[i]) a_;
	i a_;
	 a_;
	 a_;
	"\tVDESC_NO_OFFSET," a_;
	-1 a_;
	 a_;
	doit() a_{;
	Define offsets a_array;
	%s_vp_offsets[] = {\n", a_name);
	(i=0 i<argc; i++) a_{;
	(argtype[i] == "struct vnode *") a_{;
	("\tVOPARG_OFFSETOF(struct a_%s_args,a_%s),\n",;
	argname[i]) a_;
	 a_;
	 a_;
	"\tVDESC_NO_OFFSET" a_;
	"} a_";;
	Define a_F_desc;
	vnodeop_desc %s_desc = {\n", a_name);
	offset a_;
	("\t0,\n") a_;
	printable a_name;
	("\t\"%s\",\n", a_name);
	flags a_;
	 a_;
	= a_0;
	(i=0 i<argc; i++) a_{;
	(willrele[i]) a_{;
	(argdir[i] ~ /OUT/) a_{;
	| a_VDESC_VPP_WILLRELE");
	else a_{;
	| VDESC_VP%s_WILLRELE", a_vpnum);
	 a_;
	 a_;
	 a_;
	 a_;
	"," a_;
	vp a_offsets;
	("\t%s_vp_offsets,\n", a_name);
	vpp (if a_any);
	vnode **a_");
	cred (if a_any);
	ucred *a_");
	proc (if a_any);
	proc *a_");
	componentname a_;
	componentname *a_");
	transport layer a_information;
	("\tNULL,\n} a_\n");;
};
extern struct vnodeop_desc vop_whiteout_desc;
static __inline int VOP_WHITEOUT __P((struct vnode *, 
    struct componentname *, int, , , 
    braam+@pachelbel.coda.cs.cmu.edu Wed Oct 9 18:08:18, 
    braam+@pachelbel.coda.cs.cmu.edu, 
    from pachelbel.coda.cs.cmu.edu (PACHELBEL.CODA.CS.CMU.EDU [128.2.222.26]) by telemann.coda.cs.cmu.edu (8.7.4/8.7.3) with SMTP id SAA09451 for <braam@telemann.coda.cs.cmu.edu> Wed, 9 Oct 1996 18:08:17, 
    braam+@pachelbel.coda.cs.cmu.edu, 
    <199610092208.SAA09451@telemann.coda.cs.cmu.edu>, 
    braam@pachelbel.coda.cs.cmu.edu, Wed, 9 Oct 96 18:07:56, 
    braam+@pachelbel.coda.cs.cmu.edu, , , , Copyright (c) 1992,, 
    The Regents of the University of California. All rights, , 
    Redistribution and use in source and binary forms, with or, 
    modification, are permitted provided that the following, are, 
    1. Redistributions of source code must retain the above, 
    notice, this list of conditions and the following, 
    2. Redistributions in binary form must reproduce the above, 
    notice, this list of conditions and the following disclaimer in, 
    documentation and/or other materials provided with the, 
    3. All advertising materials mentioning features or use of this, 
    must display the following, 
    This product includes software developed by the University, 
    California, Berkeley and its, 
    4. Neither the name of the University nor the names of its, 
    may be used to endorse or promote products derived from this, 
    without specific prior written, , 
    THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'', 
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,, 
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR, 
    ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE, 
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR, 
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE, 
    OR SERVICES LOSS OF USE, DATA, OR PROFITS; OR BUSINESS, 
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,, 
    LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY, 
    OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY, 
    SUCH, , , vnode_if.sh,v 1.8 1995/03/10 04:13:52 chopps Exp, , , 
    [ $# -ne 1 ], 'usage: vnode_if.sh, 1, , , , , , , , , , 
    'BEGIN { print toupper("true") exit; }', , [ "$isgawk" = TRUE ], 
    GNU awk provides, , , Provide our own, , toupper(str), 
    = "echo "str" |tr a-z, | getline, , _toupper_str, , , \1:g, /, , { next, 
    {, , , , {, , , , = $1, ($2 == "WILLRELE"), =, , else, =, = $i, 
    (i < NF), = argtype[argc]", , , =, , , , , , , 
    Warning: This file is generated, (Modifications made here may easily be, 
    , Created by the, ${SCRIPT_ID}, , , , :\1:g', , "$0: Creating $out_h", 
    >, , "$copyright", "$warning", ', struct vnodeop_desc, , , 
    -e "$sed_prep" $src | $awk, doit(), Declare arg struct,, %s_args {\n",, 
    vnodeop_desc *a_desc, (i=0 i<argc; i++), a_%s \n", argtype[i],, , 
    \n");, struct vnodeop_desc %s_desc \n",, Prototype, 
    = sprintf("static __inline int %s __P((",, =, protoarg), 
    (i=0 i<argc; i++), = sprintf("%s",, 
    (i < (argc-1)) protoarg = (protoarg ",, =, ((protolen + arglen) > 77), 
    = ("\n ", +=, =, , protoarg), +=, , \n");, Define inline, 
    __inline int %s(",, (i=0 i<argc; i++), argname[i]), 
    (i < (argc-1)) printf(",, , , (i=0 i<argc; i++), %s \n", argtype[i],, , 
    %s_args a \n",, = VDESC(%s) \n",, (i=0 i<argc; i++), 
    = %s \n", argname[i],, , (VCALL(%s%s, VOFFSET(%s), &a)), arg0special,, 
    {, , {, Special cases: */\n#include, , buf *, , , , , , , | sed -e, , ', 
    End of special cases., , , "$0: Creating $out_c", >, , "$copyright", 
    "$warning", ', , vnodeop_desc vop_default_desc =, , , , , , , , , , , , 
    -e "$sed_prep" $src | $awk, do_offset(typematch), (i=0 i<argc; i++), 
    (argtype[i] == typematch), %s_args,, argname[i]), i, , , 
    "\tVDESC_NO_OFFSET,", -1, , doit(), Define offsets, 
    %s_vp_offsets[] = {\n",, (i=0 i<argc; i++), 
    (argtype[i] == "struct vnode *"), ("\tVOPARG_OFFSETOF(struct, 
    argname[i]), , , "\tVDESC_NO_OFFSET", "}, Define, 
    vnodeop_desc %s_desc = {\n",, offset, ("\t0,\n"), printable, 
    ("\t\"%s\",\n",, flags, , =, (i=0 i<argc; i++), (willrele[i]), 
    (argdir[i] ~ /OUT/), |, else, | VDESC_VP%s_WILLRELE",, , , , , ",", vp, 
    ("\t%s_vp_offsets,\n",, vpp (if, vnode **, cred (if, ucred *, proc (if, 
    proc *, componentname, componentname *, transport layer, ("\tNULL,\n}));
static __inline int VOP_WHITEOUT(dvp, cnp, flags, , , 1996, , -0400, , , , EDT, , , , , 1993, reserved., , without, conditions, met:, copyright, disclaimer., copyright, the, distribution., software, acknowledgement:, of, contributors., contributors, software, permission., , AND, THE, PURPOSE, LIABLE, CONSEQUENTIAL, GOODS, INTERRUPTION), STRICT, WAY, OF, DAMAGE., , , $', , , then, srcfile', , , , , , , , , , , 2>/dev/null`, , then, it., , , toupper(), , {, A-Z", _toupper_str, , , , , , /', , }, , , , , , , , , i=2;, {, 1, , , 0, i++;, {, "$i, , , $i, , , , , , , automatically., lost!), , script:, , , , , , , 1>&2, $out_h, , , , , vop_default_desc, , , "$toupper"', {, descriptor., name), \n");, {, argname[i]);, , , name);, it., toupper(name)), length(protoarg), , {, argtype[i]), "), length(protoarg), {, protoarg), 4, 0, , , arglen, , , function., toupper(name)), {, , "), , , {, argname[i]);, , name);, name);, {, argname[i]);, , \n}\n",, name), , , , <sys/buf.h>\n"), , ", , , , , , , "$anal_retentive", , , */', , , 1>&2, $out_c, , , , , , {, , , , , , , , , , , , ', {, {, {, a_%s),\n",, , , , , , , , {, array, name), {, {, %s_args,a_%s),\n",, , , , , ";, F_desc, name), , , name, name), , , 0, {, {, {, VDESC_VPP_WILLRELE"), {, vpnum), , , , , , offsets, name), any), "), any), "), any), "), , "), information, \n");)
	struct vnode *dvp;
	struct componentname *cnp;
	int flags;
	 ;
	 ;
	braam+@pachelbel.coda.cs.cmu.edu Wed Oct 9 18:08:18 1996;
	braam+@pachelbel.coda.cs.cmu.edu ;
	from pachelbel.coda.cs.cmu.edu (PACHELBEL.CODA.CS.CMU.EDU [128.2.222.26]) by telemann.coda.cs.cmu.edu (8.7.4/8.7.3) with SMTP id SAA09451 for <braam@telemann.coda.cs.cmu.edu> Wed, 9 Oct 1996 18:08:17 -0400;
	braam+@pachelbel.coda.cs.cmu.edu ;
	<199610092208.SAA09451@telemann.coda.cs.cmu.edu> ;
	braam@pachelbel.coda.cs.cmu.edu ;
	Wed, 9 Oct 96 18:07:56 EDT;
	braam+@pachelbel.coda.cs.cmu.edu ;
	 ;
	 ;
	 ;
	Copyright (c) 1992, 1993;
	The Regents of the University of California. All rights reserved.;
	 ;
	Redistribution and use in source and binary forms, with or without;
	modification, are permitted provided that the following conditions;
	are met:;
	1. Redistributions of source code must retain the above copyright;
	notice, this list of conditions and the following disclaimer.;
	2. Redistributions in binary form must reproduce the above copyright;
	notice, this list of conditions and the following disclaimer in the;
	documentation and/or other materials provided with the distribution.;
	3. All advertising materials mentioning features or use of this software;
	must display the following acknowledgement:;
	This product includes software developed by the University of;
	California, Berkeley and its contributors.;
	4. Neither the name of the University nor the names of its contributors;
	may be used to endorse or promote products derived from this software;
	without specific prior written permission.;
	 ;
	THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND;
	ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE;
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE;
	ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE;
	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL;
	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS;
	OR SERVICES LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION);
	HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT;
	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY;
	OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF;
	SUCH DAMAGE.;
	 ;
	 ;
	vnode_if.sh,v 1.8 1995/03/10 04:13:52 chopps Exp $';
	 ;
	 ;
	[ $# -ne 1 ] then;
	'usage: vnode_if.sh srcfile';
	1 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	'BEGIN { print toupper("true") exit; }' 2>/dev/null`;
	 ;
	[ "$isgawk" = TRUE ] then;
	GNU awk provides it.;
	 ;
	 ;
	Provide our own toupper();
	 ;
	toupper(str) {;
	= "echo "str" |tr a-z A-Z";
	| getline _toupper_str;
	 ;
	_toupper_str ;
	 ;
	 ;
	\1:g ;
	/ /';
	 ;
	{ next };
	{ ;
	 ;
	 ;
	 ;
	{ ;
	 ;
	 ;
	 ;
	= $1 i=2;;
	($2 == "WILLRELE") {;
	= 1;
	 ;
	else ;
	= 0;
	= $i i++;;
	(i < NF) {;
	= argtype[argc]" "$i;
	 ;
	 ;
	= $i;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	Warning: This file is generated automatically.;
	(Modifications made here may easily be lost!);
	 ;
	Created by the script:;
	${SCRIPT_ID} ;
	 ;
	 ;
	 ;
	:\1:g' ;
	 ;
	"$0: Creating $out_h" 1>&2;
	> $out_h;
	 ;
	"$copyright" ;
	"$warning" ;
	' ;
	struct vnodeop_desc vop_default_desc;
	 ;
	 ;
	-e "$sed_prep" $src | $awk "$toupper"';
	doit() {;
	Declare arg struct, descriptor.;
	%s_args {\n", name);
	vnodeop_desc *a_desc \n");;
	(i=0 i<argc; i++) {;
	a_%s \n", argtype[i], argname[i]);;
	 ;
	\n"); ;
	struct vnodeop_desc %s_desc \n", name);;
	Prototype it.;
	= sprintf("static __inline int %s __P((", toupper(name));
	= length(protoarg);
	protoarg) ;
	(i=0 i<argc; i++) {;
	= sprintf("%s", argtype[i]);
	(i < (argc-1)) protoarg = (protoarg ", ");
	= length(protoarg);
	((protolen + arglen) > 77) {;
	= ("\n " protoarg);
	+= 4;
	= 0;
	 ;
	protoarg) ;
	+= arglen;
	 ;
	\n"); ;
	Define inline function.;
	__inline int %s(", toupper(name));
	(i=0 i<argc; i++) {;
	argname[i]) ;
	(i < (argc-1)) printf(", ");
	 ;
	 ;
	(i=0 i<argc; i++) {;
	%s \n", argtype[i], argname[i]);;
	 ;
	%s_args a \n", name);;
	= VDESC(%s) \n", name);;
	(i=0 i<argc; i++) {;
	= %s \n", argname[i], argname[i]);;
	 ;
	(VCALL(%s%s, VOFFSET(%s), &a)) \n}\n",;
	arg0special, name);
	{ ;
	 ;
	{ ;
	Special cases: */\n#include <sys/buf.h>\n");
	 ;
	buf *";
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	| sed -e "$anal_retentive";
	 ;
	' ;
	End of special cases. */';
	 ;
	 ;
	"$0: Creating $out_c" 1>&2;
	> $out_c;
	 ;
	"$copyright" ;
	"$warning" ;
	' ;
	 ;
	vnodeop_desc vop_default_desc = {;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	-e "$sed_prep" $src | $awk ';
	do_offset(typematch) {;
	(i=0 i<argc; i++) {;
	(argtype[i] == typematch) {;
	%s_args, a_%s),\n",;
	argname[i]) ;
	i ;
	 ;
	 ;
	"\tVDESC_NO_OFFSET," ;
	-1 ;
	 ;
	doit() {;
	Define offsets array;
	%s_vp_offsets[] = {\n", name);
	(i=0 i<argc; i++) {;
	(argtype[i] == "struct vnode *") {;
	("\tVOPARG_OFFSETOF(struct %s_args,a_%s),\n",;
	argname[i]) ;
	 ;
	 ;
	"\tVDESC_NO_OFFSET" ;
	"} ";;
	Define F_desc;
	vnodeop_desc %s_desc = {\n", name);
	offset ;
	("\t0,\n") ;
	printable name;
	("\t\"%s\",\n", name);
	flags ;
	 ;
	= 0;
	(i=0 i<argc; i++) {;
	(willrele[i]) {;
	(argdir[i] ~ /OUT/) {;
	| VDESC_VPP_WILLRELE");
	else {;
	| VDESC_VP%s_WILLRELE", vpnum);
	 ;
	 ;
	 ;
	 ;
	"," ;
	vp offsets;
	("\t%s_vp_offsets,\n", name);
	vpp (if any);
	vnode **");
	cred (if any);
	ucred *");
	proc (if any);
	proc *");
	componentname ;
	componentname *");
	transport layer information;
	("\tNULL,\n} \n");;
{
	struct vop_whiteout_args a;
	a.a_desc = VDESC(vop_whiteout);
	a.a_dvp = dvp;
	a.a_cnp = cnp;
	a.a_flags = flags;
	a.a_ = ;
	a.a_ = ;
	a.a_1996 = 1996;
	a.a_ = ;
	a.a_-0400 = -0400;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_EDT = EDT;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_1993 = 1993;
	a.a_reserved. = reserved.;
	a.a_ = ;
	a.a_without = without;
	a.a_conditions = conditions;
	a.a_met: = met:;
	a.a_copyright = copyright;
	a.a_disclaimer. = disclaimer.;
	a.a_copyright = copyright;
	a.a_the = the;
	a.a_distribution. = distribution.;
	a.a_software = software;
	a.a_acknowledgement: = acknowledgement:;
	a.a_of = of;
	a.a_contributors. = contributors.;
	a.a_contributors = contributors;
	a.a_software = software;
	a.a_permission. = permission.;
	a.a_ = ;
	a.a_AND = AND;
	a.a_THE = THE;
	a.a_PURPOSE = PURPOSE;
	a.a_LIABLE = LIABLE;
	a.a_CONSEQUENTIAL = CONSEQUENTIAL;
	a.a_GOODS = GOODS;
	a.a_INTERRUPTION) = INTERRUPTION);
	a.a_STRICT = STRICT;
	a.a_WAY = WAY;
	a.a_OF = OF;
	a.a_DAMAGE. = DAMAGE.;
	a.a_ = ;
	a.a_ = ;
	a.a_$' = $';
	a.a_ = ;
	a.a_ = ;
	a.a_then = then;
	a.a_srcfile' = srcfile';
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_2>/dev/null` = 2>/dev/null`;
	a.a_ = ;
	a.a_then = then;
	a.a_it. = it.;
	a.a_ = ;
	a.a_ = ;
	a.a_toupper() = toupper();
	a.a_ = ;
	a.a_{ = {;
	a.a_A-Z" = A-Z";
	a.a__toupper_str = _toupper_str;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_/' = /';
	a.a_ = ;
	a.a_} = };
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_i=2; = i=2;;
	a.a_{ = {;
	a.a_1 = 1;
	a.a_ = ;
	a.a_ = ;
	a.a_0 = 0;
	a.a_i++; = i++;;
	a.a_{ = {;
	a.a_"$i = "$i;
	a.a_ = ;
	a.a_ = ;
	a.a_$i = $i;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_automatically. = automatically.;
	a.a_lost!) = lost!);
	a.a_ = ;
	a.a_script: = script:;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_1>&2 = 1>&2;
	a.a_$out_h = $out_h;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_vop_default_desc = vop_default_desc;
	a.a_ = ;
	a.a_ = ;
	a.a_"$toupper"' = "$toupper"';
	a.a_{ = {;
	a.a_descriptor. = descriptor.;
	a.a_name) = name);
	a.a_\n"); = \n");;
	a.a_{ = {;
	a.a_argname[i]); = argname[i]);;
	a.a_ = ;
	a.a_ = ;
	a.a_name); = name);;
	a.a_it. = it.;
	a.a_toupper(name)) = toupper(name));
	a.a_length(protoarg) = length(protoarg);
	a.a_ = ;
	a.a_{ = {;
	a.a_argtype[i]) = argtype[i]);
	a.a_") = ");
	a.a_length(protoarg) = length(protoarg);
	a.a_{ = {;
	a.a_protoarg) = protoarg);
	a.a_4 = 4;
	a.a_0 = 0;
	a.a_ = ;
	a.a_ = ;
	a.a_arglen = arglen;
	a.a_ = ;
	a.a_ = ;
	a.a_function. = function.;
	a.a_toupper(name)) = toupper(name));
	a.a_{ = {;
	a.a_ = ;
	a.a_") = ");
	a.a_ = ;
	a.a_ = ;
	a.a_{ = {;
	a.a_argname[i]); = argname[i]);;
	a.a_ = ;
	a.a_name); = name);;
	a.a_name); = name);;
	a.a_{ = {;
	a.a_argname[i]); = argname[i]);;
	a.a_ = ;
	a.a_\n}\n", = \n}\n",;
	a.a_name) = name);
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_<sys/buf.h>\n") = <sys/buf.h>\n");
	a.a_ = ;
	a.a_" = ";
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_"$anal_retentive" = "$anal_retentive";
	a.a_ = ;
	a.a_ = ;
	a.a_*/' = */';
	a.a_ = ;
	a.a_ = ;
	a.a_1>&2 = 1>&2;
	a.a_$out_c = $out_c;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_{ = {;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_' = ';
	a.a_{ = {;
	a.a_{ = {;
	a.a_{ = {;
	a.a_a_%s),\n", = a_%s),\n",;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_{ = {;
	a.a_array = array;
	a.a_name) = name);
	a.a_{ = {;
	a.a_{ = {;
	a.a_%s_args,a_%s),\n", = %s_args,a_%s),\n",;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_"; = ";;
	a.a_F_desc = F_desc;
	a.a_name) = name);
	a.a_ = ;
	a.a_ = ;
	a.a_name = name;
	a.a_name) = name);
	a.a_ = ;
	a.a_ = ;
	a.a_0 = 0;
	a.a_{ = {;
	a.a_{ = {;
	a.a_{ = {;
	a.a_VDESC_VPP_WILLRELE") = VDESC_VPP_WILLRELE");
	a.a_{ = {;
	a.a_vpnum) = vpnum);
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_offsets = offsets;
	a.a_name) = name);
	a.a_any) = any);
	a.a_") = ");
	a.a_any) = any);
	a.a_") = ");
	a.a_any) = any);
	a.a_") = ");
	a.a_ = ;
	a.a_") = ");
	a.a_information = information;
	a.a_\n"); = \n");;
	return (VCALL(dvp, VOFFSET(vop_whiteout), &a));
}

struct vop_whiteout_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct componentname *a_cnp;
	int a_flags;
	 a_;
	 a_;
	braam+@pachelbel.coda.cs.cmu.edu Wed Oct 9 18:08:18 a_1996;
	braam+@pachelbel.coda.cs.cmu.edu a_;
	from pachelbel.coda.cs.cmu.edu (PACHELBEL.CODA.CS.CMU.EDU [128.2.222.26]) by telemann.coda.cs.cmu.edu (8.7.4/8.7.3) with SMTP id SAA09451 for <braam@telemann.coda.cs.cmu.edu> Wed, 9 Oct 1996 18:08:17 a_-0400;
	braam+@pachelbel.coda.cs.cmu.edu a_;
	<199610092208.SAA09451@telemann.coda.cs.cmu.edu> a_;
	braam@pachelbel.coda.cs.cmu.edu a_;
	Wed, 9 Oct 96 18:07:56 a_EDT;
	braam+@pachelbel.coda.cs.cmu.edu a_;
	 a_;
	 a_;
	 a_;
	Copyright (c) 1992, a_1993;
	The Regents of the University of California. All rights a_reserved.;
	 a_;
	Redistribution and use in source and binary forms, with or a_without;
	modification, are permitted provided that the following a_conditions;
	are a_met:;
	1. Redistributions of source code must retain the above a_copyright;
	notice, this list of conditions and the following a_disclaimer.;
	2. Redistributions in binary form must reproduce the above a_copyright;
	notice, this list of conditions and the following disclaimer in a_the;
	documentation and/or other materials provided with the a_distribution.;
	3. All advertising materials mentioning features or use of this a_software;
	must display the following a_acknowledgement:;
	This product includes software developed by the University a_of;
	California, Berkeley and its a_contributors.;
	4. Neither the name of the University nor the names of its a_contributors;
	may be used to endorse or promote products derived from this a_software;
	without specific prior written a_permission.;
	 a_;
	THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' a_AND;
	ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, a_THE;
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR a_PURPOSE;
	ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE a_LIABLE;
	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR a_CONSEQUENTIAL;
	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE a_GOODS;
	OR SERVICES LOSS OF USE, DATA, OR PROFITS; OR BUSINESS a_INTERRUPTION);
	HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, a_STRICT;
	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY a_WAY;
	OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY a_OF;
	SUCH a_DAMAGE.;
	 a_;
	 a_;
	vnode_if.sh,v 1.8 1995/03/10 04:13:52 chopps Exp a_$';
	 a_;
	 a_;
	[ $# -ne 1 ] a_then;
	'usage: vnode_if.sh a_srcfile';
	1 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	'BEGIN { print toupper("true") exit; }' a_2>/dev/null`;
	 a_;
	[ "$isgawk" = TRUE ] a_then;
	GNU awk provides a_it.;
	 a_;
	 a_;
	Provide our own a_toupper();
	 a_;
	toupper(str) a_{;
	= "echo "str" |tr a-z a_A-Z";
	| getline a__toupper_str;
	 a_;
	_toupper_str a_;
	 a_;
	 a_;
	\1:g a_;
	/ a_/';
	 a_;
	{ next a_};
	{ a_;
	 a_;
	 a_;
	 a_;
	{ a_;
	 a_;
	 a_;
	 a_;
	= $1 a_i=2;;
	($2 == "WILLRELE") a_{;
	= a_1;
	 a_;
	else a_;
	= a_0;
	= $i a_i++;;
	(i < NF) a_{;
	= argtype[argc]" a_"$i;
	 a_;
	 a_;
	= a_$i;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	Warning: This file is generated a_automatically.;
	(Modifications made here may easily be a_lost!);
	 a_;
	Created by the a_script:;
	${SCRIPT_ID} a_;
	 a_;
	 a_;
	 a_;
	:\1:g' a_;
	 a_;
	"$0: Creating $out_h" a_1>&2;
	> a_$out_h;
	 a_;
	"$copyright" a_;
	"$warning" a_;
	' a_;
	struct vnodeop_desc a_vop_default_desc;
	 a_;
	 a_;
	-e "$sed_prep" $src | $awk a_"$toupper"';
	doit() a_{;
	Declare arg struct, a_descriptor.;
	%s_args {\n", a_name);
	vnodeop_desc *a_desc a_\n");;
	(i=0 i<argc; i++) a_{;
	a_%s \n", argtype[i], a_argname[i]);;
	 a_;
	\n"); a_;
	struct vnodeop_desc %s_desc \n", a_name);;
	Prototype a_it.;
	= sprintf("static __inline int %s __P((", a_toupper(name));
	= a_length(protoarg);
	protoarg) a_;
	(i=0 i<argc; i++) a_{;
	= sprintf("%s", a_argtype[i]);
	(i < (argc-1)) protoarg = (protoarg ", a_");
	= a_length(protoarg);
	((protolen + arglen) > 77) a_{;
	= ("\n " a_protoarg);
	+= a_4;
	= a_0;
	 a_;
	protoarg) a_;
	+= a_arglen;
	 a_;
	\n"); a_;
	Define inline a_function.;
	__inline int %s(", a_toupper(name));
	(i=0 i<argc; i++) a_{;
	argname[i]) a_;
	(i < (argc-1)) printf(", a_");
	 a_;
	 a_;
	(i=0 i<argc; i++) a_{;
	%s \n", argtype[i], a_argname[i]);;
	 a_;
	%s_args a \n", a_name);;
	= VDESC(%s) \n", a_name);;
	(i=0 i<argc; i++) a_{;
	= %s \n", argname[i], a_argname[i]);;
	 a_;
	(VCALL(%s%s, VOFFSET(%s), &a)) a_\n}\n",;
	arg0special, a_name);
	{ a_;
	 a_;
	{ a_;
	Special cases: */\n#include a_<sys/buf.h>\n");
	 a_;
	buf *a_";
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	| sed -e a_"$anal_retentive";
	 a_;
	' a_;
	End of special cases. a_*/';
	 a_;
	 a_;
	"$0: Creating $out_c" a_1>&2;
	> a_$out_c;
	 a_;
	"$copyright" a_;
	"$warning" a_;
	' a_;
	 a_;
	vnodeop_desc vop_default_desc = a_{;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	-e "$sed_prep" $src | $awk a_';
	do_offset(typematch) a_{;
	(i=0 i<argc; i++) a_{;
	(argtype[i] == typematch) a_{;
	%s_args, a_a_%s),\n",;
	argname[i]) a_;
	i a_;
	 a_;
	 a_;
	"\tVDESC_NO_OFFSET," a_;
	-1 a_;
	 a_;
	doit() a_{;
	Define offsets a_array;
	%s_vp_offsets[] = {\n", a_name);
	(i=0 i<argc; i++) a_{;
	(argtype[i] == "struct vnode *") a_{;
	("\tVOPARG_OFFSETOF(struct a_%s_args,a_%s),\n",;
	argname[i]) a_;
	 a_;
	 a_;
	"\tVDESC_NO_OFFSET" a_;
	"} a_";;
	Define a_F_desc;
	vnodeop_desc %s_desc = {\n", a_name);
	offset a_;
	("\t0,\n") a_;
	printable a_name;
	("\t\"%s\",\n", a_name);
	flags a_;
	 a_;
	= a_0;
	(i=0 i<argc; i++) a_{;
	(willrele[i]) a_{;
	(argdir[i] ~ /OUT/) a_{;
	| a_VDESC_VPP_WILLRELE");
	else a_{;
	| VDESC_VP%s_WILLRELE", a_vpnum);
	 a_;
	 a_;
	 a_;
	 a_;
	"," a_;
	vp a_offsets;
	("\t%s_vp_offsets,\n", a_name);
	vpp (if a_any);
	vnode **a_");
	cred (if a_any);
	ucred *a_");
	proc (if a_any);
	proc *a_");
	componentname a_;
	componentname *a_");
	transport layer a_information;
	("\tNULL,\n} a_\n");;
	{ a_;
	Special cases: a_*/\n");
	 a_;
	 a_;
	buf *a_";
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
};
extern struct vnodeop_desc vop_whiteout_desc;
static __inline int VOP_WHITEOUT __P((struct vnode *, 
    struct componentname *, int, , , 
    braam+@pachelbel.coda.cs.cmu.edu Wed Oct 9 18:08:18, 
    braam+@pachelbel.coda.cs.cmu.edu, 
    from pachelbel.coda.cs.cmu.edu (PACHELBEL.CODA.CS.CMU.EDU [128.2.222.26]) by telemann.coda.cs.cmu.edu (8.7.4/8.7.3) with SMTP id SAA09451 for <braam@telemann.coda.cs.cmu.edu> Wed, 9 Oct 1996 18:08:17, 
    braam+@pachelbel.coda.cs.cmu.edu, 
    <199610092208.SAA09451@telemann.coda.cs.cmu.edu>, 
    braam@pachelbel.coda.cs.cmu.edu, Wed, 9 Oct 96 18:07:56, 
    braam+@pachelbel.coda.cs.cmu.edu, , , , Copyright (c) 1992,, 
    The Regents of the University of California. All rights, , 
    Redistribution and use in source and binary forms, with or, 
    modification, are permitted provided that the following, are, 
    1. Redistributions of source code must retain the above, 
    notice, this list of conditions and the following, 
    2. Redistributions in binary form must reproduce the above, 
    notice, this list of conditions and the following disclaimer in, 
    documentation and/or other materials provided with the, 
    3. All advertising materials mentioning features or use of this, 
    must display the following, 
    This product includes software developed by the University, 
    California, Berkeley and its, 
    4. Neither the name of the University nor the names of its, 
    may be used to endorse or promote products derived from this, 
    without specific prior written, , 
    THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'', 
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,, 
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR, 
    ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE, 
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR, 
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE, 
    OR SERVICES LOSS OF USE, DATA, OR PROFITS; OR BUSINESS, 
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,, 
    LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY, 
    OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY, 
    SUCH, , , vnode_if.sh,v 1.8 1995/03/10 04:13:52 chopps Exp, , , 
    [ $# -ne 1 ], 'usage: vnode_if.sh, 1, , , , , , , , , , 
    'BEGIN { print toupper("true") exit; }', , [ "$isgawk" = TRUE ], 
    GNU awk provides, , , Provide our own, , toupper(str), 
    = "echo "str" |tr a-z, | getline, , _toupper_str, , , \1:g, /, , { next, 
    {, , , , {, , , , = $1, ($2 == "WILLRELE"), =, , else, =, = $i, 
    (i < NF), = argtype[argc]", , , =, , , , , , , 
    Warning: This file is generated, (Modifications made here may easily be, 
    , Created by the, ${SCRIPT_ID}, , , , :\1:g', , "$0: Creating $out_h", 
    >, , "$copyright", "$warning", ', struct vnodeop_desc, , , 
    -e "$sed_prep" $src | $awk, doit(), Declare arg struct,, %s_args {\n",, 
    vnodeop_desc *a_desc, (i=0 i<argc; i++), a_%s \n", argtype[i],, , 
    \n");, struct vnodeop_desc %s_desc \n",, Prototype, 
    = sprintf("static __inline int %s __P((",, =, protoarg), 
    (i=0 i<argc; i++), = sprintf("%s",, 
    (i < (argc-1)) protoarg = (protoarg ",, =, ((protolen + arglen) > 77), 
    = ("\n ", +=, =, , protoarg), +=, , \n");, Define inline, 
    __inline int %s(",, (i=0 i<argc; i++), argname[i]), 
    (i < (argc-1)) printf(",, , , (i=0 i<argc; i++), %s \n", argtype[i],, , 
    %s_args a \n",, = VDESC(%s) \n",, (i=0 i<argc; i++), 
    = %s \n", argname[i],, , (VCALL(%s%s, VOFFSET(%s), &a)), arg0special,, 
    {, , {, Special cases: */\n#include, , buf *, , , , , , , | sed -e, , ', 
    End of special cases., , , "$0: Creating $out_c", >, , "$copyright", 
    "$warning", ', , vnodeop_desc vop_default_desc =, , , , , , , , , , , , 
    -e "$sed_prep" $src | $awk, do_offset(typematch), (i=0 i<argc; i++), 
    (argtype[i] == typematch), %s_args,, argname[i]), i, , , 
    "\tVDESC_NO_OFFSET,", -1, , doit(), Define offsets, 
    %s_vp_offsets[] = {\n",, (i=0 i<argc; i++), 
    (argtype[i] == "struct vnode *"), ("\tVOPARG_OFFSETOF(struct, 
    argname[i]), , , "\tVDESC_NO_OFFSET", "}, Define, 
    vnodeop_desc %s_desc = {\n",, offset, ("\t0,\n"), printable, 
    ("\t\"%s\",\n",, flags, , =, (i=0 i<argc; i++), (willrele[i]), 
    (argdir[i] ~ /OUT/), |, else, | VDESC_VP%s_WILLRELE",, , , , , ",", vp, 
    ("\t%s_vp_offsets,\n",, vpp (if, vnode **, cred (if, ucred *, proc (if, 
    proc *, componentname, componentname *, transport layer, ("\tNULL,\n}, 
    {, Special cases:, , , buf *, , , , , , ));
static __inline int VOP_WHITEOUT(dvp, cnp, flags, , , 1996, , -0400, , , , EDT, , , , , 1993, reserved., , without, conditions, met:, copyright, disclaimer., copyright, the, distribution., software, acknowledgement:, of, contributors., contributors, software, permission., , AND, THE, PURPOSE, LIABLE, CONSEQUENTIAL, GOODS, INTERRUPTION), STRICT, WAY, OF, DAMAGE., , , $', , , then, srcfile', , , , , , , , , , , 2>/dev/null`, , then, it., , , toupper(), , {, A-Z", _toupper_str, , , , , , /', , }, , , , , , , , , i=2;, {, 1, , , 0, i++;, {, "$i, , , $i, , , , , , , automatically., lost!), , script:, , , , , , , 1>&2, $out_h, , , , , vop_default_desc, , , "$toupper"', {, descriptor., name), \n");, {, argname[i]);, , , name);, it., toupper(name)), length(protoarg), , {, argtype[i]), "), length(protoarg), {, protoarg), 4, 0, , , arglen, , , function., toupper(name)), {, , "), , , {, argname[i]);, , name);, name);, {, argname[i]);, , \n}\n",, name), , , , <sys/buf.h>\n"), , ", , , , , , , "$anal_retentive", , , */', , , 1>&2, $out_c, , , , , , {, , , , , , , , , , , , ', {, {, {, a_%s),\n",, , , , , , , , {, array, name), {, {, %s_args,a_%s),\n",, , , , , ";, F_desc, name), , , name, name), , , 0, {, {, {, VDESC_VPP_WILLRELE"), {, vpnum), , , , , , offsets, name), any), "), any), "), any), "), , "), information, \n");, , */\n"), , , ", , , , , , )
	struct vnode *dvp;
	struct componentname *cnp;
	int flags;
	 ;
	 ;
	braam+@pachelbel.coda.cs.cmu.edu Wed Oct 9 18:08:18 1996;
	braam+@pachelbel.coda.cs.cmu.edu ;
	from pachelbel.coda.cs.cmu.edu (PACHELBEL.CODA.CS.CMU.EDU [128.2.222.26]) by telemann.coda.cs.cmu.edu (8.7.4/8.7.3) with SMTP id SAA09451 for <braam@telemann.coda.cs.cmu.edu> Wed, 9 Oct 1996 18:08:17 -0400;
	braam+@pachelbel.coda.cs.cmu.edu ;
	<199610092208.SAA09451@telemann.coda.cs.cmu.edu> ;
	braam@pachelbel.coda.cs.cmu.edu ;
	Wed, 9 Oct 96 18:07:56 EDT;
	braam+@pachelbel.coda.cs.cmu.edu ;
	 ;
	 ;
	 ;
	Copyright (c) 1992, 1993;
	The Regents of the University of California. All rights reserved.;
	 ;
	Redistribution and use in source and binary forms, with or without;
	modification, are permitted provided that the following conditions;
	are met:;
	1. Redistributions of source code must retain the above copyright;
	notice, this list of conditions and the following disclaimer.;
	2. Redistributions in binary form must reproduce the above copyright;
	notice, this list of conditions and the following disclaimer in the;
	documentation and/or other materials provided with the distribution.;
	3. All advertising materials mentioning features or use of this software;
	must display the following acknowledgement:;
	This product includes software developed by the University of;
	California, Berkeley and its contributors.;
	4. Neither the name of the University nor the names of its contributors;
	may be used to endorse or promote products derived from this software;
	without specific prior written permission.;
	 ;
	THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND;
	ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE;
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE;
	ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE;
	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL;
	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS;
	OR SERVICES LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION);
	HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT;
	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY;
	OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF;
	SUCH DAMAGE.;
	 ;
	 ;
	vnode_if.sh,v 1.8 1995/03/10 04:13:52 chopps Exp $';
	 ;
	 ;
	[ $# -ne 1 ] then;
	'usage: vnode_if.sh srcfile';
	1 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	'BEGIN { print toupper("true") exit; }' 2>/dev/null`;
	 ;
	[ "$isgawk" = TRUE ] then;
	GNU awk provides it.;
	 ;
	 ;
	Provide our own toupper();
	 ;
	toupper(str) {;
	= "echo "str" |tr a-z A-Z";
	| getline _toupper_str;
	 ;
	_toupper_str ;
	 ;
	 ;
	\1:g ;
	/ /';
	 ;
	{ next };
	{ ;
	 ;
	 ;
	 ;
	{ ;
	 ;
	 ;
	 ;
	= $1 i=2;;
	($2 == "WILLRELE") {;
	= 1;
	 ;
	else ;
	= 0;
	= $i i++;;
	(i < NF) {;
	= argtype[argc]" "$i;
	 ;
	 ;
	= $i;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	Warning: This file is generated automatically.;
	(Modifications made here may easily be lost!);
	 ;
	Created by the script:;
	${SCRIPT_ID} ;
	 ;
	 ;
	 ;
	:\1:g' ;
	 ;
	"$0: Creating $out_h" 1>&2;
	> $out_h;
	 ;
	"$copyright" ;
	"$warning" ;
	' ;
	struct vnodeop_desc vop_default_desc;
	 ;
	 ;
	-e "$sed_prep" $src | $awk "$toupper"';
	doit() {;
	Declare arg struct, descriptor.;
	%s_args {\n", name);
	vnodeop_desc *a_desc \n");;
	(i=0 i<argc; i++) {;
	a_%s \n", argtype[i], argname[i]);;
	 ;
	\n"); ;
	struct vnodeop_desc %s_desc \n", name);;
	Prototype it.;
	= sprintf("static __inline int %s __P((", toupper(name));
	= length(protoarg);
	protoarg) ;
	(i=0 i<argc; i++) {;
	= sprintf("%s", argtype[i]);
	(i < (argc-1)) protoarg = (protoarg ", ");
	= length(protoarg);
	((protolen + arglen) > 77) {;
	= ("\n " protoarg);
	+= 4;
	= 0;
	 ;
	protoarg) ;
	+= arglen;
	 ;
	\n"); ;
	Define inline function.;
	__inline int %s(", toupper(name));
	(i=0 i<argc; i++) {;
	argname[i]) ;
	(i < (argc-1)) printf(", ");
	 ;
	 ;
	(i=0 i<argc; i++) {;
	%s \n", argtype[i], argname[i]);;
	 ;
	%s_args a \n", name);;
	= VDESC(%s) \n", name);;
	(i=0 i<argc; i++) {;
	= %s \n", argname[i], argname[i]);;
	 ;
	(VCALL(%s%s, VOFFSET(%s), &a)) \n}\n",;
	arg0special, name);
	{ ;
	 ;
	{ ;
	Special cases: */\n#include <sys/buf.h>\n");
	 ;
	buf *";
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	| sed -e "$anal_retentive";
	 ;
	' ;
	End of special cases. */';
	 ;
	 ;
	"$0: Creating $out_c" 1>&2;
	> $out_c;
	 ;
	"$copyright" ;
	"$warning" ;
	' ;
	 ;
	vnodeop_desc vop_default_desc = {;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	-e "$sed_prep" $src | $awk ';
	do_offset(typematch) {;
	(i=0 i<argc; i++) {;
	(argtype[i] == typematch) {;
	%s_args, a_%s),\n",;
	argname[i]) ;
	i ;
	 ;
	 ;
	"\tVDESC_NO_OFFSET," ;
	-1 ;
	 ;
	doit() {;
	Define offsets array;
	%s_vp_offsets[] = {\n", name);
	(i=0 i<argc; i++) {;
	(argtype[i] == "struct vnode *") {;
	("\tVOPARG_OFFSETOF(struct %s_args,a_%s),\n",;
	argname[i]) ;
	 ;
	 ;
	"\tVDESC_NO_OFFSET" ;
	"} ";;
	Define F_desc;
	vnodeop_desc %s_desc = {\n", name);
	offset ;
	("\t0,\n") ;
	printable name;
	("\t\"%s\",\n", name);
	flags ;
	 ;
	= 0;
	(i=0 i<argc; i++) {;
	(willrele[i]) {;
	(argdir[i] ~ /OUT/) {;
	| VDESC_VPP_WILLRELE");
	else {;
	| VDESC_VP%s_WILLRELE", vpnum);
	 ;
	 ;
	 ;
	 ;
	"," ;
	vp offsets;
	("\t%s_vp_offsets,\n", name);
	vpp (if any);
	vnode **");
	cred (if any);
	ucred *");
	proc (if any);
	proc *");
	componentname ;
	componentname *");
	transport layer information;
	("\tNULL,\n} \n");;
	{ ;
	Special cases: */\n");
	 ;
	 ;
	buf *";
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
{
	struct vop_whiteout_args a;
	a.a_desc = VDESC(vop_whiteout);
	a.a_dvp = dvp;
	a.a_cnp = cnp;
	a.a_flags = flags;
	a.a_ = ;
	a.a_ = ;
	a.a_1996 = 1996;
	a.a_ = ;
	a.a_-0400 = -0400;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_EDT = EDT;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_1993 = 1993;
	a.a_reserved. = reserved.;
	a.a_ = ;
	a.a_without = without;
	a.a_conditions = conditions;
	a.a_met: = met:;
	a.a_copyright = copyright;
	a.a_disclaimer. = disclaimer.;
	a.a_copyright = copyright;
	a.a_the = the;
	a.a_distribution. = distribution.;
	a.a_software = software;
	a.a_acknowledgement: = acknowledgement:;
	a.a_of = of;
	a.a_contributors. = contributors.;
	a.a_contributors = contributors;
	a.a_software = software;
	a.a_permission. = permission.;
	a.a_ = ;
	a.a_AND = AND;
	a.a_THE = THE;
	a.a_PURPOSE = PURPOSE;
	a.a_LIABLE = LIABLE;
	a.a_CONSEQUENTIAL = CONSEQUENTIAL;
	a.a_GOODS = GOODS;
	a.a_INTERRUPTION) = INTERRUPTION);
	a.a_STRICT = STRICT;
	a.a_WAY = WAY;
	a.a_OF = OF;
	a.a_DAMAGE. = DAMAGE.;
	a.a_ = ;
	a.a_ = ;
	a.a_$' = $';
	a.a_ = ;
	a.a_ = ;
	a.a_then = then;
	a.a_srcfile' = srcfile';
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_2>/dev/null` = 2>/dev/null`;
	a.a_ = ;
	a.a_then = then;
	a.a_it. = it.;
	a.a_ = ;
	a.a_ = ;
	a.a_toupper() = toupper();
	a.a_ = ;
	a.a_{ = {;
	a.a_A-Z" = A-Z";
	a.a__toupper_str = _toupper_str;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_/' = /';
	a.a_ = ;
	a.a_} = };
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_i=2; = i=2;;
	a.a_{ = {;
	a.a_1 = 1;
	a.a_ = ;
	a.a_ = ;
	a.a_0 = 0;
	a.a_i++; = i++;;
	a.a_{ = {;
	a.a_"$i = "$i;
	a.a_ = ;
	a.a_ = ;
	a.a_$i = $i;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_automatically. = automatically.;
	a.a_lost!) = lost!);
	a.a_ = ;
	a.a_script: = script:;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_1>&2 = 1>&2;
	a.a_$out_h = $out_h;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_vop_default_desc = vop_default_desc;
	a.a_ = ;
	a.a_ = ;
	a.a_"$toupper"' = "$toupper"';
	a.a_{ = {;
	a.a_descriptor. = descriptor.;
	a.a_name) = name);
	a.a_\n"); = \n");;
	a.a_{ = {;
	a.a_argname[i]); = argname[i]);;
	a.a_ = ;
	a.a_ = ;
	a.a_name); = name);;
	a.a_it. = it.;
	a.a_toupper(name)) = toupper(name));
	a.a_length(protoarg) = length(protoarg);
	a.a_ = ;
	a.a_{ = {;
	a.a_argtype[i]) = argtype[i]);
	a.a_") = ");
	a.a_length(protoarg) = length(protoarg);
	a.a_{ = {;
	a.a_protoarg) = protoarg);
	a.a_4 = 4;
	a.a_0 = 0;
	a.a_ = ;
	a.a_ = ;
	a.a_arglen = arglen;
	a.a_ = ;
	a.a_ = ;
	a.a_function. = function.;
	a.a_toupper(name)) = toupper(name));
	a.a_{ = {;
	a.a_ = ;
	a.a_") = ");
	a.a_ = ;
	a.a_ = ;
	a.a_{ = {;
	a.a_argname[i]); = argname[i]);;
	a.a_ = ;
	a.a_name); = name);;
	a.a_name); = name);;
	a.a_{ = {;
	a.a_argname[i]); = argname[i]);;
	a.a_ = ;
	a.a_\n}\n", = \n}\n",;
	a.a_name) = name);
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_<sys/buf.h>\n") = <sys/buf.h>\n");
	a.a_ = ;
	a.a_" = ";
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_"$anal_retentive" = "$anal_retentive";
	a.a_ = ;
	a.a_ = ;
	a.a_*/' = */';
	a.a_ = ;
	a.a_ = ;
	a.a_1>&2 = 1>&2;
	a.a_$out_c = $out_c;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_{ = {;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_' = ';
	a.a_{ = {;
	a.a_{ = {;
	a.a_{ = {;
	a.a_a_%s),\n", = a_%s),\n",;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_{ = {;
	a.a_array = array;
	a.a_name) = name);
	a.a_{ = {;
	a.a_{ = {;
	a.a_%s_args,a_%s),\n", = %s_args,a_%s),\n",;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_"; = ";;
	a.a_F_desc = F_desc;
	a.a_name) = name);
	a.a_ = ;
	a.a_ = ;
	a.a_name = name;
	a.a_name) = name);
	a.a_ = ;
	a.a_ = ;
	a.a_0 = 0;
	a.a_{ = {;
	a.a_{ = {;
	a.a_{ = {;
	a.a_VDESC_VPP_WILLRELE") = VDESC_VPP_WILLRELE");
	a.a_{ = {;
	a.a_vpnum) = vpnum);
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_offsets = offsets;
	a.a_name) = name);
	a.a_any) = any);
	a.a_") = ");
	a.a_any) = any);
	a.a_") = ");
	a.a_any) = any);
	a.a_") = ");
	a.a_ = ;
	a.a_") = ");
	a.a_information = information;
	a.a_\n"); = \n");;
	a.a_ = ;
	a.a_*/\n") = */\n");
	a.a_ = ;
	a.a_ = ;
	a.a_" = ";
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	return (VCALL(dvp, VOFFSET(vop_whiteout), &a));
}

struct vop_whiteout_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct componentname *a_cnp;
	int a_flags;
	 a_;
	 a_;
	braam+@pachelbel.coda.cs.cmu.edu Wed Oct 9 18:08:18 a_1996;
	braam+@pachelbel.coda.cs.cmu.edu a_;
	from pachelbel.coda.cs.cmu.edu (PACHELBEL.CODA.CS.CMU.EDU [128.2.222.26]) by telemann.coda.cs.cmu.edu (8.7.4/8.7.3) with SMTP id SAA09451 for <braam@telemann.coda.cs.cmu.edu> Wed, 9 Oct 1996 18:08:17 a_-0400;
	braam+@pachelbel.coda.cs.cmu.edu a_;
	<199610092208.SAA09451@telemann.coda.cs.cmu.edu> a_;
	braam@pachelbel.coda.cs.cmu.edu a_;
	Wed, 9 Oct 96 18:07:56 a_EDT;
	braam+@pachelbel.coda.cs.cmu.edu a_;
	 a_;
	 a_;
	 a_;
	Copyright (c) 1992, a_1993;
	The Regents of the University of California. All rights a_reserved.;
	 a_;
	Redistribution and use in source and binary forms, with or a_without;
	modification, are permitted provided that the following a_conditions;
	are a_met:;
	1. Redistributions of source code must retain the above a_copyright;
	notice, this list of conditions and the following a_disclaimer.;
	2. Redistributions in binary form must reproduce the above a_copyright;
	notice, this list of conditions and the following disclaimer in a_the;
	documentation and/or other materials provided with the a_distribution.;
	3. All advertising materials mentioning features or use of this a_software;
	must display the following a_acknowledgement:;
	This product includes software developed by the University a_of;
	California, Berkeley and its a_contributors.;
	4. Neither the name of the University nor the names of its a_contributors;
	may be used to endorse or promote products derived from this a_software;
	without specific prior written a_permission.;
	 a_;
	THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' a_AND;
	ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, a_THE;
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR a_PURPOSE;
	ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE a_LIABLE;
	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR a_CONSEQUENTIAL;
	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE a_GOODS;
	OR SERVICES LOSS OF USE, DATA, OR PROFITS; OR BUSINESS a_INTERRUPTION);
	HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, a_STRICT;
	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY a_WAY;
	OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY a_OF;
	SUCH a_DAMAGE.;
	 a_;
	 a_;
	vnode_if.sh,v 1.8 1995/03/10 04:13:52 chopps Exp a_$';
	 a_;
	 a_;
	[ $# -ne 1 ] a_then;
	'usage: vnode_if.sh a_srcfile';
	1 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	'BEGIN { print toupper("true") exit; }' a_2>/dev/null`;
	 a_;
	[ "$isgawk" = TRUE ] a_then;
	GNU awk provides a_it.;
	 a_;
	 a_;
	Provide our own a_toupper();
	 a_;
	toupper(str) a_{;
	= "echo "str" |tr a-z a_A-Z";
	| getline a__toupper_str;
	 a_;
	_toupper_str a_;
	 a_;
	 a_;
	\1:g a_;
	/ a_/';
	 a_;
	{ next a_};
	{ a_;
	 a_;
	 a_;
	 a_;
	{ a_;
	 a_;
	 a_;
	 a_;
	= $1 a_i=2;;
	($2 == "WILLRELE") a_{;
	= a_1;
	 a_;
	else a_;
	= a_0;
	= $i a_i++;;
	(i < NF) a_{;
	= argtype[argc]" a_"$i;
	 a_;
	 a_;
	= a_$i;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	Warning: This file is generated a_automatically.;
	(Modifications made here may easily be a_lost!);
	 a_;
	Created by the a_script:;
	${SCRIPT_ID} a_;
	 a_;
	 a_;
	 a_;
	:\1:g' a_;
	 a_;
	"$0: Creating $out_h" a_1>&2;
	> a_$out_h;
	 a_;
	"$copyright" a_;
	"$warning" a_;
	' a_;
	struct vnodeop_desc a_vop_default_desc;
	 a_;
	 a_;
	-e "$sed_prep" $src | $awk a_"$toupper"';
	doit() a_{;
	Declare arg struct, a_descriptor.;
	%s_args {\n", a_name);
	vnodeop_desc *a_desc a_\n");;
	(i=0 i<argc; i++) a_{;
	a_%s \n", argtype[i], a_argname[i]);;
	 a_;
	\n"); a_;
	struct vnodeop_desc %s_desc \n", a_name);;
	Prototype a_it.;
	= sprintf("static __inline int %s __P((", a_toupper(name));
	= a_length(protoarg);
	protoarg) a_;
	(i=0 i<argc; i++) a_{;
	= sprintf("%s", a_argtype[i]);
	(i < (argc-1)) protoarg = (protoarg ", a_");
	= a_length(protoarg);
	((protolen + arglen) > 77) a_{;
	= ("\n " a_protoarg);
	+= a_4;
	= a_0;
	 a_;
	protoarg) a_;
	+= a_arglen;
	 a_;
	\n"); a_;
	Define inline a_function.;
	__inline int %s(", a_toupper(name));
	(i=0 i<argc; i++) a_{;
	argname[i]) a_;
	(i < (argc-1)) printf(", a_");
	 a_;
	 a_;
	(i=0 i<argc; i++) a_{;
	%s \n", argtype[i], a_argname[i]);;
	 a_;
	%s_args a \n", a_name);;
	= VDESC(%s) \n", a_name);;
	(i=0 i<argc; i++) a_{;
	= %s \n", argname[i], a_argname[i]);;
	 a_;
	(VCALL(%s%s, VOFFSET(%s), &a)) a_\n}\n",;
	arg0special, a_name);
	{ a_;
	 a_;
	{ a_;
	Special cases: */\n#include a_<sys/buf.h>\n");
	 a_;
	buf *a_";
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	| sed -e a_"$anal_retentive";
	 a_;
	' a_;
	End of special cases. a_*/';
	 a_;
	 a_;
	"$0: Creating $out_c" a_1>&2;
	> a_$out_c;
	 a_;
	"$copyright" a_;
	"$warning" a_;
	' a_;
	 a_;
	vnodeop_desc vop_default_desc = a_{;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	-e "$sed_prep" $src | $awk a_';
	do_offset(typematch) a_{;
	(i=0 i<argc; i++) a_{;
	(argtype[i] == typematch) a_{;
	%s_args, a_a_%s),\n",;
	argname[i]) a_;
	i a_;
	 a_;
	 a_;
	"\tVDESC_NO_OFFSET," a_;
	-1 a_;
	 a_;
	doit() a_{;
	Define offsets a_array;
	%s_vp_offsets[] = {\n", a_name);
	(i=0 i<argc; i++) a_{;
	(argtype[i] == "struct vnode *") a_{;
	("\tVOPARG_OFFSETOF(struct a_%s_args,a_%s),\n",;
	argname[i]) a_;
	 a_;
	 a_;
	"\tVDESC_NO_OFFSET" a_;
	"} a_";;
	Define a_F_desc;
	vnodeop_desc %s_desc = {\n", a_name);
	offset a_;
	("\t0,\n") a_;
	printable a_name;
	("\t\"%s\",\n", a_name);
	flags a_;
	 a_;
	= a_0;
	(i=0 i<argc; i++) a_{;
	(willrele[i]) a_{;
	(argdir[i] ~ /OUT/) a_{;
	| a_VDESC_VPP_WILLRELE");
	else a_{;
	| VDESC_VP%s_WILLRELE", a_vpnum);
	 a_;
	 a_;
	 a_;
	 a_;
	"," a_;
	vp a_offsets;
	("\t%s_vp_offsets,\n", a_name);
	vpp (if a_any);
	vnode **a_");
	cred (if a_any);
	ucred *a_");
	proc (if a_any);
	proc *a_");
	componentname a_;
	componentname *a_");
	transport layer a_information;
	("\tNULL,\n} a_\n");;
	{ a_;
	Special cases: a_*/\n");
	 a_;
	 a_;
	buf *a_";
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	| sed -e a_"$anal_retentive";
	 a_;
	' a_;
	End of special cases. a_*/';
	 a_;
	' a_;
	vnodeop_desc *vfs_op_descs[] = a_{;
	/* MUST BE FIRST a_*/;
	/* XXX: SPECIAL CASE a_*/;
	/* XXX: SPECIAL CASE a_*/;
	 a_;
	 a_;
	-e "$sed_prep" $src | $awk a_';
	doit() a_{;
	name) a_;
};
extern struct vnodeop_desc vop_whiteout_desc;
static __inline int VOP_WHITEOUT __P((struct vnode *, 
    struct componentname *, int, , , 
    braam+@pachelbel.coda.cs.cmu.edu Wed Oct 9 18:08:18, 
    braam+@pachelbel.coda.cs.cmu.edu, 
    from pachelbel.coda.cs.cmu.edu (PACHELBEL.CODA.CS.CMU.EDU [128.2.222.26]) by telemann.coda.cs.cmu.edu (8.7.4/8.7.3) with SMTP id SAA09451 for <braam@telemann.coda.cs.cmu.edu> Wed, 9 Oct 1996 18:08:17, 
    braam+@pachelbel.coda.cs.cmu.edu, 
    <199610092208.SAA09451@telemann.coda.cs.cmu.edu>, 
    braam@pachelbel.coda.cs.cmu.edu, Wed, 9 Oct 96 18:07:56, 
    braam+@pachelbel.coda.cs.cmu.edu, , , , Copyright (c) 1992,, 
    The Regents of the University of California. All rights, , 
    Redistribution and use in source and binary forms, with or, 
    modification, are permitted provided that the following, are, 
    1. Redistributions of source code must retain the above, 
    notice, this list of conditions and the following, 
    2. Redistributions in binary form must reproduce the above, 
    notice, this list of conditions and the following disclaimer in, 
    documentation and/or other materials provided with the, 
    3. All advertising materials mentioning features or use of this, 
    must display the following, 
    This product includes software developed by the University, 
    California, Berkeley and its, 
    4. Neither the name of the University nor the names of its, 
    may be used to endorse or promote products derived from this, 
    without specific prior written, , 
    THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'', 
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,, 
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR, 
    ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE, 
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR, 
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE, 
    OR SERVICES LOSS OF USE, DATA, OR PROFITS; OR BUSINESS, 
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,, 
    LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY, 
    OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY, 
    SUCH, , , vnode_if.sh,v 1.8 1995/03/10 04:13:52 chopps Exp, , , 
    [ $# -ne 1 ], 'usage: vnode_if.sh, 1, , , , , , , , , , 
    'BEGIN { print toupper("true") exit; }', , [ "$isgawk" = TRUE ], 
    GNU awk provides, , , Provide our own, , toupper(str), 
    = "echo "str" |tr a-z, | getline, , _toupper_str, , , \1:g, /, , { next, 
    {, , , , {, , , , = $1, ($2 == "WILLRELE"), =, , else, =, = $i, 
    (i < NF), = argtype[argc]", , , =, , , , , , , 
    Warning: This file is generated, (Modifications made here may easily be, 
    , Created by the, ${SCRIPT_ID}, , , , :\1:g', , "$0: Creating $out_h", 
    >, , "$copyright", "$warning", ', struct vnodeop_desc, , , 
    -e "$sed_prep" $src | $awk, doit(), Declare arg struct,, %s_args {\n",, 
    vnodeop_desc *a_desc, (i=0 i<argc; i++), a_%s \n", argtype[i],, , 
    \n");, struct vnodeop_desc %s_desc \n",, Prototype, 
    = sprintf("static __inline int %s __P((",, =, protoarg), 
    (i=0 i<argc; i++), = sprintf("%s",, 
    (i < (argc-1)) protoarg = (protoarg ",, =, ((protolen + arglen) > 77), 
    = ("\n ", +=, =, , protoarg), +=, , \n");, Define inline, 
    __inline int %s(",, (i=0 i<argc; i++), argname[i]), 
    (i < (argc-1)) printf(",, , , (i=0 i<argc; i++), %s \n", argtype[i],, , 
    %s_args a \n",, = VDESC(%s) \n",, (i=0 i<argc; i++), 
    = %s \n", argname[i],, , (VCALL(%s%s, VOFFSET(%s), &a)), arg0special,, 
    {, , {, Special cases: */\n#include, , buf *, , , , , , , | sed -e, , ', 
    End of special cases., , , "$0: Creating $out_c", >, , "$copyright", 
    "$warning", ', , vnodeop_desc vop_default_desc =, , , , , , , , , , , , 
    -e "$sed_prep" $src | $awk, do_offset(typematch), (i=0 i<argc; i++), 
    (argtype[i] == typematch), %s_args,, argname[i]), i, , , 
    "\tVDESC_NO_OFFSET,", -1, , doit(), Define offsets, 
    %s_vp_offsets[] = {\n",, (i=0 i<argc; i++), 
    (argtype[i] == "struct vnode *"), ("\tVOPARG_OFFSETOF(struct, 
    argname[i]), , , "\tVDESC_NO_OFFSET", "}, Define, 
    vnodeop_desc %s_desc = {\n",, offset, ("\t0,\n"), printable, 
    ("\t\"%s\",\n",, flags, , =, (i=0 i<argc; i++), (willrele[i]), 
    (argdir[i] ~ /OUT/), |, else, | VDESC_VP%s_WILLRELE",, , , , , ",", vp, 
    ("\t%s_vp_offsets,\n",, vpp (if, vnode **, cred (if, ucred *, proc (if, 
    proc *, componentname, componentname *, transport layer, ("\tNULL,\n}, 
    {, Special cases:, , , buf *, , , , , , , | sed -e, , ', 
    End of special cases., , ', vnodeop_desc *vfs_op_descs[] =, 
    /* MUST BE FIRST, /* XXX: SPECIAL CASE, /* XXX: SPECIAL CASE, , , 
    -e "$sed_prep" $src | $awk, doit(), name)));
static __inline int VOP_WHITEOUT(dvp, cnp, flags, , , 1996, , -0400, , , , EDT, , , , , 1993, reserved., , without, conditions, met:, copyright, disclaimer., copyright, the, distribution., software, acknowledgement:, of, contributors., contributors, software, permission., , AND, THE, PURPOSE, LIABLE, CONSEQUENTIAL, GOODS, INTERRUPTION), STRICT, WAY, OF, DAMAGE., , , $', , , then, srcfile', , , , , , , , , , , 2>/dev/null`, , then, it., , , toupper(), , {, A-Z", _toupper_str, , , , , , /', , }, , , , , , , , , i=2;, {, 1, , , 0, i++;, {, "$i, , , $i, , , , , , , automatically., lost!), , script:, , , , , , , 1>&2, $out_h, , , , , vop_default_desc, , , "$toupper"', {, descriptor., name), \n");, {, argname[i]);, , , name);, it., toupper(name)), length(protoarg), , {, argtype[i]), "), length(protoarg), {, protoarg), 4, 0, , , arglen, , , function., toupper(name)), {, , "), , , {, argname[i]);, , name);, name);, {, argname[i]);, , \n}\n",, name), , , , <sys/buf.h>\n"), , ", , , , , , , "$anal_retentive", , , */', , , 1>&2, $out_c, , , , , , {, , , , , , , , , , , , ', {, {, {, a_%s),\n",, , , , , , , , {, array, name), {, {, %s_args,a_%s),\n",, , , , , ";, F_desc, name), , , name, name), , , 0, {, {, {, VDESC_VPP_WILLRELE"), {, vpnum), , , , , , offsets, name), any), "), any), "), any), "), , "), information, \n");, , */\n"), , , ", , , , , , , "$anal_retentive", , , */', , , {, */, */, */, , , ', {, )
	struct vnode *dvp;
	struct componentname *cnp;
	int flags;
	 ;
	 ;
	braam+@pachelbel.coda.cs.cmu.edu Wed Oct 9 18:08:18 1996;
	braam+@pachelbel.coda.cs.cmu.edu ;
	from pachelbel.coda.cs.cmu.edu (PACHELBEL.CODA.CS.CMU.EDU [128.2.222.26]) by telemann.coda.cs.cmu.edu (8.7.4/8.7.3) with SMTP id SAA09451 for <braam@telemann.coda.cs.cmu.edu> Wed, 9 Oct 1996 18:08:17 -0400;
	braam+@pachelbel.coda.cs.cmu.edu ;
	<199610092208.SAA09451@telemann.coda.cs.cmu.edu> ;
	braam@pachelbel.coda.cs.cmu.edu ;
	Wed, 9 Oct 96 18:07:56 EDT;
	braam+@pachelbel.coda.cs.cmu.edu ;
	 ;
	 ;
	 ;
	Copyright (c) 1992, 1993;
	The Regents of the University of California. All rights reserved.;
	 ;
	Redistribution and use in source and binary forms, with or without;
	modification, are permitted provided that the following conditions;
	are met:;
	1. Redistributions of source code must retain the above copyright;
	notice, this list of conditions and the following disclaimer.;
	2. Redistributions in binary form must reproduce the above copyright;
	notice, this list of conditions and the following disclaimer in the;
	documentation and/or other materials provided with the distribution.;
	3. All advertising materials mentioning features or use of this software;
	must display the following acknowledgement:;
	This product includes software developed by the University of;
	California, Berkeley and its contributors.;
	4. Neither the name of the University nor the names of its contributors;
	may be used to endorse or promote products derived from this software;
	without specific prior written permission.;
	 ;
	THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND;
	ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE;
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE;
	ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE;
	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL;
	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS;
	OR SERVICES LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION);
	HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT;
	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY;
	OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF;
	SUCH DAMAGE.;
	 ;
	 ;
	vnode_if.sh,v 1.8 1995/03/10 04:13:52 chopps Exp $';
	 ;
	 ;
	[ $# -ne 1 ] then;
	'usage: vnode_if.sh srcfile';
	1 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	'BEGIN { print toupper("true") exit; }' 2>/dev/null`;
	 ;
	[ "$isgawk" = TRUE ] then;
	GNU awk provides it.;
	 ;
	 ;
	Provide our own toupper();
	 ;
	toupper(str) {;
	= "echo "str" |tr a-z A-Z";
	| getline _toupper_str;
	 ;
	_toupper_str ;
	 ;
	 ;
	\1:g ;
	/ /';
	 ;
	{ next };
	{ ;
	 ;
	 ;
	 ;
	{ ;
	 ;
	 ;
	 ;
	= $1 i=2;;
	($2 == "WILLRELE") {;
	= 1;
	 ;
	else ;
	= 0;
	= $i i++;;
	(i < NF) {;
	= argtype[argc]" "$i;
	 ;
	 ;
	= $i;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	Warning: This file is generated automatically.;
	(Modifications made here may easily be lost!);
	 ;
	Created by the script:;
	${SCRIPT_ID} ;
	 ;
	 ;
	 ;
	:\1:g' ;
	 ;
	"$0: Creating $out_h" 1>&2;
	> $out_h;
	 ;
	"$copyright" ;
	"$warning" ;
	' ;
	struct vnodeop_desc vop_default_desc;
	 ;
	 ;
	-e "$sed_prep" $src | $awk "$toupper"';
	doit() {;
	Declare arg struct, descriptor.;
	%s_args {\n", name);
	vnodeop_desc *a_desc \n");;
	(i=0 i<argc; i++) {;
	a_%s \n", argtype[i], argname[i]);;
	 ;
	\n"); ;
	struct vnodeop_desc %s_desc \n", name);;
	Prototype it.;
	= sprintf("static __inline int %s __P((", toupper(name));
	= length(protoarg);
	protoarg) ;
	(i=0 i<argc; i++) {;
	= sprintf("%s", argtype[i]);
	(i < (argc-1)) protoarg = (protoarg ", ");
	= length(protoarg);
	((protolen + arglen) > 77) {;
	= ("\n " protoarg);
	+= 4;
	= 0;
	 ;
	protoarg) ;
	+= arglen;
	 ;
	\n"); ;
	Define inline function.;
	__inline int %s(", toupper(name));
	(i=0 i<argc; i++) {;
	argname[i]) ;
	(i < (argc-1)) printf(", ");
	 ;
	 ;
	(i=0 i<argc; i++) {;
	%s \n", argtype[i], argname[i]);;
	 ;
	%s_args a \n", name);;
	= VDESC(%s) \n", name);;
	(i=0 i<argc; i++) {;
	= %s \n", argname[i], argname[i]);;
	 ;
	(VCALL(%s%s, VOFFSET(%s), &a)) \n}\n",;
	arg0special, name);
	{ ;
	 ;
	{ ;
	Special cases: */\n#include <sys/buf.h>\n");
	 ;
	buf *";
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	| sed -e "$anal_retentive";
	 ;
	' ;
	End of special cases. */';
	 ;
	 ;
	"$0: Creating $out_c" 1>&2;
	> $out_c;
	 ;
	"$copyright" ;
	"$warning" ;
	' ;
	 ;
	vnodeop_desc vop_default_desc = {;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	-e "$sed_prep" $src | $awk ';
	do_offset(typematch) {;
	(i=0 i<argc; i++) {;
	(argtype[i] == typematch) {;
	%s_args, a_%s),\n",;
	argname[i]) ;
	i ;
	 ;
	 ;
	"\tVDESC_NO_OFFSET," ;
	-1 ;
	 ;
	doit() {;
	Define offsets array;
	%s_vp_offsets[] = {\n", name);
	(i=0 i<argc; i++) {;
	(argtype[i] == "struct vnode *") {;
	("\tVOPARG_OFFSETOF(struct %s_args,a_%s),\n",;
	argname[i]) ;
	 ;
	 ;
	"\tVDESC_NO_OFFSET" ;
	"} ";;
	Define F_desc;
	vnodeop_desc %s_desc = {\n", name);
	offset ;
	("\t0,\n") ;
	printable name;
	("\t\"%s\",\n", name);
	flags ;
	 ;
	= 0;
	(i=0 i<argc; i++) {;
	(willrele[i]) {;
	(argdir[i] ~ /OUT/) {;
	| VDESC_VPP_WILLRELE");
	else {;
	| VDESC_VP%s_WILLRELE", vpnum);
	 ;
	 ;
	 ;
	 ;
	"," ;
	vp offsets;
	("\t%s_vp_offsets,\n", name);
	vpp (if any);
	vnode **");
	cred (if any);
	ucred *");
	proc (if any);
	proc *");
	componentname ;
	componentname *");
	transport layer information;
	("\tNULL,\n} \n");;
	{ ;
	Special cases: */\n");
	 ;
	 ;
	buf *";
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	| sed -e "$anal_retentive";
	 ;
	' ;
	End of special cases. */';
	 ;
	' ;
	vnodeop_desc *vfs_op_descs[] = {;
	/* MUST BE FIRST */;
	/* XXX: SPECIAL CASE */;
	/* XXX: SPECIAL CASE */;
	 ;
	 ;
	-e "$sed_prep" $src | $awk ';
	doit() {;
	name) ;
{
	struct vop_whiteout_args a;
	a.a_desc = VDESC(vop_whiteout);
	a.a_dvp = dvp;
	a.a_cnp = cnp;
	a.a_flags = flags;
	a.a_ = ;
	a.a_ = ;
	a.a_1996 = 1996;
	a.a_ = ;
	a.a_-0400 = -0400;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_EDT = EDT;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_1993 = 1993;
	a.a_reserved. = reserved.;
	a.a_ = ;
	a.a_without = without;
	a.a_conditions = conditions;
	a.a_met: = met:;
	a.a_copyright = copyright;
	a.a_disclaimer. = disclaimer.;
	a.a_copyright = copyright;
	a.a_the = the;
	a.a_distribution. = distribution.;
	a.a_software = software;
	a.a_acknowledgement: = acknowledgement:;
	a.a_of = of;
	a.a_contributors. = contributors.;
	a.a_contributors = contributors;
	a.a_software = software;
	a.a_permission. = permission.;
	a.a_ = ;
	a.a_AND = AND;
	a.a_THE = THE;
	a.a_PURPOSE = PURPOSE;
	a.a_LIABLE = LIABLE;
	a.a_CONSEQUENTIAL = CONSEQUENTIAL;
	a.a_GOODS = GOODS;
	a.a_INTERRUPTION) = INTERRUPTION);
	a.a_STRICT = STRICT;
	a.a_WAY = WAY;
	a.a_OF = OF;
	a.a_DAMAGE. = DAMAGE.;
	a.a_ = ;
	a.a_ = ;
	a.a_$' = $';
	a.a_ = ;
	a.a_ = ;
	a.a_then = then;
	a.a_srcfile' = srcfile';
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_2>/dev/null` = 2>/dev/null`;
	a.a_ = ;
	a.a_then = then;
	a.a_it. = it.;
	a.a_ = ;
	a.a_ = ;
	a.a_toupper() = toupper();
	a.a_ = ;
	a.a_{ = {;
	a.a_A-Z" = A-Z";
	a.a__toupper_str = _toupper_str;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_/' = /';
	a.a_ = ;
	a.a_} = };
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_i=2; = i=2;;
	a.a_{ = {;
	a.a_1 = 1;
	a.a_ = ;
	a.a_ = ;
	a.a_0 = 0;
	a.a_i++; = i++;;
	a.a_{ = {;
	a.a_"$i = "$i;
	a.a_ = ;
	a.a_ = ;
	a.a_$i = $i;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_automatically. = automatically.;
	a.a_lost!) = lost!);
	a.a_ = ;
	a.a_script: = script:;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_1>&2 = 1>&2;
	a.a_$out_h = $out_h;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_vop_default_desc = vop_default_desc;
	a.a_ = ;
	a.a_ = ;
	a.a_"$toupper"' = "$toupper"';
	a.a_{ = {;
	a.a_descriptor. = descriptor.;
	a.a_name) = name);
	a.a_\n"); = \n");;
	a.a_{ = {;
	a.a_argname[i]); = argname[i]);;
	a.a_ = ;
	a.a_ = ;
	a.a_name); = name);;
	a.a_it. = it.;
	a.a_toupper(name)) = toupper(name));
	a.a_length(protoarg) = length(protoarg);
	a.a_ = ;
	a.a_{ = {;
	a.a_argtype[i]) = argtype[i]);
	a.a_") = ");
	a.a_length(protoarg) = length(protoarg);
	a.a_{ = {;
	a.a_protoarg) = protoarg);
	a.a_4 = 4;
	a.a_0 = 0;
	a.a_ = ;
	a.a_ = ;
	a.a_arglen = arglen;
	a.a_ = ;
	a.a_ = ;
	a.a_function. = function.;
	a.a_toupper(name)) = toupper(name));
	a.a_{ = {;
	a.a_ = ;
	a.a_") = ");
	a.a_ = ;
	a.a_ = ;
	a.a_{ = {;
	a.a_argname[i]); = argname[i]);;
	a.a_ = ;
	a.a_name); = name);;
	a.a_name); = name);;
	a.a_{ = {;
	a.a_argname[i]); = argname[i]);;
	a.a_ = ;
	a.a_\n}\n", = \n}\n",;
	a.a_name) = name);
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_<sys/buf.h>\n") = <sys/buf.h>\n");
	a.a_ = ;
	a.a_" = ";
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_"$anal_retentive" = "$anal_retentive";
	a.a_ = ;
	a.a_ = ;
	a.a_*/' = */';
	a.a_ = ;
	a.a_ = ;
	a.a_1>&2 = 1>&2;
	a.a_$out_c = $out_c;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_{ = {;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_' = ';
	a.a_{ = {;
	a.a_{ = {;
	a.a_{ = {;
	a.a_a_%s),\n", = a_%s),\n",;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_{ = {;
	a.a_array = array;
	a.a_name) = name);
	a.a_{ = {;
	a.a_{ = {;
	a.a_%s_args,a_%s),\n", = %s_args,a_%s),\n",;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_"; = ";;
	a.a_F_desc = F_desc;
	a.a_name) = name);
	a.a_ = ;
	a.a_ = ;
	a.a_name = name;
	a.a_name) = name);
	a.a_ = ;
	a.a_ = ;
	a.a_0 = 0;
	a.a_{ = {;
	a.a_{ = {;
	a.a_{ = {;
	a.a_VDESC_VPP_WILLRELE") = VDESC_VPP_WILLRELE");
	a.a_{ = {;
	a.a_vpnum) = vpnum);
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_offsets = offsets;
	a.a_name) = name);
	a.a_any) = any);
	a.a_") = ");
	a.a_any) = any);
	a.a_") = ");
	a.a_any) = any);
	a.a_") = ");
	a.a_ = ;
	a.a_") = ");
	a.a_information = information;
	a.a_\n"); = \n");;
	a.a_ = ;
	a.a_*/\n") = */\n");
	a.a_ = ;
	a.a_ = ;
	a.a_" = ";
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_"$anal_retentive" = "$anal_retentive";
	a.a_ = ;
	a.a_ = ;
	a.a_*/' = */';
	a.a_ = ;
	a.a_ = ;
	a.a_{ = {;
	a.a_*/ = */;
	a.a_*/ = */;
	a.a_*/ = */;
	a.a_ = ;
	a.a_ = ;
	a.a_' = ';
	a.a_{ = {;
	a.a_ = ;
	return (VCALL(dvp, VOFFSET(vop_whiteout), &a));
}

struct vop_whiteout_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct componentname *a_cnp;
	int a_flags;
	 a_;
	 a_;
	braam+@pachelbel.coda.cs.cmu.edu Wed Oct 9 18:08:18 a_1996;
	braam+@pachelbel.coda.cs.cmu.edu a_;
	from pachelbel.coda.cs.cmu.edu (PACHELBEL.CODA.CS.CMU.EDU [128.2.222.26]) by telemann.coda.cs.cmu.edu (8.7.4/8.7.3) with SMTP id SAA09451 for <braam@telemann.coda.cs.cmu.edu> Wed, 9 Oct 1996 18:08:17 a_-0400;
	braam+@pachelbel.coda.cs.cmu.edu a_;
	<199610092208.SAA09451@telemann.coda.cs.cmu.edu> a_;
	braam@pachelbel.coda.cs.cmu.edu a_;
	Wed, 9 Oct 96 18:07:56 a_EDT;
	braam+@pachelbel.coda.cs.cmu.edu a_;
	 a_;
	 a_;
	 a_;
	Copyright (c) 1992, a_1993;
	The Regents of the University of California. All rights a_reserved.;
	 a_;
	Redistribution and use in source and binary forms, with or a_without;
	modification, are permitted provided that the following a_conditions;
	are a_met:;
	1. Redistributions of source code must retain the above a_copyright;
	notice, this list of conditions and the following a_disclaimer.;
	2. Redistributions in binary form must reproduce the above a_copyright;
	notice, this list of conditions and the following disclaimer in a_the;
	documentation and/or other materials provided with the a_distribution.;
	3. All advertising materials mentioning features or use of this a_software;
	must display the following a_acknowledgement:;
	This product includes software developed by the University a_of;
	California, Berkeley and its a_contributors.;
	4. Neither the name of the University nor the names of its a_contributors;
	may be used to endorse or promote products derived from this a_software;
	without specific prior written a_permission.;
	 a_;
	THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' a_AND;
	ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, a_THE;
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR a_PURPOSE;
	ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE a_LIABLE;
	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR a_CONSEQUENTIAL;
	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE a_GOODS;
	OR SERVICES LOSS OF USE, DATA, OR PROFITS; OR BUSINESS a_INTERRUPTION);
	HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, a_STRICT;
	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY a_WAY;
	OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY a_OF;
	SUCH a_DAMAGE.;
	 a_;
	 a_;
	vnode_if.sh,v 1.8 1995/03/10 04:13:52 chopps Exp a_$';
	 a_;
	 a_;
	[ $# -ne 1 ] a_then;
	'usage: vnode_if.sh a_srcfile';
	1 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	'BEGIN { print toupper("true") exit; }' a_2>/dev/null`;
	 a_;
	[ "$isgawk" = TRUE ] a_then;
	GNU awk provides a_it.;
	 a_;
	 a_;
	Provide our own a_toupper();
	 a_;
	toupper(str) a_{;
	= "echo "str" |tr a-z a_A-Z";
	| getline a__toupper_str;
	 a_;
	_toupper_str a_;
	 a_;
	 a_;
	\1:g a_;
	/ a_/';
	 a_;
	{ next a_};
	{ a_;
	 a_;
	 a_;
	 a_;
	{ a_;
	 a_;
	 a_;
	 a_;
	= $1 a_i=2;;
	($2 == "WILLRELE") a_{;
	= a_1;
	 a_;
	else a_;
	= a_0;
	= $i a_i++;;
	(i < NF) a_{;
	= argtype[argc]" a_"$i;
	 a_;
	 a_;
	= a_$i;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	Warning: This file is generated a_automatically.;
	(Modifications made here may easily be a_lost!);
	 a_;
	Created by the a_script:;
	${SCRIPT_ID} a_;
	 a_;
	 a_;
	 a_;
	:\1:g' a_;
	 a_;
	"$0: Creating $out_h" a_1>&2;
	> a_$out_h;
	 a_;
	"$copyright" a_;
	"$warning" a_;
	' a_;
	struct vnodeop_desc a_vop_default_desc;
	 a_;
	 a_;
	-e "$sed_prep" $src | $awk a_"$toupper"';
	doit() a_{;
	Declare arg struct, a_descriptor.;
	%s_args {\n", a_name);
	vnodeop_desc *a_desc a_\n");;
	(i=0 i<argc; i++) a_{;
	a_%s \n", argtype[i], a_argname[i]);;
	 a_;
	\n"); a_;
	struct vnodeop_desc %s_desc \n", a_name);;
	Prototype a_it.;
	= sprintf("static __inline int %s __P((", a_toupper(name));
	= a_length(protoarg);
	protoarg) a_;
	(i=0 i<argc; i++) a_{;
	= sprintf("%s", a_argtype[i]);
	(i < (argc-1)) protoarg = (protoarg ", a_");
	= a_length(protoarg);
	((protolen + arglen) > 77) a_{;
	= ("\n " a_protoarg);
	+= a_4;
	= a_0;
	 a_;
	protoarg) a_;
	+= a_arglen;
	 a_;
	\n"); a_;
	Define inline a_function.;
	__inline int %s(", a_toupper(name));
	(i=0 i<argc; i++) a_{;
	argname[i]) a_;
	(i < (argc-1)) printf(", a_");
	 a_;
	 a_;
	(i=0 i<argc; i++) a_{;
	%s \n", argtype[i], a_argname[i]);;
	 a_;
	%s_args a \n", a_name);;
	= VDESC(%s) \n", a_name);;
	(i=0 i<argc; i++) a_{;
	= %s \n", argname[i], a_argname[i]);;
	 a_;
	(VCALL(%s%s, VOFFSET(%s), &a)) a_\n}\n",;
	arg0special, a_name);
	{ a_;
	 a_;
	{ a_;
	Special cases: */\n#include a_<sys/buf.h>\n");
	 a_;
	buf *a_";
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	| sed -e a_"$anal_retentive";
	 a_;
	' a_;
	End of special cases. a_*/';
	 a_;
	 a_;
	"$0: Creating $out_c" a_1>&2;
	> a_$out_c;
	 a_;
	"$copyright" a_;
	"$warning" a_;
	' a_;
	 a_;
	vnodeop_desc vop_default_desc = a_{;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	-e "$sed_prep" $src | $awk a_';
	do_offset(typematch) a_{;
	(i=0 i<argc; i++) a_{;
	(argtype[i] == typematch) a_{;
	%s_args, a_a_%s),\n",;
	argname[i]) a_;
	i a_;
	 a_;
	 a_;
	"\tVDESC_NO_OFFSET," a_;
	-1 a_;
	 a_;
	doit() a_{;
	Define offsets a_array;
	%s_vp_offsets[] = {\n", a_name);
	(i=0 i<argc; i++) a_{;
	(argtype[i] == "struct vnode *") a_{;
	("\tVOPARG_OFFSETOF(struct a_%s_args,a_%s),\n",;
	argname[i]) a_;
	 a_;
	 a_;
	"\tVDESC_NO_OFFSET" a_;
	"} a_";;
	Define a_F_desc;
	vnodeop_desc %s_desc = {\n", a_name);
	offset a_;
	("\t0,\n") a_;
	printable a_name;
	("\t\"%s\",\n", a_name);
	flags a_;
	 a_;
	= a_0;
	(i=0 i<argc; i++) a_{;
	(willrele[i]) a_{;
	(argdir[i] ~ /OUT/) a_{;
	| a_VDESC_VPP_WILLRELE");
	else a_{;
	| VDESC_VP%s_WILLRELE", a_vpnum);
	 a_;
	 a_;
	 a_;
	 a_;
	"," a_;
	vp a_offsets;
	("\t%s_vp_offsets,\n", a_name);
	vpp (if a_any);
	vnode **a_");
	cred (if a_any);
	ucred *a_");
	proc (if a_any);
	proc *a_");
	componentname a_;
	componentname *a_");
	transport layer a_information;
	("\tNULL,\n} a_\n");;
	{ a_;
	Special cases: a_*/\n");
	 a_;
	 a_;
	buf *a_";
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	 a_;
	| sed -e a_"$anal_retentive";
	 a_;
	' a_;
	End of special cases. a_*/';
	 a_;
	' a_;
	vnodeop_desc *vfs_op_descs[] = a_{;
	/* MUST BE FIRST a_*/;
	/* XXX: SPECIAL CASE a_*/;
	/* XXX: SPECIAL CASE a_*/;
	 a_;
	 a_;
	-e "$sed_prep" $src | $awk a_';
	doit() a_{;
	name) a_;
	 a_;
	 a_;
	' a_NULL;
};
extern struct vnodeop_desc vop_whiteout_desc;
static __inline int VOP_WHITEOUT __P((struct vnode *, 
    struct componentname *, int, , , 
    braam+@pachelbel.coda.cs.cmu.edu Wed Oct 9 18:08:18, 
    braam+@pachelbel.coda.cs.cmu.edu, 
    from pachelbel.coda.cs.cmu.edu (PACHELBEL.CODA.CS.CMU.EDU [128.2.222.26]) by telemann.coda.cs.cmu.edu (8.7.4/8.7.3) with SMTP id SAA09451 for <braam@telemann.coda.cs.cmu.edu> Wed, 9 Oct 1996 18:08:17, 
    braam+@pachelbel.coda.cs.cmu.edu, 
    <199610092208.SAA09451@telemann.coda.cs.cmu.edu>, 
    braam@pachelbel.coda.cs.cmu.edu, Wed, 9 Oct 96 18:07:56, 
    braam+@pachelbel.coda.cs.cmu.edu, , , , Copyright (c) 1992,, 
    The Regents of the University of California. All rights, , 
    Redistribution and use in source and binary forms, with or, 
    modification, are permitted provided that the following, are, 
    1. Redistributions of source code must retain the above, 
    notice, this list of conditions and the following, 
    2. Redistributions in binary form must reproduce the above, 
    notice, this list of conditions and the following disclaimer in, 
    documentation and/or other materials provided with the, 
    3. All advertising materials mentioning features or use of this, 
    must display the following, 
    This product includes software developed by the University, 
    California, Berkeley and its, 
    4. Neither the name of the University nor the names of its, 
    may be used to endorse or promote products derived from this, 
    without specific prior written, , 
    THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'', 
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,, 
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR, 
    ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE, 
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR, 
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE, 
    OR SERVICES LOSS OF USE, DATA, OR PROFITS; OR BUSINESS, 
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,, 
    LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY, 
    OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY, 
    SUCH, , , vnode_if.sh,v 1.8 1995/03/10 04:13:52 chopps Exp, , , 
    [ $# -ne 1 ], 'usage: vnode_if.sh, 1, , , , , , , , , , 
    'BEGIN { print toupper("true") exit; }', , [ "$isgawk" = TRUE ], 
    GNU awk provides, , , Provide our own, , toupper(str), 
    = "echo "str" |tr a-z, | getline, , _toupper_str, , , \1:g, /, , { next, 
    {, , , , {, , , , = $1, ($2 == "WILLRELE"), =, , else, =, = $i, 
    (i < NF), = argtype[argc]", , , =, , , , , , , 
    Warning: This file is generated, (Modifications made here may easily be, 
    , Created by the, ${SCRIPT_ID}, , , , :\1:g', , "$0: Creating $out_h", 
    >, , "$copyright", "$warning", ', struct vnodeop_desc, , , 
    -e "$sed_prep" $src | $awk, doit(), Declare arg struct,, %s_args {\n",, 
    vnodeop_desc *a_desc, (i=0 i<argc; i++), a_%s \n", argtype[i],, , 
    \n");, struct vnodeop_desc %s_desc \n",, Prototype, 
    = sprintf("static __inline int %s __P((",, =, protoarg), 
    (i=0 i<argc; i++), = sprintf("%s",, 
    (i < (argc-1)) protoarg = (protoarg ",, =, ((protolen + arglen) > 77), 
    = ("\n ", +=, =, , protoarg), +=, , \n");, Define inline, 
    __inline int %s(",, (i=0 i<argc; i++), argname[i]), 
    (i < (argc-1)) printf(",, , , (i=0 i<argc; i++), %s \n", argtype[i],, , 
    %s_args a \n",, = VDESC(%s) \n",, (i=0 i<argc; i++), 
    = %s \n", argname[i],, , (VCALL(%s%s, VOFFSET(%s), &a)), arg0special,, 
    {, , {, Special cases: */\n#include, , buf *, , , , , , , | sed -e, , ', 
    End of special cases., , , "$0: Creating $out_c", >, , "$copyright", 
    "$warning", ', , vnodeop_desc vop_default_desc =, , , , , , , , , , , , 
    -e "$sed_prep" $src | $awk, do_offset(typematch), (i=0 i<argc; i++), 
    (argtype[i] == typematch), %s_args,, argname[i]), i, , , 
    "\tVDESC_NO_OFFSET,", -1, , doit(), Define offsets, 
    %s_vp_offsets[] = {\n",, (i=0 i<argc; i++), 
    (argtype[i] == "struct vnode *"), ("\tVOPARG_OFFSETOF(struct, 
    argname[i]), , , "\tVDESC_NO_OFFSET", "}, Define, 
    vnodeop_desc %s_desc = {\n",, offset, ("\t0,\n"), printable, 
    ("\t\"%s\",\n",, flags, , =, (i=0 i<argc; i++), (willrele[i]), 
    (argdir[i] ~ /OUT/), |, else, | VDESC_VP%s_WILLRELE",, , , , , ",", vp, 
    ("\t%s_vp_offsets,\n",, vpp (if, vnode **, cred (if, ucred *, proc (if, 
    proc *, componentname, componentname *, transport layer, ("\tNULL,\n}, 
    {, Special cases:, , , buf *, , , , , , , | sed -e, , ', 
    End of special cases., , ', vnodeop_desc *vfs_op_descs[] =, 
    /* MUST BE FIRST, /* XXX: SPECIAL CASE, /* XXX: SPECIAL CASE, , , 
    -e "$sed_prep" $src | $awk, doit(), name), , , '));
static __inline int VOP_WHITEOUT(dvp, cnp, flags, , , 1996, , -0400, , , , EDT, , , , , 1993, reserved., , without, conditions, met:, copyright, disclaimer., copyright, the, distribution., software, acknowledgement:, of, contributors., contributors, software, permission., , AND, THE, PURPOSE, LIABLE, CONSEQUENTIAL, GOODS, INTERRUPTION), STRICT, WAY, OF, DAMAGE., , , $', , , then, srcfile', , , , , , , , , , , 2>/dev/null`, , then, it., , , toupper(), , {, A-Z", _toupper_str, , , , , , /', , }, , , , , , , , , i=2;, {, 1, , , 0, i++;, {, "$i, , , $i, , , , , , , automatically., lost!), , script:, , , , , , , 1>&2, $out_h, , , , , vop_default_desc, , , "$toupper"', {, descriptor., name), \n");, {, argname[i]);, , , name);, it., toupper(name)), length(protoarg), , {, argtype[i]), "), length(protoarg), {, protoarg), 4, 0, , , arglen, , , function., toupper(name)), {, , "), , , {, argname[i]);, , name);, name);, {, argname[i]);, , \n}\n",, name), , , , <sys/buf.h>\n"), , ", , , , , , , "$anal_retentive", , , */', , , 1>&2, $out_c, , , , , , {, , , , , , , , , , , , ', {, {, {, a_%s),\n",, , , , , , , , {, array, name), {, {, %s_args,a_%s),\n",, , , , , ";, F_desc, name), , , name, name), , , 0, {, {, {, VDESC_VPP_WILLRELE"), {, vpnum), , , , , , offsets, name), any), "), any), "), any), "), , "), information, \n");, , */\n"), , , ", , , , , , , "$anal_retentive", , , */', , , {, */, */, */, , , ', {, , , , NULL)
	struct vnode *dvp;
	struct componentname *cnp;
	int flags;
	 ;
	 ;
	braam+@pachelbel.coda.cs.cmu.edu Wed Oct 9 18:08:18 1996;
	braam+@pachelbel.coda.cs.cmu.edu ;
	from pachelbel.coda.cs.cmu.edu (PACHELBEL.CODA.CS.CMU.EDU [128.2.222.26]) by telemann.coda.cs.cmu.edu (8.7.4/8.7.3) with SMTP id SAA09451 for <braam@telemann.coda.cs.cmu.edu> Wed, 9 Oct 1996 18:08:17 -0400;
	braam+@pachelbel.coda.cs.cmu.edu ;
	<199610092208.SAA09451@telemann.coda.cs.cmu.edu> ;
	braam@pachelbel.coda.cs.cmu.edu ;
	Wed, 9 Oct 96 18:07:56 EDT;
	braam+@pachelbel.coda.cs.cmu.edu ;
	 ;
	 ;
	 ;
	Copyright (c) 1992, 1993;
	The Regents of the University of California. All rights reserved.;
	 ;
	Redistribution and use in source and binary forms, with or without;
	modification, are permitted provided that the following conditions;
	are met:;
	1. Redistributions of source code must retain the above copyright;
	notice, this list of conditions and the following disclaimer.;
	2. Redistributions in binary form must reproduce the above copyright;
	notice, this list of conditions and the following disclaimer in the;
	documentation and/or other materials provided with the distribution.;
	3. All advertising materials mentioning features or use of this software;
	must display the following acknowledgement:;
	This product includes software developed by the University of;
	California, Berkeley and its contributors.;
	4. Neither the name of the University nor the names of its contributors;
	may be used to endorse or promote products derived from this software;
	without specific prior written permission.;
	 ;
	THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND;
	ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE;
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE;
	ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE;
	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL;
	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS;
	OR SERVICES LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION);
	HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT;
	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY;
	OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF;
	SUCH DAMAGE.;
	 ;
	 ;
	vnode_if.sh,v 1.8 1995/03/10 04:13:52 chopps Exp $';
	 ;
	 ;
	[ $# -ne 1 ] then;
	'usage: vnode_if.sh srcfile';
	1 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	'BEGIN { print toupper("true") exit; }' 2>/dev/null`;
	 ;
	[ "$isgawk" = TRUE ] then;
	GNU awk provides it.;
	 ;
	 ;
	Provide our own toupper();
	 ;
	toupper(str) {;
	= "echo "str" |tr a-z A-Z";
	| getline _toupper_str;
	 ;
	_toupper_str ;
	 ;
	 ;
	\1:g ;
	/ /';
	 ;
	{ next };
	{ ;
	 ;
	 ;
	 ;
	{ ;
	 ;
	 ;
	 ;
	= $1 i=2;;
	($2 == "WILLRELE") {;
	= 1;
	 ;
	else ;
	= 0;
	= $i i++;;
	(i < NF) {;
	= argtype[argc]" "$i;
	 ;
	 ;
	= $i;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	Warning: This file is generated automatically.;
	(Modifications made here may easily be lost!);
	 ;
	Created by the script:;
	${SCRIPT_ID} ;
	 ;
	 ;
	 ;
	:\1:g' ;
	 ;
	"$0: Creating $out_h" 1>&2;
	> $out_h;
	 ;
	"$copyright" ;
	"$warning" ;
	' ;
	struct vnodeop_desc vop_default_desc;
	 ;
	 ;
	-e "$sed_prep" $src | $awk "$toupper"';
	doit() {;
	Declare arg struct, descriptor.;
	%s_args {\n", name);
	vnodeop_desc *a_desc \n");;
	(i=0 i<argc; i++) {;
	a_%s \n", argtype[i], argname[i]);;
	 ;
	\n"); ;
	struct vnodeop_desc %s_desc \n", name);;
	Prototype it.;
	= sprintf("static __inline int %s __P((", toupper(name));
	= length(protoarg);
	protoarg) ;
	(i=0 i<argc; i++) {;
	= sprintf("%s", argtype[i]);
	(i < (argc-1)) protoarg = (protoarg ", ");
	= length(protoarg);
	((protolen + arglen) > 77) {;
	= ("\n " protoarg);
	+= 4;
	= 0;
	 ;
	protoarg) ;
	+= arglen;
	 ;
	\n"); ;
	Define inline function.;
	__inline int %s(", toupper(name));
	(i=0 i<argc; i++) {;
	argname[i]) ;
	(i < (argc-1)) printf(", ");
	 ;
	 ;
	(i=0 i<argc; i++) {;
	%s \n", argtype[i], argname[i]);;
	 ;
	%s_args a \n", name);;
	= VDESC(%s) \n", name);;
	(i=0 i<argc; i++) {;
	= %s \n", argname[i], argname[i]);;
	 ;
	(VCALL(%s%s, VOFFSET(%s), &a)) \n}\n",;
	arg0special, name);
	{ ;
	 ;
	{ ;
	Special cases: */\n#include <sys/buf.h>\n");
	 ;
	buf *";
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	| sed -e "$anal_retentive";
	 ;
	' ;
	End of special cases. */';
	 ;
	 ;
	"$0: Creating $out_c" 1>&2;
	> $out_c;
	 ;
	"$copyright" ;
	"$warning" ;
	' ;
	 ;
	vnodeop_desc vop_default_desc = {;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	-e "$sed_prep" $src | $awk ';
	do_offset(typematch) {;
	(i=0 i<argc; i++) {;
	(argtype[i] == typematch) {;
	%s_args, a_%s),\n",;
	argname[i]) ;
	i ;
	 ;
	 ;
	"\tVDESC_NO_OFFSET," ;
	-1 ;
	 ;
	doit() {;
	Define offsets array;
	%s_vp_offsets[] = {\n", name);
	(i=0 i<argc; i++) {;
	(argtype[i] == "struct vnode *") {;
	("\tVOPARG_OFFSETOF(struct %s_args,a_%s),\n",;
	argname[i]) ;
	 ;
	 ;
	"\tVDESC_NO_OFFSET" ;
	"} ";;
	Define F_desc;
	vnodeop_desc %s_desc = {\n", name);
	offset ;
	("\t0,\n") ;
	printable name;
	("\t\"%s\",\n", name);
	flags ;
	 ;
	= 0;
	(i=0 i<argc; i++) {;
	(willrele[i]) {;
	(argdir[i] ~ /OUT/) {;
	| VDESC_VPP_WILLRELE");
	else {;
	| VDESC_VP%s_WILLRELE", vpnum);
	 ;
	 ;
	 ;
	 ;
	"," ;
	vp offsets;
	("\t%s_vp_offsets,\n", name);
	vpp (if any);
	vnode **");
	cred (if any);
	ucred *");
	proc (if any);
	proc *");
	componentname ;
	componentname *");
	transport layer information;
	("\tNULL,\n} \n");;
	{ ;
	Special cases: */\n");
	 ;
	 ;
	buf *";
	 ;
	 ;
	 ;
	 ;
	 ;
	 ;
	| sed -e "$anal_retentive";
	 ;
	' ;
	End of special cases. */';
	 ;
	' ;
	vnodeop_desc *vfs_op_descs[] = {;
	/* MUST BE FIRST */;
	/* XXX: SPECIAL CASE */;
	/* XXX: SPECIAL CASE */;
	 ;
	 ;
	-e "$sed_prep" $src | $awk ';
	doit() {;
	name) ;
	 ;
	 ;
	' NULL;
{
	struct vop_whiteout_args a;
	a.a_desc = VDESC(vop_whiteout);
	a.a_dvp = dvp;
	a.a_cnp = cnp;
	a.a_flags = flags;
	a.a_ = ;
	a.a_ = ;
	a.a_1996 = 1996;
	a.a_ = ;
	a.a_-0400 = -0400;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_EDT = EDT;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_1993 = 1993;
	a.a_reserved. = reserved.;
	a.a_ = ;
	a.a_without = without;
	a.a_conditions = conditions;
	a.a_met: = met:;
	a.a_copyright = copyright;
	a.a_disclaimer. = disclaimer.;
	a.a_copyright = copyright;
	a.a_the = the;
	a.a_distribution. = distribution.;
	a.a_software = software;
	a.a_acknowledgement: = acknowledgement:;
	a.a_of = of;
	a.a_contributors. = contributors.;
	a.a_contributors = contributors;
	a.a_software = software;
	a.a_permission. = permission.;
	a.a_ = ;
	a.a_AND = AND;
	a.a_THE = THE;
	a.a_PURPOSE = PURPOSE;
	a.a_LIABLE = LIABLE;
	a.a_CONSEQUENTIAL = CONSEQUENTIAL;
	a.a_GOODS = GOODS;
	a.a_INTERRUPTION) = INTERRUPTION);
	a.a_STRICT = STRICT;
	a.a_WAY = WAY;
	a.a_OF = OF;
	a.a_DAMAGE. = DAMAGE.;
	a.a_ = ;
	a.a_ = ;
	a.a_$' = $';
	a.a_ = ;
	a.a_ = ;
	a.a_then = then;
	a.a_srcfile' = srcfile';
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_2>/dev/null` = 2>/dev/null`;
	a.a_ = ;
	a.a_then = then;
	a.a_it. = it.;
	a.a_ = ;
	a.a_ = ;
	a.a_toupper() = toupper();
	a.a_ = ;
	a.a_{ = {;
	a.a_A-Z" = A-Z";
	a.a__toupper_str = _toupper_str;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_/' = /';
	a.a_ = ;
	a.a_} = };
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_i=2; = i=2;;
	a.a_{ = {;
	a.a_1 = 1;
	a.a_ = ;
	a.a_ = ;
	a.a_0 = 0;
	a.a_i++; = i++;;
	a.a_{ = {;
	a.a_"$i = "$i;
	a.a_ = ;
	a.a_ = ;
	a.a_$i = $i;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_automatically. = automatically.;
	a.a_lost!) = lost!);
	a.a_ = ;
	a.a_script: = script:;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_1>&2 = 1>&2;
	a.a_$out_h = $out_h;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_vop_default_desc = vop_default_desc;
	a.a_ = ;
	a.a_ = ;
	a.a_"$toupper"' = "$toupper"';
	a.a_{ = {;
	a.a_descriptor. = descriptor.;
	a.a_name) = name);
	a.a_\n"); = \n");;
	a.a_{ = {;
	a.a_argname[i]); = argname[i]);;
	a.a_ = ;
	a.a_ = ;
	a.a_name); = name);;
	a.a_it. = it.;
	a.a_toupper(name)) = toupper(name));
	a.a_length(protoarg) = length(protoarg);
	a.a_ = ;
	a.a_{ = {;
	a.a_argtype[i]) = argtype[i]);
	a.a_") = ");
	a.a_length(protoarg) = length(protoarg);
	a.a_{ = {;
	a.a_protoarg) = protoarg);
	a.a_4 = 4;
	a.a_0 = 0;
	a.a_ = ;
	a.a_ = ;
	a.a_arglen = arglen;
	a.a_ = ;
	a.a_ = ;
	a.a_function. = function.;
	a.a_toupper(name)) = toupper(name));
	a.a_{ = {;
	a.a_ = ;
	a.a_") = ");
	a.a_ = ;
	a.a_ = ;
	a.a_{ = {;
	a.a_argname[i]); = argname[i]);;
	a.a_ = ;
	a.a_name); = name);;
	a.a_name); = name);;
	a.a_{ = {;
	a.a_argname[i]); = argname[i]);;
	a.a_ = ;
	a.a_\n}\n", = \n}\n",;
	a.a_name) = name);
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_<sys/buf.h>\n") = <sys/buf.h>\n");
	a.a_ = ;
	a.a_" = ";
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_"$anal_retentive" = "$anal_retentive";
	a.a_ = ;
	a.a_ = ;
	a.a_*/' = */';
	a.a_ = ;
	a.a_ = ;
	a.a_1>&2 = 1>&2;
	a.a_$out_c = $out_c;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_{ = {;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_' = ';
	a.a_{ = {;
	a.a_{ = {;
	a.a_{ = {;
	a.a_a_%s),\n", = a_%s),\n",;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_{ = {;
	a.a_array = array;
	a.a_name) = name);
	a.a_{ = {;
	a.a_{ = {;
	a.a_%s_args,a_%s),\n", = %s_args,a_%s),\n",;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_"; = ";;
	a.a_F_desc = F_desc;
	a.a_name) = name);
	a.a_ = ;
	a.a_ = ;
	a.a_name = name;
	a.a_name) = name);
	a.a_ = ;
	a.a_ = ;
	a.a_0 = 0;
	a.a_{ = {;
	a.a_{ = {;
	a.a_{ = {;
	a.a_VDESC_VPP_WILLRELE") = VDESC_VPP_WILLRELE");
	a.a_{ = {;
	a.a_vpnum) = vpnum);
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_offsets = offsets;
	a.a_name) = name);
	a.a_any) = any);
	a.a_") = ");
	a.a_any) = any);
	a.a_") = ");
	a.a_any) = any);
	a.a_") = ");
	a.a_ = ;
	a.a_") = ");
	a.a_information = information;
	a.a_\n"); = \n");;
	a.a_ = ;
	a.a_*/\n") = */\n");
	a.a_ = ;
	a.a_ = ;
	a.a_" = ";
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_"$anal_retentive" = "$anal_retentive";
	a.a_ = ;
	a.a_ = ;
	a.a_*/' = */';
	a.a_ = ;
	a.a_ = ;
	a.a_{ = {;
	a.a_*/ = */;
	a.a_*/ = */;
	a.a_*/ = */;
	a.a_ = ;
	a.a_ = ;
	a.a_' = ';
	a.a_{ = {;
	a.a_ = ;
	a.a_ = ;
	a.a_ = ;
	a.a_NULL = NULL;
	return (VCALL(dvp, VOFFSET(vop_whiteout), &a));
}

/* Special cases: */
#include <sys/buf.h>

struct vop_strategy_args {
	struct vnodeop_desc *a_desc;
	struct buf *a_bp;
};
extern struct vnodeop_desc vop_strategy_desc;
static __inline int VOP_STRATEGY __P((struct buf *));
static __inline int VOP_STRATEGY(bp)
	struct buf *bp;
{
	struct vop_strategy_args a;
	a.a_desc = VDESC(vop_strategy);
	a.a_bp = bp;
	return (VCALL(bp->b_vp, VOFFSET(vop_strategy), &a));
}

struct vop_bwrite_args {
	struct vnodeop_desc *a_desc;
	struct buf *a_bp;
};
extern struct vnodeop_desc vop_bwrite_desc;
static __inline int VOP_BWRITE __P((struct buf *));
static __inline int VOP_BWRITE(bp)
	struct buf *bp;
{
	struct vop_bwrite_args a;
	a.a_desc = VDESC(vop_bwrite);
	a.a_bp = bp;
	return (VCALL(bp->b_vp, VOFFSET(vop_bwrite), &a));
}

/* End of special cases. */
