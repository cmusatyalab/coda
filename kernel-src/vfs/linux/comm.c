/* 
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */

/*
 * This code was written for the Coda file system at Carnegie Mellon
 * University.  Contributers include David Steere, James Kistler, and
 * M. Satyanarayanan.  */

/*
 * HISTORY
 * cfs_subr.c,v
 * Revision 1.2  1996/01/02 16:57:01  bnoble
 * Added support for Coda MiniCache and raw inode calls (final commit)
 *
 * Revision 1.1.2.1  1995/12/20 01:57:27  bnoble
 * Added CFS-specific files
 *
 * Revision 3.1.1.1  1995/03/04  19:07:59  bnoble
 * Branch for BSD port revisions
 *
 * Revision 3.1  1995/03/04  19:07:58  bnoble
 * Bump to major revision 3 to prepare for BSD port
 *
 * Revision 2.8  1995/03/03  17:00:04  dcs
 * Fixed kernel bug involving sleep and upcalls. Basically if you killed
 * a job waiting on venus, the venus upcall queues got trashed. Depending
 * on luck, you could kill the kernel or not.
 * (mods to cfs_subr.c and cfs_mach.d)
 *
 * Revision 2.7  95/03/02  22:45:21  dcs
 * Sun4 compatibility
 * 
 * Revision 2.6  95/02/17  16:25:17  dcs
 * These versions represent several changes:
 * 1. Allow venus to restart even if outstanding references exist.
 * 2. Have only one ctlvp per client, as opposed to one per mounted cfs device.d
 * 3. Allow ody_expand to return many members, not just one.
 * 
 * Revision 2.5  94/11/09  15:56:26  dcs
 * Had the thread sleeping on the wrong thing!
 * 
 * Revision 2.4  94/10/14  09:57:57  dcs
 * Made changes 'cause sun4s have braindead compilers
 * 
 * Revision 2.3  94/10/12  16:46:26  dcs
 * Cleaned kernel/venus interface by removing XDR junk, plus
 * so cleanup to allow this code to be more easily ported.
 * 
 * Revision 1.2  92/10/27  17:58:22  lily
 * merge kernel/latest and alpha/src/cfs
 * 
 * Revision 2.4  92/09/30  14:16:26  mja
 * 	Incorporated Dave Steere's fix for the GNU-Emacs bug.
 * 	Also, included his cfs_flush routine in place of the former cfsnc_flush.
 * 	[91/02/07            jjk]
 * 
 * 	Added contributors blurb.
 * 	[90/12/13            jjk]
 * 
 * 	Hack to allow users to keep coda venus calls uninterruptible. THis
 * 	basically prevents the Gnu-emacs bug from appearing, in which a call
 * 	was being interrupted, and return EINTR, but gnu didn't check for the
 * 	error and figured the file was buggered.
 * 	[90/12/09            dcs]
 * 
 * Revision 2.3  90/08/10  10:23:20  mrt
 * 	Removed include of vm/vm_page.h as it no longer exists.
 * 	[90/08/10            mrt]
 * 
 * Revision 2.2  90/07/05  11:26:35  mrt
 * 	Initialize name cache on first call to vcopen.
 * 	[90/05/23            dcs]
 * 
 * 	Created for the Coda File System.
 * 	[90/05/23            dcs]
 * 
 * Revision 1.5  90/05/31  17:01:35  dcs
 * Prepare for merge with facilities kernel.
 * 
 * Revision 1.2  90/03/19  15:56:25  dcs
 * Initialize name cache on first call to vcopen.
 * 
 * Revision 1.1  90/03/15  10:43:26  jjk
 * Initial revision
 * 
 */ 

/* @(#)cfs_subr.c	1.5 87/09/14 3.2/4.3CFSSRC */


#include <linux/sched.h>
#include <linux/lp.h>
#include <linux/malloc.h>
#include <linux/ioport.h>
#include <linux/fcntl.h>
#include <linux/delay.h>
#include <linux/skbuff.h>
#include <linux/proc_fs.h>

#include "linux/coda.h"
#include "cfs_linux.h"
#include "psdev.h"
#include "cnode.h"
#include "super.h" 
#include "namecache.h"


struct vcomm *coda_psdev_vcomm(struct inode *psdev);
int coda_downcall(int opcode, union outputArgs *out);
int coda_upcall(struct coda_sb_info *mntinfo, int inSize,int *outSize, caddr_t buffer);

extern int coda_debug;
extern int coda_print_entry;

