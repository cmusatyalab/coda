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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/rvm-src/rvm/rvm_logstatus.c,v 4.5 1998/04/14 20:45:24 braam Exp $";
#endif _BLURB_

/*
*
*                            RVM log status area support
*
*/
#include <unistd.h>
#include <sys/file.h>
#include <sys/errno.h>
#include "rvm_private.h"

#ifdef RVM_LOG_TAIL_BUG
#include <rvmtesting.h>
extern unsigned long *ClobberAddress;
#endif RVM_LOG_TAIL_BUG

/* global variables */

extern int          errno;              /* kernel error number */
rvm_bool_t          rvm_utlsw;          /* true iff RVM called by rvmutl,
                                           permits certain structures to be
                                           retained after errors are discovered
                                           */
extern rvm_bool_t   rvm_no_update;      /* no segment or log update if true */
extern char         *rvm_errmsg;        /* internal error message buffer */

extern rvm_length_t page_size;          /* system page size */
extern rvm_length_t page_mask;          /* mask for rounding down to page size */
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
extern rvm_length_t trans_coalesces_vec[trans_coalesces_len]; /* transactions
                                                                 coalesed per flush */

/* root of global log device list */
log_t               *default_log;       /* default log descriptor ptr */

#ifdef RVM_LOG_TAIL_SHADOW
rvm_offset_t        log_tail_shadow;        /* shadow log tail pointer */
rvm_offset_t        last_log_tail;          /* last committed log tail value */
rvm_bool_t          last_log_valid = rvm_false; /* validity of last_log_tail */
rvm_bool_t          has_wrapped = rvm_false;    /* whether or not we wrapped */
char *log_tail_shadow_in_object = "Compiled with a shadow log tail offset\n";
#endif RVM_LOG_TAIL_SHADOW

/* locals */

static RVM_MUTEX    log_root_lock;      /* for list header, links & default */
list_entry_t        log_root;           /* header for log descriptor list */

static rvm_offset_t file_status_offset = /* log status area offset in files */
    RVM_OFFSET_INITIALIZER(0,FILE_STATUS_OFFSET);

static rvm_offset_t raw_status_offset = /* log status area offset in partitions */
    RVM_OFFSET_INITIALIZER(0,RAW_STATUS_OFFSET);

static rvm_offset_t min_trans_size =    /* minimum usable log size as offset */
    RVM_OFFSET_INITIALIZER(0,MIN_TRANS_SIZE);
/* log_root initialization */
void init_log_list()
    {
    init_list_header(&log_root,log_id);
    mutex_init(&log_root_lock);
    default_log = (log_t *)NULL;
    }

/* enter new log in log list and establish default log if necessary */
/* 
  if we are looking for the RVM_LOG_TAIL_BUG, there can only ever
  be one log.  I *believe* that it is possibly to only have one log
  open at a time.  But, I'm not going to coda_assert that in the general 
  case -bnoble 7/30/94
*/

void enter_log(log)
    log_t           *log;               /* log descriptor */
    {

    CODA_ASSERT(log != NULL);
#ifdef RVM_LOG_TAIL_BUG
    CODA_ASSERT(default_log == NULL);
#endif RVM_LOG_TAIL_BUG
    CRITICAL(log_root_lock,
        {
        (void)move_list_entry(NULL,(list_entry_t *)&log_root,
                              log);
        if (default_log == NULL)
            default_log = log;
        });

#ifdef RVM_LOG_TAIL_BUG
    /* 
      this is massively unportable: for the moment, coda_assert we are
      on pmax_mach. 
    */
#ifndef	__MACH__
    CODA_ASSERT(0);
#endif	/* __MACH__ */
#ifndef mips
    CODA_ASSERT(0);
#endif mips
    ClobberAddress = &(default_log->status.log_tail.low);
    protect_page__Fi(ClobberAddress);
#endif RVM_LOG_TAIL_BUG
#ifdef RVM_LOG_TAIL_SHADOW
    RVM_ASSIGN_OFFSET(log_tail_shadow,default_log->status.log_tail);
    RVM_ASSIGN_OFFSET(last_log_tail,log_tail_shadow);
    last_log_valid = rvm_true;
#endif RVM_LOG_TAIL_SHADOW

    }

/* find an existing log -- returns descriptor ptr or NULL */
log_t *find_log(log_dev)
    char            *log_dev;
    {
    log_t           *log;
#ifdef DJGPP
    rvm_return_t    retval;
    char            *log_dev_fullname = make_full_name(log_dev, 0, &retval);
    CODA_ASSERT(log_dev_fullname && retval == RVM_SUCCESS);
#else
    char            *log_dev_fullname = log_dev;
#endif

    CODA_ASSERT(log_dev != NULL);
    CRITICAL(log_root_lock,
        {
        FOR_ENTRIES_OF(log_root,log_t,log)
            if (strcmp(log->dev.name,log_dev_fullname) == 0)
                goto found;

        log = NULL;
found:;
        });

#ifdef DJGPP
    free(log_dev_fullname);
#endif

    return log;
    }
/* log daemon control */

/* create daemon */
static rvm_return_t fork_daemon(log) 
    log_t           *log;
   {
    log_daemon_t    *daemon = &log->daemon; /* truncation daemon descriptor */

    /* create daemon thread */
    if (daemon->thread == (cthread_t)NULL)
        {
        daemon->state = rvm_idle;
        mutex_init(&daemon->lock);
        daemon->thread = cthread_fork((void *)log_daemon,log);
        if (daemon->thread == (cthread_t)NULL)
            return RVM_ELOG;
        }

    return RVM_SUCCESS;
    }

