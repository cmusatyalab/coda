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
*                   RVM internal structure support functions
*
*                   * Doubly linked list and free list functions
*                   * RVM structures cache
*                   * Initializers, Copiers, and Finalizers for RVM exported
*                     structures.
*                   * RW_lock functions
*
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if defined(hpux) || defined(__hpux)
#include <hp_bsd.h>
#endif /* hpux */
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include "rvm_private.h"

/* globals */

extern rvm_length_t     page_size;
extern rvm_length_t     page_mask;
extern char             *rvm_errmsg;    /* internal error message buffer */
extern rvm_length_t rvm_optimizations;  /* optimization switches */
/* locals */

/* locks for free lists */
RVM_MUTEX free_lists_locks[NUM_CACHE_TYPES];

/* creation count vector for instances of internal types */
long type_counts[NUM_CACHE_TYPES];

/* free list vector for internal types */
list_entry_t free_lists[NUM_CACHE_TYPES];

/* preallocation count vector for internal types free lists */
long pre_alloc[NUM_CACHE_TYPES] = {NUM_PRE_ALLOCATED};

/* allocation cache maximum counts */
long max_alloc[NUM_CACHE_TYPES] = {MAX_ALLOCATED};

/* internal structure size vector for generic list allocations */
long cache_type_sizes[NUM_CACHE_TYPES] = {CACHE_TYPE_SIZES};

#ifndef ZERO
#define ZERO 0
#endif

/* initialization lock & flag */
/* cannot be statically allocated if using pthreads */
static RVM_MUTEX        free_lists_init_lock = MUTEX_INITIALIZER;
static rvm_bool_t       free_lists_inited = rvm_false;
/*  Routines to allocate and manipulate the doubly-linked circular lists 
    used in RVM (derived from rpc2 routines)

    List headers always use the list_entry_t structure and maintain a count of
    elements on the list.  Headers can be statically or dynamically allocated,
    but must be initialized with init_list_header before use.
*/
/* routine to initialize a list header
   can be statically or dynamically allocated
*/
void init_list_header(whichlist,struct_id)
    list_entry_t    *whichlist;         /* pointer to list header */
    struct_id_t     struct_id;          /* list type */
    {
    whichlist->nextentry = whichlist;   /* pointers are to self now */
    whichlist->preventry = whichlist;
    whichlist->struct_id = struct_id;   /* set the type */
    whichlist->list.length = 0;         /* list is empty */
    whichlist->is_hdr = rvm_true;       /* mark header */
    }

/*  routine to allocate typed list cells
    creates 1 entry of id type & returns address of cell
*/
static list_entry_t *malloc_list_entry(id)
    struct_id_t     id;
    {
    register list_entry_t    *cell;

    /* allocate the element */
    cell = (list_entry_t *)
        malloc((unsigned)cache_type_sizes[ID_INDEX(id)]);
    assert(cell != NULL);
    type_counts[ID_INDEX(id)] ++;       /* count allocations */

    cell->struct_id = id;               /* cell type */
    cell->is_hdr = rvm_false;           /* is not a header */

    return cell;
    }
/*  generic routine to move elements between lists    
    the types of lists must be the same if both from & to ptrs are not null.
    if cell is NULL, the 1st entry in fromptr list is selected
    if fromptr is NULL, victim must not be NULL.
    if fromptr is not null, from->list.length is decremented.
    if toptr is not null, victim is appended & to->list.length is incremented.
    in all cases a pointer to the moved entry is returned.
*/
list_entry_t *move_list_entry(fromptr, toptr, victim)
    register list_entry_t *fromptr;     /* from list header */
    register list_entry_t *toptr;       /* to list header */
    register list_entry_t *victim;      /* pointer to entry to be moved */
    {

    if (fromptr != NULL)
        {
        assert(fromptr->is_hdr);
        if ((victim == NULL) && LIST_EMPTY((*fromptr)))
            victim = malloc_list_entry(fromptr->struct_id);
        else
            {
            if (victim == NULL)         /* choose 1st if no victim */
                victim = fromptr->nextentry;
            assert(!victim->is_hdr);
            assert(victim->list.name == fromptr);
            /* remque((void *)victim); */ /* unlink from first list */
            if (victim->nextentry)
                victim->nextentry->preventry = victim->preventry;
            if (victim->preventry)
                victim->preventry->nextentry = victim->nextentry;
            victim->nextentry = victim->preventry = NULL;

            fromptr->list.length --;
            }
        }
    else
        {
        assert(victim != NULL);
        assert(!victim->is_hdr);
        assert(toptr != NULL);
        }

    if (toptr != NULL)
        {
        assert(toptr->is_hdr);
        assert(victim->struct_id == toptr->struct_id);
        victim->list.name = toptr;
        /* insque((void *)victim,(void *)toptr->preventry); */ /* insert at tail of second list */
        victim->preventry = toptr->preventry;
        victim->nextentry = toptr;
        victim->preventry->nextentry = toptr->preventry = victim;

        toptr->list.length ++;
        }
    else victim->list.name = NULL;

    return victim;
    }
/* list insertion routine */
void insert_list_entry(entry,new_entry)
    register list_entry_t *entry;       /* existing list entry */
    register list_entry_t *new_entry;   /* entry to insert after entry */
    {
    list_entry_t        *list_hdr;      /* header of target list */

    /* basic sanity checks */
    assert(!new_entry->is_hdr);
    assert(new_entry->struct_id == entry->struct_id);

    /* discover header of target list */
    if (entry->is_hdr)
        list_hdr = entry;
    else
        list_hdr = entry->list.name;

    /* further sanity checks */
    assert(list_hdr != NULL);
    assert(list_hdr->is_hdr);
    assert(new_entry->struct_id == list_hdr->struct_id);

    /* remove list_entry from any list it might be on */
    if (new_entry->list.name != NULL)
        (void)move_list_entry(NULL,NULL,new_entry);

    /* do insertion into target list */
    new_entry->list.name = list_hdr;
    new_entry->nextentry = entry->nextentry;
    entry->nextentry = new_entry;
    new_entry->preventry = entry;
    new_entry->nextentry->preventry = new_entry;
    list_hdr->list.length++;

    }
/* internal types free lists support */

/* initialization -- call once at initialization
   free lists will be initialized and be pre-allocated with the number of
   elements specified in NUM_PRE_ALLOCATED (from rvm_private.h)
*/
static void init_free_lists()
    {
    list_entry_t    *cell;
    int             i,j;

    assert(!free_lists_inited);
    for (i = 0; i < ID_INDEX(struct_last_cache_id); i++)
        {
        init_list_header(&free_lists[i],INDEX_ID(i));
        mutex_init(&free_lists_locks[i]);
        for (j = 0; j < pre_alloc[i]; j++)
            {
            cell = malloc_list_entry(INDEX_ID(i));
            assert(cell != NULL);
            (void)move_list_entry(NULL,&free_lists[i],cell);
            }
        }
    }

/* get a cell from free list */
list_entry_t *alloc_list_entry(id)
    struct_id_t     id;
    {
    list_entry_t    *cell;

    assert((((long)id > (long)struct_first_id) && 
           ((long)id < (long)struct_last_cache_id)));

    CRITICAL(free_lists_locks[ID_INDEX(id)],
        {
        cell = move_list_entry(&free_lists[ID_INDEX(id)],
                               NULL,NULL);
        });

    return cell;
    }
/* kill cell */
static void kill_list_entry(cell)
    list_entry_t    *cell;
    {
    assert(cell != NULL);

    /* unlink from present list */
    if (cell->list.name != NULL)
        (void)move_list_entry(cell->list.name,NULL,cell);

    /* terminate with extreme prejudice */
    type_counts[ID_INDEX(cell->struct_id)] --;
    free((char *)cell);
    }

/* move cell to free list
   will remove cell from any list that it is on before freeing
*/
static void free_list_entry(cell)
    register list_entry_t    *cell;
    {
    int id_index;
    assert(cell != NULL);
    assert((((long)cell->struct_id>(long)struct_first_id) && 
           ((long)cell->struct_id<(long)struct_last_cache_id)));


    id_index = ID_INDEX(cell->struct_id);
    CRITICAL(free_lists_locks[id_index],
        {                               /* begin free_list_lock crit sec */
        if (max_alloc[id_index] >
            free_lists[id_index].list.length)
            {
             /* move to appropriate free list */
            (void)move_list_entry(cell->list.name,
                              &free_lists[id_index],
                              cell);
            }
        else
            /* kill if enough of this type cached */
            kill_list_entry(cell);
        });                             /* end free_list_lock crit sec */
    }
/* clear free lists */
void clear_free_list(id)
    struct_id_t     id;                 /* type of free list */
    {
    list_entry_t    *cell;

    CRITICAL(free_lists_locks[ID_INDEX(id)],
        {                               /* begin free_list_lock crit sec */
        while (LIST_NOT_EMPTY(free_lists[ID_INDEX(id)]))
            {
            cell = free_lists[ID_INDEX(id)].nextentry;
            kill_list_entry(cell);
            }
        });                             /* end free_list_lock crit sec */
    }

