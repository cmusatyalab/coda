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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/rvm-src/rds/Attic/rdsinit.c,v 4.3 1997/04/01 01:57:17 clement Exp $";
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
#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif
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

#define UP 101
#define DOWN 102
PRIVATE round_to_multiple(pval, n, dir)
     long *pval;
     long n;
     int dir;
{
  long rem;
  if ((rem = *pval % n) != 0) {
    printf("Umn... your input is not a multiple of %d\n", n);
    if (dir==UP)
      *pval += n-rem; /* round up to next integral multiple*/
    else if (dir==DOWN)
      *pval -= rem;   /* round down */
    else { /* this should not happen */
      printf("rdsinit: round_to_multi(): internal prog. error\n");
      exit(-1);
    }
    printf(" I'll round it %s to %s integral multiple which is %d (0x%x)\n",
	   (dir==UP ? "up" : "down"), (dir==UP ? "next" : "prev"), 
           *pval, *pval);
  }
}

PRIVATE get_dev_size(devName, plength)
     char *devName;
     long *plength;
{
  char string[80], *sptr;
  int paraOK=0;
  long rem;
  do {
      printf("\n============================================================\n");
      printf("0) length of data file/dev\n");
      printf("   Data file/dev (%s) is the segment to be mapped by rvm.  \n", devName);
      printf("   There will be two regions in the segment:\n");
      printf("   one is for the heap, the other is for the static.\n");
      printf("   One more extra page (pagesize=%d) will also \n", RVM_PAGE_SIZE);
      printf("   be needed for the segment header.\n");
      printf("   Length of data file/dev must be a multiple of pagesize.\n");
      printf("   Please make sure you have enough space in your disk.\n");
      printf("   =====> please enter the length, you may enter either decimal\n");
      printf("          or hex value (in the latter case, precede the number by 0x).\n");
      printf("          length: ");
      fgets(string,80,stdin);
    
      sptr = string;
      if ((*sptr == '0') && (*(++sptr) == 'x'))
	  sscanf(string, "0x%lx", plength);
      else
	  *plength = atoi(string);
      
      if ((rem = *plength % RVM_PAGE_SIZE) != 0) {
	printf("Umn... your length is not a multiple of %d\n", RVM_PAGE_SIZE);
	*plength += RVM_PAGE_SIZE-rem; /* round up to next integral multiple*/
	printf(" I'll round it up to next integral multiple which is %d (0x%x)\n"
	       , *plength, *plength);
      }

      printf("\nThe data file/dev (%s) will be of length %d (0x%x)\n", devName, *plength, *plength);
      printf("Do you agree ? (y|n) ");	
      fgets(string,80,stdin);
      if (strcmp(string,"y\n") == 0 || strcmp(string,"Y\n") == 0
          || strcmp(string, "\n") == 0 )
	  paraOK = 1;
    } while (!paraOK);
}

