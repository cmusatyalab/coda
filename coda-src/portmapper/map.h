/* $Id: map.h,v 1.2 1998/08/05 23:49:32 braam Exp $ */

/* structures for sorting port mappings -- optimization is on reads, with
   assumptions:
	- Most reads will be for a specific name using lookup_pbynvp
	- Few programs will have more than one version
	- Writes are infrequent, if not almost non-event
	- No one uses any protocol but UDP (17)
*/

#ifndef _MAP_H
#define _MAP_H

/* Hash for a particular protocol -- by name */
#define NAMEHASHSIZE	397
#define MAXNAMELEN	256

struct protoentry
{
	char	*name;
	int	version, port, protocol;
	struct protoentry	*next, *prev;
};

/* functions available */

long portmap_bind(char *host);
void initnamehashtable(void);
struct protoentry *find_mapping(char *name, int version, int protocol, int port);
void register_mapping(char *name, int version, int protocol, int port);
void delete_mapping(struct protoentry *pe);


#endif
