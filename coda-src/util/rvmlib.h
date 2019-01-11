/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2018 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#ifndef _RVMLIB_H_
#define _RVMLIB_H_ 1

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "coda_string.h"
#include <rvm/rvm.h>
#include <unistd.h>
#include <stdlib.h>
#include <setjmp.h>

#include <lwp/lwp.h>
#include <rvm/rvm.h>
#include <rvm/rds.h>

#include <rvm/rds.h>
#include <util.h>

#ifdef __cplusplus
}
#endif

/*  *****  Types  *****  */

typedef enum
{
    UNSET = 0, /* uninitialized */
    RAWIO = 1, /* raw disk partition */
    UFS   = 2, /* Unix file system */
    VM    = 3 /* virtual memory */
} rvm_type_t;

typedef struct {
    rvm_tid_t *tid;
    rvm_tid_t tids;
    /*	jmp_buf abort; */
    intentionList_t list;

    /* where was the transaction started */
    const char *file;
    int line;
} rvm_perthread_t;

/*  *****  Variables  *****  */

extern rvm_type_t RvmType; /* your program must supply this! */
extern long rvm_no_yield; /*  exported by rvm */

/*  ***** Functions  ***** */

#ifdef __cplusplus
extern "C" {
#endif

int rvmlib_in_transaction(void);
void rvmlib_abort(int);

void rvmlib_set_range(void *base, unsigned long size);
void rvmlib_modify_bytes(void *dest, const void *newval, int len);
char *rvmlib_strdup(const char *src, const char *file, int line);

void *rvmlib_malloc(unsigned long size, const char *file, int line);
void rvmlib_free(void *p, const char *file, int line);

void rvmlib_init_threaddata(rvm_perthread_t *rvmptt);
extern void rvmlib_set_thread_data(void *);
rvm_perthread_t *rvmlib_thread_data(void);

#define rvmlib_begin_transaction(restore_mode) \
    _rvmlib_begin_transaction(restore_mode, __FILE__, __LINE__);
void _rvmlib_begin_transaction(int restore_mode, const char file[], int line);
void rvmlib_end_transaction(int flush_mode, rvm_return_t *statusp);

#ifdef __cplusplus
}
#endif

#define CODA_STACK_LENGTH 0x20000 /* 128 K */
#define LOGTHRESHOLD 50

/* pointer to rvm_perthread_t must be under this rock! */
extern int optimizationson;

#define RVMLIB_ASSERT(errmsg)                           \
    do {                                                \
        fprintf(stderr, "RVMLIB_ASSERT: %s\n", errmsg); \
        fflush(stderr);                                 \
        coda_assert("0", __FILE__, __LINE__);           \
    } while (0)

#define rvmlib_rec_malloc(size) rvmlib_malloc(size, __FILE__, __LINE__)
#define rvmlib_rec_free(addr) rvmlib_free(addr, __FILE__, __LINE__)
#define rvmlib_rec_strdup(size) rvmlib_strdup(size, __FILE__, __LINE__)

#define RVMLIB_REC_OBJECT(object) rvmlib_set_range(&(object), sizeof(object))

void rvmlib_check_trans(char *where, char *file);
#define rvmlib_intrans() rvmlib_check_trans(__FUNCTION__, __FILE__)

/* macros */

#define RVMLIB_MODIFY(object, newValue)                                       \
    do {                                                                      \
        rvm_perthread_t *_rvm_data = rvmlib_thread_data();                    \
        if (RvmType == VM)                                                    \
            (object) = (newValue);                                            \
        else if (RvmType == RAWIO ||                                          \
                 RvmType == UFS) { /* is object a pointer? */                 \
            rvm_return_t ret = rvm_set_range(_rvm_data->tid, (char *)&object, \
                                             sizeof(object));                 \
            if (ret != RVM_SUCCESS)                                           \
                printf("Modify Bytes error %s\n", rvm_return(ret));           \
            CODA_ASSERT(ret == RVM_SUCCESS);                                  \
            (object) = (newValue);                                            \
        } else {                                                              \
            CODA_ASSERT(0);                                                   \
        }                                                                     \
    } while (0)

#endif /* _RVMLIB_H_ */
