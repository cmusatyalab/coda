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

/* ************************************************** */
/* These routines define the psuedo device for communication between
 * Coda's Venus and Minicache in Mach 2.6. They used to be in cfs_subr.c, 
 * but I moved them to make it easier to port the Minicache without 
 * porting coda. -- DCS 10/12/94
 */

/* 
 * Renamed to cfs_psdev: pseudo-device driver.
 */

/*
 * HISTORY
 * $Log:	cfs_psdev.c,v $
 * Revision 1.4.18.9  97/12/05  08:58:07  rvb
 * peter found this one
 * 
 * Revision 1.4.18.8  97/11/26  15:28:57  rvb
 * Cant make downcall pbuf == union cfs_downcalls yet
 * 
 * Revision 1.4.18.7  97/11/25  09:40:49  rvb
 * Final cfs_venus.c w/o macros, but one locking bug
 * 
 * Revision 1.4.18.6  97/11/20  11:46:41  rvb
 * Capture current cfs_venus
 * 
 * Revision 1.4.18.5  97/11/18  10:27:15  rvb
 * cfs_nbsd.c is DEAD!!!; integrated into cfs_vf/vnops.c; cfs_nb_foo and cfs_foo are joined
 * 
 * Revision 1.4.18.4  97/11/13  22:02:59  rvb
 * pass2 cfs_NetBSD.h mt
 * 
 * Revision 1.4.18.3  97/11/12  12:09:38  rvb
 * reorg pass1
 * 
 * Revision 1.4.18.2  97/10/29  16:06:09  rvb
 * Kill DYING
 * 
 * Revision 1.4.18.1  1997/10/28 23:10:15  rvb
 * >64Meg; venus can be killed!
 *
 * Revision 1.4  1996/12/12 22:10:58  bnoble
 * Fixed the "downcall invokes venus operation" deadlock in all known cases.  There may be more
 *
 * Revision 1.3  1996/11/13 04:14:20  bnoble
 * Merging BNOBLE_WORK_6_20_96 into main line
 *
 * Revision 1.2.8.1  1996/08/22 14:25:04  bnoble
 * Added a return code from vc_nb_close
 *
 * Revision 1.2  1996/01/02 16:56:58  bnoble
 * Added support for Coda MiniCache and raw inode calls (final commit)
 *
 * Revision 1.1.2.1  1995/12/20 01:57:24  bnoble
 * Added CFS-specific files
 *
 * Revision 1.1  1995/03/14  20:52:15  bnoble
 * Initial revision
 *
 */

/* These routines are the device entry points for Venus. */

extern int cfsnc_initialized;    /* Set if cache has been initialized */

#include <vcfs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/select.h>

#include <cfs/cfs.h>
#include <cfs/cfsk.h>
#include <cfs/cnode.h>
#include <cfs/cfsio.h>

struct vnode *cfs_ctlvp = 0;
int cfs_psdev_print_entry = 0;

#ifdef __GNUC__
#define ENTRY    \
    if(cfs_psdev_print_entry) myprintf(("Entered %s\n",__FUNCTION__))
#else
#define ENTRY
#endif 


/* vcfsattach: do nothing */
void
vcfsattach(n)
    int n;
{
}

/* 
 * These functions are written for NetBSD.
 */
int 
vc_nb_open(dev, flag, mode, p)    
    dev_t        dev;      
    int          flag;     
    int          mode;     
    struct proc *p;             /* NetBSD only */
{
    register struct vcomm *vcp;
    struct ody_mntinfo *op;
    struct cnode       *cp;
    
    ENTRY;

    if (minor(dev) >= NVCFS || minor(dev) < 0)
	return(ENXIO);
    
    if (!cfsnc_initialized)
	cfsnc_init();
    
