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


#define LIST_HEAD(name) \
	struct dllist_head name = { &name, &name }

#define INIT_LIST_HEAD(ptr) do { \
	(ptr)->next = (ptr); (ptr)->prev = (ptr); \
} while (0)


/*
 * Insert a new entry after the specified head..
 */
static __inline__ void list_add(struct dllist_head *entry, struct dllist_head *head)
{
	head->next->prev = entry;
	entry->next = head->next;
	entry->prev = head;
	head->next = entry;
}

/*
 * Delete a list entry by making the prev/next entries
 * point to each other.
 */

static __inline__ void list_del(struct dllist_head *entry)
{
	entry->prev->next = entry->next;
	entry->next->prev = entry->prev;
}

static __inline__ int list_empty(struct dllist_head *head)
{
	return head->next == head;
}


#define list_entry(ptr, type, member) \
	((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))

#endif
