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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/util/rec_dlist.cc,v 4.4 1998/08/26 21:13:00 braam Exp $";
#endif /*_BLURB_*/









/*
 *
 * rec_dlist.c -- Implementation of recoverable dlist type.
 *	Doubly linked circular list with head pointing to the first element
 *
 */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <setjmp.h>

#include <rvmlib.h>
#ifdef __cplusplus
}
#endif __cplusplus

#include "rec_dlist.h"

/* DEBUGGING! -JJK */
/*
extern FILE *logFile;
extern void Die(char * ...);
*/


void *rec_dlist::operator new(size_t size) {
    rec_dlist *r = 0;
    
    r = (rec_dlist *)rvmlib_rec_malloc(size);
    CODA_ASSERT(r);
    return(r);
}

void rec_dlist::operator delete(void *deadobj, size_t size) {
	rvmlib_rec_free(deadobj);
}

rec_dlist::rec_dlist(RCFN F){
    Init(F);
}

rec_dlist::~rec_dlist() {
    DeInit();
}


void rec_dlist::Init(RCFN f) {
    RVMLIB_REC_OBJECT(*this);
    head = 0;
    cnt = 0;
    CmpFn = f;
}

void rec_dlist::DeInit() {
    if (cnt != 0) abort();
}

/* The compare function is not necessarily recoverable, so don't insist on an enclosing transaction! */
void rec_dlist::SetCmpFn(RCFN F) {
    if (rvmlib_thread_data()->tid != 0)
	RVMLIB_REC_OBJECT(*this);
    CmpFn = F;
}

/* insert in appropriate position, maintaining the sorted order */
void rec_dlist::insert(rec_dlink *p) 
{
    rec_dlink *dl;
    if ((p->next != 0) || (p->prev != 0)) abort();
/*	{ print(logFile); p->print(logFile); Die("rec_dlist::insert: link != 0"); }*/

    RVMLIB_REC_OBJECT(*this);
    RVMLIB_REC_OBJECT(*p);
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
	    RVMLIB_REC_OBJECT(*dl);
	    RVMLIB_REC_OBJECT(*(dl->prev));
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
	    RVMLIB_REC_OBJECT(*dl);
	    RVMLIB_REC_OBJECT(*head);
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
void rec_dlist::prepend(rec_dlink *p)
{
    rec_dlink *dl;
    if ((p->next != 0) || (p->prev != 0)) abort();
/*	{ print(logFile); p->print(logFile); Die("rec_dlist::prepend: link != 0"); }*/

    RVMLIB_REC_OBJECT(*this);
    RVMLIB_REC_OBJECT(*p);
    if (head == 0){
	head = p;
	p->next = p->prev = p;
    }
    else {
	dl = head;
	RVMLIB_REC_OBJECT(*dl);	
	RVMLIB_REC_OBJECT(*(dl->prev));	
	head = p;
	p->next = dl;
	p->prev = dl->prev;
	dl->prev->next = p;
	dl->prev = p;
    }

    cnt++;
}

/* add at the end of dlist */
void rec_dlist::append(rec_dlink *p) 
{
    rec_dlink   *dl;
    if ((p->next != 0) || (p->prev != 0)) abort();
/*	{ print(logFile); p->print(logFile); Die("rec_dlist::append: link != 0"); }*/

    RVMLIB_REC_OBJECT(*this);
    RVMLIB_REC_OBJECT(*p);
    if (head == 0){
	head = p;
	p->next = p->prev = p;
    }
    else {
	dl = head->prev;
	RVMLIB_REC_OBJECT(*dl);
	RVMLIB_REC_OBJECT(*head);
	dl->next = p;
	p->prev = dl;
	p->next = head;
	head->prev = p;
    }

    cnt++;
}



rec_dlink *rec_dlist::remove(rec_dlink *p) {
    if (head ==	0) return(0);	    /* empty list */

    RVMLIB_REC_OBJECT(*this);    
    RVMLIB_REC_OBJECT(*p);
    
    if (p->next == p){
	/* only one element */
	head = 0;
	p->next = p->prev = 0;
	cnt--;
	return(p);
    }
    /* remove the element */
    RVMLIB_REC_OBJECT(*(p->next));
    RVMLIB_REC_OBJECT(*(p->prev));
    p->next->prev = p->prev;
    p->prev->next = p->next;

    if (head == p)
	/* remove head of list */
	head = p->next;

    p->prev = p->next = 0;
    cnt--;
    return(p);			    
}


rec_dlink *rec_dlist::first() {
    return(head);
}


rec_dlink *rec_dlist::last() {
    return(head == 0 ? 0 : head->prev);
}


rec_dlink *rec_dlist::get(DlGetType type) 
{
    if (head ==	0) return(0);	    /* empty list */
    return(remove(type == DlGetMin ? head : head->prev));
}


int rec_dlist::count() {
    return(cnt);
}


int rec_dlist::IsMember(rec_dlink *p) {
    rec_dlist_iterator next(*this);
    rec_dlink *dl;
    while (dl = next()) {
	int res = (CmpFn ? CmpFn(dl, p) : !(dl == p));
	if (res == 0) return(1);

	if (res > 0) break;
    }
    return(0);
}


void rec_dlist::print() {
    print(stderr);
}


void rec_dlist::print(FILE *fp) {
    fflush(fp);
    print(fileno(fp));
}


void rec_dlist::print(int fd) {
    /* first print out the dlist header */
    char buf[80];
    sprintf(buf, "%#08x : Default Rec_Dlist : count = %d\n",
	     (long)this, cnt);
    write(fd, buf, strlen(buf));

    /* then print out all of the olinks */
    rec_dlist_iterator next(*this);
    rec_dlink *p;
    while(p = next()) p->print(fd);
}

rec_dlist_iterator::rec_dlist_iterator(rec_dlist& l, DlIterOrder Order) 
{
    cdlist = &l;
    cdlink = (rec_dlink *)-1;
    order = Order;
}

rec_dlink * rec_dlist_iterator::operator() ()
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

rec_dlink::rec_dlink() 
{
    RVMLIB_REC_OBJECT(*this);

    Init();
}

void rec_dlink::Init() {
    RVMLIB_REC_OBJECT(*this);
    next = 0;
    prev = 0;
}
    

void rec_dlink::print() {
    print(stderr);
}


void rec_dlink::print(FILE *fp) {
    fflush(fp);
    print(fileno(fp));
}


void rec_dlink::print(int fd) {
    char buf[80];
    sprintf(buf, "%#08x : Default rec_dlink : prev = %x, next = %x\n",
	     (long)this, prev, next);
    write(fd, buf, strlen(buf));
}
