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

/*
 * Base class that implements persistent reference counted objects. Useable for
 * VM objects that can be referenced from different places and should be
 * automatically destroyed when the last reference is dropped.
 */

#ifndef _PERSISTENT_H_
#define _PERSISTENT_H_

#include <rvmlib.h>
#include <venusrecov.h>
#include <coda_assert.h>
#include "rec_dllist.h"

class PersistentObject {
protected:
     unsigned int rec_refcount;
/*T*/unsigned int refcount;

public:
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

    PersistentObject(void)
    {
	RVMLIB_REC_OBJECT(rec_refcount);
	rec_refcount = 0;
	refcount = 1;
    }

    virtual ~PersistentObject(void)
    {
	CODA_ASSERT(!rec_refcount && refcount <= 1);
    }
	
    void ResetTransient(void)
    {
	refcount = 0;
	/* no RVM references anymore, delayed destruction */
	if (!rec_refcount)
	    delete this;
    }

    void Rec_GetRef(void)
    {
	/* Assume we already have a 'volatile' refcount on this object */
	RVMLIB_REC_OBJECT(rec_refcount);
	rec_refcount++;
    }

    void Rec_PutRef(void)
    {
	CODA_ASSERT(rec_refcount);
	RVMLIB_REC_OBJECT(rec_refcount);
	rec_refcount--;
	if (!refcount && !rec_refcount)
	    delete this;
    }

    void GetRef(void)
    {
	refcount++;
    }

    void PutRef(void) {
	CODA_ASSERT(refcount);
	refcount--;
	/*
	 * Only destroy the object if we happen to be in a transaction,
	 * otherwise we'll destroy ourselves later during ResetTransient,
	 * or when a reference is regained and then dropped in a transaction.
	 */
	if (rvmlib_in_transaction()) {
	    if (!refcount && !rec_refcount)
		delete this;
	}
    }
};

#endif /* _PERSISTENT_H_ */

