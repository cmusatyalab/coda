/*
 * Super block/filesystem wide operations
 *
 * Peter J. Braam <braam@maths.ox.ac.uk>, 
 * Michael Callahan <callahan@maths.ox.ac.uk> Aug 1996
 *
 */

#define __NO_VERSION__
#include <linux/module.h>

#include <asm/system.h>
#include <asm/segment.h>


#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/locks.h>
#include <linux/string.h>
#include <asm/segment.h>


#include <psdev.h>
#include <super.h>
#include <cfs.h>
#include <cnode.h>



/* VFS super_block ops */
void print_vattr( struct vattr *attr );
static struct super_block *coda_read_super(struct super_block *, void *, int);
static void coda_read_inode(struct inode *);
int coda_fetch_inode(struct inode *, struct vattr *);
static int  coda_notify_change(struct inode *inode, struct iattr *attr);
static void coda_put_inode(struct inode *);
static void coda_put_super(struct super_block *);


/* helper functions */
void coda_load_creds(struct CodaCred *cred);
extern inline struct vcomm *coda_inode_vcomm(struct inode *inode);
extern int coda_upcall(struct coda_sb_info *csbp, int inSize, int *outSize, caddr_t *buffer );
extern struct inode *coda_cinode_make(ViceFid *fid, struct super_block *sb);
extern int coda_cnode_free(struct cnode *cnp);
extern struct cnode *coda_cnode_alloc(void);
extern void coda_cinode_free(struct coda_inode *);
static int coda_get_rootfid(struct super_block *sb, ViceFid *fidp);
static int coda_get_psdev(void *, struct inode **);
static void coda_vattr_to_iattr(struct inode *, struct vattr *);
static void coda_iattr_to_vattr(struct iattr *, struct vattr *);

extern int cfsnc_initialized;
extern int coda_debug;
extern int coda_print_entry;

extern struct inode_operations coda_file_inode_operations;
extern struct inode_operations coda_dir_inode_operations;
extern struct inode_operations coda_ioctl_inode_operations;
extern struct inode_operations coda_symlink_inode_operations;



struct super_operations coda_super_operations =
{
	coda_read_inode,        /* read_inode */
	coda_notify_change,	/* notify_change */
        NULL,                   /* write_inode */
	coda_put_inode,	        /* put_inode */
	coda_put_super,	        /* put_super */
	NULL,			/* write_super */
	NULL,       		/* statfs */
	NULL			/* remount_fs */
};


static int coda_get_psdev(void *data, struct inode **res_dev)
{
        char **psdev_path = data;
        struct inode *psdev = 0;
        int error = 0;

ENTRY;
        error = namei((char *) *psdev_path, &psdev);
        if (error) {
          DEBUG("namei error %d for %d\n", error, (int) psdev_path);
          goto error;
        }
        

        if (!S_ISCHR(psdev->i_mode)) {
          DEBUG("not a character device\n");
          goto error;
        }
        
        if (MAJOR(psdev->i_rdev) != CODA_PSDEV_MAJOR) {
          DEBUG("device %d not a Coda PSDEV device\n", MAJOR(psdev->i_rdev));
          goto error;
        }

        if (MINOR(psdev->i_rdev) >= MAX_CODADEVS) { 
          DEBUG("minor %d not an allocated Coda PSDEV\n", psdev->i_rdev);
          goto error;
        }

        if (psdev->i_count < 2) {
          DEBUG("device not open (i_count = %d)\n", psdev->i_count);
          goto error;
        }
        
        *res_dev = psdev;

        return 0;
      
EXIT;  
error:
        return 1;
}

static void
coda_read_inode(struct inode *inode)
{
        inode->u.generic_ip = NULL;
}

