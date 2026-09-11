// ============================================================
// 示例：JsonLexer 的典型用法
// ------------------------------------------------------------
// JsonLexer 负责把 JSON 文本切分成 Token（记号）序列，
// 是语法分析器（Parser）的前置阶段，也可以单独用于调试、格式化等场景。
//
// 覆盖场景：
//   1. 完整扫描：循环 nextToken() 直到 EndOfFile，打印每个 Token 的类型/值/位置
//   2. 预读：peekToken() 查看下一个 Token 但不消费
//   3. 空白处理：空格、换行会被自动跳过，行号/列号自动跟踪
//   4. 转义字符串：字符串内的 \n \" 等转义会被还原
//   5. 异常处理：非法字符 / 字符串未闭合时抛出 JsonLexerException（带行列号）
//
// 打印 Token 统一使用库提供的 Token::toString()，
// 输出格式：Token{type=String, value="name", line=2, column=5}
//
// 编译运行（在项目根目录执行）：
//   cmake --preset gcc-debug
//   cmake --build --preset gcc-debug --target example_json_lexer
//   ./build/gcc-debug/example/example_json_lexer
// ============================================================

#include <iostream>
#include <string>

#include "exceptions.hpp"
#include "lexer.hpp"

namespace {

// 打印单个 Token：直接使用库提供的 Token::toString()
void printToken(const Token& token) {
    std::cout << "  " << token.toString() << '\n';
}

// ---------- 场景 1 & 3：完整扫描一段 JSON 文本 ----------
void demoScanAll() {
    std::cout << "== 1. Scan all tokens ==\n";

    // 一段包含对象、数组、字符串、整数、小数、指数、负数、布尔的典型 JSON。
    // 注意：readKeyword() 目前只实现了 true，false / null 关键字支持尚在开发中，
    // 这里暂不放入演示输入，待实现后可自行加入测试。
    const std::string json = R"({
        "name": "Alice",
        "age": 30,
        "pi": 3.14e0,
        "score": -1.5,
        "isVip": true,
        "hobbies": ["reading", "coding"]
    })";

    std::cout << "source:\n" << json << "\ntokens:\n";

    JsonLexer lexer(json);
    while (true) {
        Token token = lexer.nextToken();  // 消费并返回下一个 Token
        printToken(token);
        if (token.m_type == TokenType::EndOfFile) {
            break;  // 扫描结束
        }
    }
}

// ---------- 场景 2：peekToken 预读（不消费） ----------
void demoPeek() {
    std::cout << "\n== 2. peekToken (lookahead) ==\n";

    JsonLexer lexer("[1, 2]");

    // peek 只查看不前进：连续 peek 两次得到的是同一个 Token
    const Token& first_peek = lexer.peekToken();
    std::cout << "peek #1: " << first_peek.toString() << '\n';
    const Token& second_peek = lexer.peekToken();
    std::cout << "peek #2: " << second_peek.toString() << '\n';

    // nextToken 会返回刚才预读的那个 Token，之后才继续前进
    Token consumed = lexer.nextToken();
    std::cout << "nextToken after peek: " << consumed.toString() << '\n';
    std::cout << "next token now: " << lexer.nextToken().toString() << '\n';
}

// ---------- 场景 4：转义字符串 ----------
void demoEscapeString() {
    std::cout << "\n== 3. Escaped string ==\n";

    // 原始字符串里包含转义：\n 会被还原成真正的换行字符
    JsonLexer lexer(R"("line1\nline2 \"quoted\" \u4e2d")");
    Token token = lexer.nextToken();

    std::cout << "token = " << token.toString() << '\n';
    std::cout << "token value (with real newline):\n" << token.m_value << '\n';
}

// ---------- 场景 5：词法错误与异常定位 ----------
void demoLexerErrors() {
    std::cout << "\n== 4. Lexer errors ==\n";

    // 错误 1：非法字符 '@'
    try {
        JsonLexer lexer("{\"key\": @}");
        while (lexer.nextToken().m_type != TokenType::EndOfFile) {
        }
    } catch (const JsonLexerException& e) {
        std::cout << "invalid char -> " << e.what() << '\n';
        std::cout << "  position: line=" << e.line() << ", column=" << e.column() << '\n';
    }

    // 错误 2：字符串未闭合
    try {
        JsonLexer lexer("{\"name\": \"Alice");
        while (lexer.nextToken().m_type != TokenType::EndOfFile) {
        }
    } catch (const JsonLexerException& e) {
        std::cout << "unterminated string -> " << e.what() << '\n';
    }
}

}  // namespace

int main() {
    demoScanAll();
    demoPeek();
    demoEscapeString();
    demoLexerErrors();

    std::cout << "\nAll JsonLexer demos finished.\n";
    return 0;
}
