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
    int operator=(dhashtab&);	    // not supported!
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