static struct super_block *
coda_read_super(struct super_block *sb, void *data, int silent)
{
        struct inode *psdev = 0, *root = 0; 
        struct coda_sb_info *sb_info = 0;
	struct vcomm *vcp;
        ViceFid fid;
	kdev_t dev = sb->s_dev;
        int error;

ENTRY;

   
#if 0
        MOD_INC_USE_COUNT; /* XXXX */
#endif   
        if (coda_get_psdev (data, &psdev))
          goto error;
  
        sb_info = (struct coda_sb_info *) kmalloc(sizeof(struct coda_sb_info),
                                                  GFP_KERNEL);

        if (!sb_info) {
          DEBUG("out of memory for sb_info\n");
          goto error;
        }
	/* printk("coda_read_super: sb_info at %d.\n", (int) sb_info); */
        lock_super(sb);

        coda_sbp(sb) = sb_info;
        sb_info->s_psdev = psdev;
	vcp = coda_inode_vcomm(psdev);
	sb_info->mi_vcomm = vcp;
        sb->s_blocksize = 1024;	/* smbfs knows what to do? */
        sb->s_blocksize_bits = 10;
        sb->s_magic = CODA_SUPER_MAGIC;
        sb->s_dev = dev;
        sb->s_op = &coda_super_operations;

        /* XXXXXX
        if (!cfsnc_initialized) {
          cfs_vfsopstats_init();
          cfs_vnodeopstats_init();
          cfsnc_init();
        }
        */

        /* Make a root cnode.
           read_inode will recognize that this is the root inode and not
           try to initialize it using GETATTR while this sys_mount system
           call is underway. */

        fid.Volume = 0;
        fid.Vnode = 0;
        fid.Unique = 0;

	/* printk("coda_read_super: root->i_ino: %ld\n", root->i_ino); */
	/* get root fid from Venus: this needs the root inode */
	error = coda_get_rootfid(sb, &fid);
	if ( error ) {
	  printk("coda_read_super: coda_get_rootfid failed with %d\n",
		 error);
	  goto error;
	}	  

         printk("coda_read_super: rootfid is (%ld, %ld, %ld)\n", fid.Volume, fid.Vnode, fid.Unique); 

        root = coda_cinode_make(&fid, sb);
        
        if (!root) {
          DEBUG("iget root inode failed\n");
          unlock_super(sb);
          goto error;
        }

        sb->s_mounted = root;
	unlock_super(sb);

        return sb;
EXIT;  
error:

#if 0
	/*        MOD_DEC_USE_COUNT;*/
#endif
        if (root) {
                iput(root);
                coda_cinode_free(ITOCI(root));
        }
        if (sb_info)
          kfree(sb_info);
        if (psdev)
          iput(psdev);
        sb->s_dev = 0;
        return NULL;
}

int 
coda_fetch_inode (struct inode *inode, struct vattr *attr)
{
        struct cnode *cp;
        int ino, error=0;
ENTRY;
        DEBUG("reading for ino: %ld\n", inode->i_ino);
        ino = inode->i_ino;
        if (!ino)
                panic("coda_fetch_inode: inode called with i_ino = 0\n");

        inode->i_op = NULL;
        inode->i_mode = 0;

        cp = ITOC(inode);
        CHECK_CNODE(cp);
        
        if (cp->c_fid.Volume == 0 &&
            cp->c_fid.Vnode == 0 &&
            cp->c_fid.Unique == 0) {
                /* This is the special root inode created during the sys_mount
                   system call.  We can't do anything here because that call is
                   still executing. */
                inode->i_ino = 1;
                inode->i_op = NULL;
                return 0;
        }
        
        if (IS_CTL_FID( &(cp->c_fid) )) {
                /* This is the special magic control file.  Venus doesn't want
                   to hear a GETATTR about this! */
                inode->i_op = &coda_ioctl_inode_operations;
                return 0;
        }

        if ( ! attr ) {
                printk("coda_fetch_inode: called with NULL vattr, ino %ld\n", inode->i_ino);
                return -1; /* XXXX what to return */
        }

        if (coda_debug) print_vattr(attr);
        coda_vattr_to_iattr(inode, attr);

        if (error) {
                /* Since Venus is involved, there is no guarantee of a
                   successful getattr (i.e., a successful read_inode)
                   unlike what happens with Unix filesystem semantics.
                   It's left to the callers of iget to check for this case
                   by consulting the cp->c_invalid field; the caller
                   should then fail its operation and iput the inode
                   immediately to get rid of it, we hope.  Maybe next time
                   getattr will succeed.
                   XXX: Ask Linus about this race condition.  */
                DEBUG("getattr failed");
        }

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
        EXIT;
        return error;
}

