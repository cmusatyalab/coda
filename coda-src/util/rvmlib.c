/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/







#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdlib.h>

#include <setjmp.h>
#include <stdio.h>
#include <rvm/rds.h>
#include "util.h"
#include "rvmlib.h"

#ifdef __cplusplus
}
#endif __cplusplus

const int RVM_THREAD_DATA_ROCK_TAG = 2001;	
rvm_type_t RvmType = UNSET;     /* What kind of persistence are we relying on? */

void rvmlib_init_threaddata(rvm_perthread_t *rvmptt)
{
	if (RvmType == RAWIO || RvmType == UFS) {
		rvmptt->tid = NULL;
		rvmptt->list.table = NULL;
		rvmptt->list.count = 0;
		rvmptt->list.size = 0;
		rvmlib_set_thread_data(rvmptt);
		CODA_ASSERT(rvmlib_thread_data() != 0);
	}
}

rvm_perthread_t *rvmlib_thread_data() 
{
	rvm_perthread_t *data = 0;
	int lwprc = LWP_GetRock(RVM_THREAD_DATA_ROCK_TAG, (char **)&data);
	if (lwprc != LWP_SUCCESS)
	/*RVMLIB_ASSERT("thread_data: LWP_GetRock failed");*/
		return(0);
	return(data);
}

void rvmlib_set_thread_data(void *p) {
    int lwprc = LWP_NewRock(RVM_THREAD_DATA_ROCK_TAG, (char *)p);
    if (lwprc != LWP_SUCCESS) 
	    abort();
}
void rvmlib_begin_transaction(restore_mode)
{
    rvm_perthread_t *_rvm_data;
    rvm_return_t _status;

    switch (RvmType) {
    case RAWIO:
    case UFS:
	    /* Initialize the rvm_perthread_t object. */
	    _rvm_data = rvmlib_thread_data();
	    if (_rvm_data == 0) 
		    RVMLIB_ASSERT("BeginTransaction: _rvm_data = 0");
	    if (_rvm_data->tid != 0) {
		    RVMLIB_ASSERT("_rvm_data->tid is non zero during begin transaction");
	    }
	    rvm_init_tid(&(_rvm_data->tids));
	    _rvm_data->tid = &(_rvm_data->tids);
	    _rvm_data->list.table = NULL;
	    _rvm_data->list.count = 0;
	    _rvm_data->list.size = 0;
	    /* Begin the transaction. */

	    _status = rvm_begin_transaction(_rvm_data->tid, restore_mode);
	    if (_status != RVM_SUCCESS)
		    CODA_ASSERT(0);
	    break;
    case VM:
	    _status = RVM_SUCCESS;
	    break;
    default:
        CODA_ASSERT(0);
    }
}

void rvmlib_end_transaction(int flush_mode, rvm_return_t *statusp)
{
	rvm_perthread_t *_rvm_data;
	rvm_return_t _status = RVM_SUCCESS;

	if (RvmType == VM) {
		if (statusp)
			*(statusp) = RVM_SUCCESS;
		return;
	}

	if (RvmType != RAWIO && RvmType != UFS)
		CODA_ASSERT(0);

	_rvm_data = rvmlib_thread_data();
	if (_rvm_data == 0) 
		RVMLIB_ASSERT("rvmlib_end_transaction: _rvm_data = 0");

	/* UFS or RAWIO case */
	if (flush_mode == no_flush) {
		_status = rvm_end_transaction(_rvm_data->tid, no_flush);
		CODA_ASSERT(_status == 0);
		if (_rvm_data->list.table != NULL)
			_status = rds_do_free(&_rvm_data->list, no_flush);
		CODA_ASSERT(_status == 0);
	} else {/* flush mode */
		if (_rvm_data->list.table != NULL) {
			_status = rvm_end_transaction(_rvm_data->tid, no_flush);
			CODA_ASSERT(_status == 0);
			_status = (rvm_return_t)rds_do_free(&_rvm_data->list, flush);
			CODA_ASSERT(_status == 0);
		} else
			_status = rvm_end_transaction(_rvm_data->tid, flush);
			CODA_ASSERT(_status == 0);
	}


	if (statusp)
		*(statusp) = _status;

	/* De-initialize the rvm_perthread_t object. */
	_rvm_data->tid = 0;
        if (_rvm_data->list.table) 
		free(_rvm_data->list.table);
}

