#include <gtest/gtest.h>
#include "value.hpp"

#include <climits>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

/*

# 方法解读：

TEST(TestSuiteName, TestName)
- TestSuiteName：测试套件名称，用于分组相关的测试
- TestName：具体的测试名称，描述要测试什么

# 关键字前缀解读：

EXPECT: 测试继续执行，即使失败
ASSERT: 测试立即终止，如果失败

# JsonValue 测试组织（均为 JsonValueTest 套件）：

1. 构造与类型判断：六种类型的构造函数 + type() / isXxx()
2. 安全取值 asXxx：正确类型取值与引用修改
3. 类型不匹配异常：错误类型调用 asXxx / 容器操作抛 JsonValueException
4. 数组专属操作：pushBack / operator[](size_t) / at(size_t)
5. 对象专属操作：operator[](key) / at(key)
6. 通用工具：size / isEmpty / clear / visit
7. 拷贝与移动：深拷贝语义与移动后目标可用性
8. 复杂嵌套结构冒烟测试

*/

// ==================== 1. 构造与类型判断 ====================

TEST(JsonValueTest, DefaultIsNull) {
    JsonValue my_json;
    EXPECT_TRUE(my_json.isNull());
    EXPECT_EQ(my_json.type(), JsonType::Null);
}

TEST(JsonValueTest, ConstructorIsNull) {
    JsonValue my_json = JsonValue(nullptr);  // 显式调用 JsonValue(std::nullptr_t) 构造函数
    EXPECT_TRUE(my_json.isNull());
    EXPECT_EQ(my_json.type(), JsonType::Null);
}

TEST(JsonValueTest, ConstructorFromBool) {
    JsonValue t(true);
    EXPECT_TRUE(t.isBool());
    EXPECT_FALSE(t.isNull());
    EXPECT_FALSE(t.isNumber());
    EXPECT_EQ(t.type(), JsonType::Bool);
    EXPECT_TRUE(t.asBool());

    JsonValue f(false);
    EXPECT_TRUE(f.isBool());
    EXPECT_FALSE(f.asBool());
}

TEST(JsonValueTest, ConstructorFromInt) {
    JsonValue v(42);  // int 构造后以 double 存储
    EXPECT_TRUE(v.isNumber());
    EXPECT_EQ(v.type(), JsonType::Number);
    EXPECT_DOUBLE_EQ(v.asNumber(), 42.0);
}

TEST(JsonValueTest, ConstructorFromIntBoundary) {
    // 32 位 int 极值转 double 无损，用于验证 int -> double 存储转换
    JsonValue max_v(INT_MAX);
    JsonValue min_v(INT_MIN);
    EXPECT_DOUBLE_EQ(max_v.asNumber(), static_cast<double>(INT_MAX));
    EXPECT_DOUBLE_EQ(min_v.asNumber(), static_cast<double>(INT_MIN));
}

TEST(JsonValueTest, ConstructorFromDouble) {
    JsonValue v(3.14);
    EXPECT_TRUE(v.isNumber());
    EXPECT_EQ(v.type(), JsonType::Number);
    EXPECT_DOUBLE_EQ(v.asNumber(), 3.14);
}

TEST(JsonValueTest, ConstructorFromDoubleBoundary) {
    JsonValue big(1e300);
    JsonValue small(-2.5);
    EXPECT_DOUBLE_EQ(big.asNumber(), 1e300);
    EXPECT_DOUBLE_EQ(small.asNumber(), -2.5);
}

TEST(JsonValueTest, ConstructorFromString) {
    JsonValue v(std::string("hello"));
    EXPECT_TRUE(v.isString());
    EXPECT_EQ(v.type(), JsonType::String);
    EXPECT_EQ(v.asString(), "hello");
}

TEST(JsonValueTest, ConstructorFromCString) {
    JsonValue v("world");
    EXPECT_TRUE(v.isString());
    EXPECT_EQ(v.asString(), "world");

    JsonValue empty("");
    EXPECT_TRUE(empty.isString());
    EXPECT_TRUE(empty.asString().empty());
}

