/*
** The "list" data structure.
*/

#ifndef _DS_LIST_H_
#define _DS_LIST_H_

#include <odytypes.h>

/* 
   Lists are an ordered set of elements of type T*.
   They are generic containers.  The only restriction on elements
   is that they not be NULL.
   They are not thread safe.
   They can be ordered or not.
   Ordered lists contain an sense of "orderedness", called order(i,j)
      order(i,j) should form a total order, with equivalences
                0 if i and j are "equivalent"
               <0 if i should be before j
               >0 if i should be after j
   The "orderfulness" of a list affects it's mutators
   
   Lists can be "safe" or "unsafe".  Safe lists cannot be destroyed
   unless they are empty.

   Ordered lists can contain "duplicates," or "duplicates" can be
   forbidden.  If the list is unordered, two elements i,j are
   "duplicates" iff they are pointer-equal.  If the list is ordered,
   two elements i,j are "duplicates" if order(i,j) == 0.

   All list clients are responsible for allocation and deallocation of
   list items.
*/

typedef struct ds_list_t ds_list_t;            /* opaque list */

/*
   The following operations are allowed on lists:
*/

/*** Observers ***/

/* 
   ds_list_valid returns TRUE if l has a valid list magic number.
   ds_list_count returns the number of items in l.
   ds_list_first returns the first element of l, or NULL if l is empty.
   ds_list_first returns the last element of l, or NULL if l is empty.
   ds_list_member returns e if l contains the argument e, NULL otherwise.  
                  If the list is sorted, the ordering function is used to
		  test equality.  If the list is unsorted, pointer-equality
		  of elements is used.
*/

extern bool  ds_list_valid (ds_list_t *l);   /* TRUE => looks like a list */
extern int   ds_list_count (ds_list_t *l);   /* number of elements in list */
extern void *ds_list_first (ds_list_t *l);   /* return head element */
extern void *ds_list_last  (ds_list_t *l);   /* return tail element */
extern void *ds_list_member(ds_list_t *l,    /* is e in l? */
			     void *e); 

/*** Mutators ***/


/*
   ds_list_create:  create a new list
                    c is the comparison function for this list.
		        If c is NULL, the list is said to be "unordered"
   		        If c is non-null, c should point to a valid comparison
			function; it's a good idea for c to dynamically 
			typecheck it's arguments.
		    safe_destroy == FALSE; okay to delete a list without
		        checking to see that it is empty first.
		    dups_ok == TRUE; okay to insert duplicates.

   ds_list_destroy: destroy the list.  Asserts if list is safe and nonempty.
   ds_list_insert:  If l is unordered, insert i in l.
                    If l is ordered, inserts i s.t. all elements j
			before i in l obey order(j,i) < 0.
                    returns it's argument if successful, or
		        NULL if list is a no-dup list, and i would be a dup.
   ds_list_append:  If l is unordered, append i to l.
                    If l is ordered, inserts i s.t. all elements j
                        after i in l obey order(i,j) < 0.
                    returns it's argument if successful, or
		        NULL if list is a no-dup list, and i would be a dup.
   ds_list_get_first: Remove and return first element in l, or NULL if empty.
   ds_list_get_first: Remove and return last element in l, or NULL if empty.
   ds_list_remove:    Remove the element denoted by it's argument.  
                      The comparison function is used to test equality if
		      the list is sorted.  If the list is unsorted, pointer
		      equality is used.
		      Returns its argument if successful, NULL if p not on l.
*/
		    
extern ds_list_t     *ds_list_create    (COMPFN c, 
					 bool safe_destroy,
					 bool dups_ok); 
extern void           ds_list_destroy   (ds_list_t *l);
extern void          *ds_list_insert    (ds_list_t *l, void *i);
extern void          *ds_list_append    (ds_list_t *l, void *i);
extern void          *ds_list_get_first (ds_list_t *l);
extern void          *ds_list_get_last  (ds_list_t *l);
extern void          *ds_list_remove    (ds_list_t *l, void *p);
extern void           ds_list_print     (ds_list_t *l,
					 bool forward,
					 void (*printer)(void*));

/*** Iterators ***/

typedef struct ds_list_iter_t ds_list_iter_t;     /* opaque */

/* 
   You can create an interator, destroy an iterator, 
   or ask for the "next" element in the sequence.  Iterators and
   lists communicate with one another: if an iterator's "next" element
   is removed from the iterator's list, the iterator will be advanced
   one element.  Once an iterator has reached the end of the list (i.e.
   ds_list_iter_next returns NULL) it is considered closed: new items
   added to the end of the list will not be picked up by the iterator.
   New items added before an iterator's concpetion of "next" will also
   not be picked up by the iterator.  Frankly, twiddling the list while
   the iterator is hooked up is kinda silly anyway.
*/

extern ds_list_iter_t *ds_list_iter_create  (ds_list_t *l);
extern void            ds_list_iter_destroy (ds_list_iter_t *i);
extern void           *ds_list_iter_next    (ds_list_iter_t *i);

#endif /* _DS_LIST_H_ */
