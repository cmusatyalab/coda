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
    int operator=(rec_ohashtab&);			/* not supported! */
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
