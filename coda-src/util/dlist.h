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

enum DlGetType { DlGetMin, DlGetMax };

class dlist {
  friend class dhashtab;
    dlink *head;	    // head of list 
    int cnt;
    CFN	CmpFn;		    // function to order the elements 
  public:
    dlist();		    // default init with NULL compare func
    dlist(CFN);
    virtual ~dlist();
    void insert(dlink *);   // insert in sorted order 
    void prepend(dlink *);  // add at beginning of list 
    void append(dlink *);   // add at end of list 
    dlink *remove(dlink	*); // remove specified entry
    dlink *first();	    // return head of list
    dlink *last();	    // return tail of list
    dlink *get(DlGetType =DlGetMin);	// return and remove head or tail of list
    void clear();	    // remove all entries
    int count();
    int IsMember(dlink *);
    virtual void print();
    virtual void print(FILE *);
    virtual void print(int);
};

enum DlIterOrder { DlAscending, DlDescending };

class dlist_iterator {
    dlist *cdlist;	    // current dlist
    dlink *cdlink;	    // current dlink
    DlIterOrder	order;	    // iteration order
  public:
    dlist_iterator(dlist&, DlIterOrder =DlAscending);
    dlink *operator()();    // return next object or 0.
                            // Does *not* support safe deletion
                            // of currently returned entry.  See the 
                            // comment below for more explanation.
};

// Safe deletion of currently returned entry by list_iterators:
// ============================================================
// 
// There are totally six different list_iterators in coda-src/util, and
// they have two different behaviours with respect to the deletion of the
// current entry returned by the () operator.  (The following discussion
// applies only to the cases when there are no concurrent threads, or
// when the current will never yield explictly or implictly.  See the 
// note about a pitfall of concurrent threads in the next few 
// paragraphs.)
// 
// Three of them support safe deletion of the current entry:
// 
//   olist_iterator
//   rec_olist_iterator
//   rec_smolist_iterator
// 
// But the other three does not support the safe deletion behaviour:
// 
//   dlist_iterator
//   rec_dlist_iterator
//   arrlist_iterator   /* actually not applicable, since the list
// 			 * is malloc'ed and is an array */
// 
// "Safe deletion of the current entry returned by the () operator"
// means something like the following should work (assuming no 
// concurrent threads):
// 
//   class foo {
// 	 somelink handle;
// 	 /* ... */
//   };
// 
//   somelist_iterator next(list);
//   somelink *d;
//   class foo *f;
//   
//   while (d = next()) {
// 	 /* get the base of the structure linked up thru' handle */
// 	 f = strbase(foo, d, handle);
// 	 
// 	 /* ... */
// 	 
// 	 delete f;  /* will the subsequent next() work ? */
//   }
// 
// The above works only if somelist_iterator internally remembers
// the address of the next object in the list; otherwise, after the
// current object f is deleted, the reference to the next object is lost.
// 
// Some Coda programmers get around the fact that some list_iterators do
// not support the safe deletion behaviour, and they would use the
// following code instead:
// 
//   somelink *d = next();
// 
//   while (d) {
// 	 f = strbase(foo, d, handle);
// 	 d = next();  /* get the next object, as I am going to delete the
// 			 current one */
// 	 /* ... */
//     
// 	 delete f;  /* and the subsequent next() will always work */
//   }
// 
// I would prefer the list_iterators to support safe
// deletion, but making a overhaul of them is probably not our
// top-priority task for the moment.   In any case,  I prefer the
// behaviour to be stated explictly in the header files and some other
// documentations.
// 
// A pitfall of using list_iterators with concurrent threads:
// =================================================================
// 
// When you use concurrent threads to access data items returned by
// some list_iterators, you *must* be careful and put a *lock* around the list.
// As always, data items shared by multiple concurrent users must always
// be protected by locks, or strange race conditions may happen.
// 
// For example, we think the following code, found in server's callback
// routine, is wrong:
// 
// while (p) {
//     current = strbase (..., p, handle);
//     p = next();
// 
//     RPC_Call();
//     current->something = foo;
// }
// 
// Now the RPC call internally yields ands allows other threads to run,
// another thread can now do some cleanup action which can destroy
// current or next.
// 
//     -- clement and jaharkes (July/20/99)

class dlink {		    // objects are derived from this class
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
