#pragma once

#include "babylon/string_view.h"

// clang-format off
#include BABYLON_EXTERNAL(absl/base/attributes.h) // ABSL_ATTRIBUTE_ALWAYS_INLINE
// clang-format on

#include <functional> // std::hash, std::equal_to
#include <stdexcept>  // std::out_of_range

BABYLON_NAMESPACE_BEGIN

// std::initializer_list不支持带参数构造，这里通过定义相同结构类型
// 并结合类型转换来提供构造能力
template <typename T>
class InitializerList {
 public:
  inline constexpr InitializerList(const T* data, size_t size) noexcept
      : _data(data), _size(size) {}
  inline constexpr operator const ::std::initializer_list<T>&() const noexcept {
    return reinterpret_cast<const ::std::initializer_list<T>&>(*this);
  }
  inline ~InitializerList() noexcept = default;

 private:
  // 这里类型和顺序需要和std::initializer_list保持一致
  const T* _data;
  size_t _size;
};

#if __cplusplus < 201703L
////////////////////////////////////////////////////////////////////////////////
// BasicStringView begin
template <typename C, typename T>
inline constexpr BasicStringView<C, T>::BasicStringView(const_pointer buffer,
                                                        size_type size) noexcept
    :
#if __GLIBCXX__
      _size(size),
      _data(buffer) {
}
#else  // !__GLIBCXX__
      _data(buffer),
      _size(size) {
}
#endif // !__GLIBCXX__

template <typename C, typename T>
inline constexpr BasicStringView<C, T>::BasicStringView(
    const_pointer c_str) noexcept
    : BasicStringView(c_str,
                      c_str != nullptr ? traits_type::length(c_str) : 0) {}

template <>
inline constexpr BasicStringView<char, ::std::char_traits<char>>::
    BasicStringView(const_pointer c_str) noexcept
    : BasicStringView(c_str, c_str != nullptr ? __builtin_strlen(c_str) : 0) {}

template <typename C, typename T>
template <typename A>
inline BasicStringView<C, T>::BasicStringView(
    const ::std::basic_string<C, T, A>& other) noexcept
    :
#if __GLIBCXX__
      _size(other.size()),
      _data(other.data()) {
}
#else  // !__GLIBCXX__
      _data(other.data()),
      _size(other.size()) {
}
#endif // !__GLIBCXX__

template <typename C, typename T>
inline constexpr typename BasicStringView<C, T>::const_iterator
BasicStringView<C, T>::begin() const noexcept {
  return cbegin();
}

template <typename C, typename T>
inline constexpr typename BasicStringView<C, T>::const_iterator
BasicStringView<C, T>::cbegin() const noexcept {
  return _data;
}

template <typename C, typename T>
inline constexpr typename BasicStringView<C, T>::const_iterator
BasicStringView<C, T>::end() const noexcept {
  return cend();
}

template <typename C, typename T>
inline constexpr typename BasicStringView<C, T>::const_iterator
BasicStringView<C, T>::cend() const noexcept {
  return _data + _size;
}

template <typename C, typename T>
inline constexpr typename BasicStringView<C, T>::reverse_iterator
BasicStringView<C, T>::rbegin() const noexcept {
  return crbegin();
}

template <typename C, typename T>
inline constexpr typename BasicStringView<C, T>::reverse_iterator
BasicStringView<C, T>::crbegin() const noexcept {
  return reverse_iterator(cend());
}

template <typename C, typename T>
inline constexpr typename BasicStringView<C, T>::reverse_iterator
BasicStringView<C, T>::rend() const noexcept {
  return crend();
}

template <typename C, typename T>
inline constexpr typename BasicStringView<C, T>::reverse_iterator
BasicStringView<C, T>::crend() const noexcept {
  return reverse_iterator(cbegin());
}

template <typename C, typename T>
inline constexpr typename BasicStringView<C, T>::const_reference
BasicStringView<C, T>::operator[](size_type position) const noexcept {
  return _data[position];
}

template <typename C, typename T>
inline typename BasicStringView<C, T>::const_reference
BasicStringView<C, T>::at(size_type position) const {
  if (position < size()) {
    return _data[position];
  }
  throw ::std::out_of_range("out of range");
}

