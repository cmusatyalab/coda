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
 * Tree inode operations
 *
 * Inodes are simulated using files.
 * The name of the file is the "inode number" for the file transformed
 * into a tree structure: e.g. inode 4711 in base 10 with width 1 and 
 * depth 5 would become 0/4/7/1/1.
 * at width 2 it would become 0/0/0/47/11
 * The vnode, volume, unique and dataversion fields are stored in the
 * resource forks:
 * 0/4/7/1/.1 and 0/0/0/47/.11
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include "coda_string.h"
#include <errno.h>
#include <math.h>

#include <lwp/lwp.h>
#include <lwp/lock.h>
#include <util.h>
#include "vicetab.h"
#include "inodeops.h"
#include "partition.h"
#include "mkpath.h"

static int f_init (union PartitionData **data, Partent partent, Device *dev);
static int f_iopen(struct DiskPartition *, Inode inode_number, int flag);
static int f_get_header(struct DiskPartition *dp, struct i_header *header, 
                        Inode ino);
static int f_idec(struct DiskPartition *dp, Inode inode_number, 
                  Inode parent_vol);
static Inode f_icreate(struct DiskPartition *dp, Inode inode_number, 
		       u_long volume, u_long vnode, u_long unique, 
		       u_long dataversion);
static int f_iread(struct DiskPartition *dp, Inode inode_number, 
		   Inode parent_vol, int offset, char *buf, int count);
static int f_iwrite(struct DiskPartition *dp, Inode inode_number,
		    Inode  parent_vol, int  offset, char *buf, int count);
static int f_put_header(struct DiskPartition *dp, struct i_header *header, 
			Inode ino);
static int f_iinc(struct DiskPartition *dp, Inode  inode_number, 
		  Inode parent_vol);
static int f_magic();
int f_list_coda_inodes(struct DiskPartition *, char *resultFile,
                   int (*judgeInode)(struct ViceInodeInfo*, VolumeId), 
                   int judgeParam);
static int Header2Info(struct DiskPartition *dp, struct i_header *header, 
		       Inode ino, struct ViceInodeInfo *info);

struct inodeops inodeops_ftree = {
    f_icreate,
    f_iopen,
    f_iread,
    f_iwrite,
    f_iinc,
    f_idec,
    f_get_header,
    f_put_header,
    f_init,
    f_magic,
    f_list_coda_inodes
};


static mode_t   mode=S_IREAD | S_IWRITE;


static int f_magic()
{
    return VICEMAGIC;
}

/* note: caller responsible for string allocation */
void 
f_inotostr(struct DiskPartition *dp, Inode ino, char *filename)
{
    char str[MAXPATHLEN];
    int d, mask, depth, width;
    unsigned long comp;

    depth = dp->d->ftree.depth;
    width = dp->d->ftree.logwidth;
    mask =  ~(0xffffffff << width);
    sprintf(filename, "%s", dp->name);

    for ( d = depth-1 ; d >= 0 ; d--  ) {
	comp = ino >> (d * width);
	snprintf(str, MAXPATHLEN, "/%x", (unsigned)(comp & mask));
	strcat(filename, str);
    }
	
}

void 
printnames(struct DiskPartition *dp, int low, int step, int high)
{
    char file[MAXPATHLEN];
    int i;

    for ( i = low ; i < high ; i = i + step ) {
	f_inotostr(dp, i, file);
	printf("file: %s\n", file);
    }
}

/*
 * init: do some sanity checks
 * open resource database 
 * set up bitmap 
 */
