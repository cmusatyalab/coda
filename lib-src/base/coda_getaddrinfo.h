/* BLURB lgpl

                           Coda File System
                              Release 6

          Copyright (c) 2003 Carnegie Mellon University
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
#include <rpc2/rpc2_addrinfo.h>

#define CODA_AI_RES_SRV 0x8000

int coda_getaddrinfo(const char *node, const char *service,
		     const struct RPC2_addrinfo *hints,
		     struct RPC2_addrinfo **res);
void coda_reorder_addrinfo(struct RPC2_addrinfo **res);

#ifdef __cplusplus
}
#endif

#endif /* _CODA_GETADDRINFO_H_ */

