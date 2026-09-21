#include <gtest/gtest.h>
#include "parser.hpp"
#include "serializer.hpp"

#include <limits>
#include <string>
#include <utility>
#include <vector>

/*
JsonSerializer 测试组织（均为 JsonSerializerTest 套件）：
1. 基础类型 dump：null / bool / number（整数、负数、小数、极值）
2. 字符串转义：\" \\ \b \f \n \r \t 及其余控制字符 \u00XX
3. 容器：空对象、空数组、数组、对象、多层嵌套
4. dumpPretty：精确匹配缩进格式（空容器保持内联、每层换行）
5. 回环测试：parse -> dump -> 再 parse -> 再 dump，两次输出完全一致
6. 异常与降级：NaN/Infinity 输出 null、嵌套超深抛 JsonException
*/

namespace {

// 解析辅助：把 JSON 文本解析成 JsonValue，简化用例书写
JsonValue parseOk(const std::string& src) {
    return JsonParser(src).parse();
}

// 回环校验（仅适用于键序稳定的输入：标量、数组、单键对象）：
// parse -> dump -> parse -> dump，两次 dump 结果必须逐字节一致
std::string roundTrip(const std::string& src) {
    const std::string first = JsonSerializer::dump(parseOk(src));
    const std::string second = JsonSerializer::dump(parseOk(first));
    EXPECT_EQ(first, second);
    return first;
}

// 递归结构相等：多键对象受 unordered_map 键序影响，dump 字符串的
// 逐字节比较不是有效断言，回环正确性应按"结构等价"校验
bool jsonEqual(const JsonValue& a, const JsonValue& b) {
    if (a.type() != b.type()) {
        return false;
    }
    switch (a.type()) {
        case JsonType::Null:
            return true;
        case JsonType::Bool:
            return a.asBool() == b.asBool();
        case JsonType::Number:
            return a.asNumber() == b.asNumber();
        case JsonType::String:
            return a.asString() == b.asString();
        case JsonType::Array: {
            if (a.size() != b.size()) {
                return false;
            }
            for (size_t i = 0; i < a.size(); ++i) {
                if (!jsonEqual(a.at(i), b.at(i))) {
                    return false;
                }
            }
            return true;
        }
        case JsonType::Object: {
            if (a.size() != b.size()) {
                return false;
            }
            for (const auto& [key, val] : a.asObject()) {
                const auto it = b.asObject().find(key);
                if (it == b.asObject().end() || !jsonEqual(val, it->second)) {
                    return false;
                }
            }
            return true;
        }
    }
    return false;
}

// 构造 N 层嵌套数组：内层是一个 null，外面套 N 层 []
JsonValue makeNestedArrays(int levels) {
    JsonValue v = JsonValue(nullptr);
    for (int i = 0; i < levels; ++i) {
        v = JsonValue(std::vector<JsonValue>{std::move(v)});
    }
    return v;
}

}  // namespace

// ==================== 1. 基础类型 dump ====================

TEST(JsonSerializerTest, DumpNull) {
    EXPECT_EQ(JsonSerializer::dump(parseOk("null")), "null");
    EXPECT_EQ(JsonSerializer::dump(JsonValue()), "null");
}

TEST(JsonSerializerTest, DumpBooleans) {
    EXPECT_EQ(JsonSerializer::dump(parseOk("true")), "true");
    EXPECT_EQ(JsonSerializer::dump(parseOk("false")), "false");
}

TEST(JsonSerializerTest, DumpIntegerNumbers) {
    // 整数值不允许出现小数点与多余的小数位
    EXPECT_EQ(JsonSerializer::dump(parseOk("0")), "0");
    EXPECT_EQ(JsonSerializer::dump(parseOk("42")), "42");
    EXPECT_EQ(JsonSerializer::dump(parseOk("-7")), "-7");
}

TEST(JsonSerializerTest, DumpFloatNumbers) {
    EXPECT_EQ(JsonSerializer::dump(parseOk("3.14")), "3.14");
    EXPECT_EQ(JsonSerializer::dump(parseOk("-1.5")), "-1.5");
    EXPECT_EQ(JsonSerializer::dump(parseOk("0.1")), "0.1");
    // 指数形式：取能无损往返的最短表示
    EXPECT_EQ(JsonSerializer::dump(parseOk("1e-10")), "1e-10");
    // 2^53 边界：整数值超过直接整数输出范围，走浮点最短表示路径
    EXPECT_EQ(JsonSerializer::dump(parseOk("9007199254740992")), "9007199254740992");
}

