/*
 *  linux/drivers/block/loop.c
 *
 *  Written by Theodore Ts'o, 3/29/93
 * 
 * Copyright 1993 by Theodore Ts'o.  Redistribution of this file is
 * permitted under the GNU Public License.
 *
 * more DES encryption plus IDEA encryption by Nicholas J. Leon, June 20, 1996
 * DES encryption plus some minor changes by Werner Almesberger, 30-MAY-1993
 *
 * Modularized and updated for 1.1.16 kernel - Mitch Dsouza 28th May 1994
 *
 * Adapted for 1.3.59 kernel - Andries Brouwer, 1 Feb 1996
 *
 * Fixed do_loop_request() re-entrancy - <Vincent.Renardias@waw.com> Mar 20, 1997
 */

#include <linux/module.h>
#include <linux/pagemap.h>
#include <asm/pgtable.h>
#include <linux/config.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/major.h>
#include <linux/init.h>

#include <asm/uaccess.h>

#ifdef CONFIG_BLK_DEV_LOOP_DES
# /*nodep*/ include <linux/des.h>
#endif

#ifdef CONFIG_BLK_DEV_LOOP_IDEA
# /*nodep*/ include <linux/idea.h>
#endif

#include <linux/loop.h>		/* must follow des.h */

#define MAJOR_NR LOOP_MAJOR

#define DEVICE_NAME "loop"
#define DEVICE_REQUEST do_lo_request
#define DEVICE_NR(device) (MINOR(device))
#define DEVICE_ON(device)
#define DEVICE_OFF(device)
#define DEVICE_NO_RANDOM
#define TIMEOUT_VALUE (6 * HZ)
#include <linux/blk.h>

#define MAX_LOOP 8
static struct loop_device loop_dev[MAX_LOOP];
static int loop_sizes[MAX_LOOP];
static int loop_blksizes[MAX_LOOP];

/*
 * Transfer functions
 */
static int transfer_none(struct loop_device *lo, int cmd, char *raw_buf,
		  char *loop_buf, int size)
{
	if (cmd == READ)
		memcpy(loop_buf, raw_buf, size);
	else
		memcpy(raw_buf, loop_buf, size);
	return 0;
}

static int transfer_xor(struct loop_device *lo, int cmd, char *raw_buf,
		 char *loop_buf, int size)
{
	char	*in, *out, *key;
	int	i, keysize;

	if (cmd == READ) {
		in = raw_buf;
		out = loop_buf;
	} else {
		in = loop_buf;
		out = raw_buf;
	}
	key = lo->lo_encrypt_key;
	keysize = lo->lo_encrypt_key_size;
	for (i=0; i < size; i++)
		*out++ = *in++ ^ key[(i & 511) % keysize];
	return 0;
}

#ifdef DES_AVAILABLE
static int transfer_des(struct loop_device *lo, int cmd, char *raw_buf,
		  char *loop_buf, int size)
{
	unsigned long tmp[2];
	unsigned long x0,x1,p0,p1;

	if (size & 7)
		return -EINVAL;
	x0 = lo->lo_des_init[0];
	x1 = lo->lo_des_init[1];
	while (size) {
		if (cmd == READ) {
			tmp[0] = (p0 = ((unsigned long *) raw_buf)[0])^x0;
			tmp[1] = (p1 = ((unsigned long *) raw_buf)[1])^x1;
			des_ecb_encrypt((des_cblock *) tmp,(des_cblock *)
			    loop_buf,lo->lo_des_key,DES_ENCRYPT);
			x0 = p0^((unsigned long *) loop_buf)[0];
			x1 = p1^((unsigned long *) loop_buf)[1];
		}
		else {
			p0 = ((unsigned long *) loop_buf)[0];
			p1 = ((unsigned long *) loop_buf)[1];
			des_ecb_encrypt((des_cblock *) loop_buf,(des_cblock *)
			    raw_buf,lo->lo_des_key,DES_DECRYPT);
			((unsigned long *) raw_buf)[0] ^= x0;
			((unsigned long *) raw_buf)[1] ^= x1;
			x0 = p0^((unsigned long *) raw_buf)[0];
			x1 = p1^((unsigned long *) raw_buf)[1];
		}
		size -= 8;
		raw_buf += 8;
		loop_buf += 8;
	}
	return 0;
}
#endif

