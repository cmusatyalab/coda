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






#ifndef _REMOTE_LOG_H_
#define _REMOTE_LOG_H_
/* 
 * remotelog.h
 *	This file describes the data structures used
 *	to covert a local log into one for a remote site
 */

#include <olist.h>
#include <vcrcommon.h>
#include <vice.h>
#include <cvnode.h>

#include "pdlist.h"
#include "reslog.h"


struct rmtle : public olink {
    VnodeId	dvnode;
    Unique_t 	dunique;
    int		local;
    union {
	struct {
	    pdlist *log;
	} local;
	struct {
	    int nentries;
	    rlent *log;
	} remote;
    }u ;

    rmtle(VnodeId vn, Unique_t unq, pdlist *llog) {
	dvnode = vn;
	dunique = unq;
	local = 1;
	u.local.log = llog;
    }

    rmtle(VnodeId vn, Unique_t unq) {
	dvnode = vn;
	dunique = unq;
	local = 1;
	u.local.log = 0;
    }
    rmtle(VnodeId vn, Unique_t unq, int n, rlent *rlog) {
	dvnode = vn;
	dunique = unq;
	local = 0;
	u.remote.nentries = n;
	u.remote.log = rlog;
    }
    ~rmtle() {

    }
};

/* 
 * class he:
 * Elements of a linked list.
 * One element / host in the log buffer.
 * Each element has a list of vnode-res log headers(rmtle)
 */

struct he : public olink {
    olist vlist;
    long    hid;
    
    he(long id) {
	hid = id;
    }
};

extern rmtle *FindRMTLE(olist *, VnodeId , Unique_t);
extern he *FindHE(olist *, long);
extern rmtle *AddRMTLE(olist *, VnodeId , Unique_t);
extern rmtle *AddLocalRMTLE(olist *, VnodeId , Unique_t , pdlist *);
extern rmtle *AddRemoteRMTLE(olist *, VnodeId , Unique_t , int , rlent *);
extern char *FlattenLocalRMTLElist(olist *, int *);
extern olist *BuildRemoteRMTLElist(char *, int );
extern void GetRmSubTreeLocalRMTLE(int , VnodeId , Unique_t , olist *, pdlist *);
extern void GetRmSubTreeLocalRMTLE(int, VnodeId, Unique_t, olist *, int, PMemMgr *,
				   int, int);
extern void BuildRemoteResLogIndexByHost(rlent *, int, olist *);
extern void PurgeRemoteResLogIndexByHost(olist *);
extern void PurgeLocalRMTLElist(olist *);
#endif _REMOTE_LOG_H_


