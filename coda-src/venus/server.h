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

#ifndef _SERVER_H_
#define _SERVER_H_

#include <netinet/in.h>
#include "refcounted.h"
#include "dllist.h"

class Server : protected RefCountedObject {
    friend class Realm;

public:
    Server(const struct in_addr *ipv4addr, Realm *r);
    ~Server(void);

    const struct in_addr *ipaddr(void) { return &ipv4addr; }

    void print(FILE *f);

private:
    struct in_addr ipv4addr;
    struct dllist_head servers;
    Realm *realm;
};

#endif /* _SERVER_H_ */

