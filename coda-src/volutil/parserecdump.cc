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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/volutil/parserecdump.cc,v 4.2 1997/02/26 16:04:02 rvb Exp $";
#endif /*_BLURB_*/






/*
 *  parserecdump: parses dumped array of recoverable storage used
 *  by a server.
 */


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <sys/time.h>
#include <ctype.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <stdio.h>
#include <sys/file.h>
#include <assert.h>

#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif

#ifdef __cplusplus
}
#endif __cplusplus

#include "vol-dumprecstore.h"

int cmprec(dumprec_t *a, dumprec_t *b)
{
    if (a->rec_addr < b->rec_addr)
	return -1;
    else if (a->rec_addr > b->rec_addr)
	return 1;
    else if (a->size > b->size)
	return -1;
    else if (a->size < b->size)
	return 1;
    else return 0;
}	

PRIVATE void printdump(dumprec_t *a, int size)
{
    char    name[32];
    for (int i = 0; i < size; i++){
	switch(a[i].type) {
	    case VOLHEADT: 
		strcpy(name, "VOLHEADT");
		break;
	    case SVNODEPTRARRT:
		strcpy(name, "SVNODEPTRARRT");
		break;
	    case LVNODEPTRARRT:
		strcpy(name, "LVNODEPTRARRT");
		break;
	    case SVNODEDISKPTRT:
		strcpy(name, "SVNODEDISKPTRT");
		break;
	    case LVNODEDISKPTRT:
		strcpy(name, "LVNODEDISKPTRT");
		break;
	    case VOLDISKDATAT:
		strcpy(name, "VOLDISKDATAT");
		break;
	    case DIRINODET:
		strcpy(name, "DIRINODET");
		break;
	    case DIRPAGET:
		strcpy(name, "DIRPAGET");
		break;
	    case CAMELOTFREESTORE:
		strcpy(name, "CAMELOTFREESTORE");
		break;
	    case SFREEVNODEPTRARRT:
		strcpy(name,"SFREEVNODEPTRARRT");
		break;
	    case LFREEVNODEPTRARRT:
		strcpy(name,"LFREEVNODEPTRARRT");
		break;
	    case SFREEVNODEDISKPTRT:
		strcpy(name,"SFREEVNODEDISKPTRT");
		break;
	    case LFREEVNODEDISKPTRT:
		strcpy(name,"LFREEVNODEDISKPTRT");
		break;

	    default:
		strcpy(name, "Unknown");
		break;
	}
	printf("[%d] address = 0x%x size = 0x%x type = %s index = %d\n", 
	       i, a[i].rec_addr, a[i].size, name, a[i].index);
    }
}
PRIVATE void checkdump(dumprec_t *a, int size)
{
    printf("Checking dump....\n");
    for(int i = 1; i < size; i++)
	if ((a[i].rec_addr == a[i-1].rec_addr) || 
	    (a[i].rec_addr < (a[i-1].rec_addr + a[i-1].size)))
	    printf("Bad rec address at %d\n", i);
    printf("Finished Checking dump\n");
}

main(int argc, char **argv)
{
    if (argc != 2){
	printf("Usage: parserecdump <filename>\n");
	exit(-1);
    }
    char *fname = argv[1];
    struct stat st;
    if (stat(fname, &st) != 0){
	printf("Couldnt stat %s errno = %d\n", fname, errno);
	exit(-1);
    }
    int nelem = st.st_size/sizeof(dumprec_t);
    dumprec_t *dump;
    dump = (dumprec_t *)malloc(nelem * sizeof(dumprec_t));
    int fd = open(fname, O_RDONLY, 0644);
    if (fd < 0){
	printf("Couldnt open file %s\n", fname);
	exit(-1);
    }
    if (read(fd, (char *)dump, nelem * sizeof(dumprec_t)) !=st.st_size){
	printf("Error while reading %s\n", fname);
	exit(-1);
    }
    qsort((char *)dump, nelem, sizeof(dumprec_t), 
		(int (*)(const void *, const void *))cmprec);
    printdump(dump, nelem);
    checkdump(dump, nelem);
}


