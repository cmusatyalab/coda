#include <stdio.h>
#include <string.h>
#include "server.h"
#include "realm.h"

Server::Server(const struct in_addr *addr, struct dllist_head *head, Realm *r)
{
    memcpy(&ipv4addr, addr, sizeof(struct in_addr));
    realm = r;
    rootserver = 0;

    list_add(&servers, head);
}

Server::~Server(void)
{
    list_del(&servers);
}

void Server::print(FILE *f)
{
    fprintf(f, " server '%s'\n", inet_ntoa(ipv4addr));
}

