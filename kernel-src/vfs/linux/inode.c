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

#include <cfs.h>
#include <cnode.h>
#include <super.h>
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
static int coda_getlink(struct inode *inode, char **buffer, int *lenght);
static int coda_readpage(struct inode *inode, struct page *page);

/* external routines */
void coda_load_creds(struct CodaCred *cred);
extern int coda_upcall(struct coda_sb_info *, int insize, int *outsize, caddr_t buffer);
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
        int error=0, size, payload_offset;
        char *buf;
        struct inputArgs *in;
        struct outputArgs *out;
        struct cnode *dircnp;
ENTRY;
        *result = NULL;
        
        CDEBUG(D_INODE, "name: %s, length %d, mode %o\n", name, length, mode);

        if (!dir || !S_ISDIR(dir->i_mode)) {
                printk("coda_create: inode is null or not a directory\n");
                iput(dir);
                return -ENOENT;
        }
	
        dircnp = ITOC(dir);
        CHECK_CNODE(dircnp);

        /* cannot contact dead venus */
        if (IS_DYING(dircnp)) {
                COMPLAIN_BITTERLY(create, dircnp->c_fid);
                return -ENODEV; 
        }


        if ( length > CFS_MAXNAMLEN ) {
                printk("name too long: create, %lx,%lx,%lx(%s)\n", 
                      dircnp->c_fid.Volume, 
                      dircnp->c_fid.Vnode, 
                      dircnp->c_fid.Unique, name);
                return -ENAMETOOLONG;
        }

        payload_offset = VC_INSIZE(cfs_create_in);
        CODA_ALLOC(buf, char *, CFS_MAXNAMLEN + payload_offset);
        in = (struct inputArgs *)buf;
        out = (struct outputArgs *)buf;
        INIT_IN(in, CFS_CREATE);  
        in->d.cfs_create.VFid = dircnp->c_fid;
        /*  XXXX      in->d.cfs_create.excl = exclusive; */
        in->d.cfs_create.attr.va_mode = mode;
        in->d.cfs_create.mode = mode;
        coda_load_creds(&(in->cred));
        in->d.cfs_create.name = (char *) payload_offset;
        
        /* Venus must get null terminated string */
        memcpy((char *)in + payload_offset, name, length);
        *( (char *)in + payload_offset + length ) = '\0';

        size = payload_offset + length + 1;

#if 0
CDEBUG(D_INODE, "total size passed to upcall: %d, string: %s, length: %d, offset %d\n", size,  (char *)in + payload_offset, length, payload_offset );
#endif
        error = coda_upcall(coda_sbp(dir->i_sb), size, &size, buf);

	if ( error ) {
	    goto exit; 
	} else 
	    error = out->result;
	    
        if ( error ) {
	    CDEBUG(D_INODE, "create: (%lx.%lx.%lx), result %ld\n",
		   out->d.cfs_create.VFid.Volume,
		   out->d.cfs_create.VFid.Vnode,
		   out->d.cfs_create.VFid.Unique,
		   out->result); 
	    goto exit;
	}

	error = coda_cnode_make(result, &out->d.cfs_create.VFid, dir->i_sb);
	if ( error ) {
	    *result = 0;
	    goto exit;
	}

	/* invalidate the directory cnode's attributes */
	/*                dircnp->c_flags &= ~C_VATTR;
			  cfsnc_zapfid(&(dircnp->c_fid)); */

exit: 
        if (buf) 
                CODA_FREE(buf, (CFS_MAXNAMLEN + VC_INSIZE(cfs_create_in)));
        iput(dir);
        EXIT;
        return -error;
}			     

	
static int coda_lookup(struct inode *dir, const char *name, int length, 
		       struct inode **res_inode)
{
        struct cnode *dircnp, *savedcnp;
     	struct inputArgs *in;
	struct outputArgs *out;
        char *buf= NULL;
	int error = 0;
	int payload_offset, size;
	
	*res_inode = NULL;
CDEBUG(D_INODE, "\n");

