#pragma once

#include "babylon/serialization/traits.h"

#include <unordered_map>

BABYLON_NAMESPACE_BEGIN

template <typename K, typename V, typename H, typename E, typename A>
class SerializeTraits<::std::unordered_map<K, V, H, E, A>> :
        public BasicSerializeTraits<
            SerializeTraits<::std::unordered_map<K, V, H, E, A>>> {
private:
    using Value = ::std::unordered_map<K, V, H, E, A>;
    using Base = BasicSerializeTraits<SerializeTraits<Value>>;
    using CodedOutputStream = typename Base::CodedOutputStream;
    using CodedInputStream = typename Base::CodedInputStream;
    using WireFormatLite = typename Base::WireFormatLite;
    using PrintStream = typename Base::PrintStream;

public:
    static constexpr bool SERIALIZABLE = SerializeTraits<K>::SERIALIZABLE
        && SerializeTraits<V>::SERIALIZABLE;
    static constexpr bool SERIALIZED_SIZE_CACHED =
        SerializeTraits<K>::SERIALIZED_SIZE_CACHED ||
        SerializeTraits<V>::SERIALIZED_SIZE_CACHED;

    static void serialize(const Value& value, CodedOutputStream& os) noexcept {
        for (const auto& one_value : value) {
            SerializationHelper::serialize_packed_field(one_value.first, os);
            SerializationHelper::serialize_packed_field(one_value.second, os);
        }
    }

    static bool deserialize(CodedInputStream& is, Value& value) noexcept {
        const void* data = nullptr;
        int size = 0;
        while (is.GetDirectBufferPointer(&data, &size)) {
            K k;
            if (ABSL_PREDICT_FALSE(!SerializationHelper::deserialize_packed_field(is, k))) {
                return false;
            }
            V v;
            if (ABSL_PREDICT_FALSE(!SerializationHelper::deserialize_packed_field(is, v))) {
                return false;
            }
            value.emplace(::std::move(k), ::std::move(v));
        }
        return true;
    }

    static size_t calculate_serialized_size(const Value& value) noexcept {
        size_t size = 0;
        for (const auto& one_value : value) {
            size += SerializationHelper::calculate_serialized_size_packed_field(one_value.first);
            size += SerializationHelper::calculate_serialized_size_packed_field(one_value.second);
        }
        return size;
    }

    static size_t serialized_size_cached(const Value& value) noexcept {
        size_t size = 0;
        for (const auto& one_value : value) {
            size += SerializationHelper::serialized_size_cached_packed_field(one_value.first);
            size += SerializationHelper::serialized_size_cached_packed_field(one_value.second);
        }
        return size;
    }

    static bool print(const Value& value, PrintStream& ps) noexcept {
        if (value.empty()) {
            return ps.print_raw("[]");
        }
        if (ABSL_PREDICT_FALSE(!ps.print_raw("[")
                    || !ps.indent())) {
            return false;
        }
        for (const auto& one_value : value) {
            if (ABSL_PREDICT_FALSE(!ps.start_new_line()
                    || !ps.print_raw("{")
                    || !ps.indent()
                    || !ps.start_new_line())) {
                return false;
            }
            if (ABSL_PREDICT_FALSE(
                    !SerializationHelper::print_field(
                        "key", one_value.first, ps))) {
                return false;
            }
            if (ABSL_PREDICT_FALSE(
                    !SerializationHelper::print_field(
                        "value", one_value.second, ps))) {
                return false;
            }
            if (ABSL_PREDICT_FALSE(!ps.outdent()
                        || !ps.print_raw("}"))) {
                return false;
            }
        }
        if (ABSL_PREDICT_FALSE(!ps.outdent()
                    || !ps.start_new_line()
                    || !ps.print_raw("]"))) {
            return false;
        }
        return true;
    }
};
#if __cplusplus < 201703L
template <typename K, typename V, typename H, typename E, typename A>
constexpr bool SerializeTraits<::std::unordered_map<K, V, H, E, A>>::SERIALIZABLE;
#endif // __cplusplus < 201703L

BABYLON_NAMESPACE_END
