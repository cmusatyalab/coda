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

static char *rcsid = "$Header: /afs/cs.cmu.edu/project/coda-braam/src/coda-4.0.1/RCSLINK/./coda-src/util/rec_smolist.h,v 1.1 1996/11/22 19:08:22 braam Exp $";
#endif /*_BLURB_*/








/*
 *
 * rec_smolist.h -- Specification of a recoverable singly-linked list type 
 * where list elements can be on only one list at a time.
 *
 * NOTE (Satya, 5/31/95)
 * ---------------------
 * The files util/rec_smolist.[ch] were originally called vol/recolist.[ch].
 * The original data structures  recolink and recolist are now
 * called rec_smolink and rec_smolist (for "small" olist and olink).   
 * No functional code changes have been made --- just systematic renaming.
 * 
 * This change was made to reduce confusion with rec_olist and rec_olink.
 * The rec_smo* data structures are very similar to the rec_o* data structures
 * except that the recoverable structures are smaller.  
 * rec_smolist occupies the size of one long  (pointer to the last element)
 * while rec_olist is 12 bytes long.  
 * These data structures were defined as part of a fix to the 
 * server a long time ago, avoiding re-initialization.
 * rec_smolist was designed to convert each volume's vnode array from an
 * array of pointers (each to a single vnode) to an array of lists of
 * vnodes without having to reinitialize the servers.  It is now
 * possible to have multiple vnodes with the same vnode number because
 * resolution can recreate a previously deleted vnode. 
 *
 */

#ifndef _UTIL_REC_SMOLIST_H_
#define _UTIL_REC_SMOLIST_H_ 1

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>

#ifdef __cplusplus
}
#endif __cplusplus



class rec_smolist;
class rec_smolist_iterator;
class rec_smolink;


class rec_smolist {
  friend class rec_smolist_iterator;
    rec_smolink *last;	    // last->next is head of list

  public:
    rec_smolist();
    ~rec_smolist();
    void insert(rec_smolink *);	// add at head of list
    void append(rec_smolink *);	// add at tail of list
    rec_smolink *remove(rec_smolink *); // remove specified entry
    rec_smolink *get();		// return and remove head of list
    int	IsEmpty();		// 1 if list is empty
    void print();
    void print(FILE *);
    void print(int);
};


class rec_smolist_iterator {
    rec_smolist *clist;		// current olist
    rec_smolink *clink;		// current olink
    rec_smolink *nlink;		// next olink (in case they remove the object)
	
  public:
    rec_smolist_iterator(rec_smolist&);
    rec_smolink *operator()();	// return next object or 0
};


/* generic list object:
   1. pointer to the next list item
   2. size of object 
   3. pointer to object 
*/
class rec_smolink {		    
  friend class rec_smolist;
  friend class rec_smolist_iterator;
  friend void main(int , char **);
    rec_smolink *next;
  public:
    rec_smolink();
    ~rec_smolink();
    void print();
    void print(FILE *);
    void print(int);
};

#endif	not _UTIL_REC_SMOLIST_H_
