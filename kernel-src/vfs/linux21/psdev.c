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

#include <linux/config.h> /* for CONFIG_PROC_FS */
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
#include <linux/poll.h>
#include <asm/io.h>
#include <asm/segment.h>
#include <asm/system.h>
#include <asm/poll.h>
#include <asm/uaccess.h>

#include <linux/coda.h>
#include <linux/coda_linux.h>
#include <linux/coda_fs_i.h>
#include <linux/coda_psdev.h>
#include <linux/coda_cache.h>
#include <linux/coda_sysctl.h>


/*
 * Where is the prototype?
 */

int proc_register_dynamic(struct proc_dir_entry * dir,
			  struct proc_dir_entry * dp);

/* 
 * Coda stuff
 */
extern struct file_system_type coda_fs_type;
extern int init_coda_fs(void);
extern int cfsnc_get_info(char *buffer, char **start, off_t offset, int length, int dummy);
extern int cfsnc_nc_info(char *buffer, char **start, off_t offset, int length, int dummy);

/* statistics */
struct coda_upcallstats coda_callstats;
int           coda_hard = 0;  /* introduces a timeout on upcalls */
unsigned long coda_timeout = 30; /* .. secs, then signals will dequeue */
extern struct coda_sb_info coda_super_info[MAX_CODADEVS];
struct vcomm psdev_vcomm[MAX_CODADEVS];

/* queue stuff for the messages */
static inline void init_queue(struct queue *head)
{
	head->forw = head;
	head->back = head;
}

static inline struct vmsg *q_getnext(struct queue *elt)
{
	return (struct vmsg *)(elt->forw);
}

static inline int q_end(struct vmsg *msg, struct queue *queue)
{
	return (struct queue *)msg == queue;
}

static inline int q_empty(struct queue *queue)
{
	return queue->forw == queue;
}

/* insert before head, ie. at the tail */
void coda_q_insert(struct queue *el, struct queue *head)
{
	el->forw = head->back->forw;
	el->back = head->back;
	head->back->forw = el;
	head->back = el;
}

void coda_q_remove(struct queue *el)
{
	el->forw->back = el->back;
	el->back->forw = el->forw;
}

static struct vcomm *coda_psdev2vcomm(struct file *file)
{
       	unsigned int minor = MINOR(file->f_dentry->d_inode->i_rdev);
	struct vcomm *vcp = NULL;

	if ( (minor >= 0)  && (minor < MAX_CODADEVS) )
	        vcp = &psdev_vcomm[minor];
	return vcp;
}
	
/*
 * Device operations
 */


