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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/venus/vproc_vfscalls.cc,v 4.8 1997/09/26 16:43:09 rvb Exp $";
#endif /*_BLURB_*/




/*
 *
 *    Implementation of the Venus VFS interface.
 *
 *    This set of calls supports the VFS/Vnode file system interface created by Sun Microsystems.
 *    The call interface should be identical to the latest release (actually, the release implemented
 *    by the kernel).  The implementation of each call is based on the following template:
 *
 *    vproc::template(...) {
 *        \* Log message *\
 *
 *        \* Argument validation *\
 *
 *        for (;;) {
 *            Begin_VFS(VolumeId, vfsop);
 *            if (u.u_error) break;
 *
 *            \* Get objects *\
 *
 *            \* Semantic, protection checks *\
 *
 *            \* Invoke CFS operation *\
 *
 *    FreeLocks:
 *            \* Put objects *\
 *            End_VFS(&retry_call);
 *            if (!retry_call) break;
 *        }
 *
 *        \* Handle EINCONS result *\
 *    }
 *
 *
 *    ToDo:
 *        1. Decide whether or not data is needed in the following cases (under all COP modes):
 *          - object have its attributes set
 *          - target object of a hard link
 *          - target of a remove
 *          - source or target of a rename
 *
 */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <sys/file.h>
#ifndef __FreeBSD__
// Since vproc.h knows struct uio.
#include <sys/uio.h>
#endif  /* __FreeBSD__ */
#ifdef __cplusplus
}
#endif __cplusplus

/* interface */
#include <vice.h>

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif

#ifdef __cplusplus
}
#endif __cplusplus

/* from libal */
#include <prs_fs.h>

/* from venus */
#include "fso.h"
#include "hdb.h"
#include "local.h"
#include "venus.private.h"
#include "vproc.h"
#include "worker.h"


/* Temporary!  Move to cnode.h. -JJK */
#define	C_INCON	0x2

/* From <vfs/vnode.h>.  Not otherwise defined since conditionalized by KERNEL. -JJK */
#ifdef __FreeBSD__
#include <sys/vnode.h>
#else 
#define IO_UNIT		0x01		/* do io as atomic unit for VOP_RDWR */
#define IO_APPEND	0x02		/* append write for VOP_RDWR */
#define IO_SYNC		0x04		/* sync io for VOP_RDWR */
#ifndef	__linux__
#define IO_NDELAY	0x08		/* non-blocking i/o for fifos */
#endif 
#endif

/* ***** VFS Operations  ***** */

/* These operations originally took a struct vfs * as first argument.
   But that argument was NEVER used anywhere.  My best guess is that it was
   present to match calls of similar name inside the kernel.  Since this is a
   pain to port to BSD44 and no purpose is served by it, I've deleted all
   references to struct vfs from Venus.   (Satya, 8/15/96)
*/
void vproc::mount(char *path, void *data) {
    LOG(1, ("vproc::mount: path = %s\n", path));
    u.u_error = EOPNOTSUPP;
}


void vproc::unmount() {
    LOG(1, ("vproc::unmount\n"));
    u.u_error = EOPNOTSUPP;
}


void vproc::root(struct vnode **vpp) {
    LOG(1, ("vproc::root\n"));

    /* Set OUT parameter. */
    MAKE_VNODE(*vpp, rootfid, VDIR);
}


void vproc::statfs(struct statfs *sbp) {
    LOG(1, ("vproc::statfs\n"));
    u.u_error = EOPNOTSUPP;
}


void vproc::sync() {
    LOG(1, ("vproc::sync\n"));
    u.u_error = EOPNOTSUPP;
}


void vproc::vget(struct vnode **vpp, struct fid *fidp) {
    struct cfid *cfidp = (struct cfid *)fidp;

    LOG(1, ("vproc::vget: fid = (%x.%x.%x), nc = %x\n",
	     (cfidp->cfid_fid.Volume, cfidp->cfid_fid.Vnode,
	     cfidp->cfid_fid.Unique, u.u_nc)));

    int code = 0;
    fsobj *f = 0;
    struct cnode *cp = 0;
    *vpp = 0;
    if (u.u_nc && LogLevel >= 100)
	u.u_nc->print(logFile);

    for (;;) {
	Begin_VFS(cfidp->cfid_fid.Volume, (int)VFSOP_VGET);
	if (u.u_error) break;

	u.u_error = FSDB->Get(&f, &cfidp->cfid_fid, CRTORUID(u.u_cred), RC_STATUS);
	if (u.u_error) {
	    if (u.u_error == EINCONS) {
		u.u_error = 0;

		/* Set OUT parameter according to "fake" vnode. */
		MAKE_VNODE(*vpp, cfidp->cfid_fid, VLNK);
		VTOC(*vpp)->c_flags |= C_INCON;
	    }

	    goto FreeLocks;
	}

	/* Set OUT parameter. */
	MAKE_VNODE(*vpp, f->fid, FTTOVT(f->stat.VnodeType));
	if (f->IsFake() || f->IsMTLink())
	    VTOC(*vpp)->c_flags |= C_INCON;

FreeLocks:
	/* Update namectxt if applicable. */
	if (u.u_error == 0 && u.u_nc)
	    u.u_nc->CheckComponent(f);
	FSDB->Put(&f);
	int retry_call = 0;
	End_VFS(&retry_call);
	if (!retry_call) break;
    }
}


/* ***** Vnode Operations  ***** */

void vproc::open(struct vnode **vpp, int flags) {
    struct cnode *cp = VTOC(*vpp);

    LOG(1, ("vproc::open: fid = (%x.%x.%x), flags = %x\n",
	     cp->c_fid.Volume, cp->c_fid.Vnode, cp->c_fid.Unique, flags));

    /* Expand the flags argument into some useful predicates. */

    int readp = (flags & FREAD) != 0;
    int writep = (flags & FWRITE) != 0;
    int truncp = (flags & O_TRUNC) != 0;
    int exclp = (flags & O_EXCL) != 0;
    int	execp =	0;	    /* With VFS we're no longer told of execs! -JJK */

    fsobj *f = 0;

    for (;;) {
	Begin_VFS(cp->c_fid.Volume, (int)VFSOP_OPEN,
		  writep ? VM_MUTATING : VM_OBSERVING);
	if (u.u_error) break;

	/* Get the object. */
	u.u_error = FSDB->Get(&f, &cp->c_fid, CRTORUID(u.u_cred), RC_DATA);
	if (u.u_error) goto FreeLocks;

	/* Exclusive mode open fails. */
#ifndef __FreeBSD__
	// inamura@isl.ntt.co.jp For FreeBSD. Date: Fri Jul 11 1997
	if (exclp) { u.u_error = EEXIST; goto FreeLocks; }
#endif
	/* Verify that we have the necessary permission. */
	if (readp) {
	    long rights = f->IsFile() ? PRSFS_READ : PRSFS_LOOKUP;
	    int modes = f->IsFile() ? R_OK : 0;
	    u.u_error = f->Access(rights, modes, CRTORUID(u.u_cred));
	    if (u.u_error) goto FreeLocks;
	}
	if (writep || truncp) {
	    if (f->IsDir()) { u.u_error = EISDIR; goto FreeLocks; }
	    if ((*vpp)->v_flag & VTEXT)
		{ u.u_error = ETXTBSY; goto FreeLocks; }

	    /* Truncating requires write permission. */
	    /* Otherwise, either write or insert suffices (to support insert only directories). */
	    long rights = (truncp
			   ? (long)PRSFS_WRITE
			   : (long)(PRSFS_WRITE | PRSFS_INSERT));
	    u.u_error = f->Access(rights, W_OK, CRTORUID(u.u_cred));
	    if (u.u_error) goto FreeLocks;
	}

	/* Do the operation. */
	u.u_error = f->Open(writep, execp, truncp,
			    &cp->c_device, &cp->c_inode, CRTORUID(u.u_cred));
	if (u.u_error) goto FreeLocks;

FreeLocks:
	FSDB->Put(&f);
	int retry_call = 0;
	End_VFS(&retry_call);
	if (!retry_call) break;
    }

    if (u.u_error == EINCONS) {
	u.u_error = ENOENT;
	k_Purge(&cp->c_fid, 1);
    }
}


