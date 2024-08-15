#pragma once

#include "babylon/serialization/traits.h"
#include "babylon/type_traits.h" // BABYLON_*_INVOCABLE_CHECKER

BABYLON_NAMESPACE_BEGIN

// 支持SerializeTraits实现时用到的一些辅助功能
// 包括一些反射探测能力，以及嵌套序列化支持等
struct SerializationHelper {
  using WireFormatLite = ::google::protobuf::internal::WireFormatLite;
  using CodedInputStream = ::google::protobuf::io::CodedInputStream;
  using CodedOutputStream = ::google::protobuf::io::CodedOutputStream;
  using PrintStream = Serialization::PrintStream;

  BABYLON_HAS_STATIC_MEMBER_CHECKER(BABYLON_SERIALIZED_SIZE_COMPLEXITY,
                                    HasSerializedSizeComplexity)
  BABYLON_HAS_STATIC_MEMBER_CHECKER(BABYLON_PRINT_AS_OBJECT, HasPrintAsObject)

  BABYLON_DECLARE_MEMBER_INVOCABLE(serialize, SerializeInvocable)
  BABYLON_DECLARE_MEMBER_INVOCABLE(deserialize, DeserializeInvocable)
  BABYLON_DECLARE_MEMBER_INVOCABLE(calculate_serialized_size,
                                   CalculateSerializedSizeInvocable)
  BABYLON_DECLARE_MEMBER_INVOCABLE(serialized_size_cached,
                                   SerializedSizeCachedInvocable)
  BABYLON_DECLARE_MEMBER_INVOCABLE(empty, EmptyInvocable)
  BABYLON_DECLARE_MEMBER_INVOCABLE(print, PrintInvocable)

  // 序列化大小的计算复杂度超过O(1)
  static constexpr int SERIALIZED_SIZE_COMPLEXITY_COMPLEX = 0;
  // 序列化大小的计算复杂度为O(1)，也可能是通过缓存加速后达到
  static constexpr int SERIALIZED_SIZE_COMPLEXITY_SIMPLE = 1;
  // 序列化大小的计算可以编译期完成
  static constexpr int SERIALIZED_SIZE_COMPLEXITY_TRIVIAL = 2;

  // serialize
  template <typename T>
  inline static constexpr bool serialize_invocable() noexcept {
    return SerializeInvocable<T, CodedOutputStream&>::value;
  }
  template <typename T,
            typename ::std::enable_if<serialize_invocable<T>(), int>::type = 0>
  inline static void serialize(const T& value, CodedOutputStream& os) noexcept {
    value.serialize(os);
  }
  template <typename T,
            typename ::std::enable_if<!serialize_invocable<T>(), int>::type = 0>
  inline static void serialize(const T&, CodedOutputStream&) noexcept {}

  // deserialize
  template <typename T>
  inline static constexpr bool deserialize_invocable() noexcept {
    return DeserializeInvocable<T, CodedInputStream&>::value;
  }
  template <typename T, typename ::std::enable_if<deserialize_invocable<T>(),
                                                  int>::type = 0>
  inline static bool deserialize(CodedInputStream& is, T& value) noexcept {
    return value.deserialize(is);
  }
  template <typename T, typename ::std::enable_if<!deserialize_invocable<T>(),
                                                  int>::type = 0>
  inline static bool deserialize(CodedInputStream&, T&) noexcept {
    return false;
  }

  // calculate_serialized_size
  template <typename T>
  inline static constexpr bool calculate_serialized_size_invocable() noexcept {
    return CalculateSerializedSizeInvocable<T>::value;
  }
  template <typename T,
            typename ::std::enable_if<calculate_serialized_size_invocable<T>(),
                                      int>::type = 0>
  inline static size_t calculate_serialized_size(const T& value) noexcept {
    return value.calculate_serialized_size();
  }
  template <typename T,
            typename ::std::enable_if<!calculate_serialized_size_invocable<T>(),
                                      int>::type = 0>
  inline static size_t calculate_serialized_size(const T&) noexcept {
    return 0;
  }

  // serialized_size_cached
  template <typename T,
            typename ::std::enable_if<SerializedSizeCachedInvocable<T>::value,
                                      int>::type = 0>
  inline static size_t serialized_size_cached(const T& value) noexcept {
    return value.serialized_size_cached();
  }
  template <typename T,
            typename ::std::enable_if<!SerializedSizeCachedInvocable<T>::value,
                                      int>::type = 0>
  inline static size_t serialized_size_cached(const T& value) noexcept {
    return SerializeTraits<T>::calculate_serialized_size(value);
  }

