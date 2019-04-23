/* BLURB lgpl

                           Coda File System
                              Release 7

          Copyright (c) 1987-2019 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

/*
 * Public definitions for the Recoverable Dynamic Storage package.
 */

#ifndef _RDS_H_
#define _RDS_H_

#include <stdio.h>
#include "rvm.h"

/* Error codes */

#define SUCCESS 0
#define ERVM_FAILED -1
#define EBAD_LIST -2
#define EBAD_SEGMENT_HDR -3
#define EHEAP_VERSION_SKEW -4
#define EHEAP_INIT -5
#define EBAD_ARGS -6
#define ECORRUPT -7
#define EFREED_TWICE -8
#define ENO_ROOM -9

/* Function definitions */

int rds_zap_heap(char *DevName, rvm_offset_t DevLength, char *startAddr,
                 rvm_length_t staticLength, rvm_length_t heapLength,
                 unsigned long nlists, unsigned long chunkSize, int *err);

int rds_init_heap(char *base, rvm_length_t length, unsigned long chunkSize,
                  unsigned long nlists, rvm_tid_t *tid, int *err);

int rds_load_heap(char *DevName, rvm_offset_t DevLength, char **staticAddr,
                  int *err);

int rds_unload_heap(int *err);

int rds_start_heap(char *startAddr, int *err);

int rds_stop_heap(int *err);

int rds_prealloc(unsigned long size, unsigned long nblocks, rvm_tid_t *tid,
                 int *err);

char *rds_malloc(unsigned long size, rvm_tid_t *tid, int *err);

int rds_free(char *addr, rvm_tid_t *tid, int *err);

int rds_maxblock(unsigned long size);

/*
 * Because a transaction may abort we don't actually want to free
 * objects until the end of the transaction. So fake_free records our intention
 * to free an object. do_free actually frees the object. It's called as part
 * of the commit.
 */

typedef struct intlist {
    unsigned long size;
    unsigned long count;
    char **table;
} intentionList_t;

#define STARTSIZE 128 /* Initial size of list, may grow over time */

int rds_fake_free(char *addr, intentionList_t *list);

int rds_do_free(intentionList_t *list, rvm_mode_t mode);

/* Heap statistics reporting */
typedef struct {
    unsigned malloc; /* Allocation requests */
    unsigned prealloc; /* Preallocation requests */
    unsigned free; /* Block free requests */
    unsigned coalesce; /* Heap coalesce count */
    unsigned hits; /* No need to split */
    unsigned misses; /* Split required */
    unsigned large_list; /* Largest list pointer changed */
    unsigned large_hits; /* Large blocks present in list */
    unsigned large_misses; /* Large block split required */
    unsigned merged; /* Objects merged from coalesce */
    unsigned unmerged; /* Objects not merged in coalesce */
    unsigned freebytes; /* Number of free bytes in heap */
    unsigned mallocbytes; /* Bytes allocated */
} rds_stats_t;

int rds_print_stats(void);
int rds_clear_stats(int *err);
int rds_get_stats(rds_stats_t *stats);

extern int rds_tracing;
extern FILE *rds_tracing_file;

int rds_trace_on(FILE *);
int rds_trace_off(void);
int rds_trace_dump_heap(void);

#define RDS_LOG(format, a...)                       \
    do {                                            \
        if (rds_tracing && rds_tracing_file) {      \
            fprintf(rds_tracing_file, format, ##a); \
            fflush(rds_tracing_file);               \
        }                                           \
    } while (0);

#endif /* _RDS_H_ */
