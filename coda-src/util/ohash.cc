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

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/coda-src/util/ohash.cc,v 1.2 1997/01/07 18:41:36 rvb Exp";
#endif /*_BLURB_*/









/*
 *
 * ohash.c -- Implementation of ohashtab type.
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
#if defined(__linux__) || defined(__NetBSD__)
#include <unistd.h>
#include <stdlib.h>
#endif __NetBSD__

#ifdef __cplusplus
}
#endif __cplusplus

#include "ohash.h"
#include "olist.h"

/* DEBUGGING! -JJK */
/*
extern FILE *logFile;
extern void Die(char * ...);
*/


ohashtab::ohashtab(int size, int (*hashfn)(void *)) {
    /* Ensure that size is a power of 2 so that we can use "AND" for modulus division. */
    /*assert(size > 0);*/ if (size <= 0) abort();
    for (sz = 1; sz < size; sz *= 2) ;
    /*assert(sz == size);*/ if (sz != size) abort();

    /* Allocate and initialize the array. */
    a = new olist[sz];

    /* Store the hash function. */
    hfn = hashfn;

    cnt = 0;
}


ohashtab::ohashtab(ohashtab& ht) {
    abort();
}


ohashtab::operator=(ohashtab& ht) {
    abort();
    return(0);	/* to keep C++ happy !! */
}


ohashtab::~ohashtab() {
    /* This is dangerous! */
    /* Perhaps we should abort() if count() != 0?  -JJK */
    clear();

    delete[] a;
}


void ohashtab::insert(void *key, olink *p) {
    int bucket = hfn(key) & (sz - 1);
    a[bucket].insert(p);
    cnt++;
}


void ohashtab::append(void *key, olink *p) {
    int bucket = hfn(key) & (sz - 1);
    a[bucket].append(p);
    cnt++;
}


olink *ohashtab::remove(void *key, olink *p) {
    int bucket = hfn(key) & (sz - 1);
    return(cnt--, a[bucket].remove(p));
}


olink *ohashtab::first() {
    if (cnt == 0) return(0);

    for (int i = 0; i < sz; i++) {
	olink *p = a[i].first();
	if (p != 0) return(p);
    }
    abort();
    return(0); /* dummy to keep g++ happy */
/*    print(logFile); Die("ohashtab::first: cnt > 0 but no object found");*/
}


olink *ohashtab::last() {
    if (cnt == 0) return(0);

    for (int i = sz - 1; i >= 0; i--) {
	olink *p = a[i].last();
	if (p != 0) return(p);
    }
    abort();
    return(0); /* dummy to keep g++ happy */
/*    print(logFile); Die("ohashtab::last: cnt > 0 but no object found");*/
}


olink *ohashtab::get(void *key) {
    int bucket = hfn(key) & (sz - 1);
    return(cnt--, a[bucket].get());
}


void ohashtab::clear() {
    /* Clear all the olists. */
    for (int i = 0; i < sz; i++) a[i].clear();
    cnt = 0;
}


int ohashtab::count() {
    return(cnt);
}


int ohashtab::IsMember(void *key, olink *p) {
    int bucket = hfn(key) & (sz - 1);
    return(a[bucket].IsMember(p));
}


int ohashtab::bucket(void *key) {
    return(hfn(key) & (sz - 1));
}


void ohashtab::print() {
    print(stdout);
}


void ohashtab::print(FILE *fp) {
    fflush(fp);
    print(fileno(fp));
}


void ohashtab::print(int fd) {
    /* first print out the ohashtab header */
    char buf[40];
    sprintf(buf, "%#08x : Default Ohashtab\n", (long)this);
    write(fd, buf, strlen(buf));

    /* then print out all of the olists */
    for (int i = 0; i < sz; i++) a[i].print(fd);
}


ohashtab_iterator::ohashtab_iterator(ohashtab& ht, void *key) {
    chashtab = &ht;
    allbuckets = (key == (void *)-1);
    cbucket = allbuckets ? 0 : (chashtab->hfn)(key) & (chashtab->sz - 1);
    nextlink = new olist_iterator(chashtab->a[cbucket]);
}


ohashtab_iterator::~ohashtab_iterator() {
    delete nextlink;
}


olink *ohashtab_iterator::operator()() {
    for (;;) {
	/* Take next entry from the current bucket. */
	olink *l = (*nextlink)();
	if (l) return(l);

	/* Can we continue with the next bucket? */
	if (!allbuckets) return(0);

	/* Try the next bucket. */
	if (++cbucket >= chashtab->sz) return(0);
	delete nextlink;
	nextlink = new olist_iterator(chashtab->a[cbucket]);
    }
}
