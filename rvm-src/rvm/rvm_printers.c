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

static char *rcsid = "$Header: rvm_printers.c,v 1.4 96/11/19 14:25:02 tilt Exp $";
#endif _BLURB_

/*
*
*                       RVM structure printers
*
*/

#include "rvm_private.h"

/* global variables */

extern int          errno;              /* kernel error number */
extern log_t        *default_log;       /* default log descriptor ptr */
extern rvm_bool_t   rvm_utlsw;          /* true if running in rvmutl */
extern char         *rvm_errmsg;        /* internal error message buffer */
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
/* print rvm_offset_t */
static int pr_offset(offset,stream)   
    rvm_offset_t    *offset;
    FILE            *stream;
    {
    int             tot_chars = 0;
    float           flt_val;

    if (RVM_OFFSET_HIGH_BITS_TO_LENGTH(*offset) != 0)
        {
        flt_val = OFFSET_TO_FLOAT(*offset);
        tot_chars = fprintf(stream,"%10.3f",flt_val);
        }
    else
        tot_chars = fprintf(stream,"%10.1lu",RVM_OFFSET_TO_LENGTH(*offset));

    return tot_chars;
    }

/* print time value */
static int pr_timeval(out_stream,timestamp,usec)
    FILE            *out_stream;
    struct timeval  *timestamp;         /* timeval to print */
    rvm_bool_t      usec;               /* print microsecs if true */
    {
    int             err;

    /* print seconds */
    err = fprintf(out_stream,"%10lu",timestamp->tv_sec);
    if (err == EOF) return err;

    /* print usec as decimal fraction after seconds if requested */
    if (usec)
        err = fprintf(out_stream,".%06lu",timestamp->tv_usec);

    return err;
    }
/* histogram value printer -- handles placement of <= for definition values */
static int pr_histo_val(out_stream,val,width,is_def,gtr,us)
    FILE            *out_stream;        /* target stream */
    rvm_length_t    val;                /* histogram value */
    int             width;              /* print width of histogram data */
    rvm_bool_t      is_def;             /* value is bucket size if true */
    rvm_bool_t      gtr;                /* print '> ' if true */
    rvm_bool_t      us;                 /* true if value unsigned */
    {
    char            str[20];            /* string buffer */
    int             pad;
    int             err;

    /* convert value and get width of padding */
    if (us)
        err = (int)sprintf(str,"%lu",val);
    else
        err = (int)sprintf(str,"%ld",val);
    if (err == EOF) return err;
    pad = width - strlen(str);
    if (!is_def) pad += 2;              /* compensate for '<=' */

    /* print padding and relational op */
    err = fprintf(out_stream,"%*c",pad,' ');
    if (err == EOF) return err;
    if (is_def)
        {
        if (gtr)
            err = fprintf(out_stream,"> ");
        else
            err = fprintf(out_stream,"<=");
        }
    if (err == EOF) return err;

    /* print converted value */
    err = fprintf(out_stream,"%s",str);
    return err;
    }
/* histogram printer */
static int pr_histogram(out_stream,histo,histo_def,length,
                        width,leading,gtr,us)
    FILE            *out_stream;        /* target stream */
    rvm_length_t    *histo;             /* histogram data */
    rvm_length_t    *histo_def;         /* histogram bucket sizes */
    rvm_length_t    length;             /* length of histogram vectors */
    int             width;              /* print width of histogram data */
    int             leading;            /* number of leading spaces */
    rvm_bool_t      gtr;                /* print final > bucket if true */
    rvm_bool_t      us;                 /* values unsigned if true */
    {
    int             err;
    rvm_length_t    i;

    /* print buckets */
    err = fprintf(out_stream,"%*c",leading,' ');
    if (err == EOF) return err;

    for (i=0; i<length-1; i++)
        {
        err = pr_histo_val(out_stream,histo_def[i],width,
			   rvm_true,rvm_false,us);
        if (err == EOF) return err;
        }
    if (gtr)
        err = pr_histo_val(out_stream,histo_def[length-2],
                           width,rvm_true,rvm_true,us);
    else
        err = pr_histo_val(out_stream,histo_def[length-1],
                           width,rvm_true,rvm_false,us);
    if (err == EOF) return err;
    err = putc('\n',out_stream);
    if (err == EOF) return err;

    /* print data */
    err = fprintf(out_stream,"%*c",leading,' ');
    if (err == EOF) return err;

    for (i=0; i<length; i++)
        {
        err = pr_histo_val(out_stream,histo[i],
                           width,rvm_false,rvm_false,us);
        if (err == EOF) return err;
        }
    err = putc('\n',out_stream);
    return err;
    }
