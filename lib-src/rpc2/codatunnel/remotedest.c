/* BLURB lgpl

                           Coda File System
                              Release 8

          Copyright (c) 2017-2020 Carnegie Mellon University
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
#include <gnutls/gnutls.h>

#include "codatunnel.private.h"

/* Code to track and manage known destinations We use a very simple
   array of fixed length, and brute force search to get started.
   Later, this can be made into a hash table or other more efficient
   data structure
*/

#define DESTARRAY_SIZE 100 /* should be plenty for early debugging */
dest_t destarray[DESTARRAY_SIZE]; /* only 0..hilimit-1 are in use */
static int hilimit = 0; /* one plus highest index in use in destarray */

static void cleardest(dest_t *d)
{
    int i;

    memset(&d->destaddr, 0, sizeof(struct sockaddr_storage));
    d->destlen = 0;
    memset(&d->fqdn, 0, NI_MAXHOST);
    d->state          = FREE;
    d->tcphandle      = NULL;
    d->packets_sent   = 0;
    d->my_tls_session = NULL;
    d->uvcount        = 0;
    d->uvoffset       = 0;
    d->outbound_queue = NULL;
    for (i = 0; i < UVBUFLIMIT; i++) {
        ((d->enqarray[i]).b).base = NULL;
        ((d->enqarray[i]).b).len  = 0;
        (d->enqarray[i]).numbytes = 0;
    }
    d->decrypted_record = NULL;
}

void initdestarray(uv_loop_t *mainloop)
{ /* initialize the global data structure, so that
     the destaddr fields are completely zeroed for memcmp()
     comparisons in later getdest() calls; otherwise padding
     in structures may cause trouble */
    int i;

    hilimit = 0;

    for (i = 0; i < DESTARRAY_SIZE; i++) {
        dest_t *d = &destarray[i];
        cleardest(d);
        uv_mutex_init(&d->uvcount_mutex);
        uv_cond_init(&d->uvcount_nonzero);
        uv_mutex_init(&d->tls_receive_record_mutex);
        uv_mutex_init(&d->tls_send_record_mutex);
        uv_mutex_init(&d->outbound_mutex);
        uv_async_init(mainloop, &d->outbound_worker, outbound_worker_cb);
        d->outbound_worker.data = d;
    }
}

