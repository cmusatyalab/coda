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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/util/ohash.h,v 4.1 1997/01/08 21:51:05 rvb Exp $";
#endif /*_BLURB_*/









/*
 *
 * ohash.h -- Specification of hash-table type where each bucket is a singly-linked
 * list (an olist).
 *
 */

#ifndef _UTIL_HTAB_H_
#define _UTIL_HTAB_H_ 1


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include "olist.h"


class ohashtab;
class ohashtab_iterator;


class ohashtab {
  friend class ohashtab_iterator;
    int	sz;			    // size of the array
    olist *a;			    // array of olists
    int	(*hfn)(void *);		    // the hash function
    int cnt;
  public:
    ohashtab(int, int (*)(void *));
    ohashtab(ohashtab&);	    // not supported!
    operator=(ohashtab&);	    // not supported!
    virtual ~ohashtab();
    void insert(void *,	olink *);   // add at head of list
    void append(void *,	olink *);   // add at tail of list
    olink *remove(void *, olink	*); // remove specified entry
    olink *first();		    // return first element of table
    olink *last();		    // return last element of table
    olink *get(void *);		    // return and remove head of list
    void clear();		    // remove all entries
    int count();
    int IsMember(void *, olink *);
    int	bucket(void *);		    // returns bucket number of key
    virtual void print();
    virtual void print(FILE *);
    virtual void print(int);
};


class ohashtab_iterator {
    ohashtab *chashtab;		    // current ohashtab
    int	allbuckets;		    // iterate over all or single bucket
    int	cbucket;		    // current bucket
    olist_iterator *nextlink;	    // current olist iterator
  public:
    ohashtab_iterator(ohashtab&, void * =(void *)-1);
    ~ohashtab_iterator();
    olink *operator()();	    // return next object or 0
};

#endif	not _UTIL_HTAB_H_
