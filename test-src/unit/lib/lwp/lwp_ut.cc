#include "gtest/gtest.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <lwp/lwp.h>
#include <unistd.h>
#include <sys/wait.h>

#ifdef __cplusplus
}
#endif

#include <testing/memory.h>

namespace
{
class LwpTest : public ::testing::Test {
};

class LwpDeathTest : public ::testing::Test {
public:
    static char sync;
};

char LwpDeathTest::sync = 0;

RVM_TEST_F(LwpDeathTest, init_wrong_priority)
{
    PROCESS main;
    int ret_val = 0;

    ret_val = LWP_Init(LWP_VERSION, LWP_MAX_PRIORITY + 1, &main);
    EXPECT_EQ(ret_val, LWP_EBADPRI);

    ret_val = LWP_TerminateProcessSupport();
    EXPECT_EQ(ret_val, LWP_EINIT);
}

RVM_TEST_F(LwpDeathTest, init)
{
    PROCESS main;
    int ret_val = 0;

    ret_val = LWP_Init(LWP_VERSION, LWP_MAX_PRIORITY, &main);
    EXPECT_EQ(ret_val, LWP_SUCCESS);

    ret_val = LWP_TerminateProcessSupport();
    EXPECT_EQ(ret_val, LWP_SUCCESS);
}

static void dummy_main(void *p)
{
    LWP_SignalProcess(&LwpDeathTest::sync);
}

RVM_TEST_F(LwpDeathTest, create_process)
{
    PROCESS main;
    PROCESS sec_proc;
    int ret_val = 0;

    ret_val = LWP_Init(LWP_VERSION, LWP_MAX_PRIORITY, &main);
    EXPECT_EQ(ret_val, LWP_SUCCESS);

    ret_val = LWP_CreateProcess(dummy_main, 1, 1, NULL, "dummy", &sec_proc);
    EXPECT_EQ(ret_val, LWP_SUCCESS);

    ret_val = LWP_WaitProcess(&LwpDeathTest::sync);
    EXPECT_EQ(ret_val, LWP_SUCCESS);

    ret_val = LWP_DestroyProcess(sec_proc);
    EXPECT_EQ(ret_val, LWP_SUCCESS);

    ret_val = LWP_TerminateProcessSupport();
    EXPECT_EQ(ret_val, LWP_SUCCESS);
}

RVM_TEST_F(LwpDeathTest, create_process_null_main)
{
    PROCESS sec_proc;
    int ret_val = 0;

    ret_val = LWP_Init(LWP_VERSION, LWP_MAX_PRIORITY, NULL);
    EXPECT_EQ(ret_val, LWP_SUCCESS);

    ret_val = LWP_CreateProcess(dummy_main, 1, 1, NULL, "dummy", &sec_proc);
    EXPECT_EQ(ret_val, LWP_SUCCESS);

    ret_val = LWP_WaitProcess(&LwpDeathTest::sync);
    EXPECT_EQ(ret_val, LWP_SUCCESS);

    ret_val = LWP_DestroyProcess(sec_proc);
    EXPECT_EQ(ret_val, LWP_SUCCESS);

    ret_val = LWP_TerminateProcessSupport();
    EXPECT_EQ(ret_val, LWP_SUCCESS);
}

static void dummy_main_dispatch(void *p)
{
    int ret_val = 0;
    while (1) {
        ret_val = LWP_INTERNALSIGNAL(&LwpDeathTest::sync, 0);
        EXPECT_EQ(ret_val, LWP_SUCCESS);
        LWP_DispatchProcess();
    }
}

RVM_TEST_F(LwpDeathTest, dispatch_process)
{
    PROCESS main;
    PROCESS sec_proc;
    int ret_val = 0;

    ret_val = LWP_Init(LWP_VERSION, LWP_MAX_PRIORITY, &main);
    EXPECT_EQ(ret_val, LWP_SUCCESS);

    ret_val =
        LWP_CreateProcess(dummy_main_dispatch, 1, 1, NULL, "dummy", &sec_proc);
    EXPECT_EQ(ret_val, LWP_SUCCESS);

    ret_val = LWP_WaitProcess(&LwpDeathTest::sync);
    EXPECT_EQ(ret_val, LWP_SUCCESS);

    ret_val = LWP_DestroyProcess(sec_proc);
    EXPECT_EQ(ret_val, LWP_SUCCESS);

    ret_val = LWP_TerminateProcessSupport();
    EXPECT_EQ(ret_val, LWP_SUCCESS);
}

static void dummy_main_non_exiting(void *p)
{
    while (1) {
        ;
    }
}

RVM_TEST_F(LwpDeathTest, non_exiting_process)
{
    PROCESS main;
    PROCESS sec_proc;
    int ret_val = 0;

    ret_val = LWP_Init(LWP_VERSION, LWP_MAX_PRIORITY, &main);
    EXPECT_EQ(ret_val, LWP_SUCCESS);

    ret_val = LWP_CreateProcess(dummy_main_non_exiting, 1, 1, NULL, "dummy",
                                &sec_proc);
    EXPECT_EQ(ret_val, LWP_SUCCESS);

    ret_val = LWP_TerminateProcessSupport();
    EXPECT_EQ(ret_val, LWP_SUCCESS);
}

static void dummy_main_exiting(void *p)
{
    return;
}

RVM_TEST_F(LwpDeathTest, exiting_process)
{
    PROCESS main;
    PROCESS sec_proc;
    int ret_val = 0;

    ret_val = LWP_Init(LWP_VERSION, LWP_MAX_PRIORITY, &main);
    EXPECT_EQ(ret_val, LWP_SUCCESS);

    ret_val =
        LWP_CreateProcess(dummy_main_exiting, 1, 1, NULL, "dummy", &sec_proc);
    EXPECT_EQ(ret_val, LWP_SUCCESS);

    ret_val = LWP_TerminateProcessSupport();
    EXPECT_EQ(ret_val, LWP_SUCCESS);
}

} // namespace
