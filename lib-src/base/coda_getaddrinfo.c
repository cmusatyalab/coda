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

/*
 * This code handles DNS SRV record (RFC2782), and IN A lookups for a
 * specified service within a domain. In a perfect world 'getaddrinfo'
 * would do all of this for us ;)
 */

/*
 * Special extended version of getaddrinfo for Coda,
 *   coda_getaddrinfo      - adds (ai_flags & CODA_AI_RES_SRV), which performs
 *			     SRV record lookups, additional information such as
 *			     priority and weights are stored in the returned
 *			     addrinfo list.
 *   coda_reorder_addrinfo - re-shuffles the addrinfo list according to the
 *			     RFC's recommendations for load-balancing among
 *			     multiple servers with the same priority.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_ARPA_NAMESER_H
#include <arpa/nameser.h>
#endif
#ifdef HAVE_RESOLV_H
#include <resolv.h>
#endif

#include "coda_getservbyname.h"

#ifndef NS_INT32SZ /* NetBSD and RedHat5 */
#define NS_INT32SZ INT32SZ
#define NS_INT16SZ INT16SZ
#define NS_HFIXEDSZ HFIXEDSZ
#define ns_t_srv T_SRV
#define ns_c_in  C_IN

#ifndef T_SRV /* MacOS X */
#define T_SRV 33
#endif /* !T_SRV */
#endif /* !NS_INT32SZ */

#ifdef PF_INET6
#ifndef HAVE_STRUCT_IN6_ADDR
struct in6_addr {
    uint8_t u6_addr[16];
};
#endif
#ifndef HAVE_STRUCT_SOCKADDR_IN6
struct sockaddr_in6 {
    uint16_t sin6_family;
    uint16_t sin6_port;
    uint32_t sin6_flowinfo;
    struct in6_addr sin6_addr;
};
#endif
#endif

#include "coda_getaddrinfo.h"

#ifdef HAVE_RES_SEARCH
static char *srvdomainname(const char *node, const char *service,
			   const char *proto)
{
    int len = strlen(service) + strlen(proto) + strlen(node) + 6;
    char *domain = malloc(len);

    if (domain)
	sprintf(domain, "_%s._%s.%s.", service, proto, node);

    return domain;
}

static int DN_HOST(unsigned char *msg, int mlen, unsigned char **ptr,
		   char *dest)
{
    int len = dn_expand(msg, msg + mlen, *ptr, dest, MAXHOSTNAMELEN);
    if (len < 0 || len > MAXHOSTNAMELEN) return -1;
    *ptr += len;
    return 0;
}

static int DN_SHORT(unsigned char *msg, int mlen, unsigned char **ptr, int *dest)
{
    if (*ptr + NS_INT16SZ > msg + mlen)
	return -1;
    *dest = ntohs(*(uint16_t *)*ptr);
    *ptr += NS_INT16SZ;
    return 0;
}

static int DN_INT(unsigned char *msg, int mlen, unsigned char **ptr, int *dest)
{
    if (*ptr + NS_INT32SZ > msg + mlen)
	return -1;
    *dest = ntohl(*(uint32_t *)*ptr);
    *ptr += NS_INT32SZ;
    return 0;
}

static int parse_res_reply(unsigned char *answer, int alen,
			   const struct RPC2_addrinfo *hints,
			   struct RPC2_addrinfo **res)
{
    unsigned char *p = answer;
    char name[MAXHOSTNAMELEN];
    int priority, weight, port, dummy;
    int err = RPC2_EAI_AGAIN, tmperr;

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
	struct RPC2_addrinfo *cur = NULL;

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

	/* according to the RFC, if there is a SRV record that has '.' as the
	 * target, the service is decidedly not available at this domain */
	if (name[0] == '.' && name[1] == '\0') {
	    err = RPC2_EAI_NONAME;
	    break;
	}

	tmperr = RPC2_getaddrinfo(name, NULL, hints, &cur);
	if (!tmperr) {
	    struct RPC2_addrinfo *p;
	    int i;

	    /* adjust weight depending on how many results we got */
	    if (weight) {
		for (p = cur, i = 0; p; p = p->ai_next, i++) /**/;
		weight /= i;
	    }

	    for (p = cur; p; p = p->ai_next) {
		switch(p->ai_family) {
		case PF_INET:
		    ((struct sockaddr_in *)p->ai_addr)->sin_port = htons(port);
		    break;
#ifdef PF_INET6
		case PF_INET6:
		    ((struct sockaddr_in6 *)p->ai_addr)->sin6_port = htons(port);
		    break;
#endif
		}
		p->ai_priority = priority;
		p->ai_weight = weight;
	    }
	    cur->ai_next = *res;
	    *res = cur;
	}
	if (!err || err == RPC2_EAI_AGAIN)
	    err = tmperr;
    }
    return err;
}

static int do_srv_lookup(const char *node, const char *service,
			 const struct RPC2_addrinfo *hints,
			 struct RPC2_addrinfo **res)
{
#ifdef TESTING
    fprintf(stderr, "Doing SRV record lookup for %s %s\n", node, service);
#endif

    unsigned char answer[1024];
    char *srvdomain;
    int len;
    const char *proto = (hints && hints->ai_protocol == IPPROTO_UDP) ?
	"udp" : "tcp";

    srvdomain = srvdomainname(node, service, proto);
    if (!srvdomain)
	return RPC2_EAI_MEMORY;

