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

TEST_F(StringKeyValueStoreTest, add_key_alias)
{
    const char *key            = "key1";
    const char *alias_key      = "key_alias";
    const char *expected_value = "value1";
    const char *actual_value;
    int ret_code = 0;

    ret_code = conf->add(key, expected_value);
    ASSERT_FALSE(ret_code);
    ret_code = conf->add_key_alias(key, alias_key);
    ASSERT_FALSE(ret_code);

    actual_value = conf->get_value(key);
    ASSERT_STREQ(expected_value, actual_value);

    actual_value = conf->get_value(alias_key);
    ASSERT_STREQ(expected_value, actual_value);
}

TEST_F(StringKeyValueStoreTest, add_same_key_twice)
{
    const char *key          = "key1";
    const char *first_value  = "val1";
    const char *second_value = "val2";
    const char *actual_value;
    int ret_code = 0;

    ret_code = conf->add(key, first_value);
    ASSERT_FALSE(ret_code);
    ret_code = conf->add(key, second_value);
    ASSERT_EQ(ret_code, EEXIST);

    actual_value = conf->get_value(key);
    ASSERT_STREQ(first_value, actual_value);
}

TEST_F(StringKeyValueStoreTest, replace_aliased_key)
{
    const char *key                = "key1";
    const char *alias_key          = "key_alias";
    const char *expected_value     = "value1";
    const char *new_expected_value = "value2";
    const char *actual_value;

    conf->add(key, expected_value);
    conf->add_key_alias(key, alias_key);

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

TEST_F(StringKeyValueStoreTest, has_key)
{
    const char *key1        = "key1";
    const char *key2        = "key2";
    const char *dummy_value = "dummy_value";

    ASSERT_FALSE(conf->has_key(key1));
    ASSERT_FALSE(conf->has_key(key2));

    conf->add(key1, dummy_value);

    ASSERT_TRUE(conf->has_key(key1));
    ASSERT_FALSE(conf->has_key(key2));
}

TEST_F(StringKeyValueStoreTest, set_alias_on_existing_value)
{
    const char *key1              = "key1";
    const char *key2              = "key2";
    const char *dummy_value       = "dummy_value";
    const char *dummy_value_alias = "dummy_value2";
    const char *actual_value      = NULL;
    int ret_code                  = 0;

    ASSERT_FALSE(conf->has_key(key1));
    ASSERT_FALSE(conf->has_key(key2));

    conf->add(key1, dummy_value);
    conf->add(key2, dummy_value_alias);

    ret_code = conf->add_key_alias(key1, key2);
    ASSERT_EQ(ret_code, EEXIST);

    ASSERT_TRUE(conf->has_key(key1));
    ASSERT_TRUE(conf->has_key(key2));

    actual_value = conf->get_value(key1);
    ASSERT_STREQ(actual_value, dummy_value);

    actual_value = conf->get_value(key2);
    ASSERT_STREQ(actual_value, dummy_value_alias);
}

TEST_F(StringKeyValueStoreTest, is_key_alias)
{
    const char *key            = "key1";
    const char *alias_key      = "key_alias";
    const char *other_key      = "other_key";
    const char *expected_value = "val";
    int ret_code               = 0;

    ret_code = conf->add(key, expected_value);
    ASSERT_FALSE(ret_code);
    ret_code = conf->add_key_alias(key, alias_key);
    ASSERT_FALSE(ret_code);

    ASSERT_TRUE(conf->has_key(key));
    ASSERT_TRUE(conf->has_key(alias_key));
    ASSERT_FALSE(conf->has_key(other_key));

    ASSERT_FALSE(conf->is_key_alias(key));
    ASSERT_TRUE(conf->is_key_alias(alias_key));
    ASSERT_FALSE(conf->is_key_alias(other_key));
}

} // namespace
