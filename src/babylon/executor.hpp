#pragma once

#include "babylon/executor.h"

// clang-foramt off
#include "babylon/protect.h"
// clang-foramt on

BABYLON_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
// Executor begin
template <typename F, typename C, typename... Args>
#if __cpp_concepts && __cpp_lib_coroutine
  requires(::std::invocable<C &&, Args && ...> &&
           !CoroutineInvocable<C &&, Args && ...>)
#endif // __cpp_concepts && __cpp_lib_coroutine
inline Future<Executor::ResultType<C&&, Args&&...>, F> Executor::execute(
    C&& callable, Args&&... args) noexcept {
  using R = ResultType<C&&, Args&&...>;
  struct S {
    Promise<R, F> promise;
    typename ::std::decay<C>::type callable;
    ::std::tuple<typename ::std::decay<Args>::type...> args_tuple;
    void operator()() noexcept {
      apply_and_set_value(promise, ::std::move(callable),
                          ::std::move(args_tuple));
    }
  } s {.promise {},
       .callable {::std::forward<C>(callable)},
       .args_tuple {::std::forward<Args>(args)...}};
  auto future = s.promise.get_future();
  MoveOnlyFunction<void(void)> function {::std::move(s)};
  auto ret = invoke(::std::move(function));
  if (ABSL_PREDICT_FALSE(ret != 0)) {
    future = Future<R, F>();
  }
  return future;
}

#if __cpp_concepts && __cpp_lib_coroutine
template <typename F, typename C, typename... Args>
  requires CoroutineInvocable<C&&, Args&&...> && Executor::IsPlainFunction<C>
inline Future<Executor::ResultType<C&&, Args&&...>, F> Executor::execute(
    C&& callable, Args&&... args) noexcept {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#if ABSL_HAVE_ADDRESS_SANITIZER
#pragma GCC diagnostic ignored "-Warray-bounds"
#endif // ABSL_HAVE_ADDRESS_SANITIZER
  auto task =
      ::std::invoke(::std::forward<C>(callable), ::std::forward<Args>(args)...);
#pragma GCC diagnostic pop
  return execute<F>(::std::move(task));
}

template <typename F, typename C, typename... Args>
  requires CoroutineInvocable<C&&, Args&&...> && (!Executor::IsPlainFunction<C>)
inline Future<Executor::ResultType<C&&, Args&&...>, F> Executor::execute(
    C&& callable, Args&&... args) noexcept {
  using R = ResultType<C&&, Args&&...>;
  using InnerArgsTuple = typename CallableArgs<C>::type;
  Promise<R, F> promise;
  auto future = promise.get_future();
  submit(await_apply_and_set_value<InnerArgsTuple>(
      ::std::move(promise), ::std::forward<C>(callable),
      ::std::forward_as_tuple(::std::forward<Args>(args)...)));
  return future;
}

template <typename F, typename A>
  requires coroutine::Awaitable<A, CoroutineTask<>::promise_type>
inline Future<Executor::AwaitResultType<A&&>, F> Executor::execute(
    A&& awaitable) noexcept {
  using R = AwaitResultType<A&&>;
  Promise<R, F> promise;
  auto future = promise.get_future();
  submit(
      await_and_set_value(::std::move(promise), ::std::forward<A>(awaitable)));
  return future;
}
#endif // __cpp_concepts && __cpp_lib_coroutine

template <typename C, typename... Args>
#if __cpp_concepts && __cpp_lib_coroutine
  requires ::std::is_invocable<C&&, Args&&...>::value &&
           (!CoroutineInvocable<C &&, Args && ...>)
#endif // __cpp_concepts && __cpp_lib_coroutine
inline int Executor::submit(C&& callable, Args&&... args) noexcept {
  struct S {
    typename ::std::decay<C>::type callable;
    ::std::tuple<typename ::std::decay<Args>::type...> args_tuple;
    void operator()() {
      ::std::apply(::std::move(callable), ::std::move(args_tuple));
    }
  };
  S s {.callable {::std::forward<C>(callable)},
       .args_tuple {::std::forward<Args>(args)...}};
  MoveOnlyFunction<void(void)> function {::std::move(s)};
  return invoke(::std::move(function));
}

