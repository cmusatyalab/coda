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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/rvm-src/rvm/Attic/rvm_private.h,v 4.5 1998/03/06 20:21:47 braam Exp $";
#endif _BLURB_

/*
*
*                 Internal Definitions for RVM
*
*/

/*LINTLIBRARY*/

/* permit multiple includes */
#ifndef _RVM_PRIVATE_
#define _RVM_PRIVATE_ 1

/* turn on debuging for now */
#ifndef DEBUG
#define DEBUG 1
#endif  DEBUG

#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include "rvm.h"
#include "rvm_statistics.h"

/* note: Log Version must change if Statistics Version changed */
#define RVM_LOG_VERSION     "RVM Log Version  1.4 Oct 17, 1997 "

/* general purpose macros */

/* make sure realloc knows what to do with null ptr */
#define REALLOC(x,l)      (((x)==NULL) ? malloc(l) : realloc((x),(l)))

/* bcopy <=> memcpy, defs of syscalls */
#ifdef __STDC__
#define BCOPY(x,y,n)     memcpy((y),(x),(n))
#define BZERO(x,n)       memset((x),0,(n))
#else
#define BCOPY(x,y,n)     bcopy((x),(y),(n))
#define BZERO(x,n)       bzero((x),(n))
#endif

/* loop terminated by explicit break */
#define DO_FOREVER  for (;;)
#define MACRO_BEGIN			do {
#define MACRO_END			} while(0)

#define FORWARD     rvm_true            /* record scan forward */
#define REVERSE     rvm_false           /* record scan reverse */
/* assert that preserves stack */
#ifdef ASSERT
#undef ASSERT
#endif
#define ASSERT(ex) \
    MACRO_BEGIN \
    if (!(ex)) \
        { \
        long _i_ = 0; \
        fflush(stdout); \
        fprintf(stderr,"Assertion failed: file \"%s\", line %d\n", \
                __FILE__, __LINE__); \
        fflush(stderr); \
         _i_ = *(long *)_i_; \
        abort(); \
         } \
    MACRO_END
/* RVM Internal Error Messages */

#define ERR_DATE_SKEW       "Current time before last recorded - check kernel date"
/* timestamp arithmetic */

/* comparison macros */
#define TIME_LSS(x,y)       (((x).tv_sec < (y).tv_sec) || \
                             ((((x).tv_sec == (y).tv_sec) && \
                             ((x).tv_usec < (y).tv_usec))))
#define TIME_GTR(x,y)       (((x).tv_sec > (y).tv_sec) || \
                             ((((x).tv_sec == (y).tv_sec) && \
                             ((x).tv_usec > (y).tv_usec))))
#define TIME_LEQ(x,y)       (!TIME_GTR((x),(y)))
#define TIME_GEQ(x,y)       (!TIME_LSS((x),(y)))
#define TIME_EQL(x,y)       (((x).tv_sec == (y).tv_sec) && \
                             ((x).tv_usec == (y).tv_usec))
#define TIME_EQL_ZERO(x)    (((x).tv_sec == 0 && ((x).tv_usec == 0)))

#define ZERO_TIME(x)        MACRO_BEGIN \
                            (x).tv_sec = 0; (x).tv_usec = 0; \
                            MACRO_END

/* scatter/gather i/o vector */
typedef struct
    {
    char            *vmaddr;            /* address of buffer */
    rvm_length_t    length;             /* buffer length */
    }
io_vec_t;

/* range monitoring vector */
typedef struct
    {
    char            *vmaddr;            /* range vm address */
    rvm_length_t    length;             /* range length */
    unsigned long   format;             /* data print format switches */
    int             radix;              /* print radix for vmaddr */
    }
chk_vec_t;

/* signal handler function type (rvmutl only) */
typedef rvm_bool_t rvm_signal_call_t();

/* recovery monitor call-back function type */
typedef void rvm_monitor_call_t();
/*  rvm_length_t    vmaddr;
    rvm_length_t    length;
    char            *data_ptr;
    rvm_offset_t    *data_offset;
    rec_hdr_t       *rec_hdr;
    rvm_length_t    index;
    char            *msg;
*/
/*                    round up & down macros
            **** all depend on sizes being a power of 2 ****
*/
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

#define SECTOR_SIZE          512
#define SECTOR_MASK          ((rvm_length_t)(~(SECTOR_SIZE-1)))
#define ROUND_TO_SECTOR_SIZE(x) (((rvm_length_t)(x)+SECTOR_SIZE-1) \
                                 &SECTOR_MASK)
#define CHOP_TO_SECTOR_SIZE(x)  ((rvm_length_t)(x)&SECTOR_MASK)

#define SECTOR_INDEX(x)      ((x) & (SECTOR_SIZE-1))

#define ROUND_OFFSET_TO_SECTOR_SIZE(x) \
    rvm_rnd_offset_to_sector(&(x))

#define CHOP_OFFSET_TO_SECTOR_SIZE(x) \
    (RVM_MK_OFFSET(RVM_OFFSET_HIGH_BITS_TO_LENGTH(x), \
                   CHOP_TO_SECTOR_SIZE(RVM_OFFSET_TO_LENGTH(x))))

#define OFFSET_TO_SECTOR_INDEX(x) \
    (SECTOR_INDEX(RVM_OFFSET_TO_LENGTH((x))))

#define CHOP_OFFSET_TO_LENGTH_SIZE(x) \
    (RVM_MK_OFFSET(RVM_OFFSET_HIGH_BITS_TO_LENGTH(x), \
                   CHOP_TO_LENGTH(RVM_OFFSET_TO_LENGTH(x))))

#define ROUND_TO_PAGE_SIZE(x) (((rvm_length_t)(x)+page_size-1) \
                               &page_mask)
#define CHOP_TO_PAGE_SIZE(x)  ((rvm_length_t)(x)&page_mask)

/* other stuff... */

#define OFFSET_TO_FLOAT(x) \
    ((4.294967e+9)*((float)(RVM_OFFSET_HIGH_BITS_TO_LENGTH(x))) \
     + (float)(RVM_OFFSET_TO_LENGTH(x)))
/* internal structure id's */
typedef enum
    {
    struct_first_id = 9,                /* base for free list array length */

                                        /* free list allocated structures */
    log_id,                             /* log device descriptor */
    int_tid_id,                         /* internal transaction descriptor */
    tid_rvm_id,                         /* external tid while on free list */
    range_id,                           /* range descriptor */
    seg_id,                             /* segment descriptor */
    region_id,                          /* internal region descriptor */
    region_rvm_id,                      /* external region while on free list */
    options_rvm_id,                     /* external options while on free list */
    statistics_rvm_id,                  /* rvm_statistics record while on free list */
    mem_region_id,                      /* vm region tree node */
    dev_region_id,                      /* device region tree node */
    log_special_id,                     /* special log record */

    struct_last_cache_id,               /* marker for free lists array length */

                                        /* non-free list allocated structures */

    log_status_id,                      /* log status descriptor */
    log_dev_status_id,                  /* log device status area (on disk) */
    log_wrap_id,                        /* log wrap-around marker */
    log_seg_id,                         /* segment mapping marker in log */
    seg_dict_id,                        /* recovery dictionary segment desc. */
    trans_hdr_id,                       /* transaction header in log */
    rec_end_id,                         /* log record end marker */
    nv_range_id,                        /* new value range header */
    nv_buf_id,                          /* new value vm buffer */
    free_page_id,                       /* free page header descriptor */
    rw_qentry_id,                       /* rw_lock queue entry */
    tree_root_id,                       /* tree root */
    /* mmapped_list_id,*/                    /* BSD/mmap systems only */
    struct_last_id                      /* marker for last structure id */
   }
struct_id_t;
/* macros to use struct_id's as int's & vice versa
   for free list allocated structures only */
#define ID_INDEX(id)    ((rvm_length_t)(id)-(rvm_length_t)struct_first_id-1)
#define INDEX_ID(i)     ((struct_id_t)((i)+1+(long)struct_first_id))

