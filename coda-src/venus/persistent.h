/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 2002 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#ifndef _PERSISTENT_H_
#define _PERSISTENT_H_

#include <rvmlib.h>
#include <venusrecov.h>
#include <coda_assert.h>
#include "refcounted.h"
#include "rec_dllist.h"

class PersistentObject : protected RefCountedObject {
private:
    int rec_refcount;

public:
    struct dllist_head list;

    void *operator new(size_t size)
    {
	void *p = rvmlib_rec_malloc(size);
	CODA_ASSERT(p);
	return p;
    }

    void operator delete(void *p, size_t size)
    {
	rvmlib_rec_free(p);
    }

    PersistentObject(struct dllist_head *head = NULL)
    {
	if (!head)
	    rec_list_head_init(&list);
	else
	    rec_list_add(&list, head);

	RVMLIB_REC_OBJECT(rec_refcount);
	rec_refcount = 0;
    }

    ~PersistentObject(void)
    {
	CODA_ASSERT(!rec_refcount);
	Recov_BeginTrans();
	rec_list_del(&list);
	Recov_EndTrans(0);
    }
	
    void ResetTransient(void)
    {
	refcount = rec_refcount;
	/* no RVM references anymore, delayed destruction */
	if (!refcount)
	    delete this;
    }

    void Rec_GetRef(void)
    {
	/* Assume we already have a 'volatile' refcount on this object */
	CODA_ASSERT(refcount > rec_refcount);
	RVMLIB_REC_OBJECT(rec_refcount);
	rec_refcount++;
    }

    void Rec_PutRef(void)
    {
	CODA_ASSERT(rec_refcount);
	RVMLIB_REC_OBJECT(rec_refcount);
	rec_refcount--;
    }
};

#endif /* _PERSISTENT_H_ */

