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

#include <stdio.h>
#include <string.h>
#include "server.h"
#include "realm.h"

Server::Server(const struct in_addr *addr, Realm *r)
{
    memcpy(&ipv4addr, addr, sizeof(struct in_addr));
    realm = r;
    list_head_init(&servers);
}

Server::~Server(void)
{
    list_del(&servers);
}

void Server::print(FILE *f)
{
    fprintf(f, " server '%s'\n", inet_ntoa(ipv4addr));
}