/* terminate daemon */
static rvm_return_t join_daemon(log)
    log_t           *log;
    {
    log_daemon_t    *daemon = &log->daemon; /* truncation daemon descriptor */
    daemon_state_t  state;              /* daemon control state code */
    rvm_return_t    retval = RVM_SUCCESS;

    if (daemon->thread != (cthread_t)NULL)
        {
        /* terminate the daemon */
        CRITICAL(daemon->lock,          /* begin daemon lock crit sec */
            {
            if (daemon->state != error)
                daemon->state = terminate;
            state = daemon->state;
            });                         /* end daemon lock crit sec */
        if (state != error)
            condition_signal(&daemon->code);

        /* wait for daemon thread to terminate */
        retval = (rvm_return_t)cthread_join(daemon->thread);
        daemon->thread = (cthread_t)NULL;
        }
    daemon->truncate = 0;

    return retval;
    }
/* set log truncation options */
static rvm_return_t set_truncate_options(log,rvm_options)
    log_t           *log;               /* log descriptor ptr */
    rvm_options_t   *rvm_options;       /* optional options descriptor */
    {
    log_daemon_t    *daemon = &log->daemon; /* truncation daemon descriptor */
    rvm_return_t    retval = RVM_SUCCESS;

    if (rvm_utlsw)                      /* no log options allowed */
        return RVM_SUCCESS;

    /* set truncation threshold if parameter within range and
       thread package installed */
    if ((rvm_options->truncate > 0) && (rvm_options->truncate <= 100)
        && (cthread_self() != (cthread_t)NULL))
        {
        /* update daemon thread */
        retval = fork_daemon(log);      /* create daemon if necessary */
        daemon->truncate = rvm_options->truncate;
        }
    else
        retval = join_daemon(log);      /* terminate daemon */

    return retval;
    }
/* close log device */
rvm_return_t close_log(log)
    log_t           *log;
    {
    log_special_t   *special;
    rvm_return_t    retval = RVM_SUCCESS;

    /* make sure all transactions ended */
    CRITICAL(log->tid_list_lock,        /* begin tid_list_lock crit sec */
        {
        if (LIST_NOT_EMPTY(log->tid_list))
            retval = RVM_EUNCOMMIT;
        });                             /* end tid_list_lock crit sec */
    if (retval != RVM_SUCCESS) return retval;

    /* issue terminate to daemon */
    (void)join_daemon(log);             /* can we do something on error? */

    /* flush log and close */
    CRITICAL(log->truncation_lock,
        {
        if ((retval=flush_log(log,&log->status.n_flush))
            == RVM_SUCCESS)
            CRITICAL(log->dev_lock,
                {
                if ((retval=write_log_status(log,NULL))
                    == RVM_SUCCESS)
                    if (close_dev(&log->dev) < 0)
                        retval = RVM_EIO;
                });
        });
    if (retval != RVM_SUCCESS) return retval;
    if (default_log == log) {
#ifdef RVM_LOG_TAIL_BUG
	unprotect_page__Fi(ClobberAddress);
	ClobberAddress = 0;
#endif RVM_LOG_TAIL_BUG
#ifdef RVM_LOG_TAIL_SHADOW
	RVM_ZERO_OFFSET(log_tail_shadow);
	RVM_ZERO_OFFSET(last_log_tail);
	last_log_valid = rvm_false;
#endif RVM_LOG_TAIL_SHADOW
	default_log = NULL;
    }
    /* kill unflushed log_special records */
    UNLINK_ENTRIES_OF(log->special_list,log_special_t,special)
        free_log_special(special);

    /* free descriptor */
    free_log(log);

    return retval;
    }
/* termination close of all log devices */
rvm_return_t close_all_logs()
    {
    log_t           *log;               /* log device descriptor ptr */
    rvm_return_t    retval = RVM_SUCCESS;

    /* cycle through log list */
    CRITICAL(log_root_lock,             /* begin log_root_lock crit sec */
        {
        UNLINK_ENTRIES_OF(log_root,log_t,log)
            {
            if ((retval=close_log(log)) != RVM_SUCCESS)
                break;
            }
        });                             /* end log_root_lock crit sec */

    return retval;
    }
/* pre-load log raw i/o gather write buffer with tail log sector */
static rvm_return_t preload_wrt_buf(log)
    log_t           *log;               /* log descriptor */
    {
    device_t        *dev = &log->dev;   /* device descriptor ptr */
    log_status_t    *status = &log->status; /* log status descriptor */
    rvm_offset_t    tail_sector;        /* log tail sector */

    tail_sector = CHOP_OFFSET_TO_SECTOR_SIZE(status->log_tail);
    if (read_dev(dev,&tail_sector,dev->wrt_buf,SECTOR_SIZE) < 0)
        return RVM_EIO;

    dev->ptr = RVM_ADD_LENGTH_TO_ADDR(dev->wrt_buf,
                   OFFSET_TO_SECTOR_INDEX(status->log_tail));
    dev->buf_start = dev->ptr;
    dev->sync_offset = status->log_tail;

    return RVM_SUCCESS;
    }
