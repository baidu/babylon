#pragma once

#include "babylon/environment.h"

#if BABYLON_HAS_INCLUDE("absl/strings/internal/resize_uninitialized.h")

#include BABYLON_EXTERNAL(absl/strings/internal/resize_uninitialized.h)

#else // !BABYLON_HAS_INCLUDE("absl/strings/internal/resize_uninitialized.h")

#include BABYLON_EXTERNAL(absl/meta/type_traits.h)

#ifndef ABSL_NAMESPACE_BEGIN
#define ABSL_NAMESPACE_BEGIN
#define ABSL_NAMESPACE_END
#endif // ABSL_NAMESPACE_BEGIN

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace strings_internal {

template <typename string_type, typename = void>
struct ResizeUninitializedTraits {
    static void resize(string_type* s, size_t new_size) {
        s->resize(new_size);
    }
};

template <typename string_type>
struct ResizeUninitializedTraits<
        string_type,
        ::absl::void_t<decltype(std::declval<string_type&>()
                                  .__resize_default_init(237))> > {
    static void resize(string_type* s, size_t new_size) {
        s->__resize_default_init(new_size);
    }
};

template <typename string_type, typename = void>
inline void STLStringResizeUninitialized(string_type* s, size_t new_size) {
    ResizeUninitializedTraits<string_type>::resize(s, new_size);
}

} // strings_internal
ABSL_NAMESPACE_END
} // absl

#endif // !BABYLON_HAS_INCLUDE("absl/strings/internal/resize_uninitialized.h")
