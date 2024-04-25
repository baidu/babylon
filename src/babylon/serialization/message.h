#pragma once

#include "babylon/serialization/traits.h"

#include <google/protobuf/message_lite.h>   // google::protobuf::MessageLite
#include <google/protobuf/text_format.h>    // google::protobuf::TextFormat::Printer

BABYLON_NAMESPACE_BEGIN

// google::protobuf::MessageLite
// proto定义的message均属于此范畴
template <typename T>
class SerializeTraits<T,
      typename ::std::enable_if<
        ::std::is_base_of<::google::protobuf::MessageLite, T>::value
        >::type> : public BasicSerializeTraits<SerializeTraits<T>> {
private:
    using Base = BasicSerializeTraits<SerializeTraits<T>>;
    using CodedOutputStream = typename Base::CodedOutputStream;
    using CodedInputStream = typename Base::CodedInputStream;
    using PrintStream = typename Base::PrintStream;

public:
    static constexpr bool SERIALIZABLE = true;
    static constexpr bool SERIALIZED_SIZE_CACHED = true;
    static constexpr int SERIALIZED_SIZE_COMPLEXITY =
        SerializationHelper::SERIALIZED_SIZE_COMPLEXITY_SIMPLE;
    static constexpr bool PRINT_AS_OBJECT = true;

    static void serialize(const T& value, CodedOutputStream& os) noexcept {
        value.SerializeWithCachedSizes(&os);
    }
    static bool deserialize(CodedInputStream& is, T& value) noexcept {
        return value.ParseFromCodedStream(&is);
    }
    static size_t calculate_serialized_size(const T& value) noexcept {
        return value.ByteSizeLong();
    }
    static size_t serialized_size_cached(const T& value) noexcept {
        return value.GetCachedSize();
    }
    static bool print(const T& value, PrintStream& ps) noexcept {
        ::google::protobuf::TextFormat::Printer printer;
        printer.SetInitialIndentLevel(ps.indent_level());
        printer.SetUseShortRepeatedPrimitives(true);
        ps.flush();
        return printer.Print(value, &ps.stream());
    }
};
#if __cplusplus < 201703L
template <typename T>
constexpr bool SerializeTraits<
    T, typename ::std::enable_if<
    ::std::is_base_of<::google::protobuf::MessageLite, T>::value
    >::type>::SERIALIZABLE;
#endif // __cplusplus < 201703L

BABYLON_NAMESPACE_END
