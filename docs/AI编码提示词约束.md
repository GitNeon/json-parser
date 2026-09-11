# json-parser AI 编码提示词约束

> 用途：在与 AI 协作修改本仓库前，将本文件内容粘贴给 AI（或配置为 AGENTS.md / .cursorrules），约束其产出的代码与项目现状保持一致。

## 1. 项目背景与边界

- 项目：`json-parser`，**纯手写、零第三方依赖**的 C++ JSON 解析器库，禁止引入 nlohmann/json、rapidjson 等任何 JSON 第三方库，核心逻辑只依赖 C++17 标准库。
- 功能模块：词法分析（Lexer）、递归下降语法解析（Parser）、数据存储（JsonValue，基于 `std::variant`）、序列化（Serializer）、异常容错。
- 跨平台目标：Windows 原生 / WSL2 / Linux 一套代码编译运行，代码须保持平台中立，不写平台专有代码（必要时用宏隔离）。
- 目标产物：静态库 `json-parser`、示例程序 `my-json`、单元测试可执行文件（如 `test_json_value`）。

## 2. 版本硬性约束（不得突破）

| 项 | 约束 |
|---|---|
| C++ 标准 | **C++17 强制**（`cxx_std_17` + `CXX_EXTENSIONS OFF`，MSVC 另加 `/permissive-`）；**禁止使用 C++20/23 特性**（concepts、ranges、span、format 等） |
| CMake | **最低 3.28.0**；预设文件为 `CMakePresets.json`（version 6），修改 CMakeLists 后须提示用户重新 configure |
| 生成器 | Ninja（多配置预设按需使用） |
| 编译器 | GCC / Clang / MSVC（clang-cl）三线兼容；MSVC 需保证源码为 UTF-8 |
| 测试框架 | GoogleTest 1.14.0，**离线来源**：`thirdparty/googletest-1.14.0.zip`（FetchContent 指向本地包，不要改回在线 URL） |
| 源码字符集 | 统一 UTF-8，中文注释可正常编译；**错误消息与一切用户可见提示一律英文** |

## 3. 常用构建命令

```bash
cmake --preset gcc-debug    # 或 clang-debug / gcc-release / clang-release
cmake --build --preset gcc-debug
ctest --test-dir build/gcc-debug    # 或 ctest --preset gcc-debug
```

- 构建目录一律为 `build/<presetName>`，已入 `.gitignore`。
- `.clangd` 由 CMake 自动生成（指向 compile_commands.json），**禁止手动修改**；切换预设后需重新 configure。
- 根目录 `CMakeLists.txt` 中库与头文件的源列表（`JSON_PARSER_SOURCES` / `JSON_PARSER_HEADERS`）是集中声明的，**新增/删除源文件必须同步更新**。

## 4. 目录结构规则

```
include/    # 对外公开头文件（声明 + 轻量 inline 实现），含 common.hpp（MUST_USE / MAYBE_UNUSED 宏）
src/        # 核心实现（common/lexer/parser/serializer/value 对应 .cpp）
tests/      # 单元测试：tests/*.cpp 为 GoogleTest 用例，tests/standalone/*.cpp 为手写 main 测试（都须在对应 CMakeLists.txt 注册目标并 add_test）
main.cpp    # Demo 入口，不得放入库逻辑
example/    # 手写示例代码：每个示例独立 main、独立可执行、互不依赖（在 example/CMakeLists.txt 注册）
data/       # JSON 测试/示例数据
docs/       # 设计与笔记文档，功能变更时同步更新
thirdparty/ # 离线依赖包（googletest zip），不要改动
```

## 5. 命名规范（与 .clang-tidy 对齐，写码即遵守）

| 对象 | 规范 | 示例 |
|---|---|---|
| 类/结构体/枚举/类型别名 | 大驼峰 PascalCase | `JsonValue`、`TokenType` |
| 函数 / 成员函数 | 小驼峰 camelBack | `nextToken()`、`pushBack()`、`toString()` |
| 普通局部变量 | snake_case | `source`、`line_count` |
| 类成员变量 | snake_case + `m_` 前缀 | `m_source`、`m_pos`、`m_has_peek` |
| 枚举 | `enum class` + 显式底层类型（如 `std::uint8_t`），枚举值 PascalCase | `enum class TokenType : std::uint8_t { LBrace, ... }` |
| 宏 | 全大写 + 下划线 | `MUST_USE`、`MAYBE_UNUSED` |
| 异常类型 | PascalCase 且语义清晰 | `JsonValueException` |

## 6. 语言与语法规范

