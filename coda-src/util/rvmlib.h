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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/util/rvmlib.h,v 4.7 1998/04/14 21:08:37 braam Exp $";
#endif /*_BLURB_*/








#ifndef _RVMLIB_H_
#define _RVMLIB_H_ 1

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <string.h>
#include <rvm.h>
#ifdef __MACH__
#include <sysent.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif

#ifdef	__MACH__
  /* yuck yuck yuck */
#define CMU
#include <setjmp.h>
#undef CMU
  /* yuck yuck yuck */
#else
#include <setjmp.h>
#endif	/* __MACH__ */

#include <rds.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include "util.h"


/*  *****  Types  *****  */

typedef enum {	UNSET =	0,		/* uninitialized */
		RAWIO = 1,		/* raw disk partition */
		UFS = 2,		/* Unix file system */
		VM = 3			/* virtual memory */
} rvm_type_t;

typedef struct {
	rvm_tid_t *tid;
	jmp_buf abort;
	intentionList_t list;
	rvm_tid_t thetid;
	void (*die)(char *arg, ...);
} rvm_perthread_t;


/*  *****  Variables  *****  */

extern rvm_type_t RvmType;		/* your program must supply this! */
extern long rvm_no_yield;		/*  exported by rvm */

/*  ***** Functions  ***** */

extern void rvmlib_internal_abort(char *);
#ifdef	__linux__
extern void *rvmlib_internal_malloc(int, int abortonerr);
extern void rvmlib_internal_free(void *, int abortonerr);
#else
extern void *rvmlib_internal_malloc(int, int abortonerr =1);
extern void rvmlib_internal_free(void *, int abortonerr =1);
#endif
extern void rvmlib_internal_set_thread_data(void *);

#define CODA_STACK_LENGTH 0x20000	/* 128 K */
#define LOGTHRESHOLD	50

#ifdef DJGPP
#define _setjmp setjmp
#define _longjmp longjmp
#include <setjmp.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <lwp.h>

#ifdef __cplusplus
}
#endif __cplusplus


const int RVM_THREAD_DATA_ROCK_TAG = 2001;	/* pointer to rvm_perthread_t must be under this rock! */
extern rvm_perthread_t *rvmlib_internal_thread_data();
extern int optimizationson;

#define	RVM_THREAD_DATA\
    (rvmlib_internal_thread_data())

#define RVM_SET_THREAD_DATA(p)\
    (rvmlib_internal_set_thread_data((p)))

