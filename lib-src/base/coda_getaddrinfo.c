/* BLURB lgpl

                           Coda File System
                              Release 6

          Copyright (c) 2002 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights
                           none currently
#*/

/*
 * This code handles DNS SRV record (RFC2782), and IN A lookups for a
 * specified service within a domain. In a perfect world 'getaddrinfo'
 * would do all of this for us ;)
 */

#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <resolv.h>
#include <netdb.h>

#include "coda_getaddrinfo.h"

#if 0
/* We need the official protocol name for this service for the SRV record
 * lookup. Having hints will help, otherwise we have to fallback on trying
 * getservbyname for the various protocols. */
static char *get_proto_from_hints(const char *service,
				  const struct coda_addrinfo *hints)
{
    struct protoent *pe;

    if (hints) {
	if (hints->ai_protocol) {
	    pe = getprotobynumber(hints->ai_protocol);
	    if (pe) return pe->p_name;
	}

	if (hints->ai_socktype) {
	    if (hints->ai_socktype == SOCK_STREAM)
		return "tcp";
	    else if (hints->ai_socktype == SOCK_DGRAM)
		return "udp";
	}
    }

    /* hints didn't help, let's try to find the service */
    if (getservbyname(service, "tcp"))
	return "tcp";
    else if (getservbyname(service, "udp"))
	return "udp";

    return NULL;
}
#endif

struct summary {
    int family;
    int socktype;
    int protocol;
    int flags;
};

static char *srvdomainname(const char *realm, const char *service,
			   const struct summary *sum)
{
    char *proto, *domain;
    int len;
    
    proto = (sum->protocol == IPPROTO_TCP) ? "tcp" : "udp";
    len = strlen(service) + strlen(proto) + strlen(realm) + 6;
    domain = malloc(len);
    if (domain)
	sprintf(domain, "_%s._%s.%s.", service, proto, realm);

    return domain;
}


static int DN_HOST(char *msg, int mlen, char **ptr, char *dest)
{
    int len = dn_expand(msg, msg + mlen, *ptr, dest, MAXHOSTNAMELEN);
    if (len < 0) return -1;
    *ptr += len;
    return 0;
}

static int DN_SHORT(char *msg, int mlen, char **ptr, int *dest)
{
    if (*ptr + NS_INT16SZ > msg + mlen)
	return -1;
    *dest = ntohs(*(uint16_t *)*ptr);
    *ptr += NS_INT16SZ;
    return 0;
}

static int DN_INT(char *msg, int mlen, char **ptr, int *dest)
{
    if (*ptr + NS_INT32SZ > msg + mlen)
	return -1;
    *dest = ntohl(*(uint32_t *)*ptr);
    *ptr += NS_INT32SZ;
    return 0;
}

static int resolve_host(const char *name, int port, const struct summary *sum,
			int priority, int weight, struct coda_addrinfo **res)
{
    struct hostent *he;
    int i, resolved = 0;

#ifdef HAVE_GETIPNODEBYNAME
    int err, flags = (sum->family == PF_INET6) ? AI_ALL : 0;

    he = getipnodebyname(name, sum->family, flags, &err);
    if (!he) {
	switch (err) {
	case HOST_TRY_AGAIN: return EAI_AGAIN;
	case HOST_NOADDRESS: return EAI_NODATA;
	case HOST_NOT_FOUND: return EAI_NONAME;
	case HOST_NORECOVERY:
	default:	     return EAI_FAIL;
	}
    }
#else
    he = gethostbyname(name);
    if (!he) {
	switch(h_errno) {
	case TRY_AGAIN:      return EAI_AGAIN;
	case NO_ADDRESS:     return EAI_NODATA;
	case HOST_NOT_FOUND: return EAI_NONAME;
	case NO_RECOVERY:
	default:	     return EAI_FAIL;
	}
    }
#endif

    /* count number of distinct ip's for this server and adjust weight */
    for (i = 0; he->h_addr_list[i]; i++) /**/;
    if (i) weight /= i;

    for (i = 0; he->h_addr_list[i]; i++) {
	struct coda_addrinfo *ai;
	struct sockaddr_in *sin;

	ai = malloc(sizeof(*ai));
	if (!ai) break;
	memset(ai, 0, sizeof(*ai));

	ai->ai_family = sum->family;
	ai->ai_socktype = sum->socktype;
	ai->ai_protocol = sum->protocol;
	ai->ai_priority = priority;
	ai->ai_weight = weight;

	sin = malloc(sizeof(*sin));
	if (!sin) {
	    free(ai);
	    break;
	}
	memset(sin, 0, sizeof(*sin));

	sin->sin_family = AF_INET;
	sin->sin_port = htons(port);
	sin->sin_addr = *(struct in_addr *)he->h_addr_list[i];

	ai->ai_addrlen = sizeof(*sin);
	ai->ai_addr = (struct sockaddr *)sin;

	if (sum->flags & AI_CANONNAME)
	    ai->ai_canonname = strdup(he->h_name);

	ai->ai_next = *res;
	*res = ai;
	resolved++;
    }

#ifdef HAVE_GETIPNODEBYNAME
    freehostent(he);
#endif

    return resolved ? 0 : (i ? EAI_MEMORY : EAI_NODATA);
}

