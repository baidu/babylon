#pragma once

#include "babylon/absl_base_internal_invoke.h" // ::absl::base_internal::is_invocable_r
#include "babylon/string_view.h"               // StringView

// clang-format off
#include "babylon/protect.h"
// clang-format on

#include "absl/utility/utility.h" // absl::apply

#include <type_traits> // std::invoke_result

namespace std {
#if !__cpp_lib_is_invocable
template <typename F, typename... Args>
using is_invocable = ::absl::base_internal::is_invocable_r<void, F, Args...>;
using ::absl::base_internal::invoke_result_t;
using ::absl::base_internal::is_invocable_r;
#endif // !__cpp_lib_is_invocable

#if !__cpp_lib_apply
using ::absl::apply;
#endif // !__cpp_lib_apply

#if !__cpp_lib_invoke
using ::absl::base_internal::invoke;
#endif // !__cpp_lib_invoke

#if !__cpp_lib_remove_cvref
template <typename T>
using remove_cvref_t =
    typename ::std::remove_cv<typename ::std::remove_reference<T>::type>::type;
#endif // !__cpp_lib_remove_cvref

#if __cpp_concepts && !__cpp_lib_concepts
template <typename C, typename... Args>
concept invocable = is_invocable<C, Args...>::value;
#endif
} // namespace std

#if !__cpp_lib_is_invocable
BABYLON_NAMESPACE_BEGIN
namespace internal {
template <typename V, typename F, typename... Args>
struct InvokeResult {};
template <typename F, typename... Args>
struct InvokeResult<
    typename ::std::enable_if<::std::is_invocable<F, Args...>::value>::type, F,
    Args...> {
  using type = ::std::invoke_result_t<F, Args...>;
};
} // namespace internal
BABYLON_NAMESPACE_END

namespace std {
template <typename F, typename... Args>
struct invoke_result
    : public ::babylon::internal::InvokeResult<void, F, Args...> {};
} // namespace std
#endif // !__cpp_lib_is_invocable

BABYLON_NAMESPACE_BEGIN

template <typename T>
struct TypeId;
// 表示一种唯一ID
struct Id {
  // 唯一ID不支持拷贝和移动
  Id() = delete;
  Id(Id&& other) = delete;
  Id(const Id& other) = delete;
  Id& operator=(Id&& other) = delete;
  Id& operator=(const Id& other) = delete;
  ~Id() noexcept = default;

  // 仅能通过一个名称构造
  inline constexpr Id(StringView name) noexcept;

  // 相等唯一取决于是同一个对象
  inline constexpr bool operator==(const Id& other) const noexcept;
  inline constexpr bool operator!=(const Id& other) const noexcept;

  // ID的人可读明文表达，用于打印日志，不能用于同一性判定
  const StringView name;

  template <typename T>
  friend struct TypeId;
};

// 静态的typeid，不依赖rtti，但是只能判断相等，以及提供人可读的类型名
// 而没有反射信息，因此也相对会更快
template <typename T>
struct TypeId {
#if !__clang__ && BABYLON_GCC_VERSION < 90300
  // gcc 9.3之前无法正确处理unused __PRETTY_FUNCTION__
  // 无法实现constexpr初始化
  inline static const StringView get_type_name() noexcept;
  static const Id ID;
#else  // __clang__ || GLIBCXX_VERSION >= 920200312
  inline static constexpr babylon::StringView get_type_name() noexcept;
  static constexpr Id ID {TypeId<T>::get_type_name()};
#endif // __clang__ || GLIBCXX_VERSION >= 920200312
};

// 预期不被使用的类型，用来辅助类型探测
struct NeverUsed final {
  NeverUsed() = delete;
  NeverUsed(const NeverUsed&) = delete;
  NeverUsed(NeverUsed&&) = delete;
};

// 不占用空间的占位符类型，主要用于支持一些类型特化技巧
// 例如支持，满足某些条件，才生成某些辅助成员的场景
// 可以针对不满足条件生成类型为ZeroSized的『占位辅助成员』
// 这个占位辅助成员即不占用空间，也拟态成真实成员便于统一编码
struct ZeroSized final {
// 有些版本GCC的array-bounds检测会对这里产生越界误报
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
  template <typename... Args>
  inline constexpr ZeroSized(Args&&...) noexcept {}
#pragma GCC diagnostic pop

