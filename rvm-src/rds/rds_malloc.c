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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/rvm-src/rds/rds_malloc.c,v 4.1 1997/01/08 21:54:27 rvb Exp $";
#endif _BLURB_


#include <stdio.h>
#include <rvm.h>
#include <rds.h>
#include <rds_private.h>
#include <rvm_segment.h>
    
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
char *
rds_malloc(size, tid, err)
     unsigned long size;
     rvm_tid_t     *tid;
     int	   *err;
{
    free_block_t *fbp=NULL;
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

    i = (size / RDS_CHUNK_SIZE) + 1;         /* determine which list to use */
    
    if (tid == NULL) {			     /* Use input tid if non-null */
	atid = rvm_malloc_tid();
	rvmret = rvm_begin_transaction(atid, restore);
	if (rvmret != RVM_SUCCESS) {
	    (*err) = (int)rvmret;
	    rvm_free_tid(atid);
	    return NULL;
	}
    } else
	atid = tid;
    

    *err = SUCCESS; 		/* Initialize the error value */
    CRITICAL({
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

	ASSERT(fbp->size == i);	/* Sanity check */
    
	/* Check to see that the guards are valid and the type is free */
	ASSERT((fbp->type == FREE_GUARD) && ((*BLOCK_END(fbp)) == END_GUARD));
		
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
    
	if (tid == NULL) {		/* Let code below pick up the error. */
	    (*err) =(int) rvm_end_transaction(atid, no_flush);
	    rvm_free_tid(atid);
	}
    });

    if (*err != SUCCESS) return NULL;

    RDS_LOG("rdstrace: malloc addr %p size %x req %x\n",
			     USER_BLOCK(fbp), i * RDS_CHUNK_SIZE, orig_size);
    
    return(USER_BLOCK(fbp));
}

