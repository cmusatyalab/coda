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

#include <string.h>
#include "coda_getservbyname.h"

#define PORT_rpc2portmap 369
#define PORT_codaauth2   370
#define PORT_venus       2430
#define PORT_venus_se    2431
#define PORT_codasrv     2432
#define PORT_codasrv_se  2433

struct servent *coda_getservbyname(const char *name, const char *proto)
{
    static struct servent s;
    struct servent *ps;

    ps = getservbyname(name, proto);
    if (ps) return ps;

    /* getservbyname failed, let's see if we happen to know the port number */

    /* Coda doesn't care about these, we have identical tcp and udp numbers */
    s.s_name = NULL;
    s.s_aliases = NULL;
    s.s_proto = NULL;

    if     (!strcmp(name, "rpc2portmap")) s.s_port = htons(PORT_rpc2portmap);
    else if (!strcmp(name, "codaauth2"))  s.s_port = htons(PORT_codaauth2);
    else if (!strcmp(name, "venus"))	  s.s_port = htons(PORT_venus);
    else if (!strcmp(name, "venus-se"))	  s.s_port = htons(PORT_venus_se);
    else if (!strcmp(name, "codasrv"))	  s.s_port = htons(PORT_codasrv);
    else if (!strcmp(name, "codasrv-se")) s.s_port = htons(PORT_codasrv_se);

    return s.s_port ? &s : NULL;
}

