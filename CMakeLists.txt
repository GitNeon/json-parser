cmake_minimum_required(VERSION 3.28.0)

project(json-parser
    VERSION 1.0.0
    DESCRIPTION "A lightweight JSON parser written in C++"
    LANGUAGES CXX
)

# ============================================================
# 基础构建配置
# ============================================================

# 导出编译命令数据库，供 clangd、VS Code 等工具提供代码提示
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


# 设置默认构建类型为 Debug
# 仅当前项目为顶层项目时生效，作为第三方库集成时不会覆盖父工程配置
# 仅对 Ninja/Makefile 等单配置生成器生效，Visual Studio 等多配置工具以 IDE 选择为准
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
endif()

# ============================================================
# 源文件与头文件声明
# ============================================================

set(JSON_PARSER_SOURCES
    src/common.cpp
    src/lexer.cpp
    src/parser.cpp
    src/serializer.cpp
    src/value.cpp
)

set(JSON_PARSER_HEADERS
    include/common.hpp
    include/lexer.hpp
    include/parser.hpp
    include/serializer.hpp
    include/value.hpp
)

# ============================================================
# 构建核心静态库
# ============================================================

# 定义静态库目标
add_library(json-parser STATIC
    ${JSON_PARSER_SOURCES}
    ${JSON_PARSER_HEADERS}
)

# 配置头文件包含路径
# PUBLIC  : 依赖本库的其他目标也能自动访问 include 目录
# PRIVATE : 仅本库内部源码能访问 src 目录，不对外暴露
target_include_directories(json-parser
    PUBLIC
        # 构建阶段使用的头文件路径
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        # 安装部署后的头文件路径（预留能力，后续可扩展安装功能）
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# 配置 C++ 标准：要求 C++17，禁用编译器非标准扩展
# PUBLIC 表示所有依赖本库的目标，都会自动遵循 C++17 标准
target_compile_features(json-parser PUBLIC cxx_std_17)
set_target_properties(json-parser PROPERTIES
    CXX_EXTENSIONS OFF
)

# ============================================================
# 编译器警告配置
# ============================================================

# 开启高警告等级，并将警告视为错误，强制养成规范编码习惯
# 仅对当前库生效（PRIVATE），不会污染依赖本库的外部项目
if(MSVC)
    target_compile_options(json-parser PRIVATE
        /W4          # 启用高等级警告集
        /WX          # 所有警告视为编译错误
        /permissive- # 禁用微软语法扩展，严格遵循 C++ 标准
        /utf-8       # 统一源码与执行字符集为 UTF-8，避免中文乱码
    )
else()
    # GCC / Clang 通用警告配置
    target_compile_options(json-parser PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )
endif()

# ============================================================
# 构建信息输出
# ============================================================
message(STATUS "")
message(STATUS "========== Project: ${PROJECT_NAME} v${PROJECT_VERSION} ==========")
message(STATUS "Description   : ${PROJECT_DESCRIPTION}")
message(STATUS "C++ Standard  : C++${CMAKE_CXX_STANDARD} (required=${CMAKE_CXX_STANDARD_REQUIRED})")

message(STATUS "---------- Platform / Compiler ----------")
message(STATUS "System        : ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "Generator     : ${CMAKE_GENERATOR}")
message(STATUS "Compiler      : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Build Type    : ${CMAKE_BUILD_TYPE}")

message(STATUS "---------- Directories ----------")
message(STATUS "Source Dir    : ${PROJECT_SOURCE_DIR}")
message(STATUS "Binary Dir    : ${PROJECT_BINARY_DIR}")

message(STATUS "---------- Sources ----------")
message(STATUS "Source files  : ${JSON_PARSER_SOURCES}")
message(STATUS "Header files  : ${JSON_PARSER_HEADERS}")
message(STATUS "============================================================")
message(STATUS "")

# ==============================
# 可执行程序：示例入口
# ==============================
add_executable(my-json main.cpp)

# 让可执行程序链接我们刚才写的静态库 json-parser
target_link_libraries(my-json PRIVATE json-parser)


# ============================================================
# 单元测试
# ============================================================

# include(CTest) 会定义 BUILD_TESTING 选项（默认 ON），并自动调用 enable_testing()
include(CTest)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

# ============================================================
# clangd 支持：自动生成项目根 .clangd
# ============================================================

# compile_commands.json 生成在构建目录（binaryDir）内，clangd 默认无法发现，
# 会退回无任何 include 路径的 fallback 模式，导致 gtest 等头文件解析失败。
# 这里在每次 configure 时生成项目根 .clangd，指向当前构建目录。
#
# 注意事项：
#   1. 切换预设后需重新 configure（cmake --preset <name>）才会更新此文件；
#   2. 本文件为自动生成物（已加入 .gitignore），手动修改会在下次 configure 时被覆盖，
#      如需追加 clangd 自定义配置，请改用 ~/.config/clangd/config.yaml 等用户级配置。
file(RELATIVE_PATH CLANGD_DB_DIR_REL
    ${PROJECT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
file(WRITE ${PROJECT_SOURCE_DIR}/.clangd
    "# 由 CMake 配置阶段自动生成，请勿手动修改（见根 CMakeLists.txt）\n"
    "CompileFlags:\n"
    "  CompilationDatabase: ${CLANGD_DB_DIR_REL}\n")
message(STATUS "clangd         : .clangd -> ${CLANGD_DB_DIR_REL}/compile_commands.json")
