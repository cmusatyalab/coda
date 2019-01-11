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
 * dlist.h -- Specification of  doubly-linked list type where list elements
 * can be on only one list at a time and are kept in sorted order.
 *
 */

#ifndef _UTIL_DLIST_H_
#define _UTIL_DLIST_H_ 1

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#ifdef __cplusplus
}
#endif

class dlink;
class dlist;
class dhashtab;
typedef int (*CFN)(dlink *, dlink *);

enum DlGetType
{
    DlGetMin,
    DlGetMax
};

class dlist {
    friend class dhashtab;
    dlink *head; // head of list
    int cnt;
    CFN CmpFn; // function to order the elements
public:
    dlist(); // default init with NULL compare func
    dlist(CFN);
    virtual ~dlist();
    void insert(dlink *); // insert in sorted order
    void prepend(dlink *); // add at beginning of list
    void append(dlink *); // add at end of list
    dlink *remove(dlink *); // remove specified entry
    dlink *first(); // return head of list
    dlink *last(); // return tail of list
    dlink *get(DlGetType = DlGetMin); // return and remove head or tail of list
    void clear(); // remove all entries
    int count();
    int IsMember(dlink *);
    virtual void print();
    virtual void print(FILE *);
    virtual void print(int);
};

enum DlIterOrder
{
    DlAscending,
    DlDescending
};

class dlist_iterator {
    dlist *cdlist; // current dlist
    dlink *cdlink; // current dlink
    DlIterOrder order; // iteration order
public:
    dlist_iterator(dlist &, DlIterOrder = DlAscending);
    dlink *operator()(); // return next object or 0.
        // Does *not* support safe deletion
        // of currently returned entry.  See the
        // comment below for more explanation.
};

class dlink { // objects are derived from this class
    friend class dlist;
    friend class dlist_iterator;
    dlink *next;
    dlink *prev;

public:
    dlink();
    void clear() { next = prev = NULL; };
    int is_linked() { return next != NULL; };
    virtual ~dlink();
    virtual void print();
    virtual void print(FILE *);
    virtual void print(int);
};

#endif /* _UTIL_DLIST_H_ */
