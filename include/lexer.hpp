#include <cctype>
#include <cstdint>
#include <string>
#include <utility>


#include "common.hpp"

// 所有记号类型
enum class TokenType : std::uint8_t {
    LBrace,     // {
    RBrace,     // }
    LBracket,   // [
    RBracket,   // ]
    Colon,      // :
    Comma,      // ,
    String,     // "xxx"
    Number,     // 123, -1.5, 1e10
    True,       // true
    False,      // false
    Null,       // null
    EndOfFile,  // EOF结束文件结束标志
};

// 单个记号结构体
// 仅字面量类（String / Number / 关键字）有值；符号类留空
struct Token {
    TokenType m_type = TokenType::EndOfFile;  // NOLINT
    std::string m_value;                      // NOLINT
    std::size_t m_line = 1;                   // NOLINT
    std::size_t m_column = 1;                 // NOLINT

    MUST_USE std::string toString() const;
};

class JsonLexer {
public:
    explicit JsonLexer(std::string source);

    // 消费并返回下一个TOKEN
    MUST_USE Token nextToken();

    // 预读一个TOKEN(不消费)
    MUST_USE const Token& peekToken();

    /* ===== 字符工具 ===== */
    MUST_USE bool isAtEnd() const noexcept;
    MUST_USE char currentChar() const;  // 不前进，紧查看
    char advanceChar();                 // 取当前字符并前进，更新行/列
    bool matchChar(char expected);      // 若当前字符 == expected 则前进并返回 true

    void skipWhitespace();  // 跳过空白（空格、tab、回车、换行）

    /* ===== 字符读取 ===== */
    Token readString();   // 读取字符串，以"开始
    Token readNumber();   // 读取数字，以数字或者负号-开始
    Token readKeyword();  // 读取关键字，以字母开始的，例如null, true, false

private:
    /* ===== 内部辅助（从 readString 拆分，降低认知复杂度） ===== */
    void readEscape(std::string& out);         // 处理 \ 开头的转义序列，结果追加到 out
    MUST_USE std::string readUnicodeEscape();  // 读取 \u 后的 4 位十六进制，返回 "\uXXXX" 原文

    std::string m_source;
    std::size_t m_pos = 0;     // 当前读取位置
    std::size_t m_line = 1;    // 行号
    std::size_t m_column = 1;  // 列号

    Token m_peeked_token;     // 预读的TOKEN缓存
    bool m_has_peek = false;  // 是否预读
};

/* ===== inline 内联函数 ===== */
inline JsonLexer::JsonLexer(std::string source) : m_source(std::move(source)) {}

inline bool JsonLexer::isAtEnd() const noexcept {
    return m_pos >= m_source.size();
}

inline char JsonLexer::currentChar() const {
    return m_source[m_pos];
}

inline char JsonLexer::advanceChar() {
    char c = m_source[m_pos++];

    if (c == '\n') {
        m_line++;
        m_column = 1;
    } else {
        m_column++;
    }
    return c;
}

inline bool JsonLexer::matchChar(char expected) {
    if (isAtEnd() || m_source[m_pos] != expected) {
        return false;
    }

    advanceChar();
    return true;
}

inline void JsonLexer::skipWhitespace() {
    while (!isAtEnd() && std::isspace(static_cast<unsigned char>(currentChar())) != 0) {
        advanceChar();
    }
}
