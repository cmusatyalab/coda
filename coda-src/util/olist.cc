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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/util/olist.cc,v 4.4 1998/06/11 14:40:08 jaharkes Exp $";
#endif /*_BLURB_*/









/*
 *
 * olist.c -- Implementation of olist type.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include "coda_assert.h"
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
    olink *q, *prev;
    
    if (tail ==	0) 
	return(0);	    // empty list

    prev = tail;
    q = tail->next;
    while ( q != tail && q != p) {
	prev = q;
	q = q->next;
    }

    // 3 cases: p found and p is not the only element in olist
    //          p found and p is the only element in the olist
    //          p not found
	
    if ( q == p ) {
	prev->next = p->next;
	p->next = 0;
	cnt--;
	if (tail == p) { // we have removed tail, reassign or 
			 // set tail = 0 if olist is now empty
	    tail = ( prev == tail ) ? 0 : prev; 
	}
	return p;
    } else {
	return(0);      // not found 
    }
    // not reached
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
    print(stderr);
}


void olist::print(FILE *fp) {
    fflush(fp);
    print(fileno(fp));
}


void olist::print(int fd) {
    /* first print out the olist header */
    char buf[1000];
    sprintf(buf, "this: %#08x, tail: %#08x : Default Olist : count = %d\n",
	    (long)this, (long)this->tail, cnt);
    write(fd, buf, strlen(buf));

    /* then print out all of the olinks */
    olist_iterator next(*this);
    olink *p;
    while(p = next()) p->print(fd);
}


olist_iterator::olist_iterator(olist& l) {
    clist = &l;
    clink = (olink *)-1;
    nlink = (olink *)-1;
}


olink *olist_iterator::operator()() {
    if ( clist->last() == 0 )	{/* empty list */
	clink = (olink *)-1;
	nlink = (olink *)-1;
	return 0;
    }
    
    switch((unsigned int)clink) {
    case -1:		/* state == NOTSTARTED */
	clink = clist->first();
	if ( clink != 0 ) { 
	    nlink = clink->next; /* clink may be deleted before next iter.,
				    keep ptr to next olink now */
	    return clink;
	} else {
	    nlink = (olink *)-1;
	    return 0;
	}

    case 0:		/* not reached */
	return 0;

    default:		/* state == INPROGRESS */
	if (clink == clist->last()) {	/* last already done */
	    clink = (olink *)0;
	    nlink = (olink *)-1;
	    return 0;
	}
	CODA_ASSERT(nlink != (olink *)-1);
	clink = nlink;		/* we saved nlink in last iteration */
	nlink = clink->next;	/* clink may be del. before next iter.,
				   keep ptr to next olink now */
	return clink;
    }

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
    print(stderr);
}


void olink::print(FILE *fp) {
    fflush(fp);
    print(fileno(fp));
}


void olink::print(int fd) {
    char buf[80];
    sprintf(buf, "this: %#08x , next: %#08x: Default Olink\n",
	    (long)this,(long)this->next);
    write(fd, buf, strlen(buf));
}
