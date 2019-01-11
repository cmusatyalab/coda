/* BLURB lgpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-2016 Carnegie Mellon University
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

/* This routine initializes a region of memory to contain the heap. The
 * calling routine is assumed to have started a transaction and so assumes
 * the responsibility of aborting on error.
 * This routine should run entirely seperately from the other heap operations.
 * As such it is assumed to never be run concurently with any other rds
 * routine.
 */

int rds_init_heap(char *base, rvm_length_t length, unsigned long chunk_size,
                  unsigned long nlists, rvm_tid_t *tid, int *err)
{
    heap_header_t *hdrp = (heap_header_t *)base;
    free_block_t *fbp;
    int i;
    rvm_length_t remaining_space;
    rvm_length_t heap_hdr_len;
    rvm_return_t rvmret;
    guard_t *addr;

    /* heap consists of a heap_header_t followed by nlist list headers */
    heap_hdr_len = sizeof(heap_header_t) + nlists * sizeof(free_list_t);
    if (heap_hdr_len > length) {
        printf("Heap not long enough to hold heap header\n");
        (*err) = ENO_ROOM;
        return -1;
    }

    rvmret = rvm_set_range(tid, base, heap_hdr_len);
    if (rvmret != RVM_SUCCESS) {
        (*err) = (int)rvmret;
        return -1;
    }

    assert(chunk_size >= sizeof(free_block_t) + sizeof(guard_t));

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
        hdrp->lists[i].head  = (free_block_t *)NULL;
        hdrp->lists[i].guard = FREE_LIST_GUARD;
    }

    /* For the last list, make it point to the first page after the list header
     * array. At this point, write out a null pointer and the size of the
     * remaining region. Make the last list header point to this address */

    remaining_space = length - heap_hdr_len;

    /* determine where the first block will start */
    fbp =
        (free_block_t *)((char *)&(hdrp->lists[nlists]) + sizeof(free_list_t));
    /* Round this up to the chunk size */
    fbp = (free_block_t *)(((long)((char *)fbp + chunk_size - 1) / chunk_size) *
                           chunk_size);

    rvmret = rvm_set_range(tid, fbp, sizeof(free_block_t));
    if (rvmret != RVM_SUCCESS) {
        (*err) = (int)rvmret;
        return -1;
    }

    /* put the block on the list, making it null */
    fbp->size = remaining_space / chunk_size;
    fbp->type = FREE_GUARD;
    fbp->prev = fbp->next    = (free_block_t *)NULL;
    hdrp->lists[nlists].head = fbp;

    hdrp->stats.freebytes =
        fbp->size * chunk_size; /* Num of allocatable bytes*/

    /* Add the guard to the end of the block */
    addr = (guard_t *)((char *)fbp + fbp->size * chunk_size);
    assert((char *)addr <= base + length);

    addr--; /* point to last word in the block */
    rvmret = rvm_set_range(tid, addr, sizeof(guard_t));
    if (rvmret != RVM_SUCCESS) {
        (*err) = (int)rvmret;
        return -1;
    }
    (*addr) = END_GUARD;

    (*err) = SUCCESS;
    return 0;
}
