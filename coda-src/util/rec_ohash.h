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

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/coda-src/util/rec_ohash.h,v 1.1.1.1 1996/11/22 19:08:20 rvb Exp";
#endif /*_BLURB_*/









/*
 *
 *    rec_ohash.h -- Specification of hash-table type where each bucket is a recoverable
 *    singly-linked list (a rec_olist).
 *
 */

#ifndef _UTIL_REC_OHASH_H_
#define _UTIL_REC_OHASH_H_ 1

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include "ohash.h"
#include "rec_olist.h"
#include "rvmlib.h"


class rec_ohashtab;
class rec_ohashtab_iterator;
typedef int (*RHFN)(void *);


class rec_ohashtab {
  friend class rec_ohashtab_iterator;
    int	sz;						/* size of the array */
    rec_olist *a;					/* array of olists */
    RHFN hfn;						/* the hash function */
    int cnt;

  public:
    void *operator new(size_t);
    void operator delete(void *, size_t);

    rec_ohashtab(int, RHFN);
    rec_ohashtab(rec_ohashtab&);			// not supported! 
    void Init(int, RHFN);
    operator=(rec_ohashtab&);				/* not supported! */
    ~rec_ohashtab();
    void DeInit();
    void SetHFn(RHFN); 

    void insert(void *,	rec_olink *);			/* add at head of list */
    void append(void *,	rec_olink *);			/* add at tail of list */
    rec_olink *remove(void *, rec_olink	*);		/* remove specified entry */
    rec_olink *first();					/* return first element of table */
    rec_olink *last();					/* return last element of table */
    rec_olink *get(void	*);				/* return and remove head of list */

    int count();
    int IsMember(void *, rec_olink *);
    int	bucket(void *);					/* returns bucket number of key */
    /*virtual*/ void print();
    /*virtual*/ void print(FILE *);
    /*virtual*/ void print(int);
};


class rec_ohashtab_iterator {
    rec_ohashtab *chashtab;				/* current rec_ohashtab */
    int	allbuckets;					/* iterate over all or single bucket */
    int	cbucket;					/* current bucket */
    rec_olist_iterator *nextlink;			/* current olist iterator */

  public:
    rec_ohashtab_iterator(rec_ohashtab&, void * =(void *)-1);
    ~rec_ohashtab_iterator();
    void Reset();
    rec_olink *operator()();				/* return next object or 0 */
};

#endif	not _UTIL_REC_OHASH_H_
