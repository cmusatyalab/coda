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
 * rec_smolist.c -- Implementation of rec_smolist type.
 *
 *                  Read comment in rec_smolist.h for relationship
 *		    to rec_olist.c (Satya, 5/31/95)
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#ifdef __cplusplus
}
#endif

#include "util.h"
#include "rvmlib.h"
#include "rec_smolist.h"


rec_smolist::rec_smolist() {
/* the user of this package should do their own initialization */
}

rec_smolist::~rec_smolist() {
}

void rec_smolist::insert(struct rec_smolink *p)
{
    CODA_ASSERT(p->next == 0);

    if (last !=	0) {	// at least one entry exists
	RVMLIB_MODIFY(p->next, last->next);
	RVMLIB_MODIFY(last->next, p);
    }
    else {		// no existing entries
	RVMLIB_MODIFY(p->next, p);
	RVMLIB_MODIFY(last, p);
    }
}


void rec_smolist::append(struct rec_smolink *p)
{
    CODA_ASSERT(p->next == 0);

    if (last !=	0) {	/* at least one entry exists */
	RVMLIB_MODIFY(p->next, last->next);
	RVMLIB_MODIFY(last->next, p);
	RVMLIB_MODIFY(last, p);
    }
    else {		/* no existing entries */
	RVMLIB_MODIFY(p->next, p);
	RVMLIB_MODIFY(last, p);
    }
}


struct rec_smolink *rec_smolist::remove(struct rec_smolink *p)
{
    if (last ==	0) return(0);	    // empty list

    struct rec_smolink *q = last;
    while(q->next != last && q->next != p){
	q =  q->next;
    }
    if (q->next	== p) {			    /* q == prev(p) */
	RVMLIB_MODIFY(q->next, p->next);    // remove p from list
	RVMLIB_MODIFY(p->next, 0);	    // reset p
	if (last == p){		    /* we removed entry at end of list */
	    if (q == p){
		RVMLIB_MODIFY(last, 0);
	    }
	    else {
		RVMLIB_MODIFY(last, q);
	    }
	}
	return(p);
    }
    return(0);			    // not found
}


struct rec_smolink *rec_smolist::get(void)
{
    if (last ==	0) return(0);	    /* empty list */
    struct rec_smolink *q = last->next;
    CODA_ASSERT(q->next);
    RVMLIB_MODIFY(last->next, q->next);	    // remove head entry
    RVMLIB_MODIFY(q->next, 0);		    // reset removed entry
    if (q == last) {
	RVMLIB_MODIFY(last, 0);	    // there was only one entry
    }
    return(q);
}

int rec_smolist::IsEmpty(void)
{
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
    sprintf(buf, "%p : Default Olist\n", this);
    write(fd, buf, strlen(buf));

    /* then print out all of the olinks */
    rec_smolist_iterator next(*this);
    struct rec_smolink *p;
    while((p = next()))
	rec_smolink_print(p, fd);
}


rec_smolist_iterator::rec_smolist_iterator(rec_smolist& l)
{
    clist = &l;
    clink = l.last;
    nlink = clink ? clink->next : 0;
}

/* If the user of this iterator decides to delete the object we return, we would
 * loose the integrity of the list. Therefore we keep track of the next object we
 * will look at, so if the current object disappears, we still have the next one.
 */
struct rec_smolink *rec_smolist_iterator::operator()()
{
    clink = nlink;

    if (nlink) nlink = nlink->next;

    /* Stop if we've looked at all the objects. */
    if (clink == clist->last) nlink = 0;
    
    return(clink);
}

void rec_smolink_print(struct rec_smolink *l, int fd)
{
    char buf[40];
    sprintf(buf, "%p : Olink next = %p\n", l, l->next);
    write(fd, buf, strlen(buf));
}
