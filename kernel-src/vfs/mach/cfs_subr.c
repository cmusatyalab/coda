#ifndef _BLURB_
#define _BLURB_
/*

            Coda: an Experimental Distributed File System
                             Release 3.1

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

static char *rcsid = "$Header: /afs/cs.cmu.edu/project/coda-braam/src/coda-4.0.1/RCSLINK/./kernel-src/vfs/mach/cfs_subr.c,v 1.1 1996/11/22 19:16:08 braam Exp $";
#endif /*_BLURB_*/


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
 * Branch for NetBSD port revisions
 *
 * Revision 3.1  1995/03/04  19:07:58  bnoble
 * Bump to major revision 3 to prepare for NetBSD port
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

#include <cfs/cfs.h>
#include <cfs/cnode.h>
#include <vcfs.h>

#if	NVCFS

struct cnode *cfs_alloc C_ARGS((void));
struct cnode *cfs_find C_ARGS((ViceFid *fid));
#ifdef	__MACH__
extern struct fs *igetfs C_ARGS((dev_t));
#endif /* MACH */



/* God this kills me. Isn't there a better way of going about this? - DCS*/
char pass_process_info;

/*
 * Statistics
 */
struct {
	int	ncalls;			/* client requests */
	int	nbadcalls;		/* upcall failures */
	int	reqs[CFS_NCALLS];	/* count of each request */
} cfs_clstat;


/*
 * Cnode lookup stuff.
 * NOTE: CFS_CACHESIZE must be a power of 2 for cfshash to work!
 */
struct cnode *cfs_freelist = NULL;
int cfs_reuse = 0;
int cfs_new = 0;
int cfs_active = 0;

#define CFS_CACHESIZE 512
struct cnode *cfs_cache[CFS_CACHESIZE];

#define cfshash(fid) \
    (((fid)->Volume + (fid)->Vnode) & (CFS_CACHESIZE-1))


/* 
 * Key question: whether to sleep interuptably or uninteruptably when
 * waiting for Venus.  The former seems better (cause you can ^C a
 * job), but then GNU-EMACS completion breaks. Use tsleep with no
 * timeout, and no longjmp happens. But, when sleeping
 * "uninterruptibly", we don't get told if it returns abnormally
 * (e.g. kill -9).  
 */

/* If you want this to be interruptible, set this to > PZERO */
int cfscall_sleep = PZERO - 1;

int
cfscall(mntinfo, inSize, outSize, buffer) 
     struct cfs_mntinfo *mntinfo; int inSize; int *outSize; caddr_t buffer;
{
	struct vcomm *vcp;
	struct vmsg *vmp;
	int error;

	if (mntinfo == NULL) {
	    /* Unlikely, but could be a race condition with a dying warden */
	    return ENODEV;
	}

	vcp = &(mntinfo->mi_vcomm);
	
	cfs_clstat.ncalls++;
	cfs_clstat.reqs[((struct inputArgs *)buffer)->opcode]++;

	if (!VC_OPEN(vcp))
	    return(ENODEV);

	CFS_ALLOC(vmp,struct vmsg *,sizeof(struct vmsg));
	/* Format the request message. */
	vmp->vm_data = buffer;
	vmp->vm_flags = 0;
	vmp->vm_inSize = inSize;
	vmp->vm_outSize 
	    = *outSize ? *outSize : inSize; /* |buffer| >= inSize */
	vmp->vm_opcode = ((struct inputArgs *)buffer)->opcode;
	vmp->vm_unique = ++vcp->vc_seq;
	if (cfsdebug)
	    myprintf(("Doing a call for %d.%d\n", 
		      vmp->vm_opcode, vmp->vm_unique));
	
	/* Fill in the common input args. */
	((struct inputArgs *)buffer)->unique = vmp->vm_unique;

	/* Append msg to request queue and poke Venus. */
	INSQUE(vmp->vm_chain, vcp->vc_requests);
	SELWAKEUP(vcp->vc_selproc);

	/* We can be interrupted while we wait for Venus to process
	 * our request.  If the interrupt occurs before Venus has read
	 * the request, we dequeue and return. If it occurs after the
	 * read but before the reply, we dequeue, send a signal
	 * message, and return. If it occurs after the reply we ignore
	 * it. In no case do we want to restart the syscall.  If it
	 * was interrupted by a venus shutdown (vcclose), return
	 * ENODEV.  */

