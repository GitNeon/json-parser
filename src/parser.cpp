#include <cstdlib>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "exceptions.hpp"
#include "parser.hpp"

namespace {
// TokenType 枚举 -> 可读字符串，仅用于拼装报错信息
const char* tokenTypeName(TokenType type) {
    switch (type) {
        case TokenType::LBrace:
            return "'{'";
        case TokenType::RBrace:
            return "'}'";
        case TokenType::LBracket:
            return "'['";
        case TokenType::RBracket:
            return "']'";
        case TokenType::Colon:
            return "':'";
        case TokenType::Comma:
            return "','";
        case TokenType::String:
            return "string";
        case TokenType::Number:
            return "number";
        case TokenType::True:
            return "true";
        case TokenType::False:
            return "false";
        case TokenType::Null:
            return "null";
        case TokenType::EndOfFile:
            return "end of input";
        default:
            return "unknown token";
    }
}
};  // namespace

JsonParser::JsonParser(std::string source)
    : m_lexer(std::move(source)), m_current(m_lexer.nextToken()) {}

const Token& JsonParser::peek() const {
    return m_current;
}

// 吃掉当前 Token，并从 Lexer 取下一个 Token 补位
Token JsonParser::consume() {
    Token old = std::move(m_current);
    m_current = m_lexer.nextToken();
    return old;
}

Token JsonParser::match(TokenType expect) {
    if (m_current.m_type != expect) {
        throw JsonParseException(m_current.m_line,
                                 m_current.m_column,
                                 "expected " + std::string(tokenTypeName(expect)) + " but got " +
                                     tokenTypeName(m_current.m_type));
    }
    return consume();
}


// ===================== 解析入口 =====================

JsonValue JsonParser::parse() {
    // 顶层根节点可以是 6 种类型中的任意一种（对象、数组、数字、字符串...）
    // parseValue 从深度 0 开始
    JsonValue root = parseValue(0);

    // 合法 JSON 解析完根节点后必须正好到达 EOF：
    // 拦截 "123abc"、"{...} {...}" 这类"后面还有多余内容"的畸形输入
    if (peek().m_type != TokenType::EndOfFile) {
        throw JsonParseException(peek().m_line,
                                 peek().m_column,
                                 "unexpected content after the top-level value");
    }

    // NRVO：root 直接"移交"给调用方，不产生整棵树的拷贝
    return root;
}

// ===================== 递归解析函数 =====================

// value ::= object | array | string | number | true | false | null
JsonValue JsonParser::parseValue(int depth) {
    // 栈溢出保护：每递归一层 depth + 1，超过上限说明输入嵌套过深
    if (depth > kMaxDepth) {
        throw JsonParseException(peek().m_line,
                                 peek().m_column,
                                 "maximum nesting depth exceeded (limit " +
                                     std::to_string(kMaxDepth) + ")");
    }

    // 核心路由：看当前 Token 是什么，就调用对应的子解析函数。
    // 递归下降法的"下降"就体现在：parseObject/parseArray 内部
    // 会再次调用 parseValue 处理嵌套结构，层层深入再层层返回。
    switch (peek().m_type) {
        case TokenType::LBrace:
            return parseObject(depth);
        case TokenType::LBracket:
            return parseArray(depth);
        case TokenType::String:
            return parseString();
        case TokenType::Number:
            return parseNumber();
        case TokenType::True:
        case TokenType::False:
        case TokenType::Null:
            return parseLiteral();
        default:
            // 到这里的是 '}' ']' ',' ':' 或 EOF 等不该出现在值位置的记号
            throw JsonParseException(peek().m_line,
                                     peek().m_column,
                                     "unexpected token: " +
                                         std::string(tokenTypeName(peek().m_type)));
    }
}

