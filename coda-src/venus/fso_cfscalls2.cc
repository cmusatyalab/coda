/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

/*
 *
 *    CFS calls2.
 *
 */


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <stdio.h>
#include <sys/file.h>

#include "coda_string.h"
#include <unistd.h>
#include <stdlib.h>

#include <netdb.h>

#include <rpc2.h>
#include <vice.h>
/* from libal */
#include <prs.h>

#ifdef __cplusplus
}
#endif __cplusplus



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
#include "coda_expansion.h"

#ifndef MIN
#define MIN(a,b)  ( ((a) < (b)) ? (a) : (b) )
#endif


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

    /*  write lock the object if we might diddle it below.  Disabling
     * replacement and bumping reference counts are performed
     * elsewhere under read lock. */
    if (writep || truncp || 
        (IsDir() && (!data.dir->udcf || !data.dir->udcfvalid)))
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
    if (IsDir()) {
	if (data.dir->udcf == 0) {
	    data.dir->udcf = &cf;
	    FSO_ASSERT(this, data.dir->udcfvalid == 0);
	    FSO_ASSERT(this, data.dir->udcf->Length() == 0);
	}

	/* Recompute udir contents if necessary. */
	if (!data.dir->udcfvalid) {
	    LOG(100, ("fsobj::Open: recomputing udir\n"));


	    /* XXX I reactivated this code. It seems a good idea
	       pjb 9/21/98 */
#if 0
	    /* Reset a cache entry that others are still reading, but
               that we must now change. */
	    if (openers > 1) {
		LOG(100, ("fsobj::Open: udir in use, detaching for current users\n"));

		/* Unlink the old inode.  Kernel will keep it around
                   for current openers. */
		::unlink(data.dir->udcf->name);

		/* Get a fresh inode, initialize it, and plug it into
                   the fsobj. */
		int tfd = ::open(data.dir->udcf->name, O_BINARY | O_RDWR | O_CREAT, V_MODE);
		if (tfd < 0) CHOKE("fsobj::Open: open");
#if !defined(DJGPP) && !defined(__CYGWIN32__)
		if (::fchmod(tfd, V_MODE) < 0)
		    CHOKE("fsobj::Open: fchmod");
		if (::fchown(tfd, (uid_t)V_UID, (gid_t)V_GID) < 0)
		    CHOKE("fsobj::Open: fchown");
#endif
		struct stat tstat;
		if (::fstat(tfd, &tstat) < 0) CHOKE("fsobj::Open: fstat");
		if (::close(tfd) < 0) CHOKE("fsobj::Open: close");
		data.dir->udcf->inode = tstat.st_ino;
	    }
#endif 0
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
			{ print(logFile); CHOKE("fsobj::Open: owriteq remove"); }
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
int fsobj::Close(int writep, int execp, vuid_t vuid) 
{
    LOG(10, ("fsobj::Close: (%s, %d, %d), uid = %d\n",
	      comp, writep, execp, vuid));

    int code = 0;

    /* Update openers state; send object to server(s) if necessary. */
    if (openers < 1)
	{ print(logFile); CHOKE("fsobj::Close: openers < 1"); }
    openers--;
    if (writep) {
	PromoteLock();    

	if (!WRITING(this))
	    { print(logFile); CHOKE("fsobj::Close: !WRITING"); }
	Writers--;

	/* The object only gets sent to the server(s) if we are the
           last writer to close. */
	if (WRITING(this)) {
	    FSO_RELE(this);		    /* Unpin object. */
	    return(0);
	}

	Recov_BeginTrans();
	/* Last writer: remove from owrite queue. */
	if (FSDB->owriteq->remove(&owrite_handle) != &owrite_handle)
		{ print(logFile); CHOKE("fsobj::Close: owriteq remove"); }
	RVMLIB_REC_OBJECT(flags);
	flags.owrite = 0;

	/* Don't do store on files that were deleted while open. */
	if (DYING(this)) {
		LOG(0, ("fsobj::Close: last writer && dying (%x.%x.%x)\n",
			fid.Volume, fid.Vnode, fid.Unique));
		stat.Length = 0;	    /* Necessary for blocks maintenance! */
	}
	Recov_EndTrans(((EMULATING(this) || LOGGING(this)) ? DMFP : CMFP));
	if (DYING(this)) {
	    FSO_RELE(this);		    /* Unpin object. */
	    return(0);
	}

	/* We need to send the new mtime to Vice in the RPC call,
	   so we get the status off */
	/* the disk.  If the file was freshly created and there
	   were no writes, then we should */
	/* send the time of the mknod.  However, we don't know the
	   time of the mknod so we */
	/* approximate it by the current time.  Note that we are
	   fooled by the truncation and */
	/* subsequent closing (without further writing) of an
	   existing file. */
	long NewLength;
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
		case ERETRY: print(logFile); CHOKE("fsobj::Close: Store returns ERETRY");
		default: eprint("unknown store error %d", code); break;
	    }
	}
    }
    if (execp) {
	if (!EXECUTING(this))
	    { print(logFile); CHOKE("fsobj::Close: !EXECUTING"); }
	Execers--;
    }

    FSO_RELE(this);		    /* Unpin object. */
    EnableReplacement();
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
/*
	static char fileModeMap[8] = {
	    PRSFS_INSERT | PRSFS_DELETE	| PRSFS_ADMINISTER,				* --- *
	    PRSFS_INSERT | PRSFS_DELETE	| PRSFS_ADMINISTER,				* --x *
	    PRSFS_INSERT | PRSFS_DELETE	| PRSFS_ADMINISTER | PRSFS_WRITE,		* -w- *
	    PRSFS_INSERT | PRSFS_DELETE	| PRSFS_ADMINISTER | PRSFS_WRITE,		* -wx *
	    PRSFS_INSERT | PRSFS_DELETE	| PRSFS_ADMINISTER | PRSFS_READ,		* r-- *
	    PRSFS_INSERT | PRSFS_DELETE	| PRSFS_ADMINISTER | PRSFS_READ,		* r-x *
	    PRSFS_INSERT | PRSFS_DELETE	| PRSFS_ADMINISTER | PRSFS_READ | PRSFS_WRITE,	* rw- *
	    PRSFS_INSERT | PRSFS_DELETE	| PRSFS_ADMINISTER | PRSFS_READ | PRSFS_WRITE	* rwx *
	};
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
	    { print(logFile); CHOKE("fsobj::Access: pfid == Null"); }
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


/* local-repair modification */
/* inc_fid is an OUT parameter which allows caller to form "fake symlink" if it desires. */
/* Explicit parameter for TRAVERSE_MTPTS? -JJK */
int fsobj::Lookup(fsobj **target_fso_addr, ViceFid *inc_fid, char *name, vuid_t vuid, int flags) {
    LOG(10, ("fsobj::Lookup: (%s/%s), uid = %d\n",
	      comp, name, vuid));
    int  len;
    char *subst = NULL, expand[CODA_MAXNAMLEN];

    /* We're screwed if (name == "."). -JJK */
    if (STREQ(name, "."))
	{ print(logFile); CHOKE("fsobj::Lookup: name = ."); }

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

	/* Check for @cpu/@sys expansion. */
	len = strlen(name);
	if (len >= 4 && name[len-4] == '@')
	{
	    if      (strcmp(&name[len-3], "cpu") == 0)
		subst = CPUTYPE;
	    else if (strcmp(&name[len-3], "sys") == 0)
		subst = SYSTYPE;

	    /* Embed the processor/system name for @cpu/@sys expansion. */
	    if (subst && (len + strlen(subst)) < CODA_MAXNAMLEN)
	    {
		memset(expand, 0, CODA_MAXNAMLEN);
		strncpy(expand, name, len-4);
		strcpy(&expand[len-4], subst);
		name = expand;
	    }
	}

	/* Lookup the target object. */

	if (STREQ(name, "..")) {
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
			CHOKE("fsobj::Lookup: pfid = NULL");
		    }
		}
		target_fid = pfid;
	    }
	}
	else {
	    code = dir_Lookup(name, &target_fid, flags);
	    if (code) return(code);
	}
    }

    /* Map fid --> fso. */
    {
	code = FSDB->Get(&target_fso, &target_fid, vuid, RC_STATUS, name);
	if (code) {
	    if (code == EINCONS && inc_fid != 0) *inc_fid = target_fid;

	    /* If the getattr failed, the object might not exist on all server.
	     * As this is `fixed' by resolving the parent, and we just
	     * destroyed the object RecResolve won't work. That is why we
	     * submit this directory for resolution as well. -JH */
	    if (code == ESYNRESOLVE)
		vol->ResSubmit(&((VprocSelf())->u.u_resblk), &fid);

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
		if (!HAVEALLDATA(target_fso)) {
		    FSDB->Put(&target_fso);
		    code = FSDB->Get(&target_fso, &target_fid, vuid, RC_DATA, name);
		    if (code) {
			if (code == EINCONS) *inc_fid = target_fid;
			return(code);
		    }
		}

		target_fso->PromoteLock();
		code = target_fso->TryToCover(inc_fid, vuid);
		if (code == EINCONS || code == ERETRY) {
		    FSDB->Put(&target_fso);
		    return(code);
		}
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

/* Call with the link contents fetched already. */
/* Call with object read-locked. */
int fsobj::Readlink(char *buf, int len, int *cc, vuid_t vuid) {
    LOG(10, ("fsobj::Readlink : (%s, %x, %d, %x), uid = %d\n",
	      comp, buf, len, cc, vuid));

    if (!HAVEALLDATA(this))
	{ print(logFile); CHOKE("fsobj::Readlink: called without data"); }
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