TEST(JsonValueTest, ConstructorFromVector) {
    std::vector<JsonValue> elements{JsonValue(1), JsonValue("two"), JsonValue(true)};
    JsonValue v(std::move(elements));
    EXPECT_TRUE(v.isArray());
    EXPECT_EQ(v.type(), JsonType::Array);
    EXPECT_EQ(v.size(), 3U);

    JsonValue empty_arr(std::vector<JsonValue>{});
    EXPECT_TRUE(empty_arr.isArray());
    EXPECT_EQ(empty_arr.size(), 0U);
}

TEST(JsonValueTest, ConstructorFromObjectMap) {
    std::unordered_map<std::string, JsonValue> obj{
        {"name", JsonValue("tom")},
        {"age", JsonValue(20)},
    };
    JsonValue v(std::move(obj));
    EXPECT_TRUE(v.isObject());
    EXPECT_EQ(v.type(), JsonType::Object);
    EXPECT_EQ(v.size(), 2U);
    EXPECT_EQ(v["name"].asString(), "tom");

    JsonValue empty_obj(std::unordered_map<std::string, JsonValue>{});
    EXPECT_TRUE(empty_obj.isObject());
    EXPECT_EQ(empty_obj.size(), 0u);
}

// ==================== 2. 安全取值 asXxx ====================

TEST(JsonValueTest, AsBoolModifiesValue) {
    JsonValue v(true);
    v.asBool() = false;  // 通过引用修改存储值
    EXPECT_TRUE(v.isBool());
    EXPECT_FALSE(v.asBool());

    const JsonValue& cv = v;
    EXPECT_FALSE(cv.asBool());  // const 重载可读
}

TEST(JsonValueTest, AsNumberModifiesValue) {
    JsonValue v(1.0);
    v.asNumber() = 9.5;
    EXPECT_DOUBLE_EQ(v.asNumber(), 9.5);

    const JsonValue& cv = v;
    EXPECT_DOUBLE_EQ(cv.asNumber(), 9.5);
}

TEST(JsonValueTest, AsStringModifiesValue) {
    JsonValue v("hello");
    v.asString() += "!";
    EXPECT_EQ(v.asString(), "hello!");

    const JsonValue& cv = v;
    EXPECT_EQ(cv.asString(), "hello!");
}

TEST(JsonValueTest, AsArrayReturnsReference) {
    JsonValue v(std::vector<JsonValue>{JsonValue(1)});
    v.asArray().push_back(JsonValue(2));
    EXPECT_EQ(v.size(), 2u);

    const JsonValue& cv = v;
    EXPECT_EQ(cv.asArray().size(), 2u);
}

TEST(JsonValueTest, AsObjectReturnsReference) {
    JsonValue v(std::unordered_map<std::string, JsonValue>{});
    v.asObject().emplace("k", JsonValue(true));
    EXPECT_TRUE(v["k"].asBool());

    const JsonValue& cv = v;
    EXPECT_EQ(cv.asObject().size(), 1u);
}

// ==================== 3. 类型不匹配异常 ====================

TEST(JsonValueTest, AsBoolThrowsOnWrongType) {
    JsonValue v(1);  // Number
    EXPECT_THROW(v.asBool(), JsonValueException);
}

TEST(JsonValueTest, AsNumberThrowsOnWrongType) {
    JsonValue v(true);  // Bool
    EXPECT_THROW(v.asNumber(), JsonValueException);

    JsonValue s("abc");  // String
    EXPECT_THROW(s.asNumber(), JsonValueException);
}

TEST(JsonValueTest, AsStringThrowsOnWrongType) {
    JsonValue v(3.14);  // Number
    EXPECT_THROW(v.asString(), JsonValueException);
}

TEST(JsonValueTest, AsArrayThrowsOnWrongType) {
    JsonValue v(std::unordered_map<std::string, JsonValue>{});  // Object
    EXPECT_THROW(v.asArray(), JsonValueException);
}

TEST(JsonValueTest, AsObjectThrowsOnWrongType) {
    JsonValue v(std::vector<JsonValue>{});  // Array
    EXPECT_THROW(v.asObject(), JsonValueException);
}