/* number of free list allocated structures */
#define NUM_CACHE_TYPES ((rvm_length_t)struct_last_cache_id \
                         -(rvm_length_t)struct_first_id-1)
#define NUM_TYPES       ((rvm_length_t)struct_last_id \
                         -(rvm_length_t)struct_first_id-1)

/* preallocation sizes for internal structure free lists
   must be in same order as above free list allocted enum's
*/
#define NUM_PRE_ALLOCATED \
    0,                                  /* log's */ \
    20,                                 /* tid's */ \
    20,                                 /* rvm_tid's */ \
    50,                                 /* range's */ \
    0,                                  /* seg's */ \
    10,                                 /* region's */ \
    0,                                  /* rvm_region's */ \
    0,                                  /* rvm_options */ \
    2,                                  /* rvm_statistics */ \
    10,                                 /* mem_region nodes */ \
    1,                                  /* dev_region nodes */ \
    1                                   /* special log markers */

/* maximum sizes for internal structure free lists
   must be in same order as above free list allocted enum's
*/
#define MAX_ALLOCATED \
    0,                                  /* log's */ \
    20,                                 /* tid's */ \
    20,                                 /* rvm_tid's */ \
    50,                                 /* range's */ \
    0,                                  /* seg's */ \
    10,                                 /* region's */ \
    0,                                  /* rvm_region's */ \
    0,                                  /* rvm_options */ \
    2,                                  /* rvm_statistics */ \
    10,                                 /* mem_region nodes */ \
    2000,                               /* dev_region nodes */ \
    1                                   /* special log markers */
/* sizes and names of internal types
   must be in same order as above enum's
*/
#define CACHE_TYPE_SIZES \
    sizeof(log_t), \
    sizeof(int_tid_t), \
    sizeof(rvm_tid_t), \
    sizeof(range_t), \
    sizeof(seg_t), \
    sizeof(region_t), \
    sizeof(rvm_region_t), \
    sizeof(rvm_options_t), \
    sizeof(rvm_statistics_t), \
    sizeof(mem_region_t), \
    sizeof(dev_region_t), \
    sizeof(log_special_t)

#define OTHER_TYPE_SIZES \
    0, \
    sizeof(log_status_t), \
    sizeof(log_dev_status_t), \
    sizeof(log_wrap_t), \
    sizeof(log_seg_t), \
    sizeof(seg_dict_t), \
    sizeof(trans_hdr_t), \
    sizeof(rec_end_t), \
    sizeof(nv_range_t), \
    sizeof(nv_buf_t), \
    sizeof(free_page_t), \
    sizeof(rw_qentry_t), \
    sizeof(tree_root_t)/*, \
    sizeof(mmapped_list_t)*/

#define TYPE_NAMES \
    "log_id", \
    "int_tid_id", \
    "tid_rvm_id", \
    "range_id", \
    "seg_id", \
    "region_id", \
    "region_rvm_id", \
    "options_rvm_id", \
    "statistics_rvm_id", \
    "mem_region_id", \
    "dev_region_id", \
    "log_special_id", \
    "struct_last_cache_id", \
    "log_status_id", \
    "log_dev_status_id", \
    "log_wrap_id", \
    "log_seg_id", \
    "seg_dict_id", \
    "trans_hdr_id", \
    "rec_end_id", \
    "nv_range_id", \
    "nv_buf_id", \
    "free_page_id", \
    "rw_qentry_id", \
    "tree_root_id"/*, \
    "mmapped_list_id"*/
/* doubly-linked list cell header
   this structure serves as the link and struct_id carrier for larger
   structures when declared as the 1st field of the structure.
   it is also used as the root, or header, of a list when statically allocated,
   or embedded in another structure as other than the 1st field,
   in which case its struct_id is that of the type of elements on the list.
*/
typedef struct list_entry_s
    {
    struct list_entry_s *nextentry;	/* in accordance with insque(3) */
    struct list_entry_s *preventry;
    union
        {
        struct list_entry_s  *name;     /* back pointer to head of list */
        long                 length;    /* length of list if header */
        }               list;
    struct_id_t         struct_id;	/* self identifier; NEVER altered */
    rvm_bool_t          is_hdr;         /* true if list header */
    }
list_entry_t;

/* list macros, lst: address of list header */
#define LIST_EMPTY(lst)     ((lst).list.length == 0)
#define LIST_NOT_EMPTY(lst) ((lst).list.length != 0)

/* list iterators for simple list traversals, no unlinking */
#define FOR_ENTRIES_OF(lst,type,ptr)    /* list iterator, FIFO order */ \
    for ( \
         (ptr) = (type *)((lst).nextentry); \
         !((ptr)->links.is_hdr); \
         (ptr) = (type *)((ptr)->links.nextentry) \
         )

#define FOR_REVERSE_ENTRIES_OF(lst,type,ptr) /* list iterator, LIFO order */ \
    for ( \
         (ptr) = (type *)((lst).preventry); \
         !((ptr)->links.is_hdr); \
         (ptr) = (type *)((ptr)->links.preventry) \
         )

/* list iterators for traversals that unlink the entries */
#define UNLINK_ENTRIES_OF(lst,type,ptr) /* list generator, FIFO order */ \
    for ( \
         (ptr) = (type *)((lst).nextentry); \
         !((ptr)->links.is_hdr); \
         (ptr) = (type *)((lst).nextentry) \
         )

#define UNLINK_REVERSE_ENTRIES_OF(lst,type,ptr) /* list generator, LIFO order */ \
    for ( \
         (ptr) = (type *)((lst).preventry); \
         !((ptr)->links.is_hdr); \
         (ptr) = (type *)((lst).preventry) \
         )

/* free page list entry */
typedef struct
    {
    list_entry_t        links;          /* list links */
    rvm_length_t        len;            /* length of free pages in bytes */
    }
free_page_t;
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

/* protect a critical section
   lck:  address of mutex
   body: the critical section code
*/
#define CRITICAL(lck,body) \
    MACRO_BEGIN \
    mutex_lock(&(lck)); \
    body; \
    mutex_unlock(&(lck)); \
    MACRO_END

/*  rw_lock (read/write) support
    An rw_lock permits many readers of a structure, but only
    if there is no writer pending.  Only a single writer is permitted,
    and to get the write lock, there must be no readers.
    If a write is requested, no additional readers will be permitted
    until the write is satisfied.  Blocked threads are processed in
    FIFO order.
*/
typedef enum                            /* rw_lock access modes */
    {
    r = 32,                             /* get lock for read-only */
    w,                                  /* get lock for read/write */
    f                                   /* lock free, (internal use only) */
    }
rw_lock_mode_t;

typedef struct                          /* rw_lock structure */
    {
    RVM_MUTEX           mutex;          /* mutex to protect rw_lock innards */
    long                read_cnt;       /* read lock count, 0 ==> free */
    long                write_cnt;      /* write lock count, 0 ==> free */
    list_entry_t        queue;          /* blocked thread queue */
    rw_lock_mode_t      lock_mode;      /* current lock mode */
    }
rw_lock_t;

typedef struct                          /* rw_lock queue entry */
    {
    list_entry_t        links;          /* queue links & struct_id */
    RVM_CONDITION       wait;           /* condition code for blocking */                                          
    rw_lock_mode_t      mode;           /* access mode */
    }
rw_qentry_t;

/* protect a rw_lock critical section
   lck:  address of rw_lock
   mode: r or w
   body: the critical section code
*/
#define RW_CRITICAL(rwl,mode,body) \
    MACRO_BEGIN \
    rw_lock(&(rwl),(mode)); \
    body; \
    rw_unlock(&(rwl),(mode)); \
    MACRO_END

/* macro for testing if an rw_lock is free */
#define RW_LOCK_FREE(rwl) \
    (((rwl).read_cnt+(rwl).write_cnt) == 0) && ((rwl).lock_mode == f)
