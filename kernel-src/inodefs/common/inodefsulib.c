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

static char *rcsid = "$Header: /afs/cs.cmu.edu/project/coda-braam/ss/kernel-src/inodefs/common/RCS/inodefsulib.c,v 4.2 1997/04/30 20:02:23 braam Exp braam $";
#endif /*_BLURB_*/

/*
 * userspace IFS library.
 * Designed to simulate the inode related system calls
 *
 * Based on an idea of Peter Braam.
 *
 * Written by Werner Frielingsdorf. 24-10-96.
 * Rewritten with enhancements  Peter Braam
 *
 */

/*
 * Inodes are simulated using files.
 * The name of the file is the "inode number" for the file
 * 1 2 3 4 5 6 7 etc.
 * The vnode, volume, unique and dataversion fields are stored in the
 * resource forks:
 * .1 .2 .3 .4 etc.
 */
#if defined(__linux__) || defined(__BSD44__)
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/dir.h>
#include <errno.h>

#include "uifs.h"


/* static data */
static ino_t uifs_nextino = 0;

static char	*mountpnt="/vicepa";
static mode_t   mode=S_IREAD | S_IWRITE;


void inotostr(ino_t ino, char *filename)
{
    sprintf(filename,"%s/%lu",mountpnt,ino);
}


void inotores(ino_t ino, char *filename)
{
        sprintf(filename,"%s/.%lu",mountpnt,ino);
}



/*
 * iopen, simply returns an open call on the first file of the inode chain
 * and lseek to the data offset.
 */

int iopen(dev_t dev, ino_t inode_number,int flag)
{
	char	filename[FNAMESIZE];
	int	fd;

	/* Attempt open. */

    if ( flag & O_CREAT ) {
        printf("Cannot create files with iopen. Use icreate.\n");
        return -1;
    }

	inotostr(inode_number, filename);
	fd=open(filename, flag, 0600);
	if (fd<0)	return -1;	

	return fd;
}



/*
 * icreate.  Simulates the kernel ifs call icreate.
 * returns -1 in case of failure
 */

int icreate(dev_t dev, ino_t inode_number, u_long volume, u_long vnode, 
            u_long unique, u_long dataversion)
{
	int	fd, rc;
	ino_t	i;
	char	filename[FNAMESIZE];
	char	resfilename[FNAMESIZE];
	struct
	i_header header={1, volume, vnode, unique, dataversion, VICEMAGIC};

	/*  Find an available inode. */
    fd = 0;
    while ( fd == 0 ) { 
        if ( uifs_nextino == 0 ) {
            /* XXX critical */
            uifs_nextino = maxino() + 1;
            i= uifs_nextino;
        } else {
            uifs_nextino++;
            i= uifs_nextino;
        }

        inotostr(i, filename);
        if ((fd = open(filename, O_CREAT | O_EXCL, mode)) < 0) {
            if ( errno == EEXIST ) {
                uifs_nextino = 0; 
                fd = 0; 
                continue;
            } else {
                return -1;
            }
        } else {
            close(fd);
        }
    }

	/* write header */
	inotores(i,resfilename);
	if ( (fd = open(resfilename, O_CREAT | O_EXCL | O_RDWR, mode) ) < 0 ){
        printf("Error opening resource for inode file %d\n",i);
        unlink(filename);
        return -1;
	} else { 
        rc = write(fd, (char *)&header, sizeof(struct i_header)); 
        if ( rc != sizeof(struct i_header)) {
            close (fd);
            printf("Error writing header for inode file %d\n",i);
            return -1;
        }
	}
	close(fd);

	return i;
}

/* write header */
static int put_header(struct i_header *header, ino_t ino)
{
	char	resfilename[FNAMESIZE];
    int fd, rc;

    inotores(ino, resfilename);
	if ( (fd = open(resfilename, O_RDWR)) < 0 ){
        printf("Error opening resource for inode file %d\n",ino);
        return -1;
	} else { 
        rc = write(fd, (char *)header, sizeof(struct i_header)); 
        if ( rc != sizeof(struct i_header)) {
            close (fd);
            printf("Error writing header for inode file %d\n",ino);
            return -1;
        }
	}

	close(fd);
    return 0;
}

/* read header */
int get_header(struct i_header *header, ino_t ino)
{
	char	resfilename[FNAMESIZE];
    int fd, rc;

    inotores(ino, resfilename);
	if ( (fd = open(resfilename, O_RDONLY) ) < 0 ){
        printf("Error opening resource for inode file %d\n",ino);
        return -1;
	} else { 
        rc = read(fd, (char *)header, sizeof(struct i_header)); 
        if ( rc != sizeof(struct i_header)) {
            close (fd);
            printf("Error reading header for inode file %d\n",ino);
            return -1;
        }
	}

	close(fd);
    return 0;
}


