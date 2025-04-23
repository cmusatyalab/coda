/* BLURB lgpl

                           Coda File System
                              Release 8

          Copyright (c) 1987-2025 Carnegie Mellon University
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
*                       RVM log recovery support
*
*/
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/time.h>
#include <errno.h>
#include "rvm_private.h"

#ifdef RVM_LOG_TAIL_BUG
#include <rvmtesting.h>
extern unsigned long *ClobberAddress;
#endif /* RVM_LOG_TAIL_BUG */

/* global variables */

extern log_t *default_log; /* default log descriptor ptr */
extern list_entry_t seg_root; /* segment list */
extern rw_lock_t seg_root_lock; /* segment list lock */
extern rvm_bool_t rvm_utlsw; /* true if running in rvmutl */
extern char *rvm_errmsg; /* internal error message buffer */

rvm_bool_t rvm_no_yield = rvm_false; /* inhibit yields in recovery */
rvm_length_t rvm_num_nodes; /* number of nodes in change tree */
rvm_length_t rvm_max_depth; /* maximum depth of change tree */

chk_vec_t *rvm_chk_vec          = NULL; /* monitor range vector */
rvm_length_t rvm_chk_len        = 0; /* length of monitor range vector */
rvm_monitor_call_t *rvm_monitor = NULL; /* call-back function ptr */
rvm_signal_call_t *rvm_chk_sigint; /* SIGINT test call (rvmutl only) */
rvm_length_t truncation_times_vec[truncation_times_len] = {
    truncation_times_dist
};
rvm_bool_t rvm_no_update; /* no segment or log update if true */
rvm_bool_t rvm_replay; /* is replay if true */
rvm_bool_t rvm_chk_sum; /* force checksumming of all records */
rvm_bool_t rvm_shadow_buf; /* use shadow buffer */

/* macros & locals */

#ifndef ZERO
#define ZERO 0
#else
#endif

/*static rvm_length_t     nv_local_max = NV_LOCAL_MAX;*/
static struct timeval trunc_start_time;
static rvm_length_t last_tree_build_time;
static rvm_length_t last_tree_apply_time;

#define NODES_PER_YIELD 1000000
static rvm_length_t num_nodes = NODES_PER_YIELD;
/* test if modification range will change monitored addresses */
/* nv_addr   - vm address */
/* nv_len    - length of vm range */
/* nv_data   - nv data in vm */
/* nv_offset - offset of data in log */
/* rec_hdr   - ptr to record header if not null */
/* msg       - invocation message */
static void monitor_vmaddr(char *nv_addr, rvm_length_t nv_len, char *nv_data,
                           rvm_offset_t *nv_offset, rec_hdr_t *rec_hdr,
                           char *msg)
{
    rvm_length_t last_chk_addr;
    rvm_length_t last_nv_addr;
    rvm_length_t i;

    /* check monitored ranges for specified range */
    for (i = 0; i < rvm_chk_len; i++) {
        if (rvm_chk_sigint != NULL)
            if ((*rvm_chk_sigint)(NULL))
                return; /* test for interrupt */

        last_chk_addr = (rvm_length_t)RVM_ADD_LENGTH_TO_ADDR(
            rvm_chk_vec[i].vmaddr, rvm_chk_vec[i].length);
        last_nv_addr = (rvm_length_t)RVM_ADD_LENGTH_TO_ADDR(nv_addr, nv_len);

        if ((((rvm_length_t)rvm_chk_vec[i].vmaddr >= (rvm_length_t)nv_addr) &&
             ((rvm_length_t)rvm_chk_vec[i].vmaddr < last_nv_addr)) ||
            ((last_chk_addr > (rvm_length_t)nv_addr) &&
             (last_chk_addr < last_nv_addr))) {
            /* found modification, call print support */
            if (nv_data != NULL) /* check bytes offset */
                nv_data = RVM_ADD_LENGTH_TO_ADDR(nv_data, BYTE_SKEW(nv_addr));
            (*rvm_monitor)((rvm_length_t)nv_addr, nv_len, nv_data, nv_offset,
                           rec_hdr, i, msg);
        }
    }

    return;
}

char *tst_buf; /* debug temp */

/* allocate log recovery buffers */
rvm_return_t alloc_log_buf(log_t *log /* log descriptor */)
{
    log_buf_t *log_buf = &log->log_buf; /* log buffer descriptor */

    if ((log_buf->buf = page_alloc(log_buf->length)) == NULL)
        return RVM_ENO_MEMORY;
#ifdef SPECIAL_DEBUG
    if ((log_buf->shadow_buf = page_alloc(log_buf->length)) == NULL)
        return RVM_ENO_MEMORY;
    if ((tst_buf = page_alloc(log_buf->length)) == NULL)
        return RVM_ENO_MEMORY;
#endif /* SPECIAL_DEBUG */
    log_buf->buf_len = RVM_MK_OFFSET(0, log_buf->length);

    if ((log_buf->aux_buf = page_alloc(log_buf->aux_length)) == NULL)
        return RVM_ENO_MEMORY;

    /* write-protect the buffers */
    /* I've taken out the mach-specific code, but it might be interesting to
     * implement this feature on other systems using mprotect. Therefore I've
     * `retained' the essence of the original code in this comment -- JH
     *
     * MACH_RVM_PROTECT
     *
     * protect(log_buf->buf,        log_buf->length,     FALSE, VM_PROT_READ);
     *
     * #ifdef SPECIAL_DEBUG
     * protect(log_buf->shadow_buf, log_buf->length,     FALSE, VM_PROT_READ);
     * protect(tst_buf,             log_buf->length,     FALSE, VM_PROT_READ);
     * #endif SPECIAL_DEBUG
     *
     * protect(log_buf->aux_buf,    log_buf->aux_length, FALSE, VM_PROT_READ);
     */

    return RVM_SUCCESS;
}

/* free log recovery buffer */
void free_log_buf(log_t *log /* log descriptor */)
{
    log_buf_t *log_buf = &log->log_buf; /* log buffer descriptor */

    if (log_buf->buf != NULL) {
        page_free(log_buf->buf, log_buf->length);
        log_buf->buf    = NULL;
        log_buf->length = 0;
        RVM_ZERO_OFFSET(log_buf->buf_len);
        log_buf->ptr = -1;
    }

    if (log_buf->aux_buf != NULL) {
        page_free(log_buf->aux_buf, log_buf->aux_length);
        log_buf->aux_buf    = NULL;
        log_buf->aux_length = 0;
    }
}

/* init log buffer with desired offset data from log */
rvm_return_t
init_buffer(log_t *log /* log descriptor */,
            rvm_offset_t *offset /* offset in log to load */,
            rvm_bool_t direction /* true ==> forward */,
            rvm_bool_t synch /* true ==> synchronization required */)
{
    log_buf_t *log_buf = &log->log_buf; /* log buffer descriptor */
    rvm_length_t length; /* length of buffer */
    rvm_offset_t read_len; /* read length calculation temp */
    rvm_return_t retval = RVM_SUCCESS; /* return value */

    assert(RVM_OFFSET_GEQ(*offset, log->status.log_start));
    assert(RVM_OFFSET_LEQ(*offset, log->dev.num_bytes));
    assert(log->trunc_thread == cthread_self());

    /* calculate buffer read length and ptr */
    log_buf->ptr = OFFSET_TO_SECTOR_INDEX(*offset);
    if (direction == FORWARD) { /* forward */
        log_buf->offset = CHOP_OFFSET_TO_SECTOR_SIZE(*offset);
        if (RVM_OFFSET_EQL(log_buf->offset, log->dev.num_bytes))
            read_len = log->status.log_size;
        else
            read_len = RVM_SUB_OFFSETS(log->dev.num_bytes, log_buf->offset);
    } else { /* reverse */
        log_buf->offset = ROUND_OFFSET_TO_SECTOR_SIZE(*offset);
        if (RVM_OFFSET_EQL(log_buf->offset, log->status.log_start))
            log_buf->offset = log->dev.num_bytes;
        if (RVM_OFFSET_EQL(log_buf->offset, log->dev.num_bytes))
            read_len = log->status.log_size;
        else
            read_len = RVM_SUB_OFFSETS(log_buf->offset, log->status.log_start);
    }

    /* get actual length to read */
    if (RVM_OFFSET_GTR(read_len, log_buf->buf_len))
        length = log_buf->length;
    else
        length = RVM_OFFSET_TO_LENGTH(read_len);
    /* set offset of read for reverse fill */
    if (direction == REVERSE) {
        log_buf->offset = RVM_SUB_LENGTH_FROM_OFFSET(log_buf->offset, length);
        if (log_buf->ptr == 0)
            log_buf->ptr = length;
        else
            log_buf->ptr += (length - SECTOR_SIZE);
    }

    /* lock device & allow swap if necessary */
    if (synch) {
        if (!rvm_no_yield)
            cthread_yield();
        assert(log->trunc_thread == cthread_self());
        mutex_lock(&log->dev_lock); /* begin dev_lock crit sec */
        assert(log->trunc_thread == cthread_self());
    }

    /* allow write to buffer */
    /* MACH_RVM_PROTECT
     *
     * protect(log_buf->buf, log_buf->length, FALSE,
     *         VM_PROT_WRITE | VM_PROT_READ);
     */

    /* read data from log device */
    if ((log_buf->r_length =
             read_dev(&log->dev, &log_buf->offset, log_buf->buf, length)) < 0) {
        retval            = RVM_EIO; /* i/o error */
        log_buf->r_length = 0; /* buffer invalid */
    }
    assert(log->trunc_thread == cthread_self());

    /* write protect buffer & unlock */
    /* MACH_RVM_PROTECT
     *
     * protect(log_buf->buf, log_buf->length, FALSE, VM_PROT_READ);
     *
     * #ifdef SPECIAL_DEBUG
     * / * re-read into shadow buffer & compare * /
     * if (rvm_shadow_buf)
     * {
     *     ret = vm_protect(task_self_,(vm_address_t)(log_buf->shadow_buf),
     *                      (vm_size_t)(log_buf->length),FALSE,
     *                      VM_PROT_WRITE | VM_PROT_READ);
     *     assert(ret == KERN_SUCCESS);
     *     if ((r_length=read_dev(&log->dev,&log_buf->offset,
     *                            log_buf->shadow_buf,length)) < 0)
     *     {
     *         retval = RVM_EIO;               / * i/o error * /
     *         assert(rvm_false);
     *     }
     *     assert(r_length == length);
     *     assert(r_length == log_buf->r_length);
     *     ret = vm_protect(task_self_,(vm_address_t)(log_buf->shadow_buf),
     *                      (vm_size_t)(log_buf->length),FALSE,VM_PROT_READ);
     *     assert(ret == KERN_SUCCESS);
     *     assert(memcmp(log_buf->buf,log_buf->shadow_buf,length) == 0);
     * }
     * #endif SPECIAL_DEBUG
     */

    if (synch)
        mutex_unlock(&log->dev_lock); /* end dev_lock crit sec */
    assert(log->trunc_thread == cthread_self());

    return retval;
}

/* refill buffer in scan direction */
static rvm_return_t
refill_buffer(log_t *log /* log descriptor */,
              rvm_bool_t direction /* true ==> forward */,
              rvm_bool_t synch /* true ==> synchronization required */)
{
    log_buf_t *log_buf = &log->log_buf; /* log buffer descriptor */
    rvm_offset_t offset; /* new buffer offset temp */

    /* compute new offset for buffer fill */
    offset = RVM_ADD_LENGTH_TO_OFFSET(log_buf->offset, log_buf->ptr);

    /* fill the buffer */
    return init_buffer(log, &offset, direction, synch);
}

/* compare buf & shadow buf from gdb */
#ifdef DEBUG_GDB
int log_buf_cmp(int disp)
{
    log_buf_t *log_buf = &default_log->log_buf;
    int i;

    if (disp < 0)
        disp = 0;
    for (i = disp; i < log_buf->r_length; i++)
        if (log_buf->buf[i] != log_buf->shadow_buf[i])
            return i;

    return -1;
}

/* compare with disk */
int disk_buf_cmp(char *buf, int disp)
{
    log_buf_t *log_buf = &default_log->log_buf;
    int i;
    int r_length;

    /* allow write to buffer */
    /* MACH_RVM_PROTECT
     *
     * protect(log_buf->buf, log_buf->length, FALSE, VM_PROT_WRITE | VM_PROT_READ);
     */

    /* read buffer from log */
    if ((r_length = read_dev(&default_log->dev, &log_buf->offset, tst_buf,
                             log_buf->r_length)) < 0)
        assert(rvm_false); /* i/o error */
    assert(r_length == log_buf->r_length);

    /* re-protect buffer */
    /* MACH_RVM_PROTECT
     *
     * protect(log_buf->buf, log_buf->length, FALSE, VM_PROT_READ);
     */

    /* compare results */
    if (disp < 0)
        disp = 0;
    for (i = disp; i < log_buf->r_length; i++)
        if (buf[i] != tst_buf[i])
            return i;

    return -1;
}

/* locate byte in buffer via gdb */
int find_byte(char chr, char *buf, int disp, int max_len)
{
    int i;

    if (disp < 0)
        disp = 0;
    for (i = disp; i < max_len; i++)
        if (chr == buf[i])
            return i;

    return -1;
}

/* locate word in buffer via gdb */
int find_word(rvm_length_t wrd, rvm_length_t *buf, int disp, int max_len)
{
    int i;

    if (disp < 0)
        disp = 0;
    for (i = disp / sizeof(rvm_length_t); i < max_len / sizeof(rvm_length_t);
         i++)
        if (wrd == buf[i])
            return i;

    return -1;
}

/* find word in log buffer via gdb */
int find_buf_word(rvm_length_t wrd, int disp)
{
    log_buf_t *log_buf = &default_log->log_buf;

    return find_word(wrd, (rvm_length_t *)log_buf->buf, disp,
                     log_buf->r_length);
}
#endif /* DEBUG_GDB */

