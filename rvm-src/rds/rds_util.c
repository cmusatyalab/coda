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

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/rvm-src/rds/rds_util.c,v 1.1.1.1 1996/11/22 18:40:02 rvb Exp";
#endif _BLURB_


#include <stdio.h>
#include <rvm.h>
#include <rds.h>
#include <rds_private.h>
#include <rvm_segment.h>
    
int
enqueue(list, block, tid, err)
     free_list_t  *list;
     free_block_t *block;
     rvm_tid_t    *tid;
     int	  *err;
{
    ASSERT(0);
    return -1;
}

/*
 * Dequeue a memory object from the front of a list, return the memory object.
 */

free_block_t *
dequeue(list, tid, err)
     free_list_t  *list;
     rvm_tid_t    *tid;
     int	   *err;
{
    free_block_t *block = list->head;
    free_block_t *ptr;
    rvm_return_t rvmerr;
    
    /* Take the block off the head of the list. */
    rvmerr = rvm_set_range(tid, list, sizeof(free_list_t));
    if (rvmerr != RVM_SUCCESS) {
	(*err) = (int) rvmerr;
	return NULL;
    }
    ptr = list->head = block->next;

    if (ptr) {
	rvmerr = rvm_set_range(tid, &(ptr->prev), sizeof(free_list_t *));
	if (rvmerr != RVM_SUCCESS) {
	    (*err) = (int) rvmerr;
	    return NULL;
	}
	ptr->prev = (free_block_t *)NULL;
    }
	
    *err = SUCCESS;
    return block;
}

/*
 * Sometimes we want to remove an object from the middle of a list. This
 * routine takes a list, a pointer to the previous object, a ptr to the object,
 * and a transaction ID, the status is passed back in err. NOTE: If the object
 * is the first one on the list, prev isn't used.
 */
int
rm_from_list(list, bp, tid, err)
     free_list_t   *list;
     free_block_t  *bp;
     rvm_tid_t     *tid;
     int	    *err;
{
    rvm_return_t rvmret;
    free_block_t *ptr;
    
    /* If block is at the head of the list, dequeue will cleanly remove it. */
    if (bp == list->head) {
	bp = dequeue(list, tid, err);
	if (bp == NULL)
	    return NULL;
    } else {
	/* Because we're not at the end, we know bp->prev is a valid pointer. */
	ptr = bp->prev;
	rvmret = rvm_set_range(tid, &(ptr->next), sizeof(free_block_t *));
	if (rvmret != RVM_SUCCESS) {
	    (*err) = (int) rvmret;
	    return NULL;
	}
	ptr->next = bp->next;
	
	ptr = bp->next;
	if (ptr) {
	    rvmret = rvm_set_range(tid, &(ptr->prev), sizeof(free_block_t *));
	    if (rvmret != RVM_SUCCESS) {
		(*err) = (int) rvmret;
		return NULL;
	    }
	    ptr->prev = bp->prev; /* NOTE: this may be zero */
	}
    }

    *err = SUCCESS;
    return 1;
}

