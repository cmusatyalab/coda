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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/util/rec_bstree.cc,v 4.2 1997/02/26 16:03:05 rvb Exp $";
#endif /*_BLURB_*/









/*
 *
 *    Implementation of recoverable binary search tree, based on example in
 *    "Data Structures and Algorithms" by AHU.
 *
 */


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif

#include <setjmp.h>
#include <stdio.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include "rec_bstree.h"

/* DEBUGGING! -JJK */
/*
extern FILE *logFile;
extern void Die(char * ...);
*/


/*  *****  Some Useful Macros  *****  */

#define	ZERONODE(n)\
{\
    RVMLIB_REC_OBJECT(*(n));\
    (n)->mytree = 0;\
    (n)->parent = 0;\
    (n)->leftchild = 0;\
    (n)->rightchild = 0;\
}

#define	FIRST(r, n)\
{\
    (n) = (r);\
    while ((n)->leftchild != 0)\
	(n) = (n)->leftchild;\
}

#define LAST(r, n)\
{\
    (n) = (r);\
    while ((n)->rightchild != 0)\
	(n) = (n)->rightchild;\
}

#define	SPLICE(n1, n2)\
{\
    if ((n1)->parent == 0)\
	root = (n2);\
    else {\
	RVMLIB_REC_OBJECT(*((n1)->parent));\
	if (((n1)->parent)->leftchild == (n1))\
	    ((n1)->parent)->leftchild = (n2);\
	else\
	    ((n1)->parent)->rightchild = (n2);\
    }\
\
    if ((n2) != 0) {\
	RVMLIB_REC_OBJECT(*(n2));\
	(n2)->parent = (n1)->parent;\
    }\
}


/*  *****  Binary Search Tree  *****  */

void *rec_bstree::operator new (size_t size) {
    rec_bstree *r = 0;

    r = (rec_bstree *)RVMLIB_REC_MALLOC(size);
    assert(r);
    return(r);
}

void rec_bstree::operator delete(void *deadobj, size_t size){
    RVMLIB_REC_FREE(deadobj);
}


rec_bstree::rec_bstree(RBSTCFN F) {
    Init(F);
}


rec_bstree::~rec_bstree() {
    DeInit();
}

void rec_bstree::Init(RBSTCFN F) {
    RVMLIB_REC_OBJECT(*this);

    root = 0;
    CmpFn = F;
    cnt = 0;

    inserts = 0;
    removes = 0;
    gets = 0;
}

void rec_bstree::DeInit() {
    if (cnt != 0) abort();
}

/* The compare function is not necessarily recoverable, so don't insist on an enclosing transaction! */
void rec_bstree::SetCmpFn(RBSTCFN F) {
    if (RVM_THREAD_DATA->tid != 0)
	RVMLIB_REC_OBJECT(*this);

    CmpFn = F;
}


void rec_bstree::ClearStatistics() {
    RVMLIB_REC_OBJECT(*this);

    inserts = 0;
    removes = 0;
    gets = 0;
}




rec_bstree::rec_bstree(rec_bstree& bst) {
    abort();
}


rec_bstree::operator=(rec_bstree& bst) {
    abort();
    return(0); /* keep C++ happy */
}




/* Does NOT require uniqueness of keys. */
void rec_bstree::insert(rec_bsnode *b) {
/*
    if (!IsOrdered())
	{ print(logFile); b->print(logFile); Die("rec_bstree::insert: !ordered at entry"); }
*/
    if (b->tree() != 0) abort();
/*	{ print(logFile); b->print(logFile); Die("rec_bstree::insert: already on %x", b->tree()); }*/
    RVMLIB_REC_OBJECT(*this);
    inserts++;

    if (root == 0) {
	root = b;
	RVMLIB_REC_OBJECT(*b);
	b->mytree = this;
    }
    else {
	rec_bsnode *curr = root;
	for (;;) {
	    int res = CmpFn(b, curr);
	    if (res == 0) {
		/* Break ties using addresses. */
		if ((char *)b > (char *)curr) res = 1;
		else if ((char *)b < (char *)curr) res = -1;
		else abort();
/*		    { print(logFile); b->print(logFile); Die("rec_bstree::insert: found in tree"); }*/
	    }

	    if (res < 0) {
		if (curr->leftchild == 0) {
		    RVMLIB_REC_OBJECT(*b);
		    b->parent = curr;
		    b->mytree = this;
		    RVMLIB_REC_OBJECT(*curr);
		    curr->leftchild = b;
		    break;
		}
		curr = curr->leftchild;
	    }
	    else {	/* res > 0 */
		if (curr->rightchild == 0) {
		    RVMLIB_REC_OBJECT(*b);
		    b->parent = curr;
		    b->mytree = this;
		    RVMLIB_REC_OBJECT(*curr);
		    curr->rightchild = b;
		    break;
		}
		curr = curr->rightchild;
	    }
	}
    }

    cnt++;
/*
    if (!IsOrdered())
	{ print(logFile); b->print(logFile); Die("rec_bstree::insert: !ordered at exit"); }
*/
}


