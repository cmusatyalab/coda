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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/util/rec_olist.h,v 4.1 1997/01/08 21:51:09 rvb Exp $";
#endif /*_BLURB_*/









/*
 *
 *    rec_olist.h -- Specification of a recoverable singly-linked list type
 *    where list elements can be on only one list at a time.
 *
 */

#ifndef _UTIL_REC_OLIST_H_
#define _UTIL_REC_OLIST_H_ 1

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>

#ifdef __cplusplus
}
#endif __cplusplus



#include "olist.h"
#include "rvmlib.h"


class rec_olist;
class rec_olist_iterator;
class rec_olink;


class rec_olist {
  friend class rec_olist_iterator;
    rec_olink *tail;				    /* tail->next is head of list */
    int cnt;

  public:
    void *operator new(size_t);
    void operator delete(void *, size_t);

    rec_olist();
    rec_olist(rec_olist&);			    /* not supported! */
    void Init();
    operator=(rec_olist&);			    /* not supported! */
    ~rec_olist();
    void DeInit();

    void insert(rec_olink *);			    /* add at head of list */
    void append(rec_olink *);			    /* add at tail of list */
    rec_olink *remove(rec_olink	*);		    /* remove specified entry */
    rec_olink *first();				    /* return head of list */
    rec_olink *last();				    /* return tail of list */
    rec_olink *get();				    /* return and remove head of list */

    int count();
    int IsMember(rec_olink *);
    /*virtual*/ void print();
    /*virtual*/ void print(FILE *);
    /*virtual*/ void print(int);
};


class rec_olist_iterator {
    rec_olist *clist;				    /* current rec_olist */
    rec_olink *clink;				    /* current rec_olink */

  public:
    rec_olist_iterator(rec_olist&);
    rec_olink *operator()();			    /* return next object or 0 */
};


class rec_olink	{				    /* objects are derived from this class */
  friend class rec_olist;
  friend class rec_olist_iterator;
    rec_olink *next;

  public:
    rec_olink();
    void Init();
    rec_olink(rec_olink&);			    /* not supported! */
    operator=(rec_olink&);			    /* not supported! */
/*
    ~rec_olink();
    void DeInit();
*/

    /*virtual*/ void print();
    /*virtual*/ void print(FILE *);
    /*virtual*/ void print(int);
};

#endif	not _UTIL_REC_OLIST_H_
