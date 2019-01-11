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
#include "rds_private.h"

/* Split the first block larger than size chunks into two objects.
 * The object with the appropriate size is returned to the caller
 * AND IS NOT PLACED ON ANY LIST. The remaining object is
 * placed onto the appropriate list, based on its new size. Assume caller
 * aborts on error. The returned object is Object2 to make thing easier.
 */

free_block_t *split(int size, rvm_tid_t *tid, int *err)
{
    free_block_t *newObject1, *newObject2;
    free_block_t *bp, *tempbp;
    rvm_return_t rvmerr;
    int remaining_size, i;
    free_list_t *list;
    int second_attempt = 0;

    /* Find the list with the largest blocks that is non-empty. */
    /* Only do the setrange if necessary... */
    if (RDS_FREE_LIST[RDS_MAXLIST].head == NULL) {
        rvmerr = rvm_set_range(tid, &RDS_MAXLIST, sizeof(unsigned long));
        if (rvmerr != RVM_SUCCESS) {
            (*err) = (int)rvmerr;
            return NULL;
        }

        /* Don't need a set range, assume caller already did that. */
        RDS_STATS.large_list++; /* Only bump once, not once per MAXLIST-- */

        /* find the first nonempty list larger than size */
        while (RDS_MAXLIST > size && RDS_FREE_LIST[RDS_MAXLIST].head == NULL) {
            RDS_MAXLIST--;
        }

        /* If no possible block big enough now, see if coalesce will save us.
	 * Coalesce resets MAXLIST to the highest nonempty list.
	 */
        if (RDS_FREE_LIST[RDS_MAXLIST].head == NULL) {
            coalesce(tid, err);
            if (*err)
                return NULL;
        }
    }

retry:
    /* Kind of a hack, try to avoid fragmenting the blocks on MAXLIST by
     * picking from a list that is a multiple of the requested size. */
    list = &RDS_FREE_LIST[RDS_MAXLIST];
    for (i = size * 2; i < RDS_MAXLIST; i += size) {
        if (RDS_FREE_LIST[i].head) {
            list = &RDS_FREE_LIST[i];
            break;
        }
    }

    bp     = NULL;
    tempbp = list->head;
    while (tempbp) {
        /* best fit strategy, only really useful for the largest list which
	 * contains mixed size blocks but since we break out whenever a match
	 * is found this shouldn't add much overhead to the simpler cases */
        if (tempbp->size >= size && (!bp || tempbp->size < bp->size)) {
            bp = tempbp;
            if (bp->size == size)
                break;
        }
        tempbp = tempbp->next;
    }

    if (!bp) {
        if (!second_attempt) {
            /* No block was big enough on the first try,
	     * coalesce and try again */
            coalesce(tid, err);
            if (*err)
                return NULL;

            second_attempt = 1;
            goto retry;
        }

        *err = ENO_ROOM;
        return NULL;
    }

    assert(bp && bp->size >= size); /* Assume we found an appropriate block */

    if (size == bp->size) { /* We have an exact fit */
        rm_from_list(list, bp, tid, err);
        if (*err != SUCCESS)
            return NULL;
        return bp;
    }

    /* Calculate size of block remaining after desired block is split off. */
    remaining_size = bp->size - size;
    assert(remaining_size > 0);

    newObject1 = bp;
    newObject2 = (free_block_t *)/* Cast as char * to get byte addition */
        ((char *)bp + remaining_size * RDS_CHUNK_SIZE);

    /* Init the headers for the new objects. */

    /* For newObject1, lowguard is set, size and highguard need updating. */
    rvmerr = rvm_set_range(tid, newObject1, sizeof(free_block_t));
    if (rvmerr != RVM_SUCCESS) {
        (*err) = (int)rvmerr;
        return NULL;
    }
    newObject1->size = remaining_size;

    /* Add the highguard to the end of the block */
    rvmerr = rvm_set_range(tid, BLOCK_END(newObject1), sizeof(guard_t));
    if (rvmerr != RVM_SUCCESS) {
        (*err) = (int)rvmerr;
        return NULL;
    }
    (*BLOCK_END(newObject1)) = END_GUARD;

    /* for newObject2, size and lowguard need setting, highguard doesn't */
    rvmerr = rvm_set_range(tid, newObject2, sizeof(free_block_t));
    if (rvmerr != RVM_SUCCESS) {
        (*err) = (int)rvmerr;
        return NULL;
    }
    newObject2->size = size;
    newObject2->type = FREE_GUARD;

    /* Put Object1 on the appropriate free list(s). */
    /* If Object1 doesn't need to be moved, nothing needs to happen. */

    /* Otherwise Object1 is taken off the old list and moved to a new one. */
    if (newObject1->size < RDS_MAXLIST) {
        rm_from_list(list, newObject1, tid, err);
        if (*err != SUCCESS)
            return NULL;

        /* newObject1 has been removed, now add it to the appropriate list */
        put_block(newObject1, tid, err);
        if (*err != SUCCESS)
            return NULL;
    }

    *err = SUCCESS;
    return newObject2;
}
