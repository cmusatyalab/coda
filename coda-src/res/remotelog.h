/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/






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


