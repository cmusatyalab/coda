#ifndef _SERVER_H_
#define _SERVER_H_

#include <netinet/in.h>
#include "refcounted.h"
#include "dllist.h"

class Server : protected RefCountedObject {
    friend class Realm;

public:
    Server(const struct in_addr *ipv4addr, struct dllist_head *head, Realm *r);
    ~Server(void);

    const struct in_addr *ipaddr(void) { return &ipv4addr; }

    void print(FILE *f);

private:
    struct in_addr ipv4addr;
    struct dllist_head servers;
    Realm *realm;
};

#endif /* _SERVER_H_ */

