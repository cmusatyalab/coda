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








#ifndef _INDEX_H_
#define _INDEX_H_ 1

class vindex {
  friend class vindex_iterator;
    Device  unix_dev;
    VolId   vol_id;	    /* unique volume id of object's volume */
    int	    vol_index;	    /* index of object's volume in recoverable storage */
    int	    vtype;	    /* vLarge (= 0) or vSmall (= 1) (from cvnode.h) */
    int	    camindex;
public:
    vindex(VolId volid = -1, int vnodetype = -1, Device dev = -1,
	   int size = -1, int volindex = -1);
    operator=(vindex&);
    ~vindex();
    int	    elts();
    int	    vnodes();
    int	    IsEmpty(VnodeId);
    int	    get(VnodeId, Unique_t, VnodeDiskObject*);
    int	    oget(bit32, Unique_t, VnodeDiskObject*);
    int	    put(VnodeId, Unique_t, VnodeDiskObject*);
    int	    oput(bit32, Unique_t, VnodeDiskObject*);
};

class vindex_iterator {
    vindex *v_ind;
    rec_smolist *vlists;
    int clist;
    int nlists;
    rec_smolist_iterator *nextlink;
  public:
    vindex_iterator(vindex&);
    ~vindex_iterator();
    int operator()(VnodeDiskObject *);
};
    
#endif _INDEX_H_
