#pragma once

#include <cstdint>
#include <stdexcept>
#include <unordered_map>
#include <variant>
#include <vector>

#include "common.hpp"

// 自定义异常
class JsonValueException : public std::runtime_error {
public:
    explicit JsonValueException(const std::string& msg) : std::runtime_error(msg) {}
};

// 枚举定义：JSON的六种数据类型
enum class JsonType : std::uint8_t {
    Null,    // 空值 null
    Bool,    // 布尔值 true/false
    Number,  // 数字（统一用 double 存储）
    String,  // 字符串
    Array,   // 数组
    Object   // 对象（键值对）
};

// JSON 统一值类：用一个类承载所有6种JSON数据类型
class JsonValue {
public:
    // ===================== 类型定义 =====================
    typedef std::variant<std::monostate,                             // 索引0 -> Null
                         bool,                                       // 索引1 -> Bool
                         double,                                     // 索引2 -> Number
                         std::string,                                // 索引3 -> String
                         std::vector<JsonValue>,                     // 索引4 -> Array（递归嵌套）
                         std::unordered_map<std::string, JsonValue>  // 索引5 -> Object（递归嵌套）
                         >
        StorageType;

    // ===================== 构造函数 =====================
    // 默认构造，生成null类型
    JsonValue();

    // 显式构造null类型，语义更清晰
    explicit JsonValue(std::nullptr_t);

    // 其他各种数据类型的构造函数
    explicit JsonValue(bool value);

    explicit JsonValue(int value);

    explicit JsonValue(double value);

    explicit JsonValue(std::string value);

    explicit JsonValue(const char* value);

    explicit JsonValue(std::vector<JsonValue> value);

    explicit JsonValue(std::unordered_map<std::string, JsonValue>);

    // 显式声明五大函数：移动构造、拷贝构造、移动赋值、拷贝赋值、析构函数
    // 这里全部采用默认机制，C++内置数据类型 + variant 自带深拷贝和移动语义，无需自己实现
    JsonValue(const JsonValue&) = default;
    JsonValue(JsonValue&&) noexcept = default;
    JsonValue& operator=(const JsonValue&) = default;
    JsonValue& operator=(JsonValue&&) noexcept = default;
    ~JsonValue() = default;

    // ===================== 类型判断 =====================
    // 获取当前节点的类型枚举
    MUST_USE JsonType type() const noexcept;

    // 快速判断当前类型
    MUST_USE bool isNull() const noexcept;
    MUST_USE bool isBool() const noexcept;
    MUST_USE bool isNumber() const noexcept;
    MUST_USE bool isString() const noexcept;
    MUST_USE bool isArray() const noexcept;
    MUST_USE bool isObject() const noexcept;

    // ===================== 安全取值接口 =====================
    bool& asBool();
    MUST_USE const bool& asBool() const;

    double& asNumber();
    MUST_USE const double& asNumber() const;

    std::string& asString();
    MUST_USE const std::string& asString() const;

    std::vector<JsonValue>& asArray();
    MUST_USE const std::vector<JsonValue>& asArray() const;

    std::unordered_map<std::string, JsonValue>& asObject();
    MUST_USE const std::unordered_map<std::string, JsonValue>& asObject() const;

    // ===================== 数组专属操作 =====================
    // 按下标访问，常规方式不检查越界行为
    JsonValue& operator[](size_t index);
    const JsonValue& operator[](size_t index) const;

    // 带越界检查的访问
    JsonValue& at(size_t index);
    MUST_USE const JsonValue& at(size_t index) const;

    // 尾部添加元素
    void pushBack(JsonValue value);

    // ===================== 对象专属操作 =====================
    // 按键访问，key不存在则自动创建，适合新添加一个值
    JsonValue& operator[](const std::string& key);

    // 按键访问对象的值，不存在则抛异常
    JsonValue& at(const std::string& key);
    MUST_USE const JsonValue& at(const std::string& key) const;

    // ===================== 通用工具方法 =====================
    // 针对数组/对象返回长度，其他类型长度为0
    MUST_USE size_t size() const;

    // 判断是否为空（null/空串/空数组/空对象 都算空）
    MUST_USE bool isEmpty() const;

    // 清空，都为null
    void clear();