/* load log auxiliary buffer */
rvm_return_t
load_aux_buf(log_t *log /* log descriptor */,
             rvm_offset_t *log_offset /* buffer read offset */,
             rvm_length_t length /* data length wanted */,
             rvm_length_t *aux_ptr /* ptr to aux. buf offset */,
             rvm_length_t *data_len /* ptr to actual data length read */,
             rvm_bool_t synch /* true ==> synchronization required */,
             rvm_bool_t pre_load /* permit pre-loading of range */)
{
    log_buf_t *log_buf = &log->log_buf; /* log buffer descriptor */
    rvm_offset_t high_offset; /* end of read area */
    rvm_length_t read_len; /* buffer read length */
    rvm_return_t retval = RVM_SUCCESS;

    assert(log->trunc_thread == cthread_self());

    /* check offset */
    if (RVM_OFFSET_GTR(*log_offset, log->dev.num_bytes)) {
        *aux_ptr = -1; /* out of bounds -- partial record */
        return RVM_SUCCESS;
    }

    /* see if request is already in buffer */
    high_offset =
        RVM_ADD_LENGTH_TO_OFFSET(log_buf->aux_offset, log_buf->aux_rlength);
    if ((RVM_OFFSET_GEQ(*log_offset, log_buf->aux_offset)) &&
        (RVM_OFFSET_LSS(*log_offset, high_offset))) {
        /* yes, have at least some of the data so report how much */
        *aux_ptr = RVM_OFFSET_TO_LENGTH(
            RVM_SUB_OFFSETS(*log_offset, log_buf->aux_offset));
        read_len =
            RVM_OFFSET_TO_LENGTH(RVM_SUB_OFFSETS(high_offset, *log_offset));
        if (read_len < length)
            *data_len = read_len;
        else
            *data_len = length;
        return RVM_SUCCESS;
    }

    /* if less than sector requested, see if pre-load permitted */
    if (pre_load && (length < SECTOR_SIZE))
        read_len = log_buf->aux_length; /* yes, fill buffer */
    else
        read_len = length; /* no, just do what requested */
    /* determine length and offset for log read */
    log_buf->aux_offset = CHOP_OFFSET_TO_SECTOR_SIZE(*log_offset);
    high_offset         = RVM_ADD_LENGTH_TO_OFFSET(*log_offset, read_len);
    high_offset         = ROUND_OFFSET_TO_SECTOR_SIZE(high_offset);
    if (RVM_OFFSET_GTR(high_offset, log->dev.num_bytes))
        high_offset = log->dev.num_bytes; /* don't read past end of log */

    /* report actual length read and ptr into buffer */
    read_len =
        RVM_OFFSET_TO_LENGTH(RVM_SUB_OFFSETS(high_offset, log_buf->aux_offset));
    *aux_ptr = OFFSET_TO_SECTOR_INDEX(*log_offset);
    if (read_len > log_buf->aux_length) {
        if ((read_len >= length) &&
            (length <= (log_buf->aux_length - SECTOR_SIZE)))
            *data_len = length;
        else
            *data_len = log_buf->aux_length - *aux_ptr;
        read_len = log_buf->aux_length;
    } else
        *data_len = length;

    /* lock device and allow swap if necessary */
    if (synch) {
        if (!rvm_no_yield)
            cthread_yield(); /* allow swap now */
        assert(log->trunc_thread == cthread_self());
        mutex_lock(&log->dev_lock); /* begin dev_lock crit sec */
        assert(log->trunc_thread == cthread_self());
    }

    /* allow write to buffer */
    /* MACH_RVM_PROTECT
     *
     * protect(log_buf->aux_buf, log_buf->aux_length, FALSE,
     *         VM_PROT_WRITE | VM_PROT_READ);
     */

    /* read new value data from log */
    if ((log_buf->aux_rlength = read_dev(&log->dev, &log_buf->aux_offset,
                                         log_buf->aux_buf, read_len)) < 0) {
        retval               = RVM_EIO;
        log_buf->aux_rlength = 0;
    }
    assert(log->trunc_thread == cthread_self());

    /* write protect buffer & unlock */
    /* MACH_RVM_PROTECT
     *
     * protect(log_buf->aux_buf, log_buf->aux_length, FALSE, VM_PROT_READ);
     */

    if (synch)
        mutex_unlock(&log->dev_lock); /* end dev_lock crit sec */
    assert(log->trunc_thread == cthread_self());

    return retval;
}

void clear_aux_buf(log_t *log /* log descriptor */)
{
    log_buf_t *log_buf = &log->log_buf; /* log buffer descriptor */

    RVM_ZERO_OFFSET(log_buf->aux_offset);
    log_buf->aux_rlength = 0;
}

/* record header type validation */
static rvm_bool_t chk_hdr_type(rec_hdr_t *rec_hdr /* generic record header */)
{
    switch (rec_hdr->struct_id) {
    case trans_hdr_id: /* transaction header */
        return rvm_true;
    case log_seg_id: /* log segment dictionary entry */
        return rvm_true;
    case log_wrap_id: /* log wrap-aound marker */
        return rvm_true;
    default: /* unknown header type */
        return rvm_false;
    }
}

/* test if record belongs to currently valid part of log */
rvm_bool_t chk_hdr_currency(log_t *log /* log descriptor */,
                            rec_hdr_t *rec_hdr /* generic record header */)
{
    log_status_t *status = &log->status; /* status descriptor */

    /* be sure record number makes sense */
    if ((status->first_rec_num != 0) &&
        (rec_hdr->rec_num < status->first_rec_num))
        return rvm_false; /* obsolete record */

    /* be sure record written after previous truncation & before this one */
    if (TIME_LSS(rec_hdr->timestamp, status->prev_trunc) ||
        TIME_GTR(rec_hdr->timestamp, status->last_trunc))
        return rvm_false; /* obsolete record */

    return rvm_true;
}

void reset_hdr_chks(log_t *log /* log descriptor */)
{
    log_buf_t *log_buf = &log->log_buf; /* log buffer descriptor */

    log_buf->prev_rec_num = 0;
    ZERO_TIME(log_buf->prev_timestamp);
}

/* test if record is out of sequence in log */
rvm_bool_t chk_hdr_sequence(log_t *log /* log descriptor */,
                            rec_hdr_t *rec_hdr /* generic record header */,
                            rvm_bool_t direction /* scan direction */)
{
    log_buf_t *log_buf = &log->log_buf; /* recovery buffer descriptor */

    /* check record number closely */
    if ((log_buf->prev_rec_num != 0) &&
        (((direction == FORWARD) &&
          (rec_hdr->rec_num != log_buf->prev_rec_num + 1)) ||
         ((direction == REVERSE) &&
          (rec_hdr->rec_num != log_buf->prev_rec_num - 1))))
        return rvm_false; /* sequence error */

    /* check record write time closely */
    if ((!TIME_EQL_ZERO(log_buf->prev_timestamp)) &&
        (((direction == FORWARD) &&
          TIME_LSS(rec_hdr->timestamp, log_buf->prev_timestamp)) ||
         ((direction == REVERSE) &&
          TIME_GTR(rec_hdr->timestamp, log_buf->prev_timestamp))))
        return rvm_false; /* sequence error */

    return rvm_true;
}

/* record header validation */
static rvm_bool_t chk_hdr(log_t *log /* log descriptor */,
                          rec_hdr_t *rec_hdr /* generic record header */,
                          rec_end_t *rec_end /* generic record end marker */,
                          rvm_bool_t direction /* scan direction */)
{
    /* be sure record type valid */
    if (!chk_hdr_type(rec_hdr))
        return rvm_false;

    /* checks for normal operation only */
    if (!rvm_utlsw) {
        /* make sure record current */
        if (chk_hdr_currency(log, rec_hdr) != rvm_true)
            return rvm_false; /* record obsolete */

        /* make sure record in proper sequence */
        if (chk_hdr_sequence(log, rec_hdr, direction) != rvm_true)
            return rvm_false; /* sequence error */
    }

    /* generic record head/end validation */
    if ((rec_end != NULL) &&
        ((rec_end->rec_hdr.struct_id != rec_end_id) ||
         (rec_hdr->struct_id != rec_end->rec_type) ||
         (rec_hdr->rec_num != rec_end->rec_hdr.rec_num) ||
         (rec_hdr->rec_length != rec_end->rec_hdr.rec_length) ||
         (!TIME_EQL(rec_hdr->timestamp, rec_end->rec_hdr.timestamp))))
        return rvm_false;

    return rvm_true;
}

/* log record header validation */
rvm_bool_t validate_hdr(log_t *log /* log descriptor */,
                        rec_hdr_t *rec_hdr /* generic record header */,
                        rec_end_t *rec_end /* generic record end marker */,
                        rvm_bool_t direction /* scan direction */)
{
    log_buf_t *log_buf = &log->log_buf; /* recovery buffer descriptor */

    /* clear sequence checking hide-a-ways if direction reversed */
    if (direction != log_buf->prev_direction)
        reset_hdr_chks(log);

    /* do basic record header checks */
    if (!chk_hdr(log, rec_hdr, rec_end, direction))
        return rvm_false; /* header invalid */

    /* type-specific validation */
    switch (rec_hdr->struct_id) {
    case trans_hdr_id: /* transaction header */
        break;
    case log_seg_id: /* log segment dictionary entry */
        break;
    case log_wrap_id: /* log wrap-aound marker */
        goto exit;
    default: /* unknown/improper header type */
        return rvm_false;
    }

    /* update buffer ptr and previous record state */
    if (direction == FORWARD) /* forward, return header position */
        log_buf->ptr = (long)rec_hdr - (long)log_buf->buf;
    else /* reverse, return end marker pos. */
        log_buf->ptr = (long)rec_end - (long)log_buf->buf;

exit:
    log_buf->prev_rec_num   = rec_hdr->rec_num;
    log_buf->prev_timestamp = rec_hdr->timestamp;
    log_buf->prev_direction = direction;

    return rvm_true;
}

/* get next new value range by forward scan of transaction record
   ptr points to next range header
   exits with as much of range in buffer as will fit */
rvm_return_t
scan_nv_forward(log_t *log /* log descriptor */,
                rvm_bool_t synch /* true ==> synchronization required */)
{
    log_buf_t *log_buf = &log->log_buf; /* log buffer descriptor */
    rvm_offset_t offset; /* offset calculation temp */
    rec_hdr_t *rec_hdr; /* temporary cast for record header */
    rvm_return_t retval; /* return value */

    /* see if new header is entirely within buffer */
    if ((log_buf->ptr + sizeof(rec_hdr_t)) >= log_buf->r_length) {
        /* no, refill buffer */
        offset = RVM_ADD_LENGTH_TO_OFFSET(log_buf->offset, log_buf->ptr);
        if ((retval = init_buffer(log, &offset, FORWARD, synch)) != RVM_SUCCESS)
            return retval;
    }

    /* check header */
    rec_hdr = (rec_hdr_t *)&log_buf->buf[log_buf->ptr];
    switch (rec_hdr->struct_id) {
    case nv_range_id:
        break;
    case rec_end_id:
        return RVM_SUCCESS;

    default:
        return RVM_SUCCESS; /* need better reporting */
    }

    /* get whole range in buffer */
    if ((log_buf->ptr + rec_hdr->rec_length) > log_buf->r_length) {
        if ((retval = refill_buffer(log, FORWARD, synch)) != RVM_SUCCESS)
            return retval;
    }

    return RVM_SUCCESS;
}

/* get previous new value range by reverse scan of transaction record
   ptr points to previous range header; exits with range in buffer */
static rvm_return_t
scan_nv_reverse(log_t *log /* log descriptor */,
                rvm_bool_t synch /* true ==> synchronization required */)
{
    log_buf_t *log_buf = &log->log_buf; /* log buffer descriptor */
    rec_hdr_t *rec_hdr; /* temporary cast for record header */
    long len = 0; /* back displacement to prev. hdr */
    rvm_offset_t offset; /* offset calculation temp */
    rvm_return_t retval; /* return value */

    /* get new header position */
    rec_hdr = (rec_hdr_t *)&log_buf->buf[log_buf->ptr];
    switch (rec_hdr->struct_id) {
    case rec_end_id:
        len = ((rec_end_t *)rec_hdr)->sub_rec_len;
        break;

    case nv_range_id:
        len = ((nv_range_t *)rec_hdr)->sub_rec_len;
        break;

    default:
        assert(rvm_false); /* trouble -- log damage? */
    }

    /* see if new header is entirely within buffer */
    if ((log_buf->ptr - len) < 0) {
        /* no, refill buffer according to length of data */
        if ((len - sizeof(nv_range_t)) <=
            NV_LOCAL_MAX) { /* small, get data into buffer */
            if ((retval = refill_buffer(log, REVERSE, synch)) != RVM_SUCCESS)
                return retval;
            log_buf->ptr -= len;
        } else { /* large, skip data for now */
            offset = RVM_ADD_LENGTH_TO_OFFSET(
                log_buf->offset, (log_buf->ptr + sizeof(nv_range_t)));
            offset = RVM_SUB_LENGTH_FROM_OFFSET(offset, len);
            retval = init_buffer(log, &offset, REVERSE, synch);
            if (retval != RVM_SUCCESS)
                return retval;
            log_buf->ptr -= sizeof(nv_range_t);
        }
    } else
        log_buf->ptr -= len;
    /* exit pointing to new header */
    rec_hdr = (rec_hdr_t *)&log_buf->buf[log_buf->ptr];
    if (rec_hdr->struct_id == trans_hdr_id)
        return RVM_SUCCESS;
    assert(rec_hdr->struct_id == nv_range_id);

    return RVM_SUCCESS;
}

