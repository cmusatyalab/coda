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






#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#ifdef __MACH__
#include <libc.h>
#endif __MACH__
#ifdef __NetBSD__
#include <stdlib.h>
#endif __NetBSD__
#include <dtcreg.h>

#ifdef __cplusplus
}
#endif __cplusplus

/* Hack program for use on IBM-RTs with DT2806 boards
   Allows use to unilaterally release clock if a program dies
	after gaining exclusive control to it.
   USE WITH CAUTION: releases clock even if checked out already
   You should only need this while debugging you program, if it
   exits before closing /dev/dtc0 */

main()
    {
    int dtcfd;

    /* Open the DT2806 board */
    dtcfd = open("/dev/dtc0",O_RDONLY,0);
    if (dtcfd < 0)
	{
	perror("/dev/dtc0");
	return(-1);
	}
    
    /* Call the dtc driver to return control of /dev/dtc0 to the kernel */
    DTCCTL(dtcfd);
    close(dtcfd);
    }