void clear_free_lists()
    {
    int             i;

    for (i = 0; i < ID_INDEX(struct_last_cache_id); i++)
        clear_free_list(INDEX_ID(i));
    }
/* unique name generator */
/* Cannot be statically allocated in pthreads */
static RVM_MUTEX     uname_lock = MUTEX_INITIALIZER;
static struct timeval   uname = {0,0};

void make_uname(new_uname)
    struct timeval  *new_uname;
    {
    /* generate a uname */
    CRITICAL(uname_lock,
        {
        new_uname->tv_sec = uname.tv_sec;
        new_uname->tv_usec = uname.tv_usec++;
        if (uname.tv_usec >= 1000000)
            {
            uname.tv_sec++;
            uname.tv_usec = 0;
            }
        });
    }

/* uname initialization */
long init_unames()
    {
    struct timeval  new_uname;
    long            retval;

    retval= gettimeofday(&new_uname,(struct timezone *)NULL);
    if ( retval ) {
	    printf("init_unames: retval %ld\n", retval);
	    perror("init_names:");
	    return retval;
    }
    
    CRITICAL(uname_lock,
        {
        if (TIME_GTR(new_uname,uname))
            uname = new_uname;
        });

    return 0;
    }

/* module initialization */
/* Locks cannot be statically allocated in pthreads. */
long init_utils()
    {
    CRITICAL(free_lists_init_lock,
        {
        if (!free_lists_inited)
            {
            init_free_lists();
            free_lists_inited = rvm_true;
            }
        });

    return init_unames();
    }
/* time value arithmetic */
struct timeval add_times(x,y)
    struct timeval  *x;
    struct timeval  *y;
    {
    struct timeval  tmp;
    
    tmp.tv_sec = x->tv_sec + y->tv_sec;
    tmp.tv_usec = x->tv_usec + y->tv_usec;
    if (tmp.tv_usec >= 1000000)
        {
        tmp.tv_sec++;
        tmp.tv_usec -= 1000000;
        }
    return tmp;
    }

struct timeval sub_times(x,y)
    struct timeval  *x;
    struct timeval  *y;
    {
    struct timeval  tmp;
    
    tmp = *x;
    if (tmp.tv_usec < y->tv_usec)
        {
        tmp.tv_sec--;
        tmp.tv_usec += 1000000;
        }
    tmp.tv_usec -= y->tv_usec;
    tmp.tv_sec -= y->tv_sec;
    return tmp;
    }

/* round time to seconds */
long round_time(x)
    struct timeval  *x;
    {
    if (x->tv_usec >= 500000)
        return x->tv_sec+1;

    return x->tv_sec;
    }
/* region descriptor allocator/finalizer */
region_t *make_region()
    { 
    region_t    *region;
    
    region = (region_t *)alloc_list_entry(region_id);
    if (region != NULL)
        {
        init_rw_lock(&region->region_lock);
        mutex_init(&region->count_lock);
        }
    return region;
    }

void free_region(region)
    region_t    *region;
    {
    assert(region->links.struct_id == region_id);
    assert(LOCK_FREE(region->count_lock));

    rw_lock_clear(&region->region_lock);
    mutex_clear(&region->count_lock);
    free_list_entry((list_entry_t *) region);
    }
/* construct full path name for file names */
char *make_full_name(dev_str,dev_name,retval)
    char            *dev_str;           /* device name */
    char            *dev_name;          /* device name buffer for descriptor */
    rvm_return_t    *retval;            /* return code */
    {
    char            wd_name[MAXPATHLEN+1]; /* current working directory */
    long            wd_len = 0;         /* working dir string length */
    long            len;                /* length of total device path */

    *retval = RVM_SUCCESS;
    len = strlen(dev_str) + 1;          /* one extra for null terminator */

    /* see if working directory must be added to device name */
#ifndef DJGPP
    if (*dev_str != '/')
#else
    if (dev_str[1] != ':')
#endif
        {
        if (getcwd(wd_name, sizeof(wd_name)) == 0)
            assert(rvm_false);
        wd_len = strlen(wd_name);
        len += (wd_len+1);              /* one extra for '/' */
        }
    if (len > (MAXPATHLEN+1))
        {
        *retval = RVM_ENAME_TOO_LONG;
        return NULL;
        }

    /* allocate buffer, if necessary, and copy full path name */
    if (dev_name == NULL)
        if ((dev_name=malloc(len)) == NULL)
            {
            *retval = RVM_ENO_MEMORY;
            return NULL;
            }

    *dev_name = 0;
    if (wd_len != 0)
        {
        (void)strcpy(dev_name,wd_name);
#ifdef DJGPP
	if (dev_str[0] == '/' || dev_str[0] != '\\')
	    dev_name[2] = '\0';
#endif
        dev_name[wd_len] = '/';
        dev_name[wd_len+1] = 0;
        }
    (void)strcat(dev_name,dev_str);

    return dev_name;
    }
/* device descriptor initializer */
rvm_return_t dev_init(dev,dev_str)
    device_t        *dev;               /* device descriptor */
    char            *dev_str;           /* device name */
    {
    rvm_return_t    retval;

    /* set device name */
    if (dev_str != NULL)
        {
        dev->name = make_full_name(dev_str,NULL,&retval);
        if (retval != RVM_SUCCESS) return retval;
        dev->name_len = strlen(dev->name)+1;
        }

    dev->iov = NULL;
    dev->iov_len = 0;
    dev->raw_io = rvm_false;
    dev->read_only = rvm_false;
    dev->wrt_buf = NULL;
    dev->buf_start = NULL;
    dev->buf_end = NULL;
    dev->ptr = NULL;
    RVM_ZERO_OFFSET(dev->sync_offset);
    dev->pad_buf = NULL;
    dev->pad_buf_len = 0;

    return RVM_SUCCESS;
    }
/* segment descriptor allocator/finalizer */
seg_t *make_seg(seg_dev_name,retval)
    char            *seg_dev_name;      /* segment device name */
    rvm_return_t    *retval;            /* return code */
    {
    seg_t           *seg;

    /* initialize segment descriptor */
    seg = (seg_t *)alloc_list_entry(seg_id);
    if (seg != NULL)
        {
        /* initialize segment device */
        if ((*retval=dev_init(&seg->dev,seg_dev_name)) != RVM_SUCCESS)
            {
            free(seg);
            return NULL;                /* no space for device name */
            }

        /* initialize the lists & locks*/
        mutex_init(&seg->seg_lock);
        mutex_init(&seg->dev_lock);
        init_list_header(&seg->map_list,region_id);
        init_list_header(&seg->unmap_list,region_id);
        }

    return seg;
    }

void free_seg(seg)
    seg_t       *seg;
    {
    assert(seg->links.struct_id == seg_id);

    /* all lists should be empty and locks free */
    assert(LIST_EMPTY(seg->map_list));
    assert(LIST_EMPTY(seg->unmap_list));
    assert(LOCK_FREE(seg->seg_lock));
    assert(LOCK_FREE(seg->dev_lock));

    mutex_clear(&seg->seg_lock);
    mutex_clear(&seg->dev_lock);
    if (seg->dev.name != NULL)
        {
        free(seg->dev.name);            /* free device name char array */
        seg->dev.name = NULL;
        }
    free_list_entry(&seg->links);
    }
/* segemnt dictionary finalizer */
void free_seg_dict_vec(log)
    log_t           *log;
    {
    int             i;                  /* loop counter */

    if (log->seg_dict_vec != NULL)
        {
        /* free tree roots */
        for (i=0; i < log->seg_dict_len; i++)
            clear_tree_root(&log->seg_dict_vec[i].mod_tree);

        free((char *)log->seg_dict_vec);
        log->seg_dict_vec = NULL;
        log->seg_dict_len = 0;
        }
    }
/* log descriptor finalizer */
void free_log(log)
    log_t           *log;
    {
    assert(log->links.struct_id == log_id);
    assert(LIST_EMPTY(log->tid_list));     /* should not be any transactions
                                              now */
    assert(LIST_EMPTY(log->flush_list));   /* should not be any queued no_flush
                                              transactions either */
    assert(LIST_EMPTY(log->special_list)); /* no special log records should
                                              be left */
    assert(LOCK_FREE(log->dev_lock));      /* all locks should be free */
    assert(LOCK_FREE(log->tid_list_lock));
    assert(LOCK_FREE(log->flush_list_lock));
    assert(LOCK_FREE(log->special_list_lock));
    assert(RW_LOCK_FREE(log->flush_lock));
    assert(LOCK_FREE(log->truncation_lock));
    assert(LOCK_FREE(log->daemon.lock));
    assert((log->daemon.thread != ZERO)
           ? log->daemon.state == terminate : 1);

    /* finalizer control structures */
    mutex_clear(&log->dev_lock);
    mutex_clear(&log->tid_list_lock);
    mutex_clear(&log->flush_list_lock);
    mutex_clear(&log->special_list_lock);
    rw_lock_clear(&log->flush_lock);
    mutex_clear(&log->truncation_lock);
    mutex_clear(&log->daemon.lock);
    condition_clear(&log->daemon.code);
    condition_clear(&log->daemon.flush_flag);
    condition_clear(&log->daemon.wake_up);

    /* free malloc-ed buffers */
    if (log->dev.name != NULL)
        free(log->dev.name);            /* kill name string */
    if (log->dev.iov != NULL)
        free((char *)log->dev.iov);     /* kill io vector */
    if (log->dev.wrt_buf != NULL)       /* kill raw io gather write buffer */
        page_free(log->dev.wrt_buf,log->dev.wrt_buf_len);
    log->dev.wrt_buf_len = 0;
    log->dev.name = NULL;
    log->dev.iov = NULL;
    free_log_buf(log);                  /* kill recovery buffers */

    free_list_entry(&log->links);       /* free descriptor */
    }