    if (cfs_ctlvp == 0) {
	ViceFid ctlfid;
	
	ctlfid.Volume = CTL_VOL;
	ctlfid.Vnode = CTL_VNO;
	ctlfid.Unique = CTL_UNI;
	
	
	cp = makecfsnode(&ctlfid, 0, VCHR);
	cfs_ctlvp = CTOV(cp);
    }
    
    vcp = &cfs_mnttbl[minor(dev)].mi_vcomm;
    if (VC_OPEN(vcp))
	return(EBUSY);
    
    /* Make first 4 bytes be zero */
    cfs_mnttbl[minor(dev)].mi_name = (char *)0;
    bzero(&(vcp->vc_selproc), sizeof (struct selinfo));
    INIT_QUEUE(vcp->vc_requests);
    INIT_QUEUE(vcp->vc_replys);
    MARK_VC_OPEN(vcp);
    
    cfs_mnttbl[minor(dev)].mi_vfschain.vfsp = NULL;
    cfs_mnttbl[minor(dev)].mi_vfschain.rootvp = NULL;
    cfs_mnttbl[minor(dev)].mi_vfschain.next = NULL;

    return(0);
}


int 
vc_nb_close (dev, flag, mode, p)    
    dev_t        dev;      
    int          flag;     
    int          mode;     
    struct proc *p;
{
    register struct vcomm *	vcp;
    register struct vmsg *vmp;
    struct ody_mntinfo *op;
    int                 err;
	
    ENTRY;

    if (minor(dev) >= NVCFS || minor(dev) < 0)
	return(ENXIO);
    
    vcp = &cfs_mnttbl[minor(dev)].mi_vcomm;
    
    if (!VC_OPEN(vcp))
	panic("vcclose: not open");
    
    if (cfs_mnttbl[minor(dev)].mi_name) {
	CFS_FREE(cfs_mnttbl[minor(dev)].mi_name,
		 strlen(cfs_mnttbl[minor(dev)].mi_name));
	cfs_mnttbl[minor(dev)].mi_name = 0;
    }
    
    /* prevent future operations on this vfs from succeeding by auto-
     * unmounting any vfs mounted via this device. This frees user or
     * sysadm from having to remember where all mount points are located.
     * Put this before WAKEUPs to avoid queuing new messages between
     * the WAKEUP and the unmount (which can happen if we're unlucky)
     */
    for (op = &cfs_mnttbl[minor(dev)].mi_vfschain; op ; op = op->next) {
	if (op->rootvp) {
	    /* Let unmount know this is for real */
	    VTOC(op->rootvp)->c_flags |= C_UNMOUNTING;
	    cfs_unmounting(op->vfsp);
	    err = dounmount(op->vfsp, flag, p);
	    if (err)
		myprintf(("Error %d unmounting vfs in vcclose(%d)\n", 
			  err, minor(dev)));
	} else {
	    /* Should only be null if no mount has happened. */
	    if (op != &cfs_mnttbl[minor(dev)].mi_vfschain) 
		myprintf(("Help! assertion failed in vcwrite\n"));
	    
	}
    }
    
    /* Wakeup clients so they can return. */
    for (vmp = (struct vmsg *)GETNEXT(vcp->vc_requests);
	 !EOQ(vmp, vcp->vc_requests);
	 vmp = (struct vmsg *)GETNEXT(vmp->vm_chain))
    {	    
	/* Free signal request messages and don't wakeup cause
	   no one is waiting. */
	if (vmp->vm_opcode == CFS_SIGNAL) {
	    CFS_FREE((caddr_t)vmp->vm_data, (u_int)VC_IN_NO_DATA);
	    CFS_FREE((caddr_t)vmp, (u_int)sizeof(struct vmsg));
	    continue;
	}
	
	wakeup(&vmp->vm_sleep);
    }
    
    for (vmp = (struct vmsg *)GETNEXT(vcp->vc_replys);
	 !EOQ(vmp, vcp->vc_replys);
	 vmp = (struct vmsg *)GETNEXT(vmp->vm_chain))
    {
	wakeup(&vmp->vm_sleep);
    }
    