TEST(JsonValueTest, ConstAccessorsThrowOnWrongType) {
    const JsonValue v("hello");  // String
    // const 版访问器带 MUST_USE(nodiscard)，在 EXPECT_THROW 内需显式丢弃返回值以消除警告
    EXPECT_THROW(static_cast<void>(v.asBool()), JsonValueException);
    EXPECT_THROW(static_cast<void>(v.asNumber()), JsonValueException);
    EXPECT_THROW(static_cast<void>(v.asArray()), JsonValueException);
    EXPECT_THROW(static_cast<void>(v.asObject()), JsonValueException);
}

TEST(JsonValueTest, ExceptionCarriesErrorMessage) {
    // JsonValueException 继承 std::runtime_error，消息不应为空
    try {
        JsonValue v(true);
        v.asNumber();
        FAIL() << "expected JsonValueException to be thrown";
    } catch (const JsonValueException& e) {
        EXPECT_NE(e.what(), nullptr);
        EXPECT_STRNE(e.what(), "");
    }
}

// ==================== 4. 数组专属操作 ====================

TEST(JsonValueTest, ArrayPushBackAndAccess) {
    JsonValue v(std::vector<JsonValue>{});
    v.pushBack(JsonValue(1));
    v.pushBack(JsonValue("two"));
    v.pushBack(JsonValue(true));
    EXPECT_EQ(v.size(), 3u);
    EXPECT_DOUBLE_EQ(v[0].asNumber(), 1.0);
    EXPECT_EQ(v[1].asString(), "two");
    EXPECT_TRUE(v[2].asBool());
}

TEST(JsonValueTest, ArrayOperatorIndexReadWrite) {
    JsonValue v(std::vector<JsonValue>{JsonValue(0)});
    v[0] = JsonValue(10.0);  // 按下标写
    EXPECT_DOUBLE_EQ(v[0].asNumber(), 10.0);
    EXPECT_EQ(v.size(), 1u);

    v[0] = JsonValue(20.0);  // 再次覆盖
    EXPECT_DOUBLE_EQ(v[0].asNumber(), 20.0);
}

TEST(JsonValueTest, ArrayAtReturnsElement) {
    JsonValue v(std::vector<JsonValue>{JsonValue("a"), JsonValue("b"), JsonValue("c")});
    EXPECT_EQ(v.at(0).asString(), "a");
    EXPECT_EQ(v.at(2).asString(), "c");

    // at 返回引用，可修改
    v.at(1) = JsonValue("B");
    EXPECT_EQ(v.at(1).asString(), "B");

    // const 版本
    const JsonValue& cv = v;
    EXPECT_EQ(cv.at(0).asString(), "a");
}

TEST(JsonValueTest, ArrayAtThrowsOnOutOfRange) {
    JsonValue v(std::vector<JsonValue>{JsonValue(1)});
    EXPECT_THROW(v.at(1), JsonValueException);    // index == size 越界
    EXPECT_THROW(v.at(100), JsonValueException);  // 远超过界

    JsonValue empty(std::vector<JsonValue>{});
    EXPECT_THROW(empty.at(0), JsonValueException);  // 空数组
}

TEST(JsonValueTest, ConstArrayAtThrowsOnOutOfRange) {
    const JsonValue v(std::vector<JsonValue>{JsonValue(1)});
    EXPECT_THROW(static_cast<void>(v.at(5)), JsonValueException);
}

TEST(JsonValueTest, ArrayOpsThrowOnNonArray) {
    JsonValue v(true);  // Bool，非数组
    EXPECT_THROW(v[0], JsonValueException);
    EXPECT_THROW(v.at(0), JsonValueException);
    EXPECT_THROW(v.pushBack(JsonValue(1)), JsonValueException);

    const JsonValue& cv = v;
    EXPECT_THROW(static_cast<void>(cv[0]), JsonValueException);
    EXPECT_THROW(static_cast<void>(cv.at(0)), JsonValueException);
}

// ==================== 5. 对象专属操作 ====================

