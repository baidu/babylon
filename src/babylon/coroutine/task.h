#pragma once

#include "babylon/coroutine/promise.h"

#if __cpp_concepts && __cpp_lib_coroutine

BABYLON_NAMESPACE_BEGIN

template <typename T = void>
class CoroutineTask {
 public:
  class promise_type;

  inline CoroutineTask() noexcept = default;
  inline CoroutineTask(CoroutineTask&& other) noexcept;
  CoroutineTask(const CoroutineTask&) = delete;
  inline CoroutineTask& operator=(CoroutineTask&& other) noexcept;
  CoroutineTask& operator=(const CoroutineTask&) = delete;
  inline ~CoroutineTask() noexcept;

  inline CoroutineTask<T>& set_executor(BasicExecutor& executor) & noexcept;
  inline CoroutineTask<T>&& set_executor(BasicExecutor& executor) && noexcept {
    set_executor(executor);
    return ::std::move(*this);
  }
  inline BasicExecutor* executor() const noexcept;

  //////////////////////////////////////////////////////////////////////////////
  // Protocol to be an awaitable
  inline static constexpr bool await_ready() noexcept;
  template <typename P>
    requires(::std::is_base_of<BasicCoroutinePromise, P>::value)
  inline ::std::coroutine_handle<> await_suspend(
      std::coroutine_handle<P> awaiter) noexcept;
  inline ::std::coroutine_handle<> await_suspend(
      std::coroutine_handle<> awaiter) noexcept;
  inline T await_resume() noexcept;
  //////////////////////////////////////////////////////////////////////////////

 private:
  inline CoroutineTask(::std::coroutine_handle<promise_type> handle) noexcept;

  inline ::std::coroutine_handle<> release() noexcept;

  inline ::std::coroutine_handle<> await_suspend(
      std::coroutine_handle<> awaiter,
      BasicExecutor* awaiter_executor) noexcept;

  ::std::coroutine_handle<promise_type> _handle;

  friend BasicCoroutinePromise;
  friend class Executor;
};

template <typename T>
class CoroutineTask<T>::promise_type : public CoroutinePromise<T> {
 public:
  inline CoroutineTask<T> get_return_object() noexcept {
    return {::std::coroutine_handle<promise_type>::from_promise(*this)};
  }
};

template <typename A>
struct CoroutineWrapperTask {};
template <typename A>
  requires requires { typename CoroutineAwaitResultType<void, A>; }
struct CoroutineWrapperTask<A> {
  using ResultType = CoroutineAwaitResultType<void, A>;
  using ForwardType = typename ::std::conditional<
      ::std::is_rvalue_reference<ResultType>::value,
      typename ::std::remove_reference<ResultType>::type, ResultType>::type;
  using type = CoroutineTask<ForwardType>;
};
template <typename A>
using CoroutineWrapperTaskType = CoroutineWrapperTask<A>::type;

// Check if a callable invocation C(Args...) is a babylon coroutine, thus,
// return CoroutineTask<T>
template <typename C, typename... Args>
concept CoroutineTaskInvocable =
    CoroutineInvocable<C, Args...> &&
    IsSpecialization<::std::invoke_result_t<C, Args...>, CoroutineTask>::value;

template <typename T>
class BasicCoroutinePromise::Transformer<CoroutineTask<T>> {
 public:
  static CoroutineTask<T>&& await_transform(BasicCoroutinePromise& promise,
                                            CoroutineTask<T>&& task) {
    if (!task.executor()) {
      task.set_executor(*promise.executor());
    }
    return ::std::move(task);
  }
};

template <typename A>
  requires requires { typename CoroutineWrapperTaskType<A&&>; }
class BasicCoroutinePromise::Transformer<A> {
 public:
  inline static CoroutineWrapperTaskType<A&&> await_transform(
      BasicCoroutinePromise&, A&& awaitable) noexcept {
    return [](A&& inner_awaitable) -> CoroutineWrapperTaskType<A&&> {
      co_return co_await BasicCoroutinePromise::NoTransformation<A> {
          ::std::forward<A>(inner_awaitable)};
    }(::std::forward<A>(awaitable));
  }
};

////////////////////////////////////////////////////////////////////////////////
// CoroutineTask begin
template <typename T>
inline CoroutineTask<T>::CoroutineTask(CoroutineTask&& other) noexcept {
  ::std::swap(_handle, other._handle);
}

template <typename T>
inline CoroutineTask<T>& CoroutineTask<T>::operator=(
    CoroutineTask&& other) noexcept {
  ::std::swap(_handle, other._handle);
  return *this;
}

template <typename T>
inline CoroutineTask<T>::~CoroutineTask() noexcept {
  if (_handle) {
    _handle.destroy();
  }
}

template <typename T>
inline CoroutineTask<T>& CoroutineTask<T>::set_executor(
    BasicExecutor& executor) & noexcept {
  _handle.promise().set_executor(executor);
  return *this;
}

template <typename T>
inline BasicExecutor* CoroutineTask<T>::executor() const noexcept {
  return _handle.promise().executor();
}

template <typename T>
inline constexpr bool CoroutineTask<T>::await_ready() noexcept {
  return false;
}

template <typename T>
template <typename P>
  requires(::std::is_base_of<BasicCoroutinePromise, P>::value)
inline ::std::coroutine_handle<> CoroutineTask<T>::await_suspend(
    ::std::coroutine_handle<P> awaiter) noexcept {
  return await_suspend(awaiter, awaiter.promise().executor());
}

template <typename T>
inline ::std::coroutine_handle<> CoroutineTask<T>::await_suspend(
    ::std::coroutine_handle<> awaiter) noexcept {
  return await_suspend(awaiter, nullptr);
}

template <typename T>
inline T CoroutineTask<T>::await_resume() noexcept {
  return _handle.promise().value();
}

template <>
inline void CoroutineTask<void>::await_resume() noexcept {}

template <typename T>
inline CoroutineTask<T>::CoroutineTask(
    ::std::coroutine_handle<promise_type> handle) noexcept
    : _handle(handle) {}

template <typename T>
inline ::std::coroutine_handle<> CoroutineTask<T>::release() noexcept {
  return ::std::exchange(_handle, nullptr);
}

template <typename T>
inline ::std::coroutine_handle<> CoroutineTask<T>::await_suspend(
    ::std::coroutine_handle<> awaiter,
    BasicExecutor* awaiter_executor) noexcept {
  auto& promise = _handle.promise();
  promise.set_awaiter(awaiter, awaiter_executor);
  if (promise.inplace_resumable()) {
    return _handle;
  }
  promise.resume(_handle);
  return ::std::noop_coroutine();
}
// CoroutineTask end
////////////////////////////////////////////////////////////////////////////////

BABYLON_NAMESPACE_END

#endif // __cpp_concepts && __cpp_lib_coroutine
