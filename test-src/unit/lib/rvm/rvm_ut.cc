#include "gtest/gtest.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <rvm/rvm.h>
#include <unistd.h> // fork()
#include <sys/wait.h> // WIFEXITED, etc.

#ifdef __cplusplus
}
#endif

#include <testing/memory.h>

namespace
{
class RvmTest : public ::testing::Test {
};

class RvmDeathTest : public ::testing::Test {
public:
    char *tmp_data;
    int tmp_data_size;
    static char *data_dev;

    void map_and_populate_region();
};

char *RvmDeathTest::data_dev = "rvm_test.data";

TEST(RvmTest, alloc_options)
{
    rvm_options_t *init_options = rvm_malloc_options();
    ASSERT_TRUE(init_options);

    rvm_free_options(init_options);

    /* rvm_malloc_options will initialize some structures
     * under the hood */
    ASSERT_EQ(rvm_terminate(), RVM_EINIT);
}

TEST(RvmTest, terminate)
{
    ASSERT_EQ(rvm_terminate(), RVM_EINIT);
}

RVM_TEST_F(RvmDeathTest, init_default)
{
    EXPECT_EQ(rvm_initialize(RVM_VERSION, NULL), RVM_SUCCESS);
    EXPECT_EQ(rvm_terminate(), RVM_SUCCESS);
}

RVM_TEST_F(RvmDeathTest, init_minimal)
{
    const int TRUNCATE_VAL      = 30; /* truncate threshold */
    rvm_options_t *init_options = rvm_malloc_options();
    EXPECT_TRUE(init_options);

    init_options->truncate = TRUNCATE_VAL;
    init_options->flags |= RVM_ALL_OPTIMIZATIONS;
    init_options->flags |= RVM_MAP_PRIVATE;

    EXPECT_EQ(rvm_initialize(RVM_VERSION, init_options), RVM_SUCCESS);

    rvm_free_options(init_options);
    ASSERT_EQ(rvm_terminate(), RVM_SUCCESS);
}

} // namespace
