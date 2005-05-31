/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
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
 *    rec_olist.h -- Specification of a recoverable singly-linked list type
 *    where list elements can be on only one list at a time.
 *
 */

#ifndef _UTIL_REC_OLIST_H_
#define _UTIL_REC_OLIST_H_ 1

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#ifdef __cplusplus
}
#endif

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
    int operator=(rec_olist&);			    /* not supported! */
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

  public:
    rec_olink *clink;				    /* current rec_olink */
    rec_olist_iterator(rec_olist&);
    rec_olink *operator()();                        /* return next object or 0 */
                                                    /* Support safe deletion 
                                                     * of currently returned 
                                                     * entry. See dlist.h also */
};


class rec_olink	{				    /* objects are derived from this class */
  friend class rec_olist;
  friend class rec_olist_iterator;
    rec_olink *next;

  public:
    rec_olink();
    void Init();
    rec_olink(rec_olink&);			    /* not supported! */
    int operator=(rec_olink&);			    /* not supported! */
/*
    ~rec_olink();
    void DeInit();
*/

    /*virtual*/ void print();
    /*virtual*/ void print(FILE *);
    /*virtual*/ void print(int);
};

#endif /* _UTIL_REC_OLIST_H_ */
