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

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/coda-src/advice/mybstree.cc,v 1.3 1997/01/07 18:40:17 rvb Exp";
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
#ifdef	__MACH__
#include <libc.h>
#include <sysent.h>
#endif
#if defined(__linux__) || defined(__NetBSD__)
#include <stdlib.h>
#include <unistd.h>
#endif /* LINUX || __NetBSD__ */
#include <assert.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include "mybstree.h"

/* DEBUGGING! -JJK */
/*
extern FILE *logFile;
extern void Die(char * ...);
*/


/*  *****  Some Useful Macros  *****  */

#define	ZERONODE(n)\
{\
     assert(n != NULL);\
    (n)->mytree = 0;\
    (n)->parent = 0;\
    (n)->leftchild = 0;\
    (n)->rightchild = 0;\
}

#define	FIRST(r, n)\
{\
    (n) = (r);\
    assert(n != NULL);\
    while ((n)->leftchild != 0)\
	(n) = (n)->leftchild;\
}

#define LAST(r, n)\
{\
    (n) = (r);\
    assert(n != NULL);\
    while ((n)->rightchild != 0)\
	(n) = (n)->rightchild;\
}

#define	SPLICE(n1, n2)\
{\
    assert(n1 != NULL);\
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
    assert(b != NULL);
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
		assert(curr != NULL);
		if (curr->leftchild == 0) {
		    assert(b != NULL);
		    b->parent = curr;
		    curr->leftchild = b;
		    break;
		}
		assert(curr != NULL);
		curr = curr->leftchild;
	    }
	    else {	/* res > 0 */
		assert(curr != NULL);
		if (curr->rightchild == 0) {
		    assert(b != NULL);
		    b->parent = curr;
		    curr->rightchild = b;
		    break;
		}
		assert(curr != NULL);
		curr = curr->rightchild;
	    }
	}
    }

    assert(b != NULL);
    b->mytree = this;
    cnt++;
/*
    if (!IsOrdered())
	{ print(logFile); b->print(logFile); Die("bstree::insert: !ordered at exit"); }
*/
}


/*
 * This routine removes an entry from the tree.  Note that it uses pointer comparison
 * to remove the entry to the node b MUST be an element of the tree.  In particular,
 * just because "IsMember" believes that b is an element of the tree, does NOT mean
 * that remove will remove the element. 
 */
bsnode *bstree::remove(bsnode *b) {
/*
    if (!IsOrdered())
	{ print(logFile); b->print(logFile); Die("bstree::remove: !ordered at entry"); }
*/
    assert(b != NULL);
    if (b->tree() != this) return(0);
    removes++;

    assert(b != NULL);
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
	assert(t != NULL);
	SPLICE(t, t->rightchild);

	/* Plug promoted node in for the one being removed. */
	SPLICE(b, t);
	assert(t != NULL);
	assert(b != NULL);
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
	assert(b != NULL);
	SPLICE(b, b->rightchild);
    }
    else {
	LAST(root, b);
	assert(b != NULL);
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

/*  
 * This routine uses content comparison to find a node equal (as per CmpFn)
 * to node b.  If it finds one, it returns the one contained in the tree.
 * Note that b need not be a member of the actual tree.
 */
bsnode *bstree::get(bsnode *b) {
    assert(b != NULL);
    if ((b->mytree != this) && (b->mytree != 0)) return(0);

    for (bsnode *curr = root; curr != 0;) {
	int res = CmpFn(b, curr);
	if (res == 0) {
	    if ((char *)b == (char *)curr) abort();
/*		{ print(logFile); b->print(logFile); Die("bstree::IsMember: found in tree"); }*/
	    return(curr);
	}

	if (res < 0) {
	    assert(curr != NULL);
	    curr = curr->leftchild;
        }
	else {	/* res > 0 */
	    assert(curr != NULL);
	    curr = curr->rightchild;
        }
    }

    return(0);
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


/*
 * This routine uses content comparison to determine of b is an element of the tree.
 * Note that b need not be an actual element of the tree!  This is great for checking
 * for duplicates before doing an insertion.
 */
/* TRUE if node OR it's value is member of the tree! */
int bstree::IsMember(bsnode *b) {
    assert(b != NULL);
    if (b->mytree == this) return(1);
    if (b->mytree != 0) return(0);

    for (bsnode *curr = root; curr != 0;) {
	int res = CmpFn(b, curr);
	if (res == 0) {
	    if ((char *)b == (char *)curr) abort();
/*		{ print(logFile); b->print(logFile); Die("bstree::IsMember: found in tree"); }*/
	    return(1);
	}

	if (res < 0) {
	    assert(curr != NULL);
	    curr = curr->leftchild;
        }
	else {	/* res > 0 */
	    assert(curr != NULL);
	    curr = curr->rightchild;
        }
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
	assert(curr != NULL);
	if (curr->leftchild != 0) {
	    int res = CmpFn(curr->leftchild, curr);
	    assert(curr != NULL);
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
	    assert(curr != NULL);
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
    snprintf(buf, 160, "%#08x : root = %x, cnt = %d, inserts = %d, removes = %d, gets = %d\n",
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
    snprintf(buf, 80, "%#08x : tree = %x, parent = %x, left = %x, right = %x\n",
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
		assert(cbstree != NULL);
		cbsnode = cbstree->first();
	    }
	    else {
		assert(cbstree != NULL);
		cbsnode = cbstree->last();
	    }
	    break;

	default:		/* state == INPROGRESS */
	    if (order == BstAscending) {
		assert(cbsnode != NULL);
		if (cbsnode->rightchild == 0) {
		    bsnode *t;
		    do {
			t = cbsnode;
			assert(cbsnode != NULL);
			cbsnode = cbsnode->parent;
		    } while (cbsnode != 0 && cbsnode->rightchild == t);
		}
		else {
		    assert(cbsnode != NULL);
		    FIRST(cbsnode->rightchild, cbsnode);
		}
	    }
	    else {
		assert(cbsnode != NULL);
		if (cbsnode->leftchild == 0) {
		    bsnode *t;
		    do {
			t = cbsnode;
			assert(cbsnode != NULL);
			cbsnode = cbsnode->parent;
		    } while (cbsnode != 0 && cbsnode->leftchild == t);
		}
		else {
		    assert(cbsnode != NULL);
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
