#include <util/bitmap7.h>
#include "gtest/gtest.h"

namespace
{
// bitmap7.
TEST(bitmap7, assign)
{
    int16_t bitmap_size = rand() & 0x7FFF;
    int16_t i           = 0;

    bitmap7 *src = new (0) bitmap7(bitmap_size, 0);
    bitmap7 *dst = new (0) bitmap7(bitmap_size, 0);
    EXPECT_GE(src->Size(), bitmap_size);
    EXPECT_GE(dst->Size(), bitmap_size);

    /* Fill it randomly */
    for (i = 0; i < bitmap_size; i++) {
        if (rand() % 2)
            src->SetIndex(i);
    }

    /* Assign to the destination */
    *dst = *src;

    for (i = 0; i < bitmap_size; i++) {
        EXPECT_EQ(src->Value(i), dst->Value(i));
    }

    if (bitmap_size > 0) {
        EXPECT_FALSE(*dst != *src);
    }

    delete (src);
    delete (dst);
}

TEST(bitmap7, copy)
{
    int16_t bitmap_size = rand() & 0x7FFF;
    int16_t i           = 0;
    int16_t start       = rand() % bitmap_size;
    int16_t len         = rand() % bitmap_size;

    bitmap7 *src = new (0) bitmap7(bitmap_size, 0);
    bitmap7 *dst = new (0) bitmap7(bitmap_size, 0);

    EXPECT_GE(src->Size(), bitmap_size);
    EXPECT_GE(dst->Size(), bitmap_size);

    /* Fill it randomly */
    for (i = 0; i < bitmap_size; i++) {
        if (rand() % 2)
            src->SetIndex(i);
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
}

TEST(bitmap7, assign_different_size)
{
    int16_t bitmap_size_1 = rand() & 0x7FFF;
    int16_t bitmap_size_2 = rand() & 0x7FFF;
    int16_t i             = 0;

    bitmap7 *src = new (0) bitmap7(bitmap_size_1, 0);
    bitmap7 *dst = new (0) bitmap7(bitmap_size_2, 0);

    EXPECT_GE(src->Size(), bitmap_size_1);
    EXPECT_GE(dst->Size(), bitmap_size_2);

    /* Fill it randomly */
    for (i = 0; i < bitmap_size_1; i++) {
        if (rand() % 2)
            src->SetIndex(i);
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
}

TEST(bitmap7, resize)
{
    int16_t bitmap_size_start = rand() & 0x7FFF;
    int16_t bitmap_size_end   = rand() & 0x7FFF;
    int16_t i                 = 0;

    bitmap7 *bm = new (0) bitmap7(bitmap_size_start, 0);

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
}

TEST(bitmap7, set_range)
{
    int16_t bitmap_size = rand() & 0x7FFF;
    int16_t start       = rand() % bitmap_size;
    int16_t len         = rand() % bitmap_size;

    bitmap7 *bm = new (0) bitmap7(bitmap_size, 0);

    EXPECT_GE(bm->Size(), bitmap_size);

    /* Clean then bitmap7 */
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
}

TEST(bitmap7, set_range_and_copy_till_end)
{
    int16_t bitmap_size = rand() & 0x7FFF;
    int16_t start       = rand() % bitmap_size;
    int16_t len         = -1;

    bitmap7 *bm     = new (0) bitmap7(bitmap_size, 0);
    bitmap7 *bm_cpy = new (0) bitmap7(bitmap_size, 0);

    EXPECT_GE(bm->Size(), bitmap_size);
    EXPECT_GE(bm_cpy->Size(), bitmap_size);

    /* Clean the bitmap7 */
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
}

TEST(bitmap7, purge_delete)
{
    int16_t bitmap_size = rand() & 0x7FFF;

    bitmap7 *bm = new (0) bitmap7(bitmap_size, 0);

    EXPECT_GE(bm->Size(), bitmap_size);

    bm->purge();

    delete (bm);
}

TEST(bitmap7, get_free_index)
{
    int16_t bitmap_size   = rand() & 0x7FFF;
    int16_t i             = 0;
    int16_t current_index = 0;

    bitmap7 *bm = new (0) bitmap7(bitmap_size, 0);

    EXPECT_GE(bm->Size(), bitmap_size);
    bm->FreeRange(0, bitmap_size);
    EXPECT_EQ(bm->Count(), 0);

    for (i = 0; i < bitmap_size; i++) {
        current_index = bm->GetFreeIndex();
        EXPECT_EQ(current_index, i);
    }

    delete (bm);
}

static void check_range(bitmap7 *bm, int start, int len, int value)
{
    int i          = 0;
    int actual_val = 0;
    for (i = start; i < start + len; i++) {
        actual_val = bm->Value(i) ? 1 : 0;
        EXPECT_EQ(actual_val, value);
    }
}

TEST(bitmap7, ranges_cases)
{
    int16_t bitmap_size = 333; // Simply need an unaligned size
    bitmap7 *ones       = new (0) bitmap7(bitmap_size, 0);
    bitmap7 *zeros      = new (0) bitmap7(bitmap_size, 0);
    bitmap7 *test_bm    = new (0) bitmap7(bitmap_size, 0);
    int start           = 0;
    int len             = 0;

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
    len   = 8;

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
    len   = 3;

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
    len   = 16;

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
    len   = 10;

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
    len   = 16;

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
    len   = 200;

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
}

} // namespace
