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

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/rvm-src/rds/rds_coalesce.c,v 1.1.1.1 1996/11/22 18:39:49 rvb Exp";

#endif _BLURB_


#include <stdio.h>
#include <stdlib.h>
#include <rvm.h>
#include <rds.h>
#include <rds_private.h>
#include <rvm_segment.h>

/*
 * Coalescing has proven necessary. This approach is to invoke it rarely,
 * like in times of emergency or when the application starts up. This
 * approach will merge all memory that can be merged, and runs linearly
 * in the number of free blocks. After this phase has run, the MAXLIST
 * is reset to the highest non-empty list.
 */

void
coalesce(tid, err)
    rvm_tid_t *tid;
    int *err;
{
    free_block_t *fbp, *nfbp, *save;
    rvm_return_t rvmret;
    int i, list, old_maxlist, merged;

    /* Make sure the heap has been initialized */
    if (!HEAP_INIT) {
	(*err) = EHEAP_INIT;
	return;
    }

    /* Update stats - don't need setrange, assume caller already has done that. */
    RDS_STATS.coalesce++;

    *err = SUCCESS; 		/* Initialize the error value */

    /* Go through the lists, examing objects. For each free object, merge it 
     * with the next consecutive object in memory if it is also free. Continue
     * merging until the next consecutive object is not free. The resulting
     * object is guaranteed to be larger than the original, so put it on a new
     * list. Because of this last fact, and because objects are split off the
     * tail in split(), it may be wiser to run this loop from high to low.
     * Need to make sure objects aren't pulled off the list from under our feet.
     */
    
    for (i = RDS_NLISTS; i > 0; i--) {
	/* Check to see if the list's guard is alright. */
	if (RDS_FREE_LIST[i].guard != FREE_LIST_GUARD) {
	    (*err) = ECORRUPT;
	    return;
	}

	fbp = RDS_FREE_LIST[i].head;

	while (fbp != NULL) {
	    ASSERT(fbp->type == FREE_GUARD);	/* Ensure fbp is a free block */
	    
	    nfbp = NEXT_CONSECUTIVE_BLOCK(fbp);
	    merged = 0;

	    /* Do the set_range outside of next loop if appropriate. */
	    if ((nfbp->type == FREE_GUARD) && (nfbp < RDS_HIGH_ADDR)) { 
		rvmret = rvm_set_range(tid, (char *)fbp, sizeof(free_block_t));
		if (rvmret != RVM_SUCCESS) {
		    (*err) = (int)rvmret;
		    return;
		}
	    }

	    /* See if the next consecutive object is free. */
	    
	    while ((nfbp->type == FREE_GUARD) && (nfbp < RDS_HIGH_ADDR)) {
		int *block_end = BLOCK_END(fbp);/* Save a ptr to the endguard */ 
		merged = 1;
		RDS_STATS.merged++;		/* Update merged object stat */
		fbp->size += nfbp->size;	/* Update the object's size */

		/* remove the second object from it's list */
		list = (nfbp->size >= RDS_MAXLIST)?RDS_MAXLIST:nfbp->size;
		ASSERT(RDS_FREE_LIST[list].head != NULL);
		
		rm_from_list(&RDS_FREE_LIST[list], nfbp, tid, err);
		if (*err != SUCCESS) {
		    return;
		}

		/* Take out the guards to avoid future confusion. I'm going
		 * to assume that the next block follows immediately on
		 * the endguard of the first block */
		rvmret = rvm_set_range(tid, (char *)block_end,
				       sizeof(guard_t) + sizeof(free_block_t));
		if (rvmret != RVM_SUCCESS) {
		    (*err) = (int)rvmret;
		    return;
		}
		*block_end = 0;
		BZERO(nfbp, sizeof(free_block_t));
		
		nfbp = NEXT_CONSECUTIVE_BLOCK(fbp);
	    }

	    if (!merged)
		RDS_STATS.unmerged++;
	    
	    /* Move fbp, if merged, it must be larger. Don't move if already
	     * on the highest list. -- Use NLISTS here if MAXLIST < NLISTS.
	     */
	    
	    if (merged && i < RDS_NLISTS) {
		/* remove fbp from it's list */
		rm_from_list(&RDS_FREE_LIST[i], fbp, tid, err);
		if (*err != SUCCESS) {
		    return;
		}

		save = fbp->next; /* Save the old value of next */
		
		/* place fbp in its new list. */
		put_block((char *)fbp, tid, err);
		if (*err != SUCCESS) {
		    return;
		}

		fbp = save; 
	    }
	    else {
		fbp = fbp->next;
	    }
	}
    }

    /* Second part is to put any real large objects on RDS_MAXLIST back where they
       belong. Reset maxlist to it's highest value, RDS_NLIST. Obviously don't
       need to do the second or third phases if RDS_MAXLIST == RDS_NLISTS. */

    if (RDS_MAXLIST < RDS_NLISTS) {
	old_maxlist = RDS_MAXLIST;

	rvmret = rvm_set_range(tid, (char *)&(RDS_MAXLIST), sizeof(RDS_MAXLIST));
	if (rvmret != RVM_SUCCESS) {
	    (*err) = (int)rvmret;
	    return;
	}
	RDS_MAXLIST = RDS_NLISTS;
    
	fbp = RDS_FREE_LIST[old_maxlist].head;
	while (fbp != NULL) {
	    if (fbp->size > old_maxlist) {
	    
		rm_from_list(&RDS_FREE_LIST[old_maxlist], fbp, tid, err);
		if (*err != SUCCESS) {
		    return;
		}

		save = fbp->next; /* Save the old value of next */
		
		/* Place the object in it's appropriate list. */
		put_block((char *)fbp, tid, err);
		if (*err != SUCCESS) {
		    return;
		}
		fbp = save;
	    }
	    else
		fbp = fbp->next;

	}

	/* Third phase is reset RDS_MAXLIST to the highest non-empty value.*/

	/* Used to be an assertion that maxlist != 1 in the next loop,
	 * Is this really a problem? I don't see it 1/29 -- dcs
	 */
	while ((RDS_FREE_LIST[RDS_MAXLIST].head == NULL) && (RDS_MAXLIST > 1)) {
	    RDS_MAXLIST--;
	}
    }

    *err = SUCCESS;
}
