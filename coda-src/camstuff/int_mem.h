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

static char *rcsid = "$Header: blurb.doc,v 1.1 96/11/22 13:29:31 raiff Exp $";
#endif /*_BLURB_*/






/*
 *
 * int_mem.h 
 * 	Created 04/18/90
 * 	Puneet Kumar
 */

#ifndef _INT_MEM_H_
#define _INT_MEM_H_
/* hacks to detect memory trashing of free lists and allocated objects */

#define	MIN_SIZE 8

#ifdef CAMELOT
extern char *camlib_rcv_malloc(int size);
extern void camlib_rcv_free(char *p);

typedef union rcv_heap_header {
    union rcv_heap_header      *next;
    struct rcv_heap_free_list  *fl;
} rcv_heap_header_t;

typedef struct rcv_heap_free_list {
  /* 
   * We might want to mutex on each size some time to increase
   * concurrency.
   */
  rcv_heap_header_t    *headPtr;
} rcv_heap_free_list_t; /* Pointer to head of free list for this size */

extern rcv_heap_free_list_t *rcv_heap_free_list;
#endif CAMELOT

#define	ALLOC_STAMP	7
#define FREE_STAMP	2
typedef struct mem_header_t{
	unsigned flag:3;	/* allocated or free */
	unsigned size:29;	/* size of the block allocated  (right shifted by 3) */
	char	file[6];	/* name of file where allocated */
	unsigned short line;	/* line number of file  */
} mem_header_t;

#endif _INT_MEM_H_
