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

#ifndef _CODA_GETADDRINFO_H_
#define _CODA_GETADDRINFO_H_

/*
 * This code handles DNS SRV record (RFC2782), and IN A lookups for a
 * specified service within a domain. In a perfect world 'getaddrinfo'
 * would do all of this for us ;)
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <sys/socket.h>
//#include <netdb.h>

#ifdef __CYGWIN__
/* cygwin netdb.h does not define these. */
#define EAI_ADDRFAMILY   1 
#define EAI_AGAIN        2 
#define EAI_BADFLAGS     3 
#define EAI_FAIL         4
#define EAI_FAMILY       5
#define EAI_MEMORY       6
#define EAI_NODATA       7
#define EAI_NONAME       8
#define EAI_SERVICE      9
#define EAI_SOCKTYPE    10
#define EAI_SYSTEM      11
#define EAI_BADHINTS    12
#define EAI_PROTOCOL    13
#define EAI_MAX         14
#endif

struct coda_addrinfo {
    int ai_flags;
#define CODA_AI_CANONNAME 1

    int ai_family;
    int ai_socktype;
    int ai_protocol;
    size_t ai_addrlen;
    struct sockaddr *ai_addr;
    char *ai_canonname;
    struct coda_addrinfo *ai_next;
    /* some additional SRV RR info */
    int ai_priority;
    int ai_weight;
    int ai_port;
};

int coda_getaddrinfo(const char *node, const char *service,
		     const struct coda_addrinfo *hints,
		     struct coda_addrinfo **res);

void coda_freeaddrinfo(struct coda_addrinfo *res);

void coda_reorder_addrs(struct coda_addrinfo **res);

#ifdef __cplusplus
}
#endif

#endif /* _CODA_GETADDRINFO_H_ */

