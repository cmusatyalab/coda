
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
	exit(1);
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
	exit(1);
    }
    
    return rc;
}

    
    
