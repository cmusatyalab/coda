
/*
 * Directory operations for Coda filesystem
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

/* dir inode-ops */
static int coda_create(struct inode *dir, struct dentry *new, int mode);
static int coda_mknod(struct inode *dir, struct dentry *new, int mode, int rdev);
static int coda_lookup(struct inode *dir, struct dentry *target);
static int coda_link(struct dentry *old_dentry, struct inode *dir_inode, 
		     struct dentry *entry);
static int coda_unlink(struct inode *dir_inode, struct dentry *entry);
static int coda_symlink(struct inode *dir_inode, struct dentry *entry,
			const char *symname);
static int coda_mkdir(struct inode *dir_inode, struct dentry *entry, int mode);
static int coda_rmdir(struct inode *dir_inode, struct dentry *entry);
static int coda_rename(struct inode *old_inode, struct dentry *old_dentry, 
                       struct inode *new_inode, struct dentry *new_dentry);

/* dir file-ops */
static int coda_readdir(struct file *file, void *dirent, filldir_t filldir);

/* dentry ops */
static int coda_dentry_revalidate(struct dentry *de);
static void coda_dentry_delete(struct dentry *);

/* support routines */
static int coda_venus_readdir(struct file *filp, void *dirent, 
			      filldir_t filldir);
int coda_fsync(struct file *, struct dentry *dentry);

int coda_crossvol_rename = 0;
int coda_hasmknod = 0;

struct dentry_operations coda_dentry_operations =
{
	coda_dentry_revalidate, /* revalidate */
	NULL, /* hash */
	NULL, /* compare */
	coda_dentry_delete /* delete */
};

struct inode_operations coda_dir_inode_operations =
{
	&coda_dir_operations,
	coda_create,	        /* create */
	coda_lookup,	        /* lookup */
	coda_link,	        /* link */
	coda_unlink,            /* unlink */
	coda_symlink,	        /* symlink */
	coda_mkdir,	        /* mkdir */
	coda_rmdir,   	        /* rmdir */
	coda_mknod,	        /* mknod */
	coda_rename,	        /* rename */
	NULL,	                /* readlink */
	NULL,	                /* follow_link */
	NULL,	                /* readpage */
	NULL,		        /* writepage */
	NULL,		        /* bmap */
	NULL,	                /* truncate */
	coda_permission,        /* permission */
	NULL,                   /* smap */
	NULL,                   /* update page */
        coda_revalidate_inode   /* revalidate */
};

struct file_operations coda_dir_operations = {
        NULL,                   /* lseek */
        NULL,                   /* read -- bad  */
        NULL,                   /* write */
        coda_readdir,           /* readdir */
        NULL,                   /* select */
        NULL,                   /* ioctl */
        NULL,                   /* mmap */
        coda_open,              /* open */
	NULL,
        coda_release,           /* release */
	coda_fsync,             /* fsync */
        NULL,                   
	NULL,
	NULL
};


