/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 2003 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently
#*/

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
