#ifndef _BLURB_
#define _BLURB_
/*

            Coda: an Experimental Distributed File System
                             Release 4.0

          Copyright (c) 1987-1996 Carnegie Mellon University
                         All Rights Reserved

Permission  to  use, copy, modify and distribute this software and its
documentation is hereby granted,  provided  that  both  the  copyright
notice  and  this  permission  notice  appear  in  all  copies  of the
software, derivative works or  modified  versions,  and  any  portions
thereof, and that both notices appear in supporting documentation, and
that credit is given to Carnegie Mellon University  in  all  documents
and publicity pertaining to direct or indirect use of this code or its
derivatives.

CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
ANY DERIVATIVE WORK.

Carnegie  Mellon  encourages  users  of  this  software  to return any
improvements or extensions that  they  make,  and  to  grant  Carnegie
Mellon the rights to redistribute these changes without encumbrance.
*/

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/vice/printvrdb.cc,v 4.3 1997/12/20 23:35:23 braam Exp $";
#endif /*_BLURB_*/







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
#if !defined(__GLIBC__)
#include <libc.h>
#include <sysent.h>
#endif

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