        ENTRY;
        CDEBUG(D_INODE, "name %s, len %d\n", name, length);
	if (!dir || !S_ISDIR(dir->i_mode)) {
                printk("coda_lookup: inode is NULL or not a directory.\n");
                iput(dir);
                return -ENOENT;
	}
CDEBUG(D_INODE, "\n");

	dircnp = ITOC(dir);
	CHECK_CNODE(dircnp);
	
	CDEBUG(D_INODE, "lookup: %s in %ld.%ld.%ld\n", name, dircnp->c_fid.Volume,
              dircnp->c_fid.Vnode, dircnp->c_fid.Unique);

        /* check for operation on dying object */
	if ( IS_DYING(dircnp) ) {
                COMPLAIN_BITTERLY(lookup, dircnp->c_fid);
                savedcnp = dircnp;
                /* error = coda_cnode_getnew(&(CTOI(dircnp))); */
                error = 1;
                CDEBUG(D_INODE, "cannot deal with dying inodes yet\n.");
                return -ENODEV;
	}

        /* control object, create inode for it on the fly, release will
           release it.  */
        if ( strcmp(name, CFS_CONTROL) == 0 &&
             ( dir == dir->i_sb->s_mounted) ) {
                struct ViceFid ctl_fid;
                ctl_fid.Volume = CTL_VOL;
                ctl_fid.Vnode = CTL_VNO;
                ctl_fid.Unique = CTL_UNI;
                
		*res_inode = iget(dir->i_sb, CTL_INO);
		if ( *res_inode ) {
		    (*res_inode)->i_op = &coda_ioctl_inode_operations;
		    (*res_inode)->i_mode = 00444;
		    error = 0;
		} else { 
		    error = -ENOENT;
		}
		CDEBUG(D_INODE, "Lookup on CTL object; iput of ino %ld, count %d\n", dir->i_ino, dir->i_count);
		iput(dir);
                EXIT;
                return error;
        }

        /* do we have it already name cache */
	if ( (savedcnp = cfsnc_lookup(dircnp, name, length)) != NULL ) {
		CHECK_CNODE(savedcnp);
		*res_inode = CTOI(savedcnp);
		iget((*res_inode)->i_sb, (*res_inode)->i_ino);
		iput(dir);
		CDEBUG(D_INODE, "cache hit for ino: %ld, count: %d!\n",
		       (*res_inode)->i_ino, (*res_inode)->i_count);
		return 0;
	}
	CDEBUG(D_INODE, "name not found in cache!\n");

	/* is this name too long? */
        if ( length > CFS_MAXNAMLEN ) {
	        printk("name too long: lookup, %lx,%lx,%lx(%s)\n", 
                      dircnp->c_fid.Volume, 
                      dircnp->c_fid.Vnode, 
                      dircnp->c_fid.Unique, name);
                *res_inode = NULL;
                error = ENAMETOOLONG;
                goto exit;
        }
		
        /* name not cached */
        CODA_ALLOC(buf, char *, (VC_INSIZE(cfs_lookup_in) + CFS_MAXNAMLEN +1));
        in = (struct inputArgs *)buf;
        out = (struct outputArgs *)buf;
        INIT_IN(in, CFS_LOOKUP);
        in->d.cfs_lookup.VFid = dircnp->c_fid;
        coda_load_creds(&(in->cred));

        payload_offset = VC_INSIZE(cfs_lookup_in);
        in->d.cfs_lookup.name = (char *) payload_offset;


        /* send Venus a null terminated string */
        memcpy((char *)in + payload_offset, name, length);
        *((char *)in + payload_offset + length) = '\0';

        size = payload_offset + length + 1;
        error = coda_upcall(coda_sbp(dir->i_sb), size, &size, buf);
        if (!error) {
	  error =out->result;
        } else {
	  CDEBUG(D_INODE, "upcall returns error: %d\n", error);
	  *res_inode = (struct inode *) NULL;
	  goto exit;
	}

        if ( error) {
                CDEBUG(D_INODE, "venus returns error for %lx.%lx.%lx(%s)%d\n",
                      dircnp->c_fid.Volume, 
                      dircnp->c_fid.Vnode, 
                      dircnp->c_fid.Unique, 
                      name, error);
                *res_inode = (struct inode *) NULL;
		goto exit;
        } 