TEST(JsonValueTest, ObjectOperatorIndexAutoCreates) {
    JsonValue v(std::unordered_map<std::string, JsonValue>{});
    // key 不存在时自动创建并初始化为 null
    EXPECT_TRUE(v["name"].isNull());
    EXPECT_EQ(v.size(), 1u);

    // 可继续赋值覆盖
    v["name"] = JsonValue("tom");
    EXPECT_EQ(v["name"].asString(), "tom");
    EXPECT_EQ(v.size(), 1u);
}

TEST(JsonValueTest, ObjectOperatorIndexModifyExisting) {
    JsonValue v(std::unordered_map<std::string, JsonValue>{{"age", JsonValue(1)}});
    v["age"] = JsonValue(25.0);
    EXPECT_DOUBLE_EQ(v["age"].asNumber(), 25.0);
    EXPECT_EQ(v.size(), 1u);
}

TEST(JsonValueTest, ObjectAtReturnsValue) {
    JsonValue v(std::unordered_map<std::string, JsonValue>{
        {"name", JsonValue("tom")},
        {"age", JsonValue(20)},
    });
    EXPECT_EQ(v.at("name").asString(), "tom");
    EXPECT_DOUBLE_EQ(v.at("age").asNumber(), 20.0);

    v.at("age") = JsonValue(21.0);  // 引用修改
    EXPECT_DOUBLE_EQ(v.at("age").asNumber(), 21.0);

    const JsonValue& cv = v;
    EXPECT_EQ(cv.at("name").asString(), "tom");
}

TEST(JsonValueTest, ObjectAtThrowsOnMissingKey) {
    JsonValue v(std::unordered_map<std::string, JsonValue>{{"a", JsonValue(1)}});
    EXPECT_THROW(v.at("not_exist"), JsonValueException);

    const JsonValue& cv = v;
    EXPECT_THROW(static_cast<void>(cv.at("not_exist")), JsonValueException);
}

TEST(JsonValueTest, ObjectOpsThrowOnNonObject) {
    JsonValue v(std::vector<JsonValue>{JsonValue(1)});  // Array，非对象
    EXPECT_THROW(v["key"], JsonValueException);
    EXPECT_THROW(v.at("key"), JsonValueException);

    const JsonValue& cv = v;
    EXPECT_THROW(static_cast<void>(cv.at("key")), JsonValueException);
}

TEST(JsonValueTest, NestedObjectChainedAccess) {
    // operator[](key) 的"自动创建"只针对已是 Object 的节点新增 key；
    // root["user"] 首次访问自动插入的是 null 节点，必须显式赋空对象后才能继续链式操作
    JsonValue root(std::unordered_map<std::string, JsonValue>{});
    root["user"] = JsonValue(std::unordered_map<std::string, JsonValue>{});
    JsonValue& user = root["user"];

    // 链式创建深层结构：root["user"]["hobby"][0]
    user["hobby"] = JsonValue(std::vector<JsonValue>{JsonValue("reading")});

    EXPECT_EQ(root["user"]["hobby"][0].asString(), "reading");

    // 深层修改
    root["user"]["hobby"][0] = JsonValue("coding");
    EXPECT_EQ(root["user"]["hobby"][0].asString(), "coding");
}

// ==================== 6. 通用工具方法 ====================

TEST(JsonValueTest, SizeOfContainers) {
    JsonValue arr(std::vector<JsonValue>{JsonValue(1), JsonValue(2), JsonValue(3)});
    EXPECT_EQ(arr.size(), 3u);

    JsonValue obj(
        std::unordered_map<std::string, JsonValue>{{"a", JsonValue(1)}, {"b", JsonValue(2)}});
    EXPECT_EQ(obj.size(), 2u);

    JsonValue str("abcd");
    EXPECT_EQ(str.size(), 4u);
}

TEST(JsonValueTest, SizeOfEmptyAndScalarIsZero) {
    JsonValue null_v(nullptr);
    EXPECT_EQ(null_v.size(), 0u);

    JsonValue bool_v(true);
    EXPECT_EQ(bool_v.size(), 0u);

    JsonValue num_v(3.14);
    EXPECT_EQ(num_v.size(), 0u);

    JsonValue empty_str("");
    EXPECT_EQ(empty_str.size(), 0u);

    JsonValue empty_arr(std::vector<JsonValue>{});
    EXPECT_EQ(empty_arr.size(), 0u);

    JsonValue empty_obj(std::unordered_map<std::string, JsonValue>{});
    EXPECT_EQ(empty_obj.size(), 0u);
}