/* log descriptor allocation */
log_t *make_log(log_dev_name,retval)
    char            *log_dev_name;      /* device name */
    rvm_return_t    *retval;            /* return code */
    {
    log_t           *log;
    log_buf_t       *log_buf;

    /* initialize log descriptor */
    if ((log = (log_t *)alloc_list_entry(log_id)) != NULL)
        {
        /* init device and status */
        if ((*retval=dev_init(&log->dev,log_dev_name)) != RVM_SUCCESS)
            {
            free(log);
            return NULL;                /* no space for device name */
            }
	/* first and foremost */
	log->trunc_thread = (cthread_t) 0;

        log->status.valid = rvm_false;
        log->status.log_empty = rvm_false;
        log->status.trunc_state = 0;
        log->status.flush_state = 0;

        /* init log flush structures */
        log->trans_hdr.struct_id = trans_hdr_id;
        log->rec_end.struct_id = rec_end_id;
        log->log_wrap.struct_id = log_wrap_id;
        log->log_wrap.struct_id2 = log_wrap_id;  /* for scan_wrap_reverse() */
        log->log_wrap.rec_length = sizeof(log_wrap_t);

        /* init recovery buffer and dictionary */
        log_buf = &log->log_buf;
        log_buf->buf = NULL;
        log_buf->length = 0;
        RVM_ZERO_OFFSET(log_buf->buf_len);
        log_buf->aux_buf = NULL;
        log_buf->aux_length = 0;
        log_buf->split_ok = rvm_false;
        log->seg_dict_vec = NULL;
        log->seg_dict_len = 0;
        log->in_recovery = rvm_false;
        mutex_init(&log->truncation_lock);
        init_rw_lock(&log->flush_lock);
        log_buf->prev_rec_num = 0;
        ZERO_TIME(log_buf->prev_timestamp);
        log_buf->prev_direction = rvm_false;

        /* init lists and locks */
        mutex_init(&log->dev_lock);
        mutex_init(&log->tid_list_lock);
        init_list_header(&log->tid_list,int_tid_id);
        mutex_init(&log->flush_list_lock);
        init_list_header(&log->flush_list,int_tid_id);
        mutex_init(&log->special_list_lock);
        init_list_header(&log->special_list,log_special_id);

        /* init daemon control structure */
        log->daemon.thread = ZERO;
        mutex_init(&log->daemon.lock);
        condition_init(&log->daemon.code);
        condition_init(&log->daemon.flush_flag);
        condition_init(&log->daemon.wake_up);
        log->daemon.state = rvm_idle;
        }

    return log;
    }
/* log special types allocation/deallocation */
log_special_t *make_log_special(special_id,length)
    struct_id_t     special_id;         /* id of special type */
    rvm_length_t    length;             /* length of type-specific allocation */
    {
    log_special_t   *special;           /* ptr to descriptor allocated */
    char            *buf=NULL;          /* type-specific buffer */
    
    if ((special=(log_special_t *)alloc_list_entry(log_special_id))
        != NULL)
        {
        special->struct_id = special_id; /* set "real" type */

        /* buffer allocation */
        if ((length=ROUND_TO_LENGTH(length)) != 0)
            if ((buf = malloc((unsigned)length)) == NULL)
                {
                free_list_entry((list_entry_t *)special);
                return NULL;
                }
        special->rec_length = LOG_SPECIAL_SIZE + length;

        /* type specific initialization */
        switch (special_id)
            {
          case log_seg_id:
            special->special.log_seg.name=buf;
            break;
          default: assert(rvm_false);       /* should never happen */
            }
        }

    return special;
    }
void free_log_special(special)
    log_special_t   *special;           /* ptr to descriptor allocated */
    {
    assert(special->links.struct_id == log_special_id);

    /* type specific finalization */
    switch (special->struct_id)
        {
      case log_seg_id:
        if (special->special.log_seg.name != NULL)
            {
            free(special->special.log_seg.name);
            special->special.log_seg.name = NULL;
            }
        break;
      default: assert(rvm_false);       /* should not happen */
        }

    free_list_entry((list_entry_t *)special);

    }
/* range descriptor allocator/finalizer */
range_t *make_range()
    { 
    register range_t    *range;
    
    if ((range = (range_t *)alloc_list_entry(range_id)) != NULL)
        {
        range->links.node.lss = NULL;
        range->links.node.gtr = NULL;
        range->links.node.bf = 0;
        range->nvaddr = NULL;
        range->data = NULL;
        range->data_len = 0;
        range->nv.struct_id = nv_range_id;
        range->nv.is_split = rvm_false;
        }

    return range;
    }

void free_range(range)
    register range_t    *range;
    {
    assert(range->links.node.struct_id == range_id);

    if (range->data != NULL)
        {
        free(range->data);
        range->data = NULL;
        range->nvaddr = NULL;
        range->data_len = 0;
        }

    range->links.entry.list.name = NULL; /* not really on any list */
    range->links.entry.is_hdr = rvm_false;
    free_list_entry(&range->links.entry);
    }
/* internal transaction descriptor allocator/finalizer */

int_tid_t *make_tid(mode)
    rvm_mode_t      mode;               /* transaction begin mode */
    { 
    register int_tid_t  *tid;
    
    tid = (int_tid_t *)alloc_list_entry(int_tid_id);
    if (tid != NULL)
        {
        make_uname(&tid->uname);
        init_rw_lock(&tid->tid_lock);
        init_tree_root(&tid->range_tree);
        tid->x_ranges = NULL;
        tid->x_ranges_alloc = 0;
        tid->x_ranges_len = 0;
        tid->n_coalesced = 0;
        tid->range_elim = 0;
        tid->trans_elim = 0;
        RVM_ZERO_OFFSET(tid->range_overlap);
        RVM_ZERO_OFFSET(tid->trans_overlap);
        ZERO_TIME(tid->commit_stamp);
        tid->flags = rvm_optimizations & RVM_ALL_OPTIMIZATIONS;
        if (mode == restore) tid->flags |= RESTORE_FLAG;
        tid->split_range.nv.struct_id = nv_range_id;
        tid->back_link = sizeof(trans_hdr_t);
        }

    return tid;
    }

void free_tid(tid)
    register int_tid_t  *tid;
    {
    range_t           *range;

    assert(tid->links.struct_id == int_tid_id);
    rw_lock_clear(&tid->tid_lock);

    /* free range list */
    UNLINK_NODES_OF(tid->range_tree,range_t,range)
        free_range((range_t *)range);
    clear_tree_root(&tid->range_tree);

    /* free search vector */
    if (tid->x_ranges != NULL)
        {
        free(tid->x_ranges);
        tid->x_ranges = NULL;
        }

    /* free tid */
    free_list_entry(&tid->links);
    }
/* mem_region nodes for mapping */
mem_region_t *make_mem_region()
    { 
    register mem_region_t  *node;
    
    node = (mem_region_t *)alloc_list_entry(mem_region_id);
    if (node != NULL)
        {
        node->region = NULL;
        }

    return node;
    }

void free_mem_region(node)
    register mem_region_t    *node;
    {
    assert(node->links.node.struct_id == mem_region_id);

    /* free node */
    node->links.entry.list.name = NULL; /* not really on any list */
    node->links.entry.is_hdr = rvm_false;
    free_list_entry(&node->links.entry);
    }
/* dev_region nodes for recovery */
dev_region_t *make_dev_region()
    { 
    register dev_region_t  *node;
    
    node = (dev_region_t *)alloc_list_entry(dev_region_id);
    if (node == NULL) return NULL;

    node->nv_buf = NULL;
    node->nv_ptr = NULL;
    RVM_ZERO_OFFSET(node->log_offset);

    return node;
    }

void free_dev_region(node)
    register dev_region_t    *node;
    {
    assert(node->links.node.struct_id == dev_region_id);

    /* free node */
    node->links.entry.list.name = NULL; /* not really on any list */
    node->links.entry.is_hdr = rvm_false;

    if (node->nv_buf != NULL)
        {
        assert(node->nv_buf->struct_id == nv_buf_id);
        if ((--(node->nv_buf->ref_cnt)) == 0)
            {
            free(node->nv_buf);
            node->nv_buf = NULL;
            node->nv_ptr = NULL;
            }
        }
    free_list_entry(&node->links.entry);
    }
/* RVM exported structures support */

static void free_export(cell,struct_id)
    list_entry_t    *cell;
    struct_id_t     struct_id;
    {
    cell->struct_id = struct_id;
    cell->list.name = NULL;
    cell->preventry = NULL;
    cell->nextentry = NULL;
    cell->is_hdr = rvm_false;
    free_list_entry(cell);
    }


