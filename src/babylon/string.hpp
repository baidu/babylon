#pragma once

#include "babylon/string.h"

// clang-format off
#include BABYLON_EXTERNAL(absl/strings/internal/resize_uninitialized.h)
// clang-format on

#include <string>
#include <vector>

#ifndef ABSL_NAMESPACE_BEGIN
#define ABSL_NAMESPACE_BEGIN
#define ABSL_NAMESPACE_END
#endif // ABSL_NAMESPACE_BEGIN

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace strings_internal {

#if __GLIBCXX__ && _GLIBCXX_USE_CXX11_ABI
// 增加对GLIBCXX新ABI的支持
template <typename C, typename T, typename A>
inline void STLStringResizeUninitialized(::std::basic_string<C, T, A>* string,
                                         size_t new_size) noexcept {
  using StringType = ::std::basic_string<C, T, A>;
  struct S {
    struct AllocatorAndPointer : public StringType::allocator_type {
      typename StringType::pointer pointer;
    } allocator_and_pointer;
    typename StringType::size_type size;
  };
  if (string->capacity() < new_size) {
    string->reserve(new_size);
  }
  reinterpret_cast<S*>(string)->size = new_size;
  auto data = &(*string)[0];
  data[new_size] = typename StringType::value_type {};
}
#elif __GLIBCXX__ && !_GLIBCXX_USE_CXX11_ABI
// 增加对GLIBCXX老ABI的支持
template <typename C, typename T, typename A>
inline void STLStringResizeUninitialized(::std::basic_string<C, T, A>* string,
                                         size_t new_size) noexcept {
  using StringType = ::std::basic_string<C, T, A>;
  struct R {
    typename StringType::size_type length;
    typename StringType::size_type capacity;
    _Atomic_word refcount;
  };
  // string operator[]会尝试调整string的copy on
  // write状态，有额外开销，采用const_cast规避
  auto data = const_cast<char*>(string->c_str());
  auto rep = reinterpret_cast<R*>(data) - 1;
  // 需要扩展或进行copy on write的情况，进行reserve
  if (new_size > rep->capacity || rep->refcount > 0) {
    string->reserve(new_size);
    // 重新分配了空间，再次获取data和rep
    data = const_cast<char*>(string->c_str());
    rep = reinterpret_cast<R*>(data) - 1;
  }
  // 在new_size == 0时，此处可能写入默认string空间
  // 由于是原值写入，不会有问题
  // 之所以未进行判断，是希望减少一次分支
  rep->length = new_size;
  data[new_size] = typename StringType::value_type {};
}
#endif // __GLIBCXX__ && !_GLIBCXX_USE_CXX11_ABI

} // namespace strings_internal
ABSL_NAMESPACE_END
} // namespace absl

BABYLON_NAMESPACE_BEGIN

template <typename T>
inline typename T::pointer resize_uninitialized(
    T& string, typename T::size_type size) noexcept {
  ::absl::strings_internal::STLStringResizeUninitialized(&string, size);
  return &string[0];
}

// 默认实现就是T::reserve
template <typename T>
inline void stable_reserve(T& string,
                           typename T::size_type min_capacity) noexcept {
  if (min_capacity > string.capacity()) {
    string.reserve(min_capacity);
  }
}