/* print transaction statistics */
static rvm_return_t pr_trans_stats(stats,out_stream,n_trans,tot_trans)
    rvm_statistics_t    *stats;         /* ptr top statistics record */
    FILE                *out_stream;    /* output stream */
    rvm_length_t        n_trans;
    rvm_length_t        tot_trans;
    {
    rvm_length_t        n_trans_started;
    int                 err;

    /* print header for transaction statistics*/
    err = fprintf(out_stream,
            "Transaction statistics               current %s\n\n",
            "cumulative");
    if (err == EOF) return RVM_EIO;
    n_trans_started = stats->n_flush_commit + stats->n_no_flush_commit
                        + stats->n_uncommit + stats->n_abort;
    err = fprintf(out_stream,
                  "  Started:                        %10ld %10ld\n",
                  n_trans_started,
                  stats->tot_flush_commit+stats->tot_no_flush_commit
                  + stats->tot_abort+n_trans_started);
    if (err == EOF) return RVM_EIO;
    err = fprintf(out_stream,
                  "  Aborted:                        %10ld %10ld\n",
                  stats->n_abort,
                  stats->tot_abort + stats->n_abort);
    if (err == EOF) return RVM_EIO;
    err = fprintf(out_stream,
                  "  Committed, flush:               %10ld %10ld\n",
                  stats->n_flush_commit,
                  stats->tot_flush_commit + stats->n_flush_commit);
    if (err == EOF) return RVM_EIO;
    err = fprintf(out_stream,
                  "  Committed, no_flush:            %10ld %10ld\n",
                  stats->n_no_flush_commit,
                  stats->tot_no_flush_commit+stats->n_no_flush_commit);
    if (err == EOF) return RVM_EIO;
    err = fprintf(out_stream,
                  "  Total committed:                %10ld %10ld\n",
                  stats->n_no_flush_commit+stats->n_flush_commit,
                  stats->tot_no_flush_commit+stats->tot_flush_commit
                  +stats->n_no_flush_commit+stats->n_flush_commit);
    if (err == EOF) return RVM_EIO;
    err = fprintf(out_stream,
                  "  Committed, but not flushed:     %10ld\n",
                  stats->n_no_flush);
    if (err == EOF) return RVM_EIO;
    err = fprintf(out_stream,
                  "  Unflushed transactions length:  %10ld\n",
                  stats->no_flush_length);
    if (err == EOF) return RVM_EIO;
    err = fprintf(out_stream,
                  "  Not committed:                  %10ld\n",
                  stats->n_uncommit);
    if (err == EOF) return RVM_EIO;
    err = fprintf(out_stream,
                  "  Split by log wrap:              %10ld %10ld\n",
                  stats->n_split,
                  stats->tot_split + stats->n_split);
    if (err == EOF) return RVM_EIO;
    err = fprintf(out_stream,
                  "  Delayed by truncation:          %10ld %10ld\n",
                  stats->n_truncation_wait,
                  stats->tot_truncation_wait+stats->n_truncation_wait);
    if (err == EOF) return RVM_EIO;

    return RVM_SUCCESS;
    }