rec_bsnode *rec_bstree::remove(rec_bsnode *b) {
/*
    if (!IsOrdered())
	{ print(logFile); b->print(logFile); Die("rec_bstree::remove: !ordered at entry"); }
*/
    if (b->tree() != this) return(0);
    RVMLIB_REC_OBJECT(*this);
    removes++;

    if (b->leftchild == 0) {
	SPLICE(b, b->rightchild);
    }
    else if (b->rightchild == 0) {
	SPLICE(b, b->leftchild);
    }
    else {
	/* Find and remove node to be promoted. */
	rec_bsnode *t;
	FIRST(b->rightchild, t);
	SPLICE(t, t->rightchild);

	/* Plug promoted node in for the one being removed. */
	SPLICE(b, t);
	t->leftchild = b->leftchild;
	if (t->leftchild != 0) {
	    RVMLIB_REC_OBJECT(*(t->leftchild));
	    (t->leftchild)->parent = t;
	}
	t->rightchild = b->rightchild;
	if (t->rightchild != 0) {
	    RVMLIB_REC_OBJECT(*(t->rightchild));
	    (t->rightchild)->parent = t;
	}
    }

    ZERONODE(b);
    cnt--;
/*
    if (!IsOrdered())
	{ print(logFile); b->print(logFile); Die("rec_bstree::remove: !ordered at exit"); }
*/
    return(b);
}


rec_bsnode *rec_bstree::first() {
    if (root == 0) return(0);

    rec_bsnode *b;
    FIRST(root, b);
    return(b);
}


rec_bsnode *rec_bstree::last() {
    if (root == 0) return(0);

    rec_bsnode *b;
    LAST(root, b);
    return(b);
}


rec_bsnode *rec_bstree::get(BstGetType type) {
/*
    if (!IsOrdered())
	{ print(logFile); Die("rec_bstree::get: !ordered at entry"); }
*/
    if (root ==	0) return(0);
    RVMLIB_REC_OBJECT(*this);
    gets++;

    rec_bsnode *b;
    if (type == BstGetMin) {
	FIRST(root, b);
	SPLICE(b, b->rightchild);
    }
    else {
	LAST(root, b);
	SPLICE(b, b->leftchild);
    }

    ZERONODE(b);
    cnt--;
/*
    if (!IsOrdered())
	{ print(logFile); Die("rec_bstree::get: !ordered at exit"); }
*/
    return(b);
}


int rec_bstree::count() {
    return(cnt);
}


/* TRUE if node OR it's value is member of the tree! */
int rec_bstree::IsMember(rec_bsnode *b) {
    if (b->mytree == this) return(1);
    if (b->mytree != 0) return(0);

    for (rec_bsnode *curr = root; curr != 0;) {
	int res = CmpFn(b, curr);
	if (res == 0) {
	    if ((char *)b == (char *)curr) abort();
/*		{ print(logFile); b->print(logFile); Die("rec_bstree::IsMember: found in tree"); }*/
	    return(1);
	}

	if (res < 0)
	    curr = curr->leftchild;
	else	/* res > 0 */
	    curr = curr->rightchild;
    }

    return(0);
}


/* Sanity checker. */
int rec_bstree::IsOrdered() {
    rec_bstree_iterator next(*this);
    rec_bsnode *curr = 0;
    rec_bsnode *prev = 0;
    while (curr = next()) {
	if (prev != 0) {
	    int res = CmpFn(prev, curr);

	    if (res > 0 || (res == 0 && (char *)prev >= (char *)curr)) {
/*
		prev->print(logFile);
		curr->print(logFile);
*/
		return(0);
	    }
	}
	if (curr->leftchild != 0) {
	    int res = CmpFn(curr->leftchild, curr);

	    if (res > 0 || (res == 0 && (char *)curr->leftchild >= (char *)curr)) {
/*
		 (curr->leftchild)->print(logFile);
		 curr->print(logFile);
*/
		return(0);
	    }
	}
	if (curr->rightchild != 0) {
	    int res = CmpFn(curr, curr->rightchild);

	    if (res > 0 || (res == 0 && (char *)curr >= (char *)curr->rightchild)) {
/*
		 curr->print(logFile);
		 (curr->rightchild)->print(logFile);
*/
		return(0);
	    }
	}

	prev = curr;
    }
    return(1);
}


