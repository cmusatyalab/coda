/*
 * Super block/filesystem wide operations
 *
 * Copryright (C) 1996 Peter J. Braam <braam@maths.ox.ac.uk> and 
 * Michael Callahan <callahan@maths.ox.ac.uk> 
 * 
 * Rewritten for Linux 2.1.  Peter Braam <braam@cs.cmu.edu>
 * Copyright (C) Carnegie Mellon University
 */

#define __NO_VERSION__
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/locks.h>
#include <linux/unistd.h>

#include <asm/system.h>
#include <asm/uaccess.h>

#include <linux/fs.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/locks.h>
#include <linux/string.h>
#include <asm/uaccess.h>
#include <linux/vmalloc.h>
#include <asm/segment.h>

#include <linux/coda.h>
#include <linux/coda_linux.h>
#include <linux/coda_psdev.h>
#include <linux/coda_fs_i.h>
#include <linux/coda_cache.h>


/* VFS super_block ops */
static struct super_block *coda_read_super(struct super_block *, void *, int);
static void coda_read_inode(struct inode *);
static int  coda_notify_change(struct dentry *dentry, struct iattr *attr);
static void coda_put_inode(struct inode *);
static void coda_delete_inode(struct inode *);
static void coda_put_super(struct super_block *);
static int coda_statfs(struct super_block *sb, struct statfs *buf, 
		       int bufsiz);

/* helper functions */
static struct vcomm *coda_psinode2vcomm(struct inode *inode);
static int coda_get_psdev(void *, struct inode **);
static struct coda_sb_info *coda_psinode2sbi(struct inode *inode);

/* exported operations */
struct super_operations coda_super_operations =
{
	coda_read_inode,        /* read_inode */
	NULL,                   /* write_inode */
	coda_put_inode,	        /* put_inode */
	coda_delete_inode,      /* delete_inode */
	coda_notify_change,	/* notify_change */
	coda_put_super,	        /* put_super */
	NULL,			/* write_super */
	coda_statfs,   		/* statfs */
	NULL			/* remount_fs */
};

/*
 * globals
 */
struct coda_sb_info coda_super_info[MAX_CODADEVS];


static struct super_block * coda_read_super(struct super_block *sb, 
					    void *data, int silent)
{
        struct inode *psdev = 0, *root = 0; 
	struct coda_sb_info *sbi = NULL;
	struct vcomm *vc = NULL;
        ViceFid fid;
	kdev_t dev = sb->s_dev;
        int error;

	ENTRY;
        MOD_INC_USE_COUNT; 
        if (coda_get_psdev(data, &psdev))
                goto error;

        vc = coda_psinode2vcomm(psdev);
        if ( !vc )
	        goto error;
	vc->vc_sb = sb;
	vc->vc_inuse = 1;

	sbi = coda_psinode2sbi(psdev);
	if ( !sbi )
	        goto error;
        sbi->sbi_psdev = psdev;
	sbi->sbi_vcomm = vc;
	INIT_LIST_HEAD(&(sbi->sbi_cchead));
	INIT_LIST_HEAD(&(sbi->sbi_volroothead));

        lock_super(sb);
        sb->u.generic_sbp = sbi;
        sb->s_blocksize = 1024;	/* XXXXX  what do we put here?? */
        sb->s_blocksize_bits = 10;
        sb->s_magic = CODA_SUPER_MAGIC;
        sb->s_dev = dev;
        sb->s_op = &coda_super_operations;

	/* get root fid from Venus: this needs the root inode */
	error = venus_rootfid(sb, &fid);
	if ( error ) {
	        printk("coda_read_super: coda_get_rootfid failed with %d\n",
		       error);
		sb->s_dev = 0;
	        unlock_super(sb);
		goto error;
	}	  
	printk("coda_read_super: rootfid is %s\n", coda_f2s(&fid));
	
	/* make root inode */
        error = coda_cnode_make(&root, &fid, sb);
        if ( error || !root ) {
	    printk("Failure of coda_cnode_make for root: error %d\n", error);
	    sb->s_dev = 0;
	    unlock_super(sb);
	    goto error;
	} 

	printk("coda_read_super: rootinode is %ld dev %d\n", 
	       root->i_ino, root->i_dev);
	sbi->sbi_root = root;
	sb->s_root = d_alloc_root(root, NULL);
	unlock_super(sb);
	EXIT;  
        return sb;

error:
EXIT;  
	MOD_DEC_USE_COUNT;
	if (sbi) {
		sbi->sbi_vcomm = NULL;
		sbi->sbi_root = NULL;
	}
	if ( vc ) {
		vc->vc_sb = NULL;
		vc->vc_inuse = 0;
	}
        if (root) {
                iput(root);
        }
        sb->s_dev = 0;
        return NULL;
}

static void coda_put_super(struct super_block *sb)
{
        struct coda_sb_info *sb_info;

        ENTRY;


        sb->s_dev = 0;
	coda_cache_clear_all(sb);
	sb_info = coda_sbp(sb);
	sb_info->sbi_vcomm->vc_inuse = 0;
	sb_info->sbi_vcomm->vc_sb = NULL;
	printk("Coda: Bye bye.\n");
	memset(sb_info, 0, sizeof(* sb_info));

        MOD_DEC_USE_COUNT;
	EXIT;
}

/* all filling in of inodes postponed until lookup */
static void coda_read_inode(struct inode *inode)
{
	struct coda_inode_info *cii;
	ENTRY;
	cii = ITOC(inode);
	cii->c_magic = 0;
	return;
}