- 头文件统一 `#pragma once`；声明与实现分离：公开接口声明在 `include/*.hpp`，实现放 `src/*.cpp`（小型纯访问器可在头文件内联）。
- 函数返回结果要求调用方必须检查时，用项目宏 `MUST_USE`（即 `[[nodiscard]]`）标记，**不要重复展开为 `[[nodiscard]]`**，引用 `common.hpp` 即可。
- 单参数构造函数一律 `explicit`；语义为只读、不改写的接口加 `const`；不抛异常的接口加 `noexcept`。
- 资源与值传递：优先 `const&` 传参、按值 + `std::move` 转移，禁止裸 `new/delete`，禁止裸指针所有权转移；优先 `std::variant`、`std::vector`、`std::unordered_map` 等标准容器。
- 数字统一用 `double` 存储（与现有 `JsonValue` 一致）；下标/长度用 `std::size_t`。
- 类型转换必须显式 `static_cast`，禁止 C 风格强转和隐式窄化。
- 异常处理：业务错误抛项目自定义异常（继承 `std::runtime_error`，如 `JsonValueException`），错误消息用**英文**，附上下文（行号/列号/字符）便于定位。
- 语言规范（硬性）：
  - 代码标识符一律英文；
  - **错误消息、日志、用户可见提示、测试失败信息、CMake 的 `message()`/预设 `description`/`displayName` 等一律英文**，禁止在字符串字面量（`"..."`）中出现中文，避免不同编译器、终端、CI 日志的编码差异导致乱码；
  - 源码注释可继续使用中文（本项目注释以中文为主），但新增注释应与所在文件风格保持一致；
  - 对外展示的示例程序、Demo 的 `std::cout` 输出同样使用英文。
- 禁止全局可变状态、禁止 goto、禁止在循环中做不必要的字符串拼接（见 clang-tidy performance 组）。
- 头文件包含顺序与排序交给 clang-format 的 `SortIncludes`，新增 include 后自觉运行格式化。
- 不引入任何运行时第三方库；只允许使用 C++17 标准库 + 项目自身头文件。

## 7. 格式与静态检查（交付前必须自检）

- 代码风格依据 `.clang-format`（Google 基底 + 定制）：**4 空格缩进、行宽 100**、左大括号与语句同行、指针/引用靠左、控制语句关键字后留空格、尾随注释对齐、禁止单行 if、函数参数不打包。写完后应运行 clang-format 校验（`clang-format -i` 可接受，但不要改动与任务无关的既有代码）。
- 静态检查依据 `.clang-tidy`：bugprone / modernize / performance / readability / google / cppcoreguidelines / clang-analyzer 全组启用；命名、复杂度（认知复杂度 ≤ 25）等按文件配置执行。**不要擅自修改 .clang-format / .clang-tidy**。
- 编译告警红线：MSVC 为 `/W4 /WX /permissive- /utf-8`，GCC/Clang 为 `-Wall -Wextra -Wpedantic -Werror`。**任何新增代码不得产生编译警告，否则视为错误**。
- 禁止引入新的 clang-tidy 告警；若任务确实需要放宽某规则，先与用户确认，不改配置文件。

## 8. 修改与交付纪律

- 小步修改：只改与任务相关的文件；不重构、不重排既有代码，除非任务明确要求。
- 头文件/公共 API 变更时：同步检查调用方（main.cpp、src/*.cpp、tests/*）并更新。
- 新增解析能力、数据操作或序列化行为时：**必须配套 GoogleTest 用例**（在 `tests/` 下新增或扩展，并在 `tests/CMakeLists.txt` 注册目标与 `add_test`）；边界与异常场景（空输入、非法字符、越界、超长、转义字符）要有覆盖。
- 测试分两条线：GoogleTest 用例统一放 `tests/*.cpp`，用 `GTest::gtest_main` 提供 main；手写 main 测试放 `tests/standalone/`，自带 main 并返回 `ctx.summary()` 退出码（详见 `docs/单元测试流程笔记.md` 第七节）。
- 功能/行为变更时：同步更新 `README.md` 与 `docs/` 相关文档；新增数据样例放 `data/`。
- 完成任务的标准：格式化通过 → 零编译警告 → 测试全绿 → 无新增 clang-tidy 告警 → 文档同步。默认运行 Debug 预设（`gcc-debug` 或 `clang-debug`）验证。
- 工具链差异：生成的代码必须同时能被 MSVC（/permissive-）与 GCC/Clang（-Wpedantic -Werror）接受，避免使用编译器专有语法。
