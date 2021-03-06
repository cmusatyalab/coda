/* BLURB gpl

                           Coda File System
                              Release 8

          Copyright (c) 2003-2021 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef __cplusplus
}
#endif

#include "realm.h"
#include "realmdb.h"
#include "comm.h"
#include "user.h"
#include "parse_realms.h"
#include "rec_dllist.h"
#include "fso.h"

#define DEFAULT_ROOTVOLNAME "/"

/* MUST be called from within a transaction */
Realm::Realm(const RealmId id, const char *realm_name)
{
    RVMLIB_REC_OBJECT(realmid);
    realmid = id;

    RVMLIB_REC_OBJECT(name);
    name = rvmlib_rec_strdup(realm_name);
    CODA_ASSERT(name);

    RVMLIB_REC_OBJECT(rootvolname);
    rootvolname = rvmlib_rec_strdup(DEFAULT_ROOTVOLNAME);
    CODA_ASSERT(rootvolname);

    rec_list_head_init(&realms);

    /* we need a reference to prevent suicide in ResetTransient */
    RVMLIB_REC_OBJECT(rec_refcount);
    rec_refcount = 1;

    ResetTransient();

    /* and set the correct refcounts for the new object */
    refcount     = 1;
    rec_refcount = 0;
}

/* MUST be called from within a transaction */
Realm::~Realm(void)
{
    VenusFid Fid;
    fsobj *f;

    LOG(100, ("Realm::~Realm %s\n", name));

    CODA_ASSERT(!rec_refcount && refcount <= 1);

    rec_list_del(&realms);
    if (rootservers) {
        eprint("Removing realm '%s'", name);
        ReplaceRootServers();
    }

    /* remove the name entry from /coda */
    Fid.Realm  = LocalRealm->Id();
    Fid.Volume = FakeRootVolumeId;
    Fid.Vnode  = 1;
    Fid.Unique = 1;

    f = FSDB->Find(&Fid);
    if (f && f->dir_Lookup(name, &Fid, CLU_CASE_SENSITIVE) == 0)
        f->dir_Delete(name);

    rvmlib_rec_free(name);
    rvmlib_rec_free(rootvolname);

    /* kill the fake object that represents our mountlink */
    Fid.Vnode  = 0xfffffffc;
    Fid.Unique = Id();

    f = FSDB->Find(&Fid);
    if (f)
        f->Kill();
}

/* MAY be called from within a transaction */
void Realm::ResetTransient(void)
{
    rootservers = NULL;
    refcount    = 0;

    if (rvmlib_in_transaction() && !rec_refcount)
        delete this;
}

/* MUST be called from within a transaction */
void Realm::Rec_PutRef(void)
{
    CODA_ASSERT(rec_refcount);
    RVMLIB_REC_OBJECT(rec_refcount);
    rec_refcount--;
    if (!refcount && !rec_refcount)
        delete this;
}

/* MAY be called from within a transaction */
void Realm::PutRef(void)
{
    CODA_ASSERT(refcount);
    refcount--;

    return;

/* The following code is too aggressive at the moment. We end up killing a newly
 * created realm mount between the lookup and the getattr. -JH */
#if 0
    int intrans;

    if (refcount || rec_refcount)
	return;

    intrans = rvmlib_in_transaction();
    if (!intrans)
	Recov_BeginTrans();

    delete this;

    if (!intrans)
	Recov_EndTrans(MAXFP);
#endif
}

void Realm::ReplaceRootServers(struct RPC2_addrinfo *newsrvs)
{
    struct RPC2_addrinfo *p, *oldsrvs;
    struct sockaddr_in *sin;
    srvent *s;

    /* grab an extra reference count to pin down the new servers */
    for (p = newsrvs; p; p = p->ai_next) {
        if (p->ai_family != PF_INET)
            continue;

        sin = (struct sockaddr_in *)p->ai_addr;
        s   = ::GetServer(&sin->sin_addr, Id());
        s->GetRef();
        PutServer(&s);
    }

    oldsrvs     = rootservers;
    rootservers = newsrvs;

    /* drop the reference count on the old root servers */
    for (p = oldsrvs; p; p = p->ai_next) {
        if (p->ai_family != PF_INET)
            continue;

        sin = (struct sockaddr_in *)p->ai_addr;
        s   = ::GetServer(&sin->sin_addr, Id());
        s->PutRef();
        PutServer(&s);
    }
    RPC2_freeaddrinfo(oldsrvs);
}

