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

static char *rcsid = "$Header: /usr/rvb/XX/src/coda-src/kerndep/RCS/pioctl.c,v 4.1 1997/01/08 21:50:58 rvb Exp $";
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


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <sys/file.h>
#include <sys/ioctl.h>

#ifdef __MACH__
#include <sys/viceioctl.h>
#endif /* __MACH__ */
#if defined(__linux__) || defined(__BSD44__)
#include "mach_vioctl.h"
#endif /* __linux__ || __BSD44__ */

#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif

        /* this needs to be sorted out XXXX */ 
#ifdef	__linux__
#define IOCPARM_MASK 0x0000ffff
#endif 

#ifdef __cplusplus
}
#endif __cplusplus

#define	CTL_FILE    "/coda/.CONTROL"


int pioctl(const char *path, unsigned long com, struct ViceIoctl *vidata, int follow) {
    /* Pack <path, vidata, follow> into a structure 
     * since ioctl takes only one data arg. 
     */
    struct {
	const char *path;
	struct ViceIoctl vidata;
	int follow;
    } data;
    int code, fd;

    /* Must change the size field of the command to match 
       that of the new structure. */
    unsigned long cmd = (com & ~(IOCPARM_MASK << 16)); /* mask out size field */
    int	size = ((com >> 16) & IOCPARM_MASK) + sizeof(char *) + sizeof(int);


    cmd	|= (size & IOCPARM_MASK) << 16;  /* or in corrected size */

    data.path = path;
    data.vidata = *vidata;
    data.follow = follow;

    fd = open(CTL_FILE, O_RDONLY, 0);
    if (fd < 0) return(fd);

    code = ioctl(fd, cmd, &data);

    /* Ignore close code. */
    (void)close(fd);

    /* Return result of ioctl. */
    return(code);
}
