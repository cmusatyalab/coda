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









/*
 *
 * rec_dlist.h -- Specification of  doubly-linked list type where list elements
 * can be on only one list at a time and are kept in sorted order.
 *
 */

#ifndef _UTIL_REC_DLIST_H_
#define _UTIL_REC_DLIST_H_ 1

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>

#ifdef __cplusplus
}
#endif __cplusplus



#include "dlist.h"
#include "rvmlib.h"

class rec_dlink;
class rec_dlist;
class rec_dhashtab;
typedef int (*RCFN)(rec_dlink *, rec_dlink *);

/*enum DlGetType { DlGetMin, DlGetMax };*/

class rec_dlist {
    friend class rec_dhashtab;
    rec_dlink *head;	    /* head of list */
    int cnt;
    RCFN CmpFn;		    /* function to order the elements  */
	
  public:
    void *operator new (size_t);
    void operator delete(void *, size_t);

    rec_dlist(RCFN =0);
    ~rec_dlist();

    void Init(RCFN);
    void SetCmpFn(RCFN);
    void DeInit();

    void insert(rec_dlink *);				/* insert in sorted order */
    void prepend(rec_dlink *);				/* add at beginning of list */
    void append(rec_dlink *);				/* add at end of list */ 
    rec_dlink *remove(rec_dlink	*);			/* remove specified entry */
    rec_dlink *first();	    				/* return head of list */
    rec_dlink *last();	    				/* return tail of list */
    rec_dlink *get(DlGetType =DlGetMin);		/* return and remove head or tail of list */
    int count();
    int IsMember(rec_dlink *);
    /*virtual*/ void print();
    /*virtual*/ void print(FILE *);
    /*virtual*/ void print(int);
};

/*enum DlIterOrder { DlAscending, DlDescending };*/

class rec_dlist_iterator {
    rec_dlist *cdlist;	    // current dlist
    rec_dlink *cdlink;	    // current dlink
    DlIterOrder	order;	    // iteration order
  public:
    rec_dlist_iterator(rec_dlist&, DlIterOrder =DlAscending);
    rec_dlink *operator()();    // return next object or 0
};

class rec_dlink {		    /* objects are derived from this class */
  friend class rec_dlist;
  friend class rec_dlist_iterator;
    rec_dlink *next;
    rec_dlink *prev;
  public:
    rec_dlink();
    void Init();
/*
    ~rec_dlink();
*/
    /*virtual*/ void print();
    /*virtual*/ void print(FILE *);
    /*virtual*/ void print(int);
};

#endif	not _UTIL_REC_DLIST_H_
