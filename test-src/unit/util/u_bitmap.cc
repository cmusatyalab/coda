/* BLURB gpl

                           Coda File System
                              Release 7

          Copyright (c) 1987-2018 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/
/* system */

/* external */
#include <gtest/gtest.h>

/* from coda-src */
#include <util/bitmap.h>

/* from test-src */
#include <test/rvm/rvm.h>
#include <test/rvm/recov.h>

namespace {

// bitmap.
TEST(bitmap, assign) {
    int16_t bitmap_size = rand() & 0x7FFF;
    int recoverable_1 = rand() & 0x1;
    int recoverable_2 = rand() & 0x1;
    int16_t i = 0;

    Recov_BeginTrans();

    bitmap * src = new (recoverable_1) bitmap(bitmap_size, recoverable_1);
    bitmap * dst = new (recoverable_2) bitmap(bitmap_size, recoverable_2);

    EXPECT_GE(src->Size(), bitmap_size);
    EXPECT_GE(dst->Size(), bitmap_size);

    /* Fill it randomly */
    for (i = 0; i < bitmap_size; i++) {
        if (rand() % 2) src->SetIndex(i);
    }

    /* Assign to the destination */
    *dst = *src;

    for (i = 0; i < bitmap_size; i++) {
        EXPECT_EQ(src->Value(i), dst->Value(i));
    }

    EXPECT_FALSE(*dst != *src);

    delete (src);
    delete (dst);

    Recov_EndTrans(0);
}

TEST(bitmap, copy) {
    int16_t bitmap_size = rand() & 0x7FFF;
    int16_t i = 0;
    int16_t start = rand() % bitmap_size;
    int16_t len = rand() % bitmap_size;
    int recoverable_1 = rand() & 0x1;
    int recoverable_2 = rand() & 0x1;

    Recov_BeginTrans();

    bitmap * src = new (recoverable_1) bitmap(bitmap_size, recoverable_1);
    bitmap * dst = new (recoverable_2) bitmap(bitmap_size, recoverable_2);

    EXPECT_GE(src->Size(), bitmap_size);
    EXPECT_GE(dst->Size(), bitmap_size);

    /* Fill it randomly */
    for (i = 0; i < bitmap_size; i++) {
        if (rand() % 2) src->SetIndex(i);
    }

    /* Order the start and end */
    if (len > bitmap_size - start) {
        len = -1;
    }

    /* Assign to the destination */
    src->CopyRange(start, len, *dst);

    for (i = start; i < start + len; i++) {
        EXPECT_EQ(src->Value(i), dst->Value(i));
    }

    delete (src);
    delete (dst);

    Recov_EndTrans(0);

}

TEST(bitmap, assign_different_size) {
    int16_t bitmap_size_1 = rand() & 0x7FFF;
    int16_t bitmap_size_2 = rand() & 0x7FFF;
    int16_t i = 0;
    int recoverable_1 = rand() & 0x1;
    int recoverable_2 = rand() & 0x1;

    Recov_BeginTrans();

    bitmap * src = new (recoverable_1) bitmap(bitmap_size_1, recoverable_1);
    bitmap * dst = new (recoverable_2) bitmap(bitmap_size_2, recoverable_2);

    EXPECT_GE(src->Size(), bitmap_size_1);
    EXPECT_GE(dst->Size(), bitmap_size_2);

    /* Fill it randomly */
    for (i = 0; i < bitmap_size_1; i++) {
        if (rand() % 2) src->SetIndex(i);
    }

    /* Assign to the destination */
    *dst = *src;

    for (i = 0; i < bitmap_size_1; i++) {
        EXPECT_EQ(src->Value(i), dst->Value(i));
    }

    EXPECT_EQ(src->Count(), dst->Count());
    EXPECT_EQ(src->Size(), dst->Size());
    EXPECT_GE(dst->Size(), bitmap_size_1);

    delete (src);
    delete (dst);

    Recov_EndTrans(0);

}

TEST(bitmap, resize) {
    int16_t bitmap_size_start = rand() & 0x7FFF;
    int16_t bitmap_size_end = rand() & 0x7FFF;
    int16_t i = 0;
    int recoverable = rand() & 0x1;

    Recov_BeginTrans();

    bitmap * bm = new (recoverable) bitmap(bitmap_size_start, recoverable);

    EXPECT_GE(bm->Size(), bitmap_size_start);

    /* Fill it with ones */
    for (i = 0; i < bitmap_size_start; i++) {
        bm->SetIndex(i);
    }

    EXPECT_EQ(bm->Count(), bitmap_size_start);

    bm->Resize(bitmap_size_end);

    if (bitmap_size_end < bitmap_size_start) {
        EXPECT_EQ(bm->Count(), bitmap_size_end);
    } else {
        EXPECT_EQ(bm->Count(), bitmap_size_start);
    }

    delete (bm);

    Recov_EndTrans(0);

}

TEST(bitmap, set_range) {
    int16_t bitmap_size = rand() & 0x7FFF;
    int16_t start = rand() % bitmap_size;
    int16_t len = rand() % bitmap_size;
    int recoverable = rand() & 0x1;

    Recov_BeginTrans();

    bitmap * bm = new (recoverable) bitmap(bitmap_size, recoverable);

    EXPECT_GE(bm->Size(), bitmap_size);

    /* Clean then bitmap */
    bm->FreeRange(0, bitmap_size);
    EXPECT_EQ(bm->Count(), 0);

    /* Order the start and end */
    if (len > bitmap_size - start) {
        len = -1;
    }

    /* Set the range */
    bm->SetRange(start, len);

    /* Verify the set bits */
    if (len >= 0) {
        EXPECT_EQ(bm->Count(), len);
    } else {
        EXPECT_EQ(bm->Count(), bitmap_size - start);
    }

    delete (bm);

    Recov_EndTrans(0);
}

TEST(bitmap, set_range_and_copy_till_end) {
    int16_t bitmap_size = rand() & 0x7FFF;
    int16_t start = rand() % bitmap_size;
    int16_t len = -1;
    int recoverable_1 = rand() & 0x1;
    int recoverable_2 = rand() & 0x1;

    Recov_BeginTrans();

    bitmap * bm = new (recoverable_1) bitmap(bitmap_size, recoverable_1);
    bitmap * bm_cpy = new (recoverable_2) bitmap(bitmap_size, recoverable_2);

    EXPECT_GE(bm->Size(), bitmap_size);
    EXPECT_GE(bm_cpy->Size(), bitmap_size);

    /* Clean the bitmap */
    bm->FreeRange(0, bitmap_size);
    bm_cpy->FreeRange(0, bitmap_size);
    EXPECT_EQ(bm->Count(), 0);
    EXPECT_EQ(bm_cpy->Count(), 0);

    /* Set the range */
    bm->SetRange(start, len);
    bm->CopyRange(start, len, *bm_cpy);

    /* Verify the set bits */
    EXPECT_EQ(bm->Count(), bitmap_size - start);
    EXPECT_EQ(bm_cpy->Count(), bitmap_size - start);

    delete (bm);
    delete (bm_cpy);

    Recov_EndTrans(0);

}

TEST(bitmap, purge_delete) {
    int16_t bitmap_size = rand() & 0x7FFF;
    int recoverable = rand() & 0x1;

    Recov_BeginTrans();

    bitmap * bm = new (recoverable) bitmap(bitmap_size, recoverable);

    EXPECT_GE(bm->Size(), bitmap_size);

    bm->purge();

    delete (bm);

    Recov_EndTrans(0);
}

TEST(bitmap, get_free_index) {
    int16_t bitmap_size = rand() & 0x7FFF;
    int16_t i = 0;
    int16_t current_index = 0;
    int recoverable = rand() & 0x1;

    Recov_BeginTrans();

    bitmap * bm = new (recoverable) bitmap(bitmap_size, recoverable);
    
    EXPECT_GE(bm->Size(), bitmap_size);
    bm->FreeRange(0, bitmap_size);
    EXPECT_EQ(bm->Count(), 0);

    for (i = 0; i < bitmap_size; i++) {
        current_index = bm->GetFreeIndex();
        EXPECT_EQ(current_index, i);
    }

    delete (bm);

    Recov_EndTrans(0);
}

static void check_range(bitmap* bm, int start, int len, int value) {
    int i = 0;
    int actual_val = 0;
    for (i = start; i < start + len; i++) {
        actual_val = bm->Value(i) ? 1 :0;
        EXPECT_EQ(actual_val, value);
    }
}

TEST(bitmap, ranges_cases) {
    int16_t bitmap_size = 333; // Simply need an unaligned size
    int recoverable_1 = rand() & 0x1;
    int recoverable_2 = rand() & 0x1;
    int recoverable_3 = rand() & 0x1;

    Recov_BeginTrans();

    bitmap * ones = new (recoverable_1) bitmap(bitmap_size, recoverable_1);
    bitmap * zeros = new (recoverable_2) bitmap(bitmap_size, recoverable_2);
    bitmap * test_bm = new (recoverable_3) bitmap(bitmap_size, recoverable_3);
    int start = 0;
    int len = 0;

    EXPECT_GE(ones->Size(), bitmap_size);
    EXPECT_GE(zeros->Size(), bitmap_size);
    EXPECT_GE(test_bm->Size(), bitmap_size);

    zeros->FreeRange(0, bitmap_size);
    ones->SetRange(0, bitmap_size);

    EXPECT_EQ(zeros->Count(), 0);
    EXPECT_EQ(ones->Count(), bitmap_size);

    zeros->FreeRange(0, -1);
    ones->SetRange(0, -1);

    EXPECT_EQ(zeros->Count(), 0);
    EXPECT_EQ(ones->Count(), bitmap_size);

    /* Same byte range and aligned */
    start = 8;
    len = 8;

    // Clean
    *test_bm = *zeros;
    // Copy
    ones->CopyRange(start, len, *test_bm);
    EXPECT_EQ(test_bm->Count(), len);
    check_range(test_bm, start, len, 0x1);
    // Free
    test_bm->FreeRange(start, len);
    EXPECT_EQ(test_bm->Count(), 0);
    check_range(test_bm, start, len, 0x0);
    // Set
    test_bm->SetRange(start, len);
    EXPECT_EQ(test_bm->Count(), len);
    check_range(test_bm, start, len, 0x1);

    /* Same byte range and unaligned */
    start = 10;
    len = 3;

    // Clean
    *test_bm = *zeros;
    // Copy
    ones->CopyRange(start, len, *test_bm);
    EXPECT_EQ(test_bm->Count(), len);
    check_range(test_bm, start, len, 0x1);
    // Free
    test_bm->FreeRange(start, len);
    EXPECT_EQ(test_bm->Count(), 0);
    check_range(test_bm, start, len, 0x0);
    // Set
    test_bm->SetRange(start, len);
    EXPECT_EQ(test_bm->Count(), len);
    check_range(test_bm, start, len, 0x1);

    /* Consecutive bytes range and aligned */
    start = 16;
    len = 16;

    // Clean
    *test_bm = *zeros;
    // Copy
    ones->CopyRange(start, len, *test_bm);
    EXPECT_EQ(test_bm->Count(), len);
    check_range(test_bm, start, len, 0x1);
    // Free
    test_bm->FreeRange(start, len);
    EXPECT_EQ(test_bm->Count(), 0);
    check_range(test_bm, start, len, 0x0);
    // Set
    test_bm->SetRange(start, len);
    EXPECT_EQ(test_bm->Count(), len);
    check_range(test_bm, start, len, 0x1);

    /* Consecutive bytes range and unaligned */
    start = 10;
    len = 10;

    // Clean
    *test_bm = *zeros;
    // Copy
    ones->CopyRange(start, len, *test_bm);
    EXPECT_EQ(test_bm->Count(), len);
    check_range(test_bm, start, len, 0x1);
    // Free
    test_bm->FreeRange(start, len);
    EXPECT_EQ(test_bm->Count(), 0);
    check_range(test_bm, start, len, 0x0);
    // Set
    test_bm->SetRange(start, len);
    EXPECT_EQ(test_bm->Count(), len);
    check_range(test_bm, start, len, 0x1);

    /* 3 Consecutive bytes range and unaligned */
    start = 10;
    len = 16;

    // Clean
    *test_bm = *zeros;
    // Copy
    ones->CopyRange(start, len, *test_bm);
    EXPECT_EQ(test_bm->Count(), len);
    check_range(test_bm, start, len, 0x1);
    // Free
    test_bm->FreeRange(start, len);
    EXPECT_EQ(test_bm->Count(), 0);
    check_range(test_bm, start, len, 0x0);
    // Set
    test_bm->SetRange(start, len);
    EXPECT_EQ(test_bm->Count(), len);
    check_range(test_bm, start, len, 0x1);


    /* Big unaligned bulk */
    start = 10;
    len = 200;

    // Clean
    *test_bm = *zeros;
    // Copy
    ones->CopyRange(start, len, *test_bm);
    EXPECT_EQ(test_bm->Count(), len);
    check_range(test_bm, start, len, 0x1);
    // Free
    test_bm->FreeRange(start, len);
    EXPECT_EQ(test_bm->Count(), 0);
    check_range(test_bm, start, len, 0x0);
    // Set
    test_bm->SetRange(start, len);
    EXPECT_EQ(test_bm->Count(), len);
    check_range(test_bm, start, len, 0x1);

    delete (zeros);
    delete (ones);
    delete (test_bm);

    Recov_EndTrans(0);
}

}  // namespace
