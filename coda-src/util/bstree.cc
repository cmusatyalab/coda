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

static char *rcsid = "$Header: /usr/rvb/XX/src/coda-src/util/RCS/bstree.cc,v 4.1 1997/01/08 21:51:02 rvb Exp $";
#endif /*_BLURB_*/








/*
 *
 *    Implementation of binary search tree, based on example in "Data Structures and Algorithms" by AHU.
 *
 */


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif
#ifdef __cplusplus
}
#endif __cplusplus

#include "bstree.h"

/* DEBUGGING! -JJK */
/*
extern FILE *logFile;
extern void Die(char * ...);
*/


/*  *****  Some Useful Macros  *****  */

#define	ZERONODE(n)\
{\
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
	if (((n1)->parent)->leftchild == (n1))\
	    ((n1)->parent)->leftchild = (n2);\
	else\
	    ((n1)->parent)->rightchild = (n2);\
    }\
\
    if ((n2) != 0)\
	(n2)->parent = (n1)->parent;\
}


/*  *****  Binary Search Tree  *****  */

bstree::bstree(BSTCFN F) {
    root = 0;
    CmpFn = F;
    cnt = 0;

    inserts = 0;
    removes = 0;
    gets = 0;
}


bstree::bstree(bstree& bst) {
    abort();
}


bstree::operator=(bstree& bst) {
    abort();
    return(0); /* keep C++ happy */
}


bstree::~bstree() {
    /* This is dangerous! */
    /* Perhaps we should abort() if count() != 0?  -JJK */
    clear();
}


/* Does NOT require uniqueness of keys. */
void bstree::insert(bsnode *b) {
/*
    if (!IsOrdered())
	{ print(logFile); b->print(logFile); Die("bstree::insert: !ordered at entry"); }
*/
    if (b->tree() != 0) abort();
/*	{ print(logFile); b->print(logFile); Die("bstree::insert: already on %x", b->tree()); }*/
    inserts++;

    if (root == 0)
	root = b;
    else {
	bsnode *curr = root;
	for (;;) {
	    int res = CmpFn(b, curr);
	    if (res == 0) {
		/* Break ties using addresses. */
		if ((char *)b > (char *)curr) res = 1;
		else if ((char *)b < (char *)curr) res = -1;
		else abort();
/*		    { print(logFile); b->print(logFile); Die("bstree::insert: found in tree"); }*/
	    }

	    if (res < 0) {
		if (curr->leftchild == 0) {
		    b->parent = curr;
		    curr->leftchild = b;
		    break;
		}
		curr = curr->leftchild;
	    }
	    else {	/* res > 0 */
		if (curr->rightchild == 0) {
		    b->parent = curr;
		    curr->rightchild = b;
		    break;
		}
		curr = curr->rightchild;
	    }
	}
    }

    b->mytree = this;
    cnt++;
/*
    if (!IsOrdered())
	{ print(logFile); b->print(logFile); Die("bstree::insert: !ordered at exit"); }
*/
}


bsnode *bstree::remove(bsnode *b) {
/*
    if (!IsOrdered())
	{ print(logFile); b->print(logFile); Die("bstree::remove: !ordered at entry"); }
*/
    if (b->tree() != this) return(0);
    removes++;

    if (b->leftchild == 0) {
	SPLICE(b, b->rightchild);
    }
    else if (b->rightchild == 0) {
	SPLICE(b, b->leftchild);
    }
    else {
	/* Find and remove node to be promoted. */
	bsnode *t;
	FIRST(b->rightchild, t);
	SPLICE(t, t->rightchild);

	/* Plug promoted node in for the one being removed. */
	SPLICE(b, t);
	t->leftchild = b->leftchild;
	if (t->leftchild != 0)
	    (t->leftchild)->parent = t;
	t->rightchild = b->rightchild;
	if (t->rightchild != 0)
	    (t->rightchild)->parent = t;
    }

    ZERONODE(b);
    cnt--;
/*
    if (!IsOrdered())
	{ print(logFile); b->print(logFile); Die("bstree::remove: !ordered at exit"); }
*/
    return(b);
}


bsnode *bstree::first() {
    if (root == 0) return(0);

    bsnode *b;
    FIRST(root, b);
    return(b);
}


bsnode *bstree::last() {
    if (root == 0) return(0);

    bsnode *b;
    LAST(root, b);
    return(b);
}


