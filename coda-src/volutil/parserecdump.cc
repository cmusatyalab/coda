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
 *  parserecdump: parses dumped array of recoverable storage used
 *  by a server.
 */


#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <sys/time.h>
#include <ctype.h>
#include <sys/param.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdio.h>
#include <sys/file.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include "coda_assert.h"

#ifdef __cplusplus
}
#endif

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

static void printdump(dumprec_t *a, int size)
{
    char name[32];
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
	printf("[%d] address = %p size = 0x%x type = %s index = %d\n", 
	       i, a[i].rec_addr, a[i].size, name, a[i].index);
    }
}
static void checkdump(dumprec_t *a, int size)
{
    printf("Checking dump....\n");
    for(int i = 1; i < size; i++)
	if ((a[i].rec_addr == a[i-1].rec_addr) || 
	    (a[i].rec_addr < (a[i-1].rec_addr + a[i-1].size)))
	    printf("Bad rec address at %d\n", i);
    printf("Finished Checking dump\n");
}

int main(int argc, char **argv)
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


