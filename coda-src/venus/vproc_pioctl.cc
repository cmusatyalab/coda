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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/venus/vproc_pioctl.cc,v 4.6 1998/01/10 18:39:16 braam Exp $";
#endif /*_BLURB_*/







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
#endif __cplusplus

#include <stdio.h>
#include <errno.h>
#include <string.h>
#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif

#include <auth2.h>
#include <prs_fs.h>
#include <rpc2.h>
#include <fail.h>

#ifdef __cplusplus
}
#endif __cplusplus

/* interfaces */
#include <vice.h>

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
#include "advice_daemon.h"


/* local-repair modification */
void vproc::do_ioctl(ViceFid *fid, unsigned int com, struct ViceIoctl *data) {
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
	case VIOC_AFS_DELETE_MT_PT:
	case VIOC_AFS_STAT_MT_PT:
	case VIOC_GETFID:
        case VIOC_GETPFID:
	case VIOC_SETVV:
	    {
	    fsobj *f = 0;

	    int volmode = ((com == VIOCSETAL || com == VIOC_AFS_DELETE_MT_PT ||
			    com == VIOC_SETVV) ? VM_MUTATING : VM_OBSERVING);
	    for (;;) {
		Begin_VFS(fid->Volume, (int)VFSOP_IOCTL, volmode);
		if (u.u_error) break;

		u.u_error = FSDB->Get(&f, fid, CRTORUID(u.u_cred), RC_STATUS);
		if (u.u_error) goto O_FreeLocks;

		switch(com) {
		    case VIOCSETAL:
			{
			/* Verify that target is a directory. */
			if (!f->IsDir())
			    { u.u_error = ENOTDIR; break; }

			/* Verify that we have administer permission. */
			/* No.  This must be checked at the server to allow System:Administrators to */
			/* always change ACLs.  Alternatively, we could have venus cache the identity */
			/* of administrators. */
/*
			 u.u_error = f->Access((long)PRSFS_ADMINISTER, 0, CRTORUID(u.u_cred));
			 if (u.u_error) break;
*/

			/* Do the operation. */
			RPC2_CountedBS acl;
			acl.SeqLen = strlen(data->in) + 1;
			if (acl.SeqLen > V_MAXACLLEN)
			    { u.u_error = EINVAL; break; }
			acl.SeqBody = (RPC2_ByteSeq)data->in;
			f->PromoteLock();
			u.u_error = f->SetACL(&acl, CRTORUID(u.u_cred));

			break;
			}

		    case VIOCGETAL:
			{
			/* Verify that target is a directory. */
			if (!f->IsDir())
			    { u.u_error = ENOTDIR; break; }

			/* Verify that we have lookup permission. */
			/* No.  This must be checked at the server to allow System:Administrators to */
			/* always change ACLs.  Alternatively, we could have venus cache the identity */
			/* of administrators. */
/*
			 u.u_error = f->Access((long)PRSFS_LOOKUP, 0, CRTORUID(u.u_cred));
			 if (u.u_error) break;
*/

			/* Do the operation. */
			RPC2_BoundedBS acl;
			acl.MaxSeqLen = V_MAXACLLEN;
			acl.SeqLen = 0;
			acl.SeqBody = (RPC2_ByteSeq)data->out;
			acl.SeqBody[0] = 0;
			f->PromoteLock();
			u.u_error = f->GetACL(&acl, CRTORUID(u.u_cred));
			if (u.u_error) break;

			data->out_size = (short) (acl.SeqLen == 0 ? 1 : acl.SeqLen);
			break;
			}

		    case VIOCFLUSH:
			{
			FSDB->Put(&f);

			/* This is drastic, but I'm having trouble getting rid of */
			/* MiniCache vnodes that have the "wrong" type! -JJK */
			(void)k_Purge();

			f = FSDB->Find(fid);
			if (f != 0) {
			    u.u_error = f->Flush();
			    RecovSetBound(DMFP);
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
			    out->cfs_ioctl.len = 0;
			    out->cfs_ioctl.oh.result = 0;
			    w->Return(w->msg, sizeof (struct cfs_ioctl_out));
			}

			/* Release and reacquire the target (data this time). */
			FSDB->Put(&f);
			u.u_error = FSDB->Get(&f, fid, CRTORUID(u.u_cred), RC_DATA);

			break;
			}

		    case VIOC_AFS_DELETE_MT_PT:
			{
			fsobj *target_fso = 0;
			char *target_name = data->in;

			/* Disallow deletion of ".". */
			if (STREQ(target_name, "."))
			    { u.u_error = EINVAL; break; }

			/* Verify that parent is a directory. */
			if (!f->IsDir())
			    { u.u_error = ENOTDIR; break; }

			/* Get the target object. */
			u.u_error = f->Lookup(&target_fso, 0, target_name, CRTORUID(u.u_cred));
			if (u.u_error) break;

			/* Verify that target is a mount point (either valid or dangling). */
			if (!target_fso->IsMtPt() && !target_fso->IsMTLink())
			    { u.u_error = ENOTDIR; FSDB->Put(&target_fso); break; }

			/* Verify that we have delete permission for the parent. */
			u.u_error = f->Access((long)PRSFS_DELETE, 0, CRTORUID(u.u_cred));
			if (u.u_error) { FSDB->Put(&target_fso); break; }

			/* We only remove MTLinks, not valid MtPts! */
			if (target_fso->IsMtPt()) {
			    ATOMIC(
				fsobj *root_fso = target_fso->u.root;
				FSO_ASSERT(target_fso,
					   root_fso != 0 && root_fso->u.mtpoint == target_fso);
				root_fso->UnmountRoot();
				target_fso->UncoverMtPt();
			    , MAXFP)
			}

			/* Do the remove. */
			f->PromoteLock();
			target_fso->PromoteLock();
			u.u_error = f->Remove(data->in, target_fso, CRTORUID(u.u_cred));

			FSDB->Put(&target_fso);
			break;
			}

		    case VIOC_AFS_STAT_MT_PT:
			{
			fsobj *target_fso = 0;
			char *target_name = data->in;
			int out_size = 0;	/* needed since data->out_size is a short! */

			/* Verify that parent is a directory. */
			if (!f->IsDir())
			    { u.u_error = ENOTDIR; FSDB->Put(&target_fso); break; }

			/* Get the target object. */
			/* Take care against getting/putting object twice! */
			if (STREQ(target_name, ".")) {
			    target_fso = f;
			    f = 0;		/* Fake a FSDB->Put(&f); */
			}
			else {
			    u.u_error = f->Lookup(&target_fso, 0, target_name, CRTORUID(u.u_cred));
			    if (u.u_error) { FSDB->Put(&target_fso); break; }
			}

			/* Verify that target is a mount point (either valid or dangling). */
			if (!target_fso->IsMtPt() && !target_fso->IsMTLink())
			    { u.u_error = ENOTDIR; FSDB->Put(&target_fso); break; }

			/*Verify that we have read permission for it. */
			u.u_error = target_fso->Access((long)PRSFS_LOOKUP, 0,
						       CRTORUID(u.u_cred));
			if (u.u_error) { FSDB->Put(&target_fso); break; }

			/* Retrieve the link contents from the cache. */
			u.u_error = target_fso->Readlink(data->out, MAXPATHLEN,
							 &out_size, CRTORUID(u.u_cred));
			data->out_size = out_size;
			if (u.u_error) { FSDB->Put(&target_fso); break; }

			/* Make link a proper string. */
			data->out[data->out_size] = 0;
			(data->out_size)++;

			FSDB->Put(&target_fso);
			break;
			}

		    case VIOC_GETFID:
			{
			if (data->in_size != 0 && data->in_size != (int)sizeof(int))
			    { u.u_error = EINVAL; break; }
			if (data->in_size == (int)sizeof(int)) {
			    /* Backup and use volroot's mount point if directed. */
			    int backup;
			    bcopy((const void *)data->in, (void *) &backup, (int)sizeof(int));
			    if (backup) {
				if (f->fid.Volume == rootfid.Volume ||
				    f->fid.Vnode != ROOT_VNODE ||
				    f->fid.Unique != ROOT_UNIQUE ||
				    f->u.mtpoint == 0)
				    { u.u_error = EINVAL; break; }

				ViceFid mtptfid = f->u.mtpoint->fid;
				FSDB->Put(&f);
				u.u_error = FSDB->Get(&f, &mtptfid,
						      CRTORUID(u.u_cred), RC_STATUS);
				if (u.u_error) break;
			    }
			}

			char *cp = data->out;

			/* Copy out the fid. */
			bcopy((const void *)&f->fid, (void *) cp, (int)sizeof(ViceFid));
			cp += sizeof(ViceFid);

			/* Copy out the VV.  This will be garbage unless the object is replicated! */
			bcopy((const void *)&f->stat.VV, (void *) cp, (int)sizeof(ViceVersionVector));
			cp += sizeof(ViceVersionVector);

			data->out_size = (cp - data->out);
			break;
			}

		    case VIOC_GETPFID:
			{
			if (data->in_size != (int)sizeof(ViceFid))
			    { u.u_error = EINVAL; break; }
			ViceFid *fid = (ViceFid *)data->in;

			FSDB->Put(&f);
			u.u_error = FSDB->Get(&f, fid, CRTORUID(u.u_cred), RC_STATUS);
			if (u.u_error) break;

			/* if we're at a mount point, back up over it. */
			if (f->IsRoot() && !f->IsVenusRoot()) {
			    if (f->u.mtpoint == 0) 
				{ u.u_error = ENOENT; break; }

			    ViceFid mtptfid = f->u.mtpoint->fid;
			    FSDB->Put(&f);
			    u.u_error = FSDB->Get(&f, &mtptfid, CRTORUID(u.u_cred), RC_STATUS);
			    if (u.u_error) break;
		        }

			/* Copy out the parent fid. */
			bcopy((const void *)&f->pfid, (void *) data->out, (int)sizeof(ViceFid));
			data->out_size = (short)sizeof(ViceFid);

			break;
			}

		    case VIOC_SETVV:
			{
			if (data->in_size != (int)sizeof(ViceVersionVector))
			    { u.u_error = EINVAL; break; }

			f->PromoteLock();
			u.u_error = f->SetVV((ViceVersionVector *)data->in,
					     CRTORUID(u.u_cred));

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

        /* flushasr always fails to "Get" an object since it is inconsistent */
	/* therefore, we do a Find to get the pointer as opposed to the normal */
	/* object pioctls that Get the object */
        case VIOC_FLUSHASR:
	    {
		fsobj *f = FSDB->Find(fid);	/* note that find does no locking */
		if (f) {
		    /* ASR flush operation allowed only for files */
		    LOG(100, ("Going to reset lastresolved time for %x.%x.%x\n",
			      fid->Volume, fid->Vnode, fid->Unique));
		    u.u_error = f->SetLastResolved(0);
		}
	    }
	    return;
	    
	/* Volume-based. */
	case VIOCGETVOLSTAT:
	case VIOCSETVOLSTAT:
	case VIOCWHEREIS:
	case VIOC_FLUSHVOLUME:
	case VIOC_ENABLEREPAIR:
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
	    {
//#ifdef    TIMING
 	    if (!Simulating)
 		gettimeofday(&u.u_tv1, 0);
//#endif    TIMING
	    volent *v = 0;
	    if ((u.u_error = VDB->Get(&v, fid->Volume)) != 0) break;

	    int volmode = ((com == VIOC_REPAIR || com == VIOC_PURGEML) ? VM_MUTATING : VM_OBSERVING);
	    int entered = 0;
	    if ((u.u_error = v->Enter(volmode, CRTORUID(u.u_cred))) != 0) goto V_FreeLocks;
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
		    char name[32];
		    RPC2_BoundedBS Name;
		    Name.SeqBody = (RPC2_ByteSeq)name;
		    Name.MaxSeqLen = 32;
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

		    /* Retrieve the volume status from the server(s). */
		    u.u_error = v->GetVolStat(&volstat, &Name, &OfflineMsg,
					      &MOTD, CRTORUID(u.u_cred));
		    if (u.u_error) break;

		    /* Format is (status, name, offlinemsg, motd). */
		    char *cp = data->out;
		    bcopy((char *)&volstat, cp, (int)sizeof(VolumeStatus));
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

		case VIOCSETVOLSTAT:
		    {
		    /* Format is (status, name, offlinemsg, motd). */
		    char *cp = data->in;

		    /* Volume status block. */
		    VolumeStatus volstat;
		    bcopy((const void *)cp, (void *) (char *)&volstat, (int)sizeof(VolumeStatus));
		    cp += sizeof(VolumeStatus);

		    /* Volume name. */
		    char name[32];
		    unsigned long namelen = strlen(cp) + 1;
		    if (namelen > 32) { u.u_error = EINVAL; break; }
		    strcpy(name, cp);
		    RPC2_BoundedBS Name;
		    Name.SeqBody = (RPC2_ByteSeq)name;
		    Name.MaxSeqLen = 32;
		    Name.SeqLen = namelen;
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
		    {
			/* Hack!  Implement the "usecallback" toggle this cheap and dirty way! */
			if (STREQ(offlinemsg, "usecallback=1"))
			    { v->UseCallBack(1); break; }
			if (STREQ(offlinemsg, "usecallback=0"))
			    { v->UseCallBack(0); break; }
		    }
		    u.u_error = v->SetVolStat(&volstat, &Name, &OfflineMsg,
					      &MOTD, CRTORUID(u.u_cred));
		    if (u.u_error) break;

		    /* Copy all the junk back out. */
		    /* Format is (status, name, offlinemsg, motd). */
		    cp = data->out;
		    bcopy((char *)&volstat, cp, (int)sizeof(VolumeStatus));
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
		    v->GetHosts((unsigned long *)data->out);

		    data->out_size = MAXHOSTS * (short)sizeof(unsigned long);
		    break;
		    }

		case VIOC_FLUSHVOLUME:
		    {
		    /* This is drastic, but I'm having trouble getting rid of */
		    /* MiniCache vnodes that have the "wrong" type! -JJK */
		    (void)k_Purge();

		    FSDB->Flush(fid->Volume);
		    RecovSetBound(DMFP);

		    break;
		    }

/*
  BEGIN_HTML
  <a name="beginrepair"><strong> beginrepair handler </strong></a>
  END_HTML
*/
		case VIOC_ENABLEREPAIR:
		    {
		    /* Try to enable target volume for repair by this user. */
#define	startp	    (data->out)
#define	RWVols	    ((VolumeId *)(startp))
#define	LockUids    ((vuid_t *)(RWVols + MAXHOSTS))
#define	LockWSs	    ((unsigned long *)(LockUids + MAXHOSTS))
#define	endp	    ((char *)(LockWSs + MAXHOSTS))
		    u.u_error = v->EnableRepair(CRTORUID(u.u_cred), RWVols,
						LockUids, LockWSs);

		    data->out_size = (endp - startp);
#undef	startp
#undef	RWVols
#undef	LockUids
#undef	LockWSs
#undef	endp

		    /* This is drastic, but I'm having trouble getting rid of */
		    /* MiniCache vnodes that have the "wrong" type! -JJK */
		    (void)k_Purge();

		    break;
		    }
/*
  BEGIN_HTML
  <a name="quit"><strong> quit handler </strong></a>
  END_HTML
*/
		case VIOC_DISABLEREPAIR:
		    {
		    /* Disable repair of target volume by this user. */
		    u.u_error = v->DisableRepair(CRTORUID(u.u_cred));

		    /* This is drastic, but I'm having trouble getting rid of */
		    /* MiniCache vnodes that have the "wrong" type! -JJK */
		    (void)k_Purge();

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
#define	RepairFile  (data->in)
#define	startp	    (data->out)
#define	RWVols	    ((VolumeId *)(startp))
#define	ReturnCodes ((int *)(RWVols + MAXHOSTS))
#define	endp	    ((char *)(ReturnCodes + MAXHOSTS))
		    data->out_size = (endp - startp);
		    u.u_error = v->Repair(fid, RepairFile,
					  CRTORUID(u.u_cred), RWVols, ReturnCodes);

 	            LOG(0, ("MARIA: VIOC_REPAIR calls volent::Repair which returns %d\n",u.u_error));
		    if ((AdviceEnabled) && (u.u_error == 0)) {
 		        /* We don't have the object so can't provide a pathname */
		        NotifyUsersObjectConsistent("???UNKNOWN???",fid);
		    }

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
		    unsigned long Hosts[MAXHOSTS];
		    v->GetHosts(Hosts);

		    /* Count number of hosts to make sure buffer size wouldn't be exceeded. */
		    int i;
		    int nHosts = 0;
		    for (i = 0; i < MAXHOSTS; i++)
			if (Hosts[i] != 0) nHosts++;
		    if (nHosts * (int)sizeof(ViceStatistics) > VC_MAXDATASIZE)
			{ u.u_error = EINVAL; break; }

		    /* Get statistics from each host. */
		    /* OUT data for hosts that are incommunicado will be zero. */
		    bzero(data->out, nHosts * (int)sizeof(ViceStatistics));
		    ViceStatistics *Stats = (ViceStatistics *)data->out;
		    for (i = 0; i < MAXHOSTS; i++)
			if (Hosts[i] != 0) {
			    srvent *s;
			    GetServer(&s, Hosts[i]);
			    (void)s->GetStatistics(Stats);
			    Stats++;
			}

		    data->out_size = (char *)Stats - data->out;
		    break;
		    }

		case VIOC_CHECKPOINTML:
		    {
		    char *ckpdir = (data->in_size == 0 ? 0 : data->in);
		    u.u_error = v->CheckPointMLEs(CRTORUID(u.u_cred), ckpdir);
		    break;
		    }

		case VIOC_PURGEML:
		    {
		    u.u_error = v->PurgeMLEs(CRTORUID(u.u_cred));
		    break;
		    }
     	        case VIOC_BEGINML:
		    {
		    /* 
		     * Begin logging mutations to this volume. 
		     * This is "pseudo-disconnected" mode, in which
		     * fetches may be performed but mutations are logged.
		     */
		    char *startp = data->in;
#define agep ((unsigned *)(startp))
#define timep ((unsigned *)(agep + 1))
		    u.u_error = v->WriteDisconnect(*agep, *timep); 
		    if (u.u_error == 0) 
			eprint("Logging updates to volume %s", v->name);

		    break;
		    }
   	        case VIOC_ENDML:
		    {
		    /* 
		     * Stop logging mutations to this volume, and
		     * schedule it for reintegration if appropriate.
		     */
		    u.u_error = v->WriteReconnect();    
		    if (u.u_error == 0) 
			eprint("Propagating updates to volume %s (|CML| = %d)", 
			       v->name, v->CML.count());
		    break;
		    }
	      case VIOC_ENABLEASR:
		    {			
			u.u_error = v->EnableASR(CRTORUID(u.u_cred));
			break;
		    }
	      case VIOC_DISABLEASR:
		    {
			u.u_error = v->DisableASR(CRTORUID(u.u_cred));
			break;
		    }
	    }

V_FreeLocks:
	    if (entered) v->Exit(volmode, CRTORUID(u.u_cred));
 	    float elapsed = 0.0;
#ifdef	    TIMING
 	    
 	    if (!Simulating) {
 		gettimeofday(&u.u_tv2, 0);
 		elapsed = SubTimes(u.u_tv2, u.u_tv1);
 	    }
#endif      TIMING
 	    /* Hack to include this as an ioctl request in the proper vsr. */
	    if (v->vid != LocalFakeVid) {
		vsr *vsr = v->GetVSR(CRTORUID(u.u_cred));
		vsr->RecordEvent((int)VFSOP_TO_VSE(VFSOP_IOCTL),  u.u_error, (RPC2_Unsigned) elapsed);
		v->PutVSR(vsr);
	    }
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
	case VIOCGETTIME:
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
	case VIOC_BWHINT:
	case VIOC_LISTCACHE:
	case VIOC_GET_MT_PT:
        case VIOC_WD_ALL:
	case VIOC_WR_ALL:
	case VIOC_REP_CMD:
	    {
	    switch(com) {
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
		      if (data->in_size != (int)sizeof(VolumeId)) {
			u.u_error = EINVAL; break;
		      }
		      VolumeId *vol_id = (VolumeId *)data->in;
		      volent *vv = VDB->Find(*vol_id);
		      if (vv == 0) {
			MarinerLog("Could not find volume = %x\n", *vol_id);
			u.u_error = EINVAL;
			break;
		      }
		      vv->GetMountPath((char *)data->out, 0);
		      if (strcmp((char *)data->out, "???") == 0) {
			MarinerLog("Could not get mount point path for %x\n",
				   *vol_id);
			u.u_error = EINVAL;
			break;
		      }
		      data->out_size = (short) strlen((char *)data->out)+1;
		      break;
		    }
		
		case VIOCSETTOK:
		    {
		    /* Format of data is (len, secret, len, clear) */
		    char *startp = data->in;
#define secretlen ((long *)(startp))
#define secretp ((SecretToken *)(secretlen + 1))
#define clearlen ((long *)(secretp + 1))
#define clearp ((ClearToken *)(clearlen + 1))
#define endp ((char *)(clearp + 1))
/*
		    if (*secretlen != (int)sizeof(SecretToken) ||
			*clearlen != (int)sizeof(ClearToken))
			{ u.u_error = EINVAL; break; }
*/
		    userent *ue;
		    GetUser(&ue, CRTORUID(u.u_cred));
		    u.u_error = ue->SetTokens(secretp, clearp)
		      ? 0
		      : EPERM;
		    PutUser(&ue);
#undef	secretlen
#undef	secretp
#undef	clearlen
#undef	clearp
#undef	endp

		    break;
		    }

		case VIOCGETTOK:
		    {
		    /* Format of data is (len, secret, len, clear) */
		    char *startp = data->out;
#define secretlen ((long *)(startp))
#define secretp ((SecretToken *)(secretlen + 1))
#define clearlen ((long *)(secretp + 1))
#define clearp ((ClearToken *)(clearlen + 1))
#define endp ((char *)(clearp + 1))
		    userent *ue;
		    GetUser(&ue, CRTORUID(u.u_cred));
		    u.u_error = (int) ue->GetTokens(secretp, clearp);
		    PutUser(&ue);
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
		    userent *ue;
		    GetUser(&ue, CRTORUID(u.u_cred));
		    ue->Invalidate();
		    PutUser(&ue);

		    break;
		    }

		case VIOCCKSERV:
		    {
		    int bufsize = 2048;		/* XXX - IN/OUT parameter. */
		    if (data->in_size == 0) {   /* probe everybody we know */
			ServerProbe();
			DownServers(data->out, &bufsize);
		    } else {
			/* probe only those listed. */
			/* format is #hosts, hostaddr, hostaddr, ... */
#define nservers ((int *)(data->in))
#define hostids ((unsigned long *)(nservers + 1))
			DoProbes(*nservers, hostids);
			DownServers(*nservers, hostids, data->out, &bufsize);
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

		case VIOCGETTIME:
		    {
		    long secs, usecs;
		    u.u_error = GetTime(&secs, &usecs);

		    break;
		    }

		case VIOC_VENUSLOG:
		    {
		    if (CRTORUID(u.u_cred) != V_UID)
			{ u.u_error = EACCES; break; }

		    long on;
		    bcopy((const void *)data->in, (void *) &on, (int)sizeof(long));
		    on &= 0xff;
		    if (on) DebugOn(); else DebugOff();

		    break;
		    }

		case VIOC_GETVENUSSTATS:
		    {
		    if (sizeof(VenusStatistics) > VC_MAXDATASIZE)
			{ u.u_error = EINVAL; break; }

		    VenusStatistics *Stats = (VenusStatistics *)data->out;
		    bzero((void *)Stats, (int)sizeof(VenusStatistics));
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
		    RecovSetBound(DMFP);

		    break;
		    }

		case VIOC_HDB_ADD:
		    {
		    struct hdb_add_msg *msgp = (struct hdb_add_msg *)data->in;
		    if (data->in_size != (int)sizeof(struct hdb_add_msg))
			{ u.u_error = EINVAL; break; }

		    u.u_error = HDBD_Request(HdbAdd, msgp,
					     CRTOEUID(u.u_cred), CRTORUID(u.u_cred));

		    break;
		    }

		case VIOC_HDB_DELETE:
		    {
		    struct hdb_delete_msg *msgp = (struct hdb_delete_msg *)data->in;
		    if (data->in_size != (int)sizeof(struct hdb_delete_msg))
			{ u.u_error = EINVAL; break; }

		    u.u_error = HDBD_Request(HdbDelete, msgp,
					     CRTOEUID(u.u_cred), CRTORUID(u.u_cred));

		    break;
		    }

		case VIOC_HDB_CLEAR:
		    {
		    struct hdb_clear_msg *msgp = (struct hdb_clear_msg *)data->in;
		    if (data->in_size != (int)sizeof(struct hdb_clear_msg))
			{ u.u_error = EINVAL; break; }

		    u.u_error = HDBD_Request(HdbClear, msgp,
					     CRTOEUID(u.u_cred), CRTORUID(u.u_cred));

		    break;
		    }

		case VIOC_HDB_LIST:
		    {
		    struct hdb_list_msg *msgp = (struct hdb_list_msg *)data->in;
		    if (data->in_size != (int)sizeof(struct hdb_list_msg))
			{ u.u_error = EINVAL; break; }

		    u.u_error = HDBD_Request(HdbList, msgp,
					     CRTOEUID(u.u_cred), CRTORUID(u.u_cred));

		    break;
		    }

		case VIOC_HDB_WALK:
		    {
		    struct hdb_walk_msg *msgp = (struct hdb_walk_msg *)data->in;
		    if (data->in_size != (int)sizeof(struct hdb_walk_msg))
			{ u.u_error = EINVAL; break; }

		    u.u_error = HDBD_Request(HdbWalk, msgp,
					     CRTOEUID(u.u_cred), CRTORUID(u.u_cred));

		    break;
		    }


	        case VIOC_HDB_VERIFY:
		    {
		    struct hdb_verify_msg *msgp = (struct hdb_verify_msg *)data->in;		    
		    if (data->in_size != (int)sizeof(struct hdb_verify_msg))
			{ u.u_error = EINVAL; break; }

		    u.u_error = HDBD_Request(HdbVerify, msgp,
					     CRTOEUID(u.u_cred), CRTORUID(u.u_cred));

		    break;
		    }

		case VIOC_HDB_ENABLE:
		    {
		    struct hdb_walk_msg *msgp = (struct hdb_walk_msg *)data->in;
		    if (data->in_size != (int)sizeof(struct hdb_walk_msg))
			{ u.u_error = EINVAL; break; }

		    u.u_error = HDBD_Request(HdbEnable, msgp,
					     CRTOEUID(u.u_cred), CRTORUID(u.u_cred));

		    break;
		    }
		    
		case VIOC_HDB_DISABLE:
		    {
		    struct hdb_walk_msg *msgp = (struct hdb_walk_msg *)data->in;
		    if (data->in_size != (int)sizeof(struct hdb_walk_msg))
			{ u.u_error = EINVAL; break; }

		    u.u_error = HDBD_Request(HdbDisable, msgp,
					     CRTOEUID(u.u_cred), CRTORUID(u.u_cred));

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
		    bcopy((const void *)data->in, (void *) &on, (int)sizeof(int));

		    /* We would like "waitforever" behavior to be settable on a per-process group basis. */
		    /* However, this would require cooperation with the kernel, which I don't want to */
		    /* mess with now.  So instead, we will set it on a per-user basis (at least for now). */
		    userent *ue;
		    GetUser(&ue, CRTORUID(u.u_cred));
		    ue->SetWaitForever(on);
		    PutUser(&ue);

		    break;
		    }

		case VIOC_GETPATH:
		    {
		    if (data->in_size != (int)sizeof(ViceFid))
			{ u.u_error = EINVAL; break; }
		    ViceFid *fid = (ViceFid *)data->in;
		    int	out_size = MAXPATHLEN;	    /* needed since data->out_size is a short! */
		    GetPath(fid, data->out, &out_size, 0);
		    data->out_size = out_size;

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
#define hostids ((unsigned long *)(nservers + 1))
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
#define hostids ((unsigned long *)(nservers + 1))
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

	        case VIOC_BWHINT:
		    {
		    srv_iterator next;
		    srvent *s;

		    if (data->in_size != (int)sizeof(long))
			{ u.u_error = EINVAL; break; }
		    
		    /* this should go away */
		    rpc2_Bandwidth = *((unsigned *)data->in);
		    
		    while (s = next()) {
			/* need bytes/sec */
			if (s->InitBandwidth(*((long *)data->in) >> 3)) {
			    u.u_error = EINVAL;
			    break;
			}
		    }
		    
		    break;
		    }
		case VIOC_WD_ALL:
		    {
		    char *startp = data->in;
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
			ASSERT(sscanf(data->in, "%d", &rep_cmd) == 1);
			switch (rep_cmd) {
			case REP_CMD_BEGIN:
			    {
				/* begin a repair session */
				int mode, dummy;
				fsobj *obj = FSDB->Find(fid);
				ASSERT(obj);
				sscanf(data->in, "%d %d", &dummy, &mode);
				LRDB->BeginRepairSession(&obj->fid,
							 mode ? REP_SCRATCH_MODE : REP_DIRECT_MODE, 
							 data->out);
				data->out_size = (short)sizeof(data->out);
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
				ViceFid *squirrelFid;

				ASSERT(LRDB);
				squirrelFid = LRDB->RFM_LookupGlobalRoot(LRDB->repair_root_fid);

				sscanf(data->in, "%d %d", &dummy, &commit);
				LRDB->EndRepairSession(commit, data->out);
				data->out_size = (short)strlen(data->out) + 1;
				u.u_error = 0;
				if (strncmp(data->out, "repair session completed", strlen("repair session completed")) == 0) {

				  LOG(0, ("MARIA:  End local repair successful\n"));
				  if (AdviceEnabled) {
				    /* We don't have the object so can't provide a pathname */
				    NotifyUsersObjectConsistent("???UNKNOWN???", squirrelFid);
				  }
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
				LRDB->ContinueRepairSession(data->out);
				data->out_size = (short)strlen(data->out) + 1;
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
				LRDB->PreserveLocalMutation(data->out);
				data->out_size = (short)strlen(data->out) + 1;
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
				LRDB->PreserveAllLocalMutation(data->out);
				data->out_size = (short)strlen(data->out) + 1;
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
				LRDB->DiscardLocalMutation(data->out);
				data->out_size = (short)strlen(data->out) + 1;
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
				LRDB->DiscardAllLocalMutation(data->out);
				data->out_size = (short)strlen(data->out) + 1;
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
				char fpath[MAXNAMELEN];
				sscanf(data->in, "%d %s", &dummy, fpath);
				if (LRDB->repair_root_fid == NULL) {
				    sprintf(data->out, "no repair session going on\n");
				    u.u_error = ENOENT;
				} else {
				    FILE *fp = fopen(fpath, "w");
				    if (fp == NULL) {
					u.u_error = ENOENT;
					sprintf(data->out, "can not open %s\n", fpath);
				    } else {
					LRDB->ListCML(LRDB->repair_root_fid, fp);
					sprintf(data->out, "local mutations are:\n");
					fflush(fp);
					fclose(fp);
					u.u_error = 0;
				    }
				}
				data->out_size = (short)strlen(data->out) + 1;
				break;
			    }
/*
  BEGIN_HTML
  <a name="setlocalview"><strong> setlocalview handler </strong></a>
  END_HTML
*/
			case REP_CMD_LOCAL_VIEW:
			    {
				LRDB->SetSubtreeView(SUBTREE_LOCAL_VIEW, data->out);
				data->out_size = (short)strlen(data->out) + 1;
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
				LRDB->SetSubtreeView(SUBTREE_GLOBAL_VIEW, data->out);
				data->out_size = (short)strlen(data->out) + 1;
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
				LRDB->SetSubtreeView(SUBTREE_MIXED_VIEW, data->out);
				data->out_size = (short)strlen(data->out) + 1;
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
