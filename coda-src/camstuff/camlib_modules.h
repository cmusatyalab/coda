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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/camstuff/camlib_modules.h,v 4.1 1997/01/08 21:49:30 rvb Exp $";
#endif /*_BLURB_*/





#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <assert.h>
#include <rvm.h>
#include <rds.h>
#include <sys/resource.h>
#include <setjmp.h>
    
#ifdef __cplusplus
}
#endif __cplusplus

extern  int camlog_fd;
extern  char camlog_record[];
typedef enum {NotYetSet, Camelot, NoPersistence, Rvm} RvmType;
extern RvmType RvmMechanism;
extern long rvm_no_yield;

typedef struct {
	rvm_tid_t *tid;
	jmp_buf   abort;
	intentionList_t list;
	} rvm_perthread_t;
    
#define RVM_THREAD_DATA	((rvm_perthread_t *)cthread_data(cthread_self()))
#define CODA_STACK_LENGTH 0x20000	/* 128 K */
#define LOGTHRESHOLD	50
/*
 *                          
 * 
 * INITIALIZE_SERVER --  Initializes a server, performing recovery if
 * necessary.  Must be called once, before using any other Camelot Library
 * function.
 *
 * Parameters:
 *
 *     initProc -- ({\tt void (*)()}) Procedure (no arguments) called once
 *     when the server is first brought up.  Generally, this procedure will 
 *     initialize the server's recoverable storage.  It runs as a top level
 *     transaction, and we guarantee that it will be executed to completion 
 *     before the server starts operation.  (Optional parameter; may be 
 *     {\tt NULL}.)
 *
 *     unlisted -- ({\tt boolean\_t}) Flag to indicate whether this is an
 *     {\bf unlisted server}.  Unlisted servers do not automatically publicize
 *     their service with the system name server, thus strict control over
 *     access rights can be maintained.
 *
 *     name -- ({\tt char *}) The server's name.  If unlisted is {\tt FALSE},
 *     the service will be publicized under this name.
 *
 * Syntax:
 *
 *	INITIALIZE_SERVER(initProc, unlisted, name);
 *
 */
#ifdef CAMELOT
#define CAMLIB_INITIALIZE_SERVER(initProc, unlisted, name)		    \
switch (RvmMechanism) {							    \
    case Camelot :							    \
	CamlibInitServerInternal((initProc), (unlisted), (name),	    \
		(CAM_PTF_CAMLIB_PREPAREPROC_T) NULL,			    \
	        (CAM_PTF_CAMLIB_COMMITPROC_T) NULL,    			    \
		(CAM_PTF_CAMLIB_ABORTPROC_T) NULL,			    \
		sizeof(struct camlib_recoverable_segment) - sizeof(int));   \
	break;								    \
	                                                                    \
    case NoPersistence :					       	    \
	if ((initProc) != NULL)						    \
	    (*initProc)();						    \
	camlibRecoverableSegment = (camlib_recoverable_segment *)malloc(sizeof(struct camlib_recoverable_segment));\
       break;                                                               \
	                                                                    \
    case Rvm : {                                                            \
	/* i don't think it's necessary to contact the name server... */    \
	rvm_return_t err;						    \
	rvm_options_t *options = rvm_malloc_options();			    \
	struct rlimit stackLimit;					    \
	options->log_dev = _Rvm_Log_Device;				    \
	if (prottrunc)							    \
	   options->truncate = 0;					    \
	else if (_Rvm_Truncate > 0 && _Rvm_Truncate < 100) {		    \
	    LogMsg(0, 0, stdout, "Setting Rvm Truncate threshhold to %d.\n", _Rvm_Truncate); \
	    options->truncate = _Rvm_Truncate;				    \
	} 								    \
	sbrk(0x20000000 - sbrk(0)); /* for garbage reasons. */		    \
	stackLimit.rlim_cur = CODA_STACK_LENGTH;			    \
/*	setrlimit(RLIMIT_STACK, &stackLimit);*/	/* Set stack growth limit */ \
        if ((err = RVM_INIT(options)) != RVM_SUCCESS)	/* Start rvm */	    \
	    LogMsg(0, SrvDebugLevel, stdout, "rvm_init failed %s", rvm_return(err));	    \
        assert(err == RVM_SUCCESS);			  		    \
	assert(_Rvm_Data_Device != NULL);	   /* Load in recoverable mem */ \
        rds_load_heap(_Rvm_Data_Device,_Rvm_DataLength,(char **)&camlibRecoverableSegment, (int *)&err);  \
	if (err != RVM_SUCCESS)						    \
	    LogMsg(0, SrvDebugLevel, stdout, "rds_load_heap error %s",rvm_return(err));	    \
	assert(err == RVM_SUCCESS);                                         \
        /* Possibly do recovery on data structures, coalesce, etc */	    \
	rvm_free_options(options);					    \
	if ((initProc) != NULL)	 /* Call user specified init procedure */   \
	    (*initProc)();						    \
        break;                                                              \
    }                                                                       \
	                                                                    \
    case NotYetSet:							    \
    default:	                                                            \
	printf("No persistence method selected!\n");			    \
	exit(-1); /* No persistence method selected, so die */		    \
}
#else CAMELOT
#define CAMLIB_INITIALIZE_SERVER(initProc, unlisted, name)		    \
switch (RvmMechanism) {							    \
    case Camelot :							    \
	assert(0);							    \
	break;								    \
	                                                                    \
    case NoPersistence :					       	    \
	if ((initProc) != NULL)						    \
	    (*initProc)();						    \
	camlibRecoverableSegment = (camlib_recoverable_segment *)malloc(sizeof(struct camlib_recoverable_segment));\
       break;                                                               \
	                                                                    \
    case Rvm : {                                                            \
	/* i don't think it's necessary to contact the name server... */    \
	rvm_return_t err;						    \
	rvm_options_t *options = rvm_malloc_options();			    \
	struct rlimit stackLimit;					    \
	options->log_dev = _Rvm_Log_Device;				    \
	if (prottrunc)							    \
	   options->truncate = 0;					    \
	else if (_Rvm_Truncate > 0 && _Rvm_Truncate < 100) {		    \
	    LogMsg(0, 0, stdout, "Setting Rvm Truncate threshhold to %d.\n", _Rvm_Truncate); \
	    options->truncate = _Rvm_Truncate;				    \
	} 								    \
	sbrk(0x20000000 - sbrk(0)); /* for garbage reasons. */		    \
	stackLimit.rlim_cur = CODA_STACK_LENGTH;			    \
/*	setrlimit(RLIMIT_STACK, &stackLimit);*/	/* Set stack growth limit */ \
        if ((err = RVM_INIT(options)) != RVM_SUCCESS)	/* Start rvm */	    \
	    LogMsg(0, SrvDebugLevel, stdout, "rvm_init failed %s", rvm_return(err));	    \
        assert(err == RVM_SUCCESS);			  		    \
	assert(_Rvm_Data_Device != NULL);	   /* Load in recoverable mem */ \
        rds_load_heap(_Rvm_Data_Device,_Rvm_DataLength,(char **)&camlibRecoverableSegment, (int *)&err);  \
	if (err != RVM_SUCCESS)						    \
	    LogMsg(0, SrvDebugLevel, stdout, "rds_load_heap error %s",rvm_return(err));	    \
	assert(err == RVM_SUCCESS);                                         \
        /* Possibly do recovery on data structures, coalesce, etc */	    \
	rvm_free_options(options);					    \
	if ((initProc) != NULL)	 /* Call user specified init procedure */   \
	    (*initProc)();						    \
        break;                                                              \
    }                                                                       \
	                                                                    \
    case NotYetSet:							    \
    default:	                                                            \
	printf("No persistence method selected!\n");			    \
	exit(-1); /* No persistence method selected, so die */		    \
}
#endif CAMELOT

/*
 *                          
 * 
 * INITIALIZE_SERVER_2 -- Alternate version of {\tt INITIALIZE\_SERVER} that
 * allows the programmer to provide procedures to be executed upon receipt
 * of certain system messages.
 *
 * Parameters:
 *
 *     initProc -- ({\tt void (*)()}) Procedure (no arguments) called once
 *     when the server is first brought up.  Generally, this procedure will 
 *     initialize the server's recoverable storage.  It runs as a top level
 *     transaction, and we guarantee that it will be executed to completion 
 *     before the server starts operation.  (Optional parameter; may be 
 *     {\tt NULL}.)
 *
 *     unlisted -- ({\tt boolean\_t}) Flag to indicate whether this is an
 *     {\it unlisted} server.  Unlisted servers do not automatically publicize
 *     their service with the system name server, thus strict control over
 *     access rights can be maintained.
 *
 *     name -- ({\tt char *}) The server's name.  If unlisted is {\tt FALSE},
 *     the service will be publicized under this name.
 *
 *     prepareProc -- ({\tt boolean\_t (*)()}) This procedure will be executed
 *     each time a transaction family that called the server is asked to
 *     prepare.  It is passed two arguments, the TID of the top level
 *     transaction in the familly and the serialization timestamp for the
 *     family.  The timestamp will only be meaningful if the family was hybrid
 *     atomic.  Otherwise it will be null.  The prepare proc must return a
 *     boolean value, indicating how the server is to vote in the commit
 *     protocol.  ({\tt TRUE} ==> Commit, {\tt FALSE} ==> Abort.)  (Optional
 *     parameter; may be {\tt NULL}.)
 * 
 *     commitProc -- ({\tt void (*)()}) This procedure is executed after a
 *     family that called the server commits.  It is passed one argument,
 *     the TID of the top level transaction in the family.  No guarantees
 *     are made that the commit proc will be executed if the server crashes.
 *     (Optional parameter; may be {\tt NULL}.)
 * 
 *     abortProc -- ({\tt void (*)()}) Analogous to {\tt CommitProc} for
 *     aborts, except that the abort proc is called on a per transaction
 *     basis rather than a per family basis.  The abort proc is given one
 *     argument, the TID of the aborting transaction.  No guarantees are
 *     made that the abort proc will be executed if the server crashes.
 *    (Optional parameter; may be {\tt NULL}.)
 *
 * Syntax:
 *
 *	INITIALIZE_SERVER_2(initProc, unlisted, name,
 *			    prepareProc, commitProc, abortProc);
 * 
 */
#define CAMLIB_INITIALIZE_SERVER_2(initProc, unlisted, name, prepareProc, commitProc, abortProc)\
    assert(0); /* This isn't used, why bother with it? */


/*
 * START_SERVER -- The heart of the server.  Starts servicing incoming
 * requests.  (Servicing a request consists of receiving it, dispatching
 * it to the appropriate operation procedure, and responding to it.)
 * Note that this procedure does not return.
 *
 * Parameters:
 *
 *     demuxProc -- A function, typically generated by MIG, that checks to
 *     see if a message represents an RPC to this server, and if so, executes
 *     the appropriate operation procedure with the arguments contained in the
 *     message.  The demux proc must return {\tt TRUE} iff the message was
 *     recognized and processed.
 *
 *     maxMpl -- Maximum multiprogramming level for serving requests.  New
 *     threads will be allocated in response to demand, up to this level.
 *     Specifying a maximum multiprogramming level less than two causes a
 *     maximum of two threads to be enforced.
 * 
 *     transTimeOut -- The amount of time that a transaction family is allowed
 *     to remain uncommitted at the server, in seconds.  If this interval
 *     elapses and the family has not committed or aborted, the family will
 *     be aborted by the server.  The interval is measured from the time that
 *     a server call on behalf of the family arrives at the server, or a
 *     new family is created at the server.  Each additional server call on 
 *     behalf of a family causes the family's interval timer to be reset.
 *     A value of zero for {\tt transTimeOut} indicates that no time limit
 *     should be enforced.
 */