  template <typename T>
  inline ZeroSized& operator=(T&&) noexcept {
    return *this;
  }

  template <typename T>
  inline operator T() const noexcept {
    return T();
  }

  char data[0];
};
static_assert(sizeof(ZeroSized) == 0, "sizeof(ZeroSized) not zero as expected");

#define BABYLON_HAS_STATIC_MEMBER_CHECKER(M, C)                \
  template <typename __BABYLON_TPL_T>                          \
  struct C {                                                   \
    template <typename __BABYLON_TPL_U>                        \
    static auto checker(int) -> decltype(__BABYLON_TPL_U::M);  \
    template <typename __BABYLON_TPL_U>                        \
    static ::babylon::NeverUsed checker(...);                  \
    static constexpr bool value =                              \
        !::std::is_same<decltype(checker<__BABYLON_TPL_T>(0)), \
                        ::babylon::NeverUsed>::value;          \
  };

// 检测是否F(args)是否可调用
template <typename F, typename... Args>
struct IsInvocable {
  template <typename FF>
  static auto checker(int)
      -> decltype(::std::declval<FF>()(::std::declval<Args>()...));
  template <typename FF>
  static NeverUsed checker(...);

  static constexpr bool value =
      !::std::is_same<decltype(checker<F>(0)), NeverUsed>::value;
};

// 定义一个用于检测是否T::F(args)可调用的检测器
// F: 希望检测的静态函数名
// C: 希望生成的检测器名
// 采用宏实现主要因为函数名F的指定无法通过模版机制实现
// 用法
// 1、创建检测器
// BABYLON_DECLARE_STATIC_INVOCABLE(some_function, Checker);
// 2、使用检测器检测
// struct SomeClass {
//     static void some_function(Arg1, Arg2);
// };
// Checker<SomeClass, Arg1, Arg2>::value == true
// Checker<int, Arg1, Arg2>::value == false
#define BABYLON_DECLARE_STATIC_INVOCABLE(F, C)                        \
  template <typename __BABYLON_TPL_T, typename... __BABYLON_TPL_Args> \
  struct C {                                                          \
    template <typename __BABYLON_TPL_U>                               \
    static auto checker(int) -> decltype(__BABYLON_TPL_U::F(          \
        ::std::declval<__BABYLON_TPL_Args>()...));                    \
    template <typename __BABYLON_TPL_U>                               \
    static ::babylon::NeverUsed checker(...);                         \
    static constexpr bool value =                                     \
        !::std::is_same<decltype(checker<__BABYLON_TPL_T>(0)),        \
                        ::babylon::NeverUsed>::value;                 \
  };

// 定义一个用于检测是否T().F(args)可调用的检测器
// F: 希望检测的成员函数名
// C: 希望生成的检测器名
// 采用宏实现主要因为函数名F的指定无法通过模版机制实现
// 用法
// 1、创建检测器
// BABYLON_DECLARE_MEMBER_INVOCABLE(some_function, Checker);
// 2、使用检测器检测
// struct SomeClass {
//     void some_function(Arg1, Arg2);
// };
// Checker<SomeClass, Arg1, Arg2>::value == true
// Checker<int, Arg1, Arg2>::value == false
#define BABYLON_DECLARE_MEMBER_INVOCABLE(F, C)                                \
  template <typename __BABYLON_TPL_T, typename... __BABYLON_TPL_Args>         \
  struct C {                                                                  \
    template <typename __BABYLON_TPL_U>                                       \
    static auto checker(int) -> decltype(::std::declval<__BABYLON_TPL_U>().F( \
        ::std::declval<__BABYLON_TPL_Args>()...));                            \
    template <typename __BABYLON_TPL_U>                                       \
    static ::babylon::NeverUsed checker(...);                                 \
    static constexpr bool value =                                             \
        !::std::is_same<decltype(checker<__BABYLON_TPL_T>(0)),                \
                        ::babylon::NeverUsed>::value;                         \
  };

