/* BLURB lgpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#ifndef _DLIST_H_
#define _DLIST_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <stddef.h>

/*
 * doubly linked list implementation -- based on linux 
 * kernel code lists.
 *
 */

struct dllist_head {
	struct dllist_head *next, *prev;
};

#define INIT_LIST_HEAD(name) \
    struct dllist_head name = { &name, &name }

#define list_entry(ptr, type, member) \
	((type *)((char *)(ptr)-offsetof(type, member)))

#ifdef CODA_PTR_TO_MEMBER
#define list_entry_plusplus(ptr, type, member) \
	((type *)((char *)(ptr)-(size_t)(&type::member)))
#else
#define list_entry_plusplus(ptr, type, member) list_entry(ptr, type, member)
#endif

#define list_for_each(ptr, head) \
        for (ptr = (head).next; ptr != &(head); ptr = ptr->next)

#ifdef __cplusplus
extern "C" {
#endif

int list_empty(struct dllist_head *head);
void list_del(struct dllist_head *entry);
void list_add(struct dllist_head *entry, struct dllist_head *head);
void list_head_init(struct dllist_head *ptr);

#ifdef __cplusplus
}
#endif

#endif /* _DLLIST_H_ */

