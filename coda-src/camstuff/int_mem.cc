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

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/coda-src/camstuff/int_mem.cc,v 1.1.1.1 1996/11/22 19:09:06 rvb Exp";
#endif /*_BLURB_*/






#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#ifdef CAMELOT
#include <camlib/camlib.h>
#include <cam/cam_er_error.h>
#endif CAMELOT
#include <stdarg.h>
#include <libc.h>
#include <rvm.h>
#include <rds.h>
#include "camlib_modules.h"

#ifdef __cplusplus
}
#endif __cplusplus

#include "int_mem.h"
#include "util.h"

#define	MIN_SIZE 8

extern RvmType RvmMechanism;
extern char *_Rvm_Data_device;

int MallocCheck = 1;
#ifdef CAMELOT
extern char *camlib_rcv_malloc(int size);
extern void camlib_rcv_free(char *p);
extern rcv_heap_free_list_t *rcv_heap_free_list;
#endif CAMELOT

char *internal_malloc(int size, char *file, int line)
{
  LogMsg(9, VolDebugLevel, stdout, "Malloc called file %s line %d.\n", file, line);

  switch (RvmMechanism) {
    case Camelot: {
#ifdef CAMELOT
      rcv_heap_header_t *rhp;
      mem_header_t mytemplate;

      if (MallocCheck){
	  char *p = camlib_rcv_malloc(size + sizeof(mem_header_t));
	  rhp = (rcv_heap_header_t *)(p - sizeof(union rcv_heap_header));
	  /* fill up memory header */
	  mem_header_t *mhp = (mem_header_t *)p;
	  p += sizeof(mem_header_t);
	  int i = 0;
	  int n = MIN_SIZE;
	  while (n < (size + sizeof(mem_header_t) + sizeof(union rcv_heap_header))){
	      i += 1;
	      n <<= 1;
	  }

	  if (mhp->flag == ALLOC_STAMP){
	      CAM_ER_PRINT((msg, "Camelot allocated block (0x%x) previously allocated at line %d file %s\n",
			    p, mhp->line, mhp->file));
	      assert(0);
	  }
/*
	  CAM_ER_PRINT((msg, "Setting mem header for block 0x%x \n stamp = %d, size = %d\nfile = %s line = %d", 
			p, ALLOC_STAMP, n, file, line));
*/
	  /* since size is always > 8 we can ignore the 3 lower bits;
	   we use it for the alloc/free flag */
	  mytemplate.size = n >> 3;
	  mytemplate.flag = ALLOC_STAMP;
	  assert(rhp->fl == &rcv_heap_free_list[i]);
	  if (file[0] == '.' && file[1] == '.' && file[2] == '/')
	      strncpy(mytemplate.file, &file[3], 3);
	  else
	      strncpy(mytemplate.file, file, 3);
	  strncpy(&mytemplate.file[3], &file[strlen(file)-3], 3);
	  mytemplate.line = line;
	  MODIFY(*mhp, mytemplate);
	  return(p);
      }
      return(camlib_rcv_malloc(size));
#else CAMELOT
      assert(0);
#endif CAMELOT
    }
    case NoPersistence :
      return((char *)malloc(size));

    case Rvm : {
      rvm_tid_t *tid = 0;  /* For now rds_malloc doesn't use a tid... */
      int err;
      char *addr = rds_malloc(size, tid, &err);

      if (addr == (char *)NULL) {
	  if ((int)err > 0) 
	      printf("RVM_ERROR %s in rds_malloc\n", rvm_return((rvm_return_t) err));
	  else
	      printf("ERROR in rds_malloc %d\n", err);
	  assert(0);
      }

      /* When I use rvm_malloc_tid above, I will need to rvm_free_tid(tid); */
      return addr;
    }
    case NotYetSet:
      assert(0);
  }
}


void internal_free(char *p)
{
    switch (RvmMechanism) {
      case Camelot: {
#ifdef CAMELOT
	if (MallocCheck){
	    /* check stamp & magic number - change stamp */
	    mem_header_t *mhp = (mem_header_t *)(p - sizeof(mem_header_t));
	    rcv_heap_header_t *rhp = (rcv_heap_header_t *)(p - sizeof(mem_header_t) - sizeof(union rcv_heap_header));

	    if (mhp->flag == FREE_STAMP){
		CAM_ER_PRINT((msg, "internal_Free: Trying to free a block twice \n"));
		CAM_ER_PRINT((msg, "Block at 0x%x\n", p));
		assert(0);
	    }
	    if (mhp->flag != ALLOC_STAMP){
		CAM_ER_PRINT((msg, "internal_free: Trying to free block (0x%x) with garbled allocation stamp(0x%x)\n", p, mhp->flag));
		assert(0);
	    }
	    int i = 0;
	    int n = MIN_SIZE;
	    int size = mhp->size << 3;
	    while (n < size) {
		i += 1;
		n <<= 1;
	    }
	    if (n != size){
		CAM_ER_PRINT((msg, "Block at 0x%x has garbled size field (0x%x) \n", p, mhp->size));
		assert(0);
	    }
	    if (rhp->fl != &rcv_heap_free_list[i]){
		CAM_ER_PRINT((msg, "Free List pointer at block 0x%x has been garbled\n",
		       p));
		CAM_ER_PRINT((msg, "It is 0x%x, should be 0x%x\n", rhp->fl, &rcv_heap_free_list[i]));
		assert(0);
	    }
	    mem_header_t mytemplate = *mhp;
	    mytemplate.flag = FREE_STAMP;
	    mytemplate.line = 0;
	    mytemplate.file[0] = '\0';
	    mytemplate.size = 0; 
	    MODIFY(*mhp, mytemplate);

	    p = (char *)mhp;
	}
	camlib_rcv_free(p);
#else CAMELOT
	assert(0);
#endif CAMELOT
	break;
      }

      case NoPersistence:
	free(p);
	break;

      case Rvm: {
	int err;
	rvm_tid_t *tid = 0;
	rds_free(p, tid, &err);

	if (err > SUCCESS) 
	    printf("RVM_ERROR %s in rds_free\n", rvm_return((rvm_return_t) err));
	else if (err < SUCCESS)
	    printf("ERROR in RDS_FREE %d\n", err);

	if (err != SUCCESS)
	    assert(0);
      }
      case NotYetSet:
	assert(0);
    }
}






