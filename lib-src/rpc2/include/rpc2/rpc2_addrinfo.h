/* BLURB lgpl

                           Coda File System
                              Release 5

            Copyright (c) 2003 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights

#*/

#ifndef _RPC2_ADDRINFO_H_
#define _RPC2_ADDRINFO_H_

#include <sys/types.h>
#include <netinet/in.h>

/* struct addrinfo wrappers, by using our own versions we can support systems
 * that don't have these useful functions yet and can avoid allocation problems
 * when copying the struct around. But to simplify things we should definitely
 * try to keep the layout identical if the system already has getaddrinfo. */
struct RPC2_addrinfo {
    int ai_flags;
    int ai_family;
    int ai_socktype;
    int ai_protocol;
    size_t ai_addrlen;
    struct sockaddr *ai_addr;
    char *ai_canonname;
    struct RPC2_addrinfo *ai_next;
    int ai_priority; /* used by coda_getaddrinfo when looking up SRV records */
    int ai_weight;
};

/* These functions match their non RPC2_ counterparts */
int RPC2_getaddrinfo(const char *node, const char *service,
		     const struct RPC2_addrinfo *hints,
		     struct RPC2_addrinfo **res);
void RPC2_freeaddrinfo(struct RPC2_addrinfo *res);
const char *RPC2_gai_strerror(int errcode);

/* flag bits for ai_flags */
#define RPC2_AI_PASSIVE	      0x0001
#define RPC2_AI_CANONNAME     0x0002
#define RPC2_AI_NUMERICHOST   0x0004
#define RPC2_AI_RES_SRV       0x8000

/* errorcodes returned by RPC2_getaddrinfo */
#define RPC2_EAI_BADFLAGS   -1
#define RPC2_EAI_NONAME     -2
#define RPC2_EAI_AGAIN      -3
#define RPC2_EAI_FAIL       -4
#define RPC2_EAI_NODATA     -5
#define RPC2_EAI_FAMILY     -6
#define RPC2_EAI_SOCKTYPE   -7
#define RPC2_EAI_SERVICE    -8
#define RPC2_EAI_ADDRFAMILY -9
#define RPC2_EAI_MEMORY     -10
#define RPC2_EAI_SYSTEM     -11

/* copyaddrinfo is in my opinion missing from the getaddrinfo suite */
/* cmpaddrinfo tests whether 'host' matches any of the entries in 'node' */
struct RPC2_addrinfo *RPC2_allocaddrinfo(const struct sockaddr *addr,
					 size_t addrlen, int socktype,
					 int protocol);
struct RPC2_addrinfo *RPC2_copyaddrinfo(const struct RPC2_addrinfo *node);
int RPC2_cmpaddrinfo(const struct RPC2_addrinfo *node,
		     const struct RPC2_addrinfo *host);

/* this one is inspired by inet_ntop, but this adds the portnumber to the
 * output and only works for addrinfo structs. */
void RPC2_formataddrinfo(const struct RPC2_addrinfo *host,
			 char *buf, size_t buflen);

/* The maximum size that the previous buffer can be,
 * inet6 addr + ':' + portnumber + '\0' */
#define RPC2_ADDRSTRLEN (46 + 1 + 10 + 1)


#endif /* _RPC2_ADDRINFO_H */

