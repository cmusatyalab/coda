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







/********************************
 * makevrdb.c			*
 * Jay Kistler	 	*
 ********************************/

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <sys/types.h>
#include <sys/file.h>
#include <stdlib.h>

#ifdef __cplusplus
}
#endif __cplusplus



/* This is cheating! */
#define VSG_MEMBERS 8
struct vrent {
    unsigned header : 32;
    unsigned nextptr : 32;
    char key[33];
    VolumeId volnum;
    /*byte*/unsigned char nServers;
    VolumeId ServerVolnum[VSG_MEMBERS];
    unsigned long addr;
};
#define VRDB_PATH "/vice/db/VRDB"
#define VRDB_TEMP "/vice/db/VRDB.new"


void main(int argc, char *argv[]) {
    int fd = open(VRDB_TEMP, O_TRUNC|O_WRONLY|O_CREAT, 0644);
    if (fd == -1) {
	printf("makevrdb:  Unable to create %s; aborted\n", VRDB_TEMP);
	exit(1);
    }

    struct vrent vre;
    char line[500];
    int lineno = 0;
    while (gets(line) != NULL) {
	lineno++;
	int servercount;
	if (sscanf(line, "%32s %u %d %u %u %u %u %u %u %u %u",
		   vre.key, &vre.volnum, &servercount,
		   &vre.ServerVolnum[0], &vre.ServerVolnum[1],
		   &vre.ServerVolnum[2], &vre.ServerVolnum[3],
		   &vre.ServerVolnum[4], &vre.ServerVolnum[5],
		   &vre.ServerVolnum[6], &vre.ServerVolnum[7]) != 11) {
	    printf("Bad input line(%d): %s\n", lineno, line);
	    printf("makevrdb aborted\n");
	    exit(1);
	}
	vre.nServers = servercount;
	vre.addr = 0xe0000009;

	if (write(fd, &vre, sizeof(struct vrent)) != sizeof(struct vrent)) {
	    printf("write error on input line(%d): %s\n", lineno, line);
	    printf("makevrdb aborted\n");
	    exit(1);
	}
    }

    close(fd);

    if (rename(VRDB_TEMP, VRDB_PATH) == -1) {
	printf("Unable to rename %s to %s; new vrdb not created\n",
	       VRDB_TEMP, VRDB_PATH);
    }
    else
	printf("VRDB created, %d entries\n", lineno);

    exit(0);
}
