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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/util/rec_olist.cc,v 4.2 1997/02/26 16:03:06 rvb Exp $";
#endif /*_BLURB_*/









/*
 *
 *    rec_olist.c -- Implementation of recoverable olist type.
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

#include "rec_olist.h"

/* DEBUGGING! -JJK */
/*
extern FILE *logFile;
extern void Die(char * ...);
*/

void *rec_olist::operator new(size_t size) {
    rec_olist *r = 0;

    r = (rec_olist *)RVMLIB_REC_MALLOC(size);
    assert(r);
    return(r);
}

void rec_olist::operator delete(void *deadobj, size_t size) {
    RVMLIB_REC_FREE(deadobj);
}

rec_olist::rec_olist() {
    Init();
}

rec_olist::~rec_olist() {
    DeInit();
}

void rec_olist::Init() {
  rvm_perthread_t *_rvm_data;
  rvm_return_t ret;
  RVMLIB_REC_OBJECT(*this);
  tail = 0;
  cnt = 0;
}

void rec_olist::DeInit() {
    if (cnt != 0) abort();
}

rec_olist::rec_olist(rec_olist& ol) {
    abort();
}

rec_olist::operator=(rec_olist& ol) {
    abort();
    return(0); /* keep C++ happy */
}

void rec_olist::insert(rec_olink *p) {
    if (p->next != 0) abort();
/*	{ print(logFile); p->print(logFile); Die("rec_olist::insert: p->next != 0"); }*/

    RVMLIB_REC_OBJECT(*this);
    RVMLIB_REC_OBJECT(*p);
    if (tail !=	0) {		// at least one entry exists
        p->next = tail->next;
	RVMLIB_REC_OBJECT(*tail);
	tail->next = p;
    }
    else {			// no existing entries
        p->next = p;
	tail = p;
    }

    cnt++;
}


void rec_olist::append(rec_olink *p) {
    if (p->next != 0) abort();
/*	{ print(logFile); p->print(logFile); Die("rec_olist::append: p->next != 0"); }*/

    RVMLIB_REC_OBJECT(*this);
    RVMLIB_REC_OBJECT(*p);
    if (tail !=	0) {		// at least one entry exists
        p->next = tail->next;
	RVMLIB_REC_OBJECT(*tail);
	tail->next = p;
	tail = p;
    }
    else {			// no existing entries
        p->next = p;
	tail = p;
    }

    cnt++;
}


rec_olink *rec_olist::remove(rec_olink *p) {
    if (tail ==	0) return(0);	    // empty list

    rec_olink *q = tail;
    while(q->next != tail && q->next != p)
	q = q->next;
    if (q->next	== p) {		    // q == prev(p)
        RVMLIB_REC_OBJECT(*q);
	q->next	= p->next;	    // remove p from list
	RVMLIB_REC_OBJECT(*p);
	p->next	= 0;		    // reset p
	RVMLIB_REC_OBJECT(*this);
	if (tail == p)		    // we removed entry at end of list
	    tail = (q == p) ? 0	: q;	// was it the only entry?
	cnt--;
	return(p);
    }

    return(0);			    // not found
}


rec_olink *rec_olist::first() {
    if (tail ==	0) return(0);	    // empty list

    return(tail->next);
}


rec_olink *rec_olist::last() {
    return(tail);
}


rec_olink *rec_olist::get() {
    if (tail ==	0) return(0);	    // empty list

    return(remove(tail->next));
}


int rec_olist::count() {
    return(cnt);
}


int rec_olist::IsMember(rec_olink *p) {
    rec_olist_iterator next(*this);
    rec_olink *ol;
    while (ol = next())
	if (ol == p) return(1);
    return(0);
}


void rec_olist::print() {
    print(stdout);
}


void rec_olist::print(FILE *fp) {
    fflush(fp);
    print(fileno(fp));
}


void rec_olist::print(int fd) {
    /* first print out the rec_olist header */
    char buf[80];
    snprintf(buf, 80, "%#08x : Default rec_olist : count = %d\n",
	     (long)this, cnt);
    write(fd, buf, strlen(buf));

    /* then print out all of the rec_olinks */
    rec_olist_iterator next(*this);
    rec_olink *p;
    while(p = next()) p->print(fd);
}


rec_olist_iterator::rec_olist_iterator(rec_olist& l) {
    clist = &l;
    clink = (rec_olink *)-1;
    nlink = (rec_olink *)-1;
}


rec_olink *rec_olist_iterator::operator()() {
    if ( clist->last() == 0 ) {	/* empty list */
	clink = (rec_olink *)-1;
	nlink = (rec_olink *)-1;
	return 0;
    }
    
    switch((unsigned int)clink) {
	case -1:		/* state == NOTSTARTED */
	    clink = clist->first();
	    if ( clink != 0 ) {
		nlink = clink->next; /* clink may be deleted before next iter.,
				        keep ptr to next rec_olink now */
		return clink;
	    } else {
		nlink = (rec_olink *)-1;
		return 0;
	    }
	    
	    break;

        case 0:			/* not reached */
	    return 0;

	default:		/* state == INPROGRESS */
	    if (clink == clist->last()) { /* last already done */
		clink = (rec_olink *)0;
		nlink = (rec_olink *)-1;
		return 0;
	    }
	    assert(nlink != (rec_olink *)-1);
	    clink = nlink;	/* we saved nlink in last iteration */
	    nlink = clink->next; /* clink may be del. before next iter.,
				    keep ptr to next rec_olink now*/
	    return clink;
    }
    
}


rec_olink::rec_olink() {
    /* There is never any point in calling "new rec_olink"! */
    if (this == 0) abort();

    Init();
}


void rec_olink::Init() {
    RVMLIB_REC_OBJECT(*this)
    next = 0;
}


rec_olink::rec_olink(rec_olink& ol) {
    abort();
}


rec_olink::operator=(rec_olink& ol) {
    abort();
    return(0); /* keep C++ happy */
}


/*
rec_olink::~rec_olink() {
    DeInit();
}


void rec_olink::DeInit() {
}
*/


void rec_olink::print() {
    print(stdout);
}


void rec_olink::print(FILE *fp) {
    fflush(fp);
    print(fileno(fp));
}


void rec_olink::print(int fd) {
    char buf[40];
    snprintf(buf, 40, "%#08x : Default rec_olink\n", (long)this);
    write(fd, buf, strlen(buf));
}
