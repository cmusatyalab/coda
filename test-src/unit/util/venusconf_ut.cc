#include "gtest/gtest.h"
#include "gmock/gmock-matchers.h"
#include <venusconf.h>

namespace
{
using ::testing::StartsWith;

class VenusConfTest : public ::testing::Test {
protected:
    VenusConf *conf;
    void SetUp() override { conf = new VenusConf(); }

    void TearDown() override { delete (conf); }
};

TEST_F(VenusConfTest, get_int_value)
{
    const char *key     = "key1";
    const char *value   = "12";
    const int value_int = 12;
    const char *actual_value;
    int actual_value_int;

    conf->add(key, value);
    actual_value = conf->get_value(key);
    ASSERT_STREQ(value, actual_value);

    actual_value_int = conf->get_int_value(key);
    ASSERT_EQ(actual_value_int, value_int);
}

TEST_F(VenusConfTest, get_bool_value)
{
    const char *key   = "key1";
    const char *value = "1";
    const char *actual_value;

    conf->add(key, value);
    actual_value = conf->get_value(key);
    ASSERT_STREQ(value, actual_value);

    ASSERT_TRUE(conf->get_bool_value(key));

    conf->set(key, "0");
    ASSERT_FALSE(conf->get_bool_value(key));
}

TEST_F(VenusConfTest, add_on_off_pair)
{
    const char *on_key  = "on";
    const char *off_key = "off";

    conf->add_on_off_pair(on_key, off_key, "1");
    ASSERT_TRUE(conf->get_bool_value(on_key));
    ASSERT_FALSE(conf->get_bool_value(off_key));
}

TEST_F(VenusConfTest, add_and_set_on_off_pair)
{
    const char *on_key  = "on";
    const char *off_key = "off";

    conf->add_on_off_pair(on_key, off_key, "1");
    ASSERT_TRUE(conf->get_bool_value(on_key));
    ASSERT_FALSE(conf->get_bool_value(off_key));

    conf->set(on_key, "0");
    ASSERT_FALSE(conf->get_bool_value(on_key));
    ASSERT_TRUE(conf->get_bool_value(off_key));
}

TEST_F(VenusConfTest, set_int)
{
    const char *key   = "int_key";
    const int int_val = 5;
    int actual_val    = 0;

    conf->set_int(key, int_val);

    actual_val = conf->get_int_value(key);
    ASSERT_EQ(int_val, actual_val);
}

TEST_F(VenusConfTest, set_int64_t_max)
{
    const char *key       = "int_key";
    const int64_t int_val = INT64_MAX;
    int64_t actual_val    = 0;

    conf->set_int(key, int_val);

    actual_val = conf->get_int_value(key);
    ASSERT_EQ(int_val, actual_val);
}

TEST_F(VenusConfTest, set_int64_t_min)
{
    const char *key       = "int_key";
    const int64_t int_val = INT64_MIN;
    int64_t actual_val    = 0;

    conf->set_int(key, int_val);

    actual_val = conf->get_int_value(key);
    ASSERT_EQ(int_val, actual_val);
}

TEST_F(VenusConfTest, set_uint64_t_max)
{
    const char *key        = "int_key";
    const uint64_t int_val = UINT64_MAX;
    uint64_t actual_val    = 0;

    conf->set_int(key, int_val);

    actual_val = conf->get_int_value(key);
    ASSERT_EQ(int_val, actual_val);
}

TEST_F(VenusConfTest, relative_path_handling)
{
    const char *actual_value = NULL;

    conf->load_default_config();

    conf->set("pid_file", "run.pid");
    actual_value = conf->get_value("pid_file");
    ASSERT_STREQ(actual_value, "run.pid");

    conf->set("run_control_file", "control_file");
    actual_value = conf->get_value("run_control_file");
    ASSERT_STREQ(actual_value, "control_file");

    conf->configure_cmdline_options();
    conf->apply_consistency_rules();

    actual_value = conf->get_value("pid_file");
    ASSERT_THAT(actual_value, StartsWith(conf->get_value("cachedir")));

    actual_value = conf->get_value("run_control_file");
    ASSERT_THAT(actual_value, StartsWith(conf->get_value("cachedir")));
}

} // namespace
