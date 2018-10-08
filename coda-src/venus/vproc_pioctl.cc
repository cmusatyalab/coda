/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2016 Carnegie Mellon University
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
#include <lka.h>
#include <vice.h>

#ifdef __cplusplus
}
#endif

/* from vicedep */
#include <venusioctl.h>
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
#include "realmdb.h"

/* local-repair modification */
void vproc::do_ioctl(VenusFid *fid, unsigned char nr, struct ViceIoctl *data)
{
    /*
     *    We partition the ioctls into 3 categories:
     *      O - those on a particular object
     *      V - those on a volume as a whole
     *      F - those on the (Coda) filesystem as a whole
     */
    switch(nr) {
	/* Object-based. */
	case _VIOCSETAL:
	case _VIOCGETAL:
	case _VIOCFLUSH:
	case _VIOCPREFETCH:
	case _VIOC_ADD_MT_PT:
	case _VIOC_AFS_DELETE_MT_PT:
	case _VIOC_AFS_STAT_MT_PT:
	case _VIOC_GETPFID:
	case _VIOC_SETVV:
	    {
	    fsobj *f = 0;

	    int volmode = (nr == _VIOCSETAL || nr == _VIOC_ADD_MT_PT ||
			   nr == _VIOC_AFS_DELETE_MT_PT || nr == _VIOC_SETVV)
	      ? VM_MUTATING : VM_OBSERVING;
	    int rcrights = RC_STATUS;
	    if (nr == _VIOC_ADD_MT_PT || nr == _VIOC_AFS_DELETE_MT_PT)
		rcrights |= RC_DATA;

	    for (;;) {
	        Begin_VFS(fid, CODA_IOCTL, volmode);
		if (u.u_error) break;

		u.u_error = FSDB->Get(&f, fid, u.u_uid, rcrights);
		if (u.u_error) goto O_FreeLocks;

		switch(nr) {
		    case _VIOCSETAL:
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
			/* u.u_error = f->Access(PRSFS_ADMINISTER, C_A_F_OK, u.u_uid); if (u.u_error) break; */

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

		    case _VIOCGETAL:
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
			 u.u_error = f->Access(PRSFS_LOOKUP, C_A_F_OK, u.u_uid);
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

		    case _VIOCFLUSH:
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

		    case _VIOCPREFETCH:
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
			    out->coda_ioctl.data =
				(char *)sizeof(struct coda_ioctl_out);
			    out->coda_ioctl.len = 0;
			    out->coda_ioctl.oh.result = 0;
			    w->Return(w->msg, sizeof (struct coda_ioctl_out));
			}

			/* Release and reacquire the target (data this time). */
			FSDB->Put(&f);
			u.u_error = FSDB->Get(&f, fid, u.u_uid, RC_DATA);

			break;
			}

		    case _VIOC_ADD_MT_PT:
			{
			/* A mount-link is virtually identical to a symlink.
			 * In fact Coda stores mount-links as symlinks on the
			 * server. The only visible differences are that the
			 * mount-link has a Unix modemask of 0644, while a
			 * symlink has 0777. A mountpoint's contents always
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
			verifyname(link_name, NAME_NO_DOTS | NAME_NO_CONFLICT |
				   NAME_NO_EXPANSION);
			if (u.u_error) break;

			/* Verify that parent is a directory. */
			if (!f->IsDir()) { u.u_error = ENOTDIR; break; }

			/* Verify that the target doesn't exist. */
			u.u_error = f->Lookup(&target_fso, NULL, link_name,
					      u.u_uid, CLU_CASE_SENSITIVE);
			FSDB->Put(&target_fso);
			if (u.u_error == 0) { u.u_error = EEXIST; break; }
			if (u.u_error != ENOENT) { break; }
			u.u_error = 0;

			/* Verify that we have insert permission. */
			u.u_error = f->Access(PRSFS_INSERT, C_A_F_OK, u.u_uid);
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
			 *
			 * Additionally, collapsing an expanded object that
			 * was previously a mount point modifies the old mount-
			 * link to start with '$'. This is to avoid problems
			 * with ignoring dangling symlinks within
			 * TryToCover, and is never visible to the user. - Adam
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

			/* Send a downcall to the kernel to get rid of any
			 * negative name cache entries for the newly created
			 * object */
			k_Purge(fid, 0);
			break;
			}

		    case _VIOC_AFS_DELETE_MT_PT:
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
			u.u_error = f->Lookup(&target_fso, NULL, target_name,
					      u.u_uid, CLU_CASE_SENSITIVE);
			if (u.u_error) break;

			/* Verify that target is a mount point (either valid or dangling). */
			if (!target_fso->IsMtPt() && !target_fso->IsMTLink()) {
			    FSDB->Put(&target_fso);
			    u.u_error = ENOTDIR;
			    break;
			}

			/* Verify that we have delete permission for the parent. */
			u.u_error = f->Access(PRSFS_DELETE, C_A_F_OK, u.u_uid);
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

			/* Get rid of anything cached in the kernel relating to
			 * the removed volume and it's children */
			k_Purge(&target_fso->fid, 1);

			FSDB->Put(&target_fso);
			break;
			}

		    case _VIOC_AFS_STAT_MT_PT:
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
			u.u_error = f->Lookup(&target_fso, NULL, target_name,
					      u.u_uid, CLU_CASE_SENSITIVE);
			if (u.u_error) {
			    FSDB->Put(&target_fso);
			    break;
			}

			/* Verify that target is a mount point (either valid or dangling). */
			if (!target_fso->IsMtPt() && !target_fso->IsMTLink())
			    { u.u_error = ENOTDIR; FSDB->Put(&target_fso); break; }

			/*Verify that we have read permission for it. */
			u.u_error = target_fso->Access(PRSFS_LOOKUP, C_A_F_OK, u.u_uid);
			if (u.u_error) { FSDB->Put(&target_fso); break; }

			/* Retrieve the link contents from the cache. */
			u.u_error = target_fso->Readlink((char *)data->out,
							 CODA_MAXPATHLEN-1,
							 &out_size, u.u_uid);
			if (u.u_error) {
			    FSDB->Put(&target_fso);
			    break;
			}

			/* Make link a proper string. */
			data->out_size = out_size;
			((char *)data->out) [data->out_size] = '\0';
			(data->out_size)++;

			FSDB->Put(&target_fso);
			break;
			}

		    case _VIOC_GETPFID:
		    {
			if (data->in_size <= sizeof(VenusFid) || 
			    *((char *)data->in + data->in_size - 1) != '\0')
			    { u.u_error = EINVAL; break; }

			ViceFid fid;
			memcpy(&fid, data->in, sizeof(ViceFid));
			char *realmname = (char *)data->in + sizeof(ViceFid);
			Realm *realm = REALMDB->GetRealm(realmname);

			VenusFid vfid;
			MakeVenusFid(&vfid, realm->Id(), &fid);

			FSDB->Put(&f);

			u.u_error = FSDB->Get(&f, &vfid, u.u_uid, RC_STATUS);
			if (u.u_error) break;

			/* if we're at a mount point, back up over it. */
			if (f->IsRoot()) {
			    if (f->u.mtpoint == 0) 
				{ u.u_error = ENOENT; break; }

			    VenusFid mtptfid = f->u.mtpoint->fid;
			    FSDB->Put(&f);
			    u.u_error = FSDB->Get(&f, &mtptfid, u.u_uid, RC_STATUS);
			    if (u.u_error) break;
		        }

			/* Copy out the parent fid. */
			memcpy(data->out, MakeViceFid(&f->pfid), sizeof(ViceFid));
			data->out_size = (short)sizeof(ViceFid);

			break;
			}

		    case _VIOC_SETVV:
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
	case _VIOC_ENABLEREPAIR:
	case _VIOC_DISABLEREPAIR:
        case _VIOC_EXPANDOBJECT:
        case _VIOC_COLLAPSEOBJECT:
        case _VIOC_REPAIR:
	case _VIOC_FLUSHASR:
	case _VIOC_GETFID:
	    {
	    fsobj *f = 0;

	    int volmode =
	      (nr == _VIOC_EXPANDOBJECT || nr == _VIOC_ENABLEREPAIR ||
	       nr == _VIOC_COLLAPSEOBJECT || nr == _VIOC_DISABLEREPAIR ||
	       nr == _VIOC_REPAIR)
	      ? VM_MUTATING : VM_OBSERVING;

	    int rcrights = RC_STATUS;

	    if (nr == _VIOC_EXPANDOBJECT || nr == _VIOC_ENABLEREPAIR ||
		nr == _VIOC_COLLAPSEOBJECT || nr == _VIOC_DISABLEREPAIR ||
		nr == _VIOC_REPAIR)
	      rcrights |= RC_DATA;

	    for (;;) {
		Begin_VFS(fid, CODA_IOCTL, volmode);
		if (u.u_error) break;

		u.u_error = FSDB->Get(&f, fid, u.u_uid, RC_STATUS,
				      NULL, NULL, NULL, 1);
		if (u.u_error && u.u_error != EINCONS) goto OI_FreeLocks;

		switch(nr) {

		case _VIOC_ENABLEREPAIR:
		case _VIOC_EXPANDOBJECT:
		  {
		    f->PromoteLock();
		    u.u_error = f->ExpandObject();

		    /* Make sure the kernel drops the symlink */
		    (void)k_Purge(fid, 1);
		    (void)k_Purge(&f->pfid, 1);
		    break;
		  }

		case _VIOC_DISABLEREPAIR:
		  u.u_error = 0;
		case _VIOC_COLLAPSEOBJECT:
		  {
		      f->PromoteLock();
		      u.u_error = f->CollapseObject();

		      /* Make sure the kernel drops the subtree */
		      (void)k_Purge(&f->fid, 1);
		      (void)k_Purge(&f->pfid, 1);
		      break;
		  }

		  /*
		    BEGIN_HTML
		    <a name="dorepair"><strong> dorepair handler </strong></a>
		    END_HTML
		  */
		case _VIOC_REPAIR:
		  {
		    if (f->IsLocalObj()) {
		      int rc;
		      fsobj *fakedir = f;

		      /* find the expanded object */
		      f = NULL;
		      rc = fakedir->Lookup(&f, NULL, LOCALCACHE, u.u_uid,
				    CLU_CASE_SENSITIVE | CLU_TRAVERSE_MTPT, 1);
		      if(rc)
			rc = fakedir->Lookup(&f, NULL, LOCALCACHE_HIDDEN,
			   u.u_uid, CLU_CASE_SENSITIVE | CLU_TRAVERSE_MTPT, 1);
		      if(rc) {
			LOG(0, ("VIOC_REPAIR: Lookup() failed for LOCALCACHE:%d\n", rc));
			break;
		      }
		      else
			CODA_ASSERT(f);

		      LOG(0, ("VIOC_REPAIR: called on expanded directory (%s)! redirecting to localcache (%s)\n", FID_(&fakedir->fid), FID_(&f->fid)));

		      FSDB->Put(&fakedir);
		    }

#ifdef TIMING
		    gettimeofday(&u.u_tv1, 0); u.u_tv2.tv_sec = 0;
#endif
		    /* whether we need this depends on the granularity of repair calls */
		    volent *v = 0;
		    if ((u.u_error = VDB->Get(&v, MakeVolid(&f->fid)))) break;

		    int entered = 0;
		    if ((u.u_error = v->Enter(volmode, u.u_uid)) != 0)
		      CODA_ASSERT("V_FreeLocks!\n");//goto V_FreeLocks;
		    entered = 1;

		    /* Try to repair target object. */
#define	RepairFile  ((char *)data->in)
#define	startp	    (data->out)
#define	RWVols	    ((VolumeId *)(startp))
#define	ReturnCodes ((int *)(RWVols + VSG_MEMBERS))
#define	endp	    ((char *)(ReturnCodes + VSG_MEMBERS))
		    data->out_size = (endp - startp);

                    u.u_error = EOPNOTSUPP;

		    if(v->IsReplicated()) {
		      VenusFid fid = f->fid;
		      LOG(0, ("VIOC_REPAIR calling repvol::Repair (%s)\n",
			      FID_(&f->fid)));
		      FSDB->Put(&f);
		      u.u_error = ((repvol *)v)->Repair(&fid, RepairFile,
							u.u_uid, RWVols,
							ReturnCodes);
		    }
		    else
		      LOG(0, ("VIOC_REPAIR: non-replicated volume!\n"));

		    LOG(0, ("VIOC_REPAIR: repvol::Repair returns %d\n",
			    u.u_error));
#undef	RepairFile
#undef	startp
#undef	RWVols
#undef	ReturnCodes
#undef	endp
		    if (entered) v->Exit(volmode, u.u_uid);
#ifdef TIMING
		    gettimeofday(&u.u_tv2, 0);
#endif

		    VDB->Put(&v);
		    break;
		  }

		case _VIOC_FLUSHASR:
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

		case _VIOC_GETFID:
		    {
		    if (!(data->in_size == 0 || data->in_size == sizeof(int))) {
			u.u_error = EINVAL;
			break;
		    }
		    /* Backup and use volroot's mount point if directed. */
		    if (data->in_size == sizeof(int) && *(int *)data->in != 0) {
			if (!FID_IsVolRoot(&f->fid) || f->u.mtpoint == 0) {
			    u.u_error = EINVAL;
			    break;
			}

			VenusFid mtptfid = f->u.mtpoint->fid;
			FSDB->Put(&f);
			u.u_error = FSDB->Get(&f, &mtptfid, u.u_uid, RC_STATUS,
					      NULL, NULL, NULL, 1);
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
		if(f)
		  FSDB->Put(&f);
		int retry_call = 0;
		End_VFS(&retry_call);
		if (!retry_call) break;
	    }
	    }
	    return;

	/* Volume-based. */
	case _VIOCGETVOLSTAT:
	case _VIOCSETVOLSTAT:
	case _VIOCWHEREIS:
	case _VIOC_FLUSHVOLUME:
	case _VIOC_GETSERVERSTATS:
	case _VIOC_CHECKPOINTML:
	case _VIOC_PURGEML:
	case _VIOC_WD:
	case _VIOC_ENABLEASR:
	case _VIOC_DISABLEASR: 
	case _VIOC_LISTCACHE_VOLUME:
	case _VIOC_SYNCCACHE:
	case _VIOC_REDIR:
	case _VIOC_REP_CMD:

	    {
#ifdef TIMING
 	    gettimeofday(&u.u_tv1, 0); u.u_tv2.tv_sec = 0;
#endif
	    volent *v = 0;
	    if ((u.u_error = VDB->Get(&v, MakeVolid(fid)))) break;

	    int volmode = ((nr == _VIOC_PURGEML || nr == _VIOC_REP_CMD) ?
			   VM_MUTATING : VM_OBSERVING);
	    int entered = 0;
	    if ((u.u_error = v->Enter(volmode, u.u_uid)) != 0)
            goto V_FreeLocks;
	    entered = 1;

	    switch(nr) {
	        case _VIOC_LISTCACHE_VOLUME:
	            {
		      /* List cache status */
		      struct listcache_in {
			uint8_t  long_format;
			uint8_t  valid;
		      } *data_in;
                      char listcache_temp[] = "/tmp/listcache-XXXXXX";
		      FILE *temp_fp;
                      int temp_fd;

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
                      temp_fd = mkstemp(listcache_temp);
                      if (temp_fd == -1) {
                          LOG(0, ("Failed to create temporary file: %s\n", listcache_temp));
                          u.u_error = errno;
                          break;
                      }
                      temp_fp = fdopen(temp_fd, "w");
                      CODA_ASSERT(temp_fp != NULL);

		      v->ListCache(temp_fp, data_in->long_format, data_in->valid);
                      fflush(temp_fp);

#ifdef __CYGWIN32__
                      if (chown(listcache_temp, u.u_uid, -1) == -1)
#else
                      if (fchown(temp_fd, u.u_uid, -1) == -1)
#endif
                      {
                          LOG(0, ("Cannot chown file: %s\n", listcache_temp));
                          u.u_error = errno;
                      }
		      fclose(temp_fp);

                      data->out_size = strlen(listcache_temp)+1;
		      strcpy(data->out, listcache_temp);
		      break;
		    }
		    
		case _VIOCGETVOLSTAT:
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
		    unsigned int    age, hogtime;
		    int             conflict, cml_count;
		    uint64_t	    cml_bytes;
		    int		    local_only = 0;
		    if (data->in_size == sizeof(int) &&
			*(int *)data->in == 1)
			local_only = 1;

		    /* Retrieve the volume status from the server(s). */
		    u.u_error = v->GetVolStat(&volstat, &Name, &conn_state,
					      &age, &hogtime,
					      &conflict, &cml_count,
					      &cml_bytes,
					      &OfflineMsg, &MOTD, u.u_uid,
					      local_only);
		    if (u.u_error) break;

		    /* Format is (status, name, conn_state, conflict,
		       cml_count, offlinemsg, motd, age, hogtime) */
		    /* First we make sure we won't overflow data->out */
		    if ( (sizeof(VolumeStatus) + /* volstat */
			 strlen(name) + 1 + /* name */
			 sizeof(int) + /* conn_state */
			 sizeof(int) + /* conflict */
			 sizeof(int) + /* cml_count */
			 strlen(offlinemsg) + 1 + /* offlinemsg */
			 strlen(motd) + 1 + /* motd */
			 sizeof(unsigned int) + /* age */
			 sizeof(unsigned int) + /* hogtime */
			 sizeof(uint64_t))	/* cml_bytes */
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
		    strcpy(cp, name); cp += strlen(name) + 1;

		    /* do we have to worry about alignment? */
		    *(int32_t *)cp = (int32_t)conn_state; cp += sizeof(int32_t);
		    *(int32_t *)cp = (int32_t)conflict;   cp += sizeof(int32_t);
		    *(int32_t *)cp = (int32_t)cml_count;  cp += sizeof(int32_t);

		    strcpy(cp, offlinemsg); cp += strlen(offlinemsg) + 1;
		    strcpy(cp, motd); cp += strlen(motd) + 1;

		    *(uint32_t *)cp = (uint32_t)age;     cp += sizeof(uint32_t);
		    *(uint32_t *)cp = (uint32_t)hogtime; cp += sizeof(uint32_t);
		    *(uint64_t *)cp = (uint64_t)cml_bytes;
		    cp += sizeof(uint64_t);

		    data->out_size = (cp - data->out);
		    break;
		    }

		case _VIOCSETVOLSTAT:
		    {
		    /* Format is (status, name, offlinemsg, motd). */
		    char *cp = (char *) data->in;

		    /* Volume status block. */
		    VolumeStatus volstat;
		    memcpy(&volstat, cp, sizeof(VolumeStatus));
		    cp += sizeof(VolumeStatus);

		    /* Volume name. */
                    char name[V_MAXVOLNAMELEN];
		    unsigned int namelen = strlen(cp) + 1;
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
		    unsigned int offlinemsglen = strlen(cp) + 1;
		    if (offlinemsglen > 256) { u.u_error = EINVAL; break; }
		    strcpy(offlinemsg, cp);
		    RPC2_BoundedBS OfflineMsg;
		    OfflineMsg.SeqBody = (RPC2_ByteSeq)offlinemsg;
		    OfflineMsg.MaxSeqLen = 256;
		    OfflineMsg.SeqLen = offlinemsglen;
		    cp += offlinemsglen;

		    /* Message of the day for this volume. */
		    char motd[256];
		    unsigned int motdlen = strlen(cp) + 1;
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

		case _VIOCWHEREIS:
		    {
                        /* Extract the host array from the vsgent or volent as appropriate. */
                        v->GetHosts((struct in_addr *)data->out);
                        /* Extract an array if vids from the vsgent or volent as appropriate. */
			v->GetVids((VolumeId *)(data->out + (VSG_MEMBERS * sizeof(struct in_addr))));
                        data->out_size = VSG_MEMBERS * (sizeof(struct in_addr) + sizeof(VolumeId));
                        break;
		    }

		case _VIOC_FLUSHVOLUME:
		    {
		    /* This is drastic, but I'm having trouble getting rid of */
		    /* MiniCache vnodes that have the "wrong" type! -JJK */
		    (void)k_Purge();

		    FSDB->Flush(MakeVolid(fid));
		    Recov_SetBound(DMFP);

		    break;
		    }

		case _VIOC_GETSERVERSTATS:
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

		case _VIOC_CHECKPOINTML:
		    {
		    char *ckpdir = (data->in_size == 0 ? 0 : (char *) data->in);
                    u.u_error = EOPNOTSUPP;
                    if (v->IsReadWrite())
                        u.u_error = ((reintvol *)v)->CheckPointMLEs(u.u_uid, ckpdir);
		    break;
		    }

		case _VIOC_PURGEML:
		    {
                    u.u_error = EOPNOTSUPP;
                    if (v->IsReadWrite())
                        u.u_error = ((reintvol *)v)->PurgeMLEs(u.u_uid);
		    break;
		    }
		case _VIOC_WD:
		    {
		    /* 
		     * Begin logging mutations to this volume. 
		     * This is "pseudo-disconnected" mode, in which
		     * fetches may be performed but mutations are logged.
		     */
		    char *startp = (char *) data->in;
#define agep ((unsigned int *)(startp))
#define timep ((unsigned int *)(agep + 1))
                    u.u_error = EOPNOTSUPP;
                    if (v->IsReadWrite())
                        u.u_error = ((reintvol *)v)->WriteDisconnect(*agep, *timep); 
#undef timep
#undef agep
		    break;
		    }
		case _VIOC_ENABLEASR:
		    {			
                        u.u_error = EOPNOTSUPP;
                        if (v->IsReplicated())
                            u.u_error = ((repvol *)v)->AllowASR(u.u_uid);
			break;
		    }
		case _VIOC_DISABLEASR:
		    {
                        u.u_error = EOPNOTSUPP;
                        if (v->IsReplicated())
                            u.u_error = ((repvol *)v)->DisallowASR(u.u_uid);
			break;
		    }

		case _VIOC_SYNCCACHE:
			{
                  u.u_error = EOPNOTSUPP;
                  if (v->IsReadWrite()) {
		      v->Exit(volmode, u.u_uid);
		      entered = 0;
		      u.u_error = ((reintvol *)v)->SyncCache(NULL);
		  }
		  break;
              }

		case _VIOC_REDIR:
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

	    case _VIOC_REP_CMD:
	      {
		int rep_cmd;
		CODA_ASSERT(sscanf((char *) data->in, "%d", &rep_cmd) == 1);
		switch (rep_cmd) {
/*
  BEGIN_HTML
  <a name="beginrepair"><strong> beginrepair handler </strong></a>
  END_HTML
*/
		  case REP_CMD_BEGIN:
		    {
		      /* This ioctl only figures out what type of conflict
		       * we are dealing with and verifies the object is
		       * expanded correctly. No mutations are performed.
		       * The idea of 'beginning repair' is historical, and
		       * there is no problem calling this on the same directory
		       * or volume many times without an 'endrepair', or
		       * attempting any form of repair without calling this
		       * first. */
		      /*
		       *      1 - Local/Global repair session
		       *      2 - Server/Server repair session
		       *      3 - Both Local/Global and Server/Server
		       */

		      int code = -1, rc;
		      fsobj *dir = NULL, *localcache = NULL;
		      char *msg;

		      msg = (char *)data->out;
		      if(!msg) {
			LOG(0, ("REP_CMD_BEGIN: (%s) bad data->out parameter\n",
				FID_(fid)));
			code = -1;
			goto BEGIN_cleanup;
		      }

		      dir = FSDB->Find(fid);
		      if(!dir) {
			LOG(0, ("REP_CMD_BEGIN: (%s) <= this object missing!\n",
				FID_(fid)));
			code = -1;
			goto BEGIN_cleanup;
		      }

		      if(!dir->IsLocalObj()) {
			LOG(0, ("REP_CMD_BEGIN: (%s) not an expanded dir\n",
				FID_(fid)));
			code = -1;
			goto BEGIN_cleanup;
		      }

		      rc = dir->Lookup(&localcache, fid, LOCALCACHE_HIDDEN,
				       u.u_uid, CLU_CASE_SENSITIVE |
				       CLU_TRAVERSE_MTPT, 1);

		      if(!localcache)
			rc = dir->Lookup(&localcache, fid, LOCALCACHE,
					 u.u_uid, CLU_CASE_SENSITIVE |
					 CLU_TRAVERSE_MTPT, 1);
		      if(!localcache || (rc && rc != EINCONS)) {
			LOG(0, ("REP_CMD_BEGIN: (%s) failed finding localcache "
				"object.. bad news.\n", FID_(fid)));
			code = -1;
			goto BEGIN_cleanup;
		      }

		      if(localcache->IsFake())
			code = 2;

		      if(localcache->IsToBeRepaired()) {
			if(code == 2)
			  code = 3;
			else
			  code = 1;
		      }

		    BEGIN_cleanup:
		      if(localcache)
			FSDB->Put(&localcache);

		      sprintf(msg, "%d", code);
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
		      /*
		       * This ioctl literally does nothing. The only applicable
		       * use might be for unfreezing volumes if mechanisms
		       * are implemented to freeze state upon server-server
		       * conflict discovery.
		       *
		       * Even then, that might be better suited to the
		       * _VIOC_REPAIR ioctl, which figures out if any repair
		       * actually occurred and if it was successful.
		       */
		      char *msg;

		      msg = (char *)data->out;
		      if(!msg) {
			u.u_error = EINVAL;
			break;
		      }

		      u.u_error = EOPNOTSUPP;
		      if(v->IsReadWrite()) {
			sprintf(msg, "no action performed\n");
			u.u_error = 0;
		      }

		      data->out_size = (short)strlen(msg) + 1;
		      break;
		    }
/*
  BEGIN_HTML
  <a name="checklocal"><strong> checklocal handler </strong></a>
  END_HTML
*/
		  case REP_CMD_CHECK:
		  {
		    char *msg;
		    msg = (char *)data->out;
		    if(!msg)
		      u.u_error = EINVAL;
		    else
		      u.u_error = EOPNOTSUPP;
		    if(msg && v->IsReadWrite()) {
		      ClientModifyLog *cml = ((reintvol *)v)->GetCML();
		      cml->CheckCMLHead(msg);
		      u.u_error = 0;
		    }
		    else
		      sprintf(msg, "not a replicated volume\n");

		    data->out_size = (short)strlen(msg) + 1;
		    break;
		  }

/*
  BEGIN_HTML
  <a name="preservelocal"><strong> preservelocal handler </strong></a>
  END_HTML
*/
		case REP_CMD_PRESERVE:
		  {
		    char *msg;
		    msg = (char *)data->out;
		    if(!msg) {
		      u.u_error = EINVAL;
		      break;
		    }

		    msg[0] = '\0';
		    u.u_error = EOPNOTSUPP;
		    if(v->IsReadWrite()) {
		      ClientModifyLog *cml = ((reintvol *)v)->GetCML();
		      u.u_error = 0;
		      cml->PreserveLocalMutation(msg);
		    }

		    data->out_size = (short)strlen(msg) + 1;
		    break;
		  }

/*
  BEGIN_HTML
  <a name="preservealllocal"><strong> preservealllocal handler </strong></a>
  END_HTML
*/
		case REP_CMD_PRESERVE_ALL:
		  {
		    char *msg;
		    msg = (char *)data->out;
		    if(!msg) {
		      u.u_error = EINVAL;
		      break;
		    }

		    u.u_error = EOPNOTSUPP;
		    if(v->IsReadWrite()) {
		      ClientModifyLog *cml = ((reintvol *)v)->GetCML();
		      CODA_ASSERT(cml);
		      if(!cml) {
			sprintf(msg, "no client modify log on this volume\n");
			data->out_size = (short)strlen(msg) + 1;
			u.u_error = EINVAL;
			break;
		      }
		      u.u_error = 0;
		      cml->PreserveAllLocalMutation(msg);
		    }

		    data->out_size = (short)strlen(msg) + 1;
		    break;
		  }

/*
  BEGIN_HTML
  <a name="discardlocal"><strong> discardlocal handler </strong></a>
  END_HTML
*/
		case REP_CMD_DISCARD:
		  {
		    char *msg;
		    msg = (char *)data->out;
		    if(!msg) {
		      u.u_error = EINVAL;
		      break;
		    }

		    u.u_error = EOPNOTSUPP;
		    if(v->IsReadWrite()) {
		      ClientModifyLog *cml = ((reintvol *)v)->GetCML();
		      CODA_ASSERT(cml);
		      if(!cml) {
			sprintf(msg, "no client modify log on this volume\n");
			data->out_size = (short)strlen(msg) + 1;
			u.u_error = EINVAL;
			break;
		      }
		      u.u_error = cml->DiscardLocalMutation(msg);
		    }

		    data->out_size = (short)strlen(msg) + 1;
		    break;
		  }
/*
  BEGIN_HTML
  <a name="discardalllocal"><strong> discardalllocal handler </strong></a>
  END_HTML
*/
		case REP_CMD_DISCARD_ALL:
		  {
		    /* Not supported. Use PURGEML instead. */
		    data->out_size = 0;
		    u.u_error = EOPNOTSUPP;
		    break;
		  }
/*
  BEGIN_HTML
  <a name="listlocal"><strong> listlocal handler </strong></a>
  END_HTML
*/
		case REP_CMD_LIST:
		  {
		    if(!v->IsReadWrite()) {
                        u.u_error = EOPNOTSUPP;
                        break;
                    }

                    /* list local mutations belonging to this session */
                    char listlocal_temp[] = "/tmp/listlocal-XXXXXX";
                    int count;
		    FILE *temp_fp;
                    int temp_fd;

                    temp_fd = mkstemp(listlocal_temp);
                    if (temp_fd == -1) {
                        LOG(0, ("Failed to create temporary file: %s\n", listlocal_temp));
                        u.u_error = errno;
                        break;
                    }
                    temp_fp = fdopen(temp_fd, "w");
                    CODA_ASSERT(temp_fp != NULL);

                    ClientModifyLog *cml = ((reintvol *)v)->GetCML();
                    CODA_ASSERT(cml);

                    u.u_error = 0;
                    count = cml->ListCML(temp_fp);
                    fprintf(temp_fp, "%d entries total in the modify log for volume %s\n",
                            count, v->GetName());
                    fflush(temp_fp);

#ifdef __CYGWIN32__
                    if (chown(listlocal_temp, u.u_uid, -1) == -1)
#else
                    if (fchown(temp_fd, u.u_uid, -1) == -1)
#endif
                    {
                        LOG(0, ("Cannot chown file: %s\n", listlocal_temp));
                        u.u_error = errno;
                    }
                    fclose(temp_fp);

                    data->out_size = strlen(listlocal_temp)+1;
                    strcpy(data->out, listlocal_temp);
		    break;
		  }
		default:
		  eprint("bogus REP_CMD(%d)", rep_cmd);
		  break;
		}
	      }
	    }

	    V_FreeLocks:
	      if (entered) v->Exit(volmode, u.u_uid);
#ifdef TIMING
	      gettimeofday(&u.u_tv2, 0);
#endif
	      VDB->Put(&v);
	    if (u.u_error == ERETRY)
		  u.u_error = EWOULDBLOCK;
	    return;
	    }
	/* FS-based. */
	case _VIOCSETTOK:
	case _VIOCGETTOK:
	case _VIOCUNLOG:
	case _VIOCCKSERV:
	case _VIOCCKBACK:
	case _VIOC_VENUSLOG:
	case _VIOC_GETVENUSSTATS:
	case _VIOC_FLUSHCACHE:
	case _VIOC_HDB_ADD:
	case _VIOC_HDB_DELETE:
	case _VIOC_HDB_CLEAR:
	case _VIOC_HDB_LIST:
	case _VIOC_HDB_WALK:
	case _VIOC_HDB_VERIFY:
	case _VIOC_HDB_ENABLE:
	case _VIOC_HDB_DISABLE:
	case _VIOC_CLEARPRIORITIES:
	case _VIOC_WAITFOREVER:
	case _VIOC_GETPATH:
	case _VIOC_TRUNCATELOG:
	case _VIOC_DISCONNECT:
	case _VIOC_RECONNECT:
	case _VIOC_LISTCACHE:
	case _VIOC_GET_MT_PT:
	case _VIOC_WD_ALL:
	case _VIOC_SYNCCACHE_ALL:
	case _VIOC_UNLOADKERNEL:
	case _VIOC_LOOKASIDE:
        case _VIOC_ZLIMIT:
	    {
	    switch(nr) {
                case _VIOC_LOOKASIDE:
	            {
		      /* cache lookaside command (cfs lka) */
		      memset(data->out, 0, CFS_PIOBUFSIZE);
		      LKParseAndExecute((char *)data->in,  
                                  (char *)data->out, CFS_PIOBUFSIZE-1);
		      data->out_size = strlen(data->out) + 1; 
		      break; /* outmsg has success/failure info */

	            }
		case _VIOC_LISTCACHE:
	            {
		      /* List cache status */
		      struct listcache_in {
			uint8_t  long_format;
			uint8_t  valid;
		      } *data_in;
                      char listcache_temp[] = "/tmp/listcache-XXXXXX";
		      FILE *temp_fp;
                      int temp_fd;

		      /* Do ListCache */
		      data_in = (struct listcache_in *)data->in;
                      temp_fd = mkstemp(listcache_temp);
                      if (temp_fd == -1) {
                          LOG(0, ("Failed to create temporary file: %s\n", listcache_temp));
                          u.u_error = errno;
                          break;
                      }
                      temp_fp = fdopen(temp_fd, "w");
                      CODA_ASSERT(temp_fp != NULL);

		      VDB->ListCache(temp_fp, data_in->long_format, data_in->valid);
                      fflush(temp_fp);

#ifdef __CYGWIN32__
                      if (chown(listcache_temp, u.u_uid, -1) == -1)
#else
                      if (fchown(temp_fd, u.u_uid, -1) == -1)
#endif
                      {
                          LOG(0, ("Cannot chown file: %s\n", listcache_temp));
                          u.u_error = errno;
                      }
		      fclose(temp_fp);

                      data->out_size = strlen(listcache_temp)+1;
		      strcpy(data->out, listcache_temp);
		      break;
		    }
		case _VIOC_GET_MT_PT:
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
		
		case _VIOCSETTOK:
		    {
		    /* Format of data is (len, secret, len, clear) */
		    char *startp = (char *) data->in;
#define secretlen ((uint32_t *)(startp))
#define secretp ((SecretToken *)(secretlen + 1))
#define clearlen ((uint32_t *)(secretp + 1))
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

		case _VIOCGETTOK:
		    {
		    /* Format of data is (len, secret, len, clear) */
		    char *startp = (char *) data->out;
#define secretlen ((uint32_t *)(startp))
#define secretp ((SecretToken *)(secretlen + 1))
#define clearlen ((uint32_t *)(secretp + 1))
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

		case _VIOCUNLOG:
		    {
		    Realm *realm = REALMDB->GetRealm(data->in);
		    userent *ue = realm->GetUser(u.u_uid);
		    ue->Invalidate();
		    FSDB->ResetUser(u.u_uid);
		    PutUser(&ue);
		    realm->PutRef();

		    break;
		    }

		case _VIOCCKSERV:
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

		case _VIOCCKBACK:
/*		case _VIOCCKVOLS:*/
		    {
/*		    VDB->CheckVolumes();*/
		    FSDB->InvalidateMtPts();

		    break;
		    }

		case _VIOC_VENUSLOG:
		    {
		    if (u.u_uid != V_UID)
			{ u.u_error = EACCES; break; }

		    uint32_t on;
		    memcpy(&on, data->in, sizeof(uint32_t));
		    on &= 0xff;
		    if (on) DebugOn(); else DebugOff();

		    break;
		    }

		case _VIOC_GETVENUSSTATS:
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

		case _VIOC_FLUSHCACHE:
		    {
		    /* This is drastic, but I'm having trouble getting rid of */
		    /* MiniCache vnodes that have the "wrong" type! -JJK */
		    (void)k_Purge();

		    FSDB->Flush();
		    Recov_SetBound(DMFP);

		    break;
		    }

		case _VIOC_HDB_ADD:
		    {
		    struct hdb_add_msg *msgp = (struct hdb_add_msg *)data->in;
		    if (data->in_size != (int)sizeof(struct hdb_add_msg))
			{ u.u_error = EINVAL; break; }

		    u.u_error = HDBD_Request(HdbAdd, msgp, &u);

		    break;
		    }

		case _VIOC_HDB_DELETE:
		    {
		    struct hdb_delete_msg *msgp = (struct hdb_delete_msg *)data->in;
		    if (data->in_size != (int)sizeof(struct hdb_delete_msg))
			{ u.u_error = EINVAL; break; }

		    u.u_error = HDBD_Request(HdbDelete, msgp, &u);

		    break;
		    }

		case _VIOC_HDB_CLEAR:
		    {
		    struct hdb_clear_msg *msgp = (struct hdb_clear_msg *)data->in;
		    if (data->in_size != (int)sizeof(struct hdb_clear_msg))
			{ u.u_error = EINVAL; break; }

		    u.u_error = HDBD_Request(HdbClear, msgp, &u);

		    break;
		    }

		case _VIOC_HDB_LIST:
		    {
		    struct hdb_list_msg *msgp = (struct hdb_list_msg *)data->in;
		    if (data->in_size != (int)sizeof(struct hdb_list_msg))
			{ u.u_error = EINVAL; break; }

		    u.u_error = HDBD_Request(HdbList, msgp, &u);

		    break;
		    }

		case _VIOC_HDB_WALK:
		    {
		    struct hdb_walk_msg *msgp = (struct hdb_walk_msg *)data->in;
		    if (data->in_size != (int)sizeof(struct hdb_walk_msg))
			{ u.u_error = EINVAL; break; }

		    u.u_error = HDBD_Request(HdbWalk, msgp, &u);

		    break;
		    }


	        case _VIOC_HDB_VERIFY:
		    {
		    struct hdb_verify_msg *msgp = (struct hdb_verify_msg *)data->in;		    
		    if (data->in_size != (int)sizeof(struct hdb_verify_msg))
			{ u.u_error = EINVAL; break; }

		    u.u_error = HDBD_Request(HdbVerify, msgp, &u);

		    break;
		    }

		case _VIOC_HDB_ENABLE:
		    {
		    struct hdb_walk_msg *msgp = (struct hdb_walk_msg *)data->in;
		    if (data->in_size != (int)sizeof(struct hdb_walk_msg))
			{ u.u_error = EINVAL; break; }

		    u.u_error = HDBD_Request(HdbEnable, msgp, &u);

		    break;
		    }
		    
		case _VIOC_HDB_DISABLE:
		    {
		    struct hdb_walk_msg *msgp = (struct hdb_walk_msg *)data->in;
		    if (data->in_size != (int)sizeof(struct hdb_walk_msg))
			{ u.u_error = EINVAL; break; }

		    u.u_error = HDBD_Request(HdbDisable, msgp, &u);

		    break;
		    }
		    
		case _VIOC_CLEARPRIORITIES:
		    {
		    FSDB->ClearPriorities();

		    break;
		    }

		case _VIOC_WAITFOREVER:
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

		case _VIOC_GETPATH:
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

	        case _VIOC_TRUNCATELOG:
		    {
		    RecovFlush(1);
		    RecovTruncate(1);

		    break;
		    }

	        case _VIOC_DISCONNECT:
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

	        case _VIOC_RECONNECT:
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

		case _VIOC_WD_ALL:
		    {
		    char *startp = (char *) data->in;
#define agep ((unsigned int *)(startp))
#define timep ((unsigned int *)(agep + 1))
		    u.u_error = VDB->WriteDisconnect(*agep, *timep);
		    break;
		    }
		case _VIOC_SYNCCACHE_ALL:
		    {
		    u.u_error = VDB->SyncCache();
		    break;
		    }

		case _VIOC_ZLIMIT:
		    {
		    char *startp = (char *) data->in;
#define rzlimit ((int *)(startp))
#define yzlimit ((int *)(rzlimit + 1))
		    redzone_limit = *rzlimit;
		    yellowzone_limit = *yzlimit;
		    u.u_error = 0;
#undef rzlimit
#undef yzlimit
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
