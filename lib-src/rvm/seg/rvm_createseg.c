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
#include <assert.h>

#include <rvm/rvm.h>
#include <rvm/rvm_segment.h>
#include "rvm_segment_private.h"

/* rvm_create_segment erases the old contents of the recoverable
 * segment, and write a new structure to it. The arguments specify the
 * number of regions, and the form of each region in the new segment
 * structure. It is important to realize that all information that
 * used to exist in the segment will no longer be accessible.  */

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

    assert( nregions <= RVM_MAX_REGIONS );
	
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

    /* XXXXXX this needs a check to bound the number of regions */

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