#ifdef CAMELOT
void CAMLIB_START_SERVER(
#ifdef	__cplusplus
    CAM_PTF_CAMLIB_START_SERVER_DEMUXPROC_T demuxProc,
    u_int	    maxMpl,
    u_int	    transTimeOut 
#endif
);
#endif CAMELOT

/*
 * TERMINATE_SERVER -- Prints a message in the system log and kills the
 * server.
 *
 * Parameters:
 *
 *      message -- The message to be printed in the system log.
 */
#ifdef CAMELOT
void CAMLIB_TERMINATE_SERVER(
#ifdef	__cplusplus
    char *message 
#endif
);
#endif CAMELOT

/* 
 *                          
 *
 * BEGIN_TRANSACTION, END_TRANSACTION -- These macros delimit a
 * {\tt transaction block}, a sequence of statements that are executed
 * as a transaction.  If this construct occurs within the scope of a
 * transaction, the statements will execute as a subtransaction of the
 * enclosing transaction.  If the construct occurs outside the scope of
 * a transaction, the statements will execute as a new top level transaction
 * with type new value logging, standard.  Syntactically, the construct
 * forms a single code block.
 *
 * Parameters:
 *
 *     status -- {\tt (OUT int)}  Must be a variable in the scope of the
 *     enclosing block.  When the transaction has completed, the variable
 *     will contain a value specifiying the outcome of the transaction.
 *     Status will be 0 if the transaction committed.  Codes between 1 and 
 *     $2^{16}-1$ (inclusive) are voluntary abort codes, and other values are 
 *     system abort codes. 
 *
 * Syntax:
 *
 *      BEGIN_TRANSACTION
 *	    <declarations>
 *
 *          <arbitrary C code>
 *
 *      END_TRANSACTION(status)
 *
 */
#define CAMLIB_BEGIN_TRANSACTION					    \
CAM_BEGIN("TRANSACTION")						    \
    camlib_thread_data_block_t *_threadPtr = CAMLIB_THIS_THREAD_DB_PTR;     \
    camlib_thread_db_trans_state_t _savedTransState;			    \
    int _status;							    \
									    \
    CamlibBeginTrans(&_savedTransState, __FILE__, __LINE__);		    \
									    \
    if ((_status = _setjmp(_threadPtr->current.abort)) == 0)		    \
    {									    \
        /* If new trans is top level, set top level trans info. */	    \
	if (CAM_TID_TOP_LEVEL(_threadPtr->tid))				    \
	    _threadPtr->topLevel = _threadPtr->current;			    \
									    \
        {								    \
            /* User code goes in this block */

#undef CAMLIB_BEGIN_TRANSACTION

#define CAMLIB_END_TRANSACTION(status)					    \
	}								    \
    }									    \
									    \
    (status) = CamlibEndTrans(_status, &_savedTransState);		    \
    CAM_LEAVE;								    \
CAM_END

#undef CAMLIB_END_TRANSACTION


/* 
 *                          
 *
 * BEGIN_TOP_LEVEL_TRANSACTION, END_TOP_LEVEL_TRANSACTION -- These macros
 * delimit a {\bf top level transaction block}, a sequence of statements that
 * execute as a new top level transaction.  The transaction type defaults to
 * new value logging, standard.  The commit protocol defaults to two phased.
 * These defaults can be circumvented by use of the alternate form
 * {\tt BEGIN\_TOP\_LEVEL\_TRANSACTION\_2}.  Syntactically, the construct
 * forms a single code block.
 *
 * Parameters:
 *
 *     status -- {\tt (OUT int)}  Must be a variable in the scope of the
 *     enclosing block.  When the transaction has completed, the variable
 *     will contain a value specifiying the outcome of the transaction.
 *     Status will be 0 if the transaction committed.  Codes between 
 *     1 and $2^{16}-1$ are voluntary abort codes, and other values are
 *     system abort codes.
 *
 * Syntax:
 *
 *      BEGIN_TOP_LEVEL_TRANSACTION
 *	    <declarations>
 *
 *          <arbitrary C code>
 *
 *      END_TOP_LEVEL_TRANSACTION(status)
 *
 */
#define CAMLIB_BEGIN_TOP_LEVEL_TRANSACTION \
    CAMLIB_BEGIN_TOP_LEVEL_TRANSACTION_2(CAM_TRAN_NV_STANDARD)

#define CAMLIB_END_TOP_LEVEL_TRANSACTION(status) \
    CAMLIB_END_TOP_LEVEL_TRANSACTION_2(CAM_PROT_TWO_PHASED, (status))

#define CAMLIB_IN_RVM_TRANSACTION	\
    (RvmMechanism == Rvm && RVM_THREAD_DATA != (rvm_perthread_t *)NULL)

/* 
 *                          
 *
 * BEGIN_TOP_LEVEL_TRANSACTION_2, END_TOP_LEVEL_TRANSACTION_2 -- Alternate
 * form of {\tt BEGIN\_TOP\_LEVEL\_TRANSACTION/END\_TOP\_LEVEL\_TRANSACTION}
 * delimiters, which allows transaction type and commit protocol to be
 * specified. 
 *
 * Parameters:
 *
 *     transType -- {\tt (transaction\_type\_t)} The transaction type.
 *
 *     commitProt -- {\tt (protocol\_type\_t)} The commit protocol.
 *     
 *     status -- {\tt (OUT int)}  Must be a variable in the scope of the
 *     enclosing block.  When the transaction has completed, the variable
 *     will contain a value specifiying the outcome of the transaction.
 *     Status will be 0 if the transaction committed. Codes between 1 and 
 *     $2^{16}-1$ (inclusive) are voluntary abort codes, and other values are 
 *     system abort codes.
 *
 * Syntax:
 *
 *      BEGIN_TOP_LEVEL_TRANSACTION_2(transType)
 *	    <declarations>
 *
 *          <arbitrary C code>
 *
 *      END_TOP_LEVEL_TRANSACTION_2(commitProt, status)
 *
 */
#ifdef CAMELOT
#define CAMLIB_BEGIN_TOP_LEVEL_TRANSACTION_2(transType)			    \
{									    \
    camlib_thread_data_block_t *_threadPtr;				    \
    camlib_thread_db_trans_state_t _savedTransState;			    \
    rvm_perthread_t *_rvm_data;						    \
    int _status;							    \
    extern int prottrunc;						    \
									    \
    if (RvmMechanism == Camelot) {					    \
        _threadPtr = CAMLIB_THIS_THREAD_DB_PTR;			            \
	CamlibBeginTopLevelTrans((transType), &_savedTransState,	    \
                                            __FILE__, __LINE__);	    \
       _status = _setjmp(_threadPtr->current.abort);			    \
    }									    \
    else if (RvmMechanism == Rvm) {					    \
       _rvm_data = (rvm_perthread_t *)malloc(sizeof(rvm_perthread_t));	    \
       _rvm_data->tid = rvm_malloc_tid();				    \
       _rvm_data->list.size = _rvm_data->list.count = 0;		    \
       _rvm_data->list.table = NULL;					    \
       if (RVM_THREAD_DATA != (rvm_perthread_t *)NULL)			    \
	   LogMsg(0, SrvDebugLevel, stdout, "CAMLIB_BEGIN_TRANS is nested!!! file %s line %d",	    \
                                            __FILE__, __LINE__);	    \
       if (prottrunc) {							    \
           rvm_options_t *options = rvm_malloc_options();		    \
	   if (rvm_query(&options, 0) != RVM_SUCCESS) {			    \
	       LogMsg(0, SrvDebugLevel, stdout, "CAMLIB_BEGIN_TRANS: rvm query failed \n");	    \
	       assert(0);						    \
	   }								    \
	   if (options->log_dev_cur > LOGTHRESHOLD) {			    \
	       LogMsg(0, SrvDebugLevel, stdout, "Going to protect stacks and truncate\n");	    \
	       LWP_ProtectStacks();					    \
	       assert(rvm_no_yield);					    \
	       rvm_truncate();						    \
	       LogMsg(0, SrvDebugLevel, stdout, "Finished Truncating - Unprotecting stacks\n");	    \
	       LWP_UnProtectStacks();					    \
	   }								    \
	   if (options->log_dev != 0) { /* XXX hack to prevent leakage */   \
	       free(options->log_dev);					    \
	       options->log_dev = 0;					    \
	   }								    \
	   rvm_free_options(options);					    \
       }								    \
       _status = rvm_begin_transaction(_rvm_data->tid, restore);  	    \
       if (_status == 0) {						    \
	   cthread_set_data(cthread_self(), (char *)_rvm_data);		    \
	   _status = _setjmp(_rvm_data->abort);		    		    \
	   }								    \
    }									    \
    else if (RvmMechanism == NoPersistence) _status = 0;		    \
    if (_status	== 0)							    \
    {									    \
	/* New trans is top level. Set up top level info. */		    \
	if (RvmMechanism == Camelot) {					    \
	    _threadPtr->topLevel = _threadPtr->current;	    		    \
	    }								    \
	else if (RvmMechanism == Rvm) /* do nothing */;			    \
	else if (RvmMechanism == NoPersistence) /* do nothing */;	    \
	{							        
            /* User code goes in this block */
#else CAMELOT
#define CAMLIB_BEGIN_TOP_LEVEL_TRANSACTION_2(transType)			    \
{									    \
    rvm_perthread_t *_rvm_data;						    \
    int _status;							    \
    extern int prottrunc;						    \
									    \
    if (RvmMechanism == Camelot) {					    \
        assert(0);							    \
    }									    \
    else if (RvmMechanism == Rvm) {					    \
       _rvm_data = (rvm_perthread_t *)malloc(sizeof(rvm_perthread_t));	    \
       _rvm_data->tid = rvm_malloc_tid();				    \
       _rvm_data->list.size = _rvm_data->list.count = 0;		    \
       _rvm_data->list.table = NULL;					    \
       if (RVM_THREAD_DATA != (rvm_perthread_t *)NULL)			    \
	   LogMsg(0, SrvDebugLevel, stdout, "CAMLIB_BEGIN_TRANS is nested!!! file %s line %d",	    \
                                            __FILE__, __LINE__);	    \
       if (prottrunc) {							    \
           rvm_options_t *options = rvm_malloc_options();		    \
	   if (rvm_query(&options, 0) != RVM_SUCCESS) {			    \
	       LogMsg(0, SrvDebugLevel, stdout, "CAMLIB_BEGIN_TRANS: rvm query failed \n");	    \
	       assert(0);						    \
	   }								    \
	   if (options->log_dev_cur > LOGTHRESHOLD) {			    \
	       LogMsg(0, SrvDebugLevel, stdout, "Going to protect stacks and truncate\n");	    \
	       LWP_ProtectStacks();					    \
	       assert(rvm_no_yield);					    \
	       rvm_truncate();						    \
	       LogMsg(0, SrvDebugLevel, stdout, "Finished Truncating - Unprotecting stacks\n");	    \
	       LWP_UnProtectStacks();					    \
	   }								    \
	   if (options->log_dev != 0) { /* XXX hack to prevent leakage */   \
	       free(options->log_dev);					    \
	       options->log_dev = 0;					    \
	   }								    \
	   rvm_free_options(options);					    \
       }								    \
       _status = rvm_begin_transaction(_rvm_data->tid, restore);  	    \
       if (_status == 0) {						    \
	   cthread_set_data(cthread_self(), (char *)_rvm_data);		    \
	   _status = _setjmp(_rvm_data->abort);		    		    \
	   }								    \
    }									    \
    else if (RvmMechanism == NoPersistence) _status = 0;		    \
    if (_status	== 0)							    \
    {									    \
	/* New trans is top level. Set up top level info. */		    \
	if (RvmMechanism == Camelot) {					    \
            assert(0);							    \
	    }								    \
	else if (RvmMechanism == Rvm) /* do nothing */;			    \
	else if (RvmMechanism == NoPersistence) /* do nothing */;	    \
	{							        
            /* User code goes in this block */
#endif CAMELOT

#ifdef CAMELOT
#define CAMLIB_END_TOP_LEVEL_TRANSACTION_2(commitProt, status)		    \
	}								    \
    }									    \
									    \
    if (RvmMechanism == Camelot) {					    \
	(status) = CamlibEndTopLevelTrans((commitProt), _status, 	    \
				      &_savedTransState);		    \
    }									    \
    else if (RvmMechanism == Rvm) {					    \
	if (_status == 0) {						    \
	    if (_rvm_data->list.table != NULL) {			    \
		(status) = (int) rvm_end_transaction(_rvm_data->tid, no_flush); \
		if ((status) == RVM_SUCCESS) 				    \
		    (status) = rds_do_free(&_rvm_data->list, flush);	    \
	    } else {							    \
		(status) = (int) rvm_end_transaction(_rvm_data->tid, flush); \
	    }								    \
        }								    \
	else (status) = _status;					    \
	rvm_free_tid(_rvm_data->tid);				            \
	if (_rvm_data->list.table != NULL) free(_rvm_data->list.table);     \
	free(_rvm_data);						    \
	cthread_set_data(cthread_self(), (char *)0x0);			    \
    }   								    \
    else if (RvmMechanism == NoPersistence) {				    \
       status = 0;							    \
       if (camlog_fd){							    \
          if (write(camlog_fd, camlog_record, 512 + 8 + 512) == -1){ \
	     printf("Error in writing camlog file; errno = %d\n", errno);\
	     exit(-1);							    \
	  }								    \
       }								    \
    }									    \
}
#else CAMELOT
#define CAMLIB_END_TOP_LEVEL_TRANSACTION_2(commitProt, status)		    \
	}								    \
    }									    \
									    \
    if (RvmMechanism == Camelot) {					    \
	assert(0);							    \
    }									    \
    else if (RvmMechanism == Rvm) {					    \
	if (_status == 0) {						    \
	    if (_rvm_data->list.table != NULL) {			    \
		(status) = (int) rvm_end_transaction(_rvm_data->tid, no_flush); \
		if ((status) == RVM_SUCCESS) 				    \
		    (status) = rds_do_free(&_rvm_data->list, flush);	    \
	    } else {							    \
		(status) = (int) rvm_end_transaction(_rvm_data->tid, flush); \
	    }								    \
        }								    \
	else (status) = _status;					    \
	rvm_free_tid(_rvm_data->tid);				            \
	if (_rvm_data->list.table != NULL) free(_rvm_data->list.table);     \
	free(_rvm_data);						    \
	cthread_set_data(cthread_self(), (char *)0x0);			    \
    }   								    \
    else if (RvmMechanism == NoPersistence) {				    \
       status = 0;							    \
       if (camlog_fd){							    \
          if (write(camlog_fd, camlog_record, 512 + 8 + 512) == -1){ \
	     printf("Error in writing camlog file; errno = %d\n", errno);\
	     exit(-1);							    \
	  }								    \
       }								    \
    }									    \
}
#endif CAMELOT


