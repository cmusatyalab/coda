#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/param.h>

#include "vicetab.h"
#include "partition.h"

#define MODE 00700

void dosubs(int *, int, int);
int mdirs(int);

int
main(int argc, char **argv)
{
    int level = 1, rc;
    int depth, width;
    struct DiskPartition *dp;

    if ( argc != 3 ) {
	printf("Usage %s vicetab partitionpath\n", argv[0]);
	exit(1);
    }
    
    DP_Init(argv[1]);

    dp = DP_Get(argv[2]);
    
    if ( !dp ) {
	printf("Error getting partition named %s. Check vicetab.\n", argv[2]);
	exit(1);
    }

    rc = chdir(dp->name);
    depth = dp->d->ftree.depth;
    width = dp->d->ftree.width;


    if ( rc ) {
	perror("");
	exit(1);
    }

    dosubs(&level, width, depth);
    return 0;
}

void 
dosubs(int *level, int width, int depth)
{
    int d;
    char dir[256];
    (*level)++;
    mdirs(width);
    /* printf("Starting %d\n", *level); */
    /* only decend if not at top level */
    if (*level < depth) {
	for ( d = 0 ; d < width ; d++ ) {
	    sprintf(dir, "%x", d);
	    chdir(dir);
	    dosubs(level, width, depth);
	    chdir("..");
	}
    }
    /* printf("Ending %d\n", *level); */

    (*level)--;
}

int mdirs(int width)
{
    int d, rc;
    char dir[256];
    
    
    for ( d = 0; d < width; d++) {
	if ( sprintf(dir, "%x", d) <= 0 ) {
	    perror("");
	    exit(3);
	}
	rc = mkdir(dir, MODE);
	
	if (rc) {
	    perror("");
	    exit(3);
	}
    }
    return 0;
}