/* validate record in buffer in forward scan */
static rvm_return_t
validate_rec_forward(log_t *log /* log descriptor */,
                     rvm_bool_t synch /* true ==> synchronization required */)
{
    log_buf_t *log_buf = &log->log_buf; /* log buffer descriptor */
    rec_hdr_t *rec_hdr; /* temporary cast for next record hdr */
    rec_end_t *rec_end = NULL; /* temporary cast for record end */
    rvm_offset_t end_offset; /* temporary for caluculating end */
    rvm_return_t retval;
    rvm_length_t tmp_ptr;
    rvm_length_t tmp_len;

    /* see if next header is entirely within buffer */
    if ((log_buf->ptr + MAX_HDR_SIZE) > log_buf->r_length) {
        /* no, re-init buffer */
        end_offset = RVM_ADD_LENGTH_TO_OFFSET(log_buf->offset, log_buf->ptr);
        retval     = init_buffer(log, &end_offset, FORWARD, synch);
        if (retval != RVM_SUCCESS)
            return retval;
    }

    /* check header type */
    rec_hdr = (rec_hdr_t *)&log_buf->buf[log_buf->ptr];
    if (rec_hdr->struct_id == log_wrap_id)
        goto validate; /* skip rec_end stuff for wrap */
    if (!chk_hdr(log, rec_hdr, NULL, FORWARD))
        goto no_record; /* no next record */

    /* see if record will fit in buffer */
    if ((ROUND_TO_SECTOR_SIZE(rec_hdr->rec_length + sizeof(rec_end_t)) +
         SECTOR_SIZE) <= log_buf->length) {
        /* yes, get whole record in buffer */
        if ((log_buf->ptr + rec_hdr->rec_length + sizeof(rec_end_t)) >
            log_buf->length) {
            /* refill buffer */
            if ((retval = refill_buffer(log, FORWARD, synch)) != RVM_SUCCESS)
                return retval;
            rec_hdr = (rec_hdr_t *)&log_buf->buf[log_buf->ptr];
        }
        tmp_ptr = log_buf->ptr + rec_hdr->rec_length;
        rec_end = (rec_end_t *)&log_buf->buf[tmp_ptr];
    } else {
        /* no, won't fit -- read rec_end into aux buffer for validation */
        end_offset = RVM_ADD_LENGTH_TO_OFFSET(
            log_buf->offset, log_buf->ptr + rec_hdr->rec_length);

        /* check offset alignment to see if rec_hdr is trash */
        tmp_ptr = RVM_OFFSET_TO_LENGTH(end_offset);
        if (tmp_ptr != CHOP_TO_LENGTH(tmp_ptr))
            goto no_record; /* end marker alignment wrong */
        retval = load_aux_buf(log, &end_offset, sizeof(rec_end_t), &tmp_ptr,
                              &tmp_len, synch, rvm_false);
        if (retval != RVM_SUCCESS)
            return retval;
        if (tmp_ptr == -1)
            goto no_record; /* record end not available */
        rec_end = (rec_end_t *)&log_buf->aux_buf[tmp_ptr];
    }

    /* validate whole record now that end is available */
validate:
    if (validate_hdr(log, rec_hdr, rec_end, FORWARD))
        return RVM_SUCCESS;

no_record: /* no next record */
    log_buf->ptr = -1;
    return RVM_SUCCESS;
}

/* scan forward from present position at a record structure
   returns updated offset indexed by ptr; -1 ==> no next rec. */
rvm_return_t
scan_forward(log_t *log /* log descriptor */,
             rvm_bool_t synch /* true ==> synchronization required */)
{
    log_buf_t *log_buf = &log->log_buf; /* log buffer descriptor */
    rec_hdr_t *rec_hdr; /* cast for next record hdr */
    rvm_return_t retval;

    assert(log_buf->ptr != -1); /* invalid position */
    rec_hdr = (rec_hdr_t *)&log_buf->buf[log_buf->ptr];
    switch (rec_hdr->struct_id) {
    case trans_hdr_id:
    case log_seg_id:
        log_buf->ptr += (rec_hdr->rec_length + sizeof(rec_end_t));
        break;
    case rec_end_id:
        log_buf->ptr += sizeof(rec_end_t);
        break;
    case nv_range_id: /* scan past remaining ranges */
        DO_FOREVER
        {
            if ((retval = scan_nv_forward(log, synch)) != RVM_SUCCESS)
                return retval;
            rec_hdr = (rec_hdr_t *)&log_buf->buf[log_buf->ptr];
            switch (rec_hdr->struct_id) {
            case nv_range_id:
                log_buf->ptr += rec_hdr->rec_length;
                break;
            case rec_end_id:
                log_buf->ptr += sizeof(rec_end_t);
                goto trans_done;
            default: /* validate_rec_forward will handle */
                goto trans_done;
            }
        }
    trans_done:
        break;
    case log_wrap_id:
        retval = init_buffer(log, &log->status.log_start, FORWARD, synch);
        if (retval != RVM_SUCCESS)
            return retval;
        break;
    default:
        if (rvm_utlsw) {
            log_buf->ptr = -1; /* utility can handle unknown records */
            return RVM_SUCCESS;
        }
        assert(rvm_false); /* unknown record type */
    }

    /* validate next record */
    return validate_rec_forward(log, synch);
}

/* scan for wrap marker */
rvm_return_t
scan_wrap_reverse(log_t *log /* log descriptor */,
                  rvm_bool_t synch /* true ==> synchronization required */)
{
    log_buf_t *log_buf = &log->log_buf; /* log buffer descriptor */
    rec_hdr_t *rec_hdr; /* temporary cast for record header */
    log_wrap_t *log_wrap; /* temporary cast for wrap marker */
    long tmp_ptr; /* temporary buffer ptr */
    rvm_return_t retval;

    /* load last sectors of log */
    retval = init_buffer(log, &log->dev.num_bytes, REVERSE, synch);
    if (retval != RVM_SUCCESS)
        return retval;

    /* scan for wrap marker */
    /* for the purpose of locating the wrap marker, we use the (duplicated)
       struct_id2 which, while positions at the end of the record, guarantees
       that we must interpret it first, otherwise, we may possibly
       mis-interpret other field of the record to have a struct_id of
       log_wrap_id ! */
    for (tmp_ptr = (log_buf->ptr - sizeof(log_wrap_t)); tmp_ptr >= 0;
         tmp_ptr -= sizeof(rvm_length_t)) {
        log_wrap = (log_wrap_t *)&log_buf->buf[tmp_ptr];
        if (log_wrap->struct_id2 == log_wrap_id) {
            assert((log_wrap->rec_hdr.struct_id == log_wrap_id) || rvm_utlsw);
            /* XXXX fix this */
#if 0
            if (!((log_wrap->struct_id == log_wrap_id) || rvm_utlsw)) {
                printf("not true!\n");
                assert(0);
            }
#endif
            break;
        }
    }

    /* validate header if tmp_ptr legit */
    if ((tmp_ptr >= 0) && (tmp_ptr < log_buf->r_length)) {
        log_buf->ptr = tmp_ptr;
        rec_hdr      = (rec_hdr_t *)&log_buf->buf[log_buf->ptr];
        if (!validate_hdr(log, rec_hdr, NULL, REVERSE))
            log_buf->ptr = -1;
    } else
        /* no wrap marker found */
        if (rvm_utlsw)
            log_buf->ptr = -1; /* utility can deal with it */
        else
            assert(rvm_false);

    return RVM_SUCCESS;
}

/* validate current record in buffer in reverse scan */
rvm_return_t
validate_rec_reverse(log_t *log /* log descriptor */,
                     rvm_bool_t synch /* true ==> synchronization required */)
{
    log_buf_t *log_buf   = &log->log_buf; /* log buffer descriptor */
    log_status_t *status = &log->status; /* status area */
    rec_end_t *rec_end   = NULL; /* temporary cast for record end */
    rec_hdr_t *rec_hdr; /* temporary cast for record header */
    rvm_length_t tmp_ptr; /* temporary buffer ptr */
    rvm_length_t tmp_len;
    rvm_offset_t offset; /* temp for offset calculations */
    rvm_return_t retval;

    /* get previous end marker into buffer */
    if ((long)(log_buf->ptr - sizeof(rec_end_t)) < 0) {
        offset = RVM_ADD_LENGTH_TO_OFFSET(log_buf->offset, log_buf->ptr);
        if (RVM_OFFSET_EQL(offset, status->log_start)) {
            retval = scan_wrap_reverse(log, synch);
            return retval; /* exit pointing to wrap marker */
        } else {
            retval = init_buffer(log, &offset, REVERSE, synch);
            if (retval != RVM_SUCCESS)
                return retval;
        }
    }
    log_buf->ptr -= sizeof(rec_end_t);

    /* check new end marker */
    rec_end = (rec_end_t *)&log_buf->buf[log_buf->ptr];
    if (rec_end->rec_hdr.struct_id != rec_end_id)
        goto no_record; /* no next record */
    /* see if record will fit in buffer */
    if ((ROUND_TO_SECTOR_SIZE(rec_end->rec_hdr.rec_length + sizeof(rec_end_t)) +
         SECTOR_SIZE) <= log_buf->length) {
        /* yes, get whole record in buffer */
        if ((long)(log_buf->ptr - rec_end->rec_hdr.rec_length) < 0) {
            /* refill buffer (be sure end marker is included) */
            log_buf->ptr += sizeof(rec_end_t);
            if ((retval = refill_buffer(log, REVERSE, synch)) != RVM_SUCCESS)
                return retval;
            log_buf->ptr -= sizeof(rec_end_t);
            rec_end = (rec_end_t *)&log_buf->buf[log_buf->ptr];
        }
        tmp_ptr = log_buf->ptr - rec_end->rec_hdr.rec_length;
        rec_hdr = (rec_hdr_t *)&log_buf->buf[tmp_ptr];
    } else {
        /* no, save rec_end for validation & get header in aux. buffer */
        offset = RVM_SUB_LENGTH_FROM_OFFSET(log_buf->offset,
                                            rec_end->rec_hdr.rec_length);
        offset = RVM_ADD_LENGTH_TO_OFFSET(offset, log_buf->ptr);

        /* check offset alignment to see if rec_end is trash */
        tmp_ptr = RVM_OFFSET_TO_LENGTH(offset);
        if (tmp_ptr != CHOP_TO_LENGTH(tmp_ptr))
            goto no_record; /* header alignment wrong */
        retval = load_aux_buf(log, &offset, MAX_HDR_SIZE, &tmp_ptr, &tmp_len,
                              synch, rvm_false);
        if (retval != RVM_SUCCESS)
            return retval;
        if (tmp_ptr == -1)
            goto no_record; /* record header not available */
        rec_hdr = (rec_hdr_t *)&log_buf->aux_buf[tmp_ptr];
    }

    /* validate whole record now that header is available */
    if (validate_hdr(log, rec_hdr, rec_end, REVERSE))
        return RVM_SUCCESS;

no_record:
    log_buf->ptr = -1; /* no next record */
    return RVM_SUCCESS;
}

/* scan backward from present position at a record structure
   returns index of offset in ptr; -1 ==> no next rec. */
rvm_return_t
scan_reverse(log_t *log /* log descriptor */,
             rvm_bool_t synch /* true ==> synchronization required */)
{
    log_buf_t *log_buf   = &log->log_buf; /* log buffer descriptor */
    log_status_t *status = &log->status; /* status area */
    rec_hdr_t *rec_hdr; /* temporary cast for record header */
    rvm_offset_t offset; /* temp for offset calculations */
    rvm_return_t retval;

    assert(log_buf->ptr != -1); /* can't reposition from this! */

    /* test if scan starting from tail */
    offset = RVM_ADD_LENGTH_TO_OFFSET(log_buf->offset, log_buf->ptr);
    if (RVM_OFFSET_EQL(offset, status->prev_log_tail) ||
        (rvm_utlsw && RVM_OFFSET_EQL(offset, status->log_tail)))
        return validate_rec_reverse(log, synch);

    /* test if at start of log & must wrap around */
    if ((RVM_OFFSET_EQL(log_buf->offset, status->log_start)) &&
        (log_buf->ptr == 0)) {
        if ((retval = scan_wrap_reverse(log, synch)) != RVM_SUCCESS)
            return retval;
        return RVM_SUCCESS; /* exit pointing to wrap marker */
    }

    /* move to previous record end marker */
    rec_hdr = (rec_hdr_t *)&log_buf->buf[log_buf->ptr];
    switch (rec_hdr->struct_id) {
    case trans_hdr_id:
    case log_seg_id:
    case log_wrap_id:
        break;
    case rec_end_id:
        if (((rec_end_t *)rec_hdr)->rec_type !=
            trans_hdr_id) { /* record is always in buffer */
            log_buf->ptr -= rec_hdr->rec_length;
            break;
        }
    case nv_range_id: /* scan past remaining ranges */
        DO_FOREVER
        {
            if ((retval = scan_nv_reverse(log, synch)) != RVM_SUCCESS)
                return retval;
            rec_hdr = (rec_hdr_t *)&log_buf->buf[log_buf->ptr];
            if (rec_hdr->struct_id == trans_hdr_id)
                break;
        }
        break;
    default: {
        if (rvm_utlsw) {
            log_buf->ptr = -1; /* utl can recover */
            return RVM_SUCCESS;
        }
        assert(rvm_false); /* not at recognizable point in log */
    }
    }

    /* validate new record and set log_buf->ptr */
    return validate_rec_reverse(log, synch);
}

/* Recovery: phase 1 -- locate current log tail from last status
 * block location */

/* log_wrap status update for tail location */
static void
set_wrap_status(log_status_t *status /* status descriptor */,
                rec_hdr_t *rec_hdr /* current record scanned in buffer */)
{
    status->wrap_time = rec_hdr->timestamp;
    status->n_special++;
    status->tot_wrap++;
}

/* range checksum computation & check */
static rvm_return_t
range_chk_sum(log_t *log /* log descriptor */,
              nv_range_t *nv /* range header */,
              rvm_bool_t *chk_val /* result [out] */,
              rvm_bool_t synch /* true ==> synchronization required */)
{
    log_buf_t *log_buf = &log->log_buf; /* log buffer descriptor */
    rvm_length_t nv_chk_sum; /* nv's check sum */
    rvm_length_t chk_sum_temp = 0; /* check sum temp */
    rvm_length_t nv_length; /* actual length of data */
    rvm_length_t chk_length; /* length of check summed range */
    rvm_length_t align_skew; /* initial alignment skew */
    rvm_return_t retval; /* return value */

    (*chk_val) = rvm_false;
    nv_chk_sum = nv->chk_sum;
    nv_length  = nv->length;
    align_skew = BYTE_SKEW(RVM_OFFSET_TO_LENGTH(nv->offset));
    log_buf->ptr += sizeof(nv_range_t);

    /* do checksum over as many buffer loads as needed */
    DO_FOREVER
    {
        chk_length = log_buf->r_length - log_buf->ptr - align_skew;
        if (chk_length > nv_length)
            chk_length = nv_length;
        chk_sum_temp +=
            chk_sum(&log_buf->buf[log_buf->ptr + align_skew], chk_length);
        nv_length -= chk_length;
        log_buf->ptr += (chk_length + align_skew);
        if (nv_length == 0)
            break; /* done */
        if ((retval = refill_buffer(log, FORWARD, synch)) != RVM_SUCCESS)
            return retval;
        align_skew = 0; /* following buffers have no padding */
    }
    log_buf->ptr = ROUND_TO_LENGTH(log_buf->ptr);

    /* report result */
    if (nv_chk_sum == chk_sum_temp)
        (*chk_val) = rvm_true;

    return RVM_SUCCESS;
}

