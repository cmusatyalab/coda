/* BLURB lgpl

                           Coda File System
                              Release 7

          Copyright (c) 1987-2019 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights
                           none currently
#*/

#include <stdlib.h>
#include <string.h>
#include <rvm/rvm.h>
#include <rvm/rvm_segment.h>
#include "rvm_segment_private.h"

/* from rvm_private.h */
rvm_bool_t rvm_register_page(char *vmaddr, rvm_length_t length);

/* Global variables */

extern rvm_bool_t rvm_map_private; /* Do we map private or not. */

/* Here's a hack to help debug the file server -- save the amount of space
 * mapped in so the file server can dump it out again. -- Just a hack.
 */
long rds_rvmsize;
char *rds_startaddr;

/*
 * rvm_load_segment 
 * - locates the place in the process's address where to load segments
 * - then maps in the recoverable regions at that point. 
 * - it returns an array of the region descriptors.
 */
rvm_return_t rvm_load_segment(char *DevName, rvm_offset_t DevLength,
                              rvm_options_t *options, unsigned long *nregions,
                              rvm_region_def_t **regions)
{
    rvm_region_t *region     = rvm_malloc_region();
    rvm_region_t *hdr_region = rvm_malloc_region();
    rvm_segment_hdr_t *hdrp;
    rvm_return_t err;
    int i;

    /* HACK */ rds_rvmsize = 0; /* HACK */

    /* Read in the header region of the segment. */
    hdr_region->data_dev   = strdup(DevName);
    hdr_region->dev_length = DevLength; /* Struct assignment */
    RVM_ZERO_OFFSET(hdr_region->offset);
    hdr_region->length = RVM_SEGMENT_HDR_SIZE;
    hdr_region->vmaddr = 0;

    hdr_region->vmaddr = NULL;
    if (!rvm_map_private) {
        err = allocate_vm(&(hdr_region->vmaddr), hdr_region->length);
        if (err != RVM_SUCCESS)
            return err;
    }
    /* else, as vmaddr is NULL, the segment will be pre-allocated and
     * registered by rvm_map->establish_range->round_region->page_alloc -JH */

    err = rvm_map(hdr_region, options);
    if (err != RVM_SUCCESS)
        return err; /* Some error condition exists, return the error code */

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
    region->data_dev   = strdup(DevName);
    region->dev_length = DevLength; /* Struct assignment */

    /* Setup return region definition array */
    (*nregions) = hdrp->nregions;
    (*regions) =
        (rvm_region_def_t *)malloc(sizeof(rvm_region_def_t) * (*nregions));

    /* HACK */ rds_startaddr = hdrp->regions[0].vmaddr; /* HACK */

    for (i = 0; i < hdrp->nregions; i++)
        if ((unsigned long)(hdrp->regions[i].vmaddr) >= 0) {
            region->offset = (*regions)[i].offset = hdrp->regions[i].offset;
            region->length = (*regions)[i].length = hdrp->regions[i].length;
            region->vmaddr = (*regions)[i].vmaddr = hdrp->regions[i].vmaddr;

            /* HACK */ rds_rvmsize += region->length; /* HACK */

            if (!rvm_map_private) {
                err = allocate_vm(&(region->vmaddr), region->length);
                if (err != RVM_SUCCESS)
                    return err;
            } else if (!rvm_register_page(region->vmaddr, region->length))
                return RVM_EINTERNAL;

            err = rvm_map(region, options);
            if (err != RVM_SUCCESS)
                return err; /* Some error condition exists, abort */
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
    rvm_free_region(region);
    return err;
}
