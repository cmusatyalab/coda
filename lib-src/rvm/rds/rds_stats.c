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

int rds_tracing = FALSE;
FILE *rds_tracing_file = NULL;
    
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
    printf(" Free bytes: \t %x\n", RDS_STATS.freebytes);
    printf(" Alloced bytes:\t %x\n", RDS_STATS.mallocbytes);
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

int rds_trace_on(FILE *file)
{
  assert(HEAP_INIT);
  assert(file);
  rds_tracing = TRUE;
  rds_tracing_file = file;
  RDS_LOG("rdstrace: tracing on\n");

  return 0;
}


int
rds_trace_off ()
{
  assert(HEAP_INIT);
  RDS_LOG("rdstrace: tracing off\n");
  rds_tracing = FALSE;
  return 0;
}

void rds_trace_dump_stats()
{
  RDS_LOG("rdstrace: start dump_stats\n");
  RDS_LOG("rdstrace: Free_bytes \t %x\n", RDS_STATS.freebytes);
  RDS_LOG("rdstrace: Alloced_bytes\t %x\n",
		       RDS_STATS.mallocbytes);
  RDS_LOG("rdstrace: Mallocs \t %d\n", RDS_STATS.malloc);
  RDS_LOG("rdstrace: Frees \t %d\n",  RDS_STATS.free);
  RDS_LOG("rdstrace: Preallocs \t %d\n",  RDS_STATS.prealloc);
  RDS_LOG("rdstrace: Hits \t\t %d\n",  RDS_STATS.hits);
  RDS_LOG("rdstrace: Misses \t %d\n",  RDS_STATS.misses);
  RDS_LOG("rdstrace: Large_Hits \t %d\n",  RDS_STATS.large_hits);
  RDS_LOG("rdstrace: Large_Misses \t %d\n",
		       RDS_STATS.large_misses);
  RDS_LOG("rdstrace: Coalesces \t %d\n",  RDS_STATS.coalesce);
  RDS_LOG("rdstrace: Merges \t %d\n", RDS_STATS.merged);
  RDS_LOG("rdstrace: Not_Merged \t %d\n", RDS_STATS.unmerged);
  RDS_LOG("rdstrace: Large_List %d\n", RDS_STATS.large_list);
  RDS_LOG("rdstrace: stop dump_stats\n");
}

void rds_trace_dump_free_lists()
{
  int i, j;
  free_block_t *fbp, *ptr;
  
  RDS_LOG("rdstrace: start dump_free_lists\n");
  
  for (i = 1; i < RDS_NLISTS + 1; i++) {

    fbp = RDS_FREE_LIST[i].head;
    
    if (RDS_FREE_LIST[i].guard != FREE_LIST_GUARD)
      RDS_LOG("rdstrace: Error!!! Bad guard on list %d!!!\n", i);
    
    if (fbp && (fbp->prev != (free_block_t *)NULL))
      RDS_LOG("rdstrace: Error!!! Non-null Initial prev pointer.\n");
    
    j = 0;
    while (fbp != NULL) {
      j++;

      if (i == RDS_MAXLIST) {
	RDS_LOG("rdstrace: size %ld count 1\n", fbp->size);
      }
      
      if (fbp->type != FREE_GUARD)
	RDS_LOG("rdstrace: Error!!! Bad lowguard on block\n");
      
      if ((*BLOCK_END(fbp)) != END_GUARD)
	RDS_LOG("rdstrace: Error!!! Bad highguard, %p=%lx\n",
			     BLOCK_END(fbp), *BLOCK_END(fbp));
      
      ptr = fbp->next;
      
      if (ptr && (ptr->prev != fbp))
	RDS_LOG("rdstrace: Error!!! Bad chain link %p <-> %p\n",
			     fbp, ptr);
      
      if (i != RDS_MAXLIST && fbp->size != i)
	RDS_LOG("rdstrace: Error!!! OBJECT IS ON WRONG LIST!!!!\n");
      
      fbp = fbp->next;
    }
    
    if (i != RDS_MAXLIST)
      RDS_LOG("rdstrace: size %d count %d\n", i, j);
  }
  RDS_LOG("rdstrace: stop dump_free_lists\n");
}

void rds_trace_dump_blocks()
{
  int same;
  free_block_t *fbp, *next_fbp;
  
  RDS_LOG("rdstrace: start dump_blocks\n");
  
  fbp = (free_block_t *)((char *)&(((free_list_t *)RDS_FREE_LIST)[RDS_NLISTS])
			 + sizeof(free_list_t));
  same = 1;
  
  while ((char *)fbp < (char *)RDS_HIGH_ADDR) {
    
    if ((fbp->type != FREE_GUARD) && (fbp->type != ALLOC_GUARD))
      RDS_LOG("rdstrace: Error!!! Bad lowguard on block\n");
    
    if ((*BLOCK_END(fbp)) != END_GUARD)
      RDS_LOG("rdstrace: Error!!! Bad highguard, %p=%lx\n",
			   BLOCK_END(fbp), *BLOCK_END(fbp));
    
    next_fbp = NEXT_CONSECUTIVE_BLOCK(fbp);
    
    RDS_LOG("rdstrace: addr %p size %ld %s\n",
			 fbp,
			 fbp->size * RDS_CHUNK_SIZE,
			 (fbp->type == FREE_GUARD ? "free":"alloc"));


    /* This code let's common sizes be printed together.  For now need individual
       addresses so can't use this.
       
    if ((char *)next_fbp < (char *)RDS_HIGH_ADDR) {
      if ((fbp->type == next_fbp->type) && (fbp->size == next_fbp->size)) {
	same++;
      } else {
	RDS_LOG("rdstrace: %s size %d count %d\n",
			     (fbp->type == FREE_GUARD ? "free":"alloc"),
			     fbp->size, same);
	same = 1;
      }
    } else {
      RDS_LOG("rdstrace: %s size %d count %d\n",
			   (fbp->type == FREE_GUARD ? "free":"alloc"),
			   fbp->size, same);
    }
    */
    
    fbp = next_fbp;
  }
  
  RDS_LOG("rdstrace: stop dump_blocks\n");
}

int
rds_trace_dump_heap ()
{
  assert(HEAP_INIT);
    CRITICAL({
      RDS_LOG("rdstrace: start heap_dump\n");
      RDS_LOG("rdstrace: version_string %s\n", RDS_VERSION_STAMP);
      RDS_LOG("rdstrace: heaplength %ld\n", RDS_HEAPLENGTH);
      RDS_LOG("rdstrace: chunk_size %ld\n", RDS_CHUNK_SIZE);
      RDS_LOG("rdstrace: nlists %ld\n", RDS_NLISTS);
      rds_trace_dump_stats();
      RDS_LOG("rdstrace: maxlist %ld\n", RDS_MAXLIST);
      rds_trace_dump_free_lists();
      rds_trace_dump_blocks();
      RDS_LOG("rdstrace: stop heap_dump\n");
    });
  return 0;
}