void vproc::close(struct vnode *vp, int flags) {
    struct cnode *cp = VTOC(vp);

    LOG(1, ("vproc::close: fid = (%x.%x.%x), flags = %x\n",
	     cp->c_fid.Volume, cp->c_fid.Vnode, cp->c_fid.Unique, flags));

    /* Expand the flags argument into some useful predicates. */
    int writep = (flags & (FWRITE | O_TRUNC)) != 0;
    int	execp =	0;	    /* ??? -JJK */

    fsobj *f = 0;

    for (;;) {
	Begin_VFS(cp->c_fid.Volume, (int) VFSOP_CLOSE,
		  writep ? VM_MUTATING : VM_OBSERVING);
	if (u.u_error) break;

	/* Get the object. */
        /* We used to fetch the DATA too.  However, this creates problems
         * if you are in the DYING state and you have an active reference to
         * the file (since you cannot fetch and you cannot garbage collect).
         * We're reasonably confident that closing an object without having
         * the DATA causes no problems; however, we'll leave a zero-level 
         * log statement in as evidence to the contrary... (mre:6/14/94) 
         */
        u.u_error = FSDB->Get(&f, &cp->c_fid, CRTORUID(u.u_cred), RC_STATUS);
        if (u.u_error) goto FreeLocks;

        if (!DYING(f) && !HAVEDATA(f)) 
          LOG(0, ("vproc::close: Don't have DATA and not DYING! (fid = <%x.%x.%x>, flags = %x)\n", cp->c_fid.Volume, cp->c_fid.Vnode, cp->c_fid.Unique, flags));

	/* Do the operation. */
	u.u_error = f->Close(writep, execp, CRTORUID(u.u_cred));

FreeLocks:
	FSDB->Put(&f);
	int retry_call = 0;
	End_VFS(&retry_call);
	if (!retry_call) break;
    }

    if (u.u_error == EINCONS) {
	u.u_error = EINVAL;	    /* XXX -JJK */
	k_Purge(&cp->c_fid, 1);
    }
}


void vproc::rdwr(struct vnode *vp, struct uio *uiop,
		  enum uio_rw rwflag, int ioflag) {
    struct cnode *cp = VTOC(vp);

    LOG(1, ("vproc::rdwr: fid = (%x.%x.%x), rwflag = %d, ioflag = %d\n",
	     cp->c_fid.Volume, cp->c_fid.Vnode, cp->c_fid.Unique, rwflag, ioflag));

    int offset = (int) uiop->uio_offset;
    char *buf = uiop->uio_iov->iov_base;
    int len = uiop->uio_iov->iov_len;
    int cc = 0;

    if (len > V_BLKSIZE)
	{ u.u_error = EINVAL; return; }

    fsobj *f = 0;

    /* Sanity checks. */
    if (rwflag != UIO_READ && rwflag != UIO_WRITE)
	Choke("vproc::rdwr: rwflag bogus (%d)", rwflag);
    if (rwflag == UIO_READ && uiop->uio_resid == 0)
	return;
    if (((int)uiop->uio_offset < 0 ||
	  (int)(uiop->uio_offset + uiop->uio_resid) < 0))
	{ u.u_error = EINVAL; return; }
    if (uiop->uio_resid == 0)
	return;

    for (;;) {
	Begin_VFS(cp->c_fid.Volume, (int)VFSOP_RDWR,
		  rwflag == UIO_WRITE ? VM_MUTATING : VM_OBSERVING);
	if (u.u_error) break;

	/* Get the object. */
	u.u_error = FSDB->Get(&f, &cp->c_fid, CRTORUID(u.u_cred), RC_DATA);
	if (u.u_error) goto FreeLocks;

	/* Verify that it is a file. */
	if (!f->IsFile())
	    { u.u_error = EISDIR; goto FreeLocks; }

	/* Adjust offset in the case of append. */
	if ((ioflag & IO_APPEND) && (rwflag == UIO_WRITE)) {
	    struct vattr va;
	    va_init(&va);
	    f->GetVattr(&va);
	    offset = (int) va.va_size;
	}

	/* Do the operation. */
	u.u_error = f->RdWr(buf, rwflag, offset, len, &cc, CRTORUID(u.u_cred));
	if (u.u_error) goto FreeLocks;
	uiop->uio_resid -= cc;

FreeLocks:
	FSDB->Put(&f);
	int retry_call = 0;
	End_VFS(&retry_call);
	if (!retry_call) break;
    }

    if (u.u_error == EINCONS) {
	u.u_error = EBADF;	    /* XXX -JJK */
	k_Purge(&cp->c_fid, 1);
    }
}


void vproc::ioctl(struct vnode *vp, unsigned int com,
		   struct ViceIoctl *data, int flags) {
    struct cnode *cp = VTOC(vp);

    LOG(1, ("vproc::ioctl(%d): fid = (%x.%x.%x), com = %s\n",
	     u.u_cred.cr_uid,
	     cp->c_fid.Volume, cp->c_fid.Vnode, cp->c_fid.Unique, IoctlOpStr(com)));

    do_ioctl(&cp->c_fid, com, data);

    if (u.u_error == EINCONS) {
	u.u_error = ENOENT;
	k_Purge(&cp->c_fid, 1);
    }
}


void vproc::select(struct vnode *vp, int rwflag) {
    struct cnode *cp = VTOC(vp);

    LOG(1, ("vproc::select: fid = (%x.%x.%x), rwflag = %d\n",
	     cp->c_fid.Volume, cp->c_fid.Vnode, cp->c_fid.Unique, rwflag));

    u.u_error = EOPNOTSUPP;
}


void vproc::getattr(struct vnode *vp, struct vattr *vap) {
    struct cnode *cp = VTOC(vp);

    LOG(1, ("vproc::getattr: fid = (%x.%x.%x)\n",
	     cp->c_fid.Volume, cp->c_fid.Vnode, cp->c_fid.Unique));

    fsobj *f = 0;

    for (;;) {
	Begin_VFS(cp->c_fid.Volume, (int)VFSOP_GETATTR);
	if (u.u_error) break;

	/* Get the object. */
	u.u_error = FSDB->Get(&f, &cp->c_fid, CRTORUID(u.u_cred), RC_STATUS);
	if (u.u_error) goto FreeLocks;

	/* No rights required to get attributes? -JJK */

	/* Do the operation. */
	f->GetVattr(vap);

FreeLocks:
	FSDB->Put(&f);
	int retry_call = 0;
	End_VFS(&retry_call);
	if (!retry_call) break;
    }

    if (u.u_error == EINCONS) {
	u.u_error = 0;
	k_Purge(&cp->c_fid, 1);

	/* Make a "fake" vattr block for the inconsistent object. */
	va_init(vap);
#ifndef __linux__
	vap->va_mode = 0444 | FTTOVT(SymbolicLink);
#else
	vap->va_mode = 0444;
	vap->va_type = FTTOVT(SymbolicLink);
#endif
	vap->va_uid = (short)V_UID;
	vap->va_gid = (short)V_GID;
	vap->va_fsid = 1;
	vap->va_nlink = 1;
	vap->va_size = 27;  /* @XXXXXXXX.YYYYYYYY.ZZZZZZZZ */
	vap->va_blocksize = V_BLKSIZE;
	VA_ID(vap) = FidToNodeid(&cp->c_fid);
	VA_ATIME_1(vap) = Vtime();
	VA_ATIME_2(vap) = 0;
	vap->va_mtime = vap->va_atime;
	vap->va_ctime = vap->va_atime;
	vap->va_rdev = 1;

#ifdef __MACH__
	vap->va_blocks = NBLOCKS(vap->va_size) << 1;    /* 512 byte units! */
#endif /* __MACH__ */
#ifdef __BSD44__
	vap->va_bytes = vap->va_size;
#endif /* __BSD44__ */
    }
}