/* rvm_region_t functions    */
rvm_region_t *rvm_malloc_region()
    { 
    rvm_region_t    *new_rvm_region;
    
    if (!free_lists_inited) (void)init_utils();
    new_rvm_region = (rvm_region_t *)alloc_list_entry(region_rvm_id);
    if (new_rvm_region != NULL)
        {
        rvm_init_region(new_rvm_region);
        new_rvm_region->from_heap = rvm_true;
        }
    return new_rvm_region;
    }
        
void rvm_free_region(rvm_region)
    rvm_region_t    *rvm_region;
    {
    if ((!bad_region(rvm_region))&&(free_lists_inited)&&
        (rvm_region->from_heap))
        free_export((list_entry_t *)rvm_region,region_rvm_id);
    }
void rvm_init_region(rvm_region)
    rvm_region_t    *rvm_region;
    { 
    if (rvm_region != NULL)
        {
        BZERO((char *) rvm_region,sizeof(rvm_region_t));
        rvm_region->struct_id = rvm_region_id;
        }
    }

rvm_region_t *rvm_copy_region(rvm_region)
    rvm_region_t    *rvm_region;
    {
    rvm_region_t    *new_rvm_region;
    
    if (bad_region(rvm_region)) return NULL;
    if (!free_lists_inited) (void)init_utils();

    new_rvm_region = (rvm_region_t *)alloc_list_entry(region_rvm_id);
    if (new_rvm_region != NULL)
        {
        (void)BCOPY((char *)rvm_region,(char *)new_rvm_region,
                    sizeof(rvm_region_t));
        new_rvm_region->from_heap = rvm_true;
        }
    return new_rvm_region;
    }
/* rvm_statistics_t functions */
rvm_statistics_t *rvm_malloc_statistics()
    { 
    rvm_statistics_t    *new_rvm_statistics;
    
    new_rvm_statistics = (rvm_statistics_t *)
                         alloc_list_entry(statistics_rvm_id);
    if (new_rvm_statistics != NULL)
        {
        rvm_init_statistics(new_rvm_statistics);
        new_rvm_statistics->from_heap = rvm_true;
        }
    return new_rvm_statistics;
    }

void rvm_free_statistics(rvm_statistics)
    rvm_statistics_t    *rvm_statistics;
    {
    if ((!bad_statistics(rvm_statistics)) && (free_lists_inited)
        && (rvm_statistics->from_heap))
        free_export((list_entry_t *)rvm_statistics,statistics_rvm_id);
    }
void rvm_init_statistics(rvm_statistics)
    rvm_statistics_t    *rvm_statistics;
    {
    if (rvm_statistics != NULL)
        {
        BZERO((char *)rvm_statistics,sizeof(rvm_statistics_t));
        rvm_statistics->struct_id = rvm_statistics_id;
        }
    }

rvm_statistics_t *rvm_copy_statistics(rvm_statistics)
    rvm_statistics_t   *rvm_statistics;
    {
    rvm_statistics_t   *new_rvm_statistics;
    
    if (bad_statistics(rvm_statistics)) return NULL;
    if (!free_lists_inited) (void)init_utils();

    new_rvm_statistics =
        (rvm_statistics_t *)alloc_list_entry(statistics_rvm_id);
    if (new_rvm_statistics != NULL)
        {
        (void) BCOPY((char *)rvm_statistics,(char *)new_rvm_statistics,
                     sizeof(rvm_statistics_t));
        new_rvm_statistics->from_heap = rvm_true;
        }
    return new_rvm_statistics;
    }
/* rvm_options_t functions */

rvm_options_t *rvm_malloc_options()
    { 
    rvm_options_t    *new_rvm_options;
    
    if (!free_lists_inited) (void)init_utils();
    new_rvm_options = (rvm_options_t *)
        alloc_list_entry(options_rvm_id);
    if (new_rvm_options != NULL)
        {
        rvm_init_options(new_rvm_options);
        new_rvm_options->from_heap = rvm_true;
        }
    return new_rvm_options;
    }

void rvm_free_options(rvm_options)
    rvm_options_t    *rvm_options;
    {
    if ((!bad_options(rvm_options)) && (free_lists_inited)
        && (rvm_options->from_heap))
        {
        /* release tid_array */
        if (rvm_options->tid_array != NULL)
            {
            free((char *)rvm_options->tid_array);
            rvm_options->tid_array = (rvm_tid_t *)NULL;
            rvm_options->n_uncommit = 0;
            }

        /* free options record */
        free_export((list_entry_t *)rvm_options,options_rvm_id);
        }
    }
void rvm_init_options(rvm_options)
    rvm_options_t    *rvm_options;
    {
    if (rvm_options != NULL)
        {
        BZERO((char *)rvm_options,sizeof(rvm_options_t));
        rvm_options->struct_id = rvm_options_id;
        rvm_options->truncate = TRUNCATE;
        rvm_options->recovery_buf_len = RECOVERY_BUF_LEN;
        rvm_options->flush_buf_len = FLUSH_BUF_LEN;
        rvm_options->max_read_len = MAX_READ_LEN;
        rvm_options->create_log_file = rvm_false;
        RVM_ZERO_OFFSET(rvm_options->create_log_size);
        rvm_options->create_log_mode = 0600;
        }
    }

rvm_options_t *rvm_copy_options(rvm_options)
    rvm_options_t   *rvm_options;
    {
    rvm_options_t   *new_rvm_options;
    
    if (bad_options(rvm_options)) return NULL;
    if (!free_lists_inited) (void)init_utils();

    new_rvm_options = (rvm_options_t *)
        alloc_list_entry(options_rvm_id);
    if (new_rvm_options != NULL)
        {
        (void) BCOPY((char *)rvm_options,(char *)new_rvm_options,
                     sizeof(rvm_options_t));
        new_rvm_options->from_heap = rvm_true;
        }
    return new_rvm_options;
    }
/*      rvm_tid_t functions    */

rvm_tid_t *rvm_malloc_tid()
    { 
    rvm_tid_t    *new_rvm_tid;
    
    if (!free_lists_inited) (void)init_utils();
    new_rvm_tid = (rvm_tid_t *)alloc_list_entry(tid_rvm_id);
    if (new_rvm_tid != NULL)
        {
        rvm_init_tid(new_rvm_tid);
        new_rvm_tid->from_heap = rvm_true;
        }
    return new_rvm_tid;
    }
        
void rvm_free_tid(rvm_tid)
    rvm_tid_t    *rvm_tid;
    {
    if ((!bad_tid(rvm_tid))&&(free_lists_inited)&&
        (rvm_tid->from_heap))
        free_export((list_entry_t *)rvm_tid,tid_rvm_id);
    }

void rvm_init_tid(rvm_tid_t *rvm_tid)
{ 
	if (rvm_tid != NULL) {
		BZERO((char *)rvm_tid,sizeof(rvm_tid_t));
		rvm_tid->struct_id = rvm_tid_id;
        }
}

rvm_tid_t *rvm_copy_tid(rvm_tid)
    rvm_tid_t    *rvm_tid;
    {
    rvm_tid_t    *new_rvm_tid;
    
    if (bad_tid(rvm_tid)) return NULL;
    if (!free_lists_inited) (void)init_utils();

    new_rvm_tid = (rvm_tid_t *)alloc_list_entry(tid_rvm_id);
    if (new_rvm_tid != NULL)
        {
        (void) BCOPY((char *)rvm_tid,(char *)new_rvm_tid,
                     sizeof(rvm_tid_t));
        new_rvm_tid->from_heap = rvm_true;
        }
    return new_rvm_tid;
    }
/* RVM User enumeration type print name support */
static char *return_codes[(long)rvm_last_code-(long)rvm_first_code-1] =
    {
    "RVM_EINIT","RVM_EINTERNAL","RVM_EIO","RVM_EPLACEHOLDER","RVM_ELOG",
    "RVM_ELOG_VERSION_SKEW","RVM_EMODE","RVM_ENAME_TOO_LONG",
    "RVM_ENO_MEMORY","RVM_ENOT_MAPPED","RVM_EOFFSET",
    "RVM_EOPTIONS","RVM_EOVERLAP","RVM_EPAGER","RVM_ERANGE",
    "RVM_EREGION","RVM_EREGION_DEF","RVM_ESRC","RVM_ESTATISTICS",
    "RVM_ESTAT_VERSION_SKEW","RVM_ETERMINATED","RVM_ETHREADS",
    "RVM_ETID","RVM_ETOO_BIG","RVM_EUNCOMMIT",
    "RVM_EVERSION_SKEW","RVM_EVM_OVERLAP"
     };

static char *rvm_modes[(long)rvm_last_mode-(long)rvm_first_mode-1] =
    {
    "restore","no_restore","flush","no_flush"
    };

static char *rvm_types[(long)rvm_last_struct_id-(long)rvm_first_struct_id-1] =
    {
    "rvm_region_t","rvm_options_t","rvm_tid_t","rvm_statistics_id"
    };
/* RVM enumeration type print name routines */
char *rvm_return(code)
    rvm_return_t    code;
    {
    if (code == RVM_SUCCESS) return "RVM_SUCCESS";

    if (((long)code > (long)rvm_first_code) &&
        ((long)code < (long)rvm_last_code))
        return return_codes[(long)code-(long)rvm_first_code-1];
    else
        return "Invalid RVM return code";
    }
