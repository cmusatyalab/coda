#ifndef _BLURB_
#define _BLURB_
/*

            Coda: an Experimental Distributed File System
                             Release 3.1

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

static char *rcsid = "$Header: /afs/cs.cmu.edu/project/coda-braam/src/coda-4.0.1/RCSLINK/./kernel-src/vfs/mach/cfsnc_print.c,v 1.1 1996/11/22 19:16:16 braam Exp $";
#endif /*_BLURB_*/


/* 
 * Mach Operating System
 * Copyright (c) 1990 Carnegie-Mellon University
 * Copyright (c) 1989 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */

/*
 * This code was written for the Coda file system at Carnegie Mellon University.
 * Contributers include David Steere, James Kistler, and M. Satyanarayanan.
 */

/*
 * HISTORY
 * cfsnc_print.c,v
 * Revision 1.2  1996/01/02 16:57:20  bnoble
 * Added support for Coda MiniCache and raw inode calls (final commit)
 *
 * Revision 1.1.2.1  1995/12/20 01:57:46  bnoble
 * Added CFS-specific files
 *
 * Revision 3.1.1.1  1995/03/04  19:08:10  bnoble
 * Branch for NetBSD port revisions
 *
 * Revision 3.1  1995/03/04  19:08:09  bnoble
 * Bump to major revision 3 to prepare for NetBSD port
 *
 * Revision 2.1  1994/07/21  16:25:28  satya
 * Conversion to C++ 3.0; start of Coda Release 2.0
 *
 * Revision 1.2  92/10/27  17:58:35  lily
 * merge kernel/latest and alpha/src/cfs
 * 
 * Revision 2.2  90/07/05  11:27:08  mrt
 * 	Created for Coda file system
 * 	[90/07/05  10:15:18  mrt]
 * 
 * Revision 1.2  90/03/19  16:35:16  dcs
 * Changed Revision Log.
 * 
 */

/*
 * This file sends an ioctl to the cfs device driver to ask it to print
 * the cache table (by the hash table) to /dev/console.
 */

#include <sys/ioctl.h>
#include <cfs/cfsio.h>

main()
{
  int fd, err;
	
  fd = open("/dev/cfs1",0,0);
  if (fd < 0) {
    perror("Open /dev/cfs1");
    exit(-1);
  }

  err = ioctl(fd, CFSPRINT, 0);
  if (err < 0) {
    perror("Ioctl /dev/cfs1");
    exit(-1);
  }

  close(fd);
}