/* The only attributes that are allowed to be changed are:
      va_mode	    chmod, fchmod
      va_uid	    chown, fchown
      va_size	    truncate, ftruncate
      va_mtime	    utimes
*/
void vproc::setattr(struct vnode *vp, struct vattr *vap) {
    struct cnode *cp = VTOC(vp);

    LOG(1, ("vproc::setattr: fid = (%x.%x.%x)\n",
	     cp->c_fid.Volume, cp->c_fid.Vnode, cp->c_fid.Unique));

    fsobj *f = 0;

    /* 
     * BSD44 supports chflags, which sets the va_flags field of 
     * the vattr.  Coda doesn't support these flags, but we will
     * allow calls that clear the field.  
     * 
     * Note that even though a vattr that includes this field is 
     * defined for mach in venus_vnode.h, the definition it picks 
     * up is the native one from mach_vnode.h.
     */
    /* Cannot set these attributes. */
    if ( (vap->va_fsid != VA_IGNORE_FSID) ||
	 (VA_ID(vap) != VA_IGNORE_ID) ||
	 (vap->va_nlink != VA_IGNORE_NLINK) ||
	 (vap->va_blocksize != VA_IGNORE_BLOCKSIZE) ||
	 (vap->va_rdev != VA_IGNORE_RDEV) ||
#ifdef __BSD44__
	 (vap->va_flags != VA_IGNORE_FLAGS &&
	     vap->va_flags != 0) ||
#endif /* __BSD44__ */
	 (VA_STORAGE(vap) != VA_IGNORE_STORAGE) )
	{ u.u_error = EINVAL; return; }

    /* Should be setting at least one of these. */
    if ( (vap->va_mode == VA_IGNORE_MODE) &&
	 (vap->va_uid == VA_IGNORE_UID) &&
	 (vap->va_gid == VA_IGNORE_GID) &&
	 (vap->va_size == VA_IGNORE_SIZE) &&
#ifdef __BSD44__
	 (vap->va_flags == VA_IGNORE_FLAGS) &&
#endif /* __BSD44__ */
	 (VA_ATIME_1(vap) == VA_IGNORE_TIME1) &&
	 (VA_MTIME_1(vap) == VA_IGNORE_TIME1) &&
	 (VA_CTIME_1(vap) == VA_IGNORE_TIME1) )

	Choke("vproc::setattr: no attributes specified");

    for (;;) {
	Begin_VFS(cp->c_fid.Volume, (int)VFSOP_SETATTR);
	if (u.u_error) break;

	/* Get the object. */
	u.u_error = FSDB->Get(&f, &cp->c_fid, CRTORUID(u.u_cred), RC_STATUS);
	if (u.u_error) goto FreeLocks;

	/* Symbolic links are immutable. */
	if (f->IsSymLink() || f->IsMtPt())
	    { u.u_error = EINVAL; goto FreeLocks; }

	/* Permission checks. */
	{
	    /* chmod, fchmod */
	    if (vap->va_mode != VA_IGNORE_MODE) {
		u.u_error = f->Access((long)PRSFS_WRITE, 0, CRTORUID(u.u_cred));
		if (u.u_error) goto FreeLocks;
	    }

	    /* chown, fchown */
	    if (vap->va_uid != VA_IGNORE_UID) {
		/* Need to allow for System:Administrators here! -JJK */
		if (f->stat.Owner != (vuid_t)vap->va_uid)
		    { u.u_error = EACCES; goto FreeLocks; }

		u.u_error = f->Access((long)PRSFS_ADMINISTER, 0, CRTORUID(u.u_cred));
		if (u.u_error) goto FreeLocks;
	    }
	    /* gid should be V_GID for chown requests, VA_IGNORE_GID otherwise */
#if	0
	    /* whose idea was this anyways? */
	    if (vap->va_gid != VA_IGNORE_GID &&	vap->va_gid != V_GID) {
		u.u_error = EACCES;
		goto FreeLocks;
	    }
#else
	    if (vap->va_gid != VA_IGNORE_GID)
	        vap->va_gid = V_GID;
#endif
	    /* truncate, ftruncate */
	    if (vap->va_size != VA_IGNORE_SIZE) {
		if (!f->IsFile())
		    { u.u_error = EISDIR; goto FreeLocks; }

		u.u_error = f->Access((long)PRSFS_WRITE, W_OK, CRTORUID(u.u_cred));
		if (u.u_error) goto FreeLocks;
	    }

	    /* utimes */
	    if (VA_ATIME_1(vap) != VA_IGNORE_TIME1 ||
		VA_MTIME_1(vap) != VA_IGNORE_TIME1 ||
		VA_CTIME_1(vap) != VA_IGNORE_TIME1) {
		u.u_error = f->Access((long)PRSFS_WRITE, 0, CRTORUID(u.u_cred));
		if (u.u_error) goto FreeLocks;
	    }
	}

	/* Do the operation. */
	f->PromoteLock();
	u.u_error = f->SetAttr(vap, CRTORUID(u.u_cred));

FreeLocks:
	FSDB->Put(&f);
	int retry_call = 0;
	End_VFS(&retry_call);
	if (!retry_call) break;
    }

    if (u.u_error == EINCONS) {
	u.u_error = ENOENT;
	k_Purge(&cp->c_fid, 1);
    }
}


void vproc::access(struct vnode *vp, int mode) {
    struct cnode *cp = VTOC(vp);

    LOG(1, ("vproc::access: fid = (%x.%x.%x), mode = %#o\n",
	     cp->c_fid.Volume, cp->c_fid.Vnode, cp->c_fid.Unique, mode));

    /* Translation of Unix mode bits to AFS protection classes. */
    static long DirAccessMap[8] = {
	0,
	PRSFS_LOOKUP,
	PRSFS_INSERT | PRSFS_DELETE,
	PRSFS_LOOKUP | PRSFS_INSERT | PRSFS_DELETE,
	PRSFS_LOOKUP,
	PRSFS_LOOKUP,
	PRSFS_LOOKUP | PRSFS_INSERT | PRSFS_DELETE,
	PRSFS_LOOKUP | PRSFS_INSERT | PRSFS_DELETE
    };
    static long FileAccessMap[8] = {
	0,
	PRSFS_READ,
	PRSFS_WRITE,
	PRSFS_READ | PRSFS_WRITE,
	PRSFS_READ,
	PRSFS_READ,
	PRSFS_READ | PRSFS_WRITE,
	PRSFS_READ | PRSFS_WRITE
    };
    long rights = 0;
    int modes = 0;

    fsobj *f = 0;

    for (;;) {
	Begin_VFS(cp->c_fid.Volume, (int)VFSOP_ACCESS);
	if (u.u_error) break;

	/* Get the object. */
	u.u_error = FSDB->Get(&f, &cp->c_fid, CRTORUID(u.u_cred), RC_STATUS);
	if (u.u_error) goto FreeLocks;

	modes = ((mode & OWNERBITS) >> 6);
	rights = f->IsDir()
	  ? DirAccessMap[modes]
	  : FileAccessMap[modes];
	u.u_error = f->Access(rights, modes, CRTORUID(u.u_cred));
	if (u.u_error) goto FreeLocks;

FreeLocks:
	FSDB->Put(&f);
	int retry_call = 0;
	End_VFS(&retry_call);
	if (!retry_call) break;
    }

    if (u.u_error == EINCONS) {
	u.u_error = ENOENT;
	k_Purge(&cp->c_fid, 1);
    }
}


