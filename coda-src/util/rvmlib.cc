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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/util/Attic/rvmlib.cc,v 4.2 1997/02/26 16:03:08 rvb Exp $";
#endif /*_BLURB_*/







#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#ifdef __MACH__
#include <libc.h>
#endif /* __MACH__ */
#ifdef __BSD44__
#include <stdlib.h>
#endif /* __BSD44__ */

#include <setjmp.h>
#include <stdio.h>
#include <rds.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include "util.h"
#include "rvmlib.h"

rvm_type_t RvmType = UNSET;     /* What kind of persistence are we relying on? */

void rvmlib_internal_abort(char *errmsg) {
    fprintf(stderr, errmsg);
    abort();
}


void *rvmlib_internal_malloc(int size, int abortonerr) {
#ifdef	__linux__
  int err;
  void *p;
#endif
    rvm_perthread_t *_rvm_data = RVM_THREAD_DATA;
    if (_rvm_data == 0) rvmlib_internal_abort("RecMalloc: _rvm_data = 0");

#ifdef	__linux__
    err = 0;
    p = rds_malloc(size, _rvm_data->tid, &err);
#else
    int err = 0;
    void *p = rds_malloc(size, _rvm_data->tid, &err);
#endif
    if (err != 0) {
	if (abortonerr) {
	    if (_rvm_data->tid == 0) return(0);	    /* should we call rvmlib_internal_abort()? */
	    else RVMLIB_ABORT((rvm_return_t)err)
	}
	else assert(0);
    }
    return(p);
}


void rvmlib_internal_free(void *p, int abortonerr) {
#ifdef	__linux__
  int err;  
#endif
    rvm_perthread_t *_rvm_data = RVM_THREAD_DATA;
    if (_rvm_data == 0) rvmlib_internal_abort("RecFree: _rvm_data = 0");

#ifdef	__linux__
    err = 0;
#else
    int err = 0;
#endif
    err = rds_fake_free((char *)p, &(_rvm_data->list));
    if (err != 0) {
	if (abortonerr) {
	    if (_rvm_data->tid == 0) return;            /* should we call rvmlib_internal_abort()? */
	    else RVMLIB_ABORT((rvm_return_t)err)
	}
	else assert(0);
    }
}

rvm_perthread_t *rvmlib_internal_thread_data() {
    rvm_perthread_t *data = 0;
    int lwprc = LWP_GetRock(RVM_THREAD_DATA_ROCK_TAG, (char **)&data);
    if (lwprc != LWP_SUCCESS)
	/*rvmlib_internal_abort("thread_data: LWP_GetRock failed");*/return(0);
    return(data);
}
void rvmlib_internal_set_thread_data(void *p) {
    int lwprc = LWP_NewRock(RVM_THREAD_DATA_ROCK_TAG, (char *)p);
    if (lwprc != LWP_SUCCESS) abort();
}
