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





#ifndef _VVLIST_H_
#define _VVLIST_H_ 1
    
#include <vcrcommon.h>
#include <cvnode.h>
    
typedef struct vventry {
    int isThere;	/* We have seen an existing vnode for this entry */
    long unique;
    ViceStoreId StoreId;
    struct vventry *next;
} vvent;

#define ENDLARGEINDEX "End of the Large Vnode List.\n"
#define LISTLINESIZE 160
#define MAXLISTNAME 40

class vvtable {
    friend class vvent_iterator;
    vvent **vvlist;
    int nlists;
    
  public:
    vvtable(FILE *Ancient, VnodeClass vclass, int listsize);
    ~vvtable();
    int IsModified(int vnodeNumber, long unique, ViceStoreId *StoreId);
};


// Iterate through 1 list! Just one!
class vvent_iterator {
    vvent *cvvent;		// current olist
	
  public:
    vvent_iterator(vvtable&, int);
    vvent *operator()();	// return next object or 0
};

extern int  ValidListVVHeader(FILE *, register Volume *, int *);
extern void DumpListVVHeader(int, register Volume *vp, int Incremental, int);
extern void ListVV(int fd, int vnode, struct VnodeDiskObject *vnp);
extern void getlistfilename(char *, VolumeId, VolumeId, char *);

#endif  _VVLIST_H_