static unsigned int coda_psdev_poll(struct file *file, poll_table * wait)
{
        struct vcomm *vcp = coda_psdev2vcomm(file);
	unsigned int mask = POLLOUT | POLLWRNORM;

	if ( !vcp ) 
	        return -ENXIO;

	poll_wait(file, &(vcp->vc_waitq), wait);
	if (!q_empty(&(vcp->vc_pending)))
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
	int error = 0;
	int size;
        u_long uniq;
        u_long opcode;
        u_long opcodebuf[2];
        
        if (!vcp)
                return -ENXIO;

        /* Peek at the opcode, uniquefier */
	if (copy_from_user(opcodebuf, buf, 2 * sizeof(u_long)))
	        return -EFAULT;
	opcode = opcodebuf[0];
        uniq = opcodebuf[1];

	CDEBUG(D_PSDEV, "(process,opc,uniq)=(%d,%ld,%ld)\n", 
	       current->pid, opcode, uniq);

        if (DOWNCALL(opcode)) {
		struct super_block *sb = NULL;
                union outputArgs *dcbuf;
		size = sizeof(*dcbuf);

		sb = vcp->vc_sb;
		if ( !sb ) {
			printk("coda_psdev_write: downcall, no SB!\n");
			return count;
		}
		CDEBUG(D_PSDEV, "handling downcall\n");

		if  ( count < sizeof(struct cfs_out_hdr) ) {
		        printk("coda_downcall opc %ld uniq %ld, not enough!\n",
			       opcode, uniq);
			return count;
		}
		CODA_ALLOC(dcbuf, union outputArgs *, size);
		if ( count > size ) {
		        printk("Coda: downcall opc %ld, uniq %ld, too much!",
			       opcode, uniq);
		        count = size;
		}
		if (copy_from_user(dcbuf, buf, count))
		        return -EFAULT;

		/* what downcall errors does Venus handle ? */
		error = coda_downcall(opcode, dcbuf, sb);

		if ( error) {
		        printk("psdev_write: coda_downcall error: %d\n", 
			       error);
			return 0;
		}
		CODA_FREE(dcbuf, size);
		return count;
        }

        
        /* Look for the message on the processing queue. */
        for (vmp = q_getnext(&(vcp->vc_processing));
	     !q_end(vmp, &(vcp->vc_processing));
             vmp = q_getnext(&(vmp->vm_chain))) {
	        if (vmp->vm_unique == uniq) {
			break;
			CDEBUG(D_PSDEV,"Eureka: uniq %ld on queue!\n", uniq);
		}
	}
        if (q_end(vmp, &(vcp->vc_processing))) {
	        printk("psdev_write: msg (%ld, %ld) not found\n", 
		       opcode, uniq);
		return(-ESRCH);
        }

        /* Remove the message from the processing queue */
        coda_q_remove(&(vmp->vm_chain));

        /* move data into response buffer. */
        if (vmp->vm_outSize < count) {
                printk("psdev_write: too much cnt: %d, cnt: %d, opc: %ld, uniq: %ld.\n",
		       vmp->vm_outSize, count, opcode, uniq);
		count = vmp->vm_outSize; /* don't have more space! */
	}
        if (copy_from_user(vmp->vm_data, buf, count))
	        return -EFAULT;

	/* adjust outsize. is this usefull ?? */
        vmp->vm_outSize = count;	
        vmp->vm_flags |= VM_WRITE;

	CDEBUG(D_PSDEV, 
	       "Found! Count %d for (opc,uniq)=(%ld,%ld), vmsg at %x\n", 
	        count, opcode, uniq, (int)&vmp);

        wake_up(&vmp->vm_sleep);
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
        if (q_empty(&(vcp->vc_pending))) {
              return 0;	
        }
    
        vmp = q_getnext(&(vcp->vc_pending));
        coda_q_remove(&(vmp->vm_chain));

        /* Move the input args into userspace */
        if (vmp->vm_inSize <= count)
              result = vmp->vm_inSize;

        if (count < vmp->vm_inSize) {
                printk ("psdev_read: Venus read %d bytes of %d in message\n",
			count, vmp->vm_inSize);
        }

        if ( copy_to_user(buf, vmp->vm_data, result))
	        return -EFAULT;
        
        if (vmp->vm_chain.forw == 0 || vmp->vm_chain.back == 0)
                printk("coda_psdev_read: bad chain");

        /* If request was a signal, don't enqueue */
        if (vmp->vm_opcode == CFS_SIGNAL) {
                    CDEBUG(D_PSDEV, "vcread: signal msg (%d, %d)\n", 
                              vmp->vm_opcode, vmp->vm_unique);
              CODA_FREE(vmp->vm_data, sizeof(struct cfs_in_hdr));
              CODA_FREE(vmp, sizeof(struct vmsg));
              return count;
        }
    
        vmp->vm_flags |= VM_READ;
        coda_q_insert(&(vmp->vm_chain), &(vcp->vc_processing));

        return result;
}


