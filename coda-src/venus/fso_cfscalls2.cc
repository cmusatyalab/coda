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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/venus/fso_cfscalls2.cc,v 4.13 1998/08/27 19:40:49 braam Exp $";
#endif /*_BLURB_*/







/*
 *
 *    CFS calls2.
 *
 */


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <stdio.h>
#ifdef __BSD44__
#include <sys/dir.h>
#endif
#include <sys/file.h>
#ifndef __FreeBSD__
// Since vproc.h knows struct uio.
#include <sys/uio.h>
#endif
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include <rpc2.h>

#ifdef __cplusplus
}
#endif __cplusplus

/* interfaces */
#include <vice.h>

/* from libal */
#include <prs_fs.h>

/* from venus */
#include "comm.h"
#include "fso.h"
#include "local.h"
#include "mariner.h"
#include "user.h"
#include "venuscb.h"
#include "venus.private.h"
#include "venusrecov.h"
#include "venusvol.h"
#include "vproc.h"
#include "worker.h"


/* Call with object write-locked. */
int fsobj::Open(int writep, int execp, int truncp, venus_cnode *cp, vuid_t vuid) 
{
    LOG(10, ("fsobj::Open: (%s, %d, %d, %d), uid = %d\n",
	      comp, writep, execp, truncp, vuid));

    if (cp) {
	    cp->c_device = 0;
	    cp->c_inode = 0;
	    cp->c_cfname[0] = '\0';
    }
    int code = 0;

    /* 
     * write lock the object if we might diddle it below.  Disabling replacement
     * and bumping reference counts are performed elsewhere under read lock.  
     * The simulator makes its own assumptions, leave alone in that case.
     */
    if (!Simulating && (writep || truncp || 
        (IsDir() && (!data.dir->udcf || !data.dir->udcfvalid))))
        PromoteLock();

    /* Update usage counts here. */
    DisableReplacement();
    FSO_HOLD(this);			/* Pin object until close arrives. */
    openers++;
    if (writep) {
	Writers++;
	if (!flags.owrite) {
	    Recov_BeginTrans();
	    FSDB->FreeBlocks((int) BLOCKS(this));
	    FSDB->owriteq->append(&owrite_handle);
	    RVMLIB_REC_OBJECT(flags);
	    flags.owrite = 1;
	    Recov_EndTrans(((EMULATING(this) || LOGGING(this)) ? DMFP : CMFP));
	}
    }
    if (execp)
	Execers++;

    /* Do truncate if necessary. */
    if (truncp && writep) {	/* truncp is acted upon only if writep */
	struct coda_vattr va; va_init(&va);
	va.va_size = 0;
	if ((code = SetAttr(&va, vuid)) != 0)
	    goto Exit;
    }

    /* Read/Write Sharing Stat Collection */
    if (EMULATING(this) && !flags.discread) {
	Recov_BeginTrans();
	RVMLIB_REC_OBJECT(flags);
	flags.discread = 1;
	Recov_EndTrans(MAXFP);
      } 

    /* If object is directory make sure Unix-format contents are valid. */
    if (!Simulating && IsDir()) {
	if (data.dir->udcf == 0) {
	    data.dir->udcf = &cf;
	    FSO_ASSERT(this, data.dir->udcfvalid == 0);
	    FSO_ASSERT(this, data.dir->udcf->Length() == 0);
	}

	/* Recompute udir contents if necessary. */
	if (!data.dir->udcfvalid) {
	    LOG(100, ("fsobj::Open: recomputing udir\n"));

	    /* XXXX WHO put this between "#if 0".  If this code is 
	       ever activated again then the open needs O_BINARY for DJGPP */

#if	0
	    /* Reset a cache entry that others are still reading, but that we must now change. */
	    if (openers > 1) {
		LOG(100, ("fsobj::Open: udir in use, detaching for current users\n"));

		/* Unlink the old inode.  Kernel will keep it around for current openers. */
		::unlink(data.dir->udcf->name);

		/* Get a fresh inode, initialize it, and plug it into the fsobj. */
		int tfd = ::open(data.dir->udcf->name, O_RDWR | O_CREAT, V_MODE);
		if (tfd < 0) Choke("fsobj::Open: open");
		if (::fchmod(tfd, V_MODE) < 0)
		    Choke("fsobj::Open: fchmod");
		if (::fchown(tfd, (uid_t)V_UID, (gid_t)V_GID) < 0)
		    Choke("fsobj::Open: fchown");
		struct stat tstat;
		if (::fstat(tfd, &tstat) < 0) Choke("fsobj::Open: fstat");
		if (::close(tfd) < 0) Choke("fsobj::Open: close");
		data.dir->udcf->inode = tstat.st_ino;
	    }
#endif	0

	    /* (Re)Build the Unix-format directory. */
	    dir_Rebuild();
	    struct stat tstat;
	    data.dir->udcf->Stat(&tstat);
	    FSDB->ChangeDiskUsage((int) NBLOCKS(tstat.st_size) - NBLOCKS(data.dir->udcf->Length()));
	    Recov_BeginTrans();
	    data.dir->udcf->SetLength((int) tstat.st_size);
	    Recov_EndTrans(MAXFP);
	}
    }

    /* <device, inode> handle is OUT parameter. */
    if (cp) {
	    cp->c_device = FSDB->device;
	    if ( IsDir() ) {
		    strncpy(cp->c_cfname, data.dir->udcf->name, 8);
		    cp->c_inode= data.dir->udcf->Inode();
	    } else {
	            cp->c_inode = data.file->Inode();
		    strncpy(cp->c_cfname, data.file->name, 8);
	    }
    }

Exit:
    if (code != 0) {
	/* Back out transaction if truncate failed! */
	openers--;
	if (writep) {
	    Writers--;
	    if (!WRITING(this)) {
		Recov_BeginTrans();
		if (FSDB->owriteq->remove(&owrite_handle) != &owrite_handle)
			{ print(logFile); Choke("fsobj::Open: owriteq remove"); }
		RVMLIB_REC_OBJECT(flags);
		flags.owrite = 0;
		FSDB->ChangeDiskUsage((int) BLOCKS(this));
		Recov_EndTrans(0);
	    }
	}
	if (execp)
	    Execers--;
	FSO_RELE(this);
	EnableReplacement();
    }
    return(code);
}


