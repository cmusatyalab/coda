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

static char *rcsid = "$Header: /afs/cs.cmu.edu/project/coda-braam/src/coda-4.0.1/rvm-src/tests/RCS/rvm_basher.c,v 1.1 1996/11/22 19:17:25 braam Exp braam $";
#endif _BLURB_

/*
*
*			RVM Stress Test Program
*
*/


#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "rvm.h"
#include "rvm_statistics.h"
#include "rvm_segment.h"
#include "rds.h"

#ifndef RVM_MAJOR_VERSION
#define RVM_MAJOR_VERSION     1
#endif  RVM_MAJOR_VERSION
#ifndef RVM_MINOR_VERSION
#define RVM_MINOR_VERSION     3
#endif  RVM_MINOR_VERSION
#if ((RVM_MAJOR_VERSION >= 2) && (RVM_MINOR_VERSION >= 0))
#include "rvm_query.h"
#endif VERSION_TEST

#ifndef RVM_USELWP
/* XXX bogus by Eric and Peter to get compile */
#include <dummy_cthreads.h>

/* define types symbolically to permit use of non-Cthread thread support */
#define RVM_MUTEX       struct mutex
#define RVM_MUTEX_T	mutex_t
#define RVM_CONDITION	struct condition
#define RVM_CONDITION_T	condition_t

#else RVM_USELWP                        /* special thread support for Coda */
#include "rvm_lwp.h"
#endif RVM_USELWP

extern int errno;
#ifdef LINUX
extern char *sys_errlist[]; /* XXX JET MUCKING */
#else
extern const char *const sys_errlist[]; /* XXX JET MUCKING */
#endif
extern int sys_nerr;

extern rvm_region_def_t *RegionDefs;    /* hooks to rds */
extern long NRegionDefs;
extern rvm_bool_t rds_testsw;                 /* special test mode switch */

extern void clear_free_lists();
/* ASSERT that preserves stack */
#ifdef ASSERT
#undef ASSERT
#endif ASSERT
#define ASSERT(ex) \
    { \
    if (!(ex)) \
        { \
        long _i_ = 0; \
        fprintf(stderr,"ASSERTion failed: file \"%s\", line %d\n", \
                __FILE__, __LINE__); \
        fflush(stderr); \
         _i_ = *(long *)_i_; \
        abort(); \
         } \
    }

#undef CRITICAL
#define CRITICAL(lock,body) {mutex_lock(&lock); {body;} mutex_unlock(&lock);}

typedef struct list_entry_s
    {
    struct list_entry_s *nextentry;	/* in accordance with insque(3) */
    struct list_entry_s *preventry;
    union
        {
        struct list_entry_s  *name;     /* back pointer to head of list */
        int                  length;    /* length of list if header */
        }               list;
    rvm_bool_t          is_header;      /* true if list header */
    }
list_entry_t;

typedef struct
    {
    list_entry_t    links;              /* allocated list links */
    RVM_MUTEX       lock;               /* lock for access to data area */
    int             size;               /* usable size of block */
    rvm_length_t    chksum;             /* block checksum */
    char            *ptr;               /* ptr to rds allocated space */
    }
block_t;
/* system variables */
#define CMD_MAX     2048                /* maximum cmd line length */
static char         cmd_line[CMD_MAX];  /* command line buffer */
static char         *cmd_cur;           /* ptr to current command line
                                           position */

list_entry_t        alloc_list;         /* list of allocated blocks */
RVM_MUTEX           alloc_list_lock;    /* protects list header & block links */

int		    nthreads = 0;       /* Number of threads */
cthread_t	    *threads;           /* thread handles vector */
int                 *last;              /* last operation of thread */
rvm_bool_t          Terminate = rvm_false; /* thread exit switch */
RVM_MUTEX           print_lock;         /* syncronize worker start msgs */
RVM_MUTEX           thread_lock = MUTEX_INITIALIZER; /* lock for thread exit code */
RVM_CONDITION       thread_exit_code;   /* code to signal termination to main */

RVM_MUTEX           chk_lock = MUTEX_INITIALIZER; /* segment checker variables */
rvm_bool_t          chk_block;          /* require threads to block for chk */
RVM_CONDITION       blk_code;           /* blocked threads wait code */
RVM_CONDITION       chk_code;           /* checker wait code */
int                 blk_cnt;            /* counter for blocked threads */
rvm_length_t        optimizations;      /* optimizations used in test */
int                 max_cycles;         /* maximum operation loop cycles */
int                 max_ranges;         /* maximum number ranges/trans */
int                 max_trans;          /* max number trans per modify test */
int                 max_block_size;     /* maximum modification block size */
int                 max_mod_size;       /* maximum modification size */
int                 max_moby_size;      /* maximum moby modification size */
int                 num_pre_alloc;      /* number of blocks to preallocate */
int                 cycle;              /* operation loop cycle count */
int                 op_cnt;             /* basher operation count */
int                 max_op_cnt;         /* operation count threshold */
int                 chk_range_frac;     /* fraction of ranges to check */
int                 abort_frac;         /* fraction of trans to abort */
int                 restore_frac;       /* fraction of trans to begin in
                                           restore mode and commit */
int                 flush_frac;         /* fraction of trans to commit/flush  */
int                 trunc_frac;         /* truncation threshold */
int                 epoch_trunc_frac;   /* epoch truncation threshold */
int                 incr_trunc_time;    /* incr trunc run time slice */
int                 seed;               /* random num gen seed */
int                 chk_alloc;          /* allocation checking level */
rvm_offset_t	    DataLen;            /* length of data raw partition */
char                DataFileName[MAXPATHLEN+1]; /* heap segment file name */
char                LogFileName[MAXPATHLEN+1]; /* log file name */
char                PlumberFileName[MAXPATHLEN+1]; /* plumber file name */
FILE                *PlumberFile;       /* file descriptor for plumber file */
struct timeval      init_time;          /* time when test run started */
rvm_length_t        rec_buf_len;        /* recovery buffer length */
rvm_length_t        flush_buf_len;      /* flush buffer length */
rvm_length_t        max_read_len;       /* maximum read length */
rvm_bool_t          all_tests;          /* do all tests */
rvm_bool_t          time_tests;         /* timestamp op loop if true */
rvm_bool_t          vm_test;            /* compare vm with segment if true */
rvm_bool_t          moby_test;          /* do large range test if true */
rvm_bool_t          chk_sum_sw;         /* request checksumming */
rvm_bool_t          no_yield_sw;        /* request no-yield truncations */
rvm_bool_t          vm_protect_sw;      /* request vm buffer protection */
rvm_bool_t          pre_alloc_trunc;    /* truncate after preallocation */
rvm_bool_t          show_brk;           /* print break point */
/* string name lookup entry declarations */

typedef enum
    {
    UNKNOWN = -123456789,               /* unknown string key */

    ALL_KEY = 1,                        /* key for do all entities */
    ABORT_KEY,                          /* set abort fraction key */
    BLOCK_SIZE_KEY,                     /* allocation block size key */
    BRK_KEY,                            /* show break point key */
    CHK_ALLOC_KEY,                      /* allocation checker key */
    CHK_SUM_KEY,                        /* rvm_chk_sum key */
    CYCLES_KEY,                         /* number of cycles key */
    DATA_KEY,                           /* set data file key */
    FLUSH_KEY,                          /* set flush fraction key */
    FLUSH_BUF_KEY,                      /* set flush buffer length key */
    INCR_KEY,                           /* enable incrmental truncation key */
    LOG_KEY,                            /* set log file key */
    MAX_READ_KEY,                       /* set max read length key */
    MOBY_KEY,                           /* moby range test key */
    MOBY_MAX_KEY,                       /* maximum moby range size key */
    MOD_SIZE_KEY,                       /* maximum modification size key */
    NONE_KEY,                           /* no choices key */
    OPS_KEY,                            /* number of operations/cycle key */
    OPT_KEY,                            /* set optimizations key */
    PARMS_KEY,                          /* show test parameters key */
    PLUMBER_KEY,                        /* setup plumber file key */
    PRE_ALLOC_KEY,                      /* number of blocks to preallocate */
    PRE_ALLOC_TRUNC_KEY,                /* truncate after preallocation */
    PRIORITY_KEY,                       /* set run priority */
    QUIT_KEY,                           /* quit program key */
    RANGES_KEY,                         /* set num ranges key */
    REC_BUF_KEY,                        /* set recovery buffer length */
    RESTORE_KEY,                        /* set restore fraction key */
    RUN_KEY,                            /* start tests key */
    SEED_KEY,                           /* random num gen key */
    THREADS_KEY,                        /* set num threads key */
    TIME_KEY,                           /* timestamp cycle key */
    TRANS_KEY,                          /* set num trans key */
    TRUNC_KEY,                          /* set truncation fraction key */
    EPOCH_TRUNC_KEY,                    /* set epoch truncation fraction key */
    INCR_TRUNC_TIME_KEY,                /* set incr trunc time slice key */
    VM_KEY,                             /* key for vm check */
    CHK_RANGE_KEY,                      /* set chk_range fraction key */
    NO_YIELD_KEY,                       /* set no_yield truncation mode */
    VM_PROT_KEY                         /* request vm buffer protection */
    }
