/*
 * Super block/filesystem wide operations
 *
 * Peter J. Braam <braam@maths.ox.ac.uk>, 
 * Michael Callahan <callahan@maths.ox.ac.uk> Aug 1996
 * Rewritten for Linux 2.1.57 Peter Braam <braam@cs.cmu.edu>
 */

#define __NO_VERSION__
#include <linux/config.h>
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
#include <linux/coda_cnode.h>
#include <linux/coda_namecache.h>


/* VFS super_block ops */
static struct super_block *coda_read_super(struct super_block *, void *, int);
static void coda_read_inode(struct inode *);
static int  coda_notify_change(struct inode *inode, struct iattr *attr);
static void coda_put_inode(struct inode *);
static void coda_delete_inode(struct inode *);
static void coda_put_super(struct super_block *);
static int coda_statfs(struct super_block *sb, struct statfs *buf, 
		       int bufsiz);


/* helper functions */
static inline struct vcomm *coda_psinode2vcomm(struct inode *inode);
static int coda_get_psdev(void *, struct inode **);
int coda_fetch_inode(struct inode *, struct coda_vattr *);

extern inline struct vcomm *coda_psdev_vcomm(struct inode *inode);
extern int coda_cnode_make(struct inode **inode, ViceFid *fid, 
			   struct super_block *sb);
extern struct cnode *coda_cnode_alloc(void);
extern void coda_cnode_free(struct cnode *);
char *coda_f2s(struct ViceFid *, char *);

extern int cfsnc_initialized;
extern int coda_debug;
extern int coda_print_entry;

extern struct inode_operations coda_file_inode_operations;
extern struct inode_operations coda_dir_inode_operations;
extern struct inode_operations coda_ioctl_inode_operations;
extern struct inode_operations coda_symlink_inode_operations;
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
	char str[50];

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

        lock_super(sb);
        sb->u.generic_sbp = sbi;
        sb->s_blocksize = 1024;	/* XXXXX */
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
	printk("coda_read_super: rootfid is %s\n", coda_f2s(&fid, str));
	
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
                coda_cnode_free(ITOC(root));
        }
        sb->s_dev = 0;
        return NULL;
}


static void coda_put_super(struct super_block *sb)
{
        struct coda_sb_info *sb_info;

        ENTRY;

        lock_super(sb);

        sb->s_dev = 0;
	coda_cache_clear_all(sb);
	sb_info = coda_sbp(sb);
	sb_info->sbi_vcomm->vc_sb = NULL;
	printk("Coda: Bye bye.\n");
	memset(sb_info, 0, sizeof(* sb_info));

        unlock_super(sb);
        MOD_DEC_USE_COUNT;
EXIT;
}

/* all filling in of inodes postponed until lookup */
static void coda_read_inode(struct inode *inode)
{
	ENTRY;
        inode->u.generic_ip = NULL;
	EXIT;
}

static void coda_put_inode(struct inode *inode) 
{
        CDEBUG(D_INODE,"ino: %ld, cnp: %x\n", inode->i_ino,
	       (int) inode->u.generic_ip);
}

static void coda_delete_inode(struct inode *inode)
{
        struct cnode *cnp;
        struct inode *open_inode;

        ENTRY;
        CDEBUG(D_SUPER, " inode->ino: %ld, count: %d\n", 
	       inode->i_ino, inode->i_count);        

	if ( inode->i_ino == CTL_INO ) {
	        clear_inode(inode);
		return;
	}

        cnp = ITOC(inode);

        open_inode = cnp->c_ovp;
        if ( open_inode ) {
                CDEBUG(D_SUPER, "DELINO cached file: ino %ld count %d.\n",  
		       open_inode->i_ino,  open_inode->i_count);
                cnp->c_ovp = NULL;
                iput( open_inode );
        }
	
	coda_cache_clear_cnp(cnp);

	inode->u.generic_ip = NULL;
        coda_cnode_free(cnp);
        clear_inode(inode);
EXIT;
}

static int  coda_notify_change(struct inode *inode, struct iattr *iattr)
{
        struct cnode *cnp;
        struct coda_vattr vattr;
        int error;
ENTRY;
        memset(&vattr, 0, sizeof(vattr)); 
        cnp = ITOC(inode);
        CHECK_CNODE(cnp);

        coda_iattr_to_vattr(iattr, &vattr);
        vattr.va_type = C_VNON; /* cannot set type */
	CDEBUG(D_SUPER, "vattr.va_mode %o\n", vattr.va_mode);

        error = venus_setattr(inode->i_sb, &cnp->c_fid, &vattr);

        if ( !error ) {
	        coda_vattr_to_iattr(inode, &vattr); 
		coda_cache_clear_cnp(cnp);
/* 		cfsnc_zapfid(&(cnp->c_fid)); */
        }
	CDEBUG(D_SUPER, "inode.i_mode %o, error %d\n", 
	       inode->i_mode, error);

	EXIT;
        return error;
}

