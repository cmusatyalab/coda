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

/* dir inode-ops */
static int coda_create(struct inode *dir, struct dentry *new, int mode);
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

/* support routines */
static int coda_venus_readdir(struct file *filp, void *dirent, 
			      filldir_t filldir);
int coda_fsync(struct file *, struct dentry *dentry);

struct dentry_operations coda_dentry_operations =
{
	NULL, /* revalidate */
	NULL, /* hash */
	NULL,
	coda_dentry_delete
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
	NULL,		        /* mknod */
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
        NULL                    /* revalidate */
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
        CDEBUG(D_INODE, "name %s, len %d in ino %ld\n", 
	       name, length, dir->i_ino);

	if (!dir || !S_ISDIR(dir->i_mode)) {
		printk("coda_lookup: inode is NULL or not a directory\n");
		return -ENOENT;
	}

	dircnp = ITOC(dir);
	CHECK_CNODE(dircnp);

	if ( length > CFS_MAXNAMLEN ) {
	        printk("name too long: lookup, %s (%*s)\n", 
		       coda_f2s(&dircnp->c_fid), length, name);
		return -ENAMETOOLONG;
	}
	
	CDEBUG(D_INODE, "lookup: %*s in %s\n", length, name, 
	       coda_f2s(&dircnp->c_fid));

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
	if (!error || (error == -CFS_NOCACHE) ) {
		if (error == -CFS_NOCACHE) 
			dropme = 1;
	    	error = coda_cnode_make(&res_inode, &resfid, dir->i_sb);
		if (error)
			return -error;
	} else if (error != -ENOENT) {
	        CDEBUG(D_INODE, "error for %s(%*s)%d\n",
		       coda_f2s(&dircnp->c_fid), length, name, error);
		return error;
	}
	CDEBUG(D_INODE, "lookup: %s is (%s) type %d result %d, dropme %d\n",
	       name, coda_f2s(&resfid), type, error, dropme);

exit:
	entry->d_time = 0;
	entry->d_op = &coda_dentry_operations;
	d_add(entry, res_inode);
	if ( dropme ) 
		d_drop(entry);
        EXIT;
        return 0;
}


int coda_permission(struct inode *inode, int mask)
{
        struct coda_inode_info *cp;
        int error;
 
        ENTRY;

        if ( mask == 0 ) {
                EXIT;
                return 0;
        }

	if ( coda_access_cache == 1 ) {
		if ( coda_cache_check(inode, mask) ) {
			return 0; 
		}
	}

        cp = ITOC(inode);
        CHECK_CNODE(cp);

        CDEBUG(D_INODE, "mask is %o\n", mask);
        error = venus_access(inode->i_sb, &(cp->c_fid), mask);
    
        CDEBUG(D_INODE, "fid: %s, ino: %ld (mask: %o) error: %d\n", 
	       coda_f2s(&(cp->c_fid)), inode->i_ino, mask, error);

	if ( error == 0 ) {
		coda_cache_enter(inode, mask);
	}

        return error; 
}



/* creation routines: create, mkdir, link, symlink */

