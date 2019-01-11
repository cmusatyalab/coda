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

/*
*
*                   RVM  Unmapping
*
*/

#include "rvm_private.h"

/* global variables */

extern log_t *default_log; /* default log descriptor ptr */
extern char *rvm_errmsg; /* internal error message buffer */

extern rw_lock_t region_tree_lock; /* lock for region tree */
extern tree_node_t *region_tree; /* root of mapped region tree */
/* rvm_unmap */
rvm_return_t
    rvm_unmap(rvm_region) rvm_region_t *rvm_region; /* region to unmap */
{
    rvm_return_t retval;
    region_t *region; /* internal region descriptor */
    seg_t *seg; /* internal segment descriptor */

    if (bad_init())
        return RVM_EINIT;
    if ((retval = bad_region(rvm_region)) != RVM_SUCCESS)
        return retval;

    /* find and lock region descriptor */
    region = find_whole_range(rvm_region->vmaddr, /* begin region_tree_lock, */
                              rvm_region->length,
                              w); /* region_lock crit sects */
    if (region == NULL)
        return RVM_ENOT_MAPPED;

    /* check if has uncommitted transactions */
    if (region->n_uncommit != 0) {
        retval = RVM_EUNCOMMIT;
        goto err_exit;
    }

    /* be sure whole region specified */
    if ((region->vmaddr != rvm_region->vmaddr) ||
        (region->length != rvm_region->length)) {
        retval = RVM_ERANGE;
        goto err_exit;
    }

    /* remove from region tree and unlock tree */
    if (!tree_delete(&region_tree, (tree_node_t *)region->mem_region,
                     mem_total_include))
        assert(rvm_false); /* couldn't find node */
    rw_unlock(&region_tree_lock, w); /* end region_tree_lock crit sect */
    rw_unlock(&region->region_lock, w); /* end region_lock crit sect */

    /* unlink from seg's map_list */
    seg = region->seg;
    CRITICAL(seg->seg_lock, /* begin seg_lock critical section */
             {
                 (void)move_list_entry(&seg->map_list, NULL,
                                       (list_entry_t *)region);

                 /* if dirty, put on unmapped list; otherwise scrap */
                 if (region->dirty) {
                     make_uname(&region->unmap_ts); /* timestamp unmap */
                     (void)move_list_entry(NULL, &seg->unmap_list,
                                           (list_entry_t *)region);
                 } else
                     free_region(region);
             }); /* end seg_lock critical section */

    return RVM_SUCCESS;

err_exit:
    rw_unlock(&region->region_lock, w);
    rw_unlock(&region_tree_lock, w);
    return retval;
}