void vproc::lookup(struct vnode *dvp, char *name, struct vnode **vpp) {
    struct cnode *dcp = VTOC(dvp);

    LOG(1, ("vproc::lookup: fid = (%x.%x.%x), name = %s, nc = %x\n",
	     dcp->c_fid.Volume, dcp->c_fid.Vnode, dcp->c_fid.Unique,
	     name, u.u_nc));
    if (u.u_nc && LogLevel >= 100)
	u.u_nc->print(logFile);

    fsobj *parent_fso = 0;
    fsobj *target_fso = 0;
    struct cnode *cp = 0;
    *vpp = 0;

    for (;;) {
	Begin_VFS(dcp->c_fid.Volume, (int)VFSOP_LOOKUP);
	if (u.u_error) break;

	/* Get the parent object and verify that it is a directory. */
	u.u_error = FSDB->Get(&parent_fso, &dcp->c_fid, CRTORUID(u.u_cred), RC_DATA);
	if (u.u_error) goto FreeLocks;
	if (!parent_fso->IsDir())
	    { u.u_error = ENOTDIR; goto FreeLocks; }

	/* Get the target object. */
	if (STREQ(name, ".")) {
	    /* Don't get the same object twice! */
	    target_fso = parent_fso;
	    parent_fso = 0;		    /* Fake a FSDB->Put(&parent_fso); */
	}
	else {
	    ViceFid inc_fid;
	    u.u_error = parent_fso->Lookup(&target_fso, &inc_fid, name, CRTORUID(u.u_cred));
	    if (u.u_error) {
		if (u.u_error == EINCONS) {
		    u.u_error = 0;

		    /* Set OUT parameter according to "fake" vnode. */
		    MAKE_VNODE(*vpp, inc_fid, VLNK);
		    VTOC(*vpp)->c_flags |= C_INCON;
		}

		goto FreeLocks;
	    }
	}

	/* Set OUT parameter. */
	MAKE_VNODE(*vpp, target_fso->fid, FTTOVT(target_fso->stat.VnodeType));
	if (target_fso->IsFake() || target_fso->IsMTLink())
	    VTOC(*vpp)->c_flags |= C_INCON;

FreeLocks:
	/* Update namectxt if applicable. */
	if (u.u_error == 0 && u.u_nc)
	    u.u_nc->CheckComponent(target_fso);
	FSDB->Put(&parent_fso);
	FSDB->Put(&target_fso);
	int retry_call = 0;
	End_VFS(&retry_call);
	if (!retry_call) break;
    }

    if (u.u_error == EINCONS) {
	u.u_error = ENOENT;
	k_Purge(&dcp->c_fid, 1);
    }
}


void vproc::create(struct vnode *dvp, char *name, struct vattr *vap,
		   int excl, int mode, struct vnode **vpp) {
    struct cnode *dcp = VTOC(dvp);

    LOG(1, ("vproc::create: fid = (%x.%x.%x), name = %s, excl = %d, mode = %d\n",
	     dcp->c_fid.Volume, dcp->c_fid.Vnode, dcp->c_fid.Unique,
	     name, excl, mode));

    fsobj *parent_fso = 0;
    fsobj *target_fso = 0;
    struct cnode *cp = 0;
    *vpp = 0;

    /* Expand the flags into some useful predicates. */
    int readp = (mode & VREAD) != 0;
    int writep = (mode & VWRITE) != 0;
    int truncp = (vap->va_size == 0);
    int exclp = excl;

    /* Disallow creation of {'.','..', '/'}. */
    if (STREQ(name, "..") || STREQ(name, ".") || STREQ(name, ""))
	{ u.u_error = EINVAL; return; }

    /* Disallow names of the form "@XXXXXXXX.YYYYYYYY.ZZZZZZZZ". */
    if (strlen(name) == 27 && name[0] == '@' && name[9] == '.' && name[18] == '.')
	{ u.u_error = EINVAL; return; }

    for (;;) {
	Begin_VFS(dcp->c_fid.Volume, (int)VFSOP_CREATE);
	if (u.u_error) break;

	/* Get the parent object and verify that it is a directory. */
	u.u_error = FSDB->Get(&parent_fso, &dcp->c_fid, CRTORUID(u.u_cred), RC_DATA);
	if (u.u_error) goto FreeLocks;
	if (!parent_fso->IsDir())
	    { u.u_error = ENOTDIR; goto FreeLocks; }

	/* Get the target object (if it exists). */
	if ((LRDB->repair_root_fid) && (parent_fso->IsLocalObj() || 
					LRDB->RFM_IsRootParent(&parent_fso->fid))) {
	    /* cross mount-point when under local/global repair */
	    ViceFid dummy;
	    u.u_error = parent_fso->Lookup(&target_fso, &dummy, name, CRTORUID(u.u_cred));
	} else {
	    u.u_error = parent_fso->Lookup(&target_fso, 0, name, CRTORUID(u.u_cred));
	}
	if (u.u_error == 0) {
	    FSDB->Put(&parent_fso);	    /* avoid deadlock! */

	    /* Found target.  Error if EXCL requested. */
	    if (exclp) { u.u_error = EEXIST; goto FreeLocks; }

	    /* Verify that it is a file. */
	    if (!target_fso->IsFile()) { u.u_error = EISDIR; goto FreeLocks; }

	    /* Verify that we have the necessary permissions. */
	    if (readp) {
		u.u_error = target_fso->Access((long)PRSFS_READ, R_OK, CRTORUID(u.u_cred));
		if (u.u_error) goto FreeLocks;
	    }
	    if (writep || truncp) {
		u.u_error = target_fso->Access((long)PRSFS_WRITE, W_OK, CRTORUID(u.u_cred));
		if (u.u_error) goto FreeLocks;
	    }

	    /* We need the data now. XXX -JJK */
	    ViceFid target_fid = target_fso->fid;
	    FSDB->Put(&target_fso);
	    u.u_error = FSDB->Get(&target_fso, &target_fid, CRTORUID(u.u_cred), RC_DATA);
	    if (u.u_error) goto FreeLocks;

	    /* Do truncate if necessary. */
	    if (truncp) {
		/* Be careful to truncate only (i.e., don't set any other attrs)! */
		va_init(vap);
		vap->va_size = 0;

		target_fso->PromoteLock();
		u.u_error = target_fso->SetAttr(vap, CRTORUID(u.u_cred));
		if (u.u_error) goto FreeLocks;
		target_fso->DemoteLock();
	    }
	}
	else {
	    if (u.u_error != ENOENT) goto FreeLocks;
	    u.u_error = 0;

	    /* Verify that we have create permission. */
	    u.u_error = parent_fso->Access((long)PRSFS_INSERT, 0, CRTORUID(u.u_cred));
	    if (u.u_error) goto FreeLocks;

	    /* Do the create. */
	    parent_fso->PromoteLock();
	    u.u_error = parent_fso->Create(name, &target_fso, CRTORUID(u.u_cred),
					   vap->va_mode & 0777, FSDB->StdPri());
	    /* Probably ought to do something here if EEXIST! -JJK */
	    if (u.u_error) goto FreeLocks;
	}

	/* Set OUT parameters. */
	target_fso->GetVattr(vap);
	MAKE_VNODE(*vpp, target_fso->fid, VREG);
	VTOC(*vpp)->c_device = FSDB->device;
	VTOC(*vpp)->c_inode = target_fso->data.file->Inode();

FreeLocks:
	FSDB->Put(&parent_fso);
	FSDB->Put(&target_fso);
	int retry_call = 0;
	End_VFS(&retry_call);
	if (!retry_call) break;
    }

    if (u.u_error == EINCONS) {
	u.u_error = ENOENT;
	k_Purge(&dcp->c_fid, 1);
    }
}


