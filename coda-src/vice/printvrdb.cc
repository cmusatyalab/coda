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
 * printvrdb.c			*
 * Jay Kistler	 	*
 ********************************/

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/file.h>

#ifdef __cplusplus
}
#endif __cplusplus
#include <voltypes.h>


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


int main(int argc, char *argv[]) {
    int fd = open(VRDB_PATH, O_RDONLY, 0);
    if (fd < 0) {
	printf("printvrdb:  Unable to open %s; aborted\n", VRDB_PATH);
	exit(1);
    }

    struct vrent vre;
    while (read(fd, &vre, sizeof(struct vrent)) == sizeof(struct vrent)) {
	printf("%32s %u %d %x %x %x %x %x %x %x %x %x\n",
	       vre.key, ntohl(vre.volnum), vre.nServers,
	       ntohl(vre.ServerVolnum[0]), ntohl(vre.ServerVolnum[1]),
	       ntohl(vre.ServerVolnum[2]), ntohl(vre.ServerVolnum[3]),
	       ntohl(vre.ServerVolnum[4]), ntohl(vre.ServerVolnum[5]),
	       ntohl(vre.ServerVolnum[6]), ntohl(vre.ServerVolnum[7]),
	       ntohl(vre.addr));
    }

    close(fd);

    exit(0);
}