	/* Ignore return, We have to check anyway */
	SLEEP(&vmp->vm_sleep, (cfscall_sleep));

	if (VC_OPEN(vcp)) {	/* Venus is still alive */
 	/* Op went through, interrupt or not... */
	    if (vmp->vm_flags & VM_WRITE) {
		error = 0;
		*outSize = vmp->vm_outSize;
	    }

	    else if (!(vmp->vm_flags & VM_READ)) { 
		/* Interrupted before venus read it. */
		if (cfsdebug)
		    myprintf(("interrupted before read: intr = %x, %x, op = %d.%d, flags = %x\n",
			   SIGLIST, THECURSIG,
			   vmp->vm_opcode, vmp->vm_unique, vmp->vm_flags));
		REMQUE(vmp->vm_chain);
		error = EINTR;
	    }
	    
	    else { 	
		/* (!(vmp->vm_flags & VM_WRITE)) means interrupted after
                   upcall started */
		/* Interrupted after start of upcall, send venus a signal */
		struct inputArgs *dog;
		struct vmsg *svmp;
		
		if (cfsdebug)
		    myprintf(("Sending Venus a signal: intr = %x, %x, op = %d.%d, flags = %x\n",
			   SIGLIST, THECURSIG,
			   vmp->vm_opcode, vmp->vm_unique, vmp->vm_flags));
		
		REMQUE(vmp->vm_chain);
		error = EINTR;
		
		CFS_ALLOC(svmp, struct vmsg *, sizeof (struct vmsg));

		CFS_ALLOC((svmp->vm_data), char *, VC_IN_NO_DATA);
		dog = (struct inputArgs *)svmp->vm_data;
		
		svmp->vm_flags = 0;
		dog->opcode = svmp->vm_opcode = CFS_SIGNAL;
		dog->unique = svmp->vm_unique = vmp->vm_unique;
		svmp->vm_inSize = VC_IN_NO_DATA;
		svmp->vm_outSize = VC_IN_NO_DATA;
		
		if (cfsdebug)
		    myprintf(("cfscall: enqueing signal msg (%d, %d)\n",
			   svmp->vm_opcode, svmp->vm_unique));
		
		/* insert at head of queue! */
		INSQUE(svmp->vm_chain, vcp->vc_requests);
		SELWAKEUP(vcp->vc_selproc);
	    }
	}

	else {	/* If venus died (!VC_OPEN(vcp)) */
	    if (cfsdebug)
		myprintf(("vcclose woke op %d.%d flags %d\n",
		       vmp->vm_opcode, vmp->vm_unique, vmp->vm_flags));
	    
	    if (!vmp->vm_flags & VM_WRITE)
		error = ENODEV;
	}

	CFS_FREE(vmp, sizeof(struct vmsg));
	return(error);
}


#define ODD(vnode)        ((vnode) & 0x1)

/*
 * There are 6 cases where invalidations occur. The semantics of each
 * is listed here.
 *
 * CFS_FLUSH     -- flush all entries from the name cache and the cnode cache.
 * CFS_PURGEUSER -- flush all entries from the name cache for a specific user
 *                  This call is a result of token expiration.
 *
 * The next two are the result of callbacks on a file or directory.
 * CFS_ZAPDIR    -- flush the attributes for the dir from its cnode.
 *                  Zap all children of this directory from the namecache.
 * CFS_ZAPFILE   -- flush the attributes for a file.
 *
 * The fifth is a result of Venus detecting an inconsistent file.
 * CFS_PURGEFID  -- flush the attribute for the file
 *                  If it is a dir (odd vnode), purge its 
 *                  children from the namecache
 *                  remove the file from the namecache.
 *
 * The sixth allows Venus to replace local fids with global ones
 * during reintegration.
 *
 * CFS_REPLACE -- replace one ViceFid with another throughout the name cache 
 */