#ifdef IDEA_AVAILABLE

extern void idea_encrypt_block(idea_key,char *,char *,int);

static int transfer_idea(struct loop_device *lo, int cmd, char *raw_buf,
		  char *loop_buf, int size)
{
  if (cmd==READ) {
    idea_encrypt_block(lo->lo_idea_en_key,raw_buf,loop_buf,size);
  }
  else {
    idea_encrypt_block(lo->lo_idea_de_key,loop_buf,raw_buf,size);
  }
  return 0;
}
#endif

static transfer_proc_t xfer_funcs[MAX_LOOP] = {
	transfer_none,		/* LO_CRYPT_NONE */
	transfer_xor,		/* LO_CRYPT_XOR */
#ifdef DES_AVAILABLE
	transfer_des,		/* LO_CRYPT_DES */
#else
	NULL,			/* LO_CRYPT_DES */
#endif
#ifdef IDEA_AVAILABLE           /* LO_CRYPT_IDEA */
	transfer_idea
#else
	NULL
#endif
};


#define MAX_DISK_SIZE 1024*1024*1024


static void figure_loop_size(struct loop_device *lo)
{
	int	size;

	if (S_ISREG(lo->lo_dentry->d_inode->i_mode))
		size = (lo->lo_dentry->d_inode->i_size - lo->lo_offset) / BLOCK_SIZE;
	else {
		kdev_t lodev = lo->lo_device;
		if (blk_size[MAJOR(lodev)])
			size = blk_size[MAJOR(lodev)][MINOR(lodev)] -
                                lo->lo_offset / BLOCK_SIZE;
		else
			size = MAX_DISK_SIZE;
	}

	loop_sizes[lo->lo_number] = size;
}

static void do_lo_request(void)
{
	int	real_block, block, offset, len, blksize, size;
	char	*dest_addr;
	struct loop_device *lo;
	struct buffer_head *bh;
	struct request *current_request;

repeat:
	INIT_REQUEST;
	current_request=CURRENT;
	CURRENT=current_request->next;
	if (MINOR(current_request->rq_dev) >= MAX_LOOP)
		goto error_out;
	lo = &loop_dev[MINOR(current_request->rq_dev)];
	if (!lo->lo_dentry || !lo->transfer)
		goto error_out;

	blksize = BLOCK_SIZE;
	if (blksize_size[MAJOR(lo->lo_device)]) {
	    blksize = blksize_size[MAJOR(lo->lo_device)][MINOR(lo->lo_device)];
	    if (!blksize)
	      blksize = BLOCK_SIZE;
	}

	dest_addr = current_request->buffer;
	
	if (blksize < 512) {
		block = current_request->sector * (512/blksize);
		offset = 0;
	} else {
		block = current_request->sector / (blksize >> 9);
		offset = (current_request->sector % (blksize >> 9)) << 9;
	}
	block += lo->lo_offset / blksize;
	offset += lo->lo_offset % blksize;
	if (offset > blksize) {
		block++;
		offset -= blksize;
	}
	len = current_request->current_nr_sectors << 9;

	if (current_request->cmd == WRITE) {
		if (lo->lo_flags & LO_FLAGS_READ_ONLY)
			goto error_out;
	} else if (current_request->cmd != READ) {
		printk("unknown loop device command (%d)?!?", current_request->cmd);
		goto error_out;
	}
	spin_unlock_irq(&io_request_lock);
	while (len > 0) {
		real_block = block;
		if (lo->lo_flags & LO_FLAGS_DO_BMAP) {
			real_block = bmap(lo->lo_dentry->d_inode, block);
			if (!real_block) {
				printk("loop: block %d not present\n", block);
				goto error_out_lock;
			}
		}
		bh = getblk(lo->lo_device, real_block, blksize);
		if (!bh) {
			printk("loop: device %s: getblk(-, %d, %d) returned NULL",
			       kdevname(lo->lo_device),
			       block, blksize);
			goto error_out_lock;
		}
		if (!buffer_uptodate(bh) && ((current_request->cmd == READ) ||
					(offset || (len < blksize)))) {
			ll_rw_block(READ, 1, &bh);
			wait_on_buffer(bh);
			if (!buffer_uptodate(bh)) {
				brelse(bh);
				goto error_out_lock;
			}
		}
		size = blksize - offset;
		if (size > len)
			size = len;
			   
		if ((lo->transfer)(lo, current_request->cmd, bh->b_data + offset,
				   dest_addr, size)) {
			printk("loop: transfer error block %d\n", block);
			brelse(bh);
			goto error_out_lock;
		}
		if (current_request->cmd == WRITE) {
			mark_buffer_uptodate(bh, 1);
			mark_buffer_dirty(bh, 1);
		}
		brelse(bh);
		dest_addr += size;
		len -= size;
		offset = 0;
		block++;
	}
	spin_lock_irq(&io_request_lock);
	current_request->next=CURRENT;
	CURRENT=current_request;
	end_request(1);
	goto repeat;
error_out_lock:
	spin_lock_irq(&io_request_lock);
error_out:
	current_request->next=CURRENT;
	CURRENT=current_request;
	end_request(0);
	goto repeat;
}

