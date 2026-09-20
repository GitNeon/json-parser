// ============================================================
// 示例：JsonParser 的典型用法
// ------------------------------------------------------------
// JsonParser 组合 JsonLexer，把 JSON 文本递归下降解析成 JsonValue 树，
// 之后即可用 JsonValue 提供的取值接口任意访问嵌套数据。
//
// 覆盖场景：
//   1. 解析对象/数组：parse() 一次生成整棵树
//   2. 嵌套取值：at(key) / at(index) 逐层深入
//   3. 顶层基础类型：JSON 合法根节点也可以是数字、字符串、true 等
//   4. 修改节点：解析后直接改树中的值（可写访问）
//   5. 错误处理：语法错误抛出 JsonParseException，带行号/列号
//
// 编译运行（在项目根目录执行）：
//   cmake --preset clang-debug
//   cmake --build --preset clang-debug --target example_json_parser
//   ./build/clang-debug/example/example_json_parser
// ============================================================

#include <iostream>
#include <string>

#include "exceptions.hpp"
#include "parser.hpp"
#include "value.hpp"

namespace {

// ---------- 场景 1 & 2：解析对象并逐层访问嵌套数据 ----------
void demoParseObject() {
    std::cout << "== 1. Parse object and access nested values ==\n";

    const std::string json = R"({
        "name": "Alice",
        "age": 30,
        "isVip": true,
        "address": {"city": "Beijing", "zip": "100000"},
        "hobbies": ["reading", "coding", null]
    })";

    // parse() 成功返回整棵树；失败抛 JsonParseException
    JsonValue root = JsonParser(json).parse();

    // 基础类型取值：at("key") 返回 JsonValue 引用，再按类型转
    std::cout << "name    = " << root.at("name").asString() << '\n';
    std::cout << "age     = " << root.at("age").asNumber() << '\n';
    std::cout << "isVip   = " << (root.at("isVip").asBool() ? "true" : "false") << '\n';

    // 嵌套对象：链式 at() 一路深入
    std::cout << "city    = " << root.at("address").at("city").asString() << '\n';

    // 嵌套数组：at(index) 按下标访问
    const JsonValue& hobbies = root.at("hobbies");
    std::cout << "hobbies[1] = " << hobbies.at(1).asString() << '\n';
    std::cout << "hobbies[2] is null? " << (hobbies.at(2).isNull() ? "yes" : "no") << '\n';

    // 类型判断接口：先判类型再取值是安全的使用姿势
    std::cout << "root is object? " << (root.isObject() ? "yes" : "no") << '\n';
    std::cout << "hobbies size = " << hobbies.size() << '\n';
}

// ---------- 场景 3：顶层是数组 / 基础类型也合法 ----------
void demoParseOtherRoots() {
    std::cout << "\n== 2. Top-level array and primitive ==\n";

    // 顶层是数组：对数组元素做求和
    JsonValue arr = JsonParser("[1, 2.5, -3, 4]").parse();
    double sum = 0.0;
    for (size_t i = 0; i < arr.size(); ++i) {
        sum += arr.at(i).asNumber();
    }
    std::cout << "sum of [1, 2.5, -3, 4] = " << sum << '\n';

    // 顶层是基础类型：数字、字符串、true、null 都是合法 JSON
    JsonValue num = JsonParser("3.14").parse();
    std::cout << "3.14 is number? " << (num.isNumber() ? "yes" : "no") << '\n';

    JsonValue str = JsonParser(R"("hello")").parse();
    std::cout << R"("hello" -> )"<< str.asString() << '\n';

    JsonValue nullValue = JsonParser("null").parse();
    std::cout << "null is null? " << (nullValue.isNull() ? "yes" : "no") << '\n';
}

// ---------- 场景 4：解析后修改树中的节点 ----------
void demoModifyTree() {
    std::cout << "\n== 3. Modify value after parsing ==\n";

    JsonValue root = JsonParser(R"({"count": 1, "tags": ["a"]})").parse();

    // asNumber() 返回 double 引用，直接赋值即完成修改
    root.at("count").asNumber() = 42;

    // 数组追加元素：pushBack 接收 JsonValue（自动构造）
    root.at("tags").pushBack(JsonValue("b"));

    // 对象新增键值对：operator[] 不存在则自动创建
    root["newKey"] = JsonValue("newValue");

    std::cout << "count = " << root.at("count").asNumber() << '\n';
    std::cout << "tags.size = " << root.at("tags").size() << '\n';
    std::cout << "newKey = " << root.at("newKey").asString() << '\n';
}

// ---------- 场景 5：语法错误的精确定位 ----------
void demoErrorHandling() {
    std::cout << "\n== 4. Error handling ==\n";

    // 错误用例：key 不是字符串 + 尾随逗号
    const std::string broken = "{\n  \"a\": 1,\n  123: \"x\"\n}";

    try {
        // 本例预期解析失败；丢弃返回值规避 [[nodiscard]] 警告
        JsonValue ignored = JsonParser(broken).parse();
        (void)ignored;
    } catch (const JsonParseException& e) {
        // what() 已含完整信息；line()/column() 可程序化获取位置
        std::cout << "error: " << e.what() << '\n';
        std::cout << "at line " << e.line() << ", column " << e.column() << '\n';
    }

    // 所有 JSON 库异常的公共基类是 JsonException，统一兜底即可
    try {
        JsonValue ignored = JsonParser("[1, 2,").parse();  // 括号不闭合
        (void)ignored;
    } catch (const JsonException& e) {
        std::cout << "error: " << e.what() << '\n';
    }
}

}  // namespace

int main() {
    demoParseObject();
    demoParseOtherRoots();
    demoModifyTree();
    demoErrorHandling();
    return 0;
}
