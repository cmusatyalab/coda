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

#include "coda_getaddrinfo.h"

#if 0
/* We need the official protocol name for this service for the SRV record
 * lookup. Having hints will help, otherwise we have to fallback on trying
 * getservbyname for the various protocols. */
static char *get_proto_from_hints(const char *service,
				  const struct addrinfo *hints)
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


struct srv {
    char name[MAXHOSTNAMELEN];
    int  port;
    int  priority;
    int  weight;
    struct srv *next;
};

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

static int parse_res_reply(char *answer, int alen, const struct summary *sum,
			   struct srv **srvs)
{
    char *p = answer, name[MAXHOSTNAMELEN];
    int dummy;

    /* arghhhhh, I don't like digging through libresolv output */
    p += NS_HFIXEDSZ; /* what is in the header? probably nothing interesting */

    /* skip original query + type + class */
    if (DN_HOST(answer, alen, &p, name) ||
	DN_SHORT(answer, alen, &p, &dummy) ||
        DN_SHORT(answer, alen, &p, &dummy))
	return EAI_AGAIN; /* corrupted packet, retryable? */

    while (p < answer + alen) {
	int type, size;
	struct srv *new;

	if (DN_HOST(answer, alen, &p, name) ||
	    DN_SHORT(answer, alen, &p, &type) ||
	    DN_SHORT(answer, alen, &p, &dummy) || // class
	    DN_INT(answer, alen, &p, &dummy) ||   // ttl
	    DN_SHORT(answer, alen, &p, &size))
	{
	    break; /* corrupted packet? */
	}

	if (type != ns_t_srv) {
	    p += size; 
	    continue;
	}

	new = malloc(sizeof(*new));
	if (!new) {
	    p += size;
	    continue;
	}

	if (DN_SHORT(answer, alen, &p, &new->priority) ||
	    DN_SHORT(answer, alen, &p, &new->weight) ||
	    DN_SHORT(answer, alen, &p, &new->port) ||
	    DN_HOST(answer, alen, &p, new->name))
	{
	    free(new);
	    break; /* corrupted packet? */
	}
	/* easier to work with */
	new->weight++;

	//fprintf(stderr, "got srv record for %s:%d\n", new->name, new->port);
	if (new->name[0] == '.' && new->name[1] == '\0') {
	    free(new);
	    continue;
	}

	new->next = *srvs;
	*srvs = new;
    }
    return *srvs ? 0 : EAI_AGAIN;
}

static int srv_compare(const void *A, const void *B)
{
    struct srv *a = (struct srv *)A, *b = (struct srv *)B; 

    if (a->priority < b->priority) return -1;
    if (a->priority > b->priority) return 1;

    if (a->weight < b->weight) return -1;
    if (a->weight > b->weight) return 1;

    return 0;
}

static void reorder_srvs(struct srv **srvs)
{
    struct srv *p, **srvlist;
    int i, len = 0, total_weight = 0;

    /* sort by priority, lowest first */
    /* how many entries do we have */
    for (p = *srvs ; p; p = p->next) len++;

    /* make an array and sort it */
    srvlist = malloc(len * sizeof(*srvlist));
    p = *srvs;
    for (i = 0; i < len; i++) {
	srvlist[i] = p;
	p = p->next;
    }
    qsort(srvlist, len, sizeof(*srvlist), srv_compare);

    /* pull everything back out ordered by weight */
    *srvs = NULL;
    while(1) {
	int priority = -1;
	for (i = 0; i < len; i++) {
	    if (!srvlist[i]) continue;
	    if (priority == -1)
		priority = srvlist[i]->priority;
	    if (srvlist[i]->priority != priority)
		break;
	    total_weight += srvlist[i]->weight;
	}
	/* done? */
	if (priority == -1)
	    break;

	while (total_weight > 0) {
	    int current = rand() % total_weight;
	    for (i = 0; i < len; i++) {
		if (!srvlist[i]) continue;
		current -= srvlist[i]->weight;
		if (current <= 0) {
		    total_weight -= srvlist[i]->weight;

		    srvlist[i]->next = *srvs;
		    *srvs = srvlist[i];
		    srvlist[i] = NULL;
		}
	    }
	}
    }
    free(srvlist);
}