void vproc::remove(struct vnode *dvp, char *name) {
    struct cnode *dcp = VTOC(dvp);

    LOG(1, ("vproc::remove: fid = (%x.%x.%x), name = %s\n",
	     dcp->c_fid.Volume, dcp->c_fid.Vnode, dcp->c_fid.Unique, name));

    fsobj *parent_fso = 0;
    fsobj *target_fso = 0;

    /* Disallow removal of {'.','..', '/'}. */
    if (STREQ(name, "..") || STREQ(name, ".") || STREQ(name, ""))
	{ u.u_error = EINVAL; return; }

    for (;;) {
	Begin_VFS(dcp->c_fid.Volume, (int)VFSOP_REMOVE);
	if (u.u_error) break;

	/* Get the parent object and verify that it is a directory. */
	u.u_error = FSDB->Get(&parent_fso, &dcp->c_fid, CRTORUID(u.u_cred), RC_DATA);
	if (u.u_error) goto FreeLocks;
	if (!parent_fso->IsDir())
	    { u.u_error = ENOTDIR; goto FreeLocks; }

	/* Get the target object. */
	u.u_error = parent_fso->Lookup(&target_fso, 0, name, CRTORUID(u.u_cred));
	if (u.u_error) goto FreeLocks;

	/* Verify that it is not a directory. */
	if (target_fso->IsDir())
	    { u.u_error = EISDIR; goto FreeLocks; }

	/* Verify that we have delete permission for the parent. */
	u.u_error = parent_fso->Access((long)PRSFS_DELETE, 0, CRTORUID(u.u_cred));
	if (u.u_error) goto FreeLocks;

	/* Do the remove. */
	parent_fso->PromoteLock();
	target_fso->PromoteLock();
	u.u_error = parent_fso->Remove(name, target_fso, CRTORUID(u.u_cred));

FreeLocks:
	FSDB->Put(&parent_fso);
	FSDB->Put(&target_fso);
	int retry_call = 0;
	End_VFS(&retry_call);
	if (!retry_call) break;
    }

    if (u.u_error == EINCONS) {
	u.u_error = ENOENT;
	k_Purge(&dcp->c_fid, 1);
    }
}


void vproc::link(struct vnode *vp, struct vnode *tdvp, char *toname) {
    struct cnode *dcp = VTOC(tdvp);
    struct cnode *scp = VTOC(vp);

    LOG(1, ("vproc::link: fid = (%x.%x.%x), td_fid = (%x.%x.%x), toname = %s\n",
	     scp->c_fid.Volume, scp->c_fid.Vnode, scp->c_fid.Unique,
	     dcp->c_fid.Volume, dcp->c_fid.Vnode, dcp->c_fid.Unique, toname));

    fsobj *parent_fso = 0;
    fsobj *source_fso = 0;
    fsobj *target_fso = 0;

    /* Disallow link of {'.','..', '/'}. */
    if (STREQ(toname, "..") || STREQ(toname, ".") || STREQ(toname, ""))
	{ u.u_error = EINVAL; return; }

    /* Disallow names of the form "@XXXXXXXX.YYYYYYYY.ZZZZZZZZ". */
    if (strlen(toname) == 27 && toname[0] == '@' && toname[9] == '.' && toname[18] == '.')
	{ u.u_error = EINVAL; return; }

    /* Another pathological case. */
    if (FID_EQ(dcp->c_fid, scp->c_fid))
	{ u.u_error = EINVAL; return; }

    for (;;) {
	Begin_VFS(dcp->c_fid.Volume, (int)VFSOP_LINK);
	if (u.u_error) break;

	/* Get the target parent and verify that it is a directory. */
	u.u_error = FSDB->Get(&parent_fso, &dcp->c_fid, CRTORUID(u.u_cred), RC_DATA);
	if (u.u_error) goto FreeLocks;
	if (!parent_fso->IsDir())
	    { u.u_error = ENOTDIR; goto FreeLocks; }

	/* Verify that the source is in the target parent. */
	if (!parent_fso->dir_IsParent(&scp->c_fid)) {
	    FSDB->Put(&parent_fso);
	    u.u_error = FSDB->Get(&source_fso, &scp->c_fid, CRTORUID(u.u_cred), RC_STATUS);
	    if (u.u_error) goto FreeLocks;

	    /* Source exists, but it is not in the target parent. */
	    u.u_error = EXDEV;
	    goto FreeLocks;
	}

	/* Get the source object. */
	/* This violates locking protocol if FID_LT(scp->c_fid, dcp->c_fid)! -JJK */
	u.u_error = FSDB->Get(&source_fso, &scp->c_fid, CRTORUID(u.u_cred), RC_STATUS);
	if (u.u_error) goto FreeLocks;

	/* Verify that the source is a file. */
	if (!source_fso->IsFile())
	    { u.u_error = EISDIR; goto FreeLocks; }

	/* Verify that the target doesn't exist. */
	u.u_error = parent_fso->Lookup(&target_fso, 0, toname, CRTORUID(u.u_cred));
	if (u.u_error == 0) { u.u_error = EEXIST; goto FreeLocks; }
	if (u.u_error != ENOENT) goto FreeLocks;
	u.u_error = 0;

	/* Verify that we have insert permission. */
	u.u_error = parent_fso->Access((long)PRSFS_INSERT, 0, CRTORUID(u.u_cred));
	if (u.u_error) goto FreeLocks;

	/* Do the operation. */
	parent_fso->PromoteLock();
	source_fso->PromoteLock();
	u.u_error = parent_fso->Link(toname, source_fso, CRTORUID(u.u_cred));

FreeLocks:
	FSDB->Put(&source_fso);
	FSDB->Put(&parent_fso);
	FSDB->Put(&target_fso);
	int retry_call = 0;
	End_VFS(&retry_call);
	if (!retry_call) break;
    }

    if (u.u_error == EINCONS) {
	u.u_error = ENOENT;
	k_Purge(&dcp->c_fid, 1);
	k_Purge(&scp->c_fid, 1);
    }
}


