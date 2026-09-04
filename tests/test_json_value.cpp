#include <gtest/gtest.h>
#include "value.hpp"

/*

# 方法解读：
TEST(TestSuiteName, TestName)

- TestSuiteName：测试套件名称，用于分组相关的测试
- TestName：具体的测试名称，描述要测试什么

# 关键字前缀解读
EXPECT: 测试继续执行，即使失败
ASSERT: 测试立即终止，如果失败
*/

TEST(JsonValueTest, DefaultIsNull) {
    JsonValue my_json;
    EXPECT_TRUE(my_json.isNull());
    EXPECT_EQ(my_json.type(), JsonType::Null);
}

TEST(JsonValueTest, ConstructorIsNull) {
    JsonValue my_json = JsonValue(nullptr);  // 显式调用 JsonValue(std::nullptr_t) 构造函数
    EXPECT_TRUE(my_json.isNull());
    EXPECT_EQ(my_json.type(), JsonType::Null);
}
