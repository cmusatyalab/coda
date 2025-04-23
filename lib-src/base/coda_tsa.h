/* BLURB lgpl

                           Coda File System
                              Release 8

          Copyright (c) 2021-2025 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights
                           none currently
#*/

/* Define annotations to leverage clang's thread safety analysis functionality
 * to identify problematic RVM transaction usage.
 * - Forgetting to end a transaction in error paths.
 * - Making sure any function that uses rvm functionality is called from
 *   places that started a transaction.
 *
 * Hoping to add at some point.
 * - Identifying places where our thread yields during an active transaction.
 * - Identify variables in RVM that are mutated while not in a transaction.
 */

#ifndef _CODA_TSA_H_
#define _CODA_TSA_H_ 1

#ifdef __cplusplus
extern "C" {
#endif

/* Probably only works when clang is used as the compiler */
#if defined(__has_attribute)
#if __has_attribute(acquire_capability)

/* define a dummy global variables to be used as a unique markers to
 * indicate if we are in a transaction or not. */
extern struct __attribute__((capability("mutex"))) {
    char x;
} __rvm_transaction__, __no_transaction__;

/* Simple begin transaction, end transaction annotations */
#define BEGINS_TRANSACTION                                  \
    __attribute__((release_capability(__no_transaction__))) \
    __attribute__((acquire_capability(__rvm_transaction__)))
#define ENDS_TRANSACTION                                     \
    __attribute__((release_capability(__rvm_transaction__))) \
    __attribute__((acquire_capability(__no_transaction__)))

/* This formalize the existing 'MUST be called from within transaction'
 * comments as a traceable annotation on the function prototype. */
#define REQUIRES_TRANSACTION \
    __attribute__((requires_capability(__rvm_transaction__)))

/* This indicates the function (or a child) starts a transaction, so we
 * shouldn't be in a transaction already, also useful to tag yielding functions
 * that we don't want to call during a transaction.
 *
 * Use on function prototypes to declare they should not be called when a RVM
 * transaction is already active, i.e. the function (or children) start their
 * own transaction or yield to other LWP threads.
 *
 * Should be used in the code to mark entry points such as main() and RPC2
 * server function implementations. */
#define EXCLUDES_TRANSACTION \
    __attribute__((requires_capability(__no_transaction__)))
// this is the weaker version that doesn't rely on a __no_transaction__ lock.
//    __attribute__((locks_excluded(__rvm_transaction__)))

/* This is more tricky, some functions change their behaviour based on an in
 * place test (rvm_in_trans) or from a passed argument (recoverable=True).
 * The first case is probably ok, for the second case we can't actually prove
 * they were called correctly. Either way, disable analysis to avoid warnings.
 * It may be useful to split these functions into RVM and non-RVM variants. */
#define TRANSACTION_OPTIONAL __attribute__((no_thread_safety_analysis))

/* And these should be useful to flag specific variables that should only be
 * mutated while within a transaction. */
#define RVM_OBJECT __attribute__((guarded_by(__rvm_transaction__)))
#define RVM_OBJECT_PTR __attribute__((pt_guarded_by(__rvm_transaction__)))

/* add annotations to some librvm functions */
#include <rvm/rvm.h>

rvm_return_t rvm_begin_transaction(rvm_tid_t *, rvm_mode_t) BEGINS_TRANSACTION;
rvm_return_t rvm_set_range(rvm_tid_t *, void *,
                           rvm_length_t) REQUIRES_TRANSACTION;
rvm_return_t rvm_modify_bytes(rvm_tid_t *, void *, const void *,
                              rvm_length_t) REQUIRES_TRANSACTION;
rvm_return_t rvm_abort_transaction(rvm_tid_t *) ENDS_TRANSACTION;
rvm_return_t rvm_end_transaction(rvm_tid_t *, rvm_mode_t) ENDS_TRANSACTION;

#endif /* __has_attribute(acquire_capability) */
#endif /* defined(__has_attribute) */

#ifdef __cplusplus
}
#endif

#ifndef REQUIRES_TRANSACTION
#define BEGINS_TRANSACTION
#define ENDS_TRANSACTION
#define REQUIRES_TRANSACTION
#define EXCLUDES_TRANSACTION
#define TRANSACTION_OPTIONAL
#define RVM_OBJECT
#define RVM_OBJECT_PTR
#endif

#endif /* _CODA_TSA_H */
