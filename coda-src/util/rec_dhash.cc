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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/util/rec_dhash.cc,v 4.1 1997/01/08 21:51:07 rvb Exp $";
#endif /*_BLURB_*/









/*
 *
 * rec_dhash.c -- Implementation of rec_dhashtab type.
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
#ifdef __NetBSD__
#include <unistd.h>
#include <stdlib.h>
#endif __NetBSD__

#include <setjmp.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include "rec_dhash.h"


/* DEBUGGING! -JJK */
/*
extern FILE *logFile;
extern void Die(char * ...);
*/


void *rec_dhashtab::operator new(size_t size) {
    rec_dhashtab *r = 0;
    r = (rec_dhashtab *)RVMLIB_REC_MALLOC(sizeof(rec_dhashtab));
    assert(r);
    return(r);
}

void rec_dhashtab::operator delete(void *deadobj, size_t size) {
    RVMLIB_REC_FREE(deadobj);
}

rec_dhashtab::rec_dhashtab(int hashtabsize, RHFN hashfn, RCFN CF) {
    Init(hashtabsize, hashfn, CF);
}

rec_dhashtab::~rec_dhashtab() {
    DeInit();
}

void rec_dhashtab::Init(int hashtabsize, RHFN hashfn, RCFN CF) {
    /* Ensure that hashtabsize is a power of 2 so that we can use "AND" for modulus division. */
    if (hashtabsize <= 0) abort();
    for (sz = 1; sz < hashtabsize; sz *= 2) ;
    if (sz != hashtabsize) abort();

    RVMLIB_REC_OBJECT(*this);

    /* Allocate and initialize the array. */
    /* N.B. Normal vector construction won't work because RECOVERABLE vector must be allocated! */
    {
	a = (rec_dlist *)RVMLIB_REC_MALLOC(sz * sizeof(rec_dlist));
	
	for (int bucket = 0; bucket < sz; bucket++)
	    a[bucket].Init(CF);
    }

    /* Store the hash function. */
    hfn = hashfn;

    cnt = 0;
}

rec_dhashtab::rec_dhashtab(rec_dhashtab& ht) {
    abort();
}

rec_dhashtab::operator=(rec_dhashtab& ht) {
    abort();
    return(0); /* keep C++ happy */
}

void rec_dhashtab::DeInit() {
    RVMLIB_REC_OBJECT(*this);
    
    /* free up array */
    {
	for (int bucket = 0; bucket < sz; bucket++)
	    a[bucket].DeInit();

	RVMLIB_REC_FREE(a);
    }
    
    a = NULL;
    sz = 0;
    hfn = NULL;
    cnt = 0;
}

/* The hash function is not necessarily recoverable, so don't insist on an enclosing transaction! */
void rec_dhashtab::SetHFn(RHFN hashfn) {
    if (RVM_THREAD_DATA->tid != 0)
	RVMLIB_REC_OBJECT(*this);
    hfn = hashfn;
}

/* The compare function is not necessarily recoverable, so don't insist on an enclosing transaction! */
void rec_dhashtab::SetCmpFn(RCFN F) {
    for (int i = 0; i < sz; i++)
	a[i].SetCmpFn(F);
}

void rec_dhashtab::insert(void *key, rec_dlink *p) {
    RVMLIB_REC_OBJECT(*this);
    int bucket = hfn(key) & (sz - 1);
    a[bucket].insert(p);
    cnt++;
}

void rec_dhashtab::prepend(void *key, rec_dlink *p) {
    RVMLIB_REC_OBJECT(*this);    
    int bucket = hfn(key) & (sz - 1);
    a[bucket].prepend(p);
    cnt++;
}


void rec_dhashtab::append(void *key, rec_dlink *p) {
    RVMLIB_REC_OBJECT(*this);    
    int bucket = hfn(key) & (sz - 1);
    a[bucket].append(p);
    cnt++;
}


rec_dlink *rec_dhashtab::remove(void *key, rec_dlink *p) {
    RVMLIB_REC_OBJECT(*this);    
    int bucket = hfn(key) & (sz - 1);
    return(cnt--, a[bucket].remove(p));
}


rec_dlink *rec_dhashtab::first() {
    if (cnt == 0) return(0);

    for (int i = 0; i < sz; i++) {
	rec_dlink *p = a[i].first();
	if (p != 0) return(p);
    }
    abort();
    return(0); /* keep g++ happy */
}


rec_dlink *rec_dhashtab::last() {
    if (cnt == 0) return(0);

    for (int i = sz - 1; i >= 0; i--) {
	rec_dlink *p = a[i].last();
	if (p != 0) return(p);
    }
    abort();
    return(0); /* keep g++ happy */
}


rec_dlink *rec_dhashtab::get(void *key, DlGetType type) {
    if (cnt == 0) return(0);

    RVMLIB_REC_OBJECT(*this);
    int bucket = hfn(key) & (sz - 1);
    return(cnt--, a[bucket].get(type));
}


int rec_dhashtab::count() {
    return(cnt);
}

int rec_dhashtab::IsMember(void *key, rec_dlink *p) {
    int bucket = hfn(key) & (sz - 1);
    return(a[bucket].IsMember(p));
}

int rec_dhashtab::bucket(void *key) {
    return(hfn(key) & (sz - 1));
}

void rec_dhashtab::print() {
    print(stdout);
}


void rec_dhashtab::print(FILE *fp) {
    fflush(fp);
    print(fileno(fp));
}


void rec_dhashtab::print(int fd) {
    /* first print out the rec_dhashtab header */
    char buf[40];
    sprintf(buf, "%#08x : Default rec_dhashtab\n", (long)this);
    write(fd, buf, strlen(buf));

    /* then print out all of the dlists */
    for (int i = 0; i < sz; i++) a[i].print(fd);
}


rec_dhashtab_iterator::rec_dhashtab_iterator(rec_dhashtab& ht, DhIterOrder Order, void *key) {
    chashtab = &ht;
    allbuckets = (key == (void *)-1);
    order = Order;
    cbucket = allbuckets
      ? (order == DhAscending ? 0 : chashtab->sz - 1)
      : (chashtab->bucket(key));
    nextlink = new rec_dlist_iterator(chashtab->a[cbucket],
				      order == DhAscending ? DlAscending : DlDescending);
}

rec_dhashtab_iterator::~rec_dhashtab_iterator() {
    delete nextlink;
}

rec_dlink *rec_dhashtab_iterator::operator()() {
    for (;;) {
	/* Take next entry from the current bucket. */
	rec_dlink *l = (*nextlink)();
	if (l) return(l);

	/* Can we continue with the next bucket? */
	if (!allbuckets) return(0);

	/* Try the next bucket. */
	if (order == DhAscending) {
	    if (++cbucket >= chashtab->sz) return(0);
	}
	else {
	    if (--cbucket < 0) return(0);
	}
	delete nextlink;
	nextlink = new rec_dlist_iterator(chashtab->a[cbucket],
					  order == DhAscending ? DlAscending : DlDescending);
    }
}