static int loop_set_fd(struct loop_device *lo, kdev_t dev, unsigned int arg)
{
	struct file	*file;
	struct inode	*inode;
	int error;

	MOD_INC_USE_COUNT;

	error = -EBUSY;
	if (lo->lo_dentry)
		goto out;

	error = -EBADF;
	file = fget(arg);
	if (!file)
		goto out;

	error = -EINVAL;
	inode = file->f_dentry->d_inode;
	if (!inode) {
		printk("loop_set_fd: NULL inode?!?\n");
		goto out_putf;
	}

	if (S_ISBLK(inode->i_mode)) {
		error = blkdev_open(inode, file);
		lo->lo_device = inode->i_rdev;
		lo->lo_flags = 0;
	} else if (S_ISREG(inode->i_mode)) {
		lo->lo_device = inode->i_dev;
		lo->lo_flags = LO_FLAGS_DO_BMAP;
		error = 0;
	}
	if (error)
		goto out_putf;

	if (IS_RDONLY (inode) || is_read_only(lo->lo_device)) {
		lo->lo_flags |= LO_FLAGS_READ_ONLY;
		set_device_ro(dev, 1);
	} else {
		invalidate_inode_pages (inode);
		set_device_ro(dev, 0);
	}

	lo->lo_dentry = file->f_dentry;
	lo->lo_dentry->d_count++;
	lo->transfer = NULL;
	figure_loop_size(lo);

out_putf:
	fput(file);
out:
	if (error)
		MOD_DEC_USE_COUNT;
	return error;
}

static int loop_clr_fd(struct loop_device *lo, kdev_t dev)
{
	struct dentry *dentry = lo->lo_dentry;

	if (!dentry)
		return -ENXIO;
	if (lo->lo_refcnt > 1)	/* we needed one fd for the ioctl */
		return -EBUSY;

	if (S_ISBLK(dentry->d_inode->i_mode))
		blkdev_release (dentry->d_inode);
	lo->lo_dentry = NULL;
	dput(dentry);
	lo->lo_device = 0;
	lo->lo_encrypt_type = 0;
	lo->lo_offset = 0;
	lo->lo_encrypt_key_size = 0;
	memset(lo->lo_encrypt_key, 0, LO_KEY_SIZE);
	memset(lo->lo_name, 0, LO_NAME_SIZE);
	loop_sizes[lo->lo_number] = 0;
	invalidate_buffers(dev);
	MOD_DEC_USE_COUNT;
	return 0;
}

