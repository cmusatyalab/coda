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

static char *rcsid = "$Header: blurb.doc,v 1.1 96/11/22 13:29:31 raiff Exp $";
#endif /*_BLURB_*/




#ifdef __cplusplus
extern "C" {
#endif __cplusplus
#ifdef __MACH__
#include <libc.h>
#endif __MACH__

#ifdef __NetBSD__
#include <stdlib.h>
#endif __NetBSD__

#include <stdio.h>
#ifdef __cplusplus
}
#endif __cplusplus

#ifdef LINUX
#include "util.h"
#else
#include <util.h>
#endif
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
	assert(list);
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
    assert(newlist);
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

    assert(cursize < maxsize);

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
