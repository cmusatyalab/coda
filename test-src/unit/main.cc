#include <stdio.h>
#include <time.h>
#include <stdlib.h>

#include <gtest/gtest.h>

int main(int argc, char **argv) {
    int32_t seed = 0;
    printf("Running main() from %s\n", __FILE__);
    testing::InitGoogleTest(&argc, argv);

    /* Get the random seed used by gtest */
    seed = ::testing::GTEST_FLAG(random_seed);

    if (seed == 0) {
        seed = time(nullptr) & 0xFFFF;
        ::testing::GTEST_FLAG(random_seed) = seed;
    }

    srand(seed);

    return RUN_ALL_TESTS();
}