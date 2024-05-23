#pragma once

#include "babylon/environment.h"

// clang-format off
#include BABYLON_EXTERNAL(absl/base/config.h)
#include BABYLON_EXTERNAL(absl/base/internal/invoke.h)
#include BABYLON_EXTERNAL(absl/meta/type_traits.h)
// clang-format on

// add some essential features for old LTS abseil-cpp
// TODO(lijiang01): some inactive repo depend on old abseil-cpp head
//                  use header check trick to adapt them before they switch to
//                  some LTS
// clang-format off
#if defined(ABSL_LTS_RELEASE_VERSION) || \
    !BABYLON_HAS_INCLUDE(                \
        BABYLON_EXTERNAL(absl/base/internal/dynamic_annotations.h))
// clang-format on

// very old abseil-cpp dont have these macros
#ifndef ABSL_NAMESPACE_BEGIN
#define ABSL_NAMESPACE_BEGIN
#define ABSL_NAMESPACE_END
#endif // ABSL_NAMESPACE_BEGIN

// add invoke_result_t before 20200923
#if ABSL_LTS_RELEASE_VERSION < 20200923L
namespace absl {
ABSL_NAMESPACE_BEGIN
namespace base_internal {
template <typename F, typename... Args>
using invoke_result_t = InvokeT<F, Args...>;
} // namespace base_internal
ABSL_NAMESPACE_END
} // namespace absl
#endif // ABSL_LTS_RELEASE_VERSION < 20200923L

// add invoke_result_r before 20200923
#if ABSL_LTS_RELEASE_VERSION < 20220623L
namespace absl {
ABSL_NAMESPACE_BEGIN
namespace base_internal {

template <typename AlwaysVoid, typename, typename, typename...>
struct IsInvocableRImpl : std::false_type {};

template <typename R, typename F, typename... Args>
struct IsInvocableRImpl<
    absl::void_t<absl::base_internal::invoke_result_t<F, Args...> >, R, F,
    Args...>
    : std::integral_constant<
          bool,
          std::is_convertible<absl::base_internal::invoke_result_t<F, Args...>,
                              R>::value ||
              std::is_void<R>::value> {};

// Type trait whose member `value` is true if invoking `F` with `Args` is valid,
// and either the return type is convertible to `R`, or `R` is void.
// C++11-compatible version of `std::is_invocable_r`.
template <typename R, typename F, typename... Args>
using is_invocable_r = IsInvocableRImpl<void, R, F, Args...>;

} // namespace base_internal
ABSL_NAMESPACE_END
} // namespace absl
#endif // ABSL_LTS_RELEASE_VERSION < 20220623L

#endif // defined(ABSL_LTS_RELEASE_VERSION) ||
       // !BABYLON_HAS_INCLUDE(BABYLON_EXTERNAL(absl/base/internal/dynamic_annotations.h))
