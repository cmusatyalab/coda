#ifndef _BLURB_
#define _BLURB_
/*

            Coda: an Experimental Distributed File System
                             Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
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
#endif /*_BLURB_*/



#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/file.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "coda_flock.h"

#ifdef __cplusplus
}
#endif __cplusplus

int main(int argc, char **argv)
{
    if (argc < 2) {
	fprintf (stderr,"Usage: sqlunlock <filename>\n");
	exit (-1);
    }
    for (int i = 1; i<argc; i++) {
	char *file = argv[i];
	int fd = open(file,O_RDWR,0);
	if (fd < 0) {
	    fprintf (stderr, "Could not open file %s; error #%d\n",
		     file,errno);
	    break;
	}
	int locked = myflock(fd, MYFLOCK_SH, MYFLOCK_NB);
	if (!locked) {
	    myflock(fd, MYFLOCK_UN, MYFLOCK_BL);
	    fprintf (stderr, "File %s not locked\n",file);
	    break;
	}
	if (errno != EWOULDBLOCK) {
	    fprintf (stderr, "Lock test for file %s failed [%d]\n",
		     file,errno);
	    break;
	}
	if (myflock(fd,MYFLOCK_UN, MYFLOCK_BL)) {
	    fprintf (stderr, "Unlock of file %s failed [%d]\n",
		     file,errno);
	    break;
	}
    }
}
	
