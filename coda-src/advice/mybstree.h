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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/advice/Attic/mybstree.h,v 4.1 1997/01/08 21:49:18 rvb Exp $";
#endif /*_BLURB_*/









/*
 *
 *    Specification of binary search tree.
 *
 */

#ifndef _UTIL_BSTREE_H_
#define _UTIL_BSTREE_H_ 1

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>

#ifdef __cplusplus
}
#endif __cplusplus



class bstree;
class bsnode;
class bstree_iterator;
typedef int (*BSTCFN)(bsnode *, bsnode *);


enum BstGetType { BstGetMin, BstGetMax };

class bstree {
    bsnode *root;
    BSTCFN CmpFn;				/* function to order the nodes */
    int cnt;

    /* statistics */
    int inserts;
    int removes;
    int gets;

  public:
    bstree(BSTCFN);
    bstree(bstree&);				/* not supported! */
    operator=(bstree&);				/* not supported! */
    virtual ~bstree();

    void insert(bsnode *);			/* insert in sorted order */
    bsnode *remove(bsnode *);			/* remove specified entry */
    bsnode *first();				/* return MINIMUM node */
    bsnode *last();				/* return MAXMIMUM node */
    bsnode *get(BstGetType =BstGetMin);		/* return and remove MIN or MAX node */
    bsnode *get(bsnode *);			/* return a node in the tree */
    void clear();				/* remove all entries */

    int count();
    int IsMember(bsnode *);
    int	IsOrdered();				/* sanity checker */
    virtual void print();
    virtual void print(FILE *);
    virtual void print(int);
};


class bsnode {
  friend class bstree;
  friend class bstree_iterator;
    bstree *mytree;
    bsnode *parent;
    bsnode *leftchild;
    bsnode *rightchild;

  public:
    bsnode();
    bsnode(bsnode&);				/* not supported! */
    operator=(bsnode&);				/* not supported! */
    virtual ~bsnode();

    bstree *tree();
    virtual void print();
    virtual void print(FILE *);
    virtual void print(int);
};


enum BstIterOrder { BstAscending, BstDescending };

class bstree_iterator {
    bstree *cbstree;				/* tree being iterated over */
    bsnode *cbsnode;				/* current node in the iteration */
    BstIterOrder order;

  public:
    bstree_iterator(bstree&, BstIterOrder =BstAscending);
    bsnode *operator()();			/* return next node or 0 */
};

#endif	not _UTIL_BSTREE_H_