/* create log descriptor and open log device */
rvm_return_t open_log(dev_name,log_ptr,status_buf,rvm_options)
    char            *dev_name;          /* name of log storage device */
    log_t           **log_ptr;          /* addr of log descriptor ptr */
    char            *status_buf;        /* optional i/o buffer */
    rvm_options_t   *rvm_options;       /* optional options descriptor */
    {
    log_t           *log;               /* log descriptor ptr */
    log_buf_t       *log_buf;           /* log buffer descriptor ptr */
    device_t        *dev;               /* device descriptor ptr */
    log_status_t    *status;            /* log status descriptor */
    rvm_length_t    flags = O_RDWR;     /* device open flags */
    rvm_options_t   local_options;      /* local options record */
    rvm_return_t    retval;

    /* build internal log structure */
    if ((log = make_log(dev_name,&retval)) == NULL)
        goto err_exit2;
    dev = &log->dev;
    status = &log->status;
    log_buf = &log->log_buf;

    /* allocate recovery buffers */
    if (rvm_options == NULL)
        {
        rvm_options = &local_options;
        rvm_init_options(rvm_options);
        }
    if ((long)(rvm_options->recovery_buf_len) < MIN_RECOVERY_BUF_LEN)
        rvm_options->recovery_buf_len = MIN_RECOVERY_BUF_LEN;
    log_buf->length=ROUND_TO_PAGE_SIZE(rvm_options->recovery_buf_len);
    log_buf->aux_length = ROUND_TO_PAGE_SIZE(log_buf->length/2);
    if ((retval=alloc_log_buf(log)) != RVM_SUCCESS)
        return retval;

    /* open the device and determine characteristics */
    if (rvm_no_update) flags = O_RDONLY;
    if (open_dev(dev,flags,0) != 0)
        {
        retval = RVM_EIO;
        goto err_exit2;
        }
    if (set_dev_char(dev,NULL) < 0)
        {
        retval = RVM_EIO;
        goto err_exit;
        }
    if (dev->raw_io) dev->num_bytes =   /* enought to read status area */
        RVM_ADD_LENGTH_TO_OFFSET(raw_status_offset,
                                 LOG_DEV_STATUS_SIZE);
    /* open status area */
    if ((retval=read_log_status(log,status_buf)) != RVM_SUCCESS)
        {
        if (rvm_utlsw) goto keep_log; /* keep damaged status */
        goto err_exit;
        }
    log->status.trunc_state = 0;
    log->status.flush_state = 0;
        
    /* create daemon truncation thread */
    if ((retval=set_truncate_options(log,rvm_options))
        != RVM_SUCCESS) goto err_exit;
    /* raw i/o support */
    if (dev->raw_io)
        {
        /* assign gather write buffer */
        if ((long)(rvm_options->flush_buf_len) < MIN_FLUSH_BUF_LEN)
            rvm_options->flush_buf_len = MIN_FLUSH_BUF_LEN;
        dev->wrt_buf_len =
            ROUND_TO_PAGE_SIZE(rvm_options->flush_buf_len);
        dev->wrt_buf = page_alloc(dev->wrt_buf_len);
        if (dev->wrt_buf == NULL)
            {
            retval = RVM_ENO_MEMORY;
            goto err_exit;
            }
        dev->buf_end = RVM_ADD_LENGTH_TO_ADDR(dev->wrt_buf,
                                              dev->wrt_buf_len);
        
        /* pre-load write buffer */
        if ((retval=preload_wrt_buf(log)) != RVM_SUCCESS)
            goto err_exit;
        }

    /* enter in log list*/
keep_log:
    enter_log(log);
    *log_ptr = log;
    return retval;

err_exit:
    (void)close_dev(dev);
err_exit2:
    free_log(log);
    *log_ptr = (log_t *)NULL;
    return retval;
    }
/* log options processing */
rvm_return_t do_log_options(log_ptr,rvm_options)
    log_t           **log_ptr;          /* addr of log descriptor ptr */
    rvm_options_t   *rvm_options;       /* ptr to rvm options descriptor */
    {
    rvm_return_t    retval;
    log_t           *log = NULL;
    char            *log_dev;

    if ((rvm_options == NULL) || (rvm_options->log_dev == NULL))
        return RVM_SUCCESS;

    /* see if need to build a log descriptor */
    log_dev = rvm_options->log_dev;
    if ((log=find_log(log_dev)) == NULL)
        {
        /* see if already have a log */
        if (default_log != NULL)
            return RVM_ELOG;
        
        /* build log descriptor */
        if ((retval=open_log(log_dev,&log,NULL,rvm_options))
            != RVM_SUCCESS) {
		printf("open_log failed.\n");
		return retval;
	}
        /* do recovery processing for log */
        log->in_recovery = rvm_true;
        if ((retval = log_recover(log,&log->status.tot_recovery,
                                  rvm_false,RVM_RECOVERY)) != RVM_SUCCESS) {
		printf("log_recover failed.\n");
		return retval;
	}

        /* pre-load write buffer with new tail sector */
        if (log->dev.raw_io)
            {
            CRITICAL(log->dev_lock,retval=preload_wrt_buf(log));
            if (retval != RVM_SUCCESS) {
		    return retval;
		    printf("preload_wrt_buff failed\n");
	    }
            }
        }

    /* process options and return log descriptor if wanted */
    retval = set_truncate_options(log,rvm_options);
    if (log_ptr != NULL)
        *log_ptr = log;

    return retval;
    }
