#pragma once

#include "babylon/environment.h"

#if BABYLON_HAS_INCLUDE("absl/numeric/bits.h")

#include BABYLON_EXTERNAL(absl/numeric/bits.h)

#else // !BABYLON_HAS_INCLUDE("absl/numeric/bits.h")

// TODO(lijiang01): 早期abseil-cpp未提供bits.h头文件和相关函数功能
//                  等兼容期过后删除这些拷贝来的实现
#include BABYLON_EXTERNAL(absl/base/optimization.h)

#ifndef ABSL_NAMESPACE_BEGIN
#define ABSL_NAMESPACE_BEGIN
#define ABSL_NAMESPACE_END
#endif // ABSL_NAMESPACE_BEGIN

namespace absl {
ABSL_NAMESPACE_BEGIN

inline constexpr bool has_single_bit(size_t x) noexcept {
    return x != 0 && (x & (x - 1)) == 0;
}

inline CONSTEXPR_SINCE_CXX14 size_t bit_ceil(size_t n) noexcept {
    if (ABSL_PREDICT_FALSE(n < 2)) {
        return 1;
    }
    auto leading_zero = __builtin_clzll(n - 1);
    auto tailing_zero = 64 - leading_zero;
    return static_cast<size_t>(1) << tailing_zero;
}

ABSL_NAMESPACE_END
} // absl

#endif // !BABYLON_HAS_INCLUDE("absl/numeric/bits.h")