/* inode operations for directories */
/* acces routines: lookup, readlink, permission */
static int coda_lookup(struct inode *dir, struct dentry *entry)
{
        struct coda_inode_info *dircnp;
	struct inode *res_inode = NULL;
	struct ViceFid resfid;
	int dropme = 0; /* to indicate entry should not be cached */
	int type;
	int error = 0;
	const char *name = entry->d_name.name;
	size_t length = entry->d_name.len;
	
        ENTRY;

	dircnp = ITOC(dir);

	if ( length > CODA_MAXNAMLEN ) {
	        printk("name too long: lookup, %s (%*s)\n", 
		       coda_f2s(&dircnp->c_fid), length, name);
		return -ENAMETOOLONG;
	}


	if (!dir || !S_ISDIR(dir->i_mode)) {
		printk("coda_lookup: inode is NULL or not a directory\n");
		return -ENOTDIR;
	}


        CDEBUG(D_INODE, "name %s, len %d in ino %ld, fid %s\n", 
	       name, length, dir->i_ino, coda_f2s(&dircnp->c_fid));

        /* control object, create inode on the fly */
        if (coda_isroot(dir) && coda_iscontrol(name, length)) {
	        error = coda_cnode_makectl(&res_inode, dir->i_sb);
		CDEBUG(D_SPECIAL, 
		       "Lookup on CTL object; dir ino %ld, count %d\n", 
		       dir->i_ino, dir->i_count);
                goto exit;
        }

        error = venus_lookup(dir->i_sb, &(dircnp->c_fid), 
			     (const char *)name, length, &type, &resfid);

	res_inode = NULL;
	if (!error) {
		if (type & CODA_NOCACHE) {
			type &= (~CODA_NOCACHE);
			CDEBUG(D_INODE, "dropme set for %s\n", 
			       coda_f2s(&resfid));
			dropme = 1;
		}
	    	error = coda_cnode_make(&res_inode, &resfid, dir->i_sb);
		if (error)
			return error;
	} else if (error != -ENOENT) {
	        CDEBUG(D_INODE, "error for %s(%*s)%d\n",
		       coda_f2s(&dircnp->c_fid), length, name, error);
		return error;
	}
	CDEBUG(D_INODE, "lookup: %s is (%s), type %d result %d, dropme %d\n",
	       name, coda_f2s(&resfid), type, error, dropme);

exit:
	entry->d_time = 0;
	entry->d_op = &coda_dentry_operations;
	d_add(entry, res_inode);
	if ( dropme ) {
		d_drop(entry);
		ITOC(res_inode)->c_flags |= C_VATTR;
	}
        EXIT;
        return 0;
}


int coda_permission(struct inode *inode, int mask)
{
        struct coda_inode_info *cp = ITOC(inode);
        int error;
 
        ENTRY;
	coda_vfs_stat.permission++;
	coda_permission_stat.count++;

        if ( mask == 0 ) {
                return 0;
        }

	if ( coda_access_cache == 1 ) {
		if ( coda_cache_check(inode, mask) ) {
			coda_permission_stat.hit_count++;
			return 0; 
		}
	}

        cp = ITOC(inode);

        CDEBUG(D_INODE, "mask is %o\n", mask);
        error = venus_access(inode->i_sb, &(cp->c_fid), mask);
    
        CDEBUG(D_INODE, "fid: %s, ino: %ld (mask: %o) error: %d\n", 
	       coda_f2s(&(cp->c_fid)), inode->i_ino, mask, error);

	if ( error == 0 ) {
		coda_cache_enter(inode, mask);
	}

        return error; 
}



/* creation routines: create, mknod, mkdir, link, symlink */

static int coda_create(struct inode *dir, struct dentry *de, int mode)
{
        int error=0;
        struct coda_inode_info *dircnp;
	const char *name=de->d_name.name;
	int length=de->d_name.len;
	struct inode *result = NULL;
	struct ViceFid newfid;
	struct coda_vattr attrs;

	ENTRY;
	coda_vfs_stat.create++;

	CDEBUG(D_INODE, "name: %s, length %d, mode %o\n",name, length, mode);

        if (!dir || !S_ISDIR(dir->i_mode)) {
                printk("coda_create: inode is null or not a directory\n");
                return -ENOENT;
        }

	if (coda_isroot(dir) && coda_iscontrol(name, length))
		return -EPERM;

	dircnp = ITOC(dir);

        if ( length > CODA_MAXNAMLEN ) {
		printk("name too long: create, %s(%s)\n", 
		       coda_f2s(&dircnp->c_fid), name);
		return -ENAMETOOLONG;
        }

	error = venus_create(dir->i_sb, &(dircnp->c_fid), name, length, 
				0, mode, 0, &newfid, &attrs);

        if ( error ) {
		CDEBUG(D_INODE, "create: %s, result %d\n",
		       coda_f2s(&newfid), error); 
		d_drop(de);
		return error;
	}

	error = coda_cnode_make(&result, &newfid, dir->i_sb);
	if ( error ) {
		d_drop(de);
		result = NULL;
		return error;
	}

	/* invalidate the directory cnode's attributes */
	dircnp->c_flags |= C_VATTR;
	d_instantiate(de, result);
        return 0;
}			     