key_id_t;

#define STR_NAME_LEN 31                 /* maximum length of string name */

typedef struct                          /* string name vector entry */
    {
    char            str_name[STR_NAME_LEN+1];  /* character string for name */
    key_id_t        key;
    }
str_name_entry_t;

/* test parameter defaults */

#define             FLUSH_DEFAULT       20
#define             ABORT_DEFAULT       10
#define             RESTORE_DEFAULT     20
#define             TRANS_DEFAULT       10
#define             RANGES_DEFAULT      100
#define             CYCLES_DEFAULT      -1
#define             OP_CNT_DEFAULT      1000
#define             THREADS_DEFAULT     25
#define             BLOCK_SIZE_DEFAULT  10000
#define             MOD_SIZE_DEFAULT    10000
#define             MOBY_MOD_SIZE_DEFAULT 1000000
#define             PRE_ALLOC_DEFAULT   1000
#define             SEED_DEFAULT        1
#define             CHK_ALLOC_DEFAULT   0
#define             CHK_RANGE_DEFAULT   0
/* list maintainance functions */

/* list header initializer */
void init_list_head(whichlist)
    list_entry_t    *whichlist;         /* pointer to list header */
    {
    whichlist->nextentry = whichlist;   /* pointers are to self now */
    whichlist->preventry = whichlist;
    whichlist->list.length = 0;         /* list is empty */
    whichlist->is_header = rvm_true;    /* mark header */
    }

/* list entry initializer */
void init_list_ent(entry)
    list_entry_t    *entry;
    {
    entry->nextentry = NULL;
    entry->preventry = NULL;
    entry->list.name = NULL;
    entry->is_header = rvm_false;
    }
/* list entry mover */
list_entry_t *move_list_ent(fromptr, toptr, victim)
    register list_entry_t *fromptr;     /* from list header */
    register list_entry_t *toptr;       /* to list header */
    register list_entry_t *victim;      /* pointer to entry to be moved */
    {

    if (fromptr != NULL)
        {
        ASSERT(fromptr->is_header);
        if ((victim == NULL) && (fromptr->list.length == 0))
            return NULL;
        else
            {
            if (victim == NULL)         /* choose 1st if no victim */
                victim = fromptr->nextentry;
            ASSERT(!victim->is_header);
            ASSERT(victim->list.name == fromptr);
            remque(victim);             /* unlink from first list */
            fromptr->list.length --;
            }
        }
    else
        {
        ASSERT(victim != NULL);
        ASSERT(!victim->is_header);
        ASSERT(toptr != NULL);
        }

    if (toptr != NULL)
        {
        ASSERT(toptr->is_header);
        victim->list.name = toptr;
        insque(victim,toptr->preventry); /* insert at tail of second list */
        toptr->list.length ++;
        }
    else victim->list.name = NULL;

    return victim;
    }
/* Byte-aligned checksum and move functions */

#define LENGTH_MASK          ((rvm_length_t)(~(sizeof(rvm_length_t)-1)))
#define ROUND_TO_LENGTH(len) (((rvm_length_t)((rvm_length_t)(len) \
                                              +sizeof(rvm_length_t)-1)) \
                              & LENGTH_MASK)
#define CHOP_TO_LENGTH(len)  ((rvm_length_t)((rvm_length_t)(len) \
                                             & LENGTH_MASK))
#define ALIGNED_LEN(addr,len) (ROUND_TO_LENGTH((rvm_length_t)(addr) \
                                               +(rvm_length_t)(len)) \
                               - CHOP_TO_LENGTH(addr))
#define BYTE_SKEW(len)       ((rvm_length_t)(len) & ~LENGTH_MASK)

/* zero-pad unused bytes of word */
rvm_length_t zero_pad(word,addr,leading)
    rvm_length_t    word;               /* value to be zero-padded */
    char            *addr;              /* address of 1st/last byte */
    rvm_bool_t      leading;            /* true if leading bytes are zeroed */
    {
    char            *word_array = (char *)&word; /* byte access of word value */
    int             skew = BYTE_SKEW(addr);
    int             i;

    if (leading)                        /* zero pad leading bytes */
        {
        for (i=sizeof(rvm_length_t)-1; i>0; i--)
            if (i <= skew) word_array[i-1] = 0;
        }
    else                                /* zero pad trailing bytes */
        {
        for (i=0; i<(sizeof(rvm_length_t)-1); i++)
          if (i >= skew) word_array[i+1] = 0;
        }

    return word;
    }
/* checksum function: forms checksum of arbitrarily aligned range
   by copying preceeding, trailing bytes to make length 0 mod length size */
rvm_length_t check_sum(nvaddr,len)
    char            *nvaddr;            /* address of 1st byte */
    rvm_length_t    len;                /* byte count */
    {
    rvm_length_t    *base;              /* 0 mod sizeof(rvm_length_t) addr */
    rvm_length_t    length;             /* number of words to sum */
    rvm_length_t    chk_sum;            /* check sum temp */
    rvm_length_t    i;

    if (len == 0) return 0;

    /* get zero-byte aligned base address & length of region */
    base = (rvm_length_t *)CHOP_TO_LENGTH(nvaddr);
    length = (ALIGNED_LEN(nvaddr,len)/sizeof(rvm_length_t)) - 1;

    /* process boundary words */
    chk_sum = zero_pad_word(*base,nvaddr,rvm_true);
    if (length >= 2)
        chk_sum += zero_pad_word(base[length--],&nvaddr[len-1],rvm_false);
    if (length <= 1) return chk_sum;

    /* form check sum of remaining full words */
    for (i=1; i < length; i++)
        chk_sum += base[i];

    return chk_sum;
    }
/* test block check sum */
rvm_bool_t test_chk_sum(block)
    block_t         *block;
    {
    rvm_length_t    chksum;

    chksum = check_sum(block->ptr,block->size);
    if (chksum != block->chksum)
        {
        printf("\n?  Block checksum doesn't match\n");
        ASSERT(rvm_false);
        }

    return rvm_true;
    }
/* pick random block from alloc list */
block_t *pick_random()
    {
    block_t         *block;             /* allocated block descriptor */
    int             i;
    
    /* exit if list empty */
    if (alloc_list.list.length == 0)
        return NULL;

    /* pick a random block */
    block = (block_t *)alloc_list.nextentry;
    for (i = random() % alloc_list.list.length; i > 0;i--)
        block = (block_t *)block->links.nextentry;

    return block;
    }
/* allocator */
void do_malloc(id)
    int             id;                 /* thread id */
    {
    rvm_tid_t       tid;                /* transaction identifier */
    rvm_return_t    ret;                /* rvm error return */
    int             err;                /* rds error return */
    block_t         *block;             /* allocated block descriptor */

    /* pick a size and allocate a block in recoverable heap */
    block = (block_t *)malloc(sizeof(block_t));
    init_list_ent(&block->links);
    mutex_init(&block->lock);
    block->size = random() % max_block_size;
    if (block->size == 0) block->size = 1;
    block->ptr = rds_malloc(block->size, 0, &err);
    if (err != SUCCESS) 
        {
        if (err > SUCCESS)
            printf("\n%d: rds_malloc = %s\n", id,
                   /* rvm_return((rvm_return_t *)err)); XXX BOGUS JET */
                   rvm_return((rvm_return_t)err));
        else
            {
            if (err == ENO_ROOM) return;
            printf("\n%d: rds_malloc = %d\n", id, err);
            }
        ASSERT(rvm_false);
	}

    /* transactionally clear the allocated space */
    rvm_init_tid(&tid);
    if ((ret=rvm_begin_transaction(&tid,no_restore))
        != RVM_SUCCESS)
        {
        printf("\n%d: ERROR: malloc begin_trans, code: %s\n",
               id,rvm_return(ret));
        ASSERT(rvm_false);
        }
    if ((ret=rvm_set_range(&tid,block->ptr,block->size))
        != RVM_SUCCESS)
        {
        printf("\n%d: ERROR: malloc set_range, code: %s\n",
               id,rvm_return(ret));
        ASSERT(rvm_false);
        }
    (void)bzero(block->ptr,block->size); /* blow away obsolete guards */
    block->chksum = 0;
    /* commit the block */
    if ((ret=rvm_end_transaction(&tid,no_flush)) != RVM_SUCCESS)
        {
        printf("\n%d: ERROR: alloc end_trans, code: %s\n",
               id,rvm_return(ret));
        ASSERT(rvm_false);
            }

    /* put the block on the allocated list */
    CRITICAL(alloc_list_lock,           /* begin alloc list crit sec */
        {
        (void)move_list_ent(NULL,&alloc_list,(list_entry_t *)block);
        });                             /* end alloc list crit sec */

    }