	CDEBUG(D_INODE, "lookup: vol %lx vno %lx uni %lx type %o result %ld\n",
	       out->d.cfs_lookup.VFid.Volume, 
	       out->d.cfs_lookup.VFid.Vnode,
	       out->d.cfs_lookup.VFid.Unique,
	       out->d.cfs_lookup.vtype,
	       out->result);

	/* at last we have our inode number from Venus, now allocate storage for
	   the cnode and do iget, and fill in the attributes */
	error = coda_cnode_make(res_inode, &out->d.cfs_lookup.VFid, 
                                             dir->i_sb);
	if ( error ) {
	    *res_inode = NULL;
	    error = -error;
	    goto exit;
	    
	}

	/* put the thing in the name cache */
	savedcnp = ITOC(*res_inode);
	CHECK_CNODE(savedcnp);
	CDEBUG(D_INODE, "ABOUT to enter into cache.\n");
	cfsnc_enter(dircnp, name, length, savedcnp);
	CDEBUG(D_INODE, "entered in cache\n");
		         
exit:
        if (buf) 
                CODA_FREE(buf, (VC_INSIZE(cfs_lookup_in) + CFS_MAXNAMLEN + 1));
        iput(dir);
        EXIT;
        return -error;
}



int
coda_unlink(struct inode *dir_inode, const char *name, int length)
{
        struct cnode *dircnp;
        struct inputArgs *inp;
        struct outputArgs *out;
        int payload_offset;
        int result = 0, s;
        char *buff = NULL;

	ENTRY;
        dircnp = ITOC(dir_inode);
        CHECK_CNODE(dircnp);

        CDEBUG(D_INODE, " %s in %ld.%ld.%ld, ino %ld\n", name , dircnp->c_fid.Volume, 
              dircnp->c_fid.Vnode, dircnp->c_fid.Unique, dir_inode->i_ino);

        if ( IS_DYING(dircnp) ) {
                COMPLAIN_BITTERLY(unlink, dircnp->c_fid);
                iput(dir_inode);
        EXIT;
                return -ENODEV;
        }

        /* this file should no longer be in the namecache! */
        cfsnc_zapfile(dircnp, (const char *)name, length);

        /* control object */
        
        CODA_ALLOC(buff,  char *, CFS_MAXNAMLEN + sizeof(struct inputArgs));
        inp = (struct inputArgs *) buff;
        out = (struct outputArgs *) buff;
        INIT_IN(inp, CFS_REMOVE);
        
        inp->d.cfs_remove.VFid = dircnp->c_fid;
        coda_load_creds(&(inp->cred));

        payload_offset = VC_INSIZE(cfs_remove_in); 
        inp->d.cfs_remove.name = (char *)(payload_offset);

        /* Venus must get null terminated string */
        memcpy((char *)inp + payload_offset, name, length);
        *( (char *)inp + payload_offset + length ) = '\0';

        s = payload_offset + length + 1;

        result = coda_upcall(coda_sbp(dir_inode->i_sb), s, &s, (char *)inp);

        if ( result ) {
            printk("coda_unlink: upcall error: %d\n", result);
            goto exit;
        } else {
            result = -out->result;
            if ( result ) {
                goto exit;
            }
        }
        
exit:
        if (buff) CODA_FREE(buff, CFS_MAXNAMLEN + sizeof(struct inputArgs));
        CDEBUG(D_INODE, "returned %d\n", result);
        iput(dir_inode);
        EXIT;
        return result;

}



void 
coda_load_creds(struct CodaCred *cred)
{
        int i;

        cred->cr_uid = (vuid_t) current->uid;
        cred->cr_euid = (vuid_t) current->euid;
        cred->cr_suid = (vuid_t) current->suid;
        cred->cr_fsuid = (vuid_t) current->fsuid;

        cred->cr_gid = (vgid_t) current->gid;
        cred->cr_egid = (vgid_t) current->egid;
        cred->cr_sgid = (vgid_t) current->sgid;
        cred->cr_fsgid = (vgid_t) current->fsgid;

        for ( i = 0 ; i < NGROUPS ; ++i ) {
                cred->cr_groups[i] = (vgid_t) current->groups[i];
        }

}


