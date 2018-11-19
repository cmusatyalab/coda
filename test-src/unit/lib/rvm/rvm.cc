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

/* external */
#include <gtest/gtest.h>

/* from test */
#include <test/rvm/rvm.h>
#include <test/lwp/lwp.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <fcntl.h>

#ifdef HAVE_OSRELDATE_H
#include <osreldate.h>
#endif

/* from rvm */
#include <rvm/rds.h>
#include <rvm/rvm.h>
#include <rvm/rvm_segment.h>
#include <rvm/rvm_statistics.h>
#include <test/rvm/rvm.h>

#ifdef __cplusplus
}
#endif

static const char *VM_RVMADDR = (char *)0x50000000;

#ifndef MAX
#define MAX(a,b)  ( ((a) > (b)) ? (a) : (b) )
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

/*  *****  Private Variables  *****  */

static rvm_options_t Recov_Options;
static char *Recov_RvgAddr = 0;
static rvm_length_t Recov_RvgLength = 0;
static char *Recov_RdsAddr = 0;
static rvm_length_t Recov_RdsLength = 0;
static rvm_statistics_t Recov_Statistics;


static void Recov_CheckParms(struct rvm_config config)
{
    ASSERT_EQ(config.configInit, true);
    ASSERT_GE(config.LogDeviceSize, MinLogDeviceSize) << "RVM log segment too small";
    ASSERT_GE(config.DataDeviceSize, MAX(config.LogDeviceSize, MinDataDeviceSize)) << "RVM data segment too small";
}

static void Recov_InitRVM(struct rvm_config config)
{
    rvm_return_t ret;
    char *logdev = strdup(config.LogDevice);

    rvm_init_options(&Recov_Options);
    Recov_Options.log_dev = logdev;
    Recov_Options.truncate = 0;
    //Recov_Options.flags = RVM_COALESCE_TRANS;  /* oooh, daring */
    Recov_Options.flags = RVM_ALL_OPTIMIZATIONS;
    Recov_Options.flags |= RVM_MAP_PRIVATE;

    rvm_init_statistics(&Recov_Statistics);

    /* Get rid of any old log */
    unlink(config.LogDevice);

    /* Pass in the correct parameters so that RVM_INIT can create
        * a new logfile */
    Recov_Options.create_log_file = rvm_true;
    Recov_Options.create_log_size = RVM_MK_OFFSET(0, config.LogDeviceSize);
    Recov_Options.create_log_mode = 0600;
    /* as far as the log is concerned RVM_INIT will now handle the
        * rest of the creation. */

    LWPInit();

    ret = RVM_INIT(&Recov_Options);
    free(logdev);

    if (ret != RVM_SUCCESS) RVMDestroy(config);

    ASSERT_NE(ret, RVM_ELOG_VERSION_SKEW) << "RVM_INIT failed, destroying RVM ... ";
    ASSERT_EQ(ret, RVM_SUCCESS) << "RVM_INIT failed";
}

static void Recov_InitRDS(struct rvm_config config)
{
    rvm_return_t ret = 0;
    rvm_length_t devsize = 0;
    int fd = 0;
    int u_ret = 0;

    devsize = RVM_ROUND_LENGTH_DOWN_TO_PAGE_SIZE(config.DataDeviceSize);
    Recov_RdsAddr = (char *)VM_RVMADDR;
    Recov_RvgLength= RVM_ROUND_LENGTH_UP_TO_PAGE_SIZE(config.staticHeapRatio * config.DataDeviceSize);
    Recov_RdsLength= devsize - Recov_RvgLength - RVM_SEGMENT_HDR_SIZE;

    /* Initialize data segment. */

    fd = open(config.DataDevice, O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, 0600);
    ASSERT_NE(fd, 0) << "Couldn't open RVM data device";

    u_ret = ftruncate(fd, config.DataDeviceSize);
    ASSERT_EQ(u_ret, 0) << "Couldn't truncate RVM data device";

    u_ret = close(fd);
    ASSERT_EQ(u_ret, 0) << "Couldn't close RVM data device";

    rds_zap_heap(config.DataDevice, RVM_LENGTH_TO_OFFSET(devsize),
                 Recov_RdsAddr,
                 Recov_RvgLength,
                 Recov_RdsLength,
                 (unsigned long)DFLT_RDSNL,
                 (unsigned long)DFLT_RDSCS, 
                 &ret);
    ASSERT_EQ(ret, SUCCESS) << "rds_zap_heap failed";

    rds_load_heap(config.DataDevice, RVM_LENGTH_TO_OFFSET(config.DataDeviceSize), &Recov_RvgAddr, &ret);
    ASSERT_EQ(ret, SUCCESS) << "rds_load_heap failed";
}

void RVMInit(struct rvm_config config)
{
    /* Set unset parameters to defaults (as appropriate). */
    Recov_CheckParms(config);

    /* Initialize the RVM package. */
    Recov_InitRVM(config);
    Recov_InitRDS(config);
}

void RVMDestroy(struct rvm_config config)
{
    int ret = 0;
    rvm_return_t rvmret = 0;

    rds_unload_heap(&ret);
    EXPECT_EQ(ret, 0) << "Error unloading heap";

    rvmret = rvm_terminate();
    EXPECT_EQ(rvmret, RVM_SUCCESS) << "Unable to terminate RVM";


    /* Remove log and data files */
    ret = unlink(config.LogDevice);
    EXPECT_EQ(ret, 0) << "Unable to delete log device";
    ret = unlink(config.DataDevice);
    EXPECT_EQ(ret, 0) << "Unable to delete data device";

    LWPUnInit();
}