TEST(JsonSerializerTest, DumpTopLevelScalars) {
    // JSON 合法顶层可以是任意基础类型
    EXPECT_EQ(JsonSerializer::dump(parseOk("\"test\"")), "\"test\"");
    EXPECT_EQ(JsonSerializer::dump(parseOk("123")), "123");
}

// ==================== 2. 字符串转义 ====================

TEST(JsonSerializerTest, DumpEscapesShortForms) {
    // 手工构造包含全部 7 种短转义字符的字符串
    JsonValue v(std::string("a\"b\\c\nd\te\rf\bg\fh"));
    EXPECT_EQ(JsonSerializer::dump(v), "\"a\\\"b\\\\c\\nd\\te\\rf\\bg\\fh\"");
}

TEST(JsonSerializerTest, DumpEscapesOtherControlChars) {
    // 没有短转义形式的控制字符统一输出 \u00XX
    JsonValue v(std::string("\x01"));
    EXPECT_EQ(JsonSerializer::dump(v), "\"\\u0001\"");
}

TEST(JsonSerializerTest, DumpKeepsUtf8Raw) {
    // UTF-8 多字节字符（>= 0x80）按原样透传，不做转义
    JsonValue v(std::string("中文"));
    EXPECT_EQ(JsonSerializer::dump(v), "\"中文\"");
}

// ==================== 3. 容器 ====================

TEST(JsonSerializerTest, DumpEmptyContainers) {
    EXPECT_EQ(JsonSerializer::dump(parseOk("{}")), "{}");
    EXPECT_EQ(JsonSerializer::dump(parseOk("[]")), "[]");
}

TEST(JsonSerializerTest, DumpArray) {
    EXPECT_EQ(JsonSerializer::dump(parseOk("[1,2,3]")), "[1,2,3]");
    EXPECT_EQ(JsonSerializer::dump(parseOk("[1,true,null,\"x\"]")),
              "[1,true,null,\"x\"]");
    EXPECT_EQ(JsonSerializer::dump(parseOk("[[1],[[2]]]")), "[[1],[[2]]]");
}

TEST(JsonSerializerTest, DumpObjectAndRoundParse) {
    // unordered_map 键序不确定，不能做整串精确匹配，
    // 改为 dump 后重新解析，校验内容完整不丢失
    const std::string dumped = JsonSerializer::dump(parseOk(R"({"a":1,"b":"x","c":false})"));
    JsonValue back = parseOk(dumped);

    EXPECT_EQ(back.at("a").asNumber(), 1.0);
    EXPECT_EQ(back.at("b").asString(), "x");
    EXPECT_EQ(back.at("c").asBool(), false);
    EXPECT_EQ(back.size(), 3u);
}

TEST(JsonSerializerTest, DumpNestedStructure) {
    // 数组顺序确定，可用数组包对象做精确匹配
    EXPECT_EQ(JsonSerializer::dump(parseOk("[{\"a\":[1,{\"b\":null}]}]")),
              "[{\"a\":[1,{\"b\":null}]}]");
}

// ==================== 4. dumpPretty 格式化 ====================

TEST(JsonSerializerTest, PrettyEmptyContainersStayInline) {
    // 空对象/空数组在 pretty 模式下也保持 {} / [] 内联，不产生空行
    EXPECT_EQ(JsonSerializer::dumpPretty(parseOk("{}"), 2), "{}");
    EXPECT_EQ(JsonSerializer::dumpPretty(parseOk("[]"), 2), "[]");
}

TEST(JsonSerializerTest, PrettyArrayExactFormat) {
    EXPECT_EQ(JsonSerializer::dumpPretty(parseOk("[1,[2,3],{\"x\":null}]"), 2),
              "[\n"
              "  1,\n"
              "  [\n"
              "    2,\n"
              "    3\n"
              "  ],\n"
              "  {\n"
              "    \"x\": null\n"
              "  }\n"
              "]");
}

