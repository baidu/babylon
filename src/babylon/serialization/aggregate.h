#pragma once

#include "babylon/serialization/traits.h"

#include <boost/preprocessor/punctuation/is_begin_parens.hpp>   // BOOST_PP_IS_BEGIN_PARENS
#include <boost/preprocessor/seq/for_each.hpp>                // BOOST_PP_SEQ_FOR_EACH_R
#include <boost/preprocessor/seq/for_each_i.hpp>                // BOOST_PP_SEQ_FOR_EACH_I_R
#include <boost/preprocessor/seq/variadic_seq_to_seq.hpp>       // BOOST_PP_VARIADIC_SEQ_TO_SEQ
#include <boost/preprocessor/stringize.hpp>                     // BOOST_PP_STRINGIZE
#include <boost/preprocessor/tuple/size.hpp>                    // BOOST_PP_TUPLE_ELEM
#include <boost/preprocessor/variadic/to_seq.hpp>               // BOOST_PP_VARIADIC_TO_SEQ

////////////////////////////////////////////////////////////////////////////////
// BABYLON_SERIALIZABLE begin
//
// 对于任意自定义结构S，如果其成员均满足SerializeTraits
// 那么就可以简单通过BABYLON_SERIALIZABLE宏来实现SerializeTraits所需接口
// 从而使自身也满足SerializeTraits
//
// 典型用法如下
// class S {
//     type a;
//     type b;
//     ...
//
//     BABYLON_SERIALIZABLE((a, 1)(b, 2)...);
// };
// 宏内部枚举出所有参与序列化的成员即可，允许存在不参与序列化的成员
// 不参与序列化的成员在序列化过程中会保持初始状态
// 每个参与序列化的成员需要指定一个唯一的编号（但无需连续/递增）
// 编号用于支持跨版本序列化/反序列化时稳定标识一个成员
// 概念等价于Protocol Buffer中的Field Number
// https://protobuf.dev/programming-guides/proto3/#assigning-field-numbers
//
// 如果不关心跨版本序列化，也可以采用如下简写方法
// class S {
//     type a;
//     type b;
//     ...
//
//     BABYLON_SERIALIZABLE(a, b, ...);
// };
// 宏展开时会自动为参与序列化的成员生成连续递增编号
// 因此如果后续调整顺序或删除成员，调整前后的序列化数据就无法相互解析了
//
// BABYLON_SERIALIZABLE仅实现对明确指定成员的序列化，当S继承自某个基类时
// 基类并不会参与序列化过程，如果需要包含基类，可以使用专为此场景定制的
// BABYLON_SERIALIZABLE_WITH_BASE宏，典型用法如下
// class S : public B {
//     type a;
//     type b;
//     ...
//
//     BABYLON_SERIALIZABLE_WITH_BASE((B, 1), (a, 2)(b, 3))
// };
// 实际序列化时基类会以『相当于一个成员』的方式参与到序列化过程当中
// 因此，也需要对基类设置一个唯一编号
//
// 如果不关心跨版本序列化，包含基类的宏也可以采用如下简写方法
// class S : public B {
//     type a;
//     type b;
//     ...
//
//     BABYLON_SERIALIZABLE_WITH_BASE(B, a, b)
// };
#define BABYLON_SERIALIZABLE(...) \
    __BABYLON_SERIALIZABLE_IMPL(, \
        BOOST_PP_VARIADIC_SEQ_TO_SEQ( \
            BOOST_PP_IF( \
                BOOST_PP_IS_BEGIN_PARENS(__VA_ARGS__), \
                __BABYLON_SERIALIZABLE_REMOVE_FIRST, \
                __BABYLON_SERIALIZABLE_AUTO_TAG_FROM \
            )(1, ##__VA_ARGS__) \
        ) \
    ) 

#define BABYLON_SERIALIZABLE_WITH_BASE(base, ...) \
    __BABYLON_SERIALIZABLE_IMPL(\
        BOOST_PP_VARIADIC_SEQ_TO_SEQ( \
            BOOST_PP_IF( \
                BOOST_PP_IS_BEGIN_PARENS(base), \
                __BABYLON_SERIALIZABLE_REMOVE_FIRST, \
                __BABYLON_SERIALIZABLE_AUTO_TAG_FROM \
            )(1, base) \
        ), \
        BOOST_PP_VARIADIC_SEQ_TO_SEQ( \
            BOOST_PP_IF( \
                BOOST_PP_IS_BEGIN_PARENS(base), \
                __BABYLON_SERIALIZABLE_REMOVE_FIRST, \
                __BABYLON_SERIALIZABLE_AUTO_TAG_FROM \
            )(2, ##__VA_ARGS__) \
        ) \
    )

#define __BABYLON_SERIALIZABLE_REMOVE_FIRST(first, ...) \
    __VA_ARGS__

#define __BABYLON_SERIALIZABLE_AUTO_TAG_FROM(begin_field_number, ...) \
    BOOST_PP_SEQ_FOR_EACH_I_R(\
        1, __BABYLON_SERIALIZABLE_AUTO_TAG_EACH, begin_field_number, \
        BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__)) \

#define __BABYLON_SERIALIZABLE_AUTO_TAG_EACH(r, begin_field_number, index, member) \
    (member, begin_field_number + index)

#define __BABYLON_SERIALIZABLE_IMPL(base, fields) \
    /* 所有成员需要能够序列化 */ \
    BOOST_PP_SEQ_FOR_EACH_R(1, __BABYLON_SERIALIZABLE_ASSERT_BASE, _, base) \
    BOOST_PP_SEQ_FOR_EACH_R(1, __BABYLON_SERIALIZABLE_ASSERT_FIELD, _, fields) \
    /* 所有成员都是TRIVIAL时，整体也是TRIVIAL，否则是SIMPLE */ \
    static constexpr int BABYLON_SERIALIZED_SIZE_COMPLEXITY = \
        (true \
            BOOST_PP_SEQ_FOR_EACH_R( \
                1, __BABYLON_SERIALIZABLE_TRIVIAL_BASE, _, base) \
            BOOST_PP_SEQ_FOR_EACH_R( \
                1, __BABYLON_SERIALIZABLE_TRIVIAL_FIELD, _, fields)) ? \
                    ::babylon::SerializationHelper::SERIALIZED_SIZE_COMPLEXITY_TRIVIAL : \
                    ::babylon::SerializationHelper::SERIALIZED_SIZE_COMPLEXITY_SIMPLE; \
    /* 通过成员聚合实现的序列化，都按照对象模式进行打印 */ \
    static constexpr bool BABYLON_PRINT_AS_OBJECT = true; \
    /* 计算并累加所有成员的序列化大小，并且对COMPLEX的成员在外部进行缓存加速 */ \
    /* 对非TRIVIAL成员较多的情况，会进一步对最终累加结果进行缓存加速 */ \
    size_t calculate_serialized_size() const noexcept { \
        size_t __babylon_tmp_size = 0 \
            BOOST_PP_SEQ_FOR_EACH_R(1, __BABYLON_SERIALIZABLE_CAL_SIZE_BASE, _, base) \
            BOOST_PP_SEQ_FOR_EACH_R(1, __BABYLON_SERIALIZABLE_CAL_SIZE_FIELD, _, fields) \
        ; \
        __babylon_cached_serialized_size = __babylon_tmp_size; \
        return __babylon_tmp_size; \
    } \
    /* 任意成员CACHED，则整体也是CACHED */ \
    /* 任意成员COMPLEX，则需要进行缓存加速，因此整体也是CACHED */ \
    /* SIMPLE成员过多，需要整体缓存加速，此时整体也是CACHED */ \
    static constexpr bool __babylon_need_cache() noexcept { \
        return sizeof(__babylon_cached_serialized_size) > 0 \
            BOOST_PP_SEQ_FOR_EACH_R( \
                1, __BABYLON_SERIALIZABLE_CACHED_BASE, _, base) \
            BOOST_PP_SEQ_FOR_EACH_R( \
                1, __BABYLON_SERIALIZABLE_CACHED_FIELD, _, fields) \
        ; \
    } \
    template <typename... Args, typename = typename ::std::enable_if< \
        (sizeof...(Args) > 0) || __babylon_need_cache()>::type> \
    size_t serialized_size_cached(Args...) const noexcept { \
        /* 如果存在整体缓存，直接使用缓存结果 */ \
        if CONSTEXPR_SINCE_CXX17 (sizeof(__babylon_cached_serialized_size) > 0) { \
            return static_cast<size_t>(__babylon_cached_serialized_size); \
        } \
        /* 否则累加各成员 */ \
        return 0 \
            BOOST_PP_SEQ_FOR_EACH_R(1, __BABYLON_SERIALIZABLE_CH_SIZE_BASE, _, base) \
            BOOST_PP_SEQ_FOR_EACH_R(1, __BABYLON_SERIALIZABLE_CH_SIZE_FIELD, _, fields) \
        ; \
    } \
    /* 实现SerializeTraits约定的序列化接口 */ \
    void serialize(::google::protobuf::io::CodedOutputStream& os) const noexcept { \
        BOOST_PP_SEQ_FOR_EACH_R(1, __BABYLON_SERIALIZABLE_SER_BASE, _, base) \
        BOOST_PP_SEQ_FOR_EACH_R(1, __BABYLON_SERIALIZABLE_SER_FIELD, _, fields) \
        (void) os; \
    } \
    /* 实现SerializeTraits约定的反序列化接口 */ \
    bool deserialize(::google::protobuf::io::CodedInputStream& is) noexcept { \
        const void* __babylon_tmp_data; \
        int __babylon_tmp_size; \
        while (is.GetDirectBufferPointer(&__babylon_tmp_data, \
                                         &__babylon_tmp_size)) { \
            /* 对齐Protocol Buffer协议 */ \
            /* message = [field] */ \
            /* field = tag data */ \
            /* tag = field_number << 3 | wire_type */ \
            auto __babylon_tmp_tag = is.ReadTag(); \
            switch (__babylon_tmp_tag >> 3) { \
                BOOST_PP_SEQ_FOR_EACH_R( \
                    1, __BABYLON_SERIALIZABLE_DER_BASE, _, base) \
                BOOST_PP_SEQ_FOR_EACH_R( \
                    1, __BABYLON_SERIALIZABLE_DER_FIELD, _, fields) \
                /* 对于未知的field_number尝试跳过 */ \
                default: \
                    if (ABSL_PREDICT_FALSE( \
                            !::babylon::SerializationHelper:: \
                                consume_unknown_field( \
                                    __babylon_tmp_tag, is))) { \
                        return false; \
                    } \
            } \
        } \
        return true; \
    } \
    /* 实现SerializeTraits约定的打印接口 */ \
    bool print(::babylon::Serialization::PrintStream& ps) const noexcept { \
        (void) ps; \
        return true \
            BOOST_PP_SEQ_FOR_EACH_R(1, __BABYLON_SERIALIZABLE_PRINT_BASE, _, base) \
            BOOST_PP_SEQ_FOR_EACH_R(1, __BABYLON_SERIALIZABLE_PRINT_FIELD, _, fields) \
        ; \
    } \
    /* 对于COMPLEX成员，定义配套的成员缓存 */ \
    BOOST_PP_SEQ_FOR_EACH_R(1, __BABYLON_SERIALIZABLE_CACHED_SIZE_BASE, _, base) \
    BOOST_PP_SEQ_FOR_EACH_R(1, __BABYLON_SERIALIZABLE_CACHED_SIZE_FIELD, _, fields) \
    /* 统计SIMPLE成员数量，过多时定义整体缓存 */ \
    mutable ::std::conditional<(10 > (0 \
            BOOST_PP_SEQ_FOR_EACH_R(1, __BABYLON_SERIALIZABLE_COUNT_FIELD, _, fields) \
        )), \
        ::babylon::ZeroSized, uint32_t>::type __babylon_cached_serialized_size {0}; \
    /* 授权给SerializationHelper确保接口函数可以被框架调用 */ \
    friend ::babylon::SerializationHelper;

#define __BABYLON_SERIALIZABLE_ASSERT_BASE(r, data, base) \
    static_assert(::babylon::SerializeTraits< \
        BOOST_PP_TUPLE_ELEM(0, base)>::SERIALIZABLE, \
        "base must be serializable"); \

#define __BABYLON_SERIALIZABLE_ASSERT_FIELD(r, data, field) \
    static_assert(::babylon::SerializeTraits< \
        decltype(BOOST_PP_TUPLE_ELEM(0, field))>::SERIALIZABLE, \
        "member must be serializable"); \

#define __BABYLON_SERIALIZABLE_TRIVIAL_BASE(r, data, base) \
    && (::babylon::SerializeTraits<\
            BOOST_PP_TUPLE_ELEM(0, base)>::SERIALIZED_SIZE_COMPLEXITY == \
        ::babylon::SerializationHelper::SERIALIZED_SIZE_COMPLEXITY_TRIVIAL)

#define __BABYLON_SERIALIZABLE_TRIVIAL_FIELD(r, data, field) \
    && (::babylon::SerializeTraits< \
            decltype(BOOST_PP_TUPLE_ELEM(0, field))>::SERIALIZED_SIZE_COMPLEXITY == \
        ::babylon::SerializationHelper::SERIALIZED_SIZE_COMPLEXITY_TRIVIAL)

#define __BABYLON_SERIALIZABLE_CACHED_BASE(r, data, base) \
    || ::babylon::SerializeTraits< \
            BOOST_PP_TUPLE_ELEM(0, base)>::SERIALIZED_SIZE_CACHED

#define __BABYLON_SERIALIZABLE_CACHED_FIELD(r, data, field) \
    || ::babylon::SerializeTraits< \
            decltype(BOOST_PP_TUPLE_ELEM(0, field))>::SERIALIZED_SIZE_CACHED

#define __BABYLON_SERIALIZABLE_SER_BASE(r, data, base) \
    ::babylon::SerializationHelper::serialize_field( \
        ::babylon::SerializationHelper::make_tag< \
                BOOST_PP_TUPLE_ELEM(0, base)>( \
            BOOST_PP_TUPLE_ELEM(1, base)), \
        *static_cast<const BOOST_PP_TUPLE_ELEM(0, base)*>(this), os, \
         __babylon_cached_size_base); \

#define __BABYLON_SERIALIZABLE_SER_FIELD(r, data, field) \
    ::babylon::SerializationHelper::serialize_field( \
        ::babylon::SerializationHelper::make_tag< \
                decltype(BOOST_PP_TUPLE_ELEM(0, field))>( \
            BOOST_PP_TUPLE_ELEM(1, field)), \
        BOOST_PP_TUPLE_ELEM(0, field), os, \
        BOOST_PP_CAT(__babylon_cached_size_field, \
            BOOST_PP_TUPLE_ELEM(0, field))); \

#define __BABYLON_SERIALIZABLE_DER_BASE(r, data, base) \
    case BOOST_PP_TUPLE_ELEM(1, base): \
        if (ABSL_PREDICT_FALSE( \
                !::babylon::SerializationHelper::deserialize_field( \
                        __babylon_tmp_tag, is, \
                        *static_cast<BOOST_PP_TUPLE_ELEM(0, base)*>( \
                            this)))) { \
            return false; \
        } \
        break;

#define __BABYLON_SERIALIZABLE_DER_FIELD(r, data, field) \
    case BOOST_PP_TUPLE_ELEM(1, field): \
        if (ABSL_PREDICT_FALSE( \
                !::babylon::SerializationHelper::deserialize_field( \
                        __babylon_tmp_tag, is, \
                        BOOST_PP_TUPLE_ELEM(0, field)))) { \
            return false; \
        } \
        break;

#define __BABYLON_SERIALIZABLE_CAL_SIZE_BASE(r, data, base) \
    + ::babylon::SerializationHelper::calculate_serialized_size_field( \
        ::babylon::SerializationHelper::make_tag_size< \
                BOOST_PP_TUPLE_ELEM(0, base)>( \
            BOOST_PP_TUPLE_ELEM(1, base)), \
        *static_cast<const BOOST_PP_TUPLE_ELEM(0, base)*>(this), \
        __babylon_cached_size_base)

#define __BABYLON_SERIALIZABLE_CAL_SIZE_FIELD(r, data, field) \
    + ::babylon::SerializationHelper::calculate_serialized_size_field( \
        ::babylon::SerializationHelper::make_tag_size< \
                decltype(BOOST_PP_TUPLE_ELEM(0, field))>( \
            BOOST_PP_TUPLE_ELEM(1, field)), \
        BOOST_PP_TUPLE_ELEM(0, field), \
        BOOST_PP_CAT(__babylon_cached_size_field, \
            BOOST_PP_TUPLE_ELEM(0, field)))

#define __BABYLON_SERIALIZABLE_CH_SIZE_BASE(r, data, base) \
    + ::babylon::SerializationHelper::serialized_size_cached_field( \
        ::babylon::SerializationHelper::make_tag_size< \
                BOOST_PP_TUPLE_ELEM(0, base)>( \
            BOOST_PP_TUPLE_ELEM(1, base)), \
        *static_cast<const BOOST_PP_TUPLE_ELEM(0, base)*>(this), \
        __babylon_cached_size_base)

#define __BABYLON_SERIALIZABLE_CH_SIZE_FIELD(r, data, field) \
    + ::babylon::SerializationHelper::serialized_size_cached_field( \
        ::babylon::SerializationHelper::make_tag_size< \
                decltype(BOOST_PP_TUPLE_ELEM(0, field))>( \
            BOOST_PP_TUPLE_ELEM(1, field)), \
        BOOST_PP_TUPLE_ELEM(0, field), \
        BOOST_PP_CAT(__babylon_cached_size_field, \
            BOOST_PP_TUPLE_ELEM(0, field)))

#define __BABYLON_SERIALIZABLE_CACHED_SIZE_BASE(r, data, base) \
    mutable ::std::conditional< \
            ::babylon::SerializeTraits< \
                    BOOST_PP_TUPLE_ELEM( \
                        0, base)>::SERIALIZED_SIZE_COMPLEXITY == \
                ::babylon::SerializationHelper::SERIALIZED_SIZE_COMPLEXITY_COMPLEX, \
            uint32_t, ::babylon::ZeroSized>::type \
        __babylon_cached_size_base {0};

#define __BABYLON_SERIALIZABLE_CACHED_SIZE_FIELD(r, data, field) \
    mutable ::std::conditional< \
            ::babylon::SerializeTraits< \
                    decltype(BOOST_PP_TUPLE_ELEM( \
                        0, field))>::SERIALIZED_SIZE_COMPLEXITY == \
                ::babylon::SerializationHelper::SERIALIZED_SIZE_COMPLEXITY_COMPLEX, \
            uint32_t, ::babylon::ZeroSized>::type \
        BOOST_PP_CAT(__babylon_cached_size_field, \
                     BOOST_PP_TUPLE_ELEM(0, field)) {0};

#define __BABYLON_SERIALIZABLE_PRINT_BASE(r, data, base) \
    && ::babylon::SerializationHelper::print_field( \
            "__base__", \
            *static_cast<const BOOST_PP_TUPLE_ELEM(0, base)*>(this), ps) \

#define __BABYLON_SERIALIZABLE_PRINT_FIELD(r, data, field) \
    && ::babylon::SerializationHelper::print_field( \
            BOOST_PP_STRINGIZE(BOOST_PP_TUPLE_ELEM(0, field)), \
            BOOST_PP_TUPLE_ELEM(0, field), ps)

#define __BABYLON_SERIALIZABLE_COUNT_FIELD(r, data, field) \
    + (::babylon::SerializeTraits< \
            decltype(BOOST_PP_TUPLE_ELEM( \
                    0, field))>::SERIALIZED_SIZE_COMPLEXITY == \
        ::babylon::SerializationHelper::SERIALIZED_SIZE_COMPLEXITY_COMPLEX ? 10 : ( \
            ::babylon::SerializeTraits< \
                decltype(BOOST_PP_TUPLE_ELEM( \
                        0, field))>::SERIALIZED_SIZE_COMPLEXITY == \
            ::babylon::SerializationHelper::SERIALIZED_SIZE_COMPLEXITY_TRIVIAL ? 0 : 1))
// BABYLON_SERIALIZABLE end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// BABYLON_COMPATIBLE begin
// TODO(lijiang01): 兼容之前的用法，后续统一到BABYLON_SERIALIZABLE
#define BABYLON_COMPATIBLE(...) \
    BABYLON_SERIALIZABLE(__VA_ARGS__)

#define BABYLON_COMPATIBLE_WITH_BASE(base, ...) \
    BABYLON_SERIALIZABLE_WITH_BASE(base, __VA_ARGS__)
// BABYLON_COMPATIBLE end
////////////////////////////////////////////////////////////////////////////////
