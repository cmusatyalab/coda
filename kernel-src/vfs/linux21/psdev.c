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
#include <linux/poll.h>
#include <linux/init.h>
#include <linux/list.h>
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
#include <linux/coda_proc.h>

/* 
 * Coda stuff
 */
extern struct file_system_type coda_fs_type;
extern int init_coda_fs(void);

/* statistics */
int           coda_hard    = 0;  /* allows signals during upcalls */
unsigned long coda_timeout = 30; /* .. secs, then signals will dequeue */

struct coda_sb_info coda_super_info;
struct venus_comm coda_upc_comm;
	
/*
 * Device operations
 */

static unsigned int coda_psdev_poll(struct file *file, poll_table * wait)
{
        struct venus_comm *vcp = &coda_upc_comm;
	unsigned int mask = POLLOUT | POLLWRNORM;

	poll_wait(file, &(vcp->vc_waitq), wait);
	if (!list_empty(&vcp->vc_pending))
                mask |= POLLIN | POLLRDNORM;

	return mask;
}


/*
 *	Receive a message written by Venus to the psdev
 */
 
static ssize_t coda_psdev_write(struct file *file, const char *buf, 
				size_t count, loff_t *off)
{
        struct venus_comm *vcp = &coda_upc_comm;
        struct upc_req *req = NULL;
        struct upc_req *tmp;
	struct list_head *lh;
	struct coda_in_hdr hdr;
	int error;


	if ( !coda_upc_comm.vc_pid ) 
		return -EIO;
        /* Peek at the opcode, uniquefier */
	if (copy_from_user(&hdr, buf, 2 * sizeof(u_long)))
	        return -EFAULT;

	CDEBUG(D_PSDEV, "(process,opc,uniq)=(%d,%ld,%ld), count %d\n", 
	       current->pid, hdr.opcode, hdr.unique, count);

        if (DOWNCALL(hdr.opcode)) {
		struct super_block *sb = NULL;
                union outputArgs *dcbuf;
		int size = sizeof(*dcbuf);

		sb = coda_super_info.sbi_sb;
		if ( !sb ) {
			printk("coda_psdev_write: downcall, no SB!\n");
			return count;
		}
		CDEBUG(D_PSDEV, "handling downcall\n");

		if  ( count < sizeof(struct coda_out_hdr) ) {
		        printk("coda_downcall opc %ld uniq %ld, not enough!\n",
			       hdr.opcode, hdr.unique);
			return count;
		}
		CODA_ALLOC(dcbuf, union outputArgs *, size);
		if ( count > size ) {
		        printk("Coda: downcall opc %ld, uniq %ld, too much!",
			       hdr.opcode, hdr.unique);
		        count = size;
		}
		if (copy_from_user(dcbuf, buf, count))
		        return -EFAULT;

		/* what downcall errors does Venus handle ? */
		error = coda_downcall(hdr.opcode, dcbuf, sb);

		if ( error) {
		        printk("psdev_write: coda_downcall error: %d\n", 
			       error);
			return 0;
		}
		CODA_FREE(dcbuf, size);
		return count;
        }

        
        /* Look for the message on the processing queue. */
	lh  = &vcp->vc_processing;
        while ( (lh = lh->next) != &vcp->vc_processing ) {
		tmp = list_entry(lh, struct upc_req , uc_chain);
	        if (tmp->uc_unique == hdr.unique) {
			req = tmp;
			list_del(&req->uc_chain);
			CDEBUG(D_PSDEV,"Eureka: uniq %ld on queue!\n", 
			       hdr.unique);
			break;
		}
	}
        if (!req) {
	        printk("psdev_write: msg (%ld, %ld) not found\n", 
		       hdr.opcode, hdr.unique);
		return(-ESRCH);
        }

        /* move data into response buffer. */
        if (req->uc_outSize < count) {
                printk("psdev_write: too much cnt: %d, cnt: %d, opc: %ld, uniq: %ld.\n",
		       req->uc_outSize, count, hdr.opcode, hdr.unique);
		count = req->uc_outSize; /* don't have more space! */
	}
        if (copy_from_user(req->uc_data, buf, count))
	        return -EFAULT;

	/* adjust outsize. is this usefull ?? */
        req->uc_outSize = count;	
        req->uc_flags |= REQ_WRITE;

	CDEBUG(D_PSDEV, 
	       "Found! Count %d for (opc,uniq)=(%ld,%ld), upc_req at %x\n", 
	        count, hdr.opcode, hdr.unique, (int)&req);

        wake_up(&req->uc_sleep);
        return(count);  
}

/*
 *	Read a message from the kernel to Venus
 */

