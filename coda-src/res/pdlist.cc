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

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/coda-src/res/pdlist.cc,v 1.2 1997/01/07 18:41:03 rvb Exp";
#endif /*_BLURB_*/







/* 
 * pdlist.c
 * implement the portable doubly linked list interface
 *
 */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <libc.h>
#include <sys/types.h>
#include <netinet/in.h>

#ifdef	__linux__
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#endif

#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>
#include "logalloc.h"
#include "pdlist.h"


pdlist::pdlist(int o, PMemMgr *s) {
    head = -1;
    cnt = 0;
    storageMgr = s;
    offset = o;		/* offset of the link field in storage class */
}

pdlist::pdlist(int o, PMemMgr *s, int count, int hd) {
    head = hd;
    cnt = count;
    storageMgr = s;
    offset = o;		/* offset of the link field in storage class */
}

pdlist::~pdlist() {
    head = -1;
    cnt = 0;
    storageMgr = NULL;
}

void pdlist::prepend(pdlink *p) {
    if ((p->next != -1) || (p->prev != -1)) abort();
    int pindex = storageMgr->AddrToIndex((char *)p);

    if (head != -1) {
	/* non-empty list */
	p->next = head;
	pdlink *ahead = (pdlink *)((char *)storageMgr->IndexToAddr(head) + offset);
	p->prev = ahead->prev;
	pdlink *aprev = (pdlink *)((char *)storageMgr->IndexToAddr(ahead->prev) + offset);
	aprev->next = pindex;
	ahead->prev = pindex;
	head = pindex;
    }
    else {
	/* empty list */
	head = pindex;
	p->next = p->prev = pindex;
    }
    cnt++;
}
void pdlist::append(pdlink *p) {
    LogMsg(9, SrvDebugLevel, stdout,  "pdlist::append(0x%x): Going to append at count = %d", 
	    this, cnt);
    int pindex = storageMgr->AddrToIndex((char *)p);
    if ((p->next != -1) || (p->prev != -1)) abort();
    
    if (head == -1) {
	/* empty list */
	p->next = p->prev = pindex;
	head = pindex;
    }
    else {
	/* non-empty list */
	pdlink *ahead = (pdlink *)((char *)storageMgr->IndexToAddr(head) + offset);
	pdlink *aprev = (pdlink *)((char *)storageMgr->IndexToAddr(ahead->prev) + offset);
	aprev->next = pindex;
	p->prev = ahead->prev;
	p->next = head;
	ahead->prev = pindex;
    }
    cnt++;
}
pdlink* pdlist::remove(pdlink *p) {
    LogMsg(49, SrvDebugLevel, stdout,  "pdlist::remove(0x%x): Going to remove at count = %d",
	    this, cnt);
    if (head == -1) return(0);		/* empty list */
    int pindex = storageMgr->AddrToIndex((char *)p);
    if (p->next == pindex) {
	/* only one element */
	head = -1;
	p->next = p->prev = -1;
	cnt--;
	return(p);
    }
    pdlink *anext = (pdlink *)((char *)storageMgr->IndexToAddr(p->next) + offset);
    pdlink *aprev = (pdlink *)((char *)storageMgr->IndexToAddr(p->prev) + offset);

    /* remove the element */
    anext->prev = p->prev;
    aprev->next = p->next;
    
    if (head == pindex)
	/* remove head of list */
	head = p->next;
    p->prev = p->next = -1;
    cnt--;
    return(p);
}
pdlink *pdlist::get() {
    LogMsg(49, SrvDebugLevel, stdout,  "pdlist::get(0x%x) at count = %d",
	    this, cnt);
    if (head == -1) return(0);
    return(remove((pdlink *)((char *)storageMgr->IndexToAddr(head) + offset)));
}
pdlink *pdlist::first() {
    if (head == -1) return(0);
    return((pdlink *)((char *)storageMgr->IndexToAddr(head) + offset));
}
pdlink *pdlist::last() {
    if (head == -1) return(0);
    pdlink *ahead = (pdlink *)((char *)storageMgr->IndexToAddr(head) + offset);
    return((pdlink *)((char *)storageMgr->IndexToAddr(ahead->prev) + offset));
}

