# GoogleTest 测试框架入门教程

> 本教程基于本项目实际配置编写（GoogleTest 1.14.0 + CMake FetchContent + CTest），
> 所有示例均可在本项目的 `tests/` 目录下直接使用。

---

## 目录

1. [GoogleTest 是什么](#1-googletest-是什么)
2. [核心概念：三层结构](#2-核心概念三层结构)
3. [断言：测试的基石](#3-断言测试的基石)
4. [编写第一个测试：TEST 宏](#4-编写第一个测试test-宏)
5. [测试夹具：TEST_F 宏](#5-测试夹具test_f-宏)
6. [参数化测试：TEST_P 宏](#6-参数化测试test_p-宏)
7. [本项目是如何接入 GoogleTest 的](#7-本项目是如何接入-googletest-的)
8. [编译与运行测试](#8-编译与运行测试)
9. [常用场景速查](#9-常用场景速查)
10. [常见坑与排错](#10-常见坑与排错)

---

## 1. GoogleTest 是什么

GoogleTest（简称 gtest）是 Google 开源的 C++ 单元测试框架，也是 C++ 生态中事实上的标准测试框架。

它解决的核心问题：**让你用统一的写法描述"输入是什么、期望输出是什么"，框架自动执行并汇报结果**。没有它，你得自己写 `if (a == b) printf("通过")` 这类裸代码，用例多了以后管理、运行、统计都是灾难。

一个最小可运行的测试长这样（来自本项目 `tests/test_json_value.cpp`）：

```cpp
#include <gtest/gtest.h>
#include "value.hpp"

TEST(JsonValueTest, DefaultIsNull) {
    JsonValue v;
    EXPECT_TRUE(v.isNull());
    EXPECT_EQ(v.type(), JsonType::Null);
}
```

不需要写 `main()`（后面解释为什么），编译运行后输出：

```text
[==========] Running 1 test from 1 test suite.
[ RUN      ] JsonValueTest.DefaultIsNull
[       OK ] JsonValueTest.DefaultIsNull (0 ms)
[==========] 1 test from 1 test suite ran. (1 ms total)
[  PASSED  ] 1 test.
```

---

## 2. 核心概念：三层结构

GoogleTest 的组织结构是三层，从大到小：

| 层级 | 概念 | 类比 | 数量关系 |
|---|---|---|---|
| 1 | 测试套件（Test Suite） | 文件夹 | 一个套件包含多个测试 |
| 2 | 测试（Test） | 文件 | 一个测试包含多个断言 |
| 3 | 断言（Assertion） | 文件里的每一行检查 | 最小单位，判定通过/失败 |

以上面的例子对应：

- `JsonValueTest` → 测试套件名（建议按"被测类名 + Test"命名）
- `DefaultIsNull` → 测试名（描述被测行为）
- `EXPECT_TRUE(...)`、`EXPECT_EQ(...)` → 断言

**命名建议**：测试名用"能读成一句话"的英文，如 `ParseEmptyObjectReturnsEmpty`、`InvalidUtf8ThrowsException`。将来输出失败列表时，名字本身就是文档。

---

## 3. 断言：测试的基石

### 3.1 两种断言：EXPECT 与 ASSERT

每个断言宏都有 `EXPECT_` 和 `ASSERT_` 两个版本：

| 版本 | 失败后行为 | 适用场景 |
|---|---|---|
| `EXPECT_*` | 标记失败，**继续执行**后面的断言 | 大多数场景（默认用它） |
| `ASSERT_*` | 标记失败，**立即终止当前测试** | 后续代码依赖该结果（如指针非空检查） |

```cpp
TEST(Example, ExpectVsAssert) {
    JsonValue* v = parse("{\"a\":1}");
    ASSERT_NE(v, nullptr);  // 若 v 为空，后面的 v->type() 会崩，必须用 ASSERT
    EXPECT_EQ(v->type(), JsonType::Object);  // 失败也无所谓，继续检查其他方面
    EXPECT_EQ(v->size(), 1);
}
```

经验法则：**检查"继续执行的前提条件"用 ASSERT，检查"被测行为的结果"用 EXPECT**。

### 3.2 常用断言速查表

| 断言宏 | 含义（通过条件） |
|---|---|
| `EXPECT_TRUE(x)` / `EXPECT_FALSE(x)` | x 为真 / 假 |
| `EXPECT_EQ(a, b)` / `EXPECT_NE(a, b)` | a == b / a != b |
| `EXPECT_LT(a, b)`、`EXPECT_LE`、`EXPECT_GT`、`EXPECT_GE` | a < b、a <= b、a > b、a >= b |
| `EXPECT_STREQ(a, b)` | C 字符串（`const char*`）内容相等 |
| `EXPECT_FLOAT_EQ(a, b)` / `EXPECT_DOUBLE_EQ(a, b)` | 浮点数近似相等（容忍 4 ULP） |
| `EXPECT_NEAR(a, b, tol)` | \|a - b\| <= tol（自定义容差） |
| `EXPECT_THROW(stmt, ExType)` | 语句抛出指定类型异常 |
| `EXPECT_NO_THROW(stmt)` | 语句不抛异常 |
| `EXPECT_ANY_THROW(stmt)` | 语句抛出任意异常 |
| `EXPECT_DEATH(stmt, regex)` | 语句导致进程崩溃（死亡测试） |

**重点提醒——浮点数比较**：

```cpp
// ❌ 错误：浮点数直接 EXPECT_EQ，0.1+0.2 != 0.3，大概率误报失败
EXPECT_EQ(0.1 + 0.2, 0.3);

// ✅ 正确：用近似比较
EXPECT_DOUBLE_EQ(0.1 + 0.2, 0.3);   // 严格近似（4 ULP）
EXPECT_NEAR(0.1 + 0.2, 0.3, 1e-9);  // 自定义容差
```

本项目的 JSON Number 解析测试，凡涉及 double 一律用 `EXPECT_DOUBLE_EQ` 或 `EXPECT_NEAR`。

### 3.3 失败时输出自定义信息

`<<` 可以给任何断言附加说明，失败时会打印出来：

```cpp
EXPECT_EQ(token.type, TokenType::String)
    << "第 " << i << " 个 token 应为字符串，实际输入: " << input;
```

善用这个功能，可以省掉失败后反复加打印调试的时间。

### 3.4 直接标记失败

```cpp
TEST(Parse, InvalidInput) {
    try {
        parse("this is not json");
        FAIL() << "非法输入应当抛异常，但这里没有抛";  // 无条件失败
    } catch (const JsonException& e) {
        EXPECT_STREQ(e.what(), "unexpected token");
    }
}
```

> 通常更推荐直接用 `EXPECT_THROW`，上面这种 try-catch 写法只在需要检查异常细节时使用。

---

## 4. 编写第一个测试：TEST 宏

`TEST(套件名, 测试名)` 是最基础的写法，适合**被测对象构造简单、无共享状态**的场景：

```cpp
// tests/test_lexer.cpp
#include <gtest/gtest.h>
#include "lexer.hpp"

// 同一个套件下写多个测试，每个测试相互独立、执行顺序不保证
TEST(LexerTest, EmptyInputYieldsEof) {
    Lexer lexer("");
    EXPECT_EQ(lexer.next().type, TokenType::EndOfFile);
}

TEST(LexerTest, SkipsWhitespace) {
    Lexer lexer("  [ 1 , 2 ]");
    std::vector<Token> tokens = lexer.tokenize();
    EXPECT_EQ(tokens.size(), 5);
}

TEST(LexerTest, RejectsIllegalCharacter) {
    Lexer lexer("@");
    EXPECT_THROW(lexer.tokenize(), JsonException);
}
```

**关键认知：每个 TEST 之间是完全隔离的**。各自有独立的局部变量，一个测试失败不影响其他测试的执行。所以不要在测试间共享可变全局状态。

---

## 5. 测试夹具：TEST_F 宏

当多个测试需要**相同的初始化/清理代码**时，用测试夹具（Test Fixture）消除重复：

```cpp
#include <gtest/gtest.h>
#include "parser.hpp"

// 第一步：定义夹具类，必须继承 ::testing::Test
class ParserTest : public ::testing::Test {
protected:
    // SetUp 在每个测试前自动调用（相当于"构造前置逻辑"）
    void SetUp() override {
        parser_ = std::make_unique<Parser>(sample_);
    }

    // TearDown 在每个测试后自动调用（相当于"析构后置逻辑"）
    // 没有清理需求时可以不写，RAII 会兜底
    void TearDown() override {}

    std::string sample_ = R"({"name": "json", "version": 1})";
    std::unique_ptr<Parser> parser_;
};

// 第二步：用 TEST_F 代替 TEST，第一个参数必须是夹具类名
TEST_F(ParserTest, ParsesTopLevelObject) {
    JsonValue v = parser_->parse();
    EXPECT_EQ(v.type(), JsonType::Object);
}

TEST_F(ParserTest, ParsesStringValue) {
    JsonValue v = parser_->parse();
    EXPECT_EQ(v["name"].asString(), "json");
}
```

### 必须理解的规则

1. **夹具类继承 `::testing::Test`**，成员放在 `protected:` 下（gtest 子类需要访问）。
2. **每个测试都会新建一份夹具对象**：`SetUp` → 测试体 → `TearDown`，测试之间天然隔离，不会互相污染。
3. `TEST_F` 的第一个参数是**类名**而不是随意取的套件名，且同一夹具类下的所有 `TEST_F` 构成一个套件。
4. SetUp/TearDown 里**不要用 ASSERT**——失败不代表测试失败，应改用 `GTEST_FAIL()` 或让异常自然抛出。

### SetUp 与构造函数的区别

| 方式 | 调用时机 | 能否感知派生类成员 | 推荐 |
|---|---|---|---|
| 构造函数 | 对象创建时 | 否（派生类成员未初始化） | 简单初始化 |
| `SetUp()` | 每个测试运行前 | 是 | 需要用到派生类状态时 |

新手记不住区别就统一用 `SetUp()`，永远不会错。

---

## 6. 参数化测试：TEST_P 宏

当**同一个测试逻辑要跑 N 组不同数据**时，TEST_P 让你只写一遍逻辑：

```cpp
#include <gtest/gtest.h>
#include "lexer.hpp"

// 第一步：定义参数化夹具，继承 ::testing::TestWithParam<T>，T 是参数类型
class NumberParseTest : public ::testing::TestWithParam<std::pair<std::string, double>> {
protected:
    Lexer lexer{GetParam().first};  // GetParam() 拿到当前这组参数
};

// 第二步：用 TEST_P 写测试逻辑（只写一遍）
TEST_P(NumberParseTest, ParsesNumberLiteral) {
    const auto& [input, expected] = GetParam();
    JsonValue v = parse(input);
    EXPECT_DOUBLE_EQ(v.asNumber(), expected);
}

// 第三步：注册参数，每组参数会展开成独立的测试用例
INSTANTIATE_TEST_SUITE_P(
    NumberLiterals,            // 实例名（会出现在测试名前缀里）
    NumberParseTest,           // 夹具类名
    ::testing::Values(
        std::make_pair("0", 0.0),
        std::make_pair("-1", -1.0),
        std::make_pair("3.14", 3.14),
        std::make_pair("1e10", 1e10),
        std::make_pair("-2.5E-3", -2.5e-3)
    )
);
```

运行后输出中会看到 5 个测试：

```text
[ RUN      ] NumberLiterals/NumberParseTest.ParsesNumberLiteral/0   ("0")
[ RUN      ] NumberLiterals/NumberParseTest.ParsesNumberLiteral/1   ("-1")
...
```

**判断什么时候用 TEST_P**：当你发现自己在复制粘贴同一个 `TEST` 然后只改里面的常量时，就该用 TEST_P 了。JSON 解析器有大量边界输入（空串、深层嵌套、转义字符），这是 TEST_P 的主战场。

---

## 7. 本项目是如何接入 GoogleTest 的

本项目使用 **CMake FetchContent** 方式集成，相关代码在 `tests/CMakeLists.txt`：

```cmake
include(FetchContent)

FetchContent_Declare(
    googletest
    # 网络原因改用本地包：URL 指向 thirdparty/ 下的 zip，配置阶段直接解压，不联网
    # 恢复在线拉取时改回: URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    URL ${PROJECT_SOURCE_DIR}/thirdparty/googletest-1.14.0.zip
)
FetchContent_MakeAvailable(googletest)

add_executable(test_json_value test_json_value.cpp)

target_link_libraries(test_json_value PRIVATE
    json-parser
    GTest::gtest_main   # 提供 main()，测试文件里不用自己写
)

add_test(NAME test_json_value COMMAND test_json_value)
```

逐行解读关键点：

| 配置 | 作用 |
|---|---|
| `FetchContent_MakeAvailable(googletest)` | 把 GoogleTest 作为子项目下载并编译，产出 `GTest::gtest`、`GTest::gtest_main` 等目标 |
| `URL .../googletest-1.14.0.zip` | 当前指向本地 zip（离线可用）；注释里保留了在线拉取地址，网络好时可切回 |
| `GTest::gtest_main` | **自动提供 `main()` 函数**，这就是测试文件不用写 main 的原因。如果链接 `GTest::gtest`（不带 main），就必须自己写 main 并调用 `RUN_ALL_TESTS()` |
| `json-parser` | 链接被测库，PUBLIC 的 include 目录和 C++17 标准会自动传递给测试 |
| `add_test(NAME ... COMMAND ...)` | 把测试可执行文件注册进 CTest，之后可以用 `ctest` 命令统一驱动 |

### 新增一个测试文件的完整步骤

以新增 `test_lexer.cpp` 为例：

1. 在 `tests/` 下创建 `test_lexer.cpp`，写测试代码；
2. 在 `tests/CMakeLists.txt` 中仿照现有写法追加：

```cmake
add_executable(test_lexer test_lexer.cpp)

target_link_libraries(test_lexer PRIVATE
    json-parser
    GTest::gtest_main
)

add_test(NAME test_lexer COMMAND test_lexer)
```

3. 重新 configure + build，即可被 `ctest` 收集到。

> 根目录 `CMakeLists.txt` 中 `include(CTest)` + `if(BUILD_TESTING)` 的组合意味着：
> 加 `-DBUILD_TESTING=OFF` 可以完全不编译测试（加快纯开发构建）。

---

## 8. 编译与运行测试

### 8.1 构建并运行

```bash
# 在项目根目录，用预设配置并构建（本项目的预设见 CMakePresets.json）
cmake --preset <预设名>
cmake --build --preset <预设名>

# 方式一：直接运行测试可执行文件（最直观，推荐日常使用）
./build/tests/test_json_value

# 方式二：通过 CTest 运行（会汇总所有 add_test 注册的测试）
ctest --test-dir build
```

### 8.2 命令行过滤：只跑你想跑的

测试可执行文件支持丰富的命令行参数（`--help` 查看全部），最常用的是 `--gtest_filter`：

```bash
# 只跑 ParserTest 套件下的所有测试
./test_json_parser --gtest_filter=ParserTest.*

# 只跑某一个具体测试
./test_json_parser --gtest_filter=LexerTest.RejectsIllegalCharacter

# 排除某些测试（跑全部，但跳过 DeathTest 套件）
./test_json_parser --gtest_filter=-DeathTest.*

# 组合：跑 A 套件和 B 套件，但排除 B 中的某一个
./test_json_parser --gtest_filter=A.*:B.*-B.SlowTest

# 重复跑 100 次，检验偶发失败（flaky test）
./test_json_value --gtest_repeat=100

# 测试失败时立即停下（调试首个失败用例）
./test_json_value --gtest_fail_fast
```

过滤语法：`套件.测试` 匹配，`:` 分隔多个正向过滤，`-` 开头表示排除。

### 8.3 读懂测试输出

```text
[==========] Running 3 tests from 2 test suites.      ← 总览：要跑多少个测试
[----------] 2 tests from LexerTest                   ← 一个套件开始
[ RUN      ] LexerTest.SkipsWhitespace                ← 单个测试开始
tests/test_lexer.cpp:12: Failure                       ← 失败定位：文件:行号
Expected equality of these values:
  tokens.size()
    Which is: 7
  5
[  FAILED  ] LexerTest.SkipsWhitespace (0 ms)          ← 该测试失败
[----------] 2 tests from LexerTest (1 ms total)
[  PASSED  ] 2 tests.                                  ← 全局统计
[  FAILED  ] 1 test, listed below:
[  FAILED  ] LexerTest.SkipsWhitespace
```

排错三板斧：**看 `文件:行号` → 看 Expected/Actual 两行 → 定位断言参数**。

---

## 9. 常用场景速查

### 9.1 浮点数比较

```cpp
EXPECT_DOUBLE_EQ(v.asNumber(), 3.14);          // 默认选择
EXPECT_NEAR(computed, expected, 1e-6);         // 误差有物理含义时（如迭代计算）
```

### 9.2 异常测试

```cpp
// 抛出指定异常
EXPECT_THROW(parse("{"), JsonException);
// 不抛任何异常
EXPECT_NO_THROW(parse("{}"));
// 检查异常消息内容
try {
    parse("[1,]");
    FAIL() << "应当抛出异常";
} catch (const JsonException& e) {
    EXPECT_NE(std::string(e.what()).find("expected value"), std::string::npos);
}
```

### 9.3 容器/字符串内容检查

```cpp
// std::string 之间可以直接用 EXPECT_EQ（注意别拿它比较 const char*，用 STREQ）
std::string out = serialize(v);
EXPECT_EQ(out, R"({"a":1})");

// 容器大小
EXPECT_EQ(arr.size(), 3);
EXPECT_TRUE(arr.empty());

// 逐元素检查
for (size_t i = 0; i < arr.size(); ++i) {
    EXPECT_EQ(arr[i].asNumber(), i + 1) << "index = " << i;
}
```

### 9.4 临时禁用某个测试

测试名前加 `DISABLED_` 前缀，该测试会被跳过（不会误报失败）：

```cpp
TEST(LexerTest, DISABLED_DepthLimitNotImplementedYet) { ... }
```

配合命令行 `--gtest_also_run_disabled_tests` 可强制运行被禁用的测试。

### 9.5 测试死亡（进程崩溃）场景

```cpp
// 用于断言"这段代码会崩溃/abort"，多用于检查防御性退出
EXPECT_DEATH(serialize(nullptr), ".*");
```

初学阶段用到的不多，知道有这个能力即可。

---

## 10. 常见坑与排错

| 现象/错误 | 原因与解决 |
|---|---|
| `undefined reference to 'main'` | 链接了 `GTest::gtest` 而不是 `GTest::gtest_main`，或两个都没链 |
| 中文注释编译报错（MSVC） | 本项目已配置 `/utf-8`，若新增编译单元仍报错，确认文件编码为 UTF-8（无 BOM） |
| 测试里改了静态变量，影响其他测试 | 测试间共享全局/静态状态导致污染；把状态移入夹具类成员（每个测试独立一份） |
| `EXPECT_EQ(0.1+0.2, 0.3)` 失败 | 浮点精度问题，改用 `EXPECT_DOUBLE_EQ` 或 `EXPECT_NEAR`（见 9.1） |
| ctest 找不到新加的测试 | 忘记重新 configure；跑一次 `cmake --preset <预设名>` 再构建 |
| `TEST_F` 编译报错说没有夹具定义 | `TEST_F` 第一个参数必须是已定义的夹具**类名**，且该类必须继承 `::testing::Test` |
| 参数化测试没跑起来 | 忘记写 `INSTANTIATE_TEST_SUITE_P`，或 `TEST_P` 与 `TEST_F` 混用（参数化必须 `TestWithParam` + `TEST_P` 成对出现） |
| FetchContent 下载失败 | 本项目已改为本地 zip（`thirdparty/googletest-1.14.0.zip`），正常不会联网；若手动切回在线 URL，需保证网络可达或配置代理 |
| 同一断言失败多次刷屏 | 给断言加 `<< "index = " << i` 上下文信息，快速定位是哪组数据（见 3.3） |

---

## 附：推荐学习路径

1. **先用好 `TEST` + `EXPECT_*`**：给 `Lexer`、`Parser`、`Serializer` 各写 5~10 个基础用例；
2. **发现重复后引入 `TEST_F`**：把构造样例 JSON 的公共代码提取到夹具的 `SetUp`；
3. **边界数据成组出现后引入 `TEST_P`**：如数字字面量、转义字符、非法输入集合；
4. 需要了解 mock（隔离外部依赖）时，再学习 GoogleTest 的姊妹库 **GoogleMock**（同一仓库，集成时多链一个 `GTest::gmock_main` 即可）。

官方文档：<https://google.github.io/googletest/>
