/*
 * Inode operations for Coda filesystem
 * P. Braam and M. Callahan
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

#include <linux/coda.h>
#include <coda_linux.h>
#include <psdev.h>
#include <cnode.h>
#include <super.h>
#include <upcall.h>
#include "namecache.h"

/* prototypes */
static int coda_create(struct inode *dir, const char *name, int len, int mode,
		       struct inode **result);
static int coda_lookup(struct inode *dir, const char *name, int length, 
		       struct inode **res_inode);
static int coda_ioctl_permission(struct inode *inode, int mask);
static int coda_permission(struct inode *inode, int mask);
static int coda_readlink(struct inode *inode, char *buffer, int length);
int coda_unlink(struct inode *dir_inode, const char *name, int length);
int coda_mkdir(struct inode *dir_inode, const char *name, int length, 
               int mode);
int coda_rmdir(struct inode *dir_inode, const char *name, int length);
static int coda_rename(struct inode *old_inode, const char *old_name, 
                       int old_length, struct inode *new_inode, 
                       const char *new_name, int new_length, int must_be_dir);
static int coda_link(struct inode *old_inode, struct inode *dir_inode,
		    const char *name, int length);
static int coda_symlink(struct inode *dir_inode, const char *name, int namelen,
		       const char *symname);
static int coda_follow_link(struct inode * dir, struct inode * inode,
			    int flag, int mode, struct inode ** res_inode);
static int coda_readpage(struct inode *inode, struct page *page);
static int coda_cnode_makectl(struct inode **inode, struct super_block *sb);

/* external routines */
extern struct cnode *coda_cnode_alloc(void);
extern  int coda_cnode_make(struct inode **inode, ViceFid *, struct super_block *);

/* external data structures */
extern struct file_operations coda_file_operations;
extern struct file_operations coda_dir_operations;
extern struct file_operations coda_ioctl_operations;
extern int coda_debug;
extern int coda_print_entry;
extern int coda_access_cache;


/* exported from this file */

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
	coda_rename,		/* rename */
	NULL,			/* readlink */
	NULL,			/* follow_link */
	coda_readpage,    	/* readpage */
	NULL,			/* writepage */
	NULL,			/* bmap */
	NULL,			/* truncate */
        coda_permission         /* permission */
};

struct inode_operations coda_dir_inode_operations =
{
	&coda_dir_operations,
	coda_create,	/* create */
	coda_lookup,	/* lookup */
	coda_link,	/* link */
	coda_unlink,    /* unlink */
	coda_symlink,	/* symlink */
	coda_mkdir,	/* mkdir */
	coda_rmdir,   	/* rmdir */
	NULL,		/* mknod */
	coda_rename,	/* rename */
	NULL,	        /* readlink */
	NULL,	        /* follow_link */
	NULL,	        /* readpage */
	NULL,		/* writepage */
	NULL,		/* bmap */
	NULL,	        /* truncate */
	coda_permission /* permission */
};

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
	coda_follow_link,      	/* follow_link */
	NULL,			/* readpage */
	NULL,			/* writepage */
	NULL,			/* bmap */
	NULL,			/* truncate */
	NULL            	/* permission */
};

struct inode_operations coda_ioctl_inode_operations =
{
	&coda_ioctl_operations,
	NULL,		                /* create */
	NULL,		                /* lookup */
	NULL,		                /* link */
	NULL,		                /* unlink */
	NULL,		                /* symlink */
	NULL,		                /* mkdir */
	NULL,	       	                /* rmdir */
	NULL,		                /* mknod */
	NULL,		                /* rename */
	NULL,	                        /* readlink */
	NULL,	                        /* follow_link */
	NULL,	                        /* readpage */
	NULL,		                /* writepage */
	NULL,		                /* bmap */
	NULL,	                        /* truncate */
	coda_ioctl_permission	        /* permission */
};

