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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/rvm-src/rvm/Attic/rvm.h,v 4.4 1998/08/26 15:40:13 braam Exp $";
#endif _BLURB_

/*
*                       Definitions for RVM
*
*
*/

/*LINTLIBRARY*/

/* permit multiple includes */
#ifndef RVM_VERSION

/* Version string for initialization */
#define RVM_VERSION         "RVM Interface Version 1.3  7 Mar 1994"
#define RVM_VERSION_MAX     128         /* 128 char maximum version str length */

/* be sure parallel libraries are used */
#ifndef _PARALLEL_LIBRARIES
#define _PARALLEL_LIBRARIES 1
#endif

/* get timestamp structures and system constants */
#include <sys/time.h>
#include <sys/param.h>

#ifndef C_ARGS
#if defined(c_plusplus) || defined(__STDC__)
#define C_ARGS(arglist) arglist
#else   /* c_plusplus || __STDC__ */
#define C_ARGS(arglist) ()
#endif  /* c_plusplus || __STDC__ */
#endif  /* C_ARGS */

/* define bool, TRUE, and FALSE if libc.h hasn't been included
   these definitions will not be repeated in libc.h if it is
   included later.
*/

#ifndef TRUE
#define TRUE 1
#endif  TRUE

#ifndef FALSE
#define FALSE 0
#endif  FALSE

/* RVM's use of false, true and bool causes trouble with versions of gcc
   above 2.6 or so; because of this, the names of RVM's definitions
   have been changed to rvm_{false,true,bool_t}. (Originally changed
   in rvm.h by Satya (7/31/96); propogated to the rest of the RVM code
   8/23/96 by tilt */

typedef enum { rvm_false = 0, rvm_true = 1 } rvm_bool_t;

/*  structure identifiers: rvm_struct_id_t
    codes placed in the first field of each
    structure instance to identify the object.
*/
typedef enum 
    {
    rvm_first_struct_id = 39,           /* internal use only */

    rvm_region_id,                      /* identifier for rvm_region's */
    rvm_options_id,                     /* identifier for rvm_options */
    rvm_tid_id,                         /* identifier for rvm_tid's */
    rvm_statistics_id,                  /* identifier for rvm_statistics rec's */
    rvm_last_struct_id                  /* internal use only */
    } 
rvm_struct_id_t;

/*  Transaction mode codes: rvm_mode_t */
typedef enum
    {
    rvm_first_mode = 139,               /* internal use only */

    restore,                            /* restore memory on abort */
    no_restore,                         /* do not restore memory on abort */
    flush,                              /* flush records to logdev on commit */
    no_flush,                           /* do not flush records on commit */

    rvm_last_mode                       /* internal use only */
    }
rvm_mode_t;

/*  Function return codes:  rvm_return_t */
typedef enum {
    RVM_SUCCESS = 0,                    /* success return code */

    rvm_first_code = 199,               /* internal use only */

    RVM_EINIT,                          /* RVM not initialized */
    RVM_EINTERNAL,                      /* internal error, see rvm_errmsg */
    RVM_EIO,                            /* I/O error, see errno */
    RVM_ELOG,                           /* invalid log device */
    RVM_ELOG_VERSION_SKEW,              /* RVM log format version skew */
    RVM_EMODE,                          /* invalid transaction begin/end mode */
    RVM_ENAME_TOO_LONG,                 /* device name longer than 1023 chars */
    RVM_ENO_MEMORY,                     /* heap exhausted */
    RVM_ENOT_MAPPED,                    /* designated region not mapped */
    RVM_EOFFSET,                        /* invalid segment offset */
    RVM_EOPTIONS,                       /* invalid options record or pointer */
    RVM_EOVERLAP,                       /* region overlaps existing seg mapping */
    RVM_EPAGER,                         /* invalid external pager */
    RVM_ERANGE,                         /* invalid virtual memory address */
    RVM_EREGION,                        /* invalid region descriptor or pointer */
    RVM_EREGION_DEF,                    /* invalid region definition descriptor */
    RVM_ESRC,                           /* invalid address range for new
                                           values */
    RVM_ESTATISTICS,                    /* invalid statistics record */
    RVM_ESTAT_VERSION_SKEW,             /* RVM statistics format version skew */
    RVM_ETERMINATED,                    /* terminated by error already reported */
    RVM_ETHREADS,                       /* illegal C Thread library */
    RVM_ETID,                           /* invalid transaction identifier or ptr */
    RVM_ETOO_BIG,                       /* internal resouces exceeded */
    RVM_EUNCOMMIT,                      /* uncommitted transaction(s) pending */
    RVM_EVERSION_SKEW,                  /* RVM library version skew */
    RVM_EVM_OVERLAP,                    /* region overlaps existing vm mapping */

    rvm_last_code                       /* internal use only */
    }
