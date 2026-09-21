#pragma once

#include <string>

#include "common.hpp"
#include "value.hpp"

/*
=====================================================================
JSON 序列化器（Serializer）
=====================================================================

核心思想：解析是"Token流 --递归--> JsonValue树"，序列化则是反向的
"JsonValue树 --递归遍历--> JSON文本"。借助 JsonValue::visit() 封装的
std::visit，对 variant 内 6 种类型分别做字符串拼接输出。

对外提供两种输出模式（均为无状态的静态接口，无需构造实例）：
    1. dump()          紧凑模式：无空白无换行，适合网络传输
    2. dumpPretty()    格式化模式：逐层缩进 + 换行，适合日志与调试

输出规则（对照设计文档阶段四）：
    null   -> null
    bool   -> true / false
    number -> 整数值不带小数点，浮点值取"能无损往返的最短表示"
    string -> 双引号包裹，内部字符按 JSON 规则反向转义
    array  -> [] 包裹，元素逗号分隔
    object -> {} 包裹，键值对 "key":value 逗号分隔

转义细节（与 Lexer 支持的转义集严格互逆，保证回环解析安全）：
    \" \\ \b \f \n \r \t 七种短转义，其余 < 0x20 的控制字符输出 \u00XX
*/

class JsonSerializer {
public:
    // 紧凑模式：单行无空白输出，如 {"a":1,"b":[1,2]}
    MUST_USE static std::string dump(const JsonValue& value);

    // 格式化模式：indentStep 为每层缩进的空格数（小于等于 0 时退化为紧凑模式）
    MUST_USE static std::string dumpPretty(const JsonValue& value, int indentStep = 4);

    // 递归深度上限：与 JsonParser::kMaxDepth 保持一致，
    // 防止手工构造的超深嵌套树在递归序列化时栈溢出
    static constexpr int kMaxDepth = 100;
};
