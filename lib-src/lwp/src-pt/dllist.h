/* BLURB lgpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#ifndef _DLLIST_H_
#define _DLLIST_H_

/* based on linux kernel code lists. */

/* struct dllist_head is defined in <lwp/lock.h> */
#include <lwp/lock.h>
#if 0
struct dllist_head {
	struct dllist_head *next, *prev;
};
#endif

#define dllist_entry(ptr, type, member) \
	((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))

#define dllist_init(head) do { \
    struct dllist_head *h = head; \
    h->prev = h->next = h; } while(0);

#define dllist_add(entry, head) do { \
    struct dllist_head *h = head, *e = entry; \
    h->next->prev = e; \
    e->next = h->next; \
    e->prev = h; \
    h->next = e; } while(0);

#define dllist_del(entry) do { \
    struct dllist_head *e = entry; \
    e->prev->next = e->next; \
    e->next->prev = e->prev; \
    e->prev = e->next = e; } while(0);

#define dllist_empty(head) ((head)->next == head)

#endif /* _DLLIST_H_ */

