/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
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
 * ohash.c -- Implementation of ohashtab type.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

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
    /*CODA_ASSERT(size > 0);*/ if (size <= 0) abort();
    for (sz = 1; sz < size; sz *= 2) ;
    /*CODA_ASSERT(sz == size);*/ if (sz != size) abort();

    /* Allocate and initialize the array. */
    a = new olist[sz];

    /* Store the hash function. */
    hfn = hashfn;

    cnt = 0;
}


ohashtab::ohashtab(ohashtab& ht) {
    abort();
}


int ohashtab::operator=(ohashtab& ht) {
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
    print(stderr);
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
