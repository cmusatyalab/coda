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

static char *rcsid = "$Header: /afs/cs.cmu.edu/project/coda-braam/src/coda-4.0.1/RCSLINK/./coda-src/util/dlist.cc,v 1.1 1996/11/22 19:08:05 braam Exp $";
#endif /*_BLURB_*/








/*
 *
 * dlist.c -- Implementation of dlist type.
  *	Doubly linked circular list with head pointing to the first element
 *
 */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#ifdef __MACH__
#include <libc.h>
#include <sysent.h>
#endif /* __MACH__ */
#if __NetBSD__  || LINUX
#include <unistd.h>
#include <stdlib.h>
#endif __NetBSD__

#ifdef __cplusplus
}
#endif __cplusplus


#include "dlist.h"


/* DEBUGGING! -JJK */
/*
extern FILE *logFile;
extern void Die(char * ...);
*/


dlist::dlist()
{
    head = 0;
    cnt = 0;
    CmpFn = 0;
}

dlist::dlist(CFN F){
    head = 0;
    cnt = 0;
    CmpFn = F;
}


dlist::~dlist() {
    /* This is dangerous! */
    /* Perhaps we should abort() if count() != 0?  -JJK */
    clear();
}

/* insert in appropriate position, maintaining the sorted order */
void dlist::insert(dlink *p) 
{
    dlink *dl;
    if ((p->next != 0) || (p->prev != 0)) abort();
/*	{ print(logFile); p->print(logFile); Die("dlist::insert: link != 0"); }*/

    if (head !=	0) {	/* at least one entry exists */
	/* find the point of insertion */
	dl = head;
	if (CmpFn)
	    while(CmpFn(dl, p) <= 0){
		dl = dl->next;
		if (dl == head){
		    /* gone through entire list  - insert at end */
		    dl = 0;
		    break;
		}
	    }
	/* if CmpFn is NULL, insert will insert at the beginning of the list */
	if (dl) {
	    /* insert before dl */
	    p->next = dl;
	    p->prev = dl->prev;
	    dl->prev->next = p;
	    dl->prev = p;
	    if (dl == head) 
		head = p;
	}
	else {
	    /* insert at end of list */
	    dl = head->prev;
	    dl->next = p;
	    p->prev = dl;
	    p->next = head;
	    head->prev = p;
	}
    }	    
    else{		/* no existing entries */
	head = p;
	p->next = p->prev = p;
    }

    cnt++;
}

/* add at beginning of list */
void dlist::prepend(dlink *p)
{   
    dlink *dl;
    if ((p->next != 0) || (p->prev != 0)) abort();
/*	{ print(logFile); p->print(logFile); Die("dlist::prepend: link != 0"); }*/

    if (head == 0){
	head = p;
	p->next = p->prev = p;
    }
    else {
	dl = head;
	head = p;
	p->next = dl;
	p->prev = dl->prev;
	dl->prev->next = p;
	dl->prev = p;
    }

    cnt++;
}

/* add at the end of dlist */
void dlist::append(dlink *p) 
{
    dlink   *dl;
    if ((p->next != 0) || (p->prev != 0)) abort();
/*	{ print(logFile); p->print(logFile); Die("dlist::append: link != 0"); }*/

    if (head == 0){
	head = p;
	p->next = p->prev = p;
    }
    else {
	dl = head->prev;
	dl->next = p;
	p->prev = dl;
	p->next = head;
	head->prev = p;
    }

    cnt++;
}



dlink *dlist::remove(dlink *p) {
    if (head ==	0) return(0);	    /* empty list */

    if (p->next == p){
	/* only one element */
	head = 0;
	p->next = p->prev = 0;
	cnt--;
	return(p);
    }
    /* remove the element */
    p->next->prev = p->prev;
    p->prev->next = p->next;

    if (head == p)
	/* remove head of list */
	head = p->next;

    p->prev = p->next = 0;
    cnt--;
    return(p);			    
}


dlink *dlist::first() {
    return(head);
}


dlink *dlist::last() {
    return(head == 0 ? 0 : head->prev);
}


dlink *dlist::get(DlGetType type) 
{
    if (head ==	0) return(0);	    /* empty list */

    return(remove(type == DlGetMin ? head : head->prev));
}


void dlist::clear() 
{
    dlink *p;
    while(p = get()) ;
    if (cnt != 0) abort();
/*	{ print(logFile); Die("dlist::clear: cnt != 0 after gets"); }*/
}


int dlist::count() {
    return(cnt);
}


int dlist::IsMember(dlink *p) {
    dlist_iterator next(*this);
    dlink *dl;
    while (dl = next()) {
	int res = (CmpFn ? CmpFn(dl, p) : !(dl == p));
	if (res == 0) return(1);

	if (res > 0) break;
    }
    return(0);
}


void dlist::print() {
    print(stdout);
}


void dlist::print(FILE *fp) {
    fflush(fp);
    print(fileno(fp));
}


void dlist::print(int fd) {
    /* first print out the dlist header */
    char buf[80];
    sprintf(buf, "%#08x : Default Dlist : count = %d\n",
	     (long)this, cnt);
    write(fd, buf, strlen(buf));

    /* then print out all of the olinks */
    dlist_iterator next(*this);
    dlink *p;
    while(p = next()) p->print(fd);
}

dlist_iterator::dlist_iterator(dlist& l, DlIterOrder Order) 
{
    cdlist = &l;
    cdlink = (dlink *)-1;
    order = Order;
}

dlink * dlist_iterator::operator() ()
{
    switch((unsigned int)cdlink) {
	case -1:		/* state == NOTSTARTED */
	    if (order == DlAscending) {
		cdlink = cdlist->first();
	    }
	    else {
		cdlink = cdlist->last();
	    }
	    break;

	default:		/* state == INPROGRESS */
	    if (order == DlAscending) {
		cdlink = cdlink->next;
		if (cdlink == cdlist->first())
		    cdlink = 0;
	    }
	    else {
		cdlink = cdlink->prev;
		if (cdlink == cdlist->last())
		    cdlink = 0;
	    }
	    break;

	case 0:			/* state == FINISHED */
	    break;
    }

    return(cdlink);
}

dlink::dlink() 
{
    next = 0;
    prev = 0;
}


dlink::~dlink() 
{
}


void dlink::print() {
    print(stdout);
}


void dlink::print(FILE *fp) {
    fflush(fp);
    print(fileno(fp));
}


void dlink::print(int fd) {
    char buf[80];
    sprintf(buf, "%#08x : Default Dlink : prev = %x, next = %x\n",
	     (long)this, prev, next);
    write(fd, buf, strlen(buf));
}
