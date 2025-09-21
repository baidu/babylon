#pragma once

#include "babylon/environment.h"

// clang-format off
#include BABYLON_EXTERNAL(absl/meta/type_traits.h)
// clang-format on

// add some essential features for old LTS abseil-cpp
#ifdef ABSL_LTS_RELEASE_VERSION

#if ABSL_LTS_RELEASE_VERSION < 20220623L
#define BABYLON_TMP_NEED_INVOKE_RESULT_R 1
#endif // ABSL_LTS_RELEASE_VERSION < 20220623L

#if ABSL_LTS_RELEASE_VERSION < 20200923L
#define BABYLON_TMP_NEED_INVOKE_RESULT_T 1
#endif // ABSL_LTS_RELEASE_VERSION < 20200923L

#else // !ABSL_LTS_RELEASE_VERSION

// TODO(lijiang01): some inactive repo depend on old abseil-cpp head
//                  use macro check trick to adapt them before they switch to
//                  some LTS

// clang-format off
#include BABYLON_EXTERNAL(absl/base/optimization.h)
// clang-format on

#ifndef ABSL_ASSUME
#define BABYLON_TMP_NEED_INVOKE_RESULT_R 1
#endif // ABSL_ASSUME

#ifndef ABSL_HAVE_FEATURE
#define BABYLON_TMP_NEED_INVOKE_RESULT_T 1
#endif // ABSL_HAVE_FEATURE

#endif // !ABSL_LTS_RELEASE_VERSION

#if BABYLON_TMP_NEED_INVOKE_RESULT_R || BABYLON_TMP_NEED_INVOKE_RESULT_T

// very old abseil-cpp dont have these macros
#ifndef ABSL_NAMESPACE_BEGIN
#define ABSL_NAMESPACE_BEGIN
#define ABSL_NAMESPACE_END
#endif // ABSL_NAMESPACE_BEGIN
#endif // BABYLON_TMP_NEED_INVOKE_RESULT_R || BABYLON_TMP_NEED_INVOKE_RESULT_T

#if BABYLON_TMP_NEED_INVOKE_RESULT_T
namespace absl {
ABSL_NAMESPACE_BEGIN
namespace base_internal {
template <typename F, typename... Args>
using invoke_result_t = InvokeT<F, Args...>;
} // namespace base_internal
ABSL_NAMESPACE_END
} // namespace absl
#endif // BABYLON_TMP_NEED_INVOKE_RESULT_T

#if BABYLON_TMP_NEED_INVOKE_RESULT_R
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
#endif // BABYLON_TMP_NEED_INVOKE_RESULT_R

#undef BABYLON_TMP_NEED_INVOKE_RESULT_R
#undef BABYLON_TMP_NEED_INVOKE_RESULT_T
