#include <stringkeyvaluestore.h>
#include <codaconfcmdlineparser.h>
#include "gtest/gtest.h"

namespace
{
class CodaConfCmdLineParserTest : public ::testing::Test {
protected:
    char conf_path[200];
    char *argv[3];
    const int argc = 3;
    void SetUp() override
    {
        getcwd(conf_path, 100);
        sprintf(conf_path, "%s/%s", conf_path, "lib/base/venus.conf.sample");
        argv[0] = "unit";
        argv[1] = "cachesize";
        argv[2] = "100MB";
    }

    void TearDown() override {}
};

TEST_F(CodaConfCmdLineParserTest, parse_into_store)
{
    const char *key                   = "cachesize";
    const char *original_value        = "200MB";
    const char *expected_parsed_value = "100MB";
    const char *actual_value;
    StringKeyValueStore original_store;
    CodaConfCmdLineParser parser(original_store);

    original_store.add("cachesize", original_value);
    actual_value = original_store.get_value(key);
    ASSERT_STREQ(original_value, actual_value);

    parser.set_args(argc, argv);
    parser.parse();
    actual_value = original_store.get_value(key);
    ASSERT_STREQ(expected_parsed_value, actual_value);
}

TEST_F(CodaConfCmdLineParserTest, persistance_parsed_values_after_destroy)
{
    const char *key            = "cachesize";
    const char *expected_value = "100MB";
    const char *actual_value;
    StringKeyValueStore original_store;
    CodaConfCmdLineParser *parser = new CodaConfCmdLineParser(original_store);

    parser->set_args(argc, argv);
    parser->parse();
    actual_value = original_store.get_value(key);
    ASSERT_STREQ(expected_value, actual_value);

    delete parser;

    actual_value = original_store.get_value(key);
    ASSERT_STREQ(expected_value, actual_value);
}

TEST_F(CodaConfCmdLineParserTest, check_collisions_when_parsed_twice)
{
    const char *key                   = "cachesize";
    const char *original_value        = "200MB";
    const char *expected_parsed_value = "100MB";
    const char *actual_value;
    int ret_code = 0;
    StringKeyValueStore original_store;
    CodaConfCmdLineParser parser(original_store);

    parser.set_args(argc, argv);

    ret_code = parser.parse();
    ASSERT_FALSE(ret_code);

    ret_code = parser.parse();
    ASSERT_EQ(ret_code, EEXIST);
}

TEST_F(CodaConfCmdLineParserTest, check_collisions_after_add)
{
    const char *key                   = "cachesize";
    const char *original_value        = "200MB";
    const char *expected_parsed_value = "100MB";
    const char *actual_value;
    int ret_code = 0;
    StringKeyValueStore original_store;
    CodaConfCmdLineParser parser(original_store);

    parser.set_args(argc, argv);

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