/* transaction validation & status update for tail location */
static rvm_return_t
set_trans_status(log_t *log /* log descriptor */,
                 rec_hdr_t *rec_hdr /* current trans record in buffer */)
{
    log_buf_t *log_buf   = &log->log_buf; /* log buffer descriptor */
    log_status_t *status = &log->status; /* status descriptor */
    trans_hdr_t trans_hdr; /* copy of header */
    long num_ranges = 0; /* range scan counter */
    nv_range_t *nv; /* range header */
    rvm_bool_t chk_val; /* checksum test result */
    rvm_return_t retval; /* return value */

    /* keep copy of header to get status if ranges are OK */
    BCOPY((char *)rec_hdr, (char *)&trans_hdr, sizeof(trans_hdr_t));

    /* scan and check sum all ranges */
    log_buf->ptr += sizeof(trans_hdr_t);
    DO_FOREVER
    {
        if ((retval = scan_nv_forward(log, NO_SYNCH)) != RVM_SUCCESS)
            return retval;
        rec_hdr = (rec_hdr_t *)&(log_buf->buf[log_buf->ptr]);
        if (rec_hdr->struct_id == rec_end_id)
            break; /* done */
        if (rec_hdr->struct_id != nv_range_id)
            goto bad_record; /* invalid record */
        nv = (nv_range_t *)rec_hdr;
        if (trans_hdr.rec_hdr.rec_num != nv->rec_hdr.rec_num)
            goto bad_record; /* wrong transaction */

        /* test range's data check sum */
        retval = range_chk_sum(log, nv, &chk_val, NO_SYNCH);
        if (retval != RVM_SUCCESS)
            return retval;
        if (chk_val != rvm_true)
            goto bad_record; /* check sum failure */

        num_ranges++;
    }
    /* be sure all ranges are present */
    if (num_ranges != trans_hdr.num_ranges)
        goto bad_record; /* incomplete */

    /* transaction complete, update status */
    status->last_uname = trans_hdr.uname;
    if (trans_hdr.flags & FLUSH_FLAG)
        status->n_flush_commit++;
    else
        status->n_no_flush_commit++;
    if (((trans_hdr.flags & FIRST_ENTRY_FLAG) != 0) &&
        ((trans_hdr.flags & LAST_ENTRY_FLAG) == 0))
        status->n_split++;
    return RVM_SUCCESS;

bad_record:
    log_buf->ptr = -1;
    return RVM_SUCCESS;
}

/* Locate tail, update in-memory copy of status block; always reads forward */
rvm_return_t locate_tail(log_t *log /* log descriptor */)
{
    log_status_t *status = &log->status; /* status descriptor */
    log_buf_t *log_buf   = &log->log_buf; /* log buffer descriptor */
    rvm_offset_t tail; /* tail offset */
    rvm_offset_t temp_tail; /* tail offset temp */
    rvm_length_t last_rec_num = 0; /* record number of tail record */
    rec_hdr_t *rec_hdr; /* current record scanned in buffer */
    struct timeval save_last_trunc;
    struct timeval last_write = status->last_write; /* last write to log */
    rvm_bool_t save_rvm_utlsw = rvm_utlsw;
    rvm_return_t retval       = RVM_SUCCESS; /* return value */

    assert(log->trunc_thread == cthread_self());
    assert((status->trunc_state & RVM_TRUNC_PHASES) == ZERO);
    status->trunc_state |= RVM_TRUNC_FIND_TAIL;

    /* initialize scanner sequence checking state and buffers */
    rvm_utlsw = rvm_false;
    reset_hdr_chks(log);
    clear_aux_buf(log);

    /* if truncation caught in crash, reset head */
    if (!RVM_OFFSET_EQL_ZERO(status->prev_log_head)) {
        status->log_head     = status->prev_log_head;
        status->last_rec_num = status->next_rec_num - 1;
    }

    /* set temporary timestamp for record validation */
    save_last_trunc = status->last_trunc;
    make_uname(&status->last_trunc);
    if (TIME_GTR(save_last_trunc, status->last_trunc)) { /* date/time wrong! */
        retval     = RVM_EINTERNAL;
        rvm_errmsg = ERR_DATE_SKEW;
        goto err_exit;
    }

    /* need to update status: init read buffer at head */
    retval = init_buffer(log, &status->log_head, FORWARD, NO_SYNCH);
    if (retval != RVM_SUCCESS)
        goto err_exit;
    assert(log->trunc_thread == cthread_self());
    assert((status->trunc_state & RVM_TRUNC_PHASES) == RVM_TRUNC_FIND_TAIL);

    /* validate 1st record, none ==> log empty */
    rec_hdr = (rec_hdr_t *)&(log_buf->buf[log_buf->ptr]);
    if (!validate_hdr(log, rec_hdr, NULL, FORWARD)) {
#ifdef RVM_LOG_TAIL_BUG
        unprotect_page__Fi(ClobberAddress);
#endif /* RVM_LOG_TAIL_BUG */
#ifdef RVM_LOG_TAIL_SHADOW
        assert(RVM_OFFSET_EQL(log_tail_shadow, status->log_tail));
#endif /* RVM_LOG_TAIL_SHADOW */
        status->log_tail = status->log_head;
#ifdef RVM_LOG_TAIL_SHADOW
        RVM_ASSIGN_OFFSET(log_tail_shadow, status->log_tail);
#endif /* RVM_LOG_TAIL_SHADOW */
#ifdef RVM_LOG_TAIL_BUG
        protect_page__Fi(ClobberAddress);
#endif /* RVM_LOG_TAIL_BUG */
        clear_log_status(log);
        goto exit;
    }
    /* update status block head info if necessary */
    if (status->first_rec_num == 0)
        status->first_rec_num = rec_hdr->rec_num;
    if (TIME_EQL_ZERO(status->first_write))
        status->first_write = rec_hdr->timestamp;
    if (rec_hdr->struct_id == log_wrap_id)
        status->wrap_time = rec_hdr->timestamp;

    /* locate first transaction, if needed */
    if (TIME_EQL_ZERO(status->first_uname))
        do {
            /* update other status data */
            rec_hdr            = (rec_hdr_t *)&(log_buf->buf[log_buf->ptr]);
            last_rec_num       = rec_hdr->rec_num;
            status->last_write = rec_hdr->timestamp;
            if (rec_hdr->struct_id == log_wrap_id)
                status->wrap_time = rec_hdr->timestamp;

            if (rec_hdr->struct_id == trans_hdr_id) { /* transaction found */
                status->first_uname = ((trans_hdr_t *)rec_hdr)->uname;
                status->last_uname  = ((trans_hdr_t *)rec_hdr)->uname;
                break;
            }
            if (rec_hdr->struct_id == log_wrap_id)
                status->wrap_time = rec_hdr->timestamp;
            if ((retval = scan_forward(log, NO_SYNCH)) != RVM_SUCCESS)
                goto err_exit;
            assert(log->trunc_thread == cthread_self());
            assert((status->trunc_state & RVM_TRUNC_PHASES) ==
                   RVM_TRUNC_FIND_TAIL);
            if (rvm_chk_sigint != NULL) /* test for interrupt */
                if ((*rvm_chk_sigint)(NULL))
                    goto err_exit;
        } while (log_buf->ptr != -1); /* tail found, no transactions */

    /* re-init scanner sequence checking state since small logs can cause
       a few records to be rescanned and re-init read buffer at tail
    */
    tail = status->log_tail;
    reset_hdr_chks(log);
    if ((retval = init_buffer(log, &tail, FORWARD, NO_SYNCH)) != RVM_SUCCESS)
        goto err_exit;
    assert(log->trunc_thread == cthread_self());
    assert((status->trunc_state & RVM_TRUNC_PHASES) == RVM_TRUNC_FIND_TAIL);
    /* see if record at tail is valid, scan until bad record found */
    if ((retval = validate_rec_forward(log, NO_SYNCH)) != RVM_SUCCESS)
        goto err_exit;
    DO_FOREVER
    {
        if (log_buf->ptr == -1)
            break; /* tail located */

        /* compute provisional new tail offset, rec_num, timestamp */
        rec_hdr   = (rec_hdr_t *)(&log_buf->buf[log_buf->ptr]);
        temp_tail = RVM_ADD_LENGTH_TO_OFFSET(
            log_buf->offset,
            (log_buf->ptr + rec_hdr->rec_length + sizeof(rec_end_t)));
        last_rec_num = rec_hdr->rec_num;
        last_write   = rec_hdr->timestamp;

        /* type-specific status data recovery */
        switch (rec_hdr->struct_id) {
        case log_wrap_id:
            set_wrap_status(status, rec_hdr);
            tail = status->log_start;
            break;

        case trans_hdr_id:
            if ((retval = set_trans_status(log, rec_hdr)) != RVM_SUCCESS)
                goto err_exit;
            assert(log->trunc_thread == cthread_self());
            assert((status->trunc_state & RVM_TRUNC_PHASES) ==
                   RVM_TRUNC_FIND_TAIL);
            if (log_buf->ptr != -1)
                tail = temp_tail; /* update if trans OK */
            break;

        case log_seg_id:
            status->n_special++;
            tail = temp_tail;
            break;

        default:
            assert(rvm_false); /* error - should have header */
        }

        /* scan to next record */
        if (log_buf->ptr == -1)
            break; /* tail located */
        if ((retval = scan_forward(log, NO_SYNCH)) != RVM_SUCCESS)
            goto err_exit;
        assert(log->trunc_thread == cthread_self());
        assert((status->trunc_state & RVM_TRUNC_PHASES) == RVM_TRUNC_FIND_TAIL);
        if (rvm_chk_sigint != NULL) /* test for interrupt */
            if ((*rvm_chk_sigint)(NULL))
                goto err_exit;
    }
    /* tail found, update in-memory status */
#ifdef RVM_LOG_TAIL_BUG
    unprotect_page__Fi(ClobberAddress);
#endif /* RVM_LOG_TAIL_BUG */
#ifdef RVM_LOG_TAIL_SHADOW
    assert(RVM_OFFSET_EQL(log_tail_shadow, status->log_tail));
#endif /* RVM_LOG_TAIL_SHADOW */
    status->log_tail = tail;
#ifdef RVM_LOG_TAIL_SHADOW
    RVM_ASSIGN_OFFSET(log_tail_shadow, status->log_tail);
#endif /* RVM_LOG_TAIL_SHADOW */
#ifdef RVM_LOG_TAIL_BUG
    protect_page__Fi(ClobberAddress);
#endif /* RVM_LOG_TAIL_BUG */
    status->last_write = last_write;
    if (RVM_OFFSET_EQL(status->log_head, status->log_tail))
        clear_log_status(log); /* log empty */
    else { /* log not empty */
        status->log_empty = rvm_false;

        if (status->next_rec_num <= last_rec_num)
            status->next_rec_num = last_rec_num + 1;
        if (status->last_rec_num != last_rec_num)
            status->last_rec_num = last_rec_num;
    }

exit:
    status->valid = rvm_true;
err_exit:
    rvm_utlsw          = save_rvm_utlsw;
    status->last_trunc = save_last_trunc;
    assert(log->trunc_thread == cthread_self());
    assert((status->trunc_state & RVM_TRUNC_PHASES) == RVM_TRUNC_FIND_TAIL);
    return retval;
}

/* add segment short id to dictionary */
rvm_return_t enter_seg_dict(log_t *log, long seg_code)
{
    seg_dict_t *seg_dict;
    long old_dict_size, new_dict_size;

    /* lengthen seg_dict_vec if necessary */
    if (log->seg_dict_len < seg_code) {
        new_dict_size = seg_code * sizeof(seg_dict_t);
        old_dict_size = log->seg_dict_len * sizeof(seg_dict_t);
        log->seg_dict_vec =
            (seg_dict_t *)REALLOC((char *)log->seg_dict_vec, new_dict_size);
        if (log->seg_dict_vec == NULL)
            return RVM_ENO_MEMORY;
        (void)BZERO((char *)((long)log->seg_dict_vec + old_dict_size),
                    new_dict_size - old_dict_size);
        log->seg_dict_len = seg_code;
    }

    /* enter in dictionary if not already defined */
    seg_dict = &log->seg_dict_vec[SEG_DICT_INDEX(seg_code)];
    if (seg_dict->struct_id != seg_dict_id) {
        seg_dict->struct_id = seg_dict_id;
        seg_dict->seg_code  = seg_code;
        seg_dict->seg       = NULL;
        init_tree_root(&seg_dict->mod_tree);
        (void)dev_init(&seg_dict->dev, NULL);
    }
    return RVM_SUCCESS;
}

/* complete definition of seg_dict entry */
rvm_return_t def_seg_dict(
    log_t *log /* log descriptor */,
    rec_hdr_t *
        rec_hdr /* log segment definition descriptor (with log record header) */)
{
    log_seg_t *log_seg; /* log segment definition descriptor */
    seg_dict_t *seg_dict; /* segment dictionary entry */
    char *seg_name; /* ptr to segment name in seg_dict rec */
    device_t *dev; /* device descriptor */
    rvm_return_t retval;

    assert(rec_hdr->struct_id == log_seg_id);
    log_seg = (log_seg_t *)RVM_ADD_LENGTH_TO_ADDR(rec_hdr, sizeof(rec_hdr_t));

    /* create dictionary entry if necessary */
    if ((retval = enter_seg_dict(log, log_seg->seg_code)) != RVM_SUCCESS)
        return retval;
    seg_dict = &log->seg_dict_vec[SEG_DICT_INDEX(log_seg->seg_code)];

    /* if segment not defined, set device name (open later) */
    seg_name      = (char *)((rvm_length_t)rec_hdr + LOG_SPECIAL_SIZE);
    seg_dict->seg = seg_lookup(seg_name, &retval);
    if (seg_dict->seg == NULL) {
        assert(log->in_recovery || rvm_utlsw);
        dev       = &seg_dict->dev;
        dev->name = malloc(log_seg->name_len + 1);
        if (dev->name == NULL)
            return RVM_ENO_MEMORY;
        (void)strcpy(dev->name, seg_name);
        dev->num_bytes = log_seg->num_bytes;
    }

    return RVM_SUCCESS;
}

/* change tree comparator for tree_insert */
static long cmp_partial_include(tree_node_t *node1, tree_node_t *node2)
{
    dev_region_t *region1 = (dev_region_t *)node1;
    dev_region_t *region2 = (dev_region_t *)node2;
    return dev_partial_include(&region1->offset, &region1->end_offset,
                               &region2->offset, &region2->end_offset);
}

/* set length of change tree node from offsets */
static void set_node_length(dev_region_t *node /* change tree node */)
{
    rvm_offset_t offset_temp; /* offset arithmetic temp */

    offset_temp = RVM_SUB_OFFSETS(node->end_offset, node->offset);
    assert(RVM_OFFSET_LEQ(offset_temp, node->end_offset)); /* overflow! */
    node->length = RVM_OFFSET_TO_LENGTH(offset_temp);
}

