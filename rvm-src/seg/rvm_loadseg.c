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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/rvm-src/seg/Attic/rvm_loadseg.c,v 4.4 1998/05/15 01:24:10 braam Exp $";
#endif _BLURB_

#include <stdlib.h>
#include <rvm.h>
#include "rvm_segment.h"
#include "rvm_segment_private.h"

/* Here's a hack to help debug the file server -- save the amount of space
 * mapped in so the file server can dump it out again. -- Just a hack.
 */
long rds_rvmsize;
char *rds_startaddr;

/*
 * rvm_load_segment locates the place in the process's address space at which the
 * already initialized memory, then maps in the recoverable regions at that
 * point. It returns an array of the region descriptors.
 */
rvm_return_t
rvm_load_segment(DevName, DevLength, options, nregions, regions)
     char *DevName;
     rvm_offset_t DevLength;
     rvm_options_t *options;
     unsigned long *nregions;
     rvm_region_def_t *regions[];
{
    rvm_region_t *region = rvm_malloc_region();
    rvm_region_t *hdr_region = rvm_malloc_region();
    rvm_segment_hdr_t *hdrp;
    rvm_return_t err;
    int i;

    /* HACK */ rds_rvmsize = 0; /* HACK */
    
    /* Read in the header region of the segment. */
    hdr_region->data_dev = DevName;
    hdr_region->dev_length = DevLength;		/* Struct assignment */
    RVM_ZERO_OFFSET(hdr_region->offset);
    hdr_region->length = RVM_SEGMENT_HDR_SIZE;

    hdr_region->vmaddr = NULL;
    err = allocate_vm(&(hdr_region->vmaddr), hdr_region->length);
    if (err != RVM_SUCCESS)
	return err;
    
    err = rvm_map(hdr_region,options);
    if (err != RVM_SUCCESS)
	return err; 	/* Some error condition exists, return the error code */
    
    hdrp = (rvm_segment_hdr_t *)(hdr_region->vmaddr);

    /* Make sure struct_id is correct */
    if (hdrp->struct_id != rvm_segment_hdr_id)
	return (rvm_return_t)RVM_ESEGMENT_HDR;
    
    /* Match version stamps */
    if (strcmp(hdrp->version, RVM_SEGMENT_VERSION) != 0)
	return RVM_EVERSION_SKEW;

    /* Make sure the regions do not overlap */
    if (overlap(hdrp->nregions, hdrp->regions))
	return RVM_EVM_OVERLAP;
    
    /* Map in the regions */
    region->data_dev = DevName; 
    region->dev_length = DevLength;		/* Struct assignment */
    
    /* Setup return region definition array */
    (*nregions) = hdrp->nregions;
    (*regions) = (rvm_region_def_t *)malloc(sizeof(rvm_region_def_t)*(*nregions));

    /* HACK */ rds_startaddr = hdrp->regions[0].vmaddr; /* HACK */
    for (i = 0; i < hdrp->nregions; i++) 
	if ((unsigned int)(hdrp->regions[i].vmaddr) >= 0) {
	    region->offset = (*regions)[i].offset = hdrp->regions[i].offset;
	    region->length = (*regions)[i].length = hdrp->regions[i].length;
	    region->vmaddr = (*regions)[i].vmaddr = hdrp->regions[i].vmaddr;

	    /* HACK */ rds_rvmsize += region->length; /* HACK */
	    
	    err = allocate_vm(&(region->vmaddr), region->length);
	    if (err != RVM_SUCCESS)
		return err;

	    err = rvm_map(region, options);
	    if (err != RVM_SUCCESS)
		return err; 	/* Some error condition exists, abort */

	    printf("Just mapped in region (%x,%d)\n",region->vmaddr, region->length);
	}

    /* Clean up, we no longer need the header region */
    switch (err = rvm_unmap(hdr_region)) {
      case RVM_EREGION:
      case RVM_EUNCOMMIT:
      case RVM_ENOT_MAPPED:
      case RVM_ERANGE:
	deallocate_vm(hdr_region->vmaddr, hdr_region->length);
	return err;
	break;
      default:
	/* do nothing */
	break;
    }

    err = deallocate_vm(hdr_region->vmaddr, hdr_region->length);

    rvm_free_region(hdr_region);
    return err;
}
