/*
 * ifs iopen, icreate, idec, iinc, istat, iread & iwrite syscall support
 * for Linux.
 *
 * Written by Werner Frielingsdorf.
 *
 * Note:  iput() on a free inode causes an ext2 warning.
 * So some functions (actually almost all) iget and inode, see that its
 * no used (i_nlink=0) and the iput it, causing a ext2 warning.
 *
 */
#include <linux/fs.h>
#include <linux/ifs.h>
#include <linux/mount.h>
#include <linux/kdev_t.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/binfmts.h>
#include <linux/mm.h>
#include <asm/segment.h>
#include <sys/stat.h>
#include <linux/ext2_fs.h>

#define CROSS_MNTPNT 0

#define COPY_ARGS(type, kargs) \
	struct type		kargs;\
	memcpy_fromfs(&(kargs), args, sizeof((kargs)));


extern inode_io_call *kern_inode_io;


/* Sanity check,
   get mounted inode and check ino with parent_vol. */
int parent_check(struct super_block *super, ino_t vol)
{
	struct inode	*inode;


	inode=__iget(super, super->s_mounted->i_ino, CROSS_MNTPNT);
	if (inode == NULL)
		return -EINVAL;  /* couldn't find root inode */

	if (inode->i_ino != vol) {
		iput(inode);
		return -EINVAL;  /* check failed */
	}
	iput(inode);
	return 0;
}

/*
 * Call to return the superblock of a device.
 * returns NULL if not found.
 */
struct super_block *find_super(kdev_t device)
{
	struct super_block *super;

	for (super = super_blocks + 0; super < super_blocks + NR_SUPER ; super++) {
		if (super->s_dev == device) 
			return super;
	}
	return NULL;
}

/*
 * iopen 
 * calls inode_open from fs/exec.c
 * It does most the work.
 */
int iopen(kdev_t device, ino_t inode_number, int flag)
{

	int res;
	struct super_block	*super;
	struct inode		*inode;

	/* Find super block */
	if ((super=find_super(device)) == NULL)
		return -ENODEV;

	/* Get the mount points (root) inode */
	inode=__iget(super, inode_number, CROSS_MNTPNT);

	/* Call open_inode */
	if (inode != NULL) {
		if (inode->i_nlink) 
			res = open_inode(inode, flag);
		else
			res = -EINVAL;
		iput(inode);
		return res;
	}
	else
		return -EINVAL;

	/* device not found */
	return -ENODEV;
}


/*
 * icreate,
 * Creates an inode without linking it to a directory entry.
 * The volume, vnode, dataversion etc flags have not yet been
 * saved within the inode.
 */
int icreate(kdev_t device, ino_t inode_number, struct icreate_args *args)
{
	struct	inode *inode;
	struct	inode *res;
	struct	super_block *super;
	static	char	filename;
	int	err=0;
	COPY_ARGS(icreate_args, kargs);

	/*  Get root inode from super block */
	if ((super=find_super(device)) == NULL) 
		return -ENODEV;

	inode=__iget(super, super->s_mounted->i_ino, CROSS_MNTPNT);


	/* The only way it would seem to create an inode is to create a
	   file from the i_op functions, increment the link count and
	   then delete it.  This leaves a valid inode in the inode table
	   which has no reference.
	   Yep, it is pretty ugly. */

	if (filename++ == 'Z')
		filename = 'A';
	if ( inode && (err=inode->i_op->create(inode, &filename, 1, S_IFREG, &res)) == 0) {
		res->i_nlink++;
#if 0
		res->i_ctime=kargs.volume;
		res->i_size=kargs.vnode;
		res->i_mtime=kargs.unique;
		res->i_atime=kargs.dataversion;
#endif

		/* Need to get the inode again since create dropped it. */
		inode=__iget(super, super->s_mounted->i_ino, CROSS_MNTPNT);

		if (inode->i_op->unlink(inode, &filename, 1) == 0) {
			iput(res);
			return res->i_ino;
		}
		else 
			printk("ifs : unlink failed on inode %lu!\n",(long)res->i_ino);
	}
	else {
		iput(inode);
		printk("ifs : create failed!\n");
		return err;
	}

	return -EINVAL;
}



/*
 * iread,
 * opens the inode similarly to iopen, then fills in a file structure
 * and calls i_ops read.
 */
int iread(kdev_t device, ino_t inode_number, struct io_args *args)
{
	struct super_block	*super;
	struct inode		*inode;
	struct file		file;
	int			res;

	COPY_ARGS(io_args, kargs);
	
	/* Check the memory is valid to write on. */
	res = verify_area(VERIFY_WRITE, kargs.buffer, kargs.count);
	if (res)
		return res;

	/* Do the sanity check. */
	if ((super=find_super(device)) == NULL)
		return -ENODEV;
	if (parent_check(super, kargs.parentvol))
		return -EINVAL;

	/* Get the inode. */
	inode=__iget(super, inode_number, CROSS_MNTPNT);
	if (inode == NULL)
		return -EINVAL;
	if (!inode->i_nlink) {
		iput(inode);
		return -EINVAL;
	}

	/* Fill in the file flags, (may need some more work here. */
	file.f_flags = O_RDONLY;
	file.f_pos = kargs.offset;
	file.f_reada = 0;
	file.f_inode = inode;
	
	/* Call read from i_ops. */
	res=inode->i_op->default_file_ops->read(inode, &file, kargs.buffer, kargs.count);
	iput(inode);
	return res;
	
}











/*
 * iwrite,
 * opens the inode similarly to iopen, then fills in a file structure
 * and calls i_ops write (much the same as iread).
 */
