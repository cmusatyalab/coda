#include "gtest/gtest.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <rvm/rvm.h>
#include <lwp/lwp.h>
#include <unistd.h> // fork()
#include <sys/wait.h> // WIFEXITED, etc.

#ifdef __cplusplus
}
#endif

#include <testing/memory.h>

namespace
{
static const int populated_value   = 0xa;
static const int region_size       = 4096;
static const char *data_dev        = "rvm_test.data";
static const char *log_dev         = "rvm_test.log";
static rvm_region_t *rvm_reg       = NULL;
static rvm_options_t *init_options = NULL;

static void create_data_device(const char *name, int size)
{
    int trunc_ret = 0;
    FILE *fptr    = fopen(name, "w+");
    trunc_ret     = ftruncate(fileno(fptr), size);
    EXPECT_EQ(trunc_ret, 0);
    fclose(fptr);
}

void populate_segment(int value, int length)
{
    rvm_return_t ret_val = 0;
    rvm_tid_t *tid       = NULL;

    tid = rvm_malloc_tid();
    EXPECT_TRUE(tid);

    ret_val = rvm_begin_transaction(tid, rvm_mode_t::restore);
    EXPECT_EQ(ret_val, RVM_SUCCESS);

    ret_val = rvm_set_range(tid, rvm_reg->vmaddr, length);
    EXPECT_EQ(ret_val, RVM_SUCCESS);

    memset(rvm_reg->vmaddr, value, length);

    ret_val = rvm_end_transaction(tid, rvm_mode_t::flush);
    EXPECT_EQ(ret_val, RVM_SUCCESS);

    rvm_free_tid(tid);
}

void init_options_init()
{
    init_options           = rvm_malloc_options();
    init_options->log_dev  = (char *)log_dev;
    init_options->truncate = 30;
    init_options->flags |= RVM_ALL_OPTIMIZATIONS;
    init_options->flags |= RVM_MAP_PRIVATE;
}

/* create a log file and run with it */
void create_region()
{
    rvm_return_t ret_val = 0;
    rvm_offset_t offset;

    remove(log_dev);
    remove(data_dev);

    init_options_init();

    ASSERT_EQ(rvm_initialize(RVM_VERSION, NULL), RVM_SUCCESS);

    offset  = RVM_MK_OFFSET(0, 4096);
    ret_val = rvm_create_log(init_options, &offset, 0644);
    EXPECT_EQ(ret_val, RVM_SUCCESS);

    ret_val = rvm_set_options(init_options);
    EXPECT_EQ(ret_val, RVM_SUCCESS);

    rvm_reg = rvm_malloc_region();
    EXPECT_TRUE(rvm_reg);

    rvm_reg->length   = region_size;
    rvm_reg->data_dev = (char *)data_dev;
    create_data_device(rvm_reg->data_dev, rvm_reg->length);

    ret_val = rvm_map(rvm_reg, NULL);
    EXPECT_EQ(ret_val, RVM_SUCCESS);
}

/* create a log file and run with it */
void load_region()
{
    rvm_return_t ret_val = 0;

    init_options_init();

    ASSERT_EQ(rvm_initialize(RVM_VERSION, init_options), RVM_SUCCESS);

    rvm_reg = rvm_malloc_region();
    EXPECT_TRUE(rvm_reg);

    rvm_reg->length   = region_size;
    rvm_reg->data_dev = (char *)data_dev;

    ret_val = rvm_map(rvm_reg, NULL);
    EXPECT_EQ(ret_val, RVM_SUCCESS);
}

/* create a log file and run with it */
void destroy_region()
{
    rvm_return_t ret_val = 0;

    ret_val = rvm_unmap(rvm_reg);
    EXPECT_EQ(ret_val, RVM_SUCCESS);

    rvm_free_region(rvm_reg);
    rvm_free_options(init_options);

    EXPECT_EQ(rvm_terminate(), RVM_SUCCESS);
}

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
    EXPECT_EQ(rvm_initialize(RVM_VERSION, NULL), RVM_SUCCESS);
    EXPECT_EQ(rvm_terminate(), RVM_SUCCESS);
}

