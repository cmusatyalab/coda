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

#ifndef _REALM_H_
#define _REALM_H_

#include "persistent.h"

class connent;

class Realm : public PersistentObject {
    friend class RealmDB;
    friend class fsobj; // Fakeify

public:
    Realm(const char *realm);
    ~Realm(void);

    void ResetTransient(void);

    int GetAdmConn(connent **cpp);

    const char *Name(void) { return name; }
    const RealmId Id(void) { return (RealmId)this; }
    void print(FILE *f);

private:
    char *name;
    struct dllist_head realms;

/*T*/struct coda_addrinfo *rootservers;
};

#endif /* _REALM_H_ */

