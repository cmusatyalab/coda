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

static char *rcsid = "$Header: /usr/rvb/XX/src/coda-src/vol/RCS/testvrdb.cc,v 4.1 1997/01/08 21:52:17 rvb Exp $";
#endif /*_BLURB_*/





#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif
#include <struct.h>
#include <sys/file.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include "vrdb.h"

void PrintVRDB() {
    ohashtab_iterator next(VRDB.namehtb, (void *)-1);

    olink *l;
    vrent *v;
    while (l = next()) {
	v = strbase(vrent, l, namehtblink);
	v->print();
    }
}
void BuildVRDB() {
    char *infile = "/vice/vol/VRList";
    FILE *vrlist;
    vrlist = fopen(infile, "r");
    if (vrlist == NULL) {
	printf("MakeVRDB: unable to open file %s\n", infile);
	exit(-1);
    }
    int fd = open("/vice/db/VRDB", O_CREAT | O_TRUNC | O_WRONLY, 0644);
    vrent vre;
    char line[500];
    int lineno = 0;
    while (fgets(line, sizeof(line), vrlist) != NULL) {
	lineno++;
	int servercount;
	if (sscanf(line, "%32s %x %d %x %x %x %x %x %x %x %x %x",
		   vre.key, &vre.volnum, &servercount,
		   &vre.ServerVolnum[0], &vre.ServerVolnum[1],
		   &vre.ServerVolnum[2], &vre.ServerVolnum[3],
		   &vre.ServerVolnum[4], &vre.ServerVolnum[5],
		   &vre.ServerVolnum[6], &vre.ServerVolnum[7],
		   &vre.addr) != 12) {
	    printf("Bad input line(%d): %s\n", lineno, line);
	    printf("makevrdb aborted\n");
	    exit(-1);
	}
	vre.nServers = servercount;
	vre.hton();
	
	if (write(fd, &vre, sizeof(struct vrent)) != sizeof(struct vrent)) {
	    printf("write error on input line(%d): %s\n", lineno, line);
	    printf("makevrdb aborted\n");
	    exit(-1);
	}
    }
    close(fd);
}
main(int argc, char **argv) {
    char input;

    BuildVRDB();
    CheckVRDB();

    while (1) {
	char buf[80];
	printf("Option(v, n, i)? "); gets(buf);
	sscanf(buf, "%c", &input);
	switch (input) {
	  case 'v': {
	      long vnum;
	      printf("volume number? "); gets(buf);
	      sscanf(buf, "%x", &vnum);
	      printf("The volume number is 0x%x\n", vnum);
	      vrent *newvre = VRDB.find(vnum);
	      if (newvre) 
		  newvre->print();
	      else 
		  printf("Couldn't find the vrdb entry!\n");
	  }
	    break;
	  case 'n': {
	      char buf[80];
	      printf("Volume name? "); gets(buf);
	      vrent *newvre = VRDB.find(buf);
	      if (newvre) newvre->print();
	      else printf("vrent  not found \n");
	  }
	    break;
	  case 'i': {
	      printf("Rereading the data base from /vice/vol/VRList");
	      BuildVRDB();
	      CheckVRDB();
	  }
	    break;

	  default: printf("unknown option\n");
	}
    }
}