    MARK_VC_CLOSED(vcp);
    return 0;
}


int 
vc_nb_read(dev, uiop, flag)   
    dev_t        dev;  
    struct uio  *uiop; 
    int          flag;
{
    register struct vcomm *	vcp;
    register struct vmsg *vmp;
    int error = 0;
    
    ENTRY;

    if (minor(dev) >= NVCFS || minor(dev) < 0)
	return(ENXIO);
    
    vcp = &cfs_mnttbl[minor(dev)].mi_vcomm;
    /* Get message at head of request queue. */
    if (EMPTY(vcp->vc_requests))
	return(0);	/* Nothing to read */
    
    vmp = (struct vmsg *)GETNEXT(vcp->vc_requests);
    
    /* Move the input args into userspace */
    uiop->uio_rw = UIO_READ;
    error = uiomove(vmp->vm_data, vmp->vm_inSize, uiop);
    if (error) {
	myprintf(("vcread: error (%d) on uiomove\n", error));
	error = EINVAL;
    }

#ifdef DIAGNOSTIC    
    if (vmp->vm_chain.forw == 0 || vmp->vm_chain.back == 0)
	panic("vc_nb_read: bad chain");
#endif

    REMQUE(vmp->vm_chain);
    
    /* If request was a signal, free up the message and don't
       enqueue it in the reply queue. */
    if (vmp->vm_opcode == CFS_SIGNAL) {
	if (cfsdebug)
	    myprintf(("vcread: signal msg (%d, %d)\n", 
		      vmp->vm_opcode, vmp->vm_unique));
	CFS_FREE((caddr_t)vmp->vm_data, (u_int)VC_IN_NO_DATA);
	CFS_FREE((caddr_t)vmp, (u_int)sizeof(struct vmsg));
	return(error);
    }
    
    vmp->vm_flags |= VM_READ;
    INSQUE(vmp->vm_chain, vcp->vc_replys);
    
    return(error);
}

int
vc_nb_write(dev, uiop, flag)   
    dev_t        dev;  
    struct uio  *uiop; 
    int          flag;
{
    register struct vcomm *	vcp;
    register struct vmsg *vmp;
    struct cfs_out_hdr *out;
    u_long seq;
    u_long opcode;
    int buf[2];
    int error = 0;

    ENTRY;

    if (minor(dev) >= NVCFS || minor(dev) < 0)
	return(ENXIO);
    
    vcp = &cfs_mnttbl[minor(dev)].mi_vcomm;
    
    /* Peek at the opcode, unique without transfering the data. */
    uiop->uio_rw = UIO_WRITE;
    error = uiomove((caddr_t)buf, sizeof(int) * 2, uiop);
    if (error) {
	myprintf(("vcwrite: error (%d) on uiomove\n", error));
	return(EINVAL);
    }
    
    opcode = buf[0];
    seq = buf[1];
	
    if (cfsdebug)
	myprintf(("vcwrite got a call for %d.%d\n", opcode, seq));
    
    if (DOWNCALL(opcode)) {
	union outputArgs pbuf;
	
	/* get the rest of the data. */
	uiop->uio_rw = UIO_WRITE;
	error = uiomove((caddr_t)&pbuf.cfs_purgeuser.oh.result, sizeof(pbuf) - (sizeof(int)*2), uiop);
	if (error) {
	    myprintf(("vcwrite: error (%d) on uiomove (Op %d seq %d)\n", 
		      error, opcode, seq));
	    return(EINVAL);
	    }
	
	return handleDownCall(opcode, &pbuf);
    }
    
    /* Look for the message on the (waiting for) reply queue. */
    for (vmp = (struct vmsg *)GETNEXT(vcp->vc_replys);
	 !EOQ(vmp, vcp->vc_replys);
	 vmp = (struct vmsg *)GETNEXT(vmp->vm_chain))
    {
	if (vmp->vm_unique == seq) break;
    }
    
    if (EOQ(vmp, vcp->vc_replys)) {
	if (cfsdebug)
	    myprintf(("vcwrite: msg (%d, %d) not found\n", opcode, seq));
	
	return(ESRCH);
	}
    
    /* Remove the message from the reply queue */
    REMQUE(vmp->vm_chain);
    
    /* move data into response buffer. */
    out = (struct cfs_out_hdr *)vmp->vm_data;
    /* Don't need to copy opcode and uniquifier. */
    
    /* get the rest of the data. */
    if (vmp->vm_outSize < uiop->uio_resid) {
	myprintf(("vcwrite: more data than asked for (%d < %d)\n",
		  vmp->vm_outSize, uiop->uio_resid));
	wakeup(&vmp->vm_sleep); 	/* Notify caller of the error. */
	return(EINVAL);
    } 
    
    buf[0] = uiop->uio_resid; 	/* Save this value. */
    uiop->uio_rw = UIO_WRITE;
    error = uiomove((caddr_t) &out->result, vmp->vm_outSize - (sizeof(int) * 2), uiop);
    if (error) {
	myprintf(("vcwrite: error (%d) on uiomove (op %d seq %d)\n", 
		  error, opcode, seq));
	return(EINVAL);
    }
    
    /* I don't think these are used, but just in case. */
    /* XXX - aren't these two already correct? -bnoble */
    out->opcode = opcode;
    out->unique = seq;
    vmp->vm_outSize	= buf[0];	/* Amount of data transferred? */
    vmp->vm_flags |= VM_WRITE;
    wakeup(&vmp->vm_sleep);
    
    return(0);
}