static int sockaddr_equal(const struct sockaddr_storage *a,
                          const struct sockaddr_storage *b, socklen_t len)
{
    if (a->ss_family != b->ss_family)
        return 0;

    switch (a->ss_family) {
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
    return NULL; /* dest not found */
}

dest_t *createdest(const struct sockaddr_storage *x, socklen_t xlen)
{
    /* assumes that x refers to a destination that doesn't
       already exist in destarray[];
       creates a new entry for x and returns pointer to it
       xlen says how many bytes of *x to use in comparisons
       Returns NULL if fqdn of x cannot be obtained
    */
    int i, rc;

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

    /* get fqdn of desired destination, for use in GNUTLS certificate check */
    rc = getnameinfo((struct sockaddr *)x, xlen, d->fqdn, sizeof(d->fqdn), NULL,
                     0, NI_NAMEREQD);
    if (rc) { /* something went wrong */
        fprintf(stderr, "getnameinfo() --> %d (%s)\n", rc, gai_strerror(rc));
        return (NULL);
    }
    return d;
}

static void _free_dest_cb(uv_handle_t *handle)
{
    dest_t *d = handle->data;
    DEBUG("_free_dest_cb(%p)\n", d);
    if (d->decrypted_record)
        free(d->decrypted_record);
    d->decrypted_record = NULL;
    free(d->tcphandle);
    cleardest(d); /* make slot FREE again */
}

/* Release resources allocated for the specified dest_t */
void free_dest(dest_t *d)
{
    int i;

    DEBUG("free_dest(%p)\n", d);
    uv_mutex_lock(&d->uvcount_mutex);
    if (d->state == TCPCLOSING) {
        uv_mutex_unlock(&d->uvcount_mutex);
        return;
    }

    d->state = TCPCLOSING;

    uv_read_stop((uv_stream_t *)d->tcphandle);

    /* drain received buffer queue */
    for (i = 0; i < d->uvcount; i++)
        free(d->enqarray[i].b.base);
    d->uvcount = -1;

    uv_cond_signal(&d->uvcount_nonzero); /* wake blocked sleepers, if any */
    uv_mutex_unlock(&d->uvcount_mutex);

    if (d->tcphandle)
        uv_close((uv_handle_t *)d->tcphandle, _free_dest_cb);
}

/* nb is number of useful bytes in thisbuf->base */
void enq_element(dest_t *d, const uv_buf_t *thisbuf, int nb)
{
    DEBUG("enq_element(%p, %p, %d)\n", d, thisbuf, nb);

    uv_mutex_lock(&d->uvcount_mutex);
    if (d->state != TLSHANDSHAKE && d->state != TCPACTIVE) {
        /* destination is not in a state to pick buffer off of the queue */
        DEBUG("dest state is %s, not queueing buffer\n",
              tcpstatename(d->state));
        free(thisbuf->base);
        uv_mutex_unlock(&d->uvcount_mutex);
        return;
    }

    /* make sure we don't try to queue when we're draining */
    assert(d->uvcount != -1);

    DEBUG("d->uvcount = %d\n", d->uvcount);
    if (d->uvcount >= UVBUFLIMIT) { /* no space; drop packet */
        DEBUG("Dropping packet\n");
        free(thisbuf->base);
    } else { /* append to list of uvbufs */
        DEBUG("Enqing packet\n");
        (d->enqarray)[d->uvcount].b        = *thisbuf;
        (d->enqarray)[d->uvcount].numbytes = nb;
        d->uvcount++;
        if (d->uvcount == 1) { /* zero to one transition */
            DEBUG("Zero to one transition\n");
            d->uvoffset = 0;
            /* wake blocked sleeper, if any */
            uv_cond_signal(&d->uvcount_nonzero);
        }
    }
    uv_mutex_unlock(&d->uvcount_mutex);
}

/* Callback function to check if any buffers are available */
int poll_uvbytes(gnutls_transport_ptr_t gtp, unsigned int ms)
{
    dest_t *d = (dest_t *)gtp;
    int ret;

    uv_mutex_lock(&d->uvcount_mutex);

    while (d->uvcount == 0 && ms) {
        ret = uv_cond_timedwait(&d->uvcount_nonzero, &d->uvcount_mutex,
                                ((uint64_t)ms) * 1000);
    }
    ret = d->uvcount;

    uv_mutex_unlock(&d->uvcount_mutex);

    return ret > 0 ? 1 : ret;
}

/* Upcall function to give gnutls_record_recv() bytes on demand.
   Puts nread bytes into buffer at tlsbuf; return actual bytes filled
   (which may be less than nread; in that case, GNUTLS is expected
   to make multiple calls to obtain correct number of bytes)
   GNUTLS specs require this call to return 0 on connection termination
   and -1 on error of any kind
   Therefore, this has to be a blocking call if no data is available
*/
ssize_t eat_uvbytes(gnutls_transport_ptr_t gtp, void *tlsbuf, size_t nread)
{
    int i, found, stillneeded, here;

    DEBUG("eat_uvbytes(%p, %p, %d)\n", gtp, tlsbuf, (int)nread);

    dest_t *d   = (dest_t *)gtp;
    found       = 0;
    stillneeded = nread; /* (found + stillneeded) always equals nread */

    uv_mutex_lock(&d->uvcount_mutex);

    DEBUG("d->uvcount = %d\n", d->uvcount);
    while (d->uvcount == 0) { /* block until something arrives */
        DEBUG("going to sleep\n");
        uv_cond_wait(&d->uvcount_nonzero, &d->uvcount_mutex);
        DEBUG("just woke up, uvcount = %d\n", d->uvcount);
    }

    /* I hold d->uvcount_mutex, and d->uvcount is nonzero */
    assert(d->uvcount);

    /* negative uvcount indicates this connection is closing/draining */
    while (stillneeded && d->uvcount > 0) {
        /* consume as many buffered packets as possible until sated */

        DEBUG(
            "d->uvcount = %d  d->uvoffset = %d  (d->enqarray[0]).b = %p,%lu (d->enqarray[0]).numbytes = %d  found = %d  stillneeded = %d\n",
            d->uvcount, d->uvoffset, (d->enqarray[0]).b.base,
            (d->enqarray[0]).b.len, (d->enqarray[0]).numbytes, found,
            stillneeded);

        here = (d->enqarray[0]).numbytes - d->uvoffset; /* available in buf */

        char *src = &(((char *)(((d->enqarray[0]).b).base))[d->uvoffset]);
        DEBUG("here = %d  src = %p\n", here, src);

        if (here > stillneeded)
            here = stillneeded;

        memcpy(((char *)tlsbuf) + found, src, here);
        d->uvoffset += here;
        found += here;
        stillneeded -= here;

        DEBUG(
            "d->uvcount = %d  d->uvoffset = %d  (d->enqarray[0]).numbytes = %d  found = %d  stillneeded = %d\n",
            d->uvcount, d->uvoffset, (d->enqarray[0]).numbytes, found,
            stillneeded);

        if (d->uvoffset >= (d->enqarray[0]).numbytes) {
            /* We have completely consumed uvbuf at enqarray[0]; advance to next uvbuf */
            free((d->enqarray[0]).b.base);
            (d->enqarray[0].b).base = NULL;
            (d->enqarray[0].b).len  = 0;
            d->enqarray[0].numbytes = 0;
            d->uvcount--;
            d->uvoffset = 0;

            for (i = 0; i < d->uvcount; i++) { /* shift left */
                (d->enqarray)[i] = (d->enqarray)[i + 1];
            }
        }
    }

    /* we have run out of uvbufs, but may or may not have found nread bytes */
    DEBUG("d->uvcount = %d  d->uvoffset = %d  found = %d  nread = %d\n",
          d->uvcount, d->uvoffset, found, (int)nread);
    uv_mutex_unlock(&d->uvcount_mutex);
    return (found);
}

/* Map current thread id to unique small integer */
int mapthread(uv_thread_t t)
{
#define MAXTHREAD 20
    static int thread_count = 0;
    static uv_thread_t known_threads[MAXTHREAD];
    int i;

    for (i = 0; i < thread_count; i++) {
        if (known_threads[i] == t)
            return (i); /* found it */
    }

    /* Didn't find it */
    if (thread_count >= MAXTHREAD)
        return (-1); /* too many threads */

    /* discovered a new thread; add it to list of knowns */
    known_threads[thread_count] = t;
    thread_count++;
    return (thread_count - 1); /* thread numbers start at 0 */
}

const char *tcpstatename(enum deststate mystate)
{
    switch (mystate) {
    case FREE:
        return ("FREE");
    case ALLOCATED:
        return ("ALLOCATED");
    case TCPATTEMPTING:
        return ("TCPATTEMPTING");
    case TLSHANDSHAKE:
        return ("TLSHANDSHAKE");
    case TCPACTIVE:
        return ("TCPACTIVE");
    case TCPCLOSING:
        return ("TCPCLOSING");
    default:
        return ("UNKNOWN");
    }
}
