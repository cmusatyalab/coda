/* BLURB lgpl

                           Coda File System
                              Release 6

          Copyright (c) 2002-2008 Carnegie Mellon University
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

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
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

static int addrinfo_init(int family, const void *addr, short port,
			 const struct RPC2_addrinfo *hints,
			 struct RPC2_addrinfo **res)
{
    int socktype = SOCK_STREAM, protocol = IPPROTO_TCP;
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
#if defined(PF_INET6)
    case PF_INET6:
	{
	    struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)&ss;
	    sin6->sin6_family = PF_INET6;
	    sin6->sin6_addr = *(struct in6_addr *)addr;
	    sin6->sin6_port = port;
	    addrlen = sizeof(*sin6);
	    break;
	}
#endif
    default:
	return RPC2_EAI_FAMILY;
    }

    if (hints) {
	socktype = hints->ai_socktype;
	protocol = hints->ai_protocol;
    }

    ai = RPC2_allocaddrinfo((struct sockaddr *)&ss, addrlen,
			    socktype, protocol);
    if (!ai)
	return RPC2_EAI_MEMORY;

    ai->ai_next = *res;
    *res = ai;
    return 0;
}

static int getaddrinfo_noresolve(const char *node, short port,
				 const struct RPC2_addrinfo *hints,
				 struct RPC2_addrinfo **res)
{
    struct RPC2_addrinfo *ai;
    int family = hints ? hints->ai_family : PF_UNSPEC;
#if defined(PF_INET6)
    char addr[sizeof(struct in6_addr)];
#else
    char addr[sizeof(struct in_addr)];
#endif
    int err4, err6;

    err4 = err6 = RPC2_EAI_NONAME;
#if defined(PF_INET6)
    if (!hints || hints->ai_family != PF_INET6) /* PF_UNSPEC || PF_INET */
#endif
    {
	struct in_addr addr;
	if (!node) {
	    if (hints && hints->ai_flags & RPC2_AI_PASSIVE)
		 addr.s_addr = INADDR_ANY;
	    else addr.s_addr = INADDR_LOOPBACK;
	}
	else if (inet_pton(PF_INET, node, &addr) <= 0)
	    goto v4_not_found;

	err4 = addrinfo_init(PF_INET, &addr, port, hints, res);
    }
v4_not_found:

#if defined(PF_INET6)
    if (!hints || hints->ai_family != PF_INET) /* PF_UNSPEC || PF_INET6 */
    {
	struct in6_addr addr;
	if (!node) {
	    if (hints && hints->ai_flags & RPC2_AI_PASSIVE)
		 addr = in6addr_any;
	    else addr = in6addr_loopback;
	}
	else if (inet_pton(PF_INET6, node, &addr) <= 0)
	    goto v6_not_found;

	err6 = addrinfo_init(PF_INET6, &addr, port, hints, res);
    }
v6_not_found:
#endif

    /* did we find any usable address? */
    if (!err4 || !err6)
	return 0;

    /* perhaps we had an allocation error? */
    if (err4 == RPC2_EAI_MEMORY || err6 == RPC2_EAI_MEMORY)
	return RPC2_EAI_MEMORY;

    /* unable to get an ip-address from the passed arguments */
    return RPC2_EAI_NONAME;
}
#endif /* !HAVE_GETADDRINFO */

/* exported helper functions */

