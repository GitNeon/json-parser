// ============================================================
// my-json：JSON 解析器控制台交互程序
// ------------------------------------------------------------
// 菜单式 REPL 界面，覆盖 JSON 的完整使用闭环：
//   加载（键盘/文件）→ 查看（紧凑/格式化）→ 查询/修改（路径定位）
//   → 校验 → 回环测试
//
// 路径语法（查询/修改共用）：
//   key(.key|[index])* 组合，如 user.address.city、items[0].name；
//   空路径表示根节点。
//
// 界面输出全部为英文，避免不同终端的字符集差异导致乱码。
//
// 编译运行（在项目根目录执行）：
//   cmake --preset clang-debug
//   cmake --build --preset clang-debug --target my-json
//   ./build/clang-debug/my-json
// ============================================================

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "exceptions.hpp"
#include "parser.hpp"
#include "serializer.hpp"
#include "value.hpp"

namespace {

// ===================== Banner 字符画 =====================

constexpr size_t kBannerRows = 6;

// "MY JSON PARSER" 字符画：每个字形固定 6 行等宽。
// 采用"字形表 + 运行时逐行拼接"而不是手写整行文本，
// 从机制上保证各字母纵向对齐，增删字母也不会破坏排版。
struct BannerGlyph {
    std::array<const char*, kBannerRows> m_rows;
};

const std::array<BannerGlyph, 23> kBANNER = {{
    // M
    {" __  __ ",
     "|  \\/  |",
     "| |\\/| |",
     "| |  | |",
     "| |  | |",
     "|_|  |_|"},
    // 字母间距
    {" ", " ", " ", " ", " ", " "},
    // Y
    {"__   __",
     "\\ \\ / /",
     " \\ V / ",
     "  | |  ",
     "  | |  ",
     "  |_|  "},
    // 单词间距
    {"   ", "   ", "   ", "   ", "   ", "   "},
    // J
    {"       ",
     "     _ ",
     "    | |",
     " _  | |",
     "| |_| |",
     " \\___/ "},
    {" ", " ", " ", " ", " ", " "},
    // S
    {"       ",
     " ____  ",
     "/ ___| ",
     "\\___ \\ ",
     " ___) |",
     "|____/ "},
    {" ", " ", " ", " ", " ", " "},
    // O
    {"       ",
     "  ___  ",
     " / _ \\ ",
     "| | | |",
     "| |_| |",
     " \\___/ "},
    {" ", " ", " ", " ", " ", " "},
    // N
    {" _   _  ",
     "| \\ | | ",
     "|  \\| | ",
     "| |\\  | ",
     "| | \\ | ",
     "|_|  \\_|"},
    // 单词间距
    {"   ", "   ", "   ", "   ", "   ", "   "},
    // P
    {"       ",
     " ____  ",
     "|  _ \\ ",
     "| |_) |",
     "|  __/ ",
     "|_|    "},
    {" ", " ", " ", " ", " ", " "},
    // A
    {"         ",
     "    _    ",
     "   / \\   ",
     "  / _ \\  ",
     " / ___ \\ ",
     "/_/   \\_\\"},
    {" ", " ", " ", " ", " ", " "},
    // R
    {"       ",
     " ____  ",
     "|  _ \\ ",
     "| |_) |",
     "|  _ < ",
     "|_| \\_\\"},
    {" ", " ", " ", " ", " ", " "},
    // S
    {"       ",
     " ____  ",
     "/ ___| ",
     "\\___ \\ ",
     " ___) |",
     "|____/ "},
    {" ", " ", " ", " ", " ", " "},
    // E
    {"       ",
     " _____ ",
     "| ____|",
     "|  _|  ",
     "| |___ ",
     "|_____|"},
    {" ", " ", " ", " ", " ", " "},
    // R
    {"       ",
     " ____  ",
     "|  _ \\ ",
     "| |_) |",
     "|  _ < ",
     "|_| \\_\\"},
}};

// 逐行拼接字形得到完整 Banner，分隔线宽度取最长行动态计算
void printBanner() {
    std::vector<std::string> rows(kBannerRows);
    for (const BannerGlyph& glyph : kBANNER) { 
        for (size_t row = 0; row < kBannerRows; ++row) {
            rows[row] += glyph.m_rows.at(row);
        }
    }

    size_t width = 0;
    for (const std::string& row : rows) {
        width = row.size() > width ? row.size() : width;
    }

    const std::string rule(width, '=');
    std::cout << rule << '\n';
    for (const std::string& row : rows) {
        std::cout << row << '\n';
    }
    std::cout << rule << '\n';
    std::cout << "  MY JSON PARSER v1.0.0 -- parse / inspect / modify / serialize\n";
}

// ===================== 通用小工具 =====================

std::string trim(const std::string& text) {
    size_t begin = 0;
    size_t end = text.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
        --end;
    }
    return text.substr(begin, end - begin);
}

