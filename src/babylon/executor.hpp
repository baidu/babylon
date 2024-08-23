#pragma once

#include "babylon/executor.h"

#include <type_traits>

BABYLON_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
// Executor::begin
template <typename F, typename C, typename... Args>
inline Future<typename InvokeResult<C, Args...>::type, F> Executor::execute(
    C&& callable, Args&&... args) noexcept {
  using DC = typename ::std::decay<C>::type;
  using R = typename InvokeResult<C, Args...>::type;
  Promise<R, F> promise;
  auto future = promise.get_future();
  MoveOnlyFunction<void(void)> function {::std::bind(
      [](Promise<R, F>& captured_promise, DC& captured_callable,
         typename ::std::decay<Args>::type&... captured_args) {
        run_and_set(captured_promise, normalize(::std::move(captured_callable)),
                    ::std::move(captured_args)...);
      },
      ::std::move(promise),
      uncomposable_bind_argument(::std::forward<C>(callable)),
      uncomposable_bind_argument(::std::forward<Args>(args))...)};
  auto ret = invoke(::std::move(function));
  if (ABSL_PREDICT_FALSE(ret != 0)) {
    future = Future<R, F>();
  }
  return future;
}

template <typename C, typename... Args>
inline int Executor::submit(C&& callable, Args&&... args) noexcept {
  typedef typename ::std::decay<C>::type DC;
  MoveOnlyFunction<void(void)> function {::std::bind(
      [](DC& captured_callable,
         typename ::std::decay<Args>::type&... captured_args) {
        normalize(::std::move(captured_callable))(
            ::std::move(captured_args)...);
      },
      uncomposable_bind_argument(::std::forward<C>(callable)),
      uncomposable_bind_argument(::std::forward<Args>(args))...)};
  return invoke(::std::move(function));
}

template <typename C,
          typename ::std::enable_if<
              ::std::is_member_function_pointer<C>::value, int>::type>
inline auto Executor::normalize(C&& callable) noexcept
    -> decltype(::std::mem_fn(callable)) {
  return ::std::mem_fn(callable);
}

template <typename C,
          typename ::std::enable_if<
              !::std::is_member_function_pointer<C>::value, int>::type>
inline C&& Executor::normalize(C&& callable) noexcept {
  return ::std::forward<C>(callable);
}

template <typename F, typename C, typename... Args>
inline void Executor::run_and_set(Promise<void, F>& promise, C&& callable,
                                  Args&&... args) noexcept {
  callable(::std::forward<Args>(args)...);
  promise.set_value();
}

template <typename R, typename F, typename C, typename... Args,
          typename ::std::enable_if<!::std::is_same<void, R>::value, int>::type>
inline void Executor::run_and_set(Promise<R, F>& promise, C&& callable,
                                  Args&&... args) noexcept {
  promise.set_value(callable(::std::forward<Args>(args)...));
}
// Executor::end
////////////////////////////////////////////////////////////////////////////////

BABYLON_NAMESPACE_END
