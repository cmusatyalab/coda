
#ifndef _DS_RRLIST_PRIVATE_H_
#define _DS_RRLIST_PRIVATE_H_

#include <odytypes.h>

#include "ds_list.h"
#include "ds_rrlist.h"

/*
 * ds_rrlist.private.h: implementation-specific details of 
 *                      resource request lists.
 */

extern magic_t ds_rrlist_magic;

#define DS_RRLIST_VALID(rp)   ((rp) && ((rp)->magic == ds_rrlist_magic))

/*
   Each request node is placed in four different structures for
   fast access: hashed by pid and reqid, and sorted by lower bound
   and upper bound.  The sorted forms are currently kept as
   lists; this may change to something slicker if insertion seems
   to take too long.  I don't expect these to grow very much, so
   I'm really not worried about it.
*/

struct ds_rrlist_t {
    magic_t          magic;          /* magic number */
    long             value;          /* current value */
    ds_hash_t       *pidhash;        /* hash table on pids */
    ds_hash_t       *reqhash;        /* hash table on request id's */
    ds_list_t       *less;           /* list for lower bounds */
    ds_list_t       *greater;        /* list for upper bounds */
};

/* size of hash tables */
#define DS_RRLIST_HASHSIZE 67

/* We need a structure that hashes all request id's to the rrlist that
   that request is currently on. */

/* elements for that hash table */

extern magic_t ds_rrlist_reqtab_elt_magic;

#define DS_RRLIST_REQTAB_ELT_VALID(rre)   \
         ((rre) && ((rre)->magic == ds_rrlist_reqtab_elt_magic))


typedef struct ds_rrlist_reqtab_elt_t {
    magic_t          magic;
    long             reqid;
    ds_rrlist_t     *list;
} ds_rrlist_reqtab_elt_t;

/* 
 * Note: ds_rrlist.c is the only entity that sees these monsters,
 * so it is responsible for both allocation and deallocation.
 */

/* ds_rrlist_reqtab_elt_t *rre, long rqid, ds_rrlist_t *lst */
#define DS_RRLIST_REQTAB_ELT_CREATE(rre,rqid,lst)  \
do {                                               \
    ALLOC(rre,ds_rrlist_reqtab_elt_t);             \
    (rre)->magic = ds_rrlist_reqtab_elt_magic;     \
    (rre)->reqid = rqid;                           \
    (rre)->list = lst;                             \
} while (0)

#define DS_RRLIST_REQTAB_ELT_DESTROY(rre)     \
do {                                          \
    ASSERT(DS_RRLIST_REQTAB_ELT_VALID(rre));  \
    (rre)->magic = 0;                         \
    (rre)->reqid = 0;                         \
    (rre)->list = NULL;                       \
    FREE(rre);                                \
} while (0)

#endif _DS_RRLIST_PRIVATE_H_
