/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 2002 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#ifndef _PARSE_REALMS_H_
#define _PARSE_REALMS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <coda_getaddrinfo.h>

/* Given "name@realm", replaced the '@' with '\0' and returns a pointer to the
 * realm part. If the input matches either 'name@' or 'name' it doesn't change
 * the realm (but strips the '@'). */
void SplitRealmFromName(char *name, char **realm);

/* Given a realm name, returns a list of ip-addresses of the realm's root
 * servers. */
struct addrinfo *GetRealmServers(const char *realm_name);

#ifdef __cplusplus
}
#endif

#endif /* _PARSE_REALMS_H_ */
