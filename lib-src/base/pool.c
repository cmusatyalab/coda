/* BLURB lgpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#include "coda_assert.h"
#include <stdlib.h>
#include <string.h>
#include <lock.h>
#include <preempt.h>
#include "pool.h"
#include "bitvect.h"

struct Pool {
	Lock lock;
	int count;
	int size;
	char *base;
	Bitv freemap;
};

void P_Destroy(PPool *pool)
{
	PRE_BeginCritical();

	if ( !pool ) {
		PRE_EndCritical();
		return;
	}

	if ( (*pool)->freemap ) 
		Bitv_free(&(*pool)->freemap);
	free(pool);
	PRE_EndCritical();
}

PPool P_New(int count, int size) 
{
	PPool pool;

	pool = malloc(sizeof(struct Pool) + size*count);
	if ( !pool ) 
		return NULL;

	pool->freemap = Bitv_new(count);
	if ( !pool->freemap ) {
		free(pool);
		return NULL;
	}
        pool->base = (char *)pool + sizeof(struct Pool);
	return pool;
}

void P_Free(PPool pool, void *addr)
{
	int slot;
	if ( !pool || !addr ) {
		eprint("bogus call to P_Free");
		return;
	}

	ObtainWriteLock(&pool->lock);

	if ( (char *)addr < pool->base || (char *)addr > pool->base + pool->count*pool->size) {
		free(addr);
		return;
	}
		
	slot = ((char *)addr - pool->base)/pool->size;
	CODA_ASSERT( (((char *)addr-pool->base) % pool->size) == 0 );
	
	Bitv_clear(pool->freemap, slot);
	ReleaseWriteLock(&pool->lock);

}

void *P_Malloc(PPool pool)
{
	int loc;

	if ( !pool ) 
		return NULL;

	ObtainWriteLock(&pool->lock);

	loc = Bitv_getfree(pool->freemap);

	if ( loc >= 0 ) 
		return (void *) (pool->base + pool->size * loc);
	else
		return malloc(pool->size);
	ReleaseWriteLock(&pool->lock);

}