/* print log statistics */
static rvm_return_t pr_log_stats(stats,out_stream,n_trans,tot_trans)
    rvm_statistics_t    *stats;         /* ptr top statistics record */
    FILE                *out_stream;    /* output stream */
    rvm_length_t        n_trans;
    rvm_length_t        tot_trans;
    {
    rvm_length_t    n_flush;
    rvm_length_t    tot_flush;
    rvm_length_t    n_recs;
    rvm_length_t    tot_recs;
    rvm_length_t    tot_truncations;
    rvm_length_t    len_temp1;
    rvm_length_t    len_temp2;
    int             err;

    /* print header for log function statistics*/
    err = fprintf(out_stream,
                  "\nLog function statistics              current %s\n\n",
                  "cumulative");
    if (err == EOF) return RVM_EIO;

    err = fprintf(out_stream,
                  "  rvm_flush calls:                %10ld %10ld\n",
                  stats->n_rvm_flush,
                  stats->tot_rvm_flush + stats->n_rvm_flush);
    if (err == EOF) return RVM_EIO;
    err = fprintf(out_stream,
                  "  Internal flushes, incl. commit: %10ld %10ld\n",
                  stats->n_flush,
                  stats->tot_flush + stats->n_flush);
    n_flush = stats->n_flush + stats->n_rvm_flush;
    tot_flush = stats->tot_flush + stats->tot_rvm_flush + n_flush;
    err = fprintf(out_stream,
                  "  Total flushes:                  %10ld %10ld\n",
                  n_flush,tot_flush);
    if (err == EOF) return RVM_EIO;
    len_temp1 = len_temp2 = 0;
    if (n_flush > 0) len_temp1 =
        (1000*round_time(&stats->flush_time))/n_flush;
    if (tot_flush > 0) len_temp2 =
        (1000*round_time(&stats->tot_flush_time))/tot_flush;
    err = fprintf(out_stream,
                  "  Average flush time (msec):      %10ld %10ld\n",
                  len_temp1,len_temp2);
    if (err == EOF) return RVM_EIO;
    err = fprintf(out_stream,
                  "  Last flush time (msec):         %10ld\n\n",
                  stats->last_flush_time);
    if (err == EOF) return RVM_EIO;
    err = fprintf(out_stream,
                  "  rvm_truncate calls:                        %10ld\n",
            stats->tot_rvm_truncate);
    if (err == EOF) return RVM_EIO;
    err = fprintf(out_stream,
                  "  Automatic truncations:                     %10ld\n",
            stats->tot_async_truncation);
    if (err == EOF) return RVM_EIO;
    err = fprintf(out_stream,
                  "  Forced synch. truncations:                 %10ld\n",
            stats->tot_sync_truncation);
    if (err == EOF) return RVM_EIO;
    err = fprintf(out_stream,
                  "  Recovery truncations:                      %10ld\n",
            stats->tot_recovery);
    if (err == EOF) return RVM_EIO;
    tot_truncations = stats->tot_async_truncation
        + stats->tot_sync_truncation + stats->tot_recovery
        + stats->tot_rvm_truncate;
    err = fprintf(out_stream,
                  "  Total truncations:                         %10ld\n",
                  tot_truncations);
    if (err == EOF) return RVM_EIO;
    len_temp1 = 0;
    if (tot_truncations > 0) len_temp1 =
        round_time(&stats->tot_truncation_time)/tot_truncations;
    err = fprintf(out_stream,
                  "  Average truncation time (sec):             %10ld\n",
                  len_temp1);
    if (err == EOF) return RVM_EIO;
    err = fprintf(out_stream,
                  "  Last truncation time (sec):                %10ld\n",
                  stats->last_truncation_time);
    if (err == EOF) return RVM_EIO;
    err = fprintf(out_stream,
                  "  Last tree build time (sec):                %10ld\n",
                  stats->last_tree_build_time);
    if (err == EOF) return RVM_EIO;
    err = fprintf(out_stream,
                  "  Last tree apply time (sec):                %10ld\n\n",
                  stats->last_tree_apply_time);
    if (err == EOF) return RVM_EIO;
    err = fprintf(out_stream,
                  "  Transaction records:            %10ld %10ld\n",
                  n_trans,
                  tot_trans+n_trans);
    if (err == EOF) return RVM_EIO;
    err = fprintf(out_stream,
                  "  Bookeeping records:             %10ld %10ld\n",
                  stats->n_special,
                  stats->tot_special + stats->n_special);
    if (err == EOF) return RVM_EIO;
    err = fprintf(out_stream,
                  "  Log wrap-arounds:               %10ld %10ld\n",
                  stats->n_wrap,
                  stats->tot_wrap+stats->n_wrap);
    n_recs = n_trans + stats->n_special + stats->n_wrap;
    tot_recs = tot_trans + stats->tot_special + stats->tot_wrap;
    err = fprintf(out_stream,
                  "  Total records:                  %10ld %10ld\n\n",
                  n_recs,
                  tot_recs+n_recs);
    if (err == EOF) return RVM_EIO;
    err = fprintf(out_stream,
                  "  Log used:                       %10ld%%%10ld%%\n",
                  stats->log_dev_cur,
                  stats->log_dev_max);
    if (err == EOF) return RVM_EIO;
    err = fprintf(out_stream,
                  "  Log written (bytes):            ");
    if (err == EOF) return RVM_EIO;
    err = pr_offset(&stats->log_written,out_stream);
    if (err == EOF) return RVM_EIO;
    err = fprintf(out_stream," ");
    if (err == EOF) return RVM_EIO;
    err = pr_offset(&stats->tot_log_written,out_stream);
    if (err == EOF) return RVM_EIO;

    return RVM_SUCCESS;
    }