#define BABYLON_MEMBER_INVOCABLE_CHECKER(F, C) \
  BABYLON_DECLARE_MEMBER_INVOCABLE(F, C)

// 检测是否可以进行hash计算
// 例如，如下检测可以通过
// IsHashable<std::hash<::std::string>, const std::string&>::value == true
// IsHashable<std::hash<::std::string>, const char*>::value == true
BABYLON_DECLARE_MEMBER_INVOCABLE(operator(), IsHashable)

// 检测是否可以进行==和!=运算
template <typename A, typename B>
struct IsEqualityComparable {
  template <typename AA, typename BB>
  static auto checker(int)
      -> decltype(::std::declval<AA>() == ::std::declval<BB>());
  template <typename AA, typename BB>
  static NeverUsed checker(...);
  static constexpr bool value =
      ::std::is_same<decltype(checker<A, B>(0)), bool>::value;
};

#define BABYLON_TYPE_DEFINED_CHECKER(D, C)                                  \
  struct C {                                                                \
    template <typename __BABYLON_TPL_T>                                     \
    static typename __BABYLON_TPL_T::D* checker(int);                       \
    template <typename __BABYLON_TPL_T>                                     \
    static void checker(...);                                               \
    template <typename __BABYLON_TPL_T>                                     \
    static constexpr bool value() {                                         \
      return !::std::is_same<void,                                          \
                             decltype(checker<__BABYLON_TPL_T>(0))>::value; \
    }                                                                       \
    template <typename __BABYLON_TPL_T>                                     \
    using type =                                                            \
        typename ::std::remove_pointer<decltype(checker<__BABYLON_TPL_T>(   \
            0))>::type;                                                     \
  };

////////////////////////////////////////////////////////////////////////////////
// ParameterPack begin
// 用于处理parameter pack，即typename... Args
// 的辅助类，其中定义了First/Last的typedef
// ParameterPack<F, ..., L>::First == F
// ParameterPack<F, ..., L>::Last == L
template <typename... Args>
class ParameterPack;

template <typename T, typename... Args>
class ParameterPack<T, Args...> {
 public:
  using First = T;
  using Last = typename ParameterPack<Args...>::Last;
};

template <typename T>
class ParameterPack<T> {
 public:
  using Last = T;
};
// ParameterPack end
////////////////////////////////////////////////////////////////////////////////

template <typename, template <typename...> typename>
struct IsSpecialization : public ::std::false_type {};
template <template <typename...> typename T, typename... Args>
struct IsSpecialization<T<Args...>, T> : public ::std::true_type {};
template <typename T, template <typename...> typename TP>
constexpr bool IsSpecializationValue = IsSpecialization<T, TP>::value;

template <typename C>
struct CallableArgs;
template <typename R, typename... Args>
struct CallableArgs<R(Args...)> {
  using type = ::std::tuple<Args...>;
};
template <typename R, typename... Args>
struct CallableArgs<R (&)(Args...)> : public CallableArgs<R(Args...)> {};
template <typename R, typename... Args>
struct CallableArgs<R (&&)(Args...)> : public CallableArgs<R(Args...)> {};
template <typename R, typename... Args>
struct CallableArgs<R (*)(Args...)> : public CallableArgs<R(Args...)> {};
template <typename R, typename T, typename... Args>
struct CallableArgs<R (T::*)(Args...)> : public CallableArgs<R(T*, Args...)> {};
template <typename R, typename T, typename... Args>
struct CallableArgs<R (T::*&)(Args...)> : public CallableArgs<R(T*, Args...)> {
};
template <typename R, typename T, typename... Args>
struct CallableArgs<R (T::*&&)(Args...)> : public CallableArgs<R(T*, Args...)> {
};
template <typename R, typename T, typename... Args>
struct CallableArgs<R (T::*const)(Args...)>
    : public CallableArgs<R(T*, Args...)> {};
