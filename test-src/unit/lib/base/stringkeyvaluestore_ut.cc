#include <stringkeyvaluestore.h>
#include "gtest/gtest.h"

namespace
{
class StringKeyValueStoreTest : public ::testing::Test {
protected:
    StringKeyValueStore *conf;
    void SetUp() override { conf = new StringKeyValueStore(); }

    void TearDown() override { delete (conf); }
};

TEST_F(StringKeyValueStoreTest, get_non_added_value)
{
    const char *key = "key1";
    const char *actual_value;

    actual_value = conf->get_value(key);
    ASSERT_FALSE(actual_value);
}

TEST_F(StringKeyValueStoreTest, add)
{
    const char *key            = "key1";
    const char *expected_value = "value1";
    const char *actual_value;

    conf->add(key, expected_value);
    actual_value = conf->get_value(key);

    ASSERT_STREQ(expected_value, actual_value);
}

TEST_F(StringKeyValueStoreTest, replace)
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
