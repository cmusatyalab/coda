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

static char *rcsid = "$Header: /usr/rvb/XX/src/rvm-src/rds/RCS/rds_free.c,v 4.1 1997/01/08 21:54:27 rvb Exp $";
#endif _BLURB_

#include <stdlib.h>
#include <stdio.h>
#ifdef __MACH__
#include <sysent.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif
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

/* There's another bug too: if tid1 frees an object, then tid2 mallocs it,
 * then tid1 aborts, the object is referenced twice. To avoid this and the
 * above two problems, rather than actually freeing an object during a
 * transaction, keep a record of our intension to do so and only do it when
 * the transaction commits. If it aborts, we don't want to free the object
 * so we can just throw away the intension record. Thus we can keep that record
 * in VM. The record only needs to indicate which transaction and which object.
 * By keeping a list per transaction, each entry is simply an address.
 * So rds_fake_free(addr, list) adds one's intension of freeing the object.
 * rds_do_free(list, mode) actually frees the objects on the list, using
 * mode as the end_transaction mode.
 */
     
int
rds_free(addr, tid, err)
     char	  *addr;
     rvm_tid_t    *tid;
     int	  *err;
{
    free_block_t *bp = BLOCK_HDR(addr);  /* find pointer to block header */
    rvm_tid_t *atid;
    rvm_return_t rvmret;
    
    /* Make sure the heap has been initialized */
    if (!HEAP_INIT) {
	(*err) = EHEAP_INIT;
	return -1;
    }

    if (tracing_rds) {
       (*rds_trace_printer)("rdstrace: Error!!! rds_free called\n");
    }

    /* Make sure that the pointer is word aligned */
    if ((bp == NULL) || (int)bp % 4) {
	(*err) = EBAD_ARGS;
	return -1;
    }
    
    /* Verify that the guards are intact */
    if (bp->type == FREE_GUARD) { 	/* object was already freed */
	(*err) = EFREED_TWICE;
	return -1;
    }

    /* improper guard --> memory corruption */
    if ((bp->type != ALLOC_GUARD) || (*BLOCK_END(bp) != END_GUARD)) {
	(*err) = ECORRUPT;
	return -1;
    }

    if (tid == NULL) {			     /* Use input tid if non-null */
	atid = rvm_malloc_tid();
	rvmret = rvm_begin_transaction(atid, restore);
	if (rvmret != RVM_SUCCESS) {
	    (*err) = (int) rvmret;
	    rvm_free_tid(atid);
	    return -1;
	}
    } else
	atid = tid;

    *err = SUCCESS; 		/* Initialize the error value */
    CRITICAL({
	/* Update statistics */
	rvmret = rvm_set_range(atid, &RDS_STATS, sizeof(rds_stats_t));
	if (rvmret != RVM_SUCCESS) {
	    (*err) = (int)rvmret;
	} else {
	    /* Set the lowguard to reflect that the block has been allocated. */
	    rvmret = rvm_set_range(atid, &(bp->type), sizeof(guard_t));
	    if (rvmret != RVM_SUCCESS) {
		if (tid == NULL) {
		    rvm_abort_transaction(atid);
		    rvm_free_tid(atid);
		}
		(*err) = (int)rvmret;
	    } else {
		bp->type = FREE_GUARD;

		RDS_STATS.free++;
		RDS_STATS.freebytes   += bp->size * RDS_CHUNK_SIZE;
		RDS_STATS.mallocbytes -= bp->size * RDS_CHUNK_SIZE;    

		/* Add the block to the approprite free list. */
		put_block(bp, atid, err); /* Error is picked up below... */
	    }
	}
	if ((*err != SUCCESS) && (tid == NULL)) {
	    rvm_abort_transaction(atid);
	    rvm_free_tid(atid);
	} else if (tid == NULL) {
	    rvmret = rvm_end_transaction(atid, no_flush);
	    rvm_free_tid(atid);
	    if (rvmret != RVM_SUCCESS) {
		(*err) = (int)rvmret;
	    }
	}
    });
	
    return 0;
}

