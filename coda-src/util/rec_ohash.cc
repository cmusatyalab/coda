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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/util/rec_ohash.cc,v 4.2 1997/02/26 16:03:06 rvb Exp $";
#endif /*_BLURB_*/









/*
 *
 *    rec_ohash.c -- Implementation of recoverable ohashtab type.
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

#include "rec_ohash.h"

/* DEBUGGING! -JJK */
/*
extern FILE *logFile;
extern void Die(char * ...);
*/

void *rec_ohashtab::operator new(size_t size) {
    rec_ohashtab *r = 0;

    r = (rec_ohashtab *)RVMLIB_REC_MALLOC(size);
    assert(r);
    return(r);
}

void rec_ohashtab::operator delete(void *deadobj, size_t size) {
	RVMLIB_REC_FREE(deadobj);
}

rec_ohashtab::rec_ohashtab(int hashtabsize, RHFN hashfn) {
    rec_ohashtab::Init(hashtabsize, hashfn);
}

rec_ohashtab::~rec_ohashtab() {
    DeInit();
}

void rec_ohashtab::Init(int hashtabsize, RHFN hashfn) {
    RVMLIB_REC_OBJECT(*this);

    /* Ensure that hashtabsize is a power of 2 so that we can use "AND" for modulus division. */
    if (hashtabsize <= 0) abort();
    for (sz = 1; sz < hashtabsize; sz *= 2) ;
    if (sz != hashtabsize) abort();

    /* Allocate and initialize the array. */
    /* N.B. Normal vector construction won't work because RECOVERABLE vector must be allocated! */
    {
	a = (rec_olist *)RVMLIB_REC_MALLOC(sz * sizeof(rec_olist));
    
	for (int bucket = 0; bucket < sz; bucket++)
	    a[bucket].Init();
    }
    /* Store the hash function. */
    hfn = hashfn;

    cnt = 0;
}

void rec_ohashtab::DeInit() {
    RVMLIB_REC_OBJECT(*this);

    /* Free up array. */
    {
	for (int bucket = 0; bucket < sz; bucket++)
	    a[bucket].DeInit();

	RVMLIB_REC_FREE(a);
    }
}


rec_ohashtab::rec_ohashtab(rec_ohashtab& ht) {
    abort();
}


rec_ohashtab::operator=(rec_ohashtab& ht) {
    abort();
    return(0); /* keep C++ happy */
}




/* The hash function is not necessarily recoverable, so don't insist on an enclosing transaction! */
void rec_ohashtab::SetHFn(RHFN hashfn) {
    if (RVM_THREAD_DATA->tid != 0)
	RVMLIB_REC_OBJECT(*this);
    hfn = hashfn;
}


void rec_ohashtab::insert(void *key, rec_olink *p) {
    RVMLIB_REC_OBJECT(*this);
    int bucket = hfn(key) & (sz - 1);
    a[bucket].insert(p);
    cnt++;
}


void rec_ohashtab::append(void *key, rec_olink *p) {
    RVMLIB_REC_OBJECT(*this);
    int bucket = hfn(key) & (sz - 1);
    a[bucket].append(p);
    cnt++;
}


rec_olink *rec_ohashtab::remove(void *key, rec_olink *p) {
    RVMLIB_REC_OBJECT(*this);
    int bucket = hfn(key) & (sz - 1);
    return(cnt--, a[bucket].remove(p));
}


rec_olink *rec_ohashtab::first() {
    if (cnt == 0) return(0);

    for (int i = 0; i < sz; i++) {
	rec_olink *p = a[i].first();
	if (p != 0) return(p);
    }
    abort();
    return(0); /* keep g++ happy */
}


rec_olink *rec_ohashtab::last() {
    if (cnt == 0) return(0);

    for (int i = sz - 1; i >= 0; i--) {
	rec_olink *p = a[i].last();
	if (p != 0) return(p);
    }
    abort();
    return(0); /* keep g++ happy */
}


rec_olink *rec_ohashtab::get(void *key) {
    RVMLIB_REC_OBJECT(*this);
    int bucket = hfn(key) & (sz - 1);
    return(cnt--, a[bucket].get());
}


int rec_ohashtab::count() {
    return(cnt);
}


int rec_ohashtab::IsMember(void *key, rec_olink *p) {
    int bucket = hfn(key) & (sz - 1);
    return(a[bucket].IsMember(p));
}


int rec_ohashtab::bucket(void *key) {
    return(hfn(key) & (sz - 1));
}


void rec_ohashtab::print() {
    print(stdout);
}


void rec_ohashtab::print(FILE *fp) {
    fflush(fp);
    print(fileno(fp));
}


void rec_ohashtab::print(int fd) {
    /* first print out the rec_ohashtab header */
    char buf[40];
    sprintf(buf, "%#08x : Default rec_ohashtab\n", (long)this);
    write(fd, buf, strlen(buf));

    /* then print out all of the rec_olists */
    for (int i = 0; i < sz; i++) a[i].print(fd);
}


rec_ohashtab_iterator::rec_ohashtab_iterator(rec_ohashtab& ht, void *key) {
    chashtab = &ht;
    allbuckets = (key == (void *)-1);
    cbucket = (allbuckets ? 0 : chashtab->bucket(key));
    nextlink = new rec_olist_iterator(chashtab->a[cbucket]);
}

void rec_ohashtab_iterator::Reset() {
  if (allbuckets)
    cbucket = 0;
  delete nextlink;
  nextlink = new rec_olist_iterator(chashtab->a[cbucket]);
}

rec_ohashtab_iterator::~rec_ohashtab_iterator() {
    delete nextlink;
}


rec_olink *rec_ohashtab_iterator::operator()() {
    for (;;) {
	/* Take next entry from the current bucket. */
	rec_olink *l = (*nextlink)();
	if (l) return(l);

	/* Can we continue with the next bucket? */
	if (!allbuckets) return(0);

	/* Try the next bucket. */
	if (++cbucket >= chashtab->sz) return(0);
	delete nextlink;
	nextlink = new rec_olist_iterator(chashtab->a[cbucket]);
    }
}
