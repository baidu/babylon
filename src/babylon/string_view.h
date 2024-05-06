#pragma once

#include "babylon/environment.h"

#if __cplusplus >= 201703L
#include <string_view> // std::string_view
#else                  // __cplusplus < 201703L
#include <cstddef>     // ptrdiff_t
#include <limits>      // std::numeric_limits
#include <string>      // std::string
#endif                 // __cplusplus < 201703L

BABYLON_NAMESPACE_BEGIN

#ifndef __CUDACC__
#define BABYLON_CONSTEXPR constexpr
#else
#define BABYLON_CONSTEXPR
#endif

#if __cplusplus >= 201703L
// c++17之后引用std::basic_string_view实现
// 采用继承而非using主要为了多编译单元下符号稳定
template <typename C, typename T = ::std::char_traits<C>>
class BasicStringView : public ::std::basic_string_view<C, T> {
 public:
  using Base = typename BasicStringView::basic_string_view;
  using Base::Base;

  inline constexpr BasicStringView() noexcept = default;
  inline constexpr BasicStringView(Base other) noexcept : Base(other) {}

  template <typename A>
  inline BasicStringView(const ::std::basic_string<C, T, A>& other) noexcept
      : Base(other.data(), other.size()) {}
};
#else // __cplusplus < 201703L
// c++17之前需要提供::std::basic_string_view的替换实现
template <typename C, typename T = ::std::char_traits<C>>
class BasicStringView {
 public:
  typedef T traits_type;
  typedef C value_type;
  typedef C* pointer;
  typedef const C* const_pointer;
  typedef C& reference;
  typedef const C& const_reference;
  typedef const C* const_iterator;
  typedef const_iterator iterator;
  typedef ::std::reverse_iterator<const_iterator> const_reverse_iterator;
  typedef const_reverse_iterator reverse_iterator;
  typedef size_t size_type;
  typedef ptrdiff_t difference_type;

  inline constexpr BasicStringView() noexcept = default;
  inline constexpr BasicStringView(BasicStringView&& other) noexcept = default;
  inline constexpr BasicStringView(const BasicStringView& other) noexcept =
      default;
  inline CONSTEXPR_SINCE_CXX14 BasicStringView& operator=(
      BasicStringView&& other) noexcept = default;
  inline CONSTEXPR_SINCE_CXX14 BasicStringView& operator=(
      const BasicStringView& other) noexcept = default;

  inline constexpr BasicStringView(const_pointer c_str) noexcept;
  inline constexpr BasicStringView(const_pointer buffer,
                                   size_type size) noexcept;
  template <typename A>
  inline BasicStringView(const ::std::basic_string<C, T, A>& other) noexcept;

  inline constexpr const_iterator begin() const noexcept;
  inline constexpr const_iterator cbegin() const noexcept;
  inline constexpr const_iterator end() const noexcept;
  inline constexpr const_iterator cend() const noexcept;
  inline constexpr reverse_iterator rbegin() const noexcept;
  inline constexpr reverse_iterator crbegin() const noexcept;
  inline constexpr reverse_iterator rend() const noexcept;
  inline constexpr reverse_iterator crend() const noexcept;

  inline constexpr const_reference operator[](
      size_type position) const noexcept;
  inline const_reference at(size_type position) const;
  inline constexpr const_reference front() const noexcept;
  inline constexpr const_reference back() const noexcept;
  inline constexpr const_pointer data() const noexcept;

  inline constexpr size_type size() const noexcept;
  inline constexpr size_type length() const noexcept;
  inline constexpr size_type max_size() const noexcept;
  inline constexpr bool empty() const noexcept;

  inline CONSTEXPR_SINCE_CXX14 void remove_prefix(size_type num) noexcept;
  inline CONSTEXPR_SINCE_CXX14 void remove_suffix(size_type num) noexcept;
  inline CONSTEXPR_SINCE_CXX14 void swap(BasicStringView& other) noexcept;

  inline size_type copy(pointer dest, size_type count,
                        size_type position = 0) const;
  inline constexpr BasicStringView substr(
      size_type position = 0, size_type count = npos) const noexcept;

  inline BABYLON_CONSTEXPR int32_t
  compare(const BasicStringView& other) const noexcept;
  inline BABYLON_CONSTEXPR int32_t
  compare(size_type position, size_type count,
          const BasicStringView& other) const noexcept;
  inline BABYLON_CONSTEXPR int32_t
  compare(size_type position, size_type count, const BasicStringView& other,
          size_type other_position, size_type other_count) const noexcept;
  inline BABYLON_CONSTEXPR int32_t compare(size_type position, size_type count,
                                           const_pointer c_str) const noexcept;
  inline BABYLON_CONSTEXPR int32_t compare(size_type position, size_type count,
                                           const_pointer buffer,
                                           size_type size) const noexcept;

  // 即使增补了std::string_view，在c++17之前，std::string也并不能直接和其交互
  // 需要提供额外的转换函数，选择提供std::initializer_list而非直接std::string
  // 主要考虑对于std::string::assign/append等函数中对应版本不会产生临时对象
  // 不过有个缺陷是因为无法直接转换为std::string，因此对于copy
  // initialization的场景 需要调整使用方法 void function(const ::std::string&)
  // std::string s = sv;    ->    std::string s(sv);  // direct initialization
  // function(sv);          ->    function({sv});     // copy list
  // initialization
  inline constexpr operator ::std::initializer_list<value_type>()
      const noexcept;

#if !__GLIBCXX__
  // LIBCPP环境下std::string_view和babylon::BasicStringView是无关类型
  // 这里主动增加转换功能，保持可以无缝混用
  // GLIBCXX环境下为子类关系，无需增加这个转换支持
  inline constexpr operator ::std::string_view() const noexcept;
  inline constexpr BasicStringView(::std::string_view sv) noexcept;
#endif // !__GLIBCXX__

  static constexpr size_type npos {::std::numeric_limits<size_type>::max()};

 private:
  static constexpr value_type EMPTY {'\0'};

  // 成员构成和c++17的string_view保持一致，保证多个不同-std设置的编译单元可以兼容
  // GLIBCXX和LIBCPP的ABI不一致，这里适配两种场景
#if __GLIBCXX__
  size_type _size {0};
  const_pointer _data {&EMPTY};
#else  // !__GLIBCXX__
  const_pointer _data {&EMPTY};
  size_type _size {0};
#endif // !__GLIBCXX__
};
#endif // __cplusplus < 201703L
using StringView = BasicStringView<char>;
using WStringView = BasicStringView<wchar_t>;

BABYLON_NAMESPACE_END

// GLIBCXX中对c++11和c++14未定义string_view，补充定义
// LIBCPP中已经定义了string_view，无法增补
#if __GLIBCXX__ && __cplusplus < 201703L
namespace std {
template <typename C, typename T = std::char_traits<C>>
class basic_string_view : public ::babylon::BasicStringView<C, T> {
 public:
  using Base = typename basic_string_view::BasicStringView;
  using Base::Base;

  inline constexpr basic_string_view() noexcept = default;
  inline constexpr basic_string_view(Base other) noexcept : Base(other) {}
};
using string_view = basic_string_view<char>;
using wstring_view = basic_string_view<wchar_t>;
} // namespace std
#endif // __GLIBCXX__ && __cplusplus < 201703L

#include "babylon/string_view.hpp"
