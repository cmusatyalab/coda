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

/* Unit tests for the app-side file cookie table. Pure data structure — no
 * libuv, gnutls, or LWP needed — so it links against libcodatunnel via
 * librpc2. */

#include <gtest/gtest.h>

#include "cookie.h"

TEST(cookie, add_then_get_is_pending)
{
    EXPECT_EQ(ct_cookie_add(0xbeef, nullptr), 0);
    uint32_t status;
    uint64_t nbytes;
    EXPECT_EQ(ct_cookie_get(0xbeef, &status, &nbytes), CT_COOKIE_PENDING);
    EXPECT_EQ(ct_cookie_remove(0xbeef), 0);
}

TEST(cookie, deliver_then_get_returns_status)
{
    EXPECT_EQ(ct_cookie_add(0xdeadbeef, nullptr), 0);
    EXPECT_EQ(ct_cookie_deliver(0xdeadbeef, 0, 1234), 0);
    uint32_t status;
    uint64_t nbytes;
    EXPECT_EQ(ct_cookie_get(0xdeadbeef, &status, &nbytes), 0);
    EXPECT_EQ(status, 0U);
    EXPECT_EQ(nbytes, 1234U);
    EXPECT_EQ(ct_cookie_remove(0xdeadbeef), 0);
}

TEST(cookie, deliver_unknown_is_dropped)
{
    EXPECT_EQ(ct_cookie_deliver(0x5678, 0, 0), -1);
}

TEST(cookie, get_unknown_cookie)
{
    uint32_t status;
    uint64_t nbytes;
    EXPECT_EQ(ct_cookie_get(0x1234, &status, &nbytes), -1);
}

TEST(cookie, remove_unknown_cookie)
{
    EXPECT_EQ(ct_cookie_remove(0x9999), -1);
}

TEST(cookie, add_rejects_zero_cookie)
{
    EXPECT_EQ(ct_cookie_add(0, nullptr), -1);
}

TEST(cookie, add_rejects_duplicate)
{
    EXPECT_EQ(ct_cookie_add(0x7777, nullptr), 0);
    EXPECT_EQ(ct_cookie_add(0x7777, nullptr), -1);
    EXPECT_EQ(ct_cookie_remove(0x7777), 0);
}

TEST(cookie, waiter_is_stored_and_returned)
{
    struct tag_dummy {
        int tag;
    };
    struct tag_dummy d;
    d.tag = 42;
    EXPECT_EQ(ct_cookie_add(0x42, &d), 0);
    EXPECT_EQ(ct_cookie_waiter(0x42), (void *)&d);
    EXPECT_EQ(ct_cookie_waiter(0x43), nullptr);
    EXPECT_EQ(ct_cookie_remove(0x42), 0);
}

static int visited[16];
static int visit_count;
static void visit(uint64_t cookie, void *waiter, void *arg)
{
    (void)arg;
    visited[visit_count++] = (int)cookie;
}

/* foreach visits every in-use entry in table order, passing the waiter. */
TEST(cookie, foreach_visits_all_in_use_entries)
{
    visit_count = 0;
    ct_cookie_add(11, (void *)0x1);
    ct_cookie_add(22, (void *)0x2);
    ct_cookie_foreach(visit, NULL);
    EXPECT_EQ(visit_count, 2);
    EXPECT_EQ(visited[0], 11);
    EXPECT_EQ(visited[1], 22);
    ct_cookie_remove(11);
    ct_cookie_remove(22);
}

/* foreach skips free slots and never crashes on an empty table. */
TEST(cookie, foreach_empty_table)
{
    visit_count = 0;
    ct_cookie_foreach(visit, NULL);
    EXPECT_EQ(visit_count, 0);
}

TEST(cookie, table_is_bounded_and_recovers)
{
    for (uint64_t i = 0; i < CT_COOKIE_MAX; i++)
        EXPECT_EQ(ct_cookie_add(i + 1, nullptr), 0);
    EXPECT_EQ(ct_cookie_add(CT_COOKIE_MAX + 1, nullptr), -1);
    EXPECT_EQ(ct_cookie_remove(1), 0);
    EXPECT_EQ(ct_cookie_add(CT_COOKIE_MAX + 1, nullptr), 0);
    EXPECT_EQ(ct_cookie_remove(CT_COOKIE_MAX + 1), 0);
    for (uint64_t i = 1; i < CT_COOKIE_MAX; i++)
        EXPECT_EQ(ct_cookie_remove(i + 1), 0);
}