static int loop_set_status(struct loop_device *lo, struct loop_info *arg)
{
	struct loop_info info;
	int err;

	if (!lo->lo_dentry)
		return -ENXIO;
	if (!arg)
		return -EINVAL;
	err = verify_area(VERIFY_READ, arg, sizeof(info));
	if (err)
		return err;
	copy_from_user(&info, arg, sizeof(info));
	if ((unsigned int) info.lo_encrypt_key_size > LO_KEY_SIZE)
		return -EINVAL;
	switch (info.lo_encrypt_type) {
	case LO_CRYPT_NONE:
		break;
	case LO_CRYPT_XOR:
		if (info.lo_encrypt_key_size < 0)
			return -EINVAL;
		break;
#ifdef DES_AVAILABLE
	case LO_CRYPT_DES:
		if (info.lo_encrypt_key_size != 8)
			return -EINVAL;
		des_set_key((des_cblock *) lo->lo_encrypt_key,
		   lo->lo_des_key);
		memcpy(lo->lo_des_init,info.lo_init,8);
		break;
#endif
#ifdef IDEA_AVAILABLE
	case LO_CRYPT_IDEA:
	  {
	        uint16 tmpkey[8];

	        if (info.lo_encrypt_key_size != IDEAKEYSIZE)
		        return -EINVAL;
                /* create key in lo-> from info.lo_encrypt_key */
		memcpy(tmpkey,info.lo_encrypt_key,sizeof(tmpkey));
		en_key_idea(tmpkey,lo->lo_idea_en_key);
		de_key_idea(lo->lo_idea_en_key,lo->lo_idea_de_key);
		break;
	  }
#endif
	default:
		return -EINVAL;
	}
	lo->lo_offset = info.lo_offset;
	strncpy(lo->lo_name, info.lo_name, LO_NAME_SIZE);
	lo->lo_encrypt_type = info.lo_encrypt_type;
	lo->transfer = xfer_funcs[lo->lo_encrypt_type];
	lo->lo_encrypt_key_size = info.lo_encrypt_key_size;
	if (info.lo_encrypt_key_size)
		memcpy(lo->lo_encrypt_key, info.lo_encrypt_key,
		       info.lo_encrypt_key_size);
	figure_loop_size(lo);
	return 0;
}

static int loop_get_status(struct loop_device *lo, struct loop_info *arg)
{
	struct loop_info	info;
	int err;
	
	if (!lo->lo_dentry)
		return -ENXIO;
	if (!arg)
		return -EINVAL;
	err = verify_area(VERIFY_WRITE, arg, sizeof(info));
	if (err)
		return err;
	memset(&info, 0, sizeof(info));
	info.lo_number = lo->lo_number;
	info.lo_device = kdev_t_to_nr(lo->lo_dentry->d_inode->i_dev);
	info.lo_inode = lo->lo_dentry->d_inode->i_ino;
	info.lo_rdevice = kdev_t_to_nr(lo->lo_device);
	info.lo_offset = lo->lo_offset;
	info.lo_flags = lo->lo_flags;
	strncpy(info.lo_name, lo->lo_name, LO_NAME_SIZE);
	info.lo_encrypt_type = lo->lo_encrypt_type;
	if (lo->lo_encrypt_key_size && capable(CAP_SYS_ADMIN)) {
		info.lo_encrypt_key_size = lo->lo_encrypt_key_size;
		memcpy(info.lo_encrypt_key, lo->lo_encrypt_key,
		       lo->lo_encrypt_key_size);
	}
	copy_to_user(arg, &info, sizeof(info));
	return 0;
}

