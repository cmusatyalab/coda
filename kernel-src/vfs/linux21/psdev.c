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
 *              Adapted to become the Linux 2.0 Coda pseudo device
 *              Peter  Braam  <braam@maths.ox.ac.uk> 
 *              Michael Callahan <mjc@emmy.smith.edu>           
 *
 *              Changes for Linux 2.1
 *              Copyright (c) 1997 Carnegie-Mellon University
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
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <asm/io.h>
#include <asm/segment.h>
#include <asm/system.h>
#include <asm/poll.h>
#include <asm/uaccess.h>

#include <linux/coda.h>
#include <linux/coda_linux.h>
#include <linux/coda_psdev.h>
#include <linux/coda_cnode.h>
#include <linux/coda_namecache.h>
#include <linux/coda_sysctl.h>


/* 
 * Coda stuff
 */
extern struct file_system_type coda_fs_type;
extern int coda_downcall(int opcode, struct outputArgs *out);
extern int init_coda_fs(void);
extern int cfsnc_get_info(char *buffer, char **start, off_t offset, int length, int dummy);
extern int cfsnc_nc_info(char *buffer, char **start, off_t offset, int length, int dummy);

/* statistics */
struct coda_upcallstats coda_callstats;

extern struct coda_sb_info coda_super_info[MAX_CODADEVS];
struct vcomm psdev_vcomm[MAX_CODADEVS];

/*
 * Device operations
 */


static struct vcomm *coda_psdev2vcomm(struct file *file)
{
       	unsigned int minor = MINOR(file->f_dentry->d_inode->i_rdev);
	struct vcomm *vcp = NULL;

	if ( (minor >= 0)  && (minor < MAX_CODADEVS) )
	        vcp = &psdev_vcomm[minor];
	return vcp;
}
	

static unsigned int coda_psdev_poll(struct file *file, poll_table * wait)
{
        struct vcomm *vcp = coda_psdev2vcomm(file);
	unsigned int mask = POLLOUT | POLLWRNORM;

	if ( !vcp ) 
	        return -ENXIO;

	poll_wait(&(vcp->vc_waitq), wait);
	if (!EMPTY(vcp->vc_pending))
                mask |= POLLIN | POLLRDNORM;

	return mask;
}


/*
 *	Receive a message written by Venus to the psdev
 */
 
