#ifndef _BLURB_
#define _BLURB_
/*

     RVM: an Experimental Recoverable Virtual Memory Package
			Release 1.3

       Copyright (c) 1990-1994 Carnegie Mellon University
                      All Rights Reserved.

Permission  to use, copy, modify and distribute this software and
its documentation is hereby granted (including for commercial  or
for-profit use), provided that both the copyright notice and this
permission  notice  appear  in  all  copies  of   the   software,
derivative  works or modified versions, and any portions thereof,
and that both notices appear  in  supporting  documentation,  and
that  credit  is  given  to  Carnegie  Mellon  University  in all
publications reporting on direct or indirect use of this code  or
its derivatives.

RVM  IS  AN  EXPERIMENTAL  SOFTWARE  PACKAGE AND IS KNOWN TO HAVE
BUGS, SOME OF WHICH MAY  HAVE  SERIOUS  CONSEQUENCES.    CARNEGIE
MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.
CARNEGIE MELLON DISCLAIMS ANY  LIABILITY  OF  ANY  KIND  FOR  ANY
DAMAGES  WHATSOEVER RESULTING DIRECTLY OR INDIRECTLY FROM THE USE
OF THIS SOFTWARE OR OF ANY DERIVATIVE WORK.

Carnegie Mellon encourages (but does not require) users  of  this
software to return any improvements or extensions that they make,
and to grant Carnegie Mellon the  rights  to  redistribute  these
changes  without  encumbrance.   Such improvements and extensions
should be returned to Software.Distribution@cs.cmu.edu.

*/

static char *rcsid = "$Header: rdsinit.c,v 1.6 96/11/19 14:32:12 tilt Exp $";
#endif _BLURB_


/*
 * This module holds the routines to initialize the datadevice of a heap.
 * Hopefully it'll someday be merged with something hank wrote to initialize
 * the log.
 */

#include <stdlib.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <stdio.h>
#include <unistd.h>
#include <rvm.h>
#include <rvm_segment.h>
#include <rds.h>

#ifdef __STDC__
#include <string.h>
#define BZERO(D,L)   memset((D),0,(L))
#else
#define BZERO(D,L)   bzero((D),(L))
#endif

void
PollAndYield() {
}

extern int          errno;              /* kernel error number */

int main(argc, argv)
     int  argc;
     char *argv[];
{
    rvm_options_t       *options;       /* options descriptor ptr */
    rvm_return_t	ret;
    int err, fd, i;
    char string[80], *static_addr, *sptr, buf[4096];
    int slen, hlen, nlists, chunksize;
    rvm_offset_t DataLen;
    struct stat sbuf;
    long length;
    
    if (argc < 3) {
	printf("Usage: %s logdevice datadevice\n", argv[0]);
	exit(-1);
    }

    /* initialized RVM */
    options = rvm_malloc_options();
    options->log_dev = argv[1];
	      
    ret = RVM_INIT(options);
    if  (ret != RVM_SUCCESS) 	
	printf("?  rvm_initialize failed, code: %s\n",rvm_return(ret));
    else
	printf("rvm_initialize succeeded.\n");

    /* get data file charateristics, size */
    errno = 0;
    if (stat(argv[2], &sbuf) < 0)
        sbuf.st_mode = 0;
    switch (sbuf.st_mode & S_IFMT) {
  case S_IFSOCK:
  case S_IFDIR:
  case S_IFLNK:
  case S_IFBLK: 
    printf("?  Illegal file type!\n");
    exit(-1);
    
  case S_IFCHR:
  get_size:   printf("Enter the length of the file/device %s: ", argv[2]);
    fgets(string,80,stdin);
    
    sptr = string;
    if ((*sptr == '0') && (*(++sptr) == 'x'))
        sscanf(string, "0x%lx", &length);
    else
        length = atoi(string);
    
    DataLen = RVM_MK_OFFSET(0, length);
    break;
    
  case S_IFREG:		/* Normal files. */
    length = sbuf.st_size;	
    if (length == 0) goto get_size;
    DataLen = RVM_MK_OFFSET(0, sbuf.st_size);
    break;
  default:
    if (errno == ENOENT) goto get_size; /* must create file */
    printf("?  Error getting status for %s, errno = %d\n",
            argv[2],errno);
    exit(-1);
    }

    fd = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 00644);
    if (fd < 0) {
	printf("?  Couldn't truncate %s.\n", argv[2]);
	exit(-1);
    }

    printf("Going to initialize data file to zero, could take awhile.\n");
    lseek(fd, 0, 0);
    BZERO(buf, 4096);
    for (i = 0; i < length; i+= 4096) {
	if (write(fd, buf, 4096) != 4096) {
	    printf("?  Couldn't write to %s.\n", argv[2]);
	    exit(-1);
	}
    }
    printf("done.\n");
    
    close(fd);

    printf("starting address of rvm: 0x");
    fgets(string,80,stdin);
    sscanf(string, "%x", (int *)&static_addr);
    printf("heap len: 0x");
    fgets(string,80,stdin);
    sscanf(string, "%X", &hlen);
    printf("static len: 0x");
    fgets(string,80,stdin);
    sscanf(string, "%X", &slen);
    printf("nlists: ");
    fgets(string,80,stdin);
    nlists = atoi(string);
    printf("chunksize: ");
    fgets(string,80,stdin);
    chunksize = atoi(string);

    rds_zap_heap(argv[2], DataLen, static_addr, slen, hlen, nlists, chunksize, &err);
    if (err == SUCCESS)
	printf("rds_zap_heap completed successfully.\n");
    else
        {
        if (err > SUCCESS)
            printf("?  ERROR: rds_zap_heap %s.\n",
                   rvm_return(err));
        else
            printf("?  ERROR: rds_zap_heap, code: %d\n", err);
        exit(-1);
        }

    ret = rvm_terminate();
    if (ret != RVM_SUCCESS)
        {
	printf("\n? Error in rvm_terminate, ret = %s\n", rvm_return(ret));
        exit(-1);
        }
    else
	printf("rvm_terminate succeeded.\n");

    return 0;
}
