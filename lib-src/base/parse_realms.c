/* BLURB lgpl

                           Coda File System
                              Release 6

          Copyright (c) 2002-2003 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/


#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

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

static void ResolveRootServers(char *servers, const char *service,
			       struct RPC2_addrinfo **res)
{
    struct RPC2_addrinfo hints;
    char *host;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = PF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_flags    = RPC2_AI_CANONNAME;

    while ((host = strtok(servers, ", \t\n")) != NULL)
    {
	servers = NULL;
	coda_getaddrinfo(host, service, &hints, res);
    }
    /* sort preferred addresses to the head of the list */
    coda_reorder_addrinfo(res);
}
	
static int isbadaddr(struct RPC2_addrinfo *ai, const char *name)
{
    struct in_addr *ip;

#warning "assuming ipv4 only"
    if (ai->ai_family != PF_INET)
	return 0;

    ip = &((struct sockaddr_in *)ai->ai_addr)->sin_addr;

    if (ip->s_addr == INADDR_ANY ||
	ip->s_addr == INADDR_NONE ||
	ip->s_addr == INADDR_LOOPBACK ||
	(ip->s_addr & IN_CLASSA_NET) == IN_LOOPBACKNET ||
	IN_MULTICAST(ip->s_addr) ||
	IN_BADCLASS(ip->s_addr))
    {
	fprintf(stderr, "An address in realm '%s' resolved to unusable address '%s', ignoring it\n", name, inet_ntoa(*ip));
	return 1;
    }
    return 0;
}

void GetRealmServers(const char *name, const char *service,
		     struct RPC2_addrinfo **res)
{
    struct RPC2_addrinfo **p, *results = NULL;
    char *realmtab = NULL;
    FILE *f;
    int namelen, found = 0;

    if (!name || name[0] == '\0')
	CONF_STR(name, "realm", "DEFAULT");

    if (strcmp(name, "localhost") == 0)
	return;

    CONF_STR(realmtab, "realmtab", SYSCONFDIR "/realms");

    f = fopen(realmtab, "r");
    if (f) {
	namelen = strlen(name);
	while (!found && fgets(line, MAXLINELEN, f))
	{
	    if (line[0] == '#') continue;

	    if (strncmp(line, name, namelen) == 0 &&
		(line[namelen] == '\0' || isspace(line[namelen])))
	    {
		ResolveRootServers(&line[namelen], service, &results);
		found = 1;
	    }
	}
	fclose(f);
    }

    if (!found) {
	struct RPC2_addrinfo hints;

#ifdef PF_INET6
	/* As we expect onlu FQDNs, the name should contain at least one '.'
	 * This also prevents lookups for accidentally mistyped paths as well
	 * as things that the OS might look for like 'Recycle Bin'. */
	char tmp[sizeof(struct in6_addr)];
	if (inet_pton(PF_INET6, name, &tmp) <= 0)
#endif
	    if (strchr(name, '.') == NULL)
		return;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family   = PF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;
	hints.ai_flags    = RPC2_AI_CANONNAME | CODA_AI_RES_SRV;

	coda_getaddrinfo(name, service, &hints, &results);
    }

    /* Is is really necessary to filter out loopback and other bad
     * addresses from the nameserver response? */
    for (p = &results; *p;) {
	if (isbadaddr(*p, name)) {
	    struct RPC2_addrinfo *cur = *p;
	    *p = cur->ai_next;
	    cur->ai_next = NULL;
	    RPC2_freeaddrinfo(cur);
	    continue;
	}
	p = &(*p)->ai_next;
    }
    *p = *res;
    *res = results;
}