/* deallocator function */
void do_free(id)
    int             id;                 /* thread id */
    {
    int             err;                /* rds error return */
    block_t         *block;             /* allocated block descriptor */
    
    CRITICAL(alloc_list_lock,           /* begin alloc list crit sec */
        {
        /* select a random entry */
        if ((block=pick_random()) == NULL)
            {
            /* exit if list empty */
            mutex_unlock(&alloc_list_lock);
            return;
            }

        /* unlink the block */
        (void)move_list_ent(&alloc_list,NULL,(list_entry_t *)block);
        });                             /* end alloc list crit sec */

    /* wait for possible modifications to complete and check sum */
    CRITICAL(block->lock,{});
    test_chk_sum(block);

    /* free the selected block */
    rds_free(block->ptr, 0, &err);
    if (err != SUCCESS)
        {
        if (err > SUCCESS)
            printf("\n%d: rds_free = %s\n", id,
                   /* rvm_return((rvm_return_t *)err)); */
                   rvm_return((rvm_return_t)err));
        else
            printf("\n%d: rds_free = %d\n", id, err);
        ASSERT(rvm_false);
        }

    /* deallocate list entry */
    mutex_clear(&block->lock);
    free(block);
    }
/* rvm_chk_range test */
void test_chk_range(tid,addr,len,id)
    rvm_tid_t       *tid;
    char            *addr;
    rvm_length_t    len;
    int             id;                 /* thread id */
    {
    rvm_return_t    ret;             /* rvm return code */

#if ((RVM_MAJOR_VERSION >= 2) && (RVM_MINOR_VERSION >= 0))
    /* test if range is declared in tid */
    if ((ret = rvm_chk_range(tid,addr,len))
        != RVM_SUCCESS)
        {
        printf("\n%d: ERROR: modify chk_range, code: %s\n",
               id,rvm_return(ret));
        ASSERT(rvm_false);
        }

    /* modify range and re-test: should fail */
    if ((ret = rvm_chk_range(tid,addr,len+1))
        != RVM_ENO_RANGE)
        {
        printf("\n%d: ERROR: modify chk_range, code: %s\n",
               id,rvm_return(ret));
        ASSERT(rvm_false);
        }

    if ((ret = rvm_chk_range(tid,addr+1,len))
        != RVM_ENO_RANGE)
        {
        printf("\n%d: ERROR: modify chk_range, code: %s\n",
               id,rvm_return(ret));
        ASSERT(rvm_false);
        }
#endif VERSION_TEST
    }
/* block modifier transaction */
void do_trans(block,range_list,do_flush,id)
    block_t         *block;             /* block descriptor */
    block_t         *range_list;        /* list of modification ranges */
    rvm_bool_t      do_flush;
    int             id;                 /* thread id */
    {
    int             start,finish,temp,i,j;
    int             n_ranges;
    block_t         *range;
    char            *save_area;
    char            new_val;
    rvm_bool_t      do_abort = rvm_false;
    rvm_tid_t       tid;
    rvm_mode_t      start_mode = no_restore;
    rvm_mode_t      commit_mode = no_flush;
    rvm_return_t    ret;                /* rvm error return */

    /* choose number of ranges and whether to abort */
    n_ranges = random() % max_ranges;
    if ((random() % 100) < abort_frac) 
        {
        do_abort = rvm_true;
        save_area = malloc(block->size);
        ASSERT(save_area != NULL);
        bcopy(block->ptr,save_area,block->size);
        start_mode = restore;
        }
    else
        if ((random() % 100) < restore_frac)
            start_mode = restore;       /* force test of existing data buffers */

    /* prepare a transaction */
    rvm_init_tid(&tid);
    if ((ret=rvm_begin_transaction(&tid,start_mode))
        != RVM_SUCCESS)
        {
        printf("\n%d: ERROR: modify begin_trans, code: %s\n",
               id,rvm_return(ret));
        ASSERT(rvm_false);
        }
    /* pick random ranges and modifications */
    for (j=0; j<n_ranges; j++)
        {
        /* select a random range within the recoverable block */
        range = (block_t *)malloc(sizeof(block_t));
        ASSERT(range != NULL);
        init_list_ent(&range->links);
        start = random() % block->size;
        temp = finish = random() % block->size;
        if (start > finish)
            {
            finish = start;
            start = temp;
            }
        range->ptr = &block->ptr[start];
        range->size = finish-start+1;
        if (range->size > max_mod_size)
            range->size = random() % max_mod_size;
        move_list_ent(NULL,range_list,range);
        
        /* tell RVM */
        if ((ret=rvm_set_range(&tid,range->ptr,range->size))
            != RVM_SUCCESS)
            {
            printf("\n%d: ERROR: modify set_range, code: %s\n",
                   id,rvm_return(ret));
            ASSERT(rvm_false);
            }
        /* see if range should be checked */
        if ((random() % 100) < chk_range_frac)
            test_chk_range(&tid,range->ptr,range->size,id);

        /* set the selected range to random values */
        new_val = random() % 256;
        for (i=0; i<range->size; i++)
            block->ptr[start+i] = new_val;
        }

    /* commit or abort the changes */
    if (do_abort)
        {
        if ((ret=rvm_abort_transaction(&tid)) != RVM_SUCCESS)
            {
            printf("\n%d: ERROR: modify abort_trans, code: %s\n",
               id,rvm_return(ret));
            ASSERT(rvm_false);
            }

        /* check restoration */
        for (i=0; i<block->size; i++)
            if (save_area[i] != block->ptr[i])
                {
                printf("\n%d: modified region improperly restored by abort\n",
                       id);
                ASSERT(rvm_false);
                }
        free(save_area);
        }
    else
        {
        if (do_flush) commit_mode = flush;
        if ((ret=rvm_end_transaction(&tid,commit_mode)) != RVM_SUCCESS)
            {
            printf("\n%d: ERROR: modify end_trans, code: %s\n",
               id,rvm_return(ret));
            ASSERT(rvm_false);
            }
        }
    }
/* block modifier */
void do_modify(id)
    int             id;                 /* thread id */
    {
    int             n_trans;            /* number of transactions per block */
    rvm_return_t    ret;                /* rvm error return */
    block_t         *block;             /* block descriptor */
    block_t         range_list;         /* list of modification ranges */
    block_t         *range;
    rvm_bool_t      do_flush;

    /* select a random entry */
    CRITICAL(alloc_list_lock,           /* begin alloc list crit sec */
        {
        if ((block=pick_random()) != NULL)
            /* all changes must be in crit sec so block doesn't get free'd */
            mutex_lock(&block->lock);   /* begin block crit sec */
        });                             /* end alloc list crit sec */
    cthread_yield();                    /* give free a chance to block... */
    if (block == NULL) return;          /* exit if list empty */
    test_chk_sum(block);                /* test for damage  */

    /* pick random number of transactions; modify block */
    n_trans = random() % max_trans;
    init_list_head(&range_list);

    /* execute the transactions */
    while (n_trans-- >= 0)
        {
        do_flush = (rvm_bool_t)((random() % 100) < flush_frac);
        do_trans(block,&range_list,do_flush,id);

        /* insert marker between transactions */
        range = (block_t *)malloc(sizeof(block_t));
        ASSERT(range != NULL);
        init_list_ent(&range->links);
        range->size = n_trans+1;
        range->ptr = NULL;
        move_list_ent(NULL,&range_list,range);
        cthread_yield();
        }

    /* resum block and kill range list */
    block->chksum = check_sum(block->ptr,block->size);
    while (range_list.links.nextentry->is_header != rvm_true)
        {
        range = (block_t *)move_list_ent(&range_list,NULL,NULL);
        free(range);
        }
    mutex_unlock(&block->lock);         /* end block crit sec */
    }
