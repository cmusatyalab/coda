/*
** ds_hash.private.h: Implementation details of ds_hash_t, ds_hash_elt_t
*/

#ifndef _DS_HASH_PRIVATE_H_
#define _DS_HASH_PRIVATE_H_

#include <odytypes.h>

#include "ds_list.h"  /* we're a client of lists */
#include "ds_hash.h"  /* public parts */

/* magic numbers for structures */

extern magic_t ds_hash_magic;
extern magic_t ds_hash_iter_magic;

/* A hash table has a magic number, an array of ds_list_t's, 
   and a count of the number of elements.
   The safety and duplicate properties are maintained by the
   table's lists; the table doesn't bother about it.

   Hash tables are pretty simple: lists do most of the work
   They also don't need to keep track of their iterators: the lists
   do that (remember, that a hash iterator is just a list iterator
   that is pointed at several lists in a row.)
*/

struct ds_hash_t {
    magic_t       magic;
    HFN           hfn;
    int           nbuckets;
    ds_list_t   **buckets;
    int           count;
};

struct ds_hash_iter_t {
    magic_t         magic;
    ds_hash_t      *table;
    int             curbucket;
    ds_list_iter_t *curiter;
};

#define DS_HASH_VALID(tp)      ((tp) && ((tp)->magic == ds_hash_magic))
#define DS_HASH_ITER_VALID(ip) ((ip) && ((ip)->magic == ds_hash_iter_magic))

#endif /* _DS_HASH_PRIVATE_H_ */

