/* BLURB lgpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#include <voltypes.h>
#include <viceinode.h>
#include <vicetab.h>
#include <partition.h>

/* exported routines 
 * these routines access the partition methods through 
 * the device number of the partition
 * This is done for backward compatibility.
 */

Inode icreate (Device, Inode, u_long, u_long, u_long, u_long);
int iopen   (Device, Inode, int);
int iread   (Device dev, Inode inode_number, Inode parent_vol, 
		    int offset, char *buf, int count);
int iwrite  (Device dev, Inode inode_number,Inode  parent_vol, 
		    int  offset, char *buf, int count);
int iinc (Device dev, Inode  inode_number, Inode parent_vol);
int idec (Device dev, Inode inode_number, Inode parent_vol);
int get_header(struct DiskPartition *dp, struct i_header *header, Inode ino);
int put_header(struct DiskPartition *dp, struct i_header *header, Inode ino);
int ListCodaInodes(char *devname, char *mountedOn, char *resultFile,
		   int (*judgeInode)(struct ViceInodeInfo*, VolumeId), 
		   int judgeParam);
