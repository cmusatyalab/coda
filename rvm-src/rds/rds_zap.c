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

#include <stdlib.h>
#include <stdio.h>
#include <rvm.h>
#include <rds.h>
#include <rds_private.h>
#include <rvm_segment.h>
    
/*
 * Put the heap in the first, or lower address range, and the statics in the
 * upper address range.
 */
int
rds_zap_heap(DevName, DevLength, startAddr, staticLength, heapLength, nlists, chunkSize, err)
     char 		*DevName;
     rvm_offset_t 	DevLength;
     char  		*startAddr;
     rvm_length_t 	staticLength;
     rvm_length_t	heapLength;
     unsigned long 	nlists;
     unsigned long 	chunkSize;
     int		*err;
{
    rvm_region_def_t regions[2], *loadregions;
    rvm_tid_t *tid;
    unsigned long n_loadregions;
    rvm_return_t rvmret;
    
    regions[0].length = heapLength;
    regions[0].vmaddr = startAddr;
    regions[1].length = staticLength;
    regions[1].vmaddr = startAddr + heapLength;
    /* Create an air bubble at the end? */
    /* Determine the length of the segment, and create a region which makes the
     * rest of it air. */

    /* Create the segments */
    rvmret = rvm_create_segment(DevName, DevLength, NULL, 2, regions);
    if (rvmret != RVM_SUCCESS) {
	(*err) = (int) rvmret;
	return -1;
    }

    /* Force the writes from create to appear in the data segment. */
    if ((rvmret = rvm_truncate()) != RVM_SUCCESS) {
	(*err) = (int) rvmret;
	return -1;
    }
    
    /* Map in the appropriate structures by calling Rvm_Load_Segment. */
    rvmret = rvm_load_segment(DevName, DevLength, NULL, &n_loadregions, &loadregions);
    if (rvmret != RVM_SUCCESS) {
	(*err) = (int) rvmret;
	return -1;
    }

    /* Total sanity checks -- since we just created the segment */
    if (n_loadregions != 2) {
	free(loadregions);
	*err = EBAD_SEGMENT_HDR;
	return -1;
    }
    
    free(loadregions);

    /* Start a transaction to initialize the heap */
    tid = rvm_malloc_tid();
    rvmret = rvm_begin_transaction(tid, restore);
    if (rvmret != RVM_SUCCESS) {
	(*err) = (int) rvmret;
	rvm_free_tid(tid);
	return -1;
    }

    *err = SUCCESS; 		/* Initialize the error value */
    rds_init_heap(startAddr, heapLength, chunkSize, nlists, tid, err);
    if (*err != SUCCESS) {
	rvm_abort_transaction(tid);
	rvm_free_tid(tid);
	return -1;
    }

    rvmret = rvm_end_transaction(tid, no_flush);
    if (rvmret != RVM_SUCCESS) {
	(*err) = (int) rvmret;
	rvm_free_tid(tid);
	return -1;
    }

    rvm_free_tid(tid);
    *err = SUCCESS;
    return 0;
}