static int resolve_host(const char *name, int port, const struct summary *sum,
			struct addrinfo **res)
{
    struct hostent *he;
    int i, resolved = 0;

    //fprintf(stderr, "resolving %s\n", name);

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

    for (i = 0; he->h_addr_list[i]; i++) {
	struct addrinfo *ai;
	struct sockaddr_in *sin;

	ai = malloc(sizeof(*ai));
	if (!ai) break;
	memset(ai, 0, sizeof(*ai));

	ai->ai_family    = sum->family;
	ai->ai_socktype  = sum->socktype;
	ai->ai_protocol  = sum->protocol;
	if (sum->flags & AI_CANONNAME)
	    ai->ai_canonname = strdup(he->h_name);

	sin = malloc(sizeof(*sin));
	if (!sin) {
	    free(ai);
	    break;
	}
	memset(sin, 0, sizeof(*sin));

	sin->sin_family = AF_INET;
	sin->sin_port = htons(port);
	sin->sin_addr = *(struct in_addr *)he->h_addr_list[i];

	ai->ai_addrlen   = sizeof(*sin);
	ai->ai_addr      = (struct sockaddr *)sin;

	ai->ai_next = *res;
	*res = ai;
	resolved++;
	//fprintf(stderr, "got server %s:%d\n", inet_ntoa(sin->sin_addr), ntohs(sin->sin_port));
    }

#ifdef HAVE_GETIPNODEBYNAME
    freehostent(he);
#endif

    return resolved ? 0 : (i ? EAI_MEMORY : EAI_NODATA);
}


static int do_srv_lookup(const char *realm, const char *service,
			 const struct summary *sum, struct addrinfo **res)
{
    struct srv *srvs = NULL;
    char answer[1024], *srvdomain;
    int err, len, resolved = 0;
    
    srvdomain = srvdomainname(realm, service, sum);
    if (!srvdomain)
	return EAI_MEMORY;

    len = res_search(srvdomain, ns_c_in, ns_t_srv, answer, sizeof(answer));

    free(srvdomain);
    
    if (len == -1)
	return EAI_FAIL;

    err = parse_res_reply(answer, len, sum, &srvs);
    if (err)
	return err;

    reorder_srvs(&srvs);

    /* turn all srvs into resolved addrinfo structs */
    *res = NULL;
    err = 0;
    while (srvs) {
	struct srv *this = srvs;
	int ret;

	srvs = srvs->next;

	ret = resolve_host(this->name, this->port, sum, res);
	if (!err) err = ret;
	else resolved++;

	free(this);
    }

    return resolved ? 0 : err;
}

int coda_getaddrinfo(const char *node, const char *service,
		     const struct addrinfo *hints,
		     struct addrinfo **res)
{
    struct summary sum = { PF_UNSPEC, 0, 0, 0 };
    int err;

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

    if (!node && !service)
	return EAI_NONAME;

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

    err = EAI_NONAME;
    if (node && service)
	/* try to find SRV records */
	err = do_srv_lookup(node, service, &sum, res);

    /* fall back to A records */
    if (err) {
	char *proto = (sum.protocol == IPPROTO_TCP) ? "tcp" : "udp";
	struct servent *se = getservbyname(service, proto);
	if (!se) return EAI_SERVICE;

	err = resolve_host(node, se->s_port, &sum, res);
    }

    return err;
}

void coda_freeaddrinfo(struct addrinfo *res)
{
    while (res) {
	struct addrinfo *ai = res;
	res = res->ai_next;

	if (ai->ai_addr)      free(ai->ai_addr);
	if (ai->ai_canonname) free(ai->ai_canonname);
	free(ai);
    }
}

#ifdef TESTING
int main(int argc, char **argv)
{
    struct addrinfo *res = NULL, *p;
    struct sockaddr_in *sin;
    int err;

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = PF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    err = coda_getaddrinfo(argv[1], argv[2], &hints, &res);
    printf("err: %d\n", err);

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
