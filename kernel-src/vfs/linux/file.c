/*
 * File operations for coda.
 * P. Braam 
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

#include "namecache.h"

#include "linux/coda.h"
#include <coda_linux.h>
#include <psdev.h>
#include "super.h"
#include "upcall.h"
#include "cnode.h"

/* prototypes */
/* dir ops */
int coda_readdir(struct inode *inode, struct file *file, void *dirent,  
                 filldir_t filldir);
int coda_open(struct inode *i, struct file *f);
void coda_release(struct inode *i, struct file *f);
/* file operations */
int coda_file_read(struct inode *i, struct file *f, char *buf, int count);
static int coda_file_write(struct inode *i, struct file *f, const char *buf, int count);
/* pioctl ops */
int coda_ioctl_open(struct inode *i, struct file *f);
void coda_ioctl_release(struct inode *i, struct file *f);
static int coda_pioctl(struct inode * inode, struct file * filp, 
                       unsigned int cmd, unsigned long arg);
static int coda_file_fsync(struct inode *, struct file *);


/* static stuff */
static int coda_venus_readdir(struct inode *inode, struct file *filp, 
                              void *dirent, filldir_t filldir);
static void coda_prepare_openfile(struct inode *coda_inode, struct file *coda_file, 
                           struct inode *open_inode, struct file *open_file);
static void coda_restore_codafile(struct inode *coda_inode, struct file *coda_file, 
                           struct inode *open_inode, struct file *open_file);
static int coda_inode_grab(dev_t dev, ino_t ino, struct inode **ind);
static struct super_block *coda_find_super(kdev_t device);
int venus_fsync(struct super_block *sb, struct ViceFid *fid);

/* external to this file */
void coda_load_creds(struct coda_cred *cred);

extern int coda_debug;
extern int coda_print_entry;

/* exported from this file */
struct file_operations coda_dir_operations = {
        NULL,                   /* lseek */
        NULL,          /* read -- bad  */
        NULL,                   /* write */
        coda_readdir,           /* readdir */
        NULL,                   /* select */
        NULL,                   /* ioctl */
        NULL,                   /* mmap */
        coda_open,              /* open */
        coda_release,           /* release */
        coda_file_fsync,             /* fsync */
	NULL,                   /* fasync */
	NULL,                   /* check_media_change */
        NULL                    /* revalidate */
};

struct file_operations coda_file_operations = {
	NULL,		        /* lseek - default should work for coda */
	coda_file_read,         /* read */
	coda_file_write,        /* write */
	NULL,          		/* readdir */
	NULL,			/* select - default */
	NULL,		        /* ioctl */
	generic_file_mmap,      /* mmap */
	coda_open,              /* open */
	coda_release,           /* release */
	coda_file_fsync,	        /* fsync */
	NULL,                   /* fasync */
	NULL,                   /* check_media_change */
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
	coda_ioctl_release,     /* release */
	NULL,		        /* fsync */
};



/* directory  operations */


/* 
 * this structure is manipulated by filldir in fs.
 * the count holds the remaining amount of space in the getdents buffer,
 * beyond the current_dir pointer.
 */

struct getdents_callback {
	struct linux_dirent * current_dir;
	struct linux_dirent * previous;
	int count;
	int error;
};