void rvmlib_abort(int status)
{
	rvm_return_t err;

	rvm_perthread_t *_rvm_data = rvmlib_thread_data();
	if (RvmType == VM) 
		RVMLIB_ASSERT("AbortTransaction: RvmType == VM");
	if (RvmType != RAWIO && RvmType != UFS) 
		CODA_ASSERT(0);
	if (_rvm_data == 0) 
		RVMLIB_ASSERT("AbortTransaction: _rvm_data = 0");
	if (_rvm_data->tid == 0) {
		RVMLIB_ASSERT("AbortTransaction: _rvm_data->tid is NULL");
	}
	err = rvm_abort_transaction(_rvm_data->tid);
	if ( err != RVM_SUCCESS ) 
		RVMLIB_ASSERT("Error aborting transaction.");
	if (_rvm_data->list.table != NULL) 
		free(_rvm_data->list.table);
	_rvm_data->list.table = NULL;
	_rvm_data->list.count = 0;
	_rvm_data->list.size = 0;
	_rvm_data->tid = 0;
}

void rvmlib_set_range(void *base, unsigned long size)
{
	if (RvmType != VM) {
		rvm_perthread_t *_rvm_data = rvmlib_thread_data();
		rvm_return_t ret;
		
		if (_rvm_data == 0) 
			RVMLIB_ASSERT("SetRange: _rvm_data = 0");
		if (_rvm_data->tid == 0 )
			RVMLIB_ASSERT("SetRange: _rvm_data->tid = 0");
		ret = rvm_set_range(_rvm_data->tid, (char *)(base), size);
		if (ret != RVM_SUCCESS) 
			RVMLIB_ASSERT("Error in rvm_set_range\n");
	}
}

void rvmlib_modify_bytes(void *dest, const void *newval, int len)
{
	rvmlib_set_range(dest, len);
	bcopy(newval, dest, len);
}

inline void *rvmlib_malloc(unsigned long size, char *file, int line)
{
	int err;
	void *p;
	rvm_perthread_t *_rvm_data; 

	switch (RvmType) {
	case VM:
		return malloc(size);
	case RAWIO :
	case UFS:
		_rvm_data = rvmlib_thread_data();
		if (_rvm_data == 0) 
			RVMLIB_ASSERT("RecMalloc: _rvm_data = 0");

		err = 0;
		p = rds_malloc(size, _rvm_data->tid, &err);
		
		if (err != 0) 
			RVMLIB_ASSERT("error in rvmlib_malloc\n");
		RDS_LOG("rdstrace: rec_malloc addr %x size %lx file %s line %d\n",
			p, size, file, line);
		return p;
	default :
		return NULL;
	}
}

inline void rvmlib_free(void *p, char *file, int line)
{
	int err = 0;  
	rvm_perthread_t *_rvm_data;

	switch (RvmType) {
	case VM:
		free(p);
		break;
	case RAWIO:
        case UFS:
		_rvm_data = rvmlib_thread_data();
		if (_rvm_data == 0) 
			RVMLIB_ASSERT("RecFree: _rvm_data = 0");
		err = rds_fake_free((char *)p, &(_rvm_data->list));
		if ( err != RVM_SUCCESS ) 
			RVMLIB_ASSERT("Error in rvmlib_free\n");
		RDS_LOG("rdstrace: rec_free addr %x file %s line %d\n",
			p, file, line);
		break; 
	default:
		CODA_ASSERT(0);
	}
}

inline void rvmlib_check_trans(char *where, char *file)

{
	if ( ! rvmlib_in_transaction() ) {
		fprintf(stderr, "Aborting: no transaction in %s (%s)!\n", where, file);
		fflush(stderr);
		abort();
	}
}


int rvmlib_in_transaction(void) 
{
    if (RvmType != RAWIO && RvmType != UFS) return(0);

    return ((rvmlib_thread_data())->tid != NULL);
}
