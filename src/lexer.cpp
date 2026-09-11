#include <array>
#include <string>

#include "exceptions.hpp"
#include "lexer.hpp"

namespace {
bool isDigitSafe(char c) {
    return std::isdigit(static_cast<unsigned char>(c)) != 0;
}

bool isAlphaSafe(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) != 0;
}

bool isHexDigitSafe(char c) {
    return std::isxdigit(static_cast<unsigned char>(c)) != 0;
}

// 将字符转为可读描述：可打印字符显示 'x'，不可打印字符显示其 ASCII 编码
std::string describeChar(char chr) {
    const auto chr_unsigned = static_cast<unsigned char>(chr);
    if (std::isprint(chr_unsigned) != 0) {
        return "'" + std::string(1, chr) + "'";
    }
    return "(Non-printable characters:  " + std::to_string(static_cast<int>(chr_unsigned)) + ")";
}

std::string hexByte(unsigned char c) {
    static constexpr std::array<char, 16> k_hex{
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    std::string s;
    s.reserve(4);
    s.append("0x");
    s.push_back(k_hex.at((c >> 4) & 0x0F));
    s.push_back(k_hex.at(c & 0x0F));
    return s;
}

// TokenType 枚举 -> 可读字符串，供 Token::toString() 使用
const char* tokenTypeName(TokenType type) {
    switch (type) {
        case TokenType::LBrace:
            return "LBrace";
        case TokenType::RBrace:
            return "RBrace";
        case TokenType::LBracket:
            return "LBracket";
        case TokenType::RBracket:
            return "RBracket";
        case TokenType::Colon:
            return "Colon";
        case TokenType::Comma:
            return "Comma";
        case TokenType::String:
            return "String";
        case TokenType::Number:
            return "Number";
        case TokenType::True:
            return "True";
        case TokenType::False:
            return "False";
        case TokenType::Null:
            return "Null";
        case TokenType::EndOfFile:
            return "EndOfFile";
        default:
            return "Unknown";
    }
}

};  // namespace

// 返回 Token 的调试字符串，格式：
//   Token{type=String, value="name", line=2, column=5}
// 符号类 Token 的 value 为空字符串
std::string Token::toString() const {
    std::string out;
    out.reserve(64);
    out.append("Token{type=")
        .append(tokenTypeName(m_type))
        .append(", value=\"")
        .append(m_value)
        .append("\", line=")
        .append(std::to_string(m_line))
        .append(", column=")
        .append(std::to_string(m_column))
        .push_back('}');
    return out;
}

Token JsonLexer::nextToken() {
    // 如果有预读的Token，先返回
    if (m_has_peek) {
        m_has_peek = false;
        return std::move(m_peeked_token);
    }

    skipWhitespace();

    // 读取完毕判断

    if (isAtEnd()) {
        return Token{TokenType::EndOfFile, "", m_line, m_column};
    }

    // 记录当前Token位于的行、列
    const std::size_t start_line = m_line;
    const std::size_t start_column = m_column;

    const char current_char = currentChar();

    // 针对字符的处理
    switch (current_char) {
        case '{': {
            advanceChar();
            return Token{TokenType::LBrace, "", start_line, start_column};
        }
        case '}': {
            advanceChar();
            return Token{TokenType::RBrace, "", start_line, start_column};
        }
        case '[': {
            advanceChar();
            return Token{TokenType::LBracket, "", start_line, start_column};
        }
        case ']': {
            advanceChar();
            return Token{TokenType::RBracket, "", start_line, start_column};
        }
        case ':': {
            advanceChar();
            return Token{TokenType::Colon, "", start_line, start_column};
        }
        case ',': {
            advanceChar();
            return Token{TokenType::Comma, "", start_line, start_column};
        }
        case '"': {
            return readString();
        }
        default:
            break;
    }

    if (current_char == '-' || isDigitSafe(current_char)) {
        return readNumber();
    }

    if (isAlphaSafe(current_char)) {
        return readKeyword();
    }

    // 其他非法字符：抛出带行列定位的词法异常
    const char bad_char = currentChar();
    advanceChar();
    throw JsonLexerException(
        start_line, start_column, "invalid characters: " + describeChar(bad_char));
}