static int f_init (union PartitionData **data, 
		   Partent partent, Device *dev)
{
    struct part_ftree_opts *options = NULL;
    struct stat buf;
    int rc, val, size, i;
    char resfilename[MAXPATHLEN];
    long filecount;
    Bitv freemap;
    struct i_header header, nullheader;

    options = (struct part_ftree_opts *)malloc(sizeof(union PartitionData));
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
    
    if ( access(Partent_dir(partent), R_OK | W_OK) != 0 ) {
	eprint("Cannot access %s\n", Partent_dir(partent));
	perror("");
	CODA_ASSERT(0);
    }

    rc = Partent_intopt(partent, "depth", &val);
    if (rc != 0) {
	eprint("Invalid depth option in %s:%s\n",
	       Partent_host(partent), Partent_dir(partent));
	CODA_ASSERT(0);
    }
    options->depth = val;

    rc = Partent_intopt(partent, "width", &val);
    if (rc != 0) {
	eprint("Invalid width option in %s:%s\n",
	       Partent_host(partent), Partent_dir(partent));
	CODA_ASSERT(0);
    }
    options->width = val;

    i=0;
    do 
	i++;
    while ( (1<<i) != options->width &&  i<20 );

    if ( i != 10) {
	options->logwidth = i;
    } else {
	eprint("Width should be a power of 2 smaller than 2^20");
	CODA_ASSERT(0);
    }

    options->next = 0;
    
    strcpy(resfilename, Partent_dir(partent));
    strcat(resfilename, "/");
    strcat(resfilename, RESOURCEDB);

    rc = stat(resfilename, &buf);
    if ( rc != 0 || !(buf.st_mode & S_IFREG) ) {
	dev = 0;
	eprint("Error in init of partition %s:%s: no resource database.", 
	       Partent_host(partent), Partent_dir(partent));
	perror("");
	CODA_ASSERT(0);
    }
    
    /* open the resource data file */
    options->resource = open(resfilename, O_RDWR);
    if ( options->resource == -1 ) {
	dev = 0;
	eprint("Error opening resource file!\n");
	perror("");
	CODA_ASSERT(0);
    }

    /* set up a bitmap */
    filecount = (long)pow(options->width, options->depth);
    freemap = Bitv_new(filecount);
    /*    Bitv_print(freemap, stdout); */
    options->freebm = freemap;
    if ( !options->freebm ) {
	eprint("Cannot setup free bitmap for %s.\n", Partent_dir(partent));
	CODA_ASSERT(0);
    }

    /* mark bits with resource records */
    size = sizeof(struct i_header);
    bzero(&nullheader, size);
    i = 0; 
    while ( read(options->resource, &header, size) == size) {
	if ( memcmp(&header, &nullheader, size) != 0 ) {
	    Bitv_set(options->freebm, i);
	}
	i++;
    }
    options->freebm = freemap;
    /* Bitv_print(freemap, stdout); */

    /* done: leave message in the log */
    printf("Partition %s: inodes in use: %d, total: %ld.\n",
	    Partent_dir(partent), Bitv_count(options->freebm), filecount); 

    return 0;
}


/*
 * iopen, simply returns an open call on the first file of the inode chain
 * and lseek to the data offset.
 */

static int f_iopen(struct DiskPartition *dp, Inode inode_number, int flag)
{
    char filename[FNAMESIZE];
    int	fd;

    /* Attempt open. */

    if ( flag & O_CREAT ) {
        eprint("Cannot create files with iopen. Use icreate.\n");
        return -1;
    }

    f_inotostr(dp, inode_number, filename);
    fd = open(filename, flag, 0600);
    if (fd<0)
	return -1;	

    return fd;
}



/*
 * icreate.  Simulates the kernel ifs call icreate.
 * returns -1 in case of failure
 */

static Inode 
f_icreate(struct DiskPartition *dp, Inode inode_number, u_long volume, 
	  u_long vnode, u_long unique, u_long dataversion)
{
    struct part_ftree_opts *opts=&(dp->d->ftree);
    int	fd, rc, pos;
    Inode i, ino;
    char	filename[FNAMESIZE];
    struct
	i_header header={1, volume, vnode, unique, dataversion, VICEMAGIC};

    /*  Find an available inode. */
    i = (Inode) Bitv_getfree(opts->freebm);
    if ( i == -1 ) {
	eprint("No more free entries in freebitmap of %s\n",
	       dp->name);
	CODA_ASSERT(0);
    }
	
    ino = i+1; /*inode numbers start at 1 */
    
    f_inotostr(dp, ino, filename);
    if (mkpath(filename, 0700) < 0) {
        eprint("f_icreate: could not make ftree path!\n");
	CODA_ASSERT(0);
    }
    if ((fd = open(filename, O_CREAT | O_EXCL, mode)) < 0) {
	if ( errno == EEXIST ) {
	    eprint("f_icreate: bitmap free at %d, inode %ld exists!\n", 
		   i, ino);
	    CODA_ASSERT(0);
	} else {  /* other error, just fail */
	    eprint("f_icreate: error %d in creating inode %ld!\n", 
		   errno, ino);
	    CODA_ASSERT(0);
	}
    } else {
	close(fd);
    }

    /* write header in resouce db */
    pos = lseek(opts->resource, i * sizeof(struct i_header), SEEK_SET);

    if ( pos != i * sizeof(struct i_header) ) {
	eprint("Cannot seek in resource db for file %s\n", filename);
	unlink(filename);
	return 0;
    }

    rc = write(opts->resource, (char *)&header, sizeof(struct i_header)); 
    if ( rc != sizeof(struct i_header)) {
	printf("Error writing header for inode file %ld\n",i);
	unlink(filename);
	return 0;
    }

    rc = fsync(opts->resource);
    if ( rc != 0 ) {
	printf("Error syncing header for inode file %ld\n",i);
	unlink(filename);
	return 0;
    }

    return ino;
}

