#ifndef _REC_DLLIST_H_
#define _REC_DLLIST_H_

#include <dllist.h>
#include <rvmlib.h>

/* persistent dllist helpers */

static inline void rec_list_head_init(struct dllist_head *p)
{
    RVMLIB_REC_OBJECT(*p);
    list_head_init(p);
}

static inline void rec_list_add(struct dllist_head *p, struct dllist_head *h)
{
    RVMLIB_REC_OBJECT(*p);
    RVMLIB_REC_OBJECT(h->next);
    RVMLIB_REC_OBJECT(h->next->prev);
    list_add(p, h);
}

static inline void rec_list_del(struct dllist_head *p)
{
    RVMLIB_REC_OBJECT(*p);
    if (!list_empty(p)) {
	RVMLIB_REC_OBJECT(p->next->prev);
	RVMLIB_REC_OBJECT(p->prev->next);
    }
    list_del(p);
}

#endif /* _REC_DLLIST_H_ */

