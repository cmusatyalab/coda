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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/rvm-src/rvm/Attic/rvm_logflush.c,v 4.1 1997/01/08 21:54:33 rvb Exp $";
#endif _BLURB_

/*
*
*                            RVM log records flush support
*
*/

#include <sys/time.h>
#include "rvm_private.h"

/* global variables */

extern  int         errno;              /* kernel error number */
extern  log_t       *default_log;       /* default log descriptor ptr */
extern char         *rvm_errmsg;        /* internal error message buffer */
extern rvm_bool_t   rvm_utlsw;          /* running under rvmutl */
extern rvm_length_t rvm_optimizations;  /* optimization switches */

rvm_length_t        flush_times_vec[flush_times_len] = {flush_times_dist};
rvm_length_t        range_lengths_vec[range_lengths_len] =
                                        {range_lengths_dist};
rvm_length_t        range_overlaps_vec[range_overlaps_len] =
                                        {range_overlaps_dist};
rvm_length_t        trans_overlaps_vec[trans_overlaps_len] =
                                        {trans_overlaps_dist};
rvm_length_t        range_elims_vec[range_elims_len] =
                                        {range_elims_dist};
rvm_length_t        trans_elims_vec[trans_elims_len] =
                                        {trans_elims_dist};
rvm_length_t        trans_coalesces_vec[trans_coalesces_len] =
                                        {trans_coalesces_dist};
/* allocate variable sized log i/o vector */
static rvm_return_t make_iov(log,length)
    log_t           *log;               /* log descriptor */
    long            length;             /* entries needed */
    {
    device_t        *dev = &log->dev;   /* device descriptor */

    /* test if enough space already available */
    if (dev->iov_len < length)
        {
        /* no, free old array */
        dev->iov_len = 0;
        if (dev->iov != NULL)
            free((char *)dev->iov);

        /* reallocate io vector */
        dev->iov = (io_vec_t *)malloc((unsigned)(sizeof(io_vec_t)*
                                          length));
        if (dev->iov == NULL) return RVM_ENO_MEMORY;
        dev->iov_len = length;
        }

    /* initialize */
    dev->io_length = 0;
    dev->iov_cnt = 0;

    return RVM_SUCCESS;
    }

/* make record number */
static long make_rec_num(log)
    log_t           *log;               /* log descriptor */
    {
    if (log->status.first_rec_num == 0)
        log->status.first_rec_num = log->status.next_rec_num;
    return log->status.next_rec_num++;
    }
/* allocate variable sized pad buffer */
static void make_pad_buf(dev,length)
    device_t        *dev;               /* device descriptor */
    long            length;             /* entries needed */
    {

    ASSERT((length >= 0) && (length < SECTOR_SIZE));

    /* see if must reallocate */
    if (length > dev->pad_buf_len)
        {
        dev->pad_buf = REALLOC(dev->pad_buf,length);
        ASSERT(dev->pad_buf != NULL);
        (void)memset(&dev->pad_buf[dev->pad_buf_len],-1,
                     length-dev->pad_buf_len);
        dev->pad_buf_len = length;
        }
    }
/* setup wrap marker i/o */
static rvm_return_t write_log_wrap(log)
    log_t           *log;
    {
    device_t        *dev = &log->dev;
    log_wrap_t      *wrap = &log->log_wrap;
    rvm_offset_t    pad_len;

    /* set timestamp and record number for wrap */
    make_uname(&wrap->timestamp);
    wrap->rec_num = make_rec_num(log);
    log->status.tot_wrap++;

    /* make iov entry */
    dev->iov[dev->iov_cnt].vmaddr = (char *)wrap;
    dev->iov[dev->iov_cnt++].length = sizeof(log_wrap_t);
    dev->io_length += sizeof(log_wrap_t);

    /* pad last sector with all 1's to kill previous wrap mark */
    pad_len = RVM_ADD_LENGTH_TO_OFFSET(log->status.log_tail,
                                       dev->io_length);
    pad_len = RVM_SUB_OFFSETS(dev->num_bytes,pad_len);
    make_pad_buf(dev,RVM_OFFSET_TO_LENGTH(pad_len));
    dev->iov[dev->iov_cnt].vmaddr = dev->pad_buf;
    dev->iov[dev->iov_cnt++].length = RVM_OFFSET_TO_LENGTH(pad_len);
    dev->io_length += RVM_OFFSET_TO_LENGTH(pad_len);

    ASSERT(dev->iov_cnt <= dev->iov_len);

    if (gather_write_dev(&log->dev,&log->status.log_tail) < 0)
        return RVM_EIO;
#ifdef RVM_LOG_TAIL_SHADOW
    /* 
     * If we've gotten this far, we're going to update the log_tail pointer, 
     * so there is a log wrap that we can allow when writing out the status
     * block.
     */
    has_wrapped = rvm_true;
#endif RVM_LOG_TAIL_SHADOW
    return update_log_tail(log,(rec_hdr_t *)wrap);
    }
