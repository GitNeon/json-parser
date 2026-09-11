# ============================================================
# 测试框架：GoogleTest（本地离线 zip）
# ============================================================
include(FetchContent)

FetchContent_Declare(
    googletest
    # 网络原因改用本地包：URL 指向 thirdparty/ 下的 zip，配置阶段直接解压，不联网
    # 恢复在线拉取时改回: URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    URL ${PROJECT_SOURCE_DIR}/thirdparty/googletest-1.14.0.zip
)
FetchContent_MakeAvailable(googletest)

# ============================================================
# 测试目标：JsonValue
# ============================================================

# value.hpp 是 header-only，但链接 json-parser 后会自动获得
# include 目录（PUBLIC 传递）和 C++17 标准，后续测试 lexer/parser 也直接复用
add_executable(test_json_value test_json_value.cpp)

target_link_libraries(test_json_value PRIVATE
    json-parser
    GTest::gtest_main   # 提供 main()，测试文件里不用自己写
)

# ============================================================
# 测试目标：JsonLexer
# ============================================================
add_executable(test_json_lexer test_json_lexer.cpp)

target_link_libraries(test_json_lexer PRIVATE
    json-parser
    GTest::gtest_main   # 提供 main()，测试文件里不用自己写
)

# ============================================================
# 注册到 CTest
# ============================================================

# 方式一）：自动发现每个 TEST 用例，ctest 里能看到每个用例的通过/失败
# include(GoogleTest)
# gtest_discover_tests(test_json_value)

# 方式二：每个测试可执行文件注册为一条测试
add_test(NAME test_json_value COMMAND test_json_value)
add_test(NAME test_json_lexer COMMAND test_json_lexer)

# 打上 gtest 标签，便于 ctest -L gtest 只跑 GoogleTest 用例
set_tests_properties(test_json_value test_json_lexer PROPERTIES LABELS "gtest")
