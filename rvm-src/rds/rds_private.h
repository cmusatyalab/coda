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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/rvm-src/rds/Attic/rds_private.h,v 4.4 1998/11/02 16:47:39 rvb Exp $";
#endif _BLURB_


/*
 * Internal type definitions for the Recoverable Dynamic Storage package.
 */

#ifdef __STDC__
#include <string.h>
#include "coda_assert.h"
#endif 

#include "rds.h"
#ifndef _RDS_PRIVATE_H_
#define _RDS_PRIVATE_H_

/********************
 *Type definitions
 */

/*
 * Rather than wasting cycles to manage locks which will only produce a
 * small amount of concurrency, We decided to have one mutex on the entire
 * heap. With the exception of coalescing, none of the routines should take
 * substantial time since most are memory changes and simple computations.
 */

/* Synchronization and Threads support */

/* 
 * We can have one of three thread models: 
 *          cthreads:         Mach threads (kernel or coroutine)
 *          lwp:              Coda's lightweight process package
 *          pthreads:         POSIX threads
 *
 * If RVM_USELWP is defined, then lwp support is compiled in.
 * If RVM_USEPT  is defined, then pthreads support is compiled in.
 * If niether of these is defined, then cthreads support is compiled in.
 *
 * It is assumed in the rds package that cthreads and pthreads use
 * preemptive scheduling, and they are synchronized appropriately.
 * 
 * You must define only one of the above targets, and it must be defined
 * consistently across the following packages: RVM, RDS, and URT
 */

#ifndef RVM_USELWP                      /* normal: use Cthreads */
#ifndef RVM_USEPT
#include <cthreads.h>

/* define types symbolically to permit use of non-Cthread thread support */
#define RVM_MUTEX       struct mutex
#define RVM_MUTEX_T	mutex_t
#define RVM_CONDITION	struct condition
#define RVM_CONDITION_T	condition_t

/* macro for testing if a lock is free */
#define LOCK_FREE(lck) \
    (mutex_try_lock(&(lck)) ? (mutex_unlock(&(lck)), rvm_true) : rvm_false)

#endif
#endif

#ifdef RVM_USELWP                       /* special thread support for Coda */
#include "rvm_lwp.h"
#endif 

#ifdef RVM_USEPT                       /* special support for pthreads */
#include "rvm_pthread.h"
#endif

#define LEAVE_CRITICAL_SECTION	goto end_critical;
#define CRITICAL(body)				\
                 mutex_lock(&heap_lock);		\
                 body;				\
end_critical:    mutex_unlock(&heap_lock);

/* Guards detect if the block structure had been illegally overwritten.
 * One is placed after the size, and before user's data. The other is placed
 * at the end of the block. */

#define FREE_GUARD  0x345298af
#define ALLOC_GUARD 0x783bd92c
#define END_GUARD   0xfd10a32e

#define RDS_BLOCK_HDR_SIZE (sizeof(block_size_t) + 2 * sizeof(guard_t))
#define BLOCK_END(bp) ((int *)((char *)(bp) + ((bp)->size * RDS_CHUNK_SIZE)) - 1)

#define USER_BLOCK(bp) ((char *)&((bp)->prev))
#define BLOCK_HDR(bp)  ((free_block_t *)((char *)(bp) - \
			     (sizeof(block_size_t) + sizeof(guard_t))))

typedef unsigned long block_size_t;
typedef unsigned long guard_t;

typedef struct fbt {
    guard_t	 type;
    block_size_t size;
    struct fbt   *prev, *next;
} free_block_t;

#define FREE_LIST_GUARD 0xad938945

typedef struct {
    guard_t	 guard;
    free_block_t *head;
} free_list_t;

#define NEXT_CONSECUTIVE_BLOCK(bp) ((free_block_t *)((char *)(bp) + ((bp)->size * RDS_CHUNK_SIZE)))

#define HEAP_LIST_GROWSIZE 20		/* Number of blocks to prealloc */

#define RDS_HEAP_VERSION "Dynamic Allocator Using Rvm Release 0.1 1 Dec 1990"
#define RDS_VERSION_MAX 80

typedef struct {
    char          version[RDS_VERSION_MAX]; /* Version String */
    unsigned long heaplength;
    unsigned long chunk_size;
    unsigned long nlists;
    rds_stats_t	  stats;		/* statistics on heap usage. */
    unsigned long maxlist;		/* Current non-empty largest list */
    unsigned long dummy[10];		/* Space to allow header to grow */
    free_list_t lists[1];              /* Number of lists is dynamically set */
} heap_header_t;

/* Global data extern declarations. */
extern heap_header_t *RecoverableHeapStartAddress;
extern free_block_t  *RecoverableHeapHighAddress;
extern RVM_MUTEX heap_lock;

extern int rds_tracing;
extern FILE *rds_tracing_file;


#define HEAP_INIT   		(RecoverableHeapStartAddress != 0)
#define RDS_VERSION_STAMP	(RecoverableHeapStartAddress->version)
#define RDS_HEAPLENGTH 		(RecoverableHeapStartAddress->heaplength)
#define RDS_CHUNK_SIZE 		(RecoverableHeapStartAddress->chunk_size)
#define RDS_FREE_LIST  		(RecoverableHeapStartAddress->lists)
#define RDS_NLISTS		(RecoverableHeapStartAddress->nlists)
#define RDS_MAXLIST		(RecoverableHeapStartAddress->maxlist)
#define RDS_STATS		(RecoverableHeapStartAddress->stats)
#define RDS_HIGH_ADDR		(RecoverableHeapHighAddress)

/*******************
 * byte <-> string
 */
#ifdef __STDC__
#define BCOPY(S,D,L)   memcpy((D),(S),(L))
#define BZERO(D,L)     memset((D),0,(L))
#else
#define BCOPY(S,D,L)   bcopy((S),(D),(L))
#define BZERO(D,L)     bzero((D),(L))    
#endif

/********************
 * Definitions of worker functions.
 */
extern int enqueue();
extern free_block_t *dequeue();
extern int print_heap();
extern free_block_t *split();
extern free_block_t *get_block();
extern int put_block();

/*********************
 * Definitions of util functions
 */
free_block_t *dequeue();
int           rm_from_list();

/***********************
 * Coalesce
 */
void coalesce();


#if 0
#ifdef CODA_ASSERT
#undef CODA_ASSERT
#endif
#define CODA_ASSERT(ex) \
    { \
    if (!(ex)) \
        { \
        long _i_ = 0; \
        fflush(stdout); \
        fprintf(stderr,"ASSERTion failed: file \"%s\", line %d\n", \
                __FILE__, __LINE__); \
        fflush(stderr); \
         _i_ = *(long *)_i_; \
        abort(); \
         } \
    }
#endif

#endif _RDS_PRIVATE_H_