static rvm_return_t
change_tree_insert(seg_dict_t *seg_dict /* seg_dict for this nv */,
                   dev_region_t *node /* change tree node for this nv */)
{
    dev_region_t *x_node; /* existing node if conflict */
    dev_region_t *split_node; /* ptr to created node, when used */
    rvm_length_t log_diff; /* adjustment to log/nv_buf offset */
    long cmpval; /* comparison return value */
    char *shadow_vmaddr; /* vmaddr of shadowed data */
    rvm_length_t shadow_length = 0; /* length of shadowed data */
    rvm_length_t shadow_skew   = 0; /* byte skew of shadowed data */
    char *shadow_ptr           = NULL; /* ptr to shadowed data in vm */
    rvm_offset_t shadow_offset; /* offset of shadowed data in log */
    rvm_return_t retval;

    /* try to insert node & see if values already there */
    if (node->length == 0)
        goto free_node; /* eliminate zero-length nodes */

    if (num_nodes-- == 0) {
        num_nodes = NODES_PER_YIELD;
        if (!(default_log->in_recovery || rvm_utlsw)) {
            if (!rvm_no_yield)
                cthread_yield(); /* allow reschedule */
        }
    }
    assert(default_log->trunc_thread == cthread_self());
    assert((default_log->status.trunc_state & RVM_TRUNC_PHASES) ==
           RVM_TRUNC_BUILD_TREE);

    if (tree_insert(&seg_dict->mod_tree, &node->links.node,
                    cmp_partial_include)) {
        if (rvm_chk_len != 0) /* do monitoring */
            monitor_vmaddr(node->vmaddr, node->length, node->nv_ptr,
                           &node->log_offset, NULL,
                           "change_tree_insert: inserting entire range");
        return RVM_SUCCESS; /* no shadowed values */
    }
    x_node = (dev_region_t *)/* get existing node */
        (seg_dict->mod_tree.traverse[seg_dict->mod_tree.level].ptr);

    /* some values already there: test existing node spans new */
    if (dev_total_include(&node->offset, &node->end_offset, &x_node->offset,
                          &x_node->end_offset) == 0) {
        if (rvm_chk_len != 0) /* do monitoring */
            monitor_vmaddr(node->vmaddr, node->length, NULL, NULL, NULL,
                           "change_tree_insert: all values shadowed");
        goto free_node; /* yes, all values shadowed */
    }
    /* some shadowed, test if new values span existing node */
    if ((cmpval = dev_total_include(&x_node->offset, &x_node->end_offset,
                                    &node->offset, &node->end_offset)) == 0)
        if (RVM_OFFSET_LSS(
                node->offset,
                x_node->offset)) { /* make node for preceding values */
            if ((split_node = make_dev_region()) == NULL)
                return RVM_ENO_MEMORY;
            if (node->nv_buf != NULL) {
                assert(RVM_OFFSET_EQL_ZERO(node->log_offset));
                assert(node->nv_buf->struct_id == nv_buf_id);
                split_node->nv_buf = node->nv_buf;
                node->nv_buf->ref_cnt++;
                split_node->nv_ptr = node->nv_ptr;
            } else
                assert(node->nv_ptr == NULL);

            /* complete the new node */
            split_node->offset     = node->offset;
            split_node->end_offset = x_node->offset;
            split_node->log_offset = node->log_offset;
            split_node->vmaddr     = node->vmaddr;
            set_node_length(split_node);
            node->vmaddr += split_node->length;
            node->offset =
                RVM_ADD_LENGTH_TO_OFFSET(node->offset, split_node->length);
            log_diff = split_node->length +
                       BYTE_SKEW(RVM_OFFSET_TO_LENGTH(split_node->offset));

            if (node->nv_ptr != NULL)
                node->nv_ptr = (char *)CHOP_TO_LENGTH(
                    RVM_ADD_LENGTH_TO_ADDR(node->nv_ptr, log_diff));
            else
                node->log_offset = CHOP_OFFSET_TO_LENGTH_SIZE(
                    RVM_ADD_LENGTH_TO_OFFSET(split_node->log_offset, log_diff));

            /* insert split node in tree */
            if (rvm_chk_len != 0) /* do monitoring */
                monitor_vmaddr(split_node->vmaddr, split_node->length, NULL,
                               NULL, NULL,
                               "change_tree_insert: inserting split node");
            if ((retval = change_tree_insert(seg_dict, split_node)) !=
                RVM_SUCCESS)
                return retval;
        }
    /* test if new values follow existing node */
    shadow_skew = BYTE_SKEW(RVM_OFFSET_TO_LENGTH(node->offset));
    if (cmpval <= 0) {
        /* yes, reset starting offset */
        shadow_vmaddr = node->vmaddr;
        shadow_length = RVM_OFFSET_TO_LENGTH(
            RVM_SUB_OFFSETS(x_node->end_offset, node->offset));
        shadow_ptr    = node->nv_ptr;
        shadow_offset = node->log_offset;
        node->offset  = x_node->end_offset;
        set_node_length(node);
        if (node->nv_ptr != NULL) /* adjust buffer pointer */
            node->nv_ptr = (char *)CHOP_TO_LENGTH(RVM_ADD_LENGTH_TO_ADDR(
                node->nv_ptr, shadow_length + shadow_skew));
        else /* adjust log offset */
            node->log_offset = CHOP_OFFSET_TO_LENGTH_SIZE(
                RVM_ADD_LENGTH_TO_OFFSET(node->log_offset,
                                         shadow_length + shadow_skew));
        node->vmaddr = RVM_ADD_LENGTH_TO_ADDR(node->vmaddr, shadow_length);
    } else
    /* new values precede existing node, but don't span it */
    { /* reset end offset */
        node->end_offset = x_node->offset;
        shadow_length    = node->length; /* save old length */
        set_node_length(node);
        shadow_length -= node->length; /* correct for new length */
        shadow_vmaddr = RVM_ADD_LENGTH_TO_ADDR(node->vmaddr, node->length);
        if (node->nv_ptr != NULL)
            shadow_ptr = (char *)CHOP_TO_LENGTH(RVM_ADD_LENGTH_TO_ADDR(
                node->nv_ptr, shadow_length + shadow_skew));
        shadow_offset = CHOP_OFFSET_TO_LENGTH_SIZE(RVM_ADD_LENGTH_TO_OFFSET(
            node->log_offset, shadow_length + shadow_skew));
    }
    /* insert modified node */
    if (rvm_chk_len != 0) /* do monitoring */
    {
        if (shadow_length != 0)
            monitor_vmaddr(shadow_vmaddr, shadow_length, shadow_ptr,
                           &shadow_offset, NULL,
                           "change_tree_insert: values shadowed");
        monitor_vmaddr(node->vmaddr, node->length, NULL, NULL, NULL,
                       "change_tree_insert: inserting non-shadowed values");
    }
    return change_tree_insert(seg_dict, node);

free_node:
    free_dev_region(node);
    return RVM_SUCCESS;
}

/* prepare new value record for seg_dict's mod_tree
   if new values are <= nv_local_max, they must be in buffer */
static rvm_return_t do_nv(log_t *log, nv_range_t *nv)
{
    log_status_t *status = &log->status; /* status descriptor */
    log_buf_t *log_buf   = &log->log_buf; /* log buffer descriptor */
    seg_dict_t *seg_dict; /* seg_dict for this nv */
    dev_region_t *node; /* change tree node for this nv */
    rvm_length_t aligned_len; /* allocation temp */
    rvm_offset_t offset; /* monitoring temp */
    rvm_bool_t chk_val; /* checksum result */
    rvm_return_t retval; /* return value */

    assert(log->trunc_thread == cthread_self());
    assert((status->trunc_state & RVM_TRUNC_PHASES) == RVM_TRUNC_BUILD_TREE);
    assert(nv->rec_hdr.struct_id == nv_range_id); /* not a nv range header */
    assert(TIME_EQL(log_buf->timestamp, nv->rec_hdr.timestamp));

    if (rvm_chk_len != 0) /* do monitoring */
    {
        offset = RVM_ADD_LENGTH_TO_OFFSET(log_buf->offset,
                                          log_buf->ptr + sizeof(nv_range_t));
        monitor_vmaddr(nv->vmaddr, nv->length, NULL, &offset, &nv->rec_hdr,
                       "do_nv: data from log");
    }

    if (nv->length == 0)
        return RVM_SUCCESS; /* ignore null changes */

    /* be sure in segment dictionary */
    if ((retval = enter_seg_dict(log, nv->seg_code)) != RVM_SUCCESS)
        return retval;
    seg_dict = &log->seg_dict_vec[SEG_DICT_INDEX(nv->seg_code)];

    /* make a tree node for changes */
    if ((node = make_dev_region()) == NULL)
        return RVM_ENO_MEMORY;
    node->offset     = nv->offset;
    node->end_offset = RVM_ADD_LENGTH_TO_OFFSET(nv->offset, nv->length);
    node->length     = nv->length;
    node->vmaddr     = nv->vmaddr;
    /* see if mods small enough to keep in vm */
    if (nv->length <= NV_LOCAL_MAX) { /* yes, get some space for nv */
        aligned_len = ALIGNED_LEN(RVM_OFFSET_TO_LENGTH(nv->offset), nv->length);
        if ((node->nv_buf = (nv_buf_t *)malloc(NV_BUF_SIZE(aligned_len))) ==
            NULL)
            return RVM_ENO_MEMORY;
        node->nv_buf->struct_id = nv_buf_id;
        node->nv_buf->alloc_len = NV_BUF_SIZE(aligned_len);
        node->nv_buf->ref_cnt   = 1;
        node->nv_buf->chk_sum   = nv->chk_sum;
        node->nv_buf->data_len  = nv->length;
        node->nv_ptr            = (char *)&node->nv_buf->buf;
        assert(((rvm_length_t)nv + sizeof(nv_range_t)) >=
               (rvm_length_t)default_log->log_buf.buf);
        assert(((rvm_length_t)nv + sizeof(nv_range_t)) <
               ((rvm_length_t)default_log->log_buf.buf +
                default_log->log_buf.r_length));

        /* basic BCOPY will not change alignment since buffer padded */
        (void)BCOPY(RVM_ADD_LENGTH_TO_ADDR(nv, sizeof(nv_range_t)),
                    node->nv_ptr, aligned_len);
    } else
        /* no, set offset in log for nv's */
        node->log_offset = RVM_ADD_LENGTH_TO_OFFSET(
            log_buf->offset,
            (rvm_length_t)nv - (rvm_length_t)log_buf->buf + sizeof(nv_range_t));

    /* put in change tree */
    if ((retval = change_tree_insert(seg_dict, node)) != RVM_SUCCESS)
        return retval;

    /* see if complete check sum test wanted */
    if (rvm_chk_sum) {
        if ((retval = range_chk_sum(log, nv, &chk_val, SYNCH)) != RVM_SUCCESS)
            return retval;
        assert(chk_val == rvm_true); /* check sum failure */
        if ((retval = scan_nv_reverse(log, SYNCH)) != RVM_SUCCESS)
            return retval;
        assert(log->trunc_thread == cthread_self());
        assert((status->trunc_state & RVM_TRUNC_PHASES) ==
               RVM_TRUNC_BUILD_TREE);
    }

    return RVM_SUCCESS;
}

/* scan modifications of transaction in reverse order & build tree */
static rvm_return_t
do_trans(log_t *log /* log descriptor */,
         rvm_bool_t skip_trans /* scan, but ignore if true */)
{
    log_status_t *status = &log->status; /* status descriptor */
    log_buf_t *log_buf   = &log->log_buf; /* log buffer descriptor */

    rec_hdr_t *rec_hdr; /* last record header scanned */
    rec_end_t *rec_end; /* end marker for transaction */
    trans_hdr_t *trans_hdr; /* transaction header ptr */
    long num_ranges = 0; /* ranges processed */
    long prev_range = 0; /* previous range number */
    rvm_return_t retval; /* return value */

    assert(log->trunc_thread == cthread_self());
    assert((status->trunc_state & RVM_TRUNC_PHASES) == RVM_TRUNC_BUILD_TREE);

    /* remember the transaction's timestamp and scan ranges */
    rec_end = (rec_end_t *)&log_buf->buf[log_buf->ptr];
    assert(rec_end->rec_hdr.struct_id == rec_end_id);
    log_buf->timestamp = rec_end->rec_hdr.timestamp;
    DO_FOREVER
    {
        if ((retval = scan_nv_reverse(log, SYNCH)) != RVM_SUCCESS)
            return retval;
        assert(log->trunc_thread == cthread_self());
        assert((status->trunc_state & RVM_TRUNC_PHASES) ==
               RVM_TRUNC_BUILD_TREE);
        rec_hdr = (rec_hdr_t *)&log_buf->buf[log_buf->ptr];

        /* test for end */
        if (rec_hdr->struct_id == trans_hdr_id)
            break; /* done */

        /* check order and process the range */
        assert(rec_hdr->struct_id == nv_range_id);
        if (prev_range != 0)
            assert(((nv_range_t *)rec_hdr)->range_num == (prev_range - 1));
        if (!skip_trans)
            if ((retval = do_nv(log, (nv_range_t *)rec_hdr)) != RVM_SUCCESS)
                return retval;

        /* tally ranges processed */
        num_ranges++;
        prev_range = ((nv_range_t *)rec_hdr)->range_num;
    }

    /* sanity checks at the end... */
    trans_hdr = (trans_hdr_t *)rec_hdr;
    assert(trans_hdr->rec_hdr.struct_id == trans_hdr_id);
    assert(TIME_EQL(trans_hdr->rec_hdr.timestamp, log_buf->timestamp));
    assert(trans_hdr->num_ranges == num_ranges);
    if (num_ranges != 0)
        assert(prev_range == 1);

    return RVM_SUCCESS;
}