/*
 * CONCURRENT_THREAD -- Creates a thread that runs concurrently with the
 * calling thread.  The new thread initially runs outside the scope of a
 * transaction.  (New transactions can be created from the new thread if
 * desired.)  The new thread vanishes when (and if) the procedure to be
 * called returns.
 *
 * Parameters:
 *
 *     proc -- The procedure that runs in the new thread.  May take up to
 *     one argument, of any type that can be passed as a pointer.
 * 
 *     arg -- The argument to be passed to {\tt proc}.  (Ignored if {\tt proc}
 *     has no argument.)  {\tt arg} should not point to an automatic (stack)
 *     object, as such an object can easily be deallocated while the concurrent
 *     thread is still running.
 */
#ifdef CAMELOT
void CAMLIB_CONCURRENT_THREAD(
#ifdef	__cplusplus
    CAM_PTF_CAMLIB_CONCURRENT_THREAD_PROC_T proc,
    any_t 	    arg 
#endif
);
#endif CAMELOT

/*
 *                          
 * 
 * ABORT --  Aborts the innermost nested transaction.  Control will
 * transfer to the statement after the end of the transaction being aborted.
 * Can be invoked anywhere in the scope of a transaction.  (If this
 * procedure is called by a server while it is in the scope of a client
 * transaction, the transaction will be aborted, causing the client to 
 * transfer control.)  The indicated status code will be passed back to the
 * status variable in the {\tt END\_TRANSACTION} delimiter.
 * 
 * Parameters:
 *
 *     status --  ({\tt int}) Status code to be returned.  Must be between
 *     1 and $2^{16}-1$. If it is out of this range, the system abort code
 *     {\tt ACC\_ILLEGAL\_USER\_ABORT\_CODE} will be returned.
 *
 * Syntax:
 *
 *    ABORT(status);
 */
#ifdef CAMELOT
#define CAMLIB_ABORT(status)						    \
switch (RvmMechanism) {							    \
    case NotYetSet : {							    \
	break; /* This should already have been detected. */		    \
       }                                                                    \
	                                                                    \
    case NoPersistence : {					       	    \
       printf("***ERROR - Abort Called in Transaction \n");	     	    \
       exit((status));							    \
       }                                                                    \
	                                                                    \
    case Rvm : {                                                            \
       int _status = (int) rvm_abort_transaction(RVM_THREAD_DATA->tid);	    \
       LogMsg(0, SrvDebugLevel, stdout, "Rvm transaction aborted %d", status);		    \
       _longjmp(RVM_THREAD_DATA->abort, status);			    \
       break;								    \
       }								    \
	                                                                    \
    case Camelot :							    \
	do {								    \
	    int _status = (status);					    \
									    \
	    /* Range check and translate status code */			    \
            _status = ((CAM_ER_GET_CLASS(_status) != 0 || _status == 0)	    \
	       ? CAMLIB_ACC_ILLEGAL_USER_ABORT_CODE			    \
	       : CAM_ER_CLASS_USER_ABORT | _status);			    \
									    \
            CamlibInternalAbort(_status, FALSE, __FILE__, __LINE__);	    \
        } while(0);                                                         \
     }
#else  CAMELOT
#define CAMLIB_ABORT(status)						    \
switch (RvmMechanism) {							    \
    case NotYetSet : {							    \
	break; /* This should already have been detected. */		    \
       }                                                                    \
	                                                                    \
    case NoPersistence : {					       	    \
       printf("***ERROR - Abort Called in Transaction \n");	     	    \
       exit((status));							    \
       }                                                                    \
	                                                                    \
    case Rvm : {                                                            \
       int _status = (int) rvm_abort_transaction(RVM_THREAD_DATA->tid);	    \
       LogMsg(0, SrvDebugLevel, stdout, "Rvm transaction aborted %d", status);		    \
       _longjmp(RVM_THREAD_DATA->abort, status);			    \
       break;								    \
       }								    \
	                                                                    \
    case Camelot :							    \
	assert(0);							    \
     }
#endif CAMELOT

/*
 *                          
 * 
 * ABORT_TOP_LEVEL -- Aborts the enclosing top level transaction. 
 * Transfers control appropriately. Can be invoked anwhere in the
 * scope of a transaction.  The indicated status code will be passed
 * back to the status variable in the {\tt END\_TRANSACTION} delimiter.
 *
 * Parameters:
 *
 *     status --  ({\tt int}) Status code to be returned.  Must be between
 *     1 and $2^{16}-1$. If it is out of this range, the system abort code
 *     {\tt ACC\_ILLEGAL\_USER\_ABORT\_CODE} will be returned.
 *
 * Syntax:
 *
 *    ABORT_TOP_LEVEL(status);
 * 
 */
#ifdef CAMELOT
#define CAMLIB_ABORT_TOP_LEVEL(status)					    \
switch (RvmMechanism) {							    \
    case NotYetSet :							    \
	break; /* This should already have been detected. */ 		    \
	                                                                    \
    case NoPersistence :					       	    \
       printf("***ERROR - Abort Called in Transaction \n");		    \
       exit((status));							    \
       break;								    \
	                                                                    \
    case Rvm :                                                              \
       (status) = (int) rvm_abort_transaction(RVM_THREAD_DATA->tid);	    \
       _longjmp(RVM_THREAD_DATA->abort, status);			    \
       break;								    \
									    \
    case Camelot :							    \
        do {								    \
             int _status = (status);					    \
									    \
            /* Range check and translate status code */			    \
            _status = ((CAM_ER_GET_CLASS(_status) != 0 || _status == 0)	    \
	       ? CAMLIB_ACC_ILLEGAL_USER_ABORT_CODE			    \
	       : CAM_ER_CLASS_USER_ABORT | _status);			    \
									    \
            CamlibInternalAbort(_status, TRUE, __FILE__, __LINE__);	    \
        } while(0);                                                         \
        break;                                                              \
    }
#else CAMELOT
#define CAMLIB_ABORT_TOP_LEVEL(status)					    \
switch (RvmMechanism) {							    \
    case NotYetSet :							    \
	break; /* This should already have been detected. */ 		    \
	                                                                    \
    case NoPersistence :					       	    \
       printf("***ERROR - Abort Called in Transaction \n");		    \
       exit((status));							    \
       break;								    \
	                                                                    \
    case Rvm :                                                              \
       (status) = (int) rvm_abort_transaction(RVM_THREAD_DATA->tid);	    \
       _longjmp(RVM_THREAD_DATA->abort, status);			    \
       break;								    \
									    \
    case Camelot :							    \
	assert(0);							    \
        break;                                                              \
    }
#endif CAMELOT