/* accumulate running statistics totals */
void copy_log_stats(log)
    log_t           *log;
    {
    log_status_t    *status = &log->status; /* status area descriptor */
    rvm_length_t    i;
    rvm_offset_t    temp;

    CODA_ASSERT(((&log->dev == &default_log->dev) && (!rvm_utlsw)) ?
           (!LOCK_FREE(default_log->dev_lock)) : 1);

    /* sum epoch counts */
    status->tot_abort += status->n_abort;
    status->n_abort = 0;
    status->tot_flush_commit += status->n_flush_commit;
    status->n_flush_commit = 0;
    status->tot_no_flush_commit += status->n_no_flush_commit;
    status->n_no_flush_commit = 0;
    status->tot_split += status->n_split;
    status->n_split = 0;
    status->tot_flush += status->n_flush;
    status->n_flush = 0;
    status->tot_rvm_flush += status->n_rvm_flush;
    status->n_rvm_flush = 0;
    status->tot_special += status->n_special;
    status->n_special = 0;
    status->tot_truncation_wait += status->n_truncation_wait;
    status->n_truncation_wait = 0;
    status->tot_range_elim += status->n_range_elim;
    status->n_range_elim = 0;
    status->tot_trans_elim += status->n_trans_elim;
    status->n_trans_elim = 0;
    status->tot_trans_coalesced += status->n_trans_coalesced;
    status->n_trans_coalesced = 0;
    status->tot_range_overlap =
        RVM_ADD_OFFSETS(status->tot_range_overlap,
                        status->range_overlap);
    RVM_ZERO_OFFSET(status->range_overlap);
    status->tot_trans_overlap =
        RVM_ADD_OFFSETS(status->tot_trans_overlap,
                        status->trans_overlap);
    RVM_ZERO_OFFSET(status->trans_overlap);

    /* sum length of log writes */
    log_tail_length(log,&temp);
    status->tot_log_written = RVM_ADD_OFFSETS(status->tot_log_written,
                                              status->log_size);
    status->tot_log_written = RVM_SUB_OFFSETS(status->tot_log_written,
                                              temp);
    /* sum cumulative histograms and zero current */
    for (i=0; i < flush_times_len; i++)
        {
        status->tot_flush_times[i] += status->flush_times[i];
        status->flush_times[i] = 0;
        }
    status->tot_flush_time = add_times(&status->tot_flush_time,
                                       &status->flush_time);
    for (i=0; i < range_lengths_len; i++)
        {
        status->tot_range_lengths[i] += status->range_lengths[i];
        status->range_lengths[i] = 0;
        status->tot_range_overlaps[i] += status->range_overlaps[i];
        status->range_overlaps[i] = 0;
        status->tot_trans_overlaps[i] += status->trans_overlaps[i];
        status->trans_overlaps[i] = 0;
        }

    for (i=0; i < range_elims_len; i++)
        {
        status->tot_range_elims[i] += status->range_elims[i];
        status->range_elims[i] = 0;
        status->tot_trans_elims[i] += status->trans_elims[i];
        status->trans_elims[i] = 0;
        }
    ZERO_TIME(status->flush_time);
    }
/* clear non-permenant log status area fields */
void clear_log_status(log)
    log_t           *log;
    {
    log_status_t    *status = &log->status; /* status area descriptor */
 
    CODA_ASSERT(((&log->dev == &default_log->dev) && (!rvm_utlsw)) ?
           (!LOCK_FREE(default_log->dev_lock)) : 1);

    status->valid = rvm_true;
    status->log_empty = rvm_true;
    status->first_rec_num = 0;
    status->last_rec_num = 0;
    ZERO_TIME(status->first_uname);
    ZERO_TIME(status->last_uname);
    ZERO_TIME(status->last_commit);
    ZERO_TIME(status->first_write);
    ZERO_TIME(status->last_write);
    ZERO_TIME(status->wrap_time);
    ZERO_TIME(status->flush_time);
    RVM_ZERO_OFFSET(status->prev_log_head);
    RVM_ZERO_OFFSET(status->prev_log_tail);

    copy_log_stats(log);
    }
/* log status block initialization */
rvm_return_t init_log_status(log)
    log_t           *log;               /* log descriptor */
    {
    rvm_length_t    i;
    log_status_t    *status = &log->status; /* status area descriptor */
    rvm_offset_t    *status_offset;     /* offset of status area */

    /* initialize boundaries & size */
    if (log->dev.raw_io) status_offset = &raw_status_offset;
    else status_offset = &file_status_offset;
    status->log_start = RVM_ADD_LENGTH_TO_OFFSET(*status_offset,
                                                 LOG_DEV_STATUS_SIZE);
    status->log_size = RVM_SUB_OFFSETS(log->dev.num_bytes,
                                       status->log_start);

    /* initialize head and tail pointers */
    status->log_head = status->log_start;
#ifdef RVM_LOG_TAIL_BUG
    unprotect_page__Fi(ClobberAddress);
#endif RVM_LOG_TAIL_BUG
#ifdef RVM_LOG_TAIL_SHADOW
    CODA_ASSERT(RVM_OFFSET_EQL(log_tail_shadow,status->log_tail));
#endif RVM_LOG_TAIL_SHADOW
    status->log_tail = status->log_start;
#ifdef RVM_LOG_TAIL_SHADOW
	RVM_ASSIGN_OFFSET(log_tail_shadow,status->log_tail);
#endif RVM_LOG_TAIL_SHADOW
#ifdef RVM_LOG_TAIL_BUG
    protect_page__Fi(ClobberAddress);
#endif RVM_LOG_TAIL_BUG
    RVM_ZERO_OFFSET(status->prev_log_head);
    RVM_ZERO_OFFSET(status->prev_log_tail);

    /* init status variables */
    clear_log_status(log);
    make_uname(&status->status_init);   /* initialization timestamp */
    status->last_trunc = status->status_init;
    status->prev_trunc = status->status_init;
    status->next_rec_num = 1;
    status->log_dev_max = 0;
    status->last_flush_time = 0;
    status->last_truncation_time = 0;
    status->last_tree_build_time = 0;
    status->last_tree_apply_time = 0;

    /* clear cumulative statistics */
    status->tot_rvm_truncate = 0;
    status->tot_async_truncation = 0;
    status->tot_sync_truncation = 0;
    status->tot_truncation_wait = 0;
    status->tot_recovery = 0;
    status->tot_abort = 0;
    status->tot_flush_commit = 0;
    status->tot_no_flush_commit = 0;
    status->tot_split = 0;
    status->tot_rvm_flush = 0;
    status->tot_flush = 0;
    status->tot_special = 0;
    status->tot_wrap = 0;
    status->tot_range_elim = 0;
    status->tot_trans_elim = 0;
    status->tot_trans_coalesced = 0;
    RVM_ZERO_OFFSET(status->tot_range_overlap);
    RVM_ZERO_OFFSET(status->tot_trans_overlap);
    RVM_ZERO_OFFSET(status->tot_log_written);
    /* clear timings and histograms */
    ZERO_TIME(status->tot_flush_time);
    ZERO_TIME(status->tot_truncation_time);
    for (i=0; i < flush_times_len; i++)
        status->tot_flush_times[i] = 0;
    for (i=0; i < truncation_times_len; i++)
        {
        status->tot_tree_build_times[i] = 0;
        status->tot_tree_apply_times[i] = 0;
        status->tot_truncation_times[i] = 0;
        }
    for (i=0; i < range_lengths_len; i++)
        {
        status->tot_range_lengths[i] = 0;
        status->tot_range_overlaps[i] = 0;
        status->tot_trans_overlaps[i] = 0;
        }
    for (i=0; i < range_elims_len; i++)
        {
        status->tot_range_elims[i] = 0;
        status->tot_trans_elims[i] = 0;
        status->tot_trans_coalesces[i] = 0;
        }

    /* write the device areas */
    return write_log_status(log,NULL);
    }