static int
coda_ioctl_permission(struct inode *inode, int mask)
{
        ENTRY;

        return 0;
}


	
static int coda_create(struct inode *dir, const char *name, int length, 
		       int mode, struct inode **result)
{
        int error=0;
        struct cnode *dircnp;
	struct ViceFid newfid;
	struct coda_vattr attrs;
ENTRY;
        *result = NULL;
	CDEBUG(D_INODE, "name: %s, length %d, mode %o\n", name, length, mode);

	/* sanity */
        if (!dir || !S_ISDIR(dir->i_mode)) {
                printk("coda_create: inode is null or not a directory\n");
                error = ENOENT;
		goto exit;
        }
	dircnp = ITOC(dir);
        CHECK_CNODE(dircnp);

	if ( length > CODA_MAXNAMLEN ) {
		char str[50];
		printk("name too long: create, %s(%s)\n", 
		       coda_f2s(&dircnp->c_fid, str), name);
		error = ENAMETOOLONG;
		goto exit;
        }

	error = - venus_create(dir->i_sb, &(dircnp->c_fid), name, length, 
				0, mode, &newfid, &attrs);

	    
        if ( error && error != ENOENT ) {
	    CDEBUG(D_INODE, "create: %s in (%lx.%lx.%lx), result %d\n",
		   name, dircnp->c_fid.Volume,
		   dircnp->c_fid.Vnode,
		   dircnp->c_fid.Unique,
		   error); 
	    goto exit;
	}

	error = coda_cnode_make(result, &newfid, dir->i_sb);
	if ( error ) {
	    *result = 0;
	    goto exit;
	}

	/* invalidate the directory cnode's attributes */
	/*                dircnp->c_flags &= ~C_VATTR;
			  cfsnc_zapfid(&(dircnp->c_fid)); */

exit: 
        iput(dir);
        EXIT;
        return -error;
}			     

	
static int coda_lookup(struct inode *dir, const char *name, int length, 
		       struct inode **res_inode)
{
        struct cnode *dircnp, *savedcnp;
	int error = 0;
	int type;
	struct ViceFid resfid;
	
	*res_inode = NULL;
        ENTRY;
        CDEBUG(D_INODE, "name %s, len %d\n", name, length);

	/* sanity */
	if (!dir || !S_ISDIR(dir->i_mode)) {
                printk("coda_lookup: inode is NULL or not a directory.\n");
                error = ENOENT;
		goto exit;
	}
	dircnp = ITOC(dir);
	CHECK_CNODE(dircnp);
	
	CDEBUG(D_INODE, "lookup: %s in %ld.%ld.%ld\n", name, 
	       dircnp->c_fid.Volume,
	       dircnp->c_fid.Vnode, dircnp->c_fid.Unique);

	/* special case. iput(dir) and iget(res_inode) balance out */
	if ( length == 1 && name[0]=='.' ) {
	        *res_inode = dir;
		return 0;
	}

        /* check for operation on dying object */
	if ( IS_DYING(dircnp) ) {
                COMPLAIN_BITTERLY(lookup, dircnp->c_fid);
                savedcnp = dircnp;
		error = ENODEV;
		goto exit;
	}

        /* control object, create inode for it on the fly, release will
           release it.  */
        if ( (dir == dir->i_sb->s_mounted) && 
	     (CODA_CONTROLLEN == length ) && 
	     (strncmp(name, CODA_CONTROL, CODA_CONTROLLEN) == 0 )) {
	    /* warning coda_cnode_makectl returns negative errors */
	        error = -coda_cnode_makectl(res_inode, dir->i_sb);
		CDEBUG(D_SPECIAL, "Lookup on CTL object; iput of ino %ld, count %d\n", dir->i_ino, dir->i_count);
                goto exit;
        }

        /* do we have it already name cache */
	if ( (savedcnp = cfsnc_lookup(dircnp, name, length)) != NULL ) {
		CHECK_CNODE(savedcnp);
		*res_inode = CTOI(savedcnp);
		iget((*res_inode)->i_sb, (*res_inode)->i_ino);
		CDEBUG(D_INODE, "cache hit for ino: %ld, count: %d, name %s!\n",
		       (*res_inode)->i_ino, (*res_inode)->i_count, name);
		goto exit;
	}
	CDEBUG(D_INODE, "name not found in cache (%s)!\n", name);

	/* is this name too long? */
        if ( length > CODA_MAXNAMLEN ) {
	        printk("name too long: lookup, %lx,%lx,%lx(%s)\n", 
                      dircnp->c_fid.Volume, 
                      dircnp->c_fid.Vnode, 
                      dircnp->c_fid.Unique, name);
                *res_inode = NULL;
                error = ENAMETOOLONG;
                goto exit;
        }
		
