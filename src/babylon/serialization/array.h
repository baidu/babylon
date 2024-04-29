#pragma once

#include "babylon/serialization/traits.h"

BABYLON_NAMESPACE_BEGIN

template <typename T, size_t N>
class SerializeTraits<T[N]>
    : public BasicSerializeTraits<SerializeTraits<T[N]>> {
 private:
  using Value = T[N];
  using Base = BasicSerializeTraits<SerializeTraits<Value>>;
  using CodedOutputStream = typename Base::CodedOutputStream;
  using CodedInputStream = typename Base::CodedInputStream;
  using WireFormatLite = typename Base::WireFormatLite;
  using PrintStream = typename Base::PrintStream;

 public:
  static constexpr bool SERIALIZABLE = SerializeTraits<T>::SERIALIZABLE;
  static constexpr bool SERIALIZED_SIZE_CACHED =
      SerializeTraits<T>::SERIALIZED_SIZE_CACHED;
  static constexpr int SERIALIZED_SIZE_COMPLEXITY =
      SerializeTraits<T>::SERIALIZED_SIZE_COMPLEXITY ==
              SerializationHelper::SERIALIZED_SIZE_COMPLEXITY_TRIVIAL
          ? SerializationHelper::SERIALIZED_SIZE_COMPLEXITY_SIMPLE
          : SerializationHelper::SERIALIZED_SIZE_COMPLEXITY_COMPLEX;

  static void serialize(const Value& value, CodedOutputStream& os) noexcept {
#if ABSL_IS_LITTLE_ENDIAN
    if CONSTEXPR_SINCE_CXX17 (::std::is_same<float, T>::value ||
                              ::std::is_same<double, T>::value) {
      os.WriteRaw(value, N * sizeof(T));
    } else
#endif // ABSL_IS_LITTLE_ENDIAN
    {
      for (const auto& one_value : value) {
        SerializationHelper::serialize_packed_field(one_value, os);
      }
    }
  }

  static bool deserialize(CodedInputStream& is, Value& value) noexcept {
#if ABSL_IS_LITTLE_ENDIAN
    if CONSTEXPR_SINCE_CXX17 (::std::is_same<float, T>::value ||
                              ::std::is_same<double, T>::value) {
      if (ABSL_PREDICT_FALSE(!is.ReadRaw(value, N * sizeof(T)))) {
        return false;
      }
    } else
#endif // ABSL_IS_LITTLE_ENDIAN
    {
      for (size_t i = 0; i < N; ++i) {
        if (ABSL_PREDICT_FALSE(
                !SerializationHelper::deserialize_packed_field(is, value[i]))) {
          return false;
        }
      }
    }
    return true;
  }

  static size_t calculate_serialized_size(const Value& value) noexcept {
    if CONSTEXPR_SINCE_CXX17 (SerializeTraits<T>::SERIALIZED_SIZE_COMPLEXITY ==
                              SerializationHelper::
                                  SERIALIZED_SIZE_COMPLEXITY_TRIVIAL) {
      // TRIVIAL时计算不依赖value，随便传入一个地址用于类型匹配
      return N * SerializationHelper::calculate_serialized_size_packed_field(
                     value[0]);
    }
    size_t size = 0;
    for (const auto& one_value : value) {
      size += SerializationHelper::calculate_serialized_size_packed_field(
          one_value);
    }
    return size;
  }

  static bool print(const Value& value, PrintStream& ps) noexcept {
    bool first = true;
    if (ABSL_PREDICT_FALSE(!ps.print_raw("["))) {
      return false;
    }
    for (size_t i = 0; i < N; ++i) {
      if (first) {
        first = false;
      } else if (ABSL_PREDICT_FALSE(!ps.print_raw(", "))) {
        return false;
      }
      if (ABSL_PREDICT_FALSE(!SerializeTraits<T>::print(value[i], ps))) {
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
template <typename T, size_t N>
constexpr bool SerializeTraits<T[N]>::SERIALIZABLE;
#endif // __cplusplus < 201703L

BABYLON_NAMESPACE_END
