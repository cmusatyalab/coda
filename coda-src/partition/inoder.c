#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/param.h>

#include <voltypes.h>
#include "inodeops.h"
#include "viceinode.h"
#include "partition.h"

#define MODE 00700

void dosubs(int *, int, int);
int mdirs(int);

int
main(int argc, char **argv)
{
    int level = 1, rc, dev;
    int depth, width;
    struct DiskPartition *dp;

    if ( argc < 4 ) {
	printf("Usage %s <vicetab> <dir> {icreate,iinc,idec,header,setheader} opts\n", argv[0]);
	exit(1);
    }
    
    InitPartitions(argv[1]);

    dp = VGetPartition(argv[2]);
    
    if ( !dp ) {
	printf("Error getting partition named %s. Check vicetab.\n", argv[2]);
	exit(1);
    }

    depth = dp->d->ftree.depth;
    width = dp->d->ftree.width;
    dev = dp->device;
    
    if ( strcmp(argv[3], "icreate") == 0 ){
	if ( argc == 8 ) {
	    u_long vol, vnode, uniq, vers;
	    Inode ino;

	    vol = atoi(argv[4]);
	    vnode = atoi(argv[5]);
	    uniq = atoi(argv[6]);
	    vers = atoi(argv[7]);
	    ino = icreate(dev, 0, vol, vnode, uniq, vers);
	    printf("Created inode %ld (error if <=0)\n", ino);
	    if ( ino > 0 ) 
		exit(0);
	    else 
		exit(1);
	} else {
	    printf("Usage %s <vicetab> <dir> icreate <vol> <vnode> <uniq> <vers>\n", argv[0]);
	    exit(1);
	}

    } else if ( strcmp(argv[3], "header") == 0 ) { 
	if ( argc == 5 ) {
	    Inode ino= atoi(argv[4]);
	    struct i_header header;
	    int rc;

	    rc = dp->ops->get_header(dp, &header, ino);
	    
	    if ( rc == 0 ) {
		printf("Header for inode %ld\n", ino);
		printf(" lnk   %ld\n", header.lnk);
		printf(" vol   0x%lx\n", header.volume);
		printf(" vnode 0x%lx\n", header.vnode);
		printf(" uniq  0x%lx\n", header.unique);
		printf(" vers  %ld\n", header.dataversion);
		printf(" magic %ld\n", header.magic);
		exit(0);
	    } else {
		printf("Error getting inode header %d\n", ino);
		exit(1);
	    }
	} else {
	    printf("Usage %s <vicetab> <dir> header <ino>\n", argv[0]);
	    exit(1);
	}

    } else if ( strcmp(argv[3], "iinc") == 0 ) { 
	if ( argc == 5 ) {
	    int ino = atoi(argv[4]);
	    iinc(dev, ino, 0);
	} else {
	    printf("Usage %s <vicetab> <dir> iinc <ino>\n", argv[0]);
	    exit(1);
	}

    } else if ( strcmp(argv[3], "idec") == 0 ) { 
	if ( argc == 5  ) {
	    int ino = atoi(argv[4]);
	    idec(dev, ino, 0);
	    exit(0);
	} else {
	    printf("Usage %s <vicetab> <dir> idec <ino>\n", argv[0]);
	    exit(1);
	}
	    
    } else if ( strcmp(argv[3], "setheader") == 0 ) { 
	if ( argc == 10 ) {
	    int magic = dp->ops->magic();
	    Inode ino = atoi(argv[4]);
	    int lnk = atoi(argv[5]);
	    int vol = atoi(argv[6]);
	    int vnode = atoi(argv[7]);
	    int unique = atoi(argv[8]);
	    int version = atoi(argv[9]);
	    struct i_header header={lnk, vol, vnode, unique, version, magic};
	    
	    rc = dp->ops->put_header(dp, &header, ino);
	    if ( rc != 0 ) {
		printf("Could not put header for ino %ld\n", ino);
		exit(1);
	    }
	    exit(0);

	} else {
	    printf("Usage %s <vicetab> <dir> setheader <ino> <lnk> <vol> <vnode> <uniq> <vers>\n", argv[0]);
	    exit(1);
	}
    } else {
	printf("Usage %s <vicetab> <dir> {icreate,iinc,idec,header,setheader} opts\n", argv[0]);
	exit(1);
    }

    return 0;
}