bool isDigits(const std::string& text) {
    if (text.empty()) {
        return false;
    }
    return std::all_of(text.begin(), text.end(), [](char chr) {
        return std::isdigit(static_cast<unsigned char>(chr)) != 0;
    });
}

const char* jsonTypeName(JsonType type) {
    switch (type) {
        case JsonType::Null:
            return "null";
        case JsonType::Bool:
            return "bool";
        case JsonType::Number:
            return "number";
        case JsonType::String:
            return "string";
        case JsonType::Array:
            return "array";
        case JsonType::Object:
            return "object";
    }
    return "unknown";
}

// 读一行；stdin 结束（EOF）返回 false
bool readLine(std::string& line) {
    return static_cast<bool>(std::getline(std::cin, line));
}

// 多行输入 JSON 文本，空行结束；各行用 '\n' 连接保留原始行号，便于报错定位
std::string readMultilineJson() {
    std::cout << "Enter JSON text, finish with an empty line:\n> ";
    std::string text;
    std::string line;
    while (readLine(line) && !trim(line).empty()) {
        if (!text.empty()) {
            text += '\n';
        }
        text += line;
        std::cout << "> ";
    }
    return text;
}

void pause() {
    std::cout << "\nPress ENTER to continue...";
    std::string line;
    if (!readLine(line)) {
        std::cout << '\n';
    }
}

// ===================== 路径解析 =====================

// 递归结构相等：两棵树类型与内容完全一致即视为相等。
// 回环校验不能用 dump 字符串逐字节比较——两次构建的树
// 插入顺序可能不同，unordered_map 的迭代序会随之改变
bool jsonEqual(const JsonValue& lhs, const JsonValue& rhs) {
    if (lhs.type() != rhs.type()) {
        return false;
    }
    switch (lhs.type()) {
        case JsonType::Null:
            return true;
        case JsonType::Bool:
            return lhs.asBool() == rhs.asBool();
        case JsonType::Number:
            return lhs.asNumber() == rhs.asNumber();
        case JsonType::String:
            return lhs.asString() == rhs.asString();
        case JsonType::Array: {
            if (lhs.size() != rhs.size()) {
                return false;
            }
            for (size_t i = 0; i < lhs.size(); ++i) {
                if (!jsonEqual(lhs.at(i), rhs.at(i))) {
                    return false;
                }
            }
            return true;
        }
        case JsonType::Object: {
            if (lhs.size() != rhs.size()) {
                return false;
            }
            return std::all_of(lhs.asObject().begin(),
                               lhs.asObject().end(),
                               [&rhs](const auto& entry) {
                                   const auto iter = rhs.asObject().find(entry.first);
                                   return iter != rhs.asObject().end() &&
                                          jsonEqual(entry.second, iter->second);
                               });
        }
    }
    return false;
}

// 路径语法：key(.key|[index])*，如 user.address.city、items[0].name；空路径表示根节点
// 成功返回目标节点指针（可用于读/写），失败返回 nullptr 并填充 error

// 解析 "[index]" 段：pos 指向 '['，成功时推进 pos 并返回下一节点，失败返回 nullptr
JsonValue* resolveIndexSegment(JsonValue* current,
                               const std::string& path,
                               size_t& pos,
                               std::string& error) {
    const size_t close = path.find(']', pos);
    if (close == std::string::npos) {
        error = "unclosed '[' in path";
        return nullptr;
    }
    const std::string index_text = path.substr(pos + 1, close - pos - 1);
    if (!isDigits(index_text)) {
        error = "invalid array index: [" + index_text + "]";
        return nullptr;
    }
    // 超长数字直接判越界，避免 stoul 溢出抛异常
    if (index_text.size() > 9) {
        error = "array index out of range: [" + index_text + "]";
        return nullptr;
    }
    if (!current->isArray()) {
        error = "path segment [" + index_text + "] used on a non-array value";
        return nullptr;
    }
    const size_t index = static_cast<size_t>(std::stoul(index_text));
    if (index >= current->size()) {
        error = "array index out of range: [" + index_text + "]";
        return nullptr;
    }
    pos = close + 1;
    return &current->at(index);
}

// 解析普通对象 key 段：pos 指向 key 首字符，读到下一个 '.' 或 '[' 为止
JsonValue* resolveKeySegment(JsonValue* current,
                             const std::string& path,
                             size_t& pos,
                             std::string& error) {
    size_t key_end = pos;
    while (key_end < path.size() && path[key_end] != '.' && path[key_end] != '[') {
        ++key_end;
    }
    const std::string key = path.substr(pos, key_end - pos);
    if (!current->isObject()) {
        error = "path segment '" + key + "' used on a non-object value";
        return nullptr;
    }
    const auto iter = current->asObject().find(key);
    if (iter == current->asObject().end()) {
        error = "key not found: '" + key + "'";
        return nullptr;
    }
    pos = key_end;
    return &iter->second;
}

