#ifndef _BLURB_
#define _BLURB_
/*

            Coda: an Experimental Distributed File System
                             Release 4.0

          Copyright (c) 1987-1996 Carnegie Mellon University
                         All Rights Reserved

Permission  to  use, copy, modify and distribute this software and its
documentation is hereby granted,  provided  that  both  the  copyright
notice  and  this  permission  notice  appear  in  all  copies  of the
software, derivative works or  modified  versions,  and  any  portions
thereof, and that both notices appear in supporting documentation, and
that credit is given to Carnegie Mellon University  in  all  documents
and publicity pertaining to direct or indirect use of this code or its
derivatives.

CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
ANY DERIVATIVE WORK.

Carnegie  Mellon  encourages  users  of  this  software  to return any
improvements or extensions that  they  make,  and  to  grant  Carnegie
Mellon the rights to redistribute these changes without encumbrance.
*/

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/coda-src/sys/ifsulib.c,v 1.1.1.1 1996/11/22 19:15:34 rvb Exp";
#endif /*_BLURB_*/

/*
 * userspace IFS library.
 * Designed to simulate the IFS kernel inode_io system call in Linux.
 *
 * Base on an idea of Peter Braam.
 *
 * Written by Werner Frielingsdorf.
 * 
 * 24-10-96.
 *
 * Still planning on optimizing the code a little more.
 *
 * 
 *
 */

/*
 * Inodes are simulated using files.
 * The files go in chains representing links.
 * 123-1, 123-2, 123-3 etc.
 * The vnode, volume, unique and dataversion fields are stored at the
 * beginning of the the file.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/dir.h>
#include <errno.h>

#define	FNAMESIZE	256
#define MAX_NODES	99999
#define	MAX_LINKS	999
#define	FILEDATA	36
#define DATAOFFSET	(sizeof(long)*4)


struct i_header {
	long	volume;
	long	vnode;
	long	unique;
	long	dataversion;
};

/*
 * Hard coded for the time being.
 * -- Should be dynamic and determined by the device.
 */
char	mountpnt[MAXPATHLEN]="/mnt/vice";

#define		INOTOSTR(inode_number, lnk, filename) \
		sprintf(filename,"%s/%lu-%d",mountpnt,inode_number,lnk);



/*
 * iopen, simply returns an open call on the first file of the inode chain
 * and lseek to the data offset.
 *
 */
int iopen(dev, inode_number, flag)
dev_t	dev;
ino_t	inode_number;
int	flag;

{
	char	filename[FNAMESIZE];
	int	fd;

	/* Attempt open. */

	INOTOSTR(inode_number, 1, filename);
	fd=open(filename, O_CREAT | flag, 0600);
	if (fd<0)
		return -1;	

	/* Do lseek to offset of data */
	if (lseek(fd, DATAOFFSET, 0)<0) {
		close (fd);
		return -1;
	}
	return fd;
}



/*
 * icreate.  Simulates the kernel ifs call icreate.
 * Creates a inode chain by creating the first file "inodeno-1".
 *
 * Could do with some caching.
 */

int icreate(dev, inode_number, volume, vnode, unique, dataversion)
dev_t	dev;
ino_t	inode_number;
u_long	volume;
u_long	vnode;
u_long	unique;
u_long	dataversion;

{
	int	fd;
	ino_t	i;
	char	filename[FNAMESIZE];
	struct
	i_header header={volume, vnode, unique, dataversion};

	/*  Find an available inode.  Cache this.  */
	for (i=0;i<=MAX_NODES;i++) {
		INOTOSTR(i,1, filename);
		if ((fd=creat(filename, 0700)) < 0) {
			if (errno!=EEXIST)
				return -1; 
		}
		else
			break;
	}

	/* write header */

	if (write(fd, (char *)&header, sizeof(struct i_header))<0) {
		close (fd);
		return -1;
	}

	close(fd);

	return i;
}




