#ifndef _BLURB_
#define _BLURB_
/*

     RVM: an Experimental Recoverable Virtual Memory Package
			Release 1.3

       Copyright (c) 1990-1994 Carnegie Mellon University
                      All Rights Reserved.

Permission  to use, copy, modify and distribute this software and
its documentation is hereby granted (including for commercial  or
for-profit use), provided that both the copyright notice and this
permission  notice  appear  in  all  copies  of   the   software,
derivative  works or modified versions, and any portions thereof,
and that both notices appear  in  supporting  documentation,  and
that  credit  is  given  to  Carnegie  Mellon  University  in all
publications reporting on direct or indirect use of this code  or
its derivatives.

RVM  IS  AN  EXPERIMENTAL  SOFTWARE  PACKAGE AND IS KNOWN TO HAVE
BUGS, SOME OF WHICH MAY  HAVE  SERIOUS  CONSEQUENCES.    CARNEGIE
MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.
CARNEGIE MELLON DISCLAIMS ANY  LIABILITY  OF  ANY  KIND  FOR  ANY
DAMAGES  WHATSOEVER RESULTING DIRECTLY OR INDIRECTLY FROM THE USE
OF THIS SOFTWARE OR OF ANY DERIVATIVE WORK.

Carnegie Mellon encourages (but does not require) users  of  this
software to return any improvements or extensions that they make,
and to grant Carnegie Mellon the  rights  to  redistribute  these
changes  without  encumbrance.   Such improvements and extensions
should be returned to Software.Distribution@cs.cmu.edu.

*/

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/rvm-src/rds/Attic/rds_prealloc.c,v 4.2 1998/11/02 16:47:39 rvb Exp $";
#endif _BLURB_


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