JsonValue* resolvePath(JsonValue& root, const std::string& path, std::string& error) {
    JsonValue* current = &root;
    size_t pos = 0;

    while (pos < path.size()) {
        if (path[pos] == '.') {
            ++pos;
            continue;
        }
        if (path[pos] == '[') {
            current = resolveIndexSegment(current, path, pos, error);
        } else {
            current = resolveKeySegment(current, path, pos, error);
        }
        if (current == nullptr) {
            return nullptr;
        }
    }

    return current;
}

// ===================== 应用状态与菜单 =====================

struct AppState {
    JsonValue m_document;
    bool m_loaded = false;
};

// 两列菜单条目，运行时按固定列位补空格，保证边框对齐
void printMenuRow(const std::string& left, const std::string& right) {
    constexpr size_t inner_width = 62;  // 边框内总宽
    constexpr size_t right_column = 34; // 右列起始列
    std::cout << "|  " << left;
    if (right.empty()) {
        std::cout << std::string(inner_width - 2 - left.size(), ' ') << "|\n";
        return;
    }
    std::cout << std::string(right_column - 2 - left.size(), ' ') << right
              << std::string(inner_width - right_column - right.size(), ' ') << "|\n";
}

void printMenu(const AppState& app) {
    // 状态行：显示当前文档概况
    std::cout << '\n';
    if (!app.m_loaded) {
        std::cout << "Document: (none loaded)";
    } else {
        const JsonType type = app.m_document.type();
        std::cout << "Document: " << jsonTypeName(type);
        if (type == JsonType::Array) {
            std::cout << " (" << app.m_document.size() << " items)";
        } else if (type == JsonType::Object) {
            std::cout << " (" << app.m_document.size() << " keys)";
        }
    }

    const std::string rule = "+" + std::string(62, '-') + "+";
    std::cout << "\n\n" << rule << '\n';
    printMenuRow("1. Load JSON from keyboard", "5. Query value by path");
    printMenuRow("2. Load JSON from file", "6. Modify value by path");
    printMenuRow("3. Print document (compact)", "7. Validate JSON text");
    printMenuRow("4. Print document (pretty)", "8. Round-trip check");
    printMenuRow("0. Exit", "");
    std::cout << rule << '\n' << "Choose an option: ";
}

bool requireDocument(const AppState& app) {
    if (app.m_loaded) {
        return true;
    }
    std::cout << "No document loaded yet (use option 1 or 2 first).\n";
    return false;
}

// ===================== 菜单动作 =====================

void actionLoadFromKeyboard(AppState& app) {
    std::cout << "\n[Load JSON from keyboard]\n";
    const std::string text = readMultilineJson();
    if (text.empty()) {
        std::cout << "No input, nothing loaded.\n";
        return;
    }
    try {
        app.m_document = JsonParser(text).parse();
        app.m_loaded = true;
        std::cout << "OK, document loaded.\n";
    } catch (const JsonException& e) {
        std::cout << "Parse failed: " << e.what() << '\n';
    }
}

void actionLoadFromFile(AppState& app) {
    std::cout << "\n[Load JSON from file]\nFile path: ";
    std::string path;
    if (!readLine(path)) {
        return;
    }
    path = trim(path);
    if (path.empty()) {
        std::cout << "No path given.\n";
        return;
    }

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::cout << "Cannot open file: " << path << '\n';
        return;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    try {
        app.m_document = JsonParser(buffer.str()).parse();
        app.m_loaded = true;
        std::cout << "OK, document loaded from " << path << '\n';
    } catch (const JsonException& e) {
        std::cout << "Parse failed: " << e.what() << '\n';
    }
}

void actionPrintCompact(AppState& app) {
    if (!requireDocument(app)) {
        return;
    }
    std::cout << "\n[Compact output]\n" << JsonSerializer::dump(app.m_document) << '\n';
}

void actionPrintPretty(AppState& app) {
    if (!requireDocument(app)) {
        return;
    }
    std::cout << "\n[Pretty output]\nIndent step (default 4): ";
    std::string input;
    if (!readLine(input)) {
        return;
    }
    input = trim(input);

    int indent = 4;
    if (!input.empty()) {
        // 仅接受合理的正整数缩进，非法输入回退默认值
        if (!isDigits(input) || input.size() > 6) {
            std::cout << "Invalid indent, using 4.\n";
        } else {
            indent = std::stoi(input);
        }
    }
    std::cout << JsonSerializer::dumpPretty(app.m_document, indent) << '\n';
}

