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
    void print();
    void print(FILE *);
    void print(int);
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
