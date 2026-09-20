#pragma once

#include <string>

#include "common.hpp"
#include "lexer.hpp"
#include "value.hpp"

/*
=====================================================================
递归下降语法解析器（Recursive Descent Parser）
=====================================================================

核心思想：JSON 的语法天然是递归的（对象里可以嵌套对象/数组，数组
里也可以嵌套数组），所以我们为每一条语法规则写一个同名解析函数，
函数之间互相递归调用，边读 Token 边构建 JsonValue 树。

对照的 BNF 语法规则（每条规则对应下方一个私有成员函数）：

    value   ::= object | array | string | number | true | false | null
    object  ::= '{' pair (',' pair)* '}'        （空对象 '{}' 也合法）
    pair    ::= string ':' value
    array   ::= '[' value (',' value)* ']'      （空数组 '[]' 也合法）

数据流向：

    JSON原始文本 --Lexer--> Token流 --Parser--> JsonValue 树
*/
class JsonParser {
public:
    // 构造时传入完整的JSON字符串，Parser内部持有Lexer负责切词
    explicit JsonParser(std::string source);

    // 解析成JsonValue树
    MUST_USE JsonValue parse();

    // 递归深度上限
    static constexpr int kMaxDepth = 100;

private:
    // 预读取一个token，不消费（后面的解析分支要靠它决定走向）
    MUST_USE const Token& peek() const;

    // 消耗当前 Token 并返回它，同时从 Lexer 取出下一个 Token 填补位置
    Token consume();

    // 校验当前 Token 是否为期望类型：
    //   - 不匹配 -> 直接抛出带行号列号的语法异常
    //   - 匹配     -> 消耗掉并返回该 Token
    // 用途示例：解析键值对时消耗中间的 ':' -> match(TokenType::Colon)
    Token match(TokenType expect);

    MUST_USE JsonValue parseValue(int depth);

    MUST_USE JsonValue parseObject(int depth);

    MUST_USE JsonValue parseArray(int depth);

    MUST_USE JsonValue parseString();

    MUST_USE JsonValue parseNumber();

    // true / false / null 三个关键字字面量的统一处理
    MUST_USE JsonValue parseLiteral();

    JsonLexer m_lexer;

    Token m_current;
};
