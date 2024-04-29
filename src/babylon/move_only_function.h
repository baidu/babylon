#pragma once

#include "babylon/type_traits.h"

#include <functional>

BABYLON_NAMESPACE_BEGIN

// ::std::function要求包装目标必须可以拷贝构造
// 而典型的被包装对象bind和lambda，都可能闭包携带不可拷贝资源
// 为了能支持这部分对象的统一包装，提供一种只能移动的function类型
template <typename>
class MoveOnlyFunction;
template <typename R, typename... Args>
class MoveOnlyFunction<R(Args...)> {
 public:
  inline MoveOnlyFunction() noexcept = default;
  inline MoveOnlyFunction(MoveOnlyFunction&&) = default;
  inline MoveOnlyFunction(const MoveOnlyFunction&) = delete;
  inline MoveOnlyFunction& operator=(MoveOnlyFunction&&) = default;
  inline MoveOnlyFunction& operator=(const MoveOnlyFunction&) = delete;

  template <typename C,
            typename = typename ::std::enable_if<
                // 满足调用签名
                IsInvocable<C, Args...>::value
                    // 支持移动构造
                    && ::std::is_move_constructible<C>::value
                // 非引用类型，以及函数类型
                && (!::std::is_reference<C>::value ||
                    ::std::is_function<typename ::std::remove_reference<
                        C>::type>::value)>::type>
  inline MoveOnlyFunction(C&& callable) noexcept;
  template <typename C,
            typename = typename ::std::enable_if<
                IsInvocable<C, Args...>::value&& ::std::is_move_constructible<
                    C>::value &&
                (!::std::is_reference<C>::value ||
                 ::std::is_function<
                     typename ::std::remove_reference<C>::type>::value)>::type>
  inline MoveOnlyFunction& operator=(C&& callable) noexcept;

  // 是否可调用，即持有函数
  inline operator bool() const noexcept;

  // 实际调用
  inline R operator()(Args... args) const;

 private:
  ::std::function<R(Args&&...)> _function;
};

// ::std::bind在遇到参数也是bind_expression时，会进行特化处理，做多级组合
// 对需要把函数作为参数传递的场景，会导致bind_expression和普通函数表现不一致
// 这里专门制作一个包装以便统一处理，例如
// ::std::bind(function, normal_args, args_maybe_bind_result);
// 改为
// ::std::bind(function, normal_args,
// uncomposable_bind_argument(arg_maybe_bind_result));
template <typename T>
struct UncomposableBindArgument {
  inline UncomposableBindArgument(T&& value);
  inline UncomposableBindArgument(const T& value);
  inline operator T&() noexcept;
  inline operator const T&() const noexcept;

  T value;
};

template <typename T>
inline UncomposableBindArgument<typename ::std::decay<T>::type>
uncomposable_bind_argument(T&& value);

BABYLON_NAMESPACE_END

#include "babylon/move_only_function.hpp"