    // ===================== visit访问接口 =====================
    // 封装std::visit，为 m_data 提供统一的访问方式
    template <typename Visitor>
    decltype(auto) visit(Visitor&& vis) {
        return std::visit(std::forward<Visitor>(vis), m_data);
    }

    template <typename Visitor>
    decltype(auto) visit(Visitor&& vis) const {
        return std::visit(std::forward<Visitor>(vis), m_data);
    }

private:
    StorageType m_data;
};

// ===================== inline functions =====================
// 各种构造函数
inline JsonValue::JsonValue() : m_data(std::monostate{}) {}

inline JsonValue::JsonValue(std::nullptr_t) : m_data(std::monostate{}) {}

inline JsonValue::JsonValue(bool value) : m_data(value) {}

inline JsonValue::JsonValue(int value) : m_data(static_cast<double>(value)) {}

inline JsonValue::JsonValue(double value) : m_data(value) {}

inline JsonValue::JsonValue(std::string value) : m_data(std::move(value)) {}

inline JsonValue::JsonValue(const char* value) : m_data(std::string(value)) {}

inline JsonValue::JsonValue(std::vector<JsonValue> value) : m_data(std::move(value)) {}

inline JsonValue::JsonValue(std::unordered_map<std::string, JsonValue> value)
    : m_data(std::move(value)) {}

// 类型查询
inline JsonType JsonValue::type() const noexcept {
    // 通过 variant 的索引判断当前存储的类型
    switch (m_data.index()) {
        case 0:
            return JsonType::Null;
        case 1:
            return JsonType::Bool;
        case 2:
            return JsonType::Number;
        case 3:
            return JsonType::String;
        case 4:
            return JsonType::Array;
        case 5:
            return JsonType::Object;
        default:
            return JsonType::Null;
    }
}

// 类型判断
inline bool JsonValue::isNull() const noexcept {
    return std::holds_alternative<std::monostate>(m_data);
}

inline bool JsonValue::isBool() const noexcept {
    return std::holds_alternative<bool>(m_data);
}

inline bool JsonValue::isNumber() const noexcept {
    return std::holds_alternative<double>(m_data);
}

inline bool JsonValue::isString() const noexcept {
    return std::holds_alternative<std::string>(m_data);
}

inline bool JsonValue::isArray() const noexcept {
    return std::holds_alternative<std::vector<JsonValue>>(m_data);
}

inline bool JsonValue::isObject() const noexcept {
    return std::holds_alternative<std::unordered_map<std::string, JsonValue>>(m_data);
}

// 取值
inline bool& JsonValue::asBool() {
    if (!isBool()) {
        throw JsonValueException("类型不匹配，期望 Bool 类型");
    }
    return std::get<bool>(m_data);
}

inline const bool& JsonValue::asBool() const {
    if (!isBool()) {
        throw JsonValueException("类型不匹配，期望 Bool 类型");
    }
    return std::get<bool>(m_data);
}

inline double& JsonValue::asNumber() {
    if (!isNumber()) {
        throw JsonValueException("类型不匹配，期望 Number 类型");
    }
    return std::get<double>(m_data);
}

inline const double& JsonValue::asNumber() const {
    if (!isNumber()) {
        throw JsonValueException("类型不匹配，期望 Number 类型");
    }
    return std::get<double>(m_data);
}

inline std::string& JsonValue::asString() {
    if (!isString()) {
        throw JsonValueException("类型不匹配，期望 String 类型");
    }
    return std::get<std::string>(m_data);
}

// 补充到 asString() 非 const 实现之后
inline const std::string& JsonValue::asString() const {
    if (!isString()) {
        throw JsonValueException("类型不匹配，期望 String 类型");
    }
    return std::get<std::string>(m_data);
}

inline std::vector<JsonValue>& JsonValue::asArray() {
    if (!isArray()) {
        throw JsonValueException("类型不匹配，期望 Array 类型");
    }
    return std::get<std::vector<JsonValue>>(m_data);
}

inline const std::vector<JsonValue>& JsonValue::asArray() const {
    if (!isArray()) {
        throw JsonValueException("类型不匹配，期望 Array 类型");
    }
    return std::get<std::vector<JsonValue>>(m_data);
}

