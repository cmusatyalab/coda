/*
   Copyright 1997-98 Michael Callahan
   This program is free software.  You may copy it according
   to the conditions of the GNU General Public License version 2.0
   or later; see the file COPYING in the source directory.
*/

#include <unistd.h>
#include <stdio.h>
#include "coda_string.h"
#include <ctype.h>

#include "vxd.h"

struct far_ptr codadev_api = {0,0};

main(int argc, char *argv[])
{
    char mountstring[] = "Z\\\\CODA\\CLUSTER";
    int res;

    if ((argc != 2) || 
	(strlen(argv[1]) != 2) ||
	(argv[1][1] != ':')) {
	printf ("usage: %s <driveletter>:\n", argv[0]);
	exit (1);
    }

    mountstring[0] = toupper(argv[1][0]);

    printf ("Mounting on %c:\n", mountstring[0]);

    if (!open_vxd("CODADEV ", &codadev_api)) {
	printf ("CODADEV not there!\n");
	exit (1);
    }

    res = DeviceIoControl(&codadev_api, 2, mountstring, strlen(mountstring), NULL, 0);
    if (res) {
	printf ("Mount failed: %d\n", res);
	exit (1);
    }

    printf ("Mount OK\n");
}


