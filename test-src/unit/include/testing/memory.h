/* BLURB gpl

                           Coda File System
                              Release 7

             Copyright (c) 2019 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#ifndef _CODA_MEMORY_H_
#define _CODA_MEMORY_H_ 1

#ifdef __cplusplus
extern "C" {
#endif

#include <valgrind/memcheck.h>

#ifdef __cplusplus
}
#endif

#include "gtest/gtest.h"

#define TEST_FOR_MEMORY_LEAKS_SETUP                                   \
    unsigned long base_definite, base_dubious, base_reachable,        \
        base_suppressed;                                              \
    unsigned long leaked, dubious, reachable, suppressed;             \
    VALGRIND_DO_LEAK_CHECK;                                           \
    VALGRIND_COUNT_LEAKS(base_definite, base_dubious, base_reachable, \
                         base_suppressed);

#define TEST_FOR_MEMORY_LEAKS_CHECK                               \
    VALGRIND_DO_LEAK_CHECK;                                       \
    VALGRIND_COUNT_LEAKS(leaked, dubious, reachable, suppressed); \
    EXPECT_EQ(base_definite, leaked) << "Memory Leak found";      \
    EXPECT_EQ(base_dubious, dubious) << "Memory Leak found";      \
    EXPECT_EQ(base_reachable, reachable) << "Memory Leak found";  \
    EXPECT_EQ(base_suppressed, suppressed) << "Memory Leak found";

#define RVM_TEST_BODY(test_case_name, test)                          \
    SCOPED_TRACE("Memory Leaks Found");                              \
    ASSERT_EXIT(({                                                   \
                    TEST_FOR_MEMORY_LEAKS_SETUP                      \
                    test_case_name##_##test##_RvmTest();             \
                    VALGRIND_DO_LEAK_CHECK;                          \
                    VALGRIND_COUNT_LEAKS(leaked, dubious, reachable, \
                                         suppressed);                \
                    TEST_FOR_MEMORY_LEAKS_CHECK                      \
                    exit(::testing::Test::HasFailure() ? 1 : 0);     \
                }),                                                  \
                ::testing::ExitedWithCode(0), ".*");

/* Google test wrapper to support memory leaks testing for forked
 * processes. Tests that require RVM should fork/clone since RVM
 * is NOT reentrant. */
#define RVM_TEST(test_case_name, test)                                 \
    static void test_case_name##_##test##_RvmTest();                   \
    TEST(test_case_name, test) { RVM_TEST_BODY(test_case_name, test) } \
    static void test_case_name##_##test##_RvmTest()

#define RVM_TEST_F(test_case_name, test)                                 \
    static void test_case_name##_##test##_RvmTest();                     \
    TEST_F(test_case_name, test) { RVM_TEST_BODY(test_case_name, test) } \
    static void test_case_name##_##test##_RvmTest()

#endif /* _CODA_MEMORY_H_ */
