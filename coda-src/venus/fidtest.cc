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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/venus/fidtest.cc,v 4.1 1997/01/08 21:51:22 rvb Exp $";
#endif /*_BLURB_*/




#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <errno.h>
#include <string.h>
#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#endif /* __MACH__ */
#ifdef __NetBSD__
#include <unistd.h>
#include <stdlib.h>
#endif __NetBSD__

#ifdef __cplusplus
}
#endif __cplusplus

#include "venusioctl.h"
#include "vice.h"

#define	VSG_MEMBERS 8

struct GetFid {
    ViceFid fid;
    ViceVersionVector vv;
};


void main(int argc, char **argv) {
    GetFid out;
    bzero(&out, sizeof(struct GetFid));

    struct ViceIoctl vi;
    vi.in = 0;
    vi.in_size = 0;
    vi.out = (char *)&out;
    vi.out_size = sizeof(struct GetFid);

    if (pioctl(argv[1], VIOC_GETFID, &vi, 0) != 0) {
	perror("pioctl:GETFID");
	exit(-1);
    }

    printf("FID = (%x.%x.%x)\n",
	    out.fid.Volume, out.fid.Vnode, out.fid.Unique);
    printf("\tVV = {[");
    for (int i = 0; i < VSG_MEMBERS; i++)
	printf(" %d", (&(out.vv.Versions.Site0))[i]);
    printf(" ] [ %#x %d ] [ %#x ]}\n",
	     out.vv.StoreId.Host, out.vv.StoreId.Uniquifier, out.vv.Flags);

    exit(0);
}
