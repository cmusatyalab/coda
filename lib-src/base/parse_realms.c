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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include <coda_config.h>
#include "codaconf.h"
#include "parse_realms.h"

void SplitRealmFromName(char *name, const char **realm)
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
    struct RPC2_addrinfo hints = {
	.ai_family   = PF_INET,
	.ai_socktype = SOCK_DGRAM,
	.ai_protocol = IPPROTO_UDP,
	.ai_flags    = RPC2_AI_CANONNAME,
    };
    char *host;

    while ((host = strtok(servers, ", \t\n")) != NULL)
    {
	servers = NULL;
	coda_getaddrinfo(host, service, &hints, res);
    }
    /* sort preferred addresses to the head of the list */
    coda_reorder_addrinfo(res);
}

static int in_realmtab(const char *name, const char *service,
		       struct RPC2_addrinfo **res)
{
#define MAXLINELEN 256
    char line[MAXLINELEN];
    const char *realmtab = NULL;
    int namelen, found = 0;
    FILE *f;

    CODACONF_STR(realmtab, "realmtab", SYSCONFDIR "/realms");

    f = fopen(realmtab, "r");
    if (f) {
	namelen = strlen(name);
	while (fgets(line, MAXLINELEN, f))
	{
	    /* skip commented lines and where the realm name doesn't match */
	    if (line[0] == '#' || strncmp(line, name, namelen) != 0)
		continue;

	    /* end of line or no servers named, then we shouldn't resolve */
	    if (line[namelen] == '\0' || line[namelen] == '\n')
		break;

	    /* make sure we didn't only match a prefix of a longer name */
	    if (!isspace((int)line[namelen]))
		continue;

	    ResolveRootServers(&line[namelen], service, res);
	    found = 1;
	    break;
	}
	fclose(f);
    }
    return found;
}

void GetRealmServers(const char *name, const char *service,
		     struct RPC2_addrinfo **res)
{
    struct RPC2_addrinfo hints = {
	.ai_family   = PF_INET,
	.ai_socktype = SOCK_DGRAM,
	.ai_protocol = IPPROTO_UDP,
	.ai_flags    = RPC2_AI_CANONNAME | CODA_AI_RES_SRV,
    };

    if (!name || name[0] == '\0')
	CODACONF_STR(name, "realm", "localhost");

    if (!name || name[0] == '\0' || name[0] == '.')
	return;

    if (in_realmtab(name, service, res))
	return;

    /* As we expect only FQDNs, the name should contain at least one '.'
     * This also prevents lookups for accidentally mistyped paths as well
     * as things that the OS might look for like 'Recycle Bin'. */
    if (strchr(name, '.') == NULL)
	return;

    coda_getaddrinfo(name, service, &hints, res);
}

