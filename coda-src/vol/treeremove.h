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
