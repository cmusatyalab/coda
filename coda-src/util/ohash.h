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