// 预读字符
const Token& JsonLexer::peekToken() {
    if (!m_has_peek) {
        m_peeked_token = nextToken();
        m_has_peek = true;
    }
    return m_peeked_token;
}

// ============================================================
// readString：解析双引号包裹的字符串字面量
// ------------------------------------------------------------
// 已确认当前字符为起始 '"'，本函数负责消费到闭合 '"'。
// 处理内容：
//   1. 普通字符（>= 0x20）原样写入；
//   2. 转义序列：\" \\ \/ \b \f \n \r \t 直接还原为对应字符；
//   3. \uXXXX：保留字面量文本 "\uXXXX"，不做 UTF-8 转换
//      （避免误把 BMP 之外的代理对编码搞错；后续 Serializer 阶段可按需处理）；
//   4. 控制字符（< 0x20）未转义出现 -> 抛异常；
//   5. 遇到未知转义（如 \z）-> 抛异常；
//   6. 字符串到末尾仍未闭合 -> 抛异常。
// 注：转义序列与 \u 的细节在 readEscape / readUnicodeEscape 中处理。
// ============================================================
Token JsonLexer::readString() {
    const std::size_t start_line = m_line;
    const std::size_t start_column = m_column;

    advanceChar();  // 消费起始双引号

    std::string out;
    out.reserve(16);

    while (!isAtEnd()) {
        const char chr = currentChar();

        if (chr == '"') {
            advanceChar();  // 消费末尾双引号
            return Token{TokenType::String, std::move(out), start_line, start_column};
        }

        // 2.处理转义情况：\ 及其后的转义字符由 readEscape 整体消费
        if (chr == '\\') {
            readEscape(out);
            continue;  // 转义已处理完，跳过下方的控制字符检查与追加
        }

        // 3.未控制的转义字符
        // JSON 规范规定：字符串内部不能直接出现原始控制字符（ASCII 0~31，也就是小于`0x20`）。
        // 换行、回车、退格、制表符这些**不能直接写在 JSON 字符串里面**，必须写成转义形式 `\n` /
        // `\r` / `\t` / `\b` / `\f` 或者 `\u00xx`
        const unsigned char uch = static_cast<unsigned char>(chr);
        if (uch < 0x20) {
            throw JsonLexerException(
                m_line, m_column, "unescaped control character in string " + hexByte(uch));
        }

        // 1.正常字符串一直追加
        out.push_back(chr);
        advanceChar();
    }

    throw JsonLexerException(start_line, start_column, "unterminated string: missing closing '\"'");
}

// ============================================================
// readEscape：处理反斜杠开头的转义序列
// ------------------------------------------------------------
// 调用时当前字符必须是 '\'；函数负责消费 '\' 与紧随其后的转义字符，
// 并把还原后的内容追加到 out；未知转义抛异常。
// ============================================================
void JsonLexer::readEscape(std::string& out) {
    // 记录反斜杠所在位置，方便报错时定位
    const std::size_t esc_line = m_line;
    const std::size_t esc_column = m_column;

    advanceChar();  // 消费 '\'

    // 非法情况：反斜杠之后没有内容
    if (isAtEnd()) {
        throw JsonLexerException(esc_line, esc_column, "missing character after backslash in string");
    }

    const char esc = currentChar();  // 紧跟在转义符后的字符
    advanceChar();                   // 指针后移，防止下次循环把转义字符的内容当作普通字符来处理

    switch (esc) {
        case '"': {
            out.push_back('"');
            break;
        }
        case '\\': {
            out.push_back('\\');
            break;
        }
        case '/': {
            out.push_back('/');
            break;
        }
        case 'b': {
            out.push_back('\b');
            break;
        }
        case 'f': {
            out.push_back('\f');
            break;
        }
        case 'n': {
            out.push_back('\n');
            break;
        }
        case 'r': {
            out.push_back('\r');
            break;
        }
        case 't': {
            out.push_back('\t');
            break;
        }
        case 'u': {
            out.append(readUnicodeEscape());  // 保留 \uXXXX 原文，让上游决定是否解码
            break;
        }
        default:
            throw JsonLexerException(
                esc_line, esc_column, std::string("invalid escape character in string: ") + esc);
    }
}