void vproc::rename(struct vnode *dvp, char *name,
		   struct vnode *tdvp, char *toname) {
    struct cnode *spcp = VTOC(dvp);
    struct cnode *tpcp = VTOC(tdvp);

    LOG(1, ("vproc::rename: fid = (%x.%x.%x), td_fid = (%x.%x.%x), name = %s, toname = %s\n",
	     spcp->c_fid.Volume, spcp->c_fid.Vnode, spcp->c_fid.Unique,
	     tpcp->c_fid.Volume, tpcp->c_fid.Vnode, tpcp->c_fid.Unique,
	     name, toname));

    int	SameParent = FID_EQ(spcp->c_fid, tpcp->c_fid);
    int	TargetExists = 0;
    fsobj *s_parent_fso = 0;
    fsobj *t_parent_fso = 0;
    fsobj *s_fso = 0;
    fsobj *t_fso = 0;

    /* Disallow rename from/to {'.','..', '/'}. */
    if (STREQ(name, "..") || STREQ(name, ".") || STREQ(name, "") ||
	 STREQ(toname, "..") || STREQ(toname, ".") || STREQ(toname, ""))
	{ u.u_error = EINVAL; return; }

    /* Disallow names of the form "@XXXXXXXX.YYYYYYYY.ZZZZZZZZ". */
    if (strlen(toname) == 27 && toname[0] == '@' && toname[9] == '.' && toname[18] == '.')
	{ u.u_error = EINVAL; return; }

    /* Ensure that objects are in the same volume. */
    if (spcp->c_fid.Volume != tpcp->c_fid.Volume)
	{ u.u_error = EXDEV; return; }

    for (;;) {
	Begin_VFS(spcp->c_fid.Volume, (int)VFSOP_RENAME);
	if (u.u_error) break;

	/* Acquire the parent(s). */
	if (SameParent) {
	    u.u_error = FSDB->Get(&t_parent_fso, &tpcp->c_fid, CRTORUID(u.u_cred), RC_DATA);
	    if (u.u_error) goto FreeLocks;
	}
	else {
	    if (FID_LT(spcp->c_fid, tpcp->c_fid)) {
		u.u_error = FSDB->Get(&s_parent_fso, &spcp->c_fid, CRTORUID(u.u_cred), RC_DATA);
		if (u.u_error) goto FreeLocks;
		u.u_error = FSDB->Get(&t_parent_fso, &tpcp->c_fid, CRTORUID(u.u_cred), RC_DATA);
		if (u.u_error) goto FreeLocks;
	    }
	    else {
		u.u_error = FSDB->Get(&t_parent_fso, &tpcp->c_fid, CRTORUID(u.u_cred), RC_DATA);
		if (u.u_error) goto FreeLocks;
		u.u_error = FSDB->Get(&s_parent_fso, &spcp->c_fid, CRTORUID(u.u_cred), RC_DATA);
		if (u.u_error) goto FreeLocks;
	    }
	}

	/* Verify that we have the necessary permissions in the parent(s). */
	{
	    fsobj *f = (SameParent ? t_parent_fso : s_parent_fso);
	    u.u_error = f->Access((long)PRSFS_DELETE, 0, CRTORUID(u.u_cred));
	    if (u.u_error) goto FreeLocks;
	}
	u.u_error = t_parent_fso->Access((long)PRSFS_INSERT, 0, CRTORUID(u.u_cred));
	if (u.u_error) goto FreeLocks;

	/* Acquire the source and target (if it exists). */
	/* The locking protocol is violated here! -JJK */
	/* We need data for directories! */
	/* 
	 * If the target exists, this thread must look up (and in the
	 * process read-lock) both child objects.  To avoid deadlock, this
	 * should be done in fid-order, unfortunately we do not know
	 * the fids of the objects yet.  We have to look them up to find
	 * out.  We get away with avoiding deadlock with the reintegrator 
	 * thread here because it only read-locks leaf nodes, so these
	 * lookups will go through.  
	 */
	{
	    fsobj *f = (SameParent ? t_parent_fso : s_parent_fso);
	    u.u_error = f->Lookup(&s_fso, 0, name, CRTORUID(u.u_cred));
	    if (u.u_error) goto FreeLocks;

	    if (s_fso->IsMtPt() || s_fso->IsMTLink())
		{ u.u_error = EINVAL; goto FreeLocks; }

	    if (s_fso->IsDir()) {
		ViceFid s_fid = s_fso->fid;
		FSDB->Put(&s_fso);

		u.u_error = FSDB->Get(&s_fso, &s_fid, CRTORUID(u.u_cred), RC_DATA, name);
		if (u.u_error) goto FreeLocks;
	    }
	}
	u.u_error = t_parent_fso->Lookup(&t_fso, 0, toname, CRTORUID(u.u_cred));
	if (u.u_error) {
	    if (u.u_error != ENOENT) goto FreeLocks;
	    u.u_error = 0;
	}
	else {
	    TargetExists = 1;
	    k_Purge(&t_fso->fid, 1);

	    if (t_fso->IsMtPt() || t_fso->IsMTLink())
		{ u.u_error = EINVAL; goto FreeLocks; }

	    if (t_fso->IsDir()) {
		ViceFid t_fid = t_fso->fid;
		FSDB->Put(&t_fso);

		u.u_error = FSDB->Get(&t_fso, &t_fid, CRTORUID(u.u_cred), RC_DATA, toname);
		if (u.u_error) goto FreeLocks;
	    }
	}

	/* Prevent rename from/to MtPts, MTLinks, VolRoots. -JJK */
	if (s_fso->IsMtPt() || s_fso->IsMTLink() || s_fso->IsRoot() ||
	    (TargetExists && (t_fso->IsMtPt() || t_fso->IsMTLink() || t_fso->IsRoot())))
	    { u.u_error = EINVAL; goto FreeLocks; }

	/* Watch out for aliasing. */
	if (FID_EQ(s_fso->fid, tpcp->c_fid) || FID_EQ(s_fso->fid, spcp->c_fid))
	    { u.u_error = ELOOP; goto FreeLocks; }

	/* Cannot allow rename out of the source directory if source has multiple links! */
	if (s_fso->IsFile() && s_fso->stat.LinkCount > 1 && !SameParent)
	    { u.u_error = EXDEV; goto FreeLocks; }

	/* If the target object exists, overwriting it means there are more checks that we need to make. */
	if (TargetExists) {
	    /* Verify that we have delete permission in the target parent. */
	    if (!SameParent) {
		u.u_error = t_parent_fso->Access((long)PRSFS_DELETE, 0, CRTORUID(u.u_cred));
		if (u.u_error) goto FreeLocks;
	    }

	    /* Watch out for aliasing. */
	    if (FID_EQ(t_fso->fid, tpcp->c_fid) || FID_EQ(t_fso->fid, spcp->c_fid) ||
		FID_EQ(t_fso->fid, s_fso->fid))
		{ u.u_error = ELOOP; goto FreeLocks; }

	    /* Verify that source and target are the same type of object, and that target is empty if a directory. */
	    if (t_fso->IsDir()) {
		if (!s_fso->IsDir()) { u.u_error = ENOTDIR; goto FreeLocks; }
		if (!t_fso->dir_IsEmpty()) { u.u_error = ENOTEMPTY; goto FreeLocks; }
	    }
	    else {
		if (s_fso->IsDir()) { u.u_error = EISDIR; goto FreeLocks; }
	    }

	    /* Ensure that the target is not a descendent of the source. */
	    /* This shoots the locking protocol to hell! -JJK */
	    /* This fails, perhaps incorrectly, if user doesn't have read permission up to volume root! -JJK */
	    if (!SameParent && s_fso->IsDir() && !t_parent_fso->IsRoot()) {
		ViceFid test_fid = t_parent_fso->pfid;
		for (;;) {
		    if (FID_EQ(s_fso->fid, test_fid))
			{ u.u_error = ELOOP; goto FreeLocks; }

		    /* Exit when volume root is reached. */
		    if (test_fid.Vnode == ROOT_VNODE && test_fid.Unique == ROOT_UNIQUE)
			break;

		    /* test_fid <-- Parent(test_fid). */
		    /* Take care not to get s_parent twice! */
		    if (FID_EQ(s_parent_fso->fid, test_fid)) {
			test_fid = s_parent_fso->pfid;
			continue;
		    }
		    fsobj *test_fso = 0;
		    u.u_error = FSDB->Get(&test_fso, &test_fid, CRTORUID(u.u_cred), RC_DATA);
		    if (u.u_error) goto FreeLocks;
		    test_fid = test_fso->pfid;
		    FSDB->Put(&test_fso);
		}
	    }
	}

	/* Do the operation. */
	t_parent_fso->PromoteLock();
	if (!SameParent) s_parent_fso->PromoteLock();

	/* 
	 * To avoid potential deadlock with the reintegrator thread,
	 * we must promote the locks of the child objects in fid 
	 * order if the target exists.
	 */
	if (!TargetExists)
	    s_fso->PromoteLock();
	else {
	    if (FID_LT(s_fso->fid, t_fso->fid)) {
		    s_fso->PromoteLock();
		    t_fso->PromoteLock();
	    } else {
		    t_fso->PromoteLock();
		    s_fso->PromoteLock();
	    }
	}

	u.u_error = t_parent_fso->Rename(s_parent_fso, name, s_fso,
					 toname, t_fso, CRTORUID(u.u_cred));

FreeLocks:
	FSDB->Put(&s_parent_fso);
	FSDB->Put(&t_parent_fso);
	FSDB->Put(&s_fso);
	FSDB->Put(&t_fso);
	int retry_call = 0;
	End_VFS(&retry_call);
	if (!retry_call) break;
    }

    if (u.u_error == EINCONS) {
	u.u_error = ENOENT;
	k_Purge(&tpcp->c_fid, 1);
	if (!SameParent)
	    k_Purge(&spcp->c_fid, 1);
    }
}