rvm_return_t;

/* Enumeration type print name functions */
extern char *rvm_return C_ARGS
    ((
    rvm_return_t        code            /* return code */
    ));

extern char *rvm_mode C_ARGS
    ((
    rvm_mode_t          mode            /* transaction mode */
    ));

extern char *rvm_type C_ARGS
    ((
    rvm_struct_id_t     id              /* structure identifier */
    ));

/*  RVM basic length and offset types:
    these types are used throughout RVM to hide machine-dependent
    representations of maximum virtual memory region lengths and
    64 bit offsets.  Do not use internal types or fields or
    portability can be compromised
*/
/*  region length: rvm_length_t
    size must be >= sizeof(char *),
    type must be unsigned arithmetic */

typedef unsigned long rvm_length_t;

/*  region offset descriptor: rvm_offset_t supports 64 bit unsigned integers
    struct unecessary if machine has 64-bit ops */

typedef struct                                           
    {                                   /* INTERNAL FIELDS static */
    rvm_length_t    high;               /* private */
    rvm_length_t    low;                /* private */
    }
rvm_offset_t;

/* construct offset from two rvm_length_t sized quantities x,y
   -- this will construct an offset from two lengths even if
   8*sizeof(rvm_length_t) is > sizeof(rvm_offset_t); the "extra"
   bits, the highest order bits of parameter y, will be discarded */
#define RVM_MK_OFFSET(x,y)      rvm_mk_offset((rvm_length_t)(x), \
                                              (rvm_length_t)(y))

/* offset initializer -- same as RVM_MK_OFFSET, but compile time */
#define RVM_OFFSET_INITIALIZER(x,y)     {(x),(y)}

/* Zero an offset: create a zero offset and assign it to the parameter. */
#define RVM_ZERO_OFFSET(x)      (x) = RVM_MK_OFFSET(0,0)

/* offset and length conversion macros */

/* return low-order bits of offset x as length 
   -- "low-order bits" are the lowest numerically valued bits
   of the same size as rvm_length_t */
#define RVM_OFFSET_TO_LENGTH(x) ((x).low)

/* return high order bits of offset x as length
   -- "high-order bits" are defined as the highest ordered
   bits remaining after the low-order bits are extracted
   they are returned as rvm_length_t */
#define RVM_OFFSET_HIGH_BITS_TO_LENGTH(x) ((x).high)

/* return length x as rvm_offset_t */
#define RVM_LENGTH_TO_OFFSET(x) RVM_MK_OFFSET(0,(x))

/* rvm_offset_t and rvm_length_t arithmetic support */

/* add rvm_offset to rvm_offset; returns result (x+y)
   implemented as function call -- or simple add if 
   machine has 64-bit integer operations */
#define RVM_ADD_OFFSETS(x,y) \
    rvm_add_offsets(&(x),&(y))

/* add rvm_length to rvm_offset; return result (length+offset)
   as rvm_offset_t
   implemented as function call -- or simple add if
   machine has 64-bit integer operations */
#define RVM_ADD_LENGTH_TO_OFFSET(x,y) \
    rvm_add_length_to_offset(&(x),(y))

/* add rvm_length_t to vm address; returns address (char *)
   always implemented as simple add */
#define RVM_ADD_LENGTH_TO_ADDR(length,vmaddr) \
    ((char *)((rvm_length_t)(length)+(rvm_length_t)(vmaddr)))