  // print
  template <typename T>
  inline static constexpr bool print_invocable() noexcept {
    return PrintInvocable<T, PrintStream&>::value;
  }
  template <typename T,
            typename ::std::enable_if<print_invocable<T>(), int>::type = 0>
  inline static bool print(const T& value, PrintStream& ps) noexcept {
    return value.print(ps);
  }
  template <typename T,
            typename ::std::enable_if<!print_invocable<T>(), int>::type = 0>
  inline static bool print(const T&, PrintStream& ps) noexcept {
    return ps.print_raw("<type '") && ps.print_raw(TypeId<T>::ID.name) &&
           ps.print_raw("'>");
  }

  template <typename T,
            typename ::std::enable_if<HasSerializedSizeComplexity<T>::value,
                                      int>::type = 0>
  inline static constexpr int serialized_size_complexity() noexcept {
    return T::BABYLON_SERIALIZED_SIZE_COMPLEXITY;
  }
  template <typename T,
            typename ::std::enable_if<!HasSerializedSizeComplexity<T>::value,
                                      int>::type = 0>
  inline static constexpr int serialized_size_complexity() noexcept {
    return SERIALIZED_SIZE_COMPLEXITY_COMPLEX;
  }

  template <typename T, typename ::std::enable_if<HasPrintAsObject<T>::value,
                                                  int>::type = 0>
  inline static constexpr bool print_as_object() noexcept {
    return T::BABYLON_PRINT_AS_OBJECT;
  }
  template <typename T, typename ::std::enable_if<!HasPrintAsObject<T>::value,
                                                  int>::type = 0>
  inline static constexpr bool print_as_object() noexcept {
    return false;
  }

  inline static constexpr size_t varint_size(uint64_t value) noexcept {
#define BABYLON_TMP_CLZ static_cast<uint32_t>(__builtin_clzll(value | 0x1))
#define BABYLON_TMP_LOG2 static_cast<uint32_t>(63 ^ BABYLON_TMP_CLZ)
    return (BABYLON_TMP_LOG2 * 9 + 73) / 64;
#undef BABYLON_TMP_LOG2
#undef BABYLON_TMP_CLZ
  }

  template <typename T>
  inline static constexpr uint32_t make_tag(uint32_t field_number) noexcept {
    return (field_number << 3) | SerializeTraits<T>::WIRE_TYPE;
  }

  template <typename T>
  inline static constexpr uint32_t make_tag_size(
      uint32_t field_number) noexcept {
    return varint_size(make_tag<T>(field_number));
  }

  ////////////////////////////////////////////////////////////////////////////
  // 标准成员序列化格式
  // - wire_type in {varint, fixed32, fixed64}
  //   <tag:field_number|wire_type><serialized_data>
  // - wire_type is length-delimited
  //   <tag:field_number|wire_type><serialized_size><serialized_data>
  // 主要用于支持普通对象序列化实现
  template <typename T, typename C>
  inline static void serialize_field(uint32_t tag, const T& value,
                                     CodedOutputStream& os,
                                     const C& field_cache) noexcept {
    // 尝试优先使用外部缓存
    size_t size = field_cache;
    if CONSTEXPR_SINCE_CXX17 (sizeof(field_cache) == 0) {
      // 外部缓存不可用时尝试使用内部缓存，或者直接计算
      size = SerializeTraits<T>::serialized_size_cached(value);
    }
    // 空成员不参与序列化
    if (size == 0) {
      return;
    }

    os.WriteVarint32(tag);
    if CONSTEXPR_SINCE_CXX17 (SerializeTraits<T>::WIRE_TYPE ==
                              WireFormatLite::WIRETYPE_LENGTH_DELIMITED) {
      os.WriteVarint32(size);
    }
    SerializeTraits<T>::serialize(value, os);
  }

