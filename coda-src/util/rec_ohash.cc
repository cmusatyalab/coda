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
 *    rec_ohash.c -- Implementation of recoverable ohashtab type.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <unistd.h>
#include <stdlib.h>
#include <setjmp.h>
#include <stdio.h>

#include <rvmlib.h>
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

    r = (rec_ohashtab *)rvmlib_rec_malloc(size);
    CODA_ASSERT(r);
    return(r);
}

void rec_ohashtab::operator delete(void *deadobj, size_t size) {
	rvmlib_rec_free(deadobj);
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
	a = (rec_olist *)rvmlib_rec_malloc(sz * sizeof(rec_olist));
    
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

	rvmlib_rec_free(a);
    }
}


rec_ohashtab::rec_ohashtab(rec_ohashtab& ht) {
    abort();
}


int rec_ohashtab::operator=(rec_ohashtab& ht) {
    abort();
    return(0); /* keep C++ happy */
}




/* The hash function is not necessarily recoverable, so don't insist on an enclosing transaction! */
void rec_ohashtab::SetHFn(RHFN hashfn) {
    if (rvmlib_thread_data()->tid != 0)
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
    print(stderr);
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
