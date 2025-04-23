/* BLURB gpl

                           Coda File System
                              Release 8

          Copyright (c) 1987-2025 Carnegie Mellon University
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
 * rec_dlist.h -- Specification of  doubly-linked list type where list elements
 * can be on only one list at a time and are kept in sorted order.
 *
 */

#ifndef _UTIL_REC_DLIST_H_
#define _UTIL_REC_DLIST_H_ 1

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#ifdef __cplusplus
}
#endif

#include "dlist.h"
#include "rvmlib.h"

class rec_dlink;
class rec_dlist;
class rec_dhashtab;
typedef int (*RCFN)(rec_dlink *, rec_dlink *);

/*enum DlGetType { DlGetMin, DlGetMax };*/

class rec_dlist {
    friend class rec_dhashtab;
    rec_dlink *head; /* head of list */
    int cnt;
    RCFN CmpFn; /* function to order the elements  */

public:
    void *operator new(size_t) REQUIRES_TRANSACTION;
    void operator delete(void *) REQUIRES_TRANSACTION;

    rec_dlist(RCFN = 0);
    ~rec_dlist();

    void Init(RCFN) REQUIRES_TRANSACTION;
    void SetCmpFn(RCFN);
    void DeInit();

    void insert(rec_dlink *) REQUIRES_TRANSACTION; /* insert in sorted order */
    void
    prepend(rec_dlink *) REQUIRES_TRANSACTION; /* add at beginning of list */
    void append(rec_dlink *) REQUIRES_TRANSACTION; /* add at end of list */
    rec_dlink *
    remove(rec_dlink *) REQUIRES_TRANSACTION; /* remove specified entry */
    rec_dlink *first(); /* return head of list */
    rec_dlink *last(); /* return tail of list */
    rec_dlink *get(DlGetType = DlGetMin)
        REQUIRES_TRANSACTION; /* return and remove head or tail of list */
    int count();
    int IsMember(rec_dlink *);
    /*virtual*/ void print();
    /*virtual*/ void print(FILE *);
    /*virtual*/ void print(int);
};

/*enum DlIterOrder { DlAscending, DlDescending };*/

class rec_dlist_iterator {
    rec_dlist *cdlist; // current dlist
    rec_dlink *cdlink; // current dlink
    DlIterOrder order; // iteration order
public:
    rec_dlist_iterator(rec_dlist &, DlIterOrder = DlAscending);
    rec_dlink *operator()(); // return next object or 0
    // Does *not* support safe deletion
    // of currently returned entry.  See the
    // comment in dlist.h for more explanation.
};

class rec_dlink { /* objects are derived from this class */
    friend class rec_dlist;
    friend class rec_dlist_iterator;
    rec_dlink *next;
    rec_dlink *prev;

public:
    rec_dlink();
    void Init() REQUIRES_TRANSACTION;
    /* ~rec_dlink(); */
    /*virtual*/ void print();
    /*virtual*/ void print(FILE *);
    /*virtual*/ void print(int);
};

#endif /* _UTIL_REC_DLIST_H_ */
