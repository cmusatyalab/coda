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

static char *rcsid = "$Header: blurb.doc,v 1.1 96/11/22 13:29:31 raiff Exp $";
#endif /*_BLURB_*/







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
#include <libc.h>
#include <sysent.h>

#ifdef __cplusplus
}
#endif __cplusplus



/* This is cheating! */
#define VSG_MEMBERS 8
typedef u_long VolumeId;
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
