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

static char *rcsid = "$Header: rds_init.c,v 1.4 95/01/16 18:04:31 bnoble Exp $";
#endif _BLURB_


#include <stdio.h>
#include <rvm.h>
#include <rds.h>
#include <rds_private.h>
#include <rvm_segment.h>

/* This routine initializes a region of memory to contain the heap. The
 * calling routine is assumed to have started a transaction and so assumes
 * the responsibility of aborting on error.
 * This routine should run entirely seperately from the other heap operations.
 * As such it is assumed to never be run concurently with any other rds
 * routine.
 */

int
rds_init_heap(base, length, chunk_size, nlists, tid, err)
     char *base;
     rvm_length_t length;
     unsigned long chunk_size;
     unsigned long nlists;
     rvm_tid_t *tid;
     int       *err;
{
    heap_header_t *hdrp = (heap_header_t *)base;
    free_block_t *fbp;
    int i, remaining_space;
    unsigned long heap_hdr_len;
    rvm_return_t rvmret;
    int *addr;
    
    /* heap consists of a heap_header_t followed by nlist list headers */
    heap_hdr_len = sizeof(heap_header_t) + nlists * sizeof(free_list_t);
    if (heap_hdr_len > length) {
	printf("Heap not long enough to hold heap header\n");
	(*err) = ENO_ROOM;
	return -1;
    }

    rvmret = rvm_set_range(tid, base, heap_hdr_len);
    if (rvmret != RVM_SUCCESS) {
	(*err) = (int) rvmret;
	return -1;
    }

    strcpy(hdrp->version, RDS_HEAP_VERSION);
    hdrp->heaplength = length;
    hdrp->chunk_size = chunk_size;
    hdrp->nlists = hdrp->maxlist = nlists;

    /* Initialize the statistics to zero */
    BZERO(&(hdrp->stats), sizeof(rds_stats_t));
    
    /* create nlists free list structures, making each list null. */
    /* Since the lists are indexed by number of chunks,
     * 1 should be the first entry, not zero. */

    for (i = 1; i < nlists + 1; i++) {
	hdrp->lists[i].head = (free_block_t *)NULL;
	hdrp->lists[i].guard = FREE_LIST_GUARD;
    }

    /* For the last list, make it point to the first page after the list header
     * array. At this point, write out a null pointer and the size of the
     * remaining region. Make the last list header point to this address */

    remaining_space = length - heap_hdr_len;

    /* determine where the first block will start */
    /* Should we round this up to page size? */
    fbp = (free_block_t *)((char *)&(hdrp->lists[nlists]) + sizeof(free_list_t)); 

    rvmret = rvm_set_range(tid, fbp, sizeof(free_block_t));
    if (rvmret != RVM_SUCCESS) {
	(*err) = (int) rvmret;
	return -1; 
    }

    /* put the block on the list, making it null */
    fbp->size = remaining_space / chunk_size;
    fbp->type = FREE_GUARD;
    fbp->prev = fbp->next = (free_block_t *)NULL;
    hdrp->lists[nlists].head = fbp;

    hdrp->stats.freebytes = fbp->size * chunk_size;  /* Num of allocatable bytes*/

    /* Add the guard to the end of the block */
    addr = (int *)((char *)fbp + fbp->size * chunk_size);
    ASSERT((char *)addr <= base + length);
    
    addr--;  /* point to last word in the block */
    rvmret = rvm_set_range(tid, addr, sizeof(guard_t));
    if (rvmret != RVM_SUCCESS) {  
	(*err) = (int) rvmret;
	return -1;
    }
    (*addr) = END_GUARD;

    (*err) = SUCCESS;
    return 0;
}