#ifdef	__linux__
#define __P(ARG) ARG
#endif




/* 
 * Key question: whether to sleep interuptably or uninteruptably when
 * waiting for Venus.  The former seems better (cause you can ^C a
 * job), but then GNU-EMACS completion breaks. Use tsleep with no
 * timeout, and no longjmp happens. But, when sleeping
 * "uninterruptibly", we don't get told if it returns abnormally
 * (e.g. kill -9).  
 */


int coda_upcall(mntinfo, inSize, outSize, buffer) 
     struct coda_sb_info  *mntinfo; 
     int inSize; 
     int *outSize; 
     caddr_t buffer;
{
	union outputArgs *out;
	struct vcomm *vcommp;
	struct vmsg *vmp;
	int error = 0;

        /*        ENTRY;  */

	if (mntinfo == NULL) {
         /* Unlikely, but could be a race condition with a dying warden */
                return ENODEV;
	}
	vcommp = coda_psdev_vcomm(mntinfo->s_psdev);
	/*
	cfs_clstat.ncalls++;
	cfs_clstat.reqs[((struct inputArgs *)buffer)->opcode]++;
        */
	if (!VC_OPEN(vcommp))
          return(ENODEV);

	/* Format the request message. */
	CODA_ALLOC(vmp,struct vmsg *,sizeof(struct vmsg));
	vmp->vm_data = buffer;
	vmp->vm_flags = 0;
	vmp->vm_inSize = inSize;
	vmp->vm_outSize 
	    = *outSize ? *outSize : inSize; /* |buffer| >= inSize */
	vmp->vm_opcode = ((union inputArgs *)buffer)->ih.opcode;
	vmp->vm_unique = ++vcommp->vc_seq;
        vmp->vm_sleep = NULL;
	
	/* Fill in the common input args. */
	((union inputArgs *)buffer)->ih.unique = vmp->vm_unique;

	/* Append msg to request queue and poke Venus. */

	INSQUE(vmp->vm_chain, vcommp->vc_requests);
CDEBUG(D_UPCALL, "about to wake up Venus and sleep for (process, opc, uniq) =(%d, %d.%d)\n", current->pid, vmp->vm_opcode, vmp->vm_unique);

         SELWAKEUP(vcommp->vc_selproc);
	/* We can be interrupted while we wait for Venus to process
	 * our request.  If the interrupt occurs before Venus has read
	 * the request, we dequeue and return. If it occurs after the
	 * read but before the reply, we dequeue, send a signal
	 * message, and return. If it occurs after the reply we ignore
	 * it. In no case do we want to restart the syscall.  If it
	 * was interrupted by a venus shutdown (vcclose), return
	 * ENODEV.  */

	/* Ignore return, We have to check anyway */


	SLEEP(&vmp->vm_sleep);
CDEBUG(D_UPCALL, "process %d woken up by Venus.\n", current->pid);
	if (VC_OPEN(vcommp)) {	/* Venus is still alive */
	    /* Op went through, interrupt or not... */
	    if (vmp->vm_flags & VM_WRITE) {
		error = 0;
		*outSize = vmp->vm_outSize;
		out = (union outputArgs *)vmp->vm_data;
		error = out->oh.result;
		CDEBUG(D_UPCALL, 
		       "upcall: (u,o,r) (%ld, %ld, %ld) out at %x\n", 
		       out->oh.unique, out->oh.opcode, out->oh.result, (int)out);
	    } else if (!(vmp->vm_flags & VM_READ)) { 
		/* Interrupted before venus read it. */
		printk("coda_upcall: interrupted before read: (op,un)  (%d.%d), flags = %x\n",
		       vmp->vm_opcode, vmp->vm_unique, vmp->vm_flags);
		REMQUE(vmp->vm_chain);
		error = ERESTARTSYS;
	    } else { 	
		/* (!(vmp->vm_flags & VM_WRITE)) means interrupted after
                   upcall started */
		/* Interrupted after start of upcall, send venus a signal */
		union inputArgs *dog;
		struct vmsg *svmp;
		
		CDEBUG(D_UPCALL, "Sending Venus a signal:  op = %d.%d, flags = %x\n",
			   vmp->vm_opcode, vmp->vm_unique, vmp->vm_flags);
		
		REMQUE(vmp->vm_chain);
		error = ERESTARTSYS;
		
		CODA_ALLOC(svmp, struct vmsg *, sizeof (struct vmsg));

		CODA_ALLOC((svmp->vm_data), char *, sizeof(struct cfs_in_hdr));
		dog = (union inputArgs *)svmp->vm_data;
		
		svmp->vm_flags = 0;
		dog->ih.opcode = svmp->vm_opcode = CFS_SIGNAL;
		dog->ih.unique = svmp->vm_unique = vmp->vm_unique;
		svmp->vm_inSize = sizeof(struct cfs_in_hdr);
		svmp->vm_outSize = sizeof(struct cfs_in_hdr);
		
		CDEBUG(D_UPCALL, "coda_upcall: enqueing signal msg (%d, %d)\n",
			   svmp->vm_opcode, svmp->vm_unique);
		
		/* insert at head of queue! */
		INSQUE(svmp->vm_chain, vcommp->vc_requests);
		SELWAKEUP(vcommp->vc_selproc);
	    }
	} else {	/* If venus died (!VC_OPEN(vcommp)) */
	        printk("coda_upcall: Venus dead upon (op,un) (%d.%d) flags %d\n",
		   vmp->vm_opcode, vmp->vm_unique, vmp->vm_flags);
	    
		error = ENODEV;
	}

	CODA_FREE(vmp, sizeof(struct vmsg));
	return(error);
}