char *rvm_mode(mode)
    rvm_mode_t      mode;
    {
    if (((long)mode > (long)rvm_first_mode) &&
        ((long)mode < (long)rvm_last_mode))
        return rvm_modes[(long)mode-(long)rvm_first_mode-1];
    else
        return "Invalid RVM transaction mode";
    }
char *rvm_type(id)
    rvm_struct_id_t   id;
    {
    if (((long)id > (long)rvm_first_struct_id) &&
        ((long)id < (long)rvm_last_struct_id))
        return rvm_types[(long)id-(long)rvm_first_struct_id-1];
    else
        return "Invalid RVM structure type";
    }
/* Byte-aligned checksum and move functions */

/* zero-pad unused bytes of word */
rvm_length_t zero_pad_word(word,addr,leading)
    rvm_length_t    word;               /* value to be zero-padded */
    char            *addr;              /* address of 1st/last byte */
    rvm_bool_t      leading;            /* true if leading bytes are zeroed */
    {
    char            *word_array = (char *)&word; /* byte access of
						    word value */
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
rvm_length_t chk_sum(nvaddr,len)
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
        chk_sum += zero_pad_word(base[length],&nvaddr[len-1],rvm_false);
    if (length <= 1) return chk_sum;

    /* form check sum of remaining full words */
    for (i=1; i < length; i++)
        chk_sum += base[i];

    return chk_sum;
    }
/* copy arbitrarily aligned range, maintaining 1st src byte alignment */
void src_aligned_bcopy(src,dest,len)
    char            *src;               /* source address */
    char            *dest;              /* destination address */
    rvm_length_t    len;                /* length of range */
    {

    if (len != 0)
        (void)BCOPY(src,
                    RVM_ADD_LENGTH_TO_ADDR(dest,BYTE_SKEW(src)),
                    len);

    }

/* copy arbitrarily aligned range, maintaining 1st dest byte alignment */
void dest_aligned_bcopy(src,dest,len)
    char            *src;               /* source address */
    char            *dest;              /* destination address */
    rvm_length_t    len;                /* length of range */
    {

    if (len != 0)
        (void)BCOPY(RVM_ADD_LENGTH_TO_ADDR(src,BYTE_SKEW(dest)),
                    dest,len);

    }
/* rw_lock functions */

/* rw_lock initializer */
void init_rw_lock(rwl)
    rw_lock_t   *rwl;
    {

    mutex_init(&rwl->mutex);
    init_list_header(&rwl->queue,rw_qentry_id);
    rwl->read_cnt = 0;
    rwl->write_cnt = 0;
    rwl->lock_mode = f;
    }

/* rw_lock finalizer */
void rw_lock_clear(rwl)
    rw_lock_t   *rwl;
    {

    assert(LOCK_FREE(rwl->mutex));
    assert(LIST_EMPTY(rwl->queue));
    assert(rwl->read_cnt == 0);
    assert(rwl->write_cnt == 0);
    assert(rwl->lock_mode == f);

    mutex_clear(&rwl->mutex);
    }
void rw_lock(rwl,mode)
    rw_lock_t       *rwl;               /* ptr to rw_lock structure */
    rw_lock_mode_t  mode;               /* r or w */
    {
#ifdef RVM_USELWP
    if (mode == r) ObtainReadLock(&rwl->mutex);
    else           ObtainWriteLock(&rwl->mutex);
#else
    rw_qentry_t      q;                 /* queue entry */

    CRITICAL(rwl->mutex,                /* begin rw_lock mutex crit sec */
        {
        assert((mode == r) || (mode == w));
        assert(rwl->read_cnt >= 0);
        assert((rwl->write_cnt >= 0) && (rwl->write_cnt <= 1));
        assert((rwl->write_cnt > 0) ? (rwl->read_cnt == 0) : 1);
        assert((rwl->read_cnt > 0) ? (rwl->write_cnt == 0) : 1);

        /* see if must block */
        if (((mode == w) && ((rwl->read_cnt+rwl->write_cnt) != 0))
            || ((mode == r) && (rwl->write_cnt != 0))
            || (LIST_NOT_EMPTY(rwl->queue))) /* this term prevents starvation 
                                                of writers by readers */
            {
            /* must block: initialize queue entry & put on lock queue */
            q.links.struct_id = rw_qentry_id;
            q.links.is_hdr = rvm_false;
            q.links.list.name = NULL;
            condition_init(&q.wait);
            q.mode = mode;
            (void)move_list_entry(NULL,&rwl->queue,(list_entry_t *)&q);

            /* wait to be signaled when access ready */
            condition_wait(&q.wait,&rwl->mutex);
            assert(rwl->lock_mode == mode);
            assert((mode == w) ?
                   ((rwl->write_cnt==1) && (rwl->read_cnt==0)) : 1);
            assert((mode == r) ?
                   ((rwl->write_cnt==0) && (rwl->read_cnt>=1)) : 1);
            }
        else
            {                           /* immediate access: set the lock */
            assert((rwl->lock_mode == r) || (rwl->lock_mode == f));
            if (mode == r) rwl->read_cnt++;
            else rwl->write_cnt++;
            rwl->lock_mode = mode;
            }
        });                             /* end rw_lock mutex crit sec */
#endif
    }
void rw_unlock(rwl,mode)
    rw_lock_t       *rwl;               /* ptr to rw_lock structure */
    rw_lock_mode_t  mode;               /* r or w (for consistency chk only) */
    {
#ifdef RVM_USELWP
    if (mode == r) ReleaseReadLock(&rwl->mutex);
    else           ReleaseWriteLock(&rwl->mutex);
#else
    rw_qentry_t      *q,*old_q;         /* thread queue elements */

    CRITICAL(rwl->mutex,                /* begin rw_lock mutex crit sec */
        {
        assert((mode == r) || (mode == w));
        assert(rwl->lock_mode == mode);
        assert(rwl->read_cnt >= 0);
        assert((rwl->write_cnt >= 0) && (rwl->write_cnt <= 1));
        assert((rwl->write_cnt > 0) ? (rwl->read_cnt == 0) : 1);
        assert((rwl->read_cnt > 0) ? (rwl->write_cnt == 0) : 1);

        /* update lock counts */
        if (rwl->lock_mode == r)
            rwl->read_cnt--;
        else rwl->write_cnt--;

        /* clear lock mode if lock free */
        if ((rwl->write_cnt == 0) && (rwl->read_cnt == 0))
            rwl->lock_mode = f;

        /* see if any threads should be signaled */
        if (LIST_NOT_EMPTY(rwl->queue))
            {
            /* wake up single writer iff current readers done */
            q = (rw_qentry_t *)rwl->queue.nextentry;
            if (q->mode == w)
                {
                if (rwl->lock_mode == f)
                    {                   /* wake up writer */
                    q = (rw_qentry_t *)
                        move_list_entry(&rwl->queue,NULL,NULL);
                    rwl->write_cnt++;
                    rwl->lock_mode = w;
                    condition_signal(&q->wait);
                    }
                else
                    assert((rwl->lock_mode==r) && (rwl->write_cnt==0));
                }
            else
                do  /* wake up all readers before next writer */
                    {
                    old_q = q;          /* save entry ptr */
                    q = (rw_qentry_t *)q->links.nextentry;

                    old_q = (rw_qentry_t *)
                            move_list_entry(&rwl->queue,NULL,NULL);
                    rwl->read_cnt++;
                    assert(rwl->lock_mode != w);
                    rwl->lock_mode = r;
                    condition_signal(&old_q->wait);
                    }
                while (!(q->links.is_hdr || (q->mode == w)));
            }
        });                             /* end rw_lock mutex crit sec */
#endif
    }
/*  binary tree functions
    all functions leave locking to caller
    lookup requires a comparator function with signature:
            int cmp(target,test)
                rvm_offset_t *target;    node to locate
                rvm_offset_t *test;      node to test against

            returns:    1           target > test
                        0           target = test
                       -1           target < test
*/
/* traversal vector initializer */
static void chk_traverse(tree)
    tree_root_t     *tree;
    {
    if (tree->traverse_len <= (tree->max_depth+1))
        {
        tree->traverse_len += TRAVERSE_LEN_INCR;
        if (tree->traverse != NULL)
            free((char *)tree->traverse);
        tree->traverse=(tree_pos_t *)
            malloc(tree->traverse_len*sizeof(tree_pos_t));
        if (tree->traverse == NULL)
            assert(rvm_false);
        }
    }

#define SET_TRAVERSE(tr,pt,st) \
    (tr)->traverse[++(tr)->level].ptr = (tree_node_t *)(pt); \
    (tr)->traverse[(tr)->level].state = (st)

/* tree root initialization */
void init_tree_root(root)
    tree_root_t     *root;
    {
    root->struct_id = tree_root_id;
    root->root = NULL;
    root->traverse = NULL;
    root->traverse_len = 0;
    root->n_nodes = 0;
    root->max_depth = 0;
    root->unlink = rvm_false;
    }

