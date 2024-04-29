#pragma once

#include "babylon/serialization/traits.h"

#include <memory>

BABYLON_NAMESPACE_BEGIN

template <typename T>
class SerializeTraits<::std::unique_ptr<T>>
    : public BasicSerializeTraits<SerializeTraits<::std::unique_ptr<T>>> {
 private:
  using Value = ::std::unique_ptr<T>;
  using Base = BasicSerializeTraits<SerializeTraits<Value>>;
  using CodedOutputStream = typename Base::CodedOutputStream;
  using CodedInputStream = typename Base::CodedInputStream;
  using WireFormatLite = typename Base::WireFormatLite;
  using WireType = typename Base::WireType;
  using PrintStream = typename Base::PrintStream;
  using MutableType = typename ::std::remove_const<T>::type;

 public:
  static constexpr bool SERIALIZABLE =
      SerializeTraits<MutableType>::SERIALIZABLE;
  static constexpr bool SERIALIZED_SIZE_CACHED =
      SerializeTraits<MutableType>::SERIALIZED_SIZE_CACHED;
  static constexpr int SERIALIZED_SIZE_COMPLEXITY =
      SerializeTraits<MutableType>::SERIALIZED_SIZE_COMPLEXITY;
  static constexpr WireType WIRE_TYPE = SerializeTraits<MutableType>::WIRE_TYPE;
  static constexpr bool PRINT_AS_OBJECT =
      SerializeTraits<MutableType>::PRINT_AS_OBJECT;

  static void serialize(const Value& value, CodedOutputStream& os) noexcept {
    if (value) {
      SerializeTraits<MutableType>::serialize(*value, os);
    }
  }

  static bool deserialize(CodedInputStream& is, Value& value) noexcept {
    const void* data = nullptr;
    int size = 0;
    if (is.GetDirectBufferPointer(&data, &size)) {
      if (!value) {
        // TODO(lijiang01): 升级支持不可默认构造类型
        value.reset(new MutableType);
      }
      return SerializeTraits<MutableType>::deserialize(
          is, const_cast<MutableType&>(*value));
    }
    return true;
  }

  static size_t calculate_serialized_size(const Value& value) noexcept {
    return value
               ? SerializeTraits<MutableType>::calculate_serialized_size(*value)
               : 0;
  }

  static size_t serialized_size_cached(const Value& value) noexcept {
    return value ? SerializeTraits<MutableType>::serialized_size_cached(*value)
                 : 0;
  }

  static bool print(const Value& value, PrintStream& ps) noexcept {
    return value ? SerializeTraits<MutableType>::print(*value, ps) : true;
  }
};
#if __cplusplus < 201703L
template <typename T>
constexpr bool SerializeTraits<::std::unique_ptr<T>>::SERIALIZABLE;
#endif // __cplusplus < 201703L

BABYLON_NAMESPACE_END
