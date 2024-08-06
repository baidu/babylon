#include "babylon/any.h"

#include "babylon/protect.h"

BABYLON_NAMESPACE_BEGIN

///////////////////////////////////////////////////////////////////////////////
// Any::TypeDescriptor begin
void Any::TypeDescriptor<void>::destructor(void*) noexcept {}

void Any::TypeDescriptor<void>::deleter(void*) noexcept {}

void Any::TypeDescriptor<void>::copy_constructor(void*, const void*) noexcept {}

void* Any::TypeDescriptor<void>::copy_creater(const void*) noexcept {
  return nullptr;
}

#if __cplusplus < 201703L
ABSL_ATTRIBUTE_WEAK constexpr Any::Descriptor
    Any::TypeDescriptor<void>::descriptor;
#endif // __cplusplus < 201703L

uintptr_t constexpr_symbol_generator();
ABSL_ATTRIBUTE_WEAK uintptr_t constexpr_symbol_generator() {
  return reinterpret_cast<uintptr_t>(Any::descriptor<void>());
}
// Any::TypeDescriptor end
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
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

#include "babylon/unprotect.h"