int 
coda_mkdir(struct inode *dir_inode, const char *name, int length, int mode)
{
        struct cnode *dircnp;
        struct vattr attrs;
        char * buffer;
        struct inputArgs *inp;
        struct outputArgs *out;
        int error, size, payload_offset;

        ENTRY;

        dircnp = ITOC(dir_inode);
        CDEBUG(D_INODE, "before checking cnode\n");
        CHECK_CNODE(dircnp);
        CDEBUG(D_INODE, "after check cnode\n");

        if ( IS_DYING(dircnp) ) {
                COMPLAIN_BITTERLY(mkdir, dircnp->c_fid);
                iput(dir_inode);
                return -ENODEV;
        }

   CDEBUG(D_INODE, "after is dying\n");
        if ( length > CFS_MAXNAMLEN ) {
                iput(dir_inode);
                return -EPERM;
        }
    CDEBUG(D_INODE, "after is namelen\n");
        payload_offset = VC_INSIZE(cfs_mkdir_in);
        CODA_ALLOC(buffer, char *, CFS_MAXNAMLEN + payload_offset);
    CDEBUG(D_INODE, "after alloc, payload offset %d alloced %d, length %d.\n", payload_offset, payload_offset + CFS_MAXNAMLEN, length);

        inp = (struct inputArgs *) buffer;
        out = (struct outputArgs *) buffer;
        INIT_IN(inp, CFS_MKDIR);
    CDEBUG(D_INODE, "after init in\n");        
        coda_load_creds(&(inp->cred));
    CDEBUG(D_INODE, "after coda creads\n");
        inp->d.cfs_mkdir.VFid = dircnp->c_fid;
        attrs.va_mode = mode;
    CDEBUG(D_INODE, "before assigning attrs\n");
        inp->d.cfs_mkdir.attr = attrs;
    CDEBUG(D_INODE, "after assigning attrs\n");
        inp->d.cfs_mkdir.name = (char *) payload_offset;

        /* Venus must get null terminated string */
        memcpy((char *)inp + payload_offset, name, length);
    CDEBUG(D_INODE, "after memcpy\n");
        *((char *)inp + payload_offset + length) = '\0';
    CDEBUG(D_INODE, "after assigning 0\n");
        size = payload_offset + length + 1;
        
        error = coda_upcall(coda_sbp(dir_inode->i_sb), size, &size, 
                            (char *) inp);
        
        if ( !error ) {
                error = out->result;
        }
        if ( error ) {
                CDEBUG(D_INODE, "returned error %d\n", error);
                if (buffer) CODA_FREE(buffer, CFS_MAXNAMLEN + payload_offset); 
                iput(dir_inode);
                return -error;
        }
         
        CDEBUG(D_INODE, "mkdir: (%ld.%ld.%ld) result %ld\n",
              out->d.cfs_mkdir.VFid.Volume,
              out->d.cfs_mkdir.VFid.Vnode,
              out->d.cfs_mkdir.VFid.Unique,
              out->result); 

        if (buffer) CODA_FREE(buffer, CFS_MAXNAMLEN + 
                              VC_INSIZE(cfs_mkdir_in)); 
        
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
        char * buffer;
        struct inputArgs *inp;
        struct outputArgs *out;
        int error, size;
ENTRY;
        dircnp = ITOC(dir_inode);
        CHECK_CNODE(dircnp);

        if ( IS_DYING(dircnp) ) {
                COMPLAIN_BITTERLY(rmdir, dircnp->c_fid);
                iput(dir_inode);
                return -ENODEV;
        }


        /* this directory name should no longer be in the namecache */
        cfsnc_zapfile(dircnp, (const char *)name, length);

        size = length + 1;
        if ( size > CFS_MAXNAMLEN ) {
                printk("coda_rmdir: name too long.\n");
                iput(dir_inode);
                return -EPERM;
        }

        CODA_ALLOC(buffer, char *, CFS_MAXNAMLEN + 
                   VC_INSIZE(cfs_rmdir_in));
        inp = (struct inputArgs *) buffer;
        out = (struct outputArgs *) buffer;
        INIT_IN(inp, CFS_RMDIR);
        
        coda_load_creds(&(inp->cred));

        inp->d.cfs_rmdir.VFid = dircnp->c_fid;
        inp->d.cfs_rmdir.name = (char *)(VC_INSIZE(cfs_rmdir_in));
        strncpy((char *)inp + (int) inp->d.cfs_rmdir.name, name, size);
        
        size += VC_INSIZE(cfs_rmdir_in);
        
        error = coda_upcall(coda_sbp(dir_inode->i_sb), size, &size, 
                            (char *) inp);
        
        if ( !error ) {
                error = out->result;
        }
        if ( error ) {
                CDEBUG(D_INODE, "returned error %d\n", error);
                if (buffer) CODA_FREE(buffer, CFS_MAXNAMLEN + 
                                      VC_INSIZE(cfs_rmdir_in)); 
                iput(dir_inode);
                return error;
        }
         
        CDEBUG(D_INODE, " result %ld\n", out->result); 
EXIT;
        if (buffer) CODA_FREE(buffer, CFS_MAXNAMLEN + 
                              VC_INSIZE(cfs_rmdir_in)); 
        iput(dir_inode);
        return error;
                
}