/*
 * There are 7 cases where invalidations occur. The semantics of each
 * is listed here.
 *
 * CFS_FLUSH     -- flush all entries from the name cache and the cnode cache.
 * CFS_PURGEUSER -- flush all entries from the name cache for a specific user
 *                  This call is a result of token expiration.
 *                  Linux does a cfsnc_flush since cred's are not maintained.
 *
 * The next arise as the result of callbacks on a file or directory.
 * CFS_ZAPDIR    -- flush the attributes for the dir from its cnode.
 *                  Zap all children of this directory from the namecache.
 * CFS_ZAPFILE   -- flush the cached attributes for a file.
 * CFS_ZAPVNODE  -- in linux the same as zap file (no creds).
 *
 * The next is a result of Venus detecting an inconsistent file.
 * CFS_PURGEFID  -- flush the attribute for the file
 *                  If it is a dir (odd vnode), purge its 
 *                  children from the namecache
 *                  remove the file from the namecache.
 *
 * The last  allows Venus to replace local fids with global ones
 * during reintegration.
 *
 * CFS_REPLACE -- replace one ViceFid with another throughout the name cache 
 */

int coda_downcall(int opcode, union outputArgs *out)
{

    /* Handle invalidate requests. */
    switch (opcode) {
      case CFS_FLUSH : {
	cfsnc_flush();
	return(0);
      }
      case CFS_PURGEUSER : {
	cfsnc_flush();
	return(0);
      }
      case CFS_ZAPDIR : {
	      ViceFid *fid = &out->cfs_zapdir.CodaFid;
	      cfsnc_zapfid(fid);
	      cfsnc_zapParentfid(fid);     
	      CDEBUG(D_UPCALL, "zapdir: fid = (%lx.%lx.%lx), \n",fid->Volume, 
					  fid->Vnode, 
					  fid->Unique);
	      return(0);
      }
      case CFS_ZAPVNODE : {
        /*
	  cfs_clstat.ncalls++;
	  cfs_clstat.reqs[CFS_ZAPVNODE]++;
	  */
	  cfsnc_zapfid(&out->cfs_zapvnode.VFid);
	  return(0);
      }	
      case CFS_ZAPFILE : {
	  cfsnc_zapfid(&out->cfs_zapfile.CodaFid);
	  return 0;
      }
      case CFS_PURGEFID : {
	      ViceFid *fid = &out->cfs_purgefid.CodaFid;
	  /*
	  cfs_clstat.ncalls++;
	  cfs_clstat.reqs[CFS_PURGEFID]++;
	  */
	      cfsnc_zapfid(fid);
	      cfsnc_zapParentfid(fid);     
	      CDEBUG(D_UPCALL, "purgefid: fid = (%lx.%lx.%lx)\n", 
                                            fid->Volume, fid->Vnode,
                                            fid->Unique);
	      return 0;
      }
      case CFS_REPLACE : {
        /*
	  cfs_clstat.ncalls++;
	  cfs_clstat.reqs[CFS_REPLACE]++;
	  */
	  cfsnc_replace(&out->cfs_replace.OldFid, 
			&out->cfs_replace.NewFid);
	  return (0);
      }			   
    }
      return 0;
}