        /* name not cached */
	error = -venus_lookup(dir->i_sb, &(dircnp->c_fid), 
			     name, length, &type, &resfid);
 

        if ( error) {
                CDEBUG(D_INODE, "venus returns error for %lx.%lx.%lx(%s)%d\n",
                      dircnp->c_fid.Volume, 
                      dircnp->c_fid.Vnode, 
                      dircnp->c_fid.Unique, 
                      name, error);
                *res_inode = (struct inode *) NULL;
		goto exit;
        } 

	CDEBUG(D_INODE, "lookup: vol %lx vno %lx uni %lx type %o result %d\n",
	       resfid.Volume, 
	       resfid.Vnode,
	       resfid.Unique,
	       type,
	       error);

	/* at last we have our inode number from Venus, now allocate storage for
	   the cnode and do iget, and fill in the attributes */
	error = -coda_cnode_make(res_inode, &resfid,  dir->i_sb);
	if ( error ) {
	    *res_inode = NULL;
	    goto exit;
	}

	/* put the thing in the name cache */
	savedcnp = ITOC(*res_inode);
	CHECK_CNODE(savedcnp);
	CDEBUG(D_INODE, "ABOUT to enter into cache.\n");
	cfsnc_enter(dircnp, name, length, savedcnp);
	CDEBUG(D_INODE, "entered in cache\n");
		         
exit:
        iput(dir);
        EXIT;
        return -error;
}



int
coda_unlink(struct inode *dir_inode, const char *name, int length)
{
        struct cnode *dircnp;
        int result = 0;

	ENTRY;
        dircnp = ITOC(dir_inode);
        CHECK_CNODE(dircnp);

        CDEBUG(D_INODE, " %s in %ld.%ld.%ld, ino %ld\n", name , dircnp->c_fid.Volume, 
              dircnp->c_fid.Vnode, dircnp->c_fid.Unique, dir_inode->i_ino);

        if ( IS_DYING(dircnp) ) {
                COMPLAIN_BITTERLY(unlink, dircnp->c_fid);
                result = ENODEV;
		goto exit;
        }

        /* this file should no longer be in the namecache! */
        cfsnc_zapfile(dircnp, (const char *)name, length);

        /* control object */
        
        result = -venus_remove(dir_inode->i_sb, &(dircnp->c_fid), name, length);
        
exit:
        CDEBUG(D_INODE, "returned %d\n", result);
        iput(dir_inode);
        EXIT;
        return result;

}




int 
coda_mkdir(struct inode *dir_inode, const char *name, int length, int mode)
{
        struct cnode *dircnp;
        int error;
	char fidstr[50];
	struct ViceFid newfid;
	struct coda_vattr attr;

        ENTRY;

        dircnp = ITOC(dir_inode);
        CHECK_CNODE(dircnp);

	if (!dir_inode || !S_ISDIR(dir_inode->i_mode)) {
		printk("coda_mkdir: inode is NULL or not a directory\n");
		iput(dir_inode);
		return -ENOENT;
	}

        if ( length > CODA_MAXNAMLEN ) {
                iput(dir_inode);
                return -ENAMETOOLONG;
        }

	CDEBUG(D_INODE, "mkdir %s (len %d) in %s, mode %o.\n", 
	       name, length, coda_f2s(&(dircnp->c_fid), fidstr), mode);

        attr.va_mode = mode;
	error = -venus_mkdir(dir_inode->i_sb, &(dircnp->c_fid), 
			       name, length, &newfid, &attr);

        if ( error ) {
                CDEBUG(D_INODE, "returned error %d\n", error);
                iput(dir_inode);
                return -error;
        }

        CDEBUG(D_INODE, "mkdir: (%ld.%ld.%ld) result %d\n",
	       newfid.Volume,
	       newfid.Vnode,
	       newfid.Unique,
	       error); 

        CDEBUG(D_INODE, "before final iput\n");
        iput(dir_inode);
EXIT;
        CDEBUG(D_INODE, "exiting.\n");
        return -error;
                
}


