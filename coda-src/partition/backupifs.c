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
 * a dummy partition type for use by the backup system
 * no methods are needed at all, but some can be added
 * should we want that.
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
static int b_init (union PartitionData **data, Partent partent, Device *dev);

struct inodeops inodeops_backup = {
    NULL, /*icreate*/
    NULL, /*iopen*/
    NULL, /*iread*/
    NULL, /*iwrite*/
    NULL, /*iinc*/
    NULL, /*idec*/
    NULL, /*get_header*/
    NULL, /*put_header*/
    b_init, /*init*/
    NULL, /*magic*/
    NULL /*list_coda_inodes */
};

static int 
b_init (union PartitionData **data, Partent partent, Device *dev)
{
    struct stat buf;
    int rc;
    
    *data = NULL;
    
    rc = stat(Partent_dir(partent), &buf);
    if ( rc == 0 ) {
	*dev = buf.st_dev;
    } else {
	eprint("Error in init of partition %s:%s", 
	       Partent_host(partent), Partent_dir(partent));
	perror("");
	CODA_ASSERT(0);
    }
    
    return 0;
}


