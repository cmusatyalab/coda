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

    if (strcmp(name, LOCALREALM) != 0) {
	GetRealmServers(name, "codasrv", &rootservers);
	if (rootservers)
	    eprint("Created realm '%s'", name);
	else
	    eprint("Failed to find servers for realm '%s'", name);
    }
}

void Realm::ResetTransient(void)
{
    rootservers = NULL;

    /* this might destroy the object, so it has to be called last */
    PersistentObject::ResetTransient();
}

Realm::~Realm(void)
{
    struct dllist_head *p;
    eprint("Removing realm %s", name);

    rec_list_del(&realms);
    if (rootservers) {
	coda_freeaddrinfo(rootservers);
	rootservers = NULL;
    }
    rvmlib_rec_free(name); 

    REALMDB->RebuildRoot();
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
    int tryagain;

    LOG(100, ("GetAdmConn: \n"));

    if (this == LocalRealm)
	return ETIMEDOUT;

    *cpp = 0;

retry:
    tryagain = 0;
    if (!rootservers)
	GetRealmServers(name, "codasrv", &rootservers);
    else
	coda_reorder_addrs(&rootservers);

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
	goto retry;
    }
    return ETIMEDOUT;
}

