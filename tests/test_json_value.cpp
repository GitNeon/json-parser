#include <gtest/gtest.h>
#include "value.hpp"

TEST(JsonValueTest, DefaultIsNull) {
    JsonValue v;
    EXPECT_TRUE(v.isNull());
    EXPECT_EQ(v.type(), JsonType::Null);
}
