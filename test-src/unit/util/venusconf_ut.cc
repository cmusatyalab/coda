#include <venusconf.h>
#include "gtest/gtest.h"

namespace
{
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

} // namespace