/* read log status area from log device */
rvm_return_t read_log_status(log,status_buf)
    log_t               *log;           /* log descriptor */
    char                *status_buf;    /* optional i/o buffer */
    {
    log_status_t        *status = &log->status; /* status area descriptor */
    rvm_offset_t        *status_offset; /* device status area offset */
    log_dev_status_t    *dev_status;    /* status i/o area typed ptr */
    char                status_io[LOG_DEV_STATUS_SIZE]; /* i/o buffer */
    rvm_length_t        saved_chk_sum;  /* save area for checksum read */

    /* read the status areas */
    if (status_buf != NULL)
        dev_status = (log_dev_status_t *)status_buf;
    else
        dev_status = (log_dev_status_t *)status_io;
    if (log->dev.raw_io) status_offset = &raw_status_offset;
    else status_offset = &file_status_offset;
    if (read_dev(&log->dev,status_offset,
                  dev_status,LOG_DEV_STATUS_SIZE) < 0)
        return RVM_EIO;

    /* save old checksum and compute new */
    saved_chk_sum = dev_status->chk_sum;
    dev_status->chk_sum = 0;
    dev_status->chk_sum = chk_sum((char *)dev_status,
                                  LOG_DEV_STATUS_SIZE);

    /* copy to log descriptor */
    (void)BCOPY(&dev_status->status,(char *)status,
                sizeof(log_status_t));  
    status->valid = rvm_false;          /* status not valid until tail found */

    /* compare checksum, struct_id, and version */
    if ((dev_status->chk_sum != saved_chk_sum)
        || (dev_status->struct_id != log_dev_status_id))
        return RVM_ELOG;                /* status area damaged */
    if (strcmp(dev_status->version,RVM_VERSION) != 0)
        return RVM_ELOG_VERSION_SKEW;
    if (strcmp(dev_status->log_version,RVM_LOG_VERSION) != 0)
        return RVM_ELOG_VERSION_SKEW;
    if (strcmp(dev_status->statistics_version,RVM_STATISTICS_VERSION) != 0)
        return RVM_ESTAT_VERSION_SKEW;

    /* set log device length to log size at creation */
    if (log->dev.raw_io)
        log->dev.num_bytes = RVM_ADD_OFFSETS(status->log_size,
                                             status->log_start);
    status->update_cnt = UPDATE_STATUS;
    return RVM_SUCCESS;
    }
/* write log status area on log device */
rvm_return_t write_log_status(log,dev)
    log_t               *log;
    device_t            *dev;           /* optional device */
    {
    log_status_t        *status = &log->status; /* status area descriptor */
    rvm_offset_t        *status_offset; /* device status area offset */
    log_dev_status_t    *dev_status;    /* status i/o area typed ptr */
    char                status_io[LOG_DEV_STATUS_SIZE]; /* i/o buffer */

    /* initializations */
#ifdef RVM_LOG_TAIL_SHADOW
    CODA_ASSERT(RVM_OFFSET_EQL(log_tail_shadow,log->status.log_tail));
    /* we'll check to see whether this log offest is before the
       previous one.  If so, assert.  Some false assertions, but hey. */
    if (last_log_valid == rvm_true) {
	if (has_wrapped == rvm_true) {
	    /* this log value should be LESS than the previous one */
	    CODA_ASSERT(RVM_OFFSET_GEQ(last_log_tail,log->status.log_tail));
	    /* We've accounted for the log_wrap; reset it. */
	    has_wrapped = rvm_false;
	} else {
	    /* this log value should be GREATER than the previous one */
	    CODA_ASSERT(RVM_OFFSET_LEQ(last_log_tail,log->status.log_tail));
	}
    } else {
	last_log_valid = rvm_true;
    }
    RVM_ASSIGN_OFFSET(last_log_tail,log->status.log_tail);
#endif RVM_LOG_TAIL_SHADOW
    if (dev == NULL) dev = &log->dev;
    (void) BZERO(status_io,LOG_DEV_STATUS_SIZE); /* clear buffer */

    /* set up device status i/o area */
    status->update_cnt = UPDATE_STATUS;
    make_uname(&status->status_write);
    dev_status = (log_dev_status_t *)status_io;
    dev_status->struct_id = log_dev_status_id;
    (void)BCOPY((char *)status,&dev_status->status,
                sizeof(log_status_t));
    (void)strcpy(dev_status->version,RVM_VERSION);
    (void)strcpy(dev_status->log_version,RVM_LOG_VERSION);
    (void)strcpy(dev_status->statistics_version,
                 RVM_STATISTICS_VERSION);

    /* compute checksum */
    dev_status->chk_sum = 0;
    dev_status->chk_sum = chk_sum((char *)dev_status,
                                  LOG_DEV_STATUS_SIZE);

    /* write the status areas */
    if (dev->raw_io) status_offset = &raw_status_offset;
    else status_offset = &file_status_offset;
    if (write_dev(dev,status_offset,dev_status,
                  LOG_DEV_STATUS_SIZE,SYNCH) < 0)
        return RVM_EIO;

    return RVM_SUCCESS;
    }
