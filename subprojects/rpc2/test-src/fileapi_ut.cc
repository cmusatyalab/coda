/* BLURB gpl

                            Coda File System
                               Release 8

           Copyright (c) 2026 Carnegie Mellon University
                   Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under the
GNU General Public Licence, as shown in the file LICENSE. The technical and
financial contributors to Coda are listed in the file CREDITS.

                    Additional copyrights:
*/

/* Exercises the enabled/disabled contract of the daemon-driven file API.
 * codatunnel_fork is never called in the test process, so the tunnel is off
 * and every file_* call must report "disabled" without touching the network. */

#include <fcntl.h>
#include <netinet/in.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <gtest/gtest.h>

#include "codatunnel.private.h"

TEST(fileapi, disabled_when_fork_never_called)
{
    EXPECT_EQ(0, codatunnel_enabled());
}

TEST(fileapi, register_is_error_and_zeroes_cookie_when_disabled)
{
    struct sockaddr_in peer;
    memset(&peer, 0, sizeof(peer));
    peer.sin_family      = AF_INET;
    peer.sin_port        = htons(2094);
    peer.sin_addr.s_addr = htonl(0x0100007f);

    uint64_t cookie = 0xdeadbeef;
    int fd          = open("/dev/null", O_RDONLY);
    EXPECT_GE(fd, 0);
    EXPECT_EQ(codatunnel_file_register(
                  reinterpret_cast<const struct sockaddr *>(&peer),
                  sizeof(peer), fd, 0, 0, 0, &cookie),
              -1);
    EXPECT_EQ(cookie, (uint64_t)0);
    close(fd);
}

TEST(fileapi, wait_finalize_unreg_contract_when_disabled)
{
    uint64_t nbytes = 7; /* must survive untouched on the -1 path */
    EXPECT_EQ(-1, codatunnel_file_wait(1234, 0, 0, &nbytes));
    EXPECT_EQ(nbytes, (uint64_t)7);
    /* must not crash even for an unknown / zero cookie */
    codatunnel_file_unreg(1234);
    codatunnel_file_unreg(0);
    SUCCEED();
}
