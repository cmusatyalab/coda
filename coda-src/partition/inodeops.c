/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#include <vcrcommon.h>
#include <partition.h>

#include <fcntl.h>

#ifndef O_BINARY
#define O_BINARY 0
#endif

Inode 
icreate (Device devno, Inode ino, u_long volume, u_long vnode, 
	 u_long unique, u_long dataversion)
{
    struct DiskPartition *dp;
    Inode rc = 0;
    
    dp = DP_Find(devno);

    if ( dp )
	rc = dp->ops->icreate(dp, ino, volume, vnode, unique, dataversion);
     
    return rc;
}

int 
iopen(Device devno, Inode inode, int flag)
{
    struct DiskPartition *dp;
    int rc = -1;
    
    flag |= O_BINARY;

    dp = DP_Find(devno);

    if ( dp )
	rc = dp->ops->iopen(dp, inode, flag);
     
    return rc;
}

int 
idec(Device devno, Inode inode, Inode parent_vol)
{
    struct DiskPartition *dp;
    int rc = -1;
    
    dp = DP_Find(devno);

    if ( dp )
	rc = dp->ops->idec(dp, inode, parent_vol);
     
    return rc;
}

int 
iinc(Device devno, Inode  inode, Inode parent_vol)
{
    struct DiskPartition *dp;
    int rc = -1;
    
    dp = DP_Find(devno);

    if ( dp )
	rc = dp->ops->iinc(dp, inode, parent_vol);
     
    return rc;
}

int 
iwrite(Device devno, Inode inode, Inode  parent_vol, 
       int  offset, char *buf, int count)
{
    struct DiskPartition *dp;
    int rc = -1;
    
    dp = DP_Find(devno);

    if ( dp )
	rc = dp->ops->iwrite(dp, inode, parent_vol, offset, buf, count);
     
    return rc;
}


int 
iread(Device devno, Inode inode, Inode parent_vol, 
      int offset, char *buf, int count)
{
    struct DiskPartition *dp;
    int rc = -1;
    
    dp = DP_Find(devno);

    if ( dp )
	rc = dp->ops->iread(dp, inode, parent_vol, offset, buf, count);
     
    return rc;
}

int 
get_header(struct DiskPartition *dp, struct i_header *header, Inode ino)
{
    if ( dp )
	return dp->ops->get_header(dp, header, ino);
    else
	return -1;
}

int
put_header(struct DiskPartition *dp, struct i_header *header, Inode ino)
{
    if ( dp ) 
	return dp->ops->put_header(dp, header, ino);
    else
	return -1;
}

	