#if __GLIBCXX__ && !_GLIBCXX_USE_CXX11_ABI && GLIBCXX_VERSION >= 720191114L
template <typename C, typename T, typename A>
inline void stable_reserve(
    ::std::basic_string<C, T, A>& string,
    typename ::std::basic_string<C, T, A>::size_type min_capacity) noexcept {
  using StringType = ::std::basic_string<C, T, A>;
  using size_type = typename StringType::size_type;
  using allocator_type = typename StringType::allocator_type;
  using traits_type = typename std::allocator_traits<allocator_type>;
  using value_type = typename StringType::value_type;
  using pointer = typename StringType::pointer;
  using bytes_allocator_type =
      typename traits_type::template rebind_alloc<char>;
  // std::string::_Rep
  struct R {
    size_type length;
    size_type capacity;
    _Atomic_word refcount;
    alignas(size_type) value_type data[0];
  };
  // std::string
  struct S : public allocator_type {
    // 直接设置string的内部成员
    inline S(allocator_type allocator, pointer data) noexcept
        : allocator_type {allocator}, data {data} {}
    pointer data;
  };

  if (string.capacity() >= min_capacity) {
    return;
  }

  size_t allocate_bytes = sizeof(R) + (min_capacity + 1) * sizeof(value_type);
  bytes_allocator_type allocator = string.get_allocator();
  auto new_rep = reinterpret_cast<R*>(allocator.allocate(allocate_bytes));
  new_rep->length = string.size();
  new_rep->capacity = min_capacity;
  new_rep->refcount = 0;
  T::copy(new_rep->data, string.c_str(), string.size() + 1);

  // 通过析构正确处理copy on write，之后直接设置新的内容
  string.~basic_string();
  new (&string) S {allocator, new_rep->data};
}
#endif // __GLIBCXX__ && !_GLIBCXX_USE_CXX11_ABI && GLIBCXX_VERSION >=
       // 720191114L

#if __GLIBCXX__
template <
    typename T, typename A,
    typename = typename ::std::enable_if<::std::is_trivial<T>::value>::type>
inline typename ::std::vector<T, A>::pointer resize_uninitialized(
    ::std::vector<T, A>& vector,
    typename ::std::vector<T, A>::size_type size) noexcept {
  using VectorType = ::std::vector<T, A>;
  struct VectorImplType : public VectorType::allocator_type {
    typename VectorType::pointer _M_start;
    typename VectorType::pointer _M_finish;
    typename VectorType::pointer _M_end_of_storage;
  };
  static_assert(sizeof(VectorType) == sizeof(VectorImplType),
                "GLIBCXX seems change its ABI");

  vector.reserve(size);
  auto& impl = reinterpret_cast<VectorImplType&>(vector);
  impl._M_finish = impl._M_start + size;
  return impl._M_start;
}
template <typename A>
inline void resize_uninitialized(
    ::std::vector<bool, A>& vector,
    typename ::std::vector<bool, A>::size_type size) noexcept {
  using VectorType = ::std::vector<bool, A>;
  struct VectorImplType : public VectorType::allocator_type {
    struct {
      unsigned long* _M_p;
      unsigned int _M_offset;
    } _M_start;
    struct {
      unsigned long* _M_p;
      unsigned int _M_offset;
    } _M_finish;
    void* _M_end_of_storage;
  };
  static_assert(sizeof(VectorType) == sizeof(VectorImplType),
                "GLIBCXX seems change its ABI");

  vector.reserve(size);
  auto& impl = reinterpret_cast<VectorImplType&>(vector);
  impl._M_finish._M_p = impl._M_start._M_p + size / sizeof(unsigned long) / 8;
  impl._M_finish._M_offset = size % (sizeof(unsigned long) * 8);
}
#endif

#if __GLIBCXX__ && _GLIBCXX_USE_CXX11_ABI
inline ::std::tuple<char*, size_t> exchange_string_buffer(
    ::std::string& string, char* buffer, size_t buffer_size) noexcept {
  using StringType = ::std::string;
  struct S {
    struct AllocatorAndPointer : public StringType::allocator_type {
      typename StringType::pointer pointer;
    } allocator_and_pointer;
    typename StringType::size_type size;
    union {
      typename StringType::value_type local_buf[0];
      typename StringType::size_type capacity;
    };
  };
  typename StringType::pointer old_buffer =
      reinterpret_cast<S&>(string).allocator_and_pointer.pointer ==
              reinterpret_cast<S&>(string).local_buf
          ? nullptr
          : reinterpret_cast<S&>(string).allocator_and_pointer.pointer;
  typename StringType::size_type old_capacity =
      reinterpret_cast<S&>(string).capacity;
  reinterpret_cast<S&>(string).allocator_and_pointer.pointer = buffer;
  reinterpret_cast<S&>(string).size = buffer_size - 1;
  reinterpret_cast<S&>(string).capacity = buffer_size - 1;
  reinterpret_cast<S&>(string).allocator_and_pointer.pointer[buffer_size - 1] =
      typename StringType::value_type {};
  return {old_buffer, old_capacity + 1};
}
#endif

BABYLON_NAMESPACE_END