int handleDownCall(opcode, out)
     int opcode; struct outputArgs *out;
{
    int error;
    
    /* Handle invalidate requests. */
    switch (opcode) {
      case CFS_FLUSH : {
	  cfs_flush();
	  
	  CFSDEBUG(CFS_FLUSH,cfs_testflush();)    /* print remaining cnodes */
	      return(0);
      }
	
      case CFS_PURGEUSER : {
	  cfs_clstat.ncalls++;
	  cfs_clstat.reqs[CFS_PURGEUSER]++;
	  
	  cfsnc_purge_user(&out->d.cfs_purgeuser.cred);
	  return(0);
      }
	
      case CFS_ZAPFILE : {
	  struct cnode *cp;
	  int error = 0;
	  
	  cfs_clstat.ncalls++;
	  cfs_clstat.reqs[CFS_ZAPFILE]++;
	  
	  cp = cfs_find(&out->d.cfs_zapfile.CodaFid);
	  if (cp != NULL) {
	      VN_HOLD(CTOV(cp));
	      
	      cp->c_flags &= ~C_VATTR;
	      if (CTOV(cp)->v_flag & VTEXT)
		  error = cfs_vmflush(cp);
	      
	      CFSDEBUG(CFS_ZAPFILE, myprintf(("zapfile: fid = (%x.%x.%x), 
                                              refcnt = %d, error = %d\n",
					      cp->c_fid.Volume, 
					      cp->c_fid.Vnode, 
					      cp->c_fid.Unique, 
					      CNODE_COUNT(cp) - 1, error));)
		  
	      VN_RELE(CTOV(cp));
	  }
	  
	  return(error);
      }
	
      case CFS_ZAPDIR : {
	  struct cnode *cp;
	  
	  cfs_clstat.ncalls++;
	  cfs_clstat.reqs[CFS_ZAPDIR]++;
	  
	  cp = cfs_find(&out->d.cfs_zapdir.CodaFid);
	  if (cp != NULL) {
	      VN_HOLD(CTOV(cp));
	      
	      cp->c_flags &= ~C_VATTR;
	      cfsnc_zapParentfid(&out->d.cfs_zapdir.CodaFid);     
	      
	      CFSDEBUG(CFS_ZAPDIR, myprintf(("zapdir: fid = (%x.%x.%x), 
                                          refcnt = %d\n",cp->c_fid.Volume, 
					     cp->c_fid.Vnode, 
					     cp->c_fid.Unique, 
					     CNODE_COUNT(cp) - 1));)
		  
		  VN_RELE(CTOV(cp));
	  }
	  
	  return(0);
      }
	
      case CFS_ZAPVNODE : {
	  cfs_clstat.ncalls++;
	  cfs_clstat.reqs[CFS_ZAPVNODE]++;
	  
	  cfsnc_zapvnode(&out->d.cfs_zapvnode.VFid, &out->d.cfs_zapvnode.cred);
	  return(0);
      }	
	
      case CFS_PURGEFID : {
	  struct cnode *cp;
	  int error = 0;
	  
	  cfs_clstat.ncalls++;
	  cfs_clstat.reqs[CFS_PURGEFID]++;
	  
	  cp = cfs_find(&out->d.cfs_purgefid.CodaFid);
	  if (cp != NULL) {
	      VN_HOLD(CTOV(cp));
	      
	      if (ODD(out->d.cfs_purgefid.CodaFid.Vnode)) { /* Vnode is a directory */
		  cfsnc_zapParentfid(&out->d.cfs_purgefid.CodaFid);     
	      }
	      
	      cp->c_flags &= ~C_VATTR;
	      cfsnc_zapfid(&out->d.cfs_purgefid.CodaFid);
	      if (!(ODD(out->d.cfs_purgefid.CodaFid.Vnode)) 
		  && (CTOV(cp)->v_flag & VTEXT)) {
		  
		  error = cfs_vmflush(cp);
	      }
	      CFSDEBUG(CFS_PURGEFID, myprintf(("purgefid: fid = (%x.%x.%x), 
                                            refcnt = %d, error = %d\n",
                                            cp->c_fid.Volume, cp->c_fid.Vnode,
                                            cp->c_fid.Unique, 
					    CNODE_COUNT(cp) - 1, error));)
		  
		  VN_RELE(CTOV(cp));
	  }
	  return(error);
      }

      case CFS_REPLACE : {
	  cfs_clstat.ncalls++;
	  cfs_clstat.reqs[CFS_REPLACE]++;
	  
	  cfsnc_replace(&out->d.cfs_replace.OldFid, 
			&out->d.cfs_replace.NewFid);
	  
	  return (0);
      }			   
    }
}



