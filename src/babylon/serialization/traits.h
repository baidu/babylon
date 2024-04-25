#pragma once

#include "babylon/environment.h"                            // BABYLON_USE_PROTOBUF

#if BABYLON_USE_PROTOBUF

#include "babylon/any.h"                                    // babylon::Any

#include BABYLON_EXTERNAL(absl/container/flat_hash_map.h)                   // absl::flat_hash_map
#include "boost/preprocessor/cat.hpp"                       // BOOST_PP_CAT
#include "google/protobuf/io/coded_stream.h"                // google::protobuf::io::Coded*Stream
#include "google/protobuf/io/zero_copy_stream_impl_lite.h"  // google::protobuf::io::ZeroCopy*Stream
#include "google/protobuf/wire_format_lite.h"               // google::protobuf::internal::WireFormatLite

BABYLON_NAMESPACE_BEGIN

// 序列化特征
// 对于一个类型T，有三种方法可以实现序列化
// 1. 直接特化SerializeTraits模板，实现序列化功能
// 2. 对于 struct / class 类型的T，可以按照协议约定实现序列化成员函数
//    对应的SerializeTraits模板会通过萃取这些协议成员的方式自动形成
// 3. 对于 struct / class 类型的T，如果 成员/基类 都已经支持的序列化特征
//    可以简单通过增补 BABYLON_SERIALIZABLE宏定义
//    来辅助自动生成符合协议约定的序列化成员函数，实现序列化功能
//
// 对SerializeTraits进行模板特化也有三种常见方式
// 1. 特化支持一个具体类S，进行完全特化template <> SerializeTraits<S>
// 2. 特化支持一个模板类S<U>，进行偏特化template <U> SerializeTraits<S<U>>
// 3. 特化支持某一类S，S可以满足编译期表达式X<S> == true
//    比如典型的『某个类型的子类』『某几个明确类型』等
//    此时需要使用到专门预留的模板参数E
//    template <S> SerializeTraits<S, std::enable_if_t<X<S>>>
template <typename T, typename E = void>
class SerializeTraits;

// 序列化功能接口
// 对于类型T，在通过SerializeTraits支持了序列化后，可以调用
// Serialization::serialize_to_coded_stream
// Serialization::parse_from_coded_stream
// 来进行序列化和反序列化
class Serialization {
private:
    using CodedInputStream = ::google::protobuf::io::CodedInputStream;
    using CodedOutputStream = ::google::protobuf::io::CodedOutputStream;
    using ZeroCopyOutputStream = ::google::protobuf::io::ZeroCopyOutputStream;

public:
    // 用来实现文本格式输出的输出流
    // 封装了缩进/转义等格式化功能
    class PrintStream;

    // 类型消除的序列化接口
    // 接口含义与Serialization的同名模板接口一致
    class Serializer;

    // 计算序列化后的预期大小
    // 返回值表示序列化结果的字节数
    // 仅在SerializeTraits<T>::SERIALIZABLE == true时可用
    // 否则恒定返回0
    template <typename T>
    inline static size_t calculate_serialized_size(const T& value) noexcept;

    // 序列化到一个CodedOutputStream中
    // 返回值表示序列化是否成功
    // 仅在SerializeTraits<T>::SERIALIZABLE == true时可用
    // 否则恒定失败
    // 
    // SerializeTraits<T>::SERIALIZED_SIZE_CACHED == true时
    // 如果在调用序列化函数之前，已经明确调用过calculate_serialized_size
    // 则可以使用with_cached_size版本来避免再次计算序列化大小的开销
    template <typename T>
    static bool serialize_to_coded_stream(
            const T& value, CodedOutputStream& os) noexcept;
    template <typename T>
    static bool serialize_to_coded_stream_with_cached_size(
            const T& value, CodedOutputStream& os) noexcept;

    // 序列化到一个std::string中
    // 返回值表示序列化是否成功
    // 仅在SerializeTraits<T>::SERIALIZABLE == true时可用
    // 否则恒定失败
    template <typename T>
    static bool serialize_to_string(const T& value, ::std::string& s) noexcept;

    // 序列化到一块定长内存中
    // 返回值表示序列化是否成功
    // 仅在SerializeTraits<T>::SERIALIZABLE == true时可用
    // 否则恒定失败
    //
    // 由于需要外部准备好足够的缓冲区
    // 因此势必需要先调用一次calculate_serialized_size
    // 因此仅提供with_cached_size版本
    template <typename T>
    static bool serialize_to_array_with_cached_size(
            const T& value, void* buffer, size_t size) noexcept;

    // 从CodedInputStream中反序列化
    // 返回值表示反序列化是否成功
    // 仅在SerializeTraits<T>::SERIALIZABLE == true时可用
    // 否则恒定失败
    template <typename T>
    static bool parse_from_coded_stream(
            CodedInputStream& is, T& value) noexcept;

    // 从std::string中反序列化
    // 返回值表示反序列化是否成功
    // 仅在SerializeTraits<T>::SERIALIZABLE == true时可用
    // 否则恒定失败
    template <typename T>
    static bool parse_from_string(
            const ::std::string& s, T& value) noexcept;