inline std::unordered_map<std::string, JsonValue>& JsonValue::asObject() {
    if (!isObject()) {
        throw JsonValueException("类型不匹配，期望 Object 类型");
    }
    return std::get<std::unordered_map<std::string, JsonValue>>(m_data);
}

inline const std::unordered_map<std::string, JsonValue>& JsonValue::asObject() const {
    if (!isObject()) {
        throw JsonValueException("类型不匹配，期望 Object 类型");
    }
    return std::get<std::unordered_map<std::string, JsonValue>>(m_data);
}

// 数组操作
inline JsonValue& JsonValue::operator[](size_t index) {
    return asArray()[index];
}

inline const JsonValue& JsonValue::operator[](size_t index) const {
    return asArray()[index];
}

inline JsonValue& JsonValue::at(size_t index) {
    std::vector<JsonValue>& arr = asArray();

    if (index >= arr.size()) {
        throw JsonValueException("数组下标越界");
    }
    return asArray()[index];
}

inline const JsonValue& JsonValue::at(size_t index) const {
    const auto& arr = asArray();
    if (index >= arr.size()) {
        throw JsonValueException("数组下标越界");
    }
    return arr[index];
}

inline void JsonValue::pushBack(JsonValue value) {
    std::vector<JsonValue>& arr = asArray();
    arr.push_back(std::move(value));
}

// 对象操作
inline JsonValue& JsonValue::operator[](const std::string& key) {
    auto& obj = asObject();
    return obj[key];
}

inline JsonValue& JsonValue::at(const std::string& key) {
    auto& obj = asObject();
    auto iter = obj.find(key);
    if (iter == obj.end()) {
        throw JsonValueException("对象中键不存在：" + key);
    }
    return iter->second;
}

inline const JsonValue& JsonValue::at(const std::string& key) const {
    const auto& obj = asObject();
    auto iter = obj.find(key);
    if (iter == obj.end()) {
        throw JsonValueException("对象中键不存在：" + key);
    }
    return iter->second;
}

// 获取 字符串，数组，map 长度
inline size_t JsonValue::size() const {
    // 比较高级的写法：
    // return std::visit(
    //     [](auto&& val) -> size_t {
    //         using T = std::decay_t<decltype(val)>;
    //         if constexpr (std::is_same_v<T, std::vector<JsonValue>> ||
    //                       std::is_same_v<T, std::unordered_map<std::string, JsonValue>>) {
    //             return val.size();
    //         } else {
    //             return 0;
    //         }
    //     },
    //     m_data);

    // 常规易于理解的写法
    // 数组
    const auto* arr = std::get_if<std::vector<JsonValue>>(&m_data);
    if (arr != nullptr) {
        return arr->size();
    }
    // 对象map
    const auto* obj = std::get_if<std::unordered_map<std::string, JsonValue>>(&m_data);
    if (obj != nullptr) {
        return obj->size();
    }

    // string
    const auto* str = std::get_if<std::string>(&m_data);
    if (str != nullptr) {
        return str->size();
    }

    // 数字、布尔、null 全部返回0
    return 0;
}

// 判断 字符串，数组，map 是否为空
inline bool JsonValue::isEmpty() const {
    return std::visit(
        [](auto&& val) -> bool {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, std::monostate>) {
                return true;
            } else if constexpr (std::is_same_v<T, std::string> ||
                                 std::is_same_v<T, std::vector<JsonValue>> ||
                                 std::is_same_v<T, std::unordered_map<std::string, JsonValue>>) {
                return val.empty();
            } else {
                return false;  // bool和数字没有"空"的概念
            }
        },
        m_data);
}

// 清空variant变体
inline void JsonValue::clear() {
    m_data = std::monostate{};  // 重置为 null
}

/*
备注：

std::variant 的核心使用

- 用 `std::variant` 替代传统 C 语言 `union`，类型安全，自动管理构造 /
析构，不会出现内存泄漏和未定义行为。

- 用 `std::monostate` 表示 JSON 的 `null`：因为 variant
必须持有一个类型的值，不能真正为空，所以用一个空类占位。

- 配套工具：
  - `std::holds_alternative<T>`：判断当前是否存储 T 类型
  - `std::get<T>`：取出 T 类型的值，类型错误抛出异常
  - `std::visit`：多分支访问，适合序列化、遍历等需要对所有类型处理的场景

*/