template <typename C, typename T>
inline constexpr typename BasicStringView<C, T>::const_reference
BasicStringView<C, T>::front() const noexcept {
  return _data[0];
}

template <typename C, typename T>
inline constexpr typename BasicStringView<C, T>::const_reference
BasicStringView<C, T>::back() const noexcept {
  return _data[_size - 1];
}

template <typename C, typename T>
inline constexpr typename BasicStringView<C, T>::const_pointer
BasicStringView<C, T>::data() const noexcept {
  return _data;
}

template <typename C, typename T>
inline constexpr typename BasicStringView<C, T>::size_type
BasicStringView<C, T>::size() const noexcept {
  return _size;
}

template <typename C, typename T>
inline constexpr typename BasicStringView<C, T>::size_type
BasicStringView<C, T>::length() const noexcept {
  return size();
}

template <typename C, typename T>
inline constexpr typename BasicStringView<C, T>::size_type
BasicStringView<C, T>::max_size() const noexcept {
  return (npos - sizeof(size_type) - sizeof(void*)) / sizeof(value_type) / 4;
}

template <typename C, typename T>
inline constexpr bool BasicStringView<C, T>::empty() const noexcept {
  return size() == 0;
}

template <typename C, typename T>
inline CONSTEXPR_SINCE_CXX14 void BasicStringView<C, T>::remove_prefix(
    size_type num) noexcept {
  _data += num;
  _size -= num;
}

template <typename C, typename T>
inline CONSTEXPR_SINCE_CXX14 void BasicStringView<C, T>::remove_suffix(
    size_type num) noexcept {
  _size -= num;
}

template <typename C, typename T>
inline CONSTEXPR_SINCE_CXX14 void BasicStringView<C, T>::swap(
    BasicStringView& other) noexcept {
  auto old_data = _data;
  auto old_size = _size;
  _data = other._data;
  _size = other._size;
  other._data = old_data;
  other._size = old_size;
}

template <typename C, typename T>
inline typename BasicStringView<C, T>::size_type BasicStringView<C, T>::copy(
    pointer dest, size_type count, size_type position) const {
  if (position <= size()) {
    const auto copy_count = ::std::min(count, _size - position);
    traits_type::copy(dest, _data + position, copy_count);
    return copy_count;
  }
  throw std::out_of_range("out of range");
}

template <typename C, typename T>
inline constexpr BasicStringView<C, T> BasicStringView<C, T>::substr(
    size_type position, size_type count) const noexcept {
  return BasicStringView(_data + position,
                         count < _size - position ? count : _size - position);
}

template <typename C, typename T>
inline BABYLON_CONSTEXPR int32_t
BasicStringView<C, T>::compare(const BasicStringView& other) const noexcept {
  return _size == other._size ? traits_type::compare(_data, other._data, _size)
         : _size > other._size
             ? (traits_type::compare(_data, other._data, other._size) < 0 ? -1
                                                                          : 1)
         : traits_type::compare(_data, other._data, _size) > 0 ? 1
                                                               : -1;
}

template <>
inline BABYLON_CONSTEXPR int32_t
BasicStringView<char, ::std::char_traits<char>>::compare(
    const BasicStringView<char, ::std::char_traits<char>>& other)
    const noexcept {
  return _size == other._size ? __builtin_memcmp(_data, other._data, _size)
         : _size > other._size
             ? (__builtin_memcmp(_data, other._data, other._size) < 0 ? -1 : 1)
         : __builtin_memcmp(_data, other._data, _size) > 0 ? 1
                                                           : -1;
}

template <typename C, typename T>
inline BABYLON_CONSTEXPR int32_t
BasicStringView<C, T>::compare(size_type position, size_type count,
                               const BasicStringView& other) const noexcept {
  return substr(position, count).compare(other);
}

template <typename C, typename T>
inline BABYLON_CONSTEXPR int32_t BasicStringView<C, T>::compare(
    size_type position, size_type count, const BasicStringView& other,
    size_type other_position, size_type other_count) const noexcept {
  return substr(position, count)
      .compare(other.substr(other_position, other_count));
}

