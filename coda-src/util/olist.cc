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

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/coda-src/util/olist.cc,v 1.2 1997/01/07 18:41:37 rvb Exp";
#endif /*_BLURB_*/









/*
 *
 * olist.c -- Implementation of olist type.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#endif /* __MACH__ */
#if  __NetBSD__ || LINUX
#include <unistd.h>
#include <stdlib.h>
#endif __NetBSD__

#ifdef __cplusplus
}
#endif __cplusplus

#include "olist.h"



/* DEBUGGING! -JJK */
/*
extern FILE *logFile;
extern void Die(char * ...);
*/


olist::olist() {
    tail = 0;
    cnt = 0;
}


olist::olist(olist& ol) {
    abort();
}


olist::operator=(olist& ol) {
    abort();
    return(0);  /* to keep C++ happy!! */
}


olist::~olist() {
    /* This is dangerous! */
    /* Perhaps we should abort() if count() != 0?  -JJK */
    clear();
}


void olist::insert(olink *p) {
    if (p->next != 0) abort();
/*	{ print(logFile); p->print(logFile); Die("olist::insert: p->next != 0"); }*/

    if (tail !=	0) {	// at least one entry exists
	p->next = tail->next;
	tail->next = p;
    }
    else {		// no existing entries
	p->next = p;
	tail = p;
    }

    cnt++;
}


void olist::append(olink *p) {
    if (p->next != 0) abort();
/*	{ print(logFile); p->print(logFile); Die("olist::append: p->next != 0"); }*/

    if (tail !=	0) {	// at least one entry exists
	p->next = tail->next;
	tail->next = p;
	tail = p;
    }
    else {		// no existing entries
	p->next = p;
	tail = p;
    }

    cnt++;
}


olink *olist::remove(olink *p) {
    if (tail ==	0) return(0);	    // empty list

    olink *q = tail;
    while(q->next != tail && q->next != p)
	q = q->next;
    if (q->next	== p) {		    // q == prev(p)
	q->next = p->next;	    // remove p from list
	p->next	= 0;		    // reset p
	if (tail == p)		    // we removed entry at end of list
	    tail = (q == p) ? 0 : q;    // was it the only entry?
	cnt--;
	return(p);
    }

    return(0);			    // not found
}


olink *olist::first() {
    if (tail ==	0) return(0);	    // empty list

    return(tail->next);
}


olink *olist::last() {
    return(tail);
}


olink *olist::get() {
    if (tail ==	0) return(0);	    // empty list

    return(remove(tail->next));
}


void olist::clear() {
    olink *p;
    while(p = get()) ;
    if (cnt != 0) abort();
/*	{ print(logFile); Die("olist::clear: cnt != 0 after gets"); }*/
}


int olist::count() {
    return(cnt);
}


int olist::IsMember(olink *p) {
    olist_iterator next(*this);
    olink *ol;
    while (ol = next())
	if (ol == p) return(1);
    return(0);
}


void olist::print() {
    print(stdout);
}


void olist::print(FILE *fp) {
    fflush(fp);
    print(fileno(fp));
}


void olist::print(int fd) {
    /* first print out the olist header */
    char buf[40];
    sprintf(buf, "%#08x : Default Olist : count = %d\n",
	     (long)this, cnt);
    write(fd, buf, strlen(buf));

    /* then print out all of the olinks */
    olist_iterator next(*this);
    olink *p;
    while(p = next()) p->print(fd);
}


olist_iterator::olist_iterator(olist& l) {
    clist = &l;
    clink = (olink *)-1;
}


olink *olist_iterator::operator()() {
    switch((unsigned int)clink) {
	case -1:		/* state == NOTSTARTED */
	    clink = clist->first();
	    break;

	default:		/* state == INPROGRESS */
	    clink = clink->next;
	    if (clink == clist->first())
		clink = 0;
	    break;

	case 0:			/* state == FINISHED */
	    break;
    }

    return(clink);
}


olink::olink() {
    next = 0;
}


olink::olink(olink& ol) {
    abort();
}


olink::operator=(olink& ol) {
    abort();
    return(0);  /* to keep C++ happy!! */
}


olink::~olink() {
}


void olink::print() {
    print(stdout);
}


void olink::print(FILE *fp) {
    fflush(fp);
    print(fileno(fp));
}


void olink::print(int fd) {
    char buf[40];
    sprintf(buf, "%#08x : Default Olink\n", (long)this);
    write(fd, buf, strlen(buf));
}
