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

static char *rcsid = "$Header: rds_zap.c,v 1.1 96/11/22 13:40:03 raiff Exp $";
#endif _BLURB_

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
    int n_loadregions;
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