/* tree root finalizer */
void clear_tree_root(root)
    tree_root_t     *root;
    {

    assert(root->struct_id == tree_root_id);
    if (root->traverse != NULL)
        {
        free(root->traverse);
        root->traverse = NULL;
        root->traverse_len = 0;
        }
    }
/* balance checker */
static int get_depth(node,n_nodes)
    tree_node_t     *node;
    long            *n_nodes;
    {
    int             lss_depth = 1;      /* init to 1 for this node */
    int             gtr_depth = 1;

    if (node == NULL) return 0;
    assert((node->bf >= -1) && (node->bf <= 1));

    if (n_nodes != NULL) (*n_nodes)++;
    lss_depth += get_depth(node->lss,n_nodes);
    gtr_depth += get_depth(node->gtr,n_nodes);

    assert(node->bf == (gtr_depth - lss_depth)); /* check balance factor */
    assert((node->bf >= -1) && (node->bf <= 1));

    if (gtr_depth > lss_depth)          /* return depth of deeper side */
        return gtr_depth;
    else 
        return lss_depth;
    }

#ifdef UNUSED_FUNCTIONS
/* Guaranteed to return 0, for now */
static int chk_balance(tree)
    tree_root_t     *tree;              /* ptr to root of tree */
    {
    long            n_nodes = 0;
    get_depth(tree->root,&n_nodes);

    assert(n_nodes == tree->n_nodes);
    return 0;
    }
#endif

/* binary tree lookup -- returns ptr to node found (or NULL) */
tree_node_t *tree_lookup(tree,node,cmp)
    tree_root_t     *tree;              /* root of tree */
    tree_node_t     *node;              /* node w/ range to lookup */
    cmp_func_t      *cmp;               /* ptr to comparator function */
    {
    tree_node_t     *cur;               /* current search node */
    tree_node_t     *par = NULL;        /* parent of cur */

    assert(tree->struct_id == tree_root_id);
    cur = tree->root;
    while (cur != NULL)                 /* search */
        {
        assert(cur != par);             /* loop detector */
        par = cur;
        switch ((*cmp)(node,cur))
            {
          case -1:                      /* lss */
            cur = cur->lss;
            break;
          case 0:                       /* match */
            return cur;
          case 1:                       /* gtr */
            cur = cur->gtr;
            break;
          default:  assert(rvm_false);
            }
        }

    return NULL;                        /* not found */
    }
/* insertion rotation function */
static void insert_rotate(tree,bal_pnt,bal_pnt_par,sub_root,new_bf)
    tree_root_t     *tree;              /* ptr to root of tree */
    tree_node_t     *bal_pnt;           /* balance point */
    tree_node_t     *bal_pnt_par;       /* parent of bal_pnt */
    tree_node_t     *sub_root;          /* root of unbalanced sub-tree */
    long            new_bf;             /* new balance factor */
    {
    tree_node_t     *new_bal_pnt = sub_root; /* new balance node */

    assert(tree->struct_id == tree_root_id);
    if (new_bf == 1)
        {
        /* right heavy */
        if (sub_root->bf == 1)
            {
            /* rotate RR */
            bal_pnt->gtr = sub_root->lss;
            sub_root->lss = bal_pnt;
            bal_pnt->bf = sub_root->bf = 0;
            }
        else
            {
            /* RL rotations */
            new_bal_pnt = sub_root->lss;
            sub_root->lss = new_bal_pnt->gtr;
            bal_pnt->gtr = new_bal_pnt->lss;
            new_bal_pnt->gtr = sub_root;
            new_bal_pnt->lss = bal_pnt;
                
            /* adjust balance factors */
            switch (new_bal_pnt->bf)
                {
              case 0:   bal_pnt->bf = sub_root->bf = 0;
                        break;
              case 1:   bal_pnt->bf = -1; sub_root->bf = 0;
                        break;
              case -1:  bal_pnt->bf = 0; sub_root->bf = 1;
                        break;
              default:  assert(rvm_false);
                }
            new_bal_pnt->bf = 0;
            }
        }
    else
        {
        /* left heavy */
        if (sub_root->bf == -1)
            {
            /* rotate LL */
            bal_pnt->lss = sub_root->gtr;
            sub_root->gtr = bal_pnt;
            bal_pnt->bf = sub_root->bf = 0;
            }
        else
            {
            /* LR rotations */
            new_bal_pnt = sub_root->gtr;
            sub_root->gtr = new_bal_pnt->lss;
            bal_pnt->lss = new_bal_pnt->gtr;
            new_bal_pnt->lss = sub_root;
            new_bal_pnt->gtr = bal_pnt;
                
            /* adjust balance factors */
            switch (new_bal_pnt->bf)
                {
              case 0:   bal_pnt->bf = sub_root->bf = 0;
                        break;
              case 1:   bal_pnt->bf = 0; sub_root->bf = -1;
                        break;
              case -1:  bal_pnt->bf = 1; sub_root->bf = 0;
                        break;
              default:  assert(rvm_false);
                }
            new_bal_pnt->bf = 0;
            }
        }

    /* complete rotation by re-inserting balanced sub-tree */
    if (bal_pnt_par == NULL)
        tree->root = new_bal_pnt;
    else
        if (bal_pnt == bal_pnt_par->gtr)
            bal_pnt_par->gtr = new_bal_pnt;
        else
            if (bal_pnt == bal_pnt_par->lss)
                bal_pnt_par->lss = new_bal_pnt;
    }
/* binary tree insertion - traverse vector left suitable for 
   successor iterator */
rvm_bool_t tree_insert(tree,node,cmp)
    tree_root_t     *tree;              /* ptr to root of tree */
    tree_node_t     *node;              /* node to insert */
    cmp_func_t      *cmp;               /* comparator */
    {
    tree_node_t     *cur;               /* current search node */
    tree_node_t     *par = NULL;        /* parent of cur */
    tree_node_t     *sub_root;          /* root of unbalanced sub-tree */
    tree_node_t     *last_unbal;        /* last seen unbalanced node */
    tree_node_t     *last_unbal_par = NULL; /* parent of last unbalanced node */
    int             val=0;              /* last comparison value */
    long            new_bf;             /* new balance factor */

    assert(tree->struct_id == tree_root_id);
    chk_traverse(tree);
    node->gtr = node->lss = NULL;
    node->bf = 0;
    if (tree->root == NULL)
        {
        tree->root = node;              /* insert node into empty tree */
        tree->n_nodes = tree->max_depth = 1;
        return rvm_true;
        }

    /* search for insertion point */
    tree->level = -1;
    /* tree->root cannot be null */
    cur = last_unbal = tree->root;
    assert(cur != NULL);
    while (cur != NULL)
        {
        /* retain most recent unbalanced node and parent */
        if (cur->bf != 0)
            {
            last_unbal = cur;
            last_unbal_par = par;
            }

        /* compare for insertion point */
        assert((cur->bf >= -1) && (cur->bf <= 1));
        par = cur;
        switch (val=(*cmp)(node,cur))
            {
          case -1:  SET_TRAVERSE(tree,cur,lss); /* lss */
                    cur = cur->lss; break;
          case 0:   SET_TRAVERSE(tree,cur,self);/* match; leave ptr to node */
                    return rvm_false;
          case 1:   SET_TRAVERSE(tree,NULL,gtr); /* gtr */
                    cur = cur->gtr; break;
          default:  assert(rvm_false);
            }
        }
    /* insert node */
    if (val == 1)
        par->gtr = node;
    else
        par->lss = node;
    tree->n_nodes++;

    /* determine side of possible imbalance and set new balance factor */
    if ((new_bf=(*cmp)(node,last_unbal)) == 1)
        sub_root = last_unbal->gtr;
    else
        sub_root = last_unbal->lss;

    /* set balance factors of sub-tree, all of which must have been 0 */
    cur = sub_root;
    while (cur != node)
        {
        assert(cur->bf == 0);
        if ((cur->bf=(*cmp)(node,cur)) == 1)
            cur = cur->gtr;
        else
            cur = cur->lss;
        }

    /* set balance factor of first unbalanced node; check for imbalance */
    if (last_unbal->bf == 0)
        {
        last_unbal->bf = new_bf;
        tree->level++;
        }
    else
        if ((last_unbal->bf+new_bf) == 0)
            last_unbal->bf = 0;
        else                            /* tree unbalanced: rotate */
            insert_rotate(tree,last_unbal,last_unbal_par,
                          sub_root,new_bf);

    /* record maximum depth */
    if ((tree->level+1) > tree->max_depth)
        tree->max_depth = tree->level+1;

    return rvm_true;
    }
