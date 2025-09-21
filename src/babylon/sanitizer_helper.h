#pragma once

#include "babylon/environment.h"

// clang-format off
#include BABYLON_EXTERNAL(absl/base/config.h) // ABSL_HAVE_ADDRESS_SANITIZER ABSL_HAVE_LEAK_SANITIZER
#include BABYLON_EXTERNAL(absl/base/attributes.h) // ABSL_ATTRIBUTE_ALWAYS_INLINE
// clang-format on

#if BABYLON_HAS_ADDRESS_SANITIZER
#include <sanitizer/asan_interface.h> // ASAN_*

#endif // BABYLON_HAS_ADDRESS_SANITIZER

#if defined(ABSL_HAVE_LEAK_SANITIZER) || BABYLON_HAS_ADDRESS_SANITIZER
#include <sanitizer/lsan_interface.h> // __lsan_*
#define BABYLON_HAVE_LEAK_SANITIZER 1
#endif // ABSL_HAVE_LEAK_SANITIZER || BABYLON_HAS_ADDRESS_SANITIZER

BABYLON_NAMESPACE_BEGIN

struct SanitizerHelper {
  template <typename T>
  ABSL_ATTRIBUTE_ALWAYS_INLINE inline static T* poison(T* address) noexcept {
    return static_cast<T*>(poison(address, sizeof(*address)));
  }

  template <typename T>
  ABSL_ATTRIBUTE_ALWAYS_INLINE inline static T* unpoison(T* address) noexcept {
    return static_cast<T*>(unpoison(address, sizeof(*address)));
  }

  template <typename T>
  ABSL_ATTRIBUTE_ALWAYS_INLINE inline static T* poison(T* address,
                                                       size_t size) noexcept {
    (void)size;
#if BABYLON_HAS_ADDRESS_SANITIZER
    ASAN_POISON_MEMORY_REGION(address, size);
#endif // BABYLON_HAS_ADDRESS_SANITIZER
    return address;
  }

  template <typename T>
  ABSL_ATTRIBUTE_ALWAYS_INLINE inline static T* unpoison(T* address,
                                                         size_t size) noexcept {
    (void)size;
#if BABYLON_HAS_ADDRESS_SANITIZER
    ASAN_UNPOISON_MEMORY_REGION(address, size);
#endif // BABYLON_HAS_ADDRESS_SANITIZER
    return address;
  }

  class PoisonGuard {
   public:
    PoisonGuard(PoisonGuard&&) = delete;
    PoisonGuard(const PoisonGuard&) = delete;
    PoisonGuard& operator=(PoisonGuard&&) = delete;
    PoisonGuard& operator=(const PoisonGuard&) = delete;

    ABSL_ATTRIBUTE_ALWAYS_INLINE
    inline ~PoisonGuard() noexcept {
#if BABYLON_HAS_ADDRESS_SANITIZER
      poison(_address, _size);
#endif // BABYLON_HAS_ADDRESS_SANITIZER
    }

    template <typename T>
    ABSL_ATTRIBUTE_ALWAYS_INLINE inline PoisonGuard(T* address) noexcept
        : PoisonGuard {address, sizeof(*address)} {}

    ABSL_ATTRIBUTE_ALWAYS_INLINE
    inline PoisonGuard(void* address, size_t size) noexcept
#if BABYLON_HAS_ADDRESS_SANITIZER
        : _address {address}, _size {size} {
      unpoison(address, size);
    }
#else  // !BABYLON_HAS_ADDRESS_SANITIZER
    {
      (void)address;
      (void)size;
    }
#endif // !BABYLON_HAS_ADDRESS_SANITIZER

   private:
#if BABYLON_HAS_ADDRESS_SANITIZER
    void* _address;
    size_t _size;
#endif // BABYLON_HAS_ADDRESS_SANITIZER
  };
};

#ifdef BABYLON_HAVE_LEAK_SANITIZER
class LeakCheckDisabler {
 public:
  LeakCheckDisabler() : _disable_leak_check(true) {
    __lsan_disable();
  }
  LeakCheckDisabler(bool disable_leak_check)
      : _disable_leak_check(disable_leak_check) {
    if (_disable_leak_check) {
      __lsan_disable();
    }
  }
  LeakCheckDisabler(const LeakCheckDisabler&) = delete;
  LeakCheckDisabler& operator=(const LeakCheckDisabler&) = delete;
  ~LeakCheckDisabler() {
    if (_disable_leak_check) {
      __lsan_enable();
    }
  }
 private:
  bool _disable_leak_check;
};

#define BABYLON_LEAK_CHECK_DISABLER(...)                \
  LeakCheckDisabler __disabler_##__LINE__{__VA_ARGS__}; \
  ((void)0)

#else
#define BABYLON_LEAK_CHECK_DISABLER(...) ((void)0)
#endif // BABYLON_HAVE_LEAK_SANITIZER

BABYLON_NAMESPACE_END
