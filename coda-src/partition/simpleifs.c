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

/*
 * userspace IFS library.
 * Designed to simulate the inode related system calls
 *
 * Based on an idea of Peter Braam.
 *
 * Written by Werner Frielingsdorf. 24-10-96.
 * Rewritten with enhancements  Peter Braam
 * Completely rewritten to use partition methods
 *
 */

/*
 * Simple Inode Operations
 *
 * Inodes are simulated using files.
 * The name of the file is the "inode number" for the file
 * 1 2 3 4 5 6 7 etc.
 * The vnode, volume, unique and dataversion fields are stored in the
 * resource forks:
 * .1 .2 .3 .4 etc.
 */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>
#include "partition.h" /* this includes simpleifs.h */

static int set_link(struct DiskPartition *dp, long *count, Inode ino);
static Inode maxino(struct DiskPartition *dp);
static int inosort(const struct dirent * const *a, 
		   const struct dirent *const *b);

static int s_init (union PartitionData **data, Partent partent, Device *dev);
static int s_iopen(struct DiskPartition *, Inode inode_number, int flag);
static int s_get_header(struct DiskPartition *dp, struct i_header *header, 
			Inode ino);
static int s_idec(struct DiskPartition *dp, Inode inode_number, 
		  Inode parent_vol);
static Inode s_icreate(struct DiskPartition *dp, Inode inode_number, u_long volume, u_long vnode, u_long unique, u_long dataversion);
static int s_iread(struct DiskPartition *dp, Inode inode_number, Inode parent_vol, int offset, char *buf, int count);
static int s_iwrite(struct DiskPartition *dp, Inode inode_number,Inode  parent_vol, int  offset, char *buf, int count);
static int s_put_header(struct DiskPartition *dp, struct i_header *header, Inode ino);
static int s_iinc(struct DiskPartition *dp, Inode  inode_number, Inode parent_vol);
static int s_magic();
int s_list_coda_inodes(struct DiskPartition *, char *resultFile,
		   int (*judgeInode)(struct ViceInodeInfo*, VolumeId), 
		   int judgeParam);
static int UifsGetHeader(struct DiskPartition *dp, Inode ino, struct ViceInodeInfo *info);

struct inodeops inodeops_simple = {
    s_icreate,
    s_iopen,
    s_iread,
    s_iwrite,
    s_iinc,
    s_idec,
    s_get_header,
    s_put_header,
    s_init,
    s_magic,
    s_list_coda_inodes
};


/* static data */
static mode_t   mode=S_IREAD | S_IWRITE;

static int s_magic()
{
    return VICEMAGIC;
}

static void inotostr(struct DiskPartition *dp, Inode ino, char *filename)
{
    sprintf(filename,"%s/%lu", dp->name, ino);
}


static void inotores(struct DiskPartition *dp, Inode ino, char *filename)
{
        sprintf(filename,"%s/.%lu", dp->name, ino);
}


/*
 * init: do some sanity checks
 */
static int 
s_init (union PartitionData **data, Partent partent, Device *dev)
{
    struct part_simple_opts *options = NULL;
    struct stat buf;
    int rc;
    
    options = (struct part_simple_opts *)malloc(sizeof(union PartitionData));
    if ( options == NULL ) {
	eprint("Out of memory\n");
	CODA_ASSERT(0);
    } else {
	*data = (union PartitionData *)options;
    }
    
    rc = stat(Partent_dir(partent), &buf);
    if ( rc == 0 ) {
	*dev = buf.st_dev;
    } else {
	eprint("Error in init of partition %s:%s", 
	       Partent_host(partent), Partent_dir(partent));
	perror("");
	CODA_ASSERT(0);
    }
    
    options->next = 0;
    return 0;
}


/*
 * iopen, simply returns an open call on the first file of the inode chain
 * and lseek to the data offset.
 */

static int s_iopen(struct DiskPartition *dp, Inode inode_number,int flag)
{
    char	filename[FNAMESIZE];
    int	fd;

	/* Attempt open. */

    if ( flag & O_CREAT ) {
        printf("Cannot create files with iopen. Use icreate.\n");
        return -1;
    }

	inotostr(dp, inode_number, filename);
	fd=open(filename, flag, 0600);
	if (fd<0)	return -1;	

	return fd;
}



/*
 * icreate.  Simulates the kernel ifs call icreate.
 * returns -1 in case of failure
 */

