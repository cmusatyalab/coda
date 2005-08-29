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

/* structures for sorting port mappings -- optimization is on reads, with
   assumptions:
	- Most reads will be for a specific name using lookup_pbynvp
	- Few programs will have more than one version
	- Writes are infrequent, if not almost non-event
	- No one uses any protocol but UDP (17)
*/

#ifndef _MAP_H
#define _MAP_H

#include <dllist.h>
#include "portmapper.h"

/* Hash for a particular protocol -- by name */
#define NAMEHASHSIZE	397
#define MAXNAMELEN	256

struct protoentry
{
	char	*name;
	int	version, port, protocol;
	struct dllist_head chain;
};

/* functions available */

long portmap_bind(char *host);
void initnamehashtable(void);
struct protoentry *find_mapping(PM_Name name, PM_Version version,
				PM_Protocol protocol, PM_Port port);
void register_mapping(PM_Name name, PM_Version version, PM_Protocol protocol,
		      PM_Port port);
void delete_mapping(struct protoentry *pe);

#endif
