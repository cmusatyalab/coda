/*
 * Pioctl operations for Coda.
 * Original version: (C) 1996 Peter Braam 
 * Rewritten for Linux 2.1: (C) 1997 Carnegie Mellon University
 *
 * Carnegie Mellon encourages users of this code to contribute improvements
 * to the Coda project. Contact Peter Braam <coda@cs.cmu.edu>.
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/locks.h>
#include <asm/segment.h>
#include <linux/string.h>
#include <asm/uaccess.h>

#include <linux/coda.h>
#include <linux/coda_linux.h>
#include <linux/coda_fs_i.h>
#include <linux/coda_cache.h>
#include <linux/coda_psdev.h>

/* pioctl ops */
static int coda_ioctl_permission(struct inode *inode, int mask);
static int coda_ioctl_open(struct inode *i, struct file *f);
static int coda_ioctl_release(struct inode *i, struct file *f);
static int coda_pioctl(struct inode * inode, struct file * filp, 
                       unsigned int cmd, unsigned long arg);

/* exported from this file */
struct inode_operations coda_ioctl_inode_operations =
{
	&coda_ioctl_operations,
	NULL,	                /* create */
	NULL,	                /* lookup */
	NULL,	                /* link */
	NULL,	                /* unlink */
	NULL,	                /* symlink */
	NULL,	                /* mkdir */
	NULL,	                /* rmdir */
	NULL,		        /* mknod */
	NULL,		        /* rename */
	NULL,	                /* readlink */
	NULL,	                /* follow_link */
	NULL,	                /* readpage */
	NULL,		        /* writepage */
	NULL,		        /* bmap */
	NULL,	                /* truncate */
	coda_ioctl_permission,  /* permission */
	NULL,                   /* smap */
	NULL,                   /* update page */
        NULL                    /* revalidate */
};

struct file_operations coda_ioctl_operations = {
	NULL,		        /* lseek - default should work for coda */
	NULL,                   /* read */
	NULL,                   /* write */
	NULL,          		/* readdir */
	NULL,			/* select - default */
	coda_pioctl,	        /* ioctl */
	NULL,                   /* mmap */
	coda_ioctl_open,        /* open */
	NULL,
	coda_ioctl_release,     /* release */
	NULL,		        /* fsync */
};

/* the coda pioctl inode ops */
static int coda_ioctl_permission(struct inode *inode, int mask)
{
        ENTRY;

        return 0;
}

/* The pioctl file ops*/
int coda_ioctl_open(struct inode *i, struct file *f)
{

        ENTRY;

        CDEBUG(D_PIOCTL, "File inode number: %ld\n", 
	       f->f_dentry->d_inode->i_ino);

	EXIT;
        return 0;
}

int coda_ioctl_release(struct inode *i, struct file *f) 
{
        return 0;
}


static int coda_pioctl(struct inode * inode, struct file * filp, 
		       unsigned int cmd, unsigned long user_data)
{
        struct dentry *target_de;
        int error;
	struct PioctlData data;
        struct inode *target_inode = NULL;
        struct coda_inode_info *cnp;

        ENTRY;
        /* get the Pioctl data arguments from user space */
        if (copy_from_user(&data, (int *)user_data, sizeof(data))) {
	    return -EINVAL;
	}
       
        /* 
         * Look up the pathname. Note that the pathname is in 
         * user memory, and namei takes care of this
         */
	CDEBUG(D_PIOCTL, "namei, data.follow = %d\n", 
	       data.follow);
        if ( data.follow ) {
                target_de = namei(data.path);
	} else {
	        target_de = lnamei(data.path);
	}
		
	if ( IS_ERR(target_de) ) {
                CDEBUG(D_PIOCTL, "error: lookup fails.\n");
		return PTR_ERR(target_de);
        } else {
	        target_inode = target_de->d_inode;
	}
	
	CDEBUG(D_PIOCTL, "target ino: 0x%ld, dev: 0x%d\n",
	       target_inode->i_ino, target_inode->i_dev);

	/* return if it is not a Coda inode */
	if ( target_inode->i_sb != inode->i_sb ) {
  	        if ( target_de )
		        dput(target_de);
	        return  -EINVAL;
	}

	/* now proceed to make the upcall */
        cnp = ITOC(target_inode);

	error = venus_pioctl(inode->i_sb, &(cnp->c_fid), cmd, &data);

        CDEBUG(D_PIOCTL, "ioctl on inode %ld\n", target_inode->i_ino);
	CDEBUG(D_DOWNCALL, "dput on ino: %ld, icount %d, dcount %d\n", target_inode->i_ino, 
	       target_inode->i_count, target_de->d_count);
        if ( target_de ) 
	        dput(target_de);
        return error;
}