/*  we need _something_ */
static int coda_statfs(struct super_block *sb, struct statfs *buf, 
		       int bufsiz)
{
	struct statfs tmp;

#define NB_SFS_SIZ 0x895440

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

/*   */
int coda_fetch_inode (struct inode *inode, struct coda_vattr *attr)
{
        struct cnode *cp;
        int ino, error=0;
        CDEBUG(D_SUPER, "fetch for ino: %ld\n", inode->i_ino);

        ino = inode->i_ino;
        if (!ino)
                printk("coda_fetch_inode: inode called with i_ino = 0 (don't worry)\n");

        inode->i_op = NULL;
        inode->i_mode = 0;

        cp = ITOC(inode);
        CHECK_CNODE(cp);

        /* root inode  */
        if (cp->c_fid.Volume == 0 &&
            cp->c_fid.Vnode == 0 &&
            cp->c_fid.Unique == 0) {
	        inode->i_ino = 1;
		inode->i_op = NULL;
		return 0;
        }
        
        if (IS_CTL_FID( &(cp->c_fid) )) {
                /* This is the special magic control file.  
		   Venus doesn't want
                   to hear a GETATTR about this! */
                inode->i_op = &coda_ioctl_inode_operations;
                return 0;
        }

        if ( ! attr ) {
                printk("coda_fetch_inode: called with NULL vattr, ino %ld\n",
		       inode->i_ino);
                return -1; /* XXX */
        }

        if (coda_debug & D_SUPER ) print_vattr(attr);
        coda_vattr_to_iattr(inode, attr);

        if (S_ISREG(inode->i_mode))
                inode->i_op = &coda_file_inode_operations;
        else if (S_ISDIR(inode->i_mode))
                inode->i_op = &coda_dir_inode_operations;
        else if (S_ISLNK(inode->i_mode))
                inode->i_op = &coda_symlink_inode_operations;
        else {
                printk ("coda_read_inode: what kind of inode is this? i_mode = %o\n", inode->i_mode);
                inode->i_op = NULL;
        }
        return error;
}

static inline struct vcomm *
coda_psinode2vcomm(struct inode *inode) 
{
        
	unsigned int minor = MINOR(inode->i_rdev);
CDEBUG(D_PSDEV,"minor %d\n", minor);
	if ( minor < MAX_CODADEVS ) 
	      return &(psdev_vcomm[minor]);
	else
	      return NULL;
}

struct coda_sb_info *coda_psinode2sbi(struct inode *inode) 
{
	unsigned int minor = MINOR(inode->i_rdev);

	CDEBUG(D_PSDEV,"minor %d\n", minor);
	if ( minor < MAX_CODADEVS ) 
	        return &(coda_super_info[minor]);
	else
	        return NULL;
}

static int coda_get_psdev(void *data, struct inode **res_dev)
{
        char **psdev_path;
        struct inode *psdev = 0;
	struct dentry *ent=NULL;

 
	if ( ! data ) { 
		printk("coda_read_super: no data!\n");
		goto error;
	} else {
		psdev_path = data;
	}
        ent = namei((char *) *psdev_path);
        if (IS_ERR(ent)) {
		printk("namei error %ld for %d\n", PTR_ERR(ent), 
		       (int) psdev_path);
		goto error;
        }
	psdev = ent->d_inode;
        

        if (!S_ISCHR(psdev->i_mode)) {
		printk("not a character device\n");
		goto error;
        }
	CDEBUG(D_PSDEV,"major %d, minor %d, count %d\n", 
	       MAJOR(psdev->i_rdev), 
	       MINOR(psdev->i_rdev), psdev->i_count);
        
        if (MAJOR(psdev->i_rdev) != CODA_PSDEV_MAJOR) {
		printk("device %d not a Coda PSDEV device\n", 
		       MAJOR(psdev->i_rdev));
		goto error;
        }

        if (MINOR(psdev->i_rdev) >= MAX_CODADEVS) { 
		printk("minor %d not an allocated Coda PSDEV\n", 
		       psdev->i_rdev);
		goto error;
        }

        if (psdev->i_count < 1) {
		printk("coda device minor %d not open (i_count = %d)\n", 
		       MINOR(psdev->i_rdev), psdev->i_count);
		goto error;
        }
        
        *res_dev = psdev;

        return 0;
      
EXIT;  
error:
        return 1;
}