/* deletion rotation function */
static rvm_bool_t delete_rotate(tree,bal_pnt,bal_pnt_par,sub_root,new_bf)
    tree_root_t     *tree;              /* ptr to root of tree */
    tree_node_t     *bal_pnt;           /* balance point */
    tree_node_t     *bal_pnt_par;       /* parent of bal_pnt */
    tree_node_t     *sub_root;          /* root of unbalanced sub-tree */
    long            new_bf;             /* new balance factor */
    {
    tree_node_t     *new_bal_pnt = sub_root; /* new balance point */
    long            old_sub_root_bf = sub_root->bf;

    assert(tree->struct_id == tree_root_id);
    if (new_bf == 1)
        {                               /* right heavy */
        if ((sub_root->bf == 1)
            || ((sub_root->bf == 0) && (sub_root->lss->bf == -1)))
            {                           /* RR rotations */
            bal_pnt->gtr = sub_root->lss;
            sub_root->lss = bal_pnt;
            if (sub_root->bf == 1)
                bal_pnt->bf = sub_root->bf = 0;
            else
                {
                bal_pnt->bf = 1;
                sub_root->bf = -1;
                }
            }
        else
            {                           /* RL rotations */
            new_bal_pnt = sub_root->lss;
            sub_root->lss = new_bal_pnt->gtr;
            bal_pnt->gtr = new_bal_pnt->lss;
            new_bal_pnt->gtr = sub_root;
            new_bal_pnt->lss = bal_pnt;

            /* adjust balance factors */
            switch (new_bal_pnt->bf)
                {
              case 0:   bal_pnt->bf = 0; sub_root->bf++;
                        break;
              case 1:   bal_pnt->bf = -1; sub_root->bf++;
                        break;
              case -1:  bal_pnt->bf = 0; sub_root->bf = 1;
                        break;
              default:  assert(rvm_false);
                }
            if (old_sub_root_bf == 0) new_bal_pnt->bf = 1;
            else new_bal_pnt->bf = 0;
            }
        }
    else
        {                               /* left heavy */
        if ((sub_root->bf == -1)
            || ((sub_root->bf == 0) && (sub_root->gtr->bf == 1)))
            {                           /* LL rotations */
            bal_pnt->lss = sub_root->gtr;
            sub_root->gtr = bal_pnt;
            if (sub_root->bf == -1)
                bal_pnt->bf = sub_root->bf = 0;
            else
                {
                bal_pnt->bf = -1;
                sub_root->bf = 1;
                }
            }
        else
            {                           /* LR rotations */
            new_bal_pnt = sub_root->gtr;
            sub_root->gtr = new_bal_pnt->lss;
            bal_pnt->lss = new_bal_pnt->gtr;
            new_bal_pnt->lss = sub_root;
            new_bal_pnt->gtr = bal_pnt;
                
            /* adjust balance factors */
            switch (new_bal_pnt->bf)
                {
              case 0:   bal_pnt->bf = 0; sub_root->bf--;
                        break;
              case 1:   bal_pnt->bf = 0; sub_root->bf = -1;
                        break;
              case -1:  bal_pnt->bf = 1; sub_root->bf--;
                        break;
              default:  assert(rvm_false);
                }
            if (old_sub_root_bf == 0) new_bal_pnt->bf = -1;
            else new_bal_pnt->bf = 0;
            }
        }
    /* complete rotation by re-inserting balanced sub-tree */
    if (bal_pnt_par == NULL)
        tree->root = new_bal_pnt;
    else
        if (bal_pnt == bal_pnt_par->gtr)
            bal_pnt_par->gtr = new_bal_pnt;
        else
            if (bal_pnt == bal_pnt_par->lss)
                bal_pnt_par->lss = new_bal_pnt;

    /* return true if depth changed */
    if (new_bal_pnt->bf == 0)
        return rvm_true;
    return rvm_false;
    }
/* binary tree deletion -- does not free the node
   traverse vector not left suitable for iterators */
rvm_bool_t tree_delete(tree,node,cmp)
    tree_root_t     *tree;              /* ptr to root of tree */
    tree_node_t     *node;              /* node to delete */
    cmp_func_t      *cmp;               /* comparator */
    {
    tree_node_t     *cur;               /* current search node */
    tree_node_t     *sub_root=NULL;     /* unbalanced sub tree root */
    tree_node_t     *bal_pnt_par;       /* parent of balance point */
    tree_node_t     *old_root = tree->root; /* save state of old root */
    long            old_root_bf = tree->root->bf;
    int             node_level;         /* level at which node found */
    long            new_bf=0;           /* new balance factor */

    /* search for target node */
    assert(tree->struct_id == tree_root_id);
    chk_traverse(tree);
    tree->level = -1;
    cur = tree->root;
    while (cur != NULL)
        {
        /* determine branch to follow */
        assert((cur->bf >= -1) && (cur->bf <= 1));
        switch ((*cmp)(node,cur))
            {
          case -1:                      /* lss */
            SET_TRAVERSE(tree,cur,lss);
            cur = cur->lss;
            break;
          case 0:
            SET_TRAVERSE(tree,cur,self);
            if (cur == node) goto delete; /* located */
            assert(rvm_false);          /* multiple entries ?!?! */
          case 1:                       /* gtr */
            SET_TRAVERSE(tree,cur,gtr);
            cur = cur->gtr;
            break;
          default:  assert(rvm_false);
            }
        }

        return rvm_false;               /* not found */
    /* see if simple delete: node has <= 1 child */
  delete:
    tree->n_nodes--;
    node_level = tree->level;
    if (node->lss == NULL)
        {
        cur = node->gtr;
        tree->traverse[tree->level].state = gtr;
        }
    else if (node->gtr == NULL)
        {
        cur = node->lss;
        tree->traverse[tree->level].state = lss;
        }
    else
        {
        /* must select replacement node - use deeper side if possible,
           otherwise choose alternately with depth */
        if ((new_bf = node->bf) == 0)
            {
            new_bf = tree->level & 1;
            if (new_bf == 0) new_bf = -1;
            }
        if (new_bf == 1)
            {
            cur = node->gtr;            /* locate successor */
            tree->traverse[tree->level].state = gtr;
            }
        else
            {
            cur = node->lss;            /* locate predecessor */
            tree->traverse[tree->level].state = lss;
            }
        while (cur != NULL)
            {
            assert((cur->bf >= -1) && (cur->bf <= 1));
            if (new_bf == 1)
                {
                SET_TRAVERSE(tree,cur,lss);
                cur = cur->lss;
                }
            else
                {
                SET_TRAVERSE(tree,cur,gtr);
                cur = cur->gtr;
                }
            }
        /* unlink selected node */
        if (tree->level == 0)
            {
            cur = tree->root;
            if (new_bf == 1)
                tree->root = cur->gtr;
            else
                tree->root = cur->lss;
            }
        else
            {
            if (new_bf == 1)
                cur = tree->traverse[tree->level].ptr->gtr;
            else
                cur = tree->traverse[tree->level].ptr->lss;
            if (tree->traverse[tree->level-1].state == lss)
                tree->traverse[tree->level-1].ptr->lss = cur;
            else
                tree->traverse[tree->level-1].ptr->gtr = cur;
            cur = tree->traverse[tree->level].ptr;
            }

        /* update selected node's state with target state */
        cur->bf = node->bf;
        cur->gtr = node->gtr;
        cur->lss = node->lss;
        }

    /* delete target node */
    if (node_level == 0)
        tree->root = cur;
    else
        if (tree->traverse[node_level-1].state == lss)
            tree->traverse[node_level-1].ptr->lss = cur;
        else
            tree->traverse[node_level-1].ptr->gtr = cur;
    tree->traverse[node_level].ptr = cur;
    /* rebalance as necessary up path */
    while (--tree->level >= 0)
        {
        switch (tree->traverse[tree->level].state)
            {
          case lss:
            new_bf = 1;
            sub_root = tree->traverse[tree->level].ptr->gtr;
            break;
          case gtr:
            new_bf = -1;
            sub_root = tree->traverse[tree->level].ptr->lss;
            break;
          case self:
          default:      assert(rvm_false);
            }

        /* if tree balanced at this point, set new factor and quit */
        if (tree->traverse[tree->level].ptr->bf == 0)
            {
            tree->traverse[tree->level].ptr->bf = new_bf;
            break;
            }
        if ((tree->traverse[tree->level].ptr->bf+new_bf) == 0)
            {
            tree->traverse[tree->level].ptr->bf = 0;
            continue;
            }

        /* must rotate to balance */
        if (tree->level == 0)
            bal_pnt_par = NULL;
        else
            bal_pnt_par = tree->traverse[tree->level-1].ptr;
        if (!delete_rotate(tree,tree->traverse[tree->level].ptr,
                           bal_pnt_par,sub_root,new_bf))
            break;                      /* done, depth didn't change */
        }

    /* adjust maximum height */
    if ((tree->root == NULL)
        || ((old_root == tree->root) && (old_root_bf != 0)
            && (tree->root->bf == 0))
        || ((old_root != tree->root) && (tree->root->bf == 0)))
        tree->max_depth--;

    return rvm_true;
    }