int
vc_nb_ioctl(dev, cmd, addr, flag, p) 
    dev_t         dev;       
    int           cmd;       
    caddr_t       addr;      
    int           flag;      
    struct proc  *p;
{
    ENTRY;

    switch(cmd) {
    case CFSRESIZE: {
	struct cfs_resize *data = (struct cfs_resize *)addr;
	return(cfsnc_resize(data->hashsize, data->heapsize, IS_DOWNCALL));
	break;
    }
    case CFSSTATS:
	if (cfsnc_use) {
	    cfsnc_gather_stats();
	    return(0);
	} else {
	    return(ENODEV);
	}
	break;
    case CFSPRINT:
	if (cfsnc_use) {
	    print_cfsnc();
	    return(0);
	} else {
	    return(ENODEV);
	}
	break;
    case ODYBIND:
	/* Bind a name to our device. Used to allow more than one kind of FS */
	if (cfs_mnttbl[minor(dev)].mi_name) {
	    if (cfsdebug)
		myprintf(("ODYBIND: dev %d already has name %s\n", minor(dev),
			  cfs_mnttbl[minor(dev)].mi_name));
	    return(EBUSY);	/* Some name already used. */
	} else {
	    struct ody_bind *data = (struct ody_bind *)addr;
	    char *name;
	    
	    CFS_ALLOC(name, char *, (data->size));
	    copyin(data->name, name, data->size);
	    
	    if (cfsdebug)
		myprintf(("ODYBIND: binding %s to dev %d\n", 
			  name, minor(dev)));
	    
	    cfs_mnttbl[minor(dev)].mi_name = name;
	    return(0);
	}
	break;
    default :
	return(EINVAL);
	break;
    }
}

int
vc_nb_select(dev, flag, p)         
    dev_t         dev;    
    int           flag;   
    struct proc  *p;
{
    register struct vcomm *	vcp;
    
    ENTRY;
    
    if (minor(dev) >= NVCFS || minor(dev) < 0)
	return(ENXIO);
    
    vcp = &cfs_mnttbl[minor(dev)].mi_vcomm;
    
    if (flag != FREAD)
	return(0);
    
