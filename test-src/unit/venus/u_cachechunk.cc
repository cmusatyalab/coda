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
#include <sys/stat.h>

/* external */
#include <gtest/gtest.h>

/* from coda-src */
#include <venus/fso_cachefile.h>

/* from test-src */
#include <test/rvm/rvm.h>

namespace {

// cachechunk.
TEST(cachechunk, construct) {
    int start = rand() & 0x7FF;
    int len = rand() & 0x7F;
    char name[13];
    struct stat stat_b;
    CacheChunk * cc = new CacheChunk(start, len);

    EXPECT_EQ(cc->GetLength(), len);
    EXPECT_EQ(cc->GetStart(), start);
    EXPECT_TRUE(cc->isValid());

    delete cc;

    cc = new CacheChunk();
    EXPECT_FALSE(cc->isValid());

    delete cc;
}

TEST(cachechunk, list) {
    int list_len = rand() & 0x3F;
    int * start = new int[list_len];
    int * len = new int[list_len];
    dlist cachechunk_list;
    CacheChunk * cc = NULL;

    /* Create a list with random cachechunks */
    for (int i = 0;  i < list_len; i++) {
        start[i] =  rand() & 0x7FF;
        len[i] =  rand() & 0x7F;

        cachechunk_list.insert((dlink *) new CacheChunk(start[i], len[i]));
    }

    EXPECT_EQ(list_len, cachechunk_list.count());

    /* Check the elements in the list */
    for (int i = 0;  i < list_len; i++) {
        cc = (CacheChunk *) cachechunk_list.last();

        EXPECT_EQ(start[i], cc->GetStart());
        EXPECT_EQ(len[i], cc->GetLength());
        EXPECT_TRUE(cc->isValid());

        cachechunk_list.remove((dlink *) cc);

        EXPECT_EQ(list_len - 1 - i, cachechunk_list.count());

        delete cc;
    }

    delete [] start;
    delete [] len;

}

}  // namespace