static int parse_res_reply(char *answer, int alen, const struct summary *sum,
			   struct coda_addrinfo **res)
{
    char *p = answer, name[MAXHOSTNAMELEN];
    int priority, weight, port, dummy;
    int err = EAI_AGAIN, tmperr;

    /* arghhhhh, I don't like digging through libresolv output */
    p += NS_HFIXEDSZ; /* what is in the header? probably nothing interesting */

    /* skip original query + type + class */
    if (DN_HOST(answer, alen, &p, name) ||
	DN_SHORT(answer, alen, &p, &dummy) ||
        DN_SHORT(answer, alen, &p, &dummy))
	return err; /* corrupted packet, retryable? */

    while (p < answer + alen)
    {
	int type, size;

	if (DN_HOST(answer, alen, &p, name) ||
	    DN_SHORT(answer, alen, &p, &type) ||
	    DN_SHORT(answer, alen, &p, &dummy) || // class
	    DN_INT(answer, alen, &p, &dummy) || // ttl
	    DN_SHORT(answer, alen, &p, &size))
	{
	    break; /* corrupted packet? */
	}

	if (type != ns_t_srv) {
	    p += size; 
	    continue;
	}

	if (DN_SHORT(answer, alen, &p, &priority) ||
	    DN_SHORT(answer, alen, &p, &weight) ||
	    DN_SHORT(answer, alen, &p, &port) ||
	    DN_HOST(answer, alen, &p, name))
	{
	    break; /* corrupted packet? */
	}

	if (name[0] == '.' && name[1] == '\0')
	{
	    continue;
	}

	tmperr = resolve_host(name, port, sum, priority, weight, res);
	if (err == EAI_AGAIN)
	    err = tmperr;
    }
    return err;
}

static int do_srv_lookup(const char *realm, const char *service,
			 const struct summary *sum, struct coda_addrinfo **res)
{
    char answer[1024], *srvdomain;
    int len;
    
#ifdef TESTING
    fprintf(stderr, "Doing SRV record lookup for %s %s\n", realm, service);
#endif
    srvdomain = srvdomainname(realm, service, sum);
    if (!srvdomain)
	return EAI_MEMORY;

    len = res_search(srvdomain, ns_c_in, ns_t_srv, answer, sizeof(answer));

    free(srvdomain);
    
    if (len == -1)
	return EAI_FAIL;

    return parse_res_reply(answer, len, sum, res);
}

int coda_getaddrinfo(const char *node, const char *service,
		     const struct coda_addrinfo *hints,
		     struct coda_addrinfo **res)
{
    struct coda_addrinfo *srvs = NULL;
    struct summary sum = { PF_UNSPEC, 0, 0, 0 };
    struct in_addr addr;
    char *tmpnode, *end;
    int err, len, port, is_ip = 0;

    if (hints) {
	sum.family   = hints->ai_family;
	sum.socktype = hints->ai_socktype;
	sum.protocol = hints->ai_protocol;
	sum.flags    = hints->ai_flags;
    }

    /* check arguments */
    if (sum.family != PF_UNSPEC &&
	sum.family != PF_INET &&
	sum.family != PF_INET6)
	return EAI_FAMILY;

    if (sum.socktype &&
	sum.socktype != SOCK_STREAM &&
	sum.socktype != SOCK_DGRAM)
	return EAI_SOCKTYPE;

    if (!node || !service)
	return EAI_NONAME;

    tmpnode = strdup(node);
    if (!tmpnode)
	return EAI_MEMORY;

    /* force some defaults */
    if (sum.family == PF_UNSPEC)
	sum.family = PF_INET;

    if (!sum.socktype)
	sum.socktype = SOCK_STREAM;

    if (!sum.protocol) {
	if (sum.socktype == SOCK_STREAM)
	    sum.protocol = IPPROTO_TCP;
	else if (sum.socktype == SOCK_DGRAM)
	    sum.protocol = IPPROTO_UDP;
    }

