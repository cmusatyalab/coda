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