void vproc::mkdir(struct vnode *dvp, char *name,
		  struct vattr *vap, struct vnode **vpp) {
    struct cnode *dcp = VTOC(dvp);

    LOG(1, ("vproc::mkdir: fid = (%x.%x.%x), name = %s\n",
	     dcp->c_fid.Volume, dcp->c_fid.Vnode, dcp->c_fid.Unique, name));

    fsobj *parent_fso = 0;
    fsobj *target_fso = 0;
    struct cnode *cp = 0;
    *vpp = 0;

    /* Disallow mkdir of {'.','..', '/'}. */
    if (STREQ(name, "..") || STREQ(name, ".") || STREQ(name, ""))
	{ u.u_error = EINVAL; return; }

    /* Disallow names of the form "@XXXXXXXX.YYYYYYYY.ZZZZZZZZ". */
    if (strlen(name) == 27 && name[0] == '@' && name[9] == '.' && name[18] == '.')
	{ u.u_error = EINVAL; return; }

    for (;;) {
	Begin_VFS(dcp->c_fid.Volume, (int)VFSOP_MKDIR);
	if (u.u_error) break;

	/* Get the parent object and verify that it is a directory. */
	u.u_error = FSDB->Get(&parent_fso, &dcp->c_fid, CRTORUID(u.u_cred), RC_DATA);
	if (u.u_error) goto FreeLocks;
	if (!parent_fso->IsDir()) { u.u_error = ENOTDIR; goto FreeLocks; }

	/* Verify that the target doesn't exist. */
	u.u_error = parent_fso->Lookup(&target_fso, 0, name, CRTORUID(u.u_cred));
	if (u.u_error == 0) { u.u_error = EEXIST; goto FreeLocks; }
	if (u.u_error != ENOENT) goto FreeLocks;
	u.u_error = 0;

	/* Verify that we have insert permission. */
	u.u_error = parent_fso->Access((long)PRSFS_INSERT, 0, CRTORUID(u.u_cred));
	if (u.u_error) goto FreeLocks;

	/* Do the operation. */
	parent_fso->PromoteLock();
	u.u_error = parent_fso->Mkdir(name, &target_fso, CRTORUID(u.u_cred),
				      vap->va_mode & 0777, FSDB->StdPri());
	if (u.u_error) goto FreeLocks;

	/* Set OUT parameter. */
	target_fso->GetVattr(vap);
	if (u.u_error) goto FreeLocks;
	MAKE_VNODE(*vpp, target_fso->fid, VDIR);

FreeLocks:
	FSDB->Put(&parent_fso);
	FSDB->Put(&target_fso);
	int retry_call = 0;
	End_VFS(&retry_call);
	if (!retry_call) break;
    }

    if (u.u_error == EINCONS) {
	u.u_error = ENOENT;
	k_Purge(&dcp->c_fid, 1);
    }
}


void vproc::rmdir(struct vnode *dvp, char *name) {
    struct cnode *dcp = VTOC(dvp);

    LOG(1, ("vproc::rmdir: fid = (%x.%x.%x), name = %s\n",
	     dcp->c_fid.Volume, dcp->c_fid.Vnode, dcp->c_fid.Unique, name));

    fsobj *parent_fso = 0;
    fsobj *target_fso = 0;

    /* Disallow removal of {'.','..', '/'}. */
    if (STREQ(name, "..") || STREQ(name, ".") || STREQ(name, ""))
	{ u.u_error = EINVAL; return; }

    for (;;) {
	Begin_VFS(dcp->c_fid.Volume, (int)VFSOP_RMDIR);
	if (u.u_error) break;

	/* Get the parent object and verify that it is a directory. */
	u.u_error = FSDB->Get(&parent_fso, &dcp->c_fid, CRTORUID(u.u_cred), RC_DATA);
	if (u.u_error) goto FreeLocks;
	if (!parent_fso->IsDir()) { u.u_error = ENOTDIR; goto FreeLocks; }

	/* Get the target object. */
	u.u_error = parent_fso->Lookup(&target_fso, 0, name, CRTORUID(u.u_cred));
	if (u.u_error) goto FreeLocks;

	/* Sanity check. */
	if (target_fso->IsRoot())
	    { target_fso->print(logFile); Choke("vproc::rmdir: target is root"); }

	/* Verify that it is a directory (mount points are an exception). */
	if (!target_fso->IsDir() || target_fso->IsMtPt())
	    { u.u_error = ENOTDIR; goto FreeLocks; }

	/* Must have data for the target. */
	ViceFid target_fid; target_fid = target_fso->fid;
	FSDB->Put(&target_fso);
	u.u_error = FSDB->Get(&target_fso, &target_fid, CRTORUID(u.u_cred), RC_DATA, name);
	if (u.u_error) goto FreeLocks;

	/* Verify that the target is empty. */
	if (!target_fso->dir_IsEmpty())
	    { u.u_error = ENOTEMPTY; goto FreeLocks; }

	/* Verify that we have delete permission for the parent. */
	u.u_error = parent_fso->Access((long)PRSFS_DELETE, 0, CRTORUID(u.u_cred));
	if (u.u_error) goto FreeLocks;

	/* Do the operation. */
	parent_fso->PromoteLock();
	target_fso->PromoteLock();
	u.u_error = parent_fso->Rmdir(name, target_fso, CRTORUID(u.u_cred));

FreeLocks:
	FSDB->Put(&parent_fso);
	FSDB->Put(&target_fso);
	int retry_call = 0;
	End_VFS(&retry_call);
	if (!retry_call) break;
    }

    if (u.u_error == EINCONS) {
	u.u_error = ENOENT;
	k_Purge(&dcp->c_fid, 1);
    }
}


void vproc::readdir(struct vnode *dvp, struct uio *uiop) {
    struct cnode *dcp = VTOC(dvp);

    LOG(1, ("vproc::readdir: fid = (%x.%x.%x), offset = %d, count = %d\n",
	     dcp->c_fid.Volume, dcp->c_fid.Vnode, dcp->c_fid.Unique,
	     uiop->uio_offset, uiop->uio_iov->iov_len));

    int offset = (int) uiop->uio_offset;
    char *buf = uiop->uio_iov->iov_base;
    int len = uiop->uio_iov->iov_len;
    int cc = 0;

    if (len > V_BLKSIZE)
	{ u.u_error = EINVAL; return; }

    fsobj *f = 0;

    for (;;) {
	Begin_VFS(dcp->c_fid.Volume, (int)VFSOP_READDIR);
	if (u.u_error) break;

	/* Get the object. */
	u.u_error = FSDB->Get(&f, &dcp->c_fid, CRTORUID(u.u_cred), RC_DATA);
	if (u.u_error) goto FreeLocks;

	/* Verify that it is a directory. */
	if (!f->IsDir())
	    { u.u_error = ENOTDIR; goto FreeLocks; }

	/* Verify that we have read permission. */
/*
	 u.u_error = f->Access((long)PRSFS_LOOKUP, 0, CRTORUID(u.u_cred));
	 if (u.u_error) goto FreeLocks;
*/

	/* Do the operation. */
	u.u_error = f->Readdir(buf, offset, len, &cc, CRTORUID(u.u_cred));
	if (u.u_error) goto FreeLocks;
	uiop->uio_resid -= cc;

FreeLocks:
	FSDB->Put(&f);
	int retry_call = 0;
	End_VFS(&retry_call);
	if (!retry_call) break;
    }

    if (u.u_error == EINCONS) {
	u.u_error = ENOENT;
	k_Purge(&dcp->c_fid, 1);
    }
}