/*
 * ABORT_NAMED_TRANSACTION -- Aborts the transaction with the given TID.
 * If the transaction is not top level, has already committed, and its top
 * level ancestor hasn't yet committed, its closest uncommitted ancestor
 * is aborted.  Returns {\tt TRUE} if the transaction (or its ancestor) is
 * aborted successfully.  If the top level ancestor has already committed or 
 * aborted, the call has no effect and {\tt FALSE} is returned.  If the named
 * transaction is aborted by the call, the given status will be passed in the 
 * appropriate status variable.  If the named transaction has already
 * committed, and an ancestor must be aborted, the system abort code
 * {\tt AC\_COMMITTED\_CHILD\_ABORTED} will be passed instead.  Note that
 * this call can be used to abort a transaction that did not even run at the
 * calling server or application.  If, however, the caller is running under
 * the named transaction, an immediate transfer of control will occur as if
 * {\tt ABORT} had been called.  Otherwise, transfer of control will occur at
 * the appropriate thread as soon as it trys to perform any Camelot Library
 * operation.
 * 
 * Parameters:
 * 
 *     tid -- The TID of the transaction to be aborted.
 * 
 *     status -- The status code to pass on to the transaction enclosing the 
 *     given transaction.
 */
#ifdef CAMELOT
boolean_t CAMLIB_ABORT_NAMED_TRANSACTION(
#ifdef	__cplusplus
cam_tid_t	tid,
int	 status 
#endif
);
#endif CAMELOT

/* 
 *                          
 *
 * THIS_TID  -- Returns the TID of the transaction under whose scope the
 * calling thread is running, or {\tt CAM\_TID\_NULL} if the calling thread is
 * outside the scope of a transaction.
 *
 * Syntax:
 *
 *	cam_tid_t tid;
 * 
 *      tid = THIS_TID;
 */
#define CAMLIB_THIS_TID (CAMLIB_THIS_THREAD_DB_PTR->tid)

#undef CAMLIB_THIS_TID


/* 
 *                          
 *
 * ABORT_CHECK -- Checks to see if the transaction under which this thread is
 * running has aborted or (in the case of servers) is aborting.  If so,
 * waits for abort to complete (if necessary) and transfers control
 * appropriately.  This function should be called occasionally during long
 * computations that do not use any other Camelot library functions except
 * for REC.  This is especially important for servers, which are subject
 * to be killed by the Transaction Manager if threads continue to operate
 * for long periods of time on behalf of transactions that are in the
 * process of aborting.
 *
 * Syntax:
 *
 *    ABORT_CHECK();
 * 
 */
#define CAMLIB_ABORT_CHECK()	CamlibAbortCheck()

#undef CAMLIB_ABORT_CHECK


/* 
 * ABORT_CODE_TO_STR -- Translates transaction status (as returned by
 * {\tt END\_TRANSACTION}, {\tt WRAP\_SERVER\_CALL}, etc.) into a printable
 * character string.  Typically this function will be used only when it has
 * been determined that the status is a system abort code, as the library
 * cannot produce detailed error messages for user abort codes, which are
 * server/application specific.
 * 
 * Parameters:
 *
 *     status -- Transaction status code.
 *
 * Syntax:
 *
 *     printf("%s\n", ABORT_CODE_TO_STR(status));
 *
 */
char *CAMLIB_ABORT_CODE_TO_STR(
#ifdef	__cplusplus
int status 
#endif
);

/*
 *                          
 *
 * BEGIN_RECOVERABLE_DECLARATIONS, END_RECOVERABLE_DECLARATIONS -- These
 * macros delimit the {\tt recoverable object declaration block}.  This
 * construct should be put in a '.h' file and {\tt \#included} in each file
 * where recoverable objects are used, and in the file containing the call
 * to {\tt INITIALIZE\_SERVER}.  If no recoverable objects are used by a
 * server, a recoverable object declaration block with no C declarations is
 * required.
 * 
 *
 * Syntax:
 *
 *      BEGIN_RECOVERABLE_DECLARATIONS
 *
 *          <Zero or more C declarations>
 *
 *      END_RECOVERABLE_DECLARATIONS
 */
#define CAMLIB_BEGIN_RECOVERABLE_DECLARATIONS \
struct camlib_recoverable_segment {

#define CAMLIB_END_RECOVERABLE_DECLARATIONS	\
    int	camlibDummy;				\
};

extern struct camlib_recoverable_segment *camlibRecoverableSegment;

#define DEFINE_RECOVERABLE_OBJECTS	\
struct camlib_recoverable_segment *camlibRecoverableSegment;


/*
 *                          
 *
 * REC -- Access macro for recoverable objects.  Takes the name of a
 * recoverable object (as declared in the {\tt recoverable object declaration
 * block} and returns the recoverable object itself.
 *
 *
 * Parameters:
 *
 *      name -- Name of the object to be accessed.  Can be all or part of
 *      any object declared with {\tt DECLARE\_RECOVERABLE\_OBJECTS}.
 * 
 * Syntax:
 *
 *      b = REC(a);
 *      c = REC(b[43]);
 *      d = REC(b[27].foo);
 *      ptr = &REC(b[45]);
 */
#define CAMLIB_REC(name) \
    (((struct camlib_recoverable_segment *) (camlibRecoverableSegment))->name)


/*
 *                          
 *
 * MODIFY -- Modifies a recoverable object.  Can be called from anywhere
 * within the scope of a transaction.  Locking of the object is  NOT automatic.
 * Type coercion and compile time type checking are enforced as if the macro
 * call were replaced by the C statement: {$\tt object = newValue;$}  Note
 * that {\tt MODIFY} evaluates its first argument twice, so it should have no
 * side effects.
 * 
 * Parameters:
 *
 *      object -- The recoverable object to be modified.  Must be of a type 
 *	that can be used on the left side of an assignment.  (A syntax error
 *      will result if object is an array, but array elements or structures
 *	are fine.)
 * 
 *	newValue -- The new value to be stored in object.
 *
 * Syntax:
 *
 *      MODIFY(REC(name), newValue);
 *      MODIFY(*ptr, newValue);
 * 
 */
#ifdef CAMELOT
#define CAMLIB_MODIFY(object, newValue)					    \
do {									    \
    char *_ptr;								    \
    cam_regptr_t _optr;							    \
    if (RvmMechanism == Camelot) {					    \
        /* Construct an object pointer to the object to be modified	*/  \
        _ptr = (char *) &(object);					    \
        _optr.segmentId = camlibSegDescList->segmentId;			    \
        _optr.highOffset = CAM_REGPTR_MAX_HIGH_OFFSET;			    \
        _optr.lowOffset = ((u_int) _ptr) - camlibRecSegHigh;		    \
                                                                            \
        CamlibBeforeModify(&_optr, sizeof(object), _ptr, __FILE__, __LINE__);   \
									    \
        (object) = (newValue);						    \
									    \
        CamlibAfterModify(&_optr, sizeof(object), _ptr);		    \
    }									    \
    else if (RvmMechanism == NoPersistence) (object) = (newValue);	    \
    else if (RvmMechanism == Rvm) { /* is object a pointer? */		    \
        rvm_return_t ret = rvm_set_range(RVM_THREAD_DATA->tid, (char *)&object, sizeof(object)); \
	if (ret != RVM_SUCCESS)						    \
	    printf("Modify Bytes error %s\n",rvm_return(ret));		    \
        assert(ret == RVM_SUCCESS);					    \
        (object) = (newValue);						    \
    }									    \
} while(0)
#else CAMELOT
#define CAMLIB_MODIFY(object, newValue)					    \
do {									    \
    if (RvmMechanism == Camelot) {					    \
        assert(0);							    \
    }									    \
    else if (RvmMechanism == NoPersistence) (object) = (newValue);	    \
    else if (RvmMechanism == Rvm) { /* is object a pointer? */		    \
        rvm_return_t ret = rvm_set_range(RVM_THREAD_DATA->tid, (char *)&object, sizeof(object)); \
	if (ret != RVM_SUCCESS)						    \
	    printf("Modify Bytes error %s\n",rvm_return(ret));		    \
        assert(ret == RVM_SUCCESS);					    \
        (object) = (newValue);						    \
    }									    \
} while(0)
#endif CAMELOT

/*
 *                          
 *
 * MODIFY_BYTES -- Modifies a recoverable object, with "byte copy" semantics.
 * (No type checking or type coercion are performed.)
 *
 *
 * Parameters:
 * 
 *      objectPtr -- A pointer to the recoverable object to be modified.  
 * 
 *	newValuePtr -- A pointer to the new value to be stored in object.
 * 
 *      length -- The length, in bytes, of the object to be modified.
 * 
 * Syntax:
 *
 *      MODIFY_BYTES(&REC(name), newValuePtr, sizeof(REC(name)));
 *      MODIFY_BYTES(objectPtr, newValuePtr, length);
 */
#ifdef CAMELOT
#define CAMLIB_MODIFY_BYTES(objectPtr, newValuePtr, length)		    \
do {									    \
    char *_ptr;								    \
    u_int _length;							    \
    cam_regptr_t _optr;							    \
    if(RvmMechanism == Camelot){					    \
	/* Construct an	object pointer to the object to	be modified */	    \
        _ptr = (char *) (objectPtr);					    \
	_optr.segmentId = camlibSegDescList->segmentId;			    \
	_optr.highOffset = CAM_REGPTR_MAX_HIGH_OFFSET;			    \
	_optr.lowOffset = (((u_int) _ptr) - camlibRecSegHigh);		    \
	_length	= (length);						    \
			                                                    \
	CamlibBeforeModify(&_optr, _length, _ptr, __FILE__, __LINE__);	    \
	                                                                    \
	(void) bcopy((char *) (newValuePtr), _ptr, (int) _length);	    \
	                                                                    \
	CamlibAfterModify(&_optr, _length, _ptr);			    \
    }									    \
    else if (RvmMechanism == NoPersistence) 				    \
	(void) bcopy((char *) (newValuePtr), (char *) (objectPtr), (int) (length));\
    else if (RvmMechanism == Rvm) {					    \
        rvm_return_t ret =						    \
	    rvm_modify_bytes(RVM_THREAD_DATA->tid, (char *)objectPtr,	    \
			     (char *)newValuePtr, (int)length);             \
	if (ret != RVM_SUCCESS)						    \
	    printf("Modify Bytes error for %x, %s\n",objectPtr, rvm_return(ret));\
        assert(ret == RVM_SUCCESS);					    \
    }									    \
}while(0)
#else CAMELOT
#define CAMLIB_MODIFY_BYTES(objectPtr, newValuePtr, length)		    \
do {									    \
    if(RvmMechanism == Camelot){					    \
        assert(0);							    \
    }									    \
    else if (RvmMechanism == NoPersistence) 				    \
	(void) bcopy((char *) (newValuePtr), (char *) (objectPtr), (int) (length));\
    else if (RvmMechanism == Rvm) {					    \
        rvm_return_t ret =						    \
	    rvm_modify_bytes(RVM_THREAD_DATA->tid, (char *)objectPtr,	    \
			     (char *)newValuePtr, (int)length);             \
	if (ret != RVM_SUCCESS)						    \
	    printf("Modify Bytes error for %x, %s\n",objectPtr, rvm_return(ret));\
        assert(ret == RVM_SUCCESS);					    \
    }									    \
}while(0)
#endif CAMELOT

