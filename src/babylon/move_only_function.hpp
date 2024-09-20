#pragma once

#include "babylon/move_only_function.h"

BABYLON_NAMESPACE_BEGIN

namespace internal {
namespace move_only_function {

// 包装一个可移动构造的Callable类型，提供无效的拷贝构造
// 主要用来提供装入std::function的能力，但是后续不应该真正调用拷贝构造
template <typename C, typename R, typename... Args>
class MoveOnlyCallable {
 public:
  // 只支持移动，补充无效的拷贝构造实现，用来支持存入std::function
  inline MoveOnlyCallable() = delete;
  inline MoveOnlyCallable(MoveOnlyCallable&&) = default;
  inline MoveOnlyCallable(const MoveOnlyCallable&);
  inline MoveOnlyCallable& operator=(MoveOnlyCallable&&) = default;
  inline MoveOnlyCallable& operator=(const MoveOnlyCallable&);
  // 从Callable移动构造
  inline MoveOnlyCallable(C&& callable);
  // 代理Callable的调用
  inline R operator()(Args&&... args);

 private:
  C _callable;
};

template <typename C, typename R, typename... Args>
inline MoveOnlyCallable<C, R, Args...>::MoveOnlyCallable(
    const MoveOnlyCallable& other)
    : _callable(::std::move(const_cast<MoveOnlyCallable&>(other))._callable) {
  ::abort();
}

template <typename C, typename R, typename... Args>
inline MoveOnlyCallable<C, R, Args...>&
MoveOnlyCallable<C, R, Args...>::operator=(const MoveOnlyCallable&) {
  ::abort();
}

template <typename C, typename R, typename... Args>
inline MoveOnlyCallable<C, R, Args...>::MoveOnlyCallable(C&& callable)
    : _callable(::std::move(callable)) {}

template <typename C, typename R, typename... Args>
inline R MoveOnlyCallable<C, R, Args...>::operator()(Args&&... args) {
  return static_cast<R>(_callable(::std::forward<Args>(args)...));
}

} // namespace move_only_function
} // namespace internal

template <typename R, typename... Args>
template <typename C, typename>
inline MoveOnlyFunction<R(Args...)>::MoveOnlyFunction(C&& callable) noexcept
    : _function(internal::move_only_function::MoveOnlyCallable<C, R, Args...>(
          ::std::move(callable))) {}

template <typename R, typename... Args>
template <typename C, typename>
inline MoveOnlyFunction<R(Args...)>& MoveOnlyFunction<R(Args...)>::operator=(
    C&& callable) noexcept {
  _function = internal::move_only_function::MoveOnlyCallable<C, R, Args...>(
      ::std::move(callable));
  return *this;
}

template <typename R, typename... Args>
inline MoveOnlyFunction<R(Args...)>::operator bool() const noexcept {
  return static_cast<bool>(_function);
}

template <typename R, typename... Args>
inline R MoveOnlyFunction<R(Args...)>::operator()(Args... args) const {
  return _function(::std::forward<Args>(args)...);
}

BABYLON_NAMESPACE_END