  template <typename T>
  inline static bool deserialize_field(uint32_t tag, CodedInputStream& is,
                                       T& value) noexcept {
#ifndef NDEBUG
    auto wire_type {tag & 0x7};
    if (ABSL_PREDICT_FALSE(wire_type != SerializeTraits<T>::WIRE_TYPE)) {
      return false;
    }
#endif // NDEBUG
    (void)tag;
    if CONSTEXPR_SINCE_CXX17 (SerializeTraits<T>::WIRE_TYPE ==
                              WireFormatLite::WIRETYPE_LENGTH_DELIMITED) {
      uint32_t length;
      auto saved_limit =
          is.PushLimit(is.ReadVarint32(&length) ? static_cast<int>(length) : 0);
      auto success = SerializeTraits<T>::deserialize(is, value);
      is.PopLimit(saved_limit);
      return success;
    } else {
      return SerializeTraits<T>::deserialize(is, value);
    }
  }

  template <typename T, typename C>
  inline size_t static calculate_serialized_size_field(
      uint32_t tag_size, const T& value, C& field_cache) noexcept {
    auto size = SerializeTraits<T>::calculate_serialized_size(value);
    // 空成员不参与序列化
    if (size == 0) {
      return 0;
    }

    // 尝试填充外部缓存
    field_cache = size;

    if CONSTEXPR_SINCE_CXX17 (SerializeTraits<T>::WIRE_TYPE ==
                              WireFormatLite::WIRETYPE_LENGTH_DELIMITED) {
      size += varint_size(size);
    }
    size += tag_size;
    return size;
  }

  template <typename T, typename C>
  inline size_t static serialized_size_cached_field(
      uint32_t tag_size, const T& value, const C& field_cache) noexcept {
    // 尝试优先使用外部缓存
    size_t size = field_cache;
    if CONSTEXPR_SINCE_CXX17 (sizeof(field_cache) == 0) {
      // 外部缓存不可用时尝试使用内部缓存，或者直接计算
      size = SerializeTraits<T>::serialized_size_cached(value);
    }

    // 空成员不参与序列化
    if (size == 0) {
      return 0;
    }

    if CONSTEXPR_SINCE_CXX17 (SerializeTraits<T>::WIRE_TYPE ==
                              WireFormatLite::WIRETYPE_LENGTH_DELIMITED) {
      size += varint_size(size);
    }
    size += tag_size;
    return size;
  }
  ////////////////////////////////////////////////////////////////////////////

  ////////////////////////////////////////////////////////////////////////////
  // packed成员序列化格式，扩展支持了length-delimited类型
  // - wire_type in {varint, fixed32, fixed64}
  //   <serialized_data>
  // - wire_type is length-delimited
  //   <serialized_size><serialized_data>
  // 主要用于支持容器类的实现
  template <typename T>
  inline static void serialize_packed_field(const T& value,
                                            CodedOutputStream& os) noexcept {
    if CONSTEXPR_SINCE_CXX17 (SerializeTraits<T>::WIRE_TYPE ==
                              WireFormatLite::WIRETYPE_LENGTH_DELIMITED) {
      auto size = SerializeTraits<T>::serialized_size_cached(value);
      os.WriteVarint32(size);
    }
    SerializeTraits<T>::serialize(value, os);
  }

  template <typename T>
  inline static bool deserialize_packed_field(CodedInputStream& is,
                                              T& value) noexcept {
    if CONSTEXPR_SINCE_CXX17 (SerializeTraits<T>::WIRE_TYPE ==
                              WireFormatLite::WIRETYPE_LENGTH_DELIMITED) {
      uint32_t length;
      auto saved_limit =
          is.PushLimit(is.ReadVarint32(&length) ? static_cast<int>(length) : 0);
      auto success = SerializeTraits<T>::deserialize(is, value);
      is.PopLimit(saved_limit);
      return success;
    } else {
      return SerializeTraits<T>::deserialize(is, value);
    }
  }

  template <typename T>
  inline static size_t calculate_serialized_size_packed_field(
      const T& value) noexcept {
    auto size = SerializeTraits<T>::calculate_serialized_size(value);
    if CONSTEXPR_SINCE_CXX17 (SerializeTraits<T>::WIRE_TYPE ==
                              WireFormatLite::WIRETYPE_LENGTH_DELIMITED) {
      size += varint_size(size);
    }
    return size;
  }

  template <typename T>
  inline static size_t serialized_size_cached_packed_field(
      const T& value) noexcept {
    auto size = SerializeTraits<T>::serialized_size_cached(value);
    if CONSTEXPR_SINCE_CXX17 (SerializeTraits<T>::WIRE_TYPE ==
                              WireFormatLite::WIRETYPE_LENGTH_DELIMITED) {
      size += varint_size(size);
    }
    return size;
  }
  ////////////////////////////////////////////////////////////////////////////