static int coda_create(struct inode *dir, struct dentry *de, int mode)
{
        int error=0;
        struct coda_inode_info *dircnp;
	const char *name=de->d_name.name;
	int length=de->d_name.len;
	struct inode *result = NULL;
	struct ViceFid newfid;
	struct coda_vattr attrs;

	CDEBUG(D_INODE, "name: %s, length %d, mode %o\n",name, length, mode);

        if (!dir || !S_ISDIR(dir->i_mode)) {
                printk("coda_create: inode is null or not a directory\n");
                return -ENOENT;
        }

	if (coda_isroot(dir) && coda_iscontrol(name, length))
		return -EPERM;

	dircnp = ITOC(dir);
        CHECK_CNODE(dircnp);

        if ( length > CFS_MAXNAMLEN ) {
		char str[50];
		printk("name too long: create, %s(%s)\n", 
		       coda_f2s(&dircnp->c_fid), name);
		return -ENAMETOOLONG;
        }

	error = venus_create(dir->i_sb, &(dircnp->c_fid), name, length, 
				0, mode, &newfid, &attrs);

        if ( error ) {
		char str[50];
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
	dircnp->c_flags &= ~C_VATTR;
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


	if (!dir || !S_ISDIR(dir->i_mode)) {
		printk("coda_mkdir: inode is NULL or not a directory\n");
		return -ENOENT;
	}

        if ( len > CFS_MAXNAMLEN )
                return -ENAMETOOLONG;

	if (coda_isroot(dir) && coda_iscontrol(name, len))
		return -EPERM;

        dircnp = ITOC(dir);
        CHECK_CNODE(dircnp);

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
	dircnp->c_flags &= ~C_VATTR;
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
	char str[50];
	int error;

        ENTRY;

	if (coda_isroot(dir_inode) && coda_iscontrol(name, len))
		return -EPERM;

        dir_cnp = ITOC(dir_inode);
        CHECK_CNODE(dir_cnp);
        cnp = ITOC(inode);
        CHECK_CNODE(cnp);

	CDEBUG(D_INODE, "old: fid: %s\n", coda_f2s(&(cnp->c_fid)));
	CDEBUG(D_INODE, "directory: %s\n", coda_f2s(&(dir_cnp->c_fid)));

        if ( len > CFS_MAXNAMLEN ) {
                printk("coda_link: name too long. \n");
                return -ENAMETOOLONG;
        }

        error = venus_link(dir_inode->i_sb,&(cnp->c_fid), &(dir_cnp->c_fid), 
			   (const char *)name, len);

	if (  ! error ) { 
		dir_cnp->c_flags &= ~C_VATTR;
		inode->i_nlink++;
		d_instantiate(de, inode);
	} else {
		d_drop(de);
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

	if (coda_isroot(dir_inode) && coda_iscontrol(name, len))
		return -EPERM;

	if ( len > CFS_MAXNAMLEN )
                return -ENAMETOOLONG;

	symlen = strlen(symname);
	if ( symlen > CFS_MAXPATHLEN )
                return -ENAMETOOLONG;

        CDEBUG(D_INODE, "symname: %s, length: %d\n", symname, symlen);

	/*
	 * This entry is now negative. Since we do not create
	 * an inode for the entry we have to drop it. 
	 */
	d_drop(de);

	error = venus_symlink(dir_inode->i_sb, &(dir_cnp->c_fid), name, len, 
			      symname, symlen);

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
        struct coda_inode_info *dircnp;
        int error;
	const char *name = de->d_name.name;
	int len = de->d_name.len;
	char fidstr[50];

	ENTRY;

        dircnp = ITOC(dir);
        CHECK_CNODE(dircnp);

        CDEBUG(D_INODE, " %s in %s, ino %ld\n", name , 
	       coda_f2s(&(dircnp->c_fid)), dir->i_ino);

        /* this file should no longer be in the namecache! */

        error = venus_remove(dir->i_sb, &(dircnp->c_fid), name, len);

        if ( error ) {
                CDEBUG(D_INODE, "upc returned error %d\n", error);
                return error;
        }

        /* cache management */
	dircnp->c_flags &= ~C_VATTR;
	
	de->d_inode->i_nlink--;
	d_delete(de);

        return 0;
}

int coda_rmdir(struct inode *dir, struct dentry *de)
{
        struct coda_inode_info *dircnp;
	const char *name = de->d_name.name;
	int len = de->d_name.len;
        int error, rehash = 0;

	if (!dir || !S_ISDIR(dir->i_mode)) {
		printk("coda_rmdir: inode is NULL or not a directory\n");
		return -ENOENT;
	}
        dircnp = ITOC(dir);
        CHECK_CNODE(dircnp);

	if (len > CFS_MAXNAMLEN)
		return -ENAMETOOLONG;

	error = -EBUSY;
	if (de->d_count > 1) {
		/* Attempt to shrink child dentries ... */
		shrink_dcache_parent(de);
		if (de->d_count > 1)
			return error;
	}
	/* Drop the dentry to force a new lookup */
	if (!list_empty(&de->d_hash)) {
		d_drop(de);
		rehash = 1;
	}

	/* update i_nlink and free the inode before unlinking;
	   if rmdir fails a new lookup set i_nlink right.*/
	if (de->d_inode->i_nlink)
		de->d_inode->i_nlink --;
	d_delete(de);

	error = venus_rmdir(dir->i_sb, &(dircnp->c_fid), name, len);

        if ( error ) {
                CDEBUG(D_INODE, "upc returned error %d\n", error);
                return error;
        }

	if (rehash)
		d_add(de, NULL);
	/* XXX how can mtime be set? */

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
        int error, rehash = 0, update = 1;
ENTRY;
        old_cnp = ITOC(old_dir);
        CHECK_CNODE(old_cnp);
        new_cnp = ITOC(new_dir);
        CHECK_CNODE(new_cnp);

        CDEBUG(D_INODE, "old: %s, (%d length, %d strlen), new: %s (%d length, %d strlen).\n", old_name, old_length, strlen(old_name), new_name, new_length, strlen(new_name));

        if ( (old_length > CFS_MAXNAMLEN) || new_length > CFS_MAXNAMLEN ) {
                return -ENAMETOOLONG;
        }

        /* the old file should go from the namecache */
/*         cfsnc_zapfile(old_cnp, (const char *)old_name, old_length); */
/*         cfsnc_zapfile(new_cnp, (const char *)new_name, new_length); */

        /* cross directory moves */
	if (new_dir != old_dir  &&
	    S_ISDIR(old_inode->i_mode) && 
	    old_dentry->d_count > 1)
	        shrink_dcache_parent(old_dentry);
		
	/* We must prevent any new references to the
	 * target while the rename is in progress, so
	 * we unhash the dentry.  */
	if (!list_empty(&new_dentry->d_hash)) {
	        d_drop(new_dentry);
		rehash = 1;
	}

        error = venus_rename(old_dir->i_sb, &(old_cnp->c_fid), 
			     &(new_cnp->c_fid), old_length, new_length, 
			     (const char *) old_name, (const char *)new_name);

	if (rehash) {
		d_add(new_dentry, new_inode);
	}

        if ( error ) {
                CDEBUG(D_INODE, "returned error %d\n", error);
                return error;
        }
	/* Update the dcache if needed */
	if (update)
	        d_move(old_dentry, new_dentry);
         
        CDEBUG(D_INODE, "result %d\n", error); 

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

        if (!inode || !inode->i_sb || !S_ISDIR(inode->i_mode)) {
                printk("coda_readdir: inode is NULL or not a directory\n");
                return -EBADF;
        }

        cnp = ITOC(inode);
        CHECK_CNODE(cnp);
        
        if ( !cnp->c_ovp ) {
                CDEBUG(D_FILE, "open inode pointer = NULL.\n");
                return -ENODEV;
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
	return result;
        EXIT;
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
        unsigned short flags = f->f_flags;
	unsigned short coda_flags = coda_flags_to_cflags(flags);

        ENTRY;
        
        CDEBUG(D_SPECIAL, "OPEN inode number: %ld, flags %o.\n", 
	       f->f_dentry->d_inode->i_ino, flags);

        cnp = ITOC(i);
        CHECK_CNODE(cnp);

	error = venus_open(i->i_sb, &(cnp->c_fid), coda_flags, &ino, &dev); 
	if (error) {
	        CDEBUG(D_FILE, "venus: dev %d, inode %ld, out->result %d\n",
		       dev, ino, error);
		return error;
	}

        /* coda_upcall returns ino number of cached object, get inode */
        CDEBUG(D_FILE, "cache file dev %d, ino %ld\n", dev, ino);

        if ( ! cnp->c_ovp ) {
                error = coda_inode_grab(dev, ino, &cont_inode);
                
                if ( error ){
                        printk("coda_open: coda_inode_grab error %d.", error);
                        if (cont_inode) iput(cont_inode);
			return error;
                }
                CDEBUG(D_FILE, "GRAB: coda_inode_grab: ino %ld, ops at %x\n", 
		       cont_inode->i_ino, (int)cont_inode->i_op);
                cnp->c_ovp = cont_inode; 
        } 
        cnp->c_ocount++;

        /* if opened for writing flush cache entry.  */
/*         if ( flags & (O_WRONLY | O_RDWR) ) { */
/* 	        cfsnc_zapfid(&(cnp->c_fid)); */
/* 	}  */

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
        unsigned short flags = f->f_flags;
	unsigned short cflags = coda_flags_to_cflags(flags);

        ENTRY;

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

	error = venus_release(i->i_sb, &(cnp->c_fid), cflags);

        CDEBUG(D_FILE, "coda_release: result: %d\n", error);
        return error;
}

/* support routines */
/* 
 * this structure is manipulated by filldir in vfs layer.
 * the count holds the remaining amount of space in the getdents buffer,
 * beyond the current_dir pointer.
 */

struct getdents_callback {
	struct linux_dirent * current_dir;
	struct linux_dirent * previous;
	int count;
	int error;
};

static int coda_venus_readdir(struct file *filp, void *getdent, 
			      filldir_t filldir)
{
        int result = 0,  offset, count, pos, error = 0;
	int errfill;
        caddr_t buff = NULL;
        struct venus_dirent *vdirent;
        struct getdents_callback *dents_callback;
        int string_offset;
	int size;
        char debug[255];

        ENTRY;        

        /* we also need the ofset of the string in the dirent struct */
        string_offset = sizeof ( char )* 2  + sizeof(unsigned int) + 
                        sizeof(unsigned short);

        dents_callback = (struct getdents_callback *) getdent;

        size = count =  dents_callback->count;
        CODA_ALLOC(buff, void *, size);
        if ( ! buff ) { 
                printk("coda_venus_readdir: out of memory.\n");
                return -ENOMEM;
        }

        /* we use this routine to read the file into our buffer */
        result = read_exec(filp->f_dentry, filp->f_pos, buff, count, 1);
        if ( result < 0) {
                printk("coda_venus_readdir: cannot read directory %d.\n",
		       result);
                error = result;
                goto exit;
        }
        if ( result == 0) {
                error = result;
                goto exit;
        }

        /* Parse and write into user space. Filldir tells us when done! */
        offset = filp->f_pos;
        pos = 0;
        CDEBUG(D_FILE, "offset %d, count %d.\n", offset, count);

        while ( pos + string_offset < result ) {
                vdirent = (struct venus_dirent *) (buff + pos);

                /* test if the name is fully in the buffer */
                if ( pos + string_offset + (int) vdirent->d_namlen >= result ){
                        break;
                }
                
                /* now we are certain that we can read the entry from buff */

                /* for debugging, get the string out */
                memcpy(debug, vdirent->d_name, vdirent->d_namlen);
                *(debug + vdirent->d_namlen) = '\0';

                /* if we don't have a null entry, copy it */
                if ( vdirent->d_fileno ) {
                        int namlen  = vdirent->d_namlen;
                        off_t offs  = filp->f_pos; 
                        ino_t ino   = vdirent->d_fileno;
                        char *name  = vdirent->d_name;
                        /* adjust count */
                        count = dents_callback->count;

			errfill = filldir(dents_callback,  name, namlen, 
					  offs, ino); 
CDEBUG(D_FILE, "ino %ld, namlen %d, reclen %d, type %d, pos %d, string_offs %d, name %s, offset %d, count %d.\n", vdirent->d_fileno, vdirent->d_namlen, vdirent->d_reclen, vdirent->d_type, pos,  string_offset, debug, (u_int) offs, dents_callback->count);

		      /* errfill means no space for filling in this round */
                      if ( errfill < 0 ) break;
                }
                /* next one */
                filp->f_pos += (unsigned int) vdirent->d_reclen;
                pos += (unsigned int) vdirent->d_reclen;
        } 

exit:
        CODA_FREE(buff, size);
        return error;
}
