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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/rvm-src/rvm/Attic/rvm_status.c,v 4.2 1997/04/01 01:55:57 clement Exp $";
#endif _BLURB_

/*
*
*                   RVM Global Options
*
*/

#include "rvm_private.h"

/* global variables */
extern log_t        *default_log;       /* default log descriptor ptr */
extern int          errno;              /* kernel error number */
extern rvm_bool_t   rvm_utlsw;          /* true if call by rvmutl */
extern char         *rvm_errmsg;        /* internal error message buffer */
extern rvm_length_t rvm_max_read_len;   /* maximum Mach read length */
extern rvm_length_t flush_times_vec[flush_times_len]; /* flush timing histogram defs */
extern rvm_length_t truncation_times_vec[truncation_times_len]; /* truncation timing 
                                                                   histogram defs */
extern rvm_length_t range_lengths_vec[range_lengths_len]; /* range length
                                                             histogram defs */
extern rvm_length_t range_overlaps_vec[range_overlaps_len]; /* range coalesce
                                                             histogram defs */
extern rvm_length_t trans_overlaps_vec[trans_overlaps_len]; /* trans coalesce
                                                             histogram defs */
extern rvm_length_t range_elims_vec[range_elims_len]; /* ranges eliminated by range
                                                         coalesce histogram defs */
extern rvm_length_t trans_elims_vec[trans_elims_len]; /* ranges eliminated by trans
                                                         coalesce histogram defs */
rvm_length_t        trans_coalesces_vec[trans_coalesces_len]; /* transactions
                                                                 coalesed per flush */

rvm_length_t        rvm_optimizations;  /* optimizations switches */

/* version strings */
char                rvm_version[RVM_VERSION_MAX] =
                        {RVM_VERSION};
char                rvm_log_version[RVM_VERSION_MAX] =
                        {RVM_LOG_VERSION};
char                rvm_statistics_version[RVM_VERSION_MAX] =
                        {RVM_STATISTICS_VERSION};
char                rvm_release[RVM_VERSION_MAX] =
                        {"RVM Release 11 Jan 1993"};

/* local macros */
#define TID_ARRAY_REALLOC_INCR  5       /* allocate tid_array 5 elements at a
                                           time */
/* rvm_set_options */
rvm_return_t rvm_set_options(rvm_options)
    rvm_options_t   *rvm_options;
    {
    rvm_return_t    retval;

    /* be sure RVM is initialized */
    if (bad_init()) return RVM_EINIT;

    /* must have an options record here */
    if ((retval=bad_options(rvm_options)) != RVM_SUCCESS)
        return retval;                  /* bad options ptr or record */
    if (rvm_options == NULL)
        return RVM_EOPTIONS;

    /* check validity of options record & ptr */
    return do_rvm_options(rvm_options);

    }
/* structure validation */
rvm_return_t bad_statistics(rvm_statistics)
    rvm_statistics_t   *rvm_statistics;
    {
    if (rvm_statistics == NULL)
        return RVM_SUCCESS;
    if (rvm_statistics->struct_id != rvm_statistics_id)
        return RVM_ESTATISTICS;

    return RVM_SUCCESS;
    }

rvm_return_t bad_options(rvm_options,chk_log_dev)
    rvm_options_t   *rvm_options;
    rvm_bool_t      chk_log_dev;
    {
    if (rvm_options == NULL)
        return RVM_SUCCESS;
    if (rvm_options->struct_id != rvm_options_id)
        return RVM_EOPTIONS;

    if (chk_log_dev && (rvm_options->log_dev != NULL))
        if (strlen(rvm_options->log_dev) > (MAXPATHLEN-1))
            return RVM_ENAME_TOO_LONG;

    return RVM_SUCCESS;
    }
