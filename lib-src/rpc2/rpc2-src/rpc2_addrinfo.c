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
 * This code provides getaddrinfo functionality, but we use our own structures
 * so that we can provide the following functions,
 *   RPC2_allocaddrinfo  - converts a sockaddr to addrinfo.
 *   RPC2_copyaddrinfo   - performs a deep copy operation.
 *   RPC2_formataddrinfo - addrinfo to string conversion.
 *   RPC2_cmpaddrinfo    - test if we have a member of a linked list of
 *			   addrinfo structures.
 *
 * Then there are some convenience functions for RPC2,
 *   rpc2_resolve	 - resolves host/port ident structure to addrinfo.
 *   rpc2_splitaddrinfo  - breaks an addrinfo into host/port ident.
 *   rpc2_simplifyHost   - used to for compatibility with old rpc2 apps.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif

#include "rpc2.private.h"

/* internal helper functions */

#ifndef HAVE_GETADDRINFO
#ifndef HAVE_GETIPNODEBYNAME

/* flags */
#define AI_ALL 0

/* errorcodes */
#define HOST_TRY_AGAIN TRY_AGAIN
#define HOST_NOADDRESS NO_ADDRESS
#define HOST_NORECOVERY NO_RECOVERY

static struct hostent *getipnodebyname(const char *name, int af, int flags,
				       int *error_num)
{
    struct hostent *he = NULL;
    int err;

    if (af == PF_INET) {
	he = gethostbyname(name);
	err = h_errno;
    } else
	err = HOST_NOADDRESS;

    if (!he && error_num)
	*error_num = err;

    return he;
}

static void freehostent(struct hostent *ip)
{
    return;
}
#endif /* !HAVE_GETIPNODEBYNAME */

static struct RPC2_addrinfo *addrinfo_init(int family, const void *addr,
					   short port,
					   const struct RPC2_addrinfo *hints)
{
    struct sockaddr_storage ss;
    struct RPC2_addrinfo *ai;
    int addrlen = 0;
    memset(&ss, 0, sizeof(ss));

    switch (family) {
    case PF_INET:
	{
	    struct sockaddr_in *sin = (struct sockaddr_in *)&ss;
	    sin->sin_family = PF_INET;
	    sin->sin_addr = *(struct in_addr *)addr;
	    sin->sin_port = port;
	    addrlen = sizeof(*sin);
	    break;
	}
    case PF_INET6:
	{
	    struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)&ss;
	    sin6->sin6_family = PF_INET6;
	    sin6->sin6_addr = *(struct in6_addr *)addr;
	    sin6->sin6_port = port;
	    addrlen = sizeof(*sin6);
	    break;
	}
    }
    ai = RPC2_allocaddrinfo((struct sockaddr *)&ss, addrlen);
    if (ai) {
	if (hints) {
	    ai->ai_socktype = hints->ai_socktype;
	    ai->ai_protocol = hints->ai_protocol;
	} else {
	    ai->ai_socktype = SOCK_STREAM;
	    ai->ai_protocol = IPPROTO_TCP;
	}
    }
    return ai;
}

static int getaddrinfo_noresolve(const char *node, short port,
				 const struct RPC2_addrinfo *hints,
				 struct RPC2_addrinfo **res)
{
    struct RPC2_addrinfo *ai;
    int family = PF_UNSPEC;
    char addr[sizeof(struct in6_addr)];

    if (hints->ai_family != PF_INET6 &&
	node && inet_pton(PF_INET, node, &addr) > 0)
	family = PF_INET;

    if (hints->ai_family != PF_INET &&
	node && inet_pton(PF_INET6, node, &addr) > 0)
	family = PF_INET6;

    /* unspecified family and we couldn't figure it out from the address */
    if (family == PF_UNSPEC)
	return RPC2_EAI_NONAME;

    switch(family) {
    case PF_INET:
	{
	    struct in_addr *inaddr = (struct in_addr *)&addr;
	    if (!node) {
		if (hints && hints->ai_flags & RPC2_AI_PASSIVE)
		     inaddr->s_addr = INADDR_ANY;
		else inaddr->s_addr = INADDR_LOOPBACK;
	    }
	    break;
	}
    case PF_INET6:
	{
	    struct in6_addr *in6addr = (struct in6_addr *)&addr;
	    if (!node) {
		if (hints && hints->ai_flags & RPC2_AI_PASSIVE)
		     *in6addr = in6addr_any;
		else *in6addr = in6addr_loopback;
	    }
	    break;
	}
    }
    ai = addrinfo_init(family, &addr, port, hints);
    ai->ai_next = *res;
    *res = ai;
    return ai ? 0 : RPC2_EAI_MEMORY;
}
#endif /* !HAVE_GETADDRINFO */