/* tree node structures */

typedef struct tree_node_s              /* basic tree node */
    {
    struct tree_node_s  *lss;           /* ptr to less than entry */
    struct tree_node_s  *gtr;           /* ptr to greater than entry */
    long                bf;             /* balance factor */
    struct_id_t         struct_id;      /* self identifier */
    }
tree_node_t;

typedef union
    {
    tree_node_t         node;           /* links for trees */
    list_entry_t        entry;          /* links for allocation cache */
    }
tree_links_t;

typedef enum                            /* traversal states */
    {
    lss = 50,
    self,
    gtr,
    init
    }
traverse_state_t;

typedef struct                          /* tree traversal position entry */
    {
    tree_node_t         *ptr;           /* node pointer */
    traverse_state_t     state;          /* state of traversal {lss,self,gtr} */
    }
tree_pos_t;

typedef struct                          /* tree root structure */
    {
    struct_id_t         struct_id;      /* self identifier */
    tree_node_t         *root;          /* ptr to root node */
    tree_pos_t          *traverse;      /* traversal position vector */
    rvm_length_t        traverse_len;   /* max length of traverse vector */
    long                level;          /* current position in traversal
                                           vector */
    rvm_length_t        n_nodes;        /* number of nodes in tree */
    rvm_length_t        max_depth;      /* length of deepest path in tree */
    rvm_bool_t          unlink;         /* unlink nodes as traversed */
    }
tree_root_t;

#define TRAVERSE_LEN_INCR  15           /* allocate 15 slots at a time */
/* tree structure iterators 
     -- nodes are delinked as traversed
     -- do not use tree_insert or tree_delete or otherwise change
        tree shape in body of iterators if iteration is to be continued
     -- iterators may not be nested for same tree
*/
#define FOR_NODES_OF(tree,type,ptr)     /* tree iterator, lss -> gtr order */ \
    for ( \
         (ptr) = (type *)init_tree_generator(&(tree),FORWARD,rvm_false); \
         (ptr) != NULL; \
         (ptr) = (type *)tree_successor(&(tree)) \
         )

#define FOR_REVERSE_NODES_OF(tree,type,ptr) /* tree iterator, gtr -> lss order */ \
    for ( \
         (ptr) = (type *)init_tree_generator(&(tree),REVERSE,rvm_false); \
         (ptr) != NULL; \
         (ptr) = (type *)tree_predecessor(&(tree)) \
         )

/* insertion test and iterate from existing nodes with equivalent key */
#define FROM_EXISTING_NODE_OF(tree,type,ptr,node,cmp) \
    for ( \
         (ptr) = (type *)tree_iterate_insert(&(tree),(node),(cmp)); \
         (ptr) != NULL; \
         (ptr) = (type *)tree_successor(&(tree)) \
         )

#define UNLINK_NODES_OF(tree,type,ptr)  /* tree iterator, lss -> gtr order */ \
    for ( \
         (ptr) = (type *)init_tree_generator(&(tree),FORWARD,rvm_true); \
         (ptr) != NULL; \
         (ptr) = (type *)tree_successor(&(tree)) \
         )

#define UNLINK_REVERSE_NODES_OF(tree,type,ptr) /* tree iterator, gtr -> lss order */ \
    for ( \
         (ptr) = (type *)init_tree_generator(&(tree),REVERSE,rvm_true); \
         (ptr) != NULL; \
         (ptr) = (type *)tree_predecessor(&(tree)) \
         )
/* Structure to remember where we have/have not mmapped */

/* vm buffers for dev_region_t nodes */
typedef struct
    {
    struct_id_t         struct_id;      /* self identifier */
    rvm_length_t        ref_cnt;        /* references to buffer */
    rvm_length_t        chk_sum;        /* data buffer checksum */
    rvm_length_t        alloc_len;      /* allocated length of buffer */
    rvm_length_t        data_len;       /* length of log data */
    char                *buf;           /* start of data area */
    }
nv_buf_t;

#define NV_BUF_SIZE(len)  (ROUND_TO_LENGTH((len)) + sizeof(nv_buf_t))

/* storage device region node */
typedef struct
    {
    tree_links_t        links;          /* ptr structure */
    rvm_offset_t        offset;         /* segment start offset of changes */
    rvm_offset_t        end_offset;     /* end offset (offset + length) */
    rvm_length_t        length;         /* length of region */
    char                *nv_ptr;        /* ptr into nv_buf */
    nv_buf_t            *nv_buf;        /* buffer for new values if allocated */
    rvm_offset_t        log_offset;     /* location of new values in log */
    char                *vmaddr;        /* original vm addr (debug use only) */
    }
dev_region_t;

/* virtual memory region node */
typedef struct
    {
    tree_links_t    links;              /* ptr structure */
    struct region_s *region;            /* region descriptor */
    char            *vmaddr;            /* base address */
    rvm_length_t    length;             /* length of vm region */
    } 
mem_region_t;

/* node comparator function type */
typedef long cmp_func_t();
/*  tree_node_t     *node1;
    tree_node_t     *node2;
*/
/* log records written by commit, and associated with new value records */

/* transaction record header: trans_hdr_t -- a single copy in the log descriptor
*/
typedef struct
    {
    struct_id_t     struct_id;          /* self-identifier */
    rvm_length_t    rec_length;         /* log record length, displacement to
                                           end mark */
    struct timeval  timestamp;          /* timestamp of log entry */
    rvm_length_t    rec_num;            /* record number of log entry */
    rvm_length_t    num_ranges;         /* number of ranges in record */
    struct timeval  uname;              /* uname of transaction */
    struct timeval  commit_stamp;       /* timestamp of commit */
    rvm_length_t    n_coalesced;        /* count of coalesced transactions */
    rvm_length_t    flags;              /* mode and optimization flags */
    }
trans_hdr_t;

/* new value record range header: nv_range_t */
typedef struct
    {
    struct_id_t     struct_id;          /* self-identifier */
    rvm_length_t    rec_length;         /* total nv record length, displacement
                                           to next nv_range or end mark */
    struct timeval  timestamp;          /* timestamp of log entry */
    rvm_length_t    rec_num;            /* record number of entry */
    rvm_length_t    sub_rec_len;        /* back displacement to previous hdr */
    rvm_length_t    range_num;          /* range number in record */
    rvm_length_t    length;             /* actual modification length */
    rvm_offset_t    offset;             /* offset of changes in segment */
    char            *vmaddr;            /* modification vm address */
    rvm_length_t    chk_sum;            /* data checksum */
    long            seg_code;           /* segment short name */
    rvm_bool_t      is_split;           /* is a range split for log wrap */
    }
nv_range_t;
/* special log types -- these records are inserted into the log to
   record events not related to transaction commit and new value
   recording.
   These are generally used by the recovery algorithm to reconstruct the
   committed images of segments at the time of a crash.
*/
/* segment mapping descriptor -- inserted by map when a segment
   is mapped the first time; used to relate the short names to
   an actual device or file name */
typedef struct
    {
    long            seg_code;           /* segment short name */
    rvm_offset_t    num_bytes;          /* maximum usable length of seg dev */
    long            name_len;           /* length of segment name */
    char            *name;              /* full path name */
    }
log_seg_t;

/* log_special_t: the carrier for all special log types
   free list allocated; additional type-dependent data can be placed
   after this structure; all records end with rec_end_t record
*/
typedef struct
    {
    list_entry_t    links;              /* list links and free list struct id */
                                        /* following fields are written in log */
    struct_id_t     struct_id;          /* special log type id (union tag) */
    rvm_length_t    rec_length;         /* log record length, displacement to
                                           end mark */
    struct timeval  timestamp;          /* timestamp of record entry */
    rvm_length_t    rec_num;            /* record number of entry */
    union
        {
        log_seg_t   log_seg;            /* segment mapping marker */
        }           special;
    }
log_special_t;
/* generic log entry types */

