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

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/rvm-src/seg/rvm_createseg.c,v 1.1.1.1 1996/11/22 19:17:13 rvb Exp";
#endif _BLURB_

#include <stdio.h>
#include <rvm.h>
#include "rvm_segment.h"
#include "rvm_segment_private.h"

/* rvm_create_segment erases the old contents of the recoverable segment, and 
 * write a new structure to it. The arguments specify the number of regions, and 
 * the form of each region in the new segment structure. It is important to 
 * realize that all information that used to exist in the segment will 
 * no longer be accessible.
 */
rvm_return_t 
rvm_create_segment(DevName, DevLength, options, nregions, regionDefs) 
     char 	    	*DevName;
     rvm_offset_t	DevLength;
     rvm_options_t  	*options;
     rvm_length_t	nregions; 
     rvm_region_def_t   regionDefs[];
{
    rvm_region_t *region = rvm_malloc_region();
    rvm_segment_hdr_t *hptr;
    rvm_offset_t offset;
    rvm_tid_t *tid;
    rvm_return_t err;
    int i;

    /* Make sure the region definitions do not overlap. */
    if (overlap(nregions, regionDefs))
	return RVM_ERANGE;
	
    /* Erase the old contents of the segment, including entries in the log */

    /* Map in the first RVM_SEGMENT_HDR_SIZE bytes of the segment */
    
    region->data_dev = DevName;
    region->dev_length = DevLength;
    RVM_ZERO_OFFSET(region->offset);
    region->length = RVM_SEGMENT_HDR_SIZE;
    region->vmaddr = 0;
    
    /* allocate the address range for this region */
    err = allocate_vm(&(region->vmaddr), region->length);
    if (err != RVM_SUCCESS) {
	rvm_free_region(region);
	return err;
    }
    
    err = rvm_map(region, options);
    if (err != RVM_SUCCESS) {
	rvm_free_region(region);
	return err; 	/* Some error condition exists, return the error code */
    }
    
    tid = rvm_malloc_tid();
    err = rvm_begin_transaction(tid, restore);
    if (err != RVM_SUCCESS) {
	rvm_free_tid(tid);
	rvm_free_region(region);
	return err;
    }
    
    /* Set up the header region. This is always a fixed size. */
    hptr = (rvm_segment_hdr_t *) region->vmaddr;

    err = rvm_set_range(tid, (char *)hptr, RVM_SEGMENT_HDR_SIZE);
    if (err != RVM_SUCCESS) {
	rvm_abort_transaction(tid);
	rvm_free_tid(tid);
	rvm_free_region(region);
	return err;
    }

    hptr->struct_id = rvm_segment_hdr_id;
    strcpy(hptr->version, RVM_SEGMENT_VERSION);
    hptr->nregions = nregions;

    /* First region goes right after segment header */
    RVM_ZERO_OFFSET(offset);
    offset = RVM_ADD_LENGTH_TO_OFFSET(offset, RVM_SEGMENT_HDR_SIZE);

    /* For each region definition, set it to start at the next available spot
     * in the segment, fill in the length and vmaddr fields, and
     * determine the next available spot in the segment.
     */
    for (i = 0; i < nregions; i++) {
	hptr->regions[i].offset = offset;
	hptr->regions[i].length = regionDefs[i].length;
	hptr->regions[i].vmaddr = regionDefs[i].vmaddr;
	/* printf("Creating region at offset %x,%x , vmaddr %x, len %d\n",
	       hptr->regions[i].offset.high, hptr->regions[i].offset.low,
	       hptr->regions[i].vmaddr, hptr->regions[i].length); */
	offset = RVM_ADD_LENGTH_TO_OFFSET(offset, regionDefs[i].length);
    }

    err = rvm_end_transaction(tid, flush);
    rvm_free_tid(tid);
    if (err != RVM_SUCCESS) {
	rvm_free_region(region);
	return err;
    }
	
    /* The segment should now be all set to go, clean up. */
    err = rvm_unmap(region);
    if (err != RVM_SUCCESS)
	printf("create_segment unmap failed %s\n", rvm_return(err));

    deallocate_vm(region->vmaddr, region->length);

    rvm_free_region(region);
    return err;
}
