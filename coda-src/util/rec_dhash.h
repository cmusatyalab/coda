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
 * dhash.h -- Specification of recoverable hash-table type where each bucket 
 * is a doubly-linked list (a dlist).
 *
 */

#ifndef _UTIL_REC_DHTAB_H_
#define _UTIL_REC_DHTAB_H_ 1

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>

#ifdef __cplusplus
}
#endif __cplusplus


#include "dhash.h"
#include "rec_dlist.h"
#include "rvmlib.h"

class rec_dlink;
class rec_dhashtab;
class rec_dhashtab_iterator;
typedef int (*RHFN)(void *);


class rec_dhashtab {
  friend class rec_dhashtab_iterator;
    int	sz;			    // size of the array
    rec_dlist *a;		    // array of dlists
    RHFN hfn;			    // the hash function
    int cnt;

  public:
    void *operator new (size_t);
    void operator delete(void *, size_t);

    rec_dhashtab(int, RHFN, RCFN =0);
    rec_dhashtab(rec_dhashtab&);    // not supported!
    void Init(int, RHFN, RCFN); 
    operator=(rec_dhashtab&);	    // not supported!
    ~rec_dhashtab();
    void DeInit();
    void SetHFn(RHFN); 
    void SetCmpFn(RCFN);
    void insert(void *,	rec_dlink *);			/* add in sorted order */ 
    void prepend(void *, rec_dlink *);			/* add at head of list */
    void append(void *,	rec_dlink *);			/* add at tail of list */
    rec_dlink *remove(void *, rec_dlink	*);		/* remove specified entry */
    rec_dlink *first();		    			/* return first element of table */
    rec_dlink *last();		    			/* return last element of table */
    rec_dlink *get(void *, DlGetType =DlGetMin); // return and remove head or tail of list

    int count();
    int IsMember(void *, rec_dlink *);
    int	bucket(void *);		    // returns bucket number of key
    /*virtual*/ void print();
    /*virtual*/ void print(FILE *);
    /*virtual*/ void print(int);
};


/*enum DhIterOrder { DhAscending, DhDescending };*/

class rec_dhashtab_iterator {
    rec_dhashtab *chashtab;		// current dhashtab
    int	allbuckets;		    	// iterate over all or single bucket
    int	cbucket;		    	// current bucket
    rec_dlist_iterator *nextlink;	// current dlist iterator
    DhIterOrder	order;		    	// iteration order
  public:
    rec_dhashtab_iterator(rec_dhashtab&, DhIterOrder =DhAscending, void * =(void *)-1);
    ~rec_dhashtab_iterator();
    rec_dlink *operator()();	    // return next object or 0
    
};

#endif	not _UTIL_REC_DHTAB_H_
