/*
 *      	An implementation of a loadable kernel mode driver providing
 *		multiple kernel/user space bidirectional communications links.
 *
 * 		Author: 	Alan Cox <alan@cymru.net>
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 * 
 *              Adapted to become the Coda pseudo device
 *              Peter  Braam  <braam@maths.ox.ac.uk> 
 *              Michael Callahan <mjc@emmy.smith.edu>           
 *
 */

/* 
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */


/*
 * This code was written for the Coda file system at Carnegie Mellon
 * University.  Contributers include David Steere, James Kistler, and
 * M. Satyanarayanan.  
 */


/* These routines define the psuedo device for communication between
 * Coda's Venus and Minicache in Mach 2.6. They used to be in cfs_subr.c, 
 * but I moved them to make it easier to port the Minicache without 
 * porting coda. -- DCS 10/12/94
 */


/*
 * HISTORY
 * cfs_psdev.c,v
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

#include <linux/module.h>

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/sched.h>
#include <linux/lp.h>
#include <linux/malloc.h>
#include <linux/ioport.h>
#include <linux/fcntl.h>
#include <linux/delay.h>
#include <linux/skbuff.h>


#include "psdev.h"
#include "cfs.h"
#include "cnode.h"



#include <asm/io.h>
#include <asm/segment.h>
#include <asm/system.h>


/*
 *      Debugging aids
 */
int coda_debug =1;
int coda_print_entry = 1; 


/* 
 * Coda stuff
 */
extern struct file_system_type coda_fs_type;
extern int coda_downcall(int opcode, struct outputArgs *out);
extern int init_coda_fs(void);


/*
 * Initialize the mnttbl. indexed by minor device number 
 */ 

struct cfs_mntinfo cfs_mnttbl[MAX_CODADEVS]; 


inline struct vcomm *coda_inode_vcomm(struct inode *inode) 
{
       unsigned int minor = MINOR(inode->i_rdev);
#if 0
       DEBUG("ino is %ld.\n", inode->i_ino);
#endif
       if (minor >= MAX_CODADEVS)
               return 0;

       return &cfs_mnttbl[minor].mi_vcomm;
}

/*
 * Device operations
 */


static int coda_psdev_select(struct inode *inode, struct file *file, 
                          int sel_type, select_table * wait)
{
        struct vcomm *vcp = coda_inode_vcomm(inode);

        /* messes up the log	ENTRY;  */

        if (!vcp)
              return -ENXIO;
        
        if (sel_type != SEL_IN)
              return 0;
    
        if (!EMPTY(vcp->vc_requests))
              return 1;
    
        select_wait(&vcp->vc_selproc, wait);
    
        return(0);
}


/*
 *	Receive a message written by Venus to the psdev
 */
 
static int coda_psdev_write(struct inode * inode, struct file * file, const char * buf, int count)
{
        struct vcomm *vcp = coda_inode_vcomm(inode);
        struct vmsg *vmp;
        struct outputArgs *out;
	int error=0;
        u_long seq;
        u_long opcode;
        u_long opcodebuf[2];

        /* ENTRY; */
        
        if (!vcp)
              return -ENXIO;

        /* Peek at the opcode, unique id */
        memcpy_fromfs (opcodebuf, buf, 2 * sizeof(int));
	/*        count -= 2 * sizeof(u_long);*/
        buf += 2 * sizeof(u_long);
    
        opcode = opcodebuf[0];
        seq = opcodebuf[1];
#if 0
	DEBUG("(process,opc,seq)=(%d,%ld.%ld)\n", current->pid, opcode, seq);
#endif   
        if (DOWNCALL(opcode)) {
              struct outputArgs pbuf;

	      DEBUG("handling downcall\n");

              /* get the rest of the data. */
              memcpy_fromfs(&pbuf.result, buf, sizeof(pbuf) - (2 * sizeof(int)));

              error = coda_downcall(opcode, &pbuf);
	      if ( error) {
		printk("psdev_write: error in coda_downcall: %d\n", error);
	      return 0;
	      }
	      return count;
        }

        
        /* Look for the message on the (waiting for) reply queue. */
        for (vmp = (struct vmsg *)GETNEXT(vcp->vc_replies);
             !EOQ(vmp, vcp->vc_replies);
             vmp = (struct vmsg *)GETNEXT(vmp->vm_chain))
          {
	    if (vmp->vm_unique == seq) break;
          }

    
        if (EOQ(vmp, vcp->vc_replies)) {
	  DEBUG("msg (%ld, %ld) not found\n", opcode, seq);
	  
	  return(-ESRCH);
        }

        /* Remove the message from the reply queue */
        REMQUE(vmp->vm_chain);

        /* move data into response buffer. */
        /* Don't need to copy opcode and uniquifier. */
        out = (struct outputArgs *)vmp->vm_data;

        
        /* get the rest of the data. */
#if 0
        if (vmp->vm_outSize < (count - 2 * sizeof(u_long))) {
              myprintf(("vcwrite: more data than asked for (%d < %d)\n",
                        vmp->vm_outSize, count));
              WAKEUP(&vmp->vm_sleep); 	/* Notify caller of the error. */
              return -EINVAL;
        } 
#endif

#if 0
DEBUG("memcpy_fromfs: previously attemted %d, now %d ", vmp->vm_outSize - (sizeof(u_long) * 2), count - 2 *sizeof(u_long) );
#endif
        memcpy_fromfs(&out->result, buf, count - 2 *sizeof(u_long) );

        /* I don't think these are used, but just in case. */
        /* XXX - aren't these two already correct? -bnoble */
        out->opcode = opcode;
        out->unique = seq;
        vmp->vm_outSize = count;	
        vmp->vm_flags |= VM_WRITE;
        WAKEUP(&vmp->vm_sleep);
        return(count);  
}