static int 
coda_rename(struct inode *old_inode, const char *old_name, int old_length, 
            struct inode *new_inode, const char *new_name, int new_length,
            int must_be_dir)
{
        struct cnode *new_cnp, *old_cnp;
        char * buffer = NULL;
        struct inputArgs *inp;
        struct outputArgs *out;
        int error, size, s, buffer_size;
ENTRY;
        old_cnp = ITOC(old_inode);
        CHECK_CNODE(old_cnp);
        new_cnp = ITOC(new_inode);
        CHECK_CNODE(new_cnp);

        CDEBUG(D_INODE, "old: %s, (%d length, %d strlen), new: %s (%d length, %d strlen).\n", old_name, old_length, strlen(old_name), new_name, new_length, strlen(new_name));

        if ( IS_DYING(old_cnp) || IS_DYING(new_cnp)) {
                COMPLAIN_BITTERLY(rename, old_cnp->c_fid);
                COMPLAIN_BITTERLY(rename, new_cnp->c_fid);
                iput(old_inode);
                iput(new_inode);
                return -ENODEV;
        }

        /* the old file should go from the namecache */
        cfsnc_zapfile(old_cnp, (const char *)old_name, old_length);
        cfsnc_zapfile(new_cnp, (const char *)new_name, new_length);

        buffer_size = 2*CFS_MAXNAMLEN + VC_INSIZE(cfs_rename_in) +8;
        CODA_ALLOC(buffer, char *, buffer_size);
                   
        inp = (struct inputArgs *) buffer;
        out = (struct outputArgs *) buffer;
        INIT_IN(inp, CFS_RENAME);
        
        coda_load_creds(&(inp->cred));

        inp->d.cfs_rename.sourceFid = old_cnp->c_fid;
        inp->d.cfs_rename.destFid = new_cnp->c_fid;

        size = VC_INSIZE(cfs_rename_in);

        inp->d.cfs_rename.srcname = (char *)size;

        s = ( old_length & ~0x3) +4; /* round up to word boundary */
        if ( s > CFS_MAXNAMLEN ) {
                if ( buffer ) CODA_FREE(buffer, buffer_size);
                iput(old_inode);
                iput(new_inode);
                return -ENAMETOOLONG;
        }

        /* Venus must receive an null terminated string */
        memcpy((char *)inp + size, old_name, old_length);
        *((char *)inp + size + old_length) = '\0';
        size += s;
        inp->d.cfs_rename.destname = (char *)size;

        s = ( new_length & ~0x3) +4; /* round up to word boundary */
        if ( s > CFS_MAXNAMLEN ) {
                if ( buffer ) CODA_FREE(buffer, buffer_size);
                iput(old_inode);
                iput(new_inode);
                return -ENAMETOOLONG;
        }
        /* another null terminated string for Venus */
        memcpy((char *)inp + size, new_name, new_length);
        *((char *)inp + size + new_length) = '\0';

        size += s;
        CDEBUG(D_INODE, "destname in packet: %s\n", 
              (char *)inp + (int) inp->d.cfs_rename.destname);
        CDEBUG(D_INODE, "size %d\n", size);
        error = coda_upcall(coda_sbp(old_inode->i_sb), buffer_size, &size, 
                            (char *) inp);
        
        if ( !error ) {
                error = out->result;
        }
        if ( error ) {
                CDEBUG(D_INODE, "returned error %d\n", error);
                if (buffer) CODA_FREE(buffer, buffer_size);
                iput(old_inode);
                iput(new_inode);
                return error;
        }
         
        CDEBUG(D_INODE, " result %ld\n", out->result); 

        if (buffer) CODA_FREE(buffer, buffer_size);
        iput(old_inode);
        iput(new_inode);
      EXIT;
  return error;
                
}


