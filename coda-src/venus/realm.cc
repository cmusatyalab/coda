/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 2003 Carnegie Mellon University
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

#include <rvmlib.h>
#include "realm.h"
#include "realmdb.h"
#include "comm.h"
#include "user.h"
#include "parse_realms.h"
#include "rec_dllist.h"

/* MUST be called from within a transaction */
Realm::Realm(const char *realm_name)
{
    int len = strlen(realm_name) + 1;

    RVMLIB_REC_OBJECT(name);
    name = (char *)rvmlib_rec_malloc(len); 
    CODA_ASSERT(name);
    rvmlib_set_range(name, len);
    strcpy(name, realm_name);

    rec_list_head_init(&realms);

    RVMLIB_REC_OBJECT(rec_refcount);
    /* we need a reference to prevent suicide in ResetTransient */
    rec_refcount = 1;

    ResetTransient();

    /* and set the correct refcounts for the new object */
    refcount = 1;
    rec_refcount = 0;
}

/* MUST be called from within a transaction */
Realm::~Realm(void)
{
    VenusFid Fid;
    fsobj *f;

    CODA_ASSERT(!rec_refcount && refcount <= 1);

    rec_list_del(&realms);
    if (rootservers) {
	eprint("Removing realm '%s'", name);

	RPC2_freeaddrinfo(rootservers);
	rootservers = NULL;
    }
    rvmlib_rec_free(name); 

    delete system_anyuser;

    /* kill the fake object that represents our mountlink */
    Fid.Realm = LocalRealm->Id();
    Fid.Volume = FakeRootVolumeId;
    Fid.Vnode = 0xfffffffc;
    Fid.Unique = Id();

    f = FSDB->Find(&Fid);
    if (f) f->Kill();
}

/* MAY be called from within a transaction */
void Realm::ResetTransient(void)
{
    rootservers = NULL;
    refcount = 0;
    system_anyuser = new userent(Id(), (uid_t)-1);

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
    int intrans;

    CODA_ASSERT(refcount);
    refcount--;

    if (refcount || rec_refcount)
	return;

    intrans = rvmlib_in_transaction();
    if (!intrans)
	Recov_BeginTrans();
    
    delete this;

    if (!intrans)
	Recov_EndTrans(MAXFP);
}

/* Get a connection to any server (as root). */
/* MUST NOT be called from within a transaction */
int Realm::GetAdmConn(connent **cpp)
{
    struct RPC2_addrinfo *p;
    int code = 0;
    int tryagain = 0;
    int unknown = !rootservers;

    LOG(100, ("GetAdmConn: \n"));

    if (STREQ(name, LOCALREALM))
	return ETIMEDOUT;

    *cpp = 0;

retry:
    if (!rootservers)
	GetRealmServers(name, "codasrv", &rootservers);
    else {
	coda_reorder_addrinfo(&rootservers);
	/* our cached addresses might be stale, re-resolve if we can't reach
	 * any of the servers */
	tryagain = 1;
    }

    if (!rootservers)
	return ETIMEDOUT;

    /* Get a connection to any custodian. */
    for (p = rootservers; p; p = p->ai_next) {
	struct sockaddr_in *sin;
	srvent *s;

	CODA_ASSERT(p->ai_family == PF_INET);
	sin = (struct sockaddr_in *)p->ai_addr;

	s = ::GetServer(&sin->sin_addr, Id());
	code = s->GetConn(cpp, V_UID);
	PutServer(&s);
	switch(code) {
	case ERETRY:
	    tryagain = 1;
	case ETIMEDOUT:
	    continue;

	case 0:
	case EINTR:
	    /* We might have discovered a new realm */
	    if (unknown) {
		VenusFid Fid;
		fsobj *f;

		eprint("Resolved realm '%s'", name);

		Fid.Realm = LocalRealm->Id();
		Fid.Volume = FakeRootVolumeId;
		Fid.Vnode = 1;
		Fid.Unique = 1;

		f = FSDB->Find(&Fid);
		if (f) {
		    Recov_BeginTrans();
		    f->Kill();
		    Recov_EndTrans(MAXFP);
		}
	    }
	    return code;

	default:
	    if (code < 0)
		eprint("GetAdmConn: bogus code (%d)", code);
	    return code;
	}
    }
    if (tryagain) {
	RPC2_freeaddrinfo(rootservers);
	rootservers = NULL;
	tryagain = 0;
	goto retry;
    }
    return ETIMEDOUT;
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