/*
 *                          
 *
 * REC_MALLOC -- Recoverable analogue of the C library {\tt malloc} procedure.
 * Returns a pointer to a word-aligned block of memory of the given size 
 * allocated from the reccoverable heap.  Must be called from within the
 * scope of a transaction.  If there is insufficient memory available on
 * the recoverable heap, the enclosing transaction aborts with status
 * {\tt ACC\_REC\_MALLOC\_OUT\_OF\_MEMORY}.  A pointer to the new block
 * should be stored in recoverable memory in the same transaction that does
 * a {\tt REC\_MALLOC}, so that a pointer to the new block will be preserved
 * if and only if the {\tt REC\_MALLOC} commits.
 *
 * 
 * Parameters:
 *
 *      size -- ({\tt int}) The size, in bytes, of the memory block desired.
 * 
 * Syntax:
 *
 *	char *ptr;
 *
 *	ptr = REC_MALLOC(size);
 */
extern char *internal_malloc(int size, char *file, int line);
#define CAMLIB_REC_MALLOC(size) internal_malloc((int)(size), __FILE__, __LINE__)


/*
 *                          
 *
 * REC_FREE -- Recoverable analogue of the C library {\tt free} procedure.
 * Returns a block previously obtained with {\tt REC\_MALLOC} to the storage
 * pool.  This call attempts to verify that its argument is valid, and
 * halts the server with an appropriate error message if an invalid
 * argument is identified.
 * 
 * Parameters:
 *
 *      ptr -- ({\tt char *}) Pointer to a block returned by {\tt REC\_MALLOC}.
 *
 * Syntax:
 *
 *	REC_FREE(ptr);
 */
extern void internal_free(char *p);
#define CAMLIB_REC_FREE(p) 							\
    if (RvmMechanism == Rvm) {							\
	int rvmrc;								\
	rvmrc = rds_fake_free((p), &(RVM_THREAD_DATA->list));			\
	assert(rvmrc  == SUCCESS);						\
    } else 									\
        internal_free((p));


/*
 *                          
 *
 * REC_NEW -- Allocates a new rcoverable object of the given type.  Returns
 * a (typed) pointer to the object.  (Syntactic sugar for {\tt REC\_MALLOC}.)
 *
 * 
 * Parameters:
 *
 *	type -- The type of the new object to be allocated.
 * 
 * Syntax:
 *
 *	foo_t *fp;
 *
 *	fp = REC_NEW(foo_t);
 */
#define CAMLIB_REC_NEW(type) ((type *) camlib_rcv_malloc(sizeof(type)))

#undef CAMLIB_REC_NEW


/*
 *                          
 *
 * PREFETCH -- Causes the page(s) containing a recoverable object to be
 * fetched from disk into main memory, in anticipation of the fact that the
 * object will be read soon.  This has the effect of reducing the latency
 * when the object is read.
 * 
 *
 * Parameters:
 * 
 *    object -- The recoverable object to be fetched from disk.
 * 
 * Syntax:
 * 
 *    PREFETCH(REC(name));
 */
#define CAMLIB_PREFETCH(object)						    \
    CamlibPrefetchInternal((char *) &(object), sizeof(object),		    \
			   __FILE__, __LINE__)

#undef CAMLIB_PREFETCH


/*
 *                          
 *
 * PREFETCH_BYTES -- Causes the page(s) containing the given area in
 * recoverable memory to be fetched from disk into main memory, in
 * anticipation of the fact that the memory will be read soon.  This has
 * the effect of reducing the latency when the memory is read.
 * 
 * Parameters:
 * 
 *    ptr -- A pointer to the area of recoverable memory to be fetched from
 *    disk.
 * 
 *    length -- The length of the area in bytes.
 *
 * Syntax:
 * 
 *    PREFETCH_BYTES(ptr, length);
 */
#define CAMLIB_PREFETCH_BYTES(ptr, length) \
    CamlibPrefetchInternal((char *) (ptr), (length), __FILE__, __LINE__)

#undef CAMLIB_PREFETCH_BYTES


/*
 *                          
 *
 * PREFLUSH -- Causes the page(s) in main memory onto which the given
 * recoverable object is currently mapped to be flushed out to disk.  This
 * frees the memory for other data, which decreases the likelihood that useful
 * data will be paged out.  (This call should only be used when it is known
 * that the given object will not be referenced again in the forseeable
 * future.)
 *
 * Parameters:
 * 
 *    object -- The recoverable object to be flushed out to disk.
 * 
 * Syntax:
 * 
 *    PREFLUSH(REC(name));
 */
#define CAMLIB_PREFLUSH(object)						    \
    CamlibPreflushInternal((char *) &(object), sizeof(object),		    \
			   __FILE__, __LINE__)

#undef CAMLIB_PREFLUSH

/*
 *                          
 *
 * PREFLUSH_BYTES -- Causes the page(s) in main memory onto which the given
 * area in recoverable memory is currently mapped to be flushed out to disk.
 * This frees the main memory for other data, decreasing the likelihood that
 * useful data will be paged out.  (This call should only be used  when it is
 * known that the given area in recoverable memory will not be referenced
 * again in the forseeable future.)
 *
 * Parameters:
 * 
 *    ptr -- A pointer to the area of recoverable memory to be fetched from
 *    disk.
 * 
 *    length -- The length of the area in bytes.
 * 
 * Syntax:
 * 
 *    PREFLUSH_BYTES(ptr, length);
 */
#define CAMLIB_PREFLUSH_BYTES(ptr, length) \
    CamlibPreflushInternal((char *) (ptr), (length), __FILE__, __LINE__)

#undef CAMLIB_PREFLUSH_BYTES


/*
 *                          
 *
 * ZERO_FILL -- Causes the specified recoverable object to be filled
 * with zeros, in a manner that is efficient for large objects.
 * Reduces the cost of subsequent {\tt MODIFYs} within the object, if
 * old value/new value logging is being used.  This call has the
 * semantics of a lazy top level transaction:  It executes
 * atomically, and permanence is assured if any non-lazy transaction
 * commits after this call returns.  Must be called from within the
 * scope of a transaction. 
 * THIS CALL IS NOT YET IMPLEMENTED IN ITS FINAL FORM.  The semantics
 * are correct, but they are faked in the obvious way with a server-based
 * transaction and multiple calls to {\tt MODIFY\_BYTES.}
 *
 * Parameters:
 * 
 *    object -- The recoverable object to be zero filled.
 * 
 * Syntax:
 * 
 *    ZERO_FILL(REC(name));
 */
#define CAMLIB_ZERO_FILL(object)					    \
    CamlibZeroFillInternal((char *) &(object), sizeof(object),		    \
			   __FILE__, __LINE__)

#undef CAMLIB_ZERO_FILL


/*
 *                          
 *
 * ZERO_FILL_BYTES -- Causes the specified area of recoverable memory to
 * be filled with zeros, in a manner that is efficient for large areas.
 * Reduces the cost of subsequent {\tt MODIFYs} in the area, if old
 * value/new value logging is being used.  This call has the
 * semantics of a lazy top level transaction:  It executes
 * atomically, and permanence is assured if any non-lazy transaction
 * commits after this call returns.  Must be called from within the
 * scope of a transaction. 
 * THIS CALL IS NOT YET IMPLEMENTED IN ITS FINAL FORM.  The semantics
 * are correct, but they are faked in the obvious way with a server-based
 * transaction and multiple {\tt MODIFY\_BYTES} calls.
 *
 * Parameters:
 * 
 *    ptr -- A pointer to the area of recoverable memory to be zero filled.
 * 
 *    length -- The length of the area in bytes.
 * 
 * Syntax:
 * 
 *    ZERO_FILL_BYTES(ptr, length);
 */
#define CAMLIB_ZERO_FILL_BYTES(ptr, length)  \
    CamlibZeroFillInternal((char *) (ptr), (length), __FILE__, __LINE__)

#undef CAMLIB_ZERO_FILL_BYTES


/*
 *                          
 *
 * SERVER_CALL -- Performs a remote procedure call (RPC) on a Camelot server.
 * The remote operation executes under the scope of the enclosing transaction.
 * If this call is made outside the (dynamic) scope of a transaction it will
 * cause the server or application to halt with an error message.  If the
 * call does not respond within 60 seconds, it will time out and abort the
 * innermost nested transaction with status {\tt ACC\_SERVER\_CALL\_TIMEOUT}.
 * Note that the parameters to the call may be evaluated twice, so they
 * should contain no side effects.
 * 
 * Parameters:
 *
 *      serverName -- ({\tt char *}) Name of the server, as specified in
 *      {\tt INITIALIZE\_SERVER}.
 *
 *      call -- The actual call.  The arguments to the call must be preceded
 *      by the keyword {\tt ARGS}, unless the call has no arguments, in which
 *	case the keyword {\tt NOARGS} must be used.  (These keywords cause
 *	hidden system arguments to be passed.)
 *
 * Syntax:
 *
 *      SERVER_CALL("Jill", jill_write(ARGS index, value));
 *
 *   or:
 * 
 *      SERVER_CALL("Jill", jill_reset(NOARGS));
 */
#define CAMLIB_SERVER_CALL(serverName, call) \
    CAMLIB_SERVER_CALL_2((serverName), (call), 60000)

#undef CAMLIB_SERVER_CALL


/*
 *                          
 *
 * SERVER_CALL_2 -- Alternate form of {\tt SERVER\_CALL} that allows the
 * timeout interval to be specified.
 * 
 * Parameters:
 *
 *      serverName -- ({\tt char *}) Name of the server, as specified in
 *      {\tt INITIALIZE\_SERVER}.
 *
 *      call -- The actual call.  The arguments to the call must be preceded
 *      by the keyword {\tt ARGS}, unless the call has no arguments, in which
 *	case the keyword {\tt NOARGS} must be used.  (These keywords cause
 *	hidden system arguments to be passed.)
 *
 *      timeOut -- ({\tt u\_int}) Timeout interval in milliseconds.  A value of
 *	zero indicates that the largest possible timeout value should be used.
 *      (Approximately 50 days.)
 * 
 * Syntax:
 *
 *    SERVER_CALL_2("Jill", jill_write(ARGS index, value), 1000);
 * 
 *   or:
 * 
 *    SERVER_CALL_2("Jill", jill_reset(NOARGS), 1000);
 */