    // 从定长内存中反序列化
    // 返回值表示反序列化是否成功
    // 仅在SerializeTraits<T>::SERIALIZABLE == true时可用
    // 否则恒定失败
    template <typename T>
    static bool parse_from_array(
            const char* data, size_t size, T& value) noexcept;

    // 打印明文可读结果到ZeroCopyOutputStream
    // 不支持打印的类型输出类型名占位
    template <typename T>
    static bool print_to_stream(const T& value,
                                ZeroCopyOutputStream& os) noexcept;

    // 打印明文可读结果到std::string
    // 不支持打印的类型输出类型名占位
    template <typename T>
    static bool print_to_string(const T& value, ::std::string& s) noexcept;

    // 注册类型T支持反射序列化，一般不主动使用
    // 而是通过注册宏BABYLON_REGISTER_SERIALIZER来自动调用
    // 注册后可以通过serializer_for_name来获取类型消除版本的序列化器
    template <typename T>
    static void register_serializer() noexcept;

    // 根据类全名获得对应的类型消除版本的序列化器
    // 是类型消除版本的序列化功能的入口，需要对应类型注册后才可使用
    static Serializer* serializer_for_name(StringView class_full_name) noexcept;

public:
    class PrintStream {
    public:
        ~PrintStream() noexcept;

        // 设置或获取底层的ZeroCopyOutputStream
        // 在设置或获取前，确保没有积攒的写操作
        // 例如应该先进行一次flush
        void set_stream(ZeroCopyOutputStream& os) noexcept;
        ZeroCopyOutputStream& stream() noexcept;

        // 设置或获取缩进层次
        void set_indent_level(size_t level) noexcept;
        size_t indent_level() const noexcept;

        // 增加或减少一层缩进
        bool indent() noexcept;
        bool outdent() noexcept;

        // 输出原始字符串，不会尝试进行转义或校验
        bool print_raw(const char* data, size_t size) noexcept;
        bool print_raw(StringView sv) noexcept;

        // 格式化输出一个『字符串』，表示为
        // "content \n escaped"
        bool print_string(StringView sv) noexcept;

        // 换行，新行输出前会按照要求进行指定层次的缩进
        bool start_new_line() noexcept;

        // 提交当前缓存的内容
        // 调用后就可以直接对底层ZeroCopyOutputStream
        // 进行操作了
        void flush() noexcept;

    private:
        bool print_blank(size_t size) noexcept;

        ZeroCopyOutputStream* _os {nullptr};
        char* _buffer {nullptr};
        size_t _buffer_size {0};
        bool _at_start_of_line {true};
        size_t _indent_level {0};
    };

    class Serializer {
    public:
        virtual ~Serializer() noexcept;

        // 含义和Serialization同名函数相同，但是支持类型消除的使用方式
        virtual bool parse_from_coded_stream(CodedInputStream& is, Any& instance) noexcept = 0;
        virtual bool parse_from_string(const ::std::string&, Any& instance) noexcept = 0;
        virtual bool parse_from_array(const char* data, size_t size, Any& value) noexcept = 0;
        virtual bool print_to_stream(const Any& value, ZeroCopyOutputStream& os) noexcept = 0;
        virtual bool print_to_string(const Any& value, ::std::string& s) noexcept = 0;
    };

    // 默认的序列化器
    template <typename T>
    class DefaultSerializer : public Serializer {
        inline T& mutable_value(Any& instance) noexcept;
        virtual bool parse_from_coded_stream(
                CodedInputStream& is, Any& instance) noexcept override;
        virtual bool parse_from_string(
                const ::std::string& str, Any& instance) noexcept override;
        virtual bool parse_from_array(
                const char* data, size_t size, Any& instance) noexcept override;
        virtual bool print_to_stream(
                const Any& instance, ZeroCopyOutputStream& os) noexcept override;
        virtual bool print_to_string(
                const Any& instance, ::std::string& s) noexcept override;
    };

private:
    static ::absl::flat_hash_map<::std::string,
                                 ::std::unique_ptr<Serializer>,
                                 ::std::hash<StringView>,
                                 ::std::equal_to<StringView>>&
    serializers() noexcept;
};

// 序列化特征的默认实现，设计了一套协议机制
// 一般情况下，自定义类型的序列化可以通过直接对任意类型T
// 按照协议实现必要的函数来自动生成有效的SerializeTraits
//
// 但是对于有些场景，例如基本类型或者无法修改的三方库类型
// 无法修改其实现来增补函数，此时可以通过特化SerializeTraits<S>来支持序列化
// 特化时通过继承BasicSerializeTraits<SerializeTraits<S>>的方式
// 可以部分沿用默认协议机制，只进行局部调整
// 
// 协议最小集，实现这三个接口，就可以使S具备序列化能力
// struct S {
//     // 序列化
//     bool serialize(CodedOutputStream&) const;
//     // 反序列化
//     bool deserialize(CodedOutputStream&);
//     // 计算序列化大小
//     size_t calculate_serialized_size() const;
// };
//
// 在构成S的成员均支持了序列化操作的情况下
// 上述接口可以通过BABYLON_SERIALIZABLE宏来自动实现
// 具体用法参见babylon/serialization/aggregate.h
template <typename T>
class BasicSerializeTraits;
template <typename T, typename E>
class SerializeTraits : public BasicSerializeTraits<SerializeTraits<T>> {
private:
    using Base = BasicSerializeTraits<SerializeTraits<T>>;
    using WireType = typename Base::WireType;

// 实际偏特化时对于不需要改写的默认值可以留空
// 这里显式写出来只是为了方便阅读全集以及注释描述
public:
    // 是否支持序列化，默认false
    // 可以通过实现T::serialize/T::deserialize/T::calculate_serialized_size
    // 成员函数来表示支持序列化
    static constexpr bool SERIALIZABLE = Base::SERIALIZABLE;