/* Print out the free list structure */
int
print_heap()
{
    int i, j;
    int total_size = 0;
    free_block_t *fbp, *ptr;
    
    if (!HEAP_INIT)	/* Make sure RecoverableHeapStartAddress is inited */
	return -1;

    CRITICAL({
	printf("Heap starts at %lx, uses %ld sized chunks, and use %ld of %ld lists\n",
	       (long)RecoverableHeapStartAddress, RDS_CHUNK_SIZE, 
	       RDS_MAXLIST, RDS_NLISTS);

	for (i = 1; i < RDS_NLISTS + 1; i++) {
	    printf("list %d %c\n",i, ((i == RDS_MAXLIST)?'+':' '));
	    fbp = RDS_FREE_LIST[i].head;

	    if (RDS_FREE_LIST[i].guard != FREE_LIST_GUARD)
		printf("Bad guard on list %d!!!\n", i);

	    if (fbp && (fbp->prev != (free_block_t *)NULL))
		printf("Non-null Initial prev pointer.\n");
	    
	    j = 1;
	    while (fbp != NULL) {
		printf("%d	block %lx, size %ld\n", j++, (long)fbp, 
		       fbp->size);
		total_size += fbp->size;
	    
		if (fbp->type != FREE_GUARD)
		    printf("Bad lowguard on block\n");
		if ((*BLOCK_END(fbp)) != END_GUARD)
		    printf("Bad highguard, %lx=%x\n",(long)BLOCK_END(fbp), 
						      *BLOCK_END(fbp));
		ptr = fbp->next;
		if (ptr && (ptr->prev != fbp))
		    printf("Bad chain link %lx <-> %lx\n", (long)fbp, 
			   (long)ptr);
		if (i != RDS_MAXLIST && fbp->size != i)
		    printf("OBJECT IS ON WRONG LIST!!!!\n");
		    
		fbp = fbp->next;
	    }
	}
    });
    
    printf("Sum of sizes of objects in free lists is %d.\n", total_size);
    return 0;
}

/*
 * Rather than having malloc access the lists directly, it seems wiser to
 * have intermediary routines to get and put blocks on the list.
 */

free_block_t *
get_block(size, tid, err)
      int       size;
      rvm_tid_t *tid;
      int	*err;
{
    int list = ((size >= RDS_MAXLIST)?RDS_MAXLIST:size);
    free_block_t *fbp;
    
    /* Check the guard on the list. */
    if (RDS_FREE_LIST[list].guard != FREE_LIST_GUARD) {
	*err = ECORRUPT;
	return NULL;
    }
    
    /* Update stats. Don't need a setrange, caller should have done that. */
    if ((RDS_FREE_LIST[list].head == NULL) ||	   /* For smaller blocks */
	(RDS_FREE_LIST[list].head->size != size)) { /* In case of large block */
	/* A block isn't available so we need to split one. */
	if (list < RDS_MAXLIST)
	    RDS_STATS.misses++;
	else
	    RDS_STATS.large_misses++;
	
	return split(size, tid, err);
    }

    ASSERT(RDS_FREE_LIST[list].head->size == size); /* Sanity check */

    if (list < RDS_MAXLIST)
	RDS_STATS.hits++;	
    else
	RDS_STATS.large_hits++;	
	

    fbp = dequeue(&RDS_FREE_LIST[list], tid, err);
    /* Fbp could be null indicating an error occured in dequeue. Let
       the calling routine handle this error. */
    
    return fbp;
}

int
put_block(bp, tid, err)
     free_block_t *bp;
     rvm_tid_t    *tid;
     int	  *err;
{
    rvm_return_t rvmerr;
    int size = (((bp->size) >= RDS_MAXLIST)?RDS_MAXLIST:(bp->size));
    free_list_t *list = &RDS_FREE_LIST[size];
    free_block_t *ptr;
    
    /* Check the guard on the list. */
    if (list->guard != FREE_LIST_GUARD) {
	*err = ECORRUPT;
	return -1;
    }
    
    /* Add the block to the head of the list */
    rvmerr = rvm_set_range(tid, bp, sizeof(free_block_t));
    if (rvmerr != RVM_SUCCESS) {
	(*err) = (int) rvmerr;
	return -1;
    }
    bp->next = list->head;
    bp->prev = (free_block_t *)NULL;

    /* Make the old head of the list point to the new block. */
    ptr = bp->next;
    if (ptr) {
	rvmerr = rvm_set_range(tid, &(ptr->prev), sizeof(free_block_t *));
	if (rvmerr != RVM_SUCCESS) {
	    (*err) = (int) rvmerr;
	    return -1;
	}
	ptr->prev = bp;
    }
	
    /* Make the head point to the freed block */
    rvmerr = rvm_set_range(tid, list, sizeof(free_list_t));
    if (rvmerr != RVM_SUCCESS) {
	(*err) = (int) rvmerr;
	return -1;
    }
    list->head = bp;
    
    (*err) = SUCCESS;
    return 0;
}