/* rvm options processing */
rvm_return_t do_rvm_options(rvm_options)
    rvm_options_t   *rvm_options;
    {
    log_t           *log;               /* log descriptor */
    rvm_return_t    retval;

    if (rvm_options != NULL)
        {
        /* set up maximum read length for large transafers */
        rvm_options->max_read_len =
            CHOP_TO_SECTOR_SIZE(rvm_options->max_read_len);
        if (rvm_options->max_read_len < SECTOR_SIZE)
            rvm_options->max_read_len = MAX_READ_LEN;
        rvm_max_read_len = rvm_options->max_read_len;

        /* do log - modifying options */
        if ((retval=do_log_options(&log,rvm_options)) != RVM_SUCCESS)
            return retval;

        /* set optimizations */
        rvm_optimizations = rvm_options->flags & (RVM_ALL_OPTIMIZATIONS);
        if (rvm_optimizations & RVM_COALESCE_TRANS)
            rvm_optimizations |= RVM_COALESCE_RANGES;
        }

    return RVM_SUCCESS;
    }
/* rvm_query */
rvm_return_t rvm_query(rvm_options,rvm_region)
    rvm_options_t       *rvm_options;
    rvm_region_t        *rvm_region;
    {
    log_t               *log;           /* log descriptor */
    log_status_t        *status;        /* log status area descriptor */
    int_tid_t           *tid;           /* transaction descriptor */
    region_t            *region=NULL;   /* mapped region descriptor */
    range_t             *range;         /* tid modification range */
    rvm_length_t        n_tids = 0;     /* number of tids found */
    rvm_bool_t          copy_tid = rvm_false; /* copy tid if true */
    rvm_return_t        retval;

    /* be sure RVM is initialized */
    if (bad_init()) return RVM_EINIT;

    /* check validity of region record & ptr */
    if (rvm_region != NULL)
        if (bad_region(rvm_region))
            return RVM_EREGION;

    /* check validity of options record */
    if (rvm_options == NULL) return RVM_EOPTIONS;
    if ((retval=bad_options(rvm_options,rvm_false)) != RVM_SUCCESS)
        return retval;

    /* set fields for log options */
    if (default_log != NULL)
        {
        log = default_log;
        status = &default_log->status;

        /* set log device name if buffer supplied */
        if (rvm_options->log_dev != NULL)
            (void)strcpy(rvm_options->log_dev,log->dev.name);
        rvm_options->truncate = log->daemon.truncate;
        rvm_options->recovery_buf_len = log->log_buf.length;
        rvm_options->flush_buf_len = log->dev.wrt_buf_len;

        /* log truncation fields */
        CRITICAL(log->dev_lock,         /* begin dev_lock crit sec */
            {
            rvm_options->log_empty = rvm_false;
            if (RVM_OFFSET_EQL_ZERO(status->prev_log_head))
                if (RVM_OFFSET_EQL(status->log_head,status->log_tail))
                    rvm_options->log_empty = rvm_true;
            });                         /* end dev_lock crit sec */
        /* if region specified, look it up */
        if (rvm_region != NULL)
            {                           /* begin region_lock crit sect */
            region = find_whole_range(rvm_region->vmaddr,
                                      rvm_region->length,r);
            if (region == NULL)
                return RVM_ENOT_MAPPED; /* not locked if not found
                                                       */
            }

        /* count uncommitted transactions */
        CRITICAL(log->tid_list_lock,    /* begin log tid list crit sec */
            {
            FOR_ENTRIES_OF(log->tid_list,int_tid_t,tid)
                {
                if (rvm_region == NULL)
                    copy_tid = rvm_true;
                else
                    {
                    /* see if tid modifies specified region */
                    copy_tid = rvm_false;
                    RW_CRITICAL(tid->tid_lock,r, /* begin tid lock crit sec */
                        {
                        FOR_NODES_OF(tid->range_tree,range_t,range)
                            if (range->region == region)
                                {
                                copy_tid = rvm_true;
                                break;
                                }
                        });             /* end tid lock crit sec */
                    }
                /* copy uncommitted tid descriptions to uncommitted tid array */
                if (copy_tid)
                    {
                    rvm_options->n_uncommit++;
                    if (n_tids < rvm_options->n_uncommit)
                        {
                        /* reallocate tid_array */
                        n_tids += TID_ARRAY_REALLOC_INCR;
                        rvm_options->tid_array = (rvm_tid_t *)
                            REALLOC(rvm_options->tid_array,
                                    n_tids*sizeof(rvm_tid_t));
                        if (rvm_options->tid_array == NULL)
                            {
                            retval = RVM_ENO_MEMORY;
                            goto err_exit;
                            }
                        }

                    /* copy tid uname */
                    rvm_init_tid(&rvm_options->tid_array[
                                   rvm_options->n_uncommit-1]);
                    rvm_options->tid_array[rvm_options->
                                           n_uncommit-1].uname
                                               = tid->uname;
                    rvm_options->tid_array[rvm_options->
                                           n_uncommit-1].tid
                                               = tid;
                    }
                }
err_exit:;
            });                         /* end log tid list crit sec */

        if (rvm_region != NULL)
            rw_unlock(&region->region_lock,r); /* end region_lock crit sect */
        }

    /* return non-log options */
    rvm_options->flags = rvm_optimizations;
    rvm_options->max_read_len = rvm_max_read_len;

    return retval;
    }
