#ifndef _REALM_H_
#define _REALM_H_

#include "persistent.h"

class Server;

class Realm : protected PersistentObject {
    friend class RealmDB;

public:
//    vdb   *VDB;
///*T*/vsgdb *VSGDB;
    u_int32_t id;

    Realm(const char *realm, struct dllist_head *h);
    ~Realm(void);

    void ResetTransient(void);
    void PutRef(void) { PersistentObject::PutRef(); }

    Server *GetServer(struct in_addr *ipv4addr);
//    volent *GetVolume(const char *volname);

    const char *Name(void) { return name; }
    void print(FILE *f);

private:
    char *name;
    struct dllist_head realms;

/*T*/struct dllist_head servers;
};

#endif /* _REALM_H_ */

