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
#include "rec_dllist.h"
#include "realm.h"
#include "realmdb.h"
#include "comm.h"
#include "parse_realms.h"

Realm::Realm(const char *realm_name)
{
    int len = strlen(realm_name) + 1;

    RVMLIB_REC_OBJECT(name);
    name = (char *)rvmlib_rec_malloc(len); 
    CODA_ASSERT(name);
    rvmlib_set_range(name, len);
    strcpy(name, realm_name);

    rec_list_head_init(&realms);

    rootservers = NULL;

    eprint("Created realm '%s'", name);
}

void Realm::ResetTransient(void)
{
    rootservers = NULL;

    /* this might destroy the object, so it has to be called last */
    /* Dang, only works right when it is a virtual function, but we have a
     * problem storing C++ objects with virtual functions in RVM */
    //PersistentObject::ResetTransient();
    
    refcount = 0;
}

Realm::~Realm(void)
{
    struct dllist_head *p;
    VenusFid Fid;
    fsobj *f;
    eprint("Removing realm '%s'", name);
    
    rec_list_del(&realms);
    if (rootservers) {
	coda_freeaddrinfo(rootservers);
	rootservers = NULL;
    }
    rvmlib_rec_free(name); 

    Fid.Realm = LocalRealm->Id();
    Fid.Volume = FakeRootVolumeId;

    /* kill the fake object that represented our mountlink */
    Fid.Vnode = 0xfffffffc;
    Fid.Unique = Id();
    f = FSDB->Find(&Fid);
    if (f) f->Kill();
}

void Realm::print(FILE *f)
{
    struct coda_addrinfo *p;
    int i = 0;

    fprintf(f, "%08x realm '%s', refcount %d/%d\n", Id(), Name(),
	    refcount, rec_refcount);
    for (p = rootservers; p; p = p->ai_next) {
	struct sockaddr_in *sin = (struct sockaddr_in *)p->ai_addr;
	fprintf(f, "\t%s\n", inet_ntoa(sin->sin_addr)); 
    }
}

/* Get a connection to any server (as root). */
int Realm::GetAdmConn(connent **cpp)
{
    struct coda_addrinfo *p;
    int code = 0;
    int tryagain = 0;
    int unknown = !rootservers;

    LOG(100, ("GetAdmConn: \n"));

    if (this == LocalRealm)
	return ETIMEDOUT;

    *cpp = 0;

retry:
    if (!rootservers)
	GetRealmServers(name, "codasrv", &rootservers);
    else {
	coda_reorder_addrs(&rootservers);
	/* our cached addresses might be stale, re-resolve if we can't reach
	 * any of the servers */
	tryagain = 1;
    }

    if (!rootservers)
	return ETIMEDOUT;

    /* Get a connection to any custodian. */
    for (p = rootservers; p; p = p->ai_next) {
	struct sockaddr_in *sin = (struct sockaddr_in *)p->ai_addr;
	srvent *s;
	s = ::GetServer(&sin->sin_addr, Id());
	code = s->GetConn(cpp, V_UID);
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
	coda_freeaddrinfo(rootservers);
	rootservers = NULL;
	tryagain = 0;
	goto retry;
    }
    return ETIMEDOUT;
}

