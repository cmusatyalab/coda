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
              Copyright (c) 2002-2003 Intel Corporation

#*/

/*
 *
 *    Implementation of the Venus Pioctl interface.
 *
 *    Pioctls find their way through the VFS interface as vproc::ioctl calls.
 *    The code to handle pioctls has been collected here for convenience.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <errno.h>
#include "coda_string.h"
#include <unistd.h>
#include <stdlib.h>

#include <auth2.h>
#include <prs.h>
#include <rpc2/rpc2.h>
#include <rpc2/fail.h>
#include <lka.h>
#include <vice.h>

#ifdef __cplusplus
}
#endif

/* from vicedep */
#include <venusioctl.h>
#include <writeback.h>
/* from venus */
#include "comm.h"
#include "fso.h"
#include "hdb.h"
#include "local.h"
#include "user.h"
#include "venusstats.h"
#include "venus.private.h"
#include "venusvol.h"
#include "vproc.h"
#include "worker.h" 
#include "adv_daemon.h"
#include "realmdb.h"


/* local-repair modification */
void vproc::do_ioctl(VenusFid *fid, unsigned int com, struct ViceIoctl *data) {
    /*
     *    We partition the ioctls into 3 categories:
     *      O - those on a particular object
     *      V - those on a volume as a whole
     *      F - those on the (Coda) filesystem as a whole
     */

    switch(com) {
	/* Object-based. */
	case VIOCSETAL:
	case VIOCGETAL:
	case VIOCFLUSH:
	case VIOCPREFETCH:
	case VIOC_ADD_MT_PT:
	case VIOC_AFS_DELETE_MT_PT:
	case VIOC_AFS_STAT_MT_PT:
        case VIOC_GETPFID:
	case VIOC_SETVV:
	    {
	    fsobj *f = 0;

	    int volmode = (com == VIOCSETAL || com == VIOC_ADD_MT_PT ||
			    com == VIOC_AFS_DELETE_MT_PT || com == VIOC_SETVV) ?
			   VM_MUTATING : VM_OBSERVING;
	    int rcrights = RC_STATUS;
	    if (com == VIOC_ADD_MT_PT || com == VIOC_AFS_DELETE_MT_PT)
		rcrights |= RC_DATA;

	    for (;;) {
		Begin_VFS(fid, CODA_IOCTL, volmode);
		if (u.u_error) break;

		u.u_error = FSDB->Get(&f, fid, u.u_uid, rcrights);
		if (u.u_error) goto O_FreeLocks;

		switch(com) {
		    case VIOCSETAL:
			{
			/* Verify that target is a directory. */
			if (!f->IsDir())
			    { u.u_error = ENOTDIR; break; }

			/* Verify that we have administer permission. */
			/* No.  This must be checked at the server to
                           allow System:Administrators to */
			/* always change ACLs.  Alternatively, we
                           could have venus cache the identity */
			/* of administrators. */
			/* u.u_error = f->Access((long)PRSFS_ADMINISTER, 0, u.u_uid); if (u.u_error) break; */

			/* Do the operation. */
			RPC2_CountedBS acl;
			acl.SeqLen = strlen((char *) data->in) + 1;
			if (acl.SeqLen > V_MAXACLLEN)
			    { u.u_error = EINVAL; break; }
			acl.SeqBody = (RPC2_ByteSeq)(char *) data->in;
			f->PromoteLock();
			u.u_error = f->SetACL(&acl, u.u_uid);

			break;
			}

		    case VIOCGETAL:
			{
			/* Verify that target is a directory. */
			if (!f->IsDir())
			    { u.u_error = ENOTDIR; break; }

			/* Verify that we have lookup permission. */
			/* No.  This must be checked at the server to
                           allow System:Administrators to */
			/* always change ACLs.  Alternatively, we
                           could have venus cache the identity */
			/* of administrators. */
/*
			 u.u_error = f->Access((long)PRSFS_LOOKUP, 0, u.u_uid);
			 if (u.u_error) break;
*/

			/* Do the operation. */
			RPC2_BoundedBS acl;
			acl.MaxSeqLen = V_MAXACLLEN;
			acl.SeqLen = 0;
			acl.SeqBody = (RPC2_ByteSeq)data->out;
			acl.SeqBody[0] = 0;
			f->PromoteLock();
			u.u_error = f->GetACL(&acl, u.u_uid);
			if (u.u_error) break;

			data->out_size = (short) (acl.SeqLen == 0 ? 1 : acl.SeqLen);
			break;
			}

		    case VIOCFLUSH:
			{
			FSDB->Put(&f);

			/* This is drastic, but I'm having trouble getting rid of */
			/* MiniCache vnodes that have the "wrong" type! -JJK */
			(void)k_Purge(fid, 1);

			f = FSDB->Find(fid);
			if (f != 0) {
			    u.u_error = f->Flush();
			    Recov_SetBound(DMFP);
			    f = 0;
			}

			break;
			}

		    case VIOCPREFETCH:
			{
/*
 *			 if (type == VPT_Worker)
 *			     if (!((worker *)this)->returned)
 *				 ((worker *)this)->Return(0);
 */
			/* return early to user */
			if (type == VPT_Worker) {
			    worker *w = (worker *)this;
			    union outputArgs *out;
			    out = (union outputArgs *)w->msg->msg_buf;
			    out->coda_ioctl.len = 0;
			    out->coda_ioctl.oh.result = 0;
			    w->Return(w->msg, sizeof (struct coda_ioctl_out));
			}

			/* Release and reacquire the target (data this time). */
			FSDB->Put(&f);
			u.u_error = FSDB->Get(&f, fid, u.u_uid, RC_DATA);

			break;
			}

		    case VIOC_ADD_MT_PT:
			{
			/* A mount-link is virtually identical to a symlink.
			 * In fact Coda stores mount-links as symlinks on the
			 * server. The only visible differences are that the
			 * mount-link has a Unix modemask of 0644, while a
			 * symlink has 0755. And a mountpoint's contents always
			 * start with '#', '@' (or '%'?)
			 *
			 * This code is almost identical to vproc::symlink in
			 * vproc_vfscalls. -JH
			 */
			fsobj *target_fso = NULL;
			char contents[CODA_MAXNAMLEN+1];
			char *link_name = (char *) data->in;
			char *arg = strchr(link_name, '/');

			if (!arg) { u.u_error = EINVAL; break; };
			*arg = '\0'; arg++;

			/* Disallow special names. */
			verifyname(link_name, NAME_NO_DOTS | NAME_NO_CONFLICT);
                        if (u.u_error) break;

			/* Verify that parent is a directory. */
			if (!f->IsDir()) { u.u_error = ENOTDIR; break; }

			/* Verify that the target doesn't exist. */
			u.u_error = f->Lookup(&target_fso, 0, link_name, u.u_uid, CLU_CASE_SENSITIVE);
			FSDB->Put(&target_fso);
			if (u.u_error == 0) { u.u_error = EEXIST; break; }
			if (u.u_error != ENOENT) { break; }
			u.u_error = 0;

			/* Verify that we have insert permission. */
			u.u_error = f->Access((long)PRSFS_INSERT, 0, u.u_uid);
			if (u.u_error) { break; }

			/*
			 * Regular mount-links start with a '#', optionally
			 * followed by a volume name (current path is used in
			 * case the volume name is not specified), optionally
			 * followed by '@' and a realm/domain name (the realm
			 * of the parent volume is used if this is not
			 * specified), and end with a single '.'.
			 *
			 * The ending character doesn't seem too important, it
			 * looks like the '.' was added mostly because of the
			 * buggy implementation of TryToCover which strips the
			 * last character of the volume name.
			 *
			 * There are references in the code that indicate there
			 * used to be mount-links that started with '%'. I
			 * don't know what they were used for.
			 *
			 * Internally, Venus creates mount-links starting with
			 * '@', followed by a Fid (volume.vnode.unique),
			 * optionally followed by '@' and a realm/domain name.
			 * These are used for conflicts and during repair to
			 * mount a specific object in the fake repair volume.
			 *
			 * -JH
			 */
			/* make it a 'magic' mount name */
			snprintf(contents, CODA_MAXNAMLEN,  "#%s.", arg);
			contents[CODA_MAXNAMLEN] = '\0';

			/* Do the operation. */
			f->PromoteLock();
			u.u_error = f->Symlink(contents, link_name, u.u_uid,
					       0644, FSDB->StdPri());
			if (u.u_error) { break; }
			/* set vattr fields? */
			break;
			}

		    case VIOC_AFS_DELETE_MT_PT:
			{
			fsobj *target_fso = 0;
			char *target_name = (char *) data->in;

			/* Disallow deletion of special names. */
                        verifyname(target_name, NAME_NO_DOTS | NAME_NO_CONFLICT);
                        if (u.u_error) break;

			/* Verify that parent is a directory. */
			if (!f->IsDir())
			    { u.u_error = ENOTDIR; break; }

			/* Get the target object. */
			u.u_error = f->Lookup(&target_fso, 0, target_name, u.u_uid, CLU_CASE_SENSITIVE);
			if (u.u_error) break;

			/* Verify that target is a mount point (either valid or dangling). */
			if (!target_fso->IsMtPt() && !target_fso->IsMTLink()) {
                            FSDB->Put(&target_fso);
                            u.u_error = ENOTDIR;
                            break;
                        }

			/* Verify that we have delete permission for the parent. */
			u.u_error = f->Access((long)PRSFS_DELETE, 0, u.u_uid);
			if (u.u_error) {
                            FSDB->Put(&target_fso);
                            break;
                        }

			/* We only remove MTLinks, not valid MtPts! */
			if (target_fso->IsMtPt()) {
			    Recov_BeginTrans();
                            fsobj *root_fso = target_fso->u.root;
                            FSO_ASSERT(target_fso,
                                       root_fso && root_fso->u.mtpoint == target_fso);
                            root_fso->UnmountRoot();
                            target_fso->UncoverMtPt();
                            Recov_EndTrans(MAXFP);
			}

			/* Do the remove. */
			f->PromoteLock();
			target_fso->PromoteLock();
			u.u_error = f->Remove(target_name, target_fso, u.u_uid);
			k_Purge(&target_fso->fid, 1);


			FSDB->Put(&target_fso);
			break;
			}

		    case VIOC_AFS_STAT_MT_PT:
			{
			fsobj *target_fso = 0;
			char *target_name = (char *) data->in;
			int out_size = 0;	/* needed since data->out_size is a short! */
                        verifyname(target_name, NAME_NO_DOTS);

			/* Verify that parent is a directory. */
			if (!f->IsDir()) {
                            u.u_error = ENOTDIR;
                            break;
                        }

			/* Get the target object. */
			u.u_error = f->Lookup(&target_fso, 0, target_name, u.u_uid, CLU_CASE_SENSITIVE);
			if (u.u_error) {
			    FSDB->Put(&target_fso);
			    break;
			}

			/* Verify that target is a mount point (either valid or dangling). */
			if (!target_fso->IsMtPt() && !target_fso->IsMTLink())
			    { u.u_error = ENOTDIR; FSDB->Put(&target_fso); break; }

			/*Verify that we have read permission for it. */
			u.u_error = target_fso->Access((long)PRSFS_LOOKUP, 0, u.u_uid);
			if (u.u_error) { FSDB->Put(&target_fso); break; }

			/* Retrieve the link contents from the cache. */
			u.u_error = target_fso->Readlink((char *)data->out,
							 CODA_MAXPATHLEN,
							 &out_size, u.u_uid);
			if (u.u_error) {
                            FSDB->Put(&target_fso);
                            break;
                        }

			/* Make link a proper string. */
			data->out_size = out_size;
			((char *)data->out) [data->out_size] = 0;
			(data->out_size)++;

			FSDB->Put(&target_fso);
			break;
			}

		    case VIOC_GETPFID:
			{
			if (data->in_size != (int)sizeof(VenusFid))
			    { u.u_error = EINVAL; break; }
			VenusFid *fid = (VenusFid *)data->in;

			FSDB->Put(&f);
			u.u_error = FSDB->Get(&f, fid, u.u_uid, RC_STATUS);
			if (u.u_error) break;

			/* if we're at a mount point, back up over it. */
			if (f->IsRoot() && !f->IsVenusRoot()) {
			    if (f->u.mtpoint == 0) 
				{ u.u_error = ENOENT; break; }

			    VenusFid mtptfid = f->u.mtpoint->fid;
			    FSDB->Put(&f);
			    u.u_error = FSDB->Get(&f, &mtptfid, u.u_uid, RC_STATUS);
			    if (u.u_error) break;
		        }

			/* Copy out the parent fid. */
			memcpy(data->out, &f->pfid, sizeof(VenusFid));
			data->out_size = (short)sizeof(VenusFid);

			break;
			}

		    case VIOC_SETVV:
			{
			if (data->in_size != (int)sizeof(ViceVersionVector))
			    { u.u_error = EINVAL; break; }

			f->PromoteLock();
			u.u_error = f->SetVV((ViceVersionVector *)data->in, u.u_uid);

			break;
			}
		}
O_FreeLocks:
		FSDB->Put(&f);
		int retry_call = 0;
		End_VFS(&retry_call);
		if (!retry_call) break;
	    }
	    }
	    return;

	/* Object-based. Allowing access to inconsistent objects. */
	case VIOC_ENABLEREPAIR:
        case VIOC_FLUSHASR:
	case VIOC_GETFID:
	    {
	    fsobj *f = 0;

	    for (;;) {
		Begin_VFS(fid, CODA_IOCTL, VM_OBSERVING);
		if (u.u_error) break;

		u.u_error = FSDB->Get(&f, fid, u.u_uid, RC_STATUS,
				      NULL, NULL, 1);
		if (u.u_error) goto OI_FreeLocks;

		switch(com) {
		case VIOC_ENABLEREPAIR:
		    {
		    /* Try to enable target volume for repair by this user. */
		    VolumeId  *RWVols  = (VolumeId *)data->out;
		    uid_t    *LockUids = (uid_t *)&(RWVols[VSG_MEMBERS]);
		    unsigned long *LockWSs =
			(unsigned long *)&(LockUids[VSG_MEMBERS]);
		    char      *endp     = (char *)&(LockWSs[VSG_MEMBERS]);

		    /* actually EnableRepair operates on the volume, but we
		     * also check if the object is really inconsistent */
		    if (f->IsFake() && f->vol->IsReplicated())
                        u.u_error =
			    ((repvol *)f->vol)->EnableRepair(u.u_uid,
							     RWVols,
							     LockUids,
							     LockWSs);
                    else
                        u.u_error = EOPNOTSUPP;

		    data->out_size = (endp - data->out);

		    /* Make sure the kernel drops the symlink */
		    (void)k_Purge(fid, 1);
		    (void)k_Purge(&f->pfid, 1);
		    break;
		    }

		case VIOC_FLUSHASR:
		    /* This function used to be built around FSDB->Find, which
		     * did no locking. Now we use FSDB->Get, which does do
		     * locking. Hopefully this is better, and nothing breaks.
		     * --JH */
		    {
		    /* ASR flush operation allowed only for files */
		    LOG(100, ("Going to reset lastresolved time for %s\n", FID_(fid)));
		    u.u_error = f->SetLastResolved(0);
		    break;
		    }

		case VIOC_GETFID:
		    {
		    if (!(data->in_size == 0 || data->in_size == sizeof(int))) {
			u.u_error = EINVAL;
			break;
		    }
		    /* Backup and use volroot's mount point if directed. */
		    if (data->in_size == sizeof(int) && *(int *)data->in != 0) {
			if (FID_EQ(&f->fid, &rootfid) ||
			    !FID_IsVolRoot(&f->fid) || f->u.mtpoint == 0) {
			    u.u_error = EINVAL;
			    break;
			}

			VenusFid mtptfid = f->u.mtpoint->fid;
			FSDB->Put(&f);
			u.u_error = FSDB->Get(&f, &mtptfid, u.u_uid, RC_STATUS,
					      NULL, NULL, 1);
			if (u.u_error) break;
		    }

		    struct GetFid {
			ViceFid fid;
			ViceVersionVector vv;
			char realm[MAXHOSTNAMELEN+1];
		    } *cp;
		    cp = (struct GetFid *)data->out;

		    /* Copy out the fid. */
		    memcpy(&cp->fid, MakeViceFid(&f->fid), sizeof(ViceFid));

		    /* Copy out the VV. This will be garbage unless the
		     * object is replicated! */
		    memcpy(&cp->vv, &f->stat.VV, sizeof(ViceVersionVector));

		    strcpy(cp->realm, f->vol->realm->Name());

		    data->out_size = sizeof(struct GetFid);
		    break;
		    }
		}
OI_FreeLocks:
		FSDB->Put(&f);
		int retry_call = 0;
		End_VFS(&retry_call);
		if (!retry_call) break;
	    }
	    }
	    return;

	/* Volume-based. */
	case VIOCGETVOLSTAT:
	case VIOCSETVOLSTAT:
	case VIOCWHEREIS:
	case VIOC_FLUSHVOLUME:
	case VIOC_DISABLEREPAIR:
	case VIOC_REPAIR:
	case VIOC_GETSERVERSTATS:
	case VIOC_CHECKPOINTML:
	case VIOC_PURGEML:
        case VIOC_BEGINML:
        case VIOC_ENDML:
        case VIOC_ENABLEASR:
        case VIOC_DISABLEASR: 
	case VIOC_LISTCACHE_VOLUME:
        case VIOC_BEGINWB:
        case VIOC_STATUSWB:
        case VIOC_ENDWB:
        case VIOC_AUTOWB:
	case VIOC_SYNCCACHE:
	case VIOC_REDIR:
	    {
#ifdef TIMING
 	    gettimeofday(&u.u_tv1, 0); u.u_tv2.tv_sec = 0;
#endif
	    volent *v = 0;
	    if ((u.u_error = VDB->Get(&v, MakeVolid(fid)))) break;

	    int volmode = ((com == VIOC_REPAIR || com == VIOC_PURGEML) ?
			   VM_MUTATING : VM_OBSERVING);
	    int entered = 0;
	    if ((u.u_error = v->Enter(volmode, u.u_uid)) != 0)
		goto V_FreeLocks;
	    entered = 1;

	    switch(com) {
	        case VIOC_LISTCACHE_VOLUME:
	            {
		      /* List cache status */
		      struct listcache_in {
			char fname[23];		/* "/tmp/_Venus_List_Cache" */
			int  first_volume;
			int  long_format;
			int  valid;
		      } *data_in;

		      /* Check whether specified volume is a mount point */
		      fsobj *f = FSDB->Find(fid);
		      if (f == 0) {
			MarinerLog("Specified volume is not valid fsobj.");
			u.u_error = EINVAL;
			break;
		      }
		      if ( !f->IsRoot() && !f->IsMtPt() ) {
			MarinerLog("Specified volume is not a mount point.");
			u.u_error = EINVAL;
			break;
		      }

		      /* Do ListCache */
		      data_in = (struct listcache_in *)data->in;
		      char *venus_file = data_in->fname;
		      FILE *fp;
		      if (data_in->first_volume)
			fp = fopen(venus_file, "w+");
		      else
			fp = fopen(venus_file, "a+");
		      if (fp == NULL) {
			MarinerLog("Cannot open file: %s\n", venus_file);
			u.u_error = errno;
			break;
		      }
		      v->ListCache(fp, data_in->long_format, data_in->valid);
		      fflush(fp);
		      fclose(fp);
		      if ( chmod(venus_file, 00666) < 0 ) {
			MarinerLog("Cannot chmod file: %s\n", venus_file);
			u.u_error = errno;
		      }
		      break;
		    }
		    
		case VIOCGETVOLSTAT:
		    {
		    /* Volume status block. */
		    VolumeStatus volstat;

		    /* Volume name. */
		    char name[V_MAXVOLNAMELEN];
		    RPC2_BoundedBS Name;
		    Name.SeqBody = (RPC2_ByteSeq)name;
		    Name.MaxSeqLen = V_MAXVOLNAMELEN;
		    Name.SeqLen = 0;

		    /* Offline message for this volume. */
		    char offlinemsg[256];
		    RPC2_BoundedBS OfflineMsg;
		    OfflineMsg.SeqBody = (RPC2_ByteSeq)offlinemsg;
		    OfflineMsg.MaxSeqLen = 256;
		    OfflineMsg.SeqLen = 0;

		    /* Message of the day for this volume. */
		    char motd[256];
		    RPC2_BoundedBS MOTD;
		    MOTD.SeqBody = (RPC2_ByteSeq)motd;
		    MOTD.MaxSeqLen = 256;
		    MOTD.SeqLen = 0;

		    VolumeStateType conn_state;
		    int             conflict;
		    int             cml_count;
		    int		    local_only = 0;
		    if (data->in_size == sizeof(int) &&
			*(int *)data->in == 1)
			local_only = 1;

		    /* Retrieve the volume status from the server(s). */
		    u.u_error = v->GetVolStat(&volstat, &Name, &conn_state,
					      &conflict, &cml_count,
					      &OfflineMsg, &MOTD, u.u_uid,
					      local_only);
		    if (u.u_error) break;

		    /* Format is (status, name, conn_state, conflict,
		       cml_count, offlinemsg, motd) */
		    /* First we make sure we won't overflow data->out */
		    if ( (sizeof(VolumeStatus) + /* volstat */
			 strlen(name) + 1 + /* name */
			 sizeof(int) + /* conn_state */
			 sizeof(int) + /* conflict */
			 sizeof(int) + /* cml_count */
			 strlen(offlinemsg) + 1 + /* offlinemsg */
			 strlen(motd) + 1) /* motd */
			 > VC_MAXDATASIZE ) {
			LOG(0, ("vproc::do_ioctl: VIOCGETVOLSTAT: buffer is "
				"not large enough to hold the message\n"));
			u.u_error = EINVAL;
			break;
		    }
		    /* then we copy the stuff to the buffer */
		    char *cp = (char *) data->out;/* Invariant: cp always
						     point to next loc. to
						     be copied into */
		    memcpy(cp, &volstat, sizeof(VolumeStatus));
		    cp += sizeof(VolumeStatus);
		    strcpy(cp, name);
		    cp += strlen(name) + 1;
		    memcpy (cp, &conn_state, sizeof(int));
		    cp += sizeof(int);
		    memcpy (cp, &conflict, sizeof(int));
		    cp += sizeof(int);
		    memcpy (cp, &cml_count, sizeof(int));
		    cp += sizeof(int);
		    strcpy(cp, offlinemsg);
		    cp += strlen(offlinemsg) + 1;
		    strcpy(cp, motd);
		    cp += strlen(motd) + 1;
		    data->out_size = (cp - data->out);
		    break;
		    }

		case VIOCSETVOLSTAT:
		    {
		    /* Format is (status, name, offlinemsg, motd). */
		    char *cp = (char *) data->in;

		    /* Volume status block. */
		    VolumeStatus volstat;
		    memcpy(&volstat, cp, sizeof(VolumeStatus));
		    cp += sizeof(VolumeStatus);

		    /* Volume name. */
                    char name[V_MAXVOLNAMELEN];
		    unsigned long namelen = strlen(cp) + 1;
		    if (namelen >= V_MAXVOLNAMELEN) { u.u_error = EINVAL; break; }
		    strcpy(name, cp);

		    RPC2_BoundedBS Name;
		    Name.SeqBody = (RPC2_ByteSeq)name;
		    Name.MaxSeqLen = V_MAXVOLNAMELEN;
#if 0
		    /* Avoid setting the volumename, otherwise cfs setquota
		     * renames all volume replicas to that of their replicated
		     * parent. This might confuse ViceGetVolumeInfo and leads
		     * to subtle corruption. --JH */
		    Name.SeqLen = namelen;
#else
		    Name.SeqLen = 0;
#endif
		    cp += namelen;

		    /* Offline message for this volume. */
		    char offlinemsg[256];
		    unsigned long offlinemsglen = strlen(cp) + 1;
		    if (offlinemsglen > 256) { u.u_error = EINVAL; break; }
		    strcpy(offlinemsg, cp);
		    RPC2_BoundedBS OfflineMsg;
		    OfflineMsg.SeqBody = (RPC2_ByteSeq)offlinemsg;
		    OfflineMsg.MaxSeqLen = 256;
		    OfflineMsg.SeqLen = offlinemsglen;
		    cp += offlinemsglen;

		    /* Message of the day for this volume. */
		    char motd[256];
		    unsigned long motdlen = strlen(cp) + 1;
		    if (motdlen >= 256) { u.u_error = EINVAL; break; }
		    strcpy(motd, cp);
		    RPC2_BoundedBS MOTD;
		    MOTD.SeqBody = (RPC2_ByteSeq)motd;
		    MOTD.MaxSeqLen = 256;
		    MOTD.SeqLen = motdlen;

		    /* Send the volume status to the server(s). */
		    u.u_error = v->SetVolStat(&volstat, &Name, &OfflineMsg,
					      &MOTD, u.u_uid);
		    if (u.u_error) break;

		    /* Copy all the junk back out. */
		    /* Format is (status, name, offlinemsg, motd). */
		    cp = (char *) data->out;
		    memcpy(cp, &volstat, sizeof(VolumeStatus));
		    cp += sizeof(VolumeStatus);
		    strcpy(cp, name);
		    cp += strlen(name) + 1;
		    strcpy(cp, offlinemsg);
		    cp += strlen(offlinemsg) + 1;
		    strcpy(cp, motd);
		    cp += strlen(motd) + 1;
		    data->out_size = (cp - data->out);
		    break;
		    }

		case VIOCWHEREIS:
		    {
                        /* Extract the host array from the vsgent or volent as appropriate. */
                        v->GetHosts((struct in_addr *)data->out);
                        /* Extract an array if vids from the vsgent or volent as appropriate. */
			v->GetVids((VolumeId *)(data->out + (VSG_MEMBERS * sizeof(struct in_addr))));
                        data->out_size = VSG_MEMBERS * (sizeof(struct in_addr) + sizeof(VolumeId));
                        break;
		    }

		case VIOC_FLUSHVOLUME:
		    {
		    /* This is drastic, but I'm having trouble getting rid of */
		    /* MiniCache vnodes that have the "wrong" type! -JJK */
		    (void)k_Purge();

		    FSDB->Flush(MakeVolid(fid));
		    Recov_SetBound(DMFP);

		    break;
		    }

		case VIOC_DISABLEREPAIR:
		    {
		    /* Disable repair of target volume by this user. */
                    if (v->IsReplicated())
                        u.u_error =
                            ((repvol *)v)->DisableRepair(u.u_uid);
                    else
                        u.u_error = EOPNOTSUPP;

		    (void)k_Purge(fid, 1);

		    break;
		    }
/*
  BEGIN_HTML
  <a name="dorepair"><strong> dorepair handler </strong></a>
  END_HTML
*/
		case VIOC_REPAIR:
		    {
		    /* Try to repair target object. */
#define	RepairFile  ((char *)data->in)
#define	startp	    (data->out)
#define	RWVols	    ((VolumeId *)(startp))
#define	ReturnCodes ((int *)(RWVols + VSG_MEMBERS))
#define	endp	    ((char *)(ReturnCodes + VSG_MEMBERS))
		    data->out_size = (endp - startp);
                    u.u_error = EOPNOTSUPP;
                    if (v->IsReplicated())
                        u.u_error = ((repvol *)v)->Repair(fid, RepairFile,
                                       u.u_uid, RWVols, ReturnCodes);

 	            LOG(0, ("MARIA: VIOC_REPAIR calls volent::Repair which returns %d\n",u.u_error));
		    /* We don't have the object so can't provide a pathname
		     * if ((SkkEnabled) && (u.u_error == 0)) {
		     *   NotifyUsersObjectConsistent("???UNKNOWN???",fid);
		     * } */
		    
#undef	RepairFile
#undef	startp
#undef	RWVols
#undef	ReturnCodes
#undef	endp

		    break;
		    }

		case VIOC_GETSERVERSTATS:
		    {
		    /* Extract the host array from the vsgent or volent as appropriate. */
		    struct in_addr Hosts[VSG_MEMBERS];
		    v->GetHosts(Hosts);

		    /* Count number of hosts to make sure buffer size wouldn't be exceeded. */
		    int i;
		    int nHosts = 0;
		    for (i = 0; i < VSG_MEMBERS; i++)
			if (Hosts[i].s_addr) nHosts++;
		    if (nHosts * (int)sizeof(ViceStatistics) > VC_MAXDATASIZE)
			{ u.u_error = EINVAL; break; }

		    /* Get statistics from each host. */
		    /* OUT data for hosts that are incommunicado will be zero. */
		    memset(data->out, 0, nHosts * (int)sizeof(ViceStatistics));
		    ViceStatistics *Stats = (ViceStatistics *)data->out;
		    for (i = 0; i < VSG_MEMBERS; i++)
			if (Hosts[i].s_addr) {
			    srvent *s = GetServer(&Hosts[i], v->GetRealmId());
			    (void)s->GetStatistics(Stats);
			    PutServer(&s);
			    Stats++;
			}

		    data->out_size = (char *)Stats - data->out;
		    break;
		    }

		case VIOC_CHECKPOINTML:
		    {
		    char *ckpdir = (data->in_size == 0 ? 0 : (char *) data->in);
                    u.u_error = EOPNOTSUPP;
                    if (v->IsReplicated())
                        u.u_error = ((repvol *)v)->CheckPointMLEs(u.u_uid, ckpdir);
		    break;
		    }

		case VIOC_PURGEML:
		    {
                    u.u_error = EOPNOTSUPP;
                    if (v->IsReplicated())
                        u.u_error = ((repvol *)v)->PurgeMLEs(u.u_uid);
		    break;
		    }
     	        case VIOC_BEGINML:
		    {
		    /* 
		     * Begin logging mutations to this volume. 
		     * This is "pseudo-disconnected" mode, in which
		     * fetches may be performed but mutations are logged.
		     */
		    char *startp = (char *) data->in;
#define agep ((unsigned *)(startp))
#define timep ((unsigned *)(agep + 1))
                    u.u_error = EOPNOTSUPP;
                    if (v->IsReplicated())
                        u.u_error = ((repvol *)v)->WriteDisconnect(*agep, *timep); 
		    if (u.u_error == 0) 
			eprint("Logging updates to volume %s", v->GetName());

		    break;
		    }
   	        case VIOC_ENDML:
		    {
		    /* 
		     * Stop logging mutations to this volume, and
		     * schedule it for reintegration if appropriate.
		     */
                    u.u_error = EOPNOTSUPP;
                    if (v->IsReplicated())
                        u.u_error = ((repvol *)v)->WriteReconnect();    
		    if (u.u_error == 0) 
			eprint("Propagating updates to volume %s (|CML| = %d)", 
			       v->GetName(), ((repvol *)v)->GetCML()->count());
		    break;
		    }
	      case VIOC_ENABLEASR:
		    {			
                        u.u_error = EOPNOTSUPP;
                        if (v->IsReplicated())
                            u.u_error = ((repvol *)v)->EnableASR(u.u_uid);
			break;
		    }
	      case VIOC_DISABLEASR:
		    {
                        u.u_error = EOPNOTSUPP;
                        if (v->IsReplicated())
                            u.u_error = ((repvol *)v)->DisableASR(u.u_uid);
			break;
		    }
              case VIOC_BEGINWB:
		    {  
		      /* request writeback caching from the server ! */
                        u.u_error = EOPNOTSUPP;
                        if (v->IsReplicated())
                            u.u_error = ((repvol *)v)->EnterWriteback(u.u_uid);
		      break;
		    }
 	      case VIOC_AUTOWB:
		    {
                        u.u_error = EOPNOTSUPP;
                        if (v->IsReplicated()) {
                            v->flags.autowriteback = !v->flags.autowriteback;
                            eprint("Auto Writeback on volume %s is now %s",
                                   v->GetName(), v->flags.autowriteback ?
                                   "enabled" : "disabled");

                            if (!v->flags.autowriteback) {
                                /* first we need to not be an observer on the
                                 * volume! (massive kluge! -leg, 5/9/99) */
                                v->Exit(volmode, u.u_uid);
                                entered = 0;
                                u.u_error = ((repvol *)v)->LeaveWriteback(u.u_uid);
                                //  if (u.u_error == 0)
                                //	u.u_error = WB_DISABLED;
                            }
                            else {
                                u.u_error = ((repvol *)v)->EnterWriteback(u.u_uid);
                                //if (u.u_error == 0)
                                //	u.u_error = WB_PERMIT_GRANTED;
                            }
                        }
			break;
		    }
   	      case VIOC_STATUSWB:
	      {
		  int *cp = (int *)data->out;
                  *cp = 0;
                  if (v->IsReplicated())
                      *cp = ((repvol *)v)->IsWritebacking();
		  data->out_size = sizeof(int);
		  break;
	      }
              case VIOC_ENDWB:
              {
                  u.u_error = EOPNOTSUPP;
                  if (v->IsReplicated()) {
                      /* first we need to not be an observer on the volume! 
                         (massive kluge! -leg, 5/9/99) */
                      v->flags.autowriteback = 0;
                      v->Exit(volmode, u.u_uid);
                      entered = 0;
                      /* now we'll leave writeback mode */
                      v->flags.autowriteback = 0;
                      u.u_error = ((repvol *)v)->LeaveWriteback(u.u_uid);
                  }
		  break;
              }
	      case VIOC_SYNCCACHE:
              {
                  u.u_error = EOPNOTSUPP;
                  if (v->IsReplicated()) {
		      int old_wb_flag = 0;

		      v->Exit(volmode, u.u_uid);
		      entered = 0;
		      old_wb_flag = v->flags.writebackreint;
		      v->flags.writebackreint = 1;

		      u.u_error = ((repvol *)v)->SyncCache(NULL);

		      v->flags.writebackreint = old_wb_flag;
		  }
		  break;
              }

	      case VIOC_REDIR:
	      {
		  struct in_addr staging_server;
		  if (data->in_size != (int)sizeof(struct in_addr)) {
		      u.u_error = EINVAL; break;
		  }
		  if (!v->IsReplicated()) {
		      u.u_error = EINVAL; break;
		  }

		  staging_server = *(struct in_addr *)data->in;
		  ((repvol *)v)->SetStagingServer(&staging_server);
		  break;
	      }
	    }

V_FreeLocks:
	    if (entered) v->Exit(volmode, u.u_uid);
 	    float elapsed = 0.0;
#ifdef TIMING
 	    
	    gettimeofday(&u.u_tv2, 0);
	    elapsed = SubTimes(&(u.u_tv2), &(u.u_tv1));
#endif

	    VDB->Put(&v);
	    }
	    if (u.u_error == ERETRY)
		u.u_error = EWOULDBLOCK;
	    return;

	/* FS-based. */
	case VIOCSETTOK:
	case VIOCGETTOK:
	case VIOCUNLOG:
	case VIOCCKSERV:
	case VIOCCKBACK:
	case VIOC_VENUSLOG:
	case VIOC_GETVENUSSTATS:
	case VIOC_FLUSHCACHE:
	case VIOC_HDB_ADD:
	case VIOC_HDB_DELETE:
	case VIOC_HDB_CLEAR:
	case VIOC_HDB_LIST:
	case VIOC_HDB_WALK:
	case VIOC_HDB_VERIFY:
	case VIOC_HDB_ENABLE:
	case VIOC_HDB_DISABLE:
	case VIOC_CLEARPRIORITIES:
	case VIOC_WAITFOREVER:
	case VIOC_GETPATH:
        case VIOC_TRUNCATELOG:
        case VIOC_DISCONNECT:
        case VIOC_RECONNECT:
        case VIOC_SLOW:
	case VIOC_STRONG:
	case VIOC_ADAPTIVE:
	case VIOC_LISTCACHE:
	case VIOC_GET_MT_PT:
        case VIOC_WD_ALL:
	case VIOC_WR_ALL:
	case VIOC_REP_CMD:
        case VIOC_UNLOADKERNEL:	
        case VIOC_LOOKASIDE:
	    {
	    switch(com) {
                case VIOC_LOOKASIDE:
	            {
		      /* cache lookaside command (cfs lka) */
		      memset(data->out, 0, CFS_PIOBUFSIZE);
		      LKParseAndExecute((char *)data->in,  
                                  (char *)data->out, CFS_PIOBUFSIZE-1);
		      data->out_size = strlen(data->out) + 1; 
		      break; /* outmsg has success/failure info */

	            }
		case VIOC_LISTCACHE:
	            {
		      /* List cache status */
		      struct listcache_in {
 			char fname[23];		/* "/tmp/_Venus_List_Cache" */
			int  first_volume;
			int  long_format;
			int  valid;
		      } *data_in;

		      data_in = (struct listcache_in *)data->in;
		      char *venus_file = data_in->fname;

		      FILE *fp;
		      if (data_in->first_volume)
			fp = fopen(venus_file, "w+");
		      else
			fp = fopen(venus_file, "a+");
		      if (fp == NULL) {
			MarinerLog("Cannot open file: %s\n", venus_file);
			u.u_error = errno;
			break;
		      }
		      VDB->ListCache(fp, data_in->long_format, data_in->valid);
		      fflush(fp);
		      fclose(fp);
		      if ( chmod(venus_file, 00666) < 0 ) {
			MarinerLog("Cannot chmod file: %s\n", venus_file);
			u.u_error = errno;
		      }
		      break;
		    }
		case VIOC_GET_MT_PT:
	            {
		      /* Get mount point pathname */
		      if (data->in_size < (int)sizeof(VolumeId) + 1) {
			u.u_error = EINVAL; break;
		      }
		      Volid volid;

		      Realm *r = REALMDB->GetRealm((char *)data->in +
						   sizeof(VolumeId));
		      volid.Realm = r->Id();
		      r->PutRef();

		      volid.Volume = *(VolumeId *)data->in;
		      volent *vv = VDB->Find(&volid);
		      if (!vv) {
			MarinerLog("Could not find volume = %x.%x\n",
				   volid.Realm, volid.Volume);
			u.u_error = EINVAL;
			break;
		      }
		      vv->GetMountPath((char *)data->out, 0);
		      vv->release();
		      if (STREQ((char *)data->out, "???")) {
			MarinerLog("Could not get mount point path for %x.%x\n",
				   volid.Realm, volid.Volume);
			u.u_error = EINVAL;
			break;
		      }
		      data->out_size = (short) strlen((char *)data->out)+1;
		      break;
		    }
		
		case VIOCSETTOK:
		    {
		    /* Format of data is (len, secret, len, clear) */
		    char *startp = (char *) data->in;
#define secretlen ((long *)(startp))
#define secretp ((SecretToken *)(secretlen + 1))
#define clearlen ((long *)(secretp + 1))
#define clearp ((ClearToken *)(clearlen + 1))
#define realmp ((char *)(clearp + 1))
/*
		    if (*secretlen != (int)sizeof(SecretToken) ||
			*clearlen != (int)sizeof(ClearToken))
			{ u.u_error = EINVAL; break; }
*/
		    Realm *realm = REALMDB->GetRealm(realmp);

		    u.u_error = realm->NewUserToken(u.u_uid, secretp, clearp);
		    if (!u.u_error) {
			connent *c = NULL;
			/* attempt to trigger reintegration */
			realm->GetAdmConn(&c);
			PutConn(&c);
		    }
		    realm->PutRef();
#undef	secretlen
#undef	secretp
#undef	clearlen
#undef	clearp
#undef	realmp
		    break;
		    }

		case VIOCGETTOK:
		    {
		    /* Format of data is (len, secret, len, clear) */
		    char *startp = (char *) data->out;
#define secretlen ((long *)(startp))
#define secretp ((SecretToken *)(secretlen + 1))
#define clearlen ((long *)(secretp + 1))
#define clearp ((ClearToken *)(clearlen + 1))
#define endp ((char *)(clearp + 1)) 
		    Realm *realm = REALMDB->GetRealm(data->in);
		    userent *ue = realm->GetUser(u.u_uid);	   
		    u.u_error = ue->GetTokens(secretp, clearp);
		    PutUser(&ue);
		    realm->PutRef();
		    if (u.u_error) break;

		    *secretlen = (int)sizeof(SecretToken);
		    *clearlen = (int)sizeof(ClearToken);
		    data->out_size = (short)(endp - startp);
#undef	secretlen
#undef	secretp
#undef	clearlen
#undef	clearp
#undef	endp
		    break;
		    }

		case VIOCUNLOG:
		    {
		    Realm *realm = REALMDB->GetRealm(data->in);
		    userent *ue = realm->GetUser(u.u_uid);
		    ue->Invalidate();
		    PutUser(&ue);
		    realm->PutRef();

		    break;
		    }

		case VIOCCKSERV:
		    {
		    unsigned int bufsize = 2048; /* XXX - IN/OUT parameter. */
		    if (data->in_size == 0) {    /* probe everybody we know */
			ServerProbe();
			DownServers((char *) data->out, &bufsize);
		    } else {
			/* probe only those listed. */
			/* format is #hosts, hostaddr, hostaddr, ... */
#define nservers ((int *)(data->in))
#define hostids ((struct in_addr *)(nservers + 1))
			DoProbes(*nservers, hostids);
			DownServers(*nservers, hostids, (char *) data->out, &bufsize);
#undef nservers
#undef hostids
		    }
		    data->out_size = bufsize;
		    break;
		    }

		case VIOCCKBACK:
/*		case VIOCCKVOLS:*/
		    {
/*		    VDB->CheckVolumes();*/
		    FSDB->InvalidateMtPts();

		    break;
		    }

		case VIOC_VENUSLOG:
		    {
		    if (u.u_uid != V_UID)
			{ u.u_error = EACCES; break; }

		    long on;
		    memcpy(&on, data->in, sizeof(long));
		    on &= 0xff;
		    if (on) DebugOn(); else DebugOff();

		    break;
		    }

		case VIOC_GETVENUSSTATS:
		    {
		    if (sizeof(VenusStatistics) > VC_MAXDATASIZE)
			{ u.u_error = EINVAL; break; }

		    VenusStatistics *Stats = (VenusStatistics *)data->out;
		    memset((void *)Stats, 0, (int)sizeof(VenusStatistics));
		    Stats->VFSStats = VFSStats;
		    Stats->CommStats.RPCOpStats = RPCOpStats;
		    GetCSS(&Stats->CommStats.RPCPktStats);

		    data->out_size = (short)sizeof(VenusStatistics);
		    break;
		    }

		case VIOC_FLUSHCACHE:
		    {
		    /* This is drastic, but I'm having trouble getting rid of */
		    /* MiniCache vnodes that have the "wrong" type! -JJK */
		    (void)k_Purge();

		    FSDB->Flush();
		    Recov_SetBound(DMFP);

		    break;
		    }

		case VIOC_HDB_ADD:
		    {
		    struct hdb_add_msg *msgp = (struct hdb_add_msg *)data->in;
		    if (data->in_size != (int)sizeof(struct hdb_add_msg))
			{ u.u_error = EINVAL; break; }

		    u.u_error = HDBD_Request(HdbAdd, msgp, &u);

		    break;
		    }

		case VIOC_HDB_DELETE:
		    {
		    struct hdb_delete_msg *msgp = (struct hdb_delete_msg *)data->in;
		    if (data->in_size != (int)sizeof(struct hdb_delete_msg))
			{ u.u_error = EINVAL; break; }

		    u.u_error = HDBD_Request(HdbDelete, msgp, &u);

		    break;
		    }

		case VIOC_HDB_CLEAR:
		    {
		    struct hdb_clear_msg *msgp = (struct hdb_clear_msg *)data->in;
		    if (data->in_size != (int)sizeof(struct hdb_clear_msg))
			{ u.u_error = EINVAL; break; }

		    u.u_error = HDBD_Request(HdbClear, msgp, &u);

		    break;
		    }

		case VIOC_HDB_LIST:
		    {
		    struct hdb_list_msg *msgp = (struct hdb_list_msg *)data->in;
		    if (data->in_size != (int)sizeof(struct hdb_list_msg))
			{ u.u_error = EINVAL; break; }

		    u.u_error = HDBD_Request(HdbList, msgp, &u);

		    break;
		    }

		case VIOC_HDB_WALK:
		    {
		    struct hdb_walk_msg *msgp = (struct hdb_walk_msg *)data->in;
		    if (data->in_size != (int)sizeof(struct hdb_walk_msg))
			{ u.u_error = EINVAL; break; }

		    u.u_error = HDBD_Request(HdbWalk, msgp, &u);

		    break;
		    }


	        case VIOC_HDB_VERIFY:
		    {
		    struct hdb_verify_msg *msgp = (struct hdb_verify_msg *)data->in;		    
		    if (data->in_size != (int)sizeof(struct hdb_verify_msg))
			{ u.u_error = EINVAL; break; }

		    u.u_error = HDBD_Request(HdbVerify, msgp, &u);

		    break;
		    }

		case VIOC_HDB_ENABLE:
		    {
		    struct hdb_walk_msg *msgp = (struct hdb_walk_msg *)data->in;
		    if (data->in_size != (int)sizeof(struct hdb_walk_msg))
			{ u.u_error = EINVAL; break; }

		    u.u_error = HDBD_Request(HdbEnable, msgp, &u);

		    break;
		    }
		    
		case VIOC_HDB_DISABLE:
		    {
		    struct hdb_walk_msg *msgp = (struct hdb_walk_msg *)data->in;
		    if (data->in_size != (int)sizeof(struct hdb_walk_msg))
			{ u.u_error = EINVAL; break; }

		    u.u_error = HDBD_Request(HdbDisable, msgp, &u);

		    break;
		    }
		    
		case VIOC_CLEARPRIORITIES:
		    {
		    FSDB->ClearPriorities();

		    break;
		    }

		case VIOC_WAITFOREVER:
		    {
		    int on;
		    memcpy(&on, data->in, sizeof(int));

		    /* We would like "waitforever" behavior to be settable on a
		     * per-process group basis. However, this would require
		     * cooperation with the kernel, which I don't want to mess
		     * with now.  So instead, we will set it on a per-user
		     * basis (at least for now). */
		    Realm *realm = REALMDB->GetRealm((char *)data->in+sizeof(int));
		    userent *ue = realm->GetUser(u.u_uid);
		    ue->SetWaitForever(on);
		    PutUser(&ue);
		    realm->PutRef();

		    break;
		    }

		case VIOC_GETPATH:
		    {
		    if (data->in_size <= sizeof(VenusFid) || 
			*((char *)data->in + data->in_size - 1) != '\0')
			{ u.u_error = EINVAL; break; }

		    ViceFid fid;
		    memcpy(&fid, data->in, sizeof(ViceFid));
		    char *realmname = (char *)data->in + sizeof(ViceFid);
		    Realm *realm = REALMDB->GetRealm(realmname);

		    int	out_size = CODA_MAXPATHLEN;	    /* needed since data->out_size is a short! */
		    VenusFid vfid;
		    MakeVenusFid(&vfid, realm->Id(), &fid);

		    GetPath(&vfid, (char *) data->out, &out_size, 1);

		    data->out_size = out_size;

		    realm->PutRef();

		    break;
		    }

	        case VIOC_TRUNCATELOG:
		    {
		    RecovFlush(1);
		    RecovTruncate(1);

		    break;
		    }

	        case VIOC_DISCONNECT:
		    {
		    if (data->in_size == 0)  /* disconnect from everyone */
		        u.u_error = FailDisconnect(0,0);
		    else
#define nservers ((int *)(data->in))
#define hostids ((struct in_addr *)(nservers + 1))
		        u.u_error = FailDisconnect(*nservers, hostids);
#undef nservers
#undef hostids
		    if (u.u_error < 0) u.u_error = EINVAL; /* fail returns -1 */
		    break;
		    }

	        case VIOC_RECONNECT:
		    {
		    if (data->in_size == 0)  /* reconnect to everyone */
		        u.u_error = FailReconnect(0,0);
		    else
#define nservers ((int *)(data->in))
#define hostids ((struct in_addr *)(nservers + 1))
		        u.u_error = FailReconnect(*nservers, hostids);
#undef nservers
#undef hostids
		    if (u.u_error < 0) u.u_error = EINVAL; /* fail returns -1 */

		    break;
		    }

	        case VIOC_SLOW:
		    {
		    if (data->in_size != (int)sizeof(unsigned))
			{ u.u_error = EINVAL; break; }

		    unsigned *speed = (unsigned *)data->in;

		    u.u_error = FailSlow(speed);
		    if (u.u_error < 0) u.u_error = EINVAL; /* fail returns -1 */
		    break;
		    }

		/* cfs strong: Force strong connectivity to all servers */
	        case VIOC_STRONG:
		    {
		    srv_iterator next;
		    srvent *s;
		    while ((s = next()))
			s->ForceStrong(1);
		    break;
		    }
		/* cfs adaptive: Allow dynamic adaptation of connectivity */
	        case VIOC_ADAPTIVE:
		    {
		    srv_iterator next;
		    srvent *s;
		    while ((s = next()))
			s->ForceStrong(0);
		    break;
		    }

		case VIOC_WD_ALL:
		    {
		    char *startp = (char *) data->in;
#define agep ((unsigned *)(startp))
#define timep ((unsigned *)(agep + 1))
		    u.u_error = VDB->WriteDisconnect(*agep, *timep);
		    if (u.u_error == 0) 
			eprint("Logging updates to all volumes");
		    break;
		    }
   	        case VIOC_WR_ALL:
		    {
		    u.u_error = VDB->WriteReconnect();
		    if (u.u_error == 0) 
			eprint("Propagating updates to all volumes");
		    break;
		    }    	        
		case VIOC_REP_CMD:
		    {
			int rep_cmd;
			CODA_ASSERT(sscanf((char *) data->in, "%d", &rep_cmd) == 1);
			switch (rep_cmd) {
			case REP_CMD_BEGIN:
			    {
				/* begin a repair session */
				int mode, dummy;
				fsobj *obj = FSDB->Find(fid);
				CODA_ASSERT(obj);
				sscanf((char *) data->in, "%d %d", &dummy, &mode);
				LRDB->BeginRepairSession(&obj->fid,
							 mode ? REP_SCRATCH_MODE : REP_DIRECT_MODE, 
							 (char *) data->out);
				data->out_size = (short)sizeof((char *) data->out);
				u.u_error = 0;
				break;
			    }
/*
  BEGIN_HTML
  <a name="endrepair"><strong> endrepair handler </strong></a>
  END_HTML
*/
			case REP_CMD_END:
			    {			
				int commit, dummy;
				VenusFid *squirrelFid;

				CODA_ASSERT(LRDB);
				squirrelFid = LRDB->RFM_LookupGlobalRoot(LRDB->repair_root_fid);

				sscanf((char *) data->in, "%d %d", &dummy, &commit);
				LRDB->EndRepairSession(commit, (char *) data->out);
				data->out_size = (short)strlen((char *) data->out) + 1;
				u.u_error = 0;
				if (strncmp((char *) data->out, "repair session completed", strlen("repair session completed")) == 0) {

				  LOG(0, ("MARIA:  End local repair successful\n"));
				  /* We don't have the object so can't provide a pathname
				   * if (SkkEnabled) {
				   *   NotifyUsersObjectConsistent("???UNKNOWN???", squirrelFid);
				   * } */
				} else {
				  LOG(0, ("MARIA:  End local repair failed\n"));
				}
				break;
			    }
/*
  BEGIN_HTML
  <a name="checklocal"><strong> checklocal handler </strong></a>
  END_HTML
*/
			case REP_CMD_CHECK:
			    {
				LRDB->ContinueRepairSession((char *) data->out);
				data->out_size = (short)strlen((char *) data->out) + 1;
				u.u_error = 0;
				break;
			    }
/*
  BEGIN_HTML
  <a name="preservelocal"><strong> preservelocal handler </strong></a>
  END_HTML
*/
			case REP_CMD_PRESERVE:
			    {
				LRDB->PreserveLocalMutation((char *) data->out);
				data->out_size = (short)strlen((char *) data->out) + 1;
				u.u_error = 0;
				break;
			    }
/*
  BEGIN_HTML
  <a name="preservealllocal"><strong> preservealllocal handler </strong></a>
  END_HTML
*/
			case REP_CMD_PRESERVE_ALL:
			    {
				LRDB->PreserveAllLocalMutation((char *) data->out);
				data->out_size = (short)strlen((char *) data->out) + 1;
				u.u_error = 0;
				break;
			    }
/*
  BEGIN_HTML
  <a name="discardlocal"><strong> discardlocal handler </strong></a>
  END_HTML
*/
			case REP_CMD_DISCARD:
			    {
				LRDB->DiscardLocalMutation((char *) data->out);
				data->out_size = (short)strlen((char *) data->out) + 1;
				u.u_error = 0;
				break;
			    }
/*
  BEGIN_HTML
  <a name="discardalllocal"><strong> discardalllocal handler </strong></a>
  END_HTML
*/
			case REP_CMD_DISCARD_ALL:
			    {
				LRDB->DiscardAllLocalMutation((char *) data->out);
				data->out_size = (short)strlen((char *) data->out) + 1;
				u.u_error = 0;
				break;
			    }
/*
  BEGIN_HTML
  <a name="listlocal"><strong> listlocal handler </strong></a>
  END_HTML
*/
			case REP_CMD_LIST:
			    {  
				/* list local mutations belonging to this session */
				int dummy;
				char fpath[CODA_MAXPATHLEN];
				sscanf((char *) data->in, "%d %s", &dummy, fpath);
				if (LRDB->repair_root_fid == NULL) {
				    sprintf((char *) data->out, "no repair session going on\n");
				    u.u_error = ENOENT;
				} else {
				    FILE *fp = fopen(fpath, "w");
				    if (fp == NULL) {
					u.u_error = ENOENT;
					sprintf((char *) data->out, "can not open %s\n", fpath);
				    } else {
					LRDB->ListCML(LRDB->repair_root_fid, fp);
					sprintf((char *) data->out, "local mutations are:\n");
					fflush(fp);
					fclose(fp);
					u.u_error = 0;
				    }
				}
				data->out_size = (short)strlen((char *) data->out) + 1;
				break;
			    }
/*
  BEGIN_HTML
  <a name="setlocalview"><strong> setlocalview handler </strong></a>
  END_HTML
*/
			case REP_CMD_LOCAL_VIEW:
			    {
				LRDB->SetSubtreeView(SUBTREE_LOCAL_VIEW, (char *) data->out);
				data->out_size = (short)strlen((char *) data->out) + 1;
				u.u_error = 0;
				break;
			    }
/*
  BEGIN_HTML
  <a name="setglobalview"><strong> setglobalview handler </strong></a>
  END_HTML
*/
			case REP_CMD_GLOBAL_VIEW:
			    {
				LRDB->SetSubtreeView(SUBTREE_GLOBAL_VIEW, (char *) data->out);
				data->out_size = (short)strlen((char *) data->out) + 1;
				u.u_error = 0;
				break;
			    }
/*
  BEGIN_HTML
  <a name="setmixedview"><strong> setmixedview handler </strong></a>
  END_HTML
*/
			case REP_CMD_MIXED_VIEW:
			    {
				LRDB->SetSubtreeView(SUBTREE_MIXED_VIEW, (char *) data->out);
				data->out_size = (short)strlen((char *) data->out) + 1;
				u.u_error = 0;
				break;
			    }
			default:
			    eprint("bogus REP_CMD(%d)", rep_cmd);
			}
			break;
		    }		    
		}
	    }
	    if (u.u_error == ERETRY)
		u.u_error = EWOULDBLOCK;
	    return;

	default:
	    u.u_error = EOPNOTSUPP;
	    return;
    }
}
