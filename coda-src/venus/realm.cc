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
    strcpy(name, realm_name);

    rec_list_head_init(&realms);

    rootservers = NULL;
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
    Server *s;

    rec_list_del(&realms);
    if (rootservers) {
	free(rootservers);
	rootservers = NULL;
    }
    rvmlib_rec_free(name); 
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

static int isbadaddr(struct in_addr *ip, char *name)
{
	if (ip->s_addr == INADDR_ANY ||
	    ip->s_addr == INADDR_NONE ||
	    ip->s_addr == INADDR_LOOPBACK ||
	    (ip->s_addr & IN_CLASSA_NET) == IN_LOOPBACKNET ||
	    IN_MULTICAST(ip->s_addr) ||
	    IN_BADCLASS(ip->s_addr))
	{
	    fprintf(stderr, "An address in realm '%s' resolved to bad or unusable address '%s', ignoring it\n", name, inet_ntoa(*ip));
	    return 1;
	}
	return 0;
}

/* Get a connection to any server (as root). */
int Realm::GetAdmConn(connent **cpp)
{
    struct coda_addrinfo *p;
    int code = 0;
    int tryagain = 0;

    LOG(100, ("GetAdmConn: \n"));

    CODA_ASSERT(this != LocalRealm);

    *cpp = 0;

retry:
    if (!rootservers)
	GetRealmServers(name, "codasrv", &rootservers);
    else
	coda_reorder_addrs(&rootservers);

    if (!rootservers) {
	eprint("Failed to find servers for realm '%s'", name);
	return ETIMEDOUT;
    }

    /* Get a connection to any custodian. */
    for (p = rootservers; p; p = p->ai_next) {
	struct sockaddr_in *sin = (struct sockaddr_in *)p->ai_addr;
	srvent *s;
	if (isbadaddr(&sin->sin_addr, name)) continue;
	s = ::GetServer(&sin->sin_addr, Id());
	code = s->GetConn(cpp, V_UID);
	switch(code) {
	case ERETRY:
	    tryagain = 1;
	case ETIMEDOUT:
	    continue;

	case 0:
	case EINTR:
	    return(code);

	default:
	    if (code < 0)
		eprint("GetAdmConn: bogus code (%d)", code);
	    return(code);
	}
    }
    if (tryagain) {
	coda_freeaddrinfo(rootservers);
	rootservers = NULL;
	goto retry;
    }
}

