/* BLURB gpl

                           Coda File System
                              Release 8

          Copyright (c) 2018-2025 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently
#*/

#include <stdio.h>
#include <time.h>
#include <stdlib.h>

#include <gtest/gtest.h>

int main(int argc, char **argv)
{
    int32_t seed = 0;
    testing::InitGoogleTest(&argc, argv);

    /* Get the random seed used by gtest */
    seed = ::testing::GTEST_FLAG(random_seed);

    if (seed == 0) {
        seed                               = time(nullptr) & 0xFFFF;
        ::testing::GTEST_FLAG(random_seed) = seed;
    }

    srand(seed);

    return RUN_ALL_TESTS();
}