    len = res_search(srvdomain, ns_c_in, ns_t_srv, answer, sizeof(answer));

    free(srvdomain);
    
    /* make sure we actually got a usable answer */
    if (len < NS_HFIXEDSZ || len > sizeof(answer))
	return RPC2_EAI_FAIL;

    return parse_res_reply(answer, len, hints, res);
}
#endif

void coda_reorder_addrinfo(struct RPC2_addrinfo **srvs)
{
    struct RPC2_addrinfo **tmp, *res, **tail;

    /* sort by priority, lowest first */
start:
    /* very simple sort, should be efficient for already sorted list */
    for (tmp = srvs; *tmp && (*tmp)->ai_next; tmp = &(*tmp)->ai_next) {
	struct RPC2_addrinfo *next = (*tmp)->ai_next;

#ifdef PF_INET6
	/* Should we prefer ipv6 addresses? */
	if ((*tmp)->ai_family == PF_INET6 && next->ai_family == PF_INET)
	    continue;
#endif

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

int coda_getaddrinfo(const char *node, const char *service,
		     const struct RPC2_addrinfo *hints,
		     struct RPC2_addrinfo **res)
{
    struct RPC2_addrinfo Hints, *srvs = NULL;
    static int initialized = 0;
    int err = RPC2_EAI_FAIL;

    if (!initialized) {
#ifdef HAVE_RES_SEARCH
	/* assuming that if we have res_search, we'll have res_init and
	 * _res.options and so on */
	res_init();

	/* turn off default domain substitutions */
	_res.options &= ~(RES_DEFNAMES | RES_DNSRCH);
#endif
	initialized = 1;
    }

    err = RPC2_EAI_NONAME;
    if (hints && (hints->ai_flags & CODA_AI_RES_SRV))
    {
#ifdef PF_INET6
	char tmp[sizeof(struct in6_addr)];
#endif
	char *end;

	/* We want to clear the CODA_AI_RES_SRV, but hints is const
	 * so we make a copy */
	Hints = *hints;
	Hints.ai_flags &= ~CODA_AI_RES_SRV;
	hints = &Hints;

	/* we can only do an IN SRV record lookup if both node and
	 * service are specified and not numerical */
	if (!node || !service)
	    return RPC2_EAI_NONAME;

	if (hints->ai_flags & RPC2_AI_NUMERICHOST)
	    return RPC2_EAI_BADFLAGS;

	if (strtol(service, &end, 10) && *end == '\0')
	    return RPC2_EAI_BADFLAGS;

#ifdef PF_INET6
	/* check whether we were given an IP address in a format that doesn't
	 * match the hinted address family */
	if (hints->ai_family == PF_INET6 && inet_pton(PF_INET, node, &tmp) > 0)
	    return RPC2_EAI_BADFLAGS;

	if (hints->ai_family == PF_INET && inet_pton(PF_INET6, node, &tmp) > 0)
	    return RPC2_EAI_BADFLAGS;
#endif

#ifdef HAVE_RES_SEARCH
	/* try to find SRV records */
	err = do_srv_lookup(node, service, hints, &srvs);
	if (err == RPC2_EAI_NONAME) /* found a SRV record with a '.' target? */
	    goto Exit;

	if (!err) {
	    coda_reorder_addrinfo(&srvs);
	    goto Exit;
	}
#endif
    }

    /* when not doing SRV record lookup or when SRV lookup failed,
     * we fall back to a normal lookup */
    err = RPC2_getaddrinfo(node, service, hints, &srvs);
    if (err == RPC2_EAI_SERVICE) {
	/* try to map service to port number */
	const char *proto = NULL;
	struct servent *s;

	if (hints && hints->ai_socktype == SOCK_STREAM) proto = "tcp";
	if (hints && hints->ai_socktype == SOCK_DGRAM)  proto = "udp";
	s = coda_getservbyname(service, proto);

	if (s) {
	    char buf[6];
	    sprintf(buf, "%d", ntohs(s->s_port));
	    err = RPC2_getaddrinfo(node, buf, hints, &srvs);
	}
    }

Exit:
    /* append new addresses to the end of **res? */
    while (*res) res = &(*res)->ai_next;

    *res = srvs;
    return err;
}

#ifdef TESTING
int main(int argc, char **argv)
{
    struct RPC2_addrinfo *res = NULL, *p;
    struct sockaddr_in *sin;
    int err;

    struct RPC2_addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = PF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    err = coda_getaddrinfo(argv[1], argv[2], &hints, &res);
    printf("err: %d\n", err);

    coda_reorder_addrinfo(&res);

    for (p = res; p; p = p->ai_next) {
	char buf[RPC2_ADDRSTRLEN];
	printf("ai_flags %d ai_family %d ai_socktype %d ai_protocol %d\n",
	       p->ai_flags, p->ai_family, p->ai_socktype, p->ai_protocol);

	RPC2_formataddrinfo(p, buf, sizeof(*buf));
	printf("addrlen %d sin_family %d addr %s\n",
	       p->ai_addrlen, sin->sin_family, buf);
	printf("ai_canonname %s\n", p->ai_canonname);
	printf("ai_priority %d ai_weight %d\n", p->ai_priority, p->ai_weight);
    }
    RPC2_freeaddrinfo(res);

    exit(0);
}
#endif

