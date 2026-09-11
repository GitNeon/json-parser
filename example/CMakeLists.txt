 # ============================================================
# 手写示例代码
# ------------------------------------------------------------
# 每个示例都是独立的可执行程序（各自包含 main），
# 仅链接核心静态库 json-parser，示例之间互不依赖。
# ============================================================

# 统一创建示例目标的辅助函数：
#   add_json_example(<目标名> <源文件>)
function(add_json_example target source)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE json-parser)

    # 示例代码同样保持高警告等级（不作为错误，避免阻塞学习演示）
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra)
    endif()
endfunction()

# JsonValue 用法示例
add_json_example(example_json_value example_json_value.cpp)

# JsonLexer 用法示例
add_json_example(example_json_lexer example_json_lexer.cpp)