void vproc::symlink(struct vnode *dvp, char *contents,
		    struct vattr *vap, char *name) {
    struct cnode *dcp = VTOC(dvp);

    LOG(1, ("vproc::symlink: fid = (%x.%x.%x), contents = %s, name = %s\n",
	     dcp->c_fid.Volume, dcp->c_fid.Vnode, dcp->c_fid.Unique, contents, name));

    fsobj *parent_fso = 0;
    fsobj *target_fso = 0;

    /* Disallow link of {'.','..', '/'}. */
    if (STREQ(name, "..") || STREQ(name, ".") || STREQ(name, ""))
	{ u.u_error = EINVAL; return; }

    /* Disallow names of the form "@XXXXXXXX.YYYYYYYY.ZZZZZZZZ". */
    if (strlen(name) == 27 && name[0] == '@' && name[9] == '.' && name[18] == '.')
	{ u.u_error = EINVAL; return; }

    for (;;) {
	Begin_VFS(dcp->c_fid.Volume, (int)VFSOP_SYMLINK);
	if (u.u_error) break;

	/* Get the parent object and verify that it is a directory. */
	u.u_error = FSDB->Get(&parent_fso, &dcp->c_fid, CRTORUID(u.u_cred), RC_DATA);
	if (u.u_error) goto FreeLocks;
	if (!parent_fso->IsDir()) { u.u_error = ENOTDIR; goto FreeLocks; }

	/* Verify that the target doesn't exist. */
	u.u_error = parent_fso->Lookup(&target_fso, 0, name, CRTORUID(u.u_cred));
	if (u.u_error == 0) { u.u_error = EEXIST; goto FreeLocks; }
	if (u.u_error != ENOENT) goto FreeLocks;
	u.u_error = 0;

	/* Verify that we have insert permission. */
	u.u_error = parent_fso->Access((long)PRSFS_INSERT, 0, CRTORUID(u.u_cred));
	if (u.u_error) goto FreeLocks;

	/* Do the operation. */
	unsigned short Mode;
	Mode = ((contents[0] == '%' || contents[0] == '#' || contents[0] == '@') &&
		(contents[strlen(contents) - 1] == '.'))
	  ? 0644		/* mount point */
	  : 0755;		/* real symbolic link */
	parent_fso->PromoteLock();
	u.u_error = parent_fso->Symlink(contents, name, CRTORUID(u.u_cred),
					Mode, FSDB->StdPri());
	if (u.u_error) goto FreeLocks;

	/* Set vattr fields? */

	/* Target is not an OUT parameter. */

FreeLocks:
	FSDB->Put(&parent_fso);
	FSDB->Put(&target_fso);
	int retry_call = 0;
	End_VFS(&retry_call);
	if (!retry_call) break;
    }

    if (u.u_error == EINCONS) {
	u.u_error = ENOENT;
	k_Purge(&dcp->c_fid, 1);
    }
}


void vproc::readlink(struct vnode *vp, struct uio *uiop) {
    struct cnode *cp = VTOC(vp);

    LOG(1, ("vproc::readlink: fid = (%x.%x.%x)\n",
	     cp->c_fid.Volume, cp->c_fid.Vnode, cp->c_fid.Unique));

    char *buf = uiop->uio_iov->iov_base;
    int len = uiop->uio_iov->iov_len;
    int cc = 0;

    if (len > MAXPATHLEN)
	{ u.u_error = EINVAL; return; }

    fsobj *f = 0;

    for (;;) {
	Begin_VFS(cp->c_fid.Volume, (int)VFSOP_READLINK);
	if (u.u_error) break;

	/* Get the object. */
	u.u_error = FSDB->Get(&f, &cp->c_fid, CRTORUID(u.u_cred), RC_DATA);
	if (u.u_error) goto FreeLocks;

	/* Verify that it is a symlink. */
	if (!f->IsSymLink())
	    { u.u_error = EINVAL; goto FreeLocks; }

	/*Verify that we have read permission for it. */
/*
	 u.u_error = f->Access((long)PRSFS_LOOKUP, 0, CRTORUID(u.u_cred));
	 if (u.u_error)
	     { if (u.u_error == EINCONS) u.u_error = ENOENT; goto FreeLocks; }
*/

	/* Retrieve the link contents from the cache. */
	u.u_error = f->Readlink(buf, len, &cc, CRTORUID(u.u_cred));
	if (u.u_error) goto FreeLocks;
	uiop->uio_resid -= cc;

FreeLocks:
	FSDB->Put(&f);
	int retry_call = 0;
	End_VFS(&retry_call);
	if (!retry_call) break;
    }

    if (u.u_error == EINCONS) {
	u.u_error = 0;
	k_Purge(&cp->c_fid, 1);

	/* Make a "fake" name for the inconsistent object. */
	sprintf(buf, "@%08x.%08x.%08x",
		cp->c_fid.Volume, cp->c_fid.Vnode, cp->c_fid.Unique);
	uiop->uio_resid -= 27;
    }
}


void vproc::fsync(struct vnode *vp) {
    struct cnode *cp = VTOC(vp);

    LOG(1, ("vproc::fsync: fid = (%x.%x.%x)\n",
	     cp->c_fid.Volume, cp->c_fid.Vnode, cp->c_fid.Unique));

    fsobj *f = 0;

    for (;;) {
	Begin_VFS(cp->c_fid.Volume, (int)VFSOP_FSYNC);
	if (u.u_error) break;

	/* Get the object. */
	u.u_error = FSDB->Get(&f, &cp->c_fid, CRTORUID(u.u_cred), RC_STATUS);
	if (u.u_error) goto FreeLocks;

	/* 
	 * what we want: if the file is open for write, sync the
	 * changes to it and flush associated RVM updates.
	 * below is the heavy handed version.
	 * NB: if this is changed to modify object state, this
	 * operation can no longer be an observer!
	 */
	if (f->flags.owrite) {
	    ::sync();
	    RecovFlush();
	}

FreeLocks:
	FSDB->Put(&f);
	int retry_call = 0;
	End_VFS(&retry_call);
	if (!retry_call) break;
    }

    if (u.u_error == EINCONS) {
	u.u_error = ENOENT;
	k_Purge(&cp->c_fid, 1);
    }
}


void vproc::inactive(struct vnode *vp) {
    struct cnode *cp = VTOC(vp);

    LOG(1, ("vproc::inactive: fid = (%x.%x.%x)\n",
	     cp->c_fid.Volume, cp->c_fid.Vnode, cp->c_fid.Unique));

    u.u_error = EOPNOTSUPP;
}


void vproc::fid(struct vnode *vp, struct fid **fidpp) {
    struct cnode *cp = VTOC(vp);

    LOG(1, ("vproc::fid: fid = (%x.%x.%x)\n",
	     cp->c_fid.Volume, cp->c_fid.Vnode, cp->c_fid.Unique));

    u.u_error = EOPNOTSUPP;
}