#define CAMLIB_SERVER_CALL_2(serverName, call, timeOut)			    \
do CAM_BEGIN("SERVER_CALL")						    \
    camlib_thread_data_block_t *_threadPtr = CAMLIB_THIS_THREAD_DB_PTR;	    \
    port_t _port;							    \
    cam_tid_t _tid;							    \
    u_int _timeOut = (timeOut);						    \
    cam_rw_lock_t _suspendLock;						    \
    camlib_accessed_srv_data_block_t *_sdbPtr;                              \
    cam_cht_mutex_t _latch;						    \
    int _retCode;							    \
    camlib_on_call_rec_t _ourOnCallRec;					    \
									    \
    CAMLIB_TRANS_CHECK(_threadPtr->tdbPtr.srv, "SERVER_CALL");		    \
    CAMLIB_SRV_BASED_CHECK(_threadPtr->tid, "SERVER_CALL");		    \
    CamlibRegisterReplyPort(&_ourOnCallRec);  /* Does CamlibAbortCheck */   \
									    \
    _tid = _threadPtr->tid;						    \
    if (_timeOut == 0)							    \
	_timeOut = 0xFFFFFFFF;						    \
    if (camlibServer)							    \
	_suspendLock = &_threadPtr->tdbPtr.srv->suspendLock;		    \
    else								    \
	_suspendLock = (cam_rw_lock_t) NULL;				    \
									    \
    if ((serverName) != CAMLIB_UNLISTED_SERVER_PORT)			    \
    {									    \
	/* If we've cached a port for this server, try it */		    \
	if ((_sdbPtr = CamlibAccessedServerLookup((serverName),		    \
				    CAM_LATCH_KEEP, &_latch)) != NULL)	    \
	{                                                                   \
	    _port = _sdbPtr->port;					    \
	    cam_cht_unlock(_latch);					    \
	    _retCode = (call);						    \
	    if (camlibServer)						    \
		CAMLIB_GET_SUSPEND_LOCK(_threadPtr);			    \
    	}								    \
									    \
	/* If no cached port or cached port failed, ask name server */	    \
	if (_sdbPtr == NULL || _retCode == SEND_INVALID_PORT)		    \
	{								    \
	    if (_sdbPtr != NULL)  /* We gave up latch, so get it back */    \
		cam_cht_lock(_latch);					    \
	    _port = CamlibCachePort((serverName), _sdbPtr, _latch);	    \
	    if (_port != PORT_NULL)					    \
	    {								    \
		_retCode = (call);					    \
		if (camlibServer)					    \
		    CAMLIB_GET_SUSPEND_LOCK(_threadPtr);		    \
	    }								    \
	}								    \
    }									    \
    else /* Handle unlisted server */					    \
    {									    \
	_port = _threadPtr->unlistedServerPort;				    \
	_retCode = (call);						    \
	if (camlibServer)						    \
	    CAMLIB_GET_SUSPEND_LOCK(_threadPtr);			    \
    }									    \
									    \
    CamlibDeregisterReplyPort(&_ourOnCallRec);				    \
									    \
    if (_port == PORT_NULL)						    \
	CamlibInternalAbort(CAMLIB_ACC_SERVER_LOOKUP_FAILED, FALSE,	    \
			    __FILE__, __LINE__);			    \
									    \
    if (_retCode != 0)							    \
    {									    \
	if (_retCode == CAM_ER_CALLING_TRANS_ABORTED) /* Death Pill */	    \
	{								    \
	    CamlibAbortCheck();						    \
	    CAM_ER_PRINT_HALT((msg,					    \
		"SERVER_CALL: No record of abort for death pill!\n"));	    \
	}								    \
	else /* Catch timeouts etc. */					    \
	{								    \
	    if ((serverName) != CAMLIB_UNLISTED_SERVER_PORT)		    \
		CamlibUncachePort((serverName), _port);			    \
	    CamlibInternalAbort(CamlibMassageMachErrorCode(_retCode),	    \
				FALSE, __FILE__, __LINE__);		    \
	}								    \
    }									    \
    CAM_LEAVE;								    \
CAM_END while(0)

#undef CAMLIB_SERVER_CALL_2

#define CAMLIB_ARGS _port, _tid, _timeOut, _suspendLock,

#define CAMLIB_NOARGS _port, _tid, _timeOut, _suspendLock


/*
 *                          
 *
 * WRAP_SERVER_CALL -- Performs an operation on a Camelot server.  The
 * operation will be automatically "wrapped" in a top level transaction
 * at the server.  This call can be made outside the scope of a transaction,
 * e.g. from a limited application.  The transaction type defaults to
 * new value logging, standard, the commit protocol defaults to two phased,
 * and the time out interval defaults to 60 seconds.  These defaults can be
 * overridden by using the alternate form {\tt WRAP\_SERVER\_CALL\_2}.  Note
 * that the parameters to the call may be evaluated twice, so they should
 * contain no side effects.
 * 
 * Parameters:
 *
 *      serverName -- ({\tt char *}) Name of the server.
 *
 *      call -- The actual call.  The arguments to the call must be preceded
 *      by the keyword {\tt ARGS}, unless the call has no arguments, in which
 *	case the keyword {\tt NOARGS} must be used.  (These keywords cause
 *	hidden system arguments to be passed.)
 *
 *      status -- ({\tt int}) A variable in which the status of the remote
 *      transaction will be placed.  (Must be in the scope of the block
 *	containing the call.)
 * 
 * Syntax:
 *
 *    WRAP_SERVER_CALL("Jill", jill_write(ARGS index, value), status);
 * 
 *   or:
 * 
 *    WRAP_SERVER_CALL("Jill", jill_reset(NOARGS), status);
 */
#define CAMLIB_WRAP_SERVER_CALL(serverName, call, status)		    \
    CAMLIB_WRAP_SERVER_CALL_2((serverName), (call), (status),		    \
		       CAM_TRAN_NV_STANDARD, CAM_PROT_TWO_PHASED, 60000)

#undef CAMLIB_WRAP_SERVER_CALL 


/*
 *                          
 *
 * WRAP_SERVER_CALL_2 -- Alternate form of {\tt WRAP\_SERVER\_CALL} that allows
 * the transaction type, commit protocol and timeout interval to be specified.
 * Note that the parameters to the call may be evaluated twice, so they should
 * contain no side effects.
 *
 * Parameters:
 *
 *      serverName -- ({\tt (char *}) Name of the server.
 *
 *      call -- The actual call.  The arguments to the call must be preceded
 *      by the keyword {\tt ARGS}, unless the call has no arguments, in which
 *	case the keyword {\tt NOARGS} must be used.  (These keywords cause
 *	hidden system arguments to be passed.)
 *
 *      status -- ({\tt int}) A variable in which the status of the remote
 *      transaction will be placed.  (Must be in the scope of the block
 *	containing the call.)
 *
 *	transType -- ({\tt transaction\_type\_t}) The transaction type.
 *
 *	commitProt -- ({\tt protocol\_type\_t}) The commit protocol.
 *
 *      timeOut -- ({\tt u\_int}) Timeout interval in milliseconds.  A value of
 *	zero indicates that the largest possible timeout value should be used.
 *      (Approximately 50 days.)
 * 
 * Syntax:
 *
 *    WRAP_SERVER_CALL_2("Jill", jill_write(ARGS index, value), status,
 *		         TRAN_NV_STANDARD, PROT_TWO_PHASED, 1000);
 * 
 *   or:
 * 
 *    WRAP_SERVER_CALL_2("Jill", jill_reset(NOARGS), status,
 * 		         TRAN_NV_STANDARD, PROT_TWO_PHASED, 1000);
 * 
 */
#define CAMLIB_WRAP_SERVER_CALL_2(serverName, call, status, transType, commitProt, timeOut)\
do CAM_BEGIN("WRAP_SERVER_CALL")					    \
    camlib_thread_data_block_t *_threadPtr = CAMLIB_THIS_THREAD_DB_PTR;	    \
    port_t _port;							    \
    cam_tid_t _tid;							    \
    u_int _timeOut = (timeOut);						    \
    cam_rw_lock_t _suspendLock;						    \
    camlib_accessed_srv_data_block_t *_sdbPtr;                              \
    cam_cht_mutex_t _latch;						    \
    int _retCode;							    \
									    \
    CamlibAbortCheck();							    \
									    \
    /* Set up TID to cause wrap at server */				    \
    _tid = CAM_TID_MAKE_WRAP((transType), (commitProt));		    \
    if (_timeOut == 0)							    \
	_timeOut = 0xFFFFFFFF;						    \
    if (camlibServer && _threadPtr->tdbPtr.srv)				    \
	_suspendLock = &_threadPtr->tdbPtr.srv->suspendLock;		    \
    else								    \
	_suspendLock = (cam_rw_lock_t) NULL;				    \
									    \
    if ((serverName) != CAMLIB_UNLISTED_SERVER_PORT)			    \
    {									    \
	/* If we've cached a port for this server, try it */		    \
	if ((_sdbPtr = CamlibAccessedServerLookup((serverName),		    \
				CAM_LATCH_KEEP, &_latch)) != NULL)	    \
	{                                                                   \
	    _port = _sdbPtr->port;					    \
	    cam_cht_unlock(_latch);					    \
	    _retCode = (call);						    \
	    if (camlibServer)						    \
		CAMLIB_GET_SUSPEND_LOCK(_threadPtr);			    \
	}								    \
									    \
	/* If no cached port or cached port failed, ask name server */	    \
	if (_sdbPtr == NULL || _retCode == SEND_INVALID_PORT)		    \
	{								    \
	    if (_sdbPtr != NULL)  /* We gave up latch, so get it back */    \
		cam_cht_lock(_latch);					    \
	    _port = CamlibCachePort((serverName), _sdbPtr, _latch);	    \
	    if (_port != PORT_NULL)					    \
	    {								    \
		_retCode = (call);					    \
		if (camlibServer)					    \
		    CAMLIB_GET_SUSPEND_LOCK(_threadPtr);		    \
	    }								    \
	}								    \
    }									    \
    else /* Handle unlisted server */					    \
    {									    \
	_port = CAMLIB_THIS_THREAD_DB_PTR->unlistedServerPort;		    \
	_retCode = (call);						    \
	if (camlibServer)						    \
	    CAMLIB_GET_SUSPEND_LOCK(_threadPtr);			    \
    }									    \
									    \
    if (_port == PORT_NULL)						    \
	(status) = CAMLIB_ACC_SERVER_LOOKUP_FAILED;			    \
    else if (_retCode == 0)						    \
	(status) = 0;							    \
    else if (CAM_ER_GET_CLASS(_retCode) == CAM_ER_CLASS_USER_ABORT)	    \
	(status) = CAM_ER_GET_CODE(_retCode);				    \
    else if (CAM_ER_GET_CLASS(_retCode) == CAM_ER_CLASS_SYSTEM_ABORT	    \
      || CAM_ER_GET_CLASS(_retCode) == CAM_ER_CLASS_RESTRICTED_USER_ABORT)  \
	(status) = _retCode;  /* Death Pill */				    \
    else    /* Timeout etc., invalidate cached port */			    \
    {									    \
	if ((serverName) != CAMLIB_UNLISTED_SERVER_PORT)		    \
	    CamlibUncachePort((serverName), _port);			    \
	(status) = CamlibMassageMachErrorCode(_retCode);		    \
    }									    \
									    \
    CAM_LEAVE;								    \
CAM_END while(0)

#undef CAMLIB_WRAP_SERVER_CALL_2

