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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/rvm-src/rvm/Attic/rvm_trans.c,v 4.4 1998/11/02 16:47:49 rvb Exp $";
#endif _BLURB_

/*
*
*                   RVM Transaction & Range Functions
*
*/

#include "rvm_private.h"

/* global variables */

extern log_t        *default_log;       /* default log descriptor ptr */
extern int          errno;              /* kernel error number */
extern char         *rvm_errmsg;        /* internal error message buffer */
extern rvm_length_t rvm_optimizations;  /* optimization switches */

/* local constants, macros */

#define X_RANGES_INCR   5               /* increment for x_ranges allocation */

/* special search function signature type */
typedef rvm_bool_t bool_func_t();
/* rvm_tid checker */
rvm_return_t bad_tid(rvm_tid)
    rvm_tid_t           *rvm_tid;
    {
    if (rvm_tid == NULL)
        return RVM_ETID;

    if (rvm_tid->struct_id != rvm_tid_id)
        return RVM_ETID;

    return RVM_SUCCESS;
    }

/* tid lookup and checker */
int_tid_t *get_tid(rvm_tid)
    rvm_tid_t           *rvm_tid;
    {
    int_tid_t           *tid;

    /* sanity checks */
    if (bad_tid(rvm_tid) != RVM_SUCCESS) return NULL;
    if (rvm_tid->uname.tv_sec == 0) return NULL;
    tid = (int_tid_t *)rvm_tid->tid;
    if (tid == NULL) return NULL;
    if (tid->links.struct_id != int_tid_id) return NULL;

    /* begin tid_lock crit sec (normally ended by caller) */
    RW_CRITICAL(tid->tid_lock,w,
        {
        if (TIME_EQL(rvm_tid->uname,tid->uname)
            && TIME_EQL_ZERO(tid->commit_stamp))
           return tid;

        });

    return NULL;
    }
/* reallocate tid x_ranges vector if necessary */
static rvm_bool_t alloc_x_ranges(tid,len)
    int_tid_t       *tid;
    int             len;
    {
    if ((tid->x_ranges_len+len) > tid->x_ranges_alloc)
        {
        if (len <= X_RANGES_INCR)
            tid->x_ranges_alloc += X_RANGES_INCR;
        else
            tid->x_ranges_alloc += len;
        tid->x_ranges = (range_t **)REALLOC(tid->x_ranges,
                             tid->x_ranges_alloc*sizeof(range_t *));
        if (tid->x_ranges == NULL) return rvm_false;
        }

    return rvm_true;
    }

/* construct range descriptor and fill in region parameters */
static range_t *build_range(region,dest,length)
    region_t        *region;
    char            *dest;
    rvm_length_t    length;
    {
    range_t         *new_range;         /* ptr for new range descriptor */

    /* create range descriptor and initialize */
    if ((new_range = make_range()) == NULL) return NULL;
    new_range->region = region;
    new_range->nv.seg_code = region->seg->seg_code;

    /* set basic size fields */
    new_range->nv.length = length;
    new_range->nv.vmaddr = dest;

    /* calculate offsets */
    new_range->nv.offset = RVM_ADD_LENGTH_TO_OFFSET(region->offset,
             ((rvm_length_t)dest-(rvm_length_t)region->vmaddr));
    new_range->end_offset =
        RVM_ADD_LENGTH_TO_OFFSET(new_range->nv.offset,length);

    return new_range;
    }
/* save old values for a range */
static char *save_ov(range)
    range_t         *range;
    {

    range->data_len = RANGE_LEN(range);
    range->data = malloc(range->data_len);
    if (range->data != NULL)
        BCOPY(range->nv.vmaddr,range->data,range->nv.length);

    return range->data;
    }

/* restore old values for a transaction */
static void restore_ov(tid)
    int_tid_t       *tid;
    {
    range_t         *range;

    /* ranges must be restored in reverse order of modification
       to exactly restore memory when optimizations are not done;
       order irrelevant if optimizing since ranges will be disjoint */
    UNLINK_REVERSE_NODES_OF(tid->range_tree,range_t,range)
        {
        CODA_ASSERT(range->links.node.struct_id == range_id);

        /* restore old values if necessary */
        if (range->nv.length != 0)
            {
            if (TID(RESTORE_FLAG))
                BCOPY(range->data,range->nv.vmaddr,range->nv.length);

            /* decr uncommited mods cnt */
            CODA_ASSERT(range->region->links.struct_id == region_id);
            CRITICAL(range->region->count_lock,
                     range->region->n_uncommit--);
            }

        free_range(range);
        }
    }