static int lo_ioctl(struct inode * inode, struct file * file,
	unsigned int cmd, unsigned long arg)
{
	struct loop_device *lo;
	int dev;

	if (!inode)
		return -EINVAL;
	if (MAJOR(inode->i_rdev) != MAJOR_NR) {
		printk("lo_ioctl: pseudo-major != %d\n", MAJOR_NR);
		return -ENODEV;
	}
	dev = MINOR(inode->i_rdev);
	if (dev >= MAX_LOOP)
		return -ENODEV;
	lo = &loop_dev[dev];
	switch (cmd) {
	case LOOP_SET_FD:
		return loop_set_fd(lo, inode->i_rdev, arg);
	case LOOP_CLR_FD:
		return loop_clr_fd(lo, inode->i_rdev);
	case LOOP_SET_STATUS:
		return loop_set_status(lo, (struct loop_info *) arg);
	case LOOP_GET_STATUS:
		return loop_get_status(lo, (struct loop_info *) arg);
	case BLKGETSIZE:   /* Return device size */
		if (!lo->lo_dentry)
			return -ENXIO;
		if (!arg)
			return -EINVAL;
		return put_user(loop_sizes[lo->lo_number] << 1, (long *) arg);
	default:
		return -EINVAL;
	}
	return 0;
}

static int lo_open(struct inode *inode, struct file *file)
{
	struct loop_device *lo;
	int	dev;

	if (!inode)
		return -EINVAL;
	if (MAJOR(inode->i_rdev) != MAJOR_NR) {
		printk("lo_open: pseudo-major != %d\n", MAJOR_NR);
		return -ENODEV;
	}
	dev = MINOR(inode->i_rdev);
	if (dev >= MAX_LOOP)
		return -ENODEV;
	lo = &loop_dev[dev];
	lo->lo_refcnt++;
	MOD_INC_USE_COUNT;
	return 0;
}

static int lo_release(struct inode *inode, struct file *file)
{
	struct loop_device *lo;
	int	dev;

	if (!inode)
		return 0;
	if (MAJOR(inode->i_rdev) != MAJOR_NR) {
		printk("lo_release: pseudo-major != %d\n", MAJOR_NR);
		return 0;
	}
	dev = MINOR(inode->i_rdev);
	if (dev >= MAX_LOOP)
		return 0;
	fsync_dev(inode->i_rdev);
	lo = &loop_dev[dev];
	if (lo->lo_refcnt <= 0)
		printk("lo_release: refcount(%d) <= 0\n", lo->lo_refcnt);
	else  {
		lo->lo_refcnt--;
		MOD_DEC_USE_COUNT;
	}
	return 0;
}

/*
 * Generic "readpage" function for block devices that have the normal
 * bmap functionality. This is most of the block device filesystems.
 * Reads the page asynchronously --- the unlock_buffer() and
 * mark_buffer_uptodate() functions propagate buffer state into the
 * page struct once IO has completed.
 */

#define release_page(page) __free_page((page))

static inline void add_to_page_cache(struct page * page,
	struct inode * inode, unsigned long offset,
	struct page **hash)
{
	atomic_inc(&page->count);
	page->flags &= ~((1 << PG_uptodate) | (1 << PG_error));
	page->offset = offset;
	add_page_to_inode_queue(inode, page);
	__add_page_to_hash_queue(page, hash);
}


int generic_block_readpage(struct file * file, struct page * page)
{
	struct dentry *dentry = file->f_dentry;
	struct inode *inode = dentry->d_inode;
	unsigned long block;
	int *p, nr[PAGE_SIZE/512];
	int i;
	kdev_t dev;
	ssize_t blocksize, j, blocksize_bits;

	dev = inode->i_rdev;

	blocksize = BLOCK_SIZE;
	if (blksize_size[MAJOR(dev)] && blksize_size[MAJOR(dev)][MINOR(dev)])
		blocksize = blksize_size[MAJOR(dev)][MINOR(dev)];

	j = blocksize;
	blocksize_bits = 0;
	while(j != 1) {
		blocksize_bits++;
		j >>= 1;
	}

	atomic_inc(&page->count);
	set_bit(PG_locked, &page->flags);
	set_bit(PG_free_after, &page->flags);
	
	i = PAGE_SIZE >> blocksize_bits;
	block = page->offset >> blocksize_bits;
	p = nr;
	do {
		*p = block;
		i--;
		block++;
		p++;
	} while (i > 0);

	/* IO start */
	brw_page(READ, page, dev, nr, blocksize, 0);
	return 0;
}