/* setup header for nv log entry */
static void build_trans_hdr(tid,is_first,is_last)
    int_tid_t       *tid;
    rvm_bool_t      is_first;           /* true if 1st header written */
    rvm_bool_t      is_last;            /* true if last header written */
    {
    log_t           *log = tid->log;
    trans_hdr_t     *trans_hdr = &tid->log->trans_hdr;
    device_t        *dev = &log->dev;

    /* setup entry header */
    make_uname(&trans_hdr->timestamp);
    trans_hdr->rec_num = make_rec_num(log);
    trans_hdr->num_ranges = 0;
    trans_hdr->rec_length = TRANS_SIZE - sizeof(rec_end_t);
    trans_hdr->uname = tid->uname;
    trans_hdr->commit_stamp = tid->commit_stamp;
    log->status.last_commit = tid->commit_stamp;

    trans_hdr->flags = tid->flags;
    if (is_first)
        trans_hdr->flags |= FIRST_ENTRY_FLAG;
    if (is_last)
        trans_hdr->flags |= LAST_ENTRY_FLAG;
    trans_hdr->n_coalesced = tid->n_coalesced;

    tid->back_link = sizeof(trans_hdr_t);

    /* enter in iovec */
    dev->iov[0].vmaddr = (char *)trans_hdr;
    dev->iov[0].length = sizeof(trans_hdr_t);
    dev->io_length = TRANS_SIZE;
    dev->iov_cnt = 1;

    }
/* setup end marker for log entry */
static void build_rec_end(log,timestamp,rec_num,rec_type,back_link)
    log_t           *log;               /* log descriptor */
    struct timeval  *timestamp;         /* log record timestamp */
    long            rec_num;            /* log record sequence number */
    struct_id_t     rec_type;           /* struct_id of rec header */
    rvm_length_t    back_link;          /* displacement to previous header */
    {
    rec_end_t       *rec_end = &log->rec_end;
    trans_hdr_t     *trans_hdr = &log->trans_hdr;
    device_t        *dev = &log->dev;

    /* setup entry end marker */
    rec_end->rec_num = rec_num;
    rec_end->rec_type = rec_type;
    rec_end->timestamp = *timestamp;
    rec_end->rec_length = dev->io_length - sizeof(rec_end_t);
    trans_hdr->rec_length = rec_end->rec_length;
    rec_end->sub_rec_len = back_link;

    /* enter in iovec */
    dev->iov[dev->iov_cnt].vmaddr = (char *)rec_end;
    dev->iov[dev->iov_cnt++].length = sizeof(rec_end_t);

    ASSERT(dev->iov_cnt <= dev->iov_len);
    }
/* setup nv_range record */
static void build_nv_range(log,tid,range)
    log_t           *log;               /* log descriptor */
    int_tid_t       *tid;               /* transaction descriptor */
    range_t         *range;             /* range descriptor */
    {
    nv_range_t      *nv_range;          /* nv_range header */
    device_t        *dev = &log->dev;

    /* setup header fields */
    nv_range = &range->nv;
    log->trans_hdr.num_ranges += 1;
    nv_range->timestamp = log->trans_hdr.timestamp;
    nv_range->range_num = log->trans_hdr.num_ranges;
    nv_range->rec_num = log->trans_hdr.rec_num;
    nv_range->rec_length = RANGE_SIZE(range);
    nv_range->chk_sum =
        chk_sum(range->nvaddr+BYTE_SKEW(nv_range->vmaddr),
                range->nv.length);
    dev->io_length += nv_range->rec_length; /* accumulate lengths */
    nv_range->sub_rec_len = tid->back_link;
    tid->back_link = nv_range->rec_length;

    /* setup header i/o */
    dev->iov[dev->iov_cnt].vmaddr = (char *)nv_range;
    dev->iov[dev->iov_cnt++].length = sizeof(nv_range_t);
    ASSERT(dev->iov_cnt <= dev->iov_len);
    
    /* setup io for new values */
    dev->iov[dev->iov_cnt].vmaddr =
        (char *)CHOP_TO_LENGTH(range->nvaddr);
    dev->iov[dev->iov_cnt++].length = RANGE_LEN(range);

    ASSERT(dev->iov_cnt <= dev->iov_len);
    enter_histogram(nv_range->length,log->status.range_lengths,
                    range_lengths_vec,range_lengths_len);
    }
