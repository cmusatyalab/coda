#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netdb.h>
#include <assert.h>

#include <rpc2/rpc2.h>
#include "rpc2.private.h"

int RPC2_getaddrinfo(const char *node, const char *service,
		     const struct rpc2_addrinfo *hints,
		     struct rpc2_addrinfo **res)
{
    struct addrinfo *result;
    int ret;

    /* here we use the fact that rpc2_addrinfo and addrinfo structures are
     * identical (except for possibly the allocation policy) */
    ret = getaddrinfo(node, service, (const struct addrinfo *)hints, &result);
    if (ret)
	return ret;

    *res = RPC2_copyaddrinfo((struct rpc2_addrinfo *)result);
    freeaddrinfo(result);

    return 0;
}

int RPC2_freeaddrinfo(struct rpc2_addrinfo *res)
{
    struct rpc2_addrinfo *p;
    while (res) {
	p = res;
	res = res->ai_next;

	if (p->ai_canonname)
	    free(p->ai_canonname);
	free(p);
    }
}

const char *RPC2_gai_strerror(int errcode)
{
    return gai_strerror(errcode);
}

void RPC2_ntop(struct rpc2_addrinfo *ai, char *buf, size_t buflen)
{
    int n, port = 0;

    if (!ai) {
	strncpy(buf, "(no addrinfo)", buflen-1);
	buf[buflen-1] = '\0';
	return;
    }

    if (ai->ai_canonname)
	strncpy(buf, ai->ai_canonname, buflen-1);

    else if (!inet_ntop(ai->ai_family, ai->ai_addr, buf, buflen))
	strncpy(buf, "(untranslatable)", buflen-1);

    n = strlen(buf);
    if (n < buflen - 3) {
	switch (ai->ai_family) {
	case AF_INET:
	    port = ((struct sockaddr_in *)ai->ai_addr)->sin_port; break;
	case AF_INET6:
	    port = ((struct sockaddr_in6 *)ai->ai_addr)->sin6_port; break;
	default: break;
	}
	if (port)
	    snprintf(&buf[n], buflen - n,":%u", port);
    }
    buf[buflen-1] = '\0';
}

/* deep copy an (rpc2_)addrinfo structure */
struct rpc2_addrinfo *RPC2_copyaddrinfo(struct rpc2_addrinfo *addrinfo)
{
    struct rpc2_addrinfo *head = NULL, *cur, *prev = NULL, *src = addrinfo;
    
    /* this loop probably a bit uglier than it should be because I'm
     * trying to keep the same order */
    while (src) {
	cur = rpc2_allocaddrinfo(src->ai_addr, src->ai_addrlen);
	if (!cur) {
	    RPC2_freeaddrinfo(head);
	    head = NULL;
	    break;
	    /* assert(0 && "Allocation failed in rpc2_copyaddrinfo"); */
	}

	/* copy the guts of the addrinfo structure */
	cur->ai_flags    = src->ai_flags;
	cur->ai_socktype = src->ai_socktype;
	cur->ai_protocol = src->ai_protocol;
	if (src->ai_canonname)
	    cur->ai_canonname = strdup(src->ai_canonname);
	cur->ai_next = NULL;

	/* link into the previously allocated addrinfo */
	if (prev)
	    prev->ai_next = cur;
	else
	    head = cur;

	prev = cur;
	src = src->ai_next;
    }
    return head;
}

int RPC2_cmpaddrinfo(struct rpc2_addrinfo *node, struct rpc2_addrinfo *host)
{
    if (!node && !host)
	return 1;

    for(; node != NULL; node = node->ai_next) {
	if (node->ai_family != host->ai_family)
	    continue;
	if (node->ai_addrlen != host->ai_addrlen)
	    continue;
	if (memcmp(node->ai_addr, host->ai_addr, host->ai_addrlen) != 0)
	    continue;
	return 1;
    }
    return 0;
}

struct rpc2_addrinfo *rpc2_allocaddrinfo(struct sockaddr *addr, size_t addrlen)
{
    struct rpc2_addrinfo *ai;
    ai = (struct rpc2_addrinfo *)malloc(sizeof(struct rpc2_addrinfo) + addrlen);
    if (!ai)
	return NULL;

