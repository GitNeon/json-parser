#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>

#include "common.hpp"

/*
===================== 自定义异常体系 =====================
组织约定：
1. 所有自定义异常统一派生自 JsonException，调用方只需 catch (JsonException&)
   即可统一兜底，同时保留按具体类型精确捕获的能力；
2. 项目尚未引入命名空间，异常类型沿用全局作用域 + Json 前缀的命名风格，
   与 JsonValue / JsonType 等保持一致；
3. 新增异常类型时优先继承 JsonException 或其现有子类，不要直接继承
   std::runtime_error，避免破坏统一的异常层次；
4. 异常类型应代表"错误类别"，同一类错误不要拆出多个子类型。
*/

// 库级根异常：JSON 库所有异常的基类
class JsonException : public std::runtime_error {
public:
    explicit JsonException(const std::string& msg) : std::runtime_error(msg) {}
};

// 取值/访问异常：类型不匹配、数组下标越界、对象键不存在等
class JsonValueException : public JsonException {
public:
    explicit JsonValueException(const std::string& msg) : JsonException(msg) {}
};

// 词法分析异常：携带出错位置（行号、列号均从 1 开始计数）。
class JsonLexerException : public JsonException {
public:
    // 参数顺序固定为 (行, 列, 消息)
    JsonLexerException(std::size_t line, std::size_t column, const std::string& msg)
        : JsonException(buildMessage(line, column, msg)), m_line(line), m_column(column) {}

    // 错误位置访问器：调用方程序化获取位置（例如测试断言），无需解析 what() 文本
    MUST_USE std::size_t line() const noexcept {
        return m_line;
    }

    MUST_USE std::size_t column() const noexcept {
        return m_column;
    }

private:
    // 在构造函数中拼装一次完整消息，直接交给 std::runtime_error 缓存，
    // what() 由基类直接返回，无需额外状态
    static std::string buildMessage(std::size_t line, std::size_t column, const std::string& msg) {
        return "Lexer error: [line " + std::to_string(line) + " , column " +
               std::to_string(column) + "]: " + msg;
    }

    std::size_t m_line;
    std::size_t m_column;
};

// 语法解析异常
class JsonParseException : public JsonException {
public:
    explicit JsonParseException(const std::string& msg) : JsonException(msg) {}
};
