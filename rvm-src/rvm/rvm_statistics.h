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
*                 Definitions for RVM Statistics Structures
*
*/
/*LINTLIBRARY*/

/* permit multiple includes */
#ifndef RVM_STATISTICS_VERSION

#define RVM_STATISTICS_VERSION  "RVM Statistics Version 1.1 8 Dec 1992"

#include <stdio.h>

/* histgram definitions */

#define flush_times_len         10      /* length of flush timing vectors */
#define flush_times_dist                /* timing distribution in millisecs */ \
    25,50,100,250,500,1000,2500,5000,10000 /* use as array initializer */

#define truncation_times_len    5       /* length of truncation timing vectors */
#define truncation_times_dist           /* timing distribution in seconds */ \
    1,10,100,500

#define range_lengths_len       13      /* length of range length histogram */
#define range_lengths_dist              /* range lengths in bytes */ \
    0,4,8,16,32,64,128,256,512,1024,2048,4096

#define range_overlaps_len      13      /* length of overlaps eliminated by
                                           range coalesce */
#define range_overlaps_dist             /* range lengths in bytes */ \
    0,4,8,16,32,64,128,256,512,1024,2048,4096

#define trans_overlaps_len      13      /* length of overlaps eliminated by
                                           transaction coalesce */
#define trans_overlaps_dist             /* range lengths in bytes */ \
    0,4,8,16,32,64,128,256,512,1024,2048,4096

#define range_elims_len          6      /* ranges eliminated from log by
                                           range coalesce */
#define range_elims_dist                /* number of ranges */ \
    0,5,10,50,100

#define trans_elims_len          6      /* ranges eliminated from log by
                                           trans coalesce */
#define trans_elims_dist                /* number of ranges */ \
    0,5,10,50,100

#define trans_coalesces_len     6       /* transactions coalesced by
                                           trans coalesce */
#define trans_coalesces_dist            /* number of transactions */ \
    0,5,10,50,100