static void coda_put_inode(struct inode *in) 
{
	ENTRY;

        CDEBUG(D_INODE,"ino: %ld, count %d\n", in->i_ino, in->i_count);

	if ( in->i_count == 1 ) 
		in->i_nlink = 0;
		
}

static void coda_delete_inode(struct inode *inode)
{
        struct coda_inode_info *cii;
        struct inode *open_inode;

        ENTRY;
        CDEBUG(D_SUPER, " inode->ino: %ld, count: %d\n", 
	       inode->i_ino, inode->i_count);        

        cii = ITOC(inode);
	if ( inode->i_ino == CTL_INO || cii->c_magic != CODA_CNODE_MAGIC ) {
	        clear_inode(inode);
		return;
	}


	if ( ! list_empty(&cii->c_volrootlist) )
		list_del(&cii->c_volrootlist);

        open_inode = cii->c_ovp;
        if ( open_inode ) {
                CDEBUG(D_SUPER, "DELINO cached file: ino %ld count %d.\n",  
		       open_inode->i_ino,  open_inode->i_count);
                cii->c_ovp = NULL;
                iput(open_inode);
        }
	
	coda_cache_clear_inode(inode);
	CDEBUG(D_DOWNCALL, "clearing inode: %ld, %x\n", inode->i_ino, cii->c_flags);
	inode->u.generic_ip = NULL;
        clear_inode(inode);
	EXIT;
}

static int  coda_notify_change(struct dentry *de, struct iattr *iattr)
{
	struct inode *inode = de->d_inode;
        struct coda_inode_info *cii;
        struct coda_vattr vattr;
        int error;
	
	ENTRY;
        memset(&vattr, 0, sizeof(vattr)); 
        cii = ITOC(inode);
        CHECK_CNODE(cii);

        coda_iattr_to_vattr(iattr, &vattr);
        vattr.va_type = C_VNON; /* cannot set type */
	CDEBUG(D_SUPER, "vattr.va_mode %o\n", vattr.va_mode);

        error = venus_setattr(inode->i_sb, &cii->c_fid, &vattr);

        if ( !error ) {
	        coda_vattr_to_iattr(inode, &vattr); 
		coda_cache_clear_inode(inode);
        }
	CDEBUG(D_SUPER, "inode.i_mode %o, error %d\n", 
	       inode->i_mode, error);

	EXIT;
        return error;
}

/*  we need _something_ for this routine. Let's mimic AFS */
static int coda_statfs(struct super_block *sb, struct statfs *buf, 
		       int bufsiz)
{
	struct statfs tmp;

	tmp.f_type = CODA_SUPER_MAGIC;
	tmp.f_bsize = 1024;
	tmp.f_blocks = 9000000;
	tmp.f_bfree = 9000000;
	tmp.f_bavail = 9000000 ;
	tmp.f_files = 9000000;
	tmp.f_ffree = 9000000;
	tmp.f_namelen = 0;
	copy_to_user(buf, &tmp, bufsiz);
	return 0; 
}


/* init_coda: used by filesystems.c to register coda */

struct file_system_type coda_fs_type = {
   "coda", 0, coda_read_super, NULL
};

int init_coda_fs(void)
{
	return register_filesystem(&coda_fs_type);
}

/* MODULE stuff is in psdev.c */

/*  helpers */
static struct vcomm *coda_psinode2vcomm(struct inode *inode) 
{
        
	unsigned int minor = MINOR(inode->i_rdev);
	CDEBUG(D_PSDEV,"minor %d\n", minor);
	if ( minor < MAX_CODADEVS ) 
	      return &(psdev_vcomm[minor]);
	else
	      return NULL;
}

static struct coda_sb_info *coda_psinode2sbi(struct inode *inode) 
{
	unsigned int minor = MINOR(inode->i_rdev);

	CDEBUG(D_PSDEV,"minor %d\n", minor);
	if ( (minor >= 0) && (minor < MAX_CODADEVS)) 
	        return &(coda_super_info[minor]);
	else
	        return NULL;
}

/* name lookup for psdev passed in by mount */
static int coda_get_psdev(void *data, struct inode **res_dev)
{
        char **psdev_path;
        struct inode *psdev = 0;
	struct dentry *ent=NULL;

 
	if ( ! data ) { 
		printk("coda_get_psdev: no data!\n");
		return 1;
	} 

	psdev_path = data;
        ent = namei((char *) *psdev_path);
        if (IS_ERR(ent)) {
		printk("namei error %ld for %d\n", PTR_ERR(ent), 
		       (int) psdev_path);
		return 1;
        }
	psdev = ent->d_inode;

        if (!S_ISCHR(psdev->i_mode)) {
		printk("not a character device\n");
		return 1;
        }
	CDEBUG(D_PSDEV,"major %d, minor %d, count %d\n", 
	       MAJOR(psdev->i_rdev), 
	       MINOR(psdev->i_rdev), psdev->i_count);
        
        if (MAJOR(psdev->i_rdev) != CODA_PSDEV_MAJOR) {
		printk("device %d not a Coda PSDEV device\n", 
		       MAJOR(psdev->i_rdev));
		return 1;
        }

        if (MINOR(psdev->i_rdev) >= MAX_CODADEVS) { 
		printk("minor %d not an allocated Coda PSDEV\n", 
		       psdev->i_rdev);
		return 1;
        }

        if (psdev->i_count < 1) {
		printk("coda device minor %d not open (i_count = %d)\n", 
		       MINOR(psdev->i_rdev), psdev->i_count);
		return 1;
        }
        
        *res_dev = psdev;
	EXIT;  
        return 0;
}
