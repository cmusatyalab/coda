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

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/rvm-src/rds/rds_stats.c,v 1.1.1.1 1996/11/22 18:39:56 rvb Exp";
#endif _BLURB_


#include <stdio.h>
#include <rvm.h>
#include <rds.h>
#include <rds_private.h>
#include <rvm_segment.h>

int tracing_rds = FALSE;
void (*rds_trace_printer) ();
    
/*
 * Print out the current statistics
 */
int
rds_print_stats()
{
    if (!HEAP_INIT)	/* Make sure RecoverableHeapStartAddress is inited */
	return -1;	

    /* Not making this a critical section since a race condition only reports
       slightly bogus statistics -- and off by one over several thousand isn't
       significant. */
       
    printf("Number of\n");
    printf(" Free bytes: \t %d\n", RDS_STATS.freebytes);
    printf(" Alloced bytes:\t %d\n", RDS_STATS.mallocbytes);
    printf(" Mallocs: \t %d\n", RDS_STATS.malloc);
    printf(" Frees: \t %d\n",  RDS_STATS.free);
    printf(" Preallocs: \t %d\n",  RDS_STATS.prealloc);
    printf(" Hits: \t\t %d\n",  RDS_STATS.hits);
    printf(" Misses: \t %d\n",  RDS_STATS.misses);
    printf(" Large Hits: \t %d\n",  RDS_STATS.large_hits);
    printf(" Large Misses: \t %d\n",  RDS_STATS.large_misses);
    printf(" Coalesces: \t %d\n",  RDS_STATS.coalesce);
    printf(" Merges \t %d\n", RDS_STATS.merged);
    printf(" Not Merged: \t %d\n", RDS_STATS.unmerged);
    printf(" Times the Large List pointer has changed: %d\n", RDS_STATS.large_list);

    return 0;
}

/* Zero out the stats structure. */
int
rds_clear_stats(err)
     int *err;
{
    rvm_return_t rvmret;
    rvm_tid_t *atid = rvm_malloc_tid();

    rvmret = rvm_begin_transaction(atid, restore);
    if (rvmret != RVM_SUCCESS) {
	(*err) = (int)rvmret;
	rvm_free_tid(atid);
	return -1;
    }    

    CRITICAL({
	rvmret = rvm_set_range(atid, &RDS_STATS, sizeof(rds_stats_t));
	if (rvmret == RVM_SUCCESS) 
	    BZERO(&RDS_STATS, sizeof(rds_stats_t));
    });

    if (rvmret != RVM_SUCCESS) {
	rvm_abort_transaction(atid);
	(*err) = (int)rvmret;
	rvm_free_tid(atid);
	return -1;
    }
    
    rvmret = rvm_end_transaction(atid, no_flush);
    if (rvmret != RVM_SUCCESS) {
	(*err) = (int)rvmret;
	rvm_free_tid(atid);
	return -1; /* should I abort here just in case? */
    }
	
    *err = SUCCESS;
    rvm_free_tid(atid);
    return 0;
}

/*
 * Return a structure initialized from the statistics in the heap header.
 * Like print_stats, this really doesn't need to be critical -- dcs 1/29
 */

int rds_get_stats(stats)
     rds_stats_t *stats;
{
    if (stats == NULL)    /* stats structure must be already allocated */
	return EBAD_ARGS;

    BCOPY(&RDS_STATS, stats, sizeof(rds_stats_t));
    return 0;
}

int rds_trace_on(printer)
     void (*printer) (char *, ...);
{
  ASSERT(HEAP_INIT);
  tracing_rds = TRUE;
  rds_trace_printer = printer;

  (*rds_trace_printer)("rdstrace: tracing on\n");

  return 0;
}


int
rds_trace_off ()
{
  ASSERT(HEAP_INIT);
  if (tracing_rds) {
    (*rds_trace_printer)("rdstrace: tracing off\n");
    tracing_rds = FALSE;
  }
  return 0;
}

void rds_trace_dump_stats()
{
  (*rds_trace_printer)("rdstrace: start dump_stats\n");
  (*rds_trace_printer)("rdstrace: Free_bytes \t %d\n", RDS_STATS.freebytes);
  (*rds_trace_printer)("rdstrace: Alloced_bytes\t %d\n",
		       RDS_STATS.mallocbytes);
  (*rds_trace_printer)("rdstrace: Mallocs \t %d\n", RDS_STATS.malloc);
  (*rds_trace_printer)("rdstrace: Frees \t %d\n",  RDS_STATS.free);
  (*rds_trace_printer)("rdstrace: Preallocs \t %d\n",  RDS_STATS.prealloc);
  (*rds_trace_printer)("rdstrace: Hits \t\t %d\n",  RDS_STATS.hits);
  (*rds_trace_printer)("rdstrace: Misses \t %d\n",  RDS_STATS.misses);
  (*rds_trace_printer)("rdstrace: Large_Hits \t %d\n",  RDS_STATS.large_hits);
  (*rds_trace_printer)("rdstrace: Large_Misses \t %d\n",
		       RDS_STATS.large_misses);
  (*rds_trace_printer)("rdstrace: Coalesces \t %d\n",  RDS_STATS.coalesce);
  (*rds_trace_printer)("rdstrace: Merges \t %d\n", RDS_STATS.merged);
  (*rds_trace_printer)("rdstrace: Not_Merged \t %d\n", RDS_STATS.unmerged);
  (*rds_trace_printer)("rdstrace: Large_List %d\n", RDS_STATS.large_list);
  (*rds_trace_printer)("rdstrace: stop dump_stats\n");
}

