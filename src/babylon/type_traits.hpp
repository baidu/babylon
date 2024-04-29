#pragma once

#include "babylon/type_traits.h"

// clang-format off
#include "babylon/protect.h"
// clang-format on

BABYLON_NAMESPACE_BEGIN

///////////////////////////////////////////////////////////////////////////////
// Id begin
inline constexpr Id::Id(StringView name) noexcept : name {name} {}

inline constexpr bool Id::operator==(const Id& other) const noexcept {
  return this == &other;
}

inline constexpr bool Id::operator!=(const Id& other) const noexcept {
  return this != &other;
}

template <typename C, typename T>
inline ::std::basic_ostream<C, T>& operator<<(::std::basic_ostream<C, T>& os,
                                              const Id& id) noexcept {
  os << id.name;
  return os;
}
// Id end
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// TypeId begin

// 从__PRETTY_FUNCTION__表达中截取关键的类型字符串
#if __clang__
template <typename T>
inline constexpr StringView TypeId<T>::get_type_name() noexcept {
  // static babylon::StringView babylon::TypeId<${type_name}>::get_type_name()
  // [T = ${typename}]
  return StringView(
      __PRETTY_FUNCTION__ +
          __builtin_strlen("static babylon::StringView babylon::TypeId<"),
      (__builtin_strlen(__PRETTY_FUNCTION__) -
       __builtin_strlen("static babylon::StringView "
                        "babylon::TypeId<>::get_type_name() [T = ]")) /
          2);
}
#elif GLIBCXX_VERSION >= 920200312 // !__clang__ && GLIBCXX_VERSION >= 920200312
template <typename T>
inline constexpr StringView TypeId<T>::get_type_name() noexcept {
  // static constexpr babylon::StringView babylon::TypeId<T>::get_type_name()
  // [with T = ${type_name}; babylon::StringView =
  // babylon::BasicStringView<char>]
  return StringView(
      __PRETTY_FUNCTION__ +
          __builtin_strlen("static constexpr babylon::StringView "
                           "babylon::TypeId<T>::get_type_name() [with T = "),
      __builtin_strlen(__PRETTY_FUNCTION__) -
          __builtin_strlen(
              "static constexpr babylon::StringView "
              "babylon::TypeId<T>::get_type_name() [with T = ; "
              "babylon::StringView = babylon::BasicStringView<char>]"));
}
#else                              // !__clang__ && GLIBCXX_VERSION < 920200312
template <typename T>
inline const StringView TypeId<T>::get_type_name() noexcept {
  // static const StringView babylon::TypeId<T>::get_type_name() [with T = int;
  // babylon::StringView = babylon::BasicStringView<char>]
  return StringView(
      __PRETTY_FUNCTION__ +
          __builtin_strlen("static const StringView "
                           "babylon::TypeId<T>::get_type_name() [with T = "),
      __builtin_strlen(__PRETTY_FUNCTION__) -
          __builtin_strlen(
              "static const StringView babylon::TypeId<T>::get_type_name() "
              "[with T = ; babylon::StringView = "
              "babylon::BasicStringView<char>]"));
}
#endif                             // !__clang__ && GLIBCXX_VERSION < 920200312

#if !__clang__ && BABYLON_GCC_VERSION < 50000
template <typename T>
__attribute__((init_priority(101)))
const Id TypeId<T>::ID {TypeId<T>::get_type_name()};
#elif !__clang__ && BABYLON_GCC_VERSION < 90300
template <typename T>
__attribute__((init_priority(101)))
const Id TypeId<T>::ID {.name = TypeId<T>::get_type_name()};
#elif __cplusplus < 201703L // && (__clang__ || BABYLON_GCC_VERSION >= 90300)
template <typename T>
constexpr Id TypeId<T>::ID;
#endif // __cplusplus < 201703L && (__clang__ || BABYLON_GCC_VERSION >= 90300)

// TypeId end
///////////////////////////////////////////////////////////////////////////////

BABYLON_NAMESPACE_END

// clang-format off
#include "babylon/unprotect.h"
// clang-format on