static int coda_psdev_open(struct inode * inode, struct file * file)
{
        register struct vcomm *vcp = NULL;
        ENTRY;
        
	vcp =coda_psdev2vcomm(file);

        if (!vcp)
		return -ENODEV;

	if (vcp->vc_inuse)
		return -EBUSY;

	memset(vcp, 0, sizeof(struct vcomm));
	vcp->vc_inuse = 1;
	MOD_INC_USE_COUNT;

        init_queue(&(vcp->vc_pending));
        init_queue(&(vcp->vc_processing));

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
	ENTRY;

        vcp = coda_psdev2vcomm(file);
        
        if ( !vcp || !vcomm_open(vcp) ) {
              printk("psdev_release: not open");
              return 0;
        }
    
	
	/* flush the name cache so that we can unmount */
	CDEBUG(D_PSDEV, "Flushing the cache.\n");
	/* cfsnc_flush(); */
	/* cfsnc_use = 0; */
	CDEBUG(D_PSDEV, "Done.\n");
	
	/* if operations are in progress perhaps the kernel
	   can profit from setting the C_DYING flag on the root 
	   cnode of Coda filesystems */
        if (coda_super_info[minor].sbi_root) {
                struct coda_inode_info *cnp = 
			ITOC(coda_super_info[minor].sbi_root);
                cnp->c_flags |= C_DYING;
        } else 
		vcp->vc_inuse = 0;	
	
    
        /* Wakeup clients so they can return. */
        for (vmp = q_getnext(&(vcp->vc_pending));
             !q_end(vmp, &(vcp->vc_pending));
             vmp = q_getnext(&(vmp->vm_chain))) {	    
              /* Free signal request messages and don't wakeup cause
                 no one is waiting. */
              if (vmp->vm_opcode == CFS_SIGNAL) {
                    CODA_FREE(vmp->vm_data, sizeof(struct cfs_in_hdr));
                    CODA_FREE(vmp, (u_int)sizeof(struct vmsg));
                    continue;
              }
              wake_up(&vmp->vm_sleep);
        }
        
        for (vmp = q_getnext(&(vcp->vc_processing));
             !q_end(vmp, &(vcp->vc_processing));
             vmp = q_getnext(&(vmp->vm_chain))) {
	        wake_up(&vmp->vm_sleep);
        }
        
        mark_vcomm_closed(vcp);
	MOD_DEC_USE_COUNT;
	EXIT;
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


#ifdef CONFIG_PROC_FS

struct proc_dir_entry proc_coda = {
        0, 4, "coda",
        S_IFDIR | S_IRUGO | S_IXUGO | S_IWUSR, 2, 0, 0,
        0, &proc_net_inode_operations,

};

struct proc_dir_entry proc_coda_ncstats =  {
                0 , 12, "coda-ncstats",
                S_IFREG | S_IRUGO, 1, 0, 0,
                0, &proc_net_inode_operations,
                cfsnc_nc_info
        };

#endif


int init_coda_psdev(void)
{
	if(register_chrdev(CODA_PSDEV_MAJOR,"coda_psdev", &coda_psdev_fops)) {
              printk(KERN_ERR "coda_psdev: unable to get major %d\n", 
		     CODA_PSDEV_MAJOR);
              return -EIO;
	}
	memset(psdev_vcomm, 0, sizeof(psdev_vcomm));
	memset(coda_super_info, 0, sizeof(coda_super_info));
	memset(&coda_callstats, 0, sizeof(coda_callstats));

#ifdef CONFIG_PROC_FS
	proc_register(&proc_root,&proc_coda);
	proc_register(&proc_coda, &proc_coda_ncstats);
	coda_sysctl_init();
#endif 
	return 0;
}


#ifdef MODULE

EXPORT_NO_SYMBOLS;

MODULE_AUTHOR("Peter J. Braam <braam@cs.cmu.edu>");

int init_module(void)
{
  int status;
  printk(KERN_INFO "Coda Kernel/User communications module 2.0\n");

  status = init_coda_psdev();
  if ( status ) {
	  printk("Problem (%d) in init_coda_psdev\n", status);
	  return status;
  }

  status = init_coda_fs();
  if (status) {
	  printk("coda: failed in init_coda_fs!\n");
  }
  return status;
}


void cleanup_module(void)
{
        int err;

        ENTRY;

        if ( (err = unregister_filesystem(&coda_fs_type)) != 0 ) {
                printk("coda: failed to unregister filesystem\n");
        }
        unregister_chrdev(CODA_PSDEV_MAJOR,"coda_psdev");

#if CONFIG_PROC_FS
        coda_sysctl_clean();
        proc_unregister(&proc_coda, proc_coda_ncstats.low_ino);
	proc_unregister(&proc_root, proc_coda.low_ino);
#endif 
}

#endif

