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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/res/logalloc.cc,v 4.1 1997/01/08 21:49:58 rvb Exp $";
#endif /*_BLURB_*/







/*
 * logalloc.c 
 * Implements the Memory allocator for portable lists 
 */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <assert.h>
#if !defined(__GLIBC__)
#include <libc.h>
#endif
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>
#include "logalloc.h"

extern void ChooseWrapAroundVnode(PMemMgr *, int);
extern int GetIndexViaWrapAround(PMemMgr *, int);

PMemMgr::PMemMgr(int cSize, int initsize, int vindex, int maxentries) {
    volindex = vindex;
    classSize = cSize;
    maxRecordsAllowed = maxentries;
    maxEntries = initsize;
    nEntries = 0;
    nEntriesAllocated = 0;
    nEntriesDeallocated = 0;
    if(maxEntries > 0) {
	baseAddr = (char *)malloc(cSize * maxEntries);
	assert(baseAddr != 0);
	bitmapSize = maxEntries >> 3;
	bitmap = (unsigned char *)malloc(bitmapSize);
	bzero(bitmap, bitmapSize);
    }
    else {
	baseAddr = 0;
	bitmapSize = 0;
	bitmap = 0;
    }
    for (int i = 0; i < RESOPCOUNTARRAYSIZE; i++) 
	opCountArray[i].AllocCount = opCountArray[i].DeallocCount = 0;
    wrapvnode = -1;
    wrapunique = -1;
}
PMemMgr::PMemMgr(int cSize, int vindex){
    volindex = vindex;
    classSize = cSize;
    maxRecordsAllowed = MAXLOGSIZE;
    maxEntries = STORAGESIZE;
    nEntries = 0;
    nEntriesAllocated = 0;
    nEntriesDeallocated = 0;
    baseAddr = (char *)malloc(cSize * maxEntries);
    assert(baseAddr != 0);
    bitmapSize = maxEntries >> 3;
    bitmap = (unsigned char *)malloc(bitmapSize);
    bzero(bitmap, bitmapSize);
    for (int i = 0; i < RESOPCOUNTARRAYSIZE; i++)
	opCountArray[i].AllocCount = opCountArray[i].DeallocCount = 0;
    wrapvnode = -1;
    wrapunique = -1;
}

PMemMgr::~PMemMgr() {
    if (baseAddr)
	free(baseAddr);
    if (bitmap)
	free(bitmap);
}

/* allocate more log records */
void PMemMgr::GrowStorageArea() {
    /* first allocate storage and copy old storage */
    LogMsg(0, SrvDebugLevel, stdout,  "GrowStorageArea: Growing log from %d to %d entries",
	    maxEntries, maxEntries+STORAGEGROWSIZE);
    char *newbaseaddress = (char *)malloc((maxEntries+STORAGEGROWSIZE)
					  * classSize);
    LogMsg(9, SrvDebugLevel, stdout,  "OldAddress is 0x%x and new address is 0x%x", 
	    baseAddr, newbaseaddress);
    assert(newbaseaddress != 0);
    if (baseAddr){
	bcopy(baseAddr, newbaseaddress, (maxEntries * classSize));
	free(baseAddr);
    }
    baseAddr = newbaseaddress;
    maxEntries += STORAGEGROWSIZE;
    
    /* reset bitmap */
    int newbitmapSize = bitmapSize + (STORAGEGROWSIZE >> 3);
    unsigned char *newbitmap = (unsigned char *)malloc(newbitmapSize);
    assert(newbitmap != 0);
    bzero(newbitmap, newbitmapSize);
    if (bitmap) {
	bcopy(bitmap, newbitmap, bitmapSize);
	free(bitmap);
    }
    bitmap = newbitmap;
    bitmapSize = newbitmapSize;
}

/* try to find an empty slot --> a zero bit in the bitmap */
/* return the index of the zero bit; negative ---> no empty slot */
int PMemMgr::GetFreeBitIndex() {
    int j;

    for (int offset = 0; offset < bitmapSize; offset++){
	if ((~bitmap[offset]) & 255){
	    /* atleast one bit is zero */
	    /* set the bit  and return index */
	    unsigned char availbits = ~bitmap[offset];
	    for (j = 0; j < 8; j++){
		if ((128 >> j) & availbits){
		    /* jth bit is available */
		    bitmap[offset] |= (128 >> j);
		    break;
		}
	    }
	    assert(j < 8);
	    return((offset << 3) + j);
	}
    }
    return(-1);		/* no free slot */
}

/* Free up a slot at the index */
void PMemMgr::FreeBitIndex(int index){
    int offset = index >> 3;	/* the byte offset into bitmap */
    int bitoffset = index & 7;
    assert(offset < bitmapSize);
    /* make sure bit is set */
    assert(bitmap[offset] & (1 << (7-bitoffset)));
    bitmap[offset] &= ~(1 << (7 - bitoffset));
}

void PMemMgr::SetBitIndex(int index){
    int offset = index >> 3;	/* the byte offset into bitmap */
    int bitoffset = index & 7;
    assert(offset < bitmapSize);
    /* make sure bit is set */
    assert(~(bitmap[offset]) & (1 << (7-bitoffset)));
    bitmap[offset] |= (1 << (7 - bitoffset));
}
/* find an unused log record - return -1 if cant find any */
int PMemMgr::NewMem() {
    int index = GetFreeBitIndex();
    if (index < 0){
	if (maxEntries < maxRecordsAllowed) {
	    /* can allocate more space */
	    GrowStorageArea();
	    index = GetFreeBitIndex();
	    if (index < 0) return(-1);
	}
	else {
	    /* try to get space from some existing log */
	    index = GetIndexViaWrapAround(this, volindex);
	    if (index < 0) return(-1);
	    SetBitIndex(index);
	}
    }
    nEntries++;
    nEntriesAllocated++;
    LogMsg(9, SrvDebugLevel, stdout,  "NewMem: returning index %d", index);
    return(index);
}


/* return a log record to the unused record pool */
void PMemMgr::FreeMem(char *entry) {
    int index = AddrToIndex((char *)entry);
    if (index >= maxEntries) { 
	/* make it segv so we can look at the stack */
	LogMsg(0, SrvDebugLevel, stdout,  "FreeMem: Index(%d) out of range - max is %d", 
		index, maxEntries);
	char *p = 0;
	p[0] = 'a';
    }
    FreeBitIndex(index);
    nEntries--;
    nEntriesDeallocated++;
}

/* given an address. return the index in the storage area */
int PMemMgr::AddrToIndex(char *addr) {
    int offset = addr - baseAddr;
    int index = offset/classSize;
    if (index >= maxEntries) {
	printf("AddrToIndex: index = %d and maxEntries = %d BAD BAD BAD\n",
	       index, maxEntries);
	char *p = 0;
	p[0] = 'a';
    }
    return(index);
}

void *PMemMgr::IndexToAddr(int index) {
    if (index >= maxEntries) {
	printf("IndexToAddr: index = %d and maxEntries = %d BAD BAD BAD\n",
	       index, maxEntries);
	char *p = 0;
	p[0] = 'a';
    }
    return(baseAddr + (classSize * index));
}

    