/* print timing histograms */
static rvm_return_t pr_time_histos(stats,out_stream,n_trans,tot_trans)
    rvm_statistics_t    *stats;         /* ptr top statistics record */
    FILE                *out_stream;    /* output stream */
    rvm_length_t        n_trans;
    rvm_length_t        tot_trans;
    {
    int             err;

    err = fprintf(out_stream,"\n\nTiming Histograms\n");
    if (err == EOF) return RVM_EIO;
    err = fprintf(out_stream,"\n  Current Flush Timings (msec):\n");
    if (err == EOF) return RVM_EIO;
    err = pr_histogram(out_stream,stats->flush_times,flush_times_vec,
                       flush_times_len,6,2,rvm_true,rvm_true);
    if (err == EOF) return RVM_EIO;
    err = fprintf(out_stream,"\n  Cummulative Flush Timings (msec):\n");
    if (err == EOF) return RVM_EIO;
    err = pr_histogram(out_stream,stats->tot_flush_times,flush_times_vec,
                       flush_times_len,6,2,rvm_true,rvm_true);
    if (err == EOF) return RVM_EIO;

    err=fprintf(out_stream,"\n\n  Truncation Timings for Tree Build (sec):\n");
    if (err == EOF) return RVM_EIO;
    err = pr_histogram(out_stream,stats->tot_tree_build_times,
                       truncation_times_vec,truncation_times_len,
                       4,2,rvm_true,rvm_true);
    if (err == EOF) return RVM_EIO;
    err = fprintf(out_stream,"\n  Truncation Timings for Tree Apply (sec):\n");
    if (err == EOF) return RVM_EIO;
    err = pr_histogram(out_stream,stats->tot_tree_apply_times,
                       truncation_times_vec,truncation_times_len,
                       4,2,rvm_true,rvm_true);
    if (err == EOF) return RVM_EIO;
    err = fprintf(out_stream,"\n  Total Truncation Timings (sec):\n");
    if (err == EOF) return RVM_EIO;
    err = pr_histogram(out_stream,stats->tot_truncation_times,
                       truncation_times_vec,truncation_times_len,
                       4,2,rvm_true,rvm_true);
    if (err == EOF) return RVM_EIO;

    return RVM_SUCCESS;
    }