TEST(JsonValueTest, IsEmptyScenarios) {
    EXPECT_TRUE(JsonValue(nullptr).isEmpty());                                       // null 为空
    EXPECT_TRUE(JsonValue("").isEmpty());                                            // 空串为空
    EXPECT_TRUE(JsonValue(std::vector<JsonValue>{}).isEmpty());                      // 空数组为空
    EXPECT_TRUE(JsonValue(std::unordered_map<std::string, JsonValue>{}).isEmpty());  // 空对象为空

    EXPECT_FALSE(JsonValue("x").isEmpty());                                   // 非空串
    EXPECT_FALSE(JsonValue(std::vector<JsonValue>{JsonValue(1)}).isEmpty());  // 非空数组

    // bool 与数字没有“空”概念，一律非空（注意 false / 0 也是非空！）
    EXPECT_FALSE(JsonValue(false).isEmpty());
    EXPECT_FALSE(JsonValue(true).isEmpty());
    EXPECT_FALSE(JsonValue(0.0).isEmpty());
}

TEST(JsonValueTest, ClearResetsToNull) {
    JsonValue arr(std::vector<JsonValue>{JsonValue(1), JsonValue(2)});
    arr.clear();
    EXPECT_TRUE(arr.isNull());
    EXPECT_EQ(arr.type(), JsonType::Null);
    EXPECT_TRUE(arr.isEmpty());

    JsonValue str("hello");
    str.clear();
    EXPECT_TRUE(str.isNull());

    JsonValue obj(std::unordered_map<std::string, JsonValue>{{"k", JsonValue(1)}});
    obj.clear();
    EXPECT_TRUE(obj.isNull());

    JsonValue num(3.14);
    num.clear();
    EXPECT_TRUE(num.isNull());

    // 对 null 再次 clear 不抛异常
    JsonValue null_v(nullptr);
    EXPECT_NO_THROW(null_v.clear());
    EXPECT_TRUE(null_v.isNull());
}

TEST(JsonValueTest, VisitReadsStoredValue) {
    // visit 对 Number 命中 double 分支
    JsonValue num(2.5);
    double got_num = num.visit([](auto&& val) -> double {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, double>) {
            return val;
        } else {
            return -1.0;  // 走错分支则返回哨兵值
        }
    });
    EXPECT_DOUBLE_EQ(got_num, 2.5);

    // visit 对 Array 命中 vector 分支
    JsonValue arr(std::vector<JsonValue>{JsonValue(1)});
    size_t got_size = arr.visit([](auto&& val) -> size_t {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, std::vector<JsonValue>>) {
            return val.size();
        } else {
            return static_cast<size_t>(-1);
        }
    });
    EXPECT_EQ(got_size, 1u);
}

TEST(JsonValueTest, VisitOnConstValue) {
    const JsonValue str("hi");
    std::string got = str.visit([](const auto& val) -> std::string {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, std::string>) {
            return val;
        } else {
            return std::string();
        }
    });
    EXPECT_EQ(got, "hi");
}

// ==================== 7. 拷贝与移动 ====================

TEST(JsonValueTest, CopyConstructorIsDeepCopy) {
    // 数组内嵌对象，验证深拷贝不共享底层数据
    JsonValue src(std::vector<JsonValue>{JsonValue(1)});
    src[0] = JsonValue(std::unordered_map<std::string, JsonValue>{{"k", JsonValue(1)}});

    JsonValue dst(src);
    EXPECT_EQ(dst.size(), src.size());

    // 修改副本不影响原件
    dst[0]["k"] = JsonValue(100.0);
    EXPECT_DOUBLE_EQ(src[0]["k"].asNumber(), 1.0);
    EXPECT_DOUBLE_EQ(dst[0]["k"].asNumber(), 100.0);

    // 副本增删元素不影响原件
    dst.pushBack(JsonValue(2));
    EXPECT_EQ(src.size(), 1u);
    EXPECT_EQ(dst.size(), 2u);
}