template <typename R, typename T, typename... Args>
struct CallableArgs<R (T::*const&)(Args...)>
    : public CallableArgs<R(T*, Args...)> {};
template <typename R, typename T, typename... Args>
struct CallableArgs<R (T::*)(Args...) const>
    : public CallableArgs<R(const T*, Args...)> {};
template <typename R, typename T, typename... Args>
struct CallableArgs<R (T::*&)(Args...) const>
    : public CallableArgs<R(const T*, Args...)> {};
template <typename R, typename T, typename... Args>
struct CallableArgs<R (T::*&&)(Args...) const>
    : public CallableArgs<R(const T*, Args...)> {};
template <typename R, typename T, typename... Args>
struct CallableArgs<R (T::*const)(Args...) const>
    : public CallableArgs<R(const T*, Args...)> {};
template <typename R, typename T, typename... Args>
struct CallableArgs<R (T::*const&)(Args...) const>
    : public CallableArgs<R(const T*, Args...)> {};

#if __cpp_noexcept_function_type
template <typename R, typename... Args>
struct CallableArgs<R(Args...) noexcept> : public CallableArgs<R(Args...)> {};
template <typename R, typename... Args>
struct CallableArgs<R (&)(Args...) noexcept> : public CallableArgs<R(Args...)> {
};
template <typename R, typename... Args>
struct CallableArgs<R (&&)(Args...) noexcept>
    : public CallableArgs<R(Args...)> {};
template <typename R, typename... Args>
struct CallableArgs<R (*)(Args...) noexcept> : public CallableArgs<R(Args...)> {
};
template <typename R, typename T, typename... Args>
struct CallableArgs<R (T::*)(Args...) noexcept>
    : public CallableArgs<R(T*, Args...)> {};
template <typename R, typename T, typename... Args>
struct CallableArgs<R (T::*&)(Args...) noexcept>
    : public CallableArgs<R(T*, Args...)> {};
template <typename R, typename T, typename... Args>
struct CallableArgs<R (T::*&&)(Args...) noexcept>
    : public CallableArgs<R(T*, Args...)> {};
template <typename R, typename T, typename... Args>
struct CallableArgs<R (T::*const)(Args...) noexcept>
    : public CallableArgs<R(T*, Args...)> {};
template <typename R, typename T, typename... Args>
struct CallableArgs<R (T::*const&)(Args...) noexcept>
    : public CallableArgs<R(T*, Args...)> {};
template <typename R, typename T, typename... Args>
struct CallableArgs<R (T::*)(Args...) const noexcept>
    : public CallableArgs<R(const T*, Args...)> {};
template <typename R, typename T, typename... Args>
struct CallableArgs<R (T::*&)(Args...) const noexcept>
    : public CallableArgs<R(const T*, Args...)> {};
template <typename R, typename T, typename... Args>
struct CallableArgs<R (T::*&&)(Args...) const noexcept>
    : public CallableArgs<R(const T*, Args...)> {};
template <typename R, typename T, typename... Args>
struct CallableArgs<R (T::*const)(Args...) const noexcept>
    : public CallableArgs<R(const T*, Args...)> {};
template <typename R, typename T, typename... Args>
struct CallableArgs<R (T::*const&)(Args...) const noexcept>
    : public CallableArgs<R(const T*, Args...)> {};
#endif // __cpp_noexcept_function_type

template <typename C>
struct CallableArgs
    : public CallableArgs<decltype(&::std::decay<C>::type::operator())> {
  using BaseType =
      typename CallableArgs<decltype(&::std::decay<C>::type::operator())>::type;
  template <typename T, typename... Args>
  static ::std::tuple<Args...> RemoveFirst(::std::tuple<T, Args...>);
  using type = decltype(RemoveFirst(::std::declval<BaseType>()));
};

class Void final {
 private:
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#pragma GCC diagnostic ignored "-Wunused-private-field"
  char _dummy[0];
#pragma GCC diagnostic pop
};

BABYLON_NAMESPACE_END

// clang-format off
#include "babylon/unprotect.h"
// clang-format on

#include "babylon/type_traits.hpp"