static int 
coda_link(struct inode *old_inode, struct inode *dir_inode, 
          const char *name, int length)
{
        int error, payload_offset, size;
        char *buff = NULL;
        struct inputArgs *inp;
        struct outputArgs *out;
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

        if ( length > CFS_MAXNAMLEN ) {
                printk("coda_link: name too long. \n");
                iput(dir_inode);
                iput(old_inode);
                return -ENAMETOOLONG;
        }

        /* Check for operation on a dying object */
        if (IS_DYING(dir_cnp) || IS_DYING(old_cnp)) {
                COMPLAIN_BITTERLY(link, old_cnp->c_fid);
                COMPLAIN_BITTERLY(link, dir_cnp->c_fid);
                iput(dir_inode);
                iput(old_inode);
                return(-ENODEV);	/* Can't contact dead venus */
        }
    
        /* Check for link to/from control object. */

        CODA_ALLOC(buff, char *, CFS_MAXNAMLEN + sizeof(struct inputArgs));
        inp = (struct inputArgs *) buff;
        out = (struct outputArgs *) buff;

        payload_offset = (VC_INSIZE(cfs_link_in));
        INIT_IN(inp, CFS_LINK);
        coda_load_creds(&(inp->cred));
        inp->d.cfs_link.sourceFid = old_cnp->c_fid;
        inp->d.cfs_link.destFid = dir_cnp->c_fid;
        inp->d.cfs_link.tname = (char *)payload_offset;

        /* make sure strings are null terminated */
        memcpy((char *)inp + payload_offset, name, length);
        *((char *)inp + payload_offset + length) = '\0';
        size = payload_offset + length + 1;
        
        error = coda_upcall(coda_sbp(dir_inode->i_sb), size, &size, 
                            (char *)inp);
        
        if (!error) {
                error = out->result;
        } else {
                printk("coda_link: upcall error: %d\n", error);
        }

        if (error) { 
                printk("coda_unlink: venus returns error: %d\n", error);
        }

        CDEBUG(D_INODE, "link result %d\n",error);
        iput(dir_inode);
        iput(old_inode);
        if (buff) CODA_FREE(buff, CFS_MAXNAMLEN + sizeof(struct inputArgs));
EXIT;
        return(-error);


}


