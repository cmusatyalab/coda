/*
** ds_list.c: implementation of ds_list_t.
*/

#include <stdlib.h>

#include <odytypes.h>

#include "ds_list.h"
#include "ds_list.private.h"

/* magic numbers */

const magic_t ds_list_magic = 257652478;
const magic_t ds_list_elt_magic = 15206031;
const magic_t ds_list_iter_magic = 195588386;

/* list elements */

static
ds_list_elt_t *
ds_list_elt_create(void *contents) {
    ds_list_elt_t *result;

    ALLOC(result,ds_list_elt_t);
    result->magic = ds_list_elt_magic;
    result->p = result->n = NULL;
    result->contents = contents;
    return result;
}

static
void
ds_list_elt_destroy(ds_list_elt_t *e) {
    CODA_ASSERT(DS_LIST_ELT_VALID(e));
    e->p = e->n = NULL;
    e->contents = NULL;
    e->magic = 0;
    FREE(e);
}

/* ds_list_t's */

bool
ds_list_valid(ds_list_t *l) {
    if (DS_LIST_VALID(l)) 
	return TRUE;
    else
	return FALSE;
}

int
ds_list_count(ds_list_t *l) {
    CODA_ASSERT(DS_LIST_VALID(l));
    return l->count;
}

void *
ds_list_first(ds_list_t *l) {
    CODA_ASSERT(DS_LIST_VALID(l));
    if (l->head) {
	CODA_ASSERT(DS_LIST_ELT_VALID(l->head));
	return l->head->contents;
    } else {
	return NULL;
    }
}

void *
ds_list_last(ds_list_t *l) {
    CODA_ASSERT(DS_LIST_VALID(l));
    if (l->tail) {
	CODA_ASSERT(DS_LIST_ELT_VALID(l->tail));
	return l->tail->contents;
    } else {
	return NULL;
    }
}

static ds_list_elt_t *
ds_list_find_member(ds_list_t *l, void *e) {
    ds_list_elt_t *cur;
    bool           found = FALSE;

    /* calling functions must test validity of l */
    cur = l->head;

    while (!found && cur != NULL) {
	CODA_ASSERT(DS_LIST_ELT_VALID(cur));
	/* test is different for sorted, unsorted lists */
	if (l->cmpfn != NULL) {
	    if (l->cmpfn(e,cur->contents) == 0) {
		found = TRUE;
	    } else {
		cur = cur->n;
	    }
	} else {
	    if (e == cur->contents) {
		found = TRUE;
	    } else {
		cur = cur->n;
	    }
	}
    }
    return cur;
}

void *
ds_list_member(ds_list_t *l, void *e) {
    ds_list_elt_t *elt;

    CODA_ASSERT(DS_LIST_VALID(l));    
    CODA_ASSERT(e != NULL);
    elt = ds_list_find_member(l,e);
    if (elt)
	return elt->contents;
    else
	return NULL;
}


ds_list_t *
ds_list_create(COMPFN c,
	       bool   safe_destroy,
	       bool   dups_ok) {
    ds_list_t *result;

    ALLOC(result,ds_list_t);
    result->magic = ds_list_magic;

    result->cmpfn = c;
    result->is_safe = safe_destroy;
    result->has_dups= dups_ok;
    result->count = 0;
    result->head = result->tail = NULL;
    result->iter_list = NULL;

    return result;
}

void
ds_list_destroy(ds_list_t *l) {
    CODA_ASSERT(DS_LIST_VALID(l));

    /* Either the list must be unsafe, or there must be nothing in it. */
    CODA_ASSERT(!(l->is_safe) || (l->count == 0));

    l->magic = 0;
    l->cmpfn = NULL;
    l->head = l->tail = NULL;
    l->count = 0;

    FREE(l);
}