/* Call with object write-locked. */
/* We CANNOT return ERETRY from this routine! */
int fsobj::Close(int writep, int execp, vuid_t vuid) {
    LOG(10, ("fsobj::Close: (%s, %d, %d), uid = %d\n",
	      comp, writep, execp, vuid));

    int code = 0;

    /* Update openers state; send object to server(s) if necessary. */
    if (openers < 1)
	{ print(logFile); Choke("fsobj::Close: openers < 1"); }
    openers--;
    if (writep) {
        if (!Simulating) 
            PromoteLock();    

	if (!WRITING(this))
	    { print(logFile); Choke("fsobj::Close: !WRITING"); }
	Writers--;

	/* The object only gets sent to the server(s) if we are the last writer to close. */
	if (WRITING(this)) {
	    FSO_RELE(this);		    /* Unpin object. */
	    return(0);
	}

	Recov_BeginTrans();
	/* Last writer: remove from owrite queue. */
	if (FSDB->owriteq->remove(&owrite_handle) != &owrite_handle)
		{ print(logFile); Choke("fsobj::Close: owriteq remove"); }
	RVMLIB_REC_OBJECT(flags);
	flags.owrite = 0;

	/* Don't do store on files that were deleted while open. */
	if (DYING(this)) {
		LOG(1, ("fsobj::Close: last writer && dying (%x.%x.%x)\n",
			fid.Volume, fid.Vnode, fid.Unique));
		stat.Length = 0;	    /* Necessary for blocks maintenance! */
	}
	Recov_EndTrans(((EMULATING(this) || LOGGING(this)) ? DMFP : CMFP));
	if (DYING(this)) {
	    FSO_RELE(this);		    /* Unpin object. */
	    return(0);
	}

	if (!Simulating) {
	    /* We need to send the new mtime to Vice in the RPC call, so we get the status off */
	    /* the disk.  If the file was freshly created and there were no writes, then we should */
	    /* send the time of the mknod.  However, we don't know the time of the mknod so we */
	    /* approximate it by the current time.  Note that we are fooled by the truncation and */
	    /* subsequent closing (without further writing) of an existing file. */
	    unsigned long NewLength;
	    Date_t NewDate;
	    {
		struct stat tstat;
		data.file->Stat(&tstat);
		if (tstat.st_size == 0) tstat.st_mtime = Vtime();

		NewLength = tstat.st_size;
		NewDate = tstat.st_mtime;
	    }
	    int old_blocks = (int) BLOCKS(this);
	    int new_blocks = (int) NBLOCKS(NewLength);
	    UpdateCacheStats(&FSDB->FileDataStats, WRITE, MIN(old_blocks, new_blocks));
	    if (NewLength < stat.Length)
		UpdateCacheStats(&FSDB->FileDataStats, REMOVE, (old_blocks - new_blocks));
	    else if (NewLength > stat.Length)
		UpdateCacheStats(&FSDB->FileDataStats, CREATE, (new_blocks - old_blocks));
	    FSDB->ChangeDiskUsage((int) NBLOCKS(NewLength));
	    Recov_BeginTrans();
	    data.file->SetLength((unsigned int) NewLength);
	    Recov_EndTrans(MAXFP);

	    /* Attempt the Store. */
	    vproc *v = VprocSelf();
	    if (v->type == VPT_Worker)
		if (flags.era) ((worker *)v)->StoreFid = fid;
	    code = Store(NewLength, NewDate, vuid);
	    if (v->type == VPT_Worker)
		((worker *)v)->StoreFid = NullFid;
	    if (code) {
		eprint("failed to store %s on server", comp);
		switch (code) {
		    case ENOSPC: eprint("server partition full"); break;
		    case EDQUOT: eprint("over your disk quota"); break;
		    case EACCES: eprint("protection failure"); break;
		    case ERETRY: print(logFile); Choke("fsobj::Close: Store returns ERETRY");
		    default: eprint("unknown store error %d", code); break;
		}
	    }
	}
    }
    if (execp) {
	if (!EXECUTING(this))
	    { print(logFile); Choke("fsobj::Close: !EXECUTING"); }
	Execers--;
    }

    FSO_RELE(this);		    /* Unpin object. */
    EnableReplacement();
    return(code);
}



