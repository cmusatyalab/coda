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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/util/rec_smolist.cc,v 4.3 1998/06/11 14:40:13 jaharkes Exp $";
#endif /*_BLURB_*/








/*
 *
 * rec_smolist.c -- Implementation of rec_smolist type.
 *
 *                  Read comment in rec_smolist.h for relationship
 *		    to rec_olist.c (Satya, 5/31/95)
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

#include "util.h"
#include "rvmlib.h"
#include "rec_smolist.h"


rec_smolist::rec_smolist() {
/* the user of this package should do their own initialization */
}

rec_smolist::~rec_smolist() {
}

void rec_smolist::insert(rec_smolink *p) {
    assert(p->next == 0);

    if (last !=	0) {	// at least one entry exists
	CAMLIB_MODIFY(p->next, last->next);
	CAMLIB_MODIFY(last->next, p);
    }
    else {		// no existing entries
      CAMLIB_MODIFY(p->next, p);
	CAMLIB_MODIFY(last, p);
    }
}


void rec_smolist::append(rec_smolink *p) {
    assert(p->next == 0);

    if (last !=	0) {	/* at least one entry exists */
	CAMLIB_MODIFY(p->next, last->next);
	CAMLIB_MODIFY(last->next, p);
	CAMLIB_MODIFY(last, p);
    }
    else {		/* no existing entries */
	CAMLIB_MODIFY(p->next, p);
	CAMLIB_MODIFY(last, p);
    }
}


rec_smolink *rec_smolist::remove(rec_smolink *p) {
    if (last ==	0) return(0);	    // empty list

    rec_smolink *q = last;
    while(q->next != last && q->next != p){
	q =  q->next;
    }
    if (q->next	== p) {			    /* q == prev(p) */
	CAMLIB_MODIFY(q->next, p->next);    // remove p from list
	CAMLIB_MODIFY(p->next, 0);	    // reset p
	if (last == p){		    /* we removed entry at end of list */
	    if (q == p){
		CAMLIB_MODIFY(last, 0);
	    }
	    else {
		CAMLIB_MODIFY(last, q);
	    }
	}
	return(p);
    }
    return(0);			    // not found
}


rec_smolink *rec_smolist::get() {
    if (last ==	0) return(0);	    /* empty list */
    rec_smolink *q = last->next;
    assert(q->next);
    CAMLIB_MODIFY(last->next, q->next);	    // remove head entry
    CAMLIB_MODIFY(q->next, 0);		    // reset removed entry
    if (q == last) {
	CAMLIB_MODIFY(last, 0);	    // there was only one entry
    }
    return(q);
}

int rec_smolist::IsEmpty() {
    if (last == 0) return(1);
    return(0);
}

void rec_smolist::print() {
    print(stderr);
}


void rec_smolist::print(FILE *fp) {
    fflush(fp);
    print(fileno(fp));
}


void rec_smolist::print(int fd) {
    /* first print out the olist header */
    char buf[40];
    sprintf(buf, "%#08x : Default Olist\n", (long)this);
    write(fd, buf, strlen(buf));

    /* then print out all of the olinks */
    rec_smolist_iterator next(*this);
    rec_smolink *p;
    while(p = next()) p->print(fd);
}


rec_smolist_iterator::rec_smolist_iterator(rec_smolist& l) {
    clist = &l;
    clink = l.last;
    nlink = clink ? clink->next : 0;
}

/* If the user of this iterator decides to delete the object we return, we would
 * loose the integrity of the list. Therefore we keep track of the next object we
 * will look at, so if the current object disappears, we still have the next one.
 */
rec_smolink * rec_smolist_iterator::operator()() {
    clink = nlink;

    if (nlink) nlink = nlink->next;

    /* Stop if we've looked at all the objects. */
    if (clink == clist->last) nlink = 0;
    
    return(clink);
}

rec_smolink::rec_smolink() {
/* the user should do initialization */
printf("in rec_smolink constructor - shouldnt be \n");
}


rec_smolink::~rec_smolink() {
    printf("rec_smolink: dtor deleting %x\n", this);
}

void rec_smolink::print() {
    print(stderr);
}


void rec_smolink::print(FILE *fp) {
    fflush(fp);
    print(fileno(fp));
}


void rec_smolink::print(int fd) {
    char buf[40];
    sprintf(buf, "%#08x : Olink next = %d\n", (long)this, next);
    write(fd, buf, strlen(buf));
}
