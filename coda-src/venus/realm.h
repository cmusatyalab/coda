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

class Server;
class connent;

class Realm : protected PersistentObject {
    friend class RealmDB;

public:
    Realm(const char *realm);
    ~Realm(void);

    void ResetTransient(void);

    void GetRef(void) { PersistentObject::GetRef(); }
    void PutRef(void) { PersistentObject::PutRef(); }

    void Rec_GetRef(void) { PersistentObject::Rec_GetRef(); }
    void Rec_PutRef(void) { PersistentObject::Rec_PutRef(); }

    int GetAdmConn(connent **cpp);

    Server *GetServer(struct in_addr *ipv4addr);

    const char *Name(void) { return name; }
    const RealmId Id(void) { return (RealmId)this; }
    void print(FILE *f);

private:
    char *name;
    struct dllist_head realms;

/*T*/struct coda_addrinfo *rootservers;
/*T*/struct dllist_head servers;
};

#endif /* _REALM_H_ */