/* Semantics for shared and private memory areas are different past
 * the end of the file. A shared mapping past the last page of the
 * file is an error and results in a SIGBUS, while a private mapping
 * just maps in a zero page.
 *
 * The goto's are kind of ugly, but this streamlines the normal case
 * of having it in the page cache, and handles the special cases
 * reasonably without having a lot of duplicated code.
 *
 * WSH 06/04/97: fixed a memory leak and moved the allocation of
 * new_page ahead of the wait if we're sure to need it.  
 *
 * A blatant adaptation of filemanp.c to deal with block devices.
 * It should be possible to share most of the code.
 *
*/

static unsigned long blockmap_nopage(struct vm_area_struct * area, unsigned long address, int no_share)
{
	struct file * file = area->vm_file;
	struct dentry * dentry = file->f_dentry;
	struct inode * inode = dentry->d_inode;
	unsigned long offset;
	struct page * page, **hash;
	unsigned long old_page, new_page;
	size_t size;
	kdev_t dev;
	ssize_t blocksize, i, blocksize_bits;

	/* gather device information */
	dev = inode->i_rdev;

	blocksize = BLOCK_SIZE;
	if (blksize_size[MAJOR(dev)] && blksize_size[MAJOR(dev)][MINOR(dev)])
		blocksize = blksize_size[MAJOR(dev)][MINOR(dev)];

	i = blocksize;
	blocksize_bits = 0;
	while(i != 1) {
		blocksize_bits++;
		i >>= 1;
	}

	if (blk_size[MAJOR(dev)])
		size = ((loff_t) blk_size[MAJOR(dev)][MINOR(dev)] 
			<< BLOCK_SIZE_BITS) >> blocksize_bits;
	else
		goto no_page;

	new_page = 0;
	offset = (address & PAGE_MASK) - area->vm_start + area->vm_offset;

	/* We don't support mapping past the size of the device */
	if (offset >= size &&   area->vm_mm == current->mm)
		goto no_page;

	/*
	 * Do we have something in the page cache already?
	 */
	hash = page_hash(inode, offset);
	page = __find_page(inode, offset, *hash);
	if (!page)
		goto no_cached_page;

found_page:
	/*
	 * Ok, found a page in the page cache, now we need to check
	 * that it's up-to-date.  First check whether we'll need an
	 * extra page -- better to overlap the allocation with the I/O.
	 */
	if (no_share && !new_page) {
		new_page = __get_free_page(GFP_KERNEL);
		if (!new_page)
			goto failure;
	}

	if (PageLocked(page))
		goto page_locked_wait;
	if (!PageUptodate(page))
		goto page_read_error;

success:
	/*
	 * Found the page, need to check sharing and possibly
	 * copy it over to another page..
	 */
	old_page = page_address(page);
	if (!no_share) {
		/*
		 * Ok, we can share the cached page directly.. Get rid
		 * of any potential extra pages.
		 */
		if (new_page)
			free_page(new_page);

		flush_page_to_ram(old_page);
		return old_page;
	}

	/*
	 * No sharing ... copy to the new page.
	 */
	copy_page(new_page, old_page);
	flush_page_to_ram(new_page);
     	release_page(page);
	return new_page;

no_cached_page:
	new_page = __get_free_page(GFP_KERNEL);
	if (!new_page)
		goto no_page;

	/*
	 * During getting the above page we might have slept,
	 * so we need to re-check the situation with the page
	 * cache.. The page we just got may be useful if we
	 * can't share, so don't get rid of it here.
	 */
	page = find_page(inode, offset);
	if (page)
		goto found_page;

	/*
	 * Now, create a new page-cache page from the page we got
	 */
	page = mem_map + MAP_NR(new_page);
	new_page = 0;
	add_to_page_cache(page, inode, offset, hash);

	if (generic_block_readpage(file, page) != 0)
		goto failure;

#if 0
	/*
	 * Do a very limited read-ahead if appropriate
	 */
	if (PageLocked(page))
		new_page = try_to_read_ahead(file, offset + PAGE_SIZE, 0);
#endif
	goto found_page;

page_locked_wait:
	__wait_on_page(page);
	if (PageUptodate(page))
		goto success;
	
page_read_error:
	/*
	 * Umm, take care of errors if the page isn't up-to-date.
	 * Try to re-read it _once_. We do this synchronously,
	 * because there really aren't any performance issues here
	 * and we need to check for errors.
	 */
	if (generic_block_readpage(file, page) != 0)
		goto failure;
	wait_on_page(page);
	if (PageError(page))
		goto failure;
	if (PageUptodate(page))
		goto success;

	/*
	 * Things didn't work out. Return zero to tell the
	 * mm layer so, possibly freeing the page cache page first.
	 */
failure:
	release_page(page);
	if (new_page)
		free_page(new_page);
no_page:
	return 0;
}


