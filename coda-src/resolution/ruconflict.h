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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/resolution/ruconflict.h,v 4.1 1997/01/08 21:50:40 rvb Exp $";
#endif /*_BLURB_*/





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
#endif _RUCONFLICT_H_ 
