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
#include "server.h"
#include "comm.h"
#include "parse_realms.h"

Realm::Realm(const char *realm_name, struct dllist_head *h) :
    PersistentObject(h)
{
    int len = strlen(realm_name) + 1;

    RVMLIB_REC_OBJECT(name);
    name = (char *)rvmlib_rec_malloc(len); 
    CODA_ASSERT(name);
    strcpy(name, realm_name);

    /* Grab a reference until volumes hold on to this realm... */
    Rec_GetRef();
    ResetTransient();
    Rec_PutRef();
}

void Realm::ResetTransient(void)
{
    list_head_init(&servers);

    PersistentObject::ResetTransient();

    rootservers = NULL;
}

Realm::~Realm(void)
{
    struct dllist_head *p;
    Server *s;

    free(rootservers);
    rvmlib_rec_free(name); 

    for (p = servers.next; p != &servers; ) {
	s = list_entry(p, Server, servers);
	p = p->next;
	s->PutRef();
    }
    list_del(&servers);
}

Server *Realm::GetServer(struct in_addr *host)
{
    struct dllist_head *p;
    Server *s;

    CODA_ASSERT(host != 0);
    CODA_ASSERT(host->s_addr != 0);

    list_for_each(p, servers) {
	s = list_entry(p, Server, servers);
	if (s->ipaddr()->s_addr == host->s_addr) {
	    s->GetRef();
	    return s;
	}
    }

    s = new Server(host, &servers, this);

    return s;
}

void Realm::print(FILE *f)
{
    int i = 0;

    fprintf(f, "%08x realm '%s', refcount %d\n", Id(), Name(), refcount);
    while(rootservers && rootservers[i].s_addr != INADDR_ANY)
	fprintf(f, "\t%s\n", inet_ntoa(rootservers[i++])); 

}


/* Get a connection to any server (as root). */
int Realm::GetAdmConn(connent **cpp)
{
    LOG(100, ("GetAdmConn: \n"));

    *cpp = 0;
    int code = 0;

    if (!rootservers)
	rootservers = GetRealmServers(name);

    if (!rootservers) {
	eprint("Failed to find servers for realm '%s'", name);
	return ETIMEDOUT;
    }

    /* Get a connection to any custodian. */
    for (;;) {
	int tryagain = 0, i = 0;
	while(rootservers[i].s_addr != INADDR_ANY) {
	    srvent *s = ::GetServer(&rootservers[i++], Id());
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
	if (!tryagain)
	    return(ETIMEDOUT);
    }
}