/*
 *	Read a message from the kernel to Venus
 */

static int coda_psdev_read(struct inode * inode, struct file * file, char * buf, int count)
{
        unsigned int minor = MINOR(inode->i_rdev);
        struct vcomm *vcp = coda_inode_vcomm(inode);
        struct vmsg *vmp;
#if 0 
        DEBUG("process %d\n", current->pid);
#endif 
        if (!vcp)
              return -ENXIO;
        
        vcp = &cfs_mnttbl[minor].mi_vcomm;
        
        /* Get message at head of request queue. */
        if (EMPTY(vcp->vc_requests)) {
              return 0;	/* Nothing to read */
        }
    
        vmp = (struct vmsg *)GETNEXT(vcp->vc_requests);
        REMQUE(vmp->vm_chain);

        /* Move the input args into userspace */
        
        if (vmp->vm_inSize < count)
              count = vmp->vm_inSize;

        if (count < vmp->vm_inSize) {
              printk ("cfs_read: warning: venus read %d bytes of %d long 
                                           message\n",count, vmp->vm_inSize);
        }

        memcpy_tofs(buf, vmp->vm_data, count);
        
#ifdef DIAGNOSTIC    
        if (vmp->vm_chain.forw == 0 || vmp->vm_chain.back == 0)
              panic("coda_psdev_read: bad chain");
#endif

        /* If request was a signal, free up the message and don't
           enqueue it in the reply queue. */
        if (vmp->vm_opcode == CFS_SIGNAL) {
              if (coda_debug)
                    myprintf(("vcread: signal msg (%d, %d)\n", 
                              vmp->vm_opcode, vmp->vm_unique));
              CODA_FREE((caddr_t)vmp->vm_data, (u_int)VC_IN_NO_DATA);
              CODA_FREE((caddr_t)vmp, (u_int)sizeof(struct vmsg));
              return count;
        }
    
        vmp->vm_flags |= VM_READ;
        INSQUE(vmp->vm_chain, vcp->vc_replies);

        return count;
}

static int coda_psdev_lseek(struct inode * inode, struct file * file,
                         off_t offset, int origin)
{

  ENTRY;
  
  return -ESPIPE;
}

static int coda_psdev_open(struct inode * inode, struct file * file)
{
        register struct vcomm *vcp = coda_inode_vcomm(inode);
        unsigned int       minor = MINOR(inode->i_rdev);

        ENTRY;
        
        if (!vcp)
              return -ENODEV;
        

        /* zero the mount information at this minor XXXXXXX*/ 
        /*        if (VC_OPEN(vcp))
              return -EBUSY; */
        
        MOD_INC_USE_COUNT;
	
        /* Make first 4 bytes be zero */
        cfs_mnttbl[minor].mi_name = (char *)0;

        INIT_QUEUE(vcp->vc_requests);
	DEBUG("init queue: vcp->vc_requests at %d\n", (int) &(vcp->vc_requests));
        INIT_QUEUE(vcp->vc_replies);
    
        cfs_mnttbl[minor].mi_vfschain.vfsp = NULL;
        cfs_mnttbl[minor].mi_vfschain.rootvp = NULL;
        cfs_mnttbl[minor].mi_vfschain.next = NULL;
    
        return 0;
}


static void coda_psdev_release(struct inode * inode, struct file * file)
{
        unsigned int minor = MINOR(inode->i_rdev);
        struct vcomm *vcp;
        struct vmsg *vmp;
        struct ody_mntinfo *op;

        ENTRY;

        if (minor < 0 || minor >= MAX_CODADEVS)
              return;

        vcp = &cfs_mnttbl[minor].mi_vcomm;
        
        if (!VC_OPEN(vcp)) {
              printk("vcclose: not open");
              return;
        }
    
        if (cfs_mnttbl[minor].mi_name) {
              CODA_FREE(cfs_mnttbl[minor].mi_name,
                       strlen(cfs_mnttbl[minor].mi_name));
              cfs_mnttbl[minor].mi_name = 0;
        }
    
        /* prevent future operations on this vfs from succeeding by auto-
         * unmounting any vfs mounted via this device. This frees user or
         * sysadm from having to remember where all mount points are located.
         * Put this before WAKEUPs to avoid queuing new messages between
         * the WAKEUP and the unmount (which can happen if we're unlucky)
         */
        for (op = &cfs_mnttbl[minor].mi_vfschain; op ; op = op->next) {
                if (op->rootvp) {
                        struct cnode *cnp = ITOC(op->rootvp);
                        /* Let unmount know this is for real */
                        cnp->c_flags |= C_DYING;
#if 0
                        XXXXX
                                err = DOUNMOUNT(op->vfsp);
                    if (err)
                          myprintf(("Error %d unmounting vfs in vcclose(%d)\n", 
                                    err, minor));
#endif
              } else {
                    /* Should only be null if no mount has happened. */
                    if (op != &cfs_mnttbl[minor].mi_vfschain) 
                          myprintf(("Help! assertion failed in vcwrite\n"));
              }
        }
    
        /* Wakeup clients so they can return. */
        for (vmp = (struct vmsg *)GETNEXT(vcp->vc_requests);
             !EOQ(vmp, vcp->vc_requests);
             vmp = (struct vmsg *)GETNEXT(vmp->vm_chain)) {	    
              /* Free signal request messages and don't wakeup cause
                 no one is waiting. */
              if (vmp->vm_opcode == CFS_SIGNAL) {
                    CODA_FREE((caddr_t)vmp->vm_data, (u_int)VC_IN_NO_DATA);
                    CODA_FREE((caddr_t)vmp, (u_int)sizeof(struct vmsg));
                    continue;
              }
    
              WAKEUP(&vmp->vm_sleep);
        }
        
        for (vmp = (struct vmsg *)GETNEXT(vcp->vc_replies);
             !EOQ(vmp, vcp->vc_replies);
             vmp = (struct vmsg *)GETNEXT(vmp->vm_chain)) {
              WAKEUP(&vmp->vm_sleep);
        }
        
        MARK_VC_CLOSED(vcp);
        
        MOD_DEC_USE_COUNT;
}


static struct file_operations coda_psdev_fops = {
      coda_psdev_lseek,
      coda_psdev_read,
      coda_psdev_write,
      NULL,		/* coda_psdev_readdir */
      coda_psdev_select,
      NULL,
      NULL,		/* coda_psdev_mmap */
      coda_psdev_open,
      coda_psdev_release
};

int init_coda_psdev(void)
{
        
	if(register_chrdev(CODA_PSDEV_MAJOR,"coda_psdev", &coda_psdev_fops)) {
              printk(KERN_ERR "coda_psdev: unable to get major %d\n", CODA_PSDEV_MAJOR);
              return -EIO;
	}
        
	return 0;
}

#ifdef MODULE

int init_module(void)
{
  int status;
  printk(KERN_INFO "Coda Kernel/User communications module 0.04\n");

  init_coda_psdev();
  

  
  if ((status = init_coda_fs()) == 0)
    register_symtab(0);
  else
    {
      printk("coda: failed in init_coda_fs!\n");
    }
  return status;
}


void cleanup_module(void)
{
        int err;

        ENTRY;

        unregister_chrdev(CODA_PSDEV_MAJOR,"coda_psdev");
        
        if ( (err = unregister_filesystem(&coda_fs_type)) != 0 ) {
                printk("coda: failed to unregister filesystem\n");
        }

}

#endif



