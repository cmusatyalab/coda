#include <stdio.h>
#include <time.h>
#include <stdlib.h>

#include <gtest/gtest.h>

#include <test/rvm/rvm.h>

char dir_template[] = "/tmp/cachedir.XXXXXX";

char * CacheDir;

int main(int argc, char **argv) {
    struct rvm_config config;
    int32_t seed = 0;
    int ret = 0;
    printf("Running main() from %s\n", __FILE__);
    testing::InitGoogleTest(&argc, argv);

    CacheDir = mkdtemp(dir_template);
    printf("Changing to CacheDir %s\n", CacheDir);
    if (chdir(CacheDir)) {
        perror("CacheDir chdir");
        exit(EXIT_FAILURE);
    }
    

    INIT_RVM_CONFIG(config)

    RvmType = UFS;

    RVMInit(config);


    /* Get the random seed used by gtest */
    seed = ::testing::GTEST_FLAG(random_seed);

    if (seed == 0) {
        seed = time(NULL) & 0xFFFF;
        ::testing::GTEST_FLAG(random_seed) = seed;
    }

    srand(seed);

    ret = RUN_ALL_TESTS();

    RVMDestroy(config);

    return ret;
}