  template <typename T>
  inline static bool print_field(StringView field_name, const T& value,
                                 Serialization::PrintStream& ps) noexcept {
    if (!SerializeTraits<T>::PRINT_AS_OBJECT) {
      return ps.print_raw(field_name) && ps.print_raw(": ") &&
             SerializeTraits<T>::print(value, ps) && ps.start_new_line();
    } else {
      return ps.print_raw(field_name) && ps.print_raw(" {") && ps.indent() &&
             ps.start_new_line() && SerializeTraits<T>::print(value, ps) &&
             ps.outdent() && ps.print_raw("}") && ps.start_new_line();
    }
  }

  static bool consume_unknown_field(uint32_t tag,
                                    CodedInputStream& is) noexcept;
};

template <typename T>
class BasicSerializeTraits<SerializeTraits<T>> {
 protected:
  using CodedOutputStream = ::google::protobuf::io::CodedOutputStream;
  using CodedInputStream = ::google::protobuf::io::CodedInputStream;
  using WireFormatLite = ::google::protobuf::internal::WireFormatLite;
  using WireType = WireFormatLite::WireType;
  using PrintStream = Serialization::PrintStream;

 public:
  static constexpr bool SERIALIZABLE =
      SerializationHelper::serialize_invocable<T>() &&
      SerializationHelper::deserialize_invocable<T>() &&
      SerializationHelper::calculate_serialized_size_invocable<T>();

#if !__clang__
  static constexpr bool COMPATIBLE = SerializeTraits<T>::SERIALIZABLE;
#endif // !__clang__

  static constexpr WireType WIRE_TYPE =
      WireFormatLite::WIRETYPE_LENGTH_DELIMITED;

  static constexpr bool SERIALIZED_SIZE_CACHED =
      SerializationHelper::SerializedSizeCachedInvocable<T>::value;

  static constexpr int SERIALIZED_SIZE_COMPLEXITY =
      SerializationHelper::serialized_size_complexity<T>();

  static constexpr bool PRINT_AS_OBJECT =
      SerializationHelper::print_as_object<T>();

  // 序列化函数
  // 对标Message::SerializeToCodedStream
  inline static void serialize(const T& value, CodedOutputStream& os) noexcept {
    SerializationHelper::serialize(value, os);
  }

  // 反序列化函数
  // 对标Message::ParseFromCodedStream
  inline static bool deserialize(CodedInputStream& is, T& value) noexcept {
    return SerializationHelper::deserialize(is, value);
  }

  // 序列化预期大小计算函数
  // Message分包协议要求前缀序列化长度
  // 因此需要提供序列化动作发生前的单独长度计算能力
  // 对标Message::ByteSizeLong
  inline static size_t calculate_serialized_size(const T& value) noexcept {
    return SerializationHelper::calculate_serialized_size(value);
  }

  // 『如果存在』利用缓存获取最近一次调用calculate_serialized_size的结果
  // 默认实现不支持真实缓存，会通过再次调用calculate_serialized_size得到结果
  static size_t serialized_size_cached(const T& value) noexcept {
    return SerializationHelper::serialized_size_cached(value);
  }

  static bool print(const T& value, PrintStream& ps) noexcept {
    return SerializationHelper::print(value, ps);
  }

  inline static constexpr WireType wire_type() noexcept {
    return SerializeTraits<T>::WIRE_TYPE;
  }
};
#if !__clang__ && __cplusplus < 201703L
template <typename T>
constexpr bool BasicSerializeTraits<SerializeTraits<T>>::COMPATIBLE;
#endif // !__clang__ && __cplusplus < 201703L

////////////////////////////////////////////////////////////////////////////////
// Serialization begin
template <typename T>
inline size_t Serialization::calculate_serialized_size(
    const T& value) noexcept {
  return SerializeTraits<T>::calculate_serialized_size(value);
}

template <typename T>
bool Serialization::serialize_to_coded_stream(const T& value,
                                              CodedOutputStream& os) noexcept {
  if CONSTEXPR_SINCE_CXX17 (SerializeTraits<T>::SERIALIZED_SIZE_CACHED) {
    SerializeTraits<T>::calculate_serialized_size(value);
  }
  return serialize_to_coded_stream_with_cached_size(value, os);
}