/* subtract rvm_offset from rvm_offset; return result (x-y) as rvm_offset_t
   implemented as function call -- or simple subtract if
   machine has 64-bit integer operations */
#define RVM_SUB_OFFSETS(x,y) \
    rvm_sub_offsets(&(x),&(y))

/* subtract rvm_length from rvm_offset; return result (offset-length)
   as rvm_offset_t
   implemented as function call or simple subtract if
   machine has 64-bit integer operations  */
#define RVM_SUB_LENGTH_FROM_OFFSET(x,y) \
    rvm_sub_length_from_offset(&(x),(y))

/* subtract rvm_length_t from vm address; returns address (char *)
   always implemented as simple subtract */
#define RVM_SUB_LENGTH_FROM_ADDR(vmaddr,length) \
    ((char *)((rvm_length_t)(vmaddr)-(rvm_length_t)(length)))

/*  rvm_offset_t comparison macros */
#define RVM_OFFSET_LSS(x,y)     (((x).high < (y).high) || \
                                 ((((x).high == (y).high) && \
                                 ((x).low < (y).low))))
#define RVM_OFFSET_GTR(x,y)     (((x).high > (y).high) || \
                                 ((((x).high == (y).high) && \
                                 ((x).low > (y).low))))
#define RVM_OFFSET_LEQ(x,y)     (!RVM_OFFSET_GTR((x),(y)))
#define RVM_OFFSET_GEQ(x,y)     (!RVM_OFFSET_LSS((x),(y)))
#define RVM_OFFSET_EQL(x,y)     (((x).high == (y).high) && \
                                 ((x).low == (y).low))
#define RVM_OFFSET_EQL_ZERO(x)  (((x).high == 0) && ((x).low == 0))

/* page-size rounding macros */

/* return page size as rvm_length_t */
#define RVM_PAGE_SIZE           rvm_page_size()

/* return rvm_length x rounded up to next integral page-size length */
#define RVM_ROUND_LENGTH_UP_TO_PAGE_SIZE(x)  ((rvm_length_t)( \
    ((rvm_length_t)(x)+rvm_page_size()-1) & rvm_page_mask()))

/* return rvm_length x rounded down to integral page-size length */
#define RVM_ROUND_LENGTH_DOWN_TO_PAGE_SIZE(x)  ((rvm_length_t)( \
     (rvm_length_t)(x) & rvm_page_mask()))

/* return address x rounded up to next page boundary */
#define RVM_ROUND_ADDR_UP_TO_PAGE_SIZE(x)  ((char *)( \
    ((rvm_length_t)(x)+rvm_page_size()-1) & rvm_page_mask()))             

/* return address x rounded down to page boundary */
#define RVM_ROUND_ADDR_DOWN_TO_PAGE_SIZE(x)  ((char *)( \
    (rvm_length_t)(x) & rvm_page_mask()))

/* return rvm_offset x rounded up to next integral page-size offset */
#define RVM_ROUND_OFFSET_UP_TO_PAGE_SIZE(x)  \
    rvm_rnd_offset_up_to_page(&(x))

/* return rvm_offset x rounded down to integral page-size offset */
#define RVM_ROUND_OFFSET_DOWN_TO_PAGE_SIZE(x)  \
    rvm_rnd_offset_dn_to_page(&(x))

/* transaction identifier descriptor */
typedef struct
    {
    rvm_struct_id_t struct_id;          /* self-identifier, do not change */
    rvm_bool_t      from_heap;          /* true if heap allocated;
                                           do not change */
    struct timeval  uname;              /* unique name (timestamp) */

    void            *tid;               /* internal use only */
    rvm_length_t    reserved;           /* internal use only */
    }
rvm_tid_t;

/* rvm_tid_t initializer, copier & finalizer */

extern rvm_tid_t    *rvm_malloc_tid C_ARGS(());

extern void rvm_init_tid C_ARGS
    ((
    rvm_tid_t       *tid                /* pointer to record to initialize */
    ));
extern rvm_tid_t *rvm_copy_tid C_ARGS
    ((
    rvm_tid_t       *tid                /* pointer to record to be copied */
    ));