static void split_range(range,new_range,avail)
    range_t         *range;             /* range to split */
    range_t         *new_range;         /* temporary range descriptor */
    rvm_length_t    avail;              /* space available */
    {

    /* copy basic data from parent range */
    new_range->nv.timestamp = range->nv.timestamp;
    new_range->nv.seg_code = range->nv.seg_code;
    new_range->nv.vmaddr = range->nv.vmaddr;
    new_range->nv.offset = range->nv.offset;
    new_range->nv.is_split = range->nv.is_split;
    new_range->nvaddr = range->nvaddr;
    new_range->data = NULL;
    new_range->data_len = 0;

    /* set length of split */
    ASSERT(BYTE_SKEW(avail) == 0);
    new_range->nv.length =
        avail - BYTE_SKEW(RVM_OFFSET_TO_LENGTH(range->nv.offset));

    /* adjust original range for split */
    range->nvaddr = RVM_ADD_LENGTH_TO_ADDR(range->nvaddr,avail);
    range->nv.vmaddr =
        RVM_ADD_LENGTH_TO_ADDR(range->nv.vmaddr,new_range->nv.length);
    range->nv.length -= new_range->nv.length;
    range->nv.offset = RVM_ADD_LENGTH_TO_OFFSET(range->nv.offset,
                                                new_range->nv.length);
    range->nv.is_split = rvm_true;

    ASSERT(BYTE_SKEW(range->nv.vmaddr) == 0);
    ASSERT(BYTE_SKEW(range->nvaddr) == 0);
    ASSERT(BYTE_SKEW(RVM_OFFSET_TO_LENGTH(range->nv.offset)) == 0);
    }
static rvm_bool_t write_range(tid,range,log_free)
    int_tid_t       *tid;               /* transaction descriptor */
    range_t         *range;             /* range descriptor */
    rvm_offset_t    *log_free;          /* ptr to length of log tail area */
    {
    log_t           *log = tid->log;    /* log descriptor */
    rvm_offset_t    avail;              /* log space available */

    /* assign default nvaddr */
    if (range->nvaddr == NULL)
        range->nvaddr = (char *)CHOP_TO_LENGTH(range->nv.vmaddr);

    /* see if range will fit in log */
    avail = RVM_SUB_LENGTH_FROM_OFFSET(*log_free,
                    ((long)log->dev.io_length+sizeof(log_wrap_t)));
    ASSERT(RVM_OFFSET_GTR(*log_free,avail)); /* underflow!! */
    if (RANGE_SIZE(range) > RVM_OFFSET_TO_LENGTH(avail))
        {
        /* no, see if there's enough useful */
        if (RVM_OFFSET_TO_LENGTH(avail) < MIN_NV_RANGE_SIZE)
            return rvm_true;            /* no, wrap around first */

        /* yes, build new descriptor for as much as fits */
        split_range(range,&tid->split_range,
                    RVM_OFFSET_TO_LENGTH(avail)-NV_RANGE_OVERHEAD);
        build_nv_range(log,tid,&tid->split_range);
        return rvm_true;                /* now wrap around */
        }

    /* enter nv_range header & new values */
    build_nv_range(log,tid,range);

    /* do region's uncommitted transaction accounting */
    if (TID(FLUSH_FLAG))
        CRITICAL(range->region->count_lock,
            range->region->n_uncommit--);

    return rvm_false;
    }