static int coda_get_rootfid(struct super_block *sb, ViceFid *fidp)
{
        struct inputArgs *inp;
	struct outputArgs *outp;
	int error=0;
	int size;
ENTRY;
	/* XXX Is dying?? */

	CODA_ALLOC(inp, struct inputArgs *, sizeof(struct inputArgs));
	outp = (struct outputArgs *) inp;
	INIT_IN(inp, CFS_ROOT)

	size = VC_OUTSIZE(cfs_root_out);
	DEBUG("about to make upcall.\n");

	error = coda_upcall(coda_sbp(sb), VC_IN_NO_DATA, &size, (caddr_t *) inp);

	if ( ! error ) {
	        error = outp->result;
		if ( error ) 
                        printk("coda_get_rootfid: GETROOT result = %d\n", 
                               error);
        }
	
	if (error) {
	        printk("coda_get_rootfid: error %d\n", error);
	}
	
	if ( !error ) {
	        *fidp = (ViceFid) outp->d.cfs_root.VFid;
		DEBUG("VolumeId: %ld, VnodeId: %ld.\n",fidp->Volume, fidp->Vnode);
	}


	if (inp)  CODA_FREE(inp, sizeof(struct inputArgs));
        EXIT;
	return error;
}

int
coda_getvattr(struct ViceFid *fid, struct vattr *attr, struct coda_sb_info *coda_sbp)
{
        struct inputArgs *inp;
        struct outputArgs *outp;
        int size, error;
        struct ViceFid  vfid;
ENTRY;
        vfid = *fid;

        if ( IS_CTL_FID(fid) ) return 0;

        /* XXX: IS_DYING? */
        /* XX in this case rescue _may_ be possible */

        CODA_ALLOC(inp, struct inputArgs *, sizeof(struct inputArgs));
        outp = (struct outputArgs *) inp;

        INIT_IN(inp, CFS_GETATTR);
        coda_load_creds(&(inp->cred));
        inp->d.cfs_getattr.VFid = vfid;
        size = VC_OUTSIZE(cfs_getattr_out);
        error = coda_upcall(coda_sbp, VC_INSIZE(cfs_getattr_in), 
                            &size, (caddr_t *)inp);
     
	if ( error ) {
	        printk("coda_getvattr: upcall returns %d\n", error);
		goto exit;
	} else {
	        error = outp->result;
                if (error) {
		        DEBUG ("result: %ld\n", outp->result); 
		        goto exit;
		}
        }

	*attr = (struct vattr) outp->d.cfs_getattr.attr;

exit:
        if (inp) CODA_FREE(inp, sizeof(struct inputArgs));
        EXIT;
        return -error;
}


        


static void coda_put_super(struct super_block *sb)
{
        struct coda_sb_info *sb_info;

        ENTRY;

        lock_super(sb);

        sb->s_dev = 0;

        /*  XXXXXXX
	cfs_kill(coda_sbp(sb));
        */

	sb_info = coda_sbp(sb);
	iput(sb_info->s_psdev);

        kfree(coda_sbp(sb));

        unlock_super(sb);
EXIT;
/*        MOD_DEC_USE_COUNT;*/
}

static void coda_put_inode(struct inode *inode)
{
        struct coda_inode *cinode;
        struct inode *open_inode;

        ENTRY;
        DEBUG(" inode->ino: %ld\n", inode->i_ino);        

        cinode = ITOCI(inode);
        open_inode = cinode->ci_cnode.c_ovp;

        if ( open_inode ) {
                DEBUG("PUT cached file: ino %ld count %d.\n",  open_inode->i_ino,  open_inode->i_count);
                cinode->ci_cnode.c_ovp = NULL;
                iput( open_inode );
        }
        coda_cinode_free(cinode);

        clear_inode(inode);
EXIT;
}