static int 
coda_venus_readdir(struct inode *inode, struct file *filp, void *getdent, 
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
        CODA_ALLOC(buff, void *, count);
        if ( ! buff ) { 
                printk("coda_venus_readdir: out of memory.\n");
                return -ENOMEM;
        }

        /* we use this routine to read the file into our buffer */
        result = read_exec(inode, filp->f_pos, buff, count, 1);
        if ( result < 0) {
                printk("coda_venus_readdir: cannot read directory.\n");
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

                        error = verify_area(VERIFY_WRITE, dents_callback->current_dir, count);
                        if (error) {
                                CDEBUG(D_FILE, "verify area fails!!!\n");
                                goto exit;
                        }

                      errfill = filldir(dents_callback,  name, namlen, offs, ino); 
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


void coda_flags_to_cflags(short flags, short *coda_flags)
{
	*coda_flags = 0;
        /*flags & 
		(~(O_RDONLY|O_RDWR|O_WRONLY|O_EXCL|O_TRUNC|O_CREAT));  */
	if ( flags & (O_RDONLY | O_RDWR) ) { 
		CDEBUG(D_FILE, "--> C_READ added\n");
		*coda_flags |= C_O_READ;
	}
	if ( flags & (O_WRONLY | O_RDWR) ) { 
		CDEBUG(D_FILE, "--> C_WRITE added\n");
		*coda_flags |= C_O_WRITE;
	}
	if ( flags & O_TRUNC )  { 
		CDEBUG(D_FILE, "--> C_O_TRUNC added\n");
		*coda_flags |= C_O_TRUNC;
	}
	if ( flags & O_EXCL ) {
		*coda_flags |= C_O_EXCL;
		CDEBUG(D_FILE, "--> C_O_EXCL added\n");
	}
#if 0
	if ( flags & O_CREAT ) {
		*coda_flags |= C_O_CREAT;
		CDEBUG(D_FILE, "--> C_O_CREAT added\n");
	}
#endif

}

/* ask venus to cache the file and return the inode of the container file,
   put this inode pointer in the cnode for future reference */
int
coda_open(struct inode *i, struct file *f)
{
	ino_t ino;
	dev_t dev;
        struct cnode *cnp;
        int error = 0;
        struct inode *cont_inode = NULL;
        unsigned short flags = f->f_flags, coda_flags;

        ENTRY;
        
        CDEBUG(D_FILE, "OPEN inode number: %ld, flags %o.\n", f->f_inode->i_ino, flags);

        if ( flags & O_CREAT ) {
                flags &= ~O_EXCL; /* taken care of by coda_create ?? */
        }

        coda_flags_to_cflags(flags, &coda_flags);

        cnp = ITOC(i);
        CHECK_CNODE(cnp);

	error = -venus_open(i->i_sb, &(cnp->c_fid), coda_flags, &ino, &dev);

        if (error) {
                printk("coda_open: coda_upcall returned %d\n", error);
                goto exit;
        }

        /* coda_upcall returns ino number of cached object, get inode */
        CDEBUG(D_FILE, "cache file dev %d, ino %ld\n", dev, ino);

        if ( ! cnp->c_ovp ) {
                error = coda_inode_grab(dev, ino, &cont_inode);
                
                if ( error ){
                        printk("coda_open: coda_inode_grab error %d.", error);
                        if (cont_inode) iput(cont_inode);
                        goto exit;
                } 
                CDEBUG(D_FILE, "GRAB: coda_inode_grab: ino %ld\n", cont_inode->i_ino);
                cnp->c_ovp = cont_inode; 
        } 

        cnp->c_ocount++;

        /* if opened for writing flush cache entry.  */
        if ( flags & FWRITE ) {
	    cfsnc_zapfid(&(cnp->c_fid));
	    CDEBUG(D_FILE, "increasing owrite for ino %ld\n", i->i_ino);
	    cnp->c_owrite++;

	} 

        cnp->c_device = dev;
        cnp->c_inode  = ino;
        

exit:
        CDEBUG(D_FILE, "result %d, coda i->i_count is %d for ino %ld\n", 
              error, i->i_count, i->i_ino);
        CDEBUG(D_FILE, "cache ino: %ld, count %d\n", cnp->c_ovp->i_ino, cnp->c_ovp->i_count);
        EXIT;
        return -error;

}


void
coda_release(struct inode *i, struct file *f)
{
        struct cnode *cnp;
        int error;
        unsigned short flags = f->f_flags;
	unsigned short coda_flags;
        ENTRY;

        cnp =ITOC(i);
        CHECK_CNODE(cnp);
	coda_flags_to_cflags(flags, &coda_flags);
	
        CDEBUG(D_FILE,  
	       "RELEASE coda (ino %ld, ct %d) cache (ino %ld, ct %d) flags %o\n",
               i->i_ino, i->i_count, (cnp->c_ovp ? cnp->c_ovp->i_ino : 0),
               (cnp->c_ovp ? cnp->c_ovp->i_count : -99), flags);


        /* even when c_ocount=0 we cannot put c_ovp to
          * NULL since the file may be mmapped.
	 * See code in inode.c (coda_put_inode) for
          * further handling of close.
	 */

        --cnp->c_ocount;

        if (flags & FWRITE) {
		CDEBUG(D_FILE, "reducing owrite for ino %ld\n", i->i_ino);
                --cnp->c_owrite;
        }


	error = -venus_release(i->i_sb, &(cnp->c_fid), coda_flags);
	if ( error ) {
		printk("venus_release returns error %d for ino %ld\n",
		       error, i->i_ino);
	}

        CDEBUG(D_FILE, "result: %d\n", error);
        return ;
}



/*  File operations */
int 
coda_file_read(struct inode *coda_inode, struct file *coda_file, 
               char *buff, int count)
{
        struct cnode *cnp;
        struct inode *cont_inode = NULL;
        struct file  open_file;
        int result = 0;

        ENTRY;

        cnp = ITOC(coda_inode);
        CHECK_CNODE(cnp);
        
        if ( IS_DYING(cnp) ) {
             COMPLAIN_BITTERLY(rdwr, cnp->c_fid);
             return -ENODEV;
        }
	
	/* container */
        cont_inode = cnp->c_ovp;
        if ( cont_inode == NULL ) {
                /* I don't understand completely if Linux has the file open 
                   for every possible read operation -- if not there may be no
                   open inode pointer in the cnode. The netbsd code seems
                   to deal with a lot of exceptions for dumping core,
                   loading pages of executables etc. My impression is that
                   linux has a file pointer open in all these cases. So 
                   let's try this for now. Perhaps we need to contact Venus. */
                CDEBUG(D_FILE, "cached inode is 0!\n");
                return 0; /* ??? */
        }

        coda_prepare_openfile(coda_inode, coda_file, cont_inode, &open_file);

        if ( ! open_file.f_op ) { 
                CDEBUG(D_FILE, "cached file has not file operations.\n");
                return 0;
        }

        if ( ! open_file.f_op->read ) {
                CDEBUG(D_FILE, "read not supported by cache file file operations.\n" );
                return 0;
        }
	
        result = open_file.f_op->read(cont_inode, &open_file , buff, count);
        CDEBUG(D_FILE, " result %d, count %d, position: %d\n", result, count, (int)open_file.f_pos);


        coda_restore_codafile(coda_inode, coda_file, cont_inode, &open_file);
        
        return result;
}


static int 
coda_file_write(struct inode *coda_inode, struct file *coda_file, 
               const char *buff, int count)
{
        struct cnode *cnp;
        struct inode *cont_inode = NULL;
        struct file  cont_file;
        int result = 0;

        ENTRY;

        cnp = ITOC(coda_inode);
        CHECK_CNODE(cnp);
        
        if ( IS_DYING(cnp) ) {
             COMPLAIN_BITTERLY(rdwr, cnp->c_fid);
             return -ENODEV;
        }

        cont_inode = cnp->c_ovp;

        if ( cont_inode == NULL ) {
                /* I don't understand completely if Linux has the file open 
                   for every possible read operation -- if not there may be no
                   open inode pointer in the cnode. The netbsd code seems
                   to deal with a lot of exceptions for dumping core,
                   loading pages of executables etc. My impression is that
                   linux has a file pointer open in all these cases. So 
                   let's try this for now. Perhaps we need to contact Venus. */
                printk("coda_file_write: cached inode is 0!\n");
                return -1; 
        }

        coda_prepare_openfile(coda_inode, coda_file, cont_inode, &cont_file);

        if ( ! cont_file.f_op ) { 
                printk("coda_file_write: container file has no file ops.\n");
                return 0;
        }

        if ( ! cont_file.f_op->write ) {
                printk("coda_file_write: write not supported by container.\n" );
                return 0;
        }
         
        /*        cnp->c_flags &= ~C_VATTR; */

	down(&cont_inode->i_sem);
        result = cont_file.f_op->write(cont_inode, &cont_file , buff, count);
	up(&cont_inode->i_sem);
        coda_restore_codafile(coda_inode, coda_file, cont_inode, &cont_file);

        return result;
}

static int 
coda_file_fsync(struct inode *coda_inode, struct file *coda_file)
{
        struct cnode *cnp;
        struct inode *cont_inode = NULL;
        struct file  cont_file;
        int result = 0;

        ENTRY;

        cnp = ITOC(coda_inode);
        CHECK_CNODE(cnp);
        
        if ( IS_DYING(cnp) ) {
             COMPLAIN_BITTERLY(rdwr, cnp->c_fid);
             return -ENODEV;
        }

        cont_inode = cnp->c_ovp;

        if ( cont_inode == NULL ) {
                /* I don't understand completely if Linux has the file open 
                   for every possible read operation -- if not there may be no
                   open inode pointer in the cnode. The netbsd code seems
                   to deal with a lot of exceptions for dumping core,
                   loading pages of executables etc. My impression is that
                   linux has a file pointer open in all these cases. So 
                   let's try this for now. Perhaps we need to contact Venus. */
                printk("coda_file_write: cached inode is 0!\n");
                return -1; 
        }

        coda_prepare_openfile(coda_inode, coda_file, cont_inode, &cont_file);

        if ( ! cont_file.f_op ) { 
                printk("coda_file_write: container file has no file ops.\n");
                return 0;
        }

        if ( ! cont_file.f_op->fsync ) {
                printk("coda_file_fsync: fsync not supported by container.\n" );
                return 0;
        }
         
        /*        cnp->c_flags &= ~C_VATTR; */

	down(&cont_inode->i_sem);
        result = cont_file.f_op->fsync(cont_inode, &cont_file);
	up(&cont_inode->i_sem);
        coda_restore_codafile(coda_inode, coda_file, cont_inode, &cont_file);

	if ( result ) 
		return result;
	
	/* now tell Venus to sync RVM */
	result = venus_fsync(coda_inode->i_sb, &(cnp->c_fid));
	return result;
}
                

void 
coda_prepare_openfile(struct inode *i, struct file *coda_file, 
                      struct inode *cont_inode, struct file *cont_file)
{

        /*        *cont_file = *coda_file;  */
        cont_file->f_pos = coda_file->f_pos;
        cont_file->f_mode = coda_file->f_mode;
        cont_file->f_flags = coda_file->f_flags;
        cont_file->f_count  = coda_file->f_count;
        cont_file->f_owner  = coda_file->f_owner;
        cont_file->f_op = cont_inode->i_op->default_file_ops;
        cont_file->f_inode = cont_inode;

        return ;
}

void
coda_restore_codafile(struct inode *coda_inode, struct file *coda_file, 
                      struct inode *open_inode, struct file *open_file)
{
        coda_file->f_pos = open_file->f_pos;
	coda_inode->i_size = open_inode->i_size;
	/*	coda_inode->i_atime = open_inode->i_atime;
	coda_inode->i_mtime = open_inode->i_mtime; */
        return;
}

int 
coda_readdir(struct inode *inode, struct file *file, 
                  void *dirent,  filldir_t filldir)
{
        int result = 0;
        struct cnode *cnp;
        struct file open_file;

        ENTRY;

        if (!inode || !inode->i_sb || !S_ISDIR(inode->i_mode)) {
                printk("coda_readdir: inode is NULL or not a directory\n");
                return -EBADF;
        }

        cnp = ITOC(inode);
        CHECK_CNODE(cnp);

        if (IS_DYING(cnp)) {
                COMPLAIN_BITTERLY(readdir, cnp->c_fid);
                return -ENODEV;
        }
        
        /* control stuff */

        if ( !cnp->c_ovp ) {
                CDEBUG(D_FILE, "open inode pointer = NULL.\n");
                return -ENODEV;
        }
        
        if ( S_ISREG(cnp->c_ovp->i_mode) ) {
                /* Venus: we must read Venus dirents from the file */
                result = coda_venus_readdir(cnp->c_ovp, file, dirent, filldir);
                return result;
                                                                            
        } else {
                /* potemkin case: we are handed a directory inode */
                coda_prepare_openfile(inode, file, cnp->c_ovp, &open_file);
                result = open_file.f_op->readdir(cnp->c_ovp, 
                                                 &open_file, dirent, filldir);

                coda_restore_codafile(inode, file, cnp->c_ovp, &open_file);
                return result;
        }
        EXIT;
}




/* The pioctl ops*/
int
coda_ioctl_open(struct inode *i, struct file *f)
{


        ENTRY;

        CDEBUG(D_PIOCTL, "File inode number: %ld\n", f->f_inode->i_ino);

	EXIT;
        return 0;

}

void
coda_ioctl_release(struct inode *i, struct file *f) 
{
        return;
}


static int
coda_pioctl(struct inode * inode, struct file * filp, unsigned int cmd,
            unsigned long arg)
{
        int error;
	struct PioctlData iap;
        struct inode *target_inode = NULL;
        struct cnode *cnp;

        ENTRY;
    
        /* get the arguments from user space */
        error = verify_area(VERIFY_READ, (int *) arg, sizeof(iap));
        if ( error ) {
                printk("coda_pioctl: cannot read from user space!.\n");
		goto exit;
        }
        memcpy_fromfs(&iap, (int *) arg, sizeof(iap));
       
        /* 
         *  Look up the pathname. Note that the pathname is in 
         * user memory, and namei takes care of this
         */
	CDEBUG(D_PIOCTL, "pioclt-debugging: calling namei, iap.follow = %d\n",
	       iap.follow);
	/* look up this target inode */
        if ( iap.follow ) {
                error = namei(iap.path, &target_inode); 
	} else {
	        error = lnamei(iap.path, &target_inode); 
	}

	if (error) {
                CDEBUG(D_PIOCTL, "error: lookup returns %d\n",error);
		goto exit;
        }
	
	CDEBUG(D_PIOCTL, "pioclt-debugging: target ino: 0x%ld, dev: 0x%d\n",
	   target_inode->i_ino, target_inode->i_dev);

	/* return if it is not a Coda inode */
	if ( target_inode->i_sb != inode->i_sb ) {
	        error =  -EINVAL;
		goto exit;
	}

	/* now proceed to make the upcall */
        cnp = ITOC(target_inode);
        CHECK_CNODE(cnp);

        CDEBUG(D_PIOCTL, "operating on inode %ld\n", target_inode->i_ino);

	error = -venus_pioctl(inode->i_sb, &(cnp->c_fid), cmd, &iap); 
        
        if (error) {
		printk("coda_pioctl: upcall returns: %d\n", error);
                goto exit;
        }

        
exit:
        if ( target_inode ) 
	        iput(target_inode);
        return(error);
}

/* static stuff */

/* grab the ext2 inode of the cached file;
   XXX an optimization could be made here if somehow we could
       find this superblock without scanning; seems ridiculous
*/
static int 
coda_inode_grab(dev_t dev, ino_t ino, struct inode **ind)
{
        struct super_block *sbptr;

        sbptr = coda_find_super(dev);

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
        return 0;
}


static struct super_block *
coda_find_super(kdev_t device)
{
        struct super_block *super;

        for (super = super_blocks + 0; super < super_blocks + NR_SUPER ; super++
) {
                if (super->s_dev == device) 
                        return super;
        }
        return NULL;
}
