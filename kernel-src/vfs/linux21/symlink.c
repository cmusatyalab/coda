/*
 * Symlink inode operations for Coda filesystem
 * Original version: (C) 1996 P. Braam and M. Callahan
 * Rewritten for Linux 2.1. (C) 1997 Carnegie Mellon University
 * 
 * Carnegie Mellon encourages users to contribute improvements to
 * the Coda project. Contact Peter Braam (coda@cs.cmu.edu).
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/locks.h>
#include <asm/segment.h>
#include <asm/uaccess.h>
#include <linux/string.h>

#include <linux/coda.h>
#include <linux/coda_linux.h>
#include <linux/coda_psdev.h>
#include <linux/coda_fs_i.h>
#include <linux/coda_cache.h>
#include <linux/coda_proc.h>

static int coda_readlink(struct dentry *de, char *buffer, int length);
static struct dentry *coda_follow_link(struct dentry *, struct dentry *, 
				       unsigned int);

struct inode_operations coda_symlink_inode_operations = {
	NULL,			/* no file-operations */
	NULL,			/* create */
	NULL,			/* lookup */
	NULL,			/* link */
	NULL,			/* unlink */
	NULL,			/* symlink */
	NULL,			/* mkdir */
	NULL,			/* rmdir */
	NULL,			/* mknod */
	NULL,			/* rename */
	coda_readlink,		/* readlink */
	coda_follow_link,     	/* follow_link */
	NULL,			/* readpage */
	NULL,			/* writepage */
	NULL,			/* bmap */
	NULL,			/* truncate */
	NULL,            	/* permission */
	NULL,                   /* smap */
	NULL,                   /* update page */
        NULL                    /* revalidate */
};

static int coda_readlink(struct dentry *de, char *buffer, int length)
{
	struct inode *inode = de->d_inode;
        int len;
	int error;
        char *buf;
	struct coda_inode_info *cp;
        ENTRY;

        cp = ITOC(inode);
	coda_vfs_stat.readlink++;

        /* the maximum length we receive is len */
        if ( length > CODA_MAXPATHLEN ) 
	        len = CODA_MAXPATHLEN;
	else
	        len = length;
	CODA_ALLOC(buf, char *, len);
	if ( !buf ) 
	        return -ENOMEM;
	
	error = venus_readlink(inode->i_sb, &(cp->c_fid), buf, &len);

        CDEBUG(D_INODE, "result %s\n", buf);
	if (! error) {
		copy_to_user(buffer, buf, len);
		put_user('\0', buffer + len);
		error = len;
	}
	if ( buf )
		CODA_FREE(buf, len);
	return error;
}

static struct dentry *coda_follow_link(struct dentry *de, struct dentry *base,
				       unsigned int follow)
{
	struct inode *inode = de->d_inode;
	int error;
	struct coda_inode_info *cnp;
	unsigned int len;
	char mem[CODA_MAXPATHLEN];
	char *path;
	ENTRY;
	CDEBUG(D_INODE, "(%x/%ld)\n", inode->i_dev, inode->i_ino);
	
        cnp = ITOC(inode);
	coda_vfs_stat.follow_link++;

	len = CODA_MAXPATHLEN;
	error = venus_readlink(inode->i_sb, &(cnp->c_fid), mem, &len);

	if (error) {
		dput(base);
		return ERR_PTR(error);
	}
	len = strlen(mem);
	path = kmalloc(len + 1, GFP_KERNEL);
	if (!path) {
		dput(base);
		return ERR_PTR(-ENOMEM);
	}
	memcpy(path, mem, len);
	path[len] = 0;

	base = lookup_dentry(path, base, follow);
	kfree(path);
	return base;
}
