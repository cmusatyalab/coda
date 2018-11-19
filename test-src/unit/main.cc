#include <stdio.h>
#include <time.h>
#include <stdlib.h>

#include <gtest/gtest.h>

#include <test/rvm/rvm.h>

int main(int argc, char **argv) {
    struct rvm_config config;
    int32_t seed = 0;
    int ret = 0;
    printf("Running main() from %s\n", __FILE__);
    testing::InitGoogleTest(&argc, argv);


    INIT_RVM_CONFIG(config)

    RvmType = UFS;

    RVMInit(config);


    /* Get the random seed used by gtest */
    seed = ::testing::GTEST_FLAG(random_seed);

    if (seed == 0) {
        seed = time(nullptr) & 0xFFFF;
        ::testing::GTEST_FLAG(random_seed) = seed;
    }

    srand(seed);

    ret = RUN_ALL_TESTS();

    RVMDestroy(config);

    return ret;
}