struct RPC2_addrinfo *RPC2_allocaddrinfo(const struct sockaddr *addr,
					 size_t addrlen, int socktype,
					 int protocol)
{
    struct RPC2_addrinfo *ai =
	(struct RPC2_addrinfo *)malloc(sizeof(*ai) + addrlen);

    if (!ai)
	return NULL;
    memset(ai, 0, sizeof(*ai));

    ai->ai_family = addr->sa_family;
    ai->ai_socktype = socktype;
    ai->ai_protocol = protocol;
    ai->ai_addrlen = addrlen;
    ai->ai_addr = (struct sockaddr *)&(ai[1]);
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
        cur = RPC2_allocaddrinfo(src->ai_addr, src->ai_addrlen,
				 src->ai_socktype, src->ai_protocol);
        if (!cur) {
            RPC2_freeaddrinfo(head);
            head = NULL;
            break;
            /* assert(0 && "Allocation failed in rpc2_copyaddrinfo"); */
        }

        /* copy the guts of the addrinfo structure */
        cur->ai_flags = src->ai_flags;
        if (src->ai_canonname)
            cur->ai_canonname = strdup(src->ai_canonname);
        cur->ai_next = NULL;

        /* link into the previously allocated addrinfo */
        if (prev)
             prev->ai_next = cur;
        else head = cur;

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

	switch (node->ai_family) {
	case PF_INET:
	    {
		struct sockaddr_in *sinN, *sinH;
		sinN = (struct sockaddr_in *)node->ai_addr;
		sinH = (struct sockaddr_in *)host->ai_addr;

		if (host->ai_addrlen == sizeof(struct sockaddr_in) &&
		    sinN->sin_port == sinH->sin_port		   &&
		    memcmp(&sinN->sin_addr, &sinH->sin_addr,
			   sizeof(sinN->sin_addr)) == 0)
		    return 1;
		break;
	    }
#if defined(PF_INET6)
	case PF_INET6:
	    {
		struct sockaddr_in6 *sin6N, *sin6H;
		sin6N = (struct sockaddr_in6 *)node->ai_addr;
		sin6H = (struct sockaddr_in6 *)host->ai_addr;

		if (host->ai_addrlen == sizeof(struct sockaddr_in6) &&
		    sin6N->sin6_port == sin6H->sin6_port	    &&
		    memcmp(&sin6N->sin6_addr, &sin6H->sin6_addr,
			   sizeof(sin6N->sin6_addr)) == 0)
		    return 1;
		break;
	    }
#endif
	default:
	    break;
	}
    }
    return 0;
}

void rpc2_formataddrinfo(const struct RPC2_addrinfo *ai, char *buf,
			 size_t buflen, int use_canonname)
{
    int n, port = 0;
    void *addr = NULL;
    char *p = buf;

    buflen--;

    if (!ai) {
	strncpy(buf, "(no addrinfo)", buflen);
	buf[buflen] = '\0';
	return;
    }

    switch (ai->ai_family) {
    case PF_INET:
	addr = &((struct sockaddr_in *)ai->ai_addr)->sin_addr;
	port = ((struct sockaddr_in *)ai->ai_addr)->sin_port;
	break;
#if defined(PF_INET6)
    case PF_INET6:
	addr = &((struct sockaddr_in6 *)ai->ai_addr)->sin6_addr;
	port = ((struct sockaddr_in6 *)ai->ai_addr)->sin6_port;
	*(p++) = '[';
	break;
#endif
    }

    /* only use the hostname if it is available and we can comfortably pack
     * it with the port number in the buffer */
    if (use_canonname && ai->ai_canonname &&
        strlen(ai->ai_canonname) < (buflen - 12))
    {
	strncpy(buf, ai->ai_canonname, buflen);
	p = buf;
    }
    else if (!addr || !inet_ntop(ai->ai_family, addr, p, buf + buflen - p))
    {
	strncpy(buf, "(untranslatable)", buflen);
	p = buf;
    }
    buf[buflen] = '\0'; /* just in case inet_ntop didn't \0 terminate */

    n = strlen(buf);
    if (port && n < buflen - 3)
	snprintf(&buf[n], buflen - n,"%s:%u", p != buf ? "]" : "", ntohs(port));
    buf[buflen] = '\0';
}

void RPC2_formataddrinfo(const struct RPC2_addrinfo *ai, char *buf,
			 size_t buflen)
{
	rpc2_formataddrinfo(ai, buf, buflen, 1);
}

/* standard getaddrinfo functionality */

int RPC2_getaddrinfo(const char *node, const char *service,
		     const struct RPC2_addrinfo *hints,
		     struct RPC2_addrinfo **res)
{
#ifdef TESTING
    fprintf(stderr, "Doing A record lookup for %s %s\n",
            node ? node : "(null)", service ? service : "(null)");
#endif

#ifdef HAVE_GETADDRINFO
    struct addrinfo *ai = NULL, *head;
    struct RPC2_addrinfo **new, *list = NULL;
    int ret;