static ssize_t coda_psdev_write(struct file *file, const char *buf, 
				size_t count, loff_t *off)
{
        struct vcomm *vcp = coda_psdev2vcomm(file);
        struct vmsg *vmp;
        struct outputArgs *out;
	int error = 0;
	int size;
        u_long uniq;
        u_long opcode;
        u_long opcodebuf[2];
        
        if (!vcp)
                return -ENXIO;

        /* Peek at the opcode, unique id */
	if (copy_from_user(opcodebuf, buf, 2 * sizeof(u_long)))
	        return -EFAULT;
	opcode = opcodebuf[0];
        uniq = opcodebuf[1];

	CDEBUG(D_PSDEV, "(process,opc,uniq)=(%d,%ld,%ld)\n", 
	       current->pid, opcode, uniq);

        if (DOWNCALL(opcode)) {
                struct outputArgs pbuf;

		CDEBUG(D_PSDEV, "handling downcall\n");

              /* get the rest of the data. */
		size = sizeof(pbuf);
		if  ( count < sizeof(pbuf) ) {
		        printk("Coda: downcall opc %ld, uniq %ld, not enough!\n",
			       opcode, uniq);
			size =count;
		} else if ( count > sizeof(pbuf) ) {
		        printk("Coda: downcall opc %ld, uniq %ld, too much!",
			       opcode, uniq);
		        size = sizeof(pbuf);
		}
		if (copy_from_user(&pbuf, buf, size))
		        return -EFAULT;

	      /* what errors for coda_downcall should be
	      * sent to Venus ? 
	      */
		error = coda_downcall(opcode, &pbuf);
		if ( error) {
		        printk("psdev_write: coda_downcall error: %d\n", 
			       error);
			return 0;
		}
		return count;
        }

        
        /* Look for the message on the processing queue. */
        for (vmp = (struct vmsg *)GETNEXT(vcp->vc_processing);
	     !EOQ(vmp, vcp->vc_processing);
             vmp = (struct vmsg *)GETNEXT(vmp->vm_chain)) {
	        if (vmp->vm_unique == uniq) break;
		CDEBUG(D_PSDEV,"Eureka: uniq %ld on queue!\n", uniq);
	}
        if (EOQ(vmp, vcp->vc_processing)) {
	        printk("psdev_write: msg (%ld, %ld) not found\n", 
		       opcode, uniq);
		return(-ESRCH);
        }

        /* Remove the message from the processing queue */
        REMQUE(vmp->vm_chain);

        /* move data into response buffer. */
        /* Don't need to copy opcode and uniquifier. */
        out = (struct outputArgs *)vmp->vm_data;
        /* get the rest of the data. */
        if (vmp->vm_outSize < count) {
                printk("Coda write: too much outs: %d, cnt: %d, opc: %ld, uniq: %ld.\n",
		       vmp->vm_outSize, count, opcode, uniq);
		wake_up_interruptible(&vmp->vm_sleep); 	
		return -EINVAL;
        } else if (vmp->vm_outSize > count) {
                printk("Coda write: too much outs: %d, cnt: %d, opc: %ld, uniq: %ld.\n",
		       vmp->vm_outSize, count, opcode, uniq);
	}
        if (copy_from_user(out, buf, count))
	        return -EFAULT;

	/* adjust outsize. is this usefull ?? */
        vmp->vm_outSize = count;	
        vmp->vm_flags |= VM_WRITE;

	CDEBUG(D_PSDEV, 
	       "Found! Count %d for (opc,uniq)=(%ld,%ld), vmsg at %x\n", 
	        count, opcode, uniq, (int)&vmp);

        wake_up_interruptible(&vmp->vm_sleep);
        return(count);  
}

/*
 *	Read a message from the kernel to Venus
 */

