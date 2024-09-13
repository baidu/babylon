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

BABYLON_NAMESPACE_END

#include "babylon/move_only_function.hpp"
