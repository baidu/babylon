#pragma once

#include "babylon/environment.h"

#include BABYLON_EXTERNAL(absl/base/attributes.h)       // ABSL_ATTRIBUTE_ALWAYS_INLINE

#ifdef ABSL_HAVE_ADDRESS_SANITIZER
#include <sanitizer/asan_interface.h>   // ASAN_*
#endif // ABSL_HAVE_ADDRESS_SANITIZER

BABYLON_NAMESPACE_BEGIN

struct SanitizerHelper {
    template <typename T>
    ABSL_ATTRIBUTE_ALWAYS_INLINE
    inline static T* poison(T* address) noexcept {
        return static_cast<T*>(poison(address, sizeof(*address)));
    }

    template <typename T>
    ABSL_ATTRIBUTE_ALWAYS_INLINE
    inline static T* unpoison(T* address) noexcept {
        return static_cast<T*>(unpoison(address, sizeof(*address)));
    }

    template <typename T>
    ABSL_ATTRIBUTE_ALWAYS_INLINE
    inline static T* poison(T* address, size_t size) noexcept {
        (void) size;
#ifdef ABSL_HAVE_ADDRESS_SANITIZER
        ASAN_POISON_MEMORY_REGION(address, size);
#endif // ABSL_HAVE_ADDRESS_SANITIZER
        return address;
    }

    template <typename T>
    ABSL_ATTRIBUTE_ALWAYS_INLINE
    inline static T* unpoison(T* address, size_t size) noexcept {
        (void) size;
#ifdef ABSL_HAVE_ADDRESS_SANITIZER
        ASAN_UNPOISON_MEMORY_REGION(address, size);
#endif // ABSL_HAVE_ADDRESS_SANITIZER
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
#ifdef ABSL_HAVE_ADDRESS_SANITIZER
            poison(_address, _size);
#endif // ABSL_HAVE_ADDRESS_SANITIZER
        }

        template <typename T>
        ABSL_ATTRIBUTE_ALWAYS_INLINE
        inline PoisonGuard(T* address) noexcept
            : PoisonGuard {address, sizeof(*address)} {}

        ABSL_ATTRIBUTE_ALWAYS_INLINE
        inline PoisonGuard(void* address, size_t size) noexcept
#ifdef ABSL_HAVE_ADDRESS_SANITIZER
                : _address {address}, _size {size} {
            unpoison(address, size);
        }
#else // !ABSL_HAVE_ADDRESS_SANITIZER
        {
            (void) address;
            (void) size;
        }
#endif // !ABSL_HAVE_ADDRESS_SANITIZER

    private:
#ifdef ABSL_HAVE_ADDRESS_SANITIZER
        void* _address;
        size_t _size;
#endif // ABSL_HAVE_ADDRESS_SANITIZER
    };
};

BABYLON_NAMESPACE_END
