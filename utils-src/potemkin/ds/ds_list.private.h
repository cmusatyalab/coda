/*
** ds_list.private.h: Implementation details of ds_list_t, ds_list_elt_t
*/

#ifndef _DS_LIST_PRIVATE_H_
#define _DS_LIST_PRIVATE_H_

#include <odytypes.h>

#include "ds_list.h"  /* public parts */

/* magic numbers for structures */
extern const magic_t ds_list_magic;
extern const magic_t ds_list_elt_magic;
extern const magic_t ds_list_iter_magic;

/* the structures themselves. */


/* 
 * An element has a magic number, a next and prev link, and the contents.
 * The contents themselves are untyped.
 */


typedef struct ds_list_elt_t {
    magic_t              magic;
    struct ds_list_elt_t *n;
    struct ds_list_elt_t *p;
    void                 *contents;
} ds_list_elt_t;

/* 
 * A list has a magic number, a comparison function, 
 * safety and duplicate information, a count of elements, and a pointer
 * to the head  and tail elements.  
 * The list is doubly-linked: the head and tail elements each point to
 * null "off the end" of the list.
 * There is also a "list" of active (i.e.: not-destroyed) iterators
 * for this list.  Naturally, the iterator "list" cannot be a ds_list_t.
 */

struct ds_list_t {
    magic_t                magic;
    COMPFN                 cmpfn;
    bool                   is_safe;
    bool                   has_dups;
    int                    count;
    struct ds_list_elt_t  *head;
    struct ds_list_elt_t  *tail;
    struct ds_list_iter_t *iter_list;
};

struct ds_list_iter_t {
    magic_t                 magic;
    ds_list_t              *list;
    ds_list_elt_t          *next_elt;
    struct ds_list_iter_t  *next_iter;
};

#define DS_LIST_VALID(lp) ((lp) && ((lp)->magic == ds_list_magic))
#define DS_LIST_ELT_VALID(ep) ((ep) && ((ep)->magic == ds_list_elt_magic))
#define DS_LIST_ITER_VALID(ip) ((ip) && ((ip)->magic == ds_list_iter_magic))

#endif /* _DS_LIST_PRIVATE_H_ */

