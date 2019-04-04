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

TEST_F(StringKeyValueStoreTest, set_key_alias)
{
    const char *key            = "key1";
    const char *alias_key      = "key_alias";
    const char *expected_value = "value1";
    const char *actual_value;

    conf->add(key, expected_value);
    conf->set_key_alias(key, alias_key);

    actual_value = conf->get_value(key);
    ASSERT_STREQ(expected_value, actual_value);

    actual_value = conf->get_value(alias_key);
    ASSERT_STREQ(expected_value, actual_value);
}

TEST_F(StringKeyValueStoreTest, replace_aliased_key)
{
    const char *key                = "key1";
    const char *alias_key          = "key_alias";
    const char *expected_value     = "value1";
    const char *new_expected_value = "value2";
    const char *actual_value;

    conf->add(key, expected_value);
    conf->set_key_alias(key, alias_key);

    actual_value = conf->get_value(key);
    ASSERT_STREQ(expected_value, actual_value);

    actual_value = conf->get_value(alias_key);
    ASSERT_STREQ(expected_value, actual_value);

    conf->replace(alias_key, new_expected_value);

    actual_value = conf->get_value(key);
    ASSERT_STREQ(new_expected_value, actual_value);

    actual_value = conf->get_value(alias_key);
    ASSERT_STREQ(new_expected_value, actual_value);
}

} // namespace
