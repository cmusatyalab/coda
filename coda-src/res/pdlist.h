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
 * pdlist.h  
 * Specification of memory independent doubly linked list type
 * Members of the list are allocated from a block of memory. 
 * The base address of this block and size of each element is stored in the 
 * list header.
 * The pointers are not addresses but indices into this block of memory.
 *
 */

#ifndef _UTIL_PDLIST_H_
#define _UTIL_PDLIST_H_ 1
#include "vcrcommon.h"

class pdlink;
class pdlist;
class PMemMgr;
class log;
class res_mgrpent;
class VNResLog;

class pdlist {
  public:
    short int head;	/* negative -> NIL */
    int cnt;		/* number of elements in list */
    int offset;		/* offset of pdlink in storage class */
    PMemMgr *storageMgr;/* class from which objects are allocated */

    pdlist(int, PMemMgr *);
    pdlist(int, PMemMgr *, int, int);
    ~pdlist();
    void prepend(pdlink *);
    void append(pdlink *);
    pdlink *remove(pdlink *);
    pdlink *get();		/* return and remove head of list */
    pdlink *first();
    pdlink *last();
    pdlink *prev(pdlink *);
    int count();
};
class pdlist_iterator {
    pdlist *cdlist;
    pdlink *cdlink;
  public:
    pdlist_iterator(pdlist&);
    pdlink *it_remove(pdlink *);	/* special remove function with iterator */
    pdlink *operator()();		/* return next object of class */
};
class pdlist_prev_iterator {
    pdlist *cdlist;
    pdlink *cdlink;
  public:
    pdlist_prev_iterator(pdlist&);
    pdlink *it_remove(pdlink *);	/* special remove function with iterator */
    pdlink *operator()();		/* return prev object of class */
};

class pdlink {
    friend	log *GetLogEntry(PMemMgr*);
    friend 	class pdlist;
    friend 	class pdlist_iterator;
    friend 	class pdlist_prev_iterator;
    int	next;	/* index of next element in list */
    int	prev;	/* index of previous element in list */
  public:
    pdlink();
    ~pdlink();
    
    void init();
    void ntoh();
    void hton();
};
    
    
#endif not _UTIL_PDLIST_H_ 