RVM_TEST(RvmDeathTest, init_minimal)
{
    const int TRUNCATE_VAL      = 30; /* truncate threshold */
    rvm_options_t *init_options = rvm_malloc_options();
    EXPECT_TRUE(init_options);

    init_options->truncate = TRUNCATE_VAL;
    init_options->flags |= RVM_ALL_OPTIMIZATIONS;
    init_options->flags |= RVM_MAP_PRIVATE;

    EXPECT_EQ(rvm_initialize(RVM_VERSION, init_options), RVM_SUCCESS);

    rvm_free_options(init_options);
    EXPECT_EQ(rvm_terminate(), RVM_SUCCESS);
}

RVM_TEST(RvmDeathTest, init_default_alloc)
{
    rvm_region_t *rvm_reg = NULL;
    ASSERT_EQ(rvm_initialize(RVM_VERSION, NULL), RVM_SUCCESS);
    rvm_reg = rvm_malloc_region();
    ASSERT_TRUE(rvm_reg);
    rvm_free_region(rvm_reg);

    ASSERT_EQ(rvm_terminate(), RVM_SUCCESS);
}

RVM_TEST(RvmDeathTest, create_log_dev)
{
    rvm_return_t ret_val = 0;
    rvm_offset_t offset;

    ASSERT_EQ(rvm_initialize(RVM_VERSION, NULL), RVM_SUCCESS);

    remove(log_dev);

    init_options_init();

    offset  = RVM_MK_OFFSET(0, 4096);
    ret_val = rvm_create_log(init_options, &offset, 0644);
    EXPECT_EQ(ret_val, RVM_SUCCESS);

    ret_val = rvm_set_options(init_options);
    EXPECT_EQ(ret_val, RVM_SUCCESS);

    rvm_free_options(init_options);

    EXPECT_EQ(rvm_terminate(), RVM_SUCCESS);
}

RVM_TEST(RvmDeathTest, map_region)
{
    create_region();
    destroy_region();
}

static void create_and_populate_rvm()
{
    create_region();
    populate_segment(populated_value, 128);
    destroy_region();
}

RVM_TEST(RvmDeathTest, write_to_mapped_region)
{
    create_and_populate_rvm();
}

RVM_TEST(RvmDeathTest, abort_transaction)
{
    rvm_return_t ret_val = 0;
    rvm_tid_t *tid       = NULL;
    char *rvm_data       = NULL;

    create_region();
    populate_segment(populated_value, 128);

    rvm_data = rvm_reg->vmaddr;

    tid = rvm_malloc_tid();
    EXPECT_TRUE(tid);

    ret_val = rvm_begin_transaction(tid, rvm_mode_t::restore);
    EXPECT_EQ(ret_val, RVM_SUCCESS);

    EXPECT_EQ(rvm_data[0], populated_value);
    ret_val = rvm_set_range(tid, rvm_reg->vmaddr, 1);
    EXPECT_EQ(ret_val, RVM_SUCCESS);

    rvm_data[0] = 0x5;
    EXPECT_EQ(rvm_data[0], 0x5);

    ret_val = rvm_abort_transaction(tid);
    EXPECT_EQ(ret_val, RVM_SUCCESS);
    EXPECT_EQ(rvm_reg->vmaddr[0], populated_value);

    rvm_free_tid(tid);

    destroy_region();
}

static void recover_and_check_rvm()
{
    char *rvm_data;

    load_region();

    rvm_data = rvm_reg->vmaddr;
    for (int i = 0; i < 128; i++) {
        EXPECT_EQ(rvm_data[i], populated_value);
    }

    destroy_region();
}

TEST(RvmDeathTest, map_and_recover_region)
{
    RVM_LAUNCH_IN_INSTANCE(create_and_populate_rvm);
    RVM_LAUNCH_IN_INSTANCE(recover_and_check_rvm);
}

} // namespace