static Inode 
s_icreate(struct DiskPartition *dp, Inode inode_number, 
	u_long volume, u_long vnode, 
	u_long unique, u_long dataversion)
{
    struct part_simple_opts *opts=&dp->d->simple;
    int	fd, rc;
    Inode	i;
    char	filename[FNAMESIZE];
    char	resfilename[FNAMESIZE];
    struct
	i_header header={1, volume, vnode, unique, dataversion, VICEMAGIC};

    /*  Find an available inode. */
    fd = 0;
    while ( fd == 0 ) { 
        if ( opts->next == 0 ) {
            /* XXX critical */
            opts->next = maxino(dp) + 1;
            i= opts->next;
        } else {
            opts->next++;
            i= opts->next;
        }

        inotostr(dp, i, filename);
        if ((fd = open(filename, O_CREAT | O_EXCL, mode)) < 0) {
            if ( errno == EEXIST ) {
                opts->next = 0; 
                fd = 0; 
                continue;
            } else {
                return 0;
            }
        } else {
            close(fd);
        }
    }

	/* write header */
	inotores(dp, i, resfilename);
	if ( (fd = open(resfilename, O_CREAT | O_EXCL | O_RDWR, mode) ) < 0 ){
        printf("Error opening resource for inode file %ld\n",i);
        unlink(filename);
        return 0;
	} else { 
        rc = write(fd, (char *)&header, sizeof(struct i_header)); 
        if ( rc != sizeof(struct i_header)) {
            close (fd);
            printf("Error writing header for inode file %ld\n",i);
            return 0;
        }
	}
	close(fd);

	return i;
}

/* write header */
static int 
s_put_header(struct DiskPartition *dp, struct i_header *header, Inode ino)
{
    char	resfilename[FNAMESIZE];
    int fd, rc;

    inotores(dp, ino, resfilename);
    if ( (fd = open(resfilename, O_RDWR)) < 0 ){
        printf("Error opening resource for inode file %ld\n",ino);
        return -1;
    } else { 
        rc = write(fd, (char *)header, sizeof(struct i_header)); 
        if ( rc != sizeof(struct i_header)) {
            close (fd);
            printf("Error writing header for inode file %ld\n",ino);
            return -1;
        }
    }

    close(fd);
    return 0;
}

/* read header */
static int 
s_get_header(struct DiskPartition *dp, struct i_header *header, Inode ino)
{
    char	resfilename[FNAMESIZE];
    int fd, rc;

    inotores(dp, ino, resfilename);
    if ( (fd = open(resfilename, O_RDONLY) ) < 0 ){
        printf("Error opening resource for inode file %ld\n",ino);
        return -1;
    } else { 
        rc = read(fd, (char *)header, sizeof(struct i_header)); 
        if ( rc != sizeof(struct i_header)) {
            close (fd);
            printf("Error reading header for inode file %ld\n",ino);
            return -1;
        }
    }

    close(fd);
    return 0;
}


/* set the link count in the header */
static int 
set_link(struct DiskPartition *dp, long *count, Inode ino)
{
    char	resfilename[FNAMESIZE];
    int fd, rc;

    if ( count <= 0) {
        printf("Cannot set link number to non-positive integer\n");
        return -1;
    }

    inotores(dp, ino, resfilename);
    if ( (fd = open(resfilename, O_RDWR)) < 0 ){
        printf("Error opening resource for inode file %ld\n",ino);
        return -1;
    } else { 
        rc = write(fd, (char *)count, sizeof(long)); 
        if ( rc != sizeof(long)) {
            close (fd);
            printf("Error reading header for inode file %ld\n",ino);
            return -1;
        }
    }
    
    close(fd);
    return 0;
}


/*
 * iinc, increments the links by adding another file to the inode chain.
 */
static int 
s_iinc(struct DiskPartition *dp, Inode  inode_number, Inode parent_vol)
{
    int  rc;
    char	inofile[FNAMESIZE];
    struct	stat	statinf;
    struct  i_header header;
    long lnk;

    inotostr(dp, inode_number,  inofile);
	/* Get number of files in chain from stat info */
    if (stat(inofile, &statinf) < 0)
	return -1;

    rc = s_get_header(dp, &header, inode_number);
    if  ( rc != 0 ) return -1;
    
    lnk = header.lnk + 1;
    rc = set_link(dp, &lnk, inode_number);
    if  ( rc != 0 ) return -1;
    
    return 0;
}


/*
 * idec
 * decreases the link attribute in the resource, or removes file
 * when this is about to become negative
 */
static int 
s_idec(struct DiskPartition *dp, Inode inode_number, Inode parent_vol)
{
    int rc;
    struct  i_header header;
    long lnk;

    rc = s_get_header(dp, &header, inode_number);
    if ( rc != 0 ) return -1;
    
    if ( header.lnk == 1 ) { /* file needs to be removed */
        char filename[FNAMESIZE];
        char resfilename[FNAMESIZE];

        inotostr(dp, inode_number, filename);
        inotores(dp, inode_number, resfilename);
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
        set_link(dp, &lnk , inode_number);
    }

    return 0;
}


/*
 * iread,
 * opens the first file in the inode chain and performs a read & close.
 */
