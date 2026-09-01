#pragma once

// 强制调用者检查函数的返回值
#define MUST_USE [[nodiscard]]

// 变量/函数/参数虽然没有使用，但是需要保留，禁止编译器发出警告
#define MAYBE_UNUSED [[maybe_unused]]
