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





#ifndef _RUCONFLICT_H_
#define _RUCONFLICT_H_ 1
#include <olist.h>
#include <dlist.h>
#include <vcrcommon.h>
#include <cvnode.h>
class rsle;


class RUParm {
  public:
    dlist *vlist;		// list of vnodes of all objects 
    olist *AllLogs;		// remote log grouped by host and object 
    unsigned long srvrid;  	// serverid where rm happened 
    unsigned long vid;		// volume id 
    int	rcode;			// return code: 0 -> no conflicts 
    
    RUParm(dlist *vl, olist *rmtlog, unsigned long id, unsigned long v) {
	vlist = vl;
	AllLogs = rmtlog;
	srvrid = id;
	vid = v;
	rcode = 0;
    }
};
extern int RUConflict(rsle *, dlist *, olist *, ViceFid *);
extern int FileRUConf(rsle *, Vnode *);
extern int FileRUConf(ViceVersionVector *, Vnode *);
int NewDirRUConf(RUParm *, char *, long , long );

#endif /* _RUCONFLICT_H_ */

