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

#include "dllist.h"

void list_head_init(struct dllist_head *ptr)
{
	ptr->next = ptr;
	ptr->prev = ptr;
}

/*
 * Insert a new entry after the specified head..
 */
void list_add(struct dllist_head *entry, struct dllist_head *head)
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
void list_del(struct dllist_head *entry)
{
	entry->prev->next = entry->next;
	entry->next->prev = entry->prev;
	entry->prev = entry;
	entry->next = entry;
}

int list_empty(struct dllist_head *head)
{
	return head->next == head;
}