/* print transaction optimization statistics */
static rvm_return_t pr_opt_stats(stats,out_stream,n_trans,tot_trans)
    rvm_statistics_t    *stats;         /* ptr top statistics record */
    FILE                *out_stream;    /* output stream */
    rvm_length_t        n_trans;
    rvm_length_t        tot_trans;
    {
    rvm_length_t        n_flush;
    rvm_length_t        tot_flush;
    rvm_offset_t        off_temp;
    rvm_length_t        len_temp1 = 0;
    rvm_length_t        len_temp2 = 0;
    int                 err;

    err = fprintf(out_stream,
            "\n\nTransaction Optimization Statistics          current %s\n\n",
            "cumulative");
    if (err == EOF) return RVM_EIO;
    n_trans -= stats->n_split;
    tot_trans -= stats->tot_split;
    err = fprintf(out_stream,
                  "  Ranges eliminated\n");
    if (err == EOF) return RVM_EIO;
    err = fprintf(out_stream,
                  "   Range coalesce:                        %10lu %10lu\n",
                  stats->n_range_elim,stats->tot_range_elim);
    if (err == EOF) return RVM_EIO;
    err = fprintf(out_stream,
                  "   Trans coalesce:                        %10lu %10lu\n",
                  stats->n_trans_elim,stats->tot_trans_elim);
    if (err == EOF) return RVM_EIO;
    err = fprintf(out_stream,
                  "    Totals:                               %10lu %10lu\n",
                  stats->n_range_elim+stats->n_trans_elim,
                  stats->tot_range_elim+stats->tot_trans_elim);
    if (err == EOF) return RVM_EIO;
    err = fprintf(out_stream,
                  "  Avg. number eliminated per transaction\n");
    if (err == EOF) return RVM_EIO;
    err = fprintf(out_stream,
                  "   Range coalesce:                        %10ld %10ld\n",
                  (n_trans != 0 ? stats->n_range_elim/n_trans : 0),
                  (tot_trans != 0 ? stats->tot_range_elim/tot_trans : 0));
    if (err == EOF) return RVM_EIO;
    err = fprintf(out_stream,
                  "   Trans coalesce:                        %10ld %10ld\n",
                  (n_trans != 0 ? stats->n_trans_elim/n_trans : 0),
                  (tot_trans != 0 ? stats->tot_trans_elim/tot_trans : 0));
    if (err == EOF) return RVM_EIO;
    err = fprintf(out_stream,
                  "    Totals:                               %10ld %10ld\n",
                  (n_trans != 0
                   ? (stats->n_range_elim+stats->n_trans_elim)/
                      n_trans : 0),
                  (tot_trans != 0
                   ? (stats->tot_range_elim+stats->tot_trans_elim)/
                      tot_trans : 0));
    if (err == EOF) return RVM_EIO;

    err = fprintf(out_stream,
                  "  Range length eliminated\n");
    if (err == EOF) return RVM_EIO;
    off_temp = RVM_ADD_OFFSETS(stats->tot_range_overlap,
                               stats->tot_trans_overlap);
    err = fprintf(out_stream,
                  "   Range coalesce:                        %10lu ",
                  RVM_OFFSET_TO_LENGTH(stats->range_overlap));
    if (err == EOF) return RVM_EIO;
    pr_offset(&stats->tot_range_overlap,out_stream);
    if (err == EOF) return RVM_EIO;
    err = putc('\n',out_stream);
    if (err == EOF) return RVM_EIO;
    err = fprintf(out_stream,
                  "   Trans coalesce:                        %10lu ",
                  RVM_OFFSET_TO_LENGTH(stats->trans_overlap));
    if (err == EOF) return RVM_EIO;
    pr_offset(&stats->tot_trans_overlap,out_stream);
    if (err == EOF) return RVM_EIO;
    err = putc('\n',out_stream);
    if (err == EOF) return RVM_EIO;
    err = fprintf(out_stream,
                  "    Totals:                               %10lu ",
                  RVM_OFFSET_TO_LENGTH(stats->range_overlap)
                  + RVM_OFFSET_TO_LENGTH(stats->trans_overlap));
    if (err == EOF) return RVM_EIO;
    pr_offset(&off_temp,out_stream);
    if (err == EOF) return RVM_EIO;
    err = putc('\n',out_stream);
    if (err == EOF) return RVM_EIO;

    err = fprintf(out_stream,"  Log savings\n");
    if (err == EOF) return RVM_EIO;
    err = fprintf(out_stream,
                  "   Range coalesce:                        %10lu ",
                  RVM_OFFSET_TO_LENGTH(stats->range_overlap)
                  + stats->n_range_elim*sizeof(nv_range_t));
    if (err == EOF) return RVM_EIO;
    off_temp =
        RVM_ADD_LENGTH_TO_OFFSET(stats->tot_range_overlap,
                                 stats->tot_range_elim*sizeof(nv_range_t));
    pr_offset(&off_temp,out_stream);
    if (err == EOF) return RVM_EIO;
    err = putc('\n',out_stream);
    if (err == EOF) return RVM_EIO;
    err = fprintf(out_stream,
                  "   Trans coalesce:                        %10lu ",
                  RVM_OFFSET_TO_LENGTH(stats->trans_overlap)
                  + stats->n_trans_elim*sizeof(nv_range_t));
    if (err == EOF) return RVM_EIO;
    off_temp =
        RVM_ADD_LENGTH_TO_OFFSET(stats->tot_trans_overlap,
                                 stats->tot_trans_elim*sizeof(nv_range_t));
    pr_offset(&off_temp,out_stream);
    if (err == EOF) return RVM_EIO;
    err = putc('\n',out_stream);
    if (err == EOF) return RVM_EIO;
    err = fprintf(out_stream,
                  "    Totals:                               %10lu ",
                  RVM_OFFSET_TO_LENGTH(stats->trans_overlap)
                  + stats->n_trans_elim*sizeof(nv_range_t)
                  + RVM_OFFSET_TO_LENGTH(stats->range_overlap)
                  + stats->n_range_elim*sizeof(nv_range_t));
    if (err == EOF) return RVM_EIO;
    off_temp =
        RVM_ADD_LENGTH_TO_OFFSET(off_temp,
                                 stats->tot_range_elim*sizeof(nv_range_t));
    off_temp = RVM_ADD_OFFSETS(stats->tot_range_overlap,off_temp);
    pr_offset(&off_temp,out_stream);
    if (err == EOF) return RVM_EIO;
    err = putc('\n',out_stream);
    if (err == EOF) return RVM_EIO;

    n_flush = stats->n_flush + stats->n_rvm_flush;
    tot_flush = stats->tot_flush + stats->tot_rvm_flush + n_flush;
    if (n_flush > 0)
        len_temp1 = stats->n_trans_coalesced/n_flush;
    if (tot_flush > 0)
        len_temp2 = stats->tot_trans_coalesced/tot_flush;
    err = fprintf(out_stream,
                  "  Transactions coalesced per flush:       %10lu %10lu\n",
                  len_temp1,len_temp2);
    if (err == EOF) return RVM_EIO;

    return RVM_SUCCESS;
    }