/* Assume only one thread can have an open tid at a time. Since we're only
 * modifying per-tid structures, we don't have any concurency.
 */
int rds_fake_free(addr, list)
     char *addr;
     intentionList_t *list;
{
    char **temp;
    
    free_block_t *bp = BLOCK_HDR(addr);  /* find pointer to block header */
    
    /* Make sure the heap has been initialized */
    if (!HEAP_INIT) {
	return EHEAP_INIT;
    }

    /* Make sure that the pointer is word aligned */
    if ((bp == NULL) || (int)bp % 4)
	return EBAD_ARGS;
    
    /* Verify that the guards are intact */
    if (bp->type == FREE_GUARD) 	/* object was already freed */
	return EFREED_TWICE;

    /* improper guard --> memory corruption */
    if ((bp->type != ALLOC_GUARD) || (*BLOCK_END(bp) != END_GUARD))
	return ECORRUPT;

    /* If no intention list has been built for this tid yet, build one. */
    if (list->table == NULL) {
	list->size = STARTSIZE;
	list->table = (char **)malloc(list->size);
	list->count = 0;
    } else if ((list->count * 4) == list->size) {
	list->size *= 2;
	temp = (char **)malloc(list->size);
	BCOPY(list->table, temp, list->count * sizeof(char *));
	free(list->table);
	list->table = temp;
    }

    (list->table)[list->count++] = (char *)addr;
    return(SUCCESS);
}

int rds_do_free(list, mode)
     intentionList_t *list;
     rvm_mode_t mode;
{
    int i, err;
    rvm_tid_t *tid = rvm_malloc_tid();
    rvm_return_t rvmret;
    
    rvmret = rvm_begin_transaction(tid, restore);
    if (rvmret != RVM_SUCCESS) {
        rvm_free_tid(tid);
	return (int) rvmret;
    }
    
    if (tracing_rds) {
        (*rds_trace_printer)("rdstrace: start do_free\n");
    }

    err = SUCCESS; 		/* Initialize the error value */
    CRITICAL({
	/* Only need to set the range once. To be safe, doing in critical...*/
	rvmret = rvm_set_range(tid, &RDS_STATS, sizeof(rds_stats_t));
	if (rvmret != RVM_SUCCESS) {
	    err = (int)rvmret;
	} else
	  for (i = 0; i < list->count; i++) {
	    /* find pointer to block header */
	    free_block_t *bp = BLOCK_HDR((list->table)[i]); 

	    /* Set the lowguard to reflect that the block has been allocated. */
	    ASSERT(bp->type == ALLOC_GUARD);
	    rvmret = rvm_set_range(tid, &(bp->type), sizeof(guard_t));
	    if (rvmret != RVM_SUCCESS) {
		err = (int)rvmret;
		break;
	    }
	    bp->type = FREE_GUARD;

	    /* Update statistics */
	    RDS_STATS.free++;
	    RDS_STATS.freebytes   += bp->size * RDS_CHUNK_SIZE;
	    RDS_STATS.mallocbytes -= bp->size * RDS_CHUNK_SIZE; 
	    
	    if (tracing_rds) {
	        (*rds_trace_printer)("rdstrace: addr %d size %d\n",
				     bp, bp->size * RDS_CHUNK_SIZE);
	    } 

	    /* Add the block to the approprite free list. */
	    put_block(bp, tid, &err); 
	    if (err != SUCCESS)
		break;
	}
	
	if (tracing_rds) {
	    (*rds_trace_printer)("rdstrace: end do_free\n");
	}

	if (err != SUCCESS) {
	    rvm_abort_transaction(tid);
	} else {
	    rvmret = rvm_end_transaction(tid, mode);
	}
    });
	
    rvm_free_tid(tid);
    free(list->table);
    list->table = NULL;		/* Just to be safe */

    if (err != SUCCESS) return err;
    if (rvmret != RVM_SUCCESS) return (int) rvmret;
    return (err != SUCCESS) ? err : ((rvmret != RVM_SUCCESS) ? (int)rvmret : 0);
}