template <typename C, typename T>
inline BABYLON_CONSTEXPR int32_t BasicStringView<C, T>::compare(
    size_type position, size_type count, const_pointer c_str) const noexcept {
  return substr(position, count).compare(c_str);
}

template <typename C, typename T>
inline BABYLON_CONSTEXPR int32_t BasicStringView<C, T>::compare(
    size_type position, size_type count, const_pointer buffer,
    size_type size) const noexcept {
  return substr(position, count).compare(BasicStringView(buffer, size));
}

template <typename C, typename T>
inline constexpr BasicStringView<C, T>::operator ::std::initializer_list<
    value_type>() const noexcept {
  return ::babylon::InitializerList<value_type>(_data, _size);
}

#if !__GLIBCXX__
template <typename C, typename T>
inline constexpr BasicStringView<C, T>::operator ::std::string_view()
    const noexcept {
  return ::std::string_view(_data, _size);
}

template <typename C, typename T>
inline constexpr BasicStringView<C, T>::BasicStringView(
    ::std::string_view sv) noexcept
    : BasicStringView(sv.data(), sv.size()) {}
#endif // !__GLIBCXX__

template <typename C, typename T>
constexpr
    typename BasicStringView<C, T>::value_type BasicStringView<C, T>::EMPTY;
// BasicStringView end
////////////////////////////////////////////////////////////////////////////////

template <typename C, typename T>
inline constexpr bool operator==(BasicStringView<C, T> left,
                                 BasicStringView<C, T> right) noexcept {
  return left.size() == right.size() &&
         0 == __builtin_memcmp(left.data(), right.data(),
                               sizeof(C) * left.size());
}

template <typename C, typename T>
inline constexpr bool operator==(
    typename ::std::common_type<BasicStringView<C, T>>::type left,
    BasicStringView<C, T> right) noexcept {
  return left == right;
}

template <typename C, typename T>
inline constexpr bool operator==(
    BasicStringView<C, T> left,
    typename ::std::common_type<BasicStringView<C, T>>::type right) noexcept {
  return left == right;
}

template <typename C, typename T>
inline constexpr bool operator!=(BasicStringView<C, T> left,
                                 BasicStringView<C, T> right) noexcept {
  return !(left == right);
}

template <typename C, typename T>
inline constexpr bool operator!=(
    typename ::std::common_type<BasicStringView<C, T>>::type left,
    BasicStringView<C, T> right) noexcept {
  return !(left == right);
}

template <typename C, typename T>
inline constexpr bool operator!=(
    BasicStringView<C, T> left,
    typename ::std::common_type<BasicStringView<C, T>>::type right) noexcept {
  return !(left == right);
}

template <typename C, typename T>
inline BABYLON_CONSTEXPR bool operator<(
    typename ::std::common_type<BasicStringView<C, T>>::type left,
    BasicStringView<C, T> right) noexcept {
  return left.compare(right) < 0;
}

template <typename C, typename T>
inline BABYLON_CONSTEXPR bool operator<(
    BasicStringView<C, T> left,
    typename ::std::common_type<BasicStringView<C, T>>::type right) noexcept {
  return left.compare(right) < 0;
}

template <typename C, typename T>
inline BABYLON_CONSTEXPR bool operator<(BasicStringView<C, T> left,
                                        BasicStringView<C, T> right) noexcept {
  return left.compare(right) < 0;
}

template <typename C, typename T>
inline BABYLON_CONSTEXPR bool operator<=(
    typename ::std::common_type<BasicStringView<C, T>>::type left,
    BasicStringView<C, T> right) noexcept {
  return left.compare(right) <= 0;
}

template <typename C, typename T>
inline BABYLON_CONSTEXPR bool operator<=(
    BasicStringView<C, T> left,
    typename ::std::common_type<BasicStringView<C, T>>::type right) noexcept {
  return left.compare(right) <= 0;
}

template <typename C, typename T>
inline BABYLON_CONSTEXPR bool operator<=(BasicStringView<C, T> left,
                                         BasicStringView<C, T> right) noexcept {
  return left.compare(right) <= 0;
}