int iwrite(kdev_t device, ino_t inode_number, struct io_args *args)
{
	struct super_block	*super;
	struct inode		*inode;
	struct file		file;
	int			res;
	COPY_ARGS(io_args, kargs);
	
	/* Check the memory can be read from.  */
	res = verify_area(VERIFY_READ, kargs.buffer, kargs.count);
	if (res)
		return res;

	/* Do the sanity check. */
	if ((super=find_super(device)) == NULL)
		return -ENODEV;
	if (parent_check(super, kargs.parentvol))
		return -EINVAL;

	/* Get the inode. */
	inode=__iget(super, inode_number, CROSS_MNTPNT);
	if (inode == NULL)
		return -EINVAL;
	if (!inode->i_nlink) {
		iput(inode);
		return -EINVAL;
	}

	/* Fill in the file fields (also may need some work). */
	file.f_flags = O_WRONLY;
	file.f_pos = kargs.offset;
	file.f_inode = inode;

	/* Call write from i_ops */
	res=inode->i_op->default_file_ops->write(inode, &file, kargs.buffer, kargs.count);
	iput(inode);
	return res;
	
}



/*
 * iinc,
 * increments the inode link count, and marks the inode dirty.
 * Also checks the parent_vol inode is correct. (sanity check)
 */
int iinc(kdev_t device, ino_t inode_number, ino_t parentvol) 
{
	struct super_block	*super;
	struct inode		*inode;
	int			res;

	/* Sanity check,
	   get mounted inode and check ino with parent_vol. */
	if ((super=find_super(device)) == NULL)
		return -ENODEV;

	if (parent_check(super, parentvol))
		return -EINVAL;

	/* Get inode */
	inode=__iget(super, inode_number, CROSS_MNTPNT);
	if (inode==NULL) 
		return -EINVAL;
	if (!inode->i_nlink) {
		iput(inode);
		return -EINVAL;
	}

	/* Increment links and mark as dirty */
	inode->i_nlink++;
	res=inode->i_nlink;
	inode->i_dirt=1;
	iput(inode);
	return 0;
}



/*
 * idec,
 * decrements the inode link count, and marks the inode dirty.
 * links cannot be decremented below 0.
 * Also checks the parent_vol inode is correct. (sanity check)
 */
int idec(kdev_t device, ino_t inode_number, ino_t parentvol) 
{
	struct super_block	*super;
	struct inode		*inode;


	/* Get super inode and check inode number */
	if ((super=find_super(device)) == NULL)
		return -ENODEV;
	if (parent_check(super, parentvol))
		return -EINVAL;

	/* Get inode and decrement links, no further than 0. */
	inode=__iget(super, inode_number, CROSS_MNTPNT);
	if (inode==NULL)
		return -EINVAL;

	if (!inode->i_nlink) {
		iput(inode);	/* Causes ext2 warning. grrr. */
		return -EINVAL;
	}
	else {
		inode->i_nlink--;
		inode->i_dirt=1;	/* Mark dirty. */
	}

	if (!inode->i_nlink) {
		super->s_op->put_inode(inode);	/* Remove inode */
		return -EINVAL;
	}

	iput(inode);	/* Return inode */

	return 0;
}

/*
 * istat,
 * returns statistics of inode.
 */

int istat(kdev_t device, ino_t inode_number, struct stat *args) 
{
	struct super_block	*super;
	struct inode		*inode;
	int			res=0;
	struct stat		kstat;

	/* Verify the memory is writable. */
	res = verify_area(VERIFY_WRITE, (char *)args, sizeof(struct stat));
	if (res)
		return res;

	/* Get the inode */
	if ((super=find_super(device)) == NULL)
		return -ENODEV;
	inode=__iget(super, inode_number, CROSS_MNTPNT);
	if (inode==NULL)
		return -EINVAL;

	/* Fill the information in kernel memory. */
	kstat.st_dev = kdev_t_to_nr(inode->i_dev);
	kstat.st_ino = inode->i_ino;
	kstat.st_mode = inode->i_mode;
	kstat.st_nlink = inode->i_nlink;
	kstat.st_uid = inode->i_uid;
	kstat.st_gid = inode->i_gid;
	kstat.st_rdev = kdev_t_to_nr(inode->i_rdev);
	kstat.st_size = inode->i_size;
	kstat.st_atime = inode->i_atime;
	kstat.st_mtime = inode->i_mtime;
	kstat.st_ctime = inode->i_ctime;

	/* Copy the information to user memory. */
	memcpy_tofs((char *)args,(char *)&kstat,sizeof(kstat));
	iput(inode);
	return 0;
}

int module_inode_io(kdev_t device, ino_t inode_number, int flag, ...)
{
	int	res;
	va_list args;
/* Check user level */
	if (!suser())
		return -EPERM;
		
/* Assuming device is already mounted */

	va_start(args, flag);
	switch (flag) {
	case ICREATE :
		res = icreate(device, inode_number, va_arg(args, struct icreate_args*));
		break;
	case IOPEN :
		res = iopen(device, inode_number, va_arg(args, int));
		break;
	case IREAD :
		res = iread(device, inode_number, va_arg(args, struct io_args*));
		break;
	case IWRITE :
		res = iwrite(device, inode_number, va_arg(args, struct io_args*));
		break;
	case IINC :
		res = iinc(device, inode_number,
			va_arg(args, ino_t));
		break;
	case IDEC :
		res = idec(device, inode_number,
			va_arg(args, ino_t));
		break;
	case ISTAT :
		res = istat(device, inode_number,
			va_arg(args, struct stat *));
		break;
	default :
		 res = -EINVAL;
	}
	va_end(args);
	return res;

}


#ifdef MODULE
int init_module(void)
{
	kern_inode_io=&module_inode_io;
	return 0 ;
}
int cleanup_module(void)
{
	kern_inode_io=NULL;
	return 0;
}
#endif
