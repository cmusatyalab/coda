#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netdb.h>
#include <assert.h>

#include <rpc2/rpc2.h>
#include "rpc2.private.h"

int RPC2_getaddrinfo(const char *node, const char *service,
		     const struct RPC2_addrinfo *hints,
		     struct RPC2_addrinfo **res)
{
    struct addrinfo *result;
    int ret;

    /* here we use the fact that RPC2_addrinfo and addrinfo structures are
     * identical (except for possibly the allocation policy) */
    ret = getaddrinfo(node, service, (const struct addrinfo *)hints, &result);
    if (ret)
	return ret;

    *res = RPC2_copyaddrinfo((struct RPC2_addrinfo *)result);
    freeaddrinfo(result);

    return 0;
}

int RPC2_freeaddrinfo(struct RPC2_addrinfo *res)
{
    struct RPC2_addrinfo *p;
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

void RPC2_formataddrinfo(struct RPC2_addrinfo *ai, char *buf, size_t buflen)
{
    int n, port = 0;
    void *addr = NULL;

    if (!ai) {
	strncpy(buf, "(no addrinfo)", buflen-1);
	buf[buflen-1] = '\0';
	return;
    }
    switch (ai->ai_family) {
    case PF_INET:
	addr = &((struct sockaddr_in *)ai->ai_addr)->sin_addr;
	port = ((struct sockaddr_in *)ai->ai_addr)->sin_port;
	break;
    case PF_INET6:
	addr = &((struct sockaddr_in6 *)ai->ai_addr)->sin6_addr;
	port = ((struct sockaddr_in6 *)ai->ai_addr)->sin6_port; break;
    default: break;
    }

    if (ai->ai_canonname)
	strncpy(buf, ai->ai_canonname, buflen-1);

    else if (!addr || !inet_ntop(ai->ai_family, addr, buf, buflen))
	strncpy(buf, "(untranslatable)", buflen-1);

    n = strlen(buf);
    if (port && n < buflen - 3)
	snprintf(&buf[n], buflen - n,":%u", ntohs(port));

    buf[buflen-1] = '\0';
}

/* deep copy an (rpc2_)addrinfo structure */
struct RPC2_addrinfo *RPC2_copyaddrinfo(struct RPC2_addrinfo *addrinfo)
{
    struct RPC2_addrinfo *head = NULL, *cur, *prev = NULL, *src = addrinfo;
    
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

int RPC2_cmpaddrinfo(struct RPC2_addrinfo *node, struct RPC2_addrinfo *host)
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

struct RPC2_addrinfo *rpc2_allocaddrinfo(struct sockaddr *addr, size_t addrlen)
{
    struct RPC2_addrinfo *ai;
    ai = (struct RPC2_addrinfo *)malloc(sizeof(struct RPC2_addrinfo) + addrlen);
    if (!ai)
	return NULL;

    memset(ai, 0, sizeof(struct RPC2_addrinfo));

    if (addrlen == sizeof(struct sockaddr_in))
	ai->ai_family = PF_INET;
    else if (addrlen == sizeof(struct sockaddr_in6))
	ai->ai_family = PF_INET6;

    ai->ai_addr = (struct sockaddr *)&(ai[1]);
    ai->ai_addrlen = addrlen;
    memcpy(ai->ai_addr, addr, addrlen);

    return ai;
}

void rpc2_printaddrinfo(struct RPC2_addrinfo *ai, FILE *f)
{
    char addr[RPC2_ADDRSTRLEN];
    RPC2_formataddrinfo(ai, addr, RPC2_ADDRSTRLEN);
    fprintf(f, "AddrInfo = %s", addr);
}

/* Returns struct RPC2_addrinfo by resolving the input parameters.
 * Returns NULL on failure. */
struct RPC2_addrinfo *rpc2_resolve(RPC2_HostIdent *Host, RPC2_PortIdent *Port)
{
    struct RPC2_addrinfo hint, *result;
    char buf[11];
    char *node = NULL, *service = NULL;
    int retval;

    if (Host) {
	/* We can easily convert a numeric ip-address to an addrinfo
	 * structure without having to resolve the whole thing. */
	if (Host->Tag == RPC2_HOSTBYINETADDR) {
	    RPC2_HostIdent host = *Host;
	    rpc2_simplifyHost(&host, Port);
	    return host.Value.AddrInfo;
	}

	if (Host->Tag == RPC2_HOSTBYADDRINFO)
	    return RPC2_copyaddrinfo(Host->Value.AddrInfo);
    }

    /* Here we know that Host is either NULL, or RPC2_HOSTBYNAME
     * (or RPC2_DUMMYHOST) and we'll have to use the resolver */

    if (Port) {
	switch (Port->Tag) {
	case RPC2_PORTBYNAME:
	    service = Port->Value.Name;
	    break;

	case RPC2_PORTBYINETNUMBER:
	    snprintf(buf, 11, "%u", ntohs(Port->Value.InetPortNumber));
	    service = buf;
	    break;
	}
    }

    /* Resolve host */
    memset(&hint, 0, sizeof(struct RPC2_addrinfo));
    hint.ai_family = rpc2_ipv6ready ? PF_UNSPEC : PF_INET;
    hint.ai_socktype = SOCK_DGRAM;

    if (Host && Host->Tag == RPC2_HOSTBYNAME)
	node = Host->Value.Name;
    else /* no host specified, we must be resolving for a listener */
	hint.ai_flags = AI_PASSIVE;

    /* getaddrinfo doesn't allow both node and service to be NULL */
    if (!node && !service)
	service = "0";

    retval = RPC2_getaddrinfo(node, service, &hint, &result);
    if (retval) {
	say(0, RPC2_DebugLevel, "rpc2_resolve: getaddrinfo failed\n");
	return NULL;
    }
    return result;
}

/* Fills in the HostIdent/PortIdent structures based on the RPC2_addrinfo */
void rpc2_splitaddrinfo(RPC2_HostIdent *Host, RPC2_PortIdent *Port,
			struct RPC2_addrinfo *addr)
{
    if (Host) {
	if (rpc2_ipv6ready) {
	    Host->Tag = RPC2_HOSTBYADDRINFO;
	    Host->Value.AddrInfo = RPC2_copyaddrinfo(addr);
	} else {
	    assert(addr->ai_family == PF_INET);
	    Host->Tag = RPC2_HOSTBYINETADDR;
	    Host->Value.InetAddress = 
		((struct sockaddr_in *)addr->ai_addr)->sin_addr;
	}
    }
    if (Port) {
	Port->Tag = RPC2_PORTBYINETNUMBER;
	switch (addr->ai_family) {
	case PF_INET:
	    Port->Value.InetPortNumber =
		((struct sockaddr_in *)addr->ai_addr)->sin_port;
	    break;
	case PF_INET6:
	    Port->Value.InetPortNumber =
		((struct sockaddr_in6 *)addr->ai_addr)->sin6_port;
	    break;
	default:
	    Port->Tag = RPC2_DUMMYPORT;
	}
    }
}

/* We might be getting RPC2_HOSTBYINETADDR, but all the code uses
 * RPC2_HOSTBYADDRINFO. So this function just simplifies life */
void rpc2_simplifyHost(RPC2_HostIdent *Host, RPC2_PortIdent *Port)
{
    struct sockaddr_in sin;

    if (Host->Tag == RPC2_HOSTBYADDRINFO)
	return;

    assert(Host->Tag == RPC2_HOSTBYINETADDR);
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = PF_INET;
    sin.sin_addr = Host->Value.InetAddress;

    if (Port) {
	struct servent *se;

	switch(Port->Tag) {
	case RPC2_PORTBYINETNUMBER:
	    sin.sin_port = Port->Value.InetPortNumber;
	    break;

	case RPC2_PORTBYNAME:
	    se = getservbyname(Port->Value.Name, "udp");
	    if (se)
		sin.sin_port = se->s_port;
	    break;
	}
    }

    Host->Tag = RPC2_HOSTBYADDRINFO;
    Host->Value.AddrInfo =
	rpc2_allocaddrinfo((struct sockaddr *)&sin, sizeof(sin));
}