pdlink *pdlist::prev(pdlink *p){
    int index = storageMgr->AddrToIndex((char *)p);
    if (head != -1 && head == index) return(NULL);
    return((pdlink *)((char *)storageMgr->IndexToAddr(p->prev) + offset));
}

int pdlist::count() {
    return(cnt);
}

pdlink::pdlink() {
    next = prev = -1;
}

pdlink::~pdlink() {
    next = prev = -1;
}

void pdlink::init() {
    next = prev = -1;
}

void pdlink::hton() {
    next = htonl(next);
    prev = htonl(prev);
}

void pdlink::ntoh() {
    next = ntohl(next);
    prev = ntohl(prev);
}
pdlist_iterator::pdlist_iterator(pdlist& pdl) {
    LogMsg(49, SrvDebugLevel, stdout,  "pdlist_iterator:: count = %d", pdl.cnt);
    cdlist = &pdl;
    if (pdl.head == -1) cdlink = 0;
    else 
	cdlink = (pdlink *)((char *)(pdl.storageMgr->IndexToAddr(pdl.head)) + pdl.offset);
}

pdlink *pdlist_iterator::it_remove(pdlink *pdl) {
    if (cdlink == pdl) {
	cdlink = (pdlink *)((char *)(cdlist->storageMgr->IndexToAddr(cdlink->next))
			    + cdlist->offset);
	if (cdlink == pdl)
	    /* only one element */
	    cdlink = 0;
	if (cdlist->storageMgr->AddrToIndex((char *)cdlink) == cdlist->head) cdlink = 0;
    }
    return(cdlist->remove(pdl));
}
pdlink *pdlist_iterator::operator()() {
    pdlink *ret = cdlink;
    if (cdlink)
	cdlink = (pdlink *)((char *)(cdlist->storageMgr->IndexToAddr(cdlink->next))
			    + cdlist->offset);
    if (cdlink && cdlist->storageMgr->AddrToIndex((char *)cdlink) == cdlist->head)  
	cdlink = 0;
    return(ret);
}

pdlist_prev_iterator::pdlist_prev_iterator(pdlist& pdl) {
    cdlist = &pdl;
    if (pdl.head == -1) cdlink = 0;
    else{
	pdlink *ahead = (pdlink *)((char *)
				   (pdl.storageMgr->IndexToAddr(pdl.head))
				   + pdl.offset);
	pdlink *atail = (pdlink *)((char *)
				   (pdl.storageMgr->IndexToAddr(ahead->prev)) + 
				   pdl.offset);
	cdlink = (pdlink *)atail;
    }
}

pdlink *pdlist_prev_iterator::it_remove(pdlink *pdl) {
    if (cdlink == pdl){
	cdlink = (pdlink *)((char *)(cdlist->storageMgr->IndexToAddr(cdlink->prev))
			    + cdlist->offset);
	if (cdlink == pdl)
	    /* only one element */
	    cdlink = 0;
	/* check if we have reached the tail */
	pdlink *ahead = (pdlink *)((char *)
				   (cdlist->storageMgr->IndexToAddr(cdlist->head))
				   + cdlist->offset);
	if (cdlink && 
	    cdlist->storageMgr->AddrToIndex((char *)cdlink) == ahead->prev)
	    cdlink = 0;
    }
    return(cdlist->remove(pdl));
}

pdlink *pdlist_prev_iterator::operator()() {
    pdlink *ret = cdlink;

    /* set current pdlink to previous link */
    if (cdlink)
	cdlink = (pdlink *)((char *)
			    (cdlist->storageMgr->IndexToAddr(cdlink->prev))
			    + cdlist->offset);
    /* check if we have reached tail again */
    pdlink *ahead = (pdlink *)((char *)
			       (cdlist->storageMgr->IndexToAddr(cdlist->head))
			       + cdlist->offset);
    if (cdlink && cdlist->storageMgr->AddrToIndex((char *)cdlink) == ahead->prev)
	cdlink = 0;
    return(ret);
}