/*
 * Private mappings just need to be able to load in the map.
 *
 * (This is actually used for shared mappings as well, if we
 * know they can't ever get write permissions..)
 */
static struct vm_operations_struct block_private_mmap = {
	NULL,			/* open */
	NULL,			/* close */
	NULL,			/* unmap */
	NULL,			/* protect */
	NULL,			/* sync */
	NULL,			/* advise */
	blockmap_nopage,	/* nopage */
	NULL,			/* wppage */
	NULL,			/* swapout */
	NULL,			/* swapin */
};



int generic_block_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct vm_operations_struct *ops;
	struct  inode *inode=file->f_dentry->d_inode;
	kdev_t dev;
	ssize_t blocksize;

	dev = inode->i_rdev;
	blocksize = BLOCK_SIZE;
	if (blksize_size[MAJOR(dev)] && blksize_size[MAJOR(dev)][MINOR(dev)])
		blocksize = blksize_size[MAJOR(dev)][MINOR(dev)];

	if ((vma->vm_flags & VM_SHARED) 
	    && (vma->vm_flags & VM_MAYWRITE)) {
		/* we don't support this (yet) */
		return -EINVAL;
	} else {
		ops = &block_private_mmap;
		if (vma->vm_offset & (blocksize - 1))
			return -EINVAL;
	}
	if (!S_ISBLK(inode->i_mode))
		return -EACCES;
	UPDATE_ATIME(inode);
	vma->vm_file = file;
	file->f_count++;
	vma->vm_ops = ops;
	return 0;
}



static struct file_operations lo_fops = {
	NULL,			/* lseek - default */
	block_read,		/* read - general block-dev read */
	block_write,		/* write - general block-dev write */
	NULL,			/* readdir - bad */
	NULL,			/* poll */
	lo_ioctl,		/* ioctl */
	generic_block_mmap,	/* mmap */
	lo_open,		/* open */
	NULL,
	lo_release		/* release */
};


/*
 * And now the modules code and kernel interface.
 */
#ifdef MODULE
#define loop_init init_module
#endif

__initfunc(int
loop_init( void )) {
	int	i;

	if (register_blkdev(MAJOR_NR, "loop", &lo_fops)) {
		printk("Unable to get major number %d for loop device\n",
		       MAJOR_NR);
		return -EIO;
	}
#ifndef MODULE
	printk("loop: registered device at major %d\n", MAJOR_NR);
#ifdef DES_AVAILABLE
	printk("loop: DES encryption available\n");
#endif
#ifdef IDEA_AVAILABLE
	printk("loop: IDEA encryption available\n");
#endif
#endif

	blk_dev[MAJOR_NR].request_fn = DEVICE_REQUEST;
	for (i=0; i < MAX_LOOP; i++) {
		memset(&loop_dev[i], 0, sizeof(struct loop_device));
		loop_dev[i].lo_number = i;
	}
	memset(&loop_sizes, 0, sizeof(loop_sizes));
	memset(&loop_blksizes, 0, sizeof(loop_blksizes));
	blk_size[MAJOR_NR] = loop_sizes;
	blksize_size[MAJOR_NR] = loop_blksizes;

	return 0;
}

#ifdef MODULE
void
cleanup_module( void ) {
  if (unregister_blkdev(MAJOR_NR, "loop") != 0)
    printk("loop: cleanup_module failed\n");
}
#endif
