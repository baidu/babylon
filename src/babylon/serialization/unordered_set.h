#pragma once

#include "babylon/serialization/traits.h"

#include <unordered_set>

BABYLON_NAMESPACE_BEGIN

template <typename T, typename H, typename E, typename A>
class SerializeTraits<::std::unordered_set<T, H, E, A>> :
        public BasicSerializeTraits<
            SerializeTraits<::std::unordered_set<T, H, E, A>>> {
private:
    using Value = ::std::unordered_set<T, H, E, A>;
    using Base = BasicSerializeTraits<SerializeTraits<Value>>;
    using CodedOutputStream = typename Base::CodedOutputStream;
    using CodedInputStream = typename Base::CodedInputStream;
    using PrintStream = typename Base::PrintStream;

public:
    static constexpr bool SERIALIZABLE = SerializeTraits<T>::SERIALIZABLE;
    static constexpr bool SERIALIZED_SIZE_CACHED =
        SerializeTraits<T>::SERIALIZED_SIZE_CACHED;
    static constexpr int SERIALIZED_SIZE_COMPLEXITY =
        SerializeTraits<T>::SERIALIZED_SIZE_COMPLEXITY ==
                SerializationHelper::SERIALIZED_SIZE_COMPLEXITY_TRIVIAL ?
            SerializationHelper::SERIALIZED_SIZE_COMPLEXITY_SIMPLE :
            SerializationHelper::SERIALIZED_SIZE_COMPLEXITY_COMPLEX;

    static void serialize(const Value& value, CodedOutputStream& os) noexcept {
        for (const auto& one_value : value) {
            SerializationHelper::serialize_packed_field(one_value, os);
        }
    }

    static bool deserialize(CodedInputStream& is, Value& value) noexcept {
        const void* data = nullptr;
        int size = 0;
        while (is.GetDirectBufferPointer(&data, &size)) {
            T result;
            if (ABSL_PREDICT_FALSE(!SerializationHelper::deserialize_packed_field(is, result))) {
                return false;
            }
            value.emplace(::std::move(result));
        }
        return true;
    }

    static size_t calculate_serialized_size(const Value& value) noexcept {
        size_t size = 0;
        for (const auto& one_value : value) {
            size += SerializationHelper::calculate_serialized_size_packed_field(one_value);
        }
        return size;
    }

    static size_t serialized_size_cached(const Value& value) noexcept {
        size_t size = 0;
        for (const auto& one_value : value) {
            size += SerializationHelper::serialized_size_cached_packed_field(one_value);
        }
        return size;
    }

    static bool print(const Value& value, PrintStream& ps) noexcept {
        bool first = true;
        if (ABSL_PREDICT_FALSE(!ps.print_raw("["))) {
            return false;
        }
        for (const auto& one_value : value) {
            if (first) {
                first = false;
            } else if (ABSL_PREDICT_FALSE(!ps.print_raw(", "))) {
                return false;
            }
            if (ABSL_PREDICT_FALSE(!SerializeTraits<T>::print(one_value, ps))) {
                return false;
            }
        }
        if (ABSL_PREDICT_FALSE(!ps.print_raw("]"))) {
            return false;
        }
        return true;
    }
};
#if __cplusplus < 201703L
template <typename T, typename H, typename E, typename A>
constexpr bool SerializeTraits<::std::unordered_set<T, H, E, A>>::SERIALIZABLE;
#endif // __cplusplus < 201703L

BABYLON_NAMESPACE_END