/* log wrap-around validation */
static rvm_return_t
chk_wrap(log_t *log /* log descriptor */,
         rvm_bool_t force_wrap_chk /* wrap check required if true */,
         rvm_bool_t *skip_trans /* set true if bad split */)
{
    log_status_t *status = &log->status; /* status descriptor */
    log_buf_t *log_buf   = &log->log_buf; /* log buffer descriptor */
    rvm_offset_t offset; /* offset temp */
    rvm_offset_t end_offset; /* offset of last trans end marker */
    rec_end_t *rec_end; /* last record scanned in buffer */
    trans_hdr_t last_trans_hdr; /* last transaction record header */
    trans_hdr_t *trans_hdr; /* header temporary */
    log_wrap_t *log_wrap; /* wrap-around marker */
    rvm_length_t tmp_ptr; /* buffer index temp */
    rvm_length_t data_len; /* length temporary */
    rvm_return_t retval; /* return value */

    *skip_trans = rvm_false;
    rec_end     = (rec_end_t *)&log_buf->buf[log_buf->ptr];
    offset      = RVM_ADD_LENGTH_TO_OFFSET(log_buf->offset, log_buf->ptr);
    offset = RVM_SUB_LENGTH_FROM_OFFSET(offset, rec_end->rec_hdr.rec_length);

    /* check if transaction header is at start of log data area */
    if (!RVM_OFFSET_EQL(offset, status->log_start) && (!force_wrap_chk))
        return RVM_SUCCESS; /* no, nothing more needed */

    /* get header */
    if (force_wrap_chk) {
        /* header can be anywhere */
        if (RVM_OFFSET_LSS(offset, log_buf->offset)) {
            retval = load_aux_buf(log, &offset, sizeof(trans_hdr_t), &tmp_ptr,
                                  &data_len, SYNCH, rvm_false);
            if (retval != RVM_SUCCESS)
                return retval;
            assert(log->trunc_thread == cthread_self());
            assert((status->trunc_state & RVM_TRUNC_PHASES) ==
                   RVM_TRUNC_BUILD_TREE);
            assert(data_len >= sizeof(trans_hdr_t));
            trans_hdr = (trans_hdr_t *)&log_buf->aux_buf[tmp_ptr];
        } else
            trans_hdr = (trans_hdr_t *)&log_buf
                            ->buf[log_buf->ptr - rec_end->rec_hdr.rec_length];
    } else
        /* header is at start of aux_buf or recovery buffer */
        if (RVM_OFFSET_LSS(offset, log_buf->offset))
            trans_hdr = (trans_hdr_t *)log_buf->aux_buf;
        else
            trans_hdr = (trans_hdr_t *)log_buf->buf;

    /* check for split transaction */
    assert(trans_hdr->rec_hdr.struct_id == trans_hdr_id);
    if (TRANS_HDR(FIRST_ENTRY_FLAG) && TRANS_HDR(LAST_ENTRY_FLAG))
        return RVM_SUCCESS; /* not split, nothing more needed */

    /* split, see if must check further or skip record */
    assert(TRANS_HDR(FIRST_ENTRY_FLAG) || TRANS_HDR(LAST_ENTRY_FLAG));
    if (!TRANS_HDR(LAST_ENTRY_FLAG)) {
        if (log_buf->split_ok) { /* split previously checked */
            log_buf->split_ok = rvm_false;
            return RVM_SUCCESS;
        }
        if (force_wrap_chk) /* if not last entry, trans not good */
        {
            *skip_trans = rvm_true;
            return RVM_SUCCESS;
        }
    }

    /* must make local copy and scan for first record of transaction */
    end_offset = RVM_ADD_LENGTH_TO_OFFSET(log_buf->offset,
                                          (log_buf->ptr + sizeof(rec_end_t)));
    (void)BCOPY(trans_hdr, &last_trans_hdr, sizeof(trans_hdr_t));
    if ((retval = scan_reverse(log, SYNCH)) != RVM_SUCCESS)
        return retval;
    assert(log->trunc_thread == cthread_self());
    assert((status->trunc_state & RVM_TRUNC_PHASES) == RVM_TRUNC_BUILD_TREE);

    /* wrap-around had better be next... */
    assert((long)log_buf->ptr >= 0);
    log_wrap = (log_wrap_t *)&log_buf->buf[log_buf->ptr];
    assert(log_wrap->rec_hdr.struct_id == log_wrap_id);
    assert(log_wrap->rec_hdr.rec_num == (last_trans_hdr.rec_hdr.rec_num - 1));

    /* now scan for first record of transaction */
    if ((retval = scan_reverse(log, SYNCH)) != RVM_SUCCESS)
        return retval;
    assert(log->trunc_thread == cthread_self());
    assert((status->trunc_state & RVM_TRUNC_PHASES) == RVM_TRUNC_BUILD_TREE);
    assert((long)log_buf->ptr >= 0);
    rec_end = (rec_end_t *)&log_buf->buf[log_buf->ptr];
    assert(rec_end->rec_hdr.struct_id == rec_end_id);
    /* check if the header is the first record of last transaction */
    offset = RVM_ADD_LENGTH_TO_OFFSET(log_buf->offset, log_buf->ptr);
    offset = RVM_SUB_LENGTH_FROM_OFFSET(offset, rec_end->rec_hdr.rec_length);
    if (RVM_OFFSET_LSS(offset, log_buf->offset)) {
        /* header is in aux_buf */
        tmp_ptr   = OFFSET_TO_SECTOR_INDEX(offset);
        trans_hdr = (trans_hdr_t *)&log_buf->aux_buf[tmp_ptr];
    } else {
        /* header is in recovery buffer */
        tmp_ptr =
            RVM_OFFSET_TO_LENGTH(RVM_SUB_OFFSETS(offset, log_buf->offset));
        assert(tmp_ptr >= 0);
        trans_hdr = (trans_hdr_t *)&log_buf->buf[tmp_ptr];
    }

    /* sanity checks... */
    assert(trans_hdr->rec_hdr.struct_id == trans_hdr_id);
    assert(TRANS_HDR(FIRST_ENTRY_FLAG));
    assert(TIME_EQL(trans_hdr->uname, last_trans_hdr.uname));
    assert(trans_hdr->rec_hdr.rec_num == (last_trans_hdr.rec_hdr.rec_num - 2));

    /* all is well, restore last transaction record */
    log_buf->prev_rec_num = 0;
    ZERO_TIME(log_buf->prev_timestamp);
    if ((retval = init_buffer(log, &end_offset, REVERSE, SYNCH)) != RVM_SUCCESS)
        return retval;
    assert(log->trunc_thread == cthread_self());
    assert((status->trunc_state & RVM_TRUNC_PHASES) == RVM_TRUNC_BUILD_TREE);
    log_buf->ptr -= sizeof(rec_end_t);
    log_buf->split_ok = rvm_true;

    return RVM_SUCCESS;
}

/* Recovery: phase 2 -- build modification trees, and
   construct dictionary of segment short names
*/
#define X(a)
static rvm_return_t build_tree(log_t *log /* log descriptor */)
{
    log_status_t *status = &log->status; /* status descriptor */
    log_buf_t *log_buf   = &log->log_buf; /* log buffer descriptor */
    rvm_return_t retval; /* return value */
    rvm_offset_t tail; /* tail offset temp */
    rec_end_t *rec_end; /* last record scanned in buffer */
    rvm_length_t trans_cnt    = 0; /* transactions processed */
    rvm_bool_t force_wrap_chk = rvm_false; /* true if suspect bad wrap */
    rvm_bool_t skip_trans; /* true if bad wrap trans to be skipped */

    assert(log->trunc_thread == cthread_self());
    assert(((status->trunc_state & RVM_TRUNC_PHASES) == RVM_TRUNC_FIND_TAIL) ||
           ((status->trunc_state & RVM_TRUNC_PHASES) == ZERO));
    status->trunc_state = (status->trunc_state & (~RVM_TRUNC_FIND_TAIL)) |
                          RVM_TRUNC_BUILD_TREE;

    /* reset sequence checks and init scan buffers */
    X(reset_hdr)
    reset_hdr_chks(log);
    X(clear_aux)
    clear_aux_buf(log);
    X(init_buf)
    if (RVM_OFFSET_EQL(status->prev_log_tail, status->log_start))
        retval = init_buffer(log, &status->log_start, FORWARD, SYNCH);
    else
        retval = init_buffer(log, &status->prev_log_tail, REVERSE, SYNCH);
    assert(retval == RVM_SUCCESS);
    assert(log->trunc_thread == cthread_self());
    X(done_init_buf)
    /* scan in reverse from tail to find records for uncommitted changes */
    num_nodes         = NODES_PER_YIELD;
    log_buf->split_ok = rvm_false; /* split records not checked yet */
    tail              = status->prev_log_tail; /* use previous epoch tail */
    while (!RVM_OFFSET_EQL(tail, status->prev_log_head)) {
        X(start loop)
        if ((retval = scan_reverse(log, SYNCH)) != RVM_SUCCESS)
            return retval;
        X(done scan_reverse)
        assert(log->trunc_thread == cthread_self());
        assert((status->trunc_state & RVM_TRUNC_PHASES) ==
               RVM_TRUNC_BUILD_TREE);
        if (rvm_chk_sigint != NULL) /* test for interrupt */
            if ((*rvm_chk_sigint)(NULL))
                return RVM_SUCCESS;
        assert((long)log_buf->ptr >= 0); /* log damage, invalid record */

        /* check type of end marker, do type-dependent processing */
        rec_end = (rec_end_t *)&log_buf->buf[log_buf->ptr];
        if (rec_end->rec_hdr.struct_id == log_wrap_id) {
            X(log_wrap)
            if (!log_buf->split_ok)
                force_wrap_chk = rvm_true;
        } else {
            X(else)
            assert(rec_end->rec_hdr.struct_id == rec_end_id);
            switch (rec_end->rec_type) {
            case trans_hdr_id: /* process transaction */
                X(trans_hdr_id : chk_wrap)
                if ((retval = chk_wrap(log, force_wrap_chk, &skip_trans)) !=
                    RVM_SUCCESS)
                    return retval;
                force_wrap_chk = rvm_false;
                X(trans_hdr_id : do_trans)
                if ((retval = do_trans(log, skip_trans)) != RVM_SUCCESS)
                    return retval;
                X(trans_hdr_id : end)
                trans_cnt++;
                break;
            case log_seg_id: /* enter seg short id in dictionary */
                X(log_seg_id : def_seg_dict)
                if ((retval = def_seg_dict(
                         log, (rec_hdr_t *)RVM_SUB_LENGTH_FROM_ADDR(
                                  rec_end, rec_end->rec_hdr.rec_length))) !=
                    RVM_SUCCESS)
                    return retval;
                X(log_seg_id : done)
                log_buf->ptr -= rec_end->rec_hdr.rec_length;
                break;
            default:
                assert(rvm_false); /* trouble, log damage? */
            }
        }

        /* update local tail ptr */
        tail = RVM_ADD_LENGTH_TO_OFFSET(log_buf->offset, log_buf->ptr);
    }

    /* leave buffer unprotected for later phases */
    /* MACH_RVM_PROTECT
 *
 * protect(log_buf->buf, log_buf->length, FALSE, VM_PROT_WRITE | VM_PROT_READ);
 */

    return RVM_SUCCESS;
}

/* pre-scan change tree to see how much to read to read into buffer */
static dev_region_t *pre_scan(log_t *log /* log descriptor */,
                              tree_root_t *tree /* current tree root */)
{
    log_buf_t *log_buf      = &log->log_buf; /* log buffer descriptor */
    dev_region_t *last_node = NULL;
    dev_region_t *node; /* current change tree node */
    rvm_offset_t temp;

    /* find node with least offset */
    node = (dev_region_t *)tree->root;
    /* XXX - Can node ever be NULL?  If so, last_node can be random */
    /* I currently believe it must be NON-null */
    assert(node != NULL);
    while (node != NULL) {
        assert(node->links.node.struct_id == dev_region_id);
        last_node = node;
        node      = (dev_region_t *)node->links.node.lss;
    }
    log_buf->offset = CHOP_OFFSET_TO_SECTOR_SIZE(last_node->offset);

    /* scan for maximum offset node that will fit in buffer */
    node = (dev_region_t *)tree->root;
    while (node != NULL) {
        assert(node->links.node.struct_id == dev_region_id);

        /* compute buffer extension for this node */
        temp = RVM_SUB_OFFSETS(node->end_offset, log_buf->offset);
        temp = ROUND_OFFSET_TO_SECTOR_SIZE(temp);

        /* see if will fit in log buffer */
        if (RVM_OFFSET_GTR(temp, log_buf->buf_len))
            node = (dev_region_t *)node->links.node.lss; /* try smaller */
        else {
            /* see if there's another that will also fit */
            last_node = node;
            node      = (dev_region_t *)node->links.node.gtr;
        }
    }

    return last_node;
}