/* consistency check for log head/tail ptrs */
static rvm_bool_t chk_tail(log)
    log_t           *log;
    {
    log_status_t    *status = &log->status; /* status area descriptor */

    /* basic range checks -- current epoch */
    CODA_ASSERT(RVM_OFFSET_GEQ(status->log_tail,status->log_start));
    CODA_ASSERT(RVM_OFFSET_LEQ(status->log_tail,log->dev.num_bytes));
    CODA_ASSERT(RVM_OFFSET_GEQ(status->log_head,status->log_start));
    CODA_ASSERT(RVM_OFFSET_LEQ(status->log_head,log->dev.num_bytes));

    /* basic range checks -- previous epoch */
    if (!RVM_OFFSET_EQL_ZERO(status->prev_log_head))
        {
        CODA_ASSERT(RVM_OFFSET_EQL(status->log_head,
                              status->prev_log_tail));
        CODA_ASSERT(RVM_OFFSET_GEQ(status->prev_log_tail,
                              status->log_start));
        CODA_ASSERT(RVM_OFFSET_LEQ(status->prev_log_tail,
                              log->dev.num_bytes));
        CODA_ASSERT(RVM_OFFSET_GEQ(status->prev_log_head,
                              status->log_start));
        CODA_ASSERT(RVM_OFFSET_LEQ(status->prev_log_head,
                              log->dev.num_bytes));
        CODA_ASSERT(RVM_OFFSET_EQL(status->prev_log_tail,
                              status->log_head));
        }
    /* current <==> previous epoch consistency checks */
    if (RVM_OFFSET_GTR(status->log_head,status->log_tail))
        {                               /* current epoch wrapped */
        CODA_ASSERT(RVM_OFFSET_GEQ(status->log_head,status->log_tail));
        if (!RVM_OFFSET_EQL_ZERO(status->prev_log_head))
            {                           /* check previous epoch */
            CODA_ASSERT(RVM_OFFSET_LEQ(status->prev_log_head,
                                  status->prev_log_tail));
            CODA_ASSERT(RVM_OFFSET_GEQ(status->prev_log_head,
                                  status->log_tail));
            CODA_ASSERT(RVM_OFFSET_GEQ(status->prev_log_head,
                                  status->log_tail));
            }
        }
    else
        {                               /* current epoch not wrapped */
        if (!RVM_OFFSET_EQL_ZERO(status->prev_log_head))
            {                           /* check previous epoch */
            if (RVM_OFFSET_GTR(status->prev_log_head,
                               status->prev_log_tail))
                {                       /* previous epoch wrapped */
                CODA_ASSERT(RVM_OFFSET_GTR(status->prev_log_head,
                                      status->log_tail));
                CODA_ASSERT(RVM_OFFSET_GEQ(status->prev_log_head,
                                      status->log_tail));
                }
            else
                {                       /* previous epoch not wrapped */
                CODA_ASSERT(RVM_OFFSET_GTR(status->log_head,
                                      status->prev_log_head));
                }
            }
        }

    /* raw i/o buffer checks */
    if (log->dev.raw_io)
        {
        CODA_ASSERT((SECTOR_INDEX((long)log->dev.ptr)) ==
               (OFFSET_TO_SECTOR_INDEX(status->log_tail)));
        }

    return rvm_true;
    }