static rvm_return_t write_tid(tid)
    int_tid_t       *tid;               /* transaction descriptor */
    {
    log_t           *log = tid->log;    /* log descriptor */
    log_status_t    *status = &log->status; /* status block descriptor */
    range_t         *range;             /* range ptr */
    rvm_offset_t    log_free;           /* size of log tail area */
    rvm_return_t    retval;

    /* check that transactions are logged in commit order */
    ASSERT(TIME_GTR(tid->commit_stamp,log->status.last_commit));

    /* initialize counters & allocate i/o vector for 2*(#ranges+1) plus
       2 headers, 2 end marks, wrap marker, and padding (6) */
    if ((retval=make_iov(log,2*(tid->range_tree.n_nodes+1)+6))
        !=RVM_SUCCESS) return retval;

    /* see if must wrap before logging tid */
    log_tail_sngl_w(log,&log_free);
    if (RVM_OFFSET_TO_LENGTH(log_free) < MIN_TRANS_SIZE)
        {
        if ((retval=write_log_wrap(log)) != RVM_SUCCESS)
            return retval;
        log_tail_sngl_w(log,&log_free);
        }

    /* output transaction header */
    build_trans_hdr(tid,rvm_true,rvm_true);

    /* build log records */
    FOR_NODES_OF(tid->range_tree,range_t,range)
        {
        if (write_range(tid,range,&log_free))
            {
            /* insert end marker */
            build_rec_end(log,&log->trans_hdr.timestamp,
                          log->trans_hdr.rec_num,
                          trans_hdr_id,tid->back_link);

            /* write a wrap and restart */
            log->status.n_split++;
            log->trans_hdr.flags &= ~LAST_ENTRY_FLAG;
            if ((retval=write_log_wrap(log)) != RVM_SUCCESS)
                return retval;

            /* make new transaction log entry header */
            log_tail_sngl_w(log,&log_free);
            build_trans_hdr(tid,rvm_false,rvm_true);

            /* process remainder of range */
            if (write_range(tid,range,&log_free))
                ASSERT(rvm_false);
            }
        }
    /* insert end marker */
    build_rec_end(log,&log->trans_hdr.timestamp,
                  log->trans_hdr.rec_num,
                  trans_hdr_id,tid->back_link);

    /* accumulate range savings statistics and initiate i/o */
    status->range_overlap = RVM_ADD_OFFSETS(status->range_overlap,
                                            tid->range_overlap);
    status->trans_overlap = RVM_ADD_OFFSETS(status->trans_overlap,
                                            tid->trans_overlap);
    status->n_range_elim += tid->range_elim;
    status->n_trans_elim += tid->trans_elim;
    status->n_trans_coalesced += tid->n_coalesced;
    enter_histogram(tid->range_elim,status->range_elims,
                    range_elims_vec,range_elims_len);
    enter_histogram(tid->trans_elim,status->trans_elims,
                    trans_elims_vec,trans_elims_len);
    enter_histogram(RVM_OFFSET_TO_LENGTH(tid->range_overlap),
                    status->range_overlaps,
                    range_overlaps_vec,range_overlaps_len);
    enter_histogram(RVM_OFFSET_TO_LENGTH(tid->trans_overlap),
                    status->trans_overlaps,
                    trans_overlaps_vec,trans_overlaps_len);
    enter_histogram(tid->n_coalesced,status->tot_trans_coalesces,
                    trans_coalesces_vec,trans_coalesces_len);
    if (gather_write_dev(&log->dev,&log->status.log_tail) < 0)
        return RVM_EIO;
    return update_log_tail(log,(rec_hdr_t *)&log->trans_hdr);
    }
/* wait for a truncation to free space for log record
   -- assumes dev_lock is held */
static rvm_return_t wait_for_space(log,space_needed,log_free,did_wait)
    log_t           *log;               /* log descriptor */
    rvm_offset_t    *space_needed;      /* amount of space required */
    rvm_offset_t    *log_free;          /* size calculation temp */
    rvm_bool_t      *did_wait;
    {
    rvm_return_t    retval = RVM_SUCCESS;

    /* see if enough space for current record */
    *did_wait = rvm_false;
    DO_FOREVER
        {
        log_tail_length(log,log_free);
        if (RVM_OFFSET_GEQ(*log_free,*space_needed))
            break;                      /* enough */

        /* no, release log device & wait for truncation completion */
        mutex_unlock(&log->dev_lock);
        retval = wait_for_truncation(log,NULL);

        /* regain log device & count waits */
        mutex_lock(&log->dev_lock);
        *did_wait = rvm_true;
        log->status.n_truncation_wait++;
        if (retval != RVM_SUCCESS) break;
        }

    return retval;
    }
