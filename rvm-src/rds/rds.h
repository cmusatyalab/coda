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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/rvm-src/rds/Attic/rds.h,v 4.2 1998/08/26 15:40:11 braam Exp $";
#endif _BLURB_

/*
 * Public definitions for the Recoverable Dynamic Storage package.
 */

#ifndef	_RDS_H_
#define	_RDS_H_

#include "rvm.h"

/* Error codes */

#define SUCCESS             0
#define ERVM_FAILED         -1
#define EBAD_LIST           -2
#define EBAD_SEGMENT_HDR    -3
#define EHEAP_VERSION_SKEW  -4
#define EHEAP_INIT          -5
#define EBAD_ARGS           -6
#define ECORRUPT            -7
#define EFREED_TWICE        -8
#define ENO_ROOM            -9

/* Function definitions */

extern int rds_zap_heap(
      char 	            *DevName,
      rvm_offset_t          DevLength,
      char                  *startAddr,
      rvm_length_t          staticLength,
      rvm_length_t          heapLength,
      unsigned long         nlists,
      unsigned long         chunkSize,
      int                   *err
     );

extern int rds_init_heap(
      char                  *base,
      rvm_length_t          length,
      unsigned long         chunkSize,
      unsigned long         nlists,
      rvm_tid_t             *tid,
      int                   *err
    );

extern int rds_load_heap(
      char                  *DevName,
      rvm_offset_t          DevLength,
      char                  **staticAddr,
      int                   *err
    );

extern int rds_start_heap(
      char                  *startAddr,
      int                   *err
    );

extern int rds_prealloc(
      unsigned long         size,
      unsigned long         nblocks,
      rvm_tid_t             *tid,
      int                   *err
    );

extern char *rds_malloc(
      unsigned long size,
      rvm_tid_t             *tid,
      int                   *err
    );

extern int rds_free(
      char                  *addr,
      rvm_tid_t             *tid,
      int                   *err
    );

/*
 * Because a transaction may abort we don't actually want to free
 * objects until the end of the transaction. So fake_free records our intention
 * to free an object. do_free actually frees the object. It's called as part
 * of the commit.
 */

typedef struct intlist {
    unsigned long           size;
    unsigned long           count;
    char                    **table;
} intentionList_t;

#define STARTSIZE 128   /* Initial size of list, may grow over time */

extern int rds_fake_free(
      char                 *addr,
      intentionList_t      *list
    );

extern int rds_do_free(
      intentionList_t       *list,
      rvm_mode_t            mode
    );

/* Heap statistics reporting */
typedef struct {
    unsigned            malloc;         /* Allocation requests */
    unsigned            prealloc;       /* Preallocation requests */
    unsigned            free;           /* Block free requests */
    unsigned            coalesce;       /* Heap coalesce count */
    unsigned            hits;           /* No need to split */
    unsigned            misses;         /* Split required */
    unsigned            large_list;     /* Largest list pointer changed */
    unsigned            large_hits;     /* Large blocks present in list */
    unsigned            large_misses;   /* Large block split required */
    unsigned            merged;         /* Objects merged from coalesce */
    unsigned            unmerged;       /* Objects not merged in coalesce */
    unsigned            freebytes;      /* Number of free bytes in heap */
    unsigned            mallocbytes;    /* Bytes allocated */
} rds_stats_t;

extern int rds_print_stats();
extern int rds_clear_stats(int *err);
extern int rds_get_stats(rds_stats_t *stats);  

extern int rds_tracing;
extern FILE *rds_tracing_file;
extern int rds_trace_on(FILE *);
extern int rds_trace_off();
extern int rds_trace_dump_heap();
#define RDS_LOG(format, a...)\
  do {                       \
  if (rds_tracing && rds_tracing_file) { \
    fprintf(rds_tracing_file, format, ## a);\
    fflush(rds_tracing_file); }\
} while (0) ;                            


#endif _RDS_H_
