/*
 * ds_safeq.c: implementation of ds_safeq_t
 */

#include <stdlib.h>
#include <odytypes.h>

#include <lwp.h>
#include <lock.h>
#include "ds_list.h"
#include "ds_safeq.h"
#include "ds_safeq.private.h"

/* magic numbers */

const magic_t ds_safeq_magic = 1574524372;

bool
ds_safeq_valid(ds_safeq_t *q) {
    if (DS_SAFEQ_VALID(q))
	return TRUE;
    else 
	return FALSE;
}

ds_safeq_t *
ds_safeq_create() 
{
    ds_safeq_t *result;

    ALLOC(result,ds_safeq_t);
    result->sq_magic = ds_safeq_magic;
    Lock_Init(&result->sq_lock);
    result->sq_list = ds_list_create(NULL, FALSE, TRUE);
    CODA_ASSERT(result->sq_list);
    
    return result;
}

void
ds_safeq_destroy(q)
    ds_safeq_t *q;
{
    CODA_ASSERT(DS_SAFEQ_VALID(q));

    ObtainWriteLock(&q->sq_lock);
    /* Make sure we do not yeild... */
    q->sq_magic = 0;
    ds_list_destroy(q->sq_list);
    /* XXX - do we want to leve the lock locked? */
    ReleaseWriteLock(&q->sq_lock);
    FREE(q);
}

void
ds_safeq_enq(q, i)
    ds_safeq_t *q;
    void       *i;
{
    CODA_ASSERT(DS_SAFEQ_VALID(q));
    ObtainWriteLock(&q->sq_lock);
    CODA_ASSERT(ds_list_insert(q->sq_list,i) == i);
    LWP_NoYieldSignal(&q->sq_signal);
    ReleaseWriteLock(&q->sq_lock);
}

void *
ds_safeq_deq(q)
    ds_safeq_t *q;
{
    void *result;

    while (1) {
	CODA_ASSERT(DS_SAFEQ_VALID(q));
	ObtainWriteLock(&q->sq_lock);
	result = ds_list_get_first(q->sq_list);
	ReleaseWriteLock(&q->sq_lock);
	if (!result) {
	    LWP_WaitProcess(&q->sq_signal);
	} else {
	    break;
	}
    }
    return result;
}