/* log record end marker: rec_end_t -- a single copy in the log descriptor */
typedef struct
    {
    struct_id_t     struct_id;          /* self-identifier */
    rvm_length_t    rec_length;         /* back displacement to record header */
    struct timeval  timestamp;          /* timestamp of log record */
    rvm_length_t    rec_num;            /* record number of entry */
    struct_id_t     rec_type;           /* type of recorded ended */
    rvm_length_t    sub_rec_len;        /* back displacement to previous sub-
                                           record; same as rec_length if none */
    }
rec_end_t;

/* log wrap-around marker -- a single copy in the log descriptor */
typedef struct
    {
    struct_id_t     struct_id;          /* self-identifier */
    rvm_length_t    rec_length;         /* own size, so same as log_rec_head_t */
    struct timeval  timestamp;          /* timestamp of wrap around */
    rvm_length_t    rec_num;            /* record number of entry */
    struct_id_t     struct_id2;         /* for scan_wrap_reverse()! */
    }
log_wrap_t;

/* generic record header; not actually allocated, but any record header
   can be cast to this to get its type & length for detailed analysis 
*/
typedef struct
    {
    struct_id_t     struct_id;          /* type of entry */
    rvm_length_t    rec_length;         /* record length */
    struct timeval  timestamp;          /* timestamp of record entry */
    long            rec_num;            /* record number of entry */
    }
rec_hdr_t;
/* device descriptor -- included in log and segment descriptors */
typedef struct
    {
    char            *name;              /* print name of device */
    long            name_len;           /* allocation length */
    long            handle;             /* device handle */
    rvm_offset_t    num_bytes;          /* length of device */
    rvm_bool_t      raw_io;             /* true if using raw i/o */
    unsigned long   type;                /* to store device type */
    rvm_bool_t      read_only;          /* true if opened read-only */

    io_vec_t        *iov;               /* gather write io vector */
    long            iov_len;            /* length of iov array */
    long            iov_cnt;            /* count of entries used in iov */
    rvm_length_t    io_length;          /* accumulated length of i/o */
    rvm_offset_t    last_position;      /* last location seeked or transfered */
                                        /* the following fields are used for
                                           log devices only */
    char            *wrt_buf;           /* working raw io write buffer base */
    rvm_length_t    wrt_buf_len;        /* usable wrt_buf length */
    char            *ptr;               /* write buffer fill ptr */
    char            *buf_start;         /* start of buffer flush region */
    char            *buf_end;           /* end of buffer */
    rvm_offset_t    sync_offset;        /* end offset after last sync */

    char            *pad_buf;           /* padding buffer */
    long            pad_buf_len;        /* length of current pad buf */
    }
device_t;
/* log structure macros */

#define RANGE_LEN(range)    (ALIGNED_LEN((range)->nv.vmaddr, \
                                         (range)->nv.length))

#define RANGE_SIZE(range)   ((rvm_length_t)(NV_RANGE_OVERHEAD \
                            + RANGE_LEN(range)))

#define TRANS_SIZE          (ROUND_TO_LENGTH((sizeof(trans_hdr_t) \
                                              + sizeof(rec_end_t))))

#define NV_RANGE_OVERHEAD   (ROUND_TO_LENGTH(sizeof(nv_range_t)))

#define MIN_NV_RANGE_SIZE   (NV_RANGE_OVERHEAD+64)

#define MIN_TRANS_SIZE      (TRANS_SIZE + MIN_NV_RANGE_SIZE \
                             + ROUND_TO_LENGTH(sizeof(log_wrap_t)))

#define LOG_SPECIAL_SIZE    (ROUND_TO_LENGTH(sizeof(log_special_t) \
                                             - sizeof(list_entry_t)))

#define LOG_SPECIAL_IOV_MAX 3

/* largest log type header on disc */
#define MAX_HDR_SIZE        (ROUND_TO_LENGTH((sizeof(log_special_t) \
                                             + MAXPATHLEN)))
/* other constants */

/* maximum size nv's kept in vm during recovery */
#define NV_LOCAL_MAX        (8*1024 - ROUND_TO_LENGTH(NV_BUF_SIZE( \
                                              sizeof(rvm_length_t)+1)))

/* size of status area i/o buffer */
#define LOG_DEV_STATUS_SIZE \
                ROUND_TO_SECTOR_SIZE(sizeof(log_dev_status_t))

/* offsets for log status structures in files and partitions */
#define RAW_STATUS_OFFSET   16*SECTOR_SIZE
#define FILE_STATUS_OFFSET  0

#define UPDATE_STATUS       100         /* flushes before updating log status area */
/* log status descriptor -- included in the log descriptor */
#ifdef RVM_LOG_TAIL_SHADOW
extern rvm_offset_t log_tail_shadow;
extern rvm_bool_t   has_wrapped;
#define RVM_ASSIGN_OFFSET(x,y)  (x) = (y)
#endif RVM_LOG_TAIL_SHADOW

typedef struct
    {
                                        /* status area control fields */
    long            update_cnt;         /* number of updates before write */
    rvm_bool_t      valid;              /* data in status area valid */
    rvm_bool_t      log_empty;          /* true if log device & buffer empty */

                                        /* log pointers & limits */
    rvm_offset_t    log_start;          /* first offset for records */
    rvm_offset_t    log_size;           /* dev.num_bytes - log_start:
                                           space for records */
    rvm_offset_t    log_head;           /* current log head */
    rvm_offset_t    log_tail;           /* current log tail */
    rvm_offset_t    prev_log_head;      /* previous head (truncation only) */
    rvm_offset_t    prev_log_tail;      /* previous tail (truncation only) */

                                        /* consistency check fields */
    struct timeval  status_init;        /* timestamp log creation */
    struct timeval  status_write;       /* timestamp for last status write*/
    struct timeval  last_trunc;         /* timestamp for last truncation */
    struct timeval  prev_trunc;         /* timestamp for previous truncation */
    struct timeval  first_write;        /* timestamp of first record in log */
    struct timeval  last_write;         /* timestamp of last record in log */
    struct timeval  first_uname;        /* first transaction uname in log */
    struct timeval  last_uname;         /* last transaction uname in log */
    struct timeval  last_commit;        /* last transaction commit timestamp */
    struct timeval  wrap_time;          /* wrap timestamp if log wrapped */
    rvm_length_t    first_rec_num;      /* 1st rec num of truncation epoch */
    rvm_length_t    last_rec_num;       /* last rec num of truncation epoch */
    rvm_length_t    next_rec_num;       /* assignment counter for rec_nums */

                                        /* transaction statistics */
    rvm_length_t    n_abort;            /* number of transactions aborted */
    rvm_length_t    n_flush_commit;     /* number of flush mode commits */
    rvm_length_t    n_no_flush_commit;  /* number of no_flush mode commits */
    rvm_length_t    n_split;            /* number trans split for log wrap */
    rvm_length_t    n_truncation_wait;  /* transactions delayed by truncation */

                                        /* log statistics */
    rvm_length_t    n_flush;            /* number of internal flushes */
    rvm_length_t    n_rvm_flush;        /* number of explicit flush calls */
    rvm_length_t    n_special;          /* number of special log records */
    rvm_offset_t    range_overlap;      /* current overlap eliminated by range coalesce */
    rvm_offset_t    trans_overlap;      /* current overlap eliminated by trans coalesce */
    rvm_length_t    n_range_elim;       /* number of ranges eliminated by
                                           range coalesce/flush */
    rvm_length_t    n_trans_elim;       /* number of ranges eliminated by
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

    rvm_length_t    flush_times[flush_times_len]; /* flush timings */
    rvm_length_t    range_lengths[range_lengths_len]; /* range lengths flushed */
    rvm_length_t    range_elims[range_elims_len]; /* num ranges eliminated by
                                                     range coalesce/flush */
    rvm_length_t    trans_elims[trans_elims_len]; /* num ranges eliminated by
                                                     trans coalesce/flush */
    rvm_length_t    range_overlaps[range_overlaps_len]; /* space saved by
                                                           range coalesce/flush */
    rvm_length_t    trans_overlaps[range_overlaps_len]; /* space saved by
                                                           trans coalesce/flush */

                                        /* cummulative transaction stats */
    rvm_length_t    tot_abort;          /* total aborted transactions */
    rvm_length_t    tot_flush_commit;   /* total flush commits */
    rvm_length_t    tot_no_flush_commit; /* total no_flush commits */
    rvm_length_t    tot_split;          /* total transactions split for log
                                           wrap-around */

                                        /* cummulative log statistics */
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

                                        /* truncation statistics */
    rvm_length_t    tot_rvm_truncate;   /* total explicit rvm_truncate calls */
    rvm_length_t    tot_async_truncation; /* total asynchronous truncations */
    rvm_length_t    tot_sync_truncation; /* total forced synchronous truncations */
    rvm_length_t    tot_truncation_wait; /* total transactions delayed by truncation */
    rvm_length_t    tot_recovery;       /* total recovery truncations */
    struct timeval  tot_flush_time;     /* total time spent in flush */
    struct timeval  tot_truncation_time; /* cumulative truncation time */

                                        /* histogram vectors */

    rvm_length_t    tot_tree_build_times[truncation_times_len]; /* truncation timings */
    rvm_length_t    tot_tree_apply_times[truncation_times_len];
    rvm_length_t    tot_truncation_times[truncation_times_len];
    rvm_length_t    tot_flush_times[flush_times_len]; /* cummulative flush timings */
    rvm_length_t    tot_range_lengths[range_lengths_len]; /* cummulative range lengths flushed */
    rvm_length_t    tot_range_elims[range_elims_len]; /* total num ranges eliminated by
                                                         range coalesce/flush */
    rvm_length_t    tot_trans_elims[trans_elims_len]; /* total num ranges eliminated by                                                 trans coalesce/flush */
    rvm_length_t    tot_range_overlaps[range_overlaps_len]; /* space saved by
                                                           range coalesce/flush */
    rvm_length_t    tot_trans_overlaps[range_overlaps_len]; /* space saved by
                                                           trans coalesce/flush */
    rvm_length_t    tot_trans_coalesces[trans_coalesces_len]; /* transactions coalesced
                                                                 per flush  */
    rvm_length_t    flush_state;        /* flush status */
    rvm_length_t    trunc_state;        /* truncation status */
    }
