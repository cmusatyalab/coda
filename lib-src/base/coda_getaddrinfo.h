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
#include <netdb.h>

int coda_getaddrinfo(const char *node, const char *service,
		     const struct addrinfo *hints,
		     struct addrinfo **res);
void coda_freeaddrinfo(struct addrinfo *res);

#ifdef __cplusplus
}
#endif


#endif /* _CODA_GETADDRINFO_H_ */
