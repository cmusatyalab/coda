#ifndef _DLIST_H
#define _DLIST_H


/*
 * doubly linked list implementation -- based on linux 
 * kernel code lists.
 *
 */

struct dllist_head {
	struct dllist_head *next, *prev;
};
#define dllist_chain dllist_head


#define list_entry(ptr, type, member) \
	((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))

#endif

inline int list_empty(struct dllist_head *head);
inline void list_del(struct dllist_head *entry);
inline void list_add(struct dllist_head *entry, struct dllist_head *head);
inline void list_head_init(struct dllist_head *ptr);