/* compute log entry size; truncate if necessary */
static rvm_return_t log_tid(log,tid)
    log_t           *log;               /* log descriptor */
    int_tid_t       *tid;               /* tid to log */
    {
    rvm_offset_t    log_free;           /* log size temp, debug only */
    rvm_bool_t      did_wait;           /* debug only */
    rvm_return_t    retval;             /* return value */

    /* start daemon truncation if necessary */
    (void)initiate_truncation(log,cur_log_percent(log,&tid->log_size));

    CRITICAL(log->dev_lock,             /* begin dev_lock crit sec */
        {
        /* flush any immediate stream records */
        if ((retval=flush_log_special(log)) != RVM_SUCCESS)
            goto err_exit;

        /* wait if truncation required to get space */
        retval = wait_for_space(log,&tid->log_size,
                                &log_free,&did_wait);
        if (retval != RVM_SUCCESS) goto err_exit;

        /* transfer tid to log device */
        if ((retval=write_tid(tid)) != RVM_SUCCESS)
            goto err_exit;

        /* save uname of first & last transactions logged */
        log->status.last_uname = tid->uname;
        if (TIME_EQL_ZERO(log->status.first_uname))
            log->status.first_uname = tid->uname;
err_exit:;
        });                             /* end dev_lock crit sec */
    if (retval != RVM_SUCCESS) return retval;

    /* scrap tid */
    if (retval == RVM_SUCCESS)
        CRITICAL(log->flush_list_lock,free_tid(tid));

    return retval;
    }
/* set up special log entry i/o */
static void build_log_special(log,special)
    log_t           *log;               /* log descriptor */
    log_special_t   *special;           /* special log entry */
    {
    device_t        *dev = &log->dev;   /* log device descriptor */
    rvm_length_t    length;

    /* timestamp the entry */
    make_uname(&special->timestamp);

    /* check that records are logged in strict FIFO order */
    ASSERT(TIME_GTR(special->timestamp,log->status.last_write));

    /* prepare i/o */
    special->rec_num = make_rec_num(log);
    dev->io_length = special->rec_length+sizeof(rec_end_t);
    dev->iov[dev->iov_cnt].vmaddr = (char *)&special->struct_id;
    dev->iov[dev->iov_cnt++].length = LOG_SPECIAL_SIZE;

    /* type-specific build operations */
    switch (special->struct_id)
        {
      case log_seg_id:                  /* copy segment device name */
            {
            length = special->rec_length-LOG_SPECIAL_SIZE;
            dev->iov[dev->iov_cnt].vmaddr =
                special->special.log_seg.name;
            dev->iov[dev->iov_cnt++].length = length;
            break;
            }
       default:         ASSERT(rvm_false); /* unknown record type */
         }

    ASSERT(dev->iov_cnt <= dev->iov_len);
    }
/* insure space available in log; truncate if necessary, and initiate
   i/o for special log entries; */
static rvm_return_t log_special(log,special)
    log_t           *log;               /* log descriptor */
    log_special_t   *special;           /* special entry descriptor */
    {
    rvm_offset_t    max_log_free;       /* log size temp, debug only */
    rvm_bool_t      did_wait;           /* debug only */
    rvm_offset_t    log_free;           /* size calculation temp */
    rvm_offset_t    special_size;       /* maximum size needed in log */
    rvm_return_t    retval;             /* return value */

    /* see if truncation required to get space */
    special_size = RVM_MK_OFFSET(0,special->rec_length
                  + sizeof(log_wrap_t) + sizeof(rec_end_t));
    if ((retval=wait_for_space(log,&special_size,
                               &max_log_free,&did_wait))
        != RVM_SUCCESS) return retval;

    /* be sure enough i/o vector slots available */
    if ((retval=make_iov(log,LOG_SPECIAL_IOV_MAX))
        != RVM_SUCCESS) return retval;
    
    /* find out how much log available & wrap if necessary */
    log_tail_sngl_w(log,&log_free);
    if (RVM_OFFSET_LSS(log_free,special_size))
        if ((retval=write_log_wrap(log)) != RVM_SUCCESS)
            return retval;

    /* build special entry */
    log->status.n_special++;
    build_log_special(log,special);
    build_rec_end(log,&special->timestamp,special->rec_num,
                  special->struct_id,special->rec_length);

    /* do the i/o & update log tail */
    if (gather_write_dev(&log->dev,&log->status.log_tail) < 0)
        return RVM_EIO;
    retval = update_log_tail(log,(rec_hdr_t *)&special->struct_id);
    if (retval != RVM_SUCCESS) return retval;
    
    free_log_special(special);

    return RVM_SUCCESS;
    }
