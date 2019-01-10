/* BLURB lgpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-2010 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#include <stdio.h>
#include "rds_private.h"

/************** NOTE: ***************/
/* we create our own transactions in the following routines, even
 * though there is a tid in the interface. This might result in unreferenced
 * objects if the thread which malloc'ed the object decides to abort. These
 * routines have no way of knowing if such an event happens. Also, if the user
 * free's then aborts, could have pointers into the free list objects.
 *
 * Interface has been changed so that if a user feels confident that his
 * transactions are serialized, (the above problem won't arise), he can pass
 * in a non-null tid ptr and it will be used. So if you aren't sure, make the
 * tidptr is zero!
 */

/* Allocate a free block which will hold size + some bytes. The some refers to
 * a size field, and two guards to detect overwriting of memory, which will
 * be added to the object. Treat the last list seperately since it holds objects
 * of that size chunks OR LARGER. A pointer to size bytes will be returned. */
char *rds_malloc(unsigned long size, rvm_tid_t *tid, int *err)
{
    free_block_t *fbp = NULL;
    rvm_tid_t *atid;
    rvm_return_t rvmret;
    int i;
    unsigned long orig_size = size;

    /* Make sure the heap has been initialized */
    if (!HEAP_INIT) {
        (*err) = EHEAP_INIT;
        return NULL;
    }

    /* Reserve bytes to hold the block's size and 2 guards, hidden from user */
    size += RDS_BLOCK_HDR_SIZE;

    i = (size / RDS_CHUNK_SIZE) + 1; /* determine which list to use */

    if (tid == NULL) { /* Use input tid if non-null */
        atid   = rvm_malloc_tid();
        rvmret = rvm_begin_transaction(atid, restore);
        if (rvmret != RVM_SUCCESS) {
            (*err) = (int)rvmret;
            rvm_free_tid(atid);
            return NULL;
        }
    } else
        atid = tid;

    *err = SUCCESS; /* Initialize the error value */
    START_CRITICAL;
    {
        /* Update stats */
        rvmret = rvm_set_range(atid, &RDS_STATS, sizeof(rds_stats_t));
        if (rvmret != RVM_SUCCESS) {
            (*err) = (int)rvmret;
            if (tid == NULL) {
                rvm_abort_transaction(atid);
                rvm_free_tid(atid);
            }
            LEAVE_CRITICAL_SECTION;
        }

        RDS_STATS.malloc++;
        RDS_STATS.freebytes -= i * RDS_CHUNK_SIZE;
        RDS_STATS.mallocbytes += i * RDS_CHUNK_SIZE;

        /* Get a block of the correct size. */
        fbp = get_block(i, atid, err);
        if (*err != SUCCESS) {
            if (tid == NULL) {
                rvm_abort_transaction(atid);
                rvm_free_tid(atid);
            }
            LEAVE_CRITICAL_SECTION;
        }

        assert(fbp->size == i); /* Sanity check */

        /* Check to see that the guards are valid and the type is free */
        assert((fbp->type == FREE_GUARD) && ((*BLOCK_END(fbp)) == END_GUARD));

        /* Set the lowguard to reflect that the block has been allocated. */
        rvmret = rvm_set_range(atid, fbp, sizeof(free_block_t));
        if (rvmret != RVM_SUCCESS) {
            if (tid == NULL) {
                rvm_abort_transaction(atid);
                rvm_free_tid(atid);
            }
            (*err) = (int)rvmret;
            LEAVE_CRITICAL_SECTION;
        }
        fbp->type = ALLOC_GUARD;
        fbp->prev = fbp->next = NULL;

        if (tid == NULL) { /* Let code below pick up the error. */
            (*err) = (int)rvm_end_transaction(atid, no_flush);
            rvm_free_tid(atid);
        }
    }
    END_CRITICAL;

    if (*err != SUCCESS)
        return NULL;

    RDS_LOG("rdstrace: malloc addr %p size %lx req %lx\n", USER_BLOCK(fbp),
            i * RDS_CHUNK_SIZE, orig_size);

    return (USER_BLOCK(fbp));
}