static ssize_t coda_psdev_read(struct file * file, char * buf, 
			    size_t count, loff_t *off)
{
        struct vcomm *vcp = coda_psdev2vcomm(file);
        struct vmsg *vmp;
	int result = count ;

        if (!vcp)
              return -ENXIO;
        
        /* Get message at head of request queue. */
        if (EMPTY(vcp->vc_pending)) {
              return 0;	/* Nothing to read */
        }
    
        vmp = (struct vmsg *)GETNEXT(vcp->vc_pending);
        REMQUE(vmp->vm_chain);

        /* Move the input args into userspace */
        
        if (vmp->vm_inSize <= count)
              result = vmp->vm_inSize;

        if (count < vmp->vm_inSize) {
                printk ("psdev_read: warning: venus read %d bytes of %d long 
                                           message\n",count, vmp->vm_inSize);
        }

        if ( copy_to_user(buf, vmp->vm_data, result))
	        return -EFAULT;
        
        if (vmp->vm_chain.forw == 0 || vmp->vm_chain.back == 0)
                coda_panic("coda_psdev_read: bad chain");

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
        INSQUE(vmp->vm_chain, vcp->vc_processing);

        return result;
}


static int coda_psdev_open(struct inode * inode, struct file * file)
{
        register struct vcomm *vcp = NULL;

        ENTRY;
        
	vcp = coda_psdev2vcomm(file);

        if (!vcp)
              return -ENODEV;
	memset(vcp, 0, sizeof(struct vcomm));

        MOD_INC_USE_COUNT;

        INIT_QUEUE(vcp->vc_pending);
        INIT_QUEUE(vcp->vc_processing);

	cfsnc_init();
	CDEBUG(D_PSDEV, "Name cache initialized.\n");

	memset(&coda_callstats, 0, sizeof(struct coda_upcallstats));
	EXIT;
        return 0;
}


static int
coda_psdev_release(struct inode * inode, struct file * file)
{
        struct vcomm *vcp;
        struct vmsg *vmp;
	unsigned int minor = MINOR(file->f_dentry->d_inode->i_rdev);

        vcp = coda_psdev2vcomm(file);
        
        if ( !vcp || !vcomm_open(vcp) ) {
              printk("psdev_release: not open");
              return 0;
        }
    
	
	/* flush the name cache so that we can unmount */
	CDEBUG(D_PSDEV, "Flushing the cache.\n");
	cfsnc_flush();
	cfsnc_use = 0;
	CDEBUG(D_PSDEV, "Done.\n");
	
        /* prevent future operations on this vfs from succeeding by
         * auto- unmounting any vfs mounted via this device. This
         * frees user or sysadm from having to remember where all
         * mount points are located.  Put this before WAKEUPs to avoid
         * queuing new messages between the WAKEUP and the unmount
         * (which can happen if we're unlucky) */

        if (coda_super_info[minor].sbi_root) {
                struct cnode *cnp = ITOC(coda_super_info[minor].sbi_root);
                /* Let unmount know this is for real */
                cnp->c_flags |= C_DYING;
		/* XXX Could we force an unmount here? */
        }
	
    
        /* Wakeup clients so they can return. */
        for (vmp = (struct vmsg *)GETNEXT(vcp->vc_pending);
             !EOQ(vmp, vcp->vc_pending);
             vmp = (struct vmsg *)GETNEXT(vmp->vm_chain)) {	    
              /* Free signal request messages and don't wakeup cause
                 no one is waiting. */
              if (vmp->vm_opcode == CFS_SIGNAL) {
                    CODA_FREE((caddr_t)vmp->vm_data, (u_int)VC_IN_NO_DATA);
                    CODA_FREE((caddr_t)vmp, (u_int)sizeof(struct vmsg));
                    continue;
              }
    
              wake_up_interruptible(&vmp->vm_sleep);
        }
        
        for (vmp = (struct vmsg *)GETNEXT(vcp->vc_processing);
             !EOQ(vmp, vcp->vc_processing);
             vmp = (struct vmsg *)GETNEXT(vmp->vm_chain)) {
	        wake_up_interruptible(&vmp->vm_sleep);
        }
        
        mark_vcomm_closed(vcp);
	cfsnc_use = 0;
        MOD_DEC_USE_COUNT;
	return 0;
}


static struct file_operations coda_psdev_fops = {
      NULL,                  /* llseek */
      coda_psdev_read,       /* read */
      coda_psdev_write,      /* write */
      NULL,		     /* coda_psdev_readdir */
      coda_psdev_poll,       /* poll */
      NULL,                  /* ioctl */
      NULL,		     /* coda_psdev_mmap */
      coda_psdev_open,       /* open */
      coda_psdev_release,    /* release */
      NULL,                  /* fsync */
      NULL,                  /* fasync */
      NULL,                  /* check_media_change */
      NULL,                  /* revalidate */
      NULL                   /* lock */
};

int init_coda_psdev(void)
{
        
	if(register_chrdev(CODA_PSDEV_MAJOR,"coda_psdev", &coda_psdev_fops)) {
              printk(KERN_ERR "coda_psdev: unable to get major %d\n", 
		     CODA_PSDEV_MAJOR);
              return -EIO;
	}
        
	return 0;
}


#ifdef CONFIG_PROC_FS

struct proc_dir_entry proc_coda = {
        0, 4, "coda",
        S_IFDIR | S_IRUGO | S_IXUGO, 2, 0, 0,
        0, &proc_net_inode_operations,

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

#endif

#ifdef MODULE
int init_module(void)
{
  int status;
  printk(KERN_INFO "Coda Kernel/User communications module 0.04\n");

#ifdef CONFIG_PROC_FS
  proc_register(&proc_root,&proc_coda);
  proc_register(&proc_coda, &proc_coda_cache);
  proc_register(&proc_coda, &proc_coda_ncstats);
  coda_sysctl_init();
#endif 

  init_coda_psdev();
  
  if ((status = init_coda_fs()) != 0)
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
#if CONFIG_PROC_FS
        coda_sysctl_clean();
        proc_unregister(&proc_coda, proc_coda_cache.low_ino);
        proc_unregister(&proc_coda, proc_coda_ncstats.low_ino);
	proc_unregister(&proc_root, proc_coda.low_ino);
#endif 
}

#endif