/* range tree comparator function for chronological order insertion */
static long cmp_range_num(range1,range2)
    range_t         *range1;
    range_t         *range2;
    {

    /* compare range numbers */
    if (range1->nv.range_num > range2->nv.range_num)
        return 1;
    if (range1->nv.range_num < range2->nv.range_num)
        return -1;
    return 0;
    }

/* unconditionally add new range descriptor to tid's range tree
   used for non-optimized transactions only */
static rvm_return_t add_new_range(tid,new_range)
    int_tid_t       *tid;
    range_t         *new_range;         /* ptr for new range descriptor */
    {

    /* build old value record if necessary */
    CODA_ASSERT(new_range->links.node.struct_id == range_id);
    if (new_range->nv.length != 0)
        {
        if (TID(RESTORE_FLAG))
            if (save_ov(new_range) == NULL)
                {
                free_range(new_range);
                return RVM_ENO_MEMORY;
                }

        /* count uncommitted mods */
        CODA_ASSERT(new_range->region->links.struct_id == region_id);
        CRITICAL(new_range->region->count_lock,
                 new_range->region->n_uncommit++);
        }

    /* insert range to transaction's range tree in order of arrival */
    new_range->nv.range_num = tid->range_tree.n_nodes+1;
    if (!tree_insert(&tid->range_tree,(tree_node_t *)new_range,
                     cmp_range_num))
        CODA_ASSERT(rvm_false);

    return RVM_SUCCESS;
    }
/* range tree comparator for tree_insert by compound key:
   {region, segment displacement}
   adjacent nodes within region are considered equivalent */
static long region_partial_include(range1,range2)
    range_t         *range1;
    range_t         *range2;
    {

    /* compare displacements if in same region */
    if (range1->region == range2->region)
        {
        /* compare displacement within segment */
        if (RVM_OFFSET_GTR(range1->nv.offset,range2->end_offset))
            return 1;
        if (RVM_OFFSET_LSS(range1->end_offset,range2->nv.offset))
            return -1;
        return 0;
        }

    /* compare region descriptor address */
    if (range1->region > range2->region)
        return 1;
    return -1;
    }

/* range tree comparator for tree_insert by compound key:
   {segment, segment displacement}
   adjacent nodes within segment are considered equivalent */
static long segment_partial_include(range1,range2)
    range_t         *range1;
    range_t         *range2;
    {

    /* compare displacements if in same segment */
    if (range1->nv.seg_code == range2->nv.seg_code)
        {
        /* compare displacement within segment */
        if (RVM_OFFSET_GTR(range1->nv.offset,range2->end_offset))
            return 1;
        if (RVM_OFFSET_LSS(range1->end_offset,range2->nv.offset))
            return -1;
        return 0;
        }
    
    /* compare segment codes */
    if (range1->nv.seg_code > range2->nv.seg_code)
        return 1;
    return -1;
    }
/* detect and form list of overlapping or adjacent ranges */
static rvm_bool_t find_overlap(tid,new_range,cmp_func,elim,overlap,retval)
    int_tid_t       *tid;               /* transaction descriptor to search */
    range_t         *new_range;         /* descriptor for new range */
    cmp_func_t      *cmp_func;          /* tree comparator function */
    rvm_length_t    *elim;              /* number of eliminated ranges */
    rvm_offset_t    *overlap;           /* length of eliminated ranges */
    rvm_return_t    *retval;            /* error code return */
    {
    range_t         *range;             /* ptr to existing range descriptor */
    rvm_offset_t    off_tmp;            /* arithmetic temporary */

    tid->x_ranges_len = 0;
    *retval = RVM_SUCCESS;

    /* try to insert, iterate on collision */
    FROM_EXISTING_NODE_OF(tid->range_tree,range_t,range,
                         new_range,*cmp_func)
        {
        /* see if out of region/segment or new_range */
        if ((*cmp_func)(new_range,range) != 0)
            break;

        /* save overlaping/adjacent ranges */
        if (!alloc_x_ranges(tid,1))
            {
            *retval = RVM_ENO_MEMORY;
            return rvm_true;
            }
        tid->x_ranges[tid->x_ranges_len++] = range;
        (*elim)++;

        /* special case: new range completly contained in existing range */
        if ((tid->x_ranges_len == 1)
            && (RVM_OFFSET_GEQ(new_range->nv.offset,range->nv.offset)
                && RVM_OFFSET_LEQ(new_range->end_offset,
                                  range->end_offset)))
            {
            *overlap = RVM_ADD_LENGTH_TO_OFFSET(*overlap,
                                                new_range->nv.length);
            return rvm_true;
            }
        /* general case: merge all overlaping/adjacent ranges
           tally savings and save new bounds of composite range */

        /* test if non-overlaping values preceed new range */
        if (RVM_OFFSET_LSS(range->nv.offset,new_range->nv.offset))
            {
            off_tmp = RVM_SUB_OFFSETS(range->end_offset,
                                      new_range->nv.offset);
            *overlap = RVM_ADD_OFFSETS(*overlap,off_tmp);
            new_range->nv.offset = range->nv.offset;
            }
        else
            /* test if non-overlapping values follow new range */
            if (RVM_OFFSET_GTR(range->end_offset,new_range->end_offset))
                {
                off_tmp = RVM_SUB_OFFSETS(new_range->end_offset,
                                          range->nv.offset);
                *overlap = RVM_ADD_OFFSETS(*overlap,off_tmp);
                new_range->end_offset = range->end_offset;
                }
        else
            /* range completely contained in new range */
            *overlap = RVM_ADD_LENGTH_TO_OFFSET(*overlap,
                                                range->nv.length);

        /* update length of composite range */
        new_range->nv.length =
            RVM_OFFSET_TO_LENGTH(RVM_SUB_OFFSETS(new_range->end_offset,
                                                new_range->nv.offset));
        }


    return rvm_false;
    }
