#include <gtest/gtest.h>
#include "parser.hpp"

#include <string>

/*
JsonParser 测试组织（均为 JsonParserTest 套件）：

1. 基础类型：null / bool / number（整数、小数、负数、指数）/ string
2. 容器：空对象、空数组、对象、数组的常规解析
3. 嵌套结构：多层对象嵌套、多维数组、混合结构
4. 重复 key：后出现的值覆盖先出现的（last-wins）
5. 异常：尾随逗号、非字符串 key、缺冒号、缺逗号、
   括号不闭合、顶层多余内容、空输入、递归深度超限
6. 错误定位：JsonParseException 携带的行号 / 列号准确

常用断言速查：
    EXPECT_EQ(a, b)      相等
    EXPECT_DOUBLE_EQ     浮点相等（带误差容忍）
    EXPECT_TRUE / FALSE  布尔判断
    EXPECT_THROW(expr, ExceptionType)  表达式抛出指定异常
*/

namespace {

// 解析辅助：把 JSON 文本解析成 JsonValue，简化用例书写
JsonValue parseOk(const std::string& src) {
    return JsonParser(src).parse();
}

// 期望解析失败：返回异常对象供进一步断言（行号/列号）
JsonParseException parseBad(const std::string& src) {
    try {
        // [[nodiscard]] 约束返回值必须被使用，这里故意丢弃并触发异常路径
        JsonValue ignored = JsonParser(src).parse();
        (void)ignored;
    } catch (const JsonParseException& e) {
        return e;
    }
    // 到这里说明本该失败的输入解析成功了，标记用例失败
    ADD_FAILURE() << "expected JsonParseException for input: " << src;
    return JsonParseException(0, 0, "unreachable");
}

}  // namespace

// ==================== 1. 基础类型 ====================

TEST(JsonParserTest, ParseNull) {
    JsonValue v = parseOk("null");
    EXPECT_TRUE(v.isNull());
    EXPECT_EQ(v.type(), JsonType::Null);
}

TEST(JsonParserTest, ParseBooleans) {
    JsonValue t = parseOk("true");
    EXPECT_TRUE(t.isBool());
    EXPECT_TRUE(t.asBool());

    JsonValue f = parseOk("false");
    EXPECT_TRUE(f.isBool());
    EXPECT_FALSE(f.asBool());
}

TEST(JsonParserTest, ParseNumbers) {
    // 整数
    EXPECT_DOUBLE_EQ(parseOk("0").asNumber(), 0.0);
    EXPECT_DOUBLE_EQ(parseOk("42").asNumber(), 42.0);
    // 负数与小数
    EXPECT_DOUBLE_EQ(parseOk("-1.5").asNumber(), -1.5);
    // 指数
    EXPECT_DOUBLE_EQ(parseOk("1e3").asNumber(), 1000.0);
    EXPECT_DOUBLE_EQ(parseOk("-2.5E-2").asNumber(), -0.025);
}

TEST(JsonParserTest, ParseString) {
    JsonValue v = parseOk(R"("hello")");
    EXPECT_TRUE(v.isString());
    EXPECT_EQ(v.asString(), "hello");

    // 转义字符由 Lexer 还原：JSON 的 \" \\ \n 转成真实字符
    // 注意：行注释结尾绝不能是反斜杠——"行尾\+换行"会被拼接，
    // 注释会吞掉下一行代码（编译期 line splice 陷阱）
    JsonValue esc = parseOk(R"("a\"b\\c\n")");
    EXPECT_EQ(esc.asString(), "a\"b\\c\n");
}

// ==================== 2. 容器 ====================

TEST(JsonParserTest, ParseEmptyContainers) {
    JsonValue obj = parseOk("{}");
    EXPECT_TRUE(obj.isObject());
    EXPECT_EQ(obj.size(), 0U);

    JsonValue arr = parseOk("[]");
    EXPECT_TRUE(arr.isArray());
    EXPECT_EQ(arr.size(), 0U);
}

TEST(JsonParserTest, ParseSimpleObject) {
    JsonValue v = parseOk(R"({"name": "Alice", "age": 30, "vip": true})");

    EXPECT_TRUE(v.isObject());
    EXPECT_EQ(v.size(), 3U);
    EXPECT_EQ(v.at("name").asString(), "Alice");
    EXPECT_DOUBLE_EQ(v.at("age").asNumber(), 30.0);
    EXPECT_TRUE(v.at("vip").asBool());
}

TEST(JsonParserTest, ParseSimpleArray) {
    JsonValue v = parseOk(R"([1, "two", false, null])");

    EXPECT_TRUE(v.isArray());
    EXPECT_EQ(v.size(), 4U);
    EXPECT_DOUBLE_EQ(v.at(0).asNumber(), 1.0);
    EXPECT_EQ(v.at(1).asString(), "two");
    EXPECT_FALSE(v.at(2).asBool());
    EXPECT_TRUE(v.at(3).isNull());
}

// ==================== 3. 嵌套结构 ====================

TEST(JsonParserTest, ParseNestedObject) {
    JsonValue v = parseOk(R"({"a": {"b": {"c": "deep"}}})");
    EXPECT_EQ(v.at("a").at("b").at("c").asString(), "deep");
}

TEST(JsonParserTest, ParseNestedArray) {
    JsonValue v = parseOk(R"([[1, 2], [3, [4, 5]]])");
    EXPECT_DOUBLE_EQ(v.at(1).at(1).at(0).asNumber(), 4.0);
}