/* RVM statistics record */
typedef struct
    {
    rvm_struct_id_t struct_id;          /* self-identifier, do not change */
    rvm_bool_t      from_heap;          /* true if heap allocated;
					   do not change */

                                        /* transaction statistics --
					   current epoch */

    rvm_length_t    n_abort;            /* number of transactions aborted */
    rvm_length_t    n_flush_commit;     /* number of flush mode commits */
    rvm_length_t    n_no_flush_commit;  /* number of no_flush mode commits */
    rvm_length_t    n_uncommit;         /* number of uncommited transactions */
    rvm_length_t    n_no_flush;         /* number of queued no_flush transactions */
    rvm_length_t    n_truncation_wait;  /* total transactions delayed by truncation */
    rvm_offset_t    no_flush_length;    /* length of queued no_flush transactions */
                                        /* log statistics -- current epoch */

    rvm_length_t    n_split;            /* number trans split for log wrap */
    rvm_length_t    n_flush;            /* number of internal flushes */
    rvm_length_t    n_rvm_flush;        /* number of explicit flush calls */
    rvm_length_t    n_special;          /* number of special log records */
    rvm_length_t    n_wrap;             /* number of log wrap-arounds (0 or 1) */
    rvm_length_t    log_dev_cur;        /* current % log device in use */
    rvm_offset_t    log_written;        /* current length of writes to log */
    rvm_offset_t    range_overlap;      /* current overlap eliminated by range coalesce */
    rvm_offset_t    trans_overlap;      /* current overlap eliminated by trans coalesce */
    rvm_length_t    n_range_elim;       /* current number of ranges eliminated by
                                           range coalesce/flush */
    rvm_length_t    n_trans_elim;       /* current number of ranges eliminated by
                                           trans coalesce/flush */
    rvm_length_t    n_trans_coalesced;  /* number of transactions coalesced in
                                           this flush cycle */
    struct timeval  flush_time;         /* time spent in flushes */
    rvm_length_t    last_flush_time;    /* duration of last flush (msec) */
    rvm_length_t    last_truncation_time; /* duration of last truncation (sec) */
    rvm_length_t    last_tree_build_time; /* duration of tree build (sec) */
    rvm_length_t    last_tree_apply_time; /* duration of tree apply phase
                                             (sec) */
                                        /* histogram vectors */

    rvm_length_t    flush_times[flush_times_len]; /* flush timings (msec) */
    rvm_length_t    range_lengths[range_lengths_len]; /* range lengths flushed */
    rvm_length_t    range_elims[range_elims_len]; /* num ranges eliminated by
                                                     range coalesce/flush */
    rvm_length_t    trans_elims[trans_elims_len]; /* num ranges eliminated by
                                                     trans coalesce/flush */
    rvm_length_t    range_overlaps[range_overlaps_len]; /* space saved by
                                                           range coalesce/flush */
    rvm_length_t    trans_overlaps[range_overlaps_len]; /* space saved by
                                                           trans coalesce/flush */
                                        /* transaction stats -- cumulative since log init */

    rvm_length_t    tot_abort;          /* total aborted transactions */
    rvm_length_t    tot_flush_commit;   /* total flush commits */
    rvm_length_t    tot_no_flush_commit; /* total no_flush commits */

                                        /* log stats -- cumulative */

    rvm_length_t    tot_split;          /* total transactions split for log wrap-around */
    rvm_length_t    tot_flush;          /* total internal flush calls  */
    rvm_length_t    tot_rvm_flush;      /* total explicit rvm_flush calls  */
    rvm_length_t    tot_special;        /* total special log records */
    rvm_length_t    tot_wrap;           /* total log wrap-arounds */
    rvm_length_t    log_dev_max;        /* maximum % log device used so far */
    rvm_offset_t    tot_log_written;    /* total length of all writes to log */
    rvm_offset_t    tot_range_overlap;  /* total overlap eliminated by range coalesce */
    rvm_offset_t    tot_trans_overlap;  /* total overlap eliminated by trans coalesce */
    rvm_length_t    tot_range_elim;     /* total number of ranges eliminated by
                                           range coalesce */
    rvm_length_t    tot_trans_elim;     /* total number of ranges eliminated by
                                           trans coalesce */
    rvm_length_t    tot_trans_coalesced; /* total number of transactions coalesced */
                                           
                                        /* truncation stats -- cummulative */

    rvm_length_t    tot_rvm_truncate;   /* total explicit rvm_truncate calls */
    rvm_length_t    tot_async_truncation; /* total asynchronous truncations */
    rvm_length_t    tot_sync_truncation; /* total forced synchronous truncations */
    rvm_length_t    tot_truncation_wait; /* total transactions delayed by truncation */
    rvm_length_t    tot_recovery;       /* total recovery truncations */
    struct timeval  tot_flush_time;     /* total time spent in flush */
    struct timeval  tot_truncation_time; /* cumulative truncation time */

                                        /* histogram vectors */
                                        /* truncation timings (sec) */
    rvm_length_t    tot_tree_build_times[truncation_times_len];
    rvm_length_t    tot_tree_apply_times[truncation_times_len];
    rvm_length_t    tot_truncation_times[truncation_times_len];
                                        /* cummulative flush timings (msec) */
    rvm_length_t    tot_flush_times[flush_times_len];
                                        /* cummulative range lengths */
    rvm_length_t    tot_range_lengths[range_lengths_len];
                                        /* total num ranges eliminated by
                                           range coalesce/flush */
    rvm_length_t    tot_range_elims[range_elims_len];
                                        /* total num ranges eliminated by
                                           trans coalesce/flush */
    rvm_length_t    tot_trans_elims[trans_elims_len];
                                        /* space saved by range coalesce/flush */
    rvm_length_t    tot_range_overlaps[range_overlaps_len];
                                        /* space saved by trans coalesce/flush */
    rvm_length_t    tot_trans_overlaps[range_overlaps_len];
                                        /* transactions coalesced per flush  */
    rvm_length_t    tot_trans_coalesces[trans_coalesces_len];
    }
rvm_statistics_t;
/* get RVM statistics */
extern rvm_return_t rvm_statistics(
    char                *version,       /* pointer to RVM statistics version string */
    rvm_statistics_t    *statistics     /* address of pointer to statistics
                                           descriptor] */
    );
#define RVM_STATISTICS(statistics) \
    rvm_statistics(RVM_STATISTICS_VERSION,(statistics))

/* rvm_statistics_t initializer, copier & finalizer */

extern rvm_statistics_t *rvm_malloc_statistics ();

extern void rvm_init_statistics(rvm_statistics_t *statistics);
extern rvm_statistics_t *rvm_copy_statistics(rvm_statistics_t *statistics); 
extern void rvm_free_statistics(rvm_statistics_t *statistics);

/* rvm_statistics_t printer */
extern rvm_return_t rvm_print_statistics(
    rvm_statistics_t    *statistics,    /* pointer to record to be printed */
    FILE                *out_stream     /* output stream */
    );


#endif /* _RVM_STATISTICS_VERSION */