rvm_return_t update_log_tail(log,rec_hdr)
    log_t           *log;
    rec_hdr_t       *rec_hdr;           /* header of last record */
    {
    log_status_t    *status = &log->status; /* status area descriptor */
    rvm_length_t    temp;

    CODA_ASSERT(((&log->dev == &default_log->dev) && (!rvm_utlsw)) ?
           (!LOCK_FREE(default_log->dev_lock)) : 1);

    /* update unique name timestamps */
    status->last_write = rec_hdr->timestamp;
    if (TIME_EQL_ZERO(status->first_write))
        status->first_write = status->last_write;

    status->log_empty = rvm_false;
    if (rec_hdr->struct_id != log_wrap_id)
        {
        /* update and check tail length */
        temp = rec_hdr->rec_length+sizeof(rec_end_t);
        CODA_ASSERT(temp == log->dev.io_length);
#ifdef RVM_LOG_TAIL_BUG
	unprotect_page__Fi(ClobberAddress);
#endif RVM_LOG_TAIL_BUG
#ifdef RVM_LOG_TAIL_SHADOW
    CODA_ASSERT(RVM_OFFSET_EQL(log_tail_shadow,status->log_tail));
#endif RVM_LOG_TAIL_SHADOW
        status->log_tail = RVM_ADD_LENGTH_TO_OFFSET(status->log_tail,
                                                    temp);
#ifdef RVM_LOG_TAIL_SHADOW
	RVM_ASSIGN_OFFSET(log_tail_shadow,status->log_tail);
#endif RVM_LOG_TAIL_SHADOW
#ifdef RVM_LOG_TAIL_BUG
	protect_page__Fi(ClobberAddress);
#endif RVM_LOG_TAIL_BUG
        CODA_ASSERT(chk_tail(log));

        /* update unames if transaction */
        if (rec_hdr->struct_id == trans_hdr_id)
            {
            status->last_uname = ((trans_hdr_t *)rec_hdr)->uname;
            if (TIME_EQL_ZERO(status->first_uname))
                status->first_uname = status->last_uname;
            }

        /* count updates & update disk copies if necessary */
        if (--status->update_cnt != 0)
            return RVM_SUCCESS;
        }

    if (sync_dev(&log->dev) < 0)        /* sync file buffers before status write */
        return RVM_EIO;

    /* if tail wrapped around, correct pointers */
    if (rec_hdr->struct_id == log_wrap_id)
        {
#ifdef RVM_LOG_TAIL_BUG
        unprotect_page__Fi(ClobberAddress);
#endif RVM_LOG_TAIL_BUG
#ifdef RVM_LOG_TAIL_SHADOW
    CODA_ASSERT(RVM_OFFSET_EQL(log_tail_shadow,status->log_tail));
#endif RVM_LOG_TAIL_SHADOW
        status->log_tail = status->log_start;
#ifdef RVM_LOG_TAIL_SHADOW
	RVM_ASSIGN_OFFSET(log_tail_shadow,status->log_tail);
#endif RVM_LOG_TAIL_SHADOW
#ifdef RVM_LOG_TAIL_BUG
        protect_page__Fi(ClobberAddress);
#endif RVM_LOG_TAIL_BUG
        log->dev.sync_offset = status->log_start;
        CODA_ASSERT(chk_tail(log));
        }

    return write_log_status(log,NULL);  /* update disk status block */
    }
/* determine total length of log tail area */
void log_tail_length(log,tail_length)
    log_t           *log;               /* log descriptor */
    rvm_offset_t    *tail_length;       /* length [out] */
    {
    log_status_t    *status = &log->status; /* status area descriptor */
    rvm_offset_t    temp;

    /* determine effective head */
    if (!RVM_OFFSET_EQL_ZERO(status->prev_log_head))
        temp = CHOP_OFFSET_TO_SECTOR_SIZE(status->prev_log_head);
    else                                /* no previous epoch */
        temp = CHOP_OFFSET_TO_SECTOR_SIZE(status->log_head);

    /* determine usable area */
    if (RVM_OFFSET_GEQ(status->log_tail,status->log_head) &&
        RVM_OFFSET_GEQ(status->log_tail,status->prev_log_head))
        {
        /* current not wrapped & previous not wrapped */
        *tail_length = RVM_SUB_OFFSETS(log->dev.num_bytes,
                                       status->log_tail);
        if (RVM_OFFSET_LSS(*tail_length,min_trans_size))
            RVM_ZERO_OFFSET(*tail_length);
        *tail_length = RVM_ADD_OFFSETS(*tail_length,temp);
        *tail_length = RVM_SUB_OFFSETS(*tail_length,status->log_start);
        }
    else
        /* all other cases */
        *tail_length = RVM_SUB_OFFSETS(temp,status->log_tail);

    }
/* determine length of log tail area usable in single write */
void log_tail_sngl_w(log,tail_length)
    log_t           *log;               /* log descriptor */
    rvm_offset_t    *tail_length;       /* length [out] */
    {
    log_status_t    *status = &log->status; /* status area descriptor */

    /* determine effective head */
    if (!RVM_OFFSET_EQL_ZERO(status->prev_log_head))
        *tail_length=CHOP_OFFSET_TO_SECTOR_SIZE(status->prev_log_head);
    else                            /* no previous epoch */
        *tail_length = CHOP_OFFSET_TO_SECTOR_SIZE(status->log_head);

    /* determine effective end of useable area if
        neither current nor previous wrapped */
    if (RVM_OFFSET_GEQ(status->log_tail,status->log_head) &&
        RVM_OFFSET_GEQ(status->log_tail,status->prev_log_head))
        *tail_length = log->dev.num_bytes;

    /* subtract current current tail & verify log ptrs */
    *tail_length = RVM_SUB_OFFSETS(*tail_length,status->log_tail);
    CODA_ASSERT(chk_tail(log));
    }
/* determine length of log currently in use */
void cur_log_length(log,length)
    log_t           *log;               /* log descriptor */
    rvm_offset_t    *length;            /* length [out] */
    {
    log_status_t    *status = &log->status; /* log status area descriptor */

    if (RVM_OFFSET_GEQ(status->log_tail,status->log_head))
        *length = RVM_SUB_OFFSETS(status->log_tail,status->log_head);
    else
        {
        *length = RVM_SUB_OFFSETS(log->dev.num_bytes,status->log_head);
        *length = RVM_ADD_OFFSETS(*length,status->log_tail);
        *length = RVM_SUB_OFFSETS(*length,status->log_start);
        }
    }