static int  
coda_notify_change(struct inode *inode, struct iattr *iattr)
{
        struct cnode *cnp;
        struct vattr vattr;
        struct inputArgs *inp;
        struct outputArgs *out;
        int error, size, buffer_size;
ENTRY;
        cnp = ITOC(inode);
        CHECK_CNODE(cnp);

        if ( IS_DYING(cnp) ) {
                COMPLAIN_BITTERLY(notify_change, cnp->c_fid);
                iput(inode);
                return -ENODEV;
        }

        buffer_size = sizeof(struct inputArgs);
        CODA_ALLOC(inp, struct inputArgs *, buffer_size);
        out = (struct outputArgs *) inp;
        INIT_IN(inp, CFS_SETATTR);
        coda_load_creds(&(inp->cred));

        inp->d.cfs_setattr.VFid = cnp->c_fid;
        coda_iattr_to_vattr(iattr, &vattr);
        vattr.va_type = VNON; /* cannot set type */
	DEBUG("vattr.va_mode %o\n", vattr.va_mode);
	inp->d.cfs_setattr.attr = vattr;
        size = VC_INSIZE(cfs_setattr_in);

        


        error = coda_upcall(coda_sbp(inode->i_sb), size, &size, 
                            (caddr_t *) inp);
        
        if ( error ) {
	        printk("coda_notify_change: upcall returns: %d\n", error);
		goto exit;
	} else {
	        error = out->result;
		if ( error ) {
		        DEBUG("venus returned  %d\n", error);
			goto exit;
		}
        }
        
exit: 
        DEBUG(" result %ld\n", out->result); 
	EXIT;
        if ( inp ) CODA_FREE(inp, buffer_size);
        return -error;

}

static void coda_vattr_to_iattr(struct inode *inode, struct vattr *attr)
{
        /* inode's i_dev, i_flags, i_ino are set by iget 
           XXX: is this all we need ??
           */

        
        inode->i_mode &= ~S_IFMT;  /* clear type bits */
        switch (attr->va_type) {
        case VNON:
                inode->i_mode |= 0;
                break;
        case VREG:
                inode->i_mode |= S_IFREG;
                break;
        case VDIR:
                inode->i_mode |= S_IFDIR;
                break;
        case VLNK:
                inode->i_mode |= S_IFLNK;
                break;
        default:
                inode->i_mode |= 0;
        }



        inode->i_mode |= attr->va_mode & ~S_IFMT;
        inode->i_uid = (uid_t) attr->va_uid;
        inode->i_gid = (gid_t) attr->va_gid;
        inode->i_nlink = attr->va_nlink;
        inode->i_size = attr->va_size;
	/*  XXX This needs further study */
	/*
        inode->i_blksize = attr->va_blocksize;
	inode->i_blocks = attr->va_size/attr->va_blocksize 
	  + (attr->va_size % attr->va_blocksize ? 1 : 0); 
	  */
        inode->i_atime = attr->va_atime.tv_sec;
        inode->i_mtime = attr->va_mtime.tv_sec;
        inode->i_ctime = attr->va_ctime.tv_sec;
}

/* 
 * BSD sets attributes that need not be modified to -1. 
 * Linux uses the valid field to indicate what should be
 * looked at.  The BSD type field needs to be deduced from linux 
 * mode.
 * So we have to do some translations here.
 */

