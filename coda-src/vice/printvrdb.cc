/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2016 Carnegie Mellon University
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
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/file.h>
#include <sys/param.h>
#include <unistd.h>
#include <fcntl.h>

#ifdef __cplusplus
}
#endif

#include <voltypes.h>

#include <codaconf.h>
#include <vice_file.h>
#include <vcrcommon.h>

/* This is cheating! */
struct vrent {
    unsigned header : 32;
    unsigned nextptr : 32;
    char key[33];
    VolumeId volnum;
    /*byte*/ unsigned char nServers;
    VolumeId ServerVolnum[VSG_MEMBERS];
    unsigned long addr;
};

#define VRDB_PATH vice_config_path("db/VRDB")
#define VRDB_TEMP vice_config_path("db/VRDB.new")

void ReadConfigFile()
{
    const char *vicedir;

    /* Load configuration file to get vice dir. */
    codaconf_init("server.conf");
    vicedir = codaconf_lookup("vicedir", "/vice");
    vice_dir_init(vicedir);
}

int main(int argc, char *argv[])
{
    int fd;

    ReadConfigFile();

    fd = open(VRDB_PATH, O_RDONLY, 0);
    if (fd < 0) {
        printf("printvrdb:  Unable to open %s; aborted\n", VRDB_PATH);
        exit(EXIT_FAILURE);
    }

    struct vrent vre;
    while (read(fd, &vre, sizeof(struct vrent)) == sizeof(struct vrent)) {
        printf("%32s %u %d %x %x %x %x %x %x %x %x %x\n", vre.key,
               (int)ntohl(vre.volnum), vre.nServers,
               (int)ntohl(vre.ServerVolnum[0]), (int)ntohl(vre.ServerVolnum[1]),
               (int)ntohl(vre.ServerVolnum[2]), (int)ntohl(vre.ServerVolnum[3]),
               (int)ntohl(vre.ServerVolnum[4]), (int)ntohl(vre.ServerVolnum[5]),
               (int)ntohl(vre.ServerVolnum[6]), (int)ntohl(vre.ServerVolnum[7]),
               (int)ntohl(vre.addr));
    }

    close(fd);

    exit(EXIT_SUCCESS);
}
