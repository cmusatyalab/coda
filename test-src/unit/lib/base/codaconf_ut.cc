#include <codaconf.h>
#include "gtest/gtest.h"

namespace
{
class codaconf : public ::testing::Test {
protected:
    char conf_path[200];
    void SetUp() override
    {
        getcwd(conf_path, 100);
        sprintf(conf_path, "%s/%s", conf_path, "lib/base/venus.conf.sample");
        codaconf_init(conf_path);
    }

    void TearDown() override { codaconf_free(); }
};

// codaconf.
TEST_F(codaconf, lookup)
{
    const char *value;

    value = codaconf_lookup("cachesize", "0");

    ASSERT_STRNE(value, "0");
    ASSERT_STREQ(value, "100MB");
}

TEST_F(codaconf, lookup_fail)
{
    const char *value;

    value = codaconf_lookup("realm", "0");

    ASSERT_STREQ(value, "0");
}

} // namespace
