/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

/* structure management for an RPC2 portmapper */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <stdlib.h>
#include "coda_string.h"
#include "coda_assert.h"
#include <netinet/in.h>

#include <lwp/lwp.h>
#include <ports.h>
#include <rpc2/rpc2.h>
#include "portmapper.h"
#include "map.h"

struct dllist_head namehashtable[NAMEHASHSIZE];

/* takes a nul-terminated name, returns a hash % NAMEHASHSIZE */
int namehash(PM_Name name)
{
	/* Elfhash from UNIX SVR4 */
	int h=0, g;

	CODA_ASSERT(name);

	while (*name)
	{
		h = (h << 4) + *name++;
		if ((g = (h & 0xF0000000)))
			h ^= g >> 24;
		h &= ~g >> 24;
	}
	return (h % NAMEHASHSIZE);
}

/* initialize the name hash -- only call this once */
void initnamehashtable(void)
{
	int i;
	for (i = 0; i <NAMEHASHSIZE; i++)
		list_head_init(&namehashtable[i]);
}

/* find a service, return a null pointer if not found; rudimentary wildcard
   support -- ignore the following:
	version (if -1)
	protocol (if -1)
	port (if -1)
*/
struct protoentry *find_mapping(PM_Name name, PM_Version version,
				PM_Protocol protocol, PM_Port port)
{
	struct dllist_head *le;
	struct protoentry *pe;
	int bucket;

	CODA_ASSERT(name);

	bucket = namehash(name);

	/* not even anything in the name's bucket */
	if (list_empty(&namehashtable[bucket]))
		return NULL;

	for (le = namehashtable[bucket].next ;
	     le != &namehashtable[bucket];
	     le = le->next)
	{
		pe = list_entry(le, struct protoentry, chain);
		if (version != -1 && version != pe->version)
			continue;
		if (protocol != -1 && protocol != pe->protocol)
			continue;
		if (port != -1 && port != pe->port)
			continue;
		if (strcmp((char *)name, pe->name) != 0)
			continue;

		/* found it, or at least something remarkably similar */
		return pe;
	}

	return NULL;
}

/* register a new service -- assumes that there are no collisions -- be
   careful to delete the old one first */
void register_mapping(PM_Name name, PM_Version version, PM_Protocol protocol,
		      PM_Port port)
{
	struct protoentry *pe;
	int bucket;

	CODA_ASSERT(name);

	bucket = namehash(name);

	CODA_ASSERT(pe = (struct protoentry *)
			malloc(sizeof(struct protoentry)));

	pe->name = strdup((char *)name);
	pe->version = version;
	pe->protocol = protocol;
	pe->port = port;

	list_head_init(&pe->chain);
	list_add(&pe->chain, &namehashtable[bucket]);
}

/* given a pointer to an entry, delete it */
void delete_mapping(struct protoentry *pe)
{
	CODA_ASSERT(pe);
	CODA_ASSERT(pe->name);

	list_del(&pe->chain);

	free(pe->name);
	free(pe);
}

long portmap_bind(char *host)
{
	RPC2_BindParms bp;
	RPC2_HostIdent hident;
	RPC2_PortIdent pident;
	RPC2_SubsysIdent sident;
	RPC2_CountedBS cident;
	long	rc;
	RPC2_Handle	cid;
	struct timeval	timeout;

	hident.Tag = RPC2_HOSTBYNAME;
	strcpy(hident.Value.Name, host);

	pident.Tag = RPC2_PORTBYINETNUMBER;
	pident.Value.InetPortNumber = ntohs(PORT_rpc2portmap);

	sident.Tag = RPC2_SUBSYSBYID;
	sident.Value.SubsysId = htonl(PORTMAPPER_SUBSYSID);

	cident.SeqBody = 0;
	cident.SeqLen = 0;

	timeout.tv_sec = 20;
	timeout.tv_usec = 0;

	bp.SecurityLevel = RPC2_OPENKIMONO;
	bp.EncryptionType = 0;
	/*	bp.timeout = (struct timeval *) NULL; */
	bp.SharedSecret = 0;
	bp.ClientIdent = &cident;
	bp.SideEffectType = 0;
	bp.Color = 0;

	rc = RPC2_NewBinding(&hident, &pident, &sident, &bp, &cid);
	if ( ! rc ) 
		return cid;
	return 0;
}
