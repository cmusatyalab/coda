/*
 * File operations for Coda.
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
#include <linux/coda_psdev.h>
#include <linux/coda_cache.h>
#include <linux/coda_proc.h>

/* file operations */
static int coda_readpage(struct file *file, struct page * page);
static ssize_t coda_file_read(struct file *f, char *buf, size_t count, loff_t *off);
static ssize_t coda_file_write(struct file *f, const char *buf, size_t count, loff_t *off);
static int coda_file_mmap(struct file * file, struct vm_area_struct * vma);

/* also exported from this file (used for dirs) */
int coda_fsync(struct file *, struct dentry *dentry);

struct inode_operations coda_file_inode_operations = {
	&coda_file_operations,	/* default file operations */
	NULL,			/* create */
	NULL,		        /* lookup */
	NULL,			/* link */
	NULL,		        /* unlink */
	NULL,			/* symlink */
	NULL,			/* mkdir */
	NULL,			/* rmdir */
	NULL,			/* mknod */
	NULL,		        /* rename */
	NULL,			/* readlink */
	NULL,			/* follow_link */
	coda_readpage,    	/* readpage */
	NULL,			/* writepage */
	NULL,			/* bmap */
	NULL,			/* truncate */
        coda_permission,        /* permission */
	NULL,                   /* smap */
	NULL,                   /* update page */
        coda_revalidate_inode   /* revalidate */
};

struct file_operations coda_file_operations = {
	NULL,		        /* lseek - default should work for coda */
	coda_file_read,         /* read */
	coda_file_write,        /* write */
	NULL,          		/* readdir */
	NULL,			/* select - default */
	NULL,		        /* ioctl */
	coda_file_mmap,         /* mmap */
	coda_open,              /* open */
	NULL,
	coda_release,           /* release */
	coda_fsync,		/* fsync */
	NULL,                   /* fasync */
	NULL,                   /* check_media_change */
	NULL,                   /* revalidate */
	NULL                    /* lock */
};

/*  File file operations */
static int coda_readpage(struct file * coda_file, struct page * page)
{
	struct dentry *de = coda_file->f_dentry;
	struct inode *coda_inode = de->d_inode;
	struct dentry cont_dentry;
	struct file cont_file;
        struct coda_inode_info *cii;

        ENTRY;
	coda_vfs_stat.readpage++;
        
        cii = ITOC(coda_inode);

        if ( ! cii->c_ovp ) {
		printk("coda_readpage: no open inode for ino %ld, %s\n", 
		       coda_inode->i_ino, de->d_name.name);
                return -ENXIO;
        }
       
        coda_prepare_openfile(coda_inode, coda_file, cii->c_ovp,
			      &cont_file, &cont_dentry);

        CDEBUG(D_INODE, "coda ino: %ld, cached ino %ld, page offset: %lx\n", 
	       coda_inode->i_ino, cii->c_ovp->i_ino, page->offset);

        generic_readpage(&cont_file, page);
        EXIT;
        return 0;
}

static int coda_file_mmap(struct file * file, struct vm_area_struct * vma)
{
        struct coda_inode_info *cii;
	int res;

	coda_vfs_stat.file_mmap++;

        ENTRY;
	cii = ITOC(file->f_dentry->d_inode);
	cii->c_mmcount++;
  
	res =generic_file_mmap(file, vma);
	EXIT;
	return res;
}

static ssize_t coda_file_read(struct file *coda_file, char *buff, 
			   size_t count, loff_t *ppos)
{
        struct coda_inode_info *cnp;
	struct inode *coda_inode = coda_file->f_dentry->d_inode;
        struct inode *cont_inode = NULL;
        struct file  cont_file;
	struct dentry cont_dentry;
        int result = 0;

	ENTRY;
	coda_vfs_stat.file_read++;

        cnp = ITOC(coda_inode);
        CHECK_CNODE(cnp);
	
        cont_inode = cnp->c_ovp;
        if ( cont_inode == NULL ) {
                printk("coda_file_read: cached inode is 0!\n");
                return -1;
        }

        coda_prepare_openfile(coda_inode, coda_file, cont_inode, 
			      &cont_file, &cont_dentry);

        if (!cont_file.f_op || ! cont_file.f_op->read) { 
                printk( "container file has no read in file operations.\n");
                return -1;
        }

        result = cont_file.f_op->read(&cont_file , buff, count, 
				      &(cont_file.f_pos));

        CDEBUG(D_FILE, "ops at %x result %d, count %d, position: %d\n", 
	       (int)cont_file.f_op, result, count, (int)cont_file.f_pos);

        coda_restore_codafile(coda_inode, coda_file, cont_inode, &cont_file);
        return result;
}