    if (!EMPTY(vcp->vc_requests))
	return(1);
    
    selrecord(p, &(vcp->vc_selproc));
    
    return(0);
}


/*
 * Statistics
 */
struct cfs_clstat cfs_clstat;

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
int cfs_pcatch = PCATCH;

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
	cfs_clstat.reqs[((struct cfs_in_hdr *)buffer)->opcode]++;

	if (!VC_OPEN(vcp))
	    return(ENODEV);

	CFS_ALLOC(vmp,struct vmsg *,sizeof(struct vmsg));
	/* Format the request message. */
	vmp->vm_data = buffer;
	vmp->vm_flags = 0;
	vmp->vm_inSize = inSize;
	vmp->vm_outSize 
	    = *outSize ? *outSize : inSize; /* |buffer| >= inSize */
	vmp->vm_opcode = ((struct cfs_in_hdr *)buffer)->opcode;
	vmp->vm_unique = ++vcp->vc_seq;
	if (cfsdebug)
	    myprintf(("Doing a call for %d.%d\n", 
		      vmp->vm_opcode, vmp->vm_unique));
	
	/* Fill in the common input args. */
	((struct cfs_in_hdr *)buffer)->unique = vmp->vm_unique;

	/* Append msg to request queue and poke Venus. */
	INSQUE(vmp->vm_chain, vcp->vc_requests);
	selwakeup(&(vcp->vc_selproc));

	/* We can be interrupted while we wait for Venus to process
	 * our request.  If the interrupt occurs before Venus has read
	 * the request, we dequeue and return. If it occurs after the
	 * read but before the reply, we dequeue, send a signal
	 * message, and return. If it occurs after the reply we ignore
	 * it. In no case do we want to restart the syscall.  If it
	 * was interrupted by a venus shutdown (vcclose), return
	 * ENODEV.  */

	/* Ignore return, We have to check anyway */
	tsleep(&vmp->vm_sleep, (cfscall_sleep|cfs_pcatch), "cfscall", 0);

	if (VC_OPEN(vcp)) {	/* Venus is still alive */
 	/* Op went through, interrupt or not... */
	    if (vmp->vm_flags & VM_WRITE) {
		error = 0;
		*outSize = vmp->vm_outSize;
	    }

	    else if (!(vmp->vm_flags & VM_READ)) { 
		/* Interrupted before venus read it. */
		if (cfsdebug||1)
		    myprintf(("interrupted before read: op = %d.%d, flags = %x\n",
			   vmp->vm_opcode, vmp->vm_unique, vmp->vm_flags));
		REMQUE(vmp->vm_chain);
		error = EINTR;
	    }
	    
	    else { 	
		/* (!(vmp->vm_flags & VM_WRITE)) means interrupted after
                   upcall started */
		/* Interrupted after start of upcall, send venus a signal */
		struct cfs_in_hdr *dog;
		struct vmsg *svmp;
		
		if (cfsdebug||1)
		    myprintf(("Sending Venus a signal: op = %d.%d, flags = %x\n",
			   vmp->vm_opcode, vmp->vm_unique, vmp->vm_flags));
		
		REMQUE(vmp->vm_chain);
		error = EINTR;
		
		CFS_ALLOC(svmp, struct vmsg *, sizeof (struct vmsg));

		CFS_ALLOC((svmp->vm_data), char *, VC_IN_NO_DATA);
		dog = (struct cfs_in_hdr *)svmp->vm_data;
		
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
		selwakeup(&(vcp->vc_selproc));
	    }
	}

	else {	/* If venus died (!VC_OPEN(vcp)) */
	    if (cfsdebug)
		myprintf(("vcclose woke op %d.%d flags %d\n",
		       vmp->vm_opcode, vmp->vm_unique, vmp->vm_flags));
	    
		error = ENODEV;
	}

	CFS_FREE(vmp, sizeof(struct vmsg));
	return(error);
}
