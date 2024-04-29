#include "babylon/new.h"

#include <stdlib.h> // ::aligned_alloc

////////////////////////////////////////////////////////////////////////////////
// TODO(lijiang01): 待旧编译器淘汰后可以去掉此兼容代码
//
// 在编译器支持c++17之前，-faligned-new相关符号也并不存在
// 这里通过对低版本补充对应类型和函数声明来方便兼容代码的编写
#if _LIBCPP_VERSION < 8000L && GLIBCXX_VERSION < 700000000L
ABSL_ATTRIBUTE_WEAK
void* operator new(size_t count, ::std::align_val_t alignment) {
  size_t alignment_size = static_cast<size_t>(alignment);
  size_t size = (count + alignment_size - 1) & -alignment_size;
  return ::aligned_alloc(alignment_size, size);
}

ABSL_ATTRIBUTE_WEAK
void operator delete(void* ptr) noexcept {
  ::free(ptr);
}

ABSL_ATTRIBUTE_WEAK
void operator delete(void* ptr, ::std::align_val_t) noexcept {
  ::free(ptr);
}

ABSL_ATTRIBUTE_WEAK
void operator delete(void* ptr, size_t, ::std::align_val_t) noexcept {
  ::free(ptr);
}

ABSL_ATTRIBUTE_WEAK
void operator delete(void* ptr, size_t) noexcept {
  ::free(ptr);
}
#endif // _LIBCPP_VERSION < 10000L && GLIBCXX_VERSION < 700000000L
////////////////////////////////////////////////////////////////////////////////
