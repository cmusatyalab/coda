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
 * dhash.c -- Implementation of dhashtab type.
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

#include "dhash.h"
#include "dlist.h"

/* DEBUGGING! -JJK */
/*
extern FILE *logFile;
extern void Die(char * ...);
*/


dhashtab::dhashtab(int size, int (*hashfn)(void *), int (*CF)(dlink *, dlink *)) {
    /* Ensure that size is a power of 2 so that we can use "AND" for modulus division. */
    /*CODA_ASSERT(size > 0);*/ if (size <= 0) abort();
    for (sz = 1; sz < size; sz *= 2) ;
    /*CODA_ASSERT(sz == size);*/ if (sz != size) abort();

    /* Allocate and initialize the array. */
    /* XXXX - hack - munge with internals to set up the compare function */
    a = new dlist[sz];
    for (int i = 0; i < sz; i++)
	a[i].CmpFn = CF;

    /* Store the hash function. */
    hfn = hashfn;

    cnt = 0;
}

dhashtab::dhashtab(dhashtab& ht) {
    abort();
}


dhashtab::operator=(dhashtab& ht) {
    abort();
    return(0); /* to keep C++ happy !! */
}


dhashtab::~dhashtab() {
    /* This is dangerous! */
    /* Perhaps we should abort() if count() != 0?  -JJK */
    clear();

    delete[] a;
}


void dhashtab::insert(void *key, dlink *p) {
    int bucket = hfn(key) & (sz - 1);
    a[bucket].insert(p);
    cnt++;
}

void dhashtab::prepend(void *key, dlink *p) {
    int bucket = hfn(key) & (sz - 1);
    a[bucket].prepend(p);
    cnt++;
}


void dhashtab::append(void *key, dlink *p) {
    int bucket = hfn(key) & (sz - 1);
    a[bucket].append(p);
    cnt++;
}


dlink *dhashtab::remove(void *key, dlink *p) {
    int bucket = hfn(key) & (sz - 1);
    return(cnt--, a[bucket].remove(p));
}


dlink *dhashtab::first() {
    if (cnt == 0) return(0);

    for (int i = 0; i < sz; i++) {
	dlink *p = a[i].first();
	if (p != 0) return(p);
    }
    abort();
    return(0); /* dummy, to keep g++ happy */
/*    print(logFile); Die("dhashtab::first: cnt > 0 but no object found");*/
}


dlink *dhashtab::last() {
    if (cnt == 0) return(0);

    for (int i = sz - 1; i >= 0; i--) {
	dlink *p = a[i].last();
	if (p != 0) return(p);
    }
    abort();
    return(0); /* dummy to keep g++ happy */
/*    print(logFile); Die("dhashtab::last: cnt > 0 but no object found");*/
}


dlink *dhashtab::get(void *key, DlGetType type) {
    if (cnt == 0) return(0);

    int bucket = hfn(key) & (sz - 1);
    return(cnt--, a[bucket].get(type));
}


void dhashtab::clear() {
    /* Clear all the olists. */
    for (int i = 0; i < sz; i++) a[i].clear();
    cnt = 0;
}

int dhashtab::count() {
    return(cnt);
}

int dhashtab::IsMember(void *key, dlink *p) {
    int bucket = hfn(key) & (sz - 1);
    return(a[bucket].IsMember(p));
}

int dhashtab::bucket(void *key) {
    return(hfn(key) & (sz - 1));
}

void dhashtab::print() {
    print(stderr);
}


void dhashtab::print(FILE *fp) {
    fflush(fp);
    print(fileno(fp));
}


void dhashtab::print(int fd) {
    /* first print out the dhashtab header */
    char buf[40];
    sprintf(buf, "%#08x : Default dhashtab\n", (long)this);
    write(fd, buf, strlen(buf));

    /* then print out all of the dlists */
    for (int i = 0; i < sz; i++) a[i].print(fd);
}


/* I'm keeping this so that both Order and Key can be defaulted! -JJK */
dhashtab_iterator::dhashtab_iterator(dhashtab& ht, void *key) {
    chashtab = &ht;
    allbuckets = (key == (void *)-1);
    order = DhAscending;
    cbucket = allbuckets
      ? (order == DhAscending ? 0 : chashtab->sz - 1)
      : (chashtab->hfn)(key) & (chashtab->sz - 1);
    nextlink = new dlist_iterator(chashtab->a[cbucket], 
    			((order == DhAscending) ? DlAscending : DlDescending));
}

dhashtab_iterator::dhashtab_iterator(dhashtab& ht, DhIterOrder Order, void *key) {
    chashtab = &ht;
    allbuckets = (key == (void *)-1);
    order = Order;
    cbucket = allbuckets
      ? (order == DhAscending ? 0 : chashtab->sz - 1)
      : (chashtab->hfn)(key) & (chashtab->sz - 1);
    nextlink = new dlist_iterator(chashtab->a[cbucket],
				   order == DhAscending ? DlAscending : DlDescending);
}

dhashtab_iterator::~dhashtab_iterator() {
    delete nextlink;
}

dlink *dhashtab_iterator::operator()() {
    for (;;) {
	/* Take next entry from the current bucket. */
	dlink *l = (*nextlink)();
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
	nextlink = new dlist_iterator(chashtab->a[cbucket],
				      order == DhAscending ? DlAscending : DlDescending);
    }
}
