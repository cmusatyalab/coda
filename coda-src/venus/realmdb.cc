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

#include "realmdb.h"

/* This is initialized by RealmDBInit() */
Realm *LocalRealm;

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

    for(p = realms.next; p != &realms;) {
	realm = list_entry(p, Realm, realms);
	p = p->next;
	realm->ResetTransient();
    }
}

Realm *RealmDB::GetRealm(const char *realmname)
{
    struct dllist_head *p;
    Realm *realm;

    if (realmname[0] == '\0')
	realmname = default_realm;

    CODA_ASSERT(strlen(realmname) <= MAXHOSTNAMELEN);

    list_for_each(p, realms) {
	realm = list_entry(p, Realm, realms);
	if (STREQ(realm->Name(), realmname)) {
	    realm->GetRef();
	    return realm;
	}
    }

    Recov_BeginTrans();
    realm = new Realm(realmname);
    rec_list_add(&realm->realms, &realms);
    Recov_EndTrans(0);

    return realm;
}

Realm *RealmDB::GetRealm(const RealmId realmid)
{
    struct dllist_head *p;
    Realm *realm;

    list_for_each(p, realms) {
	realm = list_entry(p, Realm, realms);
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
	realm = list_entry(p, Realm, realms);
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

    LocalRealm = REALMDB->GetRealm(LOCALREALM);
}

int FID_IsLocalFake(VenusFid *fid)
{
    return (fid->Realm == LocalRealm->Id());
}