/* merge new range with existing range(s) */
static rvm_return_t merge_range(tid,region,new_range)
    int_tid_t       *tid;
    region_t        *region;
    range_t         *new_range;         /* ptr to new range descriptor */
    {
    range_t         *range;             /* ptr range descriptor */
    char            *vmaddr;            /* working vm address */
    char            *data_addr;         /* working ov buffer address */
    rvm_length_t    len;                /* length temp */
    int             i;
    rvm_return_t    retval;

    /* do simple insertion if not optimizing */
    if (!TID(RVM_COALESCE_RANGES))
        return add_new_range(tid,new_range);

    /* search for overlap/adjacency with existing ranges in same region */
    if (find_overlap(tid,new_range,region_partial_include,
                     &tid->range_elim,&tid->range_overlap,&retval))
        {
        /* totally optimized away or error */
        free_range(new_range);
        return retval;
        }

    /* if new range was inserted, update region ref count and exit */
    if (tid->x_ranges_len == 0)
        {
        if (TID(RESTORE_FLAG))
            if (save_ov(new_range) == NULL)
                {
                if (!tree_delete(&tid->range_tree,new_range,
                                 region_partial_include))
                    CODA_ASSERT(rvm_false);
                free_range(new_range);
                return RVM_ENO_MEMORY;
                }
        CRITICAL(region->count_lock,region->n_uncommit++);
        }
    else
        {
        /* update vmaddr and allocate new old value buffer */
        range = tid->x_ranges[0];
        if (range->nv.vmaddr < new_range->nv.vmaddr)
            new_range->nv.vmaddr = range->nv.vmaddr;
        if (TID(RESTORE_FLAG))
            {
            new_range->data_len = RANGE_LEN(new_range);
            new_range->data = malloc(new_range->data_len);
            if (range->data == NULL) return RVM_ENO_MEMORY;
            }
        /* otherwise, merge existing old values into new node and put
           its data in 1st existing node in the tree; kill any others */
        vmaddr = new_range->nv.vmaddr;
        data_addr = new_range->data;
        for (i = 0; i < tid->x_ranges_len; i++)
            {
            range = tid->x_ranges[i];
            if (TID(RESTORE_FLAG))
                {
                /* copy old values preceeding existing range to new buffer */
                if (vmaddr < range->nv.vmaddr)
                    {
                    len = (rvm_length_t)RVM_SUB_LENGTH_FROM_ADDR(
                                            range->nv.vmaddr,vmaddr);
                    BCOPY(vmaddr,data_addr,len);
                    vmaddr = RVM_ADD_LENGTH_TO_ADDR(vmaddr,len);
                    data_addr = RVM_ADD_LENGTH_TO_ADDR(data_addr,len);
                    }

                /* copy old values of existing range to new buffer */
                BCOPY(range->data,data_addr,range->nv.length);
                vmaddr =
                    RVM_ADD_LENGTH_TO_ADDR(vmaddr,range->nv.length);
                data_addr =
                    RVM_ADD_LENGTH_TO_ADDR(data_addr,range->nv.length);

                /* copy trailing old values if necessary */
                if (i == (tid->x_ranges_len-1))
                    {
                    len = (rvm_length_t)
                        RVM_ADD_LENGTH_TO_ADDR(new_range->nv.vmaddr,
                                               new_range->nv.length);
                    if ((char *)len > vmaddr)
                        {
                        len = (rvm_length_t)
                            RVM_SUB_LENGTH_FROM_ADDR(len,vmaddr);
                        BCOPY(vmaddr,data_addr,len);
                        CODA_ASSERT(RVM_ADD_LENGTH_TO_ADDR(vmaddr,len) ==
                           RVM_ADD_LENGTH_TO_ADDR(new_range->nv.vmaddr,
                                                  new_range->nv.length));
                        }
                    }
                }

            /* kill replaced nodes in tree */
            if (i != 0)
                {
                if (!tree_delete(&tid->range_tree,range,
                                 region_partial_include))
                    CODA_ASSERT(rvm_false);
                free_range(range);
                }
            }
        /* update ov buffer, merged range size */
        range = tid->x_ranges[0];
        if (TID(RESTORE_FLAG))
            {
            free(range->data);
            range->data = new_range->data;
            range->data_len = new_range->data_len;
            new_range->data = NULL;
            }
        range->nv.vmaddr = new_range->nv.vmaddr;
        range->nv.length = new_range->nv.length;
        range->nv.offset = new_range->nv.offset;
        range->end_offset = new_range->end_offset;
        free_range(new_range); 

        /* update region uncommitted reference count */
        CRITICAL(region->count_lock,
            region->n_uncommit -= (tid->x_ranges_len-1));
        }    

    return RVM_SUCCESS;
    }
