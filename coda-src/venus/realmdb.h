#ifndef _REALMDB_H_
#define _REALMDB_H_

#include "persistent.h"

class Realm;

class RealmDB : protected PersistentObject {
    friend void RealmDBInit(void);

public:
    RealmDB(void);
    ~RealmDB(void);

    void ResetTransient(void);

    Realm *GetRealm(const char *realm);

    void print(FILE *f);
    void print(void) { print(stdout); }

private:
    struct dllist_head realms;
};

void RealmDBInit(void);
#define REALMDB (rvg->recov_REALMDB)

#endif /* _REALMDB_H_ */

