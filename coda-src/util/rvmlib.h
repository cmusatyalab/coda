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
#endif /*_BLURB_*/


#ifndef _RVMLIB_H_
#define _RVMLIB_H_ 1

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <string.h>
#include <rvm.h>
#include <unistd.h>
#include <stdlib.h>

#include <setjmp.h>
#include <lwp.h>

#include <rds.h>
#include <util.h>

#ifdef __cplusplus
}
#endif __cplusplus



/*  *****  Types  *****  */

typedef enum {	UNSET =	0,		/* uninitialized */
		RAWIO = 1,		/* raw disk partition */
		UFS = 2,		/* Unix file system */
		VM = 3			/* virtual memory */
} rvm_type_t;

typedef struct {
	rvm_tid_t *tid;
	rvm_tid_t tids;
	jmp_buf abort;
	intentionList_t list;
} rvm_perthread_t;


/*  *****  Variables  *****  */

extern rvm_type_t RvmType;	 /* your program must supply this! */
extern long rvm_no_yield;	 /*  exported by rvm */

/*  ***** Functions  ***** */



#ifdef DJGPP
#define _setjmp setjmp
#define _longjmp longjmp
#include <setjmp.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

int rvmlib_in_transaction(void);
void rvmlib_assert(char *message);
void rvmlib_abort(int);

void rvmlib_set_range(void *base, unsigned long size);
void rvmlib_modify_bytes(void *dest, const void *newval, int len);

void *rvmlib_malloc(unsigned long size, char *file, int line);
void rvmlib_free(void *p, char *file, int line);

void rvmlib_init_threaddata(rvm_perthread_t *rvmptt);
extern void rvmlib_set_thread_data(void *);
rvm_perthread_t *rvmlib_thread_data(void);

void rvmlib_begin_transaction(int restore_mode);
void rvmlib_end_transaction(int flush_mode, rvm_return_t *statusp);


#ifdef __cplusplus
}
#endif __cplusplus


#define CODA_STACK_LENGTH 0x20000	/* 128 K */
#define LOGTHRESHOLD	50

/* pointer to rvm_perthread_t must be under this rock! */
extern int optimizationson;


#define rvmlib_rec_malloc(size) rvmlib_malloc(size, __FILE__, __LINE__)
#define rvmlib_rec_free(addr) rvmlib_free(addr, __FILE__, __LINE__)


#ifdef OLDTRANS

#define	RVMLIB_BEGIN_TRANSACTION(restore_mode)\
{\
    rvm_perthread_t *_rvm_data;\
    rvm_tid_t tid;\
    rvm_return_t _status;\
\
    if (RvmType == RAWIO || RvmType == UFS ) {\
	/* Initialize the rvm_perthread_t object. */\
	_rvm_data = rvmlib_thread_data();\
	if (_rvm_data == 0) rvmlib_assert("BeginTransaction: _rvm_data = 0");\
	if (_rvm_data->tid != 0) { \
	   if (_rvm_data->die) \
              (_rvm_data->die)("BeginTransaction: _rvm_data->tid = %x, nested trans file %s line %d",\
						_rvm_data->tid, __FILE__, __LINE__);\
	   else rvmlib_assert("_rvm_data->tid is non zero during begin transaction");\
	   }\
	rvm_init_tid(&tid);\
	_rvm_data->tid = &tid;\
	_rvm_data->list.table = NULL;\
	_rvm_data->list.count = 0;\
	_rvm_data->list.size = 0;\
\
        /* I am skipping the protected truncate bit for now */	\
\
	/* Begin the transaction. */\
	_status = rvm_begin_transaction(_rvm_data->tid, (restore_mode));\
	if (_status == RVM_SUCCESS)\
	    _status = (rvm_return_t)_setjmp(_rvm_data->abort);\
    }\
    else if (RvmType == VM) {\
	_status = RVM_SUCCESS;\
    }\
    else {\
        assert(0);\
    }\
\
    if (_status == 0/*RVM_SUCCESS*/) {\
	/* User code goes in this block. */

#define	RVMLIB_END_TRANSACTION(flush_mode, statusp)\
	/* User code goes in this block. */\
    }\
\
    if (RvmType == RAWIO || RvmType == UFS) {\
	/* End the transaction. */\
	if (_status == 0/*RVM_SUCCESS*/) {\
	   if (flush_mode == no_flush) {\
		_status = rvm_end_transaction(_rvm_data->tid, flush_mode);\
                if ((_status == RVM_SUCCESS) && (_rvm_data->list.table != NULL))\
                _status = (rvm_return_t)rds_do_free(&_rvm_data->list, flush_mode);\
		}\
           else {\
              /* flush mode */\
              if (_rvm_data->list.table != NULL) {\
		_status = rvm_end_transaction(_rvm_data->tid, no_flush);\
                if (_status == RVM_SUCCESS) \
                _status = (rvm_return_t)rds_do_free(&_rvm_data->list, flush);\
	      }\
              else \
                 _status = rvm_end_transaction(_rvm_data->tid, flush);\
           }\
	}\
	if (statusp)\
	    *(statusp) = _status;\
	else {\
	    if (_status != RVM_SUCCESS) (_rvm_data->die)("EndTransaction: _status = %d", _status);\
	}\
\
	/* De-initialize the rvm_perthread_t object. */\
	_rvm_data->tid = 0;\
        if (_rvm_data->list.table) free(_rvm_data->list.table);\
    }\
    else if (RvmType == VM) {\
	if (statusp)\
	    *(statusp) = RVM_SUCCESS;\
    }\
    else {\
       assert(0);\
    }\
}


#else

#define RVMLIB_BEGIN_TRANSACTION(restore_mode) rvmlib_begin_transaction(restore_mode);

#define	RVMLIB_END_TRANSACTION(flush_mode, statusp) rvmlib_end_transaction(flush_mode, (rvm_return_t *) statusp);

#endif

#define RVMLIB_REC_OBJECT(object) rvmlib_set_range(&(object), sizeof(object))


inline void rvmlib_check_trans(char *where, char *file);
#define rvmlib_intrans()  rvmlib_check_trans(__FUNCTION__, __FILE__)


/* *** Camelot compatibility macros *** */

#define SRV_RVM(name) \
    (((struct camlib_recoverable_segment *) (camlibRecoverableSegment))->name)

#define RVMLIB_MODIFY(object, newValue)					    \
do {									    \
    rvm_perthread_t *_rvm_data = rvmlib_thread_data();\
    if (RvmType == VM) (object) = (newValue);	    	    		    \
    else if (RvmType == RAWIO || RvmType == UFS) { /* is object a pointer? */		    \
        rvm_return_t ret = rvm_set_range(_rvm_data->tid, (char *)&object, sizeof(object)); \
	if (ret != RVM_SUCCESS)						    \
	    printf("Modify Bytes error %s\n",rvm_return(ret));		    \
        assert(ret == RVM_SUCCESS);					    \
        (object) = (newValue);						    \
    }									    \
    else {								    \
       assert(0);							    \
    }								    	    \
} while(0)




#endif	_RVMLIB_H_
