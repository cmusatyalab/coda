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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/kernel-src/inodefs/common/Attic/inodefs.h,v 4.5 1997/12/01 17:28:32 braam Exp $";
#endif /*_BLURB_*/

/* Interface definition for inode operations; the definition is 
   identical for both kernel and user-level implementations of
   these operations.  Also includes interface definitions for a 
   few Coda-specific system calls like pioctl() and setpag().

   This file gets installed in include, and used in the 
   compilation of kernel-src as well as coda-src.  I expect quite
   a few more definitions to end up here eventually.

   Created: Satya 1/24/97 from an old ifs.h file by Puneet Kumar
*/

#ifndef _INODEFS_H_
#define _INODEFS_H_

#ifdef KERNEL

/* Code to be added here for kernel compiles */

#else /* KERNEL */

#ifdef __MACH__
#include <sysent.h>	/* Mach defines these in sysent.h */
#else /* __MACH__ */
#ifdef __FreeBSD__
#include <coda.h>	/* for struct ViceIoctl */
#endif /* __FreeBSD__ */
extern int icreate __P((int, int, int, int, int, int));
extern int iopen   __P((int, int, int));
extern int iread   __P((int, int, long, unsigned int, char *, unsigned int));
extern int iwrite  __P((int, int, long, unsigned int, char *, unsigned int));
extern int iinc    __P((int, int, long));
extern int idec    __P((int, int, long));
extern int pioctl  __P((char *, int, struct ViceIoctl *, int));
#endif /* __MACH__ */

#endif /* _KERNEL */

#endif /* _INODEFS_H_ */