static int coda_mknod(struct inode *dir, struct dentry *de, int mode, int rdev)
{
        int error=0;
        struct coda_inode_info *dircnp;
	const char *name=de->d_name.name;
	int length=de->d_name.len;
	struct inode *result = NULL;
	struct ViceFid newfid;
	struct coda_vattr attrs;

	if ( coda_hasmknod == 0 )
		return -EIO;

	coda_vfs_stat.create++;

	CDEBUG(D_INODE, "name: %s, length %d, mode %o, rdev %x\n",name, length, mode, rdev);

        if (!dir || !S_ISDIR(dir->i_mode)) {
                printk("coda_mknod: inode is null or not a directory\n");
                return -ENOENT;
        }

	if (coda_isroot(dir) && coda_iscontrol(name, length))
		return -EPERM;

	dircnp = ITOC(dir);

        if ( length > CODA_MAXNAMLEN ) {
		printk("name too long: mknod, %s(%s)\n", 
		       coda_f2s(&dircnp->c_fid), name);
		return -ENAMETOOLONG;
        }

	error = venus_create(dir->i_sb, &(dircnp->c_fid), name, length, 
				0, mode, rdev, &newfid, &attrs);

        if ( error ) {
		CDEBUG(D_INODE, "mknod: %s, result %d\n",
		       coda_f2s(&newfid), error); 
		d_drop(de);
		return error;
	}

	error = coda_cnode_make(&result, &newfid, dir->i_sb);
	if ( error ) {
		d_drop(de);
		result = NULL;
		return error;
	}

	/* invalidate the directory cnode's attributes */
	dircnp->c_flags |= C_VATTR;
	d_instantiate(de, result);
        return 0;
}			     

static int coda_mkdir(struct inode *dir, struct dentry *de, int mode)
{
        struct coda_inode_info *dircnp;
	struct inode *inode;
	struct coda_vattr attr;
	const char *name = de->d_name.name;
	int len = de->d_name.len;
	int error;
	struct ViceFid newfid;

	ENTRY;
	coda_vfs_stat.mkdir++;

	if (!dir || !S_ISDIR(dir->i_mode)) {
		printk("coda_mkdir: inode is NULL or not a directory\n");
		return -ENOENT;
	}

        if ( len > CODA_MAXNAMLEN )
                return -ENAMETOOLONG;

	if (coda_isroot(dir) && coda_iscontrol(name, len))
		return -EPERM;

        dircnp = ITOC(dir);

	CDEBUG(D_INODE, "mkdir %s (len %d) in %s, mode %o.\n", 
	       name, len, coda_f2s(&(dircnp->c_fid)), mode);

	attr.va_mode = mode;
	error = venus_mkdir(dir->i_sb, &(dircnp->c_fid), 
			       name, len, &newfid, &attr);
        
        if ( error ) {
	        CDEBUG(D_INODE, "mkdir error: %s result %d\n", 
		       coda_f2s(&newfid), error); 
		d_drop(de);
                return error;
        }
         
	CDEBUG(D_INODE, "mkdir: new dir has fid %s.\n", 
	       coda_f2s(&newfid)); 

	error = coda_cnode_make(&inode, &newfid, dir->i_sb);
	if ( error ) {
		d_drop(de);
		return error;
	}
	
	/* invalidate the directory cnode's attributes */
	dircnp->c_flags |= C_VATTR;
	dir->i_nlink++;
	d_instantiate(de, inode);
        return 0;
}

