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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/util/bitmap.cc,v 4.1 1997/01/08 21:51:01 rvb Exp $";
#endif /*_BLURB_*/




/*
 * bitmap.c 
 * Created Feb 13, 1992	-- Puneet Kumar
 * Definition for the bitmap class 
 */
#ifdef __cplusplus
extern "C" {
#endif
#include <stdio.h>
#include <string.h>
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
#endif

#include "util.h"
#include "rvmlib.h"
#include "bitmap.h"
extern char* hex(long, int =0);

void *bitmap::operator new(size_t size, int recable) {
    bitmap *x;

    if (recable) {
	x = (bitmap *)RVMLIB_REC_MALLOC(sizeof(bitmap));
	assert(x);
    }
    else {
	x = (bitmap *) malloc(sizeof(bitmap));
	assert(x);
    }
    
    /* GROSS HACK BELOW; if this object was born via new, we
       are confident malloced is set.  But if object was
       allocated on stack, malloced is undefined.  We rely
       on the unlikeliness of the undefined value being exactly
       BITMAP_VIANEW for correctness.  Bletch!!! This would be much
       easier and cleaner if delete() took extra args, just like new()
       Obviously, I disagree with Stroustrup's polemic on this topic on
       pg 66 of the CARM (1st edition, 1991).
       
       Satya, 2/95
       
    */
    x->malloced = BITMAP_VIANEW;  
    return(x);
}


void bitmap::operator delete(void *deadobj, size_t size){
    /* Nothing to do; ~bitmap() has already done deallocation */
    }

bitmap::bitmap(int inputmapsize, int recable) {

    assert(malloced != BITMAP_NOTVIANEW); /* ensure malloced is undefined if via stack! */
    if (malloced != BITMAP_VIANEW) malloced = BITMAP_NOTVIANEW; /* infer I must be on the stack */
    /* From this point on, malloced is definitely defined */

    recoverable = recable;
    if (recoverable)
	RVMLIB_SET_RANGE(this, sizeof(bitmap));

    while (!(inputmapsize & 7)) 
	inputmapsize++;			/* must be a multiple of 8 */
    if (inputmapsize > 0) {
	mapsize = inputmapsize >> 3;
	if (recoverable) {
	    map = (char *)RVMLIB_REC_MALLOC(mapsize);
	    assert(map);
	    RVMLIB_SET_RANGE(map, mapsize);
	}
	else
	    map = new char[mapsize];

	bzero(map, mapsize);
    }
    else {
	mapsize = 0;
	map = NULL;
    }
}

bitmap::~bitmap() {
    assert(malloced == BITMAP_VIANEW); /* Temporary, until we discover if 
    				bitmaps are ever allocated on the stack
				Satya (6/5/95) */
				
    if (recoverable) {
	RVMLIB_REC_OBJECT(*this);
	if (map) 
	    RVMLIB_REC_FREE(map);
    }
    else {
	if (map) 
	    delete[] map;
    }
    map = NULL;
    mapsize = 0;

    /* Finally, get rid of the object itself if it was allocated
       via new.  This should really have been in delete(), but can't
       test malloced there */
    if (malloced == BITMAP_VIANEW) {
	if (recoverable)
	    RVMLIB_REC_FREE(this);
	else
	    free(this);
    }
}

void bitmap::Grow(int newsize) {
    if (newsize < (mapsize << 3)) return;
    while (!(newsize & 7)) 
	newsize++;		/* must be a multiple of 8 */

    int newmapsize = newsize >> 3;
    char *newmap;
    if (recoverable) {
	newmap = (char *)RVMLIB_REC_MALLOC(newmapsize);
	assert(newmap);
	RVMLIB_SET_RANGE(newmap, newmapsize);
    }
    else {
	newmap = new char[newmapsize];
	assert(newmap);
    }
    bzero(newmap, newmapsize);
    if (map) {
	assert(mapsize > 0);
	bcopy(map, newmap, mapsize);
	if (recoverable)
	    RVMLIB_REC_FREE(map);
	else
	    delete[] map;
    }
    if (recoverable) 
	RVMLIB_SET_RANGE(this, sizeof(bitmap));
    mapsize = newmapsize;
    map = newmap;
}

int bitmap::GetFreeIndex() {
    int j;
    for (int offset = 0; offset < mapsize; offset++){
	if ((~map[offset]) & ALLOCMASK){
	    /* atleast one bit is zero */
	    /* set the bit  and return index */
	    unsigned char availbits = ~map[offset];
	    for (j = 0; j < 8; j++){
		if ((HIGHBIT >> j) & availbits){
		    /* jth bit is available */
		    if (recoverable)
			RVMLIB_SET_RANGE(&map[offset], sizeof(char));
		    map[offset] |= (128 >> j);
		    break;
		}
	    }
	    assert(j < 8);
	    return((offset << 3) + j);
	}
    }
    return(-1);		/* no free slot */

}

void bitmap::SetIndex(int index) {
    int offset = index >> 3;	/* the byte offset into bitmap */
    int bitoffset = index & 7;
    assert(offset < mapsize);

    if (recoverable) RVMLIB_SET_RANGE(&map[offset], sizeof(char));

    /* make sure bit is not set */
    if ((~map[offset]) & (1 << (7-bitoffset))) 
	map[offset] |= (1 << (7 - bitoffset));
}

void bitmap::FreeIndex(int index) {
    int offset = index >> 3;	/* the byte offset into bitmap */
    int bitoffset = index & 7;
    assert(offset < mapsize);

    if (recoverable) RVMLIB_SET_RANGE(&map[offset], sizeof(char));

    /* make sure bit is set */
    if (map[offset] & (1 << (7-bitoffset)))
	map[offset] &= ~(1 << (7 - bitoffset));
}

int bitmap::Value(int index) {
    if (index > (mapsize << 3)) return(-1);
    int offset = index >> 3;
    int bitoffset = index & 7;
    
    return(map[offset] & (1 << (7 - bitoffset)));
}

int bitmap::Count() {
    int count = 0;
    for (int i = 0; i < mapsize; i++) 
	for (int j = 0; j < 8; j++) 
	    if (map[i] & (1 << j)) 
		count++;
    return(count);
}

int bitmap::Size() {
    return (mapsize << 3);
}
void bitmap::purge() {
    if (recoverable) {
	RVMLIB_REC_OBJECT(*this);
	RVMLIB_REC_FREE(map);
    }
    else {
	if (map)
	    delete[] map;
    }
    map = NULL;
    mapsize = 0;
}

void bitmap::operator=(bitmap& b) {
    if (mapsize != b.mapsize) {
	/* deallocate existing map entry */
	if (map) {
	    if (recoverable) 
		RVMLIB_REC_FREE(map);
	    else
		delete[] map;
	}
	
	/* allocate new map */
	if (recoverable) {
	    RVMLIB_REC_OBJECT(*this);
	    map = (char *)RVMLIB_REC_MALLOC(b.mapsize);
	    assert(map);
	    RVMLIB_SET_RANGE(map, mapsize);
	}
	else {
	    map = new char[mapsize];
	    assert(map);
	}
    }
    else {
	/* use space of old map itself */
	if (recoverable) 
	    RVMLIB_SET_RANGE(map, mapsize);
    }
    
    bcopy(b.map, map, mapsize);
}
int bitmap::operator!=(bitmap& b) {
    if (mapsize != b.mapsize) 	
	return (1);
    for (int i = 0; i < mapsize; i++)
	if (map[i] != b.map[i]) return(1);
    return(0);
}

void bitmap::print() {
    print(stdout);
}
void bitmap::print(FILE *fp) {
    print(fileno(fp));
}

void bitmap::print(int fd) {
    char buf[512];
    sprintf(buf, "mapsize %d\n map:\n\0", mapsize);
    write(fd, buf, strlen(buf));
    for (int i = 0; i < mapsize; i++) {
	unsigned long l = (unsigned long) map[i];
	l = l & 0x000000ff;
	sprintf(buf, "0x%x \0",l);
	write(fd, buf, strlen(buf));
    }
    sprintf(buf, "\n");
    write(fd, buf, strlen(buf));
}

#ifdef notdef
ostream &operator<<(ostream& s, bitmap *b) {
    s << "mapsize = " << b->mapsize << '\n';
    s << "bitmap = " ;
    for (int i = 0; i < b->mapsize; i++) {
	long l = (long)(b->map[i]);
	s << hex(l, 2);
    }
    s << '\n';
    return(s);
}
#endif notdef