/* set the link count in the header */
static int set_link(long *count, ino_t ino)
{
	char	resfilename[FNAMESIZE];
    int fd, rc;

    if ( count <= 0) {
        printf("Cannot set link number to non-positive integer\n");
        return -1;
    }

    inotores(ino, resfilename);
	if ( (fd = open(resfilename, O_RDWR)) < 0 ){
        printf("Error opening resource for inode file %d\n",ino);
        return -1;
	} else { 
        rc = write(fd, (char *)count, sizeof(long)); 
        if ( rc != sizeof(long)) {
            close (fd);
            printf("Error reading header for inode file %d\n",ino);
            return -1;
        }
	}

	close(fd);
    return 0;
}


/*
 * iinc, increments the links by adding another file to the inode chain.
 */
int iinc(dev_t dev, ino_t  inode_number, ino_t parent_vol)
{
    int  rc;
	char	inofile[FNAMESIZE];
	char	linkfile[FNAMESIZE];
	struct	stat	statinf;
    struct  i_header header;
    long lnk;

    inotostr(inode_number,  inofile);
	/* Get number of files in chain from stat info */
	if (stat(inofile, &statinf) < 0)
		return -1;

    rc = get_header(&header, inode_number);
    if  ( rc != 0 ) return -1;
    
    lnk = header.lnk + 1;
    rc = set_link(&lnk, inode_number);
    if  ( rc != 0 ) return -1;

    return 0;
}


/*
 * idec
 * decreases the link attribute in the resource, or removes file
 * when this is about to become negative
 */
int idec(dev_t dev, ino_t inode_number, ino_t parent_vol)
{
    int rc;
	struct	stat	statinf;
    struct  i_header header;
    long lnk;

    rc = get_header(&header, inode_number);
    if ( rc != 0 ) return -1;
    
    if ( header.lnk == 1 ) { /* file needs to be removed */
        char filename[FNAMESIZE];
        char resfilename[FNAMESIZE];

        inotostr(inode_number, filename);
        inotores(inode_number, resfilename);
        rc = unlink(filename);
        if ( rc ) printf("Error deleting file: %s\n", filename);
            
        rc = unlink(resfilename);
        if ( rc ) {
            printf("Error deleting resource file: %s\n", resfilename);
            return -1;
        }
        return 0;  

    } else { /* just decrease link count */
        lnk = header.lnk - 1;
        set_link(&lnk , inode_number);
    }

    return 0;
}


/*
 * iread,
 * opens the first file in the inode chain and performs a read & close.
 */
int iread(dev_t dev, ino_t inode_number, ino_t parent_vol, int offset, 
          char *buf, int count) 
{
	int	fd,res;
	char	inofile[FNAMESIZE];

	if (offset < 0) {
		errno= EINVAL;
		return -1;
	}
	inotostr(inode_number,  inofile);

	if ((fd=open(inofile, O_RDONLY , 0))<0)
		return fd;
	
	/*  lseek and read the requested data  */
	if (lseek(fd,offset, 0)<0)
		return -1;

	res=read(fd, buf, count);
	close(fd);
	return res;
}

/*
 * iwrite,
 * Similar to iread, opens the first file in the inode chain,
 * and reads data and then closes the fd.
 *
 */
int iwrite(dev_t dev, ino_t inode_number,ino_t  parent_vol, int  offset, 
           char *buf, int count)
{
	int	fd,res;
	char inofile[FNAMESIZE];

	inotostr(inode_number,  inofile);

	if (offset < 0) {
		errno= EINVAL;
		return -1;
	}

	if ((fd=open(inofile, O_WRONLY, 0))<0)
		return fd;
	
	/*  lseek and read the requested data  */
	if (lseek(fd, offset, 0)<0)
		return -1;

	res=write(fd, buf, count);
	close(fd);
	return res;
}

/*
 * istat,
 * retrieves inode information from the first file in the inode chain.
 * and stores the attributes from the resource file to the fields of the
 * stat struct.
 *
 */
int istat(dev_t dev,ino_t  inode_number, struct stat *statbuf)
{
	int	fd,res;
	char inofile[FNAMESIZE];
	struct i_header header;

	inotostr(inode_number, inofile);

	if (stat(inofile, statbuf)<0)
		return -1;

    if ( get_header(&header, inode_number) != 0 ) 
        return -1;

    /* is this really what we want??? XXXX */	
	(*(long *)&statbuf->st_gid)=header.volume;
    statbuf->st_nlink = header.lnk;
	statbuf->st_size=header.vnode;
	statbuf->st_blksize=header.unique;
	statbuf->st_blocks=header.dataversion;
	return 0;
}



ino_t maxino() 
{
    struct dirent **namelist;
    int n, i;
    ino_t max;
    
    n = scandir(mountpnt, &namelist, 0, &inosort);
    if (n < 0)
        perror("scandir");
    else
        max =  atoi(namelist[n-1]->d_name);

    for ( i = 0 ; i < n ; i++ ) free(namelist[i]);
    free(namelist) ;

    return max;

}

static int inosort(const struct dirent **a, const struct dirent **b)
{
    ino_t inoa, inob;
    
    inoa = atoi((*a)->d_name);
    inob = atoi((*b)->d_name);
    
    if ( inoa == inob ) return 0;
    if ( inoa > inob ) return 1;
    if ( inoa < inob ) return -1;

}

#endif
