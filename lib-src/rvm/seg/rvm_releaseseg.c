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

#include <rvm/rvm.h>
#include <rvm/rvm_segment.h>
#include "rvm_segment_private.h"

/* release regions of a segment */
rvm_return_t
rvm_release_segment (
    unsigned long       nregions,     /* number of regions mapped */
    rvm_region_def_t    **regions)    /* array of region descriptors */
{
    rvm_region_t *region = rvm_malloc_region();
    rvm_return_t err;
    int i;

    for (i = 0; i < nregions; i++) {
        region->offset = (*regions)[i].offset;
        region->length = (*regions)[i].length;
        region->vmaddr = (*regions)[i].vmaddr;

        err = rvm_unmap(region);
        if (err != RVM_SUCCESS)
            printf("release_segment unmap failed %s\n", rvm_return(err));

        deallocate_vm(region->vmaddr, region->length);
    }
    rvm_free_region(region);
    free(*regions);
}

