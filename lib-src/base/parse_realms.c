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

static struct in_addr *ResolveRootServers(char *servers)
{
    int i;
    struct in_addr *hosts;
    char *host;

    hosts = (struct in_addr *)malloc(sizeof(struct in_addr));
    if (!hosts) {
	fprintf(stderr, "Cannot allocate initial hosts array");
	return NULL;
    }

    i = 0;
    for (i = 0; (host = strtok(servers, " \t\n")) != NULL; servers = NULL)
    {

#ifndef GETHOSTBYNAME_ACCEPTS_IPADDRS
	if (!inet_aton(host, &hosts[i]))
#endif
	{
	    struct hostent *h = gethostbyname(host);
	    if (!h) {
		fprintf(stderr, "Cannot resolve realm rootserver '%s'", host);
		continue;
	    }
	    if (h->h_length != sizeof(struct in_addr)) {
		fprintf(stderr, "Cannot find IPv4 address for realm rootserver '%s'", host);
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
	    fprintf(stderr, "Address for '%s' resolved to bad or unusable address '%s', ignoring it", host, inet_ntoa(hosts[i]));
	    continue;
	}

	hosts = (struct in_addr *)realloc(hosts, (i+2)*sizeof(struct in_addr));
	if (!hosts) {
	    fprintf(stderr, "Cannot realloc hosts array");
	    return NULL;
	}
	i++;
    }
    hosts[i].s_addr = INADDR_ANY;

    return hosts;
}

struct in_addr *GetRealmServers(const char *realm_name)
{
    char *realmtab = NULL;
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

    return NULL;
}


