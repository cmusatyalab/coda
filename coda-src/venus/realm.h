#ifndef _REALM_H_
#define _REALM_H_

#include "persistent.h"

class Server;
class connent;

class Realm : protected PersistentObject {
    friend class RealmDB;

public:
//    vdb   *VDB;
///*T*/vsgdb *VSGDB;

    Realm(const char *realm, struct dllist_head *h);
    ~Realm(void);

    void ResetTransient(void);

    void GetRef(void) { PersistentObject::GetRef(); }
    void PutRef(void) { PersistentObject::PutRef(); }

    void Rec_GetRef(void) { PersistentObject::Rec_GetRef(); }
    void Rec_PutRef(void) { PersistentObject::Rec_PutRef(); }

    int GetAdmConn(connent **cpp);

    Server *GetServer(struct in_addr *ipv4addr);
//    volent *GetVolume(const char *volname);

    const char *Name(void) { return name; }
    const RealmId Id(void) { return id; }
    void print(FILE *f);

private:
    RealmId id;
    char *name;

/*T*/struct in_addr *rootservers;
/*T*/struct dllist_head servers;
};

#endif /* _REALM_H_ */

