#pragma once

#include "babylon/serialization/traits.h"

#if BABYLON_USE_PROTOBUF

BABYLON_NAMESPACE_BEGIN

// 32bit内的基础类型
#define BABYLON_TMP_GEN(type)                                                  \
  template <>                                                                  \
  class SerializeTraits<type>                                                  \
      : public BasicSerializeTraits<SerializeTraits<type>> {                   \
   public:                                                                     \
    static constexpr bool SERIALIZABLE = true;                                 \
    static constexpr WireType WIRE_TYPE = WireFormatLite::WIRETYPE_VARINT;     \
    static constexpr int SERIALIZED_SIZE_COMPLEXITY =                          \
        SerializationHelper::SERIALIZED_SIZE_COMPLEXITY_SIMPLE;                \
    static void serialize(const type& value, CodedOutputStream& os) noexcept { \
      os.WriteVarint32(static_cast<uint32_t>(value));                          \
    }                                                                          \
    static bool deserialize(CodedInputStream& is, type& value) noexcept {      \
      uint32_t uvalue;                                                         \
      if (ABSL_PREDICT_FALSE(!is.ReadVarint32(&uvalue))) {                     \
        return false;                                                          \
      }                                                                        \
      value = static_cast<type>(uvalue);                                       \
      return true;                                                             \
    }                                                                          \
    static size_t calculate_serialized_size(const type& value) noexcept {      \
      return CodedOutputStream::VarintSize32(static_cast<uint32_t>(value));    \
    }                                                                          \
    static bool print(const type& value, PrintStream& ps) noexcept {           \
      return ps.print_raw(::std::to_string(value));                            \
    }                                                                          \
  };

BABYLON_TMP_GEN(bool);
BABYLON_TMP_GEN(int8_t);
BABYLON_TMP_GEN(int16_t);
BABYLON_TMP_GEN(int32_t);
BABYLON_TMP_GEN(uint8_t);
BABYLON_TMP_GEN(uint16_t);
BABYLON_TMP_GEN(uint32_t);
#undef BABYLON_TMP_GEN

// 64bit的基础类型
#define BABYLON_TMP_GEN(type)                                                  \
  template <>                                                                  \
  class SerializeTraits<type>                                                  \
      : public BasicSerializeTraits<SerializeTraits<type>> {                   \
   public:                                                                     \
    static constexpr bool SERIALIZABLE = true;                                 \
    static constexpr WireType WIRE_TYPE = WireFormatLite::WIRETYPE_VARINT;     \
    static constexpr int SERIALIZED_SIZE_COMPLEXITY =                          \
        SerializationHelper::SERIALIZED_SIZE_COMPLEXITY_SIMPLE;                \
    static void serialize(const type& value, CodedOutputStream& os) noexcept { \
      os.WriteVarint64(static_cast<uint64_t>(value));                          \
    }                                                                          \
    static bool deserialize(CodedInputStream& is, type& value) noexcept {      \
      uint64_t uvalue;                                                         \
      if (ABSL_PREDICT_FALSE(!is.ReadVarint64(&uvalue))) {                     \
        return false;                                                          \
      }                                                                        \
      value = static_cast<type>(uvalue);                                       \
      return true;                                                             \
    }                                                                          \
    static size_t calculate_serialized_size(const type& value) noexcept {      \
      return CodedOutputStream::VarintSize64(static_cast<uint64_t>(value));    \
    }                                                                          \
    static bool print(const type& value, PrintStream& ps) noexcept {           \
      return ps.print_raw(::std::to_string(value));                            \
    }                                                                          \
  };

BABYLON_TMP_GEN(int64_t);
BABYLON_TMP_GEN(uint64_t);
#undef BABYLON_TMP_GEN

