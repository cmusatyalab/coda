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

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/rvm-src/rvm/rvm_unmap.c,v 1.1.1.1 1996/11/22 19:16:59 rvb Exp";
#endif _BLURB_

/*
*
*                   RVM  Unmapping
*
*/

#include "rvm_private.h"

/* global variables */

extern log_t    *default_log;           /* default log descriptor ptr */
extern int      errno;                  /* kernel error number */
extern char     *rvm_errmsg;            /* internal error message buffer */

extern
rw_lock_t       region_tree_lock;       /* lock for region tree */
extern
tree_node_t     *region_tree;           /* root of mapped region tree */
/* rvm_unmap */
rvm_return_t rvm_unmap(rvm_region)
    rvm_region_t        *rvm_region;    /* region to unmap */
    {
    rvm_return_t        retval;
    region_t            *region;        /* internal region descriptor */
    seg_t               *seg;           /* internal segment descriptor */

    if (bad_init()) return RVM_EINIT;
    if ((retval=bad_region(rvm_region)) != RVM_SUCCESS)
        return retval;

    /* find and lock region descriptor */
    region = find_whole_range(rvm_region->vmaddr,    /* begin region_tree_lock, */
                              rvm_region->length,w); /* region_lock crit sects */
    if (region == NULL)
        return RVM_ENOT_MAPPED;

    /* check if has uncommitted transactions */
    if (region->n_uncommit != 0)
        {
        retval = RVM_EUNCOMMIT;
        goto err_exit;
        }

    /* be sure whole region specified */
    if ((region->vmaddr != rvm_region->vmaddr) ||
        (region->length != rvm_region->length))
        {
        retval = RVM_ERANGE;
        goto err_exit;
        }

    /* remove from region tree and unlock tree */
    if (!tree_delete(&region_tree,(tree_node_t *)region->mem_region,
                      mem_total_include))
        ASSERT(rvm_false);              /* couldn't find node */
    rw_unlock(&region_tree_lock,w);     /* end region_tree_lock crit sect */
    rw_unlock(&region->region_lock,w);  /* end region_lock crit sect */

    /* unlink from seg's map_list */
    seg = region->seg;
    CRITICAL(seg->seg_lock,             /* begin seg_lock critical section */
        {
        (void)move_list_entry(&seg->map_list,NULL,
                              (list_entry_t *)region);

        /* if dirty, put on unmapped list; otherwise scrap */
        if (region->dirty)
            {
            make_uname(&region->unmap_ts); /* timestamp unmap */
            (void)move_list_entry(NULL,&seg->unmap_list,
                                  (list_entry_t *)region);
            }
        else
            free_region(region);
        });                             /* end seg_lock critical section */

    return RVM_SUCCESS;

err_exit:
    rw_unlock(&region->region_lock,w);
    rw_unlock(&region_tree_lock,w);
    return retval;
    }
