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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <coda_config.h>
#include "codaconf.h"
#include "parse_realms.h"

#define MAXLINELEN 256
static char line[MAXLINELEN];

#define CODASRV "codasrv"

void SplitRealmFromName(char *name, char **realm)
{
    /* Here we do the following 'translation' */
    /* "name"     -> keep existing realm */
    /* "name@"    -> keep existing realm */
    /* "name@xxx" -> return realm 'xxx' */

    char *p;

    p = strrchr(name, '@');
    if (p) {
	*p = '\0';
	if (p[1])
	    *realm = &p[1];
    }
}

/* Coda only looks up IPv4 UDP addresses */
static int simpleaddrinfo(const char *realm, const char *service,
			  struct addrinfo **res)
{
    struct addrinfo hints;
    int proto = IPPROTO_UDP;

#ifdef HAVE_GETPROTOBYNAME
    struct protoent *pe;
    pe = getprotobyname("udp");
    if (pe)
	proto = pe->p_proto;
#endif

    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = PF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = proto;

    return coda_getaddrinfo(realm, service, &hints, res);
}

static struct addrinfo *ResolveRootServers(char *servers)
{
    struct addrinfo *res = NULL, *tmp, *p;
    char *host;
    int i, err;

    i = 0;
    for (i = 0; (host = strtok(servers, ", \t\n")) != NULL; servers = NULL)
    {
	tmp = NULL;
	err = simpleaddrinfo(host, CODASRV, &tmp);
	if (err) continue;
	for (p = tmp; p && p->ai_next; p = p->ai_next) /*loop*/;
	if (p)
	    p->ai_next = res;
	res = tmp;
    }
    return res;
}
	

#if 0
/* where to put this test, could be useful */
	if (hosts[i].s_addr == INADDR_ANY ||
	    hosts[i].s_addr == INADDR_NONE ||
	    hosts[i].s_addr == INADDR_LOOPBACK ||
	    (hosts[i].s_addr & IN_CLASSA_NET) == IN_LOOPBACKNET ||
	    IN_MULTICAST(hosts[i].s_addr) ||
	    IN_BADCLASS(hosts[i].s_addr))
	{
	    fprintf(stderr, "Address for '%s' resolved to bad or unusable address '%s', ignoring it", host, inet_ntoa(hosts[i]));
	    continue;
	}
#endif

struct addrinfo *GetRealmServers(const char *realm_name)
{
    char *realmtab = NULL;
    struct addrinfo *res = NULL;
    FILE *f;
    int namelen = strlen(realm_name), found;

    CONF_STR(realmtab, "realmtab", SYSCONFDIR "/realms");

    f = fopen(realmtab, "r");
    if (!f) {
	fprintf(stderr, "Couldn't open '%s'", realmtab);
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

    if (simpleaddrinfo(realm_name, CODASRV, &res) == 0)
	return res;

    return NULL;
}

