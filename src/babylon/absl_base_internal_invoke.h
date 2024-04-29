#pragma once

#include "babylon/environment.h"

// clang-format off
#include BABYLON_EXTERNAL(absl/base/config.h)
#include BABYLON_EXTERNAL(absl/base/internal/invoke.h)
#include BABYLON_EXTERNAL(absl/meta/type_traits.h)
// clang-format on

#include <functional>

// 2023年之前没有定义absl::is_invocable_r，需要补充一下
#if ABSL_LTS_RELEASE_VERSION < 20220623L

#ifndef ABSL_NAMESPACE_BEGIN
#define ABSL_NAMESPACE_BEGIN
#define ABSL_NAMESPACE_END
#endif // ABSL_NAMESPACE_BEGIN

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace base_internal {

// 2021年之前没有定义invoke_result_t标准名
#if !BABYLON_HAS_INCLUDE("absl/flags/commandlineflag.h")
template <typename F, typename... Args>
using invoke_result_t = InvokeT<F, Args...>;
#endif // !BABYLON_HAS_INCLUDE("absl/numeric/bits.h")

////////////////////////////////////////////////////////////////////////////////
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
////////////////////////////////////////////////////////////////////////////////

} // namespace base_internal
ABSL_NAMESPACE_END
} // namespace absl

#endif // ABSL_LTS_RELEASE_VERSION < 20220623L
