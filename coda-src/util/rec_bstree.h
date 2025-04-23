/* BLURB gpl

                           Coda File System
                              Release 8

          Copyright (c) 1987-2025 Carnegie Mellon University
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
 *    Specification of recoverable binary search tree.
 *
 */

#ifndef _UTIL_REC_BSTREE_H_
#define _UTIL_REC_BSTREE_H_ 1

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#ifdef __cplusplus
}
#endif

#include "bstree.h"
#include "rvmlib.h"

class rec_bstree;
class rec_bsnode;
class rec_bstree_iterator;
typedef int (*RBSTCFN)(rec_bsnode *, rec_bsnode *);

/*enum BstGetType { BstGetMin, BstGetMax };*/

class rec_bstree {
    rec_bsnode *root;
    RBSTCFN CmpFn; /* function to order the nodes */
    int cnt;

    /* statistics */
    int inserts;
    int removes;
    int gets;

public:
    void *operator new(size_t) REQUIRES_TRANSACTION;
    void operator delete(void *) REQUIRES_TRANSACTION;

    rec_bstree(RBSTCFN);
    rec_bstree(rec_bstree &); /* not supported! */
    ~rec_bstree();
    void Init(RBSTCFN) REQUIRES_TRANSACTION;
    void SetCmpFn(RBSTCFN);
    void ClearStatistics() REQUIRES_TRANSACTION;
    int operator=(rec_bstree &); /* not supported! */
    void DeInit();

    void insert(rec_bsnode *) REQUIRES_TRANSACTION; /* insert in sorted order */
    rec_bsnode *
    remove(rec_bsnode *) REQUIRES_TRANSACTION; /* remove specified entry */
    rec_bsnode *first(); /* return MINIMUM node */
    rec_bsnode *last(); /* return MAXMIMUM node */
    rec_bsnode *get(BstGetType = BstGetMin)
        REQUIRES_TRANSACTION; /* return and remove MIN or MAX node */

    int count();
    int IsMember(rec_bsnode *);
    int IsOrdered(); /* sanity checker */
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
    void Init() REQUIRES_TRANSACTION;
    rec_bsnode(rec_bsnode &); /* not supported! */
    int operator=(rec_bsnode &); /* not supported! */
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
    rec_bstree *crec_bstree; /* tree being iterated over */
    rec_bsnode *crec_bsnode; /* current node in the iteration */
    BstIterOrder order;

public:
    rec_bstree_iterator(rec_bstree &, BstIterOrder = BstAscending);
    rec_bsnode *operator()(); /* return next node or 0 */
};

#endif /* _UTIL_REC_BSTREE_H_ */