/* rvm_statistics */
rvm_return_t rvm_statistics(version,rvm_statistics)
    char                *version;       /* ptr to statistics version */
    rvm_statistics_t    *rvm_statistics; /* ptr to stats record */
    {
    log_t               *log;           /* log descriptor */
    log_status_t        *status;        /* log status area descriptor */
    int_tid_t           *tid;           /* transaction ptr */
    rvm_length_t        i;
    rvm_return_t        retval;

    /* be sure RVM is initialized */
    if (bad_init()) return RVM_EINIT;

    /* check validity of statistics version and record */
    if (strcmp(version, RVM_STATISTICS_VERSION))
        return RVM_ESTAT_VERSION_SKEW;
    if (rvm_statistics == NULL)
        return RVM_ESTATISTICS;
    if ((retval=bad_statistics(rvm_statistics)) != RVM_SUCCESS)
        return retval;

    /* check log */
    if (default_log == NULL)
        return RVM_ELOG;
    log = default_log;
    status = &log->status;
    /* copy log and transaction statistics from log status area */
    rvm_statistics->log_dev_cur = cur_log_percent(log,NULL);
    CRITICAL(log->dev_lock,             /* begin dev_lock crit sec */
        {
        rvm_statistics->n_abort = status->n_abort;
        rvm_statistics->n_flush_commit = status->n_flush_commit;
        rvm_statistics->n_no_flush_commit = 
            status->n_no_flush_commit;
        rvm_statistics->n_split = status->n_split;
        rvm_statistics->n_flush = status->n_flush;
        rvm_statistics->n_rvm_flush = status->n_rvm_flush;
        rvm_statistics->n_special = status->n_special;
        rvm_statistics->n_wrap = 0;
        if (RVM_OFFSET_GTR(status->log_head,status->log_tail))
            rvm_statistics->n_wrap = 1;
        rvm_statistics->tot_abort = status->tot_abort;
        rvm_statistics->tot_flush_commit = status->tot_flush_commit;
        rvm_statistics->tot_no_flush_commit =
            status->tot_no_flush_commit;
        rvm_statistics->tot_split = status->tot_split;
        rvm_statistics->tot_rvm_truncate =
            status->tot_rvm_truncate;
        rvm_statistics->tot_async_truncation =
            status->tot_async_truncation;
        rvm_statistics->tot_sync_truncation =
            status->tot_sync_truncation;
        rvm_statistics->tot_truncation_wait = 
            status->tot_truncation_wait;
        rvm_statistics->tot_recovery = status->tot_recovery;
        rvm_statistics->tot_flush = status->tot_flush;
        rvm_statistics->tot_rvm_flush = status->tot_rvm_flush;
        rvm_statistics->tot_special = status->tot_special;
        rvm_statistics->tot_wrap = status->tot_wrap;
        rvm_statistics->log_dev_max = status->log_dev_max;
        cur_log_length(log,&rvm_statistics->log_written);
        rvm_statistics->tot_log_written = status->tot_log_written;
        rvm_statistics->range_overlap = status->range_overlap;
        rvm_statistics->tot_range_overlap = status->tot_range_overlap;
        rvm_statistics->trans_overlap = status->trans_overlap;
        rvm_statistics->tot_trans_overlap = status->tot_trans_overlap;
        rvm_statistics->n_range_elim = status->n_range_elim;
        rvm_statistics->n_trans_elim = status->n_trans_elim;
        rvm_statistics->n_trans_coalesced = status->n_trans_coalesced;
        rvm_statistics->tot_range_elim = status->tot_range_elim;
        rvm_statistics->tot_trans_elim = status->tot_trans_elim;
        rvm_statistics->tot_trans_coalesced = status->tot_trans_coalesced;
        rvm_statistics->last_flush_time = status->last_flush_time;
        rvm_statistics->last_truncation_time = status->last_truncation_time;
        rvm_statistics->last_tree_build_time = status->last_tree_build_time;
        rvm_statistics->last_tree_apply_time = status->last_tree_apply_time;
        /* copy histograms and timings */
        for (i=0; i < flush_times_len; i++)
            {
            rvm_statistics->flush_times[i] = status->flush_times[i];
            rvm_statistics->tot_flush_times[i] =
                status->tot_flush_times[i];
            }
        rvm_statistics->flush_time = status->flush_time;
        rvm_statistics->tot_flush_time = status->tot_flush_time;
        rvm_statistics->tot_truncation_time = status->tot_truncation_time;
        for (i=0; i < range_lengths_len; i++)
            {
            rvm_statistics->range_lengths[i] =
                status->range_lengths[i];
            rvm_statistics->tot_range_lengths[i] =
                status->tot_range_lengths[i];
            rvm_statistics->range_overlaps[i] =
                status->range_overlaps[i];
            rvm_statistics->tot_range_overlaps[i] =
                status->tot_range_overlaps[i];
            rvm_statistics->trans_overlaps[i] =
                status->trans_overlaps[i];
            rvm_statistics->tot_trans_overlaps[i] =
                status->tot_trans_overlaps[i];
            }
        for (i=0; i < range_elims_len; i++)
            {
            rvm_statistics->range_elims[i] = status->range_elims[i];
            rvm_statistics->tot_range_elims[i] =
                status->tot_range_elims[i];
            rvm_statistics->trans_elims[i] = status->trans_elims[i];
            rvm_statistics->tot_trans_elims[i] =
                status->tot_trans_elims[i];
            rvm_statistics->tot_trans_coalesces[i] =
                status->tot_trans_coalesces[i];
            }
        for (i=0; i < truncation_times_len; i++)
            {
            rvm_statistics->tot_tree_build_times[i] =
                status->tot_tree_build_times[i];
            rvm_statistics->tot_tree_apply_times[i] =
                status->tot_tree_apply_times[i];
            rvm_statistics->tot_truncation_times[i] =
                status->tot_truncation_times[i];
            }
        });                             /* end dev_lock crit sec */
    /* get non-status area statistics */
    CRITICAL(log->tid_list_lock,
        rvm_statistics->n_uncommit = log->tid_list.list.length);
    CRITICAL(log->flush_list_lock,
        {
        rvm_statistics->n_no_flush = 0;
        RVM_ZERO_OFFSET(rvm_statistics->no_flush_length);
        FOR_ENTRIES_OF(log->flush_list,int_tid_t,tid)
            {
            if (!TID(FLUSH_FLAG))
                {
                rvm_statistics->n_no_flush++;
                rvm_statistics->no_flush_length = RVM_ADD_OFFSETS(
                       rvm_statistics->no_flush_length,tid->log_size);
                }
            }
        });

    return RVM_SUCCESS;
    }