/*
 *                          
 *
 * UNLISTED_SERVER -- This macro takes the place of the serverName parameter
 * when the {\tt SERVER\_CALL}, {\tt SERVER\_CALL\_2}, {\tt WRAP\_SERVER\_CALL}
 * or {\tt WRAP\_SERVER\_CALL\_2} macros are used to call {\bf unlisted} servers,
 * which are addressed by port rather than by name.
 *
 * Parameters:
 *
 *      port -- The port on which RPC will be sent to the unlisted server.
 * 
 * Syntax:
 *
 *      SERVER_CALL(UNLISTED_SERVER(port), jill_write(ARGS index, value));
 * 
 *   or:
 * 
 *      SERVER_CALL_2(UNLISTED_SERVER(port),
 *		      jill_write(ARGS index, value), 1000);
 *   or:
 * 
 *      WRAP_SERVER_CALL(UNLISTED_SERVER(port),
 *			 jill_write(ARGS index, value), status);
 *   or:
 * 
 *      WRAP_SERVER_CALL_2(UNLISTED_SERVER(port),
 *			   jill_write(ARGS index, value), status,
 *		           TRAN_NV_STANDARD, PROT_TWO_PHASED, 1000);
 *   or:
 * 
 *      NOARGS form of any of the above calls.
 */
#define CAMLIB_UNLISTED_SERVER(port)					    \
    (CAMLIB_THIS_THREAD_DB_PTR->unlistedServerPort = (port),		    \
	CAMLIB_UNLISTED_SERVER_PORT)

#undef CAMLIB_UNLISTED_SERVER


/* 
 *                          
 *
 * THIS_PORT  -- Returns the port on which the RPC currently running arrived.
 * Note that this call must be made from the thread chosen by the server
 * to process the RPC; the receiving port will NOT be passed down to threads
 * formed by cobegin or cofor blocks.
 * 
 *
 * Syntax:
 *
 *	port_t port;
 *
 *      port = THIS_PORT;
 */
#define CAMLIB_THIS_PORT (CAMLIB_THIS_THREAD_DB_PTR->port)

#undef CAMLIB_THIS_PORT

/*
 *                          
 *
 * LOCK -- Obtains a lock on the given name with the specified mode
 * ({\tt LOCK\_MODE\_READ} or {\tt LOCK\_MODE\_WRITE})  The full lock name
 * consists of the name and the name space.  The name space can be
 * the primary lock name space, {\tt LOCK\_SPACE\_PRIMARY} or a space returned
 * by {\tt ALLOC\_LOCK\_SPACE}  If a conflicting lock is currently held, the
 * thread in progress suspends until the lock becomes available.  If the
 * lock is already held, or a write lock is held and a read lock is requested,
 * this operation is a no-op.  If a read lock is held, and a write lock is
 * requested, the read lock will be promoted to a write lock (as soon as the
 * write lock is available).
 * 
 * Parameters:
 *
 *      name -- ({\tt u\_int}) Name on which the lock is sought.
 * 
 *      nameSpace -- ({\tt u\_int}) The name space pertaining to the name.
 *
 *      mode -- ({\tt camlib\_lock\_mode\_t}) Mode of lock sought.
 *
 * Syntax:
 * 
 *    LOCK(name, nameSpace, mode);
 */
#ifdef CAMELOT
#define CAMLIB_LOCK(name, nameSpace, mode) \
switch (RvmMechanism) {							    \
    case NotYetSet :							    \
	break; /* This should already have been detected. */ 		    \
	                                                                    \
    case Camelot :							    \
      CamlibLockInternal((name), (nameSpace), (mode), __FILE__, __LINE__);  \
      break;                                                                \
	                                                                    \
    case NoPersistence :					       	    \
	/* Do Nothing */						    \
       break;								    \
	                                                                    \
    case Rvm :                                                              \
	/* Do Nothing */						    \
       break;								    \
}
#else CAMELOT
#define CAMLIB_LOCK(name, nameSpace, mode) \
switch (RvmMechanism) {							    \
    case NotYetSet :							    \
	break; /* This should already have been detected. */ 		    \
	                                                                    \
    case Camelot :							    \
      assert(0);							    \
      break;                                                                \
	                                                                    \
    case NoPersistence :					       	    \
	/* Do Nothing */						    \
       break;								    \
	                                                                    \
    case Rvm :                                                              \
	/* Do Nothing */						    \
       break;								    \
}
#endif CAMELOT

/*
 *                          
 * 
 * TRY_LOCK --  Obtains a lock on the given name with the given mode if
 * it is immediately available, in which case the function returns the value
 * {\tt TRUE} Otherwise, the function returns immediately with the value
 * {\tt FALSE}  If the lock is already held, or a write lock is held and a 
 * read lock is requested, this operation is a no-op.  If a read lock is 
 * held, and a write lock is requested, the read lock will be promoted to a 
 * write lock if no conflict would result.  If a conflict would result, the
 * function returns immediately with the value {\tt FALSE}.
 *
 * 
 * Parameters:
 *
 *      name -- ({\tt u\_int}) Name on which the lock is sought.
 * 
 *      nameSpace -- ({\tt u\_int}) The name space pertaining to the name.
 *
 *      mode -- ({\tt camlib\_lock\_mode\_t}) Mode of lock sought.
 *
 * Syntax:
 * 
 *    if (TRY_LOCK(name, nameSpace, mode))
 *	  ...;
 *    else
 *	  ...;
 */
#ifdef CAMELOT
#define CAMLIB_TRY_LOCK(name, nameSpace, mode) \
switch (RvmMechanism) {							    \
    case NotYetSet :							    \
	break; /* This should already have been detected. */		    \
	                                                                    \
    case Camelot :							    \
      CamlibTryLockInternal((name), (nameSpace), (mode), __FILE__, __LINE__);\
      break;                                                                \
	                                                                    \
    case NoPersistence :					       	    \
	/* Do nothing */						    \
       break;								    \
	                                                                    \
    case Rvm :                                                              \
	/* Do nothing */						    \
       break;								    \
}
#else CAMELOT
#define CAMLIB_TRY_LOCK(name, nameSpace, mode) \
switch (RvmMechanism) {							    \
    case NotYetSet :							    \
	break; /* This should already have been detected. */		    \
	                                                                    \
    case Camelot :							    \
      assert(0);							    \
      break;                                                                \
	                                                                    \
    case NoPersistence :					       	    \
	/* Do nothing */						    \
       break;								    \
	                                                                    \
    case Rvm :                                                              \
	/* Do nothing */						    \
       break;								    \
}
#endif CAMELOT

/*
 *                          
 * 
 * UNLOCK --  Causes a transaction to prematurely release a lock.  (In
 * fact, the possession-count for the weakest mode for which the transaction
 * holds the lock is decremented.  If both the read and write count are zero
 * after the decrement, then the lock will actually be released.)  If
 * the calling transaction doesn't hold a lock on the given name, the
 * server will halt with an appropriate error message.  This procedure must
 * be used with extreme caution, especially on write locks, as it makes it
 * very easy to violate atomicity and serializability.
 * 
 * Parameters:
 * 
 *      name -- ({\tt u\_int}) Name on which the lock is sought.
 * 
 *      nameSpace -- ({\tt u\_int}) The name space pertaining to the name.
 *
 * Syntax:
 * 
 *    UNLOCK(name, nameSpace);
 */
#ifdef CAMELOT
#define CAMLIB_UNLOCK(name, nameSpace) \
switch (RvmMechanism) {							    \
    case NotYetSet :							    \
	break; /* This should already have been detected. */		    \
	                                                                    \
    case Camelot :							    \
      CamlibUnlockInternal((name), (nameSpace), __FILE__, __LINE__);	    \
      break;                                                                \
	                                                                    \
    case NoPersistence :					       	    \
       break;								    \
	                                                                    \
    case Rvm :                                                              \
	/* Do nothing */						    \
       break;								    \
}
#else CAMELOT
#define CAMLIB_UNLOCK(name, nameSpace) \
switch (RvmMechanism) {							    \
    case NotYetSet :							    \
	break; /* This should already have been detected. */		    \
	                                                                    \
    case Camelot :							    \
      assert(0);							    \
      break;                                                                \
	                                                                    \
    case NoPersistence :					       	    \
       break;								    \
	                                                                    \
    case Rvm :                                                              \
	/* Do nothing */						    \
       break;								    \
}
#endif CAMELOT

/*
 *                          
 *
 * DEMOTE_LOCK --  Causes a transaction to demote a write lock to a read 
 * lock.  If the transaction's write-count on the given lock is reduced to
 * zero, this allows concurrent transactions to secure read locks.
 * If the calling transaction doesn't hold a write lock on the given
 * name, the server will halt with an appropriate error message.  This
 * procedure must be used with extreme caution, as it makes it very easy to
 * violate atomicity and serializability.
 *
 * Parameters:
 * 
 *      name -- ({\tt u\_int}) Name on which the lock is sought.
 * 
 *      nameSpace -- ({\tt u\_int}) The name space pertaining to the name.
 * 
 * Syntax:
 * 
 *    DEMOTE_LOCK(name, nameSpace);
 */
#define CAMLIB_DEMOTE_LOCK(name, nameSpace) \
    CamlibDemoteLockInternal((name), (nameSpace), __FILE__, __LINE__)

#undef CAMLIB_DEMOTE_LOCK


/*
 *                          
 *
 * LOCK_NAME --  Generates a lock name for a recoverable object.  The name
 * will be the real memory address of the object.  The object must be of
 * a type to which a pointer can be taken.  (e.g.  object cannot be an array,
 * but it can be an array element.)
 *
 * Parameters:
 *
 *      object -- The recoverable object for which a lock name is desired.
 *
 * Syntax:
 *
 *      LOCK(LOCK_NAME(REC(object)), nameSpace, mode))
 *
 */
#define CAMLIB_LOCK_NAME(object) ((u_int) &(object))


/*
 *                          
 * 
 * ALLOC_LOCK_SPACE -- Returns a new lock name space, distinct from all
 * other lock spaces returned by this call at the server in question.  This
 * allows an abstraction to generate lock names guaranteed not to conflict
 * with other locks names in use at the server.
 * 
 * Syntax:
 * 
 *    u_int newLockSpace;
 * 
 *    newLockSpace = ALLOC_LOCK_SPACE();
 */
#define CAMLIB_ALLOC_LOCK_SPACE() \
    CamlibAllocLockSpaceInternal(__FILE__, __LINE__)

#undef CAMLIB_ALLOC_LOCK_SPACE


/*
 *                          
 * 
 * FREE_LOCK_SPACE -- Frees lock spaces allocated with
 * {\tt ALLOCATE\_LOCK\_SPACE} so they can be reused.
 * CURRENTLY, THIS ROUTINE IS A NO-OP.
 *
 * Syntax:
 *
 *    u_int lockSpace;
 * 
 *    FREE_LOCK_SPACE(lockSpace);
 */
#define CAMLIB_FREE_LOCK_SPACE(lockSpace)

#undef CAMLIB_FREE_LOCK_SPACE

/*
 * INITIALIZE_APPLICATION -- Initializes an application.  Must be called once,
 * before using any other Camelot Library function.  Returns {\tt TRUE} iff
 * Camelot is running at this node.  If {\tt FALSE} is returned, the
 * application can only run as a {\bf limited application}; remote camelot
 * servers can be called via {\tt WRAP\_SERVER\_CALL}, but no transactions 
 * can be initiated. 
 * 
 * Parameters:
 * 
 *     applName -- Descriptive name used by Camelot to refer to this 
 *     application in error and informational messages.
 */
#ifdef CAMELOT
boolean_t CAMLIB_INITIALIZE_APPLICATION(
#ifdef	__cplusplus
char *applName 
#endif
);
#endif CAMELOT