    // 实际序列化协议采用Protocol Buffer Encoding
    // 要求每个元素需要具备一个Wire Type
    // 默认为WIRETYPE_LENGTH_DELIMITED，适用于大多数自定义类型
    // WireType的更多含义可以参考
    // https://protobuf.dev/programming-guides/encoding
    static constexpr WireType WIRE_TYPE = Base::WIRE_TYPE;

    // 序列化大小的计算是否采用缓存机制，默认false
    // 采用缓存机制的类型T，在调用其序列化函数serialize之前
    // 需要确保先调用过序列化大小计算函数calculate_serialized_size
    // 计算时会生成序列化函数中用到的缓存
    // 可以通过实现T::serialized_size_cached成员函数来表示采用缓存机制
    static constexpr bool SERIALIZED_SIZE_CACHED = Base::SERIALIZED_SIZE_CACHED;

    // 序列化大小的计算复杂度，有下列几个取值，默认COMPLEX
    // TRIVIAL：计算可以编译期完成
    // SIMPLE：计算复杂度O(1)
    // COMPLEX：计算复杂度超过O(1)
    // 复杂度主要用于指导作为成员时是否值得使用缓存机制
    // 可以通过T::BABYLON_SERIALIZED_SIZE_COMPLEXITY静态成员来设置
    static constexpr int SERIALIZED_SIZE_COMPLEXITY = Base::SERIALIZED_SIZE_COMPLEXITY;

    // 打印debug string时是否使用object展开方式，默认false
    // object展开：field {...}
    // 非object展开：field: ...
    // 可以通过T::BABYLON_PRINT_AS_OBJECT静态成员来设置
    static constexpr bool PRINT_AS_OBJECT = Base::PRINT_AS_OBJECT;

    // 序列化，默认空实现
    // 可以通过T::serialize成员函数来定制
    using Base::serialize;
    // 反序列化，默认返回失败
    // 可以通过T::deserialize成员函数来定制
    using Base::deserialize;
    // 计算序列化大小，默认返回0
    // 返回值为0的情况，调用序列化也不会产出任何输出
    // 因此框架『可能』『不会』调用相应的序列化函数
    // 实现时应该确保不会依赖序列化函数被调用而产生的副作用
    // 
    // 相应地，对于有可能返回值为0的类型
    // 反序列化实现也应该确保不被调用和使用0长度流的调用效果一致
    //
    // 可以通过T::calculate_serialized_size成员函数来定制
    using Base::calculate_serialized_size;
    // 利用缓存机制获取最近一次计算的自身序列化大小
    // 如果不支持缓存，默认调用calculate_serialized_size再计算一次
    // 可以通过T::serialized_size_cached成员函数来定制
    using Base::serialized_size_cached;
    // 输出debug string
    // 可以通过T::print成员函数来定制
    using Base::print;
};
#if __cplusplus < 201703L
template <typename T, typename E>
constexpr bool SerializeTraits<T, E>::SERIALIZABLE;
template <typename T, typename E>
constexpr typename SerializeTraits<T, E>::WireType SerializeTraits<T, E>::WIRE_TYPE;
template <typename T, typename E>
constexpr bool SerializeTraits<T, E>::SERIALIZED_SIZE_CACHED;
template <typename T, typename E>
constexpr int SerializeTraits<T, E>::SERIALIZED_SIZE_COMPLEXITY;
template <typename T, typename E>
constexpr bool SerializeTraits<T, E>::PRINT_AS_OBJECT;
#endif // __cplusplus < 201703L

namespace internal {
uintptr_t constexpr_symbol_generator();
}
BABYLON_NAMESPACE_END

#include "babylon/serialization/traits.hpp"

////////////////////////////////////////////////////////////////////////////////
// 注册一个类型的序列化器，使类型的反序列化和打印功能可以通过类名反射使用
#define BABYLON_REGISTER_SERIALIZER(cls) \
    BABYLON_REGISTER_SERIALIZER_IMPL(cls, BOOST_PP_CAT(BabylonAutoRegister, __COUNTER__))

#define BABYLON_REGISTER_SERIALIZER_IMPL(cls, register_name) \
    namespace { \
        struct register_name { \
            register_name() noexcept { \
                ::babylon::Serialization::register_serializer<cls>(); \
            } \
        } register_name; \
    }
////////////////////////////////////////////////////////////////////////////////

#endif // BABYLON_USE_PROTOBUF
