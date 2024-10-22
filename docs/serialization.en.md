**[[简体中文]](serialization.zh-cn.md)**

# serialization

## Overview

Google Protobuf offers a tag-based serialization mechanism that supports version compatibility through IDL-defined interfaces. However, there are scenarios where the IDL-defined serialization may not be sufficient:

- For performance reasons, you may want to bypass Protobuf's internal memory organization for fundamental structures used in your program.
- Complex data structures in your program may not be directly expressible using Protobuf's `message` and `repeated` constructs.

In such cases, custom structures require manual serialization, often necessitating the creation of parallel Protobuf messages and manual copying of values. To simplify the serialization of custom structures while retaining tag-based version compatibility, Babylon provides a set of declarative macros to support custom structure serialization.

## Usage

### Serialization Interface

```c++
#include <babylon/serialization.h>

using ::babylon::Serialization;
using ::babylon::SerializeTraits;

// Serialization and deserialization are handled by the static functions of Serialization.
// The value must implement the babylon::SerializeTraits serialization protocol.
// The `success` return value indicates whether the operation succeeded.

// Check if a type T is serializable.
// Even if a type is not serializable, related functions can still be called, but they will always return failure by default.
if (SerializeTraits<T>::SERIALIZABLE) {
}

// String-based serialization
std::string s;
success = Serialization::serialize_to_string(value, s);
success = Serialization::parse_from_string(s, value);

// Coded stream-based serialization
google::protobuf::io::CodedOutputStream cos;
google::protobuf::io::CodedInputStream cis;
success = Serialization::serialize_to_coded_stream(value, cos);
success = Serialization::parse_from_coded_stream(cis, value);

// Output debug string (string version)
std::string s;
success = Serialization::print_to_string(value, s);

// Output debug string (stream version)
google::protobuf::io::ZeroCopyOutputStream os;
success = Serialization::print_to_stream(value, os);

// The following types are natively supported:
// 1. Protobuf IDL-generated classes (subclasses of google::protobuf::MessageLite)
// 2. Basic types like bool, int*_t, uint*_t, float, double, enum, enum class, std::string
// 3. Basic containers: std::vector<T>, std::unique_ptr<T>
```

### Serialization Macro Declaration

```c++
#include <babylon/serialization.h>

// Custom type
class SomeType {
public:
    ...  // Regular method definitions

private:
    int32_t a;
    float b;
    std::vector<int64_t> c;
    SomeOtherType d;  // SomeOtherType must support serialization through a macro declaration
    std::unique_ptr<SomeOtherType> e;  // Wrap one layer
    SomeMessage f;  // Proto IDL-defined classes are natively serializable
    std::unique_ptr<SomeMesssage> g;
    ...

    // Declaring this makes SomeType serializable.
    // Only the listed members will participate in the serialization process.
    BABYLON_SERIALIZABLE(a, b, c, d, e, f, g, ...);
};

class SomeType {
public:
    ...

private:
    ...

    // Declaring this makes SomeType cross-version serializable.
    // Only the listed members will participate in the serialization process.
    // Each member needs a unique tag number.
    // During evolution, members can be added or removed, but tag numbers should not be reused.
    BABYLON_COMPATIBLE((a, 1)(b, 2)(c, 3)...);
};
```

### Including Base Class in Serialization

```c++
#include <babylon/serialization.h>

// A serializable base class
class BaseType {
public:
    ...

private:
    ...

    // Normally defined as serializable
    BABYLON_SERIALIZABLE(...);
    // Or version compatible
    BABYLON_COMPATIBLE(...);
};

class SubType : public BaseType {
public:
    ...

private:
    ...

    // Serialize base class along with subclass members.
    BABYLON_SERIALIZABLE_WITH_BASE(BaseType, ...);
    // For version compatibility, BaseType is treated like a member and given a unique tag.
    BABYLON_COMPATIBLE_WITH_BASE((BaseType, 1), ...);
};
```

### Custom Serialization Function Implementation

```c++
#include <babylon/serialization.h>

// For complex types that cannot be easily adapted using macros,
// custom serialization support can be implemented directly.

class SomeType {
public:
    // You can omit this function.
    // If implemented and returns true, the serialization will be version-compatible (equivalent to BABYLON_COMPATIBLE declaration).
    // If it returns false, it behaves like the default BABYLON_SERIALIZABLE declaration.
    static constexpr bool serialize_compatible() {
        return ...;
    }
    void serialize(CodedOutputStream& os) const {
        ... // Serialize this object and write to os
    }
    bool deserialize(CodedInputStream& is) {
        ... // Read data from is and reconstruct this object
        return ...; // Return true if successful, false if parsing fails
    }
    size_t calculate_serialized_size() const noexcept {
        return ...; // Calculate and return the number of bytes after serialization
    }

    ... // Other functions and member definitions
};

// If size calculation is complex, a caching mechanism can be used for optimization.
// You can declare additional functions to support this.
class SomeType {
public:
    // Declare the complexity of size calculation: TRIVIAL, SIMPLE, or COMPLEX.
    // TRIVIAL: Calculable at compile-time, SIMPLE: O(1), COMPLEX: O(n).
    // If using a caching mechanism, this should represent the complexity of the cached calculation.
    static constexpr int serialized_size_complexity() {
        return SerializationHelper::SERIALIZED_SIZE_COMPLEXITY_SIMPLE;
    }

    // Implement caching mechanism for size calculation.
    // The framework guarantees that the calculate_serialized_size function will be called before serialize.
    // In serialize, use serialized_size_cached for speedup by caching the previous calculate_serialized_size result.
    size_t serialized_size_cached() const noexcept {
        return ...;
    }

    ...
};
```

### Protocol Buffer Compatibility

```c++
////////////////////////////////////
// test.proto Suppose the following proto definition exists
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
// test.cc Suppose the following class definitions exist
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

// struct -> message conversion for forward compatibility
TestObject s;
Serialization::serialize_to_string(s, str);
TestMessage m;
Serialization::parse_from_string(str, m);

// message -> struct conversion for backward compatibility
TestMessage m;
Serialization::serialize_to_string(m, str);
TestObject s;
Serialization::parse_from_string(str, s);
///////////////////////////////////
```
