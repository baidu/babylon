#pragma once

#include "babylon/serialization/traits.h"

#include <string>

BABYLON_NAMESPACE_BEGIN

template <typename CT, typename A>
class SerializeTraits<::std::basic_string<char, CT, A>>
    : public BasicSerializeTraits<
          SerializeTraits<::std::basic_string<char, CT, A>>> {
 private:
  using Value = ::std::basic_string<char, CT, A>;
  using Base = BasicSerializeTraits<SerializeTraits<Value>>;
  using CodedOutputStream = typename Base::CodedOutputStream;
  using CodedInputStream = typename Base::CodedInputStream;
  using PrintStream = typename Base::PrintStream;

 public:
  static constexpr bool SERIALIZABLE = true;
  static constexpr int SERIALIZED_SIZE_COMPLEXITY =
      SerializationHelper::SERIALIZED_SIZE_COMPLEXITY_SIMPLE;

  static void serialize(const Value& value, CodedOutputStream& os) noexcept {
    os.WriteString(value);
  }

  static bool deserialize(CodedInputStream& is, Value& value) noexcept {
    const void* data;
    int size;
    value.clear();
    while (is.GetDirectBufferPointer(&data, &size)) {
      value.append(reinterpret_cast<const char*>(data),
                   static_cast<size_t>(size));
      is.Skip(size);
    }
    return true;
  }

  static size_t calculate_serialized_size(const Value& value) noexcept {
    return value.size();
  }

  static bool print(const Value& value, PrintStream& ps) noexcept {
    return ps.print_string(value);
  }
};

#if __cplusplus < 201703L
template <typename CT, typename A>
constexpr bool SerializeTraits<::std::basic_string<char, CT, A>>::SERIALIZABLE;
template <typename CT, typename A>
constexpr int SerializeTraits<
    ::std::basic_string<char, CT, A>>::SERIALIZED_SIZE_COMPLEXITY;
#endif // __cplusplus < 201703L

BABYLON_NAMESPACE_END