PRIVATE get_valid_parm(argc, argv, length, pstatic_addr, phlen, pslen, pnlists,pchunksize)
    int argc;
    char **argv;
    long length;
    char **pstatic_addr;
    int *phlen;
    int *pslen;
    int *pnlists;
    int *pchunksize;
{
    char string[80];
    int paraOK;
    paraOK = 0;
    do {
	if ( argc == 8 ) {
	  /* accept the 5 parameter as cmd-line argument */
	  /* Usage: rdsinit <log-file> <data-file> <a> <h> <s> <n> <c> */
	  strncpy(string,argv[3],80);
	  sscanf(string, "%x", (int *)pstatic_addr);
	  strncpy(string,argv[4],80);
	  sscanf(string, "%x", phlen);
	  strncpy(string, argv[5],80);
	  sscanf(string, "%x", pslen);
	  strncpy(string, argv[6],80);
	  *pnlists = atoi(string);
	  strncpy(string, argv[7],80);
	  *pchunksize = atoi(string);
	  argc -=5;
	} else {
	  printf("\n");
	  printf("============================================================\n");
	  printf("  Getting parameters for RDS\n");
	  printf("  (Your data file/dev is of length 0x%x (%d))\n", length, length);
	  printf("============================================================\n");
	  printf("1) starting address of rvm\n");
	  printf("   This is where heap and static will be mapped into virtual memory,\n");
	  printf("   it must be larger than the largest possible break point of your application,\n");
	  printf("   and it should not be in conflict other use of vm (such as shared libraries).\n");
	  printf("   It must be on a page boundary (pagesize=0x%x)\n", RVM_PAGE_SIZE);   
	  printf("   (In CMU, we use 0x20000000 (536870912) with Linux and BSD44,\n");
	  printf("   0x70000000 (1879048192) with Mach and BSD44 without problem.)\n");


	  printf("   =====> please enter a hex value for starting address of rvm: 0x");
	  fgets(string,80,stdin);
	  sscanf(string, "%x", (int *)pstatic_addr);
	  round_to_multiple(pstatic_addr, RVM_PAGE_SIZE, UP);
	get_heap_n_static:
	  printf("2) heap len\n");
	  printf("   It is the size of the dynamic heap to be managed by RDS,\n");
	  printf("   it must be an integral multiple of pagesize.\n");
	  printf("   =====> please enter a hex value for heap len: 0x");
	  fgets(string,80,stdin);
	  sscanf(string, "%X", phlen);
	  round_to_multiple(phlen, RVM_PAGE_SIZE, UP);
	  printf("3) static len\n");
	  printf("   It is the size of the statically allocated location to be\n");
	  printf("   managed by your application.\n");
	  printf("   It must be an integral multiple of pagesize.\n");
	  printf("   =====> please enter a hex value for static len: 0x");
	  fgets(string,80,stdin);
	  sscanf(string, "%X", pslen);
	  round_to_multiple(pslen, RVM_PAGE_SIZE, UP);

	  /* note: strictly speaking RVM_PAGE_SIZE should read
	   * RVM_SEGMENT_HDR_SIZE, but I don't want to include rvm_segment_private.h
	   */
	  if (length<*phlen+*pslen+RVM_PAGE_SIZE) {
	    printf("\n\nSorry ! your heap len + static len is too large !\n");
	    printf("   their sum must be less than 0x%x (%d)\n", 
		   length-RVM_PAGE_SIZE, length-RVM_PAGE_SIZE);
	    printf("   please re-enter\n\n");
	    goto get_heap_n_static;
	  }

	  printf("4) nlists\n");
	  printf("   It is the number of free list used by RDS.\n");
	  printf("   (nlist=100 is a reasonable choice.)\n");
	  printf("   =====> please enter a decimal value for nlists: ");
	  fgets(string,80,stdin);
	  *pnlists = atoi(string);
	get_chunksize:
	  printf("5) chunksize\n");
	  printf("   The free list are maintained in sizes of one chunk to n chunk,\n");
	  printf("   where n is the number of free lists.\n");
	  printf("   It must be an integral multiple of %d\n", sizeof(char *));
	  printf("   and must at least be RDS_MIN_CHUNK_SIZE.\n"); /* I don't what it is, either */
	  printf("   (chunksize=32 is a reasonable choice.)\n");
	  printf("   =====> please enter a decimal value for chunksize: ");
	  fgets(string,80,stdin);
	  *pchunksize = atoi(string);
	  if ( (*pchunksize % sizeof(char *)) != 0 ) {
	    printf("\n\nSorry ! chunksize must be an integral multiple of %d\n", sizeof(char *));
	    printf("   please re-enter\n\n");
	    goto get_chunksize;
	  }

        }
	printf("\nWith a data file/device of length 0x%x (%d)\n", length, length);
	printf("The following parameters are chosen:\n");
	printf("  starting address of rvm: %#10x (%10d)\n", *pstatic_addr, *pstatic_addr);
	printf("                 heap len: %#10x (%10d)\n", *phlen, *phlen);
	printf("               static len: %#10x (%10d)\n", *pslen, *pslen);
	printf("                   nlists: %#10x (%10d)\n", *pnlists, *pnlists);
	printf("               chunk size: %#10x (%10d)\n", *pchunksize, *pchunksize);
	printf("Do you agree with those parameters ? (y|n) ");
	fgets(string,80,stdin);
	if (strcmp(string,"y\n") == 0 || strcmp(string,"Y\n") == 0
	    || strcmp(string, "\n") == 0 )
	  paraOK = 1;
    } while (!paraOK);
    return;
}



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

    if (argc !=3 && argc !=8) {
	printf("Usage: %s log data [ saddr(h) hlen(h) slen(h) nl(d) chunk(d) ]\n", argv[0]);
	printf("         (h) parameter in hexidecimal\n");
	printf("         (d) parameter in decimal\n");
	exit(-1);
    }

    /* initialized RVM */
    options = rvm_malloc_options();
    options->log_dev = argv[1];
	      
    ret = RVM_INIT(options);
    if  (ret != RVM_SUCCESS) {
	printf("?  rvm_initialize failed, code: %s\n",rvm_return(ret));
	exit(-1);
    } else
	printf("rvm_initialize succeeded.\n");

    /* get data file charateristics, size */
    errno = 0;
    if (stat(argv[2], &sbuf) < 0)
        sbuf.st_mode = 0;
    switch (sbuf.st_mode & S_IFMT) {
  case S_IFSOCK:
  case S_IFDIR:
  case S_IFLNK:
/* LINUX use the same block device for raw control */
#ifndef LINUX
  case S_IFBLK: 
#endif
    printf("?  Illegal file type!\n");
    exit(-1);
    
/* LINUX use the same block device for raw control */
#ifdef LINUX
  case S_IFBLK:
#endif
  case S_IFCHR:
  get_size:
    get_dev_size(argv[2], &length);
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


    get_valid_parm(argc, argv, length, &static_addr, &hlen, &slen, &nlists, &chunksize);

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


























