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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/venus/vproc_vfscalls.cc,v 4.20 1998/10/04 21:58:11 braam Exp $";
#endif /*_BLURB_*/




/*
 *
 *    Implementation of the Venus VFS interface.
 *
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
 *        1. Decide whether or not data is needed in the following 
 *           cases (under all COP modes):
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
#include <unistd.h>
#include <stdlib.h>

#include <vice.h>
#include <prs_fs.h>

#ifdef __cplusplus
}
#endif __cplusplus

/* from libal */

/* from venus */
#include "fso.h"
#include "hdb.h"
#include "local.h"
#include "venus.private.h"
#include "vproc.h"
#include "worker.h"

/* Temporary!  Move to cnode.h. -JJK */
#define	C_INCON	0x2


/* ***** VFS Operations  ***** */

void vproc::mount(char *path, void *data) {
    LOG(1, ("vproc::mount: path = %s\n", path));
    u.u_error = EOPNOTSUPP;
}


void vproc::unmount() {
    LOG(1, ("vproc::unmount\n"));
    u.u_error = EOPNOTSUPP;
}


void vproc::root(struct venus_cnode *vpp) {
    LOG(1, ("vproc::root\n"));

    /* Set OUT parameter. */
    MAKE_CNODE(*vpp, rootfid, C_VDIR);
}


void vproc::statfs(struct statfs *sbp) {
    LOG(1, ("vproc::statfs\n"));
    u.u_error = EOPNOTSUPP;
}


void vproc::sync() {
    LOG(1, ("vproc::sync\n"));
    u.u_error = EOPNOTSUPP;
}