/*
 * Return a vnode for the given fid.
 * If no cnode exists for this fid create one and put it
 * in a table hashed by fid.Volume and fid.Vnode.  If the cnode for
 * this fid is already in the table return it (ref count is
 * incremented by cfs_find.  The cnode will be flushed from the
 * table when cfs_inactive calls cfs_unsave.
 */
struct cnode *
makecfsnode(fid, vfsp, type)
     ViceFid *fid; VFS_T *vfsp; short type;
{
    VFS_T        foo;
    struct cnode *cp;
    int          err;
    
    if ((cp = cfs_find(fid)) == NULL) {
	struct vnode *vp;
	
	cp = cfs_alloc();
	cp->c_fid = *fid;
	
	SYS_VN_INIT(cp, vfsp, type);
	
	cfs_save(cp);
	
	/* Otherwise vfsp is 0 */
	if (!IS_CTL_FID(fid))
	    ((struct cfs_mntinfo *)(vfsp->VFS_DATA))->mi_refct++;
    } else {
	VN_HOLD(CTOV(cp));
    }
    
    return cp;
}

/*
 * cfs_kill is called as a side effect to vcopen. To prevent any
 * cnodes left around from an earlier run of a venus or warden from
 * causing problems with the new instance, mark any outstanding cnodes
 * as dying. Future operations on these cnodes should fail (excepting
 * cfs_inactive of course!). Since multiple venii/wardens can be
 * running, only kill the cnodes for a particular entry in the
 * cfs_mnttbl. -- DCS 12/1/94 */

int
cfs_kill(whoIam)
     VFS_T *whoIam;
{
    int hash, count = 0;
    struct cnode *cp;

    /* 
     * Algorithm is as follows: 
     *     First, step through all cnodes and mark them unmounting.
     *         NetBSD kernels may try to fsync them now that venus
     *         is dead, which would be a bad thing.
     *
     *     Second, flush whatever vnodes we can from the name cache.
     * 
     *     Finally, step through whatever is left and mark them dying.
     *        This prevents any operation at all.
     */


    for (hash = 0; hash < CFS_CACHESIZE; hash++) {
	for (cp = cfs_cache[hash]; cp != NULL; cp = CNODE_NEXT(cp)) {
	    if (VN_VFS(CTOV(cp)) == whoIam) {
		cp->c_flags |= CN_UNMOUNTING;
	    }
	}
    }

    /* This is slightly overkill, but should work. Eventually it'd be
     * nice to only flush those entries from the namecache that
     * reference a vnode in this vfs.  */
    cfsnc_flush();
    
    for (hash = 0; hash < CFS_CACHESIZE; hash++) {
	for (cp = cfs_cache[hash]; cp != NULL; cp = CNODE_NEXT(cp)) {
	    if (VN_VFS(CTOV(cp)) == whoIam) {
		count++;
		/* Clear unmountng bit, set dying bit */
		cp->c_flags &= ~CN_UNMOUNTING;
		cp->c_flags |= C_DYING;
		CFSDEBUG(CFS_FLUSH, 
			 myprintf(("Live cnode fid %x-%x-%x flags %d count %d\n",
				   (cp->c_fid).Volume,
				   (cp->c_fid).Vnode,
				   (cp->c_fid).Unique, 
				   cp->c_flags,
				   CNODE_COUNT(cp))); );
	    }
	}
    }
    return count;
}

/*
 * There are two reasons why a cnode may be in use, it may be in the
 * name cache or it may be executing.  
 */
void
cfs_flush()
{
    int hash;
    struct cnode *cp;
    
    cfs_clstat.ncalls++;
    cfs_clstat.reqs[CFS_FLUSH]++;
    
    cfsnc_flush();		    /* flush files from the name cache */

    for (hash = 0; hash < CFS_CACHESIZE; hash++) {
	for (cp = cfs_cache[hash]; cp != NULL; cp = CNODE_NEXT(cp)) {  
	    if (!ODD(cp->c_fid.Vnode)) /* only files can be executed */
		cfs_vmflush(cp);
	}
    }
}

