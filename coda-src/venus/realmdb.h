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

#ifndef _REALMDB_H_
#define _REALMDB_H_

#include "persistent.h"
#include "realm.h"

/* special realm used for local 'fake' volumes */
#define LOCALREALM "localhost"
extern Realm *LocalRealm;

/* persistent reference to the RealmDB object */
#define REALMDB (rvg->recov_REALMDB)

class RealmDB : protected PersistentObject {
    friend void RealmDBInit(void);
    friend class fsobj; // Fakeify

public:
    RealmDB(void);
    ~RealmDB(void);

    void ResetTransient(void);

    Realm *GetRealm(const char *realm);
    Realm *GetRealm(const RealmId realmid);

    void GetDown(void);

    void print(FILE *f);
    void print(void) { print(stdout); }

private:
    struct dllist_head realms;
};

void RealmDBInit(void);

#endif /* _REALMDB_H_ */