void rds_trace_dump_free_lists()
{
  int i, j;
  free_block_t *fbp, *ptr;
  
  (*rds_trace_printer)("rdstrace: start dump_free_lists\n");
  
  for (i = 1; i < RDS_NLISTS + 1; i++) {

    fbp = RDS_FREE_LIST[i].head;
    
    if (RDS_FREE_LIST[i].guard != FREE_LIST_GUARD)
      (*rds_trace_printer)("rdstrace: Error!!! Bad guard on list %d!!!\n", i);
    
    if (fbp && (fbp->prev != (free_block_t *)NULL))
      (*rds_trace_printer)("rdstrace: Error!!! Non-null Initial prev pointer.\n");
    
    j = 0;
    while (fbp != NULL) {
      j++;

      if (i == RDS_MAXLIST) {
	(*rds_trace_printer)("rdstrace: size %d count 1\n", fbp->size);
      }
      
      if (fbp->type != FREE_GUARD)
	(*rds_trace_printer)("rdstrace: Error!!! Bad lowguard on block\n");
      
      if ((*BLOCK_END(fbp)) != END_GUARD)
	(*rds_trace_printer)("rdstrace: Error!!! Bad highguard, %x=%x\n",
			     BLOCK_END(fbp), *BLOCK_END(fbp));
      
      ptr = fbp->next;
      
      if (ptr && (ptr->prev != fbp))
	(*rds_trace_printer)("rdstrace: Error!!! Bad chain link %x <-> %x\n",
			     fbp, ptr);
      
      if (i != RDS_MAXLIST && fbp->size != i)
	(*rds_trace_printer)("rdstrace: Error!!! OBJECT IS ON WRONG LIST!!!!\n");
      
      fbp = fbp->next;
    }
    
    if (i != RDS_MAXLIST)
      (*rds_trace_printer)("rdstrace: size %d count %d\n", i, j);
  }
  (*rds_trace_printer)("rdstrace: stop dump_free_lists\n");
}

void rds_trace_dump_blocks()
{
  int same;
  free_block_t *fbp, *next_fbp;
  
  (*rds_trace_printer)("rdstrace: start dump_blocks\n");
  
  fbp = (free_block_t *)((char *)&(((free_list_t *)RDS_FREE_LIST)[RDS_NLISTS])
			 + sizeof(free_list_t));
  same = 1;
  
  while ((char *)fbp < (char *)RDS_HIGH_ADDR) {
    
    if ((fbp->type != FREE_GUARD) && (fbp->type != ALLOC_GUARD))
      (*rds_trace_printer)("rdstrace: Error!!! Bad lowguard on block\n");
    
    if ((*BLOCK_END(fbp)) != END_GUARD)
      (*rds_trace_printer)("rdstrace: Error!!! Bad highguard, %x=%x\n",
			   BLOCK_END(fbp), *BLOCK_END(fbp));
    
    next_fbp = NEXT_CONSECUTIVE_BLOCK(fbp);
    
    (*rds_trace_printer)("rdstrace: addr %d size %d %s\n",
			 fbp,
			 fbp->size * RDS_CHUNK_SIZE,
			 (fbp->type == FREE_GUARD ? "free":"alloc"));


    /* This code let's common sizes be printed together.  For now need individual
       addresses so can't use this.
       
    if ((char *)next_fbp < (char *)RDS_HIGH_ADDR) {
      if ((fbp->type == next_fbp->type) && (fbp->size == next_fbp->size)) {
	same++;
      } else {
	(*rds_trace_printer)("rdstrace: %s size %d count %d\n",
			     (fbp->type == FREE_GUARD ? "free":"alloc"),
			     fbp->size, same);
	same = 1;
      }
    } else {
      (*rds_trace_printer)("rdstrace: %s size %d count %d\n",
			   (fbp->type == FREE_GUARD ? "free":"alloc"),
			   fbp->size, same);
    }
    */
    
    fbp = next_fbp;
  }
  
  (*rds_trace_printer)("rdstrace: stop dump_blocks\n");
}

int
rds_trace_dump_heap ()
{
  ASSERT(HEAP_INIT);
  if (tracing_rds) {
    CRITICAL({
      (*rds_trace_printer)("rdstrace: start heap_dump\n");
      (*rds_trace_printer)("rdstrace: version_string %s\n", RDS_VERSION_STAMP);
      (*rds_trace_printer)("rdstrace: heaplength %d\n", RDS_HEAPLENGTH);
      (*rds_trace_printer)("rdstrace: chunk_size %d\n", RDS_CHUNK_SIZE);
      (*rds_trace_printer)("rdstrace: nlists %d\n", RDS_NLISTS);
      rds_trace_dump_stats();
      (*rds_trace_printer)("rdstrace: maxlist %d\n", RDS_MAXLIST);
      rds_trace_dump_free_lists();
      rds_trace_dump_blocks();
      (*rds_trace_printer)("rdstrace: stop heap_dump\n");
    });
  }
  return 0;
}