/* region checker for chk_vm */
rvm_bool_t chk_region(seg_file,region)
    FILE            *seg_file;
    int             region;             /* index of region */
    {
    int             j;
    int             c;
    char            *vm;
    rvm_length_t    reg_pos;            /* region position */
    int             io_ret;

    /* set up read of region data */
    reg_pos = RVM_OFFSET_TO_LENGTH(RegionDefs[region].offset);
    if ((io_ret=fseek(seg_file,reg_pos,0)) != 0)
        {
        printf("\n? Can't seek to %d in segment file, ret = %d\n",
               reg_pos,io_ret);
        ASSERT(rvm_false);
        }

    /* scan and check loop */
    vm = RegionDefs[region].vmaddr;
    for (j=0; j < RegionDefs[region].length; j++)
        {
        /* scan file */
        c = getc(seg_file);
        if (c == EOF)
            {
            printf("\n? EOF encountered while reading segment; ",
                   "offset = %d\n",reg_pos);
            ASSERT(rvm_false);
            }

        /* check the data against vm */
        if ((char)c != vm[j])
            {
            printf("\n? Error: mapped data doesn't match:\n");
            printf("         vm[%d] = 0x%x\n",j,(vm[j] & 255));
            printf("         region%d[%d] = 0x%x\n",region,j,
                   (c & 255));
            printf("         region offset = %d\n",reg_pos);
            ASSERT(rvm_false);
            return rvm_false;
            }
        reg_pos++;
        }

    return rvm_true;
    }
/* vm <==> disk comparator */
rvm_bool_t chk_vm()
    {
    rvm_return_t    ret;
    FILE            *seg_file;
    int             i;
    int             io_ret;
    rvm_statistics_t *stats;
#if ((RVM_MAJOR_VERSION >= 2) && (RVM_MINOR_VERSION >= 0))
    rvm_query_t     *query;
#endif VERSION_TEST
    rvm_return_t    retval;
    rvm_length_t    n_recs;
    rvm_length_t    tot_recs;

    if ((stats=rvm_malloc_statistics()) == NULL)
        {
        printf("\n?  rvm_malloc_statistics failed\n");
        ASSERT(rvm_false);
        }
#if ((RVM_MAJOR_VERSION >= 2) && (RVM_MINOR_VERSION >= 0))
    if ((query=rvm_malloc_query()) == NULL)
        {
        printf("\n?  rvm_malloc_query failed\n");
        ASSERT(rvm_false);
        }
    if ((retval=RVM_QUERY(query,NULL)) != RVM_SUCCESS)
        {
        printf("\n?  rvm_query failed, code: %s\n",
               rvm_return(ret));
        ASSERT(rvm_false);
        }
#endif VERSION_TEST
    printf("\nChecking segment consistency...\n");
#if ((RVM_MAJOR_VERSION >= 2) && (RVM_MINOR_VERSION >= 0))
    printf("  log head = %d\n",RVM_OFFSET_TO_LENGTH(query->log_head));
#endif VERSION_TEST

    /* check vm heap if required */
#ifdef RVM_USELWP
    if (chk_alloc != 0)
        CheckAllocs("Checking heap before truncation");
#endif RVM_USELWP

    /* truncate to sync segement with vm */
    if ((ret=rvm_truncate()) != RVM_SUCCESS)
        {
        printf("\n?  rvm_truncate failed, code: %s\n",
               rvm_return(ret));
        ASSERT(rvm_false);
        }

    /* re-check vm heap if required */
#ifdef RVM_USELWP
    if (chk_alloc != 0)
        CheckAllocs("Checking heap after truncation");
#endif RVM_USELWP

    /* get current state */
    if ((retval=RVM_STATISTICS(stats)) != RVM_SUCCESS)
        {
        printf("\n?  rvm_statistics failed, code: %s\n",
               rvm_return(ret));
        ASSERT(rvm_false);
        }
#if ((RVM_MAJOR_VERSION >= 2) && (RVM_MINOR_VERSION >= 0))
    if ((retval=RVM_QUERY(query,NULL)) != RVM_SUCCESS)
        {
        printf("\n?  rvm_query failed, code: %s\n",
               rvm_return(ret));
        ASSERT(rvm_false);
        }
#endif VERSION_TEST

    /* record log data */
    n_recs = stats->n_flush_commit + stats->n_no_flush_commit
        + stats->n_split + stats->n_special + stats->n_wrap;
    tot_recs =  n_recs+stats->tot_flush_commit+stats->tot_no_flush_commit
        + stats->tot_split + stats->tot_special + stats->tot_wrap;
#if ((RVM_MAJOR_VERSION >= 2) && (RVM_MINOR_VERSION >= 0))
    printf("  log tail = %d, records written = %d\n",
           RVM_OFFSET_TO_LENGTH(query->log_tail),tot_recs);
    printf("  number tree nodes = %d, tree depth = %d\n",
           query->n_nodes,query->max_depth);
#else
    printf("  records written = %d\n",tot_recs);
#endif VERSION_TEST

    /* open segment file read-only */
    if ((seg_file = fopen(DataFileName,"r")) == NULL)
        {
        printf("\n? Error opening segment file\n");
        printf("    errno = %d\n",errno);
        ASSERT(rvm_false);
        }
    /* check all mapped regions against segment file */
    for (i=0; i<NRegionDefs; i++)
        {
        if (!chk_region(seg_file,i))
            ASSERT(rvm_false);
        }

    /* all's well -- close file & release the other threads */
    if (io_ret=fclose(seg_file))
        {
        printf("\n? Error closing segment file\n");
        printf("    ret = %d, errno = %d\n",ret,errno);
        ASSERT(rvm_false);
        }
    printf("  memory and segment agree!\n");

    /* reset time */
    if (gettimeofday(&init_time,NULL) < 0)
        {
        perror("?  Error getting time of day");
        ASSERT(rvm_false);
        }

    rvm_free_statistics(stats);
#if ((RVM_MAJOR_VERSION >= 2) && (RVM_MINOR_VERSION >= 0))
    rvm_free_query(query);
#endif VERSION_TEST
    return rvm_true;
    }
rvm_bool_t chk_time()
    {
    struct timeval  time;

    /* get current time */
    if (gettimeofday(&time,NULL) < 0)
        {
        perror("?  Error getting time of day");
        return rvm_false;
        }

    /* print time */
    printf("  time: %s",ctime(&time.tv_sec));
    printf("  number of test operations: %d\n",max_op_cnt);
    printf("  elasped time: %d sec.\n",time.tv_sec-init_time.tv_sec);

    init_time = time;                   /* update loop start time */
    return rvm_true;
    }
/* moby range test */
rvm_bool_t chk_moby()
    {
    rvm_length_t    length;             /* range length */
    char            *start;             /* starting offset for range */
    rvm_length_t    temp;
    int             i;
    char            *vm_save;           /* data save area for test range */
    rvm_tid_t       tid;                /* transaction identifier */
    rvm_return_t    ret;                /* rvm error return */
    
    /* choose a large size for moby range */
        length = random() % max_moby_size;

    /* pick a starting point for mods */
    temp = RegionDefs[0].length - length;
    start = RVM_ADD_LENGTH_TO_ADDR(RegionDefs[0].vmaddr,
                                   (random() % temp));

    /* announce test */
    printf("\nLarge range modification\n  addr:   0x%x\n  length: %d\n",
           start,length);

    /* get a buffer and save vm */
    if ((vm_save = malloc(length)) == NULL)
        {
        printf("? Error: can't allocate buffer for moby test\n");
        ASSERT(rvm_false);
        }
    (void)bcopy(start,vm_save,length);

    /* set up transaction for moby modification */
    rvm_init_tid(&tid);
    if ((ret=rvm_begin_transaction(&tid,no_restore))
        != RVM_SUCCESS)
        {
        printf("? ERROR: chk_moby begin_trans 1, code: %s\n",
               rvm_return(ret));
        ASSERT(rvm_false);
        }
    if ((ret=rvm_set_range(&tid,start,length)) != RVM_SUCCESS)
        {
        printf("? ERROR: chk_moby set_range 1, code: %s\n",
               rvm_return(ret));
        ASSERT(rvm_false);
        }
    /* store random trash in test range & commit */
    for (i=0; i<length; i++)
        start[i] = (char)(random() % 256);
    if ((ret=rvm_end_transaction(&tid,flush)) != RVM_SUCCESS)
        {
        printf("? ERROR: chk_moby end_trans 1, code: %s\n",
               rvm_return(ret));
        ASSERT(rvm_false);
        }

    /* check the results and restore the test range contents */
    if (chk_vm() == rvm_false) return rvm_false;
    printf("\n  Restoring original data\n");
    if ((ret=rvm_begin_transaction(&tid,no_restore))
        != RVM_SUCCESS)
        {
        printf("? ERROR: chk_moby begin_trans 2, code: %s\n",
               rvm_return(ret));
        ASSERT(rvm_false);
        }
    if ((ret=rvm_set_range(&tid,start,length)) != RVM_SUCCESS)
        {
        printf("? ERROR: chk_moby set_range 2, code: %s\n",
               rvm_return(ret));
        ASSERT(rvm_false);
        }
    (void)bcopy(vm_save,start,length);
    if ((ret=rvm_end_transaction(&tid,flush)) != RVM_SUCCESS)
        {
        printf("? ERROR: chk_moby end_trans 2, code: %s\n",
               rvm_return(ret));
        ASSERT(rvm_false);
        }

    free(vm_save);
    return rvm_true;
    }
