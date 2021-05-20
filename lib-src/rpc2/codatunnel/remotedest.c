/* BLURB lgpl

                           Coda File System
                              Release 8

          Copyright (c) 2017-2021 Carnegie Mellon University
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
    d->destlen               = 0;
    d->fqdn                  = NULL;
    d->state                 = FREE;
    d->certvalidation_failed = 0;
    d->tcphandle             = NULL;
    d->my_tls_session        = NULL;
    d->uvcount               = 0;
    d->uvoffset              = 0;
    d->tls_send_queue        = NULL;
    d->outbound_queue        = NULL;
    for (i = 0; i < UVBUFLIMIT; i++) {
        d->enqarray[i].b.base   = NULL;
        d->enqarray[i].b.len    = 0;
        d->enqarray[i].numbytes = 0;
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
        uv_mutex_init(&d->tls_send_mutex);
        uv_mutex_init(&d->outbound_mutex);
        uv_async_init(mainloop, &d->wakeup, outbound_worker_cb);
        d->wakeup.data = d;
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

dest_t *createdest(const struct sockaddr_storage *x, socklen_t xlen,
                   const char *peername)
{
    /* assumes that x refers to a destination that doesn't
       already exist in destarray[];
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
    d->fqdn    = peername;

    return d;
}

static void _free_dest_cb(uv_handle_t *handle)
{
    dest_t *d = handle->data;
    DEBUG("_free_dest_cb(%p)\n", d);

    // XXX deadlocks when gnutls is waiting for more data but the current
    // dest_t should not be involved in an active handshake anyway...
    /* barrier wait to make sure gnutls_handshake has finished */
    //wait_for_handshakes();

    if (d->decrypted_record)
        free(d->decrypted_record);
    d->decrypted_record = NULL;
    free(d->tcphandle);
    free((void *)d->fqdn);
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

    if (d->tcphandle)
        uv_read_stop((uv_stream_t *)d->tcphandle);

    /* drain received buffer queue */
    for (i = 0; i < d->uvcount; i++)
        free(d->enqarray[i].b.base);
    d->uvcount = -1;

    uv_cond_signal(&d->uvcount_nonzero); /* wake blocked poll, if any */
    uv_mutex_unlock(&d->uvcount_mutex);

    drain_outbound_queues(d);

    if (d->tcphandle) {
        uv_close((uv_handle_t *)d->tcphandle, _free_dest_cb);
    } else {
        uv_handle_t handle = { .data = d };
        _free_dest_cb(&handle);
    }
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
        d->enqarray[d->uvcount].b        = *thisbuf;
        d->enqarray[d->uvcount].numbytes = nb;
        d->uvcount++;
        uv_cond_signal(&d->uvcount_nonzero);
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

    return ret != 0 ? 1 : 0;
}

/* Upcall function to give gnutls_record_recv() bytes on demand.
   Puts nread bytes into buffer at tlsbuf; return actual bytes filled
   (which may be less than nread; in that case, GNUTLS is expected
   to make multiple calls to obtain correct number of bytes)
*/
ssize_t eat_uvbytes(gnutls_transport_ptr_t gtp, void *tlsbuf, size_t nread)
{
    DEBUG("eat_uvbytes(%p, %p, %lu)\n", gtp, tlsbuf, nread);

    dest_t *d   = (dest_t *)gtp;
    ssize_t len = -1;

    uv_mutex_lock(&d->uvcount_mutex);

    /* queue empty? return EAGAIN */
    if (d->uvcount == 0) {
        DEBUG("d->uvcount = 0, returning EAGAIN\n");
        gnutls_transport_set_errno(d->my_tls_session, EAGAIN);
        errno = EAGAIN;
        goto unlock_out;
    }

    /* I hold d->uvcount_mutex, and d->uvcount is nonzero */
    assert(d->uvcount);

    /* negative uvcount indicates this connection is closing/draining */
    if (d->uvcount < 0 || d->enqarray[0].numbytes == UV_EOF) {
        DEBUG("hit EOF, returning 0\n");
        len = 0;
        goto unlock_out;
    }

    char *src = (char *)d->enqarray[0].b.base + d->uvoffset;
    len       = d->enqarray[0].numbytes - d->uvoffset; /* available in buf */
    assert(len >= 0);

    if (len > nread)
        len = nread;

    memcpy(tlsbuf, src, len);
    d->uvoffset += len;

    if (d->uvoffset >= d->enqarray[0].numbytes) {
        /* We have completely consumed uvbuf at enqarray[0]; advance to next uvbuf */
        free(d->enqarray[0].b.base);

        d->uvcount--;
        d->uvoffset = 0;

        int i;
        for (i = 0; i < d->uvcount; i++) { /* shift left */
            d->enqarray[i] = d->enqarray[i + 1];
        }

        d->enqarray[d->uvcount].b.base   = NULL;
        d->enqarray[d->uvcount].b.len    = 0;
        d->enqarray[d->uvcount].numbytes = 0;
    }
unlock_out:
    uv_mutex_unlock(&d->uvcount_mutex);
    return len;
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
    case TLSERROR:
        return ("TLSERROR");
    case TCPCLOSING:
        return ("TCPCLOSING");
    default:
        return ("UNKNOWN");
    }
}