log_status_t;
/* log status descriptor on log device: log_dev_status_t */
typedef struct
    {
    struct_id_t     struct_id;          /* self identifier */
    rvm_length_t    chk_sum;            /* check sum */
    char            version[RVM_VERSION_MAX]; /* RVM interface version string */
    char            log_version[RVM_VERSION_MAX]; /* RVM log version string */
    char            statistics_version[RVM_VERSION_MAX]; /* RVM statistics version string */
    log_status_t    status;             /* log status info */
    }
log_dev_status_t;

/* Flush and Truncation states */
                                        /* log flush initiated by rvm_flush */
#define RVM_FLUSH_CALL      (1)
                                        /* log flush initated by commit */
#define RVM_FLUSH_COMMIT    (2)
                                        /* truncation initiated by rvm_truncate */
#define RVM_RECOVERY        (4)
#define RVM_TRUNCATE_CALL   (010)
                                        /* truncation initiated by rvm daemon */   
#define RVM_ASYNC_TRUNCATE  (020)
                                        /* truncation forced by flush */
#define RVM_SYNC_TRUNCATE   (040)
                                        /* truncation phase 1: find current log tail */   
#define RVM_TRUNC_FIND_TAIL (0100)
                                        /* phase 2: build modification trees */                                 
#define RVM_TRUNC_BUILD_TREE (0200)
                                        /* phase 3: apply modifications */
#define RVM_TRUNC_APPLY     (0400)
                                        /* phase 4: update log status */                                 
#define RVM_TRUNC_UPDATE    (01000)

#define RVM_TRUNC_PHASES    (RVM_TRUNC_FIND_TAIL | RVM_TRUNC_BUILD_TREE \
                             | RVM_TRUNC_APPLY | RVM_TRUNC_UPDATE)

/* log recovery buffer descriptor -- single copy in log descriptor */
typedef struct
    {
    char            *buf;               /* working recovery buffer base */
    char            *shadow_buf;
    long            length;             /* length of allocated buffer */
    rvm_offset_t    buf_len;            /* log buffer length as offset */
    long            r_length;           /* length of data read into buffer */
    rvm_offset_t    offset;             /* offset of buffer start in segment */
    long            ptr;                /* index of present buffer position */
    struct timeval  timestamp;          /* timestamp of transaction in buffer */

    char            *aux_buf;           /* working auxillary buffer base */
    long            aux_length;         /* length of aux_buf */
    rvm_offset_t    aux_offset;         /* offset of data in buffer */
    long            aux_rlength;        /* length of data read into buffer */

    struct timeval  prev_timestamp;     /* timestamp of previous record */
    rvm_length_t    prev_rec_num;       /* previous record number */
    rvm_bool_t      prev_direction;     /* last scanning direction */
    rvm_bool_t      split_ok;           /* ok to process split records */
    }
log_buf_t;

/* log buffer management defs */

#define SYNCH       rvm_true            /* synchronization required */
#define NO_SYNCH    rvm_false           /* synchronization not required */
/* log truncation daemon control structures */

typedef enum
    {
    rvm_idle = 1000,                    /* daemon idle */
    init_truncate,                      /* initiate truncation */
    truncating,                         /* truncation in progress */
    terminate,                          /* shutdown */
    error                               /* terminated due to error */
    }
daemon_state_t;

typedef struct
    {
    cthread_t       thread;             /* daemon thread handle */
    RVM_MUTEX       lock;               /* daemon lock -- protects following
                                           fields */
    RVM_CONDITION   code;               /* condition code to signal daemon */
    RVM_CONDITION   flush_flag;         /* condition code to signal flush */
    RVM_CONDITION   wake_up;            /* conditon code to signal threads
                                           waiting for truncation completion */
    daemon_state_t  state;              /* control state */
    long            truncate;           /* truncation threshold, as % of log */
    }
log_daemon_t;
/* log descriptor */
typedef struct
    {
    list_entry_t    links;              /* list links and struct id -- points
                                           to log list root */
    long            ref_cnt;            /* count seg's using this log device */

    RVM_MUTEX       dev_lock;           /* log device lock, protects device and
                                           following i/o related fields: */
    device_t        dev;                /* log device descriptor */
    log_status_t    status;             /* log status area descriptor */
    trans_hdr_t     trans_hdr;          /* i/o header for transaction log entry */
    rec_end_t       rec_end;            /* i/o end marker for log entry */
    log_wrap_t      log_wrap;           /* i/o log wrap-around marker */
    log_buf_t       log_buf;            /* log recovery buffer */
                                        /* end of log_dev_lock protected fields */

    RVM_MUTEX       tid_list_lock;      /* lock for tid list header & links
                                           used when adding/deleting a tid */
    list_entry_t    tid_list;           /* root of active transaction list */

    RVM_MUTEX       flush_list_lock;    /* lock for flush list header & links
                                           used to add/delete a no_flush tid */
    list_entry_t    flush_list;         /* list of no_flush committed tid's */

    RVM_MUTEX       special_list_lock;  /* lock for special list header & links
                                           used to add/delete a special entry */
    list_entry_t    special_list;       /* list of special log entries */

    rw_lock_t       flush_lock;         /* log flush synchronization */
    log_daemon_t    daemon;             /* truncation daemon control */
    RVM_MUTEX       truncation_lock;    /* truncation synchronization */
    cthread_t       trunc_thread;
    rvm_bool_t      in_recovery;        /* true if in recovery */

    struct seg_dict_s
                    *seg_dict_vec;      /* recovery segment dictionary */
    long            seg_dict_len;       /* length of seg_dict_vec */
    device_t        *cur_seg_dev;       /* current segment device in truncation */
    }