TEST(JsonParserTest, ParseMixedStructure) {
    // 对象套数组套对象，经典混合结构
    JsonValue v = parseOk(R"({
        "users": [
            {"name": "Alice", "scores": [90, 85.5]},
            {"name": "Bob", "scores": []}
        ],
        "total": 2
    })");

    const JsonValue& users = v.at("users");
    EXPECT_EQ(users.size(), 2U);
    EXPECT_EQ(users.at(0).at("name").asString(), "Alice");
    EXPECT_DOUBLE_EQ(users.at(0).at("scores").at(1).asNumber(), 85.5);
    EXPECT_EQ(users.at(1).at("scores").size(), 0U);
    EXPECT_DOUBLE_EQ(v.at("total").asNumber(), 2.0);
}

TEST(JsonParserTest, ParseWhitespaceInsensitive) {
    // 空白、换行、缩进不影响解析结果
    JsonValue v = parseOk("  {\n\t\"a\" :\r\n 1\n}  ");
    EXPECT_DOUBLE_EQ(v.at("a").asNumber(), 1.0);
}

// ==================== 4. 重复 key ====================

TEST(JsonParserTest, DuplicateKeyLastWins) {
    // JSON 规范允许重复 key（虽不推荐），主流实现取最后出现的值
    JsonValue v = parseOk(R"({"a": 1, "a": 2})");
    EXPECT_EQ(v.size(), 1U);
    EXPECT_DOUBLE_EQ(v.at("a").asNumber(), 2.0);
}

// ==================== 5. 语法错误（应抛异常） ====================

TEST(JsonParserTest, RejectTrailingComma) {
    // 对象尾随逗号：{"a":1,} —— 逗号后必须是字符串 key
    EXPECT_THROW(parseOk(R"({"a":1,})"), JsonParseException);
    // 数组尾随逗号：[1,] —— 逗号后必须是 value，']' 不合法
    EXPECT_THROW(parseOk("[1,]"), JsonParseException);
}

TEST(JsonParserTest, RejectNonStringKey) {
    // 数字作 key（JSON 要求 key 必须是字符串）
    EXPECT_THROW(parseOk(R"({123: "x"})"), JsonParseException);
    // 布尔作 key
    EXPECT_THROW(parseOk("{true: 1}"), JsonParseException);
}

TEST(JsonParserTest, RejectMissingColon) {
    EXPECT_THROW(parseOk(R"({"a" 1})"), JsonParseException);
}

TEST(JsonParserTest, RejectMissingComma) {
    // 对象里两个键值对之间缺逗号
    EXPECT_THROW(parseOk(R"({"a":1 "b":2})"), JsonParseException);
    // 数组里两个元素之间缺逗号
    EXPECT_THROW(parseOk(R"([1 2])"), JsonParseException);
}

TEST(JsonParserTest, RejectUnclosedBracket) {
    EXPECT_THROW(parseOk(R"({"a":1)"), JsonParseException);
    EXPECT_THROW(parseOk("[1, 2"), JsonParseException);
    EXPECT_THROW(parseOk("{"), JsonParseException);
}

TEST(JsonParserTest, RejectMissingValue) {
    // 冒号后必须有 value
    EXPECT_THROW(parseOk(R"({"a":})"), JsonParseException);
    EXPECT_THROW(parseOk(R"({"a":)"), JsonParseException);
}

TEST(JsonParserTest, RejectContentAfterRoot) {
    // 顶层值解析完后必须正好 EOF
    EXPECT_THROW(parseOk("123 456"), JsonParseException);
    EXPECT_THROW(parseOk("{} {}"), JsonParseException);
}

TEST(JsonParserTest, RejectEmptyInput) {
    // 空输入 / 纯空白：无任何 value，直接报错
    EXPECT_THROW(parseOk(""), JsonParseException);
    EXPECT_THROW(parseOk("   \n\t "), JsonParseException);
}

TEST(JsonParserTest, RejectDeepNesting) {
    // 超过深度上限（kMaxDepth = 100）：101 层嵌套数组
    const std::string deep(101, '[');
    EXPECT_THROW(parseOk(deep), JsonParseException);

    // 恰好 100 层嵌套数组是合法的
    const std::string legal(100, '[');
    const std::string full = legal + std::string(100, ']');
    JsonValue v = parseOk(full);
    EXPECT_TRUE(v.isArray());
}

TEST(JsonParserTest, PropagateLexerError) {
    // Lexer 层面的错误（非法字符、未知关键字）会穿透 Parser 抛出，
    // 异常类型是 JsonLexerException 而非 JsonParseException
    EXPECT_THROW(parseOk("{\"a\": undefined}"), JsonLexerException);
    // 字母开头的多余内容同样在词法层就被拦截
    EXPECT_THROW(parseOk("{\"a\": 1} extra"), JsonLexerException);
    // 数字开头的多余内容能通过词法层，由 Parser 的 EOF 校验拦截
    EXPECT_THROW(parseOk("{\"a\": 1} 123"), JsonParseException);
}

// ==================== 6. 错误定位（行号/列号） ====================

TEST(JsonParserTest, ErrorCarriesLineAndColumn) {
    // 第 3 行的数字 key："  123: ..."，数字起始列是第 3 列
    JsonParseException e = parseBad("{\n  \"a\": 1,\n  123: \"x\"\n}");
    EXPECT_EQ(e.line(), 3U);
    EXPECT_EQ(e.column(), 3U);
}

TEST(JsonParserTest, ErrorAtEofPosition) {
    // '{"a":1' 共 6 个字符，EOF 位于第 1 行第 7 列
    JsonParseException e = parseBad("{\"a\":1");
    EXPECT_EQ(e.line(), 1U);
    EXPECT_EQ(e.column(), 7U);
}
