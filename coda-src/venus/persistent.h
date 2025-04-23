/* BLURB gpl

                           Coda File System
                              Release 8

          Copyright (c) 2003-2025 Carnegie Mellon University
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

#error "Only use this as a template!"
/* RVM doesn't cooperate nicely with inheritance and virtual functions */

#include <rvmlib.h>
#include <venusrecov.h>
#include <coda_assert.h>

class PersistentObject {
public:
    void *operator new(size_t size) REQUIRES_TRANSACION
    {
        void *p = rvmlib_rec_malloc(size);
        CODA_ASSERT(p);
        return p;
    }

    void operator delete(void *p) REQUIRES_TRANSACION { rvmlib_rec_free(p); }

    PersistentObject(void) REQUIRES_TRANSACION
    {
        RVMLIB_REC_OBJECT(rec_refcount);
        rec_refcount = 0;
        refcount     = 1;
    }

    virtual ~PersistentObject(void)
    {
        CODA_ASSERT(!rec_refcount && refcount <= 1);
    }

    virtual void ResetTransient(void) TRANSACTION_OPTIONAL
    {
        refcount = 0;
        /* If there are no RVM references anymore, delayed destruction. */
        if (rvmlib_in_transaction() && !rec_refcount)
            delete this;
    }

    void Rec_GetRef(void) REQUIRES_TRANSACION
    {
        RVMLIB_REC_OBJECT(rec_refcount);
        rec_refcount++;
    }

    virtual void Rec_PutRef(void) REQUIRES_TRANSACION
    {
        CODA_ASSERT(rec_refcount);
        RVMLIB_REC_OBJECT(rec_refcount);
        rec_refcount--;
        if (!refcount && !rec_refcount)
            delete this;
    }

    void GetRef(void) { refcount++; }

    virtual void PutRef(void) TRANSACTION_OPTIONAL
    {
        CODA_ASSERT(refcount);
        refcount--;
        /*
	 * Only destroy the object if we happen to be in a transaction,
	 * otherwise we'll destroy ourselves later during ResetTransient,
	 * or when a reference is regained and then dropped in a transaction.
	 */
        if (rvmlib_in_transaction() && !refcount && !rec_refcount)
            delete this;
    }

private:
    unsigned int rec_refcount;
    /*T*/ unsigned int refcount;
};

#endif /* _PERSISTENT_H_ */