log_t;
/* segment descriptor: seg_t */
typedef struct
    {
    list_entry_t    links;              /* list links and struct id */

    RVM_MUTEX       dev_lock;           /* device lock */
    device_t        dev;                /* segment device descriptor */ 
    long            seg_code;           /* short name for log entries */
    log_t           *log;               /* log descriptor ptr */

    RVM_MUTEX       seg_lock;           /* lock for seg lists: protects header
                                           and links -- used when mapping or
                                           unmapping a region */
    list_entry_t    map_list;           /* mapped region list header */
    list_entry_t    unmap_list;         /* unmapped region list header */

    rvm_bool_t      threads_waiting;    /* at least one thread is waiting to
                                           map a previously unmapped region */
    }
seg_t;

/* recovery dictionary segment descriptor: seg_dict_t */
struct seg_dict_s
    {
    struct_id_t     struct_id;          /* self-identifier */
    seg_t           *seg;               /* ptr to real segment */
    device_t        dev;                /* used in recovery only */
    long            seg_code;           /* short segment id */
    tree_root_t     mod_tree;           /* modification tree for recovery */
    };

typedef struct seg_dict_s seg_dict_t;

#define SEG_DICT_INDEX(x)   ((x)-1)     /* index of segemnt in seg_dict_vec */
/* region descriptor: region_t */
typedef struct region_s
    {
    list_entry_t    links;              /* list links and struct id
                                           -- protected by seg.map_lock
                                              or seg.unmap_lock */
    rw_lock_t       region_lock;        /* rw lock for following fields */
    seg_t           *seg;               /* back ptr to segment */
    mem_region_t    *mem_region;        /* back ptr to region tree node */
    rvm_offset_t    offset;             /* offset of region base in segment */
    rvm_offset_t    end_offset;         /* offset of region end in segment */
    char            *vmaddr;            /* virtual memory base address */
    rvm_length_t    length;             /* length of region */
    rvm_bool_t      no_copy;            /* data not copied on map */

    RVM_MUTEX       count_lock;         /* accounting lock for next 2 fields */
    long            n_uncommit;         /* # uncommitted modifications in region */
    rvm_bool_t      dirty;              /* dirty bit; set by end_transaction */

    struct timeval  unmap_ts;           /* unmap timestamp for truncation chk */
    }
region_t;

/* modification range descriptor: range_t */
typedef struct
    {
    tree_links_t    links;              /* tree links and struct id */
    char            *data;              /* old/new values, when used */
    rvm_length_t    data_len;           /* allocation length of data buffer */
    char            *nvaddr;            /* address of saved new values */
    region_t        *region;            /* back ptr to affected region */
    rvm_offset_t    end_offset;         /* end byte of range */
    nv_range_t      nv;                 /* nv range record header for i/o */
    }
range_t;
/* transaction id descriptor: int_tid_t */
typedef struct
    {
    list_entry_t    links;              /* list links and struct id; protected
                                           by log tid_list_lock */
    rw_lock_t       tid_lock;           /* remaining fields protected by
                                           tid_lock until on flush list*/
    struct timeval  uname;              /* unique identifier */
    struct timeval  commit_stamp;       /* timestamp of commit */
    log_t           *log;               /* back link to log descriptor */
    rvm_offset_t    log_size;           /* log space required */
    tree_root_t     range_tree;         /* range tree root */
    range_t         **x_ranges;         /* vector of overlaping ranges */
    long            x_ranges_alloc;     /* allocated length of x_ranges */
    long            x_ranges_len;       /* current length of x_ranges */
    long            range_elim;         /* ranges eliminated by range coalesce */
    long            trans_elim;         /* ranges eliminated by trans coalesce */
    rvm_offset_t    range_overlap;      /* overlap eliminated by range coalesce */
    rvm_offset_t    trans_overlap;      /* overlap eliminated by trans coalesce */
    rvm_length_t    n_coalesced;        /* count of coalesced transactions */
    range_t         split_range;        /* extra range for flush */
    rvm_length_t    flags;              /* mode and optimization flags */
    rvm_length_t    back_link;          /* displacement to previous header */
    }
int_tid_t;

/* definitions for tid flags field (also used in trans_hdr flags field */
#define RESTORE_FLAG        (2*RVM_COALESCE_TRANS)
#define FLUSH_FLAG          (2*RESTORE_FLAG)
#define FIRST_ENTRY_FLAG    (2*FLUSH_FLAG)
#define LAST_ENTRY_FLAG     (2*FIRST_ENTRY_FLAG)
#define FLUSH_MARK          (2*LAST_ENTRY_FLAG)

#define TID(x)              ((tid->flags & (x)) != 0)
#define TRANS_HDR(x)        ((trans_hdr->flags & (x)) != 0)
/* functions and structures for managing list of RVM-allocated
     regions of memory (added by tilt, Nov 19 1996) */
#if ! defined(MACH)
typedef struct rvm_page_entry {
    char                   *start;
    char                   *end;
    struct rvm_page_entry  *prev;
    struct rvm_page_entry  *next;
} rvm_page_entry_t;

rvm_bool_t rvm_register_page(char *vmaddr, rvm_length_t length);
rvm_bool_t rvm_unregister_page(char *vmaddr, rvm_length_t length);
rvm_bool_t mem_chk(char *vmaddr, rvm_length_t length);
rvm_page_entry_t *find_page_entry(char *vmaddr);
#endif /* __linux__ || __BSD44__ || __CYGWIN32__ */
/* list management functions */

extern
void init_list_header();                /* [rvm_utils.c] */
/*  list_entry_t    *whichlist;
    struct_id_t     struct_id;
*/
extern
list_entry_t *move_list_entry();        /* [rvm_utils.c] */
/*  register list_entry_t *fromptr;
    register list_entry_t *toptr;
    register list_entry_t *cell;
*/
extern
void insert_list_entry();               /* [rvm_utils.c] */
/*  register list_entry_t *entry;
    register list_entry_t *new_entry;
*/
extern
list_entry_t *alloc_list_entry();        /* [rvm_utils.c] */
/*  struct_id_t     id; */

/* internal type allocators/deallocators */

extern
void clear_free_list();                 /* [rvm_utils.c] */
/*  struct_id_t     id; */

extern
region_t *make_region();                /* [rvm_utils.c] */

extern
void free_region();                     /* [rvm_utils.c] */
/*  region_t        *region; */

extern
seg_t *make_seg();                      /* [rvm_utils.c] */
/*  char            *seg_dev_name;
    rvm_return_t    *retval
*/
extern
void free_seg();                        /* [rvm_utils.c] */
/*  seg_t           *seg; */

extern
void free_seg_dict_vec();               /* [rvm_utils.c] */
/*  log_t           *log; */

extern
log_t *make_log();                      /* [rvm_utils.c] */
/*  char            *dev_name;
    rvm_return_t    *retval
*/
extern
void free_log();                        /* [rvm_utils.c] */
/*  log_t           *log; */

extern
char *make_full_name();                /* [rvm_utils.c] */
/*  char            *dev_str;
    char            *dev_name;
    rvm_return_t    *retval;
*/
extern
void free_log();                        /* [rvm_utils.c] */
/*  log_t           *log; */

extern
log_special_t *make_log_special();      /* [rvm_utils.c] */
/*  struct_id_t     special_id;
    rvm_length_t    length;
*/
extern
void free_log_special();                /* [rvm_utils.c] */
/*  log_special_t   *special; */
extern
rvm_return_t dev_init();                /* [rvm_utils.c] */
/*  device_t        *dev;
    char            *dev_str;
*/
extern
range_t *make_range();                  /* [rvm_utils.c] */

extern
void free_range();                      /* [rvm_utils.c] */
/*  range_t         *range; */

extern
int_tid_t *make_tid();                  /* [rvm_utils.c] */
/*  rvm_mode_t      mode; */