#if __cpp_concepts && __cpp_lib_coroutine
template <typename C, typename... Args>
  requires Executor::IsCoroutineTaskValue<C&&, Args&&...> &&
           Executor::IsPlainFunction<C>
inline int Executor::submit(C&& callable, Args&&... args) noexcept {
  auto task =
      ::std::invoke(::std::forward<C>(callable), ::std::forward<Args>(args)...);
  return submit(::std::move(task));
}

template <typename C, typename... Args>
  requires CoroutineInvocable<C&&, Args&&...> &&
           (!Executor::IsCoroutineTaskValue<C &&, Args && ...>) &&
           Executor::IsPlainFunction<C>
inline int Executor::submit(C&& callable, Args&&... args) noexcept {
  using TaskType = ::std::invoke_result_t<C&&, Args&&...>;
  struct S {
    static CoroutineTask<> wrapper(TaskType task) {
      co_await ::std::move(task);
    }
  };
  auto task =
      ::std::invoke(::std::forward<C>(callable), ::std::forward<Args>(args)...);
  return submit(S::wrapper(::std::move(task)));
}

template <typename C, typename... Args>
  requires CoroutineInvocable<C&&, Args&&...> && (!Executor::IsPlainFunction<C>)
inline int Executor::submit(C&& callable, Args&&... args) noexcept {
  using InnerArgsTuple = typename CallableArgs<C>::type;
  struct S {
    static CoroutineTask<> wrapper(C callable, InnerArgsTuple args_tuple) {
      co_await ::std::apply(callable, ::std::move(args_tuple));
    }
  };
  auto wrapper_task =
      S::wrapper(::std::forward<C>(callable),
                 ::std::forward_as_tuple(::std::forward<Args>(args)...));
  return submit(::std::move(wrapper_task));
}

template <typename T>
inline int Executor::submit(CoroutineTask<T>&& task) noexcept {
  task.set_executor(*this);
  auto handle = task.release();
  auto ret = invoke([handle] {
    handle.resume();
  });
  if (ABSL_PREDICT_FALSE(ret != 0)) {
    handle.destroy();
  }
  return ret;
}
#endif // __cpp_concepts && __cpp_lib_coroutine

template <typename P, typename C, typename... Args>
inline void Executor::apply_and_set_value(
    P& promise, C&& callable, ::std::tuple<Args...>&& args_tuple) noexcept {
  promise.set_value(
      ::std::apply(::std::forward<C>(callable), ::std::move(args_tuple)));
}

template <typename F, typename C, typename... Args>
inline void Executor::apply_and_set_value(
    Promise<void, F>& promise, C&& callable,
    ::std::tuple<Args...>&& args_tuple) noexcept {
  ::std::apply(::std::forward<C>(callable), ::std::move(args_tuple));
  promise.set_value();
}

#if __cpp_concepts && __cpp_lib_coroutine
template <typename P, typename A>
CoroutineTask<> Executor::await_and_set_value(P promise, A awaitable) noexcept {
  promise.set_value(co_await ::std::move(awaitable));
}

template <typename F, typename A>
CoroutineTask<> Executor::await_and_set_value(Promise<void, F> promise,
                                              A awaitable) noexcept {
  co_await ::std::move(awaitable);
  promise.set_value();
}

template <typename T, typename P, typename C>
CoroutineTask<> Executor::await_apply_and_set_value(P promise, C callable,
                                                    T args_tuple) noexcept {
  promise.set_value(co_await ::std::apply(::std::forward<C>(callable),
                                          ::std::move(args_tuple)));
}

template <typename T, typename F, typename C>
CoroutineTask<> Executor::await_apply_and_set_value(Promise<void, F> promise,
                                                    C callable,
                                                    T args_tuple) noexcept {
  co_await ::std::apply(::std::forward<C>(callable), ::std::move(args_tuple));
  promise.set_value();
}
#endif // __cpp_concepts && __cpp_lib_coroutine
// Executor end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// ThreadPoolExecutor begin
template <typename R, typename P>
void ThreadPoolExecutor::set_balance_interval(
    ::std::chrono::duration<R, P> balance_interval) noexcept {
  _balance_interval = balance_interval;
}
// ThreadPoolExecutor end
////////////////////////////////////////////////////////////////////////////////

BABYLON_NAMESPACE_END

#include "babylon/unprotect.h"