/* Call with file contents fetched already. */
/* Call with object write-locked. */
     int fsobj::RdWr(char *buf, enum uio_rw rwflag, int offset, int len, int *cc, vuid_t vuid) 
{

    Choke("We think this is deprecated.");
    LOG(10, ("fsobj::RdWr: (%s, %d, %d, %d), uid = %d\n",
	      comp, rwflag, offset, len, vuid));

    if (!HAVEDATA(this))
	{ print(logFile); Choke("fsobj::RdWr: called without data"); }
    if (!IsFile())
	return(EINVAL);
/*
    if (openers < 1)
	{ print(logFile); Choke("fsobj::RdWr: openers < 1"); }
    if (rwflag == UIO_WRITE && !WRITING(this))
	{ print(logFile); Choke("fsobj::RdWr: write && !WRITING"); }
*/

    int code = 0;

    if (rwflag == UIO_WRITE) 
        PromoteLock();

    /* Open the file, seek to the requested offset, rdwr the requested number of bytes, and close the file. */
    int mode = (rwflag == UIO_READ ? O_RDONLY : O_WRONLY);
    int tfd = ::open(data.file->Name(), mode, 0);
    if (tfd < 0) Choke("fsobj::RdWr: open");
    if (::lseek(tfd, offset, L_SET) < 0) Choke("fsobj::RdWr: lseek");
    if (rwflag == UIO_READ) {
	*cc = ::read(tfd, buf, len);
	if (*cc < 0) Choke("fsobj::RdWr: read");
    }
    else {
	*cc = ::write(tfd, buf, len);
	if (*cc != len) Choke("fsobj::RdWr: write");
    }
    if (::close(tfd) < 0) Choke("fsobj::RdWr: close");

    return(code);
}