/* print transaction optimization histograms */
static rvm_return_t pr_opt_histos(stats,out_stream,n_trans,tot_trans)
    rvm_statistics_t    *stats;         /* ptr top statistics record */
    FILE                *out_stream;    /* output stream */
    rvm_length_t        n_trans;
    rvm_length_t        tot_trans;
    {
    int                 err;
    int                 i;
    rvm_length_t        overlaps_totals_vec[range_overlaps_len];
    rvm_length_t        elims_totals_vec[range_elims_len];

    err=fprintf(out_stream,
                "\n\nTranasction Modification Range Distributions\n\n");
    if (err == EOF) return RVM_EIO;
    err=fprintf(out_stream,"  Current Range Lengths (bytes):\n");
    if (err == EOF) return RVM_EIO;
    err = pr_histogram(out_stream,stats->range_lengths,
                       range_lengths_vec,7,10,2,rvm_false,rvm_true);
    if (err == EOF) return RVM_EIO;
    err = putc('\n',out_stream);
    if (err == EOF) return RVM_EIO;
    err = pr_histogram(out_stream,&stats->range_lengths[7],
                       &range_lengths_vec[7],range_lengths_len-7,
                       10,2,rvm_true,rvm_true);
    if (err == EOF) return RVM_EIO;
    err=fprintf(out_stream,"\n  Cumulative Range Lengths (bytes):\n");
    if (err == EOF) return RVM_EIO;
    err = pr_histogram(out_stream,stats->tot_range_lengths,
                       range_lengths_vec,7,10,2,rvm_false,rvm_true);
    if (err == EOF) return RVM_EIO;
    err = putc('\n',out_stream);
    if (err == EOF) return RVM_EIO;
    err = pr_histogram(out_stream,&stats->tot_range_lengths[7],
                       &range_lengths_vec[7],range_lengths_len-7,
                       10,2,rvm_true,rvm_true);
    if (err == EOF) return RVM_EIO;
    err=fprintf(out_stream,"\n\n  %s (bytes):\n",
                "Current Range Lengths Eliminated");
    if (err == EOF) return RVM_EIO;
    err=fprintf(out_stream,
                "   Range coalesce:\n");
    if (err == EOF) return RVM_EIO;
    err = pr_histogram(out_stream,stats->range_overlaps,
                       range_overlaps_vec,7,10,2,rvm_false,rvm_true);
    if (err == EOF) return RVM_EIO;
    err = putc('\n',out_stream);
    if (err == EOF) return RVM_EIO;
    err = pr_histogram(out_stream,&stats->range_overlaps[7],
                       &range_overlaps_vec[7],range_overlaps_len-7,
                       10,2,rvm_true,rvm_true);
    if (err == EOF) return RVM_EIO;

    err=fprintf(out_stream,
                "   Trans coalesce:\n");
    if (err == EOF) return RVM_EIO;
    err = pr_histogram(out_stream,stats->trans_overlaps,
                       trans_overlaps_vec,7,10,2,rvm_false,rvm_true);
    if (err == EOF) return RVM_EIO;
    err = putc('\n',out_stream);
    if (err == EOF) return RVM_EIO;
    err = pr_histogram(out_stream,&stats->trans_overlaps[7],
                       &trans_overlaps_vec[7],trans_overlaps_len-7,
                       10,2,rvm_true,rvm_true);
    if (err == EOF) return RVM_EIO;

    err=fprintf(out_stream,
                "    Totals:\n");
    if (err == EOF) return RVM_EIO;
    for (i=0;i<range_overlaps_len;i++)
        overlaps_totals_vec[i] = stats->range_overlaps[i]
                                 + stats->trans_overlaps[i];
    err = pr_histogram(out_stream,overlaps_totals_vec,
                       range_overlaps_vec,7,10,2,rvm_false,rvm_true);
    if (err == EOF) return RVM_EIO;
    err = putc('\n',out_stream);
    if (err == EOF) return RVM_EIO;
    err = pr_histogram(out_stream,&overlaps_totals_vec[7],
                       &range_overlaps_vec[7],range_overlaps_len-7,
                       10,2,rvm_true,rvm_true);
    if (err == EOF) return RVM_EIO;

    err=fprintf(out_stream,"\n  %s (bytes):\n",
                "Cumulative Range Lengths Eliminated");
    if (err == EOF) return RVM_EIO;
    err=fprintf(out_stream,
                "   Range coalesce:\n");
    if (err == EOF) return RVM_EIO;
    err = pr_histogram(out_stream,stats->tot_range_overlaps,
                       range_overlaps_vec,7,10,2,rvm_false,rvm_true);
    if (err == EOF) return RVM_EIO;
    err = putc('\n',out_stream);
    if (err == EOF) return RVM_EIO;
    err = pr_histogram(out_stream,&stats->tot_range_overlaps[7],
                       &range_overlaps_vec[7],range_overlaps_len-7,
                       10,2,rvm_true,rvm_true);
    if (err == EOF) return RVM_EIO;

    err=fprintf(out_stream,
                "   Trans coalesce:\n");
    if (err == EOF) return RVM_EIO;
    err = pr_histogram(out_stream,stats->tot_trans_overlaps,
                       trans_overlaps_vec,7,10,2,rvm_false,rvm_true);
    if (err == EOF) return RVM_EIO;
    err = putc('\n',out_stream);
    if (err == EOF) return RVM_EIO;
    err = pr_histogram(out_stream,&stats->tot_trans_overlaps[7],
                       &trans_overlaps_vec[7],trans_overlaps_len-7,
                       10,2,rvm_true,rvm_true);
    if (err == EOF) return RVM_EIO;

    err=fprintf(out_stream,
                "    Totals:\n");
    if (err == EOF) return RVM_EIO;
    for (i=0;i<range_overlaps_len;i++)
        overlaps_totals_vec[i] = stats->tot_range_overlaps[i]
                                 + stats->tot_trans_overlaps[i];
    err = pr_histogram(out_stream,overlaps_totals_vec,
                       range_overlaps_vec,7,10,2,rvm_false,rvm_true);
    if (err == EOF) return RVM_EIO;
    err = putc('\n',out_stream);
    if (err == EOF) return RVM_EIO;
    err = pr_histogram(out_stream,&overlaps_totals_vec[7],
                       &range_overlaps_vec[7],range_overlaps_len-7,
                       10,2,rvm_true,rvm_true);
    if (err == EOF) return RVM_EIO;

    err=fprintf(out_stream,
                "\n  Current Number of Eliminated Ranges per Transaction\n");
    if (err == EOF) return RVM_EIO;
    err=fprintf(out_stream,
                "   Range coalesce:\n");
    if (err == EOF) return RVM_EIO;
    err = pr_histogram(out_stream,stats->range_elims,range_elims_vec,
                       range_elims_len,10,2,rvm_true,rvm_true);
    if (err == EOF) return RVM_EIO;
    err=fprintf(out_stream,
                "   Trans coalesce:\n");
    if (err == EOF) return RVM_EIO;
    err = pr_histogram(out_stream,stats->trans_elims,trans_elims_vec,
                       trans_elims_len,10,2,rvm_true,rvm_true);
    if (err == EOF) return RVM_EIO;
    err=fprintf(out_stream,
                "    Totals:\n");
    if (err == EOF) return RVM_EIO;
    for (i=0;i<range_elims_len;i++)
        elims_totals_vec[i] = stats->range_elims[i]
                              + stats->trans_elims[i];
    err = pr_histogram(out_stream,elims_totals_vec,
                       range_elims_vec,range_elims_len,10,2,rvm_true,rvm_true);
    if (err == EOF) return RVM_EIO;

    err=fprintf(out_stream,"\n  %s\n",
                "Cummulative Number of Eliminated Ranges per Transaction");
    if (err == EOF) return RVM_EIO;
    err=fprintf(out_stream,
                "   Range coalesce:\n");
    if (err == EOF) return RVM_EIO;
    err = pr_histogram(out_stream,stats->tot_range_elims,
                       range_elims_vec,range_elims_len,10,2,rvm_true,rvm_true);
    if (err == EOF) return RVM_EIO;
    err=fprintf(out_stream,
                "   Trans coalesce:\n");
    if (err == EOF) return RVM_EIO;
    err = pr_histogram(out_stream,stats->tot_trans_elims,
                       trans_elims_vec,trans_elims_len,10,2,rvm_true,rvm_true);
    if (err == EOF) return RVM_EIO;
    err=fprintf(out_stream,
                "    Totals:\n");
    if (err == EOF) return RVM_EIO;
    for (i=0;i<range_elims_len;i++)
        elims_totals_vec[i] = stats->tot_range_elims[i]
                              + stats->tot_trans_elims[i];
    err = pr_histogram(out_stream,elims_totals_vec,
                       range_elims_vec,range_elims_len,10,2,rvm_true,rvm_true);
    if (err == EOF) return RVM_EIO;

    err=fprintf(out_stream,"\n  %s\n",
                "Cummulative Number of Transactions Coalesced per flush Cycle");
    if (err == EOF) return RVM_EIO;
    err = pr_histogram(out_stream,stats->tot_trans_coalesces,
                       trans_coalesces_vec,trans_coalesces_len,10,2,
		       rvm_true,rvm_true);
    if (err == EOF) return RVM_EIO;
    err = putc('\n',out_stream);
    if (err == EOF) return RVM_EIO;

    return RVM_SUCCESS;
    }
