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

/* Coda only looks up IPv4 UDP addresses */
static void simpleaddrinfo(const char *realm, const char *service,
			   struct coda_addrinfo **res)
{
    struct coda_addrinfo hints;
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
    hints.ai_flags    = AI_CANONNAME;

    coda_getaddrinfo(realm, service, &hints, res);
}

static void ResolveRootServers(char *servers, const char *service,
			       struct coda_addrinfo **res)
{
    char *host;

    while ((host = strtok(servers, ", \t\n")) != NULL)
    {
	servers = NULL;
	simpleaddrinfo(host, service, res);
    }
}
	
void GetRealmServers(const char *name, const char *service,
		     struct coda_addrinfo **res)
{
    char *realmtab = NULL;
    FILE *f;
    int namelen, found = 0;

    if (!name || name[0] == '\0')
	CONF_STR(name, "realm", "DEFAULT");


    CONF_STR(realmtab, "realmtab", SYSCONFDIR "/realms");

    f = fopen(realmtab, "r");
    if (f) {
	namelen = strlen(name);
	while (!found && fgets(line, MAXLINELEN, f))
	{
	    if (line[0] == '#') continue;

	    if (strncmp(line, name, namelen) == 0 && isspace(line[namelen])) {
		ResolveRootServers(&line[namelen], service, res);
		found = 1;
	    }
	}
	fclose(f);
    }

    if (!found) {
	char *fullname = malloc(strlen(name) + 2);
	if (fullname) {
	    strcpy(fullname, name);
	    strcat(fullname, ".");
	    simpleaddrinfo(fullname, service, res);
	    free(fullname);
	}
    }
}