/* local-repair modification */
/* Need to incorporate System:Administrator knowledge here! -JJK */
int fsobj::Access(long rights, int modes, vuid_t vuid) 
{
    LOG(10, ("fsobj::Access : (%s, %d, %d), uid = %d\n",
	      comp, rights, modes, vuid));

    int code = 0;

    /* Disallow mutation of backup, read-only, rw-replica, and zombie volumes. */
    if ((flags.backup || flags.readonly || flags.rwreplica) &&
	 (rights & (long)(PRSFS_WRITE | PRSFS_DELETE | PRSFS_INSERT | PRSFS_LOCK)))
	return(EROFS);

    /* Disallow mutation of fake directories and mtpts.  Always permit
       reading of the same. */
    if (IsFake() || IsLocalObj()) {
	if (rights & (long)(PRSFS_WRITE | PRSFS_DELETE | PRSFS_INSERT | PRSFS_LOCK))
	    return(EROFS);

	return(0);
    }

    /* If the object is not a directory, the access check must be made
       with respect to its parent. */
    /* In that case we release the non-directory object during the
       check, and reacquire it on exit. */
    /* N.B.  The only time we should be called on a mount point is via
       "fs lsmount"! -JJK */
    if (!IsDir() || IsMtPt()) {
	/* Pin the object and record the lock level. */
	FSO_HOLD(this);
	LockLevel level = (writers > 0 ? WR : RD);

	/* Refine the permissions according to the file mode bits. */
	static char fileModeMap[8] = {
	    PRSFS_INSERT | PRSFS_DELETE	| PRSFS_ADMINISTER,				/* --- */
	    PRSFS_INSERT | PRSFS_DELETE	| PRSFS_ADMINISTER,				/* --x */
	    PRSFS_INSERT | PRSFS_DELETE	| PRSFS_ADMINISTER | PRSFS_WRITE,		/* -w- */
	    PRSFS_INSERT | PRSFS_DELETE	| PRSFS_ADMINISTER | PRSFS_WRITE,		/* -wx */
	    PRSFS_INSERT | PRSFS_DELETE	| PRSFS_ADMINISTER | PRSFS_READ,		/* r-- */
	    PRSFS_INSERT | PRSFS_DELETE	| PRSFS_ADMINISTER | PRSFS_READ,		/* r-x */
	    PRSFS_INSERT | PRSFS_DELETE	| PRSFS_ADMINISTER | PRSFS_READ | PRSFS_WRITE,	/* rw- */
	    PRSFS_INSERT | PRSFS_DELETE	| PRSFS_ADMINISTER | PRSFS_READ | PRSFS_WRITE	/* rwx */
	};
/*
	rights &= fileModeMap[(stat.Mode & OWNERBITS) >> 6];
*/

	/* check if the object is GlobalRootObj for a local-fake tree */
	if (LRDB->RFM_IsGlobalRoot(&fid)) {
	    /* 
	     * we can safely retrun 0 here. because if the parent's acl is updated
	     * during disconnection, then "this" object won't become a global root node.
	     */
	    FSO_RELE(this);
	    return(0);
	}

	/* Record the parent fid and release the object. */
	ViceFid parent_fid = pfid;
	if (FID_EQ(&NullFid, &parent_fid))
	    { print(logFile); Choke("fsobj::Access: pfid == Null"); }
	UnLock(level);
	FSO_RELE(this);

	/* Get the parent object, make the check, and put the parent. */
	fsobj *parent_fso = 0;
	code = FSDB->Get(&parent_fso, &parent_fid, vuid, RC_STATUS);
	if (code == 0)
	    code = parent_fso->Access(rights, 0, vuid);
	FSDB->Put(&parent_fso);

	/* Reacquire the child at the appropriate level and unpin it. */
	Lock(level);
	/* FSO_RELE(this); // Used to be here.  Moved earlier to avoid problems in fsdb::Get */

	/* Check mode bits if necessary. */
	/* Special case if file is "virgin" and this user is the creator. */
	/*
	  This is wrong.  Only the kernel can decide virginity, asking
	  Venus to do this leads to a race condition
	if (code == 0 && !(IsVirgin() && stat.Owner == vuid))
	*/
	if (!(modes & C_A_C_OK))
	    if (((modes & C_A_X_OK) != 0 && (stat.Mode & OWNEREXEC) == 0) ||
		((modes & C_A_W_OK) != 0 && (stat.Mode & OWNERWRITE) == 0) ||
		((modes & C_A_R_OK) != 0 && (stat.Mode & OWNERREAD) == 0))
		code = EACCES;

	return(code);
    }

    /* No sense checking when there is no hope of success. */
    if (rights == 0) return(EACCES);

    if (EMULATING(this) || (DIRTY(this) && LOGGING(this))) {
	/* Don't insist on validity when disconnected. */
	if ((code = CheckAcRights(vuid, rights, 0)) != ENOENT)
	    return(code);
	if ((code = CheckAcRights(ALL_UIDS, rights, 0)) != ENOENT)
	    return(code);
    }
    else {
	FSO_ASSERT(this, (HOARDING(this) || (LOGGING(this) && !DIRTY(this))));

	userent *ue;
	GetUser(&ue, vuid);
	int tokensvalid = ue->TokensValid();
	PutUser(&ue);
	vuid_t CheckVuid = (tokensvalid ? vuid : ALL_UIDS);

	if ((code = CheckAcRights(CheckVuid, rights, 1)) != ENOENT)
	    return(code);

	/* We must re-fetch status; rights will be returned as a side-effect. */
	/* Promote the lock level if necessary. */
	if (FETCHABLE(this)) {
	    LockLevel level = (writers > 0 ? WR : RD);
	    if (level == RD) PromoteLock();
	    code = GetAttr(vuid);
	    if (level == RD) DemoteLock();
	    if (code != 0) return(code);
	}

	if ((code = CheckAcRights(CheckVuid, rights, 1)) != ENOENT)
	    return(code);
    }

    return(EACCES);
}


