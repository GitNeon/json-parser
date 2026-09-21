// ============================================================
// 示例：JsonSerializer 的典型用法
// ------------------------------------------------------------
// JsonSerializer 把 JsonValue 树反向输出为 JSON 文本，与 JsonParser
// 互为逆操作：Parser 负责"文本 -> 树"，Serializer 负责"树 -> 文本"。
//
// 覆盖场景：
//   1. 紧凑输出：dump() 无空白无换行，适合网络传输/落盘存储
//   2. 格式化输出：dumpPretty() 逐层缩进，适合日志与调试打印
//   3. 手工构造 + 序列化：不经过 Parser，直接建树再导出
//   4. 字符串转义与数字格式：转义规则与最短无损浮点表示
//   5. 回环校验：parse -> dump -> 再 parse，结构完全一致
//   6. 边界行为：空容器内联、NaN/Infinity 降级、嵌套超深报错
//
// 编译运行（在项目根目录执行）：
//   cmake --preset clang-debug
//   cmake --build --preset clang-debug --target example_json_serializer
//   ./build/clang-debug/example/example_json_serializer
// ============================================================

#include <iostream>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "exceptions.hpp"
#include "parser.hpp"
#include "serializer.hpp"
#include "value.hpp"

namespace {

// ---------- 场景 1 & 2：同一段数据的两种输出模式 ----------
void demoTwoOutputModes() {
    std::cout << "== 1. Compact vs pretty output ==\n";

    const std::string json = R"({
        "name": "Alice",
        "age": 30,
        "scores": [95.5, 88, 76.25],
        "tags": {"vip": true, "note": null},
        "emptyList": []
    })";

    JsonValue root = JsonParser(json).parse();

    // 紧凑模式：单行无空白，体积最小
    std::cout << "dump():\n" << JsonSerializer::dump(root) << "\n\n";

    // 格式化模式：第二个参数是每层缩进的空格数（缺省 4）
    std::cout << "dumpPretty(value, 2):\n" << JsonSerializer::dumpPretty(root, 2) << "\n\n";

    std::cout << "dumpPretty(value):  # 默认 4 空格\n"
              << JsonSerializer::dumpPretty(root) << '\n';
}

// ---------- 场景 3：手工构造 JsonValue 树再序列化 ----------
void demoManualBuild() {
    std::cout << "\n== 2. Build tree manually and serialize ==\n";

    // 从零构造：{"msg": "hello", "nums": [1, 2, 3]}
    JsonValue root = JsonValue(std::unordered_map<std::string, JsonValue>{});
    root["msg"] = JsonValue("hello");

    JsonValue nums = JsonValue(std::vector<JsonValue>{});
    nums.pushBack(JsonValue(1));
    nums.pushBack(JsonValue(2));
    nums.pushBack(JsonValue(3));
    root["nums"] = std::move(nums);

    // unordered_map 键序不确定，这里只演示输出形态
    std::cout << JsonSerializer::dumpPretty(root) << '\n';
}

// ---------- 场景 4：字符串转义与数字格式规则 ----------
void demoEscapesAndNumbers() {
    std::cout << "\n== 3. String escapes and number formatting ==\n";

    // 字符串反向转义：双引号、反斜杠、控制字符都还原成合法 JSON 转义序列
    JsonValue tricky = JsonValue(std::string("line1\nline2\ttab \"quoted\" \\end"));
    std::cout << "escaped: " << JsonSerializer::dump(tricky) << '\n';

    // 数字：整数不带小数点，浮点取最短无损表示
    JsonValue num1 = JsonValue(42);
    JsonValue num2 = JsonValue(-7);
    JsonValue num3 = JsonValue(3.14);
    JsonValue num4 = JsonValue(0.1);
    std::cout << "int 42    -> " << JsonSerializer::dump(num1) << '\n';
    std::cout << "int -7    -> " << JsonSerializer::dump(num2) << '\n';
    std::cout << "double pi -> " << JsonSerializer::dump(num3) << '\n';
    std::cout << "double0.1 -> " << JsonSerializer::dump(num4) << '\n';
}

// ---------- 场景 5：回环校验（parse -> dump -> 再 parse） ----------
void demoRoundTrip() {
    std::cout << "\n== 4. Round trip: parse -> dump -> parse ==\n";

    const std::string src = R"({"user":"Bob","active":true,"score":98.5,"friends":["Carol","Dave"]})";

    const std::string dumped = JsonSerializer::dump(JsonParser(src).parse());
    JsonValue back = JsonParser(dumped).parse();

    // 两次转换后内容保持一致
    std::cout << "first dump : " << dumped << '\n';
    std::cout << "user       = " << back.at("user").asString() << '\n';
    std::cout << "score      = " << back.at("score").asNumber() << '\n';
    std::cout << "friends[0] = " << back.at("friends").at(0).asString() << '\n';
}

// ---------- 场景 6：边界行为 ----------
void demoEdgeCases() {
    std::cout << "\n== 5. Edge cases ==\n";

    // 空容器在 pretty 模式下也保持内联输出
    std::cout << "empty object: " << JsonSerializer::dumpPretty(JsonParser("{}").parse(), 2)
              << '\n';
    std::cout << "empty array : " << JsonSerializer::dumpPretty(JsonParser("[]").parse(), 2)
              << '\n';

    // NaN / Infinity 无法用 JSON 表示，降级为 null 保证输出合法
    JsonValue notANumber = JsonValue(std::numeric_limits<double>::quiet_NaN());
    std::cout << "NaN         -> " << JsonSerializer::dump(notANumber) << '\n';

    // 嵌套超过深度上限（100 层）时抛 JsonException，防止递归栈溢出
    JsonValue deep = JsonValue(nullptr);
    for (int i = 0; i < 101; ++i) {
        deep = JsonValue(std::vector<JsonValue>{std::move(deep)});
    }
    try {
        const std::string ignored = JsonSerializer::dump(deep);  // 预期抛异常
        (void)ignored;
    } catch (const JsonException& e) {
        std::cout << "deep nesting: " << e.what() << '\n';
    }
}

}  // namespace

int main() {
    demoTwoOutputModes();
    demoManualBuild();
    demoEscapesAndNumbers();
    demoRoundTrip();
    demoEdgeCases();
    return 0;
}
