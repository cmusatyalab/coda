/* BLURB gpl

                           Coda File System
                              Release 7

          Copyright (c) 1987-2019 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <assert.h>

#ifdef __cplusplus
}
#endif

#include "venusmux.h"

/* interfaces */

/* from venus */

/* socket connecting us back to our parent */
int parent_fd = -1;

static struct mux_cb_entry *_MUX_CBEs;

/* Add file descriptors that have a callback to the fd_set */
int _MUX_FD_SET(fd_set *fds)
{
    struct mux_cb_entry *cbe = _MUX_CBEs;
    int maxfd                = -1;

    for (; cbe; cbe = cbe->next) {
        FD_SET(cbe->fd, fds);
        if (cbe->fd > maxfd)
            maxfd = cbe->fd;
    }
    return maxfd;
}

/* Dispatch callbacks for file descriptors in the fd_set */
void _MUX_Dispatch(fd_set *fds)
{
    struct mux_cb_entry *cbe = _MUX_CBEs, *next;
    while (cbe) {
        /* allow callback to remove itself without messing with the iterator */
        next = cbe->next;

        if (FD_ISSET(cbe->fd, fds))
            cbe->cb(cbe->fd, cbe->udata);

        cbe = next;
    }
}

/* Helper to add a file descriptor with callback to main select loop.
 *
 * Call with cb == NULL to remove existing callback.
 * cb is called with fd == -1 when an existing callback is removed or updated.
 */
void MUX_add_callback(int fd, void (*cb)(int fd, void *udata), void *udata)
{
    struct mux_cb_entry *cbe = _MUX_CBEs, *prev = NULL;

    for (; cbe; cbe = cbe->next) {
        if (cbe->fd == fd) {
            /* remove old callback entry */
            if (prev)
                prev->next = cbe->next;
            else
                _MUX_CBEs = cbe->next;

            /* allow cb to free udata resources */
            cbe->cb(-1, cbe->udata);

            free(cbe);
            break;
        }
        prev = cbe;
    }
    /* if we are not adding a new callback, we're done */
    if (cb == NULL)
        return;

    cbe = (struct mux_cb_entry *)malloc(sizeof(*cbe));
    assert(cbe != NULL);

    cbe->fd    = fd;
    cbe->cb    = cb;
    cbe->udata = udata;

    cbe->next = _MUX_CBEs;
    _MUX_CBEs = cbe;
}
