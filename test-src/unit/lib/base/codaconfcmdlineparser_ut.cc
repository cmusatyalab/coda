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

} // namespace