int 
coda_rmdir(struct inode *dir_inode, const char *name, int length)
{
        struct cnode *dircnp;
        int error;
ENTRY;

	if (!dir_inode || !S_ISDIR(dir_inode->i_mode)) {
		printk("coda_rmdir: inode is NULL or not a directory\n");
		return -ENOENT;
	}

        dircnp = ITOC(dir_inode);
        CHECK_CNODE(dircnp);

        /* this directory name should no longer be in the namecache */
        cfsnc_zapfile(dircnp, (const char *)name, length);

        if ( length > CODA_MAXNAMLEN ) {
                printk("coda_rmdir: name too long.\n");
                iput(dir_inode);
                return -ENAMETOOLONG;
        }

	error = -venus_rmdir(dir_inode->i_sb, &(dircnp->c_fid), name, length);
         
        CDEBUG(D_INODE, " result %d\n", error); 
EXIT;
        iput(dir_inode);
        return error;
                
}


static int 
coda_rename(struct inode *old_inode, const char *old_name, int old_length, 
            struct inode *new_inode, const char *new_name, int new_length,
            int must_be_dir)
{
        struct cnode *new_cnp, *old_cnp;
	int error;
ENTRY;
        old_cnp = ITOC(old_inode);
        CHECK_CNODE(old_cnp);
        new_cnp = ITOC(new_inode);
        CHECK_CNODE(new_cnp);

        CDEBUG(D_INODE, "old: %s, (%d length, %d strlen), new: %s (%d length, %d strlen).\n", old_name, old_length, strlen(old_name), new_name, new_length, strlen(new_name));

        if ( (old_length > CODA_MAXNAMLEN) || new_length > CODA_MAXNAMLEN ) {
                return -ENAMETOOLONG;
        }
        /* the old file should go from the namecache */
        cfsnc_zapfile(old_cnp, (const char *)old_name, old_length);
        cfsnc_zapfile(new_cnp, (const char *)new_name, new_length);

        error = -venus_rename(old_inode->i_sb, &(old_cnp->c_fid), 
			     &(new_cnp->c_fid), old_length, new_length, 
			     (const char *) old_name, (const char *)new_name);

        CDEBUG(D_INODE, " result %d\n", error); 

        iput(old_inode);
        iput(new_inode);
      EXIT;
  return error;
                
}


static int 
coda_link(struct inode *old_inode, struct inode *dir_inode, 
          const char *name, int length)
{
        int error;
        struct cnode *dir_cnp, *old_cnp;

        ENTRY;

        dir_cnp = ITOC(dir_inode);
        CHECK_CNODE(dir_cnp);

        old_cnp = ITOC(old_inode);
        CHECK_CNODE(old_cnp);

	CDEBUG(D_INODE, "old: fid: (%ld.%ld.%ld)\n",  old_cnp->c_fid.Volume, 
               old_cnp->c_fid.Vnode, old_cnp->c_fid.Unique);
	CDEBUG(D_INODE, "directory: fid: (%ld.%ld.%ld)\n", dir_cnp->c_fid.Volume, 
               dir_cnp->c_fid.Vnode, dir_cnp->c_fid.Unique);

        if ( length > CODA_MAXNAMLEN ) {
                printk("coda_link: name too long. \n");
                iput(dir_inode);
                iput(old_inode);
                return -ENAMETOOLONG;
        }

        /* Check for operation on a dying object */
 
        error = -venus_link(dir_inode->i_sb,&(old_cnp->c_fid), 
			   &(dir_cnp->c_fid), name, length);

 
        CDEBUG(D_INODE, "link result %d\n",error);
        iput(dir_inode);
        iput(old_inode);
EXIT;
        return(-error);


}


static int 
coda_symlink(struct inode *dir_inode, const char *name, int namelen,
             const char *symname)
{

        struct cnode *dir_cnp = ITOC(dir_inode);	
        int error=0;
	int symlen;
        
        ENTRY;
    
	error = -ENAMETOOLONG;
	if ( namelen > CODA_MAXNAMLEN ) { 
		iput(dir_inode);
	        return error;
	}
	symlen = strlen(symname);
	if ( symlen > CODA_MAXNAMLEN ) { 
		iput(dir_inode);
	        return error;
	}
        
        CDEBUG(D_INODE, "symname: %s, length: %d\n", symname, symlen);

	error = -venus_symlink(dir_inode->i_sb, &(dir_cnp->c_fid), name, 
			      namelen, symname, symlen);
        
        iput(dir_inode);
        CDEBUG(D_INODE, "in symlink result %d\n",error);
        EXIT;
        return(-error);
}