void
coda_iattr_to_vattr(struct iattr *iattr, struct vattr *vattr)
{
        umode_t mode;
        unsigned int valid;

        /* clean out */        
        vattr->va_mode = (umode_t) -1;
        vattr->va_uid = (vuid_t) -1; 
        vattr->va_gid = (vgid_t) -1;
        vattr->va_size = (off_t) -1;
	vattr->va_atime.tv_sec = (time_t) -1;
        vattr->va_mtime.tv_sec  = (time_t) -1;
	vattr->va_ctime.tv_sec  = (time_t) -1;
	vattr->va_atime.tv_nsec =  (time_t) -1;
        vattr->va_mtime.tv_nsec = (time_t) -1;
	vattr->va_ctime.tv_nsec = (time_t) -1;
        vattr->va_type = VNON;
	vattr->va_fileid = (long)-1;
	vattr->va_gen = (long)-1;
	vattr->va_bytes = (long)-1;
	vattr->va_fsid = (long)-1;
	vattr->va_nlink = (short)-1;
	vattr->va_blocksize = (long)-1;
	vattr->va_rdev = (dev_t)-1;
        vattr->va_flags = 0;

        /* determine the type */
        mode = iattr->ia_mode;
                if ( S_ISDIR(mode) ) {
                vattr->va_type = VDIR; 
        } else if ( S_ISREG(mode) ) {
                vattr->va_type = VREG;
        } else if ( S_ISLNK(mode) ) {
                vattr->va_type = VLNK;
        } else {
                /* don't do others */
                vattr->va_type = VNON;
        }

        /* set those vattrs that need change */
        valid = iattr->ia_valid;
        if ( valid & ATTR_MODE ) 
                vattr->va_mode = iattr->ia_mode;
        if ( valid & ATTR_UID )
                vattr->va_uid = (vuid_t) iattr->ia_uid;
        if ( valid & ATTR_GID ) 
                vattr->va_gid = (vgid_t) iattr->ia_gid;
        if ( valid & ATTR_SIZE )
                vattr->va_size = iattr->ia_size;
	if ( valid & ATTR_ATIME )
                vattr->va_atime.tv_sec = iattr->ia_atime;
        if ( valid & ATTR_MTIME )
                vattr->va_mtime.tv_sec = iattr->ia_mtime;
        if ( valid & ATTR_CTIME )
                vattr->va_ctime.tv_sec = iattr->ia_ctime;
        
}
  

void
print_vattr( attr )
	struct vattr *attr;
{
    char *typestr;

    switch (attr->va_type) {
    case VNON:
	typestr = "VNON";
	break;
    case VREG:
	typestr = "VREG";
	break;
    case VDIR:
	typestr = "VDIR";
	break;
    case VBLK:
	typestr = "VBLK";
	break;
    case VCHR:
	typestr = "VCHR";
	break;
    case VLNK:
	typestr = "VLNK";
	break;
    case VSOCK:
	typestr = "VSCK";
	break;
    case VFIFO:
	typestr = "VFFO";
	break;
    case VBAD:
	typestr = "VBAD";
	break;
    default:
	typestr = "????";
	break;
    }


    printk("attr: type %s (%o)  mode %o uid %d gid %d fsid %d rdev %d\n",
	      typestr, (int)attr->va_type, (int)attr->va_mode, (int)attr->va_uid, 
	      (int)attr->va_gid, (int)attr->va_fsid, (int)attr->va_rdev);
    
    printk("      fileid %d nlink %d size %d blocksize %d bytes %d\n",
	      (int)attr->va_fileid, (int)attr->va_nlink, 
	      (int)attr->va_size,
	      (int)attr->va_blocksize,(int)attr->va_bytes);
    printk("      gen %ld flags %ld vaflags %d\n",
	      attr->va_gen, attr->va_flags, attr->va_vaflags);
    printk("      atime sec %d nsec %d\n",
	      (int)attr->va_atime.tv_sec, (int)attr->va_atime.tv_nsec);
    printk("      mtime sec %d nsec %d\n",
	      (int)attr->va_mtime.tv_sec, (int)attr->va_mtime.tv_nsec);
    printk("      ctime sec %d nsec %d\n",
	      (int)attr->va_ctime.tv_sec, (int)attr->va_ctime.tv_nsec);
}

/* init_coda: used by filesystems.c to register coda */

struct file_system_type coda_fs_type = {
  coda_read_super, "coda", 0, NULL
};


int init_coda_fs(void)
{
  return register_filesystem(&coda_fs_type);
}



