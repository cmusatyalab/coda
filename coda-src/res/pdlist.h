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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/res/Attic/pdlist.h,v 4.1 1997/01/08 21:49:58 rvb Exp $";
#endif /*_BLURB_*/







/*
 * pdlist.h  
 * Specification of memory independent doubly linked list type
 * Members of the list are allocated from a block of memory. 
 * The base address of this block and size of each element is stored in the 
 * list header.
 * The pointers are not addresses but indices into this block of memory.
 *
 */

#ifndef _UTIL_PDLIST_H_
#define _UTIL_PDLIST_H_ 1
#include "vcrcommon.h"

class pdlink;
class pdlist;
class PMemMgr;
class log;
class res_mgrpent;
class VNResLog;

class pdlist {
  public:
    short int head;	/* negative -> NIL */
    int cnt;		/* number of elements in list */
    int offset;		/* offset of pdlink in storage class */
    PMemMgr *storageMgr;/* class from which objects are allocated */

    pdlist(int, PMemMgr *);
    pdlist(int, PMemMgr *, int, int);
    ~pdlist();
    void prepend(pdlink *);
    void append(pdlink *);
    pdlink *remove(pdlink *);
    pdlink *get();		/* return and remove head of list */
    pdlink *first();
    pdlink *last();
    pdlink *prev(pdlink *);
    int count();
};
class pdlist_iterator {
    pdlist *cdlist;
    pdlink *cdlink;
  public:
    pdlist_iterator(pdlist&);
    pdlink *it_remove(pdlink *);	/* special remove function with iterator */
    pdlink *operator()();		/* return next object of class */
};
class pdlist_prev_iterator {
    pdlist *cdlist;
    pdlink *cdlink;
  public:
    pdlist_prev_iterator(pdlist&);
    pdlink *it_remove(pdlink *);	/* special remove function with iterator */
    pdlink *operator()();		/* return prev object of class */
};

class pdlink {
    friend	log *GetLogEntry(PMemMgr*);
    friend 	class pdlist;
    friend 	class pdlist_iterator;
    friend 	class pdlist_prev_iterator;
    int	next;	/* index of next element in list */
    int	prev;	/* index of previous element in list */
  public:
    pdlink();
    ~pdlink();
    
    void init();
    void ntoh();
    void hton();
};
    
    
#endif not _UTIL_PDLIST_H_ 
