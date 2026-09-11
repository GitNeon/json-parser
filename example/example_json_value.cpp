// ============================================================
// 示例：JsonValue 的典型用法
// ------------------------------------------------------------
// 覆盖场景：
//   1. 构造六种 JSON 类型（null / bool / number / string / array / object）
//   2. 类型判断：isNull / isNumber / ... 以及 type() 枚举
//   3. 安全取值：asBool / asNumber / asString / asArray / asObject
//   4. 数组操作：pushBack / operator[](下标) / at(下标)
//   5. 对象操作：operator[](键) / at(键)
//   6. 嵌套结构：构建一个完整的 JSON 文档树
//   7. 通用工具：size / isEmpty / clear
//   8. 异常处理：类型不匹配、下标越界、键不存在
//
// 编译运行（在项目根目录执行）：
//   cmake --preset gcc-debug
//   cmake --build --preset gcc-debug --target example_json_value
//   ./build/gcc-debug/example/example_json_value
// ============================================================

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "exceptions.hpp"
#include "value.hpp"

namespace {

// 将 JsonType 枚举转成可读字符串，方便打印
const char* typeName(JsonType type) {
    switch (type) {
        case JsonType::Null:
            return "Null";
        case JsonType::Bool:
            return "Bool";
        case JsonType::Number:
            return "Number";
        case JsonType::String:
            return "String";
        case JsonType::Array:
            return "Array";
        case JsonType::Object:
            return "Object";
        default:
            return "Unknown";
    }
}

// ---------- 场景 1 & 2：构造六种类型并判断类型 ----------
void demoBasicTypes() {
    std::cout << "== 1. Basic types & type check ==\n";

    JsonValue nullVal;              // 默认构造即为 null
    JsonValue boolVal(true);        // 布尔
    JsonValue intVal(42);           // 整数（内部统一存为 double）
    JsonValue numVal(3.14);         // 浮点数
    JsonValue strVal("hello json"); // 字符串（支持 const char*）

    std::cout << "nullVal  : type=" << typeName(nullVal.type())
              << ", isNull=" << std::boolalpha << nullVal.isNull() << '\n';
    std::cout << "boolVal  : type=" << typeName(boolVal.type())
              << ", value=" << boolVal.asBool() << '\n';
    std::cout << "intVal   : type=" << typeName(intVal.type())
              << ", value=" << intVal.asNumber() << '\n';
    std::cout << "numVal   : value=" << numVal.asNumber() << '\n';
    std::cout << "strVal   : value=\"" << strVal.asString() << "\"\n";
}

// ---------- 场景 3 & 4：数组的构建与访问 ----------
void demoArray() {
    std::cout << "\n== 2. Array operations ==\n";

    // 方式一：先构造空数组，再逐个 pushBack
    JsonValue scores(std::vector<JsonValue>{});
    scores.pushBack(JsonValue(90));
    scores.pushBack(JsonValue(85.5));
    scores.pushBack(JsonValue(77));

    // 方式二：直接用 vector 初始化
    JsonValue tags(std::vector<JsonValue>{JsonValue("json"), JsonValue("cpp"), JsonValue(17)});

    std::cout << "scores size = " << scores.size() << '\n';

    // operator[]：不检查越界，速度优先
    std::cout << "scores[0] = " << scores[0].asNumber() << '\n';

    // at()：带越界检查，越界时抛 JsonValueException
    try {
        std::cout << "scores.at(10) = " << scores.at(10).asNumber() << '\n';
    } catch (const JsonValueException& e) {
        std::cout << "caught expected exception: " << e.what() << '\n';
    }

    // 遍历数组：asArray() 返回底层 vector 引用
    std::cout << "tags = [";
    for (const JsonValue& tag : tags.asArray()) {
        if (tag.isString()) {
            std::cout << tag.asString() << ' ';
        } else if (tag.isNumber()) {
            std::cout << tag.asNumber() << ' ';
        }
    }
    std::cout << "]\n";
}

// ---------- 场景 5 & 6：对象与嵌套结构 ----------
void demoObject() {
    std::cout << "\n== 3. Object & nesting ==\n";

    // operator[](key)：键不存在时自动创建 null 值，适合“写入”
    JsonValue person(std::unordered_map<std::string, JsonValue>{});
    person["name"] = JsonValue("Alice");
    person["age"] = JsonValue(30);
    person["isVip"] = JsonValue(true);

    // 嵌套数组：person.hobbies = ["reading", "coding"]
    person["hobbies"] = JsonValue(std::vector<JsonValue>{});
    person["hobbies"].pushBack(JsonValue("reading"));
    person["hobbies"].pushBack(JsonValue("coding"));

    // 嵌套对象：person.address = { "city": "Shanghai" }
    person["address"] = JsonValue(std::unordered_map<std::string, JsonValue>{});
    person["address"]["city"] = JsonValue("Shanghai");

    // at(key)：键不存在时抛异常，适合“只读”访问
    std::cout << "name    = " << person.at("name").asString() << '\n';
    std::cout << "age     = " << person.at("age").asNumber() << '\n';
    std::cout << "city    = " << person.at("address").at("city").asString() << '\n';
    std::cout << "hobby[1]= " << person.at("hobbies").at(1).asString() << '\n';
    std::cout << "key count = " << person.size() << '\n';

    // 遍历对象的所有键值对
    std::cout << "all keys: ";
    for (const auto& [key, value] : person.asObject()) {
        std::cout << key << "(" << typeName(value.type()) << ") ";
    }
    std::cout << '\n';

    // 读取不存在的键 -> 抛 JsonValueException
    try {
        std::cout << person.at("not_exist").asString() << '\n';
    } catch (const JsonValueException& e) {
        std::cout << "caught expected exception: " << e.what() << '\n';
    }
}

// ---------- 场景 7：size / isEmpty / clear ----------
void demoUtilities() {
    std::cout << "\n== 4. size / isEmpty / clear ==\n";

    JsonValue emptyObj(std::unordered_map<std::string, JsonValue>{});
    JsonValue strVal("abc");
    JsonValue numVal(1.0);

    std::cout << "emptyObj isEmpty = " << std::boolalpha << emptyObj.isEmpty() << '\n';
    std::cout << "strVal   size    = " << strVal.size() << '\n'; // 字符串长度为 3
    std::cout << "numVal   size    = " << numVal.size() << '\n'; // 数字没有长度，返回 0

    strVal.clear(); // 重置为 null
    std::cout << "after clear, strVal isNull = " << strVal.isNull() << '\n';
}

// ---------- 场景 8：异常处理（类型不匹配） ----------
void demoExceptions() {
    std::cout << "\n== 5. Type-mismatch exception ==\n";

    JsonValue numVal(123);
    try {
        // 对 Number 调用 asString() -> 类型不匹配
        std::cout << numVal.asString() << '\n';
    } catch (const JsonValueException& e) {
        std::cout << "caught JsonValueException: " << e.what() << '\n';
    }

    // 也可以只捕获基类 JsonException，统一兜底库内所有异常
    try {
        numVal.asArray();
    } catch (const JsonException& e) {
        std::cout << "caught JsonException (base): " << e.what() << '\n';
    }
}

}  // namespace

int main() {
    demoBasicTypes();
    demoArray();
    demoObject();
    demoUtilities();
    demoExceptions();

    std::cout << "\nAll JsonValue demos finished.\n";
    return 0;
}
