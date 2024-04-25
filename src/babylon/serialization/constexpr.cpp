#include "babylon/serialization/scalar.h"

#if BABYLON_USE_PROTOBUF

BABYLON_NAMESPACE_BEGIN

// 对于static constexpr成员，虽然大多情况下直接编译期计算即可
// 无需产生符号，但是在一些特殊情况下（例如取地址等）依然需要符号辅助
// 在-std=c++17之前，这些符号需要独立定义环节来产生
// 在仅引用头文件的声明的情况下，不会产生符号，只可能产生依赖
// 因此需要和独立定义环节链接才能正常使用
//
// 在-std=c++17之后，static constexpr成员声明即为定义
// 独立的定义环节已经不建议使用，报出-Wdepracated
// 当需要符号辅助时，依据头文件直接在当前编译单元生成弱符号或唯一符号
// 因此部分编译器认为对于独立的定义环节也不再需要默认产出符号
//
// 但是对于多编译单元混用-std=c++14/c++17的情况下，可能出现
// 1、库提供者采用-std=c++17编译，不再产出符号
// 2、库使用这采用-std=c++14编译，也不产出符号，但产生了依赖
// 最终导致链接时符号缺失
//
// 为了支持混用，这里采用桩函数通过取地址方式强制产出符号
namespace internal {
ABSL_ATTRIBUTE_WEAK uintptr_t constexpr_symbol_generator() {
    return static_cast<uintptr_t>(0)
        + reinterpret_cast<uintptr_t>(&SerializationHelper::SERIALIZED_SIZE_COMPLEXITY_COMPLEX)
        + reinterpret_cast<uintptr_t>(&SerializationHelper::SERIALIZED_SIZE_COMPLEXITY_SIMPLE)
        + reinterpret_cast<uintptr_t>(&SerializationHelper::SERIALIZED_SIZE_COMPLEXITY_TRIVIAL)
#define BABYLON_TMP_GEN(type) \
        + reinterpret_cast<uintptr_t>(&SerializeTraits<type>::SERIALIZABLE) \
        + reinterpret_cast<uintptr_t>(&SerializeTraits<type>::WIRE_TYPE) \
        + reinterpret_cast<uintptr_t>(&SerializeTraits<type>::SERIALIZED_SIZE_COMPLEXITY)
        BABYLON_TMP_GEN(bool)
        BABYLON_TMP_GEN(int8_t)
        BABYLON_TMP_GEN(int16_t)
        BABYLON_TMP_GEN(int32_t)
        BABYLON_TMP_GEN(int64_t)
        BABYLON_TMP_GEN(uint8_t)
        BABYLON_TMP_GEN(uint16_t)
        BABYLON_TMP_GEN(uint32_t)
        BABYLON_TMP_GEN(uint64_t)
        BABYLON_TMP_GEN(float)
        BABYLON_TMP_GEN(double)
#undef BABYLON_TMP_GEN
        ;
}
} // internal

// 当库本身用-std=c++14编译时，强制产出的符号为常量符号
// 会和使用者采用-std=c++17编译时产生的唯一符号发生重定义冲突
// 这里对-std=c++14的情况改写为弱符号
#if __cplusplus < 201703L
ABSL_ATTRIBUTE_WEAK constexpr int
SerializationHelper::SERIALIZED_SIZE_COMPLEXITY_COMPLEX;
ABSL_ATTRIBUTE_WEAK constexpr int
SerializationHelper::SERIALIZED_SIZE_COMPLEXITY_SIMPLE;
ABSL_ATTRIBUTE_WEAK constexpr int
SerializationHelper::SERIALIZED_SIZE_COMPLEXITY_TRIVIAL;
#define BABYLON_TMP_GEN(type) \
ABSL_ATTRIBUTE_WEAK constexpr bool SerializeTraits<type>::SERIALIZABLE; \
ABSL_ATTRIBUTE_WEAK constexpr ::google::protobuf::internal::WireFormatLite::WireType SerializeTraits<type>::WIRE_TYPE; \
ABSL_ATTRIBUTE_WEAK constexpr int SerializeTraits<type>::SERIALIZED_SIZE_COMPLEXITY;
BABYLON_TMP_GEN(bool)
BABYLON_TMP_GEN(int8_t)
BABYLON_TMP_GEN(int16_t)
BABYLON_TMP_GEN(int32_t)
BABYLON_TMP_GEN(int64_t)
BABYLON_TMP_GEN(uint8_t)
BABYLON_TMP_GEN(uint16_t)
BABYLON_TMP_GEN(uint32_t)
BABYLON_TMP_GEN(uint64_t)
BABYLON_TMP_GEN(float)
BABYLON_TMP_GEN(double)
#undef BABYLON_TMP_GEN
#endif // __cplusplus < 201703L

BABYLON_NAMESPACE_END

#endif // BABYLON_USE_PROTOBUF