/* rvm_set_range */
rvm_return_t rvm_set_range(rvm_tid,dest,length)
    rvm_tid_t       *rvm_tid;           /* transaction affected */
    void            *dest;              /* base vm address of range */
    rvm_length_t    length;             /* length of range */
    {
    int_tid_t       *tid;               /* internal tid ptr */
    region_t        *region;
    range_t         *new_range;         /* ptr to new range descriptor */
    rvm_return_t    retval = RVM_SUCCESS;

    /* basic entry checks */
    if (bad_init()) return RVM_EINIT;

    /* ignore null ranges */
    if ((rvm_optimizations != 0) && (length == 0))
        return RVM_SUCCESS;

    /* lookup and lock tid */
    if ((tid = get_tid(rvm_tid)) == NULL) /* begin tid_lock critical section */
        return RVM_ETID;

    /* lookup and lock region */
    region = find_whole_range(dest,length,r); /* begin region_lock crit sect */
    if (region == NULL)
        retval = RVM_ENOT_MAPPED;
    else
        {
        /* build new range descriptor and do optimizations */
        new_range = build_range(region,dest,length);
        retval = merge_range(tid,region,new_range);
        rw_unlock(&region->region_lock,r);   /* end region_lock crit sect */
        }

    rw_unlock(&tid->tid_lock,w);        /* end tid_lock critical section */
    return retval;
    }
/* rvm_modify_bytes */
rvm_return_t rvm_modify_bytes(rvm_tid,dest,src,length)
    rvm_tid_t           *rvm_tid;       /* transaction affected */
    void                *dest;          /* base vm address of range */
    const void                *src;           /* source of nv's */
    rvm_length_t        length;         /* length of range */
    {
    rvm_return_t        retval;

    /* call rvm_set_range to do most of the work */
    if ((retval = rvm_set_range(rvm_tid,dest,length)) != RVM_SUCCESS)
        return retval;

    /* must memmove since there is no guarantee
       that src and dest don't overlap */
/*    (void)memmove(dest,src,length); <-- not available on RT's */
    (void)BCOPY(src,dest,(int)length);

    return RVM_SUCCESS;
    }
/* calculate transaction's new value log entry size */
static void nv_size(tid,size)
    int_tid_t       *tid;               /* transaction to size */
    rvm_offset_t    *size;              /* compute length; double [out] */
    {
    range_t         *range;             /* current range */

    /* sum sizes of ranges */
    RVM_ZERO_OFFSET(*size);
    FOR_NODES_OF(tid->range_tree,range_t,range)
        *size = RVM_ADD_LENGTH_TO_OFFSET(*size,RANGE_SIZE(range));
    }

/* compute total i/o size */
static rvm_return_t nv_io_size(tid,size)
    int_tid_t       *tid;               /* transaction to size */
    rvm_offset_t    *size;              /* compute length; double [out] */
    {
    log_t           *log = tid->log;    /* log descriptor */

    /* compute length of log entries; with overhead,
       including possible wrap & split (too small to overflow) */
    nv_size(tid,size);
    *size = RVM_ADD_LENGTH_TO_OFFSET(*size,TRANS_SIZE+MIN_TRANS_SIZE);

    /* test max size */
    if (RVM_OFFSET_GTR(*size,log->status.log_size))
        return RVM_ETOO_BIG;            /* bigger than log device */
    if (RVM_OFFSET_HIGH_BITS_TO_LENGTH(*size) != 0)
        return RVM_ETOO_BIG;            /* 32 bit max */

    return RVM_SUCCESS;
    }
