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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/norton/dummy.cc,v 4.1 1997/01/08 21:49:49 rvb Exp $";
#endif /*_BLURB_*/




#ifdef __cplusplus
extern "C" {
#endif __cplusplus
#include <stdio.h>
#if !defined(__GLIBC__)
#include <libc.h>
#endif
#include <assert.h>
#ifdef __cplusplus
}
#endif __cplusplus

#include <cvnode.h>
#include <vlist.h>

extern vle *FindVLE(dlist& dl, ViceFid *fid);

int AllowResolution = 1;
int nodumpvm = 0;
int large = 500;
int small = 500;

vv_t NullVV = {{0, 0, 0, 0, 0, 0, 0, 0}, {0, 0}, 0};

PollAndYield() {
    dlist dl;
    ViceFid fid;

    assert(0);
    FindVLE(dl, &fid);
    return(0);
}

void Die(char *msg) {
    fprintf(stderr, "%s\n", msg);
    assert(0);
    return;
}

int GetFsObj(ViceFid *fid, Volume **volptr, Vnode **vptr,
	     int lock, int VolumeLock, int ignoreIncon, int ignoreBQ) {
    assert(0);
    return(0);
}

    