static int 
coda_symlink(struct inode *dir_inode, const char *name, int namelen,
             const char *symname)
{

        char *buff=NULL;
        struct inputArgs *inp;
        struct outputArgs *out;
        struct cnode *dir_cnp = ITOC(dir_inode);	
        int error=0, s, strsize, size, payload_offset;
        
        ENTRY;
    
        /* Check for operation on a dying object */
        if (IS_DYING(dir_cnp)) {
                COMPLAIN_BITTERLY(symlink, dir_cnp->c_fid);
                iput(dir_inode);
                return(-ENODEV);	
        }
        
        /* 
         * allocate space for regular input, 
         * plus 1 path and 1 name, plus padding 
         */        
        /*[sizeof(struct inputArgs) + CFS_MAXPATHLEN + CFS_MAXNAMLEN + 8] ???*/
        CODA_ALLOC(buff, char *, sizeof(struct inputArgs) + CFS_MAXPATHLEN 
                  + CFS_MAXNAMLEN + 8);
        inp = (struct inputArgs *)buff;
        out = (struct outputArgs *)buff;
        INIT_IN(inp, CFS_SYMLINK);
        
        inp->d.cfs_symlink.VFid = dir_cnp->c_fid;
        /*        inp->d.cfs_symlink.attr = *tva; XXXXXX */ 

        coda_load_creds(&(inp->cred));
        payload_offset = VC_INSIZE(cfs_symlink_in);
        inp->d.cfs_symlink.srcname =(char*) payload_offset;
    
        strsize = strlen(symname);
        CDEBUG(D_INODE, "symname: %s, length: %d\n", symname, strsize);
        s = ( strsize  & ~0x3 ) + 4; /* Round up to word boundary. */
        if (s > CFS_MAXPATHLEN) {
                printk("coda_symlink: name too long.\n");
                 error = ENAMETOOLONG;
                goto exit;
        }
        /* don't forget to copy out the null termination */
        memcpy((char *)inp + payload_offset, symname, strsize);
        *((char *)inp + payload_offset + strsize ) = '\0';

        
        payload_offset += s;
        inp->d.cfs_symlink.tname = (char *) payload_offset;
        s = (namelen & ~0x3) + 4;	/* Round up to word boundary. */
        if (s > CFS_MAXNAMLEN) {
                printk("coda_symlink: target name too long.\n");
                error = ENAMETOOLONG;
                goto exit;
        }
        memcpy((char *)inp + payload_offset, name, namelen);
        *((char *)inp + payload_offset + namelen) = '\0';

        size = payload_offset + s;
        error = coda_upcall(coda_sbp(dir_inode->i_sb), size, &size, (char *)inp);
        if (!error) {
                error = out->result; 
        } else {
                printk("coda_symlink: coda_upcall returns error: %d.\n", error);
        }
        
exit:    
        iput(dir_inode);
        if (buff) {
                CODA_FREE(buff, sizeof(struct inputArgs) + CFS_MAXPATHLEN 
                         + CFS_MAXNAMLEN + 8);
        }
        CDEBUG(D_INODE, "in symlink result %d\n",error);
        EXIT;
        return(-error);
}


static int
coda_permission(struct inode *inode, int mask)
{
        struct cnode *cp;
        struct inputArgs *inp =NULL;
        struct outputArgs *outp;
        int size;
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
        	else if (in_group_p(inode->i_gid))
                mode >>= 3;
        	if (((mode & mask & 0007) == mask) )
                return 0;
	}


        cp = ITOC(inode);
        CHECK_CNODE(cp);

        /* Check for operation on a dying object */
        if (IS_DYING(cp)) {
                COMPLAIN_BITTERLY(permission, cp->c_fid);
                return(-ENODEV);	/* Can't contact dead venus */
        }


        CODA_ALLOC(inp, struct inputArgs *, sizeof(struct inputArgs));
        outp = (struct outputArgs *)inp;
        
        INIT_IN(inp, CFS_ACCESS);
        inp->d.cfs_access.VFid = cp->c_fid;
        inp->d.cfs_access.flags = mask << 6;

        CDEBUG(D_INODE, "mask is %o\n", mask);

        size = VC_OUT_NO_DATA;
        coda_load_creds(&(inp->cred));    

        error = coda_upcall(coda_sbp(inode->i_sb), VC_INSIZE(cfs_access_in),
                            &size, (char *)inp);
    
        if (!error) {
                error = outp->result; 
        } else {
                printk("coda_permission: coda upcall returned error %d\n", error);
        }

        CDEBUG(D_INODE, "returning %d\n", -error);
        if (inp) CODA_FREE(inp, sizeof(struct inputArgs));
        EXIT;
        return -error; 
}