/* determine percentage of log currently in use */
long cur_log_percent(log,space_needed)
    log_t           *log;               /* log descriptor */
    rvm_offset_t    *space_needed;      /* space neded immediately */
    {
    log_status_t    *status = &log->status; /* log status area descriptor */
    float           cur_size;           /* current size of log as float */
    rvm_length_t    cur_percent;        /* current franction of log used (%) */
    rvm_offset_t    temp;               /* log free space calculation temp */

    CRITICAL(log->dev_lock,             /* begin dev_lock crit sec */
        {
        /* find out how much space is there now & set high water mark */
        log_tail_length(log,&temp);
        temp = RVM_SUB_OFFSETS(status->log_size,temp);
        cur_size = OFFSET_TO_FLOAT(temp);
        cur_percent = (long)(100.0*(cur_size/
                                    OFFSET_TO_FLOAT(status->log_size)));
        CODA_ASSERT((cur_percent >= 0) && (cur_percent <= 100));
        if (cur_percent > status->log_dev_max)
            status->log_dev_max = cur_percent;

        /* if space_needed specified, recompute percentage */
        if (space_needed != NULL)
            {
            temp = RVM_ADD_OFFSETS(temp,*space_needed);
            cur_size = OFFSET_TO_FLOAT(temp);
            cur_percent = (long)(100.0*(cur_size/
                                    OFFSET_TO_FLOAT(status->log_size)));
            }
        });                             /* end dev_lock crit sec */

    return cur_percent;
    }
/* rvm_create_log application interface */
rvm_return_t rvm_create_log(rvm_options,log_len,mode)
    rvm_options_t   *rvm_options;       /* ptr to options record */
    rvm_offset_t    *log_len;           /* length of log data area */
    long            mode;               /* file creation protection mode */
    {
    log_t           *log;               /* descriptor for log */
    rvm_offset_t    offset;             /* offset temporary */
    char            *end_mark = "end";
    long            save_errno;
    rvm_return_t    retval;

    if (bad_init()) return RVM_EINIT;   /* not initialized */
    if ((retval=bad_options(rvm_options,rvm_true)) != RVM_SUCCESS)
        return retval;                  /* bad options ptr or record */
    if (rvm_options == NULL)
        return RVM_EOPTIONS;            /* must have an options record */

    /* check length of file name */
    if (strlen(rvm_options->log_dev) >= MAXPATHLEN)
        return RVM_ENAME_TOO_LONG;

    /* check that log file length is legal */
    offset = RVM_ADD_LENGTH_TO_OFFSET(*log_len,
    	    	 LOG_DEV_STATUS_SIZE+FILE_STATUS_OFFSET);
    offset = CHOP_OFFSET_TO_SECTOR_SIZE(offset);
    if (RVM_OFFSET_HIGH_BITS_TO_LENGTH(offset) != 0)
        return RVM_ETOO_BIG;

    /* be sure not an already declared log */
    if (find_log(rvm_options->log_dev) != NULL)
        return RVM_ELOG;

    /* build a log descriptor and create log file*/
    if ((log=make_log(rvm_options->log_dev,&retval)) == NULL)
        return retval;
#ifdef RVM_LOG_TAIL_BUG
    /* 
      We only need to track the log descriptor while we are 
      building it.  It isn't going to be inserted into the list
      until later, so ClobberAddress won't be set properly.
    */
    ClobberAddress = &(log->status.log_tail.low);
    protect_page__Fi(ClobberAddress);
#endif RVM_LOG_TAIL_BUG
#ifdef RVM_LOG_TAIL_SHADOW
    RVM_ASSIGN_OFFSET(log_tail_shadow,log->status.log_tail);
#endif RVM_LOG_TAIL_SHADOW
    if (open_dev(&log->dev,O_WRONLY,mode) == 0) /* don't allow create yet */
        {
        retval = RVM_ELOG;              /* error -- file already exists */
        goto err_exit;
        }
    if (errno != ENOENT)
        {
        retval = RVM_EIO;               /* other i/o error, errno specifies */
        goto err_exit;
        }
    if (open_dev(&log->dev,O_WRONLY | O_CREAT,mode) != 0)
        {                               /* do real create */
        retval = RVM_EIO;
        goto err_exit;
        }
    /* force file length to specified size by writting last byte */
    log->dev.num_bytes = offset;
    offset = RVM_SUB_LENGTH_FROM_OFFSET(offset,strlen(end_mark));
    if (write_dev(&log->dev,&offset,end_mark,
                  strlen(end_mark),NO_SYNCH) < 0)
        {
        retval = RVM_EIO;
        goto err_exit;
        }

    /* complete initialization */
    retval = init_log_status(log);

err_exit:
    if (log->dev.handle != 0)
        {
        save_errno = errno;
        (void)close_dev(&log->dev);
        errno = save_errno;
        }
#ifdef RVM_LOG_TAIL_BUG
    /* drop the "temporary" clobber address */
    unprotect_page__Fi(ClobberAddress);
    ClobberAddress = 0;
#endif RVM_LOG_TAIL_BUG
#ifdef RVM_LOG_TAIL_SHADOW
    RVM_ZERO_OFFSET(log_tail_shadow);
#endif RVM_LOG_TAIL_SHADOW
    free_log(log);

    return retval;
    }
/* special routines for basher */
rvm_offset_t rvm_log_head()
    {
    return default_log->status.log_head;
    }

rvm_offset_t rvm_log_tail()
    {
    return default_log->status.log_tail;
    }

rvm_length_t rvm_next_rec_num()
    {
    return default_log->status.next_rec_num;
    }