/* try to make de an entry in dir_inodde linked to source_de */ 
static int coda_link(struct dentry *source_de, struct inode *dir_inode, 
	  struct dentry *de)
{
	struct inode *inode = source_de->d_inode;
        const char * name = de->d_name.name;
	int len = de->d_name.len;
        struct coda_inode_info *dir_cnp, *cnp;
	int error;

        ENTRY;
	coda_vfs_stat.link++;

	if (coda_isroot(dir_inode) && coda_iscontrol(name, len))
		return -EPERM;

        dir_cnp = ITOC(dir_inode);
        cnp = ITOC(inode);

	CDEBUG(D_INODE, "old: fid: %s\n", coda_f2s(&(cnp->c_fid)));
	CDEBUG(D_INODE, "directory: %s\n", coda_f2s(&(dir_cnp->c_fid)));

        if ( len > CODA_MAXNAMLEN ) {
                printk("coda_link: name too long. \n");
                return -ENAMETOOLONG;
        }

        error = venus_link(dir_inode->i_sb,&(cnp->c_fid), &(dir_cnp->c_fid), 
			   (const char *)name, len);

	if (  ! error ) { 
		dir_cnp->c_flags |= C_VATTR;
		++inode->i_count;
		d_instantiate(de, inode);
		inode->i_nlink++;
	} else  {
		d_drop(de);
		return error;
	}

        CDEBUG(D_INODE, "link result %d\n",error);
	EXIT;
        return(error);
}


static int coda_symlink(struct inode *dir_inode, struct dentry *de,
			const char *symname)
{
        const char *name = de->d_name.name;
	int len = de->d_name.len;
        struct coda_inode_info *dir_cnp = ITOC(dir_inode);
	int symlen;
        int error=0;
        
        ENTRY;
	coda_vfs_stat.symlink++;

	if (coda_isroot(dir_inode) && coda_iscontrol(name, len))
		return -EPERM;

	if ( len > CODA_MAXNAMLEN )
                return -ENAMETOOLONG;

	symlen = strlen(symname);
	if ( symlen > CODA_MAXPATHLEN )
                return -ENAMETOOLONG;

        CDEBUG(D_INODE, "symname: %s, length: %d\n", symname, symlen);

	/*
	 * This entry is now negative. Since we do not create
	 * an inode for the entry we have to drop it. 
	 */
	d_drop(de);
	error = venus_symlink(dir_inode->i_sb, &(dir_cnp->c_fid), name, len, 
			      symname, symlen);

	/* mtime is no good anymore */
	if ( !error ) {
		dir_cnp->c_flags |= C_VATTR;
	}

        CDEBUG(D_INODE, "in symlink result %d\n",error);
        EXIT;
        return error;
}

/* destruction routines: unlink, rmdir */
int coda_unlink(struct inode *dir, struct dentry *de)
{
        struct coda_inode_info *dircnp = ITOC(dir);
        int error;
	const char *name = de->d_name.name;
	int len = de->d_name.len;

	ENTRY;
	coda_vfs_stat.unlink++;

        CDEBUG(D_INODE, " %s in %s, dirino %ld\n", name , 
	       coda_f2s(&(dircnp->c_fid)), dir->i_ino);

        error = venus_remove(dir->i_sb, &(dircnp->c_fid), name, len);
        if ( error ) {
                CDEBUG(D_INODE, "upc returned error %d\n", error);
                return error;
        }

        /* cache management: mtime has changed, ask Venus */
	dircnp->c_flags |= C_VATTR;
	de->d_inode->i_nlink--;
	d_delete(de);

        return 0;
}

