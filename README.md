# C\+\+ 手写跨平台JSON解析器 \- 项目说明文档

## 1\. 项目简介

本项目是**纯手写、零第三方依赖**的 C\+\+17 JSON 解析器库，不使用 nlohmann/json、rapidjson 等开源库，从零实现 JSON 词法分析、语法解析、数据存储、序列化与异常容错能力。

项目主打**跨平台一致性**，一套代码可在 **WSL2、Windows 原生、Linux 真机** 编译运行，采用 CMake\+Ninja 现代化工具链，配合 VS Code 统一开发、构建、调试、代码规范流程，适合 C\+\+ 工程实践、编译原理入门与库开发能力训练。

核心特点：

- ✅ 零第三方库依赖，仅依赖 C\+\+17 标准库

- ✅ 平台兼容：Windows / WSL2 / Linux

- ✅ 高性能 Ninja 增量构建，编译速度快

- ✅ 完整工程化：代码格式化、静态检查、内存检测、单元测试

- ✅ 严格编译告警约束，代码健壮性强

## 2\. 工具链与开发环境选型

### 2\.1 整体技术栈

统一开发方案：**VS Code \+ CMake \+ Ninja \+ C\+\+17**

- **编译标准**：C\+\+17（强制标准，禁用编译器扩展）

- **构建系统**：CMake 3\.28\+（顶层跨平台配置）

- **构建后端**：Ninja（默认首选，替代 Make，增量构建高效）

- **编译器支持**：Clang（主推）、GCC、MSVC（全平台适配）

- **调试工具**：GDB、AddressSanitizer、UndefinedBehaviorSanitizer

- **代码规范**：Clang\-Format、Clang\-Tidy

- **测试框架**：GoogleTest（CMake 在线拉取，无需本地安装）

### 2\.2 多环境适配方案

|运行环境|编译器|构建工具|调试器|构建目录|
|---|---|---|---|---|
|WSL2 Ubuntu（主力开发）|Clang\+\+ / G\+\+|CMake \+ Ninja|GDB \+ ASAN|build\-wsl|
|Windows 原生 Clang|LLVM Clang\+\+|CMake \+ Ninja|GDB \+ ASAN|build\-win\-clang|
|Windows 原生 MSVC|MSVC cl\.exe|CMake \+ Ninja|VS Debugger|build\-win\-msvc|
|Linux 真机/服务器|Clang\+\+ / G\+\+|CMake \+ Ninja|GDB \+ ASAN|build\-linux|

### 2\.3 WSL2 环境依赖安装（主力环境）

Ubuntu/Debian 一键安装所有开发依赖：

```bash
sudo apt update
sudo apt install build-essential cmake ninja-build clang llvm lld gdb clang-format clang-tidy doxygen git
```

### 2\.4 VS Code 必备插件

- C/C\+\+ Extension Pack：代码智能提示、语法解析、调试

- CMake Tools：工程配置、构建、切换编译套件

- Clang\-Format：代码自动格式化

- WSL：远程连接 WSL2 开发环境

- GitLens：版本控制辅助

## 3\. 项目功能需求

### 3\.1 核心基础功能

- **词法解析（Lexer）**：读取 JSON 字符串，拆分合法 Token（字符串、数字、布尔值、空值、括号、逗号、冒号等），过滤空白字符，识别非法字符并抛出异常

- **语法解析（Parser）**：基于递归下降算法，将 Token 流解析为内存数据结构，支持嵌套对象、嵌套数组解析

- **数据类型支持**：完整支持 JSON 六大基础类型：Object、Array、String、Number、Bool、Null

- **数据访问接口**：提供统一 API 读取、修改、新增、删除 JSON 节点数据

- **序列化功能**：将内存中的 JSON 数据结构，重新序列化为标准 JSON 字符串（支持压缩格式与格式化缩进格式）

### 3\.2 进阶容错与调试功能

- JSON 语法错误精准报错：提示错误行数、错误字符、错误类型

- 内存安全检测：支持 ASAN/UBSAN 检测内存越界、野指针、未定义行为

- 严格编译告警：所有编译警告视为错误，规避不规范代码