/* save new values for a range */
static rvm_return_t save_nv(range)
    range_t         *range;             /* modification range descriptor */
    {
    rvm_length_t    range_len;          /* save size of range */

    if (range->nv.length != 0)
        {
        range_len = RANGE_LEN(range);
        if (range->data == NULL)
            {
            range->data = malloc(range_len);
            if (range->data == NULL) return RVM_ENO_MEMORY;
            range->nvaddr = range->data;
            range->data_len = range_len;
            }
        CODA_ASSERT(range->data_len >= range_len);
    
        /* copy to old value space */
        src_aligned_bcopy(range->nv.vmaddr,range->data,
                          range->nv.length);
        }

    return RVM_SUCCESS;
    }

/* save all new values for no_flush commit */
static rvm_return_t save_all_nvs(tid)
    int_tid_t       *tid;               /* committing transaction */
    {
    range_t         *range;             /* modification range descriptor */
    rvm_return_t    retval;

    /* cache nv's for future flush */
    FOR_NODES_OF(tid->range_tree,range_t,range)
        {
        /* allocate buffer and save new values for range */
        if ((retval=save_nv(range)) != RVM_SUCCESS)
            return retval;

        /* update uncommited count */
        CODA_ASSERT(range->region->links.struct_id == region_id);
        CRITICAL(range->region->count_lock,
                 range->region->n_uncommit--);
        }

    return RVM_SUCCESS;
    }