TEST(JsonValueTest, CopyAssignmentIsDeepCopy) {
    JsonValue src(std::unordered_map<std::string, JsonValue>{{"v", JsonValue(1)}});
    JsonValue dst;
    dst = src;

    EXPECT_EQ(dst.size(), src.size());
    dst["v"] = JsonValue(99.0);
    EXPECT_DOUBLE_EQ(src["v"].asNumber(), 1.0);
    EXPECT_DOUBLE_EQ(dst["v"].asNumber(), 99.0);
}

TEST(JsonValueTest, MoveConstructorTransfersValue) {
    JsonValue src(std::unordered_map<std::string, JsonValue>{
        {"name", JsonValue("tom")},
        {"age", JsonValue(20)},
    });
    JsonValue dst(std::move(src));
    EXPECT_EQ(dst.size(), 2u);
    EXPECT_EQ(dst["name"].asString(), "tom");
    EXPECT_DOUBLE_EQ(dst["age"].asNumber(), 20.0);
}

TEST(JsonValueTest, MoveAssignmentTransfersValue) {
    JsonValue src("payload");
    JsonValue dst;
    dst = std::move(src);
    EXPECT_TRUE(dst.isString());
    EXPECT_EQ(dst.asString(), "payload");
}

TEST(JsonValueTest, MovedFromObjectIsReusable) {
    // 被移动的对象处于“有效但未指定”状态，重新赋值后应可正常使用
    JsonValue src("old");
    JsonValue dst(std::move(src));
    EXPECT_EQ(dst.asString(), "old");

    src = JsonValue("new");
    EXPECT_TRUE(src.isString());
    EXPECT_EQ(src.asString(), "new");
}

TEST(JsonValueTest, CopyAssignmentSelfAndChain) {
    // 链式赋值拷贝同源
    JsonValue a(std::vector<JsonValue>{JsonValue(1)});
    JsonValue b;
    JsonValue c;
    c = b = a;
    EXPECT_EQ(c.size(), 1u);
    EXPECT_EQ(b.size(), 1u);
}

// ==================== 8. 复杂嵌套冒烟测试 ====================

TEST(JsonValueTest, ComplexNestedStructure) {
    // 模拟一段真实 JSON 文档的结构：对象 -> 数组 -> 对象 -> 标量
    JsonValue doc(std::unordered_map<std::string, JsonValue>{});

    doc["title"] = JsonValue("diary");
    doc["tags"] =
        JsonValue(std::vector<JsonValue>{JsonValue("cpp"), JsonValue("json"), JsonValue("test")});

    JsonValue entry(std::unordered_map<std::string, JsonValue>{});
    entry["id"] = JsonValue(1);
    entry["done"] = JsonValue(false);
    doc["entries"] = JsonValue(std::vector<JsonValue>{entry});

    // 逐层断言
    EXPECT_TRUE(doc.isObject());
    EXPECT_EQ(doc.size(), 3u);
    EXPECT_EQ(doc["title"].asString(), "diary");
    EXPECT_EQ(doc["tags"].size(), 3u);
    EXPECT_EQ(doc["tags"][1].asString(), "json");

    EXPECT_TRUE(doc["entries"].isArray());
    EXPECT_DOUBLE_EQ(doc["entries"][0]["id"].asNumber(), 1.0);
    EXPECT_FALSE(doc["entries"][0]["done"].asBool());

    // 深拷贝后整体比较一致，且相互独立
    JsonValue copy(doc);
    copy["entries"][0]["done"] = JsonValue(true);
    EXPECT_FALSE(doc["entries"][0]["done"].asBool());
    EXPECT_TRUE(copy["entries"][0]["done"].asBool());
    EXPECT_EQ(copy["tags"].size(), doc["tags"].size());

    // clear 只影响自身
    copy.clear();
    EXPECT_TRUE(copy.isNull());
    EXPECT_EQ(doc["title"].asString(), "diary");
}
