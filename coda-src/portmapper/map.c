
/* $Id: map.c,v 1.2 1998/04/07 05:22:46 robert Exp $ */

/* structure management for an RPC2 portmapper */

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "portmapper.h"
#include "map.h"

struct protoentry *namehashtable[NAMEHASHSIZE];

/* takes a nul-terminated name, returns a hash % NAMEHASHSIZE */
int namehash(char *name)
{
	/* Elfhash from UNIX SVR4 */
	int h=0, g;

	assert(name);

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
	{
		namehashtable[i] = (struct protoentry *) 0;
	}
}

/* find a service, return a null pointer if not found; rudimentary wildcard
   support -- ignore the following:
	name (if null)
	version (if -1)
	protocol (if -1)
	port (if -1)
*/
struct protoentry *find_mapping(char *name, int version, int protocol, int port)
{

#define COMPARE(pe, name, version, protocol, port)		\
	( ((version != -1)&&(!(version == pe->version))) &&	\
	  ((protocol != -1)&&(!(protocol == pe->protocol))) &&	\
	  ((port != -1)&&(!(port == pe->port))) &&		\
	  ((name)&&(strcmp(name, pe->name)))			\
	)

	struct protoentry *temp;
	int bucket;

	assert(name);

	bucket = namehash(name);

	if (!namehashtable[bucket])
	{
		/* not even anything in the name's bucket */
		return 0;
	}

	for (temp=namehashtable[bucket]; temp &&
		COMPARE(temp, name, version, protocol, port); temp=temp->next)
	{
		/* nothing */
	}

	return temp;
#undef COMPARE
}

/* register a new service -- assumes that there are no collisions -- be
   careful to delete the old one first */
void register_mapping(char *name, int version, int protocol, int port)
{
	struct protoentry *temp;
	int bucket;

	assert(name);

	bucket = namehash(name);

	assert(temp = (struct protoentry *) malloc(sizeof(struct protoentry)));

	temp->next = namehashtable[bucket];
	namehashtable[bucket] = temp;
	if (temp->next)
		temp->next->prev = temp;
	temp->prev = (struct protoentry *) 0;

	temp->name = strdup(name);
	temp->version = version;
	temp->protocol = protocol;
	temp->port = port;
}

/* given a pointer to an entry, delete it */
void delete_mapping(struct protoentry *pe)
{
	int bucket;
	assert(pe);
	assert(pe->name);

	bucket = namehash(pe->name);
	
	if (pe->prev)
	{
		/* not first entry */
		pe->prev->next = pe->next;
	}
	else
	{
		/* first entry */
		namehashtable[bucket] = pe->next;
	}

	if (pe->next)
	{
		/* not last entry */
		pe->next->prev = pe->prev;
	}

	free(pe->name);
	free(pe);
}