static ssize_t coda_psdev_read(struct file * file, char * buf, 
			       size_t count, loff_t *off)
{
        struct venus_comm *vcp = &coda_upc_comm;
        struct upc_req *req;
	int result = count ;

        CDEBUG(D_PSDEV, "count %d\n", count);
        if (list_empty(&(vcp->vc_pending))) {
              return -1;	
        }
    
        req = list_entry((vcp->vc_pending.next), struct upc_req, uc_chain);
        list_del(&(req->uc_chain));

        /* Move the input args into userspace */
        if (req->uc_inSize <= count)
              result = req->uc_inSize;

        if (count < req->uc_inSize) {
                printk ("psdev_read: Venus read %d bytes of %d in message\n",
			count, req->uc_inSize);
        }

        if ( copy_to_user(buf, req->uc_data, result))
	        return -EFAULT;
        
        /* If request was a signal, don't enqueue */
        if (req->uc_opcode == CODA_SIGNAL) {
                    CDEBUG(D_PSDEV, "vcread: signal msg (%d, %d)\n", 
                              req->uc_opcode, req->uc_unique);
              CODA_FREE(req->uc_data, sizeof(struct coda_in_hdr));
              CODA_FREE(req, sizeof(struct upc_req));
              return count;
        }
    
        req->uc_flags |= REQ_READ;
        list_add(&(req->uc_chain), vcp->vc_processing.prev);

        return result;
}


static int coda_psdev_open(struct inode * inode, struct file * file)
{
        struct venus_comm *vcp = &coda_upc_comm;
        ENTRY;
	
	/* first opener: must be lento. Initialize & take its pid */
	if ( file->f_flags == O_RDWR ) {
		if ( vcp->vc_pid ) {
			printk("Venus pid already set to %d!!\n", vcp->vc_pid);
			return -1;
		}
		if ( vcp->vc_inuse ) {
			printk("psdev_open: Cannot O_RDWR while open.\n");
			return -1;
		}
	}
	
	vcp->vc_inuse++;
	MOD_INC_USE_COUNT;


	if ( file->f_flags == O_RDWR ) {
		vcp->vc_pid = current->pid;
		vcp->vc_seq = 0;
		INIT_LIST_HEAD(&vcp->vc_pending);
		INIT_LIST_HEAD(&vcp->vc_processing);
	}

	CDEBUG(D_PSDEV, "inuse: %d, vc_pid %d, caller %d\n",
	       vcp->vc_inuse, vcp->vc_pid, current->pid);

	EXIT;
        return 0;
}



static int coda_psdev_release(struct inode * inode, struct file * file)
{
        struct venus_comm *vcp = &coda_upc_comm;
        struct upc_req *req;
	struct list_head *lh, *next;
	ENTRY;

	if ( !vcp->vc_inuse ) {
		printk("psdev_release: Not open.\n");
		return -1;
	}

	vcp->vc_inuse--;
	MOD_DEC_USE_COUNT;
	CDEBUG(D_PSDEV, "inuse: %d, vc_pid %d, caller %d\n",
	       vcp->vc_inuse, vcp->vc_pid, current->pid);

	if ( vcp->vc_pid != current->pid ) {
		return 0;
	}
        
	vcp->vc_pid = 0;
        /* Wakeup clients so they can return. */
	CDEBUG(D_PSDEV, "wake up pending clients\n");
	lh = vcp->vc_pending.next;
	next = lh;
	while ( (lh = next) != &vcp->vc_pending) {
		next = lh->next;
		req = list_entry(lh, struct upc_req, uc_chain);
		/* Async requests need to be freed here */
		if (req->uc_flags & REQ_ASYNC) {
			CODA_FREE(req->uc_data, sizeof(struct coda_in_hdr));
			CODA_FREE(req, (u_int)sizeof(struct upc_req));
			continue;
		}
		wake_up(&req->uc_sleep);
        }
        
	lh = &vcp->vc_processing;
	CDEBUG(D_PSDEV, "wake up processing clients\n");
	while ( (lh = lh->next) != &vcp->vc_processing) {
		req = list_entry(lh, struct upc_req, uc_chain);
	        wake_up(&req->uc_sleep);
        }
	CDEBUG(D_PSDEV, "Done.\n");

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
      NULL,
      coda_psdev_release,    /* release */
      NULL,                  /* fsync */
      NULL,                  /* fasync */
      NULL,                  /* check_media_change */
      NULL,                  /* revalidate */
      NULL                   /* lock */
};



__initfunc(int init_coda(void)) 
{
	int status;
	printk(KERN_INFO "Coda Kernel/Venus communications, v4.6.0, braam@cs.cmu.edu\n");
	
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

int init_coda_psdev(void)
{
	if(register_chrdev(CODA_PSDEV_MAJOR,"coda_psdev", &coda_psdev_fops)) {
              printk(KERN_ERR "coda_psdev: unable to get major %d\n", 
		     CODA_PSDEV_MAJOR);
              return -EIO;
	}
	memset(&coda_upc_comm, 0, sizeof(coda_upc_comm));
	memset(&coda_super_info, 0, sizeof(coda_super_info));

	coda_sysctl_init();

	return 0;
}


#ifdef MODULE

MODULE_AUTHOR("Peter J. Braam <braam@cs.cmu.edu>");

int init_module(void)
{
	int status;
	printk(KERN_INFO "Coda Kernel/Venus communications (module), v5.0-pre1, braam@cs.cmu.edu.\n");

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
	coda_sysctl_clean();
}

#endif