/* forward order iteration generator: balance not maintained if nodes unlinked */
tree_node_t *tree_successor(tree)
    tree_root_t     *tree;              /* ptr to tree root descriptor */
    {
    tree_node_t     *cur;               /* current search node */

    /* determine how to continue */
    assert(tree->struct_id == tree_root_id);

    DO_FOREVER
        {
        cur = tree->traverse[tree->level].ptr;
        if (cur != NULL)
            assert((cur->bf >= -1) && (cur->bf <= 1));
        switch (tree->traverse[tree->level].state)
            {
          case gtr:
            if (cur == NULL)
                {
                if (--tree->level < 0)
                    return NULL;
                continue;
                }
          case lss:
            tree->traverse[tree->level].state = self;
            tree->traverse[tree->level].ptr = cur->gtr;
            goto unlink;
          case self:
            tree->traverse[tree->level].state = gtr;
            if (cur == NULL) continue;
            if (cur->lss != NULL) break;
            tree->traverse[tree->level].ptr = cur->gtr;
            goto unlink;
          case init:
            assert(tree->level == 0);
            tree->traverse[0].state = lss;
            break;
          default:  assert(rvm_false);
            }

        /* locate successor */
        while ((cur=cur->lss) != NULL)
            {
            assert((cur->bf >= -1) && (cur->bf <= 1));
            SET_TRAVERSE(tree,cur,lss);
            }
        }
    /* set next traverse node ptr */
  unlink:
    assert(cur != NULL);
    if (tree->unlink)
        {
        tree->n_nodes--;
        if (tree->level == 0)
            tree->root = cur->gtr;
        else
            tree->traverse[tree->level-1].ptr->lss = cur->gtr;
        assert(cur->lss == NULL);
        }

    assert((cur->bf >= -1) && (cur->bf <= 1));
    return cur;
    }
/* reverse order iterator generator: balance not maintained if nodes unlinked */
tree_node_t *tree_predecessor(tree)
    tree_root_t     *tree;              /* ptr to tree root descriptor */
    {
    tree_node_t     *cur;               /* current search node */

    /* determine how to continue */
    assert(tree->struct_id == tree_root_id);

    DO_FOREVER
        {
        cur = tree->traverse[tree->level].ptr;
        if (cur != NULL)
            assert((cur->bf >= -1) && (cur->bf <= 1));
        switch (tree->traverse[tree->level].state)
            {
          case lss:
            if (cur == NULL)
                {
                if (--tree->level < 0)
                    return NULL;
                continue;
                }
          case gtr:
            tree->traverse[tree->level].state = self;
            tree->traverse[tree->level].ptr = cur->lss;
            goto unlink;
          case self:
            tree->traverse[tree->level].state = lss;
            if (cur == NULL) continue;
            if (cur->gtr != NULL) break;
            tree->traverse[tree->level].ptr = cur->lss;
            goto unlink;
          case init:
            assert(tree->level == 0);
            tree->traverse[0].state = gtr;
            break;
          default:  assert(rvm_false);
            }

        /* locate predecessor */
        while ((cur=cur->gtr) != NULL)
            {
            assert((cur->bf >= -1) && (cur->bf <= 1));
            SET_TRAVERSE(tree,cur,gtr);
            }
        }
    /* set next traverse node ptr */
  unlink:
    assert(cur != NULL);
    if (tree->unlink)
        {
        tree->n_nodes--;
        if (tree->level == 0)
            tree->root = cur->lss;
        else
            tree->traverse[tree->level-1].ptr->gtr = cur->lss;
        assert(cur->gtr == NULL);
        }

    assert((cur->bf >= -1) && (cur->bf <= 1));
    return cur;
    }
/* tree iteration initializers */
tree_node_t *init_tree_generator(tree,direction,unlink)
    tree_root_t     *tree;              /* ptr to tree root descriptor */
    rvm_bool_t      direction;          /* FORWARD ==> lss -> gtr */
    rvm_bool_t      unlink;             /* unlink nodes from tree if true */
    {

    assert(tree->struct_id == tree_root_id);
    tree->unlink = unlink;
    tree->level = -1;
    if (tree->root == NULL) return NULL;
    chk_traverse(tree);
    SET_TRAVERSE(tree,tree->root,init);

    if (direction == FORWARD)
        return tree_successor(tree);
    else
        return tree_predecessor(tree);
    }
/* initilizer for iteration after insertion failure */
tree_node_t *tree_iterate_insert(tree,node,cmp)
    tree_root_t     *tree;              /* ptr to root of tree */
    tree_node_t     *node;              /* node to insert */
    cmp_func_t      *cmp;               /* comparator */
    {
    tree_node_t     *cur;               /* current search node */
    int             first_level;        /* level of smallest node */

    /* try to insert node */
    assert(tree->struct_id == tree_root_id);
    tree->unlink = rvm_false;
    if (tree_insert(tree,node,cmp))
        return NULL;                    /* done, no iteration required */

    /* collision, locate smallest conflicting node */
    first_level = tree->level;
    cur = tree->traverse[tree->level].ptr->lss;
    tree->traverse[tree->level].state = lss;
    while (cur != NULL)
        switch ((*cmp)(cur,node))
            {
          case -1:  SET_TRAVERSE(tree,NULL,gtr);
                    cur = cur->gtr;
                    break;
          case 0:   SET_TRAVERSE(tree,cur,lss);
                    first_level = tree->level;
                    cur = cur->lss;
                    break;
          case 1:                       /* shouldn't happen */
          default:  assert(rvm_false); 
            }

    /* return smallest conflicting node */
    tree->level = first_level;
    cur = tree->traverse[tree->level].ptr;
    tree->traverse[tree->level].ptr = cur->gtr;
    tree->traverse[tree->level].state = self;

    return cur;
    }
/* histogram data gathering function */
void enter_histogram(val,histo,histo_def,length)
    long            val;                /* value to log */
    long            *histo;             /* histogram data */
    long            *histo_def;         /* histogram bucket sizes */
    long            length;             /* length of histogram vectors */
    {
    long            i;

    /* increment proper bucket */
    for (i=0; i<length-1; i++)
        if (val <= histo_def[i])
            {
            histo[i]++;
            return;
            }

    histo[length-1]++;                  /* outsized */
    return;
    }
/* The following functions are needed only on machines without 64-bit
   integer operations and are used only within macros defined in rvm.h
*/
/* rvm_offset_t constructor */
rvm_offset_t rvm_mk_offset(x,y)
    rvm_length_t        x;
    rvm_length_t        y;
    {
    rvm_offset_t        tmp;

    tmp.high = x;
    tmp.low = y;

    return tmp;
    }

/* add rvm_length to rvm_offset; return (offset + length) */
rvm_offset_t rvm_add_length_to_offset(offset,length)
    rvm_offset_t    *offset;            /* ptr to offset */
    rvm_length_t    length;               
    {
    rvm_offset_t    tmp;

    tmp.high = offset->high;
    tmp.low = offset->low + length;
    if (tmp.low < offset->low)          /* test for overflow */
        tmp.high ++;                    /* do carry */

    return tmp;
    }

/* subtract rvm_length from rvm_offset; return (offset - length)) */
rvm_offset_t rvm_sub_length_from_offset(offset,length)
    rvm_offset_t    *offset;            /* ptr to offset */
    rvm_length_t    length;             /* length to subtract */
    {
    rvm_offset_t    tmp;

    tmp.high = offset->high;
    tmp.low = offset->low - length;
    if (tmp.low > offset->low)          /* test for underflow */
        tmp.high --;                    /* do borrow */

    return tmp;
    }
/* add rvm_offset to rvm_offset; return (x+y) */
rvm_offset_t rvm_add_offsets(x,y)
    rvm_offset_t    *x,*y;              /* operand ptrs */
    {
    rvm_offset_t    tmp;

    tmp.high = x->high + y->high;       /* add high order bits */
    tmp.low = x->low + y->low;          /* add low order bits */
    if (tmp.low < x->low)               /* test for overflow */
        tmp.high ++;                    /* do carry */

    return tmp;
    }

/* subtract rvm_offset from rvm_offset; return (x-y) */
rvm_offset_t rvm_sub_offsets(x,y)
    rvm_offset_t    *x,*y;              /* operand ptrs */
    {
    rvm_offset_t    tmp;

    tmp.high = x->high - y->high;       /* subtract high order bits */
    tmp.low = x->low - y->low;          /* subtract low order bits */
    if (tmp.low > x->low)               /* test for underflow */
        tmp.high --;                    /* do borrow */


    return tmp;
    }
/* page rounding functions for rvm_offset; return offset rounded up/down
   to page boundrary: used only for rvm.h macro support */
rvm_offset_t rvm_rnd_offset_up_to_page(x)
    rvm_offset_t    *x;                 /* operand ptr */
    {
    rvm_offset_t    tmp;

    tmp = rvm_add_length_to_offset(x,page_size-1);
    tmp.low = tmp.low & page_mask;

    return tmp;
    }

rvm_offset_t rvm_rnd_offset_dn_to_page(x)
    rvm_offset_t    *x;                 /* operand ptr */
    {
    rvm_offset_t    tmp;

    tmp.high = x->high;
    tmp.low = x->low & page_mask;

    return tmp;
    }

/* page size, mask export functions: used only for rvm.h macros */
rvm_length_t rvm_page_size()
    {
    return page_size;
    }

rvm_length_t rvm_page_mask()
    {
    return page_mask;
    }

/* round offset to sector size support */
rvm_offset_t rvm_rnd_offset_to_sector(x)
    rvm_offset_t    *x;
    {
    rvm_offset_t    tmp;

    tmp = RVM_ADD_LENGTH_TO_OFFSET((*x),SECTOR_SIZE-1);
    tmp.low = tmp.low & (SECTOR_MASK);

    return tmp;
    }
