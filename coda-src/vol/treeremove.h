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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/vol/treeremove.h,v 4.1 1997/01/08 21:52:17 rvb Exp $";
#endif /*_BLURB_*/






#ifndef _VOL_TREEREMOVE_H_
#define _VOL_TREEREMOVE_H_ 1

#include <srv.h>
#include <olist.h>
#include <dlist.h>

class TreeRmBlk {
  public:
    ClientEntry *client;
    VolumeId VSGVnum;
    Volume *volptr;
    ViceStatus *status;
    ViceStoreId *storeid;
    dlist *vlist;
    int IsResolve;
    olist *hvlog;
    unsigned long srvrid;
    int	*blocks;

  TreeRmBlk(){ }
  init(ClientEntry *cl, VolumeId vnum, Volume *vptr, ViceStatus *st,
		  ViceStoreId *stid, dlist *vl, int resolve, olist *logtree, 
		  unsigned long svid, int *blks) {
      client = cl;
      VSGVnum = vnum;
      volptr = vptr;
      status = st;
      storeid = stid;
      vlist = vl;
      blocks = blks;
      *blocks = 0;
      IsResolve = resolve;
      if (IsResolve) {
	  hvlog = logtree;
	  srvrid = svid;
      }
      else {
	  hvlog = NULL;
	  srvrid = 0;
      }
  return(0); /* keep C++ happy */
  }
};

extern int PerformTreeRemoval(PDirEntry, void *);

#endif _VOL_TREEREMOVE_H_
