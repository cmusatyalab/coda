
#include <linux/fs.h>
#include <linux/ifs.h>
#include <linux/mount.h>
#include <linux/kdev_t.h>
#include <linux/module.h>
#include <linux/kernel.h>

inode_io_call *kern_inode_io=NULL;

int sys_inode_io(kdev_t device, ino_t inode_number, int flag, void *args)
{
#ifdef COMPILE_AS_LIBRARY

	return module_inode_io(device, inode_number, flag, args);

#else
	if (kern_inode_io == NULL)
		return -ENOSYS;
	else
		return kern_inode_io(device, inode_number, flag, args);
#endif
}

