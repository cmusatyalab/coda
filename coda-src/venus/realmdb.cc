#include "realmdb.h"
#include "realm.h"

RealmDB::RealmDB(void)
{
    RVMLIB_REC_OBJECT(*this);
    rec_list_head_init(&realms);
}

RealmDB::~RealmDB(void)
{
    CODA_ASSERT(list_empty(&realms));
    abort(); /* we really shouldn't be destroying this object */
}

void RealmDB::ResetTransient(void)
{
    struct dllist_head *p;
    Realm *realm;

    PersistentObject::ResetTransient();

    list_for_each(p, realms) {
	realm = (Realm *)list_entry(p, PersistentObject, list);
	realm->ResetTransient();
    }
}

Realm *RealmDB::GetRealm(const char *realmname)
{
    struct dllist_head *p;
    Realm *realm;

    list_for_each(p, realms) {
	realm = (Realm *)list_entry(p, PersistentObject, list);
	if (strcmp(realm->Name(), realmname) == 0) {
	    realm->GetRef();
	    return realm;
	}
    }

    Recov_BeginTrans();
    realm = new Realm(realmname, &realms);
    Recov_EndTrans(0);

    return realm;
}

Realm *RealmDB::GetRealm(const RealmId realmid)
{
    struct dllist_head *p;
    Realm *realm;

    list_for_each(p, realms) {
	realm = (Realm *)list_entry(p, PersistentObject, list);
	if (realm->Id() == realmid) {
	    realm->GetRef();
	    return realm;
	}
    }
    return NULL;
}

void RealmDB::print(FILE *f)
{
    struct dllist_head *p;
    Realm *realm;

    fprintf(f, "*** BEGIN RealmDB ***\n");

    list_for_each(p, realms) {
	realm = (Realm *)list_entry(p, PersistentObject, list);
	realm->print(f);
    }

    fprintf(f, "*** END RealmDB ***\n");
}


void RealmDBInit(void)
{
    if (InitMetaData) {
	Recov_BeginTrans();
	RVMLIB_REC_OBJECT(REALMDB);
	REALMDB = new RealmDB;
	REALMDB->Rec_GetRef();
	Recov_EndTrans(0);
    }
}