/*
 * As a debugging measure, print out any cnodes that lived through a
 * name cache flush.  
 */
void
cfs_testflush()
{
    int hash;
    struct cnode *cp;
    
    for (hash = 0; hash < CFS_CACHESIZE; hash++) {
	for (cp = cfs_cache[hash];
	     cp != NULL;
	     cp = CNODE_NEXT(cp)) {  
	    myprintf(("Live cnode fid %x-%x-%x\n",
		      (cp->c_fid).Volume,(cp->c_fid).Vnode,
		      (cp->c_fid).Unique));
	}
    }
}

/*
 * Put a cnode in the hash table
 */
void
cfs_save(cp)
     struct cnode *cp;
{
	CNODE_NEXT(cp) = cfs_cache[cfshash(&cp->c_fid)];
	cfs_cache[cfshash(&cp->c_fid)] = cp;
}

/*
 * Remove a cnode from the hash table
 */
void
cfs_unsave(cp)
     struct cnode *cp;
{
    struct cnode *ptr;
    struct cnode *ptrprev = NULL;
    
    ptr = cfs_cache[cfshash(&cp->c_fid)]; 
    while (ptr != NULL) { 
	if (ptr == cp) { 
	    if (ptrprev == NULL) {
		cfs_cache[cfshash(&cp->c_fid)] 
		    = CNODE_NEXT(ptr);
	    } else {
		CNODE_NEXT(ptrprev) = CNODE_NEXT(ptr);
	    }
	    CNODE_NEXT(cp) = (struct cnode *)NULL;
	    
	    return; 
	}	
	ptrprev = ptr;
	ptr = CNODE_NEXT(ptr);
    }	
}

/*
 * Allocate a cnode.
 */
struct cnode *
cfs_alloc()
{
    struct cnode *cp;

    if (cfs_freelist) {
	cp = cfs_freelist;
	cfs_freelist = CNODE_NEXT(cp);
	cfs_reuse++;
    }
    else {
	CFS_ALLOC(cp, struct cnode *, sizeof(struct cnode));
	VNODE_VM_INFO_INIT(CTOV(cp));
	cfs_new++;
    }
    CN_INIT(cp);

    return(cp);
}

/*
 * Deallocate a cnode.
 */
void
cfs_free(cp)
     register struct cnode *cp;
{
    
    CNODE_NEXT(cp) = cfs_freelist;
    cfs_freelist = cp;
}

/*
 * Lookup a cnode by fid. If the cnode is dying, it is bogus so skip it.
 * NOTE: this allows multiple cnodes with same fid -- dcs 1/25/95
 */
struct cnode *
cfs_find(fid) 
     ViceFid *fid;
{
    struct cnode *cp;

    cp = cfs_cache[cfshash(fid)];
    while (cp) {
	if ((cp->c_fid.Vnode == fid->Vnode) &&
	    (cp->c_fid.Volume == fid->Volume) &&
	    (cp->c_fid.Unique == fid->Unique) &&
	    (!IS_DYING(cp)))
	    {
		cfs_active++;
		return(cp); 
	    }		    
	cp = CNODE_NEXT(cp);
    }
    return(NULL);
}

/* cfs_grab_vnode: lives in either cfs_mach.c or cfs_nbsd.c */

int
cfs_vmflush(cp)
     struct cnode *cp;
{
#ifndef __NetBSD__
    /* Unset <device, inode> so that page_read doesn't try to use
       (possibly) invalid cache file. */
    cp->c_device = 0;
    cp->c_inode = 0;

    return(inode_uncache_try(VTOI(CTOV(cp))) ? 0 : ETXTBSY);
#else /* __NetBSD__ */
    return 0;
#endif /* __NetBSD__ */
}

/*
 * Utilities used by both client and server
 * Standard levels:
 * 0) no debugging
 * 1) hard failures
 * 2) soft failures
 * 3) current test software
 * 4) main procedure entry points
 * 5) main procedure exit points
 * 6) utility procedure entry points
 * 7) utility procedure exit points
 * 8) obscure procedure entry points
 * 9) obscure procedure exit points
 * 10) random stuff
 * 11) all <= 1
 * 12) all <= 2
 * 13) all <= 3
 * ...
 */


#endif	/* NVCFS */