/* write header */
static int 
f_put_header(struct DiskPartition *dp, struct i_header *header, Inode ino)
{
    struct part_ftree_opts *opts = &(dp->d->ftree);
    int rc, pos;

    pos = lseek(opts->resource, (ino - 1) * sizeof(struct i_header), SEEK_SET);
    if ( pos != (ino -1) * sizeof(struct i_header) ){
        printf("Error seeking resource for inode file %ld\n",ino);
        return -1;
    } else { 
	rc = write(opts->resource, (char *)header, sizeof(struct i_header)); 
	if ( rc != sizeof(struct i_header)) {
	    printf("Error writing header for inode file %ld\n",ino);
	    return -1;
	}
    }
    return 0;
}

/* read header */
static int 
f_get_header(struct DiskPartition *dp, struct i_header *header, Inode ino)
{
    struct part_ftree_opts *opts = &(dp->d->ftree);
    int rc, pos;

    pos = lseek(opts->resource, (ino - 1) * sizeof(struct i_header), SEEK_SET);
    if ( pos != (ino -1) * sizeof(struct i_header) ){
        printf("Error seeking resource for inode file %ld\n",ino);
        return -1;
    } else { 
        rc = read(opts->resource, (char *)header, sizeof(struct i_header)); 
        if ( rc != sizeof(struct i_header)) {
            printf("Error reading header for inode file %ld\n",ino);
            return -1;
        }
    }
    return 0;
}



/*
 * chagne the lnk attribute 
 */
static int 
f_change_lnk(struct DiskPartition *dp, Inode  ino, long value, int inc)
{
    struct part_ftree_opts *opts = &(dp->d->ftree);
    struct i_header header;
    int rc;

    rc = f_get_header(dp, &header, ino);
    if ( rc != 0 ) {
        printf("Cannot get header %ld\n", ino);
        return -1;
    }

    if ( value ) 
	header.lnk = value;
    else 
	header.lnk = header.lnk + inc;

    if ( header.lnk < 0 ) {
	printf("Cannot set link count of ino %ld to negative number.",
	       ino);
	return -1;
    }

    /* check for removal case (lnk = 0) */
    if ( header.lnk == 0 ) { 
	char filename[MAXPATHLEN];

        f_inotostr(dp, ino, filename);
	rc = unlink(filename);
	if ( rc != 0 ) {
	    printf("Error unlinking inode file %ld!\n", ino);
	    return -1;
	}
	bzero(&header, sizeof(struct i_header));
	Bitv_clear(opts->freebm, ino-1);
    }

    rc = f_put_header(dp, &header, ino);
    if ( rc != 0 ) {
        printf("Cannot put header %ld\n", ino);
        return -1;
    }

    return 0;
}


/*
 * idec
 * decreases the link attribute in the resource, or removes file
 * when this is about to become negative
 */
static int 
f_idec(struct DiskPartition *dp, Inode inode_number, Inode parent_vol)
{
    return f_change_lnk(dp, inode_number, 0, -1);
}

/*
 * iinc
 * increases the link attribute in the resource
 */
static int 
f_iinc(struct DiskPartition *dp, Inode inode_number, Inode parent_vol)
{
    return f_change_lnk(dp, inode_number, 0, +1);
}


/*
 * iread,
 * opens the first file in the inode chain and performs a read & close.
 */