// ============================================================
// readUnicodeEscape：读取 \u 之后的 4 位十六进制数字
// ------------------------------------------------------------
// 返回字面量原文 "\uXXXX"，不做 UTF-8 转换
// （避免误把 BMP 之外的代理对编码搞错；后续 Serializer 阶段可按需处理）。
// ============================================================
std::string JsonLexer::readUnicodeEscape() {
    if (m_source.size() < m_pos + 4) {
        throw JsonLexerException(m_line, m_column, "\\u escape requires 4 hex digits");
    }

    std::string hex;
    hex.reserve(4);

    for (int i = 0; i < 4; ++i) {
        const char digit = currentChar();
        if (!isHexDigitSafe(digit)) {
            throw JsonLexerException(m_line, m_column, "\\u escape contains invalid character");
        }
        hex.push_back(digit);
        advanceChar();
    }

    return "\\u" + hex;
}

// ============================================================
// readNumber：解析数字字面量
// ------------------------------------------------------------
// 已确认当前字符为 '-' 或数字。完整支持：
//   - 可选负号
//   - 整数部分：0 / 非0开头的连续数字（不允许前导零，如 0123 非法）
//   - 可选小数部分：. 后必须跟至少 1 位数字
//   - 可选指数部分：e/E 后可选 +/-，再必须跟至少 1 位数字
//   - 数字以字符串形式保存在 Token.value，后续由 Parser 转 double
// ============================================================
Token JsonLexer::readNumber() {
    const std::size_t start_line = m_line;
    const std::size_t start_column = m_column;
    const std::size_t start_pos = m_pos;

    if (!isAtEnd() && currentChar() == '-') {
        advanceChar();
    }

    if (isAtEnd() || !isDigitSafe(currentChar())) {
        throw JsonLexerException(m_line, m_column, "number is missing integer part");
    }

    if (currentChar() == '0') {
        advanceChar();
        // JSON不允许前导0（0本身除外）
        if (!isAtEnd() && isDigitSafe(currentChar())) {
            throw JsonLexerException(m_line, m_column, "leading zeros are not allowed in numbers");
        }
    } else {
        // 1-9 开头的连续数字
        while (!isAtEnd() && isDigitSafe(currentChar())) {
            advanceChar();
        }
    }

    // 可选小数部分处理
    if (!isAtEnd() && currentChar() == '.') {
        advanceChar();
        if (isAtEnd() || !isDigitSafe(currentChar())) {
            throw JsonLexerException(m_line, m_column, "fraction part requires at least one digit");
        }

        while (!isAtEnd() && isDigitSafe(currentChar())) {
            advanceChar();
        }
    }

    // 可选指数部分
    if (!isAtEnd() && (currentChar() == 'e' || currentChar() == 'E')) {
        advanceChar();
        if (!isAtEnd() && (currentChar() == '+' || currentChar() == '-')) {
            advanceChar();
        }
        if (isAtEnd() || !isDigitSafe(currentChar())) {
            throw JsonLexerException(m_line, m_column, "exponent part requires at least one digit");
        }
        while (!isAtEnd() && isDigitSafe(currentChar())) {
            advanceChar();
        }
    }

    const std::string text = m_source.substr(start_pos, m_pos - start_pos);
    return Token{TokenType::Number, text, start_line, start_column};
}

// ============================================================
// readKeyword：解析关键字
// ------------------------------------------------------------
// 已确认当前字符为字母。本函数读取连续的字母片段，然后匹配：
//   true  -> TokenType::True
//   false -> TokenType::False
//   null  -> TokenType::Null
// 其它任意字母组合（如 abc、undefined）均视为非法关键字。
// ============================================================
Token JsonLexer::readKeyword() {
    const std::size_t start_line = m_line;
    const std::size_t start_column = m_column;
    const std::size_t start_pos = m_pos;

    while (!isAtEnd() && isAlphaSafe(currentChar())) {
        advanceChar();
    }

    const std::string word = m_source.substr(start_pos, m_pos - start_pos);

    if (word == "true") {
        return Token{TokenType::True, word, start_line, start_column};
    }

    throw JsonLexerException(start_line, start_column, "unknown keyword: " + word);
}
