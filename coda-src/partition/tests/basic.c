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

#include <coda_string.h>
#include <util.h>
#include <partition.h>
#include <vicetab.h>
extern void  printnames(struct DiskPartition *dp, int low, int step, int high);

int
main(int argc, char **argv)
{
    struct DiskPartition *dp;
    Inode testcreate;
    Device devno;
    int fd, count;
    char *buff="This is a test string";

    InitPartitions("vicetab");
    dp = VGetPartition("simpled");
    devno = dp->device;

    testcreate = icreate(devno, 0, 0, 0, 0, 0);
    printf("icreate returned: %d\n", testcreate);
    if ( testcreate == 0 )
	exit(1);
    
    fd = iopen(devno, testcreate, O_RDONLY);
    printf("iopen returned: %d\n", fd);
    if ( fd != -1 ) 
	close(fd);
    else 
	exit(2);

    count = iwrite(devno, testcreate, 0, 0, buff, strlen(buff));
    printf("iwrite returned %d (of %d)\n", count, strlen(buff));

    printnames(VGetPartition("/tmp/f"), 0, 1, 64);
    dp = VGetPartition("/tmp/f");
    devno = dp->device;
    testcreate = icreate(devno, 0, 0, 0, 0, 0);
    printf("icreate returned: %d\n", testcreate);
    if ( testcreate == 0 )
	exit(1);
    
    fd = iopen(devno, testcreate, O_RDONLY);
    printf("iopen returned: %d\n", fd);
    if ( fd != -1 ) 
	close(fd);
    else 
	exit(2);

    count = iwrite(devno, testcreate, 0, 0, buff, strlen(buff));
    printf("iwrite returned %d (of %d)\n", count, strlen(buff));
    
    return 0;
}

    
    