int coda_rmdir(struct inode *dir, struct dentry *de)
{
        struct coda_inode_info *dircnp;
	const char *name = de->d_name.name;
	int len = de->d_name.len;
        int error;

	ENTRY;
	coda_vfs_stat.rmdir++;

	if (!dir || !S_ISDIR(dir->i_mode)) {
		printk("coda_rmdir: inode is NULL or not a directory\n");
		return -ENOENT;
	}
        dircnp = ITOC(dir);

	if (len > CODA_MAXNAMLEN)
		return -ENAMETOOLONG;

	if (!list_empty(&de->d_hash))
		return -EBUSY;
	error = venus_rmdir(dir->i_sb, &(dircnp->c_fid), name, len);

        if ( error ) {
                CDEBUG(D_INODE, "upc returned error %d\n", error);
                return error;
        }

	if (de->d_inode->i_nlink)
		de->d_inode->i_nlink --;

        return 0;
}

/* rename */
static int coda_rename(struct inode *old_dir, struct dentry *old_dentry, 
		       struct inode *new_dir, struct dentry *new_dentry)
{
        const char *old_name = old_dentry->d_name.name;
        const char *new_name = new_dentry->d_name.name;
	int old_length = old_dentry->d_name.len;
	int new_length = new_dentry->d_name.len;
	struct inode *old_inode = old_dentry->d_inode;
	struct inode *new_inode = new_dentry->d_inode;
        struct coda_inode_info *new_cnp, *old_cnp;
        int error;

	ENTRY;
	coda_vfs_stat.rename++;

        if ( (old_length > CODA_MAXNAMLEN) || new_length > CODA_MAXNAMLEN ) {
                return -ENAMETOOLONG;
        }

        old_cnp = ITOC(old_dir);
        new_cnp = ITOC(new_dir);

        CDEBUG(D_INODE, "old: %s, (%d length, %d strlen), new: %s"
	       "(%d length, %d strlen).old:d_count: %d, new:d_count: %d\n", 
	       old_name, old_length, strlen(old_name), new_name, new_length, 
	       strlen(new_name),old_dentry->d_count, new_dentry->d_count);

	if (new_inode == old_inode)
		return 0;

	/* make sure target is not in use */
	if (new_inode && S_ISDIR(new_inode->i_mode)) { 
		/*
                 * Prune any children before testing for busy.
                 */
                if (new_dentry->d_count > 1)
                        shrink_dcache_parent(new_dentry);

                if (new_dentry->d_count > 1)
                        return -EBUSY;
        }

	/* the C library will do unlink/create etc */
	if ( coda_crossvol_rename == 0 && 
	     old_cnp->c_fid.Volume != new_cnp->c_fid.Volume )
		return -EXDEV;

        error = venus_rename(old_dir->i_sb, &(old_cnp->c_fid), 
			     &(new_cnp->c_fid), old_length, new_length, 
			     (const char *) old_name, (const char *)new_name);

        if ( error ) {
                CDEBUG(D_INODE, "returned error %d\n", error);
                return error;
        }

	coda_flag_inode(new_inode, C_VATTR);
	coda_flag_inode(old_dir, C_VATTR);
	coda_flag_inode(new_dir, C_VATTR);

	CDEBUG(D_INODE, "result %d\n", error); 
	d_move(old_dentry, new_dentry);

	EXIT;
	return 0;
}



/* file operations for directories */
int coda_readdir(struct file *file, void *dirent,  filldir_t filldir)
{
        int result = 0;
        struct coda_inode_info *cnp;
        struct file open_file;
	struct dentry open_dentry;
	struct inode *inode=file->f_dentry->d_inode;

        ENTRY;
	coda_vfs_stat.readdir++;

        if (!inode || !inode->i_sb || !S_ISDIR(inode->i_mode)) {
                printk("coda_readdir: inode is NULL or not a directory\n");
                return -EBADF;
        }

        cnp = ITOC(inode);
        if ( !cnp->c_ovp ) {
                CDEBUG(D_FILE, "open inode pointer = NULL.\n");
                return -EIO;
        }

	coda_prepare_openfile(inode, file, cnp->c_ovp, &open_file,
			      &open_dentry);
        if ( S_ISREG(cnp->c_ovp->i_mode) ) {
                /* Venus: we must read Venus dirents from the file */
                result = coda_venus_readdir(&open_file, dirent, filldir);
        } else {
                /* potemkin case: we are handed a directory inode */
                result = open_file.f_op->readdir(&open_file, dirent, filldir);
        }
	coda_restore_codafile(inode, file, cnp->c_ovp, &open_file);
        EXIT;
	return result;
}

