/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
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
#endif

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

#include <rpc2/rpc2.h>
#include <vice.h>
/* from libal */
#include <prs.h>

#ifdef __cplusplus
}
#endif

#include <coda_config.h>

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

#ifndef MIN
#define MIN(a,b)  ( ((a) < (b)) ? (a) : (b) )
#endif


/* Call with object write-locked. */
/* MUST NOT be called from within a transaction. */
int fsobj::Open(int writep, int execp, int truncp, venus_cnode *cp, uid_t uid) 
{
    LOG(10, ("fsobj::Open: (%s, %d, %d, %d), uid = %d\n",
	      comp, writep, execp, truncp, uid));

    if (cp) {
	    cp->c_device = 0;
	    cp->c_inode = 0;
	    cp->c_cfname[0] = '\0';
    }
    int code = 0;

    if (IsSymLink())
	return ELOOP;

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
	if ((code = SetAttr(&va, uid)) != 0)
	    goto Exit;
   }

    /* If object is directory make sure Unix-format contents are valid. */
    if (IsDir()) {
	if (data.dir->udcf == 0) {
	    Recov_BeginTrans();
            RVMLIB_REC_OBJECT(cf);
	    data.dir->udcf = &cf;
            data.dir->udcf->Create();
	    data.dir->udcfvalid = 0;
	    Recov_EndTrans(MAXFP);
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
#endif
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
            CacheFile *container = IsDir() ? data.dir->udcf : data.file;

	    cp->c_device = FSDB->device;
	    cp->c_inode = container->Inode();
	    strncpy(cp->c_cfname, container->Name(), CNODE_CFNAME_LEN-1);
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

/* Sync file to the servers */
/* Call with object write-locked. */
/* We CANNOT return ERETRY from this routine! */
int fsobj::Sync(uid_t uid) 
{
    LOG(10, ("fsobj::Sync: (%s), uid = %d\n", comp, uid));

    int code = 0;

    FSO_ASSERT(this, openers != 0 && WRITING(this));

    /* Don't do store on files that were deleted. */
    if (DYING(this)) return 0;

    PromoteLock();    

    /* We need to send the new mtime to Vice in the RPC call, so we get the
     * status off the disk.  If the file was freshly created and there were no
     * writes, then we should send the time of the mknod.  However, we don't
     * know the time of the mknod so we approximate it by the current time.
     * Note that we are fooled by the truncation and subsequent closing
     * (without further writing) of an existing file. */
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
        ((worker *)v)->StoreFid = fid;
    code = Store(NewLength, NewDate, uid);
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
    DemoteLock();    

    return(code);
}

/* Call with object write-locked. */
/* We CANNOT return ERETRY from this routine! */
void fsobj::Release(int writep, int execp) 
{
    LOG(10, ("fsobj::Release: (%s, %d, %d)\n", comp, writep, execp));

    FSO_ASSERT(this, openers != 0);

    openers--;

    if (execp) {
	if (!EXECUTING(this))
	    { print(logFile); CHOKE("fsobj::Release: !EXECUTING"); }
	Execers--;
    }

    if (writep) {
	PromoteLock();    

	if (!WRITING(this))
	    { print(logFile); CHOKE("fsobj::Release: !WRITING"); }
	Writers--;

        /* The object only gets removed from the owrite queue if we were the
         * last writer to close. */
	if (!WRITING(this)) {
            Recov_BeginTrans();
            /* Last writer: remove from owrite queue. */
            if (FSDB->owriteq->remove(&owrite_handle) != &owrite_handle)
            { print(logFile); CHOKE("fsobj::Release: owriteq remove"); }
            RVMLIB_REC_OBJECT(flags);
            flags.owrite = 0;

            /* Fudge size of files that were deleted while open. */
            if (DYING(this)) {
                LOG(0, ("fsobj::Release: last writer && dying (%s)\n",
                        FID_(&fid)));
                RVMLIB_REC_OBJECT(stat.Length);
                stat.Length = 0;	    /* Necessary for blocks maintenance! */
            }
            Recov_EndTrans(((EMULATING(this) || LOGGING(this)) ? DMFP : CMFP));
        }
    }

    FSO_RELE(this);	    /* Unpin object. */
    EnableReplacement();    /* Won't enable as long as object is in use */
    return;
}

int fsobj::Close(int writep, int execp, uid_t uid) 
{
    int code = 0;

    /* The object only is sent to the server(s) if we are the last writer
     * to close. */
    if (writep && Writers == 1)
        code = Sync(uid);
    Release(writep, execp);
    return code;
}


/* local-repair modification */
/* Need to incorporate System:Administrator knowledge here! -JJK */
int fsobj::Access(long rights, int modes, uid_t uid) 
{
    LOG(10, ("fsobj::Access : (%s, %d, %d), uid = %d\n",
	      comp, rights, modes, uid));

    int code = 0;

#define PRSFS_MUTATE (PRSFS_WRITE | PRSFS_DELETE | PRSFS_INSERT | PRSFS_LOCK)
    /* Disallow mutation of backup, rw-replica, and zombie volumes. */
    if (vol->IsBackup() || vol->IsReadWriteReplica()) {
	if (rights & PRSFS_MUTATE)
	    return(EROFS);
	/* But don't allow reading unless the acl allows us to. */
    }

    /* Disallow mutation of fake directories and mtpts.  Always permit
       reading of the same. */
    if (IsFake() || IsLocalObj()) {
	if (rights & PRSFS_MUTATE)
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
#if 0
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
#endif

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
	VenusFid parent_fid = pfid;
	if (FID_EQ(&NullFid, &parent_fid))
	    { print(logFile); CHOKE("fsobj::Access: pfid == Null"); }
	UnLock(level);
	//FSO_RELE(this); this was moved up here by someone to avoid problems
	//in FSDB->Get. But it's really bad, because we lose the guarantee that
	//the current object doesn't get swept from under us while we release
	//the lock. --JH

	/* Get the parent object, make the check, and put the parent. */
	fsobj *parent_fso = 0;
	code = FSDB->Get(&parent_fso, &parent_fid, uid, RC_STATUS);
	if (code == 0)
	    code = parent_fso->Access(rights, 0, uid);
	FSDB->Put(&parent_fso);

	/* Reacquire the child at the appropriate level and unpin it. */
	Lock(level);
	FSO_RELE(this);

	/* Check mode bits if necessary. */
	/* There should be a special case if this user is the creator.
	   This code used to have a test for `virginity', but only the kernel
	   can decide on this, asking Venus to do so leads to a race condition.
	   --JH
	*/
	if (!(modes & C_A_C_OK))
	    if (((modes & C_A_X_OK) && (stat.Mode & OWNEREXEC) == 0) ||
		((modes & C_A_W_OK) && (stat.Mode & OWNERWRITE) == 0) ||
		((modes & C_A_R_OK) && (stat.Mode & OWNERREAD) == 0))
		code = EACCES;

	return(code);
    }

    /* No sense checking when there is no hope of success. */
    if (rights == 0) return(EACCES);

    if (EMULATING(this) || (DIRTY(this) && LOGGING(this))) {
	/* Don't insist on validity when disconnected. */
	if ((code = CheckAcRights(uid, rights, 0)) != ENOENT)
	    return(code);
	return(EACCES);
    }

    /* XXX we might be 'RESOLVING(this)', what will happen then? -JH */
    FSO_ASSERT(this, (HOARDING(this) || (LOGGING(this) && !DIRTY(this))));

    /* !!! important point, this is where we map unauthenticated users to
     * System:AnyUser !!! */
    userent *ue = vol->realm->GetUser(uid);
    uid_t CheckVuid = ue->GetUid();
    PutUser(&ue);

    if ((code = CheckAcRights(CheckVuid, rights, 1)) != ENOENT)
	return(code);

    /* We must re-fetch status; rights will be returned as a side-effect. */
    /* Promote the lock level if necessary. */
    if (FETCHABLE(this)) {
	LockLevel level = (writers > 0 ? WR : RD);
	if (level == RD) PromoteLock();
	code = GetAttr(uid);
	if (level == RD) DemoteLock();
	if (code != 0) return(code);
    }

    if ((code = CheckAcRights(CheckVuid, rights, 1)) != ENOENT)
	return(code);

    return(EACCES);
}


/* local-repair modification */
/* inc_fid is an OUT parameter which allows caller to form "fake symlink" if it desires. */
/* Explicit parameter for TRAVERSE_MTPTS? -JJK */
int fsobj::Lookup(fsobj **target_fso_addr, VenusFid *inc_fid, char *name, uid_t uid, int flags)
{
    LOG(10, ("fsobj::Lookup: (%s/%s), uid = %d\n", comp, name, uid));

    /* We're screwed if (name == "." or ".."). -JJK */
    CODA_ASSERT(!STREQ(name, ".") && !STREQ(name, ".."));

    int code = 0;
    *target_fso_addr = 0;
    int	traverse_mtpts = (inc_fid != 0);	/* ? -JJK */
    Realm *realm = NULL;

    fsobj *target_fso = 0;
    VenusFid target_fid;

    /* Map name --> fid. */
    {
	/* Verify that we have lookup permission. */
	code = Access((long)PRSFS_LOOKUP, 0, uid);
	if (code) {
	    if (code == EINCONS && inc_fid != 0) *inc_fid = fid;
	    return(code);
	}

	/* Lookup the target object. */
	{
	    code = dir_Lookup(name, &target_fid, flags);

	    if (code) {
		if (vol->GetRealmId() != LocalRealm->Id() ||
		    vol->GetVolumeId() != FakeRootVolumeId)
		    return code;

		/* regular lookup failed, but if we are in the fake root
		 * volume, we can try to check for a new realm */

		// don't even bother to follow lookups of dangling symlinks
		if (name[0] == '#' || name[0] == '@')
		    return ENOENT;

		// Try to get and mount the realm.
		realm = REALMDB->GetRealm(name);
		target_fid = fid;
		target_fid.Vnode = 0xfffffffc;
		target_fid.Unique = realm->Id();
	    }
	}
    }

    /* Map fid --> fso. */
    {
	int status = RC_STATUS;
get_object:
	code = FSDB->Get(&target_fso, &target_fid, uid, status, name);

	if (realm) {
	    realm->PutRef();
	    realm = NULL;
	}

	if (code) {
	    if (code == EINCONS && inc_fid != 0) *inc_fid = target_fid;

	    /* If the getattr failed, the object might not exist on all
	     * servers. This is `fixed' by resolving the parent, but we just
	     * destroyed the object and RecResolve won't work. That is why we
	     * submit this directory for resolution. -JH */
	    if (code == ESYNRESOLVE && vol->IsReplicated())
		((repvol *)vol)->ResSubmit(&((VprocSelf())->u.u_resblk), &fid);

	    return(code);
	}

	/* Handle mount points. */
	if (traverse_mtpts) {
	    /* If the target is a covered mount point and it needs checked, uncover it (and unmount the root). */
	    if (target_fso->IsMtPt() && target_fso->flags.ckmtpt)
	    {
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
		    status |= RC_DATA;
		    goto get_object;
		}

		target_fso->PromoteLock();
		code = target_fso->TryToCover(inc_fid, uid);
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
int fsobj::Readlink(char *buf, unsigned long len, int *cc, uid_t uid)
{
    LOG(10, ("fsobj::Readlink : (%s, %x, %d, %x), uid = %d\n",
	      comp, buf, len, cc, uid));

    if (!HAVEALLDATA(this))
	{ print(logFile); CHOKE("fsobj::Readlink: called without data"); }
    if (!IsSymLink() && !IsMtPt())
	return(EINVAL);

    if (stat.Length > len - 1) {
	eprint("readlink: contents > bufsize");
	return(EINVAL);
    }

    /* Fill in the buffer. */
    char *ptr, *end, *out;
    ptr = data.symlink;
    end = ptr + stat.Length;
    out = buf;
    while (ptr != end) {
	char *subst, *c;
	unsigned int tmp_len;

	c = strchr(ptr, '@');
	if (!c || end < c + 4) c = end;

	/* copy everything up to this point */
	tmp_len = c - ptr;

	if (tmp_len >= len)
	    tmp_len = len-1;
	if (tmp_len) {
	    memcpy(out, ptr, tmp_len);
	    out += tmp_len;
	    len -= tmp_len;
	}

	ptr = c + 4;
	if (ptr > end) break;

	subst = c;
	tmp_len = 4;
	if (strncmp(&c[1], "cpu", 3) == 0) {
	     subst = CPUTYPE;
	     tmp_len = strlen(CPUTYPE);
	}
	else if (strncmp(&c[1], "sys", 3) == 0) {
	     subst = SYSTYPE;
	     tmp_len = strlen(SYSTYPE);
	}

	if (tmp_len >= len)
	    tmp_len = len-1;
	if (tmp_len) {
	    memcpy(out, subst, tmp_len);
	    out += tmp_len;
	    len -= tmp_len;
	}
    }
    *out = '\0';
    *cc = out - buf;

    LOG(100, ("fsobj::Readlink: contents = %s\n", buf));

    return(0);
}
