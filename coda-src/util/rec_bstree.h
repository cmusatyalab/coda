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

static char *rcsid = "$Header: blurb.doc,v 1.1 96/11/22 13:29:31 raiff Exp $";
#endif /*_BLURB_*/









/*
 *
 *    Specification of recoverable binary search tree.
 *
 */

#ifndef _UTIL_REC_BSTREE_H_
#define _UTIL_REC_BSTREE_H_ 1

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>

#ifdef __cplusplus
}
#endif __cplusplus


#include "bstree.h"
#include "rvmlib.h"


class rec_bstree;
class rec_bsnode;
class rec_bstree_iterator;
typedef int (*RBSTCFN)(rec_bsnode *, rec_bsnode *);


/*enum BstGetType { BstGetMin, BstGetMax };*/

class rec_bstree {
    rec_bsnode *root;
    RBSTCFN CmpFn;					/* function to order the nodes */
    int cnt;

    /* statistics */
    int inserts;
    int removes;
    int gets;

  public:
    void *operator new (size_t);
    void operator delete(void *, size_t);

    rec_bstree(RBSTCFN);
    rec_bstree(rec_bstree&);				/* not supported! */
    ~rec_bstree();
    void Init(RBSTCFN);
    void SetCmpFn(RBSTCFN);
    void ClearStatistics();
    operator=(rec_bstree&);				/* not supported! */
    void DeInit();

    void insert(rec_bsnode *);				/* insert in sorted order */
    rec_bsnode *remove(rec_bsnode *);			/* remove specified entry */
    rec_bsnode *first();				/* return MINIMUM node */
    rec_bsnode *last();					/* return MAXMIMUM node */
    rec_bsnode *get(BstGetType =BstGetMin);		/* return and remove MIN or MAX node */

    int count();
    int IsMember(rec_bsnode *);
    int	IsOrdered();					/* sanity checker */
    /*virtual*/ void print();
    /*virtual*/ void print(FILE *);
    /*virtual*/ void print(int);
};


class rec_bsnode {
  friend class rec_bstree;
  friend class rec_bstree_iterator;
    rec_bstree *mytree;
    rec_bsnode *parent;
    rec_bsnode *leftchild;
    rec_bsnode *rightchild;

  public:
    rec_bsnode();
    void Init();
    rec_bsnode(rec_bsnode&);				/* not supported! */
    operator=(rec_bsnode&);				/* not supported! */
/*
    ~rec_bsnode();
    void DeInit();
*/

    rec_bstree *tree();
    /*virtual*/ void print();
    /*virtual*/ void print(FILE *);
    /*virtual*/ void print(int);
};


/*enum BstIterOrder { BstAscending, BstDescending };*/

class rec_bstree_iterator {
    rec_bstree *crec_bstree;				/* tree being iterated over */
    rec_bsnode *crec_bsnode;				/* current node in the iteration */
    BstIterOrder order;

  public:
    rec_bstree_iterator(rec_bstree&, BstIterOrder =BstAscending);
    rec_bsnode *operator()();				/* return next node or 0 */
};

#endif	not _UTIL_REC_BSTREE_H_
