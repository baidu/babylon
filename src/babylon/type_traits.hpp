#pragma once

#include "babylon/type_traits.h"

// clang-format off
#include "babylon/protect.h"
// clang-format on

#include "fmt/format.h"

#include <list>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

BABYLON_NAMESPACE_BEGIN

///////////////////////////////////////////////////////////////////////////////
// Id begin
inline constexpr Id::Id(StringView name_value) noexcept : name {name_value} {}

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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#pragma GCC diagnostic ignored "-Wunsafe-buffer-usage"
// 从__PRETTY_FUNCTION__表达中截取关键的类型字符串
#if __clang__
template <typename T>
inline constexpr babylon::StringView TypeId<T>::get_type_name() noexcept {
  // clang-format off
  // static babylon::StringView babylon::TypeId<${type_name}>::get_type_name() [T = ${type_name}]
  return StringView(
      __PRETTY_FUNCTION__ +
          __builtin_strlen("static babylon::StringView babylon::TypeId<"),
      (__builtin_strlen(__PRETTY_FUNCTION__) -
       __builtin_strlen("static babylon::StringView babylon::TypeId<>::get_type_name() [T = ]")) /
          2);
  // clang-format on
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
#pragma GCC diagnostic pop

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

namespace fmt {
template <typename C>
struct formatter<::babylon::Id, C>
    : public formatter<::babylon::StringView, C> {
  using Base = formatter<::babylon::StringView, C>;
  template <typename CT>
  inline auto format(const ::babylon::Id& id, CT& ctx) const noexcept
      -> decltype(ctx.out()) {
    return Base::format(id.name, ctx);
  }
};
} // namespace fmt

#if __cpp_lib_format >= 201907L
#include <format>
template <typename C>
struct std::formatter<::babylon::Id, C>
    : public ::std::formatter<::babylon::StringView, C> {
  using Base = ::std::formatter<::babylon::StringView, C>;
  template <typename CT>
  inline auto format(const ::babylon::Id& id, CT& ctx) const noexcept
      -> decltype(ctx.out()) {
    return Base::format(id.name, ctx);
  }
};
#endif

// clang-format off
#include "babylon/unprotect.h"
// clang-format on
