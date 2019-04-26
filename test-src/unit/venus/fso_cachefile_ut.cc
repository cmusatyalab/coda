/* BLURB gpl
                           Coda File System
                              Release 7
          Copyright (c) 1987-2019 Carnegie Mellon University
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
#include <venus/venusconf.h>

using namespace std;

namespace
{
class CacheFileTest : public ::testing::Test {
protected:
    void SetUp() override { GetVenusConf().load_default_config(); }

    void TearDown() override {}
};

void get_container_file_path(int i, char name[300])
{
    char cwd[256];
    getcwd(cwd, 256);
    sprintf(name, "%s/%02X/%02X/%02X/%02X", cwd, (i >> 24) & 0xff,
            (i >> 16) & 0xff, (i >> 8) & 0xff, i & 0xff);
}

void get_container_file_root_path(int i, char name[300])
{
    char cwd[256];
    getcwd(cwd, 256);
    sprintf(name, "%s/%02X", cwd, (i >> 24) & 0xff);
}

void delete_container_root_path(int i)
{
    char root_path[300];
    char rm_cmd[320];
    get_container_file_root_path(i, root_path);
    sprintf(rm_cmd, "rm -rf %s", root_path);
    EXPECT_EQ(system(rm_cmd), 0);
}

void EXPECT_FILE_EXISTS(char name[13], bool exists)
{
    struct stat stat_b;

    if (exists) {
        EXPECT_FALSE(stat(name, &stat_b))
            << "File " << name << " doesn't exists";
    } else {
        EXPECT_TRUE(stat(name, &stat_b)) << "File " << name << " exists";
    }
}

TEST_F(CacheFileTest, construct)
{
    int idx = rand() & 0x7FF;
    char cachefile_path[300];
    CacheFile *cf = new CacheFile(idx, 0, rand() % 2);

    get_container_file_path(idx, cachefile_path);

    {
        SCOPED_TRACE("prior delete");
        EXPECT_FILE_EXISTS(cachefile_path, false);
    }

    delete (cf);

    {
        SCOPED_TRACE("after delete");
        EXPECT_FILE_EXISTS(cachefile_path, false);
    }

    delete_container_root_path(idx);
}

TEST_F(CacheFileTest, create)
{
    int idx       = rand() & 0x7FF;
    int size      = rand() & 0x7FF;
    CacheFile *cf = new CacheFile(idx, 0, rand() % 2);
    char cachefile_path[300];
    struct stat stat_b;

    get_container_file_path(idx, cachefile_path);

    {
        SCOPED_TRACE("CacheFile Create");
        cf->Create(size);
        EXPECT_FILE_EXISTS(cachefile_path, true);
        stat(cachefile_path, &stat_b);
        EXPECT_EQ(stat_b.st_size, size);
        EXPECT_EQ(stat_b.st_blocks, 0);
    }

    cf->Truncate(0);

    {
        SCOPED_TRACE("After Truncate");
        EXPECT_FILE_EXISTS(cachefile_path, true);
        stat(cachefile_path, &stat_b);
        EXPECT_EQ(stat_b.st_size, 0);
        EXPECT_EQ(stat_b.st_blocks, 0);
    }

    delete (cf);

    EXPECT_FILE_EXISTS(cachefile_path, false);

    delete_container_root_path(idx);
}

TEST_F(CacheFileTest, create_ref_cnt)
{
    int idx       = rand() & 0x7FF;
    int size      = rand() & 0x7FF;
    CacheFile *cf = new CacheFile(idx, 0, rand() % 2);
    char cachefile_path[300];
    struct stat stat_b;

    get_container_file_path(idx, cachefile_path);

    cf->Create(size);
    EXPECT_FILE_EXISTS(cachefile_path, true);
    stat(cachefile_path, &stat_b);
    EXPECT_EQ(stat_b.st_size, size);
    EXPECT_EQ(stat_b.st_blocks, 0);

    cf->IncRef();
    cf->IncRef();
    cf->IncRef();

    EXPECT_FILE_EXISTS(cachefile_path, true);

    cf->DecRef();
    cf->DecRef();
    cf->DecRef();
    cf->DecRef();

    EXPECT_FILE_EXISTS(cachefile_path, false);

    delete (cf);

    EXPECT_FILE_EXISTS(cachefile_path, false);

    delete_container_root_path(idx);
}

} // namespace
