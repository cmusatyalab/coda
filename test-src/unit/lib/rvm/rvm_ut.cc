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

using RvmDeathTest = RvmTest;

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

RVM_TEST(RvmDeathTest, init_default)
{
    ASSERT_EQ(rvm_initialize(RVM_VERSION, NULL), RVM_SUCCESS);
    ASSERT_EQ(rvm_terminate(), RVM_SUCCESS);
}

RVM_TEST(RvmDeathTest, init_minimal)
{
    const int TRUNCATE_VAL      = 30; /* truncate threshold */
    rvm_options_t *init_options = rvm_malloc_options();
    ASSERT_TRUE(init_options);

    init_options->truncate = TRUNCATE_VAL;
    init_options->flags |= RVM_ALL_OPTIMIZATIONS;
    init_options->flags |= RVM_MAP_PRIVATE;

    ASSERT_EQ(rvm_initialize(RVM_VERSION, init_options), RVM_SUCCESS);

    rvm_free_options(init_options);
    ASSERT_EQ(rvm_terminate(), RVM_SUCCESS);
}

} // namespace
