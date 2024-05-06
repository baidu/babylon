#include "babylon/any.h"

BABYLON_NAMESPACE_BEGIN

Any::Any(const Descriptor* descriptor, void* instance) noexcept
    : _meta {.v = reinterpret_cast<uint64_t>(descriptor) |
                  static_cast<uint64_t>(HolderType::INSTANCE) << 56 |
                  static_cast<uint64_t>(Type::INSTANCE) << 48},
      _holder {.pointer_value = instance} {}

Any& Any::assign(const Descriptor* descriptor, void* instance) noexcept {
  destroy();
  _meta.v = reinterpret_cast<uint64_t>(descriptor) |
            static_cast<uint64_t>(HolderType::INSTANCE) << 56 |
            static_cast<uint64_t>(Type::INSTANCE) << 48;
  _holder.pointer_value = instance;
  return *this;
}

Any& Any::cref(const Descriptor* descriptor, const void* instance) noexcept {
  destroy();
  _meta.v = reinterpret_cast<uint64_t>(descriptor) |
            static_cast<uint64_t>(HolderType::CONST_REFERENCE) << 56 |
            static_cast<uint64_t>(Type::INSTANCE) << 48;
  _holder.const_pointer_value = instance;
  return *this;
}

Any& Any::ref(const Descriptor* descriptor, const void* instance) noexcept {
  return cref(descriptor, instance);
}

Any& Any::ref(const Descriptor* descriptor, void* instance) noexcept {
  destroy();
  _meta.v = reinterpret_cast<uint64_t>(descriptor) |
            static_cast<uint64_t>(HolderType::MUTABLE_REFERENCE) << 56 |
            static_cast<uint64_t>(Type::INSTANCE) << 48;
  _holder.pointer_value = instance;
  return *this;
}

BABYLON_NAMESPACE_END