    /* conditionally strip any terminating '.' if the name is an ip-address */
    len = strlen(tmpnode);
    if (tmpnode[len-1] == '.') {
	tmpnode[len-1] = '\0';
	is_ip = inet_aton(tmpnode, &addr);
	if (!is_ip)
	    tmpnode[len-1] = '.';
    }
    port = strtol(service, &end, 10);
    if (*service == '\0' || *end != '\0')
	port = 0;

    if (!is_ip && !port)
	/* try to find SRV records */
	err = do_srv_lookup(tmpnode, service, &sum, &srvs);
    else
	err = EAI_NONAME;

    /* fall back to A records */
    if (err) {
	char *proto = (sum.protocol == IPPROTO_TCP) ? "tcp" : "udp";
	if (!port) {
	    struct servent *se = getservbyname(service, proto);
	    if (!se) {
		free(tmpnode);
		return EAI_SERVICE;
	    }
	    port = ntohs(se->s_port);
	}
#ifdef TESTING
	fprintf(stderr, "Doing A record lookup for %s\n", tmpnode);
#endif
	err = resolve_host(tmpnode, port, &sum, 0, 0, &srvs);
    }

    coda_reorder_addrs(&srvs);

    /* append new addresses to the end */
    while (*res) res = &(*res)->ai_next;
    *res = srvs;

    free(tmpnode);
    return err;
}

void coda_freeaddrinfo(struct coda_addrinfo *res)
{
    while (res) {
	struct coda_addrinfo *ai = res;
	res = res->ai_next;

	if (ai->ai_addr)      free(ai->ai_addr);
	if (ai->ai_canonname) free(ai->ai_canonname);
	free(ai);
    }
}

void coda_reorder_addrs(struct coda_addrinfo **srvs)
{
    struct coda_addrinfo **tmp, *res, **tail;

    /* sort by priority, lowest first */
start:
    /* very simple sort, should be efficient for already sorted list */
    for (tmp = srvs; *tmp && (*tmp)->ai_next; tmp = &(*tmp)->ai_next) {
	struct coda_addrinfo *next = (*tmp)->ai_next;

	if ((*tmp)->ai_priority < next->ai_priority)
	    continue;

	/* move 0-weight items to the beginning of their priority */
	if ((*tmp)->ai_priority == next->ai_priority &&
	    ((*tmp)->ai_weight == 0 || next->ai_weight != 0))
	    continue;

	/* swap current and next */
	(*tmp)->ai_next = next->ai_next;
	next->ai_next = *tmp;
	*tmp = next;

	/* and start again from the beginning */
	goto start;
    }

    /* then order within each priority by weight */
    res = NULL; tail = &res;
    while(*srvs)
    {
	int total_weight = 0;

	/* calculate the sum of all weights of the lowest priority */
	for (tmp = srvs; *tmp; tmp = &(*tmp)->ai_next) {
	    if ((*tmp)->ai_priority != (*srvs)->ai_priority)
		break;
	    total_weight += (*tmp)->ai_weight + 1; 
	}

	while (total_weight > 0)
	{
	    int selector = (rand() % total_weight) + 1;

	    for (tmp = srvs; *tmp; tmp = &(*tmp)->ai_next) {
		selector -= (*tmp)->ai_weight + 1;
		if (selector <= 0)
		    break;
	    }

	    /* selected an entry, pull it off the list and append it to a
	     * temporary list */
	    total_weight -= (*tmp)->ai_weight + 1;

	    *tail = *tmp;
	    *tmp = (*tail)->ai_next;
	    tail = &(*tail)->ai_next;
	    *tail = NULL;
	}
    }
    *srvs = res;
}

#ifdef TESTING
int main(int argc, char **argv)
{
    struct coda_addrinfo *res = NULL, *p;
    struct sockaddr_in *sin;
    int err;

    struct coda_addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = PF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    err = coda_getaddrinfo(argv[1], argv[2], &hints, &res);
    printf("err: %d\n", err);

    coda_reorder_addrs(&res);

    for (p = res; p; p = p->ai_next) {
	printf("flags %d family %d socktype %d protocol %d\n",
	       p->ai_flags, p->ai_family, p->ai_socktype, p->ai_protocol);

	sin = (struct sockaddr_in *)p->ai_addr;
	printf("addrlen %d sin_family %d addr %s:%d\n",
	       p->ai_addrlen, sin->sin_family,
	       inet_ntoa(sin->sin_addr), ntohs(sin->sin_port));
	printf("canonical name %s\n", p->ai_canonname);
    }

    coda_freeaddrinfo(res);

    exit(0);
}
#endif
