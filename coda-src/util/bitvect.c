#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <lock.h>
#include <util.h>
#include "bitvect.h"


struct Bitv_s {
    int length;
    unsigned char *bytes;
    unsigned long *words;
    Lock lock;
};

#define BPW (8 * sizeof(unsigned long))
#define nwords(len) (( ((len) + BPW -1 ) & (~(BPW - 1)) )/BPW)
#define ALLOCMASK (~((unsigned long) 0))
#define HIGHBIT 1<<(8*sizeof(unsigned long) - 1)
#define HIGHCBIT 128

Bitv Bitv_new(int len)
{
    Bitv set;

    assert(len >= 0);
    set = malloc(sizeof(*set));
    assert(set);

    if (len > 0) {
	set->words = calloc(nwords(len), sizeof(unsigned long));
	assert(set->words);
    } else {
	set->words = NULL;
    }
    set->bytes = (unsigned char *) set->words;
    set->length = len;
    Lock_Init(&set->lock);

    return set;
}

void Bitv_free(Bitv *b)
{

    PRE_BeginCritical();

    if ( !(b && *b) ) {
	PRE_EndCritical();
	eprint("Trying to free NULL Bitv.\n");
	assert(0);
    }

    if ((*b)->words)
	free((*b)->words);
    free(*b);

    PRE_EndCritical();
}

int Bitv_length(Bitv b)
{
    assert(b);
    U_rlock(b);
    return(b->length);
    U_runlock(b);
}

int Bitv_get(Bitv b, int n)
{
    int index = n/8;
    int bitoffset = n%8;
    assert(b);
    assert(0 <= n && n < b->length);

    U_rlock(b);
    return ((b->bytes[index] >> bitoffset) & 1);
    U_runlock(b);
}

int Bitv_put(Bitv b, int n, int bit)
{
    int previous;

    assert(b);
    assert(bit == 0 || bit == 1);
    assert(0 <= n && n < b->length);

    U_wlock(b);
    previous = ((b->bytes[n/8] >> (n%8)) & 1);

    if ( bit == 1 ) 
	b->bytes[n/8] |= 1 << (n%8);
    else
	b->bytes[n/8] &= ~(1 << (n%8));
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

    assert(b);

    U_wlock(b);
    for ( i=0 ; i < nwords(b->length) ; i++ ) {
	if ( (~(b->words[i])) & ALLOCMASK ) {
	    unsigned long availbits = ~(b->words[i]);

	    for ( j=0 ; j < 8*sizeof(unsigned long); j++ ) 
		if ( (1<<j) & availbits ) 
		    break;
	    assert( j < 8*sizeof(unsigned long));
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
    assert(b);
    U_rlock(b);
    for (i = 0; i < nwords(b->length) ; i++) 
	for (j = 0; j < 8 * sizeof(unsigned long); j++) 
	    if (b->bytes[i] & (1 << j)) 
		count++;
    U_runlock(b);
    return(count);
}


void Bitv_print(Bitv b, FILE *fd) 
{
    int i;
    assert(b);

    U_rlock(b);
    fprintf(fd, "Length of map: %d, content: \n", b->length);
    for ( i=0 ; i < nwords(b->length) ; i++)
	fprintf(fd,"%#08x ", b->words[i]);
    fprintf(fd, "\n");
    U_runlock(b);
}