/* ask venus to cache the file and return the inode of the container file,
   put this inode pointer in the cnode for future read/writes */
int coda_open(struct inode *i, struct file *f)
{
        ino_t ino;
	dev_t dev;
        struct coda_inode_info *cnp;
        int error = 0;
        struct inode *cont_inode = NULL;
        unsigned short flags = f->f_flags & (~O_EXCL);
	unsigned short coda_flags = coda_flags_to_cflags(flags);
	struct coda_cred *cred;

        ENTRY;
	coda_vfs_stat.open++;

        CDEBUG(D_SPECIAL, "OPEN inode number: %ld, count %d, flags %o.\n", 
	       f->f_dentry->d_inode->i_ino, f->f_dentry->d_count, flags);

        cnp = ITOC(i);


	error = venus_open(i->i_sb, &(cnp->c_fid), coda_flags, &ino, &dev); 
	if (error) {
	        CDEBUG(D_FILE, "venus: dev %d, inode %ld, out->result %d\n",
		       dev, ino, error);
		return error;
	}

        /* coda_upcall returns ino number of cached object, get inode */
        CDEBUG(D_FILE, "cache file dev %d, ino %ld\n", dev, ino);
	error = coda_inode_grab(dev, ino, &cont_inode);
	
	if ( error || !cont_inode ){
		printk("coda_open: coda_inode_grab error %d.", error);
		if (cont_inode) 
			iput(cont_inode);
		return error;
	}

	CODA_ALLOC(cred, struct coda_cred *, sizeof(*cred));
	coda_load_creds(cred);
	f->private_data = cred;

        if (  cnp->c_ovp ) {
		iput(cnp->c_ovp);
		cnp->c_ovp = NULL;
	}
	cnp->c_ovp = cont_inode; 
        cnp->c_ocount++;

        CDEBUG(D_FILE, "result %d, coda i->i_count is %d for ino %ld\n", 
	       error, i->i_count, i->i_ino);
        CDEBUG(D_FILE, "cache ino: %ld, count %d, ops %x\n", 
	       cnp->c_ovp->i_ino, cnp->c_ovp->i_count,
	       (int)(cnp->c_ovp->i_op));
        EXIT;
        return 0;
}

int coda_release(struct inode *i, struct file *f)
{
        struct coda_inode_info *cnp;
        int error;
        unsigned short flags = (f->f_flags) & (~O_EXCL);
	unsigned short cflags = coda_flags_to_cflags(flags);
	struct coda_cred *cred;

        ENTRY;
	coda_vfs_stat.release++;

	cred = (struct coda_cred *)f->private_data;

        cnp =ITOC(i);
        CHECK_CNODE(cnp);
        CDEBUG(D_FILE,  
	       "RELEASE coda (ino %ld, ct %d) cache (ino %ld, ct %d)\n",
               i->i_ino, i->i_count, (cnp->c_ovp ? cnp->c_ovp->i_ino : 0),
               (cnp->c_ovp ? cnp->c_ovp->i_count : -99));


        /* even when c_ocount=0 we cannot put c_ovp to
         * NULL since the file may be mmapped.
	 * See code in inode.c (coda_put_inode) for
	 * further handling of close.
	 */

        --cnp->c_ocount;

        if (flags & (O_WRONLY | O_RDWR)) {
                --cnp->c_owrite;
        }

	error = venus_release(i->i_sb, &(cnp->c_fid), cflags, cred);

	CODA_FREE(cred, sizeof(*cred));
	f->private_data = NULL;

        CDEBUG(D_FILE, "coda_release: result: %d\n", error);
        return error;
}