extern
void free_tid();                        /* [rvm_utils.c] */
/*  register int_tid_t  *tid; */

extern
int_tid_t *get_tid();                   /* [rvm_trans.c] */
/*  rvm_tid_t *rvm_tid; */

extern
mem_region_t *make_mem_region();        /* [rvm_utils.c] */

extern
void free_mem_region();                 /* [rvm_utils.c] */
/*  mem_region_t   *node; */

extern
dev_region_t *make_dev_region();        /* [rvm_utils.c] */

extern
void free_dev_region();                 /* [rvm_utils.c] */
/*  dev_region_t   *node; */
/* log management functions */

extern
void init_log_list();                   /* [rvm_logstatus.c] */

extern
void enter_log();                       /* [rvm_logstatus.c] */
/*  log_t           *log; */

extern
log_t *find_log();                      /* [rvm_logstatus.c] */
/*  char            *log_dev; */

extern
rvm_return_t open_log();                /* [rvm_logstatus.c] */
/*  char            *dev_name;
    log_t           **log_ptr;
    char            *status_buf;
    rvm_options_t   *rvm_options;
*/
extern
rvm_return_t create_log();              /* [rvm_logstatus.c] */
/*  log_t           **log_ptr;
    rvm_options_t   *rvm_options;
*/
extern
rvm_return_t do_log_options();          /* [rvm_logstatus.c] */
/*  log_t           *log;
    rvm_options_t   *rvm_options;
*/
extern
rvm_return_t close_log();               /* [rvm_logstatus.c] */
/*  log_t           *log; */

extern
rvm_return_t close_all_logs();          /* [rvm_logstatus.c] */

extern
void copy_log_stats();                  /* [rvm_logstatus.c] */
/*  log_t           *log; */

extern
void clear_log_status();                /* [rvm_logstatus.c] */
/*  log_t           *log; */

extern
rvm_return_t init_log_status();         /* [rvm_logstatus.c] */
/*  log_t           *log; */
extern
rvm_return_t read_log_status();         /* [rvm_logstatus.c] */
/*  log_t           *log;
    char            *status_buf;
*/
extern
rvm_return_t write_log_status();        /* [rvm_logstatus.c] */
/*  log_t           *log;
    device_t        *dev;
*/
extern
rvm_return_t update_log_tail();         /* [rvm_logstatus.c] */
/*  log_t           *log;
    rec_hdr_t       *rec_hdr;
*/
extern
void log_tail_length();                 /* [rvm_logstatus.c] */
/*  log_t           *log;
    rvm_offset_t    *tail_length;
*/
extern
void log_tail_sngl_w();                 /* [rvm_logstatus.c] */
/*  log_t           *log;
    rvm_offset_t    *tail_length;
*/
extern
long cur_log_percent();                 /* [rvm_logstatus.c] */
/*  log_t           *log;
    rvm_offset_t    *space_nneded;
*/
extern
void cur_log_length();                  /* [rvm_logstatus.c] */
/*  log_t           *log;
    rvm_offset_t    *length;
*/
extern
rvm_return_t queue_special();           /* [rvm_logflush.c] */
/*  log_t           *log;
    log_special_t   *special;
*/
extern
rvm_return_t flush_log_special();       /* [rvm_logflush.c] */
/*  log_t           *log; */

extern
rvm_return_t flush_log();               /* [rvm_logflush.c] */
/*  log_t           *log;
    long            *count;
*/
extern
rvm_return_t locate_tail();             /* [rvm_logrecovr.c] */
/*  log_t           *log; */

extern
rvm_return_t init_buffer();             /* [rvm_logrecovr.c] */
/*  log_t           *log;
    rvm_offset_t    *offset;
    rvm_bool_t      direction;
    rvm_bool_t      synch;
*/
extern
void clear_aux_buf();                   /* [rvm_logrecovr.c] */
/*  log_t           *log; */

extern
rvm_return_t load_aux_buf();            /* [rvm_logrecovr.c] */
/*  log_t           *log;
    rvm_offset_t    *offset;
    rvm_length_t    length;
    rvm_length_t    *aux_ptr;
    rvm_length_t    *data_len;
    rvm_bool_t      direction;
    rvm_bool_t      synch;
*/
extern
void reset_hdr_chks();                  /* [rvm_logrecovr.c] */
/*  log_t           *log; */

extern
rvm_bool_t chk_hdr_type();              /* [rvm_logrecovr.c] */
/*  rec_hdr_t       *rec_hdr; */

extern
rvm_bool_t chk_hdr_currency();          /* [rvm_logrecovr.c] */
/*  log_t           *log;
    rec_hdr_t       *rec_hdr;
*/
extern
rvm_bool_t chk_hdr_sequence();          /* [rvm_logrecovr.c] */
/*  log_t           *log;
    rec_hdr_t       *rec_hdr;
    rvm_bool_t      direction;
*/
extern
rvm_bool_t chk_hdr();                   /* [rvm_logrecovr.c] */
/*  log_t           *log;
    rec_hdr_t       *rec_hdr;
    rec_end_t       *rec_end;
    rvm_bool_t      direction;
*/
extern
rvm_bool_t validate_hdr();              /* [rvm_logrecovr.c] */
/*  log_t           *log;
    rec_hdr_t       *rec_hdr;
    rec_end_t       *rec_end;
    rvm_bool_t      direction;
*/
extern
rvm_return_t validate_rec_forward();    /* [rvm_logrecovr.c] */
/*  log_t           *log;
    rvm_bool_t      synch;
*/
extern
rvm_return_t validate_rec_reverse();    /* [rvm_logrecovr.c] */
/*  log_t           *log;
    rvm_bool_t      synch;
*/
extern
rvm_return_t scan_forward();            /* [rvm_logrecovr.c] */
/*  log_t           *log;
    rvm_bool_t      synch;
*/
extern
rvm_return_t scan_reverse();            /* [rvm_logrecovr.c] */
/*  log_t           *log;
    rvm_bool_t      synch;
*/
extern
rvm_return_t scan_nv_forward();         /* [rvm_logrecovr.c] */
/*  log_t           *log;
    rvm_bool_t      synch;
*/
extern
rvm_return_t scan_nv_reverse();         /* [rvm_logrecovr.c] */
/*  log_t           *log;
    rvm_bool_t      synch;
*/
extern
rvm_return_t scan_wrap_reverse();       /* [rvm_logrecovr.c] */
/*  log_t           *log;
    rvm_bool_t      synch;
*/
extern
rvm_bool_t initiate_truncation();       /* [rvm_logrecovr.c] */
/*  log_t           *log;
    long            threshold;
*/
extern
rvm_return_t wait_for_truncation();     /* [rvm_logrecovr.c] */
/*  log_t           *log;
    struct timeval  *time_stamp;
*/
extern
rvm_return_t log_recover();             /* [rvm_logrecovr.c] */
/*  log_t           *log;
    long            *count;
    rvm_bool_t      is_daemon;
    rvm_length_t    flag;
*/
extern
void log_daemon();                      /* [rvm_logrecovr.c] */
/*  log_t           *log; */

extern
rvm_return_t change_tree_insert();      /* [rvm_logrecovr.c] */
/*  seg_dict_t      *seg_dict;
    dev_region_t    *node;
*/
extern
rvm_return_t apply_mods();              /* [rvm_logrecovr.c] */
/*  log_t           *log; */

extern
rvm_return_t alloc_log_buf();           /* [rvm_logrecovr.c] */
/*  log_t           *log; */

extern
void free_log_buf();                    /* [rvm_logrecovr.c] */
/*  log_t           *log; */
/* Segment & region management functions */

extern
seg_t *seg_lookup();                    /* [rvm_map.c] */
/*  char            *dev_name;
    rvm_return_t    *retval;
*/
extern
long open_seg_dev();                    /* [rvm_map.c] */
/*  seg_t           *seg;
    rvm_offset_t    *dev_length;
*/
extern
long close_seg_dev();                   /* [rvm_map.c] */
/*  seg_t           *seg; */

