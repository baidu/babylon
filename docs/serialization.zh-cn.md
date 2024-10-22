**[[English]](serialization.en.md)**

# serialization

## 原理

google::protobuf提供了一种基于tag号的版本兼容的序列化机制，并通过代码生成技术，支持了idl式的序列化接口定义；不过有些情况下，仅使用idl定义模式支持的序列化无法完全满足需求

- 对于程序中使用到的基础结构，出于访问性能考虑，有时需要绕过protobuf的成员内存组织方式
- 对于程序用到的复杂数据结构，有时无法直接通过message/repeated等结构表达

这些场景下用到的自定义结构就无法直接借助protobuf来完成序列化。往往需要额外定义一套信息量对等的message，并额外增加手动赋值等操作开销。为了简便支持自定义结构的序列化，以及实现基于tag的版本兼容机制，制作了一套声明式的宏函数，来支持这些自定义结构的序列化

## 使用方法

### 序列化接口

```c++
#include <babylon/serialization.h>

using ::babylon::Serialization;
using ::babylon::SerializeTraits;

// 序列化和反序列化统一通过Serialization静态成员函数提供
// value需要实现序列化协议babylon::SerializeTraits
// 返回success用于判定操作成功

// 判断T是否可序列化
// 不可序列化时，也可以调用相关函数，但是默认都会返回失败
if (SerializeTraits<T>::SERIALIZABLE) {
}

// string版本
std::string s;
success = Serialization::serialize_to_string(value, s);
success = Serialization::parse_from_string(s, value);

// coded stream版本
google::protobuf::io::CodedOutputStream cos;
google::protobuf::io::CodedInputStream cis;
success = Serialization::serialize_to_coded_stream(value, cos);
success = Serialization::parse_from_coded_stream(cis, value);

// 输出debug stirng，string版本
std::string s;
success = Serialization::print_to_string(value, s);

// 输出debug stirng，stream版本
google::protobuf::io::ZeroCopyOutputStream os;
success = Serialization::print_to_stream(value, os);

// 默认对于如下value类型默认支持
// 1. proto idl生成类，即google::protobuf::MessageLite子类
// 2. 基础类型，即bool, int*_t, uint*_t, float, double, enum, enum class, std::string
// 3. 基础容器：std::vector<T>, std::unique_ptr<T>
```

### 序列化宏声明

```c++
#include <babylon/serialization.h>

// 任意自定义类型
class SomeType {
public:
    ...  // 常规方法定义

// 成员不需要公开访问权限
private:
    int32_t a;
    float b;
    std::vector<int64_t> c;
    // SomeOtherType需要通过宏声明等方式支持序列化
    SomeOtherType d;
    // 包装一层
    std::unique_ptr<SomeOtherType> e;
    // proto idl定义类天然可序列化
    SomeMessage f;
    std::unique_ptr<SomeMesssage> g;
    ...
    
    // 声明后SomeType即可进行序列化
    // 只有列出的成员会实际参与序列化过程
    BABYLON_SERIALIZABLE(a, b, c, d, e, f, g, ...);
};

class SomeType {
public:
    ...

private:
    ...
    
    // 声明后SomeType即可进行跨版本序列化
    // 只有列出的成员会实际参与序列化过程
    // 每个成员需要对应唯一的tag号
    // 在迭代过程中，成员可以增删，但是要tag号不会重复使用
    BABYLON_COMPATIBLE((a, 1)(b, 2)(c, 3)...);
};
```

### 包含基类的序列化

```c++
#include <babylon/serialization.h>

// 一个可序列化的基类
class BaseType {
public:
    ...
    
private:
    ...
    
    // 正常定义为可序列化
    BABYLON_SERIALIZABLE(...);
    // 或者可版本兼容
    BABYLON_COMPATIBLE(...);
};

class SubType : public BaseType {
public:
    ...
    
private:
    ...
    
    // 可序列化级联，BaseType会像第一个成员一样序列化
    BABYLON_SERIALIZABLE_WITH_BASE(BaseType, ...);
    // 版本兼容级联，需要为BaseType指定独立tag
    // 会像一个具有BaseType类型的成员一样序列化
    BABYLON_COMPATIBLE_WITH_BASE((BaseType, 1), ...);
};
```

### 序列化函数实现