template <typename T>
bool Serialization::serialize_to_coded_stream_with_cached_size(
    const T& value, CodedOutputStream& os) noexcept {
  if CONSTEXPR_SINCE_CXX17 (!SerializeTraits<T>::SERIALIZABLE) {
    return false;
  }
  SerializeTraits<T>::serialize(value, os);
  return !os.HadError();
}

template <typename T>
bool Serialization::serialize_to_string(const T& value,
                                        ::std::string& s) noexcept {
  s.clear();
  ::google::protobuf::io::StringOutputStream ss {&s};
  CodedOutputStream cs {&ss};
  return serialize_to_coded_stream(value, cs);
}

template <typename T>
bool Serialization::serialize_to_array_with_cached_size(const T& value,
                                                        void* buffer,
                                                        size_t size) noexcept {
  ::google::protobuf::io::ArrayOutputStream as {buffer, static_cast<int>(size)};
  CodedOutputStream cs {&as};
  return serialize_to_coded_stream_with_cached_size(value, cs);
}

template <typename T>
bool Serialization::parse_from_coded_stream(CodedInputStream& is,
                                            T& value) noexcept {
  return SerializeTraits<T>::deserialize(is, value);
}

template <typename T>
bool Serialization::parse_from_string(const ::std::string& s,
                                      T& value) noexcept {
  return parse_from_array(s.c_str(), s.size(), value);
}

template <typename T>
bool Serialization::parse_from_array(const char* data, size_t size,
                                     T& value) noexcept {
  CodedInputStream cs {reinterpret_cast<const uint8_t*>(data),
                       static_cast<int>(size)};
  return parse_from_coded_stream(cs, value);
}

template <typename T>
bool Serialization::print_to_stream(const T& value,
                                    ZeroCopyOutputStream& os) noexcept {
  PrintStream ps;
  ps.set_stream(os);
  return SerializeTraits<T>::print(value, ps);
}

template <typename T>
bool Serialization::print_to_string(const T& value, ::std::string& s) noexcept {
  s.clear();
  ::google::protobuf::io::StringOutputStream ss {&s};
  return print_to_stream(value, ss);
}

template <typename T>
void Serialization::register_serializer() noexcept {
  serializers().emplace(
      ::std::string {TypeId<T>::ID.name},
      ::std::unique_ptr<Serializer> {new DefaultSerializer<T> {}});
}
// Serialization end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Serialization::DefaultSerializer begin
template <typename T>
inline T& Serialization::DefaultSerializer<T>::mutable_value(
    Any& instance) noexcept {
  T* value = instance.get<T>();
  if (ABSL_PREDICT_FALSE(value == nullptr)) {
    // TODO(lijiang01): 支持不可默认构造的类型
    instance = ::std::unique_ptr<T> {new T};
    value = instance.get<T>();
  }
  return *value;
}

template <typename T>
bool Serialization::DefaultSerializer<T>::parse_from_coded_stream(
    CodedInputStream& is, Any& instance) noexcept {
  return Serialization::parse_from_coded_stream(is, mutable_value(instance));
}

template <typename T>
bool Serialization::DefaultSerializer<T>::parse_from_string(
    const ::std::string& str, Any& instance) noexcept {
  return Serialization::parse_from_string(str, mutable_value(instance));
}

template <typename T>
bool Serialization::DefaultSerializer<T>::parse_from_array(
    const char* data, size_t size, Any& instance) noexcept {
  return Serialization::parse_from_array(data, size, mutable_value(instance));
}

template <typename T>
bool Serialization::DefaultSerializer<T>::print_to_stream(
    const Any& instance, ZeroCopyOutputStream& os) noexcept {
  const T* value = instance.get<T>();
  if (ABSL_PREDICT_FALSE(value == nullptr)) {
    return false;
  }
  return Serialization::print_to_stream(*value, os);
}

template <typename T>
bool Serialization::DefaultSerializer<T>::print_to_string(
    const Any& instance, ::std::string& s) noexcept {
  const T* value = instance.get<T>();
  if (ABSL_PREDICT_FALSE(value == nullptr)) {
    return false;
  }
  return Serialization::print_to_string(*value, s);
}
// Serialization::DefaultSerializer end
////////////////////////////////////////////////////////////////////////////////

BABYLON_NAMESPACE_END
