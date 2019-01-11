/*
 * (Thread-)safe queues
 */

#ifndef _DS_SAFEQ_H_
#define _DS_SAFEQ_H_

#include <odytypes.h>

typedef struct ds_safeq_t ds_safeq_t;

/* 
 * observer - test that a safeq is a valid structure 
 */
extern bool ds_safeq_valid(ds_safeq_t *q);

/*
 * Mutators
 *
 * A deq will block until there is something to return.
 */
extern ds_safeq_t *ds_safeq_create();
extern void ds_safeq_destroy(ds_safeq_t *q);
extern void ds_safeq_enq(ds_safeq_t *q, void *i);
extern void *ds_safeq_deq(ds_safeq_t *q);

#endif /* _DS_SAFEQ_H_ */
