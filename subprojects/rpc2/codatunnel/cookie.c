/* BLURB lgpl

                            Coda File System
                               Release 8

           Copyright (c) 2026 Carnegie Mellon University
                   Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                         Additional copyrights
#*/

/* App-side file cookie table: a fixed array of slots, linearly scanned. The
 * transfer counts per venus/codasrv are small, so O(n) look-ups are fine and
 * it avoids pulling libuv/gnutls/LWP into the table (kept dependency-free so
 * it can be unit-tested on its own). */

#include <string.h>

#include "cookie.h"

struct ct_cookie_entry {
    uint64_t cookie;
    uint32_t status;
    uint64_t nbytes;
    void *waiter;
    int finished;
    int in_use;
};

static struct ct_cookie_entry cookies[CT_COOKIE_MAX];

static int find_slot(uint64_t cookie)
{
    for (int i = 0; i < CT_COOKIE_MAX; i++)
        if (cookies[i].in_use && cookies[i].cookie == cookie)
            return i;
    return -1;
}

static int find_free_slot(void)
{
    for (int i = 0; i < CT_COOKIE_MAX; i++)
        if (!cookies[i].in_use)
            return i;
    return -1;
}

static void init_slot(int i, uint64_t cookie, void *waiter)
{
    cookies[i].cookie   = cookie;
    cookies[i].status   = 0;
    cookies[i].nbytes   = 0;
    cookies[i].waiter   = waiter;
    cookies[i].finished = 0;
    cookies[i].in_use   = 1;
}

int ct_cookie_add(uint64_t cookie, void *waiter)
{
    int i;
    if (cookie == 0 || find_slot(cookie) >= 0)
        return -1;
    i = find_free_slot();
    if (i < 0)
        return -1;
    init_slot(i, cookie, waiter);
    return 0;
}

int ct_cookie_deliver(uint64_t cookie, uint32_t status, uint64_t nbytes)
{
    int i = find_slot(cookie);
    if (i < 0)
        return -1;
    cookies[i].status   = status;
    cookies[i].nbytes   = nbytes;
    cookies[i].finished = 1;
    return 0;
}

int ct_cookie_get(uint64_t cookie, uint32_t *status, uint64_t *nbytes)
{
    int i = find_slot(cookie);
    if (i < 0)
        return -1;
    if (!cookies[i].finished)
        return CT_COOKIE_PENDING;
    if (status)
        *status = cookies[i].status;
    if (nbytes)
        *nbytes = cookies[i].nbytes;
    return 0;
}

void *ct_cookie_waiter(uint64_t cookie)
{
    int i = find_slot(cookie);
    if (i < 0)
        return NULL;
    return cookies[i].waiter;
}

int ct_cookie_remove(uint64_t cookie)
{
    int i = find_slot(cookie);
    if (i < 0)
        return -1;
    memset(&cookies[i], 0, sizeof cookies[i]);
    return 0;
}

void ct_cookie_foreach(ct_cookie_visitor_t fn, void *arg)
{
    for (int i = 0; i < CT_COOKIE_MAX; i++)
        if (cookies[i].in_use)
            fn(cookies[i].cookie, cookies[i].waiter, arg);
}