/* test monitor */
void monitor()
    {
    
    mutex_lock(&chk_lock);              /* begin chk_lock crit sec */
    if (chk_block)
        {
        /* block while check is done */
        blk_cnt++;
        condition_signal(&chk_code);
        while (chk_block)
            condition_wait(&blk_code,&chk_lock);
        }

    /* count operations and become chk thread if at threshold */
    if ((op_cnt-- ) == 0)
        {
        /* block other worker threads and truncate */
        chk_block = rvm_true;
        if (nthreads > 1)
            while (blk_cnt != (nthreads-1))
                condition_wait(&chk_code,&chk_lock);

        /* do monitoring tests */
        printf("\nEnd test cycle %5d\n",cycle);
        if (time_tests)                 /* timestamp the loop */
            if (!chk_time()) goto err_exit;
        if (moby_test)                  /* test large range */
            if (!chk_moby()) goto err_exit;
        if (vm_test)                    /* compare vm with segment */
            if (!chk_vm()) goto err_exit;

        chk_block = rvm_false;
        blk_cnt = 0;
        op_cnt = max_op_cnt;
        if (((cycle++) >= max_cycles) && (max_cycles != -1))
            Terminate = rvm_true;
        condition_broadcast(&blk_code);
        }
    goto exit;

  err_exit:
    Terminate = rvm_true;
  exit:
    mutex_unlock(&chk_lock);            /* end chk_lock crit sec */
    return;
    }
/* testing thread function */
worker(id)
    int             id;                 /* thread indentifier */
    {
    int             err, i, count = 0;
    rvm_return_t    ret;
    rvm_tid_t       *tid = rvm_malloc_tid();

    CRITICAL(print_lock,
             printf("Worker thread %d running...\n\n",id));

    while (!Terminate)
        {
        monitor();                      /* do monitoring functions */

        last[id] = 1;
        do_malloc(id);                  /* allocate a recoverable heap block */
        cthread_yield();

        last[id] = 2;
        do_free(id);                    /* free a recoverable heap block */
        cthread_yield();

        last[id] = 3;
        do_modify(id);                  /* modify a recoverable heap block */

        cthread_yield();
        }

    /* signal termination */
    if (--nthreads == 0)
        condition_signal(&thread_exit_code);
    cthread_exit(0);
    }
/* string name lookup: accepts minimum substring for match */
static int  lookup_str_name(str,str_vec,ambig_str)
    char            *str;               /* name to lookup */
    str_name_entry_t *str_vec;          /* defining vector */
    char            *ambig_str;         /* ambiguous name type */
    {
    int             i = 0;              /* loop counter */
    int             str_index = (int)UNKNOWN; /* string index in vector */

    while (str_vec[i].str_name[0] != '\0')
        {
        /* test if name string starts with str */
        if (strstr(str_vec[i].str_name,str) ==
            str_vec[i].str_name)
            {
            /* yes, candidate found -- test further */
            if (strcmp(str_vec[i].str_name,str) == 0)
                return i;               /* exact match, select this name */
            if (str_index == (int)UNKNOWN)
                str_index = i;          /* substring match, remember name */
            else
                {                       /* error: ambigous name */
                fprintf(stderr,"? %s: ambiguous %s: %s or %s?\n",
                        str,
                        ambig_str,
                        str_vec[i].str_name,
                        str_vec[str_index].str_name);
                return (int)UNKNOWN;
                }
            }
        i++;
        }

    /* see if name not found */
    if (str_index == (int)UNKNOWN)
        fprintf(stderr,"? %s: %s not found\n",str,ambig_str);

    return str_index;
    }
/* scanner utilities */

/* cmd line cursor incrementor */
#define incr_cur(_i) cmd_cur = &cmd_cur[_i]

/* tests for end of line characters */
#define end_line(c) (((c) == '\0') || ((c) == '\n'))
#define scan_stop(c) (end_line(c) || ((c) == '#'))

static void skip_white(ptr)
    char            **ptr;
    {
    while (isspace(**ptr))
        (*ptr)++;
    }

static void skip_lf()
    {
    while (rvm_true)
        if (getc(stdin) == '\n') return;
    }

/* string scanner */
static int  scan_str(str,len)
    char            *str;               /* ptr to string buffer */
    int             len;                /* maximum length */
    {
    int             indx = 0;

    skip_white(&cmd_cur);
    while (!scan_stop(*cmd_cur) && (!isspace(*cmd_cur))
           && (indx < len))
        str[indx++] = *(cmd_cur++);

    str[indx] = '\0';

    return indx;                        /* return length */
    }
/* integer scanner & range checker */
static int scan_int(low_range,high_range,default_val,name_str,err_str)
    int                 low_range;
    int                 high_range;
    int                 default_val;
    char                *name_str;
    char                *err_str;
    {
    int                 val;

    /* be sure there's something there */
    skip_white(&cmd_cur);
    if (scan_stop(*cmd_cur))
        {
        printf("?  No parameter given for %s\n",name_str);
        return 0;
        }

    /* scan as interger */
    val = strtol(cmd_cur,&cmd_cur,0);

    /* check range */
    if ((low_range != 0) || (high_range != 0)
        && ((val >= low_range) && (val <= high_range)))
        return val;

    /* out of range - print message, return default */
    printf("?  Warning: %d is out of range %d: %d for %s\n",val,
           low_range,high_range,name_str);
    printf("     %d assigned as default\n",default_val);

    return default_val;
    }
/* read prompted line from terminal */
static char *read_prompt_line(prompt,null_ok)
    char            *prompt;            /* prompt string */
    rvm_bool_t      null_ok;            /* true if null input ok */
    {

    /* read from input stream if nothing left in command line */
    skip_white(&cmd_cur);
    if (scan_stop(*cmd_cur) && (! null_ok))
        while (rvm_true)
            {
            /* prompt if required */
            cmd_line[0] = '\0';
            if (prompt != NULL)
                printf("%s ",prompt);

            /* get line and check termination conditions */
            cmd_cur = fgets(cmd_line,CMD_MAX,stdin);
            if (cmd_cur == NULL)
                {
                if (feof(stdin))
                    {
                    printf("\n?  Error: EOF reported from stdin !!\n");
                    exit(1);
                    }
                return NULL;            /* error */
                }
            if ((null_ok) || (cmd_line[0] != '\0'))
                return cmd_cur=cmd_line;
            }
    }
/* display test parameters */
void show_test_parms()
    {
    int             priority;

    printf("\nCurrent Test Parameters:\n\n");

    printf("  Log file:  %s\n",LogFileName);
    printf("  Data file: %s\n",DataFileName);
    printf("  Data file length:                         %u\n",
           RVM_OFFSET_TO_LENGTH(DataLen));
    printf("  Number of operations/cycle:               %d\n",max_op_cnt);
    printf("  Maximum number of transactions/operation: %d\n",max_trans);
    printf("  Maximum number of ranges/transaction:     %d\n",max_ranges);
    printf("  Maximum modification range size:          %d\n",max_mod_size);
    printf("  Maximum big modification range size:      %d\n",max_moby_size);
    printf("  Maximum allocation block size:            %d\n",
           max_block_size);
    printf("  Number of preallocated blocks:            %d\n",
           num_pre_alloc);
    if (pre_alloc_trunc)
        printf("  Truncate after preallocation:              true\n");
    else
        printf("  Truncate after preallocation:             false\n");

#if ((RVM_MAJOR_VERSION >= 2) && (RVM_MINOR_VERSION >= 0))
    printf("  %% ranges to check:                        %d\n",chk_range_frac);
#endif VERSION_TEST
    printf("  %% transactions to abort:                  %d\n",abort_frac);
    printf("  %% transactions to commit with flush:      %d\n",flush_frac);
    printf("  %% transactions started with restore:      %d\n",restore_frac);

    printf("\n");
    if (max_cycles == -1)
        printf("  Number of test cycles:                    unlimited\n");
    else
        printf("  Number of test cycles:                    %d\n",
               max_cycles);
    printf("  Number of worker threads:                 %d\n",nthreads);
    priority = getpriority(PRIO_PROCESS,0);
    printf("  Execution priority:                       %d\n",priority);
    printf("  Random seed:                              %d\n",seed);
    printf("  Allocator check level:                    %d\n",chk_alloc);
    if (PlumberFileName[0] != '\0')
        printf("  Plumber file: %s\n",PlumberFileName);

    /* print test options */
    printf("  Test options: ");
    if (moby_test)
        printf("big_range ");
    if (time_tests)
        printf("time_tests ");
    if (vm_test)
        printf("chk_vm ");
    printf("\n");

    /* print optimizations */
    printf("\n  RVM optimizations: ");
    if (optimizations == 0)
        printf("none");
    if ((optimizations & RVM_COALESCE_RANGES) != 0)
        printf("coalesce_ranges ");
    if ((optimizations & RVM_COALESCE_TRANS) != 0)
        printf("coalesce_trans ");
#if ((RVM_MAJOR_VERSION >= 2) && (RVM_MINOR_VERSION >= 0))
    if ((optimizations & RVM_INCR_TRUNCATION) != 0)
        printf("incr_truncation ");
#endif VERSION_TEST
    printf("\n");
    printf("  RVM truncation threshold:                 %d%%\n",
           trunc_frac);
#if ((RVM_MAJOR_VERSION >= 2) && (RVM_MINOR_VERSION >= 0))
    printf("  RVM epoch truncation threshold:           %d%%\n",
           epoch_trunc_frac);
    printf("  RVM incremental truncation time slice:    %d msec.\n",
           incr_trunc_time);
#endif VERSION_TEST
    printf("  RVM recovery buffer length:               %d\n",rec_buf_len);
    printf("  RVM flush buffer length:                  %d\n",flush_buf_len);
    printf("  RVM max. read length:                     %d\n",max_read_len);
    if (chk_sum_sw)
        printf("  RVM check sum flag:                        true\n");
    else
        printf("  RVM check sum flag:                       false\n");
    if (no_yield_sw)
        printf("  RVM no yield flag:                         true\n");
    else
        printf("  RVM no yield flag:                        false\n");
    if (vm_protect_sw)
        printf("  RVM vm protect flag:                       true\n");
    else
        printf("  RVM vm protect flag:                      false\n");
    if (show_brk)
        printf("  Show break point:                          true\n");
    else
        printf("  Show break point:                         false\n");
    printf("\n");
    }