static int 
f_iread(struct DiskPartition *dp, Inode inode_number, Inode parent_vol, 
      int offset, char *buf, int count) 
{
	int	fd,res;
	char	inofile[FNAMESIZE];

	if (offset < 0) {
		errno= EINVAL;
		return -1;
	}
	f_inotostr(dp, inode_number,  inofile);

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
f_iwrite(struct DiskPartition *dp, Inode inode_number,Inode  parent_vol, 
       int  offset, char *buf, int count)
{
	int	fd,res;
	char inofile[FNAMESIZE];

	f_inotostr(dp, inode_number,  inofile);

	if (offset < 0) {
		errno= EINVAL;
		return -1;
	}

	if ((fd=open(inofile, O_WRONLY, 0))<0)
		return fd;
	
	/*  lseek and write the requested data  */
	if (lseek(fd, offset, 0)<0)
		return -1;

	res=write(fd, buf, count);
	close(fd);
	return res;
}

#if 0
/*
 * istat,
 * retrieves inode information from the first file in the inode chain.
 * and stores the attributes from the resource file to the fields of the
 * stat struct.
 *
 */
static int 
istat(struct DiskPartition *dp, Inode  inode_number, struct stat *statbuf)
{
    char inofile[FNAMESIZE];
    struct i_header header;

    f_inotostr(dp, inode_number, inofile);

    if (stat(inofile, statbuf)<0)
	return -1;

    if ( f_get_header(dp, &header, inode_number) != 0 ) 
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

static int inosort(const struct dirent **a, const struct dirent **b)
{
    Inode inoa, inob;
    
    inoa = atoi((*a)->d_name);
    inob = atoi((*b)->d_name);
    
    if ( inoa == inob ) return 0;
    if ( inoa > inob ) return 1;
    return -1;
}

/* generic routine to do things on all files in the tree */
void dosubs(int *level, int width, int depth)
{
    int d;
    char dir[256];
    (*level)++;
    mdirs(width);
printf("Starting %d\n", *level);
    /* only decend if not at top level */
    if (*level < depth) {
	for ( d = 0 ; d < width ; d++ ) {
	    sprintf(dir, "%x", d);
	    chdir(dir);
	    dosubs(level, width, depth);
	    chdir("..");
	}
    }
printf("Ending %d\n", *level);

    (*level)--;
}
#endif 

int f_list_coda_inodes(struct DiskPartition *dp, char *resultFile,
		       int (*judgeInode)(struct ViceInodeInfo*, VolumeId), 
		       int judgeParam)
{
    struct part_ftree_opts *opts;
    struct i_header header;
    int rc, fd, size = sizeof(struct i_header);
    Inode ino;
    FILE *inodeFile = NULL;

    CODA_ASSERT(dp && dp->ops && dp->d);
    LogMsg(9, VolDebugLevel, stdout, 
	   "Entering ListCodaInodes(%s, %s, 0x%x, %u)",
	   dp->name, resultFile, judgeInode, judgeParam);
   
    opts = &(dp->d->ftree);

    inodeFile = fopen(resultFile, "w");
    if (inodeFile == NULL) {
        LogMsg(0, VolDebugLevel, stdout, 
	       "Unable to create inode description file %s", resultFile);
	return -1;
   }


    LogMsg(0, VolDebugLevel, stdout, 
	   "Scanning inodes in directory %s...", dp->name);

    /* scan the directory for inodes */
    fd = opts->resource;
    lseek(fd, 0, SEEK_SET);
    ino = 0;
    while ( read(fd, &header, size) == size ) {
        struct ViceInodeInfo info;
	ino++;

	rc = Header2Info(dp, &header, ino, &info);
	if ( rc == 0 ) {
	    if (fwrite((char *)&info, sizeof info, 1, inodeFile) != 1) {
	        LogMsg(0, VolDebugLevel, stdout, 
		       "Error writing inode file for partition %s", dp->name);
		goto out;
	    }
	}
    }
    fclose(inodeFile);
    return 0;
out:
    if (inodeFile) fclose(inodeFile);
    return -1;
}

static int Header2Info(struct DiskPartition *dp, struct i_header *header, 
		       Inode ino, struct ViceInodeInfo *info)
{
    struct stat sbuf;
    int rc;
    char filename[FNAMESIZE];

    f_inotostr(dp, ino, filename);
    rc = stat(filename, &sbuf);
    if ( rc != 0 )
	return -1;

    if ( (header->magic != VICEMAGIC) || !(S_ISREG(sbuf.st_mode)) ) 
	return -1;
    
    info->InodeNumber = ino;
    info->ByteCount = sbuf.st_size;
    info->LinkCount = header->lnk;
    info->VolumeNo = header->volume;
    info->VnodeNumber = header->vnode;
    info->VnodeUniquifier = header->unique;
    info->InodeDataVersion = header->dataversion;
    return 0;
}


