#pragma once

#include "babylon/serialization/traits.h"

#include <vector>

BABYLON_NAMESPACE_BEGIN

template <typename T, typename A>
class SerializeTraits<::std::vector<T, A>>
    : public BasicSerializeTraits<SerializeTraits<::std::vector<T, A>>> {
 private:
  using Value = ::std::vector<T, A>;
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
              SerializationHelper::SERIALIZED_SIZE_COMPLEXITY_TRIVIAL
          ? SerializationHelper::SERIALIZED_SIZE_COMPLEXITY_SIMPLE
          : SerializationHelper::SERIALIZED_SIZE_COMPLEXITY_COMPLEX;

  static void serialize(const Value& value, CodedOutputStream& os) noexcept {
#if ABSL_IS_LITTLE_ENDIAN
    if CONSTEXPR_SINCE_CXX17 (::std::is_same<float, T>::value ||
                              ::std::is_same<double, T>::value) {
      os.WriteRaw(value.data(), value.size() * sizeof(T));
    } else
#endif // ABSL_IS_LITTLE_ENDIAN
    {
      for (const auto& one_value : value) {
        SerializationHelper::serialize_packed_field(one_value, os);
      }
    }
  }

  static bool deserialize(CodedInputStream& is, Value& value) noexcept {
    if CONSTEXPR_SINCE_CXX17 (::std::is_same<float, T>::value ||
                              ::std::is_same<double, T>::value) {
      auto num = static_cast<size_t>(is.BytesUntilLimit()) / sizeof(T);
      value.reserve(value.size() + num);
    }

    while (is.BytesUntilLimit() > 0) {
      value.emplace_back();
      if (ABSL_PREDICT_FALSE(!SerializationHelper::deserialize_packed_field(
              is, value.back()))) {
        value.pop_back();
        return false;
      }
    }
    return true;
  }

  static size_t calculate_serialized_size(const Value& value) noexcept {
    if CONSTEXPR_SINCE_CXX17 (SerializeTraits<T>::SERIALIZED_SIZE_COMPLEXITY ==
                              SerializationHelper::
                                  SERIALIZED_SIZE_COMPLEXITY_TRIVIAL) {
      // TRIVIAL时计算不依赖value，随便传入一个地址用于类型匹配
      return value.size() *
             SerializationHelper::calculate_serialized_size_packed_field(
                 value[0]);
    } else {
      size_t size = 0;
      for (const auto& one_value : value) {
        size += SerializationHelper::calculate_serialized_size_packed_field(
            one_value);
      }
      return size;
    }
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
template <typename T, typename A>
constexpr bool SerializeTraits<::std::vector<T, A>>::SERIALIZABLE;
#endif // __cplusplus < 201703L

// 由于std::vector<bool>特化采用了压缩存储，接口和其他类型不完全兼容，也特化一下
template <typename A>
class SerializeTraits<::std::vector<bool, A>>
    : public BasicSerializeTraits<SerializeTraits<::std::vector<bool, A>>> {
 private:
  using Value = ::std::vector<bool, A>;
  using Base = BasicSerializeTraits<SerializeTraits<Value>>;
  using CodedOutputStream = typename Base::CodedOutputStream;
  using CodedInputStream = typename Base::CodedInputStream;
  using PrintStream = typename Base::PrintStream;

 public:
  static constexpr bool SERIALIZABLE = true;
  static constexpr bool SERIALIZED_SIZE_CACHED = false;
  static constexpr int SERIALIZED_SIZE_COMPLEXITY =
      SerializationHelper::SERIALIZED_SIZE_COMPLEXITY_SIMPLE;

  static void serialize(const Value& value, CodedOutputStream& os) noexcept {
    for (bool one_value : value) {
      SerializationHelper::serialize_packed_field(one_value, os);
    }
  }

  static bool deserialize(CodedInputStream& is, Value& value) noexcept {
    while (is.BytesUntilLimit() > 0) {
      bool result;
      if (ABSL_PREDICT_FALSE(
              !SerializationHelper::deserialize_packed_field(is, result))) {
        return false;
      }
      value.push_back(result);
    }
    return true;
  }

  static size_t calculate_serialized_size(const Value& value) noexcept {
    static_assert(SerializationHelper::varint_size(0) == 1, "");
    static_assert(SerializationHelper::varint_size(1) == 1, "");
    return value.size();
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
      if (ABSL_PREDICT_FALSE(!SerializeTraits<bool>::print(one_value, ps))) {
        return false;
      }
    }
    if (ABSL_PREDICT_FALSE(!ps.print_raw("]"))) {
      return false;
    }
    return true;
  }
};

BABYLON_NAMESPACE_END