/* Embed the processor/system name in the code for @cputype/@sys expansion. */

#ifdef __NetBSD__
#ifdef i386
static char cputype [] = "i386";
static char systype [] = "i386_nbsd1";
#endif /* i386 */
#ifdef arm32
static char cputype [] = "arm32";
static char systype [] = "arm32_nbsd1";
#endif
#endif /* __NetBSD__ */

#ifdef __FreeBSD__
#ifdef i386
static char cputype [] = "i386";
static char systype [] = "i386_fbsd2";
#endif /* i386 */
#endif /* __FreeBSD__ */

#ifdef __linux__
#ifdef i386
static char cputype [] = "i386";
static char systype [] = "i386_linux";
#endif /* i386 */
#ifdef sparc
static char cputype [] = "sparc";
static char systype [] = "sparc_linux";
#endif
#ifdef __alpha__
static char cputype [] = "alpha";
static char systype [] = "alpha_linux";
#endif
#endif /* __linux__ */

#ifdef __CYGWIN32__
#ifdef i386
static char cputype [] = "i386";
static char systype [] = "i386_win32";
#endif 
#endif 

#ifdef DJGPP
static char cputype [] = "i386";
static char systype [] = "i386_win32";
#endif 

/* local-repair modification */
/* inc_fid is an OUT parameter which allows caller to form "fake symlink" if it desires. */
/* Explicit parameter for TRAVERSE_MTPTS? -JJK */
int fsobj::Lookup(fsobj **target_fso_addr, ViceFid *inc_fid, char *name, vuid_t vuid) {
    LOG(10, ("fsobj::Lookup: (%s/%s), uid = %d\n",
	      comp, name, vuid));

    /* We're screwed if (name == "."). -JJK */
    if (STREQ(name, "."))
	{ print(logFile); Choke("fsobj::Lookup: name = ."); }

    int code = 0;
    *target_fso_addr = 0;
    int	traverse_mtpts = (inc_fid != 0);	/* ? -JJK */

    fsobj *target_fso = 0;
    ViceFid target_fid;

    /* Map name --> fid. */
    {
	/* Verify that we have lookup permission. */
	code = Access((long)PRSFS_LOOKUP, 0, vuid);
	if (code) {
	    if (code == EINCONS && inc_fid != 0) *inc_fid = fid;
	    return(code);
	}

	/* Check for @cputype/@sys expansion. */
	if (STREQ(name, "@cputype"))
	    name = cputype;
	else if (STREQ(name, "@sys"))
	    name = systype;

	/* Lookup the target object. */
#if	0
	{
	    /* XXX Band-Aid here! -JJK */
	    /* We're getting failures in which the data has disappeared by the time the dir_Lookup is */
	    /* called below.  The eventual fix is probably to have ALL of the dir_ routines return ERETRY */
	    /* if there is no data.  For now, I'll just fix this one place, since it's the most common! */
	    if (!HAVEDATA(this)) {
		LOG(0, ("fsobj::Lookup: (%s, %x.%x.%x), data is gone!\n",
			comp, fid.Volume, fid.Vnode, fid.Unique));
		return(ERETRY);
	    }

	    /* I think this was fixed by checking for BUSY(f) in fsdb::CallBackBreak()! -JJK */
	}
#endif	0
	/* Haven't we already choked if STREQ(name, ".") ??? */
	if (STREQ(name, ".")) 
	    target_fid = fid;
	else if (STREQ(name, "..")) {
	    if (IsRoot()) {
		/* Back up over a mount point. */
		LOG(100, ("fsobj::Lookup: backing up over a mount point\n"));

		/* Next fid is the parent of the mount point. */
		if (!traverse_mtpts || u.mtpoint == 0 || FID_EQ(&u.mtpoint->pfid, &NullFid))
		    return(ENOENT);

		target_fid = u.mtpoint->pfid;
	    }
	    else {		
		if (FID_EQ(&NullFid, &pfid)) {
		    if (LRDB->RFM_IsGlobalRoot(&fid) || LRDB->RFM_IsLocalRoot(&fid)) {
			return EACCES;
		    } else {
			print(logFile); 
			Choke("fsobj::Lookup: pfid = NULL");
		    }
		}
		target_fid = pfid;
	    }
	}
	else {
	    code = dir_Lookup(name, &target_fid);
	    if (code) return(code);
	}
    }

    /* Map fid --> fso. */
    {
	code = FSDB->Get(&target_fso, &target_fid, vuid, RC_STATUS, name);
	if (code) {
	    if (code == EINCONS && inc_fid != 0) *inc_fid = target_fid;
	    return(code);
	}

	/* Handle mount points. */
	if (traverse_mtpts) {
	    /* If the target is a covered mount point and it needs checked, uncover it (and unmount the root). */
	    if (target_fso->IsMtPt() && target_fso->flags.ckmtpt) {
		fsobj *root_fso = target_fso->u.root;
		FSO_ASSERT(target_fso, (root_fso != 0 && root_fso->u.mtpoint == target_fso));
		Recov_BeginTrans();
		root_fso->UnmountRoot();
		target_fso->UncoverMtPt();
		Recov_EndTrans(MAXFP);
		target_fso->flags.ckmtpt = 0;
	    }

	    /* If the target is an uncovered mount point, try to cover it. */
	    if (target_fso->IsMTLink()) {
		/* We must have the data here. */
		if (!HAVEDATA(target_fso)) {
		    FSDB->Put(&target_fso);
		    code = FSDB->Get(&target_fso, &target_fid, vuid, RC_DATA, name);
		    if (code) {
			if (code == EINCONS && inc_fid != 0) *inc_fid = target_fid;
			return(code);
		    }
		}

		target_fso->PromoteLock();
		code = target_fso->TryToCover(inc_fid, vuid);
		if (code == EINCONS && inc_fid != 0)
		    { FSDB->Put(&target_fso); return(EINCONS); }
		if (code == ERETRY)
		    { FSDB->Put(&target_fso); return(ERETRY); }
		code = 0;
		target_fso->DemoteLock();
	    }

	    /* If the target is a covered mount point, cross it. */
	    if (target_fso->IsMtPt()) {
		LOG(100, ("fsobj::Lookup: crossing mount point\n"));

		/* Get the volume root, and release the mount point. */
		fsobj *root_fso = target_fso->u.root;
		root_fso->Lock(RD);
		FSDB->Put(&target_fso);
		target_fso = root_fso;
	    }
	}
    }

    *target_fso_addr = target_fso;
    return(0);
}


