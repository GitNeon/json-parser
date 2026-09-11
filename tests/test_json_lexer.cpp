#include <gtest/gtest.h>
#include "lexer.hpp"

#include <string>

/*
JsonLexer 测试组织（均为 JsonLexerTest 套件）：

1. 基础记号流：简单对象 / 数组的 nextToken 顺序、字面值与行列位置
2. 字面量：字符串（转义 / \u / 未闭合）、数字（负数 / 小数 / 指数）、关键字
3. peekToken：预读不消费
4. 空白与换行：skipWhitespace 与 line / column 跟踪
5. 异常：非法字符、非法转义、非法数字、未知关键字

*/

// ==================== 1. 基础记号流 ====================

TEST(JsonLexerTest, SimpleObjectTest) {
    // 输入 {"a":123}，期望记号序列：
    // LBrace -> String("a") -> Colon -> Number("123") -> RBrace -> EndOfFile
    JsonLexer lexer(R"({"a":123})");

    // 记号 1：{（符号类记号的 value 恒为空串）
    Token token = lexer.nextToken();
    EXPECT_EQ(token.m_type, TokenType::LBrace);
    EXPECT_EQ(token.m_value, "");
    EXPECT_EQ(token.m_line, 1U);
    EXPECT_EQ(token.m_column, 1U);

    // 记号 2：字符串键 "a"，value 为去掉外层双引号后的内容
    token = lexer.nextToken();
    EXPECT_EQ(token.m_type, TokenType::String);
    EXPECT_EQ(token.m_value, "a");
    EXPECT_EQ(token.m_line, 1U);
    EXPECT_EQ(token.m_column, 2U);

    // 记号 3：冒号
    token = lexer.nextToken();
    EXPECT_EQ(token.m_type, TokenType::Colon);
    EXPECT_EQ(token.m_value, "");
    EXPECT_EQ(token.m_column, 5U);

    // 记号 4：数字 123，词法阶段只保留原文，转 double 交给 Parser
    token = lexer.nextToken();
    EXPECT_EQ(token.m_type, TokenType::Number);
    EXPECT_EQ(token.m_value, "123");
    EXPECT_EQ(token.m_column, 6U);

    // 记号 5：}
    token = lexer.nextToken();
    EXPECT_EQ(token.m_type, TokenType::RBrace);
    EXPECT_EQ(token.m_value, "");
    EXPECT_EQ(token.m_column, 9U);

    // 最后一个可见字符消费完毕，源串已到末尾
    EXPECT_TRUE(lexer.isAtEnd());

    // 记号 6：EOF 结束标志
    token = lexer.nextToken();
    EXPECT_EQ(token.m_type, TokenType::EndOfFile);
    EXPECT_EQ(token.m_value, "");

    // 到达 EOF 后继续调用仍返回 EOF，不会崩溃
    EXPECT_EQ(lexer.nextToken().m_type, TokenType::EndOfFile);
}
