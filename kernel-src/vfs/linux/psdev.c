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
#include <linux/proc_fs.h>

#include <linux/coda.h>
#include "cfs_linux.h"
#include "psdev.h"
#include "super.h"
#include "namecache.h"
#include "sysctl.h"
#include <cnode.h>



#include <asm/io.h>
#include <asm/segment.h>
#include <asm/system.h>


/*
 *      Debugging aids
 */
int coda_debug =0;
int coda_print_entry = 0; 
int coda_access_cache = 1;


/* 
 * Coda stuff
 */
extern struct file_system_type coda_fs_type;
extern int coda_downcall(int opcode, struct outputArgs *out);
extern int init_coda_fs(void);
extern int cfsnc_get_info(char *buffer, char **start, off_t offset, int length, int dummy);
extern int cfsnc_nc_info(char *buffer, char **start, off_t offset, int length, int dummy);

/*
 * globals
 */
struct coda_sb_info coda_super_info;

inline struct vcomm *coda_psdev_vcomm(struct inode *inode) 
{
	/*
	 * at some future point, we may want different inodes
	 * coming from different minor numbers to use
	 * different coda_sb_info's (for odyssey eg).
	 * Right now we don't use the argument.
	 */
	
	return &(coda_super_info.mi_vcomm);
}

/*
 * Device operations
 */

static int coda_psdev_select(struct inode *inode, struct file *file, 
                          int sel_type, select_table * wait)
{
        struct vcomm *vcp = coda_psdev_vcomm(inode);

        if (!vcp)
              return -ENXIO;
        
        if (sel_type != SEL_IN)
              return 0;
    
        if (!EMPTY(vcp->vc_requests))
              return 1;
    
        select_wait(&vcp->vc_selproc, wait);
    
        return 0;
}


/*
 *	Receive a message written by Venus to the psdev
 */
 
static int coda_psdev_write(struct inode *inode, struct file *file, const char *buf, int count)
{
        struct vcomm *vcp = coda_psdev_vcomm(inode);
        struct vmsg *vmp;
        struct outputArgs *out;
	int error = 0;
        u_long seq;
        u_long opcode;
        u_long opcodebuf[2];
        
        if (!vcp)
              return -ENXIO;

        /* Peek at the opcode, unique id */
        memcpy_fromfs(opcodebuf, buf, 2 * sizeof(u_long));
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

	      /* should coda_downcall ever return an error ? 
	          NetBSD returns all kinds of errors; all but
		 ESCHR will crash Venus. Why? 
	        */
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
        struct vcomm *vcp = coda_psdev_vcomm(inode);
        struct vmsg *vmp;
#if 0 
        DEBUG("process %d\n", current->pid);
#endif 
        if (!vcp)
              return -ENXIO;
        
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
              coda_panic("coda_psdev_read: bad chain");
#endif

        /* If request was a signal, free up the message and don't
           enqueue it in the reply queue. */
        if (vmp->vm_opcode == CFS_SIGNAL) {
                    CDEBUG(D_PSDEV, "vcread: signal msg (%d, %d)\n", 
                              vmp->vm_opcode, vmp->vm_unique);
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
        register struct vcomm *vcp = NULL;

        ENTRY;
        
	vcp = coda_psdev_vcomm(inode);

        if (!vcp)
              return -ENODEV;
        
        MOD_INC_USE_COUNT;

        INIT_QUEUE(vcp->vc_requests);
        INIT_QUEUE(vcp->vc_replies);
	DEBUG("init queue: vcp->vc_requests at %d\n", (int) &(vcp->vc_requests));
	cfsnc_init();
	DEBUG("Name cache initialized.\n");
        return 0;
}


static void coda_psdev_release(struct inode * inode, struct file * file)
{
        struct vcomm *vcp;
        struct vmsg *vmp;

        ENTRY;

        vcp = coda_psdev_vcomm(inode);
        
        if (!VC_OPEN(vcp)) {
              printk("psdev_release: not open");
              return;
        }
    
        /* prevent future operations on this vfs from succeeding by auto-
         * unmounting any vfs mounted via this device. This frees user or
         * sysadm from having to remember where all mount points are located.
         * Put this before WAKEUPs to avoid queuing new messages between
         * the WAKEUP and the unmount (which can happen if we're unlucky)
         */
	
	/* flush the name cache so that we can unmount */
	DEBUG("Flushing the cache.\n");
	cfsnc_flush();
	cfsnc_use = 0;
	DEBUG("Done.\n");
	
        if (coda_super_info.mi_rootvp) {
                struct cnode *cnp = ITOC(coda_super_info.mi_rootvp);
                /* Let unmount know this is for real */
                cnp->c_flags |= C_DYING;
		/* XXX Could we force an unmount here? */
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
	cfsnc_use = 1;
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

struct proc_dir_entry proc_coda = {
        0, 4, "coda",
        S_IFDIR | S_IRUGO | S_IXUGO, 2, 0, 0,
        0, &proc_net_inode_operations,
        NULL, NULL,
        NULL,
        NULL, NULL      
};

struct proc_dir_entry proc_coda_cache =  {
                0 , 10, "coda-cache",
                S_IFREG | S_IRUGO, 1, 0, 0,
                0, &proc_net_inode_operations,
                cfsnc_get_info
        };

struct proc_dir_entry proc_coda_ncstats =  {
                0 , 12, "coda-ncstats",
                S_IFREG | S_IRUGO, 1, 0, 0,
                0, &proc_net_inode_operations,
                cfsnc_nc_info
        };

int init_module(void)
{
  int status;
  printk(KERN_INFO "Coda Kernel/User communications module 0.04\n");
  proc_register_dynamic( &proc_root, &proc_coda);
  proc_register_dynamic (&proc_net, &proc_coda_cache);
  proc_register_dynamic (&proc_net, &proc_coda_ncstats);
  coda_sysctl_init();
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
        coda_sysctl_clean();
	proc_unregister(&proc_root, proc_coda.low_ino);
        proc_unregister(&proc_net, proc_coda_cache.low_ino);
        proc_unregister(&proc_net, proc_coda_ncstats.low_ino);
}

#endif



