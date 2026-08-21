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

/* File-transfer cookie table. Pure data: no libuv, no gnutls, no LWP types,
 * so it can be unit-tested on its own. Every app-side file registration owns
 * one entry, keyed by a 64-bit cookie; the daemon's CT_FILEDONE (demuxed in
 * codatunnel_recvfrom) delivers a terminal status, and the waiting app side
 * reads it before releasing the slot. */

#ifndef _CODATUNNEL_COOKIE_H_
#define _CODATUNNEL_COOKIE_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum number of concurrent registrations the table can track. Chosen to
 * comfortably exceed any realistic number of in-flight transfers on one
 * venus/codasrv, at a cost of a 256-entry scan per look-up. */
#define CT_COOKIE_MAX 256

/* ct_cookie_get() returns this when the cookie is known but the daemon has
 * not yet delivered a terminal status. A plain -1 means the cookie is
 * unknown; 0 means a status is available. */
#define CT_COOKIE_PENDING (-2)

/* Allocate a slot for cookie. cookie 0 is reserved (the "no cookie" sentinel).
 * Returns 0 on success, -1 if cookie is 0, already present, or the table is
 * full. waiter is opaque per-entry state (the daemon's registration record,
 * NULL on the app side); the table stores it but never dereferences it. */
int ct_cookie_add(uint64_t cookie, void *waiter);

/* Record the terminal status for a cookie. Called by the CT_FILEDONE demux.
 * Returns 0 if the cookie is known (status/nbytes stored), -1 if unknown and
 * the delivery is dropped (peer already forgot the registration). */
int ct_cookie_deliver(uint64_t cookie, uint32_t status, uint64_t nbytes);

/* If the cookie is known and a status has been delivered, store *status and
 * *nbytes (when non-NULL) and return 0. Returns CT_COOKIE_PENDING if the
 * cookie is known but not yet finished, and -1 if the cookie is unknown. */
int ct_cookie_get(uint64_t cookie, uint32_t *status, uint64_t *nbytes);

/* Return the opaque per-entry state stored for the cookie, or NULL if
 * unknown. The app side passes NULL (its waiters wake on one shared event);
 * the daemon stores its registration record here. */
void *ct_cookie_waiter(uint64_t cookie);

/* Free the slot. Returns 0 if the cookie existed, -1 otherwise. */
int ct_cookie_remove(uint64_t cookie);

/* Iterate every in-use cookie in table order. fn is called with the cookie,
 * its opaque waiter (the daemon's registration record), and arg. Used to fail
 * pending transfers when their channel is torn down. */
typedef void (*ct_cookie_visitor_t)(uint64_t cookie, void *waiter, void *arg);
void ct_cookie_foreach(ct_cookie_visitor_t fn, void *arg);

#ifdef __cplusplus
}
#endif

#endif /* _CODATUNNEL_COOKIE_H_ */