/* log immediate records flush -- log device locked by caller */
rvm_return_t flush_log_special(log)
    log_t           *log;
    {
    log_special_t   *special;           /* special record to log */
    rvm_return_t    retval = RVM_SUCCESS;

    /* process the special list */
    DO_FOREVER
        {
        CRITICAL(log->special_list_lock, /* begin special_list_lock crit sec */
            {
            if (LIST_NOT_EMPTY(log->special_list))
                special = (log_special_t *)
                    move_list_entry(&log->special_list,NULL,NULL);
            else special = NULL;
            });                         /* end special_list_lock crit sec */
        if (special == NULL) break;

        /* flush this special request */
        if ((retval=log_special(log,special)) != RVM_SUCCESS)
            break;
        }

    return retval;
    }
/* internal log flush */
rvm_return_t flush_log(log,count)
    log_t           *log;
    long            *count;              /* statistics counter */
    {
    int_tid_t       *tid;               /* tid to log */
    rvm_bool_t      break_sw;           /* break switch for loop termination */
    struct timeval  start_time;
    struct timeval  end_time;
    long            kretval;
    rvm_return_t    retval = RVM_SUCCESS;

    /* allow only one flush at a time to avoid commit ordering problems */
    RW_CRITICAL(log->flush_lock,w,      /* begin flush_lock crit sec */
        {
        /* process statistics */
        if (count != NULL) (*count)++;
        kretval= gettimeofday(&start_time,(struct timezone *)NULL);
        if (kretval != 0)
            {
            retval = RVM_EIO;
            goto err_exit;
            }

        /* establish flush mark so future commits won't be flushed
           and cause extraordinarily long delay to this flush */
        CRITICAL(log->flush_list_lock,  /* begin flush_list_lock crit sec */
            {
            if (LIST_NOT_EMPTY(log->flush_list))
                ((int_tid_t *)(log->flush_list.preventry))->flags
                    |= FLUSH_MARK;
            });                         /* end flush_list_lock crit sec */
        /* flush all queued tid's */
        DO_FOREVER
            {
            /* do tid's one at a time to allow no_flush commits while flushing */
            CRITICAL(log->flush_list_lock, /* begin flush_list_lock crit sec */
                {
                if (LIST_NOT_EMPTY(log->flush_list))
                    tid = (int_tid_t *)log->flush_list.nextentry;
                else tid = NULL;
                });                     /* end flush_list_lock crit sec */
            if (tid == NULL) break;

            /* flush this tid */
            break_sw = (rvm_bool_t)TID(FLUSH_MARK);
            retval = log_tid(log,tid);
            if ((retval != RVM_SUCCESS) || break_sw)
                break;
            }

        /* force buffers to disk */
        CRITICAL(log->dev_lock,
            {
            if (sync_dev(&log->dev) < 0)
                retval = RVM_EIO;
            });
err_exit:;
        });                             /* end flush_lock crit sec */

    /* terminate timing */
    if (retval == RVM_SUCCESS)
        {
        kretval= gettimeofday(&end_time,(struct timezone *)NULL);
        if (kretval != 0) return RVM_EIO;
        end_time = sub_times(&end_time,&start_time);
        log->status.flush_time = add_times(&log->status.flush_time,
                                           &end_time);
        end_time.tv_usec = end_time.tv_usec/1000;
        end_time.tv_usec += end_time.tv_sec*1000;
        log->status.last_flush_time = end_time.tv_usec;
        enter_histogram(end_time.tv_usec,log->status.flush_times,
                        flush_times_vec,flush_times_len);
        }
    return retval;
    }
/* exported flush routine */
rvm_return_t rvm_flush()
    {
    rvm_return_t    retval;

    /* do application interface checks */
    if (bad_init()) return RVM_EINIT;
    if (default_log == NULL) return RVM_ELOG;

    /* flush the queues */
    if ((retval=flush_log(default_log,
                          &default_log->status.n_rvm_flush))
        != RVM_SUCCESS) return retval;

    return RVM_SUCCESS;
    }
/* special log entries enqueuing routine */
rvm_return_t queue_special(log,special)
    log_t           *log;               /* log descriptor */
    log_special_t   *special;           /* special entry descriptor */
    {

    /* queue  request for immediate flush */
    CRITICAL(log->special_list_lock,
             (void)move_list_entry(NULL,&log->special_list,
                                   &special->links));

    return RVM_SUCCESS;
    }