extern
rvm_return_t close_all_segs();          /* [rvm_map.c] */

extern
rvm_return_t define_seg();              /* [rvm_map.c] */
/*  log_t           *log;
    seg_t           *seg;
*/
extern
rvm_return_t define_all_segs();         /* [rvm_map.c] */
/*  log_t           *log; */

extern
region_t *find_whole_range();           /* [rvm_map.c] */
/*  char            *dest;
    rvm_length_t    length;
    rw_lock_mode    mode;
*/
extern
region_t *find_partial_range();         /* [rvm_map.c] */
/*  char            *dest;
    rvm_length_t    length;
    long            *code;
*/
extern                                  /* [rvm_map.c] */
long mem_partial_include();
/*  tree_node_t     *tnode1;
    tree_node_t     *tnode2;
*/
extern                                  /* [rvm_map.c] */
long mem_total_include();
/*  tree_node_t     *tnode1;
    tree_node_t     *tnode2;
*/
extern                                /* [rvm_map.c] */
long dev_partial_include();
/*  rvm_offset_t    *base1,*end1;
    rvm_offset_t    *base2,*end2;
*/
extern                                  /* [rvm_map.c] */
long dev_total_include();
/*  rvm_offset_t    *base1,*end1;
    rvm_offset_t    *base2,*end2;
*/
extern
char *page_alloc();                     /* [rvm_map.c] */
/*  rvm_length_t   len; */
extern
void page_free();                       /* [rvm_map.c] */
/*  char            *base;
    rvm_length_t    length;
*/

/* segment dictionary functions */
extern
rvm_return_t enter_seg_dict();          /* [rvm_logrecovr.c] */
/*  log_t           *log;
    long            seg_code;
*/
extern
rvm_return_t def_seg_dict();            /* [rvm_logrecovr.c] */
/*  log_t           *log;
    rec_hdr_        *rec_hdr_t;
*/
/* I/O functions */

extern
long open_dev();                        /* [rvm_io.c] */
/*  device_t        *dev;
    long            flags;
    long            mode;
*/
extern
long close_dev();                       /* [rvm_io.c] */
/*  device_t        *dev; */

extern
long read_dev();                        /* [rvm_io.c] */
/*  device_t        *dev;
    rvm_offset_t    *offset;
    char            *dest;
    rvm_length_t    length;
*/
extern
long write_dev();                       /* [rvm_io.c] */
/*  device_t        *dev;
    rvm_offset_t    *offset;
    char            *src;
    rvm_length_t    length;
    rvm_bool_t      no_sync;
*/
extern
long sync_dev();                        /* [rvm_io.c] */
/*  device_t        *dev; */

extern
long gather_write_dev();                /* [rvm_io.c] */
/*  device_t        *dev;
    rvm_offset_t    *offset;
    struct iovec    *iov;
    rvm_length_t    iovcnt;
*/    

/* length is optional [rvm_io.c] */
extern long set_dev_char(device_t *dev,rvm_offset_t *dev_length);

/* read/write lock */
extern                                  /* [rvm_utils.c] */
void rw_lock();                     
/*  rw_lock_t       *rwl;
    rw_lock_mode_t  mode;
*/
extern                                  /* [rvm_utils.c] */
void rw_unlock();                     
/*  rw_lock_t       *rwl;
    rw_lock_mode_t  mode;
*/
extern                                  /* [rvm_utils.c] */
void init_rw_lock();                     
/*  rw_lock_t       *rwl; */

extern
void init_tree_root();                  /* [rvm_utils.c] */
/*  tree_root_t     *root; */

extern
void clear_tree_root();                 /* [rvm_utils.c] */
/*  tree_root_t     *root; */

extern                                  /* [rvm_utils.c] */
void rw_lock_clear();                     
/*  rw_lock_t       *rwl; */
/* Binary Tree Functions */

extern
tree_node_t *tree_lookup();             /* [rvm_utils.c] */
/*  tree_root_t     *tree;
    tree_node_t     *node;
    cmp_func_t      *cmp;
*/
extern
rvm_bool_t tree_insert();               /* [rvm_utils.c] */
/*  tree_root_t     *tree;
    tree_node_t     *node;
    cmp_func_t      *cmp;
*/
extern
rvm_bool_t tree_delete();               /* [rvm_utils.c] */
/*  tree_root_t     *tree;
    tree_node_t     *node;
    cmp_func_t      *cmp;
*/
extern
tree_node_t *init_tree_generator();     /* [rvm_utils.c] */
/*  tree_root_t     *tree;
    rvm_bool_t      direction;
    rvm_bool_t      unlink;
*/
extern
tree_node_t *tree_iterate_insert();     /* [rvm_utils.c] */
/*  tree_root_t     *tree;
    tree_node_t     *node;
    cmp_func_t      *cmp;
*/
extern
tree_node_t *tree_successor();          /* [rvm_utils.c] */
/*  tree_root_t     *tree;
    rvm_bool_t      direction;
*/
extern
tree_node_t *tree_predecessor();        /* [rvm_utils.c] */
/*  tree_root_t     *tree;
    rvm_bool_t      direction;
*/
/* initialization, query, and structure checkers */

extern rvm_bool_t bad_init();                  /* [rvm_init.c] */

extern
rvm_return_t bad_options();             /* [rvm_status.c] */
/*  rvm_options_t   *rvm_options;
    rvm_bool_t            chk_log_dev;
*/
extern
rvm_return_t bad_statistics();          /* [rvm_status.c] */
/*  rvm_statistics_t   *rvm_statistics; */

extern
rvm_return_t bad_region();              /* [rvm_map.c] */
/*   rvm_region_t   *rvm_region; */

extern
rvm_return_t bad_tid();                 /* [rvm_trans.c] */
/*   rvm_tid_t      *rvm_tid; */

extern
rvm_return_t do_rvm_options();          /* [rvm_status.c] */
/*  rvm_options_t   *rvm_options; */             
/* make unique name */
extern                                  /* [rvm_utils.c] */
void make_uname();
/*  struct timeval  *time; */
extern                                  /* [rvm_utils.c] */
long init_unames();

/* time value arithmetic */
extern
struct timeval add_times();             /* [rvm_utils.c] */
/*  struct timeval  *x;
    struct timeval  *y;
*/
extern
struct timeval sub_times();             /* [rvm_utils.c] */
/*  struct timeval  *x;
    struct timeval  *y;
*/
extern
long round_time();                      /* [rvm_utils.c] */
/*  struct timeval  *x; */

/* statistics gathering functions */
extern
void enter_histogram();                 /* [rvm_utils] */
/*  long            val;
    long            *histo;
    long            *histo_def;
    long            length;    
*/

/* various initializers */
extern
void init_map_roots();                  /* [rvm_map.c] */

extern
long init_utils();                      /* [rvm_utils.c] */

/* check summing and byte-aligned copy and pad functions */
extern
rvm_length_t chk_sum();                 /* rvm_utils.c */
/*  char            *nvaddr;
    rvm_length_t    len;
*/
extern
rvm_length_t zero_pad_word();           /* rvm_utils.c */
/*  rvm_length_t    word;
    char            *addr;
    rvm_bool_t      leading;
*/
extern
void src_aligned_bcopy();               /* rvm_utils.c */
/*  char            *src;
    char            *dest;
    rvm_length_t    len;
*/
extern
void dest_aligned_bcopy();               /* rvm_utils.c */
/*  char            *src;
    char            *dest;
    rvm_length_t    len;
*/

/*  offset arithmetic */
extern
rvm_offset_t rvm_rnd_offset_to_sector(); /* [rvm_utils.c] */
/*  rvm_offset_t    *x; */

/* debug support */
extern
void rvm_debug();                       /* [rvm_debug] */
/*  rvm_length_t    val; */


#endif _RVM_PRIVATE_