/* support routines */
/* 
 * this structure is manipulated by filldir in vfs layer.
 * the count holds the remaining amount of space in the getdents buffer,
 * beyond the current_dir pointer.
 */


/* should be big enough to hold any single directory entry */
#define DIR_BUFSIZE 2048

static int coda_venus_readdir(struct file *filp, void *getdent, 
			      filldir_t filldir)
{
        int bufsize;
	int offset = filp->f_pos; /* offset in the directory file */
	int count = 0;
	int pos = 0;      /* offset in the block we read */
	int result = 0; /* either an error or # of entries returned */
	int errfill;
        char *buff = NULL;
        struct venus_dirent *vdirent;
        int string_offset = (int) (&((struct venus_dirent *)(0))->d_name);
	int i;

        ENTRY;        

        CODA_ALLOC(buff, char *, DIR_BUFSIZE);
        if ( !buff ) { 
                printk("coda_venus_readdir: out of memory.\n");
                return -ENOMEM;
        }

        /* we use this routine to read the file into our buffer */
        bufsize = read_exec(filp->f_dentry, filp->f_pos, buff, DIR_BUFSIZE, 1);
        if ( bufsize < 0) {
                printk("coda_venus_readdir: cannot read directory %d.\n",
		       bufsize);
                result = bufsize;
                goto exit;
        }
        if ( bufsize == 0) {
                result = 0;
                goto exit;
        }
	
        /* Parse and write into user space. Filldir tells us when done! */
        CDEBUG(D_FILE, "buffsize: %d offset %d, count %d.\n", 
	       bufsize, offset, count);

	i = 0;
	result = 0; 
        while ( pos + string_offset < bufsize && i < 1024) {
                vdirent = (struct venus_dirent *) (buff + pos);

                /* test if the name is fully in the buffer */
                if ( pos + string_offset + (int) vdirent->d_namlen >= bufsize ){
			if ( result == 0 )
				printk("CODA: Invalid directory cfino: %ld\n", 
				       filp->f_dentry->d_inode->i_ino);
                        break;
                }
                /* now we are certain that we can read the entry from buff */

                /* if we don't have a null entry, copy it */
                if ( vdirent->d_fileno && vdirent->d_reclen ) {
                        int namlen  = vdirent->d_namlen;
                        off_t offs  = filp->f_pos; 
                        ino_t ino   = vdirent->d_fileno;
                        char *name  = vdirent->d_name;

			errfill = filldir(getdent,  name, namlen, 
					  offs, ino); 
CDEBUG(D_FILE, "entry %d: ino %ld, namlen %d, reclen %d, type %d, pos %d, string_offs %d, name %*s, offset %d, result: %d, errfill: %d.\n", i,vdirent->d_fileno, vdirent->d_namlen, vdirent->d_reclen, vdirent->d_type, pos,  string_offset, vdirent->d_namlen, vdirent->d_name, (u_int) offs, result, errfill);
			/* errfill means no space for filling in this round */
			if ( errfill < 0 ) {
				result = 0;
				break;
			}
                        /* adjust count */
                        result++;
                }
                /* next one */
                filp->f_pos += vdirent->d_reclen;
		if ( filp->f_pos > filp->f_dentry->d_inode->i_size )
			break; 
		if ( !vdirent->d_reclen ) {
			printk("CODA: Invalid directory, cfino: %ld\n", 
			       filp->f_dentry->d_inode->i_ino);
			result = -EINVAL;
			break;
		}
                pos += (unsigned int) vdirent->d_reclen;
		i++;
        } 

       if ( i >= 1024 ) {
               printk("Repeating too much in readdir %ld\n", 
                      filp->f_dentry->d_inode->i_ino);
               result = -EINVAL;
       }

exit:
        CODA_FREE(buff, DIR_BUFSIZE);
        return result;
}