static ssize_t coda_file_write(struct file *coda_file, const char *buff, 
			    size_t count, loff_t *ppos)
{
        struct coda_inode_info *cnp;
	struct inode *coda_inode = coda_file->f_dentry->d_inode;
        struct inode *cont_inode = NULL;
        struct file  cont_file;
	struct dentry cont_dentry;
        int result = 0;

        ENTRY;
	coda_vfs_stat.file_write++;

        cnp = ITOC(coda_inode);
        CHECK_CNODE(cnp);

        cont_inode = cnp->c_ovp;
        if ( cont_inode == NULL ) {
                printk("coda_file_write: cached inode is 0!\n");
                return -1; 
        }

        coda_prepare_openfile(coda_inode, coda_file, cont_inode, 
			      &cont_file, &cont_dentry);

        if (!cont_file.f_op || !cont_file.f_op->write) {
                printk("coda_file_write: container file has no file ops.\n");
                return -1;
        }

	down(&cont_inode->i_sem);
        result = cont_file.f_op->write(&cont_file , buff, count, 
				       &(cont_file.f_pos));
	up(&cont_inode->i_sem);
        coda_restore_codafile(coda_inode, coda_file, cont_inode, &cont_file);
	
	if (result)
		cnp->c_flags |= C_VATTR;

        return result;
}

int coda_fsync(struct file *coda_file, struct dentry *coda_dentry)
{
        struct coda_inode_info *cnp;
	struct inode *coda_inode = coda_dentry->d_inode;
        struct inode *cont_inode = NULL;
        struct file  cont_file;
	struct dentry cont_dentry;
        int result = 0;
        ENTRY;
	coda_vfs_stat.fsync++;

	if (!(S_ISREG(coda_inode->i_mode) || S_ISDIR(coda_inode->i_mode) ||
	      S_ISLNK(coda_inode->i_mode)))
		return -EINVAL;

        cnp = ITOC(coda_inode);
        CHECK_CNODE(cnp);

        cont_inode = cnp->c_ovp;
        if ( cont_inode == NULL ) {
                printk("coda_file_write: cached inode is 0!\n");
                return -1; 
        }

        coda_prepare_openfile(coda_inode, coda_file, cont_inode, 
			      &cont_file, &cont_dentry);

	down(&cont_inode->i_sem);

        result = file_fsync(&cont_file ,&cont_dentry);
	if ( result == 0 ) {
		result = venus_fsync(coda_inode->i_sb, &(cnp->c_fid));
	}

	up(&cont_inode->i_sem);

        coda_restore_codafile(coda_inode, coda_file, cont_inode, &cont_file);
        return result;
}
/* 
 * support routines
 */

/* instantiate the container file and dentry object to do io */                
void coda_prepare_openfile(struct inode *i, struct file *coda_file, 
			   struct inode *cont_inode, struct file *cont_file,
			   struct dentry *cont_dentry)
{
        cont_file->f_pos = coda_file->f_pos;
        cont_file->f_mode = coda_file->f_mode;
        cont_file->f_flags = coda_file->f_flags;
        cont_file->f_count  = coda_file->f_count;
        cont_file->f_owner  = coda_file->f_owner;
	cont_file->f_op = cont_inode->i_op->default_file_ops;
	cont_file->f_dentry = cont_dentry;
        cont_file->f_dentry->d_inode = cont_inode;
        return ;
}

/* update the Coda file & inode after I/O */
void coda_restore_codafile(struct inode *coda_inode, struct file *coda_file, 
			   struct inode *open_inode, struct file *open_file)
{
        coda_file->f_pos = open_file->f_pos;
	/* XXX what about setting the mtime here too? */
	/* coda_inode->i_mtime = open_inode->i_mtime; */
	coda_inode->i_size = open_inode->i_size;
        return;
}

/* grab the ext2 inode of the container file */
int coda_inode_grab(dev_t dev, ino_t ino, struct inode **ind)
{
        struct super_block *sbptr;

        sbptr = get_super(dev);

        if ( !sbptr ) {
                printk("coda_inode_grab: coda_find_super returns NULL.\n");
                return -ENXIO;
        }
                
        *ind = NULL;
        *ind = iget(sbptr, ino);

        if ( *ind == NULL ) {
                printk("coda_inode_grab: iget(dev: %d, ino: %ld) 
                       returns NULL.\n", dev, ino);
                return -ENOENT;
        }
	CDEBUG(D_FILE, "ino: %ld, ops at %x\n", ino, (int)(*ind)->i_op);
        return 0;
}

