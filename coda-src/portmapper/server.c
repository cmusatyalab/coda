/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

/* server.c -- receive incoming RPCs and perform appropriate activities */

#include <sys/param.h>
#include <netinet/in.h>
#include <rpc2.h>
#include "portmapper.h"
#include "map.h"

/* return 1 if is a connection from localhost, else return 0 */
int portmapper_is_local_connection(RPC2_Handle cid)
{
	RPC2_PeerInfo pi;
	unsigned int ip;	/* wrong type, but RPC2's fault */

	RPC2_GetPeerInfo(cid, &pi);

	ip = ntohl(pi.RemoteHost.Value.InetAddress.s_addr);

	/* 127.0.0.1 */
#ifdef __CYGWIN32__	
       	return 1;
#endif
       
	return (ip == 0x7F000001);
}

/* Register a mapping between a name and port number -- only accepted from
   localhost -- this version of the call does not remove an old entry with
   the same name, etc.  Instead it returns PM_COLLISION */
long portmapper_server_register_excl(RPC2_Handle cid,
			PM_Name name,
			PM_Version version,
			PM_Protocol protocol,
			PM_Port port)
{
	struct protoentry *pe;

	/* only localhost is authorized to change mappings */
	if (!portmapper_is_local_connection(cid))
	{
		return PM_DENIED;
	}

	if ((version < 0) || (protocol < 0) || (port <0))
	{
		return PM_BADREQUEST;
	}

	if ((pe = find_mapping(name, version, protocol, -1)))
	{
		/* already a mapping */
		return PM_COLLISION;
	}

	/* addition is ok */

	register_mapping(name, version, protocol, port);

	return PM_SUCCESS;
}

/* As with register_excl, except that it removes the old entry silently if
   there is a collision */
long portmapper_server_register_sqsh(RPC2_Handle cid,
			PM_Name name,
			PM_Version version,
			PM_Protocol protocol,
			PM_Port port)
{
	struct protoentry *pe;

	/* only localhost is authorized to change mappings */
	if (!portmapper_is_local_connection(cid))
	{
		return PM_DENIED;
	}

        if ((version < 0) || (protocol < 0) || (port <0))       
        {
                return PM_BADREQUEST;
        }

	/* if there is a collision, delete the old one */
	if ((pe = find_mapping(name, version, protocol, -1)))
	{
		/* already a mapping */
		delete_mapping(pe);
	}

	/* addition is ok */

	register_mapping(name, version, protocol, port);

	return PM_SUCCESS;
}

/* Remove a mapping between a name and a port number -- only accepted from
   localhost */
long portmapper_server_delete(RPC2_Handle cid,
			PM_Name name,
			PM_Version version,
			PM_Protocol protocol)
{
	struct protoentry *pe;

	/* only localhost is authorized to change mappings */
	if (!portmapper_is_local_connection(cid))
	{
		return PM_DENIED;
	}

        if ((version < 0) || (protocol < 0))
        {
                return PM_BADREQUEST;
        }

	/* find the old one */
	if (!(pe = find_mapping(name, version, protocol, -1)))
	{
		/* not found */
		return PM_NOTFOUND;
	}

	/* removal is ok */

	delete_mapping(pe);

	return PM_SUCCESS;
}

/* Lookup port based on name, version, protocol */
long portmapper_server_lookup_pbynvp(RPC2_Handle cid,
			PM_Name name,
			PM_Version version,
			PM_Protocol protocol,
			PM_Port *port)
{
	struct protoentry *pe;

	if (!(pe = find_mapping(name, version, protocol, -1)))
	{
		/* not found */
		*port = 0;
		return PM_NOTFOUND;
	}

	/* found it */

	*port = pe->port;

	return PM_SUCCESS;
}

/* lookup a port/protocol based on name, version */
long portmapper_server_lookup_pbynv(RPC2_Handle cid,
			PM_Name name,
			PM_Version version,
			PM_Protocol *protocol,
			PM_Port *port)
{
	struct protoentry *pe;

	if (!(pe = find_mapping(name, version, -1, -1)))
	{
		/* not found */
		*port = *protocol = 0;
		return PM_NOTFOUND;
	}

	/* found it */

	*port = pe->port;
	*protocol = pe->protocol;

	return PM_SUCCESS;
}

long portmapper_server_lookup_pbyn(RPC2_Handle cid,
			PM_Name name,
			PM_Version *version,
			PM_Protocol *protocol,
			PM_Port *port)
{
	struct protoentry *pe;

	if (!(pe = find_mapping(name, -1, -1, -1)))
	{
		/* not found */
		*version = *port = *protocol = 0;
		return PM_NOTFOUND;
	}

	/* found it */

	*version = pe->version;
	*port = pe->port;
	*protocol = pe->protocol;

	return PM_SUCCESS;
}



