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


#include <stdio.h>
#include <rvm.h>
#include <rds.h>
#include <rds_private.h>
#include <rvm_segment.h>
#include "coda_assert.h"

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

/* Preallocate nblocks of size bytes. Used to fill free lists with blocks
 * of appropriate size so splits won't happen during rds_malloc().
 */

int
rds_prealloc(size, nblocks, tid, err)
     unsigned long size, nblocks;
     rvm_tid_t     *tid;
     int	   *err;
{
    free_block_t *bp;
    rvm_tid_t *atid;
    int i;
    rvm_return_t rvmerr;
    
    if (!HEAP_INIT) {  /* Make sure the heap is initialized */
	(*err) = EHEAP_INIT;    
	return -1;
    }

    /* Reserve bytes to hold the block's size and 2 guards, hidden from user */
    /* Calculate the chunk size which holds that many bytes. */
    size = ((size + RDS_BLOCK_HDR_SIZE) / RDS_CHUNK_SIZE) + 1;
    
    /*
     * if size == maxlist, then preallocing is pointless. The new object
     * is placed on the beginning of the list, then every split after that
     * will return that same block, and put_block will put it back at the head.
     */
    if (size == RDS_MAXLIST) {
	*err = SUCCESS;
	return -1;
    }

    if (tid == NULL) {		     /* Use input tid if non-null */
	atid = rvm_malloc_tid();
	rvmerr = rvm_begin_transaction(atid, restore);
	if (rvmerr != RVM_SUCCESS) {
	    (*err) = (int) rvmerr;
	    rvm_free_tid(atid);
	    return -1;
	}
    } else
	atid = tid;

    /* Update statistics */
    rvmerr = rvm_set_range(atid, &RDS_STATS, sizeof(rds_stats_t));
    if ((rvmerr != RVM_SUCCESS) && (tid == NULL)) {
	rvm_abort_transaction(atid);
	(*err) = (int)rvmerr;
	rvm_free_tid(atid);
	return -1;
    }
    RDS_STATS.prealloc++;	/* Update statistics. */

    *err = SUCCESS; 		/* Initialize the error value */

    /*
     * Here I put the critical section within the loop. I don't think prealloc
     * needs to be streamlined and it allows slightly more parallelization.
     */

    for (i = 0; i < nblocks; i++) {
	CRITICAL({
	    /* Get a block */	
	    bp = split(size, atid, err); 
	    if (bp != NULL) { 
		/* Add the block to the appropriate list. */
		put_block(bp, atid, err);
	    }
	});

	if (*err != SUCCESS) {
	    if (tid == NULL) {
		rvm_abort_transaction(atid);
		rvm_free_tid(atid);
	    }
	    return -1;
	}
    }

    if (tid == NULL) {
	rvmerr = rvm_end_transaction(atid, no_flush);
	if (rvmerr != RVM_SUCCESS) {
	    (*err) = (int) rvmerr;
	    rvm_free_tid(atid);
	    return -1;
	}
	
	rvm_free_tid(atid);
    }
    
    *err = SUCCESS;
    return 0;
}



