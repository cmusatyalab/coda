#include <sys/types.h>
#include "ifs.h"
#include <syscall.h>
#include <sys/sysmacros.h>
#include <linux/module.h>



ino_t iopen(dev, inode_number, flags)
dev_t	dev;
ino_t	inode_number;
int	flags;
{
	int flag=flags;
	return syscall(SYS_inode_io, dev, inode_number, IOPEN, &flag);
}

ino_t icreate(dev, near_inode_number, volume, vnode, unique, dataversion)
dev_t	dev;
ino_t	near_inode_number;
u_long	volume;
u_long	vnode;
u_long	unique;
u_long	dataversion;
{
	struct icreate_args args = {volume, vnode, unique, dataversion};
	return syscall(SYS_inode_io, dev, near_inode_number, ICREATE, &args);
}

int iinc(dev, inode_number, parentvol)
dev_t	dev;
ino_t	inode_number;
ino_t	parentvol;
{
	return syscall(SYS_inode_io, dev, inode_number, IINC, parentvol);
}

int idec(dev, inode_number, parentvol)
dev_t	dev;
ino_t	inode_number;
ino_t	parentvol;
{
	return syscall(SYS_inode_io, dev, inode_number, IDEC, parentvol);
}

int istat(dev, inode_number, buf)
dev_t	dev;
ino_t	inode_number;
struct stat	*buf;
{
	return syscall(SYS_inode_io, dev, inode_number, ISTAT, buf);
}

int iread(dev, inode_number, parentvol, offset, buffer, count)
dev_t	dev;
ino_t	inode_number;
ino_t	parentvol;
int	offset;
char	*buffer;
int	count;
{
	struct io_args args={parentvol, offset, buffer, count};
	return syscall(SYS_inode_io, dev, inode_number, IREAD, &args);
}

int iwrite(dev, inode_number, parentvol, offset, buffer, count)
dev_t	dev;
ino_t	inode_number;
ino_t	parentvol;
int	offset;
char	*buffer;
int	count;
{
	struct io_args args={parentvol, offset, buffer, count};
	return syscall(SYS_inode_io, dev, inode_number, IWRITE, &args);
}