/* merge large node disk-resident new values with segment data */
static rvm_return_t
disk_merge(log_t *log /* log descriptor */,
           dev_region_t *node /* node to merge */,
           rvm_bool_t preload /* end sector preload done if true */)
{
    log_status_t *status  = &log->status; /* status descriptor */
    log_buf_t *log_buf    = &log->log_buf; /* log buffer descriptor */
    rvm_length_t data_len = 0; /* actual nv data length read */
    rvm_length_t buf_ptr; /* log buffer ptr */
    rvm_length_t aux_ptr; /* aux buffer ptr
                             (compensates for sector alignment) */
    rvm_length_t tmp_ptr; /* temporary buffer ptr */
    long rw_length; /* actual i/o transfer length */
    rvm_offset_t end_offset; /* end offset temporary */
    rvm_return_t retval; /* return value */
    rvm_bool_t was_preloaded = preload; /* save preload state */

    assert(log->trunc_thread == cthread_self());
    assert((status->trunc_state & RVM_TRUNC_PHASES) == RVM_TRUNC_APPLY);
    assert(node->links.node.struct_id == dev_region_id);

    /* set log buffer pointer and end offset */
    end_offset = CHOP_OFFSET_TO_SECTOR_SIZE(node->end_offset);
    buf_ptr =
        RVM_OFFSET_TO_LENGTH(RVM_SUB_OFFSETS(node->offset, log_buf->offset));
    node->log_offset =
        RVM_ADD_LENGTH_TO_OFFSET(node->log_offset, BYTE_SKEW(buf_ptr));
    DO_FOREVER
    { /* fill log buffer from aux buf */
        while ((buf_ptr < log_buf->length) && (node->length != 0)) {
            /* see how much to get in this pass & load aux_buf */
            if ((log_buf->length - buf_ptr) < node->length)
                rw_length = log_buf->length - buf_ptr; /* fill log_buf */
            else
                rw_length = node->length; /* get all remaining */
            retval = load_aux_buf(log, &node->log_offset, rw_length, &aux_ptr,
                                  &data_len, SYNCH, rvm_true);
            if (retval != RVM_SUCCESS)
                return retval;
            /* sanity checks and monitoring */
            assert((aux_ptr + data_len) <= log_buf->aux_rlength);
            assert((buf_ptr + data_len) <= log_buf->length);
            assert(BYTE_SKEW(aux_ptr) == BYTE_SKEW(node->vmaddr));
            assert((long)(node->length - data_len) >= 0);
            if (rvm_chk_len != 0)
                monitor_vmaddr(node->vmaddr, data_len,
                               &log_buf->aux_buf[aux_ptr], NULL, NULL,
                               "disk_merge: data read from log:");

            /* preload of last modified segment sector */
            if (RVM_OFFSET_GTR(RVM_ADD_LENGTH_TO_OFFSET(node->offset, data_len),
                               end_offset) &&
                (!preload)) {
                /* must load last sector of mods from segment */
                tmp_ptr = CHOP_TO_SECTOR_SIZE(buf_ptr + data_len);
                if (!(log->in_recovery || rvm_utlsw || rvm_no_yield)) {
                    cthread_yield(); /* allow reschedule */
                    assert(log->trunc_thread == cthread_self());
                }
                rw_length = read_dev(log->cur_seg_dev, &end_offset,
                                     &log_buf->buf[tmp_ptr], SECTOR_SIZE);
                if (rw_length < 0)
                    return RVM_EIO;
                assert(log->trunc_thread == cthread_self());
                assert((status->trunc_state & RVM_TRUNC_PHASES) ==
                       RVM_TRUNC_APPLY);
                assert(rw_length == SECTOR_SIZE);
                preload = rvm_true;

                /* monitor data from last sector */
                if (rvm_chk_len != 0)
                    monitor_vmaddr(node->vmaddr, data_len,
                                   &log_buf->buf[buf_ptr], NULL, NULL,
                                   "disk_merge: data read from segment:");
            }

            /* copy to segment (in log buffer) */
            (void)BCOPY(&log_buf->aux_buf[aux_ptr], &log_buf->buf[buf_ptr],
                        data_len);

            /* tally bytes merged & do monitoring */
            if (rvm_chk_len != 0) {
                monitor_vmaddr(node->vmaddr, data_len, &log_buf->buf[buf_ptr],
                               NULL, NULL,
                               "disk_merge: data merged to segment:");
            }
            node->length -= data_len;
            node->vmaddr += data_len;
            node->log_offset =
                RVM_ADD_LENGTH_TO_OFFSET(node->log_offset, data_len);
            node->offset = RVM_ADD_LENGTH_TO_OFFSET(node->offset, data_len);
            buf_ptr += data_len;
            /* if done, set final write length */
            if (node->length == 0) {
                assert(RVM_OFFSET_EQL(node->offset, node->end_offset));
                end_offset = RVM_ADD_LENGTH_TO_OFFSET(log_buf->offset, buf_ptr);
                assert(RVM_OFFSET_EQL(end_offset, node->end_offset));
                if (!was_preloaded)
                    log_buf->r_length = ROUND_TO_SECTOR_SIZE(buf_ptr);
                return RVM_SUCCESS;
            }
        }

        /* write buffer to segment & monitor */
        assert(buf_ptr == log_buf->length);
        rw_length = write_dev(log->cur_seg_dev, &log_buf->offset, log_buf->buf,
                              log_buf->length, SYNCH);
        if (rw_length < 0)
            return RVM_EIO;
        assert(log->trunc_thread == cthread_self());
        assert((status->trunc_state & RVM_TRUNC_PHASES) == RVM_TRUNC_APPLY);
        assert(rw_length == log_buf->length);
        if (rvm_chk_len != 0)
            monitor_vmaddr(node->vmaddr - data_len, data_len,
                           &log_buf->buf[buf_ptr - data_len], NULL, NULL,
                           "disk_merge: data written to segment:");
        if (!(log->in_recovery || rvm_utlsw || rvm_no_yield)) {
            cthread_yield(); /* allow reschedule */
            assert(log->trunc_thread == cthread_self());
            assert((status->trunc_state & RVM_TRUNC_PHASES) == RVM_TRUNC_APPLY);
        }
        log_buf->offset = RVM_ADD_LENGTH_TO_OFFSET(log_buf->offset, buf_ptr);
        buf_ptr         = 0;
        assert(OFFSET_TO_SECTOR_INDEX(log_buf->offset) == 0);
    }
}

/* merge node's new values with segment data in buffer */
static rvm_return_t
merge_node(log_t *log /* log descriptor */,
           dev_region_t *node /* current change tree node */,
           rvm_bool_t preload /* end sector preload done if true */)
{
    log_buf_t *log_buf = &log->log_buf; /* log buffer descriptor */
    rvm_length_t temp;
    rvm_return_t retval; /* return value */

    /* do monitoring and merge node data into segment */
    if (RVM_OFFSET_EQL_ZERO(node->log_offset)) { /* data in node */
        if (rvm_chk_len != ZERO)
            monitor_vmaddr(node->vmaddr, node->length, node->nv_ptr, NULL, NULL,
                           "merge_node: data copied from node:");
        temp = RVM_OFFSET_TO_LENGTH(
            RVM_SUB_OFFSETS(node->offset, log_buf->offset));
        assert((temp + node->length) <= log_buf->r_length);
        dest_aligned_bcopy(node->nv_ptr, &log_buf->buf[temp], node->length);
    } else /* data on disk -- use aux_buf */
        if ((retval = disk_merge(log, node, preload)) != RVM_SUCCESS)
            return retval;

    /* free node and check for yield */
    (void)free_dev_region(node);
    if (num_nodes-- == 0) {
        num_nodes = NODES_PER_YIELD;
        if (!(log->in_recovery || rvm_utlsw || rvm_no_yield)) {
            cthread_yield(); /* allow reschedule */
            assert(log->trunc_thread == cthread_self());
        }
    }

    return RVM_SUCCESS;
}

static rvm_return_t
update_seg(log_t *log /* log descriptor */,
           seg_dict_t *seg_dict /* segment dictionary entry */,
           device_t *seg_dev /* segment device descriptor */)
{
    log_status_t *status = &log->status; /* status descriptor */
    log_buf_t *log_buf   = &log->log_buf; /* log buffer descriptor */
    long r_length; /* length of data transferred */
    rvm_bool_t preload; /* end sector preload done if true */
    char *addr = NULL; /* monitoring address */
    rvm_offset_t temp; /* offset temporary */
    dev_region_t *node; /* current node */
    dev_region_t *last_node; /* last node before buffer write */
    rvm_return_t retval = RVM_SUCCESS; /* return value */
    long nodes_done     = 0;

    /* sanity checks and initializations */
    assert(&log->dev != seg_dev);
    assert(log->trunc_thread == cthread_self());
    assert((status->trunc_state & RVM_TRUNC_PHASES) == RVM_TRUNC_APPLY);
    rvm_num_nodes = seg_dict->mod_tree.n_nodes;
    rvm_max_depth = seg_dict->mod_tree.max_depth;
    clear_aux_buf(log);

    /* process the change tree */
    if (!(log->in_recovery || rvm_utlsw)) /* begin segment dev_lock crit sec
                                             */
    {
        mutex_lock(&seg_dict->seg->dev_lock);
        assert(log->trunc_thread == cthread_self());
        assert((status->trunc_state & RVM_TRUNC_PHASES) == RVM_TRUNC_APPLY);
    }
    while (seg_dict->mod_tree.root != NULL) {
        /* pre-scan tree to determine how to fill buffer */
        last_node = pre_scan(log, &seg_dict->mod_tree);

        /* initialize buffer with segment data */
        temp = RVM_SUB_OFFSETS(last_node->end_offset, log_buf->offset);
        temp = ROUND_OFFSET_TO_SECTOR_SIZE(temp);
        if (RVM_OFFSET_LEQ(temp, log_buf->buf_len)) {
            /* node(s) fit in log buffer */
            log_buf->r_length = RVM_OFFSET_TO_LENGTH(
                RVM_SUB_OFFSETS(last_node->end_offset, log_buf->offset));
            log_buf->r_length = ROUND_TO_SECTOR_SIZE(log_buf->r_length);
            assert(log_buf->r_length <= log_buf->length);
            preload = rvm_true;
        } else {
            log_buf->r_length = SECTOR_SIZE; /* very large node!! */
            preload           = rvm_false;
        }
        /* allow reschedule & do the read */
        if (!(log->in_recovery || rvm_utlsw || rvm_no_yield)) {
            cthread_yield();
            assert(log->trunc_thread == cthread_self());
            assert((status->trunc_state & RVM_TRUNC_PHASES) == RVM_TRUNC_APPLY);
        }
        if ((r_length = read_dev(seg_dev, &log_buf->offset, log_buf->buf,
                                 log_buf->r_length)) < 0) {
            retval = RVM_EIO;
            goto err_exit;
        }
        assert(log->trunc_thread == cthread_self());
        assert((status->trunc_state & RVM_TRUNC_PHASES) == RVM_TRUNC_APPLY);
        assert(r_length == log_buf->r_length);

        /* merge selected nodes into buffer */
        num_nodes = NODES_PER_YIELD;
        UNLINK_NODES_OF(seg_dict->mod_tree, dev_region_t, node)
        {
            assert(node->links.node.struct_id == dev_region_id);
            nodes_done++;

            /* do monitoring */
            if (rvm_chk_len != 0) {
                temp = log_buf->offset;
                addr = (char *)CHOP_TO_SECTOR_SIZE(node->vmaddr);
                monitor_vmaddr(addr, log_buf->r_length, log_buf->buf,
                               &log_buf->offset, NULL,
                               "update_seg: data read from segment:");
            }

            /* merge data */
            if ((retval = merge_node(log, node, preload)) != RVM_SUCCESS)
                goto err_exit;
            if (rvm_chk_sigint != NULL) /* test for interrupt */
                if ((*rvm_chk_sigint)(NULL))
                    goto err_exit;
            if (node == last_node)
                break;
        }

        /* update the segment on disk */
        if ((r_length = write_dev(seg_dev, &log_buf->offset, log_buf->buf,
                                  log_buf->r_length, rvm_true)) < 0) {
            retval = RVM_EIO;
            goto err_exit;
        }
        assert(log->trunc_thread == cthread_self());
        assert((status->trunc_state & RVM_TRUNC_PHASES) == RVM_TRUNC_APPLY);
        assert(r_length == log_buf->r_length);
        /* do monitoring */
        if (rvm_chk_len != 0) {
            if (!RVM_OFFSET_EQL(temp, log_buf->offset))
                addr = RVM_ADD_LENGTH_TO_ADDR(
                    addr, RVM_OFFSET_TO_LENGTH(
                              RVM_SUB_OFFSETS(log_buf->offset, temp)));
            monitor_vmaddr(addr, log_buf->r_length, log_buf->buf,
                           &log_buf->offset, NULL,
                           "update_seg: data written to segment:");
        }
    }

    /* tree checks and cleanup after unlinking */
    assert(nodes_done == rvm_num_nodes);
    assert(seg_dict->mod_tree.n_nodes == 0);

err_exit:
    if (!(log->in_recovery || rvm_utlsw)) /* end segment dev_lock crit sec */
    {
        mutex_unlock(&seg_dict->seg->dev_lock);
        assert(log->trunc_thread == cthread_self());
        assert((status->trunc_state & RVM_TRUNC_PHASES) == RVM_TRUNC_APPLY);
    }
    return retval;
}

/* Recovery: phase 3 -- apply modifications to segments */
static rvm_return_t apply_mods(log_t *log /* log descriptor */)
{
    log_status_t *status = &log->status; /* status descriptor */
    seg_dict_t *seg_dict; /* current segment dictionary entry */
    device_t *seg_dev; /* segment device descriptor */
    rvm_return_t retval = RVM_SUCCESS; /* return value */
    long i; /* loop counter */
    rvm_length_t flags = O_RDWR;

    assert(log->trunc_thread == cthread_self());
    assert((status->trunc_state & RVM_TRUNC_PHASES) == RVM_TRUNC_BUILD_TREE);
    status->trunc_state = (status->trunc_state & ~RVM_TRUNC_BUILD_TREE) |
                          RVM_TRUNC_APPLY;

    /* iterate through segment dictionary */
    for (i = 0; i < log->seg_dict_len; i++) {
        seg_dict = &log->seg_dict_vec[i];
        assert(seg_dict->struct_id == seg_dict_id);

        if (seg_dict->mod_tree.root == NULL)
            continue; /* no changes to this seg */

        /* open device and get characteristics if necessary */
        if (log->in_recovery) {
            seg_dev = &seg_dict->dev;
            if (rvm_no_update)
                flags = O_RDONLY;
            if (open_dev(seg_dev, flags, 0) < 0)
                return RVM_EIO;
            assert(log->trunc_thread == cthread_self());
            if (set_dev_char(seg_dev, &seg_dev->num_bytes) < 0) {
                close_dev(seg_dev);
                return RVM_EIO;
            }
            assert(log->trunc_thread == cthread_self());
            assert((status->trunc_state & RVM_TRUNC_PHASES) == RVM_TRUNC_APPLY);
        } else {
            assert(seg_dict->seg->links.struct_id == seg_id);
            seg_dev = &(seg_dict->seg->dev); /* already open */
        }
        log->cur_seg_dev = seg_dev;

        /* read segment data and merge new values */
        if ((retval = update_seg(log, seg_dict, seg_dev)) != RVM_SUCCESS)
            return retval;
        assert(log->trunc_thread == cthread_self());
        assert((status->trunc_state & RVM_TRUNC_PHASES) == RVM_TRUNC_APPLY);

        /* close segment device if in recovery */
        if (log->in_recovery)
            if (close_dev(seg_dev) < 0)
                return RVM_EIO;
    }

    /* re-protect buffer */
    /* MACH_RVM_PROTECT
     *
     * protect(log->log_buf.buf, log->log_buf.length, FALSE, VM_PROT_READ);
     */

    return retval;
}