void actionQueryValue(AppState& app) {
    if (!requireDocument(app)) {
        return;
    }
    std::cout << "\n[Query value by path]\n"
              << "Path syntax: key.key / key[index], e.g. user.address.city, items[0].name\n"
              << "Path (empty = root): ";
    std::string path;
    if (!readLine(path)) {
        return;
    }
    path = trim(path);

    std::string error;
    JsonValue* target = resolvePath(app.m_document, path, error);
    if (target == nullptr) {
        std::cout << "Path error: " << error << '\n';
        return;
    }
    std::cout << "Type  : " << jsonTypeName(target->type()) << '\n';
    std::cout << "Value : " << JsonSerializer::dump(*target) << '\n';
}

void actionModifyValue(AppState& app) {
    if (!requireDocument(app)) {
        return;
    }
    std::cout << "\n[Modify value by path]\n"
              << "Path syntax: key.key / key[index], e.g. user.age, items[0]\n"
              << "Path (empty = root): ";
    std::string path;
    if (!readLine(path)) {
        return;
    }
    path = trim(path);

    std::string error;
    JsonValue* target = resolvePath(app.m_document, path, error);
    if (target == nullptr) {
        std::cout << "Path error: " << error << '\n';
        return;
    }

    // 新值本身用 JSON 文本表达（如 42、"abc"、[1,2]），复用 Parser 完成类型转换
    std::cout << "New value (JSON text, e.g. 42, \"abc\", [1,2], {\"k\":true}): ";
    std::string value_text;
    if (!readLine(value_text)) {
        return;
    }
    value_text = trim(value_text);
    if (value_text.empty()) {
        std::cout << "No value given, nothing changed.\n";
        return;
    }

    try {
        *target = JsonParser(value_text).parse();
        std::cout << "OK, value updated.\n";
    } catch (const JsonException& e) {
        std::cout << "New value is not valid JSON: " << e.what() << '\n';
    }
}

void actionValidateJson() {
    std::cout << "\n[Validate JSON text]\n";
    const std::string text = readMultilineJson();
    if (text.empty()) {
        std::cout << "No input.\n";
        return;
    }
    try {
        JsonValue ignored = JsonParser(text).parse();  // 只验证，不使用结果
        (void)ignored;
        std::cout << "Result: VALID JSON\n";
    } catch (const JsonParseException& e) {
        // what() 已含行号/列号，无需重复拼接
        std::cout << "Result: INVALID -- " << e.what() << '\n';
    } catch (const JsonException& e) {
        std::cout << "Result: INVALID -- " << e.what() << '\n';
    }
}

void actionRoundTrip(AppState& app) {
    if (!requireDocument(app)) {
        return;
    }
    std::cout << "\n[Round-trip check: parse -> dump -> parse -> dump]\n";
    const std::string first = JsonSerializer::dump(app.m_document);

    JsonValue reparsed;
    try {
        reparsed = JsonParser(first).parse();
    } catch (const JsonException& e) {
        std::cout << "FAILED: re-parse error: " << e.what() << '\n';
        return;
    }

    // 按结构等价校验无损性：类型与内容逐节点一致即通过
    // （不能用字符串比较，两棵树的键迭代序可能不同）
    if (jsonEqual(app.m_document, reparsed)) {
        std::cout << "Result: OK, serialization is lossless.\n" << first << '\n';
    } else {
        std::cout << "Result: MISMATCH between the original and re-parsed document.\n"
                  << "first : " << first << '\n'
                  << "second: " << JsonSerializer::dump(reparsed) << '\n';
    }
}

}  // namespace

// ===================== 主循环 =====================

int main() {
    printBanner();

    AppState app;
    while (true) {
        printMenu(app);
        std::string choice;
        if (!readLine(choice)) {
            break;  // stdin 结束，按退出处理
        }
        choice = trim(choice);

        if (choice == "1") {
            actionLoadFromKeyboard(app);
        } else if (choice == "2") {
            actionLoadFromFile(app);
        } else if (choice == "3") {
            actionPrintCompact(app);
        } else if (choice == "4") {
            actionPrintPretty(app);
        } else if (choice == "5") {
            actionQueryValue(app);
        } else if (choice == "6") {
            actionModifyValue(app);
        } else if (choice == "7") {
            actionValidateJson();
        } else if (choice == "8") {
            actionRoundTrip(app);
        } else if (choice == "0" || choice == "q" || choice == "quit" || choice == "exit") {
            std::cout << "Bye!\n";
            break;
        } else if (!choice.empty()) {
            std::cout << "Unknown option: " << choice << '\n';
        }

        if (choice != "0") {
            pause();
        }
    }
    return 0;
}