// float
template <>
class SerializeTraits<float>
    : public BasicSerializeTraits<SerializeTraits<float>> {
 public:
  static constexpr bool SERIALIZABLE = true;
  static constexpr WireType WIRE_TYPE = WireFormatLite::WIRETYPE_FIXED32;
  static constexpr int SERIALIZED_SIZE_COMPLEXITY =
      SerializationHelper::SERIALIZED_SIZE_COMPLEXITY_TRIVIAL;

  static void serialize(const float& value, CodedOutputStream& os) noexcept {
    os.WriteLittleEndian32(WireFormatLite::EncodeFloat(value));
  }
  static bool deserialize(CodedInputStream& is, float& value) noexcept {
    uint32_t uvalue;
    if (ABSL_PREDICT_FALSE(!is.ReadLittleEndian32(&uvalue))) {
      return false;
    }
    value = WireFormatLite::DecodeFloat(uvalue);
    return true;
  }
  static constexpr size_t calculate_serialized_size(const float&) noexcept {
    return 4;
  }
  static bool print(const float& value, PrintStream& ps) noexcept {
    return ps.print_raw(::std::to_string(value));
  }
};

// double
template <>
class SerializeTraits<double>
    : public BasicSerializeTraits<SerializeTraits<double>> {
 public:
  static constexpr bool SERIALIZABLE = true;
  static constexpr WireType WIRE_TYPE = WireFormatLite::WIRETYPE_FIXED64;
  static constexpr int SERIALIZED_SIZE_COMPLEXITY =
      SerializationHelper::SERIALIZED_SIZE_COMPLEXITY_TRIVIAL;

  static void serialize(const double& value, CodedOutputStream& os) noexcept {
    os.WriteLittleEndian64(WireFormatLite::EncodeDouble(value));
  }
  static bool deserialize(CodedInputStream& is, double& value) noexcept {
    uint64_t uvalue;
    if (ABSL_PREDICT_FALSE(!is.ReadLittleEndian64(&uvalue))) {
      return false;
    }
    value = WireFormatLite::DecodeDouble(uvalue);
    return true;
  }
  static constexpr size_t calculate_serialized_size(const double&) noexcept {
    return 8;
  }
  static bool print(const double& value, PrintStream& ps) noexcept {
    return ps.print_raw(::std::to_string(value));
  }
};

// enum
template <typename T>
class SerializeTraits<T,
                      typename ::std::enable_if<::std::is_enum<T>::value>::type>
    : public BasicSerializeTraits<SerializeTraits<T>> {
 private:
  using Base = BasicSerializeTraits<SerializeTraits<T>>;
  using CodedOutputStream = typename Base::CodedOutputStream;
  using CodedInputStream = typename Base::CodedInputStream;
  using WireFormatLite = typename Base::WireFormatLite;
  using WireType = typename WireFormatLite::WireType;
  using PrintStream = typename Base::PrintStream;

 public:
  static constexpr bool SERIALIZABLE = true;
  static constexpr WireType WIRE_TYPE = WireFormatLite::WIRETYPE_VARINT;
  static constexpr int SERIALIZED_SIZE_COMPLEXITY =
      SerializationHelper::SERIALIZED_SIZE_COMPLEXITY_SIMPLE;

  static void serialize(const T& value, CodedOutputStream& os) noexcept {
    os.WriteVarint64(static_cast<uint64_t>(value));
  }
  static bool deserialize(CodedInputStream& is, T& value) noexcept {
    uint64_t uvalue;
    if (ABSL_PREDICT_FALSE(!is.ReadVarint64(&uvalue))) {
      return false;
    }
    value = static_cast<T>(uvalue);
    return true;
  }
  static size_t calculate_serialized_size(const T& value) noexcept {
    return CodedOutputStream::VarintSize64(static_cast<uint64_t>(value));
  }
  static bool print(const T& value, PrintStream& ps) noexcept {
    return ps.print_raw(::std::to_string(static_cast<int64_t>(value)));
  }
};
#if __cplusplus < 201703L
template <typename T>
constexpr bool SerializeTraits<
    T, typename ::std::enable_if<::std::is_enum<T>::value>::type>::SERIALIZABLE;
template <typename T>
constexpr typename SerializeTraits<
    T, typename ::std::enable_if<::std::is_enum<T>::value>::type>::WireType
    SerializeTraits<T, typename ::std::enable_if<
                           ::std::is_enum<T>::value>::type>::WIRE_TYPE;
template <typename T>
constexpr int SerializeTraits<
    T, typename ::std::enable_if<::std::is_enum<T>::value>::type>::
    SERIALIZED_SIZE_COMPLEXITY;
#endif // __cplusplus < 201703L

BABYLON_NAMESPACE_END

#endif // BABYLON_USE_PROTOBUF
