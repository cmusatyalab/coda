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
    int fd, count, i, rc;
    char *buff="This is a test string";

    InitPartitions("vicetab");
    
    if ( argc != 3 ) {
	printf("Usage %s dir count.\n", argv[0]);
	exit(1);
    }

    dp = VGetPartition(argv[1]);
    devno = dp->device;

    count = atoi(argv[2]);

    /*    printnames(dp, 1, 1, count); */
    for ( i=1 ; i <= count ; i++ ) {
	testcreate = icreate(devno, i, i+1, i+2, i+3, i+4);
	if ( testcreate == 0 )
	exit(1);
    }
    
    return 0;
}

    
    