static int
coda_permission(struct inode *inode, int mask)
{
        struct cnode *cp;
        int error;
        int mode = inode->i_mode;
 
        ENTRY;

        if ( mask == 0 ) {
                EXIT;
                return 0;
        }

        /* we should be able to trust what is in the mode
           although Venus should be told to return the 
           correct modes to the kernel */
        if ( coda_access_cache == 1 ) { 
            if (current->fsuid == inode->i_uid)
                    mode >>= 6;
	    /*** XXX how bad is this? XXXX */
	    else if (in_group_p(inode->i_gid))
		    mode >>= 3;
	    if (((mode & mask & 0007) == mask) )
		    return 0;
	}


        cp = ITOC(inode);
        CHECK_CNODE(cp);

        error = -venus_access(inode->i_sb, &(cp->c_fid), mask);

        CDEBUG(D_INODE, "returning %d\n", -error);
        EXIT;
        return -error; 
}


static int
coda_readlink(struct inode *inode, char *buffer, int length)
{
        int len;
	int error;
        char *buf;
	struct cnode *cp;
        ENTRY;

        cp = ITOC(inode);
        CHECK_CNODE(cp);

        /* the maximum length we receive is len */
        if ( length > CODA_MAXPATHLEN ) 
	        len = CODA_MAXPATHLEN;
	else
	        len = length;
	CODA_ALLOC(buf, char *, len);
	if ( !buf ) {
		error = -ENOMEM;
		goto exit;
	}
        /* get the link's path into kernel memory */
        error = venus_readlink(inode->i_sb, &(cp->c_fid), buf, &len);

	if ( error ) {
		CDEBUG(D_INODE, "error %s\n", buf);
                goto exit;
        }

        memcpy_tofs(buffer, buf, len);
	put_user('\0', buffer + len);
        error = len;

exit:
        if ( buf ) CODA_FREE(buf, len);
	iput(inode);
        EXIT;
        return error;
}

static int 
coda_follow_link(struct inode * dir, struct inode * inode,
			    int flag, int mode, struct inode ** res_inode)
{
        int error;
        char link[CODA_MAXPATHLEN];
        int length;
	struct cnode *cnp;

        *res_inode = NULL;

        ENTRY;
        if (!dir) {
            dir = current->fs->root;
            dir->i_count++;
        }
        if (!inode) {
            iput (dir);
            return -ENOENT;
        }
        if (!S_ISLNK(inode->i_mode)) {
            iput (dir);
            *res_inode = inode;
            return 0;
        }
        if (current->link_count > 10) {
            iput (dir);
            iput (inode);
            return -ELOOP;
        }
	
        cnp = ITOC(inode);
        CHECK_CNODE(cnp);        

        /* do the readlink to get the path */
	length = CODA_MAXPATHLEN;
        error = venus_readlink(inode->i_sb, &(cnp->c_fid), link, &length); 

        CDEBUG(D_INODE, "coda_getlink returned %d\n", error);
        if (error) {
                iput(dir);
		iput(inode);
                CDEBUG(D_INODE, "coda_follow_link: coda_getlink error.\n");
                return error;
        }
	iput(inode);

        current->link_count++;
        error = open_namei (link, flag, mode, res_inode, dir);
        current->link_count--;

        EXIT;
        return error;
}

static
int coda_readpage(struct inode * inode, struct page * page)
{
        struct inode *open_inode;
        struct cnode *cnp;

        ENTRY;
        
        cnp = ITOC(inode);
        CHECK_CNODE(cnp);

        if ( ! cnp->c_ovp ) {
            printk("coda_readpage: no open inode for ino %ld\n", inode->i_ino);
                return -ENXIO;
        }

        open_inode = cnp->c_ovp;

        CDEBUG(D_INODE, "coda ino: %ld, cached ino %ld, page offset: %lx\n", inode->i_ino, open_inode->i_ino, page->offset);

        generic_readpage(open_inode, page);
        EXIT;
        return 0;
}


/* the CONTROL inode is made without asking attributes from Venus */
int coda_cnode_makectl(struct inode **inode, struct super_block *sb)
{
    int error = 0;

    *inode = iget(sb, CTL_INO);
    if ( *inode ) {
	(*inode)->i_op = &coda_ioctl_inode_operations;
	(*inode)->i_mode = 00444;
	error = 0;
    } else { 
	error = -ENOMEM;
    }
    
    return error;
}