/* get data file name and size */
void set_data_file()
    {
    int                 len;
    struct stat 	sbuf;

    while (rvm_true)
        {
        read_prompt_line("  Enter data file name: ",rvm_false);
        skip_white(&cmd_cur);
        len = scan_str(DataFileName,MAXPATHLEN);
        if (len != 0) break;
        printf("\n?  Data file name must be specified\n");
        }

    /* get length of segment if partition specified */
    if (stat(DataFileName, &sbuf) < 0) 
        {
	printf("%s\n", errno < sys_nerr
               ? sys_errlist[errno]
               : "Cannot stat data file");
        DataFileName[0] = '\000';
	return;
        }
    switch (sbuf.st_mode & S_IFMT)
        {
      case S_IFSOCK:
      case S_IFDIR:
      case S_IFLNK:
      case S_IFBLK: 
	printf("Illegal file type!\n");
        DataFileName[0] = '\0';
	return;

      case S_IFCHR:
        while (rvm_true)
            {
            read_prompt_line("Enter the length of device: ",rvm_false);
            len = scan_int(512,2000000000,0,"DataLen",
                           "?  Bad data device length\n");
            if (len != 0) break;
            }
	DataLen = RVM_MK_OFFSET(0,len);
        return;

      default:
        DataLen = RVM_MK_OFFSET(0,sbuf.st_size); /* Normal files. */
        return;
        }
    }
/* get log file name */
void set_log_file()
    {
    int             len;

    while (rvm_true)
        {
        read_prompt_line("  Enter log file name: ",rvm_false);
        skip_white(&cmd_cur);
        len = scan_str(LogFileName,MAXPATHLEN);
        if (len != 0) break;
        printf("\n?  Log file name must be specified\n");
        }
    }

/* set execution priority */
static void set_priority()
    {
    int             priority;
    int             err;

    priority = scan_int(0,20,0,"scheduling priority",
                        "?  Bad scheduling priority");

    err = setpriority(PRIO_PROCESS,0,priority);
    if (err != 0)
        printf("?  Error setting process priority, err = %d\n",err);

    }
/* option flags */
#define MAX_FLAGS   15                  /* maximum number of flag codes */

static str_name_entry_t flag_vec[MAX_FLAGS] = /* flag codes vector */
                    {
                    {"all_optimizations",ALL_KEY}, /* enable all optimizations */
                    {"none",NONE_KEY}, /* no optimizations */
                    {"coalesce_ranges",RANGES_KEY}, /* enable range coalesce */
                    {"rvm_coalesce_ranges",RANGES_KEY}, /* enable range coalesce */
                    {"ranges",RANGES_KEY}, /* enable range coalesce */
                    {"coalesce_trans",TRANS_KEY}, /* enable transaction coalesce */
                    {"rvm_coalesce_trans",TRANS_KEY}, /* enable transaction coalesce */
                    {"trans",TRANS_KEY}, /* enable transaction coalesce */

#if ((RVM_MAJOR_VERSION >= 2) && (RVM_MINOR_VERSION >= 0))
                    {"rvm_incr_truncation",INCR_KEY}, /* enable incr truncation */
                    {"incr",INCR_KEY}, /* enable incr truncation */
                    {"incr_truncation",INCR_KEY}, /* enable incr truncation */

#endif VERSION_TEST
                    {"",(key_id_t)NULL} /* end mark, do not delete */
                    };
/* set rvm_options flags */
static void set_flags()
    {
    char            string[80];         /* flag code name string */
    int             i;

    optimizations = 0;

    /* scan loop */
    while (rvm_true)
        {
        /* see if done */
        skip_white(&cmd_cur);
        if (scan_stop(*cmd_cur))
            return;

        /* scan and lookup flag code */
        (void)scan_str(string,80);
        i = lookup_str_name(string,flag_vec,"option flag");
        if (i == (int)UNKNOWN)          /* error */
            {
            *cmd_cur = '\0';
            return;
            }

        /* process code */
        switch (flag_vec[i].key)
            {
          case ALL_KEY:
            optimizations |= RVM_ALL_OPTIMIZATIONS;
            continue;
          case NONE_KEY:
            optimizations = 0;
            continue;
          case RANGES_KEY:
            optimizations |= RVM_COALESCE_RANGES;
            continue;
          case TRANS_KEY:
            optimizations |= RVM_COALESCE_TRANS;
            continue;
#if ((RVM_MAJOR_VERSION >= 2) && (RVM_MINOR_VERSION >= 0))
          case INCR_KEY:
            optimizations |= RVM_INCR_TRUNCATION;
#endif VERSION_TEST
            }
        }
    }
/* plumber support */
static void setup_plumber_file()
    {
    int             len;

    while (rvm_true)
        {
        read_prompt_line("  Enter plumber file name: ",rvm_false);
        skip_white(&cmd_cur);
        len = scan_str(PlumberFileName,MAXPATHLEN);
        if (len != 0) break;
        printf("\n?  Plumber file name must be specified\n");
        }

    /* try to open file if really can use plumber */
#ifdef RVM_USELWP
    PlumberFile = fopen(PlumberFileName,"w");
    if (PlumberFile == NULL)
        printf("?  File %s could not be opened, errno = %d\n",
               PlumberFileName,errno);
    else
        fclose(PlumberFile);
#else  RVM_USELWP
    printf("W  Plumber not available with Cthreads\n");
#endif RVM_USELWP
    }

/* call function (to be used from GDB) */
void call_plumber()
    {
#ifdef RVM_USELWP
    PlumberFile = fopen(PlumberFileName,"w");
    if (PlumberFile == NULL)
        printf("?  File %s could not be opened, errno = %d\n",
               PlumberFileName,errno);
    plumber(PlumberFile);
    fclose(PlumberFile);
#else  RVM_USELWP
    printf("?  Plumber not available with Cthreads\n");
#endif RVM_USELWP
    }
/* print break point and limit */
void show_break()
    {
    rvm_length_t    cur_brk;
    struct rlimit   rlp;

    /* get current break point */
    errno = 0;
    if ((cur_brk=(rvm_length_t)sbrk(0)) == -1)
        {
        printf("\n? Error getting current break point\n");
        printf("    errno = %d\n",errno);
        ASSERT(rvm_false);
        }

    /* get system maximum */
    errno = 0;
    if (getrlimit(RLIMIT_DATA,&rlp) < 0)
        {
        printf("\n? Error getting data segment limit\n");
        printf("    errno = %d\n",errno);
        ASSERT(rvm_false);
        }

    /* print the limits */
    printf("\nCurrent break point:         0x%x\n",
           RVM_ROUND_LENGTH_UP_TO_PAGE_SIZE(cur_brk+5*RVM_PAGE_SIZE));
    printf("Maximum data segment length: 0x%x\n\n",rlp.rlim_max);
    exit(0);
    }
/* command dispatch */
#define MAX_CMDS   50                   /* maximum number of commands */