```c++
#include <babylon/serialization.h>

// 对于无法简单进行宏声明改造的复杂类型
// 可以通过直接实现相关接口的方式来支持序列化

// 基础实现
class SomeType {
public:
    // 可以不实现此函数
    // 实现后返回true表示序列化方式可版本兼容，相当于BABYLON_COMPATIBLE声明
    // 返回false和不实现含义相同，默认相当于BABYLON_SERIALIZABLE声明
    static constexpr bool serialize_compatible() {
        return ...;
    }
    void serialize(CodedOutputStream& os) const {
        ... // 将this序列化并写入os
    }
    bool deserialize(CodedInputStream& is) {
        ... // 从is读入数据，并重建到this
        return ...; // 正常完成返回true，解析错误等返回false
    }
    size_t caculate_serialized_size() const noexcept {
        return ...; // 计算并返回『序列化后』的bytes数
    }

    ... // 其他函数和成员定义
};

// size计算复杂时需要通过缓存等机制加速
// 可以通过额外函数进行声明和支持
class SomeType {
public:
    // 声明计算size的复杂度，分为TRIVIAL/SIMPLE/COMPLEX三个级别
    // 代表的编译期可计算，O(1)可计算和O(n)可计算
    // 注意，如果设计了后面的缓存机制，那么这里需要声明的是『使用缓存』时的复杂度
    static constexpr int serialized_size_complexity() {
        return SerializationHelper::SERIALIZED_SIZE_COMPLEXITY_SIMPLE;
    }
    
    // 实现用于加速size计算的缓存机制
    // 框架会保证在调用serialize函数前，一定调用过caculate_serialized_size
    // 这样实际进行serialize内部可以改为调用serialized_size_cached
    // 并通过缓存上一次caculate_serialized_size的结果来加速
    // 这也是protobuf::Message采用的加速手段
    size_t serialized_size_cached() const noexcept {
        return ...;
    }

    ...
};
```

### 兼容Protocol Buffer

```c++
////////////////////////////////////
// test.proto 假设有如下一个proto定义
enum TestEnum {
};

message TestMessage {
    optional bool b = 1;
    optional int32 i32 = 4;
    optional int64 i64 = 5;
    optional uint32 u32 = 8;
    optional uint64 u64 = 9;
    optional float f = 16;
    optional double d = 17;
    optional TestEnum e = 18;
    optional string s = 19;
    optional bytes by = 20;
    optional TestMessage m = 21;
    
    repeated bool rpb = 44 [packed = true];
    repeated int32 rpi32 = 47 [packed = true];
    repeated int64 rpi64 = 48 [packed = true];
    repeated uint32 rpu32 = 51 [packed = true];
    repeated uint64 rpu64 = 52 [packed = true];
    repeated float rpf = 59 [packed = true];
    repeated double rpd = 60 [packed = true];
    repeated TestEnum rpe = 61 [packed = true];
};
////////////////////////////////////

///////////////////////////////////
// test.cc 假设有如下一个类定义
#include <babylon/serialization.h>

enum TestEnum {
};

struct TestSubObject {
    bool _b;
    int32_t _i32;
    int64_t _i64;
    uint32_t _u32;
    uint64_t _u64;
    float _f;
    double _d;
    TestEnum _e;
    ::std::string _s;
    ::std::string _by;
    ::std::vector<::std::string> _rs;
    ::std::vector<::std::string> _rby;
    ::std::vector<bool> _rpb;
    ::std::vector<int32_t> _rpi32;
    ::std::vector<int64_t> _rpi64;
    ::std::vector<uint32_t> _rpu32;
    ::std::vector<uint64_t> _rpu64;
    ::std::vector<float> _rpf;
    ::std::vector<double> _rpd;
    ::std::vector<TestEnum> _rpe;
    
    BABYLON_COMPATIBLE(
        (_b, 1)(_i32, 4)(_i64, 5)(_u32, 8)(_u64, 9)
        (_f, 16)(_d, 17)(_e, 18)(_s, 19)(_by, 20)
        (_rs, 41)(_rby, 42)
        (_rpb, 44)(_rpi32, 47)(_rpi64, 48)(_rpu32, 51)(_rpu64, 52)
        (_rpf, 59)(_rpd, 60)(_rpe, 61)
    );
};

struct TestObject {
    bool _b;
    int32_t _i32;
    int64_t _i64;
    uint32_t _u32;
    uint64_t _u64;
    float _f;
    double _d;
    TestEnum _e;
    ::std::string _s;
    ::std::string _by;
    TestSubObject _m;
    ::std::vector<::std::string> _rs;
    ::std::vector<::std::string> _rby;
    ::std::vector<TestSubObject> _rm;
    ::std::vector<bool> _rpb;
    ::std::vector<int32_t> _rpi32;
    ::std::vector<int64_t> _rpi64;
    ::std::vector<uint32_t> _rpu32;
    ::std::vector<uint64_t> _rpu64;
    ::std::vector<float> _rpf;
    ::std::vector<double> _rpd;
    ::std::vector<TestEnum> _rpe;
    
    BABYLON_COMPATIBLE(
        (_b, 1)(_i32, 4)(_i64, 5)(_u32, 8)(_u64, 9)
        (_f, 16)(_d, 17)(_e, 18)(_s, 19)(_by, 20)
        (_m, 21)(_rs, 41)(_rby, 42)(_rm, 43)
        (_rpb, 44)(_rpi32, 47)(_rpi64, 48)(_rpu32, 51)(_rpu64, 52)
        (_rpf, 59)(_rpd, 60)(_rpe, 61)
    );
};

// struct -> message正向可兼容
TestObject s;
Serialization::serialize_to_string(s, str);
TestMessage m;
Serialization::parse_from_string(str, m);

// message -> struct反向可兼容
TestObject m;
Serialization::serialize_to_string(m, str);
TestMessage s;
Serialization::parse_from_string(str, s);
///////////////////////////////////
```