/* merge range into queued tid */
static rvm_return_t merge_tid(q_tid,tid,new_range)
    int_tid_t       *q_tid;             /* ptr to last queued tid */
    int_tid_t       *tid;               /* ptr to new tid */
    range_t         *new_range;         /* new range ptr */
    {
    region_t        *region = new_range->region;
    range_t         *range;             /* existing range ptr */
    rvm_length_t    range_len;          /* save size of range */
    char            *nv_ptr;            /* ptr into new value buffers */
    char            *new_nv_ptr;        /* 2nd ptr into new value buffers */
    rvm_offset_t    old_offset;         /* original offset of new range */
    rvm_length_t    old_length;         /* original length of new range */
    rvm_offset_t    old_end_offset;     /* original end offset of new range */
    rvm_length_t    data_off;           /* low order bytes of data offset */
    int             i;
    rvm_return_t    retval;

    /* search for overlap/adjacency with existing ranges in same segment */
    old_offset = new_range->nv.offset;
    old_length = new_range->nv.length;
    old_end_offset = new_range->end_offset;
    if (find_overlap(q_tid,new_range,segment_partial_include,
                     &tid->trans_elim,&tid->trans_overlap,&retval))
        {
        if (retval != RVM_SUCCESS) return retval;

        /* new range totally contained in existing range, update new values */
        range = q_tid->x_ranges[0];
        nv_ptr = (char *)RVM_OFFSET_TO_LENGTH(
                   RVM_SUB_OFFSETS(old_offset,range->nv.offset));
        nv_ptr = RVM_ADD_LENGTH_TO_ADDR(nv_ptr,range->data
             + BYTE_SKEW(RVM_OFFSET_TO_LENGTH(range->nv.offset)));
        CODA_ASSERT(range->data != NULL);
        CODA_ASSERT((nv_ptr+old_length-range->data) <= range->data_len);
        BCOPY(new_range->nv.vmaddr,nv_ptr,old_length);
        goto exit;
        }
    /* see if simply inserted */
    if (q_tid->x_ranges_len == 0)
        {
        q_tid->log_size =               /* update length of queued tid */
            RVM_ADD_LENGTH_TO_OFFSET(q_tid->log_size,
                                     RANGE_SIZE(new_range));
        /* save new values if no_flush commit */
        if (TID(FLUSH_FLAG))
            {
            if (new_range->data != NULL)
                {
                free(new_range->data);
                new_range->data = NULL;
                new_range->data_len = 0;
                new_range->nvaddr = NULL;
                }
            return RVM_SUCCESS;
            }
        if ((retval=save_nv(new_range)) != RVM_SUCCESS)
            return retval;
        range = new_range;
        goto update;
        }

    /* see if existing totally contained in new and no other overlap */
    range = q_tid->x_ranges[0];
    if ((q_tid->x_ranges_len == 1)
            && (RVM_OFFSET_GEQ(range->nv.offset,old_offset)
                && RVM_OFFSET_LEQ(range->end_offset,old_end_offset)))
        {
        /* copy new range data */
        if (TID(FLUSH_FLAG))
            {
            if (new_range->data != NULL)
                {
                free(new_range->data);
                new_range->data = NULL;
                new_range->data_len = 0;
                new_range->nvaddr = NULL;
                }
            }
        else
            if ((retval=save_nv(new_range)) != RVM_SUCCESS)
                return retval;
        goto replace;
        }
    /* do general merge: reallocate new value save buffer */
    if (new_range->data != NULL)
        free(new_range->data);
    data_off = RVM_OFFSET_TO_LENGTH(new_range->nv.offset);
    new_range->data_len = ALIGNED_LEN(data_off,new_range->nv.length);
    if ((new_range->data=malloc(new_range->data_len)) == NULL)
        return RVM_ENO_MEMORY;
    new_range->nvaddr = new_range->data;

    /* save new values from new range */
    nv_ptr =
        RVM_ADD_LENGTH_TO_ADDR(new_range->data,
            RVM_OFFSET_TO_LENGTH(RVM_SUB_OFFSETS(old_offset,
                new_range->nv.offset))+BYTE_SKEW(data_off));
    CODA_ASSERT((nv_ptr+old_length-new_range->data) <= new_range->data_len);
    BCOPY(new_range->nv.vmaddr,nv_ptr,old_length);

    /* update vmaddr -- only valid if no remappings have been done
       since last commit; from now on, used only by rvmutl */
    if (range->nv.vmaddr < new_range->nv.vmaddr)
        new_range->nv.vmaddr = range->nv.vmaddr;
    /* merge new values from existing ranges
       copy leading bytes from 1st existing range */
    if (RVM_OFFSET_GTR(old_offset,range->nv.offset))
        {
        range_len =
            RVM_OFFSET_TO_LENGTH(RVM_SUB_OFFSETS(old_offset,
                                                 range->nv.offset));
        CODA_ASSERT(range->data != NULL);
        nv_ptr = RVM_ADD_LENGTH_TO_ADDR(range->data,BYTE_SKEW(
                         RVM_OFFSET_TO_LENGTH(range->nv.offset)));
        new_nv_ptr = RVM_ADD_LENGTH_TO_ADDR(new_range->data,
                                            BYTE_SKEW(data_off));
        CODA_ASSERT((new_nv_ptr-new_range->data+range_len)
               <= new_range->data_len);
        BCOPY(nv_ptr,new_nv_ptr,range_len);
        }

    /* copy trailing bytes from last existing range */
    range = q_tid->x_ranges[q_tid->x_ranges_len-1];
    if (RVM_OFFSET_GTR(range->end_offset,old_end_offset))
        {
        range_len =
            RVM_OFFSET_TO_LENGTH(RVM_SUB_OFFSETS(range->end_offset,
                                                 old_end_offset));
        CODA_ASSERT(range->data != NULL);
        nv_ptr = RVM_ADD_LENGTH_TO_ADDR(range->data,BYTE_SKEW(
                         RVM_OFFSET_TO_LENGTH(range->nv.offset)));
        nv_ptr = RVM_ADD_LENGTH_TO_ADDR(nv_ptr,range->nv.length);
        nv_ptr = RVM_SUB_LENGTH_FROM_ADDR(nv_ptr,range_len);
        new_nv_ptr = RVM_ADD_LENGTH_TO_ADDR(new_range->data,
                         BYTE_SKEW(data_off)+new_range->nv.length);
        new_nv_ptr = RVM_SUB_LENGTH_FROM_ADDR(new_nv_ptr,range_len);
        CODA_ASSERT((nv_ptr-range->data+range_len) <= range->data_len);
        CODA_ASSERT((new_nv_ptr-new_range->data+range_len)
               <= new_range->data_len);
        BCOPY(nv_ptr,new_nv_ptr,range_len);
        }
    /* delete nodes now obsolete */
    for (i = 1; i < q_tid->x_ranges_len; i++)
        {
        range = q_tid->x_ranges[i];
        q_tid->log_size =
            RVM_SUB_LENGTH_FROM_OFFSET(q_tid->log_size,
                                       RANGE_SIZE(range));
        if (!tree_delete(&q_tid->range_tree,range,
                             segment_partial_include))
            CODA_ASSERT(rvm_false);
        free_range(range);
        }
    range = q_tid->x_ranges[0];

    /* replace existing range data with new range data*/
  replace:
    q_tid->log_size =
        RVM_SUB_LENGTH_FROM_OFFSET(q_tid->log_size,
                                   RANGE_SIZE(range));
    q_tid->log_size =
        RVM_ADD_LENGTH_TO_OFFSET(q_tid->log_size,
                                 RANGE_SIZE(new_range));
    range->nv.vmaddr = new_range->nv.vmaddr;
    range->nv.length = new_range->nv.length;
    range->nv.offset = new_range->nv.offset;
    range->end_offset = new_range->end_offset;
    free(range->data);
    range->data = new_range->data;
    range->data_len = new_range->data_len;
    range->nvaddr = new_range->nvaddr;
    new_range->data = NULL;

  exit:
    free_range(new_range);
  update:    /* update region's uncommitted reference count */
    if (range->data != NULL)
        CRITICAL(region->count_lock,region->n_uncommit--);
    return RVM_SUCCESS;
    }