static int 
s_iread(struct DiskPartition *dp, Inode inode_number, Inode parent_vol, 
      int offset, char *buf, int count) 
{
	int	fd,res;
	char	inofile[FNAMESIZE];

	if (offset < 0) {
		errno= EINVAL;
		return -1;
	}
	inotostr(dp, inode_number,  inofile);

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
static int 
s_iwrite(struct DiskPartition *dp, Inode inode_number,Inode  parent_vol, 
       int  offset, char *buf, int count)
{
	int	fd,res;
	char inofile[FNAMESIZE];

	inotostr(dp, inode_number,  inofile);

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
static int istat(struct DiskPartition *dp, Inode  inode_number, struct stat *statbuf)
{
    char inofile[FNAMESIZE];
    struct i_header header;

    inotostr(dp, inode_number, inofile);

    if (stat(inofile, statbuf)<0)
	return -1;

    if ( s_get_header(dp, &header, inode_number) != 0 ) 
        return -1;

    /* is this really what we want??? XXXX */	
    (*(long *)&statbuf->st_gid)=header.volume;
    statbuf->st_nlink = header.lnk;
    statbuf->st_size=header.vnode;
    statbuf->st_blksize=header.unique;
#ifndef DJGPP
    statbuf->st_blocks=header.dataversion;
#endif
    return 0;
}




static int inosort(const struct dirent * const *a, const struct dirent * const *b)
{
    Inode inoa, inob;
    
    inoa = atoi((*a)->d_name);
    inob = atoi((*b)->d_name);
    
    if ( inoa == inob ) 
	    return 0;
    else  if ( inoa > inob ) 
	    return 1;

    return -1;

}

static Inode maxino(struct DiskPartition *dp) 
{
    struct dirent **namelist;
    int n, i;
    Inode max = 0;

#ifndef __CYGWIN32__    
    n = scandir(dp->name, &namelist, 0, &inosort);
#else
    CODA_ASSERT(0);
#endif
    if (n < 0)
        perror("scandir");
    else
        max =  atoi(namelist[n-1]->d_name);

    for ( i = 0 ; i < n ; i++ ) free(namelist[i]);
    free(namelist) ;

    return max;

}


int s_list_coda_inodes(struct DiskPartition *dp, char *resultFile,
		       int (*judgeInode)(struct ViceInodeInfo*, VolumeId), 
		       int judgeParam)
{
    DIR *pdir;
    Inode ino;
    char *devname=dp->name;
    struct dirent *ent=NULL;
    int rc;
    FILE *inodeFile = NULL;
    char  err[200];
    struct dinode *inodes = NULL;

    LogMsg(9, VolDebugLevel, stdout, 
	   "Entering ListCodaInodes(%s, %s, 0x%x, %u)",
	   devname, resultFile, judgeInode, judgeParam);
   
    pdir = opendir(devname);
    if (pdir == NULL) {
       sprintf(err, "Could not read directory %s to get inode list\n", 
	       devname);
       perror(err);
       return -1;
   }

    inodeFile = fopen(resultFile, "w");
    if (inodeFile == NULL) {
        LogMsg(0, VolDebugLevel, stdout, 
	       "Unable to create inode description file %s", resultFile);
	return -1;
   }


    LogMsg(0, VolDebugLevel, stdout, 
	   "Scanning inodes in directory %s...", devname);

    /* scan the directory for inodes */
    while ( (ent = readdir(pdir)) != NULL) {
        struct ViceInodeInfo info;
	ino = atol(ent->d_name);

	if ( !isdigit(*(ent->d_name)) )
	    continue;
	rc = UifsGetHeader(dp, ino, &info);
	if ( rc == 0 ) {
	    if (fwrite((char *)&info, sizeof info, 1, inodeFile) != 1) {
	        LogMsg(0, VolDebugLevel, stdout, 
		       "Error writing inode file for partition %s", devname);
		goto out;
	  }
	}
    }
    closedir(pdir);
    fclose(inodeFile);
    free((char *)inodes);
    return 0;

out:
    if (pdir) closedir(pdir);
    if (ent) free(ent);
    if (inodeFile) fclose(inodeFile);
    return -1;
}

static int UifsGetHeader(DiskPartition *dp, Inode ino, 
			 struct ViceInodeInfo *info)
{
    struct i_header header;
    struct stat sbuf;
    int rc;
    char filename[FNAMESIZE];

    rc = s_get_header(dp, &header, ino);
    if ( rc != 0 ) 
	return -1;
    
    inotostr(dp, ino, filename);
    rc = stat(filename, &sbuf);
    if ( rc != 0 )
	return -1;

    if ( (header.magic != VICEMAGIC) || !(S_ISREG(sbuf.st_mode)) ) 
	return -1;
    
    info->InodeNumber = ino;
    info->ByteCount = sbuf.st_size;
    info->LinkCount = header.lnk;
    info->VolumeNo = header.volume;
    info->VnodeNumber = header.vnode;
    info->VnodeUniquifier = header.unique;
    info->InodeDataVersion = header.dataversion;
    return 0;
}