/* Get a connection to any server (as root). */
/* MUST NOT be called from within a transaction */
int Realm::GetAdmConn(connent **cpp)
{
    struct RPC2_addrinfo *p, *tmp = NULL;
    struct sockaddr_in *sin;
    srvent *s;
    int code = 0, unknown, resolve;

    LOG(100, ("GetAdmConn: %s\n", name));

    if (STREQ(name, LOCALREALM))
        return ETIMEDOUT;

    *cpp = 0;

    unknown = resolve = !rootservers;
retry:
    if (resolve) {
        struct RPC2_addrinfo *newsrvs = NULL;
        GetRealmServers(name, "codasrv", &newsrvs);
        ReplaceRootServers(newsrvs);
        resolve = 0;
    } else {
        coda_reorder_addrinfo(&rootservers);
        /* our cached addresses might be stale, re-resolve if we end up not
	 * reaching any of the servers */
        resolve = 1;
    }
    tmp = RPC2_copyaddrinfo(rootservers);

    /* Get a connection to any custodian. */
    for (p = tmp; p; p = p->ai_next) {
        if (p->ai_family != PF_INET)
            continue;

        sin = (struct sockaddr_in *)p->ai_addr;

        s    = ::GetServer(&sin->sin_addr, Id());
        code = s->GetConn(cpp, ANYUSER_UID);
        PutServer(&s);

        switch (code) {
        case ERETRY:
            resolve = 1;
        case ETIMEDOUT:
            continue;

        case 0:
        case EINTR:
            /* We might have discovered a new realm */
            if (unknown) {
                VenusFid Fid;
                fsobj *f;

                eprint("Resolved realm '%s'", name);
                Fid.Realm  = LocalRealm->Id();
                Fid.Volume = FakeRootVolumeId;
                Fid.Vnode  = 1;
                Fid.Unique = 1;

                f = FSDB->Find(&Fid);
                if (f && f->dir_Lookup(name, &Fid, CLU_CASE_SENSITIVE) != 0) {
                    Recov_BeginTrans();

                    Fid.Vnode  = 0xfffffffc;
                    Fid.Unique = realmid;
                    f->dir_Create(name, &Fid);

                    Recov_EndTrans(MAXFP);
                }
            }
            goto exit_done;

        default:
            if (code < 0)
                eprint("GetAdmConn: bogus code (%d)", code);
            goto exit_done;
        }
    }
    if (resolve)
        goto retry;

    code = ETIMEDOUT;

exit_done:
    if (tmp)
        RPC2_freeaddrinfo(tmp);
    return code;
}

/* MUST NOT be called from within a transaction */
void Realm::SetRootVolName(char *name)
{
    Recov_BeginTrans();
    RVMLIB_REC_OBJECT(rootvolname);
    rvmlib_rec_free(rootvolname);
    rootvolname = rvmlib_rec_strdup(name);
    CODA_ASSERT(rootvolname);
    Recov_EndTrans(MAXFP);
}

void Realm::print(FILE *f)
{
    struct RPC2_addrinfo *p;

    fprintf(f, "%08x realm '%s', refcount %d/%d\n", (unsigned int)Id(), Name(),
            refcount, rec_refcount);
    for (p = rootservers; p; p = p->ai_next) {
        char buf[RPC2_ADDRSTRLEN];
        RPC2_formataddrinfo(p, buf, RPC2_ADDRSTRLEN);
        fprintf(f, "\t%s\n", buf);
    }
}
