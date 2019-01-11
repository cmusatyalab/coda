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

/*
 * Base class that implements refcounting. Useable for VM objects that can be
 * referenced from different places and should be automatically destroyed when
 * the last reference is dropped
 */

#ifndef _REFCOUNTED_H_
#define _REFCOUNTED_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <assert.h>

#ifdef __cplusplus
}
#endif

class RefCountedObject {
protected:
    unsigned int refcount;

    /* Creation grabs an implicit reference */
    RefCountedObject() { refcount = 1; }

    /* Deletion is ok when the refcount is either 0 or 1.
     * i.e. when it is one `delete' is similar to `PutRef', but PutRef is
     * still preferred */
    virtual ~RefCountedObject() { assert(refcount <= 1); }

public:
    /* Grab a reference to the object */
    void GetRef(void) { refcount++; }

    /* Put a reference, destroying the object when the last reference is put */
    void PutRef(void)
    {
        assert(refcount > 0);
        if (!(--refcount))
            delete this;
    }

    /* Print the current reference count */
    void PrintRef(FILE *f) { fprintf(f, "\trefcount %u\n", refcount); }
};

#ifdef TESTING
/* some code to test the functionality of a RefCountedObject */
class RefObj : public RefCountedObject {
public:
    RefObj() { printf("Creating RefObj\n"); };
    ~RefObj() { printf("Destroying RefObj\n"); };
};

void main(void)
{
    RefObj *test = new RefObj;

    printf("New object refcnt should be 1\n");
    test->PrintRef(stdout);
    test->GetRef();
    printf("Got a reference refcnt should be 2\n");
    test->PrintRef(stdout);
    test->PutRef();
    printf("Dropped a reference refcnt should be 1\n");
    test->PrintRef(stdout);
    test->PutRef();
    printf("Dropped a reference Object should be destroyed\n");
    printf("Done");
}
#endif
#endif /* _REFCOUNTED_H_ */
