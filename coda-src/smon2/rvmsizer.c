/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#include <stdio.h>
#include <stdlib.h> 
#include <sys/types.h>  // fts
#include <sys/stat.h>   // fts
#include <coda_fts.h>	// fts
#include <string.h>     // strcpy, strcat, strcmp
#include <errno.h>	// errno

/* vnode size
 * this is an over guess by about 16 bytes per file */
#define FILERVMSIZE 128
/* size of the Dir (vnode, 400 in other, 128 page pointers, and an int) */
#define DIRSIZE 1028 // 112+400 (node) + 128*4 (pages) + 4 (int) 
/* how much name size a dir can hold before it gains another page size */
#define DIRPAGE 2048  
/* how much room is used in the header that is not the name of files
   for each file */
#define DIRNONNAME 16 
/* the number of bytes lost in each dir page to non files
   1*32 to page header
   4*32 to allocation
   8*32 to hash
   1*32 other? not sure what
*/
#define PAGELOSS 448


void scantree(char * const *root)
{
    int options = FTS_COMFOLLOW | FTS_PHYSICAL | FTS_XDEV;
    FTS *tree;
    FTSENT *obj;
    char *err;
    int rc;

    unsigned long large_vnodes = 0, small_vnodes = 0, dirpages = 0;
    unsigned long long filesize = 0, totalsize = 0;
    unsigned long long tmpsize;
    unsigned long tmp;

    tree = fts_open(root, options, NULL);

    while ((obj = fts_read(tree)) != NULL || errno != 0) {
	if (!obj) {
	    perror("fts_read");
	    continue;
	}
	err = NULL;
	switch(obj->fts_info) {
	case FTS_D:
	    large_vnodes++;
	    obj->fts_number = PAGELOSS + 64; /* account for '.'/'..' entries */
	    break;

	case FTS_DP:
	    /* account for used directory pages */
	    tmp = (obj->fts_number + (DIRPAGE-1)) / DIRPAGE;

	    if (tmp >= 128) {
		printf("\rWARNING: '%s' needs %lu pages and won't fit in Coda\n",
		       obj->fts_path, tmp);
	    }

	    dirpages += tmp;
	    printf("\r%lu directories, %lu files, %lu directory pages",
		   large_vnodes, small_vnodes, dirpages);
	    break;

	case FTS_DC:   /* directory cycle */
	    err = "Directory cycle detected";
	    large_vnodes++;
	    break;
	    
	case FTS_DNR:  /* unreadable directory */
	    err = "Unreadable directory";
	    large_vnodes++;
	    break;

	case FTS_DOT:  /* "." or ".." entry */
	    err = "Unexpected '.' or '..' entry returned";
	    break;

	case FTS_ERR:  /* error */
	    err = "Unexpected error returned";
	    break;

	case FTS_NS:   /* no stat data */
	    err = "Unable to stat";
	    /* assume it is a file? */
	    small_vnodes++;
	    break;

	case FTS_NSOK: /* no stat data requested */
	    err = "No stat data requested?";
	    break;

	case FTS_F:
	    /* regular file */
	case FTS_SL:
	case FTS_SLNONE:
	    /* symbolic link */
	case FTS_DEFAULT:
	    /* anything else */
	    small_vnodes++;
	    if (obj->fts_statp)
		filesize += obj->fts_statp->st_size;
	    break;
	}
	if (err) {
	    printf("\r%s: %s", err, obj->fts_path);
	    if (errno)
		printf(" - %s", strerror(errno));
	    printf("\n");
	}

	/* Avoid double counting for directories */
	if (obj->fts_info == FTS_D)
	    continue;

	if (obj->fts_statp)
	    totalsize += obj->fts_statp->st_size;

	/* all filenames are padded to a multiple of 32 */
	tmp = (obj->fts_namelen + DIRNONNAME + (32-1)) & ~(32-1);

	/* account for used directory space in our parent */
	if (DIRPAGE - (obj->fts_parent->fts_number % DIRPAGE) < tmp) {
	    obj->fts_parent->fts_number =
		(obj->fts_parent->fts_number + (DIRPAGE-1)) & ~(DIRPAGE-1);
	}

	/* are we starting a new page? */
	if (obj->fts_parent->fts_number % DIRPAGE == 0)
	    obj->fts_parent->fts_number += PAGELOSS;
	obj->fts_parent->fts_number += tmp;
    }

    rc = fts_close(tree);
    if (rc)
	perror("fts_close");

#define OneMB (1024 * 1024)
#define MB(x) ((double)(x) / OneMB)

    tmpsize = filesize;
    printf("\ntotal file size        %llu bytes (%.2fMB)\n",
	   tmpsize, MB(tmpsize));

    tmpsize /= small_vnodes;
    printf("average file size      %llu bytes\n", tmpsize);

    tmpsize = totalsize - filesize;
    printf("total directory size   %llu bytes (%.2fMB)\n",
	   tmpsize, MB(tmpsize));

    tmpsize /= large_vnodes;
    printf("average directory size %llu bytes\n", tmpsize);

    tmpsize = (unsigned long long) dirpages * DIRPAGE;
    printf("estimated RVM used by directory data, %llu bytes (%.2fMB)\n",
	   tmpsize, MB(tmpsize));

    tmpsize += (unsigned long long) small_vnodes * FILERVMSIZE +
	      (unsigned long long) large_vnodes * DIRSIZE;
    printf("estimated RVM usage based on object counts, %llu bytes (%.2fMB)\n",
	   tmpsize, MB(tmpsize));

    tmpsize = totalsize * 0.04;
    printf("estimated RVM usage based on 4%% rule,       %llu bytes (%.2fMB)\n",
	   tmpsize, MB(tmpsize));
}


int main (int argc, char **argv) 
{
    char *exe = *argv;
    int v = 0;

    while(--argc) {
	argv++;
	if (**argv != '-') break;

	if ((*argv)[1] == 'v') v++;   /* verbosity level (0, 1, 2) */
	else goto usage;
    }

    if (argc == 0) {
usage:
	printf("Usage: %s [--] dir\n", exe);
	exit(-1);
    }

    scantree(argv);

    exit(0);
}