/* Recovery: phase 4 -- update head/tail of log */
static rvm_return_t status_update(log_t *log /* log descriptor */,
                                  rvm_length_t new_1st_rec_num)
{
    log_status_t *status = &log->status; /* status descriptor */
    struct timeval end_time; /* end of action time temp */
    int kretval;
    rvm_return_t retval = RVM_SUCCESS; /* return value */

    assert(log->trunc_thread == cthread_self());
    assert((status->trunc_state & RVM_TRUNC_PHASES) == RVM_TRUNC_APPLY);
    status->trunc_state = (status->trunc_state & ~RVM_TRUNC_APPLY) |
                          RVM_TRUNC_UPDATE;

    /* update the status block on disk */
    CRITICAL(log->dev_lock, /* begin log device lock crit sec */
             {
                 assert(log->trunc_thread == cthread_self());
                 assert((status->trunc_state & RVM_TRUNC_PHASES) ==
                        RVM_TRUNC_UPDATE);
                 status->prev_trunc = status->last_trunc;

                 if (RVM_OFFSET_EQL(status->log_head, status->log_tail))
                     clear_log_status(log); /* log empty */
                 else {
                     RVM_ZERO_OFFSET(status->prev_log_head);
                     RVM_ZERO_OFFSET(status->prev_log_tail);
                     status->first_rec_num = new_1st_rec_num;
                 }

                 /* end timings */
                 kretval = gettimeofday(&end_time, (struct timezone *)NULL);
                 if (kretval != 0)
                     goto err_exit;
                 end_time = sub_times(&end_time, &trunc_start_time);
                 status->tot_truncation_time =
                     add_times(&status->tot_truncation_time, &end_time);
                 status->last_truncation_time = round_time(&end_time);
                 enter_histogram(status->last_truncation_time,
                                 log->status.tot_truncation_times,
                                 truncation_times_vec, truncation_times_len);
                 status->last_tree_build_time = last_tree_build_time;
                 enter_histogram(last_tree_build_time,
                                 log->status.tot_tree_build_times,
                                 truncation_times_vec, truncation_times_len);
                 status->last_tree_apply_time = last_tree_apply_time;
                 enter_histogram(last_tree_apply_time,
                                 log->status.tot_tree_apply_times,
                                 truncation_times_vec, truncation_times_len);

                 retval = write_log_status(log, NULL);
             err_exit:;
                 assert(log->trunc_thread == cthread_self());
                 assert((status->trunc_state & RVM_TRUNC_PHASES) ==
                        RVM_TRUNC_UPDATE);
             }); /* end log device lock crit sec */
    if (kretval != 0)
        return RVM_EIO;
    if (retval != RVM_SUCCESS)
        return retval;

    if (log->in_recovery && (!rvm_utlsw)) /* do recovery-only processing */
    {
        /* kill segment dictionary */
        free_seg_dict_vec(log);

        log->in_recovery = rvm_false;
    }

    return retval;
}

/* switch truncation epochs */
static rvm_return_t
new_epoch(log_t *log /* log descriptor */,
          rvm_length_t *count /* ptr to statistics counter */)
{
    log_status_t *status = &log->status; /* log status descriptor */
    rvm_return_t retval  = RVM_SUCCESS;

    /* be sure last records in truncation are in log */
    assert(log->trunc_thread == cthread_self());
    if (sync_dev(&log->dev) < 0)
        return RVM_EIO;
    assert(log->trunc_thread == cthread_self());

    /* count truncations & accumulate statistics */
    (*count)++;
    copy_log_stats(log);

    /* set up head/tail pointers for truncation */
    status->prev_log_head = status->log_head;
    status->log_head      = status->log_tail;
    status->prev_log_tail = status->log_tail;
    status->last_rec_num  = status->next_rec_num - 1;

    /* set epoch time stamp and write status block */
    make_uname(&status->last_trunc);
    if ((retval = write_log_status(log, NULL)) != RVM_SUCCESS)
        return retval;
    assert(log->trunc_thread == cthread_self());

    /* restore log segment definitions */
    retval = define_all_segs(log);
    assert(log->trunc_thread == cthread_self());
    return retval;
}

/* recover committed state from log */
rvm_return_t log_recover(log_t *log /* log descriptor */,
                         rvm_length_t *count /* ptr to statistics counter */,
                         rvm_bool_t is_daemon /* true if called by daemon */,
                         rvm_length_t flag /* truncation type flag */)
{
    log_status_t *status = &log->status; /* log status descriptor */
    log_daemon_t *daemon = &log->daemon; /* log daemon descriptor */
    struct timeval end_time; /* end of action time temp */
    struct timeval tmp_time; /* local timing temp */
    int kretval;
    rvm_bool_t do_truncation     = rvm_false;
    rvm_return_t retval          = RVM_SUCCESS;
    rvm_length_t new_1st_rec_num = 0;
    X(start)
    CRITICAL(
        log->truncation_lock, /* begin truncation lock crit sec */
        {
            /* capture truncation thread & flag for checking */
            assert(log->trunc_thread == (cthread_t)NULL);
            assert(status->trunc_state == ZERO);
            log->trunc_thread   = cthread_self();
            status->trunc_state = flag;
            X(dev_lock)
            CRITICAL(
                log->dev_lock, /* begin dev_lock crit sec */
                {
                    /* process statistics */
                    assert(log->trunc_thread == cthread_self());
                    kretval = gettimeofday(&trunc_start_time,
                                           (struct timezone *)NULL);
                    if (kretval != 0) {
                        retval = RVM_EIO;
                        goto err_exit1;
                    }
                    last_tree_build_time = 0;
                    last_tree_apply_time = 0;
                    X(in_recovery)
                    /* phase 1: locate tail & start new epoch */
                    if (log->in_recovery) {
                        if ((retval = locate_tail(log)) != RVM_SUCCESS)
                            goto err_exit1;
                        assert((status->trunc_state & RVM_TRUNC_PHASES) ==
                               RVM_TRUNC_FIND_TAIL);
                    }
                    assert(log->trunc_thread == cthread_self());
                    if (rvm_chk_sigint != NULL) /* test for interrupt */
                        if ((*rvm_chk_sigint)(NULL))
                            goto err_exit1;
                    /* see if truncation actually needed */
                    if (RVM_OFFSET_EQL(status->log_tail, status->log_head))
                        status->log_empty = rvm_true;
                    else {
                        status->log_empty = rvm_false;
                        do_truncation     = rvm_true;
                        new_1st_rec_num   = status->next_rec_num;

                        /* switch epochs */
                        if ((retval = new_epoch(log, count)) != RVM_SUCCESS)
                            goto err_exit1;
                        assert(log->trunc_thread == cthread_self());
                    }

                    X(err_exit1)
                err_exit1:;
                    /* signal `initiate_truncation' that the first part is done */
                    if (is_daemon) {
                        mutex_lock(&daemon->lock);
                        assert(log->daemon.thread == cthread_self());
                        assert(daemon->state == truncating);
                        assert((status->trunc_state & RVM_ASYNC_TRUNCATE) != 0);
                        condition_signal(&daemon->flush_flag);
                        mutex_unlock(&daemon->lock);
                    }
                }); /* end dev_lock crit sec */

            if (retval != RVM_SUCCESS)
                goto err_exit;
            if (rvm_chk_sigint != NULL) /* test for interrupt */
                if ((*rvm_chk_sigint)(NULL))
                    goto err_exit;
            /* do log scan if truncation actually needed */
            if (do_truncation) {
                X(do_trunc)
                /* build tree and time */
                kretval = gettimeofday(&tmp_time, (struct timezone *)NULL);
                if (kretval != 0)
                    assert(0); /* return RVM_EIO; */
                X(build_tree)
                if ((retval = build_tree(log)) != RVM_SUCCESS) /* phase 2 */
                    assert(0); /* return retval; */
                X(build_tree done)
                assert(log->trunc_thread == cthread_self());
                assert((status->trunc_state & RVM_TRUNC_PHASES) ==
                       RVM_TRUNC_BUILD_TREE);

                kretval = gettimeofday(&end_time, (struct timezone *)NULL);
                if (kretval != 0)
                    assert(0); /* return RVM_EIO; */
                end_time             = sub_times(&end_time, &tmp_time);
                last_tree_build_time = round_time(&end_time);
                if (rvm_chk_sigint != NULL) /* test for interrupt */
                    if ((*rvm_chk_sigint)(NULL))
                        goto err_exit;

                /* apply tree and time */
                kretval = gettimeofday(&tmp_time, (struct timezone *)NULL);
                if (kretval != 0)
                    assert(0); /* return RVM_EIO; */
                X(apply_mods)
                if ((retval = apply_mods(log)) != RVM_SUCCESS) /* phase 3 */
                    goto err_exit;
                X(apply_mods end)
                assert(log->trunc_thread == cthread_self());
                assert((status->trunc_state & RVM_TRUNC_PHASES) ==
                       RVM_TRUNC_APPLY);
                kretval = gettimeofday(&end_time, (struct timezone *)NULL);
                if (kretval != 0)
                    assert(0); /* return RVM_EIO; */
                end_time             = sub_times(&end_time, &tmp_time);
                last_tree_apply_time = round_time(&end_time);
                if (rvm_chk_sigint != NULL) /* test for interrupt */
                    if ((*rvm_chk_sigint)(NULL))
                        goto err_exit;
            } else
                status->trunc_state =
                    (status->trunc_state & ~RVM_TRUNC_PHASES) | RVM_TRUNC_APPLY;
            X(status_upd)
            /* always update the status */
            retval = status_update(log, new_1st_rec_num); /* phase 4 */
            assert(log->trunc_thread == cthread_self());
            assert((status->trunc_state & RVM_TRUNC_PHASES) ==
                   RVM_TRUNC_UPDATE);
            /* wake up any threads waiting on a truncation */
        err_exit:
            assert(log->trunc_thread == cthread_self());
            CRITICAL(daemon->lock, /* begin daemon->lock crit sec */
                     {
                         assert(log->trunc_thread == cthread_self());
                         if (is_daemon) {
                             assert(log->daemon.thread == cthread_self());
                             assert((status->trunc_state &
                                     RVM_ASYNC_TRUNCATE) != 0);
                             assert(daemon->state == truncating);
                             if (retval != RVM_SUCCESS)
                                 daemon->state = error;
                         }
                         assert(log->trunc_thread == cthread_self());
                     }); /* end daemon->lock crit sec */

            log->trunc_thread   = (cthread_t)NULL;
            status->trunc_state = ZERO;
        }); /* end truncation lock crit sec */

    return retval;
}
#undef X

/* rvm_truncate */
rvm_return_t rvm_truncate()
{
    rvm_return_t retval;

    /* initial checks */
    if (bad_init())
        return RVM_EINIT;
    if (default_log == NULL)
        return RVM_ELOG;

    /* flush any queued records */
    if ((retval = flush_log(default_log, &default_log->status.n_flush)) !=
        RVM_SUCCESS)
        return retval;

    /* do truncation */
    retval = log_recover(default_log, &default_log->status.tot_rvm_truncate,
                         rvm_false, RVM_TRUNCATE_CALL);
    return retval;
}

/* map & flush <--> truncation synchronization functions */

/* initiate asynchronous truncation */
rvm_bool_t
initiate_truncation(log_t *log /* log descriptor */,
                    rvm_length_t threshold /* log % truncation threshold */)
{
    log_daemon_t *daemon = &log->daemon; /* daemon control descriptor */
    rvm_bool_t did_init  = rvm_false; /* true if initiated truncation */

    /* test threshold for asynch truncation */
    if (!daemon->truncate || threshold < daemon->truncate)
        return rvm_false;

    /* trigger a truncation if log at threshold */
    CRITICAL(daemon->lock, /* begin daemon->lock crit sec */
             {
                 /* wake up daemon if idle */
                 if (daemon->state == rvm_idle) {
                     did_init      = rvm_true;
                     daemon->state = truncating;
                     condition_signal(&daemon->code);
                     condition_wait(&daemon->flush_flag, &daemon->lock);
                 }
             }); /* end daemon->lock crit sec */

    return did_init;
}

/* wait until truncation has processed all records up to time_stamp */
rvm_return_t
wait_for_truncation(log_t *log /* log descriptor */,
                    struct timeval *time_stamp /* time threshold */)
{
    log_daemon_t *daemon   = &log->daemon; /* deamon control descriptor */
    log_status_t *status   = &log->status; /* log status descriptor */
    rvm_bool_t force_trunc = rvm_false; /* do synchronous truncation */
    rvm_bool_t exit_sw     = rvm_false;
    rvm_return_t retval    = RVM_SUCCESS;

    while (!exit_sw) {
        CRITICAL(daemon->lock, /* begin daemon lock crit sec */
                 {
                     /* synchronous truncation if daemon not in use */
                     if ((daemon->truncate == 0) ||
                         (daemon->state == rvm_idle)) {
                         force_trunc = rvm_true;
                         goto exit_wait;
                     }

                     /* wait for concurrent truncation completion */
                     while (daemon->state == truncating) {
                         condition_wait(&daemon->wake_up, &daemon->lock);
                     }
                     if (daemon->state == error) {
                         retval = RVM_EINTERNAL; /* quit if daemon had error */
                         goto exit_wait;
                     }

                     /* see if records up to time threshold have been processed */
                     if ((time_stamp == NULL) ||
                         (TIME_GEQ(status->last_trunc, *time_stamp)))
                         goto exit_wait; /* yes, exit */

                     /* no, must trigger another truncation */
                     daemon->state = truncating;
                     condition_signal(&daemon->code);
                     goto exit_crit_sec;

                 exit_wait:
                     exit_sw = rvm_true;
                 exit_crit_sec:;
                 }); /* end daemon lock crit sec */
    }

    /* do synchronous truncation */
    if (force_trunc)
        retval = log_recover(log, &log->status.tot_sync_truncation, rvm_false,
                             RVM_SYNC_TRUNCATE);

    return retval;
}

/* truncation daemon */
void log_daemon(void *arg)
{
    log_t *log           = arg; /* log descriptor */
    log_daemon_t *daemon = &log->daemon; /* deamon control descriptor */
    daemon_state_t state; /* daemon state code */
    rvm_return_t retval __attribute__((unused));

#ifdef RVM_USELWP
    PRE_Concurrent(1);
#endif

    DO_FOREVER
    {
        /* wait to be awakened by request */
        CRITICAL(daemon->lock, /* begin daemon lock crit sec */
                 {
                     if (daemon->state != terminate) {
                         daemon->state = rvm_idle;
                         condition_broadcast(&daemon->wake_up);
                         while (daemon->state == rvm_idle) {
                             condition_wait(&daemon->code, &daemon->lock);
                         }
                     }
                     state = daemon->state; /* end daemon lock crit sec */
                 });

        /* process request */
        switch (state) {
        case truncating: /* do a truncation */
            retval = log_recover(log, &log->status.tot_async_truncation,
                                 rvm_true, RVM_ASYNC_TRUNCATE);

            CRITICAL(daemon->lock, state = daemon->state);
            if (state == error)
                cthread_exit(retval); /* error -- return code */
            if (state != terminate)
                break;

        case terminate:
#ifdef RVM_USELWP
            daemon->thread = NULL;
#endif
            cthread_exit(RVM_SUCCESS); /* normal exit */
            return;

        default:
            assert(rvm_false); /* error */
        }
    }
}
