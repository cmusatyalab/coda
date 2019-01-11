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

#ifndef _REALMDB_H_
#define _REALMDB_H_

#include <rvmlib.h>
#include "realm.h"

/* special realm used for local 'fake' volumes */
#define LOCALREALM "localhost"
extern Realm *LocalRealm;

/* persistent reference to the RealmDB object */
#define REALMDB (rvg->recov_REALMDB)

class RealmDB {
    friend void RealmDBInit(void);
    friend class fsobj; // Fakeify

public:
    /* MUST be called from within a transaction */
    void *operator new(size_t size)
    {
        void *p = rvmlib_rec_malloc(size);
        CODA_ASSERT(p);
        return p;
    }

    /* MUST be called from within a transaction */
    void operator delete(void *p) { rvmlib_rec_free(p); }

    /* MUST be called from within a transaction */
    RealmDB(void);
    ~RealmDB(void);

    /* MAY be called from within a transaction */
    void ResetTransient(void);

    Realm *GetRealm(const char *realm);
    Realm *GetRealm(const RealmId realmid);

    /* MUST NOT be called from within a transaction */
    void GetDown(void);

    void print(FILE *f);
    void print(void) { print(stdout); }

private:
    struct dllist_head realms;
    RealmId max_realmid; /*T*/
};

void RealmDBInit(void);

#endif /* _REALMDB_H_ */
