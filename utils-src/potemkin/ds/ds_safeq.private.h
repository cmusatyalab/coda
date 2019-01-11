/*
 * ds_safeq.private.h: Implementation details of ds_safeq_t
 */

#ifndef _DS_SAFEQ_PRIVATE_H_
#define _DS_SAFEQ_PRIVATE_H_

#include <odytypes.h>
#include <lwp/lock.h>
#include "ds_safeq.h"

extern const magic_t ds_safeq_magic;

struct ds_safeq_t {
    magic_t sq_magic;
    char sq_signal;
    struct Lock sq_lock;
    ds_list_t *sq_list;
};

#define DS_SAFEQ_VALID(qp) ((qp) && ((qp)->sq_magic == ds_safeq_magic))

#endif /* _DS_SAFEQ_PRIVATE_H_ */