void vproc::vget(struct venus_cnode *vpp, struct cfid *cfidp) {

    LOG(1, ("vproc::vget: fid = %s, nc = %x\n", FID_(&cfidp->cfid_fid),
	    u.u_nc));

    fsobj *f = 0;

    if (u.u_nc && LogLevel >= 100)
	u.u_nc->print(logFile);

    for (;;) {
	Begin_VFS(cfidp->cfid_fid.Volume, CODA_VGET);
	if (u.u_error) break;

	u.u_error = FSDB->Get(&f, &cfidp->cfid_fid, CRTORUID(u.u_cred), RC_STATUS);
	if (u.u_error) {
	    if (u.u_error == EINCONS) {
		u.u_error = 0;

		/* Set OUT parameter according to "fake" vnode. */
		MAKE_CNODE(*vpp, cfidp->cfid_fid, C_VLNK);
		vpp->c_flags |= C_INCON;
	    }

	    goto FreeLocks;
	}

	/* Set OUT parameter. */
	MAKE_CNODE(*vpp, f->fid, FTTOVT(f->stat.VnodeType));
	if (f->IsFake() || f->IsMTLink())
		vpp->c_flags |= C_INCON;

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

void vproc::open(struct venus_cnode *cp, int flags) {

    LOG(1, ("vproc::open: fid = %s , flags = %x\n", 
	    FID_(&cp->c_fid), flags));

    /* Expand the flags argument into some useful predicates. */

    int readp =  (flags & C_O_READ)  != 0;
    int writep = (flags & C_O_WRITE) != 0;
    int truncp = (flags & C_O_TRUNC) != 0;
    int exclp =  (flags & C_O_EXCL)  != 0;
    int createp =  (flags & C_O_CREAT)  != 0;
    int	execp =	0;	    /* With VFS we're no longer told of execs! -JJK */

    if ( createp ) 
	    writep = 1;

    fsobj *f = 0;

    for (;;) {
	Begin_VFS(cp->c_fid.Volume, CODA_OPEN,
		  writep ? VM_MUTATING : VM_OBSERVING);
	if (u.u_error) break;

	/* Get the object. */
	u.u_error = FSDB->Get(&f, &cp->c_fid, CRTORUID(u.u_cred), RC_DATA);
	if (u.u_error) goto FreeLocks;

	if (exclp) { 
		u.u_error = EEXIST; 
		goto FreeLocks; 
	}

	/* Verify that we have the necessary permission. */
	if (readp) {
	    long rights = f->IsFile() ? PRSFS_READ : PRSFS_LOOKUP;
	    int modes = f->IsFile() ? C_A_R_OK : 0;
	    u.u_error = f->Access(rights, modes, CRTORUID(u.u_cred));
	    if (u.u_error) goto FreeLocks;
	}
	if (writep || truncp) {
	    if (f->IsDir()) { u.u_error = EISDIR; goto FreeLocks; }

	    /* Special modes to pass:
                Truncating requires write permission.
		  Newly created stuff is writeable if parent allows it, 
                   modebits are ignored for new files: C_O_C_OK
	         Otherwise, either write or insert suffices (to support
                   insert only directories). 
	    */
	    long rights = (truncp
			   ? (long)PRSFS_WRITE
			   : (long)(PRSFS_WRITE | PRSFS_INSERT));
	    int modes = (createp ? C_A_C_OK : C_A_W_OK);
	    u.u_error = f->Access(rights, modes, CRTORUID(u.u_cred));
	    if (u.u_error) goto FreeLocks;
	}

	/* Do the operation. */
	u.u_error = f->Open(writep, execp, truncp,
			    cp, CRTORUID(u.u_cred));
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


void vproc::close(struct venus_cnode *cp, int flags) 
{
    LOG(1, ("vproc::close: fid = %s, flags = %x\n",
	    FID_(&cp->c_fid), flags));

    /* Expand the flags argument into some useful predicates. */
    int writep = (flags & (C_O_WRITE | C_O_TRUNC)) != 0;
    int	execp =	0;	    /* ??? -JJK */

    fsobj *f = 0;

    for (;;) {
	Begin_VFS(cp->c_fid.Volume, CODA_CLOSE,
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
          LOG(0, ("vproc::close: Don't have DATA and not DYING! (fid = %s, flags = %x)\n", FID_(&cp->c_fid), flags));

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



void vproc::ioctl(struct venus_cnode *cp, unsigned int com,
		   struct ViceIoctl *data, int flags) 
{

    LOG(1, ("vproc::ioctl(%d): fid = %s, com = %s\n",
	     u.u_cred.cr_uid, FID_(&cp->c_fid), IoctlOpStr(com)));

    do_ioctl(&cp->c_fid, com, data);

    if (u.u_error == EINCONS) {
	u.u_error = ENOENT;
	k_Purge(&cp->c_fid, 1);
    }
}



void vproc::getattr(struct venus_cnode *cp, struct coda_vattr *vap) 
{
    LOG(1, ("vproc::getattr: fid = %s\n", FID_(&cp->c_fid)));

    fsobj *f = 0;

    for (;;) {
	Begin_VFS(cp->c_fid.Volume, CODA_GETATTR);
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
	vap->va_mode = 0444;
	vap->va_type = FTTOVT(SymbolicLink);
	vap->va_uid = (short)V_UID;
	vap->va_gid = (short)V_GID;
	vap->va_nlink = 1;
	vap->va_size = 27;  /* @XXXXXXXX.YYYYYYYY.ZZZZZZZZ */
	vap->va_blocksize = V_BLKSIZE;
	vap->va_fileid = FidToNodeid(&cp->c_fid);
	vap->va_atime.tv_sec = Vtime();
        vap->va_atime.tv_nsec = 0;
	vap->va_mtime = vap->va_atime;
	vap->va_ctime.tv_sec = 0;
	vap->va_ctime.tv_nsec = 0;
	vap->va_rdev = 1;
	vap->va_bytes = vap->va_size;
    }
}


/* The only attributes that are allowed to be changed are:
      va_mode	    chmod, fchmod
      va_uid	    chown, fchown
      va_size	    truncate, ftruncate
      va_mtime	    utimes
*/
void vproc::setattr(struct venus_cnode *cp, struct coda_vattr *vap) {

    LOG(1, ("vproc::setattr: fid = %s\n", FID_(&cp->c_fid)));

    fsobj *f = 0;

    /* 
     * BSD44 supports chflags, which sets the va_flags field of 
     * the vattr.  Coda doesn't support these flags, but we will
     * allow calls that clear the field.  
     */
    /* Cannot set these attributes. */
    if ( (vap->va_fileid != VA_IGNORE_ID) ||
	 (vap->va_nlink != VA_IGNORE_NLINK) ||
	 (vap->va_blocksize != VA_IGNORE_BLOCKSIZE) ||
	 (vap->va_rdev != VA_IGNORE_RDEV) ||
	 (vap->va_flags != VA_IGNORE_FLAGS && vap->va_flags != 0) ||
	  (vap->va_bytes != VA_IGNORE_STORAGE) ) {
	    u.u_error = EINVAL; 
	    return; 
    }

    /* Should be setting at least one of these. */
    if ( (vap->va_mode == VA_IGNORE_MODE) &&
	 (vap->va_uid == VA_IGNORE_UID) &&
	 (vap->va_gid == VA_IGNORE_GID) &&
	 (vap->va_size == VA_IGNORE_SIZE) &&
	 (vap->va_flags == VA_IGNORE_FLAGS) &&
	 (vap->va_atime.tv_sec == VA_IGNORE_TIME1) &&
	 (vap->va_mtime.tv_sec == VA_IGNORE_TIME1) &&
	 (vap->va_ctime.tv_sec == VA_IGNORE_TIME1) )

	CHOKE("vproc::setattr: no attributes specified");

    for (;;) {
	Begin_VFS(cp->c_fid.Volume, CODA_SETATTR);
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
		    if ( vap->va_mode & S_ISUID ) 
			    /* XXXX totally wrong: 
			       here a test for membership of 
			       System:Adminstrators of the euid
			       is what we want. 
			       
			       For now we will ask the fileserver to
			       enforce that.
			    */
			    u.u_error = f->Access((long)PRSFS_ADMINISTER, 
						  0, CRTORUID(u.u_cred));
		    else
			    u.u_error = f->Access((long)PRSFS_WRITE, 0, 
						  CRTORUID(u.u_cred));
		    if (u.u_error) 
			    goto FreeLocks;
	    }

	    /* chown, fchown */
	    if (vap->va_uid != VA_IGNORE_UID) {
		/* Need to allow for System:Administrators here! -JJK */
#if 0
		if (f->stat.Owner != (vuid_t)vap->va_uid)
		    { u.u_error = EACCES; goto FreeLocks; }
#endif
		u.u_error = f->Access((long)PRSFS_ADMINISTER, 0, CRTORUID(u.u_cred));
		if (u.u_error) goto FreeLocks;
	    }
	    /* gid should be V_GID for chown requests, 
	       VA_IGNORE_GID otherwise */
	    if (vap->va_gid != VA_IGNORE_GID)
	        vap->va_gid = V_GID;
	    /* truncate, ftruncate */
	    if (vap->va_size != VA_IGNORE_SIZE) {
		    if (!f->IsFile()) {
		    u.u_error = EISDIR; 
		    goto FreeLocks; 
		    }

		    u.u_error = f->Access((long)PRSFS_WRITE, C_A_W_OK, 
					  CRTORUID(u.u_cred));
		    if (u.u_error) 
			    goto FreeLocks;
	    }

	    /* utimes */
	    if ( (vap->va_atime.tv_sec != VA_IGNORE_TIME1) ||
		 (vap->va_mtime.tv_sec != VA_IGNORE_TIME1) ||
		 (vap->va_ctime.tv_sec != VA_IGNORE_TIME1) ) {
		    u.u_error = f->Access((long)PRSFS_WRITE, 0, 
					  CRTORUID(u.u_cred));
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
	if (!retry_call) 
		break;
    }

    if (u.u_error == EINCONS) {
	    u.u_error = ENOENT;
	    k_Purge(&cp->c_fid, 1);
    }
}


void vproc::access(struct venus_cnode *cp, int mode) 
{

    LOG(1, ("vproc::access: fid = %s, mode = %#o\n", FID_(&cp->c_fid), mode));

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
	Begin_VFS(cp->c_fid.Volume, CODA_ACCESS);
	if (u.u_error) break;

	/* Get the object. */
	u.u_error = FSDB->Get(&f, &cp->c_fid, CRTORUID(u.u_cred), RC_STATUS);
	if (u.u_error) goto FreeLocks;

	modes = mode & (OWNERBITS >> 6);
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


void vproc::lookup(struct venus_cnode *dcp, char *name, 
		   struct venus_cnode *cp) 
{

    LOG(1, ("vproc::lookup: fid = %s, name = %s, nc = %x\n",
	     FID_(&dcp->c_fid),name, u.u_nc));

    fsobj *parent_fso = 0;
    fsobj *target_fso = 0;

    for (;;) {
	Begin_VFS(dcp->c_fid.Volume, CODA_LOOKUP);
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
		    MAKE_CNODE(*cp, inc_fid, C_VLNK);
		    cp->c_flags |= C_INCON;
		}

		goto FreeLocks;
	    }
	}

	/* Set OUT parameter. */
	MAKE_CNODE(*cp, target_fso->fid, FTTOVT(target_fso->stat.VnodeType));
	if (target_fso->IsFake() || target_fso->IsMTLink())
	    cp->c_flags |= C_INCON;

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


void vproc::create(struct venus_cnode *dcp, char *name, struct coda_vattr *vap,
		   int excl, int mode, struct venus_cnode *cp) 
{

    LOG(1, ("vproc::create: fid = %s, name = %s, excl = %d, mode = %d\n",
	     FID_(&dcp->c_fid), name, excl, mode));

    fsobj *parent_fso = 0;
    fsobj *target_fso = 0;

    /* Expand the flags into some useful predicates. */
    int readp = (mode & C_M_READ) != 0;
    int writep = (mode & C_M_WRITE) != 0;
    int truncp = (vap->va_size == 0);
    int exclp = excl;

    /* Disallow creation of {'.','..', '/'}. */
    if (STREQ(name, "..") || STREQ(name, ".") || STREQ(name, ""))
	{ u.u_error = EINVAL; return; }

    /* Disallow names of the form "@XXXXXXXX.YYYYYYYY.ZZZZZZZZ". */
    if (strlen(name) == 27 && name[0] == '@' && name[9] == '.' && name[18] == '.')
	{ u.u_error = EINVAL; return; }

    for (;;) {
	Begin_VFS(dcp->c_fid.Volume, CODA_CREATE);
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
		u.u_error = target_fso->Access((long)PRSFS_READ, C_A_R_OK, CRTORUID(u.u_cred));
		if (u.u_error) goto FreeLocks;
	    }
	    if (writep || truncp) {
		u.u_error = target_fso->Access((long)PRSFS_WRITE, C_A_W_OK, CRTORUID(u.u_cred));
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
	MAKE_CNODE(*cp, target_fso->fid, C_VREG);
	cp->c_device = FSDB->device;
	cp->c_inode = target_fso->data.file->Inode();

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


void vproc::remove(struct venus_cnode *dcp, char *name) 
{

    LOG(1, ("vproc::remove: fid = %s, name = %s\n",
	     FID_(&dcp->c_fid), name));

    fsobj *parent_fso = 0;
    fsobj *target_fso = 0;

    /* Disallow removal of {'.','..', '/'}. */
    if (STREQ(name, "..") || STREQ(name, ".") || STREQ(name, ""))
	{ u.u_error = EINVAL; return; }

    for (;;) {
	Begin_VFS(dcp->c_fid.Volume, CODA_REMOVE);
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


void vproc::link(struct venus_cnode *scp, struct venus_cnode *dcp, 
		 char *toname) 
{

    LOG(1, ("vproc::link: fid = %s, td_fid = %s, toname = %s\n",
	    FID_(&scp->c_fid), FID_2(&dcp->c_fid), toname));

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
    if (FID_EQ(&dcp->c_fid, &scp->c_fid))
	{ u.u_error = EINVAL; return; }

    for (;;) {
	Begin_VFS(dcp->c_fid.Volume, CODA_LINK);
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


void vproc::rename(struct venus_cnode *spcp, char *name,
		   struct venus_cnode *tpcp, char *toname) 
{

    LOG(1, ("vproc::rename: fid = %s, td_fid = %s, name = %s, toname = %s\n",
	     FID_(&spcp->c_fid), FID_2(&tpcp->c_fid), name, toname));

    int	SameParent = FID_EQ(&spcp->c_fid, &tpcp->c_fid);
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
	Begin_VFS(spcp->c_fid.Volume, CODA_RENAME);
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
	if (FID_EQ(&s_fso->fid, &tpcp->c_fid) || FID_EQ(&s_fso->fid, &spcp->c_fid))
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
	    if (FID_EQ(&t_fso->fid, &tpcp->c_fid) || FID_EQ(&t_fso->fid, &spcp->c_fid) ||
		FID_EQ(&t_fso->fid, &s_fso->fid))
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
		    if (FID_EQ(&s_fso->fid, &test_fid))
			{ u.u_error = ELOOP; goto FreeLocks; }

		    /* Exit when volume root is reached. */
		    if (FID_IsVolRoot(&test_fid))
			break;

		    /* test_fid <-- Parent(test_fid). */
		    /* Take care not to get s_parent twice! */
		    if (FID_EQ(&s_parent_fso->fid, &test_fid)) {
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


void vproc::mkdir(struct venus_cnode *dcp, char *name,
		  struct coda_vattr *vap, struct venus_cnode *cp) 
{

    LOG(1, ("vproc::mkdir: fid = %s, name = %s\n", FID_(&dcp->c_fid), name));

    fsobj *parent_fso = 0;
    fsobj *target_fso = 0;

    /* Disallow mkdir of {'.','..', '/'}. */
    if (STREQ(name, "..") || STREQ(name, ".") || STREQ(name, ""))
	{ u.u_error = EINVAL; return; }

    /* Disallow names of the form "@XXXXXXXX.YYYYYYYY.ZZZZZZZZ". */
    if (strlen(name) == 27 && name[0] == '@' && name[9] == '.' && name[18] == '.')
	{ u.u_error = EINVAL; return; }

    for (;;) {
	Begin_VFS(dcp->c_fid.Volume, CODA_MKDIR);
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
	MAKE_CNODE(*cp, target_fso->fid, C_VDIR);

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


void vproc::rmdir(struct venus_cnode *dcp, char *name) 
{

    LOG(1, ("vproc::rmdir: fid = %s, name = %s\n", FID_(&dcp->c_fid), name));

    fsobj *parent_fso = 0;
    fsobj *target_fso = 0;

    /* Disallow removal of {'.','..', '/'}. */
    if (STREQ(name, "..") || STREQ(name, ".") || STREQ(name, ""))
	{ u.u_error = EINVAL; return; }

    for (;;) {
	Begin_VFS(dcp->c_fid.Volume, CODA_RMDIR);
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
	    { target_fso->print(logFile); CHOKE("vproc::rmdir: target is root"); }

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




void vproc::symlink(struct venus_cnode *dcp, char *contents,
		    struct coda_vattr *vap, char *name) 
{

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
	Begin_VFS(dcp->c_fid.Volume, CODA_SYMLINK);
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


void vproc::readlink(struct venus_cnode *cp, struct coda_string *string) 
{

	LOG(1, ("vproc::readlink: fid = %s\n", FID_(&cp->c_fid)));

    char *buf = string->cs_buf;
    int len = string->cs_maxlen;
    string->cs_len = 0;

    if (len > CODA_MAXPATHLEN)
	{ u.u_error = EINVAL; return; }

    fsobj *f = 0;

    for (;;) {
	Begin_VFS(cp->c_fid.Volume, CODA_READLINK);
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
	u.u_error = f->Readlink(buf, len, &string->cs_len, CRTORUID(u.u_cred));
	if (u.u_error) 
		goto FreeLocks;

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
	string->cs_len = 27;
    }
}


void vproc::fsync(struct venus_cnode *cp) 
{

    LOG(1, ("vproc::fsync: fid = %s\n", FID_(&cp->c_fid)));

    fsobj *f = 0;

    for (;;) {
	Begin_VFS(cp->c_fid.Volume, CODA_FSYNC);
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

void vproc::inactive(struct venus_cnode *cp) 
{

    LOG(1, ("vproc::inactive: fid = %s\n", FID_(&cp->c_fid)));

    u.u_error = EOPNOTSUPP;
}

