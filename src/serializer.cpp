#include "serializer.hpp"

#include <array>
#include <cmath>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "exceptions.hpp"

namespace {

// ===================== 字符串/数字/缩进格式化工具 =====================

// 字符串反向转义：双引号、反斜杠、\b \f \n \r \t 及其余控制字符
void writeString(std::ostringstream& out, const std::string& str) {
    out << '"';
    for (const char chr : str) {
        const auto uch = static_cast<unsigned char>(chr);
        switch (chr) {
            case '"':
                out << "\\\"";
                break;
            case '\\':
                out << "\\\\";
                break;
            case '\b':
                out << "\\b";
                break;
            case '\f':
                out << "\\f";
                break;
            case '\n':
                out << "\\n";
                break;
            case '\r':
                out << "\\r";
                break;
            case '\t':
                out << "\\t";
                break;
            default:
                if (uch < 0x20) {
                    // 其余控制字符没有短转义形式，统一按 \u00XX 输出。
                    // 走查表拼字符而非 snprintf：避免 C 风格变参调用与临时缓冲区，
                    // 且 uch < 0x20 时高位两位恒为 "00"，只需拼两个十六进制数字
                    static constexpr std::array<char, 16> k_hex{
                        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
                    out << "\\u00" << k_hex.at(uch >> 4) << k_hex.at(uch & 0x0F);
                } else {
                    // 可打印 ASCII 与 UTF-8 多字节序列（>= 0x80）按原样透传
                    out << chr;
                }
        }
    }
    out << '"';
}

// 数字转文本：整数不带小数点，浮点数取最短无损表示，非有限值输出 null
void writeNumber(std::ostringstream& out, double num) {
    // NaN / Infinity 不是合法 JSON 数值，统一降级为 null，
    // 保证输出永远是可被 Parser 重新解析的合法 JSON
    if (!std::isfinite(num)) {
        out << "null";
        return;
    }

    // 整数值（含 0 与负数）直接按整数输出，避免出现 "42.000000" 这类多余小数
    if (std::fabs(num) < 1e15 && std::floor(num) == num) {
        out << static_cast<long long>(num);
        return;
    }

    // 浮点数：从 15 位有效数字开始逐级放宽到 17 位，
    // 取第一个"转回 double 后与原值完全相等"的最短表示，
    // 既不丢精度（回环安全），也不出现 "0.10000000000000001" 这类长尾
    for (int precision = 15; precision <= 17; ++precision) {
        std::ostringstream candidate;
        candidate.precision(precision);
        candidate << num;

        std::istringstream backIn(candidate.str());
        double roundTrip = 0.0;
        backIn >> roundTrip;
        if (roundTrip == num) {
            out << candidate.str();
            return;
        }
    }

    // 17 位有效数字必然覆盖 double 的全部精度，此分支仅为编译器保险
    std::ostringstream fallback;
    fallback.precision(17);
    fallback << num;
    out << fallback.str();
}

// 输出 depth 层对应的缩进（depth * indentStep 个空格）
void writeIndent(std::ostringstream& out, int depth, int indentStep) {
    for (int i = 0; i < depth * indentStep; ++i) {
        out << ' ';
    }
}

// 递归入口前置声明：数组/对象分支需要回调它遍历子节点
void serializeValue(std::ostringstream& out,
                    const JsonValue& value,
                    int depth,
                    int indentStep,
                    bool pretty);

/*
std::visit 访问器：对 variant 内 6 种类型各写一个重载分支，
是"一个类型一个 operator()"的最直观 visitor 写法。

数组/对象分支内部再递回 serializeValue()，实现树的递归遍历；
成员全部为只读上下文，const 修饰保证序列化过程绝不改动原树。
*/
struct ValueWriter {
    // 序列化上下文：输出流、当前递归深度、缩进配置
    std::ostringstream& out;
    int depth;
    int indentStep;
    bool pretty;

    // null -> null（monostate 即 JsonValue 内部的 null 占位类型）
    void operator()(std::monostate) const {
        out << "null";
    }

    // bool -> true / false
    void operator()(bool value) const {
        out << (value ? "true" : "false");
    }

    // number -> 交给统一的数字格式化
    void operator()(double value) const {
        writeNumber(out, value);
    }

    // string -> 双引号包裹 + 反向转义
    void operator()(const std::string& value) const {
        writeString(out, value);
    }

    // array -> [v1,v2,...]，pretty 模式下每个元素独占一行
    void operator()(const std::vector<JsonValue>& arr) const {
        // 空数组直接输出 []，两种模式行为一致，避免无意义的换行
        if (arr.empty()) {
            out << "[]";
            return;
        }

        out << '[';
        for (size_t i = 0; i < arr.size(); ++i) {
            if (i > 0) {
                out << ',';
            }
            if (pretty) {
                out << '\n';
                writeIndent(out, depth + 1, indentStep);
            }
            serializeValue(out, arr[i], depth + 1, indentStep, pretty);
        }
        if (pretty) {
            out << '\n';
            writeIndent(out, depth, indentStep);
        }
        out << ']';
    }

    // object -> {"k":v,...}，pretty 模式下每组键值对独占一行
    void operator()(const std::unordered_map<std::string, JsonValue>& obj) const {
        // 空对象直接输出 {}，与空数组的处理保持对称
        if (obj.empty()) {
            out << "{}";
            return;
        }

        out << '{';
        bool first = true;
        for (const auto& [key, val] : obj) {
            if (!first) {
                out << ',';
            }
            first = false;
            if (pretty) {
                out << '\n';
                writeIndent(out, depth + 1, indentStep);
            }
            writeString(out, key);
            out << ':';
            if (pretty) {
                out << ' ';
            }
            serializeValue(out, val, depth + 1, indentStep, pretty);
        }
        if (pretty) {
            out << '\n';
            writeIndent(out, depth, indentStep);
        }
        out << '}';
    }
};

// 递归入口实现：先做深度保护再分发到 visitor
void serializeValue(std::ostringstream& out,
                    const JsonValue& value,
                    int depth,
                    int indentStep,
                    bool pretty) {
    if (depth > JsonSerializer::kMaxDepth) {
        throw JsonException("serialization nesting depth exceeded (limit " +
                            std::to_string(JsonSerializer::kMaxDepth) + ")");
    }
    value.visit(ValueWriter{out, depth, indentStep, pretty});
}

}  // namespace

// ===================== 对外接口 =====================

std::string JsonSerializer::dump(const JsonValue& value) {
    std::ostringstream out;
    serializeValue(out, value, 0, 0, false);
    return out.str();
}

std::string JsonSerializer::dumpPretty(const JsonValue& value, int indentStep) {
    // 缩进步长非法时退化为紧凑模式，保证任何输入都有合法输出
    const bool pretty = indentStep > 0;
    std::ostringstream out;
    serializeValue(out, value, 0, indentStep, pretty);
    return out.str();
}