/* called when a cache lookup succeeds */
static int coda_dentry_revalidate(struct dentry *de)
{
	int valid = 1;
	struct inode *inode = de->d_inode;
	struct coda_inode_info *cii;
	ENTRY;

	if (!inode)
		return 1;

	cii = ITOC(de->d_inode);
	if (coda_isroot(inode))
		return 1;
	if (is_bad_inode(inode))
		return 0;

	if (! (cii->c_flags & (C_PURGE | C_FLUSH)) )
		return valid;

	shrink_dcache_parent(de);

	if (de->d_count > 1) {
		/* pretend it's valid, but don't change the flags */
		CDEBUG(D_DOWNCALL, "BOOM for: ino %ld, %s\n",
		       de->d_inode->i_ino, coda_f2s(&cii->c_fid));
		return 1;
	}

	/* propagate for a flush */
	if (cii->c_flags & C_FLUSH) 
		coda_flag_inode_children(inode, C_FLUSH);

	/* clear the flags. */
	cii->c_flags &= ~(C_VATTR | C_PURGE | C_FLUSH);

	return 0;
}

/*
 * This is the callback from dput() when d_count is going to 0.
 * We use this to unhash dentries with bad inodes.
 */
static void coda_dentry_delete(struct dentry * dentry)
{
	int flags;

	if (!dentry->d_inode) 
		return ;

	flags =  (ITOC(dentry->d_inode)->c_flags) & C_PURGE;
	if (is_bad_inode(dentry->d_inode) || flags) {
		CDEBUG(D_DOWNCALL, "bad inode, unhashing %s/%s, %ld\n", 
		       dentry->d_parent->d_name.name, dentry->d_name.name,
		       dentry->d_inode->i_ino);
		d_drop(dentry);
	}
}



/*
 * This is called when we want to check if the inode has
 * changed on the server.  Coda makes this easy since the
 * cache manager Venus issues a downcall to the kernel when this 
 * happens 
 */

int coda_revalidate_inode(struct dentry *dentry)
{
	struct coda_vattr attr;
	int error = 0;
	int old_mode;
	ino_t old_ino;
	struct inode *inode = dentry->d_inode;
	struct coda_inode_info *cii = ITOC(inode);

	ENTRY;
	CDEBUG(D_INODE, "revalidating: %*s/%*s\n", 
	       dentry->d_name.len, dentry->d_name.name,
	       dentry->d_parent->d_name.len, dentry->d_parent->d_name.name);

	if ( cii->c_flags == 0 )
		return 0;

	/* Venus closed the device .... */
	if ( cii->c_flags & C_DYING ) { 
		make_bad_inode(inode);
		return -EIO;
	}

	
	if (cii->c_flags & (C_VATTR | C_PURGE | C_FLUSH)) {
		error = venus_getattr(inode->i_sb, &(cii->c_fid), &attr);
		if ( error ) { 
			make_bad_inode(inode);
			return -EIO;
		}

		/* this inode may be lost if:
		   - it's ino changed 
		   - type changes must be permitted for repair and
		   missing mount points.
		*/
		old_mode = inode->i_mode;
		old_ino = inode->i_ino;
		coda_vattr_to_iattr(inode, &attr);


		if ((old_mode & S_IFMT) != (inode->i_mode & S_IFMT)) {
			printk("Coda: inode %ld, fid %s changed type!\n",
			       inode->i_ino, coda_f2s(&(cii->c_fid)));
		}

		/* the following can happen when a local fid is replaced 
		   with a global one, here we lose and declar the inode bad */
		if (inode->i_ino != old_ino) {
			make_bad_inode(inode);
			inode->i_mode = old_mode;
			return -EIO;
		}
		
		if ( cii->c_flags ) 
			coda_flag_inode_children(inode, C_FLUSH);
		
		cii->c_flags &= ~(C_VATTR | C_PURGE | C_FLUSH);
	}

	return 0;
}