    ret = getaddrinfo(node, service, (const struct addrinfo *)hints, &ai);
    /* map getaddrinfo error codes into RPC2_getaddrinfo errorcodes */
    switch (ret) {
    case 0:		 break;
    case EAI_FAMILY:	 return RPC2_EAI_FAMILY;
    case EAI_SOCKTYPE:	 return RPC2_EAI_SOCKTYPE;
    case EAI_BADFLAGS:	 return RPC2_EAI_BADFLAGS;
    case EAI_NONAME:	 return RPC2_EAI_NONAME;
    case EAI_SERVICE:	 return RPC2_EAI_SERVICE;
    case EAI_MEMORY:	 return RPC2_EAI_MEMORY;
    case EAI_FAIL:	 return RPC2_EAI_FAIL;
    case EAI_AGAIN:	 return RPC2_EAI_AGAIN;
    case EAI_SYSTEM:	 return RPC2_EAI_SYSTEM;
    default:		 return RPC2_EAI_FAIL;
    }

    head = ai;
    new = &list;
    while (ai) {
	*new = RPC2_allocaddrinfo(ai->ai_addr, ai->ai_addrlen,
				  ai->ai_socktype, ai->ai_protocol);
	if (!*new) {
	    RPC2_freeaddrinfo(list);
	    list = NULL;
	    break;
	}

	(*new)->ai_flags = ai->ai_flags;
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

#if defined (PF_INET6)
    family = (hints && hints->ai_family == PF_INET) ? PF_INET : PF_INET6;
    he = getipnodebyname(node, family, (family == PF_INET6) ? AI_ALL : 0, &err);
#else
    family = PF_INET;
    he = getipnodebyname(node, family, 0, &err);

#endif

    if (!he) {
	switch (err) {
	case HOST_TRY_AGAIN: return RPC2_EAI_AGAIN;
	case HOST_NOADDRESS:
	case HOST_NOT_FOUND: return RPC2_EAI_NONAME;
	case HOST_NORECOVERY:
	default:	     return RPC2_EAI_FAIL;
	}
    }

    for (i = 0; he->h_addr_list[i]; i++) {
	int err = 
	    addrinfo_init(he->h_addrtype, he->h_addr_list[i], port, hints, res);
	if (err)
	    continue;

	if (hints && hints->ai_flags & RPC2_AI_CANONNAME)
	    (*res)->ai_canonname = strdup(he->h_name);

	resolved++;
    }
    freehostent(he);

    return resolved ? 0 : (i ? RPC2_EAI_MEMORY : RPC2_EAI_NONAME);
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
#ifdef HAVE_GAI_STRERROR
    return gai_strerror(errcode);
#else
    const char *str;
    switch (errcode) {
    case RPC2_EAI_FAMILY:     str = "Family not supported"; break;
    case RPC2_EAI_SOCKTYPE:   str = "Socktype not supported"; break;
    case RPC2_EAI_BADFLAGS:   str = "Invalid flags"; break;
    case RPC2_EAI_NONAME:     str = "Node or service not found"; break;
    case RPC2_EAI_SERVICE:    str = "Service not available"; break;
    case RPC2_EAI_MEMORY:     str = "Allocation failure"; break;
    case RPC2_EAI_FAIL:       str = "Permanent lookup failure"; break;
    case RPC2_EAI_AGAIN:      str = "Temporary lookup failure"; break;
    case RPC2_EAI_SYSTEM:     str = "System error"; break;
    default:		      str = "Unknown failure"; break;
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
#if defined(PF_INET6)
        case PF_INET6:
            Port->Value.InetPortNumber =
                ((struct sockaddr_in6 *)ai->ai_addr)->sin6_port;
            break;
#endif
        default:
            Port->Tag = RPC2_DUMMYPORT;
        }
    }
}

void rpc2_simplifyHost(RPC2_HostIdent *Host, RPC2_PortIdent *Port)
{
    struct sockaddr_in sin;
    struct RPC2_addrinfo *ai;

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

    ai = RPC2_allocaddrinfo((struct sockaddr *)&sin, sizeof(sin),
			    SOCK_DGRAM, IPPROTO_UDP);
    assert(ai != NULL);

    Host->Tag = RPC2_HOSTBYADDRINFO;
    Host->Value.AddrInfo = ai;
}

