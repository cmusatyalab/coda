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

#include <util/bitmap.h>
#include "gtest/gtest.h"

namespace
{
// bitmap (legacy).

TEST(bitmap, constructor)
{
    bitmap *b = new (0) bitmap(32);
    EXPECT_EQ(b->Size(), 32);
    EXPECT_EQ(b->Count(), 0);
    delete (b);
}

TEST(bitmap, set_index)
{
    bitmap *b = new (0) bitmap(64);
    EXPECT_EQ(b->Count(), 0);

    b->SetIndex(0);
    EXPECT_NE(b->Value(0), 0);
    EXPECT_EQ(b->Count(), 1);

    b->SetIndex(32);
    EXPECT_NE(b->Value(32), 0);
    EXPECT_EQ(b->Count(), 2);

    delete (b);
}

TEST(bitmap, free_index)
{
    bitmap *b = new (0) bitmap(64);

    b->SetIndex(5);
    EXPECT_EQ(b->Count(), 1);

    b->FreeIndex(5);
    EXPECT_EQ(b->Value(5), 0);
    EXPECT_EQ(b->Count(), 0);

    delete (b);
}

TEST(bitmap, set_range)
{
    bitmap *b = new (0) bitmap(64);
    EXPECT_EQ(b->Count(), 0);

    b->SetRange(0, 8);
    EXPECT_EQ(b->Count(), 8);

    b->SetRange(16, 4);
    EXPECT_EQ(b->Count(), 12);

    delete (b);
}

TEST(bitmap, free_range)
{
    bitmap *b = new (0) bitmap(64);

    b->SetRange(0, 8);
    b->SetRange(16, 4);
    EXPECT_EQ(b->Count(), 12);

    b->FreeRange(0, 4);
    EXPECT_EQ(b->Count(), 8);

    b->FreeRange(16, -1);
    EXPECT_EQ(b->Count(), 4);

    delete (b);
}

TEST(bitmap, get_free_index)
{
    bitmap *b = new (0) bitmap(16);
    EXPECT_EQ(b->Count(), 0);

    for (int i = 0; i < 16; i++) {
        EXPECT_EQ(b->GetFreeIndex(), i);
    }

    EXPECT_EQ(b->Count(), 16);
    EXPECT_EQ(b->GetFreeIndex(), -1);

    delete (b);
}

TEST(bitmap, grow)
{
    bitmap *b = new (0) bitmap(32);
    EXPECT_EQ(b->Size(), 32);

    for (int i = 0; i < 32; i++)
        b->SetIndex(i);
    EXPECT_EQ(b->Count(), 32);

    b->Grow(48);
    EXPECT_EQ(b->Size(), 48);
    EXPECT_EQ(b->Count(), 32);

    int idx = b->GetFreeIndex();
    EXPECT_GE(idx, 32);

    delete (b);
}

TEST(bitmap, resize_shrink)
{
    bitmap *b = new (0) bitmap(64);

    for (int i = 0; i < 32; i++)
        b->SetIndex(i);
    EXPECT_EQ(b->Count(), 32);

    b->Resize(16);
    EXPECT_EQ(b->Size(), 16);
    EXPECT_EQ(b->Count(), 16);

    delete (b);
}

TEST(bitmap, copy_assign)
{
    bitmap *src = new (0) bitmap(64);
    bitmap *dst = new (0) bitmap(64);

    src->SetRange(0, 32);

    *dst = *src;

    EXPECT_EQ(dst->Count(), 32);
    EXPECT_FALSE(*dst != *src);

    delete (src);
    delete (dst);
}

TEST(bitmap, copy_range)
{
    bitmap *src = new (0) bitmap(64);
    bitmap *dst = new (0) bitmap(64);

    src->SetRange(8, 16);

    src->CopyRange(8, 16, *dst);

    EXPECT_EQ(dst->Count(), 16);

    delete (src);
    delete (dst);
}

TEST(bitmap, purge)
{
    bitmap *b = new (0) bitmap(32);
    b->SetIndex(5);
    EXPECT_EQ(b->Count(), 1);

    b->purge();
    EXPECT_EQ(b->Size(), 0);
    EXPECT_EQ(b->Count(), 0);

    delete (b);
}

TEST(bitmap, random_stress)
{
    int size   = rand() % 256 + 1;
    bitmap *bm = new (0) bitmap(size);

    for (int i = 0; i < 100; i++) {
        int idx = rand() % size;
        if (rand() % 2)
            bm->SetIndex(idx);
        else
            bm->FreeIndex(idx);
        EXPECT_GE(bm->Count(), 0);
        EXPECT_LE(bm->Count(), size);
    }

    delete (bm);
}

} // namespace