/* rvm_print_stats */
rvm_return_t rvm_print_statistics(stats,out_stream)
    rvm_statistics_t    *stats;         /* ptr top statistics record */
    FILE                *out_stream;    /* output stream */
    {
    rvm_length_t    n_trans;
    rvm_length_t    tot_trans;
    rvm_return_t    retval;

    /* initial checks */
    if (bad_init()) return RVM_EINIT;
    if (default_log == NULL) return RVM_ELOG;
    if (stats == NULL) return RVM_ESTATISTICS;
    if ((retval=bad_statistics(stats)) != RVM_SUCCESS)
        return retval;

    /* global totals */
    n_trans = stats->n_flush_commit+stats->n_no_flush_commit
                +stats->n_split;
    tot_trans = stats->tot_flush_commit+stats->tot_no_flush_commit
                +stats->tot_split;

    /* print transaction statistics */
    if ((retval=pr_trans_stats(stats,out_stream,n_trans,tot_trans))
        != RVM_SUCCESS) return retval;

    /* print log statistics */
    if ((retval=pr_log_stats(stats,out_stream,n_trans,tot_trans))
        != RVM_SUCCESS) return retval;

    /* timing histogram printing */
    if ((retval=pr_time_histos(stats,out_stream,n_trans,tot_trans))
        != RVM_SUCCESS) return retval;

    /* transaction optimization statistics */
    if ((retval=pr_opt_stats(stats,out_stream,n_trans,tot_trans))
        != RVM_SUCCESS) return retval;

    /* transaction optimization histograms */
    if ((retval=pr_opt_histos(stats,out_stream,n_trans,tot_trans))
        != RVM_SUCCESS) return retval;

    return RVM_SUCCESS;
    }