- 边界容错：支持空 JSON、空对象、空数组、转义字符解析、超长字符串解析

### 3\.3 工程功能

- 跨平台编译构建，多环境构建目录隔离，互不污染

- 自动化单元测试，覆盖解析、序列化、异常场景

- 统一代码风格与静态检查，保证代码规范性

## 4\. 项目目录结构

采用标准 C\+\+ 库工程结构，分离源码、头文件、测试、配置文件，结构清晰易维护：

```plaintext
json_parser/
├── .vscode/                # VS Code 工程配置（CMake预设、编译套件、工作区配置）
├── demo                    # 参考的其他作者的JSON解析器实现
├── include/                # 对外公开头文件
│   ├── json_value.h        # JSON 数据类型定义
│   ├── json_lexer.h        # 词法分析器声明
│   ├── json_parser.h       # 语法解析器声明
│   └── json_serialize.h    # 序列化工具声明
├── src/                    # 核心源码实现
│   ├── json_value.cpp
│   ├── json_lexer.cpp
│   ├── json_parser.cpp
│   └── json_serialize.cpp
├── tests/                  # 单元测试用例
│   └── json_test.cpp
├── main.cpp                # 项目Demo入口，演示解析与序列化功能
├── CMakeLists.txt          # 全局构建配置文件
├── .clang-format           # 代码格式化规则
├── .clang-tidy             # 代码静态检查规则
├── .gitignore              # Git 忽略文件配置
└── README.md               # 项目说明文档
```

## 5\. 工程化配置说明

### 5\.1 编译标准与全局约束

- 统一启用 **C\+\+17** 标准，禁用编译器非标准扩展，保证跨平台一致性

- 默认使用 Ninja 构建生成器，提升编译与增量更新速度

- 区分平台编译参数：

  - GCC/Clang：开启 \-Wall \-Wextra \-Wpedantic \-Werror 严格告警

  - MSVC：开启 /W4 /WX 最高级别警告，警告即错误

- 可选开启 ASAN/UBSAN 内存检测，用于调试内存错误

### 5\.2 构建产物隔离规则

不同平台构建产物独立存放，避免编译缓存冲突：

- WSL2 构建产物：`build-wsl/`

- Windows Clang 构建产物：`build-win-clang/`

- Windows MSVC 构建产物：`build-win-msvc/`

- Linux 真机构建产物：`build-linux/`

### 5\.3 单元测试配置

- 通过 CMake FetchContent 自动拉取 GoogleTest，无需手动安装

- 默认开启单元测试，可通过编译选项关闭

- 支持一键编译、运行所有测试用例，校验解析器正确性

### 5\.4 代码规范配置

- `.clang-format`：统一代码缩进、括号风格、命名排版、空行规范，全员一致编码风格

- `.clang-tidy`：静态代码扫描，检测内存泄漏、裸指针滥用、语法瑕疵、低效代码

## 6\. 编译与运行教程

### 6\.1 基础构建流程（VS Code CMake Tools）

1. VS Code 连接 WSL2 远程环境

2. 选择编译套件：Clang / GCC

3. 选择构建生成器：Ninja

4. 执行 Configure 配置工程

5. 执行 Build 编译项目

6. 运行 json\_demo 可执行文件，测试基础功能

7. 运行 run\_tests 可执行文件，执行单元测试

### 6\.2 命令行手动构建

```bash
# 创建构建目录
mkdir build-wsl && cd build-wsl

# CMake 配置（Ninja + C++17）
cmake -G Ninja -DCMAKE_CXX_STANDARD=17 ..

# 编译
ninja

# 运行Demo
./json_demo

# 运行单元测试
./run_tests
```

## 7\. 项目开发规范

- 所有代码必须通过 clang\-format 格式化、clang\-tidy 静态检查

- 禁止使用第三方 JSON 库，纯手动实现核心逻辑

- 所有编译警告必须修复，严格遵循 \-Werror / /WX 约束

- 新增功能必须配套单元测试用例，保证功能稳定性

- 调试阶段默认开启 ASAN，规避内存安全问题
