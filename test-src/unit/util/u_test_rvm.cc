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
#include <stdint.h>

/* external */
#include <gtest/gtest.h>

/* from test-src */
#include <test/rvm/rvm.h>
#include <test/rvm/recov.h>

namespace {

// test_rvm.

// Reinitialization of rvm is NOT supported
TEST(test_rvm, init_destroy) {
}

// Reinitialization of rvm is NOT supported
TEST(test_rvm, DISABLED_reinit) {
    struct rvm_config config;
    INIT_RVM_CONFIG(config)

    RVMInit(config);
    RVMDestroy(config);

    INIT_RVM_CONFIG(config);

    RVMInit(config);
    RVMDestroy(config);
}

TEST(test_rvm, alloc) {
    void * test_alloc = NULL;

    Recov_BeginTrans();

    test_alloc = rvmlib_rec_malloc(sizeof(uint32_t));
    EXPECT_TRUE(test_alloc);

    rvmlib_rec_free(test_alloc);

    Recov_EndTrans(0);
}

}  // namespace