    memset(ai, 0, sizeof(struct rpc2_addrinfo));

    if (addrlen == sizeof(struct sockaddr_in))
	ai->ai_family = AF_INET;
    else if (addrlen == sizeof(struct sockaddr_in6))
	ai->ai_family = AF_INET6;

    ai->ai_addr = (struct sockaddr *)&(ai[1]);
    ai->ai_addrlen = addrlen;
    memcpy(ai->ai_addr, addr, addrlen);

    return ai;
}

void rpc2_printaddrinfo(struct rpc2_addrinfo *ai, FILE *f)
{
    char addr[RPC2_ADDRSTRLEN];
    RPC2_ntop(ai, addr, RPC2_ADDRSTRLEN);
    fprintf(f, "AddrInfo = %s", addr);
}

/* Returns struct rpc2_addrinfo by resolving the input parameters.
 * Returns NULL on failure. */
struct rpc2_addrinfo *rpc2_resolve(RPC2_HostIdent *Host, RPC2_PortIdent *Port)
{
    struct rpc2_addrinfo hint, *result;
    char nbuf[INET_ADDRSTRLEN], pbuf[11];
    char *node = NULL, *service = NULL;
    int retval;

    /* shortcut, maybe even too short in some cases */
    if (Host && Host->Tag == RPC2_HOSTBYADDRINFO)
	return RPC2_copyaddrinfo(Host->Value.AddrInfo);

    /* Resolve port */
    if (Port) {
	switch (Port->Tag) {
	case RPC2_PORTBYNAME:
	    service = Port->Value.Name;
	    break;

	case RPC2_PORTBYINETNUMBER:
	    snprintf(pbuf, 11, "%u", ntohs(Port->Value.InetPortNumber));
	    service = pbuf;
	    break;
	}
    }

    /* Resolve host */
    memset(&hint, 0, sizeof(struct rpc2_addrinfo));
    hint.ai_family = PF_UNSPEC;
    hint.ai_socktype = SOCK_DGRAM;

    if (Host) {
	switch(Host->Tag) {
	case RPC2_HOSTBYINETADDR:	/* you passed it in in network order! */
	    if (!inet_ntop(AF_INET, &Host->Value.InetAddress, nbuf,
			   INET_ADDRSTRLEN)) {
		say(0, RPC2_DebugLevel, "rpc2_resolve: inet_ntop failed\n");
		return NULL;
	    }
	    node = nbuf;
	    hint.ai_flags = AI_NUMERICHOST;
	    break;

	case RPC2_HOSTBYNAME:
	    node = Host->Value.Name;
	    break;

	default:
	    break;
	}
    } else /* no host specified, I guess we're resolving for a listener */
	hint.ai_flags = AI_PASSIVE;

    retval = RPC2_getaddrinfo(node, service, &hint, &result);
    if (retval) {
	say(0, RPC2_DebugLevel, "rpc2_resolve: getaddrinfo failed\n");
	return NULL;
    }
    return result;
}

/* Fills in the HostIdent/PortIdent structures based on the rpc2_addrinfo */
void rpc2_splitaddrinfo(RPC2_HostIdent *Host, RPC2_PortIdent *Port,
			struct rpc2_addrinfo *addr)
{
    if (Host) {
	Host->Tag = RPC2_HOSTBYADDRINFO;
	Host->Value.AddrInfo = RPC2_copyaddrinfo(addr);
    }
    if (Port) {
	Port->Tag = RPC2_PORTBYINETNUMBER;
	switch (addr->ai_family) {
	case AF_INET:
	    Port->Value.InetPortNumber =
		((struct sockaddr_in *)addr->ai_addr)->sin_port;
	    break;
	case AF_INET6:
	    Port->Value.InetPortNumber =
		((struct sockaddr_in6 *)addr->ai_addr)->sin6_port;
	    break;
	default:
	    Port->Tag = RPC2_DUMMYPORT;
	}
    }
}

