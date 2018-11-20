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

void get_container_file_path(int i, char name[13]) {
    sprintf(name, "%02X/%02X/%02X/%02X",
           (i>>24) & 0xff, (i>>16) & 0xff, (i>>8) & 0xff, i & 0xff);
}

void EXPECT_FILE_EXISTS(char name[13], bool exists) {
    struct stat stat_b;

    if (exists) {
        EXPECT_FALSE(stat(name, &stat_b)) << "File " << name << " doesn't exists";
    } else {
        EXPECT_TRUE(stat(name, &stat_b)) << "File " << name << " exists";
    }
        
}

// cachefile.
TEST(cachefile, construct) {
    int idx = rand() & 0x7FF;
    char name[13];
    struct stat stat_b;
    CacheFile * cf = new CacheFile(idx, 0);

    get_container_file_path(idx, name);

    EXPECT_FILE_EXISTS(name, false);

    delete(cf);

    EXPECT_FILE_EXISTS(name, false);
}

TEST(cachefile, create) {
    int idx = rand() & 0x7FF;
    int size = rand() & 0x7FF;
    CacheFile * cf = new CacheFile(idx, 0);
    char name[13];
    struct stat stat_b;

    get_container_file_path(idx, name);

    cf->Create(size);
    EXPECT_FILE_EXISTS(name, true);
    stat(name, &stat_b);
    EXPECT_EQ(stat_b.st_size, size);
    EXPECT_EQ(stat_b.st_blocks, 0);


    cf->Truncate(0);

    EXPECT_FILE_EXISTS(name, true);
    stat(name, &stat_b);
    EXPECT_EQ(stat_b.st_size, 0);
    EXPECT_EQ(stat_b.st_blocks, 0);

    delete(cf);

    EXPECT_FILE_EXISTS(name, false);
}

TEST(cachefile, create_ref_cnt) {
    int idx = rand() & 0x7FF;
    int size = rand() & 0x7FF;
    CacheFile * cf = new CacheFile(idx, 0);
    char name[13];
    struct stat stat_b;

    get_container_file_path(idx, name);

    cf->Create(size);
    EXPECT_FILE_EXISTS(name, true);
    stat(name, &stat_b);
    EXPECT_EQ(stat_b.st_size, size);
    EXPECT_EQ(stat_b.st_blocks, 0);

    cf->IncRef();
    cf->IncRef();
    cf->IncRef();

    EXPECT_FILE_EXISTS(name, true);

    cf->DecRef();
    cf->DecRef();
    cf->DecRef();
    cf->DecRef();

    EXPECT_FILE_EXISTS(name, false);

    delete(cf);

    EXPECT_FILE_EXISTS(name, false);
}

}  // namespace