/* exported helper functions */

struct RPC2_addrinfo *RPC2_allocaddrinfo(const struct sockaddr *addr,
					 size_t addrlen)
{
    struct RPC2_addrinfo *ai =
	(struct RPC2_addrinfo *)malloc(sizeof(*ai) + addrlen);

    if (!ai)
	return NULL;
    memset(ai, 0, sizeof(*ai));

    if (addrlen == sizeof(struct sockaddr_in))
	ai->ai_family = PF_INET;
    else if (addrlen == sizeof(struct sockaddr_in6))
	ai->ai_family = PF_INET6;
    else {
	free(ai);
	return NULL;
    }

    ai->ai_addr = (struct sockaddr *)&(ai[1]);
    ai->ai_addrlen = addrlen;
    memcpy(ai->ai_addr, addr, addrlen);
    return ai;
}

struct RPC2_addrinfo *RPC2_copyaddrinfo(const struct RPC2_addrinfo *ai)
{
    struct RPC2_addrinfo *head = NULL, *cur, *prev = NULL;
    const struct RPC2_addrinfo *src = ai;
    
    /* this loop probably a bit uglier than it should be because I'm
     * trying to keep the same order */
    while (src) {
        cur = RPC2_allocaddrinfo(src->ai_addr, src->ai_addrlen);
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

int RPC2_cmpaddrinfo(const struct RPC2_addrinfo *node,
		     const struct RPC2_addrinfo *host)
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

void RPC2_formataddrinfo(const struct RPC2_addrinfo *ai,
			 char *buf, size_t buflen)
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
        port = ((struct sockaddr_in6 *)ai->ai_addr)->sin6_port;
	break;
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


/* standard getaddrinfo functionality */

int RPC2_getaddrinfo(const char *node, const char *service,
		     const struct RPC2_addrinfo *hints,
		     struct RPC2_addrinfo **res)
{
#ifdef TESTING
    fprintf(stderr, "Doing A record lookup for %s %s\n", node, service);
#endif

#ifdef HAVE_GETADDRINFO
    struct addrinfo *ai = NULL, *head;
    struct RPC2_addrinfo **new, *list = NULL;
    int ret;

    ret = getaddrinfo(node, service, (const struct addrinfo *)hints, &ai);
    if (ret)
	return ret;

    head = ai;
    new = &list;
    while (ai) {
	*new = RPC2_allocaddrinfo(ai->ai_addr, ai->ai_addrlen);
	if (!*new) {
	    RPC2_freeaddrinfo(list);
	    list = NULL;
	    break;
	}

	(*new)->ai_flags    = ai->ai_flags;
	(*new)->ai_socktype = ai->ai_socktype;
	(*new)->ai_protocol = ai->ai_protocol;
	if (ai->ai_canonname)
	    (*new)->ai_canonname = strdup(ai->ai_canonname);

	new = &(*new)->ai_next;
	ai = ai->ai_next;
    }
    freeaddrinfo(head);

    if (!list)
	return RPC2_EAI_MEMORY;

    *new = *res;
    *res = list;
    return 0;
#else
    struct hostent *he;
    int err, family, i, resolved = 0;
    short port = 0;
    char *end;

    /* map the service name (or string formatted port number) into a usable
     * port number in network byte order */

    if (service) {
	port = htons(strtol(service, &end, 10));
	if (*service == '\0' || *end != '\0') {
	    const char *proto = (hints && hints->ai_protocol == IPPROTO_UDP) ?
		"udp" : "tcp";
	    struct servent *se = getservbyname(service, proto);
	    if (!se)
		return RPC2_EAI_SERVICE;
	    port = se->s_port;
	}
    }

    if (!node || (hints && hints->ai_flags & RPC2_AI_NUMERICHOST))
	return getaddrinfo_noresolve(node, port, hints, res);

    family = (hints && hints->ai_family == PF_INET) ? PF_INET : PF_INET6;

    he = getipnodebyname(node, family, (family == PF_INET6) ? AI_ALL : 0, &err);
    if (!he) {
	switch (err) {
	case HOST_TRY_AGAIN: return RPC2_EAI_AGAIN;
	case HOST_NOADDRESS: return RPC2_EAI_NODATA;
	case HOST_NOT_FOUND: return RPC2_EAI_NONAME;
	case HOST_NORECOVERY:
	default:	     return RPC2_EAI_FAIL;
	}
    }

    for (i = 0; he->h_addr_list[i]; i++) {
	struct RPC2_addrinfo *ai =
	    addrinfo_init(he->h_addrtype, he->h_addr_list[i], port, hints);
	if (!ai)
	    continue;

	if (hints && hints->ai_flags & RPC2_AI_CANONNAME)
	    ai->ai_canonname = strdup(he->h_name);

	ai->ai_next = *res;
	*res = ai;
	resolved++;
    }
    freehostent(he);

    return resolved ? 0 : (i ? RPC2_EAI_MEMORY : RPC2_EAI_NODATA);
#endif
}

void RPC2_freeaddrinfo(struct RPC2_addrinfo *res)
{
    while (res) {
	struct RPC2_addrinfo *ai = res;
	res = res->ai_next;

	if (ai->ai_canonname)
	    free(ai->ai_canonname);
	free(ai);
    }
}

const char *RPC2_gai_strerror(const int errcode)
{
#ifdef HAVE_GETADDRINFO
    return gai_strerror(errcode);
#else
    const char *str;
    switch (errcode) {
    case RPC2_EAI_FAMILY:     str = "Family not supported"; break;
    case RPC2_EAI_SOCKTYPE:   str = "Socktype not supported"; break;
    case RPC2_EAI_BADFLAGS:   str = "Invalid flags"; break;
    case RPC2_EAI_NONAME:     str = "Node or service not found"; break;
    case RPC2_EAI_SERVICE:    str = "Service not available"; break;
    case RPC2_EAI_ADDRFAMILY: str = "Bad address family"; break;
    case RPC2_EAI_NODATA:     str = "No known addresses"; break;
    case RPC2_EAI_MEMORY:     str = "Allocation failure"; break;
    case RPC2_EAI_FAIL:       str = "Permanent lookup failure"; break;
    case RPC2_EAI_AGAIN:      str = "Temporary lookup failure"; break;
    case RPC2_EAI_SYSTEM:     str = "System error"; break;
    }
    return str;
#endif
}


/* rpc2 specific helpers */

void rpc2_printaddrinfo(const struct RPC2_addrinfo *ai, FILE *f)
{
    char buf[RPC2_ADDRSTRLEN];
    RPC2_formataddrinfo(ai, buf, RPC2_ADDRSTRLEN);
    fputs("Addrinfo = ", f);
    fputs(buf, f);
}

struct RPC2_addrinfo *rpc2_resolve(RPC2_HostIdent *Host, RPC2_PortIdent *Port)
{
    struct RPC2_addrinfo hint, *result = NULL;
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

	default:
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
        hint.ai_flags = RPC2_AI_PASSIVE;

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

void rpc2_splitaddrinfo(RPC2_HostIdent *Host, RPC2_PortIdent *Port,
			const struct RPC2_addrinfo *ai)
{
    if (Host) {
        if (rpc2_ipv6ready) {
            Host->Tag = RPC2_HOSTBYADDRINFO;
            Host->Value.AddrInfo = RPC2_copyaddrinfo(ai);
        } else {
            assert(ai->ai_family == PF_INET);
            Host->Tag = RPC2_HOSTBYINETADDR;
            Host->Value.InetAddress = 
                ((struct sockaddr_in *)ai->ai_addr)->sin_addr;
        }
    }
    if (Port) {
        Port->Tag = RPC2_PORTBYINETNUMBER;
        switch (ai->ai_family) {
        case PF_INET:
            Port->Value.InetPortNumber =
                ((struct sockaddr_in *)ai->ai_addr)->sin_port;
            break;
        case PF_INET6:
            Port->Value.InetPortNumber =
                ((struct sockaddr_in6 *)ai->ai_addr)->sin6_port;
            break;
        default:
            Port->Tag = RPC2_DUMMYPORT;
        }
    }
}

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

	default:
	    break;
        }
    }

    Host->Tag = RPC2_HOSTBYADDRINFO;
    Host->Value.AddrInfo =
        RPC2_allocaddrinfo((struct sockaddr *)&sin, sizeof(sin));
}