void rec_bstree::print() {
    print(stdout);
}


void rec_bstree::print(FILE *fp) {
    fflush(fp);
    print(fileno(fp));
}


void rec_bstree::print(int fd) {
    /* first print out the rec_bstree header */
    char buf[160];
    sprintf(buf, "%#08x : root = %x, cnt = %d, inserts = %d, removes = %d, gets = %d\n",
	     (long)this, root, cnt, inserts, removes, gets);
    write(fd, buf, strlen(buf));

    /* then print out all of the rec_bsnodes */
    rec_bstree_iterator next(*this);
    rec_bsnode *b;
    while (b = next())
	b->print(fd);
}


/*  ***** Binary Search Tree Nodes  *****  */

rec_bsnode::rec_bsnode() {
    /* There is never any point in calling "new rec_bsnode"! */
    if (this == 0) abort();

    Init();
}


void rec_bsnode::Init() {
    RVMLIB_REC_OBJECT(*this)
    mytree = 0;
    parent = 0;
    leftchild = 0;
    rightchild = 0;
}


rec_bsnode::rec_bsnode(rec_bsnode& b) {
    abort();
}


rec_bsnode::operator=(rec_bsnode& b) {
    abort();
    return(0); /* keep C++ happy */
}


/*
rec_bsnode::~rec_bsnode() {
    DeInit();
}


void rec_bsnode::DeInit() {
}
*/


rec_bstree *rec_bsnode::tree() {
    if (mytree == 0)
	if (parent != 0 || leftchild != 0 || rightchild != 0) abort();
/*	    { print(logFile); Die("rec_bsnode::tree: tree = 0 && ptr != 0"); }*/

    return(mytree);
}


void rec_bsnode::print() {
    print(stdout);
}


void rec_bsnode::print(FILE *fp) {
    fflush(fp);
    print(fileno(fp));
}


void rec_bsnode::print(int fd) {
    char buf[80];
    sprintf(buf, "%#08x : tree = %x, parent = %x, left = %x, right = %x\n",
	     (long)this, mytree, parent, leftchild, rightchild);
    write(fd, buf, strlen(buf));
}


/*  *****  Binary Search Tree Iterator  *****  */

rec_bstree_iterator::rec_bstree_iterator(rec_bstree& Bst, BstIterOrder Order) {
    crec_bstree = &Bst;
    crec_bsnode = (rec_bsnode *)-1;
    order = Order;
}


rec_bsnode *rec_bstree_iterator::operator()() {
    rec_bsnode *prev = crec_bsnode;

    switch((unsigned int)crec_bsnode) {
	case -1:		/* state == NOTSTARTED */
	    if (order == BstAscending) {
		crec_bsnode = crec_bstree->first();
	    }
	    else {
		crec_bsnode = crec_bstree->last();
	    }
	    break;

	default:		/* state == INPROGRESS */
	    if (order == BstAscending) {
		if (crec_bsnode->rightchild == 0) {
		    rec_bsnode *t;
		    do {
			t = crec_bsnode;
			crec_bsnode = crec_bsnode->parent;
		    } while (crec_bsnode != 0 && crec_bsnode->rightchild == t);
		}
		else {
		    FIRST(crec_bsnode->rightchild, crec_bsnode);
		}
	    }
	    else {
		if (crec_bsnode->leftchild == 0) {
		    rec_bsnode *t;
		    do {
			t = crec_bsnode;
			crec_bsnode = crec_bsnode->parent;
		    } while (crec_bsnode != 0 && crec_bsnode->leftchild == t);
		}
		else {
		    LAST(crec_bsnode->leftchild, crec_bsnode);
		}
	    }
	    break;

	case 0:			/* state == FINISHED */
	    break;
    }

    /* DEBUG!  Prevent looping! */
    if (crec_bsnode != 0 && crec_bsnode == prev)
	crec_bsnode = 0;

    return(crec_bsnode);
}