/* merge ranges of transaction with previous transactions */
static rvm_return_t coalesce_trans(tid,q_tid)
    int_tid_t       *tid;               /* tid to log */
    int_tid_t       *q_tid;             /* ptr to last queued tid */
    {
    range_t         *range;             /* range ptr */
    rvm_return_t    retval;

    /* merge ranges of new tid with queued tid */
    UNLINK_NODES_OF(tid->range_tree,range_t,range)
        {
        /* merge the range into queued tid */
        if ((retval=merge_tid(q_tid,tid,range)) != RVM_SUCCESS)
            return retval;
        }

    /* completely merged, update commit stamp and statistics
       scrap descriptor */
    q_tid->commit_stamp = tid->commit_stamp;
    q_tid->range_elim += tid->range_elim;
    q_tid->trans_elim += tid->trans_elim;
    q_tid->range_overlap = RVM_ADD_OFFSETS(q_tid->range_overlap,
                                           tid->range_overlap);
    q_tid->trans_overlap = RVM_ADD_OFFSETS(q_tid->trans_overlap,
                                           tid->trans_overlap);
    q_tid->n_coalesced++;
    free_tid(tid);

    return RVM_SUCCESS;
    }
/* get address of queued tid that current tid can merge with */
static int_tid_t *get_queued_tid(tid)
    int_tid_t       *tid;               /* tid to log */
    {
    log_t           *log = tid->log;    /* log descriptor */
    int_tid_t       *q_tid;             /* ptr to last queued tid */
    rvm_offset_t    size_temp;

    /* get last queued tid in flush list */
    q_tid = (int_tid_t *)log->flush_list.preventry;
    size_temp = RVM_ADD_OFFSETS(q_tid->log_size,tid->log_size);

    /* see if can legitimately merge */
    if ((log->flush_list.list.length == 0)
        || ((q_tid->flags & FLUSH_MARK) != 0)
        || ((q_tid->flags & RVM_COALESCE_TRANS) == 0)
        || (RVM_OFFSET_GTR(size_temp,log->status.log_size)))
        q_tid = NULL;                   /* no, force re-init of merge */

    return q_tid;
    }
/* establish log entry for committing tid */
static rvm_return_t queue_tid(tid)
    int_tid_t       *tid;               /* tid to log */
    {
    log_t           *log = tid->log;    /* log descriptor */
    int_tid_t       *q_tid;             /* ptr to last queued tid */
    rvm_bool_t      flush_flag;
    rvm_return_t    retval;

    /* make sure transaction not too large for log */
    if ((retval=nv_io_size(tid,&tid->log_size)) != RVM_SUCCESS)
        return retval;                  /* transaction too big to log */

    if (init_unames() != 0)             /* update uname generator */
        return RVM_EIO;
    make_uname(&tid->commit_stamp);     /* record commit timestamp */
    flush_flag = (rvm_bool_t)TID(FLUSH_FLAG); /* save flush flag */

    /* queue tid for flush */
    CRITICAL(log->flush_list_lock,      /* begin flush_list_lock crit sec */
        {
        /* test for transaction coalesce */
        if (TID(RVM_COALESCE_TRANS))
            {
            /* see if must initialize coalescing */
            if ((q_tid=get_queued_tid(tid)) == NULL)
                {
                if (flush_flag) goto enqueue; /* nothing to coalesce! */

                /* initialize transaction merger by inserting copy of tid
                   with empty tree so that ranges get reordered by merge */
                if ((q_tid=(int_tid_t *)alloc_list_entry(int_tid_id)) == NULL)
                    {
                    retval = RVM_ENO_MEMORY;
                    goto exit;
                    }
                BCOPY(tid,q_tid,sizeof(int_tid_t));
                init_tree_root(&q_tid->range_tree);
                tid->x_ranges = NULL; /* array now owned by q_tid */
                (void)move_list_entry(NULL,&log->flush_list,
                                      &q_tid->links);
                }

            /* merge ranges of tid with previously queued tid(s) */
            retval = coalesce_trans(tid,q_tid);
            goto exit;
            }
        /* save new values if necessary and queue */
        if (!flush_flag)
            if (tid->range_tree.n_nodes != 0)
                if ((retval = save_all_nvs(tid)) != RVM_SUCCESS)
                    goto exit;  /* too big for heap */

        /* enqueue new tid */
enqueue:
        (void)move_list_entry(NULL,&log->flush_list,&tid->links);

exit:;
        });                             /* end flush_list_lock crit sec */
    if (retval != RVM_SUCCESS) return retval;

    /* flush log if commit requires */
    if (flush_flag)
        retval = flush_log(log,&log->status.n_flush);

    return retval;
    }