TEST(JsonSerializerTest, PrettyObjectExactFormat) {
    // 单键对象不受 unordered_map 键序影响，可做精确匹配
    EXPECT_EQ(JsonSerializer::dumpPretty(parseOk("{\"key\":[1,2]}"), 4),
              "{\n"
              "    \"key\": [\n"
              "        1,\n"
              "        2\n"
              "    ]\n"
              "}");
}

TEST(JsonSerializerTest, PrettyDegeneratesToCompact) {
    // 缩进步长 <= 0 时退化为紧凑模式
    const JsonValue v = parseOk("[1,{\"a\":2}]");
    EXPECT_EQ(JsonSerializer::dumpPretty(v, 0), JsonSerializer::dump(v));
    EXPECT_EQ(JsonSerializer::dumpPretty(v, -1), JsonSerializer::dump(v));
}

// ==================== 5. 回环测试 ====================

TEST(JsonSerializerTest, RoundTripBasicTypes) {
    EXPECT_EQ(roundTrip("null"), "null");
    EXPECT_EQ(roundTrip("true"), "true");
    EXPECT_EQ(roundTrip("123"), "123");
    EXPECT_EQ(roundTrip("-3.25"), "-3.25");
    EXPECT_EQ(roundTrip("\"hello\""), "\"hello\"");
}

TEST(JsonSerializerTest, RoundTripEscapedString) {
    // 覆盖 Lexer 可解码的全部短转义（\uXXXX 在本项目按字面量保留，不参与回环）
    EXPECT_EQ(roundTrip("\"a\\\"b\\\\c\\nd\\te\\rf\\bg\\fh\""),
              "\"a\\\"b\\\\c\\nd\\te\\rf\\bg\\fh\"");
}

TEST(JsonSerializerTest, RoundTripNestedStructure) {
    const std::string src =
        "{\"name\":\"json\",\"nums\":[1,-2.5,1e-3],\"flag\":true,"
        "\"inner\":{\"list\":[[1],[{}]],\"nil\":null}}";
    // 多键对象键序不稳定，改用结构等价校验回环：源结构 == dump 后再 parse 的结构
    const JsonValue origin = parseOk(src);
    const JsonValue back = parseOk(JsonSerializer::dump(origin));
    EXPECT_TRUE(jsonEqual(origin, back));

    // 再抽查关键字段，双重确认内容无丢失
    EXPECT_EQ(back.at("name").asString(), "json");
    EXPECT_EQ(back.at("nums").size(), 3u);
    EXPECT_DOUBLE_EQ(back.at("nums").at(1).asNumber(), -2.5);
    EXPECT_TRUE(back.at("flag").asBool());
    EXPECT_TRUE(back.at("inner").at("nil").isNull());
}

TEST(JsonSerializerTest, RoundTripTopLevelArray) {
    // JSON 合法根节点可以是数组
    EXPECT_EQ(roundTrip("[1,[2,[3,[null]]]]"), "[1,[2,[3,[null]]]]");
}

// ==================== 6. 异常与降级 ====================

TEST(JsonSerializerTest, NonFiniteNumbersDegradeToNull) {
    // NaN / Infinity 无法用 JSON 表示，统一降级为 null 保证输出合法
    EXPECT_EQ(JsonSerializer::dump(JsonValue(std::numeric_limits<double>::quiet_NaN())),
              "null");
    EXPECT_EQ(JsonSerializer::dump(JsonValue(std::numeric_limits<double>::infinity())),
              "null");
    EXPECT_EQ(JsonSerializer::dump(JsonValue(-std::numeric_limits<double>::infinity())),
              "null");
}

TEST(JsonSerializerTest, DepthLimitAllowsMaxNesting) {
    // 100 层嵌套在保护上限之内，正常输出
    std::string expected(100, '[');
    expected += "null";
    expected.append(100, ']');
    EXPECT_EQ(JsonSerializer::dump(makeNestedArrays(100)), expected);
}

TEST(JsonSerializerTest, DepthLimitThrowsBeyondMaxNesting) {
    // 超过上限抛 JsonException，防止递归序列化栈溢出
    // （(void) 显式丢弃返回值：EXPECT_THROW 宏内部无法消费 [[nodiscard]]）
    EXPECT_THROW((void)JsonSerializer::dump(makeNestedArrays(101)), JsonException);
    EXPECT_THROW((void)JsonSerializer::dumpPretty(makeNestedArrays(101), 4), JsonException);
}
