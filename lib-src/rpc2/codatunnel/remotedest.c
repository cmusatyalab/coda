/* BLURB lgpl

                           Coda File System
                              Release 6

          Copyright (c) 2017-2018 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights

#*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <uv.h>

#include "codatunnel.private.h"

/* Code to track and manage known destinations We use a very simple
   array of fixed length, and brute force search to get started.
   Later, this can be made into a hash table or other more efficient
   data structure
*/

#define DESTARRAY_SIZE 100   /* should be plenty for early debugging */
dest_t destarray[DESTARRAY_SIZE]; /* only 0..hilimit-1 are in use */
int hilimit = 0; /* one plus highest index in use in destarray */

void cleardest(dest_t *d)
{
    memset(&d->destaddr, 0, sizeof(struct sockaddr_storage));
    d->destlen = 0;
    d->state = FREE;
    d->tcphandle = NULL;
    d->received_packet = NULL;
    d->nextbyte = 0;
    d->ntoh_done = 0;
    d->packets_sent = 0;
}


void initdestarray()
{/* initialize the global data structure, so that
    the destaddr fields are completely zeroed for memcmp()
    comparisons in later getdest() calls; otherwise padding
    in structures may cause trouble */
    int i;

    hilimit = 0;

    for (i = 0; i < DESTARRAY_SIZE; i++)
        cleardest(&destarray[i]);
}

static int sockaddr_equal(const struct sockaddr_storage *a,
                          const struct sockaddr_storage *b,
                          socklen_t len)
{
    if (a->ss_family != b->ss_family)
        return 0;

    switch(a->ss_family) {
    case AF_INET: {
        struct sockaddr_in *a_in = (struct sockaddr_in *)a;
        struct sockaddr_in *b_in = (struct sockaddr_in *)b;

        if (len < sizeof(struct sockaddr_in))
            return 0;

        return (a_in->sin_port == b_in->sin_port &&
                a_in->sin_addr.s_addr == b_in->sin_addr.s_addr);
    }
    case AF_INET6: {
        struct sockaddr_in6 *a_in6 = (struct sockaddr_in6 *)a;
        struct sockaddr_in6 *b_in6 = (struct sockaddr_in6 *)b;

        if (len < sizeof(struct sockaddr_in6))
            return 0;

        return (a_in6->sin6_port == b_in6->sin6_port &&
                memcmp(&a_in6->sin6_addr, &b_in6->sin6_addr,
                       sizeof(struct in6_addr)) == 0);
    }
    default:
        return memcmp(a, b, len) == 0;
    }
    return 0;
}

dest_t *getdest(const struct sockaddr_storage *x, socklen_t xlen)
{
    /* returns pointer to structure in destarray[] if x is a known destination;
       returns NULL otherwise
       xlen says how many bytes of *x to compare; rest is don't care
    */
    int i;

    for (i = 0; i < hilimit; i++) {
        dest_t *d = &destarray[i];
        if (d->state != FREE && d->destlen == xlen &&
            sockaddr_equal(&d->destaddr, x, xlen))
            return d;
    }
    return NULL;  /* dest not found */
}

dest_t *createdest(const struct sockaddr_storage *x, socklen_t xlen)
{
    /* assumes that x refers to a destination that
       doesn't already exist in destarray[];
       creates a new entry for x and returns pointer to it
       xlen says how many bytes of *x to use in comparisons
    */
    int i;

    for (i = 0; i < hilimit; i++) {
        if (destarray[i].state == FREE)
            break;
    }

    if (i == hilimit) {
        hilimit++; /* advance highwatermark */
        /* Gross hack for now; nicer error handling needed; eventually
           this should be a dynamically allocated structure that can grow */
        assert(hilimit < DESTARRAY_SIZE);
    }

    dest_t *d = &destarray[i];
    cleardest(d);
    d->state = ALLOCATED;
    memcpy(&d->destaddr, x, xlen);
    d->destlen = xlen;

    return d;
}


static void _free_dest_cb(uv_handle_t *handle)
{
    dest_t *d = handle->data;
    free(d->received_packet);
    cleardest(d); /* make slot FREE again */
}

/* Release resources allocated for the specified dest_t */
void free_dest(dest_t *d)
{
    if (d->state == TCPCLOSING)
        return;

    d->state = TCPCLOSING;
    uv_close((uv_handle_t *)d->tcphandle, _free_dest_cb);
}
