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

static char *rcsid = "$Header: /home/clement/ah/ss/coda-src/util/RCS/olist.h,v 4.1 1997/01/08 21:51:06 rvb Exp clement $";
#endif /*_BLURB_*/









/*
 *
 * olist.h -- Specification of singly-linked list type where list elements
 * can be on only one list at a time.
 *
 */

#ifndef _UTIL_LIST_H_
#define _UTIL_LIST_H_ 1

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>

#ifdef __cplusplus
}
#endif __cplusplus


class olist;
class olist_iterator;
class olink;


class olist {
  friend class olist_iterator;
    olink *tail;	    // tail->next is head of list
    int cnt;
  public:
    olist();
    olist(olist&);	    // not supported!
    operator=(olist&);	    // not supported!
    virtual ~olist();
    void insert(olink *);   // add at head of list
    void append(olink *);   // add at tail of list
    olink *remove(olink	*); // remove specified entry
    olink *first();	    // return head of list
    olink *last();	    // return tail of list
    olink *get();	    // return and remove head of list
    void clear();	    // remove all entries
    int count();
    int IsMember(olink *);
    virtual void print();
    virtual void print(FILE *);
    virtual void print(int);
};


class olist_iterator {
    olist *clist;	    // current olist
    olink *clink;	    // current olink; may be deleted before next iter.
    olink *nlink;	    // nlink save ahead the ptr to next olink in list
  public:
    olist_iterator(olist&);
    olink *operator()();    // return next object or 0
};


class olink {		    // objects are derived from this class
  friend class olist;
  friend class olist_iterator;
    olink *next;
  public:
    olink();
    olink(olink&);	    // not supported!
    operator=(olink&);	    // not supported!
    virtual ~olink();
    virtual void print();
    virtual void print(FILE *);
    virtual void print(int);
};

#endif	not _UTIL_LIST_H_
