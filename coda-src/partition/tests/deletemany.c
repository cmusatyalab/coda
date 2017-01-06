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


#include <util.h>
#include <../partition.h>
#include <../vicetab.h>
extern void  printnames(struct DiskPartition *dp, int low, int step, int high);

int
main(int argc, char **argv)
{
    struct DiskPartition *dp;
    Inode testcreate;
    Device devno;
    int fd, first, last, i, rc;
    char *buff="This is a test string";

    InitPartitions("vicetab");
    
    if ( argc != 4 ) {
	printf("Usage %s dir first last.\n", argv[0]);
	exit(EXIT_FAILURE);
    }

    dp = VGetPartition(argv[1]);
    devno = dp->device;

    first = atoi(argv[2]);
    last =  atoi(argv[3]);
    printf("Deleting inodes %d - %d in %s...", first, last, argv[1]);

    for ( i=first ; i <= last ; i++ ) {
	testcreate = idec(devno, i, 0);
	printf("idec returning %d for inode %ld\n", testcreate, i);
	if ( testcreate != 0 )
	exit(EXIT_FAILURE);
    }
    
    return rc;
}

    
    
