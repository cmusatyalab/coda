/* BLURB gpl

                            Coda File System
                               Release 8

           Copyright (c) 2018-2026 Carnegie Mellon University
                   Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                         Additional copyrights
                            none currently
#*/

#include <al/pdbarray.h>
#include "gtest/gtest.h"

namespace
{
// pdb_array.

TEST(pdb_array, init_empty)
{
    pdb_array a;
    pdb_array_init(&a);
    EXPECT_EQ(pdb_array_size(&a), 0);
    EXPECT_EQ(a.data, nullptr);
    pdb_array_free(&a);
}

TEST(pdb_array, add_sorted)
{
    pdb_array a;
    pdb_array_init(&a);

    pdb_array_add(&a, 2);
    pdb_array_add(&a, 4);
    EXPECT_EQ(pdb_array_size(&a), 2);
    EXPECT_EQ(a.data[0], 2);
    EXPECT_EQ(a.data[1], 4);

    pdb_array_add(&a, 5);
    pdb_array_add(&a, 3);
    pdb_array_add(&a, 1);
    EXPECT_EQ(pdb_array_size(&a), 5);
    EXPECT_EQ(a.data[0], 1);
    EXPECT_EQ(a.data[1], 2);
    EXPECT_EQ(a.data[2], 3);
    EXPECT_EQ(a.data[3], 4);
    EXPECT_EQ(a.data[4], 5);

    pdb_array_free(&a);
}

TEST(pdb_array, add_duplicate)
{
    pdb_array a;
    pdb_array_init(&a);

    pdb_array_add(&a, 5);
    pdb_array_add(&a, 5);
    EXPECT_EQ(pdb_array_size(&a), 1);
    EXPECT_EQ(a.data[0], 5);

    pdb_array_free(&a);
}

TEST(pdb_array, del)
{
    pdb_array a;
    pdb_array_init(&a);

    for (int i = 1; i <= 5; i++)
        pdb_array_add(&a, i);
    EXPECT_EQ(pdb_array_size(&a), 5);

    pdb_array_del(&a, 5);
    pdb_array_del(&a, 1);
    pdb_array_del(&a, 3);
    EXPECT_EQ(pdb_array_size(&a), 2);
    EXPECT_EQ(a.data[0], 2);
    EXPECT_EQ(a.data[1], 4);

    pdb_array_free(&a);
}

TEST(pdb_array, del_missing)
{
    pdb_array a;
    pdb_array_init(&a);

    pdb_array_add(&a, 1);
    pdb_array_add(&a, 3);

    pdb_array_del(&a, 2);
    EXPECT_EQ(pdb_array_size(&a), 2);
    EXPECT_EQ(a.data[0], 1);
    EXPECT_EQ(a.data[1], 3);

    pdb_array_free(&a);
}

TEST(pdb_array, search)
{
    pdb_array a;
    pdb_array_init(&a);

    pdb_array_add(&a, 10);
    pdb_array_add(&a, 20);
    pdb_array_add(&a, 30);

    EXPECT_EQ(pdb_array_search(&a, 20), 1);
    EXPECT_EQ(pdb_array_search(&a, 10), 0);
    EXPECT_EQ(pdb_array_search(&a, 30), 2);
    EXPECT_EQ(pdb_array_search(&a, 15), -1);
    EXPECT_EQ(pdb_array_search(&a, 0), -1);
    EXPECT_EQ(pdb_array_search(&a, 100), -1);

    pdb_array_free(&a);
}

TEST(pdb_array, search_empty)
{
    pdb_array a;
    pdb_array_init(&a);
    EXPECT_EQ(pdb_array_search(&a, 42), -1);
    pdb_array_free(&a);
}

TEST(pdb_array, copy)
{
    pdb_array src, dst;
    pdb_array_init(&src);
    pdb_array_init(&dst);

    pdb_array_add(&src, 1);
    pdb_array_add(&src, 2);
    pdb_array_add(&src, 3);

    pdb_array_copy(&dst, &src);

    EXPECT_EQ(pdb_array_size(&dst), 3);
    EXPECT_EQ(dst.data[0], 1);
    EXPECT_EQ(dst.data[1], 2);
    EXPECT_EQ(dst.data[2], 3);

    pdb_array_free(&src);
    pdb_array_free(&dst);
}

TEST(pdb_array, merge)
{
    pdb_array a, b;
    pdb_array_init(&a);
    pdb_array_init(&b);

    pdb_array_add(&a, 2);
    pdb_array_add(&a, 4);

    pdb_array_add(&b, 1);
    pdb_array_add(&b, 3);
    pdb_array_add(&b, 5);

    pdb_array_merge(&a, &b);
    pdb_array_free(&b);

    EXPECT_EQ(pdb_array_size(&a), 5);
    EXPECT_EQ(a.data[0], 1);
    EXPECT_EQ(a.data[1], 2);
    EXPECT_EQ(a.data[2], 3);
    EXPECT_EQ(a.data[3], 4);
    EXPECT_EQ(a.data[4], 5);

    pdb_array_free(&a);
}

TEST(pdb_array, merge_duplicates)
{
    pdb_array a, b;
    pdb_array_init(&a);
    pdb_array_init(&b);

    pdb_array_add(&a, 1);
    pdb_array_add(&a, 3);
    pdb_array_add(&a, 5);

    pdb_array_add(&b, 2);
    pdb_array_add(&b, 3);
    pdb_array_add(&b, 4);

    pdb_array_merge(&a, &b);
    pdb_array_free(&b);

    EXPECT_EQ(pdb_array_size(&a), 5);
    EXPECT_EQ(a.data[0], 1);
    EXPECT_EQ(a.data[1], 2);
    EXPECT_EQ(a.data[2], 3);
    EXPECT_EQ(a.data[3], 4);
    EXPECT_EQ(a.data[4], 5);

    pdb_array_free(&a);
}

TEST(pdb_array, head_next)
{
    pdb_array a;
    pdb_array_off off;
    pdb_array_init(&a);

    pdb_array_add(&a, 5);
    pdb_array_add(&a, 10);
    pdb_array_add(&a, 15);

    EXPECT_EQ(pdb_array_head(&a, &off), 5);
    EXPECT_EQ(pdb_array_next(&a, &off), 10);
    EXPECT_EQ(pdb_array_next(&a, &off), 15);
    EXPECT_EQ(pdb_array_next(&a, &off), 0);

    pdb_array_free(&a);
}

TEST(pdb_array, pack_unpack)
{
    pdb_array src, dst;
    pdb_array_init(&src);
    pdb_array_init(&dst);

    pdb_array_add(&src, 10);
    pdb_array_add(&src, 20);
    pdb_array_add(&src, 30);

    int32_t buf[16];
    int n = pdb_array_pack(buf, &src);
    EXPECT_EQ(n, 4);

    n = pdb_array_unpack(buf, &dst);
    EXPECT_EQ(n, 4);
    EXPECT_EQ(pdb_array_size(&dst), 3);
    EXPECT_EQ(dst.data[0], 10);
    EXPECT_EQ(dst.data[1], 20);
    EXPECT_EQ(dst.data[2], 30);

    pdb_array_free(&src);
    pdb_array_free(&dst);
}

} // namespace