static str_name_entry_t cmd_vec[MAX_CMDS] = /* command codes vector */
                    {
                    {"abort_frac",ABORT_KEY}, /* set abort fraction */
                    {"all_tests",ALL_KEY},    /* do all test options */
                    {"block_size",BLOCK_SIZE_KEY}, /* set maximum allocation block size */
                    {"mod_size",MOD_SIZE_KEY},  /* set maximum modification size */
                    {"log_file",LOG_KEY}, /* set log file */
                    {"data_file",DATA_KEY}, /* set data file */
                    {"restore_frac",RESTORE_KEY}, /* set restore fraction */
                    {"flush_buf",FLUSH_BUF_KEY}, /* set flush buffer length */
                    {"flush_frac",FLUSH_KEY}, /* set flush fraction */
                    {"check_alloc",CHK_ALLOC_KEY}, /* enable allocation checking */
                    {"chk_sum",CHK_SUM_KEY}, /* set rvm_chk_sum */
                    {"chk_vm",VM_KEY},      /* test vm againt segment */
                    {"time_tests",TIME_KEY},  /* time the test cycles */
                    {"ranges",RANGES_KEY}, /* max number of ranges/trans */
                    {"trans",TRANS_KEY}, /* max number of trans/modify */
                    {"cycles",CYCLES_KEY}, /* number of cycles */
                    {"operations",OPS_KEY}, /* number of operations/cycle */
                    {"ops",OPS_KEY},    /* number of operations/cycle */
                    {"optimizations",OPT_KEY}, /* set optimizations */
                    {"opts",OPT_KEY},   /* set optimizations */
                    {"show_parmeters",PARMS_KEY},   /* show test parameters */
                    {"parmeters",PARMS_KEY},   /* show test parameters */
                    {"parms",PARMS_KEY},   /* show test parameters */
                    {"priority",PRIORITY_KEY}, /* set tests priority */
                    {"read_length",MAX_READ_KEY}, /* set maximum read length */
                    {"rec_buf",REC_BUF_KEY}, /* set recovery buffer length */
                    {"run",RUN_KEY},    /* start tests */
                    {"seed",SEED_KEY},  /* random num gen seed */
                    {"threads",THREADS_KEY}, /* number of threads */
                    {"truncate_frac",TRUNC_KEY}, /* set truncation threshold */
                    {"big_range",MOBY_KEY}, /* test moby ranges */
                    {"max_big_range",MOBY_MAX_KEY}, /* set maximum moby range size */
                    {"quit",QUIT_KEY}, /* quit test program */
                    {"plumber",PLUMBER_KEY}, /* plumber file setup */
                    {"pre_allocate",PRE_ALLOC_KEY}, /* number blocks preallocated */
                    {"pre_allocate_trunc",PRE_ALLOC_TRUNC_KEY}, /* truncate
                                                                   after preallocation */
                    {"show_break_point",BRK_KEY}, /* print break point */
                    {"break_point",BRK_KEY}, /* print break point */
    /* version dependent commands */
#if ((RVM_MAJOR_VERSION >= 2) && (RVM_MINOR_VERSION >= 0))
                    {"epoch_truncate_frac",EPOCH_TRUNC_KEY}, /* set epoch trunc threshold */
                    {"incr_trunc_time",INCR_TRUNC_TIME_KEY}, /* set incr trunc
                                                                time slice */
                    {"chk_range_frac",CHK_RANGE_KEY}, /* set % of ranges to check */
                    {"no_yield",NO_YIELD_KEY}, /* set no yield mode */
                    {"vm_protect",VM_PROT_KEY}, /* setvm buffer protect mode */
#endif VERSION_TEST
                    {"",(key_id_t)NULL} /* end mark, do not delete */
                    };
