/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
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
#include <netdb.h>

#ifdef __cplusplus
}
#endif

#include <rvmlib.h>
#include "rec_dllist.h"
#include "realm.h"
#include "server.h"
#include "comm.h"

#define MAXLINELEN 256
static char line[MAXLINELEN];

static struct in_addr *ResolveRootServers(char *servers)
{
    int i;
    struct in_addr *hosts;
    char *host;

    hosts = (struct in_addr *)malloc(sizeof(struct in_addr));
    if (!hosts) {
	eprint("Cannot allocate initial hosts array");
	return NULL;
    }

    i = 0;
    for (i = 0; host = strtok(servers, " \t\n");) {

	struct hostent *h;
	servers = NULL;

#ifndef GETHOSTBYNAME_ACCEPTS_IPADDRS
	if (!inet_aton(host, &hosts[i]))
#endif
	{
	    h = gethostbyname(host);
	    if (!h) {
		eprint("Cannot resolve realm rootserver '%s'", host);
		continue;
	    }
	    if (h->h_length != sizeof(struct in_addr)) {
		eprint("Cannot find IPv4 address for realm rootserver '%s'",
		       host);
		continue;
	    }
	    memcpy(&hosts[i], h->h_addr, sizeof(struct in_addr));
	}

	if (hosts[i].s_addr == INADDR_ANY ||
	    hosts[i].s_addr == INADDR_NONE ||
	    hosts[i].s_addr == INADDR_LOOPBACK ||
	    (hosts[i].s_addr & IN_CLASSA_NET) == IN_LOOPBACKNET ||
	    IN_MULTICAST(hosts[i].s_addr) ||
	    IN_BADCLASS(hosts[i].s_addr))
	{
	    eprint("Address for '%s' resolved to bad or unusable address '%s', "
		   "ignoring it", host, inet_ntoa(hosts[i]));
	    continue;
	}

	hosts = (struct in_addr *)realloc(hosts, (i+2)*sizeof(struct in_addr));
	if (!h) {
	    eprint("Cannot realloc hosts array");
	    return NULL;
	}
	i++;
    }
    hosts[i].s_addr = INADDR_ANY;

    return hosts;
}

struct in_addr *GetRealmServers(const char *realm_name)
{
    FILE *f;
    int namelen = strlen(realm_name), found;

    f = fopen(realmtab, "r");
    if (!f) {
	eprint("Couldn't open '%s'", realmtab);
	return NULL;
    }

    found = 0;
    while (!found && fgets(line, MAXLINELEN, f)) {
	if (line[0] == '#') continue;

	if (strncmp(line, realm_name, namelen) == 0 && isspace(line[namelen]))
	    found = 1;
    }
    fclose(f);

    if (found)
	return ResolveRootServers(&line[namelen]);

    return NULL;
}


Realm::Realm(const char *realm_name, struct dllist_head *h) :
    PersistentObject(h)
{
    int len = strlen(realm_name) + 1;

    RVMLIB_REC_OBJECT(name);
    name = (char *)rvmlib_rec_malloc(len); 
    CODA_ASSERT(name);
    strcpy(name, realm_name);

    RVMLIB_REC_OBJECT(id);
#warning "realmid's"
    id = 0;

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

#if 0
volent *Realm::GetVolume(const char *volname)
{
    volent *v;

    VDB->Get(&v, volname);

#if 0
    list_for_each(p, repvols) {
	v = list_entry(p, volent, volumes);
	if (strcmp(v->volname, volname) == 0) {
	    v->GetRef();
	    return v;
	}
    }
    list_for_each(p, volreps) {
	v = list_entry(p, volent, volumes);
	if (strcmp(v->volname, volname) == 0) {
	    v->GetRef();
	    return v;
	}
    }

    Recov_BeginTrans();
    v = new volent(volname, &volumes);
    Recov_EndTrans(0);
#endif

    return v;
}
#endif

void Realm::print(FILE *f)
{
    int i = 0;

    fprintf(f, "%08x realm '%s'\n", Id(), Name());
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

