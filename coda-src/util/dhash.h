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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/util/dhash.h,v 4.2 1997/09/04 22:14:02 braam Exp $";
#endif /*_BLURB_*/









/*
 *
 * dhash.h -- Specification of hash-table type where each bucket is a
 * doubly-linked list (a dlist).
 * 
 */

#ifndef _UTIL_DHTAB_H_
#define _UTIL_DHTAB_H_ 1


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include "dlist.h"

class dlink;
class dhashtab;
class dhashtab_iterator;


class dhashtab {
  friend class dhashtab_iterator;
    int	sz;			    // size of the array
    dlist *a;			    // array of dlists
    int	(*hfn)(void *);		    // the hash function
    int cnt;
  public:
    dhashtab(int, int (*)(void *), CFN);
    dhashtab(dhashtab&);	    // not supported!
    operator=(dhashtab&);	    // not supported!
    virtual ~dhashtab();
    void insert(void *,	dlink *);   // add in sorted order of list 
    void prepend(void *, dlink *);  // add at head of list 
    void append(void *,	dlink *);   // add at tail of list
    dlink *remove(void *, dlink	*); // remove specified entry
    dlink *first();		    // return first element of table
    dlink *last();		    // return last element of table
    dlink *get(void *, DlGetType =DlGetMin);	// return and remove head or tail of list
    void clear();		    // remove all entries
    int count();
    int IsMember(void *, dlink *);
    int	bucket(void *);		    // returns bucket number of key
    virtual void print();
    virtual void print(FILE *);
    virtual void print(int);
};


enum DhIterOrder { DhAscending, DhDescending };

class dhashtab_iterator {
    dhashtab *chashtab;		    // current dhashtab
    int	allbuckets;		    // iterate over all or single bucket
    int	cbucket;		    // current bucket
    dlist_iterator *nextlink;	    // current dlist iterator
    DhIterOrder	order;		    // iteration order
  public:
    dhashtab_iterator(dhashtab&, void * =(void *)-1);	    // iterates in ASCENDING order!
    dhashtab_iterator(dhashtab&, DhIterOrder, void * =(void *)-1);
    ~dhashtab_iterator();
    dlink *operator()();	    // return next object or 0
    
};

#endif	not _UTIL_HTAB_H_
