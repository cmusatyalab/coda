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

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/coda-src/vol/testmount.c,v 1.2 1997/01/07 18:43:22 rvb Exp";
#endif /*_BLURB_*/

extern "C" {
#ifdef __MACH__
#include <libc.h>
#include <sysent.h>
#endif /* __MACH__ */
#ifdef __NetBSD__
#include <unistd.h>
#include <stdlib.h>
#endif __NetBSD__

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

}
int MountedAtRoot(char *path)
    /* Returns 1 if path is mounted at "/", 0 otherwise */
    {
    struct stat rootbuf, pathbuf;

    /* Check exactly one slash, and in first position */
    if (rindex(path, '/') != path) return(0);

    /* Then compare root and path device id's */
    if (stat("/", &rootbuf))
	{
	perror("/");
	return(0);
	}
    if (stat(path, &pathbuf))
	{
	perror(path);
	return(0);
	}

    if (rootbuf.st_dev == pathbuf.st_dev) return(0);
    else return(1);
    }

main(int argc, char *argv[])
    {
    if (argc != 2) 
	{
	printf("Usage: testmount <path>\n");
	exit (-1);
	}
    if (MountedAtRoot(argv[1])) printf("Mounted at root\n");
    else printf("Not mounted at root\n");
    }


