#ifndef _BLURB_
#define _BLURB_
/*

            Coda: an Experimental Distributed File System
                             Release 4.0

          Copyright (c) 1987-1996 Carnegie Mellon University
                         All Rights Reserved

Permission  to  use, copy, modify and distribute this software and its
documentation is hereby granted,  provided  that  both  the  copyright
notice  and  this  permission  notice  appear  in  all  copies  of the
software, derivative works or  modified  versions,  and  any  portions
thereof, and that both notices appear in supporting documentation, and
that credit is given to Carnegie Mellon University  in  all  documents
and publicity pertaining to direct or indirect use of this code or its
derivatives.

CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
ANY DERIVATIVE WORK.

Carnegie  Mellon  encourages  users  of  this  software  to return any
improvements or extensions that  they  make,  and  to  grant  Carnegie
Mellon the rights to redistribute these changes without encumbrance.
*/

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/coda-src/volutil/vol-rcvheap.h,v 1.1.1.1 1996/11/22 19:13:37 rvb Exp";
#endif /*_BLURB_*/






/* vol-rcvheap.h
 * Created March 1990
 */

/* Copied from Camelot sources to track down camelot 
 * recoverable storage corruption 
 */
#ifndef _RCV_HEAP_PROCESSED_
#define _RCV_HEAP_PROCESSED_ 1

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <cam/camelot_types.h>
#include <cam/c_args.h>

#ifdef __cplusplus
}
#endif __cplusplus



/*
 * Types used by rcv_malloc.c
 */

/* 
 * Structure of memory block header.
 * When free, next points to next block on free list.
 * When allocated, fl points to free list.
 * Size of header is 4 bytes, so minimum usable block size is 8 bytes.
 */

typedef union rcv_heap_header {
    union rcv_heap_header      *next;
    struct rcv_heap_free_list  *fl;
} rcv_heap_header_t;

typedef struct rcv_heap_end_block_header {
    union rcv_heap_header      *flag;
    int				remaining;
} rcv_heap_end_block_header_t;

typedef struct rcv_heap_free_list {
  /* 
   * We might want to mutex on each size some time to increase
   * concurrency.
   */
  rcv_heap_header_t    *headPtr;
#ifdef INSERT_DEBUG_CODE
  int			in_use;
#endif INSERT_DEBUG_CODE
} rcv_heap_free_list_t; /* Pointer to head of free list for this size */


/*
 * Types used by rcv_heap_table.h (Hash table operations.)
 */

/*
 * The size is some random prime number.
 */
#define RCV_HEAP_TABLE_SIZE	67

#define RCV_HEAP_TID_HASH(tid)	(CAM_TID_HASH(tid) % RCV_HEAP_TABLE_SIZE)

typedef struct rcv_heap_oper_struct {
  int				kind;
  char			       *address;
  struct rcv_heap_oper_struct  *next;
} rcv_heap_oper;

typedef struct rcv_heap_tid_record_struct {
  cam_tid_t          	              tid;
  boolean_t             	      commitment;
  struct rcv_heap_tid_record_struct  *next;
  struct rcv_heap_tid_record_struct  *prev;
  rcv_heap_oper                	     *opers;
} rcv_heap_tid_record;

typedef struct rcv_heap_tid_entry_struct {
  cam_tid_t			      key;
  rcv_heap_tid_record		      contents;
  struct rcv_heap_tid_entry_struct  *next;
} rcv_heap_tid_entry;

/* 
 * Here is the structure of the actual hash table.
 */
typedef struct rcv_heap_table_struct {
  /* 
   * This is a hack so that we can modify the entire array at once. 
   */
  struct rcv_heap_tbl_struct {
      rcv_heap_tid_entry *data[RCV_HEAP_TABLE_SIZE];
  }			tbl;
  int			historicalReasons1;	/* not used anymore */
  rcv_heap_tid_entry   *historicalReasons2;	/* not used anymore */
} rcv_heap_table;


/* 
 * Number of buckets in the free list table.
 */
#define NBUCKETS	29

/*
 * This struct constitutes the entire header for the recoverable heap.
 * It is place in the internal portion of the recoverable segment of
 * all Camlib servers.
 */ 
typedef struct {
    rcv_heap_free_list_t	free_list[NBUCKETS];
    rcv_heap_table		table;
    char		       *last_used;
} rcv_heap_t;

#endif
