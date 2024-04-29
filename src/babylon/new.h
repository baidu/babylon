#pragma once

#include "babylon/environment.h"

// clang-format off
#include BABYLON_EXTERNAL(absl/base/attributes.h) // ABSL_ATTRIBUTE_ALWAYS_INLINE
// clang-format on

#include <new>     // ::operator new/delete
#include <utility> // std::forward

////////////////////////////////////////////////////////////////////////////////
// TODO(lijiang01): 待c++14淘汰后可以去掉此兼容代码
//
// 在c++17之前，-faligned-new功能并未默认打开
// 但是对应符号在libstdc++和libc++中其实都是可用的
// 这里通过补充对应类型和函数声明来方便兼容代码的编写
//
// 在新版libc++中，std::align_val_t是统一提供，特判避免重复定义
#if _LIBCPP_VERSION < 8000L && __cpp_aligned_new < 201606L
namespace std {
enum class align_val_t : size_t {};
}
#endif // __cpp_aligned_new < 201606L
void* operator new(size_t size, ::std::align_val_t alignment);
void operator delete(void* ptr, ::std::align_val_t alignment) noexcept;
void operator delete(void* ptr, size_t size) noexcept;
void operator delete(void* ptr, size_t size,
                     ::std::align_val_t alignment) noexcept;
////////////////////////////////////////////////////////////////////////////////

BABYLON_NAMESPACE_BEGIN

constexpr ::std::align_val_t CACHELINE_ALIGNMENT =
    ::std::align_val_t(BABYLON_CACHELINE_SIZE);

// 在使用内存对齐分配函数时，经常需要用到对齐取整功能，这里封装标准实现
// 返回值value满足
// 1、value >= unaligned_size
// 2、value % alignment == 0
// 3、符合1&2的条件下，value取满足条件的最小值
inline constexpr ABSL_ATTRIBUTE_ALWAYS_INLINE size_t
align_up(size_t unaligned_size, ::std::align_val_t alignment) noexcept {
  return (unaligned_size + static_cast<size_t>(alignment) - 1) &
         -static_cast<size_t>(alignment);
}

template <typename T, ::std::align_val_t A>
class alignas(static_cast<size_t>(A)) Aligned {
 public:
  template <typename... Args>
  inline ABSL_ATTRIBUTE_ALWAYS_INLINE Aligned(Args&&... args) noexcept
      : _object(::std::forward<Args>(args)...) {}

  inline ABSL_ATTRIBUTE_ALWAYS_INLINE operator T&() noexcept {
    return _object;
  }

  inline ABSL_ATTRIBUTE_ALWAYS_INLINE operator const T&() const noexcept {
    return _object;
  }

 private:
  T _object;
};

template <typename T>
using CachelineAligned = Aligned<T, CACHELINE_ALIGNMENT>;

BABYLON_NAMESPACE_END