void *
ds_list_insert(ds_list_t *l, void *i) {
    ds_list_elt_t *result;

    CODA_ASSERT(DS_LIST_VALID(l));
    CODA_ASSERT(i != NULL);

    /* test for duplicates */
    if (!l->has_dups) 
	if (ds_list_find_member(l,i))
	    return NULL;

    result = ds_list_elt_create(i);

    /* Is the list empty? */
    if (l->head == NULL) {
	l->head = result;
	l->tail = result;
    } else {
	/* Is the list unsorted? */
	if (l->cmpfn == NULL) {
	    CODA_ASSERT(DS_LIST_ELT_VALID(l->head));
	    result->n = l->head;
	    l->head->p = result;
	    l->head = result;
	} else {
	    /* Walk through the list, checking for the insertion point */
	    ds_list_elt_t *cur = l->head;
	    while (cur != NULL
		   && DS_LIST_ELT_VALID(cur)
		   && l->cmpfn(cur->contents,result->contents) < 0) 
	    {
		cur = cur->n;
	    }
	    if (cur == NULL) {  /* new element at end of list */
		result->p = l->tail;
		l->tail->n = result;
		l->tail = result;
	    } else {
		CODA_ASSERT(DS_LIST_ELT_VALID(cur));
		result->n = cur;
		result->p = cur->p;
		result->n->p = result;
		if (result->p) 
		    result->p->n = result;
		else                        /* new element at beginning */
		    l->head = result;
	    }
	}
    }
    l->count++;
    return i;
}

void *
ds_list_append(ds_list_t *l, void *i) {
    ds_list_elt_t *result;

    CODA_ASSERT(DS_LIST_VALID(l));
    CODA_ASSERT(i != NULL);

    /* test for duplicates */
    if (!l->has_dups) 
	if (ds_list_find_member(l,i))
	    return NULL;

    result = ds_list_elt_create(i);

    /* Is the list empty? */
    if (l->head == NULL) {
	l->head = result;
	l->tail = result;
    } else {
	/* Is the list unsorted? */
	if (l->cmpfn == NULL) {
	    CODA_ASSERT(DS_LIST_ELT_VALID(l->tail));
	    result->p = l->tail;
	    l->tail->n = result;
	    l->tail = result;
	} else {
	    /* Walk backwards, checking for the insertion point */
	    ds_list_elt_t *cur = l->tail;
	    while (cur != NULL
		   && DS_LIST_ELT_VALID(cur)
		   && l->cmpfn(result->contents,cur->contents) < 0)
	    {
		cur = cur->p;
	    }
	    if (cur == NULL) { /* new element at beginning of list */
		result->n = l->head;
		l->head->p = result;
		l->head = result;
	    } else {
		CODA_ASSERT(DS_LIST_ELT_VALID(cur));
		result->p = cur;
		result->n = cur->n;
		result->p->n = result;
		if (result->n) 
		    result->n->p = result;
		else                          /* new element at end of list */
		    l->tail = result;
	    }
	}
    }
    l->count++;
    return i;
}


/* helper function to advance iters whose next fields point to
   dying elements.  "dropping" must not have been pulled off of 
   the list when this is called. */

static void
ds_list_advance_iters(ds_list_elt_t *dropping, 
		      ds_list_iter_t *iter) {
    
    while (iter != NULL) {
	CODA_ASSERT(DS_LIST_ITER_VALID(iter));
	if (iter->next_elt == dropping) 
	    iter->next_elt = iter->next_elt->n;
	iter = iter->next_iter;
    }
}

void *
ds_list_get_first(ds_list_t *l) {
    void *result = NULL;
    ds_list_elt_t *removed = NULL;

    CODA_ASSERT(DS_LIST_VALID(l));
    if (l->head != NULL) {        /* if the list is not empty */
	removed = l->head;
	CODA_ASSERT(DS_LIST_ELT_VALID(removed));
	ds_list_advance_iters(removed,l->iter_list);
	result = l->head->contents;
	l->head = l->head->n;
	if (l->head == NULL) {
	    l->tail = NULL;      /* list is now empty */
	} else {
	    CODA_ASSERT(DS_LIST_ELT_VALID(l->head));
	    l->head->p = NULL;   /* unchain this element */
	}
	ds_list_elt_destroy(removed);
	l->count--;
    }
    return result;
}

