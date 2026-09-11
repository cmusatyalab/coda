/* BLURB lgpl

                           Coda File System
                              Release 8

          Copyright (c) 2017-2026 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights

#*/

#ifndef _CODATUNNEL_H_
#define _CODATUNNEL_H_

#include <stdint.h>
#include <sys/socket.h>

#ifdef __cplusplus
extern "C" {
#endif

int codatunnel_fork(int argc, char **argv, const char *tcp_bindaddr,
                    const char *udp_bindaddr, const char *bind_service,
                    int onlytcp, const char *sslcertdir);

/* Reports whether a codatunneld tunnel is running in this process (i.e.
 * codatunnel_fork was called). When it returns 0 the caller uses the regular
 * in-band RPC2 SFTP path rather than the tunnel. */
int codatunnel_enabled(void);

#ifdef __cplusplus
}
#endif

#endif /* _CODATUNNEL_H_ */
