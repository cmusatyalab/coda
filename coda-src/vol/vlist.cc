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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/vol/vlist.cc,v 4.1 1997/01/08 21:52:19 rvb Exp $";
#endif /*_BLURB_*/






#ifdef __cplusplus
extern "C" {
#endif __cplusplus
#include <sys/types.h>
#include <stdio.h>
#include <codadir.h>
#include <util.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <srv.h>
#include "vlist.h"

int VLECmp(vle *a, vle *b) 
{
    assert(a->fid.Volume == b->fid.Volume);

    if (a->fid.Vnode == b->fid.Vnode && a->fid.Unique == b->fid.Unique) 
	    return(0);
    if (((unsigned long)a->fid.Vnode) < ((unsigned long)b->fid.Vnode) ||
	 (a->fid.Vnode == b->fid.Vnode && 
	    ((unsigned long)a->fid.Unique) < ((unsigned long)b->fid.Unique))) 
	 return(-1);
    return(1);
}


vle *FindVLE(dlist& dl, ViceFid *fid) 
{
    dlist_iterator next(dl);
    vle *v;
    while (v = (vle *)next())
	if (FID_EQ(&v->fid, fid)) return(v);
    return(0);
}


vle *AddVLE(dlist& dl, ViceFid *fid) 
{
    vle *v = FindVLE(dl, fid);
    if (v == 0)
	dl.insert((v = new vle(fid)));
    return(v);
}