static int
coda_readlink(struct inode *inode, char *buffer, int length)
{
        int buflen = 0, result = 0;
        char *namebuf = NULL;

        ENTRY;
        /* get the link's path into kernel memory */
        result = coda_getlink(inode, &namebuf, &buflen);

        if ( result ) {
                printk("coda_readlink: coda_getlink returned error.\n");
                goto exit;
        }

        if ( buflen > length + 1 ) {
                printk("coda_readlink: coda_getlink returned too much\n");
                result = -1;  /* XXX which error code should be returned */
                goto exit;
        }

        memcpy_tofs(buffer, namebuf, buflen);
        CDEBUG(D_INODE, "result %s\n", namebuf);
        result = buflen -1;

exit:
        if ( namebuf ) CODA_FREE(namebuf, buflen);
        EXIT;
        return result;
}

static int
coda_getlink(struct inode *inode, char **buffer, int *length)
{ 
        int error, size, retlen;
        char *result;
        struct inputArgs *in;
        struct outputArgs *out;
        struct cnode *cp; 
        char *buf=NULL; /*[CFS_MAXPATHLEN + VC_INSIZE(cfs_readlink_in)];*/
        
        ENTRY;
    
	if (!S_ISLNK(inode->i_mode)) {
		iput(inode);
		return -EINVAL;
	}

        cp = ITOC(inode);
        CHECK_CNODE(cp);

        /* Check for operation on a dying object */
        if (IS_DYING(cp)) {
                COMPLAIN_BITTERLY(readlink, cp->c_fid);
                iput(inode);
                return -ENODEV;	/* Can't contact dead venus */
        }
    
    
        CODA_ALLOC(buf, char*, CFS_MAXPATHLEN + VC_INSIZE(cfs_readlink_in));
        in = (struct inputArgs *)buf;
        out = (struct outputArgs *)buf;
    
        INIT_IN(in, CFS_READLINK);
        in->d.cfs_readlink.VFid = cp->c_fid;
        coda_load_creds(&(in->cred));    
    
        size = CFS_MAXPATHLEN + VC_OUTSIZE(cfs_readlink_out);
        error =  coda_upcall(coda_sbp(inode->i_sb), VC_INSIZE(cfs_readlink_in),
                             &size, buf);
    
        if (!error) {
                error = out->result; 
        } else {
                printk("coda_readlink: coda upcall returned error %d\n", error);
                goto exit;
        }
    
        if ( error ) {
                printk("coda_readlink: Venus returned error %d\n", error);
                goto exit;
        }
        
        retlen = out->d.cfs_readlink.count;
        CODA_ALLOC(*buffer, char *, retlen+1);
        if ( ! *buffer ) {
                printk("coda_getlink: no memory.\n");
                error = ENOMEM;
                goto exit;
        }

        *length = retlen + 1;

	result =  (char *)out + (int)out->d.cfs_readlink.data;
        memcpy(*buffer, result, retlen);
        *(*buffer + retlen)='\0';

exit:        
        if (buf) CODA_FREE(buf, CFS_MAXPATHLEN + VC_INSIZE(cfs_readlink_in));
        CDEBUG(D_INODE, " result %d\n",error);
        iput(inode);
        EXIT;
        return -error;
}


static int 
coda_follow_link(struct inode * dir, struct inode * inode,
			    int flag, int mode, struct inode ** res_inode)
{
        int error;
        char *link = NULL;
        int length;

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
        if (current->link_count > 5) {
            iput (dir);
            iput (inode);
            return -ELOOP;
        }
        
        /* do the readlink to get the path */
        error = coda_getlink(inode, &link, &length); 
        CDEBUG(D_INODE, "coda_getlink returned %d\n", error);
        if (error) {
                iput(dir);
                printk("coda_follow_link: coda_getlink error.\n");
                goto exit;
        }

        current->link_count++;
        error = open_namei (link, flag, mode, res_inode, dir);
        current->link_count--;

exit:
        /* iput (inode);  */
        if ( link && length > 0 ) CODA_FREE(link, length);
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

