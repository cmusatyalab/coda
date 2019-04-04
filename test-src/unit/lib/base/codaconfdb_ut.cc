#include <codaconfdb.h>
#include "gtest/gtest.h"

namespace
{
class codaconfdb : public ::testing::Test {
protected:
    CodaConfDB *conf;
    void SetUp() override { conf = new CodaConfDB(); }

    void TearDown() override { delete (conf); }
};

// codaconf.
TEST_F(codaconfdb, get_non_added_value)
{
    const char *key            = "key1";
    const char *expected_value = "value1";
    const char *actual_value;

    actual_value = conf->get_value(key);
    ASSERT_FALSE(actual_value);
}

TEST_F(codaconfdb, add)
{
    const char *key            = "key1";
    const char *expected_value = "value1";
    const char *actual_value;

    conf->add(key, expected_value);
    actual_value = conf->get_value(key);

    ASSERT_STREQ(expected_value, actual_value);
}

TEST_F(codaconfdb, replace)
{
    const char *key          = "key1";
    const char *first_value  = "value1";
    const char *second_value = "value2";
    const char *actual_value;

    conf->add(key, first_value);

    actual_value = conf->get_value(key);
    ASSERT_STREQ(first_value, actual_value);

    conf->replace(key, second_value);

    actual_value = conf->get_value(key);
    ASSERT_STREQ(second_value, actual_value);
}

} // namespace
