/* BLURB gpl

                           Coda File System
                              Release 7

          Copyright (c) 2019 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#include "realmdb.h"
#include "fso.h"
#include "rec_dllist.h"
#include "codaconf.h"

/* This is initialized by RealmDBInit() */
Realm *LocalRealm;

RealmDB::RealmDB(void)
{
    RVMLIB_REC_OBJECT(*this);
    rec_list_head_init(&realms);
    max_realmid = 0;
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
    int nrealms = 0;

    eprint("Starting RealmDB scan");

    for (p = realms.next; p != &realms;) {
        realm = list_entry_plusplus(p, Realm, realms);
        p     = p->next;
        realm->ResetTransient();
        if (realm->Id() > max_realmid)
            max_realmid = realm->Id();
    }

    LocalRealm = GetRealm(LOCALREALM);

    list_for_each(p, realms) nrealms++;

    eprint("\tFound %d realms", nrealms);
}

Realm *RealmDB::GetRealm(const char *realmname)
{
    struct dllist_head *p;
    Realm *realm;

    if (!realmname || realmname[0] == '\0')
        realmname = "UNKNOWN";

    CODA_ASSERT(strlen(realmname) <= MAXHOSTNAMELEN);

    list_for_each(p, realms)
    {
        realm = list_entry_plusplus(p, Realm, realms);
        if (STREQ(realm->Name(), realmname)) {
            realm->GetRef();
            return realm;
        }
    }

    Recov_BeginTrans();
    realm = new Realm(++max_realmid, realmname);
    rec_list_add(&realm->realms, &realms);
    Recov_EndTrans(0);

    return realm;
}

Realm *RealmDB::GetRealm(const RealmId realmid)
{
    struct dllist_head *p;
    Realm *realm;

    list_for_each(p, realms)
    {
        realm = list_entry_plusplus(p, Realm, realms);
        if (realm->Id() == realmid) {
            realm->GetRef();
            return realm;
        }
    }
    return NULL;
}

/* MUST NOT be called from within a transaction */
void RealmDB::GetDown(void)
{
    struct dllist_head *p;
    Realm *realm;

    for (p = realms.next; p != &realms;) {
        realm = list_entry_plusplus(p, Realm, realms);
        p     = p->next;

        if (!realm->refcount && !realm->rec_refcount) {
            Recov_BeginTrans();
            realm->Rec_GetRef();
            realm->Rec_PutRef();
            Recov_EndTrans(MAXFP);
        }
    }
}

void RealmDB::print(FILE *f)
{
    struct dllist_head *p;
    Realm *realm;

    fprintf(f, "*** BEGIN RealmDB ***\n");

    list_for_each(p, realms)
    {
        realm = list_entry_plusplus(p, Realm, realms);
        realm->print(f);
    }

    fprintf(f, "*** END RealmDB ***\n");
}

static void RealmDB_GetDown(void)
{
    REALMDB->GetDown();
}

void RealmDBInit(void)
{
    static bool InitMetaData = GetVenusConf().get_bool_value("initmetadata");
    const char *dummy        = NULL;

    /* lib-src/base/parse_realms.c depends on us and this value to be set to
       the legacy codaconf db */
    CODACONF_STR(dummy, "realmtab", GetVenusConf().get_value("realmtab"));
    dummy = NULL;
    CODACONF_STR(dummy, "realm", GetVenusConf().get_value("realm"));

    if (InitMetaData) {
        Recov_BeginTrans();
        RVMLIB_REC_OBJECT(REALMDB);
        REALMDB = new RealmDB;
        Recov_EndTrans(0);
    }
    REALMDB->ResetTransient();

    /* clean up unreferenced realms about once every 15 minutes */
    FireAndForget("RealmDaemon", RealmDB_GetDown, 15 * 60);
}

int FID_IsLocalFake(VenusFid *fid)
{
    return (fid->Realm == LocalRealm->Id());
}