#define	RVMLIB_BEGIN_TRANSACTION(restore_mode)\
{\
    rvm_perthread_t *_rvm_data;\
    rvm_tid_t tid;\
    rvm_return_t _status;\
\
    if (RvmType == RAWIO || RvmType == UFS ) {\
	/* Initialize the rvm_perthread_t object. */\
	_rvm_data = RVM_THREAD_DATA;\
	if (_rvm_data == 0) rvmlib_internal_abort("BeginTransaction: _rvm_data = 0");\
	if (_rvm_data->tid != 0) { \
	   if (_rvm_data->die) \
              (_rvm_data->die)("BeginTransaction: _rvm_data->tid = %x, nested trans file %s line %d",\
						_rvm_data->tid, __FILE__, __LINE__);\
	   else rvmlib_internal_abort("_rvm_data->tid is non zero during begin transaction");\
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

#define	RVMLIB_ABORT(status)\
    rvmlib_abort(status);

#define	RVMLIB_SET_RANGE(base, size)\
    rvmlib_set_range(base, size);

#define	RVMLIB_REC_OBJECT(object)\
    RVMLIB_SET_RANGE(&(object), sizeof(object))\

#define	RVMLIB_REC_MALLOC(size)\
    rvmlib_rec_malloc(size);

#define	RVMLIB_REC_FREE(p)\
    rvmlib_rec_free(p)

#define RVMLIB_IN_TRANSACTION	\
    rvmlib_in_transaction()

inline void rvmlib_abort(int status)
{
    if (RvmType == VM) rvmlib_internal_abort("AbortTransaction: RvmType == VM");
    if (RvmType != RAWIO && RvmType != UFS) { assert(0);}
    rvm_perthread_t *_rvm_data = RVM_THREAD_DATA;
    if (_rvm_data == 0) rvmlib_internal_abort("AbortTransaction: _rvm_data = 0");
    if (_rvm_data->tid == 0) {
	if (_rvm_data->die) { 
	    (_rvm_data->die)("AbortTransaction: _rvm_data->tid = 0");
	}
        else rvmlib_internal_abort("AbortTransaction: _rvm_data->tid is NULL");
    }
    (void)rvm_abort_transaction(_rvm_data->tid);
    if (_rvm_data->list.table != NULL) free(_rvm_data->list.table);
    _rvm_data->list.table = NULL;
    _rvm_data->list.count = _rvm_data->list.size = 0;
    _rvm_data->tid = 0;
    _longjmp(_rvm_data->abort, status);
}

inline void rvmlib_set_range(void *base, unsigned long size){
    if (RvmType != VM) {
	rvm_perthread_t *_rvm_data = RVM_THREAD_DATA;
	if (_rvm_data == 0) rvmlib_internal_abort("SetRange: _rvm_data = 0");
	if (_rvm_data->tid == 0)
	    (_rvm_data->die)("SetRange: _rvm_data->tid = 0");
	rvm_return_t ret = rvm_set_range(_rvm_data->tid, (char *)(base), size);
	if (ret != RVM_SUCCESS) RVMLIB_ABORT(ret)
    }
}

inline void *rvmlib_rec_malloc(unsigned long size){
  switch ( RvmType ) {
  case VM: return malloc(size);
  case RAWIO: 
  case UFS: return  rvmlib_internal_malloc(size, 1);
  default: 
    return NULL;
  }
}

inline void rvmlib_rec_free(void *p)
{
    if (RvmType == VM) free(p);
    else if (RvmType == RAWIO || RvmType == UFS) rvmlib_internal_free(p, 1);
    else assert(0);
}

inline int rvmlib_in_transaction(void) {
    return ((RvmType == RAWIO || RvmType == UFS)
     && ((RVM_THREAD_DATA)->tid != NULL));
}

/* *** Camelot compatibility macros *** */
#ifndef CAMELOT
#define CAMLIB_INITIALIZE_SERVER(initProc, unlisted, name)		    \
void (*dummyprocptr)() = initProc; /* to pacify g++ if initProc is NULL */ \
switch (RvmType) {							    \
    case VM :					       	    \
	if (dummyprocptr != NULL)						    \
	    (*dummyprocptr)();						    \
	camlibRecoverableSegment = (camlib_recoverable_segment *)malloc(sizeof(struct camlib_recoverable_segment));\
       break;                                                               \
	                                                                    \
    case RAWIO :                                                            \
    case UFS : {                                                            \
	rvm_return_t err;						    \
	rvm_options_t *options = rvm_malloc_options();			    \
	struct rlimit stackLimit;					    \
	options->log_dev = _Rvm_Log_Device;				    \
	options->flags = optimizationson; 				    \
	if (prottrunc)							    \
	   options->truncate = 0;					    \
	else if (_Rvm_Truncate > 0 && _Rvm_Truncate < 100) {		    \
	    LogMsg(0, 0, stdout, "Setting Rvm Truncate threshhold to %d.\n", _Rvm_Truncate); \
	    options->truncate = _Rvm_Truncate;				    \
	} 								    \
	sbrk((void *)(0x20000000 - (int)sbrk(0))); /* for garbage reasons. */		    \
	stackLimit.rlim_cur = CODA_STACK_LENGTH;			    \
/*	setrlimit(RLIMIT_STACK, &stackLimit);*/	/* Set stack growth limit */ \
        err = RVM_INIT(options);                   /* Start rvm */           \
        if ( err == RVM_ELOG_VERSION_SKEW ) {                                \
            LogMsg(0, 0, stdout, "rvm_init failed because of skew RVM-log version."); \
            LogMsg(0, 0, stdout, "Coda server not started.");                  \
            exit(-1);                                                          \
	} else if (err != RVM_SUCCESS) {                                     \
	    LogMsg(0, 0, stdout, "rvm_init failed %s",rvm_return(err));	    \
            assert(0);                                                       \
	}                                                                    \
	assert(_Rvm_Data_Device != NULL);	   /* Load in recoverable mem */ \
        rds_load_heap(_Rvm_Data_Device,_Rvm_DataLength,(char **)&camlibRecoverableSegment, (int *)&err);  \
	if (err != RVM_SUCCESS)						    \
	    LogMsg(0, SrvDebugLevel, stdout, "rds_load_heap error %s",rvm_return(err));	    \
	assert(err == RVM_SUCCESS);                                         \
        /* Possibly do recovery on data structures, coalesce, etc */	    \
	rvm_free_options(options);					    \
	if (dummyprocptr != NULL) /* Call user specified init procedure */   \
	    (*dummyprocptr)();						    \
        break;                                                              \
    }                                                                       \
	                                                                    \
    case UNSET:							    \
    default:	                                                            \
	printf("No persistence method selected!\n");			    \
	exit(-1); /* No persistence method selected, so die */		    \
}

#define CAMLIB_BEGIN_TOP_LEVEL_TRANSACTION \
    RVMLIB_BEGIN_TRANSACTION(restore)

#define CAMLIB_BEGIN_TOP_LEVEL_TRANSACTION_2(transType)	\
    RVMLIB_BEGIN_TRANSACTION(restore)

#define CAMLIB_END_TOP_LEVEL_TRANSACTION(status) \
    RVMLIB_END_TRANSACTION(flush, (&(status)))

#define CAMLIB_END_TOP_LEVEL_TRANSACTION_2(commitProt, status) \
    RVMLIB_END_TRANSACTION(flush, (&(status)))

#define CAMLIB_ABORT(status)\
    RVMLIB_ABORT(status)

#define CAMLIB_ABORT_TOP_LEVEL(status)\
    RVMLIB_ABORT(status)

#define CAMLIB_BEGIN_RECOVERABLE_DECLARATIONS \
struct camlib_recoverable_segment {

#define CAMLIB_END_RECOVERABLE_DECLARATIONS	\
    int	camlibDummy;				\
};

extern struct camlib_recoverable_segment *camlibRecoverableSegment;

#define DEFINE_RECOVERABLE_OBJECTS	\
struct camlib_recoverable_segment *camlibRecoverableSegment;

#define CAMLIB_REC(name) \
    (((struct camlib_recoverable_segment *) (camlibRecoverableSegment))->name)


#define CAMLIB_MODIFY(object, newValue)					    \
do {									    \
    if (RvmType == VM) (object) = (newValue);	    	    		    \
    else if (RvmType == RAWIO || RvmType == UFS) { /* is object a pointer? */		    \
        rvm_return_t ret = rvm_set_range(RVM_THREAD_DATA->tid, (char *)&object, sizeof(object)); \
	if (ret != RVM_SUCCESS)						    \
	    printf("Modify Bytes error %s\n",rvm_return(ret));		    \
        assert(ret == RVM_SUCCESS);					    \
        (object) = (newValue);						    \
    }									    \
    else {								    \
       assert(0);							    \
    }								    	    \
} while(0)

#define CAMLIB_MODIFY_BYTES(objectPtr, newValuePtr, length)		    \
do {									    \
    if (RvmType == VM) 				    	    		    \
	(void) bcopy((char *) (newValuePtr), (char *) (objectPtr), (int) (length));\
    else if (RvmType == RAWIO || RvmType == UFS) {			    \
        rvm_return_t ret =						    \
	    rvm_modify_bytes(RVM_THREAD_DATA->tid, (char *)objectPtr,	    \
			     (char *)newValuePtr, (int)length);             \
	if (ret != RVM_SUCCESS)						    \
	    printf("Modify Bytes error for %x, %s\n",objectPtr, rvm_return(ret));\
        assert(ret == RVM_SUCCESS);					    \
    }									    \
    else {								    \
        assert(0);							    \
    }									    \
}while(0)


#define CAMLIB_REC_MALLOC(size)\
    (RvmType == VM ? malloc(size) : \
     ((RvmType == RAWIO) || (RvmType == UFS)) ? rvmlib_internal_malloc(size, 0) : \
     NULL)
    

#define CAMLIB_REC_FREE(p) \
    RVMLIB_REC_FREE(p)

#define CAMLIB_LOCK(name, nameSpace, mode) 
#define CAMLIB_TRY_LOCK(name, nameSpace, mode) 
#define CAMLIB_UNLOCK(name, nameSpace) 	
#define CAMLIB_LOCK_NAME(object) 

#define CAMLIB_IN_RVM_TRANSACTION	\
        RVMLIB_IN_TRANSACTION

#endif CAMELOT

#endif	_RVMLIB_H_
