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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/resolution/parselog.h,v 4.2 1998/11/02 16:45:30 rvb Exp $";
#endif /*_BLURB_*/





#ifndef _PARSELOG_H_
#define _PARSELOG_H_ 	1
// compops.h
// 	class definition for remoteloglist
//		groups remote logentries  by vnode and unique
#include <olist.h>
#include <vcrcommon.h>

struct remoteloglist : public olink {
    VnodeId vnode;
    Unique_t unique;
    olist slelist;

    remoteloglist(VnodeId vn, Unique_t u) { 
	vnode = vn; unique = u; 
    }
	
    ~remoteloglist() {
	// CODA_ASSERT(slelist.count() == 0);
    }
};

// exported routines
extern void ParseRemoteLogs(char *, int, int, olist **, rsle **);
extern void DeallocateRemoteLogs(olist *);
extern remoteloglist *FindLogList(he *, VnodeId, Unique_t);
extern olist *FindRemoteLog(olist *, unsigned long , ViceFid *);

#endif _PARSELOG_H_