extern void rvm_free_tid C_ARGS
    ((
    rvm_tid_t       *tid                /* pointer to record to be copied */
    ));

/*  options descriptor:  rvm_options_t */
typedef struct
    {
    rvm_struct_id_t struct_id;          /* self-identifier, do not change */
    rvm_bool_t      from_heap;          /* true if heap allocated;
                                           do not change */

    char            *log_dev;           /* device name */
    long            truncate;           /* truncation threshold, % of log */
    rvm_length_t    recovery_buf_len;   /* length of recovery buffer */
    rvm_length_t    flush_buf_len;      /* length of flush buffer (partitions only) */
    rvm_length_t    max_read_len;       /* maximum single read length (MACH
                                           only)  */
    rvm_bool_t      log_empty;          /* TRUE  ==> log empty */
    char            *pager;             /* char array for external pager name */
    long            n_uncommit;         /* length of uncommitted tid array */
    rvm_tid_t       *tid_array;         /* ptr to array of uncommitted tid's */

    rvm_length_t    flags;              /* bit vector for optimization and
                                           other flags */
    }
rvm_options_t;

/* rvm_options default values and other constants */

#define TRUNCATE            50          /* 50% default truncation threshold */
#define RECOVERY_BUF_LEN    (256*1024)  /* default recovery buffer length */
#define MIN_RECOVERY_BUF_LEN (64*1024)  /* minimum recovery buffer length */
#define FLUSH_BUF_LEN       (256*1024)  /* default flush buffer length */
#define MIN_FLUSH_BUF_LEN    (64*1024)  /* minimum flush buffer length */
#define MAX_READ_LEN        (512*1024)  /* default maximum single read length */

#define RVM_COALESCE_RANGES 1           /* coalesce adjacent or shadowed
                                           ranges within a transaction */
#define RVM_COALESCE_TRANS  2           /* coalesce adjacent or shadowed ranges
                                           within no_flush transactions */

#define RVM_ALL_OPTIMIZATIONS   (RVM_COALESCE_RANGES | RVM_COALESCE_TRANS)

/* rvm_options_t initializer, copier & finalizer */

extern rvm_options_t *rvm_malloc_options C_ARGS(());

extern void rvm_init_options C_ARGS
    ((
    rvm_options_t   *options            /* pointer to record to initialize */
    ));
extern rvm_options_t *rvm_copy_options C_ARGS
    ((
    rvm_options_t   *options            /* pointer to record to be copied */
    ));
extern void rvm_free_options C_ARGS
    ((
    rvm_options_t   *options            /* pointer to record to be freed */
    ));

/*  region descriptor: rvm_region_t */
typedef struct
    {
    rvm_struct_id_t struct_id;          /* self-identifier, do not change */
    rvm_bool_t      from_heap;          /* true if heap allocated;
                                           do not change */

    char            *data_dev;          /* device name */
    rvm_offset_t    dev_length;         /* maximum device length */
    rvm_offset_t    offset;             /* offset of region in segment */
    char            *vmaddr;            /* vm address of region/range */
    rvm_length_t    length;             /* length of region/range */
    rvm_bool_t      no_copy;            /* do not copy mapped data if true */
    }
rvm_region_t;

/* rvm_region_t allocator, initializer, copier & finalizer */
extern rvm_region_t *rvm_malloc_region C_ARGS(());

extern void rvm_init_region C_ARGS
    ((
    rvm_region_t    *region             /* pointer to record to initialize */
    ));

/* note: copier copies pointers to the char arrays */
extern rvm_region_t *rvm_copy_region C_ARGS
    ((
    rvm_region_t    *region             /* pointer to record to be copied */
    ));

extern void rvm_free_region C_ARGS
    ((
    rvm_region_t *region                /* pointer to record to be freed */
    ));

/*
        Main Function Declarations
*/

/* RVM initialization: pass version and optional options
   descriptor */
extern rvm_return_t rvm_initialize(char *version, rvm_options_t *opts);
/* init macro */
#define RVM_INIT(options) rvm_initialize(RVM_VERSION,(options))

/* shutdown RVM */
extern rvm_return_t rvm_terminate (void);   /* no parameters */