/*
 * iinc, increments the links by adding another file to the inode chain.
 */
int iinc(dev, inode_number, parent_vol)
dev_t	dev;
ino_t	inode_number;
ino_t	parent_vol;

{
	char	inofile[FNAMESIZE];
	char	linkfile[FNAMESIZE];
	struct
	stat	statinf;

	INOTOSTR(inode_number, 1, inofile);
	/* Get number of files in chain from stat info */
	if (stat(inofile, &statinf) < 0)
		return -1;

	/* Link one more file to first inode chain */
	INOTOSTR(inode_number, statinf.st_nlink+1, linkfile);
	if (link(inofile, linkfile)<0)
		return -1;
}


/*
 * idec
 * removes the last file in the inode chain.
 */
int idec(dev, inode_number, parent_vol)
dev_t	dev;
ino_t	inode_number;
ino_t	parent_vol;

{
	char	inofile[FNAMESIZE];
	char	linkfile[FNAMESIZE];
	struct
	stat	statinf;

	INOTOSTR(inode_number, 1, inofile);

	if (stat(inofile, &statinf) < 0)
		return -1;

	/* Remove last file of inode chain. */
	INOTOSTR(inode_number, statinf.st_nlink, linkfile);
	if (unlink(linkfile)<0)
		return -1;
}

/*
 * iread,
 * opens the first file in the inode chain and performs a lseek/read/close.
 *
 */
int iread(dev, inode_number, parent_vol, offset, buf, count)
dev_t	dev;
ino_t	inode_number;
ino_t	parent_vol;
int	offset;
char	*buf;
int	count;

{
	int	fd,res;
	char	inofile[FNAMESIZE];

	if (offset < 0) {
		errno= EINVAL;
		return -1;
	}
	INOTOSTR(inode_number, 1, inofile);

	if ((fd=open(inofile, O_RDONLY , 0))<0)
		return fd;
	
	/*  lseek and read the requested data  */
	if (lseek(fd, DATAOFFSET+offset, 0)<0)
		return -1;

	res=read(fd, buf, count);
	close(fd);
	return res;
}

/*
 * iwrite,
 * Similar to iread, opens the first file in the inode chain,
 * lseeks to offset, reads data and then closes the fd.
 *
 */
int iwrite(dev, inode_number, parent_vol, offset, buf, count)
dev_t	dev;
ino_t	inode_number;
ino_t	parent_vol;
int	offset;
char	*buf;
int	count;

{
	int	fd,res;
	char	inofile[FNAMESIZE];

	INOTOSTR(inode_number, 1, inofile);

	if (offset < 0) {
		errno= EINVAL;
		return -1;
	}

	if ((fd=open(inofile, O_WRONLY, 0))<0)
		return fd;
	
	/*  lseek and read the requested data  */
	if (lseek(fd, DATAOFFSET+offset, 0)<0)
		return -1;

	res=write(fd, buf, count);
	close(fd);
	return res;
}

/*
 * istat,
 * retrieves inode information from the first file in the inode chain.
 * and stores the attributes from the file header to the fields of the
 * stat struct.
 *
 */
int istat(dev, inode_number, statbuf)
dev_t	dev;
ino_t	inode_number;
struct	stat	*statbuf;

{
	int	fd,res;
	char	inofile[FNAMESIZE];
	struct
	i_header header;

	INOTOSTR(inode_number, 1, inofile);

	if (stat(inofile, statbuf)<0)
		return -1;

	if ((fd=open(inofile,O_RDONLY))<0)
		return -1;

	if (read(fd, (char *)&header, DATAOFFSET)!=DATAOFFSET) {
		close (fd);
		return -1;
	}
	close(fd);
	
	(*(long *)&statbuf->st_gid)=header.volume;
	statbuf->st_size=header.vnode;
	statbuf->st_blksize=header.unique;
	statbuf->st_blocks=header.dataversion;
	return 0;
}

