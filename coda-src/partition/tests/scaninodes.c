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
    
    if ( argc != 2 ) {
	printf("Usage %s dir.\n", argv[1]);
	exit(1);
    }

    dp = VGetPartition(argv[1]);
    devno = dp->device;

    rc = dp->ops->ListCodaInodes(dp, "/tmp/inodeinfo", NULL, 0);
    
    return rc;
}

    
    