template <typename C, typename T>
inline BABYLON_CONSTEXPR bool operator>(
    typename ::std::common_type<BasicStringView<C, T>>::type left,
    BasicStringView<C, T> right) noexcept {
  return left.compare(right) > 0;
}

template <typename C, typename T>
inline BABYLON_CONSTEXPR bool operator>(
    BasicStringView<C, T> left,
    typename ::std::common_type<BasicStringView<C, T>>::type right) noexcept {
  return left.compare(right) > 0;
}

template <typename C, typename T>
inline BABYLON_CONSTEXPR bool operator>(BasicStringView<C, T> left,
                                        BasicStringView<C, T> right) noexcept {
  return left.compare(right) > 0;
}

template <typename C, typename T>
inline BABYLON_CONSTEXPR bool operator>=(
    typename ::std::common_type<BasicStringView<C, T>>::type left,
    BasicStringView<C, T> right) noexcept {
  return left.compare(right) >= 0;
}

template <typename C, typename T>
inline BABYLON_CONSTEXPR bool operator>=(
    BasicStringView<C, T> left,
    typename ::std::common_type<BasicStringView<C, T>>::type right) noexcept {
  return left.compare(right) >= 0;
}

template <typename C, typename T>
inline BABYLON_CONSTEXPR bool operator>=(BasicStringView<C, T> left,
                                         BasicStringView<C, T> right) noexcept {
  return left.compare(right) >= 0;
}

template <typename C, typename T>
inline ::std::basic_ostream<C, T>& operator<<(
    ::std::basic_ostream<C, T>& os, BasicStringView<C, T> view) noexcept {
  os.write(view.data(), static_cast<ssize_t>(view.size()));
  return os;
}

#endif // __cplusplus < 201703L

BABYLON_NAMESPACE_END

// STL已经定义::std::string_view时对应的hash和equal_to也已经定义
// 采用偏特化萃取来支持absl的异构key判定
#if __GLIBCXX__ && __cplusplus < 201703L

namespace std {
template <>
struct hash<string_view> {
  // absl和std c++20使用is_transparent定义来判定是否支持异构键
  using is_transparent = void;
  inline __attribute__((always_inline)) size_t operator()(
      string_view sv) const noexcept {
    return ::std::_Hash_impl::hash(sv.data(), sv.size());
  }
};

// GLIBCXX利用__is_fast_hash来判断是否需要缓存hash_code，标注非快速hash，需要缓存
template <>
struct __is_fast_hash<hash<string_view>> : ::std::false_type {};

template <>
struct equal_to<string_view> {
  // absl和std c++20使用is_transparent定义来判定是否支持异构键
  using is_transparent = void;
  inline __attribute__((always_inline)) bool operator()(
      string_view left, string_view right) const noexcept {
    return left == right;
  }
};
} // namespace std

#endif // __GLIBCXX__ && __cplusplus < 201703L

namespace std {
template <>
struct hash<::babylon::StringView> {
  // absl和std c++20使用is_transparent定义来判定是否支持异构键
  using is_transparent = void;
  inline ABSL_ATTRIBUTE_ALWAYS_INLINE size_t
  operator()(::babylon::StringView sv) const noexcept {
#if __GLIBCXX__
    return ::std::_Hash_impl::hash(sv.data(), sv.size());
#else
    return ::std::__do_string_hash(sv.data(), sv.data() + sv.size());
#endif
  }
};

// GLIBCXX利用__is_fast_hash来判断是否需要缓存hash_code，标注非快速hash，需要缓存
#if __GLIBCXX__
template <>
struct __is_fast_hash<hash<::babylon::StringView>> : ::std::false_type {};
#endif

template <>
struct equal_to<::babylon::StringView> {
  // absl和std c++20使用is_transparent定义来判定是否支持异构键
  using is_transparent = void;
  inline ABSL_ATTRIBUTE_ALWAYS_INLINE bool operator()(
      ::babylon::StringView left, ::babylon::StringView right) const noexcept {
    return left == right;
  }
};
} // namespace std
