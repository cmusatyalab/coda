#include <stringkeyvaluestore.h>
#include <codaconffileparser.h>
#include "gtest/gtest.h"

namespace
{
class CodaConffileParserTest : public ::testing::Test {
protected:
    char conf_path[200];
    void SetUp() override
    {
        getcwd(conf_path, 100);
        sprintf(conf_path, "%s/%s", conf_path, "lib/base/venus.conf.sample");
    }

    void TearDown() override {}
};

TEST_F(CodaConffileParserTest, parse_into_store)
{
    const char *key                   = "cachesize";
    const char *original_value        = "200MB";
    const char *expected_parsed_value = "100MB";
    const char *actual_value;
    StringKeyValueStore original_store;
    CodaConfFileParser parser(original_store);

    original_store.add("cachesize", original_value);
    actual_value = original_store.get_value(key);
    ASSERT_STREQ(original_value, actual_value);

    parser.set_conffile(conf_path);
    parser.parse();
    actual_value = original_store.get_value(key);
    ASSERT_STREQ(expected_parsed_value, actual_value);
}

TEST_F(CodaConffileParserTest, persistance_parsed_values_after_destroy)
{
    const char *key            = "cachesize";
    const char *expected_value = "100MB";
    const char *actual_value;
    StringKeyValueStore original_store;
    CodaConfFileParser *parser = new CodaConfFileParser(original_store);

    parser->set_conffile(conf_path);
    parser->parse();
    actual_value = original_store.get_value(key);
    ASSERT_STREQ(expected_value, actual_value);

    delete parser;

    actual_value = original_store.get_value(key);
    ASSERT_STREQ(expected_value, actual_value);
}

TEST_F(CodaConffileParserTest, check_collisions_when_parsed_twice)
{
    int ret_code = 0;
    StringKeyValueStore original_store;
    CodaConfFileParser parser(original_store);

    parser.set_conffile(conf_path);

    ret_code = parser.parse();
    ASSERT_FALSE(ret_code);

    ret_code = parser.parse();
    ASSERT_EQ(ret_code, EEXIST);
}

TEST_F(CodaConffileParserTest, check_collisions_after_add)
{
    const char *key                   = "cachesize";
    const char *original_value        = "200MB";
    const char *expected_parsed_value = "100MB";
    const char *actual_value;
    int ret_code = 0;
    StringKeyValueStore original_store;
    CodaConfFileParser parser(original_store);

    parser.set_conffile(conf_path);

    original_store.add("cachesize", original_value);
    actual_value = original_store.get_value(key);
    ASSERT_STREQ(original_value, actual_value);

    ret_code = parser.parse();
    ASSERT_EQ(ret_code, EEXIST);

    original_store.add("cachesize", original_value);
    actual_value = original_store.get_value(key);
    ASSERT_STREQ(expected_parsed_value, actual_value);
}

} // namespace
