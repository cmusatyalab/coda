/* BLURB lgpl

			Coda File System
			    Release 6

	      Copyright (c) 2007 Carnegie Mellon University
		    Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

			Additional copyrights
			    none currently
#*/

#ifndef _CODA_GETSERVBYNAME_H_
#define _CODA_GETSERVBYNAME_H_

/*
 * Some systems *cough*Windows*cough* are missing the IANA registered Coda
 * ports in /etc/services, so getservbyname fails. Instead of adding fallback
 * code all over the place, we use a Coda specific wrapper so we only need the
 * server->port mappings in a single location.
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif

/* wrapper around getservbyname */
struct servent *coda_getservbyname(const char *name, const char *proto);

#ifdef __cplusplus
}
#endif

#endif /* _CODA_GETSERVBYNAME_H_ */
