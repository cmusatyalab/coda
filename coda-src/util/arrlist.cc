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

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef __BSD44__
#include <stdlib.h>
#endif /* __BSD44__ */

#include <stdio.h>
#ifdef __cplusplus
}
#endif __cplusplus

#include "util.h"
#include "arrlist.h"

arrlist::arrlist(int msz) {
    init(msz);
}
arrlist::~arrlist() {
    if (list)
	free(list);
    list = NULL;
    maxsize = 0;
    cursize = 0;
}

arrlist::arrlist() {
    init(32);
}
void arrlist::init(int msz) {
    maxsize = msz;
    cursize = 0;
    if (maxsize > 0) {
	list = (void **)malloc(sizeof(void *) * maxsize);
	CODA_ASSERT(list);
    }
    else list = NULL;

    for (int i = 0; i < maxsize; i++)
	list[i] = NULL;

}
int arrlist::Grow(int increase) {
    int i, newsize;
    if (increase == 0)
	newsize = maxsize ? (maxsize * 2) : 32;
    else 
	newsize = maxsize + increase;
    void **newlist = (void **)malloc(sizeof(void *) * newsize);
    CODA_ASSERT(newlist);
    for (i = 0; i < maxsize; i++)
	newlist[i] = list[i];
    for (; i < newsize; i++) 
	newlist[i] = NULL;
    free(list);

    list = newlist;
    maxsize = newsize;
    return(newsize);
}

void arrlist::add(void *p) {
    if (cursize >= maxsize) 
	// array is full - need to grow
	Grow();

    CODA_ASSERT(cursize < maxsize);

    list[cursize] = p;
    cursize++;
}

arrlist_iterator::arrlist_iterator(arrlist *p) {
    alp = p;
    previndex = -1;
}

arrlist_iterator::~arrlist_iterator() {
    alp = NULL;
    previndex = -1;
}

void *arrlist_iterator::operator()() {
    if (previndex < (alp->cursize - 1)) {
	previndex++;
	return(alp->list[previndex]);
    }
    else  return(NULL); 
}