main(argc, argv)
    int argc;
    char **argv;
    {
    rvm_options_t       *options;       /* options descriptor ptr */
    rvm_return_t	ret;
    char		*addr, *sptr, string[80];
    int 		err, i, length;
    
    /* initializations */
    cmd_line[0] != '\0';
    cmd_cur=cmd_line;
    chk_block = rvm_false;
    chk_sum_sw = rvm_false;
    no_yield_sw = rvm_false;
    vm_protect_sw = rvm_false;
    show_brk = rvm_false;
    blk_cnt = 0;
    mutex_init(&print_lock);
    mutex_init(&alloc_list_lock);
    init_list_head(&alloc_list);
    mutex_init(&chk_lock);
    condition_init(&blk_code);
    condition_init(&chk_code);
    mutex_init(&thread_lock);
    condition_init(&thread_exit_code);
    LogFileName[0] = '\0';
    DataFileName[0] = '\0';
    PlumberFileName[0] = '\0';
    cycle = 1;

    /* set defaults */
    max_cycles = CYCLES_DEFAULT;
    max_op_cnt = OP_CNT_DEFAULT;
    max_trans = TRANS_DEFAULT;
    max_ranges = RANGES_DEFAULT;
    max_block_size = BLOCK_SIZE_DEFAULT;
    max_mod_size = MOD_SIZE_DEFAULT;
    max_moby_size = MOBY_MOD_SIZE_DEFAULT;
    num_pre_alloc = PRE_ALLOC_DEFAULT;
    nthreads = THREADS_DEFAULT;
    flush_frac = FLUSH_DEFAULT;
    restore_frac = RESTORE_DEFAULT;
    abort_frac = ABORT_DEFAULT;
    optimizations = RVM_ALL_OPTIMIZATIONS;
    trunc_frac = TRUNCATE;
    seed = SEED_DEFAULT;
    chk_alloc = CHK_ALLOC_DEFAULT;
    max_read_len = MAX_READ_LEN;
    flush_buf_len = FLUSH_BUF_LEN;
    rec_buf_len = RECOVERY_BUF_LEN;
    pre_alloc_trunc = rvm_false;
/* version specific defaults */
#if ((RVM_MAJOR_VERSION >= 2) && (RVM_MINOR_VERSION >= 0))
    epoch_trunc_frac = EPOCH_TRUNCATE;
    incr_trunc_time = INCR_TRUNC_TIME;
    chk_range_frac = CHK_RANGE_DEFAULT;
#endif VERSION_TEST

    /* scan test commands */
    while (rvm_true)
        {
        read_prompt_line("*",rvm_false);

        /* ignore comment lines */
        skip_white(&cmd_cur);
        if (*cmd_cur == '#')
            {
            *cmd_cur = '\0';
            continue;
            }

        /* scan command name */
        (void)scan_str(string,80);
        i = lookup_str_name(string,cmd_vec,"command");
        if (i == (int)UNKNOWN)          /* error */
            {
            *cmd_cur = '\0';
            continue;
            }
        /* dispatch commands */
        switch (cmd_vec[i].key)
            {
          case LOG_KEY:                 /* get name of log file */
            set_log_file();
            continue;
          case DATA_KEY:                /* get name of data file */
            set_data_file();
            continue;
          case SEED_KEY:                /* set random number seed */
            seed = scan_int(0,100000000,SEED_DEFAULT,"seed",
                            "?  Bad seed");
            srandom(seed);
            continue;
          case THREADS_KEY:             /* get number of threads to run */
            nthreads = scan_int(1,1000,THREADS_DEFAULT,"thread",
                                "?  Bad thread count");
            continue;
          case OPS_KEY:                 /* set operation count */
            max_op_cnt = scan_int(1,10000,OP_CNT_DEFAULT,"operations",
                                  "?  Bad operation count");
            continue;
          case CYCLES_KEY:              /* set number of test cycles */
            max_cycles = scan_int(1,1000000000,CYCLES_DEFAULT,"cycles",
                                  "?  Bad cycle count");
            continue;
          case VM_KEY:                  /* test vm against segment */
            vm_test = rvm_true;
            continue;
          case TIME_KEY:                /* time stamp the test cycles */
            time_tests = rvm_true;
            continue;
          case MOBY_KEY:                /* test very large ranges */
            moby_test = rvm_true;
            continue;
          case MOBY_MAX_KEY:
            max_moby_size = scan_int(0,10000000,SEED_DEFAULT,"big range size",
                                     "?  Bad large modification size");
            continue;
          case ALL_KEY:                 /* do all test options */
            vm_test = rvm_true;
            time_tests = rvm_true;
            moby_test = rvm_true;
            continue;
          case ABORT_KEY:               /* set abort fraction */
            abort_frac = scan_int(0,100,ABORT_DEFAULT,"abort fraction",
                                  "?  Bad abort percentage");
            continue;
          case RESTORE_KEY:             /* set restore fraction */
            restore_frac = scan_int(0,100,RESTORE_DEFAULT,
                                    "restore fraction",
                                    "?  Bad restore percentage");
            continue;
          case FLUSH_KEY:               /* set flush fraction */
            flush_frac = scan_int(0,100,FLUSH_DEFAULT,"flush fraction",
                                  "?  Bad flush percentage");
            continue;
          case TRUNC_KEY:               /* set truncation fraction */
            trunc_frac = scan_int(0,100,TRUNCATE,
                                  "truncation fraction",
                                  "?  Bad truncation percentage");
            continue;
          case RANGES_KEY:              /* set max number ranges/trans */
            max_ranges = scan_int(0,1000000,RANGES_DEFAULT,
                                  "max ranges",
                                  "?  Bad maximum number of ranges");
            continue;
          case TRANS_KEY:              /* set max number trans/modify */
            max_trans = scan_int(0,100000,TRANS_DEFAULT,"max trans",
                                  "?  Bad maximum number of transactions");
            continue;
          case PARMS_KEY:               /* print test parameters */
            show_test_parms();
            continue;
          case PRE_ALLOC_KEY:           /* number of blocks to preallocate */
            num_pre_alloc = scan_int(1,100000,PRE_ALLOC_DEFAULT,
                                     "pre_alloc",
                                     "?  Bad block preallocation count");
            continue;
          case PRE_ALLOC_TRUNC_KEY:     /* truncate after preallocation */
            pre_alloc_trunc = rvm_true;
            continue;
          case RUN_KEY:
            printf("\n");
            break;
          case PRIORITY_KEY:            /* set execution priority */
            set_priority();
            continue;
          case QUIT_KEY:                /* exit program */
            exit(0);
          case OPT_KEY:                 /* set optimization flags */
            set_flags();
            continue;
          case NO_YIELD_KEY:            /* set no_yield option */
            no_yield_sw = rvm_true;
            continue;
          case REC_BUF_KEY:
            rec_buf_len = scan_int(MIN_RECOVERY_BUF_LEN,1000*RVM_PAGE_SIZE,
                                   RECOVERY_BUF_LEN,"rec_buf",
                                   "?  Bad recovery buffer length");
            continue;
          case FLUSH_BUF_KEY:
            flush_buf_len = scan_int(MIN_FLUSH_BUF_LEN,1000*RVM_PAGE_SIZE,
                                     FLUSH_BUF_LEN,"flush_buf",
                                     "?  Bad flush buffer length");
            continue;
          case BLOCK_SIZE_KEY:          /* set max allocation block size */
            max_block_size = scan_int(sizeof(rvm_length_t),100000,
                                      BLOCK_SIZE_DEFAULT,"block size",
                                      "?  Bad maximum allocation size");
            continue;
          case MOD_SIZE_KEY:            /* set max modification range size */
            max_mod_size = scan_int(1,10000000,MOD_SIZE_DEFAULT,
                                    "mod size",
                                    "?  Bad maximum modification size");
            continue;
          case MAX_READ_KEY:            /* set maximum read length */
            max_read_len = scan_int(RVM_PAGE_SIZE,1000*RVM_PAGE_SIZE,
                                    MAX_READ_LEN,"read_length",
                                    "?  Bad maximum read length");
            continue;
          case BRK_KEY:                 /* show break point */
            show_brk = rvm_true;
            continue;
/* version specific commands */
#if ((RVM_MAJOR_VERSION >= 2) && (RVM_MINOR_VERSION >= 0))
          case CHK_SUM_KEY:            /* set rvm_chk_sum switch */
            chk_sum_sw = rvm_true;
            continue;
          case CHK_RANGE_KEY:           /* set range check fraction */
            chk_range_frac = scan_int(0,100,CHK_RANGE_DEFAULT,
                                      "chk_range_frac",
                                      "? Bad chk_range fraction");
            continue;
          case VM_PROT_KEY:             /* set vm protection option */
            vm_protect_sw = rvm_true;
            continue;
          case INCR_TRUNC_TIME_KEY:     /* set incr trunc time slice */
            incr_trunc_time = scan_int(0,1000000,INCR_TRUNC_TIME,
                                       "incr trunc time slice",
                                       "?  Bad time slice size");
            continue;
          case EPOCH_TRUNC_KEY:         /* set epoch truncation fraction */
            epoch_trunc_frac = scan_int(trunc_frac,100,EPOCH_TRUNCATE,
                                  "epoch truncation fraction",
                                  "?  Bad epoch truncation percentage");
            continue;
#endif VERSION_TEST

/* LWP specific commands */
          case CHK_ALLOC_KEY:           /* get allocator check level */
            chk_alloc = scan_int(0,5,CHK_ALLOC_DEFAULT,
                                 "check_alloc",
                                 "?  Bad allocation checker level");
#ifndef RVM_USELWP
            printf("W  Allocation checking not available with Cthreads\n");
#endif  RVM_USELWP
            continue;
          case PLUMBER_KEY:             /* open plumber file */
            setup_plumber_file();
#ifndef RVM_USELWP
            printf("W  Allocation checking not available with Cthreads\n");
#endif  RVM_USELWP
            continue;

          default:  ASSERT(rvm_false);
            }
        /* check that all necessary parameters have been speced */
        if (strlen(LogFileName) == 0)
            {
            printf("\n?  Log file name must be specified\n");
            continue;
            }
        if ((strlen(DataFileName) == 0) && (show_brk == rvm_false))
            {
            printf("\n?  Data file name must be specified\n");
            continue;
            }
        break;
        }

    /* set LWP options */
#ifdef RVM_USELWP
    SetMallocCheckLevel(chk_alloc);
#endif RVM_USELWP
    
    /* esatblish RVM options */
    options = rvm_malloc_options();
    options->log_dev = LogFileName;
    options->flags &= ~RVM_ALL_OPTIMIZATIONS;
    options->flags |= optimizations;
    options->truncate = trunc_frac;
    options->flush_buf_len = flush_buf_len;
    options->recovery_buf_len = rec_buf_len;
    options->max_read_len = max_read_len;

#if ((RVM_MAJOR_VERSION >= 2) && (RVM_MINOR_VERSION >= 0))
    options->epoch_truncate = epoch_trunc_frac;
    options->incr_trunc_time = incr_trunc_time;
    if (chk_sum_sw)
        options->flags |= RVM_CHK_SUM;
    if (no_yield_sw)
        options->flags |= RVM_NO_YIELD;
    if (chk_range_frac != 0)
        options->flags |= RVM_CHK_RANGE;
    if (vm_protect_sw)
        options->flags |= RVM_VM_PROTECT;
#endif VERSION_TEST
    /* initialize RVM */
    ret = RVM_INIT(options);
    if  (ret != RVM_SUCCESS)
        {
	printf("? rvm_initialize failed, code: %s\n",rvm_return(ret));
        ASSERT(rvm_false);
        }
    else
	printf("rvm_initialize succeeded.\n");
    /* print break point if requested */
    if (show_brk) show_break();

    /* Load the data segment */
    rds_testsw = rvm_true;
    rds_load_heap(DataFileName, DataLen, &addr, &err);
    if (err == SUCCESS)
	printf("rds_start_heap successful\n");
    else
        {
        if (err > SUCCESS)
            printf("rds_start_heap = %s\n",
                   /* rvm_return((rvm_return_t *)err)); XXX BOGUS JET */
                   rvm_return((rvm_return_t)err));
        else
            printf("rds_start_heap = %d\n", err);
        ASSERT(rvm_false);
        }

    /* preload alloc_list */
    printf("Preallocating test blocks\n");
    for (i=num_pre_alloc; i>=0; i--)
        do_malloc(0);
    if (pre_alloc_trunc)
        {
        ret = rvm_truncate();
        if  (ret != RVM_SUCCESS)
            {
            printf("? rvm_truncate failed, code: %s\n",
                   rvm_return(ret));
            ASSERT(rvm_false);
            }
        }

    /* get current time */
    if (gettimeofday(&init_time,NULL) < 0)
        {
        perror("?  Error getting time of day");
        ASSERT(rvm_false);
        }
    if (time_tests)
        printf("\nTests started: %s\n",ctime(&init_time.tv_sec));
    /* start test run with input parameters */
    op_cnt = max_op_cnt;
    threads = (cthread_t *)malloc(sizeof(cthread_t) * nthreads);
    last = (int *)malloc(sizeof(int)*nthreads);

    /* fork the testing threads and run the tests */
    if (nthreads == 1)
        {
        last[0] = 0;
        worker(0);
        }
    else
        {
        for (i = 0; i < nthreads; i++)
            {
            last[i] = 0;
            threads[i] = cthread_fork(worker, i);
            }

        /* wait for all threads to terminate */
        while (nthreads > 0)
            condition_wait(&thread_exit_code,&thread_lock);
        cthread_yield();                /* let the last one out */
        }
    printf("\n All threads have exited\n");

    /* cleanup */
    if ((ret=rvm_terminate()) != RVM_SUCCESS)
        {
	printf("? rvm_terminate failed, code: %s\n",rvm_return(ret));
        ASSERT(rvm_false);
        }
    exit(0);
    }