// object ::= '{' pair (',' pair)* '}'    pair ::= string ':' value
JsonValue JsonParser::parseObject(int depth) {
    match(TokenType::LBrace);  // 消耗 '{'，不是 '{' 会在这里报错

    std::unordered_map<std::string, JsonValue> members;

    // 空对象 '{}'：直接消耗 '}' 返回，循环体一次都不执行
    if (peek().m_type == TokenType::RBrace) {
        match(TokenType::RBrace);
        return JsonValue(std::move(members));
    }

    while (true) {
        // ---- 1. 解析 key：JSON 语法规定 key 必须是字符串 ----
        if (peek().m_type != TokenType::String) {
            // 典型触发场景：数字当 key（{1: "x"}）、尾随逗号（{"a":1,}）
            throw JsonParseException(peek().m_line,
                                     peek().m_column,
                                     "object key must be a string, got " +
                                         std::string(tokenTypeName(peek().m_type)));
        }
        Token keyToken = consume();  // 消耗 key 字符串

        // ---- 2. 匹配 key 和 value 之间的冒号 ----
        match(TokenType::Colon);

        // ---- 3. 递归解析 value，深度 + 1 ----
        JsonValue value = parseValue(depth + 1);

        // 重复 key 覆盖策略：operator[] 再次赋值 = 保留最后一次出现的值
        // （与 ECMAScript 规范对 JSON.parse 的行为一致）
        // 高级用法：若想"保留第一个 key"，可改用
        //     members.try_emplace(keyToken.m_value, std::move(value));
        // try_emplace 在 key 已存在时不做任何插入/覆盖
        members[keyToken.m_value] = std::move(value);

        // ---- 4. 看下一个记号：逗号继续下一对，右大括号收尾 ----
        if (peek().m_type == TokenType::Comma) {
            consume();  // 消耗 ','
            continue;   // 回到循环顶部，此时必须是字符串 key（否则报错）
        }
        match(TokenType::RBrace);  // 消耗 '}'，缺失/不匹配在这里报错
        break;                     // 对象解析完成
    }

    return JsonValue(std::move(members));
}

// array ::= '[' value (',' value)* ']'
JsonValue JsonParser::parseArray(int depth) {
    match(TokenType::LBracket);  // 消耗 '['

    std::vector<JsonValue> items;

    // 空数组 '[]'：直接消耗 ']' 返回
    if (peek().m_type == TokenType::RBracket) {
        match(TokenType::RBracket);
        return JsonValue(std::move(items));
    }

    while (true) {
        // ---- 1. 递归解析一个元素 ----
        JsonValue value = parseValue(depth + 1);

        // 高级用法：push_back(std::move(...)) 同样是移交而非拷贝；
        // 也可用 items.emplace_back() 配合就地构造的写法
        items.push_back(std::move(value));

        // ---- 2. 逗号继续，右中括号收尾 ----
        if (peek().m_type == TokenType::Comma) {
            consume();  // 消耗 ','
            continue;   // 回到循环顶部解析下一个元素
        }
        match(TokenType::RBracket);  // 消耗 ']'，缺失在这里报错
        break;
    }

    return JsonValue(std::move(items));
}

// string：转义字符（\n、\uXXXX 等）已由 Lexer 阶段处理完毕，
// Parser 只需把 Token 里现成的字符串包装成 JsonValue
JsonValue JsonParser::parseString() {
    Token token = match(TokenType::String);
    return JsonValue(std::move(token.m_value));
}

// number：Lexer 已校验过数字格式的合法性，这里只负责"文本 -> double"
JsonValue JsonParser::parseNumber() {
    Token token = match(TokenType::Number);

    // std::strtod：C 标准库函数，转换失败时不抛异常，
    // 通过 end 指针判断"是否完整消费"，适合初学者掌控错误流程。
    //
    // 高级用法：C++ 风格可改用 std::stod(token.m_value)，
    // 转换失败会抛出 std::invalid_argument / std::out_of_range 异常，
    // 配合 try-catch 使用；但格式校验已在 Lexer 完成，这里几乎不可能失败
    char* end = nullptr;
    double number = std::strtod(token.m_value.c_str(), &end);

    // end 停在字符串末尾说明整个 Token 都是合法数字
    if (end == token.m_value.c_str()) {
        throw JsonParseException(token.m_line,
                                 token.m_column,
                                 "invalid number literal: " + token.m_value);
    }

    return JsonValue(number);
}

// true / false / null：Lexer 已保证关键字拼写正确，直接映射成 JsonValue
JsonValue JsonParser::parseLiteral() {
    Token token = consume();

    switch (token.m_type) {
        case TokenType::True:
            return JsonValue(true);
        case TokenType::False:
            return JsonValue(false);
        case TokenType::Null:
            return JsonValue(nullptr);
        default:
            // 理论上不可达：parseValue 路由保证了只会传进来这三种 Token。
            // 防御性检查，防止未来改动破坏约定时静默产出错误结果
            throw JsonParseException(token.m_line,
                                     token.m_column,
                                     "internal error: not a literal token");
    }
}
