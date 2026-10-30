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

#ifndef _CODATUNNEL_LOG_H_
#define _CODATUNNEL_LOG_H_

#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>

/* Minimum libuv version, packed the way uv_version() reports it. File
 * registration relies on uv_pipe_pending_type() reporting UV_FILE for
 * SCM_RIGHTS fds, which libuv only does since 1.44.2; on older versions the
 * fds are silently dropped. That behavior is not detectable at link time (an
 * old libuv links cleanly), so both the parent and the forked daemon check
 * uv_version() against this at startup. */
#define CT_MIN_UV_VERSION ((1 << 16) | (44 << 8) | 2)

/* Generic error logger. */
#define ERROR(...)                                         \
    do {                                                   \
        fprintf(stderr, "%s:%d ", __FUNCTION__, __LINE__); \
        fprintf(stderr, __VA_ARGS__);                      \
        fflush(stderr);                                    \
    } while (0)

/* per-packet/per-record trace; off by default (see TLOG below) */
#define DEBUG(...)

/* Timeline log for the daemon-driven file transfer and the per-packet tunnel
 * trace. Gated behind the CODATUNNEL_TLOG env var (default OFF) so a normal
 * build is silent and disk-safe; set CODATUNNEL_TLOG to any value to turn on
 * the full trace for a debugging session (no rebuild needed to toggle). Every
 * line is prefixed with a gettimeofday stamp (sec.usec) so client and server
 * daemon logs can be turned into timelines and correlated. */
static inline int ct_tlog_enabled(void)
{
    static int en = -1;

    if (en < 0)
        en = (getenv("CODATUNNEL_TLOG") != NULL);
    return en;
}

#define TLOG(...)                                                             \
    do {                                                                      \
        if (ct_tlog_enabled()) {                                              \
            struct timeval tt;                                                \
            gettimeofday(&tt, 0);                                             \
            fprintf(stderr, "%ld.%06ld ", (long)tt.tv_sec, (long)tt.tv_usec); \
            fprintf(stderr, __VA_ARGS__);                                     \
            fflush(stderr);                                                   \
        }                                                                     \
    } while (0)

#endif /* _CODATUNNEL_LOG_H_ */