/* Call with directory contents fetched already. */
/* Call with object read-locked. */
int fsobj::Readdir(char *buf, int offset, int len, int *cc, vuid_t vuid) {

    Choke("fsobj::Readdir is deprecated.");
    LOG(10, ("fsobj::Readdir : (%s, %d, %d), uid = %d\n",
	      comp, offset, len, vuid));

    if (!HAVEDATA(this))
	{ print(logFile); Choke("fsobj::Readdir: called without data"); }
    if (!IsDir())
	return(EINVAL);

    int code = 0;

    /* Open the Vice file. */
    PromoteLock();
    code = Open(0, 0, 0, 0, vuid);
    if (code)
	{ DemoteLock(); return(code); }

    /* Open the udir file, seek to the requested offset, and read the requested number of bytes. */
    int tfd = ::open(data.dir->udcf->Name(), O_RDONLY, 0);
    if (tfd < 0) Choke("fsobj::Readdir: open");
    if (::lseek(tfd, offset, L_SET) < 0) Choke("fsobj::Readdir: lseek");
    *cc = ::read(tfd, buf, len);
    if (*cc < 0) Choke("fsobj::Readdir: read");
    if (::close(tfd) < 0) Choke("fsobj::Readdir: close");

    /* Close the Vice file. */
    code = Close(0, 0, vuid);

    DemoteLock();

    if (LogLevel >= 1000) {
	for (int pos = 0; pos < *cc;) {
	    struct venus_dirent *dp = (struct venus_dirent *)&(buf[pos]);
	    if (*cc - pos < DIRSIZ(dp))
		{ print(logFile); Choke("fsobj::Readdir: dir entry too small"); }

#ifndef DJGPP
	    if (dp->d_fileno == 0) break;
#endif
	    LOG(1000, ("\t<%d, %d, %d, %s>\n",
                       dp->d_fileno, dp->d_reclen, dp->d_namlen, dp->d_name));
	    pos += (int) DIRSIZ(dp);
	}
    }

    return(code);
}


/* Call with the link contents fetched already. */
/* Call with object read-locked. */
int fsobj::Readlink(char *buf, int len, int *cc, vuid_t vuid) {
    LOG(10, ("fsobj::Readlink : (%s, %x, %d, %x), uid = %d\n",
	      comp, buf, len, cc, vuid));

    if (!HAVEDATA(this))
	{ print(logFile); Choke("fsobj::Readlink: called without data"); }
    if (!IsSymLink() && !IsMtPt())
	return(EINVAL);

    if (stat.Length > len - 1) {
	eprint("readlink: contents > bufsize");
	return(EINVAL);
    }

    /* Fill in the buffer. */
    bcopy(data.symlink, buf, (int) stat.Length);
    *cc = (int) stat.Length;
    (buf)[*cc] = 0;
    LOG(100, ("fsobj::Readlink: contents = %s\n", buf));

    return(0);
}
