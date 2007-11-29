/* BLURB lgpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef HAVE_GETPEEREID

#include "getpeereid.h"

#ifdef HAVE_UCRED_H
#include <ucred.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

int getpeereid(int sock, uid_t *euid, gid_t *egid)
{
    int rc = -1;

#ifdef HAVE_GETPEERUCRED
    ucred_t *cred;

    rc = getpeerucred(sock, &cred);
    if (rc == 0) {
	if (euid) *euid = ucred_geteuid(cred);
	if (egid) *egid = ucred_getegid(cred);
	ucred_free(cred);
    }
#elif defined(SO_PEERCRED)
    struct ucred cred;
    socklen_t len = sizeof(struct ucred);

    rc = getsockopt(sock,  SOL_SOCKET, SO_PEERCRED, &cred, &len);
    if (rc == 0) {
	if (euid) *euid = cred.uid;
	if (egid) *egid = cred.gid;
    }
#else
#error "Need getpeereid(), getpeerucred(), or getsockopt(SO_PEERCRED) support"
#endif
    return rc;
}

#endif