/* rvm_begin_transaction */
rvm_return_t rvm_begin_transaction(rvm_tid,mode)
    rvm_tid_t           *rvm_tid;       /* ptr to rvm_tid */
    rvm_mode_t          mode;           /* transaction's mode */
    {
    int_tid_t           *tid;           /* internal tid */
    log_t               *log;           /* log device  */
    rvm_return_t        retval;

    /* basic entry checks */
    if (bad_init()) return RVM_EINIT;
    if ((retval=bad_tid(rvm_tid)) != RVM_SUCCESS)
        return retval;
    if ((default_log == NULL) || (default_log->dev.handle == 0))
        return RVM_ELOG;
    if ((mode != restore) && (mode != no_restore))
        return RVM_EMODE;

    /* allocate tid descriptor */
    if ((tid = make_tid(mode)) == NULL) return RVM_ENO_MEMORY;
    rvm_tid->uname = tid->uname;

    /* queue the tid on log tid_list */
    log = default_log;                  /* this must change if > 1 log */
    tid->log = log;
    CRITICAL(log->tid_list_lock,        /* begin tid_list_lock critical section */
        {
        (void) move_list_entry(NULL,&log->tid_list,&tid->links);
        });                             /* end tid_list_lock critical section */
    rvm_tid->tid = tid;
    return RVM_SUCCESS;
    }
/* rvm_abort_transaction */
rvm_return_t rvm_abort_transaction(rvm_tid)
    rvm_tid_t       *rvm_tid;           /* ptr to transaction to abort */
    {
    int_tid_t       *tid;               /* internal tid */
    log_t           *log;               /* log descriptor ptr */

    /* basic entry checks */
    if (bad_init()) return RVM_EINIT;
    if ((tid = get_tid(rvm_tid)) == NULL) /* begin tid_lock crit section */
        return RVM_ETID;

    log = tid->log;
    CRITICAL(log->tid_list_lock,        /* unlink from log's tid_list */
             move_list_entry(&log->tid_list,NULL,&tid->links));
    tid->commit_stamp.tv_sec = 1;       /* temporary mark */
    rw_unlock(&tid->tid_lock,w);        /* end tid_lock critical section */

    /* restore virtual memory */
    restore_ov(tid);

    log->status.n_abort++;              /* count transactions aborted 
                                           (may not be exact since not locked) */

    rvm_tid->tid = NULL;
    free_tid(tid);                      /* free transaction descriptor */
    return RVM_SUCCESS;
    }
/* rvm_end_transaction */
rvm_return_t rvm_end_transaction(rvm_tid,mode)
    rvm_tid_t       *rvm_tid;           /* ptr to transaction to commit */
    rvm_mode_t      mode;               /* end mode */
    {
    int_tid_t       *tid;               /* internal tid */
    log_t           *log;               /* log descriptor ptr */
    rvm_return_t    retval;

    /* basic entry checks */
    if (bad_init()) return RVM_EINIT;
    if ((mode != flush) && (mode != no_flush))
        return RVM_EMODE;
    if ((tid = get_tid(rvm_tid)) == NULL) /* begin tid_lock crit section */
        return RVM_ETID;

    /* remove tid from log's tid_list */
    log = tid->log;
    CRITICAL(log->tid_list_lock,        /* begin tid_list_lock crit section */
        {                               /* unlink tid from log's tid_list*/
        (void)move_list_entry(&log->tid_list,NULL,&tid->links);
        if (mode == flush)              /* record flush mode and count */
            {
            tid->flags |= FLUSH_FLAG;
            log->status.n_flush_commit++;
            }
        else
            log->status.n_no_flush_commit++;
        });                             /* end tid_list_lock crit section */
    tid->commit_stamp.tv_sec = 1;       /* temporary mark */
    rw_unlock(&tid->tid_lock,w);        /* end tid_lock crit section */

    /* kill null tids */
    if ((rvm_optimizations != 0) && (tid->range_tree.n_nodes == 0))
        {
        rvm_tid->tid = NULL;
        free_tid(tid);
        return RVM_SUCCESS;
        }

    /* build new value record(s) & flush if necessary */
    if ((retval=queue_tid(tid)) != RVM_SUCCESS)
        ZERO_TIME(tid->commit_stamp);
    else
        rvm_tid->tid = NULL;
    return retval;
    }