/* map recoverable storage */
extern rvm_return_t rvm_map C_ARGS
    ((
    rvm_region_t         *region,       /* pointer to region descriptor */
    rvm_options_t        *options       /* optional ptr to option descriptor */
    ));

/* unmap recoverable storage */
extern rvm_return_t rvm_unmap C_ARGS
    ((
    rvm_region_t        *region         /* pointer to region descriptor */
    ));

/* set RVM options */
extern rvm_return_t rvm_set_options C_ARGS
    ((
    rvm_options_t       *options        /* pointer to option descriptor */
    ));

/* query RVM options */
extern rvm_return_t rvm_query C_ARGS
    ((
    rvm_options_t       *options,       /* address of pointer to option
                                           descriptor [out] */
    rvm_region_t        *region         /* optional pointer to region descriptor */
    ));

/* begin a transaction */
extern rvm_return_t rvm_begin_transaction C_ARGS
    ((
    rvm_tid_t           *tid,           /* pointer to transaction identifier */
    rvm_mode_t          mode            /* transaction begin mode */
    ));

/* declare a modification region for a transaction */
extern rvm_return_t rvm_set_range C_ARGS
    ((
    rvm_tid_t           *tid,           /* pointer to transaction identifier */
    void                *dest,          /* base address of modification range */
    rvm_length_t        length          /* length of modification range */
    ));

/* modification of a region for a transaction */
extern rvm_return_t rvm_modify_bytes C_ARGS
    ((
    rvm_tid_t           *tid,           /* pointer to transaction identifier */
    void                *dest,          /* base address of modification range */
    const void          *src,           /* base address of source range */
    rvm_length_t        length          /* length of modification range */
    ));

/* abort a transaction */
extern rvm_return_t rvm_abort_transaction C_ARGS
    ((
    rvm_tid_t           *tid            /* pointer to transaction identifier */
    ));

/* commit a transaction */
extern rvm_return_t rvm_end_transaction C_ARGS
    ((
    rvm_tid_t           *tid,           /* pointer to transaction identifier */
    rvm_mode_t          mode            /* transaction commit mode */
    ));

/* flush log cache buffer to log device */
extern rvm_return_t rvm_flush C_ARGS(( )); /* no parameters */

/* apply logged changes to segments and garbage collect the log device */
extern rvm_return_t rvm_truncate C_ARGS(( )); /* no parameters */

/* initialize log */
extern rvm_return_t rvm_create_log C_ARGS
    ((
    rvm_options_t   *rvm_options,       /* ptr to options record */
    rvm_offset_t    *log_len,           /* length of log data area */
    long            mode                /* file creation protection mode */
    ));

/* underlying support functions for length, offset, and rounding macros

   *** use outside of the macros can compromise portability ***

   these functions will not be implemented on machines with 64-bit
   integer formats since their operations will be available in the
   native instruction set
*/
extern rvm_offset_t rvm_mk_offset C_ARGS
    ((
    rvm_length_t        x,
    rvm_length_t        y
    ));
extern rvm_offset_t rvm_add_offsets C_ARGS
    ((
    rvm_offset_t        *x,
    rvm_offset_t        *y
    ));
extern rvm_offset_t rvm_add_length_to_offset C_ARGS
    ((
    rvm_offset_t        *offset,
    rvm_length_t        length
    ));
extern rvm_offset_t rvm_sub_offsets C_ARGS
    ((
    rvm_offset_t        *x,
    rvm_offset_t        *y
    ));
extern rvm_offset_t rvm_sub_length_from_offset C_ARGS
    ((
    rvm_offset_t        *offset,
    rvm_length_t        length
    ));

/* private functions to support page rounding */

extern rvm_length_t rvm_page_size C_ARGS(());
extern rvm_length_t rvm_page_mask C_ARGS(());
extern rvm_offset_t rvm_rnd_offset_up_to_page C_ARGS
    ((
    rvm_offset_t        *x
    ));
extern rvm_offset_t rvm_rnd_offset_dn_to_page C_ARGS
    ((
    rvm_offset_t        *x
    ));


#endif RVM_VERSION