void *
ds_list_get_last(ds_list_t *l) {
    void *result = NULL;
    ds_list_elt_t *removed = NULL;

    CODA_ASSERT(DS_LIST_VALID(l));
    if (l->tail != NULL) {     /* if the list is not empty */
	removed = l->tail;
	CODA_ASSERT(DS_LIST_ELT_VALID(removed));
	ds_list_advance_iters(removed,l->iter_list);
	result = l->tail->contents;
	l->tail = l->tail->p;
	if (l->tail == NULL) {
	    l->head = NULL;   /* list is now empty */
	} else {
	    CODA_ASSERT(DS_LIST_ELT_VALID(l->tail));
	    l->tail->n = NULL; /* unchain this element */
	}
	ds_list_elt_destroy(removed);
	l->count--;
    }
    return result;
}

void *
ds_list_remove(ds_list_t *l, void *e) {
    ds_list_elt_t *elt;
    void          *result;

    CODA_ASSERT(DS_LIST_VALID(l));    
    CODA_ASSERT(e != NULL);
    elt = ds_list_find_member(l,e);

    if (elt == NULL) return NULL;    /* not in list */

    ds_list_advance_iters(elt,l->iter_list);
    result = elt->contents;          /* save the answer */
    l->count--;

    if (elt->p == NULL && elt->n == NULL) {  /* only element */
	l->head = l->tail = NULL;
	ds_list_elt_destroy(elt);
	return result;
    }

    if (elt->p == NULL) {            /* first element */
	l->head = elt->n;
	CODA_ASSERT(DS_LIST_ELT_VALID(l->head));
	l->head->p = NULL;
	ds_list_elt_destroy(elt);
	return result;
    } 

    if (elt->n == NULL) {           /* last element */
	l->tail = elt->p;
	CODA_ASSERT(DS_LIST_ELT_VALID(l->tail));
	l->tail->n = NULL;
	ds_list_elt_destroy(elt);
	return result;
    }

    /* neither first nor last element */
    CODA_ASSERT(DS_LIST_ELT_VALID(elt->n));
    CODA_ASSERT(DS_LIST_ELT_VALID(elt->p));
    elt->p->n = elt->n;
    elt->n->p = elt->p;
    ds_list_elt_destroy(elt);
    return result;
}

void
ds_list_print(ds_list_t *l, bool forward, void (*printer)(void *)) {
    ds_list_elt_t *cur;

    CODA_ASSERT(DS_LIST_VALID(l));
    CODA_ASSERT(printer != NULL);

    if (forward) {
	cur = l->head;
	while (cur != NULL) {
	    printer(cur->contents);
	    cur = cur->n;
	}
    } else {
	cur = l->tail;
	while (cur != NULL) {
	    printer(cur->contents);
	    cur = cur->p;
	}
    }
}

ds_list_iter_t *
ds_list_iter_create(ds_list_t *l) {
    ds_list_iter_t *result;

    CODA_ASSERT(DS_LIST_VALID(l));
    ALLOC(result,ds_list_iter_t);

    result->next_iter = l->iter_list;
    result->next_elt = l->head;
    result->magic = ds_list_iter_magic;
    result->list = l;

    l->iter_list = result;

    return result;
}

void
ds_list_iter_destroy(ds_list_iter_t *i) {
    ds_list_iter_t *trail;
    ds_list_iter_t *cur;
    ds_list_t      *l;
    
    CODA_ASSERT(DS_LIST_ITER_VALID(i));
    l = i->list;
    CODA_ASSERT(DS_LIST_VALID(l));

    trail = NULL;
    cur = l->iter_list;
    
    while (cur != NULL) {
	if (cur == i) break;
	trail = cur;
	cur = cur->next_iter;
    }

    CODA_ASSERT(cur != NULL);
    if (trail == NULL) 
	l->iter_list = cur->next_iter;
    else
	trail->next_iter = cur->next_iter;

    i->magic = 0;
    i->list = NULL;
    i->next_elt = NULL;
    i->next_iter = NULL;
    FREE(i);
}

void *
ds_list_iter_next(ds_list_iter_t *i) {
    ds_list_t      *l;
    void           *result = NULL;

    CODA_ASSERT(DS_LIST_ITER_VALID(i));
    l = i->list;
    CODA_ASSERT(DS_LIST_VALID(l));

    if (i->next_elt) {
	CODA_ASSERT(DS_LIST_ELT_VALID(i->next_elt));
	result = i->next_elt->contents;
	i->next_elt = i->next_elt->n;
    }
    
    return result;
}
