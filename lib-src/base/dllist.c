#include "dllist.h"

inline void list_head_init(struct dllist_head *ptr)
{
	ptr->next = ptr;
	ptr->prev = ptr;
}

/*
 * Insert a new entry after the specified head..
 */
inline void list_add(struct dllist_head *entry, struct dllist_head *head)
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

inline void list_del(struct dllist_head *entry)
{
	entry->prev->next = entry->next;
	entry->next->prev = entry->prev;
	entry->prev = entry;
	entry->next = entry;
}

inline int list_empty(struct dllist_head *head)
{
	return head->next == head;
}