bsnode *bstree::get(BstGetType type) {
/*
    if (!IsOrdered())
	{ print(logFile); Die("bstree::get: !ordered at entry"); }
*/
    if (root ==	0) return(0);
    gets++;

    bsnode *b;
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
	{ print(logFile); Die("bstree::get: !ordered at exit"); }
*/
    return(b);
}


void bstree::clear() {
    bsnode *b;
    while(b = get())
	;
    if (cnt != 0) abort();
/*	{ print(logFile); Die("bstree::clear: tree not empty"); }*/
}


int bstree::count() {
    return(cnt);
}


/* TRUE if node OR it's value is member of the tree! */
int bstree::IsMember(bsnode *b) {
    if (b->mytree == this) return(1);
    if (b->mytree != 0) return(0);

    for (bsnode *curr = root; curr != 0;) {
	int res = CmpFn(b, curr);
	if (res == 0) {
	    if ((char *)b == (char *)curr) abort();
/*		{ print(logFile); b->print(logFile); Die("bstree::IsMember: found in tree"); }*/
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
int bstree::IsOrdered() {
    bstree_iterator next(*this);
    bsnode *curr = 0;
    bsnode *prev = 0;
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


void bstree::print() {
    print(stdout);
}


void bstree::print(FILE *fp) {
    fflush(fp);
    print(fileno(fp));
}


void bstree::print(int fd) {
    /* first print out the bstree header */
    char buf[160];
    sprintf(buf, "%#08x : root = %x, cnt = %d, inserts = %d, removes = %d, gets = %d\n",
	     (long)this, root, cnt, inserts, removes, gets);
    write(fd, buf, strlen(buf));

    /* then print out all of the bsnodes */
    bstree_iterator next(*this);
    bsnode *b;
    while (b = next())
	b->print(fd);
}


/*  ***** Binary Search Tree Nodes  *****  */

bsnode::bsnode() {
    mytree = 0;
    parent = 0;
    leftchild = 0;
    rightchild = 0;
}


bsnode::bsnode(bsnode& b) {
    abort();
}


bsnode::operator=(bsnode& b) {
    abort();
    return(0); /* keep C++ happy */
}


bsnode::~bsnode() {
}


bstree *bsnode::tree() {
    if (mytree == 0)
	if (parent != 0 || leftchild != 0 || rightchild != 0) abort();
/*	    { print(logFile); Die("bsnode::tree: tree = 0 && ptr != 0"); }*/

    return(mytree);
}


void bsnode::print() {
    print(stdout);
}


void bsnode::print(FILE *fp) {
    fflush(fp);
    print(fileno(fp));
}


void bsnode::print(int fd) {
    char buf[80];
    sprintf(buf, "%#08x : tree = %x, parent = %x, left = %x, right = %x\n",
	     (long)this, mytree, parent, leftchild, rightchild);
    write(fd, buf, strlen(buf));
}


/*  *****  Binary Search Tree Iterator  *****  */

bstree_iterator::bstree_iterator(bstree& Bst, BstIterOrder Order) {
    cbstree = &Bst;
    cbsnode = (bsnode *)-1;
    order = Order;
}


bsnode *bstree_iterator::operator()() {
    bsnode *prev = cbsnode;

    switch((unsigned int)cbsnode) {
	case -1:		/* state == NOTSTARTED */
	    if (order == BstAscending) {
		cbsnode = cbstree->first();
	    }
	    else {
		cbsnode = cbstree->last();
	    }
	    break;

	default:		/* state == INPROGRESS */
	    if (order == BstAscending) {
		if (cbsnode->rightchild == 0) {
		    bsnode *t;
		    do {
			t = cbsnode;
			cbsnode = cbsnode->parent;
		    } while (cbsnode != 0 && cbsnode->rightchild == t);
		}
		else {
		    FIRST(cbsnode->rightchild, cbsnode);
		}
	    }
	    else {
		if (cbsnode->leftchild == 0) {
		    bsnode *t;
		    do {
			t = cbsnode;
			cbsnode = cbsnode->parent;
		    } while (cbsnode != 0 && cbsnode->leftchild == t);
		}
		else {
		    LAST(cbsnode->leftchild, cbsnode);
		}
	    }
	    break;

	case 0:			/* state == FINISHED */
	    break;
    }

    /* DEBUG!  Prevent looping! */
    if (cbsnode != 0 && cbsnode == prev)
	cbsnode = 0;

    return(cbsnode);
}
