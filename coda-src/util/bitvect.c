/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "coda_assert.h"
#include <stdlib.h>
#include "coda_string.h"
#include <lwp/lock.h>
#include <util.h>
#include "bitvect.h"


struct Bitv_s {
    int length;
    unsigned long *words;
    Lock lock;
};

#define BPW (8 * sizeof(unsigned long))
#define nwords(len) (( ((len) + BPW -1 ) & (~(BPW - 1)) )/BPW)
#define ALLOCMASK (~((unsigned long) 0))
#define HIGHBIT 1<<(BPW - 1)
#define HIGHCBIT 128

Bitv Bitv_new(int len)
{
    Bitv set;

    CODA_ASSERT(len >= 0);
    set = malloc(sizeof(*set));
    CODA_ASSERT(set);

    if (len > 0) {
	set->words = calloc(nwords(len), sizeof(unsigned long));
	CODA_ASSERT(set->words);
    } else {
	set->words = NULL;
    }
    set->length = len;
    Lock_Init(&set->lock);

    return set;
}

void Bitv_free(Bitv *b)
{
    if ( !(b && *b) ) {
	PRE_EndCritical();
	eprint("Trying to free NULL Bitv.\n");
	CODA_ASSERT(0);
    }

    if ((*b)->words)
	free((*b)->words);
    free(*b);
}

int Bitv_length(Bitv b)
{
    int len;
    CODA_ASSERT(b);
    U_rlock(b);
    len = b->length;
    U_runlock(b);
    return len;
}

int Bitv_get(Bitv b, int n)
{
    int index = n/BPW;
    int bitoffset = n%BPW;
    int retval;
    CODA_ASSERT(b);
    CODA_ASSERT(0 <= n && n < b->length);

    U_rlock(b);
    retval = ((b->words[index] >> bitoffset) & 1);
    U_runlock(b);
    return retval;
}

int Bitv_put(Bitv b, int n, int bit)
{
    int previous;

    CODA_ASSERT(b);
    CODA_ASSERT(bit == 0 || bit == 1);
    CODA_ASSERT(0 <= n && n < b->length);

    U_wlock(b);
    previous = ((b->words[n/BPW] >> (n%BPW)) & 1);

    if ( bit == 1 ) 
	b->words[n/BPW] |= 1 << (n%BPW);
    else
	b->words[n/BPW] &= ~(1 << (n%BPW));
    U_wunlock(b);

    return previous;
}

void Bitv_set(Bitv b, int n)
{
    Bitv_put(b, n, 1);
}

void Bitv_clear(Bitv b, int n)
{
    Bitv_put(b, n, 0);
}

int Bitv_getfree(Bitv b)
{
    int i, j, loc;

    CODA_ASSERT(b);

    U_wlock(b);
    for ( i=0 ; i < nwords(b->length) ; i++ ) {
	if ( (~(b->words[i])) & ALLOCMASK ) {
	    unsigned long availbits = ~(b->words[i]);

	    for ( j=0 ; j < BPW; j++ ) 
		if ( (1<<j) & availbits ) 
		    break;
	    CODA_ASSERT( j < BPW);
	    loc = (i<<3)*sizeof(unsigned long) + j;
	    b->words[i] |= (1 << j);
	    U_wunlock(b);
	    return loc;
	}
    }
    U_wunlock(b);
    return -1;
}

int Bitv_count(Bitv b) 
{
    int i, j, count = 0;
    CODA_ASSERT(b);
    U_rlock(b);
    for (i = 0; i < nwords(b->length) ; i++) 
	for (j = 0; j < BPW; j++) 
	    if (b->words[i] & (1 << j)) 
		count++;
    U_runlock(b);
    return(count);
}


void Bitv_print(Bitv b, FILE *fd) 
{
    int i;
    CODA_ASSERT(b);

    U_rlock(b);
    fprintf(fd, "Length of map: %d, content: \n", b->length);
    for ( i=0 ; i < nwords(b->length) ; i++)
	fprintf(fd,"%lx ", b->words[i]);
    fprintf(fd, "\n");
    U_runlock(b);
}

