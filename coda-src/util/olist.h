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
 * olist.h -- Specification of singly-linked list type where list elements
 * can be on only one list at a time.
 *
 */

#ifndef _UTIL_LIST_H_
#define _UTIL_LIST_H_ 1

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#ifdef __cplusplus
}
#endif


class olist;
class olist_iterator;
class olink;

/* function to examine tag value in object derived from olink;
   returns 1 on match and 0 on mismatch (Satya, May04) */
typedef   int (*otagcompare_t)(void *tag, void *object);


class olist {
  friend class olist_iterator;
    olink *tail;	    // tail->next is head of list
    int cnt;
  public:
    olist();
    olist(olist&);	    // not supported!
    int operator= (olist&);	    // not supported!
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

    /* find object with matching tag in olist of objects derived from olink;
       return NULL if no such object;  cmpfn() knows field to examine in object */
    olink *FindObject(void *tag, otagcompare_t cmpfn);
};


class olist_iterator {
    olist *clist;	    // current olist
    olink *clink;	    // current olink; may be deleted before next iter.
    olink *nlink;	    // nlink save ahead the ptr to next olink in list
  public:
    olist_iterator(olist&);
    olink *operator()();    // return next object or 0
                            // Support safe deletion of currently
                            // returned entry.  See dlist.h also.
    void reset();           // allow re-use of iterator (Satya, 5/04)
};


class olink {		    // objects are derived from this class
  friend class olist;
  friend class olist_iterator;
    olink *next;
  public:
    olink();
    olink(olink&);	    // not supported!
    int operator=(olink&);	    // not supported!
    virtual ~olink();
    virtual void print();
    virtual void print(FILE *);
    virtual void print(int);

    /* Tag matching function for classes derviced from olink;
       Given pointer to a tag and a compare function, otagmatch() returns 0 or 1 
       Allows generalization of classes olist and ohash to test for
        presence of object with an arbitrary tag, rather than just 
          match on object pointer */
    int otagmatch (void *otag, otagcompare_t cmpfn);

};

#endif /* _UTIL_LIST_H_ */