/*
 *                          
 *
 * COBEGIN, COEND, TRANS, TRANS_2 -- These macros are used to form a
 * {\bf cobegin block}, which allows a fixed number of procedures to run
 * concurrently as subtransactions of the enclosing transaction.  (The
 * procedures run in separate top level transactions if the construct
 * occurs outside the scope of a transaction.)  The construct is syntactically
 * a single code block.  Execution of the block does not finish until
 * all of the concurrent transactions have completed (or the enclosing
 * transaction  aborts).  The alternative form {\tt TRANS\_2} can be used to
 * specify the protocols to be used by the concurrent transactions.  These
 * specifications are meaningful only if the transactions are top level,
 * i.e. the block is outside the scope of a transaction.
 * 
 * Parameters:
 *
 *      proc -- ({\tt void (*)()})  The procedure to be executed as a
 *      transaction.  The procedure can take at most one argument,
 *      and it must be passable as a pointer.
 *
 *      arg -- ({\tt Any type that can be cast to a pointer}) The argument
 *	to pass to {\tt proc}.  If {\tt proc} doesn't take an argument, this 
 *	parameter is ignored.
 *
 *      status -- ({\tt OUT int})  Must be a variable in the scope of the
 *      enclosing block.  When the transaction has completed, the variable
 *      will contain a value specifiying the outcome of the transaction.
 *      Status will be 0 if the transaction committed.  1 through $2^{16}-1$
 *      are voluntary abort codes, and other values are system abort codes.
 *
 *      transType -- ({\tt transaction\_type\_t}) The transaction type.
 *
 *      commitProt -- ({\tt protocol\_type\_t}) The commit protocol.
 *     
 * Syntax:
 *
 *      COBEGIN
 *          TRANS(proc1, arg1, status1);
 *          TRANS(proc2, arg2, status2);
 *                  .
 *                  .
 *                  .
 *          TRANS(procN, argN, statusN);
 *      COEND
 * 
 *   or:
 * 
 *      COBEGIN
 *          TRANS_2(proc1, arg1, status1, transType1, commitProc1);
 *          TRANS_2(proc2, arg2, status2, transType2, commitProc2);
 *                  .
 *                  .
 *                  .
 *          TRANS_2(procN, argN, statusN, transTypeN, commitProcN);
 *      COEND
 */
#define CAMLIB_COBEGIN							    \
CAM_BEGIN("COBEGIN")							    \
    camlib_concurrency_data_block_t _cdb;				    \
									    \
    CamlibAbortCheck();							    \
									    \
    /* Initialize concurrency data block */		    		    \
    _cdb.nthreads = 0;							    \
    condition_init(&_cdb.done);						    \
    mutex_init(&_cdb.latch);						    \
    mutex_lock(&_cdb.latch);						    \
									    \
    {

#undef CAMLIB_COBEGIN

#define CAMLIB_TRANS(proc, arg, status)					    \
    do {								    \
	CamlibSpawnSubTrans(&_cdb, (proc), (any_t) (arg), &(status),	    \
			    CAM_TRAN_NV_STANDARD, CAM_PROT_TWO_PHASED);	    \
	_cdb.nthreads++;						    \
    } while(0)

#define CAMLIB_TRANS_2(proc, arg, status, transType, commitProt)	    \
    do {								    \
	CamlibSpawnSubTrans(&_cdb, (proc), (any_t) (arg), &(status),	    \
			    (transType), (commitProt));			    \
	_cdb.nthreads++;						    \
    } while(0)

#undef CAMLIB_TRANS

#define CAMLIB_COEND							    \
    }									    \
									    \
    if (_cdb.nthreads != 0)						    \
    {									    \
	CamlibIncrementRefCount(_cdb.nthreads);				    \
	condition_wait(&_cdb.done, &_cdb.latch);			    \
    }									    \
    cam_cht_unlock(&_cdb.latch);					    \
									    \
    if (_cdb.nthreads != 0)						    \
	CAM_ER_PRINT_HALT((msg, "COEND: Reference count nonzero!\n"));	    \
									    \
    CamlibAbortCheck();							    \
									    \
    CAM_LEAVE;								    \
CAM_END

#undef CAMLIB_COEND


/*
 *                          
 *
 * COFOR --  This macro, in combination with the {\tt TRANS} and {\tt COEND}
 * macros, is used to form a {\bf cofor block}, which allows a variable
 * number of procedures to run concurrently as subtransactions of the
 * enclosing transaction.  (The procedures run in separate top level
 * transactions if the construct occurs outside the scope of a transaction.)
 * The construct is syntactically a single code block.  Execution of the
 * block does not finish until all of the concurrent transactions have
 * completed (or the enclosing transaction  aborts).  The alternative
 * form {\tt TRANS\_2} can be used to specify the protocols to be used by
 * the concurrent transactions.  These specifications are meaningful only
 * if the transactions are top level, i.e. the block is outside the scope
 * of a transaction.
 *
 * Parameters:
 *
 *      loopVar -- ({\tt OUT int}) loopVar must be an integer variable in the
 *      scope of the block containing the {\tt COFOR}.  It loops from zero to
 *      {\tt numThreads-1}.
 *
 *      numThreads -- ({\tt int}) The number of threads to be run by the
 *      {\tt COFOR}. (This expression will be evaluated once, at the top
 *      of the loop.)
 *
 *      status -- ({\tt OUT int})  Must be a variable in the scope of the
 *      enclosing block.  When the block has completed, the variable
 *      will contain a value specifiying the outcome of the transaction.
 *      Status will be 0 if the transaction committed.  1 through $2^{16}-1$
 *      voluntary abort codes, and other values are system abort codes.
 *      This parameter can refer to the loop variable (e.g. as an array 
 *	index) so that a different status variable is used for each
 *      transaction.
 *
 *      proc -- ({\tt void (*)()})  The procedure to be executed as a
 *      transaction.  The procedure can take at most one argument, and
 *      it must be passable as a pointer.  This parameter can refer to the
 *      loop variable so that it evaluates differently for each transaction.
 *
 *      arg -- ({\tt any type that can be cast to a pointer}) The argument
 *	to pass to {\tt proc}.  If {\tt proc} doesn't take an argument, this
 *	parameter is ignored.  This parameter can refer to the loop variable
 *	so that it evaluates differently for each transaction.
 *
 *      transType -- ({\tt transaction\_type\_t}) The transaction type.
 *
 *      commitProt -- ({\tt protocol\_type\_t}) The commit protocol.
 * 
 * Syntax:
 * 
 *      COFOR(loopVar, numThreads)
 *          TRANS(proc, arg, status);
 *      COEND
 * 
 *   or:
 * 
 *      COFOR(loopVar, numThreads)
 *          TRANS_2(proc, arg, status, transType, commitProc);
 *      COEND
 * 
 */
#define CAMLIB_COFOR(loopVar, numThreads)				    \
CAM_BEGIN("COFOR")							    \
    camlib_concurrency_data_block_t _cdb;				    \
    int _n;								    \
									    \
    CamlibAbortCheck();							    \
									    \
    /* Initialize concurrency data block */				    \
    _cdb.nthreads = 0;							    \
    condition_init(&_cdb.done);						    \
    mutex_init(&_cdb.latch);						    \
    mutex_lock(&_cdb.latch);						    \
									    \
    /* Do the 'for' loop */						    \
    _n = (numThreads);  /* Prevent repeated evaluation of numThreads */	    \
    for ((loopVar) = 0; (loopVar) < _n; (loopVar)++)			    \
    {

#undef CAMLIB_COFOR

/*
 *                          
 *
 * THREAD -- This macro can be used in cobegin and cofor blocks to allow
 * a fixed or variable number of procedures to run concurrently, all in the
 * scope of the enclosing transaction.  (If this construct occurs outside
 * the scope of a transaction, the concurrent threads run outside the scope
 * of a transacton.)  Note that this construct should be used with caution;
 * if multiple threads run concurrently under a single transaction, locking
 * is no longer sufficient to enforce serializability.
 *
 * Parameters:
 *
 *      proc -- ({\tt void (*)()}) The procedure to be executed by the
 *      thread.  The procedure can take at most one argument, and it must
 *      be passable as a pointer.
 *
 *      arg -- ({\tt any type that can be cast to a pointer}) The argument
 *	to pass to {\tt proc}.  If {\tt proc} doesn't take an argument, this
 *	parameter is ignored. 
 *
 * Syntax:
 *    
 *      COBEGIN
 *          THREAD(proc1, arg1);
 *          THREAD(proc2, arg2);
 *                  .
 *                  .
 *                  .
 *          THREAD(procN, argN);
 *      COEND
 * 
 *   or:
 * 
 *      COFOR(loopVar, numThreads)
 *          THREAD(proc, arg);
 *      COEND
 */
#define CAMLIB_THREAD(proc, arg)					    \
    do {								    \
	CamlibSpawnSubThread(&_cdb, (proc), (any_t) (arg));		    \
	_cdb.nthreads++;						    \
    } while(0)
#undef CAMLIB_THREAD


/*
 * camlibPortDeathProc -- Storing a function pointer in this variable
 * causes the function to be called upon receipt of a port death message,
 * with the dead port as its argument.  (A port death message indicates
 * that a port on which this server had send rights has died.  See the
 * Mach manual for additional information on port death messages.)
 */
extern void (*camlibPortDeathProc)() ;

/*
 *                          
 *
 * CamlibSetRpcReceivePort -- Macro to set the port on which the calling
 * thread will henceforth wait for RPCs.  This routine has no effect if called
 * from an application or a server system thread.  It is typically used to
 * produce threads that handle only a special class of requests.  This
 * ensures that a thread will always be available to handle requests of
 * this class even if the server is bombarded with "ordinary" requests.
 *
 * Parameters:
 *
 *	port -- ({\tt port\_t}) The port on which the calling thread is to
 *	wait for requests.
 *
 * Syntax:
 *
 *	CamlibSetRpcReceivePort(port);
 */
#define CamlibSetRpcReceivePort(port) \
    CAMLIB_THIS_THREAD_DB_PTR->rpcReceivePort = (port)
#undef CamlibSetRpcReceivePort


/*
 * camlibRecSegLow, camlibRecSegHigh -- Global variables set by
 * {\tt INITIALIZE\_SERVER} to the start and end addresses of the recoverable
 * segment.  These variables can be examined by the (rare) server or
 * application with a need to know this information.
 */
extern u_int camlibRecSegLow, camlibRecSegHigh;

/*
 *                          
 * 
 * CamlibSuppressTransTimeout -- Prevents the server's "housecleaning thread"
 * from aborting the transaction family under which the current thread
 * is running, allowing the family to remain at the server indefinitely.
 * NOTE:  If all of the transactions in the family that have done work at
 * a server abort, the server will purge all records of the family.  If a
 * future RPC causes the family to reappear at the server, it will again be
 * subject to transaction timeout, unless this procedure is reinvoked.
 * 
 * Syntax:
 * 
 *    CamlibSuppressTransTimeout();
 */
#define CamlibSuppressTransTimeout() \
     CamlibSuppressTransTimeoutInternal(__FILE__, __LINE__)

#undef CamlibSuppressTransTimeout

